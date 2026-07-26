#include "renderer_polar.h"

#include "renderer_internal.h"
#include "renderer_text.h"

// Steps each ring takes out of the screen, against the single step left empty
// in the middle: the wider the ring, the smaller the hole at the centre.
#define RING_STEPS 3

// Integer square root, rounded down. Circle geometry here never exceeds a
// screen radius, and avoiding newlib's sqrt() also avoids its absolute data
// references, which are unsafe in a relocated Pebble app.
static int integer_sqrt(int value) {
  if (value <= 0) {
    return 0;
  }

  uint32_t remainder = (uint32_t)value;
  uint32_t result = 0;
  uint32_t bit = 1u << 30;
  while (bit > remainder) {
    bit >>= 2;
  }
  while (bit != 0) {
    if (remainder >= result + bit) {
      remainder -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return (int)result;
}

// Concentric rings keep the screen's proportions, so the innermost one still
// has room for its label instead of collapsing into a slot on the narrow axis.
static GRect ring_rect(GRect bounds, int units, int scale) {
  int width = bounds.size.w * scale / units;
  int height = bounds.size.h * scale / units;
  return GRect(bounds.origin.x + (bounds.size.w - width) / 2,
               bounds.origin.y + (bounds.size.h - height) / 2, width, height);
}

typedef struct {
  GRect rect;
  bool horizontal;  // The fill runs along x rather than y.
  bool forward;     // It grows from the low edge towards the high one.
} PolarSegment;

static int segment_length(const PolarSegment *segment) {
  return segment->horizontal ? segment->rect.size.w : segment->rect.size.h;
}

static GRect segment_fill(const PolarSegment *segment, int filled) {
  GRect rect = segment->rect;
  int physical_length = segment->horizontal ? rect.size.w : rect.size.h;
  int physical_filled = BARS_MIN(physical_length, filled);
  if (segment->horizontal) {
    if (!segment->forward) {
      rect.origin.x += rect.size.w - physical_filled;
    }
    rect.size.w = physical_filled;
  } else {
    if (!segment->forward) {
      rect.origin.y += rect.size.h - physical_filled;
    }
    rect.size.h = physical_filled;
  }
  return rect;
}

// Splits the ring into the straight runs its fill travels, clockwise from
// twelve o'clock (anticlockwise when inverted): half the top band, one side,
// the bottom band, the other side, then the rest of the top band. Each corner
// belongs to the run that reaches it. The following run therefore starts past
// the preceding run's thickness instead of repainting that corner.
static int polar_segments(GRect outer, GRect inner, bool inverted,
                          PolarSegment segments[5]) {
  int left = outer.origin.x;
  int right = left + outer.size.w;
  int top = outer.origin.y;
  int bottom = top + outer.size.h;
  int inner_left = inner.origin.x;
  int inner_right = inner_left + inner.size.w;
  int inner_top = inner.origin.y;
  int inner_bottom = inner_top + inner.size.h;
  int centre_x = (left + right) / 2;
  int top_height = inner_top - top;
  int left_width = inner_left - left;

  GRect top_right =
      GRect(centre_x, top, right - centre_x, top_height);
  GRect top_left = GRect(left, top, centre_x - left, top_height);

  int index = 0;
  if (inverted) {
    // The top-left run owns the first corner; each following run starts just
    // beyond it and owns the next corner in the anticlockwise direction.
    segments[index++] = (PolarSegment){top_left, true, false};
    segments[index++] = (PolarSegment){
        GRect(left, inner_top, left_width, bottom - inner_top), false, true};
    segments[index++] = (PolarSegment){
        GRect(inner_left, inner_bottom, right - inner_left,
              bottom - inner_bottom),
        true, true};
    segments[index++] = (PolarSegment){
        GRect(inner_right, top, right - inner_right, inner_bottom - top),
        false, false};
    segments[index++] = (PolarSegment){
        GRect(centre_x, top, inner_right - centre_x, top_height), true, false};
  } else {
    // Mirror the same corner ownership in the clockwise direction.
    segments[index++] = (PolarSegment){top_right, true, true};
    segments[index++] = (PolarSegment){
        GRect(inner_right, inner_top, right - inner_right,
              bottom - inner_top),
        false, true};
    segments[index++] = (PolarSegment){
        GRect(left, inner_bottom, inner_right - left, bottom - inner_bottom),
        true, false};
    segments[index++] = (PolarSegment){
        GRect(left, top, left_width, inner_bottom - top), false, false};
    segments[index++] = (PolarSegment){
        GRect(inner_left, top, centre_x - inner_left, top_height), true, true};
  }
  return index;
}

void draw_polar_rectangular(GContext *ctx, GRect bounds, const Series *series,
                            int count, bool inverted,
                            const Settings *settings, int animation_progress) {
  int units = RING_STEPS * count + 1;

  for (int index = 0; index < count; ++index) {
    const Series *item = &series[index];
    GRect outer = ring_rect(bounds, units, units - RING_STEPS * index);
    GRect inner = ring_rect(bounds, units, units - RING_STEPS * (index + 1));
    if (!settings->seamless) {
      // Rings are far thinner than a row, so one pixel is groove enough.
      outer.origin.x += 1;
      outer.origin.y += 1;
      outer.size.w -= 2;
      outer.size.h -= 2;
    }

    int left = outer.origin.x;
    int right = left + outer.size.w;
    int top = outer.origin.y;
    int bottom = top + outer.size.h;
    int inner_left = inner.origin.x;
    int inner_right = inner_left + inner.size.w;
    int inner_top = inner.origin.y;
    int inner_bottom = inner_top + inner.size.h;
    int band_height = inner_top - top;

    graphics_context_set_fill_color(ctx, item->track_color);
    graphics_fill_rect(ctx, GRect(left, top, right - left, band_height), 0,
                       GCornerNone);
    graphics_fill_rect(
        ctx, GRect(left, inner_bottom, right - left, bottom - inner_bottom), 0,
        GCornerNone);
    graphics_fill_rect(ctx,
                       GRect(left, inner_top, inner_left - left,
                             inner_bottom - inner_top),
                       0, GCornerNone);
    graphics_fill_rect(ctx,
                       GRect(inner_right, inner_top, right - inner_right,
                             inner_bottom - inner_top),
                       0, GCornerNone);

    PolarSegment segments[5];
    int segment_count = polar_segments(outer, inner, inverted, segments);
    int perimeter = 0;
    for (int segment = 0; segment < segment_count; ++segment) {
      perimeter += segment_length(&segments[segment]);
    }
    int filled_length =
        (perimeter * animated_ratio(item, animation_progress) + 500) / 1000;
    int remaining = filled_length;

    graphics_context_set_fill_color(ctx, item->bar_color);
    for (int segment = 0; segment < segment_count && remaining > 0; ++segment) {
      int filled = BARS_MIN(remaining, segment_length(&segments[segment]));
      if (filled <= 0) {
        continue;
      }
      GRect fill = segment_fill(&segments[segment], filled);
      graphics_fill_rect(ctx, fill, 0, GCornerNone);
      remaining -= filled;
    }
    int max_text_width = BARS_MAX(1, right - left - 2);
    SmoothFontSpec font_spec =
        smooth_font_for_series(item, max_text_width, band_height);
    int letter_spacing = condensed_letter_spacing(
        item->label, font_spec.font, font_spec.line_height, max_text_width);
    int text_width = smooth_spaced_width(item->label, font_spec.font,
                                         font_spec.line_height, letter_spacing);
    int text_height = font_spec.visible_height;
    int visible_y = top + (band_height - text_height) / 2;
    GRect frame = GRect(left + (right - left - text_width) / 2,
                        visible_y - font_spec.top_offset, text_width + 3,
                        font_spec.line_height + 2);
    draw_smooth_text_letterwise(ctx, item->label, font_spec.font, frame,
                                item->text_color, settings->text_outline,
                                letter_spacing);
  }
}

// Progress angles are always measured from twelve o'clock in the direction of
// travel. This maps one such interval to the SDK's clockwise angle space.
static void fill_round_progress_span(GContext *ctx, GRect area, int thickness,
                                     bool inverted, int32_t from, int32_t to,
                                     GColor color) {
  if (to <= from) {
    return;
  }
  graphics_context_set_fill_color(ctx, color);
  if (inverted) {
    graphics_fill_radial(ctx, area, GOvalScaleModeFitCircle, thickness,
                         TRIG_MAX_ANGLE - to, TRIG_MAX_ANGLE - from);
  } else {
    graphics_fill_radial(ctx, area, GOvalScaleModeFitCircle, thickness, from,
                         to);
  }
}

// Extends a round-polar ring across another concentric annulus. This keeps both
// its dark track and its animated angular progress continuous through the
// otherwise empty outside or centre.
static void draw_round_polar_extension(GContext *ctx, GPoint centre,
                                       int outer_radius, int inner_radius,
                                       const Series *item, bool inverted,
                                       int animation_progress) {
  int thickness = outer_radius - inner_radius;
  if (thickness <= 0) {
    return;
  }

  GRect area = GRect(centre.x - outer_radius, centre.y - outer_radius,
                     outer_radius * 2, outer_radius * 2);
  graphics_context_set_fill_color(ctx, item->track_color);
  graphics_fill_radial(ctx, area, GOvalScaleModeFitCircle, thickness, 0,
                       TRIG_MAX_ANGLE);

  int32_t sweep =
      TRIG_MAX_ANGLE * animated_ratio(item, animation_progress) / 1000;
  fill_round_progress_span(ctx, area, thickness, inverted, 0, sweep,
                           item->bar_color);
}

// Same nesting as draw_polar_rectangular, but the rings follow the screen's
// circle instead of its corners, which is what a round watch wants.
void draw_polar_round(GContext *ctx, GRect bounds, const Series *series,
                      int count, bool inverted, const Settings *settings,
                      int animation_progress) {
  int units = RING_STEPS * count + 1;
  int centre_x = bounds.origin.x + bounds.size.w / 2;
  int centre_y = bounds.origin.y + bounds.size.h / 2;
  int radius = BARS_MIN(bounds.size.w, bounds.size.h) / 2;
  GPoint centre = GPoint(centre_x, centre_y);

  if (count > 0 &&
      (settings->round_polar_fill & ROUND_POLAR_FILL_OUTSIDE)) {
    int horizontal_extent =
        BARS_MAX(centre_x - bounds.origin.x,
                 bounds.origin.x + bounds.size.w - centre_x);
    int vertical_extent =
        BARS_MAX(centre_y - bounds.origin.y,
                 bounds.origin.y + bounds.size.h - centre_y);
    // One pixel past the floor of the diagonal guarantees that the annulus
    // reaches every corner after integer rounding.
    int outside_radius =
        integer_sqrt(horizontal_extent * horizontal_extent +
                     vertical_extent * vertical_extent) +
        1;
    // Continue underneath the whole first ring instead of stopping at its
    // outer edge. Two radial calls rasterized at the same nominal radius can
    // otherwise leave a jagged one-pixel seam between them.
    int first_inner = radius * (units - RING_STEPS) / units;
    draw_round_polar_extension(ctx, centre, outside_radius, first_inner,
                               &series[0], inverted, animation_progress);
  }

  if (count > 0 &&
      (settings->round_polar_fill & ROUND_POLAR_FILL_CENTRE)) {
    // Likewise, paint underneath the whole last ring so the centre and ring
    // share pixels rather than meeting at two independently rounded edges.
    int last_outer =
        radius * (units - RING_STEPS * (count - 1)) / units;
    if (!settings->seamless) {
      last_outer = BARS_MAX(0, last_outer - 1);
    }
    draw_round_polar_extension(ctx, centre, last_outer, 0, &series[count - 1],
                               inverted, animation_progress);
  }

  for (int index = 0; index < count; ++index) {
    const Series *item = &series[index];
    int outer = radius * (units - RING_STEPS * index) / units;
    int inner = radius * (units - RING_STEPS * (index + 1)) / units;
    if (!settings->seamless) {
      // Rings are far thinner than a row, so one pixel is groove enough.
      outer -= 1;
    }
    int thickness = BARS_MAX(1, outer - inner);
    // Radius the label sits on: the middle of the ring's thickness.
    int middle = outer - thickness / 2;
    GRect ring = GRect(centre_x - outer, centre_y - outer, outer * 2, outer * 2);

    graphics_context_set_fill_color(ctx, item->track_color);
    graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, thickness, 0,
                         TRIG_MAX_ANGLE);

    int32_t sweep =
        TRIG_MAX_ANGLE * animated_ratio(item, animation_progress) / 1000;
    fill_round_progress_span(ctx, ring, thickness, inverted, 0, sweep,
                             item->bar_color);

    // Width of the ring at the label's height, rather than the ring's full
    // diameter: the top of a circle is narrower than its middle.
    int band_top = centre_y - outer;
    int half_chord =
        integer_sqrt(BARS_MAX(0, outer * outer - middle * middle));
    int max_text_width = BARS_MAX(1, 2 * half_chord - 2);
    SmoothFontSpec font_spec =
        smooth_font_for_series(item, max_text_width, thickness);
    int letter_spacing = condensed_letter_spacing(
        item->label, font_spec.font, font_spec.line_height, max_text_width);
    int text_width = smooth_spaced_width(item->label, font_spec.font,
                                         font_spec.line_height, letter_spacing);
    int text_height = font_spec.visible_height;
    int visible_y = band_top + (thickness - text_height) / 2;

    GRect frame = GRect(centre_x - text_width / 2,
                        visible_y - font_spec.top_offset, text_width + 3,
                        font_spec.line_height + 2);
    draw_smooth_text_letterwise(ctx, item->label, font_spec.font, frame,
                                item->text_color, settings->text_outline,
                                letter_spacing);
  }
}
