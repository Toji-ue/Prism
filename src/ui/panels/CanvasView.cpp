#include "panels/CanvasView.h"

#include "application/AppController.h"

#include <QPainter>

CanvasView::CanvasView(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    // Checkerboard-ish dark backdrop behind the (possibly translucent) canvas.
    setFillColor(QColor(30, 30, 34));
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
    }
    emit controllerChanged();
    update();
}

void CanvasView::paint(QPainter* painter)
{
    if (!m_controller)
        return;
    painter->drawImage(QPoint(0, 0), m_controller->canvasImage());
}

void CanvasView::onCanvasChanged(int, int, int w, int h)
{
    // QQuickPaintedItem cannot partial-update; request a full item update.
    // The dirty rect is kept in the signal for the future RHI renderer.
    Q_UNUSED(w);
    Q_UNUSED(h);
    update();
}
