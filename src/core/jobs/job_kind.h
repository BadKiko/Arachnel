#pragma once

#include <QString>

namespace arachnel::core {

enum class JobKind {
    Download = 0,
    Install,
    Update,
    Move,
};

QString jobKindLabel(JobKind kind);

} // namespace arachnel::core
