#pragma once

#include <functional>

namespace arachnel::core {

class JobStore;
class LibraryStore;

class LibraryMaintenanceService
{
public:
    struct Hooks {
        std::function<void()> syncLibrary;
        std::function<void()> restorePlaceholders;
        std::function<void()> reconcileInstallState;
        std::function<void()> retryPendingInstalls;
    };

    LibraryMaintenanceService(LibraryStore* library, JobStore* jobs, Hooks hooks);
    void migratePollutedEntryIds();
    void pruneBrokenLibraryEntries();
    void pruneCancelledAddonJobs();
    void runStartupMaintenance();

private:
    LibraryStore* m_library;
    JobStore* m_jobs;
    Hooks m_hooks;
};

} // namespace arachnel::core
