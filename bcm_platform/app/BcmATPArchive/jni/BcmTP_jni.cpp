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
#include "IMediaPlayer.h"
#include "IMediaPlayerAdaptor.h"

using namespace Trellis;
using namespace Trellis::Media;

// All globals must be JNI primitives, any other data type 
// will fail to hold its value across different JNI methods
void *j_mp, *j_mContext;
jboolean g_bPlaying = false, g_bOnEOS = false;

extern "C" void initOrb(IApplicationContainer* app);

static jint JNICALL Java_com_broadcom_atparchive_BcmATPArchive_JNI_setDataSource(JNIEnv *env, jobject obj, jstring uri);
static jint JNICALL Java_com_broadcom_atparchive_BcmATPArchive_JNI_stop(JNIEnv *env, jobject obj);
static jint JNICALL Java_com_broadcom_atparchive_BcmATPArchive_JNI_getPlaybackStatus(JNIEnv *env, jobject obj);

static JNINativeMethod gMethods[] = 
{
	{"setDataSource", "(Ljava/lang/String;)I", (void *)Java_com_broadcom_atparchive_BcmATPArchive_JNI_setDataSource},
	{"stop", "()I", (void *)Java_com_broadcom_atparchive_BcmATPArchive_JNI_stop},
	{"getPlaybackStatus", "()I", (void *)Java_com_broadcom_atparchive_BcmATPArchive_JNI_getPlaybackStatus},
};

class JNIMediaPlayerContext : public StandaloneApplication
{
public:
    class LocalParameters
	{
	public:
		LocalParameters(Util::Param& p) {
            p.add("client", "MediaService");
		}
    };

    class LibraryLoader
    {
public:
	   LibraryLoader(const Util::Param& params) {
#if 0
            if (params.get("resolve") == "local") {
                Broker::registerServerLib<IFileSource>();
                Broker::registerServerLib<INetworkSource>();
                Broker::registerServerLib<IBroadcastSource>();
                Broker::registerServerLib<IQamSource>();
                Broker::registerServerLib<IOfdmSource>();
                Broker::registerServerLib<IMediaPlayer>();
                Broker::registerServerLib<IMediaRecorder>();
                Broker::registerServerLib<IStreamExtractor>();
                Broker::registerServerLib<IMediaStreamer>();
            }
#endif
        }
    };

    JNIMediaPlayerContext() :
        StandaloneApplication(0, NULL),
        localParameters(getParameters()),
        libraryLoader(getParameters()) ,
		_mediaPlayerListener(*this)
	{
		ALOGE("libbcmtp_jni: Calling StandaloneApplication::init");
	    StandaloneApplication::init();
		ALOGE("libbcmtp_jni: Calling initOrb");
        initOrb(this);
		ALOGE("libbcmtp_jni: Out of JNIMediaPlayerContext");
    }

	virtual void onCompletion()
	{
		ALOGE("libbcmtp_jni: onCompletion");
		g_bOnEOS = true;
    }

    virtual void onError(const IMedia::ErrorType  & errorType)
	{
		ALOGE("libbcmtp_jni: onError");
    }

    virtual void onPrepared()
	{
		ALOGE("libbcmtp_jni: onPrepared");
    }

    virtual void onInfo(const IMedia::InfoType  & infoType, int32_t extra)
	{
		ALOGE("libbcmtp_jni: onInfo");
    }
	
    virtual void onSeekComplete()
	{
		ALOGE("libbcmtp_jni: onSeekComplete");
    }

    virtual void onVideoSizeChanged(uint16_t width, uint16_t height)   
	{
		ALOGE("libbcmtp_jni: onVideoSizeChanged");
	}

	MediaPlayerIListenerAdaptor<JNIMediaPlayerContext> _mediaPlayerListener;
	JNIEnv		*m_env;
	jobject		m_obj;

private:
	LocalParameters localParameters;
    LibraryLoader libraryLoader;
};

static int registerNativeMethods(JNIEnv* env, const char* className, JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) 
    {
        ALOGE("libbcmtp_jni: Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }

    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) 
    {
        ALOGE("libbcmtp_jni: RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	ALOGE("libbcmtp_jni: Creating JNIMediaPlayerContext object");
	static JNIMediaPlayerContext *mContext = new JNIMediaPlayerContext();
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) 
    {
        ALOGE("libbcmtp_jni: GetEnv failed");
        return result;
    }
    assert(env != NULL);
        
    ALOGE("libbcmtp_jni: GetEnv succeeded!!");
    if (registerNativeMethods(env, "com/broadcom/atparchive/BcmATPArchive_JNI", gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0)
    {
        ALOGE("libbcmtp_jni: register interface error!!");
        return result;
    }
    ALOGE("libbcmtp_jni: register interface succeeded!!");

    result = JNI_VERSION_1_4;

	// Save the context
	j_mContext = mContext;

    return result;
}

JNIEXPORT jint JNICALL Java_com_broadcom_atparchive_BcmATPArchive_JNI_setDataSource(JNIEnv *env, jobject obj, jstring uri)
{
	jboolean isCopy;
	jbyte *nativeBytes;
	
	ALOGE("JNI_setDataSource: Enter");

	nativeBytes = (jbyte *)env->GetStringUTFChars(uri, &isCopy);

	// Print the uri string
	if (nativeBytes != NULL)
	{
		ALOGE("JNI_setDataSource: uri is valid!!");
		ALOGE("JNI_setDataSource: uri = %s, strlen = %d", nativeBytes, strlen((const char *)nativeBytes));
	}
	else
		ALOGE("JNI_setDataSource: uri is NULL!!");

	// Reset the playback flag
	g_bPlaying	= false;
	g_bOnEOS	= false;

	std::string trellis_url((const char *)nativeBytes);
	ALOGE("JNI_setDataSource: trellis_url = %s!!", trellis_url.c_str());

	// Call the media proxy library
	ALOGE("JNI_setDataSource: Calling init()");
	Initializer<IMediaPlayer>::init();
	ALOGE("JNI_setDataSource: Done with init()");

	// Use the global reference
	ALOGE("JNI_setDataSource: Calling resolve()!!");
	j_mp = (IMediaPlayer *)IMediaPlayer::resolve(Orb::ServiceInfo("MediaService"));
	ALOGE("JNI_setDataSource: mediaPlayer object created!!");

	// Store in the local reference
	IMediaPlayer *mediaPlayer;
	mediaPlayer = (IMediaPlayer *)j_mp;

	// Add the listener
	ALOGE("JNI_setDataSource: Registering the listener!!");
	JNIMediaPlayerContext *mTemp = (JNIMediaPlayerContext *)j_mContext;
	mediaPlayer->addListener(&mTemp->_mediaPlayerListener);

	// Save the fields in the class
	mTemp->m_env = env;
	mTemp->m_obj = obj;

	IMedia::NetworkMediaItem mediaItem;
	mediaItem.setUri(trellis_url);
	ALOGE("JNI_setDataSource: setUri succeeded!!");

	IMedia::StreamMetadata metadata(IMedia::AutoStreamType);
	ALOGE("JNI_setDataSource: setStreamMetadata succeeded!!");

	mediaItem.setStreamMetadata(metadata);
	ALOGE("JNI_setDataSource: setStreamMetadata succeeded!!");

	mediaPlayer->setDataSource(mediaItem);
	ALOGE("JNI_setDataSource: setDataSource succeeded!!");

	mediaPlayer->prepare();
	ALOGE("JNI_setDataSource: prepare succeeded!!");

	mediaPlayer->start();
	ALOGE("JNI_setDataSource: start succeeded!!");

	// Set the playback flag
	g_bPlaying = true;

	env->ReleaseStringUTFChars(uri, (const char *)nativeBytes);

	ALOGE("JNI_setDataSource: Exit");
	return 0;
}


JNIEXPORT jint JNICALL Java_com_broadcom_atparchive_BcmATPArchive_JNI_stop(JNIEnv *env, jobject obj)
{
	ALOGE("JNI_stop: Enter");

	// Retrieve from the global reference
	IMediaPlayer *mediaPlayer = (IMediaPlayer *)j_mp;
	mediaPlayer->stop();
	ALOGE("JNI_stop: stop succeeded!!");

	// Release the media player
	Orb::release(j_mp);
	j_mp = NULL;
	ALOGE("JNI_stop: Released the media player!!");

	// Reset the playback flag
	g_bPlaying = false;

	ALOGE("JNI_stop: Exit");
	return 0;
}

JNIEXPORT jint JNICALL Java_com_broadcom_atparchive_BcmATPArchive_JNI_getPlaybackStatus(JNIEnv *env, jobject obj)
{
	int iRet = -1;

	ALOGE("JNI_getPlaybackStatus: Enter");

	if (g_bPlaying)
	{
		while(!g_bOnEOS)
		{
			sleep(1);
		}
	}

	if (g_bOnEOS)
		iRet = 0;

	ALOGE("JNI_getPlaybackStatus: Exit");
	return iRet;
}
