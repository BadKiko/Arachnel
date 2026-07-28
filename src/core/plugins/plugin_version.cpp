#include "plugin_version.h"

#include <QRegularExpression>

namespace arachnel::core {

namespace {

struct ParsedAppVersion {
    QList<int> nums;
    QString suffix; // optional letter(s) after the numeric part, e.g. "b" in 0.1.30b
};

ParsedAppVersion parseAppVersion(const QString& raw)
{
    ParsedAppVersion out;
    QString s = raw.trimmed();
    if (s.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        s = s.mid(1).trimmed();
    static const QRegularExpression re(
        QStringLiteral(R"(^(\d+(?:\.\d+)*)([A-Za-z]*))"));
    const QRegularExpressionMatch m = re.match(s);
    if (!m.hasMatch())
        return out;
    const QStringList parts = m.captured(1).split(QLatin1Char('.'));
    out.nums.reserve(parts.size());
    for (const QString& p : parts)
        out.nums.append(p.toInt());
    out.suffix = m.captured(2).toLower();
    return out;
}

} // namespace

int compareAppVersions(const QString& left, const QString& right)
{
    const ParsedAppVersion a = parseAppVersion(left);
    const ParsedAppVersion b = parseAppVersion(right);
    const int n = qMax(a.nums.size(), b.nums.size());
    for (int i = 0; i < n; ++i) {
        const int av = i < a.nums.size() ? a.nums.at(i) : 0;
        const int bv = i < b.nums.size() ? b.nums.at(i) : 0;
        if (av != bv)
            return av < bv ? -1 : 1;
    }
    if (a.suffix == b.suffix)
        return 0;
    // No suffix sorts before letter suffixes (0.1.34 < 0.1.34a).
    if (a.suffix.isEmpty() != b.suffix.isEmpty())
        return a.suffix.isEmpty() ? -1 : 1;
    return a.suffix < b.suffix ? -1 : 1;
}

bool appVersionInRange(const QString& version, const QString& minVersion,
                       const QString& maxVersion)
{
    const QString ver = version.trimmed();
    // Local/dev builds report "dev" - don't filter them out of the store.
    if (ver.isEmpty() || ver.compare(QStringLiteral("dev"), Qt::CaseInsensitive) == 0)
        return true;
    const QString minV = minVersion.trimmed().isEmpty() ? QStringLiteral("0.0.0")
                                                        : minVersion.trimmed();
    if (compareAppVersions(ver, minV) < 0)
        return false;
    const QString maxV = maxVersion.trimmed();
    if (!maxV.isEmpty() && compareAppVersions(ver, maxV) > 0)
        return false;
    return true;
}

int comparePluginVersions(const QString& left, const QString& right)
{
    // Reuse the same numeric/suffix rules for plugin package versions.
    return compareAppVersions(left, right);
}

} // namespace arachnel::core
