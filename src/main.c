#include <pebble.h>

#include "weather_layer.h"
#include "network.h"
#include "config.h"

#define TIME_FRAME      (GRect(0, 2, 144, 168-6))
#define DATE_FRAME      (GRect(1, 66, 144, 168-62))

/* Keep a pointer to the current weather data as a global variable */
static WeatherData *weather_data;

/* Global variables to keep track of the UI elements */
static Window *window;
static TextLayer *date_layer;
static TextLayer *time_layer;
static WeatherLayer *weather_layer;

static char date_text[] = "XXX00 00";
static char time_text[] = "00:00";

/* Preload the fonts */
GFont font_date;
GFont font_time;

static const uint32_t const segments[] = {100, 300, 100, 300,100, 300,  300, 300, 300, 300,300, 300,   100, 300, 100, 300,100, 300};
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

static void handle_tick(struct tm *tick_time, TimeUnits units_changed){
  //if (units_changed & MINUTE_UNIT) {
    time_t currentTime = time(0);
    struct tm *currentLocalTime = localtime(&currentTime);
    strftime(time_text, sizeof(time_text),"%R", currentLocalTime);
    text_layer_set_text(time_layer, time_text);
  // }
  if (units_changed & DAY_UNIT) {
    char day_text[4], month_num[3];
    time_t currentTime = time(0);
    struct tm *currentLocalTime = localtime(&currentTime);
    switch (tick_time->tm_wday){
       case 0:
          strcpy (day_text, "Dmg");
          break;
       case 1:
          strcpy(day_text, "Dll");
          break;
       case 2:
          strcpy (day_text, "Dma");
          break;
       case 3:    
          strcpy (day_text, "Dme");
          break;
       case 4:
          strcpy (day_text, "Djo");
          break;
       case 5:
          strcpy (day_text, "Dve");
          break;
       case 6:
          strcpy (day_text, "Dsa");
          break;
    }
    strftime(month_num, sizeof(month_num),"%m", currentLocalTime);
    
    snprintf(date_text, sizeof(date_text), "%s%i %s", day_text, tick_time->tm_mday,month_num);
    text_layer_set_text(date_layer, date_text);
  }

  // Update the bottom half of the screen: icon and temperature
  static int animation_step = 0;
  if(bluetooth_connection_service_peek()){
    if (weather_data->updated == 0 && weather_data->error == WEATHER_E_OK){
      tick_timer_service_subscribe( SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT, handle_tick);
      // 'Animate' loading icon until the first successful weather request
      if (animation_step == 0) {
        weather_layer_set_icon(weather_layer, WEATHER_ICON_LOADING1);
      }
      else if (animation_step == 1) {
        weather_layer_set_icon(weather_layer, WEATHER_ICON_LOADING2);
      }
      else if (animation_step >= 2) {
        weather_layer_set_icon(weather_layer, WEATHER_ICON_LOADING3);
      }
      animation_step = (animation_step + 1) % 3;
    }
    else {
      // Update the weather icon and temperature
      tick_timer_service_subscribe( MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT, handle_tick);
      if (weather_data->error) {
        weather_layer_set_icon(weather_layer, WEATHER_ICON_NOT_AVAILABLE);
        weather_layer_set_temperature(weather_layer, 0, true);
      } else {
        weather_layer_set_temperature(weather_layer, weather_data->temperature, false);  
        // Day/night check
        bool night_time = false;
        if (weather_data->current_time < weather_data->sunrise || weather_data->current_time > weather_data->sunset)
          night_time = true;
        weather_layer_set_icon(weather_layer, weather_icon_for_condition(weather_data->condition, night_time));
      }
    }
  }else{
    tick_timer_service_subscribe( MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT, handle_tick);
    weather_layer_set_temperature(weather_layer, 0, true);
    weather_layer_set_icon(weather_layer, WEATHER_ICON_PHONE_ERROR);
  }
    

  // Refresh the weather info every 30 minutes
  if (units_changed & MINUTE_UNIT && (tick_time->tm_min % 30) == 0){
    request_weather();
  }
}
static void BlueConnectionHandler(bool connected){
  if(connected){
    time_t now = time(NULL);
    request_weather();
    if(weather_data->error)
      handle_tick(localtime(&now), SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT );
  }else
    vibes_enqueue_custom_pattern(pat);
}
static void init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  weather_data = malloc(sizeof(WeatherData));
  init_network(weather_data);
  
  bluetooth_connection_service_subscribe(BlueConnectionHandler);
  
  font_date = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_18));
  //font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_CONDENSED_53));
  font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_CONDENSED_53));

  time_layer = text_layer_create(TIME_FRAME);
  text_layer_set_text_color(time_layer, GColorBlack/*GColorWhite*/);
  text_layer_set_background_color(time_layer, GColorWhite/*GColorClear*/);
  text_layer_set_font(time_layer, font_time);
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(time_layer));

  date_layer = text_layer_create(DATE_FRAME);
  text_layer_set_text_color(date_layer, GColorBlack/*GColorWhite*/);
  text_layer_set_background_color(date_layer, GColorWhite/*GColorClear*/);
  text_layer_set_font(date_layer, font_date);
  text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(date_layer));

  // Add weather layer
  weather_layer = weather_layer_create(GRect(0, 90, 144, 80));
  
  layer_add_child(window_get_root_layer(window), weather_layer);
  tick_timer_service_subscribe( MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT, handle_tick);
  weather_layer_set_temperature(weather_layer, 0, true);

  // Update the screen right away
  time_t now = time(NULL);
  handle_tick(localtime(&now), SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT);
}

static void deinit(void) {
  window_destroy(window);
  tick_timer_service_unsubscribe();

  text_layer_destroy(time_layer);
  text_layer_destroy(date_layer);
  weather_layer_destroy(weather_layer);

  fonts_unload_custom_font(font_date);
  fonts_unload_custom_font(font_time);

  free(weather_data);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
