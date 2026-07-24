#pragma once

#include "bars_types.h"

void settings_load(Settings *settings);
void settings_save(const Settings *settings);
void settings_apply_message(Settings *settings, DictionaryIterator *iterator);
