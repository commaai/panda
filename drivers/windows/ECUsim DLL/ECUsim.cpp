#include "stdafx.h"
#include "ECUsim.h"

ECUsim::ECUsim(std::string sn, unsigned long can_baud) : doloop(TRUE), verbose(TRUE), can11b_enabled(TRUE), can29b_enabled(TRUE){
	this->panda = panda::Panda::openPanda(sn);
	this->panda->set_can_speed_cbps(panda::PANDA_CAN1, can_baud / 100); //Don't pass in baud where baud%100 != 0
	this->panda->set_safety_mode(panda::SAFETY_ALLOUTPUT);
	this->panda->set_can_loopback(FALSE);
	this->panda->can_clear(panda::PANDA_CAN_RX);

	DWORD threadid;
	this->thread_can = CreateThread(NULL, 0, _canthreadBootstrap, (LPVOID)this, 0, &threadid);

    return;
}

void ECUsim::stop() {
	this->doloop = FALSE;
}

void ECUsim::join() {
	WaitForSingleObject(this->thread_can, INFINITE);
}

DWORD WINAPI ECUsim::_canthreadBootstrap(LPVOID This) {
	return ((ECUsim*)This)->can_recv_thread_function();
}

DWORD ECUsim::can_recv_thread_function() {
	while (this->doloop) {
		auto msgs = this->panda->can_recv();
		for (auto& msg : msgs) {
			if (msg.bus == 0 && !msg.is_receipt /*&& msg.len == 8*/ && msg.dat[0] >= 2) {
				if (this->verbose) {
					printf("Processing message (bus: %d; addr: %X; 29b: %d):\n    ", msg.bus, msg.addr, msg.addr_29b);
					for (int i = 0; i < msg.len; i++) printf("%02X ", msg.dat[i]);
					printf("\n");
				}
				this->_CAN_process_msg(msg);
			} else {
				if (this->verbose) {
					printf("Rejecting message (bus: %d; addr: %X; 29b: %d):\n    ", msg.bus, msg.addr, msg.addr_29b);
					for (int i = 0; i < msg.len; i++) printf("%02X ", msg.dat[i]);
					printf("\n");
				}
			}
		}
	}

	return 0;
}

BOOL ECUsim::_can_addr_matches(panda::PANDA_CAN_MSG& msg) {
	if (this->can11b_enabled && !msg.addr_29b && (msg.addr == 0x7DF || (msg.addr & 0x7F8) == 0x7E0))
		return TRUE;
	if (this->can29b_enabled && msg.addr_29b && ((msg.addr & 0x1FFF00FF) == 0x18DB00F1 || (msg.addr & 0x1FFF00FF) == 0x18da00f1))
		return TRUE;
	return FALSE;
}

void ECUsim::_CAN_process_msg(panda::PANDA_CAN_MSG& msg) {
	std::string outmsg;
	uint32_t outaddr;
	uint8_t formatted_msg_buff[8];
	bool doreply;

	if (this->_can_addr_matches(msg)) {// && msg.len == 8) {
		if (memcmp(msg.dat, "\x30\x00\x00", 3) == 0 && this->can_multipart_data.size() > 0) {
			if (this->verbose) printf("Request for more data");
			outaddr = (msg.addr == 0x7DF || msg.addr == 0x7E0) ? 0x7E8 : 0x18DAF110;
			unsigned int msgnum = 1;
			while (this->can_multipart_data.size()) {
				unsigned int datalen = min(7, this->can_multipart_data.size());

				formatted_msg_buff[0] = 0x20 | msgnum;
				for (int i = 0; i < datalen; i++) {
					formatted_msg_buff[i + 1] = this->can_multipart_data.front();
					this->can_multipart_data.pop();
				}
				this->panda->can_send(outaddr, msg.addr_29b, formatted_msg_buff, datalen, panda::PANDA_CAN1);
				msgnum = (msgnum + 1) % 0x10;
				Sleep(10);
			}
		} else {
			outmsg = this->process_obd_msg(msg.dat[1], msg.dat[2], doreply);
		}

		if (doreply) {
			outaddr = (msg.addr_29b) ? 0x18DAF1EF : 0x7E8;

			if (outmsg.size() <= 5) {
				formatted_msg_buff[0] = outmsg.size() + 2;
				formatted_msg_buff[1] = 0x40 | msg.dat[1];
				formatted_msg_buff[2] = msg.dat[2]; //PID
				memcpy_s(&formatted_msg_buff[3], sizeof(formatted_msg_buff) - 3, outmsg.c_str(), outmsg.size());
				for (int i = 3 + outmsg.size(); i < 8; i++)
					formatted_msg_buff[i] = 0;

				if (this->verbose) {
					printf("Replying to %X.\n    ", outaddr);
					for (int i = 0; i < 8; i++) printf("%02X ", formatted_msg_buff[i]);
					printf("\n");
				}

				this->panda->can_send(outaddr, msg.addr_29b, formatted_msg_buff, 8, panda::PANDA_CAN1); //outmsg.size() + 3
			} else {
				uint8_t first_msg_len = min(3, outmsg.size() % 7);
				uint8_t payload_len = outmsg.size() + 3;

				formatted_msg_buff[0] = 0x10 | ((payload_len >> 8) & 0xF);
				formatted_msg_buff[1] = payload_len & 0xFF;
				formatted_msg_buff[2] = 0x40 | msg.dat[1];
				formatted_msg_buff[3] = msg.dat[2]; //PID
				formatted_msg_buff[4] = 1;
				memcpy_s(&formatted_msg_buff[3], sizeof(formatted_msg_buff) - 3, outmsg.c_str(), first_msg_len);

				this->panda->can_send(outaddr, msg.addr_29b, formatted_msg_buff, 8, panda::PANDA_CAN1);
				for (int i = first_msg_len; i < outmsg.size(); i++)
					this->can_multipart_data.push(outmsg[i]);
			}
		}
	}
}

std::string ECUsim::process_obd_msg(UCHAR mode, UCHAR pid, bool& return_data) {
	std::string tmp;
	return_data = TRUE;

	switch (mode) {
	case 0x01: // Mode : Show current data
		switch (pid) {
		case 0x00: //List supported things
			return "\xff\xff\xff\xfe"; //b"\xBE\x1F\xB8\x10" #Bitfield, random features
		case 0x01: // Monitor Status since DTC cleared
			return "\x00\x00\x00\x00"; //Bitfield, random features
		case 0x04: // Calculated engine load
			return "\x2f";
		case 0x05: // Engine coolant temperature
			return "\x3c";
		case 0x0B: // Intake manifold absolute pressure
			return "\x90";
		case 0x0C: // Engine RPM
			return "\x1A\xF8";
		case 0x0D: // Vehicle Speed
			return "\x53";
		case 0x10: // MAF air flow rate
			return "\x01\xA0";
		case 0x11: // Throttle Position
			return "\x90";
		case 0x33: // Absolute Barometric Pressure
			return "\x90";
		default:
			return_data = FALSE;
			return "";
		}
	case 0x09: // Mode : Request vehicle information
		switch (pid) {
		case 0x02: // Show VIN
			return "1D4GP00R55B123456";
		case 0xFC: // test long multi message.Ligned up for LIN responses
			for (int i = 0; i < 80; i++) {
				tmp += "\xAA\xAA";
			}
			return tmp;//">BBH", 0xAA, 0xAA, num + 1)
		case 0xFD: // test long multi message
			for (int i = 0; i < 80; i++) {
				tmp += "\xAA\xAA\xAA";
				tmp.push_back(i >> 24);
				tmp.push_back((i >> 16) & 0xFF);
				tmp.push_back((i >> 8) & 0xFF);
				tmp.push_back(i & 0xFF);
			}
			return "\xAA\xAA\xAA" + tmp;
		case 0xFE: // test very long multi message
			tmp = "\xAA\xAA\xAA";
			for (int i = 0; i < 584; i++) {
				tmp += "\xAA\xAA\xAA";
				tmp.push_back(i >> 24);
				tmp.push_back((i >> 16) & 0xFF);
				tmp.push_back((i >> 8) & 0xFF);
				tmp.push_back(i & 0xFF);
			}
			return tmp + "\xAA";
		case 0xFF:
			for (int i = 0; i < 584; i++) {
				tmp += "\xAA\xAA\xAA\xAA\xAA";
				tmp.push_back(((i + 1) >> 8) & 0xFF);
				tmp.push_back((i + 1) & 0xFF);
			}
			return "\xAA\x00\x00" + tmp;
		default:
			return_data = FALSE;
			return "";
		}
	case 0x3E:
		if (pid == 0) {
			return_data = TRUE;
			return "";
		}
		return_data = FALSE;
		return "";
	default:
		return_data = FALSE;
		return "";
	}
}
