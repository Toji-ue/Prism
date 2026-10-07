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
#include <QMutex>
#include <QTimer>
#include <QUrl>
#include <QStringList>
#include <QVariantList>
#include <QVector>

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
    Q_PROPERTY(bool brushTipSquare READ brushTipSquare WRITE setBrushTipSquare NOTIFY brushChanged)
    Q_PROPERTY(float brushTipRoundness READ brushTipRoundness WRITE setBrushTipRoundness NOTIFY brushChanged)
    Q_PROPERTY(float brushTipAngle READ brushTipAngle WRITE setBrushTipAngle NOTIFY brushChanged)
    Q_PROPERTY(bool pressureOpacity READ pressureOpacity WRITE setPressureOpacity NOTIFY brushChanged)
    Q_PROPERTY(bool tipIsImage READ tipIsImage NOTIFY brushChanged)
    /// Pressure curve gamma (0.3 soft .. 3.0 hard). Applied to incoming pen
    /// pressure before the engine sees it.
    Q_PROPERTY(qreal pressureCurve READ pressureCurve WRITE setPressureCurve NOTIFY brushChanged)
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
        // --- drawing-app additions (append only: never renumber) ---
        /// Clip Studio Paint's "Lasso fill": draw an enclosure, it fills itself.
        LassoFillTool,
        /// Clip Studio Paint's "Blend": smears the paint already on the layer.
        BlendTool,
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
    bool brushTipSquare() const;
    void setBrushTipSquare(bool square);
    float brushTipRoundness() const;
    void setBrushTipRoundness(float roundness);
    float brushTipAngle() const;
    void setBrushTipAngle(float angle);
    bool pressureOpacity() const;
    void setPressureOpacity(bool on);
    bool tipIsImage() const;
    /// Loads a grayscale PNG (max 1024x1024) as the brush tip.
    Q_INVOKABLE bool loadTipImage(const QUrl& url);
    Q_INVOKABLE void clearTipImage();
    qreal pressureCurve() const;
    void setPressureCurve(qreal gamma);
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
    /// Import/Export brush presets as .prismbrush JSON files.
    Q_INVOKABLE bool exportBrush(const QUrl& url, int index);
    Q_INVOKABLE bool importBrush(const QUrl& url);
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
    /// Bounding box of a buffer's non-transparent pixels (empty when blank).
    static prism::core::geometry::Rect contentBounds(
        const prism::core::raster::RasterBuffer& buffer);
    /// Drag the active layer with the Move tool: press, per-move preview (so
    /// the image follows the pointer instead of jumping on release), release.
    Q_INVOKABLE void beginLayerMove();
    Q_INVOKABLE void previewLayerMove(int dx, int dy);
    Q_INVOKABLE void endLayerMove();

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
    /// Outline Selection: selects the connected opaque run under (x, y).
    Q_INVOKABLE void selectOutlineAt(float x, float y);

    // --- extra tools ------------------------------------------------------
    /// Polyline / Bezier / Bezier-selection: click to add a point.
    Q_INVOKABLE void addPathPoint(float x, float y);
    /// Finishes the in-progress polyline / bezier / selection path. \p close
    /// closes it as a shape (\p mode: 0 = polyline stroke, 1 = bezier stroke,
    /// 2 = smooth selection, 3 = magnetic selection).
    Q_INVOKABLE void finishPath(int mode);
    Q_INVOKABLE void cancelPath();
    Q_INVOKABLE int pathPointCount() const;
    /// Freehand Path: press, drag, release — smooths the drag before drawing.
    Q_INVOKABLE void beginFreePath(float x, float y);
    Q_INVOKABLE void extendFreePath(float x, float y);
    Q_INVOKABLE void endFreePath();
    /// Lasso fill (Clip Studio Paint): drag an enclosure; on release the inside
    /// is filled with the brush colour. No selection is created.
    Q_INVOKABLE void beginLassoFill(float x, float y);
    Q_INVOKABLE void extendLassoFill(float x, float y);
    Q_INVOKABLE void endLassoFill();
    /// Arc: drag from one end of the arc to the other.
    Q_INVOKABLE void beginArc(float x, float y);
    Q_INVOKABLE void updateArc(float x, float y);
    Q_INVOKABLE void endArc();
    /// Pattern fill inside the selection (or the whole layer).
    Q_INVOKABLE void fillPattern(int kind, int scale);
    /// Crop the whole document to a dragged rectangle.
    Q_INVOKABLE void cropTo(float x, float y, float w, float h);
    /// Transform the active layer: rotation in degrees and uniform scale about
    /// the layer centre.
    Q_INVOKABLE void transformLayer(float degrees, float scale);
    /// Text tool: rasterizes \p text at (x, y) with the current colour.
    Q_INVOKABLE void renderText(float x, float y, const QString& text, int pixelSize);

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
    /// Colour picker finished: hand the canvas back to the drawing tool.
    Q_INVOKABLE void endEyedropper();
    /// Selects the active layer's opaque pixels (Krita's "Select Opaque").
    Q_INVOKABLE void selectLayerContent();
    bool hasSelection() const;
    int selectionX() const;
    int selectionY() const;
    int selectionW() const;
    int selectionH() const;
    int selectionRevision() const;
    /// Selection rendered as a tinted overlay for the canvas (image://selection).
    /// Also served from a GUI-thread snapshot, for the same reason as the layer
    /// thumbnails (and it used to re-scan the whole canvas per request).
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
    /// Last file error, shown in the status bar. Empty when all is well — a
    /// failed open must never look like nothing happened.
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    /// One-shot informational note for the status bar ("imported as a layer,
    /// centered, N px clipped"). Separate from lastError because it reports a
    /// success the user still needs to know about.
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    bool startPageVisible() const { return m_startPageVisible; }
    void setStartPageVisible(bool visible);
    QVariantList recentFiles() const;
    Q_INVOKABLE void clearRecentFiles();
    /// Drops recent entries whose path lives under \p directory — the selftest
    /// uses it to clean up test files earlier runs left in the list.
    void forgetRecentFilesUnder(const QString& directory);
    /// Selftest mode: drive the real code paths, but keep throwaway documents
    /// out of the persisted Recent list.
    void setSelfTestMode(bool on) { m_selfTestMode = on; }
    /// Version string shown on the start page.
    Q_INVOKABLE QString versionString() const;
    QString lastError() const { return m_lastError; }
    void setLastError(const QString& error);
    QString statusMessage() const { return m_statusMessage; }
    void setStatusMessage(const QString& message);

    // --- layer state ------------------------------------------------------
    Q_INVOKABLE void setLayerLocked(int index, bool locked);
    /// Live opacity during a slider drag — no history entry, just repaint.
    Q_INVOKABLE void previewLayerOpacity(int index, int percent);
    /// Commits the opacity change as a single history step, comparing against
    /// \p previousPercent (the value the drag started from).
    Q_INVOKABLE void commitLayerOpacity(int index, int percent, int previousPercent);
    /// Downscaled preview of a layer's own pixels (image://layerthumb/<i>).
    ///
    /// Served from a snapshot built on the GUI thread: Qt runs image provider
    /// requests on the pixmap-reader thread, and reading layer pixels from
    /// there raced with painting — that race crashed the app.
    QImage layerThumbnail(int index, int width, int height) const;

    // --- actions ----------------------------------------------------------
    Q_INVOKABLE void addLayer();
    Q_INVOKABLE void deleteLayer();
    Q_INVOKABLE void duplicateLayer();
    Q_INVOKABLE void moveLayerUp();
    Q_INVOKABLE void moveLayerDown();
    /// Moves a layer to an arbitrary index (drag & drop in the Layers docker).
    Q_INVOKABLE void moveLayerTo(int fromIndex, int toIndex);
    /// Blend mode names in enum order — index == BlendMode value, which is what
    /// the docker's combo box relies on.
    Q_INVOKABLE QStringList blendModeNames() const;
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
    /// Saves by suffix: ".prism" writes the layered project, anything else
    /// exports a flattened image in that format (Krita's Save As list).
    /// A missing suffix on a project save is corrected to ".prism" — without
    /// that, the file was written with no extension and the Open dialog's
    /// filter could not see it again.
    Q_INVOKABLE bool saveFile(const QUrl& url);
    /// Opens by suffix: ".prism" loads the project, images are imported.
    Q_INVOKABLE bool openFile(const QUrl& url);
    /// Formats the Save dialog offers (Qt format names, e.g. "png", "jpg").
    Q_INVOKABLE QStringList writableFormatNames() const;
    /// File-dialog filters built from what this Qt build can actually do, so
    /// the dialogs never offer a format whose plugin is missing (which showed
    /// as "Save As WebP" doing nothing).
    Q_INVOKABLE QStringList imageOpenFilters() const;
    Q_INVOKABLE QStringList imageSaveFilters() const;

    // --- tool shortcuts -----------------------------------------------------
    /// Per-tool hotkey, user-configurable and persisted (QSettings). Tools are
    /// single-key shortcuts like Krita's; an empty key disables the shortcut.
    Q_PROPERTY(QVariantList toolShortcutList READ toolShortcutList NOTIFY toolShortcutsChanged)
    Q_INVOKABLE QVariantList toolShortcutList() const;
    Q_INVOKABLE QString toolShortcut(int tool) const;
    Q_INVOKABLE QString defaultToolShortcut(int tool) const;
    /// \p key is a single character ("" clears the shortcut). Assigning a key
    /// that another tool already uses resets that tool to a keyless state so
    /// two tools can never claim the same shortcut.
    Q_INVOKABLE void setToolShortcut(int tool, const QString& key);
    Q_INVOKABLE bool openProject(const QUrl& url);
    Q_INVOKABLE bool importImage(const QUrl& url);
    /// Imports an image INTO the current document as a new normal raster
    /// layer (centered, clipped to canvas), undoable like any layer action.
    /// With no document open it falls back to opening the image as one.
    Q_INVOKABLE bool importImageAsLayer(const QUrl& url);
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
    Q_INVOKABLE void beginStroke(float x, float y, float pressure = 1.0F);
    Q_INVOKABLE void extendStroke(float x, float y, float pressure = 1.0F);
    /// Applies the pressure-curve gamma to a raw pressure value.
    float shapedPressure(float pressure) const;
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
    void lastErrorChanged();
    void statusMessageChanged();
    void toolShortcutsChanged();
    void canvasChanged(int x, int y, int w, int h);

private:
    void rebuildFullComposite();
    void rebuildStrokePreview();
    void addRecentFile(const QString& path);
    void loadRecentFiles();
    void loadUserPresets();
    void saveUserPresets();

    prism::core::Document m_doc{ prism::core::CanvasSize{ 1280, 800, 72 } };
    prism::core::History m_history;
    prism::canvas::Compositor m_compositor{ m_doc };
    prism::canvas::BrushSettings m_brush;
    qreal m_pressureCurve = 1.0;
    std::vector<prism::canvas::StrokePoint> m_currentStroke;
    prism::core::raster::RasterBuffer m_preview;
    prism::core::geometry::Rect m_strokeDirty;
    /// Region of \p m_preview that currently holds pixels, so the next stroke
    /// clears only that instead of the whole canvas.
    prism::core::geometry::Rect m_previewDirty;
    LayerListModel* m_layersModel = nullptr;
    int m_activeLayer = 0;
    qreal m_zoom = 1.0;
    int m_tool = BrushTool;
    int m_activePreset = 0;
    /// User presets plus the built-ins; mutable so the docker can add/remove.
    std::vector<prism::canvas::BrushPreset> m_presets;
    int m_customPresetCounter = 0;
    int m_layerRevision = 0;

    // --- snapshots for the image providers --------------------------------
    // Qt calls QQuickImageProvider::requestImage on its pixmap-reader thread.
    // Those providers must never touch the document: they read these snapshots,
    // which are rebuilt on the GUI thread by a timer. (Reading layer pixels off
    // the GUI thread crashed the app during painting.)
    mutable QMutex m_snapshotMutex;
    QVector<QImage> m_layerThumbs;
    QImage m_selectionSnapshot;
    int m_snapshotRevision = -1;
    QTimer* m_snapshotTimer = nullptr;
    void refreshSnapshots();
    /// Active selection in canvas pixels; empty means "no selection".
    /// Active selection. Mask-based so lasso and magic wand work too.
    prism::canvas::Selection m_selection;
    /// Shape-tool drag state (line / rectangle / ellipse / gradient).
    prism::core::geometry::Point m_shapeStart;
    prism::core::geometry::Point m_shapeEnd;
    bool m_shapeActive = false;
    bool m_shapeFilled = false;
    /// Points collected by the click-based path tools.
    std::vector<prism::core::geometry::Point> m_pathPoints;
    /// Freehand Path drag samples.
    std::vector<prism::canvas::StrokePoint> m_freePath;
    /// Dynamic Brush: the lagged brush position.
    prism::canvas::StrokePoint m_dynamicPos;
    /// Smoothed pen pressure for the stroke in progress. Real tablets report a
    /// noisy signal (and a dip right at tip contact), which otherwise spikes the
    /// stamp size sample to sample.
    float m_smoothedPressure = 1.0F;
    /// Brush colour remembered per tool group: [0] the painting tools, [1] the
    /// eraser. Switching tools restores the incoming tool's colour, so the
    /// eraser cannot carry a colour change back to the brush.
    prism::core::raster::Color m_colorByTool[2] = {};
    /// Tool the eyedropper was picked from, to return there after a pick.
    int m_toolBeforeEyedropper = BrushTool;
    /// Live layer drag: the pre-drag pixels, the content rectangle inside them
    /// and the offset currently previewed.
    bool m_movingLayer = false;
    prism::core::raster::RasterBuffer m_moveBefore;
    prism::core::geometry::Rect m_moveBbox{};
    int m_moveDx = 0;
    int m_moveDy = 0;
    /// Live-edit drag state (eraser AND blend): the active layer as it was
    /// before the drag, kept for the undo command. The layer is edited live.
    prism::core::raster::RasterBuffer m_eraseBefore;
    bool m_erasing = false;
    /// Blend tool: true while smearing, plus the colour being dragged.
    bool m_blending = false;
    prism::core::raster::Color m_carried;
    /// Lasso fill: the enclosure being dragged.
    std::vector<prism::core::geometry::Point> m_lassoFill;

    /// Commits a patch of the active layer (shared by every non-stroke tool).
    void commitPatch(const prism::core::raster::RasterBuffer& original,
                     const prism::core::raster::RasterBuffer& work,
                     const prism::core::geometry::Rect& rect, const char* label);

    /// Clears just \p rect in the stroke preview (see m_previewDirty).
    void clearPreviewRegion(const prism::core::geometry::Rect& rect);

    /// Paints one stroke segment, plus its mirror images when the Multibrush
    /// tool is active (Krita mirrors about the canvas centre).
    prism::core::geometry::Rect paintSegmentSymmetrical(prism::core::raster::RasterBuffer& buffer,
                                                        const prism::canvas::BrushSettings& brush,
                                                        const prism::canvas::StrokePoint& a,
                                                        const prism::canvas::StrokePoint& b);
    bool m_mirrorH = false;
    bool m_mirrorV = false;
    bool m_startPageVisible = true;
    /// Selftest runs must not touch the user's persisted state (see addRecentFile).
    bool m_selfTestMode = false;
    QStringList m_recentFiles;
    QString m_documentName = tr("Untitled");
    QString m_lastError;
    QString m_statusMessage;
    /// True until the user moves a brush slider — controls the "Custom" label.
    bool m_presetIntact = true;
};
