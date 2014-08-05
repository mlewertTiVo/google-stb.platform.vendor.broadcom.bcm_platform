/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers
Module   :  RTP: Real-time Transport Protocol
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Interface for decoding MPEG-4 parameters and payloads transported by RTP.
=============================================================================*/

#ifndef _RTP_MPEG4_H_
#define _RTP_MPEG4_H_

#include "containers/containers.h"
#include "containers/core/containers_list.h"

/** MPEG-4 parameter handler
 *
 * \param p_ctx Container context.
 * \param track Track data.
 * \param params Parameter list.
 * \return Status of decoding the MPEG-4 parameters. */
VC_CONTAINER_STATUS_T mp4_parameter_handler(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);

#endif /* _RTP_MPEG4_H_ */
