#include "core_controller_impl.h"

namespace arachnel::core {

void CoreController::refreshCatalog(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->refreshCatalog(sourceId);
}

void CoreController::refreshSelectedCatalogs()
{
    if (m_catalogController)
        m_catalogController->refreshSelectedCatalogs();
}

} // namespace arachnel::core
