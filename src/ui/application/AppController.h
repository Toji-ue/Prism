#pragma once

#include "application/LayerListModel.h"
#include "brush/BrushPresets.h"
#include "brush/BrushStroke.h"
#include "compositor/Compositor.h"
#include "document/Document.h"
#include "history/History.h"
#include "raster/ColorMath.h"
#include "selection/Selection.h"

#include <QObject>
#include <QImage>
#include <QColor>
#include <QUrl>
#include <QStringList>
#include <QVariantList>

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
    Q_PROPERTY(int tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QString brushName READ brushName NOTIFY brushChanged)
    Q_PROPERTY(int activePresetIndex READ activePresetIndex NOTIFY brushChanged)
    Q_PROPERTY(QString statusInfo READ statusInfo NOTIFY canvasChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(int selectionX READ selectionX NOTIFY selectionChanged)
    Q_PROPERTY(int selectionY READ selectionY NOTIFY selectionChanged)
    Q_PROPERTY(int selectionW READ selectionW NOTIFY selectionChanged)
    Q_PROPERTY(int selectionH READ selectionH NOTIFY selectionChanged)
    /// Bumped on every composite change: image-provider URLs append it so
    /// QML re-fetches layer thumbnails instead of serving a cached image.
    Q_PROPERTY(int layerRevision READ layerRevision NOTIFY canvasChanged)

public:
    /// Canvas tools shown in the left toolbox. Only a subset is implemented
    /// today; the rest are presented but inert until their code lands.
    enum Tool {
        // --- paint ---
        BrushTool = 0,
        EraserTool,
        LineTool,
        RectangleTool,
        EllipseTool,
        GradientTool,
        FillTool,
        EyedropperTool,
        // --- view ---
        PanTool,
        ZoomTool,
        // --- selection ---
        RectSelectTool,
        EllipseSelectTool,
        LassoSelectTool,
        PolygonSelectTool,
        MagicWandTool,
        SimilarColorSelectTool,
        // --- edit ---
        MoveTool,
        // --- not implemented yet: shown disabled in the toolbox ---
        TextTool,
        PolylineTool,
        BezierTool,
        FreehandPathTool,
        ArcTool,
        MultiBrushTool,
        DynamicBrushTool,
        BezierSelectTool,
        MagneticSelectTool,
        OutlineSelectTool,
        ContiguousSelectTool,
        TransformTool,
        CropTool,
        PatternTool,
    };
    Q_ENUM(Tool)

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
    int tool() const;
    void setTool(int tool);
    QString brushName() const;
    int activePresetIndex() const;
    QString statusInfo() const;
    QString documentName() const;
    void setDocumentName(const QString& name);
    /// Window title in Krita's shape: "name (mem MiB) — Prism".
    QString windowTitle() const;

    Q_INVOKABLE QImage canvasImage() const;

    // --- brush presets ----------------------------------------------------
    Q_INVOKABLE int presetCount() const;
    Q_INVOKABLE QString presetName(int index) const;
    Q_INVOKABLE QImage presetThumbnail(int index, int width, int height) const;
    Q_INVOKABLE void selectPreset(int index);
    /// Saves the current brush settings as a new user preset.
    Q_INVOKABLE void addPreset();
    Q_INVOKABLE void removePreset(int index);
    Q_INVOKABLE void movePreset(int index, int delta);
    /// Raw thumbnail bytes for the image provider (image://preset/<i>).
    QImage presetThumbnailRaw(int index, int width, int height) const;

    // --- canvas tools -----------------------------------------------------
    /// Flood-fills the active layer at canvas coordinates (a history step),
    /// constrained to the active selection.
    Q_INVOKABLE void fillAt(float x, float y);
    /// Samples the composited color at canvas coordinates; returns it and
    /// sets it as the brush color when \p apply is true.
    Q_INVOKABLE QColor pickColorAt(float x, float y, bool apply);
    Q_INVOKABLE qreal documentMemoryMiB() const;
    int layerRevision() const { return m_layerRevision; }
    /// Multiplies the zoom (zoom tool click). \p out zooms out instead.
    Q_INVOKABLE void zoomStep(bool out);
    /// Zooms while keeping the canvas point under the cursor pinned, which is
    /// what makes the zoom tool feel anchored rather than jumping to centre.
    /// \p anchorX / \p anchorY are canvas coordinates.
    Q_INVOKABLE qreal zoomAt(qreal anchorX, qreal anchorY, qreal factor);
    /// Translates the active layer's content by (dx, dy) canvas pixels.
    Q_INVOKABLE void moveActiveLayer(int dx, int dy);

    // --- shape tools (drag to draw) ---------------------------------------
    /// Press: remembers the anchor and starts a shape preview.
    Q_INVOKABLE void beginShape(float x, float y);
    /// Drag: redraws the preview from the anchor to (x, y).
    Q_INVOKABLE void updateShape(float x, float y);
    /// Release: commits the shape through History.
    Q_INVOKABLE void endShape();
    /// When true, rectangle/ellipse paint a solid interior.
    Q_INVOKABLE void setShapeFilled(bool filled);
    Q_INVOKABLE bool shapeFilled() const { return m_shapeFilled; }

    /// Fills the New Document dialog's size fields from the clipboard image
    /// (Krita's "Create from Clipboard").
    Q_INVOKABLE QSize clipboardImageSize() const;

    /// Similar-colour selection anywhere on the canvas (not region-limited).
    Q_INVOKABLE void similarColorAt(float x, float y, int tolerance);

    // --- selection --------------------------------------------------------
    Q_INVOKABLE void setSelection(float x, float y, float w, float h);
    /// Elliptical marquee (same drag as the rectangular one).
    Q_INVOKABLE void setEllipseSelection(float x, float y, float w, float h);
    /// Freehand lasso: \p points is a list of QPointF in canvas coordinates.
    Q_INVOKABLE void setLassoSelection(const QVariantList& points);
    /// Magic wand: selects the connected similar-color region under (x, y).
    Q_INVOKABLE void magicWandAt(float x, float y, int tolerance);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void invertSelection();
    /// Selects the active layer's opaque pixels (Krita's "Select Opaque").
    Q_INVOKABLE void selectLayerContent();
    bool hasSelection() const;
    int selectionX() const;
    int selectionY() const;
    int selectionW() const;
    int selectionH() const;
    int selectionRevision() const;
    /// Selection rendered as a tinted overlay for the canvas (image://selection).
    QImage selectionOverlay() const;

    // --- view -------------------------------------------------------------
    Q_PROPERTY(bool mirrorHorizontal READ mirrorHorizontal WRITE setMirrorHorizontal
                   NOTIFY viewChanged)
    Q_PROPERTY(bool mirrorVertical READ mirrorVertical WRITE setMirrorVertical
                   NOTIFY viewChanged)
    bool mirrorHorizontal() const { return m_mirrorH; }
    void setMirrorHorizontal(bool on);
    bool mirrorVertical() const { return m_mirrorV; }
    void setMirrorVertical(bool on);

    // --- start page / recent files ---------------------------------------
    Q_PROPERTY(bool startPageVisible READ startPageVisible WRITE setStartPageVisible
                   NOTIFY startPageChanged)
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    bool startPageVisible() const { return m_startPageVisible; }
    void setStartPageVisible(bool visible);
    QVariantList recentFiles() const;
    Q_INVOKABLE void clearRecentFiles();
    /// Version string shown on the start page.
    Q_INVOKABLE QString versionString() const;

    // --- layer state ------------------------------------------------------
    Q_INVOKABLE void setLayerLocked(int index, bool locked);
    /// Live opacity during a slider drag — no history entry, just repaint.
    Q_INVOKABLE void previewLayerOpacity(int index, int percent);
    /// Commits the opacity change as a single history step, comparing against
    /// \p previousPercent (the value the drag started from).
    Q_INVOKABLE void commitLayerOpacity(int index, int percent, int previousPercent);
    /// Downscaled preview of a layer's own pixels (image://layerthumb/<i>).
    QImage layerThumbnail(int index, int width, int height) const;

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
    /// Merges the active layer into the one below it.
    Q_INVOKABLE void mergeLayerDown();
    /// True when the active layer has a layer below it.
    Q_INVOKABLE bool canMergeDown() const;
    /// Filter menu entry point. \p id is one of: invert, desaturate, blur,
    /// brightness-contrast. Applies to the active layer, respecting the
    /// selection and the layer lock; always one undo step.
    Q_INVOKABLE void applyFilter(const QString& id);

    // --- file -------------------------------------------------------------
    Q_INVOKABLE bool saveProject(const QUrl& url);
    Q_INVOKABLE bool openProject(const QUrl& url);
    Q_INVOKABLE bool importImage(const QUrl& url);
    Q_INVOKABLE bool exportPng(const QUrl& url);
    Q_INVOKABLE void newDocument(int width, int height);
    /// Full document creation, mirroring Krita's dialog: name, layer count,
    /// and how the background color is applied.
    enum BackgroundMode {
        BackgroundAsRasterLayer = 0,
        BackgroundAsCanvasColor,
        BackgroundAsFillLayer,
    };
    Q_ENUM(BackgroundMode)
    Q_INVOKABLE void createDocument(int width, int height, const QString& name, int layerCount,
                                    const QColor& background, int backgroundOpacity,
                                    int backgroundMode);
    Q_PROPERTY(QString documentName READ documentName WRITE setDocumentName
                   NOTIFY documentNameChanged)
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY documentNameChanged)
    /// Picks the zoom that fits the whole canvas into a viewport of the
    /// given size (never above 100%). Called on first layout by the UI.
    Q_INVOKABLE void fitToViewport(qreal viewportWidth, qreal viewportHeight);

    // --- stroking ---------------------------------------------------------
    Q_INVOKABLE void beginStroke(float x, float y);
    Q_INVOKABLE void extendStroke(float x, float y);
    Q_INVOKABLE void endStroke();

signals:
    void layersChanged();
    void historyChanged();
    void brushChanged();
    void viewChanged();
    void toolChanged();
    void selectionChanged();
    void presetsChanged();
    void documentNameChanged();
    void startPageChanged();
    void recentFilesChanged();
    void canvasChanged(int x, int y, int w, int h);

private:
    void rebuildFullComposite();
    void rebuildStrokePreview();
    void addRecentFile(const QString& path);
    void loadRecentFiles();

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
    int m_tool = BrushTool;
    int m_activePreset = 0;
    /// User presets plus the built-ins; mutable so the docker can add/remove.
    std::vector<prism::canvas::BrushPreset> m_presets;
    int m_customPresetCounter = 0;
    int m_layerRevision = 0;
    /// Active selection in canvas pixels; empty means "no selection".
    /// Active selection. Mask-based so lasso and magic wand work too.
    prism::canvas::Selection m_selection;
    /// Shape-tool drag state (line / rectangle / ellipse / gradient).
    prism::core::geometry::Point m_shapeStart;
    prism::core::geometry::Point m_shapeEnd;
    bool m_shapeActive = false;
    bool m_shapeFilled = false;
    bool m_mirrorH = false;
    bool m_mirrorV = false;
    bool m_startPageVisible = true;
    QStringList m_recentFiles;
    QString m_documentName = tr("Untitled");
    /// True until the user moves a brush slider — controls the "Custom" label.
    bool m_presetIntact = true;
};
