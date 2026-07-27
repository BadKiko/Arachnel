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
        QStringLiteral("vr support"),
        QStringLiteral("vr only"),
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
    // Title-case unknown short genre-like tokens (avoid dumping long feature strings).
    if (trimmed.size() > 32 || trimmed.contains(QLatin1Char('(')))
        return {};
    return trimmed;
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

QStringList curatedGenreKeys()
{
    return {
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
    };
}

QString genreDisplayLabel(const QString& canonicalKey)
{
    if (canonicalKey.isEmpty())
        return {};
    static const QHash<QString, const char*> trKeys = {
        {QStringLiteral("Action"), "Action"},
        {QStringLiteral("Adventure"), "Adventure"},
        {QStringLiteral("RPG"), "RPG"},
        {QStringLiteral("Strategy"), "Strategy"},
        {QStringLiteral("Simulation"), "Simulation"},
        {QStringLiteral("Sports"), "Sports"},
        {QStringLiteral("Racing"), "Racing"},
        {QStringLiteral("Indie"), "Indie"},
        {QStringLiteral("Casual"), "Casual"},
        {QStringLiteral("Horror"), "Horror"},
        {QStringLiteral("Puzzle"), "Puzzle"},
        {QStringLiteral("Shooter"), "Shooter"},
        {QStringLiteral("Platformer"), "Platformer"},
        {QStringLiteral("Fighting"), "Fighting"},
        {QStringLiteral("Survival"), "Survival"},
        {QStringLiteral("Open World"), "Open World"},
        {QStringLiteral("Visual Novel"), "Visual Novel"},
        {QStringLiteral("Card"), "Card"},
        {QStringLiteral("Roguelike"), "Roguelike"},
        {QStringLiteral("Early Access"), "Early Access"},
        {QStringLiteral("Free to Play"), "Free to Play"},
        {QStringLiteral("Massively Multiplayer"), "Massively Multiplayer"},
    };
    const auto it = trKeys.constFind(canonicalKey);
    if (it != trKeys.cend())
        return QCoreApplication::translate("Core", it.value());
    return canonicalKey;
}

} // namespace arachnel::core
