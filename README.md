# Bars 2

Bars 2 is a native Pebble Time 2 recreation of ELEQUENT's classic **Bars**
watchface. It represents the hour, minute, month, date, and weekday as colourful
progress bars.

## What is included

- Native 200 × 228 layout for Pebble Time 2 (`emery`)
- Horizontal, inverted horizontal, vertical, and inverted vertical layouts
- Polar layout: the bars nest as rectangular rings sized to the screen, each
  filling from twelve o'clock clockwise, or anticlockwise when inverted
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
- Bluetooth disconnect/reconnect vibration settings
- Quick View-aware layout
- Configurable label placement (horizontal/vertical): at the start, middle, or
  end of either the filled (inside) or unfilled (outside) part of each bar, or
  always fixed at the centre of the complete bar
- Two text colours per series — each glyph switches from the empty-track colour
  to the filled-bar colour as the bar reaches it (with an optional black
  outline for legibility)
- Self-contained configuration page: no external website is required
- Individually configurable bar, text, background, and track colours

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
