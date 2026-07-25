# Bars 2

Bars 2 is a native Pebble recreation of ELEQUENT's classic **Bars** watchface.
It represents the hour, minute, month, date, and weekday as colourful progress
bars.

## What is included

- Native 200 × 228 layout for Pebble Time 2 (`emery`) and 180 × 180 for
  Pebble Round 2 (`gabbro`)
- Horizontal, inverted horizontal, vertical, and inverted vertical layouts
- Rectangular and round polar layouts: the bars nest as rings, each filling
  from twelve o'clock clockwise, or anticlockwise when inverted. Rectangular
  rings follow the screen's edges; round rings stay circular and can extend
  the first ring outside the circle, the last ring into its centre, or both
- Eleven bars to choose from: hour, minute, month, date, weekday, seconds,
  battery, daylight/night, moon, steps, and a custom countdown
- Optional single "HH:MM" bar in place of separate hour and minute bars
- Daylight/night and moonrise/moonset bars, worked out on the watch from a
  position the phone sends once, so they keep running with the phone away.
  Their colours alternate at each rise and set, and labels such as
  `SUNSET 20:42` or `MOONRISE 23:18` name the next transition
- Steps bar with a configurable goal, from today's Pebble Health count
- Custom bar counting down to a date, labelled from a template: `{d}` days
  left, `{t}` days in the span, `{p}` percent done
- Optional launch animation
- Free bar order and per-bar visibility, set by dragging and ticking the bar
  list in the settings
- Configurable clock refresh every 1, 5, 10, 20, 30, or 60 seconds
- Smooth continuous or exact whole-unit bar progress
- System, 24-hour, and 12-hour clock formats
- Localized weekday/month labels in the same 38 languages as Solar Earth, whose
  natural date-bar order seeds the default bar list
- Optional full weekday and month names in horizontal layouts
- Configurable Monday- or Sunday-first weekday progress
- Solar Earth-style LECO numbers and date labels in horizontal and vertical
  layouts, with adaptive sizing for tighter layouts
- Quick View-aware layout
- Configurable label placement (horizontal/vertical): at the start, middle, or
  end of either the filled (inside) or unfilled (outside) part of each bar, or
  always fixed at the centre of the complete bar
- Two text colours per series — each glyph switches from the empty-track colour
  to the filled-bar colour as the bar reaches it (with an optional black
  outline for legibility)
- Self-contained configuration page: no external website is required
- Individually configurable bar, track, and text colours per bar, each track
  starting as a much darker shade of its own bar's colour

## Build

```sh
pebble build
```

The installable bundle is written to `build/pebble-bars-2.pbw`.

## Origin

This is a clean-room implementation based on the public screenshots and
feature description of
[Bars by ELEQUENT](https://apps.repebble.com/55e3d873e1fe973ff20000aa).
No source code from the original application was available or copied.

The project structure and modern Pebble SDK setup are based on the local
`solar-earth` project.
