#pragma once

#include "bars_types.h"

#define BARS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BARS_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline int animated_ratio(const Series *series,
                                 int animation_progress) {
  if (series->maximum <= 0) {
    return 0;
  }
  int ratio = BARS_MAX(
      0, BARS_MIN(1000, (int)((int64_t)series->value * 1000 /
                              series->maximum)));
  return ratio * animation_progress / 1000;
}
