var STORAGE_KEY = 'bars2Settings';

// Index = SeriesId in src/c/bars_types.h. SETTING_SERIES_ORDER lists these ids
// in display order and SETTING_SERIES_VISIBLE is indexed by them, so the two
// tables must stay in sync.
var SERIES = [
  {label: 'Hour', bar: 'SETTING_HOUR_BAR_COLOR',
   text: 'SETTING_HOUR_TEXT_COLOR', onBar: 'SETTING_HOUR_TEXT_ON_BAR_COLOR'},
  {label: 'Minute', bar: 'SETTING_MINUTE_BAR_COLOR',
   text: 'SETTING_MINUTE_TEXT_COLOR', onBar: 'SETTING_MINUTE_TEXT_ON_BAR_COLOR'},
  {label: 'Month', bar: 'SETTING_MONTH_BAR_COLOR',
   text: 'SETTING_MONTH_TEXT_COLOR', onBar: 'SETTING_MONTH_TEXT_ON_BAR_COLOR'},
  {label: 'Date', bar: 'SETTING_DATE_BAR_COLOR',
   text: 'SETTING_DATE_TEXT_COLOR', onBar: 'SETTING_DATE_TEXT_ON_BAR_COLOR'},
  {label: 'Weekday', bar: 'SETTING_DAY_BAR_COLOR',
   text: 'SETTING_DAY_TEXT_COLOR', onBar: 'SETTING_DAY_TEXT_ON_BAR_COLOR'},
  {label: 'Seconds', bar: 'SETTING_SECOND_BAR_COLOR',
   text: 'SETTING_SECOND_TEXT_COLOR', onBar: 'SETTING_SECOND_TEXT_ON_BAR_COLOR'},
  {label: 'Battery', bar: 'SETTING_BATTERY_BAR_COLOR',
   text: 'SETTING_BATTERY_TEXT_COLOR',
   onBar: 'SETTING_BATTERY_TEXT_ON_BAR_COLOR'}
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

var DEFAULTS = {
  SETTING_STYLE: 0,
  SETTING_TEXT_PLACEMENT: 2,
  SETTING_CLOCK_FORMAT: 0,
  SETTING_LANGUAGE: 0,
  SETTING_CLOCK_REFRESH: 60,
  SETTING_LEADING_ZERO: 1,
  SETTING_SMOOTH_PROGRESS: 1,
  // Display order (SeriesId values) and per-series visibility, the latter
  // indexed by SeriesId rather than by position.
  SETTING_SERIES_ORDER: [0, 1, 5, 4, 2, 3, 6],
  SETTING_SERIES_VISIBLE: [1, 1, 1, 1, 1, 0, 0],
  SETTING_FULL_DATE_NAMES: 0,
  SETTING_WEEK_STARTS_SUNDAY: 0,
  SETTING_SEAMLESS_BARS: 1,
  SETTING_TEXT_OUTLINE: 0,
  SETTING_ANIMATE: 1,
  SETTING_VIBE_DISCONNECT: 1,
  SETTING_VIBE_RECONNECT: 0,
  SETTING_BACKGROUND_COLOR: '000000',
  SETTING_TRACK_COLOR: '000000',
  SETTING_HOUR_BAR_COLOR: '00FF00',
  SETTING_HOUR_TEXT_COLOR: '00FF00',
  SETTING_MINUTE_BAR_COLOR: '00AA55',
  SETTING_MINUTE_TEXT_COLOR: '00AA55',
  SETTING_MONTH_BAR_COLOR: '0055FF',
  SETTING_MONTH_TEXT_COLOR: '0055FF',
  SETTING_DATE_BAR_COLOR: 'FFFF00',
  SETTING_DATE_TEXT_COLOR: 'FFFF00',
  SETTING_DAY_BAR_COLOR: 'FF0000',
  SETTING_DAY_TEXT_COLOR: 'FF0000',
  SETTING_SECOND_BAR_COLOR: 'AA00FF',
  SETTING_SECOND_TEXT_COLOR: 'AA00FF',
  SETTING_BATTERY_BAR_COLOR: '00FFFF',
  SETTING_BATTERY_TEXT_COLOR: 'FFFFFF',
  SETTING_HOUR_TEXT_ON_BAR_COLOR: 'AAFFAA',
  SETTING_MINUTE_TEXT_ON_BAR_COLOR: '55FFFF',
  SETTING_MONTH_TEXT_ON_BAR_COLOR: '55AAFF',
  SETTING_DATE_TEXT_ON_BAR_COLOR: '555500',
  SETTING_DAY_TEXT_ON_BAR_COLOR: 'FFAAAA',
  SETTING_SECOND_TEXT_ON_BAR_COLOR: 'FFAAFF',
  SETTING_BATTERY_TEXT_ON_BAR_COLOR: 'FFFFFF'
};

// Keep this order in sync with src/c/languages.h and Solar Earth's language
// setting so a saved language index has the same meaning in both watchfaces.
var CONFIG_LANGUAGES = [
  'English', 'French', 'German', 'Spanish', 'Italian', 'Dutch',
  'Turkish', 'Czech', 'Portuguese', 'Greek', 'Swedish', 'Polish', 'Slovak',
  'Vietnamese', 'Romanian', 'Catalan', 'Norwegian', 'Russian', 'Estonian',
  'Basque', 'Finnish', 'Danish', 'Lithuanian', 'Slovenian', 'Hungarian',
  'Croatian', 'Irish', 'Latvian', 'Serbian', 'Chinese', 'Indonesian',
  'Ukrainian', 'Welsh', 'Galician', 'Japanese', 'Korean', 'Hebrew',
  'English (UK)'
];

function copyObject(source) {
  var result = {};
  Object.keys(source || {}).forEach(function (key) {
    var value = source[key];
    result[key] = Array.isArray(value) ? value.slice() : value;
  });
  return result;
}

// Drops ids that are out of range or repeated and appends whatever is missing,
// so a truncated or corrupt saved order still yields all seven series.
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

// The order travels as one integer, series id 0 in the most significant
// nibble, matching unpack_series_order() in src/c/settings.c.
function packSeriesOrder(order) {
  var packed = 0;
  order.forEach(function (id) {
    packed = packed * 16 + id;
  });
  return packed;
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
    var value = settings[key];
    if (key === 'SETTING_SERIES_ORDER') {
      payload[key] = packSeriesOrder(normalizedOrder(value));
    } else if (key === 'SETTING_SERIES_VISIBLE') {
      payload[key] = seriesVisibleMask(normalizedVisible(value));
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

function colorFieldHtml(key, label) {
  // Mount point for the swatch picker built at runtime (see buildSwatch()).
  // Uses data-color-key (not data-key) so it stays out of the generic
  // checkbox/select collect()/apply() loop.
  return '<div class="color-field" data-color-key="' + key +
         '" data-label="' + label + '"></div>';
}

function safeJson(value) {
  return JSON.stringify(value).replace(/</g, '\\u003c');
}

function configurationHtml(settings) {
  var languageOptions = [];
  CONFIG_LANGUAGES.forEach(function (language, index) {
    languageOptions.push('<option value="' + index + '">' + language + '</option>');
  });

  return [
    '<!doctype html><html lang="en"><head>',
    '<meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">',
    '<title>Bars 2</title>',
    '<style>',
    '*{box-sizing:border-box}body{margin:0;background:#101113;color:#f6f6f6;',
    'font:16px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}',
    'header{padding:24px 18px 18px;background:#08090a;border-bottom:1px solid #292b2f}',
    'h1{font-size:28px;letter-spacing:2px;margin:0;color:#aaff55}',
    'header p{color:#a8abb1;margin:7px 0 0;font-size:14px}',
    'main{padding:0 14px 110px;max-width:620px;margin:auto}',
    'section{margin-top:16px;background:#1a1c20;border:1px solid #303238;',
    'border-radius:12px;overflow:hidden}',
    'h2{font-size:13px;letter-spacing:1.2px;text-transform:uppercase;',
    'color:#8d929a;margin:0;padding:13px 15px;background:#15171a}',
    '.row,.color-row{min-height:52px;padding:10px 15px;display:flex;',
    'align-items:center;justify-content:space-between;border-top:1px solid #2a2d32;gap:16px}',
    'h2+.row,h2+.color-row{border-top:0}.row span,.color-row span{line-height:1.2}',
    'select{max-width:55%;background:#26292e;color:#fff;border:1px solid #444850;',
    'border-radius:7px;padding:8px;font-size:15px}',
    'input[type=checkbox]{width:24px;height:24px;accent-color:#55d57f}',
    '.color-field{padding:8px 15px;border-top:1px solid #2a2d32}',
    'h2+.color-field{border-top:0}',
    '.si{border-top:1px solid #2a2d32;background:#1a1c20}',
    '#list>.si:first-child{border-top:0}',
    '.si-head{display:flex;align-items:center;gap:12px;padding:9px 14px;',
    'min-height:52px}',
    '.si-handle{flex:0 0 26px;align-self:stretch;display:flex;',
    'align-items:center;justify-content:center;color:#8d929a;font-size:20px;',
    'cursor:grab;touch-action:none;user-select:none;-webkit-user-select:none;',
    '-webkit-touch-callout:none;margin:-9px 0 -9px -6px;padding:0 4px}',
    '.si-dot{flex:0 0 20px;height:20px;border-radius:50%;',
    'border:1px solid #444850}',
    '.si-name{flex:1;line-height:1.2}',
    '.si-chev{flex:0 0 14px;color:#8d929a;font-size:12px;text-align:center;',
    'transition:transform .15s}',
    '.si.open .si-chev{transform:rotate(90deg)}',
    '.si-body{padding:0 11px 8px}.si-body.collapsed{display:none}',
    '.si-body .color-field{padding:8px 4px}',
    '.si-body>.color-field:first-child{border-top:0}',
    '.si.off .si-name,.si.off .si-dot{opacity:.4}',
    '.si.dragging{position:relative;z-index:5;background:#24272c;',
    'border-radius:10px;box-shadow:0 8px 20px rgba(0,0,0,.55)}',
    '.si.dragging .si-handle{cursor:grabbing}',
    '.si.flash{animation:flash .5s ease-out}',
    '@keyframes flash{0%,100%{background:#1a1c20}50%{background:#4a2f2f}}',
    '.cc-head{display:flex;align-items:center;justify-content:space-between;',
    'gap:12px;min-height:44px}.cc-head span{line-height:1.2}',
    '.swatch-btn{width:54px;height:34px;flex:0 0 54px;border-radius:7px;',
    'border:1px solid #444850;padding:0;cursor:pointer}',
    '.swatches{display:grid;grid-template-columns:repeat(8,1fr);gap:6px;',
    'margin:10px 0 2px}',
    '.swatch{aspect-ratio:1;border-radius:6px;border:2px solid #2a2d32;',
    'cursor:pointer;box-sizing:border-box}',
    '.swatch.sel{border-color:#fff;box-shadow:0 0 0 2px #aaff55}',
    '.swatches.collapsed{display:none}',
    '.actions{position:fixed;left:0;right:0;bottom:0;background:rgba(8,9,10,.95);',
    'padding:13px 14px calc(13px + env(safe-area-inset-bottom));display:flex;',
    'gap:10px;border-top:1px solid #303238}',
    'button{min-height:48px;border:0;border-radius:10px;font-weight:700;font-size:16px}',
    '#reset{flex:1;background:#30333a;color:#fff}#save{flex:2;background:#aaff55;color:#101113}',
    '.hint{padding:0 15px 14px;color:#898e96;font-size:13px;line-height:1.35}',
    '.cfm{position:fixed;inset:0;background:rgba(0,0,0,.65);display:none;',
    'align-items:center;justify-content:center;padding:22px;z-index:60}',
    '.cfm.open{display:flex}',
    '.cfm-box{background:#1a1c20;border:1px solid #3a3d44;border-radius:14px;',
    'padding:20px;max-width:340px;width:100%}',
    '.cfm-box p{margin:0 0 18px;font-size:16px;line-height:1.3}',
    '.cfm-row{display:flex;gap:10px}',
    '.cfm-row button{flex:1;min-height:46px;border:0;border-radius:10px;',
    'font-weight:700;font-size:15px}',
    '#cfmCancel{background:#30333a;color:#fff}#cfmOk{background:#aaff55;color:#101113}',
    '</style></head><body>',
    '<header><h1>BARS 2</h1><p>Native Pebble Time 2 settings</p></header>',
    '<main>',
    '<section><h2>Layout</h2>',
    '<label class="row"><span>Bar style</span><select data-key="SETTING_STYLE">',
    '<option value="0">Horizontal</option>',
    '<option value="1">Horizontal inverted</option>',
    '<option value="2">Vertical</option>',
    '<option value="3">Vertical inverted</option>',
    '<option value="4">Polar</option>',
    '<option value="5">Polar inverted</option>',
    '</select></label>',
    '<p class="hint">Polar nests the bars as rectangular rings that fit the '
      + 'screen: the first bar in the list below is the outer ring, and each '
      + 'one fills from twelve o’clock — clockwise, or anticlockwise when '
      + 'inverted. Labels sit in the top band of their ring.</p>',
    '<label class="row"><span>Text placement</span>',
    '<select data-key="SETTING_TEXT_PLACEMENT">',
    '<option value="1">Outside — start</option>',
    '<option value="5">Outside — middle</option>',
    '<option value="0">Outside — end</option>',
    '<option value="2">Inside — start</option>',
    '<option value="3">Inside — middle</option>',
    '<option value="4">Inside — end</option>',
    '<option value="6">Always — bar middle</option>',
    '</select></label>',
    '<p class="hint">Placement applies to horizontal and vertical styles. '
      + 'Text uses the “on track” colour beside the fill and the “on bar” '
      + 'colour over the fill. “Always” fixes every label at the centre of '
      + 'its complete bar.</p>',
    '<label class="row"><span>Black text outline</span>',
    '<input type="checkbox" data-key="SETTING_TEXT_OUTLINE"></label>',
    '<label class="row"><span>Seamless bars (no gaps)</span>',
    '<input type="checkbox" data-key="SETTING_SEAMLESS_BARS"></label>',
    '<label class="row"><span>Launch animation</span>',
    '<input type="checkbox" data-key="SETTING_ANIMATE"></label>',
    '</section>',
    '<section><h2>Information</h2>',
    '<label class="row"><span>Date language</span>',
    '<select data-key="SETTING_LANGUAGE">', languageOptions.join(''),
    '</select></label>',
    '<p class="hint">Localizes weekday and month names and uses the natural '
      + 'weekday, date and month order for the selected language.</p>',
    '<label class="row"><span>Full weekday/month names</span>',
    '<input type="checkbox" data-key="SETTING_FULL_DATE_NAMES"></label>',
    '<p class="hint">Uses complete weekday and month names in horizontal '
      + 'styles. Vertical styles keep the compact abbreviations.</p>',
    '<label class="row"><span>Week starts on Sunday</span>',
    '<input type="checkbox" data-key="SETTING_WEEK_STARTS_SUNDAY"></label>',
    '<p class="hint">Changes weekday bar progress to Sunday through Saturday. '
      + 'Leave off for Monday through Sunday.</p>',
    '<label class="row"><span>Time format</span>',
    '<select data-key="SETTING_CLOCK_FORMAT">',
    '<option value="0">Watch setting</option>',
    '<option value="1">24-hour</option>',
    '<option value="2">12-hour</option>',
    '</select></label>',
    '<label class="row"><span>Clock refresh</span>',
    '<select data-key="SETTING_CLOCK_REFRESH">',
    '<option value="1">Every second</option>',
    '<option value="5">Every 5 seconds</option>',
    '<option value="10">Every 10 seconds</option>',
    '<option value="20">Every 20 seconds</option>',
    '<option value="30">Every 30 seconds</option>',
    '<option value="60">Every 60 seconds</option>',
    '</select></label>',
    '<p class="hint">Shorter intervals update the progress bars and seconds '
      + 'more smoothly, but use more battery.</p>',
    '<label class="row"><span>Smooth bar progress</span>',
    '<input type="checkbox" data-key="SETTING_SMOOTH_PROGRESS"></label>',
    '<p class="hint">Turn this off for exact whole-unit steps: 24 hours, '
      + '60 minutes, 12 months, the actual days in the month, and 7 weekdays.</p>',
    '<label class="row"><span>Leading zero for hour</span>',
    '<input type="checkbox" data-key="SETTING_LEADING_ZERO"></label>',
    '</section>',
    '<section><h2>Bluetooth</h2>',
    '<label class="row"><span>Vibrate on disconnect</span>',
    '<input type="checkbox" data-key="SETTING_VIBE_DISCONNECT"></label>',
    '<label class="row"><span>Vibrate on reconnect</span>',
    '<input type="checkbox" data-key="SETTING_VIBE_RECONNECT"></label>',
    '</section>',
    '<section><h2>Background</h2>',
    colorFieldHtml('SETTING_BACKGROUND_COLOR', 'Background'),
    colorFieldHtml('SETTING_TRACK_COLOR', 'Bar track'),
    '<p class="hint">A black track reproduces the clean look of the original face.</p>',
    '</section>',
    '<section><h2>Bars</h2>',
    '<div id="list"></div>',
    '<p class="hint">Drag ≡ to reorder the bars, untick one to hide it, and '
      + 'tap a row to open its three colours. At least one bar stays on.</p>',
    '</section>',
    '</main>',
    '<div class="actions"><button id="reset" type="button">Reset</button>',
    '<button id="save" type="button">Save</button></div>',
    '<div class="cfm" id="cfm"><div class="cfm-box">',
    '<p>Restore all settings to their defaults?</p>',
    '<div class="cfm-row"><button id="cfmCancel" type="button">Cancel</button>',
    '<button id="cfmOk" type="button">Reset</button></div></div></div>',
    '<script>',
    'var current=', safeJson(settings), ';',
    'var defaults=', safeJson(DEFAULTS), ';',
    'var SERIES=', safeJson(SERIES), ';',
    'function fields(){return document.querySelectorAll("[data-key]")}',
    'function apply(values){var list=fields();for(var i=0;i<list.length;i++){',
    'var el=list[i],key=el.getAttribute("data-key"),value=values[key];',
    'if(el.type==="checkbox"){el.checked=Number(value)===1}',
    'else{el.value=String(value)}}}',
    // Swatch colour picker: the 64-colour Pebble palette, ported from
    // solar-earth. Colours live in `colors` and are merged in collect().
    'var PEBBLE_LEVELS=["00","55","AA","FF"];',
    'var PEBBLE_COLORS=[];for(var pr=0;pr<4;pr++){for(var pg=0;pg<4;pg++){',
    'for(var pb=0;pb<4;pb++){PEBBLE_COLORS.push(',
    'PEBBLE_LEVELS[pr]+PEBBLE_LEVELS[pg]+PEBBLE_LEVELS[pb])}}}',
    'function hex(v){v=String(v||"000000").replace("#","").toUpperCase();',
    'return /^[0-9A-F]{6}$/.test(v)?v:"000000"}',
    'var colors={},openGrid=null;',
    'function initColors(values){colors={};for(var k in values){',
    'if(/_COLOR$/.test(k)){colors[k]=hex(values[k])}}}',
    'function buildSwatch(mount){var key=mount.getAttribute("data-color-key");',
    'mount.className="color-field";mount.innerHTML="";',
    'var head=document.createElement("div");head.className="cc-head";',
    'var s=document.createElement("span");s.textContent=mount.getAttribute("data-label");',
    'var btn=document.createElement("button");btn.type="button";btn.className="swatch-btn";',
    'btn.style.background="#"+hex(colors[key]);',
    'var g=document.createElement("div");g.className="swatches collapsed";',
    'PEBBLE_COLORS.forEach(function(c){var sw=document.createElement("div");',
    'sw.className=(c===hex(colors[key]))?"swatch sel":"swatch";',
    'sw.style.background="#"+c;sw.title="#"+c;sw.onclick=function(){',
    'colors[key]=c;btn.style.background="#"+c;colorChanged(key,c);',
    'var kids=g.children;',
    'for(var i=0;i<kids.length;i++){kids[i].className="swatch"}',
    'sw.className="swatch sel";g.className="swatches collapsed";openGrid=null};',
    'g.appendChild(sw)});',
    'btn.onclick=function(){var willOpen=g.classList.contains("collapsed");',
    'if(openGrid&&openGrid!==g){openGrid.classList.add("collapsed")}',
    'if(willOpen){g.classList.remove("collapsed");openGrid=g}',
    'else{g.classList.add("collapsed");openGrid=null}};',
    'head.appendChild(s);head.appendChild(btn);',
    'mount.appendChild(head);mount.appendChild(g)}',
    'function renderColors(){openGrid=null;',
    'var m=document.querySelectorAll(".color-field");',
    'for(var i=0;i<m.length;i++){buildSwatch(m[i])}}',
    // Keeps the collapsed row preview in step with the bar colour picker.
    'function colorChanged(key,value){',
    'var dots=document.querySelectorAll("[data-dot-key]");',
    'for(var i=0;i<dots.length;i++){',
    'if(dots[i].getAttribute("data-dot-key")===key){',
    'dots[i].style.background="#"+value}}}',
    // The bar list: one row per series, in display order.
    'var COLOR_ROLES=[["bar","Bar"],["text","Text (on track)"],',
    '["onBar","Text (on bar)"]];',
    'function items(){return Array.prototype.slice.call(',
    'document.querySelectorAll("#list .si"))}',
    'function shownCount(){var n=0;items().forEach(function(el){',
    'if(el.querySelector("input").checked){n++}});return n}',
    'function collapseAll(){if(openGrid){openGrid.classList.add("collapsed");',
    'openGrid=null}items().forEach(function(el){el.classList.remove("open");',
    'var b=el.querySelector(".si-body");if(b){b.classList.add("collapsed")}})}',
    'function flash(el){el.classList.remove("flash");',
    'void el.offsetWidth;el.classList.add("flash")}',
    'function seriesItem(id,shown){var spec=SERIES[id];',
    'var item=document.createElement("div");',
    'item.className=shown?"si":"si off";item.setAttribute("data-id",id);',
    'var head=document.createElement("div");head.className="si-head";',
    'var handle=document.createElement("div");handle.className="si-handle";',
    'handle.innerHTML="&#8801;";',
    'var box=document.createElement("input");box.type="checkbox";',
    'box.checked=shown;',
    'var dot=document.createElement("div");dot.className="si-dot";',
    'dot.setAttribute("data-dot-key",spec.bar);',
    'dot.style.background="#"+hex(colors[spec.bar]);',
    'var name=document.createElement("div");name.className="si-name";',
    'name.textContent=spec.label;',
    'var chev=document.createElement("div");chev.className="si-chev";',
    'chev.innerHTML="&#9654;";',
    'var body=document.createElement("div");body.className="si-body collapsed";',
    'COLOR_ROLES.forEach(function(role){var mount=document.createElement("div");',
    'mount.className="color-field";',
    'mount.setAttribute("data-color-key",spec[role[0]]);',
    'mount.setAttribute("data-label",role[1]);body.appendChild(mount)});',
    // Refuse to hide the last visible bar: an empty face is never wanted.
    'box.onclick=function(e){e.stopPropagation();',
    'if(!box.checked&&shownCount()===0){box.checked=true;flash(item);return}',
    'if(box.checked){item.classList.remove("off")}',
    'else{item.classList.add("off")}};',
    'head.onclick=function(){var willOpen=body.classList.contains("collapsed");',
    'if(willOpen){body.classList.remove("collapsed");item.classList.add("open")}',
    'else{body.classList.add("collapsed");item.classList.remove("open")}};',
    'handle.onclick=function(e){e.stopPropagation()};',
    'bindDrag(handle,item);',
    'head.appendChild(handle);head.appendChild(box);head.appendChild(dot);',
    'head.appendChild(name);head.appendChild(chev);',
    'item.appendChild(head);item.appendChild(body);return item}',
    'function renderList(order,visible){',
    'var list=document.getElementById("list");list.innerHTML="";',
    'order.forEach(function(id){',
    'list.appendChild(seriesItem(id,Number(visible[id])===1))})}',
    // Drag to reorder. Rows are collapsed first so every row is the same
    // height, which keeps the hit test to a midpoint comparison.
    'var drag=null;',
    'function startDrag(y,item){collapseAll();',
    'drag={item:item,startY:y};item.classList.add("dragging")}',
    'function moveDrag(y){if(!drag){return}',
    'var item=drag.item,list=document.getElementById("list");',
    'item.style.transform="translateY("+(y-drag.startY)+"px)";',
    'var rect=item.getBoundingClientRect();',
    'var centre=rect.top+rect.height/2;',
    'var prev=item.previousElementSibling,next=item.nextElementSibling,',
    'target=null,before=null;',
    'if(prev){var pr=prev.getBoundingClientRect();',
    'if(centre<pr.top+pr.height/2){target=prev;before=prev}}',
    'if(!target&&next){var nr=next.getBoundingClientRect();',
    'if(centre>nr.top+nr.height/2){target=next;before=next.nextElementSibling}}',
    'if(!target){return}',
    'var wasAt=rect.top;list.insertBefore(item,before);',
    // Rebase on the layout shift so the row stays under the finger.
    'drag.startY+=item.getBoundingClientRect().top-wasAt;',
    'item.style.transform="translateY("+(y-drag.startY)+"px)"}',
    'function endDrag(){if(!drag){return}',
    'drag.item.style.transform="";drag.item.classList.remove("dragging");',
    'drag=null}',
    'function bindDrag(handle,item){',
    'if(window.PointerEvent){',
    'handle.addEventListener("pointerdown",function(e){',
    'if(e.button&&e.button!==0){return}e.preventDefault();',
    'if(handle.setPointerCapture){handle.setPointerCapture(e.pointerId)}',
    'startDrag(e.clientY,item)});',
    'handle.addEventListener("pointermove",function(e){',
    'if(drag){e.preventDefault();moveDrag(e.clientY)}});',
    'handle.addEventListener("pointerup",endDrag);',
    'handle.addEventListener("pointercancel",endDrag)}',
    'else{handle.addEventListener("touchstart",function(e){e.preventDefault();',
    'startDrag(e.touches[0].clientY,item)});',
    'handle.addEventListener("touchmove",function(e){e.preventDefault();',
    'moveDrag(e.touches[0].clientY)});',
    'handle.addEventListener("touchend",endDrag);',
    'handle.addEventListener("touchcancel",endDrag)}}',
    'function collect(){var out={},list=fields();for(var i=0;i<list.length;i++){',
    'var el=list[i],key=el.getAttribute("data-key");',
    'if(el.type==="checkbox"){out[key]=el.checked?1:0}',
    'else{out[key]=parseInt(el.value,10)}}',
    'for(var ck in colors){if(colors.hasOwnProperty(ck)){out[ck]=colors[ck]}}',
    'var order=[],visible=SERIES.map(function(){return 0});',
    'items().forEach(function(el){var id=Number(el.getAttribute("data-id"));',
    'order.push(id);visible[id]=el.querySelector("input").checked?1:0});',
    'out.SETTING_SERIES_ORDER=order;out.SETTING_SERIES_VISIBLE=visible;',
    'return out}',
    'function cfmShow(v){document.getElementById("cfm").classList[v?"add":"remove"]("open")}',
    'document.getElementById("reset").onclick=function(){cfmShow(true)};',
    'document.getElementById("cfmCancel").onclick=function(){cfmShow(false)};',
    'document.getElementById("cfmOk").onclick=function(){cfmShow(false);',
    'apply(defaults);initColors(defaults);',
    'renderList(defaults.SETTING_SERIES_ORDER,defaults.SETTING_SERIES_VISIBLE);',
    'renderColors()};',
    'document.getElementById("save").onclick=function(){',
    'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(collect()))};',
    'apply(current);initColors(current);',
    'renderList(current.SETTING_SERIES_ORDER,current.SETTING_SERIES_VISIBLE);',
    'renderColors();',
    '</script></body></html>'
  ].join('');
}

Pebble.addEventListener('ready', function () {
  sendSettings(loadSettings());
});

Pebble.addEventListener('showConfiguration', function () {
  var html = configurationHtml(loadSettings());
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function (event) {
  if (!event || !event.response) {
    return;
  }

  var response = event.response;
  var settings;
  try {
    settings = JSON.parse(decodeURIComponent(response));
  } catch (firstError) {
    try {
      settings = JSON.parse(response);
    } catch (secondError) {
      console.log('Invalid Bars 2 configuration response');
      return;
    }
  }

  settings = mergedSettings(settings);
  saveSettings(settings);
  sendSettings(settings);
});
