#include "camera_button.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "controls_internal.h"

#define TAG "CAMERA_BUTTON"

static camera_button_t *s_buttons_ref = NULL;
static uint8_t s_buttons_count = 0;

static void camera_button_isr(void *arg)
{
    uint32_t id = (uint32_t)arg;
    if (s_buttons_ref && id < s_buttons_count) {
        s_buttons_ref[id].edge_pending = true;
        controls_notify_from_isr();
    }
}

esp_err_t camera_buttons_init(i2c_master_bus_handle_t bus, camera_button_t *buttons, uint8_t count)
{
    (void)bus;

    s_buttons_ref = buttons;
    s_buttons_count = count;

    for (uint8_t i = 0; i < count; i++) {
        buttons[i].button_id = i;
        buttons[i].cfg = CAMERA_BUTTON_CONFIGS[i];
        buttons[i].edge_pending = true;
        buttons[i].last_level = 1;
        buttons[i].last_edge_tick_ms = 0;

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << buttons[i].cfg.input_gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE,
        };

        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "%s disabled: GPIO %d config failed (%s)",
                     buttons[i].cfg.name,
                     buttons[i].cfg.input_gpio,
                     esp_err_to_name(err));
            continue;
        }

        err = gpio_isr_handler_add(buttons[i].cfg.input_gpio, camera_button_isr, (void *)(uint32_t)i);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "%s disabled: GPIO %d ISR add failed (%s)",
                     buttons[i].cfg.name,
                     buttons[i].cfg.input_gpio,
                     esp_err_to_name(err));
            continue;
        }

        buttons[i].last_level = gpio_get_level(buttons[i].cfg.input_gpio);
        ESP_LOGI(TAG, "%s input ready on GPIO %d", buttons[i].cfg.name, buttons[i].cfg.input_gpio);
    }

    return ESP_OK;
}

void camera_buttons_process(camera_button_t *buttons, uint8_t count, void (*emit)(const control_event_t *ev))
{
    for (uint8_t i = 0; i < count; i++) {
        if (!buttons[i].edge_pending) {
            continue;
        }

        buttons[i].edge_pending = false;

        int level = gpio_get_level(buttons[i].cfg.input_gpio);
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        if (level != buttons[i].last_level && (now_ms - buttons[i].last_edge_tick_ms) > 20) {
            buttons[i].last_edge_tick_ms = now_ms;
            buttons[i].last_level = level;

            control_event_t ev = {
                .type = (level == 0) ? CONTROL_EVENT_CAMERA_BUTTON_PRESS : CONTROL_EVENT_CAMERA_BUTTON_RELEASE,
                .device_id = i,
                .control_id = 0,
                .value = 0,
            };
            emit(&ev);
        }
    }
}
