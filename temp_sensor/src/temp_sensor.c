#include "temp_sensor.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(temp_sensor, LOG_LEVEL_INF);

static const struct device *s_i2c_dev;
static temp_reading_t       s_log[TEMP_LOG_MAX_ENTRIES];
static uint32_t             s_log_count;
static uint32_t             s_log_head;

static int ads1115_start_conversion(void)
{
    uint8_t buf[3];
    uint16_t cfg = ADS1115_CONFIG_WORD;
    buf[0] = ADS1115_REG_CONFIG;
    buf[1] = (uint8_t)(cfg >> 8);
    buf[2] = (uint8_t)(cfg & 0xFF);
    return i2c_write(s_i2c_dev, buf, sizeof(buf), ADS1115_I2C_ADDR);
}

static int ads1115_wait_ready(void)
{
    uint8_t reg = ADS1115_REG_CONFIG;
    uint8_t rbuf[2];
    int ret;
    int tries = 20;
    while (tries-- > 0) {
        k_sleep(K_MSEC(5));
        ret = i2c_write_read(s_i2c_dev, ADS1115_I2C_ADDR, &reg, 1, rbuf, 2);
        if (ret < 0) return ret;
        uint16_t cfg_read = ((uint16_t)rbuf[0] << 8) | rbuf[1];
        if (cfg_read & ADS1115_CFG_OS) return 0;
    }
    LOG_WRN("ADS1115 conversion timeout");
    return -ETIMEDOUT;
}

static int ads1115_read_result(int16_t *raw_out)
{
    uint8_t reg = ADS1115_REG_CONVERT;
    uint8_t rbuf[2];
    int ret = i2c_write_read(s_i2c_dev, ADS1115_I2C_ADDR, &reg, 1, rbuf, 2);
    if (ret < 0) return ret;
    *raw_out = (int16_t)(((uint16_t)rbuf[0] << 8) | rbuf[1]);
    return 0;
}

static int32_t raw_to_millidegrees(int16_t raw)
{
    if (raw <= 0) raw = 1;

    int32_t v_mv = ((int32_t)raw * ADS1115_FSR_MV) / ADS1115_MAX_CODE;
    if (v_mv >= VCC_MV) v_mv = VCC_MV - 1;

    int32_t r_ntc_ohm = (int32_t)R_FIXED_OHM * (VCC_MV - v_mv) / v_mv;
    if (r_ntc_ohm <= 0) r_ntc_ohm = 1;

    double ln_ratio = log((double)r_ntc_ohm / (double)NTC_R0_OHM);
    double inv_T    = (1.0 / (double)NTC_T0_KELVIN) + (ln_ratio / (double)NTC_BETA);
    double T_kelvin = 1.0 / inv_T;
    double T_celsius = T_kelvin - 273.15;

    return (int32_t)(T_celsius * 1000.0);
}

int temp_sensor_init(const struct device *i2c)
{
    if (!device_is_ready(i2c)) { LOG_ERR("I2C device not ready"); return -ENODEV; }
    s_i2c_dev   = i2c;
    s_log_count = 0;
    s_log_head  = 0;
    int ret = ads1115_start_conversion();
    if (ret < 0) { LOG_ERR("ADS1115 not responding on I2C (addr 0x%02X): %d", ADS1115_I2C_ADDR, ret); return ret; }
    LOG_INF("ADS1115 temperature sensor initialised (addr 0x%02X)", ADS1115_I2C_ADDR);
    return 0;
}

int temp_sensor_read(temp_reading_t *out)
{
    if (!out) return -EINVAL;
    out->valid        = false;
    out->celsius_mdeg = 0;
    out->timestamp_ms = k_uptime_get_32();

    int ret = ads1115_start_conversion();
    if (ret < 0) { LOG_ERR("Failed to start ADS1115 conversion: %d", ret); goto store_and_return; }

    ret = ads1115_wait_ready();
    if (ret < 0) { LOG_ERR("ADS1115 conversion wait failed: %d", ret); goto store_and_return; }

    int16_t raw = 0;
    ret = ads1115_read_result(&raw);
    if (ret < 0) { LOG_ERR("Failed to read ADS1115 result: %d", ret); goto store_and_return; }

    out->celsius_mdeg = raw_to_millidegrees(raw);
    out->valid        = true;
    LOG_INF("Temp: %d.%03d degC  (raw ADC: %d)",
            out->celsius_mdeg / 1000,
            (out->celsius_mdeg >= 0) ? (out->celsius_mdeg % 1000) : (-(out->celsius_mdeg % 1000)),
            raw);

store_and_return:
    s_log[s_log_head] = *out;
    s_log_head = (s_log_head + 1) % TEMP_LOG_MAX_ENTRIES;
    if (s_log_count < TEMP_LOG_MAX_ENTRIES) s_log_count++;
    return ret;
}

const temp_reading_t *temp_sensor_get_log(uint32_t *count_out)
{
    if (count_out) *count_out = (s_log_count < TEMP_LOG_MAX_ENTRIES) ? s_log_count : TEMP_LOG_MAX_ENTRIES;
    return s_log;
}

void temp_sensor_clear_log(void)
{
    s_log_count = 0;
    s_log_head  = 0;
    LOG_INF("Temperature log cleared");
}
