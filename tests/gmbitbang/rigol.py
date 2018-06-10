#!/usr/bin/env python
import numpy as np
import visa
import matplotlib.pyplot as plt

resources = visa.ResourceManager()
print resources.list_resources()

scope = resources.open_resource('USB0::0x1AB1::0x04CE::DS1ZA184652242::INSTR', timeout=2000, chunk_size=1024000)
print(scope.query('*IDN?').strip())

#voltscale = scope.ask_for_values(':CHAN1:SCAL?')[0]
#voltoffset = scope.ask_for_values(":CHAN1:OFFS?")[0]

#scope.write(":STOP")
scope.write(":WAV:POIN:MODE RAW")
scope.write(":WAV:DATA? CHAN1")[10:]
rawdata = scope.read_raw()
data = np.frombuffer(rawdata, 'B')
print data.shape
plt.plot(data)
plt.show()
#data = (data - 130.0 - voltoffset/voltscale*25) / 25 * voltscale

print data

