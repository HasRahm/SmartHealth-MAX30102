/*
 * particle_app/app.ino
 * MAX30102 pulse oximeter — Particle Device OS entry point.
 *
 * Serial output appears on USB serial (Serial) at 115200 baud.
 * Open with: particle serial monitor --follow
 *            or any serial terminal on the COM port assigned to the Argon.
 */

#include "Particle.h"
#include "max30102.h"

/* Run in semi-automatic mode so we don't block on cloud connection */
SYSTEM_MODE(SEMI_AUTOMATIC);

#define READ_INTERVAL_MS  500
#define WARMUP_MS        3000

static max30102_data_t reading;
static uint32_t tick = 0;
static bool sensor_ok = false;

void setup()
{
    Serial.begin(115200);
    delay(6000);   /* wait for serial monitor to attach */

    Serial.println("\n=== MAX30102 Pulse Oximeter (Particle) ===");

    if (max30102_init() != 0) {
        Serial.println("ERROR: MAX30102 init failed — check wiring & pull-ups.");
        sensor_ok = false;
    } else {
        sensor_ok = true;
        Serial.printlnf("Warming up %d ms...", WARMUP_MS);
        delay(WARMUP_MS);
        Serial.println("Place finger on sensor.\n");
    }
}

void loop()
{
    if (!sensor_ok) {
        Serial.printlnf("[%4u] Sensor init failed — check VCC/SDA/SCL wiring", tick);
        tick++;
        delay(READ_INTERVAL_MS);
        return;
    }

    int ret = max30102_read(&reading);

    if (ret < 0) {
        Serial.printlnf("[%4u] ERROR reading sensor (%d)", tick, ret);
    } else if (!reading.valid) {
        Serial.printlnf("[%4u] Waiting...  IR=%6u  Red=%6u",
                        tick, reading.ir_raw, reading.red_raw);
    } else {
        Serial.printlnf("[%4u] HR: %3d bpm  |  SpO2: %3d %%  (IR=%6u  Red=%6u)",
                        tick,
                        reading.heart_rate_bpm,
                        reading.spo2_percent,
                        reading.ir_raw,
                        reading.red_raw);
    }

    tick++;
    delay(READ_INTERVAL_MS);
}
