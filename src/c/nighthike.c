#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_label;
static TextLayer *s_time_layer;
static TextLayer *s_location_label;
static TextLayer *s_latitude_layer;
static TextLayer *s_longitude_layer;
static TextLayer *s_status_layer;
static TextLayer *s_accuracy_layer;
static TextLayer *s_heading_layer;
static TextLayer *s_altitude_layer;
static TextLayer *s_heart_rate_layer;
static TextLayer *s_heart_icon_layer;
static TextLayer *s_metric_labels[4];
static GFont s_label_font;
static GFont s_time_font;
static GFont s_coordinate_font;
static GFont s_detail_font;
static char s_accuracy_text[24];
static char s_heading_text[24];
static char s_altitude_text[24];
static char s_heart_rate_text[24];
static char s_time_text[12];
static char s_latitude_text[20];
static char s_longitude_text[20];
static bool s_has_location;
static bool s_app_focused;
static int32_t s_accuracy_meters = -1;
static int32_t s_altitude_meters;
static bool s_altitude_valid;

#define SETTINGS_KEY 1

typedef struct {
  bool imperial_units;
  bool use_system_backlight;
  uint32_t backlight_color;
} WatchSettings;

static WatchSettings s_settings = {
  .imperial_units = false,
  .use_system_backlight = false,
  .backlight_color = 0xFF0000
};

static void prv_save_settings(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void prv_load_settings(void) {
  if(persist_exists(SETTINGS_KEY) &&
     persist_get_size(SETTINGS_KEY) == sizeof(s_settings)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  }
}

static void prv_apply_backlight(void) {
  if(s_app_focused && !s_settings.use_system_backlight) {
    (void)light_set_color_rgb888(s_settings.backlight_color);
  } else {
    (void)light_set_system_color();
  }
}

static void prv_update_accuracy_display(void) {
  if(s_accuracy_meters < 0) {
    text_layer_set_text(s_accuracy_layer, s_settings.imperial_units ? "--ft" : "--m");
    return;
  }
  if(!s_settings.imperial_units) {
    if(s_accuracy_meters > 99999) {
      text_layer_set_text(s_accuracy_layer, ">99km");
    } else {
      snprintf(s_accuracy_text, sizeof(s_accuracy_text), "%ldm",
               (long)s_accuracy_meters);
      text_layer_set_text(s_accuracy_layer, s_accuracy_text);
    }
    return;
  }
  if(s_accuracy_meters >= 159325) {
    text_layer_set_text(s_accuracy_layer, ">99mi");
  } else if(s_accuracy_meters > 304) {
    int32_t miles = (s_accuracy_meters * 100 + 160934 - 1) / 160934;
    snprintf(s_accuracy_text, sizeof(s_accuracy_text), "%ldmi", (long)miles);
    text_layer_set_text(s_accuracy_layer, s_accuracy_text);
  } else {
    int32_t feet = (int32_t)(((int64_t)s_accuracy_meters * 328084 + 99999) / 100000);
    snprintf(s_accuracy_text, sizeof(s_accuracy_text), "%ldft", (long)feet);
    text_layer_set_text(s_accuracy_layer, s_accuracy_text);
  }
}

static void prv_update_altitude_display(void) {
  if(!s_altitude_valid) {
    text_layer_set_text(s_altitude_layer, s_settings.imperial_units ? "--ft" : "--m");
    return;
  }
  if(!s_settings.imperial_units) {
    if(s_altitude_meters > 9999 || s_altitude_meters < -9999) {
      snprintf(s_altitude_text, sizeof(s_altitude_text), "%s9km",
               s_altitude_meters < 0 ? "<-" : ">");
    } else {
      snprintf(s_altitude_text, sizeof(s_altitude_text), "%ldm", (long)s_altitude_meters);
    }
    text_layer_set_text(s_altitude_layer, s_altitude_text);
    return;
  }
  int64_t feet = ((int64_t)s_altitude_meters * 328084 +
                  (s_altitude_meters < 0 ? -50000 : 50000)) / 100000;
  if(feet > 47520 || feet < -47520) {
    snprintf(s_altitude_text, sizeof(s_altitude_text), "%s9mi",
             feet < 0 ? "<-" : ">");
  } else if(feet > 5280 || feet < -5280) {
    int32_t miles = (int32_t)((feet + (feet < 0 ? -2640 : 2640)) / 5280);
    snprintf(s_altitude_text, sizeof(s_altitude_text), "%ldmi", (long)miles);
  } else {
    snprintf(s_altitude_text, sizeof(s_altitude_text), "%ldft", (long)feet);
  }
  text_layer_set_text(s_altitude_layer, s_altitude_text);
}

static void prv_apply_preferences(DictionaryIterator *iterator) {
  bool changed = false;
  Tuple *units = dict_find(iterator, MESSAGE_KEY_Units);
  if(units && units->type == TUPLE_CSTRING) {
    bool imperial = strcmp(units->value->cstring, "imperial") == 0;
    if(s_settings.imperial_units != imperial) {
      s_settings.imperial_units = imperial;
      changed = true;
    }
  }

  Tuple *backlight = dict_find(iterator, MESSAGE_KEY_BacklightColor);
  if(backlight && backlight->type == TUPLE_CSTRING) {
    const char *value = backlight->value->cstring;
    uint32_t color = 0xFF0000;
    bool use_system = false;
    if(strcmp(value, "orange") == 0) color = 0xFF8000;
    else if(strcmp(value, "yellow") == 0) color = 0xFFFF00;
    else if(strcmp(value, "green") == 0) color = 0x00FF00;
    else if(strcmp(value, "blue") == 0) color = 0x0000FF;
    else if(strcmp(value, "white") == 0) color = 0xFFFFFF;
    else if(strcmp(value, "system") == 0) use_system = true;
    if(s_settings.backlight_color != color ||
       s_settings.use_system_backlight != use_system) {
      s_settings.backlight_color = color;
      s_settings.use_system_backlight = use_system;
      changed = true;
    }
  }

  if(changed) {
    prv_save_settings();
    prv_update_accuracy_display();
    prv_update_altitude_display();
    prv_apply_backlight();
  }
}

static void prv_update_heart_rate(void) {
#if defined(PBL_HEALTH)
  time_t now = time(NULL);
  HealthServiceAccessibilityMask access = health_service_metric_accessible(
    HealthMetricHeartRateBPM, now, now);
  if(access & HealthServiceAccessibilityMaskAvailable) {
    HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
    if(bpm > 0 && bpm <= 999) {
      snprintf(s_heart_rate_text, sizeof(s_heart_rate_text), "%ld", (long)bpm);
      text_layer_set_text(s_heart_rate_layer, s_heart_rate_text);
      return;
    }
  }
#endif
  text_layer_set_text(s_heart_rate_layer, "--");
}

#if defined(PBL_HEALTH)
static void prv_health_changed(HealthEventType event, void *context) {
  if(event == HealthEventHeartRateUpdate || event == HealthEventSignificantUpdate) {
    prv_update_heart_rate();
  }
}
#endif

static void prv_app_focus_changed(bool in_focus) {
  s_app_focused = in_focus;
  prv_apply_backlight();
}

static TextLayer *prv_add_text(Layer *parent, GRect frame,
                             const char *text, GFont font) {
  TextLayer *text_layer = text_layer_create(frame);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_font(text_layer, font);
  text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
  text_layer_set_text(text_layer, text);
  layer_add_child(parent, text_layer_get_layer(text_layer));
  return text_layer;
}

static void prv_update_time(struct tm *tick_time, TimeUnits units_changed) {
  strftime(s_time_text, sizeof(s_time_text),
           clock_is_24h_style() ? "%H:%M:%S" : "%I:%M:%S", tick_time);
  text_layer_set_text(s_time_layer, s_time_text);
  if(tick_time->tm_sec == 0) {
    prv_update_heart_rate();
  }
}

static void prv_format_coordinate(char *buffer, size_t size, int32_t value,
                                  char positive, char negative) {
  int32_t magnitude = value < 0 ? -value : value;
  int32_t rounded = (magnitude + 5) / 10;
  snprintf(buffer, size, "%ld.%05ld%c", (long)(rounded / 100000),
           (long)(rounded % 100000), value < 0 ? negative : positive);
}

static void prv_inbox_received(DictionaryIterator *iterator, void *context) {
  prv_apply_preferences(iterator);
  Tuple *latitude = dict_find(iterator, MESSAGE_KEY_Latitude);
  Tuple *longitude = dict_find(iterator, MESSAGE_KEY_Longitude);
  Tuple *status = dict_find(iterator, MESSAGE_KEY_LocationStatus);
  Tuple *accuracy = dict_find(iterator, MESSAGE_KEY_Accuracy);
  Tuple *altitude = dict_find(iterator, MESSAGE_KEY_Altitude);
  Tuple *altitude_valid = dict_find(iterator, MESSAGE_KEY_AltitudeValid);
  if(latitude && longitude && latitude->type == TUPLE_INT &&
     longitude->type == TUPLE_INT && latitude->length == sizeof(int32_t) &&
     longitude->length == sizeof(int32_t)) {
    int32_t latitude_value = latitude->value->int32;
    int32_t longitude_value = longitude->value->int32;
    if(latitude_value < -90000000 || latitude_value > 90000000 ||
       longitude_value < -180000000 || longitude_value > 180000000) {
      return;
    }
    prv_format_coordinate(s_latitude_text, sizeof(s_latitude_text),
                          latitude_value, 'N', 'S');
    prv_format_coordinate(s_longitude_text, sizeof(s_longitude_text),
                          longitude_value, 'E', 'W');
    text_layer_set_text(s_latitude_layer, s_latitude_text);
    text_layer_set_text(s_longitude_layer, s_longitude_text);
    s_accuracy_meters = accuracy && accuracy->type == TUPLE_INT &&
      accuracy->length == sizeof(int32_t) && accuracy->value->int32 >= 0 ?
      accuracy->value->int32 : -1;
    prv_update_accuracy_display();
    if(altitude_valid && altitude_valid->type == TUPLE_INT &&
       altitude_valid->length == sizeof(int32_t) && altitude_valid->value->int32 == 1 &&
       altitude && altitude->type == TUPLE_INT && altitude->length == sizeof(int32_t)) {
      s_altitude_meters = altitude->value->int32;
      s_altitude_valid = true;
      prv_update_altitude_display();
    } else {
      s_altitude_valid = false;
      prv_update_altitude_display();
    }
    text_layer_set_text(s_status_layer, "");
    s_has_location = true;
  } else if(status && status->type == TUPLE_INT) {
    text_layer_set_text(s_status_layer, s_has_location ? "GPS stale" :
                        status->value->int32 == 1 ? "GPS denied" :
                        "No GPS");
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  text_layer_set_text(s_status_layer, s_has_location ? "GPS stale" :
                      "No GPS");
}

static void prv_outbox_failed(DictionaryIterator *iterator,
                              AppMessageResult reason, void *context) {
  text_layer_set_text(s_status_layer, s_has_location ? "GPS stale" :
                      "No phone");
}

#if defined(PBL_COMPASS)
static void prv_heading_changed(CompassHeadingData heading) {
  if(heading.compass_status == CompassStatusUnavailable) {
    text_layer_set_text(s_heading_layer, "--");
  } else if(heading.compass_status == CompassStatusDataInvalid) {
    text_layer_set_text(s_heading_layer, "CAL");
  } else {
    int32_t clockwise = (TRIG_MAX_ANGLE - heading.magnetic_heading) % TRIG_MAX_ANGLE;
    int32_t degrees = (clockwise * 360 + TRIG_MAX_ANGLE / 2) / TRIG_MAX_ANGLE % 360;
    char direction = "NESW"[((degrees + 45) / 90) % 4];
    snprintf(s_heading_text, sizeof(s_heading_text), "%c %ldM%s", direction,
         (long)degrees, heading.compass_status == CompassStatusCalibrating ? "*" : "");
    text_layer_set_text(s_heading_layer, s_heading_text);
  }
}
#endif

static GRect prv_row_frame(GRect bounds, int16_t top, int16_t height) {
  int16_t inset = 6;
#if defined(PBL_ROUND)
  int32_t radius = bounds.size.w / 2;
  int32_t upper = top - bounds.size.h / 2;
  int32_t lower = top + height - bounds.size.h / 2;
  int32_t vertical_squared = upper * upper > lower * lower ?
                             upper * upper : lower * lower;
  while(inset < radius &&
        (radius - inset) * (radius - inset) + vertical_squared > radius * radius) {
    inset++;
  }
  inset += 4;
#endif
  return GRect(inset, top, bounds.size.w - 2 * inset, height);
}

static GRect prv_grid_frame(GRect bounds, int16_t top, int16_t height, bool right) {
  GRect row = prv_row_frame(bounds, top, height);
  int16_t center = bounds.size.w / 2;
  int16_t gap = 4;
  return right ? GRect(center + gap, top, row.origin.x + row.size.w - center - gap, height) :
                 GRect(row.origin.x, top, center - gap - row.origin.x, height);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  bool large = bounds.size.w >= 240 && bounds.size.h >= 240;
  bool medium = !large && bounds.size.w >= 192 && bounds.size.h >= 200;
  bool compact_round = PBL_IF_ROUND_ELSE(!large && !medium, false);
  int16_t label_height = 16;
  int16_t label_advance = 12;
  int16_t section_gap = large ? 12 : medium ? 8 : 2;
  const char *heading_label = large || medium ? "HEADING" : "HDG";
  const char *accuracy_label = large || medium ? "ACCURACY" : "ACC";
  const char *altitude_label = large || medium ? "ALTITUDE" : "ALT";
  const char *heart_rate_label = large || medium ? "HEART RATE" : "HR";
  int16_t time_height = large || medium ? 32 : compact_round ? 24 : 28;
  int16_t coordinate_height = large ? 32 : medium ? 28 : compact_round ? 20 : 22;
  int16_t detail_height = 34;
  int16_t stack_height = 2 * label_advance + time_height +
                         2 * coordinate_height + 2 * detail_height + 2 * section_gap;
  int16_t top = (bounds.size.h - stack_height) / 2;
  s_label_font = fonts_load_custom_font(resource_get_handle(
                  RESOURCE_ID_MONO_14));
  s_time_font = fonts_load_custom_font(resource_get_handle(
                 large || medium ? RESOURCE_ID_MONO_32 :
                 compact_round ? RESOURCE_ID_MONO_22 : RESOURCE_ID_MONO_28));
  s_coordinate_font = fonts_load_custom_font(resource_get_handle(
                       large ? RESOURCE_ID_MONO_32 :
                         medium ? RESOURCE_ID_MONO_28 :
                         compact_round ? RESOURCE_ID_MONO_20 : RESOURCE_ID_MONO_22));
  s_detail_font = fonts_load_custom_font(resource_get_handle(
                       RESOURCE_ID_MONO_BOLD_16));
  s_time_label = prv_add_text(window_layer, prv_row_frame(bounds, top, label_height),
                              "LOCAL TIME", s_label_font);
  top += label_advance;
  s_time_layer = prv_add_text(window_layer, prv_row_frame(bounds, top, time_height),
                              "", s_time_font);
  top += time_height + section_gap;
  s_location_label = prv_add_text(window_layer, prv_row_frame(bounds, top, label_height),
                                  "LOCATION", s_label_font);
  GRect status_frame = prv_row_frame(bounds, top, label_height);
  status_frame.origin.x += 64;
  status_frame.size.w -= 64;
  s_status_layer = prv_add_text(window_layer, status_frame, "Locating",
                                fonts_get_system_font(FONT_KEY_GOTHIC_09));
  top += label_advance;
  s_latitude_layer = prv_add_text(window_layer,
                                  prv_row_frame(bounds, top, coordinate_height),
                                  "-- N/S", s_coordinate_font);
  top += coordinate_height;
  s_longitude_layer = prv_add_text(window_layer,
                                   prv_row_frame(bounds, top, coordinate_height),
                                   "-- E/W", s_coordinate_font);
  top += coordinate_height + section_gap;
  s_metric_labels[0] = prv_add_text(window_layer,
                                    prv_grid_frame(bounds, top, 16, false),
                                    heading_label, s_label_font);
  s_metric_labels[1] = prv_add_text(window_layer,
                                    prv_grid_frame(bounds, top, 16, true),
                                    accuracy_label, s_label_font);
  s_heading_layer = prv_add_text(window_layer,
                                 prv_grid_frame(bounds, top + 16, 18, false),
                                 "--", s_detail_font);
  s_accuracy_layer = prv_add_text(window_layer,
                                  prv_grid_frame(bounds, top + 16, 18, true),
                                  s_settings.imperial_units ? "--ft" : "--m",
                                  s_detail_font);
  top += detail_height;
  s_metric_labels[2] = prv_add_text(window_layer,
                                    prv_grid_frame(bounds, top, 16, false),
                                    altitude_label, s_label_font);
  s_metric_labels[3] = prv_add_text(window_layer,
                                    prv_grid_frame(bounds, top, 16, true),
                                    heart_rate_label, s_label_font);
  s_altitude_layer = prv_add_text(window_layer,
                                  prv_grid_frame(bounds, top + 16, 18, false),
                                  s_settings.imperial_units ? "--ft" : "--m",
                                  s_detail_font);
  GRect heart_value_frame = prv_grid_frame(bounds, top + 16, 18, true);
  s_heart_icon_layer = prv_add_text(window_layer,
                                    GRect(heart_value_frame.origin.x,
                                          heart_value_frame.origin.y, 16,
                                          heart_value_frame.size.h),
                                    "\xE2\x99\xA5",
                                    fonts_get_system_font(FONT_KEY_FONT_FALLBACK));
  heart_value_frame.origin.x += 14;
  heart_value_frame.size.w -= 14;
  s_heart_rate_layer = prv_add_text(window_layer, heart_value_frame,
                                    "--", s_detail_font);
  time_t now = time(NULL);
  prv_update_time(localtime(&now), SECOND_UNIT);
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_time_label);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_location_label);
  text_layer_destroy(s_accuracy_layer);
  text_layer_destroy(s_heading_layer);
  text_layer_destroy(s_altitude_layer);
  text_layer_destroy(s_heart_rate_layer);
  text_layer_destroy(s_heart_icon_layer);
  for(size_t index = 0; index < ARRAY_LENGTH(s_metric_labels); index++) {
    text_layer_destroy(s_metric_labels[index]);
  }
  text_layer_destroy(s_latitude_layer);
  text_layer_destroy(s_longitude_layer);
  text_layer_destroy(s_status_layer);
  fonts_unload_custom_font(s_label_font);
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_coordinate_font);
  fonts_unload_custom_font(s_detail_font);
}

static void prv_init(void) {
  prv_load_settings();
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  app_focus_service_subscribe(prv_app_focus_changed);
  prv_app_focus_changed(true);
  prv_update_heart_rate();
#if defined(PBL_HEALTH)
  health_service_events_subscribe(prv_health_changed, NULL);
#endif
  tick_timer_service_subscribe(SECOND_UNIT, prv_update_time);
#if defined(PBL_COMPASS)
  compass_service_set_heading_filter(TRIG_MAX_ANGLE / 180);
  compass_service_subscribe(prv_heading_changed);
  CompassHeadingData heading;
  compass_service_peek(&heading);
  prv_heading_changed(heading);
#endif
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  if(app_message_open(256, 64) != APP_MSG_OK) {
    text_layer_set_text(s_status_layer, "No GPS");
  }
}

static void prv_deinit(void) {
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
#if defined(PBL_COMPASS)
  compass_service_unsubscribe();
#endif
  app_focus_service_unsubscribe();
  s_app_focused = false;
  prv_apply_backlight();
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
