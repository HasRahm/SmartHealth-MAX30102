/*
 * src/max30102.h
 * MAX30102 pulse oximeter / heart-rate sensor driver for Zephyr RTOS.
 *
 * Public API:
 *   max30102_init()     - configure sensor, start SpO2 mode
 *   max30102_read()     - read FIFO, update output struct
 *   max30102_shutdown() - power-save mode
 *   max30102_wakeup()   - resume from power-save
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <kernel.h>
#include <drivers/i2c.h>
#include <stdint.h>
#include <stdbool.h>

/* I2C address (fixed, not configurable on MAX30102) */
#define MAX30102_I2C_ADDR       0x57

/* ------------------------------------------------------------------ */
/* Register map (datasheet Table 1)                                    */
/* ------------------------------------------------------------------ */
#define REG_INT_STATUS_1        0x00
#define REG_INT_STATUS_2        0x01
#define REG_INT_ENABLE_1        0x02
#define REG_INT_ENABLE_2        0x03

#define REG_FIFO_WR_PTR         0x04
#define REG_OVF_COUNTER         0x05
#define REG_FIFO_RD_PTR         0x06
#define REG_FIFO_DATA           0x07

#define REG_FIFO_CONFIG         0x08
#define REG_MODE_CONFIG         0x09
#define REG_SPO2_CONFIG         0x0A
#define REG_LED1_PA             0x0C   /* Red LED pulse amplitude */
#define REG_LED2_PA             0x0D   /* IR  LED pulse amplitude */

#define REG_REV_ID              0xFE
#define REG_PART_ID             0xFF   /* Expected: 0x15 */

/* ------------------------------------------------------------------ */
/* Mode config (REG_MODE_CONFIG)                                       */
/* ------------------------------------------------------------------ */
#define MODE_SHDN               (1 << 7)
#define MODE_RESET              (1 << 6)
#define MODE_HR_ONLY            0x02
#define MODE_SPO2               0x03

/* ------------------------------------------------------------------ */
/* FIFO config (REG_FIFO_CONFIG)                                       */
/* ------------------------------------------------------------------ */
#define FIFO_SMP_AVE_1          0x00
#define FIFO_SMP_AVE_2          0x20
#define FIFO_SMP_AVE_4          0x40
#define FIFO_SMP_AVE_8          0x60
#define FIFO_ROLLOVER_EN        0x10

/* ------------------------------------------------------------------ */
/* SpO2 config (REG_SPO2_CONFIG)                                       */
/* ------------------------------------------------------------------ */
#define SPO2_ADC_RGE_4096       0x00
#define SPO2_ADC_RGE_8192       0x20
#define SPO2_ADC_RGE_16384      0x40
#define SPO2_ADC_RGE_32768      0x60

#define SPO2_SR_50              0x00
#define SPO2_SR_100             0x04
#define SPO2_SR_200             0x08
#define SPO2_SR_400             0x0C

#define SPO2_PW_69US_15BIT      0x00
#define SPO2_PW_118US_16BIT     0x01
#define SPO2_PW_215US_17BIT     0x02
#define SPO2_PW_411US_18BIT     0x03

/* LED current (0x1F ~ 6.2 mA, 0x3F ~ 12.5 mA) */
#define LED_CURRENT_6MA         0x1F
#define LED_CURRENT_12MA        0x3F

#define MAX30102_PART_ID        0x15

/* ------------------------------------------------------------------ */
/* Driver configuration                                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    const struct device *i2c_dev;
    uint8_t  mode;             /* MODE_SPO2 or MODE_HR_ONLY */
    uint8_t  sample_rate;      /* SPO2_SR_xxx               */
    uint8_t  pulse_width;      /* SPO2_PW_xxx               */
    uint8_t  adc_range;        /* SPO2_ADC_RGE_xxx          */
    uint8_t  led_current_red;
    uint8_t  led_current_ir;
    uint8_t  sample_avg;       /* FIFO_SMP_AVE_xxx          */
} max30102_config_t;

/* ------------------------------------------------------------------ */
/* Output data                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    int32_t  heart_rate_bpm;   /* -1 if not ready / no finger */
    int32_t  spo2_percent;     /* -1 if not ready / HR-only   */
    bool     valid;
    uint32_t red_raw;          /* latest raw Red LED sample   */
    uint32_t ir_raw;           /* latest raw IR  LED sample   */
} max30102_data_t;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */
int max30102_init(const max30102_config_t *cfg);
int max30102_read(const max30102_config_t *cfg, max30102_data_t *data);
int max30102_shutdown(const max30102_config_t *cfg);
int max30102_wakeup(const max30102_config_t *cfg);

#endif /* MAX30102_H */
