#pragma once

#include "brush/BrushStroke.h"
#include "compositor/Compositor.h"
#include "document/Document.h"
#include "history/History.h"

#include <QObject>
#include <QImage>
#include <QSize>
#include <QColor>
#include <QUrl>

#include <vector>

/// Bridges the QML shell to the core document/history and the canvas
/// compositor. Owns the Document; all mutations flow through History.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int layerCount READ layerCount NOTIFY layersChanged)
    Q_PROPERTY(int activeLayerIndex READ activeLayerIndex WRITE setActiveLayerIndex
                   NOTIFY layersChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY historyChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth CONSTANT)
    Q_PROPERTY(int canvasHeight READ canvasHeight CONSTANT)
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushChanged)
    Q_PROPERTY(float brushSize READ brushSize WRITE setBrushSize NOTIFY brushChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    // --- properties -------------------------------------------------------
    int layerCount() const;
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

    /// Flattened canvas as a QImage the view can paint. ARGB32_Premultiplied
    /// for direct QPainter use.
    Q_INVOKABLE QImage canvasImage() const;

    // --- actions ----------------------------------------------------------
    Q_INVOKABLE void addLayer();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool exportPng(const QUrl& fileUrl);

    // --- stroking ---------------------------------------------------------
    /// Mouse stroke input; pressure fixed at 1 until tablet events wired.
    Q_INVOKABLE void beginStroke(float x, float y);
    Q_INVOKABLE void extendStroke(float x, float y);
    /// Commits the stroke through History (one undo step per stroke).
    Q_INVOKABLE void endStroke();

    Q_INVOKABLE QString layerName(int index) const;

signals:
    void layersChanged();
    void historyChanged();
    void brushChanged();
    /// Emitted whenever the flattened canvas pixels changed, with the dirty
    /// rect in canvas coordinates.
    void canvasChanged(int x, int y, int w, int h);

private:
    void rebuildFullComposite();

    prism::core::Document m_doc{ prism::core::CanvasSize{ 1280, 800, 72 } };
    prism::core::History m_history;
    prism::canvas::Compositor m_compositor{ m_doc };
    prism::canvas::BrushSettings m_brush;
    std::vector<prism::canvas::StrokePoint> m_currentStroke;
    int m_activeLayer = 0;
};
