// pandaJ2534DLL.cpp : Defines the exported functions for the DLL application.
// Protocol derived from the following sites (which shall be referred to as The Protocol Reference #).
// https://web.archive.org/web/20130805013326/https://tunertools.com/prodimages/DrewTech/Manuals/PassThru_API-1.pdf
// http://web.archive.org/web/20170910063536/http://www.tiecar.net/downloads/SAE_J2534_2002.pdf

#include "stdafx.h"
#include "J2534_v0404.h"
#include "panda/panda.h"
#include "J2534Connection.h"
#include "J2534Connection_CAN.h"
#include "PandaJ2534Device.h"

// A quick way to avoid the name mangling that __stdcall liked to do
#define EXPORT comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)

std::vector<std::unique_ptr<PandaJ2534Device>> pandas;

int J25334LastError = 0;

long ret_code(long code) {
	J25334LastError = code;
	return code;
}

#define EXTRACT_DID(CID) (CID & 0xFFFF)
#define EXTRACT_CID(CID) ((CID >> 16) & 0xFFFF)

long check_valid_DeviceID(unsigned long DeviceID) {
	uint16_t dev_id = EXTRACT_DID(DeviceID);
	if (pandas.size() <= dev_id || pandas[dev_id] == nullptr)
		return ret_code(ERR_INVALID_DEVICE_ID);
	return ret_code(STATUS_NOERROR);
}

long check_valid_ChannelID(unsigned long ChannelID) {
	uint16_t dev_id = EXTRACT_DID(ChannelID);;
	uint16_t con_id = EXTRACT_CID(ChannelID);

	if (pandas.size() <= dev_id || pandas[dev_id] == nullptr)
		return ret_code(ERR_INVALID_CHANNEL_ID);

	if (pandas[dev_id]->connections.size() <= con_id) return ret_code(ERR_INVALID_CHANNEL_ID);
	if (pandas[dev_id]->connections[con_id] == nullptr) return ret_code(ERR_DEVICE_NOT_CONNECTED);

	return ret_code(STATUS_NOERROR);
}

//Do not call without checking if the device/channel id exists first.
#define get_device(DeviceID) (pandas[EXTRACT_DID(DeviceID)])
#define get_channel(ChannelID) (get_device(ChannelID)->connections[EXTRACT_CID(ChannelID)])

PANDAJ2534DLL_API long PTAPI    PassThruOpen(void *pName, unsigned long *pDeviceID) {
	#pragma EXPORT
	if (pDeviceID == NULL) return ret_code(ERR_NULL_PARAMETER);
	std::string sn = (pName == NULL) ? "" : std::string((char*)pName);
	if (sn == "J2534-2:")
		sn = "";

	auto new_panda = PandaJ2534Device::openByName(sn);
	if (new_panda == nullptr) {
		if(sn == "" && pandas.size() == 1)
			return ret_code(ERR_DEVICE_IN_USE);
		for (auto& pn : pandas) {
			if (pn->panda->get_usb_sn() == sn)
				return ret_code(ERR_DEVICE_IN_USE);
		}
		return ret_code(ERR_DEVICE_NOT_CONNECTED);
	}

	int panda_index = -1;
	for (unsigned int i = 0; i < pandas.size(); i++)
		if (pandas[i] == nullptr) {
			panda_index = i;
			pandas[panda_index] = std::move(new_panda);
			break;
		}

	if (panda_index == -1) {
		if(pandas.size() == 0xFFFF) //device id will be 16 bit to fit channel next to it.
			return ret_code(ERR_FAILED); //Too many pandas. Off the endangered species list.
		pandas.push_back(std::move(new_panda));
		panda_index = pandas.size()-1;
	}

	*pDeviceID = panda_index;
	return ret_code(STATUS_NOERROR);
}
PANDAJ2534DLL_API long PTAPI	PassThruClose(unsigned long DeviceID) {
	#pragma EXPORT
	if (check_valid_DeviceID(DeviceID) != STATUS_NOERROR) return J25334LastError;
	get_device(DeviceID) = nullptr;
	return ret_code(STATUS_NOERROR);
}
PANDAJ2534DLL_API long PTAPI	PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID,
	                                            unsigned long Flags, unsigned long BaudRate, unsigned long *pChannelID) {
	#pragma EXPORT
	if (pChannelID == NULL) return ret_code(ERR_NULL_PARAMETER);
	if (check_valid_DeviceID(DeviceID) != STATUS_NOERROR) return J25334LastError;
	auto& panda = get_device(DeviceID);

	J2534Connection* conn = NULL;

	//TODO check if channel can be made
	switch (ProtocolID) {
	//SW seems to refer to Single Wire. https://www.nxp.com/files-static/training_pdf/20451_BUS_COMM_WBT.pdf
	//SW_ protocols may be touched on here: https://www.iso.org/obp/ui/#iso:std:iso:22900:-2:ed-1:v1:en
	case J1850VPW: //These protocols are outdated and will not be supported. HDS wants them to not fail to open.
	case J1850PWM:
	case J1850VPW_PS:
	case J1850PWM_PS:
	case ISO9141: //This protocol could be implemented if 5 BAUD init support is added to the panda.
	case ISO9141_PS:
		conn = new J2534Connection(panda->panda.get(), ProtocolID, Flags, BaudRate);
		break;
	case ISO14230: //Only supporting Fast init until panda adds support for 5 BAUD init.
	case ISO14230_PS:
		conn = new J2534Connection(panda->panda.get(), ProtocolID, Flags, BaudRate);
		break;
	case CAN:
	case CAN_PS:
	//case SW_CAN_PS:
		conn = new J2534Connection_CAN(panda->panda.get(), ProtocolID, Flags, BaudRate);
		break;
	//case ISO15765:
	//case ISO15765_PS:
	//case SW_ISO15765_PS:
	//case GM_UART_PS: //Suspect this is GMLAN
	//Looks like SCI based protocols may not be compatible with the panda:
	//http://mdhmotors.com/can-communications-vehicle-network-protocols/3/
	//case SCI_A_ENGINE:
	//case SCI_A_TRANS:
	//case SCI_B_ENGINE:
	//case SCI_B_TRANS:
	//case J2610_PS:*/
	default:
		return ret_code(ERR_INVALID_PROTOCOL_ID);
	}

	unsigned long channel_index;
	unsigned long err = panda->addChannel(conn, &channel_index);
	if (err == STATUS_NOERROR)
		*pChannelID = (channel_index << 16) | DeviceID;

	return ret_code(err);
}
PANDAJ2534DLL_API long PTAPI	PassThruDisconnect(unsigned long ChannelID) {
	#pragma EXPORT
	unsigned long res = check_valid_DeviceID(ChannelID);
	if (res == ERR_INVALID_DEVICE_ID) return ret_code(ERR_INVALID_CHANNEL_ID);
	if (res != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_device(ChannelID)->closeChannel(EXTRACT_CID(ChannelID)));
}
PANDAJ2534DLL_API long PTAPI	PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg,
	                                             unsigned long *pNumMsgs, unsigned long Timeout) {
	#pragma EXPORT
	if (pMsg == NULL || pNumMsgs == NULL) return ret_code(ERR_NULL_PARAMETER);
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_channel(ChannelID)->PassThruReadMsgs(pMsg, pNumMsgs, Timeout));
}
PANDAJ2534DLL_API long PTAPI	PassThruWriteMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout) {
	#pragma EXPORT
	if (pMsg == NULL || pNumMsgs == NULL) return ret_code(ERR_NULL_PARAMETER);
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_channel(ChannelID)->PassThruWriteMsgs(pMsg, pNumMsgs, Timeout));
}
PANDAJ2534DLL_API long PTAPI	PassThruStartPeriodicMsg(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval) {
	#pragma EXPORT
	if (pMsg == NULL || pMsgID == NULL) return ret_code(ERR_NULL_PARAMETER);
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_channel(ChannelID)->PassThruStartPeriodicMsg(pMsg, pMsgID, TimeInterval));
}
PANDAJ2534DLL_API long PTAPI	PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID) {
	#pragma EXPORT
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_channel(ChannelID)->PassThruStopPeriodicMsg(MsgID));
}
PANDAJ2534DLL_API long PTAPI	PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType, PASSTHRU_MSG *pMaskMsg,
	                                                   PASSTHRU_MSG *pPatternMsg, PASSTHRU_MSG *pFlowControlMsg, unsigned long *pFilterID) {
	#pragma EXPORT
	if (FilterType != PASS_FILTER && FilterType != BLOCK_FILTER && FilterType != FLOW_CONTROL_FILTER) return ret_code(ERR_NULL_PARAMETER);
	if (!pFilterID || (!pMaskMsg && !pPatternMsg && !pFlowControlMsg)) return ret_code(ERR_NULL_PARAMETER);
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_channel(ChannelID)->PassThruStartMsgFilter(FilterType, pMaskMsg, pPatternMsg, pFlowControlMsg, pFilterID));
}
PANDAJ2534DLL_API long PTAPI	PassThruStopMsgFilter(unsigned long ChannelID, unsigned long FilterID) {
	#pragma EXPORT
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	return ret_code(get_channel(ChannelID)->PassThruStopMsgFilter(FilterID));
}
PANDAJ2534DLL_API long PTAPI	PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long PinNumber, unsigned long Voltage) {
	#pragma EXPORT
	if (check_valid_DeviceID(DeviceID) != STATUS_NOERROR) return J25334LastError;
	auto& panda = get_device(DeviceID);

	switch (Voltage) {
	case SHORT_TO_GROUND:
		break;
	case VOLTAGE_OFF:
		break;
	default:
		if (!(5000 <= Voltage && Voltage <= 20000))
			return ret_code(ERR_NOT_SUPPORTED);
		break;
	}

	return ret_code(STATUS_NOERROR);
}
PANDAJ2534DLL_API long PTAPI	PassThruReadVersion(unsigned long DeviceID, char *pFirmwareVersion, char *pDllVersion, char *pApiVersion) {
	#pragma EXPORT
	if (!pFirmwareVersion || !pDllVersion || !pApiVersion) return ret_code(ERR_NULL_PARAMETER);
	if (check_valid_DeviceID(DeviceID) != STATUS_NOERROR) return J25334LastError;
	strcpy_s(pFirmwareVersion, 80, "00.02");
	strcpy_s(pDllVersion, 80, "00.01");
	strcpy_s(pApiVersion, 80, J2534_APIVER_NOVEMBER_2004);
	return ret_code(STATUS_NOERROR);
}
PANDAJ2534DLL_API long PTAPI	PassThruGetLastError(char *pErrorDescription) {
	#pragma EXPORT
	if (pErrorDescription) return ret_code(ERR_NULL_PARAMETER);
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
	return ret_code(STATUS_NOERROR);
}
PANDAJ2534DLL_API long PTAPI	PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID,
	                                          void *pInput, void *pOutput) {
	#pragma EXPORT
	if (check_valid_ChannelID(ChannelID) != STATUS_NOERROR) return J25334LastError;
	auto& dev_entry = get_device(ChannelID);
	//get_channel(ChannelID)

	switch (IoctlID) {
	case GET_CONFIG:
	{
		SCONFIG_LIST *inconfig = (SCONFIG_LIST*)pInput;
		if (inconfig == NULL)
			return ret_code(ERR_NULL_PARAMETER);
		for (unsigned int i = 0; i < inconfig->NumOfParams; i++) {
			switch (inconfig->ConfigPtr[i].Parameter) {
			case DATA_RATE:
				inconfig->ConfigPtr[i].Value = get_channel(ChannelID)->getBaud();
				break;
			case LOOPBACK:
				inconfig->ConfigPtr[i].Value = get_channel(ChannelID)->loopback;
				break;
			case BIT_SAMPLE_POINT:
				inconfig->ConfigPtr[i].Value = 80;
				break;
			case SYNC_JUMP_WIDTH:
				inconfig->ConfigPtr[i].Value = 15;
				break;
			default:
				// HDS rarely reads off values through ioctl GET_CONFIG, but it often
				// just wants the call to pass without erroring, so just don't do anything.
				printf("Got unknown code %X\n", inconfig->ConfigPtr[i].Parameter);
			}
		}
		break;
	}
	case SET_CONFIG:
	{
		SCONFIG_LIST *inconfig = (SCONFIG_LIST*)pInput;
		if (inconfig == NULL)
			return ret_code(ERR_NULL_PARAMETER);
		for (unsigned int i = 0; i < inconfig->NumOfParams; i++) {
			switch (inconfig->ConfigPtr[i].Parameter) {
			case DATA_RATE:			// 5-500000
				return ret_code(get_channel(ChannelID)->setBaud(inconfig->ConfigPtr[i].Value));
			case LOOPBACK:			// 0 (OFF), 1 (ON) [0]
				get_channel(ChannelID)->loopback = bool(inconfig->ConfigPtr[i].Value);
				break;
			case NODE_ADDRESS:		// J1850PWM Related (Not supported by panda). HDS requires these to 'work'.
			case NETWORK_LINE:
			case P1_MIN:			// A bunch of stuff relating to ISO9141 and ISO14230 that the panda
			case P1_MAX:			// currently doesn't support. Don't let HDS know we can't use these.
			case P2_MIN:
			case P2_MAX:
			case P3_MIN:
			case P3_MAX:
			case P4_MIN:
			case P4_MAX:
			case W0:
			case W1:
			case W2:
			case W3:
			case W4:
			case W5:
			case TIDLE:
			case TINIL:
			case TWUP:
			case PARITY:
			case T1_MAX:			// SCI related options. The panda does not appear to support this
			case T2_MAX:
			case T3_MAX:
			case T4_MAX:
			case T5_MAX:
				break;				// Just smile and nod.
			default:
				printf("Got unknown SET code %X\n", inconfig->ConfigPtr[i].Parameter);
			}
		}
		break;
	}
	case READ_VBATT:
		panda::PANDA_HEALTH health = dev_entry->panda->get_health();
		*(unsigned long*)pOutput = (long)(3.81f*health.voltage);
		break;
	case FIVE_BAUD_INIT:
		if (!pInput || !pOutput) return ret_code(ERR_NULL_PARAMETER);
		return ret_code(get_channel(ChannelID)->init5b((SBYTE_ARRAY*)pInput, (SBYTE_ARRAY*)pOutput));
	case FAST_INIT:
		if (!pInput || !pOutput) return ret_code(ERR_NULL_PARAMETER);
		return ret_code(get_channel(ChannelID)->initFast((PASSTHRU_MSG*)pInput, (PASSTHRU_MSG*)pOutput));
	case CLEAR_TX_BUFFER:
		return ret_code(get_channel(ChannelID)->clearTXBuff());
	case CLEAR_RX_BUFFER:
		return ret_code(get_channel(ChannelID)->clearRXBuff());
	case CLEAR_PERIODIC_MSGS:
		return ret_code(get_channel(ChannelID)->clearPeriodicMsgs());
	case CLEAR_MSG_FILTERS:
		return ret_code(get_channel(ChannelID)->clearMsgFilters());
	case CLEAR_FUNCT_MSG_LOOKUP_TABLE:			// LOOKUP TABLE IS RELATED TO J1850 PWM. Unsupported.
		if (!pInput) return ret_code(ERR_NULL_PARAMETER);
		return ret_code(STATUS_NOERROR);
	case ADD_TO_FUNCT_MSG_LOOKUP_TABLE:			// LOOKUP TABLE IS RELATED TO J1850 PWM. Unsupported.
		if (!pInput) return ret_code(ERR_NULL_PARAMETER);
		return ret_code(STATUS_NOERROR);
	case DELETE_FROM_FUNCT_MSG_LOOKUP_TABLE:	// LOOKUP TABLE IS RELATED TO J1850 PWM. Unsupported.
		return ret_code(STATUS_NOERROR);
	case READ_PROG_VOLTAGE:
		*(unsigned long*)pOutput = 0;
		break;
	default:
		printf("Got unknown IIOCTL %X\n", IoctlID);
	}

	return ret_code(STATUS_NOERROR);
}
