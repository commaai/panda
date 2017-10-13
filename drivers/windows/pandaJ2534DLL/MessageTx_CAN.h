#pragma once
#include <memory>
#include "MessageTx.h"
#include "J2534Connection_CAN.h"

class J2534Connection_CAN;

class MessageTx_CAN : public MessageTx
{
public:
	MessageTx_CAN(
		std::shared_ptr<J2534Connection> connection_in,
		PASSTHRU_MSG& to_send
	) : MessageTx(connection_in), fullmsg(to_send) , sentyet(FALSE), txInFlight(FALSE) {};

	virtual BOOL sendNextFrame() {
		uint32_t addr = ((uint8_t)fullmsg.Data[0]) << 24 | ((uint8_t)fullmsg.Data[1]) << 16 |
			((uint8_t)fullmsg.Data[2]) << 8 | ((uint8_t)fullmsg.Data[3]);

		if (auto conn_sp = std::static_pointer_cast<J2534Connection_CAN>(this->connection.lock())) {
			if (auto panda_dev_sp = conn_sp->getPandaDev()) {
				auto payload = fullmsg.Data.substr(4);
				if (panda_dev_sp->panda->can_send(addr, check_bmask(this->fullmsg.TxFlags, CAN_29BIT_ID),
					(const uint8_t*)payload.c_str(), (uint8_t)payload.size(), panda::PANDA_CAN1) == FALSE) {
					return FALSE;
				}
				this->txInFlight = TRUE;
				this->sentyet = TRUE;
				return TRUE;
			}
		}
		return FALSE;
	};

	//Returns TRUE if receipt is consumed by the msg, FALSE otherwise.
	virtual BOOL checkTxReceipt(J2534Frame frame) {
		if (txReady()) return FALSE;
		if (frame.Data == fullmsg.Data && ((this->fullmsg.TxFlags & CAN_29BIT_ID) == (frame.RxStatus & CAN_29BIT_ID))) {
			txInFlight = FALSE;
			return TRUE;
		}
		return FALSE;
	}

	virtual BOOL isFinished() {
		return !txInFlight && sentyet;
	};

	virtual BOOL txReady() {
		return !sentyet;
	};

private:
	BOOL sentyet;
	BOOL txInFlight;

	J2534Frame fullmsg;
};
