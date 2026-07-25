#include "renderer.h"

#include <string.h>

#include "renderer_internal.h"
#include "renderer_polar.h"
#include "renderer_text.h"

static void draw_horizontal(GContext *ctx, GRect bounds, const Series *series,
                            int count, bool inverted,
                            const Settings *settings,
                            int animation_progress) {
  int right = bounds.origin.x + bounds.size.w;

  for (int index = 0; index < count; ++index) {
    const Series *item = &series[index];
    int row_y = bounds.origin.y + bounds.size.h * index / count;
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
    int column_x = bounds.origin.x + bounds.size.w * index / count;
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
