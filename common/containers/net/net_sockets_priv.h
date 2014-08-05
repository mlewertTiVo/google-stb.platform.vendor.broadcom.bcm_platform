/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Network sockets
File     :  $RCSfile: net_sockets_priv.h,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Platform-specific private interface for network sockets implementation.
=============================================================================*/

#ifndef _NET_SOCKETS_PRIV_H_
#define _NET_SOCKETS_PRIV_H_

#include "net_sockets.h"

#ifdef WIN32
#include "net_sockets_win32.h"
#else
#include "net_sockets_bsd.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   STREAM_CLIENT = 0,   /**< TCP client */
   STREAM_SERVER,       /**< TCP server */
   DATAGRAM_SENDER,     /**< UDP sender */
   DATAGRAM_RECEIVER    /**< UDP receiver */
} vc_container_net_type_t;


/** Perform implementation-specific per-socket initialization.
 *
 * \return VC_CONTAINER_NET_SUCCESS or one of the error codes on failure. */
vc_container_net_status_t vc_container_net_private_init( void );

/** Perform implementation-specific per-socket deinitialization.
 * This function is always called once for each successful call to socket_private_init(). */
void vc_container_net_private_deinit( void );

/** Return the last error from the socket implementation. */
vc_container_net_status_t vc_container_net_private_last_error( void );

/** Implementation-specific internal socket close.
 *
 * \param sock Internal socket to be closed. */
void vc_container_net_private_close( SOCKET_T sock );

/** Enable or disable socket address reusability.
 *
 * \param sock Internal socket to be closed.
 * \param enable True to enable reusability, false to clear it. */
void vc_container_net_private_set_reusable( SOCKET_T sock, bool enable );

/** Query the maximum datagram size for the socket.
 *
 * \param sock The socket to query.
 * \return The maximum supported datagram size on the socket. */
size_t vc_container_net_private_maximum_datagram_size( SOCKET_T sock );

#ifdef __cplusplus
}
#endif

#endif   /* _NET_SOCKETS_PRIV_H_ */
