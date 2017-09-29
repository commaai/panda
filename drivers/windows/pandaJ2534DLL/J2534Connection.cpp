#include "stdafx.h"
#include "J2534Connection.h"
#include "Timer.h"

J2534Connection::J2534Connection(
	std::shared_ptr<PandaJ2534Device> panda_dev,
	unsigned long ProtocolID,
	unsigned long Flags,
	unsigned long BaudRate
) : panda_dev(panda_dev), ProtocolID(ProtocolID), Flags(Flags), BaudRate(BaudRate), port(0) { }

long J2534Connection::PassThruReadMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	//Timeout of 0 means return immediately. Non zero means WAIT for that time then return. Dafuk.
	long err_code = STATUS_NOERROR;
	Timer t = Timer();

	unsigned long msgnum = 0;
	while (msgnum < *pNumMsgs) {
		if (Timeout > 0 && t.getTimePassed() >= Timeout) {
			err_code = ERR_TIMEOUT;
			break;
		}

		//Synchronized won't work where we have to break out of a loop
		message_access_lock.lock();
		if (this->messages.empty()) {
			message_access_lock.unlock();
			if (Timeout == 0)
				break;
			continue;
		}

		auto msg_in = this->messages.front();
		this->messages.pop();
		message_access_lock.unlock();

		PASSTHRU_MSG *msg_out = &pMsg[msgnum++];
		msg_out->ProtocolID = this->ProtocolID;
		msg_out->DataSize = msg_in.Data.size();
		memcpy(msg_out->Data, msg_in.Data.c_str(), msg_in.Data.size());
		msg_out->Timestamp = msg_in.Timestamp;
		msg_out->RxStatus = msg_in.RxStatus;
		msg_out->ExtraDataIndex = msg_in.ExtraDataIndex;
		msg_out->TxFlags = 0;
		if (msgnum == *pNumMsgs) break;
	}

	if (msgnum == 0)
		err_code = ERR_BUFFER_EMPTY;
	*pNumMsgs = msgnum;
	return err_code;
}
long J2534Connection::PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) { return STATUS_NOERROR; }
long J2534Connection::PassThruStartPeriodicMsg(PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval) { return STATUS_NOERROR; }
long J2534Connection::PassThruStopPeriodicMsg(unsigned long MsgID) { return STATUS_NOERROR; }

long J2534Connection::PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg,
	PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID) {
	for (int i = 0; i < this->filters.size(); i++) {
		if (filters[i] == nullptr) {
			try {
				auto newfilter = std::make_shared<J2534MessageFilter>(J2534MessageFilter(this, FilterType, pMaskMsg, pPatternMsg, pFlowControlMsg));
				for (int check_idx = 0; check_idx < filters.size(); check_idx++) {
					if (filters[check_idx] == nullptr) continue;
					if (filters[check_idx] == newfilter) {
						filters[i] = nullptr;
						return ERR_NOT_UNIQUE;
					}
				}
				*pFilterID = i;
				filters[i] = newfilter;
				return STATUS_NOERROR;
			} catch (int e) {
				return e;
			}
		}
	}
	return ERR_EXCEEDED_LIMIT;
}

long J2534Connection::PassThruStopMsgFilter(unsigned long FilterID) {
	if (FilterID >= this->filters.size() || this->filters[FilterID] == nullptr)
		return ERR_INVALID_FILTER_ID;
	this->filters[FilterID] = nullptr;
	return STATUS_NOERROR;
}

long J2534Connection::PassThruIoctl(unsigned long IoctlID, void *pInput, void *pOutput) {
	return STATUS_NOERROR;
}

long J2534Connection::init5b(SBYTE_ARRAY* pInput, SBYTE_ARRAY* pOutput) { return STATUS_NOERROR; }
long J2534Connection::initFast(PASSTHRU_MSG* pInput, PASSTHRU_MSG* pOutput) { return STATUS_NOERROR; }
long J2534Connection::clearTXBuff() { return STATUS_NOERROR; }
long J2534Connection::clearRXBuff() {
	this->messages = {};
	return STATUS_NOERROR;
}
long J2534Connection::clearPeriodicMsgs() { return STATUS_NOERROR; }
long J2534Connection::clearMsgFilters() {
	for (auto& filter : this->filters) filter = nullptr;
	return STATUS_NOERROR;
}

long J2534Connection::setBaud(unsigned long baud) {
	this->BaudRate = baud;
	return STATUS_NOERROR;
}

unsigned long J2534Connection::getBaud() {
	return this->BaudRate;
}

unsigned long J2534Connection::getProtocol() {
	return this->ProtocolID;
}

unsigned long J2534Connection::getPort() {
	return this->port;
}

void J2534Connection::processMessageReceipt(const PASSTHRU_MSG_INTERNAL& msg) {
	if (this->loopback) {
		synchronized(message_access_lock) {
			this->messages.push(msg);
		}
	}
}

//Works well as long as the protocol doesn't support flow control.
void J2534Connection::processMessage(const PASSTHRU_MSG_INTERNAL& msg) {
	FILTER_RESULT filter_res = FILTER_RESULT_NEUTRAL;

	for (auto filter : this->filters) {
		if (filter == nullptr) continue;
		FILTER_RESULT current_check_res = filter->check(msg);
		if (current_check_res == FILTER_RESULT_BLOCK) return;
		if (current_check_res == FILTER_RESULT_PASS) filter_res = FILTER_RESULT_PASS;
	}

	if (filter_res == FILTER_RESULT_PASS) {
		synchronized(message_access_lock) {
			this->messages.push(msg);
		}
	}
}

unsigned long J2534Connection::getMinMsgLen() {
	return 1;
}

unsigned long J2534Connection::getMaxMsgLen() {
	return 4128;
}
