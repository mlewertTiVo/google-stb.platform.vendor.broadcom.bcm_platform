/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
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

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "NexusIR"

#include "nexusirmap.h"
#include <linux/input.h>
#include <stdlib.h>

#include <cutils/log.h>

static __u16 key(const char * name)
{
    struct keyname {
        const char * name;
        __u16 id;
    };

    /* names for all KEY_* #defines from linux/input.h */
    #define DEFINE_KEY(name) {#name, KEY_##name}
    static const struct keyname linux_keys[] = {
        //DEFINE_KEY(RESERVED),
        DEFINE_KEY(ESC),
        DEFINE_KEY(1),
        DEFINE_KEY(2),
        DEFINE_KEY(3),
        DEFINE_KEY(4),
        DEFINE_KEY(5),
        DEFINE_KEY(6),
        DEFINE_KEY(7),
        DEFINE_KEY(8),
        DEFINE_KEY(9),
        DEFINE_KEY(0),
        DEFINE_KEY(MINUS),
        DEFINE_KEY(EQUAL),
        DEFINE_KEY(BACKSPACE),
        DEFINE_KEY(TAB),
        DEFINE_KEY(Q),
        DEFINE_KEY(W),
        DEFINE_KEY(E),
        DEFINE_KEY(R),
        DEFINE_KEY(T),
        DEFINE_KEY(Y),
        DEFINE_KEY(U),
        DEFINE_KEY(I),
        DEFINE_KEY(O),
        DEFINE_KEY(P),
        DEFINE_KEY(LEFTBRACE),
        DEFINE_KEY(RIGHTBRACE),
        DEFINE_KEY(ENTER),
        DEFINE_KEY(LEFTCTRL),
        DEFINE_KEY(A),
        DEFINE_KEY(S),
        DEFINE_KEY(D),
        DEFINE_KEY(F),
        DEFINE_KEY(G),
        DEFINE_KEY(H),
        DEFINE_KEY(J),
        DEFINE_KEY(K),
        DEFINE_KEY(L),
        DEFINE_KEY(SEMICOLON),
        DEFINE_KEY(APOSTROPHE),
        DEFINE_KEY(GRAVE),
        DEFINE_KEY(LEFTSHIFT),
        DEFINE_KEY(BACKSLASH),
        DEFINE_KEY(Z),
        DEFINE_KEY(X),
        DEFINE_KEY(C),
        DEFINE_KEY(V),
        DEFINE_KEY(B),
        DEFINE_KEY(N),
        DEFINE_KEY(M),
        DEFINE_KEY(COMMA),
        DEFINE_KEY(DOT),
        DEFINE_KEY(SLASH),
        DEFINE_KEY(RIGHTSHIFT),
        DEFINE_KEY(KPASTERISK),
        DEFINE_KEY(LEFTALT),
        DEFINE_KEY(SPACE),
        DEFINE_KEY(CAPSLOCK),
        DEFINE_KEY(F1),
        DEFINE_KEY(F2),
        DEFINE_KEY(F3),
        DEFINE_KEY(F4),
        DEFINE_KEY(F5),
        DEFINE_KEY(F6),
        DEFINE_KEY(F7),
        DEFINE_KEY(F8),
        DEFINE_KEY(F9),
        DEFINE_KEY(F10),
        DEFINE_KEY(NUMLOCK),
        DEFINE_KEY(SCROLLLOCK),
        DEFINE_KEY(KP7),
        DEFINE_KEY(KP8),
        DEFINE_KEY(KP9),
        DEFINE_KEY(KPMINUS),
        DEFINE_KEY(KP4),
        DEFINE_KEY(KP5),
        DEFINE_KEY(KP6),
        DEFINE_KEY(KPPLUS),
        DEFINE_KEY(KP1),
        DEFINE_KEY(KP2),
        DEFINE_KEY(KP3),
        DEFINE_KEY(KP0),
        DEFINE_KEY(KPDOT),

        DEFINE_KEY(ZENKAKUHANKAKU),
        DEFINE_KEY(102ND),
        DEFINE_KEY(F11),
        DEFINE_KEY(F12),
        DEFINE_KEY(RO),
        DEFINE_KEY(KATAKANA),
        DEFINE_KEY(HIRAGANA),
        DEFINE_KEY(HENKAN),
        DEFINE_KEY(KATAKANAHIRAGANA),
        DEFINE_KEY(MUHENKAN),
        DEFINE_KEY(KPJPCOMMA),
        DEFINE_KEY(KPENTER),
        DEFINE_KEY(RIGHTCTRL),
        DEFINE_KEY(KPSLASH),
        DEFINE_KEY(SYSRQ),
        DEFINE_KEY(RIGHTALT),
        DEFINE_KEY(LINEFEED),
        DEFINE_KEY(HOME),
        DEFINE_KEY(UP),
        DEFINE_KEY(PAGEUP),
        DEFINE_KEY(LEFT),
        DEFINE_KEY(RIGHT),
        DEFINE_KEY(END),
        DEFINE_KEY(DOWN),
        DEFINE_KEY(PAGEDOWN),
        DEFINE_KEY(INSERT),
        DEFINE_KEY(DELETE),
        DEFINE_KEY(MACRO),
        DEFINE_KEY(MUTE),
        DEFINE_KEY(VOLUMEDOWN),
        DEFINE_KEY(VOLUMEUP),
        DEFINE_KEY(POWER),
        DEFINE_KEY(KPEQUAL),
        DEFINE_KEY(KPPLUSMINUS),
        DEFINE_KEY(PAUSE),
        DEFINE_KEY(SCALE),

        DEFINE_KEY(KPCOMMA),
        DEFINE_KEY(HANGEUL),
        DEFINE_KEY(HANGUEL),
        DEFINE_KEY(HANJA),
        DEFINE_KEY(YEN),
        DEFINE_KEY(LEFTMETA),
        DEFINE_KEY(RIGHTMETA),
        DEFINE_KEY(COMPOSE),

        DEFINE_KEY(STOP),
        DEFINE_KEY(AGAIN),
        DEFINE_KEY(PROPS),
        DEFINE_KEY(UNDO),
        DEFINE_KEY(FRONT),
        DEFINE_KEY(COPY),
        DEFINE_KEY(OPEN),
        DEFINE_KEY(PASTE),
        DEFINE_KEY(FIND),
        DEFINE_KEY(CUT),
        DEFINE_KEY(HELP),
        DEFINE_KEY(MENU),
        DEFINE_KEY(CALC),
        DEFINE_KEY(SETUP),
        DEFINE_KEY(SLEEP),
        DEFINE_KEY(WAKEUP),
        DEFINE_KEY(FILE),
        DEFINE_KEY(SENDFILE),
        DEFINE_KEY(DELETEFILE),
        DEFINE_KEY(XFER),
        DEFINE_KEY(PROG1),
        DEFINE_KEY(PROG2),
        DEFINE_KEY(WWW),
        DEFINE_KEY(MSDOS),
        DEFINE_KEY(COFFEE),
        DEFINE_KEY(SCREENLOCK),
        DEFINE_KEY(DIRECTION),
        DEFINE_KEY(CYCLEWINDOWS),
        DEFINE_KEY(MAIL),
        DEFINE_KEY(BOOKMARKS),
        DEFINE_KEY(COMPUTER),
        DEFINE_KEY(BACK),
        DEFINE_KEY(FORWARD),
        DEFINE_KEY(CLOSECD),
        DEFINE_KEY(EJECTCD),
        DEFINE_KEY(EJECTCLOSECD),
        DEFINE_KEY(NEXTSONG),
        DEFINE_KEY(PLAYPAUSE),
        DEFINE_KEY(PREVIOUSSONG),
        DEFINE_KEY(STOPCD),
        DEFINE_KEY(RECORD),
        DEFINE_KEY(REWIND),
        DEFINE_KEY(PHONE),
        DEFINE_KEY(ISO),
        DEFINE_KEY(CONFIG),
        DEFINE_KEY(HOMEPAGE),
        DEFINE_KEY(REFRESH),
        DEFINE_KEY(EXIT),
        DEFINE_KEY(MOVE),
        DEFINE_KEY(EDIT),
        DEFINE_KEY(SCROLLUP),
        DEFINE_KEY(SCROLLDOWN),
        DEFINE_KEY(KPLEFTPAREN),
        DEFINE_KEY(KPRIGHTPAREN),
        DEFINE_KEY(NEW),
        DEFINE_KEY(REDO),

        DEFINE_KEY(F13),
        DEFINE_KEY(F14),
        DEFINE_KEY(F15),
        DEFINE_KEY(F16),
        DEFINE_KEY(F17),
        DEFINE_KEY(F18),
        DEFINE_KEY(F19),
        DEFINE_KEY(F20),
        DEFINE_KEY(F21),
        DEFINE_KEY(F22),
        DEFINE_KEY(F23),
        DEFINE_KEY(F24),

        DEFINE_KEY(PLAYCD),
        DEFINE_KEY(PAUSECD),
        DEFINE_KEY(PROG3),
        DEFINE_KEY(PROG4),
        DEFINE_KEY(DASHBOARD),
        DEFINE_KEY(SUSPEND),
        DEFINE_KEY(CLOSE),
        DEFINE_KEY(PLAY),
        DEFINE_KEY(FASTFORWARD),
        DEFINE_KEY(BASSBOOST),
        DEFINE_KEY(PRINT),
        DEFINE_KEY(HP),
        DEFINE_KEY(CAMERA),
        DEFINE_KEY(SOUND),
        DEFINE_KEY(QUESTION),
        DEFINE_KEY(EMAIL),
        DEFINE_KEY(CHAT),
        DEFINE_KEY(SEARCH),
        DEFINE_KEY(CONNECT),
        DEFINE_KEY(FINANCE),
        DEFINE_KEY(SPORT),
        DEFINE_KEY(SHOP),
        DEFINE_KEY(ALTERASE),
        DEFINE_KEY(CANCEL),
        DEFINE_KEY(BRIGHTNESSDOWN),
        DEFINE_KEY(BRIGHTNESSUP),
        DEFINE_KEY(MEDIA),

        DEFINE_KEY(SWITCHVIDEOMODE),
        DEFINE_KEY(KBDILLUMTOGGLE),
        DEFINE_KEY(KBDILLUMDOWN),
        DEFINE_KEY(KBDILLUMUP),

        DEFINE_KEY(SEND),
        DEFINE_KEY(REPLY),
        DEFINE_KEY(FORWARDMAIL),
        DEFINE_KEY(SAVE),
        DEFINE_KEY(DOCUMENTS),

        DEFINE_KEY(BATTERY),

        DEFINE_KEY(BLUETOOTH),
        DEFINE_KEY(WLAN),
        DEFINE_KEY(UWB),

        DEFINE_KEY(UNKNOWN),

        DEFINE_KEY(VIDEO_NEXT),
        DEFINE_KEY(VIDEO_PREV),
        DEFINE_KEY(BRIGHTNESS_CYCLE),
        DEFINE_KEY(BRIGHTNESS_AUTO),
        DEFINE_KEY(BRIGHTNESS_ZERO),
        DEFINE_KEY(DISPLAY_OFF),

        DEFINE_KEY(WWAN),
        DEFINE_KEY(WIMAX),
        DEFINE_KEY(RFKILL),

        DEFINE_KEY(MICMUTE),

        DEFINE_KEY(OK),
        DEFINE_KEY(SELECT),
        DEFINE_KEY(GOTO),
        DEFINE_KEY(CLEAR),
        DEFINE_KEY(POWER2),
        DEFINE_KEY(OPTION),
        DEFINE_KEY(INFO),
        DEFINE_KEY(TIME),
        DEFINE_KEY(VENDOR),
        DEFINE_KEY(ARCHIVE),
        DEFINE_KEY(PROGRAM),
        DEFINE_KEY(CHANNEL),
        DEFINE_KEY(FAVORITES),
        DEFINE_KEY(EPG),
        DEFINE_KEY(PVR),
        DEFINE_KEY(MHP),
        DEFINE_KEY(LANGUAGE),
        DEFINE_KEY(TITLE),
        DEFINE_KEY(SUBTITLE),
        DEFINE_KEY(ANGLE),
        DEFINE_KEY(ZOOM),
        DEFINE_KEY(MODE),
        DEFINE_KEY(KEYBOARD),
        DEFINE_KEY(SCREEN),
        DEFINE_KEY(PC),
        DEFINE_KEY(TV),
        DEFINE_KEY(TV2),
        DEFINE_KEY(VCR),
        DEFINE_KEY(VCR2),
        DEFINE_KEY(SAT),
        DEFINE_KEY(SAT2),
        DEFINE_KEY(CD),
        DEFINE_KEY(TAPE),
        DEFINE_KEY(RADIO),
        DEFINE_KEY(TUNER),
        DEFINE_KEY(PLAYER),
        DEFINE_KEY(TEXT),
        DEFINE_KEY(DVD),
        DEFINE_KEY(AUX),
        DEFINE_KEY(MP3),
        DEFINE_KEY(AUDIO),
        DEFINE_KEY(VIDEO),
        DEFINE_KEY(DIRECTORY),
        DEFINE_KEY(LIST),
        DEFINE_KEY(MEMO),
        DEFINE_KEY(CALENDAR),
        DEFINE_KEY(RED),
        DEFINE_KEY(GREEN),
        DEFINE_KEY(YELLOW),
        DEFINE_KEY(BLUE),
        DEFINE_KEY(CHANNELUP),
        DEFINE_KEY(CHANNELDOWN),
        DEFINE_KEY(FIRST),
        DEFINE_KEY(LAST),
        DEFINE_KEY(AB),
        DEFINE_KEY(NEXT),
        DEFINE_KEY(RESTART),
        DEFINE_KEY(SLOW),
        DEFINE_KEY(SHUFFLE),
        DEFINE_KEY(BREAK),
        DEFINE_KEY(PREVIOUS),
        DEFINE_KEY(DIGITS),
        DEFINE_KEY(TEEN),
        DEFINE_KEY(TWEN),
        DEFINE_KEY(VIDEOPHONE),
        DEFINE_KEY(GAMES),
        DEFINE_KEY(ZOOMIN),
        DEFINE_KEY(ZOOMOUT),
        DEFINE_KEY(ZOOMRESET),
        DEFINE_KEY(WORDPROCESSOR),
        DEFINE_KEY(EDITOR),
        DEFINE_KEY(SPREADSHEET),
        DEFINE_KEY(GRAPHICSEDITOR),
        DEFINE_KEY(PRESENTATION),
        DEFINE_KEY(DATABASE),
        DEFINE_KEY(NEWS),
        DEFINE_KEY(VOICEMAIL),
        DEFINE_KEY(ADDRESSBOOK),
        DEFINE_KEY(MESSENGER),
        DEFINE_KEY(DISPLAYTOGGLE),
        DEFINE_KEY(BRIGHTNESS_TOGGLE),
        DEFINE_KEY(SPELLCHECK),
        DEFINE_KEY(LOGOFF),

        DEFINE_KEY(DOLLAR),
        DEFINE_KEY(EURO),

        DEFINE_KEY(FRAMEBACK),
        DEFINE_KEY(FRAMEFORWARD),
        DEFINE_KEY(CONTEXT_MENU),
        DEFINE_KEY(MEDIA_REPEAT),
        DEFINE_KEY(10CHANNELSUP),
        DEFINE_KEY(10CHANNELSDOWN),
        DEFINE_KEY(IMAGES),

        DEFINE_KEY(DEL_EOL),
        DEFINE_KEY(DEL_EOS),
        DEFINE_KEY(INS_LINE),
        DEFINE_KEY(DEL_LINE),

        DEFINE_KEY(FN),
        DEFINE_KEY(FN_ESC),
        DEFINE_KEY(FN_F1),
        DEFINE_KEY(FN_F2),
        DEFINE_KEY(FN_F3),
        DEFINE_KEY(FN_F4),
        DEFINE_KEY(FN_F5),
        DEFINE_KEY(FN_F6),
        DEFINE_KEY(FN_F7),
        DEFINE_KEY(FN_F8),
        DEFINE_KEY(FN_F9),
        DEFINE_KEY(FN_F10),
        DEFINE_KEY(FN_F11),
        DEFINE_KEY(FN_F12),
        DEFINE_KEY(FN_1),
        DEFINE_KEY(FN_2),
        DEFINE_KEY(FN_D),
        DEFINE_KEY(FN_E),
        DEFINE_KEY(FN_F),
        DEFINE_KEY(FN_S),
        DEFINE_KEY(FN_B),

        DEFINE_KEY(BRL_DOT1),
        DEFINE_KEY(BRL_DOT2),
        DEFINE_KEY(BRL_DOT3),
        DEFINE_KEY(BRL_DOT4),
        DEFINE_KEY(BRL_DOT5),
        DEFINE_KEY(BRL_DOT6),
        DEFINE_KEY(BRL_DOT7),
        DEFINE_KEY(BRL_DOT8),
        DEFINE_KEY(BRL_DOT9),
        DEFINE_KEY(BRL_DOT10),

        DEFINE_KEY(NUMERIC_0),
        DEFINE_KEY(NUMERIC_1),
        DEFINE_KEY(NUMERIC_2),
        DEFINE_KEY(NUMERIC_3),
        DEFINE_KEY(NUMERIC_4),
        DEFINE_KEY(NUMERIC_5),
        DEFINE_KEY(NUMERIC_6),
        DEFINE_KEY(NUMERIC_7),
        DEFINE_KEY(NUMERIC_8),
        DEFINE_KEY(NUMERIC_9),
        DEFINE_KEY(NUMERIC_STAR),
        DEFINE_KEY(NUMERIC_POUND),

        DEFINE_KEY(CAMERA_FOCUS),
        DEFINE_KEY(WPS_BUTTON),

        DEFINE_KEY(TOUCHPAD_TOGGLE),
        DEFINE_KEY(TOUCHPAD_ON),
        DEFINE_KEY(TOUCHPAD_OFF),

        DEFINE_KEY(CAMERA_ZOOMIN),
        DEFINE_KEY(CAMERA_ZOOMOUT),
        DEFINE_KEY(CAMERA_UP),
        DEFINE_KEY(CAMERA_DOWN),
        DEFINE_KEY(CAMERA_LEFT),
        DEFINE_KEY(CAMERA_RIGHT),

        DEFINE_KEY(ATTENDANT_ON),
        DEFINE_KEY(ATTENDANT_OFF),
        DEFINE_KEY(ATTENDANT_TOGGLE),
        DEFINE_KEY(LIGHTS_TOGGLE),

        DEFINE_KEY(ALS_TOGGLE),

        DEFINE_KEY(BUTTONCONFIG),
        DEFINE_KEY(TASKMANAGER),
        DEFINE_KEY(JOURNAL),
        DEFINE_KEY(CONTROLPANEL),
        DEFINE_KEY(APPSELECT),
        DEFINE_KEY(SCREENSAVER),
        DEFINE_KEY(VOICECOMMAND),

        DEFINE_KEY(BRIGHTNESS_MIN),
        DEFINE_KEY(BRIGHTNESS_MAX),

        /* end of table marker */
        {NULL, 0}
    };
    #undef DEFINE_KEY

    const struct keyname * ptr = linux_keys;
    while(ptr->name) {
        if (strcmp(ptr->name, name) == 0) {
            return ptr->id;
        }
        ptr++;
    }
    return 0; //not found
}

static const char* WHITESPACE = " \t\r";

using namespace android;

NexusIrMap::NexusIrMap()
{}

NexusIrMap::~NexusIrMap()
{}

/*static*/ status_t NexusIrMap::load(const String8& filename,
        sp<NexusIrMap>* outMap)
{
    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(filename, &tokenizer);
    if (status) {
        LOGE("Error %d opening key layout map file %s.", status,
                filename.string());
    } else {
        sp<NexusIrMap> map = new NexusIrMap();
        if (!map.get()) {
            LOGE("Error allocating key layout map.");
            status = NO_MEMORY;
        } else {
            Parser parser(map, tokenizer);
            status = parser.parse();
            if (!status) {
                *outMap = map;
            }
        }
        delete tokenizer;
    }
    return status;
}
__u16 NexusIrMap::get(uint32_t nexusCode) const
{
    ssize_t index = mKeys.indexOfKey(nexusCode);
    return index >= 0 ? mKeys.valueAt(index) : 0;
}

NexusIrMap::Parser::Parser(sp<NexusIrMap> map, Tokenizer* tokenizer):
    mMap(map), mTokenizer(tokenizer)
{}

NexusIrMap::Parser::~Parser()
{}

status_t NexusIrMap::Parser::parse()
{
    while (!mTokenizer->isEof()) {
        LOGV("Parsing %s: '%s'.", mTokenizer->getLocation().string(),
                mTokenizer->peekRemainderOfLine().string());

        mTokenizer->skipDelimiters(WHITESPACE);

        if (!mTokenizer->isEol() && mTokenizer->peekChar() != '#') {
            status_t status = parseKey();
            if (status) return status;
        }

        mTokenizer->nextLine();
    }
    return NO_ERROR;
}

status_t NexusIrMap::Parser::parseKey()
{
    String8 irCodeToken = mTokenizer->nextToken(WHITESPACE);
    LOGV("%s: Got IR code '%s'",
            mTokenizer->getLocation().string(),
            irCodeToken.string());

    char* end;
    uint32_t irCode = uint32_t(strtoul(irCodeToken.string(), &end, 0));
    if (*end) {
        LOGE("%s: Expected IR code, got '%s'.",
                mTokenizer->getLocation().string(),
                irCodeToken.string());
        return BAD_VALUE;
    }

    if (mMap->mKeys.indexOfKey(irCode) >= 0) {
        LOGE("%s: Duplicate entry '%s' for key 0x%08x",
                mTokenizer->getLocation().string(),
                irCodeToken.string(), irCode);
        return BAD_VALUE;
    }

    mTokenizer->skipDelimiters(WHITESPACE);
    String8 keyCodeToken = mTokenizer->nextToken(WHITESPACE);
    LOGV("%s: Got key code for '%s': '%s'",
            mTokenizer->getLocation().string(),
            irCodeToken.string(),
            keyCodeToken.string());

    __u16 keyCode = key(keyCodeToken.string());
    if (!keyCode) {
        LOGE("%s: Expected key code label, got '%s'.",
                mTokenizer->getLocation().string(),
                keyCodeToken.string());
        return BAD_VALUE;
    }

    LOGV("Parsed irCode=0x%08x ('%s'), keyCode=%d ('KEY_%s').",
            (unsigned)irCode, irCodeToken.string(),
            (int)keyCode, keyCodeToken.string());

    mMap->mKeys.add(irCode, keyCode);
    return NO_ERROR;
}
