/**
 *  @file
 * 
 *  Contains Pebble javascript to service location queries from
 *  the watch, and provide a limited set of "configuration"
 *  pages.
 *  
 *  At this time, the geolocation API is defined by
 *    http://dev.w3.org/geo/api/spec-source.html
 *  
 *  For Pebble-specific details see
 *    https://developer.getpebble.com/2/guides/javascript-guide.html
 *  and
 *    https://developer.getpebble.com/blog/2013/12/20/Pebble-Javascript-Tips-and-Tricks/
 */

/*jslint vars: true, white: true, sub: true */
/*global window, console, Pebble, XMLHttpRequest */

//  NB: in order to take advantage of the direct http access afforded by
//      github.io, we keep copies of the config pages in branch gh-pages.
//      Main versions live in master branch, so everything can be seen
//      in a single branch view.  Yeah, maybe my lack of git-fu is showing.
var urlCfgDir = "http://ewedel.github.io/pebble-sunclock/PebbleConfig/";

var urlCfgMain = urlCfgDir + "config.html";
var urlCfgShowCoords = urlCfgDir + "show_coords.html";
var urlCfgCoordsSent = urlCfgDir + "coords_sent.html";

///  Query timeout of 10 seconds, max allowed response staleness of 1 minute
var locationOptions = { "enableHighAccuracy ": false,
                        "timeout": 10000,
                        "maximumAge": 60000};
 
///  Last "real" (non-CANCELLED) response received for webviewclosed
var realResponse = "";


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

///  Do we send retrieved coord values to Pebble, or display in a config screen?
var coordsToPebble = true;

///  Do we launch a config URL to confirm after initiating async send to Pebble?
var showSendInitiatedPage = false;

///  Timeout on XMLHttpRequest for reverse geocoding
var revGeoTimeout = null;

///  Currently outstanding reverse geocoding XMLHttpRequest
var revGeoReq = null;

///  User ID which we supply to geonames.org - if you fork this repo,
///  please use your own geonames account instead of this free, limited-use one.
var geonamesId = "TwilightSunclock";


function clearOuterTimer() {
   "use strict";

   if (outerTimer !== null)
   {
      window.clearTimeout(outerTimer);
      outerTimer = null;
   }
}


/**
 *  Produce a standardized collection in the form expected by
 *  displayCoords() and eventually show_coords.html.
 *  
 *  @return The raw collection.  Feed to displayCoords() to
 *          URL-encode the collection and launch show_coords.html.
 */
function wrapCoords(fLat, fLong, utcOffset, errCode, errMsg) {
   "use strict";

   var coords = {
      'lat': fLat,
      'long': fLong,
      'tz-off': utcOffset,
      'err-code': errCode,
      'err-msg': errMsg

      //   // and optionally, post-decode,
      //      'place-name': city
      //      'region': state
      //      'country': country

   };

   return coords;

}

/**
 *  Display coords by bringing up a Pebble configuration screen.
 *  
 *  @param coords A standardized collection as produced by
 *                wrapCoords().
 */
function displayCoords(coords) {
   "use strict";

   var location = urlCfgShowCoords + "#" + encodeURIComponent(JSON.stringify(coords));
   console.log("Warping to: " + location);

   Pebble.openURL(location);
}


function locationSuccess(pos) {
   "use strict";

   clearOuterTimer();

   getIridiumFlares(pos, locationSuccessReal);
}

function locationSuccessReal(pos, flares) {
   var coordinates = pos.coords;
   //  getTimezoneOffset() returns minutes, scale to seconds to match C's time_t
   //  Note in the PST (winter) timezone, on Android (CM) 4.2.2, this returns
   //  +8 hours.  So it really is an offset from local time to UTC, and not the
   //  usual -8 hour timezone offset from UTC to local.
   var utcOffset = new Date().getTimezoneOffset() * 60;
   var flare_string = "00:00 (+3.1)";
   if (flares[0] != undefined) {
      flare_string = flares[0].date.getUTCHours() + ":" + flares[0].date.getMinutes() + " (" + flares[0].brightness + ")";
   }

   console.log("location success, sending lat/long " + coordinates.latitude +
               " / " + coordinates.longitude + ", utcOff secs = " + utcOffset +
               ", flare = " + flare_string);

   if (coordsToPebble) {
      //  Even though the coords are floats sendAppMessage transfers them as int32,
      //  dropping the fractional part.  So we make it official, using a scaled integer
      //  representation.  This is easier to handle in the Pebble anyway, since float
      //  string parsing support isn't present.
      Pebble.sendAppMessage({"latitudeData":  Math.round(coordinates.latitude * 1000000),
                            "longitudeData": Math.round(coordinates.longitude * 1000000),
                            "utcOffset": utcOffset/*,
                            "flare": flare_string*/});

      if (showSendInitiatedPage) {
         showSendInitiatedPage = false;
         console.log("Warping to: " + urlCfgCoordsSent);

         Pebble.openURL(urlCfgCoordsSent);
      }
   }
   else {
      //  Send coord / tz data to config screen to be displayed

      var coords = wrapCoords(coordinates.latitude, coordinates.longitude, utcOffset,
                              0, "");

      displayCoords(coords);
   }
}

function sendLocationError(code, message) {
   "use strict";

   if (coordsToPebble) {
      Pebble.sendAppMessage({"locationFailCode": code,
                             "locationFailMessage": message
                            });
   }
   else {
      var coords = wrapCoords(0, 0, 0,  code, message);

      displayCoords(coords);
   }
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
   sendLocationError(err.code, err.message);
}

///  Called when getCurrentPosition's guardian setTimeout() fires.
function outerTimeoutError() {
   "use strict";

   console.warn('outer timeout error (getCurrentPosition() guard fired)');
   sendLocationError(0,    // Shouldn't collide with PositionError
                     "No response from getCurrentPosition");
}


function setOuterTimer() {
   "use strict";

   clearOuterTimer();
   outerTimer = window.setTimeout(outerTimeoutError, outerTimeout);
}


/**
 *  Start asychronous coordinate read operation.
 *  
 *  Global var coordsToPebble steers whether results go to
 *  Pebble watch or to config screen display on phone.
 *  
 *  @param sendCoordsToPebble Set to true to send results
 *                   to Pebble Sunclock watchface. Set to false
 *                   to display on phone's Pebble app
 *                   supplemental config screen.
 *  @param confirmInitiated Set to true to launch "config"
 *                   display which simply confirms that we've
 *                   started the (async) send.  Without this
 *                   display, the user is faced with a
 *                   disconcerting close of the config window.
 */
function asyncReadCoords(sendCoordsToPebble, confirmInitiated) {
   "use strict";

   clearOuterTimer();

   coordsToPebble = sendCoordsToPebble;

   showSendInitiatedPage = confirmInitiated;

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

/**
 *  Use a reverse-geocoding service to find the placename
 *  corresponding to our current location, as found via an
 *  earlier asyncReadCoords() call.
 *  
 *    http://www.geonames.org/export/reverse-geocoding.html
 *  
 *  The service we use at present is geonames.org "find nearby"
 *  
 *    http://www.geonames.org/export/web-services.html#findNearby
 *  
 *  A sample query looks like:
 *  
 *    http://api.geonames.org/findNearbyJSON?lat=47.3&lng=9&username=demo&maxRows=1&style=SHORT
 *  
 *  for which the (JSON) response looks like
 *  
 *    (JSON example elided, since iOS Pebble app can't cope!)
 *  
 *  Or maybe that should be "find nearby place name"!
 *  
 *    http://www.geonames.org/export/web-services.html#findNearbyPlaceName
 *  
 *  whose sample query looks like
 *  
 *    http://api.geonames.org/findNearbyPlaceNameJSON?lat=47.3&lng=9&username=demo
 *  
 *  with a result of
 *  
 *    (JSON example elided, since iOS Pebble app can't cope!)
 *  
 *  geonames.org also has a timezone lookup, which might be fun
 *  at another time:
 *  
 *    http://www.geonames.org/export/web-services.html#timezone
 *  
 *  @param coords Collection as produced by wrapCoords().
 *                Typically looped back around to us via
 *                show_coords.html.
 */
function reverseGeoCode(coords) {
   "use strict";

   //  sample XHTMLRequest() example, from
   //    https://developer.getpebble.com/guides/js-apps/pebblekit-js/js-capabilities
   //  with W3C API documentation at
   //    http://www.w3.org/TR/XMLHttpRequest/
   //  and some great, thorough and pragmatic (incl. timeout!) usage info at
   //    https://www.inkling.com/read/javascript-definitive-guide-david-flanagan-6th/chapter-18/using-xmlhttprequest

   //  following xhr with timeout based on
   //    http://forums.getpebble.com/discussion/13224/problem-with-xmlhttprequest-timeout

   revGeoReq = new XMLHttpRequest();

   var url = 'http://api.geonames.org/findNearbyPlaceNameJSON?lat=' + coords['lat'] +
             '&lng=' + coords['long'] + '&maxRows=1&username=' + geonamesId;
   console.log("calling " + url);

   revGeoReq.open('GET', url, false /* use asynchronous */ );
   revGeoReq.onload = function() {

      if (revGeoReq.readyState === 4 && revGeoReq.status === 200) {
         window.clearTimeout(revGeoTimeout);

         console.log("xhr good response: " + revGeoReq.responseText);
         var response = JSON.parse(revGeoReq.responseText);
         console.log("xhr decoded response: " + JSON.stringify(response));

         //  in theory, JS closure assures that the mere act of referencing
         //  coords here will preserve it for use when this callback function
         //  in invoked.  See
         //    http://stackoverflow.com/questions/2690499/lifetime-of-javascript-variables
         coords['place-name'] = response.geonames[0].toponymName;
         coords['range'] = response.geonames[0].distance;
         coords['region'] = response.geonames[0].adminName1;
         coords['country'] = response.geonames[0].countryName;
         
         displayCoords(coords);
      }
   };
   revGeoReq.send(null);

   revGeoTimeout = window.setTimeout(function() {
      revGeoReq.abort();

      coords['place-name'] = "??? geonames.org timeout ???";
      //BUGBUG - is there some error code to display from revGeoReq?

      displayCoords(coords);
      },
      5000);   // timeout in ms

   //  go away until either onload or timeout functions fire.

}  /* end of function reverseGeoCode(coords) */

function parseIridiumFlares(str) {
   var re = /<a href=\"flaredetails.aspx.+?">(.+?)<\/a><\/td><td align="center">(.+?)<\/td><td align="center">\d+?°<\/td><td align="center">(\d+)°.+?<\/td>/g;
   var flares = [];
   while ((res = re.exec(str)) != null) {
      flares.push(
         {
            date: new Date(res[1] + " GMT"),
            brightness: res[2],
            azimuth: res[3]
         }
      );
   }
   return flares;
}

function getIridiumFlares(pos, callback) {
   var reqIridium = new XMLHttpRequest();
   /*
   var url = "http://www.heavens-above.com/IridiumFlares.aspx?lat=" + pos.coords.latitude +
             "&lng=" + pos.coords.longitude + "&alt=150&tz=GMT";
   */
   var url = "http://img.kmrov.ru/iridium.html";

   reqIridium.open('GET', url, false /* use asynchronous */ );
   reqIridium.onload = function() {
      if (reqIridium.readyState === 4 && reqIridium.status === 200) {
         var flares = parseIridiumFlares(reqIridium.responseText);
         callback(pos, flares);
      }
   }
   reqIridium.send(null);

   reqIridiumTimeout = window.setTimeout(function() {
      reqIridium.abort();

      callback(pos, []);
      },
   5000);   // timeout in ms
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
                              asyncReadCoords(true /* sendCoordsToPebble */ ,
                                              false /* do not show config display */ );
                           }
                        });

Pebble.addEventListener("showConfiguration",
                        function(){
                           "use strict";

                           console.log("launching configuration");

                           Pebble.openURL(urlCfgMain);
                        });

//  How requests and parameters come back to use from configuration pages.
Pebble.addEventListener("webviewclosed",
                        function(e){
                           "use strict";

                           console.log(e.type);
                           console.log(e.response);
                           console.log("webview closed, response: " + e.response);

                           if (e.response !== "CANCELLED") {
                              realResponse = e.response;
                           }
                           else {
                              if (realResponse === "") {
                                 console.log("no real response to use in lieu of CANCELLED");
                                 return;
                              }

                              console.log("retrying previous after CANCELLED");
                           }

                           if (realResponse === "cancel") {
                              console.log("js-app: configuration cancelled");
                           }
                           else if (realResponse === "show-coords") {
                              //  per user request, read coords & display them on cfg screen
                              console.log("js-app: launching coords disp query");
                              asyncReadCoords(false /* sendCoordsToPebble */ ,
                                              false /* do not show send confirmation */);
                           }
                           else if (realResponse === "send-coords") {
                              //  per user request, send coords to watch
                              console.log("js-app: sending coords to watch");
                              asyncReadCoords(true /* sendCoordsToPebble */ ,
                                              true /* show send confirmation msg */ );
                           }
                           else if (realResponse === "show-config") {
                              //  per user request, back to main config window
                              console.log("js-app: re-displaying main config window");
                              Pebble.openURL(urlCfgMain);
                           }
                           else if (realResponse.lastIndexOf("decode-", 0) === 0) {
                              //  got a reverse geocode request, everything after
                              //  the '-' is the output of wrapCoords(), passed back
                              //  to us from show_coords.html.
                              var wrapped = realResponse.substring(7);
                              console.log("js-app: decoding " + wrapped);
                              var coords = JSON.parse(decodeURIComponent(wrapped));
                              reverseGeoCode(coords);
                           }
                           else {
                              var conf_data = JSON.parse(decodeURIComponent(e.response));
                              console.log("js-app: conf returned: ",
                                          JSON.stringify(conf_data));
                           }
                        });

