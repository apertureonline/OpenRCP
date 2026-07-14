#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

void controls_set_task_handle(TaskHandle_t handle);
void controls_notify_from_isr(void);

#ifdef __cplusplus
}
#endif