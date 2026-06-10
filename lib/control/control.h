#pragma once
#include <stdint.h>

// Local cover-control state machine. Deliberately free of Arduino/Blynk includes
// so it keeps working with the cloud down and can be unit-tested on the host.
// Enum values are chosen to line up with the Blynk virtual-pin values.

namespace control {

enum Mode      { MODE_AUTO = 0, MODE_MANUAL = 1 };
enum Cover     { COVER_EXPOSED = 0, COVER_PROTECTED = 1 };
enum RainState { RAIN_DRY = 0, RAIN_WET = 1 };

struct Config {
  uint32_t debounceMs;   // wet must persist this long before closing (AUTO)
  uint32_t dryDelayMs;   // dry must persist this long before reopening (AUTO)
};

struct Inputs {
  RainState rain;        // hysteresis-resolved by the sensor; simulate-forced to WET by the caller
  Mode      mode;
  int8_t    manualCmd;   // -1 = none, 0 = open (EXPOSED), 1 = close (PROTECTED)
  uint32_t  nowMs;
};

struct State {
  Mode     mode;
  Cover    cover;
  uint32_t wetSinceMs;
  uint32_t drySinceMs;
  bool     wetStreak;
  bool     dryStreak;
  bool     coverChanged; // out: true only on the tick where the cover actually moves
};

void  init(State& s);                                          // AUTO, EXPOSED
Cover update(State& s, const Inputs& in, const Config& cfg);

} // namespace control
