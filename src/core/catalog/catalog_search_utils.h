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

/** True when the text has CJK / kana / Hangul / Thai characters (scripts written without spaces). */
bool containsCjk(const QString& text);

/**
 * Edit distance (insert / delete / replace / adjacent swap) between two tokens. Returns
 * maxDist + 1 as soon as the distance is known to exceed maxDist, so it is cheap to call on
 * every title of a large catalog.
 */
int boundedEditDistance(QStringView a, QStringView b, int maxDist);

/**
 * Cross-script phonetic key of one token: Cyrillic is transliterated, then c/k/q, ph/f, ck, sh,
 * ch... are folded and vowels dropped. "киберпанк" and "cyberpunk" both give "kbrpnk".
 * Returns an empty string when the result would be too short to mean anything.
 */
QString phoneticSkeleton(QStringView token);

/** One way of writing the query (as typed, keyboard-layout converted, or an alias expansion). */
struct QueryForm {
    QString clean;
    QString compact;
    QStringList tokens;
    QVector<QStringList> variants;   // per token: itself plus roman <-> arabic numeral forms
    QVector<bool> isStop;            // "the", "of"... may be missing from the title
    QVector<bool> isCjk;             // matched as a substring, there are no word boundaries
    QStringList skeletons;           // phoneticSkeleton() per token

    bool isEmpty() const { return tokens.isEmpty(); }
    static QueryForm build(const QString& text);
};

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

    QueryForm main;
    QueryForm layout;
    QVector<QueryForm> aliases;

    /**
     * 0 = whole words only, every query word must be present.
     * 1 = also an unfinished last word ("witch" -> "witcher").
     * 2 = also tolerate typos.
     * 3 = also match by sound across scripts (transliteration).
     * The caller starts at 0 and only widens when too few results came back, so a precise
     * query never gets noise from the looser levels.
     */
    int level = 0;

    bool isNumericOnly = false;
    bool isEmpty = true;

    static ParsedSearchQuery parse(const QString& query);
    ParsedSearchQuery withLevel(int newLevel) const
    {
        ParsedSearchQuery copy = *this;
        copy.level = newLevel;
        return copy;
    }
};

/**
 * Precomputed search data stored per entry in the SoA filter table. Kept small on purpose: there
 * is one per catalog row (~125k), so every extra QString / QStringList is tens of MB. The words
 * of the title are not stored - they are the space-separated parts of `titleClean` and are cut
 * into views while scoring - and the ids are read from the entry itself.
 */
struct CatalogSearchEntry {
    QString titleClean;
    QString acronyms;      ///< space-separated: "gta5 gtav gta"
    quint16 compactLen = 0; ///< length of titleClean without its spaces

    static CatalogSearchEntry fromEntry(const CatalogEntry& entry);
};

/**
 * Calculates the relevance score of a catalog entry for the given parsed query.
 * Returns 0 if there is no match. Higher positive values indicate higher relevance.
 */
int scoreCatalogMatch(const CatalogSearchEntry& se, const CatalogEntry& rawEntry,
                      const ParsedSearchQuery& query);

} // namespace arachnel::core
