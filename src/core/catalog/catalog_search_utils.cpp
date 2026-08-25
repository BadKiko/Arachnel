#include "catalog_search_utils.h"

#include <QChar>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace arachnel::core {

namespace {

inline bool isSymbolOrPunctuation(const QChar& c)
{
    const ushort u = c.unicode();
    // Common trademark / copyright / decorative symbols
    if (u == 0x2122 || u == 0x00AE || u == 0x00A9 || u == 0x2120 || u == 0x2117 || u == 0x2116
        || u == 0x00B0 || u == 0x00B7 || u == 0x2022 || u == 0x2605 || u == 0x2606 || u == 0x2665
        || u == 0x2666 || u == 0x2018 || u == 0x2019 || u == 0x201C || u == 0x201D
        || u == 0x00AB || u == 0x00BB || u == 0x2013 || u == 0x2014 || u == 0x2212) {
        return true;
    }
    // ASCII punctuation
    if ((u >= 33 && u <= 47) || (u >= 58 && u <= 64) || (u >= 91 && u <= 96)
        || (u >= 123 && u <= 126)) {
        return true;
    }
    return false;
}

inline bool isSearchAlphanumeric(const QChar& c)
{
    return c.isLetterOrNumber();
}

const QHash<QString, QString>& romanToArabicMap()
{
    static const QHash<QString, QString> s_map = {
        {QStringLiteral("i"), QStringLiteral("1")},
        {QStringLiteral("ii"), QStringLiteral("2")},
        {QStringLiteral("iii"), QStringLiteral("3")},
        {QStringLiteral("iv"), QStringLiteral("4")},
        {QStringLiteral("v"), QStringLiteral("5")},
        {QStringLiteral("vi"), QStringLiteral("6")},
        {QStringLiteral("vii"), QStringLiteral("7")},
        {QStringLiteral("viii"), QStringLiteral("8")},
        {QStringLiteral("ix"), QStringLiteral("9")},
        {QStringLiteral("x"), QStringLiteral("10")},
    };
    return s_map;
}

const QHash<QString, QString>& arabicToRomanMap()
{
    static const QHash<QString, QString> s_map = {
        {QStringLiteral("1"), QStringLiteral("i")},
        {QStringLiteral("2"), QStringLiteral("ii")},
        {QStringLiteral("3"), QStringLiteral("iii")},
        {QStringLiteral("4"), QStringLiteral("iv")},
        {QStringLiteral("5"), QStringLiteral("v")},
        {QStringLiteral("6"), QStringLiteral("vi")},
        {QStringLiteral("7"), QStringLiteral("vii")},
        {QStringLiteral("8"), QStringLiteral("viii")},
        {QStringLiteral("9"), QStringLiteral("ix")},
        {QStringLiteral("10"), QStringLiteral("x")},
    };
    return s_map;
}

bool isStopWord(const QString& word)
{
    static const QSet<QString> s_stopWords = {
        QStringLiteral("the"),  QStringLiteral("a"),    QStringLiteral("an"),
        QStringLiteral("of"),   QStringLiteral("for"),  QStringLiteral("and"),
        QStringLiteral("to"),   QStringLiteral("in"),   QStringLiteral("on"),
        QStringLiteral("at"),   QStringLiteral("by"),   QStringLiteral("with"),
        QStringLiteral("from"), QStringLiteral("edition"), QStringLiteral("pc"),
    };
    return s_stopWords.contains(word);
}

} // namespace

QString normalizeSearchText(const QString& text)
{
    if (text.isEmpty())
        return {};

    // 1. Replace symbols and punctuation with space BEFORE KD decomposition,
    // so characters like ™ (U+2122) don't decompose into "tm", ® into "(r)", etc.
    QString pre;
    pre.reserve(text.size());
    for (const QChar& c : text) {
        if (isSymbolOrPunctuation(c)) {
            pre.append(QLatin1Char(' '));
        } else {
            pre.append(c);
        }
    }

    // 2. Unicode KD normalization separates accented characters into base char + combining mark
    const QString decomp = pre.normalized(QString::NormalizationForm_KD);

    QString out;
    out.reserve(decomp.size());

    bool lastWasSpace = true;
    for (const QChar& c : decomp) {
        // Strip combining marks (accents, umlauts, tildes, etc.)
        const QChar::Category cat = c.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining
            || cat == QChar::Mark_Enclosing) {
            continue;
        }

        if (isSymbolOrPunctuation(c) || c.isSpace()) {
            if (!lastWasSpace) {
                out.append(QLatin1Char(' '));
                lastWasSpace = true;
            }
        } else if (isSearchAlphanumeric(c)) {
            out.append(c.toLower());
            lastWasSpace = false;
        } else {
            if (!lastWasSpace) {
                out.append(QLatin1Char(' '));
                lastWasSpace = true;
            }
        }
    }

    while (out.endsWith(QLatin1Char(' ')))
        out.chop(1);

    return out;
}

QString compactSearchText(const QString& text)
{
    if (text.isEmpty())
        return {};

    const QString normalized = normalizeSearchText(text);
    QString out;
    out.reserve(normalized.size());
    for (const QChar& c : normalized) {
        if (!c.isSpace())
            out.append(c);
    }
    return out;
}

QStringList tokenizeSearchText(const QString& normalizedText)
{
    if (normalizedText.isEmpty())
        return {};
    return normalizedText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

QStringList tokenVariants(const QString& token)
{
    QStringList variants;
    variants.append(token);

    const auto& r2a = romanToArabicMap();
    auto it1 = r2a.constFind(token);
    if (it1 != r2a.constEnd())
        variants.append(it1.value());

    const auto& a2r = arabicToRomanMap();
    auto it2 = a2r.constFind(token);
    if (it2 != a2r.constEnd())
        variants.append(it2.value());

    return variants;
}

QString convertRussianLayout(const QString& text)
{
    if (text.isEmpty())
        return {};

    static const QHash<QChar, QChar> s_ruToEn = {
        {QChar(0x0439), QLatin1Char('q')}, // й -> q
        {QChar(0x0446), QLatin1Char('w')}, // ц -> w
        {QChar(0x0443), QLatin1Char('e')}, // у -> e
        {QChar(0x043A), QLatin1Char('r')}, // к -> r
        {QChar(0x0435), QLatin1Char('t')}, // е -> t
        {QChar(0x043D), QLatin1Char('y')}, // н -> y
        {QChar(0x0433), QLatin1Char('u')}, // г -> u
        {QChar(0x0448), QLatin1Char('i')}, // ш -> i
        {QChar(0x0449), QLatin1Char('o')}, // щ -> o
        {QChar(0x0437), QLatin1Char('p')}, // з -> p
        {QChar(0x0445), QLatin1Char('[')}, // х -> [
        {QChar(0x044A), QLatin1Char(']')}, // ъ -> ]
        {QChar(0x0444), QLatin1Char('a')}, // ф -> a
        {QChar(0x044B), QLatin1Char('s')}, // ы -> s
        {QChar(0x0432), QLatin1Char('d')}, // в -> d
        {QChar(0x0430), QLatin1Char('f')}, // а -> f
        {QChar(0x043F), QLatin1Char('g')}, // п -> g
        {QChar(0x0440), QLatin1Char('h')}, // р -> h
        {QChar(0x043E), QLatin1Char('j')}, // о -> j
        {QChar(0x043B), QLatin1Char('k')}, // л -> k
        {QChar(0x0434), QLatin1Char('l')}, // д -> l
        {QChar(0x044F), QLatin1Char('z')}, // я -> z
        {QChar(0x0447), QLatin1Char('x')}, // ч -> x
        {QChar(0x0441), QLatin1Char('c')}, // с -> c
        {QChar(0x043C), QLatin1Char('v')}, // м -> v
        {QChar(0x0438), QLatin1Char('b')}, // и -> b
        {QChar(0x0442), QLatin1Char('n')}, // т -> n
        {QChar(0x044C), QLatin1Char('m')}, // ь -> m
        {QChar(0x0451), QLatin1Char('`')}, // ё -> `
    };

    bool hasCyrillic = false;
    for (const QChar& c : text) {
        if (c.unicode() >= 0x0400 && c.unicode() <= 0x04FF) {
            hasCyrillic = true;
            break;
        }
    }
    if (!hasCyrillic)
        return {};

    QString out;
    out.reserve(text.size());
    for (const QChar& c : text) {
        const QChar lower = c.toLower();
        auto it = s_ruToEn.constFind(lower);
        if (it != s_ruToEn.constEnd())
            out.append(it.value());
        else
            out.append(c);
    }
    return out;
}

QStringList generateTitleAcronyms(const QStringList& tokens)
{
    if (tokens.isEmpty())
        return {};

    QString allInitials;
    QString nonStopInitials;
    allInitials.reserve(tokens.size());
    nonStopInitials.reserve(tokens.size());

    for (const QString& tok : tokens) {
        if (tok.isEmpty())
            continue;
        const QChar initial = tok.at(0);
        allInitials.append(initial);
        if (!isStopWord(tok))
            nonStopInitials.append(initial);
    }

    QSet<QString> acronymSet;
    if (allInitials.size() >= 2)
        acronymSet.insert(allInitials);
    if (nonStopInitials.size() >= 2)
        acronymSet.insert(nonStopInitials);

    // Number conversions at end of acronym
    if (!tokens.isEmpty()) {
        const QString& lastTok = tokens.last();
        const auto& r2a = romanToArabicMap();
        const auto& a2r = arabicToRomanMap();

        auto expandNumber = [&](const QString& acr) {
            if (acr.isEmpty())
                return;
            auto itR = r2a.constFind(lastTok);
            if (itR != r2a.constEnd()) {
                QString modified = acr;
                modified.chop(1);
                modified.append(itR.value());
                acronymSet.insert(modified);
            }
            auto itA = a2r.constFind(lastTok);
            if (itA != a2r.constEnd()) {
                QString modified = acr;
                modified.chop(1);
                modified.append(itA.value());
                acronymSet.insert(modified);
            }
        };

        expandNumber(allInitials);
        expandNumber(nonStopInitials);
    }

    // Special sub-acronym for EA SPORTS FC games: "fc" + number / "fc"
    if (tokens.size() >= 3 && tokens.at(0) == QStringLiteral("ea")
        && tokens.at(1) == QStringLiteral("sports") && tokens.at(2) == QStringLiteral("fc")) {
        acronymSet.insert(QStringLiteral("fc"));
        if (tokens.size() >= 4) {
            acronymSet.insert(QStringLiteral("fc") + tokens.at(3));
            acronymSet.insert(QStringLiteral("eafc") + tokens.at(3));
            acronymSet.insert(QStringLiteral("esfc") + tokens.at(3));
        }
    }

    return acronymSet.values();
}

QStringList resolveSearchAliases(const QString& normalizedQuery)
{
    static const QHash<QString, QStringList> s_aliases = {
        {QStringLiteral("gta"), {QStringLiteral("grand theft auto")}},
        {QStringLiteral("gtav"), {QStringLiteral("grand theft auto v"), QStringLiteral("grand theft auto 5")}},
        {QStringLiteral("gta5"), {QStringLiteral("grand theft auto v"), QStringLiteral("grand theft auto 5")}},
        {QStringLiteral("gtaiv"), {QStringLiteral("grand theft auto iv"), QStringLiteral("grand theft auto 4")}},
        {QStringLiteral("gta4"), {QStringLiteral("grand theft auto iv"), QStringLiteral("grand theft auto 4")}},
        {QStringLiteral("gtasa"), {QStringLiteral("grand theft auto san andreas")}},
        {QStringLiteral("rdr"), {QStringLiteral("red dead redemption")}},
        {QStringLiteral("rdr2"), {QStringLiteral("red dead redemption 2"), QStringLiteral("red dead redemption ii")}},
        {QStringLiteral("rdr1"), {QStringLiteral("red dead redemption")}},
        {QStringLiteral("fc"), {QStringLiteral("ea sports fc"), QStringLiteral("fifa")}},
        {QStringLiteral("fifa"), {QStringLiteral("ea sports fc"), QStringLiteral("fifa")}},
        {QStringLiteral("nfs"), {QStringLiteral("need for speed")}},
        {QStringLiteral("cod"), {QStringLiteral("call of duty")}},
        {QStringLiteral("tes"), {QStringLiteral("the elder scrolls"), QStringLiteral("skyrim")}},
        {QStringLiteral("skyrim"), {QStringLiteral("the elder scrolls v skyrim"), QStringLiteral("skyrim")}},
        {QStringLiteral("cs"), {QStringLiteral("counter strike")}},
        {QStringLiteral("cs2"), {QStringLiteral("counter strike 2")}},
        {QStringLiteral("csgo"), {QStringLiteral("counter strike global offensive")}},
        {QStringLiteral("ac"), {QStringLiteral("assassin s creed")}},
        {QStringLiteral("gow"), {QStringLiteral("god of war")}},
        {QStringLiteral("kcd"), {QStringLiteral("kingdom come deliverance")}},
        {QStringLiteral("kcd2"), {QStringLiteral("kingdom come deliverance 2"), QStringLiteral("kingdom come deliverance ii")}},
        {QStringLiteral("cp2077"), {QStringLiteral("cyberpunk 2077")}},
        {QStringLiteral("re"), {QStringLiteral("resident evil")}},
        {QStringLiteral("re4"), {QStringLiteral("resident evil 4"), QStringLiteral("resident evil iv")}},
        {QStringLiteral("re2"), {QStringLiteral("resident evil 2"), QStringLiteral("resident evil ii")}},
        {QStringLiteral("re3"), {QStringLiteral("resident evil 3"), QStringLiteral("resident evil iii")}},
        {QStringLiteral("re7"), {QStringLiteral("resident evil 7"), QStringLiteral("resident evil biohazard")}},
        {QStringLiteral("re8"), {QStringLiteral("resident evil village")}},
        {QStringLiteral("dmc"), {QStringLiteral("devil may cry")}},
        {QStringLiteral("dmc5"), {QStringLiteral("devil may cry 5")}},
        {QStringLiteral("sf"), {QStringLiteral("street fighter")}},
        {QStringLiteral("sf6"), {QStringLiteral("street fighter 6")}},
        {QStringLiteral("mk"), {QStringLiteral("mortal kombat")}},
        {QStringLiteral("mk1"), {QStringLiteral("mortal kombat 1")}},
        {QStringLiteral("mk11"), {QStringLiteral("mortal kombat 11")}},
        {QStringLiteral("mkx"), {QStringLiteral("mortal kombat x"), QStringLiteral("mortal kombat 10")}},
        {QStringLiteral("tf2"), {QStringLiteral("team fortress 2")}},
        {QStringLiteral("l4d"), {QStringLiteral("left 4 dead")}},
        {QStringLiteral("l4d2"), {QStringLiteral("left 4 dead 2")}},
        {QStringLiteral("pubg"), {QStringLiteral("playerunknown s battlegrounds"), QStringLiteral("pubg")}},
        {QStringLiteral("bf"), {QStringLiteral("battlefield")}},
        {QStringLiteral("civ"), {QStringLiteral("civilization")}},
        {QStringLiteral("fm"), {QStringLiteral("football manager")}},
        {QStringLiteral("pes"), {QStringLiteral("pro evolution soccer"), QStringLiteral("efootball")}},
        {QStringLiteral("mhw"), {QStringLiteral("monster hunter world")}},
        {QStringLiteral("mhr"), {QStringLiteral("monster hunter rise")}},
        {QStringLiteral("poe"), {QStringLiteral("path of exile")}},
        {QStringLiteral("poe2"), {QStringLiteral("path of exile 2")}},
        {QStringLiteral("r6"), {QStringLiteral("rainbow six siege")}},
        {QStringLiteral("r6s"), {QStringLiteral("rainbow six siege")}},
        {QStringLiteral("tlou"), {QStringLiteral("the last of us")}},
        {QStringLiteral("tlou1"), {QStringLiteral("the last of us part i"), QStringLiteral("the last of us")}},
        {QStringLiteral("tlou2"), {QStringLiteral("the last of us part ii")}},
        {QStringLiteral("stalker"), {QStringLiteral("s t a l k e r"), QStringLiteral("stalker")}},
        {QStringLiteral("сталкер"), {QStringLiteral("s t a l k e r"), QStringLiteral("stalker")}},
        {QStringLiteral("ведьмак"), {QStringLiteral("the witcher"), QStringLiteral("witcher")}},
        {QStringLiteral("дота"), {QStringLiteral("dota")}},
        {QStringLiteral("киберпанк"), {QStringLiteral("cyberpunk 2077"), QStringLiteral("cyberpunk")}},
        {QStringLiteral("ассасин"), {QStringLiteral("assassin s creed")}},
        {QStringLiteral("скайрим"), {QStringLiteral("the elder scrolls v skyrim"), QStringLiteral("skyrim")}},
        {QStringLiteral("фоллаут"), {QStringLiteral("fallout")}},
        {QStringLiteral("батла"), {QStringLiteral("battlefield")}},
        {QStringLiteral("батлфилд"), {QStringLiteral("battlefield")}},
        {QStringLiteral("мортал комбат"), {QStringLiteral("mortal kombat")}},
        {QStringLiteral("элден ринг"), {QStringLiteral("elden ring")}},
        {QStringLiteral("диабло"), {QStringLiteral("diablo")}},
        {QStringLiteral("детройт"), {QStringLiteral("detroit become human")}},
        {QStringLiteral("палворлд"), {QStringLiteral("palworld")}},
        {QStringLiteral("хеллдайверс"), {QStringLiteral("helldivers")}},
    };

    auto it = s_aliases.constFind(normalizedQuery);
    if (it != s_aliases.constEnd())
        return it.value();

    // Check individual tokens if query has multi-words (e.g. "gta 5" -> "grand theft auto 5")
    const QStringList queryTokens = tokenizeSearchText(normalizedQuery);
    if (queryTokens.size() > 1) {
        auto firstIt = s_aliases.constFind(queryTokens.first());
        if (firstIt != s_aliases.constEnd()) {
            QStringList expanded;
            const QString rest = queryTokens.mid(1).join(QLatin1Char(' '));
            for (const QString& base : firstIt.value())
                expanded.append(base + QLatin1Char(' ') + rest);
            return expanded;
        }
    }

    return {};
}

ParsedSearchQuery ParsedSearchQuery::parse(const QString& query)
{
    ParsedSearchQuery pq;
    pq.rawQuery = query.trimmed();
    if (pq.rawQuery.isEmpty()) {
        pq.isEmpty = true;
        return pq;
    }

    pq.cleanQuery = normalizeSearchText(pq.rawQuery);
    pq.compactQuery = compactSearchText(pq.cleanQuery);
    if (pq.cleanQuery.isEmpty() && pq.compactQuery.isEmpty()) {
        pq.isEmpty = true;
        return pq;
    }
    pq.isEmpty = false;

    pq.tokens = tokenizeSearchText(pq.cleanQuery);
    pq.tokenVariantsList.reserve(pq.tokens.size());
    for (const QString& tok : pq.tokens)
        pq.tokenVariantsList.append(tokenVariants(tok));

    // Check numeric only (e.g. steam app id)
    bool ok = false;
    pq.compactQuery.toLongLong(&ok);
    pq.isNumericOnly = ok;

    // Russian layout mapping
    const QString converted = convertRussianLayout(pq.rawQuery);
    if (!converted.isEmpty() && converted != pq.rawQuery) {
        pq.layoutCleanQuery = normalizeSearchText(converted);
        pq.layoutCompactQuery = compactSearchText(pq.layoutCleanQuery);
        pq.layoutTokens = tokenizeSearchText(pq.layoutCleanQuery);
        pq.layoutTokenVariantsList.reserve(pq.layoutTokens.size());
        for (const QString& tok : pq.layoutTokens)
            pq.layoutTokenVariantsList.append(tokenVariants(tok));
    }

    // Alias expansions
    pq.aliasExpansions = resolveSearchAliases(pq.cleanQuery);
    if (pq.aliasExpansions.isEmpty() && !pq.layoutCleanQuery.isEmpty())
        pq.aliasExpansions = resolveSearchAliases(pq.layoutCleanQuery);

    pq.aliasCompacts.reserve(pq.aliasExpansions.size());
    for (const QString& alias : pq.aliasExpansions)
        pq.aliasCompacts.append(compactSearchText(alias));

    return pq;
}

CatalogSearchEntry CatalogSearchEntry::fromEntry(const CatalogEntry& entry)
{
    CatalogSearchEntry se;
    se.titleClean = normalizeSearchText(entry.title);
    se.titleCompact = compactSearchText(se.titleClean);
    se.tokens = tokenizeSearchText(se.titleClean);
    se.acronyms = generateTitleAcronyms(se.tokens);
    se.steamAppId = entry.steamAppId.trimmed();
    se.entryId = entry.id.trimmed();
    return se;
}

int scoreCatalogMatch(const CatalogSearchEntry& se, const CatalogEntry& rawEntry,
                      const ParsedSearchQuery& query)
{
    if (query.isEmpty)
        return 0;

    int score = 0;

    // 1. Direct AppID / EntryID match
    if (query.isNumericOnly && !se.steamAppId.isEmpty()) {
        if (se.steamAppId == query.compactQuery)
            return 120000;
    }
    if (!se.entryId.isEmpty() && (se.entryId == query.rawQuery || se.entryId == query.cleanQuery
                                  || se.entryId.endsWith(query.compactQuery))) {
        return 110000;
    }

    // 2. Exact Title Match
    if (se.titleCompact == query.compactQuery || se.titleClean == query.cleanQuery) {
        score = 100000;
    } else if (!query.layoutCompactQuery.isEmpty()
               && (se.titleCompact == query.layoutCompactQuery
                   || se.titleClean == query.layoutCleanQuery)) {
        score = 95000;
    }

    // 3. Title Starts With
    if (score == 0) {
        if (se.titleClean.startsWith(query.cleanQuery)
            || se.titleCompact.startsWith(query.compactQuery)) {
            score = 60000;
        } else if (!query.layoutCleanQuery.isEmpty()
                   && (se.titleClean.startsWith(query.layoutCleanQuery)
                       || se.titleCompact.startsWith(query.layoutCompactQuery))) {
            score = 55000;
        }
    }

    // 4. Substring Match of Full Query
    if (score == 0) {
        if (!query.cleanQuery.isEmpty()) {
            if (query.cleanQuery.contains(QLatin1Char(' ')) || query.cleanQuery.size() >= 5) {
                if (se.titleClean.contains(query.cleanQuery))
                    score = 45000;
            } else {
                for (const QString& t : se.tokens) {
                    if (t == query.cleanQuery) {
                        score = 45000;
                        break;
                    }
                    if (t.startsWith(query.cleanQuery)) {
                        score = 35000;
                        break;
                    }
                }
            }
        }
        if (score == 0 && !query.layoutCleanQuery.isEmpty()) {
            if (query.layoutCleanQuery.contains(QLatin1Char(' ')) || query.layoutCleanQuery.size() >= 5) {
                if (se.titleClean.contains(query.layoutCleanQuery))
                    score = 40000;
            } else {
                for (const QString& t : se.tokens) {
                    if (t == query.layoutCleanQuery) {
                        score = 40000;
                        break;
                    }
                    if (t.startsWith(query.layoutCleanQuery)) {
                        score = 30000;
                        break;
                    }
                }
            }
        }
    }

    // 5. Token-by-Token Match (All Query Tokens Must Match in Title)
    auto matchTokens = [&](const QStringList& qTokens,
                           const QVector<QStringList>& qVariantsList) -> int {
        if (qTokens.isEmpty())
            return 0;

        int tokenBonus = 0;
        int lastTitlePos = -1;
        bool inOrder = true;

        for (int qi = 0; qi < qTokens.size(); ++qi) {
            const QStringList& variants = qVariantsList.at(qi);
            bool tokenFound = false;
            int foundTitlePos = -1;

            for (int ti = 0; ti < se.tokens.size(); ++ti) {
                const QString& tTok = se.tokens.at(ti);
                for (const QString& v : variants) {
                    if (tTok == v) {
                        tokenFound = true;
                        tokenBonus += 1200;
                        foundTitlePos = ti;
                        break;
                    }
                    if (tTok.startsWith(v)) {
                        tokenFound = true;
                        tokenBonus += 700;
                        foundTitlePos = ti;
                        break;
                    }
                    if (v.size() >= 4 && tTok.contains(v)) {
                        tokenFound = true;
                        tokenBonus += 300;
                        foundTitlePos = ti;
                        break;
                    }
                }
                if (tokenFound)
                    break;
            }

            if (!tokenFound)
                return 0;

            if (foundTitlePos < lastTitlePos)
                inOrder = false;
            lastTitlePos = foundTitlePos;
        }

        int total = 25000 + tokenBonus;
        if (inOrder)
            total += 5000;
        return total;
    };

    if (score == 0 && !query.tokens.isEmpty()) {
        const int tokScore = matchTokens(query.tokens, query.tokenVariantsList);
        if (tokScore > 0)
            score = tokScore;
    }

    // 6. Token matching via Russian keyboard layout
    if (score == 0 && !query.layoutTokens.isEmpty()) {
        const int layoutScore = matchTokens(query.layoutTokens, query.layoutTokenVariantsList);
        if (layoutScore > 0)
            score = layoutScore - 3000;
    }

    // 7. Acronym Match
    if (score == 0 && !se.acronyms.isEmpty()) {
        if (se.acronyms.contains(query.compactQuery) || se.acronyms.contains(query.cleanQuery)) {
            score = 35000;
        } else if (!query.layoutCompactQuery.isEmpty()
                   && se.acronyms.contains(query.layoutCompactQuery)) {
            score = 32000;
        }
    }

    // 8. Franchise Alias Expansions
    if (score == 0 && !query.aliasExpansions.isEmpty()) {
        for (int i = 0; i < query.aliasExpansions.size(); ++i) {
            const QString& alias = query.aliasExpansions.at(i);
            const QString& aliasCompact = query.aliasCompacts.at(i);

            if (se.titleClean.contains(alias) || se.titleCompact.contains(aliasCompact)) {
                score = 30000;
                break;
            }

            const QStringList aliasTokens = tokenizeSearchText(alias);
            QVector<QStringList> aliasVariants;
            aliasVariants.reserve(aliasTokens.size());
            for (const QString& t : aliasTokens)
                aliasVariants.append(tokenVariants(t));

            const int aliasScore = matchTokens(aliasTokens, aliasVariants);
            if (aliasScore > 0) {
                score = aliasScore - 1000;
                break;
            }
        }
    }

    // 9. Compact Substring Match (e.g. "fc24" inside "easportsfc24", "spiderman2" inside "marvelsspiderman2")
    if (score == 0 && query.compactQuery.size() >= 4) {
        if (se.titleCompact.contains(query.compactQuery)) {
            score = 22000;
        } else if (!query.layoutCompactQuery.isEmpty() && query.layoutCompactQuery.size() >= 4
                   && se.titleCompact.contains(query.layoutCompactQuery)) {
            score = 19000;
        }
    }

    if (score == 0)
        return 0;

    // Length difference penalty (prefer concise matches closer in length to query)
    const int lenDiff = se.titleCompact.length() - query.compactQuery.length();
    if (lenDiff > 0)
        score -= qMin(2000, lenDiff * 15);

    // Popularity & quality bonuses
    score += qMin(1000, static_cast<int>(rawEntry.hypeScore * 15.0));
    if (rawEntry.currentPlayers > 0)
        score += qMin(500, rawEntry.currentPlayers / 20);
    if (rawEntry.metacriticScore > 0)
        score += rawEntry.metacriticScore * 3;
    if (rawEntry.recommendationsTotal > 0)
        score += qMin(400, static_cast<int>(std::log(1.0 + rawEntry.recommendationsTotal) * 35.0));
    if (!rawEntry.steamAppId.isEmpty())
        score += 150;
    if (!rawEntry.coverUrl.isEmpty() || !rawEntry.remoteCoverUrl.isEmpty())
        score += 100;

    return qMax(1, score);
}

} // namespace arachnel::core
