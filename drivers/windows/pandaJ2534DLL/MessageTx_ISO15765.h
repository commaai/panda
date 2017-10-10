#pragma once
#include "MessageTx.h"
#include "J2534Connection_ISO15765.h"

class J2534Connection_ISO15765;

class MessageTx_ISO15765 : public MessageTx
{
public:
	MessageTx_ISO15765(
		std::shared_ptr<J2534Connection> connection,
		PASSTHRU_MSG& to_send,
		std::shared_ptr<J2534MessageFilter> filter
	);

	unsigned int addressLength();

	virtual BOOL sendNextFrame();

	virtual BOOL checkTxReceipt(J2534Frame frame);

	virtual BOOL isFinished();

	virtual BOOL txReady();

	void tx_flowcontrol(uint8_t block_size, std::chrono::microseconds separation_time, BOOL sendAll = FALSE);

	std::shared_ptr<J2534MessageFilter> filter;
	unsigned long frames_sent;
	unsigned long consumed_count;
	uint8_t block_size;
	unsigned long CANid;
	std::string data_prefix;
	std::string payload;
	BOOL isMultipart;
	std::vector<std::string> framePayloads;
	BOOL txInFlight;
	BOOL sendAll;
};
