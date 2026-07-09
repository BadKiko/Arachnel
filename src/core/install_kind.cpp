#include "install_kind.h"

namespace arachnel::core {

QString installKindLabel(InstallKind kind)
{
    switch (kind) {
    case InstallKind::PortableArchive:
        return QStringLiteral("Portable");
    case InstallKind::Installer:
        return QStringLiteral("Installer");
    case InstallKind::BundledFix:
        return QStringLiteral("Bundled fix");
    case InstallKind::FixDownload:
        return QStringLiteral("Separate fix");
    }
    return QStringLiteral("Unknown");
}

} // namespace arachnel::core
