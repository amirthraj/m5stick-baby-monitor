#pragma once
#include <Arduino.h>
#include "config.h"
#include "app_state.h"

// ============================================================
//  motion_detector.h — PIR AS312 sampling and motion detection
//
//  Call acquireMotionSample() once per loop() to populate
//  state.sensor.pirActive, then call checkMotion() to decide
//  whether to fire an alert.
// ============================================================

// ── Step 1: sample ────────────────────────────────────────────
// Read the PIR pin and update state.sensor.pirActive.
// No-op if PIR_ENABLED is false.
void acquireMotionSample(AppState& state) {
    if (!PIR_ENABLED) {
        state.sensor.pirActive = false;
        return;
    }
    state.sensor.pirActive = (digitalRead(PIR_PIN) == HIGH);
}

// ── Step 2: detect ────────────────────────────────────────────
// Returns true when a motion alert should fire.
bool checkMotion(AppState& state, unsigned long now) {
    if (!PIR_ENABLED || !state.sensor.pirActive) return false;
    if ((now - state.alerts.lastMotionAlertMs) >= ALERT_COOLDOWN_MS) {
        return true;
    }
    return false;
}
