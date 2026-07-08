# Port: Arduino Leonardo / Micro (and Pro Micro) — ATmega32U4

**Status: scaffold — not yet implemented.** The 32U4 is AVR and 16 MHz like the
328P, so Timer1 and the timing math carry over, but its **USB stack and its serial
peripheral differ**, which is what makes this a real (if mechanical) port.

## Why `main` doesn't just work
- **Hardware serial is USART1**, not USART0. The 32U4's USB provides `Serial`
  (CDC), while the TX/RX header pins are `Serial1` = **USART1**. `main` writes
  `UCSR0A/B/C`, `UDR0`, `RXEN0`, `TXEN0`, `RXCIE0`, `UDRE0`, `RXC0`, and
  `ISR(USART_RX_vect)` — none of which exist on the 32U4. They must become the
  `...1` variants: `UCSR1A/B/C`, `UDR1`, `RXEN1`, `TXEN1`, `RXCIE1`, `UDRE1`,
  `RXC1`, `ISR(USART1_RX_vect)`.
- **Different port/pin mapping.** The 328P button/output assignments (PORTC/PORTD)
  map to different physical pins on the 32U4.

## What carries over unchanged
- Timer1 CTC + `OCR1A` LTC bit-clock math (16 MHz), biphase-mark ISR, LTC frame
  builder, MTC logic, drop-frame, menu, Adafruit SSD1306 over I²C.
- Recommend also taking the `F_CPU`-derived timing from `port/promini-8mhz`.

## Suggested pin map (Leonardo/Micro)

| Function          | 32U4 pin | Arduino pin |
|-------------------|----------|-------------|
| MIDI in (USART1)  | PD2      | D0 / RX1    |
| MIDI out (USART1) | PD3      | D1 / TX1    |
| LTC out           | PB4      | D8          |
| Start / Stop      | PB5, PB6 | D9, D10     |
| Menu N/P/Sel/Back | PF7/6/5/4| A0/A1/A2/A3 |
| OLED SDA / SCL    | PD1/PD0  | 2 / 3       |

(Adjust to taste; 32U4 I²C is on D2=SDA, D3=SCL.)

## Implementation checklist
- [ ] Rename all USART0 registers/ISR to USART1 (`UCSR1x`, `UDR1`, `USART1_RX_vect`).
- [ ] Remap button/LTC pins to available 32U4 ports; update `DDR/PORT/PIN` reads.
- [ ] Keep `initUSART()` baud math (uses `F_CPU`).
- [ ] Leave `Serial` (USB) alone — it's independent of USART1.
- [ ] `arduino-cli compile --fqbn arduino:avr:leonardo arduino_smpte_clockv3_0.ino`
- [ ] Consider exposing **USB-MIDI** as a bonus (32U4 can enumerate as a MIDI device).

## Build
```bash
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn arduino:avr:leonardo arduino_smpte_clockv3_0.ino
```
