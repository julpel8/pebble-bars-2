#include "settings.h"

#include "languages.h"

enum {
  PERSIST_STYLE = 100,
  PERSIST_CLOCK_FORMAT,
  PERSIST_LEADING_ZERO,
  PERSIST_SHOW_SECONDS,
  PERSIST_SHOW_BATTERY,
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
  PERSIST_LANGUAGE
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
      .text_placement = TEXT_PLACE_OUTSIDE_EDGE,
      .clock_format = CLOCK_FORMAT_SYSTEM,
      .language = LANGUAGE_EN,
      .leading_zero = false,
      .show_seconds = false,
      .show_battery = false,
      .seamless = true,
      .text_outline = true,
      .animate = true,
      .vibe_disconnect = true,
      .vibe_reconnect = false,
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
              GColorFromHEX(0x00FFFF),
          },
      .text_colors =
          {
              GColorFromHEX(0xAAFF00),
              GColorFromHEX(0x00FF00),
              GColorFromHEX(0x0055FF),
              GColorFromHEX(0xFFFF00),
              GColorFromHEX(0xFF0000),
              GColorFromHEX(0xAA00FF),
              GColorFromHEX(0x00FFFF),
          },
      .text_on_bar_colors =
          {
              GColorWhite, GColorWhite, GColorWhite, GColorWhite,
              GColorWhite, GColorWhite, GColorWhite,
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
  if (persist_exists(PERSIST_LEADING_ZERO)) {
    settings->leading_zero = persist_read_bool(PERSIST_LEADING_ZERO);
  }
  if (persist_exists(PERSIST_SHOW_SECONDS)) {
    settings->show_seconds = persist_read_bool(PERSIST_SHOW_SECONDS);
  }
  if (persist_exists(PERSIST_SHOW_BATTERY)) {
    settings->show_battery = persist_read_bool(PERSIST_SHOW_BATTERY);
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
  persist_write_bool(PERSIST_LEADING_ZERO, settings->leading_zero);
  persist_write_bool(PERSIST_SHOW_SECONDS, settings->show_seconds);
  persist_write_bool(PERSIST_SHOW_BATTERY, settings->show_battery);
  persist_write_bool(PERSIST_SEAMLESS, settings->seamless);
  persist_write_bool(PERSIST_TEXT_OUTLINE, settings->text_outline);
  persist_write_bool(PERSIST_ANIMATE, settings->animate);
  persist_write_bool(PERSIST_VIBE_DISCONNECT, settings->vibe_disconnect);
  persist_write_bool(PERSIST_VIBE_RECONNECT, settings->vibe_reconnect);
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

  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_LEADING_ZERO,
                   &settings->leading_zero);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_SHOW_SECONDS,
                   &settings->show_seconds);
  apply_bool_tuple(iterator, MESSAGE_KEY_SETTING_SHOW_BATTERY,
                   &settings->show_battery);
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
