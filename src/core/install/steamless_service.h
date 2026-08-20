#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace arachnel::core {

/**
 * Applies Steamless (SteamStub DRM remover) to freshly installed games.
 *
 * SteamStub is the Steamworks SDK DRM that wraps a game's main executable in a
 * `.bind` section. Steamless unpacks that wrapper so the game can run without
 * Steam checking the executable. This service detects SteamStub-protected .exe
 * files after an install finishes and rewrites them with Steamless' unpacked
 * output, keeping the original as `<exe>.steamstub.bak`.
 */
class SteamlessService : public QObject
{
    Q_OBJECT

public:
    using NoticeFn = std::function<void(const QString&)>;

    explicit SteamlessService(NoticeFn notice, QObject* parent = nullptr);

    /** Resolve the Steamless CLI path (bundled location or ARACHNEL_STEAMLESS_PATH). */
    QString cliPath() const;
    bool isAvailable() const;

    /**
     * Download and extract the latest Steamless release when the CLI is missing.
     * Runs on the calling (GUI) thread with an event-loop-friendly wait.
     */
    bool ensureTool(QString* errorOut = nullptr);

    /**
     * One-time setup: check for Steamless and download/configure it only when
     * it is missing (or was deleted). No-op when already installed. Reports
     * success/skip through the notice callback.
     */
    void ensureSetup();

    /** Scan installPath for SteamStub-protected executables and strip them (async). */
    void processInstall(const QString& installPath, const QString& title);

    /** True when the PE file has a SteamStub `.bind` section. */
    static bool hasSteamStub(const QString& exePath);

signals:
    void toolAvailableChanged();
    void finished(const QString& installPath, int strippedCount);

private:
    struct Result {
        int stripped = 0;
        int alreadyApplied = 0;
        int notProtected = 0;
        QString error;
        QStringList messages;
    };

    static Result processInstallSync(const QString& installPath, const QString& cliPath);
    static bool stripExecutable(const QString& exePath, const QString& cliPath, QString* errorOut);
    static QString toolRoot();

#if !defined(Q_OS_WIN)
    /**
     * Make the CLI actually runnable under Wine Mono on Linux: mirror the
     * Plugins/*.dll files next to the CLI (mono resolves Steamless.API from
     * the exe's directory) and provision the 32-bit MinGW runtime DLLs that
     * Fedora-family wine-mono builds link against. Returns false with a
     * human-readable error when the runtime cannot be made available.
     */
    static bool prepareLinuxRuntime(const QString& cliPath, QString* errorOut);
#endif

    NoticeFn m_notice;
    mutable QString m_cachedCliPath;
};

} // namespace arachnel::core
