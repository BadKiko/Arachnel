namespace {

QString cacheFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/metadata-cache.json");
}

QString normalizeTitle(QString title)
{
    title = title.toLower();
    QString out;
    out.reserve(title.size());
    for (const QChar c : title) {
        if (c.isLetterOrNumber())
            out.append(c);
    }
    return out;
}

bool isTitleStopWord(const QString& word)
{
    static const QSet<QString> kStop{
        QStringLiteral("a"),      QStringLiteral("an"),     QStringLiteral("and"),
        QStringLiteral("or"),     QStringLiteral("of"),     QStringLiteral("the"),
        QStringLiteral("for"),    QStringLiteral("to"),     QStringLiteral("in"),
        QStringLiteral("on"),     QStringLiteral("at"),     QStringLiteral("by"),
        QStringLiteral("with"),   QStringLiteral("from"),   QStringLiteral("edition"),
        QStringLiteral("pc"),
    };
    return kStop.contains(word.toLower());
}

QStringList significantTitleTokens(const QString& title)
{
    QString cleaned = title.toLower();
    cleaned.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+"),
                                       QRegularExpression::UseUnicodePropertiesOption),
                    QStringLiteral(" "));
    const QStringList raw = cleaned.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList tokens;
    tokens.reserve(raw.size());
    for (const QString& word : raw) {
        if (word.size() < 2 || isTitleStopWord(word))
            continue;
        tokens.append(word);
    }
    return tokens;
}

int titleTokenOverlapScore(const QString& wanted, const QString& candidate)
{
    const QStringList want = significantTitleTokens(wanted);
    const QStringList have = significantTitleTokens(candidate);
    if (want.isEmpty() || have.isEmpty())
        return -1;
    int matched = 0;
    for (const QString& token : want) {
        for (const QString& other : have) {
            if (other == token || other.startsWith(token) || token.startsWith(other)) {
                ++matched;
                break;
            }
        }
    }
    // 0..1000 scale - require most distinctive words to align.
    return (matched * 1000) / want.size();
}

int titleMatchScore(const QString& wanted, const QString& candidate)
{
    const QString a = normalizeTitle(wanted);
    const QString b = normalizeTitle(candidate);
    if (a.isEmpty() || b.isEmpty())
        return -1;
    if (a == b)
        return 1000;
    if (b.startsWith(a) || a.startsWith(b))
        return 800 - qAbs(a.size() - b.size());
    if (b.contains(a) || a.contains(b))
        return 500 - qAbs(a.size() - b.size());
    // Latin catalog title vs localized Steam name (or weak substring): use tokens.
    const int overlap = titleTokenOverlapScore(wanted, candidate);
    if (overlap >= 750)
        return 600 + (overlap / 10);
    return -1;
}

QString steamLanguageForUi(const QString& languageCode)
{
    const QString code = languageCode.trimmed().toLower();
    if (code == QStringLiteral("ru"))
        return QStringLiteral("russian");
    if (code == QStringLiteral("de"))
        return QStringLiteral("german");
    if (code == QStringLiteral("fr"))
        return QStringLiteral("french");
    if (code == QStringLiteral("es"))
        return QStringLiteral("spanish");
    if (code == QStringLiteral("zh") || code.startsWith(QStringLiteral("zh")))
        return QStringLiteral("schinese");
    return QStringLiteral("english");
}

QString pickTrailerUrl(const QJsonObject& movie)
{
    const QJsonObject mp4 = movie.value(QStringLiteral("mp4")).toObject();
    QString url = mp4.value(QStringLiteral("max")).toString();
    if (url.isEmpty())
        url = mp4.value(QStringLiteral("480")).toString();
    if (!url.isEmpty())
        return url;

    const QJsonObject webm = movie.value(QStringLiteral("webm")).toObject();
    url = webm.value(QStringLiteral("max")).toString();
    if (url.isEmpty())
        url = webm.value(QStringLiteral("480")).toString();
    if (!url.isEmpty())
        return url;

    url = movie.value(QStringLiteral("hls_h264")).toString();
    if (!url.isEmpty())
        return url;

    url = movie.value(QStringLiteral("dash_h264")).toString();
    return url;
}

QString buildSteamTrailerUrl(const QString& cdnPath)
{
    if (cdnPath.isEmpty())
        return {};
    return QStringLiteral("https://video.akamai.steamstatic.com/store_trailers/") + cdnPath;
}

QString pickTrailerFromStoreHighlight(const QJsonObject& highlight)
{
    const QJsonArray micro = highlight.value(QStringLiteral("microtrailer")).toArray();
    for (const QJsonValue& entry : micro) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("video/mp4"))
            continue;
        const QString url = buildSteamTrailerUrl(obj.value(QStringLiteral("filename")).toString());
        if (!url.isEmpty())
            return url;
    }
    for (const QJsonValue& entry : micro) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("video/webm"))
            continue;
        const QString url = buildSteamTrailerUrl(obj.value(QStringLiteral("filename")).toString());
        if (!url.isEmpty())
            return url;
    }

    const QJsonArray adaptive = highlight.value(QStringLiteral("adaptive_trailers")).toArray();
    for (const QJsonValue& entry : adaptive) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("encoding")).toString() != QStringLiteral("hls_h264"))
            continue;
        const QString url = buildSteamTrailerUrl(obj.value(QStringLiteral("cdn_path")).toString());
        if (!url.isEmpty())
            return url;
    }
    return {};
}

QString pickTrailerFromStoreTrailers(const QJsonObject& trailers)
{
    const QJsonArray highlights = trailers.value(QStringLiteral("highlights")).toArray();
    for (const QJsonValue& value : highlights) {
        const QString url = pickTrailerFromStoreHighlight(value.toObject());
        if (!url.isEmpty())
            return url;
    }
    const QJsonArray other = trailers.value(QStringLiteral("other_trailers")).toArray();
    if (!other.isEmpty())
        return pickTrailerFromStoreHighlight(other.first().toObject());
    return {};
}

QString pickTrailerFromMovies(const QJsonArray& movies)
{
    for (const QJsonValue& value : movies) {
        const QJsonObject movie = value.toObject();
        if (!movie.value(QStringLiteral("highlight")).toBool())
            continue;
        const QString url = pickTrailerUrl(movie);
        if (!url.isEmpty())
            return url;
    }
    if (movies.isEmpty())
        return {};
    return pickTrailerUrl(movies.first().toObject());
}

QString pickTrailerThumbnailFromMovies(const QJsonArray& movies)
{
    for (const QJsonValue& value : movies) {
        const QJsonObject movie = value.toObject();
        if (!movie.value(QStringLiteral("highlight")).toBool())
            continue;
        const QString url = movie.value(QStringLiteral("thumbnail")).toString();
        if (!url.isEmpty())
            return url;
    }
    if (movies.isEmpty())
        return {};
    return movies.first().toObject().value(QStringLiteral("thumbnail")).toString();
}

QStringList parseScreenshotUrls(const QJsonArray& screenshots, int maxCount = 8)
{
    QStringList urls;
    urls.reserve(maxCount);
    for (const QJsonValue& value : screenshots) {
        if (urls.size() >= maxCount)
            break;
        const QString url = value.toObject().value(QStringLiteral("path_full")).toString();
        if (!url.isEmpty())
            urls.append(url);
    }
    return urls;
}

bool hasCachedMedia(const GameMetadata& metadata)
{
    return !metadata.screenshotUrls.isEmpty() && !metadata.trailerUrl.isEmpty();
}

bool needsMediaRefresh(const GameMetadata& metadata)
{
    if (metadata.trailerUrl.isEmpty() || metadata.screenshotUrls.isEmpty())
        return true;
    // Older builds cached microtrailers on the image CDN (404).
    if (metadata.trailerUrl.contains(QStringLiteral("shared.akamai.steamstatic.com"))
        && metadata.trailerUrl.contains(QStringLiteral("microtrailer")))
        return true;
    return !metadata.trailerUrl.isEmpty() && metadata.trailerThumbnailUrl.isEmpty();
}

bool isVerticalLibraryCover(const QString& url)
{
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"));
}

QString buildAssetUrl(const QString& urlFormat, const QString& filename)
{
    if (filename.isEmpty())
        return {};
    QString path = urlFormat;
    path.replace(QStringLiteral("${FILENAME}"), filename);
    if (path.startsWith(QStringLiteral("http")))
        return path;
    return QStringLiteral("https://shared.akamai.steamstatic.com/store_item_assets/") + path;
}

QString pickTrailerThumbnailFromStoreHighlight(const QJsonObject& highlight)
{
    const QString format = highlight.value(QStringLiteral("trailer_url_format")).toString();
    const QString medium = highlight.value(QStringLiteral("screenshot_medium")).toString();
    QString url = buildAssetUrl(format, medium);
    if (!url.isEmpty())
        return url;
    return buildAssetUrl(format, highlight.value(QStringLiteral("screenshot_full")).toString());
}

QString pickTrailerThumbnailFromStoreTrailers(const QJsonObject& trailers)
{
    const QJsonArray highlights = trailers.value(QStringLiteral("highlights")).toArray();
    for (const QJsonValue& value : highlights) {
        const QString url = pickTrailerThumbnailFromStoreHighlight(value.toObject());
        if (!url.isEmpty())
            return url;
    }
    const QJsonArray other = trailers.value(QStringLiteral("other_trailers")).toArray();
    if (!other.isEmpty())
        return pickTrailerThumbnailFromStoreHighlight(other.first().toObject());
    return {};
}

QString pickLibraryCover(const QJsonObject& assets)
{
    const QString format = assets.value(QStringLiteral("asset_url_format")).toString();
    if (format.isEmpty())
        return {};
    for (const QString& key : {QStringLiteral("library_capsule"), QStringLiteral("library_capsule_2x")}) {
        const QString url = buildAssetUrl(format, assets.value(key).toString());
        if (!url.isEmpty())
            return url;
    }
    return {};
}

QJsonObject bestSearchItem(const QJsonArray& items, const QString& title)
{
    QJsonObject best;
    int bestScore = -1;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("app"))
            continue;
        const int score = titleMatchScore(title, item.value(QStringLiteral("name")).toString());
        if (score > bestScore) {
            bestScore = score;
            best = item;
        }
    }
    return best;
}

void appendUnique(QStringList& terms, const QString& term)
{
    const QString t = term.simplified();
    if (!t.isEmpty() && !terms.contains(t, Qt::CaseInsensitive))
        terms.append(t);
}

QStringList searchTermsFor(const QString& title)
{
    QStringList terms;
    appendUnique(terms, title);

    QString cleaned = title;
    cleaned.replace(QRegularExpression(QStringLiteral(R"(\([^)]*\))")), QString());
    cleaned.replace(QRegularExpression(QStringLiteral(R"(\[[^\]]*\])")), QString());
    cleaned = cleaned.simplified();
    appendUnique(terms, cleaned);

    QString repackStripped = cleaned;
    repackStripped.remove(QRegularExpression(QStringLiteral(R"(\|.*)"),
                                           QRegularExpression::DotMatchesEverythingOption));
    repackStripped.remove(
        QRegularExpression(QStringLiteral(R"(\s+PC\s*$)"), QRegularExpression::CaseInsensitiveOption));
    repackStripped.remove(
        QRegularExpression(QStringLiteral(R"(RePack.*)"), QRegularExpression::CaseInsensitiveOption));
    repackStripped = repackStripped.simplified();
    appendUnique(terms, repackStripped);

    if (cleaned.contains(QLatin1Char(':')))
        appendUnique(terms, cleaned.section(QLatin1Char(':'), 0, 0).trimmed());

    QString stripped = cleaned;
    stripped.replace(
        QRegularExpression(
            QStringLiteral(
                R"(\s+(alpha|beta|demo|epoch|mod|multiplayer|singleplayer|goty|)"
                R"(deluxe|definitive|remastered|complete|edition|pack|dlc|)"
                R"(director'?s\s+cut|directors\s+cut)\s*$)"),
            QRegularExpression::CaseInsensitiveOption),
        QString());
    stripped = stripped.simplified();
    appendUnique(terms, stripped);

    const QStringList words = stripped.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // Prefer "Ghost of Tsushima" over weak "Ghost of" / "Ghost" fallbacks that
    // match unrelated Steam apps when the UI language localizes the real hit.
    if (words.size() >= 3) {
        appendUnique(terms, words.mid(0, 3).join(QLatin1Char(' ')));
        if (isTitleStopWord(words.at(1)) && words.size() >= 3)
            appendUnique(terms, words.at(0) + QLatin1Char(' ') + words.at(2));
    } else if (words.size() == 2 && !isTitleStopWord(words.at(1))) {
        appendUnique(terms, words.at(0) + QLatin1Char(' ') + words.at(1));
    }
    return terms;
}

} // namespace
