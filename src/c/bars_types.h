#pragma once

#include <pebble.h>
#include <stdint.h>

#define MAX_LABEL_BYTES 32

typedef enum {
  // Appended, never reordered: saved settings store the numeric value.
  STYLE_HORIZONTAL = 0,
  STYLE_HORIZONTAL_INVERTED,
  STYLE_VERTICAL,
  STYLE_VERTICAL_INVERTED,
  STYLE_POLAR_RECTANGULAR,
  STYLE_POLAR_RECTANGULAR_INVERTED,
  STYLE_POLAR_ROUND,
  STYLE_POLAR_ROUND_INVERTED,
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
  // Bit flags: the outside and centre can be extended independently or
  // together from the first and last visible round-polar rings.
  ROUND_POLAR_FILL_NONE = 0,
  ROUND_POLAR_FILL_OUTSIDE = 1 << 0,
  ROUND_POLAR_FILL_CENTRE = 1 << 1,
  ROUND_POLAR_FILL_BOTH =
      ROUND_POLAR_FILL_OUTSIDE | ROUND_POLAR_FILL_CENTRE
} RoundPolarFill;

typedef enum {
  CLOCK_FORMAT_SYSTEM = 0,
  CLOCK_FORMAT_24H,
  CLOCK_FORMAT_12H,
  CLOCK_FORMAT_COUNT
} ClockFormat;

// Keep this order in sync with the SERIES table in src/pkjs/index.js: the
// configuration page packs these ids into SETTING_SERIES_ORDER and indexes
// SETTING_SERIES_VISIBLE by them.
typedef enum {
  SERIES_HOUR = 0,
  SERIES_MINUTE,
  SERIES_MONTH,
  SERIES_DATE,
  SERIES_DAY,
  SERIES_SECOND,
  SERIES_BATTERY,
  SERIES_DAYLIGHT,
  SERIES_MOON,
  SERIES_STEPS,
  SERIES_CUSTOM,
  SERIES_COUNT
} SeriesId;

#define MAX_SERIES SERIES_COUNT

// The seven series that existed before the list grew past a single packed
// integer. Saved orders from that build hold exactly these, four bits each.
#define LEGACY_SERIES_COUNT 7

typedef struct {
  uint8_t style;
  uint8_t text_placement;
  uint8_t round_polar_fill;
  uint8_t clock_format;
  uint8_t language;
  uint8_t clock_refresh_seconds;
  bool leading_zero;
  bool smooth_progress;
  bool full_date_names;
  bool week_starts_sunday;
  bool seamless;
  bool text_outline;
  bool animate;
  // Draws the hour and minute as one "HH:MM" bar instead of two.
  bool merge_hour_minute;
  uint16_t step_goal;
  // Local civil day numbers, days since 1970-01-01. An empty span (target not
  // after start) means the custom bar has no date set yet.
  int32_t custom_start_day;
  int32_t custom_target_day;
  // Label template for the custom bar: {d} days left, {t} days in the span,
  // {p} percent elapsed. Anything else is copied through.
  char custom_label[MAX_LABEL_BYTES];
  // Microdegrees. The watch has no receiver of its own, so the phone sends a
  // fix and the sun and moon bars work from the stored copy afterwards.
  int32_t latitude;
  int32_t longitude;
  bool location_valid;
  // Display order of the bars, as SeriesId values; hidden series keep their
  // slot so unhiding one restores its position.
  uint8_t series_order[MAX_SERIES];
  // Indexed by SeriesId, not by position in series_order.
  bool series_visible[MAX_SERIES];
  GColor background_color;
  GColor bar_colors[MAX_SERIES];
  // One track per series rather than one for the whole face, so a bar can sit
  // on a darker shade of its own colour.
  GColor track_colors[MAX_SERIES];
  GColor text_colors[MAX_SERIES];
  GColor text_on_bar_colors[MAX_SERIES];
} Settings;

typedef struct {
  SeriesId id;
  int value;
  int maximum;
  char label[MAX_LABEL_BYTES];
  // The widest label this series can show. The font ladder measures this
  // instead of the current label, so the text keeps one size as the value
  // changes.
  char measure[MAX_LABEL_BYTES];
  GColor bar_color;
  GColor track_color;
  GColor text_color;
  GColor text_on_bar_color;
} Series;
