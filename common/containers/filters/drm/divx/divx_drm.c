/*=============================================================================
Copyright (c) 2010 Broadcom Europe Limited.
All rights reserved.

Project  :  Container filters
Module   :  DIVX DRM
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Implementation of a DIVX DRM filter.
=============================================================================*/
#include <stdlib.h>
#include <string.h>

#include "containers/containers.h"
#include "containers/core/containers_filters.h"

#include "containers/core/containers_private.h"
#include "containers/core/containers_utils.h"

#include "DRMCommon/src/Device/DrmApi.h" /* DIVX DRM API */

/******************************************************************************
Defines.
******************************************************************************/
#define DIVX_DRM_INFO_SIZE 10

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct {
   uint16_t key_index;
   int32_t encrypt_offset;
   int32_t encrypt_length;
} DIVX_DRM_INFO_T;

typedef struct DIVX_DRM_TRACK_STATE_T
{
   uint32_t data_offset;                   /**< Offset to the current frame being 
                                                decrypted */
   uint8_t info_data[DIVX_DRM_INFO_SIZE];
   uint8_t info_data_len;
} DIVX_DRM_TRACK_STATE_T;

typedef struct VC_CONTAINER_FILTER_MODULE_T
{
   DIVX_DRM_TRACK_STATE_T *track_state;
   uint8_t *drm_context;
} VC_CONTAINER_FILTER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T divx_drm_filter_open( VC_CONTAINER_FILTER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T divx_drm_error_to_status_code(drmErrorCodes_t drm_error)
{
   switch (drm_error)
   {
      case DRM_SUCCESS:
         return VC_CONTAINER_SUCCESS;
      case DRM_NOT_AUTHORIZED:
         return VC_CONTAINER_ERROR_DRM_NOT_AUTHORIZED;
      case DRM_RENTAL_EXPIRED:
         return VC_CONTAINER_ERROR_DRM_EXPIRED;
      case DRM_NOT_REGISTERED:
      case DRM_NEVER_REGISTERED:
      case DRM_GENERAL_ERROR:
         return VC_CONTAINER_ERROR_DRM_FAILED;
      default:
         return VC_CONTAINER_ERROR_FAILED;
   }
}

static void divx_drm_unpack_info(uint8_t *buf, DIVX_DRM_INFO_T *drm_info)
{
   /* Unpack data in buffer (little endian byte order) */
   drm_info->key_index = buf[0] | (buf[1] << 8);
   drm_info->encrypt_offset = buf[2] | (buf[3] << 8)|(buf[4] << 16) | (buf[5] << 24);
   drm_info->encrypt_length = buf[6] | (buf[7] << 8)|(buf[8] << 16) | (buf[9] << 24);
}

static void divx_drm_pack_info(uint8_t *buf, DIVX_DRM_INFO_T *drm_info)
{
   /* Pack data into buffer (little endian byte order) */
   buf[0] = drm_info->key_index & 0xff;
   buf[1] = drm_info->key_index >> 8;
   buf[2] = drm_info->encrypt_offset       & 0xff;
   buf[3] = (drm_info->encrypt_offset>>8)  & 0xff;
   buf[4] = (drm_info->encrypt_offset>>16) & 0xff;
   buf[5] = (drm_info->encrypt_offset>>24) & 0xff;
   buf[6] = drm_info->encrypt_length       & 0xff;
   buf[7] = (drm_info->encrypt_length>>8)  & 0xff;
   buf[8] = (drm_info->encrypt_length>>16) & 0xff;
   buf[9] = (drm_info->encrypt_length>>24) & 0xff;
}

static void divx_drm_random_wait(void)
{
/* Note: you should also ensure that the implementation of 'localClock' in
   DRMCommon/src/DeviceLocal/DrmLocal.c has sufficient resolution and is
   not vulnerable to attacks from users. */
#ifdef __VIDEOCORE__
   #include "vcfw/rtos/rtos.h"
   #include "vcfw/drivers/chip/rng.h"
   const RNG_DRIVER_T *driver;
   DRIVER_HANDLE_T handle;
   driver = rng_get_func_table();
   driver->open(NULL, &handle);
   rtos_delay(driver->rand(handle) & 0xFFFF, 1);
   driver->close(handle);
#else
   #warning DIVX DRM: unknown platform, missing a secure random number generator with sufficient amount of entropy
#endif
}

static VC_CONTAINER_STATUS_T divx_drm_initialise( VC_CONTAINER_FILTER_T *p_ctx )
{
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t drm_context_size, drm_strd_size;
   uint8_t *drm_context = NULL;
   uint8_t rental_message, use_limit, use_count;
   uint8_t cgmsa_signal, acptb_signal, digital_protection_signal, ict;
   drmErrorCodes_t drm_error;
   uint8_t *configdata = NULL;
   unsigned int configdata_size = 0, i;

   for(i = 0; i < p_ctx->container->tracks_num; i++)
   {
      configdata = p_ctx->container->tracks[i]->priv->drmdata;
      configdata_size = p_ctx->container->tracks[i]->priv->drmdata_size;
      if (configdata && configdata_size) break;
   }
   
   if (!configdata || !configdata_size)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   
   drm_strd_size = drmGetStrdSize();

   /* If the chunk is smaller than the 'strd' size (+8 bytes for version and header size)
      we are not allowed to continue */
   if(configdata_size < drm_strd_size + 8) 
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   drmInitSystem(NULL, &drm_context_size);

   if ((drm_context = malloc(drm_context_size)) == NULL)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      
   if (drmInitSystem(drm_context, &drm_context_size) != DRM_SUCCESS)
   {
      status = VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
      goto error;
   }

   divx_drm_random_wait();
   if (drmSetRandomSample(drm_context) != DRM_SUCCESS) goto error;
   divx_drm_random_wait();
   if (drmSetRandomSample(drm_context) != DRM_SUCCESS) goto error;
   divx_drm_random_wait();
   if (drmSetRandomSample(drm_context) != DRM_SUCCESS) goto error;
   
   /* Pass config to DRM library (skip version and size) */
   if (drmInitPlayback(drm_context, configdata + 8) != DRM_SUCCESS)
   {
      status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      goto error;
   }

   /* Get rental status */
   drm_error = drmQueryRentalStatus(drm_context, &rental_message, &use_limit, &use_count);
   if (drm_error != DRM_SUCCESS)
   {
      status = divx_drm_error_to_status_code(drm_error);
      goto error;
   }

   /* Propagate rental status to container context */
   p_ctx->container->drm->views_max = use_limit;
   p_ctx->container->drm->views_current = use_count;

   /* FIXME: Although the following information is not useful to us, it should be propagated to 
      media player clients via the Container API */
   drmQueryCgmsa(drm_context, &cgmsa_signal);
   drmQueryAcptb(drm_context, &acptb_signal);
   drmQueryDigitalProtection(drm_context, &digital_protection_signal);
   drmQueryIct(drm_context, &ict);

   if (!rental_message)
   {
      /* Rental status does not need to be displayed, we can commit playback immediately */
      if ((drm_error = drmCommitPlayback(drm_context)) != DRM_SUCCESS)
      {
         status = divx_drm_error_to_status_code(drm_error);
         goto error;
      }
      
      /* In this case, clients do not need to know about DRM */
      p_ctx->container->drm = NULL;
   }

   module->drm_context = drm_context;
   return status;

error:
   if (drm_context) free(drm_context);
   return status;
}

static VC_CONTAINER_STATUS_T divx_drm_filter_decrypt_audio(VC_CONTAINER_FILTER_T *p_ctx, uint8_t *data, uint32_t data_size)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   drmErrorCodes_t drm_error;
   
   /* Audio data can always be thrown at DRM, if it's not encrypted,
      it won't be altered. */
   drm_error = drmDecryptAudio(module->drm_context, data, data_size);
   status = divx_drm_error_to_status_code(drm_error);
   
   return status;
}

static VC_CONTAINER_STATUS_T divx_drm_filter_decrypt_video(VC_CONTAINER_FILTER_T *p_ctx, DIVX_DRM_TRACK_STATE_T *track_state,
   uint8_t *data, uint32_t data_size, uint32_t offs, uint32_t len)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   uint8_t info_data[DIVX_DRM_INFO_SIZE];
   DIVX_DRM_INFO_T info;
   drmErrorCodes_t drm_error;
   
   memcpy(info_data, track_state->info_data, DIVX_DRM_INFO_SIZE);
   divx_drm_unpack_info(info_data, &info);
   info.encrypt_offset = offs;
   info.encrypt_length = len;
   divx_drm_pack_info(info_data, &info);

   drm_error = drmDecryptVideo(module->drm_context, data, data_size, info_data);
   status = divx_drm_error_to_status_code(drm_error);
   
   return status;
}

/*****************************************************************************
Functions exported as part of the Container Filter API
 *****************************************************************************/

static VC_CONTAINER_STATUS_T divx_drm_filter_process( VC_CONTAINER_FILTER_T *p_ctx, 
   VC_CONTAINER_PACKET_T *p_packet )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   VC_CONTAINER_TRACK_T *track = NULL;
   DIVX_DRM_TRACK_STATE_T *track_state = NULL;
   DIVX_DRM_INFO_T info;

   vc_container_assert(p_packet);

   if (!p_packet || !p_packet->data) return VC_CONTAINER_SUCCESS;

   track = p_ctx->container->tracks[p_packet->track];
   track_state = &module->track_state[p_packet->track];
   
   if (track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
   {
      divx_drm_filter_decrypt_audio(p_ctx, p_packet->data, p_packet->size);
   }
   else if (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      if (track_state->data_offset == 0)
      {
         /* Expecting fixed-size header at start of packet, copy it 
            to an internal buffer and strip it from the packet */
         uint32_t len;      
         len = MIN(p_packet->size, DIVX_DRM_INFO_SIZE);
         memcpy(track_state->info_data, p_packet->data, len);
         track_state->info_data_len += len;
         memmove(p_packet->data, p_packet->data + len, p_packet->size - len);
         p_packet->size -= len;
      }
   
      if (track_state->info_data_len < DIVX_DRM_INFO_SIZE)
      {
         /* Can only happen when we're not given enough data */
         vc_container_assert(p_packet->size == 0);
         return VC_CONTAINER_SUCCESS;
      }
         
      divx_drm_unpack_info(track_state->info_data, &info);
   
      if (p_packet->size && info.encrypt_length)
      {
         vc_container_assert(track_state->data_offset <= info.encrypt_offset);
         
         if (track_state->data_offset + p_packet->size >= info.encrypt_offset)
         {
            uint32_t encrypt_offset, encrypt_len;
            encrypt_offset = info.encrypt_offset - track_state->data_offset;
            encrypt_len = MIN(p_packet->size - encrypt_offset, info.encrypt_length);
                 
            status = divx_drm_filter_decrypt_video(p_ctx, track_state, p_packet->data, p_packet->size, 
               encrypt_offset, encrypt_len);
   
            info.encrypt_offset += encrypt_len;
            info.encrypt_length -= encrypt_len;
            divx_drm_pack_info(track_state->info_data, &info);
         }
      }
   
      track_state->data_offset += p_packet->size;
   
      if (p_packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_END)
      {
         track_state->data_offset = 0;
         track_state->info_data_len = 0;
      }
   }
   else
   {
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   p_packet->flags &= ~VC_CONTAINER_PACKET_FLAG_ENCRYPTED;
   
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T divx_drm_filter_control( VC_CONTAINER_FILTER_T *p_ctx, 
   VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   
   switch(operation)
   {
      case VC_CONTAINER_CONTROL_DRM_PLAY:
      {
         status = divx_drm_error_to_status_code(drmCommitPlayback(module->drm_context));
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

/*****************************************************************************/
static VC_CONTAINER_STATUS_T divx_drm_filter_close( VC_CONTAINER_FILTER_T *p_ctx )
{
   VC_CONTAINER_FILTER_MODULE_T *module = p_ctx->module;
   if (module && module->drm_context) free(module->drm_context);
   if (module) free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T divx_drm_filter_open( VC_CONTAINER_FILTER_T *p_ctx )
{
   VC_CONTAINER_FILTER_MODULE_T *module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   unsigned int i;

   /* Quick check to see if we potentially have DRM data */
   for(i = 0; i < p_ctx->container->tracks_num; i++)
   {
      if(p_ctx->container->tracks[i]->priv->drmdata &&
         p_ctx->container->tracks[i]->priv->drmdata_size) break;
   }
   if(i == p_ctx->container->tracks_num) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module) + p_ctx->container->tracks_num * sizeof(DIVX_DRM_TRACK_STATE_T));
   if(!module) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module) + p_ctx->container->tracks_num * sizeof(DIVX_DRM_TRACK_STATE_T));
   p_ctx->module = module;
   module->track_state = (DIVX_DRM_TRACK_STATE_T *)(module + 1);

   if ((status = divx_drm_initialise(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;

   if (p_ctx->container->drm)
      p_ctx->container->drm->format = VC_FOURCC('d','i','v','x');

   p_ctx->pf_close = divx_drm_filter_close;
   p_ctx->pf_process = divx_drm_filter_process;
   p_ctx->pf_control = divx_drm_filter_control;

   return status;

 error:
   divx_drm_filter_close(p_ctx);
   return status;
}


/* XXX FIX THIS PROPERLY XXX
 * 
 * In the interests of expedience, the JIT DRM MMAL component uses this
 * container instead of creating a fresh VLL. Some of the container
 * parsing code in here, which we don't actually need for the JIT case,
 * uses the hardware rng driver, which doesn't exist on big island. 
 * So weak-link a dummy function table here to make it compile.
 */
#if defined(__HIGHC__)
#pragma weak rng_get_func_table dummy_rng_func_table
void * dummy_rng_func_table()
{
   return NULL;
}
#endif



/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak filter_open divx_drm_filter_open
#endif
