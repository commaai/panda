#include "stdafx.h"
#include "J2534Connection_CAN.h"
#include "MessageTx_CAN.h"
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

		auto msgtx = std::make_shared<MessageTx_CAN>(shared_from_this(), *msg);
		this->schedultMsgTx(std::dynamic_pointer_cast<MessageTx>(msgtx));
	}
	return STATUS_NOERROR;
}
