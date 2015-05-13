/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
 *****************************************************************************/
#include "nexus_platform_client.h"
#include "nxclient.h"
#include "namevalue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "bstd.h"
#include "bkni.h"

BDBG_MODULE(setdisplay);

static void print_usage(void)
{
    char formatlist[1024] = "";
    unsigned n = 0;
    unsigned i;
#if NEXUS_HAS_HDMI_OUTPUT
    {
    NEXUS_HdmiOutputHandle hdmiOutput;
    hdmiOutput = NEXUS_HdmiOutput_Open(NEXUS_ALIAS_ID + 0, NULL);
    if (hdmiOutput) {
        NEXUS_HdmiOutputStatus status;
        if (!NEXUS_HdmiOutput_GetStatus(hdmiOutput, &status) && status.connected) {
            for (i=0;i<NEXUS_VideoFormat_eMax;i++) {
                if (status.videoFormatSupported[i]) {
                    const char *s = lookup_name(g_videoFormatStrs, i);
                    if (s) {
                        n += snprintf(&formatlist[n], sizeof(formatlist)-n, "%s%s", n?",":"", s);
                        if (n >= sizeof(formatlist)) break;
                    }
                }
            }
        }
        NEXUS_HdmiOutput_Close(hdmiOutput);
    }
    }
#endif
    if (!formatlist[0]) {
        for (i=0;g_videoFormatStrs[i].name;i++) {
            n += snprintf(&formatlist[n], sizeof(formatlist)-n, "%s%s", n?",":"", g_videoFormatStrs[i].name);
            if (n >= sizeof(formatlist)) break;
        }
    }
    n += snprintf(&formatlist[n], sizeof(formatlist)-n, "\n");

    printf(
        "Usage: nexus.client setdisplay OPTIONS\n"
        "  -format %s",
        formatlist
        );
    printf(
        "  -component {on|off}\n"
        "  -composite {on|off}\n"
        "  -hdmi {on|off}\n"
        "  -hdcp {m[andatory]|o[ptional]}\n"
        "  -3d {on|lr|ou|off}        enable stereoscopic (3D) video\n"
        "  -color CONTRAST,SATURATION,HUE,BRIGHTNESS   graphics color. values range between -32767 and 32768.\n"
        "  -backgroundColor          ARGB888 hex value for graphics background\n"
        );
    print_list_option("ar",g_displayAspectRatioStrs);
    printf(
        "  -sar X,Y                  Set sample aspect ratio with X:Y dimensions\n"
        "  -sd                       Apply following -format, -ar, -sar and -backgroundColor options to SD display\n"
        "  -wait                     Wait for status changes\n"
        );
    print_list_option("colorSpace",g_colorSpaceStrs);
    printf(
        "  -colorDepth {0|8|10}      0 is auto\n"
        "  -macrovision              Enable macrovision (requires special build)\n"
        );
}

static bool parse_boolean(char *s)
{
    if (*s) {
        if (*s == '1' || *s == 'y' || *s == 'Y' || *s == 'T' || *s == 't') return true;
        if (!strcasecmp(s, "on")) return true;
    }
    return false;
}

static void print_settings(const char *name, const NxClient_DisplaySettings *pSettings, const NxClient_PictureQualitySettings *pqSettings)
{
    char buf[256];
    unsigned n;
    printf("%s display settings:\n", name);

    n = 0;
    n += snprintf(&buf[n], sizeof(buf)-n, "  format: %s", lookup_name(g_videoFormatStrs, pSettings->format));
    switch (pSettings->display3DSettings.orientation) {
    case NEXUS_VideoOrientation_e3D_LeftRight:
        n += snprintf(&buf[n], sizeof(buf)-n, " (lr)");
        break;
    case NEXUS_VideoOrientation_e3D_OverUnder:
        n += snprintf(&buf[n], sizeof(buf)-n, " (ou)");
        break;
    default: break;
    }
    printf("%s\n", buf);

    n = 0;
    n += snprintf(&buf[n], sizeof(buf)-n, "  outputs:");
    if (pSettings->componentPreferences.enabled) {
        n += snprintf(&buf[n], sizeof(buf)-n, " component");
    }
    if (pSettings->compositePreferences.enabled) {
        n += snprintf(&buf[n], sizeof(buf)-n, " composite");
    }
    if (pSettings->hdmiPreferences.enabled) {
        n += snprintf(&buf[n], sizeof(buf)-n, " hdmi(%d bit,%s,%shdcp)",
            pSettings->hdmiPreferences.colorDepth,
            lookup_name(g_colorSpaceStrs, pSettings->hdmiPreferences.colorSpace),
            pSettings->hdmiPreferences.hdcp?"":"no ");
    }
    printf("%s\n", buf);

    if (pqSettings) {
        printf(
            "  color %d,%d,%d,%d\n"
            "  backgroundColor %#x\n",
            pqSettings->graphicsColor.contrast,
            pqSettings->graphicsColor.saturation,
            pqSettings->graphicsColor.hue,
            pqSettings->graphicsColor.brightness,
            pSettings->backgroundColor);
    }
}

static void print_status(void)
{
    int rc;
    NxClient_DisplayStatus status;
    rc = NxClient_GetDisplayStatus(&status);
    if (!rc) {
        printf(
            "display status\n"
            "  framebuffers=%d\n",
            status.framebuffer.number
            );
        printf("%d of %d transcode displays used\n",
            status.transcodeDisplays.used,
            status.transcodeDisplays.total);
#if NEXUS_HAS_HDMI_OUTPUT
        printf("HdmiOutput: connected? %c; preferred format %s\n",
            status.hdmi.status.connected?'y':'n',
            lookup_name(g_videoFormatStrs, status.hdmi.status.preferredVideoFormat));
#endif
    }
#if NEXUS_HAS_HDMI_OUTPUT
    {
    NEXUS_HdmiOutputHandle hdmiOutput;
    hdmiOutput = NEXUS_HdmiOutput_Open(NEXUS_ALIAS_ID + 0, NULL);
    if (hdmiOutput) {
        NEXUS_HdmiOutputBasicEdidData edid;
        rc = NEXUS_HdmiOutput_GetBasicEdidData(hdmiOutput, &edid);
        printf(
            "Basic EDID:\n"
            "  VendorID: %02x%02x\n"
            "  ProductID: %02x%02x\n"
            "  SerialNum: %02x%02x%02x%02x\n",
            edid.vendorID[0],edid.vendorID[1],
            edid.productID[0],edid.productID[1],
            edid.serialNum[0],edid.serialNum[1],edid.serialNum[2],edid.serialNum[3]
            );
        NEXUS_HdmiOutput_Close(hdmiOutput);
    }
    }
#endif
}

void nxclient_callback(void *context, int param)
{
    BSTD_UNUSED(context);
    switch (param) {
    case 0:
        BDBG_WRN(("hdmiOutputHotplug callback"));
        print_status();
        break;
    case 2:
        BDBG_WRN(("hdmiOutputHdcpChanged callback"));
        print_status();
        break;
    case 1:
        {
        NxClient_DisplaySettings settings;
        BDBG_WRN(("displaySettingsChanged callback"));
        NxClient_GetDisplaySettings(&settings);
        print_settings("change", &settings, NULL);
        }
        break;
    }
}

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"

int main(int argc, char **argv)  {
    NxClient_JoinSettings joinSettings;
    NxClient_DisplaySettings displaySettings;
    NxClient_PictureQualitySettings pqSettings;
    bool change = false;
    int curarg = 1;
    int rc;
    bool wait = false;
    bool sd = false;
    bool macrovision = false;
    char value[256];
    FILE *key = NULL;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s", argv[0]);
    joinSettings.ignoreStandbyRequest = true;
    joinSettings.timeout = 60;
    joinSettings.mode = NEXUS_ClientMode_eUntrusted;
    sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(value, "r");
    if (key != NULL) {
       memset(value, 0, sizeof(value));
       fread(value, 256, 1, key);
       if (strstr(value, "trusted:") == value) {
          const char *password = &value[8];
          joinSettings.mode = NEXUS_ClientMode_eProtected;
          joinSettings.certificate.length = strlen(password);
          memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
       }
       fclose(key);
    }
    rc = NxClient_Join(&joinSettings);
    if (rc) return -1;
    
    NxClient_GetDisplaySettings(&displaySettings);
    NxClient_GetPictureQualitySettings(&pqSettings);
    
    while (argc > curarg) {
        if (!strcmp(argv[curarg], "-h") || !strcmp(argv[curarg], "--help")) {
            print_usage();
            return 0;
        }
        else if (!strcmp(argv[curarg], "-sd")) {
            sd = true;
        }
        else if ((!strcmp(argv[curarg], "-format") || !strcmp(argv[curarg], "-display_format")) && argc>curarg+1) {
            NEXUS_VideoFormat format = lookup(g_videoFormatStrs, argv[++curarg]);
            change = true;
            if (sd) {
                displaySettings.slaveDisplay[0].format = format;
            }
            else {
                displaySettings.format = format;
            }
        }
        else if (!strcmp(argv[curarg], "-colorSpace") && argc>curarg+1) {
            change = true;
            displaySettings.hdmiPreferences.colorSpace = lookup(g_colorSpaceStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-colorDepth") && argc>curarg+1) {
            change = true;
            displaySettings.hdmiPreferences.colorDepth = atoi(argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-component") && argc>curarg+1) {
            change = true;
            displaySettings.componentPreferences.enabled = parse_boolean(argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-composite") && argc>curarg+1) {
            change = true;
            displaySettings.compositePreferences.enabled = parse_boolean(argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-hdmi") && argc>curarg+1) {
            change = true;
            displaySettings.hdmiPreferences.enabled = parse_boolean(argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-hdcp") && argc>curarg+1) {
            change = true;
            curarg++;
            if (argv[curarg][0] == 'm') {
                displaySettings.hdmiPreferences.hdcp = NxClient_HdcpLevel_eMandatory;
            }
            else if (argv[curarg][0] == 'o') {
                displaySettings.hdmiPreferences.hdcp = NxClient_HdcpLevel_eOptional;
            }
            else {
                print_usage();
                return -1;
            }
        }
        else if (!strcmp(argv[curarg], "-3d") && argc>curarg+1) {
            curarg++;
            change = true;
            if (strcmp(argv[curarg], "on")==0 || strcmp(argv[curarg], "lr")==0) {
                displaySettings.display3DSettings.orientation = NEXUS_VideoOrientation_e3D_LeftRight;
            }
            else if(strcmp(argv[curarg], "ou")==0) {
                displaySettings.display3DSettings.orientation = NEXUS_VideoOrientation_e3D_OverUnder;
            }
            else { /* off */
                displaySettings.display3DSettings.orientation = NEXUS_VideoOrientation_e2D;
            }
        }
        else if (!strcmp(argv[curarg], "-status")) {
            print_status();
        }
        else if (!strcmp(argv[curarg], "-color") && argc>curarg+1) {
            int contrast, saturation, hue, brightness;
            if (sscanf(argv[++curarg], "%d,%d,%d,%d", &contrast,&saturation,&hue,&brightness) == 4) {
                pqSettings.graphicsColor.contrast = contrast;
                pqSettings.graphicsColor.saturation = saturation;
                pqSettings.graphicsColor.hue = hue;
                pqSettings.graphicsColor.brightness = brightness;
                change = true;
            }
        }
        else if (!strcmp(argv[curarg], "-backgroundColor") && argc>curarg+1) {
            unsigned backgroundColor = strtoul(argv[++curarg], NULL, 16);
            if (sd) {
                displaySettings.slaveDisplay[0].backgroundColor = backgroundColor;
            }
            else {
                displaySettings.backgroundColor = backgroundColor;
            }
            change = true;
        }
        else if (!strcmp(argv[curarg], "-ar") && argc>curarg+1) {
            NEXUS_DisplayAspectRatio aspectRatio = lookup(g_displayAspectRatioStrs, argv[++curarg]);
            if (sd) {
                displaySettings.slaveDisplay[0].aspectRatio = aspectRatio;
            }
            else {
                displaySettings.aspectRatio = aspectRatio;
            }
            change = true;
        }
        else if (!strcmp(argv[curarg], "-sar") && argc>curarg+1) {
            int x,y;
            if (sscanf(argv[++curarg], "%d,%d", &x, &y) == 2) {
                if (sd) {
                    displaySettings.slaveDisplay[0].aspectRatio = NEXUS_DisplayAspectRatio_eSar;
                    displaySettings.slaveDisplay[0].sampleAspectRatio.x = x;
                    displaySettings.slaveDisplay[0].sampleAspectRatio.y = y;
                }
                else {
                    displaySettings.aspectRatio = NEXUS_DisplayAspectRatio_eSar;
                    displaySettings.sampleAspectRatio.x = x;
                    displaySettings.sampleAspectRatio.y = y;
                }
                change = true;
            }
        }
        else if (!strcmp(argv[curarg], "-wait")) {
            wait = true;
        }
        else if (!strcmp(argv[curarg], "-macrovision")) {
            macrovision = true;
        }
        else {
            print_usage();
            return -1;
        }
        curarg++;
    }

    if (macrovision) {
        /* hardcoded test */
        NxClient_Display_SetMacrovision(NEXUS_DisplayMacrovisionType_eAgcOnly, NULL);
    }

    if (change) {
        rc = NxClient_SetDisplaySettings(&displaySettings);
        if (rc) BERR_TRACE(rc);
        rc = NxClient_SetPictureQualitySettings(&pqSettings);
        if (rc) BERR_TRACE(rc);
        print_settings("new", &displaySettings, &pqSettings);
    }
    else {
        print_settings("current", &displaySettings, &pqSettings);
    }
    
    if (wait) {
        NxClient_CallbackThreadSettings settings;
        NxClient_GetDefaultCallbackThreadSettings(&settings);
        settings.hdmiOutputHotplug.callback = nxclient_callback;
        settings.hdmiOutputHotplug.param = 0;
        settings.hdmiOutputHdcpChanged.callback = nxclient_callback;
        settings.hdmiOutputHdcpChanged.param = 2;
        settings.displaySettingsChanged.callback = nxclient_callback;
        settings.displaySettingsChanged.param = 1;
        rc = NxClient_StartCallbackThread(&settings);
        BDBG_ASSERT(!rc);
        BKNI_Sleep(30 * 1000);
        NxClient_StopCallbackThread();
    }

    NxClient_Uninit();
    return 0;
}
