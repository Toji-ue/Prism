#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
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

    // Layer ops + undo must not corrupt the stack.
    const int before = controller.layers()->rowCount();
    controller.addLayer();
    check(controller.layers()->rowCount() == before + 1, "add layer");
    controller.undo();
    check(controller.layers()->rowCount() == before, "undo add layer");

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
    // Brush preset thumbnails are rendered by the real brush engine.
    engine.addImageProvider(QStringLiteral("preset"), new PresetImageProvider);

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
    for (const QString& arg : app.arguments()) {
        if (arg.startsWith(QStringLiteral("--screenshot=")))
            shotPath = arg.section('=', 1);
        else if (arg == QStringLiteral("--screenshot-only"))
            shotOnly = true;
    }
    if (!shotPath.isEmpty()) {
        QTimer::singleShot(2500, &app, [&engine, shotPath, shotOnly]() {
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
