#pragma once

#include <QQuickItem>

class AppController;
class QSGTexture;
class QSGSimpleTextureNode;

/// Displays the compositor's flattened canvas.
///
/// This is a scene-graph item, not a QQuickPaintedItem: the canvas is uploaded
/// once as a GPU texture and drawn by the renderer with nearest-neighbour
/// filtering. QQuickPaintedItem re-uploaded the whole item image and re-scaled
/// it on the CPU on every paint, which was the remaining source of drawing lag.
class CanvasView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(AppController* controller READ controller WRITE setController
                   NOTIFY controllerChanged)

public:
    explicit CanvasView(QQuickItem* parent = nullptr);

    AppController* controller() const { return m_controller; }
    void setController(AppController* controller);

signals:
    void controllerChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private slots:
    void onCanvasChanged(int x, int y, int w, int h);
    void onViewChanged();

private:
    AppController* m_controller = nullptr;
    /// Set whenever the picture changed; the texture is rebuilt on the next
    /// render pass, on the render thread.
    bool m_dirty = true;
    QSGTexture* m_texture = nullptr;
};
