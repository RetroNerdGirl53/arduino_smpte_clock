# Port: Adafruit Trinket M0 (SAMD21E18) — Cortex-M0+ @48 MHz

**Status: scaffold — not yet implemented. Requires a real rewrite** (ARM, no AVR
registers). **Big caveat: the Trinket M0 only exposes ~5 usable GPIO**, which is not
enough for LTC out + MIDI in/out + I²C OLED + 6 buttons. See "pin budget" below.

## Pin budget reality check
Trinket M0 usable pins: D0, D1, D2, D3, D4 (D2/D3/D4 double as I²C/analog). That's
too few for the full feature set. Options:
- **Recommended:** target a roomier SAMD21 board instead and keep the Trinket as a
  minimal LTC-only generator. Good alternatives on the same core: **Adafruit QT Py
  M0**, **Adafruit Feather M0**, **Seeed XIAO SAMD21**, or **Arduino Zero/MKR**.
- **Minimal Trinket build:** LTC out + MIDI out only (drop MIDI-in, OLED, and the
  menu; hardcode the frame rate), or add an **I²C IO-expander** for buttons and share
  the I²C bus with the OLED.

## Rewrite required (SAMD21)
- **LTC clock** → a SAMD timer. Easiest is **Adafruit_ZeroTimer** (wraps TC3/TC4/TC5)
  or a raw **TC** in match-frequency mode at the ~4800 Hz half-bit rate; toggle a pin
  in the timer ISR. `TCC` timers can also drive a pin in hardware.
- **LTC output** → `digitalWrite()` or direct `PORT->Group[g].OUTTGL.reg` for speed.
- **MIDI** → `Serial1` (a SERCOM UART) at 31250. On boards with few SERCOMs you can
  instantiate a custom SERCOM UART on chosen pads.
- **Display** → Adafruit SSD1306 over I²C (`Wire`), if pins allow.
- **Buttons** → `digitalRead`+`INPUT_PULLUP`, or an I²C expander given the pin budget.

## Checklist
- [ ] Decide board: minimal Trinket vs a larger SAMD21 (strongly recommended).
- [ ] Replace Timer1 with Adafruit_ZeroTimer / raw TC at the half-bit rate.
- [ ] Replace `PORTx` LTC toggle with `digitalWrite`/`OUTTGL`.
- [ ] `Serial1` (SERCOM) for MIDI @31250; port MTC tx + RX decode.
- [ ] Fit the UI to the available pins (or drop it on the Trinket).
- [ ] Keep the portable core (LTC frame, biphase, MTC, drop-frame) intact.

## Build
```bash
arduino-cli config add board_manager.additional_urls \
  https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core update-index
arduino-cli core install adafruit:samd
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306" "Adafruit Zero Timer Library"
arduino-cli compile --fqbn adafruit:samd:adafruit_trinket_m0 arduino_smpte_clockv3_0.ino
```
