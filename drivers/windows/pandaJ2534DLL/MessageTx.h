#pragma once
#include <memory>
#include "J2534Connection.h"
#include "J2534Frame.h"

class J2534Connection;

class MessageTx
{
public:
	MessageTx(
		std::weak_ptr<J2534Connection> connection
	) : connection(connection) { };

	virtual BOOL sendNextFrame() = 0;

	virtual BOOL checkTxReceipt(J2534Frame frame) { return FALSE; };

	virtual BOOL isFinished() { return TRUE; };

	virtual BOOL txReady() { return TRUE; };

	std::weak_ptr<J2534Connection> connection;
	std::chrono::microseconds separation_time;
};
