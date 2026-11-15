#pragma once
#include "PacketSender.h"

class LoopbackPacketSender : public PacketSender {
public:
	CLASS_MEMBER(void*, network, 0x20);
	CLASS_MEMBER(void*, loopbackCallbacks, 0x30);
	CLASS_MEMBER(void*, userList, 0x48);
	CLASS_MEMBER(void*, tempUserIds, 0x50);

	virtual ~LoopbackPacketSender() override;
	virtual bool isInitialized() override;
	virtual void send(Packet* packet) override;
	virtual void sendTo(NetworkIdentifier* id, Packet* packet, unsigned char subId) override;
	virtual void sendToServer(Packet* packet) override;
	virtual void sendToClient(const NetworkIdentifier* id, const Packet* packet, unsigned char subId) override;
	virtual void sendToClient(const NetworkIdentifier* id, const Packet* packet) override;
	virtual void sendToClients(const void* ids, const Packet* packet) override;
	virtual void sendBroadcast(const NetworkIdentifier* id, unsigned char subId, const Packet* packet) override;
	virtual void sendBroadcast(const Packet* packet) override;
	virtual void flush(const NetworkIdentifier* id, void* callback) override;
};
