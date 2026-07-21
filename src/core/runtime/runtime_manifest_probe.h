#pragma once

#include "runtime_dependency_types.h"

#include <QString>

namespace arachnel::core {

struct ManifestRuntimeNeeds {
    bool needsVc2015x64 = false;
    bool needsVc2015x86 = false;
};

ManifestRuntimeNeeds probeExecutableManifest(const QString& executablePath);

QVector<RuntimeDepotRef> depotsFromManifestNeeds(const ManifestRuntimeNeeds& needs);

} // namespace arachnel::core
