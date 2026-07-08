# Port: Arduino Pro Mini 3.3 V / 8 MHz (and other F_CPU) — ATmega328P

**Status: implemented, compile-verified.** This branch makes the LTC bit-clock and
menu-tick timing **derive from `F_CPU`** instead of hardcoding 16 MHz, so the sketch
runs correctly on 8 MHz (3.3 V Pro Mini), 16 MHz, and 20 MHz ATmega328P builds.

## What changed vs `main`
- `applyFpsTiming()` computes `OCR1A` from `F_CPU` (64-bit math on the 29.97 branch to
  avoid `F_CPU * 1001` overflow).
- `initTimer0()` computes `OCR0A` from `F_CPU` (155 @16 MHz, 77 @8 MHz).
- The MIDI baud already derived from `F_CPU`, so it needed no change.

Nothing else differs — same chip, same registers, same pinout as `main`.

## Build / flash

```bash
# 8 MHz (3.3 V Pro Mini)
arduino-cli compile --fqbn arduino:avr:pro:cpu=8MHzatmega328 arduino_smpte_clockv3_0.ino
arduino-cli upload  --fqbn arduino:avr:pro:cpu=8MHzatmega328 -p /dev/ttyUSB0 arduino_smpte_clockv3_0.ino

# 16 MHz (5 V Pro Mini / Uno / Nano) still works from this branch too
arduino-cli compile --fqbn arduino:avr:pro:cpu=16MHzatmega328 arduino_smpte_clockv3_0.ino
```

## Notes / caveats
- At 8 MHz the Timer1 resolution is coarser but 24/25/30/29.97 remain distinct
  (29.97 → `OCR1A≈1668`, ~29.976 fps).
- Absolute accuracy is still bounded by the board's oscillator; genlock is the fix for
  long-form drift (see the roadmap in `README.md`).
- If this proves out, it should be merged back to `main` (it's a strict improvement,
  no behavior change at 16 MHz).
