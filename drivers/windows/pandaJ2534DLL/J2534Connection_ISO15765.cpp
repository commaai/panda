#include "stdafx.h"
#include "J2534Connection_ISO15765.h"
#include "Timer.h"

#define msg_is_extaddr(msg) check_bmask(msg->TxFlags, ISO15765_ADDR_TYPE)
#define msg_is_padded(msg) check_bmask(msg->TxFlags, ISO15765_FRAME_PAD)

#define FRAME_SINGLE   0x00
#define FRAME_FIRST    0x10
#define FRAME_CONSEC   0x20
#define FRAME_FLOWCTRL 0x30

#define msg_get_type(msg, addrlen)   ((msg).Data[addrlen] & 0xF0)

#define is_single(msg, addrlen)      (msg_get_type(msg, addrlen) == FRAME_SINGLE)
#define is_first(msg, addrlen)       (msg_get_type(msg, addrlen) == FRAME_FIRST)
#define is_consecutive(msg, addrlen) (msg_get_type(msg, addrlen) == FRAME_CONSEC)
#define is_flowctrl(msg, addrlen)    (msg_get_type(msg, addrlen) == FRAME_FLOWCTRL)

J2534Connection_ISO15765::J2534Connection_ISO15765(
	std::shared_ptr<PandaJ2534Device> panda_dev,
	unsigned long ProtocolID,
	unsigned long Flags,
	unsigned long BaudRate
) : J2534Connection(panda_dev, ProtocolID, Flags, BaudRate) {
	this->port = 0;

	if (BaudRate % 100 || BaudRate < 10000 || BaudRate > 5000000)
		throw ERR_INVALID_BAUDRATE;

	panda_dev->panda->set_can_speed_cbps(panda::PANDA_CAN1, BaudRate / 100); //J2534Connection_CAN(panda_dev, ProtocolID, Flags, BaudRate) {};
}

long J2534Connection_ISO15765::PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	for (int msgnum = 0; msgnum < *pNumMsgs; msgnum++) {
		PASSTHRU_MSG* msg = &pMsg[msgnum];
		if (msg->ProtocolID != this->ProtocolID) {
			*pNumMsgs = msgnum;
			return ERR_MSG_PROTOCOL_ID;
		}
		if (msg->DataSize < this->getMinMsgLen() + (msg_is_extaddr(msg) ? 1 : 0) ||
			msg->DataSize > this->getMaxMsgLen() + (msg_is_extaddr(msg) ? 1 : 0) ||
			(val_is_29bit(msg->TxFlags) != this->_is_29bit() && !check_bmask(this->Flags, CAN_ID_BOTH))) {
			*pNumMsgs = msgnum;
			return ERR_INVALID_MSG;
		}

		int fid = get_matching_out_fc_filter_id(std::string((const char*)msg->Data, msg->DataSize), msg->TxFlags, 0xFFFFFFFF);
		if (fid == -1) return ERR_NO_FLOW_CONTROL;

		uint32_t addr = ((uint8_t)msg->Data[0]) << 24 | ((uint8_t)msg->Data[1]) << 16 | ((uint8_t)msg->Data[2]) << 8 | ((uint8_t)msg->Data[3]);
		uint8_t snd_buff[8] = { 0 };
		uint8_t addrlen = msg_is_extaddr(msg) ? 5 : 4;
		unsigned long payload_len = msg->DataSize - addrlen;
		unsigned int idx = 0;

		if (msg_is_extaddr(msg))
			snd_buff[idx++] = msg->Data[4]; //EXT ADDR byte

		if (payload_len <= (msg_is_extaddr(msg) ? 6 : 7)) {
			snd_buff[idx++] = payload_len;
		} else {
			printf("LONG MSG\n");
			//TODO Make work will full TX sequence. Currently only sends first frame.
			snd_buff[idx++] = 0x10 | ((payload_len >> 8) & 0xF);
			snd_buff[idx++] = payload_len & 0xFF;
		}

		memcpy_s(&snd_buff[idx], sizeof(snd_buff) - idx, &msg->Data[addrlen], payload_len);
		if (auto panda_dev_sp = this->panda_dev.lock()) {
			if (panda_dev_sp->panda->can_send(addr, val_is_29bit(msg->TxFlags), snd_buff,
				(msg_is_padded(msg) ? sizeof(snd_buff) : (payload_len + idx)), panda::PANDA_CAN1) == FALSE) {
				*pNumMsgs = msgnum;
				return ERR_INVALID_MSG;
			}
		}
	}
	return STATUS_NOERROR;
}

void J2534Connection_ISO15765::processMessageReceipt(const PASSTHRU_MSG_INTERNAL& msg) {
	PASSTHRU_MSG_INTERNAL outframe = {};
	if (msg.ProtocolID != CAN) return;

	//TX_MSG_TYPE should be set in RxStatus
	if (!check_bmask(msg.RxStatus, TX_MSG_TYPE)) return;

	int fid = get_matching_out_fc_filter_id(msg.Data, msg.RxStatus, CAN_29BIT_ID);
	if (fid == -1) return;

	uint8_t addrlen = check_bmask(this->filters[fid]->flags, ISO15765_ADDR_TYPE) ? 5 : 4;

	if (msg.Data.size() >= addrlen + 1 && (msg.Data[addrlen] & 0xF0) == FRAME_FLOWCTRL) return;

	this->conversations[fid].reset();

	outframe.ProtocolID = ISO15765;
	outframe.Timestamp = msg.Timestamp;
	outframe.RxStatus = msg.RxStatus | TX_MSG_TYPE | TX_INDICATION;
	if (check_bmask(this->filters[fid]->flags, ISO15765_ADDR_TYPE))
		outframe.RxStatus |= ISO15765_ADDR_TYPE;
	outframe.ExtraDataIndex = 0;
	outframe.TxFlags = 0;
	outframe.Data = msg.Data.substr(0, addrlen);

	synchronized(message_access_lock) {
		this->messages.push(outframe);
	}
}

//https://happilyembedded.wordpress.com/2016/02/15/can-multiple-frame-transmission/
void J2534Connection_ISO15765::processMessage(const PASSTHRU_MSG_INTERNAL& msg) {
	PASSTHRU_MSG_INTERNAL outframe = {};
	if (msg.ProtocolID != CAN) return;

	int fid = get_matching_in_fc_filter_id(msg);
	if (fid == -1) return;

	auto filter = this->filters[fid];
	auto& convo = this->conversations[fid];
	bool is_ext_addr = check_bmask(filter->flags, ISO15765_ADDR_TYPE);
	uint8_t addrlen = is_ext_addr ? 5 : 4;

	switch (msg_get_type(msg, addrlen)) {
	case FRAME_SINGLE:
		convo.reset(); //Reset any current transaction.

		if (is_ext_addr) {
			if ((msg.Data[5] & 0x0F) > 6) return;
		}
		else {
			if ((msg.Data[4] & 0x0F) > 7) return;
		}

		outframe.ProtocolID = ISO15765;
		outframe.Timestamp = msg.Timestamp;
		outframe.RxStatus = msg.RxStatus;
		outframe.TxFlags = 0;
		if (msg.Data.size() != 8 && check_bmask(this->Flags, ISO15765_FRAME_PAD))
			outframe.RxStatus |= ISO15765_PADDING_ERROR;
		if (is_ext_addr)
			outframe.RxStatus |= ISO15765_ADDR_TYPE;
		outframe.Data = msg.Data.substr(0, addrlen) + msg.Data.substr(addrlen + 1, msg.Data[addrlen]);
		outframe.ExtraDataIndex = outframe.Data.size();

		synchronized(message_access_lock) {
			this->messages.push(outframe);
		}
		break;
	case FRAME_FIRST:
	{
		if (msg.Data.size() < 12) {
			//A frame was received that could have held more data.
			//No examples of this protocol show that happening, so
			//it will be assumed that it is grounds to reset rx.
			convo.reset();
			return;
		}

		outframe.Data = msg.Data.substr(0, addrlen);
		outframe.ProtocolID = ISO15765;
		outframe.Timestamp = msg.Timestamp;
		outframe.RxStatus = msg.RxStatus | START_OF_MESSAGE;
		if (is_ext_addr)
			outframe.RxStatus |= ISO15765_ADDR_TYPE;
		outframe.ExtraDataIndex = 0;
		outframe.TxFlags = 0;
		synchronized(message_access_lock) {
			this->messages.push(outframe);
		}

		convo.init_rx_first_frame(((msg.Data[addrlen] & 0x0F) << 8) | msg.Data[addrlen + 1], msg.Data.substr(addrlen + 2, 12 - (addrlen + 2)), msg.RxStatus);

		//Doing it this way because the filter can be 5 bytes in ext address mode.
		std::string flowfilter = filter->get_flowctrl();
		uint32_t flow_addr = (((uint8_t)flowfilter[0]) << 24) | ((uint8_t)(flowfilter[1]) << 16) | ((uint8_t)(flowfilter[2]) << 8) | ((uint8_t)flowfilter[3]);

		std::string flowstrlresp;
		if (flowfilter.size() > 4)
			flowstrlresp += flowfilter[4];
		flowstrlresp += std::string("\x30\x00\x00", 3);

		if (auto panda_dev_sp = this->panda_dev.lock()) {
			panda_dev_sp->panda->can_send(flow_addr, val_is_29bit(msg.RxStatus), (const uint8_t *)flowstrlresp.c_str(), flowstrlresp.size(), panda::PANDA_CAN1);
		}
		break;
	}
	case FRAME_CONSEC:
		convo.lock();
		{
			if (convo.active == 0) {
				convo.unlock();
				return;
			}
			if ((msg.Data[addrlen] & 0x0F) != convo.next_part) {
				convo.unlock();
				return;
			}
			convo.next_part = (convo.next_part + 1) % 0x10;
			unsigned int payload_len = min(convo.bytes_remaining(), (is_ext_addr ? 6 : 7));
			if (msg.Data.size() < (addrlen + 1 + payload_len)) {
				//A frame was received that could have held more data.
				//No examples of this protocol show that happening, so
				//it will be assumed that it is grounds to reset rx.
				convo.reset();
				convo.unlock();
				return;
			}
			convo.msg += msg.Data.substr(addrlen + 1, payload_len);

			if (convo.msg.size() == convo.expected_size) {
				outframe.ProtocolID = ISO15765;
				outframe.Timestamp = msg.Timestamp;
				outframe.RxStatus = msg.RxStatus;
				if (is_ext_addr)
					outframe.RxStatus |= ISO15765_ADDR_TYPE;
				outframe.TxFlags = 0;
				outframe.Data = msg.Data.substr(0, addrlen) + convo.msg;
				outframe.ExtraDataIndex = outframe.Data.size();

				synchronized(message_access_lock) {
					this->messages.push(outframe);
				}

				convo.reset();
			}
		}
		convo.unlock();
		break;
	}
}

unsigned long J2534Connection_ISO15765::getMinMsgLen() {
	return 4;
}

unsigned long J2534Connection_ISO15765::getMaxMsgLen() {
	return 4099;
}

long J2534Connection_ISO15765::PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg,
	PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID) {

	if (FilterType != FLOW_CONTROL_FILTER) return ERR_INVALID_FILTER_ID;
	return J2534Connection::PassThruStartMsgFilter(FilterType, pMaskMsg, pPatternMsg, pFlowControlMsg, pFilterID);
}

int J2534Connection_ISO15765::get_matching_out_fc_filter_id(const std::string& msgdata, unsigned long flags, unsigned long flagmask) {
	for (int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		auto filter = this->filters[i]->get_flowctrl();
		if (filter == msgdata.substr(0, filter.size()) &&
			(this->filters[i]->flags & flagmask) == (flags & flagmask))
			return i;
	}
	return -1;
}

int J2534Connection_ISO15765::get_matching_in_fc_filter_id(const PASSTHRU_MSG_INTERNAL& msg, unsigned long flagmask) {
	for (int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		if (this->filters[i]->check(msg) == FILTER_RESULT_MATCH &&
			(this->filters[i]->flags & flagmask) == (msg.RxStatus & flagmask))
			return i;
	}
	return -1;
}