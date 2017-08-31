// panda.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "device.h"
#include "panda.h"

#define REQUEST_IN 0xC0
#define REQUEST_OUT 0x40

#define CAN_TRANSMIT 1
#define CAN_EXTENDED 4

using namespace panda;

#pragma pack(1)
typedef struct _PANDA_CAN_MSG_INTERNAL {
	uint32_t rir;
	uint32_t f2;
	uint8_t dat[8];
} PANDA_CAN_MSG_INTERNAL;

Panda::Panda(
	WINUSB_INTERFACE_HANDLE WinusbHandle,
	HANDLE DeviceHandle,
	tstring devPath_,
	std::string sn_
) : usbh(WinusbHandle), devh(DeviceHandle), devPath(devPath_), sn(sn_) {
	printf("CREATED A PANDA %s\n", this->sn.c_str());
}

Panda::~Panda() {
	WinUsb_Free(this->usbh);
	CloseHandle(this->devh);
	printf("Cleanup Panda %s\n", this->sn.c_str());
}

std::vector<std::string> Panda::listAvailablePandas() {
	std::vector<std::string> ret;
	auto map_sn_to_devpath = detect_pandas();

	for (auto kv : map_sn_to_devpath) {
		ret.push_back(std::string(kv.first));
	}

	return ret;
}

std::unique_ptr<Panda> Panda::openPanda(std::string sn)
{
	auto map_sn_to_devpath = detect_pandas();

	if (map_sn_to_devpath.empty()) return nullptr;
	if (map_sn_to_devpath.find(sn) == map_sn_to_devpath.end() && sn != "") return nullptr;

	tstring devpath;
	if (sn.empty()) {
		sn = map_sn_to_devpath.begin()->first;
		devpath = map_sn_to_devpath.begin()->second;
	} else {
		devpath = map_sn_to_devpath[sn];
	}

	HANDLE deviceHandle = CreateFile(devpath.c_str(),
		GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

	if (INVALID_HANDLE_VALUE == deviceHandle) {
		_tprintf(_T("    Error opening Device Handle %d.\n"),// Msg: '%s'\n"),
			GetLastError());// , GetLastErrorAsString().c_str());
		return nullptr;
	}

	WINUSB_INTERFACE_HANDLE winusbHandle;
	if (WinUsb_Initialize(deviceHandle, &winusbHandle) == FALSE) {
		_tprintf(_T("    Error initializing WinUSB %d.\n"),// Msg: '%s'\n"),
			GetLastError());// , GetLastErrorAsString().c_str());
		CloseHandle(deviceHandle);
		return nullptr;
	}
	return std::unique_ptr<Panda>(new Panda(winusbHandle, deviceHandle, map_sn_to_devpath[sn], sn));
}

std::string Panda::get_usb_sn() {
	return std::string(this->sn);
}

int Panda::control_transfer(
	uint8_t			bmRequestType,
	uint8_t  		bRequest,
	uint16_t  		wValue,
	uint16_t  		wIndex,
	void *			data,
	uint16_t		wLength,
	unsigned int  	timeout
) {
	UNREFERENCED_PARAMETER(timeout);

	WINUSB_SETUP_PACKET SetupPacket;
	ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
	ULONG cbSent = 0;

	//Create the setup packet
	SetupPacket.RequestType = bmRequestType;
	SetupPacket.Request = bRequest;
	SetupPacket.Value = wValue;
	SetupPacket.Index = wIndex;
	SetupPacket.Length = wLength;

	//ULONG timeout = 10; // ms
	//WinUsb_SetPipePolicy(interfaceHandle, pipeID, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &timeout);

	if (WinUsb_ControlTransfer(this->usbh, SetupPacket, (PUCHAR)data, wLength, &cbSent, 0) == FALSE) {
		return -1;
	}

	return cbSent;
}

int Panda::bulk_write(UCHAR endpoint, const void * buff, ULONG length, PULONG transferred, ULONG timeout) {
	if (this->usbh == INVALID_HANDLE_VALUE || !buff || !length || !transferred) return FALSE;

	if (WinUsb_WritePipe(this->usbh, endpoint, (PUCHAR)buff, length, transferred, NULL) == FALSE) {
		_tprintf(_T("    Got error during bulk xfer: %d. Msg: '%s'\n"),
			GetLastError(), GetLastErrorAsString().c_str());
		return FALSE;
	}
	return TRUE;
}

int Panda::bulk_read(UCHAR endpoint, void * buff, ULONG buff_size, PULONG transferred, ULONG timeout) {
	if (this->usbh == INVALID_HANDLE_VALUE || !buff || !buff_size || !transferred) return FALSE;

	if (WinUsb_ReadPipe(this->usbh, endpoint, (PUCHAR)buff, buff_size, transferred, NULL) == FALSE) {
		_tprintf(_T("    Got error during bulk xfer: %d. Msg: '%s'\n"),
			GetLastError(), GetLastErrorAsString().c_str());
		return FALSE;
	}
	return TRUE;
}

PANDA_HEALTH Panda::get_health()
{
	WINUSB_SETUP_PACKET SetupPacket;
	ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
	ULONG cbSent = 0;

	//Create the setup packet
	SetupPacket.RequestType = REQUEST_IN;
	SetupPacket.Request = 0xD2;
	SetupPacket.Value = 0;
	SetupPacket.Index = 0;
	SetupPacket.Length = sizeof(UCHAR);

	//uint8_t health[13];
	PANDA_HEALTH health;

	if (WinUsb_ControlTransfer(this->usbh, SetupPacket, (PUCHAR)&health, sizeof(health), &cbSent, 0) == FALSE) {
		_tprintf(_T("    Got unexpected error while reading panda health (2nd time) %d. Msg: '%s'\n"),
				GetLastError(), GetLastErrorAsString().c_str());
	}

	return health;
}

bool Panda::enter_bootloader() {
	return this->control_transfer(REQUEST_OUT, 0xd1, 0, 0, NULL, 0, 0) != -1;
}

std::string Panda::get_version() {
	char buff[0x40];
	ZeroMemory(&buff, sizeof(buff));

	int xferCount = this->control_transfer(REQUEST_IN, 0xd6, 0, 0, buff, 0x40, 0);
	if (xferCount == -1) return std::string();
	return std::string(buff);
}

//TODO: Do hash stuff for calculating the serial.
std::string Panda::get_serial() {
	char buff[0x20];
	ZeroMemory(&buff, sizeof(buff));

	int xferCount = this->control_transfer(REQUEST_IN, 0xD0, 0, 0, buff, 0x20, 0);
	if (xferCount == -1) return std::string();
	return std::string(buff);

	//dat = self._handle.controlRead(REQUEST_IN, 0xd0, 0, 0, 0x20);
	//hashsig, calc_hash = dat[0x1c:], hashlib.sha1(dat[0:0x1c]).digest()[0:4]
	//	assert(hashsig == calc_hash)
	//	return[dat[0:0x10], dat[0x10:0x10 + 10]]
}

//Secret appears to by raw bytes, not a string. TODO: Change returned type.
std::string Panda::get_secret() {
	char buff[0x10];
	ZeroMemory(&buff, sizeof(buff));

	int xferCount = this->control_transfer(REQUEST_IN, 0xd0, 1, 0, buff, 0x10, 0);
	if (xferCount == -1) return std::string();
	return std::string(buff);
}

bool Panda::set_usb_power(bool on) {
	return this->control_transfer(REQUEST_OUT, 0xe6, (int)on, 0, NULL, 0, 0) != -1;
}

bool Panda::set_esp_power(bool on) {
	return this->control_transfer(REQUEST_OUT, 0xd9, (int)on, 0, NULL, 0, 0) != -1;
}

bool Panda::esp_reset(uint16_t bootmode = 0) {
	return this->control_transfer(REQUEST_OUT, 0xda, bootmode, 0, NULL, 0, 0) != -1;
}

bool Panda::set_safety_mode(PANDA_SAFETY_MODE mode = SAFETY_NOOUTPUT) {
	return this->control_transfer(REQUEST_OUT, 0xdc, mode, 0, NULL, 0, 0) != -1;
}

bool Panda::set_can_forwarding(PANDA_CAN_PORT from_bus, PANDA_CAN_PORT to_bus) {
	if (from_bus == PANDA_CAN_UNK) return FALSE;
	return this->control_transfer(REQUEST_OUT, 0xdd, from_bus, to_bus, NULL, 0, 0) != -1;
}

bool Panda::set_gmlan(PANDA_GMLAN_HOST_PORT bus = PANDA_GMLAN_CAN3) {
	return this->control_transfer(REQUEST_OUT, 0xdb, 1, (bus == PANDA_GMLAN_CLEAR) ? 0 : bus, NULL, 0, 0) != -1;
}

bool Panda::set_can_loopback(bool enable) {
	return this->control_transfer(REQUEST_OUT, 0xe5, enable, 0, NULL, 0, 0) != -1;
}

//Can not use the full range of 16 bit speed.
bool Panda::set_can_speed_kbps(PANDA_CAN_PORT bus, uint16_t speed) {
	if (bus == PANDA_CAN_UNK) return FALSE;
	return this->control_transfer(REQUEST_OUT, 0xde, bus, speed * 10, NULL, 0, 0) != -1;
}

//Can not use full 32 bit range of rate
bool Panda::set_uart_baud(PANDA_SERIAL_PORT uart, uint32_t rate) {
	return this->control_transfer(REQUEST_OUT, 0xe4, uart, rate / 300, NULL, 0, 0) != -1;
}

bool Panda::set_uart_parity(PANDA_SERIAL_PORT uart, PANDA_SERIAL_PORT_PARITY parity) {
	return this->control_transfer(REQUEST_OUT, 0xe2, uart, parity, NULL, 0, 0) != -1;
}

bool Panda::can_send_many(const std::vector<PANDA_CAN_MSG>& can_msgs) {
	std::vector<PANDA_CAN_MSG_INTERNAL> formatted_msgs;
	formatted_msgs.reserve(can_msgs.size());

	for (auto msg : can_msgs) {
		if (msg.bus == PANDA_CAN_UNK) continue;
		if (msg.len > 8) continue;
		PANDA_CAN_MSG_INTERNAL tmpmsg = {};
		tmpmsg.rir = (msg.addr_29b) ?
			((msg.addr << 3) | CAN_TRANSMIT | CAN_EXTENDED) :
			(((msg.addr & 0x7FF) << 21) | CAN_TRANSMIT);
		tmpmsg.f2 = msg.len | (msg.bus << 4);
		memcpy(tmpmsg.dat, msg.dat, msg.len);
		formatted_msgs.push_back(tmpmsg);
	}

	if (formatted_msgs.size() == 0) return FALSE;

	unsigned int retcount;
	//uint8_t buff[0x10];

	//std::string dat("\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb\xc\xd\xe\xf", 0x10);
	//bool res = this->bulk_transfer(2, dat.c_str(), 0x10, (PULONG)&retcount, 0);
	//bool res2 = this->bulk_transfer(1, dat.c_str(), 0x10, (PULONG)&retcount, 0);

	return this->bulk_write(3, formatted_msgs.data(),
		sizeof(PANDA_CAN_MSG_INTERNAL)*formatted_msgs.size(), (PULONG)&retcount, 0);
}

bool Panda::can_send(uint32_t addr, bool addr_29b, const uint8_t dat[8], uint8_t len, PANDA_CAN_PORT bus) {
	if (bus == PANDA_CAN_UNK) return FALSE;
	PANDA_CAN_MSG msg;
	msg.addr_29b = addr_29b;
	msg.addr = addr;
	msg.len = min(len, 8);
	memcpy(msg.dat, dat, msg.len);
	msg.bus = bus;
	return this->can_send_many(std::vector<PANDA_CAN_MSG>{msg});
}

std::vector<PANDA_CAN_MSG> Panda::can_recv() {
	std::vector<PANDA_CAN_MSG> msg_recv;
	int retcount;
	char buff[sizeof(PANDA_CAN_MSG_INTERNAL) * 4];// 256];

	if (this->bulk_read(0x81, buff, sizeof(buff), (PULONG)&retcount, 0) == FALSE)
		return msg_recv;

	for (int i = 0; i < retcount; i+=sizeof(PANDA_CAN_MSG_INTERNAL)) {
		PANDA_CAN_MSG_INTERNAL *in_msg_raw = (PANDA_CAN_MSG_INTERNAL *)(buff + i);
		PANDA_CAN_MSG in_msg;

		in_msg.addr_29b = (bool)(in_msg_raw->rir & CAN_EXTENDED);
		in_msg.addr = (in_msg.addr_29b) ? (in_msg_raw->rir >> 3) : (in_msg_raw->rir >> 21);
		in_msg.recv_time = in_msg_raw->f2 >> 16;
		in_msg.len = in_msg_raw->f2 & 0xF;
		memcpy(in_msg.dat, in_msg_raw->dat, 8);

		in_msg.is_receipt = ((in_msg_raw->f2 >> 4) & 0x80) == 0x80;
		switch ((in_msg_raw->f2 >> 4) & 0x7F) {
		case PANDA_CAN1:
			in_msg.bus = PANDA_CAN1;
			break;
		case PANDA_CAN2:
			in_msg.bus = PANDA_CAN2;
			break;
		case PANDA_CAN3:
			in_msg.bus = PANDA_CAN3;
			break;
		default:
			in_msg.bus = PANDA_CAN_UNK;
		}
		msg_recv.push_back(in_msg);
	}
	return msg_recv;
}

bool Panda::can_clear(PANDA_CAN_PORT_CLEAR bus) {
	/*Clears all messages from the specified internal CAN ringbuffer as though it were drained.
	bus(int) : can bus number to clear a tx queue, or 0xFFFF to clear the global can rx queue.*/
	return this->control_transfer(REQUEST_OUT, 0xf1, bus, 0, NULL, 0, 0) != -1;
}

std::string Panda::serial_read(PANDA_SERIAL_PORT port_number) {
	std::string result;
	char buff[0x40];
	while (TRUE) {
		int retlen = this->control_transfer(REQUEST_IN, 0xe0, port_number, 0, &buff, 0x40, 0);
		if (retlen <= 0)
			break;
		result += std::string(buff, retlen);
		if (retlen < 0x40) break;
	}
	return result;
}

int Panda::serial_write(PANDA_SERIAL_PORT port_number, const void* buff, uint16_t len) {
	std::string dat;
	dat += port_number;
	dat += std::string((char*)buff, len);
	int retcount;
	if (this->bulk_write(2, dat.c_str(), len+1, (PULONG)&retcount, 0) == FALSE) return -1;
	return retcount;
}

bool Panda::serial_clear(PANDA_SERIAL_PORT port_number) {
	return this->control_transfer(REQUEST_OUT, 0xf2, port_number, 0, NULL, 0, 0) != -1;
}
