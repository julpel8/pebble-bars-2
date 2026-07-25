#include "renderer.h"

#include <string.h>

#define BARS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BARS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BARS_ARRAY_LENGTH(array) ((int)(sizeof(array) / sizeof((array)[0])))

typedef struct {
  GFont font;
  int line_height;
  int visible_height;
  int top_offset;
} SmoothFontSpec;

typedef struct {
  const char *key;
  uint8_t line_height;
  uint8_t visible_height;
  uint8_t top_offset;
} SmoothFontCandidate;

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

static int animated_ratio(const Series *series, int animation_progress) {
  if (series->maximum <= 0) {
    return 0;
  }
  int ratio = BARS_MAX(
      0, BARS_MIN(1000, (int)((int64_t)series->value * 1000 /
                              series->maximum)));
  return ratio * animation_progress / 1000;
}

static GSize smooth_text_size(const char *text, GFont font, int max_width,
                              int max_height) {
  return graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, max_width, max_height),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
}

// Returns the byte length of the next UTF-8 code point.
static size_t utf8_glyph_bytes(const char *text) {
  const unsigned char first = (unsigned char)text[0];
  size_t length = 1;
  if ((first & 0xE0) == 0xC0) {
    length = 2;
  } else if ((first & 0xF0) == 0xE0) {
    length = 3;
  } else if ((first & 0xF8) == 0xF0) {
    length = 4;
  }

  for (size_t index = 1; index < length; ++index) {
    if (text[index] == '\0' ||
        ((unsigned char)text[index] & 0xC0) != 0x80) {
      return 1;
    }
  }
  return length;
}

static int smooth_glyph_width(const char *glyph, GFont font,
                              int line_height) {
  GSize size =
      smooth_text_size(glyph, font, 200, BARS_MAX(1, line_height + 2));
  return BARS_MAX(1, size.w);
}

static int smooth_letterwise_width(const char *text, GFont font,
                                   int line_height) {
  int width = 0;
  size_t offset = 0;
  while (text[offset] != '\0') {
    size_t bytes = utf8_glyph_bytes(text + offset);
    char glyph[5] = {0};
    memcpy(glyph, text + offset, bytes);
    width += smooth_glyph_width(glyph, font, line_height);
    offset += bytes;
  }
  return width;
}

static int utf8_glyph_count(const char *text) {
  int count = 0;
  size_t offset = 0;
  while (text[offset] != '\0') {
    offset += utf8_glyph_bytes(text + offset);
    ++count;
  }
  return count;
}

static void copy_utf8_glyphs(const char *source, char *destination,
                             size_t destination_size, int max_glyphs) {
  size_t source_offset = 0;
  size_t destination_offset = 0;
  int glyph_count = 0;
  while (source[source_offset] != '\0' && glyph_count < max_glyphs) {
    size_t bytes = utf8_glyph_bytes(source + source_offset);
    if (destination_offset + bytes >= destination_size) {
      break;
    }
    memcpy(destination + destination_offset, source + source_offset, bytes);
    source_offset += bytes;
    destination_offset += bytes;
    ++glyph_count;
  }
  destination[destination_offset] = '\0';
}

static int condensed_letter_spacing(const char *text, GFont font,
                                    int line_height, int max_width) {
  int glyph_count = utf8_glyph_count(text);
  int natural_width = smooth_letterwise_width(text, font, line_height);
  if (glyph_count < 2 || natural_width <= max_width) {
    return 0;
  }
  int gaps = glyph_count - 1;
  return -((natural_width - max_width + gaps - 1) / gaps);
}

static int smooth_spaced_width(const char *text, GFont font, int line_height,
                               int letter_spacing) {
  int glyph_count = utf8_glyph_count(text);
  return smooth_letterwise_width(text, font, line_height) +
         BARS_MAX(0, glyph_count - 1) * letter_spacing;
}

// Largest first: the label takes the biggest size that still fits the bar, so
// the text grows and shrinks with the number of bars on screen.
static const SmoothFontCandidate s_leco_fonts[] = {
    {FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM, 60, 42, 18},
    {FONT_KEY_LECO_42_NUMBERS, 42, 29, 13},
    {FONT_KEY_LECO_38_BOLD_NUMBERS, 38, 27, 11},
    {FONT_KEY_LECO_36_BOLD_NUMBERS, 36, 25, 11},
    {FONT_KEY_LECO_32_BOLD_NUMBERS, 32, 22, 10},
    {FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM, 26, 18, 8},
    {FONT_KEY_LECO_20_BOLD_NUMBERS, 20, 14, 6}};

static SmoothFontSpec smooth_font_for_series(const Series *series,
                                             int max_width,
                                             int max_visible_height) {
  const SmoothFontCandidate *fonts = s_leco_fonts;
  int font_count = BARS_ARRAY_LENGTH(s_leco_fonts);

  SmoothFontSpec fallback = {
      .font = fonts_get_system_font(fonts[font_count - 1].key),
      .line_height = fonts[font_count - 1].line_height,
      .visible_height = fonts[font_count - 1].visible_height,
      .top_offset = fonts[font_count - 1].top_offset};

  const char *measurement_text = series->measure;

  for (int index = 0; index < font_count; ++index) {
    if (fonts[index].visible_height > max_visible_height) {
      continue;
    }
    GFont font = fonts_get_system_font(fonts[index].key);
    int width = smooth_letterwise_width(measurement_text, font,
                                        fonts[index].line_height);
    if (width <= max_width) {
      return (SmoothFontSpec){
          .font = font,
          .line_height = fonts[index].line_height,
          .visible_height = fonts[index].visible_height,
          .top_offset = fonts[index].top_offset};
    }
  }
  return fallback;
}

static void draw_smooth_text(GContext *ctx, const char *text, GFont font,
                             GRect frame, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void draw_smooth_text_outline(GContext *ctx, const char *text,
                                     GFont font, GRect frame) {
  static const int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
  static const int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};
  for (int index = 0; index < 8; ++index) {
    GRect outline_frame = frame;
    outline_frame.origin.x += dx[index];
    outline_frame.origin.y += dy[index];
    draw_smooth_text(ctx, text, font, outline_frame, GColorBlack);
  }
}

static void draw_smooth_text_letterwise(
    GContext *ctx, const char *text, GFont font, GRect frame, GColor color,
    bool outlined, int letter_spacing) {
  // The outline runs as its own pass over the whole label: drawn per glyph, a
  // neighbour's outline would land on top of an already filled glyph.
  for (int pass = outlined ? 0 : 1; pass < 2; ++pass) {
    int glyph_x = frame.origin.x;
    size_t offset = 0;
    while (text[offset] != '\0') {
      size_t bytes = utf8_glyph_bytes(text + offset);
      char glyph[5] = {0};
      memcpy(glyph, text + offset, bytes);

      int glyph_width = smooth_glyph_width(glyph, font, frame.size.h - 2);
      GRect glyph_frame =
          GRect(glyph_x, frame.origin.y, glyph_width + 3, frame.size.h);
      if (pass == 0) {
        draw_smooth_text_outline(ctx, glyph, font, glyph_frame);
      } else {
        draw_smooth_text(ctx, glyph, font, glyph_frame, color);
      }

      glyph_x += glyph_width + letter_spacing;
      offset += bytes;
    }
  }
}

static void draw_small_percent_glyph(GContext *ctx, int x, int y,
                                     GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(x, y, 2, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 3, y + 5, 2, 2), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, color);
  graphics_draw_line(ctx, GPoint(x + 4, y), GPoint(x, y + 6));
}

static void draw_small_percent(GContext *ctx, int x, int y, GColor color,
                               bool outlined) {
  if (outlined) {
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};
    for (int index = 0; index < 4; ++index) {
      draw_small_percent_glyph(ctx, x + dx[index], y + dy[index],
                               GColorBlack);
    }
  }
  draw_small_percent_glyph(ctx, x, y, color);
}

static int placement_lead(uint8_t placement, int lo, int hi, int fill_lo,
                          int fill_hi, bool origin_at_lo, int text_len,
                          int gap) {
  int fill_edge = origin_at_lo ? fill_hi : fill_lo;
  int lead;
  switch (placement) {
    case TEXT_PLACE_OUTSIDE_END:
      lead = origin_at_lo ? (hi - text_len - gap) : (lo + gap);
      break;
    case TEXT_PLACE_OUTSIDE_MIDDLE:
      lead = origin_at_lo ? (fill_hi + hi) / 2 - text_len / 2
                          : (lo + fill_lo) / 2 - text_len / 2;
      break;
    case TEXT_PLACE_ALWAYS_MIDDLE:
      lead = (lo + hi) / 2 - text_len / 2;
      break;
    case TEXT_PLACE_INSIDE_START:
      lead = origin_at_lo ? (lo + gap) : (hi - text_len - gap);
      break;
    case TEXT_PLACE_INSIDE_MIDDLE:
      lead = (fill_lo + fill_hi) / 2 - text_len / 2;
      break;
    case TEXT_PLACE_INSIDE_END:
      lead = origin_at_lo ? (fill_edge - gap - text_len) : (fill_edge + gap);
      break;
    case TEXT_PLACE_OUTSIDE_START:
    default:
      lead = origin_at_lo ? (fill_edge + gap) : (fill_edge - gap - text_len);
      break;
  }
  if (lead < lo + 1) {
    lead = lo + 1;
  }
  if (lead + text_len > hi - 1) {
    lead = hi - 1 - text_len;
  }
  return lead;
}

static void draw_horizontal(GContext *ctx, GRect bounds, const Series *series,
                            int count, bool inverted,
                            const Settings *settings,
                            int animation_progress) {
  int right = bounds.origin.x + bounds.size.w;

  for (int index = 0; index < count; ++index) {
    const Series *item = &series[index];
    int row_y =
        bounds.origin.y + bounds.size.h * index / count;
    int next_row_y =
        bounds.origin.y + bounds.size.h * (index + 1) / count;
    int row_height = next_row_y - row_y;
    int bar_height =
        settings->seamless ? row_height : BARS_MAX(4, row_height - 2);
    GRect track = GRect(bounds.origin.x, row_y, bounds.size.w, bar_height);
    graphics_context_set_fill_color(ctx, item->track_color);
    graphics_fill_rect(ctx, track, 0, GCornerNone);

    int fill_width =
        (bounds.size.w * animated_ratio(item, animation_progress) + 500) /
        1000;
    int fill_x = inverted ? right - fill_width : bounds.origin.x;
    graphics_context_set_fill_color(ctx, item->bar_color);
    graphics_fill_rect(ctx, GRect(fill_x, row_y, fill_width, bar_height), 0,
                       GCornerNone);
    SmoothFontSpec font_spec =
        smooth_font_for_series(item, bounds.size.w - 4, bar_height + 1);
    int text_width = smooth_letterwise_width(
        item->label, font_spec.font, font_spec.line_height);
    int text_height = font_spec.visible_height;
    int text_x =
        placement_lead(settings->text_placement, bounds.origin.x, right, fill_x,
                       fill_x + fill_width, !inverted, text_width, 3);
    int visible_y = row_y + (bar_height - text_height) / 2;
    GRect frame =
        GRect(text_x, visible_y - font_spec.top_offset, text_width + 3,
              font_spec.line_height + 2);
    draw_smooth_text_letterwise(
        ctx, item->label, font_spec.font, frame, item->text_color,
        settings->text_outline, 0);
  }
}

static void draw_vertical_battery_label(
    GContext *ctx, const Series *item, SmoothFontSpec font_spec, int column_x,
    int column_width, int visible_y, int text_height, bool outlined) {
  const char *percent = strchr(item->label, '%');
  if (!percent) {
    return;
  }

  char digits[MAX_LABEL_BYTES] = {0};
  size_t digit_bytes = (size_t)(percent - item->label);
  digit_bytes = BARS_MIN(digit_bytes, sizeof(digits) - 1);
  memcpy(digits, item->label, digit_bytes);

  const int percent_width = 5;
  const int percent_height = 7;
  const int gap = 1;
  int content_width = BARS_MAX(1, column_width - 2);
  int digit_max_width = BARS_MAX(1, content_width - gap - percent_width);
  int letter_spacing = condensed_letter_spacing(
      digits, font_spec.font, font_spec.line_height, digit_max_width);
  int digit_width = smooth_spaced_width(
      digits, font_spec.font, font_spec.line_height, letter_spacing);
  int total_width = digit_width + gap + percent_width;
  int text_x = column_x + (column_width - total_width) / 2;
  GRect frame =
      GRect(text_x, visible_y - font_spec.top_offset, digit_width + 3,
            font_spec.line_height + 2);
  draw_smooth_text_letterwise(
      ctx, digits, font_spec.font, frame, item->text_color, outlined,
      letter_spacing);

  int percent_x = text_x + digit_width + gap;
  int percent_y = visible_y + (text_height - percent_height) / 2;
  draw_small_percent(ctx, percent_x, percent_y, item->text_color, outlined);
}

static void draw_vertical(GContext *ctx, GRect bounds, const Series *series,
                          int count, bool inverted, const Settings *settings,
                          int animation_progress) {
  int top = bounds.origin.y;
  int bottom = bounds.origin.y + bounds.size.h;
  int available_height = bottom - top;

  for (int index = 0; index < count; ++index) {
    const Series *item = &series[index];
    int column_x =
        bounds.origin.x + bounds.size.w * index / count;
    int next_column_x =
        bounds.origin.x + bounds.size.w * (index + 1) / count;
    int column_width = next_column_x - column_x;
    int bar_width =
        settings->seamless ? column_width : BARS_MAX(3, column_width - 2);
    GRect track = GRect(column_x, top, bar_width, available_height);
    graphics_context_set_fill_color(ctx, item->track_color);
    graphics_fill_rect(ctx, track, 0, GCornerNone);

    int fill_height =
        (available_height * animated_ratio(item, animation_progress) + 500) /
        1000;
    int fill_y = inverted ? top : bottom - fill_height;
    graphics_context_set_fill_color(ctx, item->bar_color);
    graphics_fill_rect(ctx,
                       GRect(column_x, fill_y, bar_width, fill_height), 0,
                       GCornerNone);
    int max_text_width = BARS_MAX(1, column_width - 2);
    Series display_item = *item;
    // Columns run the full height, so the bar's width is what limits the font.
    SmoothFontSpec font_spec =
        smooth_font_for_series(&display_item, max_text_width,
                               available_height);
    bool is_word_label = item->id == SERIES_MONTH || item->id == SERIES_DAY ||
                         item->id == SERIES_CUSTOM;
    int full_label_width =
        smooth_letterwise_width(item->label, font_spec.font,
                                font_spec.line_height);
    if (is_word_label && full_label_width > max_text_width && count > 5) {
      copy_utf8_glyphs(item->label, display_item.label,
                       sizeof(display_item.label), 2);
      copy_utf8_glyphs(item->measure, display_item.measure,
                       sizeof(display_item.measure), 2);
      font_spec = smooth_font_for_series(&display_item, max_text_width,
                                         available_height);
    }
    const char *display_label = display_item.label;
    int text_height = font_spec.visible_height;
    int visible_y =
        placement_lead(settings->text_placement, top, bottom, fill_y,
                       fill_y + fill_height, inverted, text_height, 2);
    if (item->id == SERIES_BATTERY) {
      draw_vertical_battery_label(
          ctx, item, font_spec, column_x, column_width, visible_y, text_height,
          settings->text_outline);
      continue;
    }
    int letter_spacing = condensed_letter_spacing(
        display_label, font_spec.font, font_spec.line_height, max_text_width);
    int text_width = smooth_letterwise_width(
        display_label, font_spec.font, font_spec.line_height);
    text_width += BARS_MAX(0, utf8_glyph_count(display_label) - 1) *
                  letter_spacing;
    GRect frame =
        GRect(column_x + (column_width - text_width) / 2,
              visible_y - font_spec.top_offset, text_width + 3,
              font_spec.line_height + 2);
    draw_smooth_text_letterwise(
        ctx, display_label, font_spec.font, frame, item->text_color,
        settings->text_outline, letter_spacing);
  }
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
  // Pixels at the start already covered by the preceding segment's corner.
  int overlap;
} PolarSegment;

static int segment_length(const PolarSegment *segment) {
  // Rectangular polar progress follows the outside perimeter. In particular,
  // a vertical run has the full outer height, not only the shorter opening
  // between the top and bottom bands.
  return segment->horizontal ? segment->rect.size.w : segment->rect.size.h;
}

static GRect segment_fill(const PolarSegment *segment, int filled) {
  GRect rect = segment->rect;
  int physical_length = segment->horizontal ? rect.size.w : rect.size.h;
  int physical_filled =
      BARS_MIN(physical_length, BARS_MAX(segment->overlap, filled));
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
// the bottom band, the other side, then the rest of the top band. Adjacent runs
// overlap at corners while progress follows the outer edge, so every completed
// run includes its destination corner.
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
  int bottom_height = bottom - inner_bottom;
  int left_width = inner_left - left;
  int right_width = right - inner_right;
  int top_left_width = centre_x - left;
  int top_right_width = right - centre_x;

  GRect top_right = GRect(centre_x, top, top_right_width, top_height);
  GRect top_left = GRect(left, top, top_left_width, top_height);
  // Side fills overlap the top or bottom corner already painted by the
  // preceding horizontal run. Their new portion includes the opposite corner,
  // so a completed vertical run visibly reaches the very bottom or top.
  GRect right_band = GRect(inner_right, top, right_width, bottom - top);
  GRect left_band = GRect(left, top, left_width, bottom - top);
  GRect bottom_band =
      GRect(left, inner_bottom, right - left, bottom - inner_bottom);

  int index = 0;
  if (inverted) {
    segments[index++] = (PolarSegment){top_left, true, false, 0};
    segments[index++] =
        (PolarSegment){left_band, false, true, top_height};
    segments[index++] =
        (PolarSegment){bottom_band, true, true, left_width};
    segments[index++] =
        (PolarSegment){right_band, false, false, bottom_height};
    segments[index++] =
        (PolarSegment){top_right, true, false, right_width};
  } else {
    segments[index++] = (PolarSegment){top_right, true, true, 0};
    segments[index++] =
        (PolarSegment){right_band, false, true, top_height};
    segments[index++] =
        (PolarSegment){bottom_band, true, false, right_width};
    segments[index++] =
        (PolarSegment){left_band, false, false, bottom_height};
    segments[index++] =
        (PolarSegment){top_left, true, true, left_width};
  }
  return index;
}

// Steps each ring takes out of the screen, against the single step left empty
// in the middle: the wider the ring, the smaller the hole at the centre.
#define RING_STEPS 3

static void draw_polar_rectangular(GContext *ctx, GRect bounds,
                                   const Series *series, int count,
                                   bool inverted, const Settings *settings,
                                   int animation_progress) {
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
                                     bool inverted, int32_t from,
                                     int32_t to, GColor color) {
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
static void draw_polar_round(GContext *ctx, GRect bounds, const Series *series,
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

void renderer_draw(GContext *ctx, GRect full_bounds, GRect content_bounds,
                   const Series *series, int count, const Settings *settings,
                   int animation_progress) {
  graphics_context_set_fill_color(ctx, settings->background_color);
  graphics_fill_rect(ctx, full_bounds, 0, GCornerNone);

  switch (settings->style) {
    case STYLE_HORIZONTAL_INVERTED:
      draw_horizontal(ctx, content_bounds, series, count, true, settings,
                      animation_progress);
      break;
    case STYLE_VERTICAL:
      draw_vertical(ctx, content_bounds, series, count, false, settings,
                    animation_progress);
      break;
    case STYLE_VERTICAL_INVERTED:
      draw_vertical(ctx, content_bounds, series, count, true, settings,
                    animation_progress);
      break;
    case STYLE_POLAR_RECTANGULAR:
      draw_polar_rectangular(ctx, content_bounds, series, count, false,
                             settings, animation_progress);
      break;
    case STYLE_POLAR_RECTANGULAR_INVERTED:
      draw_polar_rectangular(ctx, content_bounds, series, count, true,
                             settings, animation_progress);
      break;
    case STYLE_POLAR_ROUND:
      draw_polar_round(ctx, content_bounds, series, count, false, settings,
                       animation_progress);
      break;
    case STYLE_POLAR_ROUND_INVERTED:
      draw_polar_round(ctx, content_bounds, series, count, true, settings,
                       animation_progress);
      break;
    case STYLE_HORIZONTAL:
    default:
      draw_horizontal(ctx, content_bounds, series, count, false, settings,
                      animation_progress);
      break;
  }
}
