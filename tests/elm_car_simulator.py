#!/usr/bin/env python
"""Used to Reverse/Test ELM protocol auto detect and OBD message response without a car."""
from __future__ import print_function
import sys
import os
import struct
import binascii
import time
import threading

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda

class ELMCanCarSimulator(threading.Thread):
    def __init__(self, sn, can_kbaud=500, *args, **kwargs):
        super(ELMCanCarSimulator, self).__init__(*args, **kwargs)
        self._p = Panda(sn if sn else Panda.list()[0])
        self.__stop = False
        self._multipart_data = None
        self._can_kbaud = can_kbaud

        self._p.can_recv() # Toss whatever was already there

    def stop(self):
        self.__stop = True

    def _can_send(self, addr, msg):
        print("    Reply (%x)" % addr, binascii.hexlify(msg))
        return self._p.can_send(addr, msg + b'\x00'*(8-len(msg)), 0)

    def _addr_matches(self, addr):
        return addr in (0x7DF, 0x7E0, 0x18db33f1)

    def run(self):
        self._p.set_can_speed_kbps(0, self._can_kbaud)
        self._p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)

        while not self.__stop:
            for address, ts, data, src in self._p.can_recv():
                if src is 0 and len(data) >= 3:
                    self._process_msg(data[1], data[2], address, ts, data, src)

    def change_can_baud(self, kbaud):
        self._can_kbaud = kbaud
        self._p.set_can_speed_kbps(0, self._can_kbaud)

    def _process_msg(self, mode, pid, address, ts, data, src):
        #Check functional address of 11 bit and 29 bit CAN
        #if address == 0x18db33f1: return

        print("MSG", binascii.hexlify(data[1:1+data[0]]), "Addr:", hex(address),
              "Mode:", hex(mode)[2:].zfill(2), "PID:", hex(pid)[2:].zfill(2),
              "Len:", data[0], binascii.hexlify(data[3:1+data[0]]) if data[3:1+data[0]] else '')

        if self._addr_matches(address):
            outmsg = None
            if data[:3] == b'\x30\x00\x00' and len(self._multipart_data):
                print("Request for more data");
                outaddr = 0x7E8 if address == 0x7DF or address == 0x7E0 else 0x18DAF110
                msgnum = 1
                while(self._multipart_data):
                    datalen = min(7, len(self._multipart_data))
                    msgpiece = struct.pack("B", 0x20 | msgnum) + self._multipart_data[:datalen]
                    self._can_send(outaddr, msgpiece)
                    self._multipart_data = self._multipart_data[7:]
                    msgnum = (msgnum+1)%0x10
                    time.sleep(0.01)

            elif mode == 0x01: # Mode: Show current data
                if pid == 0x00:   #List supported things
                    outmsg = b"\xff\xff\xff\xfe"#b"\xBE\x1F\xB8\x10" #Bitfield, random features
                elif pid == 0x01: # Monitor Status since DTC cleared
                    outmsg = b"\x00\x00\x00\x00" #Bitfield, random features
                elif pid == 0x04: # Calculated engine load
                    outmsg = b"\x2f"
                elif pid == 0x05: # Engine coolant temperature
                    outmsg = b"\x3c"
                elif pid == 0x0B: # Intake manifold absolute pressure
                    outmsg = b"\x90"
                elif pid == 0x0C: # Engine RPM
                    outmsg = b"\x1A\xF8"
                elif pid == 0x0D: # Vehicle Speed
                    outmsg = b"\x53"
                elif pid == 0x10: # MAF air flow rate
                    outmsg = b"\x01\xA0"
                elif pid == 0x11: # Throttle Position
                    outmsg = b"\x90"
                elif pid == 0x33: # Absolute Barometric Pressure
                    outmsg = b"\x90"
            elif mode == 0x09: # Mode: Request vehicle information
                if pid == 0x02:   # Show VIN
                    outmsg = b"1D4GP00R55B123456"
                if pid == 0xFD:   # test long multi message
                    parts = (b'\xAA\xAA\xAA' + struct.pack(">I", num) for num in range(80))
                    outmsg = b'\xAA\xAA\xAA' + b''.join(parts)
                if pid == 0xFE:   # test very long multi message
                    parts = (b'\xAA\xAA\xAA' + struct.pack(">I", num) for num in range(584))
                    outmsg = b'\xAA\xAA\xAA' + b''.join(parts) + b'\xAA'
                if pid == 0xFF:
                    outmsg = b"\xAA"*(0xFFF-3)

            if outmsg:
                outaddr = 0x7E8 if address == 0x7DF or address == 0x7E0 else 0x18DAF110

                if len(outmsg) <= 5:
                    self._can_send(outaddr,
                                   struct.pack("BBB", len(outmsg)+2, 0x40|data[1], pid) + outmsg)
                else:
                    first_msg_len = min(3, len(outmsg)%7)
                    payload_len = len(outmsg)+3
                    msgpiece = struct.pack("BBBBB", 0x10 | ((payload_len>>8)&0xF),
                                           payload_len&0xFF,
                                           0x40|data[1], pid, 1) + outmsg[:first_msg_len]
                    self._can_send(outaddr, msgpiece)
                    self._multipart_data = outmsg[first_msg_len:]


if __name__ == "__main__":
    serial = os.getenv("SERIAL") if os.getenv("SERIAL") else None
    kbaud = int(os.getenv("CANKBAUD")) if os.getenv("CANKBAUD") else 500
    sim = ELMCanCarSimulator(serial, can_kbaud=kbaud)
    sim.start()
