#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "nxclient.h"

int main(int argc, char **argv)
{
   int rc;
   int i = 0;

   fflush(stdout);

   while (1) {
      rc = NxClient_Join(NULL);
      if (rc) return -1;
      printf("join-wait-uninit -> iter %d\n", i++);
      fflush(stdout);
      sleep(1);
      NxClient_Uninit();
   }

   return 0;
}

