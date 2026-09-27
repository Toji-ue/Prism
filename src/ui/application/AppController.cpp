#include "application/AppController.h"

#include "commands/Commands.h"
#include "layer/Layer.h"

#include <QFile>
#include <QSaveFile>

#include <algorithm>

using namespace prism;

AppController::AppController(QObject* parent)
    : QObject(parent)
{
    // Start with one paintable layer, like every drawing app does.
    m_doc.addRasterLayer("Layer 1");
}

// ---------------------------------------------------------------- properties

int AppController::layerCount() const { return static_cast<int>(m_doc.layers().size()); }

int AppController::activeLayerIndex() const
{
    return std::min(m_activeLayer, layerCount() - 1);
}

void AppController::setActiveLayerIndex(int index)
{
    if (index >= 0 && index < layerCount()) {
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

QColor AppController::brushColor() const
{
    const auto& c = m_brush.color;
    return QColor(c.r, c.g, c.b, c.a);
}

void AppController::setBrushColor(const QColor& color)
{
    m_brush.color = core::raster::Color{ static_cast<std::uint8_t>(color.red()),
                                         static_cast<std::uint8_t>(color.green()),
                                         static_cast<std::uint8_t>(color.blue()), 255 };
    emit brushChanged();
}

float AppController::brushSize() const { return m_brush.radius; }

void AppController::setBrushSize(float size)
{
    m_brush.radius = std::clamp(size, 1.0F, 200.0F);
    emit brushChanged();
}

// ------------------------------------------------------------------ actions

void AppController::addLayer()
{
    const auto name = QStringLiteral("Layer %1").arg(layerCount() + 1);
    m_history.execute(std::make_unique<core::AddLayerCommand>(m_doc, name.toStdString()));
    m_activeLayer = layerCount() - 1;
    emit layersChanged();
    emit historyChanged();
}

void AppController::undo()
{
    m_history.undo();
    rebuildFullComposite();
    emit historyChanged();
}

void AppController::redo()
{
    m_history.redo();
    rebuildFullComposite();
    emit historyChanged();
}

bool AppController::exportPng(const QUrl& fileUrl)
{
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty())
        return false;

    // ARGB32... actually RGBA8888 matches our byte layout directly; PNG
    // keeps alpha. Raster rows are top-down, same as QImage — no flip.
    const auto& result = m_compositor.result();
    QImage image(result.data(), result.width(), result.height(),
                 result.width() * 4, QImage::Format_RGBA8888);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (!image.copy().save(&file, "PNG"))
        return false; // copy() detaches from the compositor's buffer
    return file.commit();
}

// ----------------------------------------------------------------- stroking

void AppController::beginStroke(float x, float y)
{
    m_currentStroke.clear();
    m_currentStroke.push_back({ x, y, 1.0F });
}

void AppController::extendStroke(float x, float y)
{
    if (!m_currentStroke.empty())
        m_currentStroke.push_back({ x, y, 1.0F });
}

void AppController::endStroke()
{
    if (m_currentStroke.empty() || m_activeLayer >= layerCount())
        return;

    // Paint into a transparent scratch buffer so the live layer only sees
    // the committed stroke, and History can snapshot before/after.
    core::raster::RasterBuffer scratch(m_doc.canvasSize().width, m_doc.canvasSize().height);
    const auto rect = canvas::BrushStroke::paint(scratch, m_brush, m_currentStroke);
    m_currentStroke.clear();

    if (rect.isEmpty())
        return;

    auto painted = scratch.copyRect(rect.x, rect.y, rect.width, rect.height);
    m_history.execute(std::make_unique<core::StrokeCommand>(
        m_doc, static_cast<std::size_t>(m_activeLayer), std::move(painted), rect.x, rect.y));

    const auto composited = m_compositor.composite(rect);
    emit historyChanged();
    emit canvasChanged(composited.x, composited.y, composited.width, composited.height);
}

QString AppController::layerName(int index) const
{
    if (index < 0 || index >= layerCount())
        return {};
    return QString::fromStdString(m_doc.layers()[static_cast<std::size_t>(index)]->name());
}

QImage AppController::canvasImage() const
{
    const auto& result = m_compositor.result();
    return QImage(result.data(), result.width(), result.height(), result.width() * 4,
                  QImage::Format_RGBA8888);
}

void AppController::rebuildFullComposite()
{
    const auto size = m_doc.canvasSize();
    const auto rect = m_compositor.composite(core::geometry::Rect{ 0, 0, size.width, size.height });
    emit canvasChanged(rect.x, rect.y, rect.width, rect.height);
}
