#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "openrcp_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BMD_SHIELD_MAX_TALLY_BYTES 255

esp_err_t bmd_shield_init(void);
bool bmd_shield_is_ready(void);

i2c_master_bus_handle_t bmd_shield_get_bus_handle(void);

esp_err_t bmd_shield_poll_tally(uint8_t *tally_bytes, size_t max_len, size_t *out_len);

// Camera control transmit helpers
esp_err_t bmd_shield_send_aperture_normalized(uint8_t camera_number_1_based, float normalized);
esp_err_t bmd_shield_send_gain_iso_code(uint8_t camera_number_1_based, int iso_code);
esp_err_t bmd_shield_send_manual_white_balance(uint8_t camera_number_1_based, int temperature_k, int tint);

// Shading / color correction
esp_err_t bmd_shield_send_lift(uint8_t camera_number_1_based,
                               float red, float green, float blue, float luma);

esp_err_t bmd_shield_send_gain_adjust(uint8_t camera_number_1_based,
                                      float red, float green, float blue, float luma);

esp_err_t bmd_shield_send_gamma_adjust(uint8_t camera_number_1_based,
                                       float red, float green, float blue, float luma);

esp_err_t bmd_shield_send_offset_adjust(uint8_t camera_number_1_based,
                                        float red, float green, float blue, float luma);

esp_err_t bmd_shield_send_contrast_adjust(uint8_t camera_number_1_based,
                                          float pivot, float adjust);

esp_err_t bmd_shield_send_luma_mix(uint8_t camera_number_1_based, float luma_mix);

esp_err_t bmd_shield_send_color_adjust(uint8_t camera_number_1_based, float hue, float saturation);

esp_err_t bmd_shield_send_color_correction_reset(uint8_t camera_number_1_based);

esp_err_t bmd_shield_send_camera_state(uint8_t camera_number_1_based, const openrcp_camera_state_t *cam);

#ifdef __cplusplus
}
#endif
