#include "installscript_vdf.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace arachnel::core {
namespace {

QString unescapeVdfValue(QString value)
{
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return value;
}

QStringList splitCommandArgs(const QString& command)
{
    QStringList parts;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < command.size(); ++i) {
        const QChar c = command.at(i);
        if (c == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && c.isSpace()) {
            if (!current.isEmpty()) {
                parts.append(current);
                current.clear();
            }
            continue;
        }
        current.append(c);
    }
    if (!current.isEmpty())
        parts.append(current);
    return parts;
}

struct RawRunProcess {
    QString hasRunKey;
    QString process;
    QString command;
    bool is64Bit = false;
    int index = 0;
};

QVector<RawRunProcess> collectRawSteps(const QString& text)
{
    // Match "process 1" "path", "command 1" "args", "HasRunKey", "Is64BitWindows"
    // within each "Run Process" { ... } block.
    QVector<RawRunProcess> steps;
    const QRegularExpression blockRe(
        R"re("Run Process"\s*\{([^}]*(?:\{[^}]*\}[^}]*)*)\})re",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression kvRe(R"re("([^"]+)"\s+"([^"]*)")re",
                                  QRegularExpression::CaseInsensitiveOption);

    auto it = blockRe.globalMatch(text);
    while (it.hasNext()) {
        const QString block = it.next().captured(1);
        RawRunProcess raw;
        auto kv = kvRe.globalMatch(block);
        while (kv.hasNext()) {
            const auto m = kv.next();
            const QString key = m.captured(1).trimmed();
            const QString value = unescapeVdfValue(m.captured(2).trimmed());
            const QString keyLower = key.toLower();
            if (keyLower == QLatin1String("hasrunkey")) {
                raw.hasRunKey = value;
            } else if (keyLower.startsWith(QLatin1String("process"))) {
                raw.process = value;
                const QStringList bits = key.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (bits.size() >= 2)
                    raw.index = bits.at(1).toInt();
            } else if (keyLower.startsWith(QLatin1String("command"))) {
                raw.command = value;
            } else if (keyLower == QLatin1String("is64bitwindows")) {
                raw.is64Bit = value == QLatin1String("1")
                              || value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
            }
        }
        if (!raw.process.isEmpty())
            steps.append(raw);
    }

    // Fallback: some scripts nest without matching our block regex - scan whole file.
    if (steps.isEmpty()) {
        RawRunProcess raw;
        auto kv = kvRe.globalMatch(text);
        while (kv.hasNext()) {
            const auto m = kv.next();
            const QString key = m.captured(1).trimmed();
            const QString value = unescapeVdfValue(m.captured(2).trimmed());
            const QString keyLower = key.toLower();
            if (keyLower == QLatin1String("hasrunkey"))
                raw.hasRunKey = value;
            else if (keyLower.startsWith(QLatin1String("process")) && raw.process.isEmpty()) {
                raw.process = value;
                const QStringList bits = key.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (bits.size() >= 2)
                    raw.index = bits.at(1).toInt();
            } else if (keyLower.startsWith(QLatin1String("command")) && raw.command.isEmpty())
                raw.command = value;
            else if (keyLower == QLatin1String("is64bitwindows"))
                raw.is64Bit = value == QLatin1String("1");
        }
        if (!raw.process.isEmpty())
            steps.append(raw);
    }

    std::sort(steps.begin(), steps.end(),
              [](const RawRunProcess& a, const RawRunProcess& b) { return a.index < b.index; });
    return steps;
}

} // namespace

QVector<RedistInstallStep> parseInstallScriptRunProcess(const QString& vdfText)
{
    QVector<RedistInstallStep> out;
    for (const RawRunProcess& raw : collectRawSteps(vdfText)) {
        RedistInstallStep step;
        step.processPath = raw.process;
        step.arguments = splitCommandArgs(raw.command);
        step.hasRunKey = raw.hasRunKey;
        step.require64BitWindows = raw.is64Bit;
        out.append(step);
    }
    return out;
}

void resolveInstallDirPlaceholders(QVector<RedistInstallStep>* steps, const QString& installDir)
{
    if (!steps || installDir.isEmpty())
        return;
    const QString normalized = QDir::cleanPath(installDir);
    for (RedistInstallStep& step : *steps) {
        step.processPath.replace(QStringLiteral("%INSTALLDIR%"), normalized, Qt::CaseInsensitive);
        step.processPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        step.processPath = QDir::cleanPath(step.processPath);
        for (QString& arg : step.arguments) {
            arg.replace(QStringLiteral("%INSTALLDIR%"), normalized, Qt::CaseInsensitive);
            if (arg.contains(QLatin1Char('\\')) && !arg.startsWith(QLatin1Char('-'))
                && !arg.startsWith(QLatin1Char('/'))) {
                arg.replace(QLatin1Char('\\'), QLatin1Char('/'));
            }
        }
    }
}

bool prefixHasRunKey(const QString& prefixDir, const QString& hasRunKey)
{
    if (prefixDir.isEmpty() || hasRunKey.isEmpty())
        return false;

    // Steam HasRunKey looks like:
    // HKEY_LOCAL_MACHINE\\Software\\Valve\\Steam\\Apps\\228980\\DirectX
    // Wine dumps as [Software\\Valve\\Steam\\Apps\\228980\\DirectX] in system.reg
    QString needle = hasRunKey;
    needle.replace(QStringLiteral("HKEY_LOCAL_MACHINE\\"), QString(), Qt::CaseInsensitive);
    needle.replace(QStringLiteral("HKEY_CURRENT_USER\\"), QString(), Qt::CaseInsensitive);
    needle.replace(QLatin1Char('/'), QLatin1Char('\\'));
    while (needle.startsWith(QLatin1Char('\\')))
        needle.remove(0, 1);

    // Bracket form in Wine registry files
    const QString bracket = QLatin1Char('[') + needle + QLatin1Char(']');
    const QString bracketLower = bracket.toLower();

    for (const QString& regName :
         {QStringLiteral("system.reg"), QStringLiteral("user.reg")}) {
        QFile file(prefixDir + QLatin1Char('/') + regName);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString text = QString::fromUtf8(file.readAll());
        if (text.contains(bracket, Qt::CaseInsensitive) || text.contains(needle, Qt::CaseInsensitive))
            return true;
        if (text.toLower().contains(bracketLower))
            return true;
    }
    return false;
}

} // namespace arachnel::core
