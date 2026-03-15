# Mochi — ESP32-S3 Hardware Connections

All pin numbers refer to **GPIO numbers** on the ESP32-S3 (not physical pin positions).
Power all 3.3 V modules from the ESP32-S3's 3V3 rail unless noted otherwise.

---

## 1. I2S Microphone — INMP441

| INMP441 Pin | Connects To         | GPIO |
|-------------|---------------------|------|
| SCK (BCLK)  | ESP32-S3 GPIO       | 41   |
| WS (LRCK)   | ESP32-S3 GPIO       | 42   |
| SD (DOUT)   | ESP32-S3 GPIO       | 2    |
| L/R         | GND (left channel)  | —    |
| VDD         | 3.3 V               | —    |
| GND         | GND                 | —    |

> The INMP441 is a **slave** device — ESP32-S3 generates the clock.  
> L/R tied to GND selects the left channel (standard mono config).

---

## 2. I2S Amplifier — MAX98357A

| MAX98357A Pin | Connects To     | GPIO |
|---------------|-----------------|------|
| BCLK          | ESP32-S3 GPIO   | 5    |
| LRC (LRCLK)   | ESP32-S3 GPIO   | 6    |
| DIN           | ESP32-S3 GPIO   | 7    |
| GAIN          | Leave floating (9 dB) / GND (12 dB) | — |
| SD_MODE       | 3.3 V (always on) | —  |
| VIN           | 3.3 V – 5 V     | —    |
| GND           | GND             | —    |
| OUT+          | Speaker +       | —    |
| OUT−          | Speaker −       | —    |

> Connect a **4–8 Ω speaker** directly to OUT+ / OUT−.  
> For louder output, power MAX98357A from 5 V (USB VBUS) instead of 3.3 V.

---

## 3. OLED Display — SSD1306 (128×64, I2C)

| SSD1306 Pin | Connects To   | GPIO |
|-------------|---------------|------|
| SDA         | ESP32-S3 GPIO | 8    |
| SCL         | ESP32-S3 GPIO | 9    |
| VCC         | 3.3 V         | —    |
| GND         | GND           | —    |
| RES / RST   | Not connected (set to -1 in code) | — |

> I2C address: **0x3C** (default for most SSD1306 modules).  
> Add 4.7 kΩ pull-up resistors on SDA and SCL if the module does not include them.

---

## 4. IR Receiver — TSOP38238 (38 kHz)

| TSOP38238 Pin | Connects To   | GPIO |
|---------------|---------------|------|
| OUT (signal)  | ESP32-S3 GPIO | 11   |
| VS (power)    | 3.3 V         | —    |
| GND           | GND           | —    |

> Place a **100 Ω resistor** in series with VS and a **4.7 µF capacitor** from VS to GND  
> to suppress power supply noise (from the TSOP38238 datasheet).

---

## 5. IR Transmitter LED — 940 nm LED + 2N2222 Transistor

```
ESP32-S3 GPIO 10 ──[ 100 Ω ]──► Base (2N2222)
                                  │
                               Collector ──[ 33 Ω ]──► IR LED Anode
                                  │                         │
                                 GND             IR LED Cathode ── GND
                                              (or 3.3 V ► LED ► Collector)
```

| Signal        | ESP32-S3 GPIO |
|---------------|---------------|
| IR TX control | 10            |

> The 2N2222 (NPN) transistor acts as a switch so the ESP32-S3 GPIO  
> doesn't have to source the ~100 mA needed for reliable IR transmission.  
> Adjust the 33 Ω resistor to set LED current: `I = (3.3 V − 1.2 V) / 33 Ω ≈ 64 mA`.

---

## Summary — GPIO Quick Reference

| GPIO | Function          | Device         |
|------|-------------------|----------------|
| 2    | I2S Data IN (mic) | INMP441 SD     |
| 5    | I2S BCLK (amp)    | MAX98357A BCLK |
| 6    | I2S LRCK (amp)    | MAX98357A LRC  |
| 7    | I2S DIN (amp)     | MAX98357A DIN  |
| 8    | I2C SDA (OLED)    | SSD1306 SDA    |
| 9    | I2C SCL (OLED)    | SSD1306 SCL    |
| 10   | IR TX control     | 2N2222 Base    |
| 11   | IR RX signal      | TSOP38238 OUT  |
| 41   | I2S SCK (mic)     | INMP441 SCK    |
| 42   | I2S WS (mic)      | INMP441 WS     |

---

## Power Summary

| Rail  | Connected Devices                              |
|-------|------------------------------------------------|
| 3.3 V | INMP441, SSD1306, TSOP38238, MAX98357A (min)  |
| 5 V   | MAX98357A VIN (recommended for louder output)  |
| GND   | All device GNDs, INMP441 L/R pin               |

---

*Pin definitions source: `config.h` — edit GPIO numbers there to remap any pin.*
