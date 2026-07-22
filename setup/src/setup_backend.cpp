#include "setup_backend.h"

#include "payload_footer.h"
#include "payload_footer_qt.h"
#include "self_extractor.h"
#include "win_install_registry.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLocale>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtConcurrent>

#if defined(Q_OS_WIN)
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#endif

namespace arachnel::setup {

namespace {

/** User plugin dir used by the app (org+app = Arachnel/Arachnel). Setup has a different
 *  applicationName, so AppDataLocation would be wrong — hardcode the app path. */
QString arachnelUserPluginsDir()
{
#if defined(Q_OS_WIN)
    const QByteArray roaming = qgetenv("APPDATA");
    if (roaming.isEmpty())
        return {};
    return QDir::fromNativeSeparators(QString::fromLocal8Bit(roaming)
                                      + QStringLiteral("/Arachnel/Arachnel/plugins"));
#else
    return QDir::homePath() + QStringLiteral("/.local/share/Arachnel/Arachnel/plugins");
#endif
}

bool copyPluginTree(const QString& srcRoot, const QString& dstRoot)
{
    QDir src(srcRoot);
    if (!src.exists())
        return true;
    if (!QDir().mkpath(dstRoot))
        return false;

    QDirIterator it(srcRoot, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString relative = src.relativeFilePath(it.filePath());
        const QString dest = dstRoot + QLatin1Char('/') + relative;
        if (it.fileInfo().isDir()) {
            if (!QDir().mkpath(dest))
                return false;
            continue;
        }
        if (QFile::exists(dest))
            continue; // keep existing user plugins
        if (!QDir().mkpath(QFileInfo(dest).absolutePath()))
            return false;
        if (!QFile::copy(it.filePath(), dest))
            return false;
    }
    return true;
}

/** Before wiping the install folder on update, rescue any plugins that lived next to the exe. */
void preserveInstallDirPlugins(const QString& installPath)
{
    const QString installPlugins =
        QDir(installPath).absoluteFilePath(QStringLiteral("plugins"));
    if (!QDir(installPlugins).exists())
        return;

    const QString userPlugins = arachnelUserPluginsDir();
    if (userPlugins.isEmpty())
        return;

    QDir().mkpath(userPlugins);
    QDir src(installPlugins);
    const QStringList ids = src.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& id : ids) {
        const QString from = src.absoluteFilePath(id);
        if (!QFileInfo::exists(from + QStringLiteral("/plugin.json")))
            continue;
        const QString to = userPlugins + QLatin1Char('/') + id;
        if (QDir(to).exists())
            continue;
        copyPluginTree(from, to);
    }
}

} // namespace

QString SetupBackend::detectDefaultLanguage()
{
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& language : uiLanguages) {
        if (language.startsWith(QStringLiteral("ru"), Qt::CaseInsensitive))
            return QStringLiteral("ru");
    }
    return QStringLiteral("en");
}

QString SetupBackend::defaultInstallPath()
{
#if defined(Q_OS_WIN)
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFilesX64, KF_FLAG_DEFAULT, nullptr, &path))
        || SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &path))) {
        const QString result = QString::fromWCharArray(path) + QStringLiteral("/Arachnel");
        CoTaskMemFree(path);
        return QDir::fromNativeSeparators(result);
    }
#endif
    return QStringLiteral("C:/Program Files/Arachnel");
}

bool SetupBackend::waitForArachnelExit(int timeoutMs)
{
#if !defined(Q_OS_WIN)
    Q_UNUSED(timeoutMs);
    return true;
#else
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        bool running = false;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (Process32FirstW(snapshot, &entry)) {
                do {
                    const QString name = QString::fromWCharArray(entry.szExeFile);
                    if (name.compare(QStringLiteral("arachnel_app.exe"), Qt::CaseInsensitive) == 0) {
                        running = true;
                        break;
                    }
                } while (Process32NextW(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }
        if (!running)
            return true;
        QThread::msleep(400);
    }
    return false;
#endif
}

SetupBackend::SetupBackend(QObject* parent)
    : QObject(parent)
    , m_language(detectDefaultLanguage())
{
    m_executablePath = qEnvironmentVariable("ARACHNEL_SETUP_CONTAINER");
    if (m_executablePath.isEmpty())
        m_executablePath = QCoreApplication::applicationFilePath();

    const PayloadFooter footer = readPayloadFooter(m_executablePath);
    m_hasPayload = footer.valid && footer.appSize > 0;
    m_appOffset = footer.appOffset;
    m_appSize = footer.appSize;

    const QStringList args = QCoreApplication::arguments();
    m_updateMode = args.contains(QStringLiteral("--update"))
                   || args.contains(QStringLiteral("/update"))
                   || qEnvironmentVariableIntValue("ARACHNEL_SETUP_UPDATE") == 1;

    const QString registeredPath = readWindowsInstallLocation();
    if (!registeredPath.isEmpty())
        m_installPath = QDir::fromNativeSeparators(registeredPath);
    else
        m_installPath = defaultInstallPath();

    m_phase = m_updateMode ? 3 : 0;

    m_installPulseTimer.setInterval(250);
    connect(&m_installPulseTimer, &QTimer::timeout, this, [this]() {
        if (!m_busy || m_progress < 38 || m_progress >= 88)
            return;
        setProgress(m_progress + 1);
    });
}

void SetupBackend::setInstallPath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed == m_installPath)
        return;
    m_installPath = trimmed;
    emit installPathChanged();
}

void SetupBackend::setLanguage(const QString& language)
{
    const QString normalized = language.trimmed().toLower();
    const QString effective = normalized.isEmpty() ? QStringLiteral("en") : normalized;
    if (m_language == effective)
        return;
    m_language = effective;
    emit languageChanged();
}

void SetupBackend::setCreateDesktopShortcut(bool value)
{
    if (m_createDesktopShortcut == value)
        return;
    m_createDesktopShortcut = value;
    emit createDesktopShortcutChanged();
}

void SetupBackend::setCreateStartMenuShortcut(bool value)
{
    if (m_createStartMenuShortcut == value)
        return;
    m_createStartMenuShortcut = value;
    emit createStartMenuShortcutChanged();
}

bool SetupBackend::canInstall() const
{
    return m_hasPayload && !m_installPath.trimmed().isEmpty() && !m_busy;
}

QVariantList SetupBackend::availableLanguages() const
{
    return {
        QVariantMap{{QStringLiteral("code"), QStringLiteral("en")},
                    {QStringLiteral("label"), QStringLiteral("English")}},
        QVariantMap{{QStringLiteral("code"), QStringLiteral("ru")},
                    {QStringLiteral("label"), QStringLiteral("Русский")}},
    };
}

QString SetupBackend::browseInstallFolder()
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(L"Choose install folder");

        if (!m_installPath.isEmpty()) {
            IShellItem* folder = nullptr;
            QString folderPath = QDir::toNativeSeparators(m_installPath);
            if (QDir(folderPath).exists()
                && SUCCEEDED(SHCreateItemFromParsingName(
                    reinterpret_cast<LPCWSTR>(folderPath.utf16()), nullptr,
                    IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();

    if (!path.isEmpty())
        setInstallPath(path);
    return path;
#endif
}

bool SetupBackend::createShortcuts(QString* errorOut)
{
    if (m_installedExe.isEmpty())
        return false;

#if defined(Q_OS_WIN)
    const QString desktop =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString startMenu = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
                              + QStringLiteral("/Arachnel");

    const auto makeShortcut = [&](const QString& linkPath) -> bool {
        const QString script = QStringLiteral(
            "$s = (New-Object -ComObject WScript.Shell).CreateShortcut('%1'); "
            "$s.TargetPath = '%2'; "
            "$s.WorkingDirectory = '%3'; "
            "$s.IconLocation = '%2,0'; "
            "$s.Save()")
                                   .arg(linkPath, m_installedExe,
                                        QFileInfo(m_installedExe).absolutePath());
        QProcess process;
        process.setProgram(QStringLiteral("powershell"));
        process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"),
                              QStringLiteral("Bypass"), QStringLiteral("-Command"), script});
        process.start();
        if (!process.waitForFinished(30000) || process.exitCode() != 0)
            return false;
        return true;
    };

    if (m_createStartMenuShortcut) {
        QDir().mkpath(startMenu);
        makeShortcut(startMenu + QStringLiteral("/Arachnel.lnk"));
    }
    if (m_createDesktopShortcut && !desktop.isEmpty())
        makeShortcut(desktop + QStringLiteral("/Arachnel.lnk"));
    Q_UNUSED(errorOut);
    return true;
#else
    Q_UNUSED(errorOut);
    return true;
#endif
}

bool SetupBackend::installUninstaller(const QString& installPath, QString* errorOut)
{
    const QString source =
        QCoreApplication::applicationDirPath() + QStringLiteral("/uninstall.exe");
    const QString destination = QDir(installPath).absoluteFilePath(QStringLiteral("uninstall.exe"));

    if (!QFile::exists(source)) {
        if (errorOut)
            *errorOut = QStringLiteral("Installer package is missing uninstall.exe");
        return false;
    }

    if (QFile::exists(destination) && !QFile::remove(destination)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not replace uninstall.exe");
        return false;
    }

    if (!QFile::copy(source, destination)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not copy uninstall.exe");
        return false;
    }
    return true;
}

bool SetupBackend::registerUninstall(const QString& installPath, QString* errorOut)
{
    const QString uninstallExe = QDir(installPath).absoluteFilePath(QStringLiteral("uninstall.exe"));
    return registerWindowsUninstall(
        installPath, uninstallExe, QCoreApplication::applicationVersion(), errorOut);
}

void SetupBackend::reportInstallProgress(int progress, const QString& status)
{
    QMetaObject::invokeMethod(
        this,
        [this, progress, status]() {
            setProgress(progress);
            if (!status.isEmpty())
                setStatusText(status);
        },
        Qt::QueuedConnection);
}

void SetupBackend::startInstallPulse()
{
    if (!m_installPulseTimer.isActive())
        m_installPulseTimer.start();
}

void SetupBackend::stopInstallPulse()
{
    m_installPulseTimer.stop();
}

void SetupBackend::beginUpdateIfNeeded()
{
    if (!m_updateMode || m_busy || !canInstall())
        return;
    startInstall();
}

void SetupBackend::finishSuccessfulInstall(const QString& installPath)
{
    m_installedExe = QDir(installPath).absoluteFilePath(QStringLiteral("arachnel_app.exe"));

    setProgress(90);
    setStatusText(m_updateMode
                      ? QCoreApplication::translate("Setup", "Updating uninstaller…")
                      : QCoreApplication::translate("Setup", "Registering uninstaller…"));

    QString postInstallError;
    if (!installUninstaller(installPath, &postInstallError)
        || !registerUninstall(installPath, &postInstallError)) {
        setBusy(false);
        setStatusText(postInstallError);
        emit installFinished(false, postInstallError);
        return;
    }

    setProgress(95);
    setStatusText(m_updateMode ? QCoreApplication::translate("Setup", "Refreshing shortcuts…")
                               : QCoreApplication::translate("Setup", "Creating shortcuts…"));
    createShortcuts(&postInstallError);

    setBusy(false);
    setProgress(100);
    setStatusText(m_updateMode ? QCoreApplication::translate("Setup", "Update complete")
                               : QCoreApplication::translate("Setup", "Installation complete"));
    setPhase(4);
    emit installFinished(true, {});

    if (m_updateMode) {
        launchInstalled();
        QTimer::singleShot(400, qApp, []() { QCoreApplication::quit(); });
    }
}

void SetupBackend::startInstall()
{
    if (!canInstall())
        return;

    setBusy(true);
    setPhase(3);
    setProgress(0);
    setStatusText(m_updateMode ? QCoreApplication::translate("Setup", "Please wait — updating Arachnel…")
                               : QCoreApplication::translate("Setup", "Preparing…"));
    startInstallPulse();

    const QString installPath = m_installPath;
    const QString executablePath = m_executablePath;
    const quint64 appOffset = m_appOffset;
    const quint64 appSize = m_appSize;
    const bool updateMode = m_updateMode;

    auto* watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher, installPath]() {
        const auto result = watcher->result();
        watcher->deleteLater();
        stopInstallPulse();

        if (!result.first) {
            setBusy(false);
            setStatusText(result.second);
            emit installFinished(false, result.second);
            return;
        }

        finishSuccessfulInstall(installPath);
    });

    watcher->setFuture(QtConcurrent::run([this, installPath, executablePath, appOffset, appSize,
                                          updateMode]() {
        QPair<bool, QString> result;

        const auto report = [this](int progress, const QString& status) {
            reportInstallProgress(progress, status);
        };

        if (updateMode) {
            report(2, QCoreApplication::translate("Setup", "Waiting for Arachnel to close…"));
            if (!waitForArachnelExit(90000)) {
                result.second = QCoreApplication::translate(
                    "Setup", "Arachnel is still running. Close it and try again.");
                return result;
            }
        }

        report(5, updateMode ? QCoreApplication::translate("Setup", "Please wait — updating Arachnel…")
                             : QCoreApplication::translate("Setup", "Preparing…"));

        QDir target(installPath);
        if (target.exists()) {
            if (updateMode)
                preserveInstallDirPlugins(installPath);
            report(8, QCoreApplication::translate("Setup", "Clearing install folder…"));
            if (!target.removeRecursively()) {
                result.second = QCoreApplication::translate("Setup",
                                                            "Could not clear existing install folder");
                return result;
            }
        }

        report(12, QCoreApplication::translate("Setup", "Creating install folder…"));
        if (!QDir().mkpath(installPath)) {
            result.second =
                QCoreApplication::translate("Setup", "Could not create install folder");
            return result;
        }

        QString error;
        const auto onExtractProgress = [report](int progress, const QString& status) {
            report(progress, status);
        };
        if (!extractZipSlice(executablePath, appOffset, appSize, installPath, &error,
                             onExtractProgress)) {
            result.second = error;
            return result;
        }

        report(88, QCoreApplication::translate("Setup", "Finalizing…"));
        result.first = true;
        return result;
    }));
}

void SetupBackend::launchInstalled()
{
    if (m_installedExe.isEmpty() || !QFile::exists(m_installedExe))
        return;
    QProcess::startDetached(m_installedExe, {}, QFileInfo(m_installedExe).absolutePath());
}

void SetupBackend::openInstallFolder()
{
    if (m_installPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_installPath));
}

void SetupBackend::setPhase(int phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged();
}

void SetupBackend::setProgress(int progress)
{
    const int clamped = qBound(0, progress, 100);
    if (m_progress == clamped)
        return;
    m_progress = clamped;
    emit progressChanged();
}

void SetupBackend::setStatusText(const QString& text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

void SetupBackend::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

} // namespace arachnel::setup
