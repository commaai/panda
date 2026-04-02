Welcome to the jungle
======

Firmware for the Panda Jungle testing board.
Available for purchase at the [comma shop](https://comma.ai/shop/panda-jungle).

## udev rules

To make the jungle usable without root permissions, you might need to setup udev rules for it.
On ubuntu, this should do the trick:
``` bash
sudo tee /etc/udev/rules.d/12-panda_jungle.rules <<EOF
SUBSYSTEM=="usb", ATTRS{idVendor}=="3801", ATTRS{idProduct}=="ddcf", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="3801", ATTRS{idProduct}=="ddef", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## updating the firmware
Updating the firmware is easy! In the `board/jungle/` folder, run:
``` bash
./flash.py
```

If you somehow bricked your jungle, you'll need a [comma key](https://comma.ai/shop/products/comma-key) to put the microcontroller in DFU mode for the V1.
For V2, the onboard button serves this purpose. When powered on while holding the button to put it in DFU mode, running `./recover.sh` in `board/` should unbrick it.

## SD card CAN replay (V2 only)

The jungle V2 SD card slot can replay CAN messages from an openpilot route autonomously, without needing a host PC.

### Provision the SD card

From an openpilot checkout, run:
``` bash
python board/jungle/scripts/provision_sd.py /path/to/rlog /dev/sdX
```

Or write to a binary file and `dd` it to the card:
``` bash
python board/jungle/scripts/provision_sd.py /path/to/rlog replay.bin
sudo dd if=replay.bin of=/dev/sdX bs=512
```

### Start/stop replay

``` python
from panda import PandaJungle

p = PandaJungle()
p.sd_replay_start()

# Poll progress
status = p.sd_replay_status()
# {"state": 1, "total_records": 12345, "current_record": 500}
# state: 0=idle, 1=active, 2=done, 3=error

p.sd_replay_stop()
```

Messages are sent in real time, preserving the original inter-message timing from the route. The jungle replays from the SD card automatically on the next `sd_replay_start()` call without re-provisioning.
