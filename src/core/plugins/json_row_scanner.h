#pragma once

#include <QByteArray>
#include <QByteArrayList>
#include <QVector>
#include <QtGlobal>

namespace arachnel::core::jsonrows {

// Minimal JSON scanner: finds value boundaries without building a document tree. Shared by the
// catalog source sync and the plugin catalog parser so a 70 MB feed is never held as a DOM.

inline qsizetype skipValue(const char* p, qsizetype n, qsizetype i)
{
    if (i >= n)
        return -1;
    const char c = p[i];
    if (c == '"') {
        for (++i; i < n;) {
            if (p[i] == '\\')
                i += 2;
            else if (p[i] == '"')
                return i + 1;
            else
                ++i;
        }
        return -1;
    }
    if (c == '{' || c == '[') {
        int depth = 0;
        bool inString = false;
        for (; i < n; ++i) {
            const char ch = p[i];
            if (inString) {
                if (ch == '\\')
                    ++i;
                else if (ch == '"')
                    inString = false;
            } else if (ch == '"') {
                inString = true;
            } else if (ch == '{' || ch == '[') {
                ++depth;
            } else if (ch == '}' || ch == ']') {
                if (--depth == 0)
                    return i + 1;
            }
        }
        return -1;
    }
    while (i < n && p[i] != ',' && p[i] != ']' && p[i] != '}' && p[i] != ' ' && p[i] != '\n'
           && p[i] != '\r' && p[i] != '\t') {
        ++i;
    }
    return i;
}

inline qsizetype skipSpace(const char* p, qsizetype n, qsizetype i)
{
    while (i < n && (p[i] == ' ' || p[i] == '\n' || p[i] == '\r' || p[i] == '\t'))
        ++i;
    return i;
}

struct Range {
    qsizetype begin = -1; // just after '['
    qsizetype end = -1;   // at the matching ']'
    bool valid() const { return begin >= 0 && end >= begin; }
};

// Rows array: a top-level array, or the first non-empty one among `keys` in a top-level object.
inline bool locateRows(const char* p, qsizetype n, const QByteArrayList& keys, Range* out)
{
    qsizetype i = skipSpace(p, n, 0);
    if (i >= n)
        return false;
    const auto rangeOfArray = [&](qsizetype at) {
        Range r;
        const qsizetype stop = skipValue(p, n, at);
        if (stop > at) {
            r.begin = at + 1;
            r.end = stop - 1;
        }
        return r;
    };
    if (p[i] == '[') {
        *out = rangeOfArray(i);
        return out->valid();
    }
    if (p[i] != '{')
        return false;
    QVector<Range> found(keys.size());
    ++i;
    for (;;) {
        i = skipSpace(p, n, i);
        if (i >= n)
            return false;
        if (p[i] == '}')
            break;
        if (p[i] == ',') {
            ++i;
            continue;
        }
        if (p[i] != '"')
            return false;
        const qsizetype keyEnd = skipValue(p, n, i);
        if (keyEnd < 0)
            return false;
        const QByteArrayView key(p + i + 1, keyEnd - i - 2);
        i = skipSpace(p, n, keyEnd);
        if (i >= n || p[i] != ':')
            return false;
        i = skipSpace(p, n, i + 1);
        if (i >= n)
            return false;
        const qsizetype valueEnd = skipValue(p, n, i);
        if (valueEnd < 0)
            return false;
        if (p[i] == '[') {
            for (qsizetype k = 0; k < keys.size(); ++k) {
                if (key == QByteArrayView(keys.at(k)))
                    found[k] = rangeOfArray(i);
            }
        }
        i = valueEnd;
    }
    for (const Range& r : found) {
        if (r.valid() && skipSpace(p, n, r.begin) < r.end) {
            *out = r;
            return true;
        }
    }
    return false;
}

struct RowSpan {
    qsizetype begin;
    qsizetype end;
};

inline bool collectRowSpans(const char* p, qsizetype n, const Range& range, QVector<RowSpan>* spans)
{
    qsizetype i = range.begin;
    for (;;) {
        i = skipSpace(p, n, i);
        if (i >= range.end)
            return true;
        if (p[i] == ',') {
            ++i;
            continue;
        }
        const qsizetype stop = skipValue(p, n, i);
        if (stop < 0 || stop > range.end)
            return false;
        if (p[i] == '{')
            spans->append({i, stop});
        i = stop;
    }
}


} // namespace arachnel::core::jsonrows
