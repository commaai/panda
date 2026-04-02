# Plan: Jungle V2 SD Card CAN Replay (Issue #1981)

## Context

The panda jungle v2 has an SD card slot that is currently unused. Developers who want to replay openpilot CAN routes must run a separate Python host script (`can_replay.py`) that streams messages over USB. The goal is to support fully autonomous replay from the SD card so the jungle can replay routes without a host PC, using openpilot route data written to the card in advance.

The upstream `sdmmc_jungle` branch already has a working SDMMC driver (`llsdmmc.h`) with read/write and IDMA support. This plan ports that driver to the current branch and builds the replay system on top of it.

---

## Architecture

```
Python provisioning script  →  SD card (raw binary format)
                                       ↓
                           Jungle V2 firmware reads SD card
                           in main loop, sends CAN at correct time
```

---

## 1. SD Card Binary Format

Raw sectors, no filesystem. Simple and easy to provision.

**Sector 0 (header, 512 bytes):**
```c
struct __attribute__((packed)) sd_replay_header_t {
  uint8_t  magic[8];      // "PNDREPLY"
  uint32_t num_records;   // total number of CAN records
  uint32_t record_size;   // must equal 20
  uint32_t format_version; // = 1
  // rest of sector is zero-padded
};
```

**Sectors 1+ (records, 20 bytes each):**
```c
struct __attribute__((packed)) sd_can_record_t {
  uint32_t mono_time_us;  // microseconds since replay start (relative)
  uint32_t addr;          // CAN address (standard or extended)
  uint8_t  bus;           // 0-2
  uint8_t  len;           // data byte length (0-8)
  uint8_t  data[8];       // CAN data (right-padded with zeros if len < 8)
  uint16_t pad;           // alignment padding to 20 bytes
};
```

Records are stored sorted by `mono_time_us` ascending. 20 bytes × records_per_buffer = 819 records per 16 KB IDMA buffer.

---

## 2. Firmware Changes

### 2a. Port SDMMC driver

**New file:** `board/jungle/stm32h7/llsdmmc.h`

Copy from `upstream/sdmmc_jungle:board/jungle/stm32h7/llsdmmc.h`. This file provides:
- `sdmmc_reset()` — card init/detect
- `sdmmc_read_idma(buf, sector, count)` — fast DMA reads
- `sdmmc_write_idma(buf, sector, count)` — fast DMA writes
- `gpio_sdmmc_init()` — configure SDMMC GPIO pins

**Modify:** `board/jungle/main.c`

Add after other includes:
```c
#ifdef STM32H7
  #include "board/jungle/stm32h7/llsdmmc.h"
#endif
```

### 2b. SD replay state machine

**New file:** `board/jungle/stm32h7/sd_replay.h`

State machine integrated into the main loop:

```c
typedef enum {
  SD_REPLAY_IDLE    = 0,
  SD_REPLAY_ACTIVE  = 1,
  SD_REPLAY_DONE    = 2,
  SD_REPLAY_ERROR   = 3,
} sd_replay_state_t;

extern sd_replay_state_t sd_replay_state;
extern uint32_t sd_replay_total_records;
extern uint32_t sd_replay_current_record;

void sd_replay_init(void);
void sd_replay_start(void);
void sd_replay_stop(void);
void sd_replay_tick(void);   // called from main loop
```

**sd_replay_tick() logic:**
1. If state != ACTIVE, return.
2. If internal buffer is empty, read next 16KB chunk from SD card via `sdmmc_read_idma()` into `idmabuf`.
3. Process records from buffer: for each record, check `(microsecond_timer_get() - replay_start_us) >= record.mono_time_us`.
4. When timing matches, build a `CANPacket_t` from the record and call `can_send()`.
5. Advance buffer pointer; request next SD read when buffer runs low.
6. When `sd_replay_current_record >= sd_replay_total_records`, set state to `SD_REPLAY_DONE`.

**Modify `board/jungle/main.c` — main loop:**

Replace the `generated_can_traffic` block in the `for(cnt=0;;cnt++)` loop:

```c
if (sd_replay_state == SD_REPLAY_ACTIVE) {
  sd_replay_tick();
  continue;
}
```

Add SD init after `can_init_all()`:
```c
#ifdef STM32H7
  gpio_sdmmc_init();
  if (sdmmc_reset() == sd_err_ok) {
    sd_replay_init();
  }
#endif
```

### 2c. New USB control commands

**Modify `board/jungle/main_comms.h`** — add to `comms_control_handler()`:

```c
// **** 0xa5: SD replay control (param1: 0=stop, 1=start)
case 0xa5:
  if (req->param1 == 1U) {
    sd_replay_start();
  } else {
    sd_replay_stop();
  }
  break;

// **** 0xa6: Get SD replay status
case 0xa6: {
  struct {
    uint8_t  state;         // sd_replay_state_t
    uint32_t total_records;
    uint32_t current_record;
  } __attribute__((packed)) status;
  status.state = (uint8_t)sd_replay_state;
  status.total_records = sd_replay_total_records;
  status.current_record = sd_replay_current_record;
  memcpy(resp, &status, sizeof(status));
  resp_len = sizeof(status);
  break;
}
```

---

## 3. Python Library Changes

**Modify `board/jungle/__init__.py`** — add methods to `PandaJungle`:

```python
def sd_replay_start(self):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xa5, 1, 0, b'')

def sd_replay_stop(self):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xa5, 0, 0, b'')

def sd_replay_status(self):
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xa6, 0, 0, 9)
    state, total, current = struct.unpack("<BII", dat)
    return {"state": state, "total_records": total, "current_record": current}
```

---

## 4. Provisioning Script

**New file:** `board/jungle/scripts/provision_sd.py`

```
Usage:
  provision_sd.py <rlog_or_qlog> <sd_device_or_output_file>

Examples:
  provision_sd.py /data/media/0/realdata/abc123--0/0/rlog /dev/sdb
  provision_sd.py route.rlog replay.bin
```

**Implementation steps:**
1. Parse CAN messages from openpilot rlog using `cereal`/`openpilot.tools.lib.logreader.LogReader`
2. Extract `(mono_time_us, addr, bus, data)` tuples from `sendcan` events
3. Normalize timestamps to start at 0 (subtract first message time)
4. Write header to sector 0 (512 bytes)
5. Write records in 20-byte structs starting at sector 1
6. Write to block device directly (`open(dev, 'wb')`) or to a binary file

The script should print a summary: total records written, time span, estimated replay duration.

---

## Critical Files

| File | Action |
|------|--------|
| `board/jungle/stm32h7/llsdmmc.h` | **Create** — copy from `upstream/sdmmc_jungle` |
| `board/jungle/stm32h7/sd_replay.h` | **Create** — new replay state machine |
| `board/jungle/main.c` | **Modify** — add SD init + replay tick in main loop |
| `board/jungle/main_comms.h` | **Modify** — add 0xa5/0xa6 USB commands |
| `board/jungle/__init__.py` | **Modify** — add Python API methods |
| `board/jungle/scripts/provision_sd.py` | **Create** — provisioning script |

---

## Verification

1. **Build firmware**: `scons board=jungle` — verify compiles without errors
2. **Flash & basic SD test**: insert SD card, check serial debug output for "SDMMC init done"
3. **Provision SD card**: `python provision_sd.py <rlog> /dev/sdX` → verify header + records written
4. **Replay test**: Start replay via Python, verify CAN messages appear on a panda CAN sniffer
5. **Timing test**: Measure timestamps of received CAN messages, verify relative timing matches original rlog within acceptable tolerance (±5ms)
6. **Status polling**: Call `sd_replay_status()` during replay, verify current_record increments and reaches total_records at completion
