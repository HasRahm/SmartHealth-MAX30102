/*
 * particle_app/max30102.h
 * MAX30102 pulse oximeter driver — Particle Device OS (Wire / Arduino API)
 */
#pragma once
#include "Particle.h"

/* ── Register map ─────────────────────────────────────────────────── */
#define MAX30102_ADDR           0x57

#define REG_INT_STATUS1         0x00
#define REG_INT_STATUS2         0x01
#define REG_INT_ENABLE1         0x02
#define REG_INT_ENABLE2         0x03
#define REG_FIFO_WR_PTR         0x04
#define REG_OVF_COUNTER         0x05
#define REG_FIFO_RD_PTR         0x06
#define REG_FIFO_DATA           0x07
#define REG_FIFO_CONFIG         0x08
#define REG_MODE_CONFIG         0x09
#define REG_SPO2_CONFIG         0x0A
#define REG_LED1_PA             0x0C   /* Red LED pulse amplitude */
#define REG_LED2_PA             0x0D   /* IR  LED pulse amplitude */
#define REG_PILOT_PA            0x10
#define REG_MULTI_LED_CTRL1     0x11
#define REG_MULTI_LED_CTRL2     0x12
#define REG_TEMP_INT            0x1F
#define REG_TEMP_FRAC           0x20
#define REG_TEMP_CONFIG         0x21
#define REG_PROX_INT_THRESH     0x30
#define REG_REV_ID              0xFE
#define REG_PART_ID             0xFF

/* Mode config */
#define MODE_HR                 0x02
#define MODE_SPO2               0x03
#define MODE_MULTI              0x07

/* SpO2 config */
#define SPO2_ADC_RGE_4096       (0x03 << 5)
#define SPO2_SR_100             (0x01 << 2)
#define SPO2_PW_411US_18BIT     (0x03)

/* FIFO config */
#define FIFO_SMP_AVE_4          (0x02 << 5)

/* LED current ~6 mA */
#define LED_CURRENT_6MA         0x1F

/* Samples to collect for one HR/SpO2 computation */
#define SAMPLE_BUF_SIZE         100

/* ── Public structs ───────────────────────────────────────────────── */
struct max30102_data_t {
    uint32_t ir_raw;
    uint32_t red_raw;
    int      heart_rate_bpm;
    int      spo2_percent;
    bool     valid;
};

/* ── Public API ───────────────────────────────────────────────────── */
int  max30102_init(void);
int  max30102_read(max30102_data_t *out);
