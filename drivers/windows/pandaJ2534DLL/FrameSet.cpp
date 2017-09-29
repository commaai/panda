#include "stdafx.h"
#include "FrameSet.h"

FrameSet::FrameSet() : msg(""), consumed_count(0), expected_size(0), next_part(0), istx(FALSE), flags(0) { }

std::shared_ptr<FrameSet> FrameSet::init_tx(std::string& payload, std::weak_ptr<J2534MessageFilter> filter) {
	auto res = std::make_shared<FrameSet>();
	res->consumed_count = 0;
	res->msg = payload;
	res->next_part = 1;
	res->istx = TRUE;
	res->filter = filter;
	return res;
}

std::shared_ptr<FrameSet> FrameSet::init_rx_first_frame(uint16_t final_size, const std::string& piece, unsigned long rxFlags) {
	auto res = std::make_shared<FrameSet>();
	res->expected_size = final_size & 0xFFF;
	res->msg.reserve(res->expected_size);
	res->msg = piece;
	res->next_part = 1;
	res->flags = rxFlags;
	res->istx = FALSE;
	return res;
}

bool FrameSet::rx_add_frame(uint8_t pcibyte, unsigned int max_packet_size, const std::string piece) {
	synchronized(access_lock) {
		if ((pcibyte & 0x0F) != this->next_part) {
			//TODO: Maybe this should instantly fail the transaction.
			return TRUE;
		}

		this->next_part = (this->next_part + 1) % 0x10;
		unsigned int payload_len = min(expected_size - msg.size(), max_packet_size);
		if (piece.size() < payload_len) {
			//A frame was received that could have held more data.
			//No examples of this protocol show that happening, so
			//it will be assumed that it is grounds to reset rx.
			return FALSE;
		}
		msg += piece.substr(0, payload_len);

	}
	return TRUE;
}

void FrameSet::reset() {
	synchronized(access_lock) {
		expected_size = 0;
		msg = "";
	}
}

unsigned int FrameSet::bytes_remaining() {
	synchronized(access_lock) {
		if (istx)
			return this->msg.size() - this->consumed_count;
		else
			return this->expected_size - this->msg.size();
	}
	return 0; //suppress warning
}

bool FrameSet::flush_result(std::string& final_msg) {
	synchronized(access_lock) {
		if (this->msg.size() == this->expected_size) {
			final_msg = this->msg;
			return TRUE;
		}
	}
	return FALSE;
}
