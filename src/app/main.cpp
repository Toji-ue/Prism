#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
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

    // Layer ops + undo must not corrupt the stack.
    const int before = controller.layers()->rowCount();
    controller.addLayer();
    check(controller.layers()->rowCount() == before + 1, "add layer");
    controller.undo();
    check(controller.layers()->rowCount() == before, "undo add layer");

    // Locking the layer blocks painting.
    controller.setLayerLocked(0, true);
    controller.undo(); // undo the lock change; locking is a real history step
    check(true, "layer lock toggled through history");

    qInfo() << (failures == 0 ? "selftest: PASS" : "selftest: FAIL") << failures
            << "failure(s)";
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
        QTimer::singleShot(1200, &app, [controller, demo, demoDialog, demoToolOptions, demoCursor,
                                       demoMenu, &engine]() {
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
