# RainGuard

ESP32 firmware that automatically pulls a cover over hanging clothes when rain is detected, and reopens it once they are dry. Includes an IoT dashboard (Blynk) for live status, remote control, and notifications.

[Leia em Português](README.pt-BR.md)

![Platform](https://img.shields.io/badge/platform-ESP32%20DOIT%20DevKit%20v1-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%2B%20PlatformIO-orange)
![Cloud](https://img.shields.io/badge/IoT-Blynk-23C48E)

## How it works

The rain sensor is sampled once per second. The raw ADC reading is mapped to an intensity scale of 0–100 with a hysteresis band (wet ≥ 40, dry ≤ 30 by default), so the state doesn't flicker around the threshold. A small state machine then decides the cover position: rain must persist for 2 s before closing (debounce), and dryness must persist for 5 min before reopening (dry delay, adjustable from the dashboard). An MG90S servo swings the cover between 0° (exposed) and 90° (protected) and is detached after each move to eliminate buzz and idle current. The sensor plate is only powered during reads, which slows electrolytic corrosion.

The design is **local-first**: sensing and cover control run entirely on-device, so the system keeps protecting the clothes with no WiFi or cloud. Blynk adds visibility, remote override, and notifications on top.

## Hardware

| Part | Role |
|---|---|
| ESP32 DOIT DevKit v1 | controller |
| MG90S micro servo | moves the cover |
| Rain sensor module (FC-37 / YL-83 type) | resistive plate, analog output |

Wiring (single source of truth: [`include/config.h`](include/config.h)):

| Signal | GPIO |
|---|---|
| Servo PWM | 13 |
| Rain sensor AO | 34 (ADC1, input-only) |
| Rain sensor VCC (power gate) | 25 |
| Status LED | 2 (onboard) |
| BOOT button (re-provisioning) | 0 |

Power the servo from 5 V (VIN/USB) with its ground common to the ESP32.

## Repository layout

```
platformio.ini          board, libraries, build flags
include/
  config.h              every pin, threshold, timing and calibration value
  secrets.example.h     template for the Blynk IDs (copy to secrets.h)
lib/
  control/              pure C++ state machine: auto/manual, debounce, dry delay
  rain_logic/           pure C++ sensor math: raw→intensity, hysteresis, sensitivity curve
  rain_sensor/          power-gated ADC reading
  cover/                servo driver, non-blocking attach/detach
src/
  main.cpp              entry point; all Blynk wiring (handlers, timers, pushes)
  *.h                   vendored Blynk.Edgent headers (provisioning, OTA, LED states)
```

`lib/control` and `lib/rain_logic` are deliberately Arduino-free, which is what makes the local-first rule testable and true. The `native` environment in `platformio.ini` exists to run their unit tests on a host PC.

## Getting started

### 1. Blynk console

1. Create a template at [console.blynk.cloud](https://console.blynk.cloud) (Developer Zone → Templates).
2. Add the datastreams V0–V10 listed below and an event named `rain_detected`.
3. For **V7** and **V8**, set the defaults (3 and 5) and enable **"sync with latest server value"** — the device pulls them on connect.

### 2. Secrets

```
cp include/secrets.example.h include/secrets.h
```

Fill in `BLYNK_TEMPLATE_ID` and `BLYNK_TEMPLATE_NAME` from the template's Home tab. `secrets.h` is gitignored. Do **not** define `BLYNK_AUTH_TOKEN`: Blynk.Edgent provisions the token over the air and the build fails if it is set.

### 3. Build and flash

With [PlatformIO](https://platformio.org/) (VS Code extension or CLI):

```
pio run                        # build
pio run -t upload -t monitor   # flash + serial monitor (115200 baud)
```

Adjust `upload_port` in `platformio.ini` for your machine (default `COM6`).

### 4. Provision WiFi

On first boot the device enters config mode (onboard LED blinks) and opens a WiFi access point. In the Blynk app, choose *Add device → Find devices nearby*; WiFi credentials and the auth token are delivered over the air. Hold **BOOT for ~10 s** to wipe provisioning and start over. Firmware updates ship over the air via Blynk.Air (bump `BLYNK_FIRMWARE_VERSION` in `src/main.cpp`).

## Dashboard datastreams

| Pin | Name | Values | Description |
|---|---|---|---|
| V0 | intensity | 0–100 | rain intensity |
| V1 | rainState | 0/1 | 1 = wet |
| V2 | cover | 0/1 | 1 = protected (closed over clothes) |
| V3 | mode | 0/1 | 0 = auto, 1 = manual |
| V4 | manualCommand | 0/1 | 0 = open, 1 = close; forces MANUAL |
| V5 | lastEvent | string | last cover event |
| V6 | simulateRain | 0/1 | demo switch, forces wet/100 |
| V7 | sensitivity | 1–5 | default 3, sync ON |
| V8 | dryDelayMin | 0–30 | minutes before auto-reopen; default 5, sync ON |
| V9 | reopenCountdownSec | seconds | countdown until auto-reopen |
| V10 | coverControl | 0/1 | one-tap open/close; forces MANUAL, mirrors V2 |

V7/V8 are cloud-truth (synced on connect); the rest are device-truth (pushed by the firmware).

## Configuration and calibration

All tunables live in [`include/config.h`](include/config.h): pins, thresholds, timings, servo geometry, calibration anchors. To recalibrate the sensor, set `RAIN_CALIBRATE 1`, flash, read the raw values dry and soaked over serial, update `RAIN_RAW_DRY` / `RAIN_RAW_WET`, and set it back to 0 (calibration mode keeps the plate powered, which corrodes it). Note the polarity: the raw ADC value **drops** as the plate gets wetter.

Sensitivity (V7) and dry delay (V8) set from the dashboard persist in NVS and survive reboots and re-provisioning.

---

College extension project — Computer Science.
