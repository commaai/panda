#include "stdafx.h"
#include "J2534Connection_ISO15765.h"
#include "Timer.h"
#include "constants_ISO15765.h"
#include <chrono>

J2534Connection_ISO15765::J2534Connection_ISO15765(
	std::shared_ptr<PandaJ2534Device> panda_dev,
	unsigned long ProtocolID,
	unsigned long Flags,
	unsigned long BaudRate
) : J2534Connection(panda_dev, ProtocolID, Flags, BaudRate) {
	this->port = 0;

	if (BaudRate % 100 || BaudRate < 10000 || BaudRate > 5000000)
		throw ERR_INVALID_BAUDRATE;

	panda_dev->panda->set_can_speed_cbps(panda::PANDA_CAN1, (uint16_t)(BaudRate / 100));
}

long J2534Connection_ISO15765::PassThruWriteMsgs(PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	for (unsigned int msgnum = 0; msgnum < *pNumMsgs; msgnum++) {
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
		if (msg->DataSize > 11 && fid == -1) return ERR_NO_FLOW_CONTROL; //11 bytes (4 for CANid, 7 payload) is max length of input frame.

		auto msgtx = std::make_shared<MessageTx_ISO15765>( shared_from_this(), *msg, (fid == -1) ? nullptr : this->filters[fid] );
		this->schedultMsgTx(std::dynamic_pointer_cast<MessageTx>(msgtx));
	}
	return STATUS_NOERROR;
}

void J2534Connection_ISO15765::processMessageReceipt(const J2534Frame& msg) {
	if (msg.ProtocolID != CAN) return;

	//TX_MSG_TYPE should be set in RxStatus
	if (!check_bmask(msg.RxStatus, TX_MSG_TYPE)) return;
}

//https://happilyembedded.wordpress.com/2016/02/15/can-multiple-frame-transmission/
void J2534Connection_ISO15765::processMessage(const J2534Frame& msg) {
	if (msg.ProtocolID != CAN) return;

	int fid = get_matching_in_fc_filter_id(msg);
	if (fid == -1) return;

	auto filter = this->filters[fid];
	bool is_ext_addr = check_bmask(filter->flags, ISO15765_ADDR_TYPE);
	uint8_t addrlen = is_ext_addr ? 5 : 4;

	switch (msg_get_type(msg, addrlen)) {
	case FRAME_FLOWCTRL:
		{
			/*if (msg.Data.size() < addrlen + 3) return;
			uint8_t flow_status = msg.Data[addrlen] & 0x0F;
			uint8_t block_size = msg.Data[addrlen + 1];
			uint8_t st_min = msg.Data[addrlen + 2];
			switch (flow_status) {
			case FLOWCTRL_CONTINUE: {
				if (st_min > 0xF9) break;
				if (st_min >= 0xf1 && st_min <= 0xf9) {
					this->rxConversations[fid]->tx_flowcontrol(block_size, std::chrono::microseconds((st_min & 0x0F) * 100));
				} else {
					this->rxConversations[fid]->tx_flowcontrol(block_size, std::chrono::microseconds(st_min * 1000));
				}
				if (auto panda_dev_sp = this->panda_dev.lock()) {
					panda_dev_sp->registerMultiPartTx(this->conversations[fid]);
				}
				break;
			}
			case FLOWCTRL_WAIT:
				this->rxConversations[fid]->tx_flowcontrol(0, std::chrono::microseconds(0));
				break;
			case FLOWCTRL_ABORT:
				this->rxConversations[fid] = nullptr;
				break;
			}*/
			break;
		}
	case FRAME_SINGLE:
		{
			this->rxConversations[fid] = nullptr; //Reset any current transaction.

			if (is_ext_addr) {
				if ((msg.Data[5] & 0x0F) > 6) return;
			} else {
				if ((msg.Data[4] & 0x0F) > 7) return;
			}

			J2534Frame outframe(ISO15765, msg.RxStatus, 0, msg.Timestamp);
			if (msg.Data.size() != 8 && check_bmask(this->Flags, ISO15765_FRAME_PAD))
				outframe.RxStatus |= ISO15765_PADDING_ERROR;
			if (is_ext_addr)
				outframe.RxStatus |= ISO15765_ADDR_TYPE;
			outframe.Data = msg.Data.substr(0, addrlen) + msg.Data.substr(addrlen + 1, msg.Data[addrlen]);
			outframe.ExtraDataIndex = outframe.Data.size();

			addMsgToRxQueue(outframe);
			break;
		}
	case FRAME_FIRST:
		{
			if (msg.Data.size() < 12) {
				//A frame was received that could have held more data.
				//No examples of this protocol show that happening, so
				//it will be assumed that it is grounds to reset rx.
				this->rxConversations[fid] = nullptr;
				return;
			}

			J2534Frame outframe(ISO15765, msg.RxStatus | START_OF_MESSAGE, 0, msg.Timestamp);
			if (is_ext_addr)
				outframe.RxStatus |= ISO15765_ADDR_TYPE;
			outframe.Data = msg.Data.substr(0, addrlen);

			addMsgToRxQueue(outframe);

			this->rxConversations[fid] = std::make_shared<MessageRx>(
				((msg.Data[addrlen] & 0x0F) << 8) | msg.Data[addrlen + 1],
				msg.Data.substr(addrlen + 2, 12 - (addrlen + 2)),
				msg.RxStatus, filter);

			//TODO maybe the flow control should also be scheduled in the TX list.
			//Doing it this way because the filter can be 5 bytes in ext address mode.
			std::string flowfilter = filter->get_flowctrl();
			uint32_t flow_addr = (((uint8_t)flowfilter[0]) << 24) | ((uint8_t)(flowfilter[1]) << 16) | ((uint8_t)(flowfilter[2]) << 8) | ((uint8_t)flowfilter[3]);

			std::string flowstrlresp;
			if (flowfilter.size() > 4)
				flowstrlresp += flowfilter[4];
			flowstrlresp += std::string("\x30\x00\x00", 3);

			if (auto panda_dev_sp = this->panda_dev.lock()) {
				panda_dev_sp->panda->can_send(flow_addr, val_is_29bit(msg.RxStatus), (const uint8_t *)flowstrlresp.c_str(), (uint8_t)flowstrlresp.size(), panda::PANDA_CAN1);
			}
			break;
		}
	case FRAME_CONSEC:
		{
			auto& convo = this->rxConversations[fid];
			if (convo == nullptr) return;

			if (!convo->rx_add_frame(msg.Data[addrlen], (is_ext_addr ? 6 : 7), msg.Data.substr(addrlen + 1))) {
				//Delete this conversation.
				convo = nullptr;
				return;
			}

			std::string final_msg;
			if (convo->flush_result(final_msg)) {
				convo = nullptr;
				J2534Frame outframe(ISO15765, msg.RxStatus, 0, msg.Timestamp);
				if (is_ext_addr)
					outframe.RxStatus |= ISO15765_ADDR_TYPE;
				outframe.Data = msg.Data.substr(0, addrlen) + final_msg;
				outframe.ExtraDataIndex = outframe.Data.size();

				addMsgToRxQueue(outframe);
			}
			break;
		}
	}
}

long J2534Connection_ISO15765::PassThruStartMsgFilter(unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg,
	PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID) {

	if (FilterType != FLOW_CONTROL_FILTER) return ERR_INVALID_FILTER_ID;
	return J2534Connection::PassThruStartMsgFilter(FilterType, pMaskMsg, pPatternMsg, pFlowControlMsg, pFilterID);
}

int J2534Connection_ISO15765::get_matching_out_fc_filter_id(const std::string& msgdata, unsigned long flags, unsigned long flagmask) {
	for (unsigned int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		auto filter = this->filters[i]->get_flowctrl();
		if (filter == msgdata.substr(0, filter.size()) &&
			(this->filters[i]->flags & flagmask) == (flags & flagmask))
			return i;
	}
	return -1;
}

int J2534Connection_ISO15765::get_matching_in_fc_filter_id(const J2534Frame& msg, unsigned long flagmask) {
	for (unsigned int i = 0; i < this->filters.size(); i++) {
		if (this->filters[i] == nullptr) continue;
		if (this->filters[i]->check(msg) == FILTER_RESULT_MATCH &&
			(this->filters[i]->flags & flagmask) == (msg.RxStatus & flagmask))
			return i;
	}
	return -1;
}
