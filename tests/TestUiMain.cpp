#include <QDir>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir settings(QDir::tempPath() + "/prism-ui-test-XXXXXX");
    if (!settings.isValid())
        return 2;
    qputenv("PRISM_SETTINGS_DIR", settings.path().toUtf8());
    qputenv("PRISM_RECOVERY_DIR", (settings.path() + "/recovery").toUtf8());
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("PrismTests");
    QGuiApplication::setApplicationName("UiTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
