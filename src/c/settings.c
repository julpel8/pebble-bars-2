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
  PERSIST_VIBE_DISCONNECT,
  PERSIST_VIBE_RECONNECT,
  PERSIST_BACKGROUND_COLOR,
  PERSIST_TRACK_COLOR,
  PERSIST_HOUR_BAR_COLOR,
  PERSIST_HOUR_TEXT_COLOR,
  PERSIST_MINUTE_BAR_COLOR,
  PERSIST_MINUTE_TEXT_COLOR,
  PERSIST_MONTH_BAR_COLOR,
  PERSIST_MONTH_TEXT_COLOR,
  PERSIST_DATE_BAR_COLOR,
  PERSIST_DATE_TEXT_COLOR,
  PERSIST_DAY_BAR_COLOR,
  PERSIST_DAY_TEXT_COLOR,
  PERSIST_SECOND_BAR_COLOR,
  PERSIST_SECOND_TEXT_COLOR,
  PERSIST_BATTERY_BAR_COLOR,
  PERSIST_BATTERY_TEXT_COLOR,
  PERSIST_TEXT_PLACEMENT,
  PERSIST_HOUR_TEXT_ON_BAR_COLOR,
  PERSIST_MINUTE_TEXT_ON_BAR_COLOR,
  PERSIST_MONTH_TEXT_ON_BAR_COLOR,
  PERSIST_DATE_TEXT_ON_BAR_COLOR,
  PERSIST_DAY_TEXT_ON_BAR_COLOR,
  PERSIST_SECOND_TEXT_ON_BAR_COLOR,
  PERSIST_BATTERY_TEXT_ON_BAR_COLOR,
  PERSIST_SEAMLESS,
  PERSIST_TEXT_OUTLINE,
  PERSIST_LANGUAGE,
  PERSIST_CLOCK_REFRESH,
  PERSIST_SMOOTH_PROGRESS,
  PERSIST_FULL_DATE_NAMES,
  PERSIST_WEEK_STARTS_SUNDAY,
  PERSIST_SERIES_ORDER,
  PERSIST_SERIES_VISIBLE
};

static const int s_bar_persist_keys[MAX_SERIES] = {
    PERSIST_HOUR_BAR_COLOR,    PERSIST_MINUTE_BAR_COLOR,
    PERSIST_MONTH_BAR_COLOR,   PERSIST_DATE_BAR_COLOR,
    PERSIST_DAY_BAR_COLOR,     PERSIST_SECOND_BAR_COLOR,
    PERSIST_BATTERY_BAR_COLOR};

static const int s_text_persist_keys[MAX_SERIES] = {
    PERSIST_HOUR_TEXT_COLOR,    PERSIST_MINUTE_TEXT_COLOR,
    PERSIST_MONTH_TEXT_COLOR,   PERSIST_DATE_TEXT_COLOR,
    PERSIST_DAY_TEXT_COLOR,     PERSIST_SECOND_TEXT_COLOR,
    PERSIST_BATTERY_TEXT_COLOR};

static const int s_text_on_bar_persist_keys[MAX_SERIES] = {
    PERSIST_HOUR_TEXT_ON_BAR_COLOR,    PERSIST_MINUTE_TEXT_ON_BAR_COLOR,
    PERSIST_MONTH_TEXT_ON_BAR_COLOR,   PERSIST_DATE_TEXT_ON_BAR_COLOR,
    PERSIST_DAY_TEXT_ON_BAR_COLOR,     PERSIST_SECOND_TEXT_ON_BAR_COLOR,
    PERSIST_BATTERY_TEXT_ON_BAR_COLOR};

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

// The display order travels as one integer: series id 0 in the most
// significant nibble, so MAX_SERIES ids fit in 28 bits.
static uint32_t pack_series_order(const uint8_t order[MAX_SERIES]) {
  uint32_t packed = 0;
  for (int index = 0; index < MAX_SERIES; ++index) {
    packed = (packed << 4) | (order[index] & 0x0F);
  }
  return packed;
}

// Only writes to `order` when `packed` is a complete permutation, so a corrupt
// value leaves the current order alone.
static bool unpack_series_order(uint32_t packed, uint8_t order[MAX_SERIES]) {
  uint8_t candidate[MAX_SERIES];
  uint8_t seen = 0;
  for (int index = 0; index < MAX_SERIES; ++index) {
    uint8_t id = (packed >> (4 * (MAX_SERIES - 1 - index))) & 0x0F;
    if (id >= MAX_SERIES || (seen & (1 << id))) {
      return false;
    }
    seen |= 1 << id;
    candidate[index] = id;
  }
  memcpy(order, candidate, sizeof(candidate));
  return true;
}

// An empty mask would leave a blank watchface, so it is rejected outright.
static bool apply_visible_mask(uint32_t mask, bool visible[MAX_SERIES]) {
  if ((mask & ((1 << MAX_SERIES) - 1)) == 0) {
    return false;
  }
  for (int index = 0; index < MAX_SERIES; ++index) {
    visible[index] = (mask & (1 << index)) != 0;
  }
  return true;
}

static uint32_t series_visible_mask(const bool visible[MAX_SERIES]) {
  uint32_t mask = 0;
  for (int index = 0; index < MAX_SERIES; ++index) {
    if (visible[index]) {
      mask |= 1 << index;
    }
  }
  return mask;
}

// Layout used before the series list existed: hour, minute, seconds, the three
// date parts in the language's natural order, then battery. Used as the
// starting point for installs upgrading from that build.
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
  order[index] = SERIES_BATTERY;
}

static GColor color_from_persist(int key, GColor fallback) {
  if (!persist_exists(key)) {
    return fallback;
  }
  return (GColor){.argb = (uint8_t)persist_read_int(key)};
}

static uint32_t bar_message_key(int index) {
  switch (index) {
    case SERIES_HOUR:
      return MESSAGE_KEY_SETTING_HOUR_BAR_COLOR;
    case SERIES_MINUTE:
      return MESSAGE_KEY_SETTING_MINUTE_BAR_COLOR;
    case SERIES_MONTH:
      return MESSAGE_KEY_SETTING_MONTH_BAR_COLOR;
    case SERIES_DATE:
      return MESSAGE_KEY_SETTING_DATE_BAR_COLOR;
    case SERIES_DAY:
      return MESSAGE_KEY_SETTING_DAY_BAR_COLOR;
    case SERIES_SECOND:
      return MESSAGE_KEY_SETTING_SECOND_BAR_COLOR;
    default:
      return MESSAGE_KEY_SETTING_BATTERY_BAR_COLOR;
  }
}

static uint32_t text_message_key(int index) {
  switch (index) {
    case SERIES_HOUR:
      return MESSAGE_KEY_SETTING_HOUR_TEXT_COLOR;
    case SERIES_MINUTE:
      return MESSAGE_KEY_SETTING_MINUTE_TEXT_COLOR;
    case SERIES_MONTH:
      return MESSAGE_KEY_SETTING_MONTH_TEXT_COLOR;
    case SERIES_DATE:
      return MESSAGE_KEY_SETTING_DATE_TEXT_COLOR;
    case SERIES_DAY:
      return MESSAGE_KEY_SETTING_DAY_TEXT_COLOR;
    case SERIES_SECOND:
      return MESSAGE_KEY_SETTING_SECOND_TEXT_COLOR;
    default:
      return MESSAGE_KEY_SETTING_BATTERY_TEXT_COLOR;
  }
}

static uint32_t text_on_bar_message_key(int index) {
  switch (index) {
    case SERIES_HOUR:
      return MESSAGE_KEY_SETTING_HOUR_TEXT_ON_BAR_COLOR;
    case SERIES_MINUTE:
      return MESSAGE_KEY_SETTING_MINUTE_TEXT_ON_BAR_COLOR;
    case SERIES_MONTH:
      return MESSAGE_KEY_SETTING_MONTH_TEXT_ON_BAR_COLOR;
    case SERIES_DATE:
      return MESSAGE_KEY_SETTING_DATE_TEXT_ON_BAR_COLOR;
    case SERIES_DAY:
      return MESSAGE_KEY_SETTING_DAY_TEXT_ON_BAR_COLOR;
    case SERIES_SECOND:
      return MESSAGE_KEY_SETTING_SECOND_TEXT_ON_BAR_COLOR;
    default:
      return MESSAGE_KEY_SETTING_BATTERY_TEXT_ON_BAR_COLOR;
  }
}

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings){
      .style = STYLE_HORIZONTAL,
      .text_placement = TEXT_PLACE_INSIDE_START,
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
      .vibe_disconnect = true,
      .vibe_reconnect = false,
      .series_order =
          {
              SERIES_HOUR, SERIES_MINUTE, SERIES_SECOND, SERIES_DAY,
              SERIES_MONTH, SERIES_DATE, SERIES_BATTERY,
          },
      // Indexed by SeriesId: everything but seconds and battery.
      .series_visible =
          {
              [SERIES_HOUR] = true,   [SERIES_MINUTE] = true,
              [SERIES_MONTH] = true,  [SERIES_DATE] = true,
              [SERIES_DAY] = true,    [SERIES_SECOND] = false,
              [SERIES_BATTERY] = false,
          },
      .background_color = GColorBlack,
      .track_color = GColorBlack,
      .bar_colors =
          {
              GColorFromHEX(0x00FF00),
              GColorFromHEX(0x00AA55),
              GColorFromHEX(0x0055FF),
              GColorFromHEX(0xFFFF00),
              GColorFromHEX(0xFF0000),
              GColorFromHEX(0xAA00FF),
              GColorWhite,
          },
      .text_colors =
          {
              GColorFromHEX(0x00FF00),
              GColorFromHEX(0x00AA55),
              GColorFromHEX(0x0055FF),
              GColorFromHEX(0xFFFF00),
              GColorFromHEX(0xFF0000),
              GColorFromHEX(0xAA00FF),
              GColorFromHEX(0x00FFFF),
          },
      .text_on_bar_colors =
          {
              GColorFromHEX(0xAAFFAA),
              GColorFromHEX(0x55FFFF),
              GColorFromHEX(0x55AAFF),
              GColorFromHEX(0x555500),
              GColorFromHEX(0xFFAAAA),
              GColorFromHEX(0xFFAAFF),
              GColorWhite,
          }};
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
  if (persist_exists(PERSIST_VIBE_DISCONNECT)) {
    settings->vibe_disconnect = persist_read_bool(PERSIST_VIBE_DISCONNECT);
  }
  if (persist_exists(PERSIST_VIBE_RECONNECT)) {
    settings->vibe_reconnect = persist_read_bool(PERSIST_VIBE_RECONNECT);
  }

  if (persist_exists(PERSIST_SERIES_ORDER)) {
    unpack_series_order((uint32_t)persist_read_int(PERSIST_SERIES_ORDER),
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
  settings->track_color =
      color_from_persist(PERSIST_TRACK_COLOR, settings->track_color);

  for (int index = 0; index < MAX_SERIES; ++index) {
    settings->bar_colors[index] =
        color_from_persist(s_bar_persist_keys[index],
                           settings->bar_colors[index]);
    settings->text_colors[index] =
        color_from_persist(s_text_persist_keys[index],
                           settings->text_colors[index]);
    settings->text_on_bar_colors[index] =
        color_from_persist(s_text_on_bar_persist_keys[index],
                           settings->text_on_bar_colors[index]);
  }
}

void settings_save(const Settings *settings) {
  persist_write_int(PERSIST_STYLE, settings->style);
  persist_write_int(PERSIST_TEXT_PLACEMENT, settings->text_placement);
  persist_write_int(PERSIST_CLOCK_FORMAT, settings->clock_format);
  persist_write_int(PERSIST_LANGUAGE, settings->language);
  persist_write_int(PERSIST_CLOCK_REFRESH, settings->clock_refresh_seconds);
  persist_write_bool(PERSIST_LEADING_ZERO, settings->leading_zero);
  persist_write_bool(PERSIST_SMOOTH_PROGRESS, settings->smooth_progress);
  persist_write_bool(PERSIST_FULL_DATE_NAMES, settings->full_date_names);
  persist_write_bool(PERSIST_WEEK_STARTS_SUNDAY,
                     settings->week_starts_sunday);
  persist_write_bool(PERSIST_SEAMLESS, settings->seamless);
  persist_write_bool(PERSIST_TEXT_OUTLINE, settings->text_outline);
  persist_write_bool(PERSIST_ANIMATE, settings->animate);
  persist_write_bool(PERSIST_VIBE_DISCONNECT, settings->vibe_disconnect);
  persist_write_bool(PERSIST_VIBE_RECONNECT, settings->vibe_reconnect);
  persist_write_int(PERSIST_SERIES_ORDER,
                    (int)pack_series_order(settings->series_order));
  persist_write_int(PERSIST_SERIES_VISIBLE,
                    (int)series_visible_mask(settings->series_visible));
  persist_write_int(PERSIST_BACKGROUND_COLOR, settings->background_color.argb);
  persist_write_int(PERSIST_TRACK_COLOR, settings->track_color.argb);

  for (int index = 0; index < MAX_SERIES; ++index) {
    persist_write_int(s_bar_persist_keys[index],
                      settings->bar_colors[index].argb);
    persist_write_int(s_text_persist_keys[index],
                      settings->text_colors[index].argb);
    persist_write_int(s_text_on_bar_persist_keys[index],
                      settings->text_on_bar_colors[index].argb);
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

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_SERIES_ORDER);
  if (tuple) {
    unpack_series_order(tuple->value->uint32, settings->series_order);
  }

  tuple = dict_find(iterator, MESSAGE_KEY_SETTING_SERIES_VISIBLE);
  if (tuple) {
    apply_visible_mask(tuple->value->uint32, settings->series_visible);
  }

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
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_VIBE_DISCONNECT,
                   &settings->vibe_disconnect);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_VIBE_RECONNECT,
                   &settings->vibe_reconnect);

  apply_color_tuple(iterator, MESSAGE_KEY_SETTING_BACKGROUND_COLOR,
                    &settings->background_color);
  apply_color_tuple(iterator, MESSAGE_KEY_SETTING_TRACK_COLOR,
                    &settings->track_color);
  for (int index = 0; index < MAX_SERIES; ++index) {
    apply_color_tuple(iterator, bar_message_key(index),
                      &settings->bar_colors[index]);
    apply_color_tuple(iterator, text_message_key(index),
                      &settings->text_colors[index]);
    apply_color_tuple(iterator, text_on_bar_message_key(index),
                      &settings->text_on_bar_colors[index]);
  }
}
