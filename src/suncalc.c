/**
 *  @file
 *  
 */
#include "suncalc.h"
#include "my_math.h"

/** 
 *  Given a date and geographical location (lat/long), calculate
 *  rise or set time. Nominally of sun, but may be adjusted to
 *  return various twilight times instead by means of our zenith
 *  argument.
 *  
 *  Math based on 
 *    http://williams.best.vwh.net/sunrise_sunset_algorithm.htm
 *  which in turn cites
 *  	Almanac for Computers, 1990
 * 	published by Nautical Almanac Office
 * 	United States Naval Observatory
 *    Washington, DC 20392
 *  
 *  @param year Four-digit gregorian year value. UTC. ?
 *  @param month Month of year, 1 - 12. UTC. ?
 *  @param day Day of month, 1 - 31. UTC. ?
 *  @param latitude -90.0 - +90.0. ?
 *  @param longitude -180 - +180. ?
 *  @param sunset True (non-zero) to calculate set time, false
 *                (zero) for rise time.
 *  @param zenith. Per the page cited above, useful zenith values are:
 *                   official rise/set     = 90 degrees 50'
 *                   civil twilight end    = 96 degrees
 *                   nautical twilight end = 102 degrees
 *                   astronomical twi. end = 108 degrees (i.e., night)
 *  
 *  @return Requested time given as UTC hour and fraction.  Or NO_RISE_SET_TIME
 *          if there is no rise/set for this location on this date (i.e., near
 *          a pole).
 */
float calcSun(int year, int month, int day,
              float latitude, float longitude, int sunset, float zenith)
{


   // 1. first calculate the day of the year

   int N1 = my_floor(275 * month / 9);
   int N2 = my_floor((month + 9) / 12);  // 1 = after Feb, 0 = not.
   int N3 = (1 + my_floor((year - 4 * my_floor(year / 4) + 2) / 3));
   int N = N1 - (N2 * N3) + day - 30;

   // 2. convert the longitude to hour value and calculate an approximate time

   float lngHour = longitude / 15;

   float t;
   if (!sunset)
   {
      //if rising time is desired:
      t = N + ((6 - lngHour) / 24);
   }
   else
   {
      //if setting time is desired:
      t = N + ((18 - lngHour) / 24);
   }

   // 3. calculate the Sun's mean anomaly
   float M = (0.9856 * t) - 3.289;

   // 4. calculate the Sun's true longitude

   //L = M + (1.916 * sin(M)) + (0.020 * sin(2 * M)) + 282.634
   float L = M + (1.916 * my_sin((M_PI / 180.0f) * M)) + (0.020 * my_sin((M_PI / 180.0f) * 2 * M)) + 282.634;
   if (L < 0) L += 360.0f;
   if (L > 360) L -= 360.0f;

   //5a. calculate the Sun's right ascension

   //RA = atan(0.91764 * tan(L))
   float RA = (180.0f / M_PI) * my_atan(0.91764 * my_tan((M_PI / 180.0f) * L));
   if (RA < 0) RA += 360;
   if (RA > 360) RA -= 360;

   //5b. right ascension value needs to be in the same quadrant as L

   float Lquadrant  = (my_floor(L / 90)) * 90;
   float RAquadrant = (my_floor(RA / 90)) * 90;
   RA = RA + (Lquadrant - RAquadrant);

   //5c. right ascension value needs to be converted into hours
   RA = RA / 15;

   //6. calculate the Sun's declination

   float sinDec = 0.39782 * my_sin((M_PI / 180.0f) * L);
   float cosDec = my_cos(my_asin(sinDec));

   //7a. calculate the Sun's local hour angle

   //cosH = (cos(zenith) - (sinDec * sin(latitude))) / (cosDec * cos(latitude))
   float cosH = (my_cos((M_PI / 180.0f) * zenith) - (sinDec * my_sin((M_PI / 180.0f) * latitude))) / (cosDec * my_cos((M_PI / 180.0f) * latitude));

   if (cosH >  1)
   {
      return NO_RISE_SET_TIME;
   }
   else if (cosH < -1)
   {
      return NO_RISE_SET_TIME;
   }

   //7b. finish calculating H and convert into hours

   float H;
   if (!sunset)
   {
      //if rising time is desired:
      H = 360 - (180.0f / M_PI) * my_acos(cosH);
   }
   else
   {
      //if setting time is desired:
      H = (180.0f / M_PI) * my_acos(cosH);
   }

   H = H / 15;

   //8. calculate local mean time of rising/setting
   float T = H + RA - (0.06571 * t) - 6.622;

   //9. adjust back to UTC
   float UT = T - lngHour;
   if (UT < 0)
   {
      UT += 24;
   }
   if (UT > 24)
   {
      UT -= 24;
   }

   return UT;

}  /* end of calcSun */

float calcSunRise(int year, int month, int day, float latitude, float longitude, float zenith)
{
   return calcSun(year, month, day, latitude, longitude, 0, zenith);
}

float calcSunSet(int year, int month, int day, float latitude, float longitude, float zenith)
{
   return calcSun(year, month, day, latitude, longitude, 1, zenith);
}
