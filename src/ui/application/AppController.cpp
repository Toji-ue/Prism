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
#include <QClipboard>
#include <QFont>
#include <QPainter>
#include <QFileInfo>
#include <QGuiApplication>
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
    // Start on the first preset so the toolbar/status bar read like a real app.
    selectPreset(0);
    rebuildFullComposite();
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
    m_tool = tool;
    // The eraser is a brush mode, so the tool selection drives it.
    m_brush.erase = (tool == EraserTool);
    emit toolChanged();
    emit brushChanged();
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
    emit presetsChanged();
    emit brushChanged();
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
    if (index < 0 || index >= static_cast<int>(m_doc.layers().size()))
        return {};
    const auto& src = m_doc.layers()[static_cast<std::size_t>(index)]->pixels();
    if (src.isEmpty() || width <= 0 || height <= 0)
        return {};

    // Averaged downscale with a BOUNDED sample budget: each destination pixel
    // averages at most 4x4 source samples instead of its whole block. Scanning
    // all 1M source pixels per thumbnail was too slow to regenerate during a
    // stroke - part of the drawing lag.
    constexpr int kMaxSamplesPerAxis = 4;
    core::raster::RasterBuffer out(width, height);
    const float sx = static_cast<float>(src.width()) / static_cast<float>(width);
    const float sy = static_cast<float>(src.height()) / static_cast<float>(height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int x0 = static_cast<int>(x * sx);
            const int y0 = static_cast<int>(y * sy);
            const int x1 = std::min(src.width(), x0 + std::max(1, static_cast<int>(sx)));
            const int y1 = std::min(src.height(), y0 + std::max(1, static_cast<int>(sy)));
            const int stepX = std::max(1, (x1 - x0) / kMaxSamplesPerAxis);
            const int stepY = std::max(1, (y1 - y0) / kMaxSamplesPerAxis);
            int r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int yy = y0; yy < y1; yy += stepY) {
                for (int xx = x0; xx < x1; xx += stepX) {
                    const auto c = src.at(xx, yy);
                    // Weight color by alpha so transparent areas don't darken.
                    r += c.r * c.a;
                    g += c.g * c.a;
                    b += c.b * c.a;
                    a += c.a;
                    ++n;
                }
            }
            if (a == 0 || n == 0) {
                out.set(x, y, core::raster::kTransparent);
            } else {
                out.set(x, y,
                        core::raster::Color{ static_cast<std::uint8_t>(r / a),
                                             static_cast<std::uint8_t>(g / a),
                                             static_cast<std::uint8_t>(b / a),
                                             static_cast<std::uint8_t>(a / n) });
            }
        }
    }
    return QImage(out.data(), out.width(), out.height(), out.width() * 4,
                  QImage::Format_RGBA8888)
        .copy();
}

// -------------------------------------------------------------- canvas tools

void AppController::fillAt(float x, float y)
{
    const int lx = static_cast<int>(std::floor(x));
    const int ly = static_cast<int>(std::floor(y));
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;

    auto& pixels = m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]->pixels();
    if (m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]->isLocked())
        return;
    if (lx < 0 || ly < 0 || lx >= pixels.width() || ly >= pixels.height())
        return;

    // Fill a working copy first: that reveals the affected rect, so we can
    // snapshot a matching "before" patch from the untouched layer.
    core::raster::RasterBuffer work = pixels;
    const auto rect = canvas::floodFill(work, lx, ly, m_brush.color, 24);
    if (rect.isEmpty())
        return;

    // A selection confines the fill: pixels outside it come back from the
    // untouched copy.
    canvas::clipToSelection(work, m_selection, &pixels);

    auto before = pixels.copyRect(rect.x, rect.y, rect.width, rect.height);
    auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);

    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, "Fill"));

    emit historyChanged();
    rebuildFullComposite();
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

// ------------------------------------------------------------------ selection

bool AppController::hasSelection() const { return !m_selection.isEmpty(); }
int AppController::selectionX() const { return m_selection.bounds().x; }
int AppController::selectionY() const { return m_selection.bounds().y; }
int AppController::selectionW() const { return m_selection.bounds().width; }
int AppController::selectionH() const { return m_selection.bounds().height; }
int AppController::selectionRevision() const { return m_selection.revision(); }

void AppController::setSelection(float x, float y, float w, float h)
{
    const auto s = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, s.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, s.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, s.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, s.height);
    m_selection.selectRect(core::geometry::Rect{ x0, y0, x1 - x0, y1 - y0 });
    emit selectionChanged();
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
    emit selectionChanged();
}

void AppController::magicWandAt(float x, float y, int tolerance)
{
    // Sample the flattened canvas: selecting by what the artist sees is the
    // least surprising behaviour when several layers overlap.
    m_selection.selectSimilar(m_compositor.result(), static_cast<int>(std::floor(x)),
                              static_cast<int>(std::floor(y)), tolerance);
    emit selectionChanged();
}

void AppController::clearSelection()
{
    m_selection.clear();
    emit selectionChanged();
}

void AppController::selectAll()
{
    m_selection.selectAll();
    emit selectionChanged();
}

void AppController::invertSelection()
{
    m_selection.invert();
    emit selectionChanged();
}

void AppController::selectLayerContent()
{
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    m_selection.selectFromAlpha(
        m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]->pixels());
    emit selectionChanged();
}

void AppController::setEllipseSelection(float x, float y, float w, float h)
{
    const auto s = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, s.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, s.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, s.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, s.height);
    m_selection.selectEllipse(core::geometry::Rect{ x0, y0, x1 - x0, y1 - y0 });
    emit selectionChanged();
}

void AppController::similarColorAt(float x, float y, int tolerance)
{
    m_selection.selectSimilarGlobal(m_compositor.result(), static_cast<int>(std::floor(x)),
                                    static_cast<int>(std::floor(y)), tolerance);
    emit selectionChanged();
}

void AppController::selectOutlineAt(float x, float y)
{
    const auto& flat = m_compositor.result();
    m_selection.selectContiguousOpaque(flat, static_cast<int>(std::floor(x)),
                                       static_cast<int>(std::floor(y)));
    emit selectionChanged();
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

    // Redraw the preview from scratch (a shape is replaced, not accumulated).
    m_preview.fill(core::raster::kTransparent);
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
    m_preview.fill(core::raster::kTransparent);

    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size())) {
        rebuildFullComposite();
        return;
    }
    auto& layer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];
    if (layer.isLocked()) {
        rebuildFullComposite();
        return;
    }

    // Draw on a copy of the layer so the "before" patch stays accurate, then
    // commit with the same command the brush uses.
    const core::raster::RasterBuffer original = layer.pixels();
    core::raster::RasterBuffer work = original;

    core::geometry::Rect rect;
    if (m_tool == Tool::GradientTool) {
        rect = canvas::linearGradient(work, m_shapeStart, m_shapeEnd, m_brush.color,
                                      core::raster::kTransparent);
    } else {
        const canvas::ShapeKind kind =
            m_tool == Tool::LineTool       ? canvas::ShapeKind::Line
            : m_tool == Tool::RectangleTool ? canvas::ShapeKind::Rectangle
                                            : canvas::ShapeKind::Ellipse;
        rect = canvas::rasterizeShape(work, kind, m_shapeStart, m_shapeEnd, m_brush, m_shapeFilled);
    }

    if (rect.isEmpty()) {
        rebuildFullComposite();
        return;
    }
    canvas::clipToSelection(work, m_selection, &original);

    auto before = original.copyRect(rect.x, rect.y, rect.width, rect.height);
    auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, m_tool == Tool::GradientTool ? "Gradient" : "Shape"));

    emit historyChanged();
    rebuildFullComposite();
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
                               const core::geometry::Rect& rect, const char* label)
{
    if (rect.isEmpty() || m_activeLayer < 0
        || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;
    auto before = original.copyRect(rect.x, rect.y, rect.width, rect.height);
    auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
        rect.x, rect.y, label));
    emit historyChanged();
    rebuildFullComposite();
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
        rect = canvas::strokeAlong(work, path, m_brush);
        label = "Bezier";
        break;
    }
    case 2:
        m_selection.selectSmoothPolygon(m_pathPoints);
        m_pathPoints.clear();
        emit selectionChanged();
        rebuildFullComposite();
        return;
    case 3:
        m_selection.selectMagnetic(m_compositor.result(), m_pathPoints);
        m_pathPoints.clear();
        emit selectionChanged();
        rebuildFullComposite();
        return;
    default: { // Polyline
        std::vector<canvas::StrokePoint> path;
        for (const auto& p : m_pathPoints)
            path.push_back({ static_cast<float>(p.x), static_cast<float>(p.y), 1.0F });
        rect = canvas::strokeAlong(work, path, m_brush);
        label = "Polyline";
        break;
    }
    }
    m_pathPoints.clear();
    canvas::clipToSelection(work, m_selection, &original);
    commitPatch(original, work, rect, label);
}

// ------------------------------------------------------------ freehand path

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
    const auto rect = canvas::strokeAlong(work, canvas::smoothPath(m_freePath, 2), m_brush);
    m_freePath.clear();
    m_preview.fill(core::raster::kTransparent);
    canvas::clipToSelection(work, m_selection, &original);
    commitPatch(original, work, rect, "Freehand path");
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
    const auto rect = canvas::strokeAlong(work, canvas::arcPath(m_shapeStart, m_shapeEnd), m_brush);
    m_preview.fill(core::raster::kTransparent);
    canvas::clipToSelection(work, m_selection, &original);
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
                                     : m_selection.bounds();
    const auto rect = canvas::fillPattern(work, kindEnum, scale, m_brush.color, limit);
    commitPatch(original, work, rect, "Pattern");
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
    if (cw < 2 || ch < 2)
        return;

    // Crop each layer, then resize the document. Destructive by nature, so the
    // history is cleared rather than pretending it can be undone.
    for (const auto& layer : m_doc.layers()) {
        if (layer->pixels().isEmpty())
            continue;
        layer->pixels() = layer->pixels().copyRect(x0, y0, cw, ch);
    }
    m_doc.resizeCanvas(core::CanvasSize{ cw, ch, m_doc.canvasSize().dpi });
    m_history.clear();
    m_compositor.canvasResized();
    m_selection.reset(cw, ch);
    m_preview = core::raster::RasterBuffer(cw, ch);
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
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
        p.drawText(QPointF(x, y), text);
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
    const int w = m_doc.canvasSize().width;
    const int h = m_doc.canvasSize().height;
    if (m_selection.isEmpty() || w <= 0 || h <= 0)
        return {};

    // Tint the selected area so any shape (lasso, wand) is visible. Neutral
    // white keeps the overlay inside the app's two-colour palette.
    core::raster::RasterBuffer out(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t cov = m_selection.coverageAt(x, y);
            if (cov == 0)
                continue;
            out.set(x, y, core::raster::Color{ 255, 255, 255,
                                               static_cast<std::uint8_t>(cov / 5) });
        }
    }
    return QImage(out.data(), out.width(), out.height(), out.width() * 4,
                  QImage::Format_RGBA8888)
        .copy();
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

void AppController::addRecentFile(const QString& path)
{
    if (path.isEmpty())
        return;
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 12)
        m_recentFiles.removeLast();
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
    m_history.undo();
    m_layersModel->refresh();
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
}

void AppController::redo()
{
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

    // A selection confines the filter; outside pixels come back untouched.
    canvas::clipToSelection(work, m_selection, &original);

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
    return filterStringsFor(io::ProjectIO::readableFormats());
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
    emit selectionChanged();
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
    emit selectionChanged();
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
    auto layer = std::make_unique<core::Layer>(
        core::Layer::Type::Raster, QFileInfo(path).completeBaseName().toStdString());
    layer->pixels() = core::raster::RasterBuffer(cs.width, cs.height);

    // Center the image on the canvas; parts larger than the canvas are
    // clipped (no silent resample — pixel-exact placement).
    const int dx = (cs.width - image.width()) / 2;
    const int dy = (cs.height - image.height()) / 2;
    const int x0 = std::max(0, dx);
    const int y0 = std::max(0, dy);
    const int x1 = std::min(cs.width, dx + image.width());
    const int y1 = std::min(cs.height, dy + image.height());
    for (int y = y0; y < y1; ++y) {
        const auto dstRow = static_cast<std::size_t>(y) * cs.width + static_cast<std::size_t>(x0);
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
        setStatusMessage(tr("Imported \"%1\" as a layer (%2x%3, centered).")
                             .arg(QString::fromStdString(name))
                             .arg(image.width())
                             .arg(image.height()));
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
    emit selectionChanged();
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

void AppController::beginStroke(float x, float y)
{
    m_currentStroke.clear();
    m_currentStroke.push_back({ x, y, 1.0F });
    m_preview.fill(core::raster::kTransparent);
    m_strokeDirty = core::geometry::Rect{ static_cast<int>(x), static_cast<int>(y), 0, 0 };
    m_dynamicPos = { x, y, 1.0F };

    // The eraser previews by substituting an erased copy of the ACTIVE layer,
    // so nothing on any other layer is touched on screen either.
    m_erasing = m_brush.erase && m_activeLayer >= 0
             && m_activeLayer < static_cast<int>(m_doc.layers().size());
    if (m_erasing) {
        m_eraseBefore = m_doc.layers()[static_cast<std::size_t>(m_activeLayer)]->pixels();
        m_eraseWork = m_eraseBefore;
        m_compositor.setLayerOverride(static_cast<std::size_t>(m_activeLayer), &m_eraseWork);
    } else {
        m_compositor.clearLayerOverride();
    }
}

void AppController::extendStroke(float x, float y)
{
    if (m_currentStroke.empty())
        return;
    const auto prev = m_currentStroke.back();
    // Dynamic Brush lags behind the pointer: the painted point eases toward the
    // input instead of jumping to it, which is what gives the tool its weight.
    if (m_tool == Tool::DynamicBrushTool) {
        constexpr float kFollow = 0.35F;
        m_dynamicPos.x += (x - m_dynamicPos.x) * kFollow;
        m_dynamicPos.y += (y - m_dynamicPos.y) * kFollow;
        m_currentStroke.push_back(m_dynamicPos);
    } else {
        m_currentStroke.push_back({ x, y, 1.0F });
    }
    const auto current = m_currentStroke.back();

    if (m_erasing) {
        // Erase into the substituted layer copy and re-composite that band.
        // No overlay is used, so only the active layer's pixels change.
        const auto segment = paintSegmentSymmetrical(m_eraseWork, m_brush, prev, current);
        if (segment.isEmpty())
            return;
        m_strokeDirty = core::geometry::Rect{
            std::min(m_strokeDirty.x, segment.x), std::min(m_strokeDirty.y, segment.y),
            std::max(m_strokeDirty.x + m_strokeDirty.width, segment.x + segment.width)
                - std::min(m_strokeDirty.x, segment.x),
            std::max(m_strokeDirty.y + m_strokeDirty.height, segment.y + segment.height)
                - std::min(m_strokeDirty.y, segment.y) };
        const auto composited = m_compositor.composite(segment);
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

    if (m_erasing) {
        // The drag already built the erased layer incrementally, so commit
        // exactly that. Re-painting the whole stroke here used to shrink the
        // patch rect (pixels already at alpha 0 report "no change"), which left
        // part of the erase uncommitted — erased strokes came back. It also
        // subtracted alpha twice, over-erasing soft edges.
        m_erasing = false;
        m_compositor.clearLayerOverride();
        m_currentStroke.clear();
        m_preview.fill(core::raster::kTransparent);

        const auto rect = m_strokeDirty; // union of everything erased this drag
        m_strokeDirty = {};
        if (rect.isEmpty()) {
            rebuildFullComposite();
            return;
        }
        canvas::clipToSelection(m_eraseWork, m_selection, &m_eraseBefore);
        commitPatch(m_eraseBefore, m_eraseWork, rect, "Erase");
        return;
    }

    if (m_brush.erase) {
        // Erasing subtracts from the layer's own pixels. The preview scratch
        // starts transparent, so "erasing" into it is a no-op — the erase has
        // to run against a copy of the layer. Deciding this BEFORE painting is
        // what makes the eraser work.
        const core::raster::RasterBuffer original = activeLayer.pixels();
        core::raster::RasterBuffer work = original;
        const auto rect = canvas::BrushStroke::paint(work, m_brush, m_currentStroke);
        m_currentStroke.clear();
        m_preview.fill(core::raster::kTransparent);

        if (rect.isEmpty()) {
            rebuildFullComposite();
            return;
        }
        canvas::clipToSelection(work, m_selection, &original);

        auto before = original.copyRect(rect.x, rect.y, rect.width, rect.height);
        auto after = work.copyRect(rect.x, rect.y, rect.width, rect.height);
        m_history.execute(std::make_unique<core::StrokeCommand>(
            m_doc, static_cast<std::size_t>(m_activeLayer), std::move(before), std::move(after),
            rect.x, rect.y, "Erase"));
        emit historyChanged();
        rebuildFullComposite();
        return;
    }

    // Paint stroke: repaint the full stroke fresh into the scratch (the
    // accumulated preview is identical, but a clean repaint keeps the
    // before/after snapshots consistent).
    core::raster::RasterBuffer scratch(m_doc.canvasSize().width, m_doc.canvasSize().height);
    const auto rect = canvas::BrushStroke::paint(scratch, m_brush, m_currentStroke);
    m_currentStroke.clear();
    m_preview.fill(core::raster::kTransparent);

    if (rect.isEmpty()) {
        rebuildFullComposite();
        return;
    }

    // Clip to the active selection, if any.
    canvas::clipToSelection(scratch, m_selection);

    auto painted = scratch.copyRect(rect.x, rect.y, rect.width, rect.height);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(painted), rect.x, rect.y));

    emit historyChanged();
    rebuildFullComposite();
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
