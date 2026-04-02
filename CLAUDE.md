# CLAUDE.md

## Project overview

Panda is an open-source car interface device by comma.ai. This repo contains:
- STM32H7 firmware (C, in `board/`)
- Python userspace library (`python/` and `board/jungle/__init__.py`)
- Tests (unit, protocol, MISRA static analysis, hardware-in-loop)

Adjacent repo **opendbc** lives at `../opendbc` and provides the safety model (`opendbc/safety/safety.h`), `CarParams`, and CAN packet definitions. The firmware includes it as `opendbc/safety/safety.h` and the SConscript uses `opendbc.INCLUDE_PATH`.

## Build

```bash
./setup.sh                                         # install deps into .venv (first time)
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/scons     # debug build (all targets)
RELEASE=1 CERT=board/certs/debug PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/scons  # release build
```

The `PATH` prefix is required on macOS so that `board/crypto/sign.py` (shebang: `#!/usr/bin/env python3`) resolves to the venv Python 3.12 (which has `pycryptodome`) rather than the system Python.

Firmware binaries are output to `board/obj/`. Each target builds a bootstub (RSA-2048 signed) and main app binary. `board/obj/gitversion.h` is auto-generated from git HEAD.

Compiler: `arm-none-eabi-gcc`, C standard: GNU11, flags include `-Wall -Wextra -Wstrict-prototypes -Werror -fmax-errors=1`.

## Tests

```bash
./test.sh          # scons + ruff check + pytest (non-HITL, non-MISRA)
pytest tests/      # same subset

pytest tests/misra/       # MISRA C:2012 static analysis via cppcheck
pytest tests/hitl/        # hardware-in-loop (requires physical device)
```

pytest config (pyproject.toml): `-nauto --maxprocesses=8`, excludes `tests/som/` and `tests/hitl/` by default.

## Code style

**C (board/):**
- MISRA C:2012 compliance enforced by cppcheck; see `tests/misra/coverage_table` for tracked suppressions
- 2-space indentation in most headers
- Suppress MISRA violations with inline comments: `// misra-c2012-17.7: ...`

**Python:**
- Ruff linter: `ruff check .` (line length 160, target Python 3.11)
- Enabled rule sets: E, F, W, PIE, C4, ISC, RUF100, A
- Ignored: W292, E741, E402, C408, ISC003

## Key directories

| Path | Contents |
|------|----------|
| `board/` | STM32 firmware |
| `board/drivers/` | HAL drivers (CAN, USB, SPI, UART, timers, …) |
| `board/stm32h7/` | STM32H7 linker scripts, startup, peripheral includes |
| `board/jungle/` | Jungle board firmware and Python library |
| `board/jungle/stm32h7/` | Jungle-specific STM32H7 drivers (SDMMC, SD replay) |
| `board/jungle/scripts/` | Jungle utility scripts (CAN printer, spam, provision SD) |
| `board/boards/` | Board variant hardware abstraction layers |
| `board/crypto/` | RSA/SHA implementation and firmware signing |
| `python/` | Core `Panda` class (USB + SPI transports) |
| `tests/libpanda/` | C unit tests via libpanda.so (compiled shared library) |
| `tests/hitl/` | Hardware-in-loop tests |
| `tests/misra/` | cppcheck MISRA analysis |
| `tests/usbprotocol/` | USB/CAN protocol tests |

## Jungle V2 SD card replay

The jungle V2 has an SD card slot that can replay openpilot CAN routes autonomously. See `board/jungle/README.md` for usage. Key files:

- `board/jungle/stm32h7/llsdmmc.h` — SDMMC driver (raw sector read/write via IDMA)
- `board/jungle/stm32h7/sd_replay.h` — replay state machine; call `sd_replay_tick()` from the main loop
- `board/jungle/scripts/provision_sd.py` — writes openpilot rlog → SD binary image
- USB commands: `0xa5` start/stop, `0xa6` status
- Python API: `PandaJungle.sd_replay_start/stop/status()`

## CANPacket_t

Defined in `../opendbc/opendbc/safety/can.h`:
```c
typedef struct {
  unsigned char fd : 1;
  unsigned char bus : 3;
  unsigned char data_len_code : 4;  // look up byte length with dlc_to_len[]
  unsigned char rejected : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;
  unsigned int  addr : 29;
  unsigned char checksum;
  unsigned char data[64];
} __attribute__((packed, aligned(4))) CANPacket_t;
```

Use `can_set_checksum()` before calling `can_send()`. Use `dlc_to_len[data_len_code]` to get byte length.

## USB control protocol (jungle)

Jungle-specific USB control requests are handled in `board/jungle/main_comms.h`. Assigned codes:

| Code | Direction | Description |
|------|-----------|-------------|
| 0xa0 | OUT | Set panda power (all channels) |
| 0xa1 | OUT | Set harness orientation |
| 0xa2 | OUT | Set ignition |
| 0xa3 | OUT | Set panda power (individual channel) |
| 0xa4 | OUT | Enable generated CAN traffic |
| 0xa5 | OUT | SD replay start (param1=1) / stop (param1=0) |
| 0xa6 | IN  | SD replay status (9 bytes: uint8 state, uint32 total, uint32 current) |
| 0xa8 | IN  | Microsecond timer (4 bytes) |
| 0xd2 | IN  | Jungle health packet |
| 0xde | OUT | Set CAN bitrate |
| 0xf5 | OUT | CAN silent mode |
| 0xf7 | OUT | Header pin enable/disable |
| 0xf9 | OUT | CAN-FD data bitrate |
