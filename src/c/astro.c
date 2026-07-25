#include "astro.h"

#include <string.h>

#define DEGREES_PER_TURN 360.0

// Unix epoch expressed as a Julian day, and the J2000.0 epoch the position
// series below are built on.
#define JULIAN_UNIX_EPOCH 2440587.5
#define JULIAN_J2000 2451545.0

// Altitude of the centre of each body at the moment it counts as risen: the
// sun allows for refraction and its own radius, the moon for refraction less
// its parallax (Meeus, chapter 15). Compared as sines, so no arc sine is
// needed anywhere.
#define SUN_HORIZON_DEGREES (-0.833)
#define MOON_HORIZON_DEGREES (0.125)

// The scan covers the day before and the day after, so the events either side
// of any moment in the cached day are always present.
#define WINDOW_DAYS 3
#define WINDOW_START_OFFSET (-SECONDS_PER_DAY)
#define SCAN_STEP_SECONDS (20 * SECONDS_PER_MINUTE)
// Three days hold at most three rises and three sets of either body.
#define MAX_EVENTS 8
// Half a minute is finer than the face can show, so the crossing search stops
// there rather than iterating to machine precision.
#define BISECTION_LIMIT_SECONDS 30

// Direction cosines of a body in equatorial coordinates, as a unit vector: z is
// the sine of the declination and the length of (x, y) its cosine, which is all
// the altitude test needs.
typedef struct {
  double x;
  double y;
  double z;
} Direction;

typedef void (*DirectionFn)(double julian_day, Direction *direction);

typedef struct {
  bool valid;
  int32_t day;
  int32_t latitude;
  int32_t longitude;
  int count;
  time_t times[MAX_EVENTS];
  bool rises[MAX_EVENTS];
  // Whether the body was already up when the scanned window opened, which is
  // what tells a moment before the first event whether it is up or down.
  bool up_at_start;
} EventCache;

static EventCache s_sun_cache;
static EventCache s_moon_cache;

// Pebble's own lookup tables rather than libm's sin() and cos(). Newlib reduces
// any angle past 3pi/4 through a static table, and the app loader does not fix
// up that table's address, so the read faults on the watch. These are also a
// fraction of the cost of a software double-precision sine.
static double normalize_degrees(double degrees) {
  // All angles produced here are bounded by the 32-bit time_t range, so the
  // number of whole turns comfortably fits in an int32_t. Keeping this local
  // avoids fmod(), whose newlib implementation also reads absolute data that
  // the Pebble app loader can leave unrelocated.
  int32_t whole_turns = (int32_t)(degrees / DEGREES_PER_TURN);
  degrees -= (double)whole_turns * DEGREES_PER_TURN;
  if (degrees < 0.0) {
    degrees += DEGREES_PER_TURN;
  } else if (degrees >= DEGREES_PER_TURN) {
    degrees -= DEGREES_PER_TURN;
  }
  return degrees;
}

static int32_t trig_angle(double degrees) {
  return (int32_t)(normalize_degrees(degrees) *
                   ((double)TRIG_MAX_ANGLE / DEGREES_PER_TURN));
}

static double sine(double degrees) {
  return (double)sin_lookup(trig_angle(degrees)) / (double)TRIG_MAX_RATIO;
}

static double cosine(double degrees) {
  return (double)cos_lookup(trig_angle(degrees)) / (double)TRIG_MAX_RATIO;
}

// Turns ecliptic longitude and latitude into equatorial direction cosines.
static void ecliptic_to_equatorial(double longitude, double latitude,
                                   double obliquity, Direction *direction) {
  double cos_latitude = cosine(latitude);
  double sin_latitude = sine(latitude);
  double cos_longitude = cosine(longitude);
  double sin_longitude = sine(longitude);
  double cos_obliquity = cosine(obliquity);
  double sin_obliquity = sine(obliquity);

  direction->x = cos_latitude * cos_longitude;
  direction->y = cos_obliquity * cos_latitude * sin_longitude -
                 sin_obliquity * sin_latitude;
  direction->z = sin_obliquity * cos_latitude * sin_longitude +
                 cos_obliquity * sin_latitude;
}

static double mean_obliquity(double days) {
  return 23.439 - 0.0000004 * days;
}

// Low-precision solar position, good to about a hundredth of a degree — two
// orders of magnitude tighter than the minute the face resolves.
static void sun_direction(double julian_day, Direction *direction) {
  double days = julian_day - JULIAN_J2000;
  double mean_longitude = 280.460 + 0.9856474 * days;
  double mean_anomaly = 357.528 + 0.9856003 * days;
  double longitude = mean_longitude + 1.915 * sine(mean_anomaly) +
                     0.020 * sine(2.0 * mean_anomaly);

  ecliptic_to_equatorial(longitude, 0.0, mean_obliquity(days), direction);
}

// Abridged lunar theory (Meeus, chapter 47, main terms only). Accurate to a
// few tenths of a degree, which puts rise and set within a couple of minutes.
static void moon_direction(double julian_day, Direction *direction) {
  double days = julian_day - JULIAN_J2000;
  double centuries = days / 36525.0;

  double mean_longitude = 218.316 + 481267.881 * centuries;
  double mean_anomaly = 134.963 + 477198.867 * centuries;
  double sun_anomaly = 357.529 + 35999.050 * centuries;
  double elongation = 297.850 + 445267.115 * centuries;
  double latitude_argument = 93.272 + 483202.018 * centuries;

  double longitude = mean_longitude + 6.289 * sine(mean_anomaly) +
                     1.274 * sine(2.0 * elongation - mean_anomaly) +
                     0.658 * sine(2.0 * elongation) +
                     0.214 * sine(2.0 * mean_anomaly) -
                     0.186 * sine(sun_anomaly) -
                     0.114 * sine(2.0 * latitude_argument);
  double latitude = 5.128 * sine(latitude_argument) +
                    0.281 * sine(mean_anomaly + latitude_argument) -
                    0.278 * sine(latitude_argument - mean_anomaly) -
                    0.173 * sine(2.0 * elongation - latitude_argument);

  ecliptic_to_equatorial(longitude, latitude, mean_obliquity(days), direction);
}

static double julian_day_from_epoch(time_t utc) {
  return (double)utc / (double)SECONDS_PER_DAY + JULIAN_UNIX_EPOCH;
}

// Greenwich mean sidereal time in degrees.
static double greenwich_sidereal_degrees(double julian_day) {
  double days = julian_day - JULIAN_J2000;
  return normalize_degrees(280.46061837 + 360.98564736629 * days);
}

// Sine of the body's altitude. The declination and right ascension never have to
// be recovered as angles: this is the dot product between the body's equatorial
// direction and the observer's zenith direction.
static double sin_altitude(DirectionFn body, time_t utc,
                           double latitude_degrees,
                           double longitude_degrees) {
  double julian_day = julian_day_from_epoch(utc);
  Direction direction;
  body(julian_day, &direction);

  double sidereal =
      greenwich_sidereal_degrees(julian_day) + longitude_degrees;
  double equatorial_projection =
      cosine(sidereal) * direction.x + sine(sidereal) * direction.y;
  return sine(latitude_degrees) * direction.z +
         cosine(latitude_degrees) * equatorial_projection;
}

// Narrows a bracketed horizon crossing down to BISECTION_LIMIT_SECONDS.
static time_t crossing_time(DirectionFn body, time_t before, time_t after,
                            double horizon, double latitude_degrees,
                            double longitude_degrees) {
  bool up_before =
      sin_altitude(body, before, latitude_degrees, longitude_degrees) > horizon;
  while (after - before > BISECTION_LIMIT_SECONDS) {
    time_t middle = before + (after - before) / 2;
    bool up_middle = sin_altitude(body, middle, latitude_degrees,
                                  longitude_degrees) > horizon;
    if (up_middle == up_before) {
      before = middle;
    } else {
      after = middle;
    }
  }
  return before + (after - before) / 2;
}

// Days since 1970-01-01 for a UTC instant, used only to key the cache.
static int32_t utc_day_number(time_t utc) {
  return (int32_t)(utc / SECONDS_PER_DAY) - (utc % SECONDS_PER_DAY < 0 ? 1 : 0);
}

static void build_cache(EventCache *cache, DirectionFn body, double horizon,
                        time_t now, int32_t latitude_micro,
                        int32_t longitude_micro) {
  double latitude_degrees = (double)latitude_micro / 1000000.0;
  double longitude_degrees = (double)longitude_micro / 1000000.0;
  int32_t day = utc_day_number(now);
  time_t window_start =
      (time_t)day * SECONDS_PER_DAY + WINDOW_START_OFFSET;
  time_t window_end = window_start + WINDOW_DAYS * SECONDS_PER_DAY;

  cache->valid = true;
  cache->day = day;
  cache->latitude = latitude_micro;
  cache->longitude = longitude_micro;
  cache->count = 0;

  bool up = sin_altitude(body, window_start, latitude_degrees,
                         longitude_degrees) > horizon;
  cache->up_at_start = up;

  for (time_t step = window_start; step < window_end;
       step += SCAN_STEP_SECONDS) {
    time_t next = step + SCAN_STEP_SECONDS;
    bool up_next =
        sin_altitude(body, next, latitude_degrees, longitude_degrees) > horizon;
    if (up_next != up && cache->count < MAX_EVENTS) {
      cache->times[cache->count] =
          crossing_time(body, step, next, horizon, latitude_degrees,
                        longitude_degrees);
      cache->rises[cache->count] = up_next;
      ++cache->count;
    }
    up = up_next;
  }
}

static void span_from_cache(const EventCache *cache, time_t now,
                            AstroSpan *span) {
  int previous = -1;
  int next = -1;
  for (int index = 0; index < cache->count; ++index) {
    if (cache->times[index] <= now) {
      previous = index;
    } else {
      next = index;
      break;
    }
  }

  if (next < 0) {
    // Nothing ahead in the scanned window: the body neither rises nor sets
    // again within a day either side of now.
    span->up = previous >= 0 ? cache->rises[previous] : cache->up_at_start;
    span->circumpolar = true;
    span->elapsed = 0;
    span->length = 0;
    span->next_event = 0;
    return;
  }

  time_t start = previous >= 0
                     ? cache->times[previous]
                     : (time_t)cache->day * SECONDS_PER_DAY +
                           WINDOW_START_OFFSET;
  span->up = previous >= 0 ? cache->rises[previous] : cache->up_at_start;
  span->circumpolar = false;
  span->next_event = cache->times[next];
  span->length = (int)(span->next_event - start);
  span->elapsed = (int)(now - start);
  if (span->elapsed < 0) {
    span->elapsed = 0;
  }
  if (span->length > 0 && span->elapsed > span->length) {
    span->elapsed = span->length;
  }
}

static void body_span(EventCache *cache, DirectionFn body,
                      double horizon_degrees, time_t now,
                      int32_t latitude_micro, int32_t longitude_micro,
                      AstroSpan *span) {
  if (!cache->valid || cache->day != utc_day_number(now) ||
      cache->latitude != latitude_micro ||
      cache->longitude != longitude_micro) {
    build_cache(cache, body, sine(horizon_degrees), now, latitude_micro,
                longitude_micro);
  }
  span_from_cache(cache, now, span);
}

void astro_sun_span(time_t now, int32_t latitude_micro, int32_t longitude_micro,
                    AstroSpan *span) {
  body_span(&s_sun_cache, sun_direction, SUN_HORIZON_DEGREES, now,
            latitude_micro, longitude_micro, span);
}

void astro_moon_span(time_t now, int32_t latitude_micro,
                     int32_t longitude_micro, AstroSpan *span) {
  body_span(&s_moon_cache, moon_direction, MOON_HORIZON_DEGREES, now,
            latitude_micro, longitude_micro, span);
}
