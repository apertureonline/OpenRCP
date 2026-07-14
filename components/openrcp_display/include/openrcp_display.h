#pragma once

#include "openrcp_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void openrcp_display_init(void);
void openrcp_display_render(const openrcp_model_t *model);

#ifdef __cplusplus
}
#endif