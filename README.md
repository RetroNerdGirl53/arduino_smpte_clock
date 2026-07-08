# arduino_smpte_clock

A standalone **SMPTE LTC timecode generator** with **MIDI Time Code (MTC)** in/out
for the Arduino Pro Mini (ATmega328P @ 16 MHz). It generates spec-correct linear
timecode, mirrors it as MTC quarter-frame messages, can be chased from incoming
MTC, and is driven by a small OLED menu with transport controls.

> Current firmware: **`arduino_smpte_clockv3_0.ino/arduino_smpte_clockv3_0.ino.ino`**
> (earlier `v1_0` / `v2_0` / top-level `.ino` files are kept for history).

## Features

- **SMPTE LTC output** — a full 80-bit SMPTE 12M frame (BCD time fields, drop-frame
  flag, parity, `0x3FFD` sync word) transmitted with proper **biphase-mark encoding**.
- **Selectable frame rates** — 24, 25, 29.97 (drop-frame), 30 fps, with the true
  **30000/1001 pulldown** for 29.97 (not a rounded 30 fps).
- **MIDI Time Code out** — standard two-byte `0xF1` quarter-frame messages at the
  correct four-per-frame cadence (full timecode every two frames).
- **MTC sync-in** — chases incoming MTC quarter frames to set the timecode and start
  the clock.
- **OLED UI** — 128×32 SSD1306 shows the running SMPTE or MTC readout, plus a
  scrolling button menu.
- **Transport** — start / pause / stop.

## Hardware

- **MCU:** ATmega328P @ 16 MHz (Arduino Pro Mini 5 V; Uno / Nano also work — see
  [Compatibility](#compatibility)).
- **Display:** SSD1306 128×32 I²C OLED at address `0x3C`.

### Wiring (Arduino pin labels)

| Function            | AVR pin | Arduino pin |
|---------------------|---------|-------------|
| MIDI in (UART RX)   | PD0     | D0 / RX     |
| MIDI out (UART TX)  | PD1     | D1 / TX     |
| Start button        | PD2     | D2          |
| Stop button         | PD3     | D3          |
| **SMPTE LTC out**   | PD6     | D6          |
| Menu: Next / +      | PC0     | A0          |
| Menu: Previous / −  | PC1     | A1          |
| Menu: Select / OK   | PC2     | A2          |
| Menu: Back / Exit   | PC3     | A3          |
| OLED SDA            | PC4     | A4          |
| OLED SCL            | PC5     | A5          |

All buttons are active-low and use the internal pull-ups (wire each button to GND).
MIDI in/out should go through the usual opto-isolator / current-limiting MIDI
interface on the UART pins. The LTC output on **D6** is a logic-level biphase-mark
square wave; add line-level conditioning (attenuation / DC-block) if feeding a
device expecting a ~1 Vpp balanced LTC signal.

## Build & flash

Requires the **Adafruit SSD1306** and **Adafruit GFX** libraries (GFX pulls in
Adafruit BusIO).

### Arduino IDE

1. Library Manager → install *Adafruit SSD1306* and *Adafruit GFX*.
2. Board: *Arduino Pro or Pro Mini* → ATmega328P (5V, 16 MHz) — or Uno / Nano.
3. Open `arduino_smpte_clockv3_0.ino/arduino_smpte_clockv3_0.ino.ino` and upload.

### arduino-cli

```bash
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn arduino:avr:pro:cpu=16MHzatmega328 \
  arduino_smpte_clockv3_0.ino
arduino-cli upload  --fqbn arduino:avr:pro:cpu=16MHzatmega328 -p /dev/ttyUSB0 \
  arduino_smpte_clockv3_0.ino
```

## Usage

- **Transport:** press **Start** (D2) to run from the current timecode; **Start**
  again after a pause resumes. **Stop** (D3) pauses when running, and stops/resets
  the run state when pressed again.
- **Menu:** press **Select** (A2) to open the menu. **Next/Previous** (A0/A1) move
  the highlight, **Select** confirms, **Back** (A3) steps out of a submenu or closes
  the menu from the top level. Menu items:
  - **SMPTE FPS** — 24 / 25 / 30 / 29.97 DF
  - **MIDI PPQN** — stored setting (see the roadmap note below)
  - **Display Type** — show SMPTE or MTC on the OLED

## How it works

- **Timer1** runs the LTC **half-bit clock** (prescaler 1, so 29.97 is
  distinguishable from 30). Its ISR emits biphase-mark: a transition at every bit
  boundary, plus a mid-bit transition for a `1`. At each frame boundary the timecode
  advances and the 80-bit frame is rebuilt.
- **Timer0** provides a ~10 ms tick for edge-debounced button/menu handling.
- **USART** at 31 250 baud handles MIDI: TX sends MTC quarter frames (flagged every
  20 LTC bits = 4/frame); the RX ISR decodes incoming MTC for sync.
- **29.97 drop-frame** counts a nominal 30 fps and skips frame numbers 0 and 1 at the
  top of each minute except every tenth minute (verified: 17 982 frames / 10 min),
  while the bit clock uses the 30000/1001 rate so labels track wall-clock time.

## Compatibility

| Target | Status |
|--------|--------|
| ATmega328P @ 16 MHz (Pro Mini 5 V, Uno, Nano, Duemilanove) | ✅ Supported |
| ATmega328P @ 8 MHz (3.3 V Pro Mini) | ⚠️ Timer math is hardcoded to 16 MHz — see roadmap |
| ATmega168 | ❌ Sketch (~17.7 KB) exceeds 16 KB flash |
| ATmega32U4 (Leonardo/Micro) | ❌ Uses USART1, not the `UCSR0x`/`UDR0` this code targets |
| ATmega2560 (Mega) | ⚠️ May compile, but pin/timer mapping differs — needs remapping |
| ESP32 / ESP8266 / RP2040 / SAMD / STM32 / Teensy | ❌ No AVR registers — won't compile |

The firmware pokes ATmega328P registers directly (`UCSR0x`, `TCCR1x`, `OCR1A`,
`TIMSK1`, `PORTx`, `ISR(...)`) for precise LTC timing, which is why it isn't portable
as-is.

## Testing

Host-side unit tests (compiling the real sketch against lightweight AVR/OLED fakes)
cover timecode counting, drop-frame, the LTC frame + biphase round-trip, MTC
encode/decode, menu bounds, and the bit-clock timing / 29.97 pulldown. They live in
the `.agi` envelope's scratch area rather than this repo; ask the maintainer if you
want them promoted into a `test/` folder with a CI runner.

## Roadmap / TODO

- [ ] **Board portability.** Currently ATmega328P-only. Add abstractions (or
      board-specific back-ends) for other targets: ATmega2560 (pin/timer remap),
      ATmega32U4 (USART1), and non-AVR MCUs (ESP32 / RP2040 / SAMD) which need real
      peripheral rewrites for the timer-driven LTC output and hardware UART.
- [ ] **Configurable clock speed (F_CPU).** The MIDI baud rate already derives from
      `F_CPU`, but the LTC bit-clock (`applyFpsTiming`) and the Timer0 menu tick
      (`OCR0A`) hardcode `16000000`. Derive these from `F_CPU` so 8 MHz (3.3 V Pro
      Mini) and 20 MHz builds work without editing constants.
- [ ] **MIDI clock / PPQN.** The **MIDI PPQN** menu setting is stored but **not yet
      wired to a MIDI clock (`0xF8`) generator** — a correct clock rate needs a tempo
      (BPM) source the device doesn't have. Add a tempo source (tap-tempo and/or MIDI
      clock-in) and emit `0xF8` at the selected PPQN. Related MIDI follow-ups:
      MIDI Start/Stop/Continue (`0xFA`/`0xFC`/`0xFB`) tied to transport, and full-frame
      MTC (SysEx) on locate.
- [ ] **CV (control-voltage) timing pulse.** *(tentative)* Add an analog/gate sync
      output for modular and vintage gear — e.g. a per-frame or per-quarter-note
      trigger/gate, and/or classic clock formats (DIN-sync style, Korg/Roland-style
      analog clock). Needs a dedicated gate pin (and optionally a PWM/DAC stage for
      shaped CV), plus a menu option to pick the pulse division and polarity.
- [ ] **Genlock / external reference.** Free-running accuracy is bounded by the
      board's crystal (worse on a ceramic resonator). Support locking the bit clock to
      an external reference (word clock / video sync / incoming LTC) for long-form use.
- [ ] **Consolidate sketch layout.** Fold the historical `v1_0`/`v2_0`/top-level
      copies into a single canonical sketch (or an `examples/` + `src/` structure).

## License

Licensed under the **Business Source License 1.1** (BSL 1.1) — see [`LICENSE`](LICENSE).

- **Source-available:** you may read, copy, modify, and redistribute the source.
- **Free for non-commercial use:** personal, educational, hobbyist, and research use
  (including building and using the device) is permitted.
- **Commercial use requires a separate license** from the Licensor until the Change
  Date.
- **Change Date `2030-07-08`** (4 years from first publication, per BSL 1.1): on that
  date the code automatically converts to the **Apache License 2.0**.

Licensor / copyright: **RetroNerdGirl53**. This is a source-available license, not an
OSI "open source" license. Nothing here is legal advice.
