#pragma once
#include <M5Unified.h>
#include "config.h"
#include "app_state.h"

// ============================================================
//  status_display.h — all LCD rendering in one place
//
//  Functions:
//    wakeDisplay()          — turn backlight on, reset sleep timer
//    sleepDisplay()         — turn backlight off
//    checkDisplaySleep()    — auto-sleep after inactivity
//    renderStatusScreen()   — idle status (BLE/WiFi/MQTT/mic/PIR)
//    renderAlertScreen()    — alert banner (called after dispatchAlert)
//    renderPortalScreen()   — WiFi setup instructions
//    renderDegradedBanner() — shown when both transports are down
// ============================================================

// ── Color palette ─────────────────────────────────────────────
inline uint32_t colorAlertSound()  { return M5.Display.color565(140,  20,  20); } // dark red
inline uint32_t colorAlertMotion() { return M5.Display.color565(140,  80,   0); } // dark amber
inline uint32_t colorReady()       { return M5.Display.color565(  0,  80,  20); } // dark green
inline uint32_t colorDimText()     { return M5.Display.color565(180, 180, 180); } // soft white
inline uint32_t colorDimGreen()    { return M5.Display.color565(  0, 160,  50); } // dim green
inline uint32_t colorDimGrey()     { return M5.Display.color565( 60,  60,  60); } // dark grey
inline uint32_t colorDimTeal()     { return M5.Display.color565(  0,  80, 100); } // mic bar normal
inline uint32_t colorDimRed()      { return M5.Display.color565(140,  20,  20); } // mic bar clipping
inline uint32_t colorDegraded()    { return M5.Display.color565( 80,  40,   0); } // dark orange

// ── Wake / sleep ──────────────────────────────────────────────
void wakeDisplay(AppState& state) {
    if (!state.display.displayOn) {
        M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
        state.display.displayOn = true;
    }
    state.display.lastActivityMs = millis();
}

void sleepDisplay(AppState& state) {
    if (state.display.displayOn) {
        M5.Display.setBrightness(0);
        state.display.displayOn = false;
    }
}

void checkDisplaySleep(AppState& state, unsigned long now) {
    if (state.display.displayOn &&
        (now - state.display.lastActivityMs) >= DISPLAY_SLEEP_MS) {
        sleepDisplay(state);
    }
}

// ── Status screen ─────────────────────────────────────────────
// Reads sensor and connectivity state from AppState.
// soundLevel is already in state.sensor.soundLevel — no second mic read.
void renderStatusScreen(const AppState& state) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(2);

    // Title
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(colorDimText());
    M5.Display.println("Baby Monitor");

    // Degraded warning
    if (isDegraded(state)) {
        M5.Display.setCursor(0, 20);
        M5.Display.setTextColor(colorDegraded());
        M5.Display.println("! NO CHANNEL");
    }

    // BLE
    M5.Display.setCursor(0, 30);
    M5.Display.setTextColor(state.connectivity.bleConnected ? colorDimGreen() : colorDimGrey());
    M5.Display.print("BLE: ");
    M5.Display.println(state.connectivity.bleConnected ? "Connected" : "Waiting");

    // WiFi
    M5.Display.setCursor(0, 55);
    M5.Display.setTextColor(state.connectivity.wifiOk ? colorDimGreen() : colorDimGrey());
    M5.Display.print("WiFi: ");
    M5.Display.println(state.connectivity.wifiOk ? "OK" : "---");

    // MQTT
    M5.Display.setCursor(0, 80);
    M5.Display.setTextColor(state.connectivity.mqttConnected ? colorDimGreen() : colorDimGrey());
    M5.Display.print("MQTT: ");
    M5.Display.println(state.connectivity.mqttConnected ? "OK" : "---");

    // Mic level bar (uses pre-sampled soundLevel from state)
    M5.Display.setCursor(0, 110);
    M5.Display.setTextColor(colorDimText());
    M5.Display.print("Mic:");
    int barW = map(constrain(state.sensor.soundLevel, 0, 4000), 0, 4000,
                   0, M5.Display.width() - 45);
    uint32_t barColor = (state.sensor.soundLevel >= SOUND_THRESHOLD)
                        ? colorDimRed() : colorDimTeal();
    M5.Display.fillRect(45, 112, barW, 14, barColor);

    // PIR
    M5.Display.setCursor(0, 135);
    M5.Display.setTextColor(PIR_ENABLED ? colorDimText() : colorDimGrey());
    M5.Display.print("PIR: ");
    if (PIR_ENABLED) {
        M5.Display.println(state.sensor.pirActive ? "ACTIVE" : "clear");
    } else {
        M5.Display.println("not fitted");
    }
}

// ── Alert screen ──────────────────────────────────────────────
// type: "sound" or "motion"
void renderAlertScreen(AppState& state, const char* type) {
    wakeDisplay(state);
    bool isSound = (strcmp(type, "sound") == 0);
    M5.Display.fillScreen(isSound ? colorAlertSound() : colorAlertMotion());
    M5.Display.setTextColor(colorDimText());
    M5.Display.setTextSize(3);
    M5.Display.setCursor(10, 30);
    M5.Display.println(isSound ? "CRYING!" : "MOTION!");
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 80);
    M5.Display.println("Alert sent");
    // Nudge activity timer so alert screen shows for ALERT_SCREEN_MS
    state.display.lastActivityMs = millis() - (DISPLAY_SLEEP_MS - ALERT_SCREEN_MS);
}

// ── WiFi portal screen ────────────────────────────────────────
void renderPortalScreen() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(M5.Display.color565(180, 180, 180));
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("WiFi Setup:");
    M5.Display.println("");
    M5.Display.println("Connect to:");
    M5.Display.setTextColor(M5.Display.color565(0, 160, 50));
    M5.Display.println("BabyMonitor-Setup");
    M5.Display.setTextColor(M5.Display.color565(180, 180, 180));
    M5.Display.println("");
    M5.Display.println("Then open:");
    M5.Display.setTextColor(M5.Display.color565(0, 160, 50));
    M5.Display.println("192.168.4.1");
}
