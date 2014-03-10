This is a Pebble watch face, originally created (to the best of my understanding) by MichaelEhrmann, and then updated by KarbonPebbler, Boldo, Chad Harp, Bob TheMadCow and orgemd.  (My apologies if I have left out any other contributors.  If I have, please let me know.)

# Instructions

To use this watchface, your Pebble watch and phone app must be running Pebble's version 2 code.

After you install the version 2 sunclock, when you first run it be sure that your phone is connected both to the watch and to a suitable location source (may be the phone network).  Each time the sunclock watchface is started on the Pebble, it will attempt to connect to the phone and obtain current location and timezone data.  When this is successful, the resulting data will be saved in the Pebble watch's flash for later use.

The first time the v2 app is run it will put up a message screen while retrieving location data.  If the retrieval fails an error screen will be displayed.  Once the app has managed to save a usable location, it will use that value immediately when restarted, but still attempt to obtain current location data.  Thus when you travel to a different location, or when daylight savings time changes, simply ensure that your phone is connected to the watch and network, and exit/restart the sunclock watchface.

# History

2.0.1

Fully initialize text fields before displaying the watchface. Had avoided this due to display hesitations in Pebble v1.x, but Pebble 2.x doesn't have the same difficulty.

2.0.0

Initial port to Pebble version 2 environment.

# Lineage

This version is a tweak of the updates posted by orgemd.

The following changes have been made to orgemd's version:

1.  Source code converted to use Pebble's SDK version 2 APIs.
2.  A considerable amount of code refactoring was done, both to reduce code space and hopefully to clarify the code.
3.  Some comments were added, at least for the parts I deciphered.
4.  A separate message window was added, along with comms code to query a new phone-based javascript component for location and timezone offset data.  To keep from eating the battery alive the location query is run only once, each time the sunclock watchface is started.
5.  A couple routines were set up to provide a standardized watch-based persistence interface.  This is used to save the returned location data and supply it onsubsequent restarts.
6.  Due to the above, there is no longer a compile-time setting for location. The watchface always obtains location dynamically, and will display an error message if it is not (ever) able to obtain one.  But if it had previously saved a location in flash, it will use that value instead.

-----------------------

The following is a list of orgemd's changes from Bob TheMadCow's version:

1.  Moon phase is fixed so that it shows correctly.
2.  The watch hand has been slimmed down so that it obscures less.
3.  The day/date information has been changed to a single line such as "Mon Oct 8, 2013" rather than four separate pieces.
4.  The watch face has been shifted down to allow the single day/date line.
5.  The digital time on the watch face has been made much larger to be easier to read.
6.  The numbers 6,12,18,24 have been removed from the watch face to make it less cluttered.
7.  The hourly tick marks on the face have been made slightly larger.
8.  Some tweaks to the sunrise/sunset text display.

There is both a compiled version included, as well as the src and resources stuff needed to compile it yourself.


