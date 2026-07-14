#include "iris_fader.h"

#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "IRIS_FADER";

// ============================================================
// Pins
// ============================================================
#define PIN_PWM   GPIO_NUM_29
#define PIN_AIN2  GPIO_NUM_28
#define PIN_AIN1  GPIO_NUM_27
#define PIN_STBY  GPIO_NUM_26
#define PIN_POT   GPIO_NUM_23

// Set this to false if the fader moves away from the target after rewiring.
#define MOTOR_DIRECTION_INVERTED true

// ============================================================
// PWM
// ============================================================
#define PWM_FREQ_HZ      20000
#define PWM_MODE         LEDC_LOW_SPEED_MODE
#define PWM_TIMER        LEDC_TIMER_0
#define PWM_CHANNEL      LEDC_CHANNEL_0
#define PWM_RESOLUTION   LEDC_TIMER_8_BIT
#define PWM_MAX          255

// ============================================================
// Fader discovered range
// ============================================================
#define IRIS_RAW_MIN     0
#define IRIS_RAW_MAX     2528

// ============================================================
// Motion tuning
// ============================================================
#define MOVE_PWM_FAR        145
#define MOVE_PWM_MID        132
#define MOVE_PWM_NEAR       112
#define MOVE_PWM_FINE       104

#define MOVE_BURST_FAR_MS   24
#define MOVE_BURST_MID_MS   14
#define MOVE_BURST_NEAR_MS  5
#define MOVE_BURST_FINE_MS  3

#define POSITION_DEADBAND       35
#define VERIFY_DEADBAND         45
#define FINE_ZONE               120
#define NEAR_ZONE               260
#define MID_ZONE                800
#define MIN_POSITION_DELTA      1
#define MAX_MOVE_ITER           140
#define MOVE_TIMEOUT_MS         2500
#define STOP_SETTLE_MS          90
#define LITTLE_MOVE_RETRIES_MAX 3
#define UNSTICK_PWM_BOOST       26
#define UNSTICK_BURST_BOOST_MS  5

// ============================================================
// Live user movement smoothing / publish tuning
// ============================================================
#define FILTER_NUM_OLD          2
#define FILTER_NUM_NEW          1
#define FILTER_DEN              3

#define USER_PUBLISH_DEADZONE   6
#define USER_ACTIVE_THRESHOLD   8
#define USER_IDLE_MS            120
#define USER_IGNORE_AFTER_MOVE_MS 1000

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_unit_t s_adc_unit;
static adc_channel_t s_adc_channel;

static bool s_initialized = false;
static bool s_motor_busy = false;

static int s_filtered_raw = 0;
static int s_last_published_raw = 0;
static int s_last_active_raw = 0;
static TickType_t s_last_user_motion_tick = 0;
static TickType_t s_ignore_user_until_tick = 0;

// ============================================================
// Helpers
// ============================================================
static int abs_int(int x)
{
    return (x < 0) ? -x : x;
}

static int clamp_int(int x, int lo, int hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int median3(int a, int b, int c)
{
    if ((a >= b && a <= c) || (a >= c && a <= b)) return a;
    if ((b >= a && b <= c) || (b >= c && b <= a)) return b;
    return c;
}

static void motor_stop(void)
{
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);

    gpio_set_level(PIN_AIN1, 0);
    gpio_set_level(PIN_AIN2, 0);
}

static void motor_standby(bool enable)
{
    gpio_set_level(PIN_STBY, enable ? 1 : 0);
}

static void motor_drive(bool toward_high_side, int pwm)
{
    pwm = clamp_int(pwm, 0, PWM_MAX);

    if (MOTOR_DIRECTION_INVERTED) {
        toward_high_side = !toward_high_side;
    }

    if (toward_high_side) {
        gpio_set_level(PIN_AIN1, 1);
        gpio_set_level(PIN_AIN2, 0);
    } else {
        gpio_set_level(PIN_AIN1, 0);
        gpio_set_level(PIN_AIN2, 1);
    }

    ledc_set_duty(PWM_MODE, PWM_CHANNEL, pwm);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

static int read_pot_once(void)
{
    int raw = 0;
    esp_err_t err = adc_oneshot_read(s_adc_handle, s_adc_channel, &raw);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return 0;
    }
    return raw;
}

static int read_pot_control(void)
{
    int a = read_pot_once();
    int b = read_pot_once();
    int c = read_pot_once();
    return clamp_int(median3(a, b, c), IRIS_RAW_MIN, IRIS_RAW_MAX);
}

static int read_pot_avg(void)
{
    int a = read_pot_once();
    int b = read_pot_once();
    int c = read_pot_once();
    return clamp_int((a + b + c) / 3, IRIS_RAW_MIN, IRIS_RAW_MAX);
}

static int update_filtered_raw(int raw)
{
    raw = clamp_int(raw, IRIS_RAW_MIN, IRIS_RAW_MAX);

    if (s_filtered_raw == 0) {
        s_filtered_raw = raw;
    } else {
        s_filtered_raw =
            (s_filtered_raw * FILTER_NUM_OLD + raw * FILTER_NUM_NEW) / FILTER_DEN;
    }

    return s_filtered_raw;
}

static void sync_public_state_from_current_position(void)
{
    int raw = read_pot_avg();
    int filtered = update_filtered_raw(raw);
    s_last_published_raw = filtered;
    s_last_active_raw = filtered;
    s_last_user_motion_tick = xTaskGetTickCount();
}

static esp_err_t init_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_AIN1) | (1ULL << PIN_AIN2) | (1ULL << PIN_STBY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_set_level(PIN_AIN1, 0);
    gpio_set_level(PIN_AIN2, 0);
    gpio_set_level(PIN_STBY, 1);

    return ESP_OK;
}

static esp_err_t init_pwm(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num = PIN_PWM,
        .speed_mode = PWM_MODE,
        .channel = PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    return ESP_OK;
}

static esp_err_t init_adc(void)
{
    esp_err_t err = adc_oneshot_io_to_channel(PIN_POT, &s_adc_unit, &s_adc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_io_to_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_unit_init_cfg_t adc_unit_cfg = {
        .unit_id = s_adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, s_adc_channel, &chan_cfg));

    return ESP_OK;
}

static void choose_profile(int abs_error, int *pwm, int *burst_ms)
{
    if (abs_error > MID_ZONE) {
        *pwm = MOVE_PWM_FAR;
        *burst_ms = MOVE_BURST_FAR_MS;
    } else if (abs_error > NEAR_ZONE) {
        *pwm = MOVE_PWM_MID;
        *burst_ms = MOVE_BURST_MID_MS;
    } else if (abs_error > FINE_ZONE) {
        *pwm = MOVE_PWM_NEAR;
        *burst_ms = MOVE_BURST_NEAR_MS;
    } else {
        *pwm = MOVE_PWM_FINE;
        *burst_ms = MOVE_BURST_FINE_MS;
    }
}

// ============================================================
// Public API
// ============================================================
esp_err_t iris_fader_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(init_gpio());
    ESP_ERROR_CHECK(init_pwm());
    ESP_ERROR_CHECK(init_adc());

    motor_standby(true);
    motor_stop();

    vTaskDelay(pdMS_TO_TICKS(50));

    int raw = read_pot_avg();
    s_filtered_raw = raw;
    s_last_published_raw = raw;
    s_last_active_raw = raw;
    s_last_user_motion_tick = xTaskGetTickCount();
    s_ignore_user_until_tick = 0;
    s_motor_busy = false;
    s_initialized = true;

    ESP_LOGI(TAG, "Initialized. start_raw=%d", raw);
    return ESP_OK;
}

int iris_fader_get_raw(void)
{
    int raw = read_pot_avg();
    return update_filtered_raw(raw);
}

esp_err_t iris_fader_move_to_raw_with_result(int target_raw, int *actual_raw)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    target_raw = clamp_int(target_raw, IRIS_RAW_MIN, IRIS_RAW_MAX);

    ESP_LOGI(TAG, "Moving toward target: %d", target_raw);

    s_motor_busy = true;

    int little_move_retries = 0;
    TickType_t move_start_tick = xTaskGetTickCount();

    for (int i = 0; i < MAX_MOVE_ITER; i++) {
        if ((xTaskGetTickCount() - move_start_tick) >= pdMS_TO_TICKS(MOVE_TIMEOUT_MS)) {
            ESP_LOGW(TAG, "Move timed out after %d ms.", MOVE_TIMEOUT_MS);
            motor_stop();
            sync_public_state_from_current_position();
            if (actual_raw) {
                *actual_raw = s_last_published_raw;
            }
            s_motor_busy = false;
            s_ignore_user_until_tick = xTaskGetTickCount() + pdMS_TO_TICKS(USER_IGNORE_AFTER_MOVE_MS);
            return ESP_ERR_TIMEOUT;
        }

        int pos = read_pot_control();
        int error = target_raw - pos;
        int abs_error = abs_int(error);

        ESP_LOGI(TAG, "pos=%d error=%d", pos, error);

        if (abs_error <= POSITION_DEADBAND) {
            motor_stop();
            vTaskDelay(pdMS_TO_TICKS(STOP_SETTLE_MS));

            int verify_pos = read_pot_control();
            int verify_error = target_raw - verify_pos;
            int verify_abs_error = abs_int(verify_error);

            ESP_LOGI(TAG, "verify_pos=%d verify_error=%d", verify_pos, verify_error);

            if (verify_abs_error <= VERIFY_DEADBAND) {
                ESP_LOGI(TAG, "Target reached and verified.");
                sync_public_state_from_current_position();
                if (actual_raw) {
                    *actual_raw = s_last_published_raw;
                }
                s_motor_busy = false;
                s_ignore_user_until_tick = xTaskGetTickCount() + pdMS_TO_TICKS(USER_IGNORE_AFTER_MOVE_MS);
                return ESP_OK;
            }

            ESP_LOGI(TAG, "Target drifted after stop, correcting...");
        }

        bool toward_high_side = (error > 0);

        int pwm = 0;
        int burst_ms = 0;
        choose_profile(abs_error, &pwm, &burst_ms);
        if (little_move_retries > 0) {
            pwm = clamp_int(pwm + little_move_retries * 8, 0, PWM_MAX);
            burst_ms += little_move_retries;
        }

        int before = pos;

        motor_standby(true);
        ESP_LOGI(TAG,
                 "drive=%s pwm=%d burst_ms=%d%s",
                 toward_high_side ? "higher" : "lower",
                 pwm,
                 burst_ms,
                 MOTOR_DIRECTION_INVERTED ? " motor_inverted" : "");
        motor_drive(toward_high_side, pwm);
        vTaskDelay(pdMS_TO_TICKS(burst_ms));
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(40));

        int after = read_pot_control();
        int moved = abs_int(after - before);

        if (moved < MIN_POSITION_DELTA) {
            little_move_retries++;

            ESP_LOGW(TAG,
                     "Little movement detected (%d/%d), error=%d",
                     little_move_retries,
                     LITTLE_MOVE_RETRIES_MAX,
                     target_raw - after);

            if (little_move_retries >= LITTLE_MOVE_RETRIES_MAX) {
                int remaining_error = target_raw - after;
                int remaining_abs_error = abs_int(remaining_error);
                int unstick_pwm = clamp_int(pwm + UNSTICK_PWM_BOOST, 0, PWM_MAX);
                int unstick_burst = burst_ms + UNSTICK_BURST_BOOST_MS;

                if (remaining_abs_error <= VERIFY_DEADBAND) {
                    ESP_LOGI(TAG, "Close enough after little movement, verifying...");
                    little_move_retries = 0;
                    continue;
                }

                ESP_LOGW(TAG,
                         "Little movement persisted, applying unstick pulse pwm=%d burst_ms=%d error=%d",
                         unstick_pwm,
                         unstick_burst,
                         remaining_error);

                motor_standby(true);
                motor_drive(toward_high_side, unstick_pwm);
                vTaskDelay(pdMS_TO_TICKS(unstick_burst));
                motor_stop();
                vTaskDelay(pdMS_TO_TICKS(50));

                little_move_retries = 0;
                continue;
            }

            int boost_pwm = clamp_int(pwm + 20, 0, PWM_MAX);
            int boost_burst = burst_ms + 6;

            motor_standby(true);
            motor_drive(toward_high_side, boost_pwm);
            vTaskDelay(pdMS_TO_TICKS(boost_burst));
            motor_stop();
            vTaskDelay(pdMS_TO_TICKS(50));

            continue;
        }

        little_move_retries = 0;
    }

    ESP_LOGW(TAG, "Move stopped after max attempts.");
    motor_stop();
    sync_public_state_from_current_position();
    if (actual_raw) {
        *actual_raw = s_last_published_raw;
    }
    s_motor_busy = false;
    s_ignore_user_until_tick = xTaskGetTickCount() + pdMS_TO_TICKS(USER_IGNORE_AFTER_MOVE_MS);

    return ESP_ERR_TIMEOUT;
}

esp_err_t iris_fader_move_to_raw(int target_raw)
{
    return iris_fader_move_to_raw_with_result(target_raw, NULL);
}

iris_fader_sample_t iris_fader_poll_user(void)
{
    iris_fader_sample_t out = {
        .raw = 0,
        .changed_by_user = false,
        .user_active = false,
    };

    if (!s_initialized) {
        return out;
    }

    int filtered = iris_fader_get_raw();
    TickType_t now = xTaskGetTickCount();

    out.raw = filtered;

    if (s_motor_busy) {
        return out;
    }

    if (now < s_ignore_user_until_tick) {
        return out;
    }

    if (abs_int(filtered - s_last_active_raw) >= USER_ACTIVE_THRESHOLD) {
        s_last_user_motion_tick = now;
        s_last_active_raw = filtered;
        out.user_active = true;
    } else if ((now - s_last_user_motion_tick) < pdMS_TO_TICKS(USER_IDLE_MS)) {
        out.user_active = true;
    }

    if (out.user_active && abs_int(filtered - s_last_published_raw) >= USER_PUBLISH_DEADZONE) {
        out.changed_by_user = true;
        s_last_published_raw = filtered;
    }

    return out;
}
