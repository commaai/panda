#pragma once
#include <memory>
#include "J2534_v0404.h"
#include "panda/panda.h"
#include "J2534Connection.h"

class J2534Connection;

class PandaJ2534Device {
public:
	PandaJ2534Device(std::unique_ptr<panda::Panda> new_panda);

	~PandaJ2534Device();

	static std::shared_ptr<PandaJ2534Device> openByName(std::string sn);

	DWORD closeChannel(unsigned long ChannelID);
	DWORD addChannel(J2534Connection* conn, unsigned long* channel_id);

	std::unique_ptr<panda::Panda> panda;
	std::vector<std::unique_ptr<J2534Connection>> connections;

private:
	static DWORD WINAPI _can_recv_threadBootstrap(LPVOID This);
	DWORD can_recv_thread();

	HANDLE can_thread_handle;
	HANDLE can_kill_event;

	static DWORD WINAPI _flow_control_write_threadBootstrap(LPVOID This);
	DWORD flow_control_write_thread();

	HANDLE flow_control_thread_handle;
	HANDLE flow_control_wakeup_event;
};
