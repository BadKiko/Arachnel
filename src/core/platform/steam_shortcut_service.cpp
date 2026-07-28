#include "steam_shortcut_service.h"

#include "game_launch_target.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QtEndian>

namespace arachnel::core {

namespace {

quint32 crc32Ieee(const QByteArray& data)
{
    static quint32 table[256];
    static bool ready = false;
    if (!ready) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char b : data)
        crc = table[(crc ^ b) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

quint32 steamShortcutAppId(const QString& exePath, const QString& appName)
{
    // Match common tooling: crc32("\"<exe>\"<AppName>") | 0x80000000
    const QString key = QStringLiteral("\"%1\"%2").arg(exePath, appName);
    return crc32Ieee(key.toUtf8()) | 0x80000000u;
}

QString quoteSteamPath(const QString& path)
{
    QString native = QDir::toNativeSeparators(path);
    if (native.startsWith(QLatin1Char('"')) && native.endsWith(QLatin1Char('"')))
        return native;
    return QLatin1Char('"') + native + QLatin1Char('"');
}

QString unquoteSteamPath(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        value = value.mid(1, value.size() - 2);
    return QDir::fromNativeSeparators(value);
}

struct ShortcutEntry {
    quint32 appId = 0;
    QString appName;
    QString exe;
    QString startDir;
    QString icon;
    QString shortcutPath;
    QString launchOptions;
    QString flatpakAppId;
    QString devkitGameId;
    qint32 isHidden = 0;
    qint32 allowDesktopConfig = 1;
    qint32 allowOverlay = 1;
    qint32 openVr = 0;
    qint32 devkit = 0;
    qint32 devkitOverrideAppId = 0;
    qint32 lastPlayTime = 0;
    QStringList tags;
};

bool readCString(const QByteArray& data, int& pos, QByteArray* out)
{
    const int end = data.indexOf('\0', pos);
    if (end < 0)
        return false;
    if (out)
        *out = data.mid(pos, end - pos);
    pos = end + 1;
    return true;
}

bool parseShortcutObject(const QByteArray& data, int& pos, ShortcutEntry* entry)
{
    while (pos < data.size()) {
        const char type = data.at(pos++);
        if (type == '\x08')
            return true;
        QByteArray key;
        if (!readCString(data, pos, &key))
            return false;
        const QString keyStr = QString::fromUtf8(key);

        if (type == '\x00') {
            // nested (tags)
            QStringList tags;
            while (pos < data.size()) {
                const char nestedType = data.at(pos++);
                if (nestedType == '\x08')
                    break;
                QByteArray nestedKey;
                if (!readCString(data, pos, &nestedKey))
                    return false;
                if (nestedType == '\x01') {
                    QByteArray value;
                    if (!readCString(data, pos, &value))
                        return false;
                    tags.append(QString::fromUtf8(value));
                } else if (nestedType == '\x02') {
                    if (pos + 4 > data.size())
                        return false;
                    pos += 4;
                } else if (nestedType == '\x00') {
                    // unexpected deeper nest — skip until end
                    int depth = 1;
                    while (pos < data.size() && depth > 0) {
                        const char t = data.at(pos++);
                        if (t == '\x08') {
                            --depth;
                            continue;
                        }
                        QByteArray discard;
                        if (!readCString(data, pos, &discard))
                            return false;
                        if (t == '\x01') {
                            if (!readCString(data, pos, &discard))
                                return false;
                        } else if (t == '\x02') {
                            if (pos + 4 > data.size())
                                return false;
                            pos += 4;
                        } else if (t == '\x00') {
                            ++depth;
                        }
                    }
                } else {
                    return false;
                }
            }
            if (keyStr.compare(QStringLiteral("tags"), Qt::CaseInsensitive) == 0)
                entry->tags = tags;
            continue;
        }

        if (type == '\x01') {
            QByteArray value;
            if (!readCString(data, pos, &value))
                return false;
            const QString v = QString::fromUtf8(value);
            if (keyStr == QLatin1String("AppName"))
                entry->appName = v;
            else if (keyStr == QLatin1String("Exe"))
                entry->exe = v;
            else if (keyStr == QLatin1String("StartDir"))
                entry->startDir = v;
            else if (keyStr.compare(QLatin1String("icon"), Qt::CaseInsensitive) == 0)
                entry->icon = v;
            else if (keyStr == QLatin1String("ShortcutPath"))
                entry->shortcutPath = v;
            else if (keyStr == QLatin1String("LaunchOptions"))
                entry->launchOptions = v;
            else if (keyStr == QLatin1String("FlatpakAppID"))
                entry->flatpakAppId = v;
            else if (keyStr == QLatin1String("DevkitGameID"))
                entry->devkitGameId = v;
            continue;
        }

        if (type == '\x02') {
            if (pos + 4 > data.size())
                return false;
            const qint32 v = qFromLittleEndian<qint32>(
                reinterpret_cast<const uchar*>(data.constData() + pos));
            pos += 4;
            if (keyStr.compare(QLatin1String("appid"), Qt::CaseInsensitive) == 0)
                entry->appId = static_cast<quint32>(v);
            else if (keyStr == QLatin1String("IsHidden"))
                entry->isHidden = v;
            else if (keyStr == QLatin1String("AllowDesktopConfig"))
                entry->allowDesktopConfig = v;
            else if (keyStr == QLatin1String("AllowOverlay"))
                entry->allowOverlay = v;
            else if (keyStr == QLatin1String("OpenVR"))
                entry->openVr = v;
            else if (keyStr == QLatin1String("Devkit"))
                entry->devkit = v;
            else if (keyStr == QLatin1String("DevkitOverrideAppID"))
                entry->devkitOverrideAppId = v;
            else if (keyStr == QLatin1String("LastPlayTime"))
                entry->lastPlayTime = v;
            continue;
        }

        return false;
    }
    return false;
}

QVector<ShortcutEntry> parseShortcutsVdf(const QByteArray& data)
{
    QVector<ShortcutEntry> out;
    if (data.size() < 12)
        return out;

    int pos = 0;
    if (data.at(pos) != '\x00')
        return out;
    ++pos;
    QByteArray root;
    if (!readCString(data, pos, &root) || root != "shortcuts")
        return out;

    while (pos < data.size()) {
        const char type = data.at(pos++);
        if (type == '\x08')
            break;
        if (type != '\x00')
            return {};
        QByteArray indexKey;
        if (!readCString(data, pos, &indexKey))
            return {};
        ShortcutEntry entry;
        if (!parseShortcutObject(data, pos, &entry))
            return {};
        out.append(entry);
    }
    return out;
}

void writeCString(QByteArray* out, const QByteArray& s)
{
    out->append(s);
    out->append('\0');
}

void writeStringField(QByteArray* out, const char* key, const QString& value)
{
    out->append('\x01');
    writeCString(out, key);
    writeCString(out, value.toUtf8());
}

void writeIntField(QByteArray* out, const char* key, qint32 value)
{
    out->append('\x02');
    writeCString(out, key);
    char le[4];
    qToLittleEndian(value, le);
    out->append(le, 4);
}

QByteArray serializeShortcutsVdf(const QVector<ShortcutEntry>& entries)
{
    QByteArray out;
    out.append('\x00');
    writeCString(&out, "shortcuts");
    for (int i = 0; i < entries.size(); ++i) {
        const ShortcutEntry& e = entries.at(i);
        out.append('\x00');
        writeCString(&out, QByteArray::number(i));

        writeIntField(&out, "appid", static_cast<qint32>(e.appId));
        writeStringField(&out, "AppName", e.appName);
        writeStringField(&out, "Exe", e.exe);
        writeStringField(&out, "StartDir", e.startDir);
        writeStringField(&out, "icon", e.icon);
        writeStringField(&out, "ShortcutPath", e.shortcutPath);
        writeStringField(&out, "LaunchOptions", e.launchOptions);
        writeIntField(&out, "IsHidden", e.isHidden);
        writeIntField(&out, "AllowDesktopConfig", e.allowDesktopConfig);
        writeIntField(&out, "AllowOverlay", e.allowOverlay);
        writeIntField(&out, "OpenVR", e.openVr);
        writeIntField(&out, "Devkit", e.devkit);
        writeStringField(&out, "DevkitGameID", e.devkitGameId);
        writeIntField(&out, "DevkitOverrideAppID", e.devkitOverrideAppId);
        writeIntField(&out, "LastPlayTime", e.lastPlayTime);
        writeStringField(&out, "FlatpakAppID", e.flatpakAppId);

        out.append('\x00');
        writeCString(&out, "tags");
        for (int t = 0; t < e.tags.size(); ++t) {
            out.append('\x01');
            writeCString(&out, QByteArray::number(t));
            writeCString(&out, e.tags.at(t).toUtf8());
        }
        out.append('\x08'); // end tags
        out.append('\x08'); // end shortcut
    }
    out.append('\x08'); // end shortcuts
    return out;
}

bool downloadUrlToFile(const QString& url, const QString& path)
{
    if (url.isEmpty())
        return false;
    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(20000);
    QNetworkReply* reply = nam.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 400;
    const QByteArray payload = ok ? reply->readAll() : QByteArray{};
    reply->deleteLater();
    if (!ok || payload.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(payload);
    return true;
}

bool copyLocalFileUrl(const QString& fileUrl, const QString& destPath)
{
    QString path = fileUrl;
    if (path.startsWith(QStringLiteral("file:")))
        path = QUrl(path).toLocalFile();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return false;
    QDir().mkpath(QFileInfo(destPath).absolutePath());
    QFile::remove(destPath);
    return QFile::copy(path, destPath);
}

void installGridArtwork(const QString& gridDir, quint32 appId, const QString& steamAppId,
                        const QString& coverFileUrl)
{
    QDir().mkpath(gridDir);
    const QString id = QString::number(appId);
    const QString portrait = gridDir + QLatin1Char('/') + id + QStringLiteral("p.jpg");
    const QString landscape = gridDir + QLatin1Char('/') + id + QStringLiteral(".jpg");
    const QString hero = gridDir + QLatin1Char('/') + id + QStringLiteral("_hero.jpg");

    bool gotPortrait = false;
    bool gotLandscape = false;
    bool gotHero = false;

    if (!steamAppId.isEmpty()) {
        const QString base =
            QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/").arg(steamAppId);
        gotPortrait = downloadUrlToFile(base + QStringLiteral("library_600x900.jpg"), portrait)
            || downloadUrlToFile(base + QStringLiteral("library_capsule.jpg"), portrait);
        gotLandscape = downloadUrlToFile(base + QStringLiteral("header.jpg"), landscape);
        gotHero = downloadUrlToFile(base + QStringLiteral("library_hero.jpg"), hero)
            || downloadUrlToFile(base + QStringLiteral("header.jpg"), hero);
    }

    if (!gotPortrait && !coverFileUrl.isEmpty())
        gotPortrait = copyLocalFileUrl(coverFileUrl, portrait);
    if (!gotLandscape && gotPortrait)
        QFile::copy(portrait, landscape);
    if (!gotHero && gotLandscape)
        QFile::copy(landscape, hero);
    else if (!gotHero && gotPortrait)
        QFile::copy(portrait, hero);
}

QString newestUserdataConfigDir(const QString& steamRoot)
{
    const QString userdata = steamRoot + QStringLiteral("/userdata");
    QDir dir(userdata);
    if (!dir.exists())
        return {};

    qint64 bestMtime = -1;
    QString bestConfig;
    const QFileInfoList users = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& user : users) {
        if (user.fileName() == QLatin1String("0") || user.fileName() == QLatin1String("ac"))
            continue;
        const QString config = user.absoluteFilePath() + QStringLiteral("/config");
        const QString shortcuts = config + QStringLiteral("/shortcuts.vdf");
        const QString local = config + QStringLiteral("/localconfig.vdf");
        qint64 mtime = 0;
        if (QFileInfo::exists(shortcuts))
            mtime = QFileInfo(shortcuts).lastModified().toMSecsSinceEpoch();
        else if (QFileInfo::exists(local))
            mtime = QFileInfo(local).lastModified().toMSecsSinceEpoch();
        else if (QDir(config).exists())
            mtime = QFileInfo(config).lastModified().toMSecsSinceEpoch();
        if (mtime > bestMtime) {
            bestMtime = mtime;
            bestConfig = config;
        }
    }
    return bestConfig;
}

} // namespace

QString findSteamInstallPath()
{
#if defined(Q_OS_WIN)
    const QStringList keys = {
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Valve\\Steam"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Valve\\Steam"),
    };
    for (const QString& key : keys) {
        QSettings settings(key, QSettings::NativeFormat);
        const QString path = settings.value(QStringLiteral("SteamPath")).toString();
        if (!path.isEmpty() && QDir(path).exists())
            return QDir::fromNativeSeparators(path);
    }
#endif
    const QStringList fallbacks = {
        QDir::homePath() + QStringLiteral("/.steam/steam"),
        QDir::homePath() + QStringLiteral("/.steam/root"),
        QDir::homePath() + QStringLiteral("/.steam/debian-installation"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam"),
        QDir::homePath()
            + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
        QDir::homePath() + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"),
    };
    for (const QString& path : fallbacks) {
        if (QDir(path).exists()
            && (QFileInfo::exists(path + QStringLiteral("/steam.sh"))
                || QDir(path + QStringLiteral("/userdata")).exists()
                || QDir(path + QStringLiteral("/steamapps")).exists())) {
            return path;
        }
    }
    return {};
}

QString findSteamShortcutsVdfPath()
{
    const QString steam = findSteamInstallPath();
    if (steam.isEmpty())
        return {};
    const QString config = newestUserdataConfigDir(steam);
    if (config.isEmpty())
        return {};
    return config + QStringLiteral("/shortcuts.vdf");
}

SteamShortcutResult addOrUpdateSteamShortcut(const SteamShortcutRequest& request)
{
    SteamShortcutResult result;
    if (request.target.executable.isEmpty() || !QFileInfo::exists(request.target.executable)) {
        result.error = QStringLiteral("Executable not found");
        return result;
    }

    const QString vdfPath = findSteamShortcutsVdfPath();
    if (vdfPath.isEmpty()) {
        result.error = QStringLiteral("Steam userdata not found");
        return result;
    }

    QVector<ShortcutEntry> entries;
    QFile existing(vdfPath);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("Could not read shortcuts.vdf");
            return result;
        }
        entries = parseShortcutsVdf(existing.readAll());
        existing.close();
        // If parse failed on non-empty file, refuse to overwrite user data.
        if (entries.isEmpty() && QFileInfo(vdfPath).size() > 16) {
            result.error = QStringLiteral("Could not parse shortcuts.vdf");
            return result;
        }
    }

    const QString exeQuoted = quoteSteamPath(request.target.executable);
    const QString startQuoted = quoteSteamPath(request.target.workingDirectory.isEmpty()
                                                   ? QFileInfo(request.target.executable).absolutePath()
                                                   : request.target.workingDirectory);
    const QString appName = request.target.title;
    const quint32 appId = steamShortcutAppId(unquoteSteamPath(exeQuoted), appName);

    int found = -1;
    for (int i = 0; i < entries.size(); ++i) {
        if (unquoteSteamPath(entries.at(i).exe)
                .compare(request.target.executable, Qt::CaseInsensitive)
            == 0) {
            found = i;
            break;
        }
    }

    ShortcutEntry entry = found >= 0 ? entries.at(found) : ShortcutEntry{};
    entry.appId = appId;
    entry.appName = appName;
    entry.exe = exeQuoted;
    entry.startDir = startQuoted;
    entry.icon = QDir::toNativeSeparators(request.target.executable);
    entry.launchOptions = joinLaunchArguments(request.target.arguments);
    entry.allowDesktopConfig = 1;
    entry.allowOverlay = 1;
    if (!entry.tags.contains(QStringLiteral("Arachnel")))
        entry.tags.append(QStringLiteral("Arachnel"));
    if (found >= 0)
        entries[found] = entry;
    else
        entries.append(entry);

    const QString backup = vdfPath + QStringLiteral(".arachnel.bak");
    if (QFileInfo::exists(vdfPath)) {
        QFile::remove(backup);
        QFile::copy(vdfPath, backup);
    }

    QDir().mkpath(QFileInfo(vdfPath).absolutePath());
    QFile out(vdfPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.error = QStringLiteral("Could not write shortcuts.vdf");
        return result;
    }
    out.write(serializeShortcutsVdf(entries));
    out.close();

    const QString gridDir = QFileInfo(vdfPath).absolutePath() + QStringLiteral("/grid");
    installGridArtwork(gridDir, appId, request.steamAppId, request.coverFileUrl);

    result.ok = true;
    result.shortcutsPath = vdfPath;
    result.gridAppId = appId;
    return result;
}

} // namespace arachnel::core
