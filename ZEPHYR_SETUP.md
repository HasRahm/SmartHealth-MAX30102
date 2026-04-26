# Zephyr RTOS Development Setup — nRF52840 DK (Windows)

This guide covers everything needed to build and flash a Zephyr project
on the **Nordic nRF52840 DK (PCA10056)** from scratch on Windows.
It is sensor-agnostic — follow this once, then use it for any sensor project.

---

## What You Will Install

| Tool | Purpose |
|---|---|
| Python 3.11+ | Required by west and Zephyr build system |
| west | Zephyr's meta-tool (build, flash, manage workspace) |
| Zephyr workspace | Zephyr RTOS source + modules |
| Zephyr SDK 0.16.9 | ARM cross-compiler + host tools |
| nRF Command Line Tools | J-Link flash utility (nrfjprog) |
| CMake + Ninja | Build system (may already be installed) |

---

## Step 1 — Install Python

Download and install Python 3.11 or newer from https://www.python.org/downloads/

> **Important:** During installation check **"Add Python to PATH"**

Verify:
```powershell
python --version
# Expected: Python 3.11.x or higher
```

---

## Step 2 — Install west

Open PowerShell and run:
```powershell
pip install west
west --version
# Expected: West version: v1.x.x
```

---

## Step 3 — Set Up Zephyr Workspace

This downloads the Zephyr RTOS source and all required modules (~2 GB).

```powershell
# Create and enter the workspace folder
cd C:\Users\<your-username>
west init zephyrproject
cd zephyrproject
west update
```

> This takes **10–20 minutes** depending on your internet speed.

When done, your folder will look like:
```
zephyrproject/
├── .west/
├── zephyr/          ← Zephyr RTOS source
├── bootloader/
├── modules/
└── tools/
```

---

## Step 4 — Install Zephyr SDK (ARM Toolchain)

The SDK contains the ARM cross-compiler needed to build for nRF52840.

### 4A. Download these two files from GitHub Releases (v0.16.9):
Go to: https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.16.9

Download:
- `zephyr-sdk-0.16.9_windows-x86_64_minimal.7z` (~100 MB)
- `toolchain_windows-x86_64_arm-zephyr-eabi.7z` (~76 MB)

### 4B. Extract both into the same folder

Create folder: `C:\Users\<your-username>\zephyr-sdk-0.16.9`

Extract `zephyr-sdk-0.16.9_windows-x86_64_minimal.7z` → into `C:\Users\<your-username>\`
(it will create `zephyr-sdk-0.16.9\` automatically)

Extract `toolchain_windows-x86_64_arm-zephyr-eabi.7z` → into `C:\Users\<your-username>\zephyr-sdk-0.16.9\`

### 4C. Verify the SDK structure
```
zephyr-sdk-0.16.9/
├── arm-zephyr-eabi/     ← ARM compiler lives here
├── cmake/
│   └── Zephyr-sdkConfig.cmake
├── hosttools/
└── sdk_version
```

---

## Step 5 — Install nRF Command Line Tools

Download the Windows installer from:
https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools

Run the installer. It installs `nrfjprog.exe` to:
```
C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin\
```

Verify (open a **new** PowerShell window after install):
```powershell
nrfjprog --version
# Expected: nrfjprog version: 10.x.x
```

---

## Step 6 — Install CMake and Ninja

Check if already installed:
```powershell
cmake --version
ninja --version
```

If missing, install via winget:
```powershell
winget install Kitware.CMake
winget install Ninja-build.Ninja
```

Or download manually:
- CMake: https://cmake.org/download/
- Ninja: https://github.com/ninja-build/ninja/releases

---

## Step 7 — Set Environment Variables

You need to set these **every time** you open a new PowerShell window before building.
Replace `<your-username>` with your actual Windows username.

```powershell
$env:ZEPHYR_BASE = "C:\Users\<your-username>\zephyrproject\zephyr"
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\<your-username>\zephyr-sdk-0.16.9"
$env:PATH = "C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin;" + $env:PATH
```

> **Tip:** Save these three lines in a file called `env.ps1` and run `.\env.ps1`
> at the start of each session so you don't have to retype them.

---

## Step 8 — Verify Everything Works (Hello World)

### 8A. Create a minimal test project

```powershell
mkdir C:\MyZephyrTest\src
```

Create `C:\MyZephyrTest\CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)
target_sources(app PRIVATE src/main.c)
```

Create `C:\MyZephyrTest\prj.conf`:
```ini
CONFIG_UART_CONSOLE=y
CONFIG_PRINTK=y
```

Create `C:\MyZephyrTest\src\main.c`:
```c
#include <zephyr/kernel.h>

int main(void)
{
    printk("Hello from nRF52840 DK!\n");
    while (1) {
        printk("Running...\n");
        k_sleep(K_MSEC(1000));
    }
    return 0;
}
```

### 8B. Build
```powershell
cd C:\MyZephyrTest
west build -b nrf52840dk/nrf52840 .
```

Expected output ends with:
```
Memory region    Used Size  Region Size  %age Used
         FLASH:     18564 B       1 MB      1.77%
           RAM:      5696 B     256 KB      2.17%
```

### 8C. Flash

Connect the nRF52840 DK via the **USB port labeled "J-Link USB"** (left side of board).

```powershell
west flash
```

If you get a readback protection error, run this **once**:
```powershell
west flash --recover
```
> ⚠️ `--recover` erases all flash — only needed the very first time or if the board was previously locked by other firmware.

### 8D. Open Serial Monitor

The DK creates two COM ports when plugged in. You want the **UART port** (not J-Link).
Check Device Manager → Ports (COM & LPT) to find the correct port number.

```python
python -c "
import serial
s = serial.Serial('COM14', 115200, timeout=1)
while True:
    line = s.readline()
    if line:
        print(line.decode('utf-8', errors='replace').strip())
"
```
Replace `COM14` with your actual port number.

You should see:
```
Hello from nRF52840 DK!
Running...
Running...
```

---

## Adding Your Sensor (I2C)

Once Hello World works, add I2C for your sensor.

### Update prj.conf
```ini
CONFIG_UART_CONSOLE=y
CONFIG_PRINTK=y
CONFIG_I2C=y
CONFIG_GPIO=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_MAIN_STACK_SIZE=4096
```

### Create a Device Tree overlay

Create `boards/nrf52840dk_nrf52840.overlay` inside your project:

```devicetree
/* nRF52840 DK — I2C on Arduino header pins
 * SDA = P0.26  |  SCL = P0.27
 */
&arduino_i2c {
    status = "okay";
    clock-frequency = <I2C_BITRATE_STANDARD>;  /* 100 kHz */

    your_sensor: your_sensor@<i2c_address> {
        compatible = "your,sensor-binding";
        reg = <0x<i2c_address>>;
        status = "okay";
    };
};
```

Replace `<i2c_address>` with your sensor's 7-bit I2C address (e.g. `57` for MAX30102).

### Get the I2C device in code
```c
#include <zephyr/drivers/i2c.h>

#define I2C_NODE DT_NODELABEL(arduino_i2c)

const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
if (!device_is_ready(i2c)) {
    printk("I2C not ready\n");
    return -1;
}

/* Write a register */
uint8_t buf[2] = { reg_addr, value };
i2c_write(i2c, buf, 2, SENSOR_I2C_ADDR);

/* Read a register */
uint8_t result;
i2c_write_read(i2c, SENSOR_I2C_ADDR, &reg_addr, 1, &result, 1);
```

---

## Quick Reference

### Every-session setup (PowerShell)
```powershell
$env:ZEPHYR_BASE = "C:\Users\<your-username>\zephyrproject\zephyr"
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\<your-username>\zephyr-sdk-0.16.9"
$env:PATH = "C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin;" + $env:PATH
```

### Build & Flash
```powershell
cd C:\Your\Project
west build -b nrf52840dk/nrf52840 .   # build
west build -b nrf52840dk/nrf52840 . --pristine  # clean build
west flash                             # flash
west flash --recover                   # flash + unlock (first time)
```

### Common Errors

| Error | Fix |
|---|---|
| `west: command not found` | Run `pip install west` |
| `find_package(Zephyr) failed` | `ZEPHYR_BASE` not set — run the env vars block |
| `Could not find Zephyr-sdk` | `ZEPHYR_SDK_INSTALL_DIR` not set or wrong path |
| `nrfjprog not found` | Add nRF tools to PATH or install nRF Command Line Tools |
| `Access protection is enabled` | Run `west flash --recover` once |
| `No serial output` | Wrong COM port — check Device Manager |

---

## Useful Resources

- [Zephyr Documentation](https://docs.zephyrproject.org/latest/)
- [nRF52840 DK Board Guide (Zephyr)](https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html)
- [Zephyr I2C API](https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html)
- [Nordic DevAcademy — Zephyr](https://academy.nordicsemi.com/)
- [nRF52840 DK User Guide (Nordic)](https://infocenter.nordicsemi.com/topic/ug_nrf52840_dk/UG/dk/intro.html)
