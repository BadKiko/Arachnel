#include "catalog_search_utils.h"

#include <QChar>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringView>
#include <QVarLengthArray>
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
            // Katakana and hiragana are the same sounds: fold so "ウィッチャー" finds "うぃっちゃー".
            const ushort u = c.unicode();
            if (u >= 0x30A1 && u <= 0x30F6)
                out.append(QChar(ushort(u - 0x60)));
            else
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
    static const QHash<QString, QStringList> s_aliases = [] {
    const QHash<QString, QStringList> raw = {
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
        {QStringLiteral("гта"), {QStringLiteral("grand theft auto")}},
        {QStringLiteral("майнкрафт"), {QStringLiteral("minecraft")}},
        {QStringLiteral("контр страйк"), {QStringLiteral("counter strike")}},
        {QStringLiteral("контра"), {QStringLiteral("counter strike")}},
        {QStringLiteral("кс"), {QStringLiteral("counter strike")}},
        {QStringLiteral("хитман"), {QStringLiteral("hitman")}},
        {QStringLiteral("мафия"), {QStringLiteral("mafia")}},
        {QStringLiteral("биошок"), {QStringLiteral("bioshock")}},
        {QStringLiteral("дарк соулс"), {QStringLiteral("dark souls")}},
        {QStringLiteral("темные души"), {QStringLiteral("dark souls")}},
        {QStringLiteral("тёмные души"), {QStringLiteral("dark souls")}},
        {QStringLiteral("секиро"), {QStringLiteral("sekiro")}},
        {QStringLiteral("ласт оф ас"), {QStringLiteral("the last of us")}},
        {QStringLiteral("одни из нас"), {QStringLiteral("the last of us")}},
        {QStringLiteral("рэд дед"), {QStringLiteral("red dead redemption")}},
        {QStringLiteral("ред дед"), {QStringLiteral("red dead redemption")}},
        {QStringLiteral("обливион"), {QStringLiteral("oblivion")}},
        {QStringLiteral("сайлент хилл"), {QStringLiteral("silent hill")}},
        {QStringLiteral("обитель зла"), {QStringLiteral("resident evil")}},
        {QStringLiteral("резидент ивел"), {QStringLiteral("resident evil")}},
        {QStringLiteral("фар край"), {QStringLiteral("far cry")}},
        {QStringLiteral("дальний край"), {QStringLiteral("far cry")}},
        {QStringLiteral("бог войны"), {QStringLiteral("god of war")}},
        {QStringLiteral("человек паук"), {QStringLiteral("spider man")}},
        {QStringLiteral("бэтмен"), {QStringLiteral("batman")}},
        {QStringLiteral("метро"), {QStringLiteral("metro")}},
        {QStringLiteral("холлоу найт"), {QStringLiteral("hollow knight")}},
        {QStringLiteral("террария"), {QStringLiteral("terraria")}},
        {QStringLiteral("раст"), {QStringLiteral("rust")}},
        {QStringLiteral("субнатика"), {QStringLiteral("subnautica")}},
        {QStringLiteral("варфрейм"), {QStringLiteral("warframe")}},
        {QStringLiteral("генш"), {QStringLiteral("genshin")}},
        {QStringLiteral("巫师"), {QStringLiteral("witcher")}},
        {QStringLiteral("赛博朋克"), {QStringLiteral("cyberpunk")}},
        {QStringLiteral("艾尔登法环"), {QStringLiteral("elden ring")}},
        {QStringLiteral("黑神话"), {QStringLiteral("black myth")}},
        {QStringLiteral("文明"), {QStringLiteral("civilization")}},
        {QStringLiteral("我的世界"), {QStringLiteral("minecraft")}},
        {QStringLiteral("ウィッチャー"), {QStringLiteral("witcher")}},
        {QStringLiteral("마인크래프트"), {QStringLiteral("minecraft")}},
    };
    // Keys are written naturally; the query arrives normalized (й -> и, ё -> е, katakana folded).
    QHash<QString, QStringList> out;
    out.reserve(raw.size());
    for (auto it = raw.cbegin(); it != raw.cend(); ++it)
        out.insert(normalizeSearchText(it.key()), it.value());
    return out;
    }();

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

// ---------------------------------------------------------------------------------------------
// Query forms, typo tolerance, phonetic matching and scoring.
//
// Matching is tiered. A strict tier always beats a looser one, and the looser tiers (typos,
// sound-alikes) only run when the strict ones found too few results (see the filter service),
// so a precise query never gets noise from them.
//
//   100000  title equals the query
//    95000  ... after fixing the keyboard layout (цшегсрук -> witcher)
//    80000  query words are the start of the title ("witch" -> "Witcher 3")
//    78000  ... written without spaces ("fc26" -> "FC 26 ...")
//    70000  query words appear as consecutive words inside the title
//    68000  ... written without spaces ("spiderman2" -> "Marvel's Spider-Man 2")
//    50000+ every query word is in the title, any order
//    45000  acronym ("gta5", "rdr2")
//    <=52000 franchise / translated alias ("ведьмак", "гта")
//    ~30000 typo-tolerant match (level 1)
//    ~20000 sounds the same across scripts (level 2): "киберпанк" ~ "Cyberpunk"
//
// Substrings inside a word are NOT matched ("witcher" must not find "Switcher"), except for
// scripts without spaces (CJK), where a substring is the only kind of word match there is.
// ---------------------------------------------------------------------------------------------

namespace {

inline bool isCjkUnit(ushort u)
{
    return (u >= 0x0E00 && u <= 0x0E7F) || (u >= 0x1100 && u <= 0x11FF)
           || (u >= 0x3040 && u <= 0x30FF) || (u >= 0x3400 && u <= 0x4DBF)
           || (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0xAC00 && u <= 0xD7AF)
           || (u >= 0xF900 && u <= 0xFAFF);
}

} // namespace

bool containsCjk(const QString& text)
{
    for (const QChar& c : text) {
        if (isCjkUnit(c.unicode()))
            return true;
    }
    return false;
}

int boundedEditDistance(QStringView a, QStringView b, int maxDist)
{
    const int n = a.size();
    const int m = b.size();
    if (qAbs(n - m) > maxDist)
        return maxDist + 1;
    if (n == 0 || m == 0)
        return qMax(n, m) <= maxDist ? qMax(n, m) : maxDist + 1;
    constexpr int kMax = 48;
    if (n > kMax || m > kMax)
        return a == b ? 0 : maxDist + 1;

    int rowA[kMax + 1];
    int rowB[kMax + 1];
    int rowC[kMax + 1];
    int* prev2 = rowA;
    int* prev = rowB;
    int* cur = rowC;
    for (int j = 0; j <= m; ++j)
        prev[j] = j;

    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        int rowMin = cur[0];
        for (int j = 1; j <= m; ++j) {
            const int cost = a.at(i - 1) == b.at(j - 1) ? 0 : 1;
            int v = qMin(qMin(prev[j] + 1, cur[j - 1] + 1), prev[j - 1] + cost);
            if (i > 1 && j > 1 && a.at(i - 1) == b.at(j - 2) && a.at(i - 2) == b.at(j - 1))
                v = qMin(v, prev2[j - 2] + 1);
            cur[j] = v;
            rowMin = qMin(rowMin, v);
        }
        if (rowMin > maxDist)
            return maxDist + 1;
        int* t = prev2;
        prev2 = prev;
        prev = cur;
        cur = t;
    }
    return qMin(prev[m], maxDist + 1);
}

namespace {

const QHash<QChar, QString>& cyrillicToLatin()
{
    static const QHash<QChar, QString> s_map = {
        {QChar(0x0430), QStringLiteral("a")},  {QChar(0x0431), QStringLiteral("b")},
        {QChar(0x0432), QStringLiteral("v")},  {QChar(0x0433), QStringLiteral("g")},
        {QChar(0x0491), QStringLiteral("g")},  {QChar(0x0434), QStringLiteral("d")},
        {QChar(0x0435), QStringLiteral("e")},  {QChar(0x0451), QStringLiteral("e")},
        {QChar(0x0454), QStringLiteral("e")},  {QChar(0x0436), QStringLiteral("zh")},
        {QChar(0x0437), QStringLiteral("z")},  {QChar(0x0438), QStringLiteral("i")},
        {QChar(0x0456), QStringLiteral("i")},  {QChar(0x0457), QStringLiteral("i")},
        {QChar(0x0439), QStringLiteral("i")},  {QChar(0x043A), QStringLiteral("k")},
        {QChar(0x043B), QStringLiteral("l")},  {QChar(0x043C), QStringLiteral("m")},
        {QChar(0x043D), QStringLiteral("n")},  {QChar(0x043E), QStringLiteral("o")},
        {QChar(0x043F), QStringLiteral("p")},  {QChar(0x0440), QStringLiteral("r")},
        {QChar(0x0441), QStringLiteral("s")},  {QChar(0x0442), QStringLiteral("t")},
        {QChar(0x0443), QStringLiteral("u")},  {QChar(0x0444), QStringLiteral("f")},
        {QChar(0x0445), QStringLiteral("h")},  {QChar(0x0446), QStringLiteral("ts")},
        {QChar(0x0447), QStringLiteral("ch")}, {QChar(0x0448), QStringLiteral("sh")},
        {QChar(0x0449), QStringLiteral("sh")}, {QChar(0x044A), QString()},
        {QChar(0x044B), QStringLiteral("i")},  {QChar(0x044C), QString()},
        {QChar(0x044D), QStringLiteral("e")},  {QChar(0x044E), QStringLiteral("yu")},
        {QChar(0x044F), QStringLiteral("ya")},
    };
    return s_map;
}

} // namespace

QString phoneticSkeleton(QStringView token)
{
    if (token.isEmpty())
        return {};

    // 1. Everything to Latin letters / digits.
    QString t;
    t.reserve(token.size() + 4);
    const auto& cyr = cyrillicToLatin();
    for (const QChar& ch : token) {
        const QChar lc = ch.toLower();
        if (lc.unicode() < 0x80) {
            if (!lc.isLetterOrNumber())
                return {};
            t.append(lc);
            continue;
        }
        const auto it = cyr.constFind(lc);
        if (it == cyr.cend())
            return {}; // CJK or another script: no phonetic key
        t.append(it.value());
    }

    // 2. One pass: digraphs first ("ph" is f, not p + h), then one symbol per sound class.
    //    c / k / q / s / z / x share a class: "cyber" and "kiber" are the same word to a Russian
    //    speaker, and an English "c" is read either way.
    const auto at = [&t](int i) { return i < t.size() ? t.at(i).unicode() : ushort(0); };
    QString out;
    out.reserve(t.size());
    const auto putSym = [&out](QChar sym) {
        if (out.isEmpty() || out.back() != sym)
            out.append(sym);
    };
    for (int i = 0; i < t.size(); ++i) {
        const ushort c = t.at(i).unicode();
        const ushort n = at(i + 1);
        if (c == 't' && n == 'c' && at(i + 2) == 'h') { putSym(QLatin1Char('C')); i += 2; continue; }
        if (c == 's' && n == 'c' && at(i + 2) == 'h') { putSym(QLatin1Char('S')); i += 2; continue; }
        if (c == 's' && n == 'h') { putSym(QLatin1Char('S')); i += 1; continue; }
        if (c == 'c' && n == 'h') { putSym(QLatin1Char('C')); i += 1; continue; }
        if (c == 'z' && n == 'h') { putSym(QLatin1Char('J')); i += 1; continue; }
        if (c == 'p' && n == 'h') { putSym(QLatin1Char('f')); i += 1; continue; }
        if (c == 'k' && n == 'h') { i += 1; continue; }
        if (c == 't' && n == 'h') { putSym(QLatin1Char('t')); i += 1; continue; }
        switch (c) {
        case 'a': case 'e': case 'i': case 'o': case 'u': case 'y': case 'h':
            continue; // vowels and the weak "h" carry little identity across scripts
        case 'c': case 'k': case 'q': case 's': case 'z': case 'x':
            putSym(QLatin1Char('k'));
            break;
        case 'w':
            putSym(QLatin1Char('v'));
            break;
        case 'j':
            putSym(QLatin1Char('J'));
            break;
        default:
            putSym(QChar(c));
            break;
        }
    }
    return out.size() >= 3 ? out : QString();
}

QueryForm QueryForm::build(const QString& text)
{
    QueryForm f;
    f.clean = normalizeSearchText(text);
    f.compact = compactSearchText(f.clean);
    f.tokens = tokenizeSearchText(f.clean);
    f.variants.reserve(f.tokens.size());
    f.isStop.reserve(f.tokens.size());
    f.isCjk.reserve(f.tokens.size());
    for (const QString& tok : f.tokens) {
        // A lone "5" must not turn into "v": numeral forms only help next to a title word.
        f.variants.append(f.tokens.size() > 1 ? tokenVariants(tok) : QStringList{tok});
        f.isStop.append(isStopWord(tok));
        f.isCjk.append(containsCjk(tok));
        f.skeletons.append(phoneticSkeleton(tok));
    }
    return f;
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
    pq.main = QueryForm::build(pq.rawQuery);

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
        pq.layout = QueryForm::build(converted);
    }

    // Alias expansions
    pq.aliasExpansions = resolveSearchAliases(pq.cleanQuery);
    if (pq.aliasExpansions.isEmpty() && !pq.layoutCleanQuery.isEmpty())
        pq.aliasExpansions = resolveSearchAliases(pq.layoutCleanQuery);

    pq.aliasCompacts.reserve(pq.aliasExpansions.size());
    pq.aliases.reserve(pq.aliasExpansions.size());
    for (const QString& alias : pq.aliasExpansions) {
        pq.aliasCompacts.append(compactSearchText(alias));
        pq.aliases.append(QueryForm::build(alias));
    }

    return pq;
}

CatalogSearchEntry CatalogSearchEntry::fromEntry(const CatalogEntry& entry)
{
    CatalogSearchEntry se;
    se.titleClean = normalizeSearchText(entry.title);
    const QStringList tokens = tokenizeSearchText(se.titleClean);
    se.acronyms = generateTitleAcronyms(tokens).join(QLatin1Char(' '));
    const int spaces = tokens.isEmpty() ? 0 : tokens.size() - 1;
    se.compactLen = static_cast<quint16>(qMin<int>(0xFFFF, se.titleClean.size() - spaces));
    return se;
}

namespace {

enum class Hit { None, Exact, Prefix, Substring };

using TokenViews = QVarLengthArray<QStringView, 12>;

/** The words of an already normalized title: views into it, no allocation for short titles. */
TokenViews tokensOf(const QString& clean)
{
    TokenViews out;
    const QStringView v(clean);
    qsizetype start = 0;
    for (qsizetype i = 0; i <= v.size(); ++i) {
        if (i == v.size() || v.at(i) == QLatin1Char(' ')) {
            if (i > start)
                out.append(v.mid(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

/** titleClean without its spaces equals `compact`? */
bool compactEquals(const CatalogSearchEntry& se, const QString& compact)
{
    if (compact.isEmpty() || se.compactLen != compact.size())
        return false;
    qsizetype k = 0;
    for (const QChar c : se.titleClean) {
        if (c == QLatin1Char(' '))
            continue;
        if (k >= compact.size() || compact.at(k) != c)
            return false;
        ++k;
    }
    return k == compact.size();
}

/** Is `needle` one of the space-separated words of `packed`? */
bool packedContains(const QString& packed, const QString& needle)
{
    if (packed.isEmpty() || needle.isEmpty())
        return false;
    const QStringView v(packed);
    qsizetype start = 0;
    for (qsizetype i = 0; i <= v.size(); ++i) {
        if (i == v.size() || v.at(i) == QLatin1Char(' ')) {
            if (i > start && v.mid(start, i - start) == QStringView(needle))
                return true;
            start = i + 1;
        }
    }
    return false;
}

bool isAllDigits(const QString& s)
{
    if (s.isEmpty())
        return false;
    for (const QChar& c : s) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

/** How one query word relates to one title word. Substrings only for CJK. */
Hit tokenHit(QStringView title, const QStringList& variants, bool cjk, bool allowPrefix)
{
    for (const QString& v : variants) {
        if (title == v)
            return Hit::Exact;
    }
    if (cjk) {
        for (const QString& v : variants) {
            if (title.contains(v))
                return Hit::Substring;
        }
        return Hit::None;
    }
    if (allowPrefix) {
        for (const QString& v : variants) {
            // "3" must not prefix-match "30" / "300": numbers are exact or nothing.
            if (v.size() >= 2 && !isAllDigits(v) && title.startsWith(v))
                return Hit::Prefix;
        }
    }
    return Hit::None;
}

/**
 * Query words as consecutive title words. Only the last one may be an unfinished prefix;
 * *lastIsPrefix tells whether it was ("witch" in "witcher") or a whole word.
 */
bool findPhrase(const TokenViews& toks, const QueryForm& q, bool allowPrefix, int* startAt,
                bool* lastIsPrefix)
{
    const int n = q.tokens.size();
    const int total = toks.size();
    if (n == 0 || n > total)
        return false;
    bool found = false;
    for (int s = 0; s + n <= total; ++s) {
        bool ok = true;
        bool prefix = false;
        for (int k = 0; k < n; ++k) {
            const bool last = k == n - 1;
            const Hit h = tokenHit(toks.at(s + k), q.variants.at(k), q.isCjk.at(k),
                                   last && allowPrefix);
            if (h == Hit::None || (!last && h == Hit::Prefix)) {
                ok = false;
                break;
            }
            prefix = h == Hit::Prefix;
        }
        // Keep looking for a position where the last word is complete.
        if (ok && (!found || (*lastIsPrefix && !prefix))) {
            *startAt = s;
            *lastIsPrefix = prefix;
            found = true;
            if (!prefix)
                break;
        }
    }
    return found;
}

/**
 * The query written without spaces matches whole title words glued together, starting at a word
 * boundary: "fc26" in "ea sports fc 26", "spiderman2" in "marvels spider man 2". Starting
 * mid-word is not allowed, so "witcher" does not match inside "switcher".
 */
bool findCompactPhrase(const TokenViews& toks, const QString& qc, bool allowPrefix, int* startAt)
{
    if (qc.size() < 3)
        return false;
    const int total = toks.size();
    const QStringView qv(qc);
    for (int s = 0; s < total; ++s) {
        int len = 0;
        for (int e = s; e < total; ++e) {
            const QStringView tk = toks.at(e);
            const int rest = qc.size() - len;
            if (tk.size() <= rest) {
                if (qv.mid(len, tk.size()) != tk)
                    break;
                len += tk.size();
                if (len == qc.size()) {
                    *startAt = s;
                    return true;
                }
            } else {
                if (allowPrefix && rest >= 2 && tk.startsWith(qv.mid(len))) {
                    *startAt = s;
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

/** Every non-stop query word is some title word, in any order. Returns a bonus, 0 if not. */
int matchAnyOrder(const TokenViews& toks, const QueryForm& q, bool allowPrefix, bool* inOrder)
{
    const int n = q.tokens.size();
    if (n == 0)
        return 0;
    int nonStop = 0;
    for (int k = 0; k < n; ++k)
        nonStop += q.isStop.at(k) ? 0 : 1;

    QVarLengthArray<char, 16> used(toks.size());
    for (int i = 0; i < used.size(); ++i)
        used[i] = 0;
    int bonus = 0;
    int lastPos = -1;
    bool ordered = true;
    for (int k = 0; k < n; ++k) {
        int bestPos = -1;
        int bestGain = 0;
        for (int ti = 0; ti < toks.size(); ++ti) {
            if (used[ti])
                continue;
            const Hit h = tokenHit(toks.at(ti), q.variants.at(k), q.isCjk.at(k), allowPrefix);
            // A bare two-letter start ("so" in "dark so") is too weak to pair with other words.
            const bool weakPrefix = h == Hit::Prefix && q.tokens.at(k).size() < 3;
            const int gain = h == Hit::Exact ? 1200
                : (h == Hit::Prefix && !weakPrefix) ? 700
                : h == Hit::Substring ? 500 : 0;
            if (gain > bestGain) {
                bestGain = gain;
                bestPos = ti;
                if (gain == 1200)
                    break;
            }
        }
        if (bestPos < 0) {
            if (q.isStop.at(k) && nonStop > 0)
                continue; // "the" may be missing from the title
            return 0;
        }
        used[bestPos] = 1;
        bonus += bestGain;
        if (bestPos < lastPos)
            ordered = false;
        lastPos = bestPos;
    }
    *inOrder = ordered;
    return qMax(1, bonus);
}

/** Tiered score for one written form of the query. 0 = no match. */
int scoreForm(const TokenViews& toks, const QueryForm& q, bool allowPrefix)
{
    if (q.isEmpty())
        return 0;
    int start = 0;
    bool prefix = false;
    if (findPhrase(toks, q, allowPrefix, &start, &prefix)) {
        const int base = start == 0 ? 80000 : 70000;
        return prefix ? base - 4000 : base;
    }
    if (findCompactPhrase(toks, q.compact, allowPrefix, &start))
        return start == 0 ? 78000 : 68000;
    bool ordered = true;
    const int bonus = matchAnyOrder(toks, q, allowPrefix, &ordered);
    if (bonus > 0)
        return 50000 + bonus + (ordered ? 5000 : 0);
    return 0;
}

/** Level 1: every non-stop query word is a title word, allowing a typo in some of them. */
int scoreTypos(const TokenViews& toks, const QueryForm& q)
{
    if (q.isEmpty())
        return 0;
    int edits = 0;
    int penalty = 0;
    int exact = 0;
    int checked = 0;
    for (int k = 0; k < q.tokens.size(); ++k) {
        if (q.isStop.at(k))
            continue;
        ++checked;
        const QString& qt = q.tokens.at(k);
        bool found = false;
        int bestEdits = 99;
        int bestPenalty = 0;

        for (const QStringView title : toks) {
            const Hit h = tokenHit(title, q.variants.at(k), q.isCjk.at(k), true);
            if (h != Hit::None) {
                found = true;
                bestEdits = 0;
                if (h == Hit::Exact)
                    ++exact;
                break;
            }
        }
        // Numbers are never "typos" of each other ("2077" vs "2007" are different games).
        bool hasDigit = false;
        for (const QChar& c : qt)
            hasDigit = hasDigit || c.isDigit();
        if (!found && !hasDigit && !q.isCjk.at(k) && qt.size() >= 4) {
            const int maxE = qt.size() >= 10 ? 2 : 1;
            for (const QStringView title : toks) {
                if (qAbs(title.size() - qt.size()) > maxE + 2)
                    continue;
                // A typo is rarely in the very first letter; this prunes most of the catalog.
                if (title.at(0) != qt.at(0) && title.at(title.size() - 1) != qt.at(qt.size() - 1))
                    continue;
                int d = boundedEditDistance(qt, title, maxE);
                int pen = 3500 * d;
                if (d > maxE && title.size() > qt.size() && qt.size() >= 5) {
                    // As-you-type typo: "wicher" against the start of "witcher". Ranked below a
                    // typo in a complete word.
                    const int dp = boundedEditDistance(qt, title.left(qt.size()), 1);
                    if (dp <= 1) {
                        d = dp;
                        pen = 3500 * dp + 6000;
                    }
                }
                if (d <= maxE && (d < bestEdits || (d == bestEdits && pen < bestPenalty))) {
                    bestEdits = d;
                    bestPenalty = pen;
                    found = true;
                }
            }
        }
        if (!found)
            return 0;
        edits += bestEdits;
        penalty += bestPenalty;
    }
    if (checked == 0 || edits == 0 || edits > 2)
        return 0; // no typo at all means the strict tiers would already have matched
    return 30000 - penalty + 600 * exact;
}

bool hasNonLatin(const QString& token)
{
    for (const QChar& c : token) {
        if (c.unicode() >= 0x80)
            return true;
    }
    return false;
}

/**
 * Level 2: every non-stop query word sounds like a title word (киберпанк ~ cyberpunk).
 * Only for words written in another script - a Latin word with a typo is level 1's job.
 */
int scorePhonetic(const TokenViews& toks, const QueryForm& q)
{
    if (q.isEmpty())
        return 0;
    int wordCount = 0;
    for (int k = 0; k < q.tokens.size(); ++k)
        wordCount += q.isStop.at(k) ? 0 : 1;
    // A short key ("ldn") is too ambiguous for a lone word, but fine when another word backs it up.
    const int minSkeleton = wordCount >= 2 ? 3 : 4;

    int matched = 0;
    for (int k = 0; k < q.tokens.size(); ++k) {
        if (q.isStop.at(k))
            continue;
        const QString& qt = q.tokens.at(k);
        const QString& skel = q.skeletons.at(k);
        if (skel.size() < minSkeleton || !hasNonLatin(qt))
            return 0;
        const int maxDiff = qMax(3, qt.size() / 2);
        bool found = false;
        for (const QStringView title : toks) {
            if (qAbs(title.size() - qt.size()) > maxDiff)
                continue;
            if (phoneticSkeleton(title) == skel) {
                found = true;
                break;
            }
        }
        if (!found)
            return 0;
        ++matched;
    }
    return matched > 0 ? 20000 + 500 * matched : 0;
}

} // namespace

int scoreCatalogMatch(const CatalogSearchEntry& se, const CatalogEntry& rawEntry,
                      const ParsedSearchQuery& query)
{
    if (query.isEmpty)
        return 0;

    int score = 0;

    // 1. Direct AppID / EntryID match
    if (query.isNumericOnly && !rawEntry.steamAppId.isEmpty()) {
        if (rawEntry.steamAppId.trimmed() == query.compactQuery)
            return 120000;
    }
    const QString& entryId = rawEntry.id;
    if (!entryId.isEmpty()) {
        // "steam-1328660" is found by the whole id; a bare number is matched via the app id above.
        const int n = query.compactQuery.size();
        const int idLen = entryId.size();
        if (entryId == query.rawQuery || entryId == query.cleanQuery
            || (n >= 3 && idLen > n && entryId.at(idLen - n - 1) == QLatin1Char('-')
                && entryId.endsWith(query.compactQuery))) {
            return 110000;
        }
    }

    // 2. The whole title
    if (compactEquals(se, query.compactQuery) || se.titleClean == query.cleanQuery) {
        score = 100000;
    } else if (!query.layoutCompactQuery.isEmpty()
               && (compactEquals(se, query.layoutCompactQuery)
                   || se.titleClean == query.layoutCleanQuery)) {
        score = 95000;
    }

    // The title's words, cut into views only when a word-level tier needs them.
    TokenViews toks;
    bool haveToks = false;
    const auto words = [&]() -> const TokenViews& {
        if (!haveToks) {
            toks = tokensOf(se.titleClean);
            haveToks = true;
        }
        return toks;
    };

    // 3. Words of the query inside the title (start / phrase / any order). The last word may be
    //    unfinished ("witch" -> "witcher") from level 1 on.
    const bool allowPrefix = query.level >= 1;
    if (score == 0)
        score = scoreForm(words(), query.main, allowPrefix);
    if (score == 0 && !query.layout.isEmpty()) {
        const int s = scoreForm(words(), query.layout, allowPrefix);
        if (s > 0)
            score = s - 3000;
    }

    // 4. Acronyms: "gta5", "rdr2", "fc26"
    if (score == 0 && !se.acronyms.isEmpty()) {
        if (packedContains(se.acronyms, query.compactQuery)
            || packedContains(se.acronyms, query.cleanQuery))
            score = 45000;
        else if (!query.layoutCompactQuery.isEmpty()
                 && packedContains(se.acronyms, query.layoutCompactQuery))
            score = 42000;
    }

    // 5. Franchise aliases / translations ("ведьмак" -> witcher, "gta" -> grand theft auto)
    //    Taken together with the literal match: "re 4" is "Resident Evil 4", not "ReThink 4".
    if (score < 95000) {
        for (const QueryForm& alias : query.aliases) {
            const int s = scoreForm(words(), alias, allowPrefix);
            if (s > 0) {
                score = qMax(score, qMin(s, 70000) - 8000);
                break;
            }
        }
    }

    // 6. Typos, then sounds-alike: only when the caller widened the search
    if (score == 0 && query.level >= 2)
        score = scoreTypos(words(), query.main);
    if (score == 0 && query.level >= 3) {
        score = scorePhonetic(words(), query.main);
        if (score == 0 && !query.layout.isEmpty())
            score = scorePhonetic(words(), query.layout);
    }

    if (score == 0)
        return 0;

    // Length difference penalty (prefer concise matches closer in length to query)
    const int lenDiff = se.compactLen - query.compactQuery.length();
    if (lenDiff > 0)
        score -= qMin(2000, lenDiff * 15);

    // Popularity & quality bonuses (kept well below the gap between tiers)
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
