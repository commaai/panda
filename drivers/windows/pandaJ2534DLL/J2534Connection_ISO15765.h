#pragma once
#include <string>
#include "J2534Connection.h"
#include "J2534Connection_CAN.h"
#include "FrameSet.h"

typedef struct {
	std::string dispatched_msg;
	std::string remaining_payload;
} PRESTAGED_WRITE;

class J2534Connection_ISO15765 : public J2534Connection { //J2534Connection_CAN {
public:
	J2534Connection_ISO15765(
		std::shared_ptr<PandaJ2534Device> panda_dev,
		unsigned long ProtocolID,
		unsigned long Flags,
		unsigned long BaudRate
	);

	virtual long PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);

	virtual long PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG * pMaskMsg, PASSTHRU_MSG * pPatternMsg, PASSTHRU_MSG * pFlowControlMsg, unsigned long * pFilterID);

	int get_matching_out_fc_filter_id(const std::string & msgdata, unsigned long flags, unsigned long flagmask);

	int get_matching_in_fc_filter_id(const PASSTHRU_MSG_INTERNAL & msg, unsigned long flagmask = CAN_29BIT_ID);

	virtual void processMessageReceipt(const PASSTHRU_MSG_INTERNAL& msg);
	virtual void processMessage(const PASSTHRU_MSG_INTERNAL& msg);
	void sendConsecutiveFrame(std::shared_ptr<FrameSet> frame, std::shared_ptr<J2534MessageFilter> filter);

	virtual unsigned long getMinMsgLen() {
		return 4;
	}

	virtual unsigned long getMaxMsgLen() {
		return 4099;
	};

	virtual bool _is_29bit() {
		return (this->Flags & CAN_29BIT_ID) == CAN_29BIT_ID;
	}

	virtual bool isProtoCan() {
		return TRUE;
	}

private:
	Mutex staged_writes_lock;
	std::array<PRESTAGED_WRITE, 10> staged_writes;
	std::array<std::shared_ptr<FrameSet>, 10> conversations;
};
