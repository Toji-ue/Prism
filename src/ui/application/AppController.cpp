#include "application/AppController.h"

#include "application/LayerListModel.h"
#include "commands/Commands.h"
#include "layer/Layer.h"
#include "project/ProjectIO.h"

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
{
    m_doc.addRasterLayer("Layer 1");
    m_layersModel->setDocument(&m_doc);
    m_preview = core::raster::RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
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
    emit brushChanged();
}

float AppController::brushHardness() const { return m_brush.hardness; }

void AppController::setBrushHardness(float hardness)
{
    m_brush.hardness = std::clamp(hardness, 0.0F, 1.0F);
    emit brushChanged();
}

float AppController::brushFlow() const { return m_brush.flow; }

void AppController::setBrushFlow(float flow)
{
    m_brush.flow = std::clamp(flow, 0.01F, 1.0F);
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
    m_doc = core::Document(core::CanvasSize{ width, height, 72 });
    m_doc.addRasterLayer("Layer 1");
    m_history.clear();
    m_compositor.canvasResized();
    m_activeLayer = 0;
    m_preview = core::raster::RasterBuffer(width, height);
    m_layersModel->setDocument(&m_doc);
    emit layersChanged();
    emit historyChanged();
    rebuildFullComposite();
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
    const auto r = canvas::BrushStroke::paintSegment(m_preview, m_brush, prev,
                                                     { x, y, 1.0F });
    if (!r.isEmpty()) {
        m_strokeDirty = core::geometry::Rect{
            std::min(m_strokeDirty.x, r.x), std::min(m_strokeDirty.y, r.y),
            std::max(m_strokeDirty.x + m_strokeDirty.width, r.x + r.width)
                - std::min(m_strokeDirty.x, r.x),
            std::max(m_strokeDirty.y + m_strokeDirty.height, r.y + r.height)
                - std::min(m_strokeDirty.y, r.y) };
    }
    rebuildStrokePreview();
}

void AppController::endStroke()
{
    if (m_currentStroke.empty())
        return;
    if (m_activeLayer >= static_cast<int>(m_doc.layers().size()))
        return;

    // Re-paint the full stroke fresh into the scratch (accumulated preview is
    // exact, but a clean repaint keeps before/after snapshots consistent).
    core::raster::RasterBuffer scratch(m_doc.canvasSize().width, m_doc.canvasSize().height);
    const auto rect = canvas::BrushStroke::paint(scratch, m_brush, m_currentStroke);
    m_currentStroke.clear();
    m_preview.fill(core::raster::kTransparent);

    if (rect.isEmpty()) {
        rebuildFullComposite();
        return;
    }

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
    emit canvasChanged(rect.x, rect.y, rect.width, rect.height);
}

void AppController::rebuildStrokePreview()
{
    const auto rect = m_compositor.composite(m_strokeDirty, &m_preview);
    emit canvasChanged(rect.x, rect.y, rect.width, rect.height);
}
