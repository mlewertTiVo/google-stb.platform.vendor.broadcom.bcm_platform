
/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  converter
Module   :

FILE DESCRIPTION
standalone conversion function which will convert a RSO image to a TFORMAT
image.  For LT sized images the RSO version will suffice, so no conversion is
provided for this path.

Fast paths are just ripped straight from the GL driver, slow paths have been
improved due to the limtied formats supported
=============================================================================*/

#include "nexus_graphicsv3d.h"

#include <assert.h>

/* #define FLIP_Y */

#if defined(_MSC_VER)
#include "stdbool.h"
#else
/* GNU compilers, pick up the default header */
#include <stdbool.h>
#endif

#include "converter.h"

#define NULL_LIST_SIZE 27

static uint64_t s_jobSequenceIssued = 0;

void tfconvert_hw(
   pix_format_e pf, bool direction, uint8_t *p,
   NEXUS_Graphicsv3dHandle nexusHandle,
   uint8_t * dst, uint32_t dst_width, uint32_t dst_stride, uint32_t dst_height,
   uint32_t dst_x, uint32_t dst_y,
   uint32_t width, uint32_t height,
   uint8_t * src, uint32_t src_width, uint32_t src_stride, uint32_t src_height,
   uint32_t src_x, uint32_t src_y)
{
   uint32_t xTiles;
   uint32_t yTiles;
   uint32_t numDirtyTiles;
   uint32_t numBytes;
   uint32_t tlbFmt;
   uint32_t tlbStoreFmt;
   uint32_t tlbType;
   uint8_t *startAddr;
   bool store;
   int i, j;
   NEXUS_Graphicsv3dJob nJob;
   uint32_t phySrc, phyDst, phyAddr;
   NEXUS_Error rc;

   assert(src_width == dst_width && src_height == dst_height);

   phySrc = NEXUS_AddrToOffset(src);
   phyDst = NEXUS_AddrToOffset(dst);
   phyAddr = NEXUS_AddrToOffset(p);
   startAddr = p;

   xTiles = (src_width  + 63) / 64;
   yTiles = (src_height + 63) / 64;

   numDirtyTiles = xTiles * yTiles;

   numBytes = 11 + (numDirtyTiles * 11);
   if (numBytes < NULL_LIST_SIZE)
      numBytes = NULL_LIST_SIZE;    /* Must have at least this many bytes in case we need a NULL list */

   /* set rendering mode config */
   *p++ = 113; /* KHRN_HW_INSTR_STATE_TILE_RENDERING_MODE */
   /* destination address as a word, but packed */
   p[0] = (uint8_t)((uintptr_t)phyDst);
   p[1] = (uint8_t)((uintptr_t)phyDst >> 8);
   p[2] = (uint8_t)((uintptr_t)phyDst >> 16);
   p[3] = (uint8_t)((uintptr_t)phyDst >> 24);
   p += 4;
   /* width as a short, but packed */
   p[0] = (uint8_t)src_width;
   p[1] = (uint8_t)(src_width >> 8);
   p += 2;
   /* height as a short, but packed */
   p[0] = (uint8_t)src_height;
   p[1] = (uint8_t)(src_height >> 8);
   p += 2;

   /* we just need to pull this into 32/16bpp differentiation */
   if (pf == ABGR_8888 || pf == XBGR_8888 || pf == ABGR_8888_LT || pf == XBGR_8888_LT)
   {
      tlbFmt = 1;  /* IMAGE_FORMAT_8888 */
      tlbStoreFmt = 0;
      tlbType = (pf == ABGR_8888 || pf == XBGR_8888) ? 1 : 2;
   }
   else if (pf == RGB_565 || pf == RGB_565_LT)
   {
      tlbFmt = 2;
      tlbStoreFmt = 2;
      tlbType = (pf == RGB_565) ? 1 : 2;
   }
   else
   {
      BCM_DEBUG_ERRMSG("BRCM-GRALLOC: cant convert this image type in HW");
      assert(0);
   }

   *p++ =
      (0 << 0) | /* disable ms mode */
      (0 << 1) | /* disable 64-bit color mode */
      (tlbFmt << 2) |
      (0 << 4) | /* no decimation */
      (direction ? (tlbType << 6) : 0); /* RSO/TF */

   *p++ =
      (0 << 0) | /* no vg mask */
      (0 << 1) | /* no coverage mode */
      (0 << 2) | /* don't care z-update dir */
      (0 << 3) | /* disable early Z */
      (0 << 4); /* no double buffer */

   store = false;
   for (j = 0; j != (int)yTiles; ++j)
   {
      for (i = 0; i != (int)xTiles; ++i)
      {
         /* We write the store for the previous tile here as we're not storing all tiles.
          * We can then add the final store with EOF after the loop */
         if (store)
            *p++ = 24; /* KHRN_HW_INSTR_STORE_SUBSAMPLE */

         *p++ = 29; /* KHRN_HW_INSTR_LOAD_GENERAL */

         *p++ =
            (1 << 0) | /* load color */
            (direction ? 0 : (tlbType << 4)); /* RSO/TF */

         *p++ = tlbStoreFmt;

         /* source address as a word, but packed */
         p[0] = (uint8_t)((uintptr_t)phySrc);
         p[1] = (uint8_t)((uintptr_t)phySrc >> 8);
         p[2] = (uint8_t)((uintptr_t)phySrc >> 16);
         p[3] = (uint8_t)((uintptr_t)phySrc >> 24);
         p += 4;

         *p++ = 115; /* KHRN_HW_INSTR_STATE_TILE_COORDS */
         *p++ = i;
         *p++ = j;

         store = true;
      }
   }

   /* Add the final store with EOF */
   *p++ = 25; /* KHRN_HW_INSTR_STORE_SUBSAMPLE_EOF */

   /* Pad end with NOP's if necessary */
   for (i = 11 + (numDirtyTiles * 11); i < (int)numBytes; i++)
      *p++ = 1; /* KHRN_HW_INSTR_NOP */

   memset(&nJob, 0, sizeof(NEXUS_Graphicsv3dJob));

   nJob.sProgram[0].eOperation      = NEXUS_Graphicsv3dOperation_eRenderInstr;
   nJob.sProgram[0].uiArg1          = phyAddr; /* start of control list */
   nJob.sProgram[0].uiArg2          = phyAddr + numBytes; /* end of control list */
   nJob.sProgram[0].uiCallbackParam = 0xFFFFFFFF; /* just trigger the callback (JobCallbackHandler()).  Not bothered about any meta-data */

   nJob.uiBinMemory      = 0; /* render job only, no bin memory required */
   nJob.uiUserVPM        = 0;
   nJob.bCollectTimeline = 0;
   nJob.uiJobSequence    = s_jobSequenceIssued++;

   NEXUS_FlushCache(startAddr, numBytes);

   rc = NEXUS_Graphicsv3d_SendJob(nexusHandle, &nJob);
   if (rc != NEXUS_SUCCESS)
   {
      BCM_DEBUG_ERRMSG("BRCM-GRALLOC: unable to send the job to the Graphicsv3d module");
      assert(0);
   }
}

/* TFORMAT ACCELERATION ROUTINES
 *
 * Copying from linear to TFORMAT follows the following algorithm:
 *
 * For each 4k TILE
 *    for each tile row
 *       for each pixel in row
 *           copy from linear to Tformat
 *
 * Thus, the linear memory is accessed sequentially for each row and the data scattered
 * into the TFORMAT block.  Most of the address calculation is performed in the outer loops
 * with simple offsets calculated in the inner loops.
 *
 * Each routine has two paths -- a fast path that copies (groups of) texels as integers,
 * and a slow path that copies individual texels.  The slow path is used at the edges of
 * an image if it does not coincide with a tile boundary, or when the aligment of the data
 * does not allow an integer copy.
 *
 * Addresses in the TFORMAT image are calculated using lookup tables/bitwise operations
 *
 * For a texel addressed by (X, Y):
 *
 * TFORMAT (for 32bpp) address calculated as:
 *
 * +---------------------------------------------------------------------------------+
 * | TILE ADDRESSS | MAP[Y5:Y4:X4] | Y3 | Y2 | X3 | X2 | Y1 | Y0 | X1 | X0 |  0 |  0 |
 * +---------------------------------------------------------------------------------+
 *
 * TFORMAT (for 16bpp) address calculated as:
 *
 * +---------------------------------------------------------------------------------+
 * | TILE ADDRESSS | MAP[Y5:Y4:X5] | Y3 | Y2 | X4 | X3 | Y1 | Y0 | X2 | X1 | X0 |  0 |
 * +---------------------------------------------------------------------------------+
 *
 * TFORMAT (for 8bpp) address calculated as:
 *
 * +---------------------------------------------------------------------------------+
 * | TILE ADDRESSS | MAP[Y6:Y5:X5] | Y4 | Y3 | X4 | X3 | Y2 | Y1 | Y0 | X2 | X1 | X0 |
 * +---------------------------------------------------------------------------------+
 *
 * The low order bits of the address are calculated from the subX and subY position
 * using the appropriate XSWIZZLE/YSWIZZLE macros
 *
 * MAP converts its argument bits into the tile address offset.  The Y5 (32 and 16bpp) or
 *     Y6 bit selects between even and odd tile rows (which have inverted horizontal
 *     tile order and different sub-tile addressing).
 *           0x000, 0xc00, 0x400, 0x800,
 *           0x800, 0x400, 0xc00, 0x000
 *
 * MAP is encoded in a single word in the routines below to minimise memory accesses
 * for array lookups.
 *
 * TILE ADDRESS is calculated from the block X and Y position, noting that the X position
 *              is inverted if Y5 (Y6 for 8bit) = 1 (odd block rows)
 */

const uint32_t  MAP_ODD  = 0x0c48;
const uint32_t  MAP_EVEN = 0x84c0;

/* These convert the lower four/five bits of x-address into the corresponding bits of the Tformat address
 */
#define XSWIZZLE32(X)  ((((X) & 0x3) << 0) | (((X) & 0xc ) << 2)) // 32-bit offset
#define XSWIZZLE16(X)  ((((X) & 0x7) << 0) | (((X) & 0x18) << 2)) // 16 bit offset
#define XSWIZZLE8(X)   ((((X) & 0x7) << 0) | (((X) & 0x18) << 3)) //  8 bit offset

/* These converts the lower four/five bits of y-address into the corresponding bits of the Tformat address
 */
#define YSWIZZLE32(Y)  ((((Y) & 0x3) << 4) | (((Y) & 0xc ) << 6)) // 8-bit offset
#define YSWIZZLE16(Y)  ((((Y) & 0x3) << 4) | (((Y) & 0xc ) << 6)) // 8-bit offset
#define YSWIZZLE8(Y)   ((((Y) & 0x7) << 3) | (((Y) & 0x18) << 5)) // 8-bit offset

static const uint32_t   TILE_SIZE = 4096;

/* khrn_copy_rgba8888_to_tf32
 *
 * Copies an array in RGBA 32-bit linear format to the equivalent 32-bit Tformat array
 */
static void khrn_copy_rgba8888_to_tf32(
   uint8_t   *dst,                             /* address of Tformat image                                             */
   uint32_t  dstWidth,                         /* width of destination in texels                                       */
   uint32_t  dstStartX, uint32_t dstStartY,    /* bottom left of destination region                                    */
   uint32_t  width,     uint32_t height,       /* top right of destination region                                      */
   uint8_t   *src,                             /* address of linear image                                              */
   uint32_t  srcStartX, uint32_t srcStartY,    /* bottom left of source region                                         */
   uint32_t  srcStride)                        /* stride of source image in bytes (i.e. line length including padding) */
{
   static const uint32_t   TILE_XSHIFT    = 5;
   static const uint32_t   TILE_XBITS     = 0x1f;
   static const uint32_t   TILE_YSHIFT    = 5;
   static const uint32_t   TILE_YBITS     = 0x1f;
   static const uint32_t   SUB_TILE_WIDTH = 16;
   static const uint32_t   TEXEL_SIZE     = 4;

   uint32_t   blockY, blockX, subY, subX;

   uint32_t   dstBlockWidth  = ((dstWidth + TILE_XBITS) & ~TILE_XBITS) >> TILE_XSHIFT;  /* Width in 4k tiles */
   uint32_t   dstBlockStride = dstBlockWidth * TILE_SIZE;

   uint32_t   dstEndX    = dstStartX + width - 1;
   uint32_t   dstEndY    = dstStartY + height - 1;

   uint32_t   startBlockX = dstStartX >> TILE_XSHIFT;
   uint32_t   startBlockY = dstStartY >> TILE_YSHIFT;
   uint32_t   endBlockX   = dstEndX   >> TILE_XSHIFT;
   uint32_t   endBlockY   = dstEndY   >> TILE_YSHIFT;

   int   offX = srcStartX - dstStartX;
   int   offY = srcStartY - dstStartY;

   /* For each 4k tile vertically */
   for (blockY = startBlockY; blockY <= endBlockY; ++blockY)
   {
      uint32_t   startSubY = blockY == startBlockY ? (dstStartY & TILE_YBITS) : 0;
      uint32_t   endSubY   = blockY == endBlockY   ? (dstEndY   & TILE_YBITS) : TILE_YBITS;

      uint32_t   odd = blockY & 1;
      uint32_t   mask = odd ? MAP_ODD : MAP_EVEN;

      /* For each 4k tile horizontally */
      for (blockX = startBlockX; blockX <= endBlockX; ++blockX)
      {
         uint32_t   startSubX = blockX == startBlockX ? (dstStartX & TILE_XBITS) : 0;
         uint32_t   endSubX   = blockX == endBlockX   ? (dstEndX   & TILE_XBITS) : TILE_XBITS;
         bool       fast      = startSubX == 0 && endSubX == TILE_XBITS;

         uint32_t   blockXR   = odd ? dstBlockWidth - blockX - 1 : blockX;

         /* Address of tile */
         uint8_t     *tileAddr  = dst + blockY * dstBlockStride + blockXR * TILE_SIZE;
         uint32_t    Y = (blockY << TILE_YSHIFT) + offY;
         uint32_t    X = (blockX << TILE_XSHIFT) + startSubX + offX;

         /* For each row in the tile */
         for (subY = startSubY; subY <= endSubY; ++subY)
         {
#ifdef FLIP_Y
            uint32_t  *src_texel = (uint32_t *)(src - (Y + subY) * srcStride + X * TEXEL_SIZE);
#else
            uint32_t  *src_texel = (uint32_t *)(src + (Y + subY) * srcStride + X * TEXEL_SIZE);
#endif

            uint32_t  offset     = mask >> ((subY & 0x10) >> 1);
            uint8_t   *rowAddr   = tileAddr + YSWIZZLE32(subY);
            uint32_t  *rowAddr0  = (uint32_t *)(rowAddr + ((offset & 0x0f) << 8));
            uint32_t  *rowAddr1  = (uint32_t *)(rowAddr + ((offset & 0xf0) << 4));

            if (fast)
            {
               rowAddr0[0x00] = src_texel[0];   rowAddr0[0x01] = src_texel[1]; 
               rowAddr0[0x02] = src_texel[2];   rowAddr0[0x03] = src_texel[3]; 
               rowAddr0[0x10] = src_texel[4];   rowAddr0[0x11] = src_texel[5]; 
               rowAddr0[0x12] = src_texel[6];   rowAddr0[0x13] = src_texel[7]; 
               rowAddr0[0x20] = src_texel[8];   rowAddr0[0x21] = src_texel[9]; 
               rowAddr0[0x22] = src_texel[10];  rowAddr0[0x23] = src_texel[11];
               rowAddr0[0x30] = src_texel[12];  rowAddr0[0x31] = src_texel[13];
               rowAddr0[0x32] = src_texel[14];  rowAddr0[0x33] = src_texel[15];

               rowAddr1[0x00] = src_texel[16];  rowAddr1[0x01] = src_texel[17];
               rowAddr1[0x02] = src_texel[18];  rowAddr1[0x03] = src_texel[19];
               rowAddr1[0x10] = src_texel[20];  rowAddr1[0x11] = src_texel[21];
               rowAddr1[0x12] = src_texel[22];  rowAddr1[0x13] = src_texel[23];
               rowAddr1[0x20] = src_texel[24];  rowAddr1[0x21] = src_texel[25];
               rowAddr1[0x22] = src_texel[26];  rowAddr1[0x23] = src_texel[27];
               rowAddr1[0x30] = src_texel[28];  rowAddr1[0x31] = src_texel[29];
               rowAddr1[0x32] = src_texel[30];  rowAddr1[0x33] = src_texel[31];
            }
            else
            {
               /* Slower path for edges of image */
               uint32_t   endSubX1 = endSubX < (SUB_TILE_WIDTH - 1) ? endSubX : SUB_TILE_WIDTH - 1;

               for (subX = startSubX; subX <= endSubX1; ++subX)
               {
                  uint32_t *dst_texel =  rowAddr0 + XSWIZZLE32(subX);
                  *dst_texel = *src_texel;
                  src_texel += 1;
               }

               for (subX = startSubX > SUB_TILE_WIDTH ? startSubX : SUB_TILE_WIDTH; subX <= endSubX; ++subX)
               {
                  uint32_t *dst_texel =  rowAddr1 + XSWIZZLE32(subX);
                  *dst_texel = *src_texel;
                  src_texel += 1;
               }
            }
         }
      }
   }
}

/* khrn_copy_rgb888_to_tf32
 *
 * Copies an array in RGB 24-bit linear format to the equivalent 32-bit Tformat array
 */
static void khrn_copy_rgb888_to_tf32(
   uint8_t   *dst,                             /* address of Tformat image                                               */
   uint32_t  dstWidth,                         /* width of destination in texels                                         */
   uint32_t  dstStartX, uint32_t dstStartY,    /* bottom left of destination region                                      */
   uint32_t  width,     uint32_t height,       /* top right of destination region                                        */
   uint8_t   *src,                             /* address of linear image                                                */
   uint32_t  srcStartX, uint32_t srcStartY,    /* bottom left of source region                                           */
   uint32_t  srcStride)                        /* stride of source image in bytes (i.e. line length including padding)   */
{
   static const uint32_t   TILE_XSHIFT     = 5;
   static const uint32_t   TILE_XBITS      = 0x1f;
   static const uint32_t   TILE_YSHIFT     = 5;
   static const uint32_t   TILE_YBITS      = 0x1f;
   static const uint32_t   SUB_TILE_WIDTH  = 16;
   static const uint32_t   TEXEL_SIZE      = 3;

   uint32_t   blockY, blockX, subY, subX;

   uint32_t   dstBlockWidth  = ((dstWidth + TILE_XBITS) & ~TILE_XBITS) >> TILE_XSHIFT;  /* Width in 4k tiles */
   uint32_t   dstBlockStride = dstBlockWidth * TILE_SIZE;

   uint32_t   dstEndX    = dstStartX + width - 1;
   uint32_t   dstEndY    = dstStartY + height - 1;

   uint32_t   startBlockX = dstStartX >> TILE_XSHIFT;
   uint32_t   startBlockY = dstStartY >> TILE_YSHIFT;
   uint32_t   endBlockX   = dstEndX   >> TILE_XSHIFT;
   uint32_t   endBlockY   = dstEndY   >> TILE_YSHIFT;

   int   offX = srcStartX - dstStartX;
   int   offY = srcStartY - dstStartY;

   /* For each 4k block vertically */
   for (blockY = startBlockY; blockY <= endBlockY; ++blockY)
   {
      uint32_t startSubY = blockY == startBlockY ? (dstStartY & TILE_YBITS) : 0;
      uint32_t endSubY   = blockY == endBlockY   ? (dstEndY   & TILE_YBITS) : TILE_YBITS;

      uint32_t   odd  = blockY & 1;
      uint32_t   mask = odd ? MAP_ODD : MAP_EVEN;

      /* For each 4k block horizontally */
      for (blockX = startBlockX; blockX <= endBlockX; ++blockX)
      {
         uint32_t   startSubX = blockX == startBlockX ? (dstStartX & TILE_XBITS) : 0;
         uint32_t   endSubX   = blockX == endBlockX   ? (dstEndX   & TILE_XBITS) : TILE_XBITS;
         bool       fast      = offX == 0 && startSubX == 0 && endSubX == TILE_XBITS;

         uint32_t   blockXR   = odd ? dstBlockWidth - blockX - 1 : blockX;

         /* Address of tile */
         uint8_t     *tileAddr  = dst + blockY * dstBlockStride + blockXR * TILE_SIZE;
         uint32_t    Y = (blockY << TILE_YSHIFT) + offY;
         uint32_t    X = (blockX << TILE_XSHIFT) + startSubX + offX;

         /* For each row in the tile */
         for (subY = startSubY; subY <= endSubY; ++subY)
         {
#ifdef FLIP_Y
            uint8_t   *src_texel_8 = src - (Y + subY) * srcStride + X * TEXEL_SIZE;
#else
            uint8_t   *src_texel_8 = src + (Y + subY) * srcStride + X * TEXEL_SIZE;
#endif

            uint32_t  offset     = mask >> ((subY & 0x10) >> 1);
            uint8_t   *rowAddr   = tileAddr + YSWIZZLE32(subY);
            uint32_t  *rowAddr0  = (uint32_t *)(rowAddr + ((offset & 0x0f) << 8));
            uint32_t  *rowAddr1  = (uint32_t *)(rowAddr + ((offset & 0xf0) << 4));

            if (fast)
            {
               uint32_t *src_texel = (uint32_t *)src_texel_8;

               /* Fast path for middle of image */
               uint32_t   intA, intB, intC;

               /* FIRST batch of 16 texels */
               /* Four pixels (three reads from source) */
               intA = src_texel[0];   rowAddr0[0x00] = intA;
               intB = src_texel[1];   rowAddr0[0x01] = intA >> 24 | intB << 8;
               intC = src_texel[2];   rowAddr0[0x02] = intB >> 16 | intC << 16;
                                      rowAddr0[0x03] = intC >> 8;

               /* Four pixels (three reads from source) */
               intA = src_texel[3];   rowAddr0[0x10] = intA;
               intB = src_texel[4];   rowAddr0[0x11] = intA >> 24 | intB << 8;
               intC = src_texel[5];   rowAddr0[0x12] = intB >> 16 | intC << 16;
                                      rowAddr0[0x13] = intC >> 8;

               /* Four pixels (three reads from source) */
               intA = src_texel[6];   rowAddr0[0x20] = intA;
               intB = src_texel[7];   rowAddr0[0x21] = intA >> 24 | intB << 8;
               intC = src_texel[8];   rowAddr0[0x22] = intB >> 16 | intC << 16;
                                      rowAddr0[0x23] = intC >> 8;

               /* Four pixels (three reads from source) */
               intA = src_texel[9];   rowAddr0[0x30] = intA;
               intB = src_texel[10];  rowAddr0[0x31] = intA >> 24 | intB << 8;
               intC = src_texel[11];  rowAddr0[0x32] = intB >> 16 | intC << 16;
                                      rowAddr0[0x33] = intC >> 8;

               /* SECOND batch of 16 texels */
               /* Four pixels (three reads from source) */
               intA = src_texel[12];  rowAddr1[0x00] = intA;
               intB = src_texel[13];  rowAddr1[0x01] = intA >> 24 | intB << 8;
               intC = src_texel[14];  rowAddr1[0x02] = intB >> 16 | intC << 16;
                                      rowAddr1[0x03] = intC >> 8;

               /* Four pixels (three reads from source) */
               intA = src_texel[15];  rowAddr1[0x10] = intA;
               intB = src_texel[16];  rowAddr1[0x11] = intA >> 24 | intB << 8;
               intC = src_texel[17];  rowAddr1[0x12] = intB >> 16 | intC << 16;
                                      rowAddr1[0x13] = intC >> 8;

               /* Four pixels (three reads from source) */
               intA = src_texel[18];  rowAddr1[0x20] = intA;
               intB = src_texel[19];  rowAddr1[0x21] = intA >> 24 | intB << 8;
               intC = src_texel[20];  rowAddr1[0x22] = intB >> 16 | intC << 16;
                                      rowAddr1[0x23] = intC >> 8;

               /* Four pixels (three reads from source) */
               intA = src_texel[21];  rowAddr1[0x30] = intA;
               intB = src_texel[22];  rowAddr1[0x31] = intA >> 24 | intB << 8;
               intC = src_texel[23];  rowAddr1[0x32] = intB >> 16 | intC << 16;
                                      rowAddr1[0x33] = intC >> 8;
            }
            else
            {
               uint8_t  *src_texel = src_texel_8;

               /* Slower path for edges of image */
               uint32_t   endSubX1 = endSubX < (SUB_TILE_WIDTH - 1) ? endSubX : SUB_TILE_WIDTH - 1;

               for (subX = startSubX; subX <= endSubX1; ++subX)
               {
                  uint32_t r = *src_texel++;
                  uint32_t g = *src_texel++;
                  uint32_t b = *src_texel++;
                  uint32_t *dst_texel = rowAddr0 + XSWIZZLE32(subX);

                  *dst_texel = r | g << 8 | b << 16;
               }

               for (subX = startSubX > SUB_TILE_WIDTH ? startSubX : SUB_TILE_WIDTH; subX <= endSubX; ++subX)
               {
                  uint32_t r = *src_texel++;
                  uint32_t g = *src_texel++;
                  uint32_t b = *src_texel++;
                  uint32_t *dst_texel = rowAddr1 + XSWIZZLE32(subX & 0xf);

                  *dst_texel = r | g << 8 | b << 16;
               }
            }
         }
      }
   }
}

/* khrn_copy_rgb565_to_tf16
 *
 * Copies an array in RGB565 linear format to the equivalent 16-bit Tformat array
 */
static void khrn_copy_rgb565_to_tf16(
   uint8_t   *dst,                             /* address of Tformat image                                             */
   uint32_t  dstWidth,                         /* width of destination in texels                                       */
   uint32_t  dstStartX, uint32_t dstStartY,    /* bottom left of destination region                                    */
   uint32_t  width,     uint32_t height,       /* top right of destination region                                      */
   uint8_t   *src,                             /* address of linear image                                              */
   uint32_t  srcStartX, uint32_t srcStartY,    /* bottom left of source region                                         */
   uint32_t  srcStride)                        /* stride of source image in bytes (i.e. line length including padding) */
{
   static const uint32_t   TILE_XSHIFT    = 6;
   static const uint32_t   TILE_XBITS     = 0x3f;
   static const uint32_t   TILE_YSHIFT    = 5;
   static const uint32_t   TILE_YBITS     = 0x1f;
   static const uint32_t   SUB_TILE_WIDTH = 32;
   static const uint32_t   TEXEL_SIZE     = 2;

   uint32_t   blockY, blockX, subY, subX;

   uint32_t   dstBlockWidth  = ((dstWidth + TILE_XBITS) & ~TILE_XBITS) >> TILE_XSHIFT;  /* Width in 4k tiles */
   uint32_t   dstBlockStride = dstBlockWidth * TILE_SIZE;

   uint32_t   dstEndX    = dstStartX + width  - 1;
   uint32_t   dstEndY    = dstStartY + height - 1;

   uint32_t   startBlockX = dstStartX >> TILE_XSHIFT;
   uint32_t   startBlockY = dstStartY >> TILE_YSHIFT;
   uint32_t   endBlockX   = dstEndX   >> TILE_XSHIFT;
   uint32_t   endBlockY   = dstEndY   >> TILE_YSHIFT;

   int   offX = srcStartX - dstStartX;
   int   offY = srcStartY - dstStartY;

   /* For each 4k tile vertically */
   for (blockY = startBlockY; blockY <= endBlockY; ++blockY)
   {
      uint32_t   startSubY = blockY == startBlockY ? (dstStartY & TILE_YBITS) : 0;
      uint32_t   endSubY   = blockY == endBlockY   ? (dstEndY   & TILE_YBITS) : TILE_YBITS;

      uint32_t   odd = blockY & 1;
      uint32_t   mask = odd ? MAP_ODD : MAP_EVEN;

      /* For each 4k tile horizontally */
      for (blockX = startBlockX; blockX <= endBlockX; ++blockX)
      {
         uint32_t   startSubX = blockX == startBlockX ? (dstStartX & TILE_XBITS) : 0;
         uint32_t   endSubX   = blockX == endBlockX   ? (dstEndX   & TILE_XBITS) : TILE_XBITS;
         bool       fast      = (offX & 1) == 0 && startSubX == 0 && endSubX == TILE_XBITS;

         uint32_t   blockXR   = odd ? dstBlockWidth - blockX - 1 : blockX;

         /* Address of tile */
         uint8_t    *tileAddr  = dst + blockY * dstBlockStride + blockXR * TILE_SIZE;
         uint32_t   Y = (blockY << TILE_YSHIFT) + offY;
         uint32_t   X = (blockX << TILE_XSHIFT) + startSubX + offX;

         /* For each row in the tile */
         for (subY = startSubY; subY <= endSubY; ++subY)
         {
#ifdef FLIP_Y
            uint8_t   *src_texel_8 = src - (Y + subY) * srcStride + X * TEXEL_SIZE;
#else
            uint8_t   *src_texel_8 = src + (Y + subY) * srcStride + X * TEXEL_SIZE;
#endif

            uint32_t offset      = mask >> ((subY & 0x10) >> 1);
            uint8_t  *rowAddr    = tileAddr + YSWIZZLE16(subY);
            uint8_t  *rowAddr0_8 = rowAddr + ((offset & 0x0f) << 8);
            uint8_t  *rowAddr1_8 = rowAddr + ((offset & 0xf0) << 4);
            
            if (fast)
            {
               uint32_t *src_texel = (uint32_t *)src_texel_8;
               uint32_t *rowAddr0  = (uint32_t *)rowAddr0_8;
               uint32_t *rowAddr1  = (uint32_t *)rowAddr1_8;

               /* Fast path for centre of image */
               /* Copy as integers for speed    */
               rowAddr0[0x00] = src_texel[0];   rowAddr0[0x01] = src_texel[1]; 
               rowAddr0[0x02] = src_texel[2];   rowAddr0[0x03] = src_texel[3]; 
               rowAddr0[0x10] = src_texel[4];   rowAddr0[0x11] = src_texel[5]; 
               rowAddr0[0x12] = src_texel[6];   rowAddr0[0x13] = src_texel[7]; 
               rowAddr0[0x20] = src_texel[8];   rowAddr0[0x21] = src_texel[9]; 
               rowAddr0[0x22] = src_texel[10];  rowAddr0[0x23] = src_texel[11];
               rowAddr0[0x30] = src_texel[12];  rowAddr0[0x31] = src_texel[13];
               rowAddr0[0x32] = src_texel[14];  rowAddr0[0x33] = src_texel[15];

               rowAddr1[0x00] = src_texel[16];  rowAddr1[0x01] = src_texel[17];
               rowAddr1[0x02] = src_texel[18];  rowAddr1[0x03] = src_texel[19];
               rowAddr1[0x10] = src_texel[20];  rowAddr1[0x11] = src_texel[21];
               rowAddr1[0x12] = src_texel[22];  rowAddr1[0x13] = src_texel[23];
               rowAddr1[0x20] = src_texel[24];  rowAddr1[0x21] = src_texel[25];
               rowAddr1[0x22] = src_texel[26];  rowAddr1[0x23] = src_texel[27];
               rowAddr1[0x30] = src_texel[28];  rowAddr1[0x31] = src_texel[29];
               rowAddr1[0x32] = src_texel[30];  rowAddr1[0x33] = src_texel[31];
            }
            else
            {
               uint16_t *src_texel = (uint16_t *)src_texel_8;
               uint16_t *rowAddr0  = (uint16_t *)rowAddr0_8;
               uint16_t *rowAddr1  = (uint16_t *)rowAddr1_8;

               /* Slower path for edges of image */
               uint32_t   endSubX1 = endSubX < (SUB_TILE_WIDTH - 1) ? endSubX : SUB_TILE_WIDTH - 1;

               for (subX = startSubX; subX <= endSubX1; ++subX)
               {
                  uint16_t *dst_texel =  rowAddr0 + XSWIZZLE16(subX);
                  *dst_texel = *src_texel;
                  src_texel += 1;
               }

               for (subX = startSubX > SUB_TILE_WIDTH ? startSubX : SUB_TILE_WIDTH; subX <= endSubX; ++subX)
               {
                  uint16_t *dst_texel =  rowAddr1 + XSWIZZLE16(subX);
                  *dst_texel = *src_texel;
                  src_texel += 1;
               }
            }
         }
      }
   }
}

#undef XSWIZZLE8
#undef XSWIZZLE16
#undef XSWIZZLE32
#undef YSWIZZLE8
#undef YSWIZZLE16
#undef YSWIZZLE32

static bool khrn_fast_copy_to_tf(
   pix_format_e pf,
   uint8_t * dst, uint32_t dst_width,
   uint32_t  dst_x, uint32_t dst_y,
   uint32_t  width,     uint32_t height,
   uint8_t * src, uint32_t src_stride,
   uint32_t  src_x, uint32_t src_y)
{
   /* Check for alignment */
   if (((uint32_t)src & 0x3) == 0 && (src_stride & 0x3) == 0)
   {
      switch (pf)
      {
      case ABGR_8888:
      case XBGR_8888:
         khrn_copy_rgba8888_to_tf32(dst, dst_width, dst_x, dst_y, width, height, src, src_x, src_y, src_stride);
         break;

      case BGR_888:
         khrn_copy_rgb888_to_tf32(dst, dst_width, dst_x, dst_y, width, height, src, src_x, src_y, src_stride);
         break;

      case RGB_565:
         /* all the 16bit formats are covered with this function */
         khrn_copy_rgb565_to_tf16(dst, dst_width, dst_x, dst_y, width, height, src, src_x, src_y, src_stride);
         break;
      default :
         return false;
      }

      return true;
   }
   else
      return false;
}


static int khrn_tformat_utile_addr
   (int ut_w, int ut_x, int ut_y, int isLinearTile)
{
   static const uint32_t sub_tile_addr_oddlut[] = { 2, 1, 3, 0 };
   static const uint32_t sub_tile_addr_evenlut[] = { 0, 3, 1, 2 };

   uint32_t wt, xt, yt, xst, yst, xut, yut;
   uint32_t tile_addr, sub_tile_addr, utile_addr;

   if (!isLinearTile)
   {
      /* T-FORMAT */
#ifdef _DEBUG
      /* first check that ut_w and ut_h conform to
      * exact multiples of tiles */
      if (ut_w & 0x7)
         assert(0);  /* ut_w not padded to integer # tiles! */
#endif

      /* find width and height of image in tiles */
      wt = ut_w >> 3;

      /* find address of tile given ut_x,ut_y
      * remembering tiles are 8x8 u-tiles,
      * also remembering scan direction changes
      * for odd and even lines */
      xt = ut_x >> 3;
      yt = ut_y >> 3;

#if 0
      xst = (ut_x >> 2) & 0x1;
      yst = (ut_y >> 2) & 0x1;
#else
      xst = (ut_x >> 2) & 0x1;
      yst = (ut_y >> 1) & 0x2;
#endif

      if (yt & 0x1)
      {
         xt = wt - xt - 1; /* if odd line, reverse direction */
#if 0
         /* now find address of sub-tile within tile (2 bits)
         * remembering scan order is not raster scan, and
         * changes direction given odd/even tile-scan-line. */

         switch (xst + (yst << 1))
         {
         case 0:  sub_tile_addr = 2; break;
         case 1:  sub_tile_addr = 1; break;
         case 2:  sub_tile_addr = 3; break;
         default: sub_tile_addr = 0; break;
         }
#else
         /* now find address of sub-tile within tile (2 bits)
         * remembering scan order is not raster scan, and
         * changes direction given odd/even tile-scan-line. */
         sub_tile_addr = sub_tile_addr_oddlut[ xst | yst ];
#endif
      }
      else
      {
#if 0
         /* now find address of sub-tile within tile (2 bits)
         * remembering scan order is not raster scan, and
         * changes direction given odd/even tile-scan-line. */
         switch (xst + (yst << 1))
         {
         case 0:  sub_tile_addr = 0; break;
         case 1:  sub_tile_addr = 3; break;
         case 2:  sub_tile_addr = 1; break;
         default: sub_tile_addr = 2; break;
         }
#else
         /* now find address of sub-tile within tile (2 bits)
         * remembering scan order is not raster scan, and
         * changes direction given odd/even tile-scan-line. */
         sub_tile_addr = sub_tile_addr_evenlut[ xst | yst ];
#endif
      }
      tile_addr = yt * wt + xt;

      /* now find address of u-tile within sub-tile
      * nb 4x4 u-tiles fit in one sub-tile, and are in
      * raster-scan order. */
      xut = ut_x & 0x3;
      yut = ut_y & 0x3;
      utile_addr = (yut << 2) | xut;

      return utile_addr + (sub_tile_addr << 4) + (tile_addr << 6);
   }
   else
      return ut_y * ut_w + ut_x;
}

static void copy_scissor_regions_to_tf(
   pix_format_e pf,
   uint8_t * dst, uint32_t dst_width, uint32_t dst_stride, uint32_t dst_height,
   uint32_t dst_x, uint32_t dst_y,
   uint32_t width, uint32_t height,
   uint8_t * src, uint32_t src_width, uint32_t src_stride, uint32_t src_height,
   uint32_t src_x, uint32_t src_y)
{
   int32_t begin_y, end_y, dir_y;
   int32_t begin_x, end_x, dir_x;
   int32_t y;

   int32_t isLinearTile = 0;
   switch (pf)
   {
   case ABGR_8888_LT :
   case XBGR_8888_LT :
   case BGR_888_LT   :
   case RGB_565_LT   : isLinearTile = 1; break;
   default           : isLinearTile = 0; break;
   }

   /*
      choose blitting direction to handle overlapping src/dst
   */

   if (dst_y < src_y) {
      begin_y = 0;
      end_y = height;
      dir_y = 1;
   } else {
      begin_y = height - 1;
      end_y = -1;
      dir_y = -1;
   }

   if (dst_x < src_x) {
      begin_x = 0;
      end_x = width;
      dir_x = 1;
   } else {
      begin_x = width - 1;
      end_x = -1;
      dir_x = -1;
   }

   switch (pf)
   {
   case RGB_565:
   case RGB_565_LT:
      {
         uint32_t ut_w;
         const uint32_t log2_utile_height = 2;
         const uint32_t log2_utile_width = 3;

         ut_w = ((uint32_t)(dst_stride << 3) / 16) >> 3;

         for (y = begin_y; y != end_y; y += dir_y)
         {
            uint32_t ut_y;
            int32_t x;
            void *row;

            ut_y = y >> log2_utile_height;
#ifdef FLIP_Y
            row = (uint8_t *)src - (y * src_stride);
#else
            row = (uint8_t *)src + (y * src_stride);
#endif
            for (x = begin_x; x != end_x; x += dir_x)
            {
               int32_t dx, dy;
               uint32_t ut_x;
               void *ut_base;

               ut_x = x >> log2_utile_width;
               ut_base = (uint8_t *)dst +
                  (khrn_tformat_utile_addr(ut_w, ut_x, ut_y, isLinearTile) * 64);
               dx = x & ((1 << log2_utile_width) - 1);
               dy = y & ((1 << log2_utile_height) - 1);
               ((uint16_t *)ut_base)[(dy * 8) + dx] = ((uint16_t *)row)[x];
            }
         }
      }
      break;

   case ABGR_8888:
   case XBGR_8888:
   case ABGR_8888_LT:
   case XBGR_8888_LT:
      {
         uint32_t ut_w;
         const uint32_t log2_utile_height = 2;
         const uint32_t log2_utile_width = 2;

         ut_w = ((uint32_t)(dst_stride << 3) / 32) >> 2;

         for (y = begin_y; y != end_y; y += dir_y)
         {
            uint32_t ut_y;
            int32_t x;
            void *row;

            ut_y = y >> log2_utile_height;
#ifdef FLIP_Y
            row = (uint8_t *)src - (y * src_stride);
#else
            row = (uint8_t *)src + (y * src_stride);
#endif
            for (x = begin_x; x != end_x; x += dir_x)
            {
               int32_t dx, dy;
               uint32_t ut_x;
               void *ut_base;

               ut_x = x >> log2_utile_width;
               ut_base = (uint8_t *)dst +
                  (khrn_tformat_utile_addr(ut_w, ut_x, ut_y, isLinearTile) * 64);
               dx = x & ((1 << log2_utile_width) - 1);
               dy = y & ((1 << log2_utile_height) - 1);
               ((uint32_t *)ut_base)[(dy * 4) + dx] = ((uint32_t *)row)[x];
            }
         }
      }
      break;

   case BGR_888:
   case BGR_888_LT:
      {
         uint32_t ut_w;
         const uint32_t log2_utile_height = 2;
         const uint32_t log2_utile_width = 2;

         ut_w = ((uint32_t)(dst_stride << 3) / 32) >> 2;

         for (y = begin_y; y != end_y; y += dir_y)
         {
            uint32_t ut_y;
            int32_t x;
            void *row;

            ut_y = y >> log2_utile_height;
#ifdef FLIP_Y
            row = (uint8_t *)src - (y * src_stride);
#else
            row = (uint8_t *)src + (y * src_stride);
#endif
            for (x = begin_x; x != end_x; x += dir_x)
            {
               int32_t dx, dy;
               uint32_t ut_x;
               void *ut_base;

               ut_x = x >> log2_utile_width;
               ut_base = (uint8_t *)dst +
                  (khrn_tformat_utile_addr(ut_w, ut_x, ut_y, isLinearTile) * 64);
               dx = x & ((1 << log2_utile_width) - 1);
               dy = y & ((1 << log2_utile_height) - 1);
               ((uint32_t *)ut_base)[(dy * 4) + dx] = ((uint8_t *)row)[3*x+2]<<16 | ((uint8_t *)row)[3*x+1]<<8 | ((uint8_t *)row)[3*x];
            }
         }
      }
      break;
   }
}


static void copy_scissor_regions_to_rso(
   pix_format_e pf,
   uint8_t * dst, uint32_t dst_width, uint32_t dst_stride, uint32_t dst_height,
   uint32_t dst_x, uint32_t dst_y,
   uint32_t width, uint32_t height,
   uint8_t * src, uint32_t src_width, uint32_t src_stride, uint32_t src_height,
   uint32_t src_x, uint32_t src_y)
{
   int32_t begin_y, end_y, dir_y;
   int32_t begin_x, end_x, dir_x;
   int32_t y;

   int32_t isLinearTile = 0;
   switch (pf)
   {
   case ABGR_8888_LT :
   case XBGR_8888_LT :
   case BGR_888_LT   :
   case RGB_565_LT   : isLinearTile = 1; break;
   default           : isLinearTile = 0; break;
   }

   /*
      choose blitting direction to handle overlapping src/dst
   */

   if (dst_y < src_y) {
      begin_y = 0;
      end_y = height;
      dir_y = 1;
   } else {
      begin_y = height - 1;
      end_y = -1;
      dir_y = -1;
   }

   if (dst_x < src_x) {
      begin_x = 0;
      end_x = width;
      dir_x = 1;
   } else {
      begin_x = width - 1;
      end_x = -1;
      dir_x = -1;
   }

   switch (pf)
   {
   case RGB_565:
   case RGB_565_LT:
      {
         uint32_t ut_w;
         const uint32_t log2_utile_height = 2;
         const uint32_t log2_utile_width = 3;

         ut_w = ((uint32_t)(src_stride << 3) / 16) >> 3;

         for (y = begin_y; y != end_y; y += dir_y)
         {
            uint32_t ut_y;
            int32_t x;
            void *row;

            ut_y = y >> log2_utile_height;
#ifdef FLIP_Y
            row = (uint8_t *)dst - (y * dst_stride);
#else
            row = (uint8_t *)dst + (y * dst_stride);
#endif
            for (x = begin_x; x != end_x; x += dir_x)
            {
               int32_t dx, dy;
               uint32_t ut_x;
               void *ut_base;

               ut_x = x >> log2_utile_width;
               ut_base = (uint8_t *)src +
                  (khrn_tformat_utile_addr(ut_w, ut_x, ut_y, isLinearTile) * 64);
               dx = x & ((1 << log2_utile_width) - 1);
               dy = y & ((1 << log2_utile_height) - 1);
               ((uint16_t *)row)[x] = ((uint16_t *)ut_base)[(dy * 8) + dx];
            }
         }
      }
      break;

   case ABGR_8888:
   case XBGR_8888:
   case ABGR_8888_LT:
   case XBGR_8888_LT:
      {
         uint32_t ut_w;
         const uint32_t log2_utile_height = 2;
         const uint32_t log2_utile_width = 2;

         ut_w = ((uint32_t)(src_stride << 3) / 32) >> 2;

         for (y = begin_y; y != end_y; y += dir_y)
         {
            uint32_t ut_y;
            int32_t x;
            void *row;

            ut_y = y >> log2_utile_height;
#ifdef FLIP_Y
            row = (uint8_t *)dst - (y * dst_stride);
#else
            row = (uint8_t *)dst + (y * dst_stride);
#endif
            for (x = begin_x; x != end_x; x += dir_x)
            {
               int32_t dx, dy;
               uint32_t ut_x;
               void *ut_base;

               ut_x = x >> log2_utile_width;
               ut_base = (uint8_t *)src +
                  (khrn_tformat_utile_addr(ut_w, ut_x, ut_y, isLinearTile) * 64);
               dx = x & ((1 << log2_utile_width) - 1);
               dy = y & ((1 << log2_utile_height) - 1);
               ((uint32_t *)row)[x] = ((uint32_t *)ut_base)[(dy * 4) + dx];
            }
         }
      }
      break;

   case BGR_888:
   case BGR_888_LT:
      {
         uint32_t ut_w;
         const uint32_t log2_utile_height = 2;
         const uint32_t log2_utile_width = 2;

         ut_w = ((uint32_t)(src_stride << 3) / 32) >> 2;

         for (y = begin_y; y != end_y; y += dir_y)
         {
            uint32_t ut_y;
            int32_t x;
            void *row;

            ut_y = y >> log2_utile_height;
#ifdef FLIP_Y
            row = (uint8_t *)dst - (y * dst_stride);
#else
            row = (uint8_t *)dst + (y * dst_stride);
#endif
            for (x = begin_x; x != end_x; x += dir_x)
            {
               int32_t dx, dy;
               uint32_t ut_x;
               void *ut_base;

               ut_x = x >> log2_utile_width;
               ut_base = (uint8_t *)src +
                  (khrn_tformat_utile_addr(ut_w, ut_x, ut_y, isLinearTile) * 64);
               dx = x & ((1 << log2_utile_width) - 1);
               dy = y & ((1 << log2_utile_height) - 1);
               ((uint8_t *)row)[3*x+0] = (((uint32_t *)ut_base)[(dy * 4) + dx] >>  0) & 0xFF;
               ((uint8_t *)row)[3*x+1] = (((uint32_t *)ut_base)[(dy * 4) + dx] >>  8) & 0xFF;
               ((uint8_t *)row)[3*x+2] = (((uint32_t *)ut_base)[(dy * 4) + dx] >> 16) & 0xFF;
            }
         }
      }
      break;
   }
}



void tfconvert(
   pix_format_e pf,
   uint8_t * dst, uint32_t dst_width, uint32_t dst_stride, uint32_t dst_height,
   uint32_t dst_x, uint32_t dst_y,
   uint32_t width, uint32_t height,
   uint8_t * src, uint32_t src_width, uint32_t src_stride, uint32_t src_height,
   uint32_t src_x, uint32_t src_y)
{
   uint8_t * src_bottom;
   assert((dst_x + width) <= dst_width);
   assert((dst_y + height) <= dst_height);
   assert((src_x + width) <= src_width);
   assert((src_y + height) <= src_height);

   /* go to the end of the image, we have to keep the textures the correct orientation for android */
#ifdef FLIP_Y
   src_bottom = src + (src_stride * (src_height - 1));
#else
   src_bottom = src;
#endif

   /* Check for fast path special cases
    */
   if (!khrn_fast_copy_to_tf(pf, dst, dst_width, dst_x, dst_y, width, height, src_bottom, src_stride, src_x, src_y))
   {
      /* if the fast path fails, use the slow path */
      copy_scissor_regions_to_tf(pf, dst, dst_width, dst_stride, dst_height, dst_x, dst_y, width, height, src_bottom, src_width, src_stride, src_height,
                           src_x, src_y);
   }
}



void rsoconvert(
   pix_format_e pf,
   uint8_t * dst, uint32_t dst_width, uint32_t dst_stride, uint32_t dst_height,
   uint32_t dst_x, uint32_t dst_y,
   uint32_t width, uint32_t height,
   uint8_t * src, uint32_t src_width, uint32_t src_stride, uint32_t src_height,
   uint32_t src_x, uint32_t src_y)
{
   uint8_t * dst_bottom;
   assert((dst_x + width) <= dst_width);
   assert((dst_y + height) <= dst_height);
   assert((src_x + width) <= src_width);
   assert((src_y + height) <= src_height);

   /* go to the end of the image, we have to keep the textures the correct orientation for android */
#ifdef FLIP_Y
   dst_bottom = dst + (dst_stride * (dst_height - 1));
#else
   dst_bottom = dst;
#endif

   /* if the fast path fails, use the slow path */
   copy_scissor_regions_to_rso(pf, dst_bottom, dst_width, dst_stride, dst_height, dst_x, dst_y, width, height, src, src_width, src_stride, src_height,
                        src_x, src_y);
}


