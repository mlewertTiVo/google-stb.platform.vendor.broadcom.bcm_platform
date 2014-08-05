/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: datagram_receiver.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Test datagram receiver using network sockets implementation.
=============================================================================*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "containers/net/net_sockets.h"

int main(int argc, char **argv)
{
   VC_CONTAINER_NET_T *sock;
   vc_container_net_status_t status;
   char *buffer;
   size_t buffer_size;
   size_t received;

   if (argc < 2)
   {
      printf("Usage:\n%s <port>\n", argv[0]);
      return 1;
   }

   sock = vc_container_net_open(NULL, argv[1], 0, &status);
   if (!sock)
   {
      printf("vc_container_net_open failed: %d\n", status);
      return 2;
   }

   buffer_size = vc_container_net_maximum_datagram_size(sock);
   buffer = (char *)malloc(buffer_size);
   if (!buffer)
   {
      vc_container_net_close(sock);
      printf("Failure allocating buffer\n");
      return 3;
   }

   while ((received = vc_container_net_read(sock, buffer, buffer_size)) != 0)
   {
      char *ptr = buffer;

      while (received--)
         putchar(*ptr++);
   }

   if (vc_container_net_status(sock) != VC_CONTAINER_NET_SUCCESS)
   {
      printf("vc_container_net_read failed: %d\n", vc_container_net_status(sock));
   }

   vc_container_net_close(sock);

   return 0;
}
