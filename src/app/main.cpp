#include <QCoreApplication>
#include <QDir>
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

#include "core/facade/core_controller.h"
#include "core/settings/settings_store.h"
#include "core/i18n/translation_service.h"
#include "crash_log.h"
#include "settings_identity.h"

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

void wireEngineLogging(QQmlApplicationEngine& engine, QCoreApplication& app)
{
    QObject::connect(
        &engine,
        &QQmlEngine::warnings,
        &app,
        [](const QList<QQmlError>& errors) {
            for (const QQmlError& error : errors)
                arachnel::logQmlWarning(error.url(), error.line(), error.column(),
                                        error.description());
        });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            fprintf(stderr, "Failed to load arachnel QML entry point\n");
            fflush(stderr);
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
}

void applyTranslations(QQmlApplicationEngine& engine, QCoreApplication& app)
{
    auto& core = arachnel::core::CoreController::instance();
    auto& translations = arachnel::core::TranslationService::instance();
    translations.setEngine(&engine);
    translations.applyLanguage(core.settings()->uiLanguage());

    QObject::connect(core.settings(), &arachnel::core::SettingsStore::uiLanguageChanged, &app,
                     [&translations, &core]() {
                         translations.applyLanguage(core.settings()->uiLanguage());
                         core.jobs()->refreshLocalizedText();
                     });
}

} // namespace

int main(int argc, char* argv[])
{
#if !defined(Q_OS_WIN)
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif

    arachnel::configureApplicationIdentity();

    const bool crashDialogMode = arachnel::isCrashDialogMode(argc, argv);

    arachnel::installCrashLogging();
    arachnel::logRunStarted(argc, argv);

    const QIcon windowIcon = []() {
        QIcon icon;
        for (const int size : {16, 24, 32, 48, 64, 128, 256, 512}) {
            icon.addFile(QStringLiteral(":/icons/%1.png").arg(size), QSize(size, size));
        }
        return icon;
    }();
    if (!windowIcon.isNull())
        app.setWindowIcon(windowIcon);

    arachnel::core::registerCoreTypes();

    QQmlApplicationEngine engine;
    configureQmlEngine(engine);
    wireEngineLogging(engine, app);

    if (!crashDialogMode) {
        if (auto* guiApp = qobject_cast<QGuiApplication*>(&app))
            guiApp->setQuitOnLastWindowClosed(true);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, []() {
            arachnel::markApplicationShuttingDown();
            arachnel::core::CoreController::instance().prepareShutdown();
        });
    } else {
        arachnel::core::CoreController::setCrashReporterMode(true);
        if (auto* guiApp = qobject_cast<QGuiApplication*>(&app))
            guiApp->setQuitOnLastWindowClosed(true);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, []() {
            arachnel::markApplicationShuttingDown();
        });
    }

    const int exitCode = [&]() {
        if (crashDialogMode)
            engine.loadFromModule(QStringLiteral("arachnel"), QStringLiteral("CrashReportWindow"));
        else
            engine.loadFromModule(QStringLiteral("arachnel"), QStringLiteral("Main"));

        if (!crashDialogMode)
            applyTranslations(engine, app);
        return app.exec();
    }();

    if (!crashDialogMode) {
        arachnel::markApplicationShuttingDown();
        arachnel::core::CoreController::instance().prepareShutdown();
    }

    arachnel::logRunFinished(exitCode);
    return exitCode;
}
