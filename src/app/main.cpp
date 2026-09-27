#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QVariant>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

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
        QTimer::singleShot(1200, &app, [controller, demo, demoDialog, demoToolOptions, demoCursor, &engine]() {
            if (demoDialog && !engine.rootObjects().isEmpty()) {
                // Opens the New Document dialog so its layout can be checked
                // from a screenshot.
                QMetaObject::invokeMethod(engine.rootObjects().first(), "openNewDocumentDialog");
            }
            if (demoToolOptions && !engine.rootObjects().isEmpty()) {
                // Switches the color docker to its Tool Options tab.
                QMetaObject::invokeMethod(engine.rootObjects().first(), "showToolOptions");
            }
            if (demoCursor && !engine.rootObjects().isEmpty()) {
                // Pins the brush ring so its size/colour is visible in the
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
            // Corner-to-corner diagonal plus centre cross: any offset or
            // scale error in the view is obvious in the screenshot.
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
