#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "button_panel.h"
#include "controls_internal.h"

#define TAG "BUTTON_PANEL"

static button_panel_t *s_panels_ref = NULL;
static uint8_t s_panels_count = 0;

static void panel_isr(void *arg)
{
    uint32_t panel_id = (uint32_t)arg;
    if (s_panels_ref && panel_id < s_panels_count) {
        s_panels_ref[panel_id].int_pending = true;
        controls_notify_from_isr();
    }
}

static esp_err_t pcf_write_all_high(i2c_master_dev_handle_t dev)
{
    uint8_t value = 0xFF;
    return i2c_master_transmit(dev, &value, 1, -1);
}

static esp_err_t pcf_read(i2c_master_dev_handle_t dev, uint8_t *value)
{
    return i2c_master_receive(dev, value, 1, -1);
}

esp_err_t button_panels_init(i2c_master_bus_handle_t bus, button_panel_t *panels, uint8_t count)
{
    s_panels_ref = panels;
    s_panels_count = count;

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
        return err;
    }

    for (uint8_t i = 0; i < count; i++) {
        panels[i].panel_id = i;
        panels[i].cfg = BUTTON_PANEL_CONFIGS[i];
        panels[i].dev_handle = NULL;
        panels[i].active = false;
        panels[i].int_pending = true;
        panels[i].last_raw = 0xFF;

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = panels[i].cfg.i2c_address,
            .scl_speed_hz = 100000,
        };

        err = i2c_master_bus_add_device(bus, &dev_cfg, &panels[i].dev_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "%s at 0x%02X disabled: add device failed (%s)",
                     panels[i].cfg.name,
                     panels[i].cfg.i2c_address,
                     esp_err_to_name(err));
            continue;
        }

        err = pcf_write_all_high(panels[i].dev_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "%s at 0x%02X disabled: initial PCF write failed (%s)",
                     panels[i].cfg.name,
                     panels[i].cfg.i2c_address,
                     esp_err_to_name(err));
            (void)i2c_master_bus_rm_device(panels[i].dev_handle);
            panels[i].dev_handle = NULL;
            continue;
        }

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << panels[i].cfg.int_gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "%s at 0x%02X disabled: interrupt GPIO config failed (%s)",
                     panels[i].cfg.name,
                     panels[i].cfg.i2c_address,
                     esp_err_to_name(err));
            (void)i2c_master_bus_rm_device(panels[i].dev_handle);
            panels[i].dev_handle = NULL;
            continue;
        }

        err = gpio_isr_handler_add(panels[i].cfg.int_gpio, panel_isr, (void *)(uint32_t)i);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "%s at 0x%02X disabled: interrupt handler add failed (%s)",
                     panels[i].cfg.name,
                     panels[i].cfg.i2c_address,
                     esp_err_to_name(err));
            (void)i2c_master_bus_rm_device(panels[i].dev_handle);
            panels[i].dev_handle = NULL;
            continue;
        }

        panels[i].active = true;
        ESP_LOGI(TAG,
                 "%s ready at 0x%02X, INT GPIO %d",
                 panels[i].cfg.name,
                 panels[i].cfg.i2c_address,
                 panels[i].cfg.int_gpio);
    }

    return ESP_OK;
}

void button_panels_process(button_panel_t *panels, uint8_t count, void (*emit)(const control_event_t *ev))
{
    for (uint8_t i = 0; i < count; i++) {
        if (!panels[i].active || panels[i].dev_handle == NULL) {
            continue;
        }

        if (!panels[i].int_pending) {
            continue;
        }

        panels[i].int_pending = false;

        uint8_t raw = 0xFF;
        if (pcf_read(panels[i].dev_handle, &raw) != ESP_OK) {
            continue;
        }

        uint8_t old_raw = panels[i].last_raw;
        panels[i].last_raw = raw;

        for (uint8_t btn = 0; btn < BUTTONS_PER_PANEL; btn++) {
            uint8_t bit = panels[i].cfg.button_bits[btn];
            bool old_pressed = ((old_raw & (1 << bit)) == 0);
            bool new_pressed = ((raw & (1 << bit)) == 0);

            if (old_pressed != new_pressed) {
                control_event_t ev = {
                    .type = new_pressed ? CONTROL_EVENT_PANEL_BUTTON_PRESS : CONTROL_EVENT_PANEL_BUTTON_RELEASE,
                    .device_id = i,
                    .control_id = btn,
                    .value = 0,
                };
                emit(&ev);
            }
        }
    }
}
