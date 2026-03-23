# Hardware ISO-TP Architecture

## Scope

This design is for one firmware-managed ISO-TP channel on top of classic CAN only.

It fits the existing panda architecture. However, the ISO-TP periodic hook cannot run from `tick_handler()` in [`board/main.c`](/home/willem/Clients/Ever/panda/board/main.c), because that path is only 8 Hz. V1 should add a dedicated `1 kHz` ISO-TP periodic hook and treat that as the finest timer resolution used by the ISO-TP engine.

The design below assumes:

- one global ISO-TP session for v1
- one TX byte ring for host -> firmware PDUs
- one RX byte ring for firmware -> host PDUs
- one active TX reassembly buffer
- one active RX reassembly buffer
- raw CAN USB/SPI traffic remains unchanged
- ISO-TP still goes through `safety_tx_hook()` / `safety_rx_hook()`
- no other firmware path manually transmits CAN frames on the configured ISO-TP TX arbitration ID while the ISO-TP engine is active
- once firmware queues an ISO-TP CAN frame for TX, it assumes the frame was sent immediately; this is acceptable for v1 on the OBD-II port where bus load is expected to stay low

## Recommended Hooks

Use these functions as the ISO-TP integration points:

```c
void isotp_periodic_handler(uint32_t now_us);
void isotp_rx_hook(const CANPacket_t *msg, uint32_t now_us);
```

Hooking:

- Call `isotp_rx_hook()` from [`board/drivers/fdcan.h`](/home/willem/Clients/Ever/panda/board/drivers/fdcan.h) after `safety_rx_hook()` succeeds and before the frame is pushed into `can_rx_q`.
- Call `isotp_periodic_handler()` from a higher-rate scheduler than the existing 8 Hz tick.

I would not call the ISO-TP RX hook on frames that failed `safety_rx_hook()`. Otherwise a safety-rejected flow control frame could still unblock the TX state machine.

To keep the implementation simple, both entry points should feed one shared internal engine instead of each carrying separate logic:

```c
static void isotp_kick(uint32_t now_us);
```

Recommended split:

- `isotp_rx_hook()`: parse a matching RX CAN frame, update RX/TX state, then call `isotp_kick()`
- `isotp_periodic_handler()`: handle timeouts and delayed releases, then call `isotp_kick()`

## Recommended Timing

V1 uses a dedicated `1 kHz` ISO-TP periodic hook. That is the finest timer resolution in this design.

Timing should be split like this:

- `isotp_rx_hook()`: RX assembly, FC handling, and immediate kick of TX progress after `CTS`
- `isotp_periodic_handler()`: TX timeout expiry and delayed-CF release when `STmin > 0`

My recommendation for v1 is:

- run the periodic handler at `1 kHz`
- support `STmin == 0` and millisecond `STmin` values correctly
- round any sub-millisecond `STmin` value up to `1 ms`

That keeps v1 simple while still handling the TX state machine correctly enough for low bus load on the OBD-II port.

## Session Model

Keep one session struct for v1:

```c
typedef struct {
  bool configured;             // True once bus, TX ID, and RX ID have all been set.
  uint8_t bus;                 // Panda bus number used for all ISO-TP TX/RX matching.

  uint32_t tx_id;              // Arbitration ID used for outgoing ISO-TP frames.
  uint32_t rx_id;              // Arbitration ID matched on incoming ISO-TP frames.
  bool tx_id_extended;         // True when tx_id is a 29-bit CAN identifier.
  bool rx_id_extended;         // True when rx_id is a 29-bit CAN identifier.

  bool tx_ext_addr_enabled;    // True when outgoing frames prepend a TX extended address byte.
  uint8_t tx_ext_addr;         // Extended address byte inserted ahead of the PCI on TX.
  bool rx_ext_addr_enabled;    // True when incoming frames must start with an RX extended address byte.
  uint8_t rx_ext_addr;         // Extended address byte expected ahead of the PCI on RX.
  uint16_t tx_message_timeout_ms;  // Max time for one TX wait step: TX completion or FC reception.
  uint16_t tx_transfer_timeout_ms; // Max time for one full ISO-TP transmit operation.

  struct {
    uint8_t buf[4095];         // Full host PDU currently being transmitted over ISO-TP.
    uint16_t len;              // Total number of valid bytes in tx.buf.
    uint16_t offset;           // Next payload byte index to transmit.
    uint8_t next_sn;           // Sequence number expected for the next CF we send.
    uint8_t block_size;        // Current FC-advertised block size, 0 meaning unlimited.
    uint8_t block_cf_sent;     // Number of CFs sent in the current block window.
    uint8_t wait_fc_count;     // Number of WAIT flow-control frames seen for this transfer.
    uint32_t stmin_us;         // Active separation time in microseconds between CFs.
    uint32_t deadline_us;      // Absolute timeout for the current TX wait step.
    uint32_t transfer_deadline_us; // Absolute timeout for the whole active TX transfer.
    uint32_t next_cf_us;       // Earliest timestamp when the next delayed CF may be sent.
    enum isotp_tx_state state; // Current transmit-side state machine state.
  } tx;

  struct {
    uint8_t buf[4095];         // Reassembly buffer for the ISO-TP PDU currently being received.
    uint16_t expected_len;     // Total PDU length decoded from the incoming FF.
    uint16_t offset;           // Number of payload bytes already copied into rx.buf.
    uint8_t next_sn;           // Sequence number expected on the next incoming CF.
    enum isotp_rx_state state; // Current receive-side state machine state.
  } rx;
} isotp_session_t;
```

`configured` becomes true once bus, TX ID, and RX ID are set. Extended addressing is optional.

TX timeouts are still worth keeping. Without them, a transmit step that never progresses or a peer that sends `WAIT` FC forever can leave TX stuck indefinitely. RX intentionally has no timeout handling in this design.

## Control Handlers

`ControlPacket_t` only gives you `request`, `param1`, `param2`, and `length`, so the cleanest v1 API is to expose each arbitration ID as one 32-bit packed value:

- bits `[28:0]`: arbitration ID
- bits `[30:29]`: reserved, must be zero
- bit `[31]`: `1 = 29-bit ID`, `0 = 11-bit ID`

On the wire inside `ControlPacket_t`:

- `param1`: packed arbitration ID bits `[15:0]`
- `param2`: packed arbitration ID bits `[31:16]`

I would add five new handlers and extend the existing comms reset:

| Request | Name | Purpose | Params |
| --- | --- | --- | --- |
| `0xc0` | `COMMS_RESET` | Reset ISO-TP comms state and buffers | none |
| `0xe1` | `ISOTP_SET_BUS` | Select panda CAN bus for the ISO-TP session | `param1[7:0] = bus` |
| `0xe2` | `ISOTP_SET_TX_ARB_ID` | Set packed TX arbitration ID | `param1/param2 = packed 32-bit ID` |
| `0xe3` | `ISOTP_SET_RX_ARB_ID` | Set packed RX arbitration ID | `param1/param2 = packed 32-bit ID` |
| `0xe4` | `ISOTP_SET_EXT_ADDR` | Configure TX/RX extended addressing bytes | `param1 = TX`, `param2 = RX` |
| `0xe9` | `ISOTP_SET_TX_TIMEOUT` | Configure TX message and transfer timeouts | `param1 = msg timeout ms`, `param2 = transfer timeout ms` |

### `0xc0` `COMMS_RESET`

Existing behavior stays, but also reset all ISO-TP comms state:

- clear ISO-TP TX bulk assembly buffer
- clear ISO-TP RX bulk assembly buffer
- clear ISO-TP TX ring
- clear ISO-TP RX ring
- abort active TX state
- abort active RX state

`0xc0` should be treated as an optional recovery/reset command, not a required part of normal host startup.

### `0xe1` `ISOTP_SET_BUS`

- `param1[7:0]`: panda bus number (`0..PANDA_CAN_CNT-1`)
- `param1[15:8]`: reserved, must be zero
- `param2`: reserved, must be zero
- `length`: zero

Semantics:

- stores the bus used for all ISO-TP TX and RX matching
- aborts any active ISO-TP transfer
- clears both ISO-TP rings

### `0xe2` `ISOTP_SET_TX_ARB_ID`

- `param1`: packed arbitration ID bits `[15:0]`
- `param2`: packed arbitration ID bits `[31:16]`
- `length`: zero

Validation:

- if bit 31 is clear, bits `[28:11]` must be zero
- bits `[30:29]` must be zero

Semantics:

- updates the ID used for all outgoing ISO-TP frames, including outgoing flow-control frames
- aborts active TX/RX state and clears both ISO-TP rings

### `0xe3` `ISOTP_SET_RX_ARB_ID`

Same layout as `0xe2`.

Semantics:

- updates the ID matched by `isotp_rx_hook()` for incoming data and incoming flow-control frames
- aborts active TX/RX state and clears both ISO-TP rings

### `0xe4` `ISOTP_SET_EXT_ADDR`

- `param1[7:0]`: TX extended address byte
- `param1[8]`: TX extended address enabled
- `param1[15:9]`: reserved
- `param2[7:0]`: RX extended address byte
- `param2[8]`: RX extended address enabled
- `param2[15:9]`: reserved
- `length`: zero

Semantics:

- when TX extended addressing is enabled, every outgoing ISO-TP frame starts with `tx_ext_addr`
- when RX extended addressing is enabled, incoming frames must start with `rx_ext_addr`
- aborts active TX/RX state and clears both ISO-TP rings

### `0xe9` `ISOTP_SET_TX_TIMEOUT`

- `param1`: per-message TX timeout in milliseconds
- `param2`: full-transfer TX timeout in milliseconds
- `length`: zero

Validation:

- both values must be non-zero

Semantics:

- updates `tx_message_timeout_ms`
- updates `tx_transfer_timeout_ms`
- does not clear rings
- does not abort the current TX transfer
- new values are applied the next time the TX state machine arms a deadline

## Bulk Transfers

Add one IN bulk stream and one OUT bulk stream for ISO-TP PDUs.

Recommended logical endpoints:

- `EP4 IN`: host reads completed ISO-TP PDUs from firmware RX ring
- `EP5 OUT`: host writes ISO-TP PDUs into firmware TX ring

For SPI, mirror the same logical endpoint numbers so USB and SPI stay aligned.

USB descriptor impact:

- add EP4 IN and EP5 OUT to both interface alt settings
- keep EP4 bulk in both alts
- keep EP5 bulk in both alts

## Ring Buffer Record Format

Use a length-prefixed message record, not fixed-size slots.

```c
typedef struct {
  uint16_t len;   // payload bytes after this header
  uint8_t value[];
} __attribute__((packed)) isotp_bulk_msg_t;
```

Rules:

- `len_le` is little-endian
- `len_le` must be `1..4095`
- total stored bytes per record are `2 + len_le`

### TX ring contents

`EP5 OUT` carries a byte stream of concatenated `isotp_bulk_msg_t` records.

Each record value is one full diagnostic PDU to transmit over ISO-TP using the configured session.

### RX ring contents

`EP4 IN` returns a byte stream of concatenated `isotp_bulk_msg_t` records.

Each record value is one fully reassembled ISO-TP PDU received from the configured peer.

## Bulk Read/Write Helpers

Mirror the existing CAN comms split:

```c
int comms_isotp_read(uint8_t *data, uint32_t max_len);
void comms_isotp_write(const uint8_t *data, uint32_t len);
void comms_isotp_reset(void);
```

Implementation shape:

- `comms_isotp_write()` uses an assembly buffer so an `isotp_bulk_msg_t` may span multiple USB/SPI bulk chunks
- only once a full record is assembled is it pushed atomically into the ISO-TP TX ring
- `comms_isotp_read()` pops bytes from the ISO-TP RX ring and uses a tail buffer exactly like `comms_can_read()` when a record spans multiple transport chunks

This avoids partial messages entering the session logic.

## TX State Machine

Use these states:

```c
enum isotp_tx_state {
  ISOTP_TX_IDLE = 0,
  ISOTP_TX_WAIT_FC,
  ISOTP_TX_WAIT_STMIN,
};
```

Transient actions such as "load next PDU", "send SF", "send FF", "send CF", and "abort" happen inside the periodic or RX handlers.

### TX idle path

When `tx.state == ISOTP_TX_IDLE`, `isotp_periodic_handler()`:

1. checks whether the session is configured
2. checks whether the TX ring contains a complete length-value record
3. pops the next PDU into `tx.buf`
4. arms `tx.transfer_deadline_us = now_us + (session.tx_transfer_timeout_ms * 1000U)`
5. sends either:
   - a single frame if it fits
   - a first frame if it does not

If the PDU fits in a single frame:

- queue the SF with `can_send()`
- if `can_send()` rejects it, abort TX as `TX_REJECTED`
- otherwise the ISO-TP transfer is complete immediately
- `tx.state` remains `ISOTP_TX_IDLE`
- no FC wait state is entered for an SF

Single-frame payload capacity:

- no extended address: 7 bytes
- extended address enabled: 6 bytes

First-frame payload capacity:

- no extended address: 6 bytes
- extended address enabled: 5 bytes

Classic CAN only:

- support 12-bit FF lengths only
- reject CAN-FD escape-length forms

### After first frame

After queueing an FF:

- `tx.offset` points just past the FF payload
- `tx.next_sn = 1`
- `tx.wait_fc_count = 0`
- `tx.deadline_us = now_us + (session.tx_message_timeout_ms * 1000U)`
- `tx.transfer_deadline_us = now_us + (session.tx_transfer_timeout_ms * 1000U)`
- `tx.state = ISOTP_TX_WAIT_FC`

### Handling flow control

`isotp_rx_hook()` handles matching FC frames while `tx.state == ISOTP_TX_WAIT_FC`.

Accepted FC frame checks:

- frame bus matches configured bus
- frame ID matches configured RX ID and RX ID type
- frame is not using returned/rejected path
- if RX extended addressing is enabled, byte 0 matches `rx_ext_addr`
- PCI type is `FlowControl`

FC actions:

- `CTS`:
  - decode block size
  - decode STmin
  - if decoded STmin is below `1 ms` and non-zero, round it up to `1 ms`
  - if STmin override exists later, it replaces the decoded STmin
  - `tx.block_cf_sent = 0`
  - `tx.next_cf_us = now_us`
  - `tx.state = ISOTP_TX_WAIT_STMIN`
  - call `isotp_kick(now_us)` so zero-`STmin` CF transmission can start immediately
- `WAIT`:
  - increment `tx.wait_fc_count`
  - if count exceeds `N_WFTmax`, abort TX
  - otherwise refresh `tx.deadline_us = now_us + (session.tx_message_timeout_ms * 1000U)`
- `OVERFLOW`:
  - abort TX immediately

### Sending consecutive frames

When `tx.state == ISOTP_TX_WAIT_STMIN`, `isotp_periodic_handler()`:

1. aborts if `now_us` passed the active timeout
2. returns early if `now_us < tx.next_cf_us`
3. sends CF frames while all are true:
   - `now_us >= tx.next_cf_us`
   - the session still has payload left
   - the CAN TX queue for the configured bus has room
   - the current block-size window is not exhausted

For each CF that is queued:

- build PCI byte `0x20 | next_sn`
- copy up to:
  - 7 payload bytes without extended address
  - 6 payload bytes with extended address
- call `can_send()`
- if `packet.rejected != 0`, abort TX as `TX_REJECTED`
- increments `offset`
- increments and wraps `next_sn` on 0x0F -> 0x00
- increments `block_cf_sent`

Then:

- if all bytes were sent: `tx.state = ISOTP_TX_IDLE`
- else if `block_size != 0` and `block_cf_sent >= block_size`:
  - `tx.deadline_us = now_us + (session.tx_message_timeout_ms * 1000U)`
  - `tx.state = ISOTP_TX_WAIT_FC`
- else if `tx.stmin_us == 0`:
  - continue sending CFs in the same periodic invocation, limited by queue space and block-size boundaries
- else:
  - `tx.next_cf_us = now_us + tx.stmin_us`
  - `tx.state = ISOTP_TX_WAIT_STMIN`

`STmin` is enforced from queue time rather than a transmit-complete timestamp. In v1, any sub-millisecond non-zero `STmin` is rounded up to `1 ms`. 

## RX State Machine

Use these states:

```c
enum isotp_rx_state {
  ISOTP_RX_IDLE = 0,
  ISOTP_RX_WAIT_CF,
};
```

### RX filtering

`isotp_rx_hook()` should ignore frames unless all are true:

- bus matches configured bus
- ID matches configured RX ID and RX ID type
- if RX extended addressing is enabled, byte 0 matches `rx_ext_addr`
- frame passed `safety_rx_hook()`

After filtering:

- strip the RX extended address byte if enabled
- inspect the PCI type

### Single frame

In `ISOTP_RX_IDLE`:

- parse SF length from low nibble
- reject length `0`
- reject if claimed length exceeds actual CAN payload
- push one length-value record into the ISO-TP RX ring
- stay in `ISOTP_RX_IDLE`

### First frame

In `ISOTP_RX_IDLE`:

- parse 12-bit total length
- reject length `<= single-frame capacity`
- copy FF payload into `rx.buf`
- set:
  - `rx.expected_len`
  - `rx.offset`
  - `rx.next_sn = 1`
  - `rx.state = ISOTP_RX_WAIT_CF`
- send FC `CTS`

Default FC payload to send in v1:

- flow status = `CTS`
- block size = `0` for unlimited block
- STmin = `0`

The RX side always accepts unlimited consecutive frames in v1. That keeps the receiver simple. Padding and STmin override can be added later.

### Consecutive frames

In `ISOTP_RX_WAIT_CF`:

- reject frames whose SN does not match `rx.next_sn`
- append up to the remaining number of bytes
- increment and wrap `rx.next_sn`

After append:

- if `rx.offset >= rx.expected_len`:
  - push a completed length-value record into the ISO-TP RX ring
  - return to `ISOTP_RX_IDLE`

### Unexpected frames

Recommended behavior:

- unexpected CF in `ISOTP_RX_IDLE`: ignore
- new FF while already in `ISOTP_RX_WAIT_CF`: reset the current RX transfer and start again from that FF
- unexpected SF while already in `ISOTP_RX_WAIT_CF`: ignore
- FC frames received while RX is active: ignore unless TX is waiting for FC

## Periodic Handler Logic

`isotp_periodic_handler()` should do three things:

1. expire TX step timeouts
2. expire full-transfer TX timeouts
3. start a new TX when idle
4. release a delayed CF when `STmin` expires

Pseudo-flow:

```c
void isotp_periodic_handler(uint32_t now_us) {
  if (!session.configured) return;

  if ((session.tx.state != ISOTP_TX_IDLE) &&
      timeout_expired(now_us, session.tx.transfer_deadline_us)) {
    isotp_abort_tx(ISOTP_ERR_TIMEOUT_TRANSFER);
  }

  if ((session.tx.state == ISOTP_TX_WAIT_FC) &&
      timeout_expired(now_us, session.tx.deadline_us)) {
    isotp_abort_tx(ISOTP_ERR_TIMEOUT_MESSAGE);
  }

  if (session.tx.state == ISOTP_TX_IDLE) {
    isotp_try_start_next_tx(now_us);
  }

  if ((session.tx.state == ISOTP_TX_WAIT_STMIN) &&
      time_reached(now_us, session.tx.next_cf_us)) {
    isotp_try_send_consecutive_frames(now_us);
  }
}
```

Without `isotp_tx_hook()`, the periodic handler is responsible for draining zero-`STmin` CF bursts when allowed by queue space and block-size boundaries.

## RX Hook Logic

Pseudo-flow:

```c
void isotp_rx_hook(const CANPacket_t *msg, uint32_t now_us) {
  if (!session.configured) return;
  if (!is_matching_isotp_frame(msg)) return;

  parsed = strip_ext_addr_and_parse_pci(msg);

  if ((parsed.type == FLOW_CONTROL) &&
      (session.tx.state == ISOTP_TX_WAIT_FC)) {
    isotp_handle_fc(parsed, now_us);
    return;
  }

  switch (session.rx.state) {
    case ISOTP_RX_IDLE:
      if (parsed.type == SINGLE) isotp_handle_sf(parsed);
      else if (parsed.type == FIRST) isotp_handle_ff(parsed, now_us);
      break;
    case ISOTP_RX_WAIT_CF:
      if (parsed.type == CONSECUTIVE) isotp_handle_cf(parsed, now_us);
      else if (parsed.type == FIRST) {
        isotp_abort_rx(ISOTP_ERR_RESTARTED);
        isotp_handle_ff(parsed, now_us);
      }
      break;
  }
}
```

## Defaults and Constants

Recommended defaults for v1:

- `ISOTP_MSG_MAX_LEN = 4095`
- `ISOTP_N_WFTMAX = 10`
- `ISOTP_DEFAULT_RX_STMIN = 0`
- `ISOTP_DEFAULT_TX_MESSAGE_TIMEOUT_MS = 1000`
- `ISOTP_DEFAULT_TX_TRANSFER_TIMEOUT_MS = 10000`
- session starts with `configured = false`
- TX and RX extended addressing start disabled
- TX and RX state machines start in `IDLE`
- ISO-TP TX/RX rings and bulk assembly buffers start empty

With those defaults, the host does not need to send `0xc0` before first use. The session comes up in a sane inert state and only becomes active once bus and IDs are configured.

Those timeout defaults are engineering choices, not protocol quotes. They are conservative starting points that avoid getting stuck on a dead peer or endless `WAIT` FC traffic.

## Implementation Notes

- Keep raw CAN and hardware ISO-TP separate. The ISO-TP engine should observe raw CAN traffic, but not consume or suppress it.
- Outgoing ISO-TP frames should still use `can_send()`, so safety rules remain in force.
- Treat `packet.rejected != 0` after `can_send()` as a hard TX error.
- Push completed RX PDUs into the ISO-TP RX ring only after full reassembly.
- Do not push partial RX state into the bulk ring.
- Reconfiguration should be destructive in v1: any change to bus, TX ID, RX ID, or ext address aborts active transfers and clears queued ISO-TP PDUs.

## Minimal Host Flow

1. Send `0xe1` to set the bus
2. Send `0xe2` to set TX ID
3. Send `0xe3` to set RX ID
4. Send `0xe4` to set TX/RX extended address bytes
5. Send `0xe9` to set TX message and transfer timeouts
6. Write one or more length-value records to ISO-TP OUT bulk
7. Read completed length-value records from ISO-TP IN bulk

`0xc0` remains available if the host wants to forcibly clear queued ISO-TP state after transport errors or a session reset.

## Open Items For Later

- configurable padding policy
- STmin override
- transmit-complete based pacing instead of queue-time based pacing
- multiple concurrent ISO-TP sessions
- CAN-FD support
