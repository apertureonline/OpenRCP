#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "openrcp_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONTROL_EVENT_NONE = 0,

    CONTROL_EVENT_PANEL_BUTTON_PRESS,
    CONTROL_EVENT_PANEL_BUTTON_RELEASE,

    CONTROL_EVENT_CAMERA_BUTTON_PRESS,
    CONTROL_EVENT_CAMERA_BUTTON_RELEASE,

    CONTROL_EVENT_ENCODER_CW,
    CONTROL_EVENT_ENCODER_CCW,
    CONTROL_EVENT_ENCODER_PRESS,
    CONTROL_EVENT_ENCODER_RELEASE,
} control_event_type_t;

typedef struct {
    control_event_type_t type;
    uint8_t device_id;
    uint8_t control_id;
    int32_t value;
} control_event_t;

// Changed: controls now reuses an existing I2C bus handle
esp_err_t controls_init(i2c_master_bus_handle_t shared_bus);

bool controls_get_event(control_event_t *event, uint32_t timeout_ms);
void controls_task(void *arg);

#ifdef __cplusplus
}
#endif
