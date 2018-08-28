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
#define LOG_TAG "netcoal"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <linux/kdev_t.h>
#include <sched.h>
#include <sys/resource.h>
#include <cutils/sched_policy.h>
#include <dirent.h>

#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/sockios.h>

#include "vendor_bcm_props.h"

/* _DEF values are per kernel driver default setting, fixed.
 *
 * _VAL values are per tuning for video mode default setting and can
 *  be changed via property override.
 */
#define ETH_COALESCE_TX_FRAMES_VAL  (255)
#define ETH_COALESCE_TX_FRAMES_DEF  (1)
#define ETH_COALESCE_RX_FRAMES_VAL  (256)
#define ETH_COALESCE_RX_FRAMES_DEF  (1)
#define ETH_COALESCE_RX_USECS_VAL   (4096)
#define ETH_COALESCE_RX_USECS_DEF   (0)
#define ETH_IRQ_BALANCE_VAL         (1)

#define ETH_COALESCE_DEFAULT        "default"
#define ETH_COALESCE_USEVAL         "vmode"

#define MAX_LINE_LEN                (1024)

static int set_irq_affinity (int irq, int set)
{
   FILE *file;
   int rc = 0;
   int old_affinity, new_affinity = 0;
   char name[MAX_LINE_LEN];
   char line[MAX_LINE_LEN];
   char value[PROPERTY_VALUE_MAX];
   char *p = NULL;

   rc = snprintf(name, MAX_LINE_LEN, "/proc/irq/%d/smp_affinity", irq);
   if (rc < 0 || rc >= MAX_LINE_LEN)
      return -EINVAL;

   file = fopen(name, "r");
   if (!file)
      return errno;
   fgets (line, sizeof(line), file);
   fclose(file);
   new_affinity = old_affinity = strtol(line, NULL, 16);

   if (property_get(BCM_RO_ETH_IRQ_BALANCE_MODE, value, NULL)) {
      p = strchr(value, ':');
      if (strlen(value) && (p != NULL)) {
         if (set) {
            new_affinity = strtol(p+1, NULL, 16);
         } else {
            *p = '\0';
            new_affinity = strtol(value, NULL, 16);
         }
      }
   }

   if (new_affinity != old_affinity) {
      file = fopen(name, "w");
      if (!file) {
         ALOGE("Failed to open affinity file (%s): %d (%s)", name, errno, strerror(errno));
         return errno;
      }
      fprintf(file, "%x", new_affinity);
      ALOGI("irq %d affinity: 0x%x->0x%x", irq, old_affinity, new_affinity);
      fclose(file);
   }
   return 0;
}

static int set_eth_irq_affinity (char *ifname, int set)
{
   /* find interrupts */
   FILE *file;
   int rc = 0;
   char line[MAX_LINE_LEN];
   int ifname_len = strlen(ifname);
   int line_len;
   int irq;

   file = fopen("/proc/interrupts", "r");
   if (!file) {
      ALOGE("Failed to open /proc/interrupts: %d (%s)", errno, strerror(errno));
      return errno;
   }

   /* skip the first line of header */
   fgets (line, sizeof(line), file);
   while (fgets (line, sizeof(line), file)) {
      line[MAX_LINE_LEN - 1] = '\0';
      line_len = strlen(line);
      irq = strtol(line, NULL, 10);
      if (irq > 255)
         break;

      if (line[line_len - 1] == '\n') {
         line[line_len - 1] = '\0';
         line_len--;
      }
      if (line_len < ifname_len)
         continue;
      if (strncmp(ifname, &line[line_len - ifname_len], ifname_len) != 0)
         continue;

      /* Found one */
      rc = set_irq_affinity(irq, set);
      if (rc < 0)
         break;
   }

   fclose(file);
   return rc;
}

static int filter_dots(const struct dirent *d) {
   return (strcmp(d->d_name, "..") && strcmp(d->d_name, "."));
}

int do_ethcoal(const char* mode) {
   struct ethtool_coalesce ecoal;
   struct ifreq ifr;
   int fd = -1, ret = 0;
   char value[PROPERTY_VALUE_MAX];
   int force_value = 0;
   int use_default = 0;
   int rc = 0;
   int n;
   struct dirent **d = NULL;

   if (!mode || !strlen(mode)) {
      rc = -EINVAL;
      goto out;
   }

   if (!strncmp(mode, ETH_COALESCE_DEFAULT, strlen(ETH_COALESCE_DEFAULT))) {
      use_default = 1;
   } else if (!strncmp(mode, ETH_COALESCE_USEVAL, strlen(ETH_COALESCE_USEVAL))) {
      force_value = 1;
   }

   if (!use_default && !force_value) {
      ALOGV("ignoring invalid request.");
      rc = -EINVAL;
      goto out;
   }

   memset(&ifr, 0, sizeof(ifr));
   n = scandir("/sys/class/net", &d, filter_dots, alphasort);
   if (n < 0) {
      ALOGE("failed scanning available interfaces [%s]", strerror(errno));
      rc = -errno;
      goto out;
   } else {
      for (int i = 0; i < n; i++) {
         if (!strcmp(d[i]->d_name, "eth0") || !strcmp(d[i]->d_name, "gphy")) {
            strcpy(ifr.ifr_name, d[i]->d_name);
            ALOGV("using interface '%s'", ifr.ifr_name);
         }
         free(d[i]);
      }
      free(d);
   }

   // first, set the irq affinity for the corresponding ethernet one.
   //
   if (property_get_int32(BCM_RO_ETH_IRQ_BALANCE, ETH_IRQ_BALANCE_VAL)) {
      set_eth_irq_affinity(ifr.ifr_name, force_value);
   }

   // second, attempt to set the coalescence for the ethernet interface, this
   // may fail due to socket access control and exit early if so.
   //
   // note we only assume ipv4 here.
   //
   fd = socket(AF_INET, SOCK_DGRAM|SOCK_CLOEXEC, 0);
   if (fd < 0) {
      ALOGW("failed to open socket for coalesce control: %d (%s).", errno, strerror(errno));
      rc = -EPERM;
      goto out;
   }

   ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
   if (ret < 0) {
      ALOGW("failed to SIOCGIFFLAGS for %s: %d (%s).", ifr.ifr_name, errno, strerror(errno));
      rc = ret;
      goto out;
   }

   if (!(ifr.ifr_flags & IFF_UP)) {
      ALOGV("interface %s DOWN - skipping coalesce configuration.", ifr.ifr_name);
      rc = -EINVAL;
      goto out;
   }

   ecoal.cmd = ETHTOOL_GCOALESCE;
   ifr.ifr_data = (void *)&ecoal;
   ret = ioctl(fd, SIOCETHTOOL, &ifr);
   if (ret < 0) {
      ALOGW("failed to ETHTOOL_GCOALESCE for %s: %d (%s).", ifr.ifr_name, errno, strerror(errno));
      rc = ret;
      goto out;
   }

   ALOGV("get-%s: %s=%d, %s=%d, %s=%d", ifr.ifr_name,
         BCM_RO_ETH_COALESCE_TX_FRAMES, ecoal.rx_max_coalesced_frames,
         BCM_RO_ETH_COALESCE_RX_FRAMES, ecoal.rx_max_coalesced_frames,
         BCM_RO_ETH_COALESCE_RX_USECS, ecoal.rx_coalesce_usecs);

   ecoal.cmd = ETHTOOL_SCOALESCE;
   if (use_default) {
      ecoal.tx_max_coalesced_frames = ETH_COALESCE_TX_FRAMES_DEF;
      ecoal.rx_max_coalesced_frames = ETH_COALESCE_RX_FRAMES_DEF;
      ecoal.rx_coalesce_usecs =       ETH_COALESCE_RX_USECS_DEF;
   } else if (force_value) {
      ecoal.tx_max_coalesced_frames = property_get_int32(BCM_RO_ETH_COALESCE_TX_FRAMES, ETH_COALESCE_TX_FRAMES_VAL);
      ecoal.rx_max_coalesced_frames = property_get_int32(BCM_RO_ETH_COALESCE_RX_FRAMES, ETH_COALESCE_RX_FRAMES_VAL);
      ecoal.rx_coalesce_usecs =       property_get_int32(BCM_RO_ETH_COALESCE_RX_USECS, ETH_COALESCE_RX_USECS_VAL);
   }
   ifr.ifr_data = (void *)&ecoal;
   ret = ioctl(fd, SIOCETHTOOL, &ifr);
   if (ret < 0) {
      ALOGW("failed to ETHTOOL_SCOALESCE for %s: %d (%s).", ifr.ifr_name, errno, strerror(errno));
      rc = ret;
      goto out;
   }

   ALOGI("set-%s: %s=%d, %s=%d, %s=%d", ifr.ifr_name,
         BCM_RO_ETH_COALESCE_TX_FRAMES, ecoal.rx_max_coalesced_frames,
         BCM_RO_ETH_COALESCE_RX_FRAMES, ecoal.rx_max_coalesced_frames,
         BCM_RO_ETH_COALESCE_RX_USECS, ecoal.rx_coalesce_usecs);

out:
   if (fd >= 0) {
      close(fd);
   }
   return rc;
}
