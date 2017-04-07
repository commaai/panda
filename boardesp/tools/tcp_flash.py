#!/usr/bin/env python
#
# tcp_flash.py - flashes an ESP8266 microcontroller via 'raw' TCP/IP (not HTTP).
#
# Usage:
#   tcp_flash.py <host|IP> <user1.bin> <user2.bin>
#
# Where:
#   <host|IP>    the hostname or IP address of the ESP8266 to be flashed.
#   <user1.bin>  the file holding the first flash format file. Used when the currently used flash is user2.bin
#   <user2.bin>  the file holding the second flash format file. Used when the currently used flash is user1.bin
#
# Author: Ian Marshall
# Date: 27/05/2016
#

import socket
import sys

PORT=65056

# Verify the parameters.
if len(sys.argv) < 3:
	print 'Usage: '
	print '   Usage:'
	print '     tcp_flash.py <host|IP> <user1.bin> <user2.bin>'
	print ''
	print '   Where:'
	print '     <host|IP>    the hostname or IP address of the ESP8266 to be flashed.'
	print '     <user1.bin>  the file holding the first flash format file.'
	print '                  Used when the currently used flash is user2.bin'
	print '     <user2.bin>  the file holding the second flash format file.'
	print '                  Used when the currently used flash is user1.bin'
	sys.exit(1)

# Copy the parameters to more descriptive variables.
host = sys.argv[1]
user1bin = sys.argv[2]
user2bin = sys.argv[3]
print 'Flashing to "{}"'.format(host)

# Open the connection to the ESP8266.
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(5);
s.connect((host, PORT))

# Send a request for the correct user bin to be flashed.
s.send('OTA\r\nGetNextFlash\r\n')

# Wait for the reply.
f = None
response = s.recv(128)
if response == "user1.bin\r\n":
	print 'Flashing \"{}\"...'.format(user1bin)
	f = open(user1bin, "rb")
elif response == "user2.bin\r\n":
	print 'Flashing \"{}\"...'.format(user2bin)
	f = open(user2bin, "rb")
else:
	print 'Unknown binary version requested by ESP8266: "{}"'.format(response)
	sys.exit(2)

# Read the firmware file.
contents = f.read()
f.close()

# Send through the firmware length 
s.send('FirmwareLength: {}\r\n'.format(len(contents)))

# Wait until we get the go-ahead.
response = s.recv(128)
if response != "Ready\r\n":
	print 'Received response: {}'.format(response)
	sys.exit(3)

# Send the firmware.
print 'Sending {} bytes of firmware'.format(len(contents))
s.sendall(contents)
response = s.recv(128)
if len(response) > 0:
	print 'Received response: {}'.format(response)

# Close the connection, as we're now done.
s.close()
sys.exit(0)
