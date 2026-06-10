#pragma once

// All tunables live here. Calibration values must be measured on the bench.

// --- Pin map ---
#define PIN_SERVO      13   // servo PWM (GPIO13)
#define PIN_RAIN_AO    34   // rain analog in (GPIO34, ADC1, input-only)
#define PIN_RAIN_PWR   25   // powers the sensor during a read (GPIO25)
#define PIN_LED         2   // onboard status LED
#define PIN_BOOT        0   // BOOT button (GPIO0); live pin is owned by Edgent's USE_ESP32_DEV_MODULE profile

// --- Servo geometry & drive ---
#define ANGLE_EXPOSED    0      // clothes exposed
#define ANGLE_PROTECTED  100     // cover over clothes
#define SERVO_MIN_US     500    // MG90S min pulse (us)
#define SERVO_MAX_US     2400   // MG90S max pulse (us)
#define SERVO_HZ         50
#define SERVO_DETACH_MS  700UL  // detach this long after a move to kill buzz/current

// --- Rain sensor calibration (bench-measured June 2026) ---
// On these modules the raw ADC drops as it gets wetter, so DRY > WET.
#define RAIN_RAW_DRY       3500  // raw ADC, fully dry
#define RAIN_RAW_WET       1500  // raw ADC, soaked
#define SENSOR_AVG_SAMPLES 5     // ADC samples averaged per read (denoise)
#define RAIN_CALIBRATE     0     // 1 = print raw/intensity over serial to measure RAIN_RAW_*

// --- Intensity thresholds (0..100, higher = wetter) with hysteresis band ---
#define WET_THRESHOLD  40   // intensity >= this => wet
#define DRY_THRESHOLD  30   // intensity <= this => dry; the band between them holds the previous state

// --- Sensitivity levels (V7 dashboard slider) ---
// rain_logic::levelToThresholds: wetTh = WET_THRESHOLD + (SENS_LEVEL_DEFAULT - level) * SENS_STEP,
// dryTh = wetTh - (WET_THRESHOLD - DRY_THRESHOLD). Level 3 == the bench values above.
#define SENS_LEVEL_MIN     1
#define SENS_LEVEL_MAX     5
#define SENS_LEVEL_DEFAULT 3
#define SENS_STEP          10

// --- Dashboard-tunable settings (live values persist in NVS namespace "rainguard") ---
#define DRY_DELAY_MIN_MAX           30      // V8 slider upper bound (minutes)
#define SETTINGS_COMMIT_DEBOUNCE_MS 1500UL  // slider must settle this long before the NVS commit
#define SETTINGS_TICK_MS            250UL   // committer poll period
#define COUNTDOWN_PUSH_MS           5000UL  // V9 reopen-countdown push period

// --- Timings (ms) ---
#define DEBOUNCE_MS            2000UL    // wet must persist before closing
#define DRY_DELAY_MS           300000UL  // dry must persist before auto-reopen (5 min)
#define READ_INTERVAL_MS       1000UL    // sensor sample period
#define HEARTBEAT_MS           60000UL   // periodic dashboard push
#define SENSOR_SETTLE_MS       10UL      // settle after powering the sensor before reading
#define NOTIFY_MIN_INTERVAL_MS 60000UL   // min spacing between rain notifications

// DRY_DELAY_MS doubles as the V8 default (in minutes); it is stored in a uint8
// with no clamp on the static-init path, so keep it inside the slider range.
static_assert(DRY_DELAY_MS / 60000UL <= DRY_DELAY_MIN_MAX,
              "V8 default (DRY_DELAY_MS in minutes) exceeds DRY_DELAY_MIN_MAX");
