/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Network sockets
File     :  $RCSfile: net_sockets_bsd.h,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
BSD compatible type definitions for network sockets implementation.
=============================================================================*/

#ifndef _NET_SOCKETS_BSD_H_
#define _NET_SOCKETS_BSD_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>

typedef int SOCKET_T;
typedef socklen_t SOCKADDR_LEN_T;
typedef void *SOCKOPT_CAST_T;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#endif   /* _NET_SOCKETS_BSD_H_ */
