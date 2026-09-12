# SPI protocol v4

All requests and responses are 256 bytes, with one CS assertion per frame.
The host keeps one request outstanding and reads zero-filled frames until it
receives a valid response. There is no separate header acknowledgment.

```
Host                              Panda
request [256 bytes] -------------> validate and execute
poll    [256 bytes] -------------> preparing; no valid response
                                  arm reply + next request DMA
poll    [256 bytes] <------------- response [256 bytes]
request [256 bytes] -------------> next request
```

All multibyte integers are little-endian. Payloads are limited to 248 bytes,
keeping bootstub flash writes aligned to four bytes.

| Field | Request offset | Response offset |
| --- | --- | --- |
| Sync (`0x5a`) / status | 0 | 0 |
| Endpoint | 1 | — |
| Payload length, 2 bytes | 2 | 1 |
| Maximum response payload, 2 bytes | 4 | — |
| Payload | 6 | 3 |
| Zero padding | After payload | After payload |
| CRC8 | 255 | 255 |

CRC8 uses polynomial `0xd5` and initial value `0xff`, processing bytes 254 down
through 0. It covers the header, payload, and
padding. The response status is `0x85` for success, `0x1f` for a rejected request,
or `0x79` for CAN backpressure. A busy CAN write has no effects and can be retried.

Both DMA streams cover the response and the next request. Incoming dummy bytes
clocked during the response are discarded. Only completion of the next request
invokes the protocol handler; no TX-completion interrupt is needed to rearm RX.
If CS is active when a new exchange is prepared, enabling SPI is deferred until
CS rises. This allows early polls to be ignored as whole CS sessions.

The CS interrupt discards interrupted frames. RX always accepts a full frame,
including at startup; there are no special transfer sizes for discovery.

Discovery is a request to endpoint `0xff` with an empty payload and a response
capacity of 15 bytes. The response payload contains the 12-byte MCU UID, hardware
type, low byte of the USB product ID (`0xcc` for the app, `0xee` for the bootstub),
and protocol version (`4`). The host drains any pending response before discovery
so it can reconnect to an already-running device.

The app and bootstub both require a matching v4 client. Earlier panda SPI
protocols and the raw `VERSION` exchange are no longer supported.

This protocol preserves the existing retry semantics. It has no sequence numbers
or replay cache: retrying a request after losing its response can repeat a write
or consume additional CAN receive data.
