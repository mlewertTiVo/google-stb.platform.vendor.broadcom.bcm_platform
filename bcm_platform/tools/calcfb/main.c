/* Application to walk through the /proc/{pid}/maps file and show free memory blocks */
/* for mmaps. */
#include <stdio.h>
#include <strings.h>

int main(int argc, char *argv[]) {

   FILE *proc_map = NULL;
   char proc_entry[32];
   char line_entry[256];
   char *s, *p;
   unsigned long long blk_start, blk_end;
   unsigned long long last_blk_end = 0;
   unsigned long long size;
   unsigned long long stop_address_32 = 0xc0000000;
   unsigned long long stop_address_64 = 0x7fffffffffff;
   int cnt;

   if (argc != 2) {
      printf("usage: calcfb <pid>\n");
      return 1;
   }

   sprintf(proc_entry,"/proc/%s/maps",argv[1]);
   proc_map = fopen(proc_entry,"r");
   if(proc_map==NULL) {
       printf("Failed to open %s (process id wrong?)\n",proc_entry);
       return 1;
   }
   do {
       s = fgets(line_entry,256,proc_map);
       if(s==line_entry) {
           p = index(line_entry,' ');
           if (p) *p = '\0'; /* Just look at the start and stop addresses on each line */
           cnt = sscanf(line_entry,"%llx-%llx",&blk_start,&blk_end);
           if(cnt!=2) {
               printf("Failed to read line entry, cnt=%d:%s\n",cnt,s);
           } 
           else {
                /* Make sure not to include kernel reserved addresses starting at 0xc0000000 (ARM 32-bits) */
               if(last_blk_end!=0 && blk_start!=last_blk_end && blk_start<stop_address_32) {
                   size =  (blk_start-last_blk_end)/(1024*1024);
                   if(size>0) printf("open map at %llx, size=%lldMb\n",last_blk_end, size);
               }
               last_blk_end = blk_end;
           }
       }
   } while (s==line_entry);
   fclose(proc_map);
   return 0;
}
