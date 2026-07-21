#include "core_controller_impl.h"

namespace arachnel::core {

bool CoreController::hasPendingCrashReport() const
{
    return arachnel::hasPendingCrashReport();
}

QString CoreController::pendingCrashSummary() const
{
    return arachnel::pendingCrashSummary();
}

QString CoreController::pendingCrashDetails() const
{
    return arachnel::pendingCrashDetails();
}

QString CoreController::pendingCrashReportPath() const
{
    return arachnel::pendingCrashReportPath();
}

void CoreController::dismissPendingCrashReport()
{
    arachnel::dismissPendingCrashReport();
}

void CoreController::openPendingCrashIssue()
{
    arachnel::openPendingCrashIssue();
}

void CoreController::revealPendingCrashReport()
{
    arachnel::revealPendingCrashReport();
}

void CoreController::copyPendingCrashReport()
{
    if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clipboard = gui->clipboard())
            clipboard->setText(pendingCrashDetails());
    }
}

} // namespace arachnel::core
