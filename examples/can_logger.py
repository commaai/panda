#!/usr/bin/env python

import binascii
import csv
from panda import Panda

def can_logger():
  
  try:
    p = Panda()
  
    outputfile = open('output.csv', 'wb')
    csvwriter = csv.writer(outputfile)
    #Write Header
    csvwriter.writerow(['Bus', 'MessageID', 'Message'])
    print("Writing csv file. Press Ctrl-C to exit...")

    while True:
      can_recv = p.can_recv()
      for address, _, dat, src  in can_recv:
        csvwriter.writerow([str(src), str(address), binascii.hexlify(dat)])

  except KeyboardInterrupt:
    print("Exiting...")
    outputfile.close()

if __name__ == "__main__":
  can_logger()
