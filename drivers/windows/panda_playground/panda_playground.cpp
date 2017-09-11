// panda_playground.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "panda\panda.h"

/*#if defined(UNICODE)
	#define _tcout std::wcout
	#define tstring std::wstring
#else
	#define _tcout std::cout
	#define tstring std::string
#endif*/

using namespace panda;

typedef struct {
	Panda *p;
	HANDLE kill_event;
} THREAD_DATA;

DWORD WINAPI DebugPrinter(LPVOID lpParam)
{
	THREAD_DATA* td = (THREAD_DATA*)lpParam;
	while (td->p) {
		std::string debug;
		std::string tmp = td->p->serial_read(SERIAL_DEBUG);
		while (tmp.size() > 0) {
			debug += tmp;
			tmp = {};
		}
		if (debug.size() > 0) {
			printf("***********************DEBUG >\n%s\n******************************\n", debug.c_str());
		}
	}
	return 0;
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	THREAD_DATA* td = (THREAD_DATA*)lpParam;
	while (TRUE) {
	//for(int i = 0; i < 10; i++) {
		std::vector<PANDA_CAN_MSG> msg_recv;
		printf("Thread looping.\n");
		auto err = td->p->can_recv_async(td->kill_event, msg_recv);
		printf("Length of returned messages: %d. Err code: %d\n", msg_recv.size(), err);
		for (auto msg : msg_recv) {
			printf("    Bus: %d%s; Addr: 0x%X (%s); Len: %d;",
				msg.bus, msg.is_receipt ? "r" : "", msg.addr, msg.addr_29b ? "29b" : "11b", msg.len);
			for (int i = 0; i < 8; i++) //msg.len; i++)
				printf("%02X ", msg.dat[i]);
			//printf("%c", msg.dat[i]);
			printf("\n");
		}
		if (err == FALSE) {
			break;
		}
	}
	printf("Thread end\n");
	return 0;
}

int _tmain(int Argc, _TCHAR *Argv) {
	UNREFERENCED_PARAMETER(Argc);
	UNREFERENCED_PARAMETER(Argv);
	{
		auto pandas_available = Panda::listAvailablePandas();
		printf("\nListing off discovered Pandas:\n");
		for (auto sn : pandas_available) {
			printf("    Panda '%s'\n", sn.c_str());
		}

		printf("\n");

		/*auto pandas_available = Panda::listAvailablePandas();
		for (auto sn : pandas_available) {
			_tprintf(_T("Panda '%s'\n"), sn.c_str());
		}*/

		auto p0 = Panda::openPanda("");// "02000c000f51363038363036");//"0e800a000f51363038363036");
		if (!p0) {
			printf("Panda could not be opened\n");
			system("pause");
			return 1;
		}

		HANDLE killevent = CreateEvent(NULL, TRUE, FALSE, NULL);

		THREAD_DATA td;
		td.kill_event = killevent;
		td.p = p0.get();
		DWORD threadid;
		HANDLE thread;
		DWORD res;
		/*if (GetExitCodeThread(thread, &res) == FALSE) {
			printf("FAILED TO GET THREAD RES. Err: %d\n", GetLastError());
		}*/

		//HANDLE dbgthread = CreateThread(NULL, 0, DebugPrinter, (LPVOID)&td, 0, &threadid);

		/*PANDA_HEALTH health = p0->get_health();
		_tprintf(_T("    Current: %u; Voltage: %u\n"), health.current, health.voltage);
		printf("    Read SN: %s\n", p0->get_serial().c_str());
		printf("    Read Secret: %s\n", p0->get_secret().c_str());
		printf("    Read Version: %s\n", p0->get_version().c_str());*/

		//printf("Clearnig Can\n"); Sleep(1);
		//p0->can_clear(PANDA_CAN_RX);
		//printf("Setting safety mode\n"); Sleep(1);
		p0->set_safety_mode(SAFETY_ALLOUTPUT);

		/*for (auto lin_port : { SERIAL_LIN1, SERIAL_LIN2 }) {
			printf("Doing a new LIN port\n");
			for (int i = 0; i < 10; i++) {
				p0->serial_clear(lin_port);
				uint8_t len = (rand() % LIN_MSG_MAX_LEN) + 1;
				std::string lindata;
				lindata.reserve(len);

				for (size_t j = 0; j < len; j++)
					lindata += (const char)(rand() % 128);

				for (int i = 0; i < len; i++) printf("%02X ", lindata[i]);
				printf("\n");

				p0->serial_write(lin_port, lindata.c_str(), len);
				Sleep(10);

				auto retdata = p0->serial_read(lin_port);
				for (int i = 0; i < retdata.size(); i++) printf("%02X ", retdata[i]);
				printf("\n\n");

			}
		}

		printf("\n\nSERIAL READ:\n");
		printf("%s\n\n", p0->serial_read(SERIAL_DEBUG).c_str());*/

		printf("setting speed\n"); Sleep(1);
		p0->set_can_speed_kbps(panda::PANDA_CAN1, 500);
		//printf("Disabling loopback\n"); Sleep(1);
		//p0->set_can_loopback(FALSE);
		//p0->set_can_loopback(TRUE);
		p0->set_alt_setting(1);
		//printf("Trying to unfuck the system\n"); Sleep(1);
		//p0->reset_can_interrupt_pipe();


		thread = CreateThread(NULL, 0, MyThreadFunction, (LPVOID)&td, 0, &threadid);

		/*if (GetExitCodeThread(thread, &res) == FALSE) {
			printf("FAILED TO GET THREAD RES. Err: %d\n", GetLastError());
		}*/
		Sleep(200);

		//SetEvent(killevent);
		const uint8_t candata[8] = { '\x69', 'E', 'L', 'L', 'O', '1', '2', '3' };
		/*if (p0->can_send(0x7E8, FALSE, candata, 3, PANDA_CAN1) == FALSE) {
			printf("Got err on zeroth send: %d\n", GetLastError());
		} else {
			printf("------------Send first\n");
		}*/
		//while (TRUE) {
		for (int i = 0; i < 1; i++) {
			if (p0->can_send(0x7E8, TRUE, candata, (i + 1) % 9, PANDA_CAN1) == FALSE) {
				printf("Got err on first send: %d\n", GetLastError());
			} else {
				printf("------------Send %d\n", i);
			}
			/*if (p0->can_send(0x7E8, FALSE, {}, 0, PANDA_CAN1) == FALSE) {
				printf("Got err on first send: %d\n", GetLastError());
			}
			else {
				printf("------------Send pad %d\n", i);
			}*/
			//p0->can_send(0x7E8, FALSE, candata, 7, PANDA_CAN1);

			Sleep(200);
		}
		Sleep(600);

		SetEvent(killevent);
		/*if (p0->can_send(0x7E8, FALSE, candata, 7, PANDA_CAN1) == FALSE) {
			printf("Got err on second send: %d\n", GetLastError());
		} else {
			printf("------------Send second\n");
		}

		Sleep(200);
		if (p0->can_send(0x7E8, FALSE, candata, 6, PANDA_CAN1) == FALSE) {
			printf("Got err on third send: %d\n", GetLastError());
		} else {
			printf("------------Send third\n");
		}*/

		//p0->set_alt_setting(0);

		//p0->can_send(0x7E8, FALSE, candata, 7, PANDA_CAN1);

		//Sleep(200);

		//SetEvent(killevent);
		//Sleep(500);
		//auto tmp = p0->can_recv();
 		//printf("Got data len %d\n", tmp.size());

		//system("pause");
		//p0->can_clear(PANDA_CAN_RX);
		/*const uint8_t candata[8] = { 'H', 'E', 'L', 'L', 'O', '1', '2', '3' };
			//Sleep(500);
		while (TRUE) {
			//p0->can_send(0x7E8, FALSE, candata, 7, PANDA_CAN1);
			auto can_msgs = p0->can_recv();
			if(can_msgs.size()) printf("Got %d can message\n", can_msgs.size());
			for (auto msg : can_msgs) {
				printf("    Bus: %d%s; Addr: 0x%X (%s); Len: %d;",
					msg.bus, msg.is_receipt ? "r" : "", msg.addr, msg.addr_29b ? "29b" : "11b", msg.len);
				for (int i = 0; i < 8; i++) //msg.len; i++)
					printf("%02X ", msg.dat[i]);
				//printf("%c", msg.dat[i]);
				printf("\n");
			}
			Sleep(1000);
		}*/

		/*p0->set_uart_baud(SERIAL_LIN1, 10400);
		p0->serial_write(SERIAL_LIN1, "DERP", 4);
		Sleep(500);*/
		//printf("\n\nLIN READ:\n");

		//std::string debug;
		//std::string tmp = p0->serial_read(SERIAL_DEBUG);
		//while (tmp.size() > 0) {
		//	printf("%s", tmp.c_str());
		//	tmp = {};
		//}
		//printf("\n");

		printf("pause2\n");
		system("pause");
		td.p = 0;
		Sleep(100);
	}

	//printf("pause\n");
	system("pause");
	return 0;
}

