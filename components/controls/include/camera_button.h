#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "controls.h"
#include "controls_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t button_id;
    camera_button_config_t cfg;
    volatile bool edge_pending;
    int last_level;
    uint32_t last_edge_tick_ms;
} camera_button_t;

esp_err_t camera_buttons_init(i2c_master_bus_handle_t bus, camera_button_t *buttons, uint8_t count);
void camera_buttons_process(camera_button_t *buttons, uint8_t count, void (*emit)(const control_event_t *ev));

#ifdef __cplusplus
}
#endif
