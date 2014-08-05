/*=============================================================================
Copyright (c) 2010 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers Library
Module   :  Containers Core
File     :  $RCSfile: flv_parser.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Implementation of utility functions for containers.
=============================================================================*/

#include <stdlib.h>
#include <string.h>

#include "containers/containers.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_writer_utils.h"

// Remove when we've got a proper temp io
#ifdef _VIDEOCORE
#include "filesystem/filesys.h"
#else
#include <stdio.h>
#endif

/*****************************************************************************/
static VC_CONTAINER_STATUS_T vc_container_writer_extraio_create(VC_CONTAINER_T *context, const char *uri,
   VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PARAM_UNUSED(context);

   extraio->io = vc_container_io_open( uri, VC_CONTAINER_IO_MODE_WRITE, &status );
   extraio->refcount = 0;
   extraio->temp = 0;
   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_writer_extraio_create_null(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   return vc_container_writer_extraio_create(context, "null://", extraio);
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_writer_extraio_create_temp(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   unsigned int length = strlen(context->priv->io->uri) + 5;
   char *uri = malloc(length);
   if(!uri) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   snprintf(uri, length, "%s.tmp", context->priv->io->uri);
   status = vc_container_writer_extraio_create(context, uri, extraio);
   free(uri);
   extraio->temp = true;

   if(status == VC_CONTAINER_SUCCESS && !context->priv->tmp_io)
      context->priv->tmp_io = extraio->io;

   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_writer_extraio_delete(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_STATUS_T status;
   char *uri = extraio->temp ? strdup(extraio->io->uri) : 0;

   while(extraio->refcount) vc_container_writer_extraio_disable(context, extraio);
   status = vc_container_io_close( extraio->io );

#ifdef _VIDEOCORE // Remove when we've got a proper temp io
   if(uri) filesys_remove(uri);
#else
   /* coverity[check_return] On failure the worst case is a file or directory is not removed */
   if(uri) remove(uri);
#endif
   if(uri) free(uri);

   if(context->priv->tmp_io == extraio->io)
      context->priv->tmp_io = 0;

   return status;
}

/*****************************************************************************/
int64_t vc_container_writer_extraio_enable(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_IO_T *tmp;

   if(!extraio->refcount)
   {
      vc_container_io_seek(extraio->io, INT64_C(0));
      tmp = context->priv->io;
      context->priv->io = extraio->io;
      extraio->io = tmp;
   }
   return extraio->refcount++;
}

/*****************************************************************************/
int64_t vc_container_writer_extraio_disable(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_IO_T *tmp;

   if(extraio->refcount)
   {
      extraio->refcount--;
      if(!extraio->refcount)
      {
         tmp = context->priv->io;
         context->priv->io = extraio->io;
         extraio->io = tmp;
      }
   }
   return extraio->refcount;
}
