#pragma once
#include <memory>
#include "J2534Connection.h"
#include "J2534Frame.h"

class J2534Connection;

class MessageTx
{
public:
	MessageTx(
		std::shared_ptr<J2534Connection> connection,
		PASSTHRU_MSG& to_send
	) : connection(connection), fullmsg(to_send) { };//next(nullptr),

	virtual BOOL sendNextFrame() = 0;

	virtual BOOL checkTxReceipt(J2534Frame frame) = 0;

	virtual BOOL isFinished() = 0;

	//std::shared_ptr<class MessageTx> next;
	std::weak_ptr<J2534Connection> connection;
	J2534Frame fullmsg;
};

