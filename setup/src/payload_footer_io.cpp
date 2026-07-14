#include "payload_footer_io.h"

#include <cstring>
#include <fstream>

namespace arachnel::setup {

PayloadFooter readPayloadFooterFile(const std::filesystem::path& filePath)
{
    PayloadFooter footer;
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return footer;

    file.seekg(0, std::ios::end);
    const auto endPos = file.tellg();
    if (endPos < 0)
        return footer;
    const auto fileSize = static_cast<std::uint64_t>(endPos);
    if (fileSize < static_cast<std::uint64_t>(kPayloadFooterSize))
        return footer;

    file.seekg(static_cast<std::streamoff>(fileSize - static_cast<std::uint64_t>(kPayloadFooterSize)));

    char magic[kPayloadMagicSize] = {};
    if (!file.read(magic, kPayloadMagicSize))
        return footer;
    if (std::strncmp(magic, kPayloadMagic, kPayloadMagicSize) != 0)
        return footer;

    auto readU64 = [&file]() -> std::uint64_t {
        unsigned char buf[8] = {};
        if (!file.read(reinterpret_cast<char*>(buf), 8))
            return 0;
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i)
            value |= static_cast<std::uint64_t>(buf[i]) << (8 * i);
        return value;
    };

    footer.runtimeOffset = readU64();
    footer.runtimeSize = readU64();
    footer.appOffset = readU64();
    footer.appSize = readU64();

    const auto size = static_cast<std::uint64_t>(fileSize);
    footer.valid = footer.appSize > 0 && footer.appOffset > 0
                   && footer.appOffset + footer.appSize <= size;
    if (footer.runtimeSize > 0) {
        footer.valid = footer.valid && footer.runtimeOffset > 0
                       && footer.runtimeOffset + footer.runtimeSize <= size;
    }
    return footer;
}

} // namespace arachnel::setup
