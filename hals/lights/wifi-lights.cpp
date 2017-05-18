/******************************************************************************
 *    (c)2014-2017 Broadcom Corporation
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

#define LOG_TAG "bcm_light"
#include <cutils/log.h>

#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <hardware/hardware.h>
#include <hardware/lights.h>

#define BRCM_LOG 0
#ifndef max
    #define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_haveRedLed = 1;
static int g_haveGreenLed = 1;
static int g_haveBlueLed = 1;
static int g_haveYellowLed = 1;
static int g_havePowerLed = 1;
static struct light_state_t g_wifiLedState;
static struct light_state_t g_powerLedState;


const char *RED_FILE="/sys/class/leds/wifi-red/brightness";
const char *GREEN_FILE="/sys/class/leds/wifi-green/brightness";
const char *BLUE_FILE="/sys/class/leds/wifi-blue/brightness";
const char *YELLOW_FILE="/sys/class/leds/wifi-yellow/brightness";
const char *POWER_FILE="/sys/class/leds/power/brightness";

void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);

    // figure out if we have various colored LEDS
    g_haveRedLed = (access(RED_FILE, W_OK) == 0) ? 1 : 0;
    g_haveGreenLed = (access(GREEN_FILE, W_OK) == 0) ? 1 : 0;
    g_haveBlueLed = (access(BLUE_FILE, W_OK) == 0) ? 1 : 0;
    g_haveYellowLed = (access(YELLOW_FILE, W_OK) == 0) ? 1 : 0;
    g_havePowerLed = (access(POWER_FILE, W_OK) == 0) ? 1 : 0;

    // Set to an unlikely state
    memset(&g_wifiLedState, 0xf, sizeof(g_wifiLedState));
    memset(&g_powerLedState, 0xf, sizeof(g_powerLedState));

}

static int
write_int(char const* path, int value)
{
    int fd;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    }
    return -errno;
}

static int
set_wifi_light_locked(struct light_device_t* ,
        struct light_state_t const* state)
{
    int red, green, blue, yellow;
    unsigned int colorRGB;

    /* Don't disturb if no change */
    if (!memcmp(state, &g_wifiLedState, sizeof(g_wifiLedState)))
        return 0;

    g_wifiLedState = *state;

    colorRGB = state->color;

#if BRCM_LOG
    ALOGD("set_wifi_light_locked colorRGB=%08X\n", colorRGB);
#endif

    red = (colorRGB >> 16) & 0xFF;
    green = (colorRGB >> 8) & 0xFF;
    blue = colorRGB & 0xFF;

    // Android provides red, green, and blue signals, and we will assume we have
    // one or more of red, green, blue, and yellow LEDs. Map the android colors
    // to the available LEDs
    if (g_haveYellowLed) {
        if (red > green) {
            yellow = green;
            red = red - green;
            green = 0;
        } else {
            yellow = red;
            green = green - red;
            red = 0;
        }
    }

    if (!g_haveRedLed || !g_haveYellowLed)
        yellow = red = max(yellow, red);

    if (!g_haveGreenLed || !g_haveBlueLed)
        green = blue = max(green, blue);

    if (g_haveRedLed) {
        write_int(RED_FILE, red);
    }

    if (g_haveGreenLed) {
        write_int(GREEN_FILE, green);
    }

    if (g_haveBlueLed) {
        write_int(BLUE_FILE, blue);
    }

    if (g_haveYellowLed) {
        write_int(YELLOW_FILE, yellow);
    }

    return 0;
}

static int
set_power_light_locked(struct light_device_t* ,
        struct light_state_t const* state)
{
    int red, green, blue, power;
    unsigned int colorRGB;

    /* Don't disturb if no change */
    if (!memcmp(state, &g_powerLedState, sizeof(g_powerLedState)))
        return 0;

    g_powerLedState = *state;

    colorRGB = state->color;

#if BRCM_LOG
    ALOGD("set_power_light_locked colorRGB=%08X\n", colorRGB);
#endif

    red = (colorRGB >> 16) & 0xFF;
    green = (colorRGB >> 8) & 0xFF;
    blue = colorRGB & 0xFF;

    // Android provides red, green, and blue signals, pick the brightest

    power = max(red, green);
    power = max(power, blue);

    if (g_havePowerLed) {
        write_int(POWER_FILE, power);
    }

    return 0;
}

static int
set_light_wifi(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    set_wifi_light_locked(dev, state);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int
set_light_power(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    set_power_light_locked(dev, state);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

/** Close the lights device */
static int
close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_WIFI, name)) {
        set_light = set_light_wifi;
    }
    // Lights API doesn't have a power light defined, so use buttons light ID
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
        set_light = set_light_power;
    }
    else {
        return -EINVAL;
    }

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = (struct light_device_t *)malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = LIGHTS_DEVICE_API_VERSION_2_0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}


static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "bcm_platform lights module",
    .author = "Broadcom Limited",
    .methods = &lights_module_methods,
};
