#pragma once
#include "J2534_v0404.h"

class J2534MessageFilter {
public:
	J2534MessageFilter(
		unsigned int filtertype,
		PASSTHRU_MSG *pMaskMsg,
		PASSTHRU_MSG *pPatternMsg,
		PASSTHRU_MSG *pFlowControlMsg
	);
private:
	unsigned int filtertype;
	PASSTHRU_MSG MaskMsg;
	PASSTHRU_MSG PatternMsg;
	PASSTHRU_MSG FlowControlMsg;
};