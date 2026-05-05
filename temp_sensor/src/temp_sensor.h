/*
 * temp_sensor.h
 * NTC Thermistor temperature reading via ADS1115 ADC (I2C)
 *
 * Hardware setup:
 *   - ADS1115 connected via I2C (SDA = P0.26, SCL = P0.27)
 *   - NTC thermistor in voltage divider with 9.1kΩ pull-down to GND
 *   - Top rail = 3.3V, thermistor output → AIN0 (A0) of ADS1115
 *   - ADS1115 I2C address: 0x48 (ADDR pin → GND)
 */

#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdint.h>
#include <stdbool.h>

/* ── ADS1115 I2C address ──────────────────────────────────────────────────── */
#define ADS1115_I2C_ADDR        0x48   /* ADDR pin tied to GND */

/* ── ADS1115 register pointers ───────────────────────────────────────────── */
#define ADS1115_REG_CONVERT     0x00
#define ADS1115_REG_CONFIG      0x01

/*
 * Config register (16-bit):
 *   OS[15]      = 1        → start single-shot conversion
 *   MUX[14:12]  = 100b     → AIN0 vs GND (single-ended)
 *   PGA[11:9]   = 001b     → FSR = ±4.096 V  (covers 3.3 V rail)
 *   MODE[8]     = 1        → single-shot
 *   DR[7:5]     = 100b     → 128 SPS
 *   COMP[4:0]   = 00011b   → comparator disabled, active-low, non-latching
 */
#define ADS1115_CFG_OS          (1u << 15)
#define ADS1115_CFG_MUX_AIN0   (0x4u << 12)
#define ADS1115_CFG_PGA_4V096  (0x1u << 9)
#define ADS1115_CFG_MODE_SS    (1u << 8)
#define ADS1115_CFG_DR_128SPS  (0x4u << 5)
#define ADS1115_CFG_COMP_OFF   (0x3u << 0)

#define ADS1115_CONFIG_WORD  (ADS1115_CFG_OS       | \
                              ADS1115_CFG_MUX_AIN0 | \
                              ADS1115_CFG_PGA_4V096 | \
                              ADS1115_CFG_MODE_SS  | \
                              ADS1115_CFG_DR_128SPS | \
                              ADS1115_CFG_COMP_OFF)

/* ── Thermistor / voltage-divider parameters ─────────────────────────────── */
#define VCC_MV              3300     /* supply rail in millivolts            */
#define R_FIXED_OHM         9100     /* 9.1 kΩ pull-down resistor            */

/*
 * Steinhart–Hart β-parameter equation:
 *   1/T = 1/T0 + (1/β) · ln(R/R0)
 *
 * Typical NTC 10 kΩ @ 25 °C values — adjust to your part's datasheet.
 */
#define NTC_BETA            3964     /* β coefficient (K)                    */
#define NTC_R0_OHM          10000    /* resistance at reference temp (Ω)     */
#define NTC_T0_KELVIN       298      /* reference temp 25 °C in Kelvin       */

/* ── ADS1115 full-scale for PGA ±4.096 V ────────────────────────────────── */
#define ADS1115_FSR_MV      4096     /* full-scale range in mV               */
#define ADS1115_MAX_CODE    32767    /* 15-bit positive range                 */

/* ── Storage ─────────────────────────────────────────────────────────────── */
#define TEMP_LOG_MAX_ENTRIES  128    /* circular buffer depth                 */

typedef struct {
    int32_t  celsius_mdeg;  /* temperature in milli-°C (e.g. 23500 = 23.5°C) */
    uint32_t timestamp_ms;  /* k_uptime_get_32() at time of reading           */
    bool     valid;         /* false if ADC read or conversion failed         */
} temp_reading_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the ADS1115 and clear the log buffer.
 * @param  i2c  Ready I2C device pointer (arduino_i2c).
 * @return 0 on success, negative errno on failure.
 */
int temp_sensor_init(const struct device *i2c);

/**
 * @brief  Trigger one conversion, wait for result, compute temperature,
 *         append to circular log, and return the reading.
 * @param  out  Filled with the latest reading (may have valid=false on error).
 * @return 0 on success, negative errno on I2C error.
 */
int temp_sensor_read(temp_reading_t *out);

/**
 * @brief  Return a pointer to the internal circular log and its fill count.
 * @param  count_out  Set to the number of valid entries stored so far
 *                    (saturates at TEMP_LOG_MAX_ENTRIES).
 * @return Pointer to the log array (do not free).
 */
const temp_reading_t *temp_sensor_get_log(uint32_t *count_out);

/**
 * @brief  Clear the log buffer and reset the entry counter.
 */
void temp_sensor_clear_log(void);

#endif /* TEMP_SENSOR_H */
