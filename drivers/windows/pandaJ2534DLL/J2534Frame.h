#pragma once
#include "J2534_v0404.h"
#include "panda/panda.h"

class J2534Frame {
public:
	J2534Frame(unsigned long ProtocolID, unsigned long RxStatus=0, unsigned long TxFlags=0, unsigned long Timestamp=0) :
		ProtocolID(ProtocolID), RxStatus(RxStatus), TxFlags(TxFlags), Timestamp(Timestamp), ExtraDataIndex(0), Data("") { };

	J2534Frame(const panda::PANDA_CAN_MSG& msg_in) {
		ProtocolID = CAN;
		ExtraDataIndex = 0;
		Data.reserve(msg_in.len + 4);
		Data += msg_in.addr >> 24;
		Data += (msg_in.addr >> 16) & 0xFF;
		Data += (msg_in.addr >> 8) & 0xFF;
		Data += msg_in.addr & 0xFF;
		Data += std::string((char*)&msg_in.dat, msg_in.len);
		Timestamp = msg_in.recv_time;
		RxStatus = (msg_in.addr_29b ? CAN_29BIT_ID : 0) |
			(msg_in.is_receipt ? TX_MSG_TYPE : 0);
	}

	J2534Frame(const PASSTHRU_MSG& msg) {
		ProtocolID = msg.ProtocolID;
		RxStatus = msg.RxStatus;
		TxFlags = msg.TxFlags;
		Timestamp = msg.Timestamp;
		ExtraDataIndex = msg.ExtraDataIndex;
		Data = std::string((const char*)msg.Data, msg.DataSize);
	}

	unsigned long	ProtocolID;
	unsigned long	RxStatus;
	unsigned long	TxFlags;
	unsigned long	Timestamp;
	unsigned long	ExtraDataIndex;
	std::string		Data;
};