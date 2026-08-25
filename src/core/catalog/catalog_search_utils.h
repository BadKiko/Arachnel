#pragma once

#include "catalog_types.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

/**
 * Normalizes text for search indexing and querying:
 * - Unicode KD normalization + diacritics stripping (ö -> o, é -> e, etc.)
 * - Trademark/copyright symbol stripping (™, ®, ©, ℠, etc.)
 * - Converts punctuation/special characters into spaces
 * - Lowercases and collapses consecutive whitespace
 */
QString normalizeSearchText(const QString& text);

/**
 * Compact representation keeping only alphanumeric chars [a-z0-9а-яё].
 * E.g. "EA SPORTS FC™ 26" -> "easportsfc26", "Spider-Man 2" -> "spiderman2".
 */
QString compactSearchText(const QString& text);

/**
 * Splits normalized text into word tokens.
 */
QStringList tokenizeSearchText(const QString& normalizedText);

/**
 * Generates variants of a single token (e.g. "5" <-> "v", "2" <-> "ii", "spiderman" <-> "spider").
 */
QStringList tokenVariants(const QString& token);

/**
 * Converts Russian keyboard layout typing (йцукен...) into English (qwerty...).
 * E.g. "ас 26" -> "fc 26", "цшесрук 3" -> "witcher 3", "пы 2" -> "cs 2".
 * Returns empty if text contains no Cyrillic characters.
 */
QString convertRussianLayout(const QString& text);

/**
 * Generates acronyms from game title tokens.
 * E.g. ["grand", "theft", "auto", "v"] -> ["gta5", "gtav", "gta"]
 *      ["red", "dead", "redemption", "2"] -> ["rdr2", "rdrii", "rdr"]
 *      ["the", "witcher", "3"] -> ["w3", "tw3", "wiii"]
 *      ["ea", "sports", "fc", "26"] -> ["esfc26", "fc26", "fc"]
 */
QStringList generateTitleAcronyms(const QStringList& tokens);

/**
 * Resolves well-known franchise aliases and Russian translations into search terms.
 * E.g. "gta" -> ["grand theft auto"], "fc" -> ["ea sports fc", "fifa"],
 *      "ведьмак" -> ["witcher"], "сталкер" -> ["stalker", "s t a l k e r"].
 */
QStringList resolveSearchAliases(const QString& normalizedQuery);

/** Pre-parsed query structures reused across the entire catalog search scan. */
struct ParsedSearchQuery {
    QString rawQuery;
    QString cleanQuery;
    QString compactQuery;
    QStringList tokens;
    QVector<QStringList> tokenVariantsList;

    QString layoutCleanQuery;
    QString layoutCompactQuery;
    QStringList layoutTokens;
    QVector<QStringList> layoutTokenVariantsList;

    QStringList aliasExpansions;
    QVector<QString> aliasCompacts;

    bool isNumericOnly = false;
    bool isEmpty = true;

    static ParsedSearchQuery parse(const QString& query);
};

/** Precomputed search data stored per entry in the SoA filter table. */
struct CatalogSearchEntry {
    QString titleClean;
    QString titleCompact;
    QStringList tokens;
    QStringList acronyms;
    QString steamAppId;
    QString entryId;

    static CatalogSearchEntry fromEntry(const CatalogEntry& entry);
};

/**
 * Calculates the relevance score of a catalog entry for the given parsed query.
 * Returns 0 if there is no match. Higher positive values indicate higher relevance.
 */
int scoreCatalogMatch(const CatalogSearchEntry& se, const CatalogEntry& rawEntry,
                      const ParsedSearchQuery& query);

} // namespace arachnel::core
