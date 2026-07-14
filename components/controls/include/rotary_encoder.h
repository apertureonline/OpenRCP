#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "controls.h"
#include "controls_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t encoder_id;
    rotary_encoder_config_t cfg;
    volatile bool edge_pending;
    uint8_t state;
    int last_sw;
    uint32_t last_sw_tick_ms;
} rotary_encoder_t;

esp_err_t rotary_encoders_init(rotary_encoder_t *encoders, uint8_t count);
void rotary_encoders_process(rotary_encoder_t *encoders, uint8_t count, void (*emit)(const control_event_t *ev));

#ifdef __cplusplus
}
#endif