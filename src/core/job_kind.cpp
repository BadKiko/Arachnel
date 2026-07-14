#include "job_kind.h"

#include <QCoreApplication>

namespace arachnel::core {

QString jobKindLabel(JobKind kind)
{
    switch (kind) {
    case JobKind::Download:
        return QCoreApplication::translate("Core", "Download");
    case JobKind::Install:
        return QCoreApplication::translate("Core", "Install");
    case JobKind::Update:
        return QCoreApplication::translate("Core", "Update");
    }
    return QCoreApplication::translate("Core", "Task");
}

} // namespace arachnel::core
