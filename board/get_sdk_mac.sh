#!/bin/bash
# Need formula for gcc
brew tap ArmMbed/homebrew-formulae
brew install python dfu-util arm-none-eabi-gcc
pip2 install libusb1 requests
pip2 install --user pycrypto
