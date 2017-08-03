#!/usr/bin/env python
"""Used to Reverse/Test ELM protocol auto detect and OBD message response without a car."""
from __future__ import print_function
import sys
import os
import struct
import binascii

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda

if __name__ == "__main__":
    serial = os.getenv("SERIAL") if os.getenv("SERIAL") else Panda.list()[0]
    p = Panda(serial)
    p.set_can_speed_kbps(0, 500)
    p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)

    while True:
        can_recv = p.can_recv()
        if can_recv:
            a, b, c, d = can_recv[0]
            #print(hex(a),b,repr(c),d)
        for address, ts, data, src in can_recv:
            if src is 0 and len(data) >= 2:
                #Check functional address of 11 bit and 29 bit CAN
                dat_trim = data[1:1+data[0]]
                if address == 0x18db33f1: continue
                print("MSG", binascii.hexlify(dat_trim), "Addr:", hex(address), "Mode:", hex(dat_trim[0])[2:].zfill(2),
                      "PID:", hex(dat_trim[1])[2:].zfill(2), binascii.hexlify(dat_trim[2:]) if dat_trim[2:] else '')
                if address == 0x7DF:# or address == 0x18db33f1:
                    outmsg = None
                    """
                    MSG b'0133' Addr: 0x7df Mode: 01 PID: 33
                    MSG b'010b' Addr: 0x7df Mode: 01 PID: 0b
                    MSG b'0110' Addr: 0x7df Mode: 01 PID: 10
                    MSG b'010c' Addr: 0x7df Mode: 01 PID: 0c
                        Reply b'0441001a'
                    MSG b'010d' Addr: 0x7df Mode: 01 PID: 0d
                        Reply b'034100'
                    MSG b'0104' Addr: 0x7df Mode: 01 PID: 04
                    MSG b'0133' Addr: 0x7df Mode: 01 PID: 33
                    MSG b'010c' Addr: 0x7df Mode: 01 PID: 0c
                        Reply b'0441001a'
                    MSG b'0111' Addr: 0x7df Mode: 01 PID: 11
                        Reply b'034100'
                    """
                    if data[1] == 0x01: # Mode: Show current data
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
                    if outmsg:
                        #print("Got req msg")
                        outmsg = struct.pack("BBB", len(outmsg)+2, 0x40|data[1], data[2]) + outmsg
                        outmsg += b'\x00'*(8-len(outmsg))
                        outaddr = 0x7E8 if address == 0x7DF else 0
                        #b"\x06\x41\x00\xBE\x1F\xB8\x10\x00"
                        #Should display on elm '41 00 BE 1F B8 10'
                        print("    Reply", binascii.hexlify(outmsg))#[:outmsg[0]]+1))
                        p.can_send(outaddr, outmsg, 0)
