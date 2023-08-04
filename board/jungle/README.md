Welcome to the jungle
======
Firmware for the internal Panda Jungle testing boards.
Forked from panda firmware ([adc0c12](https://github.com/commaai/panda/commit/adc0c12f7b403e04222799bde4f4ee0673e46160)).

## udev rules
To make the jungle usable without root permissions, you might need to setup udev rules for it.
On ubuntu, this should do the trick:
``` bash
sudo tee /etc/udev/rules.d/12-panda_jungle.rules <<EOF
SUBSYSTEM=="usb", ATTRS{idVendor}=="bbaa", ATTRS{idProduct}=="ddcf", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="bbaa", ATTRS{idProduct}=="ddef", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Updating the firmware on your jungle
Updating the firmware is easy! In the `board` folder, run:
``` bash
./flash.sh
```

If you somehow bricked your jungle, you'll need a [comma key](https://comma.ai/shop/products/comma-key) to put the microcontroller in DFU mode for the V1. For V2, the onboard button serves this purpose. When powered on while holding the button to put it in DFU mode, running `./recover.sh` in `board/` should unbrick it.
