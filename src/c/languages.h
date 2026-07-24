#pragma once

#include <stdint.h>

#define LANGUAGE_EN 0
#define LANGUAGE_COUNT 38

typedef enum {
  DATE_PART_WEEKDAY = 0,
  DATE_PART_DATE,
  DATE_PART_MONTH
} DatePart;

// Language indices intentionally match Solar Earth.
extern const char day_names[LANGUAGE_COUNT][7][8];
extern const char month_names[LANGUAGE_COUNT][12][8];
extern const char *const full_day_names[LANGUAGE_COUNT][7];
extern const char *const full_month_names[LANGUAGE_COUNT][12];

// The natural order of weekday, day-of-month and month for each language.
extern const uint8_t date_part_order[LANGUAGE_COUNT][3];
