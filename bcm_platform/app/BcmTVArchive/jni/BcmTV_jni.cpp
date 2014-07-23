/******************************************************************************
 *    (c)2013 Broadcom Corporation
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
#include <jni.h>
#include <utils/Log.h>
#include <assert.h>

// Trellis includes
#include <string>
#include "Broker.h"
#include "StandaloneApplication.h"
#include "ITVGuide.h"
#include "ITVChannel.h"
#include "ITVConfig.h"
#include "ITVConfigAdaptor.h"
#include "IStreamExtractor.h"
#include "IMediaPlayer.h"
#include "IQamSource.h"

// Generated header
#include "ITVChannelAdaptor.h"

using namespace Trellis;
using namespace Trellis::Media;

// NOTE: Don't mess with these # without checking the code
#if (BCHP_CHIP == 7435)
#define MAX_PRIMERS		4	// total (prefix + current + after)
#define MAX_PREFIX		1	// # of channels before the currently primed
#define MAX_SUFFIX		2	// # after
#else
#define MAX_PRIMERS		3
#define MAX_PREFIX		1
#define MAX_SUFFIX		1
#endif

#ifdef ANDROID_USES_ARTEMIS
	#define DISABLE_FCC
	#define DISABLE_PIP
	#define SVC_NAME	"ArtemisTV"
#else
	#define SVC_NAME	"TVService"
#endif

// Enable this for debug prints
//#define DEBUG_JNI

#ifdef DEBUG_JNI
#define TV_LOG	ALOGE
#else
#define TV_LOG
#endif
// All globals must be JNI primitives, any other data type 
// will fail to hold its value across different JNI methods
void *j_mp_fcc[MAX_PRIMERS], *j_mp_main, *j_mp_pip, *j_con, *j_config, *j_guide, *j_channel;

extern "C" void initOrb(Trellis::IApplicationContainer* app);
void merge_prime_and_new_lists(int iNum);
bool NeedToScan();

static jboolean		JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setChannel(JNIEnv *env, jobject obj, jint iNum);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_stop(JNIEnv *env, jobject obj);
static jobjectArray JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_getChannelList(JNIEnv *env, jobject obj, jint iNum);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setChannel_pip(JNIEnv *env, jobject obj, jint iNum, jint x, jint y, jint w, jint h);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_stop_pip(JNIEnv *env, jobject obj);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setMode(JNIEnv *env, jobject obj, jboolean bIsFCCOn);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_reprime(JNIEnv *env, jobject obj, jint iNum);
static jobjectArray JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_getPrimedList(JNIEnv *env, jobject obj);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_force_scan(JNIEnv *env, jobject obj);
static jint			JNICALL		Java_com_broadcom_tvarchive_BcmTVArchive_JNI_close(JNIEnv *env, jobject obj);

// The signature syntax is: Native-method-name, input-params, output-params & fully-qualified-name
static JNINativeMethod gMethods[] = 
{
	{"setChannel",		"(I)Z",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setChannel},
	{"stop",			"()I",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_stop},
	{"getChannelList",	"(I)[Ljava/lang/String;",	(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_getChannelList},
	{"setChannel_pip",	"(IIIII)I",					(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setChannel_pip},
	{"stop_pip",		"()I",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_stop_pip},
	{"setMode",			"(Z)I",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setMode},
	{"reprime",			"(I)I",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_reprime},
	{"getPrimedList",	"()[Ljava/lang/String;",	(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_getPrimedList},
	{"force_scan",		"()I",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_force_scan},
	{"close",			"()I",						(void *)&Java_com_broadcom_tvarchive_BcmTVArchive_JNI_close},
};

typedef struct _Ch_List
{
    int iChannel;
    struct _Ch_List *pNext;
    struct _Ch_List *pBack;
}Ch_List;

Ch_List *pHead, *pCur, *pLast;

class JNITVPlayerContext : public StandaloneApplication
{
public:
    class LocalParameters
	{
		public:
			LocalParameters(Util::Param& p) 
			{
				p.add("client", SVC_NAME);
			}
    };

    class LibraryLoader
    {
		public:
			LibraryLoader(const Util::Param& params) 
			{
			}
    };

    JNITVPlayerContext() :
        StandaloneApplication(0, NULL),
        localParameters(getParameters()),
        libraryLoader(getParameters()),
        listenerAdaptor(*this),
        channelListenerAdaptor(*this),
		_config(NULL)
	{
		TV_LOG("libbcmtv_jni: Calling StandaloneApplication::init");
	    StandaloneApplication::init();

		TV_LOG("libbcmtv_jni: Calling initOrb");
        initOrb(this);

		// Disable FCC by default
		bFCCMode = false;
		bPrimesReleased = false;
		bRePrime = false;

		// Initialize the # of primers
		iNumPrimers = MAX_PRIMERS;

		if (_config == NULL) 
		{
			TV_LOG("libbcmtv_jni: Setting up TV Channels & guide (using service: %s)", SVC_NAME);
			_config = ITVConfig::resolve(Orb::ServiceInfo(SVC_NAME));

			if (_config != NULL)
			{
				j_config = _config;
				TV_LOG("libbcmtv_jni: _config is valid!!");
			}

            _config->addListener(&listenerAdaptor);
            _channel = ITVChannel::resolve(Orb::ServiceInfo(SVC_NAME));

			if (_channel != NULL)
			{
				j_channel = _channel;
				TV_LOG("libbcmtv_jni: _channel is valid!!");
			}

			_guide = ITVGuide::resolve(Orb::ServiceInfo(SVC_NAME));

			if (_guide != NULL)
			{
				j_guide = _guide;
				TV_LOG("libbcmtv_jni: _channel is valid!!");
			}
        }

		TV_LOG("libbcmtv_jni: Out of JNITVPlayerContext");
    }

	void StartScan()
	{
		TV_LOG("libbcmtv_jni: StartScan: Calling startBlindScan");
		_scanbusy = -1;

//		_config->clearTuningParamsList();
		_config->startBlindScan();

		TV_LOG("libbcmtv_jni: StartScan: Done with startBlindScan");

		while (_scanbusy < 0) 
		{
            sleep(1);
        }
        
        TV_LOG("libbcmtv_jni: StartScan: started scanning");

        while (_scanbusy <= 0) 
		{
            sleep(1);
        }

        TV_LOG("libbcmtv_jni: StartScan, scan complete");
	}

	void onScanProgress(const uint8_t &percent) 
	{
        TV_LOG("libbcmtv_jni: onScanProgress %d\%", (unsigned)percent);
    }

    void onScanStart() 
	{
        TV_LOG("libbcmtv_jni: onScanStart");
        _scanbusy = 0;
    }

    void onScanComplete() 
	{
        TV_LOG("libbcmtv_jni: onScanComplete");
        _scanbusy = 1;
    }

    void onChannelChange() 
	{
        TV_LOG("libbcmtv_jni: OnChannelChange");
        _scanbusy = 1;
    }

    void onClockSet(ITVConfig::IListener::ClockSetType) 
	{
        TV_LOG("libbcmtv_jni: OnClockSet");
//        _scanbusy = 0;
    }

    void onLocalTimeOffsetUpdate() 
	{
		TV_LOG("libbcmtv_jni: onLocalTimeOffsetUpdate");
    }

    void onChannelUpdate(const std::string& id) 
	{
		TV_LOG("libbcmtv_jni: onChannelUpdate");
    }

	ITV::ChannelList getChannelList()
	{
		TV_LOG("libbcmtv_jni: Calling getChannelListFromChannelGroup");
		return _channel->getChannelListFromChannelGroup(0);
	}

	int _currentChannel;
    ITVChannel *_channel;
	ITV::ChannelList channelList;

	// FCC related
	int iNumChannels;
	bool bFCCMode;
	bool bPrimesReleased;
	bool bRePrime;
    unsigned int iNumPrimers;
	unsigned int iPrimedIndex;
	int iPrimedChannelList[MAX_PRIMERS];

private:
	LocalParameters localParameters;
    LibraryLoader libraryLoader;
	TVConfigIListenerAdaptor<JNITVPlayerContext> listenerAdaptor;
    TVChannelIListenerAdaptor<JNITVPlayerContext> channelListenerAdaptor;
	ITVConfig *_config;
    ITVGuide *_guide;
	int _scanbusy;
};

static int registerNativeMethods(JNIEnv* env, const char* className, JNINativeMethod* pMethods, int numMethods)
{
    jclass clazz;
	int i;

    clazz = env->FindClass(className);
    if (clazz == NULL) 
    {
        TV_LOG("libbcmtv_jni: Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }

    TV_LOG("libbcmtv_jni: numMethods = %d", numMethods);
	for (i = 0; i < numMethods; i++)
	{
		TV_LOG("libbcmtv_jni: pMethods[%d]: %s, %s, %p", i, pMethods[i].name, pMethods[i].signature, pMethods[i].fnPtr);
	}

    if (env->RegisterNatives(clazz, pMethods, numMethods) < 0) 
    {
        TV_LOG("libbcmtv_jni: RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	TV_LOG("libbcmtv_jni: Creating JNITVPlayerContext object");
	static JNITVPlayerContext *tvCon = new JNITVPlayerContext();
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) 
    {
        TV_LOG("libbcmtv_jni: GetEnv failed");
        return result;
    }
    assert(env != NULL);
        
    TV_LOG("libbcmtv_jni: GetEnv succeeded!!");
    if (registerNativeMethods(env, "com/broadcom/tvarchive/BcmTVArchive_JNI", gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0)
    {
        TV_LOG("libbcmtv_jni: register interface error!!");
        return result;
    }
    TV_LOG("libbcmtv_jni: register interface succeeded!!");

    result = JNI_VERSION_1_4;

	// Initiate a channel scan
	TV_LOG("libbcmtv_jni: Initiating a channel scan!!");
	j_con = tvCon;

#ifdef ANDROID_USES_ARTEMIS
	// Check if we need to initiate a rescan
	if (NeedToScan())
		tvCon->StartScan();
#else
	tvCon->StartScan();
#endif

    return result;
}

bool NeedToScan()
{
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	ITV::ChannelList cl;
	bool bRet = false;

	// Get channel list
    cl = pCon->getChannelList();

	if (cl.size() == 0)
	{
		TV_LOG("libbcmtv_jni: Needs a rescan!!");
		bRet = true;
	}

	else
	{
		TV_LOG("libbcmtv_jni: Channels have already been scanned!!");
	}
	
	return bRet;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setMode(JNIEnv *env, jobject obj, jboolean bIsFCCOn)
{
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;

#ifdef DISABLE_FCC
	pCon->bFCCMode = false;
#else
	// Check if we need to enable FCC
	if (bIsFCCOn)
		pCon->bFCCMode = true;

	// or disable it
	else
		pCon->bFCCMode = false;
#endif

	TV_LOG("JNI_setMode: FCC_Mode = %d", pCon->bFCCMode);

	return 0;
}

JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_getChannelList(JNIEnv *env, jobject obj, jint iNum)
{
	TV_LOG("JNI_getChannelList: Enter");
    jobjectArray retArray;
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	char szChannelInfo[255];
	int i;
	Ch_List *pTemp;

	// Get channel list
    pCon->channelList = pCon->getChannelList();
	pCon->iNumChannels = pCon->channelList.size();

	if (pCon->iNumChannels <= 0)
	{
		TV_LOG("JNI_getChannelList: Failed to lookup any channels!!");
		return NULL;
	}

    TV_LOG("JNI_getChannelList: Channel_Size = %d", pCon->iNumChannels);

	// Assign the head pointer
	pHead = (Ch_List *)malloc(sizeof(Ch_List));
	pCur = pHead;

    retArray = (jobjectArray)env->NewObjectArray(pCon->channelList.size(), env->FindClass("java/lang/String"), env->NewStringUTF(""));  

    for (i = 0; i < pCon->iNumChannels; i++) 
    {
        TV_LOG("Channel ID: %s - %s (freq = %d: progId = %d: Provider = %s)\n", pCon->channelList[i].getId().c_str(), pCon->channelList[i].getName().c_str(), pCon->channelList[i].getTuningParamsId(), pCon->channelList[i].getProgId(), pCon->channelList[i].getProviderName().c_str());
//		sprintf(szChannelInfo, "Program #%d (Channel ID: %s)", pCon->channelList[i].getProgId(), pCon->channelList[i].getId().c_str());
		sprintf(szChannelInfo, "Program #%d (%s)", pCon->channelList[i].getProgId(), pCon->channelList[i].getName().c_str());
        env->SetObjectArrayElement(retArray, i, env->NewStringUTF(szChannelInfo));

		// Populate up the link list
		pCur->iChannel = i;

		pCur->pNext = (Ch_List *)malloc(sizeof(Ch_List));
		pCur->pBack = (Ch_List *)malloc(sizeof(Ch_List));

		pLast = pCur;
		pCur = pCur->pNext;
    }

	// Complete the cirular list
	pLast->pNext = pHead;
	pHead->pBack = pLast;

	// Assign the back pointers
	pCur = pHead->pNext;
	pTemp = pHead;
    for (i = 1; i < pCon->iNumChannels; i++)
	{
		pCur->pBack = pTemp;

		pTemp = pTemp->pNext;
		pCur =  pCur->pNext;
	}

#ifdef DEBUG_JNI
	// Print the next pointers (few more than max, just to check the link list)
	TV_LOG("JNI_getChannelList: **** Front Channels ****", pCur->iChannel);
	pCur = pHead;
    for (i = 0; i < pCon->iNumChannels + 4; i++)
	{
		pCur = pCur->pNext;
		TV_LOG("JNI_getChannelList: iChannel_Next = %d", pCur->iChannel);
	}

	// Print the back pointers (few more than max, just to check the link list)
	pCur = pHead;
	TV_LOG("JNI_getChannelList: **** Back Channels ****", pCur->iChannel);
    for (i = 0; i < pCon->iNumChannels + 4; i++)
	{
		TV_LOG("JNI_getChannelList: iChannel_Back = %d", pCur->pBack->iChannel);
		pCur = pCur->pNext;
	}
#endif

	// Reset the channel #
	pCon->_currentChannel = 0;

	// Call the media proxy library
	TV_LOG("JNI_getChannelList: Calling IBroadcastSource init()");
	Initializer<Trellis::Media::IBroadcastSource>::init();

	TV_LOG("JNI_getChannelList: Calling IQamSource init()");
	Initializer<Trellis::Media::IQamSource>::init();

	// Setup FCC related stuff
	if (pCon->iNumPrimers > pCon->iNumChannels)
		pCon->iNumPrimers = pCon->iNumChannels;

#ifndef DISABLE_FCC
	TV_LOG("JNI_getChannelList (FCC): primers = %d", pCon->iNumPrimers);

	// Initialize the media players
    for (i = 0; i < pCon->iNumPrimers; i++)
	{
        j_mp_fcc[i] = (IMediaPlayer *)Trellis::Media::IMediaPlayer::resolve(SVC_NAME);
	}

	// Start prime based on the requested channel #
	merge_prime_and_new_lists(iNum);
#endif

	pCon->bPrimesReleased = false;
	pCon->bRePrime = false;

	// Save the global reference (non-FCC)
	j_mp_main = (IMediaPlayer *)Trellis::Media::IMediaPlayer::resolve(SVC_NAME);

	TV_LOG("JNI_getChannelList: Exit");
    return retArray;
}

JNIEXPORT jboolean JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setChannel(JNIEnv *env, jobject obj, jint iNum)
{
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	IMediaPlayer *mediaPlayer;
	int i;
	bool bToPrime = true;

	TV_LOG("JNI_setChannel: Enter (iNum = %d)", iNum);

	// Is FCC enabled?
	if (pCon->bFCCMode)
	{
		int i = 0;
		bool bFound = false;

		// First up, make sure there wasn't a mode change
		if (!pCon->bRePrime)
		{
			// Try looking for the channel of choice
			while (i < pCon->iNumPrimers)
			{
				// break out if we found a match
				if (pCon->iPrimedChannelList[i] == iNum)
				{
					bFound = true;
					break;
				}

				i++;
			}
		}

		if (bFound)
		{
			TV_LOG("JNI_setChannel: Found the right channel, will start playback!!");

			// Start playback immediately
			mediaPlayer = (IMediaPlayer *)j_mp_fcc[i];
			mediaPlayer->start();

			// Save the playback index
			pCon->iPrimedIndex = i;
		}

		else
		{
			TV_LOG("JNI_setChannel: Couldn't detect the right channel (will reprime)!!");
			merge_prime_and_new_lists((int)iNum);

			// Now, start playback
			mediaPlayer = (IMediaPlayer *)j_mp_fcc[pCon->iPrimedIndex];
			TV_LOG("JNI_setChannel: Starting playback for channel %d on index %d for %p", iNum, pCon->iPrimedIndex, mediaPlayer);

			mediaPlayer->start();

			// Notify the app that we don't need to reprime
			bToPrime = false;
		}
	}

	else
	{
#ifndef DISABLE_FCC
		// First, reset all the previously primed channels
		for (i = 0; i < pCon->iNumPrimers; i++)
		{
			TV_LOG("reprime: Resetting previously primed channel #%d...", pCon->iPrimedChannelList[i]);
			mediaPlayer = (IMediaPlayer *)j_mp_fcc[i];

			if (mediaPlayer)
			{
				pCon->bRePrime = true;
				mediaPlayer->reset();
			}

			// Reset the primed list as well
			pCon->iPrimedChannelList[i] = -1;
		}
#endif
		// Select the required channel
		ITV::Channel selectedChannel = pCon->channelList[iNum];
		TV_LOG("JNI_setChannel: selectedChannel-ID:  %s!!", selectedChannel.getId().c_str());

		const IMedia::MediaItem mediaItem = pCon->_channel->getChannel(selectedChannel.getId());
		TV_LOG("JNI_setChannel: mediaItem retrieved");

		if (j_mp_main == NULL)
		{
			TV_LOG("JNI_setChannel: Reinitializing j_mp_main");
			j_mp_main = (IMediaPlayer *)Trellis::Media::IMediaPlayer::resolve(SVC_NAME);
		}

		TV_LOG("JNI_setChannel: Calling setDataSource!!");
		mediaPlayer = (IMediaPlayer *)j_mp_main;
		mediaPlayer->setDataSource(mediaItem);

		mediaPlayer->prepare();
		mediaPlayer->start();
	}

	TV_LOG("JNI_setChannel: Exit");
	return bToPrime;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_stop(JNIEnv *env, jobject obj)
{
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	IMediaPlayer *mediaPlayer;

	TV_LOG("JNI_stop: Enter");
	
	// Is FCC enabled?
	if (pCon->bFCCMode)
	{
		// Retrieve from the global reference
		TV_LOG("JNI_stop: stoping playback (fcc)...");
		mediaPlayer = (IMediaPlayer *)j_mp_fcc[pCon->iPrimedIndex];
        mediaPlayer->stop();
		TV_LOG("JNI_stop: stop_fcc succeeded!!");
	}

	else
	{
		// Retrieve from the global reference
		TV_LOG("JNI_stop: stoping playback...");
		mediaPlayer = (IMediaPlayer *)j_mp_main;
		mediaPlayer->stop();
		TV_LOG("JNI_stop: stop succeeded!!");

#ifdef ANDROID_USES_ARTEMIS
		if (j_mp_main != NULL)
		{
			Orb::release(j_mp_main);
			j_mp_main = NULL;
			TV_LOG("JNI_close: Released the media player!!");
		}
#endif
	}

	TV_LOG("JNI_stop: Exit");
	return 0;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_setChannel_pip(JNIEnv *env, jobject obj, jint iNum, jint x, jint y, jint w, jint h)
{
#ifndef DISABLE_PIP
	TV_LOG("JNI_setChannel_pip: Enter (iNum = %d)", iNum);

	IMedia::MediaItem mediaItem;
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;

	// Save the pip reference
	j_mp_pip = (IMediaPlayer *)Trellis::Media::IMediaPlayer::resolve(SVC_NAME);	

	// Select the required channel
	TV_LOG("JNI_setChannel_pip: Setting up the channel...");
	ITV::Channel selectedChannel = pCon->channelList[iNum];

	// Create the mediaItem
	mediaItem = pCon->_channel->getChannel(selectedChannel.getId());
	TV_LOG("JNI_setChannel: mediaItem retrieved");

	// Make sure we process video only for PIP
	TV_LOG("JNI_setChannel_pip: Setting decoder type to VIDEO_DECODER_PIP...");
	mediaItem.setDecoderType(IMedia::VIDEO_DECODER_PIP);

	// Store in the local reference
	TV_LOG("JNI_setChannel_pip: Deriving from global reference...");
	IMediaPlayer *mediaPlayer = (IMediaPlayer *)j_mp_pip;

	// Specify a new position (based on the input params)
	TV_LOG("JNI_setChannel_pip: positioninig at %dx%d with size %dx%d...", x, y, w, h);
	mediaPlayer->setVideoWindowPosition(x, y, w, h);

	TV_LOG("JNI_setChannel_pip: Calling setDataSource...");
	mediaPlayer->setDataSource(mediaItem);

	TV_LOG("JNI_setChannel_pip: Calling prepare...");
	mediaPlayer->prepare();

	TV_LOG("JNI_setChannel_pip: Calling start...");
	mediaPlayer->start();

	TV_LOG("JNI_setChannel_pip: Exit");
#endif
	return 0;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_stop_pip(JNIEnv *env, jobject obj)
{
#ifndef DISABLE_PIP
	TV_LOG("JNI_stop_pip: Enter");

	// Retrieve from the global reference
	IMediaPlayer *mediaPlayer = (IMediaPlayer *)j_mp_pip;
	mediaPlayer->stop();
	TV_LOG("JNI_stop_pip: stop succeeded!!");

	Orb::release(j_mp_pip);
	j_mp_pip = NULL;
	TV_LOG("JNI_stop_pip: Released the media player for pip!!");

	TV_LOG("JNI_stop_pip: Exit");
#endif
	return 0;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_reprime(JNIEnv *env, jobject obj, jint iNum)
{
#ifndef DISABLE_FCC
	TV_LOG("JNI_reprime: Enter (iNum = %d)", iNum);

	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	merge_prime_and_new_lists(iNum);
#endif

	return 0;
}

void merge_prime_and_new_lists(int iNum)
{
#ifndef DISABLE_FCC
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
    IMediaPlayer *mp_primer;
    int i, j = 0, k, iTmp, iAfter, iBefore;
	ITV::Channel selectedChannel;
	int iShortList[MAX_PRIMERS], iNewList[MAX_PRIMERS], iSkipList[MAX_PRIMERS];
	bool bPrimeFound = false;
	IMedia::MediaItem mediaItem;

	// Reset the skip & short lists
	memset(iShortList, -1, (MAX_PRIMERS * sizeof(int)));
	memset(iSkipList, -1, (MAX_PRIMERS * sizeof(int)));
	
	// Initialize stuff
	pCur = pHead;

	// Loop thru the list
    for (i = 0; i < pCon->iNumChannels; i++)
	{
        // Did we find our new channel?
        if (pCur->iChannel == iNum)
        {
			iNewList[0] = iNum;

			// Compute the prefix channel #
			iBefore = pCur->pBack->iChannel;
			iNewList[1] = iBefore;

			// Same with the suffixes
			pCur = pCur->pNext;
			iAfter = pCur->iChannel;
			j = 2;

			TV_LOG("merge_prime_and_new_lists: iBefore = %d", iBefore, iAfter);

			// Populate the primed array
			while (j < MAX_PRIMERS)
			{
				iNewList[j] = iAfter;
				TV_LOG("merge_prime_and_new_lists: iAfter = %d", iAfter);

				j++;
				pCur = pCur->pNext;
				iAfter = pCur->iChannel;
			}

			break;
        }
        
        pCur = pCur->pNext;
	}

	for (i = 0, k = 0; i < pCon->iNumPrimers; i++)
	{
		iTmp = iNewList[i];
		bPrimeFound = false;

		for (j = 0; j < pCon->iNumPrimers; j++)
		{
			if (iTmp == pCon->iPrimedChannelList[j])
			{
				bPrimeFound = true;
				iSkipList[j] = 1;
				TV_LOG("merge_prime_and_new_lists: Already primed = %d", iTmp);
				break;
			}
		}

		if (!bPrimeFound)
		{
			TV_LOG("merge_prime_and_new_lists: Found non-primed = %d", iTmp);
			iShortList[k] = iTmp;
			k++;
		}
	}

	for (i = 0, k = 0; i < pCon->iNumPrimers; i++)
	{
		if (iSkipList[i] == -1)
		{
			iTmp = iShortList[k];
			k++;
			TV_LOG("merge_prime_and_new_lists: Adding prime = %d (i = %d)", iTmp, i);

			mp_primer = (IMediaPlayer *)j_mp_fcc[i];

			TV_LOG("merge_prime_and_new_lists: Calling reset");
			mp_primer->reset();

			selectedChannel = pCon->channelList[iTmp];
			mediaItem = pCon->_channel->getChannel(selectedChannel.getId());
			TV_LOG("merge_prime_and_new_lists: mediaItem retrieved");

			mediaItem.setPrimingMode(true);
			mp_primer->setDataSource(mediaItem);
			TV_LOG("merge_prime_and_new_lists: setDataSource called");

			mp_primer->prepare();

			if (iTmp == iNum)
				pCon->iPrimedIndex = i;

			pCon->iPrimedChannelList[i] = iTmp;
		}
	}

	// Finally, reset this flag
	pCon->bRePrime = false;

#ifdef DEBUG_JNI
	for (i = 0; i < pCon->iNumPrimers; i++)
	{
		TV_LOG("merge_prime_and_new_lists: Currently primed = %d on index %d with %p", pCon->iPrimedChannelList[i], i, (IMediaPlayer *)j_mp_fcc[i]);
	}
#endif
#endif
}

JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_getPrimedList(JNIEnv *env, jobject obj)
{
#ifndef DISABLE_FCC
    jobjectArray retArray;
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	char szPrimeInfo[255];
	int i;

	TV_LOG("getPrimedList: Enter");
    retArray = (jobjectArray)env->NewObjectArray(255, env->FindClass("java/lang/String"), env->NewStringUTF(""));  

    for (i = 0; i < pCon->iNumPrimers; i++) 
    {
		sprintf(szPrimeInfo, "#%d ", pCon->iPrimedChannelList[i] + 1);
		TV_LOG("%s", szPrimeInfo);
        env->SetObjectArrayElement(retArray, i, env->NewStringUTF(szPrimeInfo));
	}
	return retArray;
#else
	return NULL;
#endif
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_force_scan(JNIEnv *env, jobject obj)
{
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;

	// Kick off a new scan
	TV_LOG("JNI_force_scan: Forcing a scan!!");
	pCon->StartScan();

	return 0;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvarchive_BcmTVArchive_JNI_close(JNIEnv *env, jobject obj)
{
	JNITVPlayerContext *pCon = (JNITVPlayerContext *)j_con;
	IMediaPlayer *mp_primer;
	int i;

	TV_LOG("JNI_close: Will release the media player!!");

	// Release the media player
	if (j_mp_main != NULL)
	{
		Orb::release(j_mp_main);
		j_mp_main = NULL;
		TV_LOG("JNI_close: Released the media player!!");
	}

	// Release the media player for pip as well
	if (j_mp_pip != NULL)
	{
		Orb::release(j_mp_pip);
		j_mp_pip = NULL;
		TV_LOG("JNI_close: Released the media player for pip!!");
	}

	// Finally, release the FCC players
	if (!pCon->bPrimesReleased)
	{
		for (i = 0; i < pCon->iNumPrimers; i++)
		{
			TV_LOG("JNI_close: Resetting previously primed channel #%d...", pCon->iPrimedChannelList[i]);
			mp_primer = (IMediaPlayer *)j_mp_fcc[i];

			if (mp_primer)
			{
				mp_primer->reset();
				Orb::release(mp_primer);
				pCon->bPrimesReleased = true;
			}
		}
	}

	return 0;
}

/*
	// Need this when we're handling player exit
	// Release everything that was resolved
	Orb::release(j_guide);
	j_guide = NULL;

	Orb::release(j_channel);
	j_channel = NULL;

	Orb::release(j_config);
	j_config = NULL;
*/
