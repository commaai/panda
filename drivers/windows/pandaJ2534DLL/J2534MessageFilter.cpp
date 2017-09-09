#include "stdafx.h"
#include "J2534MessageFilter.h"

J2534MessageFilter::J2534MessageFilter(
	unsigned int filtertype,
	PASSTHRU_MSG *pMaskMsg,
	PASSTHRU_MSG *pPatternMsg,
	PASSTHRU_MSG *pFlowControlMsg
) : filtertype(filtertype) {
	if (pMaskMsg)
		memcpy(&this->MaskMsg, pMaskMsg, sizeof(PASSTHRU_MSG));
	else
		memset(&this->MaskMsg, 0, sizeof(PASSTHRU_MSG));

	if (pPatternMsg)
		memcpy(&this->PatternMsg, pPatternMsg, sizeof(PASSTHRU_MSG));
	else
		memset(&this->PatternMsg, 0, sizeof(PASSTHRU_MSG));

	if (pFlowControlMsg)
		memcpy(&this->FlowControlMsg, pFlowControlMsg, sizeof(PASSTHRU_MSG));
	else
		memset(&this->FlowControlMsg, 0, sizeof(PASSTHRU_MSG));
};
