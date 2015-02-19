/******************************************************************************
 *    (c)2011-2012 Broadcom Corporation
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
 **********************************************************************************************************/

#ifndef _TRELLIS_KEYMAP_H_
#define _TRELLIS_KEYMAP_H_

#if SBS_USES_TRELLIS_INPUT_EVENTS

static int trellisAndroidKeymap[] = {
    KEY_RESERVED,   /* CodeInvalid = 0 */
    KEY_POWER,      /* CodePower */
    KEY_UP,         /* CodeUp */
    KEY_DOWN,       /* CodeDown */
    KEY_LEFT,       /* CodeLeft */
    KEY_RIGHT,      /* CodeRight */
    KEY_ENTER,         /* CodeOk */
    KEY_PAGEUP,     /* CodePageUp */
    KEY_PAGEDOWN,   /* CodePageDown */
    KEY_PLAY,       /* CodePlay */
    KEY_RECORD,     /* CodeRecord */
    KEY_STOP,       /* CodeStop */
    KEY_PAUSE,      /* CodePause */
    KEY_NEXTSONG,   /* CodeNext */
    KEY_PREVIOUSSONG,   /*CodePrevious */
    KEY_FASTFORWARD,    /* CodeFastFwd */
    KEY_FASTFORWARD,    /* CodeFastRwd */
    KEY_VOLUMEUP,       /* CodeVolumeUp */
    KEY_VOLUMEDOWN,     /*CodeVolumeDown */
    KEY_MUTE,           /* CodeVolumeMute */
    KEY_RESERVED,       /* CodeInput */
    KEY_CHANNELUP,      /* CodeChannelUp */
    KEY_CHANNELDOWN,    /* CodeChannelDown */
    KEY_LAST,           /* CodeChannelLast */
    KEY_AUDIO,          /* CodeAudio */
    KEY_VIDEO,          /* CodeVideo */
    KEY_RESERVED,       /* CodePip */
    KEY_SUBTITLE,       /* CodeSubtitle */
    KEY_VENDOR,         /* CodeVendor */
    KEY_SETUP,          /* CodeSetup */
    KEY_MENU,           /* CodeMenu */
    KEY_INFO,           /* CodeInfo */
    KEY_PROGRAM,        /* CodeGuide */
    KEY_BACK,           /* CodeBack */
    KEY_EXIT,           /* CodeExit */
    KEY_RED,            /* CodeColorRed */
    KEY_GREEN,          /* CodeColorGreen */
    KEY_YELLOW,         /* CodeColorYellow */
    KEY_BLUE,           /* CodeColorBlue */
    KEY_TAB,            /* CodeTab */
    KEY_DELETE,         /* CodeDelete */
    KEY_BACKSPACE,      /* CodeBackspace */
    KEY_RESERVED,       /* CodeShortcut1 */
    KEY_RESERVED,       /* CodeShortcut2 */
    KEY_RESERVED,       /* CodeShortcut3 */
    KEY_RESERVED,       /* CodeShortcut4 */
    KEY_RESERVED,       /* CodeDebugTrace */
    KEY_RESERVED,       /* CodeDebugAV */
    KEY_RESERVED,       /* CodeDebugMemory */
    KEY_RESERVED,       /* CodeDebugPQ */
    KEY_EJECTCD,        /* CodeEject */
    KEY_ESC,            /* CodeEsc */
    KEY_F1,             /* CodeF1 */
    KEY_F2,             /* CodeF2 */
    KEY_F3,             /* CodeF3 */
    KEY_F4,             /* CodeF4 */
    KEY_F5,             /* CodeF5 */
    KEY_F6,             /* CodeF6 */
    KEY_F7,             /* CodeF7 */
    KEY_F8,             /* CodeF8 */
    KEY_F9,             /* CodeF9 */
    KEY_F10,            /* CodeF10 */
    KEY_F11,            /* CodeF11 */
    KEY_F12,            /* CodeF12 */
    KEY_RESERVED,       /* CodeAspect */
    KEY_RESERVED,       /* CodeTv */
    KEY_HOMEPAGE        /* CodeApps */
};

static int asciiToKeycode[] = {
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_ENTER, KEY_RESERVED,
    KEY_BACKSPACE, KEY_TAB, KEY_ENTER, KEY_RESERVED, KEY_RESERVED, KEY_ENTER, KEY_RESERVED, KEY_RESERVED,
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_ESC, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_SPACE, KEY_1, KEY_APOSTROPHE, KEY_3, KEY_4, KEY_5, KEY_7, KEY_APOSTROPHE,
    KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_8, KEY_EQUAL, KEY_COMMA, KEY_MINUS, KEY_DOT, KEY_SLASH,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7,
    KEY_8, KEY_9, KEY_SEMICOLON, KEY_SEMICOLON, KEY_COMMA, KEY_EQUAL, KEY_DOT, KEY_SLASH,
    KEY_2, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,
    KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, 
    KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, 
    KEY_X, KEY_Y, KEY_Z, KEY_RESERVED, KEY_BACKSLASH, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_RESERVED, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,
    KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, 
    KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, 
    KEY_X, KEY_Y, KEY_Z, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
};

static int asciiWithShift[] = {
    33, /*  ! */
    64, /*  @  */
    35, /*  #  */
    36, /*  $  */
    37, /* %  */
    94, /*  ^  */
    38, /*  &  */
    42, /*  *  */
    40, /*  (  */
    41, /*  )  */
    95, /*  _  */
    43, /*  +  */
    123, /*  {  */
    125, /*  }  */
    126, /*  |  */
    58, /*  :  */
    34, /*  "  */
    60, /*  <  */
    62, /*  >  */
    63 /* ? */
};
#endif /* SBS_USES_TRELLIS_INPUT_EVENTS */

#endif
