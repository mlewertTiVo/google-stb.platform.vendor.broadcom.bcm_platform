/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Network sockets
File     :  $RCSfile: net_sockets_win32.h,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Windows-specific type definitions for network sockets implementation.
=============================================================================*/

#ifndef _NET_SOCKETS_WIN32_H_
#define _NET_SOCKETS_WIN32_H_

#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET SOCKET_T;
typedef int SOCKADDR_LEN_T;
typedef char *SOCKOPT_CAST_T;

#endif   /* _NET_SOCKETS_WIN32_H_ */
