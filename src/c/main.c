#include <pebble.h>

#include "renderer.h"
#include "renderer_text.h"
#include "series.h"
#include "settings.h"

#define ANIMATION_INTERVAL_MS 33
#define ANIMATION_STEP 70

#ifdef BARS_SCREENSHOT_BUILD
#define SCREENSHOT_TIME_KEY 20000
#define SCREENSHOT_STEPS_KEY 20001
#endif

static Window *s_window;
static Layer *s_canvas_layer;
static Settings s_settings;
static AppTimer *s_animation_timer;
static int s_animation_progress = 1000;

static void mark_canvas_dirty(void) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// Eleven series of two label buffers each is close to a kilobyte, and the app
// task only gets four. Static keeps it off the render callback's stack, which
// still has the whole graphics library to fit below it.
static Series s_series[MAX_SERIES];

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect full_bounds = layer_get_bounds(layer);
  GRect content_bounds =
      layer_get_unobstructed_bounds(window_get_root_layer(s_window));
  int count = series_build(s_series, &s_settings);
  renderer_draw(ctx, full_bounds, content_bounds, s_series, count, &s_settings,
                s_animation_progress);
}

static void animation_timer_callback(void *context) {
  s_animation_timer = NULL;
  s_animation_progress += ANIMATION_STEP;
  if (s_animation_progress > 1000) {
    s_animation_progress = 1000;
  }
  mark_canvas_dirty();

  if (s_animation_progress < 1000) {
    s_animation_timer = app_timer_register(
        ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
  }
}

static void start_animation(void) {
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
    s_animation_timer = NULL;
  }

  if (!s_settings.animate) {
    s_animation_progress = 1000;
    mark_canvas_dirty();
    return;
  }

  s_animation_progress = 0;
  s_animation_timer = app_timer_register(
      ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_settings.clock_refresh_seconds >= 60 ||
      tick_time->tm_sec % s_settings.clock_refresh_seconds == 0) {
    // A tick is the one place the step count is allowed to go stale, so it
    // refreshes here rather than on every redraw.
    series_invalidate_steps();
    mark_canvas_dirty();
  }
}

static void update_tick_subscription(void) {
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(
      s_settings.clock_refresh_seconds < 60 ? SECOND_UNIT : MINUTE_UNIT,
      tick_handler);
}

static void battery_handler(BatteryChargeState state) {
  if (s_settings.series_visible[SERIES_BATTERY]) {
    mark_canvas_dirty();
  }
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (event != HealthEventMovementUpdate &&
      event != HealthEventSignificantUpdate) {
    return;
  }
  series_invalidate_steps();
  if (s_settings.series_visible[SERIES_STEPS]) {
    mark_canvas_dirty();
  }
}
#endif

static void unobstructed_change_handler(AnimationProgress progress,
                                        void *context) {
  mark_canvas_dirty();
}

static void unobstructed_did_change_handler(void *context) {
  mark_canvas_dirty();
}

static void inbox_received_handler(DictionaryIterator *iterator,
                                   void *context) {
  settings_apply_message(&s_settings, iterator);
#ifdef BARS_SCREENSHOT_BUILD
  Tuple *time_tuple = dict_find(iterator, SCREENSHOT_TIME_KEY);
  Tuple *steps_tuple = dict_find(iterator, SCREENSHOT_STEPS_KEY);
  if (time_tuple && steps_tuple) {
    series_set_screenshot_state((time_t)time_tuple->value->uint32,
                                steps_tuple->value->int32);
  }
#endif
  settings_save(&s_settings);
  update_tick_subscription();
  window_set_background_color(s_window, s_settings.background_color);
  start_animation();
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Settings message dropped: %d", reason);
}

static void main_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root_layer));
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(root_layer, s_canvas_layer);

  UnobstructedAreaHandlers handlers = {
      .change = unobstructed_change_handler,
      .did_change = unobstructed_did_change_handler};
  unobstructed_area_service_subscribe(handlers, NULL);
  start_animation();
}

static void main_window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
}

static void init(void) {
  settings_load(&s_settings);

  s_window = window_create();
  window_set_background_color(s_window, s_settings.background_color);
  window_set_window_handlers(
      s_window,
      (WindowHandlers){.load = main_window_load,
                       .unload = main_window_unload});
  window_stack_push(s_window, true);

  update_tick_subscription();
  battery_state_service_subscribe(battery_handler);
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
#endif

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  // The complete settings dictionary stays below 700 bytes.
  app_message_open(1024, 128);
}

static void deinit(void) {
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
    s_animation_timer = NULL;
  }
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  app_message_deregister_callbacks();
  window_destroy(s_window);
  renderer_text_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
