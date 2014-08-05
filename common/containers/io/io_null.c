/*=============================================================================
Copyright (c) 2010 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers
Module   :  NULL I/O
File     :  $RCSfile: flv_parser.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Implementation of a null i/o module.
=============================================================================*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_uri.h"

VC_CONTAINER_STATUS_T vc_container_io_null_open( VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T );

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_null_close( VC_CONTAINER_IO_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_null_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(buffer);
   VC_CONTAINER_PARAM_UNUSED(size);
   return size;
}

/*****************************************************************************/
static size_t io_null_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(buffer);
   VC_CONTAINER_PARAM_UNUSED(size);
   return size;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_null_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(offset);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_null_open( VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode )
{
   VC_CONTAINER_PARAM_UNUSED(unused);
   VC_CONTAINER_PARAM_UNUSED(mode);

   /* Check the URI */
   if (!vc_uri_scheme(p_ctx->uri_parts) ||
       (strcasecmp(vc_uri_scheme(p_ctx->uri_parts), "null") &&
        strcasecmp(vc_uri_scheme(p_ctx->uri_parts), "null")))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   p_ctx->pf_close = io_null_close;
   p_ctx->pf_read = io_null_read;
   p_ctx->pf_write = io_null_write;
   p_ctx->pf_seek = io_null_seek;
   return VC_CONTAINER_SUCCESS;
}
