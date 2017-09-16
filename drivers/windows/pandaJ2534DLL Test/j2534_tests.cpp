#include "stdafx.h"
#include "Loader4.h"
#include "pandaJ2534DLL/J2534_v0404.h"
#include "panda/panda.h"
#include "Timer.h"

#include <iomanip>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace pandaJ2534DLLTest
{
	void write_ioctl(unsigned int chanid, unsigned int param, unsigned int val, const __LineInfo* pLineInfo = NULL) {
		SCONFIG config = { param, val };
		SCONFIG_LIST inconfig = { 1, &config };

		Assert::AreEqual<long>(STATUS_NOERROR, PassThruIoctl(chanid, SET_CONFIG, &inconfig, NULL), _T("Failed to set IOCTL."), pLineInfo);
	}

	std::vector<panda::PANDA_CAN_MSG> panda_recv_loop(std::unique_ptr<panda::Panda>& p, unsigned int num_expected, unsigned long timeout_ms = 500) {
		std::vector<panda::PANDA_CAN_MSG> ret_messages;
		Timer t = Timer();

		while (t.getTimePassed() < timeout_ms) {
			Sleep(100);
			std::vector<panda::PANDA_CAN_MSG>msg_recv = p->can_recv();
			if (msg_recv.size() > 0) {
				ret_messages.insert(std::end(ret_messages), std::begin(msg_recv), std::end(msg_recv));
			}
			if (ret_messages.size() >= num_expected) break;
		}

		std::ostringstream stringStream;
		stringStream << "j2534_recv_loop Broke at " << t.getTimePassed() << " ms size is " << ret_messages.size();
		Logger::WriteMessage(stringStream.str().c_str());

		Assert::AreEqual<unsigned long>(num_expected, ret_messages.size(), _T("Received wrong number of messages."));
		return ret_messages;
	}

	void check_panda_can_msg(panda::PANDA_CAN_MSG& msgin, uint8_t bus, unsigned long addr, bool addr_29b,
		bool is_receipt, std::string dat, const __LineInfo* pLineInfo = NULL) {
		Assert::AreEqual<uint8_t>(bus, msgin.bus, _T("Wrong msg bus"), pLineInfo);
		Assert::AreEqual<unsigned long>(addr, msgin.addr, _T("Wrong msg addr"), pLineInfo);
		Assert::AreEqual<bool>(addr_29b, msgin.addr_29b, _T("Wrong msg 28b flag"), pLineInfo);
		Assert::AreEqual<bool>(is_receipt, msgin.is_receipt, _T("Wrong msg receipt flag"), pLineInfo);
		Assert::AreEqual<size_t>(dat.size(), msgin.len, _T("Wrong msg len"), pLineInfo);
		Assert::AreEqual<std::string>(dat, std::string((char*)msgin.dat, msgin.len), _T("Wrong msg payload"), pLineInfo);
	}

	void J2534_send_msg_checked(unsigned long chanid, unsigned long ProtocolID, unsigned long RxStatus, unsigned long TxFlags,
		unsigned long Timestamp, unsigned long DataSize, unsigned long ExtraDataIndex, const char* Data, const __LineInfo* pLineInfo = NULL) {

		PASSTHRU_MSG msg = { ProtocolID, RxStatus, TxFlags, Timestamp, DataSize, ExtraDataIndex };
		memcpy_s(msg.Data, 4128, Data, DataSize);
		unsigned long msgcount = 1;
		Assert::AreEqual<long>(STATUS_NOERROR, PassThruWriteMsgs(chanid, &msg, &msgcount, 0), _T("Failed to write message."), pLineInfo);
		Assert::AreEqual<unsigned long>(1, msgcount, _T("Wrong message count after tx."), LINE_INFO());
	}

	long J2534_send_msg(unsigned long chanid, unsigned long ProtocolID, unsigned long RxStatus, unsigned long TxFlags,
		unsigned long Timestamp, unsigned long DataSize, unsigned long ExtraDataIndex, const char* Data) {

		PASSTHRU_MSG msg = { ProtocolID, RxStatus, TxFlags, Timestamp, DataSize, ExtraDataIndex };
		memcpy_s(msg.Data, 4128, Data, DataSize);
		unsigned long msgcount = 1;
		return PassThruWriteMsgs(chanid, &msg, &msgcount, 0);
	}

	std::vector<PASSTHRU_MSG> j2534_recv_loop(unsigned int chanid, unsigned int num_expected, unsigned long timeout_ms = 500) {
		std::vector<PASSTHRU_MSG> ret_messages;
		PASSTHRU_MSG recvbuff[4] = {};
		Timer t = Timer();

		while (t.getTimePassed() < timeout_ms) {
			unsigned long msgcount = 4;
			unsigned int res = PassThruReadMsgs(chanid, recvbuff, &msgcount, 0);
			if (res == ERR_BUFFER_EMPTY) continue;
			Assert::IsFalse(msgcount > 4, _T("PassThruReadMsgs returned more data than the buffer could hold."));
			Assert::AreEqual<long>(STATUS_NOERROR, res, _T("Failed to read message."));
			if (msgcount > 0) {
				for (unsigned int i = 0; i < msgcount; i++) {
					ret_messages.push_back(recvbuff[i]);
				}
			}
			if (ret_messages.size() >= num_expected) break;
		}

		std::ostringstream stringStream;
		stringStream << "j2534_recv_loop Broke at " << t.getTimePassed() << " ms size is " << ret_messages.size();
		Logger::WriteMessage(stringStream.str().c_str());

		Assert::AreEqual<unsigned long>(num_expected, ret_messages.size(), _T("Received wrong number of messages."));
		return ret_messages;
	}

	void check_J2534_can_msg(PASSTHRU_MSG& msgin, unsigned long ProtocolID, unsigned long RxStatus, unsigned long TxFlags,
		unsigned long DataSize, unsigned long ExtraDataIndex, const char* Data, const __LineInfo* pLineInfo = NULL) {
		Assert::AreEqual<size_t>(DataSize, msgin.DataSize, _T("Wrong msg len"), pLineInfo);

		std::ostringstream logmsg;
		logmsg << "Expected Hex (";
		for (int i = 0; i < DataSize; i++) logmsg << std::hex << std::setw(2) << std::setfill('0') << int(Data[i] & 0xFF) << " ";
		logmsg << "); Actual Hex (";
		for (int i = 0; i < msgin.DataSize; i++) logmsg << std::hex << std::setw(2) << std::setfill('0') << int(((char*)msgin.Data)[i] & 0xFF) << " ";
		logmsg << ")";
		Logger::WriteMessage(logmsg.str().c_str());
		Assert::AreEqual<std::string>(std::string(Data, DataSize), std::string((char*)msgin.Data, msgin.DataSize), _T("Wrong msg payload"), pLineInfo);

		Assert::AreEqual<unsigned long>(ProtocolID, msgin.ProtocolID, _T("Wrong msg protocol"), pLineInfo);
		Assert::AreEqual<unsigned long>(RxStatus, msgin.RxStatus, _T("Wrong msg receipt rxstatus"), pLineInfo);
		Assert::AreEqual<unsigned long>(TxFlags, msgin.TxFlags, _T("Wrong msg receipt txflag"), pLineInfo);
		Assert::AreEqual<unsigned long>(ExtraDataIndex, msgin.ExtraDataIndex, _T("Wrong msg ExtraDataIndex"), pLineInfo);
	}

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

			J2534_send_msg_checked(chanid, CAN, 0, 0, 0, 6, 6, "\x0\x0\x3\xAB""HI", LINE_INFO());

			std::vector<panda::PANDA_CAN_MSG> msg_recv = panda_recv_loop(p, 1);
			check_panda_can_msg(msg_recv[0], 0, 0x3AB, FALSE, FALSE, "HI", LINE_INFO());
		}

		TEST_METHOD(J2534_CAN29b_Tx)
		{
			unsigned long chanid;
			Assert::AreEqual<long>(STATUS_NOERROR, open_dev(""), _T("Failed to open device."), LINE_INFO());
			Assert::AreEqual<long>(STATUS_NOERROR, PassThruConnect(devid, CAN, CAN_29BIT_ID, 500000, &chanid), _T("Failed to open channel."), LINE_INFO());

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."));
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 500);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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

			auto p = panda::Panda::openPanda("");
			Assert::IsTrue(p != nullptr, _T("Could not open 2nd device to test communication."), LINE_INFO());
			p->set_can_speed_kbps(panda::PANDA_CAN1, 250);
			p->set_safety_mode(panda::SAFETY_ALLOUTPUT);
			p->can_clear(panda::PANDA_CAN_RX);

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