#pragma once

#include <QElapsedTimer>
#include <QQuickItem>
#include <QVector>

class AppController;
class QSGTexture;
class QSGSimpleTextureNode;

/// Displays the compositor's flattened canvas as a grid of GPU textures.
///
/// Tiling is the point: uploading the whole canvas on every frame cost a full
/// RGBA upload (~4 MB at 1280x800) plus a texture create/destroy pair per
/// frame, which is what made drawing feel laggy. Only the tiles overlapped by
/// the dirty rectangle are re-uploaded, so a brush tick uploads one 256x256
/// tile instead of the whole canvas — the same approach Krita's canvas takes.
class CanvasView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(AppController* controller READ controller WRITE setController
                   NOTIFY controllerChanged)
    /// Human-readable upload statistics, for the --demo-stroke-bench hook.
    Q_PROPERTY(QString renderStats READ renderStats NOTIFY renderStatsChanged)

public:
    explicit CanvasView(QQuickItem* parent = nullptr);

    AppController* controller() const { return m_controller; }
    void setController(AppController* controller);

    QString renderStats() const;
    /// Clears the counters so a benchmark measures only its own window.
    Q_INVOKABLE void resetRenderStats();

signals:
    void controllerChanged();
    void renderStatsChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private slots:
    void onCanvasChanged(int x, int y, int w, int h);
    void onViewChanged();

private:
    /// 256 px keeps a tile upload at 256 KiB; sized so a 4000x4000 canvas is 16
    /// tiles across rather than hundreds of tiny draw calls.
    static constexpr int kTileSize = 256;

    /// Rebuilds the tile grid when the canvas size changes.
    void rebuildTiles(int canvasWidth, int canvasHeight);
    /// Marks every tile overlapping \p rect (image coordinates) for upload.
    void markDirty(int x, int y, int w, int h);

    AppController* m_controller = nullptr;

    /// One entry per tile: textures are owned by their scene-graph nodes.
    QVector<bool> m_tileDirty;
    int m_cols = 0;
    int m_rows = 0;
    bool m_mirrorH = false;
    bool m_mirrorV = false;

    // Upload statistics (reset by resetRenderStats).
    int m_frames = 0;
    int m_uploads = 0;
    qint64 m_uploadedBytes = 0;
    qint64 m_uploadNs = 0;
    QElapsedTimer m_timer;
};
