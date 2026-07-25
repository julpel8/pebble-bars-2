#include "settings.h"

#include <string.h>

#include "languages.h"

enum {
  PERSIST_STYLE = 100,
  PERSIST_CLOCK_FORMAT,
  PERSIST_LEADING_ZERO,
  // Retired toggles, replaced by the per-series visibility mask. They are only
  // read now, to migrate installs saved before the series list existed.
  PERSIST_LEGACY_SHOW_SECONDS,
  PERSIST_LEGACY_SHOW_BATTERY,
  // Key 105 belonged to a retired setting. Keep later IDs stable for upgrades.
  PERSIST_ANIMATE = 106,
  // Keys 107 and 108 belonged to retired settings.
  PERSIST_BACKGROUND_COLOR = 109,
  // Retired: one track colour for the whole face, now one per series. Never
  // read, but the ID stays reserved so an upgrade cannot mistake it.
  PERSIST_LEGACY_TRACK_COLOR,
  PERSIST_HOUR_BAR_COLOR,
  PERSIST_RETIRED_HOUR_TEXT_COLOR,
  PERSIST_MINUTE_BAR_COLOR,
  PERSIST_RETIRED_MINUTE_TEXT_COLOR,
  PERSIST_MONTH_BAR_COLOR,
  PERSIST_RETIRED_MONTH_TEXT_COLOR,
  PERSIST_DATE_BAR_COLOR,
  PERSIST_RETIRED_DATE_TEXT_COLOR,
  PERSIST_DAY_BAR_COLOR,
  PERSIST_RETIRED_DAY_TEXT_COLOR,
  PERSIST_SECOND_BAR_COLOR,
  PERSIST_RETIRED_SECOND_TEXT_COLOR,
  PERSIST_BATTERY_BAR_COLOR,
  PERSIST_RETIRED_BATTERY_TEXT_COLOR,
  PERSIST_TEXT_PLACEMENT,
  PERSIST_HOUR_TEXT_COLOR,
  PERSIST_MINUTE_TEXT_COLOR,
  PERSIST_MONTH_TEXT_COLOR,
  PERSIST_DATE_TEXT_COLOR,
  PERSIST_DAY_TEXT_COLOR,
  PERSIST_SECOND_TEXT_COLOR,
  PERSIST_BATTERY_TEXT_COLOR,
  PERSIST_SEAMLESS,
  PERSIST_TEXT_OUTLINE,
  PERSIST_LANGUAGE,
  PERSIST_CLOCK_REFRESH,
  PERSIST_SMOOTH_PROGRESS,
  PERSIST_FULL_DATE_NAMES,
  PERSIST_WEEK_STARTS_SUNDAY,
  // Retired: the order travelled as one integer, four bits per series, which
  // ran out of room past seven. Still read, to migrate those installs.
  PERSIST_LEGACY_SERIES_ORDER,
  PERSIST_SERIES_VISIBLE,
  // The order as one byte per series, in display order.
  PERSIST_SERIES_ORDER,
  PERSIST_MERGE_HOUR_MINUTE,
  PERSIST_STEP_GOAL,
  PERSIST_CUSTOM_START_DAY,
  PERSIST_CUSTOM_TARGET_DAY,
  PERSIST_CUSTOM_LABEL,
  PERSIST_LATITUDE,
  PERSIST_LONGITUDE,
  PERSIST_LOCATION_VALID,
  PERSIST_DAYLIGHT_BAR_COLOR,
  PERSIST_RETIRED_DAYLIGHT_TEXT_COLOR,
  PERSIST_DAYLIGHT_TEXT_COLOR,
  PERSIST_MOON_BAR_COLOR,
  PERSIST_RETIRED_MOON_TEXT_COLOR,
  PERSIST_MOON_TEXT_COLOR,
  PERSIST_STEPS_BAR_COLOR,
  PERSIST_RETIRED_STEPS_TEXT_COLOR,
  PERSIST_STEPS_TEXT_COLOR,
  PERSIST_CUSTOM_BAR_COLOR,
  PERSIST_RETIRED_CUSTOM_TEXT_COLOR,
  PERSIST_CUSTOM_TEXT_COLOR,
  PERSIST_HOUR_TRACK_COLOR,
  PERSIST_MINUTE_TRACK_COLOR,
  PERSIST_MONTH_TRACK_COLOR,
  PERSIST_DATE_TRACK_COLOR,
  PERSIST_DAY_TRACK_COLOR,
  PERSIST_SECOND_TRACK_COLOR,
  PERSIST_BATTERY_TRACK_COLOR,
  PERSIST_DAYLIGHT_TRACK_COLOR,
  PERSIST_MOON_TRACK_COLOR,
  PERSIST_STEPS_TRACK_COLOR,
  PERSIST_CUSTOM_TRACK_COLOR,
  PERSIST_ROUND_POLAR_FILL
};

// All three tables are indexed by SeriesId.
static const int s_bar_persist_keys[MAX_SERIES] = {
    PERSIST_HOUR_BAR_COLOR,     PERSIST_MINUTE_BAR_COLOR,
    PERSIST_MONTH_BAR_COLOR,    PERSIST_DATE_BAR_COLOR,
    PERSIST_DAY_BAR_COLOR,      PERSIST_SECOND_BAR_COLOR,
    PERSIST_BATTERY_BAR_COLOR,  PERSIST_DAYLIGHT_BAR_COLOR,
    PERSIST_MOON_BAR_COLOR,     PERSIST_STEPS_BAR_COLOR,
    PERSIST_CUSTOM_BAR_COLOR};

static const int s_track_persist_keys[MAX_SERIES] = {
    PERSIST_HOUR_TRACK_COLOR,     PERSIST_MINUTE_TRACK_COLOR,
    PERSIST_MONTH_TRACK_COLOR,    PERSIST_DATE_TRACK_COLOR,
    PERSIST_DAY_TRACK_COLOR,      PERSIST_SECOND_TRACK_COLOR,
    PERSIST_BATTERY_TRACK_COLOR,  PERSIST_DAYLIGHT_TRACK_COLOR,
    PERSIST_MOON_TRACK_COLOR,     PERSIST_STEPS_TRACK_COLOR,
    PERSIST_CUSTOM_TRACK_COLOR};

static const int s_text_persist_keys[MAX_SERIES] = {
    PERSIST_HOUR_TEXT_COLOR,     PERSIST_MINUTE_TEXT_COLOR,
    PERSIST_MONTH_TEXT_COLOR,    PERSIST_DATE_TEXT_COLOR,
    PERSIST_DAY_TEXT_COLOR,      PERSIST_SECOND_TEXT_COLOR,
    PERSIST_BATTERY_TEXT_COLOR,  PERSIST_DAYLIGHT_TEXT_COLOR,
    PERSIST_MOON_TEXT_COLOR,     PERSIST_STEPS_TEXT_COLOR,
    PERSIST_CUSTOM_TEXT_COLOR};

static int clamp_int(int value, int minimum, int maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

static uint8_t valid_clock_refresh(int value) {
  switch (value) {
    case 1:
    case 5:
    case 10:
    case 20:
    case 30:
    case 60:
      return value;
    default:
      return 60;
  }
}

// The Pebble palette has four levels per channel, so halving the level index is
// the darkest step that keeps a colour recognisably itself: FF and AA both land
// on 55, and 55 lands on black.
static GColor darker_color(GColor color) {
  uint8_t red = (color.argb >> 4) & 0x03;
  uint8_t green = (color.argb >> 2) & 0x03;
  uint8_t blue = color.argb & 0x03;
  return (GColor){.argb = (uint8_t)(0xC0 | ((red / 2) << 4) |
                                    ((green / 2) << 2) | (blue / 2))};
}

// Only writes to `order` when the bytes are a complete permutation, so a
// corrupt value leaves the current order alone.
static bool apply_series_order(const uint8_t *data, size_t length,
                               uint8_t order[MAX_SERIES]) {
  if (length != MAX_SERIES) {
    return false;
  }
  uint8_t candidate[MAX_SERIES];
  bool seen[MAX_SERIES] = {false};
  for (int index = 0; index < MAX_SERIES; ++index) {
    uint8_t id = data[index];
    if (id >= MAX_SERIES || seen[id]) {
      return false;
    }
    seen[id] = true;
    candidate[index] = id;
  }
  memcpy(order, candidate, sizeof(candidate));
  return true;
}

// The order used to travel as one integer, series id 0 in the most significant
// nibble. Series added since keep the default tail order behind the saved ones.
static bool apply_legacy_series_order(uint32_t packed,
                                      uint8_t order[MAX_SERIES]) {
  uint8_t candidate[MAX_SERIES];
  bool seen[MAX_SERIES] = {false};
  int count = 0;
  for (int index = 0; index < LEGACY_SERIES_COUNT; ++index) {
    uint8_t id =
        (packed >> (4 * (LEGACY_SERIES_COUNT - 1 - index))) & 0x0F;
    if (id >= LEGACY_SERIES_COUNT || seen[id]) {
      return false;
    }
    seen[id] = true;
    candidate[count++] = id;
  }
  for (uint8_t id = LEGACY_SERIES_COUNT; id < MAX_SERIES; ++id) {
    candidate[count++] = id;
  }
  memcpy(order, candidate, sizeof(candidate));
  return true;
}

// An empty mask would leave a blank watchface, so it is rejected outright.
static bool apply_visible_mask(uint32_t mask, bool visible[MAX_SERIES]) {
  if ((mask & ((1u << MAX_SERIES) - 1)) == 0) {
    return false;
  }
  for (int index = 0; index < MAX_SERIES; ++index) {
    visible[index] = (mask & (1u << index)) != 0;
  }
  return true;
}

static uint32_t series_visible_mask(const bool visible[MAX_SERIES]) {
  uint32_t mask = 0;
  for (int index = 0; index < MAX_SERIES; ++index) {
    if (visible[index]) {
      mask |= 1u << index;
    }
  }
  return mask;
}

// Layout used before the series list existed: hour, minute, seconds, the three
// date parts in the language's natural order, then battery, with the series
// added since behind them. Used as the starting point for installs upgrading
// from that build.
static void series_order_from_language(uint8_t language,
                                       uint8_t order[MAX_SERIES]) {
  static const uint8_t date_part_series[] = {
      [DATE_PART_WEEKDAY] = SERIES_DAY,
      [DATE_PART_DATE] = SERIES_DATE,
      [DATE_PART_MONTH] = SERIES_MONTH};

  int index = 0;
  order[index++] = SERIES_HOUR;
  order[index++] = SERIES_MINUTE;
  order[index++] = SERIES_SECOND;
  for (int part = 0; part < 3; ++part) {
    order[index++] = date_part_series[date_part_order[language][part]];
  }
  order[index++] = SERIES_BATTERY;
  for (uint8_t id = LEGACY_SERIES_COUNT; id < MAX_SERIES; ++id) {
    order[index++] = id;
  }
}

static GColor color_from_persist(int key, GColor fallback) {
  if (!persist_exists(key)) {
    return fallback;
  }
  return (GColor){.argb = (uint8_t)persist_read_int(key)};
}

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings){
      .style = STYLE_HORIZONTAL,
      .text_placement = TEXT_PLACE_INSIDE_START,
      .round_polar_fill = ROUND_POLAR_FILL_NONE,
      .clock_format = CLOCK_FORMAT_SYSTEM,
      .language = LANGUAGE_EN,
      .clock_refresh_seconds = 60,
      .leading_zero = true,
      .smooth_progress = true,
      .full_date_names = false,
      .week_starts_sunday = false,
      .seamless = true,
      .text_outline = false,
      .animate = true,
      .merge_hour_minute = false,
      .step_goal = 10000,
      .custom_start_day = 0,
      .custom_target_day = 0,
      .custom_label = "{d}",
      .latitude = 0,
      .longitude = 0,
      .location_valid = false,
      .series_order =
          {
              SERIES_HOUR, SERIES_MINUTE, SERIES_SECOND, SERIES_DAY,
              SERIES_MONTH, SERIES_DATE, SERIES_BATTERY, SERIES_DAYLIGHT,
              SERIES_MOON, SERIES_STEPS, SERIES_CUSTOM,
          },
      // Indexed by SeriesId: the five bars the original face showed, and
      // nothing that needs a goal, a date or a location to mean anything.
      .series_visible =
          {
              [SERIES_HOUR] = true,      [SERIES_MINUTE] = true,
              [SERIES_MONTH] = true,     [SERIES_DATE] = true,
              [SERIES_DAY] = true,       [SERIES_SECOND] = false,
              [SERIES_BATTERY] = false,  [SERIES_DAYLIGHT] = false,
              [SERIES_MOON] = false,     [SERIES_STEPS] = false,
              [SERIES_CUSTOM] = false,
          },
      .background_color = GColorBlack,
      .bar_colors =
          {
              GColorFromHEX(0x00FF00),
              GColorFromHEX(0x00AA55),
              GColorFromHEX(0x0055FF),
              GColorFromHEX(0xFFFF00),
              GColorFromHEX(0xFF0000),
              GColorFromHEX(0xAA00FF),
              GColorFromHEX(0x00FFFF),
              GColorFromHEX(0xFFAA00),
              GColorFromHEX(0xAAAAFF),
              GColorFromHEX(0x55FF00),
              GColorFromHEX(0xFF00AA),
          },
      .text_colors =
          {
              GColorFromHEX(0xAAFFAA),
              GColorFromHEX(0x55FFFF),
              GColorFromHEX(0x55AAFF),
              GColorFromHEX(0x555500),
              GColorFromHEX(0xFFAAAA),
              GColorFromHEX(0xFFAAFF),
              GColorFromHEX(0xFFFFFF),
              GColorFromHEX(0x552A00),
              GColorFromHEX(0x000055),
              GColorFromHEX(0x005500),
              GColorFromHEX(0xFFAAFF),
          }};

  // The point of a coloured track: each bar sits on a much darker shade of its
  // own colour rather than on one shared background.
  for (int index = 0; index < MAX_SERIES; ++index) {
    settings->track_colors[index] = darker_color(settings->bar_colors[index]);
  }
}

void settings_load(Settings *settings) {
  settings_set_defaults(settings);

  if (persist_exists(PERSIST_STYLE)) {
    int style = persist_read_int(PERSIST_STYLE);
    settings->style =
        style >= 0 && style < STYLE_COUNT ? style : STYLE_HORIZONTAL;
  }
  if (persist_exists(PERSIST_TEXT_PLACEMENT)) {
    settings->text_placement =
        clamp_int(persist_read_int(PERSIST_TEXT_PLACEMENT), 0,
                  TEXT_PLACE_COUNT - 1);
  }
  if (persist_exists(PERSIST_ROUND_POLAR_FILL)) {
    settings->round_polar_fill =
        clamp_int(persist_read_int(PERSIST_ROUND_POLAR_FILL),
                  ROUND_POLAR_FILL_NONE, ROUND_POLAR_FILL_BOTH);
  }
  if (persist_exists(PERSIST_CLOCK_FORMAT)) {
    settings->clock_format =
        clamp_int(persist_read_int(PERSIST_CLOCK_FORMAT), 0,
                  CLOCK_FORMAT_COUNT - 1);
  }
  if (persist_exists(PERSIST_LANGUAGE)) {
    settings->language =
        clamp_int(persist_read_int(PERSIST_LANGUAGE), 0, LANGUAGE_COUNT - 1);
  }
  if (persist_exists(PERSIST_CLOCK_REFRESH)) {
    settings->clock_refresh_seconds =
        valid_clock_refresh(persist_read_int(PERSIST_CLOCK_REFRESH));
  }
  if (persist_exists(PERSIST_STEP_GOAL)) {
    settings->step_goal =
        (uint16_t)clamp_int(persist_read_int(PERSIST_STEP_GOAL), 500, 60000);
  }
  if (persist_exists(PERSIST_CUSTOM_START_DAY)) {
    settings->custom_start_day = persist_read_int(PERSIST_CUSTOM_START_DAY);
  }
  if (persist_exists(PERSIST_CUSTOM_TARGET_DAY)) {
    settings->custom_target_day = persist_read_int(PERSIST_CUSTOM_TARGET_DAY);
  }
  if (persist_exists(PERSIST_CUSTOM_LABEL)) {
    persist_read_string(PERSIST_CUSTOM_LABEL, settings->custom_label,
                        sizeof(settings->custom_label));
  }
  if (persist_exists(PERSIST_LATITUDE)) {
    settings->latitude = persist_read_int(PERSIST_LATITUDE);
  }
  if (persist_exists(PERSIST_LONGITUDE)) {
    settings->longitude = persist_read_int(PERSIST_LONGITUDE);
  }
  if (persist_exists(PERSIST_LOCATION_VALID)) {
    settings->location_valid = persist_read_bool(PERSIST_LOCATION_VALID);
  }
  if (persist_exists(PERSIST_LEADING_ZERO)) {
    settings->leading_zero = persist_read_bool(PERSIST_LEADING_ZERO);
  }
  if (persist_exists(PERSIST_SMOOTH_PROGRESS)) {
    settings->smooth_progress = persist_read_bool(PERSIST_SMOOTH_PROGRESS);
  }
  if (persist_exists(PERSIST_FULL_DATE_NAMES)) {
    settings->full_date_names = persist_read_bool(PERSIST_FULL_DATE_NAMES);
  }
  if (persist_exists(PERSIST_WEEK_STARTS_SUNDAY)) {
    settings->week_starts_sunday =
        persist_read_bool(PERSIST_WEEK_STARTS_SUNDAY);
  }
  if (persist_exists(PERSIST_SEAMLESS)) {
    settings->seamless = persist_read_bool(PERSIST_SEAMLESS);
  }
  if (persist_exists(PERSIST_TEXT_OUTLINE)) {
    settings->text_outline = persist_read_bool(PERSIST_TEXT_OUTLINE);
  }
  if (persist_exists(PERSIST_ANIMATE)) {
    settings->animate = persist_read_bool(PERSIST_ANIMATE);
  }
  if (persist_exists(PERSIST_MERGE_HOUR_MINUTE)) {
    settings->merge_hour_minute =
        persist_read_bool(PERSIST_MERGE_HOUR_MINUTE);
  }
  if (persist_exists(PERSIST_SERIES_ORDER)) {
    uint8_t data[MAX_SERIES] = {0};
    int read = persist_read_data(PERSIST_SERIES_ORDER, data, sizeof(data));
    if (read == (int)sizeof(data)) {
      apply_series_order(data, sizeof(data), settings->series_order);
    }
  } else if (persist_exists(PERSIST_LEGACY_SERIES_ORDER)) {
    apply_legacy_series_order(
        (uint32_t)persist_read_int(PERSIST_LEGACY_SERIES_ORDER),
        settings->series_order);
  } else {
    series_order_from_language(settings->language, settings->series_order);
  }
  if (persist_exists(PERSIST_SERIES_VISIBLE)) {
    apply_visible_mask((uint32_t)persist_read_int(PERSIST_SERIES_VISIBLE),
                       settings->series_visible);
  } else {
    if (persist_exists(PERSIST_LEGACY_SHOW_SECONDS)) {
      settings->series_visible[SERIES_SECOND] =
          persist_read_bool(PERSIST_LEGACY_SHOW_SECONDS);
    }
    if (persist_exists(PERSIST_LEGACY_SHOW_BATTERY)) {
      settings->series_visible[SERIES_BATTERY] =
          persist_read_bool(PERSIST_LEGACY_SHOW_BATTERY);
    }
  }
  settings->background_color =
      color_from_persist(PERSIST_BACKGROUND_COLOR, settings->background_color);

  for (int index = 0; index < MAX_SERIES; ++index) {
    settings->bar_colors[index] =
        color_from_persist(s_bar_persist_keys[index],
                           settings->bar_colors[index]);
    settings->text_colors[index] =
        color_from_persist(s_text_persist_keys[index],
                           settings->text_colors[index]);
    // Derived from whatever the bar colour turned out to be, so a saved bar
    // colour without a saved track still gets a matching dark shade.
    settings->track_colors[index] =
        color_from_persist(s_track_persist_keys[index],
                           darker_color(settings->bar_colors[index]));
  }
}

void settings_save(const Settings *settings) {
  persist_write_int(PERSIST_STYLE, settings->style);
  persist_write_int(PERSIST_TEXT_PLACEMENT, settings->text_placement);
  persist_write_int(PERSIST_ROUND_POLAR_FILL, settings->round_polar_fill);
  persist_write_int(PERSIST_CLOCK_FORMAT, settings->clock_format);
  persist_write_int(PERSIST_LANGUAGE, settings->language);
  persist_write_int(PERSIST_CLOCK_REFRESH, settings->clock_refresh_seconds);
  persist_write_int(PERSIST_STEP_GOAL, settings->step_goal);
  persist_write_int(PERSIST_CUSTOM_START_DAY, settings->custom_start_day);
  persist_write_int(PERSIST_CUSTOM_TARGET_DAY, settings->custom_target_day);
  persist_write_string(PERSIST_CUSTOM_LABEL, settings->custom_label);
  persist_write_int(PERSIST_LATITUDE, settings->latitude);
  persist_write_int(PERSIST_LONGITUDE, settings->longitude);
  persist_write_bool(PERSIST_LOCATION_VALID, settings->location_valid);
  persist_write_bool(PERSIST_LEADING_ZERO, settings->leading_zero);
  persist_write_bool(PERSIST_SMOOTH_PROGRESS, settings->smooth_progress);
  persist_write_bool(PERSIST_FULL_DATE_NAMES, settings->full_date_names);
  persist_write_bool(PERSIST_WEEK_STARTS_SUNDAY,
                     settings->week_starts_sunday);
  persist_write_bool(PERSIST_SEAMLESS, settings->seamless);
  persist_write_bool(PERSIST_TEXT_OUTLINE, settings->text_outline);
  persist_write_bool(PERSIST_ANIMATE, settings->animate);
  persist_write_bool(PERSIST_MERGE_HOUR_MINUTE, settings->merge_hour_minute);
  persist_write_data(PERSIST_SERIES_ORDER, settings->series_order,
                     MAX_SERIES);
  persist_write_int(PERSIST_SERIES_VISIBLE,
                    (int)series_visible_mask(settings->series_visible));
  persist_write_int(PERSIST_BACKGROUND_COLOR, settings->background_color.argb);

  for (int index = 0; index < MAX_SERIES; ++index) {
    persist_write_int(s_bar_persist_keys[index],
                      settings->bar_colors[index].argb);
    persist_write_int(s_track_persist_keys[index],
                      settings->track_colors[index].argb);
    persist_write_int(s_text_persist_keys[index],
                      settings->text_colors[index].argb);
  }
}

static void apply_bool_tuple(DictionaryIterator *iterator, uint32_t key,
                             bool *destination) {
  Tuple *tuple = dict_find(iterator, key);
  if (tuple) {
    *destination = tuple->value->int32 != 0;
  }
}

static void apply_color_tuple(DictionaryIterator *iterator, uint32_t key,
                              GColor *destination) {
  Tuple *tuple = dict_find(iterator, key);
  if (tuple) {
    *destination = GColorFromHEX(tuple->value->uint32);
  }
}

void settings_apply_message(Settings *settings, DictionaryIterator *iterator) {
  Tuple *tuple = dict_find(iterator, MESSAGE_KEY_SETTING_STYLE);
  if (tuple) {
    int style = tuple->value->int32;
    settings->style =
        style >= 0 && style < STYLE_COUNT ? style : STYLE_HORIZONTAL;
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_TEXT_PLACEMENT);
  if (tuple) {
    settings->text_placement =
        clamp_int(tuple->value->int32, 0, TEXT_PLACE_COUNT - 1);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_ROUND_POLAR_FILL);
  if (tuple) {
    settings->round_polar_fill =
        clamp_int(tuple->value->int32, ROUND_POLAR_FILL_NONE,
                  ROUND_POLAR_FILL_BOTH);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_CLOCK_FORMAT);
  if (tuple) {
    settings->clock_format =
        clamp_int(tuple->value->int32, 0, CLOCK_FORMAT_COUNT - 1);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_LANGUAGE);
  if (tuple) {
    settings->language =
        clamp_int(tuple->value->int32, 0, LANGUAGE_COUNT - 1);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_CLOCK_REFRESH);
  if (tuple) {
    settings->clock_refresh_seconds =
        valid_clock_refresh(tuple->value->int32);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_STEP_GOAL);
  if (tuple) {
    settings->step_goal =
        (uint16_t)clamp_int(tuple->value->int32, 500, 60000);
  }

  // Day numbers, so the range is generous but still bounded: 1970 through the
  // middle of the twenty-second century.
  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_CUSTOM_START_DAY);
  if (tuple) {
    settings->custom_start_day = clamp_int(tuple->value->int32, 0, 80000);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_CUSTOM_TARGET_DAY);
  if (tuple) {
    settings->custom_target_day = clamp_int(tuple->value->int32, 0, 80000);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_CUSTOM_LABEL);
  if (tuple && tuple->type == TUPLE_CSTRING) {
    snprintf(settings->custom_label, sizeof(settings->custom_label), "%s",
             tuple->value->cstring);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_LATITUDE);
  if (tuple) {
    settings->latitude = clamp_int(tuple->value->int32, -90000000, 90000000);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_LONGITUDE);
  if (tuple) {
    settings->longitude = clamp_int(tuple->value->int32, -180000000, 180000000);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_SERIES_ORDER);
  if (tuple && tuple->type == TUPLE_BYTE_ARRAY) {
    apply_series_order(tuple->value->data, tuple->length,
                       settings->series_order);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_SERIES_VISIBLE);
  if (tuple) {
    apply_visible_mask(tuple->value->uint32, settings->series_visible);
  }

  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_LOCATION_VALID,
                   &settings->location_valid);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_LEADING_ZERO,
                   &settings->leading_zero);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_SMOOTH_PROGRESS,
                   &settings->smooth_progress);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_FULL_DATE_NAMES,
                   &settings->full_date_names);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_WEEK_STARTS_SUNDAY,
                   &settings->week_starts_sunday);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_SEAMLESS_BARS,
                   &settings->seamless);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_TEXT_OUTLINE,
                   &settings->text_outline);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_ANIMATE, &settings->animate);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_MERGE_HOUR_MINUTE,
                   &settings->merge_hour_minute);

  apply_color_tuple(iterator, MESSAGE_KEY_SETTING_BACKGROUND_COLOR,
                    &settings->background_color);

  // The SDK resolves MESSAGE_KEY_* at run time, so these cannot be file-scope
  // constants. Built once here and indexed by SeriesId.
  const uint32_t bar_keys[MAX_SERIES] = {
      MESSAGE_KEY_SETTING_HOUR_BAR_COLOR,
      MESSAGE_KEY_SETTING_MINUTE_BAR_COLOR,
      MESSAGE_KEY_SETTING_MONTH_BAR_COLOR,
      MESSAGE_KEY_SETTING_DATE_BAR_COLOR,
      MESSAGE_KEY_SETTING_DAY_BAR_COLOR,
      MESSAGE_KEY_SETTING_SECOND_BAR_COLOR,
      MESSAGE_KEY_SETTING_BATTERY_BAR_COLOR,
      MESSAGE_KEY_SETTING_DAYLIGHT_BAR_COLOR,
      MESSAGE_KEY_SETTING_MOON_BAR_COLOR,
      MESSAGE_KEY_SETTING_STEPS_BAR_COLOR,
      MESSAGE_KEY_SETTING_CUSTOM_BAR_COLOR};
  const uint32_t track_keys[MAX_SERIES] = {
      MESSAGE_KEY_SETTING_HOUR_TRACK_COLOR,
      MESSAGE_KEY_SETTING_MINUTE_TRACK_COLOR,
      MESSAGE_KEY_SETTING_MONTH_TRACK_COLOR,
      MESSAGE_KEY_SETTING_DATE_TRACK_COLOR,
      MESSAGE_KEY_SETTING_DAY_TRACK_COLOR,
      MESSAGE_KEY_SETTING_SECOND_TRACK_COLOR,
      MESSAGE_KEY_SETTING_BATTERY_TRACK_COLOR,
      MESSAGE_KEY_SETTING_DAYLIGHT_TRACK_COLOR,
      MESSAGE_KEY_SETTING_MOON_TRACK_COLOR,
      MESSAGE_KEY_SETTING_STEPS_TRACK_COLOR,
      MESSAGE_KEY_SETTING_CUSTOM_TRACK_COLOR};
  const uint32_t text_keys[MAX_SERIES] = {
      MESSAGE_KEY_SETTING_HOUR_TEXT_COLOR,
      MESSAGE_KEY_SETTING_MINUTE_TEXT_COLOR,
      MESSAGE_KEY_SETTING_MONTH_TEXT_COLOR,
      MESSAGE_KEY_SETTING_DATE_TEXT_COLOR,
      MESSAGE_KEY_SETTING_DAY_TEXT_COLOR,
      MESSAGE_KEY_SETTING_SECOND_TEXT_COLOR,
      MESSAGE_KEY_SETTING_BATTERY_TEXT_COLOR,
      MESSAGE_KEY_SETTING_DAYLIGHT_TEXT_COLOR,
      MESSAGE_KEY_SETTING_MOON_TEXT_COLOR,
      MESSAGE_KEY_SETTING_STEPS_TEXT_COLOR,
      MESSAGE_KEY_SETTING_CUSTOM_TEXT_COLOR};

  for (int index = 0; index < MAX_SERIES; ++index) {
    apply_color_tuple(iterator, bar_keys[index],
                      &settings->bar_colors[index]);
    apply_color_tuple(iterator, track_keys[index],
                      &settings->track_colors[index]);
    apply_color_tuple(iterator, text_keys[index],
                      &settings->text_colors[index]);
  }
}
