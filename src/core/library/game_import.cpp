#include "game_import.h"

#include "install_heuristics.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cstdlib>

namespace arachnel::core {

namespace {

QString normalizeTitle(const QString& title)
{
    QString out;
    out.reserve(title.size());
    for (const QChar ch : title.toLower()) {
        if (ch.isLetterOrNumber())
            out.append(ch);
        else if (!out.isEmpty() && !out.endsWith(QLatin1Char(' ')))
            out.append(QLatin1Char(' '));
    }
    out = out.trimmed();
    if (out.startsWith(QStringLiteral("the ")))
        out = out.mid(4);
    return out;
}

QString digitsOnly(const QString& text)
{
    QString out;
    for (const QChar ch : text) {
        if (ch.isDigit())
            out.append(ch);
        else if (!out.isEmpty())
            break;
    }
    return out;
}

bool usableAppId(const QString& id)
{
    return !id.isEmpty() && id != QLatin1String("0") && id != QLatin1String("480");
}

QString iniValue(const QString& path, const QString& key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString prefix = key + QLatin1Char('=');
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(prefix, Qt::CaseInsensitive))
            return digitsOnly(line.mid(prefix.size()).trimmed());
    }
    return {};
}

QString firstLineDigits(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return digitsOnly(QString::fromUtf8(file.readLine()).trimmed());
}

QString appIdInDir(const QString& dirPath)
{
    const QDir dir(dirPath);
    struct Probe {
        const char* file;
        const char* key; // null: whole file is the id
    };
    static const Probe probes[] = {
        {"OnlineFix.ini", "RealAppId"},
        {"steam_appid.txt", nullptr},
        {"steam_settings/steam_appid.txt", nullptr},
        {"steam_emu.ini", "AppId"},
        {"ColdClientLoader.ini", "AppId"},
    };
    for (const Probe& probe : probes) {
        const QString path = dir.filePath(QLatin1String(probe.file));
        if (!QFileInfo::exists(path))
            continue;
        const QString id = probe.key ? iniValue(path, QLatin1String(probe.key))
                                     : firstLineDigits(path);
        if (usableAppId(id))
            return id;
    }
    return {};
}

} // namespace

QString cleanGameTitleFromFolderName(const QString& folderName)
{
    QString s = folderName;

    // [RePack], (2019), {x64}
    s.remove(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]|\\([^)]*\\)|\\{[^}]*\\}")));

    // Versions and release tags first, while "." / "_" still separate them from the title:
    // "Valheim.v0.218.7-GOG" must lose "v0.218.7", not just "v0".
    static const QRegularExpression tags(
        QStringLiteral(
            "[\\s._-]+(?:v\\s?\\d+(?:[._]\\d+)*[a-z]?|build[\\s._]?\\d+(?:[._]\\d+)*"
            "|\\d+(?:[._]\\d+)+[a-z]?"
            "|repack|fitgirl|dodi|gog|codex|skidrow|plaza|cpy|razor1911|goldberg"
            "|online[\\s._-]?fix|portable|multi\\d*|rus|eng|pc|win(?:dows)?|x64|x86|steamrip"
            "|early[\\s._]?access)(?=[\\s._-]|$)"),
        QRegularExpression::CaseInsensitiveOption);
    QString previous;
    while (previous != s) {
        previous = s;
        s.remove(tags);
    }

    // "Hollow.Knight" / "Elden_Ring": what is left uses dots and underscores as spaces.
    s.replace(QLatin1Char('.'), QLatin1Char(' '));
    s.replace(QLatin1Char('_'), QLatin1Char(' '));

    s.replace(QLatin1Char('-'), QLatin1Char(' '));
    s = s.simplified();
    return s.isEmpty() ? folderName.simplified() : s;
}

QString findImportExecutable(const QString& gameRoot, const QString& title)
{
    const QString best = findGameExecutableInTree(gameRoot, title);
    if (!best.isEmpty())
        return best;

    QString biggest;
    qint64 biggestSize = -1;
    QDirIterator it(gameRoot, {QStringLiteral("*.exe")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (isExcludedGameExecutable(info.fileName()))
            continue;
        if (info.size() > biggestSize) {
            biggestSize = info.size();
            biggest = path;
        }
    }
    return biggest;
}

QString detectSteamAppIdForImport(const QString& gameRoot, const QString& executablePath)
{
    QStringList dirs;
    if (!executablePath.isEmpty())
        dirs.append(QFileInfo(executablePath).absolutePath());
    dirs.append(QDir::cleanPath(gameRoot));
    dirs.removeDuplicates();
    for (const QString& dir : dirs) {
        const QString id = appIdInDir(dir);
        if (!id.isEmpty())
            return id;
    }
    return {};
}

int importTitleScore(const QString& wanted, const QString& candidate)
{
    const QString a = normalizeTitle(wanted);
    const QString b = normalizeTitle(candidate);
    if (a.isEmpty() || b.isEmpty())
        return 0;
    if (a == b)
        return 100;

    // "hollow knight" vs "hollow knight silksong": prefix on a word boundary, penalised by the
    // length gap. Very short titles ("z") would otherwise match half the catalog.
    const QString& shorter = a.size() <= b.size() ? a : b;
    const QString& longer = a.size() <= b.size() ? b : a;
    if (shorter.size() >= 4 && longer.startsWith(shorter + QLatin1Char(' '))) {
        const int gap = int(longer.size()) - int(shorter.size());
        return std::max(55, 85 - gap);
    }

    const QStringList partsA = a.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList partsB = b.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QSet<QString> wa(partsA.begin(), partsA.end());
    const QSet<QString> wb(partsB.begin(), partsB.end());
    const int common = QSet<QString>(wa).intersect(wb).size();
    const int widest = std::max(wa.size(), wb.size());
    if (widest == 0)
        return 0;
    return common * 70 / widest;
}

QString localGameIdFromTitle(const QString& title)
{
    QString slug = normalizeTitle(title);
    slug.replace(QLatin1Char(' '), QLatin1Char('-'));
    if (slug.isEmpty())
        slug = QStringLiteral("game");
    return QStringLiteral("local-") + slug.left(48);
}

QVector<CatalogEntry> pickImportCandidates(const QVector<CatalogEntry>& catalog,
                                           const QString& title, const QString& steamAppId,
                                           int limit)
{
    struct Scored {
        int score;
        int index;
    };
    QVector<Scored> scored;

    for (int i = 0; i < catalog.size(); ++i) {
        const CatalogEntry& entry = catalog.at(i);
        if (entry.itemKind != CatalogItemKind::Game || !entry.parentEntryId.isEmpty())
            continue;

        int score = 0;
        if (!steamAppId.isEmpty()
            && (entry.steamAppId == steamAppId
                || entry.id == QStringLiteral("steam-") + steamAppId)) {
            score = 1000;
        } else {
            score = importTitleScore(title, entry.title);
            if (score < 45)
                continue;
        }
        scored.append({score, i});
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](const Scored& l, const Scored& r) { return l.score > r.score; });

    QVector<CatalogEntry> out;
    QSet<QString> seenIds;
    for (const Scored& s : scored) {
        if (out.size() >= limit)
            break;
        const CatalogEntry& entry = catalog.at(s.index);
        if (seenIds.contains(entry.id))
            continue;
        seenIds.insert(entry.id);
        out.append(entry);
    }
    return out;
}

} // namespace arachnel::core
