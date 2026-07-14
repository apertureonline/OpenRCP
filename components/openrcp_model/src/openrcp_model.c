#include "openrcp_model.h"
#include <string.h>

static void init_camera_defaults(openrcp_camera_state_t *cam)
{
    memset(cam, 0, sizeof(*cam));

    cam->selected = false;

    cam->iris_normalized = 0.5f;
    cam->iris_fader_raw = 2048;

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

    cam->shutter_speed = 50;
    cam->sharpness = 0;

    cam->tally_state = OPENRCP_TALLY_NONE;

    cam->dirty_display = true;
    cam->dirty_camera_tx = true;
    cam->dirty_motor = true;
}

void openrcp_model_init(openrcp_model_t *model)
{
    if (!model) {
        return;
    }

    memset(model, 0, sizeof(*model));

    for (uint8_t i = 0; i < OPENRCP_CAMERA_COUNT; i++) {
        init_camera_defaults(&model->cameras[i]);
    }

    model->selected_camera = 0;
    model->selected_page = OPENRCP_PAGE_MAIN;
    model->cameras[0].selected = true;
}

void openrcp_model_reset_camera_to_defaults(openrcp_model_t *model, uint8_t camera_index)
{
    if (!model || camera_index >= OPENRCP_CAMERA_COUNT) {
        return;
    }

    bool selected = (model->selected_camera == camera_index);
    openrcp_tally_state_t tally_state = model->cameras[camera_index].tally_state;
    init_camera_defaults(&model->cameras[camera_index]);
    model->cameras[camera_index].selected = selected;
    model->cameras[camera_index].tally_state = tally_state;
}

void openrcp_model_select_camera(openrcp_model_t *model, uint8_t camera_index)
{
    if (!model || camera_index >= OPENRCP_CAMERA_COUNT) {
        return;
    }

    model->cameras[model->selected_camera].selected = false;
    model->selected_camera = camera_index;
    model->cameras[camera_index].selected = true;

    model->cameras[camera_index].dirty_display = true;
    model->cameras[camera_index].dirty_camera_tx = true;
    model->cameras[camera_index].dirty_motor = true;
}

void openrcp_model_select_page(openrcp_model_t *model, openrcp_page_t page)
{
    if (!model || page >= OPENRCP_PAGE_COUNT) {
        return;
    }

    model->selected_page = page;

    openrcp_camera_state_t *cam = openrcp_model_get_selected(model);
    if (cam) {
        cam->dirty_display = true;
    }
}

const char *openrcp_model_page_name(openrcp_page_t page)
{
    switch (page) {
        case OPENRCP_PAGE_MAIN: return "MAIN";
        case OPENRCP_PAGE_COLOR_LIFT: return "LIFT";
        case OPENRCP_PAGE_COLOR_GAIN: return "GAIN";
        case OPENRCP_PAGE_GAMMA: return "GAMMA";
        case OPENRCP_PAGE_OFFSET: return "OFFSET";
        case OPENRCP_PAGE_CONTRAST: return "CONTRAST";
        case OPENRCP_PAGE_COLOR: return "COLOR";
        case OPENRCP_PAGE_SHUTTER: return "SHUTTER";
        case OPENRCP_PAGE_COUNT:
        default: return "PAGE";
    }
}

openrcp_camera_state_t *openrcp_model_get_selected(openrcp_model_t *model)
{
    if (!model) {
        return NULL;
    }

    return &model->cameras[model->selected_camera];
}

const openrcp_camera_state_t *openrcp_model_get_selected_const(const openrcp_model_t *model)
{
    if (!model) {
        return NULL;
    }

    return &model->cameras[model->selected_camera];
}

openrcp_camera_state_t *openrcp_model_get_camera(openrcp_model_t *model, uint8_t camera_index)
{
    if (!model || camera_index >= OPENRCP_CAMERA_COUNT) {
        return NULL;
    }

    return &model->cameras[camera_index];
}

void openrcp_model_mark_all_clean(openrcp_camera_state_t *cam)
{
    if (!cam) {
        return;
    }

    cam->dirty_display = false;
    cam->dirty_camera_tx = false;
    cam->dirty_motor = false;
}

void openrcp_model_mark_display_dirty(openrcp_camera_state_t *cam)
{
    if (cam) {
        cam->dirty_display = true;
    }
}

void openrcp_model_mark_camera_tx_dirty(openrcp_camera_state_t *cam)
{
    if (cam) {
        cam->dirty_camera_tx = true;
    }
}

void openrcp_model_mark_motor_dirty(openrcp_camera_state_t *cam)
{
    if (cam) {
        cam->dirty_motor = true;
    }
}
