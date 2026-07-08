#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
#include <Wire.h>               // Include Wire library for I2C
#include <Adafruit_GFX.h>       // Include Adafruit graphics library
#include <Adafruit_SSD1306.h>   // Include Adafruit SSD1306 OLED library

#ifndef F_CPU
#define F_CPU 16000000UL  // 16 MHz Pro Mini; guarded so we don't fight the core define
#endif

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SMPTE timecode variables
volatile unsigned char hourCount = 0;
volatile unsigned char minuteCount = 0;
volatile unsigned char secondCount = 0;
volatile unsigned char frameCount = 0;  // Frame count (0 .. framesForFps()-1)

// SMPTE Resolutions. FPS_2997 is a sentinel (not a real frame count) so it
// never collides with a genuine rate; framesForFps()/dropFrame() decode it.
#define FPS_24 24
#define FPS_25 25
#define FPS_30 30
#define FPS_2997 97  // Sentinel for 29.97 drop-frame

// Default configuration
volatile uint8_t selectedFPS = FPS_30;
volatile uint16_t selectedPPQN = 24;  // Stored MIDI PPQN setting (see note in loop())
volatile bool displaySMPTE = true;    // Default display type
volatile bool fpsDirty = false;       // Set when FPS changes so timing is re-applied

// MIDI settings
#define MIDI_BAUD 31250  // MIDI baud rate is 31.25 kbps
#define MIDI_INPUT_PIN PD0  // MIDI input pin (RX)
#define MIDI_OUTPUT_PIN PD1 // MIDI output pin (TX) for USART

// SMPTE (LTC) Output Configuration.
// Driven as a plain digital pin toggled by the Timer1 ISR (biphase-mark),
// NOT a timer-compare output pin, so any free GPIO works. PD6 chosen here.
#define SMPTE_OUTPUT_PIN PD6

// Transport Control Buttons
#define START_BUTTON_PIN PD2  // Button to start/resume timecode
#define STOP_BUTTON_PIN PD3   // Button to stop/pause timecode

// Menu Navigation Buttons (PORTC)
#define NEXT_BUTTON_PIN PC0      // Navigate to next / increment
#define PREVIOUS_BUTTON_PIN PC1  // Navigate to previous / decrement
#define SELECT_BUTTON_PIN PC2    // Select / confirm
#define BACK_BUTTON_PIN PC3      // Back / exit menu

// Transport control states
#define TRANSPORT_STOPPED 0
#define TRANSPORT_RUNNING 1
#define TRANSPORT_PAUSED 2

volatile uint8_t transportState = TRANSPORT_STOPPED;

// Menu states
#define MENU_MAIN 0
#define MENU_SMPTE_FPS 1
#define MENU_MIDI_PPQN 2
#define MENU_DISPLAY_TYPE 3

volatile uint8_t menuState = MENU_MAIN;
volatile uint8_t menuSelectedItem = 0;
volatile bool menuActive = false;

// Menu labels
const char *mainMenuLabels[] = {"SMPTE FPS", "MIDI PPQN", "Display Type"};

const uint8_t smpteFpsOptions[] = {FPS_24, FPS_25, FPS_30, FPS_2997};
const char *smpteFpsLabels[] = {"24 FPS", "25 FPS", "30 FPS", "29.97 DF"};

const uint16_t midiPpqnOptions[] = {1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 960};
const char *midiPpqnLabels[] = {
    "1 PPQN", "2 PPQN", "4 PPQN", "8 PPQN", "16 PPQN", "24 PPQN (Def)",
    "32 PPQN", "48 PPQN", "64 PPQN", "96 PPQN", "128 PPQN", "192 PPQN",
    "256 PPQN", "384 PPQN", "512 PPQN", "768 PPQN", "960 PPQN"
};

const char *displayTypeOptions[] = {"Display SMPTE", "Display MIDI"};

volatile bool clockStarted = false;  // SMPTE clock state
volatile bool clockPaused = false;   // True if the clock is paused

// LTC frame buffer (80 bits) and biphase-mark generator state
volatile uint8_t ltcFrame[10];        // 80 bits, transmitted LSB-first per byte
volatile uint16_t ltcBitIndex = 0;    // 0 .. 79
volatile uint8_t ltcHalf = 0;         // 0 = first half of bit, 1 = second half
volatile uint8_t ltcCurrentBit = 0;   // Value of the bit currently being sent
volatile uint8_t ltcLevel = 0;        // Current output line level

// MTC (MIDI Time Code) quarter-frame generator state
volatile uint8_t mtcPiece = 0;        // Which quarter-frame message (0..7) is next
volatile bool mtcReady = false;       // Set by the LTC ISR every quarter frame

// Incoming MTC (sync-in) reconstruction buffer
volatile unsigned char mtcQuarterFrame[8];

// Function Prototypes
void initUSART(void);
void sendMIDIByte(unsigned char b);
void sendMTCQuarterFrame(uint8_t piece);
void initTimer0(void);
void applyFpsTiming(void);
uint8_t framesForFps(uint8_t fps);
bool dropFrame(uint8_t fps);
uint8_t menuItemCount(void);
void navigateMenu(uint8_t pressedNext, uint8_t pressedPrev);
void selectMenuItem(void);
void backMenu(void);
void initButtons(void);
void buildLtcFrame(void);
void startClock(void);
void timeUpdate(void);
void initDisplay(void);
void displayMenu(void);
void displayTimecode(void);

// ---- Rate helpers -----------------------------------------------------------

// Real number of frames per second used for counting (drop-frame counts 30).
uint8_t framesForFps(uint8_t fps)
{
  return (fps == FPS_2997) ? 30 : fps;
}

// True when the selected rate is 29.97 drop-frame.
bool dropFrame(uint8_t fps)
{
  return (fps == FPS_2997);
}

// MTC timecode-type code for the hours-MS quarter frame (bits 1-2).
uint8_t mtcRateCode(void)
{
  switch (selectedFPS) {
    case FPS_24:   return 0;
    case FPS_25:   return 1;
    case FPS_2997: return 2;
    default:       return 3;  // 30 fps
  }
}

// ---- USART / MIDI -----------------------------------------------------------

// USART initialization for MIDI input and output (31.25 kbps)
void initUSART(void)
{
  unsigned int ubrr = (F_CPU / (16UL * MIDI_BAUD)) - 1;
  UBRR0H = (unsigned char)(ubrr >> 8);
  UBRR0L = (unsigned char)ubrr;

  // Enable RX, TX and the RX-complete interrupt (handled by USART_RX_vect).
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// USART Transmit function for sending MIDI bytes
void sendMIDIByte(unsigned char b)
{
  while (!(UCSR0A & (1 << UDRE0)));
  UDR0 = b;
}

// Transmit ONE MTC quarter-frame message (two bytes: 0xF1, then
// (pieceType << 4) | dataNibble). The full timecode is spread across the
// eight pieces (0..7) and completes every two frames.
void sendMTCQuarterFrame(uint8_t piece)
{
  // Snapshot the timecode at the start of a sequence so all 8 pieces agree.
  static uint8_t f, s, m, h;
  if (piece == 0) {
    f = frameCount;
    s = secondCount;
    m = minuteCount;
    h = hourCount;
  }

  uint8_t data = 0;
  switch (piece) {
    case 0: data = f & 0x0F; break;                                  // Frame LS
    case 1: data = (f >> 4) & 0x01; break;                           // Frame MS
    case 2: data = s & 0x0F; break;                                  // Sec LS
    case 3: data = (s >> 4) & 0x03; break;                           // Sec MS
    case 4: data = m & 0x0F; break;                                  // Min LS
    case 5: data = (m >> 4) & 0x03; break;                           // Min MS
    case 6: data = h & 0x0F; break;                                  // Hour LS
    case 7: data = ((h >> 4) & 0x01) | (mtcRateCode() << 1); break;  // Hour MS + rate
  }

  sendMIDIByte(0xF1);
  sendMIDIByte((uint8_t)((piece << 4) | (data & 0x0F)));
}

// USART RX ISR: decode incoming MTC quarter frames for sync-in. Without this
// handler, enabling RXCIE0 would jump to the bad-vector default and reset the
// MCU on any received byte. Non-blocking two-state machine: a 0xF1 status byte
// is followed by one data byte carrying (type << 4) | value.
ISR(USART_RX_vect)
{
  static bool expectData = false;
  unsigned char b = UDR0;

  if (b & 0x80) {                 // Status byte
    expectData = (b == 0xF1);     // Only quarter-frame is followed by our data
    return;
  }

  if (!expectData) {              // Stray/unexpected data byte
    return;
  }
  expectData = false;

  uint8_t type = (b >> 4) & 0x07;
  uint8_t value = b & 0x0F;
  mtcQuarterFrame[type] = value;

  if (type == 7)  // Last piece: reconstruct the full timecode
  {
    frameCount  = (mtcQuarterFrame[0] & 0x0F) | ((mtcQuarterFrame[1] & 0x01) << 4);
    secondCount = (mtcQuarterFrame[2] & 0x0F) | ((mtcQuarterFrame[3] & 0x03) << 4);
    minuteCount = (mtcQuarterFrame[4] & 0x0F) | ((mtcQuarterFrame[5] & 0x03) << 4);
    hourCount   = (mtcQuarterFrame[6] & 0x0F) | ((mtcQuarterFrame[7] & 0x01) << 4);

    if (!clockStarted) {
      clockStarted = true;
      clockPaused = false;
      transportState = TRANSPORT_RUNNING;
    }
  }
}

// ---- Timer0: 10 ms tick for menu / button handling --------------------------

void initTimer0(void)
{
  TCCR0A = (1 << WGM01);               // CTC mode
  TCCR0B = (1 << CS02) | (1 << CS00);  // Prescaler 1024
  // ~10 ms tick derived from F_CPU: OCR0A = round(F_CPU / 1024 / 100Hz) - 1.
  // (155 @16 MHz, 77 @8 MHz.)
  OCR0A = (uint8_t)((F_CPU / 1024UL / 100UL) - 1);
  TIMSK0 = (1 << OCIE0A);
}

// ---- LTC bit-clock timing (derived from the selected FPS) -------------------

// Timer1 fires at the half-bit rate (2 edges per bit * 80 bits * fps).
// Prescaler 1 keeps the 16 MHz timer clock, giving fine enough resolution to
// actually distinguish 29.97 from 30 (at prescaler 8 both round to the same
// OCR1A, so the pulldown would be lost). OCR1A = timerHz/halfBitHz - 1.
//
// 29.97 is the NTSC 30000/1001 "pulldown", NOT a round 30 fps: the true rate is
// 0.1% slower. Applying it here (together with drop-frame counting) is what keeps
// the timecode tracking wall-clock time; using 30 fps timing would drift ~3.6 s/hr.
// Rounded-to-nearest OCR1A @16 MHz: 24->4166, 25->3999, 30->3332, 29.97->3336.
void applyFpsTiming(void)
{
  if (selectedFPS == FPS_2997) {
    // OCR1A = round(F_CPU * 1001 / (160 * 30000)) - 1.
    // 64-bit math avoids overflow of F_CPU*1001 (e.g. 16e6*1001 > uint32 max).
    OCR1A = (uint16_t)(((uint64_t)F_CPU * 1001UL + 2400000UL) / 4800000UL - 1);
  } else {
    uint32_t div = 160UL * selectedFPS;
    OCR1A = (uint16_t)(((F_CPU + div / 2) / div) - 1);  // round to nearest
  }
}

// ---- Menu logic -------------------------------------------------------------

uint8_t menuItemCount(void)
{
  switch (menuState) {
    case MENU_SMPTE_FPS:    return 4;
    case MENU_MIDI_PPQN:    return 17;
    case MENU_DISPLAY_TYPE: return 2;
    default:                return 3;  // MENU_MAIN
  }
}

void navigateMenu(uint8_t pressedNext, uint8_t pressedPrev)
{
  uint8_t count = menuItemCount();
  if (pressedNext) {
    menuSelectedItem = (menuSelectedItem + 1) % count;
  }
  if (pressedPrev) {
    menuSelectedItem = (menuSelectedItem == 0) ? (count - 1) : (menuSelectedItem - 1);
  }
}

void selectMenuItem(void)
{
  if (menuState == MENU_MAIN)
  {
    switch (menuSelectedItem) {
      case 0: menuState = MENU_SMPTE_FPS;    menuSelectedItem = 0; break;
      case 1: menuState = MENU_MIDI_PPQN;    menuSelectedItem = 0; break;
      case 2: menuState = MENU_DISPLAY_TYPE; menuSelectedItem = 0; break;
      default: break;
    }
  }
  else if (menuState == MENU_SMPTE_FPS)
  {
    selectedFPS = smpteFpsOptions[menuSelectedItem];
    fpsDirty = true;  // Re-apply bit-clock timing at the next frame boundary
    menuState = MENU_MAIN;
    menuSelectedItem = 0;
  }
  else if (menuState == MENU_MIDI_PPQN)
  {
    selectedPPQN = midiPpqnOptions[menuSelectedItem];
    menuState = MENU_MAIN;
    menuSelectedItem = 0;
  }
  else if (menuState == MENU_DISPLAY_TYPE)
  {
    displaySMPTE = (menuSelectedItem == 0);
    menuState = MENU_MAIN;
    menuSelectedItem = 0;
  }
}

// BACK: from a sub-menu return to main; from main, close the menu entirely.
void backMenu(void)
{
  if (menuState != MENU_MAIN) {
    menuState = MENU_MAIN;
    menuSelectedItem = 0;
  } else {
    menuActive = false;
  }
}

// Timer0 ISR: edge-detected button handling for the menu system.
ISR(TIMER0_COMPA_vect)
{
  static uint8_t prevC = 0xFF;  // Previous PORTC reading (pull-ups => idle high)
  uint8_t nowC = PINC;

  // A button is "pressed" on a high->low edge.
  uint8_t pressed = (uint8_t)(prevC & ~nowC);
  prevC = nowC;

  if (menuActive)
  {
    navigateMenu((pressed & (1 << NEXT_BUTTON_PIN)) != 0,
                 (pressed & (1 << PREVIOUS_BUTTON_PIN)) != 0);

    if (pressed & (1 << SELECT_BUTTON_PIN)) {
      selectMenuItem();
    }
    if (pressed & (1 << BACK_BUTTON_PIN)) {
      backMenu();
    }
  }
  else
  {
    if (pressed & (1 << SELECT_BUTTON_PIN)) {
      menuActive = true;
      menuState = MENU_MAIN;
      menuSelectedItem = 0;
    }
  }
}

// ---- Buttons ----------------------------------------------------------------

void initButtons(void)
{
  // Menu buttons on PORTC: inputs with pull-ups.
  DDRC &= ~((1 << NEXT_BUTTON_PIN) | (1 << PREVIOUS_BUTTON_PIN) | (1 << SELECT_BUTTON_PIN) | (1 << BACK_BUTTON_PIN));
  PORTC |= (1 << NEXT_BUTTON_PIN) | (1 << PREVIOUS_BUTTON_PIN) | (1 << SELECT_BUTTON_PIN) | (1 << BACK_BUTTON_PIN);

  // Transport buttons on PORTD: inputs with pull-ups.
  DDRD &= ~((1 << START_BUTTON_PIN) | (1 << STOP_BUTTON_PIN));
  PORTD |= (1 << START_BUTTON_PIN) | (1 << STOP_BUTTON_PIN);
}

// ---- OLED -------------------------------------------------------------------

void initDisplay(void)
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3C for 128x32
    for (;;);  // Display init failed: halt
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

// ---- LTC frame construction (SMPTE 12M 80-bit layout) -----------------------

static inline void ltcSetBit(uint8_t n, uint8_t v)
{
  if (v) ltcFrame[n >> 3] |= (uint8_t)(1 << (n & 7));
  else   ltcFrame[n >> 3] &= (uint8_t)~(1 << (n & 7));
}

static inline uint8_t ltcGetBit(uint16_t n)
{
  return (ltcFrame[n >> 3] >> (n & 7)) & 1;
}

// Write a little-endian binary field of width w starting at bit position p.
static void ltcSetField(uint8_t p, uint8_t w, uint8_t value)
{
  for (uint8_t i = 0; i < w; i++) {
    ltcSetBit((uint8_t)(p + i), (value >> i) & 1);
  }
}

// Build the current 80-bit LTC frame: BCD time fields, drop-frame flag,
// parity, and the 0x3FFD sync word.
void buildLtcFrame(void)
{
  memset((void *)ltcFrame, 0, sizeof(ltcFrame));

  uint8_t ff = frameCount;
  uint8_t ss = secondCount;
  uint8_t mm = minuteCount;
  uint8_t hh = hourCount;

  ltcSetField(0, 4, ff % 10);    // Frame units
  ltcSetField(8, 2, ff / 10);    // Frame tens
  ltcSetField(16, 4, ss % 10);   // Second units
  ltcSetField(24, 3, ss / 10);   // Second tens
  ltcSetField(32, 4, mm % 10);   // Minute units
  ltcSetField(40, 3, mm / 10);   // Minute tens
  ltcSetField(48, 4, hh % 10);   // Hour units
  ltcSetField(56, 2, hh / 10);   // Hour tens

  if (dropFrame(selectedFPS)) {
    ltcSetBit(10, 1);            // Drop-frame flag
  }

  // Sync word (bits 64..79), transmission order: 0011111111111101
  static const uint8_t sync[16] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1};
  for (uint8_t i = 0; i < 16; i++) {
    ltcSetBit((uint8_t)(64 + i), sync[i]);
  }

  // Parity (biphase-mark polarity correction): make the total number of 1s
  // even. Bit 27 for 25 fps, bit 59 otherwise.
  uint8_t parityBit = (selectedFPS == FPS_25) ? 27 : 59;
  ltcSetBit(parityBit, 0);
  uint8_t ones = 0;
  for (uint16_t i = 0; i < 80; i++) {
    ones = (uint8_t)(ones + ltcGetBit(i));
  }
  if (ones & 1) {
    ltcSetBit(parityBit, 1);
  }
}

// Reset the generator and start (fresh) from the current timecode.
void startClock(void)
{
  cli();
  ltcBitIndex = 0;
  ltcHalf = 0;
  ltcLevel = 0;
  mtcPiece = 0;
  mtcReady = false;
  buildLtcFrame();
  clockStarted = true;
  clockPaused = false;
  transportState = TRANSPORT_RUNNING;
  sei();
}

// ---- Setup ------------------------------------------------------------------

void setup(void)
{
  // SMPTE output pin as a digital output (toggled by the Timer1 ISR).
  DDRD |= (1 << SMPTE_OUTPUT_PIN);
  PORTD &= ~(1 << SMPTE_OUTPUT_PIN);

  initUSART();
  initButtons();
  initTimer0();
  initDisplay();

  // Timer1: LTC half-bit clock. CTC mode, prescaler 1 (fine resolution so
  // 29.97 pulldown is distinguishable from 30 fps).
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10);
  applyFpsTiming();          // Sets OCR1A from selectedFPS
  TIMSK1 = _BV(OCIE1A);

  sei();
}

// ---- Main loop --------------------------------------------------------------

void loop(void)
{
  if (!menuActive)
  {
    // Edge-detected transport buttons (active-low with pull-ups).
    static uint8_t prevStart = 1, prevStop = 1;
    uint8_t nowStart = (PIND & (1 << START_BUTTON_PIN)) ? 1 : 0;
    uint8_t nowStop  = (PIND & (1 << STOP_BUTTON_PIN)) ? 1 : 0;

    if (prevStart == 1 && nowStart == 0) {  // START pressed
      if (transportState == TRANSPORT_PAUSED) {
        clockPaused = false;
        transportState = TRANSPORT_RUNNING;
      } else if (transportState == TRANSPORT_STOPPED) {
        startClock();
      }
    }

    if (prevStop == 1 && nowStop == 0) {  // STOP pressed
      if (transportState == TRANSPORT_RUNNING) {
        transportState = TRANSPORT_PAUSED;
        clockPaused = true;
      } else {
        transportState = TRANSPORT_STOPPED;
        clockStarted = false;
        clockPaused = false;
      }
    }

    prevStart = nowStart;
    prevStop = nowStop;

    // Emit MTC quarter frames at the cadence flagged by the LTC ISR.
    if (mtcReady && transportState == TRANSPORT_RUNNING && !clockPaused) {
      mtcReady = false;
      sendMTCQuarterFrame(mtcPiece);
      mtcPiece = (uint8_t)((mtcPiece + 1) & 0x07);
    }

    // NOTE: selectedPPQN is stored but no MIDI clock (0xF8) is emitted from it.
    // A real MIDI clock rate needs a tempo (BPM) source, which this device does
    // not have; wiring a fabricated rate would be incorrect. Left as a setting
    // until a tempo source (tap tempo / MIDI clock-in) is added.

    displayTimecode();
  }
  else
  {
    displayMenu();
  }
}

// ---- LTC output ISR (biphase-mark encoding) ---------------------------------

static inline void ltcWriteLevel(uint8_t hi)
{
  if (hi) PORTD |= (1 << SMPTE_OUTPUT_PIN);
  else    PORTD &= ~(1 << SMPTE_OUTPUT_PIN);
}

// Fires at the half-bit rate. Biphase-mark: always transition at a bit
// boundary; add a mid-bit transition only for a '1'.
ISR(TIMER1_COMPA_vect)
{
  if (!clockStarted || clockPaused) {
    return;
  }

  if (ltcHalf == 0) {
    // Start of a bit: always toggle, then latch the bit value.
    ltcLevel ^= 1;
    ltcWriteLevel(ltcLevel);
    ltcCurrentBit = ltcGetBit(ltcBitIndex);
    ltcHalf = 1;
  } else {
    // Mid-bit: toggle only for a '1'.
    if (ltcCurrentBit) {
      ltcLevel ^= 1;
      ltcWriteLevel(ltcLevel);
    }
    ltcHalf = 0;
    ltcBitIndex++;

    // One quarter frame = 20 bits: flag an MTC message (4 per frame).
    if ((ltcBitIndex % 20) == 0) {
      mtcReady = true;
    }

    if (ltcBitIndex >= 80) {
      ltcBitIndex = 0;
      timeUpdate();
      if (fpsDirty) {
        applyFpsTiming();
        fpsDirty = false;
      }
      buildLtcFrame();
    }
  }
}

// ---- Timecode counter -------------------------------------------------------

void timeUpdate(void)
{
  uint8_t fps = framesForFps(selectedFPS);

  if (frameCount < (uint8_t)(fps - 1)) {
    frameCount++;
    return;
  }

  frameCount = 0;
  if (secondCount < 59) {
    secondCount++;
  } else {
    secondCount = 0;
    if (minuteCount < 59) {
      minuteCount++;
    } else {
      minuteCount = 0;
      hourCount = (hourCount < 23) ? (hourCount + 1) : 0;
    }
  }

  // Drop-frame: at the top of each minute skip frame numbers 0 and 1,
  // except on minutes divisible by 10.
  if (dropFrame(selectedFPS) && secondCount == 0 && (minuteCount % 10) != 0) {
    frameCount = 2;
  }
}

// ---- Displays ---------------------------------------------------------------

// Render a scrolling list (128x32 fits a title + 3 items).
static void renderList(const char *title, const char *const *labels, uint8_t count)
{
  const uint8_t visible = 3;
  uint8_t start = (menuSelectedItem < visible) ? 0 : (uint8_t)(menuSelectedItem - visible + 1);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(title);
  for (uint8_t i = start; i < count && i < (uint8_t)(start + visible); i++) {
    display.print(menuSelectedItem == i ? '>' : ' ');
    display.print(' ');
    display.println(labels[i]);
  }
  display.display();
}

void displayMenu(void)
{
  switch (menuState) {
    case MENU_SMPTE_FPS:    renderList("SMPTE FPS", smpteFpsLabels, 4); break;
    case MENU_MIDI_PPQN:    renderList("MIDI PPQN", midiPpqnLabels, 17); break;
    case MENU_DISPLAY_TYPE: renderList("Display", displayTypeOptions, 2); break;
    default:                renderList("Menu", mainMenuLabels, 3); break;
  }
}

void displayTimecode(void)
{
  display.clearDisplay();
  display.setCursor(0, 0);
  char buffer[24];
  if (displaySMPTE) {
    char sep = dropFrame(selectedFPS) ? ';' : ':';
    sprintf(buffer, "SMPTE %02u:%02u:%02u%c%02u",
            hourCount, minuteCount, secondCount, sep, frameCount);
    display.println(buffer);
  } else {
    sprintf(buffer, "MTC   %02u:%02u:%02u:%02u",
            hourCount, minuteCount, secondCount, frameCount);
    display.println(buffer);
  }
  display.display();
}
