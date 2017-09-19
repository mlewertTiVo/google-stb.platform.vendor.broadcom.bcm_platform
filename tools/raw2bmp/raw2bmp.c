#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

struct __attribute__ ((__packed__)) bmp_header {
    unsigned short int type;
    unsigned int size;
    unsigned short int reserved1, reserved2;
    unsigned int offset;
};

struct bmp_infoheader {
    unsigned int size;
    int width,height;
    unsigned short int planes;
    unsigned short int bits;
    unsigned int compression;
    unsigned int imagesize;
    int xresolution,yresolution;
    unsigned int ncolours;
    unsigned int importantcolours;
};

#define HEADER_SIZE (sizeof(struct bmp_header) + sizeof(struct bmp_infoheader))

int main(int argc, char **argv) {
   int arg;
   unsigned width, height, bpp;
   char input[1024], output[1024], *p;
   FILE *fi, *fo;
   struct bmp_header header;
   struct bmp_infoheader infoheader;
   unsigned image_size;
   unsigned rownum, rowsize;
   unsigned char *row;

   while ((arg = getopt(argc, argv, "w:h:b:i:")) != -1) {
   switch (arg) {
   case 'w':
      width = (int) strtoul(optarg, NULL, 0);
      break;
   case 'h':
      height = (int) strtoul(optarg, NULL, 0);
      break;
   case 'b':
      bpp = (int) strtoul(optarg, NULL, 0);
      break;
   case 'i':
      sprintf(input, "%s", optarg);
      break;
   default:
      return -1;
   }
   }

   p = strstr(input, ".raw");
   if (p) {
      *p = '\0';
      sprintf(output, "%s.bmp", input);
      *p = '.';
   }
   fi = fopen(input, "rb");
   fo = fopen(output, "wb+");
   if (fi == NULL || fo == NULL) {
      return -1;
   }

   image_size = width * height * (bpp / 8);

   memset(&header, 0, sizeof(header));
   ((unsigned char *)&header.type)[0] = 'B';
   ((unsigned char *)&header.type)[1] = 'M';
   header.size = image_size + HEADER_SIZE;
   header.offset = HEADER_SIZE;
   memset(&infoheader, 0, sizeof(infoheader));
   infoheader.size = sizeof(infoheader);
   infoheader.width = width;
   infoheader.height = height;
   infoheader.planes = 1;
   infoheader.bits = bpp;
   infoheader.imagesize = image_size;

   fwrite(&header, 1, sizeof(header), fo);
   fwrite(&infoheader, 1, sizeof(infoheader), fo);

   rowsize = width * infoheader.bits/8;
   row = malloc(rowsize*sizeof(unsigned char));
   if (row == NULL) {
      return -1;
   }

   for (rownum=0;rownum<height;rownum++) {
      fseek(fi, (height-rownum-1) * width * (bpp/8), SEEK_SET);
      fread(row, 1, rowsize, fi);
      fwrite(row, 1, rowsize, fo);
      if (rowsize % 4) {
         char pad[3] = {0,0,0};
         fwrite(pad, 1, 4 - rowsize % 4, fo);
      }
   }

   free(row);
   fclose(fi);
   fclose(fo);
   return 0;
}
