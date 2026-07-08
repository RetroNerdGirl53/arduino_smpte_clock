# Port: Arduino Mega 2560 — ATmega2560

**Status: scaffold — not yet implemented.** The Mega is AVR @16 MHz and *does* have
`USART0`, `Timer1`, and the same register names, so `main` may **compile** nearly
as-is — but the **physical pin mapping is wrong**, so it won't behave correctly until
the port/pin assignments are remapped to the Mega headers.

## Why `main` won't work as-is
- On the Mega, `PORTC`/`PORTD`/`PIND` bits do **not** map to the same header pins as
  on the 328P. `main`'s `#define`s (`START_BUTTON_PIN PD2`, `SMPTE_OUTPUT_PIN PD6`,
  menu buttons on `PORTC`, …) point at different physical pins on the Mega.
- The Mega has **4 USARTs**; `USART0` is shared with the USB programming port. Use
  `USART1/2/3` for MIDI if you want the USB console free.

## What carries over
- Timer1 CTC + `OCR1A` bit-clock math (16 MHz), biphase-mark ISR, LTC frame builder,
  MTC, drop-frame, menu, Adafruit SSD1306.
- Consider the `F_CPU`-derived timing from `port/promini-8mhz`.

## Suggested pin map (Mega 2560)
Pick a UART and a coherent set of GPIO. Example using USART1 for MIDI:

| Function          | AVR pin | Mega pin |
|-------------------|---------|----------|
| MIDI in (USART1)  | PD2     | D19 / RX1|
| MIDI out (USART1) | PD3     | D18 / TX1|
| LTC out           | PB5     | D11      |
| Start / Stop      | PA0/PA1 | D22 / D23|
| Menu N/P/Sel/Back | PA2..5  | D24..D27 |
| OLED SDA / SCL    | PD1/PD0 | D20 / D21|

## Implementation checklist
- [ ] Switch MIDI to `USART1` (`UCSR1x`, `UDR1`, `USART1_RX_vect`) or keep `USART0`
      if you don't need the USB console.
- [ ] Remap all button + LTC pins to Mega ports (e.g. `PORTA`) and update the
      `DDR/PORT/PIN` reads accordingly.
- [ ] Confirm the OLED is on the Mega's I²C pins (SDA=D20, SCL=D21).
- [ ] `arduino-cli compile --fqbn arduino:avr:mega arduino_smpte_clockv3_0.ino`

## Build
```bash
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn arduino:avr:mega arduino_smpte_clockv3_0.ino
```
