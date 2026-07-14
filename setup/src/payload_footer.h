#pragma once

#include <cstdint>

namespace arachnel::setup {

inline constexpr char kPayloadMagic[] = "ARACHPK1";
inline constexpr int kPayloadMagicSize = 8;
inline constexpr int kPayloadFooterSize = 40; // magic + 4 x uint64

struct PayloadFooter {
    bool valid = false;
    std::uint64_t runtimeOffset = 0;
    std::uint64_t runtimeSize = 0;
    std::uint64_t appOffset = 0;
    std::uint64_t appSize = 0;
};

} // namespace arachnel::setup
