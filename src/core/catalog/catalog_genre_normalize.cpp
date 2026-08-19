#include "catalog_genre_normalize.h"

#include <QCoreApplication>
#include <QHash>

namespace arachnel::core {

namespace {

const QHash<QString, QString>& genreAliasMap()
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        const auto add = [&m](const QString& key, std::initializer_list<const char*> aliases) {
            for (const char* a : aliases)
                m.insert(QString::fromUtf8(a).toLower(), key);
            m.insert(key.toLower(), key);
        };
        add(QStringLiteral("Action"),
            {"action", "acción", "aksiyon", "azione", "action games", "экшен", "экшн"});
        add(QStringLiteral("Adventure"),
            {"adventure", "aventura", "aventure", "avventura", "приключения"});
        add(QStringLiteral("RPG"),
            {"rpg", "role-playing", "role playing", "jrpg", "arpg", "ролевые"});
        add(QStringLiteral("Strategy"),
            {"strategy", "estrategia", "stratégie", "strategia", "стратегии"});
        add(QStringLiteral("Simulation"),
            {"simulation", "simulación", "simulation games", "симуляторы", "simulator"});
        add(QStringLiteral("Sports"), {"sports", "deportes", "sport", "спорт"});
        add(QStringLiteral("Racing"), {"racing", "carreras", "course", "гонки", "driving"});
        add(QStringLiteral("Indie"), {"indie", "инди"});
        add(QStringLiteral("Casual"), {"casual", "казуальные", "casual games"});
        add(QStringLiteral("Horror"), {"horror", "terror", "хоррор", "survival horror"});
        add(QStringLiteral("Puzzle"), {"puzzle", "puzzles", "головоломки"});
        add(QStringLiteral("Shooter"),
            {"shooter", "fps", "tps", "шутер", "first-person shooter", "third-person shooter"});
        add(QStringLiteral("Platformer"), {"platformer", "platform", "платформеры"});
        add(QStringLiteral("Fighting"), {"fighting", "fight", "файтинги", "beat 'em up"});
        add(QStringLiteral("Survival"), {"survival", "выживание"});
        add(QStringLiteral("Open World"), {"open world", "открытый мир", "sandbox"});
        add(QStringLiteral("Visual Novel"), {"visual novel", "визуальная новелла"});
        add(QStringLiteral("Card"), {"card", "card game", "deckbuilding", "карточные"});
        add(QStringLiteral("Roguelike"), {"roguelike", "roguelite", "рогалик"});
        add(QStringLiteral("Early Access"),
            {"early access", "acceso anticipado", "accès anticipé", "ранний доступ"});
        add(QStringLiteral("Free to Play"), {"free to play", "f2p", "бесплатные"});
        add(QStringLiteral("Massively Multiplayer"),
            {"massively multiplayer", "mmo", "mmorpg", "массовый мультиплеер"});
        add(QStringLiteral("VR"),
            {"vr", "virtual reality", "vr games", "vr only", "vr-only", "vr support", "vr supported",
             "valve index", "htc vive", "oculus", "виртуальная реальность", "поддержка vr",
             "только vr"});
        return m;
    }();
    return map;
}

} // namespace

bool isPlayModeGenreToken(const QString& tokenLower)
{
    return tokenLower.contains(QLatin1String("single-player"))
        || tokenLower.contains(QLatin1String("singleplayer"))
        || tokenLower.contains(QLatin1String("multi-player"))
        || tokenLower.contains(QLatin1String("multiplayer"))
        || tokenLower.contains(QLatin1String("co-op")) || tokenLower.contains(QLatin1String("coop"))
        || tokenLower.contains(QLatin1String("online pvp")) || tokenLower == QLatin1String("pvp")
        || tokenLower.contains(QLatin1String("mmo"))
        || tokenLower.contains(QLatin1String("cross-platform"))
        || tokenLower.contains(QLatin1String("online fix"))
        || tokenLower.contains(QLatin1String("shared/split"))
        || tokenLower.contains(QLatin1String("shared and split"))
        || tokenLower.contains(QLatin1String("split screen"))
        || tokenLower.contains(QLatin1String("lan pvp")) || tokenLower.contains(QLatin1String("lan co-op"))
        || tokenLower.contains(QStringLiteral("однопользовател"))
        || tokenLower.contains(QStringLiteral("мультиплеер"))
        || tokenLower.contains(QStringLiteral("кооп"));
}

bool isNonGenreFeatureToken(const QString& tokenLower)
{
    if (isPlayModeGenreToken(tokenLower))
        return true;
    static const QStringList features = {
        QStringLiteral("steam achievements"),
        QStringLiteral("steam cloud"),
        QStringLiteral("steam workshop"),
        QStringLiteral("steam trading cards"),
        QStringLiteral("steam leaderboards"),
        QStringLiteral("captions available"),
        QStringLiteral("full controller support"),
        QStringLiteral("partial controller support"),
        QStringLiteral("tracked controller support"),
        QStringLiteral("remote play"),
        QStringLiteral("remote play on phone"),
        QStringLiteral("remote play on tablet"),
        QStringLiteral("remote play on tv"),
        QStringLiteral("remote play together"),
        QStringLiteral("family sharing"),
        QStringLiteral("stats"),
        QStringLiteral("includes level editor"),
        QStringLiteral("includes source sdk"),
        QStringLiteral("commentary available"),
        QStringLiteral("valve anti-cheat enabled"),
        QStringLiteral("in-app purchases"),
        QStringLiteral("hdr available"),
        QStringLiteral("custom volume controls"),
        QStringLiteral("keyboard only option"),
        QStringLiteral("playable without timed input"),
        QStringLiteral("stereo sound"),
        QStringLiteral("surround sound"),
        QStringLiteral("camera comfort"),
        QStringLiteral("color alternatives"),
        QStringLiteral("adjustable difficulty"),
        QStringLiteral("save anytime"),
        QStringLiteral("touch-friendly"),
        QStringLiteral("photo editing"),
        QStringLiteral("audio production"),
        QStringLiteral("video production"),
        QStringLiteral("utilities"),
        QStringLiteral("web publishing"),
        QStringLiteral("animation & modeling"),
        QStringLiteral("design & illustration"),
        QStringLiteral("education"),
        QStringLiteral("software training"),
        QStringLiteral("game development"),
        QStringLiteral("accounting"),
        QStringLiteral("drm"),
    };
    for (const QString& f : features) {
        if (tokenLower == f || tokenLower.contains(f))
            return true;
    }
    if (tokenLower.startsWith(QLatin1String("genre ")) && tokenLower.size() < 12)
        return true;
    return false;
}

QString canonicalizeGenreToken(const QString& rawToken)
{
    const QString trimmed = rawToken.trimmed();
    if (trimmed.isEmpty())
        return {};
    const QString lower = trimmed.toLower();
    if (isNonGenreFeatureToken(lower))
        return {};
    const auto it = genreAliasMap().constFind(lower);
    if (it != genreAliasMap().cend())
        return it.value();
    return {};
}

QStringList canonicalizeGenreTokens(const QStringList& rawTokens)
{
    QStringList keys;
    QHash<QString, bool> seen;
    for (const QString& raw : rawTokens) {
        const QString key = canonicalizeGenreToken(raw);
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key, true);
        keys.append(key);
    }
    return keys;
}

const QStringList& curatedGenreKeys()
{
    static const QStringList keys = {
        QStringLiteral("Action"),
        QStringLiteral("Adventure"),
        QStringLiteral("RPG"),
        QStringLiteral("Strategy"),
        QStringLiteral("Simulation"),
        QStringLiteral("Shooter"),
        QStringLiteral("Horror"),
        QStringLiteral("Indie"),
        QStringLiteral("Casual"),
        QStringLiteral("Sports"),
        QStringLiteral("Racing"),
        QStringLiteral("Puzzle"),
        QStringLiteral("Platformer"),
        QStringLiteral("Fighting"),
        QStringLiteral("Survival"),
        QStringLiteral("Open World"),
        QStringLiteral("Roguelike"),
        QStringLiteral("Visual Novel"),
        QStringLiteral("Card"),
        QStringLiteral("Early Access"),
        QStringLiteral("Free to Play"),
        QStringLiteral("Massively Multiplayer"),
        QStringLiteral("VR"),
    };
    return keys;
}

int curatedGenreBitIndex(const QString& canonicalKey)
{
    if (canonicalKey.isEmpty())
        return -1;
    static const QHash<QString, int> index = [] {
        QHash<QString, int> m;
        const QStringList& keys = curatedGenreKeys();
        m.reserve(keys.size());
        for (int i = 0; i < keys.size(); ++i)
            m.insert(keys.at(i), i);
        return m;
    }();
    return index.value(canonicalKey, -1);
}

quint32 curatedGenreBit(const QString& canonicalKey)
{
    const int idx = curatedGenreBitIndex(canonicalKey);
    if (idx < 0 || idx >= 32)
        return 0;
    return quint32(1) << idx;
}

quint32 genreBitsFromTokens(const QStringList& rawTokens)
{
    quint32 bits = 0;
    for (const QString& raw : rawTokens) {
        const int idx = curatedGenreBitIndex(canonicalizeGenreToken(raw));
        if (idx >= 0)
            bits |= (quint32(1) << idx);
    }
    return bits;
}

QStringList curatedGenreKeysFromBits(quint32 bits)
{
    QStringList keys;
    const QStringList& all = curatedGenreKeys();
    keys.reserve(all.size());
    for (int i = 0; i < all.size(); ++i) {
        if (bits & (quint32(1) << i))
            keys.append(all.at(i));
    }
    return keys;
}

QString genreLabelsFromBits(quint32 bits)
{
    QStringList labels;
    const QStringList& keys = curatedGenreKeys();
    labels.reserve(keys.size());
    for (int i = 0; i < keys.size(); ++i) {
        if (bits & (quint32(1) << i))
            labels.append(genreDisplayLabel(keys.at(i)));
    }
    return labels.join(QLatin1String(", "));
}

QString genreDisplayLabel(const QString& canonicalKey)
{
    if (canonicalKey.isEmpty())
        return {};
    static const QHash<QString, const char*> trKeys = {
        {QStringLiteral("Action"), QT_TRANSLATE_NOOP("Core", "Action")},
        {QStringLiteral("Adventure"), QT_TRANSLATE_NOOP("Core", "Adventure")},
        {QStringLiteral("RPG"), QT_TRANSLATE_NOOP("Core", "RPG")},
        {QStringLiteral("Strategy"), QT_TRANSLATE_NOOP("Core", "Strategy")},
        {QStringLiteral("Simulation"), QT_TRANSLATE_NOOP("Core", "Simulation")},
        {QStringLiteral("Sports"), QT_TRANSLATE_NOOP("Core", "Sports")},
        {QStringLiteral("Racing"), QT_TRANSLATE_NOOP("Core", "Racing")},
        {QStringLiteral("Indie"), QT_TRANSLATE_NOOP("Core", "Indie")},
        {QStringLiteral("Casual"), QT_TRANSLATE_NOOP("Core", "Casual")},
        {QStringLiteral("Horror"), QT_TRANSLATE_NOOP("Core", "Horror")},
        {QStringLiteral("Puzzle"), QT_TRANSLATE_NOOP("Core", "Puzzle")},
        {QStringLiteral("Shooter"), QT_TRANSLATE_NOOP("Core", "Shooter")},
        {QStringLiteral("Platformer"), QT_TRANSLATE_NOOP("Core", "Platformer")},
        {QStringLiteral("Fighting"), QT_TRANSLATE_NOOP("Core", "Fighting")},
        {QStringLiteral("Survival"), QT_TRANSLATE_NOOP("Core", "Survival")},
        {QStringLiteral("Open World"), QT_TRANSLATE_NOOP("Core", "Open World")},
        {QStringLiteral("Visual Novel"), QT_TRANSLATE_NOOP("Core", "Visual Novel")},
        {QStringLiteral("Card"), QT_TRANSLATE_NOOP("Core", "Card")},
        {QStringLiteral("Roguelike"), QT_TRANSLATE_NOOP("Core", "Roguelike")},
        {QStringLiteral("Early Access"), QT_TRANSLATE_NOOP("Core", "Early Access")},
        {QStringLiteral("Free to Play"), QT_TRANSLATE_NOOP("Core", "Free to Play")},
        {QStringLiteral("Massively Multiplayer"),
         QT_TRANSLATE_NOOP("Core", "Massively Multiplayer")},
        {QStringLiteral("VR"), QT_TRANSLATE_NOOP("Core", "VR")},
    };
    const auto it = trKeys.constFind(canonicalKey);
    if (it != trKeys.cend())
        return QCoreApplication::translate("Core", it.value());
    return canonicalKey;
}

} // namespace arachnel::core
