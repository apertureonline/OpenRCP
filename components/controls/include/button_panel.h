#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "controls.h"
#include "controls_config.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t panel_id;
    button_panel_config_t cfg;
    i2c_master_dev_handle_t dev_handle;
    bool active;
    volatile bool int_pending;
    uint8_t last_raw;
} button_panel_t;

esp_err_t button_panels_init(i2c_master_bus_handle_t bus, button_panel_t *panels, uint8_t count);
void button_panels_process(button_panel_t *panels, uint8_t count, void (*emit)(const control_event_t *ev));

#ifdef __cplusplus
}
#endif
