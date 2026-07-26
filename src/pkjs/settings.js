var STORAGE_KEY = 'bars2Settings';

// Index = SeriesId in src/c/bars_types.h. SETTING_SERIES_ORDER lists these ids
// in display order and SETTING_SERIES_VISIBLE is indexed by them, so the two
// tables must stay in sync.
var SERIES = [
  {label: 'Hour', bar: 'SETTING_HOUR_BAR_COLOR',
   track: 'SETTING_HOUR_TRACK_COLOR', text: 'SETTING_HOUR_TEXT_COLOR'},
  {label: 'Minute', bar: 'SETTING_MINUTE_BAR_COLOR',
   track: 'SETTING_MINUTE_TRACK_COLOR', text: 'SETTING_MINUTE_TEXT_COLOR'},
  {label: 'Month', bar: 'SETTING_MONTH_BAR_COLOR',
   track: 'SETTING_MONTH_TRACK_COLOR', text: 'SETTING_MONTH_TEXT_COLOR'},
  {label: 'Date', bar: 'SETTING_DATE_BAR_COLOR',
   track: 'SETTING_DATE_TRACK_COLOR', text: 'SETTING_DATE_TEXT_COLOR'},
  {label: 'Weekday', bar: 'SETTING_DAY_BAR_COLOR',
   track: 'SETTING_DAY_TRACK_COLOR', text: 'SETTING_DAY_TEXT_COLOR'},
  {label: 'Seconds', bar: 'SETTING_SECOND_BAR_COLOR',
   track: 'SETTING_SECOND_TRACK_COLOR', text: 'SETTING_SECOND_TEXT_COLOR'},
  {label: 'Battery', bar: 'SETTING_BATTERY_BAR_COLOR',
   track: 'SETTING_BATTERY_TRACK_COLOR', text: 'SETTING_BATTERY_TEXT_COLOR'},
  {label: 'Daylight / night', bar: 'SETTING_DAYLIGHT_BAR_COLOR',
   track: 'SETTING_DAYLIGHT_TRACK_COLOR', text: 'SETTING_DAYLIGHT_TEXT_COLOR'},
  {label: 'Moon', bar: 'SETTING_MOON_BAR_COLOR',
   track: 'SETTING_MOON_TRACK_COLOR', text: 'SETTING_MOON_TEXT_COLOR'},
  {label: 'Steps', bar: 'SETTING_STEPS_BAR_COLOR',
   track: 'SETTING_STEPS_TRACK_COLOR', text: 'SETTING_STEPS_TEXT_COLOR'},
  {label: 'Custom', bar: 'SETTING_CUSTOM_BAR_COLOR',
   track: 'SETTING_CUSTOM_TRACK_COLOR', text: 'SETTING_CUSTOM_TEXT_COLOR'}
];

// Ported from date_part_order in src/c/languages.c (W weekday, D date,
// M month). Only used to rebuild the list for installs saved before the
// series list existed; afterwards the saved order wins.
var DATE_ORDER = 'WDM';
var DATE_ORDER_BY_LANGUAGE = {
  0: 'WMD',   // English (US)
  6: 'DMW',   // Turkish
  24: 'MDW',  // Hungarian
  29: 'MDW',  // Chinese
  34: 'MDW',  // Japanese
  35: 'MDW'   // Korean
};
var DATE_PART_SERIES = {W: 4, D: 3, M: 2};

var PALETTE_LEVELS = ['00', '55', 'AA', 'FF'];

// Mirrors darker_color() in src/c/settings.c: the palette has four levels per
// channel, so halving the level index is the darkest step that still keeps a
// colour recognisably itself.
function darkerHex(value) {
  var source = String(value || '000000').replace('#', '');
  var result = '';
  for (var index = 0; index < 3; index++) {
    var channel = parseInt(source.substr(index * 2, 2), 16);
    var level = Math.round((isNaN(channel) ? 0 : channel) / 85);
    result += PALETTE_LEVELS[Math.floor(level / 2)];
  }
  return result;
}

var DEFAULTS = {
  SETTING_STYLE: 0,
  SETTING_TEXT_PLACEMENT: 2,
  SETTING_ROUND_POLAR_FILL: 0,
  SETTING_CLOCK_FORMAT: 0,
  SETTING_LANGUAGE: 0,
  SETTING_CLOCK_REFRESH: 60,
  SETTING_LEADING_ZERO: 1,
  SETTING_SMOOTH_PROGRESS: 1,
  // Display order (SeriesId values) and per-series visibility, the latter
  // indexed by SeriesId rather than by position.
  SETTING_SERIES_ORDER: [0, 1, 5, 4, 2, 3, 6, 7, 8, 9, 10],
  SETTING_SERIES_VISIBLE: [1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0],
  SETTING_FULL_DATE_NAMES: 0,
  SETTING_WEEK_STARTS_SUNDAY: 0,
  SETTING_SEAMLESS_BARS: 1,
  SETTING_TEXT_OUTLINE: 0,
  SETTING_ANIMATE: 1,
  SETTING_MERGE_HOUR_MINUTE: 0,
  SETTING_STEP_GOAL: 10000,
  SETTING_CUSTOM_LABEL: '{d}',
  SETTING_CUSTOM_START_DAY: 0,
  SETTING_CUSTOM_TARGET_DAY: 0,
  // Microdegrees, filled in from the phone unless the coordinates are typed by
  // hand. The watch needs them for the daylight and moon bars.
  SETTING_USE_PHONE_LOCATION: 1,
  SETTING_LATITUDE: 0,
  SETTING_LONGITUDE: 0,
  SETTING_LOCATION_VALID: 0,
  SETTING_BACKGROUND_COLOR: '000000',
  SETTING_HOUR_BAR_COLOR: '00FF00',
  SETTING_HOUR_TEXT_COLOR: 'FFFFFF',
  SETTING_MINUTE_BAR_COLOR: '00AA55',
  SETTING_MINUTE_TEXT_COLOR: 'FFFFFF',
  SETTING_MONTH_BAR_COLOR: '0055FF',
  SETTING_MONTH_TEXT_COLOR: 'FFFFFF',
  SETTING_DATE_BAR_COLOR: 'FFFF00',
  SETTING_DATE_TEXT_COLOR: 'FFFFFF',
  SETTING_DAY_BAR_COLOR: 'FF0000',
  SETTING_DAY_TEXT_COLOR: 'FFFFFF',
  SETTING_SECOND_BAR_COLOR: 'AA00FF',
  SETTING_SECOND_TEXT_COLOR: 'FFFFFF',
  SETTING_BATTERY_BAR_COLOR: '00FFFF',
  SETTING_BATTERY_TEXT_COLOR: 'FFFFFF',
  SETTING_DAYLIGHT_BAR_COLOR: 'FFAA00',
  SETTING_DAYLIGHT_TEXT_COLOR: 'FFFFFF',
  SETTING_MOON_BAR_COLOR: 'AAAAFF',
  SETTING_MOON_TEXT_COLOR: 'FFFFFF',
  SETTING_STEPS_BAR_COLOR: '55FF00',
  SETTING_STEPS_TEXT_COLOR: 'FFFFFF',
  SETTING_CUSTOM_BAR_COLOR: 'FF00AA',
  SETTING_CUSTOM_TEXT_COLOR: 'FFFFFF'
};

// Every bar starts on a much darker shade of its own colour.
SERIES.forEach(function (series) {
  DEFAULTS[series.track] = darkerHex(DEFAULTS[series.bar]);
});

// Kept in phone storage but never sent: the watch has no use for it, and an
// undeclared key would fail the whole message.
var PHONE_ONLY_KEYS = {SETTING_USE_PHONE_LOCATION: true};

function copyObject(source) {
  var result = {};
  Object.keys(source || {}).forEach(function (key) {
    var value = source[key];
    result[key] = Array.isArray(value) ? value.slice() : value;
  });
  return result;
}

// Drops ids that are out of range or repeated and appends whatever is missing,
// so a truncated or corrupt saved order still yields every series.
function normalizedOrder(order) {
  var source = Array.isArray(order) ? order : [];
  var result = [];
  var seen = {};
  source.forEach(function (value) {
    var id = Number(value);
    if (id >= 0 && id < SERIES.length && !seen[id]) {
      seen[id] = true;
      result.push(id);
    }
  });
  SERIES.forEach(function (series, id) {
    if (!seen[id]) {
      result.push(id);
    }
  });
  return result;
}

// Hiding everything would leave a blank watchface, so an empty selection falls
// back to the defaults.
function normalizedVisible(visible) {
  var source = Array.isArray(visible) ? visible : [];
  var result = SERIES.map(function (series, id) {
    return Number(source[id]) ? 1 : 0;
  });
  var shown = result.some(function (value) {
    return value === 1;
  });
  return shown ? result : DEFAULTS.SETTING_SERIES_VISIBLE.slice();
}

// Before the series list, the layout came from the language's date order plus
// the two show/hide toggles. Rebuild it once so an upgrade looks unchanged.
function migrateSeriesLayout(result, saved) {
  // Nothing saved at all is a fresh install, not an upgrade: leave the
  // defaults alone rather than deriving them from absent legacy toggles.
  if (saved.SETTING_SERIES_ORDER || Object.keys(saved).length === 0) {
    return;
  }

  var language = Number(saved.SETTING_LANGUAGE) || 0;
  var pattern = DATE_ORDER_BY_LANGUAGE[language] || DATE_ORDER;
  var order = [0, 1, 5];
  pattern.split('').forEach(function (part) {
    order.push(DATE_PART_SERIES[part]);
  });
  order.push(6);

  var visible = DEFAULTS.SETTING_SERIES_VISIBLE.slice();
  visible[5] = Number(saved.SETTING_SHOW_SECONDS) ? 1 : 0;
  visible[6] = Number(saved.SETTING_SHOW_BATTERY) ? 1 : 0;

  result.SETTING_SERIES_ORDER = order;
  result.SETTING_SERIES_VISIBLE = visible;
}

function mergedSettings(saved) {
  var result = copyObject(DEFAULTS);
  var source = saved || {};
  // Only known keys are kept: retired ones left in storage would otherwise be
  // sent to a watch whose manifest no longer declares them, failing the whole
  // message.
  Object.keys(source).forEach(function (key) {
    if (DEFAULTS.hasOwnProperty(key)) {
      result[key] = Array.isArray(source[key]) ? source[key].slice()
                                               : source[key];
    }
  });
  // The former filled-bar text colour is now the series' only text colour.
  // Prefer it once when upgrading a configuration that still has both roles.
  SERIES.forEach(function (series) {
    var legacyKey = series.text.replace('_TEXT_COLOR',
                                        '_TEXT_ON_BAR_COLOR');
    if (source.hasOwnProperty(legacyKey)) {
      result[series.text] = source[legacyKey];
    }
  });

  migrateSeriesLayout(result, source);
  result.SETTING_SERIES_ORDER = normalizedOrder(result.SETTING_SERIES_ORDER);
  result.SETTING_SERIES_VISIBLE =
      normalizedVisible(result.SETTING_SERIES_VISIBLE);
  return result;
}

function loadSettings() {
  try {
    return mergedSettings(JSON.parse(localStorage.getItem(STORAGE_KEY) || '{}'));
  } catch (error) {
    return mergedSettings();
  }
}

function saveSettings(settings) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch (error) {
    console.log('Unable to persist Bars 2 settings: ' + error);
  }
}

function seriesVisibleMask(visible) {
  var mask = 0;
  visible.forEach(function (shown, id) {
    if (shown) {
      mask |= 1 << id;
    }
  });
  return mask;
}

function messagePayload(settings) {
  var payload = {};
  // Iterates the defaults, not the settings: anything the manifest does not
  // declare must never reach sendAppMessage().
  Object.keys(DEFAULTS).forEach(function (key) {
    if (PHONE_ONLY_KEYS[key]) {
      return;
    }
    var value = settings[key];
    if (key === 'SETTING_SERIES_ORDER') {
      // One byte per series: eleven of them no longer fit in the nibbles of a
      // single integer.
      payload[key] = normalizedOrder(value);
    } else if (key === 'SETTING_SERIES_VISIBLE') {
      payload[key] = seriesVisibleMask(normalizedVisible(value));
    } else if (key === 'SETTING_CUSTOM_LABEL') {
      payload[key] = String(value == null ? '' : value);
    } else if (/_COLOR$/.test(key)) {
      payload[key] = parseInt(String(value).replace('#', ''), 16);
    } else {
      payload[key] = parseInt(value, 10) || 0;
    }
  });
  return payload;
}

function sendSettings(settings) {
  Pebble.sendAppMessage(
    messagePayload(settings),
    function () {
      console.log('Bars 2 settings sent');
    },
    function (error) {
      console.log('Unable to send Bars 2 settings: ' + JSON.stringify(error));
    }
  );
}

// The watch has no receiver of its own, so the daylight and moon bars work from
// whatever fix was last stored. A refusal or a timeout leaves the previous one
// in place rather than blanking the bars.
function refreshLocation(settings) {
  if (!Number(settings.SETTING_USE_PHONE_LOCATION)) {
    return;
  }
  if (typeof navigator === 'undefined' || !navigator.geolocation) {
    return;
  }
  navigator.geolocation.getCurrentPosition(
    function (position) {
      settings.SETTING_LATITUDE = Math.round(position.coords.latitude * 1000000);
      settings.SETTING_LONGITUDE =
          Math.round(position.coords.longitude * 1000000);
      settings.SETTING_LOCATION_VALID = 1;
      saveSettings(settings);
      sendSettings(settings);
    },
    function (error) {
      console.log('Bars 2 location unavailable: ' + error.message);
    },
    {timeout: 15000, maximumAge: 6 * 60 * 60 * 1000}
  );
}

module.exports = {
  DEFAULTS: DEFAULTS,
  SERIES: SERIES,
  loadSettings: loadSettings,
  mergedSettings: mergedSettings,
  refreshLocation: refreshLocation,
  saveSettings: saveSettings,
  sendSettings: sendSettings
};
