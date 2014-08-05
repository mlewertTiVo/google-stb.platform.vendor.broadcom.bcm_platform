/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: datagram_sender.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Test datagram sender using network sockets implementation.
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

   if (argc < 3)
   {
      printf("Usage:\n%s <address> <port>\n", argv[0]);
      return 1;
   }

   sock = vc_container_net_open(argv[1], argv[2], 0, &status);
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

   printf("Don't enter more than %d characters in one line!\n", (int)buffer_size);

   while (fgets(buffer, buffer_size, stdin))
   {
      if (!vc_container_net_write(sock, buffer, strlen(buffer)))
      {
         printf("vc_container_net_write failed: %d\n", vc_container_net_status(sock));
         break;
      }
   }

   vc_container_net_close(sock);

   return 0;
}
