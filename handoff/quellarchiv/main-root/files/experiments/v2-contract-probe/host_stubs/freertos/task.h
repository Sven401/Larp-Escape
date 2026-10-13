#pragma once
using TaskHandle_t = void *;
namespace host_sdk_source {
extern TaskHandle_t currentTask;
}
inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    return host_sdk_source::currentTask;
}
