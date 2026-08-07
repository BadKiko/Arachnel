#pragma once

#include <QString>

namespace arachnel::core {

struct BookmarkEntry {
    QString id;
    QString title;
    QString coverUrl;
    QString sourceName;
};

} // namespace arachnel::core
