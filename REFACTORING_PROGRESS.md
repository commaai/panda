# Panda Firmware Refactoring Status

## Objective
Migrate from Unity Build (header-only) to standard Declaration (.h) and Definition (.c) architecture to improve maintainability and comply with MISRA-C.

## Current Progress (2026-04-03)

### Completed:
- [x] **SConscript Core**: Fixed "Two environments" error by isolating objects in `board/obj/`.
- [x] **System Config**: Restored missing macros and fixed include order in `stm32h7_config.h`.
- [x] **Driver Migration**: All drivers (`gpio`, `spi`, `uart`, `fdcan`, `can_common`, `can_comms`, `usb`, `pwm`, `led`, `timers`, `lladc`, `llfan`, `clock_source`) moved to `.c` files.
- [x] **Global State**: Global variables consolidated in `main_definitions.c` and shared via `main_declarations.h`.
- [x] **Safety Logic**: Isolated `safety.h` definitions in `safety_definitions.c` and created `safety_mode_wrapper.c` for app-level logic.
- [x] **Board Support**: `red.h`, `tres.h`, `cuatro.h`, and `board_v2.h` refactored into `.h` + `.c` pairs.
- [x] **Bootstub Minimalist**: Refined `bs_srcs` to include only essential drivers, ensuring bootloader fits in its sector.
- [x] **Test Integrity**: Updated `tests/libpanda/` to link against real drivers, verified with `usbprotocol` tests.

### Final Status:
- **Build**: Success for `panda_h7`, `body_h7`, `panda_jungle_h7` (app and bootstub).
- **Tests**: `pytest tests/usbprotocol/` passed (6/6).
- **Style**: `ruff check .` passed.

## Critical Notes
- **Include Order**: `stm32h7xx.h` must come before `driver_declarations.h` to provide hardware types.
- **Packed Structs**: `can_health_t` and `health_t` must use `__attribute__((packed))` to maintain layout compatibility between ARM and x86 tests.
- **Externs**: Always use `extern` in headers for global state to prevent multiple definition errors.
