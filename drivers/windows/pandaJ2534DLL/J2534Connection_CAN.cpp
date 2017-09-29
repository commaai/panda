#include "stdafx.h"
#include "J2534Connection_CAN.h"
#include "Timer.h"

J2534Connection_CAN::J2534Connection_CAN(
		std::shared_ptr<PandaJ2534Device> panda_dev,
		unsigned long ProtocolID,
		unsigned long Flags,
		unsigned long BaudRate
	) : J2534Connection(panda_dev, ProtocolID, Flags, BaudRate) {
	this->port = 0;

	if (BaudRate % 100 || BaudRate < 10000 || BaudRate > 5000000)
		throw ERR_INVALID_BAUDRATE;

	panda_dev->panda->set_can_speed_cbps(panda::PANDA_CAN1, BaudRate/100);
};

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
		if (auto panda_dev_sp = this->panda_dev.lock()) {
			if (panda_dev_sp->panda->can_send(addr, val_is_29bit(msg->TxFlags), &msg->Data[4], msg->DataSize - 4, panda::PANDA_CAN1) == FALSE) {
				*pNumMsgs = msgnum;
				return ERR_INVALID_MSG;
			}
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
