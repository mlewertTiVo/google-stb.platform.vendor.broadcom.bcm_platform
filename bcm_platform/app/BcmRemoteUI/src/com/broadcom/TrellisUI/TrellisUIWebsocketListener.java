package com.broadcom.TrellisUI;
import org.jwebsocket.api.WebSocketPacket;

public interface TrellisUIWebsocketListener {
	
	void processOpened(String processOpenedHandler);

	void processPacket(String processPacketHandler, WebSocketPacket aPacket);

	void processClosed(String processClosedHandler);
}

