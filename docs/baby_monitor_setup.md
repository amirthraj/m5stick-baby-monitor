# Baby Monitor — M5StickC Plus 2 Setup Guide

## What This Does

The M5StickC Plus 2 acts as a baby monitor that:
- Detects **sound** (crying) via the built-in microphone
- Detects **motion** via the M5Stack PIR sensor (AS312)
- Sends **BLE notifications** to a nearby phone/device
- Sends **MQTT messages** over WiFi (works through walls, remotely)

Alerts show on the M5Stick display and are published simultaneously over both BLE and MQTT.

---

## Hardware Required

| Item | Where |
|------|-------|
| M5StickC Plus 2 | You have this |
| M5Stack PIR Motion Sensor (AS312) | berrybase.de — €6.65 |
| GROVE cable (included with PIR) | Comes with the PIR unit |

### Wiring

Connect the PIR sensor to the M5StickC Plus 2 using the GROVE cable:

```
PIR AS312 GROVE port → M5StickC Plus 2 GROVE port
        (plug and play — just connect the cable)
```

The GROVE port on the M5StickC Plus 2 uses:
- **GPIO 32** — PIR signal output (data)
- **GPIO 33** — unused (SCL, not needed for PIR)
- **5V** — powers the PIR
- **GND** — ground

No soldering required. The GROVE connector is keyed so it only fits one way.

---

## Arduino IDE Setup

### Step 1 — Install Arduino IDE

Download from: https://www.arduino.cc/en/software  
Version 2.x recommended (has better library manager).

### Step 2 — Add Espressif Board Package

We use the official Espressif ESP32 package (hosted on GitHub) rather than the M5Stack URL, which is hosted in China and flagged by some antivirus tools.

1. Open Arduino IDE
2. Go to **File → Preferences** (Windows/Linux) or **Arduino IDE → Settings** (Mac)
3. Find the field **"Additional boards manager URLs"**
4. Paste this URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
5. Click **OK**

### Step 3 — Install ESP32 Boards

1. Go to **Tools → Board → Boards Manager**
2. Search for `esp32`
3. Install **"esp32 by Espressif Systems"** (latest version)
4. Wait for download to finish (~200MB, includes the ESP32 toolchain)

### Step 4 — Select Your Board

1. Go to **Tools → Board → esp32**
2. Select **"ESP32 PICO-D4"** (closest match to the M5StickC Plus 2 chip)

Your Tools menu should now show:
```
Board:      ESP32 PICO-D4
Upload Speed: 921600
Port:       (see Step 6)
```

### Step 5 — Install Required Libraries

Go to **Tools → Manage Libraries** (or Sketch → Include Library → Manage Libraries)

Install these one by one (search by name):

| Library | Author | Purpose |
|---------|--------|---------|
| `M5Unified` | M5Stack | M5StickC Plus 2 hardware support |
| `PubSubClient` | Nick O'Leary | MQTT client |
| `WiFiManager` | tzapu | WiFi setup portal (no password in code) |

When installing **M5Unified**, Arduino IDE 2.x will prompt "Install missing dependencies?" — click **Install All** to also pull in M5GFX automatically.

> **Note:** The BLE library is already included with the ESP32 board package — no separate install needed.

### Step 6 — Connect & Find Port

1. Connect M5StickC Plus 2 to your PC via USB-C
2. Go to **Tools → Port**
3. Select the port that appears (usually `COM3`, `COM4`, etc. on Windows — `/dev/cu.usbserial-...` on Mac)

If no port appears:
- **Windows:** The M5StickC Plus 2 uses a **CH9102F** USB chip. Install the driver from the manufacturer's site:  
  https://www.wch-ic.com/downloads/CH343SER_EXE.html  
  After installing, unplug and replug the device. It should appear as **"USB-Enhanced-SERIAL CH9102 (COMx)"** in Device Manager.
- **Mac:** Usually works without drivers on macOS 12+
- **Cable check:** Some USB-C cables are charge-only and carry no data. If the port still doesn't appear after installing the driver, try a different cable.

---

## Code Configuration

**WiFi credentials are not stored in the code.** They are entered via a setup portal on first boot (see First Boot below). The only things to configure in `baby_monitor.ino` are:

```cpp
// ── MQTT Broker ───────────────────────────
// Using HiveMQ free public broker (no signup needed)
// For private: set up Mosquitto on a Raspberry Pi or use HiveMQ Cloud
const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "babymonitor/alerts";  // change to something unique

// ── Detection Thresholds ──────────────────
// Sound: increase if too many false alarms, decrease if missing real cries
#define SOUND_THRESHOLD  1500

// ── PIR sensor ────────────────────────────
// Change to true once the AS312 sensor arrives and is plugged in
#define PIR_ENABLED false
```

---

## First Boot — WiFi Setup

On first power-on (or after a credential reset), the M5Stick creates a temporary hotspot:

1. The display shows **"Connect to: BabyMonitor-Setup"**
2. On your phone, connect to the WiFi network named **`BabyMonitor-Setup`**
3. Open a browser and go to **`192.168.4.1`**
4. Select your home WiFi network and enter the password
5. The M5Stick saves the credentials to internal flash and connects

From now on it connects automatically on every boot — no portal needed.

**To change WiFi network later:** hold the side button while powering on. This wipes the saved credentials and shows the portal again.

---

## Uploading the Code

1. Open `baby_monitor.ino` in Arduino IDE
2. Select the correct board (**ESP32 PICO-D4**) and port
3. Click the **→ Upload** button (or press Ctrl+U / Cmd+U)
4. Wait for "Compiling sketch..." then "Uploading..."
5. The M5Stick will restart and enter the WiFi setup portal (first boot only)

**If upload fails:**
- Hold the side button on M5Stick while connecting USB (puts it in boot mode)
- Try a different USB-C cable (some are charge-only)
- Lower upload speed: Tools → Upload Speed → 115200

---

## Receiving Alerts

### BLE (Bluetooth, nearby)

Use **nRF Connect** app (Android/iOS — free):
1. Open nRF Connect → Scanner tab
2. Find device named **"BabyMonitor"**
3. Connect → find the service → enable notifications on the characteristic
4. You'll receive JSON like `{"type":"sound","ts":12345}`

On **Android with Chrome**, you can also use the companion PWA (Web Bluetooth).

### MQTT (WiFi, anywhere)

**Quick test — MQTT Explorer** (desktop app, free):
1. Download from https://mqtt-explorer.com
2. Connect to `broker.hivemq.com` port `1883`
3. Subscribe to `babymonitor/alerts`
4. Alerts appear as JSON messages in real time

**On your phone — MQTT Dash** (Android) or **MQTTool** (iOS):
- Broker: `broker.hivemq.com`
- Port: `1883`
- Topic: `babymonitor/alerts`

**Important:** The public HiveMQ broker is shared — anyone could theoretically subscribe to your topic. For a private setup, change the topic name to something unique (e.g., `babymon-xk7r29/alerts`) or run a local Mosquitto broker on a Raspberry Pi.

---

## Display Indicators

| Screen | Meaning |
|--------|---------|
| "Connect to: BabyMonitor-Setup" | First boot WiFi setup portal active |
| Dark green "Ready!" | Setup done, monitoring started |
| Status screen (auto-dims after 30s) | BLE / WiFi / MQTT status + mic bar |
| Dark red "CRYING!" | Sound alert triggered |
| Dark amber "MOTION!" | PIR alert triggered (when sensor connected) |
| PIR: not fitted | PIR disabled — sensor not yet connected |
| Screen off | Auto-sleep after 30s — press side button to wake |

---

## Tuning Tips

**Too many false sound alerts?**
- Increase `SOUND_THRESHOLD` (try 2000, 3000)
- Increase `SOUND_DURATION_MS` (requires more sustained sound before alerting)

**Missing real cries?**
- Decrease `SOUND_THRESHOLD` (try 1000, 800)

**PIR firing too often?**
- The AS312 has a small adjustable potentiometer on the board for sensitivity
- Also adjust the detection cone by repositioning the sensor

**Alert cooldown:**
- By default, alerts don't repeat within 15 seconds
- Change `ALERT_COOLDOWN_MS` in the code

---

## Architecture Overview

```
┌─────────────────────────────┐
│     M5StickC Plus 2         │
│                             │
│  [Mic] → sound level check  │──── BLE notify ──→ Phone (nRF/PWA)
│  [PIR] → motion HIGH/LOW    │
│  [IMU] → (optional, unused) │──── MQTT publish → broker.hivemq.com
│                             │                         │
│  [Display] → status         │                    any MQTT client
└─────────────────────────────┘                   (phone, PC, etc.)
         │
      GROVE
         │
   PIR AS312 sensor
```
