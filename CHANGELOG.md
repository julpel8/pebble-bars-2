# Changelog

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
- Optional seconds, battery, animation, and Bluetooth vibrations.
