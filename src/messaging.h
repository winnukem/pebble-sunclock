/**
 *  @file
 *  
 */

#pragma once

#include  "pebble.h"    // for bool type


///  Values must match those in our appinfo.json "appKeys" section.
enum {
   MSG_KEY_GET_LAT_LONG = 0x0,      // arg ignored, key is the message.
   MSG_KEY_LATITUDE = 0x1,          // scaled integer: degrees * 1000000
   MSG_KEY_LONGITUDE = 0x2,         // scaled integer: degrees * 1000000
   MSG_KEY_UTC_OFFSET = 0x3,        // integer offset from local time to UTC.
   MSG_KEY_FAIL_CODE = 0x4,         // integer? error from js w3c location API
   MSG_KEY_FAIL_MESSAGE = 0x5,      // cstring? error message from js w3c location API
};


/**
 *  Callback used to notify application when a location update has been received
 *  from the phone.  This is typically in response to an app_msg_RequestLatLong()
 *  call, but may also be initiated by the phone end.
 *  
 *  @param latitude Phone's most recently-known latitude value: degrees from the
 *             equator, positive for North, negative for South.
 *  @param longitude Phone's most recently-known longitude value: degrees from
 *             Greenwich, positive for East, negative for West.
 *  @param utcOffset Offset from Pebble / phone's local time to UTC, in seconds.
 *             Note in the PST (winter) timezone, on Android (CM) 4.2.2, this
 *             returns +8 hours.  So it really is an offset from local time to
 *             UTC, and not the usual -8 hour timezone offset from UTC to local.
 */
typedef void (*app_msg_coords_recvd_callback) (float latitude, float longitude,
                                               int32_t utcOffset);

typedef enum  {
   FAIL_SRC_APP_MSG,
   FAIL_SRC_PHONE,
} FailureSource;

/**
 *  Callback used to report a failure, either to send a request to the phone
 *  or to obtain location data on the phone
 */
typedef void (*app_msg_coords_failed_callback) (FailureSource eErrSrc,
                                                int32_t errCode, const char *pszErrMsg);


/**
 *  Initialize the Pebble / phone communications subsystem, and supply a callback
 *  to notify the application when the subsystem has received a location value
 *  from the phone.
 * 
 *  @param successCallback Called by the app_msg_* plumbing when a location message
 *                is received from the phone.  This may be in response to a
 *                app_msg_RequestLatLong(), or initiated by the phone end.
 *  @param failureCallback Called by the app_msg_* plumbing to report either a
 *                failure to communicate with the phone, or a failure detected
 *                by the phone itself (e.g., no location permission or data).
 */
void  app_msg_init(app_msg_coords_recvd_callback successCallback,
                   app_msg_coords_failed_callback failureCallback);

/**
 *  Send a request to the phone to send us current location data.
 *  This call merely initiates the request, the message might not have left
 *  the Pebble by the time this call returns.
 *  
 *  If the message successfully makes it to the phone, and a reply back to us,
 *  then the app_msg_coords_recvd_callback callback defined by app_msg_init()
 *  will be called.
 *  
 *  NOTE: There is no guarantee that an app_msg_coords_recvd_callback will be
 *        made in response to this call.  There are many failure modes, not
 *        least if the phone has its bluetooth radio turned off.
 * 
 *  @return bool \c true if initial message processing is successful, else \c false.
 */
bool  app_msg_RequestLatLong(void);

void  app_msg_deinit(void);
