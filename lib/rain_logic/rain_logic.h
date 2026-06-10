#pragma once
#include <stdint.h>

// Plain sensor math: raw->intensity mapping, wet/dry hysteresis, and the
// sensitivity-level->threshold curve. No Arduino headers here, otherwise the
// host-side native build won't compile.

namespace rain_logic {

struct Thresholds { uint8_t wet; uint8_t dry; };

struct SensCurve {
  uint8_t anchorWet;    // wet threshold at anchorLevel (config.h WET_THRESHOLD)
  uint8_t anchorLevel;  // level that maps to anchorWet (SENS_LEVEL_DEFAULT)
  uint8_t step;         // wet-threshold shift per level (SENS_STEP)
  uint8_t band;         // wet - dry hysteresis gap
  uint8_t levelMin;
  uint8_t levelMax;
};

// Higher level = more sensitive = lower wet threshold. Level is clamped into
// [levelMin, levelMax], so a corrupt stored level can never produce an
// inverted pair: dry < wet holds for any input.
Thresholds levelToThresholds(uint8_t level, const SensCurve& c);

// rawDry > rawWet on these modules (raw drops as it gets wetter); result 0..100.
uint8_t rawToIntensity(uint16_t raw, uint16_t rawDry, uint16_t rawWet);

// One hysteresis evaluation: >= wetTh => wet, <= dryTh => dry, in between
// holds prevWet.
bool hysteresisStep(uint8_t intensity, uint8_t wetTh, uint8_t dryTh, bool prevWet);

} // namespace rain_logic
