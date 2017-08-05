#!/usr/bin/env python
"""Used to Reverse/Test ELM protocol auto detect and OBD message response without a car."""
from __future__ import print_function
import sys
import os
import struct
import binascii
import time

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda

if __name__ == "__main__":
    serial = os.getenv("SERIAL") if os.getenv("SERIAL") else Panda.list()[0]
    p = Panda(serial)
    p.set_can_speed_kbps(0, 500)
    p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)

    multipart_data = None

    while True:
        can_recv = p.can_recv()
        for address, ts, data, src in can_recv:
            if src is 0 and len(data) >= 2:
                #Check functional address of 11 bit and 29 bit CAN
                dat_trim = data[1:1+data[0]]
                if address == 0x18db33f1: continue
                print("MSG", binascii.hexlify(dat_trim), "Addr:", hex(address),
                      "Mode:", hex(dat_trim[0])[2:].zfill(2),
                      "PID:", hex(dat_trim[1])[2:].zfill(2), "Len:", data[0],
                      binascii.hexlify(dat_trim[2:]) if dat_trim[2:] else '')

                if address == 0x7DF or address == 0x7E0:# or address == 0x18db33f1:
                    outmsg = None
                    if data[:3] == b'\x30\x00\x00' and len(multipart_data):
                        print("Request for more data");
                        msgnum = 1
                        while(multipart_data):
                            datalen = min(7, len(multipart_data))
                            msgpiece = struct.pack("B", 0x20 | msgnum) + multipart_data[:datalen]
                            print("    Reply", binascii.hexlify(msgpiece))
                            p.can_send(outaddr, msgpiece + b'\x00'*(8-len(msgpiece)), 0)
                            multipart_data = multipart_data[7:]
                            msgnum = (msgnum+1)%0x10
                            time.sleep(0.01)

                    elif data[1] == 0x01: # Mode: Show current data
                        if data[2] == 0x00:   #List supported things
                            outmsg = b"\xff\xff\xff\xfe"#b"\xBE\x1F\xB8\x10" #Bitfield, random features
                        elif data[2] == 0x01: # Monitor Status since DTC cleared
                            outmsg = b"\x00\x00\x00\x00" #Bitfield, random features
                        elif data[2] == 0x04: # Calculated engine load
                            outmsg = b"\x2f"
                        elif data[2] == 0x05: # Engine coolant temperature
                            outmsg = b"\x3c"
                        elif data[2] == 0x0B: # Intake manifold absolute pressure
                            outmsg = b"\x90"
                        elif data[2] == 0x0C: # Engine RPM
                            outmsg = b"\x1A\xF8"
                        elif data[2] == 0x0D: # Vehicle Speed
                            outmsg = b"\x53"
                        elif data[2] == 0x10: # MAF air flow rate
                            outmsg = b"\x01\xA0"
                        elif data[2] == 0x11: # Throttle Position
                            outmsg = b"\x90"
                        elif data[2] == 0x33: # Absolute Barometric Pressure
                            outmsg = b"\x90"
                    elif data[1] == 0x09: # Mode: Request vehicle information
                        if data[2] == 0x02:   # Show VIN
                            outmsg = b"1D4GP00R55B123456"
                        if data[2] == 0xFF:   # test very long multi message
                            outmsg = b"\xAA"*(0xFFF-3)

                    if outmsg:
                        outaddr = 0x7E8 if address == 0x7DF or address == 0x7E0 else 0

                        if len(outmsg) <= 5:
                            outmsg = struct.pack("BBB", len(outmsg)+2, 0x40|data[1], data[2]) + outmsg
                            outmsg += b'\x00'*(8-len(outmsg))
                            print("    Reply", binascii.hexlify(outmsg))
                            p.can_send(outaddr, outmsg, 0)
                        else:
                            first_msg_len = min(3, len(outmsg)%7)
                            payload_len = len(outmsg)+3
                            msgpiece = struct.pack("BBBBB", 0x10 | ((payload_len>>8)&0xF),
                                                   payload_len&0xFF,
                                                   0x40|data[1], data[2], 1) + outmsg[:first_msg_len]
                            print("    Reply", binascii.hexlify(msgpiece))
                            p.can_send(outaddr, msgpiece + b'\x00'*(8-len(msgpiece)), 0)
                            multipart_data = outmsg[first_msg_len:]
