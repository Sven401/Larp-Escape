#pragma once
#include "FreeRTOS.h"
// Single-threaded static-mutex model, not an SMP/SDK layout measurement.
struct StaticSemaphore_t {
    bool taken = false;
};
using SemaphoreHandle_t = StaticSemaphore_t *;
namespace host_mutex {
extern bool failCreate, forceBusy;
extern unsigned takes, gives, creates;
} // namespace host_mutex
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) {
    ++host_mutex::creates;
    return host_mutex::failCreate ? nullptr : storage;
}
inline int xSemaphoreTake(SemaphoreHandle_t handle, unsigned wait) {
    ++host_mutex::takes;
    if (wait)
        host_rtos::nonzeroWait = true;
    if (!handle || handle->taken || host_mutex::forceBusy)
        return pdFALSE;
    handle->taken = true;
    return pdTRUE;
}
inline int xSemaphoreGive(SemaphoreHandle_t handle) {
    ++host_mutex::gives;
    if (!handle || !handle->taken)
        return pdFALSE;
    handle->taken = false;
    return pdTRUE;
}
