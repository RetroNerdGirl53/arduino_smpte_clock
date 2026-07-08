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

## Build
```bash
arduino-cli config add board_manager.additional_urls \
  https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
arduino-cli core update-index
arduino-cli core install STMicroelectronics:stm32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
arduino-cli compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 arduino_smpte_clockv3_0.ino
```
