/**
 *  @file
 * 
 *  Contains Pebble javascript to service location queries from
 *  the watch.  This code started out lifted from SDK v2 beta 4
 *  example
 *    Examples/pebblekit-js/weather/src/js/pebble-js-app.js
 *  
 *  At this time, the geolocation API is defined by
 *    http://dev.w3.org/geo/api/spec-source.html
 *  
 *  For Pebble-specific details see
 *    https://developer.getpebble.com/2/guides/javascript-guide.html
 *  and
 *    https://developer.getpebble.com/blog/2013/12/20/Pebble-Javascript-Tips-and-Tricks/
 */


///  Query timeout of 10 seconds, max allowed response staleness of 1 minute
var locationOptions = { "enableHighAccuracy ": false,
                        "timeout": 10000,
                        "maximumAge": 60000};

/** 
 *  setTimeout() value used as a guard on getCurrentPosition()
 *  per a suggestion in Pebble's tips & tricks.
 *  
 *    http://www.w3.org/TR/geolocation-API/#position_options_interface
 *  
 *  indicates that this outer timeout might pick up things like
 *  hang on obtaining user permission info, or other "non-core"
 *  operations performed by getCurrentPosition().
 */
var outerTimeout = 15000;     // 15 seconds, a bit longer than locationOptions.

///  We should only have at most one explicit timer running, this one:
var outerTimer = null;


function clearOuterTimer() {
   "use strict";

   if (outerTimer !== null)
   {
      window.clearTimeout(outerTimer);
      outerTimer = null;
   }
}


function locationSuccess(pos) {
   "use strict";

   clearOuterTimer();

   var coordinates = pos.coords;

   //  getTimezoneOffset() returns minutes, scale to seconds to match C's time_t
   //  Note in the PST (winter) timezone, on Android (CM) 4.2.2, this returns
   //  +8 hours.  So it really is an offset from local time to UTC, and not the
   //  usual -8 hour timezone offset from UTC to local.
   var utcOffset = new Date().getTimezoneOffset() * 60;

   console.log("location success, sending lat/long " + coordinates.latitude +
               " / " + coordinates.longitude + ", utcOff secs = " + utcOffset);

   //  Even though the coords are floats sendAppMessage transfers them as int32,
   //  dropping the fractional part.  So we make it official, using a scaled integer
   //  representation.  This is easier to handle in the Pebble anyway, since float
   //  string parsing support isn't present.
   Pebble.sendAppMessage({"latitudeData":  Math.round(coordinates.latitude * 1000000),
                         "longitudeData": Math.round(coordinates.longitude * 1000000),
                         "utcOffset": utcOffset});
}

///  Called if getCurrentPosition() times out per locationOptions.
function locationError(err) {
   "use strict";

   clearOuterTimer();

   // W3's Geolocation API Specification
   //    http://www.w3.org/TR/geolocation-API/#position_error_interface
   // says
   //    The message attribute must return an error message describing the
   //    details of the error encountered. This attribute is primarily intended
   //    for debugging and developers should not use it directly in their
   //    application user interface.
   // 
   // Oh well.

   console.warn('location error (' + err.code + '): ' + err.message);
   Pebble.sendAppMessage({"locationFailCode": err.code,
                         "locationFailMessage": err.message
                         });
}

///  Called when getCurrentPosition's guardian setTimeout() fires.
function outerTimeoutError() {
   "use strict";

   console.warn('outer timeout error (getCurrentPosition() guard fired)');
   Pebble.sendAppMessage({"locationFailCode": 0,  // Shouldn't collide with PositionError
                         "locationFailMessage": "No response from getCurrentPosition"
                         });
}


function setOuterTimer() {
   "use strict";

   clearOuterTimer();
   outerTimer = window.setTimeout(outerTimeoutError, outerTimeout);
}


Pebble.addEventListener("ready",
                        function(e){
                           "use strict";

                           console.log("connect!" + e.ready);
                           console.log("connect ready type " + e.type);
                           //  don't do anything, wait for a request from the watch.
                        });


Pebble.addEventListener("appmessage",
                        function(e){
                           "use strict";

                           var timeout;  // historical, not used now.
                           if (e.payload.latLongTimeout) {
                              timeout = e.payload.latLongTimeout;
                              console.log("latLongTimeout value received: "+timeout);
                           }
                           else {
                              timeout = -1;
                           }

                           if (e.payload.getLatLong){
                              // Note: without this dummy read of getLatLong, writes back
                              // to the watch never seem to complete. ??!!?
                              var scratch = e.payload.getLatLong;

                              console.log("lat/long request received, req timeout="+timeout);
                              //  launch async location query, which will send its own reply
                              window.navigator.
                              geolocation.getCurrentPosition(locationSuccess,
                                                             locationError,
                                                             locationOptions);

                              //  per Pebble's JS Tips & Tricks page, also set
                              //  a separate timeout in case getCurrentPosition
                              //  hangs and doesn't call locationError.
                              setOuterTimer();
                           }

//                           console.log(e.type);
//                           console.log(e.payload);
                        });

//  Used once we deal with a phone-side app configuration GUI.
//  See, e.g., https://github.com/pebble-hacks/js-configure-demo
Pebble.addEventListener("webviewclosed",
                        function(e){
                           "use strict";

                           console.log("webview closed");
                           console.log(e.type);
                           console.log(e.response);
                        });

