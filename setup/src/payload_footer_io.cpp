#include "payload_footer_io.h"

#include "win_container_io.h"

#include <cstring>

namespace arachnel::setup {

PayloadFooter readPayloadFooterFile(const std::filesystem::path& filePath)
{
    PayloadFooter footer;

    const HANDLE file = openContainerForRead(filePath);
    if (file == INVALID_HANDLE_VALUE)
        return footer;

    std::uint64_t fileSize = 0;
    if (!queryContainerSize(file, &fileSize)
        || fileSize < static_cast<std::uint64_t>(kPayloadFooterSize)) {
        CloseHandle(file);
        return footer;
    }

    const std::uint64_t footerOffset =
        fileSize - static_cast<std::uint64_t>(kPayloadFooterSize);

    char magic[kPayloadMagicSize] = {};
    if (!readContainerAt(file, footerOffset, magic, kPayloadMagicSize)) {
        CloseHandle(file);
        return footer;
    }
    if (std::strncmp(magic, kPayloadMagic, kPayloadMagicSize) != 0) {
        CloseHandle(file);
        return footer;
    }

    unsigned char fields[32] = {};
    if (!readContainerAt(file, footerOffset + kPayloadMagicSize, fields, sizeof(fields))) {
        CloseHandle(file);
        return footer;
    }
    CloseHandle(file);

    auto readU64 = [&](int index) -> std::uint64_t {
        std::uint64_t value = 0;
        const int offset = index * 8;
        for (int i = 0; i < 8; ++i)
            value |= static_cast<std::uint64_t>(fields[offset + i]) << (8 * i);
        return value;
    };

    footer.runtimeOffset = readU64(0);
    footer.runtimeSize = readU64(1);
    footer.appOffset = readU64(2);
    footer.appSize = readU64(3);

    footer.valid = footer.appSize > 0 && footer.appOffset > 0
                   && footer.appOffset + footer.appSize <= fileSize;
    if (footer.runtimeSize > 0) {
        footer.valid = footer.valid && footer.runtimeOffset > 0
                       && footer.runtimeOffset + footer.runtimeSize <= fileSize;
    }
    return footer;
}

} // namespace arachnel::setup
