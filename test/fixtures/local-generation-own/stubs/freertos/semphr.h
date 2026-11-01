#pragma once
#include "FreeRTOS.h"
extern "C" SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *);
extern "C" int xSemaphoreTake(SemaphoreHandle_t, int);
extern "C" int xSemaphoreGive(SemaphoreHandle_t);
