/**
 *  @file
 *  
 *  
 *  Twilight clock watchface app.
 *  
 *  Present heap utilization, as reported by "pebble logs" at face exit:
 *  
 *    [INFO    ] I app_manager.c:134 Heap Usage for <Twilight-Clock>:
 *               Available <12608B> Used <8668B> Still allocated <0B>
 */

#include "pebble.h"

#include "config.h"
#include "ConfigData.h"
#include "helpers.h"
#include "MessageWindow.h"
#include "messaging.h"
#include "my_math.h"
#include "suncalc.h"
#include "TransBitmap.h"
#include "TransRotBmp.h"
#include "TwilightPath.h"


/// Test whether using a built-in font is smaller than using a (subsetted) resource.
#define USE_FONT_RESOURCE 1   /* 0 == "use built-in font" */

///  Main watchface window.
Window *pWindow;

///  pWindow & all supporting layers, etc. initialized ok?
bool initialized_ok = false;

#if HOUR_VIBRATION
const VibePattern hour_pattern = {
   .durations = (uint32_t[]){ 200, 100, 200, 100, 200 },
   .num_segments = 5
};
#endif

TextLayer *pTextTimeLayer      = 0;
TextLayer *pIridiumLayer       = 0;
TextLayer *pTextSunriseLayer   = 0;
TextLayer *pTextSunsetLayer    = 0;
TextLayer *pDayOfWeekLayer     = 0;
TextLayer *pMonthLayer         = 0;
TextLayer *pMoonLayer          = 0;   // moon phase

///  Not a real layer, but the layer of the base window.
///  This is where our watch "dial" (twilight bands, etc.) is drawn.
Layer     *pGraphicsNightLayer = 0;

//Make fonts global so we can deinit later
GFont pFontCurTime  = 0;
GFont pFontMoon     = 0;

///  Roboto Condensed 19: "[a-zA-Z, :0-9]" chars only.  Used for face's date text.
GFont pFontMediumText = 0;

// system font (Raster Gothic 18), doesn't need unloading:
GFont pFontSmallText = 0;

GFont pFontIridium = 0;

///  Hour hand bitmap, a transparent png which can rotate to any angle.
TransRotBmp* pTransRotBmpHourHand = 0;

/**
 *  Watchface dial: a transparent png which supplies hour marks, a face
 *  outline, and masks everything outside the face to black.  Aside from
 *  the hour marks, the interior of the face is transparent to allow
 *  twilight bands to show through.
 */
TransBitmap* pTransBmpWatchface = 0;

///  Boundary between night and astronomical twilight.
TwilightPath* pTwiPathNight = 0;

///  Boundary between astronomical and nautical twilight.
TwilightPath* pTwiPathAstro = 0;

///  Boundary between nautical and civil twilight.
TwilightPath* pTwiPathNautical = 0;

///  Daylight edge of civil twilight (i.e., sun rise / set times).
TwilightPath* pTwiPathCivil = 0;



/**
 *  Handler called when the "night layer" needs redrawing.
 *  
 *  Note that the heavy calculation has been done ahead of time by
 *  \href updateDayAndNightInfo(), so this callback should be a bit
 *  zippier than it otherwise might be.  (Not to say "fast"..)
 * 
 * @param me Night layer being updated.
 * @param ctx System-supplied context, presumably already set to defaults
 *             for *me. 
 */
void graphics_night_layer_update_callback(Layer *me, GContext *ctx)
{


   //  Don't do our display hold-off until our message pump is running.
   //  Calling back a window update handler, of which this is the first,
   //  is a good way to know.  So now we can check for available data
   //  and hold off proper clock display until it is found:
   if (! config_data_location_avail())
   {
      //  Probably initial program run: no config data persisted yet.
      //  Put up a special window informing the user of this.
      message_window_show_status ("Getting Location",
                                  "Obtaining initial location data.");

      app_msg_RequestLatLong();

      return;
   }

   GRect layerFrame = layer_get_frame(me);

   //BUGBUG: are these
   //  GRect(0, 0, 144, 168)
   //not equal to layerFrame?

   // ------------------------------------------------

   //  start out with white screen, draw full-night black to bottom part
   twilight_path_render(pTwiPathNight, ctx, GColorBlack, layerFrame);

   //  turn all of white remainder (upper part of screen) into dark grey & then
   //  turn upper part of screen above astro twilight band back into white
   twilight_path_render(pTwiPathAstro, ctx, GColorWhite, layerFrame);

   //  turn all of white remainder (upper part of screen) into medium grey &
   //  turn upper part of screen above nautical twilight band back into white
   twilight_path_render(pTwiPathNautical, ctx, GColorWhite, layerFrame);

   //  turn all of white remainder (upper part of screen) into light grey &
   //  turn upper part of screen above civil twilight band back into white
   twilight_path_render(pTwiPathCivil, ctx, GColorWhite, layerFrame);

   // ------------------------------------------------

   //  place tidy watchface frame over accumulated render of twilight bands:
   transbitmap_draw_in_rect(pTransBmpWatchface, ctx, layerFrame);

   //  not clear why this is done: perhaps the system needs it?
   graphics_context_set_compositing_mode(ctx, GCompOpAssign);

   return;

}  /* end of graphics_night_layer_update_callback() */

float get24HourAngle(int hours, int minutes)
{
   return (12.0f + hours + (minutes / 60.0f)) / 24.0f;
}

/** 
 *  Given a presumably UTC time, return the astronomical julian day.
 *  This is not day-of-year, but a much larger value.
 * 
 *  @param timeUTC UTC time, unpacked.
 */ 
int tm2jd(struct tm *timeUTC)
{
   int y, m, d, a, b, c, e, f;
   y = timeUTC->tm_year + 1900;
   m = timeUTC->tm_mon + 1;
   d = timeUTC->tm_mday;
   if (m < 3)
   {
      m += 12;
      y -= 1;
   }
   a = y / 100;
   b = a / 4;
   c = 2 - a + b;
   e = 365.25 * (y + 4716);
   f = 30.6001 * (m + 1);
   return c + d + e + f - 1524;
}


int moon_phase(int jdn)
{
   double jd;
   jd = jdn - 2451550.1;
   jd /= 29.530588853;
   jd -= (int)jd;
   return (int)(jd * 27 + 0.5); // scale fraction from 0-27 and round by adding 0.5
}


/**
 *  Update lunar phase.  Intended to be called once per day.
 */
void DisplayCurrentLunarPhase()
{

   static char moon[] = "m";
   int moonphase_number = 0;

   // moon
   time_t timeNow;
   timeNow = time(NULL);
   moonphase_number = moon_phase(tm2jd(gmtime(&timeNow)));
   // correct for southern hemisphere
   if ((moonphase_number > 0) && (config_data_get_latitude() < 0))
      moonphase_number = 28 - moonphase_number;

   // select correct font char
   if (moonphase_number == 14)
   {
      moon[0] = (unsigned char)(48);
   } else if (moonphase_number == 0)
   {
      moon[0] = (unsigned char)(49);
   } else if (moonphase_number < 14)
   {
      moon[0] = (unsigned char)(moonphase_number + 96);
   } else
   {
      moon[0] = (unsigned char)(moonphase_number + 95);
   }
//    moon[0] = (unsigned char)(moonphase_number);

   text_layer_set_text(pMoonLayer, moon);

}  /* end of DisplayCurrentLunarPhase */


/**
 *  Calculate sunrise, sunset, and all corresponding twilight
 *  times for current day.
 *  
 *  This only needs to be called once a day (aside from startup time).
 * 
 * @param update_everything True to update everything. False to
 *                          only update when the day has
 *                          changed.
 */
void updateDayAndNightInfo(bool update_everything)
{
   static char sunrise_text[] = "00:00";
   static char sunset_text[] = "00:00";

   ///  Localtime mday of most recent completed day/night update.
   ///  This means we normally update just after midnight, which
   ///  seems a good time.
   static short lastUpdateDay = -1;


   time_t timeNow;
   timeNow = time(NULL);
   struct tm tmNowLocal = *(localtime(&timeNow));

   if ((lastUpdateDay == tmNowLocal.tm_mday) && !update_everything)
   {
      return;
   }

   twilight_path_compute_current(pTwiPathNight,    &tmNowLocal);
   twilight_path_compute_current(pTwiPathAstro,    &tmNowLocal);
   twilight_path_compute_current(pTwiPathNautical, &tmNowLocal);
   twilight_path_compute_current(pTwiPathCivil,    &tmNowLocal);

   //  Want the user's default time format, but not for the current time.
   //  We can't use clock_copy_time_string(), so make an equivalent format:
   char *time_format;

   if (clock_is_24h_style())
   {
      time_format = "%R";
   } else
   {
      time_format = "%l:%M";
   }

   float sunriseTime = pTwiPathCivil->fDawnTime;
   float sunsetTime  = pTwiPathCivil->fDuskTime;

   //BUGBUG - need to round this
   tmNowLocal.tm_min = (int)(60 * (sunriseTime - ((int)(sunriseTime))));
   tmNowLocal.tm_hour = (int)sunriseTime;
   strftime(sunrise_text, sizeof(sunrise_text), time_format, &tmNowLocal);
   text_layer_set_text(pTextSunriseLayer, sunrise_text);

   tmNowLocal.tm_min = (int)(60 * (sunsetTime - ((int)(sunsetTime))));
   tmNowLocal.tm_hour = (int)sunsetTime;
   strftime(sunset_text, sizeof(sunset_text), time_format, &tmNowLocal);
   text_layer_set_text(pTextSunsetLayer, sunset_text);
   text_layer_set_text_alignment(pTextSunsetLayer, GTextAlignmentRight);

   DisplayCurrentLunarPhase();

   lastUpdateDay = tmNowLocal.tm_mday;

   //  other layers should take care of themselves, but make sure our base
   //  "dial" bitmap is updated.
   layer_mark_dirty(pGraphicsNightLayer);

}  /* end of updateDayAndNightInfo() */

/**
 *  Once a minute, update textual time displays, and analog hour hand.
 *  
 *  Also calls out to daily updater, which limits itself to acting
 *  only when due.
 * 
 *  @param tick_time The time at which the tick event was triggered.
 *                At least in PebbleOS versions 2 and earlier, this
 *                is local time.
 *  @param units_changed Which unit change triggered this tick event.
 *                Usually always minutes as that's all we register for.
 */

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{


   (void) units_changed;

   // Need to be static because they're used by the system later.
   static char time_text[] = "00:00";
   static char dow_text[] = "xxx";
   static char mon_text[14];
   static char iridium_text[] = "12:34 (-4.0)";

   strftime(dow_text, sizeof(dow_text), "%a", tick_time);
   strftime(mon_text, sizeof(mon_text), "%b %e, %Y", tick_time);

   clock_copy_time_string(time_text, sizeof(time_text));
   if (!clock_is_24h_style() && (time_text[0] == '0'))
   {
      memmove(time_text, &time_text[1], sizeof(time_text) - 1);
   }

   text_layer_set_text(pDayOfWeekLayer, dow_text);
   text_layer_set_text(pMonthLayer, mon_text);

   text_layer_set_text(pTextTimeLayer, time_text);
   text_layer_set_text_alignment(pTextTimeLayer, GTextAlignmentCenter);

   text_layer_set_text(pIridiumLayer, iridium_text);
   text_layer_set_text_alignment(pIridiumLayer, GTextAlignmentCenter);

   //  update hour hand position
   transrotbmp_set_angle(pTransRotBmpHourHand,
                         TRIG_MAX_ANGLE * get24HourAngle(tick_time->tm_hour,
                                                         tick_time->tm_min));

   transrotbmp_set_pos_centered(pTransRotBmpHourHand, 0, 9 + 2);

// Vibrate Every Hour
#if HOUR_VIBRATION

   if ((t->tick_time->tm_min == 0) && (t->tick_time->tm_sec == 0))
   {
      vibes_enqueue_custom_pattern(hour_pattern);
   }
#endif

   updateDayAndNightInfo(false);

}  /* end of handle_minute_tick() */


/**
 *  Do GUI layout for already-created window, and cache all needed resources.
 *  Also register a tick handler, initialize watch/phone messaging, and request
 *  current location data from the phone.
 */
static void  sunclock_window_load(Window * pMyWindow) 
{

   window_set_background_color(pWindow, GColorWhite);

   pFontMoon = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MOON_PHASES_SUBSET_30));

#if USE_FONT_RESOURCE
   pFontCurTime = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_32));
#else
   pFontCurTime = fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD);
#endif

   pFontMediumText = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_19));

//   pFontIridium = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_18));

   //  The v2 SDK docs suggest that we should do our base bitmap
   //  graphics directly in the window root layer, rather than creating a
   //  separate layer just for the bitmaps (& not using the base window's layer).
//   pGraphicsNightLayer = layer_create(layer_get_bounds(window_get_root_layer(pWindow)));
   pGraphicsNightLayer = window_get_root_layer(pWindow);
   if (pGraphicsNightLayer == NULL)
   {
      return;
   }
   layer_set_update_proc(pGraphicsNightLayer, graphics_night_layer_update_callback); 
//   layer_add_child(window_get_root_layer(pWindow), pGraphicsNightLayer);


   pTransBmpWatchface = transbitmap_create_with_resource_prefix(RESOURCE_ID_IMAGE_WATCHFACE);
   if (pTransBmpWatchface == NULL)
   {
      return;
   }

   //  Yes, the apparent mismatch between ZENITH_ names and TwilightPath instance
   //  names is intended (if a bit unfortunate).
   pTwiPathNight    = twilight_path_create(ZENITH_ASTRONOMICAL, ENCLOSE_SCREEN_BOTTOM,
                                           INVALID_RESOURCE);
   pTwiPathAstro    = twilight_path_create(ZENITH_NAUTICAL,     ENCLOSE_SCREEN_TOP,
                                           RESOURCE_ID_IMAGE_DARK_GREY);
   pTwiPathNautical = twilight_path_create(ZENITH_CIVIL,        ENCLOSE_SCREEN_TOP,
                                           RESOURCE_ID_IMAGE_GREY);
   pTwiPathCivil    = twilight_path_create(ZENITH_OFFICIAL,     ENCLOSE_SCREEN_TOP,
                                           RESOURCE_ID_IMAGE_LIGHT_GREY);
   if ((pTwiPathNight == NULL) || (pTwiPathAstro == NULL) ||
       (pTwiPathNautical == NULL) || (pTwiPathCivil == NULL))
   {
      return;
   }

   // time of day text
   pTextTimeLayer = text_layer_create(GRect(0, 32, 144, 32));
   if (pTextTimeLayer == NULL)
   {
      return;
   }
   text_layer_set_text_color(pTextTimeLayer, GColorBlack); 
   text_layer_set_background_color(pTextTimeLayer, GColorClear);
   text_layer_set_font(pTextTimeLayer, pFontCurTime);
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pTextTimeLayer));


   pIridiumLayer = text_layer_create(GRect(0, 64, 144, 20));
   if (pIridiumLayer == NULL)
   {
      return;
   }

   text_layer_set_text_color(pIridiumLayer, GColorBlack);
   text_layer_set_background_color(pIridiumLayer, GColorClear);
   text_layer_set_font(pIridiumLayer, pFontMediumText);
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pIridiumLayer));

   pMoonLayer = text_layer_create(GRect(0, 109, 144, 168 - 115));
   if (pMoonLayer == NULL)
   {
      return;
   }
   text_layer_set_text_color(pMoonLayer, GColorWhite);
   text_layer_set_background_color(pMoonLayer, GColorClear);
   text_layer_set_font(pMoonLayer, pFontMoon);
   text_layer_set_text_alignment(pMoonLayer, GTextAlignmentCenter);
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pMoonLayer));

   //  Add hour hand after moon phase:  looks weird (wrong) to see phase
   //  on top of the hour hand.
   pTransRotBmpHourHand = transrotbmp_create_with_resource_prefix(RESOURCE_ID_IMAGE_HOUR);
   if (pTransRotBmpHourHand == NULL)
   {
      return;
   }
   transrotbmp_set_src_ic(pTransRotBmpHourHand, GPoint(9, 56));
   transrotbmp_add_to_layer(pTransRotBmpHourHand, window_get_root_layer(pWindow));

   //  Same rectangle used for day of week and date text:
   //  text alignment avoids conflicts in the two layers.
   GRect DayDateTextRect;
   DayDateTextRect = GRect(0, 0, 144, 127 + 26);

   //Day of Week text
   pDayOfWeekLayer = text_layer_create(DayDateTextRect);
   if (pDayOfWeekLayer == NULL)
   {
      return;
   }
   text_layer_set_text_color(pDayOfWeekLayer, GColorWhite); 
   text_layer_set_background_color(pDayOfWeekLayer, GColorClear);
   text_layer_set_font(pDayOfWeekLayer, pFontMediumText);
   text_layer_set_text_alignment(pDayOfWeekLayer, GTextAlignmentLeft);
   text_layer_set_text(pDayOfWeekLayer, "xxx");
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pDayOfWeekLayer));

   //Month Text
   pMonthLayer = text_layer_create(DayDateTextRect);
   if (pMonthLayer == NULL)
   {
      return;
   }
   text_layer_set_text_color(pMonthLayer, GColorWhite);
   text_layer_set_background_color(pMonthLayer, GColorClear);
   text_layer_set_font(pMonthLayer, pFontMediumText);
   text_layer_set_text_alignment(pMonthLayer, GTextAlignmentRight);
   text_layer_set_text(pMonthLayer, "xxx");
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pMonthLayer));

   pFontSmallText = fonts_get_system_font(FONT_KEY_GOTHIC_18);

   //  Same rectangle used for sunrise / sunset text layers: updateDayAndNightInfo()
   //  changes sunset text to right-aligned.
   GRect SunRiseSetTextRect;
   SunRiseSetTextRect = GRect(0, 147, 144, 30);

   pTextSunriseLayer = text_layer_create(SunRiseSetTextRect);
   if (pTextSunriseLayer == NULL)
   {
      return;
   }
   text_layer_set_text_color(pTextSunriseLayer, GColorWhite);
   text_layer_set_background_color(pTextSunriseLayer, GColorClear);
   text_layer_set_font(pTextSunriseLayer, pFontSmallText);
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pTextSunriseLayer));

   pTextSunsetLayer = text_layer_create(SunRiseSetTextRect);
   if (pTextSunsetLayer == NULL)
   {
      return;
   }
   text_layer_set_text_color(pTextSunsetLayer, GColorWhite); 
   text_layer_set_background_color(pTextSunsetLayer, GColorClear);
   text_layer_set_font(pTextSunsetLayer, pFontSmallText);
   layer_add_child(window_get_root_layer(pWindow),
                   text_layer_get_layer(pTextSunsetLayer));

   //  Run initial tick processing before our window displays, so that all
   //  text fields are populated initially.
   time_t timeNow = time(NULL);
   struct tm * pLocalTime = localtime(&timeNow);

   handle_minute_tick(pLocalTime, MINUTE_UNIT);

   //  [Don't do location data load until our message pump is running.]
//   app_msg_RequestLatLong();

   tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

   initialized_ok = true;

}  /* end of sunclock_window_load */


/** 
 *  Release all resources and registrations allocated by
 *  \ref sunclock_window_load().
 */
static void  sunclock_window_unload(Window * pMyWindow)
{

   tick_timer_service_unsubscribe();

   SAFE_DESTROY(text_layer, pTextSunsetLayer);
   SAFE_DESTROY(text_layer, pTextSunriseLayer);
   SAFE_DESTROY(text_layer, pMonthLayer);
   SAFE_DESTROY(text_layer, pDayOfWeekLayer);
   SAFE_DESTROY(text_layer, pMoonLayer);
   SAFE_DESTROY(text_layer, pIridiumLayer);
   SAFE_DESTROY(text_layer, pTextTimeLayer);
   SAFE_DESTROY(layer,      pGraphicsNightLayer);

   transbitmap_destroy(pTransBmpWatchface);
   pTransBmpWatchface = 0;

   transrotbmp_destroy(pTransRotBmpHourHand);
   pTransRotBmpHourHand = 0;

   SAFE_DESTROY(twilight_path, pTwiPathNight);
   SAFE_DESTROY(twilight_path, pTwiPathAstro);
   SAFE_DESTROY(twilight_path, pTwiPathNautical);
   SAFE_DESTROY(twilight_path, pTwiPathCivil);

}  /* end of sunclock_window_unload() */


void sunclock_coords_recvd(float latitude, float longitude, int32_t utcOffset)
{

   APP_LOG(APP_LOG_LEVEL_DEBUG, "got coords, utcOff=%d", (int) utcOffset);

   if (config_data_is_different(latitude, longitude, utcOffset))
   {
      config_data_location_set(latitude, longitude, utcOffset); 

      updateDayAndNightInfo(true /* update_everything */);
   }

}  /* end of sunclock_coords_recvd */


/**
 *  Create base watchface window.  We're called outside of the event loop,
 *  so we do as little as possible here.
 */
void  sunclock_handle_init()
{


   pWindow = window_create();
   if (pWindow == NULL)
   {
      // oh well.
      return;
   }

   //  Defer the bulk of our start up a load handler.  In particular, it seems
   //  better to do app_message stuff there, once our event loop is running.
   window_set_window_handlers(pWindow,
                              (WindowHandlers) {
                                 .load = sunclock_window_load,
                                 .unload = sunclock_window_unload,
                               });

   window_stack_push(pWindow, true /* Animated */);

}  /* end of sunclock_handle_init() */

/**
 *  Called at application shutdown / exit.  Releases all dynamic storage
 *  allocated by \href handle_init() et al.
 *  
 *  NB: all except for our main window, since deleting it seems to provoke
 *      a Pebble "crash" report when our face exits.
 */
void sunclock_handle_deinit()
{

//   It appears that, despite what Pebble's guide says
//     http://developer.getpebble.com/guides/pebble-apps/app-structure/windows
//   it is _still_ (in SDK v2.8) not safe to destroy our own main window at exit.
//   In fact, with SDK v2.8 (maybe starting with v2.1?) attempting to destroy
//   our window when leaving the watchface via (at least) an up/down "scroll"
//   button press results in an "app crashed" message.  Humbug.
// 
//   SAFE_DESTROY(window, pWindow);    // uncomment to cause app crash @ exit

   //  Do these here since they're shared with another app window.
   //  The SDK hints that the window unload function might be called
   //  before window destruction, in future SDK releases.
   fonts_unload_custom_font(pFontMediumText);
   fonts_unload_custom_font(pFontMoon);
#if USE_FONT_RESOURCE
   fonts_unload_custom_font(pFontCurTime);
#endif

}  /* end of sunclock_handle_deinit */

