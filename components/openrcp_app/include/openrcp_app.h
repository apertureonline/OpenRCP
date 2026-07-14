#pragma once

#include <stdbool.h>

#include "openrcp_model.h"
#include "control_mapper.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    openrcp_model_t model;
} openrcp_app_t;

void openrcp_app_init(openrcp_app_t *app);
bool openrcp_app_apply_action(openrcp_app_t *app, openrcp_action_t action);

const openrcp_model_t *openrcp_app_get_model_const(const openrcp_app_t *app);
openrcp_model_t *openrcp_app_get_model(openrcp_app_t *app);

int openrcp_app_get_selected_camera(const openrcp_app_t *app);
int openrcp_app_get_selected_camera_iris_fader_raw(const openrcp_app_t *app);
void openrcp_app_set_selected_camera_iris_fader_raw(openrcp_app_t *app, int raw);

void openrcp_app_reset_cameras_to_defaults(openrcp_app_t *app, int camera_count);
void openrcp_app_set_camera_tally_state(openrcp_app_t *app, int camera_index, openrcp_tally_state_t tally_state);

#ifdef __cplusplus
}
#endif
