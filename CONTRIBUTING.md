# Contributing to panda development

As root:
  `apt-get update`
  `apt-get install git make sudo`

As non-root:
  `git clone https://github.com/commaai/panda.git`
  `cd panda/board`
  `sudo ./get_sdk.sh`
  `sudo make`
  `cd ../boardesp`
  `./get_sdk.sh`
  `sudo make`
