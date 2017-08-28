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

int _tmain(int Argc, _TCHAR *Argv) {
	UNREFERENCED_PARAMETER(Argc);
	UNREFERENCED_PARAMETER(Argv);

	auto pandas_available = Panda::listAvailablePandas();
	for (auto sn : pandas_available) {
		_tprintf(_T("Panda '%s'\n"), sn.c_str());
	}

	printf("");

	{
		/*auto pandas_available = Panda::listAvailablePandas();
		for (auto sn : pandas_available) {
			_tprintf(_T("Panda '%s'\n"), sn.c_str());
		}*/

		auto p0 = Panda::openPanda(_T(""));// _T("0e800a000f51363038363036"));
		if (p0) {
			/*PANDA_HEALTH health = p0->get_health();
			_tprintf(_T("    Current: %u; Voltage: %u\n"), health.current, health.voltage);
			printf("    Read SN: %s\n", p0->get_serial().c_str());
			printf("    Read Secret: %s\n", p0->get_secret().c_str());
			printf("    Read Version: %s\n", p0->get_version().c_str());*/

			p0->set_safety_mode(SAFETY_ALLOUTPUT);

			for (auto lin_port : { SERIAL_LIN1, SERIAL_LIN2 }) {
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
			printf("%s\n\n", p0->serial_read(SERIAL_DEBUG).c_str());

			/*p0->set_safety_mode(SAFETY_ALLOUTPUT);
			p0->set_can_loopback(TRUE);
			p0->can_clear(PANDA_CAN_RX);
			const uint8_t candata[8] = { 'H', 'E', 'L', 'L', 'O', '1', '2', '3' };
			p0->can_send(0xAA, FALSE, candata, 7, PANDA_CAN1);
			Sleep(500);
 			auto can_msgs = p0->can_recv();
			printf("Got %d can message\n", can_msgs.size());
			for (auto msg : can_msgs) {
				printf("    Bus: %d%s; Addr: 0x%X (%s); Len: %d;",
					msg.bus, msg.is_receipt ? "r" : "", msg.addr, msg.addr_29b ? "29b" : "11b", msg.len);
				for (int i = 0; i < 8; i++) //msg.len; i++)
					printf("%02X ", msg.dat[i]);
					//printf("%c", msg.dat[i]);
				printf("\n");
			}*/

			/*p0->set_uart_baud(SERIAL_LIN1, 10400);
			p0->serial_write(SERIAL_LIN1, "DERP", 4);
			Sleep(500);
			printf("\n\nLIN READ: %s\n", p0->serial_read(SERIAL_LIN1).c_str());*/
		}
	}

	system("pause");
    return 0;
}

