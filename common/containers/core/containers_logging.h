/*==============================================================================
 Copyright (c) 2010 Broadcom Europe Limited.
 All rights reserved.

 Module   :  vc_container_logging

 $Id: $

 FILE DESCRIPTION
 VC CONTAINERS HELPER FUNCTIONS / MACROS
==============================================================================*/
#ifndef VC_CONTAINERS_LOGGING_H
#define VC_CONTAINERS_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

/** \file containers_logging.h
 * Logging API used by container readers and writers
 */

typedef enum {
   VC_CONTAINER_LOG_ERROR  = 0x01,
   VC_CONTAINER_LOG_INFO   = 0x02,
   VC_CONTAINER_LOG_DEBUG  = 0x04,
   VC_CONTAINER_LOG_FORMAT = 0x08,
   VC_CONTAINER_LOG_ALL = 0xFF
} VC_CONTAINER_LOG_TYPE_T;

void vc_container_log(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, ...);
void vc_container_log_vargs(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, va_list args);
void vc_container_log_set_verbosity(VC_CONTAINER_T *ctx, uint32_t mask);
void vc_container_log_set_default_verbosity(uint32_t mask);
uint32_t vc_container_log_get_default_verbosity(void);
void vc_container_log_set_output(bool native);

#define ENABLE_CONTAINER_LOG_ERROR
#define ENABLE_CONTAINER_LOG_INFO

#ifdef ENABLE_CONTAINER_LOG_DEBUG
# define LOG_DEBUG(ctx, ...) vc_container_log(ctx, VC_CONTAINER_LOG_DEBUG, __VA_ARGS__)
#else
# define LOG_DEBUG(ctx, ...) VC_CONTAINER_PARAM_UNUSED(ctx)
#endif

#ifdef ENABLE_CONTAINER_LOG_ERROR
# define LOG_ERROR(ctx, ...) vc_container_log(ctx, VC_CONTAINER_LOG_ERROR, __VA_ARGS__)
#else
# define LOG_ERROR(ctx, ...) VC_CONTAINER_PARAM_UNUSED(ctx)
#endif

#ifdef ENABLE_CONTAINER_LOG_INFO
# define LOG_INFO(ctx, ...) vc_container_log(ctx, VC_CONTAINER_LOG_INFO, __VA_ARGS__)
#else
# define LOG_INFO(ctx, ...) VC_CONTAINER_PARAM_UNUSED(ctx)
#endif

#ifdef __cplusplus
}
#endif

#endif /* VC_CONTAINERS_LOGGING_H */
