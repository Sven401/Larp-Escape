#pragma once
#include <cstring>
#include <freertos/FreeRTOS.h>
// Deterministic single-thread value-copy model. No SMP/timing evidence.
struct StaticQueue_t {
    std::uint8_t *bytes = nullptr;
    std::size_t length = 0, width = 0, head = 0, count = 0;
};
using QueueHandle_t = StaticQueue_t *;
inline QueueHandle_t xQueueCreateStatic(std::size_t length, std::size_t width, std::uint8_t *bytes,
                                        StaticQueue_t *control) {
    host_rtos::noteApiCall();
    control->bytes = bytes;
    control->length = length;
    control->width = width;
    control->head = control->count = 0;
    return control;
}
inline int xQueueSendToBack(QueueHandle_t queue, const void *value, unsigned wait) {
    host_rtos::noteApiCall();
    host_rtos::nonzeroWait = host_rtos::nonzeroWait || wait != 0;
    if (queue->count == queue->length)
        return pdFALSE;
    std::memcpy(queue->bytes + ((queue->head + queue->count) % queue->length) * queue->width, value,
                queue->width);
    ++queue->count;
    return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void *value, unsigned wait) {
    host_rtos::noteApiCall();
    host_rtos::nonzeroWait = host_rtos::nonzeroWait || wait != 0;
    if (!queue->count)
        return pdFALSE;
    std::memcpy(value, queue->bytes + queue->head * queue->width, queue->width);
    queue->head = (queue->head + 1) % queue->length;
    --queue->count;
    return pdTRUE;
}
inline int xQueuePeek(QueueHandle_t queue, void *value, unsigned wait) {
    host_rtos::noteApiCall();
    host_rtos::nonzeroWait = host_rtos::nonzeroWait || wait != 0;
    if (!queue->count)
        return pdFALSE;
    std::memcpy(value, queue->bytes + queue->head * queue->width, queue->width);
    return pdTRUE;
}
inline int xQueueOverwrite(QueueHandle_t queue, const void *value) {
    host_rtos::noteApiCall();
    if (queue->length != 1)
        return pdFALSE;
    std::memcpy(queue->bytes, value, queue->width);
    queue->head = 0;
    queue->count = 1;
    return pdTRUE;
}
