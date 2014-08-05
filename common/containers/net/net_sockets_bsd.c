/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Network sockets
File     :  $RCSfile: net_sockets_bsd.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
BSD sockets-compatible functionality for network sockets implementation.
=============================================================================*/

#include <errno.h>
#include <unistd.h>

#include "net_sockets.h"
#include "net_sockets_priv.h"
#include "containers/core/containers_common.h"

/*****************************************************************************/

/** Default maximum datagram size.
 * This is based on the default Ethernet MTU size, less the IP and UDP headers.
 */
#define DEFAULT_MAXIMUM_DATAGRAM_SIZE (1500 - 20 - 8)

/** Maximum socket buffer size to use. */
#define MAXIMUM_BUFFER_SIZE   65536

/*****************************************************************************/
vc_container_net_status_t vc_container_net_private_last_error()
{
   switch (errno)
   {
   case EACCES:               return VC_CONTAINER_NET_ERROR_ACCESS_DENIED;
   case EFAULT:               return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case EINVAL:               return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case EMFILE:               return VC_CONTAINER_NET_ERROR_TOO_BIG;
   case EWOULDBLOCK:          return VC_CONTAINER_NET_ERROR_WOULD_BLOCK;
   case EINPROGRESS:          return VC_CONTAINER_NET_ERROR_IN_PROGRESS;
   case EALREADY:             return VC_CONTAINER_NET_ERROR_IN_PROGRESS;
   case EADDRINUSE:           return VC_CONTAINER_NET_ERROR_IN_USE;
   case EADDRNOTAVAIL:        return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case ENETDOWN:             return VC_CONTAINER_NET_ERROR_NETWORK;
   case ENETUNREACH:          return VC_CONTAINER_NET_ERROR_NETWORK;
   case ENETRESET:            return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ECONNABORTED:         return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ECONNRESET:           return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ENOBUFS:              return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case ENOTCONN:             return VC_CONTAINER_NET_ERROR_NOT_CONNECTED;
   case ESHUTDOWN:            return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ETIMEDOUT:            return VC_CONTAINER_NET_ERROR_TIMED_OUT;
   case ECONNREFUSED:         return VC_CONTAINER_NET_ERROR_CONNECTION_REFUSED;
   case ELOOP:                return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case ENAMETOOLONG:         return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case EHOSTDOWN:            return VC_CONTAINER_NET_ERROR_NETWORK;
   case EHOSTUNREACH:         return VC_CONTAINER_NET_ERROR_NETWORK;
   case EUSERS:               return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case EDQUOT:               return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case ESTALE:               return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   /* All other errors are unexpected, so just map to a general purpose error code. */
   default:
      return VC_CONTAINER_NET_ERROR_GENERAL;
   }
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_private_init()
{
   /* No additional initialization required */
   return VC_CONTAINER_NET_SUCCESS;
}

/*****************************************************************************/
void vc_container_net_private_deinit()
{
   /* No additional deinitialization required */
}

/*****************************************************************************/
void vc_container_net_private_close( SOCKET_T sock )
{
   close(sock);
}

/*****************************************************************************/
void vc_container_net_private_set_reusable( SOCKET_T sock, bool enable )
{
   int opt = enable ? 1 : 0;

   (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
}

/*****************************************************************************/
size_t vc_container_net_private_maximum_datagram_size( SOCKET_T sock )
{
   (void)sock;

   /* No easy way to determine this, just use the default. */
   return DEFAULT_MAXIMUM_DATAGRAM_SIZE;
}
