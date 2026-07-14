#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rotary_encoder.h"
#include "controls_internal.h"

#define R_START       0x0
#define R_CW_FINAL    0x1
#define R_CW_BEGIN    0x2
#define R_CW_NEXT     0x3
#define R_CCW_BEGIN   0x4
#define R_CCW_FINAL   0x5
#define R_CCW_NEXT    0x6

#define DIR_NONE      0x00
#define DIR_CW        0x10
#define DIR_CCW       0x20

static const uint8_t ttable[7][4] = {
    {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
    {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
    {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
    {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
    {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
    {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
    {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};

static rotary_encoder_t *s_encoders_ref = NULL;
static uint8_t s_encoders_count = 0;

static void encoder_isr(void *arg)
{
    uint32_t id = (uint32_t)arg;
    if (s_encoders_ref && id < s_encoders_count) {
        s_encoders_ref[id].edge_pending = true;
        controls_notify_from_isr();
    }
}

static inline uint8_t read_ab_state(const rotary_encoder_t *enc)
{
    uint8_t a = gpio_get_level(enc->cfg.pin_a) ? 1 : 0;
    uint8_t b = gpio_get_level(enc->cfg.pin_b) ? 1 : 0;
    return (uint8_t)(a | (b << 1));
}

static inline uint8_t rotary_process(rotary_encoder_t *enc, uint8_t pinstate)
{
    enc->state = ttable[enc->state & 0x0F][pinstate];
    return enc->state & 0x30;
}

esp_err_t rotary_encoders_init(rotary_encoder_t *encoders, uint8_t count)
{
    s_encoders_ref = encoders;
    s_encoders_count = count;

    for (uint8_t i = 0; i < count; i++) {
        encoders[i].encoder_id = i;
        encoders[i].cfg = ROTARY_ENCODER_CONFIGS[i];
        encoders[i].edge_pending = true;
        encoders[i].state = 0;
        encoders[i].last_sw = 1;
        encoders[i].last_sw_tick_ms = 0;

        gpio_config_t io_conf = {
            .pin_bit_mask =
                (1ULL << encoders[i].cfg.pin_a) |
                (1ULL << encoders[i].cfg.pin_b) |
                (1ULL << encoders[i].cfg.pin_sw),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        ESP_ERROR_CHECK(gpio_isr_handler_add(encoders[i].cfg.pin_a, encoder_isr, (void *)(uint32_t)i));
        ESP_ERROR_CHECK(gpio_isr_handler_add(encoders[i].cfg.pin_b, encoder_isr, (void *)(uint32_t)i));
        ESP_ERROR_CHECK(gpio_isr_handler_add(encoders[i].cfg.pin_sw, encoder_isr, (void *)(uint32_t)i));

        encoders[i].last_sw = gpio_get_level(encoders[i].cfg.pin_sw);
    }

    return ESP_OK;
}

void rotary_encoders_process(rotary_encoder_t *encoders, uint8_t count, void (*emit)(const control_event_t *ev))
{
    for (uint8_t i = 0; i < count; i++) {
        if (!encoders[i].edge_pending) {
            continue;
        }

        encoders[i].edge_pending = false;

        uint8_t result = rotary_process(&encoders[i], read_ab_state(&encoders[i]));

        if (result == DIR_CW || result == DIR_CCW) {
            control_event_t ev = {
                .type = (result == DIR_CW) ? CONTROL_EVENT_ENCODER_CW : CONTROL_EVENT_ENCODER_CCW,
                .device_id = i,
                .control_id = 0,
                .value = (result == DIR_CW) ? 1 : -1,
            };
            emit(&ev);
        }

        int sw = gpio_get_level(encoders[i].cfg.pin_sw);
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        if (sw != encoders[i].last_sw && (now_ms - encoders[i].last_sw_tick_ms) > 20) {
            encoders[i].last_sw_tick_ms = now_ms;
            encoders[i].last_sw = sw;

            control_event_t ev = {
                .type = (sw == 0) ? CONTROL_EVENT_ENCODER_PRESS : CONTROL_EVENT_ENCODER_RELEASE,
                .device_id = i,
                .control_id = 0,
                .value = 0,
            };
            emit(&ev);
        }
    }
}