#pragma once

// ============================================================
//  app_state.h — shared typed state for the whole application
//  All modules read from / write to an AppState instance.
//  No globals scattered across files.
// ============================================================

struct ConnectivityState {
    bool bleConnected  = false;
    bool wifiOk        = false;  // updated dynamically each loop from WiFi.status()
    bool mqttConnected = false;

    // MQTT reconnect backoff
    unsigned long lastMqttAttemptMs = 0;
    uint32_t      mqttBackoffMs     = 5000;  // starts at MQTT_BACKOFF_MIN_MS
};

struct SensorState {
    int  soundLevel               = 0;    // RMS value from last sample, shared by all consumers
    bool pirActive                = false;

    // Internal detection state — managed by audio_detector
    bool          soundTriggering          = false;
    unsigned long soundAboveThresholdSince = 0;
};

struct AlertState {
    unsigned long lastSoundAlertMs  = 0;
    unsigned long lastMotionAlertMs = 0;
    unsigned long lastHeartbeatMs   = 0;
};

struct DisplayState {
    bool          displayOn      = true;
    unsigned long lastActivityMs = 0;
    unsigned long lastRefreshMs  = 0;
};

// ── Top-level container ──────────────────────────────────────
struct AppState {
    ConnectivityState connectivity;
    SensorState       sensor;
    AlertState        alerts;
    DisplayState      display;
};

// ── Degraded mode helpers ────────────────────────────────────
// Returns true if at least one alert channel is operational.
inline bool hasAlertChannel(const AppState& s) {
    return s.connectivity.bleConnected || s.connectivity.mqttConnected;
}

// Returns true if all transports are down (degraded mode).
inline bool isDegraded(const AppState& s) {
    return !s.connectivity.bleConnected && !s.connectivity.mqttConnected;
}
