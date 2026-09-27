#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "application/AppController.h"
#include "panels/CanvasView.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Prism"));
    QGuiApplication::setOrganizationName(QStringLiteral("Prism"));

    // Manual registration until each type moves into its own qml module.
    qmlRegisterType<AppController>("Prism.Ui", 1, 0, "AppController");
    qmlRegisterType<CanvasView>("Prism.Ui", 1, 0, "CanvasView");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/Prism/src/ui/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (obj == nullptr && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
