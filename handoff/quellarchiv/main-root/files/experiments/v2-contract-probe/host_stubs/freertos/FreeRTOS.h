#pragma once
#include <cstddef>
#include <cstdint>
using portMUX_TYPE = unsigned;
#define portMUX_INITIALIZER_UNLOCKED 0
namespace host_rtos {
extern void (*beforeEnter)();
extern unsigned depth;
extern bool nonzeroWait;
inline void enter(portMUX_TYPE *) {
    if (!depth && beforeEnter) {
        auto hook = beforeEnter;
        beforeEnter = nullptr;
        hook();
    }
    ++depth;
}
inline void leave(portMUX_TYPE *) {
    --depth;
}
} // namespace host_rtos
#define portENTER_CRITICAL(mutex) host_rtos::enter(mutex)
#define portEXIT_CRITICAL(mutex) host_rtos::leave(mutex)
constexpr int pdTRUE = 1, pdFALSE = 0;
