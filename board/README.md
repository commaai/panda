## Programming

```
./flash.py        # flash application
./recover.py      # flash bootstub
```

## Debugging

To print out the serial console from the STM32, run `tests/debug_console.py`

Troubleshooting
----

If your panda will not flash and green LED is on, use `recover.py`.
If panda is blinking fast with green LED, use `flash.py`.

Otherwise if LED is off and panda can't be seen with `lsusb` command, use [panda paw](https://comma.ai/shop/products/panda-paw) to go into DFU mode.

If your device has an internal panda and none of the above works, try running `../scripts/reflash_internal_panda.py`.

## Source organization

Implementations live in separately compiled `.c` files. Shared headers follow subsystem
boundaries: `drivers/drivers.h`, `sys/sys.h`, `stm32h7/stm32h7.h`, and
`boards/boards.h`. Keep private helpers and constants in their source files; add
shared declarations to the relevant subsystem header instead of creating a header
for every source file. `config.h` contains build configuration and platform types.

The root `SConscript` selects sources for panda, jungle, body, and their bootstubs.
Application and bootstub objects are compiled separately so their build flags remain
isolated. Run `./test.sh` from the repository root to build all variants and run the
local tests, including firmware linkage checks and MISRA mutation tests.
