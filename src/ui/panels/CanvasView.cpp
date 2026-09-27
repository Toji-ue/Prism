#include "panels/CanvasView.h"

#include "application/AppController.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

CanvasView::CanvasView(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
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
    m_dirty = true;
    emit controllerChanged();
    update();
}

void CanvasView::onCanvasChanged(int, int, int, int)
{
    // The texture is rebuilt in updatePaintNode; Qt coalesces the updates, so
    // a burst of stroke ticks costs one upload per rendered frame.
    m_dirty = true;
    update();
}

void CanvasView::onViewChanged()
{
    // Zoom or mirror changed how the canvas maps onto the item.
    m_dirty = true;
    update();
}

QSGNode* CanvasView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        // Pixel-exact: no smoothing, the canvas is shown as stored.
        node->setFiltering(QSGTexture::Nearest);
        // A new node needs its texture (re)built below.
        m_dirty = true;
    }

    if (m_dirty && m_controller) {
        const QImage canvas = m_controller->canvasImage();
        if (!canvas.isNull() && window()) {
            QImage upload = canvas;
            if (m_controller->mirrorHorizontal() || m_controller->mirrorVertical())
                upload = upload.mirrored(m_controller->mirrorHorizontal(),
                                         m_controller->mirrorVertical());
            // Owned by the node, so replacing it releases the previous texture.
            node->setTexture(window()->createTextureFromImage(upload));
        }
        m_dirty = false;
    }

    node->setRect(boundingRect());
    return node;
}
