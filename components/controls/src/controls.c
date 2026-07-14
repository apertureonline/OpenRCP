#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "controls.h"
#include "controls_internal.h"
#include "controls_config.h"
#include "button_panel.h"
#include "camera_button.h"
#include "rotary_encoder.h"

#define TAG "CONTROLS"
#define CONTROL_QUEUE_LEN 64

static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t s_controls_task_handle = NULL;

static button_panel_t s_panels[BUTTON_PANEL_COUNT];
static camera_button_t s_camera_buttons[CAMERA_BUTTON_COUNT];
static rotary_encoder_t s_encoders[ROTARY_ENCODER_COUNT];

static i2c_master_bus_handle_t s_i2c_bus = NULL;

static void controls_i2c_sweep(i2c_master_bus_handle_t bus)
{
    char found[128] = {0};
    size_t used = 0;
    bool found_any = false;
    bool button_panel_seen[BUTTON_PANEL_COUNT] = {0};

    ESP_LOGI(TAG, "I2C sweep starting");

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t err = i2c_master_probe(bus, addr, 20);
        if (err == ESP_OK) {
            int written = snprintf(&found[used], sizeof(found) - used, "%s0x%02X", found_any ? ", " : "", addr);
            if (written > 0) {
                size_t available = sizeof(found) - used;
                used += ((size_t)written < available) ? (size_t)written : available - 1;
            }
            found_any = true;

            for (uint8_t i = 0; i < BUTTON_PANEL_COUNT; i++) {
                if (BUTTON_PANEL_CONFIGS[i].i2c_address == addr) {
                    button_panel_seen[i] = true;
                }
            }

        } else if (err != ESP_ERR_NOT_FOUND && err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "I2C probe 0x%02X returned %s", addr, esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG, "I2C sweep found: %s", found_any ? found : "none");

    for (uint8_t i = 0; i < BUTTON_PANEL_COUNT; i++) {
        if (!button_panel_seen[i]) {
            ESP_LOGW(TAG,
                     "Expected button panel %s was not seen at 0x%02X",
                     BUTTON_PANEL_CONFIGS[i].name,
                     BUTTON_PANEL_CONFIGS[i].i2c_address);
        }
    }

}

void controls_set_task_handle(TaskHandle_t handle)
{
    s_controls_task_handle = handle;
}

void controls_notify_from_isr(void)
{
    if (s_controls_task_handle == NULL) {
        return;
    }

    BaseType_t hp_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_controls_task_handle, &hp_task_woken);
    if (hp_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void emit_event(const control_event_t *ev)
{
    if (s_event_queue) {
        (void)xQueueSend(s_event_queue, ev, 0);
    }
}

esp_err_t controls_init(i2c_master_bus_handle_t shared_bus)
{
    if (shared_bus == NULL) {
        ESP_LOGE(TAG, "controls_init got NULL shared_bus");
        return ESP_ERR_INVALID_ARG;
    }

    s_event_queue = xQueueCreate(CONTROL_QUEUE_LEN, sizeof(control_event_t));
    if (!s_event_queue) {
        return ESP_FAIL;
    }

    s_i2c_bus = shared_bus;

    ESP_LOGI(TAG, "Using shared I2C bus handle for controls");
    controls_i2c_sweep(s_i2c_bus);

    esp_err_t err = button_panels_init(s_i2c_bus, s_panels, BUTTON_PANEL_COUNT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Button panel init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = rotary_encoders_init(s_encoders, ROTARY_ENCODER_COUNT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Rotary encoder init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = camera_buttons_init(s_i2c_bus, s_camera_buttons, CAMERA_BUTTON_COUNT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera button init failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

bool controls_get_event(control_event_t *event, uint32_t timeout_ms)
{
    if (!s_event_queue || !event) {
        return false;
    }

    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(s_event_queue, event, ticks) == pdTRUE;
}

void controls_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        button_panels_process(s_panels, BUTTON_PANEL_COUNT, emit_event);
        rotary_encoders_process(s_encoders, ROTARY_ENCODER_COUNT, emit_event);
        camera_buttons_process(s_camera_buttons, CAMERA_BUTTON_COUNT, emit_event);
    }
}
