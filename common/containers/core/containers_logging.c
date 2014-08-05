#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "containers/containers.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_logging.h"

#ifndef ENABLE_CONTAINERS_STANDALONE
# include "vcos.h"
#endif

#ifdef __ANDROID__
#define LOG_TAG "ContainersCore"
#include <cutils/log.h>
#endif

/* Default verbosity that will be inherited by containers */
static uint32_t default_verbosity_mask = VC_CONTAINER_LOG_ALL;

/* By default log everything that's not associated with a container context */
static uint32_t verbosity_mask = VC_CONTAINER_LOG_ALL;

/* By default use the native logging output */
static bool log_to_stdout = false;

void vc_container_log_set_default_verbosity(uint32_t mask)
{
   default_verbosity_mask = mask;
}

uint32_t vc_container_log_get_default_verbosity(void)
{
   return default_verbosity_mask;
}

void vc_container_log_set_verbosity(VC_CONTAINER_T *ctx, uint32_t mask)
{
   if(!ctx) verbosity_mask = mask;
   else ctx->priv->verbosity = mask;
}

void vc_container_log_set_output(bool native)
{
   log_to_stdout = !native;
}

void vc_container_log(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, ...)
{
   uint32_t verbosity = ctx ? ctx->priv->verbosity : verbosity_mask;
   va_list args;

   // Optimise out the call to vc_container_log_vargs etc. when it won't do anything.
   if(!(type & verbosity)) return;

   va_start( args, format );
   vc_container_log_vargs(ctx, type, format, args);
   va_end( args );
}

void vc_container_log_vargs(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, va_list args)
{
   uint32_t verbosity = ctx ? ctx->priv->verbosity : verbosity_mask;

   // If the verbosity is such that the type doesn't need logging quit now.
   if(!(type & verbosity)) return;

#ifdef __ANDROID__
   if(!log_to_stdout)
   {
      // Default to Android's "verbose" level (doesn't usually come out)
      android_LogPriority logLevel = ANDROID_LOG_VERBOSE;

      // Where type suggest a higher level is required update logLevel.
      // (Usually type contains only 1 bit as set by the LOG_DEBUG, LOG_ERROR or LOG_INFO macros)
      if (type & VC_CONTAINER_LOG_ERROR)
         logLevel = ANDROID_LOG_ERROR;
      else if (type & VC_CONTAINER_LOG_INFO)
         logLevel = ANDROID_LOG_INFO;
      else if (type & VC_CONTAINER_LOG_DEBUG)
         logLevel = ANDROID_LOG_DEBUG;

      // Actually put the message out.
      LOG_PRI_VA(logLevel, LOG_TAG, format, args);
      return;
   }
#endif

#ifndef ENABLE_CONTAINERS_STANDALONE
   if (!log_to_stdout)
   {
      vcos_vlog(format, args);
      return;
   }
#endif

   vprintf(format, args); printf("\n");
   fflush(0);
}
