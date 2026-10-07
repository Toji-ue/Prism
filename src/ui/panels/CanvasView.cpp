#include "panels/CanvasView.h"

#include "application/AppController.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

#include <algorithm>

CanvasView::CanvasView(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    m_timer.start();
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
    markDirty(0, 0, 0, 0); // width/height 0 -> everything
    emit controllerChanged();
    update();
}

QString CanvasView::renderStats() const
{
    const double mb = static_cast<double>(m_uploadedBytes) / (1024.0 * 1024.0);
    const double ms = static_cast<double>(m_uploadNs) / 1.0e6;
    return QStringLiteral("frames=%1 uploads=%2 bytes=%3 (%4 MB) uploadMs=%5 tiles=%6x%7")
        .arg(m_frames)
        .arg(m_uploads)
        .arg(m_uploadedBytes)
        .arg(mb, 0, 'f', 2)
        .arg(ms, 0, 'f', 1)
        .arg(m_cols)
        .arg(m_rows);
}

void CanvasView::resetRenderStats()
{
    m_frames = 0;
    m_uploads = 0;
    m_uploadedBytes = 0;
    m_uploadNs = 0;
    emit renderStatsChanged();
}

void CanvasView::onCanvasChanged(int x, int y, int w, int h)
{
    // Only the tiles under the dirty rect need a new texture; Qt coalesces the
    // repaints, so a burst of stroke ticks still costs one update per frame.
    markDirty(x, y, w, h);
    update();
}

void CanvasView::onViewChanged()
{
    // Zoom and pan only change where the tiles are drawn, which is recomputed
    // every frame anyway; a mirror flip also changes their contents.
    const bool mirrorH = m_controller && m_controller->mirrorHorizontal();
    const bool mirrorV = m_controller && m_controller->mirrorVertical();
    if (mirrorH != m_mirrorH || mirrorV != m_mirrorV)
        markDirty(0, 0, 0, 0); // 0 size -> all tiles
    update();
}

void CanvasView::rebuildTiles(int canvasWidth, int canvasHeight)
{
    const int cols = (canvasWidth + kTileSize - 1) / kTileSize;
    const int rows = (canvasHeight + kTileSize - 1) / kTileSize;
    if (cols == m_cols && rows == m_rows && !m_tileDirty.isEmpty())
        return;
    m_cols = cols;
    m_rows = rows;
    m_tileDirty = QVector<bool>(cols * rows, true);
}

void CanvasView::markDirty(int x, int y, int w, int h)
{
    if (m_cols <= 0 || m_rows <= 0 || m_tileDirty.isEmpty())
        return;
    if (w <= 0 || h <= 0) {
        m_tileDirty.fill(true); // whole canvas (document swapped, view mirrored)
        return;
    }
    const int c0 = std::max(0, x / kTileSize);
    const int c1 = std::min(m_cols - 1, (x + w - 1) / kTileSize);
    const int r0 = std::max(0, y / kTileSize);
    const int r1 = std::min(m_rows - 1, (y + h - 1) / kTileSize);
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c)
            m_tileDirty[static_cast<std::size_t>(r * m_cols + c)] = true;
}

QSGNode* CanvasView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* root = static_cast<QSGNode*>(oldNode);
    if (!root)
        root = new QSGNode;
    if (!m_controller || !window())
        return root;

    const int canvasW = m_controller->canvasWidth();
    const int canvasH = m_controller->canvasHeight();
    if (canvasW <= 0 || canvasH <= 0)
        return root;

    rebuildTiles(canvasW, canvasH);
    if (m_tileDirty.isEmpty())
        return root;

    ++m_frames;
    const bool mirrorH = m_controller->mirrorHorizontal();
    const bool mirrorV = m_controller->mirrorVertical();
    m_mirrorH = mirrorH;
    m_mirrorV = mirrorV;

    // Wrap the compositor's buffer without copying; each dirty tile is copied
    // out below, which both keeps the upload small and gives the render thread
    // a snapshot that the GUI thread cannot mutate underneath it.
    const QImage canvas = m_controller->canvasImage();
    if (canvas.isNull() || canvas.width() != canvasW || canvas.height() != canvasH)
        return root;

    const qreal sx = width() / canvasW;
    const qreal sy = height() / canvasH;

    const int tileCount = m_cols * m_rows;
    // A crop shrinks the canvas, so the grid has fewer tiles: the leftover nodes
    // kept their old textures AND their old rects, which painted the old content
    // outside the canvas — "crop then move" showed image spilling past the paper.
    while (root->childCount() > tileCount)
        delete root->childAtIndex(root->childCount() - 1);

    for (int i = 0; i < tileCount; ++i) {
        const int col = i % m_cols;
        const int row = i / m_cols;
        const int x = col * kTileSize;
        const int y = row * kTileSize;
        const int w = std::min(kTileSize, canvasW - x);
        const int h = std::min(kTileSize, canvasH - y);
        if (w <= 0 || h <= 0)
            continue;

        auto* node = static_cast<QSGSimpleTextureNode*>(root->childAtIndex(i));
        if (!node) {
            node = new QSGSimpleTextureNode;
            node->setOwnsTexture(true);
            node->setFiltering(QSGTexture::Nearest);
            root->appendChildNode(node);
        }

        if (m_tileDirty[static_cast<std::size_t>(i)]) {
            QImage tile = canvas.copy(x, y, w, h);
            if (mirrorH || mirrorV)
                tile = tile.mirrored(mirrorH, mirrorV);
            m_timer.restart();
            node->setTexture(window()->createTextureFromImage(tile));
            m_uploadNs += m_timer.nsecsElapsed();
            ++m_uploads;
            m_uploadedBytes += static_cast<qint64>(w) * h * 4;
            m_tileDirty[static_cast<std::size_t>(i)] = false;
        }

        // Where this tile lands in item coordinates. A mirrored axis draws the
        // tiles in reverse order, which is what keeps the flip GPU-side.
        const qreal ix = (mirrorH ? (canvasW - (x + w)) : x) * sx;
        const qreal iy = (mirrorV ? (canvasH - (y + h)) : y) * sy;
        node->setRect(QRectF(ix, iy, w * sx, h * sy));
    }

    return root;
}
