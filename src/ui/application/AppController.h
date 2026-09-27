#pragma once

#include "application/LayerListModel.h"
#include "brush/BrushStroke.h"
#include "compositor/Compositor.h"
#include "document/Document.h"
#include "history/History.h"
#include "raster/ColorMath.h"

#include <QObject>
#include <QImage>
#include <QColor>
#include <QUrl>

#include <vector>

/// Bridges QML to the core document/history and canvas compositor.
/// Owns the Document; every mutation flows through History.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(LayerListModel* layers READ layers CONSTANT)
    Q_PROPERTY(int activeLayerIndex READ activeLayerIndex WRITE setActiveLayerIndex
                   NOTIFY layersChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY historyChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth NOTIFY canvasChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight NOTIFY canvasChanged)
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushChanged)
    Q_PROPERTY(float brushSize READ brushSize WRITE setBrushSize NOTIFY brushChanged)
    Q_PROPERTY(float brushHardness READ brushHardness WRITE setBrushHardness NOTIFY brushChanged)
    Q_PROPERTY(float brushFlow READ brushFlow WRITE setBrushFlow NOTIFY brushChanged)
    Q_PROPERTY(bool eraser READ eraser WRITE setEraser NOTIFY brushChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    // --- model ------------------------------------------------------------
    LayerListModel* layers() const { return m_layersModel; }

    // --- properties -------------------------------------------------------
    int activeLayerIndex() const;
    void setActiveLayerIndex(int index);
    bool canUndo() const;
    bool canRedo() const;
    QString undoLabel() const;
    int canvasWidth() const;
    int canvasHeight() const;
    QColor brushColor() const;
    void setBrushColor(const QColor& color);
    float brushSize() const;
    void setBrushSize(float size);
    float brushHardness() const;
    void setBrushHardness(float hardness);
    float brushFlow() const;
    void setBrushFlow(float flow);
    bool eraser() const;
    void setEraser(bool erase);
    qreal zoom() const;
    void setZoom(qreal zoom);

    Q_INVOKABLE QImage canvasImage() const;

    // --- actions ----------------------------------------------------------
    Q_INVOKABLE void addLayer();
    Q_INVOKABLE void deleteLayer();
    Q_INVOKABLE void duplicateLayer();
    Q_INVOKABLE void moveLayerUp();
    Q_INVOKABLE void moveLayerDown();
    Q_INVOKABLE void setLayerVisible(int index, bool visible);
    Q_INVOKABLE void setLayerOpacity(int index, int percent);
    Q_INVOKABLE void setLayerBlendMode(int index, int mode);
    Q_INVOKABLE void renameLayer(int index, const QString& name);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clearCanvas();

    // --- file -------------------------------------------------------------
    Q_INVOKABLE bool saveProject(const QUrl& url);
    Q_INVOKABLE bool openProject(const QUrl& url);
    Q_INVOKABLE bool importImage(const QUrl& url);
    Q_INVOKABLE bool exportPng(const QUrl& url);
    Q_INVOKABLE void newDocument(int width, int height);

    // --- stroking ---------------------------------------------------------
    Q_INVOKABLE void beginStroke(float x, float y);
    Q_INVOKABLE void extendStroke(float x, float y);
    Q_INVOKABLE void endStroke();

signals:
    void layersChanged();
    void historyChanged();
    void brushChanged();
    void viewChanged();
    void canvasChanged(int x, int y, int w, int h);

private:
    void rebuildFullComposite();
    void rebuildStrokePreview();

    prism::core::Document m_doc{ prism::core::CanvasSize{ 1280, 800, 72 } };
    prism::core::History m_history;
    prism::canvas::Compositor m_compositor{ m_doc };
    prism::canvas::BrushSettings m_brush;
    std::vector<prism::canvas::StrokePoint> m_currentStroke;
    prism::core::raster::RasterBuffer m_preview;
    prism::core::geometry::Rect m_strokeDirty;
    LayerListModel* m_layersModel = nullptr;
    int m_activeLayer = 0;
    qreal m_zoom = 1.0;
};
