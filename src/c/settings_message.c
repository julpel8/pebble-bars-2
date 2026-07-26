#include "settings.h"

#include <stdio.h>

#include "languages.h"
#include "settings_internal.h"

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
