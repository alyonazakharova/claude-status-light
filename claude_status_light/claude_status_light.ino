// Claude Code status light for Seeed XIAO + 6x10 RGB Matrix (WS2812)
//
// Reads one-character commands over USB serial (any baud):
//   'R' -> waiting for your response : red dot in the middle
//   'B' -> working                   : blue dots sliding left to right through the middle
//   'G' -> finished                  : green blinks 3x, then a steady green dot
//   'O' -> off
//   'T' -> layout test (LED 0 white, first row yellow, first column cyan)
//
// Library: Adafruit NeoPixel (Library Manager -> "Adafruit NeoPixel")

#include <Adafruit_NeoPixel.h>

// ---- Hardware config -------------------------------------------------------
#define LED_PIN     D0     // 6x10 RGB Matrix for XIAO drives the LEDs from D0
#define WIDTH       6      // columns (LEDs per row as wired)
#define HEIGHT      10     // rows
#define SERPENTINE  false  // set true if every other row runs backwards (check with 'T')
#define BRIGHTNESS  8     // 0..255, keep low: 60 LEDs at full white draw ~3.6 A

#define NUM_LEDS    (WIDTH * HEIGHT)

Adafruit_NeoPixel matrix(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

enum Mode { MODE_OFF, MODE_WAITING, MODE_WORKING, MODE_DONE, MODE_TEST };

Mode mode = MODE_OFF;
unsigned long modeStartedAt = 0;

// ---- Helpers ---------------------------------------------------------------
uint16_t xy(int x, int y) {
  if (SERPENTINE && (y & 1)) {
    return y * WIDTH + (WIDTH - 1 - x);
  }
  return y * WIDTH + x;
}

void setPixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  matrix.setPixelColor(xy(x, y), color);
}

// Even-sized matrix has no single center pixel, so the "dot" is a 2x2 block.
void drawCenterDot(uint32_t color) {
  int cx = WIDTH / 2 - 1;
  int cy = HEIGHT / 2 - 1;
  setPixel(cx,     cy,     color);
  setPixel(cx + 1, cy,     color);
  setPixel(cx,     cy + 1, color);
  setPixel(cx + 1, cy + 1, color);
}

// ---- Working animation: blue dots sliding left to right through the middle --
const int   DOT_SPACING = 3;      // columns between dots; WIDTH divisible by it loops seamlessly
const float DOT_SPEED   = 0.12f;  // columns per frame (~3 columns/s at 25 fps)

float dotOffset = 0;

void resetDots() {
  dotOffset = 0;
}

// A dot is 1 column wide and 2 rows tall so it sits exactly on the middle line.
void drawDotColumn(int x, uint8_t level) {
  uint32_t color = matrix.Color(0, level / 4, level);
  int y = HEIGHT / 2 - 1;
  setPixel(x, y,     color);
  setPixel(x, y + 1, color);
}

void stepDots() {
  matrix.clear();
  dotOffset += DOT_SPEED;
  if (dotOffset >= DOT_SPACING) dotOffset -= DOT_SPACING;

  // Split each dot's brightness between the two columns it straddles for smooth motion.
  for (float pos = dotOffset; pos < WIDTH; pos += DOT_SPACING) {
    int x = (int)pos;
    float frac = pos - x;
    drawDotColumn(x, 255 * (1 - frac));
    drawDotColumn((x + 1) % WIDTH, 255 * frac);
  }
}

// ---- Mode switching --------------------------------------------------------
void setMode(Mode m) {
  mode = m;
  modeStartedAt = millis();
  matrix.clear();

  switch (mode) {
    case MODE_WAITING:
      drawCenterDot(matrix.Color(255, 0, 0));
      break;
    case MODE_WORKING:
      resetDots();
      break;
    case MODE_TEST:
      for (int x = 0; x < WIDTH; x++)  setPixel(x, 0, matrix.Color(255, 180, 0));
      for (int y = 0; y < HEIGHT; y++) setPixel(0, y, matrix.Color(0, 200, 200));
      matrix.setPixelColor(0, matrix.Color(255, 255, 255));
      break;
    default:
      break;
  }
  matrix.show();
}

void handleSerial() {
  while (Serial.available()) {
    switch (toupper(Serial.read())) {
      case 'R': setMode(MODE_WAITING); break;
      case 'B': setMode(MODE_WORKING); break;
      case 'G': setMode(MODE_DONE);    break;
      case 'O': setMode(MODE_OFF);     break;
      case 'T': setMode(MODE_TEST);    break;
      default: break;  // ignore newlines and noise
    }
  }
}

// ---- Arduino entry points --------------------------------------------------
void setup() {
  Serial.begin(115200);
  matrix.begin();
  matrix.setBrightness(BRIGHTNESS);
  setMode(MODE_OFF);
}

void loop() {
  handleSerial();

  static unsigned long lastFrame = 0;
  unsigned long now = millis();

  switch (mode) {
    case MODE_WORKING:
      if (now - lastFrame >= 40) {  // ~25 fps
        lastFrame = now;
        stepDots();
        matrix.show();
      }
      break;

    case MODE_DONE: {
      // 3 blinks (250 ms on / 250 ms off), then a steady dot.
      unsigned long t = now - modeStartedAt;
      bool on = t >= 1500 || (t / 250) % 2 == 0;
      static bool wasOn = false;
      static unsigned long lastModeStart = 0;
      if (on != wasOn || lastModeStart != modeStartedAt) {
        wasOn = on;
        lastModeStart = modeStartedAt;
        matrix.clear();
        if (on) drawCenterDot(matrix.Color(0, 255, 0));
        matrix.show();
      }
      break;
    }

    default:
      break;  // static frames are drawn once in setMode()
  }
}
