#pragma once
#include "J2534Connection.h"
#include "J2534Connection_CAN.h"
#include <string>

typedef struct ISO15765_FRAMESET {
	std::string msg;
	unsigned long expected_size;
	unsigned char next_part;

	//Critical section will be required if accessed outside of processMessage
	ISO15765_FRAMESET() : msg(""), expected_size(0), next_part(0) {	}
	~ISO15765_FRAMESET() { }

	void init_first_frame(uint16_t final_size, const std::string& piece) {
		expected_size = final_size & 0xFFF;
		msg.reserve(expected_size);
		msg = piece;
		next_part = 1;
	}
	void reset() {
		expected_size = 0;
		msg = "";
	}
} ISO15765_FRAMESET;

class J2534Connection_ISO15765 : public J2534Connection { //J2534Connection_CAN {
public:
	J2534Connection_ISO15765(
		panda::Panda* panda_dev,
		unsigned long ProtocolID,
		unsigned long Flags,
		unsigned long BaudRate
	);

	virtual long PassThruReadMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);
	virtual long PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);

	virtual long PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG * pMaskMsg, PASSTHRU_MSG * pPatternMsg, PASSTHRU_MSG * pFlowControlMsg, unsigned long * pFilterID);

	int get_matching_filter_id_for_outmsg(const PASSTHRU_MSG * msg);
	int get_matching_filter_id_for_outmsg(const PASSTHRU_MSG_INTERNAL&  msg);

	virtual void J2534Connection_ISO15765::processMessage(const PASSTHRU_MSG_INTERNAL& msg);
	virtual unsigned long getMinMsgLen();
	virtual unsigned long getMaxMsgLen();

	virtual bool _is_29bit() {
		return (this->Flags & CAN_29BIT_ID) == CAN_29BIT_ID;
	}

	virtual bool isProtoCan() {
		return TRUE;
	}

private:
	std::array<ISO15765_FRAMESET, 10> conversations;
};

