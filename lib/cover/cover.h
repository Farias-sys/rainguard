#pragma once
#include <stdint.h>
#include <ESP32Servo.h>

// MG90S cover servo. Non-blocking: moveTo() attaches and writes the target
// angle; update() detaches the servo a short time after the last move to stop
// buzz and current draw. Call update() every loop().
class Cover {
public:
  void begin(uint8_t pin, int minUs, int maxUs, int hz,
             int angleExposed, int angleProtected, uint32_t detachMs);
  void moveTo(bool protectedPosition);
  void update();
  bool isProtected() const { return _protected; }

private:
  Servo    _servo;
  uint8_t  _pin = 0;
  int      _minUs = 500, _maxUs = 2400, _hz = 50;
  int      _angleExposed = 0, _angleProtected = 90;
  uint32_t _detachMs = 700, _lastMoveMs = 0;
  bool     _protected = false, _attached = false, _moved = false;
};
