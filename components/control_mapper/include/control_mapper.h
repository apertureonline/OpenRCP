#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "controls.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACTION_NONE = 0,

    // Camera select
    ACTION_SELECT_CAMERA_1,
    ACTION_SELECT_CAMERA_2,
    ACTION_SELECT_CAMERA_3,
    ACTION_SELECT_CAMERA_4,
    ACTION_SELECT_CAMERA_5,
    ACTION_SELECT_CAMERA_6,
    ACTION_SELECT_CAMERA_7,
    ACTION_SELECT_CAMERA_8,

    // Pages
    ACTION_PAGE_PREV,
    ACTION_PAGE_NEXT,

    // Iris
    ACTION_IRIS_UP,
    ACTION_IRIS_DOWN,

    // Black level
    ACTION_BLACK_LEVEL_UP,
    ACTION_BLACK_LEVEL_DOWN,
    ACTION_BLACK_LEVEL_RESET,

    // Black RGB
    ACTION_BLACK_R_UP,
    ACTION_BLACK_R_DOWN,
    ACTION_BLACK_R_RESET,

    ACTION_BLACK_G_UP,
    ACTION_BLACK_G_DOWN,
    ACTION_BLACK_G_RESET,

    ACTION_BLACK_B_UP,
    ACTION_BLACK_B_DOWN,
    ACTION_BLACK_B_RESET,

    // White RGB
    ACTION_WHITE_R_UP,
    ACTION_WHITE_R_DOWN,
    ACTION_WHITE_R_RESET,

    ACTION_WHITE_G_UP,
    ACTION_WHITE_G_DOWN,
    ACTION_WHITE_G_RESET,

    ACTION_WHITE_B_UP,
    ACTION_WHITE_B_DOWN,
    ACTION_WHITE_B_RESET,

    // Top / bottom button strip functions for now
    ACTION_GAIN_UP,
    ACTION_GAIN_DOWN,
    ACTION_GAIN_RESET,
    ACTION_WB_UP,
    ACTION_WB_DOWN,
    ACTION_WB_RESET,
    ACTION_TINT_UP,
    ACTION_TINT_DOWN,
    ACTION_TINT_RESET,

    ACTION_SHADING_RESET_ALL,

    ACTION_CONTRAST_UP,
    ACTION_CONTRAST_DOWN,
    ACTION_HUE_UP,
    ACTION_HUE_DOWN,
    ACTION_COLOR_CORRECTION_RESET,
} openrcp_action_t;

bool control_mapper_map_event(const control_event_t *ev, openrcp_action_t *action);

#ifdef __cplusplus
}
#endif
