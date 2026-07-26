# Changelog

## [Unreleased]

### Changed

- Each series now has one text colour, used over both the track and the filled
  bar.
- The black text outline now covers the whole label instead of only the glyphs
  standing over the filled bar, so a label stays outlined across the bar and
  its track. The outline is drawn as its own pass over the label, so a
  neighbouring glyph's outline no longer lands on top of an already drawn one.

## [0.5.0] - 2026-07-25

### Added

- A track colour per bar, starting as a much darker shade of that bar's own
  colour instead of one shared background. The configuration page gains a
  fourth colour per bar.
- Daylight/night bar: progress through the current stretch of day or night,
  alternating colours at each transition and labelling the next sunrise or
  sunset explicitly.
- Moon bar: progress through the current stretch of the moon being up or down,
  alternating colours at each transition and labelling the next moonrise or
  moonset explicitly.
- Steps bar, filling towards a configurable goal from today's Pebble Health
  count.
- Custom bar counting from one date to another, labelled from a template:
  `{d}` days left, `{t}` days in the span, `{p}` percent done. Anything else in
  the template is shown as typed.
- Optional single "HH:MM" bar in place of separate hour and minute bars, taking
  the hour's slot and colours and filling across the day.
- Round polar layout, clockwise and anticlockwise: concentric circular rings
  rather than rings squared off to the screen, for Pebble Round 2.
- Optional round-polar fill extending the first ring outside the circle, the
  last ring into its centre, or both. A single visible ring can fill both.
- Matching default text colours on each bar's track and filled portion, while
  keeping both roles independently configurable.
- `gabbro` (Pebble Round 2, 180 × 180) as a build target alongside `emery`.
- Location section in the settings: coordinates come from the phone, or can be
  typed in degrees when it will not give them up. The sun and moon are worked
  out on the watch from the stored position, so those bars keep running while
  the phone is away.

### Changed

- The bar order travels and is stored as one byte per bar rather than packed
  into the nibbles of a single integer, which ran out of room past seven bars.
  Existing installs keep their order and visibility, with the new bars added
  hidden behind them.
- The single face-wide track colour is retired in favour of the per-bar tracks,
  so bars now sit on a dark shade of their own colour by default.
- Each bar's label is sized against the widest value it can show rather than
  the current one, so the text no longer resizes as a value shortens.
- The original polar style is now named rectangular polar, and its progress
  follows the complete outer perimeter of every ring, including the vertical
  sides and their destination corners.
- Text placement is disabled in the configuration for polar styles, whose
  labels always stay fixed at the top of their ring.

## [0.3.0] - 2026-07-25

### Added

- Polar layout, in clockwise and anticlockwise variants: the bars become
  rectangular rings nested to the shape of the screen, the first bar in the
  list being the outer ring. Each ring fills from twelve o'clock and carries
  its label in its top band.
- Reorderable bar list in the configuration page: drag a row to change the
  display order, tick it to show or hide the bar, and tap it to open its three
  colours.
- Per-series visibility for all seven bars, replacing the fixed layout.

### Changed

- Labels now scale with the bar: with few bars on screen they use the 60-point
  LECO face instead of stopping at 42, in both horizontal and vertical layouts.
  Packed layouts (six or seven bars) are unchanged.
- Bar order now comes from the saved list instead of the language's date order,
  which only seeds the default. Existing installs keep their current order and
  visibility on upgrade.

### Removed

- The separate "Show seconds" and "Show battery" toggles, now tick boxes in the
  bar list.

## [0.2.0] - 2026-07-24

### Added

- Configurable clock refresh every 1, 5, 10, 20, 30, or 60 seconds.
- Smooth continuous or exact whole-unit bar progress.
- Start, middle, and end text alignment for both inside and outside placement.
- Fixed centre placement, independent of each bar's fill level.
- Optional full weekday and month names in horizontal layouts.
- Optional Sunday-first weekday progress (Monday remains the default).

### Changed

- Extended Solar Earth-style typography and adaptive sizing to vertical layouts.
- Updated the default placement, colours, leading zero, outline, and progress
  settings.
- Displayed the optional seconds bar directly after minutes.

## [0.1.2] - 2026-07-24

### Added

- English configuration UI with a 38-language date selector.
- Localized weekday and month labels plus language-specific date-bar ordering,
  aligned with Solar Earth.
- Solar Earth-style LECO numbers and Gothic Bold date labels, with adaptive
  sizing for narrow layouts.

### Changed

- Split the native code into settings, series, rendering, and application
  lifecycle modules.
- Moved the unfinished polar layout to the roadmap.

### Removed

- Removed the unfinished daily bar-order shuffle.

## [0.1.0] - 2026-07-24

### Added

- Initial Pebble Time 2 implementation.
- Four bar chart modes, native Emery sizing, and Quick View support.
- Offline configuration page with per-series colours.
- Optional seconds, battery, and animation.
