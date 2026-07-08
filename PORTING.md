# Port: ESP8266 (NodeMCU / Wemos D1 mini) — Tensilica L106 @80/160 MHz

**Status: scaffold — not yet implemented. Requires a real rewrite** (no AVR
registers). The core LTC/MTC *logic* is portable; the peripheral layer is not.

## Rewrite required
`main` uses AVR registers (`TCCR1`, `UCSR0`, `PORTx`, `ISR(...)`) that don't exist
on the ESP8266. Replace them with the ESP8266 Arduino core APIs.

- **LTC half-bit clock** → hardware **Timer1** via `timer1_isr_init()`,
  `timer1_attachInterrupt()`, `timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP)`,
  `timer1_write(ticks)`. At `TIM_DIV16` the tick is `1/(80e6/16)=0.2 µs`; for the
  ~4800 Hz half-bit rate write ~1041 ticks. Put the ISR in IRAM with
  `ICACHE_RAM_ATTR`/`IRAM_ATTR`. Note: Timer1 also backs `analogWrite`/`Servo`/`Tone`
  — don't use those.
- **LTC output pin** → `digitalWrite()` in the ISR (or direct `GPOS`/`GPOC`
  registers for speed). Avoid boot-strap pins GPIO0/2/15.
- **MIDI UART** → the caveat: UART0 (`Serial`) is the USB/flash console; **UART1 is
  TX-only** (GPIO2). For MIDI **in** use `SoftwareSerial` on a spare GPIO, or accept
  losing the USB console and use UART0 (`Serial.begin(31250)`) for both in/out.
- **Display** → Adafruit SSD1306 over I²C works; default `Wire` is SDA=GPIO4 (D2),
  SCL=GPIO5 (D1).
- **Buttons** → GPIO is scarce; you may need an I²C IO-expander (PCF8574) or an
  analog button ladder on the single ADC (A0).

## Suggested pins (Wemos D1 mini)
| Function     | GPIO        | Notes                          |
|--------------|-------------|--------------------------------|
| LTC out      | GPIO13 (D7) | ISR toggles this               |
| MIDI in      | GPIO14 (D5) | SoftwareSerial RX @31250       |
| MIDI out     | GPIO2 (D4)  | UART1 TX (or SoftwareSerial)   |
| OLED SDA/SCL | GPIO4/GPIO5 | D2 / D1                        |
| Buttons      | via PCF8574 | I²C expander (pin-constrained) |

## Checklist
- [ ] Replace Timer1 register setup with `timer1_*` API; ISR in `IRAM_ATTR`.
- [ ] Replace `PORTx` LTC toggle with `digitalWrite`/`GPOS`/`GPOC`.
- [ ] Choose MIDI transport (UART0 vs UART1+SoftwareSerial) and wire RX.
- [ ] Replace button `PIN` reads with `digitalRead`/expander reads + debounce.
- [ ] Keep the portable core (LTC frame, biphase, MTC, drop-frame, menu) intact.
- [ ] Feed the WDT (`yield()`/short `loop()`); never block in the ISR.

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
```bash
arduino-cli config add board_manager.additional_urls \
  http://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn esp8266:esp8266:d1_mini arduino_smpte_clockv3_0.ino
```
