#pragma once
#include "J2534Connection.h"
#include "J2534Connection_CAN.h"
#include <string>

typedef struct ISO15765_FRAMESET {
	bool active;
	std::string msg;
	unsigned long expected_size;
	unsigned char next_part;
	bool istx;
	unsigned long flags;

	CRITICAL_SECTION access_lock;

	//Critical section will be required if accessed outside of processMessage
	ISO15765_FRAMESET() : active(FALSE), msg(""), expected_size(0), next_part(0), istx(FALSE), flags(0) {
		InitializeCriticalSectionAndSpinCount(&access_lock, 0x00000400);
	}
	~ISO15765_FRAMESET() {
		DeleteCriticalSection(&access_lock);
	}

	void init_tx(std::string& payload, unsigned long bytes_already_sent, unsigned long txFlags) {
		lock();
		{
			active = TRUE;
			expected_size = 1;
			msg = payload;
			next_part = 1;
			flags = txFlags;
			istx = TRUE;
		}
		unlock();
	}

	void init_rx_first_frame(uint16_t final_size, const std::string& piece, unsigned long rxFlags) {
		lock();
		{
			active = TRUE;
			expected_size = final_size & 0xFFF;
			msg.reserve(expected_size);
			msg = piece;
			next_part = 1;
			flags = rxFlags;
			istx = FALSE;
		}
		unlock();
	}

	/*void rx_add_frame(unsigned long addrlen) {
		lock();
		if (expected_size == 0) {
			unlock();
			return;
		}
		if ((msgin.Data[addrlen] & 0x0F) != next_part) {
			unlock();
			return;
		}
		next_part = (next_part + 1) % 0x10;
		unsigned int payload_len = min(expected_size - msg.size(), (is_ext_addr ? 6 : 7));
		if (msgin.Data.size() < (addrlen + 1 + payload_len)) {
			//A frame was received that could have held more data.
			//No examples of this protocol show that happening, so
			//it will be assumed that it is grounds to reset rx.
			reset();
			unlock();
			return;
		}
		msg += msgin.Data.substr(addrlen + 1, payload_len);
		unlock();
	}*/

	void reset() {
		lock();
		{
			active = FALSE;
			expected_size = 0;
			msg = "";
		}
		unlock();
	}

	unsigned int bytes_remaining() {
		unsigned int res;
		lock();
		{
			if(istx)
				res = this->msg.size() - this->expected_size;
			else
				res = this->expected_size - this->msg.size();
		}
		unlock();
		return res;
	}

	void lock() {
		EnterCriticalSection(&access_lock);
	}

	void unlock() {
		LeaveCriticalSection(&access_lock);
	}
} ISO15765_FRAMESET;

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

