/**
 *  @file
 *  
 *  Miscellanous support routines / macros.
 */

#pragma once


///  Helper for destroying a possibly allocated Pebble entity.
#define SAFE_DESTROY(prefix, pAlloc) \
   if (pAlloc != NULL)               \
   {                                 \
      prefix##_destroy(pAlloc);      \
      pAlloc = NULL;                 \
   }

