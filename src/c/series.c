#include "series.h"

#include <string.h>

#include "astro.h"
#include "languages.h"

#define MONTH_PROGRESS_SCALE 1000
#define DEFAULT_STEP_GOAL 10000
// Stands in for a time the watch cannot work out: no location stored, or a
// body that neither rises nor sets today.
#define UNKNOWN_TIME_LABEL "--:--"
#define SUN_EVENT_MEASURE "SUNRISE 00:00"
#define MOON_EVENT_MEASURE "MOONRISE 00:00"

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

// Days since 1970-01-01 for a civil date, so the custom bar can count whole
// local days without touching the timezone.
static int32_t days_from_civil(int year, int month, int day) {
  year -= month <= 2 ? 1 : 0;
  int32_t era = (year >= 0 ? year : year - 399) / 400;
  int32_t year_of_era = year - era * 400;
  int32_t day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int32_t day_of_era = year_of_era * 365 + year_of_era / 4 -
                       year_of_era / 100 + day_of_year;
  return era * 146097 + day_of_era - 719468;
}

static int display_hour_for(const Settings *settings,
                           const struct tm *time_info) {
  int display_hour = time_info->tm_hour;
  if (!uses_24_hour_clock(settings)) {
    display_hour %= 12;
    if (display_hour == 0) {
      display_hour = 12;
    }
  }
  return display_hour;
}

static void format_local_time(const Settings *settings, time_t utc, char *out,
                              size_t out_size) {
  struct tm local = *localtime(&utc);
  snprintf(out, out_size, "%02d:%02d", display_hour_for(settings, &local),
           local.tm_min);
}

static void initialize_series(Series *series, const Settings *settings,
                              SeriesId id, int value, int maximum,
                              const char *label, const char *measure) {
  *series = (Series){
      .id = id,
      .value = value,
      .maximum = maximum,
      .bar_color = settings->bar_colors[id],
      .track_color = settings->track_colors[id],
      .text_color = settings->text_colors[id]};
  snprintf(series->label, sizeof(series->label), "%s", label);
  snprintf(series->measure, sizeof(series->measure), "%s", measure);
}

static void initialize_hour(Series *series, const Settings *settings,
                            const struct tm *time_info) {
  char label[MAX_LABEL_BYTES];
  int display_hour = display_hour_for(settings, time_info);
  if (settings->leading_zero) {
    snprintf(label, sizeof(label), "%02d", display_hour);
  } else {
    snprintf(label, sizeof(label), "%d", display_hour);
  }

  int value = settings->smooth_progress ? seconds_since_midnight(time_info)
                                        : time_info->tm_hour;
  int maximum = settings->smooth_progress ? SECONDS_PER_DAY : 24;
  initialize_series(series, settings, SERIES_HOUR, value, maximum, label, "00");
}

static void initialize_minute(Series *series, const Settings *settings,
                              const struct tm *time_info) {
  char label[MAX_LABEL_BYTES];
  snprintf(label, sizeof(label), "%02d", time_info->tm_min);
  int value = settings->smooth_progress
                  ? time_info->tm_min * SECONDS_PER_MINUTE + time_info->tm_sec
                  : time_info->tm_min;
  int maximum = settings->smooth_progress ? SECONDS_PER_HOUR : 60;
  initialize_series(series, settings, SERIES_MINUTE, value, maximum, label,
                    "00");
}

// The merged bar takes the hour's slot, colours, and day-long span. Minutes in
// the label add precision without making the bar start over on every hour.
static void initialize_hour_minute(Series *series, const Settings *settings,
                                   const struct tm *time_info) {
  char label[MAX_LABEL_BYTES];
  int display_hour = display_hour_for(settings, time_info);
  if (settings->leading_zero) {
    snprintf(label, sizeof(label), "%02d:%02d", display_hour,
             time_info->tm_min);
  } else {
    snprintf(label, sizeof(label), "%d:%02d", display_hour, time_info->tm_min);
  }

  int value = settings->smooth_progress ? seconds_since_midnight(time_info)
                                        : time_info->tm_hour;
  int maximum = settings->smooth_progress ? SECONDS_PER_DAY : 24;
  initialize_series(series, settings, SERIES_HOUR, value, maximum, label,
                    "00:00");
}

static void initialize_second(Series *series, const Settings *settings,
                              const struct tm *time_info) {
  char label[MAX_LABEL_BYTES];
  snprintf(label, sizeof(label), "%02d", time_info->tm_sec);
  initialize_series(series, settings, SERIES_SECOND, time_info->tm_sec, 60,
                    label, "00");
}

static void initialize_battery(Series *series, const Settings *settings) {
  char label[MAX_LABEL_BYTES];
  BatteryChargeState battery = battery_state_service_peek();
  snprintf(label, sizeof(label), "%d%%", battery.charge_percent);
  initialize_series(series, settings, SERIES_BATTERY, battery.charge_percent,
                    100, label, "100%");
}

static int s_steps;
static bool s_steps_known;

void series_invalidate_steps(void) { s_steps_known = false; }

static int steps_today(void) {
  if (s_steps_known) {
    return s_steps;
  }
  s_steps = 0;
#if defined(PBL_HEALTH)
  HealthServiceAccessibilityMask access = health_service_metric_accessible(
      HealthMetricStepCount, time_start_of_today(), time(NULL));
  if (access & HealthServiceAccessibilityMaskAvailable) {
    HealthValue value = health_service_sum_today(HealthMetricStepCount);
    s_steps = value > 0 ? (int)value : 0;
  }
#endif
  s_steps_known = true;
  return s_steps;
}

static void initialize_steps(Series *series, const Settings *settings) {
  char label[MAX_LABEL_BYTES];
  int steps = steps_today();
  int goal = settings->step_goal > 0 ? settings->step_goal : DEFAULT_STEP_GOAL;
  snprintf(label, sizeof(label), "%d", steps);
  // Step counts vary from one to six digits. Measuring the actual label keeps
  // it readable in both rows and narrow columns instead of always reserving
  // the width of a five-digit placeholder.
  initialize_series(series, settings, SERIES_STEPS, steps, goal, label, label);
}

// Fills `out` from the template, replacing {d} with the days left, {t} with the
// days in the span and {p} with the percentage elapsed. Anything else is copied
// through, so a mistyped token shows on the watch rather than vanishing.
static void expand_custom_label(const char *source, int days_left,
                                int total_days, int percent, char *out,
                                size_t out_size) {
  size_t written = 0;
  size_t index = 0;
  while (source[index] != '\0' && written + 1 < out_size) {
    int value = -1;
    if (source[index] == '{' && source[index + 1] != '\0' &&
        source[index + 2] == '}') {
      switch (source[index + 1]) {
        case 'd':
          value = days_left;
          break;
        case 't':
          value = total_days;
          break;
        case 'p':
          value = percent;
          break;
        default:
          break;
      }
    }

    if (value < 0) {
      out[written++] = source[index++];
      continue;
    }

    int added =
        snprintf(out + written, out_size - written, "%d", value);
    if (added < 0) {
      break;
    }
    written += (size_t)added;
    if (written + 1 > out_size) {
      written = out_size - 1;
      break;
    }
    index += 3;
  }
  out[written] = '\0';
}

static void initialize_custom(Series *series, const Settings *settings,
                              const struct tm *time_info) {
  const char *pattern =
      settings->custom_label[0] != '\0' ? settings->custom_label : "{d}";
  int32_t total_days = settings->custom_target_day - settings->custom_start_day;
  if (total_days <= 0) {
    initialize_series(series, settings, SERIES_CUSTOM, 0, 1, "--", "--");
    return;
  }

  int32_t today = days_from_civil(time_info->tm_year + 1900,
                                  time_info->tm_mon + 1, time_info->tm_mday);
  int32_t elapsed_days = today - settings->custom_start_day;
  int value;
  int maximum;
  if (settings->smooth_progress) {
    maximum = (int)total_days * SECONDS_PER_DAY;
    value = (int)elapsed_days * SECONDS_PER_DAY +
            seconds_since_midnight(time_info);
  } else {
    maximum = (int)total_days;
    value = (int)elapsed_days;
  }
  if (value < 0) {
    value = 0;
  }
  if (value > maximum) {
    value = maximum;
  }

  int days_left = (int)(settings->custom_target_day - today);
  if (days_left < 0) {
    days_left = 0;
  }
  int percent = maximum > 0 ? (int)((int64_t)value * 100 / maximum) : 0;

  char label[MAX_LABEL_BYTES];
  char measure[MAX_LABEL_BYTES];
  expand_custom_label(pattern, days_left, (int)total_days, percent, label,
                      sizeof(label));
  // Measured at the widest the template ever reaches, so the countdown does not
  // grow the text as the numbers shorten.
  expand_custom_label(pattern, (int)total_days, (int)total_days, 100, measure,
                      sizeof(measure));
  initialize_series(series, settings, SERIES_CUSTOM, value, maximum, label,
                    measure);
}

static void initialize_astro(Series *series, const Settings *settings,
                             SeriesId id, time_t now) {
  bool is_moon = id == SERIES_MOON;
  const char *measure = is_moon ? MOON_EVENT_MEASURE : SUN_EVENT_MEASURE;
  if (!settings->location_valid) {
    initialize_series(series, settings, id, 0, 1, UNKNOWN_TIME_LABEL, measure);
    return;
  }

  AstroSpan span;
  if (is_moon) {
    astro_moon_span(now, settings->latitude, settings->longitude, &span);
  } else {
    astro_sun_span(now, settings->latitude, settings->longitude, &span);
  }

  const char *body = is_moon ? "MOON" : "SUN";
  const char *event = span.up ? "SET" : "RISE";
  char label[MAX_LABEL_BYTES];
  if (span.circumpolar || span.length <= 0) {
    // Weeks of unbroken day or night have no progress to show, so the bar sits
    // full or empty and the label still names the body's current state.
    snprintf(label, sizeof(label), "%s %s", body, span.up ? "UP" : "DOWN");
    initialize_series(series, settings, id, span.up ? 1 : 0, 1, label,
                      measure);
  } else {
    char event_time[6];
    format_local_time(settings, span.next_event, event_time,
                      sizeof(event_time));
    snprintf(label, sizeof(label), "%s%s %s", body, event, event_time);
    initialize_series(series, settings, id, span.elapsed, span.length, label,
                      measure);
  }

  if (!span.up) {
    // At a set event, the colour that has just filled the bar becomes its
    // background and the other colour starts filling it. This makes the two
    // alternating stretches continuous instead of flashing back to one colour.
    GColor color = series->bar_color;
    series->bar_color = series->track_color;
    series->track_color = color;
  }
}

static void initialize_date_part(Series *series, const Settings *settings,
                                 SeriesId id, const struct tm *time_info,
                                 uint8_t language, bool use_full_names) {
  char label[MAX_LABEL_BYTES];
  int day_progress = seconds_since_midnight(time_info);
  switch (id) {
    case SERIES_DAY: {
      int weekday = settings->week_starts_sunday
                        ? time_info->tm_wday + 1
                        : (time_info->tm_wday == 0 ? 7
                                                  : time_info->tm_wday);
      int value = settings->smooth_progress
                      ? (weekday - 1) * SECONDS_PER_DAY + day_progress
                      : weekday;
      int maximum = settings->smooth_progress ? 7 * SECONDS_PER_DAY : 7;
      const char *name = use_full_names
                             ? full_day_names[language][time_info->tm_wday]
                             : day_names[language][time_info->tm_wday];
      initialize_series(series, settings, SERIES_DAY, value, maximum, name,
                        name);
      break;
    }
    case SERIES_DATE: {
      int month_days = days_in_month(time_info);
      int value = settings->smooth_progress
                      ? (time_info->tm_mday - 1) * SECONDS_PER_DAY +
                            day_progress
                      : time_info->tm_mday;
      int maximum =
          settings->smooth_progress ? month_days * SECONDS_PER_DAY : month_days;
      snprintf(label, sizeof(label), "%02d", time_info->tm_mday);
      initialize_series(series, settings, SERIES_DATE, value, maximum, label,
                        "00");
      break;
    }
    case SERIES_MONTH:
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
      const char *name = use_full_names
                             ? full_month_names[language][time_info->tm_mon]
                             : month_names[language][time_info->tm_mon];
      initialize_series(series, settings, SERIES_MONTH, value, maximum, name,
                        name);
      break;
    }
  }
}

int series_build(Series output[MAX_SERIES], const Settings *settings) {
  time_t now = time(NULL);
  struct tm time_info = *localtime(&now);
  uint8_t language =
      settings->language < LANGUAGE_COUNT ? settings->language : LANGUAGE_EN;
  bool use_full_date_names =
      settings->full_date_names &&
      (settings->style == STYLE_HORIZONTAL ||
       settings->style == STYLE_HORIZONTAL_INVERTED);
  // Merging needs both bars on: with one of them hidden there is nothing to
  // fold together, and the visible one keeps its own meaning.
  bool merge = settings->merge_hour_minute &&
               settings->series_visible[SERIES_HOUR] &&
               settings->series_visible[SERIES_MINUTE];

  int count = 0;
  for (int slot = 0; slot < MAX_SERIES; ++slot) {
    SeriesId id = (SeriesId)settings->series_order[slot];
    if (id >= MAX_SERIES || !settings->series_visible[id]) {
      continue;
    }
    if (merge && id == SERIES_MINUTE) {
      continue;
    }

    switch (id) {
      case SERIES_HOUR:
        if (merge) {
          initialize_hour_minute(&output[count], settings, &time_info);
        } else {
          initialize_hour(&output[count], settings, &time_info);
        }
        break;
      case SERIES_MINUTE:
        initialize_minute(&output[count], settings, &time_info);
        break;
      case SERIES_SECOND:
        initialize_second(&output[count], settings, &time_info);
        break;
      case SERIES_BATTERY:
        initialize_battery(&output[count], settings);
        break;
      case SERIES_STEPS:
        initialize_steps(&output[count], settings);
        break;
      case SERIES_CUSTOM:
        initialize_custom(&output[count], settings, &time_info);
        break;
      case SERIES_DAYLIGHT:
      case SERIES_MOON:
        initialize_astro(&output[count], settings, id, now);
        break;
      default:
        initialize_date_part(&output[count], settings, id, &time_info,
                             language, use_full_date_names);
        break;
    }
    ++count;
  }
  return count;
}
