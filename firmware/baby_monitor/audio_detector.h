#pragma once
#include <M5Unified.h>
#include <math.h>
#include "config.h"
#include "app_state.h"

// ============================================================
//  audio_detector.h — mic sampling and cry detection
//
//  Call acquireSoundSample() once per loop() to populate
//  state.sensor.soundLevel, then call checkSound() to run the
//  sustained-threshold detection logic.
//  Never call acquireSoundSample() from more than one place —
//  that was the double-read bug in the original sketch.
// ============================================================

static int16_t _micBuffer[MIC_SAMPLE_COUNT];

// ── Step 1: sample ────────────────────────────────────────────
// Read the microphone and store the RMS level in state.
// Called exactly once per loop cycle, before any detection or UI.
void acquireSoundSample(AppState& state) {
    if (!M5.Mic.record(_micBuffer, MIC_SAMPLE_COUNT, MIC_SAMPLE_RATE_HZ)) {
        state.sensor.soundLevel = 0;
        return;
    }
    int64_t sumSq = 0;
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        sumSq += (int64_t)_micBuffer[i] * _micBuffer[i];
    }
    state.sensor.soundLevel = (int)sqrt((double)(sumSq / MIC_SAMPLE_COUNT));
}

// ── Step 2: detect ────────────────────────────────────────────
// Run the sustained-threshold gate against state.sensor.soundLevel.
// Returns true when a sound alert should fire (cooldown already checked).
// Updates detection bookkeeping inside state.
bool checkSound(AppState& state, unsigned long now) {
    if (state.sensor.soundLevel >= SOUND_THRESHOLD) {
        if (!state.sensor.soundTriggering) {
            // First sample above threshold — start timing
            state.sensor.soundTriggering          = true;
            state.sensor.soundAboveThresholdSince = now;
        } else if ((now - state.sensor.soundAboveThresholdSince) >= SOUND_DURATION_MS) {
            // Sustained long enough — fire if cooldown has passed
            if ((now - state.alerts.lastSoundAlertMs) >= ALERT_COOLDOWN_MS) {
                state.sensor.soundTriggering = false;
                return true;
            }
            // Cooldown active — reset so we don't spin here every cycle
            state.sensor.soundTriggering = false;
        }
    } else {
        // Dropped below threshold — reset gate
        state.sensor.soundTriggering = false;
    }
    return false;
}
