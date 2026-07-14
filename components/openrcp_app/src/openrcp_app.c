#include "openrcp_app.h"

static uint8_t action_to_camera_index(openrcp_action_t action)
{
    switch (action) {
        case ACTION_SELECT_CAMERA_1: return 0;
        case ACTION_SELECT_CAMERA_2: return 1;
        case ACTION_SELECT_CAMERA_3: return 2;
        case ACTION_SELECT_CAMERA_4: return 3;
        case ACTION_SELECT_CAMERA_5: return 4;
        case ACTION_SELECT_CAMERA_6: return 5;
        case ACTION_SELECT_CAMERA_7: return 6;
        case ACTION_SELECT_CAMERA_8: return 7;
        default: return 0xFF;
    }
}

static void mark_shading_changed(openrcp_camera_state_t *cam)
{
    openrcp_model_mark_display_dirty(cam);
    openrcp_model_mark_camera_tx_dirty(cam);
}

static void reset_selected_shading(openrcp_camera_state_t *cam)
{
    cam->gain_db = 0;
    cam->white_balance_k = 5600;
    cam->tint = 0;

    cam->black_level = 0;
    cam->black_r = 0;
    cam->black_g = 0;
    cam->black_b = 0;

    cam->white_r = 0;
    cam->white_g = 0;
    cam->white_b = 0;

    cam->gamma_r = 0;
    cam->gamma_g = 0;
    cam->gamma_b = 0;
    cam->gamma_luma = 0;

    cam->offset_r = 0;
    cam->offset_g = 0;
    cam->offset_b = 0;
    cam->offset_luma = 0;

    cam->contrast_pivot = 50;
    cam->contrast_adjust = 100;
    cam->luma_mix = 100;
    cam->hue = 0;
    cam->saturation = 100;

    mark_shading_changed(cam);
}

static void reset_color_correction(openrcp_camera_state_t *cam)
{
    cam->black_level = 0;
    cam->black_r = 0;
    cam->black_g = 0;
    cam->black_b = 0;

    cam->white_r = 0;
    cam->white_g = 0;
    cam->white_b = 0;

    cam->gamma_r = 0;
    cam->gamma_g = 0;
    cam->gamma_b = 0;
    cam->gamma_luma = 0;

    cam->offset_r = 0;
    cam->offset_g = 0;
    cam->offset_b = 0;
    cam->offset_luma = 0;

    cam->contrast_pivot = 50;
    cam->contrast_adjust = 100;
    cam->luma_mix = 100;
    cam->hue = 0;
    cam->saturation = 100;

    mark_shading_changed(cam);
}

void openrcp_app_init(openrcp_app_t *app)
{
    if (!app) {
        return;
    }

    openrcp_model_init(&app->model);
}

bool openrcp_app_apply_action(openrcp_app_t *app, openrcp_action_t action)
{
    if (!app) {
        return false;
    }

    openrcp_camera_state_t *cam = openrcp_model_get_selected(&app->model);
    if (!cam) {
        return false;
    }

    uint8_t cam_idx = action_to_camera_index(action);
    if (cam_idx < OPENRCP_CAMERA_COUNT) {
        openrcp_model_select_camera(&app->model, cam_idx);
        return true;
    }

    switch (action) {
        case ACTION_PAGE_PREV:
            if (app->model.selected_page == 0) {
                openrcp_model_select_page(&app->model, OPENRCP_PAGE_COUNT - 1);
            } else {
                openrcp_model_select_page(&app->model, app->model.selected_page - 1);
            }
            return true;

        case ACTION_PAGE_NEXT:
            openrcp_model_select_page(
                &app->model,
                (openrcp_page_t)((app->model.selected_page + 1) % OPENRCP_PAGE_COUNT)
            );
            return true;

        case ACTION_GAIN_UP:
            cam->gain_db += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_GAIN_DOWN:
            cam->gain_db -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_GAIN_RESET:
            cam->gain_db = 0;
            mark_shading_changed(cam);
            return true;

        case ACTION_WB_UP:
            cam->white_balance_k += 50;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WB_DOWN:
            cam->white_balance_k -= 50;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WB_RESET:
            cam->white_balance_k = 5600;
            mark_shading_changed(cam);
            return true;

        case ACTION_TINT_UP:
            cam->tint += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_TINT_DOWN:
            cam->tint -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_TINT_RESET:
            cam->tint = 0;
            mark_shading_changed(cam);
            return true;

        case ACTION_SHADING_RESET_ALL:
            reset_selected_shading(cam);
            return true;

        case ACTION_CONTRAST_UP:
            cam->contrast_adjust += 1;
            mark_shading_changed(cam);
            return true;

        case ACTION_CONTRAST_DOWN:
            cam->contrast_adjust -= 1;
            if (cam->contrast_adjust < 0) {
                cam->contrast_adjust = 0;
            }
            mark_shading_changed(cam);
            return true;

        case ACTION_HUE_UP:
            cam->hue += 1;
            mark_shading_changed(cam);
            return true;

        case ACTION_HUE_DOWN:
            cam->hue -= 1;
            mark_shading_changed(cam);
            return true;

        case ACTION_COLOR_CORRECTION_RESET:
            reset_color_correction(cam);
            return true;

        case ACTION_BLACK_LEVEL_UP:
            cam->black_level += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_LEVEL_DOWN:
            cam->black_level -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_LEVEL_RESET:
            cam->black_level = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_R_UP:
            cam->black_r += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_R_DOWN:
            cam->black_r -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_R_RESET:
            cam->black_r = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_G_UP:
            cam->black_g += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_G_DOWN:
            cam->black_g -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_G_RESET:
            cam->black_g = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_B_UP:
            cam->black_b += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_B_DOWN:
            cam->black_b -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_BLACK_B_RESET:
            cam->black_b = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_R_UP:
            cam->white_r += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_R_DOWN:
            cam->white_r -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_R_RESET:
            cam->white_r = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_G_UP:
            cam->white_g += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_G_DOWN:
            cam->white_g -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_G_RESET:
            cam->white_g = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_B_UP:
            cam->white_b += 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_B_DOWN:
            cam->white_b -= 1;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_WHITE_B_RESET:
            cam->white_b = 0;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            return true;

        case ACTION_IRIS_UP:
            cam->iris_fader_raw += 20;
            if (cam->iris_fader_raw > 2528) {
                cam->iris_fader_raw = 2528;
            }
            cam->iris_normalized = (float)cam->iris_fader_raw / 2528.0f;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            openrcp_model_mark_motor_dirty(cam);
            return true;

        case ACTION_IRIS_DOWN:
            cam->iris_fader_raw -= 20;
            if (cam->iris_fader_raw < 0) {
                cam->iris_fader_raw = 0;
            }
            cam->iris_normalized = (float)cam->iris_fader_raw / 2528.0f;
            openrcp_model_mark_display_dirty(cam);
            openrcp_model_mark_camera_tx_dirty(cam);
            openrcp_model_mark_motor_dirty(cam);
            return true;

        case ACTION_NONE:
        default:
            return false;
    }
}

const openrcp_model_t *openrcp_app_get_model_const(const openrcp_app_t *app)
{
    if (!app) {
        return NULL;
    }
    return &app->model;
}

openrcp_model_t *openrcp_app_get_model(openrcp_app_t *app)
{
    if (!app) {
        return NULL;
    }
    return &app->model;
}

int openrcp_app_get_selected_camera(const openrcp_app_t *app)
{
    if (!app) {
        return 0;
    }
    return app->model.selected_camera;
}

int openrcp_app_get_selected_camera_iris_fader_raw(const openrcp_app_t *app)
{
    if (!app) {
        return 0;
    }

    const openrcp_camera_state_t *cam = openrcp_model_get_selected_const(&app->model);
    if (!cam) {
        return 0;
    }

    return cam->iris_fader_raw;
}

void openrcp_app_set_selected_camera_iris_fader_raw(openrcp_app_t *app, int raw)
{
    if (!app) {
        return;
    }

    openrcp_camera_state_t *cam = openrcp_model_get_selected(&app->model);
    if (!cam) {
        return;
    }

    if (raw < 0) {
        raw = 0;
    }
    if (raw > 2528) {
        raw = 2528;
    }

    cam->iris_fader_raw = raw;
    cam->iris_normalized = (float)raw / 2528.0f;

    openrcp_model_mark_display_dirty(cam);
    openrcp_model_mark_camera_tx_dirty(cam);
}

void openrcp_app_reset_cameras_to_defaults(openrcp_app_t *app, int camera_count)
{
    if (!app) {
        return;
    }

    if (camera_count < 0) {
        camera_count = 0;
    }
    if (camera_count > OPENRCP_CAMERA_COUNT) {
        camera_count = OPENRCP_CAMERA_COUNT;
    }

    for (int i = 0; i < camera_count; i++) {
        openrcp_model_reset_camera_to_defaults(&app->model, (uint8_t)i);
    }
}

void openrcp_app_set_camera_tally_state(openrcp_app_t *app, int camera_index, openrcp_tally_state_t tally_state)
{
    if (!app) {
        return;
    }
    if (camera_index < 0 || camera_index >= OPENRCP_CAMERA_COUNT) {
        return;
    }

    openrcp_camera_state_t *cam = openrcp_model_get_camera(&app->model, (uint8_t)camera_index);
    if (!cam) {
        return;
    }

    if (cam->tally_state != tally_state) {
        cam->tally_state = tally_state;
        openrcp_model_mark_display_dirty(cam);
    }
}
