#pragma once

#include "bars_types.h"

void draw_polar_rectangular(GContext *ctx, GRect bounds, const Series *series,
                            int count, bool inverted,
                            const Settings *settings, int animation_progress);
void draw_polar_round(GContext *ctx, GRect bounds, const Series *series,
                      int count, bool inverted, const Settings *settings,
                      int animation_progress);
