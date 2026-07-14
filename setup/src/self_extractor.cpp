#include "self_extractor.h"

#include "payload_footer.h"
#include "payload_footer_qt.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace arachnel::setup {

namespace {

bool runPowerShellExpand(const QString& zipPath, const QString& destinationDir, QString* errorOut)
{
    QDir().mkpath(destinationDir);

    const QString script = QStringLiteral(
        "$ErrorActionPreference = 'Stop'; "
        "Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                               .arg(zipPath, destinationDir);

    QProcess process;
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"),
                          QStringLiteral("Bypass"), QStringLiteral("-Command"), script});
    process.start();
    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not start PowerShell");
        return false;
    }
    if (!process.waitForFinished(600000)) {
        process.kill();
        if (errorOut)
            *errorOut = QStringLiteral("Archive extraction timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorOut = stderrText.isEmpty() ? QStringLiteral("Archive extraction failed")
                                             : stderrText;
        }
        return false;
    }
    return true;
}

bool writeSliceToTempZip(const QString& containerPath, quint64 offset, quint64 size,
                         const QString& tempZipPath, QString* errorOut,
                         ExtractProgressFn onProgress = {})
{
    QFile input(containerPath);
    if (!input.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open installer payload");
        return false;
    }
    if (!input.seek(static_cast<qint64>(offset))) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid payload offset");
        return false;
    }

    QFile output(tempZipPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not create temporary archive");
        return false;
    }

    constexpr qint64 kChunk = 1024 * 1024;
    const qint64 total = static_cast<qint64>(size);
    qint64 remaining = total;
    QByteArray buffer;
    buffer.resize(static_cast<int>(qMin<qint64>(kChunk, remaining)));

    qint64 copied = 0;
    while (remaining > 0) {
        const qint64 toRead = qMin<qint64>(buffer.size(), remaining);
        const qint64 read = input.read(buffer.data(), toRead);
        if (read <= 0) {
            if (errorOut)
                *errorOut = QStringLiteral("Unexpected end of installer payload");
            return false;
        }
        if (output.write(buffer.constData(), read) != read) {
            if (errorOut)
                *errorOut = QStringLiteral("Could not write temporary archive");
            return false;
        }
        remaining -= read;
        copied += read;
        if (onProgress && total > 0) {
            const int sliceProgress = static_cast<int>((copied * 100) / total);
            onProgress(sliceProgress, {});
        }
    }
    return true;
}

QString runtimeCacheDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return base + QStringLiteral("/runtime");
}

bool isRuntimeReady(const QString& dir)
{
    return QFile::exists(dir + QStringLiteral("/arachnel_setup.exe"))
           && QFile::exists(dir + QStringLiteral("/Qt6Core.dll"));
}

} // namespace

bool extractZipSlice(const QString& containerPath, quint64 offset, quint64 size,
                     const QString& destinationDir, QString* errorOut,
                     ExtractProgressFn onProgress)
{
    const QString tempZip =
        QDir::temp().absoluteFilePath(QStringLiteral("arachnel-setup-slice.zip"));
    QFile::remove(tempZip);

    const auto mapCopyProgress = [&](int slicePercent, const QString& status) {
        if (!onProgress)
            return;
        const int overall = 15 + (slicePercent * 20) / 100;
        onProgress(overall, status);
    };

    if (!writeSliceToTempZip(containerPath, offset, size, tempZip, errorOut, mapCopyProgress))
        return false;

    if (onProgress) {
        onProgress(38, QCoreApplication::translate("Setup", "Extracting files…"));
    }

    const bool ok = runPowerShellExpand(tempZip, destinationDir, errorOut);
    QFile::remove(tempZip);
    return ok;
}

bool extractZipArchive(const QString& zipPath, const QString& destinationDir, QString* errorOut)
{
    return runPowerShellExpand(zipPath, destinationDir, errorOut);
}

bool ensureRuntimeBootstrapped(const QString& executablePath, QString* errorOut)
{
    if (qEnvironmentVariableIsSet("ARACHNEL_SETUP_RUNTIME"))
        return true;

    if (QFile::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/Qt6Core.dll")))
        return true;

    const PayloadFooter footer = readPayloadFooter(executablePath);
    if (!footer.valid || footer.runtimeSize == 0) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Installer runtime is missing. Rebuild with setup/pack.ps1 or run windeployqt.");
        }
        return false;
    }

    const QString runtimeDir = runtimeCacheDir();
    if (!isRuntimeReady(runtimeDir)) {
        QDir(runtimeDir).removeRecursively();
        QDir().mkpath(runtimeDir);
        if (!extractZipSlice(executablePath, footer.runtimeOffset, footer.runtimeSize, runtimeDir,
                             errorOut)) {
            return false;
        }
    }

    const QString targetExe = runtimeDir + QStringLiteral("/arachnel_setup.exe");
    if (!QFile::exists(targetExe)) {
        if (errorOut)
            *errorOut = QStringLiteral("Bootstrap runtime is incomplete");
        return false;
    }

    QStringList arguments = QCoreApplication::arguments();
    arguments.removeFirst();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("ARACHNEL_SETUP_RUNTIME"), QStringLiteral("1"));

    QProcess process;
    process.setProgram(targetExe);
    process.setArguments(arguments);
    process.setWorkingDirectory(runtimeDir);
    process.setProcessEnvironment(env);
    const bool started = process.startDetached();
    if (!started && errorOut)
        *errorOut = QStringLiteral("Could not start installer runtime");
    return false;
}

} // namespace arachnel::setup
