#include "application/AppController.h"

#include "application/LayerListModel.h"
#include "commands/Commands.h"
#include "fill/FloodFill.h"
#include "layer/Layer.h"
#include "project/ProjectIO.h"
#include "selection/Selection.h"

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

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_layersModel(new LayerListModel(this))
    , m_presets(canvas::builtinBrushPresets())
{
    m_doc.addRasterLayer("Layer 1");
    m_layersModel->setDocument(&m_doc);
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
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
    return tr("RGB/Alpha (8-bit integer/channel) · %1 · %2 × %3 px · %4 MiB")
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
    // Always keep at least one preset — an empty brush list is unusable.
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

    // Box-downscale: average each destination pixel's source block so the
    // thumbnail reads like the layer rather than a nearest-neighbour sample.
    core::raster::RasterBuffer out(width, height);
    const float sx = static_cast<float>(src.width()) / static_cast<float>(width);
    const float sy = static_cast<float>(src.height()) / static_cast<float>(height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int x0 = static_cast<int>(x * sx);
            const int y0 = static_cast<int>(y * sy);
            const int x1 = std::min(src.width(), std::max(x0 + 1, static_cast<int>((x + 1) * sx)));
            const int y1 = std::min(src.height(), std::max(y0 + 1, static_cast<int>((y + 1) * sy)));
            int r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int yy = y0; yy < y1; ++yy) {
                for (int xx = x0; xx < x1; ++xx) {
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

    // Sample the flattened canvas — what the artist sees is what they pick.
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
int AppController::selectionX() const { return m_selection.x; }
int AppController::selectionY() const { return m_selection.y; }
int AppController::selectionW() const { return m_selection.width; }
int AppController::selectionH() const { return m_selection.height; }

void AppController::setSelection(float x, float y, float w, float h)
{
    const auto s = m_doc.canvasSize();
    const int x0 = std::clamp(static_cast<int>(std::floor(std::min(x, x + w))), 0, s.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(std::min(y, y + h))), 0, s.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(std::max(x, x + w))), 0, s.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(std::max(y, y + h))), 0, s.height);
    m_selection = core::geometry::Rect{ x0, y0, x1 - x0, y1 - y0 };
    emit selectionChanged();
}

void AppController::clearSelection()
{
    m_selection = {};
    emit selectionChanged();
}

void AppController::selectAll()
{
    m_selection = fullRect(m_doc);
    emit selectionChanged();
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
    // Live feedback only — the history entry is created once, on release.
    m_doc.layers()[static_cast<std::size_t>(index)]->setOpacity(
        static_cast<float>(percent) / 100.0F);
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
    return io::ProjectIO::save(m_doc, url.toLocalFile());
}

bool AppController::openProject(const QUrl& url)
{
    auto doc = io::ProjectIO::load(url.toLocalFile());
    if (!doc)
        return false;
    m_doc = std::move(*doc);
    m_history.clear();
    m_compositor.canvasResized();
    m_activeLayer = 0;
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_layersModel->setDocument(&m_doc);
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
    return true;
}

bool AppController::importImage(const QUrl& url)
{
    auto doc = io::ProjectIO::importImage(url.toLocalFile());
    if (!doc)
        return false;
    m_doc = std::move(*doc);
    m_history.clear();
    m_compositor.canvasResized();
    m_activeLayer = 0;
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
    m_layersModel->setDocument(&m_doc);
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
    return true;
}

bool AppController::exportPng(const QUrl& url)
{
    return io::ProjectIO::exportPng(m_compositor, url.toLocalFile());
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
    m_selection = {};
    m_preview = core::raster::RasterBuffer(w, h);
    m_layersModel->setDocument(&m_doc);
    emit layersChanged();
    emit historyChanged();
    emit documentNameChanged();
    emit selectionChanged();
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
    return tr("%1 (%2 MiB) — Prism").arg(m_documentName).arg(documentMemoryMiB(), 0, 'f', 1);
}

// ------------------------------------------------------------------- stroking

void AppController::beginStroke(float x, float y)
{
    m_currentStroke.clear();
    m_currentStroke.push_back({ x, y, 1.0F });
    m_preview.fill(core::raster::kTransparent);
    m_strokeDirty = core::geometry::Rect{ static_cast<int>(x), static_cast<int>(y), 0, 0 };
}

void AppController::extendStroke(float x, float y)
{
    if (m_currentStroke.empty())
        return;
    const auto prev = m_currentStroke.back();
    m_currentStroke.push_back({ x, y, 1.0F });
    const auto segment = canvas::BrushStroke::paintSegment(m_preview, m_brush, prev,
                                                           { x, y, 1.0F });
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
    const auto composited = m_compositor.composite(segment, &m_preview);
    ++m_layerRevision;
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

void AppController::endStroke()
{
    if (m_currentStroke.empty())
        return;
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;

    auto& activeLayer = *m_doc.layers()[static_cast<std::size_t>(m_activeLayer)];

    // Re-paint the full stroke fresh into the scratch (the accumulated
    // preview is identical, but a clean repaint keeps snapshots consistent).
    // Note: the point list must survive until after the paint call.
    core::raster::RasterBuffer scratch(m_doc.canvasSize().width, m_doc.canvasSize().height);
    const auto rect = canvas::BrushStroke::paint(scratch, m_brush, m_currentStroke);
    m_currentStroke.clear();
    m_preview.fill(core::raster::kTransparent);

    if (activeLayer.isLocked() || rect.isEmpty()) {
        // Locked layers swallow the edit rather than silently ignoring lock.
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
