#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TAMC_GT911.h>
#include <WiFi.h>
#include <HTTPClient.h>

// -----------------------------
// USER SETTINGS
// -----------------------------
const char *WIFI_SSID = "OpenHaldex-S3";
const char *WIFI_PASS = "";
const char *HALDEX_IP = "192.168.4.1";

const char *MODE_API_URL = "http://192.168.4.1/api/mode";
const char *STATUS_API_URL = "http://192.168.4.1/api/status";
const char *MODE_NAMES[] = {
  "FWD",   // Button 0: STOCK (FWD)
  "9010",  // Button 1
  "8020",  // Button 2
  "7030",  // Button 3
  "6040",  // Button 4
  "5050"   // Button 5
};
const char *MODE_NAMES_FALLBACK[] = {
  "STOCK", // Button 0 fallback
  "90:10", // Button 1 fallback
  "80:20", // Button 2 fallback
  "70:30", // Button 3 fallback
  "60:40", // Button 4 fallback
  "50:50"  // Button 5 fallback
};

// -----------------------------
// DISPLAY + TOUCH CONFIG
// -----------------------------
#define TFT_BL 2

#define TOUCH_SDA 8
#define TOUCH_SCL 9
#define TOUCH_INT -1
#define TOUCH_RST -1

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  5, 3, 46, 7,            // DE, VSYNC, HSYNC, PCLK
  1, 2, 42, 41, 40,       // R0..R4
  39, 0, 45, 48, 47, 21,  // G0..G5
  14, 38, 18, 17, 10,     // B0..B4
  1, 1, 1,                // hsync_polarity, vsync_polarity, pclk_active_neg
  4, 8, 8,                // hsync: hpw, hbp, hfp
  4, 8, 8,                // vsync: vpw, vbp, vfp
  16000000, false, 0, 0, 0
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);
TAMC_GT911 ts(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, 800, 480);

// -----------------------------
// TOUCH CALIBRATION (tune if needed)
// -----------------------------
int TS_MIN_X = 0;
int TS_MAX_X = 799;
int TS_MIN_Y = 0;
int TS_MAX_Y = 479;

// Changed per your test: touch is mirrored left/right and top/bottom
bool SWAP_XY = false;
bool FLIP_X = true;
bool FLIP_Y = true;

// -----------------------------
// UI THEME
// -----------------------------
const uint16_t COLOR_BG = 0x0000;          // black
const uint16_t COLOR_TEXT = 0xFFFF;        // white
const uint16_t COLOR_TEXT_DIM = 0xBDF7;    // light gray
const uint16_t COLOR_RED_THEME = 0xA800;   // deep car-style red
const uint16_t COLOR_RED_BORDER = 0xF800;  // bright red border
const uint16_t COLOR_GREEN_SEL = 0x07E0;   // selected green
const uint16_t COLOR_STATUS_RED = 0xF800;
const uint16_t COLOR_STATUS_GREEN = 0x07E0;

const int RECONNECT_BTN_X = 620;
const int RECONNECT_BTN_Y = 52;
const int RECONNECT_BTN_W = 160;
const int RECONNECT_BTN_H = 36;

struct Button {
  int x;
  int y;
  int w;
  int h;
  const char *label;
};

Button buttons[6];
int selectedButton = 0; // 0 = Stock initially
uint32_t lastTouchMs = 0;
uint32_t lastButtonSendMs = 0;
String lastApiResult = "No command sent yet";
uint32_t lastWifiAttemptMs = 0;
uint32_t lastStatusDrawMs = 0;
wl_status_t lastDrawnWifiStatus = WL_IDLE_STATUS;
wl_status_t lastHandledWifiStatus = WL_IDLE_STATUS;
wl_status_t lastDrawnReconnectStatus = WL_IDLE_STATUS;

// Touch smoothing and anti-jitter state
float filtX = 0;
float filtY = 0;
bool filtInit = false;
int lastStableX = -1000;
int lastStableY = -1000;

void initButtons() {
  const int cols = 3;
  const int rows = 2;
  const int marginX = 24;
  const int marginYTop = 120;
  const int gapX = 20;
  const int gapY = 18;
  const int btnW = (800 - (2 * marginX) - (gapX * (cols - 1))) / cols;
  const int btnH = 130;

  const char *labels[6] = {
    "STOCK (FWD)", "90 / 10", "80 / 20",
    "70 / 30", "60 / 40", "50 / 50"
  };

  int idx = 0;
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      buttons[idx].x = marginX + c * (btnW + gapX);
      buttons[idx].y = marginYTop + r * (btnH + gapY);
      buttons[idx].w = btnW;
      buttons[idx].h = btnH;
      buttons[idx].label = labels[idx];
      idx++;
    }
  }
}

void drawHeader() {
  gfx->fillRect(0, 0, 800, 100, COLOR_BG);

  gfx->setTextColor(COLOR_RED_BORDER);
  gfx->setTextSize(3);
  gfx->setCursor(22, 16);
  gfx->print("Open Haldex S3 Control");

  gfx->setTextColor(COLOR_TEXT_DIM);
  gfx->setTextSize(2);
  gfx->setCursor(22, 58);
  gfx->print("Audi S3 8L torque split");
}

void drawConnectionStatus(bool force = false) {
  wl_status_t status = WiFi.status();
  if (!force && status == lastDrawnWifiStatus) {
    return;
  }
  lastStatusDrawMs = millis();
  lastDrawnWifiStatus = status;

  gfx->fillRect(0, 90, 800, 26, COLOR_BG);

  gfx->setTextSize(2);
  gfx->setCursor(22, 94);

  if (status == WL_CONNECTED) {
    gfx->setTextColor(COLOR_STATUS_GREEN);
    gfx->print("Connected to ");
    gfx->print(HALDEX_IP);
  } else {
    gfx->setTextColor(COLOR_STATUS_RED);
    gfx->print("Connecting to ");
    gfx->print(HALDEX_IP);
    gfx->print(" ...");
  }
}

void drawApiResult() {
  gfx->fillRect(0, 458, 800, 22, COLOR_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_TEXT_DIM);
  gfx->setCursor(10, 465);
  gfx->print(lastApiResult);
}

void drawReconnectButton(bool force = false) {
  wl_status_t status = WiFi.status();
  if (!force && status == lastDrawnReconnectStatus) {
    return;
  }
  lastDrawnReconnectStatus = status;

  uint16_t fill = (status == WL_CONNECTED) ? COLOR_GREEN_SEL : COLOR_RED_THEME;
  uint16_t border = (status == WL_CONNECTED) ? COLOR_GREEN_SEL : COLOR_RED_BORDER;
  const char *label = (status == WL_CONNECTED) ? "Connected" : "Reconnect";

  gfx->fillRoundRect(RECONNECT_BTN_X, RECONNECT_BTN_Y, RECONNECT_BTN_W, RECONNECT_BTN_H, 10, fill);
  gfx->drawRoundRect(RECONNECT_BTN_X, RECONNECT_BTN_Y, RECONNECT_BTN_W, RECONNECT_BTN_H, 10, border);

  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(RECONNECT_BTN_X + ((status == WL_CONNECTED) ? 18 : 22), RECONNECT_BTN_Y + 10);
  gfx->print(label);
}

void drawButton(int i) {
  uint16_t fill = (i == selectedButton) ? COLOR_GREEN_SEL : COLOR_RED_THEME;
  uint16_t border = (i == selectedButton) ? COLOR_GREEN_SEL : COLOR_RED_BORDER;

  gfx->fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 16, fill);
  gfx->drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 16, border);

  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(buttons[i].x + 18, buttons[i].y + (buttons[i].h / 2) - 8);
  gfx->print(buttons[i].label);
}

void drawAllButtons() {
  for (int i = 0; i < 6; i++) drawButton(i);
}

void drawUI() {
  gfx->fillScreen(COLOR_BG);
  drawHeader();
  drawReconnectButton(true);
  drawConnectionStatus(true);
  drawAllButtons();
  drawApiResult();
}

bool pointInButton(int px, int py, int i) {
  return (px >= buttons[i].x &&
          px < (buttons[i].x + buttons[i].w) &&
          py >= buttons[i].y &&
          py < (buttons[i].y + buttons[i].h));
}

bool pointInReconnectButton(int px, int py) {
  return (px >= RECONNECT_BTN_X &&
          px < (RECONNECT_BTN_X + RECONNECT_BTN_W) &&
          py >= RECONNECT_BTN_Y &&
          py < (RECONNECT_BTN_Y + RECONNECT_BTN_H));
}

void mapTouchPoint(int rawX, int rawY, int &x, int &y) {
  long mx = map(rawX, TS_MIN_X, TS_MAX_X, 0, 799);
  long my = map(rawY, TS_MIN_Y, TS_MAX_Y, 0, 479);

  int tx = constrain((int)mx, 0, 799);
  int ty = constrain((int)my, 0, 479);

  if (SWAP_XY) {
    int t = tx;
    tx = ty;
    ty = t;
  }
  if (FLIP_X) tx = 799 - tx;
  if (FLIP_Y) ty = 479 - ty;

  // Low-pass filter to reduce touch jitter
  const float alpha = 0.25f;
  if (!filtInit) {
    filtX = tx;
    filtY = ty;
    filtInit = true;
  } else {
    filtX = filtX + alpha * (tx - filtX);
    filtY = filtY + alpha * (ty - filtY);
  }

  x = (int)(filtX + 0.5f);
  y = (int)(filtY + 0.5f);
}

bool extractJsonStringField(const String &json, const char *key, String &out) {
  String token = String("\"") + key + "\":\"";
  int start = json.indexOf(token);
  if (start < 0) {
    return false;
  }

  start += token.length();
  int end = json.indexOf('"', start);
  if (end < 0) {
    return false;
  }

  out = json.substring(start, end);
  return true;
}

void refreshModeConfirmation() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.setTimeout(1200);
  http.begin(STATUS_API_URL);

  int code = http.GET();
  String resp = http.getString();
  http.end();

  if (code <= 0 || code >= 400) {
    return;
  }

  String mode;
  String effectiveMode;
  bool gotMode = extractJsonStringField(resp, "mode", mode);
  bool gotEffective = extractJsonStringField(resp, "effectiveMode", effectiveMode);

  if (gotMode || gotEffective) {
    lastApiResult = "Controller: ";
    if (gotMode) {
      lastApiResult += mode;
    } else {
      lastApiResult += "?";
    }

    if (gotEffective) {
      lastApiResult += " (effective ";
      lastApiResult += effectiveMode;
      lastApiResult += ")";
    }
  }
}

static bool postModeName(const char *modeName, int &codeOut, String &respOut) {
  HTTPClient http;
  http.setTimeout(1500);
  http.begin(MODE_API_URL);
  http.addHeader("Content-Type", "application/json");

  String body = String("{\"mode\":\"") + modeName + "\"}";

  int code = http.POST(body);
  String resp = http.getString();
  http.end();

  codeOut = code;
  respOut = resp;
  return (code > 0 && code < 400);
}

bool reconnectWiFiOnDemand(uint32_t timeoutMs, const char *statusText) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  lastApiResult = statusText;
  drawApiResult();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < timeoutMs) {
    delay(100);
  }

  drawConnectionStatus(true);
  drawReconnectButton(true);
  return WiFi.status() == WL_CONNECTED;
}

bool sendModeToHaldex(int modeIndex) {
  if (WiFi.status() != WL_CONNECTED) {
    if (!reconnectWiFiOnDemand(10000, "Reconnecting Wi-Fi...")) {
      lastApiResult = "Wi-Fi still offline, command not sent";
      return false;
    }
  }

  if (modeIndex < 0 || modeIndex >= 6) {
    lastApiResult = "Invalid mode index";
    return false;
  }

  int code = 0;
  String resp;
  bool ok = postModeName(MODE_NAMES[modeIndex], code, resp);

  // Some builds accept ratio strings with colon format only.
  if (!ok) {
    int code2 = 0;
    String resp2;
    if (postModeName(MODE_NAMES_FALLBACK[modeIndex], code2, resp2)) {
      code = code2;
      resp = resp2;
      ok = true;
    }
  }

  if (ok) {
    lastApiResult = "OK " + String(code) + " -> " + String(MODE_NAMES[modeIndex]);
    refreshModeConfirmation();
    return true;
  }

  lastApiResult = "HTTP fail " + String(code) + " -> " + String(MODE_NAMES[modeIndex]);
  if (resp.length() > 0) lastApiResult += " | " + resp;
  return false;
}

void handleWiFiStateChange() {
  wl_status_t status = WiFi.status();
  if (status == lastHandledWifiStatus) {
    return;
  }

  lastHandledWifiStatus = status;

  if (status == WL_CONNECTED) {
    lastApiResult = "Connected, syncing " + String(buttons[selectedButton].label);
    drawApiResult();
    sendModeToHaldex(selectedButton);
    drawApiResult();
    drawReconnectButton(true);
  }
}

void connectWiFiNonBlocking() {
  if (WiFi.status() == WL_CONNECTED || lastWifiAttemptMs != 0) return;

  lastWifiAttemptMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void handleTouch() {
  ts.read();
  if (!ts.isTouched) {
    // Reset filter when finger is released for a stable next touch-down
    filtInit = false;
    return;
  }

  uint32_t now = millis();
  if (now - lastTouchMs < 40) return;
  lastTouchMs = now;

  int rawX = ts.points[0].x;
  int rawY = ts.points[0].y;
  int x, y;
  mapTouchPoint(rawX, rawY, x, y);

  // Track latest mapped point for optional diagnostics.
  lastStableX = x;
  lastStableY = y;

  if (pointInReconnectButton(x, y)) {
    if (reconnectWiFiOnDemand(10000, "Reconnect requested...")) {
      lastApiResult = "Wi-Fi reconnected";
    } else {
      lastApiResult = "Reconnect failed";
    }
    drawApiResult();
    return;
  }

  int hitButton = -1;
  for (int i = 0; i < 6; i++) {
    if (pointInButton(x, y, i)) {
      hitButton = i;
      break;
    }
  }

  if (hitButton < 0) {
    return;
  }

  uint32_t nowSend = millis();
  if (nowSend - lastButtonSendMs < 220) {
    return;
  }
  lastButtonSendMs = nowSend;

  if (selectedButton != hitButton) {
    int prev = selectedButton;
    selectedButton = hitButton;
    drawButton(prev);
    drawButton(selectedButton);
  }

  // Always send when a button is tapped, even if it is already selected.
  sendModeToHaldex(selectedButton);
  drawApiResult();
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("[BOOT] setup start");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("[BOOT] backlight on");

  if (!gfx->begin()) {
    Serial.println("Display init failed");
    while (1) {}
  }
  Serial.println("[BOOT] display init ok");

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  ts.begin();
  ts.setRotation(ROTATION_NORMAL);
  Serial.println("[BOOT] touch ready");

  initButtons();
  drawUI();
  Serial.println("[BOOT] initial UI drawn");

  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  connectWiFiNonBlocking();
  Serial.println("[BOOT] setup complete");
}

void loop() {
  connectWiFiNonBlocking();
  drawConnectionStatus();
  drawReconnectButton();
  handleWiFiStateChange();
  handleTouch();
  delay(5);
}
