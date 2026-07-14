#pragma once

#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>

// -------- Camera buttons --------

#define CAMERA_BUTTON_COUNT          4

typedef struct {
    const char *name;
    gpio_num_t input_gpio;
} camera_button_config_t;

static const camera_button_config_t CAMERA_BUTTON_CONFIGS[CAMERA_BUTTON_COUNT] = {
    {
        .name = "camera_1",
        .input_gpio = GPIO_NUM_12,
    },
    {
        .name = "camera_2",
        .input_gpio = GPIO_NUM_10,
    },
    {
        .name = "camera_3",
        .input_gpio = GPIO_NUM_11,
    },
    {
        .name = "camera_4",
        .input_gpio = GPIO_NUM_9,
    }
};

// -------- Button panels --------

#define BUTTON_PANEL_COUNT  2
#define BUTTONS_PER_PANEL   6

typedef struct {
    const char *name;
    uint8_t i2c_address;
    gpio_num_t int_gpio;
    uint8_t button_bits[BUTTONS_PER_PANEL];
} button_panel_config_t;

static const button_panel_config_t BUTTON_PANEL_CONFIGS[BUTTON_PANEL_COUNT] = {
        {
        .name = "panel_right",
        .i2c_address = 0x23,
        .int_gpio = GPIO_NUM_6,
        .button_bits = {0, 1, 2, 3, 7, 6},
    },
        {
        .name = "panel_left",
        .i2c_address = 0x24,
        .int_gpio = GPIO_NUM_5,
        .button_bits = {0, 1, 2, 3, 7, 6},
    }


};

// -------- Rotary encoders --------

#define ROTARY_ENCODER_COUNT  7

typedef enum {
    ENCODER_TARGET_NONE = 0,
    ENCODER_TARGET_IRIS,
    ENCODER_TARGET_BLACK_LEVEL,
    ENCODER_TARGET_BLACK_R,
    ENCODER_TARGET_BLACK_G,
    ENCODER_TARGET_BLACK_B,
    ENCODER_TARGET_WHITE_R,
    ENCODER_TARGET_WHITE_G,
    ENCODER_TARGET_WHITE_B,
} encoder_target_t;

typedef struct {
    const char *name;
    gpio_num_t pin_a;
    gpio_num_t pin_b;
    gpio_num_t pin_sw;
    encoder_target_t target;
    bool invert_direction;
} rotary_encoder_config_t;

static const rotary_encoder_config_t ROTARY_ENCODER_CONFIGS[ROTARY_ENCODER_COUNT] = {
    {
        .name = "white_r",
        .pin_a = GPIO_NUM_43,
        .pin_b = GPIO_NUM_44,
        .pin_sw = GPIO_NUM_42,
        .target = ENCODER_TARGET_WHITE_R,
        .invert_direction = true,
    },
    
    {
        .name = "black_r",
        .pin_a = GPIO_NUM_52,
        .pin_b = GPIO_NUM_53,
        .pin_sw = GPIO_NUM_51,
        .target = ENCODER_TARGET_BLACK_R,
        .invert_direction = true,
    },


    // GPIO 34 is used for the B phase; verify this against your PCB revision.
    {
        .name = "black_b",
        .pin_a = GPIO_NUM_33,
        .pin_b = GPIO_NUM_34,
        .pin_sw = GPIO_NUM_54,
        .target = ENCODER_TARGET_BLACK_B,
        .invert_direction = true,
    },

    {
        .name = "white_g",
        .pin_a = GPIO_NUM_40,
        .pin_b = GPIO_NUM_41,
        .pin_sw = GPIO_NUM_39,
        .target = ENCODER_TARGET_WHITE_G,
        .invert_direction = true,
    },
    {
        .name = "black_level",
        .pin_a = GPIO_NUM_30,
        .pin_b = GPIO_NUM_31,
        .pin_sw = GPIO_NUM_32,
        .target = ENCODER_TARGET_BLACK_LEVEL,
        .invert_direction = false,
    },
    {
        .name = "black_g",
        .pin_a = GPIO_NUM_46,
        .pin_b = GPIO_NUM_47,
        .pin_sw = GPIO_NUM_45,
        .target = ENCODER_TARGET_BLACK_G,
        .invert_direction = true,
    },
    {
        .name = "white_b",
        .pin_a = GPIO_NUM_49,
        .pin_b = GPIO_NUM_50,
        .pin_sw = GPIO_NUM_48,
        .target = ENCODER_TARGET_WHITE_B,
        .invert_direction = true,
    },
};
