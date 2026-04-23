/*
 * src/main.c
 * MAX30102 pulse oximeter — polling test application.
 *
 * Place a finger on the sensor after startup to see HR and SpO2 readings.
 * Raw IR/Red values are always printed so you can confirm sensor contact.
 */

#include <kernel.h>
#include <device.h>
#include <drivers/i2c.h>
#include <logging/log.h>
#include <usb/usb_device.h>
#include <drivers/uart.h>
#include "max30102.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Resolve I2C bus device from the overlay node: max30102@57 on i2c0 */
#define MAX30102_NODE   DT_NODELABEL(max30102)
#define I2C_BUS_NODE    DT_BUS(MAX30102_NODE)

#define READ_INTERVAL_MS    500
#define WARMUP_MS           3000

int main(void)
{
    /* Enable USB and wait for host to enumerate the CDC-ACM port.
     * Without this, the first printk output is lost before the
     * serial port opens on the host side. */
    usb_enable(NULL);
    k_msleep(2000);   /* 2 s — enough for Windows to assign the COM port */

    printk("\n=== MAX30102 Pulse Oximeter (Zephyr) ===\n");

    const struct device *i2c_dev = DEVICE_DT_GET(I2C_BUS_NODE);
    if (!device_is_ready(i2c_dev)) {
        printk("ERROR: I2C bus not ready\n");
        return -ENODEV;
    }

    max30102_config_t cfg = {
        .i2c_dev         = i2c_dev,
        .mode            = MODE_SPO2,
        .sample_rate     = SPO2_SR_100,         /* 100 samples/sec  */
        .pulse_width     = SPO2_PW_411US_18BIT, /* 18-bit ADC res   */
        .adc_range       = SPO2_ADC_RGE_4096,   /* 4096 nA full-scale */
        .led_current_red = LED_CURRENT_6MA,
        .led_current_ir  = LED_CURRENT_6MA,
        .sample_avg      = FIFO_SMP_AVE_4,      /* avg 4 sa / FIFO entry */
    };

    int ret = max30102_init(&cfg);
    if (ret < 0) {
        printk("ERROR: MAX30102 init failed (%d)\n", ret);
        return ret;
    }

    printk("Sensor ready. Warming up %d ms...\n", WARMUP_MS);
    k_msleep(WARMUP_MS);
    printk("Place finger on sensor.\n\n");

    max30102_data_t reading = { 0 };
    uint32_t tick = 0;

    while (1) {
        ret = max30102_read(&cfg, &reading);

        if (ret < 0) {
            printk("[%4u] ERROR reading sensor (%d)\n", tick, ret);
        } else if (!reading.valid) {
            printk("[%4u] Waiting...  IR=%6u  Red=%6u\n",
                   tick, reading.ir_raw, reading.red_raw);
        } else {
            printk("[%4u] HR: %3d bpm  |  SpO2: %3d %%  "
                   "(IR=%6u  Red=%6u)\n",
                   tick,
                   reading.heart_rate_bpm,
                   reading.spo2_percent,
                   reading.ir_raw,
                   reading.red_raw);
        }

        tick++;
        k_msleep(READ_INTERVAL_MS);
    }

    return 0;
}
