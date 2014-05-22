package com.broadcom.TrellisUI;

import org.jwebsocket.client.java.BaseWebSocketClient;
import org.jwebsocket.api.WebSocketClientListener;
import org.jwebsocket.api.WebSocketClientEvent;
import org.jwebsocket.kit.WebSocketException;
import org.jwebsocket.api.WebSocketPacket;
import org.jwebsocket.config.JWebSocketCommonConstants;
import org.jwebsocket.kit.WebSocketSubProtocol;
import org.jwebsocket.kit.RawPacket;

import android.os.Message;
import android.os.Handler;
import java.util.List;
import java.util.ArrayList;
import android.util.Log;
import javolution.util.FastList;

public class jws{
	
	private final  int MT_OPENED = 0;
	private final  int MT_PACKET = 1;
	private final  int MT_CLOSED = 2;
	private  List<WebSocketClientListener> mListeners = new FastList<WebSocketClientListener>();
	private TrellisUIWebsocketListener trellisListener;
	private  BaseWebSocketClient mJWS;
	//private  String lastMsg;
	private  List <String> lastMsg; 
    private static final String TAG = "JWS";
	private String mProcessOpened;
	private String mProcessPacket;
	private String mProcessClosed;
	private int state;
	private boolean discardMessage = false;
	
	
	jws(String processOpened, String processPacket, String processClosed){
		
		mJWS = new BaseWebSocketClient();
		mJWS.addListener(new Listener());
		mProcessOpened = processOpened;
		mProcessClosed = processClosed;
		mProcessPacket = processPacket;
		state = MT_CLOSED;
	}
	
	public  void open(String aURI) throws WebSocketException {
		if (state==MT_OPENED)
			return;
		lastMsg = new ArrayList <String> ();
		WebSocketSubProtocol mSubProt = new WebSocketSubProtocol(JWebSocketCommonConstants.WS_SUBPROT_DEFAULT, JWebSocketCommonConstants.WS_ENCODING_DEFAULT);
		mJWS.addSubProtocol(mSubProt); 
		//mJWS.open(aURI);
		mJWS.open(13,aURI,"remote-protocol");
		Log.v(TAG,"open"+aURI);
		state = MT_OPENED;
	
	}

	public  void send(WebSocketPacket aPacket) throws WebSocketException {
		mJWS.send(aPacket);
	}
	
	public  void send(String aString) throws WebSocketException {
		Log.v(TAG,aString);
		mJWS.send(aString, "UTF-8");
	}

	public  void close(){
		if (state==MT_CLOSED)
			return;
		mJWS.close();
		state = MT_CLOSED;
	}

	public  void addListener(WebSocketClientListener aListener) {
		mListeners.add(aListener);
	}

	public  void removeListener(WebSocketClientListener aListener) {
		mListeners.remove(aListener);
	}

	public void addTrellisListner(TrellisUIWebsocketListener aListener) {
		trellisListener = aListener;
	}

	public void discardMessage() {
		discardMessage = true;
	}

	public  String getMessage() {
		if(lastMsg.size() != 0)
		{
			//return lastMsg.get(lastMsg.size()-1);
			return lastMsg.remove(0);
		}
		else return "error";
//		return lastMsg;
	}

	public String getProcessOpenedHandler() {
		return mProcessOpened;
	}

	private  Handler messageHandler = new Handler() {

		@Override
		public void handleMessage(Message aMessage) {

			switch (aMessage.what) {
				case MT_OPENED:
					notifyOpened(null);
					break;
				case MT_PACKET:
					notifyPacket(null, (RawPacket) aMessage.obj);
					break;
				case MT_CLOSED:
					notifyClosed(null);
					break;
			}
		}
	};	
	
	public  void notifyOpened(WebSocketClientEvent aEvent) {
		trellisListener.processOpened(mProcessOpened);
	}
	
	public  void notifyPacket(WebSocketClientEvent aEvent, WebSocketPacket aPacket) {
		trellisListener.processPacket(mProcessPacket,aPacket);
	}
	
	public  void notifyClosed(WebSocketClientEvent aEvent) {
		trellisListener.processClosed(mProcessClosed);
	}

	class Listener implements WebSocketClientListener {
		@Override
		public void processOpened(WebSocketClientEvent aEvent) {
			Message lMsg = new Message();
			//lMsg.what = MT_OPENED;
			//messageHandler.sendMessage(lMsg);
			Log.v(TAG,"WebSocketClientListener:processOpened");
		}

		@Override
		public void processPacket(WebSocketClientEvent aEvent, WebSocketPacket aPacket) {
			Message lMsg = new Message();
			lMsg.what = MT_PACKET;
			lMsg.obj = aPacket;
			//lastMsg = aPacket.getString();
			if (!discardMessage)
				lastMsg.add(aPacket.getString());
			Log.v(TAG,"WebSocketClientListener:processPacket");
			Log.v(TAG," size:" + lastMsg.size());
			messageHandler.sendMessage(lMsg);
		}

		@Override
		public void processClosed(WebSocketClientEvent aEvent) {
			Message lMsg = new Message();
			lMsg.what = MT_CLOSED;
			messageHandler.sendMessage(lMsg);
			Log.v(TAG,"WebSocketClientListener:processClosed");
		}

		@Override
		public void processOpening(WebSocketClientEvent aEvent) {
			Message lMsg = new Message();
			lMsg.what = MT_OPENED;
			messageHandler.sendMessage(lMsg);
			Log.v(TAG,"WebSocketClientListener:processOpening");

		}

		@Override
		public void processReconnecting(WebSocketClientEvent aEvent) {
			Log.v(TAG,"WebSocketClientListener:processReconnecting");
		}

		
	}
	
}
