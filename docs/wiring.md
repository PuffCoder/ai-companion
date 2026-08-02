# Wiring

Board: ESP32-S3-DEV-KIT-N16R8. Authoritative pin map:
[`esp32/components/bsp/bsp_board.h`](../esp32/components/bsp/bsp_board.h).

---

## INMP441 microphone (I2S)

| INMP441 | ESP32-S3 | Notes |
|---------|----------|-------|
| VDD | 3V3 | |
| GND | GND | use a **right-side** GND (see gotcha) |
| L/R | GPIO21 | driven LOW in software = left channel (saves a GND pin) |
| SCK | GPIO1 | bit clock (BCLK) |
| WS  | GPIO2 | word select (LRCLK) |
| SD  | GPIO42 | data |

### ⚠️ Gotchas (learned the hard way)
- **GND matters**: L/R was first tied to a left-side GND (under GPIO14) and the mic read
  garbage (all-zero / railing). Moving to a **right-side** GND (under GPIO19) fixed it.
  Lesson: for I2S signals, keep signal and ground on the **same side / nearest GND**.
- **L/R is now a GPIO**, not a wire to GND — GPIO21 is driven LOW in firmware to select
  the left channel. This frees one physical GND pin.
- INMP441 is a **24-bit** mic: read with a **32-bit I2S slot + right-shift**. Reading as
  16-bit gives a very weak signal.
- `slot_mask` = LEFT (because L/R = low = left channel).

### Verification (mic_test)
```
silence  → avg 12–26,   peak 50–120
speech   → avg 500–1800, peak 2000–5000
```

---

## Other peripherals

| Peripheral | Signals | GPIO | Power |
|------------|---------|------|-------|
| ST7789 LCD (SPI, landscape 320×170) | SCL/SDA/RES/DC/CS/BLK | 4 / 5 / 6 / 7 / 15 / 16 | 3.3V |
| WS2812 LED | DIN | 38 | 5V |
| Servo (head) | PWM | 14 | external 5V |
| UART → K230 | TX / RX | 17 / 18 | — |
| MAX98357A speaker (planned) | BCLK / LRC / DIN | 41 / 40 / 39 | 5V |

The ST7789 module pin order **SCL SDA RES DC CS BLK** maps 1:1 to the adjacent header
pins 4/5/6/7/15/16, so the ribbon plugs straight in.

### UART to K230
```
ESP32 GPIO17 (TX) → K230 pin10 (GPIO4, UART1 RX)
ESP32 GPIO18 (RX) ← K230 pin8  (GPIO3, UART1 TX)
GND ──────────────  GND     (must share ground)
```
⚠️ K230 uses **its own GPIO numbers** (pin8/pin10 = GPIO3/GPIO4 = UART1), **not** the
Raspberry-Pi labels (GPIO14/15) printed on some pinout diagrams.

### Reserved / do-not-use pins (ESP32-S3 N16R8)
```
0, 3, 45, 46   boot strapping
26–37          SPI flash / octal PSRAM
19, 20         USB D-/D+
43, 44         UART0 console log
```
