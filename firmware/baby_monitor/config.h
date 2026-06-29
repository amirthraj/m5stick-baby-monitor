#pragma once

// ============================================================
//  config.h — all user-facing settings in one place
//  Edit here; everything else reads these values.
// ============================================================

// ── Device identity ─────────────────────────────────────────
// DEVICE_ID   identifies the physical device in alert payloads.
// MQTT_CLIENT_ID identifies this TCP session on the broker.
// Keep them independent: same device can reconnect with the same
// client ID without changing what it reports as its identity.
constexpr char DEVICE_ID[]      = "m5stick-babymon-001";
constexpr char MQTT_CLIENT_ID[] = "babymon-c-k9m2xw47";   // must be unique on the broker

// ── MQTT broker ─────────────────────────────────────────────
// HiveMQ free public broker — no account needed.
// For production swap for a private broker + TLS on port 8883.
constexpr char     MQTT_BROKER[] = "broker.hivemq.com";
constexpr uint16_t MQTT_PORT     = 1883;
constexpr char     MQTT_TOPIC[]  = "babymonitor-k9m2xw47/alerts";
constexpr char     MQTT_HEARTBEAT_TOPIC[] = "babymonitor-k9m2xw47/status";

// ── MQTT reconnect backoff ───────────────────────────────────
constexpr uint32_t MQTT_BACKOFF_MIN_MS = 5000;   //  5 s initial retry delay
constexpr uint32_t MQTT_BACKOFF_MAX_MS = 60000;  // 60 s maximum retry delay

// ── BLE ─────────────────────────────────────────────────────
constexpr char BLE_DEVICE_NAME[]         = "BabyMonitor";
constexpr char BLE_SERVICE_UUID[]        = "ba5e0001-c7e8-4a5c-b9e1-2a3e7b8c9d0f";
constexpr char BLE_CHARACTERISTIC_UUID[] = "ba5e0002-c7e8-4a5c-b9e1-2a3e7b8c9d0f";

// ── PIR motion sensor ────────────────────────────────────────
// Set PIR_ENABLED to true once the M5Stack AS312 sensor is plugged
// into the GROVE port, then re-flash.
constexpr bool    PIR_ENABLED = false;
constexpr uint8_t PIR_PIN     = 32;   // GROVE yellow wire → GPIO 32

// ── Sound detection ──────────────────────────────────────────
// SOUND_THRESHOLD  — RMS amplitude (0–32767) that counts as loud.
//   Increase to reduce sensitivity / fewer false alarms.
//   Decrease to catch quieter crying.
// SOUND_DURATION_MS — sound must stay above threshold this long
//   before an alert fires. Filters out brief transients.
constexpr uint16_t SOUND_THRESHOLD    = 1500;
constexpr uint32_t SOUND_DURATION_MS  = 600;
constexpr uint16_t MIC_SAMPLE_COUNT   = 512;
constexpr uint32_t MIC_SAMPLE_RATE_HZ = 16000;

// ── Alert cooldown ───────────────────────────────────────────
// Minimum gap between consecutive alerts of the same type.
constexpr uint32_t ALERT_COOLDOWN_MS = 15000;

// ── Display ──────────────────────────────────────────────────
constexpr uint8_t  DISPLAY_BRIGHTNESS = 20;     // 0–255; 20 = dark-room friendly
constexpr uint32_t DISPLAY_SLEEP_MS   = 30000;  // auto-off after 30 s of inactivity
constexpr uint32_t ALERT_SCREEN_MS    = 8000;   // alert banner hold time
constexpr uint32_t STATUS_REFRESH_MS  = 3000;   // status screen redraw interval

// ── Heartbeat ────────────────────────────────────────────────
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;  // publish status every 60 s
