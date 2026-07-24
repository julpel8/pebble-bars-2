var STORAGE_KEY = 'bars2Settings';

var DEFAULTS = {
  SETTING_STYLE: 0,
  SETTING_TEXT_PLACEMENT: 1,
  SETTING_CLOCK_FORMAT: 0,
  SETTING_LANGUAGE: 0,
  SETTING_LEADING_ZERO: 0,
  SETTING_SHOW_SECONDS: 0,
  SETTING_SHOW_BATTERY: 0,
  SETTING_SEAMLESS_BARS: 1,
  SETTING_TEXT_OUTLINE: 1,
  SETTING_ANIMATE: 1,
  SETTING_VIBE_DISCONNECT: 1,
  SETTING_VIBE_RECONNECT: 0,
  SETTING_BACKGROUND_COLOR: '000000',
  SETTING_TRACK_COLOR: '000000',
  SETTING_HOUR_BAR_COLOR: '00FF00',
  SETTING_HOUR_TEXT_COLOR: 'AAFF00',
  SETTING_MINUTE_BAR_COLOR: '00AA55',
  SETTING_MINUTE_TEXT_COLOR: '00FF00',
  SETTING_MONTH_BAR_COLOR: '0055FF',
  SETTING_MONTH_TEXT_COLOR: '0055FF',
  SETTING_DATE_BAR_COLOR: 'FFFF00',
  SETTING_DATE_TEXT_COLOR: 'FFFF00',
  SETTING_DAY_BAR_COLOR: 'FF0000',
  SETTING_DAY_TEXT_COLOR: 'FF0000',
  SETTING_SECOND_BAR_COLOR: 'AA00FF',
  SETTING_SECOND_TEXT_COLOR: 'AA00FF',
  SETTING_BATTERY_BAR_COLOR: '00FFFF',
  SETTING_BATTERY_TEXT_COLOR: '00FFFF',
  SETTING_HOUR_TEXT_ON_BAR_COLOR: 'FFFFFF',
  SETTING_MINUTE_TEXT_ON_BAR_COLOR: 'FFFFFF',
  SETTING_MONTH_TEXT_ON_BAR_COLOR: 'FFFFFF',
  SETTING_DATE_TEXT_ON_BAR_COLOR: 'FFFFFF',
  SETTING_DAY_TEXT_ON_BAR_COLOR: 'FFFFFF',
  SETTING_SECOND_TEXT_ON_BAR_COLOR: 'FFFFFF',
  SETTING_BATTERY_TEXT_ON_BAR_COLOR: 'FFFFFF'
};

var COLOR_FIELDS = [
  ['SETTING_HOUR_BAR_COLOR', 'Hour — bar'],
  ['SETTING_HOUR_TEXT_COLOR', 'Hour — text (on track)'],
  ['SETTING_HOUR_TEXT_ON_BAR_COLOR', 'Hour — text (on bar)'],
  ['SETTING_MINUTE_BAR_COLOR', 'Minute — bar'],
  ['SETTING_MINUTE_TEXT_COLOR', 'Minute — text (on track)'],
  ['SETTING_MINUTE_TEXT_ON_BAR_COLOR', 'Minute — text (on bar)'],
  ['SETTING_MONTH_BAR_COLOR', 'Month — bar'],
  ['SETTING_MONTH_TEXT_COLOR', 'Month — text (on track)'],
  ['SETTING_MONTH_TEXT_ON_BAR_COLOR', 'Month — text (on bar)'],
  ['SETTING_DATE_BAR_COLOR', 'Date — bar'],
  ['SETTING_DATE_TEXT_COLOR', 'Date — text (on track)'],
  ['SETTING_DATE_TEXT_ON_BAR_COLOR', 'Date — text (on bar)'],
  ['SETTING_DAY_BAR_COLOR', 'Weekday — bar'],
  ['SETTING_DAY_TEXT_COLOR', 'Weekday — text (on track)'],
  ['SETTING_DAY_TEXT_ON_BAR_COLOR', 'Weekday — text (on bar)'],
  ['SETTING_SECOND_BAR_COLOR', 'Seconds — bar'],
  ['SETTING_SECOND_TEXT_COLOR', 'Seconds — text (on track)'],
  ['SETTING_SECOND_TEXT_ON_BAR_COLOR', 'Seconds — text (on bar)'],
  ['SETTING_BATTERY_BAR_COLOR', 'Battery — bar'],
  ['SETTING_BATTERY_TEXT_COLOR', 'Battery — text (on track)'],
  ['SETTING_BATTERY_TEXT_ON_BAR_COLOR', 'Battery — text (on bar)']
];

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
    result[key] = source[key];
  });
  return result;
}

function mergedSettings(saved) {
  var result = copyObject(DEFAULTS);
  Object.keys(saved || {}).forEach(function (key) {
    result[key] = saved[key];
  });
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

function messagePayload(settings) {
  var payload = {};
  Object.keys(settings).forEach(function (key) {
    var value = settings[key];
    if (/_COLOR$/.test(key)) {
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

function colorRowsHtml() {
  var html = [];
  COLOR_FIELDS.forEach(function (field) {
    html.push(colorFieldHtml(field[0], field[1]));
  });
  return html.join('');
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
    '</select></label>',
    '<label class="row"><span>Text placement</span>',
    '<select data-key="SETTING_TEXT_PLACEMENT">',
    '<option value="0">Outside — opposite end</option>',
    '<option value="1">Outside — bar edge</option>',
    '<option value="2">Inside — start</option>',
    '<option value="3">Inside — middle</option>',
    '<option value="4">Inside — end</option>',
    '</select></label>',
    '<p class="hint">Placement applies to horizontal and vertical styles. '
      + 'Text uses the “on track” colour beside the fill and the “on bar” '
      + 'colour (with a black outline) when centred over the bar.</p>',
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
    '<label class="row"><span>Time format</span>',
    '<select data-key="SETTING_CLOCK_FORMAT">',
    '<option value="0">Watch setting</option>',
    '<option value="1">24-hour</option>',
    '<option value="2">12-hour</option>',
    '</select></label>',
    '<label class="row"><span>Leading zero for hour</span>',
    '<input type="checkbox" data-key="SETTING_LEADING_ZERO"></label>',
    '<label class="row"><span>Show seconds</span>',
    '<input type="checkbox" data-key="SETTING_SHOW_SECONDS"></label>',
    '<label class="row"><span>Show battery</span>',
    '<input type="checkbox" data-key="SETTING_SHOW_BATTERY"></label>',
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
    '<section><h2>Series colours</h2>',
    colorRowsHtml(),
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
    'colors[key]=c;btn.style.background="#"+c;var kids=g.children;',
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
    'function collect(){var out={},list=fields();for(var i=0;i<list.length;i++){',
    'var el=list[i],key=el.getAttribute("data-key");',
    'if(el.type==="checkbox"){out[key]=el.checked?1:0}',
    'else{out[key]=parseInt(el.value,10)}}',
    'for(var ck in colors){if(colors.hasOwnProperty(ck)){out[ck]=colors[ck]}}',
    'return out}',
    'function cfmShow(v){document.getElementById("cfm").classList[v?"add":"remove"]("open")}',
    'document.getElementById("reset").onclick=function(){cfmShow(true)};',
    'document.getElementById("cfmCancel").onclick=function(){cfmShow(false)};',
    'document.getElementById("cfmOk").onclick=function(){cfmShow(false);',
    'apply(defaults);initColors(defaults);renderColors()};',
    'document.getElementById("save").onclick=function(){',
    'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(collect()))};',
    'apply(current);initColors(current);renderColors();',
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
