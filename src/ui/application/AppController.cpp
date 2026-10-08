#include "application/AppController.h"

#include "application/LayerListModel.h"
#include "commands/Commands.h"
#include "fill/FloodFill.h"
#include "filters/Filters.h"
#include "layer/Layer.h"
#include "project/ProjectIO.h"
#include "selection/Selection.h"
#include "shapes/Shapes.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFont>
#include <QPainter>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>
#include <cmath>

using namespace prism;

namespace {

prism::core::geometry::Rect fullRect(const prism::core::Document& doc)
{
    const auto s = doc.canvasSize();
    return prism::core::geometry::Rect{ 0, 0, s.width, s.height };
}

prism::core::raster::Color toRaster(QColor c)
{
    return prism::core::raster::Color{ static_cast<std::uint8_t>(c.red()),
                                       static_cast<std::uint8_t>(c.green()),
                                       static_cast<std::uint8_t>(c.blue()), 255 };
}

QColor toQColor(prism::core::raster::Color c)
{
    return QColor(c.r, c.g, c.b, c.a);
}

/// QML passes file arguments either as a `url` (FileDialog.selectedFile) or as
/// a plain string (scripts, drag-and-drop, QMetaObject::invokeMethod). A
/// Windows path string converted to QUrl is NOT a local file URL —
/// QUrl("C:\\x.png") parses "c" as a scheme — so toLocalFile() returns empty
/// and the operation silently fails with an empty file name. Accept both.
QString localPathFrom(const QUrl& url)
{
    const QString path = url.toLocalFile();
    if (!path.isEmpty())
        return path;

    QString raw = url.toString();
    if (raw.isEmpty())
        return raw;
    // The path string was parsed as a URL, so separators arrived
    // percent-encoded ("C:%5CUsers%5C..."). Decode before handing it to Qt's
    // file APIs, which want a plain filesystem path.
    raw = QUrl::fromPercentEncoding(raw.toUtf8());
    if (raw.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
        raw = QUrl(raw).toLocalFile();
    return raw;
}

/// Human label for a Qt image format, so extensions that mean the same thing
/// (jpg/jpeg/jfif) collapse into one dialog entry.
QString formatLabel(const QByteArray& format)
{
    static const QHash<QString, QString> labels = {
        { QStringLiteral("png"), QStringLiteral("PNG image") },
        { QStringLiteral("jpg"), QStringLiteral("JPEG image") },
        { QStringLiteral("jpeg"), QStringLiteral("JPEG image") },
        { QStringLiteral("jfif"), QStringLiteral("JPEG image") },
        { QStringLiteral("webp"), QStringLiteral("WebP image") },
        { QStringLiteral("tif"), QStringLiteral("TIFF image") },
        { QStringLiteral("tiff"), QStringLiteral("TIFF image") },
        { QStringLiteral("bmp"), QStringLiteral("Windows BMP image") },
        { QStringLiteral("gif"), QStringLiteral("GIF image") },
        { QStringLiteral("ico"), QStringLiteral("Windows icon") },
        { QStringLiteral("cur"), QStringLiteral("Windows cursor") },
        { QStringLiteral("ppm"), QStringLiteral("PPM image") },
        { QStringLiteral("pgm"), QStringLiteral("PGM image") },
        { QStringLiteral("pbm"), QStringLiteral("PBM image") },
        { QStringLiteral("xbm"), QStringLiteral("XBM image") },
        { QStringLiteral("xpm"), QStringLiteral("XPM image") },
    };
    const QString key = QString::fromLatin1(format).toLower();
    return labels.value(key, key.toUpper() + QStringLiteral(" image"));
}

/// Builds file-dialog filter strings ("JPEG image (*.jpg *.jpeg)") from the
/// formats this Qt build actually supports, keeping the given order.
QStringList filterStringsFor(const QList<QByteArray>& formats)
{
    QList<QPair<QString, QStringList>> groups;
    for (const QByteArray& format : formats) {
        const QString label = formatLabel(format);
        const QString pattern = QStringLiteral("*.") + QString::fromLatin1(format);
        bool appended = false;
        for (auto& group : groups) {
            if (group.first == label) {
                group.second << pattern;
                appended = true;
                break;
            }
        }
        if (!appended)
            groups.append({ label, { pattern } });
    }

    QStringList out;
    for (const auto& group : groups)
        out << QStringLiteral("%1 (%2)").arg(group.first, group.second.join(QLatin1Char(' ')));
    return out;
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_layersModel(new LayerListModel(this))
    , m_presets(canvas::builtinBrushPresets())
{
    m_doc.addRasterLayer("Layer 1");
    m_layersModel->setDocument(&m_doc);
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_selection.reset(m_doc.canvasSize().width, m_doc.canvasSize().height);
    loadRecentFiles();
    loadUserPresets();
    // Start on the first preset so the toolbar/status bar read like a real app.
    selectPreset(0);
    rebuildFullComposite();

    // Image providers run on Qt's pixmap-reader thread and may only read
    // snapshots. Refreshing them on a timer keeps every pixel access on the GUI
    // thread (and bounds the cost: a 4 Hz refresh of a 48x32 thumbnail set, not
    // a canvas-sized scan per request).
    m_snapshotTimer = new QTimer(this);
    m_snapshotTimer->setInterval(250);
    connect(m_snapshotTimer, &QTimer::timeout, this, [this]() {
        // Pixels changed, or the selection was cleared (leaving a stale overlay).
        if (m_snapshotRevision != m_layerRevision
            || (!m_selectionSnapshot.isNull() && m_selection.isEmpty()))
            refreshSnapshots();
    });
    m_snapshotTimer->start();
    refreshSnapshots();

    // The drawing record: every canvas change is a candidate frame, throttled
    // inside the recorder, so the artist has a proof-of-process timelapse for
    // the session without thinking about it while drawing.
    m_recorder = new SessionRecorder(this);
    connect(m_recorder, &SessionRecorder::full, this, [this]() {
        emit recordingChanged();
        setStatusMessage(tr("Recording buffer is full (%1 frames) - export it from File > "
                            "Export Drawing Record...")
                             .arg(m_recorder->frameCount()));
    });
    connect(this, &AppController::canvasChanged, this,
            [this](int, int, int, int) { m_recorder->capture(canvasImage(), undoLabel()); });
}

bool AppController::recording() const { return m_recorder && m_recorder->isActive(); }

void AppController::setRecording(bool on)
{
    if (!m_recorder || m_recorder->isActive() == on)
        return;
    m_recorder->setActive(on);
    if (on)
        m_recorder->capture(canvasImage(), tr("Resumed"), true);
    emit recordingChanged();
}

int AppController::recordedFrames() const { return m_recorder ? m_recorder->frameCount() : 0; }

bool AppController::exportRecording(const QUrl& url)
{
    const QString path = localPathFrom(url);
    if (path.isEmpty()) {
        setLastError(tr("No file name specified."));
        return false;
    }
    // The current state is the last frame, whatever the throttle says: a client
    // checking the record must see the finished picture.
    m_recorder->capture(canvasImage(), tr("Final"), true);

    QString error;
    const QString suffix = QFileInfo(path).suffix().toLower();
    bool ok = false;
    if (suffix == QLatin1String("gif") || suffix == QLatin1String("mp4")) {
        // A timelapse: the frames are flattened onto paper for the codec.
        const RecordedFrames frames = m_recorder->recordedFrames();
        ok = suffix == QLatin1String("gif") ? RecordExport::writeGif(path, frames, &error)
                                            : RecordExport::writeMp4(path, frames, &error);
        if (ok) {
            // Prove the file is readable before telling the artist it worked.
            int written = 0;
            QString readError;
            if (!RecordExport::verifyVideo(path, &written, &readError)) {
                setLastError(readError);
                emit recordingChanged();
                return false;
            }
        }
    } else {
        ok = m_recorder->save(path, QSize(canvasWidth(), canvasHeight()), &error);
    }

    if (!ok) {
        setLastError(error);
        emit recordingChanged();
        return false;
    }
    setLastError(QString());
    setStatusMessage(tr("Drawing record exported: %1 frames (%2).")
                         .arg(m_recorder->frameCount())
                         .arg(QFileInfo(path).fileName()));
    emit recordingChanged();
    return true;
}

// ------------------------------------------------------------------ properties

int AppController::activeLayerIndex() const
{
    return std::max(0, std::min(m_activeLayer, static_cast<int>(m_doc.layers().size()) - 1));
}

void AppController::setActiveLayerIndex(int index)
{
    if (index >= 0 && index < static_cast<int>(m_doc.layers().size())) {
        m_activeLayer = index;
        emit layersChanged();
    }
}

bool AppController::canUndo() const { return m_history.canUndo(); }
bool AppController::canRedo() const { return m_history.canRedo(); }

QString AppController::undoLabel() const
{
    return canUndo() ? QString::fromUtf8(m_history.undoDescription()) : tr("Undo");
}

int AppController::canvasWidth() const { return m_doc.canvasSize().width; }
int AppController::canvasHeight() const { return m_doc.canvasSize().height; }

QColor AppController::brushColor() const { return toQColor(m_brush.color); }

void AppController::setBrushColor(const QColor& color)
{
    m_brush.color = toRaster(color);
    // Keep the current tool's slot in sync so the choice sticks when the artist
    // switches tools and comes back.
    m_colorByTool[m_brush.erase ? 1 : 0] = m_brush.color;
    emit brushChanged();
}

float AppController::brushSize() const { return m_brush.radius; }

void AppController::setBrushSize(float size)
{
    m_brush.radius = std::clamp(size, 1.0F, 200.0F);
    m_presetIntact = false;
    emit brushChanged();
}

float AppController::brushHardness() const { return m_brush.hardness; }

void AppController::setBrushHardness(float hardness)
{
    m_brush.hardness = std::clamp(hardness, 0.0F, 1.0F);
    m_presetIntact = false;
    emit brushChanged();
}

float AppController::brushFlow() const { return m_brush.flow; }

void AppController::setBrushFlow(float flow)
{
    m_brush.flow = std::clamp(flow, 0.01F, 1.0F);
    m_presetIntact = false;
    emit brushChanged();
}

bool AppController::brushTipSquare() const
{
    return m_brush.tip.shape == canvas::BrushTip::Shape::Square;
}

void AppController::setBrushTipSquare(bool square)
{
    // Switching to the procedural editor implicitly leaves an image tip.
    m_brush.tip.kind = canvas::BrushTip::Kind::Procedural;
    m_brush.tip.shape = square ? canvas::BrushTip::Shape::Square
                               : canvas::BrushTip::Shape::Round;
    m_presetIntact = false;
    emit brushChanged();
}

float AppController::brushTipRoundness() const { return m_brush.tip.roundness; }

void AppController::setBrushTipRoundness(float roundness)
{
    m_brush.tip.roundness = std::clamp(roundness, 0.02F, 1.0F);
    m_presetIntact = false;
    emit brushChanged();
}

float AppController::brushTipAngle() const { return m_brush.tip.angle; }

void AppController::setBrushTipAngle(float angle)
{
    m_brush.tip.angle = std::fmod(std::fabs(angle), 360.0F);
    m_presetIntact = false;
    emit brushChanged();
}

bool AppController::pressureOpacity() const { return m_brush.pressureOpacity; }

void AppController::setPressureOpacity(bool on)
{
    m_brush.pressureOpacity = on;
    m_presetIntact = false;
    emit brushChanged();
}

bool AppController::tipIsImage() const
{
    return m_brush.tip.kind == canvas::BrushTip::Kind::Image;
}

bool AppController::loadTipImage(const QUrl& url)
{
    const QString path = localPathFrom(url);
    QImage image(path);
    if (image.isNull()) {
        setLastError(tr("Tip image could not be loaded: %1").arg(path));
        return false;
    }
    if (image.width() > 1024 || image.height() > 1024) {
        setLastError(tr("Tip image is too large (max 1024x1024): %1x%2")
                         .arg(image.width())
                         .arg(image.height()));
        return false;
    }

    // Re-encode as PNG so the tip carries its own bytes for export.
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, "PNG");

    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    auto& tip = m_brush.tip;
    tip.kind = canvas::BrushTip::Kind::Image;
    tip.image.mime = "image/png";
    tip.image.invert = false;
    tip.image.data.assign(png.begin(), png.end());
    tip.image.maskW = gray.width();
    tip.image.maskH = gray.height();
    tip.image.mask.assign(gray.constBits(), gray.constBits() + gray.sizeInBytes());
    m_presetIntact = false;
    emit brushChanged();
    return true;
}

void AppController::clearTipImage()
{
    m_brush.tip = canvas::BrushTip{};
    m_presetIntact = false;
    emit brushChanged();
}

qreal AppController::pressureCurve() const { return m_pressureCurve; }

void AppController::setPressureCurve(qreal gamma)
{
    m_pressureCurve = std::clamp(gamma, 0.3, 3.0);
    emit brushChanged();
}

bool AppController::eraser() const { return m_brush.erase; }

void AppController::setEraser(bool erase)
{
    m_brush.erase = erase;
    emit brushChanged();
}

qreal AppController::zoom() const { return m_zoom; }

void AppController::setZoom(qreal zoom)
{
    m_zoom = std::clamp(zoom, 0.05, 8.0);
    emit viewChanged();
}

int AppController::tool() const { return m_tool; }

void AppController::setTool(int tool)
{
    // Remember where the eyedropper was invoked from, so a pick can hand the
    // canvas back to the tool the artist was drawing with.
    if (tool == EyedropperTool && m_tool != EyedropperTool)
        m_toolBeforeEyedropper = m_tool;
    // Colour is remembered per tool: the eraser does not paint, so a colour set
    // while erasing (a pick, the colour docker) must not follow the artist back
    // to the brush — switching back restores the colour the brush had.
    const bool eraser = (tool == EraserTool);
    if (m_tool != tool && m_brush.erase != eraser) {
        m_colorByTool[m_brush.erase ? 1 : 0] = m_brush.color;
        m_brush.color = m_colorByTool[eraser ? 1 : 0];
    }
    m_tool = tool;
    // The eraser is a brush mode, so the tool selection drives it.
    m_brush.erase = eraser;
    emit toolChanged();
    emit brushChanged();
}

void AppController::endEyedropper()
{
    if (m_tool != EyedropperTool)
        return;
    // Back to the drawing tool, not to whatever happened to be active before
    // (pan, a selection tool): the point of the picker is to keep painting.
    switch (m_toolBeforeEyedropper) {
    case BrushTool:
    case EraserTool:
    case DynamicBrushTool:
    case BlendTool:
        setTool(m_toolBeforeEyedropper);
        break;
    default:
        setTool(BrushTool);
        break;
    }
}

QString AppController::brushName() const
{
    if (!m_presetIntact)
        return tr("Custom Brush");
    return presetName(m_activePreset);
}

int AppController::activePresetIndex() const { return m_activePreset; }

QString AppController::statusInfo() const
{
    const auto s = m_doc.canvasSize();
    return tr("RGB/Alpha (8-bit integer/channel) - %1 - %2 ?- %3 px - %4 MiB")
        .arg(QString::fromStdString(m_doc.colorProfile()))
        .arg(s.width)
        .arg(s.height)
        .arg(documentMemoryMiB(), 0, 'f', 1);
}

qreal AppController::documentMemoryMiB() const
{
    const auto s = m_doc.canvasSize();
    const qint64 bytesPerLayer = static_cast<qint64>(s.width) * s.height * 4;
    const qint64 total = bytesPerLayer * static_cast<qint64>(m_doc.layers().size());
    return static_cast<qreal>(total) / (1024.0 * 1024.0);
}

// ------------------------------------------------------------- brush presets

int AppController::presetCount() const { return static_cast<int>(m_presets.size()); }

QString AppController::presetName(int index) const
{
    if (index < 0 || index >= presetCount())
        return {};
    return QString::fromStdString(m_presets[static_cast<std::size_t>(index)].name);
}

QImage AppController::presetThumbnail(int index, int width, int height) const
{
    return presetThumbnailRaw(index, width, height);
}

QImage AppController::presetThumbnailRaw(int index, int width, int height) const
{
    if (index < 0 || index >= presetCount())
        return {};
    const auto thumb =
        canvas::renderPresetThumbnail(m_presets[static_cast<std::size_t>(index)].brush, width,
                                      height, core::raster::kOpaqueBlack);
    // Own the bytes: QImage would otherwise point at a buffer that dies here.
    return QImage(thumb.data(), thumb.width(), thumb.height(), thumb.width() * 4,
                  QImage::Format_RGBA8888)
        .copy();
}

void AppController::selectPreset(int index)
{
    if (index < 0 || index >= presetCount())
        return;
    const auto preset = m_presets[static_cast<std::size_t>(index)];
    m_brush = preset.brush;
    // The preset carries no color: keep the artist's current one.
    m_brush.color = toRaster(brushColor());
    m_activePreset = index;
    m_presetIntact = true;
    m_tool = m_brush.erase ? EraserTool : BrushTool;
    emit brushChanged();
    emit toolChanged();
}

void AppController::addPreset()
{
    canvas::BrushPreset preset;
    preset.name = "Custom " + std::to_string(++m_customPresetCounter);
    preset.brush = m_brush;
    m_presets.push_back(preset);
    m_activePreset = presetCount() - 1;
    m_presetIntact = true;
    saveUserPresets();
    emit presetsChanged();
    emit brushChanged();
}

bool AppController::exportBrush(const QUrl& url, int index)
{
    if (index < 0 || index >= presetCount())
        return false;
    const auto& preset = m_presets[static_cast<std::size_t>(index)];
    return io::ProjectIO::exportBrush(preset, localPathFrom(url));
}

bool AppController::importBrush(const QUrl& url)
{
    const auto path = localPathFrom(url);
    QString errorMsg;
    const auto maybePreset = io::ProjectIO::importBrush(path, &errorMsg);
    if (!maybePreset.has_value()) {
        setLastError(errorMsg);
        return false;
    }
    m_presets.push_back(maybePreset.value());
    m_activePreset = presetCount() - 1;
    m_presetIntact = true;
    saveUserPresets();
    emit presetsChanged();
    emit brushChanged();
    return true;
}

void AppController::removePreset(int index)
{
    if (index < 0 || index >= presetCount())
        return;
    // Always keep at least one preset - an empty brush list is unusable.
    if (presetCount() <= 1)
        return;
    m_presets.erase(m_presets.begin() + index);
    m_activePreset = std::clamp(m_activePreset, 0, presetCount() - 1);
    saveUserPresets();
    emit presetsChanged();
    emit brushChanged();
}

void AppController::movePreset(int index, int delta)
{
    const int target = index + delta;
    if (index < 0 || index >= presetCount() || target < 0 || target >= presetCount())
        return;
    std::swap(m_presets[static_cast<std::size_t>(index)],
              m_presets[static_cast<std::size_t>(target)]);
    m_activePreset = target;
    emit presetsChanged();
    emit brushChanged();
}

QImage AppController::layerThumbnail(int index, int width, int height) const
{
    if (index < 0 || width <= 0 || height <= 0)
        return {};
    // Reads the GUI-thread snapshot, never the document: this runs on Qt's
    // image-provider thread, where touching layer pixels raced with painting.
    QMutexLocker lock(&m_snapshotMutex);
    if (index >= m_layerThumbs.size())
        return {};
    const QImage& thumb = m_layerThumbs[index];
    if (thumb.isNull())
        return {};
    return thumb.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

// -------------------------------------------------------------- canvas tools

void AppController::fillAt(float x, float y)
{
    // Layer buffers live in buffer space; the canvas may sit inside them.
    const auto point = toBuffer(x, y);
    const int lx = point.x;
    const int ly = point.y;
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;

    auto& pixels = m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]->pixels();
    if (m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]->isLocked())
        return;
    if (lx < 0 || ly < 0 || lx >= pixels.width() || ly >= pixels.height())
        return;

    // Find the region WITHOUT writing anything: the scanline fill only walks
    // the connected region, so this stays proportional to the area being
    // filled. Copying the whole layer just to discover the rect (64 MB on a
    // 4000x4000 document) was the bucket-fill lag.
    const auto rect = canvas::floodFillRegion(
        pixels, lx, ly, 24,
        [](int, int, prism::core::raster::Color) noexcept {});
    if (rect.isEmpty())
        return;

    // Snapshot the patch, then paint the fill straight into the layer.
    auto before = pixels.copyRect(rect.x, rect.y, rect.width, rect.height);
    canvas::floodFill(pixels, lx, ly, m_brush.color, 24);
    auto after = pixels.copyRect(rect.x, rect.y, rect.width, rect.height);

    // A selection confines the fill, and what is stored must match the command.
    // The mask is canvas-sized, the patch is in buffer space: pass the offset.
    const auto origin = m_doc.canvasOrigin();
    if (!m_selection.isEmpty()) {
        canvas::clipToSelection(after, m_selection, &before, rect.x - origin.x,
                                rect.y - origin.y);
        pixels.blit(after, rect.x, rect.y);
    }

    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, "Fill"));

    emit historyChanged();
    // Only the filled rectangle changed. The compositor takes canvas coordinates.
    const auto composited = m_compositor.composite(toCanvas(rect));
    ++m_layerRevision;
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

QColor AppController::pickColorAt(float x, float y, bool apply)
{
    const int lx = static_cast<int>(std::floor(x));
    const int ly = static_cast<int>(std::floor(y));
    const auto& result = m_compositor.result();
    if (lx < 0 || ly < 0 || lx >= result.width() || ly >= result.height())
        return brushColor();

    // Sample the flattened canvas - what the artist sees is what they pick.
    const auto c = result.at(lx, ly);
    const QColor picked(c.r, c.g, c.b, 255);
    if (apply)
        setBrushColor(picked);
    return picked;
}

void AppController::zoomStep(bool out)
{
    // The canvas sheet is centred in the viewport, so zooming about the
    // centre keeps the working area in view without scroll bookkeeping.
    setZoom(out ? m_zoom / 1.25 : m_zoom * 1.25);
}

qreal AppController::zoomAt(qreal anchorX, qreal anchorY, qreal factor)
{
    // Exposes the anchor for the view to keep under the cursor; the sheet is
    // centred, so the value is informational for QML's scrolling.
    setZoom(m_zoom * factor);
    return m_zoom;
}

void AppController::moveActiveLayer(int dx, int dy)
{
    if ((dx == 0 && dy == 0) || m_activeLayer < 0
        || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked() || layer.pixels().isEmpty())
        return;
    m_history.execute(std::make_unique<core::MoveLayerContentCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), dx, dy));
    emit historyChanged();
    rebuildFullComposite();
}

// ------------------------------------------------------------- layer drag

void AppController::beginLayerMove()
{
    m_movingLayer = false;
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked() || layer.pixels().isEmpty())
        return;

    // Snapshot once per drag. Every preview step re-shifts THIS copy, so the
    // content cannot accumulate drag error, and the history entry at the end
    // gets the pre-drag pixels.
    // ponytail: one full-buffer copy + one bounds scan on mouse-down. Both are
    // O(canvas) (~30ms on 4000x4000); a persistent per-layer content rect would
    // remove them if drag-start latency ever shows up.
    m_moveBefore = layer.pixels();
    m_moveBbox = contentBounds(m_moveBefore);
    if (m_moveBbox.isEmpty())
        return;
    m_moveDx = 0;
    m_moveDy = 0;
    m_movingLayer = true;
}

core::geometry::Rect AppController::contentBounds(const core::raster::RasterBuffer& buffer)
{
    int x0 = buffer.width(), y0 = buffer.height(), x1 = -1, y1 = -1;
    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            if (buffer.at(x, y).a == 0)
                continue;
            x0 = std::min(x0, x);
            y0 = std::min(y0, y);
            x1 = std::max(x1, x);
            y1 = std::max(y1, y);
        }
    }
    if (x1 < 0)
        return {};
    return core::geometry::Rect{ x0, y0, x1 - x0 + 1, y1 - y0 + 1 };
}

void AppController::restoreOutsideCanvas(const core::geometry::Rect& dirty,
                                         core::raster::RasterBuffer& pixels,
                                         const core::raster::RasterBuffer& before)
{
    const auto canvas = m_doc.canvasRect();
    if (pixels.width() <= canvas.x + canvas.width
        && pixels.height() <= canvas.y + canvas.height)
        return; // no hidden margin: nothing can have been touched
    const int x1 = std::min(dirty.x + dirty.width, pixels.width());
    const int y1 = std::min(dirty.y + dirty.height, pixels.height());
    for (int y = std::max(0, dirty.y); y < y1; ++y) {
        const bool outsideRow = y < canvas.y || y >= canvas.y + canvas.height;
        for (int x = std::max(0, dirty.x); x < x1; ++x) {
            if (!outsideRow && x >= canvas.x && x < canvas.x + canvas.width)
                continue; // inside the canvas: keep the edit
            const bool known = x < before.width() && y < before.height();
            pixels.set(x, y, known ? before.at(x, y) : core::raster::kTransparent);
        }
    }
}

core::geometry::Point AppController::toBuffer(float x, float y) const
{
    const auto origin = m_doc.canvasOrigin();
    return core::geometry::Point{ static_cast<int>(std::floor(x)) + origin.x,
                                  static_cast<int>(std::floor(y)) + origin.y };
}

core::geometry::Rect AppController::toBuffer(core::geometry::Rect r) const
{
    const auto origin = m_doc.canvasOrigin();
    r.x += origin.x;
    r.y += origin.y;
    return r;
}

core::geometry::Rect AppController::toCanvas(core::geometry::Rect r) const
{
    const auto origin = m_doc.canvasOrigin();
    r.x -= origin.x;
    r.y -= origin.y;
    return r;
}

void AppController::previewLayerMove(int dx, int dy)
{
    if (!m_movingLayer || (dx == m_moveDx && dy == m_moveDy))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    auto& pixels = layer.pixels();

    // Only the content's own rectangle is touched: clearing the old one and
    // re-blitting the snapshot's keeps this O(content), not O(canvas), so a big
    // image stays draggable.
    const core::geometry::Rect previous{ m_moveBbox.x + m_moveDx, m_moveBbox.y + m_moveDy,
                                         m_moveBbox.width, m_moveBbox.height };
    for (int y = std::max(0, previous.y);
         y < std::min(pixels.height(), previous.y + previous.height); ++y) {
        const auto row = static_cast<std::size_t>(y) * pixels.width();
        for (int x = std::max(0, previous.x);
             x < std::min(pixels.width(), previous.x + previous.width); ++x)
            pixels.set(x, y, core::raster::kTransparent);
    }
    pixels.blit(m_moveBefore.copyRect(m_moveBbox.x, m_moveBbox.y, m_moveBbox.width,
                                      m_moveBbox.height),
                m_moveBbox.x + dx, m_moveBbox.y + dy);
    m_moveDx = dx;
    m_moveDy = dy;

    const core::geometry::Rect next{ m_moveBbox.x + dx, m_moveBbox.y + dy, m_moveBbox.width,
                                     m_moveBbox.height };
    const int ux = std::min(previous.x, next.x);
    const int uy = std::min(previous.y, next.y);
    const int uw = std::max(previous.x + previous.width, next.x + next.width) - ux;
    const int uh = std::max(previous.y + previous.height, next.y + next.height) - uy;
    const auto composited = m_compositor.composite(core::geometry::Rect{ ux, uy, uw, uh });
    ++m_layerRevision;
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::endLayerMove()
{
    if (!m_movingLayer)
        return;
    m_movingLayer = false;
    const int dx = m_moveDx;
    const int dy = m_moveDy;
    if (dx != 0 || dy != 0) {
        // The layer is already showing the moved result; the command carries the
        // pre-drag pixels so undo restores exactly one push of the stack.
        m_history.execute(std::make_unique<core::MoveLayerContentCommand>(
            m_doc, static_cast<std::size_t>(m_activeLayer), dx, dy, std::move(m_moveBefore)));
        emit historyChanged();
    }
    m_moveBefore = {};
}

// ------------------------------------------------------------------ selection

bool AppController::hasSelection() const { return !m_selection.isEmpty(); }
int AppController::selectionX() const { return m_selection.bounds().x; }
int AppController::selectionY() const { return m_selection.bounds().y; }
int AppController::selectionW() const { return m_selection.bounds().width; }
int AppController::selectionH() const { return m_selection.bounds().height; }
int AppController::selectionRevision() const { return m_selection.revision(); }

void AppController::notifySelectionChanged()
{
    // Publish the new overlay BEFORE announcing the change: QML re-fetches the
    // image the moment the revision changes, and Qt serves it on another thread,
    // so emitting first let the request race the new snapshot and render nothing
    // (invisible crop frame, invisible selection).
    refreshSelectionOverlay();
    emit selectionChanged();
}

void AppController::setSelection(float x, float y, float w, float h)
{
    const auto s = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, s.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, s.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, s.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, s.height);
    m_selection.selectRect(core::geometry::Rect{ x0, y0, x1 - x0, y1 - y0 });
    notifySelectionChanged();
}

void AppController::setLassoSelection(const QVariantList& points)
{
    std::vector<core::geometry::Point> poly;
    poly.reserve(points.size());
    for (const auto& v : points) {
        const QPointF p = v.toPointF();
        poly.push_back(core::geometry::Point{ static_cast<int>(p.x()), static_cast<int>(p.y()) });
    }
    m_selection.selectPolygon(poly);
    notifySelectionChanged();
}

void AppController::magicWandAt(float x, float y, int tolerance)
{
    // Sample the flattened canvas: selecting by what the artist sees is the
    // least surprising behaviour when several layers overlap.
    m_selection.selectSimilar(m_compositor.result(), static_cast<int>(std::floor(x)),
                              static_cast<int>(std::floor(y)), tolerance);
    notifySelectionChanged();
}

void AppController::clearSelection()
{
    m_selection.clear();
    notifySelectionChanged();
}

void AppController::selectAll()
{
    m_selection.selectAll();
    notifySelectionChanged();
}

void AppController::invertSelection()
{
    m_selection.invert();
    notifySelectionChanged();
}

void AppController::selectLayerContent()
{
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    // The mask is canvas-sized; the layer buffer may be larger after a crop, so
    // select from the canvas window of it.
    const auto canvas = m_doc.canvasRect();
    m_selection.selectFromAlpha(m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]
                                    ->pixels()
                                    .copyRect(canvas.x, canvas.y, canvas.width, canvas.height));
    notifySelectionChanged();
}

void AppController::setEllipseSelection(float x, float y, float w, float h)
{
    const auto s = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, s.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, s.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, s.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, s.height);
    m_selection.selectEllipse(core::geometry::Rect{ x0, y0, x1 - x0, y1 - y0 });
    notifySelectionChanged();
}

void AppController::similarColorAt(float x, float y, int tolerance)
{
    m_selection.selectSimilarGlobal(m_compositor.result(), static_cast<int>(std::floor(x)),
                                    static_cast<int>(std::floor(y)), tolerance);
    notifySelectionChanged();
}

void AppController::selectOutlineAt(float x, float y)
{
    const auto& flat = m_compositor.result();
    m_selection.selectContiguousOpaque(flat, static_cast<int>(std::floor(x)),
                                       static_cast<int>(std::floor(y)));
    notifySelectionChanged();
}

// ------------------------------------------------------------ shape tools

void AppController::beginShape(float x, float y)
{
    m_shapeStart = core::geometry::Point{ static_cast<int>(std::floor(x)),
                                          static_cast<int>(std::floor(y)) };
    m_shapeEnd = m_shapeStart;
    m_shapeActive = true;
    m_preview.fill(core::raster::kTransparent);
    m_strokeDirty = {};
}

void AppController::updateShape(float x, float y)
{
    if (!m_shapeActive)
        return;
    const auto previous = m_strokeDirty;
    m_shapeEnd = core::geometry::Point{ static_cast<int>(std::floor(x)),
                                        static_cast<int>(std::floor(y)) };

    // Redraw the preview from scratch (a shape is replaced, not accumulated),
    // but only clear where the previous shape was: clearing the whole preview
    // buffer every mouse move cost a full-canvas memset.
    clearPreviewRegion(m_strokeDirty);
    const canvas::ShapeKind kind =
        m_tool == Tool::LineTool       ? canvas::ShapeKind::Line
        : m_tool == Tool::RectangleTool ? canvas::ShapeKind::Rectangle
                                        : canvas::ShapeKind::Ellipse;
    const auto shapeRect =
        canvas::rasterizeShape(m_preview, kind, m_shapeStart, m_shapeEnd, m_brush, m_shapeFilled);

    // Repaint the union of where the shape was and where it is now, so the
    // stale outline is erased from the screen.
    core::geometry::Rect dirty = shapeRect;
    if (!previous.isEmpty()) {
        dirty = core::geometry::Rect{
            std::min(previous.x, shapeRect.x), std::min(previous.y, shapeRect.y),
            std::max(previous.x + previous.width, shapeRect.x + shapeRect.width)
                - std::min(previous.x, shapeRect.x),
            std::max(previous.y + previous.height, shapeRect.y + shapeRect.height)
                - std::min(previous.y, shapeRect.y) };
    }
    m_strokeDirty = shapeRect;
    const auto composited = m_compositor.composite(dirty, &m_preview);
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::endShape()
{
    if (!m_shapeActive)
        return;
    m_shapeActive = false;
    clearPreviewRegion(m_strokeDirty);
    m_previewDirty = {};

    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size())) {
        rebuildFullComposite();
        return;
    }
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked()) {
        rebuildFullComposite();
        return;
    }

    // Draw on a copy of the layer, then commit with the same command the brush
    // uses. The layer itself is left untouched by rasterising, so the "before"
    // patch can be read straight out of it — an extra full copy of the layer
    // per shape bought nothing.
    core::raster::RasterBuffer work = layer.pixels();

    const auto origin = m_doc.canvasOrigin();
    const auto offset = [&origin](core::geometry::Point p) {
        return core::geometry::Point{ p.x + origin.x, p.y + origin.y };
    };
    core::geometry::Rect rect;
    if (m_tool == Tool::GradientTool) {
        rect = canvas::linearGradient(work, offset(m_shapeStart), offset(m_shapeEnd), m_brush.color,
                                      core::raster::kTransparent);
    } else {
        const canvas::ShapeKind kind =
            m_tool == Tool::LineTool       ? canvas::ShapeKind::Line
            : m_tool == Tool::RectangleTool ? canvas::ShapeKind::Rectangle
                                            : canvas::ShapeKind::Ellipse;
        rect = canvas::rasterizeShape(work, kind, offset(m_shapeStart), offset(m_shapeEnd), m_brush,
                                      m_shapeFilled);
    }

    if (rect.isEmpty()) {
        rebuildFullComposite();
        return;
    }
    canvas::clipToSelection(work, m_selection, &layer.pixels(), rect.x - origin.x,
                            rect.y - origin.y);

    auto before = layer.pixels().copyRect(rect.x, rect.y, rect.width, rect.height);
    auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, m_tool == Tool::GradientTool ? "Gradient" : "Shape"));

    emit historyChanged();
    // Only the shape's rectangle changed.
    const auto composited = m_compositor.composite(toCanvas(rect));
    ++m_layerRevision;
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::setShapeFilled(bool filled)
{
    m_shapeFilled = filled;
}

QSize AppController::clipboardImageSize() const
{
    const QImage image = QGuiApplication::clipboard()->image();
    return image.isNull() ? QSize() : image.size();
}

void AppController::commitPatch(const core::raster::RasterBuffer& original,
                               const core::raster::RasterBuffer& work,
                               core::geometry::Rect rect, const char* label)
{
    if (rect.isEmpty() || m_activeLayer < 0
        || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    // `rect` is in the layer buffer's space. Only the canvas is editable: a crop
    // keeps the cropped-away pixels around and they must not collect edits.
    const auto canvas = m_doc.canvasRect();
    rect = rect.intersected(canvas);
    if (rect.isEmpty())
        return;
    auto before = original.copyRect(rect.x, rect.y, rect.width, rect.height);
    auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, label));
    emit historyChanged();
    // Only the patch changed: recompositing the whole canvas here made every
    // shape, fill and filter cost the canvas size instead of the edit size.
    const auto composited = m_compositor.composite(toCanvas(rect));
    ++m_layerRevision;
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

// -------------------------------------------------------- click-based paths

void AppController::addPathPoint(float x, float y)
{
    m_pathPoints.push_back(core::geometry::Point{ static_cast<int>(std::floor(x)),
                                                  static_cast<int>(std::floor(y)) });
}

void AppController::cancelPath()
{
    m_pathPoints.clear();
    m_freePath.clear();
    m_preview.fill(core::raster::kTransparent);
    rebuildFullComposite();
}

int AppController::pathPointCount() const { return static_cast<int>(m_pathPoints.size()); }

void AppController::finishPath(int mode)
{
    if (m_pathPoints.size() < 2) {
        cancelPath();
        return;
    }
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(std::max(0, m_activeLayer))];
    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;

    core::geometry::Rect rect;
    const char* label = "Path";
    // The path is collected in canvas coordinates; the layer buffer may be
    // offset by a crop.
    const auto origin = m_doc.canvasOrigin();
    const auto toBufferPoint = [&origin](core::geometry::Point p) {
        return core::geometry::Point{ p.x + origin.x, p.y + origin.y };
    };
    switch (mode) {
    case 1: { // Bezier: control handles derived from the middle points
        const auto& p = m_pathPoints;
        std::vector<canvas::StrokePoint> path;
        for (std::size_t i = 0; i + 1 < p.size(); ++i) {
            const auto& a = p[i];
            const auto& b = p[i + 1];
            const auto seg = canvas::bezierPath(
                a, core::geometry::Point{ (a.x * 2 + b.x) / 3, (a.y * 2 + b.y) / 3 },
                core::geometry::Point{ (a.x + b.x * 2) / 3, (a.y + b.y * 2) / 3 }, b, 24);
            path.insert(path.end(), seg.begin(), seg.end());
        }
        for (auto& point : path) {
            point.x += static_cast<float>(origin.x);
            point.y += static_cast<float>(origin.y);
        }
        rect = canvas::strokeAlong(work, path, m_brush);
        label = "Bezier";
        break;
    }
    case 2:
        m_selection.selectSmoothPolygon(m_pathPoints);
        m_pathPoints.clear();
        notifySelectionChanged();
        rebuildFullComposite();
        return;
    case 3:
        m_selection.selectMagnetic(m_compositor.result(), m_pathPoints);
        m_pathPoints.clear();
        notifySelectionChanged();
        rebuildFullComposite();
        return;
    default: { // Polyline
        std::vector<canvas::StrokePoint> path;
        for (const auto& p : m_pathPoints) {
            const auto b = toBufferPoint(p);
            path.push_back({ static_cast<float>(b.x), static_cast<float>(b.y), 1.0F });
        }
        rect = canvas::strokeAlong(work, path, m_brush);
        label = "Polyline";
        break;
    }
    }
    m_pathPoints.clear();
    canvas::clipToSelection(work, m_selection, &original, rect.x - origin.x,
                            rect.y - origin.y);
    commitPatch(original, work, rect, label);
}

// ------------------------------------------------------------ freehand path

// ------------------------------------------------------------- lasso fill

void AppController::beginLassoFill(float x, float y)
{
    m_lassoFill.clear();
    m_lassoFill.push_back(core::geometry::Point{ static_cast<int>(std::floor(x)),
                                                static_cast<int>(std::floor(y)) });
}

void AppController::extendLassoFill(float x, float y)
{
    const core::geometry::Point p{ static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)) };
    // Sample sparsely: the enclosure only needs enough points to describe its
    // outline, and the mask is built from a polygon.
    if (!m_lassoFill.empty()) {
        const auto& last = m_lassoFill.back();
        if (std::abs(last.x - p.x) + std::abs(last.y - p.y) < 3)
            return;
    }
    m_lassoFill.push_back(p);
}

void AppController::endLassoFill()
{
    const auto points = m_lassoFill;
    m_lassoFill.clear();
    if (points.size() < 3)
        return;
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked())
        return;

    // Reuse the selection mask as the fill stencil: the polygon maths already
    // exists and gives antialiased coverage for free.
    canvas::Selection stencil;
    stencil.reset(m_doc.canvasSize().width, m_doc.canvasSize().height);
    stencil.selectPolygon(points);

    core::geometry::Rect rect{ points[0].x, points[0].y, 1, 1 };
    for (const auto& p : points) {
        rect = core::geometry::Rect{
            std::min(rect.x, p.x), std::min(rect.y, p.y),
            std::max(rect.x + rect.width, p.x + 1) - std::min(rect.x, p.x),
            std::max(rect.y + rect.height, p.y + 1) - std::min(rect.y, p.y) };
    }
    rect = rect.intersected(core::geometry::Rect{ 0, 0, m_doc.canvasSize().width,
                                                  m_doc.canvasSize().height });
    if (rect.isEmpty())
        return;
    // The stencil is canvas-sized; the layer patch is in buffer space.
    const auto origin = m_doc.canvasOrigin();
    rect = toBuffer(rect);

    auto& pixels = layer.pixels();
    auto before = pixels.copyRect(rect.x, rect.y, rect.width, rect.height);
    auto after = before;
    const auto color = m_brush.color;
    for (int y = 0; y < after.height(); ++y) {
        for (int x = 0; x < after.width(); ++x) {
            const std::uint8_t cov =
                stencil.coverageAt(x + rect.x - origin.x, y + rect.y - origin.y);
            if (cov == 0)
                continue;
            if (cov == 255) {
                after.set(x, y, color);
                continue;
            }
            // Soft edge: lay the colour over what is already there.
            const auto dst = after.at(x, y);
            const float t = static_cast<float>(cov) / 255.0F;
            const auto ch = [t](std::uint8_t a, std::uint8_t b) {
                return static_cast<std::uint8_t>(a * (1.0F - t) + b * t + 0.5F);
            };
            after.set(x, y, core::raster::Color{ ch(dst.r, color.r), ch(dst.g, color.g),
                                                 ch(dst.b, color.b),
                                                 std::max(dst.a, color.a) });
        }
    }
    pixels.blit(after, rect.x, rect.y);

    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, "Lasso Fill"));
    emit historyChanged();
    const auto composited = m_compositor.composite(toCanvas(rect));
    ++m_layerRevision;
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::beginFreePath(float x, float y)
{
    m_freePath.clear();
    m_freePath.push_back({ x, y, 1.0F });
    m_preview.fill(core::raster::kTransparent);
    m_strokeDirty = {};
}

void AppController::extendFreePath(float x, float y)
{
    if (m_freePath.empty())
        return;
    const auto previous = m_strokeDirty;
    m_freePath.push_back({ x, y, 1.0F });
    const auto smoothed = canvas::smoothPath(m_freePath, 1);
    m_preview.fill(core::raster::kTransparent);
    const auto rect = canvas::strokeAlong(m_preview, smoothed, m_brush);
    core::geometry::Rect dirty = rect;
    if (!previous.isEmpty())
        dirty = core::geometry::Rect{
            std::min(previous.x, rect.x), std::min(previous.y, rect.y),
            std::max(previous.x + previous.width, rect.x + rect.width) - std::min(previous.x, rect.x),
            std::max(previous.y + previous.height, rect.y + rect.height) - std::min(previous.y, rect.y) };
    m_strokeDirty = rect;
    const auto composited = m_compositor.composite(dirty, &m_preview);
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::endFreePath()
{
    if (m_freePath.size() < 2) {
        cancelPath();
        return;
    }
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(std::max(0, m_activeLayer))];
    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;
    std::vector<canvas::StrokePoint> path = canvas::smoothPath(m_freePath, 2);
    offsetToBuffer(path);
    const auto rect = canvas::strokeAlong(work, path, m_brush);
    m_freePath.clear();
    m_preview.fill(core::raster::kTransparent);
    canvas::clipToSelection(work, m_selection, &original, rect.x - m_doc.canvasOrigin().x,
                            rect.y - m_doc.canvasOrigin().y);
    commitPatch(original, work, rect, "Freehand path");
}

void AppController::offsetToBuffer(std::vector<canvas::StrokePoint>& path) const
{
    const auto origin = m_doc.canvasOrigin();
    if (origin.x == 0 && origin.y == 0)
        return;
    for (auto& point : path) {
        point.x += static_cast<float>(origin.x);
        point.y += static_cast<float>(origin.y);
    }
}

// --------------------------------------------------------------------- arc

void AppController::beginArc(float x, float y)
{
    m_shapeStart = core::geometry::Point{ static_cast<int>(x), static_cast<int>(y) };
    m_shapeEnd = m_shapeStart;
    m_shapeActive = true;
    m_preview.fill(core::raster::kTransparent);
}

void AppController::updateArc(float x, float y)
{
    if (!m_shapeActive)
        return;
    const auto previous = m_strokeDirty;
    m_shapeEnd = core::geometry::Point{ static_cast<int>(x), static_cast<int>(y) };
    m_preview.fill(core::raster::kTransparent);
    const auto rect = canvas::strokeAlong(m_preview, canvas::arcPath(m_shapeStart, m_shapeEnd),
                                          m_brush);
    core::geometry::Rect dirty = rect;
    if (!previous.isEmpty())
        dirty = core::geometry::Rect{
            std::min(previous.x, rect.x), std::min(previous.y, rect.y),
            std::max(previous.x + previous.width, rect.x + rect.width) - std::min(previous.x, rect.x),
            std::max(previous.y + previous.height, rect.y + rect.height) - std::min(previous.y, rect.y) };
    m_strokeDirty = rect;
    const auto composited = m_compositor.composite(dirty, &m_preview);
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::endArc()
{
    if (!m_shapeActive) {
        return;
    }
    m_shapeActive = false;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(std::max(0, m_activeLayer))];
    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;
    std::vector<canvas::StrokePoint> path = canvas::arcPath(m_shapeStart, m_shapeEnd);
    offsetToBuffer(path);
    const auto rect = canvas::strokeAlong(work, path, m_brush);
    m_preview.fill(core::raster::kTransparent);
    canvas::clipToSelection(work, m_selection, &original, rect.x - m_doc.canvasOrigin().x,
                            rect.y - m_doc.canvasOrigin().y);
    commitPatch(original, work, rect, "Arc");
}

// ------------------------------------------------------ pattern / crop / etc

void AppController::fillPattern(int kind, int scale)
{
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(std::max(0, m_activeLayer))];
    if (layer.isLocked())
        return;
    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;
    const auto kindEnum = static_cast<canvas::PatternKind>(std::clamp(kind, 0, 2));
    core::geometry::Rect limit = m_selection.isEmpty()
                                     ? core::geometry::Rect{}
                                     : toBuffer(m_selection.bounds());
    const auto rect = canvas::fillPattern(work, kindEnum, scale, m_brush.color, limit);
    commitPatch(original, work, rect, "Pattern");
}

bool AppController::cutSelectionToLayer(float x, float y, float w, float h)
{
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return false;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked() || layer.pixels().isEmpty()) {
        setStatusMessage(tr("This layer is locked - nothing to cut."));
        return false;
    }

    const auto size = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, size.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, size.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, size.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, size.height);
    if (x1 - x0 < 2 || y1 - y0 < 2) {
        setStatusMessage(tr("Select a larger area to cut out."));
        return false;
    }

    // Nothing but transparency in the frame: say so instead of adding an empty
    // layer the artist then has to hunt for.
    const auto rect = toBuffer(core::geometry::Rect{ x0, y0, x1 - x0, y1 - y0 });
    if (contentBounds(layer.pixels().copyRect(rect.x, rect.y, rect.width, rect.height)).isEmpty()) {
        setStatusMessage(tr("Nothing to cut there - that area is empty."));
        return false;
    }

    const int sourceIndex = m_activeLayer;
    m_history.execute(std::make_unique<core::CutRegionToLayerCommand>(
        m_doc, static_cast<std::size_t>(sourceIndex), "Cut piece", rect.x, rect.y, rect.width,
        rect.height));
    m_activeLayer = static_cast<int>(m_doc.layers().size()) - 1;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();

    // Placing the piece is the next move, so hand over the Move tool.
    setTool(MoveTool);
    setStatusMessage(tr("Cut to a new layer - drag it into place."));
    return true;
}

void AppController::cropTo(float x, float y, float w, float h)
{
    const auto size = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, size.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, size.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, size.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, size.height);
    const int cw = x1 - x0;
    const int ch = y1 - y0;
    if (cw < 2 || ch < 2) {
        // A click (or a 1px twitch) must say so instead of silently doing
        // nothing while the rubber band disappears.
        setStatusMessage(tr("Crop area is too small - drag a larger rectangle."));
        return;
    }

    // Through the history, so a crop can be taken back. This used to mutate
    // every layer and then clear the undo stack: one misplaced drag destroyed
    // all the artist's undo steps, and the crop itself was irreversible.
    m_history.execute(std::make_unique<core::CropCommand>(m_doc, x0, y0, cw, ch));
    m_compositor.canvasResized();
    m_selection.reset(cw, ch);
    m_preview = core::raster::RasterBuffer(cw, ch);
    m_previewDirty = {};
    m_strokeDirty = {};
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();

    // Hand over the Move tool: the next thing anyone does after a crop is place
    // the image, and the layer still holds whatever was outside the frame.
    setTool(MoveTool);
    setStatusMessage(
        tr("Cropped to %1x%2. Drag the layer to bring the cropped part back.")
            .arg(cw)
            .arg(ch));
}

void AppController::transformLayer(float degrees, float scale)
{
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(std::max(0, m_activeLayer))];
    if (layer.isLocked())
        return;
    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;
    const auto rect = canvas::transformBuffer(work, degrees, scale,
                                              static_cast<float>(work.width()) * 0.5F,
                                              static_cast<float>(work.height()) * 0.5F);
    commitPatch(original, work, rect, "Transform");
}

void AppController::renderText(float x, float y, const QString& text, int pixelSize)
{
    if (text.isEmpty())
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(std::max(0, m_activeLayer))];
    if (layer.isLocked())
        return;

    // Rasterize with Qt's font engine into an RGBA image of the layer's size,
    // then treat it as a normal pixel patch.
    const int w = layer.pixels().width();
    const int h = layer.pixels().height();
    QImage canvasImage(w, h, QImage::Format_RGBA8888);
    canvasImage.fill(Qt::transparent);
    {
        QPainter p(&canvasImage);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont font = p.font();
        font.setPixelSize(std::max(6, pixelSize));
        p.setFont(font);
        const QColor c = brushColor();
        p.setPen(c);
        // The text arrives in canvas coordinates; the layer buffer may be offset.
        const auto origin = m_doc.canvasOrigin();
        p.drawText(QPointF(x + origin.x, y + origin.y), text);
    }

    core::raster::RasterBuffer textLayer(w, h);
    std::memcpy(textLayer.data(), canvasImage.constBits(),
                static_cast<std::size_t>(w) * h * 4);

    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;
    // Composite the text over the working copy.
    work.compositeOver(textLayer, 0, 0);
    commitPatch(original, work, core::geometry::Rect{ 0, 0, w, h }, "Text");
}

QImage AppController::selectionOverlay() const
{
    // Read-only snapshot: this is called from Qt's image-provider thread, so it
    // must not touch the selection or the document.
    QMutexLocker lock(&m_snapshotMutex);
    return m_selectionSnapshot;
}

void AppController::refreshSnapshots()
{
    // GUI thread only. Builds every image the providers are allowed to serve.
    constexpr int kThumbW = 48;
    constexpr int kThumbH = 32;
    // The selection overlay is drawn stretched over the canvas by QML, so a
    // bounded version is enough — building it at canvas resolution walked 16M
    // pixels per request on a 4000x4000 document.
    constexpr int kOverlayMax = 512;

    QVector<QImage> thumbs;
    thumbs.reserve(static_cast<int>(m_doc.layers().size()));
    for (const auto& layer : m_doc.layers()) {
        const auto& src = layer->pixels();
        if (src.isEmpty()) {
            thumbs.append(QImage());
            continue;
        }
        core::raster::RasterBuffer out(kThumbW, kThumbH);
        const float sx = static_cast<float>(src.width()) / kThumbW;
        const float sy = static_cast<float>(src.height()) / kThumbH;
        for (int y = 0; y < kThumbH; ++y) {
            for (int x = 0; x < kThumbW; ++x) {
                const int x0 = std::min(src.width() - 1, static_cast<int>(x * sx));
                const int y0 = std::min(src.height() - 1, static_cast<int>(y * sy));
                out.set(x, y, src.at(x0, y0));
            }
        }
        thumbs.append(QImage(out.data(), out.width(), out.height(), out.width() * 4,
                             QImage::Format_RGBA8888)
                          .copy());
    }

    QImage overlay = buildSelectionOverlay();

    {
        QMutexLocker lock(&m_snapshotMutex);
        m_layerThumbs = std::move(thumbs);
        m_selectionSnapshot = std::move(overlay);
        m_snapshotRevision = m_layerRevision;
    }
}

QImage AppController::buildSelectionOverlay() const
{
    QImage overlay;
    const int cw = m_doc.canvasSize().width;
    const int ch = m_doc.canvasSize().height;
    if (m_selection.isEmpty() || cw <= 0 || ch <= 0)
        return overlay;

    // The overlay is drawn stretched over the canvas by QML, so a bounded
    // version is enough — building it at canvas resolution walked 16M pixels per
    // request on a 4000x4000 document.
    constexpr int kOverlayMax = 512;
    const float scale =
        std::min(1.0F, static_cast<float>(kOverlayMax) / static_cast<float>(std::max(cw, ch)));
    const int w = std::max(1, static_cast<int>(cw * scale));
    const int h = std::max(1, static_cast<int>(ch * scale));
    core::raster::RasterBuffer out(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t cov =
                m_selection.coverageAt(std::min(cw - 1, static_cast<int>(x / scale)),
                                       std::min(ch - 1, static_cast<int>(y / scale)));
            if (cov == 0)
                continue;
            // Tint the selected area so any shape (lasso, wand) is visible.
            // Neutral white keeps the overlay inside the two-colour palette.
            out.set(x, y, core::raster::Color{ 255, 255, 255, static_cast<std::uint8_t>(cov / 5) });
        }
    }
    return QImage(out.data(), out.width(), out.height(), out.width() * 4, QImage::Format_RGBA8888)
        .copy();
}

void AppController::refreshSelectionOverlay()
{
    QImage overlay = buildSelectionOverlay();
    QMutexLocker lock(&m_snapshotMutex);
    m_selectionSnapshot = std::move(overlay);
}

// ----------------------------------------------------------------------- view

void AppController::setMirrorHorizontal(bool on)
{
    if (m_mirrorH == on)
        return;
    m_mirrorH = on;
    emit viewChanged();
}

void AppController::setMirrorVertical(bool on)
{
    if (m_mirrorV == on)
        return;
    m_mirrorV = on;
    emit viewChanged();
}

// -------------------------------------------------- start page / recent files

void AppController::setStartPageVisible(bool visible)
{
    if (m_startPageVisible == visible)
        return;
    m_startPageVisible = visible;
    emit startPageChanged();
}

void AppController::loadRecentFiles()
{
    QSettings settings;
    m_recentFiles = settings.value(QStringLiteral("recentFiles")).toStringList();
}

void AppController::loadUserPresets()
{
    QSettings settings;
    const auto data = settings.value(QStringLiteral("userPresets")).toByteArray();
    if (data.isEmpty())
        return;
    
    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return;
    
    const auto arr = doc.array();
    for (const auto& val : arr) {
        if (!val.isObject())
            continue;
        const auto obj = val.toObject();
        const auto name = obj["name"].toString().toStdString();
        const auto radius = static_cast<float>(obj["radius"].toDouble(10.0));
        const auto hardness = static_cast<float>(obj["hardness"].toDouble(1.0));
        const auto flow = static_cast<float>(obj["flow"].toDouble(1.0));
        const auto erase = obj["erase"].toBool(false);
        const auto pressureOpacity = obj["pressureOpacity"].toBool(false);
        const auto colorStr = obj["color"].toString("#000000");
        const QColor qc(colorStr);
        const auto color = core::raster::Color{
            static_cast<std::uint8_t>(qc.red()),
            static_cast<std::uint8_t>(qc.green()),
            static_cast<std::uint8_t>(qc.blue()),
            255
        };
        
        canvas::BrushPreset preset;
        preset.name = name;
        preset.brush.radius = std::clamp(radius, 1.0F, 200.0F);
        preset.brush.hardness = std::clamp(hardness, 0.0F, 1.0F);
        preset.brush.flow = std::clamp(flow, 0.01F, 1.0F);
        preset.brush.erase = erase;
        preset.brush.pressureOpacity = pressureOpacity;
        preset.brush.color = color;
        preset.brush.tip = io::ProjectIO::brushTipFromJson(obj["tip"].toObject());
        m_presets.push_back(preset);
    }
}

void AppController::saveUserPresets()
{
    // Save only the user presets (not the built-ins). The built-ins are
    // always loaded from builtinBrushPresets(), so we save starting from
    // the first preset beyond the built-in count.
    const auto builtinCount = canvas::builtinBrushPresets().size();
    if (m_presets.size() <= builtinCount)
        return; // no user presets to save
    
    QJsonArray arr;
    for (std::size_t i = builtinCount; i < m_presets.size(); ++i) {
        const auto& preset = m_presets[i];
        QJsonObject obj;
        obj["name"] = QString::fromStdString(preset.name);
        obj["radius"] = preset.brush.radius;
        obj["hardness"] = preset.brush.hardness;
        obj["flow"] = preset.brush.flow;
        obj["erase"] = preset.brush.erase;
        obj["pressureOpacity"] = preset.brush.pressureOpacity;
        const QColor qc(preset.brush.color.r, preset.brush.color.g, preset.brush.color.b);
        obj["color"] = qc.name();
        obj["tip"] = io::ProjectIO::brushTipToJson(preset.brush.tip);
        arr.append(obj);
    }
    
    const QJsonDocument doc(arr);
    QSettings settings;
    settings.setValue(QStringLiteral("userPresets"), doc.toJson(QJsonDocument::Compact));
}

void AppController::addRecentFile(const QString& path)
{
    // The selftest drives these same code paths on throwaway documents. Letting
    // them into the Recent list put a test file ("prism-blend.prism", layer at
    // ColorDodge 60%) on the start page, where opening it made every stroke
    // look grey — a test artifact presented as the user's own document.
    if (path.isEmpty() || m_selfTestMode)
        return;
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 12)
        m_recentFiles.removeLast();
    QSettings settings;
    settings.setValue(QStringLiteral("recentFiles"), m_recentFiles);
    emit recentFilesChanged();
}

void AppController::forgetRecentFilesUnder(const QString& directory)
{
    const QString prefix = QDir(directory).absolutePath() + QLatin1Char('/');
    QStringList kept;
    for (const QString& path : m_recentFiles) {
        if (!path.startsWith(prefix, Qt::CaseInsensitive))
            kept.append(path);
    }
    if (kept.size() == m_recentFiles.size())
        return;
    m_recentFiles = kept;
    QSettings settings;
    settings.setValue(QStringLiteral("recentFiles"), m_recentFiles);
    emit recentFilesChanged();
}

QVariantList AppController::recentFiles() const
{
    QVariantList out;
    for (const QString& path : m_recentFiles) {
        const QFileInfo info(path);
        if (!info.exists())
            continue; // don't offer files the user deleted
        QVariantMap entry;
        entry["name"] = info.fileName();
        entry["path"] = path;
        entry["url"] = QUrl::fromLocalFile(path).toString();
        out.append(entry);
    }
    return out;
}

void AppController::clearRecentFiles()
{
    m_recentFiles.clear();
    QSettings settings;
    settings.remove(QStringLiteral("recentFiles"));
    emit recentFilesChanged();
}

// ------------------------------------------------------------ tool shortcuts

namespace {

/// Built-in single-key defaults. Tools without a natural key start keyless;
/// the user can assign one in Settings.
struct ToolDefault {
    int tool;
    const char* name;
    const char* key;
};

const std::vector<ToolDefault>& toolDefaults()
{
    static const std::vector<ToolDefault> defaults = {
        { 0, "Freehand Brush", "B" },          // BrushTool
        { 1, "Eraser", "E" },                  // EraserTool
        { 2, "Line", "L" },                    // LineTool
        { 3, "Rectangle", "R" },               // RectangleTool
        { 4, "Ellipse", "O" },                 // EllipseTool
        { 5, "Gradient", "G" },                // GradientTool
        { 6, "Fill", "F" },                    // FillTool
        { 7, "Color Picker", "I" },            // EyedropperTool
        { 8, "Pan", "" },                      // PanTool
        { 9, "Zoom", "Z" },                    // ZoomTool
        { 10, "Rectangular Selection", "M" },  // RectSelectTool
        { 11, "Elliptical Selection", "" },    // EllipseSelectTool
        { 12, "Freehand Selection", "" },      // LassoSelectTool
        { 13, "Polygon Selection", "" },       // PolygonSelectTool
        { 14, "Contiguous Selection", "W" },   // MagicWandTool
        { 15, "Similar Color Selection", "" }, // SimilarColorSelectTool
        { 16, "Move Layer", "V" },             // MoveTool
        { 17, "Text", "" },                    // TextTool
        { 18, "Polyline", "" },                // PolylineTool
        { 19, "Bezier Curve", "" },            // BezierTool
        { 20, "Freehand Path", "" },           // FreehandPathTool
        { 21, "Arc", "" },                     // ArcTool
        { 22, "Multibrush", "" },              // MultiBrushTool
        { 23, "Dynamic Brush", "" },           // DynamicBrushTool
        { 24, "Bezier Selection", "" },        // BezierSelectTool
        { 25, "Magnetic Selection", "" },      // MagneticSelectTool
        { 26, "Outline Selection", "" },       // OutlineSelectTool
        { 27, "Contiguous Selection (all)", "" }, // ContiguousSelectTool
        { 28, "Transform", "" },               // TransformTool
        { 29, "Crop", "" },                    // CropTool
        { 30, "Pattern", "" },                 // PatternTool
    };
    return defaults;
}

QString settingsKeyForTool(int tool)
{
    return QStringLiteral("shortcuts/%1").arg(tool);
}

} // namespace

QString AppController::defaultToolShortcut(int tool) const
{
    for (const auto& d : toolDefaults())
        if (d.tool == tool)
            return QString::fromLatin1(d.key);
    return {};
}

QString AppController::toolShortcut(int tool) const
{
    QSettings settings;
    // An override exists only when it was explicitly set; otherwise default.
    return settings.contains(settingsKeyForTool(tool))
               ? settings.value(settingsKeyForTool(tool)).toString()
               : defaultToolShortcut(tool);
}

void AppController::setToolShortcut(int tool, const QString& key)
{
    // Normalize: a single character, uppercase; "" is a valid (keyless) value.
    QString normalized = key.trimmed().toUpper();
    if (normalized.size() > 1)
        normalized = normalized.left(1);

    // A shortcut claimed twice would fire ambiguously, so any other tool that
    // currently answers to this key is reset to keyless.
    if (!normalized.isEmpty()) {
        for (const auto& d : toolDefaults()) {
            if (d.tool == tool)
                continue;
            if (toolShortcut(d.tool) == normalized)
                QSettings().setValue(settingsKeyForTool(d.tool), QString());
        }
    }

    QSettings().setValue(settingsKeyForTool(tool), normalized);
    emit toolShortcutsChanged();
}

QVariantList AppController::toolShortcutList() const
{
    QVariantList out;
    for (const auto& d : toolDefaults()) {
        QVariantMap entry;
        entry["tool"] = d.tool;
        entry["name"] = QString::fromLatin1(d.name);
        entry["key"] = toolShortcut(d.tool);
        entry["defaultKey"] = QString::fromLatin1(d.key);
        out.append(entry);
    }
    return out;
}

QString AppController::versionString() const
{
    // Version only: callers add the product name, so it isn't duplicated.
    return QCoreApplication::applicationVersion();
}

void AppController::setLastError(const QString& error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
    if (!error.isEmpty())
        qWarning() << "file error:" << error;
}

void AppController::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

// ---------------------------------------------------------------- layer state

void AppController::setLayerLocked(int index, bool locked)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(index)];
    m_history.execute(std::make_unique<core::SetLayerPropertyCommand>(
        m_doc, index, core::SetLayerPropertyCommand::Property::Locked, locked,
        layer.isLocked()));
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
}

void AppController::previewLayerOpacity(int index, int percent)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    // Live feedback only - the history entry is created once, on release.
    m_doc.layers()[static_cast<std::size_t>(index)]->setOpacity(
        static_cast<float>(percent) / 100.0F);
    // The row's own "NN%" label reads the model, so without this notification
    // the percentage stayed put until the drag ended.
    m_layersModel->notifyRow(index, { LayerListModel::OpacityRole });
    rebuildFullComposite();
}

void AppController::commitLayerOpacity(int index, int percent, int previousPercent)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    if (percent == previousPercent) {
        m_layersModel->refresh();
        return;
    }
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(index)];
    // The drag already applied the new value; the command needs the value it
    // is replacing, so it is seeded with previousPercent and re-executed.
    const float before = static_cast<float>(previousPercent) / 100.0F;
    layer.setOpacity(before);
    m_history.execute(std::make_unique<core::SetLayerPropertyCommand>(
        m_doc, index, core::SetLayerPropertyCommand::Property::Opacity,
        static_cast<float>(percent) / 100.0F, before));
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

// --------------------------------------------------------------------- actions

void AppController::addLayer()
{
    const auto name = QStringLiteral("Layer %1").arg(static_cast<int>(m_doc.layers().size()) + 1);
    m_history.execute(std::make_unique<core::AddLayerCommand>(m_doc, name.toStdString()));
    m_activeLayer = static_cast<int>(m_doc.layers().size()) - 1;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
}

void AppController::deleteLayer()
{
    if (m_doc.layers().size() <= 1)
        return;
    const int idx = activeLayerIndex();
    m_history.execute(std::make_unique<core::RemoveLayerCommand>(m_doc, idx));
    m_activeLayer = std::max(0, idx - 1);
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::duplicateLayer()
{
    m_history.execute(
        std::make_unique<core::DuplicateLayerCommand>(m_doc, activeLayerIndex()));
    m_activeLayer = activeLayerIndex() + 1;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::moveLayerUp()
{
    const int idx = activeLayerIndex();
    if (idx >= static_cast<int>(m_doc.layers().size()) - 1)
        return;
    m_history.execute(std::make_unique<core::MoveLayerCommand>(m_doc, idx, idx + 1));
    m_activeLayer = idx + 1;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::moveLayerDown()
{
    const int idx = activeLayerIndex();
    if (idx <= 0)
        return;
    m_history.execute(std::make_unique<core::MoveLayerCommand>(m_doc, idx, idx - 1));
    m_activeLayer = idx - 1;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

QStringList AppController::blendModeNames() const
{
    // Order MUST match the BlendMode enum values: the combo box's
    // currentIndex is written straight into the layer's blend mode.
    static const QStringList names = {
        tr("Normal"),      tr("Multiply"),    tr("Screen"),     tr("Overlay"),
        tr("Darken"),      tr("Lighten"),     tr("Color Dodge"), tr("Color Burn"),
        tr("Hard Light"),  tr("Soft Light"),  tr("Difference"), tr("Exclusion"),
        tr("Add"),         tr("Subtract"),    tr("Divide"),     tr("Hue"),
        tr("Saturation"),  tr("Color"),       tr("Luminosity"),
    };
    return names;
}

void AppController::moveLayerTo(int fromIndex, int toIndex)
{
    const int count = static_cast<int>(m_doc.layers().size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count
        || fromIndex == toIndex)
        return;
    // Krita-style drag & drop: the dragged layer lands where it was dropped and
    // the layers in between shift by one. Undo restores the previous order.
    m_history.execute(std::make_unique<core::MoveLayerCommand>(
        m_doc, static_cast<std::size_t>(fromIndex), static_cast<std::size_t>(toIndex)));
    m_activeLayer = toIndex;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::setLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(index)];
    m_history.execute(std::make_unique<core::SetLayerPropertyCommand>(
        m_doc, index, core::SetLayerPropertyCommand::Property::Visible, visible,
        layer.isVisible()));
    m_layersModel->refresh();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::setLayerOpacity(int index, int percent)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    const float value = static_cast<float>(percent) / 100.0F;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(index)];
    m_history.execute(std::make_unique<core::SetLayerPropertyCommand>(
        m_doc, index, core::SetLayerPropertyCommand::Property::Opacity, value,
        layer.opacity()));
    m_layersModel->refresh();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::setLayerBlendMode(int index, int mode)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(index)];
    const auto value = static_cast<core::BlendMode>(mode);
    m_history.execute(std::make_unique<core::SetLayerPropertyCommand>(
        m_doc, index, core::SetLayerPropertyCommand::Property::BlendMode, value,
        layer.blendMode()));
    m_layersModel->refresh();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::renameLayer(int index, const QString& name)
{
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(index)];
    m_history.execute(std::make_unique<core::SetLayerPropertyCommand>(
        m_doc, index, core::SetLayerPropertyCommand::Property::Name, name.toStdString(),
        layer.name()));
    m_layersModel->refresh();
    emit historyChanged();
}

void AppController::undo()
{
    // History throws when there is nothing to undo (a programming error, not a
    // user error). The menu items are disabled in that state, but the keyboard
    // shortcuts and any scripted caller are not — and an escaping exception
    // killed the whole app, which is what testers saw as a random crash.
    if (!m_history.canUndo())
        return;
    m_history.undo();
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::redo()
{
    if (!m_history.canRedo())
        return;
    m_history.redo();
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::clearCanvas()
{
    m_history.execute(std::make_unique<core::ClearLayerCommand>(
        m_doc, static_cast<std::size_t>(activeLayerIndex())));
    emit historyChanged();
    rebuildFullComposite();
}

core::geometry::Rect AppController::paintSegmentSymmetrical(
    core::raster::RasterBuffer& buffer, const canvas::BrushSettings& brush,
    const canvas::StrokePoint& a, const canvas::StrokePoint& b)
{
    auto rect = canvas::BrushStroke::paintSegment(buffer, brush, a, b);
    if (m_tool != Tool::MultiBrushTool || !brush.erase)
        return rect;

    // Krita's Multibrush repeats each stroke mirrored about the canvas centre.
    const float w = static_cast<float>(m_doc.canvasSize().width);
    const float h = static_cast<float>(m_doc.canvasSize().height);
    const canvas::StrokePoint mirrorX[2] = { { w - a.x, a.y, a.pressure },
                                             { w - b.x, b.y, b.pressure } };
    const canvas::StrokePoint mirrorY[2] = { { a.x, h - a.y, a.pressure },
                                             { b.x, h - b.y, b.pressure } };
    const canvas::StrokePoint mirrorXY[2] = { { w - a.x, h - a.y, a.pressure },
                                              { w - b.x, h - b.y, b.pressure } };

    const canvas::StrokePoint* variants[3][2] = { { mirrorX, mirrorX + 1 },
                                                  { mirrorY, mirrorY + 1 },
                                                  { mirrorXY, mirrorXY + 1 } };
    for (auto& v : variants) {
        const auto r = canvas::BrushStroke::paintSegment(buffer, brush, v[0][0], v[1][0]);
        if (r.isEmpty())
            continue;
        rect = core::geometry::Rect{
            std::min(rect.x, r.x), std::min(rect.y, r.y),
            std::max(rect.x + rect.width, r.x + r.width) - std::min(rect.x, r.x),
            std::max(rect.y + rect.height, r.y + r.height) - std::min(rect.y, r.y) };
    }
    return rect;
}

bool AppController::canMergeDown() const
{
    return activeLayerIndex() > 0;
}

void AppController::mergeLayerDown()
{
    if (!canMergeDown())
        return;
    m_history.execute(std::make_unique<core::MergeLayerDownCommand>(
        m_doc, static_cast<std::size_t>(activeLayerIndex())));
    m_activeLayer = std::max(0, activeLayerIndex() - 1);
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::applyFilter(const QString& id)
{
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked() || layer.pixels().isEmpty())
        return;

    // Filter a working copy so the "before" patch comes from the untouched
    // layer, then commit through the same command a stroke uses.
    core::raster::RasterBuffer work = layer.pixels();
    const core::raster::RasterBuffer original = layer.pixels();

    core::geometry::Rect touched;
    if (id == QLatin1String("invert"))
        touched = canvas::filters::invert(work);
    else if (id == QLatin1String("desaturate"))
        touched = canvas::filters::desaturate(work);
    else if (id == QLatin1String("blur"))
        touched = canvas::filters::gaussianBlurApprox(work, 4);
    else if (id == QLatin1String("brightness-contrast"))
        touched = canvas::filters::brightnessContrast(work, 1.1F, 1.2F);
    else
        return;

    if (touched.isEmpty())
        return;

    // A selection confines the filter; outside pixels come back untouched. The
    // mask is canvas-sized while the filter works on the layer buffer, so the
    // touched rect carries the offset.
    const auto filterOrigin = m_doc.canvasOrigin();
    canvas::clipToSelection(work, m_selection, &original, touched.x - filterOrigin.x,
                            touched.y - filterOrigin.y);

    auto before = original.copyRect(touched.x, touched.y, touched.width, touched.height);
    auto after = work.copyRect(touched.x, touched.y, touched.width, touched.height);

    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        touched.x, touched.y, id.toStdString()));

    emit historyChanged();
    rebuildFullComposite();
}

// ----------------------------------------------------------------------- file

void AppController::fitToViewport(qreal viewportWidth, qreal viewportHeight)
{
    const auto s = m_doc.canvasSize();
    if (s.width <= 0 || s.height <= 0 || viewportWidth <= 0 || viewportHeight <= 0)
        return;
    const qreal fit = std::min(viewportWidth / s.width, viewportHeight / s.height);
    setZoom(std::min(fit, 1.0));
}

bool AppController::saveProject(const QUrl& url)
{
    QString path = localPathFrom(url);
    // A project file without the .prism suffix is invisible to the Open
    // dialog's filter — that is why reopened saves appeared to be missing.
    if (!path.endsWith(QStringLiteral(".prism"), Qt::CaseInsensitive))
        path += QStringLiteral(".prism");

    const bool ok = io::ProjectIO::save(m_doc, path);
    if (ok) {
        addRecentFile(path);
        setDocumentName(QFileInfo(path).fileName());
    } else {
        qWarning() << "save failed:" << path;
    }
    return ok;
}

bool AppController::saveFile(const QUrl& url)
{
    QString path = localPathFrom(url);
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty() || suffix == QLatin1String("prism"))
        return saveProject(QUrl::fromLocalFile(path));

    const bool ok = io::ProjectIO::exportImage(m_compositor, path, suffix.toUtf8());
    if (ok)
        addRecentFile(path);
    else
        qWarning() << "export failed:" << path;
    return ok;
}

bool AppController::openFile(const QUrl& url)
{
    const QString path = localPathFrom(url);
    if (path.endsWith(QStringLiteral(".prism"), Qt::CaseInsensitive))
        return openProject(QUrl::fromLocalFile(path));
    return importImage(QUrl::fromLocalFile(path));
}

QStringList AppController::writableFormatNames() const
{
    QStringList out;
    for (const QByteArray& f : io::ProjectIO::writableFormats())
        out << QString::fromLatin1(f);
    return out;
}

QStringList AppController::imageOpenFilters() const
{
    const QList<QByteArray> formats = io::ProjectIO::readableFormats();
    QStringList out = filterStringsFor(formats);
    // "Everything we can read" goes FIRST, because the first entry is the
    // filter the dialog opens with. Defaulting to a per-format filter means a
    // folder whose only image is a .jpg looks empty under "PNG image (*.png)",
    // and the user concludes the app cannot import their picture at all.
    QStringList patterns;
    for (const QByteArray& f : formats)
        patterns << QStringLiteral("*.") + QString::fromLatin1(f);
    if (!patterns.isEmpty())
        out.prepend(tr("Images (%1)").arg(patterns.join(QLatin1Char(' '))));
    return out;
}

QStringList AppController::imageSaveFilters() const
{
    // A project file is always the first choice when saving; flat image formats
    // follow.
    QStringList out = filterStringsFor(io::ProjectIO::writableFormats());
    out.prepend(tr("Prism document (*.prism)"));
    return out;
}

bool AppController::openProject(const QUrl& url)
{
    const QString filePath = localPathFrom(url);
    auto doc = io::ProjectIO::load(filePath);
    if (!doc) {
        setLastError(tr("Could not open %1 — it is not a valid Prism project.")
                         .arg(QFileInfo(filePath).fileName()));
        return false;
    }
    setLastError(QString());
    m_doc = std::move(*doc);
    m_history.clear();
    m_compositor.canvasResized();
    m_activeLayer = 0;
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_selection.reset(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_documentName = QFileInfo(filePath).fileName();
    m_layersModel->setDocument(&m_doc);
    addRecentFile(filePath);
    emit documentNameChanged();
    emit layersChanged();
    emit historyChanged();
    notifySelectionChanged();
    // The start page was covering the canvas, so a successfully opened file
    // looked like nothing happened. Opening a document must dismiss it.
    setStartPageVisible(false);
    rebuildFullComposite();
    return true;
}

bool AppController::importImage(const QUrl& url)
{
    const QString path = localPathFrom(url);
    auto doc = io::ProjectIO::importImage(path);
    if (!doc) {
        setLastError(tr("Could not open %1 — unsupported or unreadable image.")
                         .arg(QFileInfo(path).fileName()));
        return false;
    }
    setLastError(QString());
    m_doc = std::move(*doc);
    m_history.clear();
    m_compositor.canvasResized();
    m_activeLayer = 0;
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_selection.reset(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_documentName = QFileInfo(path).fileName();
    m_layersModel->setDocument(&m_doc);
    addRecentFile(path);
    emit documentNameChanged();
    emit layersChanged();
    emit historyChanged();
    notifySelectionChanged();
    setStartPageVisible(false);
    rebuildFullComposite();
    return true;
}

bool AppController::importImageAsLayer(const QUrl& url)
{
    const QString path = localPathFrom(url);
    // No canvas to add to yet: behave like Open image (creates a document).
    if (m_startPageVisible || m_doc.layers().empty())
        return importImage(QUrl::fromLocalFile(path));
    QImage image(path);
    if (image.isNull()) {
        setLastError(tr("Could not import %1 — unsupported or unreadable image.")
                         .arg(QFileInfo(path).fileName()));
        return false;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);

    const auto cs = m_doc.canvasSize();
    const auto origin = m_doc.canvasOrigin();
    auto layer = std::make_unique<core::Layer>(
        core::Layer::Type::Raster, QFileInfo(path).completeBaseName().toStdString());
    // Canvas-at-origin, like every other layer: a crop moves the canvas window
    // inside the buffers, so a canvas-sized layer would be read at the wrong
    // place. The image is written into the canvas rectangle of that buffer.
    const int bufferW = cs.width + origin.x;
    const int bufferH = cs.height + origin.y;
    layer->pixels() = core::raster::RasterBuffer(bufferW, bufferH);

    // Center the image on the canvas; parts larger than the canvas are
    // clipped (no silent resample — pixel-exact placement).
    const int dx = (cs.width - image.width()) / 2 + origin.x;
    const int dy = (cs.height - image.height()) / 2 + origin.y;
    const int x0 = std::max(origin.x, dx);
    const int y0 = std::max(origin.y, dy);
    const int x1 = std::min(origin.x + cs.width, dx + image.width());
    const int y1 = std::min(origin.y + cs.height, dy + image.height());
    for (int y = y0; y < y1; ++y) {
        const auto dstRow = static_cast<std::size_t>(y) * bufferW + static_cast<std::size_t>(x0);
        const auto srcRow = static_cast<std::size_t>(y - dy) * image.width()
            + static_cast<std::size_t>(x0 - dx);
        std::memcpy(layer->pixels().data() + dstRow * 4, image.constBits() + srcRow * 4,
                    static_cast<std::size_t>(x1 - x0) * 4);
    }

    setLastError(QString());
    const std::string name = layer->name();
    m_history.execute(
        std::make_unique<core::AddLayerCommand>(m_doc, name, std::move(layer)));
    m_activeLayer = static_cast<int>(m_doc.layers().size()) - 1;
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();

    // Placing without resampling means an oversized image is cropped. Say so
    // instead of leaving the user to wonder where the rest of the photo went.
    // (Only point at tools that actually exist: Resize Canvas is not
    // implemented yet, so the advice is the Transform tool or a bigger doc.)
    const int clippedX = std::max(0, image.width() - (x1 - x0));
    const int clippedY = std::max(0, image.height() - (y1 - y0));
    if (clippedX > 0 || clippedY > 0)
        setStatusMessage(tr("Imported \"%1\" as a layer (%2x%3). Canvas is %4x%5, so "
                            "%6x%7 px were cropped — scale the layer with the Transform "
                            "tool, or start a document large enough for it.")
                             .arg(QString::fromStdString(name))
                             .arg(image.width())
                             .arg(image.height())
                             .arg(cs.width)
                             .arg(cs.height)
                             .arg(clippedX)
                             .arg(clippedY));
    else
        setStatusMessage(tr("Imported \"%1\" as a layer (%2x%3, centered). Drag to place it.")
                             .arg(QString::fromStdString(name))
                             .arg(image.width())
                             .arg(image.height()));

    // Placing the image is the obvious next step, so hand over the Move tool:
    // leaving the brush active meant the first drag painted on top of the import
    // instead of moving it, which read as "the imported layer cannot be moved".
    setTool(MoveTool);
    return true;
}

bool AppController::exportPng(const QUrl& url)
{
    return io::ProjectIO::exportPng(m_compositor, localPathFrom(url));
}

void AppController::newDocument(int width, int height)
{
    createDocument(width, height, tr("Untitled"), 1, QColor(255, 255, 255), 100,
                   BackgroundAsRasterLayer);
}

void AppController::createDocument(int width, int height, const QString& name, int layerCount,
                                  const QColor& background, int backgroundOpacity,
                                  int backgroundMode)
{
    const int w = std::max(1, width);
    const int h = std::max(1, height);
    m_doc = core::Document(core::CanvasSize{ w, h, 72 });

    const int layers = std::max(1, layerCount);
    for (int i = 0; i < layers; ++i)
        m_doc.addRasterLayer(layers == 1 ? "Layer 1" : ("Layer " + std::to_string(i + 1)));

    auto& bottom = *m_doc.layers().front();
    const auto fill = core::raster::Color{
        static_cast<std::uint8_t>(background.red()),
        static_cast<std::uint8_t>(background.green()),
        static_cast<std::uint8_t>(background.blue()),
        static_cast<std::uint8_t>(std::clamp(backgroundOpacity, 0, 100) * 255 / 100)
    };

    switch (backgroundMode) {
    case BackgroundAsCanvasColor:
        // Krita's "canvas color" lives outside the layer stack; the closest
        // equivalent here is a filled bottom layer at the given opacity.
        bottom.setName("Background");
        bottom.pixels().fill(fill);
        break;
    case BackgroundAsFillLayer: {
        // A separate fill layer beneath the working layers.
        auto layer = std::make_unique<core::Layer>(core::Layer::Type::Raster, "Fill Layer");
        layer->pixels() = core::raster::RasterBuffer(w, h);
        layer->pixels().fill(fill);
        m_doc.insertLayer(0, std::move(layer));
        break;
    }
    case BackgroundAsRasterLayer:
    default:
        // Leave layer 1 blank; artists usually want to paint from scratch.
        break;
    }

    m_documentName = name.isEmpty() ? tr("Untitled") : name;
    m_history.clear();
    m_compositor.canvasResized();
    // Select the topmost paint layer so drawing can start immediately.
    m_activeLayer = layers - 1;
    m_selection.reset(w, h);
    m_preview = core::raster::RasterBuffer(w, h);
    m_layersModel->setDocument(&m_doc);
    emit layersChanged();
    emit historyChanged();
    emit documentNameChanged();
    notifySelectionChanged();
    setStartPageVisible(false);
    rebuildFullComposite();
}

QString AppController::documentName() const { return m_documentName; }

void AppController::setDocumentName(const QString& name)
{
    if (m_documentName == name)
        return;
    m_documentName = name;
    emit documentNameChanged();
}

QString AppController::windowTitle() const
{
    return tr("%1 (%2 MiB) - Prism").arg(m_documentName).arg(documentMemoryMiB(), 0, 'f', 1);
}

// ------------------------------------------------------------------- stroking

float AppController::shapedPressure(float pressure) const
{
    // Gamma curve: >1 hardens (light presses stay thin/faint), <1 softens.
    const float p = std::clamp(pressure, 0.0F, 1.0F);
    return std::pow(p, static_cast<float>(m_pressureCurve));
}

void AppController::beginStroke(float x, float y, float pressure)
{
    m_currentStroke.clear();
    m_smoothedPressure = pressure;
    m_currentStroke.push_back({ x, y, shapedPressure(pressure) });
    // Only the region the previous stroke touched can still hold preview
    // pixels; clearing the whole canvas here (a full-buffer memset, 64 MB on a
    // 4000x4000 document) was pure overhead on every mouse-down.
    clearPreviewRegion(m_previewDirty);
    m_strokeDirty = {};
    m_previewDirty = {};
    m_dynamicPos = { x, y, 1.0F };

    // The eraser edits the ACTIVE layer directly and keeps one snapshot for
    // undo. It used to keep a second full copy of the layer as a write buffer
    // plus a compositor layer override, which cost 128 MB of copying on a
    // 4000x4000 document every time the eraser touched down, to show exactly
    // what editing the layer in place shows anyway.
    auto& activeLayer = *m_doc.layers()[static_cast<std::size_t>(activeLayerIndex())];
    m_erasing = m_brush.erase && m_activeLayer >= 0
             && m_activeLayer < static_cast<int>(m_doc.layers().size())
             && !activeLayer.isLocked();
    if (m_erasing) {
        m_eraseBefore = activeLayer.pixels();
        m_compositor.clearLayerOverride();
    } else {
        m_eraseBefore = {};
        m_compositor.clearLayerOverride();
    }

    // Blend tool: smears the layer in place, so it needs the same live-edit
    // snapshot the eraser uses for undo.
    m_blending = m_tool == Tool::BlendTool && !activeLayer.isLocked();
    if (m_blending) {
        m_eraseBefore = activeLayer.pixels();
        m_carried = core::raster::Color{ 0, 0, 0, 0 }; // picked up on the first stamp
    }

    // Stamp the first point right away, like Krita: a click paints a dot, and
    // it keeps the accumulated preview identical to a full re-paint (which is
    // what the commit below relies on).
    if (m_erasing) {
        // Erasing edits the layer buffer, which lives in buffer space.
        auto dotPoint = m_currentStroke.front();
        dotPoint.x += static_cast<float>(m_doc.canvasOrigin().x);
        dotPoint.y += static_cast<float>(m_doc.canvasOrigin().y);
        const auto dot = paintSegmentSymmetrical(activeLayer.pixels(), m_brush, dotPoint, dotPoint);
        if (!dot.isEmpty()) {
            m_strokeDirty = dot;
            const auto composited = m_compositor.composite(toCanvas(dot));
            emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        }
    } else {
        canvas::BrushSettings dotBrush = m_brush;
        dotBrush.erase = false;
        const auto dot = paintSegmentSymmetrical(m_preview, dotBrush, m_currentStroke.front(),
                                                m_currentStroke.front());
        if (!dot.isEmpty()) {
            m_strokeDirty = dot;
            m_previewDirty = dot;
            const auto composited = m_compositor.composite(dot, &m_preview, false);
            emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        }
    }
}

void AppController::clearPreviewRegion(const core::geometry::Rect& rect)
{
    if (rect.isEmpty() || m_preview.isEmpty())
        return;
    const int x1 = std::min(rect.x + rect.width, m_preview.width());
    const int y1 = std::min(rect.y + rect.height, m_preview.height());
    for (int y = std::max(0, rect.y); y < y1; ++y)
        for (int x = std::max(0, rect.x); x < x1; ++x)
            m_preview.set(x, y, core::raster::kTransparent);
}

void AppController::extendStroke(float x, float y, float pressure)
{
    if (m_currentStroke.empty())
        return;
    const auto prev = m_currentStroke.back();
    // One bad sample must not spike the stamp: the raw pen signal dips at tip
    // contact and jitters sample to sample, so follow it with a lag.
    m_smoothedPressure += (pressure - m_smoothedPressure) * 0.4F;
    const float penPressure = m_smoothedPressure;
    // Dynamic Brush lags behind the pointer: the painted point eases toward the
    // input instead of jumping to it, which is what gives the tool its weight.
    if (m_tool == Tool::DynamicBrushTool) {
        constexpr float kFollow = 0.35F;
        m_dynamicPos.x += (x - m_dynamicPos.x) * kFollow;
        m_dynamicPos.y += (y - m_dynamicPos.y) * kFollow;
        m_dynamicPos.pressure = shapedPressure(penPressure);
        m_currentStroke.push_back(m_dynamicPos);
    } else {
        m_currentStroke.push_back({ x, y, shapedPressure(penPressure) });
    }
    const auto current = m_currentStroke.back();

    if (m_blending) {
        // Blend: smear the layer in place and re-composite the band.
        if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size())) {
            m_blending = false;
            return;
        }
        auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
        const auto origin = m_doc.canvasOrigin();
        canvas::StrokePoint bufferPrev = prev;
        canvas::StrokePoint bufferCurrent = current;
        bufferPrev.x += static_cast<float>(origin.x);
        bufferPrev.y += static_cast<float>(origin.y);
        bufferCurrent.x += static_cast<float>(origin.x);
        bufferCurrent.y += static_cast<float>(origin.y);
        const auto band = canvas::BrushStroke::smudgeSegment(layer.pixels(), m_brush, bufferPrev,
                                                            bufferCurrent, m_carried);
        if (band.isEmpty())
            return;
        m_strokeDirty = core::geometry::Rect{
            std::min(m_strokeDirty.x, band.x), std::min(m_strokeDirty.y, band.y),
            std::max(m_strokeDirty.x + m_strokeDirty.width, band.x + band.width)
                - std::min(m_strokeDirty.x, band.x),
            std::max(m_strokeDirty.y + m_strokeDirty.height, band.y + band.height)
                - std::min(m_strokeDirty.y, band.y) };
        const auto composited = m_compositor.composite(toCanvas(band));
        emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        return;
    }

    if (m_erasing) {
        // Erase straight into the active layer and re-composite that band. Only
        // the active layer changes, so no compositor override is needed and the
        // other layers stay untouched on screen. The layer index is re-checked
        // because a layer can disappear mid-drag (undo, delete, mouse button
        // still held), which used to index past the end of the stack.
        if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size())) {
            m_erasing = false;
            m_compositor.clearLayerOverride();
            return;
        }
        auto& activeLayer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
        const auto origin = m_doc.canvasOrigin();
        canvas::StrokePoint bufferPrev = prev;
        canvas::StrokePoint bufferCurrent = current;
        bufferPrev.x += static_cast<float>(origin.x);
        bufferPrev.y += static_cast<float>(origin.y);
        bufferCurrent.x += static_cast<float>(origin.x);
        bufferCurrent.y += static_cast<float>(origin.y);
        const auto segment =
            paintSegmentSymmetrical(activeLayer.pixels(), m_brush, bufferPrev, bufferCurrent);
        if (segment.isEmpty())
            return;
        m_strokeDirty = core::geometry::Rect{
            std::min(m_strokeDirty.x, segment.x), std::min(m_strokeDirty.y, segment.y),
            std::max(m_strokeDirty.x + m_strokeDirty.width, segment.x + segment.width)
                - std::min(m_strokeDirty.x, segment.x),
            std::max(m_strokeDirty.y + m_strokeDirty.height, segment.y + segment.height)
                - std::min(m_strokeDirty.y, segment.y) };
        const auto composited = m_compositor.composite(toCanvas(segment));
        emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        return;
    }

    // The eraser preview accumulates COVERAGE (a normal stamp into the scratch);
    // the compositor then subtracts it, so the erase is visible live instead of
    // only appearing when the button is released.
    const bool erasing = m_brush.erase;
    canvas::BrushSettings previewBrush = m_brush;
    previewBrush.erase = false;

    const auto segment = paintSegmentSymmetrical(m_preview, previewBrush, prev, current);
    if (segment.isEmpty())
        return;

    // Track the whole stroke's extent for the final commit,
    m_strokeDirty = core::geometry::Rect{
        std::min(m_strokeDirty.x, segment.x), std::min(m_strokeDirty.y, segment.y),
        std::max(m_strokeDirty.x + m_strokeDirty.width, segment.x + segment.width)
            - std::min(m_strokeDirty.x, segment.x),
        std::max(m_strokeDirty.y + m_strokeDirty.height, segment.y + segment.height)
            - std::min(m_strokeDirty.y, segment.y) };

    // but only recomposite the band this tick actually painted. Everything
    // else already holds base+preview from previous ticks, so redrawing the
    // entire stroke bbox every mouse move was wasted work.
    m_previewDirty = core::geometry::Rect{
        std::min(m_previewDirty.x, segment.x), std::min(m_previewDirty.y, segment.y),
        std::max(m_previewDirty.x + m_previewDirty.width, segment.x + segment.width)
            - std::min(m_previewDirty.x, segment.x),
        std::max(m_previewDirty.y + m_previewDirty.height, segment.y + segment.height)
            - std::min(m_previewDirty.y, segment.y) };
    // The layer-revision counter is deliberately NOT bumped here: doing so
    // made every layer thumbnail re-render on each mouse move, which was the
    // cause of the drawing lag. Thumbnails refresh when the stroke commits.
    const auto composited = m_compositor.composite(segment, &m_preview, erasing);
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::endStroke()
{
    if (m_currentStroke.empty())
        return;
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;

    auto& activeLayer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (activeLayer.isLocked()) {
        // Locked layers swallow the edit rather than silently ignoring lock.
        m_currentStroke.clear();
        m_preview.fill(core::raster::kTransparent);
        rebuildFullComposite();
        return;
    }

    if (m_blending) {
        // The drag already smeared the layer in place; commit that patch so undo
        // restores what was there.
        m_blending = false;
        m_currentStroke.clear();
        const auto rect = m_strokeDirty;
        m_strokeDirty = {};
        if (rect.isEmpty()) {
            m_eraseBefore = {};
            return;
        }
        auto& pixels = activeLayer.pixels();
        restoreOutsideCanvas(rect, pixels, m_eraseBefore);
        const auto origin = m_doc.canvasOrigin();
        auto before = m_eraseBefore.copyRect(rect.x, rect.y, rect.width, rect.height);
        auto after = pixels.copyRect(rect.x, rect.y, rect.width, rect.height);
        canvas::clipToSelection(after, m_selection, &before, rect.x - origin.x, rect.y - origin.y);
        pixels.blit(after, rect.x, rect.y);
        m_eraseBefore = {};
        m_history.execute(std::make_unique<core::StrokeCommand>(
            m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
            rect.x, rect.y, "Blend"));
        emit historyChanged();
        const auto composited = m_compositor.composite(toCanvas(rect));
        ++m_layerRevision;
        emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        return;
    }

    if (m_erasing) {
        // The drag already built the erased layer incrementally, so commit
        // exactly that. Re-painting the whole stroke here used to shrink the
        // patch rect (pixels already at alpha 0 report "no change"), which left
        // part of the erase uncommitted — erased strokes came back. It also
        // subtracted alpha twice, over-erasing soft edges.
        m_erasing = false;
        m_compositor.clearLayerOverride();
        m_currentStroke.clear();

        const auto rect = m_strokeDirty; // union of everything erased this drag
        m_strokeDirty = {};
        if (rect.isEmpty()) {
            // Nothing was erased: the layer was never touched, so there is
            // nothing to undo either.
            m_eraseBefore = {};
            return;
        }

        // The layer already holds the erased pixels (the drag edited it live).
        // Clip the patch to the selection and record the before/after pair so
        // undo restores exactly what was there.
        auto& layerPixels = activeLayer.pixels();
        restoreOutsideCanvas(rect, layerPixels, m_eraseBefore);
        const auto origin = m_doc.canvasOrigin();
        auto before = m_eraseBefore.copyRect(rect.x, rect.y, rect.width, rect.height);
        auto after = layerPixels.copyRect(rect.x, rect.y, rect.width, rect.height);
        canvas::clipToSelection(after, m_selection, &before, rect.x - origin.x, rect.y - origin.y);
        // What is stored must match the command's "after" patch.
        layerPixels.blit(after, rect.x, rect.y);
        m_eraseBefore = {};

        m_history.execute(std::make_unique<core::StrokeCommand>(
            m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
            rect.x, rect.y, "Erase"));
        emit historyChanged();
        const auto composited = m_compositor.composite(rect);
        ++m_layerRevision;
        emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        return;
    }

    if (m_brush.erase) {
        // Erasing subtracts from the layer's own pixels. The preview scratch
        // starts transparent, so "erasing" into it is a no-op — the erase has
        // to run against a copy of the layer. Deciding this BEFORE painting is
        // what makes the eraser work. (Only reached when the live erase preview
        // was not available, e.g. no active layer.)
        const core::raster::RasterBuffer original = activeLayer.pixels();
        core::raster::RasterBuffer work = original;
        // The stroke points are canvas coordinates; this path paints the layer.
        std::vector<canvas::StrokePoint> points = m_currentStroke;
        offsetToBuffer(points);
        const auto rect = canvas::BrushStroke::paint(work, m_brush, points);
        m_currentStroke.clear();
        clearPreviewRegion(m_previewDirty);
        m_previewDirty = {};
        m_strokeDirty = {};

        if (rect.isEmpty()) {
            return;
        }
        const auto origin = m_doc.canvasOrigin();
        canvas::clipToSelection(work, m_selection, &original, rect.x - origin.x,
                                rect.y - origin.y);
        restoreOutsideCanvas(rect, work, original);

        auto before = original.copyRect(rect.x, rect.y, rect.width, rect.height);
        auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);
        m_history.execute(std::make_unique<core::StrokeCommand>(
            m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
            rect.x, rect.y, "Erase"));
        emit historyChanged();
        // Only the erased rectangle changed; recompositing the whole canvas
        // here was the mouse-up freeze on large documents.
        const auto composited = m_compositor.composite(toCanvas(rect));
        ++m_layerRevision;
        emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
        return;
    }

    // Paint stroke. The preview already holds the WHOLE stroke: extendStroke
    // stamped exactly the segments BrushStroke::paint would re-stamp, in the
    // same order. Re-painting the stroke into a fresh full-canvas scratch and
    // then re-compositing the entire canvas cost ~1.2 s of freeze per stroke on
    // a 4000x4000 document; committing the accumulated preview over the
    // stroke's own rectangle is the same result for a fraction of the work.
    const auto rect = m_strokeDirty;
    const auto previewRect = m_previewDirty;
    m_currentStroke.clear();

    if (rect.isEmpty()) {
        m_erasing = false;
        m_compositor.clearLayerOverride();
        clearPreviewRegion(previewRect);
        m_previewDirty = {};
        m_strokeDirty = {};
        return;
    }

    auto painted = m_preview.copyRect(rect.x, rect.y, rect.width, rect.height);
    // Clip to the active selection, if any, over the patch only.
    canvas::clipToSelection(painted, m_selection, nullptr, rect.x, rect.y);
    clearPreviewRegion(previewRect);
    m_previewDirty = {};
    m_strokeDirty = {};

    // The preview is canvas-sized, the layer is not: place the patch at the
    // buffer-space position. The compositor stays in canvas coordinates.
    const auto bufferRect = toBuffer(rect);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(painted), bufferRect.x,
        bufferRect.y));

    emit historyChanged();
    // Repaint only the stroke's rectangle instead of the whole canvas.
    const auto composited = m_compositor.composite(rect);
    ++m_layerRevision; // thumbnails must re-fetch after a content change
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

// ---------------------------------------------------------------------- internals

QImage AppController::canvasImage() const
{
    const auto& result = m_compositor.result();
    return QImage(result.data(), result.width(), result.height(), result.width() * 4,
                  QImage::Format_RGBA8888);
}

void AppController::rebuildFullComposite()
{
    const auto rect = m_compositor.composite(fullRect(m_doc));
    ++m_layerRevision; // thumbnails must re-fetch after a content change
    emit canvasChanged(rect.x, rect.y, rect.width, rect.height);
}

void AppController::rebuildStrokePreview()
{
    const auto rect = m_compositor.composite(m_strokeDirty, &m_preview);
    ++m_layerRevision;
    emit canvasChanged(rect.x, rect.y, rect.width, rect.height);
}
