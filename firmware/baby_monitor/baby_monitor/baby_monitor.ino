/*
 * ============================================================
 *  Baby Monitor — M5StickC Plus 2
 * ============================================================
 *  Hardware:
 *    - M5StickC Plus 2  (ESP32-PICO-V3-02, built-in mic + IMU)
 *    - M5Stack PIR AS312 sensor connected to GROVE port
 *
 *  Alerts sent via:
 *    - BLE (Bluetooth Low Energy) notifications
 *    - MQTT over WiFi
 *
 *  Libraries required (install via Library Manager):
 *    - M5Unified        (by M5Stack)
 *    - PubSubClient     (by Nick O'Leary)
 *    - WiFiManager      (by tzapu)
 *    BLE is bundled with the ESP32 board package.
 * ============================================================
 */

#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

// ============================================================
//  CONFIGURATION — edit these before uploading
// ============================================================

// WiFi credentials are entered via the setup portal on first boot.
// No passwords stored in code. To reset and re-enter, hold the side
// button while powering on.

// MQTT broker (HiveMQ free public broker — no account needed)
// For a private setup, replace with your broker's IP or hostname
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "babymonitor-k9m2xw47/alerts";  // unique — change if you want
const char* MQTT_CLIENT_ID = "m5stick-babymon-001";      // must be unique on the broker

// PIR sensor pin (GROVE port on M5StickC Plus 2)
// Set to true once the AS312 PIR sensor arrives and is connected
#define PIR_ENABLED false
#define PIR_PIN 32

// Sound detection
// Increase if you get too many false alarms; decrease if missing real cries
#define SOUND_THRESHOLD 1500   // RMS amplitude (0-32767 range)
#define SOUND_DURATION_MS 600  // sound must exceed threshold for this many ms

// Alert cooldown — prevents rapid repeated alerts for the same event
#define ALERT_COOLDOWN_MS 15000  // 15 seconds

// Display brightness (0–255)
// 20 = very dim, good for dark room next to baby
// 80 = readable in daylight
#define DISPLAY_BRIGHTNESS 20

// Auto-off: display turns off after this many ms of no alerts/activity
// Press the M5 side button (BtnA) to wake it back up
#define DISPLAY_SLEEP_MS 30000  // 30 seconds

// How long the alert screen stays on before dimming again
#define ALERT_SCREEN_MS 8000  // 8 seconds

// Dim alert colors (easier on eyes in a dark room)
#define COLOR_ALERT_SOUND M5.Display.color565(140, 20, 20)  // dark red
#define COLOR_ALERT_MOTION M5.Display.color565(140, 80, 0)  // dark amber
#define COLOR_READY M5.Display.color565(0, 80, 20)          // dark green
#define COLOR_DIM_TEXT M5.Display.color565(180, 180, 180)   // soft white
#define COLOR_DIM_GREEN M5.Display.color565(0, 160, 50)     // dim green
#define COLOR_DIM_GREY M5.Display.color565(60, 60, 60)      // dark grey

// BLE identifiers (UUIDs — these can stay as-is)
#define BLE_SERVICE_UUID "ba5e0001-c7e8-4a5c-b9e1-2a3e7b8c9d0f"
#define BLE_CHARACTERISTIC_UUID "ba5e0002-c7e8-4a5c-b9e1-2a3e7b8c9d0f"

// ============================================================
//  GLOBALS
// ============================================================

// BLE
BLEServer* pBleServer = nullptr;
BLECharacteristic* pBleCharac = nullptr;
bool bleConnected = false;

// WiFi / MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool wifiOk = false;

// Alert timing
unsigned long lastSoundAlertMs = 0;
unsigned long lastMotionAlertMs = 0;

// Display sleep tracking
unsigned long lastActivityMs = 0;  // last time display was woken
bool displayOn = true;

// Sound detection state
unsigned long soundAboveThresholdSince = 0;
bool soundTriggering = false;

// Mic buffer
static int16_t micBuffer[512];

// ============================================================
//  BLE CALLBACKS
// ============================================================

class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    bleConnected = true;
  }
  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    // Restart advertising so new connections are possible
    BLEDevice::startAdvertising();
  }
};

// ============================================================
//  SETUP FUNCTIONS
// ============================================================

void setupBLE() {
  BLEDevice::init("BabyMonitor");
  pBleServer = BLEDevice::createServer();
  pBleServer->setCallbacks(new BleServerCallbacks());

  BLEService* pService = pBleServer->createService(BLE_SERVICE_UUID);

  pBleCharac = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  // Add the Client Characteristic Configuration descriptor (required for notify)
  pBleCharac->addDescriptor(new BLE2902());
  pBleCharac->setValue("ready");

  pService->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(BLE_SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->start();

  Serial.println("[BLE] Advertising as 'BabyMonitor'");
}

void setupWifi() {
  // Hold side button on boot to wipe saved credentials and re-enter
  if (M5.BtnA.isPressed()) {
    WiFiManager wm;
    wm.resetSettings();
    Serial.println("[WiFi] Credentials wiped — will re-enter via portal");
  }

  WiFiManager wm;

  // Show portal instructions on display
  wm.setAPCallback([](WiFiManager* w) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(COLOR_DIM_TEXT);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("WiFi Setup:");
    M5.Display.println("");
    M5.Display.println("Connect to:");
    M5.Display.setTextColor(COLOR_DIM_GREEN);
    M5.Display.println("BabyMonitor-Setup");
    M5.Display.setTextColor(COLOR_DIM_TEXT);
    M5.Display.println("");
    M5.Display.println("Then open:");
    M5.Display.setTextColor(COLOR_DIM_GREEN);
    M5.Display.println("192.168.4.1");
    Serial.println("[WiFi] Portal active — connect to BabyMonitor-Setup");
  });

  // Blocks here until connected (or portal times out after 3 min)
  wm.setConfigPortalTimeout(180);
  bool connected = wm.autoConnect("BabyMonitor-Setup");

  wifiOk = connected;
  if (wifiOk) {
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Not connected — MQTT disabled, BLE still active");
  }
}

void setupMQTT() {
  if (!wifiOk) return;
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  Serial.printf("[MQTT] Broker: %s:%d  Topic: %s\n", MQTT_BROKER, MQTT_PORT, MQTT_TOPIC);
}

// ============================================================
//  ALERT FUNCTIONS
// ============================================================

// Ensure MQTT is connected (non-blocking reconnect attempt)
void ensureMqttConnected() {
  if (!wifiOk || mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("[MQTT] Reconnecting...");
  mqttClient.connect(MQTT_CLIENT_ID);
}

// Send alert payload over BLE + MQTT
// type: "sound" or "motion"
void sendAlert(const char* type) {
  // Build JSON payload
  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"type\":\"%s\",\"device\":\"%s\",\"ts\":%lu}",
           type, MQTT_CLIENT_ID, millis());

  Serial.printf("[ALERT] %s\n", payload);

  // ── BLE notify ──────────────────────────────
  if (bleConnected) {
    pBleCharac->setValue(payload);
    pBleCharac->notify();
    Serial.println("[BLE] Notification sent");
  } else {
    Serial.println("[BLE] No client connected — skipped");
  }

  // ── MQTT publish ────────────────────────────
  ensureMqttConnected();
  if (mqttClient.connected()) {
    bool ok = mqttClient.publish(MQTT_TOPIC, payload);
    Serial.printf("[MQTT] Publish %s\n", ok ? "OK" : "FAILED");
  } else {
    Serial.println("[MQTT] Not connected — skipped");
  }

  // ── Display alert ───────────────────────────
  wakeDisplay();
  bool isSound = (strcmp(type, "sound") == 0);
  M5.Display.fillScreen(isSound ? COLOR_ALERT_SOUND : COLOR_ALERT_MOTION);
  M5.Display.setTextColor(COLOR_DIM_TEXT);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(10, 30);
  M5.Display.println(isSound ? "CRYING!" : "MOTION!");
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 80);
  M5.Display.println("Alert sent");
  // Let the alert screen show briefly, then status will refresh
  lastActivityMs = millis() - (DISPLAY_SLEEP_MS - ALERT_SCREEN_MS);
}

// ============================================================
//  SOUND DETECTION
// ============================================================

// Returns RMS amplitude of current mic sample (0–32767)
int readSoundLevel() {
  if (!M5.Mic.record(micBuffer, 512, 16000)) {
    return 0;
  }
  int64_t sumSq = 0;
  for (int i = 0; i < 512; i++) {
    sumSq += (int64_t)micBuffer[i] * micBuffer[i];
  }
  return (int)sqrt((double)(sumSq / 512));
}

void checkSound() {
  int level = readSoundLevel();
  unsigned long now = millis();

  if (level >= SOUND_THRESHOLD) {
    if (!soundTriggering) {
      // First sample above threshold — start timing
      soundTriggering = true;
      soundAboveThresholdSince = now;
    } else if ((now - soundAboveThresholdSince) >= SOUND_DURATION_MS) {
      // Sound has been sustained long enough — alert if cooldown passed
      if ((now - lastSoundAlertMs) >= ALERT_COOLDOWN_MS) {
        lastSoundAlertMs = now;
        sendAlert("sound");
      }
      soundTriggering = false;  // reset so we don't re-fire immediately
    }
  } else {
    // Below threshold — reset
    soundTriggering = false;
  }
}

// ============================================================
//  MOTION DETECTION
// ============================================================

void checkMotion() {
  if (!PIR_ENABLED) return;  // skip until sensor is connected
  if (digitalRead(PIR_PIN) == HIGH) {
    unsigned long now = millis();
    if ((now - lastMotionAlertMs) >= ALERT_COOLDOWN_MS) {
      lastMotionAlertMs = now;
      sendAlert("motion");
    }
  }
}

// ============================================================
//  STATUS DISPLAY
// ============================================================

// ============================================================
//  DISPLAY SLEEP / WAKE
// ============================================================

void wakeDisplay() {
  if (!displayOn) {
    M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
    displayOn = true;
  }
  lastActivityMs = millis();
}

void sleepDisplay() {
  if (displayOn) {
    M5.Display.setBrightness(0);
    displayOn = false;
  }
}

void updateStatusDisplay(int soundLevel) {
  if (!displayOn) return;  // don't bother drawing if screen is off

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);

  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(COLOR_DIM_TEXT);
  M5.Display.println("Baby Monitor");

  // BLE status
  M5.Display.setCursor(0, 30);
  M5.Display.setTextColor(bleConnected ? COLOR_DIM_GREEN : COLOR_DIM_GREY);
  M5.Display.print("BLE: ");
  M5.Display.println(bleConnected ? "Connected" : "Waiting");

  // WiFi status
  M5.Display.setCursor(0, 55);
  M5.Display.setTextColor(wifiOk ? COLOR_DIM_GREEN : COLOR_DIM_GREY);
  M5.Display.print("WiFi: ");
  M5.Display.println(wifiOk ? "OK" : "---");

  // MQTT status
  M5.Display.setCursor(0, 80);
  M5.Display.setTextColor(mqttClient.connected() ? COLOR_DIM_GREEN : COLOR_DIM_GREY);
  M5.Display.print("MQTT: ");
  M5.Display.println(mqttClient.connected() ? "OK" : "---");

  // Sound level bar (dim cyan / dim red)
  M5.Display.setCursor(0, 110);
  M5.Display.setTextColor(COLOR_DIM_TEXT);
  M5.Display.print("Mic:");
  int barW = map(constrain(soundLevel, 0, 4000), 0, 4000, 0, M5.Display.width() - 45);
  uint32_t barColor = soundLevel >= SOUND_THRESHOLD
                        ? M5.Display.color565(140, 20, 20)  // dim red
                        : M5.Display.color565(0, 80, 100);  // dim teal
  M5.Display.fillRect(45, 112, barW, 14, barColor);

  // PIR state
  M5.Display.setCursor(0, 135);
  M5.Display.setTextColor(PIR_ENABLED ? COLOR_DIM_TEXT : COLOR_DIM_GREY);
  M5.Display.print("PIR: ");
  if (PIR_ENABLED) {
    M5.Display.println(digitalRead(PIR_PIN) == HIGH ? "ACTIVE" : "clear");
  } else {
    M5.Display.println("not fitted");
  }
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  // Initialise M5 hardware
  auto cfg = M5.config();
  M5.begin(cfg);

  // Display orientation (landscape) + low brightness
  M5.Display.setRotation(3);
  M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(COLOR_DIM_TEXT);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Baby Monitor");
  M5.Display.println("Starting...");
  lastActivityMs = millis();

  // PIR sensor (disabled until sensor arrives)
  if (PIR_ENABLED) {
    pinMode(PIR_PIN, INPUT);
    Serial.println("[PIR] Pin 32 configured");
  } else {
    Serial.println("[PIR] Disabled — enable PIR_ENABLED when sensor arrives");
  }

  // Microphone
  M5.Mic.begin();
  Serial.println("[MIC] Initialised");

  // BLE
  M5.Display.println("BLE...");
  setupBLE();

  // WiFi + MQTT
  M5.Display.println("WiFi...");
  setupWifi();
  setupMQTT();
  ensureMqttConnected();

  // Ready
  M5.Display.fillScreen(COLOR_READY);
  M5.Display.setCursor(10, 50);
  M5.Display.setTextColor(COLOR_DIM_TEXT);
  M5.Display.setTextSize(3);
  M5.Display.println("Ready!");
  delay(1500);

  Serial.println("[SETUP] Done — monitoring started");
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
  M5.update();        // update button state
  mqttClient.loop();  // keep MQTT connection alive

  unsigned long now = millis();

  // Side button (BtnA) wakes the display
  if (M5.BtnA.wasPressed()) {
    wakeDisplay();
  }

  // Auto-sleep display after inactivity
  if (displayOn && (now - lastActivityMs >= DISPLAY_SLEEP_MS)) {
    sleepDisplay();
  }

  // Core detection (always runs regardless of display state)
  checkSound();
  checkMotion();

  // Refresh status display every 3 seconds (only if display is on)
  static unsigned long lastDisplayMs = 0;
  if (displayOn && (now - lastDisplayMs >= 3000)) {
    lastDisplayMs = now;
    int lvl = readSoundLevel();
    updateStatusDisplay(lvl);
  }

  // Reconnect MQTT periodically if disconnected
  static unsigned long lastMqttCheck = 0;
  if (now - lastMqttCheck >= 30000) {
    lastMqttCheck = now;
    ensureMqttConnected();
  }

  delay(50);  // ~20 checks/second
}
