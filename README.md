Welcome to panda
======

**This repo is a work in progress and will be for the next month while we ship our first pandas to [chffr](http://getchffr.com) users. This is not production code, and shouldn't be used as such. We are open sourcing it early for developers to start building panda support. If you want a panda, download our dashcam app [chffr](http://getchffr.com) and earn comma points!**

[panda](http://github.com/commaai/panda) is the nicest universal car interface you've ever seen. 

<img src="https://github.com/commaai/panda/blob/master/panda.png">

It supports 3x CAN, 2x LIN, and 1x GMLAN. It also charges a phone.

It uses an [STM32F413](http://www.st.com/en/microcontrollers/stm32f413-423.html?querycriteria=productId=LN2004) for low level stuff and an [ESP8266](https://en.wikipedia.org/wiki/ESP8266) for wifi. They are connected over high speed SPI, so the panda is actually capable of dumping the full contents of the busses over wifi, unlike every other dongle on amazon. ELM327 is weak, panda is strong.

It is 2nd gen hardware, reusing code and parts from the [NEO](https://github.com/commaai/neo) interface board.

Usage
------

See [panda.py](https://github.com/commaai/panda/blob/master/lib/panda.py) for how to interact with the panda.

For example, to receive CAN messages:
```
>>> from panda.lib.panda import Panda
>>> panda = Panda()
>>> panda.can_recv()
```
And to send one on bus 0:
```
>>> panda.can_send(0x1aa, "message", 0)
```
More examples coming soon

Directory structure
------

- board      -- Code that runs on the STM32
- boardesp   -- Code that runs on the ESP8266
- lib        -- Python userspace library for interfacing with the panda
- tests      -- Tests and helper programs for panda

Programming (over USB)
------

Programming the STM32
```
cd board
./get_sdk.sh
make
```

Programming the ESP
```
cd boardesp
./get_sdk.sh
make
```

Debugging
------

To print out the serial console from the STM32, run tests/debug_console.py

To print out the serial console from the ESP8266, run PORT=1 tests/debug_console.py

Hardware
------

Check out the hardware [guide](https://github.com/commaai/panda/blob/master/docs/guide.pdf)

Licensing
------

panda software is released under the MIT license unless otherwise specified.

