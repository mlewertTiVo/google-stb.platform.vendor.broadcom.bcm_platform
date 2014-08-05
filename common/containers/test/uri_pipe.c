/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: uri_pipe.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Use I/O module to read data from one URI and write it to another.
=============================================================================*/

#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_io.h"

#include "nb_io.h"

#define MAX_BUFFER_SIZE 2048

int main(int argc, char **argv)
{
   char buffer[MAX_BUFFER_SIZE];
   VC_CONTAINER_IO_T *read_io, *write_io;
   VC_CONTAINER_STATUS_T status;
   size_t received;
   bool ready = true;

   if (argc < 3)
   {
      LOG_INFO(NULL, "Usage:\n%s <read URI> <write URI>\n", argv[0]);
      return 1;
   }

   read_io = vc_container_io_open(argv[1], VC_CONTAINER_IO_MODE_READ, &status);
   if (!read_io)
   {
      LOG_INFO(NULL, "Opening <%s> for read failed: %d\n", argv[1], status);
      return 2;
   }

   write_io = vc_container_io_open(argv[2], VC_CONTAINER_IO_MODE_WRITE, &status);
   if (!write_io)
   {
      vc_container_io_close(read_io);
      LOG_INFO(NULL, "Opening <%s> for write failed: %d\n", argv[2], status);
      return 3;
   }

   nb_set_nonblocking_input(1);

   while (ready)
   {
      size_t total_written = 0;

      received = vc_container_io_read(read_io, buffer, sizeof(buffer));
      while (ready && total_written < received)
      {
         total_written += vc_container_io_write(write_io, buffer + total_written, received - total_written);
         ready &= (write_io->status == VC_CONTAINER_SUCCESS);
      }
      ready &= (read_io->status == VC_CONTAINER_SUCCESS);

      if (nb_char_available())
      {
         char c = nb_get_char();

         switch (c)
         {
         case 'q':
         case 'Q':
         case 0x04:  /* CTRL+D */
         case 0x1A:  /* CTRL+Z */
         case 0x1B:  /* Escape */
            ready = false;
            break;
         default:
            ;/* Do nothing */
         }
      }
   }

   if (read_io->status != VC_CONTAINER_SUCCESS && read_io->status != VC_CONTAINER_ERROR_EOS)
   {
      LOG_INFO(NULL, "Read failed: %d\n", read_io->status);
   }

   if (write_io->status != VC_CONTAINER_SUCCESS)
   {
      LOG_INFO(NULL, "Write failed: %d\n", write_io->status);
   }

   vc_container_io_close(read_io);
   vc_container_io_close(write_io);

   nb_set_nonblocking_input(0);

   return 0;
}
