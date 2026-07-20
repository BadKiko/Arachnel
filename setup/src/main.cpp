#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QStyleHints>
#include <cstdio>

#include "setup_backend.h"
#include "setup_translation.h"

#ifndef QT_QML_MATERIAL_IMPORT_PATH
#define QT_QML_MATERIAL_IMPORT_PATH ""
#endif

namespace {

void configureQmlEngine(QQmlApplicationEngine& engine)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    engine.addImportPath(appDir + QStringLiteral("/qml"));
    engine.addImportPath(appDir + QStringLiteral("/qml_modules"));

    const QByteArray materialPathEnv = qgetenv("QT_QML_MATERIAL_IMPORT_PATH");
    const QString materialPath = materialPathEnv.isEmpty()
        ? QStringLiteral(QT_QML_MATERIAL_IMPORT_PATH)
        : QString::fromLocal8Bit(materialPathEnv);
    if (!materialPath.isEmpty()) {
        const QString resolved = QDir::isAbsolutePath(materialPath)
            ? materialPath
            : (appDir + QLatin1Char('/') + materialPath);
        engine.addImportPath(resolved);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Arachnel Setup"));
#ifndef ARACHNEL_VERSION
#define ARACHNEL_VERSION "dev"
#endif
    QCoreApplication::setApplicationVersion(QStringLiteral(ARACHNEL_VERSION));

    const QIcon windowIcon = []() {
        QIcon icon;
        for (const int size : {16, 24, 32, 48, 64, 128, 256, 512}) {
            icon.addFile(QStringLiteral(":/icons/%1.png").arg(size), QSize(size, size));
        }
        return icon;
    }();
    if (!windowIcon.isNull())
        app.setWindowIcon(windowIcon);

    arachnel::setup::SetupBackend backend;
    arachnel::setup::SetupTranslation translations;

    QQmlApplicationEngine engine;
    configureQmlEngine(engine);
    translations.setEngine(&engine);
    translations.applyLanguage(backend.language());

    QObject::connect(&backend, &arachnel::setup::SetupBackend::languageChanged, &app,
                     [&backend, &translations]() {
                         translations.applyLanguage(backend.language());
                     });

    engine.rootContext()->setContextProperty(QStringLiteral("Setup"), &backend);

    QObject::connect(
        &engine, &QQmlEngine::warnings, &app,
        [](const QList<QQmlError>& errors) {
            for (const QQmlError& error : errors) {
                fprintf(stderr, "Setup QML warning: %s\n",
                        error.toString().toLocal8Bit().constData());
            }
            fflush(stderr);
        });

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("ArachnelSetup"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
