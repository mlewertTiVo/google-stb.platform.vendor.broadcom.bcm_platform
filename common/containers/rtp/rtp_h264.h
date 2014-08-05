/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers
Module   :  RTP: Real-time Transport Protocol
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Interface for decoding H.264 parameters and payloads transported by RTP.
=============================================================================*/

#ifndef _RTP_H264_H_
#define _RTP_H264_H_

#include "containers/containers.h"
#include "containers/core/containers_list.h"

/** H.264 parameter handler
 *
 * \param p_ctx Container context.
 * \param track Track data.
 * \param params Parameter list.
 * \return Status of decoding the H.264 parameters. */
VC_CONTAINER_STATUS_T h264_parameter_handler(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);

#endif /* _RTP_H264_H_ */
