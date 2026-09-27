#include "panels/CanvasView.h"

#include "application/AppController.h"

#include <QPainter>

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
