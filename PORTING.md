# Port: Raspberry Pi Pico / RP2040 — dual Cortex-M0+ @133 MHz

**Status: scaffold — not yet implemented. Requires a real rewrite** (ARM, no AVR
registers). Excellent target: lots of GPIO, multiple UARTs, and the **PIO** state
machines are ideal for generating LTC biphase in hardware.

## Recommended core
The **Earle Philhower** core (`rp2040:rp2040`) is the most flexible (PIO, dual UART,
`Serial1`/`Serial2`); the official `arduino:mbed_rp2040` also works.

## Rewrite required
- **LTC generation — best option is PIO:** a tiny PIO program emits the biphase-mark
  stream from a DMA-fed bit buffer with exact, CPU-free timing. Regenerate the 80-bit
  frame each frame. (Simpler fallback: `repeating_timer`/`add_repeating_timer_us` or a
  hardware alarm ISR at the ~4800 Hz half-bit rate toggling a GPIO.)
- **LTC output** → `gpio_put()` / `digitalWrite()` (or the PIO-mapped pin).
- **MIDI** → `Serial1` / `Serial2` (UART0/UART1) at 31250 on chosen GP pins; `Serial`
  is USB-CDC. USB-MIDI (TinyUSB) is also available as a bonus transport.
- **Display** → Adafruit SSD1306 over I²C (`Wire`/`Wire1`; e.g. SDA=GP4, SCL=GP5).
- **Buttons** → plenty of GPIO; `digitalRead` + `INPUT_PULLUP` + debounce.

## Suggested pins (Pico)
| Function     | GP        |
|--------------|-----------|
| LTC out      | GP15      | (PIO or timer)
| MIDI in/out  | GP1/GP0   | (UART0)
| OLED SDA/SCL | GP4/GP5   | (I2C0)
| Start/Stop   | GP16/GP17 |
| Menu N/P/S/B | GP18/19/20/21 |

## Checklist
- [ ] Choose PIO (recommended) vs repeating-timer for LTC; keep the portable frame
      builder that fills the 80-bit buffer.
- [ ] Write the PIO biphase program (or the timer ISR) + DMA feed.
- [ ] `Serial1`/`Serial2` for MIDI @31250; port MTC tx + RX decode.
- [ ] `digitalRead` buttons + debounce; SSD1306 over `Wire`.
- [ ] Optionally add USB-MIDI via Adafruit TinyUSB.

## Build
```bash
arduino-cli config add board_manager.additional_urls \
  https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core update-index
arduino-cli core install rp2040:rp2040
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn rp2040:rp2040:rpipico arduino_smpte_clockv3_0.ino
```
