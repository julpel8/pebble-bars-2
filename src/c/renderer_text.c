#include "renderer_text.h"

#include <string.h>

#define BARS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BARS_ARRAY_LENGTH(array) ((int)(sizeof(array) / sizeof((array)[0])))

typedef struct {
  const char *key;
  uint8_t line_height;
  uint8_t visible_height;
  uint8_t top_offset;
} SmoothFontCandidate;

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

int smooth_letterwise_width(const char *text, GFont font, int line_height) {
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

int utf8_glyph_count(const char *text) {
  int count = 0;
  size_t offset = 0;
  while (text[offset] != '\0') {
    offset += utf8_glyph_bytes(text + offset);
    ++count;
  }
  return count;
}

void copy_utf8_glyphs(const char *source, char *destination,
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

int condensed_letter_spacing(const char *text, GFont font, int line_height,
                             int max_width) {
  int glyph_count = utf8_glyph_count(text);
  int natural_width = smooth_letterwise_width(text, font, line_height);
  if (glyph_count < 2 || natural_width <= max_width) {
    return 0;
  }
  int gaps = glyph_count - 1;
  return -((natural_width - max_width + gaps - 1) / gaps);
}

int smooth_spaced_width(const char *text, GFont font, int line_height,
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

SmoothFontSpec smooth_font_for_series(const Series *series, int max_width,
                                      int max_visible_height) {
  const SmoothFontCandidate *fonts = s_leco_fonts;
  int font_count = BARS_ARRAY_LENGTH(s_leco_fonts);

  SmoothFontSpec fallback = {
      .font = fonts_get_system_font(fonts[font_count - 1].key),
      .line_height = fonts[font_count - 1].line_height,
      .visible_height = fonts[font_count - 1].visible_height,
      .top_offset = fonts[font_count - 1].top_offset};

  for (int index = 0; index < font_count; ++index) {
    if (fonts[index].visible_height > max_visible_height) {
      continue;
    }
    GFont font = fonts_get_system_font(fonts[index].key);
    int width = smooth_letterwise_width(series->measure, font,
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

void draw_smooth_text_letterwise(GContext *ctx, const char *text, GFont font,
                                 GRect frame, GColor color, bool outlined,
                                 int letter_spacing) {
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

void draw_small_percent(GContext *ctx, int x, int y, GColor color,
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

int placement_lead(uint8_t placement, int lo, int hi, int fill_lo, int fill_hi,
                   bool origin_at_lo, int text_len, int gap) {
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
