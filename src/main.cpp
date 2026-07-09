#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QString>
#include <cstdio>

#ifndef QT_QML_MATERIAL_IMPORT_PATH
#define QT_QML_MATERIAL_IMPORT_PATH ""
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PetWork"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("petwork.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Arachnel"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));

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

    engine.loadFromModule(QStringLiteral("arachnel"), QStringLiteral("Main"));
    return app.exec();
}
