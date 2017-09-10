#include "stdafx.h"
#include "J2534Connection.h"

J2534Connection::J2534Connection(
	panda::Panda* panda_dev,
	unsigned long ProtocolID,
	unsigned long Flags,
	unsigned long BaudRate
) : panda_dev(panda_dev), ProtocolID(ProtocolID), Flags(Flags), BaudRate(BaudRate), port(0) {
	InitializeCriticalSectionAndSpinCount(&this->message_access_lock, 0x00000400);
}

J2534Connection::~J2534Connection() {
	DeleteCriticalSection(&this->message_access_lock);
}

long J2534Connection::PassThruReadMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	*pNumMsgs = 0;
	return STATUS_NOERROR;
}
long J2534Connection::PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) { return STATUS_NOERROR; }
long J2534Connection::PassThruStartPeriodicMsg(PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval) { return STATUS_NOERROR; }
long J2534Connection::PassThruStopPeriodicMsg(unsigned long MsgID) { return STATUS_NOERROR; }

long J2534Connection::PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg,
	PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID) {
	for (int i = 0; i < this->filters.size(); i++) {
		if (filters[i] == nullptr) {
			try {
				filters[i].reset(new J2534MessageFilter(this, FilterType, pMaskMsg, pPatternMsg, pFlowControlMsg));
				*pFilterID = i;
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

bool J2534Connection::isProtoCan() {
	return this->ProtocolID == CAN || this->ProtocolID == CAN_PS;
}

unsigned long J2534Connection::getPort() {
	return this->port;
}

void J2534Connection::processMessage(const PASSTHRU_MSG_INTERNAL& msg) {
	FILTER_RESULT filter_res = FILTER_RESULT_NEUTRAL;
	for (auto filter : this->filters) {
		if (filter == nullptr) continue;
		FILTER_RESULT current_check_res = filter->check(msg);
		if (current_check_res == FILTER_RESULT_BLOCK) return;
		if (current_check_res == FILTER_RESULT_PASS) filter_res = FILTER_RESULT_PASS;
	}

	// Optionally ignore loopbacks
	if ((msg.RxStatus & TX_MSG_TYPE) == TX_MSG_TYPE && !this->loopback) return;

	if (filter_res == FILTER_RESULT_PASS) {
		EnterCriticalSection(&this->message_access_lock);
		this->messages.push(msg);
		LeaveCriticalSection(&this->message_access_lock);
	}
}

unsigned long J2534Connection::getMinMsgLen() {
	return 1;
}

unsigned long J2534Connection::getMaxMsgLen() {
	return 4128;
}
