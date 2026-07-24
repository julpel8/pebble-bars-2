# Bars 2

Bars 2 is a native Pebble Time 2 recreation of ELEQUENT's classic **Bars**
watchface. It represents the hour, minute, month, date, and weekday as colourful
progress bars.

## What is included

- Native 200 × 228 layout for Pebble Time 2 (`emery`)
- Horizontal, inverted horizontal, vertical, and inverted vertical layouts
- Optional launch animation
- Optional seconds and battery bars
- System, 24-hour, and 12-hour clock formats
- Localized weekday/month labels in the same 38 languages as Solar Earth,
  including the natural date-bar order for each language
- Solar Earth-style LECO numbers and date labels in horizontal layouts, with
  adaptive Gothic Bold fallbacks for tighter layouts
- Bluetooth disconnect/reconnect vibration settings
- Quick View-aware layout
- Configurable label placement (horizontal/vertical): outside at the opposite
  end or following the bar edge, or inside the fill at its start, middle, or end
- Two text colours per series — each glyph switches from the empty-track colour
  to the filled-bar colour as the bar reaches it (with an optional black
  outline for legibility)
- Self-contained configuration page: no external website is required
- Individually configurable bar, text, background, and track colours

## Roadmap

- Polar layout

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
