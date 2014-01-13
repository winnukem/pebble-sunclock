/**
 *  @file
 *  
 */

#pragma once

#include  "pebble.h"


///  Carries all data needed to draw a rotatable "png-trans" bitmap resource.
typedef struct
{

   //  Bitmap data extracted from single "png-trans" resource by Pebble.
   GBitmap* pBmpWhiteMask;
   GBitmap* pBmpBlackMask;

   //  RotBitmapLayer only supports a single bitmap, so for transparency
   //  we need two layers.
   RotBitmapLayer* pRbmpWhiteLayer;
   RotBitmapLayer* pRbmpBlackLayer;

} TransRotBmp;


/**
 *  Public means of instantiating TransRotBmp.  We load the bitmaps needed to
 *  render a transparent image resource at any orientation, and return a pointer
 *  to the newly created carrier object.
 *  
 *  This interface automatically infers the _WHITE / _BLACK resource suffixes
 *  generated by pebble for a "png-trans" base resource type.  This is not well
 *  documented, but is described in this forum post:
 *  
 *     http://forums.getpebble.com/discussion/4596/transparent-png-support
 *  
 *  So our single argument is exactly the name shown for the desired "png-trans"
 *  resource in the appinfo.json resources / media section (but expressed as
 *  a manifest, not a string).
 */
#define transrotbmp_create_with_resource_prefix(RESOURCE_ID_STEM_)  \
   transrotbmp_create_with_resources(RESOURCE_ID_STEM_ ## _WHITE,   \
                                     RESOURCE_ID_STEM_ ## _BLACK)

   
/**
 *  Set the "src ic" for our image layers.
 *  This isn't documented afaict, but speculate that this is the pivot
 *  point within the image about which to perform rotations.
 */
void  transrotbmp_set_src_ic(TransRotBmp *pTransBmp, GPoint ic);

/**
 *  Add our image layers to the supplied parent layer.
 */
void  transrotbmp_add_to_layer(TransRotBmp *pTransBmp, Layer *parent);

///  Set the angle at which our image resource is rendered.
void  transrotbmp_set_angle(TransRotBmp *pTransBmp, int32_t angle);

/**
 *  Set the image centered on the screen, but with a caller-specified offset.
 *  
 *  @param pTransBmp Image to position.
 *  @param offsetX X-offset from centered position, positive is towards the right.
 *  @param offsetY Y-offset from centered position, positive is towards the bottom.
 */
void transrotbmp_set_pos_centered(TransRotBmp *pTransBmp, int32_t offsetX, int32_t offsetY);

///  Destroy a bitmap instance created using transrotbmp_create_with_resource_prefix().
void  transrotbmp_destroy(TransRotBmp *pTransBmp);


/**
 *  Actual creation routine, use transrotbmp_create_with_resource_prefix()
 *  instead of calling this directly.
 */
TransRotBmp* transrotbmp_create_with_resources(uint32_t residWhiteMask,
                                               uint32_t residBlackMask);

