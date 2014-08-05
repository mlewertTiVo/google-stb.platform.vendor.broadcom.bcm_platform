/*=============================================================================
Copyright (c) 2010 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers
Module   :  Binary writer
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Implementation of a Binary writer.
=============================================================================*/
#include <stdlib.h>
#include <string.h>

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Supported extensions
******************************************************************************/
static const char *extensions[] =
{ "mp3", "aac", "adts", "ac3", "ec3", "amr", "awb", "evrc", "dts",
  "m1v", "m2v", "mp4v", "h263", "263", "h264", "264", "mvc",
  "bin", 0
};

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T binary_writer_open( VC_CONTAINER_T * );

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T binary_writer_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T binary_writer_write( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet )
{
   WRITE_BYTES(p_ctx, packet->data, packet->size);
   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T binary_writer_control( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_ES_FORMAT_T *format;
   VC_CONTAINER_TRACK_T *track;
   VC_CONTAINER_STATUS_T status;

   switch(operation)
   {
   case VC_CONTAINER_CONTROL_TRACK_ADD:
      format = (VC_CONTAINER_ES_FORMAT_T *)va_arg( args, VC_CONTAINER_ES_FORMAT_T * );

      /* Allocate and initialise track data */
      if(p_ctx->tracks_num >= 1) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
      p_ctx->tracks[p_ctx->tracks_num] = track = vc_container_allocate_track(p_ctx, 0);
      if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

      if(format->extradata_size)
      {
         status = vc_container_track_allocate_extradata( p_ctx, track, format->extradata_size );
         if(status != VC_CONTAINER_SUCCESS)
         {
            vc_container_free_track(p_ctx, track);
            return status;
         }
         WRITE_BYTES(p_ctx, format->extradata, format->extradata_size);
      }

      vc_container_format_copy(track->format, format, format->extradata_size);
      p_ctx->tracks_num++;
      return VC_CONTAINER_SUCCESS;

   case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      return VC_CONTAINER_SUCCESS;

   default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T binary_writer_open( VC_CONTAINER_T *p_ctx )
{
   const char *extension = vc_uri_path_extension(p_ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   unsigned int i;

   /* Check if the user has specified a container */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   for(i = 0; extensions[i]; i++)
      if(!strcasecmp(extension, extensions[i])) break;
   if(!extensions[i])
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = &module->track;

   p_ctx->priv->pf_close = binary_writer_close;
   p_ctx->priv->pf_write = binary_writer_write;
   p_ctx->priv->pf_control = binary_writer_control;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "binary: error opening stream (%i)", status);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open binary_writer_open
#endif
