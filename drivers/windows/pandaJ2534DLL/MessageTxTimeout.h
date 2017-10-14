#pragma once
#include "Action.h"
#include "MessageTx.h"

class MessageTxTimeout;

class MessageTxTimeoutable : public MessageTx
{
public:
	MessageTxTimeoutable(
		std::weak_ptr<J2534Connection> connection,
		PASSTHRU_MSG& to_send
	);

	unsigned long getRecvCount() {
		return recvCount;
	}

	virtual void onTimeout() = 0;

protected:
	unsigned long recvCount;

	void scheduleTimeout(std::chrono::microseconds timeoutus);

	void scheduleTimeout(unsigned long timeoutus);
};


class MessageTxTimeout : public Action
{
public:
	MessageTxTimeout(
		std::shared_ptr<MessageTxTimeoutable> msg,
		std::chrono::microseconds timeout
	);

	MessageTxTimeout(
		std::shared_ptr<MessageTxTimeoutable> msg,
		unsigned long timeout
	);

	virtual void execute();

private:
	std::weak_ptr<MessageTxTimeoutable> msg;
	unsigned long lastRecvCount;
};
