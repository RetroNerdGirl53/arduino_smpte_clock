# Port: STM32 "Blue Pill" (STM32F103C8) — Cortex-M3 @72 MHz

**Status: scaffold — not yet implemented. Requires a real rewrite** (no AVR
registers). Uses the **STMicroelectronics STM32 Arduino core** (stm32duino).

## Rewrite required
- **LTC clock** → a `HardwareTimer` in the STM32 core:
  `HardwareTimer *t = new HardwareTimer(TIM2);`
  `t->setOverflow(4800, HERTZ_FORMAT);` (the half-bit rate)
  `t->attachInterrupt(cb); t->resume();` — toggle a GPIO in `cb`. A timer channel can
  also drive a pin in hardware (PWM/toggle-on-match) for jitter-free output.
- **LTC output** → `digitalWrite()` (or direct `GPIOx->BSRR` for speed) in the ISR.
- **MIDI** → `HardwareSerial` (`Serial1`=USART1 PA9/PA10, `Serial2`=USART2 PA2/PA3)
  at 31250. USB-CDC (`Serial`) is available for debug on most cores.
- **Display** → Adafruit SSD1306 over I²C (`Wire`; I2C1 = PB6/PB7).
- **Buttons** → `digitalRead` + `INPUT_PULLUP` + debounce.

## Suggested pins (Blue Pill)
| Function     | Pin       |
|--------------|-----------|
| LTC out      | PA0       |
| MIDI in/out  | PA10/PA9  | (USART1 RX/TX)
| OLED SDA/SCL | PB7/PB6   | (I2C1)
| Start/Stop   | PB0/PB1   |
| Menu N/P/S/B | PB10/11/12/13 |

## Checklist
- [ ] Replace Timer1 register setup with a `HardwareTimer` at the half-bit rate.
- [ ] Replace `PORTx` LTC toggle with `digitalWrite`/`GPIOx->BSRR`.
- [ ] `Serial1`/`Serial2` for MIDI @31250; port MTC tx + RX decode.
- [ ] SSD1306 over `Wire` (PB6/PB7); `digitalRead` buttons + debounce.
- [ ] Note: many Blue Pills have a wrong/again-cloned USB; DIN MIDI + ST-Link upload
      is the reliable path.

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
  https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
arduino-cli core update-index
arduino-cli core install STMicroelectronics:stm32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 arduino_smpte_clockv3_0.ino
```
