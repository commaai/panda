Adaption to MQBA0 EPS 2Q1909144J 6030 used in a PQ26 no radar non ACC car (Volkswagen Polo 6C [Polo 6R Facelift]) PQ26 can be seen as nearly identical to MQB regarding can bus communication.

Panda can 0 is directly connected to engine can bus Panda can 1 is directly connected to comfort can bus

# Welcome to panda

![panda tests](https://github.com/commaai/panda/workflows/tests/badge.svg)
![panda drivers](https://github.com/commaai/panda/workflows/drivers/badge.svg)

panda speaks CAN and CAN FD, and it runs on [STM32F205](https://www.st.com/resource/en/reference_manual/rm0033-stm32f205xx-stm32f207xx-stm32f215xx-and-stm32f217xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf), [STM32F413](https://www.st.com/resource/en/reference_manual/rm0430-stm32f413423-advanced-armbased-32bit-mcus-stmicroelectronics.pdf), and [STM32H725](https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).

## Directory structure

```
.
├── board           # Code that runs on the STM32
├── drivers         # Drivers (not needed for use with Python)
├── python          # Python userspace library for interfacing with the panda
├── tests           # Tests and helper programs for panda
```

## Safety Model

When a panda powers up, by default it's in `SAFETY_SILENT` mode. While in `SAFETY_SILENT` mode, the CAN buses are forced to be silent. In order to send messages, you have to select a safety mode. Some of safety modes (for example `SAFETY_ALLOUTPUT`) are disabled in release firmwares. In order to use them, compile and flash your own build.

Safety modes optionally support `controls_allowed`, which allows or blocks a subset of messages based on a customizable state in the board.

## Code Rigor

The panda firmware is written for its use in conjuction with [openpilot](https://github.com/commaai/openpilot). The panda firmware, through its safety model, provides and enforces the
[openpilot safety](https://github.com/commaai/openpilot/blob/master/docs/SAFETY.md). Due to its critical function, it's important that the application code rigor within the `board` folder is held to high standards.

These are the [CI regression tests](https://github.com/commaai/panda/actions) we have in place:
* A generic static code analysis is performed by [cppcheck](https://github.com/danmar/cppcheck/).
* In addition, [cppcheck](https://github.com/danmar/cppcheck/) has a specific addon to check for [MISRA C:2012](https://www.misra.org.uk/MISRAHome/MISRAC2012/tabid/196/Default.aspx) violations. See [current coverage](https://github.com/commaai/panda/blob/master/tests/misra/coverage_table).
* Compiler options are relatively strict: the flags `-Wall -Wextra -Wstrict-prototypes -Werror` are enforced.
* The [safety logic](https://github.com/commaai/panda/tree/master/board/safety) is tested and verified by [unit tests](https://github.com/commaai/panda/tree/master/tests/safety) for each supported car variant.
to ensure that the behavior remains unchanged.
* A hardware-in-the-loop test verifies panda's functionalities on all active panda variants, including:
  * additional afety model checks
  * compiling and flashing the bootstub and app code
  * receiving, sending, and forwarding CAN messages on all buses
  * CAN loopback and latency tests through USB and SPI

The above tests are themselves tested by:
* a [mutation test](tests/misra/test_mutation.py) on the MISRA coverage
* 100% line coverage enforced on the safety unit tests

In addition, we run the [ruff linter](https://github.com/astral-sh/ruff) and [mypy](https://mypy-lang.org/) on panda's Python library.

## Usage

Setup dependencies:
```bash
# Ubuntu
sudo apt-get install dfu-util gcc-arm-none-eabi python3-pip libffi-dev git

# macOS
brew install --cask gcc-arm-embedded
brew install python3 dfu-util gcc@13
```

Clone panda repository and install:
``` bash
git clone https://github.com/commaai/panda.git
cd panda

# install dependencies
pip install -r requirements.txt

# install panda
python setup.py install
```

See [the Panda class](https://github.com/commaai/panda/blob/master/python/__init__.py) for how to interact with the panda.

For example, to receive CAN messages:
``` python
>>> from panda import Panda
>>> panda = Panda()
>>> panda.can_recv()
```
And to send one on bus 0:
``` python
>>> panda.can_send(0x1aa, "message", 0)
```
Note that you may have to setup [udev rules](https://github.com/commaai/panda/tree/master/drivers/linux) for Linux, such as
``` bash
sudo tee /etc/udev/rules.d/11-panda.rules <<EOF
SUBSYSTEM=="usb", ATTRS{idVendor}=="bbaa", ATTRS{idProduct}=="ddcc", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="bbaa", ATTRS{idProduct}=="ddee", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

The panda jungle uses different udev rules. See [the repo](https://github.com/commaai/panda_jungle#udev-rules) for instructions. 

## Software interface support

As a universal car interface, it should support every reasonable software interface.

- [Python library](https://github.com/commaai/panda/tree/master/python)
- [C++ library](https://github.com/commaai/openpilot/tree/master/selfdrive/boardd)
- [socketcan in kernel](https://github.com/commaai/panda/tree/master/drivers/linux) (alpha)
- [Windows J2534](https://github.com/commaai/panda/tree/master/drivers/windows)

## Licensing

panda software is released under the MIT license unless otherwise specified.
