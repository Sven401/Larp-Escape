#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
// Real host mutual exclusion, not an emulation of ESP32 interrupts/spin timing.
struct portMUX_TYPE {
    std::mutex mutex;
};
#define portMUX_INITIALIZER_UNLOCKED                                                               \
    {                                                                                              \
    }
namespace host_rtos {
extern thread_local unsigned depth;
extern thread_local std::function<void(portMUX_TYPE *)> *afterEnter, *onContended;
extern std::atomic<unsigned> lockedCalls;
extern bool nonzeroWait;
inline void noteApiCall() {
    if (depth)
        ++lockedCalls;
}
inline void enter(portMUX_TYPE *value) {
    if (!value->mutex.try_lock()) {
        if (onContended)
            (*onContended)(value);
        value->mutex.lock();
    }
    ++depth;
    if (afterEnter) {
        const auto hook = afterEnter;
        afterEnter = nullptr;
        (*hook)(value);
    }
}
inline void leave(portMUX_TYPE *value) {
    --depth;
    value->mutex.unlock();
}
} // namespace host_rtos
#define portENTER_CRITICAL(mutex) host_rtos::enter(mutex)
#define portEXIT_CRITICAL(mutex) host_rtos::leave(mutex)
constexpr int pdTRUE = 1, pdFALSE = 0;
