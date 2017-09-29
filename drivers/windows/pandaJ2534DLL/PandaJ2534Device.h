#pragma once
#include <memory>
#include <list>
#include <chrono>
#include "J2534_v0404.h"
#include "panda/panda.h"
#include "synchronize.h"
#include "J2534Connection.h"
#include "FrameSet.h"

class J2534Connection;
class FrameSet;

typedef struct FLOW_CONTROL_WRITE {
	FLOW_CONTROL_WRITE(std::shared_ptr<FrameSet> framein);
	~FLOW_CONTROL_WRITE() {};

	void refreshExpiration();

	std::weak_ptr<FrameSet> frame;
	std::chrono::time_point<std::chrono::steady_clock> expire;
} FLOW_CONTROL_WRITE;

class PandaJ2534Device {
public:
	PandaJ2534Device(std::unique_ptr<panda::Panda> new_panda);

	~PandaJ2534Device();

	static std::shared_ptr<PandaJ2534Device> openByName(std::string sn);

	DWORD closeChannel(unsigned long ChannelID);
	DWORD addChannel(J2534Connection* conn, unsigned long* channel_id);

	std::unique_ptr<panda::Panda> panda;
	std::vector<std::unique_ptr<J2534Connection>> connections;

	void insertMultiPartTxInQueue(std::unique_ptr<FLOW_CONTROL_WRITE> fcwrite);
	void registerMultiPartTx(std::shared_ptr<FrameSet> frame);

private:
	HANDLE thread_kill_event;

	HANDLE can_thread_handle;
	static DWORD WINAPI _can_recv_threadBootstrap(LPVOID This);
	DWORD can_recv_thread();

	HANDLE flow_control_wakeup_event;
	HANDLE flow_control_thread_handle;
	static DWORD WINAPI _flow_control_write_threadBootstrap(LPVOID This);
	DWORD flow_control_write_thread();
	std::list<std::unique_ptr<FLOW_CONTROL_WRITE>> active_flow_control_txs;
	Mutex active_flow_control_txs_lock;
};
