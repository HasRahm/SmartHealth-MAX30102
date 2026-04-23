/*
 * particle_app/max30102.cpp
 * MAX30102 driver implementation — Particle Device OS (Wire API)
 *
 * Algorithm:
 *   - Collect SAMPLE_BUF_SIZE samples from FIFO
 *   - DC = mean, AC = max - min over the window
 *   - HR  = zero-crossing count × (60 × sample_rate / SAMPLE_BUF_SIZE)
 *   - R   = (AC_red / DC_red) / (AC_ir / DC_ir)   ← ratio of ratios
 *   - SpO2 = 110 - 25×R  (linear approximation, integer arithmetic)
 */

#include "max30102.h"

/* ── Low-level I2C helpers ──────────────────────────────────────────── */
static int reg_write(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(MAX30102_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission();   /* 0 = success */
}

static int reg_read(uint8_t reg, uint8_t *val)
{
    Wire.beginTransmission(MAX30102_ADDR);
    Wire.write(reg);
    int err = Wire.endTransmission(false);
    if (err) return err;

    Wire.requestFrom((uint8_t)MAX30102_ADDR, (uint8_t)1);
    if (!Wire.available()) return -1;
    *val = Wire.read();
    return 0;
}

/* Read one 3-byte (18-bit) sample channel from FIFO data register */
static uint32_t read_channel(void)
{
    /* FIFO data is already pointing at REG_FIFO_DATA; caller requested 3 bytes */
    uint32_t v = 0;
    v |= ((uint32_t)Wire.read()) << 16;
    v |= ((uint32_t)Wire.read()) << 8;
    v |= ((uint32_t)Wire.read());
    return v & 0x03FFFF;   /* 18-bit mask */
}

/* ── FIFO burst read: one Red + one IR sample (6 bytes total) ───────── */
static int fifo_read(uint32_t *red_out, uint32_t *ir_out)
{
    Wire.beginTransmission(MAX30102_ADDR);
    Wire.write(REG_FIFO_DATA);
    int err = Wire.endTransmission(false);
    if (err) return err;

    Wire.requestFrom((uint8_t)MAX30102_ADDR, (uint8_t)6);
    if (Wire.available() < 6) return -1;

    *red_out = read_channel();
    *ir_out  = read_channel();
    return 0;
}

/* ── Simple DC/AC extractor over a buffer ───────────────────────────── */
static void compute_dc_ac(const uint32_t *buf, int n,
                           uint32_t *dc_out, uint32_t *ac_out)
{
    uint32_t mn = buf[0], mx = buf[0];
    uint64_t sum = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i] < mn) mn = buf[i];
        if (buf[i] > mx) mx = buf[i];
        sum += buf[i];
    }
    *dc_out = (uint32_t)(sum / n);
    *ac_out = mx - mn;
}

/* ── Zero-crossing HR estimator ─────────────────────────────────────── */
/* Returns beats-per-minute.  sample_rate_hz = 100 for our config. */
static int zero_crossings(const uint32_t *buf, int n, uint32_t dc,
                           int sample_rate_hz)
{
    int crossings = 0;
    int prev_sign = (int)buf[0] - (int)dc > 0 ? 1 : -1;
    for (int i = 1; i < n; i++) {
        int sign = (int)buf[i] - (int)dc > 0 ? 1 : -1;
        if (sign != prev_sign) {
            crossings++;
            prev_sign = sign;
        }
    }
    /* Each full beat = 2 zero crossings */
    int beats_in_window = crossings / 2;
    /* Extrapolate to 60 s */
    return (beats_in_window * 60 * sample_rate_hz) / n;
}

/* ── Public API ─────────────────────────────────────────────────────── */
int max30102_init(void)
{
    Wire.begin();

    /* Verify part ID — warn but continue for clone sensors */
    uint8_t part_id = 0;
    int id_err = reg_read(REG_PART_ID, &part_id);
    if (id_err != 0) {
        Serial.printlnf("MAX30102: I2C error reading part ID (%d) — check wiring", id_err);
        return -1;
    }
    Serial.printlnf("MAX30102 part_id=0x%02X %s", part_id,
                    part_id == 0x15 ? "(OK)" : "(unexpected — continuing anyway)");
    /* Do NOT abort on wrong ID — many clone modules return 0x00 until reset */

    /* Reset */
    reg_write(REG_MODE_CONFIG, 0x40);
    delay(10);

    /* FIFO: 4-sample average, no rollover, almost-full = 17 */
    reg_write(REG_FIFO_CONFIG, FIFO_SMP_AVE_4 | 0x0F);

    /* SpO2 mode */
    reg_write(REG_MODE_CONFIG, MODE_SPO2);

    /* SpO2 config: ADC 4096 nA, 100 sps, 18-bit pulse width */
    reg_write(REG_SPO2_CONFIG, SPO2_ADC_RGE_4096 | SPO2_SR_100 | SPO2_PW_411US_18BIT);

    /* LED currents ~6 mA */
    reg_write(REG_LED1_PA, LED_CURRENT_6MA);
    reg_write(REG_LED2_PA, LED_CURRENT_6MA);

    /* Clear FIFO pointers */
    reg_write(REG_FIFO_WR_PTR, 0x00);
    reg_write(REG_OVF_COUNTER, 0x00);
    reg_write(REG_FIFO_RD_PTR, 0x00);

    Serial.printlnf("MAX30102 init OK (part_id=0x%02X)", part_id);
    return 0;
}

/* Static sample buffers — avoid large stack allocations */
static uint32_t red_buf[SAMPLE_BUF_SIZE];
static uint32_t ir_buf[SAMPLE_BUF_SIZE];

int max30102_read(max30102_data_t *out)
{
    /* Collect SAMPLE_BUF_SIZE samples */
    for (int i = 0; i < SAMPLE_BUF_SIZE; i++) {
        /* Wait for a new sample (FIFO write ptr != read ptr) */
        uint8_t wr, rd;
        int timeout = 500;   /* ms */
        do {
            reg_read(REG_FIFO_WR_PTR, &wr);
            reg_read(REG_FIFO_RD_PTR, &rd);
            if (wr != rd) break;
            delay(1);
        } while (--timeout > 0);

        if (timeout == 0) {
            out->valid = false;
            return -1;
        }

        if (fifo_read(&red_buf[i], &ir_buf[i]) != 0) {
            out->valid = false;
            return -2;
        }
    }

    /* Store last raw values for display */
    out->ir_raw  = ir_buf[SAMPLE_BUF_SIZE - 1];
    out->red_raw = red_buf[SAMPLE_BUF_SIZE - 1];

    /* Finger-present heuristic: IR must exceed ~50k counts */
    if (out->ir_raw < 50000) {
        out->valid = false;
        return 0;
    }

    /* DC / AC */
    uint32_t dc_red, ac_red, dc_ir, ac_ir;
    compute_dc_ac(red_buf, SAMPLE_BUF_SIZE, &dc_red, &ac_red);
    compute_dc_ac(ir_buf,  SAMPLE_BUF_SIZE, &dc_ir,  &ac_ir);

    if (ac_ir == 0 || dc_ir == 0 || dc_red == 0) {
        out->valid = false;
        return 0;
    }

    /* Heart rate via zero-crossing on IR channel */
    out->heart_rate_bpm = zero_crossings(ir_buf, SAMPLE_BUF_SIZE, dc_ir, 100);

    /* SpO2:  R = (AC_red/DC_red) / (AC_ir/DC_ir)
     *         → R×1000 = (AC_red × DC_ir × 1000) / (AC_ir × DC_red)
     *        SpO2 ≈ 110 - R×25  (linear fit to Beer-Lambert)         */
    uint32_t R1000 = ((uint64_t)ac_red * dc_ir * 1000ULL) / ((uint64_t)ac_ir * dc_red);
    out->spo2_percent = 110 - (int)(R1000 * 25 / 1000);

    /* Sanity clamp */
    if (out->spo2_percent < 70)  out->spo2_percent = 70;
    if (out->spo2_percent > 100) out->spo2_percent = 100;
    if (out->heart_rate_bpm < 20 || out->heart_rate_bpm > 250) {
        out->valid = false;
        return 0;
    }

    out->valid = true;
    return 0;
}
