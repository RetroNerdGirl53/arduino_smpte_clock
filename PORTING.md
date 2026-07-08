# Port: Teensy 4.0 / 4.1 — i.MX RT1062 Cortex-M7 @600 MHz

**Status: scaffold — not yet implemented. Requires a real rewrite** (no AVR
registers), but the Teensy is arguably the *best* target for a timecode/MIDI box:
huge headroom, many hardware serials, superb timers, and first-class USB-MIDI.

## Toolchain note
Teensy uses **Teensyduino** (PJRC). Easiest build is the Arduino IDE with Teensyduino
installed; `arduino-cli` support is possible via the third-party Teensy core package
but is fiddlier than the others here.

## Rewrite required
- **LTC clock** → `IntervalTimer` (a periodic hardware-timer callback) at the
  ~4800 Hz half-bit rate, toggling a GPIO in the callback. The M7 makes jitter a
  non-issue; you could also drive a pin from a FlexPWM/QuadTimer for hardware timing.
- **LTC output** → `digitalWriteFast()` (single-cycle) in the timer callback.
- **MIDI — two great options:**
  1. **DIN MIDI** on a hardware serial (`Serial1`..`Serial8`) at 31250.
  2. **USB-MIDI** built in: `usbMIDI.sendTimeCodeQuarterFrame(...)` /
     `usbMIDI.read()` — the Teensy can enumerate as a native USB-MIDI device, which
     is likely the nicest transport for MTC.
- **Display** → Adafruit SSD1306 over I²C (`Wire`, SDA=18, SCL=19).
- **Buttons** → any digital pins, `INPUT_PULLUP` + `Bounce2` for clean debounce.

## Suggested pins (Teensy 4.0)
| Function     | Pin       |
|--------------|-----------|
| LTC out      | 2         | (`digitalWriteFast`)
| MIDI in/out  | 0/1       | (`Serial1`)
| OLED SDA/SCL | 18/19     |
| Start/Stop   | 3/4       |
| Menu N/P/S/B | 5/6/7/8   |

## Checklist
- [ ] Replace Timer1 with `IntervalTimer` at the half-bit period; keep the portable
      LTC-frame builder + biphase logic.
- [ ] Replace `PORTx` toggle with `digitalWriteFast`.
- [ ] Choose MIDI: `Serial1` DIN and/or `usbMIDI` (recommended for MTC out).
- [ ] `Bounce2` for buttons; SSD1306 over `Wire`.
- [ ] Consider `usbMIDI.sendTimeCodeQuarterFrame()` instead of hand-building MTC.

## Must preserve: SMPTE accuracy (do not regress)

Whatever timer/peripheral drives the bit clock, the port is only correct if it
keeps the accuracy `main` already has:

- **29.97 pulldown:** run 29.97 at the true **30000/1001** rate (half-bit rate =
  `160 * fps`), *distinct* from a rounded 30 fps — don't collapse 29.97 to 30.
- **Drop-frame counting:** keep `timeUpdate()`'s rule (skip frame numbers 0 and 1
  at the top of each minute except every 10th) and set the LTC drop-frame flag.
- **Verify:** 29.97 within ~0.05% of 30000/1001 and different from the 30 fps
  setting; a normal DF minute advances 1798 frames (1800 on minute 10). Port the
  host tests from the `.agi` envelope to confirm.

## Build
Preferred: Arduino IDE + Teensyduino, board "Teensy 4.0", install *Adafruit GFX* +
*Adafruit SSD1306* (+ *Bounce2*), then compile/upload `arduino_smpte_clockv3_0.ino`.
