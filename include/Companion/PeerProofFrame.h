#pragma once
#include "Companion/CommandFrame.h"
#include "Companion/PeerProof.h"
namespace larp {
namespace companion {
FrameDecodeResult decodePeerChallengeFrame(const char *, std::size_t, CommandFrame &) noexcept;
FrameDecodeResult decodePeerProofFrame(const char *, std::size_t, CommandFrame &) noexcept;
bool encodePeerChallengeFrame(std::uint16_t, std::uint16_t, std::uint16_t, const char *,
                              std::size_t, char *, std::size_t, std::size_t &) noexcept;
bool encodePeerProofFrame(std::uint16_t, std::uint16_t, std::uint16_t, const char *, std::size_t,
                          char *, std::size_t, std::size_t &) noexcept;
} // namespace companion
} // namespace larp
