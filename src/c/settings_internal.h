#pragma once

#include "bars_types.h"

int clamp_int(int value, int minimum, int maximum);
uint8_t valid_clock_refresh(int value);
bool apply_series_order(const uint8_t *data, size_t length,
                        uint8_t order[MAX_SERIES]);
bool apply_visible_mask(uint32_t mask, bool visible[MAX_SERIES]);
