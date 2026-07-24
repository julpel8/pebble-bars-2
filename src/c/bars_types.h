#pragma once

#include <pebble.h>
#include <stdint.h>

#define MAX_SERIES 7
#define MAX_LABEL_BYTES 8

typedef enum {
  STYLE_HORIZONTAL = 0,
  STYLE_HORIZONTAL_INVERTED,
  STYLE_VERTICAL,
  STYLE_VERTICAL_INVERTED,
  STYLE_COUNT
} BarStyle;

typedef enum {
  TEXT_PLACE_OUTSIDE_OPPOSITE = 0,
  TEXT_PLACE_OUTSIDE_EDGE,
  TEXT_PLACE_INSIDE_START,
  TEXT_PLACE_INSIDE_MIDDLE,
  TEXT_PLACE_INSIDE_END,
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
  bool leading_zero;
  bool show_seconds;
  bool show_battery;
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
