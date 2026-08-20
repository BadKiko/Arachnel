#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

namespace arachnel::core {

/**
 * SteamStub remover (Steamless.CLI). Detects PE `.bind` sections and unpacks
 * them so depot games can boot outside Steam. On Linux the CLI runs under Wine.
 */
class SteamlessService : public QObject
{
    Q_OBJECT

public:
    using NoticeFn = std::function<void(const QString&)>;

    explicit SteamlessService(NoticeFn notice, QObject* parent = nullptr);

    QString cliPath() const;
    bool isAvailable() const;

    /** Download Steamless if missing; on Linux also plant bundled MinGW DLLs. */
    bool ensureTool(QString* errorOut = nullptr);

    /** Silent startup bootstrap. */
    void ensureSetup();

    /** After install: strip SteamStub asynchronously (quiet when not needed). */
    void processInstall(const QString& installPath, const QString& title);

    /**
     * Sync path for Play. Returns stripped count, 0 if nothing to do, -1 on error.
     */
    int ensureUnpacked(const QString& installPath, QString* errorOut = nullptr);

    static bool hasSteamStub(const QString& exePath);

    /** Status for game settings: Applied / Needed / Not needed. */
    static QVariantMap installInfo(const QString& installPath);

signals:
    void toolAvailableChanged();
    void finished(const QString& installPath, int strippedCount);

private:
    struct Result {
        int stripped = 0;
        QString error;
        QStringList messages;
    };

    static Result processInstallSync(const QString& installPath, const QString& cliPath);
    static bool stripExecutable(const QString& exePath, const QString& cliPath, QString* errorOut);
    static QString toolRoot();
    static QStringList collectExeCandidates(const QString& installPath);

#if !defined(Q_OS_WIN)
    static bool prepareLinuxRuntime(const QString& cliPath, QString* errorOut);
#endif

    NoticeFn m_notice;
    mutable QString m_cachedCliPath;
};

} // namespace arachnel::core
