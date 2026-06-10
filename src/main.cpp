// RainGuard - ESP32 automatic clothes-rain cover (Blynk.Edgent firmware).
//
// Blynk.Edgent needs its template config #defined BEFORE BlynkEdgent.h.
// Virtual datastreams: V0 intensity,
// V1 rainState, V2 cover, V3 mode, V4 manualCommand, V5 lastEvent,
// V6 simulateRain, V7 sensitivity, V8 dryDelayMin, V9 reopenCountdownSec,
// V10 coverControl. V7/V8 are cloud-truth (synced on connect); the rest are
// device-truth (pushed, never synced).

#include "secrets.h"               // BLYNK_TEMPLATE_ID, BLYNK_TEMPLATE_NAME (gitignored)
#include "config.h"

#define BLYNK_FIRMWARE_VERSION  "0.2.0"   // bump to ship a Blynk.Air OTA update
#define USE_ESP32_DEV_MODULE              // Edgent board profile: BOOT button on GPIO0
#define BOARD_LED_PIN           PIN_LED   // single source of truth (config.h)
#define BOARD_LED_INVERSE       false
#define BOARD_LED_BRIGHTNESS    64
// BLYNK_PRINT is set via platformio.ini build_flags.

#include <Arduino.h>
#include "BlynkEdgent.h"

#include <Preferences.h>

#include "control.h"
#include "rain_sensor.h"
#include "rain_logic.h"
#include "cover.h"

static control::State  g_ctrl;
static control::Config g_ctrlCfg = { DEBOUNCE_MS, DRY_DELAY_MS };
static RainSensor      g_rain;
static Cover           g_cover;
static BlynkTimer      g_timer;

static int      g_mode       = control::MODE_AUTO;   // mirrors V3
static int8_t   g_manualCmd  = -1;                   // mirrors V4 (latched)
static bool     g_simulate   = false;                // mirrors V6
static uint32_t g_lastNotify = 0;
static String   g_lastEvent  = "Sistema iniciado";

// Dashboard-tunable settings (V7/V8). NVS namespace "rainguard" is separate
// from Edgent's "blynk", so these survive re-provisioning and still work
// offline across reboots; on reconnect the cloud value wins via syncVirtual.
// The slider can fire many writes per drag, so a debounced committer folds
// them into one flash write. Settings.h is already used by the vendored
// Edgent header, so this stays inline.
static uint8_t  g_sensLevel         = SENS_LEVEL_DEFAULT;       // mirrors V7
static uint8_t  g_dryDelayMin       = DRY_DELAY_MS / 60000UL;   // mirrors V8
static bool     g_settingsDirty     = false;
static uint32_t g_settingsChangedMs = 0;
static int      g_lastCountdownSec  = -1;                       // V9 change gate

static void applySensitivity() {
  rain_logic::SensCurve c = { WET_THRESHOLD, SENS_LEVEL_DEFAULT, SENS_STEP,
                              WET_THRESHOLD - DRY_THRESHOLD,
                              SENS_LEVEL_MIN, SENS_LEVEL_MAX };
  rain_logic::Thresholds t = rain_logic::levelToThresholds(g_sensLevel, c);
  g_rain.setThresholds(t.wet, t.dry);
}

static void applyDryDelay() {
  g_ctrlCfg.dryDelayMs = (uint32_t)g_dryDelayMin * 60000UL;
}

static void loadSettings() {
  Preferences p;
  if (p.begin("rainguard", true)) {   // read-only; begin() fails on first boot (no namespace yet) and the config.h defaults stand
    g_sensLevel   = p.getUChar("sensLvl", SENS_LEVEL_DEFAULT);
    g_dryDelayMin = p.getUChar("dryMin", DRY_DELAY_MS / 60000UL);
    p.end();
  }
  if (g_sensLevel < SENS_LEVEL_MIN) g_sensLevel = SENS_LEVEL_MIN;
  if (g_sensLevel > SENS_LEVEL_MAX) g_sensLevel = SENS_LEVEL_MAX;
  if (g_dryDelayMin > DRY_DELAY_MIN_MAX) g_dryDelayMin = DRY_DELAY_MIN_MAX;
  applySensitivity();
  applyDryDelay();
}

static void settingsChanged() {
  g_settingsDirty = true;
  g_settingsChangedMs = millis();
}

static void commitSettingsIfDue() {
  if (!g_settingsDirty) return;
  if ((uint32_t)(millis() - g_settingsChangedMs) < SETTINGS_COMMIT_DEBOUNCE_MS) return;
  Preferences p;
  if (p.begin("rainguard", false)) {
    p.putUChar("sensLvl", g_sensLevel);
    p.putUChar("dryMin", g_dryDelayMin);
    p.end();
    g_settingsDirty = false;   // kept set on begin() failure -> retried next tick
  }
}

// Effective rain drives the FSM and is what the dashboard reports, so the
// control input, V0 and V1 always agree (simulate forces wet/100).
static uint8_t effectiveIntensity() { return g_simulate ? 100 : g_rain.intensity(); }
static bool    effectiveWet()       { return g_simulate || g_rain.isWet(); }

static void pushDashboard(bool force) {     // sole publisher of V0/V1/V2 (+V10 mirror)
  static int lastI = -1, lastW = -1, lastC = -1;
  int i = effectiveIntensity();
  int w = effectiveWet() ? 1 : 0;
  int c = g_cover.isProtected() ? 1 : 0;
  if (force || i != lastI) { Blynk.virtualWrite(V0, i); lastI = i; }
  if (force || w != lastW) { Blynk.virtualWrite(V1, w); lastW = w; }
  if (force || c != lastC) {
    Blynk.virtualWrite(V2, c);
    Blynk.virtualWrite(V10, c);              // mirror so the one-tap switch tracks reality
    lastC = c;
  }
}

static void pushReopenCountdown() {          // V9: nonzero only while an AUTO reopen is pending
  int sec = 0;
  if (g_mode == control::MODE_AUTO && g_ctrl.cover == control::COVER_PROTECTED && g_ctrl.dryStreak) {
    uint32_t elapsed = millis() - g_ctrl.drySinceMs;
    if (elapsed < g_ctrlCfg.dryDelayMs) sec = (int)((g_ctrlCfg.dryDelayMs - elapsed) / 1000UL);
  }
  if (sec != g_lastCountdownSec) { Blynk.virtualWrite(V9, sec); g_lastCountdownSec = sec; }
}

static void onCoverChanged(bool nowProtected) {
  char buf[48];
  snprintf(buf, sizeof(buf), "Cobertura %s @ %lus",
           nowProtected ? "fechada" : "aberta",
           (unsigned long)(millis() / 1000));
  g_lastEvent = buf;
  Blynk.virtualWrite(V5, g_lastEvent);

  if (g_mode == control::MODE_AUTO && nowProtected) {        // self rate-limited
    uint32_t now = millis();
    if (g_lastNotify == 0 || (uint32_t)(now - g_lastNotify) >= NOTIFY_MIN_INTERVAL_MS) {
      Blynk.logEvent("rain_detected", "Chuva detectada - cobertura fechada");
      g_lastNotify = now;
    }
  }
}

static void applyControl() {
  control::Inputs in;
  in.mode      = (control::Mode)g_mode;
  in.manualCmd = (g_mode == control::MODE_MANUAL) ? g_manualCmd : (int8_t)-1;
  in.nowMs     = millis();
  in.rain      = effectiveWet() ? control::RAIN_WET : control::RAIN_DRY;

  control::Cover target = control::update(g_ctrl, in, g_ctrlCfg);
  if (g_ctrl.coverChanged) {
    bool protectedPos = (target == control::COVER_PROTECTED);
    g_cover.moveTo(protectedPos);
    onCoverChanged(protectedPos);
  }
}

static void readSensorThenControl() {
  g_rain.sample();
#if RAIN_CALIBRATE
  // keep the sensor powered between reads so AO/VCC can be probed with a meter
  Serial.printf("raw=%u intensity=%u wet=%d\n",
                g_rain.lastRaw(), g_rain.intensity(), g_rain.isWet() ? 1 : 0);
#else
  g_rain.powerOff();
#endif
  applyControl();
  pushDashboard(false);
}

static void beginSensorCycle() {
  g_rain.powerOn();
  g_timer.setTimeout(SENSOR_SETTLE_MS, readSensorThenControl);  // settle without blocking loop()
}

BLYNK_WRITE(V3) {                            // mode (0 = auto, 1 = manual)
  int newMode = param.asInt() ? control::MODE_MANUAL : control::MODE_AUTO;
  if (newMode == control::MODE_MANUAL && g_mode != control::MODE_MANUAL) {
    // adopt the physical position so MANUAL never starts on the -1 latch
    // (which would hold the cover and look unresponsive until a command)
    g_manualCmd = g_cover.isProtected() ? 1 : 0;
    Blynk.virtualWrite(V4, g_manualCmd);
  }
  g_mode = newMode;
  applyControl();
}

BLYNK_WRITE(V4) {                            // manual command (0 = open, 1 = close); forces MANUAL
  g_manualCmd = param.asInt() ? 1 : 0;
  if (g_mode != control::MODE_MANUAL) {      // was silently inert in AUTO
    g_mode = control::MODE_MANUAL;
    Blynk.virtualWrite(V3, g_mode);
  }
  applyControl();
}

BLYNK_WRITE(V6) {                            // simulate rain (0/1)
  g_simulate = param.asInt();
  applyControl();
  pushDashboard(true);                       // reflect forced intensity/state immediately
}

BLYNK_WRITE(V7) {                            // sensitivity level 1..5 (cloud-truth)
  int v = param.asInt();
  uint8_t lvl = (uint8_t)((v < SENS_LEVEL_MIN) ? SENS_LEVEL_MIN
                        : (v > SENS_LEVEL_MAX) ? SENS_LEVEL_MAX : v);
  if (lvl == g_sensLevel) return;            // syncVirtual replays must not dirty NVS
  g_sensLevel = lvl;
  applySensitivity();                        // wet/dry re-evaluates on the next 1 s sample
  settingsChanged();
}

BLYNK_WRITE(V8) {                            // dry-delay minutes 0..30 (cloud-truth)
  int v = param.asInt();
  uint8_t mins = (uint8_t)((v < 0) ? 0 : (v > DRY_DELAY_MIN_MAX) ? DRY_DELAY_MIN_MAX : v);
  if (mins == g_dryDelayMin) return;
  g_dryDelayMin = mins;
  applyDryDelay();
  settingsChanged();
  applyControl();                            // shortening below the elapsed wait reopens now
}

BLYNK_WRITE(V10) {                           // one-tap cover control; forces MANUAL
  g_manualCmd = param.asInt() ? 1 : 0;
  if (g_mode != control::MODE_MANUAL) {
    g_mode = control::MODE_MANUAL;
    Blynk.virtualWrite(V3, g_mode);
  }
  Blynk.virtualWrite(V4, g_manualCmd);
  applyControl();
}

BLYNK_CONNECTED() {
  Blynk.virtualWrite(V3, g_mode);            // reflect device state (AUTO after reboot)
  Blynk.virtualWrite(V6, g_simulate ? 1 : 0);
  Blynk.virtualWrite(V5, g_lastEvent);
  if (g_mode == control::MODE_MANUAL && g_manualCmd >= 0) {
    Blynk.virtualWrite(V4, g_manualCmd);     // only a concrete cmd; the -1 latch has nothing to say
  }
  g_lastCountdownSec = -1;                   // force a V9 refresh on the next countdown tick
  Blynk.syncVirtual(V7, V8);                 // cloud-truth settings: app edits win on reconnect
  pushDashboard(true);                       // never stale on reconnect (incl. V10 mirror)
}

void setup() {
  Serial.begin(115200);
  delay(100);

  g_rain.begin(PIN_RAIN_AO, PIN_RAIN_PWR, RAIN_RAW_DRY, RAIN_RAW_WET,
               WET_THRESHOLD, DRY_THRESHOLD, SENSOR_AVG_SAMPLES);
  g_cover.begin(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US, SERVO_HZ,
                ANGLE_EXPOSED, ANGLE_PROTECTED, SERVO_DETACH_MS);
  control::init(g_ctrl);
  loadSettings();                            // NVS over config.h defaults (local-first)

  BlynkEdgent.begin();

  // Establish a known EXPOSED position, then detach before provisioning may
  // block loop() (Cover::update() can't run during Edgent config mode).
  g_cover.moveTo(false);
  delay(SERVO_DETACH_MS);
  g_cover.update();

  g_timer.setInterval(READ_INTERVAL_MS, beginSensorCycle);
  g_timer.setInterval(HEARTBEAT_MS, []() { pushDashboard(true); });
  g_timer.setInterval(SETTINGS_TICK_MS, commitSettingsIfDue);
  g_timer.setInterval(COUNTDOWN_PUSH_MS, pushReopenCountdown);
}

void loop() {
  BlynkEdgent.run();
  g_timer.run();
  g_cover.update();
}
