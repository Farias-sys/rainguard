#include "rain_logic.h"

namespace rain_logic {

Thresholds levelToThresholds(uint8_t level, const SensCurve& c) {
  uint8_t lvl = level;
  if (lvl < c.levelMin) lvl = c.levelMin;
  if (lvl > c.levelMax) lvl = c.levelMax;

  int band = (c.band > 0) ? c.band : 1;
  int wet  = (int)c.anchorWet + ((int)c.anchorLevel - (int)lvl) * (int)c.step;
  if (wet > 100)  wet = 100;
  if (wet < band) wet = band;   // keeps dry >= 0 and dry < wet

  Thresholds t;
  t.wet = (uint8_t)wet;
  t.dry = (uint8_t)(wet - band);
  return t;
}

uint8_t rawToIntensity(uint16_t raw, uint16_t rawDry, uint16_t rawWet) {
  long den = (long)rawWet - (long)rawDry;
  if (den == 0) return 0;
  long mapped = ((long)raw - (long)rawDry) * 100L / den;
  if (mapped < 0)   mapped = 0;
  if (mapped > 100) mapped = 100;
  return (uint8_t)mapped;
}

bool hysteresisStep(uint8_t intensity, uint8_t wetTh, uint8_t dryTh, bool prevWet) {
  if (intensity >= wetTh) return true;
  if (intensity <= dryTh) return false;
  return prevWet;
}

} // namespace rain_logic
