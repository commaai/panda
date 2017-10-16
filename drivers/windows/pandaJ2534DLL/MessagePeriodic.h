#pragma once
#include "Action.h"
#include "MessageTx.h"

class J2534Connection;

class MessagePeriodic : public Action, public std::enable_shared_from_this<Action>
{
public:
	MessagePeriodic(
		std::chrono::microseconds delay,
		std::shared_ptr<MessageTx> msg
	);

	virtual void execute();

	void cancel() {
		this->active = FALSE;
	}

protected:
	std::shared_ptr<MessageTx> msg;

private:
	BOOL runyet;
	BOOL active;
};
