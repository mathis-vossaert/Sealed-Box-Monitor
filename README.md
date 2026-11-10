# SealedBoxMonitor

Embedded firmware designed for the environmental supervision of a critical sealed enclosure. This system actively monitors internal temperature, humidity, dew point, water ingress, and unauthorized door access for flux towers, as part of the CARBONIQUE project.

## 🚀 Features

- **Robust Sensor Acquisition:** Integrates moving average filtering for the HIH8131 (I2C) humidity sensor and reliable 1-Wire communication for the DS18B20 temperature sensor.
- **Advanced Alarm Logic:** Implements hysteresis for temperature thresholds and persistence delays (debouncing) to prevent false positives from transient environmental changes.
- **Transient Event Latching:** Captures and memorizes brief physical events (e.g., a quick door opening during maintenance) between telemetry transmission cycles to guarantee zero data loss.
- **Optimized Telemetry:** Packs all system alarms and hardware faults into a highly efficient, single Hexadecimal Status Word (Bitmask).
- **Fail-safe Operation:** Hardware Independent Watchdog (IWDG) enabled to automatically recover the system in case of unexpected software stalls.

---

## 📌 Hardware Pinout (STM32G030)

| Peripheral / Sensor | STM32 Pin | Function / Role |
| :--- | :--- | :--- |
| **DS18B20** (Component Temp) | `PA6` | 1-Wire Bus (Driven by Timer 1) |
| **HIH8131** (Box Temp & Hum) | `PB8` (SCL) / `PB9` (SDA) | I2C1 Bus |
| **Water Sensor** | `PA7` | Digital Input with software debouncing |
| **Door Sensor** | `PB0` | Digital Input with event latching |
| **UART (Datalogger)** | `PA2` (TX) / `PA3` (RX) | USART2 Telemetry Output |

---

## ⚡ Sensor Wiring & Configuration

- **General Power Supply:** 3.3V regulated rail for MCU logic and sensors.
- **1-Wire Bus:** Requires a **4.7 kΩ external pull-up resistor** between the `PA6` data line and the 3.3V rail.
- **Digital Inputs (Water / Door):** Relies on hardware pull-up resistors integrated directly onto the custom PCB.

---

## 📡 Telemetry & Status Word

The device outputs a formatted string over UART (USART2) at regular intervals. 
Example output: 
`CompTemp: 32.5C | BoxTemp: 28.1C | BoxHum: 45.0% | DewPoint: 15.2C | Door: 0 | Status: 0x08`

**Status Word Bitmask Definition (0x00 to 0xFF):**
- `Bit 0`: Humidity Alarm (Sustained > 60%)
- `Bit 1`: Dew Point / Condensation Risk Alarm
- `Bit 2`: Water Leak Detected
- `Bit 3`: Door Opened (Latched event)
- `Bit 4`: Box Temperature Out of Bounds
- `Bit 5`: Component Temperature Out of Bounds
- `Bit 6`: DS18B20 Hardware/Communication Fault
- `Bit 7`: HIH8131 Hardware/Communication Fault

---

## 🛠️ Build and Flash Instructions

### Prerequisites
- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) installed.
- An ST-Link V2/V3 programmer.

### Steps
1. **Open the Project:**
   - Launch **STM32CubeIDE**.
   - Navigate to `File > Open Projects from File System...` and select the cloned repository folder.
2. **Compile the Firmware:**
   - Click the **Build** (hammer) icon in the top toolbar to compile the project and ensure there are no syntax errors.
3. **Flash the MCU:**
   - Connect the ST-Link programmer to the SWD header on the STM32G030 board.
   - Click the **Run / Debug** (green play icon) to flash the firmware onto the microcontroller and start execution.
