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
    QImage canvas = m_controller->canvasImage();
    if (canvas.isNull() || width() <= 0 || height() <= 0)
        return;

    // View mirroring: the artist asked to see the canvas flipped. Input
    // mapping in QML mirrors the same way, so strokes stay under the cursor.
    if (m_controller->mirrorHorizontal() || m_controller->mirrorVertical())
        canvas = canvas.mirrored(m_controller->mirrorHorizontal(),
                                 m_controller->mirrorVertical());

    // Scale the canvas pixels to the item's zoomed size. Drawing the image
    // 1:1 (an earlier bug) showed only a crop of the canvas whenever
    // zoom != 100%, which made every stroke land in the wrong place.
    //
    // Only the region Qt actually asked to repaint is rasterized: partial
    // updates pass a clip, and scaling the whole 1M-pixel canvas per stroke
    // tick was a large part of the remaining input lag.
    QRect target = painter->clipBoundingRect().toAlignedRect()
                       .intersected(QRect(0, 0, static_cast<int>(width()),
                                          static_cast<int>(height())));
    if (target.isEmpty())
        target = QRect(0, 0, static_cast<int>(width()), static_cast<int>(height()));

    const qreal sx = static_cast<qreal>(canvas.width()) / width();
    const qreal sy = static_cast<qreal>(canvas.height()) / height();
    QRect source(static_cast<int>(target.x() * sx), static_cast<int>(target.y() * sy),
                 static_cast<int>(target.width() * sx) + 1,
                 static_cast<int>(target.height() * sy) + 1);
    source = source.intersected(QRect(0, 0, canvas.width(), canvas.height()));
    if (source.isEmpty())
        return;

    // Pixel-exact rendering: nearest-neighbour at every zoom so what is on the
    // canvas is exactly what is stored. Smoothing would blur single pixels,
    // which is wrong for a pixel-oriented drawing app.
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(target, canvas, source);
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
