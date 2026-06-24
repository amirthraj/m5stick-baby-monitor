# M5Stick Baby Monitor

A wireless baby monitor built on the **M5StickC Plus 2** (ESP32) that detects crying and movement, then sends alerts over **Bluetooth Low Energy (BLE)** and **MQTT over WiFi**.

---

## Features

- Sound detection via built-in SPM1423 microphone
- Motion detection via M5Stack PIR AS312 sensor (GROVE port) — plug in when it arrives
- BLE notifications — receive alerts on your phone with the nRF Connect app
- MQTT alerts — subscribe from anywhere with any MQTT client
- WiFi setup via captive portal — no passwords stored in code
- Very dim display mode (brightness 20/255) for dark baby rooms
- Auto-sleep display after 30 seconds, wake with side button
- 15-second alert cooldown to prevent notification spam

---

## Hardware

| Part | Details |
|------|---------|
| M5StickC Plus 2 | ESP32-PICO-V3-02, built-in mic + IMU |
| M5Stack PIR AS312 | Motion sensor, GROVE port, ~EUR 6.65 |
| GROVE cable | Included with PIR unit |

Wiring: plug the PIR into the GROVE port on the M5StickC Plus 2 — no soldering needed.

---

## Repository Structure

```
firmware/
  baby_monitor/
    baby_monitor.ino    Arduino sketch
docs/
  setup.md              Full setup and flashing guide
.gitignore
README.md
```

---

## Quick Start

See **[docs/setup.md](docs/setup.md)** for the full guide, including:

- Arduino IDE installation and board package setup
- Library installation (M5Unified, PubSubClient, WiFiManager)
- Windows USB driver for the CH9102F chip
- First-boot WiFi portal walkthrough
- Compiling and flashing the sketch
- Receiving BLE and MQTT alerts

---

## Alert Payload

Alerts are sent as JSON over both BLE notify and MQTT publish:

```json
{"type":"sound","device":"m5stick-babymon-001","ts":12345}
{"type":"motion","device":"m5stick-babymon-001","ts":12345}
```

---

## MQTT

| Setting | Value |
|---------|-------|
| Broker | `broker.hivemq.com` |
| Port | `1883` |
| Topic | `babymonitor-k9m2xw47/alerts` |

Test with [MQTT Explorer](https://mqtt-explorer.com) (desktop) or MQTT Dash / MQTTool on mobile.

---

## BLE

Device name: **BabyMonitor**

Connect with [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile) (Android/iOS) and enable notifications on the characteristic.

---

## When the PIR Sensor Arrives

In `baby_monitor.ino`, change:

```cpp
#define PIR_ENABLED false
```
to:
```cpp
#define PIR_ENABLED true
```

Then re-upload the sketch.

---

## License

MIT
