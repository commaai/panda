from __future__ import print_function
import os
import sys
import time
import socket
import select
import pytest

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
import elm_car_simulator

def elm_connect():
    s = socket.create_connection(("192.168.0.10", 35000))
    s.setblocking(0)
    return s

def read_or_fail(s):
    ready = select.select([s], [], [], 4)
    assert ready[0], "Socket did not receive data within the timeout duration."
    return s.recv(1000)

def sendrecv(s, dat):
    s.send(dat)
    return read_or_fail(s)

def send_compare(s, dat, ret):
    s.send(dat)
    res = b''
    while ret.startswith(res) and ret != res:
        ready = select.select([s], [], [], 4)
        if not ready[0]:
            print("current recv data:", repr(res))
            break;
        res += s.recv(1000)
    assert ret == res, "Data does not agree (%s) (%s)"%(repr(ret), repr(res))

def sync_reset(s):
    s.send("ATZ\r")
    res = b''
    while not res.endswith("ELM327 v1.5\r\r>"):
        res += read_or_fail(s)
        print("RES IS", repr(res))

def test_reset():
    s = socket.create_connection(("192.168.0.10", 35000))
    s.setblocking(0)

    try:
        sync_reset(s)
    finally:
        s.close()

def test_elm_cli():
    s = elm_connect()

    try:
        sync_reset(s)

        send_compare(s, b'ATI\r', b'ATI\rELM327 v1.5\r\r>')

        #Test Echo Off
        #Expected to be misimplimentation, but this is how the reference device behaved.
        send_compare(s, b'ATE0\r', b'ATE0\rOK\r\r>') #Here is the odd part
        send_compare(s, b'ATE0\r', b'OK\r\r>')       #Should prob show this immediately
        send_compare(s, b'ATI\r', b'ELM327 v1.5\r\r>')

        #Test Newline On
        send_compare(s, b'ATL1\r', b'OK\r\n\r\n>')
        send_compare(s, b'ATI\r', b'ELM327 v1.5\r\n\r\n>')
        send_compare(s, b'ATL0\r', b'OK\r\r>')
        send_compare(s, b'ATI\r', b'ELM327 v1.5\r\r>')

        send_compare(s, b'ATI\r', b'ELM327 v1.5\r\r>') #Test repeat command no echo
        send_compare(s, b'\r', b'ELM327 v1.5\r\r>')

        send_compare(s, b'aTi\r', b'ELM327 v1.5\r\r>') #Test different case

        send_compare(s, b'  a     T i\r', b'ELM327 v1.5\r\r>') #Test with white space

        send_compare(s, b'ATCATHAT\r', b'?\r\r>') #Test Invalid AT command

        send_compare(s, b'01 00 00 00 00 00 00 00\r', b'?\r\r>') #Test Invalid (too long) OBD command
        send_compare(s, b'01 GZ\r', b'?\r\r>') #Test Invalid (Non hex chars) OBD command
    finally:
        s.close()

def test_elm_setget_protocol():
    s = elm_connect()

    try:
        sync_reset(s)
        send_compare(s, b'ATE0\r', b'ATE0\rOK\r\r>') # Echo OFF

        send_compare(s, b'ATSP0\r', b"OK\r\r>") # Set auto
        send_compare(s, b'ATDP\r', b"AUTO\r\r>")
        send_compare(s, b'ATDPN\r', b"A0\r\r>")

        send_compare(s, b'ATSP6\r', b"OK\r\r>") # Set protocol
        send_compare(s, b'ATDP\r', b"ISO 15765-4 (CAN 11/500)\r\r>")
        send_compare(s, b'ATDPN\r', b"6\r\r>")

        send_compare(s, b'ATSPA6\r', b"OK\r\r>") # Set auto with protocol default
        send_compare(s, b'ATDP\r', b"AUTO, ISO 15765-4 (CAN 11/500)\r\r>")
        send_compare(s, b'ATDPN\r', b"A6\r\r>")

        send_compare(s, b'ATSP7\r', b"OK\r\r>")
        send_compare(s, b'ATDP\r', b"ISO 15765-4 (CAN 29/500)\r\r>")
        send_compare(s, b'ATDPN\r', b"7\r\r>") #Test Does not accept invalid protocols
        send_compare(s, b'ATSPD\r', b"?\r\r>")
        send_compare(s, b'ATDP\r', b"ISO 15765-4 (CAN 29/500)\r\r>")
        send_compare(s, b'ATDPN\r', b"7\r\r>")
    finally:
        s.close()

def test_elm_basic_send_can():
    s = elm_connect()
    serial = os.getenv("CANSIMSERIAL") if os.getenv("CANSIMSERIAL") else None
    sim = elm_car_simulator.ELMCanCarSimulator(serial)
    sim.start()

    try:
        sync_reset(s)
        send_compare(s, b'ATSP6\r', b"ATSP6\rOK\r\r>") # Set Proto

        send_compare(s, b'ATE0\r', b'ATE0\rOK\r\r>') # Echo OFF
        send_compare(s, b'0100\r', b"41 00 FF FF FF FE \r\r>")
        send_compare(s, b'010D\r', b"41 0D 53 \r\r>")

        send_compare(s, b'ATS0\r', b'OK\r\r>') # Spaces Off
        send_compare(s, b'0100\r', b"4100FFFFFFFE\r\r>")
        send_compare(s, b'010D\r', b"410D53\r\r>")

        send_compare(s, b'ATH1\r', b'OK\r\r>') # Spaces Off Headers On
        send_compare(s, b'0100\r', b"7E8064100FFFFFFFE\r\r>")
        send_compare(s, b'010D\r', b"7E803410D53\r\r>")

        send_compare(s, b'ATS1\r', b'OK\r\r>') # Spaces On Headers On
        send_compare(s, b'0100\r', b"7E8 06 41 00 FF FF FF FE \r\r>")
        send_compare(s, b'010D\r', b"7E8 03 41 0D 53 \r\r>")

        send_compare(s, b'1F00\r', b"NO DATA\r\r>") # Unhandled msg, no response.
    finally:
        sim.stop()
        sim.join()
        s.close()
