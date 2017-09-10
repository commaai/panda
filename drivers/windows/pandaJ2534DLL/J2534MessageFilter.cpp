#include "stdafx.h"
#include "J2534MessageFilter.h"

J2534MessageFilter::J2534MessageFilter(
	J2534Connection *const conn,
	unsigned int filtertype,
	PASSTHRU_MSG *pMaskMsg,
	PASSTHRU_MSG *pPatternMsg,
	PASSTHRU_MSG *pFlowControlMsg
) : filtertype(filtertype) {
	switch (filtertype) {
	case PASS_FILTER:
	case BLOCK_FILTER:
		if (pMaskMsg == NULL || pPatternMsg == NULL)
			throw ERR_NULL_PARAMETER;
		if (pFlowControlMsg != NULL)
			throw ERR_INVALID_MSG;
		if (pMaskMsg->DataSize != pPatternMsg->DataSize)
			throw ERR_INVALID_MSG;
		break;
	case FLOW_CONTROL_FILTER:
		if (conn->getProtocol() != ISO15765) throw ERR_MSG_PROTOCOL_ID; //CHECK
		if (pFlowControlMsg == NULL)
			throw ERR_NULL_PARAMETER;
		if (pMaskMsg == NULL || pPatternMsg == NULL)
			throw ERR_INVALID_MSG;
		break;
	default:
		throw ERR_INVALID_MSG;;
	}

	if (pMaskMsg) {
		if (!(conn->getMinMsgLen() < pMaskMsg->DataSize || pMaskMsg->DataSize < conn->getMaxMsgLen()))
			throw ERR_INVALID_MSG;
		if (conn->getProtocol() != pMaskMsg->ProtocolID)
			throw ERR_MSG_PROTOCOL_ID;
		this->maskMsg = std::string((char*)pMaskMsg->Data, pMaskMsg->DataSize);
	}

	if (pPatternMsg) {
		if (!(conn->getMinMsgLen() < pPatternMsg->DataSize || pPatternMsg->DataSize < conn->getMaxMsgLen()))
			throw ERR_INVALID_MSG;
		if (conn->getProtocol() != pPatternMsg->ProtocolID)
			throw ERR_MSG_PROTOCOL_ID;
		this->patternMsg = std::string((char*)pPatternMsg->Data, pPatternMsg->DataSize);
	}

	if (pFlowControlMsg) {
		if (!(conn->getMinMsgLen() < pFlowControlMsg->DataSize || pFlowControlMsg->DataSize < conn->getMaxMsgLen()))
			throw ERR_INVALID_MSG;
		if (conn->getProtocol() != pFlowControlMsg->ProtocolID)
			throw ERR_MSG_PROTOCOL_ID;
		this->flowCtrlMsg = std::string((char*)pFlowControlMsg->Data, pFlowControlMsg->DataSize);
		this->flowCtrlTxFlags = pFlowControlMsg->TxFlags;
	}
};

FILTER_RESULT J2534MessageFilter::check(const PASSTHRU_MSG_INTERNAL& msg) {
	bool matches = TRUE;
	if (msg.DataSize < this->maskMsg.size()) {
		matches = FALSE;
	} else {
		for (int i = 0; i < (this->maskMsg.size()); i++) {
			if (this->patternMsg[i] != (msg.Data[i] & this->maskMsg[i])) {
				matches = FALSE;
				break;
			}
		}
	}

	switch (this->filtertype) {
	case PASS_FILTER:
		return matches ? FILTER_RESULT_PASS : FILTER_RESULT_NEUTRAL;
	case BLOCK_FILTER:
		return matches ? FILTER_RESULT_NEUTRAL : FILTER_RESULT_BLOCK;
	default:
		return matches ? FILTER_RESULT_PASS : FILTER_RESULT_NEUTRAL;
	}
}