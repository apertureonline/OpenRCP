#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENRCP_CAMERA_COUNT 8

typedef enum {
    OPENRCP_PAGE_MAIN = 0,
    OPENRCP_PAGE_COLOR_LIFT,
    OPENRCP_PAGE_COLOR_GAIN,
    OPENRCP_PAGE_GAMMA,
    OPENRCP_PAGE_OFFSET,
    OPENRCP_PAGE_CONTRAST,
    OPENRCP_PAGE_COLOR,
    OPENRCP_PAGE_SHUTTER,
    OPENRCP_PAGE_COUNT
} openrcp_page_t;

typedef enum {
    OPENRCP_TALLY_NONE = 0,
    OPENRCP_TALLY_PREVIEW,
    OPENRCP_TALLY_PROGRAM
} openrcp_tally_state_t;

typedef struct {
    bool selected;

    // Lens
    float iris_normalized;      // 0.0 .. 1.0
    int iris_fader_raw;         // motor target / adc-style raw value

    // Primary controls
    int gain_db;
    int white_balance_k;
    int tint;
    int black_level;

    // Black balance / lift-style controls
    int black_r;
    int black_g;
    int black_b;

    // White balance / gain-style controls
    int white_r;
    int white_g;
    int white_b;

    // Prepared page values for future soft-control pages
    int gamma_r;
    int gamma_g;
    int gamma_b;
    int gamma_luma;

    int offset_r;
    int offset_g;
    int offset_b;
    int offset_luma;

    int contrast_pivot;
    int contrast_adjust;
    int luma_mix;
    int hue;
    int saturation;

    int shutter_speed;
    int sharpness;

    // Tally
    openrcp_tally_state_t tally_state;

    // Dirty flags
    bool dirty_display;
    bool dirty_camera_tx;
    bool dirty_motor;
} openrcp_camera_state_t;

typedef struct {
    uint8_t selected_camera; // 0..7
    openrcp_page_t selected_page;
    bool shift_mode;
    openrcp_camera_state_t cameras[OPENRCP_CAMERA_COUNT];
} openrcp_model_t;

void openrcp_model_init(openrcp_model_t *model);
void openrcp_model_reset_camera_to_defaults(openrcp_model_t *model, uint8_t camera_index);
void openrcp_model_select_camera(openrcp_model_t *model, uint8_t camera_index);
void openrcp_model_select_page(openrcp_model_t *model, openrcp_page_t page);
const char *openrcp_model_page_name(openrcp_page_t page);
openrcp_camera_state_t *openrcp_model_get_selected(openrcp_model_t *model);
const openrcp_camera_state_t *openrcp_model_get_selected_const(const openrcp_model_t *model);
openrcp_camera_state_t *openrcp_model_get_camera(openrcp_model_t *model, uint8_t camera_index);

void openrcp_model_mark_all_clean(openrcp_camera_state_t *cam);
void openrcp_model_mark_display_dirty(openrcp_camera_state_t *cam);
void openrcp_model_mark_camera_tx_dirty(openrcp_camera_state_t *cam);
void openrcp_model_mark_motor_dirty(openrcp_camera_state_t *cam);

#ifdef __cplusplus
}
#endif
