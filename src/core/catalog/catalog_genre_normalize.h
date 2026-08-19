#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace arachnel::core {

/** True for Steam categories that describe play mode, not genre. */
bool isPlayModeGenreToken(const QString& tokenLower);

/** True for feature/category tags that should not appear in genre filters. */
bool isNonGenreFeatureToken(const QString& tokenLower);

/**
 * Map a raw Steam/catalog token to a canonical English genre key
 * (Action, Adventure, RPG, …). Empty if not a curated genre.
 */
QString canonicalizeGenreToken(const QString& rawToken);

/** Unique canonical keys from raw genre tokens (order preserved). */
QStringList canonicalizeGenreTokens(const QStringList& rawTokens);

/** Curated genre keys in bit-index order (Action = bit 0). */
const QStringList& curatedGenreKeys();

/** Bit index for a canonical key, or -1. */
int curatedGenreBitIndex(const QString& canonicalKey);

/** Single-bit mask for a canonical key, or 0. */
quint32 curatedGenreBit(const QString& canonicalKey);

/** Curated genre bits from raw catalog/Steam tokens. */
quint32 genreBitsFromTokens(const QStringList& rawTokens);

/** Canonical keys whose bits are set (curated order). */
QStringList curatedGenreKeysFromBits(quint32 bits);

/** Localized labels joined with ", " for UI / details. */
QString genreLabelsFromBits(quint32 bits);

/** Localized display label for a canonical genre key (falls back to key). */
QString genreDisplayLabel(const QString& canonicalKey);

} // namespace arachnel::core
