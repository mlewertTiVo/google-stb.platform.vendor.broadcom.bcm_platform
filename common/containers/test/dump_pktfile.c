/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: dump_pktfile.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Dump a .pkt file's contents. Optional second parameter to dump to a file.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PACKET_BUFFER_SIZE    32768

enum {
   SUCCESS = 0,
   SHOWED_USAGE,
   FAILED_TO_OPEN_PKTFILE,
   FAILED_TO_OPEN_OUTPUT_FILE,
   BYTE_ORDER_MARK_MISSING,
   INVALID_BYTE_ORDER_MARK,
   MEMORY_ALLOCATION_FAILURE,
   PACKET_TOO_BIG,
};

/** Native byte order word */
#define NATIVE_BYTE_ORDER  0x50415753U
/** Reverse of native byte order - need to swap bytes around */
#define SWAP_BYTE_ORDER    0x53574150U

static unsigned long reverse_byte_order( unsigned long value )
{
   /* Reverse the order of the bytes in the word */
   return ((value << 24) | ((value & 0xFF00) << 8) | ((value >> 8) & 0xFF00) | (value >> 24));
}

int main(int argc, char **argv)
{
   int status = SUCCESS;
   FILE *pktfile = NULL, *output_file = NULL;
   uint32_t byte_order, packet_size, packet_read;
   char *buffer = NULL;

   if (argc < 2)
   {
      printf("\
Usage:\n\
  %s <pkt file> [<output file>]\n\
<pkt file> is the file to read.\n\
<output file>, if given, will receive the unpacketized data.\n", argv[0]);
      status = SHOWED_USAGE;
      goto end_program;
   }

   pktfile = fopen(argv[1], "rb");
   if (!pktfile)
   {
      printf("Failed to open pkt file <%s> for reading.\n", argv[1]);
      status = FAILED_TO_OPEN_PKTFILE;
      goto end_program;
   }

   if (fread(&byte_order, 1, sizeof(byte_order), pktfile) != sizeof(byte_order))
   {
      printf("Failed to read byte order header from pkt file.\n");
      status = BYTE_ORDER_MARK_MISSING;
      goto end_program;
   }

   if (byte_order != NATIVE_BYTE_ORDER && byte_order != SWAP_BYTE_ORDER)
   {
      printf("Invalid byte order header: 0x%08x (%u)\n", byte_order, byte_order);
      status = INVALID_BYTE_ORDER_MARK;
      goto end_program;
   }

   buffer = (char *)malloc(PACKET_BUFFER_SIZE);
   if (!buffer)
   {
      printf("Memory allocation failed\n");
      status = MEMORY_ALLOCATION_FAILURE;
      goto end_program;
   }

   if (argc > 2)
   {
      output_file = fopen(argv[2], "wb");
      if (!output_file)
      {
         printf("Failed to open <%s> for output.\n", argv[2]);
         status = FAILED_TO_OPEN_OUTPUT_FILE;
         goto end_program;
      }
   }

   while (fread(&packet_size, 1, sizeof(packet_size), pktfile) == sizeof(packet_size))
   {
      if (byte_order == SWAP_BYTE_ORDER)
         packet_size = reverse_byte_order(packet_size);

      if (packet_size >= PACKET_BUFFER_SIZE)
      {
         printf("*** Packet size is bigger than buffer (%u > %u)\n", packet_size, PACKET_BUFFER_SIZE - 1);
         status = PACKET_TOO_BIG;
         goto end_program;
      }

      packet_read = fread(buffer, 1, packet_size, pktfile);

      if (output_file)
      {
         fwrite(buffer, 1, packet_size, output_file);
      } else {
         buffer[packet_size] = '\0';
         printf("%s", buffer);
         getchar();
      }

      if (packet_read != packet_size)
         printf("*** Invalid packet size (expected %u, got %u)\n", packet_size, packet_read);
   }

end_program:
   if (pktfile) fclose(pktfile);
   if (output_file) fclose(output_file);
   if (buffer) free(buffer);
   return -status;
}
