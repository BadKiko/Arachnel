#include "plugin_host.h"

#include "crash_log.h"
#include "catalog_types.h"
#include "file_utils.h"

#include "plugin_api.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace arachnel::core {

#include "plugin_host_helpers.h"

bool PluginHost::extractArachArchive(const QString& archivePath, const QString& destDir,
                                     QString* errorOut)
{
    QDir().mkpath(destDir);

    if (!isZipArchive(archivePath)) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate(
                "Core", "Invalid plugin file. Choose a plugin package (.arach)");
        }
        return false;
    }

    QProcess process;
#if defined(Q_OS_WIN)
    // Prefer ZipFile: Expand-Archive rejects non-.zip extensions even when content is ZIP.
    process.setProgram(QStringLiteral("powershell"));
    const QString escapedArchive = escapePowerShellSingleQuotedLiteral(archivePath);
    const QString escapedDest = escapePowerShellSingleQuotedLiteral(destDir);
    process.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral(
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "[System.IO.Compression.ZipFile]::ExtractToDirectory('%1', '%2')")
            .arg(escapedArchive, escapedDest),
    });
#else
    process.setProgram(QStringLiteral("unzip"));
    process.setArguments({QStringLiteral("-q"), archivePath, QStringLiteral("-d"), destDir});
#endif

    process.start();
    if (!process.waitForStarted(15000)) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Core", "Could not start archive extraction");
        }
        return false;
    }
    // Keep the UI event loop alive while PowerShell/unzip works (large .arach packages).
    qint64 waitedMs = 0;
    while (!process.waitForFinished(100)) {
        waitedMs += 100;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if (waitedMs >= 300000) {
            process.kill();
            if (errorOut)
                *errorOut = QCoreApplication::translate("Core", "Archive extraction timed out");
            return false;
        }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QByteArray stderrBytes = process.readAllStandardError();
            QString stderrText = QString::fromUtf8(stderrBytes).trimmed();
            if (stderrText.isEmpty() || stderrText.contains(QChar(0xFFFD)))
                stderrText = QString::fromLocal8Bit(stderrBytes).trimmed();
            *errorOut = stderrText.isEmpty()
                            ? QCoreApplication::translate("Core",
                                                          "Archive extraction failed (code %1)")
                                  .arg(process.exitCode())
                            : stderrText;
        }
        return false;
    }
    return true;
}

bool PluginHost::findPluginBundleRoot(const QString& extractedDir, QString* bundleRootOut)
{
    const QString directManifest = extractedDir + QStringLiteral("/plugin.json");
    if (QFile::exists(directManifest)) {
        *bundleRootOut = extractedDir;
        return true;
    }

    QDir dir(extractedDir);
    const QStringList children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (children.size() == 1) {
        const QString nested = dir.absoluteFilePath(children.constFirst());
        if (QFile::exists(nested + QStringLiteral("/plugin.json"))) {
            *bundleRootOut = nested;
            return true;
        }
    }

    QDirIterator it(extractedDir, {QStringLiteral("plugin.json")}, QDir::Files,
                    QDirIterator::Subdirectories);
    if (it.hasNext()) {
        *bundleRootOut = QFileInfo(it.next()).absolutePath();
        return true;
    }
    return false;
}

bool PluginHost::installFromArach(const QString& archivePath)
{
    m_lastError.clear();

    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()) {
        m_lastError = QCoreApplication::translate("Core", "File not found: %1").arg(archivePath);
        return false;
    }
    if (archiveInfo.suffix().compare(QStringLiteral("arach"), Qt::CaseInsensitive) != 0) {
        m_lastError = QCoreApplication::translate("Core", "Only .arach packages are supported");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        m_lastError = QCoreApplication::translate("Core", "Failed to create temporary folder");
        return false;
    }

    QString extractError;
    if (!extractArachArchive(archiveInfo.absoluteFilePath(), tempDir.path(), &extractError)) {
        m_lastError = extractError;
        return false;
    }

    QString bundleRoot;
    if (!findPluginBundleRoot(tempDir.path(), &bundleRoot)) {
        m_lastError = QCoreApplication::translate("Core", "Archive has no plugin.json");
        return false;
    }

    QFile manifestFile(bundleRoot + QStringLiteral("/plugin.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        m_lastError = QCoreApplication::translate("Core", "Failed to read plugin.json");
        return false;
    }

    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QString id = manifest.value(QStringLiteral("id")).toString();
    const QString libraryBase = manifest.value(QStringLiteral("library")).toString();
    if (id.isEmpty() || libraryBase.isEmpty()) {
        m_lastError = QCoreApplication::translate("Core", "Invalid plugin.json");
        return false;
    }

    if (resolveLibraryFile(bundleRoot, libraryBase).isEmpty()) {
        m_lastError = QCoreApplication::translate("Core", "Package is missing library %1").arg(
            platformLibraryName(libraryBase));
        return false;
    }

    const QString pluginsRoot = writablePluginsDir();
    if (pluginsRoot.isEmpty()) {
        m_lastError = QCoreApplication::translate("Core", "Failed to create plugin folder");
        return false;
    }

    const QString targetRoot = pluginsRoot + QLatin1Char('/') + id;
    const QString stagingRoot = targetRoot + QStringLiteral(".staging");
    const QString backupRoot = targetRoot + QStringLiteral(".bak");

    // Build into staging first so a failed install never deletes the working plugin.
    removePathRecursive(stagingRoot);
    removePathRecursive(backupRoot);
    if (!QDir().mkpath(stagingRoot)) {
        m_lastError = QCoreApplication::translate("Core", "Failed to create plugin folder");
        return false;
    }

    auto copyTree = [](const QString& srcRoot, const QString& dstRoot, QString* errorOut) -> bool {
        QDir bundleDir(srcRoot);
        const QStringList files = bundleDir.entryList(QDir::Files);
        for (const QString& fileName : files) {
            if (!QFile::copy(bundleDir.absoluteFilePath(fileName),
                             dstRoot + QLatin1Char('/') + fileName)) {
                if (errorOut)
                    *errorOut = QCoreApplication::translate("Core", "Failed to copy %1").arg(fileName);
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }

        const QStringList subdirs = bundleDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subdir : subdirs) {
            const QString srcSubdir = bundleDir.absoluteFilePath(subdir);
            const QString dstSubdir = dstRoot + QLatin1Char('/') + subdir;
            QDirIterator it(srcSubdir, QDir::Files, QDirIterator::Subdirectories);
            int copied = 0;
            while (it.hasNext()) {
                it.next();
                const QString relativePath = QDir(srcSubdir).relativeFilePath(it.filePath());
                const QString destination = dstSubdir + QLatin1Char('/') + relativePath;
                if (!QDir().mkpath(QFileInfo(destination).path())) {
                    if (errorOut)
                        *errorOut =
                            QCoreApplication::translate("Core", "Failed to create plugin folder");
                    return false;
                }
                if (!QFile::copy(it.filePath(), destination)) {
                    if (errorOut)
                        *errorOut =
                            QCoreApplication::translate("Core", "Failed to copy %1").arg(relativePath);
                    return false;
                }
                if ((++copied % 8) == 0)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
        }
        return true;
    };

    QString copyError;
    if (!copyTree(bundleRoot, stagingRoot, &copyError)) {
        m_lastError = copyError;
        removePathRecursive(stagingRoot);
        return false;
    }

    // Unlock loaded plugin DLLs before renaming folders on Windows.
    unloadAll();

    if (QDir(targetRoot).exists()) {
        if (!QDir().rename(targetRoot, backupRoot)) {
            // Fallback when rename across volumes / locks fails.
            if (!copyPathRecursive(targetRoot, backupRoot, &copyError)
                || !removePathRecursive(targetRoot, &copyError)) {
                m_lastError = QCoreApplication::translate("Core", "Failed to replace existing plugin");
                if (!copyError.isEmpty())
                    m_lastError += QStringLiteral(": ") + copyError;
                removePathRecursive(stagingRoot);
                scan();
                return false;
            }
        }
    }

    if (!QDir().rename(stagingRoot, targetRoot)) {
        if (!copyPathRecursive(stagingRoot, targetRoot, &copyError)) {
            m_lastError = QCoreApplication::translate("Core", "Failed to install plugin files");
            if (!copyError.isEmpty())
                m_lastError += QStringLiteral(": ") + copyError;
            removePathRecursive(stagingRoot);
            if (QDir(backupRoot).exists())
                QDir().rename(backupRoot, targetRoot);
            scan();
            return false;
        }
        removePathRecursive(stagingRoot);
    }

    removePathRecursive(backupRoot);

    scan();
    if (!hasPlugin(id)) {
        m_lastError = QCoreApplication::translate(
            "Core",
            "Plugin files were copied but the library failed to load. Rebuild the plugin for "
            "your Arachnel version and platform (MSVC/MinGW), then reinstall.");
        if (!g_lastPluginLoadError.isEmpty())
            m_lastError += QStringLiteral(" (") + g_lastPluginLoadError + QLatin1Char(')');
        return false;
    }

    return true;
}

bool PluginHost::uninstallPlugin(const QString& pluginId)
{
    m_lastError.clear();

    const QString id = pluginId.trimmed();
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\'))
        || id == QLatin1String(".") || id == QLatin1String("..")) {
        m_lastError = QCoreApplication::translate("Core", "Invalid plugin id");
        return false;
    }

    const QString targetRoot = writablePluginsDir() + QLatin1Char('/') + id;
    QDir targetDir(targetRoot);

    // Drop loaded libraries before deleting files (DLL/.so stay locked otherwise).
    unloadAll();

    bool removedAny = false;
    QStringList roots = pluginSearchRoots();
    if (!roots.contains(writablePluginsDir(), Qt::CaseInsensitive))
        roots.prepend(writablePluginsDir());

    for (const QString& root : roots) {
        QDir dir(root + QLatin1Char('/') + id);
        if (!dir.exists())
            continue;
        if (!dir.removeRecursively()) {
            m_lastError = QCoreApplication::translate("Core", "Could not delete plugin files");
            scan();
            return false;
        }
        removedAny = true;
    }

    if (!removedAny) {
        m_lastError = QCoreApplication::translate("Core", "Plugin is not installed");
        scan();
        return false;
    }

    scan();
    return true;
}

} // namespace arachnel::core
