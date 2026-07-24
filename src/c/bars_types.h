#pragma once

#include <pebble.h>
#include <stdint.h>

#define MAX_SERIES 7
#define MAX_LABEL_BYTES 32

typedef enum {
  STYLE_HORIZONTAL = 0,
  STYLE_HORIZONTAL_INVERTED,
  STYLE_VERTICAL,
  STYLE_VERTICAL_INVERTED,
  STYLE_COUNT
} BarStyle;

typedef enum {
  // Keep the existing outside values stable so saved settings retain their
  // original visual meaning after adding the middle option.
  TEXT_PLACE_OUTSIDE_END = 0,
  TEXT_PLACE_OUTSIDE_START,
  TEXT_PLACE_INSIDE_START,
  TEXT_PLACE_INSIDE_MIDDLE,
  TEXT_PLACE_INSIDE_END,
  TEXT_PLACE_OUTSIDE_MIDDLE,
  TEXT_PLACE_ALWAYS_MIDDLE,
  TEXT_PLACE_COUNT
} TextPlacement;

typedef enum {
  CLOCK_FORMAT_SYSTEM = 0,
  CLOCK_FORMAT_24H,
  CLOCK_FORMAT_12H,
  CLOCK_FORMAT_COUNT
} ClockFormat;

typedef enum {
  SERIES_HOUR = 0,
  SERIES_MINUTE,
  SERIES_MONTH,
  SERIES_DATE,
  SERIES_DAY,
  SERIES_SECOND,
  SERIES_BATTERY
} SeriesId;

typedef struct {
  uint8_t style;
  uint8_t text_placement;
  uint8_t clock_format;
  uint8_t language;
  uint8_t clock_refresh_seconds;
  bool leading_zero;
  bool show_seconds;
  bool show_battery;
  bool smooth_progress;
  bool full_date_names;
  bool week_starts_sunday;
  bool seamless;
  bool text_outline;
  bool animate;
  bool vibe_disconnect;
  bool vibe_reconnect;
  GColor background_color;
  GColor track_color;
  GColor bar_colors[MAX_SERIES];
  GColor text_colors[MAX_SERIES];
  GColor text_on_bar_colors[MAX_SERIES];
} Settings;

typedef struct {
  SeriesId id;
  int value;
  int maximum;
  char label[MAX_LABEL_BYTES];
  GColor bar_color;
  GColor text_color;
  GColor text_on_bar_color;
} Series;
