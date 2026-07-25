#pragma once

#include "bars_types.h"

int series_build(Series output[MAX_SERIES], const Settings *settings);

// Drops the cached step count so the next build asks Health again. Reading the
// counter is a round trip to another task, so it happens on a health event or a
// tick rather than on every redraw.
void series_invalidate_steps(void);
