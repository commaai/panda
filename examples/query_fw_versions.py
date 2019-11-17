#!/usr/bin/env python3
from tqdm import tqdm
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, DATA_IDENTIFIER_TYPE

if __name__ == "__main__":
  addrs = [0x700 + i for i in range(256)]
  addrs += [0x18da0000 + (i<<8) + 0xf1 for i in range(256)]
  results = {}

  panda = Panda()
  panda.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
  print("querying addresses ...")
  for addr in tqdm(addrs):
    # skip functional broadcast addrs
    if addr == 0x7df or addr == 0x18db33f1:
      continue

    uds_client = UdsClient(panda, addr, bus=1 if panda.has_obd() else 0, timeout=0.1, debug=False)
    try:
      uds_client.tester_present()
    except NegativeResponseError:
      pass
    except MessageTimeoutError:
      continue

    resp = {}

    try:
      data = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.BOOT_SOFTWARE_IDENTIFICATION)
      if data: resp[DATA_IDENTIFIER_TYPE.BOOT_SOFTWARE_IDENTIFICATION] = data
    except NegativeResponseError:
      pass

    try:
      data = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_IDENTIFICATION)
      if data: resp[DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_IDENTIFICATION] = data
    except NegativeResponseError:
      pass

    try:
      data = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_DATA_IDENTIFICATION)
      if data: resp[DATA_IDENTIFIER_TYPE.APPLICATION_DATA_IDENTIFICATION] = data
    except NegativeResponseError:
      pass

    try:
      data = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.BOOT_SOFTWARE_FINGERPRINT)
      if data: resp[DATA_IDENTIFIER_TYPE.BOOT_SOFTWARE_FINGERPRINT] = data
    except NegativeResponseError:
      pass

    try:
      data = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_FINGERPRINT)
      if data: resp[DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_FINGERPRINT] = data
    except NegativeResponseError:
      pass

    try:
      data = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_DATA_FINGERPRINT)
      if data: resp[DATA_IDENTIFIER_TYPE.APPLICATION_DATA_FINGERPRINT] = data
    except NegativeResponseError:
      pass

    if resp.keys():
      results[addr] = resp

  print("results:")
  for addr, resp in results.items():
    for id, dat in resp.items():
      print(hex(addr), hex(id), dat.decode())
