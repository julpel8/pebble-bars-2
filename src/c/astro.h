#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

// One stretch of the sun or the moon being up, or being down, around a given
// moment. `elapsed` and `length` are seconds, so a bar can take them straight
// as its value and maximum.
typedef struct {
  int elapsed;
  int length;
  // UTC epoch of the rise or set that closes the stretch.
  time_t next_event;
  // The body is above the horizon for this stretch.
  bool up;
  // No rise and no set bracketed the moment: near the poles the body stays up
  // or down for weeks at a time. `up` still says which, and there is no
  // meaningful progress to show.
  bool circumpolar;
} AstroSpan;

// Both fill `span` for the stretch containing `now`. Results are cached per
// day and position, so calling these on every redraw is cheap.
void astro_sun_span(time_t now, int32_t latitude_micro, int32_t longitude_micro,
                    AstroSpan *span);
void astro_moon_span(time_t now, int32_t latitude_micro,
                     int32_t longitude_micro, AstroSpan *span);
