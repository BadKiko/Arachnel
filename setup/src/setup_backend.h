#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

namespace arachnel::setup {

class SetupBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString installPath READ installPath WRITE setInstallPath NOTIFY installPathChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool createDesktopShortcut READ createDesktopShortcut WRITE setCreateDesktopShortcut
                   NOTIFY createDesktopShortcutChanged)
    Q_PROPERTY(bool createStartMenuShortcut READ createStartMenuShortcut WRITE setCreateStartMenuShortcut
                   NOTIFY createStartMenuShortcutChanged)
    Q_PROPERTY(int phase READ phase WRITE setPhase NOTIFY phaseChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool hasPayload READ hasPayload CONSTANT)
    Q_PROPERTY(bool canInstall READ canInstall NOTIFY installPathChanged)
    Q_PROPERTY(bool updateMode READ updateMode CONSTANT)

public:
    explicit SetupBackend(QObject* parent = nullptr);

    QString installPath() const { return m_installPath; }
    void setInstallPath(const QString& path);

    QString language() const { return m_language; }
    void setLanguage(const QString& language);

    bool createDesktopShortcut() const { return m_createDesktopShortcut; }
    void setCreateDesktopShortcut(bool value);

    bool createStartMenuShortcut() const { return m_createStartMenuShortcut; }
    void setCreateStartMenuShortcut(bool value);

    int phase() const { return m_phase; }
    void setPhase(int phase);
    int progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }
    bool busy() const { return m_busy; }
    bool hasPayload() const { return m_hasPayload; }
    bool canInstall() const;
    bool updateMode() const { return m_updateMode; }

    Q_INVOKABLE QString browseInstallFolder();
    Q_INVOKABLE QVariantList availableLanguages() const;
    Q_INVOKABLE void startInstall();
    Q_INVOKABLE void launchInstalled();
    Q_INVOKABLE void openInstallFolder();
    Q_INVOKABLE void beginUpdateIfNeeded();

signals:
    void installPathChanged();
    void languageChanged();
    void createDesktopShortcutChanged();
    void createStartMenuShortcutChanged();
    void phaseChanged();
    void progressChanged();
    void statusTextChanged();
    void busyChanged();
    void installFinished(bool success, const QString& error);

private:
    void setProgress(int progress);
    void setStatusText(const QString& text);
    void setBusy(bool busy);
    void reportInstallProgress(int progress, const QString& status = QString());
    void startInstallPulse();
    void stopInstallPulse();
    bool createShortcuts(QString* errorOut);
    bool installUninstaller(const QString& installPath, QString* errorOut);
    bool registerUninstall(const QString& installPath, QString* errorOut);
    static QString detectDefaultLanguage();
    static QString defaultInstallPath();
    static bool waitForArachnelExit(int timeoutMs);
    void finishSuccessfulInstall(const QString& installPath);

    QString m_executablePath;
    QString m_installPath;
    QString m_installedExe;
    QString m_language;
    bool m_createDesktopShortcut = true;
    bool m_createStartMenuShortcut = true;
    int m_phase = 0;
    int m_progress = 0;
    QString m_statusText;
    bool m_busy = false;
    bool m_hasPayload = false;
    bool m_updateMode = false;
    quint64 m_appOffset = 0;
    quint64 m_appSize = 0;
    QTimer m_installPulseTimer;
};

} // namespace arachnel::setup
