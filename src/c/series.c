#include "series.h"

#include "languages.h"

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
                                 uint8_t language) {
  char label[MAX_LABEL_BYTES];
  switch (part) {
    case DATE_PART_WEEKDAY: {
      int weekday = time_info->tm_wday == 0 ? 7 : time_info->tm_wday;
      initialize_series(series, settings, SERIES_DAY, weekday, 7,
                        day_names[language][time_info->tm_wday]);
      break;
    }
    case DATE_PART_DATE:
      snprintf(label, sizeof(label), "%02d", time_info->tm_mday);
      initialize_series(series, settings, SERIES_DATE, time_info->tm_mday,
                        days_in_month(time_info), label);
      break;
    case DATE_PART_MONTH:
    default:
      initialize_series(series, settings, SERIES_MONTH, time_info->tm_mon + 1,
                        12, month_names[language][time_info->tm_mon]);
      break;
  }
}

int series_build(Series output[MAX_SERIES], const Settings *settings) {
  time_t now = time(NULL);
  struct tm time_info = *localtime(&now);
  char label[MAX_LABEL_BYTES];
  uint8_t language =
      settings->language < LANGUAGE_COUNT ? settings->language : LANGUAGE_EN;

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
  initialize_series(&output[0], settings, SERIES_HOUR, time_info.tm_hour, 24,
                    label);

  snprintf(label, sizeof(label), "%02d", time_info.tm_min);
  initialize_series(&output[1], settings, SERIES_MINUTE, time_info.tm_min, 60,
                    label);

  for (int index = 0; index < 3; ++index) {
    initialize_date_part(
        &output[index + 2], settings,
        (DatePart)date_part_order[language][index], &time_info, language);
  }

  int count = 5;
  if (settings->show_seconds) {
    snprintf(label, sizeof(label), "%02d", time_info.tm_sec);
    initialize_series(&output[count++], settings, SERIES_SECOND,
                      time_info.tm_sec, 60, label);
  }
  if (settings->show_battery) {
    BatteryChargeState battery = battery_state_service_peek();
    snprintf(label, sizeof(label), "%d%%", battery.charge_percent);
    initialize_series(&output[count++], settings, SERIES_BATTERY,
                      battery.charge_percent, 100, label);
  }
  return count;
}
