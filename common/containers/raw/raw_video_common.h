/*=============================================================================
Copyright (c) 2013 Broadcom Europe Limited.
All rights reserved.

FILE DESCRIPTION
Common definitions between raw video reader and writer.

=============================================================================*/
#ifndef RAW_VIDEO_COMMON_H
#define RAW_VIDEO_COMMON_H

static struct {
   const char *id;
   VC_CONTAINER_FOURCC_T codec;
   unsigned int size_num;
   unsigned int size_den;
} table[] = {
   {"420", VC_CONTAINER_CODEC_I420, 3, 2},
   {0, 0, 0, 0}
};

STATIC_INLINE bool from_yuv4mpeg2(const char *id, VC_CONTAINER_FOURCC_T *codec,
   unsigned int *size_num, unsigned int *size_den)
{
   unsigned int i;
   for (i = 0; table[i].id; i++)
      if (!strcmp(id, table[i].id))
         break;
  if (codec) *codec = table[i].codec;
  if (size_num) *size_num = table[i].size_num;
  if (size_den) *size_den = table[i].size_den;
  return !!table[i].id;
}

STATIC_INLINE bool to_yuv4mpeg2(VC_CONTAINER_FOURCC_T codec, const char **id,
   unsigned int *size_num, unsigned int *size_den)
{
   unsigned int i;
   for (i = 0; table[i].id; i++)
      if (codec == table[i].codec)
         break;
  if (id) *id = table[i].id;
  if (size_num) *size_num = table[i].size_num;
  if (size_den) *size_den = table[i].size_den;
  return !!table[i].id;
}

#endif /* RAW_VIDEO_COMMON_H */

