#!/usr/bin/env python
import sys
from Crypto.PublicKey import RSA

rsa = RSA.importKey(open(sys.argv[1]).read())
mod = (hex(rsa.n)[2:-1].rjust(0x100, '0'))
hh = ''.join('\\x'+mod[i:i+2] for i in range(0, 0x100, 2))
print 'char rsa_mod[] = "'+hh+'";'
print 'int rsa_e = %d;' % rsa.e


