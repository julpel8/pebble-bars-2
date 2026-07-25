#pragma once

#include "bars_types.h"

typedef struct {
  GFont font;
  int line_height;
  int visible_height;
  int top_offset;
} SmoothFontSpec;

int smooth_letterwise_width(const char *text, GFont font, int line_height);
int utf8_glyph_count(const char *text);
void copy_utf8_glyphs(const char *source, char *destination,
                      size_t destination_size, int max_glyphs);
int condensed_letter_spacing(const char *text, GFont font, int line_height,
                             int max_width);
int smooth_spaced_width(const char *text, GFont font, int line_height,
                        int letter_spacing);
SmoothFontSpec smooth_font_for_series(const Series *series, int max_width,
                                      int max_visible_height);
void draw_smooth_text_letterwise(GContext *ctx, const char *text, GFont font,
                                 GRect frame, GColor color, bool outlined,
                                 int letter_spacing);
void draw_small_percent(GContext *ctx, int x, int y, GColor color,
                        bool outlined);
int placement_lead(uint8_t placement, int lo, int hi, int fill_lo, int fill_hi,
                   bool origin_at_lo, int text_len, int gap);
