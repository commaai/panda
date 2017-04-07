Welcome to panda
======

[panda](http://github.com/commaai/panda) is the nicest universal car interface you've ever seen. 

It supports 3x CAN, 2x LIN, and 1x GMLAN.

It uses an [STM32F413](http://www.st.com/en/microcontrollers/stm32f413-423.html?querycriteria=productId=LN2004) for low level stuff and an [ESP8266](https://en.wikipedia.org/wiki/ESP8266) for wifi. They are connected over high speed SPI, so the panda is actually capable of dumping the full contents of the busses over wifi, unlike every other dongle on amazon. ELM327 is weak, panda is strong.

It is 2nd gen hardware, reusing code and parts from the [NEO](https://github.com/commaai/neo) interface board.

Hardware
------
Check out the hardware [guide](https://github.com/commaai/panda/raw/master/docs/guide.pdf)

Directory structure
------

- board      -- Code that runs on the STM32
- boardesp   -- Code that runs on the ESP8266
- lib        -- Python userspace library for interfacing with the panda
- tests      -- Tests and helper programs for panda

Usage
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

Licensing
------

panda software is released under the MIT license. See LICENSE

