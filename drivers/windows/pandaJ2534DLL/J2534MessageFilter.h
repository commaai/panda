#pragma once
#include "J2534_v0404.h"
#include "J2534Connection.h"

typedef enum {
	FILTER_RESULT_BLOCK,
	FILTER_RESULT_NEUTRAL,
	FILTER_RESULT_PASS,
	FILTER_RESULT_NOMATCH = FILTER_RESULT_BLOCK,
	FILTER_RESULT_MATCH = FILTER_RESULT_PASS,
} FILTER_RESULT;

//Forward declare
class J2534Connection;

class J2534MessageFilter {
public:
	J2534MessageFilter(
		J2534Connection *const conn,
		unsigned int filtertype,
		PASSTHRU_MSG *pMaskMsg,
		PASSTHRU_MSG *pPatternMsg,
		PASSTHRU_MSG *pFlowControlMsg
	);

	bool J2534MessageFilter::operator ==(const J2534MessageFilter &b) const;

	FILTER_RESULT check(const PASSTHRU_MSG_INTERNAL& msg);
	std::string get_flowctrl();
private:
	unsigned int filtertype;
	std::string maskMsg;
	std::string patternMsg;
	std::string flowCtrlMsg;
	unsigned long flowCtrlTxFlags;
};