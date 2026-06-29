/*
 * ============================================================
 *  Baby Monitor — M5StickC Plus 2  (refactored v2)
 * ============================================================
 *  Hardware:
 *    - M5StickC Plus 2  (ESP32-PICO-V3-02, built-in mic + IMU)
 *    - M5Stack PIR AS312 sensor (GROVE port) — optional
 *
 *  File layout:
 *    config.h           — all user settings (edit here)
 *    app_state.h        — shared typed state structs
 *    audio_detector.h   — mic sampling + cry detection
 *    motion_detector.h  — PIR sampling + motion detection
 *    alert_dispatcher.h — BLE server, WiFi/MQTT client, alert delivery
 *    status_display.h   — all LCD rendering
 *    baby_monitor.ino   — setup() + loop() orchestration only
 *
 *  Key architectural changes from v1:
 *    - Single mic acquisition per loop (no double-read)
 *    - Typed AppState shared by all modules (no scattered globals)
 *    - sendAlert() split into build / BLE / MQTT / display steps
 *    - MQTT reconnect uses capped exponential backoff
 *    - wifiOk updated dynamically from WiFi.status() each loop
 *    - DEVICE_ID separated from MQTT_CLIENT_ID
 *    - Degraded mode displayed when both transports are down
 *    - Heartbeat published on MQTT_HEARTBEAT_TOPIC every 60 s
 *
 *  Libraries required (install via Library Manager):
 *    - M5Unified        (by M5Stack)
 *    - PubSubClient     (by Nick O'Leary)
 *    - WiFiManager      (by tzapu)
 *    BLE is bundled with the ESP32 board package.
 * ============================================================
 */

#include <M5Unified.h>
#include "config.h"
#include "app_state.h"
#include "audio_detector.h"
#include "motion_detector.h"
#include "alert_dispatcher.h"
#include "status_display.h"

// ── Shared application state ──────────────────────────────────
// One instance, passed by reference to every module.
// No module-level globals except transport objects inside alert_dispatcher.h.
AppState g_state;

// ============================================================
//  SETUP
// ============================================================

void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5.begin(cfg);

    // Display — landscape, dim for dark room
    M5.Display.setRotation(3);
    M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(M5.Display.color565(180, 180, 180));
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Baby Monitor");
    M5.Display.println("Starting...");

    g_state.display.lastActivityMs = millis();
    g_state.display.displayOn      = true;

    // PIR pin
    if (PIR_ENABLED) {
        pinMode(PIR_PIN, INPUT);
        Serial.println("[PIR] GPIO 32 configured");
    } else {
        Serial.println("[PIR] Disabled — enable PIR_ENABLED when sensor arrives");
    }

    // Microphone
    M5.Mic.begin();
    Serial.println("[MIC] Initialised");

    // BLE
    M5.Display.println("BLE...");
    setupBLE(g_state);

    // WiFi (blocks during portal; updates g_state.connectivity.wifiOk)
    M5.Display.println("WiFi...");
    renderPortalScreen();   // pre-draw portal instructions in case portal opens
    setupWifi(g_state);

    // MQTT
    setupMQTT(g_state);
    maintainMqtt(g_state, millis());  // initial connection attempt

    // Ready splash
    M5.Display.fillScreen(M5.Display.color565(0, 80, 20));
    M5.Display.setCursor(10, 50);
    M5.Display.setTextColor(M5.Display.color565(180, 180, 180));
    M5.Display.setTextSize(3);
    M5.Display.println("Ready!");
    delay(1500);

    Serial.println("[SETUP] Done — monitoring started");
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
    M5.update();
    unsigned long now = millis();

    // ── 1. Maintain connectivity ───────────────────────────
    // Keeps wifiOk dynamic and handles MQTT reconnect with backoff.
    maintainMqtt(g_state, now);

    // ── 2. Button: wake display ────────────────────────────
    if (M5.BtnA.wasPressed()) {
        wakeDisplay(g_state);
    }

    // ── 3. Acquire sensor samples (once per cycle) ─────────
    // Both detectors read from state — no second mic call anywhere.
    acquireSoundSample(g_state);
    acquireMotionSample(g_state);

    // ── 4. Run detection ───────────────────────────────────
    bool soundAlert  = checkSound(g_state, now);
    bool motionAlert = checkMotion(g_state, now);

    // ── 5. Dispatch alerts ─────────────────────────────────
    if (soundAlert) {
        g_state.alerts.lastSoundAlertMs = now;
        dispatchAlert(g_state, "sound", now);
        renderAlertScreen(g_state, "sound");
    }

    if (motionAlert) {
        g_state.alerts.lastMotionAlertMs = now;
        dispatchAlert(g_state, "motion", now);
        renderAlertScreen(g_state, "motion");
    }

    // ── 6. Heartbeat ───────────────────────────────────────
    publishHeartbeat(g_state, now);

    // ── 7. Display sleep / status refresh ─────────────────
    checkDisplaySleep(g_state, now);

    if (g_state.display.displayOn &&
        !soundAlert && !motionAlert &&
        (now - g_state.display.lastRefreshMs) >= STATUS_REFRESH_MS) {
        g_state.display.lastRefreshMs = now;
        renderStatusScreen(g_state);
    }

    // ~20 checks/second; keep short so mic record completes cleanly
    delay(50);
}
