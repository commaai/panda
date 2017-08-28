// pandaJ2534DLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "pandaJ2534DLL.h"
#include "J2534_v0404.h"

int J25334LastError = 0;

/*// This is an example of an exported variable
PANDAJ2534DLL_API int npandaJ2534DLL=0;

// This is an example of an exported function.
PANDAJ2534DLL_API int fnpandaJ2534DLL(void)
{
    return 42;
}

// This is the constructor of a class that has been exported.
// see pandaJ2534DLL.h for the class definition
CpandaJ2534DLL::CpandaJ2534DLL()
{
    return;
}*/

// A quick way to avoid the name mangling that __stdcall liked to do
#define EXPORT comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)

PANDAJ2534DLL_API long PTAPI    PassThruOpen(void *pName, unsigned long *pDeviceID) {
	#pragma EXPORT
	*pDeviceID = 0xFE34;
	//J25334LastError = ERR_DEVICE_NOT_CONNECTED;
	//return ERR_DEVICE_NOT_CONNECTED;
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruClose(unsigned long DeviceID) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID, 
	                                            unsigned long Flags, unsigned long BaudRate, unsigned long *pChannelID) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruDisconnect(unsigned long ChannelID) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, 
	                                             unsigned long *pNumMsgs, unsigned long Timeout) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruWriteMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruStartPeriodicMsg(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, 
	                                                   PASSTHRU_MSG *pPatternMsg, PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruStopMsgFilter(unsigned long ChannelID, unsigned long FilterID) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long PinNumber, unsigned long Voltage) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruReadVersion(unsigned long DeviceID, char *pFirmwareVersion, char *pDllVersion, char *pApiVersion) {
	#pragma EXPORT
	strcpy_s(pFirmwareVersion, 6, "00.02");
	strcpy_s(pDllVersion, 6, "00.01");
	strcpy_s(pApiVersion, sizeof(J2534_APIVER_NOVEMBER_2004), J2534_APIVER_NOVEMBER_2004);
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruGetLastError(char *pErrorDescription) {
	#pragma EXPORT
	switch (J25334LastError) {
	case STATUS_NOERROR:
		strcpy_s(pErrorDescription, 256, "Function call successful.");
		break;
	case ERR_NOT_SUPPORTED:
		strcpy_s(pErrorDescription, 256, "Device cannot support requested functionality mandated in J2534. Device is not fully SAE J2534 compliant.");
		break;
	case ERR_INVALID_CHANNEL_ID:
		strcpy_s(pErrorDescription, 256, "Invalid ChannelID value.");
		break;
	case ERR_INVALID_PROTOCOL_ID:
		strcpy_s(pErrorDescription, 256, "Invalid or unsupported ProtocolID, or there is a resource conflict (i.e. trying to connect to multiple mutually exclusive protocols such as J1850PWM and J1850VPW, or CAN and SCI, etc.).");
		break;
	case ERR_NULL_PARAMETER:
		strcpy_s(pErrorDescription, 256, "NULL pointer supplied where a valid pointer is required.");
		break;
	case ERR_INVALID_IOCTL_VALUE:
		strcpy_s(pErrorDescription, 256, "Invalid value for Ioctl parameter.");
		break;
	case ERR_INVALID_FLAGS:
		strcpy_s(pErrorDescription, 256, "Invalid flag values.");
		break;
	case ERR_FAILED:
		strcpy_s(pErrorDescription, 256, "Undefined error, use PassThruGetLastError() for text description.");
		break;
	case ERR_DEVICE_NOT_CONNECTED:
		strcpy_s(pErrorDescription, 256, "Unable to communicate with device.");
		break;
	case ERR_TIMEOUT:
		strcpy_s(pErrorDescription, 256, "Read or write timeout:");
		// PassThruReadMsgs() - No message available to read or could not read the specified number of messages. The actual number of messages read is placed in <NumMsgs>.
		// PassThruWriteMsgs() - Device could not write the specified number of messages. The actual number of messages sent on the vehicle network is placed in <NumMsgs>.
		break;
	case ERR_INVALID_MSG:
		strcpy_s(pErrorDescription, 256, "Invalid message structure pointed to by pMsg.");
		break;
	case ERR_INVALID_TIME_INTERVAL:
		strcpy_s(pErrorDescription, 256, "Invalid TimeInterval value.");
		break;
	case ERR_EXCEEDED_LIMIT:
		strcpy_s(pErrorDescription, 256, "Exceeded maximum number of message IDs or allocated space.");
		break;
	case ERR_INVALID_MSG_ID:
		strcpy_s(pErrorDescription, 256, "Invalid MsgID value.");
		break;
	case ERR_DEVICE_IN_USE:
		strcpy_s(pErrorDescription, 256, "Device is currently open.");
		break;
	case ERR_INVALID_IOCTL_ID:
		strcpy_s(pErrorDescription, 256, "Invalid IoctlID value.");
		break;
	case ERR_BUFFER_EMPTY:
		strcpy_s(pErrorDescription, 256, "Protocol message buffer empty, no messages available to read.");
		break;
	case ERR_BUFFER_FULL:
		strcpy_s(pErrorDescription, 256, "Protocol message buffer full. All the messages specified may not have been transmitted.");
		break;
	case ERR_BUFFER_OVERFLOW:
		strcpy_s(pErrorDescription, 256, "Indicates a buffer overflow occurred and messages were lost.");
		break;
	case ERR_PIN_INVALID:
		strcpy_s(pErrorDescription, 256, "Invalid pin number, pin number already in use, or voltage already applied to a different pin.");
		break;
	case ERR_CHANNEL_IN_USE:
		strcpy_s(pErrorDescription, 256, "Channel number is currently connected.");
		break;
	case ERR_MSG_PROTOCOL_ID:
		strcpy_s(pErrorDescription, 256, "Protocol type in the message does not match the protocol associated with the Channel ID");
		break;
	case ERR_INVALID_FILTER_ID:
		strcpy_s(pErrorDescription, 256, "Invalid Filter ID value.");
		break;
	case ERR_NO_FLOW_CONTROL:
		strcpy_s(pErrorDescription, 256, "No flow control filter set or matched (for ProtocolID ISO15765 only).");
		break;
	case ERR_NOT_UNIQUE:
		strcpy_s(pErrorDescription, 256, "A CAN ID in pPatternMsg or pFlowControlMsg matches either ID in an existing FLOW_CONTROL_FILTER");
		break;
	case ERR_INVALID_BAUDRATE:
		strcpy_s(pErrorDescription, 256, "The desired baud rate cannot be achieved within the tolerance specified in SAE J2534-1 Section 6.5");
		break;
	case ERR_INVALID_DEVICE_ID:
		strcpy_s(pErrorDescription, 256, "Device ID invalid.");
		break;
	}
	return STATUS_NOERROR;
}
PANDAJ2534DLL_API long PTAPI	PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID, 
	                                          void *pInput, void *pOutput) {
	#pragma EXPORT
	return STATUS_NOERROR;
}
