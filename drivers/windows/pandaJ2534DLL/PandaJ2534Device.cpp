#include "stdafx.h"
#include "PandaJ2534Device.h"

PandaJ2534Device::PandaJ2534Device(std::unique_ptr<panda::Panda> new_panda) {
	this->panda = std::move(new_panda);

	this->panda->set_can_speed_kbps(panda::PANDA_CAN1, 500);
	this->panda->set_safety_mode(panda::SAFETY_ALLOUTPUT);
	//this->panda->set_can_loopback(TRUE);
	this->panda->set_can_loopback(FALSE);
	this->panda->set_alt_setting(1);

	DWORD threadid;
	this->can_kill_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	this->can_thread_handle = CreateThread(NULL, 0, _threadBootstrap, (LPVOID)this, 0, &threadid);
};

PandaJ2534Device::~PandaJ2534Device() {
	SetEvent(this->can_kill_event);
	DWORD res = WaitForSingleObject(this->can_thread_handle, INFINITE);
	CloseHandle(this->can_thread_handle);
}

std::unique_ptr<PandaJ2534Device> PandaJ2534Device::openByName(std::string sn) {
	auto p = panda::Panda::openPanda("");
	if (p == nullptr)
		return nullptr;
	return std::unique_ptr<PandaJ2534Device>(new PandaJ2534Device(std::move(p)));
}

DWORD PandaJ2534Device::closeChannel(unsigned long ChannelID) {
	if (this->connections.size() <= ChannelID) return ERR_INVALID_CHANNEL_ID;
	if (this->connections[ChannelID] == nullptr) return ERR_INVALID_CHANNEL_ID;
	this->connections[ChannelID] = nullptr;
	return STATUS_NOERROR;
}

DWORD PandaJ2534Device::addChannel(J2534Connection* conn, unsigned long* channel_id) {
	int channel_index = -1;
	for (unsigned int i = 0; i < this->connections.size(); i++)
		if (this->connections[i] == nullptr) {
			channel_index = i;
			break;
		}

	if (channel_index == -1) {
		if (this->connections.size() == 0xFFFF) //channelid max 16 bits
			return ERR_FAILED; //Too many channels
		this->connections.push_back(nullptr);
		channel_index = this->connections.size() - 1;
	}

	this->connections[channel_index].reset(conn);

	*channel_id = channel_index;
	return STATUS_NOERROR;
}

DWORD WINAPI PandaJ2534Device::_threadBootstrap(LPVOID This) {
	return ((PandaJ2534Device*)This)->can_recv_thread();
}

DWORD PandaJ2534Device::can_recv_thread() {
	DWORD err = TRUE;
	while (err) {
		std::vector<panda::PANDA_CAN_MSG> msg_recv;
		err = this->panda->can_recv_async(this->can_kill_event, msg_recv);
		for (auto msg_in : msg_recv) {
			//if (this->_is_29bit() != msg_in.addr_29b) {}
			PASSTHRU_MSG_INTERNAL msg_out;
			msg_out.ProtocolID = CAN;
			msg_out.DataSize = msg_in.len + 4;
			msg_out.ExtraDataIndex = msg_out.DataSize;
			msg_out.Data.reserve(msg_out.DataSize);
			msg_out.Data += msg_in.addr >> 24;
			msg_out.Data += (msg_in.addr >> 16) & 0xFF;
			msg_out.Data += (msg_in.addr >> 8) & 0xFF;
			msg_out.Data += msg_in.addr & 0xFF;
			std::string tmp = std::string((char*)&msg_in.dat, msg_in.len);
			msg_out.Data += tmp;
			msg_out.Timestamp = msg_in.recv_time;
			msg_out.RxStatus = (msg_in.addr_29b ? CAN_29BIT_ID : 0) |
				(msg_in.is_receipt ? TX_MSG_TYPE : 0);

			// TODO: Make this more efficient
			for (auto& conn : this->connections)
				if (conn->isProtoCan() && conn->getPort() == msg_in.bus)
					conn->processMessage(msg_out);
		}
	}

	return STATUS_NOERROR;
}
