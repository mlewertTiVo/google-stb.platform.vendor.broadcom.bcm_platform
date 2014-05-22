#ifndef __NEXUSINIT_H__
#define __NEXUSINIT_H__

#include "nexus_platform.h"
#include "nexus_display.h"
#if NEXUS_HAS_GRAPHICS2D
#include "nexus_graphics2d.h"
#endif

#if NEXUS_DTV_PLATFORM || (NEXUS_NUM_DISPLAYS <= 1)
#define NUM_DISPLAYS 1
#else
#define NUM_DISPLAYS 2
#endif

/* Includes graphics window and video windows */
#define NUM_GFX_LAYERS_PER_DISPLAY  1
#define NUM_VID_LAYERS_PER_DISPLAY  NEXUS_NUM_VIDEO_WINDOWS
#define NUM_LAYERS_PER_DISPLAY      (NUM_GFX_LAYERS + NUM_VID_LAYERS)
#define NUM_LAYERS                  (NUM_LAYERS_PER_DISPLAY * NUM_DISPLAYS)


extern int nexus_graphics_heap_index;


/* Specify default Graphics Heap Size here.
   NOTE: This can be overriden by specifying
         "gfx_heap_size = abc" in the environment.

Modified to 64MB in the Android Environment
*/

#define DEFAULT_GFX_HEAP_SIZE (64 * 1024 * 1024)

#define SET_NEXUS_GRAPHICS_HEAP_IDX(index) (nexus_graphics_heap_index = index)
#define GET_NEXUS_GRAPHICS_HEAP_IDX()      nexus_graphics_heap_index


#endif
