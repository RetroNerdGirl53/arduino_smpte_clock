# Port: ESP32-S3 — Xtensa LX7 dual-core @240 MHz, native USB

**Status: scaffold — not yet implemented. Requires a real rewrite** (no AVR
registers). Very similar to the ESP32 port — start from `port/esp32` — with a few
S3-specific differences.

## Same as ESP32
- **LTC** via the **RMT peripheral** (recommended, hardware-timed) or a hardware
  timer ISR (`timerBegin`/`timerAttachInterrupt`, `IRAM_ATTR`) at the ~4800 Hz
  half-bit rate toggling a GPIO.
- **MIDI** on a hardware UART (`Serial1`/`Serial2` with `begin(31250, SERIAL_8N1,
  rx, tx)` — S3 UART pins are fully remappable via the GPIO matrix).
- **Display** Adafruit SSD1306 over I²C (`Wire.begin(sda, scl)`).
- **Buttons** `digitalRead` + `INPUT_PULLUP` + debounce.

## S3-specific notes
- **Native USB (USB-OTG).** `Serial` is usually the USB-CDC port; pick the right
  console/UART. This makes **USB-MIDI** a first-class option (TinyUSB) — arguably the
  best MIDI transport on the S3, alongside or instead of DIN MIDI.
- **Pinout differs from the classic ESP32** — verify your board's exposed GPIO;
  strapping pins GPIO0/45/46 and the USB pins (GPIO19/20) should be avoided for I/O.
- Larger PSRAM/flash variants exist but aren't needed here.

## Suggested pins (generic S3 DevKit)
| Function     | GPIO      |
|--------------|-----------|
| LTC out      | GPIO4     |
| MIDI in/out  | GPIO5/6   | (Serial1)
| OLED SDA/SCL | GPIO8/9   |
| Start/Stop   | GPIO10/11 |
| Menu N/P/S/B | GPIO12/13/14/15 |

## Checklist
- [ ] Branch from / mirror `port/esp32`; adjust pins to the S3 board.
- [ ] Decide MIDI transport: DIN UART, USB-MIDI (TinyUSB), or both.
- [ ] Confirm the USB-CDC vs UART console mapping (`USB CDC On Boot`).
- [ ] RMT or hardware-timer LTC; `IRAM_ATTR` ISR.

## Build
```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn esp32:esp32:esp32s3 arduino_smpte_clockv3_0.ino
```
