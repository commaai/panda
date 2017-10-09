#include "stdafx.h"
#include "PandaJ2534Device.h"
#include "J2534Frame.h"

SCHEDULED_TX_MSG::SCHEDULED_TX_MSG(std::shared_ptr<MessageTx> msgtx) : msgtx(msgtx) {
	expire = std::chrono::steady_clock::now(); //Should be triggered immediately.
}

void SCHEDULED_TX_MSG::refreshExpiration() {
	//expire += separation_time;
}


PandaJ2534Device::PandaJ2534Device(std::unique_ptr<panda::Panda> new_panda) : txInProgress(FALSE) {
	this->panda = std::move(new_panda);

	this->panda->set_esp_power(FALSE);
	this->panda->set_safety_mode(panda::SAFETY_ALLOUTPUT);
	this->panda->set_can_loopback(FALSE);
	this->panda->set_alt_setting(1);

	DWORD canListenThreadID;
	this->thread_kill_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	this->can_thread_handle = CreateThread(NULL, 0, _can_recv_threadBootstrap, (LPVOID)this, 0, &canListenThreadID);

	DWORD flowControlSendThreadID;
	this->flow_control_wakeup_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	this->flow_control_thread_handle = CreateThread(NULL, 0, _msg_tx_threadBootstrap, (LPVOID)this, 0, &flowControlSendThreadID);
};

PandaJ2534Device::~PandaJ2534Device() {
	SetEvent(this->thread_kill_event);
	DWORD res = WaitForSingleObject(this->can_thread_handle, INFINITE);
	CloseHandle(this->can_thread_handle);

	res = WaitForSingleObject(this->flow_control_thread_handle, INFINITE);
	CloseHandle(this->flow_control_thread_handle);

	CloseHandle(this->flow_control_wakeup_event);
	CloseHandle(this->thread_kill_event);
}

std::shared_ptr<PandaJ2534Device> PandaJ2534Device::openByName(std::string sn) {
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

DWORD PandaJ2534Device::addChannel(std::shared_ptr<J2534Connection>& conn, unsigned long* channel_id) {
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

	this->connections[channel_index] = conn;

	*channel_id = channel_index;
	return STATUS_NOERROR;
}

DWORD WINAPI PandaJ2534Device::_can_recv_threadBootstrap(LPVOID This) {
	return ((PandaJ2534Device*)This)->can_recv_thread();
}

DWORD PandaJ2534Device::can_recv_thread() {
	DWORD err = TRUE;
	while (err) {
		std::vector<panda::PANDA_CAN_MSG> msg_recv;
		err = this->panda->can_recv_async(this->thread_kill_event, msg_recv);
		for (auto msg_in : msg_recv) {
			J2534Frame msg_out(msg_in);

			if (msg_in.is_receipt) {
				synchronized(active_flow_control_txs_lock) {
					if (txMsgsAwaitingEcho.size() > 0) {
						auto& msgtx = txMsgsAwaitingEcho.front()->msgtx;
						if (auto conn = msgtx->connection.lock()) {
							if (conn->isProtoCan() && conn->getPort() == msg_in.bus) {
								if (msgtx->checkTxReceipt(msg_out)) {
									conn->processMessageReceipt(msg_out);
									txMsgsAwaitingEcho.pop(); //nextWaitingMsg no longer valid
								}
							}
						} else {
							txMsgsAwaitingEcho.pop(); //nextWaitingMsg no longer valid
						}
					}
				}
			} else {
				// TODO: Make this more efficient
				for (auto& conn : this->connections)
					if (conn->isProtoCan() && conn->getPort() == msg_in.bus)
						conn->processMessage(msg_out);
			}
		}
	}

	return 0;
}

DWORD PandaJ2534Device::_msg_tx_threadBootstrap(LPVOID This) {
	return ((PandaJ2534Device*)This)->msg_tx_thread();
}

DWORD PandaJ2534Device::msg_tx_thread() {
	const HANDLE subscriptions[] = { this->flow_control_wakeup_event, this->thread_kill_event };
	DWORD sleepDuration = INFINITE;
	while (TRUE) {
		DWORD res = WaitForMultipleObjects(2, subscriptions, FALSE, sleepDuration);
		if (res == WAIT_OBJECT_0 + 1) return 0;
		if (res != WAIT_OBJECT_0 && res != WAIT_TIMEOUT) {
			printf("Got an unexpected wait result in flow_control_write_thread. Res: %d; GetLastError: %d\n. Terminating thread.", res, GetLastError());
			return 0;
		}
		ResetEvent(this->flow_control_wakeup_event);

		while (TRUE) {
			synchronized(active_flow_control_txs_lock) { //implemented with for loop. Consumes breaks.
				if (this->active_flow_control_txs.size() == 0) {
					sleepDuration = INFINITE;
					goto break_flow_ctrl_loop;
				}
				if (std::chrono::steady_clock::now() >= this->active_flow_control_txs.front()->expire) {
					auto fcontrol_write_real = std::move(this->active_flow_control_txs.front());
					this->active_flow_control_txs.pop_front();
					if (fcontrol_write_real->msgtx->sendNextFrame())
						txMsgsAwaitingEcho.push(std::move(fcontrol_write_real));
				} else { //Ran out of things that need to be sent now. Sleep!
					auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>
						(this->active_flow_control_txs.front()->expire - std::chrono::steady_clock::now());
					sleepDuration = max(1, time_diff.count());
					goto break_flow_ctrl_loop;
				}
			}
		}
		break_flow_ctrl_loop:
		continue;
	}
	return 0;
}

void PandaJ2534Device::insertMultiPartTxInQueue(std::unique_ptr<SCHEDULED_TX_MSG> fcwrite) {
	synchronized(active_flow_control_txs_lock) {
		auto iter = this->active_flow_control_txs.begin();
		for (; iter != this->active_flow_control_txs.end(); iter++) {
			if (fcwrite->expire < (*iter)->expire) break;
		}
		this->active_flow_control_txs.insert(iter, std::move(fcwrite));
	}
}

void PandaJ2534Device::registerConnectionTx(std::shared_ptr<J2534Connection> conn) {
	synchronized(ConnTxMutex) {
		auto ret = this->ConnTxSet.insert(conn);
		if (ret.second == FALSE) return; //Conn already exists.
		this->ConnTxQueue.push(conn);
		this->txInProgress = TRUE;

		auto fcwrite = std::make_unique<SCHEDULED_TX_MSG>(conn->txbuff.back());
		this->insertMultiPartTxInQueue(std::move(fcwrite));
	}
	SetEvent(flow_control_wakeup_event);
}
