#include "stdafx.h"
#include "CppUnitTest.h"
#include "Loader4.h"
#include "pandaJ2534DLL/J2534_v0404.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace pandaJ2534DLLTest
{
	TEST_CLASS(J2534DLLInitialization)
	{
	public:

		TEST_METHOD(J2534TestFunctionsExist)
		{
			long err = LoadJ2534Dll("PandaJ2534DLL.dll");
			Assert::IsTrue(err == 0, _T("Library failed to load properly. Check the export names and library location."));
		}

	};

	TEST_CLASS(J2534DeviceInitialization)
	{
	public:

		TEST_METHOD_INITIALIZE(init) {
			LoadJ2534Dll("PandaJ2534DLL.dll");
		}

		TEST_METHOD_CLEANUP(deinit) {
			if (didopen) {
				PassThruClose(devid);
				didopen = FALSE;
			}
			UnloadJ2534Dll();
		}

		TEST_METHOD(J2534TestOpenDevice)
		{
			long res = PassThruOpen("", &devid);
			Assert::AreEqual<long>(res, STATUS_NOERROR, _T("Failed to open device."), LINE_INFO());
			didopen = TRUE;
		}

		bool didopen = FALSE;
		unsigned long devid;

	};
}