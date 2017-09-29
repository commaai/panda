#pragma once

#include "J2534Connection.h"
#include "panda/panda.h"

#define val_is_29bit(num) check_bmask(num, CAN_29BIT_ID)

class J2534Connection_CAN : public J2534Connection {
public:
	J2534Connection_CAN(
		std::shared_ptr<PandaJ2534Device> panda_dev,
		unsigned long ProtocolID,
		unsigned long Flags,
		unsigned long BaudRate
	);

	virtual long PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);

	virtual unsigned long getMinMsgLen();
	virtual unsigned long getMaxMsgLen();

	virtual bool isProtoCan() {
		return TRUE;
	}

	bool _is_29bit() {
		return (this->Flags & CAN_29BIT_ID) == CAN_29BIT_ID;
	}

};