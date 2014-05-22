/******************************************************************************
 *    (c)2011 Broadcom Corporation
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
 * $brcm_Workfile: RemoteServer.cpp $
 * $brcm_Revision: 5 $
 * $brcm_Date: 10/18/11 6:11p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/broadcom/bmc/framework/src/RemoteServer.cpp $
 * 
 * 5   10/18/11 6:11p lamj
 * SW7425-1181: Websocket API change fix.
 * 
 * 4   9/7/11 7:32p leventa
 * SW7425-1077: IBC code checkin
 * 
 * 2   8/23/11 3:54p lamj
 * SW7425-1181: Manually service loop instead of fork.
 * 
 * 1   8/23/11 10:58a lamj
 * SW7425-1181: Initial remote UI check in.
 * 
 *****************************************************************************/

#ifdef ANDROID
#include "AndroidRemoteServer.h"
#endif

extern "C" 
{
	#include "libwebsockets.h"
}
#include "RemoteServer.h"

#ifdef ANDROID
#else
#include "Application.h"
#include "JSHelper.h"
#include "ViewManager.h"
#include "MediaManager.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>




#ifdef ANDROID
#else
using namespace Broadcom;
#endif

enum demo_protocols {
	/* always first */
	PROTOCOL_HTTP = 0,
	PROTOCOL_REMOTE,
	/* always last */
	PROTOCOL_DIAGNOSTIC,
	PROTOCOL_STATUS,
	DEMO_PROTOCOL_COUNT
};
struct libwebsocket_protocols protocols[5];
int constCount = 0;


#ifdef ANDROID
AndroidRemoteServer	*androidRemoteIns = NULL;
#endif

/*
protocols[PROTOCOL_HTTP].name = "http-only";
protocols[PROTOCOL_HTTP].callback =  callback_http;
*/
/*
static struct libwebsocket_protocols protocols[] = {
	[PROTOCOL_HTTP] = {
		.name = "http-only",
		.callback = callback_http,
	},
	[DEMO_PROTOCOL_COUNT] = {  
		.callback = NULL
	}
}
*/




static int callback_http(struct libwebsocket_context * cont,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void* user,
							   void* in, size_t len)
{
	char client_name[128];
	char client_ip[128];

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		fprintf(stderr, "serving HTTP URI %s\n", (char *)in);

		if ((char *) in && strcmp((char *) in, "/favicon.ico") == 0) {
			if (libwebsockets_serve_http_file(wsi,
			     "./favicon.ico", "image/x-icon"))
				fprintf(stderr, "Failed to send favicon\n");
			break;
		}

		/* send the script... when it runs it'll start websockets */

		
		if (libwebsockets_serve_http_file(wsi,
				  "./ws_home.html", "text/html"))
			fprintf(stderr, "Failed to send HTTP file\n");
		break;

	/*
	 * callback for confirming to continue with client IP appear in
	 * protocol 0 callback since no websocket protocol has been agreed
	 * yet.  You can just ignore this if you won't filter on client IP
	 * since the default uhandled callback return is 0 meaning let the
	 * connection continue.
	 */

	case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:

		libwebsockets_get_peer_addresses((int)(long)user, client_name,
			     sizeof(client_name), client_ip, sizeof(client_ip));

		fprintf(stderr, "Received network connect from %s (%s)\n",
							client_name, client_ip);

		/* if we returned non-zero from here, we kill the connection */
		break;

	default:
		break;
	}

	return 0;
}

#ifdef ANDROID
#else
void handleInput(void* in)
{
	printf("\n \n \n Message recieved: %s ;\n \n \n", in);
	Application& app = Application::Instance();
		 DFBEvent e;
		 e.clazz = DFEC_INPUT;
		 e.input.type = DIET_KEYPRESS;
		 e.input.flags = DIEF_NONE;
		  e.input.key_symbol = DIKS_NULL;
		  e.input.key_id = DIKI_UNKNOWN;
	
	if(strcmp((char *) in, "0") == 0)
		e.input.key_id = DIKI_UP;
	if(strcmp((char *) in, "1") == 0)
		 e.input.key_id = DIKI_DOWN;
	if(strcmp((char *) in, "2") == 0)
		e.input.key_id = DIKI_LEFT;
	if(strcmp((char *) in, "3") == 0)
		e.input.key_id = DIKI_RIGHT;
	if(strcmp((char *) in, "4") == 0)
		 e.input.key_id = DIKI_ENTER;	 
	if(strcmp((char *) in, "5") == 0) 
		e.input.key_symbol = DIKS_MENU;	
	if(strcmp((char *) in, "6") == 0)
		e.input.key_symbol = DIKS_EPG;
	if(strcmp((char *) in, "7") == 0) 
		e.input.key_symbol = DIKS_INFO;
	if(strcmp((char *) in, "8") == 0) 
		e.input.key_symbol = DIKS_CHANNEL_UP;
	if(strcmp((char *) in, "9") == 0) 
		e.input.key_symbol = DIKS_CHANNEL_DOWN;
	if(strcmp((char *) in, "10") == 0)
		 e.input.key_symbol = DIKS_STOP;
	if(strcmp((char *) in, "11") == 0)
		 e.input.key_symbol = DIKS_BACK;
	if(strcmp((char *) in, "12") == 0)
		 e.input.key_symbol = DIKS_FORWARD;
	if(strcmp((char *) in, "13") == 0)
		 e.input.key_symbol = DIKS_PLAY;
	if(strcmp((char *) in, "14") == 0)
		 e.input.key_symbol = DIKS_STOP;
	if(strcmp((char *) in, "15") == 0)
		 e.input.key_symbol =  DIKS_REWIND;
	if(strcmp((char *) in, "16") == 0)
		 e.input.key_symbol = DIKS_FORWARD;
	if(strcmp((char *) in, "17") == 0)
		 e.input.key_symbol = DIKS_PAUSE;
	if(strcmp((char *) in, "mouse") == 0) {
		printf("mouse event caught...posting event ...\n");
		 //app.PostUserEvent(UIEvent_Move, NULL);
		 e.input.type = DIET_AXISMOTION;
		 //e.input.type = DIET_BUTTONPRESS;
		 e.input.axis = DIAI_X;
		 e.input.flags = DFBInputEventFlags(DIEF_AXISABS | DIEF_MAX | DIEF_MIN) ;
		 e.input.axisabs = 100;
		 //e.input.axisrel = 100;
		 e.input.min = 0;
		 e.input.max = 1280;
		 e.input.device_id = DIDID_MOUSE;
		 //e.input.type = UIEvent_Move;
		 //e.clazz = DFEC_USER;
		char * payload = (char *) in;
		memset(in, 0, sizeof(char) * strlen(payload));
		 app.GetEventBuffer()->PostEvent(app.GetEventBuffer(), &e);
		 return;
		 }
	
	
	
	char * payload = (char *) in;
	memset(in, 0, sizeof(char) * strlen(payload));
	

	app.GetEventBuffer()->PostEvent(app.GetEventBuffer(), &e);

	
	e.input.type = DIET_KEYRELEASE;
	app.GetEventBuffer()->PostEvent(app.GetEventBuffer(), &e);
	e.input.key_id = DIKI_UNKNOWN;
	e.input.key_symbol = DIKS_NULL;
}
#endif

static int callback_remoteInput(struct libwebsocket_context * cont,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void* user,
							   void* in, size_t len)
{
	char client_name[128];
	char client_ip[128];

	switch (reason) {
	case LWS_CALLBACK_RECEIVE:
		if(strcmp((char *) in, "C") == 0) {
			printf("input\n");
			libwebsocket_callback_on_writable(cont, wsi);
			break;
			}
#ifdef ANDROID
		androidRemoteIns->handleInput(in, len);
#else
		handleInput(in);	
#endif		
		memset(in, 0, sizeof(char) * len);
		
	
		break;

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		//dump_handshake_info((struct lws_tokens *)(long)user);
		/* you could return non-zero here and kill the connection */
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
		int status;
		char data[LWS_SEND_BUFFER_PRE_PADDING + 5 + LWS_SEND_BUFFER_POST_PADDING];
		
		data[LWS_SEND_BUFFER_POST_PADDING] = 'm';
		data[LWS_SEND_BUFFER_POST_PADDING + 1] = 's';
		data[LWS_SEND_BUFFER_POST_PADDING + 2] = 'g';
		data[LWS_SEND_BUFFER_POST_PADDING + 3] = 's';
		//int convertSoftdesicionDataIntoChar(softDecision);

		status = libwebsocket_write(wsi, (unsigned char*) data  , 5, LWS_WRITE_TEXT );
		if (status < 0) {
			fprintf(stderr, "ERROR writing to socket");
			return 1;
		}
	
		if(constCount < 2) {
		constCount++;
		libwebsocket_callback_on_writable(cont, wsi);
		} else {
			constCount = 0;
		}
		break;
	}
	default:
		break;
	}

	return 0;
}

struct per_session_diagnostic {
	int frontendNo;
};


static int callback_diagnostic(struct libwebsocket_context * cont,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void* user,
							   void* in, size_t len)
{

	struct per_session_diagnostic *pss = (per_session_diagnostic * ) user;
	int channelValue;

	switch (reason) {
	

	case LWS_CALLBACK_RECEIVE:
		channelValue = atoi((char *) in);
		pss->frontendNo = channelValue;
		if(channelValue < 8 && channelValue >= 0) {
			libwebsocket_callback_on_writable(cont, wsi);
			break;
		}
		
		if(strcmp((char *) in, "C") == 0) {
			libwebsocket_callback_on_writable(cont, wsi);
			break;
			}
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
			int status;
			char buffer[1000];
			char pair[20]; 

#ifdef ANDROID
		ALOGI( "LWS_CALLBACK_CLIENT_WRITEABLE not implemented yet");
#else
		
		int rndX;
		int rndY;
		
		BMedia_FrontendSoftDecision decisions[100];
		BMedia_FrontendSoftDecision *pDecisions = &decisions[0];
		
		for (int i = 0; i < 100; i++) {
			BMedia_FrontendSoftDecision *decision = &decisions[i];
			memset(&decision, 0, sizeof(BMedia_FrontendSoftDecision));
		}
		
		for(int i = 0; i < strlen(buffer); i++) {
			buffer[i] = 0;
		}
		BMedia_OpenSettings mediaSettings;
		memset(&mediaSettings, 0, sizeof(BMedia_OpenSettings));
		mediaSettings.frontendNo = pss->frontendNo;
		mediaSettings.streamerGetFrontendSoftDecision = StreamerGetFrontendSoftDecision;
		BMediaHandle rMedia;
		
		rMedia = BMedia_Open(&mediaSettings);

		
		BMedia_GetFrontendSoftDecisions(rMedia, 16, pDecisions);
		
		BMedia_Close(rMedia);
			unsigned int x, y;
		
		for(int i = 0; i < 16; i++) {
			//rndX = rand() % 100;
			//rndY = rand() % 100;

			x = pDecisions[i].i / 224;
			y = pDecisions[i].q / 224;
			x += 150;
			y += 150;
			
			snprintf(pair, 100, "%d,%d;", x, y);
			strcat(buffer, pair);

		}
		

		
		
		snprintf(pair, 100, "%d,%d;", rndX, rndY);
	
		int size = strlen(pair);
		
#endif


		
		status = libwebsocket_write(wsi, (unsigned char*) buffer  , strlen(buffer), LWS_WRITE_TEXT );
		if (status < 0) {
			fprintf(stderr, "ERROR writing to socket");
			return 1;
		}
		break;
		}
	default:
		break;
	
	}
	return 0;
}

struct per_session_data_status {
	int frontendNo;
};

static int callback_status(struct libwebsocket_context * cont,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void* user,
							   void* in, size_t len)
{

	struct per_session_data_status *pss = (per_session_data_status * ) user;
	int channelValue;
	switch (reason) {
	

	case LWS_CALLBACK_RECEIVE:
		channelValue = atoi((char *) in);
		pss->frontendNo = channelValue;
		if(channelValue < 8 && channelValue >= 0) {
			libwebsocket_callback_on_writable(cont, wsi);
			break;
		}
		if(strcmp((char *) in, "C") == 0) {
			libwebsocket_callback_on_writable(cont, wsi);
			break;
			}
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		{

		int webstatus;
		char buffer[1000];
		char pair[20]; 
#ifdef ANDROID
		ALOGI( "LWS_CALLBACK_CLIENT_WRITEABLE not implemented yet");
#else

		for(int i = 0; i < strlen(buffer); i++) {
			buffer[i] = 0;
		}
		BMedia_OpenSettings mediaSettings;
		memset(&mediaSettings, 0, sizeof(BMedia_OpenSettings));
		mediaSettings.frontendNo = pss->frontendNo;
		mediaSettings.streamerGetFrontendStatus = StreamerGetFrontendStatus;
		BMediaHandle rMedia;
		BMedia_FrontendStatus status;
		
		DlnaSrc src = DlnaSrc_eQam;
		mediaSettings.frontendMode = (int)src;
		
		rMedia = BMedia_Open(&mediaSettings);

		memset(&status, 0, sizeof(status));
		BMedia_GetFrontendStatus(rMedia, &status);

		BMedia_Close(rMedia);

		snprintf(buffer, 1000, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;", status.mode, (int) status.symbolRate, status.snrEstimate, status.reacquireCount,
			status.frequency, status.receiverLock, status.fecLock, status.timeElapsed, status.dsChannelPower, status.annex, pss->frontendNo );
		
		//printf("BMedia_GetFrontendStatus, %s\n", buffer);

#endif
		webstatus = libwebsocket_write(wsi, (unsigned char*) buffer  , strlen(buffer), LWS_WRITE_TEXT );
		if (webstatus < 0) {
			fprintf(stderr, "ERROR writing to socket");
			return 1;
		}
		break;
		}
	default:
		break;
	
	}
	return 0;
}


void *websocket_loop(void* arg)
{	
	int n = 0;
	struct libwebsocket_context *context;
	int port = 7681;
	int use_ssl = 0;
	int opts = 0;
	char interface_name[128] = "";
	int loopDelay = 10;
	const char * interface = NULL;
		unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 1024 +
						  LWS_SEND_BUFFER_POST_PADDING];
	
	
	buf[LWS_SEND_BUFFER_PRE_PADDING] = 'x';
	
	context = libwebsocket_create_context(port, NULL, protocols,libwebsocket_internal_extensions, NULL, NULL, -1, -1, 0);

	if (context == NULL) {
		fprintf(stderr, "libwebsocket init failed\n");
		return NULL;
	}

	/*
	n = libwebsockets_fork_service_loop( context);
	if (n < 0) {
		fprintf(stderr, "Unable to fork service loop %d\n", n);
		return NULL;
	}
	printf("websocket loop 3\n");
	libwebsocket_context_destroy(context);
	printf("websocket loop 4\n");
	*/
	
	while(1) {
	
		libwebsocket_service(context, loopDelay);

#ifdef ANDROID
		androidRemoteIns->loop(loopDelay);
#endif

	}
	
    return NULL;
}



RemoteServer::RemoteServer(void)
{
	_someCount = 0;
	
	struct libwebsocket_protocols first;
	first.name = "http-only";
	first.callback = callback_http;

	protocols[PROTOCOL_HTTP] = first;

	struct libwebsocket_protocols second;
	second.name = "remote-protocol";
	second.callback = callback_remoteInput;

	protocols[PROTOCOL_REMOTE] = second;

	struct libwebsocket_protocols fourth;
	fourth.name = "diagnostic-protocol";
fourth.callback = callback_diagnostic;
fourth.per_session_data_size = sizeof(struct per_session_diagnostic);

protocols[PROTOCOL_DIAGNOSTIC] = fourth;



struct libwebsocket_protocols fifth;
fifth.name = "status-protocol";
fifth.callback = callback_status;
fifth.per_session_data_size = sizeof(struct per_session_data_status);


protocols[PROTOCOL_STATUS] = fifth;

	struct libwebsocket_protocols third;
	third.callback = NULL;

	protocols[DEMO_PROTOCOL_COUNT] = third;

#ifdef ANDROID
	if(NULL == androidRemoteIns) androidRemoteIns = new AndroidRemoteServer();
#endif

}

RemoteServer::~RemoteServer(void)
{
#ifdef ANDROID
	if(NULL != androidRemoteIns){
		delete androidRemoteIns;
		androidRemoteIns = NULL;
	}
#endif
}

RemoteServer& RemoteServer::Instance(void)
{
	static RemoteServer instance;
	return instance;
}

void RemoteServer::Startup()
{
	pthread_t threads;
	srand((unsigned)time(0));
	
	
	/*	
*/	
	
	

	/* Thread to handle websocket requests */
	pthread_create( &threads, NULL, websocket_loop, NULL);
}


void RemoteServer::Shutdown(void)
{

}




