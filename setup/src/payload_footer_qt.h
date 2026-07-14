#pragma once

#include "payload_footer.h"

#include <QString>

namespace arachnel::setup {

PayloadFooter readPayloadFooter(const QString& filePath);

} // namespace arachnel::setup
