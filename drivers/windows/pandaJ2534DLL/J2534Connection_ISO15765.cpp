#include "stdafx.h"
#include "J2534Connection_ISO15765.h"
#include "Timer.h"

#define msg_is_extaddr(msg) check_bmask(msg->TxFlags, ISO15765_ADDR_TYPE)
#define msg_is_padded(msg) check_bmask(msg->TxFlags, ISO15765_FRAME_PAD)

#define FRAME_SINGLE   0x00
#define FRAME_FIRST    0x10
#define FRAME_CONSEC   0x20
#define FRAME_FLOWCTRL 0x30

#define msg_get_type(msg)   ((msg).Data[4] & 0xF0)

#define is_single(msg)      (msg_get_type(msg) == FRAME_SINGLE)
#define is_first(msg)       (msg_get_type(msg) == FRAME_FIRST)
#define is_consecutive(msg) (msg_get_type(msg) == FRAME_CONSEC)
#define is_flowctrl(msg)    (msg_get_type(msg) == FRAME_FLOWCTRL)

J2534Connection_ISO15765::J2534Connection_ISO15765(
	panda::Panda* panda_dev,
	unsigned long ProtocolID,
	unsigned long Flags,
	unsigned long BaudRate
) : J2534Connection(panda_dev, ProtocolID, Flags, BaudRate) {
	this->port = 0;

	if (BaudRate % 100 || BaudRate < 10000 || BaudRate > 5000000)
		throw ERR_INVALID_BAUDRATE;

	this->panda_dev->set_can_speed_cbps(panda::PANDA_CAN1, BaudRate / 100); //J2534Connection_CAN(panda_dev, ProtocolID, Flags, BaudRate) {};
}

long J2534Connection_ISO15765::PassThruReadMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	//Timeout of 0 means return immediately. Non zero means WAIT for that time then return. Dafuk.
	long err_code = STATUS_NOERROR;
	Timer t = Timer();

	unsigned long msgnum = 0;
	while (msgnum < *pNumMsgs) {
		if (Timeout > 0 && t.getTimePassed() >= Timeout) {
			err_code = ERR_TIMEOUT;
			break;
		}

		EnterCriticalSection(&this->message_access_lock);
		if (this->messages.empty()) {
			LeaveCriticalSection(&this->message_access_lock);
			if (Timeout == 0)
				break;
			continue;
		}

		auto msg_in = this->messages.front();
		this->messages.pop();
		LeaveCriticalSection(&this->message_access_lock);

		//if (this->_is_29bit() != msg_in.addr_29b) {}
		PASSTHRU_MSG *msg_out = &pMsg[msgnum++];
		msg_out->ProtocolID = this->ProtocolID;
		msg_out->DataSize = msg_in.Data.size();
		memcpy(msg_out->Data, msg_in.Data.c_str(), msg_in.Data.size());
		msg_out->Timestamp = msg_in.Timestamp;
		msg_out->RxStatus = msg_in.RxStatus;
		msg_out->ExtraDataIndex = msg_in.ExtraDataIndex;
		msg_out->TxFlags = 0;
		if (msgnum == *pNumMsgs) break;
	}

	if (msgnum == 0)
		err_code = ERR_BUFFER_EMPTY;
	*pNumMsgs = msgnum;
	return err_code;
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

		//MULTI PACKET MSG. No EXT SUPPORT YET
		//TODO disable if doesn't match flow control
		printf("LONG MSG, checking filters!...");
		int fid = get_matching_filter_id_for_outmsg(msg);
		if (fid == -1) return ERR_NO_FLOW_CONTROL;
		printf("PASSED FILTERS\n");

		uint32_t addr = ((uint8_t)msg->Data[0]) << 24 | ((uint8_t)msg->Data[1]) << 16 | ((uint8_t)msg->Data[2]) << 8 | ((uint8_t)msg->Data[3]);
		uint8_t snd_buff[8] = { 0 };
		unsigned long payload_len = msg->DataSize - 4;

		if (payload_len <= (msg_is_extaddr(msg) ? 6 : 7)) {
			uint8_t headerlen;
			if (msg_is_extaddr(msg)) {
				headerlen = 2;
				snd_buff[0] = msg->Data[4]; //EXT ADDR byte
				snd_buff[1] = payload_len;
				memcpy_s(&snd_buff[2], sizeof(snd_buff) - 2, &msg->Data[5], payload_len);
			} else {
				headerlen = 1;
				snd_buff[0] = payload_len;
				memcpy_s(&snd_buff[1], sizeof(snd_buff) - 1, &msg->Data[4], payload_len);
			}
			if (this->panda_dev->can_send(addr, val_is_29bit(msg->TxFlags), snd_buff,
				(msg_is_padded(msg) ? sizeof(snd_buff) : (payload_len + headerlen)), panda::PANDA_CAN1) == FALSE) {
				*pNumMsgs = msgnum;
				return ERR_INVALID_MSG;
			}
		} else {
			printf("LONG MSG\n");
			//TODO Make work will full TX sequence. Currently only sends first frame.
			uint8_t headerlen = 1;
			snd_buff[0] = 0x10 | ((payload_len >> 8) & 0xF);
			snd_buff[1] = payload_len & 0xFF;
			memcpy_s(&snd_buff[2], sizeof(snd_buff) - 2, &msg->Data[4], payload_len);

			if (this->panda_dev->can_send(addr, val_is_29bit(msg->TxFlags), snd_buff,
				(msg_is_padded(msg) ? sizeof(snd_buff) : (payload_len + headerlen)), panda::PANDA_CAN1) == FALSE) {
				*pNumMsgs = msgnum;
				return ERR_INVALID_MSG;
			}
			return ERR_INVALID_MSG;
		}
	}
	return STATUS_NOERROR;
}

//https://happilyembedded.wordpress.com/2016/02/15/can-multiple-frame-transmission/
void J2534Connection_ISO15765::processMessage(const PASSTHRU_MSG_INTERNAL& msg) {
	PASSTHRU_MSG_INTERNAL outframe = {};

	if (msg.ProtocolID != CAN) return;
	if (check_bmask(msg.RxStatus, TX_MSG_TYPE)) {
		if (msg.Data.size() >= 5 && (msg.Data[4] & 0xF0) == FRAME_FLOWCTRL) return;

		int fid = get_matching_filter_id_for_outmsg(msg);
		if (fid == -1) return;
		this->conversations[fid].reset();

		outframe.ProtocolID = ISO15765;
		outframe.Timestamp = msg.Timestamp;
		outframe.RxStatus = msg.RxStatus | TX_MSG_TYPE | TX_INDICATION; //TODO REVIEW
		outframe.ExtraDataIndex = 0;
		outframe.TxFlags = 0;
		outframe.Data = msg.Data.substr(0, 4);

		EnterCriticalSection(&this->message_access_lock);
		this->messages.push(outframe);
		LeaveCriticalSection(&this->message_access_lock);
		return;
	}

	int fid = -1;
	for (int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		if (this->filters[i]->check(msg) == FILTER_RESULT_MATCH) {
			fid = i;
			break;
		}
	}
	if (fid == -1) return;
	auto filter = this->filters[fid];
	auto& convo = this->conversations[fid];

	switch (msg_get_type(msg)) {
	case FRAME_SINGLE:
		if ((msg.Data[4] & 0x0F) > 7) return;
		convo.reset(); //Reset any current transaction.

		outframe.ProtocolID = ISO15765;
		outframe.Timestamp = msg.Timestamp;
		outframe.RxStatus = msg.RxStatus;
		outframe.TxFlags = 0;
		if (msg.Data.size() != 8 && check_bmask(this->Flags, ISO15765_FRAME_PAD))
			outframe.RxStatus |= ISO15765_PADDING_ERROR;
		outframe.Data = msg.Data.substr(0, 4) + msg.Data.substr(5, msg.Data[4]);
		outframe.ExtraDataIndex = outframe.Data.size();

		EnterCriticalSection(&this->message_access_lock);
		this->messages.push(outframe);
		LeaveCriticalSection(&this->message_access_lock);
		break;
	case FRAME_FIRST:
	{
		outframe.Data = msg.Data.substr(0, 4);
		outframe.ProtocolID = ISO15765;
		outframe.Timestamp = msg.Timestamp;
		outframe.RxStatus = msg.RxStatus | START_OF_MESSAGE;
		outframe.ExtraDataIndex = 0;
		outframe.TxFlags = 0;
		EnterCriticalSection(&this->message_access_lock);
		this->messages.push(outframe);
		LeaveCriticalSection(&this->message_access_lock);

		convo.init_first_frame(((msg.Data[4] & 0x0F) << 8) | msg.Data[5], msg.Data.substr(6, 6));

		//Doing it this way because the filter can be 5 bytes in ext address mode.
		std::string flowfilter = filter->get_flowctrl();
		uint32_t flow_addr = (((uint8_t)flowfilter[0]) << 24) | ((uint8_t)(flowfilter[1]) << 16) | ((uint8_t)(flowfilter[2]) << 8) | ((uint8_t)flowfilter[3]);

		this->panda_dev->can_send(flow_addr, val_is_29bit(msg.RxStatus), (const uint8_t *)"\x30\x00\x00", 3, panda::PANDA_CAN1);
		break;
	}
	case FRAME_CONSEC:
		if (convo.expected_size == 0) return;
		if ((msg.Data[4] & 0x0F) != convo.next_part) return;
		convo.next_part = (convo.next_part + 1) % 0x10;
		convo.msg += msg.Data.substr(4+1, 7);
		if (convo.msg.size() == convo.expected_size) {
			outframe.ProtocolID = ISO15765;
			outframe.Timestamp = msg.Timestamp;
			outframe.RxStatus = msg.RxStatus;
			outframe.TxFlags = 0;
			outframe.Data = msg.Data.substr(0, 4) + convo.msg;
			outframe.ExtraDataIndex = outframe.Data.size();

			EnterCriticalSection(&this->message_access_lock);
			this->messages.push(outframe);
			LeaveCriticalSection(&this->message_access_lock);

			convo.reset();
		}
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

int J2534Connection_ISO15765::get_matching_filter_id_for_outmsg(const PASSTHRU_MSG *msg) {
	auto data = std::string((const char*)msg->Data, msg->DataSize);
	for (int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		auto filter = this->filters[i]->get_flowctrl();
		if (filter == data.substr(0, filter.size()))
			return i;
	}
	return -1;
}

int J2534Connection_ISO15765::get_matching_filter_id_for_outmsg(const PASSTHRU_MSG_INTERNAL& msg) {
	for (int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		auto filter = this->filters[i]->get_flowctrl();
		if (filter == msg.Data.substr(0, filter.size()))
			return i;
	}
	return -1;
}
