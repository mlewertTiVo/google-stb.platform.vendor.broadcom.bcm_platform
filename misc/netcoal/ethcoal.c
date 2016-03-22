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
#include <cutils/log.h>
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

#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/sockios.h>

/* _DEF values are per kernel driver default setting, fixed.
 *
 * _VAL values are per tuning for video mode default setting and can
 *  be changed via property override.
 */
#define ETH_COALESCE_TX_FRAMES      "ro.nx.eth.tx_frames"
#define ETH_COALESCE_TX_FRAMES_VAL  (255)
#define ETH_COALESCE_TX_FRAMES_DEF  (1)
#define ETH_COALESCE_RX_FRAMES      "ro.nx.eth.rx_frames"
#define ETH_COALESCE_RX_FRAMES_VAL  (256)
#define ETH_COALESCE_RX_FRAMES_DEF  (1)
#define ETH_COALESCE_RX_USECS       "ro.nx.eth.rx_usecs"
#define ETH_COALESCE_RX_USECS_VAL   (4096)
#define ETH_COALESCE_RX_USECS_DEF   (0)

#define ETH_COALESCE_MODE           "dyn.nx.netcoal.set"
#define ETH_COALESCE_DEFAULT        "default"
#define ETH_COALESCE_USEVAL         "vmode"

int main(int argc, char* argv[])
{
   struct ethtool_coalesce ecoal;
   struct ifreq ifr;
   int fd = -1, ret = 0;
   char value[PROPERTY_VALUE_MAX];
   int force_value = 0;
   int use_default = 0;
   int rc = 0;

   if (argc < 2) {
      ALOGV("an interface must be specified!");
      rc = -EINVAL;
      goto out;
   }
   if (!strlen(argv[1]) || (strlen(argv[1]) > IFNAMSIZ)) {
      ALOGV("a valid interface name must be specified!");
      rc = -EINVAL;
      goto out;
   }

   if (property_get(ETH_COALESCE_MODE, value, NULL)) {
      if (strlen(value)) {
         if (!strncmp(value, ETH_COALESCE_DEFAULT, strlen(ETH_COALESCE_DEFAULT))) {
            use_default = 1;
         } else if (!strncmp(value, ETH_COALESCE_USEVAL, strlen(ETH_COALESCE_USEVAL))) {
            force_value = 1;
         }
      }
   }

   if (!use_default && !force_value) {
      ALOGV("ignoring invalid request.");
      rc = -EINVAL;
      goto out;
   }

   memset(&ifr, 0, sizeof(ifr));

   fd = socket(AF_INET, SOCK_DGRAM, 0);
   if (fd < 0) {
      ALOGW("failed to open socket for coalesce control.");
      rc = -EPERM;
      goto out;
   }

   strcpy(ifr.ifr_name, argv[1]);

   ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
   if (ret < 0) {
      ALOGW("failed to SIOCGIFFLAGS for %s.", ifr.ifr_name);
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
      ALOGW("failed to ETHTOOL_GCOALESCE for %s.", ifr.ifr_name);
      rc = ret;
      goto out;
   }

   ALOGV("get-%s: %s=%d, %s=%d, %s=%d", ifr.ifr_name,
         ETH_COALESCE_TX_FRAMES, ecoal.rx_max_coalesced_frames,
         ETH_COALESCE_RX_FRAMES, ecoal.rx_max_coalesced_frames,
         ETH_COALESCE_RX_USECS, ecoal.rx_coalesce_usecs);

   ecoal.cmd = ETHTOOL_SCOALESCE;
   if (use_default) {
      ecoal.tx_max_coalesced_frames = ETH_COALESCE_TX_FRAMES_DEF;
      ecoal.rx_max_coalesced_frames = ETH_COALESCE_RX_FRAMES_DEF;
      ecoal.rx_coalesce_usecs =       ETH_COALESCE_RX_USECS_DEF;
   } else if (force_value) {
      ecoal.tx_max_coalesced_frames = property_get_int32(ETH_COALESCE_TX_FRAMES, ETH_COALESCE_TX_FRAMES_VAL);
      ecoal.rx_max_coalesced_frames = property_get_int32(ETH_COALESCE_RX_FRAMES, ETH_COALESCE_RX_FRAMES_VAL);
      ecoal.rx_coalesce_usecs =       property_get_int32(ETH_COALESCE_RX_USECS, ETH_COALESCE_RX_USECS_VAL);
   }
   ifr.ifr_data = (void *)&ecoal;
   ret = ioctl(fd, SIOCETHTOOL, &ifr);
   if (ret < 0) {
      ALOGW("failed to ETHTOOL_SCOALESCE for %s.", ifr.ifr_name);
      rc = ret;
      goto out;
   }

   ALOGI("set-%s: %s=%d, %s=%d, %s=%d", ifr.ifr_name,
         ETH_COALESCE_TX_FRAMES, ecoal.rx_max_coalesced_frames,
         ETH_COALESCE_RX_FRAMES, ecoal.rx_max_coalesced_frames,
         ETH_COALESCE_RX_USECS, ecoal.rx_coalesce_usecs);

out:
   if (fd >= 0) {
      close(fd);
   }
   return 0;
}
