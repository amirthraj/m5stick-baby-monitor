#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "config.h"
#include "app_state.h"

// ============================================================
//  alert_dispatcher.h — BLE server, WiFi/MQTT client, and alert
//  delivery.
//
//  Responsibilities:
//    setupBLE()       — init GATT server, start advertising
//    setupWifi()      — captive-portal provisioning
//    setupMQTT()      — register broker address
//    maintainMqtt()   — reconnect with capped exponential backoff
//    dispatchAlert()  — orchestrate the four publish steps below
//
//  The four publish steps are kept separate so they can be
//  retried, queued, or tested independently:
//    buildAlertPayload()  — JSON string
//    publishBleAlert()    — BLE notify
//    publishMqttAlert()   — MQTT publish
//    (display is handled by status_display.h / renderAlertScreen)
// ============================================================

// ── Module-level transport objects ───────────────────────────
static BLEServer*         _pBleServer  = nullptr;
static BLECharacteristic* _pBleCharac  = nullptr;
static WiFiClient         _wifiClient;
static PubSubClient       _mqttClient(_wifiClient);

// ── BLE callbacks ────────────────────────────────────────────
class _BleServerCallbacks : public BLEServerCallbacks {
public:
    AppState* state;
    explicit _BleServerCallbacks(AppState* s) : state(s) {}

    void onConnect(BLEServer*) override {
        state->connectivity.bleConnected = true;
        Serial.println("[BLE] Client connected");
    }
    void onDisconnect(BLEServer*) override {
        state->connectivity.bleConnected = false;
        BLEDevice::startAdvertising();
        Serial.println("[BLE] Client disconnected — advertising restarted");
    }
};

// ── BLE setup ─────────────────────────────────────────────────
void setupBLE(AppState& state) {
    BLEDevice::init(BLE_DEVICE_NAME);
    _pBleServer = BLEDevice::createServer();
    _pBleServer->setCallbacks(new _BleServerCallbacks(&state));

    BLEService* svc = _pBleServer->createService(BLE_SERVICE_UUID);
    _pBleCharac = svc->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    _pBleCharac->addDescriptor(new BLE2902());
    _pBleCharac->setValue("ready");
    svc->start();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->start();

    Serial.printf("[BLE] Advertising as '%s'\n", BLE_DEVICE_NAME);
}

// ── WiFi setup ────────────────────────────────────────────────
// Launches captive portal on first boot or when BtnA held at power-on.
void setupWifi(AppState& state) {
    WiFiManager wm;

    if (M5.BtnA.isPressed()) {
        wm.resetSettings();
        Serial.println("[WiFi] Credentials wiped — portal will open");
    }

    wm.setAPCallback([](WiFiManager*) {
        // Display handled in status_display; just log here
        Serial.println("[WiFi] Portal active — connect to BabyMonitor-Setup / 192.168.4.1");
    });

    wm.setConfigPortalTimeout(180);
    bool connected = wm.autoConnect("BabyMonitor-Setup");

    state.connectivity.wifiOk = connected;
    if (connected) {
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Not connected — MQTT disabled, BLE still active");
    }
}

// ── MQTT setup ────────────────────────────────────────────────
void setupMQTT(AppState& state) {
    if (!state.connectivity.wifiOk) return;
    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setKeepAlive(60);
    Serial.printf("[MQTT] Broker: %s:%d  Topic: %s\n", MQTT_BROKER, MQTT_PORT, MQTT_TOPIC);
}

// ── MQTT reconnect with capped exponential backoff ────────────
// Call every loop(). Does nothing if already connected or backoff
// period hasn't elapsed. Doubles backoff on each failure, capped
// at MQTT_BACKOFF_MAX_MS.
void maintainMqtt(AppState& state, unsigned long now) {
    // Keep wifiOk in sync with actual WiFi state each call
    state.connectivity.wifiOk = (WiFi.status() == WL_CONNECTED);
    if (!state.connectivity.wifiOk) {
        state.connectivity.mqttConnected = false;
        return;
    }

    _mqttClient.loop();
    state.connectivity.mqttConnected = _mqttClient.connected();

    if (state.connectivity.mqttConnected) {
        // Reset backoff when healthy
        state.connectivity.mqttBackoffMs = MQTT_BACKOFF_MIN_MS;
        return;
    }

    // Not connected — wait for backoff window then retry
    if ((now - state.connectivity.lastMqttAttemptMs) < state.connectivity.mqttBackoffMs) {
        return;
    }

    state.connectivity.lastMqttAttemptMs = now;
    Serial.printf("[MQTT] Reconnecting (backoff %lu ms)...\n", state.connectivity.mqttBackoffMs);

    if (_mqttClient.connect(MQTT_CLIENT_ID)) {
        state.connectivity.mqttConnected = true;
        state.connectivity.mqttBackoffMs = MQTT_BACKOFF_MIN_MS;
        Serial.println("[MQTT] Connected");
    } else {
        // Double the backoff, cap at maximum
        state.connectivity.mqttBackoffMs =
            min(state.connectivity.mqttBackoffMs * 2, (uint32_t)MQTT_BACKOFF_MAX_MS);
        Serial.printf("[MQTT] Failed (rc=%d) — next retry in %lu ms\n",
                      _mqttClient.state(), state.connectivity.mqttBackoffMs);
    }
}

// ── Heartbeat publish ─────────────────────────────────────────
// Publish a lightweight status payload on a slow cadence so
// subscribers can detect a silent device.
void publishHeartbeat(AppState& state, unsigned long now) {
    if (!state.connectivity.mqttConnected) return;
    if ((now - state.alerts.lastHeartbeatMs) < HEARTBEAT_INTERVAL_MS) return;

    state.alerts.lastHeartbeatMs = now;
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"device\":\"%s\",\"uptime\":%lu,\"ble\":%s,\"wifi\":%s}",
             DEVICE_ID, now / 1000,
             state.connectivity.bleConnected  ? "true" : "false",
             state.connectivity.wifiOk        ? "true" : "false");
    _mqttClient.publish(MQTT_HEARTBEAT_TOPIC, payload);
    Serial.printf("[MQTT] Heartbeat: %s\n", payload);
}

// ── Alert payload builder ─────────────────────────────────────
// Writes a JSON alert string into buf (caller-owned, size bytes).
void buildAlertPayload(char* buf, size_t size, const char* type, unsigned long now) {
    snprintf(buf, size,
             "{\"type\":\"%s\",\"device\":\"%s\",\"ts\":%lu}",
             type, DEVICE_ID, now);
}

// ── BLE publish ───────────────────────────────────────────────
void publishBleAlert(AppState& state, const char* payload) {
    if (!state.connectivity.bleConnected) {
        Serial.println("[BLE] No client — skipped");
        return;
    }
    _pBleCharac->setValue(payload);
    _pBleCharac->notify();
    Serial.println("[BLE] Notification sent");
}

// ── MQTT publish ──────────────────────────────────────────────
void publishMqttAlert(AppState& state, const char* payload) {
    if (!state.connectivity.mqttConnected) {
        Serial.println("[MQTT] Not connected — skipped");
        return;
    }
    bool ok = _mqttClient.publish(MQTT_TOPIC, payload);
    Serial.printf("[MQTT] Publish %s\n", ok ? "OK" : "FAILED");
}

// ── Alert orchestrator ────────────────────────────────────────
// Builds the payload once and hands it to each transport.
// Display rendering is intentionally separate (see renderAlertScreen).
void dispatchAlert(AppState& state, const char* type, unsigned long now) {
    char payload[128];
    buildAlertPayload(payload, sizeof(payload), type, now);
    Serial.printf("[ALERT] %s\n", payload);

    publishBleAlert(state, payload);
    publishMqttAlert(state, payload);
    // Caller is responsible for updating alert timestamps and display.
}
