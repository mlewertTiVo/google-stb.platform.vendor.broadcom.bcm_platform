#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define OOM_ENTRIES 6

#define FOREGROUND_APP_ADJ 0
#define VISIBLE_APP_ADJ 1
#define PERCEPTIBLE_APP_ADJ 2
#define BACKUP_APP_ADJ 3
#define CACHED_APP_MIN_ADJ 9
#define CACHED_APP_MAX_ADJ 15

struct glob_data {
  int totalMemMb;
  int lmkMinFreeKbytesAdjust;
  int lmkMinFreeKbytesAbsolute;
  int lmkExtraFreeKbytesAdjust;
  int lmkExtraFreeKbytesAbsolute;
};

int mOomMinFreeLow[OOM_ENTRIES] = {12288, 18432, 24576, 36864, 43008, 49152};
int mOomMinFreeHigh[OOM_ENTRIES] = {73728, 92160, 110592, 129024, 147456, 184320};
int mOomMinFree[OOM_ENTRIES] = {0,0,0,0,0,0};
int mOomAdj[OOM_ENTRIES] = {FOREGROUND_APP_ADJ, VISIBLE_APP_ADJ, PERCEPTIBLE_APP_ADJ, BACKUP_APP_ADJ, CACHED_APP_MIN_ADJ, CACHED_APP_MAX_ADJ};
struct glob_data glob;

static void updateOomLevels(int displayWidth, int displayHeight) {

   float scaleMem = ((float)(glob.totalMemMb-300))/(700-300);
   int minSize = 480*800;
   int maxSize = 1280*800;
   float scaleDisp = ((float)(displayWidth*displayHeight)-minSize)/(maxSize-minSize);
   float scale = scaleMem > scaleDisp ? scaleMem : scaleDisp;
   if (scale < 0) scale = 0;
   else if (scale > 1) scale = 1;
   int minfree_adj = glob.lmkMinFreeKbytesAdjust;
   int minfree_abs = glob.lmkMinFreeKbytesAbsolute;
   int is64bit = 0;
   int i, low, high;

   for (i=0; i<OOM_ENTRIES; i++) {
        low = mOomMinFreeLow[i];
        high = mOomMinFreeHigh[i];
        mOomMinFree[i] = (int)(low + ((high-low)*scale));
        if (is64bit) {
            mOomMinFree[i] = (3*mOomMinFree[i])/2;
        }
   }

   if (minfree_abs >= 0) {
       for (i=0; i<OOM_ENTRIES; i++) {
           mOomMinFree[i] = (int)((float)minfree_abs * mOomMinFree[i]
                   / mOomMinFree[OOM_ENTRIES - 1]);
       }
   }

   if (minfree_adj != 0) {
       for (i=0; i<OOM_ENTRIES; i++) {
           mOomMinFree[i] += (int)((float)minfree_adj * mOomMinFree[i]
                   / mOomMinFree[OOM_ENTRIES - 1]);
           if (mOomMinFree[i] < 0) {
               mOomMinFree[i] = 0;
           }
       }
   }

   int reserve = displayWidth * displayHeight * 4 * 3 / 1024;
   int reserve_adj = glob.lmkExtraFreeKbytesAdjust;
   int reserve_abs = glob.lmkExtraFreeKbytesAbsolute;

   if (reserve_abs >= 0) {
       reserve = reserve_abs;
   }

   if (reserve_adj != 0) {
       reserve += reserve_adj;
       if (reserve < 0) {
           reserve = 0;
       }
   }

   printf("OOM scores: (reserve %d)\n", reserve);
   printf("\tscore\tfreeAdj\tminFree\n");
   for (i=0; i<OOM_ENTRIES; i++) {
      printf("\t%d\t%d\t%d\n", mOomAdj[i], (mOomMinFree[i]*1024)/PAGE_SIZE, mOomMinFree[i]);
   }
   printf("\n");
}

int main(int argc, char *argv[]) {

   int totalMemMb = 0;
   int dispWidth = 0;
   int dispHeight = 0;

   if (argc != 4) {
      printf("usage: lmkstats <total-mem> <display-width> <display-height>\n");
      exit(1);
   }

   totalMemMb = strtol(argv[1], NULL, 10);
   totalMemMb *= 1024;
   dispWidth = strtol(argv[2], NULL, 10);
   dispHeight = strtol(argv[3], NULL, 10);

   memset(&glob, 0, sizeof(struct glob_data));
   glob.totalMemMb = totalMemMb / (1024*1024);

   glob.lmkMinFreeKbytesAdjust = 0;
   glob.lmkMinFreeKbytesAbsolute = -1; // 81920 fugu.
   glob.lmkExtraFreeKbytesAdjust = 0;
   glob.lmkExtraFreeKbytesAbsolute = -1;

   printf("OOM calcs for total memory: %d, display {%d,%d}\n", glob.totalMemMb, dispWidth, dispHeight);
   updateOomLevels(dispWidth, dispHeight);

   return 0;
}
