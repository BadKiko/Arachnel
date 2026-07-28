#pragma once

#include <QString>
#include <QStringList>

namespace arachnel::core {

/** True for Steam categories that describe play mode, not genre. */
bool isPlayModeGenreToken(const QString& tokenLower);

/** True for feature/category tags that should not appear in genre filters. */
bool isNonGenreFeatureToken(const QString& tokenLower);

/**
 * Map a raw Steam/catalog token to a canonical English genre key
 * (Action, Adventure, RPG, …). Empty if not a genre.
 */
QString canonicalizeGenreToken(const QString& rawToken);

/** Unique canonical keys from raw genre tokens (order preserved). */
QStringList canonicalizeGenreTokens(const QStringList& rawTokens);

/** Curated genre keys shown first in the filter sheet. */
QStringList curatedGenreKeys();

/** Localized display label for a canonical genre key (falls back to key). */
QString genreDisplayLabel(const QString& canonicalKey);

} // namespace arachnel::core
