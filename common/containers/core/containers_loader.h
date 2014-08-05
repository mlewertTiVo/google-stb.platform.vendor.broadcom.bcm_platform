/*=============================================================================
Copyright (c) 2010 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers Library
Module   :  Containers Core
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Containers: code for loading container modules.
=============================================================================*/

#ifndef VC_CONTAINERS_LOADER_H
#define VC_CONTAINERS_LOADER_H

/** Find and attempt to load & open reader, 'fileext' is a hint that can be used 
    to speed up loading. */
VC_CONTAINER_STATUS_T vc_container_load_reader(VC_CONTAINER_T *p_ctx, const char *fileext);

/** Find and attempt to load & open writer, 'fileext' is a hint used to help in 
    selecting the appropriate container format. */
VC_CONTAINER_STATUS_T vc_container_load_writer(VC_CONTAINER_T *p_ctx, const char *fileext);

void vc_container_unload(VC_CONTAINER_T *p_ctx);

#endif /* VC_CONTAINERS_LOADER_H */
