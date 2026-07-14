#include "control_mapper.h"
#include "controls_config.h"

static const openrcp_action_t PANEL_BUTTON_ACTIONS[BUTTON_PANEL_COUNT][BUTTONS_PER_PANEL] = {
    // panel_right control_id: 0, 1, 2, 3, 4, 5
    {
        ACTION_GAIN_DOWN,
        ACTION_GAIN_UP,
        ACTION_WB_DOWN,
        ACTION_WB_UP,
        ACTION_TINT_DOWN,
        ACTION_TINT_UP,
    },
    // panel_left control_id: 0, 1, 2, 3, 4, 5 (currently unassigned)
    {
        ACTION_NONE,
        ACTION_NONE,
        ACTION_NONE,
        ACTION_NONE,
        ACTION_NONE,
        ACTION_NONE,
    },
};

static bool map_panel_press(uint8_t device_id, uint8_t control_id, openrcp_action_t *action)
{
    if (!action || device_id >= BUTTON_PANEL_COUNT || control_id >= BUTTONS_PER_PANEL) {
        return false;
    }

    *action = PANEL_BUTTON_ACTIONS[device_id][control_id];
    return *action != ACTION_NONE;
}

static bool map_encoder_turn(uint8_t device_id, int32_t value, openrcp_action_t *action)
{
    if (!action || device_id >= ROTARY_ENCODER_COUNT) {
        return false;
    }

    bool up = (value > 0);
    const rotary_encoder_config_t *cfg = &ROTARY_ENCODER_CONFIGS[device_id];
    if (cfg->invert_direction) {
        up = !up;
    }

    switch (cfg->target) {
        case ENCODER_TARGET_IRIS:
            *action = up ? ACTION_IRIS_UP : ACTION_IRIS_DOWN;
            return true;
        case ENCODER_TARGET_BLACK_LEVEL:
            *action = up ? ACTION_BLACK_LEVEL_UP : ACTION_BLACK_LEVEL_DOWN;
            return true;
        case ENCODER_TARGET_BLACK_R:
            *action = up ? ACTION_BLACK_R_UP : ACTION_BLACK_R_DOWN;
            return true;
        case ENCODER_TARGET_BLACK_G:
            *action = up ? ACTION_BLACK_G_UP : ACTION_BLACK_G_DOWN;
            return true;
        case ENCODER_TARGET_BLACK_B:
            *action = up ? ACTION_BLACK_B_UP : ACTION_BLACK_B_DOWN;
            return true;
        case ENCODER_TARGET_WHITE_R:
            *action = up ? ACTION_WHITE_R_UP : ACTION_WHITE_R_DOWN;
            return true;
        case ENCODER_TARGET_WHITE_G:
            *action = up ? ACTION_WHITE_G_UP : ACTION_WHITE_G_DOWN;
            return true;
        case ENCODER_TARGET_WHITE_B:
            *action = up ? ACTION_WHITE_B_UP : ACTION_WHITE_B_DOWN;
            return true;
        case ENCODER_TARGET_NONE:
        default: return false;
    }
}

static bool map_encoder_press(uint8_t device_id, openrcp_action_t *action)
{
    if (!action || device_id >= ROTARY_ENCODER_COUNT) {
        return false;
    }

    /* Encoder push shortcuts: white row increases, black row decreases. */
    switch (ROTARY_ENCODER_CONFIGS[device_id].target) {
        case ENCODER_TARGET_WHITE_R: *action = ACTION_TINT_UP; return true;
        case ENCODER_TARGET_BLACK_R: *action = ACTION_TINT_DOWN; return true;
        case ENCODER_TARGET_WHITE_G: *action = ACTION_WB_UP; return true;
        case ENCODER_TARGET_BLACK_G: *action = ACTION_WB_DOWN; return true;
        case ENCODER_TARGET_WHITE_B: *action = ACTION_GAIN_UP; return true;
        case ENCODER_TARGET_BLACK_B: *action = ACTION_GAIN_DOWN; return true;
        case ENCODER_TARGET_BLACK_LEVEL:
        case ENCODER_TARGET_IRIS:
        case ENCODER_TARGET_NONE:
        default: return false;
    }
}

static bool map_camera_button_press(uint8_t device_id, openrcp_action_t *action)
{
    if (!action) {
        return false;
    }

    switch (device_id) {
        case 0: *action = ACTION_SELECT_CAMERA_1; return true;
        case 1: *action = ACTION_SELECT_CAMERA_2; return true;
        case 2: *action = ACTION_SELECT_CAMERA_3; return true;
        case 3: *action = ACTION_SELECT_CAMERA_4; return true;
        case 4: *action = ACTION_SELECT_CAMERA_5; return true;
        case 5: *action = ACTION_SELECT_CAMERA_6; return true;
        case 6: *action = ACTION_SELECT_CAMERA_7; return true;
        case 7: *action = ACTION_SELECT_CAMERA_8; return true;
        default: return false;
    }
}

bool control_mapper_map_event(const control_event_t *ev, openrcp_action_t *action)
{
    if (!ev || !action) {
        return false;
    }

    *action = ACTION_NONE;

    switch (ev->type) {
        case CONTROL_EVENT_PANEL_BUTTON_PRESS:
            return map_panel_press(ev->device_id, ev->control_id, action);

        case CONTROL_EVENT_ENCODER_CW:
        case CONTROL_EVENT_ENCODER_CCW:
            return map_encoder_turn(ev->device_id, ev->value, action);

        case CONTROL_EVENT_ENCODER_PRESS:
            return map_encoder_press(ev->device_id, action);

        case CONTROL_EVENT_CAMERA_BUTTON_PRESS:
            return map_camera_button_press(ev->device_id, action);

        default:
            return false;
    }
}
