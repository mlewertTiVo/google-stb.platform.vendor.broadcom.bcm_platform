/******************************************************************************
 *    (c) 2016 Broadcom Limited
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 *****************************************************************************/
#define LOG_TAG "hfrvideo"

//#define LOG_NDEBUG 0

#include <ctype.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>


//#define VERY_VERBOSE_LOGGING
#ifdef VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define HFR_VIDEO_ENABLE         "ro.nx.hfrvideo.mode"
#define HFR_VIDEO_ENABLE_DEF     (1)

#define HFR_VIDEO_MODE           "dyn.nx.hfrvideo.set"
#define HFR_VIDEO_DEFAULT        "default"
#define HFR_VIDEO_USEVAL         "vmode"

#define MAX_NAME_LEN 1024
#define MAX_THREADS 100

#define LOWEST_NICE_LEVEL -20
#define HIGHEST_NICE_LEVEL 19
#define NICE_LEVELS (HIGHEST_NICE_LEVEL - LOWEST_NICE_LEVEL + 1)
#define nice_to_idx(niceness) (niceness - LOWEST_NICE_LEVEL)

const int surfaceflinger_prio_map[NICE_LEVELS] = {
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* -20..-11 */
   2, 2, 1, 0, 0, 0, 0, 0, 0, 0, /* -10..-1  */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

const int nxserver_prio_map[NICE_LEVELS] = {
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* -20..-11 */
   2, 2, 1, 0, 0, 0, 0, 0, 0, 0, /* -10..-1  */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

const int mediaserver_prio_map[NICE_LEVELS] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* -20..-11 */
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* -10..-1  */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

struct thread_entity {
   pid_t tid;
   int policy;
   int priority;
   int niceness;
};

struct process_entity {
   const char *name;
   const char *thread_keep_filter;
   const char *thread_ignore_filter;
   pid_t pid;
   const int *map;
   struct thread_entity threads[MAX_THREADS];
};

static struct process_entity key_process_list[] = {
   {
      .name = "surfaceflinger",
      .thread_keep_filter = NULL,
      .thread_ignore_filter = "Binder",
      .pid = 0,
      .map = surfaceflinger_prio_map,
   },
   {
      .name = "nxserver",
      .thread_keep_filter = NULL,
      .thread_ignore_filter = "Binder",
      .pid = 0,
      .map = nxserver_prio_map,
   },
   {
      .name = "mediaserver",
      .thread_keep_filter = "OMX.broadcom.vi",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .map = mediaserver_prio_map,
   }
};
static const int num_key_processes = sizeof(key_process_list)/sizeof(key_process_list[0]);


static ssize_t get_comm_from_pid (pid_t pid, char *comm, int len)
{
   int fd;
   int rc = 0;
   char comm_file[MAX_NAME_LEN];

   snprintf(comm_file, MAX_NAME_LEN, "/proc/%d/comm", pid);
   fd = open(comm_file, O_RDONLY);
   if (fd < 0)
      return fd;
   rc = read(fd, comm, len);
   if (rc >= 0) {
      if (comm[rc - 1] == '\n')
         rc--;
      comm[rc] = '\0';
   }
   close(fd);
   return rc;
}

static int find_threads (pid_t pid, const char *keep, const char *ignore, struct thread_entity *threads)
{
   DIR *d;
   struct dirent *de;
   char proc_dir[MAX_NAME_LEN];
   pid_t tid;
   char comm[MAX_NAME_LEN];
   int found = 0;
   struct sched_param param;

   snprintf(proc_dir, MAX_NAME_LEN, "/proc/%d/task", pid);
   d = opendir(proc_dir);
   if (d == 0) {
      threads[0].tid = pid;
      threads[0].policy = sched_getscheduler(pid);
      sched_getparam(pid, &param);
      threads[0].priority = param.sched_priority;
      threads[0].niceness = getpriority(PRIO_PROCESS, pid);
      return 1;
   }

   while ((found < MAX_THREADS) && ((de = readdir(d)) != 0)) {
      tid = (pid_t)strtol(de->d_name, NULL, 10);
      if (isdigit(de->d_name[0]) &&
          (tid = (pid_t)strtol(de->d_name, NULL, 10)) &&
          (get_comm_from_pid(tid, comm, sizeof(comm)) > 0)) {

         /* Keep only target thread */
         if (keep && (strncmp(comm, keep, strlen(keep)) != 0))
            continue;

         /* Skip all ignored threads */
         if (ignore && (strncmp(comm, ignore, strlen(ignore)) == 0))
            continue;

         threads[found].tid = tid;
         threads[found].policy = sched_getscheduler(tid);
         sched_getparam(tid, &param);
         threads[found].priority = param.sched_priority;
         threads[found].niceness = getpriority(PRIO_PROCESS, tid);

         found++;
      }
   }

   closedir(d);
   if (found == MAX_THREADS)
      ALOGE("Too many threads found for %d\n", pid);

   return found;
}

/* Look for processes whose priority need to be elevated during video playback */
static int find_key_processes (void)
{
   int rc = -1;
   DIR *d;
   struct dirent *de;
   pid_t pid;
   char comm[MAX_NAME_LEN];
   int i, found = 0;

   d = opendir("/proc");
   if (d == 0)
      return -1;

   while ((found != num_key_processes) && ((de = readdir(d)) != 0)) {

      if (isdigit(de->d_name[0]) &&
          (pid = (pid_t)strtol(de->d_name, NULL, 10)) &&
          (get_comm_from_pid(pid, comm, sizeof(comm)) > 0)) {

         for (i = 0, found = 0; i < num_key_processes; i++) {
            if (key_process_list[i].pid == 0) {
               if (strncmp (key_process_list[i].name, comm, strlen(key_process_list[i].name)) == 0) {
                  key_process_list[i].pid = pid;
                  ALOGV("Found %s with pid %d\n", comm, pid);
                  find_threads(pid,
                        key_process_list[i].thread_keep_filter,
                        key_process_list[i].thread_ignore_filter,
                        key_process_list[i].threads);
                  {
                     int j;
                     for (j = 0; (j < MAX_THREADS) && key_process_list[i].threads[j].tid; j++) {
                        ALOGVV("   Thread %d at %d:%d:%d\n",
                              key_process_list[i].threads[j].tid,
                              key_process_list[i].threads[j].policy,
                              key_process_list[i].threads[j].priority,
                              key_process_list[i].threads[j].niceness);
                     }
                  }
                  found++;
               }
            }
            else
               found++;
         }
      }
   }
   closedir(d);
   return found;
}

static void elevate_priority (void)
{
   int i, j;
   struct sched_param param;

   memset(&param, sizeof(param), 0);
   for (i = 0; i < num_key_processes; i++) {
      const int *map = key_process_list[i].map;
      struct thread_entity *threads = key_process_list[i].threads;

      if (key_process_list[i].pid == 0)
         continue;

      for (j = 0; (j < MAX_THREADS) && threads[j].tid; j++) {
         param.sched_priority = map[nice_to_idx(threads[j].niceness)];

         if (param.sched_priority > 0) {
            if (threads[j].policy != SCHED_OTHER) {
               ALOGV("%d already at %d:%d:%d\n", threads[j].tid, threads[j].policy, threads[j].priority, threads[j].niceness);
               continue;
            }
            sched_setscheduler(threads[j].tid, SCHED_FIFO, &param);
            ALOGV("%d elevated to %d:%d:%d\n", threads[j].tid, SCHED_FIFO, param.sched_priority, threads[j].niceness);
         }
      }
   }
}

static void restore_priority (void)
{
   int i, j;
   struct sched_param param;

   memset(&param, sizeof(param), 0);
   for (i = 0; i < num_key_processes; i++) {
      const int *map = key_process_list[i].map;
      struct thread_entity *threads = key_process_list[i].threads;

      if (key_process_list[i].pid == 0)
         continue;

      for (j = 0; (j < MAX_THREADS) && threads[j].tid; j++) {
         param.sched_priority = map[nice_to_idx(threads[j].niceness)];

         if (param.sched_priority > 0) {
            if (threads[j].policy == SCHED_OTHER) {
               ALOGV("%d already at %d:%d:%d\n", threads[j].tid, threads[j].policy, threads[j].priority, threads[j].niceness);
               continue;
            }
            param.sched_priority = 0;
            sched_setscheduler(threads[j].tid, SCHED_OTHER, &param);
            ALOGV("%d restored to %d:%d:%d\n", threads[j].tid, SCHED_OTHER, param.sched_priority, threads[j].niceness);
         }
      }
   }
}

int main (void)
{
   char value[PROPERTY_VALUE_MAX];
   int force_value = 0;
   int use_default = 0;
   int rc = 0;

   if (property_get_int32(HFR_VIDEO_ENABLE, HFR_VIDEO_ENABLE_DEF) != 1) {
      ALOGV("hfrvideo mode not enabled.");
      return 0;
   }

   if (property_get(HFR_VIDEO_MODE, value, NULL)) {
      if (strlen(value)) {
         if (!strncmp(value, HFR_VIDEO_DEFAULT, strlen(HFR_VIDEO_DEFAULT))) {
            use_default = 1;
         } else if (!strncmp(value, HFR_VIDEO_USEVAL, strlen(HFR_VIDEO_USEVAL))) {
            force_value = 1;
         }
      }
   }

   if (!use_default && !force_value) {
      ALOGV("ignoring invalid request.");
      return -EINVAL;
   }

   rc = find_key_processes();
   if (rc < 0)
      return rc;

   ALOGV("Found %d processes\n", rc);
   if (rc != num_key_processes) {
      ALOGE("Only found %d/%d key processes\n", rc, num_key_processes);
   }

   /* Elevate priorities */
   if (force_value)
      elevate_priority();
   else if (use_default)
      restore_priority();

   return 0;
}

