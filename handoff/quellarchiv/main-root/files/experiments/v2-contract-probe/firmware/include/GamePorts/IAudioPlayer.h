#pragma once

#include <cstdint>

namespace larp {
namespace ports {
class IAudioPlayer {
  public:
    virtual ~IAudioPlayer() {}

    virtual void playTrack(std::uint16_t trackNumber) = 0;
    virtual bool isBusy() = 0;
    virtual void wakeup() = 0;
};
} // namespace ports
} // namespace larp
