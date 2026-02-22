let's vendor the gcc-arm-none-eabi dependency inside the repo. for the structure of teh vendoring, copy openpilot's third_party/ top level folder (see
  .context/openpilot/).
* the goal is to remove gcc-arm-none-eabi from setup.sh and instead check-in our own copy
* the checked-in size should be minimal
* this means remove anything we dont' use, disable unused options, etc.
* make sure to copy the structure and build.sh style from openpilot
* this needs to support linux x86+arm and mac arm (no intel mac)
* commit, push, and check CI on this branch until it passes
* you're done when CI is green and the goal is achieved
