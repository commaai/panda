#include "stdafx.h"
#include "Loader4.h"
#include "pandaJ2534DLL/J2534_v0404.h"
#include "panda/panda.h"
#include "Timer.h"
#include "ECUsim DLL\ECUsim.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace pandaJ2534DLLTest
{
	TEST_CLASS(J2534DLLInitialization)
	{
	public:

		TEST_CLASS_CLEANUP(deinit) {
			UnloadJ2534Dll();
		}

		TEST_METHOD(J2534DriverInit)
		{
			long err = LoadJ2534Dll("PandaJ2534.dll");
			Assert::IsTrue(err == 0, _T("Library failed to load properly. Check the export names and library location."));
		}

	};

	TEST_CLASS(J2534DeviceInitialization)
	{
	public:

		TEST_METHOD_INITIALIZE(init) {
			LoadJ2534Dll("PandaJ2534.dll");
		}

		TEST_METHOD_CLEANUP(deinit) {
			if (didopen) {
				PassThruClose(devid);
				didopen = FALSE;
			}
			UnloadJ2534Dll();
		}

		TEST_METHOD(J2534_OpenDevice__Empty)
		{
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
		}

		TEST_METHOD(J2534_OpenDevice__J2534_2)
		{
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev("J2534-2:"), _T("Failed to open device."), LINE_INFO());
		}

		TEST_METHOD(J2534_OpenDevice__SN)
		{
			auto pandas_available = panda::Panda::listAvailablePandas();
			Assert::IsTrue(pandas_available.size() > 0, _T("No pandas detected."));

			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(pandas_available[0].c_str()), _T("Failed to open device."), LINE_INFO());

			auto pandas_available_2 = panda::Panda::listAvailablePandas();
			for (auto panda_sn : pandas_available_2)
				Assert::AreNotEqual(panda_sn, pandas_available[0]);
		}

		TEST_METHOD(J2534_CloseDevice)
		{
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, close_dev(devid), _T("Failed to close device."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_DEVICE_ID, PassThruClose(devid), _T("The 2nd close should have failed with ERR_INVALID_DEVICE_ID."), LINE_INFO());
		}

		TEST_METHOD(J2534_ConnectDisconnect)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			Assert::AreEqual<long>(STATUS_NOERROR, PassThruDisconnect(chanid), _T("Failed to close channel."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_CHANNEL_ID, PassThruDisconnect(chanid), _T("The 2nd disconnect should have failed with ERR_INVALID_CHANNEL_ID."), LINE_INFO());
		}

		TEST_METHOD(J2534_ConnectInvalidProtocol)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_PROTOCOL_ID, PassThruConnect(devid, 999, 0, 500000, &chanid),
				_T("Did not report ERR_INVALID_PROTOCOL_ID."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_CHANNEL_ID, PassThruDisconnect(chanid), _T("The channel should not have been created."), LINE_INFO());
		}

		bool didopen = FALSE;
		unsigned long devid;

		unsigned long open_dev(const char* name, long assert_err = STATUS_NOERROR, TCHAR* failmsg = _T("Failed to open device.")) {
			unsigned int res = PassThruOpen((void*)name, &devid);
			if (res == STATUS_NOERROR) didopen = TRUE;
			return res;
		}

		unsigned long close_dev(unsigned long devid) {
			unsigned long res = PassThruClose(devid);
			if (res == STATUS_NOERROR) didopen = FALSE;
			return res;
		}

	};

	TEST_CLASS(J2534DeviceCAN)
	{
	public:

		TEST_METHOD_INITIALIZE(init) {
			LoadJ2534Dll("PandaJ2534.dll");
		}

		TEST_METHOD_CLEANUP(deinit) {
			if (didopen) {
				PassThruClose(devid);
				didopen = FALSE;
			}
			UnloadJ2534Dll();
		}

		TEST_METHOD(J2534_CAN_Tx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 6, 6, "\x0\x0\x3\xAB""HI", LINE_INFO());

			std::vector<panda::PANDA_CAN_MSG> msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "HI", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN29b_Tx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			auto p = getPanda(500);

			Assert::AreEqual<long>(ERR_INVALID_MSG, J2534_send_msg(chanid, CAN, 0, 0, 0, 6, 6, "\x0\x0\x3\xAB""HI"), _T("11b address should fail to tx."), LINE_INFO());
			J2534_send_msg_checked(chanid, CAN, 0, CAN_29BIT_ID, 0, 6, 6, "\x0\x0\x3\xAB""YO", LINE_INFO());

			std::vector<panda::PANDA_CAN_MSG> msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, TRUE, FALSE, "YO", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN11b29b_Tx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, CAN_ID_BOTH, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 6, 6, "\x0\x0\x3\xAB""HI", LINE_INFO());
			J2534_send_msg_checked(chanid, CAN, 0, CAN_29BIT_ID, 0, 6, 6, "\x0\x0\x3\xAB""YO", LINE_INFO());

			std::vector<panda::PANDA_CAN_MSG> msg_recv = panda_recv_loop(p, 2);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "HI", LINE_INFO());
			check_panda_can_msg(msg_recv[1], 0, 0x3AB, TRUE, FALSE, "YO", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_TxEcho)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 9, 9, "\x0\x0\x3\xAB""HIDOG", LINE_INFO());

			auto msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "HIDOG", LINE_INFO());

			auto j2534_msg_recv = j2534_recv_loop(chanid, 0);

			/////////////////////////////////
			write_ioctl(chanid, LOOPBACK, TRUE, LINE_INFO()); // ENABLE J2534 ECHO/LOOPBACK

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 7, 7, "\x0\x0\x3\xAB""SUP", LINE_INFO());

			msg_recv = panda_recv_loop(p, 1);

			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "SUP", LINE_INFO());

			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], CAN, TX_MSG_TYPE, 0, 3 + 4, 0, "\x0\x0\x3\xAB""SUP", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_RxAndPassAllFilters)
		{
			unsigned long chanid, filterid0;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			PASSTHRU_MSG filter = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, PASS_FILTER, &filter, &filter, 0, &filterid0), _T("Failed to create filter."), LINE_INFO());

			auto p = getPanda(500);

			p->can_send(0x1FA, FALSE, (const uint8_t*)"ABCDE", 5, panda::PANDA_CAN1);
			p->can_send(0x2AC, FALSE, (const uint8_t*)"HIJKL", 5, panda::PANDA_CAN1);

			auto j2534_msg_recv = j2534_recv_loop(chanid, 2);
			check_J2534_can_msg(j2534_msg_recv[0], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x1\xFA""ABCDE", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x2\xAC""HIJKL", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_RxAndLimitedPassFilter)
		{
			unsigned long chanid, filterid0;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			PASSTHRU_MSG filter_mask = { CAN, 0, 0, 0, 4, 4, "\xFF\xFF\xFF\xFF" };
			PASSTHRU_MSG filter = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x02\xAC" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, PASS_FILTER, &filter_mask, &filter, 0, &filterid0), _T("Failed to create filter."), LINE_INFO());

			auto p = getPanda(500);

			p->can_send(0x1FA, FALSE, (const uint8_t*)"ABCDE", 5, panda::PANDA_CAN1);
			p->can_send(0x2AC, FALSE, (const uint8_t*)"HIJKL", 5, panda::PANDA_CAN1);

			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x2\xAC""HIJKL", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_RxAndPassBlockFilter)
		{
			unsigned long chanid, filterid0, filterid1;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			PASSTHRU_MSG filter_mask0 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			PASSTHRU_MSG filter0 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, PASS_FILTER, &filter_mask0, &filter0, 0, &filterid0), _T("Failed to create filter."), LINE_INFO());

			PASSTHRU_MSG filter_mask1 = { CAN, 0, 0, 0, 4, 4, "\xFF\xFF\xFF\xFF" };
			PASSTHRU_MSG filter1 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x02\xAC" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, BLOCK_FILTER, &filter_mask1, &filter1, 0, &filterid1), _T("Failed to create filter."), LINE_INFO());

			auto p = getPanda(500);

			p->can_send(0x1FA, FALSE, (const uint8_t*)"ABCDE", 5, panda::PANDA_CAN1);
			p->can_send(0x2AC, FALSE, (const uint8_t*)"HIJKL", 5, panda::PANDA_CAN1);
			p->can_send(0x3FA, FALSE, (const uint8_t*)"MNOPQ", 5, panda::PANDA_CAN1);

			auto j2534_msg_recv = j2534_recv_loop(chanid, 2, 1000);
			check_J2534_can_msg(j2534_msg_recv[0], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x1\xFA""ABCDE", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x3\xFA""MNOPQ", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_RxAndFilterBlockPass)
		{
			//Check that the order of the pass and block filter do not matter
			unsigned long chanid, filterid0, filterid1;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			PASSTHRU_MSG filter_mask0 = { CAN, 0, 0, 0, 4, 4, "\xFF\xFF\xFF\xFF" };
			PASSTHRU_MSG filter0 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x02\xAC" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, BLOCK_FILTER, &filter_mask0, &filter0, 0, &filterid0), _T("Failed to create filter."), LINE_INFO());

			PASSTHRU_MSG filter_mask1 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			PASSTHRU_MSG filter1 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, PASS_FILTER, &filter_mask1, &filter1, 0, &filterid1), _T("Failed to create filter."), LINE_INFO());

			auto p = getPanda(500);

			p->can_send(0x1FA, FALSE, (const uint8_t*)"ABCDE", 5, panda::PANDA_CAN1);
			p->can_send(0x2AC, FALSE, (const uint8_t*)"HIJKL", 5, panda::PANDA_CAN1); // Should not pass filter
			p->can_send(0x3FA, FALSE, (const uint8_t*)"MNOPQ", 5, panda::PANDA_CAN1);

			auto j2534_msg_recv = j2534_recv_loop(chanid, 2, 2000);
			check_J2534_can_msg(j2534_msg_recv[0], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x1\xFA""ABCDE", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x3\xFA""MNOPQ", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_RxAndFilterRemoval)
		{
			//Check that the order of the pass and block filter do not matter
			unsigned long chanid, filterid0, filterid1;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			PASSTHRU_MSG filter_mask0 = { CAN, 0, 0, 0, 4, 4, "\xFF\xFF\xFF\xFF" };
			PASSTHRU_MSG filter0 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x02\xAC" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, BLOCK_FILTER, &filter_mask0, &filter0, 0, &filterid0), _T("Failed to create filter."), LINE_INFO());

			PASSTHRU_MSG filter_mask1 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			PASSTHRU_MSG filter1 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, PASS_FILTER, &filter_mask1, &filter1, 0, &filterid1), _T("Failed to create filter."), LINE_INFO());

			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStopMsgFilter(chanid, filterid0), _T("Failed to delete filter."), LINE_INFO());

			auto p = getPanda(500);

			p->can_send(0x1FA, FALSE, (const uint8_t*)"ABCDE", 5, panda::PANDA_CAN1);
			p->can_send(0x2AC, FALSE, (const uint8_t*)"HIJKL", 5, panda::PANDA_CAN1);
			p->can_send(0x3FA, FALSE, (const uint8_t*)"MNOPQ", 5, panda::PANDA_CAN1);

			auto j2534_msg_recv = j2534_recv_loop(chanid, 3, 1000);
			check_J2534_can_msg(j2534_msg_recv[0], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x1\xFA""ABCDE", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x2\xAC""HIJKL", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[2], CAN, 0, 0, 5 + 4, 0, "\x0\x0\x3\xFA""MNOPQ", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_RxWithTimeout)
		{
			//Check that the order of the pass and block filter do not matter
			unsigned long chanid, filterid0;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			PASSTHRU_MSG filter_mask0 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			PASSTHRU_MSG filter0 = { CAN, 0, 0, 0, 4, 4, "\x0\x0\x0\x0" };
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruStartMsgFilter(chanid, PASS_FILTER, &filter_mask0, &filter0, 0, &filterid0), _T("Failed to create filter."), LINE_INFO());

			auto p = getPanda(500);

			PASSTHRU_MSG recvbuff;
			unsigned long msgcount = 1;
			unsigned int res = PassThruReadMsgs(chanid, &recvbuff, &msgcount, 100); // Here is where we test the timeout
			Assert::AreEqual<long>(ERR_BUFFER_EMPTY, res, _T("No message should be found"), LINE_INFO());
			Assert::AreEqual<unsigned long>(0, msgcount, _T("Received wrong number of messages."));

			//TODO Test that the timings work right instead of just testing it doesn't crash.
		}

		TEST_METHOD(J2534_CAN_Baud)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 250000, &chanid), _T("Failed to open channel."), LINE_INFO());

			auto p = getPanda(250);

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 6, 6, "\x0\x0\x3\xAB""HI", LINE_INFO());

			std::vector<panda::PANDA_CAN_MSG> msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "HI", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN_BaudInvalid)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_BAUDRATE, PassThruConnect(devid, CAN, 0, 6000000, &chanid), _T("Baudrate should have been invalid."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_BAUDRATE, PassThruConnect(devid, CAN, 0, 200, &chanid), _T("Baudrate should have been invalid."), LINE_INFO());
			Assert::AreEqual<long>(ERR_INVALID_BAUDRATE, PassThruConnect(devid, CAN, 0, 250010, &chanid), _T("Baudrate should have been invalid."), LINE_INFO());
		}

		bool didopen = FALSE;
		unsigned long devid;

		unsigned long open_dev(const char* name, long assert_err = STATUS_NOERROR, TCHAR* failmsg = _T("Failed to open device.")) {
			unsigned int res = PassThruOpen((void*)name, &devid);
			if (res == STATUS_NOERROR) didopen = TRUE;
			return res;
		}

	};

}