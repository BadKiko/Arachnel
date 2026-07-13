#include <QCoreApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QStyleHints>
#include <QString>
#include <cstdio>

#if !defined(Q_OS_WIN)
#include <QApplication>
#else
#include <QGuiApplication>
#endif

#include "core/core_controller.h"
#include "core/settings_store.h"
#include "core/translation_service.h"

#ifndef QT_QML_MATERIAL_IMPORT_PATH
#define QT_QML_MATERIAL_IMPORT_PATH ""
#endif

int main(int argc, char *argv[])
{
#if !defined(Q_OS_WIN)
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    const QIcon windowIcon(QStringLiteral(":/icons/256.png"));
    if (!windowIcon.isNull())
        app.setWindowIcon(windowIcon);

    QCoreApplication::setOrganizationName(QStringLiteral("PetWork"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("petwork.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Arachnel"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));

    arachnel::core::registerCoreTypes();

    QQmlApplicationEngine engine;

    const QByteArray materialPathEnv = qgetenv("QT_QML_MATERIAL_IMPORT_PATH");
    if (!materialPathEnv.isEmpty())
        engine.addImportPath(QString::fromLocal8Bit(materialPathEnv));
    else
        engine.addImportPath(QStringLiteral(QT_QML_MATERIAL_IMPORT_PATH));

    QObject::connect(
        &engine,
        &QQmlEngine::warnings,
        &app,
        [](const QList<QQmlError> &errors) {
            for (const QQmlError &error : errors) {
                fprintf(stderr, "%s\n", qPrintable(error.toString()));
                fflush(stderr);
            }
        });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            fprintf(stderr, "Failed to load arachnel Main.qml\n");
            fflush(stderr);
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, []() {
        arachnel::core::CoreController::instance().prepareShutdown();
    });

    engine.loadFromModule(QStringLiteral("arachnel"), QStringLiteral("Main"));

    auto& core = arachnel::core::CoreController::instance();
    auto& translations = arachnel::core::TranslationService::instance();
    translations.setEngine(&engine);
    translations.applyLanguage(core.settings()->uiLanguage());

    QObject::connect(core.settings(), &arachnel::core::SettingsStore::uiLanguageChanged, &app,
                     [&translations, &core]() {
                         translations.applyLanguage(core.settings()->uiLanguage());
                         core.jobs()->refreshLocalizedText();
                     });

    return app.exec();
}
