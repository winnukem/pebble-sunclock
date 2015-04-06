/**
 *  @file
 *  
 *  A few constants to simplify kicking the Sunclock into test modes for debug.
 */

#ifndef sunclock_testing_h__
#define sunclock_testing_h__


///  Set true to always fail to read from watch's location cache, even when valid.
#define  TESTING_DISABLE_CACHE_READ  0

///  Set true to disable normal load-time send of location update request to phone.
#define  TESTING_DISABLE_LOCATION_REQUEST  0


#endif  // #ifndef sunclock_testing_h__

