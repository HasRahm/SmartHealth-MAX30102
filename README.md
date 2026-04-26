# SmartHealth — MAX30102 Pulse Oximeter

Firmware for reading **SpO2 (blood oxygen)** and **heart rate** from a **MAX30102** sensor.
Two independent implementations are provided:

| Implementation | Board | Framework | Flash tool |
|---|---|---|---|
| `particle_app/` | Particle Argon (nRF52840) | Particle Device OS | particle-cli |
| `nrf52840_app/` | Nordic nRF52840 DK (PCA10056) | Zephyr RTOS 4.1.0 | west + J-Link |

Both produce the same live serial output:
```
[   5] HR:  72 bpm  |  SpO2:  98 %  (IR=172891  Red=133200)
```

---

## Project Structure
```
SmartHealth-MAX30102/
├── particle_app/                  ← Particle Device OS firmware
│   ├── app.ino                    ← setup() + loop() entry point
│   ├── max30102.h                 ← register map + public API
│   └── max30102.cpp               ← I2C driver + SpO2/HR algorithm
│
├── nrf52840_app/                  ← Zephyr RTOS firmware (Nordic DK)
│   ├── CMakeLists.txt             ← west build config
│   ├── prj.conf                   ← Kconfig (I2C, UART, LOG)
│   ├── src/
│   │   └── main.c                 ← main loop + sensor config
│   └── boards/
│       └── nrf52840dk_nrf52840.overlay  ← I2C pinout (P0.26/P0.27)
│
├── src/                           ← Shared Zephyr driver (used by nrf52840_app)
│   ├── max30102.h                 ← register map + structs
│   └── max30102.c                 ← Zephyr I2C driver + algorithm
│
├── zephyr/                        ← Legacy Particle Argon Zephyr config (reference)
│   ├── prj.conf
│   ├── particle_argon.overlay
│   └── CMakeLists.txt
│
└── README.md
```

---

## Hardware

### Components
| Component | Details |
|---|---|
| MAX30102 breakout board | Pulse oximeter + heart rate sensor |
| Particle Argon **or** Nordic nRF52840 DK | nRF52840 microcontroller |
| Breadboard + jumper wires | 4 wires needed |
| USB cable | Power + serial monitor |

> **Pull-up resistors:** If your MAX30102 breakout does **not** have built-in pull-ups,
> add a **4.7 kΩ resistor** from SDA → 3.3V and another from SCL → 3.3V.

---

### Wiring — Particle Argon
| MAX30102 Pin | Argon Pin | Notes |
|---|---|---|
| VIN / VCC | 3V3 | Right side, 2nd from top |
| GND | GND | Right side, 3rd from top |
| SDA | SDA / D0 | Right side, 4th from top |
| SCL | SCL / D1 | Right side, 5th from top |
| INT | — | Leave unconnected |

---

### Wiring — nRF52840 DK
| MAX30102 Pin | DK Pin | GPIO | Notes |
|---|---|---|---|
| VIN / VCC | 3V3 | — | Arduino header 3.3V |
| GND | GND | — | Arduino header GND |
| SDA | SDA | P0.26 | Arduino header (A4/SDA) |
| SCL | SCL | P0.27 | Arduino header (A5/SCL) |
| INT | — | — | Leave unconnected |

---

## Implementation 1 — Particle Argon (Particle Device OS)

### Prerequisites
- [Node.js](https://nodejs.org/) v16+
- [particle-cli](https://docs.particle.io/getting-started/developer-tools/cli/)
  ```bash
  npm install -g particle-cli
  particle login
  ```

### Steps
```bash
# 1. Clone the repo
git clone https://github.com/HasRahm/SmartHealth-MAX30102.git
cd SmartHealth-MAX30102

# 2. Compile
particle compile argon particle_app/ --saveTo firmware_particle.bin

# 3. Put Argon into DFU mode (LED turns yellow)
particle usb dfu

# 4. Flash
particle flash --local firmware_particle.bin

# 5. Open serial monitor
particle serial monitor --follow
```

### Expected Output
```
=== MAX30102 Pulse Oximeter (Particle) ===
MAX30102 part_id=0x15 (OK)
Warming up 3000 ms...
Place finger on sensor.

[   0] Waiting...  IR=   930  Red=   920    ← no finger
[   5] HR:  72 bpm  |  SpO2:  98 %  (IR=172891  Red=133200)
```

---

## Implementation 2 — Nordic nRF52840 DK (Zephyr RTOS)

### Prerequisites

**1. Zephyr workspace** (one-time setup):
```bash
pip install west
west init ~/zephyrproject
cd ~/zephyrproject
west update
```

**2. Zephyr SDK 0.16.9** — download and extract to `~/zephyr-sdk-0.16.9`:
- [zephyr-sdk-0.16.9_windows-x86_64_minimal.7z](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.16.9)
- [toolchain_windows-x86_64_arm-zephyr-eabi.7z](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.16.9)

Extract both into the same `zephyr-sdk-0.16.9/` folder.

**3. nRF Command Line Tools** (for J-Link flashing):
- Download from [Nordic Semiconductor](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools)

### Build & Flash (Windows PowerShell)
```powershell
# Set environment
$env:ZEPHYR_BASE = "C:\Users\<you>\zephyrproject\zephyr"
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\<you>\zephyr-sdk-0.16.9"
$env:PATH = "C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin;" + $env:PATH

# Clone and build
git clone https://github.com/HasRahm/SmartHealth-MAX30102.git
cd SmartHealth-MAX30102\nrf52840_app

west build -b nrf52840dk/nrf52840 .

# Flash (first time: use --recover if board has readback protection)
west flash --recover   # first time only
west flash             # subsequent flashes
```

### Open Serial Monitor
The nRF52840 DK exposes a UART-over-USB COM port (separate from J-Link).
Find it in Device Manager → Ports (COM & LPT).

```python
# Python (works on any OS)
python -c "
import serial
s = serial.Serial('COM14', 115200, timeout=1)
while True:
    line = s.readline()
    if line:
        print(line.decode('utf-8', errors='replace').strip())
"
```
Or use **PuTTY** → Serial → your COM port → 115200 baud.

### Expected Output
```
=== MAX30102 Pulse Oximeter (nRF52840 DK / Zephyr) ===
Sensor ready. Warming up 3000 ms...
Place finger on sensor.

[   0] Waiting...  IR=   930  Red=   920    ← no finger
[   5] HR:  60 bpm  |  SpO2:  95 %  (IR=172891  Red=133200)
```

---

## Sensor Configuration

| Parameter | Value | Notes |
|---|---|---|
| I2C address | 0x57 | Fixed, not configurable |
| Mode | SpO2 | Red + IR LEDs active |
| Sample rate | 100 sps | Raw rate from ADC |
| FIFO averaging | 4 samples | Effective rate = 25 sps |
| Pulse width | 411 µs | 18-bit ADC resolution |
| ADC range | 16384 nA | Max range for high LED current |
| LED current | ~38 mA | 0xC0 — required for clone sensors |

---

## Algorithm Overview

```
MAX30102 FIFO
     │
     ▼
Collect 100 samples (~4 seconds at 25 effective sps)
     │
     ├── DC = mean(IR)         AC = max(IR) - min(IR)
     │
     ├── Finger check: IR_DC > 50,000 counts
     │
     ├── Heart Rate
     │     Zero-crossings of (IR - DC)
     │     HR = (crossings / 2) × (60 / window_seconds)
     │
     └── SpO2
           R = (AC_red / DC_red) / (AC_ir / DC_ir)
           SpO2 ≈ 110 − 25R   [linear Beer-Lambert fit]
           Clamped: 70–100%
```

**Output validation:**
- Heart rate accepted: 20–250 bpm
- SpO2 accepted: 70–100%
- Invalid readings shown as `Waiting...`

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Sensor init failed` | I2C wiring wrong | Check VCC/SDA/SCL connections |
| `ERROR reading sensor (-1)` | FIFO not filling | Check MODE register; ensure MODE_CONFIG written last in init |
| `Waiting... IR=930` | No finger on sensor | Place finger flat and still on sensor |
| `IR=262143` (max) | ADC saturating | Use max ADC range (`SPO2_ADC_RGE_32768`) |
| `west flash` fails | Readback protection | Run `west flash --recover` once |
| Clone sensor (part_id=0x00) | Non-genuine MAX30102 | Driver warns but continues — increase LED current to 0xC0 |

---

## Team
- **Kyle Theodore** — IMU Integration (ICM-20948, ADXL375)
- **Luisa De Mello** — Temperature Sensor & ADC
- **Hasin Rahmen** — Pulse Oximeter (MAX30102)

**Course:** CIS4930 — Smart & Connected Health, University of South Florida
**Supervisor:** Samir Ahmed | **Professor:** John Templeton
