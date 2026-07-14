#include "install_kind.h"

#include <QCoreApplication>

namespace arachnel::core {

QString installKindLabel(InstallKind kind)
{
    switch (kind) {
    case InstallKind::PortableArchive:
        return QCoreApplication::translate("Core", "Portable");
    case InstallKind::Installer:
        return QCoreApplication::translate("Core", "Installer");
    case InstallKind::BundledFix:
        return QCoreApplication::translate("Core", "Bundled fix");
    case InstallKind::FixDownload:
        return QCoreApplication::translate("Core", "Separate fix");
    }
    return QCoreApplication::translate("Core", "Unknown");
}

} // namespace arachnel::core
