#pragma once

#include "J2534Connection.h"
#include "panda_shared/panda.h"

class J2534Connection_ISO14230 : public J2534Connection {
public:
    J2534Connection_ISO14230(
        std::shared_ptr<PandaJ2534Device> panda_dev,
        unsigned long ProtocolID,
        unsigned long Flags,
        unsigned long BaudRate
    );

    virtual unsigned long validateTxMsg(PASSTHRU_MSG* msg);

    virtual std::shared_ptr<MessageTx> parseMessageTx(PASSTHRU_MSG& pMsg);

    virtual void setBaud(unsigned long baud);

    virtual unsigned long getMinMsgLen() {
        return 2;
    }

    virtual unsigned long getMaxMsgLen() {
        return 260;
    }

    virtual unsigned long getMaxMsgSingleFrameLen() {
        return 260;
    }

    virtual bool isProtoCan() {
        return FALSE;
    }

    virtual void processMessage(const J2534Frame& msg);
};