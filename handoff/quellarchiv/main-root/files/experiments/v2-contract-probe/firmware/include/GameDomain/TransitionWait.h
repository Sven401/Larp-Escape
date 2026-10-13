#ifndef TRANSITION_WAIT_H
#define TRANSITION_WAIT_H

#include <cstdint>

namespace larp {
namespace domain {
class TransitionWait {
  public:
    static bool elapsed(std::uint32_t now, bool &started, std::uint32_t &startedMilliseconds,
                        std::uint32_t intervalMilliseconds) {
        if (!started) {
            started = true;
            startedMilliseconds = now;
            return false;
        }
        if (static_cast<std::uint32_t>(now - startedMilliseconds) < intervalMilliseconds)
            return false;
        started = false;
        return true;
    }
};
} // namespace domain
} // namespace larp

#endif
