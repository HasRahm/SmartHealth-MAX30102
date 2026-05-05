/*
 * nrf52840_app/src/main.c  (updated — adds ADS1115 temperature sensor)
 *
 * Runs two sensors concurrently on the same I2C bus:
 *   • MAX30102  — heart-rate / SpO2   (I2C 0x57)
 *   • ADS1115   — NTC thermistor temp  (I2C 0x48, AIN0)
 *
 * Wiring summary:
 *   VCC → 3V3  |  GND → GND
 *   SDA → P0.26  |  SCL → P0.27   (shared bus for both ICs)
 *   ADS1115 ADDR → GND  (sets address to 0x48)
 *   Thermistor → AIN0 of ADS1115; 9.1 kΩ resistor from AIN0 → GND
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include "max30102.h"
#include "temp_sensor.h"

#define I2C_NODE            DT_NODELABEL(arduino_i2c)

#define WARMUP_MS           3000
#define READ_INTERVAL_MS    500

/* How often to dump the full temperature log (every N temp readings) */
#define TEMP_LOG_DUMP_EVERY 20

int main(void)
{
    printk("\n=== SmartHealth Monitor — nRF52840 DK / Zephyr ===\n");
    printk("    MAX30102 (HR/SpO2) + ADS1115 NTC Thermistor\n\n");

    /* ── Get shared I2C bus ─────────────────────────────────────────── */
    const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c)) {
        printk("ERROR: I2C bus not ready — check overlay and wiring\n");
        return -1;
    }

    /* ── Init MAX30102 ──────────────────────────────────────────────── */
    max30102_config_t cfg = {
        .i2c_dev         = i2c,
        .mode            = MODE_SPO2,
        .sample_rate     = SPO2_SR_100,
        .pulse_width     = SPO2_PW_411US_18BIT,
        .adc_range       = SPO2_ADC_RGE_32768,
        .led_current_red = 0xC0,
        .led_current_ir  = 0xC0,
        .sample_avg      = FIFO_SMP_AVE_4,
    };

    int ret = max30102_init(&cfg);
    if (ret < 0) {
        printk("ERROR: MAX30102 init failed (%d)\n", ret);
        return -1;
    }
    printk("MAX30102 ready.\n");

    /* ── Init ADS1115 temperature sensor ────────────────────────────── */
    ret = temp_sensor_init(i2c);
    if (ret < 0) {
        printk("ERROR: ADS1115/temp sensor init failed (%d)\n", ret);
        return -1;
    }
    printk("ADS1115 temperature sensor ready.\n");

    /* ── Warm-up delay ──────────────────────────────────────────────── */
    printk("\nWarming up %d ms — place finger on MAX30102...\n\n", WARMUP_MS);
    k_sleep(K_MSEC(WARMUP_MS));

    /* ── Main loop ──────────────────────────────────────────────────── */
    max30102_data_t  hr_data;
    temp_reading_t   temp_data;
    int tick = 0;

    while (1) {
        /* 1. Read MAX30102 (HR / SpO2) */
        ret = max30102_read(&cfg, &hr_data);

        if (ret < 0) {
            printk("[%4d] MAX30102 ERROR (%d)\n", tick, ret);
        } else if (!hr_data.valid) {
            printk("[%4d] HR: waiting...   IR=%-6u  Red=%-6u\n",
                   tick, hr_data.ir_raw, hr_data.red_raw);
        } else {
            printk("[%4d] HR: %3d bpm  SpO2: %3d %%  (IR=%-6u Red=%-6u)\n",
                   tick,
                   hr_data.heart_rate_bpm,
                   hr_data.spo2_percent,
                   hr_data.ir_raw,
                   hr_data.red_raw);
        }

        /* 2. Read ADS1115 temperature */
        ret = temp_sensor_read(&temp_data);

        if (ret < 0 || !temp_data.valid) {
            printk("[%4d] TEMP: read error (%d)\n", tick, ret);
        } else {
            int32_t deg  = temp_data.celsius_mdeg / 1000;
            int32_t mdeg = temp_data.celsius_mdeg % 1000;
            if (mdeg < 0) { mdeg = -mdeg; }

            printk("[%4d] TEMP: %d.%03d °C  (t=%u ms)\n",
                   tick, deg, mdeg, temp_data.timestamp_ms);
        }

        /* 3. Periodically print the stored temperature log */
        if ((tick > 0) && (tick % TEMP_LOG_DUMP_EVERY == 0)) {
            uint32_t count = 0;
            const temp_reading_t *log = temp_sensor_get_log(&count);

            printk("\n--- Temperature log dump (%u entries) ---\n", count);
            for (uint32_t i = 0; i < count; i++) {
                if (!log[i].valid) {
                    printk("  [%3u] INVALID (t=%u ms)\n",
                           i, log[i].timestamp_ms);
                } else {
                    int32_t d = log[i].celsius_mdeg / 1000;
                    int32_t m = log[i].celsius_mdeg % 1000;
                    if (m < 0) { m = -m; }
                    printk("  [%3u] %d.%03d °C  (t=%u ms)\n",
                           i, d, m, log[i].timestamp_ms);
                }
            }
            printk("-----------------------------------------\n\n");
        }

        tick++;
        k_sleep(K_MSEC(READ_INTERVAL_MS));
    }

    return 0;
}
