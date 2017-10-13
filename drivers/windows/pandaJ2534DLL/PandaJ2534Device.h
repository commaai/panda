#pragma once
#include <memory>
#include <list>
#include <queue>
#include <set>
#include <chrono>
#include "J2534_v0404.h"
#include "panda/panda.h"
#include "synchronize.h"
#include "J2534Connection.h"
#include "MessageTx.h"

class J2534Connection;
class MessageTx;

typedef struct SCHEDULED_TX_MSG {
	SCHEDULED_TX_MSG(std::shared_ptr<MessageTx> msgtx, BOOL startdelayed = FALSE);

	void refreshExpiration();
	void refreshExpiration(std::chrono::time_point<std::chrono::steady_clock> starttime);

	std::shared_ptr<MessageTx> msgtx;
	std::chrono::time_point<std::chrono::steady_clock> expire;
} FLOW_CONTROL_WRITE;


class PandaJ2534Device {
public:
	PandaJ2534Device(std::unique_ptr<panda::Panda> new_panda);

	~PandaJ2534Device();

	static std::shared_ptr<PandaJ2534Device> openByName(std::string sn);

	DWORD closeChannel(unsigned long ChannelID);
	DWORD addChannel(std::shared_ptr<J2534Connection>& conn, unsigned long* channel_id);

	std::unique_ptr<panda::Panda> panda;
	std::vector<std::shared_ptr<J2534Connection>> connections;

	void insertMultiPartTxInQueue(std::unique_ptr<SCHEDULED_TX_MSG> fcwrite);

	void registerMessageTx(std::shared_ptr<MessageTx> msg, BOOL startdelayed=FALSE);

	void registerConnectionTx(std::shared_ptr<J2534Connection> conn);

	void unstallConnectionTx(std::shared_ptr<J2534Connection> conn);

	void removeConnectionTopMessage(std::shared_ptr<J2534Connection> conn);

private:
	HANDLE thread_kill_event;

	HANDLE can_thread_handle;
	static DWORD WINAPI _can_recv_threadBootstrap(LPVOID This);
	DWORD can_recv_thread();

	HANDLE flow_control_wakeup_event;
	HANDLE flow_control_thread_handle;
	static DWORD WINAPI _msg_tx_threadBootstrap(LPVOID This);
	DWORD msg_tx_thread();
	std::list<std::unique_ptr<SCHEDULED_TX_MSG>> active_flow_control_txs;
	Mutex active_flow_control_txs_lock;
	std::queue<std::unique_ptr<SCHEDULED_TX_MSG>> txMsgsAwaitingEcho;

	std::queue<std::shared_ptr<J2534Connection>> ConnTxQueue;
	std::set<std::shared_ptr<J2534Connection>> ConnTxSet;
	Mutex ConnTxMutex;
	BOOL txInProgress;
};
