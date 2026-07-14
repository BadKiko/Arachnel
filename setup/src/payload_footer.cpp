#include "payload_footer_qt.h"

#include "payload_footer_io.h"

#include <QFile>

namespace arachnel::setup {

PayloadFooter readPayloadFooter(const QString& filePath)
{
    return readPayloadFooterFile(filePath.toStdWString());
}

} // namespace arachnel::setup
