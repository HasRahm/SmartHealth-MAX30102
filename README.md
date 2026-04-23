# SmartHealth — MAX30102 Pulse Oximeter
Firmware for the **Particle Argon** (nRF52840) that reads **SpO2** and **heart rate** from a MAX30102 sensor over I2C and prints live readings over USB serial.

---

## Hardware Required
| Component | Details |
|---|---|
| Particle Argon | nRF52840 development board |
| MAX30102 breakout board | Pulse oximeter sensor |
| Breadboard + jumper wires | 4 wires needed |
| USB cable | Micro-USB to PC |

### Wiring
| MAX30102 Pin | Argon Pin | Notes |
|---|---|---|
| VIN / VCC | 3V3 | Right side of board, 2nd from top |
| GND | GND | Right side of board, 3rd from top |
| SDA | D0 | Right side of board, 4th from top |
| SCL | D1 | Right side of board, 5th from top |
| INT | — | Leave unconnected |

> **Note:** If your MAX30102 breakout does **not** have built-in pull-up resistors, add a **4.7 kΩ resistor** from SDA → 3V3 and another from SCL → 3V3.

---

## Setup & Flash

### 1. Install dependencies
- [Node.js](https://nodejs.org/) (v16+)
- [particle-cli](https://docs.particle.io/getting-started/developer-tools/cli/)
  ```
  npm install -g particle-cli
  ```
- [dfu-util](https://dfu-util.sourceforge.net/) (Windows binaries in `tools/`)

### 2. Log in to Particle
```bash
particle login
```

### 3. Compile the firmware
```bash
particle compile argon particle_app/ --saveTo firmware.bin
```

### 4. Put the Argon into DFU mode
```bash
particle usb dfu
```
The RGB LED will turn **yellow**.

### 5. Flash the firmware
```bash
particle flash --local firmware.bin
```

### 6. Open serial monitor
```bash
particle serial monitor --follow
```

---

## Expected Output
```
=== MAX30102 Pulse Oximeter (Particle) ===
MAX30102 part_id=0x15 (OK)
Warming up 3000 ms...
Place finger on sensor.

[   0] Waiting...  IR=   142  Red=   138
[   5] HR:  72 bpm  |  SpO2:  98 %  (IR= 87432  Red= 61023)
```

- **Waiting...** — no finger on sensor (IR raw < 50,000)
- **HR / SpO2** — valid reading with finger placed on sensor

---

## Project Structure
```
SmartHealth-MAX30102/
├── particle_app/          ← Particle Device OS firmware (use this)
│   ├── app.ino            ← setup() + loop() entry point
│   ├── max30102.h         ← driver header & register map
│   └── max30102.cpp       ← I2C driver + SpO2/HR algorithm
├── src/                   ← Zephyr RTOS version (reference only)
│   ├── main.c
│   ├── max30102.h
│   └── max30102.c
├── zephyr/                ← Zephyr build config (reference only)
│   ├── prj.conf
│   ├── particle_argon.overlay
│   └── CMakeLists.txt
└── README.md
```

---

## Algorithm Overview
1. Collect **100 samples** from the MAX30102 FIFO (~4 seconds at 100 sps / 4-sample averaging)
2. Extract **DC** (mean) and **AC** (peak-to-peak) for both Red and IR channels
3. **Heart Rate** — count zero-crossings of IR signal relative to DC baseline, scale to BPM
4. **SpO2** — compute ratio of ratios: `R = (AC_red/DC_red) / (AC_ir/DC_ir)`, then `SpO2 ≈ 110 − 25R`
5. Finger-presence check: IR raw must exceed **50,000 counts**

---

## Team
- Kyle Theodore — IMU Integration (ICM-20948, ADXL375)
- Luisa De Mello — Temperature Sensor & ADC
- Hasin Rahmen — Pulse Oximeter (MAX30102)

**Course:** CIS4930 — Smart & Connected Health, University of South Florida  
**Supervisor:** Samir Ahmed | **Professor:** John Templeton
