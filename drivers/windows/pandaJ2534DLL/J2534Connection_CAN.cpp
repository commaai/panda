#include "stdafx.h"
#include "J2534Connection_CAN.h"
#include "Timer.h"

#define check_bmask(num, mask)(((num) & mask) == mask)
#define val_is_29bit(num) check_bmask(num, CAN_29BIT_ID)


J2534Connection_CAN::J2534Connection_CAN(
		panda::Panda* panda_dev,
		unsigned long ProtocolID,
		unsigned long Flags,
		unsigned long BaudRate
	) : J2534Connection(panda_dev, ProtocolID, Flags, BaudRate) {
	this->port = 0;
};

long J2534Connection_CAN::PassThruReadMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	//Timeout of 0 means return immediately. Non zero means WAIT for that time then return. Dafuk.
	long err_code = STATUS_NOERROR;
	Timer t = Timer();

	unsigned long msgnum = 0;
	while (msgnum < *pNumMsgs) {
		if (Timeout > 0 && t.getTimePassed() >= Timeout) {
			err_code = ERR_TIMEOUT;
			break;
		}

		EnterCriticalSection(&this->message_access_lock);
		if (Timeout == 0 && this->messages.empty()) {
			LeaveCriticalSection(&this->message_access_lock);
			break;
		}

		auto msg_in = this->messages.front();
		this->messages.pop();
		LeaveCriticalSection(&this->message_access_lock);

		//if (this->_is_29bit() != msg_in.addr_29b) {}
		PASSTHRU_MSG *msg_out = &pMsg[msgnum++];
		msg_out->ProtocolID = this->ProtocolID;
		msg_out->DataSize = msg_in.DataSize;
		memcpy(msg_out->Data, msg_in.Data.c_str(), msg_in.DataSize);
		msg_out->Timestamp = msg_in.Timestamp;
		msg_out->RxStatus = msg_in.RxStatus;
		if (msgnum == *pNumMsgs) break;
	}

	if (msgnum == 0)
		err_code = ERR_BUFFER_EMPTY;
	*pNumMsgs = msgnum;
	return err_code;
}

long J2534Connection_CAN::PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	//There doesn't seem to be much reason to implement the timeout here.
	for (int msgnum = 0; msgnum < *pNumMsgs; msgnum++) {
		PASSTHRU_MSG* msg = &pMsg[msgnum];
		if (msg->ProtocolID != this->ProtocolID) {
			*pNumMsgs = msgnum;
			return ERR_MSG_PROTOCOL_ID;
		}
		if (msg->DataSize < this->getMinMsgLen() || msg->DataSize > this->getMaxMsgLen() ||
			(val_is_29bit(msg->TxFlags) != this->_is_29bit() && !check_bmask(this->Flags, CAN_ID_BOTH))) {
			*pNumMsgs = msgnum;
			return ERR_INVALID_MSG;
		}

		uint32_t addr = msg->Data[0] << 24 | msg->Data[1] << 16 | msg->Data[2] << 8 | msg->Data[3];
		if (this->panda_dev->can_send(addr, val_is_29bit(msg->TxFlags), &msg->Data[4], msg->DataSize - 4, panda::PANDA_CAN1) == FALSE) {
			*pNumMsgs = msgnum;
			return ERR_INVALID_MSG;
		}
	}
	return STATUS_NOERROR;
}

unsigned long J2534Connection_CAN::getMinMsgLen() {
	return 4;
}

unsigned long J2534Connection_CAN::getMaxMsgLen() {
	return 12;
}