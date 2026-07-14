#pragma once

#include "payload_footer.h"

#include <filesystem>

namespace arachnel::setup {

PayloadFooter readPayloadFooterFile(const std::filesystem::path& filePath);

} // namespace arachnel::setup
