#include "control.h"

namespace control {

void init(State& s) {
  s.mode = MODE_AUTO;
  s.cover = COVER_EXPOSED;
  s.wetSinceMs = 0;
  s.drySinceMs = 0;
  s.wetStreak = false;
  s.dryStreak = false;
  s.coverChanged = false;
}

Cover update(State& s, const Inputs& in, const Config& cfg) {
  const Cover prev = s.cover;
  s.coverChanged = false;

  if (in.mode != s.mode) {            // any mode change re-arms AUTO debounce/dry-delay
    s.mode = in.mode;
    s.wetStreak = false;
    s.dryStreak = false;
    s.wetSinceMs = 0;
    s.drySinceMs = 0;
  }

  if (s.mode == MODE_MANUAL) {
    if (in.manualCmd == 0)      s.cover = COVER_EXPOSED;
    else if (in.manualCmd == 1) s.cover = COVER_PROTECTED;
    // manualCmd == -1: hold current cover
  } else { // MODE_AUTO
    if (in.rain == RAIN_WET) {
      if (!s.wetStreak) { s.wetStreak = true; s.wetSinceMs = in.nowMs; }
      s.dryStreak = false;
      if ((uint32_t)(in.nowMs - s.wetSinceMs) >= cfg.debounceMs) {
        s.cover = COVER_PROTECTED;
      }
    } else { // RAIN_DRY
      if (!s.dryStreak) { s.dryStreak = true; s.drySinceMs = in.nowMs; }
      s.wetStreak = false;
      if ((uint32_t)(in.nowMs - s.drySinceMs) >= cfg.dryDelayMs) {
        s.cover = COVER_EXPOSED;
      }
    }
  }

  s.coverChanged = (s.cover != prev);
  return s.cover;
}

} // namespace control
