#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>

#include "nxclient.h"

#define ALIGN	4096
#define MAX_N	(10*1024*1024)		// bytes

#define BKNI_MEMCPY 1


static uint8_t static_a_[(MAX_N+(ALIGN-1)) & ~(ALIGN-1)];
static uint8_t static_b_[(MAX_N+(ALIGN-1)) & ~(ALIGN-1)];

int max_iter = 10;
int unit_size = MAX_N;

int test_memcpy(uint8_t *a_, uint8_t *b_)
{
   struct timeval begin,end;
   register int i;
   double diff;
   unsigned long long bytes;
   char *c_ptr_from, *c_ptr_to;
   uint8_t *a;
   uint8_t *b;
   int iterations, copy_size;

   a = a_;
   b = b_;

   iterations = max_iter;
   copy_size = unit_size;

   fflush(stdout);
   printf("Static Allocation:\na @ 0x%08lX, b @ 0x%08lX\n", (long)a, (long)b);
   printf("%u iterations, each iteration copies %u bytes\n\n", (unsigned)iterations, (unsigned)copy_size);

   // word-aligned
   c_ptr_from = (char *)a;
   c_ptr_to   = (char *)b;
   c_ptr_from += 0;
   c_ptr_to += 0;

   gettimeofday(&begin, NULL);
   for (i = 0; i < iterations; i++) {
#if BKNI_MEMCPY
      BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size);
#else
      memcpy(c_ptr_to, c_ptr_from, copy_size);
#endif
   }

   gettimeofday(&end, NULL);
   bytes = (long long)iterations*(long long)copy_size;
   diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

   printf("Bandwidth (word aligned) = \t");
   printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

   // word+1 aligned
   c_ptr_from = (char *)a;
   c_ptr_to   = (char *)b;
   c_ptr_from += 1;
   c_ptr_to += 1;

   gettimeofday(&begin, NULL);
   for (i = 0; i < iterations; i++) {
#if BKNI_MEMCPY
      BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size-1);
#else
      memcpy(c_ptr_to, c_ptr_from, copy_size-1);
#endif
   }

   gettimeofday(&end, NULL);
   bytes = (long long)iterations*(long long)(copy_size-1);
   diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

   printf("Bandwidth (word+1 aligned) = \t");
   printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

   // word+2 aligned
   c_ptr_from = (char *)a;
   c_ptr_to   = (char *)b;
   c_ptr_from += 2;
   c_ptr_to += 2;

   gettimeofday(&begin, NULL);
   for (i = 0; i < iterations; i++) {
#if BKNI_MEMCPY
      BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size-2);
#else
      memcpy(c_ptr_to, c_ptr_from, copy_size-2);
#endif
   }

   gettimeofday(&end, NULL);
   bytes = (long long)iterations*(long long)(copy_size-2);
   diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

   printf("Bandwidth (word+2 aligned) = \t");
   printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

   // word+3 aligned
   c_ptr_from = (char *)a;
   c_ptr_to   = (char *)b;
   c_ptr_from += 3;
   c_ptr_to += 3;

   gettimeofday(&begin, NULL);
   for (i = 0; i < iterations; i++) {
#if BKNI_MEMCPY
      BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size-3);
#else
      memcpy(c_ptr_to, c_ptr_from, copy_size-3);
#endif
   }

   gettimeofday(&end, NULL);
   bytes = (long long)iterations*(long long)(copy_size-3);
   diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

   printf("Bandwidth (word+3 aligned) = \t");
   printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

   fflush(stdout);

   return 0;
}

int main(int argc, char **argv)
{
   uint8_t *a_ = NULL;
   uint8_t *b_ = NULL;
   int rc;
   NEXUS_MemoryAllocationSettings allocSettings;
   int mode = 0;
   int chunk = 0;
   int arg;

   while ((arg = getopt(argc, argv, "i:m:s:c:")) != -1) {
   switch (arg) {
   case 'i':
      max_iter = (int) strtoul(optarg, NULL, 0);
      break;
   case 'm':
      mode = (int) strtoul(optarg, NULL, 0);
      if (mode < 0 || mode > 3) return -1;
      break;
   case 's':
      unit_size = (int) strtoul(optarg, NULL, 0);
      break;
   case 'c':
      chunk = (int) strtoul(optarg, NULL, 0);
      unit_size = chunk * (1024*1024);
      break;
   default:
      return -1;
   }
   }

   if (unit_size == 0) {
      return -1;
   }
   if (mode == 0 && unit_size > MAX_N) {
      unit_size = MAX_N;
   }

   rc = NxClient_Join(NULL);
   if (rc) return -1;

   if (mode == 0) {
      a_ = static_a_;
      b_ = static_b_;
   } else if (mode == 1) {
      NEXUS_Memory_GetDefaultAllocationSettings(&allocSettings);
      allocSettings.alignment = ALIGN;
      rc = NEXUS_Memory_Allocate(MAX_N, &allocSettings, &a_);
      BDBG_ASSERT(!rc);
      rc = NEXUS_Memory_Allocate(MAX_N, &allocSettings, &b_);
      BDBG_ASSERT(!rc);
   } else if (mode == 2) {
      NEXUS_ClientConfiguration clientConfig;
      NEXUS_Platform_GetClientConfiguration(&clientConfig);
      NEXUS_Memory_GetDefaultAllocationSettings(&allocSettings);
      allocSettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
      allocSettings.alignment = ALIGN;
      rc = NEXUS_Memory_Allocate(MAX_N, &allocSettings, &a_);
      BDBG_ASSERT(!rc);
      rc = NEXUS_Memory_Allocate(MAX_N, &allocSettings, &b_);
      BDBG_ASSERT(!rc);
   } else if (mode == 3) {
      a_ = malloc((MAX_N+(ALIGN-1)) & ~(ALIGN-1));
      b_ = malloc((MAX_N+(ALIGN-1)) & ~(ALIGN-1));
   }

   BDBG_ASSERT(!a_);
   BDBG_ASSERT(!b_);

   memset(a_, 0, (MAX_N + (ALIGN-1)) & ~(ALIGN-1));
   memset(b_, 0, (MAX_N + (ALIGN-1)) & ~(ALIGN-1));
   test_memcpy(a_, b_);

   memset(a_, 0, (MAX_N + (ALIGN-1)) & ~(ALIGN-1));
   memset(b_, 0, (MAX_N + (ALIGN-1)) & ~(ALIGN-1));
   test_memcpy(b_, a_);

   if (mode == 1 || mode == 2) {
      NEXUS_Memory_Free(a_);
      NEXUS_Memory_Free(b_);
   }
   NxClient_Uninit();
   if (mode == 3) {
      free(a_);
      free(b_);
   }

   return 0;
}

