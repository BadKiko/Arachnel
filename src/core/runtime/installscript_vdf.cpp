#include "installscript_vdf.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

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

namespace {

struct VdfAstNode {
    QString key;
    QString value;
    QVector<VdfAstNode> children;
    bool isObject = false;
};

QVector<QString> tokenizeVdf(const QString& text)
{
    QVector<QString> tokens;
    int i = 0;
    const int len = text.length();
    while (i < len) {
        while (i < len && text.at(i).isSpace())
            ++i;
        if (i >= len)
            break;

        if (text.at(i) == QLatin1Char('/') && i + 1 < len && text.at(i + 1) == QLatin1Char('/')) {
            while (i < len && text.at(i) != QLatin1Char('\n') && text.at(i) != QLatin1Char('\r'))
                ++i;
            continue;
        }

        if (text.at(i) == QLatin1Char('{')) {
            tokens.append(QStringLiteral("{"));
            ++i;
        } else if (text.at(i) == QLatin1Char('}')) {
            tokens.append(QStringLiteral("}"));
            ++i;
        } else if (text.at(i) == QLatin1Char('"')) {
            ++i;
            QString str;
            while (i < len && text.at(i) != QLatin1Char('"')) {
                if (text.at(i) == QLatin1Char('\\') && i + 1 < len) {
                    if (text.at(i + 1) == QLatin1Char('\\') || text.at(i + 1) == QLatin1Char('"')) {
                        str.append(text.at(i + 1));
                        i += 2;
                        continue;
                    }
                }
                str.append(text.at(i));
                ++i;
            }
            if (i < len && text.at(i) == QLatin1Char('"'))
                ++i;
            tokens.append(str);
        } else {
            QString word;
            while (i < len && !text.at(i).isSpace() && text.at(i) != QLatin1Char('{')
                   && text.at(i) != QLatin1Char('}')) {
                word.append(text.at(i));
                ++i;
            }
            tokens.append(word);
        }
    }
    return tokens;
}

QVector<VdfAstNode> parseAstNodes(const QVector<QString>& tokens, int& idx)
{
    QVector<VdfAstNode> nodes;
    while (idx < tokens.size()) {
        const QString token = tokens.at(idx);
        if (token == QLatin1String("}")) {
            ++idx;
            break;
        }
        const QString key = token;
        ++idx;
        if (idx >= tokens.size())
            break;
        const QString next = tokens.at(idx);
        if (next == QLatin1String("{")) {
            ++idx;
            VdfAstNode objNode;
            objNode.key = key;
            objNode.isObject = true;
            objNode.children = parseAstNodes(tokens, idx);
            nodes.append(objNode);
        } else if (next == QLatin1String("}")) {
            continue;
        } else {
            VdfAstNode valNode;
            valNode.key = key;
            valNode.value = next;
            nodes.append(valNode);
            ++idx;
        }
    }
    return nodes;
}

const VdfAstNode* findRegistryNode(const QVector<VdfAstNode>& nodes)
{
    for (const VdfAstNode& node : nodes) {
        if (node.key.compare(QLatin1String("registry"), Qt::CaseInsensitive) == 0)
            return &node;
        if (node.isObject) {
            if (const VdfAstNode* sub = findRegistryNode(node.children))
                return sub;
        }
    }
    return nullptr;
}

} // namespace

QVector<InstallScriptRegistryValue> parseInstallScriptRegistry(const QString& vdfText)
{
    QVector<InstallScriptRegistryValue> out;
    const QVector<QString> tokens = tokenizeVdf(vdfText);
    int idx = 0;
    const QVector<VdfAstNode> rootNodes = parseAstNodes(tokens, idx);
    const VdfAstNode* regNode = findRegistryNode(rootNodes);
    if (!regNode)
        return out;

    for (const VdfAstNode& keyNode : regNode->children) {
        if (!keyNode.isObject)
            continue;

        QString rootKey;
        QString subKey;
        const QString rawKey = keyNode.key;
        const int slashIdx = rawKey.indexOf(QLatin1Char('\\'));
        const int fwdSlashIdx = rawKey.indexOf(QLatin1Char('/'));
        int splitIdx = -1;
        if (slashIdx >= 0 && fwdSlashIdx >= 0)
            splitIdx = qMin(slashIdx, fwdSlashIdx);
        else if (slashIdx >= 0)
            splitIdx = slashIdx;
        else
            splitIdx = fwdSlashIdx;

        if (splitIdx > 0) {
            rootKey = rawKey.left(splitIdx).trimmed();
            subKey = rawKey.mid(splitIdx + 1).trimmed();
        } else {
            rootKey = rawKey.trimmed();
        }

        for (const VdfAstNode& typeNode : keyNode.children) {
            const QString typeLower = typeNode.key.toLower();
            if (typeLower == QLatin1String("string")) {
                for (const VdfAstNode& valNode : typeNode.children) {
                    if (valNode.isObject) {
                        // Nested locale / language blocks (e.g. russian, english)
                        for (const VdfAstNode& subVal : valNode.children) {
                            if (!subVal.isObject) {
                                InstallScriptRegistryValue val;
                                val.rootKey = rootKey;
                                val.subKey = subKey;
                                val.valueName = subVal.key;
                                val.stringValue = subVal.value;
                                val.isDword = false;
                                out.append(val);
                            }
                        }
                    } else {
                        InstallScriptRegistryValue val;
                        val.rootKey = rootKey;
                        val.subKey = subKey;
                        val.valueName = valNode.key;
                        val.stringValue = valNode.value;
                        val.isDword = false;
                        out.append(val);
                    }
                }
            } else if (typeLower == QLatin1String("dword")) {
                for (const VdfAstNode& valNode : typeNode.children) {
                    if (!valNode.isObject) {
                        InstallScriptRegistryValue val;
                        val.rootKey = rootKey;
                        val.subKey = subKey;
                        val.valueName = valNode.key;
                        val.dwordValue = valNode.value.toUInt();
                        val.isDword = true;
                        out.append(val);
                    }
                }
            }
        }
    }

    return out;
}

void resolveRegistryPlaceholders(QVector<InstallScriptRegistryValue>* entries, const QString& installDir)
{
    if (!entries || installDir.isEmpty())
        return;

    QString normInstall = QDir::toNativeSeparators(QDir::cleanPath(installDir));
    if (!normInstall.endsWith(QLatin1Char('\\')) && !normInstall.endsWith(QLatin1Char('/')))
        normInstall.append(QLatin1Char('\\'));

    QString programData = QString::fromLocal8Bit(qgetenv("ProgramData"));
    if (programData.isEmpty())
        programData = QStringLiteral("C:\\ProgramData");
    programData = QDir::toNativeSeparators(programData);

    for (InstallScriptRegistryValue& entry : *entries) {
        if (!entry.isDword) {
            entry.stringValue.replace(QStringLiteral("%INSTALLDIR%\\"), normInstall, Qt::CaseInsensitive);
            entry.stringValue.replace(QStringLiteral("%INSTALLDIR%/"), normInstall, Qt::CaseInsensitive);
            entry.stringValue.replace(QStringLiteral("%INSTALLDIR%"), normInstall, Qt::CaseInsensitive);
            entry.stringValue.replace(QStringLiteral("%PROGRAMDATA%"), programData, Qt::CaseInsensitive);
        }
    }
}

#if defined(Q_OS_WIN)
#include <windows.h>

bool applyRegistryEntriesWindows(const QVector<InstallScriptRegistryValue>& entries)
{
    for (const auto& entry : entries) {
        HKEY hRoot = HKEY_LOCAL_MACHINE;
        REGSAM samDesired = KEY_SET_VALUE | KEY_CREATE_SUB_KEY;

        const QString rootUpper = entry.rootKey.toUpper();
        if (rootUpper.startsWith(QStringLiteral("HKEY_CURRENT_USER"))) {
            hRoot = HKEY_CURRENT_USER;
        } else if (rootUpper.startsWith(QStringLiteral("HKEY_CLASSES_ROOT"))) {
            hRoot = HKEY_CLASSES_ROOT;
        } else if (rootUpper == QStringLiteral("HKEY_LOCAL_MACHINE_WOW64_32")) {
            hRoot = HKEY_LOCAL_MACHINE;
            samDesired |= KEY_WOW64_32KEY;
        } else if (rootUpper == QStringLiteral("HKEY_LOCAL_MACHINE_WOW64_64")) {
            hRoot = HKEY_LOCAL_MACHINE;
            samDesired |= KEY_WOW64_64KEY;
        } else {
            hRoot = HKEY_LOCAL_MACHINE;
        }

        HKEY hKey = nullptr;
        std::wstring subKeyW = entry.subKey.toStdWString();
        std::replace(subKeyW.begin(), subKeyW.end(), L'/', L'\\');

        LSTATUS status = RegCreateKeyExW(hRoot, subKeyW.c_str(), 0, nullptr,
                                         REG_OPTION_NON_VOLATILE, samDesired,
                                         nullptr, &hKey, nullptr);
        if (status == ERROR_SUCCESS && hKey) {
            const wchar_t* valName = entry.valueName.isEmpty()
                                         || entry.valueName == QStringLiteral("(Default)")
                                         ? nullptr
                                         : (const wchar_t*)entry.valueName.utf16();
            if (entry.isDword) {
                DWORD val = entry.dwordValue;
                RegSetValueExW(hKey, valName, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            } else {
                std::wstring valStr = entry.stringValue.toStdWString();
                std::replace(valStr.begin(), valStr.end(), L'/', L'\\');
                RegSetValueExW(hKey, valName, 0, REG_SZ,
                               (const BYTE*)valStr.c_str(),
                               (DWORD)((valStr.size() + 1) * sizeof(wchar_t)));
            }
            RegCloseKey(hKey);
        }
    }
    return true;
}
#endif

bool applyRegistryEntries(const QVector<InstallScriptRegistryValue>& entries, const QString& protonPrefixDir)
{
    if (entries.isEmpty())
        return true;

#if defined(Q_OS_WIN)
    Q_UNUSED(protonPrefixDir);
    return applyRegistryEntriesWindows(entries);
#else
    if (protonPrefixDir.isEmpty() || !QDir(protonPrefixDir).exists())
        return false;

    // Wine registry format: append to system.reg / user.reg
    QString systemRegPath = protonPrefixDir + QStringLiteral("/system.reg");
    QFile sysFile(systemRegPath);
    if (sysFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&sysFile);
        for (const auto& entry : entries) {
            QString cleanSubKey = entry.subKey;
            cleanSubKey.replace(QLatin1Char('/'), QLatin1Char('\\'));
            out << "\n[" << cleanSubKey << "]\n";
            QString vName = entry.valueName;
            if (vName.isEmpty() || vName == QStringLiteral("(Default)")) {
                vName = QStringLiteral("@");
            } else {
                vName = QStringLiteral("\"%1\"").arg(vName);
            }
            if (entry.isDword) {
                out << vName << "=dword:" << QString::number(entry.dwordValue, 16).rightJustified(8, QLatin1Char('0')) << "\n";
            } else {
                QString val = entry.stringValue;
                val.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
                val.replace(QLatin1Char('"'), QStringLiteral("\\\""));
                out << vName << "=\"" << val << "\"\n";
            }
        }
        sysFile.close();
    }
    return true;
#endif
}

bool isUbisoftGame(const QString& gameDir)
{
    if (gameDir.isEmpty() || !QDir(gameDir).exists())
        return false;

    QDirIterator it(gameDir, {QStringLiteral("uplay*.dll"), QStringLiteral("ubisoft*.dll"), QStringLiteral("upc.exe")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString name = it.next().toLower();
        if (name.contains(QStringLiteral("uplay_r1_loader")) || name.contains(QStringLiteral("uplay_r2_loader"))
            || name.contains(QStringLiteral("uplay_r1.dll")) || name.contains(QStringLiteral("uplay_r2.dll"))
            || name.contains(QStringLiteral("uplay.dll")) || name.contains(QStringLiteral("upc.exe")))
            return true;
    }
    return false;
}

bool applyInstallScriptForGame(const QString& gameDir, const QString& protonPrefixDir)
{
    if (gameDir.isEmpty() || !QDir(gameDir).exists())
        return false;

    QVector<InstallScriptRegistryValue> allEntries;

    QDirIterator it(gameDir, {QStringLiteral("installscript*.vdf"), QStringLiteral("runasadmin.vdf"), QStringLiteral("*installscript*.vdf")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString scriptPath = it.next();
        QFile file(scriptPath);
        if (file.open(QIODevice::ReadOnly)) {
            const QString text = QString::fromUtf8(file.readAll());
            file.close();
            QVector<InstallScriptRegistryValue> parsed = parseInstallScriptRegistry(text);
            allEntries.append(parsed);
        }
    }

    // Additional Ubisoft game auto-registration
    if (isUbisoftGame(gameDir)) {
        InstallScriptRegistryValue ubi64;
        ubi64.rootKey = QStringLiteral("HKEY_LOCAL_MACHINE");
        ubi64.subKey = QStringLiteral("SOFTWARE\\Ubisoft\\Launcher\\Installs");
        ubi64.valueName = QStringLiteral("InstallDir");
        ubi64.stringValue = gameDir;
        ubi64.isDword = false;
        allEntries.append(ubi64);

        InstallScriptRegistryValue ubi32;
        ubi32.rootKey = QStringLiteral("HKEY_LOCAL_MACHINE_WOW64_32");
        ubi32.subKey = QStringLiteral("SOFTWARE\\Ubisoft\\Launcher\\Installs");
        ubi32.valueName = QStringLiteral("InstallDir");
        ubi32.stringValue = gameDir;
        ubi32.isDword = false;
        allEntries.append(ubi32);
    }

    // Auto-heal EA Activation64.dll license check bypass if present
    healEaActivation(gameDir);

    if (allEntries.isEmpty())
        return false;

    resolveRegistryPlaceholders(&allEntries, gameDir);
    return applyRegistryEntries(allEntries, protonPrefixDir);
}

bool healEaActivation(const QString& gameDir)
{
    if (gameDir.isEmpty() || !QDir(gameDir).exists())
        return false;

    const QStringList candidates = {
        gameDir + QStringLiteral("/Core/Activation64.dll"),
        gameDir + QStringLiteral("/Activation64.dll"),
        gameDir + QStringLiteral("/Core/Activation.dll"),
        gameDir + QStringLiteral("/Activation.dll")
    };

    bool patchedAny = false;
    for (const QString& dllPath : candidates) {
        if (!QFileInfo::exists(dllPath))
            continue;

        QFile file(dllPath);
        if (!file.open(QIODevice::ReadWrite))
            continue;

        QByteArray data = file.readAll();
        bool modified = false;

        // Pattern 1: test al, al; jnz +0x26; mov dword ptr [r12], 0x186aa
        const QByteArray p1 = QByteArray::fromHex("84c0752641c70424aa860100");
        int idx = data.indexOf(p1);
        if (idx != -1) {
            data[idx + 2] = static_cast<char>(0xEB);
            modified = true;
        }

        // Pattern 2: test eax, eax; jnz +0x0a; mov ebx, 0x186aa
        const QByteArray p2 = QByteArray::fromHex("85c0750abbaa860100");
        idx = data.indexOf(p2);
        if (idx != -1) {
            data[idx + 2] = static_cast<char>(0xEB);
            modified = true;
        }

        // Pattern 3: test edi, edi; jnz +0x07; mov ebx, 0x186ab; jmp +0x05; mov ebx, 0x186aa
        const QByteArray p3 = QByteArray::fromHex("85ff7507bbab860100eb05bbaa860100");
        idx = data.indexOf(p3);
        if (idx != -1) {
            data[idx + 2] = static_cast<char>(0xEB);
            modified = true;
        }

        if (modified) {
            file.seek(0);
            file.write(data);
            file.flush();
            patchedAny = true;
        }
        file.close();
    }

    return patchedAny;
}

} // namespace arachnel::core


