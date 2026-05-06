#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <stdbool.h>

#define I2C_NODE DT_NODELABEL(i2c0)
static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_NODE);

/* I2C addresses */
#define ICM20948_ADDR_PRIMARY   0x69
#define ICM20948_ADDR_ALT       0x68
#define ADXL375_ADDR            0x53

/* ICM-20948 registers */
#define ICM20948_REG_BANK_SEL   0x7F
#define ICM20948_WHO_AM_I       0x00
#define ICM20948_PWR_MGMT_1     0x06
#define ICM20948_PWR_MGMT_2     0x07
#define ICM20948_ACCEL_XOUT_H   0x2D
#define ICM20948_WHOAMI_VALUE   0xEA

/* ADXL375 registers */
#define ADXL375_DEVID           0x00
#define ADXL375_BW_RATE         0x2C
#define ADXL375_POWER_CTL       0x2D
#define ADXL375_DATAX0          0x32
#define ADXL375_DEVID_VALUE     0xE5

#define ADXL375_G_PER_LSB       0.049f

static uint8_t icm_addr = ICM20948_ADDR_PRIMARY;

static int reg_write_u8(uint8_t addr, uint8_t reg, uint8_t val)
{
    return i2c_reg_write_byte(i2c_dev, addr, reg, val);
}

static int reg_read_u8(uint8_t addr, uint8_t reg, uint8_t *val)
{
    return i2c_reg_read_byte(i2c_dev, addr, reg, val);
}

static int reg_read_bytes(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_write_read(i2c_dev, addr, &reg, 1, buf, len);
}

static int icm_set_bank(uint8_t bank)
{
    return reg_write_u8(icm_addr, ICM20948_REG_BANK_SEL, bank << 4);
}

static int icm_read_bank_u8(uint8_t bank, uint8_t reg, uint8_t *val)
{
    int ret = icm_set_bank(bank);
    if (ret) {
        return ret;
    }

    return reg_read_u8(icm_addr, reg, val);
}

static int icm_read_bank_bytes(uint8_t bank, uint8_t reg, uint8_t *buf, size_t len)
{
    int ret = icm_set_bank(bank);
    if (ret) {
        return ret;
    }

    return reg_read_bytes(icm_addr, reg, buf, len);
}

static int icm_write_bank_u8(uint8_t bank, uint8_t reg, uint8_t val)
{
    int ret = icm_set_bank(bank);
    if (ret) {
        return ret;
    }

    return reg_write_u8(icm_addr, reg, val);
}

static int icm_detect(void)
{
    uint8_t whoami = 0;
    int ret;

    icm_addr = ICM20948_ADDR_PRIMARY;
    ret = icm_read_bank_u8(0, ICM20948_WHO_AM_I, &whoami);

    if (ret == 0 && whoami == ICM20948_WHOAMI_VALUE) {
        return 0;
    }

    icm_addr = ICM20948_ADDR_ALT;
    ret = icm_read_bank_u8(0, ICM20948_WHO_AM_I, &whoami);

    if (ret == 0 && whoami == ICM20948_WHOAMI_VALUE) {
        return 0;
    }

    return -ENODEV;
}

static int icm_init(void)
{
    int ret = icm_detect();

    if (ret) {
        return ret;
    }

    /* Reset ICM */
    ret = icm_write_bank_u8(0, ICM20948_PWR_MGMT_1, 0x80);
    if (ret) {
        return ret;
    }

    k_msleep(100);

    /* Wake device */
    ret = icm_write_bank_u8(0, ICM20948_PWR_MGMT_1, 0x01);
    if (ret) {
        return ret;
    }

    k_msleep(10);

    /* Enable accel + gyro */
    ret = icm_write_bank_u8(0, ICM20948_PWR_MGMT_2, 0x00);
    if (ret) {
        return ret;
    }

    k_msleep(10);

    return icm_set_bank(0);
}

static int icm_read_accel_gyro_raw(int16_t *ax, int16_t *ay, int16_t *az,
                                   int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[12];
    int ret = icm_read_bank_bytes(0, ICM20948_ACCEL_XOUT_H, buf, sizeof(buf));

    if (ret) {
        return ret;
    }

    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
    *az = (int16_t)((buf[4] << 8) | buf[5]);

    *gx = (int16_t)((buf[6] << 8) | buf[7]);
    *gy = (int16_t)((buf[8] << 8) | buf[9]);
    *gz = (int16_t)((buf[10] << 8) | buf[11]);

    return 0;
}

static int adxl_detect(void)
{
    uint8_t devid = 0;
    int ret = reg_read_u8(ADXL375_ADDR, ADXL375_DEVID, &devid);

    if (ret) {
        return ret;
    }

    return (devid == ADXL375_DEVID_VALUE) ? 0 : -ENODEV;
}

static int adxl_init(void)
{
    int ret = adxl_detect();

    if (ret) {
        return ret;
    }

    /* Standby */
    ret = reg_write_u8(ADXL375_ADDR, ADXL375_POWER_CTL, 0x00);
    if (ret) {
        return ret;
    }

    /* 200 Hz output rate */
    ret = reg_write_u8(ADXL375_ADDR, ADXL375_BW_RATE, 0x0B);
    if (ret) {
        return ret;
    }

    /* Measurement mode */
    ret = reg_write_u8(ADXL375_ADDR, ADXL375_POWER_CTL, 0x08);
    if (ret) {
        return ret;
    }

    k_msleep(10);

    return 0;
}

static int adxl_read_accel_raw(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t buf[6];
    int ret = reg_read_bytes(ADXL375_ADDR, ADXL375_DATAX0, buf, sizeof(buf));

    if (ret) {
        return ret;
    }

    *x = (int16_t)((buf[1] << 8) | buf[0]);
    *y = (int16_t)((buf[3] << 8) | buf[2]);
    *z = (int16_t)((buf[5] << 8) | buf[4]);

    return 0;
}

static bool changed16(int16_t current, int16_t previous, int threshold)
{
    int diff = current - previous;

    if (diff < 0) {
        diff = -diff;
    }

    return diff > threshold;
}

int main(void)
{
    int ret;

    int16_t icm_ax = 0, icm_ay = 0, icm_az = 0;
    int16_t icm_gx = 0, icm_gy = 0, icm_gz = 0;

    int16_t adxl_x = 0, adxl_y = 0, adxl_z = 0;

    int16_t prev_icm_ax = 0, prev_icm_ay = 0, prev_icm_az = 0;
    int16_t prev_adxl_x = 0, prev_adxl_y = 0, prev_adxl_z = 0;

    bool first_sample = true;

    printk("\n=== Kyle IMU Validation Program ===\n");
    printk("Board: nRF52840-DK\n");
    printk("Sensors: ICM-20948 + ADXL375\n");
    printk("Interface: I2C\n\n");

    if (!device_is_ready(i2c_dev)) {
        printk("ERROR: I2C device is not ready.\n");
        return 0;
    }

    printk("I2C bus ready.\n");

    ret = icm_init();
    if (ret) {
        printk("ERROR: ICM-20948 not detected or failed to initialize. Error: %d\n", ret);
    } else {
        printk("ICM-20948 detected at address 0x%02X\n", icm_addr);
    }

    ret = adxl_init();
    if (ret) {
        printk("ERROR: ADXL375 not detected or failed to initialize. Error: %d\n", ret);
    } else {
        printk("ADXL375 detected at address 0x%02X\n", ADXL375_ADDR);
    }

    printk("\nStarting sensor read loop...\n\n");

    while (1) {
        ret = icm_read_accel_gyro_raw(&icm_ax, &icm_ay, &icm_az,
                                      &icm_gx, &icm_gy, &icm_gz);

        if (ret) {
            printk("ICM read error: %d\n", ret);
        } else {
            printk("ICM20948 accel: ax=%d ay=%d az=%d | gyro: gx=%d gy=%d gz=%d\n",
                   icm_ax, icm_ay, icm_az, icm_gx, icm_gy, icm_gz);
        }

        ret = adxl_read_accel_raw(&adxl_x, &adxl_y, &adxl_z);

        if (ret) {
            printk("ADXL read error: %d\n", ret);
        } else {
            printk("ADXL375 accel raw: x=%d y=%d z=%d | approx g: x=%.2f y=%.2f z=%.2f\n",
                   adxl_x, adxl_y, adxl_z,
                   adxl_x * ADXL375_G_PER_LSB,
                   adxl_y * ADXL375_G_PER_LSB,
                   adxl_z * ADXL375_G_PER_LSB);
        }

        if (!first_sample) {
            bool icm_changed =
                changed16(icm_ax, prev_icm_ax, 50) ||
                changed16(icm_ay, prev_icm_ay, 50) ||
                changed16(icm_az, prev_icm_az, 50);

            bool adxl_changed =
                changed16(adxl_x, prev_adxl_x, 10) ||
                changed16(adxl_y, prev_adxl_y, 10) ||
                changed16(adxl_z, prev_adxl_z, 10);

            printk("Validation: ICM=%s | ADXL=%s\n\n",
                   icm_changed ? "changing correctly" : "stable / no major movement",
                   adxl_changed ? "changing correctly" : "stable / no major movement");
        } else {
            printk("Validation: first sample captured\n\n");
            first_sample = false;
        }

        prev_icm_ax = icm_ax;
        prev_icm_ay = icm_ay;
        prev_icm_az = icm_az;

        prev_adxl_x = adxl_x;
        prev_adxl_y = adxl_y;
        prev_adxl_z = adxl_z;

        k_msleep(500);
    }

    return 0;
}