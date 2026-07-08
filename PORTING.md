# Port: ESP32 (WROOM/DevKitC) — Xtensa LX6 dual-core @240 MHz

**Status: scaffold — not yet implemented. Requires a real rewrite** (no AVR
registers). The ESP32 is actually a *great* target — plenty of GPIO, 3 UARTs, and the
**RMT peripheral** can generate the LTC biphase waveform in hardware.

## Rewrite required
Replace the AVR peripheral layer with ESP32 Arduino-core APIs.

- **LTC generation — two good options:**
  1. **RMT peripheral (recommended):** encode each frame's biphase-mark waveform as
     RMT symbols and stream it — hardware-timed, jitter-free, offloads the CPU.
     Regenerate the RMT buffer once per frame.
  2. **Hardware timer ISR:** `timerBegin`/`timerAttachInterrupt`/`timerAlarmWrite`
     at the half-bit rate (~4800 Hz); toggle a GPIO in an `IRAM_ATTR` ISR. Simpler
     to port from the existing biphase ISR.
- **LTC output** → `gpio_set_level()` / `digitalWrite()` (or the RMT channel pin).
- **MIDI** → use `Serial2` (`Serial2.begin(31250, SERIAL_8N1, RXpin, TXpin)`); any
  GPIO can be routed to a UART. Keep `Serial` (USB) for debug.
- **Display** → Adafruit SSD1306 over I²C; default SDA=21, SCL=22 (or set in
  `Wire.begin(sda, scl)`).
- **Buttons** → plenty of GPIO; use `digitalRead` with `INPUT_PULLUP` + debounce.
  Avoid input-only pins GPIO34–39 for pull-ups.

## Suggested pins (DevKitC)
| Function     | GPIO      |
|--------------|-----------|
| LTC out      | GPIO25    |
| MIDI in/out  | GPIO16/17 | (Serial2 default RX/TX)
| OLED SDA/SCL | GPIO21/22 |
| Start/Stop   | GPIO32/33 |
| Menu N/P/S/B | GPIO26/27/14/12 |

## Checklist
- [ ] Pick RMT vs hardware-timer for LTC; keep the portable LTC-frame builder.
- [ ] Move the biphase generator behind a small HAL so RMT/timer is swappable.
- [ ] `Serial2` for MIDI at 31250; port the MTC tx + RX-decode to `Serial2` reads.
- [ ] `IRAM_ATTR` on any ISR; never block or call flash-heavy code in it.
- [ ] `digitalRead` + `INPUT_PULLUP` buttons + debounce.
- [ ] Consider BLE-MIDI / USB-MIDI (S3) as a bonus transport.

## Build
```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn esp32:esp32:esp32 arduino_smpte_clockv3_0.ino
```
