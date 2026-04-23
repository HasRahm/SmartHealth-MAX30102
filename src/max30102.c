/*
 * src/max30102.c
 * MAX30102 pulse oximeter driver for Zephyr RTOS.
 *
 * Sections:
 *   A - Low-level I2C helpers
 *   B - Initialization sequence
 *   C - FIFO read engine
 *   D - SpO2 / HR algorithm (integer-only)
 */

#include "max30102.h"
#include <logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(max30102, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/* Internal circular buffer                                             */
/* ------------------------------------------------------------------ */
#define BUFFER_SIZE     100
#define MIN_SAMPLES     25      /* samples before reporting values    */

static uint32_t red_buf[BUFFER_SIZE];
static uint32_t ir_buf[BUFFER_SIZE];
static uint16_t buf_head  = 0;
static uint16_t buf_count = 0;

static const max30102_config_t *g_cfg = NULL;

/* ------------------------------------------------------------------ */
/* Section A: Low-level I2C helpers                                    */
/* ------------------------------------------------------------------ */

static int reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_write(g_cfg->i2c_dev, buf, sizeof(buf), MAX30102_I2C_ADDR);
}

static int reg_read(uint8_t reg, uint8_t *val)
{
    return i2c_write_read(g_cfg->i2c_dev,
                          MAX30102_I2C_ADDR,
                          &reg, 1,
                          val,  1);
}

static int burst_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return i2c_write_read(g_cfg->i2c_dev,
                          MAX30102_I2C_ADDR,
                          &reg, 1,
                          buf,  len);
}

/* ------------------------------------------------------------------ */
/* Section B: Initialization                                            */
/* ------------------------------------------------------------------ */

int max30102_init(const max30102_config_t *cfg)
{
    if (!cfg || !cfg->i2c_dev) {
        return -EINVAL;
    }
    g_cfg = cfg;

    /* 1. Verify PART_ID */
    uint8_t part_id = 0;
    int ret = reg_read(REG_PART_ID, &part_id);
    if (ret < 0) {
        LOG_ERR("I2C read failed — check wiring (err=%d)", ret);
        return ret;
    }
    if (part_id != MAX30102_PART_ID) {
        LOG_ERR("Wrong PART_ID: expected 0x%02X, got 0x%02X",
                MAX30102_PART_ID, part_id);
        return -ENODEV;
    }
    LOG_INF("MAX30102 found (PART_ID=0x%02X)", part_id);

    /* 2. Software reset */
    ret = reg_write(REG_MODE_CONFIG, MODE_RESET);
    if (ret < 0) { return ret; }
    k_msleep(10);

    /* Wait for reset bit to clear */
    uint8_t mode_val = 0;
    for (int i = 0; i < 10; i++) {
        reg_read(REG_MODE_CONFIG, &mode_val);
        if (!(mode_val & MODE_RESET)) { break; }
        k_msleep(5);
    }

    /* 3. FIFO config: sample averaging + rollover enabled */
    ret = reg_write(REG_FIFO_CONFIG, cfg->sample_avg | FIFO_ROLLOVER_EN | 0x0F);
    if (ret < 0) { return ret; }

    /* 4. SpO2 config: ADC range + sample rate + pulse width */
    ret = reg_write(REG_SPO2_CONFIG,
                    cfg->adc_range | cfg->sample_rate | cfg->pulse_width);
    if (ret < 0) { return ret; }

    /* 5. LED amplitudes */
    ret = reg_write(REG_LED1_PA, cfg->led_current_red);
    if (ret < 0) { return ret; }
    ret = reg_write(REG_LED2_PA, cfg->led_current_ir);
    if (ret < 0) { return ret; }

    /* 6. Set mode (starts measurements) */
    ret = reg_write(REG_MODE_CONFIG, cfg->mode);
    if (ret < 0) { return ret; }

    /* 7. Clear FIFO pointers */
    reg_write(REG_FIFO_WR_PTR, 0x00);
    reg_write(REG_OVF_COUNTER, 0x00);
    reg_write(REG_FIFO_RD_PTR, 0x00);

    /* 8. Reset sample buffer */
    buf_head  = 0;
    buf_count = 0;
    memset(red_buf, 0, sizeof(red_buf));
    memset(ir_buf,  0, sizeof(ir_buf));

    LOG_INF("MAX30102 init complete (mode=0x%02X)", cfg->mode);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Section C: FIFO read engine                                          */
/* ------------------------------------------------------------------ */

/* SpO2 mode: 6 bytes/sample (3 Red + 3 IR). HR-only: 3 bytes/sample. */
#define FIFO_BYTES_SPO2     6
#define FIFO_BYTES_HR       3
#define MAX_FIFO_DEPTH      32

static int fifo_read_samples(uint8_t *num_new)
{
    uint8_t wr_ptr, rd_ptr, ovf;
    int ret;

    ret  = reg_read(REG_FIFO_WR_PTR, &wr_ptr);
    ret |= reg_read(REG_OVF_COUNTER, &ovf);
    ret |= reg_read(REG_FIFO_RD_PTR, &rd_ptr);
    if (ret < 0) { return ret; }

    int16_t avail = (int16_t)wr_ptr - (int16_t)rd_ptr;
    if (avail < 0)  { avail += MAX_FIFO_DEPTH; }
    if (avail == 0) { *num_new = 0; return 0; }

    uint8_t bps   = (g_cfg->mode == MODE_SPO2) ? FIFO_BYTES_SPO2 : FIFO_BYTES_HR;
    uint8_t total = (uint8_t)(avail * bps);

    static uint8_t raw[MAX_FIFO_DEPTH * FIFO_BYTES_SPO2];
    ret = burst_read(REG_FIFO_DATA, raw, total);
    if (ret < 0) { return ret; }

    for (int i = 0; i < avail; i++) {
        uint8_t *p = &raw[i * bps];

        uint32_t red = ((uint32_t)(p[0] & 0x03) << 16)
                     | ((uint32_t)p[1] << 8)
                     |  (uint32_t)p[2];
        red_buf[buf_head] = red;

        if (g_cfg->mode == MODE_SPO2) {
            uint32_t ir = ((uint32_t)(p[3] & 0x03) << 16)
                        | ((uint32_t)p[4] << 8)
                        |  (uint32_t)p[5];
            ir_buf[buf_head] = ir;
        }

        buf_head = (buf_head + 1) % BUFFER_SIZE;
        if (buf_count < BUFFER_SIZE) { buf_count++; }
    }

    *num_new = (uint8_t)avail;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Section D: SpO2 / HR algorithm (integer-only)                       */
/* ------------------------------------------------------------------ */

static uint16_t decode_sample_rate(uint8_t sr_bits)
{
    switch (sr_bits & 0x1C) {
        case SPO2_SR_50:  return 50;
        case SPO2_SR_100: return 100;
        case SPO2_SR_200: return 200;
        case SPO2_SR_400: return 400;
        default:          return 100;
    }
}

/* Compute DC (mean) and AC (peak-to-valley) over the sample buffer. */
static void compute_dc_ac(const uint32_t *buf, uint16_t count, uint16_t head,
                           uint32_t *dc_out, uint32_t *ac_out)
{
    uint64_t sum  = 0;
    uint32_t vmax = 0;
    uint32_t vmin = UINT32_MAX;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (uint16_t)((head + BUFFER_SIZE - count + i) % BUFFER_SIZE);
        uint32_t v   = buf[idx];
        sum += v;
        if (v > vmax) { vmax = v; }
        if (v < vmin) { vmin = v; }
    }

    *dc_out = (uint32_t)(sum / count);
    *ac_out = vmax - vmin;
}

/* Count zero-crossings of mean-subtracted signal (each crossing = half-beat). */
static uint16_t count_zero_crossings(const uint32_t *buf, uint16_t count,
                                     uint16_t head, uint32_t dc)
{
    uint16_t crossings = 0;
    int32_t  prev_sign = 0;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx  = (uint16_t)((head + BUFFER_SIZE - count + i) % BUFFER_SIZE);
        int32_t  val  = (int32_t)buf[idx] - (int32_t)dc;
        int32_t  sign = (val >= 0) ? 1 : -1;

        if (prev_sign != 0 && sign != prev_sign) {
            crossings++;
        }
        prev_sign = sign;
    }
    return crossings;
}

static void run_algorithm(max30102_data_t *data)
{
    uint16_t n = buf_count;
    uint16_t h = buf_head;

    /* Latest raw values always available */
    uint16_t latest  = (uint16_t)((h + BUFFER_SIZE - 1) % BUFFER_SIZE);
    data->red_raw    = red_buf[latest];
    data->ir_raw     = ir_buf[latest];

    if (n < MIN_SAMPLES) {
        data->valid          = false;
        data->heart_rate_bpm = -1;
        data->spo2_percent   = -1;
        return;
    }

    /* Heart rate from IR channel */
    uint32_t ir_dc, ir_ac;
    compute_dc_ac(ir_buf, n, h, &ir_dc, &ir_ac);

    /* Low IR DC = no finger on sensor */
    if (ir_ac < 500 || ir_dc < 5000) {
        data->valid          = false;
        data->heart_rate_bpm = -1;
        data->spo2_percent   = -1;
        return;
    }

    uint16_t sr        = decode_sample_rate(g_cfg->sample_rate);
    uint16_t crossings = count_zero_crossings(ir_buf, n, h, ir_dc);

    /* HR [bpm] = (crossings / 2) / (n / sr) * 60 = crossings * sr * 30 / n */
    data->heart_rate_bpm = (int32_t)((uint32_t)crossings * sr * 30u) / n;

    if (data->heart_rate_bpm < 20 || data->heart_rate_bpm > 250) {
        data->heart_rate_bpm = -1;
        data->valid          = false;
        return;
    }

    /* SpO2 from Red/IR R-ratio */
    if (g_cfg->mode == MODE_SPO2) {
        uint32_t red_dc, red_ac;
        compute_dc_ac(red_buf, n, h, &red_dc, &red_ac);

        if (red_ac == 0 || ir_dc == 0 || red_dc == 0 || ir_ac == 0) {
            data->spo2_percent = -1;
            data->valid        = false;
            return;
        }

        /*
         * R = (AC_red / DC_red) / (AC_ir / DC_ir)
         *
         * Scaled to avoid floats:
         *   R1000 = (AC_red * DC_ir * 1000) / (DC_red * AC_ir)
         *
         * Linear empirical fit (valid for SpO2 90-100%):
         *   SpO2 = 110 - 25*R  →  110 - R1000/40
         */
        uint64_t r1000 = ((uint64_t)red_ac * ir_dc * 1000u)
                       / ((uint64_t)ir_ac  * red_dc);

        data->spo2_percent = 110 - (int32_t)(r1000 / 40u);

        if (data->spo2_percent < 70 || data->spo2_percent > 100) {
            data->spo2_percent = -1;
            data->valid        = false;
            return;
        }
    } else {
        data->spo2_percent = -1;
    }

    data->valid = true;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int max30102_read(const max30102_config_t *cfg, max30102_data_t *data)
{
    if (!cfg || !data || !g_cfg) {
        return -EINVAL;
    }

    uint8_t new_samples = 0;
    int ret = fifo_read_samples(&new_samples);
    if (ret < 0) {
        LOG_ERR("FIFO read failed: %d", ret);
        return ret;
    }

    if (new_samples > 0) {
        LOG_DBG("%d new samples (buf=%d)", new_samples, buf_count);
    }

    run_algorithm(data);
    return (int)new_samples;
}

int max30102_shutdown(const max30102_config_t *cfg)
{
    if (!g_cfg) { return -EINVAL; }
    uint8_t mode = 0;
    reg_read(REG_MODE_CONFIG, &mode);
    return reg_write(REG_MODE_CONFIG, mode | MODE_SHDN);
}

int max30102_wakeup(const max30102_config_t *cfg)
{
    if (!g_cfg) { return -EINVAL; }
    uint8_t mode = 0;
    reg_read(REG_MODE_CONFIG, &mode);
    return reg_write(REG_MODE_CONFIG, mode & ~MODE_SHDN);
}
