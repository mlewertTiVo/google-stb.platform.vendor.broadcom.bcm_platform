/*=============================================================================
Copyright (c) 2010 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  File I/O
File     :  $RCSfile: flv_parser.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Implementation of a file container i/o module.
=============================================================================*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_uri.h"

#ifdef _VIDEOCORE
#include "filesystem/filesys.h"
#endif

typedef struct VC_CONTAINER_IO_MODULE_T
{
   FILE *stream;

} VC_CONTAINER_IO_MODULE_T;

VC_CONTAINER_STATUS_T vc_container_io_file_open( VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T );

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_file_close( VC_CONTAINER_IO_T *p_ctx )
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   fclose(module->stream);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_file_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   size_t ret = fread(buffer, 1, size, p_ctx->module->stream);
   if(ret != size)
   {
      /* Sanity check return value. Some platforms (e.g. Android) can return -1 */
      if( ((int)ret) < 0 ) ret = 0;

      if( feof(p_ctx->module->stream) ) p_ctx->status = VC_CONTAINER_ERROR_EOS;
      else p_ctx->status = VC_CONTAINER_ERROR_FAILED;
   }
   return ret;
}

/*****************************************************************************/
static size_t io_file_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   return fwrite(buffer, 1, size, p_ctx->module->stream);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_file_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int ret;

   //FIXME: large file support
#ifdef _VIDEOCORE
   extern int fseek64(FILE *fp, int64_t offset, int whence);
   ret = fseek64(p_ctx->module->stream, offset, SEEK_SET);
#else
   if (offset > (int64_t)UINT_MAX)
   {
      p_ctx->status = VC_CONTAINER_ERROR_EOS;
      return VC_CONTAINER_ERROR_EOS;
   }
   ret = fseek(p_ctx->module->stream, (long)offset, SEEK_SET);
#endif   
   if(ret)
   {
      if( feof(p_ctx->module->stream) ) status = VC_CONTAINER_ERROR_EOS;
      else status = VC_CONTAINER_ERROR_FAILED;
   }

   p_ctx->status = status;
   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_file_open( VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = 0;
   const char *psz_mode = mode == VC_CONTAINER_IO_MODE_WRITE ? "wb+" : "rb";
   const char *uri = p_ctx->uri;
   FILE *stream = 0;
   VC_CONTAINER_PARAM_UNUSED(unused);

   if(vc_uri_path(p_ctx->uri_parts))
      uri = vc_uri_path(p_ctx->uri_parts);

   stream = fopen(uri, psz_mode);
   if(!stream) { status = VC_CONTAINER_ERROR_URI_NOT_FOUND; goto error; }

   /* Turn off buffering. The container layer will provide its own cache */
   setvbuf(stream, NULL, _IONBF, 0);

   module = malloc( sizeof(*module) );
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));

   p_ctx->module = module;
   module->stream = stream;
   p_ctx->pf_close = io_file_close;
   p_ctx->pf_read = io_file_read;
   p_ctx->pf_write = io_file_write;
   p_ctx->pf_seek = io_file_seek;

   if(mode == VC_CONTAINER_IO_MODE_WRITE)
   {
#ifdef _VIDEOCORE
      p_ctx->max_size = filesys_freespace64(uri);
      if(p_ctx->max_size > (1UL<<31)-1) p_ctx->max_size = (1UL<<31)-1;
#else
      p_ctx->max_size = (1UL<<31)-1; /* For now limit to 2GB */
#endif
   }
   else
   {
      //FIXME: large file support, platform-specific file size
      fseek(p_ctx->module->stream, 0, SEEK_END);
      p_ctx->size = ftell(p_ctx->module->stream);
      fseek(p_ctx->module->stream, 0, SEEK_SET);
   }

   p_ctx->capabilities = VC_CONTAINER_IO_CAPS_NO_CACHING;
   return VC_CONTAINER_SUCCESS;

 error:
   if(stream) fclose(stream);
   return status;
}
