/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container filters
Module   :  HDCP2 DRM
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Implementation of an HDCP2 DRM filter.
=============================================================================*/
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "containers/containers.h"
#include "containers/core/containers_filters.h"

#include "containers/core/containers_private.h"
#include "containers/core/containers_utils.h"
#include "interface/utils/hdcp2_link_manager/hdcp2_link_manager.h"
#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"

/******************************************************************************
Defines.
******************************************************************************/

#define HDCP2_INFO_SIZE 8

/******************************************************************************
Local Types
******************************************************************************/

typedef enum
{
   VC_CONTAINER_CRYPTO_UNSET,
   VC_CONTAINER_DECRYPT,
   VC_CONTAINER_ENCRYPT,
} VC_CONTAINER_CRYPTO_DIR_T;

typedef struct HDCP2_TRACK_STATE_T
{
   uint32_t stream_ctr;
   uint64_t input_ctr;
   bool valid;
   bool encrypt;
} HDCP2_TRACK_STATE_T;

typedef struct VC_CONTAINER_FILTER_MODULE_T
{
   HDCP2_TRACK_STATE_T *track_state;
   VC_CONTAINER_CRYPTO_DIR_T direction;
   uint32_t most_recent_track;
} VC_CONTAINER_FILTER_MODULE_T;

/******************************************************************************
Local Functions
******************************************************************************/

/***************************************************************************//**
NAME
hdcp2_err_to_container_status

SYNOPSIS
Translate hdcp2 errors into container errors

FUNCTION
hdcp2_err      The hdcp error to be translated.

RETURNS
Container error code
********************************************************************************/
static VC_CONTAINER_STATUS_T hdcp2_err_to_container_status (HDCP2_ERROR_T hdcp2_err)
{
   VC_CONTAINER_STATUS_T result;
   switch(hdcp2_err)
   {
   case HDCP2_ERROR_NONE:
      result = VC_CONTAINER_SUCCESS;
      break;
   case HDCP2_ERROR_INVALID_STREAM:
      result = VC_CONTAINER_ERROR_NO_TRACK_AVAILABLE;
      break;
   case HDCP2_ERROR_MISALIGNED:
      result = VC_CONTAINER_ERROR_CORRUPTED;
      break;
   case HDCP2_ERROR_NOT_READY:
      result = VC_CONTAINER_ERROR_NOT_READY;
      break;
   case HDCP2_ERROR_OUT_OF_MEMORY:
      result = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      break;
   case HDCP2_ERROR_CALLBACKS_EXCEEDED: /* FALLTHROUGH */
   case HDCP2_ERROR_MAX_DEVICES_EXCEEDED: /* FALLTHROUGH */
   case HDCP2_ERROR_MAX_DEPTH_EXCEEDED:
      result = VC_CONTAINER_ERROR_LIMIT_REACHED;
      break;
   case HDCP2_ERROR_INVALID_CERT:  /* FALLTHROUGH */
   case HDCP2_ERROR_RSA_ERROR:
      result = VC_CONTAINER_ERROR_DRM_FAILED;
      break;
   case HDCP2_ERROR_RX_DETACHED:  /* FALLTHROUGH */
   case HDCP2_ERROR_H_MISMATCH:  /* FALLTHROUGH */
   case HDCP2_ERROR_L_MISMATCH:  /* FALLTHROUGH */
   case HDCP2_ERROR_V_MISMATCH:  /* FALLTHROUGH */
   case HDCP2_ERROR_SRM_CORRUPTED:  /* FALLTHROUGH */
   case HDCP2_ERROR_ID_REVOKED:  /* FALLTHROUGH */
   case HDCP2_ERROR_SEND_ERROR:  /* FALLTHROUGH */
   case HDCP2_ERROR_RECEIVE_ERROR:  /* FALLTHROUGH */
   case HDCP2_ERROR_RNG_ERROR:  /* FALLTHROUGH */
   case HDCP2_ERROR_TIMER:  /* FALLTHROUGH */
   case HDCP2_ERROR_TIMEOUT:  /* FALLTHROUGH */
   case HDCP2_ERROR_L_TIMEOUT: /* FALLTHROUGH */
   default:
      result = VC_CONTAINER_ERROR_FAILED;
      break;
   }
   return result;
}

/***************************************************************************//**
NAME
hdcp2_initialise

SYNOPSIS
Set the track data for the filter.

FUNCTION
p_ctx       Filter context structure.

RETURNS
Container error code
********************************************************************************/
static VC_CONTAINER_STATUS_T hdcp2_initialise( VC_CONTAINER_FILTER_T *p_ctx )
{
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   HDCP2_TRACK_STATE_T *track;
   unsigned int i;
   bool hdcp2_track_found = 0;

   module->direction = VC_CONTAINER_DECRYPT;

   /* Check the tracks to see if encryption is actually needed. */
   for(i = 0; i < p_ctx->container->tracks_num; i++)
   {
      if(p_ctx->container->tracks[i]->priv->drmdata &&
        (OMX_BRCMDRMENCRYPTIONTYPE)*(p_ctx->container->tracks[i]->priv->drmdata)
             == OMX_DrmEncryptionHdcp2)
      {
         hdcp2_track_found = 1;
      }
   }

   if(!hdcp2_track_found) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allow for padding in the array of track state structures. */
   module->track_state = malloc(sizeof(track[0]) * p_ctx->container->tracks_num); 
   if(!module->track_state) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module->track_state, 0, sizeof(track[0]) * p_ctx->container->tracks_num);

   for(i = 0; i < p_ctx->container->tracks_num; i++)
   {
      module->track_state[i].valid = 0;
   }
   return status;
}

/***************************************************************************//**
NAME
hdcp2_unpack_data

SYNOPSIS
Extracts HDCP2 counters from the chunk header.

FUNCTION
p_buffer    Chunk header buffer
var_size    The size of the data item in bytes

RETURNS
Counter value
********************************************************************************/
static uint64_t hdcp2_unpack_data(uint8_t * p_buffer, int var_size)
{
   int i;
   uint64_t result = 0;

   for(i = 0; i < var_size; i++)
   {
      result += ((uint64_t)p_buffer[i]) << (8 * i);
   }
   return result;
}

/***************************************************************************//**
NAME
hdcp2_filter_process

SYNOPSIS
Apply HDCP2 filtering to packet data.

FUNCTION
p_ctx       Filter context structure
p_packet    data packet

RETURNS
Container error code
********************************************************************************/
static VC_CONTAINER_STATUS_T hdcp2_filter_process( VC_CONTAINER_FILTER_T *p_ctx, 
VC_CONTAINER_PACKET_T *p_packet)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   VC_CONTAINER_TRACK_T *track = NULL;
   HDCP2_TRACK_STATE_T *track_state = NULL;
   uint32_t true_data_len;
   uint32_t len;
   uint32_t track_num;

   vc_container_assert(p_packet);

   if (!p_packet || !p_packet->data) return VC_CONTAINER_SUCCESS;

   track_num = p_packet->track;
   track = p_ctx->container->tracks[track_num];
   track_state = &module->track_state[track_num];
   p_ctx->module->most_recent_track = track_num;
   
   if(module->direction == VC_CONTAINER_DECRYPT)
   {
      /* Expecting fixed-size header at start of each packet. */

      if(*((uint32_t *)p_packet->data) == VC_FOURCC('h','d','c','2'))
      {
         track_state->stream_ctr = hdcp2_unpack_data(&p_packet->data[4], sizeof(track_state->stream_ctr));
         track_state->input_ctr = hdcp2_unpack_data(p_packet->data + 4 + sizeof(track_state->stream_ctr),
         sizeof(track_state->input_ctr));
         len = 4 + sizeof(track_state->stream_ctr) + sizeof(track_state->input_ctr);
         true_data_len = p_packet->size - len;
         memmove(p_packet->data, p_packet->data + len, true_data_len);

         status = hdcp2_err_to_container_status(hdcp2_decrypt_video(p_packet->data, true_data_len, 
                   track_state->stream_ctr, track_state->input_ctr));
         p_packet->size = true_data_len;
      }
      else
      {
         /* Not encrypted. */
      }
   }
   else if (p_ctx->module->track_state[track_num].encrypt && 
          (p_packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME) == VC_CONTAINER_PACKET_FLAG_FRAME)
   {
      status = hdcp2_err_to_container_status(hdcp2_encrypt_video(track_num, p_packet->data, 
      p_packet->size, &track_state->stream_ctr, &track_state->input_ctr));

      if(status == HDCP2_ERROR_NONE)
      {
         p_ctx->module->track_state[track_num].valid = 1;
      }
      else
      {
         /* Make sure we don't send out unencrypted packets. */
         memset(p_packet->data, 0, p_packet->size);
      }
   }
   else
   {
      /* Just return the packet untouched. */
      status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }

   p_packet->flags &= ~VC_CONTAINER_PACKET_FLAG_ENCRYPTED;
   
   return status;
}

/***************************************************************************//**
NAME
hdcp2_filter_control

SYNOPSIS
Allows encryption to be switched on for a track, and enryption parameters queried.

FUNCTION
p_ctx       Filter context structure
operation   Filter command
args        variable list of command arguments

RETURNS
Container error code
********************************************************************************/
static VC_CONTAINER_STATUS_T hdcp2_filter_control( VC_CONTAINER_FILTER_T *p_ctx, 
VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t most_recent_track = p_ctx->module->most_recent_track;

   switch(operation)
   {
   case VC_CONTAINER_CONTROL_GET_DRM_METADATA:
      {
         /* Enough space for the two counters. */
         static uint8_t metadata_buffer[4 /* For FOURCC */ + sizeof(uint32_t) + sizeof(uint64_t)];
         void **pp_metadata;
         uint32_t *p_metadata_length;

         pp_metadata = va_arg(args, void*);
         p_metadata_length = va_arg(args, uint32_t*);

         if(most_recent_track != 0xFFFF && 
               p_ctx->module->track_state[most_recent_track].valid)
         {
            uint32_t encoding = VC_FOURCC('h','d','c','2');
            memcpy(metadata_buffer, &encoding, 4);
            memcpy(&metadata_buffer[4], &(p_ctx->module->track_state[most_recent_track].stream_ctr),
            sizeof(uint32_t));
            memcpy(&metadata_buffer[4] + sizeof(uint32_t), &(p_ctx->module->track_state[most_recent_track].input_ctr),
            sizeof(uint64_t));
            *p_metadata_length = 4 /* FOURCC */ + sizeof(uint32_t) + sizeof(uint64_t);
            *pp_metadata = &metadata_buffer;
            p_ctx->module->track_state[most_recent_track].valid = 0;
         }
         else
         {
            *p_metadata_length = 0;
            *pp_metadata = 0;
         }
      }
      break;

   case VC_CONTAINER_CONTROL_ENCRYPT_TRACK:
      {
         uint32_t tracknum = va_arg(args, uint32_t);
         p_ctx->module->track_state[tracknum].encrypt = 1;
      }
      break;

   default:
      { 
         status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
      }
      break;
   }
   return status;
}

/***************************************************************************//**
NAME
hdcp2_drm_filter_close

SYNOPSIS
Destroys the filter and underlying HDCP2 link and state machine.

FUNCTION
p_ctx       Filter context structure

RETURNS
Container error code
********************************************************************************/
static VC_CONTAINER_STATUS_T hdcp2_drm_filter_close( VC_CONTAINER_FILTER_T *p_ctx )
{
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;

   /* Destroy the HDCP Link Manager first */
   if(module->direction == VC_CONTAINER_DECRYPT)
   {
      hdcp2_client_destroy();
   }
   else
   {
      hdcp2_server_destroy();
   }
   if(module->track_state) 
   {
      free(module->track_state);
   }
   free(module);

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Filter API
*****************************************************************************/

/***************************************************************************//**
NAME
hdcp2_drm_filter_open

SYNOPSIS
DLL entry point: creates the HDCP2 filter and the underlying HDCP2 link and state machine.

FUNCTION
p_ctx       Filter context structure

RETURNS
Container error code
********************************************************************************/
VC_CONTAINER_STATUS_T hdcp2_drm_filter_open( VC_CONTAINER_FILTER_T *p_ctx, VC_CONTAINER_FOURCC_T type )
{
   VC_CONTAINER_FILTER_MODULE_T *module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));
   p_ctx->module = module;
   
   if ((status = hdcp2_initialise(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;

   if (type == VC_FOURCC('h','d','c','2'))
   {
      /* This is an HDCP2 server. */
      p_ctx->module->direction = VC_CONTAINER_ENCRYPT;
      hdcp2_server_create();
   }
   else if(type == VC_FOURCC('u','n','k','n'))
   {
      p_ctx->module->direction = VC_CONTAINER_DECRYPT;
      hdcp2_client_create();
   }
   else
   {
      status = VC_CONTAINER_ERROR_INVALID_ARGUMENT;
      goto error;
   }

   p_ctx->pf_close = hdcp2_drm_filter_close;
   p_ctx->pf_process = hdcp2_filter_process;
   p_ctx->pf_control = hdcp2_filter_control;
   p_ctx->container->drm = NULL;

   return status;

error:
   hdcp2_drm_filter_close(p_ctx);

   return status;
}

/********************************************************************************
Entrypoint function
********************************************************************************/

#ifndef ENABLE_CONTAINERS_STANDALONE
# pragma weak filter_open hdcp2_drm_filter_open
#endif