#include "stdafx.h"
#include "PandaJ2534Device.h"

FLOW_CONTROL_WRITE::FLOW_CONTROL_WRITE(std::shared_ptr<FrameSet> framein) : frame(framein) {
	expire = std::chrono::steady_clock::now() + framein->separation_time;
}

void FLOW_CONTROL_WRITE::refreshExpiration() {
	if (auto& frameptr = this->frame.lock()) {
		expire += frameptr->separation_time;
	}
}


PandaJ2534Device::PandaJ2534Device(std::unique_ptr<panda::Panda> new_panda) {
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
	this->flow_control_thread_handle = CreateThread(NULL, 0, _flow_control_write_threadBootstrap, (LPVOID)this, 0, &flowControlSendThreadID);
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

DWORD WINAPI PandaJ2534Device::_can_recv_threadBootstrap(LPVOID This) {
	return ((PandaJ2534Device*)This)->can_recv_thread();
}

DWORD PandaJ2534Device::can_recv_thread() {
	DWORD err = TRUE;
	while (err) {
		std::vector<panda::PANDA_CAN_MSG> msg_recv;
		err = this->panda->can_recv_async(this->thread_kill_event, msg_recv);
		for (auto msg_in : msg_recv) {
			PASSTHRU_MSG_INTERNAL msg_out;
			msg_out.ProtocolID = CAN;
			msg_out.ExtraDataIndex = 0;
			msg_out.Data.reserve(msg_in.len + 4);
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
				if (conn->isProtoCan() && conn->getPort() == msg_in.bus) {
					if (msg_in.is_receipt) {
						conn->processMessageReceipt(msg_out);
					} else {
						conn->processMessage(msg_out);
					}
				}
		}
	}

	return 0;
}

DWORD PandaJ2534Device::_flow_control_write_threadBootstrap(LPVOID This) {
	return ((PandaJ2534Device*)This)->flow_control_write_thread();
}

DWORD PandaJ2534Device::flow_control_write_thread() {
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
				auto& fcontrol_write = this->active_flow_control_txs.front();
				if (auto& frame = fcontrol_write->frame.lock()) {
					if (std::chrono::steady_clock::now() >= fcontrol_write->expire) {
						this->active_flow_control_txs.pop_front();
						if (auto& filter = frame->filter.lock()) {
							//Write message
							filter->conn->sendConsecutiveFrame(frame, filter);
						} else {
							this->active_flow_control_txs.pop_front();
						}
						//fcontrol_write->refreshExpiration();
						//insertMultiPartTxInQueue(std::move(fcontrol_write));
					} else { //Ran out of things that need to be sent now. Sleep!
						auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(fcontrol_write->expire - std::chrono::steady_clock::now());
						sleepDuration = max(1, time_diff.count());
						goto break_flow_ctrl_loop;// doloop = FALSE;
					}
				} else { // This frame has been aborted.
					this->active_flow_control_txs.pop_front();
				}
			}
		}
		break_flow_ctrl_loop:
		continue;
	}
	return 0;
}

void PandaJ2534Device::insertMultiPartTxInQueue(std::unique_ptr<FLOW_CONTROL_WRITE> fcwrite) {
	synchronized(active_flow_control_txs_lock) {
		auto iter = this->active_flow_control_txs.begin();
		for (; iter != this->active_flow_control_txs.end(); iter++) {
			if (fcwrite->expire < (*iter)->expire) break;
		}
		this->active_flow_control_txs.insert(iter, std::move(fcwrite));
	}
}

void PandaJ2534Device::registerMultiPartTx(std::shared_ptr<FrameSet> frame) {
	auto fcwrite = std::make_unique<FLOW_CONTROL_WRITE>(frame);
	insertMultiPartTxInQueue(std::move(fcwrite));
	SetEvent(this->flow_control_wakeup_event);
}
