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

		TEST_METHOD(J2534_CAN11b_Tx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 6, 6, "\x0\x0\x3\xAB""HI", LINE_INFO());

			std::vector<panda::PANDA_CAN_MSG> msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "HI", LINE_INFO());

			j2534_recv_loop(chanid, 0, 50); // Check no message is returned (since loopback is off)
		}

		TEST_METHOD(J2534_CAN29b_Tx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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
			write_ioctl(chanid, LOOPBACK, FALSE, LINE_INFO()); // DISABLE J2534 ECHO/LOOPBACK

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

	TEST_CLASS(J2534DeviceISO15765)
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

		///////////////////// Tests checking things don't send/receive /////////////////////

		//Check tx PASSES and rx FAIL WITHOUT a filter. 29 bit. NO Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_PassTxFailRx_29b_NoFilter_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//TX: works because all single frame writes should work (with or without a flow contorl filter)
			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x07""TX_TEST", LINE_INFO());

			//RX: Reads require a flow control filter, and should fail without one.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x06\x41\x00\xff\xff\xff\xfe", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);
		}

		//Check tx and rx FAIL WITHOUT a filter. 29 bit. NO Filter. NoPadding. STD address. First Frame.
		TEST_METHOD(J2534_ISO15765_FailTxRx_29b_NoFilter_NoPad_STD_FF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//TX
			Assert::AreEqual<long>(ERR_NO_FLOW_CONTROL, J2534_send_msg(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 12, 0, "\x18\xda\xef\xf1\xA1\xB2\xC3\xD4\xE5\xF6\x09\x1A"),
				_T("Should fail to tx without a filter."), LINE_INFO());
			j2534_recv_loop(chanid, 0);
			panda_recv_loop(p, 0);

			//RX; Send full response and check didn't receive flow control from J2534 device
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x14\x49\x02\x01""1D4", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""GP00R55", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""B123456", 8, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);//Check a full message is not accepted.
		}

		//Check tx PASSES and rx FAIL with a MISMATCHED filter. 29 bit. Mismatch Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_PassTxFailRx_29b_MismatchFilter_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//TX: works because all single frame writes should work (with or without a flow contorl filter)
			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 6, 0, "\x18\xda\xe0\xf1""\x11\x22", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xe0\xf1", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAE0F1, TRUE, FALSE, "\x02""\x11\x22", LINE_INFO());

			//RX. Send ISO15765 single frame to device. Address still doesn't match filter, so should not be received.
			checked_panda_send(p, 0x18DAF1E0, TRUE, "\x06\x41\x00\xff\xff\xff\xfe", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);
		}

		//Check tx and rx FAIL with a MISMATCHED filter. 29 bit. Mismatch Filter. NoPadding. STD address. First Frame.
		TEST_METHOD(J2534_ISO15765_FailTxRx_29b_MismatchFilter_NoPad_STD_FF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//TX
			Assert::AreEqual<long>(ERR_NO_FLOW_CONTROL, J2534_send_msg(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 12, 0, "\x18\xda\xe0\xf1""USELESS STUFF"),
				_T("Should fail to tx without a filter."), LINE_INFO());
			j2534_recv_loop(chanid, 0);
			panda_recv_loop(p, 0);

			//RX; Send a full response and check didn't receive flow control from J2534 device
			checked_panda_send(p, 0x18DAF1E0, TRUE, "\x10\x14\x49\x02\x01""1D4", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1E0, TRUE, "\x21""GP00R55", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1E0, TRUE, "\x22""B123456", 8, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);//Check a full message is not accepted.
		}

		//Check tx FAILS with a MISMATCHED filter 29bit flag. 29 bit. Mismatch Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_FailTxRx_29b_MismatchFilterFlag29b_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x0\x0\x1\xab", "\x0\x0\x1\xcd", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//TX
			Assert::AreEqual<long>(ERR_INVALID_MSG, J2534_send_msg(chanid, ISO15765, 0, 0, 0, 6, 0, "\x0/x0/x1/xcd\x01\x00"),
				_T("mismatched address should fail to tx."), LINE_INFO());
			j2534_recv_loop(chanid, 0);
			panda_recv_loop(p, 0);

			//RX. Send ISO15765 single frame to device. Address still doesn't match filter, so should not be received.
			checked_panda_send(p, 0x1ab, FALSE, "\x06\x41\x00\xff\xff\xff\xfe", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);
		}

		///////////////////// Tests checking things actually send/receive. Standard Addressing /////////////////////

		//Check rx passes with filter. 29 bit. Good Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_SuccessRx_29b_Filter_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x07""ABCD123", 8, 0, LINE_INFO());

			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID, 0, 11, 11, "\x18\xda\xf1\xef""ABCD123", LINE_INFO());
		}

		//Check tx passes with filter. 29 bit. Good Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x07""TX_TEST", LINE_INFO());
		}

		//Check tx passes with filter. 29 bit. Good Filter. NoPadding. STD address. Single Frame. Loopback.
		TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_STD_SF_LOOPBACK)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, TRUE, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 2);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], ISO15765, CAN_29BIT_ID | TX_MSG_TYPE, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x07""TX_TEST", LINE_INFO());
		}

		//Check rx passes with filter. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_SuccessRx_29b_Filter_NoPad_STD_FFCF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ninete", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the rest of the message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""en byte", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""s here", 7, 0, LINE_INFO());

			//Check J2534 constructed the whole message
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID, 0, 4 + 0x13, 4 + 0x13, "\x18\xda\xf1\xef""nineteen bytes here", LINE_INFO());
		}

		//Check multi frame tx passes with filter. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_STD_FFCF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 14, 0, "\x18\xda\xef\xf1""\xAA\xBB\xCC\xDD\xEE\xFF\x11\x22\x33\x44", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 0); // No TxDone msg until after the final tx frame is sent

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x10\x0A""\xAA\xBB\xCC\xDD\xEE\xFF", LINE_INFO());

			panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x30\x0\x0", 3, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x21""\x11\x22\x33\x44", LINE_INFO());

			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());
		}

		//Check rx passes with filter. 11 bit. Good Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_SuccessRx_11b_Filter_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, 0, 4, "\xff\xff\xff\xff", "\x0\x0\x1\xab", "\x0\x0\x1\xcd", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			checked_panda_send(p, 0x1ab, FALSE, "\x07""ABCD123", 8, 0, LINE_INFO());

			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, 0, 0, 11, 11, "\x0\x0\x1\xab""ABCD123", LINE_INFO());
		}

		//Check tx passes with filter. 11 bit. Good Filter. NoPadding. STD address. Single Frame.
		TEST_METHOD(J2534_ISO15765_SuccessTx_11b_Filter_NoPad_STD_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, 0, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, 0, 4, "\xff\xff\xff\xff", "\x0\x0\x1\xab", "\x0\x0\x1\xcd", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, 0, 0, 11, 0, "\x0\x0\x1\xcd""TX_TEST", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, TX_INDICATION, 0, 4, 0, "\x0\x0\x1\xcd", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x1CD, FALSE, FALSE, "\x07""TX_TEST", LINE_INFO());
		}

		//Check tx passes with filter multiple times. 29 bit. Good Filter. NoPadding. STD address. Multiple Single Frames.
		TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_STD_MultipleSF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, TRUE, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());
			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 9, 0, "\x18\xda\xef\xf1""HELLO", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 4);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], ISO15765, CAN_29BIT_ID | TX_MSG_TYPE, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[2], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[3], ISO15765, CAN_29BIT_ID | TX_MSG_TYPE, 0, 9, 0, "\x18\xda\xef\xf1""HELLO", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 2);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x07""TX_TEST", LINE_INFO());
			check_panda_can_msg(panda_msg_recv[1], 0, 0x18DAEFF1, TRUE, FALSE, "\x05""HELLO", LINE_INFO());
		}

		//Check tx passes with filter multiple times. 29 bit. Good Filter. NoPadding. STD address. Multiple Single Frames.
		TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_STD_MultipleMFSF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, TRUE, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 4 + 23, 0, "\x18\xda\xef\xf1""Long data because I can", LINE_INFO());
			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 9, 0, "\x18\xda\xef\xf1""HELLO", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x10\x17""Long d", LINE_INFO());

			panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x30\x00\x00", 3, 4, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x21""ata bec", LINE_INFO());
			check_panda_can_msg(panda_msg_recv[1], 0, 0x18DAEFF1, TRUE, FALSE, "\x22""ause I ", LINE_INFO());
			check_panda_can_msg(panda_msg_recv[2], 0, 0x18DAEFF1, TRUE, FALSE, "\x23""can", LINE_INFO());
			check_panda_can_msg(panda_msg_recv[3], 0, 0x18DAEFF1, TRUE, FALSE, "\x05""HELLO", LINE_INFO());

			auto j2534_msg_recv = j2534_recv_loop(chanid, 4);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[1], ISO15765, CAN_29BIT_ID | TX_MSG_TYPE, 0, 4 + 23, 0, "\x18\xda\xef\xf1""Long data because I can", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[2], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());
			check_J2534_can_msg(j2534_msg_recv[3], ISO15765, CAN_29BIT_ID | TX_MSG_TYPE, 0, 4 + 5, 0, "\x18\xda\xef\xf1""HELLO", LINE_INFO());

			//panda_msg_recv = panda_recv_loop(p, 1);
		}


		///////////////////// Tests checking things break or recover during send/receive /////////////////////

		//Check rx FAILS when frame is dropped. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_FailRx_29b_Filter_NoPad_STD_FFCF_DropFrame)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ninete", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the rest of the message
			//Missing the 2nd frame "\x21""en byte"
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""s here", 7, 0, LINE_INFO());

			//Check J2534 DOES NOT construct the incomplete message
			j2534_recv_loop(chanid, 0);
		}

		//Check rx ignores frames that arrive out of order. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_PassRx_29b_Filter_NoPad_STD_FFCF_FrameNumSkip)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ABCDEF", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the rest of the message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""XXXXXX", 7, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""GHIJKLM", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x23""ZZZZZZ", 7, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""NOPQRS", 7, 0, LINE_INFO());

			//Check J2534 constructa the complete message from the correctly numbered frames.
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID, 0, 4 + 0x13, 4 + 0x13, "\x18\xda\xf1\xef""ABCDEFGHIJKLMNOPQRS", LINE_INFO());
		}

		//Check Single Frame rx RESETS ongoing multiframe transmission. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_PassRx_29b_Filter_NoPad_STD_SFRxResetsMFRx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ABCDEF", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the next part of the message multi message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""GHIJKLM", 8, 0, LINE_INFO());

			//ABORTING MESSAGE
			//Send a NEW single frame message and check the J2534 device gets it (but not the original message it was receiving.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x06""ABC123", 7, 0, LINE_INFO());
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID, 0, 10, 10, "\x18\xda\xf1\xef""ABC123", LINE_INFO());

			//Resume sending the old message, and check th eJ2534 device didn't get a message.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""NOPQRS", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);
		}

		//Check Single Frame tx RESETS ongoing multiframe rx transmission. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_PassRx_29b_Filter_NoPad_STD_SFTxResetsMFRx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ABCDEF", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the next part of the message multi message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""GHIJKLM", 8, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);

			//ABORTING MESSAGE
			//Send a NEW single frame message and check the J2534 device gets it (but not the original message it was receiving.
			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID, 0, 11, 0, "\x18\xda\xef\xf1""TX_TEST", LINE_INFO());
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION, 0, 4, 0, "\x18\xda\xef\xf1", LINE_INFO());

			panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x07""TX_TEST", LINE_INFO());
			///////////////////////////

			//Resume sending the old message, and check th eJ2534 device didn't get a message.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""NOPQRS", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);
		}

		//Check multiframe rx RESETS ongoing multiframe transmission. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_PassRx_29b_Filter_NoPad_STD_FFCF_MFRxResetsMFRx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ABCDEF", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the next part of the multi message A
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""GHIJKLM", 8, 0, LINE_INFO());

			//ABORTING MESSAGE A
			//Send a NEW multi frame message (B) and check the J2534 device gets it (but not the original message it was receiving.
			//Send first frame, then check we get a flow control frame
			panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ninete", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the rest of the message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""en byte", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""s here", 7, 0, LINE_INFO());

			//Check J2534 constructed the whole message
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID, 0, 4 + 0x13, 4 + 0x13, "\x18\xda\xf1\xef""nineteen bytes here", LINE_INFO());
			//////////////////////// End sending B

			//Resume sending the multi message A, and check th eJ2534 device didn't get a message.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""NOPQRS", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);
		}

		//TODO check rx is cleared by tx (multi)

		//Check rx fails gracefully if final CF of MF rx is too short. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_FailRxFinalCFTooShort_29b_Filter_NoPad_STD_FFCF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ninete", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x30\x00\x00", 3), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE, 0, 4, 0, "\x18\xda\xf1\xef", LINE_INFO());

			//Send the rest of the message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""en byte", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""s her", 6, 0, LINE_INFO()); //The transaction should reset here because more data could have been sent.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x23""e", 2, 0, LINE_INFO());

			//Check J2534 constructed the whole message
			j2534_msg_recv = j2534_recv_loop(chanid, 0);
		}

		//Check rx fails gracefully if first frame is too short. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_FailRxFFTooShort_29b_Filter_NoPad_STD_FFCF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID, 4, "\xff\xff\xff\xff", "\x18\xda\xf1\xef", "\x18\xda\xef\xf1", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame. The transaction should reset immediately because more data could have been sent in this frame.
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x10\x13""ninet", 7, 0, LINE_INFO());
			j2534_recv_loop(chanid, 0);

			//Send the rest of the message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x21""een byt", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x22""es here", 8, 0, LINE_INFO());

			//Check J2534 constructed the whole message
			j2534_recv_loop(chanid, 0);
		}

		///////////////////// Tests checking things actually send/receive. Ext 5 byte Addressing /////////////////////

		//Check rx passes with filter. 29 bit. Good Filter. NoPadding. EXT address. Single Frame.
		TEST_METHOD(J2534_ISO15765_SuccessRx_29b_Filter_NoPad_EXT_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 5, "\xff\xff\xff\xff\xff", "\x18\xda\xf1\xef\x13", "\x18\xda\xef\xf1\x13", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13""\x06""ABC123", 8, 0, LINE_INFO());

			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 0, 11, 11, "\x18\xda\xf1\xef\x13""ABC123", LINE_INFO());
		}

		//Check tx passes with filter. 29 bit. Good Filter. NoPadding. EXT address. Single Frame.
		TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_EXT_SF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 5, "\xff\xff\xff\xff\xff", "\x18\xda\xf1\xef\x13", "\x18\xda\xef\xf1\x13", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			J2534_send_msg_checked(chanid, ISO15765, 0, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 0, 11, 0, "\x18\xda\xef\xf1\x13""DERP!!", LINE_INFO());
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | TX_INDICATION | ISO15765_ADDR_TYPE, 0, 5, 0, "\x18\xda\xef\xf1\x13", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, "\x13""\x06""DERP!!", LINE_INFO());
		}

		//Check rx passes with filter. 29 bit. Good Filter. NoPadding. EXT address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_SuccessRx_29b_Filter_NoPad_EXT_FFCF)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 5, "\xff\xff\xff\xff\xff", "\x18\xda\xf1\xef\x13", "\x18\xda\xef\xf1\x13", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			Assert::IsTrue(p->can_send(0x18DAF1EF, TRUE, (const uint8_t*)"\x13""\x10\x13""ninet", 8, panda::PANDA_CAN1), _T("Panda send says it failed."), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE | ISO15765_ADDR_TYPE, 0, 5, 0, "\x18\xda\xf1\xef\x13", LINE_INFO());

			auto panda_msg_recv = panda_recv_loop(p, 2);
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAF1EF, TRUE, TRUE, std::string("\x13""\x10\x13""ninet", 8), LINE_INFO());
			check_panda_can_msg(panda_msg_recv[1], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x13""\x30\x00\x00", 4), LINE_INFO());

			//Send the rest of the message
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13""\x21""een by", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13""\x22""tes he", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13""\x23""re", 8, 0, LINE_INFO());

			//Check J2534 constructed the whole message
			j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 0, 5 + 0x13, 5 + 0x13, "\x18\xda\xf1\xef\x13""nineteen bytes here", LINE_INFO());
		}

		//Check tx passes with filter. 29 bit. Good Filter. NoPadding. EXT address. Multi Frame.
		/*TEST_METHOD(J2534_ISO15765_SuccessTx_29b_Filter_NoPad_EXT_FFCF)
		{ //TODO when TX works with flow control}*/

		///////////////////// Tests checking things break or recover during send/receive. Ext 5 byte Addressing /////////////////////

		//Check rx FAILS when frame is dropped. 29 bit. Good Filter. NoPadding. STD address. Multi Frame.
		TEST_METHOD(J2534_ISO15765_FailRx_29b_Filter_NoPad_EXT_FFCF_DropFrame)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, ISO15765, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());
			J2534_set_flowctrl_filter(chanid, CAN_29BIT_ID | ISO15765_ADDR_TYPE, 5, "\xff\xff\xff\xff\xff", "\x18\xda\xf1\xef\x13", "\x18\xda\xef\xf1\x13", LINE_INFO());
			write_ioctl(chanid, LOOPBACK, 0, LINE_INFO());
			auto p = getPanda(500);

			//Send first frame, then check we get a flow control frame
			auto panda_msg_recv = checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13\x10\x13""ninet", 8, 1, LINE_INFO());
			check_panda_can_msg(panda_msg_recv[0], 0, 0x18DAEFF1, TRUE, FALSE, std::string("\x13\x30\x00\x00", 4), LINE_INFO());

			//Check first frame is registered with J2534
			auto j2534_msg_recv = j2534_recv_loop(chanid, 1);
			check_J2534_can_msg(j2534_msg_recv[0], ISO15765, CAN_29BIT_ID | START_OF_MESSAGE | ISO15765_ADDR_TYPE, 0, 5, 0, "\x18\xda\xf1\xef\x13", LINE_INFO());

			//Send the rest of the message
			//Missing the 2nd frame "\x13""\x21""een by"
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13""\x22""tes he", 8, 0, LINE_INFO());
			checked_panda_send(p, 0x18DAF1EF, TRUE, "\x13""\x23""re", 8, 0, LINE_INFO());

			//Check J2534 DOES NOT construct the incomplete message
			j2534_recv_loop(chanid, 0);
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