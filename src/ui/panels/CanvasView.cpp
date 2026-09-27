#include "panels/CanvasView.h"

#include "application/AppController.h"

#include <QPainter>
#include <QRectF>

CanvasView::CanvasView(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    // Transparent: the canvas backdrop/paper rectangle sits behind this item
    // in QML, and an opaque fill here would hide it.
    setFillColor(Qt::transparent);
}

void CanvasView::setController(AppController* controller)
{
    if (m_controller == controller)
        return;
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &AppController::canvasChanged, this,
                &CanvasView::onCanvasChanged);
        connect(m_controller, &AppController::viewChanged, this, &CanvasView::onViewChanged);
    }
    emit controllerChanged();
    update();
}

void CanvasView::paint(QPainter* painter)
{
    if (!m_controller)
        return;
    const QImage canvas = m_controller->canvasImage();
    if (canvas.isNull())
        return;

    // Scale the canvas pixels to the item's zoomed size. Drawing the image
    // 1:1 (the previous behaviour) showed only a crop of the canvas whenever
    // zoom != 100%, which made every stroke land in the wrong place.
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(QRectF(0.0, 0.0, width(), height()), canvas);
}

void CanvasView::onCanvasChanged(int x, int y, int w, int h)
{
    if (!m_controller || w <= 0 || h <= 0) {
        update();
        return;
    }
    // Partial repaint: Qt clips paint() to this rect, so a stroke tick
    // repaints only the band that actually changed instead of the whole
    // canvas (the "repaints everything" performance complaint).
    const qreal z = m_controller->zoom();
    const qreal margin = 1.0;
    update(QRectF(x * z - margin, y * z - margin, w * z + 2 * margin, h * z + 2 * margin)
               .toAlignedRect());
}

void CanvasView::onViewChanged()
{
    // Zoom or pan changed the mapping from canvas pixels to item pixels.
    update();
}
