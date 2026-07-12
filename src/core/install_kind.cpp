#include "install_kind.h"

namespace arachnel::core {

QString installKindLabel(InstallKind kind)
{
    switch (kind) {
    case InstallKind::PortableArchive:
        return QStringLiteral("Портабл");
    case InstallKind::Installer:
        return QStringLiteral("Установщик");
    case InstallKind::BundledFix:
        return QStringLiteral("Bundled fix");
    case InstallKind::FixDownload:
        return QStringLiteral("Separate fix");
    }
    return QStringLiteral("Unknown");
}

} // namespace arachnel::core
