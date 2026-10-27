// Claude Code status light for Seeed XIAO + 6x10 RGB Matrix (WS2812)
//
// The matrix is a cat face (eyes, nose, mouth) mounted landscape, 10 wide x 6 tall:
//
//    0123456789
//   0 .##....##.   eyes (2x2)
//   1 .##....##.
//   2 ....NN....   nose
//   3 .M..MM..M.   mouth (ω with round lobes)
//   4 .M..MM..M.
//   5 ..MM..MM..
//
// Reads one-character commands over USB serial (any baud):
//   'R' -> waiting for your response : red eyes blink 3x together, then stay on
//   'B' -> working                   : blue eyes, each blinking at random
//   'G' -> finished                  : green eyes blink 3x together, then stay on
//   'O' -> off
//   'T' -> layout test (top-left LED white, top row yellow, left column cyan)
//
// On ESP32 boards with a secrets.h next to this file (copy secrets.example.h), the light
// also joins Wi-Fi and takes the same commands over HTTP:
//   http://claude-light.local/cmd?c=B
//
// Library: Adafruit NeoPixel (Library Manager -> "Adafruit NeoPixel")

#include <Adafruit_NeoPixel.h>

#if defined(ESP32) && __has_include("secrets.h")
  #define USE_WIFI 1
  #include "secrets.h"
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>
#else
  #define USE_WIFI 0
#endif

// ---- Hardware config -------------------------------------------------------
#define LED_PIN     D0     // 6x10 RGB Matrix for XIAO drives the LEDs from D0
#define WIDTH       6      // LEDs per row as wired
#define HEIGHT      10     // rows as wired
#define SERPENTINE  false  // set true if every other row runs backwards (check with 'T')
#define ROTATION    3     // quarter turns from the wiring to the mounted face; 1 or 3 here (check with 'T')
#define BRIGHTNESS  15      // 0..255, keep low: 60 LEDs at full white draw ~3.6 A
#define HOSTNAME    "claude-light"  // Wi-Fi only: reachable as http://claude-light.local/

#define NUM_LEDS    (WIDTH * HEIGHT)

// The face is drawn on a landscape canvas; ROTATION turns it onto the wiring.
#if ROTATION % 2 == 0
  #define FACE_W WIDTH
  #define FACE_H HEIGHT
#else
  #define FACE_W HEIGHT
  #define FACE_H WIDTH
#endif

#if FACE_W != 10 || FACE_H != 6
  #error "The cat face is drawn for a 10x6 landscape canvas: pick a ROTATION that makes the matrix landscape"
#endif

Adafruit_NeoPixel matrix(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

enum Mode { MODE_OFF, MODE_WAITING, MODE_WORKING, MODE_DONE, MODE_TEST };

Mode mode = MODE_OFF;
unsigned long modeStartedAt = 0;

// Declared before any function: the Arduino IDE inserts function prototypes there.
struct Blink {
  unsigned long since;      // start of the current open+closed cycle
  unsigned long openFor;
  unsigned long closedFor;
};

Blink blinks[2];

// ---- Helpers ---------------------------------------------------------------
uint16_t xy(int x, int y) {
  if (SERPENTINE && (y & 1)) {
    return y * WIDTH + (WIDTH - 1 - x);
  }
  return y * WIDTH + x;
}

// Face coordinates (x right, y down) -> LED index.
void setPixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= FACE_W || y < 0 || y >= FACE_H) return;
  int wx, wy;
  switch (ROTATION & 3) {
    case 0:  wx = x;              wy = y;               break;
    case 1:  wx = WIDTH - 1 - y;  wy = x;               break;
    case 2:  wx = WIDTH - 1 - x;  wy = HEIGHT - 1 - y;  break;
    default: wx = y;              wy = HEIGHT - 1 - x;  break;
  }
  matrix.setPixelColor(xy(wx, wy), color);
}

// ---- Cat face --------------------------------------------------------------
// Eyes are 2x2, drawn by their left column from the top row.
const int EYE_SIZE    = 2;
const int LEFT_EYE_X  = 1;
const int RIGHT_EYE_X = 7;

void drawEye(int ex, uint32_t color) {
  for (int dy = 0; dy < EYE_SIZE; dy++)
    for (int dx = 0; dx < EYE_SIZE; dx++)
      setPixel(ex + dx, dy, color);
}

void drawNoseAndMouth() {
  uint32_t nose  = matrix.Color(255, 60, 90);
  uint32_t mouth = matrix.Color(120, 120, 120);
  setPixel(4, 2, nose);
  setPixel(5, 2, nose);
  // Cat mouth (ω): two round U-shaped lobes meeting under the nose.
  for (int y = 3; y <= 4; y++) {
    setPixel(1, y, mouth);
    setPixel(4, y, mouth);
    setPixel(5, y, mouth);
    setPixel(8, y, mouth);
  }
  setPixel(2, 5, mouth);
  setPixel(3, 5, mouth);
  setPixel(6, 5, mouth);
  setPixel(7, 5, mouth);
}

// ---- Working: each eye blinks on its own at random moments ------------------
void startBlinkCycle(Blink &b, unsigned long now) {
  b.since     = now;
  b.openFor   = random(300, 1800);
  b.closedFor = random(100, 220);
}

bool eyeOpen(Blink &b, unsigned long now) {
  unsigned long t = now - b.since;
  if (t >= b.openFor + b.closedFor) {
    startBlinkCycle(b, now);
    return true;
  }
  return t < b.openFor;
}

// ---- Drawing ---------------------------------------------------------------
void renderFrame(unsigned long now) {
  matrix.clear();

  switch (mode) {
    case MODE_WORKING: {
      uint32_t blue = matrix.Color(0, 64, 255);
      if (eyeOpen(blinks[0], now)) drawEye(LEFT_EYE_X, blue);
      if (eyeOpen(blinks[1], now)) drawEye(RIGHT_EYE_X, blue);
      drawNoseAndMouth();
      break;
    }

    case MODE_WAITING:
    case MODE_DONE: {
      uint32_t color = mode == MODE_WAITING ? matrix.Color(255, 0, 0) : matrix.Color(0, 255, 0);
      // Both eyes blink 3x together (250 ms on / 250 ms off), then stay on.
      unsigned long t = now - modeStartedAt;
      if (t >= 1500 || (t / 250) % 2 == 0) {
        drawEye(LEFT_EYE_X, color);
        drawEye(RIGHT_EYE_X, color);
      }
      drawNoseAndMouth();
      break;
    }

    case MODE_TEST:
      for (int x = 0; x < FACE_W; x++) setPixel(x, 0, matrix.Color(255, 180, 0));
      for (int y = 0; y < FACE_H; y++) setPixel(0, y, matrix.Color(0, 200, 200));
      setPixel(0, 0, matrix.Color(255, 255, 255));
      break;

    default:
      break;
  }
  matrix.show();
}

// ---- Mode switching --------------------------------------------------------
void setMode(Mode m) {
  mode = m;
  modeStartedAt = millis();
  if (mode == MODE_WORKING) {
    startBlinkCycle(blinks[0], modeStartedAt);
    startBlinkCycle(blinks[1], modeStartedAt);
  }
  renderFrame(modeStartedAt);
}

// Returns false for characters that aren't commands (newlines, noise).
bool handleCommand(char c) {
  switch (toupper(c)) {
    case 'R': setMode(MODE_WAITING); return true;
    case 'B': setMode(MODE_WORKING); return true;
    case 'G': setMode(MODE_DONE);    return true;
    case 'O': setMode(MODE_OFF);     return true;
    case 'T': setMode(MODE_TEST);    return true;
    default:  return false;
  }
}

void handleSerial() {
  while (Serial.available()) {
    handleCommand(Serial.read());
  }
}

// ---- Wi-Fi: the same commands over HTTP --------------------------------------
#if USE_WIFI
WebServer server(80);

const char *modeName() {
  switch (mode) {
    case MODE_WAITING: return "waiting";
    case MODE_WORKING: return "working";
    case MODE_DONE:    return "done";
    case MODE_TEST:    return "test";
    default:           return "off";
  }
}

// GET /cmd?c=B
void handleHttpCommand() {
  String c = server.arg("c");
  if (c.length() != 1 || !handleCommand(c[0])) {
    server.send(400, "text/plain", "usage: /cmd?c=R|B|G|O|T\n");
    return;
  }
  server.send(200, "text/plain", "ok\n");
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // answer requests right away instead of after a power-save nap
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  server.on("/cmd", handleHttpCommand);
  server.on("/", [] { server.send(200, "text/plain", String("claude-status-light: ") + modeName() + "\n"); });
  server.begin();
}

// Wi-Fi connects and reconnects in the background; this starts mDNS once and serves requests.
void serviceWifi() {
  static bool wasConnected = false;
  static bool mdnsStarted  = false;
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    if (!mdnsStarted && MDNS.begin(HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
    }
    Serial.printf("Wi-Fi connected: http://%s.local/ (%s)\n", HOSTNAME, WiFi.localIP().toString().c_str());
  }
  wasConnected = connected;
  server.handleClient();
}
#endif

// ---- Arduino entry points --------------------------------------------------
void setup() {
  Serial.begin(115200);
  matrix.begin();
  matrix.setBrightness(BRIGHTNESS);
  setMode(MODE_OFF);
#if USE_WIFI
  setupWifi();
#endif
}

void loop() {
  handleSerial();
#if USE_WIFI
  serviceWifi();
#endif

  static unsigned long lastFrame = 0;
  unsigned long now = millis();
  bool animated = mode == MODE_WORKING || mode == MODE_WAITING || mode == MODE_DONE;
  if (animated && now - lastFrame >= 40) {  // ~25 fps
    lastFrame = now;
    renderFrame(now);
  }
}
