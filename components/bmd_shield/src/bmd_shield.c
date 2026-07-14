#include "bmd_shield.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

// ============================================================
// User config
// ============================================================
#define BMD_SHIELD_I2C_PORT_NUM    0
#define BMD_SHIELD_I2C_SDA_GPIO    GPIO_NUM_21
#define BMD_SHIELD_I2C_SCL_GPIO    GPIO_NUM_22
#define BMD_SHIELD_I2C_FREQ_HZ     100000

#define BMD_SHIELD_I2C_ADDR_A      0x6E
#define BMD_SHIELD_I2C_ADDR_B      0x37

// ============================================================
// Shield register map
// ============================================================
#define BMD_REG_IDENTITY           0x0000
#define BMD_REG_HWVERSION          0x0004
#define BMD_REG_FWVERSION          0x0006

#define BMD_REG_CONTROL            0x1000

#define BMD_REG_OCARM              0x2000
#define BMD_REG_OCLENGTH           0x2001
#define BMD_REG_OCDATA             0x2100

#define BMD_REG_ITARM              0x5000
#define BMD_REG_ITLENGTH           0x5001
#define BMD_REG_ITDATA             0x5100

#define BMD_CONTROL_OVERRIDE_CONTROL   0x01
#define BMD_CONTROL_OVERRIDE_TALLY     0x02
#define BMD_CONTROL_RESET_TALLY        0x04
#define BMD_CONTROL_OVERRIDE_OUTPUT    0x08

#define BMD_OCARM_ARM                  0x01
#define BMD_ITARM_ARM                  0x01

// ============================================================
// Camera control protocol constants
// ============================================================
#define BMD_CMD_CHANGE_CONFIG      0
#define BMD_OPERATION_ASSIGN       0

#define BMD_TYPE_VOID              0
#define BMD_TYPE_INT8              1
#define BMD_TYPE_INT16             2
#define BMD_TYPE_INT32             3
#define BMD_TYPE_FIXED16           128

#define BMD_GROUP_LENS             0
#define BMD_GROUP_VIDEO            1
#define BMD_GROUP_COLOR_CORRECTION 8

#define BMD_PARAM_APERTURE_NORM    3   // Lens 0.3
#define BMD_PARAM_GAIN             1   // Video 1.1
#define BMD_PARAM_MANUAL_WB        2   // Video 1.2

#define BMD_PARAM_CC_LIFT          0   // Color Correction 8.0
#define BMD_PARAM_CC_GAMMA         1   // Color Correction 8.1
#define BMD_PARAM_CC_GAIN          2   // Color Correction 8.2
#define BMD_PARAM_CC_OFFSET        3   // Color Correction 8.3
#define BMD_PARAM_CC_CONTRAST      4   // Color Correction 8.4
#define BMD_PARAM_CC_LUMA_MIX      5   // Color Correction 8.5
#define BMD_PARAM_CC_COLOR         6   // Color Correction 8.6
#define BMD_PARAM_CC_RESET         7   // Color Correction 8.7

#define BMD_CC_STEP                0.01f
#define BMD_CC_LIFT_MIN           -2.0f
#define BMD_CC_LIFT_MAX            2.0f
#define BMD_CC_GAMMA_MIN          -4.0f
#define BMD_CC_GAMMA_MAX           4.0f
#define BMD_CC_GAIN_MIN            0.0f
#define BMD_CC_GAIN_MAX            15.9995f
#define BMD_CC_OFFSET_MIN         -8.0f
#define BMD_CC_OFFSET_MAX          8.0f
#define BMD_CC_CONTRAST_MIN        0.0f
#define BMD_CC_CONTRAST_MAX        2.0f
#define BMD_CC_COLOR_HUE_MIN      -1.0f
#define BMD_CC_COLOR_HUE_MAX       1.0f
#define BMD_CC_COLOR_SAT_MIN       0.0f
#define BMD_CC_COLOR_SAT_MAX       2.0f

static const char *TAG = "BMD_SHIELD";

static bool s_ready = false;
static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_dev_handle = NULL;
static uint8_t s_device_address = 0;

// ============================================================
// Helpers
// ============================================================
static int clamp_int(int x, int lo, int hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static float clamp_float(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int16_t float_to_fixed16(float value)
{
    float clamped = clamp_float(value, -16.0f, 15.9995f);
    int scaled = (int)(clamped * 2048.0f);
    if (scaled < -32768) scaled = -32768;
    if (scaled > 32767) scaled = 32767;
    return (int16_t)scaled;
}

static esp_err_t bmd_i2c_write_reg(uint16_t reg, const uint8_t *data, size_t len)
{
    if (!s_dev_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > 255) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t buf[2 + 255];
    buf[0] = (uint8_t)(reg & 0xFF);
    buf[1] = (uint8_t)((reg >> 8) & 0xFF);

    if (len > 0 && data != NULL) {
        memcpy(&buf[2], data, len);
    }

    return i2c_master_transmit(s_dev_handle, buf, len + 2, 100);
}

static esp_err_t bmd_i2c_read_reg(uint16_t reg, uint8_t *data, size_t len)
{
    if (!s_dev_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > 255) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t regbuf[2];
    regbuf[0] = (uint8_t)(reg & 0xFF);
    regbuf[1] = (uint8_t)((reg >> 8) & 0xFF);

    return i2c_master_transmit_receive(s_dev_handle, regbuf, sizeof(regbuf), data, len, 100);
}

static esp_err_t bmd_reg_write8(uint16_t reg, uint8_t value)
{
    return bmd_i2c_write_reg(reg, &value, 1);
}

static esp_err_t bmd_reg_read8(uint16_t reg, uint8_t *value)
{
    return bmd_i2c_read_reg(reg, value, 1);
}

static esp_err_t bmd_reg_read16(uint16_t reg, uint16_t *value)
{
    uint8_t buf[2] = {0};
    esp_err_t err = bmd_i2c_read_reg(reg, buf, 2);
    if (err != ESP_OK) {
        return err;
    }

    *value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return ESP_OK;
}

static esp_err_t bmd_reg_read32(uint16_t reg, uint32_t *value)
{
    uint8_t buf[4] = {0};
    esp_err_t err = bmd_i2c_read_reg(reg, buf, 4);
    if (err != ESP_OK) {
        return err;
    }

    *value = ((uint32_t)buf[0]) |
             ((uint32_t)buf[1] << 8) |
             ((uint32_t)buf[2] << 16) |
             ((uint32_t)buf[3] << 24);
    return ESP_OK;
}

static void put_le16(uint8_t *dst, int16_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void put_le32(uint8_t *dst, int32_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

static esp_err_t bmd_arm_tally_read(void)
{
    return bmd_reg_write8(BMD_REG_ITARM, BMD_ITARM_ARM);
}

static esp_err_t bmd_arm_control_write(void)
{
    return bmd_reg_write8(BMD_REG_OCARM, BMD_OCARM_ARM);
}

static esp_err_t bmd_write_control_payload(const uint8_t *payload, uint8_t length)
{
    esp_err_t err;

    err = bmd_i2c_write_reg(BMD_REG_OCDATA, payload, length);
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_reg_write8(BMD_REG_OCLENGTH, length);
    if (err != ESP_OK) {
        return err;
    }

    return bmd_arm_control_write();
}

static esp_err_t bmd_send_command_int8(uint8_t camera, uint8_t category, uint8_t parameter, uint8_t operation, const int8_t *values, size_t count)
{
    const uint8_t header_len = 4;
    const uint8_t param_len = 1;
    const uint8_t payload_len = (uint8_t)(4 + (param_len * count));
    const uint8_t padding_len = (payload_len % 4) ? (4 - (payload_len % 4)) : 0;
    const uint8_t total_len = header_len + payload_len + padding_len;

    uint8_t payload[4 + 4 + 8] = {0}; // enough for our current uses
    if (total_len > sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    payload[0] = camera;
    payload[1] = payload_len;
    payload[2] = BMD_CMD_CHANGE_CONFIG;
    payload[3] = 0;

    payload[4] = category;
    payload[5] = parameter;
    payload[6] = BMD_TYPE_INT8;
    payload[7] = operation;

    for (size_t i = 0; i < count; i++) {
        payload[8 + i] = (uint8_t)values[i];
    }

    return bmd_write_control_payload(payload, total_len);
}

static esp_err_t bmd_send_command_int16(uint8_t camera, uint8_t category, uint8_t parameter, uint8_t operation, const int16_t *values, size_t count)
{
    const uint8_t header_len = 4;
    const uint8_t param_len = 2;
    const uint8_t payload_len = (uint8_t)(4 + (param_len * count));
    const uint8_t padding_len = (payload_len % 4) ? (4 - (payload_len % 4)) : 0;
    const uint8_t total_len = header_len + payload_len + padding_len;

    uint8_t payload[4 + 8 + 8] = {0}; // enough for WB/tint
    if (total_len > sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    payload[0] = camera;
    payload[1] = payload_len;
    payload[2] = BMD_CMD_CHANGE_CONFIG;
    payload[3] = 0;

    payload[4] = category;
    payload[5] = parameter;
    payload[6] = BMD_TYPE_INT16;
    payload[7] = operation;

    for (size_t i = 0; i < count; i++) {
        put_le16(&payload[8 + (i * 2)], values[i]);
    }

    return bmd_write_control_payload(payload, total_len);
}

static esp_err_t bmd_send_command_fixed16(uint8_t camera, uint8_t category, uint8_t parameter, uint8_t operation, const float *values, size_t count)
{
    const uint8_t header_len = 4;
    const uint8_t param_len = 2;
    const uint8_t payload_len = (uint8_t)(4 + (param_len * count));
    const uint8_t padding_len = (payload_len % 4) ? (4 - (payload_len % 4)) : 0;
    const uint8_t total_len = header_len + payload_len + padding_len;

    uint8_t payload[4 + 8 + 8] = {0};
    if (total_len > sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    payload[0] = camera;
    payload[1] = payload_len;
    payload[2] = BMD_CMD_CHANGE_CONFIG;
    payload[3] = 0;

    payload[4] = category;
    payload[5] = parameter;
    payload[6] = BMD_TYPE_FIXED16;
    payload[7] = operation;

    for (size_t i = 0; i < count; i++) {
        int16_t fixed = float_to_fixed16(values[i]);
        put_le16(&payload[8 + (i * 2)], fixed);
    }

    return bmd_write_control_payload(payload, total_len);
}

static esp_err_t bmd_send_command_void(uint8_t camera, uint8_t category, uint8_t parameter, uint8_t operation)
{
    const uint8_t header_len = 4;
    const uint8_t payload_len = 4;
    const uint8_t total_len = header_len + payload_len;

    uint8_t payload[8] = {0};

    payload[0] = camera;
    payload[1] = payload_len;
    payload[2] = BMD_CMD_CHANGE_CONFIG;
    payload[3] = 0;

    payload[4] = category;
    payload[5] = parameter;
    payload[6] = BMD_TYPE_VOID;
    payload[7] = operation;

    return bmd_write_control_payload(payload, total_len);
}

static int gain_db_to_iso_code(int gain_db)
{
    // Temporary simple mapping for BM protocol values:
    // 1=100, 2=200, 4=400, 8=800, 16=1600 ISO
    if (gain_db <= 0) return 1;
    if (gain_db <= 6) return 2;
    if (gain_db <= 12) return 4;
    if (gain_db <= 18) return 8;
    return 16;
}

static float cc_offset_from_steps(int value)
{
    return (float)value * BMD_CC_STEP;
}

static float cc_gain_from_steps(int value)
{
    return 1.0f + ((float)value * BMD_CC_STEP);
}

static float cc_percent_from_steps(int value)
{
    return (float)value / 100.0f;
}

static esp_err_t bmd_add_device(uint8_t addr)
{
    if (!s_bus_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = BMD_SHIELD_I2C_FREQ_HZ,
        .scl_wait_us = 0,
        .flags.disable_ack_check = 0,
    };

    return i2c_master_bus_add_device(s_bus_handle, &dev_config, &s_dev_handle);
}

static esp_err_t bmd_try_address(uint8_t addr)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Probing I2C address 0x%02X", addr);

    err = i2c_master_probe(s_bus_handle, addr, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No ACK at 0x%02X (%s)", addr, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Device ACK at 0x%02X", addr);

    err = bmd_add_device(addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device 0x%02X: %s", addr, esp_err_to_name(err));
        return err;
    }

    s_device_address = addr;
    return ESP_OK;
}

// ============================================================
// Public API
// ============================================================
esp_err_t bmd_shield_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = BMD_SHIELD_I2C_PORT_NUM,
        .sda_io_num = BMD_SHIELD_I2C_SDA_GPIO,
        .scl_io_num = BMD_SHIELD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed creating I2C bus: %s", esp_err_to_name(err));
        return err;
    }

    esp_err_t err_a = bmd_try_address(BMD_SHIELD_I2C_ADDR_A);
    if (err_a != ESP_OK) {
        esp_err_t err_b = bmd_try_address(BMD_SHIELD_I2C_ADDR_B);
        if (err_b != ESP_OK) {
            ESP_LOGE(TAG, "Could not find Blackmagic shield on 0x%02X or 0x%02X",
                     BMD_SHIELD_I2C_ADDR_A, BMD_SHIELD_I2C_ADDR_B);
            return ESP_FAIL;
        }
    }

    uint32_t identity = 0;
    uint16_t hw = 0;
    uint16_t fw = 0;
    err = bmd_reg_read32(BMD_REG_IDENTITY, &identity);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed reading IDENTITY from 0x%02X: %s", s_device_address, esp_err_to_name(err));
        return err;
    }

    err = bmd_reg_read16(BMD_REG_HWVERSION, &hw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed reading HWVERSION from 0x%02X: %s", s_device_address, esp_err_to_name(err));
        return err;
    }

    err = bmd_reg_read16(BMD_REG_FWVERSION, &fw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed reading FWVERSION from 0x%02X: %s", s_device_address, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Shield address=0x%02X", s_device_address);
    ESP_LOGI(TAG, "Shield identity=0x%08" PRIx32, identity);
    ESP_LOGI(TAG, "Shield HW version=%u.%u", (hw >> 8) & 0xFF, hw & 0xFF);
    ESP_LOGI(TAG, "Shield FW version=%u.%u", (fw >> 8) & 0xFF, fw & 0xFF);

    if (identity != 0x43494453UL) {
        ESP_LOGE(TAG, "Unexpected shield identity, expected SDIC");
        return ESP_ERR_INVALID_RESPONSE;
    }

    {
        uint8_t control = BMD_CONTROL_OVERRIDE_CONTROL;
        err = bmd_reg_write8(BMD_REG_CONTROL, control);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed writing CONTROL: %s", esp_err_to_name(err));
            return err;
        }
    }

    err = bmd_arm_tally_read();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed arming tally read: %s", esp_err_to_name(err));
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "Blackmagic shield ready");
    return ESP_OK;
}

bool bmd_shield_is_ready(void)
{
    return s_ready;
}

i2c_master_bus_handle_t bmd_shield_get_bus_handle(void)
{
    return s_bus_handle;
}

esp_err_t bmd_shield_poll_tally(uint8_t *tally_bytes, size_t max_len, size_t *out_len)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!tally_bytes || !out_len || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t length = 0;
    esp_err_t err = bmd_reg_read8(BMD_REG_ITLENGTH, &length);
    if (err != ESP_OK) {
        return err;
    }

    if (length == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t to_read = (length < max_len) ? length : max_len;

    err = bmd_i2c_read_reg(BMD_REG_ITDATA, tally_bytes, to_read);
    if (err != ESP_OK) {
        return err;
    }

    *out_len = to_read;

    err = bmd_arm_tally_read();
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t bmd_shield_send_aperture_normalized(uint8_t camera_number_1_based, float normalized)
{
    float value = clamp_float(normalized, 0.0f, 1.0f);
    ESP_LOGI(TAG, "TX aperture_norm cam=%u value=%.3f", camera_number_1_based, value);
    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_LENS,
        BMD_PARAM_APERTURE_NORM,
        BMD_OPERATION_ASSIGN,
        &value,
        1
    );
}

esp_err_t bmd_shield_send_gain_iso_code(uint8_t camera_number_1_based, int iso_code)
{
    int8_t gain = (int8_t)clamp_int(iso_code, 1, 16);
    ESP_LOGI(TAG, "TX gain cam=%u iso_code=%d", camera_number_1_based, gain);
    return bmd_send_command_int8(
        camera_number_1_based,
        BMD_GROUP_VIDEO,
        BMD_PARAM_GAIN,
        BMD_OPERATION_ASSIGN,
        &gain,
        1
    );
}

esp_err_t bmd_shield_send_manual_white_balance(uint8_t camera_number_1_based, int temperature_k, int tint)
{
    int16_t values[2];
    values[0] = (int16_t)clamp_int(temperature_k, 2500, 10000);
    values[1] = (int16_t)clamp_int(tint, -50, 50);

    ESP_LOGI(TAG, "TX manual_wb cam=%u temp=%d tint=%d", camera_number_1_based, values[0], values[1]);

    return bmd_send_command_int16(
        camera_number_1_based,
        BMD_GROUP_VIDEO,
        BMD_PARAM_MANUAL_WB,
        BMD_OPERATION_ASSIGN,
        values,
        2
    );
}

esp_err_t bmd_shield_send_lift(uint8_t camera_number_1_based,
                               float red, float green, float blue, float luma)
{
    float values[4];
    values[0] = clamp_float(red, BMD_CC_LIFT_MIN, BMD_CC_LIFT_MAX);
    values[1] = clamp_float(green, BMD_CC_LIFT_MIN, BMD_CC_LIFT_MAX);
    values[2] = clamp_float(blue, BMD_CC_LIFT_MIN, BMD_CC_LIFT_MAX);
    values[3] = clamp_float(luma, BMD_CC_LIFT_MIN, BMD_CC_LIFT_MAX);

    ESP_LOGI(TAG,
             "TX lift cam=%u r=%.3f g=%.3f b=%.3f luma=%.3f",
             camera_number_1_based,
             values[0],
             values[1],
             values[2],
             values[3]);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_LIFT,
        BMD_OPERATION_ASSIGN,
        values,
        4
    );
}

esp_err_t bmd_shield_send_gain_adjust(uint8_t camera_number_1_based,
                                      float red, float green, float blue, float luma)
{
    float values[4];
    values[0] = clamp_float(red, BMD_CC_GAIN_MIN, BMD_CC_GAIN_MAX);
    values[1] = clamp_float(green, BMD_CC_GAIN_MIN, BMD_CC_GAIN_MAX);
    values[2] = clamp_float(blue, BMD_CC_GAIN_MIN, BMD_CC_GAIN_MAX);
    values[3] = clamp_float(luma, BMD_CC_GAIN_MIN, BMD_CC_GAIN_MAX);

    ESP_LOGI(TAG,
             "TX gain_adjust cam=%u r=%.3f g=%.3f b=%.3f luma=%.3f",
             camera_number_1_based,
             values[0],
             values[1],
             values[2],
             values[3]);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_GAIN,
        BMD_OPERATION_ASSIGN,
        values,
        4
    );
}

esp_err_t bmd_shield_send_gamma_adjust(uint8_t camera_number_1_based,
                                       float red, float green, float blue, float luma)
{
    float values[4];
    values[0] = clamp_float(red, BMD_CC_GAMMA_MIN, BMD_CC_GAMMA_MAX);
    values[1] = clamp_float(green, BMD_CC_GAMMA_MIN, BMD_CC_GAMMA_MAX);
    values[2] = clamp_float(blue, BMD_CC_GAMMA_MIN, BMD_CC_GAMMA_MAX);
    values[3] = clamp_float(luma, BMD_CC_GAMMA_MIN, BMD_CC_GAMMA_MAX);

    ESP_LOGI(TAG,
             "TX gamma_adjust cam=%u r=%.3f g=%.3f b=%.3f luma=%.3f",
             camera_number_1_based,
             values[0],
             values[1],
             values[2],
             values[3]);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_GAMMA,
        BMD_OPERATION_ASSIGN,
        values,
        4
    );
}

esp_err_t bmd_shield_send_offset_adjust(uint8_t camera_number_1_based,
                                        float red, float green, float blue, float luma)
{
    float values[4];
    values[0] = clamp_float(red, BMD_CC_OFFSET_MIN, BMD_CC_OFFSET_MAX);
    values[1] = clamp_float(green, BMD_CC_OFFSET_MIN, BMD_CC_OFFSET_MAX);
    values[2] = clamp_float(blue, BMD_CC_OFFSET_MIN, BMD_CC_OFFSET_MAX);
    values[3] = clamp_float(luma, BMD_CC_OFFSET_MIN, BMD_CC_OFFSET_MAX);

    ESP_LOGI(TAG,
             "TX offset_adjust cam=%u r=%.3f g=%.3f b=%.3f luma=%.3f",
             camera_number_1_based,
             values[0],
             values[1],
             values[2],
             values[3]);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_OFFSET,
        BMD_OPERATION_ASSIGN,
        values,
        4
    );
}

esp_err_t bmd_shield_send_contrast_adjust(uint8_t camera_number_1_based,
                                          float pivot, float adjust)
{
    float values[2];
    values[0] = clamp_float(pivot, 0.0f, BMD_CC_CONTRAST_MAX);
    values[1] = clamp_float(adjust, BMD_CC_CONTRAST_MIN, BMD_CC_CONTRAST_MAX);

    ESP_LOGI(TAG,
             "TX contrast_adjust cam=%u pivot=%.3f adjust=%.3f",
             camera_number_1_based,
             values[0],
             values[1]);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_CONTRAST,
        BMD_OPERATION_ASSIGN,
        values,
        2
    );
}

esp_err_t bmd_shield_send_luma_mix(uint8_t camera_number_1_based, float luma_mix)
{
    float value = clamp_float(luma_mix, 0.0f, BMD_CC_CONTRAST_MAX);

    ESP_LOGI(TAG, "TX luma_mix cam=%u value=%.3f", camera_number_1_based, value);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_LUMA_MIX,
        BMD_OPERATION_ASSIGN,
        &value,
        1
    );
}

esp_err_t bmd_shield_send_color_adjust(uint8_t camera_number_1_based, float hue, float saturation)
{
    float values[2];
    values[0] = clamp_float(hue, BMD_CC_COLOR_HUE_MIN, BMD_CC_COLOR_HUE_MAX);
    values[1] = clamp_float(saturation, BMD_CC_COLOR_SAT_MIN, BMD_CC_COLOR_SAT_MAX);

    ESP_LOGI(TAG,
             "TX color_adjust cam=%u hue=%.3f sat=%.3f",
             camera_number_1_based,
             values[0],
             values[1]);

    return bmd_send_command_fixed16(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_COLOR,
        BMD_OPERATION_ASSIGN,
        values,
        2
    );
}

esp_err_t bmd_shield_send_color_correction_reset(uint8_t camera_number_1_based)
{
    ESP_LOGI(TAG, "TX color_correction_reset cam=%u", camera_number_1_based);

    return bmd_send_command_void(
        camera_number_1_based,
        BMD_GROUP_COLOR_CORRECTION,
        BMD_PARAM_CC_RESET,
        BMD_OPERATION_ASSIGN
    );
}

esp_err_t bmd_shield_send_camera_state(uint8_t camera_number_1_based, const openrcp_camera_state_t *cam)
{
    if (!cam) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;

    err = bmd_shield_send_aperture_normalized(camera_number_1_based, cam->iris_normalized);
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_gain_iso_code(camera_number_1_based, gain_db_to_iso_code(cam->gain_db));
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_manual_white_balance(camera_number_1_based, cam->white_balance_k, cam->tint);
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_lift(
        camera_number_1_based,
        cc_offset_from_steps(cam->black_r),
        cc_offset_from_steps(cam->black_g),
        cc_offset_from_steps(cam->black_b),
        cc_offset_from_steps(cam->black_level)
    );
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_gain_adjust(
        camera_number_1_based,
        cc_gain_from_steps(cam->white_r),
        cc_gain_from_steps(cam->white_g),
        cc_gain_from_steps(cam->white_b),
        1.0f
    );
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_gamma_adjust(
        camera_number_1_based,
        cc_offset_from_steps(cam->gamma_r),
        cc_offset_from_steps(cam->gamma_g),
        cc_offset_from_steps(cam->gamma_b),
        cc_offset_from_steps(cam->gamma_luma)
    );
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_offset_adjust(
        camera_number_1_based,
        cc_offset_from_steps(cam->offset_r),
        cc_offset_from_steps(cam->offset_g),
        cc_offset_from_steps(cam->offset_b),
        cc_offset_from_steps(cam->offset_luma)
    );
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_contrast_adjust(
        camera_number_1_based,
        cc_percent_from_steps(cam->contrast_pivot),
        cc_percent_from_steps(cam->contrast_adjust)
    );
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_luma_mix(camera_number_1_based, cc_percent_from_steps(cam->luma_mix));
    if (err != ESP_OK) {
        return err;
    }

    err = bmd_shield_send_color_adjust(
        camera_number_1_based,
        cc_percent_from_steps(cam->hue),
        cc_percent_from_steps(cam->saturation)
    );
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}
