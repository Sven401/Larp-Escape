#pragma once
#include "FreeRTOS.h"
using TaskHandle_t = void *;
namespace host_sdk_source {
extern TaskHandle_t currentTask;
}
inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    host_rtos::noteApiCall();
    return host_sdk_source::currentTask;
}
