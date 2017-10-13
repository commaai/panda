#pragma once
#include "MessageTx.h"

class MessageTxTimeout;

class MessageTxTimeoutable : public std::enable_shared_from_this<MessageTxTimeoutable>, public MessageTx
{
public:
	MessageTxTimeoutable(
		std::weak_ptr<J2534Connection> connection
	) : MessageTx(connection), recvCount(0) { };

	unsigned long getRecvCount() {
		return recvCount;
	}

	virtual void onTimeout() = 0;

protected:
	unsigned long recvCount;

	void scheduleTimeout(std::chrono::microseconds timeoutus) {
		if (auto conn_sp = this->connection.lock()) {
			if (auto panda_dev_sp = conn_sp->getPandaDev()) {
				auto timeoutobj = std::make_shared<MessageTxTimeout>(shared_from_this(), timeoutus);
				panda_dev_sp->registerMessageTx(std::static_pointer_cast<MessageTx>(timeoutobj), TRUE);
			}
		}
	}

	void scheduleTimeout(unsigned long timeoutus) {
		scheduleTimeout(std::chrono::microseconds(timeoutus));
	}
};


class MessageTxTimeout : public MessageTx
{
public:
	MessageTxTimeout(
		std::shared_ptr<MessageTxTimeoutable> msg,
		std::chrono::microseconds timeout
	) : MessageTx(msg->connection), msg(msg), lastRecvCount(msg->getRecvCount()) {
		separation_time = timeout;
	};

	MessageTxTimeout(
		std::shared_ptr<MessageTxTimeoutable> msg,
		unsigned long timeout
	) : MessageTxTimeout(msg, std::chrono::microseconds(timeout * 1000)) { };

	virtual BOOL sendNextFrame() {
		if (auto msg_sp = this->msg.lock()) {
			if (msg_sp->getRecvCount() == this->lastRecvCount) {
				msg_sp->onTimeout();
			}
		}
		return FALSE; //Do not add this to the echo queue.
	};

private:
	std::weak_ptr<MessageTxTimeoutable> msg;
	unsigned long lastRecvCount;
};
