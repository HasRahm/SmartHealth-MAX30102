/*
 * nrf52840_app/src/main.c
 * MAX30102 pulse oximeter on nRF52840 DK (Zephyr RTOS)
 *
 * Serial output: COM14 at 115200 baud
 * Wiring (Arduino header):
 *   VCC → 3V3  |  GND → GND
 *   SDA → SDA (P0.26)  |  SCL → SCL (P0.27)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include "max30102.h"

/* I2C bus node — arduino_i2c maps to i2c0 on P0.26/P0.27 */
#define I2C_NODE    DT_NODELABEL(arduino_i2c)

#define WARMUP_MS       3000
#define READ_INTERVAL_MS 500

int main(void)
{
    printk("\n=== MAX30102 Pulse Oximeter (nRF52840 DK / Zephyr) ===\n");

    /* Get I2C device from device tree */
    const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c)) {
        printk("ERROR: I2C bus not ready — check overlay and wiring\n");
        return -1;
    }

    /* Sensor configuration */
    max30102_config_t cfg = {
        .i2c_dev        = i2c,
        .mode           = MODE_SPO2,
        .sample_rate    = SPO2_SR_100,
        .pulse_width    = SPO2_PW_411US_18BIT,
        .adc_range      = SPO2_ADC_RGE_32768,  /* max range — needed with high LED current */
        .led_current_red = 0xC0,              /* ~38 mA — needed for clone sensors */
        .led_current_ir  = 0xC0,
        .sample_avg     = FIFO_SMP_AVE_4,
    };

    /* Init sensor */
    int ret = max30102_init(&cfg);
    if (ret < 0) {
        printk("ERROR: Sensor init failed (%d) — check VCC/SDA/SCL wiring\n", ret);
        return -1;
    }

    printk("Sensor ready. Warming up %d ms...\n", WARMUP_MS);
    k_sleep(K_MSEC(WARMUP_MS));
    printk("Place finger on sensor.\n\n");

    max30102_data_t data;
    int tick = 0;

    while (1) {
        ret = max30102_read(&cfg, &data);

        if (ret < 0) {
            printk("[%4d] ERROR reading sensor (%d)\n", tick, ret);
        } else if (!data.valid) {
            printk("[%4d] Waiting...  IR=%6u  Red=%6u\n",
                   tick, data.ir_raw, data.red_raw);
        } else {
            printk("[%4d] HR: %3d bpm  |  SpO2: %3d %%  (IR=%6u  Red=%6u)\n",
                   tick,
                   data.heart_rate_bpm,
                   data.spo2_percent,
                   data.ir_raw,
                   data.red_raw);
        }

        tick++;
        k_sleep(K_MSEC(READ_INTERVAL_MS));
    }

    return 0;
}
