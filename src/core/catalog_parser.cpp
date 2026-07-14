#include "catalog_parser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace arachnel::core {

namespace {

CatalogItemKind itemKindFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("dlc"))
        return CatalogItemKind::Dlc;
    if (normalized == QStringLiteral("addon") || normalized == QStringLiteral("add-on"))
        return CatalogItemKind::Addon;
    return CatalogItemKind::Game;
}

CatalogComponent parseComponent(const QJsonObject& obj, const QString& sourceId,
                               const QString& parentId)
{
    CatalogComponent component;
    const QString title = obj.value(QStringLiteral("title")).toString();
    component.title = title;
    component.id = obj.value(QStringLiteral("id")).toString();
    if (component.id.isEmpty())
        component.id = slugifyCatalogId(title, sourceId + QStringLiteral("-addon"));
    component.fileSize = obj.value(QStringLiteral("fileSize")).toString();
    component.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
    component.kind = itemKindFromString(obj.value(QStringLiteral("kind")).toString());

    const QString delivery = obj.value(QStringLiteral("delivery")).toString().trimmed().toLower();
    if (delivery == QStringLiteral("direct"))
        component.delivery = ComponentDelivery::Direct;
    else
        component.delivery = ComponentDelivery::Magnet;

    component.referer = obj.value(QStringLiteral("referer")).toString();
    component.getfileUrl = obj.value(QStringLiteral("getfileUrl")).toString();
    component.optional = obj.value(QStringLiteral("optional")).toBool(false);

    const QJsonArray uris = obj.value(QStringLiteral("uris")).toArray();
    for (const QJsonValue& uri : uris) {
        const QString value = uri.toString();
        if (value.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive))
            component.magnetUris.append(value);
        else if (value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                 || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
            component.downloadUrl = value;
    }

    if (component.id == parentId)
        component.id += QStringLiteral("-component");

    return component;
}

CatalogEntry parseDownloadObject(const QJsonObject& obj, const QString& sourceId)
{
    CatalogEntry entry;
    const QString title = obj.value(QStringLiteral("title")).toString();
    entry.title = title;
    entry.id = obj.value(QStringLiteral("id")).toString();
    if (entry.id.isEmpty())
        entry.id = slugifyCatalogId(title, sourceId);
    entry.sourceId = sourceId;
    entry.sourcePageUrl = obj.value(QStringLiteral("articleUrl")).toString().trimmed();
    entry.sizeLabel = obj.value(QStringLiteral("fileSize")).toString();
    entry.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
    entry.version = entry.uploadDate.left(10);
    entry.itemKind = itemKindFromString(obj.value(QStringLiteral("kind")).toString());
    entry.parentEntryId = obj.value(QStringLiteral("parentTitle")).toString();
    entry.metadataPending = false;

    const int rawInstallKind = obj.value(QStringLiteral("installKind")).toInt(-1);
    if (rawInstallKind >= static_cast<int>(InstallKind::PortableArchive)
        && rawInstallKind <= static_cast<int>(InstallKind::FixDownload)) {
        entry.installKind = static_cast<InstallKind>(rawInstallKind);
    }

    const QJsonArray uris = obj.value(QStringLiteral("uris")).toArray();
    for (const QJsonValue& uri : uris)
        entry.magnetUris.append(uri.toString());

    const QJsonArray addons = obj.value(QStringLiteral("addons")).toArray();
    entry.addons.reserve(addons.size());
    for (const QJsonValue& addonValue : addons) {
        entry.addons.append(parseComponent(addonValue.toObject(), sourceId, entry.id));
    }

    return entry;
}

QString magnetBtih(const QStringList& uris)
{
    static const QRegularExpression pattern(
        QStringLiteral("btih:([a-fA-F0-9]{40})"), QRegularExpression::CaseInsensitiveOption);
    for (const QString& uri : uris) {
        const QRegularExpressionMatch match = pattern.match(uri);
        if (match.hasMatch())
            return match.captured(1).toLower();
    }
    return {};
}

void mergeAddons(QVector<CatalogComponent>& target, const QVector<CatalogComponent>& extra)
{
    QSet<QString> seen;
    for (const CatalogComponent& addon : target)
        seen.insert(addon.id);

    for (const CatalogComponent& addon : extra) {
        if (seen.contains(addon.id))
            continue;
        target.append(addon);
        seen.insert(addon.id);
    }
}

bool uploadDateIsNewer(const QString& candidate, const QString& existing)
{
    if (existing.isEmpty())
        return !candidate.isEmpty();
    if (candidate.isEmpty())
        return false;
    return candidate > existing;
}

void disambiguateDuplicateIds(QVector<CatalogEntry>& entries)
{
    QHash<QString, int> idCounts;
    for (const CatalogEntry& entry : entries)
        ++idCounts[entry.id];

    for (CatalogEntry& entry : entries) {
        if (idCounts.value(entry.id) <= 1)
            continue;

        const QString btih = magnetBtih(entry.magnetUris);
        if (!btih.isEmpty())
            entry.id += QLatin1Char('-') + btih.left(8);
        else if (!entry.uploadDate.isEmpty())
            entry.id += QLatin1Char('-') + entry.uploadDate.left(10).remove(QLatin1Char('-'));
    }
}

void deduplicateCatalogEntriesImpl(QVector<CatalogEntry>& entries)
{
    QHash<QString, int> keyToIndex;
    QVector<CatalogEntry> unique;
    unique.reserve(entries.size());

    for (CatalogEntry& entry : entries) {
        const QString btih = magnetBtih(entry.magnetUris);
        const QString key = btih.isEmpty() ? entry.id : entry.id + QLatin1Char(':') + btih;

        const auto it = keyToIndex.constFind(key);
        if (it == keyToIndex.constEnd()) {
            keyToIndex.insert(key, unique.size());
            unique.append(std::move(entry));
            continue;
        }

        CatalogEntry& existing = unique[it.value()];
        if (uploadDateIsNewer(entry.uploadDate, existing.uploadDate)) {
            mergeAddons(entry.addons, existing.addons);
            existing = std::move(entry);
        } else {
            mergeAddons(existing.addons, entry.addons);
        }
    }

    entries = std::move(unique);
    disambiguateDuplicateIds(entries);
}

void attachOrphanAddons(QVector<CatalogEntry>& entries)
{
    QHash<QString, int> titleToIndex;
    for (int i = 0; i < entries.size(); ++i)
        titleToIndex.insert(entries.at(i).title, i);

    QVector<int> absorbed;
    for (int i = 0; i < entries.size(); ++i) {
        CatalogEntry& entry = entries[i];
        if (entry.itemKind == CatalogItemKind::Game || entry.parentEntryId.isEmpty())
            continue;

        const int parentIndex = titleToIndex.value(entry.parentEntryId, -1);
        if (parentIndex < 0 || parentIndex == i)
            continue;

        CatalogComponent component;
        component.id = entry.id;
        component.title = entry.title;
        component.fileSize = entry.sizeLabel;
        component.uploadDate = entry.uploadDate;
        component.kind = entry.itemKind;
        component.magnetUris = entry.magnetUris;
        entries[parentIndex].addons.append(component);
        absorbed.append(i);
    }

    for (int i = absorbed.size() - 1; i >= 0; --i)
        entries.removeAt(absorbed.at(i));
}

} // namespace

void deduplicateCatalogEntries(QVector<CatalogEntry>& entries)
{
    deduplicateCatalogEntriesImpl(entries);
}

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return {};

    const QJsonObject root = document.object();
    const QJsonArray downloads = root.value(QStringLiteral("downloads")).toArray();
    QVector<CatalogEntry> entries;
    entries.reserve(downloads.size());

    for (const QJsonValue& value : downloads) {
        if (!value.isObject())
            continue;
        entries.append(parseDownloadObject(value.toObject(), sourceId));
    }

    attachOrphanAddons(entries);

    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [](const CatalogEntry& entry) {
                                     return entry.itemKind != CatalogItemKind::Game;
                                 }),
                  entries.end());

    deduplicateCatalogEntriesImpl(entries);

    return entries;
}

QString catalogFeedValidationError(const QByteArray& payload)
{
    const QByteArray trimmed = payload.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("Пустой ответ сервера");

    if (trimmed.startsWith('<')) {
        const QByteArray lower = trimmed.toLower();
        if (lower.contains("<!doctype") || lower.contains("<html"))
            return QStringLiteral(
                "Сервер вернул HTML вместо JSON (часто Cloudflare). Попробуйте другое зеркало или URL.");
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return QStringLiteral("Некорректный JSON");

    const QJsonObject root = document.object();
    if (!root.contains(QStringLiteral("downloads")))
        return QStringLiteral("Нет массива downloads — это не каталог Hydra");

    const QJsonArray downloads = root.value(QStringLiteral("downloads")).toArray();
    if (downloads.isEmpty())
        return QStringLiteral("Массив downloads пуст");

    const QVector<CatalogEntry> entries = parseCatalogFeed(payload, QStringLiteral("probe"));
    if (entries.isEmpty())
        return QStringLiteral("Не удалось разобрать игры из каталога");

    return {};
}

} // namespace arachnel::core
