#pragma once
#include "../../Utils/MemoryUtil.h"

class Packet;
struct NetworkIdentifier;

class PacketSender {
public:
	virtual ~PacketSender();
	virtual bool isInitialized();
	virtual void send(Packet* packet);
	virtual void sendTo(NetworkIdentifier* id, Packet* packet, unsigned char subId);
	virtual void sendToServer(Packet* packet);
	virtual void sendToClient(const NetworkIdentifier* id, const Packet* packet, unsigned char subId);
	virtual void sendToClient(const NetworkIdentifier* id, const Packet* packet);
	virtual void sendToClients(const void* ids, const Packet* packet);
	virtual void sendBroadcast(const NetworkIdentifier* id, unsigned char subId, const Packet* packet);
	virtual void sendBroadcast(const Packet* packet);
	virtual void flush(const NetworkIdentifier* id, void* callback);
};
