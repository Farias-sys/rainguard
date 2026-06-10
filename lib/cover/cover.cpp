#include <Arduino.h>
#include "cover.h"

void Cover::begin(uint8_t pin, int minUs, int maxUs, int hz,
                  int angleExposed, int angleProtected, uint32_t detachMs) {
  _pin = pin;
  _minUs = minUs;
  _maxUs = maxUs;
  _hz = hz;
  _angleExposed = angleExposed;
  _angleProtected = angleProtected;
  _detachMs = detachMs;

  ESP32PWM::allocateTimer(1);   // keep the servo off LEDC timer 0 (Edgent status LED uses it)
  _servo.setPeriodHertz(_hz);   // attach lazily on first moveTo()
}

void Cover::moveTo(bool protectedPosition) {
  if (!_attached) {
    _servo.attach(_pin, _minUs, _maxUs);
    _attached = true;
  }
  _servo.write(protectedPosition ? _angleProtected : _angleExposed);
  _protected = protectedPosition;
  _lastMoveMs = millis();
  _moved = true;
}

void Cover::update() {
  if (_moved && _attached &&
      (uint32_t)(millis() - _lastMoveMs) >= _detachMs) {
    _servo.detach();
    _attached = false;
    _moved = false;
  }
}
