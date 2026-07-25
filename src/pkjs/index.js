var settingsStore = require('./settings');
var DEFAULTS = settingsStore.DEFAULTS;
var SERIES = settingsStore.SERIES;
var loadSettings = settingsStore.loadSettings;
var mergedSettings = settingsStore.mergedSettings;
var refreshLocation = settingsStore.refreshLocation;
var saveSettings = settingsStore.saveSettings;
var sendSettings = settingsStore.sendSettings;

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
    '.row{min-height:52px;padding:10px 15px;display:flex;',
    'align-items:center;justify-content:space-between;border-top:1px solid #2a2d32;gap:16px}',
    'h2+.row{border-top:0}.row span{line-height:1.2}',
    'select{max-width:55%;background:#26292e;color:#fff;border:1px solid #444850;',
    'border-radius:7px;padding:8px;font-size:15px}',
    '.row.disabled{opacity:.45}.row.disabled select{cursor:not-allowed}',
    'input[type=checkbox]{width:24px;height:24px;accent-color:#55d57f}',
    'input[type=text],input[type=number],input[type=date]{max-width:55%;',
    'background:#26292e;color:#fff;border:1px solid #444850;border-radius:7px;',
    'padding:8px;font-size:15px}',
    '.chips{display:flex;flex-wrap:wrap;gap:8px;padding:0 15px 14px}',
    '.chip{min-height:36px;padding:0 12px;border:1px solid #444850;',
    'border-radius:8px;background:#26292e;color:#f6f6f6;font-size:14px;',
    'font-weight:400}',
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
    '<header><h1>BARS 2</h1><p>Native Pebble settings</p></header>',
    '<main>',
    '<section><h2>Layout</h2>',
    '<label class="row"><span>Bar style</span><select data-key="SETTING_STYLE">',
    '<option value="0">Horizontal</option>',
    '<option value="1">Horizontal inverted</option>',
    '<option value="2">Vertical</option>',
    '<option value="3">Vertical inverted</option>',
    '<option value="4">Polar rectangular</option>',
    '<option value="5">Polar rectangular inverted</option>',
    '<option value="6">Polar round</option>',
    '<option value="7">Polar round inverted</option>',
    '</select></label>',
    '<p class="hint">Polar nests the bars as rings: the first bar in the list '
      + 'below is the outer ring, and each one fills from twelve o’clock — '
      + 'clockwise, or anticlockwise when inverted. Labels sit at the top of '
      + 'their ring. The rectangular variant follows the screen’s edges; the '
      + 'round one keeps the rings circular, which suits a round watch.</p>',
    '<label class="row"><span>Round polar fill</span>',
    '<select data-key="SETTING_ROUND_POLAR_FILL" id="roundPolarFill">',
    '<option value="0">None</option>',
    '<option value="1">Outside — first bar</option>',
    '<option value="2">Centre — last bar</option>',
    '<option value="3">Outside and centre</option>',
    '</select></label>',
    '<p class="hint">Extends the first visible ring into the space outside the '
      + 'circle, the last visible ring into its centre, or both. With one '
      + 'visible bar, the same bar can fill both spaces.</p>',
    '<label class="row"><span>Text placement</span>',
    '<select data-key="SETTING_TEXT_PLACEMENT" id="textPlacement">',
    '<option value="1">Outside — start</option>',
    '<option value="5">Outside — middle</option>',
    '<option value="0">Outside — end</option>',
    '<option value="2">Inside — start</option>',
    '<option value="3">Inside — middle</option>',
    '<option value="4">Inside — end</option>',
    '<option value="6">Always — bar middle</option>',
    '</select></label>',
    '<p class="hint">Placement applies to horizontal and vertical styles. '
      + '“Always” fixes every label at the centre of its complete bar.</p>',
    '<label class="row"><span>Black text outline</span>',
    '<input type="checkbox" data-key="SETTING_TEXT_OUTLINE"></label>',
    '<p class="hint">Outlines the whole label, over the filled bar and over '
      + 'its track alike.</p>',
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
    '<label class="row"><span>Merge hour and minute</span>',
    '<input type="checkbox" data-key="SETTING_MERGE_HOUR_MINUTE"></label>',
    '<p class="hint">Draws one “HH:MM” bar in the hour’s place instead of two, '
      + 'filling across the day. Needs both the hour and the minute bar '
      + 'switched on below.</p>',
    '<label class="row"><span>Step goal</span>',
    '<select data-key="SETTING_STEP_GOAL">',
    '<option value="2000">2 000 steps</option>',
    '<option value="4000">4 000 steps</option>',
    '<option value="6000">6 000 steps</option>',
    '<option value="8000">8 000 steps</option>',
    '<option value="10000">10 000 steps</option>',
    '<option value="12000">12 000 steps</option>',
    '<option value="15000">15 000 steps</option>',
    '<option value="20000">20 000 steps</option>',
    '<option value="25000">25 000 steps</option>',
    '<option value="30000">30 000 steps</option>',
    '</select></label>',
    '<p class="hint">The steps bar fills towards this goal and shows today’s '
      + 'count.</p>',
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
    '<section><h2>Location</h2>',
    '<label class="row"><span>Use phone location</span>',
    '<input type="checkbox" data-key="SETTING_USE_PHONE_LOCATION"></label>',
    '<label class="row"><span>Latitude</span>',
    '<input type="number" data-key="SETTING_LATITUDE" data-scale="1000000" ',
    'step="0.0001" min="-90" max="90"></label>',
    '<label class="row"><span>Longitude</span>',
    '<input type="number" data-key="SETTING_LONGITUDE" data-scale="1000000" ',
    'step="0.0001" min="-180" max="180"></label>',
    '<p class="hint">The daylight/night and moon bars are worked out on the '
      + 'watch from these coordinates, so they keep running with the phone '
      + 'away. Their two colours alternate after each rise and set; the label '
      + 'names the next event and its time. Leave “use phone location” on to '
      + 'have the coordinates filled in for you, '
      + 'or turn it off and type them in degrees — south and west are '
      + 'negative.</p>',
    '</section>',
    '<section><h2>Custom bar</h2>',
    '<label class="row"><span>Label</span>',
    '<input type="text" data-key="SETTING_CUSTOM_LABEL" maxlength="16"></label>',
    '<div class="chips" id="tpl"></div>',
    '<p class="hint">{d} is the days left, {t} the days in the span and {p} '
      + 'the percentage done. Anything else is shown as typed.</p>',
    '<label class="row"><span>Counting from</span>',
    '<input type="date" data-key="SETTING_CUSTOM_START_DAY"></label>',
    '<label class="row"><span>Counting to</span>',
    '<input type="date" data-key="SETTING_CUSTOM_TARGET_DAY"></label>',
    '<p class="hint">The bar fills from the first date to the second. Until '
      + 'both are set it shows “--”.</p>',
    '</section>',
    '<section><h2>Background</h2>',
    colorFieldHtml('SETTING_BACKGROUND_COLOR', 'Background'),
    '<p class="hint">Each bar has its own track, set below and starting as a '
      + 'much darker shade of the bar’s colour.</p>',
    '</section>',
    '<section><h2>Bars</h2>',
    '<div id="list"></div>',
    '<p class="hint">Drag ≡ to reorder the bars, untick one to hide it, and '
      + 'tap a row to configure its three colours. At least one bar stays on.</p>',
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
    // Dates travel as whole days since 1970-01-01, matching the day numbers
    // series.c counts in.
    'function dayFromDate(v){if(!v){return 0}',
    'var p=String(v).split("-");if(p.length!==3){return 0}',
    'var t=Date.UTC(Number(p[0]),Number(p[1])-1,Number(p[2]));',
    'if(isNaN(t)){return 0}return Math.floor(t/86400000)}',
    'function dateFromDay(n){n=Number(n);if(!n){return ""}',
    'return new Date(n*86400000).toISOString().slice(0,10)}',
    'function scaleOf(el){return Number(el.getAttribute("data-scale"))||1}',
    'function apply(values){var list=fields();for(var i=0;i<list.length;i++){',
    'var el=list[i],key=el.getAttribute("data-key"),value=values[key];',
    'if(el.type==="checkbox"){el.checked=Number(value)===1}',
    'else if(el.type==="date"){el.value=dateFromDay(value)}',
    'else if(el.type==="number"){el.value=String(Number(value||0)/scaleOf(el))}',
    'else if(el.type==="text"){el.value=value==null?"":String(value)}',
    'else{el.value=String(value)}}}',
    'function syncStyleOptions(){',
    'var style=document.querySelector("[data-key=SETTING_STYLE]");',
    'var placement=document.getElementById("textPlacement");',
    'var roundFill=document.getElementById("roundPolarFill");',
    'var polar=Number(style.value)>=4;',
    'placement.disabled=polar;',
    'placement.parentNode.classList[polar?"add":"remove"]("disabled");',
    'var round=Number(style.value)===6||Number(style.value)===7;',
    'roundFill.disabled=!round;',
    'roundFill.parentNode.classList[round?"remove":"add"]("disabled")}',
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
    // Sixty-four swatches across thirty-four pickers is far too much markup to
    // build up front, so a grid is filled the first time it opens.
    'var filled=false;',
    'function fill(){PEBBLE_COLORS.forEach(function(c){',
    'var sw=document.createElement("div");',
    'sw.className=(c===hex(colors[key]))?"swatch sel":"swatch";',
    'sw.style.background="#"+c;sw.title="#"+c;sw.onclick=function(){',
    'colors[key]=c;colorChanged(key,c);btn.style.background="#"+c;',
    'var kids=g.children;',
    'for(var i=0;i<kids.length;i++){kids[i].className="swatch"}',
    'sw.className="swatch sel";g.className="swatches collapsed";openGrid=null};',
    'g.appendChild(sw)})}',
    'btn.onclick=function(){if(!filled){fill();filled=true}',
    'var willOpen=g.classList.contains("collapsed");',
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
    'var COLOR_ROLES=[["bar","Bar"],["track","Track"],["text","Text"]];',
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
    'else if(el.type==="date"){out[key]=dayFromDate(el.value)}',
    'else if(el.type==="number"){',
    'out[key]=Math.round(Number(el.value||0)*scaleOf(el))}',
    'else if(el.type==="text"){out[key]=el.value}',
    'else{out[key]=parseInt(el.value,10)}}',
    'for(var ck in colors){if(colors.hasOwnProperty(ck)){out[ck]=colors[ck]}}',
    'var order=[],visible=SERIES.map(function(){return 0});',
    'items().forEach(function(el){var id=Number(el.getAttribute("data-id"));',
    'order.push(id);visible[id]=el.querySelector(".si-head input").checked?1:0});',
    'out.SETTING_SERIES_ORDER=order;out.SETTING_SERIES_VISIBLE=visible;',
    // Typed coordinates are a fix as good as the phone's, so the watch is told
    // it has one.
    'out.SETTING_LOCATION_VALID=',
    '(out.SETTING_LATITUDE||out.SETTING_LONGITUDE||',
    'Number(current.SETTING_LOCATION_VALID))?1:0;',
    'return out}',
    'function buildTemplates(){var host=document.getElementById("tpl");',
    'var input=document.querySelector("[data-key=SETTING_CUSTOM_LABEL]");',
    '["{d}","{d}d","J-{d}","{p}%","{d}/{t}"].forEach(function(t){',
    'var b=document.createElement("button");b.type="button";b.className="chip";',
    'b.textContent=t;b.onclick=function(){input.value=t};',
    'host.appendChild(b)})}',
    'function cfmShow(v){document.getElementById("cfm").classList[v?"add":"remove"]("open")}',
    'document.getElementById("reset").onclick=function(){cfmShow(true)};',
    'document.getElementById("cfmCancel").onclick=function(){cfmShow(false)};',
    'document.getElementById("cfmOk").onclick=function(){cfmShow(false);',
    'apply(defaults);syncStyleOptions();initColors(defaults);',
    'renderList(defaults.SETTING_SERIES_ORDER,defaults.SETTING_SERIES_VISIBLE);',
    'renderColors()};',
    'document.getElementById("save").onclick=function(){',
    'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(collect()))};',
    'document.querySelector("[data-key=SETTING_STYLE]").onchange=syncStyleOptions;',
    'apply(current);syncStyleOptions();initColors(current);',
    'renderList(current.SETTING_SERIES_ORDER,current.SETTING_SERIES_VISIBLE);',
    'renderColors();buildTemplates();',
    '</script></body></html>'
  ].join('');
}

Pebble.addEventListener('ready', function () {
  var settings = loadSettings();
  sendSettings(settings);
  refreshLocation(settings);
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
  refreshLocation(settings);
});
