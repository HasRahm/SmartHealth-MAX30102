/*
 * temp_test_app/src/main.c
 *
 * Standalone test for ADS1115 + NTC thermistor (voltage divider).
 * Does NOT touch the MAX30102 — safe to run without your teammate's sensor.
 *
 * Wiring:
 *   ADS1115 SDA → P0.26  |  SCL → P0.27  |  VCC → 3V3  |  GND → GND
 *   ADS1115 ADDR → GND   (I2C address = 0x48)
 *   Thermistor + 9.1kΩ divider → AIN0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include "temp_sensor.h"

#define I2C_NODE         DT_NODELABEL(arduino_i2c)
#define READ_INTERVAL_MS 1000
#define NUM_READINGS     20     /* take 20 readings then dump full log */

int main(void)
{
    printk("\n========================================\n");
    printk("  ADS1115 Thermistor — Standalone Test  \n");
    printk("========================================\n\n");

    const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c)) {
        printk("FAIL: I2C bus not ready. Check SDA/SCL wiring.\n");
        return -1;
    }
    printk("I2C bus ready.\n");

   /* I2C scanner */
   printk("Scanning I2C bus...\n");
   for (uint8_t addr = 1; addr < 127; addr++) {
    	uint8_t dummy;
    	int ret2 = i2c_read(i2c, &dummy, 1, addr);
    	if (ret2 == 0) {
        	printk("Found device at 0x%02X\n", addr);
        }
    }
    printk("Scan done.\n");
	
    int ret = temp_sensor_init(i2c);
    if (ret < 0) {
        printk("FAIL: ADS1115 not found on bus (addr 0x48). "
               "Check wiring and ADDR pin.\n");
        return -1;
    }
    printk("ADS1115 found and initialised.\n\n");

    /* ── Take NUM_READINGS samples ───────────────────────────────────── */
    printk("Taking %d readings at %d ms intervals...\n\n",
           NUM_READINGS, READ_INTERVAL_MS);

    for (int i = 0; i < NUM_READINGS; i++) {
        temp_reading_t r;
        ret = temp_sensor_read(&r);

        if (ret < 0 || !r.valid) {
            printk("[%2d/%2d] ERROR: read failed (%d)\n",
                   i + 1, NUM_READINGS, ret);
        } else {
            int32_t deg  =  r.celsius_mdeg / 1000;
            int32_t mdeg = (r.celsius_mdeg >= 0)
                           ?  (r.celsius_mdeg % 1000)
                           : -(r.celsius_mdeg % 1000);

            printk("[%2d/%2d] Temperature: %d.%03d °C   (t = %u ms)\n",
                   i + 1, NUM_READINGS, deg, mdeg, r.timestamp_ms);
        }

        k_sleep(K_MSEC(READ_INTERVAL_MS));
    }

    /* ── Dump the full stored log ─────────────────────────────────────── */
    uint32_t count = 0;
    const temp_reading_t *log = temp_sensor_get_log(&count);

    printk("\n============ Stored log (%u entries) ============\n", count);
    for (uint32_t i = 0; i < count; i++) {
        if (!log[i].valid) {
            printk("  [%2u] INVALID  (t = %u ms)\n", i, log[i].timestamp_ms);
        } else {
            int32_t deg  =  log[i].celsius_mdeg / 1000;
            int32_t mdeg = (log[i].celsius_mdeg >= 0)
                           ?  (log[i].celsius_mdeg % 1000)
                           : -(log[i].celsius_mdeg % 1000);
            printk("  [%2u] %d.%03d °C  (t = %u ms)\n",
                   i, deg, mdeg, log[i].timestamp_ms);
        }
    }
    printk("=================================================\n\n");

    printk("Test complete.\n");
    return 0;
}
