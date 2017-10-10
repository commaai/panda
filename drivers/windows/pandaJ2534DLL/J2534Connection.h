#pragma once
#include "panda/panda.h"
#include "J2534_v0404.h"
#include "synchronize.h"
#include "PandaJ2534Device.h"
#include "J2534MessageFilter.h"
#include "MessageTx.h"
#include "J2534Frame.h"

class J2534MessageFilter;
class PandaJ2534Device;
class MessageTx;

#define check_bmask(num, mask)(((num) & mask) == mask)

class J2534Connection : public std::enable_shared_from_this<J2534Connection> {
	friend class PandaJ2534Device;

public:
	J2534Connection(
		std::shared_ptr<PandaJ2534Device> panda_dev,
		unsigned long ProtocolID,
		unsigned long Flags,
		unsigned long BaudRate
	);
	virtual ~J2534Connection() {};
	virtual long PassThruReadMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);
	virtual long PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);
	virtual long PassThruStartPeriodicMsg(PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval);
	virtual long PassThruStopPeriodicMsg(unsigned long MsgID);

	virtual long PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg,
		PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID);

	virtual long PassThruStopMsgFilter(unsigned long FilterID);
	virtual long PassThruIoctl(unsigned long IoctlID, void *pInput, void *pOutput);

	long init5b(SBYTE_ARRAY* pInput, SBYTE_ARRAY* pOutput);
	long initFast(PASSTHRU_MSG* pInput, PASSTHRU_MSG* pOutput);
	long clearTXBuff();
	long clearRXBuff();
	long clearPeriodicMsgs();
	long clearMsgFilters();

	long setBaud(unsigned long baud);
	unsigned long getBaud() {
		return this->BaudRate;
	}

	unsigned long getProtocol() {
		return this->ProtocolID;
	};

	virtual bool isProtoCan() {
		return FALSE;
	}

	unsigned long getPort() {
		return this->port;
	}

	virtual void processMessageReceipt(const J2534Frame& msg);
	virtual void processMessage(const J2534Frame& msg);

	virtual unsigned long getMinMsgLen() {
		return 1;
	}

	virtual unsigned long getMaxMsgLen() {
		return 4128;
	}

	void schedultMsgTx(std::shared_ptr<MessageTx> msgout);

	void rescheduleExistingTxMsgs();

	std::shared_ptr<PandaJ2534Device> getPandaDev() {
		if (auto panda_dev_sp = this->panda_dev.lock())
			return panda_dev_sp;
		return nullptr;
	}

	void addMsgToRxQueue(const J2534Frame& frame) {
		synchronized(message_access_lock) {
			messages.push(frame);
		}
	}

	bool loopback = FALSE;

protected:
	unsigned long ProtocolID;
	unsigned long Flags;
	unsigned long BaudRate;
	unsigned long port;

	std::weak_ptr<PandaJ2534Device> panda_dev;

	std::queue<J2534Frame> messages;

	std::array<std::shared_ptr<J2534MessageFilter>, 10> filters;
	std::queue<std::shared_ptr<MessageTx>> txbuff;

private:
	Mutex message_access_lock;
	Mutex staged_writes_lock;
};
