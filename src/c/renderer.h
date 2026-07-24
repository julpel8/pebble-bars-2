#pragma once

#include "bars_types.h"

void renderer_draw(GContext *ctx, GRect full_bounds, GRect content_bounds,
                   const Series *series, int count, const Settings *settings,
                   int animation_progress);
