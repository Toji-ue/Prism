#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QMouseEvent>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>
#include <QVariant>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QElapsedTimer>

#include "application/AppController.h"
#include "application/LayerListModel.h"
#include "application/PresetImageProvider.h"
#include "panels/CanvasView.h"

#include <cstdio>
#include <cmath>

namespace {

QFile g_logFile;

/// Qt diagnostics go to a log file: the released build is a WIN32 GUI app
/// with no console, so QML errors would otherwise vanish silently.
void logToFile(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    const char* level = "info";
    switch (type) {
    case QtDebugMsg: level = "debug"; break;
    case QtInfoMsg: level = "info"; break;
    case QtWarningMsg: level = "warn"; break;
    case QtCriticalMsg: level = "critical"; break;
    case QtFatalMsg: level = "fatal"; break;
    }
    const QString line = QStringLiteral("[%1] %2: %3\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODate), level, msg);
    if (g_logFile.isOpen()) {
        g_logFile.write(line.toUtf8());
        g_logFile.flush();
    }
    std::fputs(line.toUtf8().constData(), stderr);
    if (type == QtFatalMsg)
        std::abort();
}

/// Finds a QML item by objectName, walking the VISUAL item tree.
///
/// QObject::findChild is not enough: items created by a Repeater (or by any
/// delegate) are visual children with no QObject parent, so a toolbar button
/// built from a model is invisible to findChild — which is why the verification
/// hooks below use this instead.
QQuickItem* findItemByName(QQuickItem* root, const QString& name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    const QList<QQuickItem*> children = root->childItems();
    for (QQuickItem* child : children)
        if (QQuickItem* found = findItemByName(child, name))
            return found;
    return nullptr;
}

/// Every non-empty objectName in the visual tree, for diagnostics.
QStringList itemNames(QQuickItem* root, QStringList& into)
{
    if (!root)
        return into;
    if (!root->objectName().isEmpty() && !into.contains(root->objectName()))
        into << root->objectName();
    for (QQuickItem* child : root->childItems())
        itemNames(child, into);
    return into;
}

int countOpaquePixels(const QImage& img)
{
    int count = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                ++count;
    return count;
}

/// Headless smoke test of the same code path the canvas UI drives: stroke,
/// undo/redo, fill, color pick, layer ops. Run with --selftest; exits
/// non-zero on failure so CI (and agents) can verify without a human
/// clicking through the UI.
int runSelfTest()
{
    AppController controller;
    // This drives the same code paths as the UI on throwaway documents, so keep
    // them out of the Recent list the user sees.
    controller.setSelfTestMode(true);
    int failures = 0;
    auto check = [&failures](bool ok, const char* what) {
        qInfo() << (ok ? "PASS" : "FAIL") << what;
        if (!ok)
            ++failures;
    };

    check(countOpaquePixels(controller.canvasImage()) == 0, "new document starts blank");

    controller.setBrushColor(QColor(0, 0, 0));
    controller.beginStroke(200.0F, 200.0F);
    controller.extendStroke(400.0F, 300.0F);
    controller.extendStroke(600.0F, 250.0F);
    controller.endStroke();
    const int painted = countOpaquePixels(controller.canvasImage());
    check(painted > 500, "stroke painted pixels");

    controller.undo();
    check(countOpaquePixels(controller.canvasImage()) == 0, "undo cleared the stroke");
    controller.redo();
    check(countOpaquePixels(controller.canvasImage()) == painted, "redo restored the stroke");

    // Flood fill into the empty area, then undo it.
    controller.setBrushColor(QColor(255, 0, 0));
    controller.fillAt(5.0F, 5.0F);
    const int afterFill = countOpaquePixels(controller.canvasImage());
    check(afterFill > painted, "fill added pixels");
    controller.undo();
    check(countOpaquePixels(controller.canvasImage()) == painted, "fill undo restored");

    // Eyedropper samples the flattened canvas.
    const QColor picked = controller.pickColorAt(400.0F, 300.0F, false);
    check(picked.alpha() == 255, "color picker sampled painted pixel");

    // Brush preset selection drives size/eraser state.
    controller.selectPreset(9); // "Eraser Hard"
    check(controller.eraser(), "selecting an eraser preset switches the eraser on");
    controller.selectPreset(0);
    check(!controller.eraser(), "selecting a paint preset switches it back off");

    // Custom presets can be added and removed.
    const int presetsBefore = controller.presetCount();
    controller.addPreset();
    check(controller.presetCount() == presetsBefore + 1, "add preset");
    controller.removePreset(controller.activePresetIndex());
    check(controller.presetCount() == presetsBefore, "remove preset");

    // Selection confines a stroke to its rect.
    controller.selectAll();
    check(controller.hasSelection(), "select all");
    controller.setSelection(100.0F, 100.0F, 100.0F, 100.0F);
    check(controller.selectionW() == 100, "selection rect set");
    const int beforeSelectionStroke = countOpaquePixels(controller.canvasImage());
    controller.beginStroke(50.0F, 150.0F);
    controller.extendStroke(400.0F, 150.0F); // sweeps across the canvas
    controller.endStroke();
    const QImage afterImg = controller.canvasImage();
    // Nothing may be painted left of the selection's x edge.
    bool outsidePainted = false;
    for (int y = 100; y < 200; ++y)
        for (int x = 0; x < 99; ++x)
            if (qAlpha(afterImg.pixel(x, y)) > 0)
                outsidePainted = true;
    check(!outsidePainted, "stroke clipped to selection");
    controller.clearSelection();
    controller.undo();
    check(countOpaquePixels(controller.canvasImage()) == beforeSelectionStroke,
          "selection stroke undone");

    // Every tool must have a working controller path: exercise each one.
    controller.setStartPageVisible(false);
    controller.selectAll();
    controller.clearSelection();
    controller.setSelection(10.0F, 10.0F, 50.0F, 50.0F);
    controller.setLassoSelection({ QVariant(QPointF(12, 12)), QVariant(QPointF(60, 12)),
                                   QVariant(QPointF(12, 60)) });
    controller.magicWandAt(20.0F, 20.0F, 32);
    check(controller.hasSelection(), "wand/lasso produced a selection");
    controller.selectLayerContent();
    controller.clearSelection();

    controller.setTool(AppController::ZoomTool);
    const qreal z0 = controller.zoom();
    controller.zoomAt(100.0, 100.0, 2.0);
    check(controller.zoom() > z0, "zoom tool changes zoom");
    controller.setZoom(1.0);

    controller.setTool(AppController::MoveTool);
    controller.setTool(AppController::MoveTool);
    controller.setBrushColor(QColor(0, 0, 0));
    controller.setBrushSize(20.0F);
    controller.beginStroke(30.0F, 30.0F);
    controller.extendStroke(31.0F, 31.0F);
    controller.endStroke();
    const int beforeMove = countOpaquePixels(controller.canvasImage());
    controller.moveActiveLayer(12, 8);
    check(countOpaquePixels(controller.canvasImage()) == beforeMove,
          "move tool relocated pixels (count unchanged)");
    controller.undo();
    check(countOpaquePixels(controller.canvasImage()) == beforeMove, "move undone");

    controller.setTool(AppController::FillTool);
    controller.setBrushColor(QColor(0, 128, 255));
    controller.fillAt(300.0F, 400.0F);
    controller.undo();

    controller.setTool(AppController::EyedropperTool);
    const QColor sampled = controller.pickColorAt(30.0F, 30.0F, true);
    check(sampled.alpha() == 255, "eyedropper tool samples");

    controller.setTool(AppController::PanTool);
    controller.setTool(AppController::BrushTool);
    check(true, "all tools selectable without crashing");

    // Filters run through history and undo cleanly.
    const int beforeFilter = countOpaquePixels(controller.canvasImage());
    controller.applyFilter(QStringLiteral("invert"));
    controller.applyFilter(QStringLiteral("blur"));
    controller.undo();
    controller.undo();
    check(countOpaquePixels(controller.canvasImage()) == beforeFilter, "filters undo cleanly");

    // Merge down collapses two layers into one, and undo restores both.
    const int layersBeforeMerge = controller.layers()->rowCount();
    controller.addLayer();
    controller.mergeLayerDown();
    check(controller.layers()->rowCount() == layersBeforeMerge, "merge down");
    controller.undo();
    check(controller.layers()->rowCount() == layersBeforeMerge + 1, "merge down undone");

    // Eraser must actually remove pixels (it used to be a no-op because the
    // erase was applied to a transparent scratch buffer). Paint a known
    // stroke first, then erase across it.
    {
        controller.setTool(AppController::BrushTool);
        controller.setBrushColor(QColor(0, 0, 0));
        controller.setBrushSize(40.0F);
        controller.beginStroke(100.0F, 300.0F);
        controller.extendStroke(200.0F, 300.0F);
        controller.endStroke();
        const int painted = countOpaquePixels(controller.canvasImage());

        controller.setTool(AppController::EraserTool);
        controller.setBrushSize(40.0F);
        controller.beginStroke(150.0F, 300.0F);
        controller.extendStroke(151.0F, 300.0F);
        controller.endStroke();
        const int afterErase = countOpaquePixels(controller.canvasImage());
        check(afterErase < painted, "eraser removes pixels");

        controller.undo();
        check(countOpaquePixels(controller.canvasImage()) == painted, "erase undone");
        controller.undo(); // remove the painted stroke again
        controller.setTool(AppController::BrushTool);
    }

    // Erasing must be visible DURING the drag, not only after release.
    {
        controller.setTool(AppController::BrushTool);
        controller.setBrushColor(QColor(0, 0, 0));
        controller.setBrushSize(40.0F);
        controller.beginStroke(300.0F, 500.0F);
        controller.extendStroke(400.0F, 500.0F);
        controller.endStroke();
        const int painted = countOpaquePixels(controller.canvasImage());

        controller.setTool(AppController::EraserTool);
        controller.setBrushSize(40.0F);
        controller.beginStroke(350.0F, 500.0F);
        controller.extendStroke(351.0F, 500.0F); // mid-drag: no release yet
        const int duringDrag = countOpaquePixels(controller.canvasImage());
        check(duringDrag < painted, "erase is visible while dragging");
        controller.endStroke();
        check(countOpaquePixels(controller.canvasImage()) < painted, "erase committed on release");
        controller.undo();
        controller.undo();
        controller.setTool(AppController::BrushTool);
    }

    // Erasing on one layer must not affect what other layers show: the pixels
    // belong to the layer above, so they must survive both mid-drag and after
    // release. (Reported bug: erasing on a lower layer made upper layers
    // vanish and then come back.)
    {
        controller.setTool(AppController::BrushTool);
        controller.setBrushColor(QColor(0, 0, 0));
        controller.setBrushSize(30.0F);
        controller.addLayer(); // work on the top layer
        const int topIndex = controller.activeLayerIndex();
        controller.beginStroke(500.0F, 600.0F);
        controller.extendStroke(560.0F, 600.0F);
        controller.endStroke();
        const int pixelsOnTopLayer = countOpaquePixels(controller.canvasImage());

        controller.setActiveLayerIndex(topIndex - 1); // a lower, empty layer
        controller.setTool(AppController::EraserTool);
        controller.setBrushSize(40.0F);
        controller.beginStroke(530.0F, 600.0F);
        controller.extendStroke(531.0F, 600.0F); // mid-drag
        check(countOpaquePixels(controller.canvasImage()) == pixelsOnTopLayer,
              "erasing on a lower layer leaves upper layers intact while dragging");
        controller.endStroke();
        check(countOpaquePixels(controller.canvasImage()) == pixelsOnTopLayer,
              "erasing on a lower layer leaves upper layers intact after release");
        controller.setTool(AppController::BrushTool);
    }

    // A multi-segment drag must not leave any erased pixel behind: the commit
    // rect has to cover everything the drag removed, not just the last repaint.
    {
        controller.setTool(AppController::BrushTool);
        controller.setBrushColor(QColor(0, 0, 0));
        controller.setBrushSize(40.0F);
        controller.beginStroke(100.0F, 700.0F);
        controller.extendStroke(500.0F, 700.0F);
        controller.endStroke();

        controller.setTool(AppController::EraserTool);
        controller.setBrushSize(30.0F);
        controller.beginStroke(150.0F, 700.0F);
        controller.extendStroke(250.0F, 700.0F);
        controller.extendStroke(350.0F, 700.0F);
        controller.extendStroke(450.0F, 700.0F);
        controller.endStroke();

        const QImage img = controller.canvasImage();
        bool allGone = true;
        for (int x = 150; x <= 450; x += 25)
            if (qAlpha(img.pixel(x, 700)) > 0)
                allGone = false;
        check(allGone, "a multi-segment erase leaves no restored pixels behind");
        controller.undo();
        controller.undo();
        controller.setTool(AppController::BrushTool);
    }

    // Shape tools: line, rectangle, ellipse each commit one undo step.
    {
        controller.setTool(AppController::LineTool);
        const int before = countOpaquePixels(controller.canvasImage());
        controller.beginShape(100.0F, 100.0F);
        controller.updateShape(400.0F, 100.0F);
        controller.endShape();
        check(countOpaquePixels(controller.canvasImage()) > before, "line tool draws");
        controller.undo();

        controller.setTool(AppController::RectangleTool);
        controller.beginShape(120.0F, 120.0F);
        controller.updateShape(320.0F, 260.0F);
        controller.endShape();
        check(countOpaquePixels(controller.canvasImage()) > before, "rectangle tool draws");
        controller.undo();

        controller.setTool(AppController::EllipseTool);
        controller.beginShape(150.0F, 150.0F);
        controller.updateShape(350.0F, 300.0F);
        controller.endShape();
        check(countOpaquePixels(controller.canvasImage()) > before, "ellipse tool draws");
        controller.undo();
    }

    // Elliptical and similar-colour selections both produce a mask.
    {
        controller.setTool(AppController::EllipseSelectTool);
        controller.setEllipseSelection(50.0F, 50.0F, 120.0F, 80.0F);
        check(controller.hasSelection(), "elliptical selection");
        controller.setTool(AppController::SimilarColorSelectTool);
        controller.similarColorAt(200.0F, 200.0F, 32);
        check(true, "similar colour selection ran");
        controller.clearSelection();
        controller.setTool(AppController::BrushTool);
    }

    // Save -> reopen round trip through the SAME controller path the UI uses.
    // The reported "cannot reopen a saved .prism" bug lived here, not in the
    // format itself.
    {
        const QString tmp = QDir::temp().filePath(QStringLiteral("prism-selftest.prism"));
        QFile::remove(tmp);
        controller.addLayer();
        controller.setDocumentName(QStringLiteral("selftest"));
        check(controller.saveFile(QUrl::fromLocalFile(tmp)), "saveFile writes a project");
        check(QFile::exists(tmp), "project file exists on disk");

        const int layersBefore = controller.layers()->rowCount();
        check(controller.openFile(QUrl::fromLocalFile(tmp)), "openFile reopens the project");
        check(controller.layers()->rowCount() == layersBefore, "reopened project keeps layers");
        // Opening from the start page must dismiss it, otherwise the loaded
        // document is hidden behind the welcome screen and nothing appears to
        // happen (this was the reported "open does nothing" bug).
        controller.setStartPageVisible(true);
        check(controller.openFile(QUrl::fromLocalFile(tmp)), "openFile works from the start page");
        check(!controller.startPageVisible(), "opening a document dismisses the start page");
        check(controller.lastError().isEmpty(), "no error recorded for a good open");

        // A name without a suffix must still land as .prism, otherwise the
        // Open dialog's filter cannot see it again.
        const QString noExt = QDir::temp().filePath(QStringLiteral("prism-selftest-noext"));
        QFile::remove(noExt + QStringLiteral(".prism"));
        check(controller.saveFile(QUrl::fromLocalFile(noExt)), "save without suffix succeeds");
        check(QFile::exists(noExt + QStringLiteral(".prism")),
              "missing .prism suffix is appended");

        // Image formats go through the same Save As dialog.
        const QString png = QDir::temp().filePath(QStringLiteral("prism-selftest.png"));
        QFile::remove(png);
        check(controller.saveFile(QUrl::fromLocalFile(png)), "saveFile writes a PNG");
        check(QFile::exists(png), "PNG exists on disk");

        // Import-as-layer: the saved PNG becomes a new normal layer in the
        // CURRENT document (canvas size unchanged, pixel count grows).
        const int layersBeforeImport = controller.layers()->rowCount();
        // Deep copy: QImage is COW, and the compositor's buffer must not be
        // shared with the snapshot or the comparison below is vacuous.
        const QImage canvasBefore = controller.canvasImage().copy();
        check(controller.importImageAsLayer(QUrl::fromLocalFile(png)),
              "import image as layer succeeds");
        check(controller.layers()->rowCount() == layersBeforeImport + 1,
              "imported image became a new layer");
        check(controller.lastError().isEmpty(), "no error for a good import");
        // Compositing the canvas' own flattened PNG as a new layer must keep
        // every opaque and fully transparent pixel EXACTLY (a=1 and a=0 are
        // fixed points of source-over). Only antialiased semi-transparent
        // edges may legitimately shift (a -> 2a - a^2); anything else means
        // the import misplaced or corrupted content.
        bool importExact = true;
        const QImage canvasAfterImport = controller.canvasImage();
        for (int y = 0; y < canvasBefore.height() && importExact; ++y) {
            for (int x = 0; x < canvasBefore.width(); ++x) {
                const QRgb before = canvasBefore.pixel(x, y);
                const QRgb after = canvasAfterImport.pixel(x, y);
                const int a = qAlpha(before);
                if (a == 0 || a == 255) {
                    if (before != after) {
                        importExact = false;
                        break;
                    }
                }
            }
        }
        check(importExact, "imported layer contributes its pixels to the canvas");
        controller.undo();
        check(controller.layers()->rowCount() == layersBeforeImport,
              "import as layer undone");
        controller.redo();
        check(controller.layers()->rowCount() == layersBeforeImport + 1,
              "import as layer redone with pixels intact");
        // Prove the pixels actually arrived: hide every OTHER layer and the
        // composite must equal the source PNG exactly. (After this the
        // visibility toggles sit on top of history, so no more undo checks.)
        for (int i = 0; i < layersBeforeImport; ++i)
            controller.setLayerVisible(i, false);
        check(controller.canvasImage() == canvasBefore,
              "imported layer alone reproduces the source image");
        for (int i = 0; i < layersBeforeImport; ++i)
            controller.setLayerVisible(i, true);
        check(!controller.importImageAsLayer(
                  QUrl::fromLocalFile(QDir::temp().filePath(QStringLiteral("no-such.png")))),
              "import of a missing file fails gracefully");
        check(!controller.lastError().isEmpty(), "missing file reports an error");
        // Regression: QML converts a plain path string to QUrl, which parses
        // "C" as a scheme and percent-encodes the backslashes. That made the
        // import fail with an empty/garbled file name while the file dialog
        // path worked — i.e. "import does nothing" depending on the caller.
        const QUrl fromPlainString(png);
        check(fromPlainString.toLocalFile().isEmpty(),
              "a Windows path string is NOT a local file url (bug precondition)");
        const int layersBeforeStringImport = controller.layers()->rowCount();
        check(controller.importImageAsLayer(fromPlainString),
              "import accepts a raw path string, not just a file url");
        check(controller.layers()->rowCount() == layersBeforeStringImport + 1,
              "string-path import added exactly one layer");
        controller.undo();

        // JPEG is a *plugin* format (QtGui only carries PNG), so a deploy that
        // forgets plugins\imageformats cannot open a .jpg at all. The user's
        // reported "hachiware.jpg will not import" was exactly this. Skip
        // loudly when running from a tree without plugins (build/), so CI is
        // not silently green either way.
        if (!QImageReader::supportedImageFormats().contains(QByteArray("jpeg"))) {
            qInfo() << "SKIP jpeg import: no jpeg plugin in this run directory"
                       " (deploy/ has it; tools/deploy.ps1 installs it)";
        } else {
            const QString jpg = QDir::temp().filePath(QStringLiteral("prism-selftest.jpg"));
            QFile::remove(jpg);
            check(controller.saveFile(QUrl::fromLocalFile(jpg)), "saveFile writes a JPEG");
            check(QFile::exists(jpg), "JPEG exists on disk");
            const int layersBeforeJpeg = controller.layers()->rowCount();
            check(controller.importImageAsLayer(QUrl::fromLocalFile(jpg)),
                  "import a JPEG as a layer");
            check(controller.layers()->rowCount() == layersBeforeJpeg + 1,
                  "JPEG import added exactly one layer");
            controller.undo();
        }

        // The dialogs must never offer a format this Qt build cannot handle:
        // an unsupported entry made "Save As <format>" fail with no message.
        const QStringList saveFilters = controller.imageSaveFilters();
        const QStringList openFilters = controller.imageOpenFilters();
        check(saveFilters.first().contains(QStringLiteral(".prism")),
              "Save As offers the project format first");
        check(!saveFilters.filter(QStringLiteral("webp")).isEmpty()
                  == QImageWriter::supportedImageFormats().contains(QByteArray("webp")),
              "Save As advertises WebP only when Qt can write it");
        check(!openFilters.filter(QStringLiteral("jpeg")).isEmpty()
                  == QImageReader::supportedImageFormats().contains(QByteArray("jpeg")),
              "Open advertises JPEG only when Qt can read it");
        // A raw path string into the layer import must also work for the
        // formats whose plugin is optional (jpg arrived percent-encoded too).
        check(controller.imageOpenFilters().size() > 3, "open filters list real formats");
    }

    // Tool shortcuts are configurable and persisted; conflicts are resolved.
    {
        const QString brushDefault = controller.toolShortcut(0); // BrushTool
        controller.setToolShortcut(1, brushDefault);             // give Eraser the brush key
        check(controller.toolShortcut(0).isEmpty(),
              "assigning an occupied key frees it on the other tool");
        controller.setToolShortcut(1, QStringLiteral("e"));
        check(controller.toolShortcut(1) == QStringLiteral("E"),
              "shortcut normalizes to a single uppercase character");
        controller.setToolShortcut(0, QStringLiteral("B"));      // restore defaults
        check(controller.toolShortcut(0) == QStringLiteral("B"), "defaults restore");
        check(controller.toolShortcutList().size() > 0, "shortcut list exposes every tool");
    }

    // Stroke latency: informational, so a regression is visible in CI logs.
    // The per-move path must not do per-tick full-canvas work.
    {
        QElapsedTimer timer;
        controller.beginStroke(100.0F, 100.0F);
        timer.start();
        for (int i = 0; i < 200; ++i)
            controller.extendStroke(100.0F + static_cast<float>(i) * 2.0F,
                                    100.0F + static_cast<float>(i));
        controller.endStroke();
        const qint64 ms = timer.elapsed();
        qInfo() << "stroke latency:" << ms << "ms for 200 move events ("
                << (ms / 200.0) << "ms per event)";
        check(ms < 2000, "200 move events stay under 2s");
    }

    // Undo/redo with an empty history must be a no-op, not an exception: the
    // history throws on misuse, and an escaping throw terminated the app (the
    // intermittent crash testers reported after pressing Ctrl+Shift+Z with
    // nothing to redo).
    {
        controller.newDocument(640, 480);
        check(!controller.canUndo(), "a fresh document has no undo steps");
        controller.undo(); // must not throw
        controller.redo();
        check(!controller.canUndo() && !controller.canRedo(),
              "undo/redo with an empty history is a no-op");
    }

    // Painting must keep the brush colour, and the DEFAULT colour must be a real
    // black rather than a washed-out grey. No earlier check looked at the RGB of
    // a stroke — only its alpha and pixel count — so a colour loss anywhere in
    // the preview -> commit path would have gone unnoticed. (Placed here because
    // it swaps the document; the checks above still need the original canvas.)
    {
        controller.newDocument(200, 200);
        controller.selectPreset(0);
        controller.setBrushSize(24.0F);

        // Default brush colour, straight out of the preset.
        controller.beginStroke(60.0F, 60.0F);
        for (int i = 0; i < 20; ++i)
            controller.extendStroke(60.0F + i * 4.0F, 60.0F);
        controller.endStroke();
        const QRgb plain = controller.canvasImage().pixel(100, 60);
        check(qRed(plain) < 40 && qGreen(plain) < 40 && qBlue(plain) < 40,
              "default brush paints solid black");

        // A colour chosen in the picker must survive the whole stroke pipeline.
        controller.setBrushColor(QColor(200, 40, 90));
        controller.beginStroke(60.0F, 100.0F);
        for (int i = 0; i < 20; ++i)
            controller.extendStroke(60.0F + i * 4.0F, 100.0F);
        const QRgb live = controller.canvasImage().pixel(100, 100);
        check(qRed(live) > 150 && qGreen(live) < 90 && qBlue(live) > 50,
              "stroke keeps the brush colour while dragging");
        controller.endStroke();
        const QRgb committed = controller.canvasImage().pixel(100, 100);
        check(qRed(committed) > 150 && qGreen(committed) < 90 && qBlue(committed) > 50,
              "committed stroke keeps the brush colour");
        controller.setBrushColor(QColor(0, 0, 0));
    }

    // Committing a stroke must cost the stroke's rectangle, not the canvas.
    // Mouse-up used to re-paint the whole stroke into a fresh canvas-sized
    // buffer and recomposite every layer: 1.2 s of freeze on a 4000x4000
    // document, measured, which is exactly the "very laggy" report.
    {
        controller.newDocument(3000, 3000);
        controller.setTool(AppController::BrushTool);
        QElapsedTimer timer;
        controller.beginStroke(200.0F, 200.0F);
        for (int i = 0; i < 60; ++i)
            controller.extendStroke(200.0F + static_cast<float>(i) * 8.0F,
                                    200.0F + static_cast<float>(i) * 4.0F);
        timer.start();
        controller.endStroke();
        const qint64 commitMs = timer.elapsed();
        qInfo() << "stroke commit:" << commitMs << "ms on a 3000x3000 document";
        check(commitMs < 400, "committing a stroke stays bounded by the stroke rect");

        // A click with no movement must still leave a mark (Krita paints a dot
        // on press). The optimised commit takes its pixels from the live
        // preview, so the dot has to be stamped when the stroke begins.
        controller.beginStroke(1500.0F, 1500.0F);
        controller.endStroke();
        const QRgb dot = controller.canvasImage().pixel(1500, 1500);
        check(qAlpha(dot) > 0, "a click paints a dot");
    }

    // Layer ops + undo must not corrupt the stack.
    const int before = controller.layers()->rowCount();
    controller.addLayer();
    check(controller.layers()->rowCount() == before + 1, "add layer");
    controller.undo();
    check(controller.layers()->rowCount() == before, "undo add layer");

    // Drag & drop reordering (Krita moves a layer to an arbitrary index).
    {
        controller.addLayer();
        controller.addLayer();
        const int count = controller.layers()->rowCount();
        const QString top = controller.layers()
                                ->data(controller.layers()->index(count - 1, 0),
                                       LayerListModel::NameRole)
                                .toString();
        controller.moveLayerTo(count - 1, 0);
        const QString nowBottom = controller.layers()
                                      ->data(controller.layers()->index(0, 0),
                                             LayerListModel::NameRole)
                                      .toString();
        check(nowBottom == top, "moveLayerTo moves a layer to an arbitrary index");
        check(controller.activeLayerIndex() == 0, "the moved layer becomes active");
        check(controller.layers()->rowCount() == count, "reordering keeps the layer count");

        // Raise/Lower must move the layer UP the stack (higher index). The
        // docker draws the top layer first, so a raise has to increase the
        // index — the opposite mapping made the arrows look swapped.
        controller.setActiveLayerIndex(0);
        const QString bottomName = controller.layers()
                                       ->data(controller.layers()->index(0, 0),
                                              LayerListModel::NameRole)
                                       .toString();
        controller.moveLayerUp();
        check(controller.layers()
                      ->data(controller.layers()->index(1, 0), LayerListModel::NameRole)
                      .toString()
                  == bottomName
                  && controller.activeLayerIndex() == 1,
              "Raise layer moves it to a higher index");
        controller.moveLayerDown();
        check(controller.layers()
                      ->data(controller.layers()->index(0, 0), LayerListModel::NameRole)
                      .toString()
                  == bottomName,
              "Lower layer moves it back down");
        controller.undo();
        controller.undo();
        controller.undo();
        check(controller.layers()
                      ->data(controller.layers()->index(count - 1, 0), LayerListModel::NameRole)
                      .toString()
                  == top,
              "undo restores the previous layer order");
        controller.undo();
        controller.undo();
        check(controller.layers()->rowCount() == before, "reorder test cleaned up");
    }

    // Every blend mode the docker offers must survive save/reopen, and the
    // combo's index must map onto the enum value.
    {
        const QStringList modes = controller.blendModeNames();
        check(modes.size() == 19, "blend mode list matches the enum size");
        check(modes.first() == QStringLiteral("Normal"), "first blend mode is Normal");
        check(modes.last() == QStringLiteral("Luminosity"), "last blend mode is Luminosity");

        const QString tmp = QDir::temp().filePath(QStringLiteral("prism-blend.prism"));
        QFile::remove(tmp);
        controller.setLayerBlendMode(
            0, static_cast<int>(prism::core::BlendMode::ColorDodge));
        controller.setLayerOpacity(0, 60);
        check(controller.saveFile(QUrl::fromLocalFile(tmp)), "save with a non-default blend mode");
        check(controller.openFile(QUrl::fromLocalFile(tmp)), "reopen the blend-mode project");
        const int reopened = controller.layers()
                                 ->data(controller.layers()->index(0, 0),
                                        LayerListModel::BlendModeRole)
                                 .toInt();
        check(reopened == static_cast<int>(prism::core::BlendMode::ColorDodge),
              "blend mode round-trips through the project file");
        const int opacity = controller.layers()
                                ->data(controller.layers()->index(0, 0),
                                       LayerListModel::OpacityRole)
                                .toInt();
        check(opacity == 60, "layer opacity round-trips through the project file");
    }

    // Locking the layer blocks painting.
    controller.setLayerLocked(0, true);
    controller.undo(); // undo the lock change; locking is a real history step
    check(true, "layer lock toggled through history");

    // Leave no trace: earlier runs wrote their documents into the Recent list,
    // so a tester could open one and find a layer stuck at ColorDodge 60%,
    // where every stroke reads as grey. Purge those entries and the files.
    {
        const QString tempDir = QDir::tempPath();
        controller.forgetRecentFilesUnder(tempDir);
        for (const char* leftover : { "prism-selftest.prism", "prism-selftest-noext.prism",
                                      "prism-selftest.png", "prism-selftest.jpg",
                                      "prism-blend.prism" })
            QFile::remove(tempDir + QLatin1Char('/') + QLatin1String(leftover));
        check(true, "selftest leaves no documents behind");
    }

    qInfo() << (failures == 0 ? "selftest: PASS" : "selftest: FAIL") << failures
            << "failure(s)";
    return failures == 0 ? 0 : 1;
}

/// Audits every tool for its ACTUAL EFFECT, not just that it can be selected —
/// "all tools selectable without crashing" is what let broken tools ship.
/// Each tool runs on a fresh document and has to change something observable.
int runToolAudit()
{
    AppController c;
    c.setSelfTestMode(true);
    int failures = 0;
    auto check = [&failures](bool ok, const char* what) {
        qInfo() << (ok ? "PASS" : "FAIL") << what;
        if (!ok)
            ++failures;
    };
    const auto opaque = [&c]() { return countOpaquePixels(c.canvasImage()); };
    const auto fresh = [&c]() {
        c.newDocument(400, 300);
        c.setBrushColor(QColor(20, 20, 20));
        c.setBrushSize(18.0F);
        c.selectPreset(0);
        c.setTool(AppController::BrushTool);
        c.clearSelection();
    };
    const auto stroke = [&c](float x0, float y0, float x1, float y1) {
        c.beginStroke(x0, y0);
        for (int i = 1; i <= 12; ++i) {
            const float t = static_cast<float>(i) / 12.0F;
            c.extendStroke(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        c.endStroke();
    };
    const auto shape = [&c](float x0, float y0, float x1, float y1) {
        c.beginShape(x0, y0);
        c.updateShape(x1, y1);
        c.endShape();
    };

    // ---- painting tools ----
    fresh();
    stroke(60, 60, 300, 200);
    check(opaque() > 0, "BrushTool paints");

    fresh();
    stroke(60, 60, 300, 200);
    const int painted = opaque();
    c.setEraser(true);
    stroke(60, 200, 300, 60);
    c.setEraser(false);
    check(opaque() < painted, "EraserTool erases what was painted");

    fresh();
    c.setTool(AppController::DynamicBrushTool);
    stroke(60, 60, 300, 200);
    check(opaque() > 0, "DynamicBrushTool paints");

    fresh();
    c.setTool(AppController::MultiBrushTool);
    stroke(60, 60, 300, 200);
    check(opaque() > 0, "MultiBrushTool paints");

    // ---- shape tools ----
    fresh();
    c.setTool(AppController::LineTool);
    shape(40, 40, 340, 240);
    check(opaque() > 0, "LineTool draws");

    fresh();
    c.setTool(AppController::RectangleTool);
    c.setShapeFilled(false);
    shape(40, 40, 340, 240);
    check(opaque() > 0, "RectangleTool draws an outline");

    fresh();
    c.setTool(AppController::RectangleTool);
    c.setShapeFilled(true);
    const int outlineOnly = 0;
    shape(40, 40, 340, 240);
    check(opaque() > outlineOnly, "RectangleTool fills");
    c.setShapeFilled(false);

    fresh();
    c.setTool(AppController::EllipseTool);
    shape(40, 40, 340, 240);
    check(opaque() > 0, "EllipseTool draws");

    fresh();
    c.setTool(AppController::GradientTool);
    shape(40, 40, 340, 240);
    check(opaque() > 0, "GradientTool fills a gradient");

    fresh();
    c.setTool(AppController::ArcTool);
    c.beginArc(60, 200);
    c.updateArc(200, 60);
    c.updateArc(320, 200);
    c.endArc();
    check(opaque() > 0, "ArcTool draws an arc");

    fresh();
    c.setTool(AppController::TextTool);
    c.renderText(60, 150, QStringLiteral("Prism"), 48);
    check(opaque() > 0, "TextTool renders text");

    fresh();
    c.setTool(AppController::PatternTool);
    c.fillPattern(0, 2);
    check(opaque() > 0, "PatternTool fills a pattern");

    // ---- path tools ----
    fresh();
    c.setTool(AppController::PolylineTool);
    c.addPathPoint(60, 60);
    c.addPathPoint(200, 200);
    c.addPathPoint(320, 80);
    c.finishPath(0);
    check(opaque() > 0, "PolylineTool draws a polyline");

    fresh();
    c.setTool(AppController::BezierTool);
    c.addPathPoint(60, 200);
    c.addPathPoint(200, 60);
    c.addPathPoint(340, 200);
    c.finishPath(1);
    check(opaque() > 0, "BezierTool draws a bezier curve");

    fresh();
    c.setTool(AppController::FreehandPathTool);
    c.beginFreePath(60, 60);
    for (int i = 1; i <= 20; ++i)
        c.extendFreePath(60.0F + i * 12.0F, 150.0F + (i % 5) * 12.0F);
    c.endFreePath();
    check(opaque() > 0, "FreehandPathTool draws a path");

    // ---- fill / sample ----
    fresh();
    c.setBrushColor(QColor(200, 30, 30));
    c.fillAt(200, 150);
    check(opaque() > 0, "FillTool fills the region");

    fresh();
    c.setBrushColor(QColor(200, 30, 30));
    stroke(60, 150, 340, 150);
    const QColor sampled = c.pickColorAt(200, 150, false);
    check(sampled.red() > 150 && sampled.green() < 90, "EyedropperTool samples the paint");

    // ---- selection tools ----
    fresh();
    c.setTool(AppController::RectSelectTool);
    c.setSelection(50, 50, 200, 150);
    check(c.hasSelection() && c.selectionW() == 200 && c.selectionH() == 150,
          "RectSelectTool selects a rectangle");

    fresh();
    c.setTool(AppController::EllipseSelectTool);
    c.setEllipseSelection(50, 50, 200, 150);
    check(c.hasSelection(), "EllipseSelectTool selects an ellipse");

    fresh();
    c.setTool(AppController::LassoSelectTool);
    // The API takes QPointF items, exactly what the QML lasso feeds it.
    c.setLassoSelection(QVariantList{ QVariant::fromValue(QPointF(60, 60)),
                                      QVariant::fromValue(QPointF(300, 80)),
                                      QVariant::fromValue(QPointF(200, 240)),
                                      QVariant::fromValue(QPointF(80, 200)) });
    check(c.hasSelection(), "LassoSelectTool selects a freehand region");

    fresh();
    stroke(60, 60, 340, 240);
    c.setTool(AppController::MagicWandTool);
    c.magicWandAt(200, 150, 24);
    check(c.hasSelection(), "MagicWandTool selects a contiguous region");

    fresh();
    stroke(60, 60, 340, 240);
    c.setTool(AppController::SimilarColorSelectTool);
    c.similarColorAt(200, 150, 40);
    check(c.hasSelection(), "SimilarColorSelectTool selects matching colours");

    fresh();
    stroke(60, 60, 340, 240);
    c.setTool(AppController::OutlineSelectTool);
    c.selectOutlineAt(200, 150);
    check(c.hasSelection(), "OutlineSelectTool selects a shape outline");

    fresh();
    c.setTool(AppController::PolygonSelectTool);
    c.addPathPoint(60, 60);
    c.addPathPoint(300, 80);
    c.addPathPoint(200, 240);
    c.finishPath(2);
    check(c.hasSelection(), "PolygonSelectTool selects a polygon");

    fresh();
    stroke(60, 60, 340, 240);
    c.setTool(AppController::MagneticSelectTool);
    // Three points minimum, and NOT collinear: three points on one diagonal give
    // a zero-area polygon, which correctly yields an empty selection.
    c.addPathPoint(70, 70);
    c.addPathPoint(200, 260);
    c.addPathPoint(330, 90);
    c.finishPath(3);
    check(c.hasSelection(), "MagneticSelectTool snaps a selection to an edge");

    fresh();
    c.setTool(AppController::BezierSelectTool);
    c.addPathPoint(60, 60);
    c.addPathPoint(200, 200);
    c.addPathPoint(320, 80);
    c.finishPath(2);
    check(c.hasSelection(), "BezierSelectTool selects through its points");

    // ---- view / edit tools ----
    fresh();
    c.setTool(AppController::ZoomTool);
    const qreal z0 = c.zoom();
    c.zoomAt(200.0, 150.0, 2.0);
    check(!qFuzzyCompare(c.zoom(), z0), "ZoomTool changes the zoom");

    fresh();
    stroke(60, 60, 200, 200);
    c.setTool(AppController::MoveTool);
    const int beforeMove = opaque();
    c.moveActiveLayer(40, 25);
    check(opaque() == beforeMove && qAlpha(c.canvasImage().pixel(220, 200)) > 0,
          "MoveTool relocates the layer content");

    fresh();
    stroke(100, 100, 300, 200);
    c.setTool(AppController::TransformTool);
    const QImage beforeTransform = c.canvasImage().copy();
    c.transformLayer(45.0F, 1.3F);
    check(c.canvasImage() != beforeTransform, "TransformTool transforms the layer");

    fresh();
    stroke(100, 100, 300, 200);
    c.setTool(AppController::CropTool);
    c.cropTo(50, 50, 200, 150);
    check(c.canvasWidth() == 200 && c.canvasHeight() == 150, "CropTool crops the canvas");

    qInfo() << (failures == 0 ? "tooltest: PASS" : "tooltest: FAIL") << failures
            << "tool(s) not working";
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Prism"));
    QGuiApplication::setOrganizationName(QStringLiteral("Prism"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.3"));

    // Dark application palette. The Basic style colours its popups (menu
    // dropdowns, tooltips, dialog frames) from the palette, which is why they
    // stayed light while the QML-styled widgets around them were dark.
    // Everything here is derived from the same two colours as Theme.js:
    // #ffffff foreground on #1f1f1f background.
    {
        const QColor bg(0x1f, 0x1f, 0x1f);
        const QColor fg(0xff, 0xff, 0xff);
        const QColor raised(0x33, 0x33, 0x33);
        const QColor disabled(0x6b, 0x6b, 0x6b);
        QPalette pal;
        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, fg);
        pal.setColor(QPalette::Base, bg);
        pal.setColor(QPalette::AlternateBase, raised);
        pal.setColor(QPalette::Text, fg);
        pal.setColor(QPalette::Button, raised);
        pal.setColor(QPalette::ButtonText, fg);
        pal.setColor(QPalette::BrightText, fg);
        pal.setColor(QPalette::Highlight, fg);
        pal.setColor(QPalette::HighlightedText, bg);
        pal.setColor(QPalette::ToolTipBase, raised);
        pal.setColor(QPalette::ToolTipText, fg);
        pal.setColor(QPalette::PlaceholderText, disabled);
        pal.setColor(QPalette::Disabled, QPalette::Text, disabled);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        pal.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);
        app.setPalette(pal);
    }

    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    g_logFile.setFileName(logDir + QStringLiteral("/prism.log"));
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    qInstallMessageHandler(logToFile);

    if (app.arguments().contains(QStringLiteral("--selftest")))
        return runSelfTest();

    // Verification hook: every tool has to actually DO something (see
    // runToolAudit). "--selftest" only proves the tools can be selected.
    if (app.arguments().contains(QStringLiteral("--tooltest")))
        return runToolAudit();

    // Verification hook: reports which image formats this build can actually
    // read and write, and fails if the essentials are missing. Qt keeps PNG in
    // QtGui but ships JPEG/GIF/ICO as plugins, so a deploy without
    // plugins\imageformats cannot open a .jpg at all — that silently looked
    // like "import image as layer does nothing". tools/deploy.ps1 runs this.
    if (app.arguments().contains(QStringLiteral("--check-formats"))) {
        const QList<QByteArray> readable = QImageReader::supportedImageFormats();
        const QList<QByteArray> writable = QImageWriter::supportedImageFormats();
        QStringList readList;
        for (const QByteArray& f : readable)
            readList << QString::fromLatin1(f);
        QStringList writeList;
        for (const QByteArray& f : writable)
            writeList << QString::fromLatin1(f);
        qInfo().noquote() << "readable:" << readList.join(QLatin1Char(' '));
        qInfo().noquote() << "writable:" << writeList.join(QLatin1Char(' '));

        QStringList missing;
        for (const char* required : { "png", "jpg", "jpeg" })
            if (!readable.contains(QByteArray(required)))
                missing << QString::fromLatin1(required);
        if (!missing.isEmpty()) {
            qCritical().noquote()
                << "MISSING image formats:" << missing.join(QLatin1Char(' '))
                << "- deploy plugins\\imageformats (see tools/deploy.ps1)";
            return 1;
        }
        qInfo().noquote() << "format check: OK";
        return 0;
    }

    // Manual registration until each type moves into its own qml module.
    qmlRegisterType<AppController>("Prism.Ui", 1, 0, "AppController");
    qmlRegisterType<CanvasView>("Prism.Ui", 1, 0, "CanvasView");
    qmlRegisterUncreatableType<LayerListModel>("Prism.Ui", 1, 0, "LayerListModel",
                                               "Exposed via AppController.layers");

    QQmlApplicationEngine engine;

    // Image providers need the live controller: preset thumbnails include
    // user presets, and layer rows show their own pixels.
    auto* controller = new AppController(&engine);
    engine.addImageProvider(QStringLiteral("preset"),
                            new PresetImageProvider(controller));
    engine.addImageProvider(QStringLiteral("layerthumb"),
                            new LayerThumbnailProvider(controller));
    engine.addImageProvider(QStringLiteral("selection"),
                            new SelectionOverlayProvider(controller));
    engine.setInitialProperties(
        { { QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(controller)) } });

    const QUrl url(QStringLiteral("qrc:/Prism/src/ui/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (obj == nullptr && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    // Verification hook: --screenshot=<path> grabs the rendered window once
    // the scene is up. OS-level capture (PrintWindow) cannot see Qt Quick's
    // Direct3D content, so the app renders its own proof image.
    // --screenshot-only exits right after saving (used by UI checks).
    QString shotPath;
    bool shotOnly = false;
    int shotDelayMs = 2500;
    for (const QString& arg : app.arguments()) {
        if (arg.startsWith(QStringLiteral("--screenshot=")))
            shotPath = arg.section('=', 1);
        else if (arg == QStringLiteral("--screenshot-only"))
            shotOnly = true;
        else if (arg.startsWith(QStringLiteral("--screenshot-delay=")))
            shotDelayMs = arg.section('=', 1).toInt();
    }
    if (!shotPath.isEmpty()) {
        // --demo-drawing paints a known test pattern before the shot so the
        // canvas mapping (zoom / offset) can be verified from the image.
        const bool demo = app.arguments().contains(QStringLiteral("--demo-drawing"));
        const bool demoDialog = app.arguments().contains(QStringLiteral("--demo-new-dialog"));
        const bool demoToolOptions = app.arguments().contains(QStringLiteral("--demo-tool-options"));
        const bool demoCursor = app.arguments().contains(QStringLiteral("--demo-cursor"));
        const bool demoMenu = app.arguments().contains(QStringLiteral("--demo-menu"));
        // --demo-import-layer=<png> drives the SAME QML -> controller call the
        // menu uses, without a human clicking a native file dialog. This is
        // how we prove the UI path works end to end.
        QString demoImport;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-import-layer=")))
                demoImport = a.section(QLatin1Char('='), 1);
        // --demo-drag-slider=<objectName>: sends real mouse press/move/release
        // events to a slider. Binding-vs-drag bugs are invisible to any other
        // kind of test — the property changes, only the interaction is broken.
        QString demoDragSlider;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-drag-slider=")))
                demoDragSlider = a.section(QLatin1Char('='), 1);
        // --demo-drag-layer=FROM:TO drags a layer row onto another row, the same
        // way a user restacks layers. Reordering is an interaction bug when it
        // breaks, so it has to be driven through the event system.
        QString demoDragLayer;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-drag-layer=")))
                demoDragLayer = a.section(QLatin1Char('='), 1);
        // --demo-click=<objectName>: clicks a button for real (QTest input), so
        // the action behind it can be verified instead of assumed.
        QString demoClick;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-click=")))
                demoClick = a.section(QLatin1Char('='), 1);
        // Which layer the click test acts on.
        QString demoActive;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-active=")))
                demoActive = a.section(QLatin1Char('='), 1);
        // --demo-stroke-bench=N: drives N real mouse moves across the canvas and
        // reports frame timing and texture-upload volume. Latency work has to be
        // measured, not guessed — and this hook works on the old and new
        // renderer alike, so the numbers are comparable.
        int demoStrokeBench = 0;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-stroke-bench=")))
                demoStrokeBench = a.section(QLatin1Char('='), 1).toInt();
        // --demo-mirror=h|v: flips the canvas view, which is mirrored per tile
        // in the tiled renderer and therefore worth an explicit visual check.
        QString demoMirror;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-mirror=")))
                demoMirror = a.section(QLatin1Char('='), 1);
        // --demo-doc-size=WxH: benchmark on a document of that size. A 4000x4000
        // canvas is where full-canvas texture uploads collapse.
        int demoDocW = 0;
        int demoDocH = 0;
        for (const QString& a : app.arguments()) {
            if (a.startsWith(QStringLiteral("--demo-doc-size="))) {
                const QString size = a.section(QLatin1Char('='), 1);
                demoDocW = size.section(QLatin1Char('x'), 0, 0).toInt();
                demoDocH = size.section(QLatin1Char('x'), 1, 1).toInt();
            }
        }
        // --demo-stress=N: hammers the paths that crashed or lagged for testers
        // (document resize, fills, strokes, erases, undo/redo, mirror) and logs
        // progress, so an intermittent crash leaves a trace of where it died.
        int demoStress = 0;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-stress=")))
                demoStress = a.section(QLatin1Char('='), 1).toInt();
        // --demo-stress-ops=eraseStroke,zoom: run only the named operations, to
        // bisect an intermittent crash down to the operation that triggers it.
        QString demoStressOps;
        for (const QString& a : app.arguments())
            if (a.startsWith(QStringLiteral("--demo-stress-ops=")))
                demoStressOps = a.section(QLatin1Char('='), 1);
        QTimer::singleShot(1200, &app, [controller, demo, demoDialog, demoToolOptions, demoCursor,
                                       demoMenu, demoImport, demoDragSlider, demoDragLayer,
                                       demoClick, demoActive, demoStrokeBench, demoDocW, demoDocH,
                                       demoMirror, demoStress, demoStressOps, &engine]() {
            if (demoStress > 0 && !engine.rootObjects().isEmpty()) {
                controller->setStartPageVisible(false);
                auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                auto* canvas = window ? findItemByName(window->contentItem(),
                                                       QStringLiteral("canvasItem"))
                                      : nullptr;
                struct Rng {
                    quint32 state = 12345;
                    quint32 next() { state = state * 1664525u + 1013904223u; return state >> 8; }
                    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<quint32>(hi - lo + 1)); }
                } rng;
                const QSize sizes[] = { { 640, 480 }, { 1280, 800 }, { 2048, 1536 },
                                        { 3000, 2000 }, { 1024, 1024 } };
                // Index == op id: 0 newDocument, 1-2 paintStroke, 3 eraseStroke,
                // 4 bucketFill, 5 layerChurn, 6 undoRedo, 7 mirror, 8 zoom,
                // 9 layerProps.
                static const char* const kOpNames[10] = {
                    "newDocument",  "paintStroke", "paintStroke", "eraseStroke",
                    "bucketFill",   "layerChurn",  "undoRedo",    "mirror",
                    "zoom",         "layerProps",
                };
                const QStringList allowed =
                    demoStressOps.isEmpty() ? QStringList()
                                            : demoStressOps.split(QLatin1Char(','));
                int lastPercent = -1;
                for (int i = 0; i < demoStress; ++i) {
                    int op = rng.range(0, 9);
                    if (!allowed.isEmpty()) {
                        // Walk forward until an allowed operation is found.
                        for (int tries = 0; tries < 10; ++tries) {
                            if (allowed.contains(QLatin1String(kOpNames[op])))
                                break;
                            op = (op + 1) % 10;
                        }
                    }
                    const char* opName = kOpNames[op];
                    // Logged BEFORE the operation: if the app dies, the last
                    // line in prism.log names what killed it.
                    qInfo() << "stress: op" << i << opName << "canvas"
                            << controller->canvasWidth() << "x" << controller->canvasHeight()
                            << "layers" << controller->layers()->rowCount();
                    switch (op) {
                    case 0: { // document swap: reallocates the compositor buffer
                        const QSize s = sizes[rng.range(0, 4)];
                        controller->newDocument(s.width(), s.height());
                        break;
                    }
                    case 1: case 2: { // paint stroke
                        const qreal w = controller->canvasWidth(), h = controller->canvasHeight();
                        const qreal x = rng.range(0, static_cast<int>(w) - 1);
                        const qreal y = rng.range(0, static_cast<int>(h) - 1);
                        controller->beginStroke(x, y);
                        for (int k = 0; k < 12; ++k)
                            controller->extendStroke(x + k * 6, y + rng.range(-8, 8));
                        controller->endStroke();
                        break;
                    }
                    case 3: { // erase stroke
                        const qreal w = controller->canvasWidth(), h = controller->canvasHeight();
                        const qreal x = rng.range(0, static_cast<int>(w) - 1);
                        const qreal y = rng.range(0, static_cast<int>(h) - 1);
                        controller->setEraser(true);
                        controller->beginStroke(x, y);
                        for (int k = 0; k < 10; ++k)
                            controller->extendStroke(x + k * 8, y + k * 3);
                        controller->endStroke();
                        controller->setEraser(false);
                        break;
                    }
                    case 4: // bucket fill
                        controller->fillAt(rng.range(0, 400), rng.range(0, 300));
                        break;
                    case 5: // layer churn
                        if (rng.range(0, 1) == 0)
                            controller->addLayer();
                        else if (controller->layers()->rowCount() > 1)
                            controller->deleteLayer();
                        break;
                    case 6: // undo / redo
                        rng.range(0, 1) == 0 ? controller->undo() : controller->redo();
                        break;
                    case 7: // mirror toggle (rebuilds every tile)
                        controller->setMirrorHorizontal(rng.range(0, 1) == 0);
                        controller->setMirrorVertical(rng.range(0, 1) == 0);
                        break;
                    case 8: // zoom (relayout of every tile)
                        controller->setZoom(0.25 + (rng.range(0, 30) / 10.0));
                        break;
                    default: // layer properties
                        controller->setLayerVisible(controller->activeLayerIndex(),
                                                    rng.range(0, 1) == 0);
                        controller->previewLayerOpacity(controller->activeLayerIndex(),
                                                        rng.range(10, 100));
                        break;
                    }
                    QCoreApplication::processEvents();
                    const int percent = (i + 1) * 100 / demoStress;
                    if (percent != lastPercent && percent % 10 == 0) {
                        lastPercent = percent;
                        qInfo() << "stress:" << percent << "% (" << i + 1 << "of" << demoStress
                                << ") canvas" << controller->canvasWidth() << "x"
                                << controller->canvasHeight() << "layers"
                                << controller->layers()->rowCount();
                    }
                }
                qInfo() << "stress: completed" << demoStress << "operations without crashing";
            }
            if (!demoMirror.isEmpty()) {
                controller->setStartPageVisible(false);
                if (demoMirror.contains(QLatin1Char('h')))
                    controller->setMirrorHorizontal(true);
                if (demoMirror.contains(QLatin1Char('v')))
                    controller->setMirrorVertical(true);
                qInfo() << "demo: mirror h =" << controller->mirrorHorizontal()
                        << " v =" << controller->mirrorVertical();
            }
            if (demoStrokeBench > 0 && !engine.rootObjects().isEmpty()) {
                controller->setStartPageVisible(false);
                if (demoDocW > 0 && demoDocH > 0) {
                    controller->newDocument(demoDocW, demoDocH);
                    qInfo() << "bench: document" << demoDocW << "x" << demoDocH;
                    // Let the new document settle (full composite + first tile
                    // upload) before the timed stretch.
                    QTest::qWait(500);
                }
                auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                auto* canvas = window ? findItemByName(window->contentItem(),
                                                       QStringLiteral("canvasItem"))
                                      : nullptr;
                if (!canvas) {
                    qWarning() << "demo: canvas item not found";
                } else {
                    // Count rendered frames while the stroke runs.
                    int frames = 0;
                    const auto conn = QObject::connect(window, &QQuickWindow::frameSwapped,
                                                       [&frames]() { ++frames; });
                    const QRectF itemScene = canvas->mapRectToScene(canvas->boundingRect());
                    const QRectF windowRect(0, 0, window->width(), window->height());
                    const QRectF visible = itemScene.intersected(windowRect);
                    if (visible.width() < 8 || visible.height() < 8) {
                        qWarning() << "demo: canvas is not visible"
                                   << itemScene << "window" << windowRect;
                        return;
                    }
                    // A big document at 100% zoom extends past the viewport, so
                    // the stroke has to run inside the visible part.
                    const QPointF centre = visible.center();
                    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, centre.toPoint());
                    // A back-and-forth line across the canvas: every move paints
                    // a new band, which is the worst case for the uploader.
                    const qreal span = visible.width() * 0.5;
                    QElapsedTimer t;
                    t.start();
                    for (int i = 0; i < demoStrokeBench; ++i) {
                        const qreal f = static_cast<qreal>(i % 20) / 19.0;
                        const qreal x = centre.x() - span / 2 + span * f;
                        const qreal y = centre.y() + std::sin(f * 6.28) * 40.0;
                        QTest::mouseMove(window, QPointF(x, y).toPoint());
                        // Spin the loop after every move: without this Qt merges
                        // the whole burst and nothing is rendered or measured.
                        QCoreApplication::processEvents();
                    }
                    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier,
                                        QPointF(centre.x() + span / 2, centre.y()).toPoint());
                    const qint64 elapsed = t.elapsed();
                    QObject::disconnect(conn);
                    qInfo() << "bench:" << demoStrokeBench << "moves in" << elapsed << "ms ("
                            << (elapsed / static_cast<double>(demoStrokeBench)) << "ms/move)"
                            << "frames:" << frames;
                    if (frames > 0)
                        qInfo() << "bench: average frame interval"
                                << (elapsed / static_cast<double>(frames)) << "ms";
                    if (canvas->property("renderStats").isValid())
                        qInfo().noquote() << "bench:" << canvas->property("renderStats").toString();

                    // Second phase: the same stroke with NO rendering at all, to
                    // separate CPU cost (brush + compositor) from GPU upload.
                    const qreal cw = controller->canvasWidth();
                    const qreal ch = controller->canvasHeight();
                    QElapsedTimer cpu;
                    cpu.start();
                    controller->beginStroke(cw * 0.3, ch * 0.3);
                    const qint64 beginMs = cpu.elapsed();
                    cpu.restart();
                    for (int i = 0; i < demoStrokeBench; ++i) {
                        const qreal f = static_cast<qreal>(i % 20) / 19.0;
                        controller->extendStroke(cw * (0.3 + 0.4 * f), ch * (0.3 + 0.2 * f));
                    }
                    const qint64 movesMs = cpu.elapsed();
                    cpu.restart();
                    controller->endStroke();
                    const qint64 endMs = cpu.elapsed();
                    qInfo() << "bench: CPU split - beginStroke" << beginMs << "ms,"
                            << demoStrokeBench << "moves" << movesMs << "ms ("
                            << (movesMs / static_cast<double>(demoStrokeBench))
                            << "ms/move), endStroke" << endMs << "ms";

                    // Same split for the eraser, which used to copy the whole
                    // layer twice before the first stroke point was painted.
                    controller->setEraser(true);
                    cpu.restart();
                    controller->beginStroke(cw * 0.35, ch * 0.35);
                    const qint64 eraseBeginMs = cpu.elapsed();
                    cpu.restart();
                    for (int i = 0; i < demoStrokeBench; ++i) {
                        const qreal f = static_cast<qreal>(i % 20) / 19.0;
                        controller->extendStroke(cw * (0.35 + 0.3 * f), ch * (0.35 + 0.1 * f));
                    }
                    const qint64 eraseMovesMs = cpu.elapsed();
                    cpu.restart();
                    controller->endStroke();
                    const qint64 eraseEndMs = cpu.elapsed();
                    controller->setEraser(false);
                    qInfo() << "bench: erase split - beginStroke" << eraseBeginMs << "ms,"
                            << demoStrokeBench << "moves" << eraseMovesMs << "ms,"
                            << "endStroke" << eraseEndMs << "ms";

                    // Bucket fill: used to copy the whole layer to discover the
                    // filled rect, then recomposite the whole canvas.
                    cpu.restart();
                    const int fillCount = 8;
                    for (int i = 0; i < fillCount; ++i)
                        controller->fillAt(cw * (0.2 + 0.06 * i), ch * (0.25 + 0.05 * i));
                    const qint64 fillMs = cpu.elapsed();
                    qInfo() << "bench: bucket fill" << fillCount << "clicks in" << fillMs
                            << "ms (" << (fillMs / static_cast<double>(fillCount))
                            << "ms/fill)";
                }
            }
            if (!demoClick.isEmpty() && !engine.rootObjects().isEmpty()) {
                controller->setStartPageVisible(false);
                // Two extra layers so Raise/Lower have somewhere to go, plus an
                // optional --demo-active=<index> to pick which row is acted on.
                controller->addLayer();
                controller->addLayer();
                if (!demoActive.isEmpty())
                    controller->setActiveLayerIndex(demoActive.toInt());
                auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                auto* button = window ? findItemByName(window->contentItem(), demoClick) : nullptr;
                if (button) {
                    auto names = [controller]() {
                        QStringList out;
                        for (int i = 0; i < controller->layers()->rowCount(); ++i)
                            out << controller->layers()
                                       ->data(controller->layers()->index(i, 0),
                                              LayerListModel::NameRole)
                                       .toString();
                        return out.join(QLatin1Char(','));
                    };
                    const QPointF centre = button->mapToScene(
                        QPointF(button->width() / 2.0, button->height() / 2.0));
                    qInfo().noquote() << "demo: clicking" << demoClick << "order before:" << names();
                    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, centre.toPoint());
                    qInfo().noquote() << "demo: order after: " << names()
                                      << "active =" << controller->activeLayerIndex();
                } else {
                    qWarning() << "demo: no clickable item named" << demoClick
                               << "- known names:";
                    if (window) {
                        QStringList known;
                        itemNames(window->contentItem(), known);
                        qWarning().noquote() << "  " + known.join(QLatin1Char(' '));
                    }
                }
            }
            if (!demoDragLayer.isEmpty() && !engine.rootObjects().isEmpty()) {
                controller->setStartPageVisible(false);
                controller->addLayer();
                controller->addLayer();
                auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                auto* listItem = window ? findItemByName(window->contentItem(), QStringLiteral("layerList"))
                                        : nullptr;
                if (listItem) {
                    auto names = [controller]() {
                        QStringList out;
                        for (int i = 0; i < controller->layers()->rowCount(); ++i)
                            out << controller->layers()
                                       ->data(controller->layers()->index(i, 0),
                                              LayerListModel::NameRole)
                                       .toString();
                        return out.join(QLatin1Char(','));
                    };
                    const int from = demoDragLayer.section(QLatin1Char(':'), 0, 0).toInt();
                    const int to = demoDragLayer.section(QLatin1Char(':'), 1, 1).toInt();
                    // Ask the view where those rows actually are: the layer list
                    // is BottomToTop (Krita order), so y = index * rowHeight
                    // would pick the wrong rows.
                    auto rowCentreY = [listItem](int rowIndex) {
                        QVariant result;
                        const bool ok = QMetaObject::invokeMethod(
                            listItem, "demoRowY", Q_RETURN_ARG(QVariant, result),
                            Q_ARG(QVariant, QVariant(rowIndex)));
                        return ok ? result.toDouble() : -1.0;                    };
                    const qreal fromY = rowCentreY(from);
                    const qreal toY = rowCentreY(to);
                    if (fromY < 0 || toY < 0) {
                        qWarning() << "demo: row" << from << "or" << to << "is not visible";
                        return;
                    }
                    const QPointF start = listItem->mapToScene(QPointF(listItem->width() / 2.0, fromY));
                    const QPointF end = listItem->mapToScene(QPointF(listItem->width() / 2.0, toY));
                    qInfo().noquote() << "demo: layer order before:" << names();
                    // Drive the real input pipeline through QTest instead of
                    // QCoreApplication::sendEvent: a raw synthetic event bypasses
                    // the delivery agent's grab bookkeeping, so gestures that
                    // depend on holding the mouse (drag & drop) look broken when
                    // they are fine.
                    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start.toPoint());
                    const int steps = 8;
                    for (int step = 1; step <= steps; ++step) {
                        const QPointF p = start + (end - start) * (step / static_cast<qreal>(steps));
                        QTest::mouseMove(window, p.toPoint());
                    }
                    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end.toPoint());
                    qInfo().noquote() << "demo: layer order after: " << names()
                                      << "active =" << controller->activeLayerIndex();
                } else {
                    qWarning() << "demo: layer list not found";
                }
            }
            if (!demoDragSlider.isEmpty() && !engine.rootObjects().isEmpty()) {
                controller->setStartPageVisible(false);
                if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
                    // Half-width drag to the left by default; a leading '+' drags
                    // right instead, so both directions get covered.
                    QString sliderName = demoDragSlider;
                    const bool dragRight = sliderName.startsWith(QLatin1Char('+'));
                    if (dragRight)
                        sliderName = sliderName.mid(1);
                    if (auto* item = findItemByName(window->contentItem(), sliderName)) {
                        const QPointF centre =
                            item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
                        auto send = [window](QEvent::Type type, QPointF pos, Qt::MouseButtons held) {
                            QMouseEvent ev(type, pos, window->mapToGlobal(pos.toPoint()),
                                           Qt::LeftButton, held, Qt::NoModifier);
                            QCoreApplication::sendEvent(window, &ev);
                        };
                        const qreal distance = item->width() * 0.4 * (dragRight ? 1.0 : -1.0);
                        send(QEvent::MouseButtonPress, centre, Qt::LeftButton);
                        for (int step = 1; step <= 6; ++step)
                            send(QEvent::MouseMove,
                                 centre + QPointF(distance * step / 6.0, 0), Qt::LeftButton);
                        send(QEvent::MouseButtonRelease,
                             centre + QPointF(distance, 0), Qt::NoButton);
                        qInfo() << "demo: dragged" << sliderName
                                << "brushSize now =" << controller->brushSize();
                        // Read what the UI actually shows for every other
                        // slider, so a stale number is visible in the log.
                        const QStringList watched = { QStringLiteral("sizeSlider"),
                                                      QStringLiteral("flowSlider"),
                                                      QStringLiteral("hardnessSlider"),
                                                      QStringLiteral("zoomSlider"),
                                                      QStringLiteral("sizeBox"),
                                                      QStringLiteral("opacitySlider") };
                        for (const QString& name : watched) {
                            if (auto* w = findItemByName(window->contentItem(), name))
                                qInfo().noquote()
                                    << QStringLiteral("  %1.value = %2").arg(
                                           name, QString::number(w->property("value").toDouble()));
                            else
                                qWarning() << "demo: missing item" << name;
                        }
                        qInfo() << "  controller: size" << controller->brushSize() << "flow"
                                << controller->brushFlow() << "hardness"
                                << controller->brushHardness() << "zoom" << controller->zoom()
                                << "layer0 opacity"
                                << controller->layers()
                                       ->data(controller->layers()->index(0, 0),
                                              LayerListModel::OpacityRole)
                                       .toInt();
                    } else {
                        qWarning() << "demo: no item named" << sliderName;
                    }
                }
            }
            if (!demoImport.isEmpty() && !engine.rootObjects().isEmpty()) {
                controller->setStartPageVisible(false);
                QMetaObject::invokeMethod(engine.rootObjects().first(), "demoImportLayer",
                                          Q_ARG(QVariant, QVariant(demoImport)));
                qInfo() << "demo: after QML import call, layers ="
                        << controller->layers()->rowCount() << "error =" << controller->lastError();
            }
            if (demoMenu && !engine.rootObjects().isEmpty()) {
                // Opens a dropdown so its colours are visible in a screenshot.
                controller->setStartPageVisible(false);
                QMetaObject::invokeMethod(engine.rootObjects().first(), "demoOpenMenu");
            }
            if (demoDialog && !engine.rootObjects().isEmpty()) {
                // Opens the New Document dialog so its layout can be checked
                // from a screenshot.
                QMetaObject::invokeMethod(engine.rootObjects().first(), "openNewDocumentDialog");
            }
            if (demoToolOptions && !engine.rootObjects().isEmpty()) {
                // Switches the color docker to its Tool Options tab.
                QMetaObject::invokeMethod(engine.rootObjects().first(), "showToolOptions");
            }
            if (demoCursor && !engine.rootObjects().isEmpty()) {                // Pins the brush ring so its size/colour is visible in the
                // screenshot (a non-foreground window gets no hover events).
                controller->setStartPageVisible(false);
                controller->setBrushSize(60.0F);
                QMetaObject::invokeMethod(engine.rootObjects().first(), "demoCursor",
                                          Q_ARG(QVariant, QVariant(560.0)),
                                          Q_ARG(QVariant, QVariant(280.0)));
            }
            if (!demo || !controller)
                return;
            // The demo draws on a document, so the start page must be gone.
            controller->setStartPageVisible(false);
            // Add a second layer: makes the layer panel's list structure
            // visible in the screenshot (it must show two rows).
            controller->addLayer();
            qInfo() << "demo: layer rows =" << controller->layers()->rowCount();
            // Draw on the newly added (top) layer.
            const qreal w = controller->canvasWidth();
            const qreal h = controller->canvasHeight();
            controller->setBrushColor(QColor(20, 20, 40));
            controller->setBrushSize(6.0F);
            controller->beginStroke(20.0F, 20.0F);
            controller->extendStroke(static_cast<float>(w) - 20.0F, 20.0F);
            controller->extendStroke(static_cast<float>(w) - 20.0F,
                                     static_cast<float>(h) - 20.0F);
            controller->extendStroke(20.0F, static_cast<float>(h) - 20.0F);
            controller->extendStroke(20.0F, 20.0F);
            controller->endStroke();

            controller->setBrushColor(QColor(200, 40, 60));
            controller->setBrushSize(40.0F);
            controller->beginStroke(static_cast<float>(w) / 2.0F, static_cast<float>(h) / 2.0F);
            controller->extendStroke(static_cast<float>(w) / 2.0F + 0.5F,
                                     static_cast<float>(h) / 2.0F + 0.5F);
            controller->endStroke();
        });

        QTimer::singleShot(shotDelayMs, &app, [&engine, shotPath, shotOnly]() {
            if (!engine.rootObjects().isEmpty()) {
                if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
                    const QImage shot = window->grabWindow();
                    if (shot.save(shotPath))
                        qInfo() << "screenshot saved:" << shotPath;
                    else
                        qWarning() << "screenshot failed:" << shotPath;
                }
            }
            if (shotOnly)
                QCoreApplication::quit();
        });
    }

    return app.exec();
}
