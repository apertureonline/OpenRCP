#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int raw;
    bool changed_by_user;
    bool user_active;
} iris_fader_sample_t;

esp_err_t iris_fader_init(void);
esp_err_t iris_fader_move_to_raw(int target_raw);
esp_err_t iris_fader_move_to_raw_with_result(int target_raw, int *actual_raw);
iris_fader_sample_t iris_fader_poll_user(void);
int iris_fader_get_raw(void);

#ifdef __cplusplus
}
#endif
