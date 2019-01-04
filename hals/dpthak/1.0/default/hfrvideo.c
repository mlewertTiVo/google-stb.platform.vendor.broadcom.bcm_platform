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
#include <unistd.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "vendor_bcm_props.h"


//#define VERY_VERBOSE_LOGGING
#ifdef VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define HFR_VIDEO_ENABLE_DEF            (1)

#define HFR_VIDEO_MODE_VMODE            (0)
#define HFR_VIDEO_MODE_VMODE_DEF        (1)
#define HFR_VIDEO_MODE_TUNNELED         (2)
#define HFR_VIDEO_MODE_TUNNELED_DEF     (3)
#define HFR_VIDEO_MODE_INVALID          (-1)

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

const int mediadrmserver_prio_map[NICE_LEVELS] = {
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* -20..-11 */
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* -10..-1  */
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

const int mediacodec_prio_map[NICE_LEVELS] = {
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* -20..-11 */
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* -10..-1  */
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

const int hwcbinder_prio_map[NICE_LEVELS] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* -20..-11 */
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* -10..-1  */
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

const int audioserver_prio_map[NICE_LEVELS] = {
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* -20..-11 */
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* -10..-1  */
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*   0..9   */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*  10..19  */
};

struct thread_entity {
   pid_t tid;
   int policy;
   int priority;
   int niceness;
};

enum ElevateMode {
  EMSched,
  EMNiceness
};

struct process_entity {
   const char *name;
   const char *thread_keep_filter;
   const char *thread_ignore_filter;
   pid_t pid;
   enum ElevateMode mode;
   const int *map;
   int niceness;
   int default_niceness;
   struct thread_entity threads[MAX_THREADS];
};

static struct process_entity key_process_list_vmode[] = {
   {
      .name = "system/bin/surfaceflinger",
      .thread_keep_filter = NULL,
      .thread_ignore_filter = "Binder",
      .pid = 0,
      .mode = EMSched,
      .map = surfaceflinger_prio_map,
   },
   {
      .name = "vendor/bin/nxserver",
      .thread_keep_filter = NULL,
      .thread_ignore_filter = "Binder",
      .pid = 0,
      .mode = EMSched,
      .map = nxserver_prio_map,
   },
   {
      .name = "media.codec",
      .thread_keep_filter = "OMX.broadcom.vi",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMSched,
      .map = mediacodec_prio_map,
   },
   {
      .name = "media.codec",
      .thread_keep_filter = "bomx_display",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMSched,
      .map = mediacodec_prio_map,
   },
   {
      .name = "vendor/bin/hwcbinder",
      .thread_keep_filter = NULL,
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMSched,
      .map = hwcbinder_prio_map,
   },
   {
      .name = "nxssd",
      .thread_keep_filter = "nxssd_worker",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
   {
      .name = "com.google.android.exoplayer2.demo",
      .thread_keep_filter = "MediaCodec_loop",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
   {
      .name = "com.netflix.ninja",
      .thread_keep_filter = "MediaCodec_loop",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
   {
      .name = "com.google.android.media.gts",
      .thread_keep_filter = "MediaCodec_loop",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
};
static const int num_key_processes_vmode = sizeof(key_process_list_vmode)/sizeof(key_process_list_vmode[0]);

static struct process_entity key_process_list_tunneled[] = {
   {
      .name = "media.codec",
      .thread_keep_filter = "OMX.broadcom.vi",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMSched,
      .map = mediacodec_prio_map,
   },
   {
      .name = "nxssd",
      .thread_keep_filter = "nxssd_worker",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
   {
      .name = "com.google.android.exoplayer2.demo",
      .thread_keep_filter = "MediaCodec_loop",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
   {
      .name = "com.netflix.ninja",
      .thread_keep_filter = "MediaCodec_loop",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
   {
      .name = "com.google.android.media.gts",
      .thread_keep_filter = "MediaCodec_loop",
      .thread_ignore_filter = NULL,
      .pid = 0,
      .mode = EMNiceness,
      .map = NULL,
      .niceness = -18,
   },
};
static const int num_key_processes_tunneled = sizeof(key_process_list_tunneled)/sizeof(key_process_list_tunneled[0]);

static void clear_key_processes_list (struct process_entity *key_process_list, int num_key_processes)
{
   int i;

   for (i = 0; i < num_key_processes; ++i) {
      key_process_list[i].pid = 0;
      memset(&(key_process_list[i].threads), 0, sizeof(key_process_list[i].threads));
   }
}

static ssize_t get_comm_from_pid (pid_t pid, char *comm, int len)
{
   int fd;
   int rc = 0;
   char comm_file[MAX_NAME_LEN];

   snprintf(comm_file, MAX_NAME_LEN, "/proc/%d/comm", pid);
   fd = open(comm_file, O_RDONLY);
   if (fd < 0)
      return fd;

   comm[0] = '\0';
   rc = read(fd, comm, len);
   if (rc >= 0) {
      if (comm[rc - 1] == '\n')
         rc--;
      comm[rc] = '\0';
   }
   close(fd);
   return rc;
}

static ssize_t get_cmdline_from_pid (pid_t pid, char *cmdline, int len)
{
   int fd;
   int rc = 0;
   char cmdline_file[MAX_NAME_LEN];

   snprintf(cmdline_file, MAX_NAME_LEN, "/proc/%d/cmdline", pid);
   fd = open(cmdline_file, O_RDONLY);
   if (fd < 0) {
      ALOGV("error reading cmdline, pid:%d", pid);
      return fd;
    }

   cmdline[0] = '\0';
   rc = read(fd, cmdline, len);
   if (rc >= 0) {
      if (rc < len) {
         cmdline[rc] = '\0';
      }
      else {
         cmdline[rc-1] = '\0';
      }
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
      ALOGE("Too many threads found for %d", pid);

   return found;
}

static int get_key_process_list (int mode, struct process_entity **list, int *size)
{
   switch (mode) {
       case HFR_VIDEO_MODE_VMODE:
       case HFR_VIDEO_MODE_VMODE_DEF:
       {
           *list = key_process_list_vmode;
           *size = num_key_processes_vmode;
           break;
       }
       case HFR_VIDEO_MODE_TUNNELED:
       case HFR_VIDEO_MODE_TUNNELED_DEF:
       {
           *list = key_process_list_tunneled;
           *size = num_key_processes_tunneled;
           break;
       }
       default:
       {
           ALOGE("Invalid mode %d", mode);
           return -1;
       }
   }

   return 0;
}

/* Look for processes whose priority need to be elevated during video playback */
static int find_key_processes (struct process_entity *key_process_list, int num_key_processes)
{
   DIR *d;
   struct dirent *de;
   pid_t pid;
   char cmdline[MAX_NAME_LEN];
   int i, found = 0;

   d = opendir("/proc");
   if (d == 0)
      return -1;

   while ((found != num_key_processes) && ((de = readdir(d)) != 0)) {
      if (isdigit(de->d_name[0]) &&
          (pid = (pid_t)strtol(de->d_name, NULL, 10)) &&
          (get_cmdline_from_pid(pid, cmdline, sizeof(cmdline)) > 0)) {
         for (i = 0, found = 0; i < num_key_processes; i++) {
            if (key_process_list[i].pid == 0) {
               if (strstr (cmdline, key_process_list[i].name) != NULL) {
                  key_process_list[i].pid = pid;
                  ALOGV("Found %s with pid %d", cmdline, pid);
                  find_threads(pid,
                        key_process_list[i].thread_keep_filter,
                        key_process_list[i].thread_ignore_filter,
                        key_process_list[i].threads);
                  {
                     int j;
                     for (j = 0; (j < MAX_THREADS) && key_process_list[i].threads[j].tid; j++) {
                        ALOGVV("   Thread %d at %d:%d:%d",
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

static void elevate_priority (struct process_entity *key_process_list, int num_key_processes)
{
   int i, j, rc;
   struct sched_param param;

   memset(&param, 0, sizeof(param));
   for (i = 0; i < num_key_processes; i++) {
      const int *map = key_process_list[i].map;
      struct thread_entity *threads = key_process_list[i].threads;

      if (key_process_list[i].pid == 0)
         continue;

      for (j = 0; (j < MAX_THREADS) && threads[j].tid; j++) {
         if (key_process_list[i].mode == EMSched) {
             param.sched_priority = map[nice_to_idx(threads[j].niceness)];

             if (param.sched_priority > 0) {
                if (threads[j].policy != SCHED_OTHER) {
                   ALOGV("%d already at %d:%d:%d", threads[j].tid, threads[j].policy, threads[j].priority, threads[j].niceness);
                   continue;
                }
                rc = sched_setscheduler(threads[j].tid, SCHED_FIFO, &param);
                ALOGV("%d elevated to %d:%d:%d rc=%d,err=%d (%s)",
                   threads[j].tid, SCHED_FIFO, param.sched_priority, threads[j].niceness, rc, errno, strerror(errno));
             }
         } else if (key_process_list[i].mode == EMNiceness) {
             key_process_list[j].default_niceness = threads[j].niceness;
             if (threads[j].niceness == key_process_list[i].niceness) {
                 ALOGV("%d already at nice:%d", threads[j].tid, threads[j].niceness);
                 continue;
             }
             rc = setpriority(PRIO_PROCESS, threads[j].tid, key_process_list[i].niceness);
             ALOGV("%d set to nice:%d,%d rc=%d,err=%d (%s)",
                threads[j].tid, key_process_list[i].niceness, threads[j].niceness, rc, errno, strerror(errno));
         }
      }
   }
}

static void restore_priority (struct process_entity *key_process_list, int num_key_processes)
{
   int i, j, rc;
   struct sched_param param;

   memset(&param, 0, sizeof(param));
   for (i = 0; i < num_key_processes; i++) {
      const int *map = key_process_list[i].map;
      struct thread_entity *threads = key_process_list[i].threads;

      if (key_process_list[i].pid == 0)
         continue;

      for (j = 0; (j < MAX_THREADS) && threads[j].tid; j++) {
         if (key_process_list[i].mode == EMSched) {
             param.sched_priority = map[nice_to_idx(threads[j].niceness)];

             if (param.sched_priority > 0) {
                if (threads[j].policy == SCHED_OTHER) {
                   ALOGV("%d already at %d:%d:%d", threads[j].tid, threads[j].policy, threads[j].priority, threads[j].niceness);
                   continue;
                }
                param.sched_priority = 0;
                sched_setscheduler(threads[j].tid, SCHED_OTHER, &param);
                ALOGV("%d restored to %d:%d:%d", threads[j].tid, SCHED_OTHER, param.sched_priority, threads[j].niceness);
             }
         } else if (key_process_list[i].mode == EMNiceness) {
             if (threads[j].niceness == key_process_list[i].default_niceness) {
                 ALOGV("%d already at nice:%d", threads[j].tid, threads[j].niceness);
                 continue;
             }
             rc = setpriority(PRIO_PROCESS, threads[j].tid, key_process_list[i].default_niceness);
             ALOGV("%d set to nice:%d rc=%d,err=%d (%s)",
                threads[j].tid, threads[j].niceness, rc, errno, strerror(errno));
         }
      }
   }
}

int do_hfrvideo(int mode)
{
   char value[PROPERTY_VALUE_MAX];
   int video_mode = mode;
   int rc = 0;
   int num_key_processes;
   struct process_entity *key_process_list = NULL;

   if (property_get_int32(BCM_RO_HFR_VIDEO_ENABLE, HFR_VIDEO_ENABLE_DEF) != 1) {
      ALOGV("hfrvideo mode not enabled.");
      return 0;
   }

   if (video_mode == HFR_VIDEO_MODE_INVALID) {
      ALOGV("ignoring invalid request.");
      return -EINVAL;
   }

   ALOGV("Video mode %d", video_mode);
   rc = get_key_process_list(video_mode, &key_process_list, &num_key_processes);
   if (rc < 0)
      return rc;

   rc = find_key_processes(key_process_list, num_key_processes);
   if (rc < 0)
      return rc;

   ALOGV("Found %d processes", rc);
   if (rc != num_key_processes) {
      ALOGI("Found %d/%d key processes", rc, num_key_processes);
   }

   switch (video_mode) {
      case HFR_VIDEO_MODE_VMODE:
      case HFR_VIDEO_MODE_TUNNELED:
      {
         /* Elevate priorities */
         elevate_priority(key_process_list, num_key_processes);
         break;
      }
      case HFR_VIDEO_MODE_VMODE_DEF:
      case HFR_VIDEO_MODE_TUNNELED_DEF:
      {
         restore_priority(key_process_list, num_key_processes);
         break;
      }
      default:
      {
          ALOGE("Invalid request");
          return -EINVAL;
      }
   }

   clear_key_processes_list (key_process_list, num_key_processes);
   return 0;
}
