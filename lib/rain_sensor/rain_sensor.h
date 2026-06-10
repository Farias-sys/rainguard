#pragma once
#include <stdint.h>

// Power-gated resistive rain sensor. The module's raw ADC drops as it gets
// wetter, so the caller passes rawDry > rawWet. intensity() is 0..100 (higher =
// wetter); isWet() applies a hysteresis band between dryThreshold and
// wetThreshold. The mapping/hysteresis math lives in
// lib/rain_logic (pure, host-testable).
class RainSensor {
public:
  void begin(uint8_t aoPin, uint8_t pwrPin,
             uint16_t rawDry, uint16_t rawWet,
             uint8_t wetThreshold, uint8_t dryThreshold,
             uint8_t avgSamples);
  void setThresholds(uint8_t wetThreshold, uint8_t dryThreshold);  // runtime sensitivity (V7)
  void    powerOn();
  void    powerOff();
  uint8_t sample();                 // averaged read; updates intensity + wet flag
  uint8_t  intensity() const { return _intensity; }
  bool     isWet() const { return _wet; }
  uint16_t lastRaw() const { return _lastRaw; }   // last averaged raw ADC (for calibration)

private:
  uint8_t  _ao = 0, _pwr = 0;
  uint16_t _rawDry = 3500, _rawWet = 1500;
  uint8_t  _wetTh = 40, _dryTh = 30, _avg = 1;
  uint16_t _lastRaw = 0;
  uint8_t  _intensity = 0;
  bool     _wet = false;
};
