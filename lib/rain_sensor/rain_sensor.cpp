#include <Arduino.h>
#include "rain_sensor.h"
#include "rain_logic.h"

void RainSensor::begin(uint8_t aoPin, uint8_t pwrPin,
                       uint16_t rawDry, uint16_t rawWet,
                       uint8_t wetThreshold, uint8_t dryThreshold,
                       uint8_t avgSamples) {
  _ao = aoPin;
  _pwr = pwrPin;
  _rawDry = rawDry;
  _rawWet = rawWet;
  setThresholds(wetThreshold, dryThreshold);
  _avg = avgSamples ? avgSamples : 1;

  pinMode(_pwr, OUTPUT);
  digitalWrite(_pwr, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(_ao, ADC_11db);   // ~0..3.3 V full scale on GPIO34 (ADC1)
}

void RainSensor::setThresholds(uint8_t wetThreshold, uint8_t dryThreshold) {
  // hysteresis only works with dry < wet; repair rather than reject so a bad
  // pair can never wedge the sensor in an oscillating state
  if (wetThreshold > 100) wetThreshold = 100;
  if (wetThreshold < 1)   wetThreshold = 1;
  if (dryThreshold >= wetThreshold) dryThreshold = wetThreshold - 1;
  _wetTh = wetThreshold;
  _dryTh = dryThreshold;
}

void RainSensor::powerOn()  { digitalWrite(_pwr, HIGH); }
void RainSensor::powerOff() { digitalWrite(_pwr, LOW); }

uint8_t RainSensor::sample() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < _avg; i++) sum += analogRead(_ao);
  uint16_t raw = (uint16_t)(sum / _avg);
  _lastRaw = raw;

  _intensity = rain_logic::rawToIntensity(raw, _rawDry, _rawWet);
  _wet = rain_logic::hysteresisStep(_intensity, _wetTh, _dryTh, _wet);
  return _intensity;
}
