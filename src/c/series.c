#include "series.h"

#include "languages.h"

#define MONTH_PROGRESS_SCALE 1000

static bool uses_24_hour_clock(const Settings *settings) {
  if (settings->clock_format == CLOCK_FORMAT_24H) {
    return true;
  }
  if (settings->clock_format == CLOCK_FORMAT_12H) {
    return false;
  }
  return clock_is_24h_style();
}

static int days_in_month(const struct tm *time_info) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  int result = days[time_info->tm_mon];
  int year = time_info->tm_year + 1900;
  if (time_info->tm_mon == 1 &&
      ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
    result = 29;
  }
  return result;
}

static int seconds_since_midnight(const struct tm *time_info) {
  return time_info->tm_hour * SECONDS_PER_HOUR +
         time_info->tm_min * SECONDS_PER_MINUTE + time_info->tm_sec;
}

static void initialize_series(Series *series, const Settings *settings,
                              SeriesId id, int value, int maximum,
                              const char *label) {
  *series = (Series){
      .id = id,
      .value = value,
      .maximum = maximum,
      .bar_color = settings->bar_colors[id],
      .text_color = settings->text_colors[id],
      .text_on_bar_color = settings->text_on_bar_colors[id]};
  snprintf(series->label, sizeof(series->label), "%s", label);
}

static void initialize_date_part(Series *series, const Settings *settings,
                                 DatePart part, const struct tm *time_info,
                                 uint8_t language, bool use_full_names) {
  char label[MAX_LABEL_BYTES];
  int day_progress = seconds_since_midnight(time_info);
  switch (part) {
    case DATE_PART_WEEKDAY: {
      int weekday = settings->week_starts_sunday
                        ? time_info->tm_wday + 1
                        : (time_info->tm_wday == 0 ? 7
                                                  : time_info->tm_wday);
      int value = settings->smooth_progress
                      ? (weekday - 1) * SECONDS_PER_DAY + day_progress
                      : weekday;
      int maximum = settings->smooth_progress ? 7 * SECONDS_PER_DAY : 7;
      initialize_series(series, settings, SERIES_DAY, value, maximum,
                        use_full_names
                            ? full_day_names[language][time_info->tm_wday]
                            : day_names[language][time_info->tm_wday]);
      break;
    }
    case DATE_PART_DATE: {
      int month_days = days_in_month(time_info);
      int value = settings->smooth_progress
                      ? (time_info->tm_mday - 1) * SECONDS_PER_DAY +
                            day_progress
                      : time_info->tm_mday;
      int maximum =
          settings->smooth_progress ? month_days * SECONDS_PER_DAY : month_days;
      snprintf(label, sizeof(label), "%02d", time_info->tm_mday);
      initialize_series(series, settings, SERIES_DATE, value, maximum, label);
      break;
    }
    case DATE_PART_MONTH:
    default: {
      int value = time_info->tm_mon + 1;
      int maximum = 12;
      if (settings->smooth_progress) {
        int month_days = days_in_month(time_info);
        int elapsed_month =
            (time_info->tm_mday - 1) * SECONDS_PER_DAY + day_progress;
        int month_fraction =
            (int)((int64_t)elapsed_month * MONTH_PROGRESS_SCALE /
                  (month_days * SECONDS_PER_DAY));
        value = time_info->tm_mon * MONTH_PROGRESS_SCALE + month_fraction;
        maximum = 12 * MONTH_PROGRESS_SCALE;
      }
      initialize_series(series, settings, SERIES_MONTH, value, maximum,
                        use_full_names
                            ? full_month_names[language][time_info->tm_mon]
                            : month_names[language][time_info->tm_mon]);
      break;
    }
  }
}

int series_build(Series output[MAX_SERIES], const Settings *settings) {
  time_t now = time(NULL);
  struct tm time_info = *localtime(&now);
  char label[MAX_LABEL_BYTES];
  uint8_t language =
      settings->language < LANGUAGE_COUNT ? settings->language : LANGUAGE_EN;
  bool use_full_date_names =
      settings->full_date_names &&
      (settings->style == STYLE_HORIZONTAL ||
       settings->style == STYLE_HORIZONTAL_INVERTED);

  int display_hour = time_info.tm_hour;
  if (!uses_24_hour_clock(settings)) {
    display_hour %= 12;
    if (display_hour == 0) {
      display_hour = 12;
    }
  }
  if (settings->leading_zero) {
    snprintf(label, sizeof(label), "%02d", display_hour);
  } else {
    snprintf(label, sizeof(label), "%d", display_hour);
  }
  int hour_value = settings->smooth_progress
                       ? seconds_since_midnight(&time_info)
                       : time_info.tm_hour;
  int hour_maximum = settings->smooth_progress ? SECONDS_PER_DAY : 24;
  initialize_series(&output[0], settings, SERIES_HOUR, hour_value,
                    hour_maximum, label);

  snprintf(label, sizeof(label), "%02d", time_info.tm_min);
  int minute_value =
      settings->smooth_progress
          ? time_info.tm_min * SECONDS_PER_MINUTE + time_info.tm_sec
          : time_info.tm_min;
  int minute_maximum = settings->smooth_progress ? SECONDS_PER_HOUR : 60;
  initialize_series(&output[1], settings, SERIES_MINUTE, minute_value,
                    minute_maximum, label);

  int count = 2;
  if (settings->show_seconds) {
    snprintf(label, sizeof(label), "%02d", time_info.tm_sec);
    initialize_series(&output[count++], settings, SERIES_SECOND,
                      time_info.tm_sec, 60, label);
  }

  for (int index = 0; index < 3; ++index) {
    initialize_date_part(
        &output[count++], settings,
        (DatePart)date_part_order[language][index], &time_info, language,
        use_full_date_names);
  }

  if (settings->show_battery) {
    BatteryChargeState battery = battery_state_service_peek();
    snprintf(label, sizeof(label), "%d%%", battery.charge_percent);
    initialize_series(&output[count++], settings, SERIES_BATTERY,
                      battery.charge_percent, 100, label);
  }
  return count;
}
