#include "job_kind.h"

#include "i18n.h"

namespace arachnel::core {

QString jobKindLabel(JobKind kind)
{
    switch (kind) {
    case JobKind::Download:
        return trCore("Download");
    case JobKind::Install:
        return trCore("Install");
    case JobKind::Update:
        return trCore("Update");
    }
    return trCore("Task");
}

} // namespace arachnel::core
