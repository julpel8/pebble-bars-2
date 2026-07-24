#include "renderer.h"

#include <string.h>

#define BARS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BARS_MAX(a, b) ((a) > (b) ? (a) : (b))

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

static bool rects_intersect(GRect first, GRect second) {
  return first.origin.x < second.origin.x + second.size.w &&
         first.origin.x + first.size.w > second.origin.x &&
         first.origin.y < second.origin.y + second.size.h &&
         first.origin.y + first.size.h > second.origin.y;
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

static SmoothFontSpec smooth_font_for_series(const Series *series,
                                             int max_width,
                                             int max_visible_height) {
  static const SmoothFontCandidate leco_fonts[] = {
      {FONT_KEY_LECO_42_NUMBERS, 42, 29, 13},
      {FONT_KEY_LECO_38_BOLD_NUMBERS, 38, 27, 11},
      {FONT_KEY_LECO_36_BOLD_NUMBERS, 36, 25, 11},
      {FONT_KEY_LECO_32_BOLD_NUMBERS, 32, 22, 10},
      {FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM, 26, 18, 8},
      {FONT_KEY_LECO_20_BOLD_NUMBERS, 20, 14, 6}};

  const SmoothFontCandidate *fonts = leco_fonts;
  int font_count = (int)(sizeof(leco_fonts) / sizeof(leco_fonts[0]));

  SmoothFontSpec fallback = {
      .font = fonts_get_system_font(fonts[font_count - 1].key),
      .line_height = fonts[font_count - 1].line_height,
      .visible_height = fonts[font_count - 1].visible_height,
      .top_offset = fonts[font_count - 1].top_offset};

  const bool is_two_digit_value =
      series->id == SERIES_HOUR || series->id == SERIES_MINUTE ||
      series->id == SERIES_DATE || series->id == SERIES_SECOND;
  const char *measurement_text = is_two_digit_value
                                     ? "00"
                                     : (series->id == SERIES_BATTERY
                                            ? "100%"
                                            : series->label);

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
                             GRect frame, GColor color,
                             GTextAlignment alignment) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, frame, GTextOverflowModeTrailingEllipsis,
                     alignment, NULL);
}

static void draw_smooth_fill_label(
    GContext *ctx, const char *text, GFont font, GRect frame, GColor color,
    GTextAlignment alignment, bool outlined) {
  if (outlined) {
    static const int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
    static const int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};
    for (int index = 0; index < 8; ++index) {
      GRect outline_frame = frame;
      outline_frame.origin.x += dx[index];
      outline_frame.origin.y += dy[index];
      draw_smooth_text(ctx, text, font, outline_frame, GColorBlack, alignment);
    }
  }
  draw_smooth_text(ctx, text, font, frame, color, alignment);
}

static void draw_smooth_text_letterwise(
    GContext *ctx, const char *text, GFont font, GRect frame, int visible_y,
    int visible_height, GColor track_color, GColor bar_color, GRect bar_clip,
    bool outlined, int letter_spacing) {
  int glyph_x = frame.origin.x;
  size_t offset = 0;
  while (text[offset] != '\0') {
    size_t bytes = utf8_glyph_bytes(text + offset);
    char glyph[5] = {0};
    memcpy(glyph, text + offset, bytes);

    int glyph_width = smooth_glyph_width(glyph, font, frame.size.h - 2);
    GRect glyph_frame =
        GRect(glyph_x, frame.origin.y, glyph_width + 3, frame.size.h);
    GRect glyph_visible =
        GRect(glyph_x, visible_y, glyph_width, visible_height);
    if (rects_intersect(glyph_visible, bar_clip)) {
      draw_smooth_fill_label(ctx, glyph, font, glyph_frame, bar_color,
                             GTextAlignmentLeft, outlined);
    } else {
      draw_smooth_text(ctx, glyph, font, glyph_frame, track_color,
                       GTextAlignmentLeft);
    }

    glyph_x += glyph_width + letter_spacing;
    offset += bytes;
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
    graphics_context_set_fill_color(ctx, settings->track_color);
    graphics_fill_rect(ctx, track, 0, GCornerNone);

    int fill_width =
        (bounds.size.w * animated_ratio(item, animation_progress) + 500) /
        1000;
    int fill_x = inverted ? right - fill_width : bounds.origin.x;
    graphics_context_set_fill_color(ctx, item->bar_color);
    graphics_fill_rect(ctx, GRect(fill_x, row_y, fill_width, bar_height), 0,
                       GCornerNone);

    GRect bar_clip = GRect(fill_x, row_y, fill_width, bar_height);
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
        ctx, item->label, font_spec.font, frame, visible_y, text_height,
        item->text_color, item->text_on_bar_color, bar_clip,
        settings->text_outline, 0);
  }
}

static void draw_vertical_battery_label(
    GContext *ctx, const Series *item, SmoothFontSpec font_spec, int column_x,
    int column_width, int visible_y, int text_height, GRect bar_clip,
    bool outlined) {
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
      ctx, digits, font_spec.font, frame, visible_y, text_height,
      item->text_color, item->text_on_bar_color, bar_clip, outlined,
      letter_spacing);

  int percent_x = text_x + digit_width + gap;
  int percent_y = visible_y + (text_height - percent_height) / 2;
  GRect percent_rect =
      GRect(percent_x, percent_y, percent_width, percent_height);
  GColor percent_color = rects_intersect(percent_rect, bar_clip)
                             ? item->text_on_bar_color
                             : item->text_color;
  draw_small_percent(ctx, percent_x, percent_y, percent_color, outlined);
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
    graphics_context_set_fill_color(ctx, settings->track_color);
    graphics_fill_rect(ctx, track, 0, GCornerNone);

    int fill_height =
        (available_height * animated_ratio(item, animation_progress) + 500) /
        1000;
    int fill_y = inverted ? top : bottom - fill_height;
    graphics_context_set_fill_color(ctx, item->bar_color);
    graphics_fill_rect(ctx,
                       GRect(column_x, fill_y, bar_width, fill_height), 0,
                       GCornerNone);

    GRect bar_clip = GRect(column_x, fill_y, bar_width, fill_height);
    int max_text_width = BARS_MAX(1, column_width - 2);
    Series display_item = *item;
    SmoothFontSpec font_spec =
        smooth_font_for_series(&display_item, max_text_width, 29);
    bool is_date_label =
        item->id == SERIES_MONTH || item->id == SERIES_DAY;
    int full_label_width =
        smooth_letterwise_width(item->label, font_spec.font,
                                font_spec.line_height);
    if (is_date_label && full_label_width > max_text_width && count > 5) {
      copy_utf8_glyphs(item->label, display_item.label,
                       sizeof(display_item.label), 2);
      font_spec =
          smooth_font_for_series(&display_item, max_text_width, 29);
    }
    const char *display_label = display_item.label;
    int text_height = font_spec.visible_height;
    int visible_y =
        placement_lead(settings->text_placement, top, bottom, fill_y,
                       fill_y + fill_height, inverted, text_height, 2);
    if (item->id == SERIES_BATTERY) {
      draw_vertical_battery_label(
          ctx, item, font_spec, column_x, column_width, visible_y, text_height,
          bar_clip, settings->text_outline);
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
        ctx, display_label, font_spec.font, frame, visible_y, text_height,
        item->text_color, item->text_on_bar_color, bar_clip,
        settings->text_outline, letter_spacing);
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
    case STYLE_HORIZONTAL:
    default:
      draw_horizontal(ctx, content_bounds, series, count, false, settings,
                      animation_progress);
      break;
  }
}
