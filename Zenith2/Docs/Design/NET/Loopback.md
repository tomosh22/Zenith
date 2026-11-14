# NET design — the Stage 3 loopback proof

This file is part of the NET design family. The entry document is `Zenith2/Docs/Design/NET.md`. It specifies the Stage 3 proof of `Zenith2/Docs/Architecture.md` → "The Stage 3 loopback proof": the datagram ports, the transport, wire format version 1 and every message, the session state machines, the input path from a client's phase 1 to the server's phase 1, the snapshot path from the server's phase 11 to the client's proxies, the proof assertions, the loopback content and the integration suite. Every mechanism here is marked a Stage 3 foundation ([Z2-N-ARCH-787]); Stage 7 supersedes the transport, the snapshot format and possession with `Zenith2/Docs/Design/NET/Stage7.md`, and keeps the foundations of `Zenith2/Docs/Design/NET/Foundations.md` unchanged.

## Scope of this file

| Topic | Heading |
|---|---|
| ports and the transport | "Transport over datagram ports" |
| bytes on the wire | "Wire format version 1" |
| connection lifecycle | "Session state machines" |
| commands, client to server | "Input path" |
| state, server to client | "Snapshot path" |
| in-process assertions | "Proof mode" |
| game content of the proof | "Loopback content" |
| the integration suite | "The loopback suite" |

## Transport over datagram ports

### Production port over PLAT sockets

<!-- assumption: pending PLAT -->
PLAT supplies an unconnected UDP socket in a new public header (row NT1); NET assumes exactly this shape and adapts it to `Zenith_NetDatagramPort` in `Internal/Zenith_NetPlatformPort.cpp`:

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_DatagramSocket.h (pending PLAT)
struct Zenith_DatagramSocketDesc
{
	uint32 m_uBindIPv4 = 0x7F000001u;     // host byte order
	uint16 m_uBindPort = 0;               // 0 = ephemeral
	bool m_bLoopbackOnly = true;           // SendTo refuses a destination outside 127.0.0.0/8
	uint32 m_uSendBufferBytes = 1048576;  // SO_SNDBUF requested
	uint32 m_uReceiveBufferBytes = 1048576;
};
enum Zenith_DatagramSocketErrorCode : uint8
{
	DATAGRAM_SOCKET_ERROR_BIND_FAILED = 1, DATAGRAM_SOCKET_ERROR_ADDRESS_REFUSED, DATAGRAM_SOCKET_ERROR_WOULD_BLOCK,
	DATAGRAM_SOCKET_ERROR_MESSAGE_TOO_LARGE, DATAGRAM_SOCKET_ERROR_OS, DATAGRAM_SOCKET_ERROR_OUT_OF_MEMORY,
};
struct Zenith_DatagramSocketError { Zenith_DatagramSocketErrorCode m_eCode; uint8 m_auReserved[3]; uint32 m_uOsCode; };
class Zenith_DatagramSocket
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DatagramSocket>, Zenith_DatagramSocketError> Open(
		const Zenith_DatagramSocketDesc& xDesc, Zenith_Heap& xHeap);          // non-blocking, SO_REUSEADDR off
	uint16 GetLocalPort() const;
	Zenith_Result<void, Zenith_DatagramSocketError> SendTo(uint32 uIPv4, uint16 uPort, Zenith_ByteSpan xBytes);
	// WOULD_BLOCK when nothing is queued; MESSAGE_TOO_LARGE when the datagram exceeded xBuffer (truncated, discarded).
	Zenith_Result<uint32, Zenith_DatagramSocketError> ReceiveFrom(Zenith_MutableByteSpan xBuffer, uint32& uIPv4Out,
		uint16& uPortOut);
};
```

`OWNER_THREAD`, Windows through Winsock (`WSAStartup` in `Open`, balanced at destruction, `WSAECONNRESET` from an ICMP port-unreachable ignored with `SIO_UDP_CONNRESET` off), Android and Linux through BSD sockets; the implementation lives under `ZenithPlatform/Internal/<Platform>/`, so no NET file includes a socket header (rule F18). Unlike `Zenith_DevSocket.h` it is built into Shipping, because Stage 7 ships multiplayer; NET's own Stage 3 Shipping guard keeps it unused there (`Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles").
<!-- /assumption -->

The adapter maps errors one to one (`BIND_FAILED` → `NET_ERROR_BIND_FAILED`, `ADDRESS_REFUSED` → `NET_ERROR_ADDRESS_REFUSED`, `WOULD_BLOCK` → `NET_ERROR_WOULD_BLOCK`, `MESSAGE_TOO_LARGE` → `NET_ERROR_LIMIT`, `OS` → `NET_ERROR_SOCKET` with the OS code, `OUT_OF_MEMORY` → `NET_ERROR_OUT_OF_MEMORY`), always opens with `m_bLoopbackOnly = true` at Stage 3, and refuses at open any `net.bindAddress` other than `127.0.0.1` with `NET_ERROR_ADDRESS_REFUSED`.

### Memory port

`Zenith_CreateNetMemoryPortPair` (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetDatagramPort header") connects two in-process transports; ZenithNet's unit tests drive a server transport and a client transport through one pair. It is a production implementation of the port interface (ruling FINAL-DOUBLES) with the same contract as the platform port, so every session test runs the code the loopback runs.

### Pump and send discipline

| When | Thread | Transport work |
|---|---|---|
| frame-prologue step 1, `Zenith_NetModuleInstance::OnPlatformEvents` (zone `Net.Pump`) | main | `Pump`: first compare each session world's restore mark with the transport's last-seen mark and, when it moved, end the live session (§ "Client session states", § "Server peer states") before any datagram is dispatched, so no INPUT enters a window the restore cleared; then receive up to `uNET_MAX_DATAGRAMS_PER_PUMP` (512) datagrams, validate and dispatch each in arrival order (a 513th waits for the next frame); run timers (retries, timeouts, keepalives); send the handshake datagrams (REQUEST, ACCEPT, REJECT), a DISCONNECT and a client's digest-only INPUT directly; allocate and park publications |
| `Net.Send`, the module's engine-wide EPILOGUE stage after every `Net.Collect@W<tag>` | a frame worker | `Flush`: encode and send the frame's queued messages, at most `uNET_MAX_DATAGRAMS_PER_SEND` (512); further datagrams wait for the next frame and are counted in `net.send.deferred` |
| shutdown, `~Zenith_NetModuleInstance` | main | DISCONNECT to every peer, three times 10 ms apart, then `Close` |

The two frame sites never overlap: the prologue completes before `RunFrame` starts and `Net.Send` completes before `RunFrame` returns. Under `--replay` and in `NET_ROLE_OFFLINE` no transport exists and neither site does anything. The receive buffer is one 1,200-byte array owned by the transport; a datagram is fully decoded before the next `Receive`.

## Wire format version 1

### Byte conventions

Every integer is little-endian and written field by field; signed integers are two's complement; every reserved byte is written 0 and must read 0, and a non-zero reserved byte makes the packet malformed. There is no padding and no alignment. A ULEB128 field is § "Unsigned LEB128".

### Packet header

| Offset | Type | Field | Rule |
|---|---|---|---|
| 0 | `uint32` | magic | `uNET_PACKET_MAGIC` (`0x31504E5A`, bytes `5A 4E 50 31`, "ZNP1") |
| 4 | `uint16` | protocol version | `uNET_PROTOCOL_VERSION` (1) |
| 6 | `uint8` | message kind | `Zenith_NetMessageKind` 1 … 7 |
| 7 | `uint8` | flags | 0 |
| 8 | `uint64` | session ID | 0 in CONNECT_REQUEST and in a CONNECT_REJECT; the session's ID otherwise |
| 16 | `uint32` | packet sequence | per sender and session, from 1, incremented by 1 per datagram |
| 20 | — | message body | per kind |
| 20 + n | `uint32` | CRC-32C | of bytes 0 … 19 + n (`Zenith2/Docs/Design/SAVE/Formats.md` → "CRC-32C", `Zenith_SaveCrc32c`) |

A datagram is 24 … 1,200 bytes; the body is at most `uNET_MAX_MESSAGE_BYTES` (1,176).

### Packet integrity

The CRC-32C trailer rejects corruption that the UDP checksum lets through and makes fuzzed input cheap to discard; it authenticates nothing (Stage 3 binds the loopback interface alone; Stage 7 adds authenticated sessions, `Zenith2/Docs/Design/NET/Stage7.md` → "Session security"). The sequence and the 64-packet sliding window of § "Packet validation order" reject duplicates and replays inside a session.

### Message kinds

| Kind | Value | Direction | Body bytes | Reliability at Stage 3 |
|---|---|---|---|---|
| `CONNECT_REQUEST` | 1 | client → server | 64 | repeated every `net.retryIntervalMs` until answered |
| `CONNECT_ACCEPT` | 2 | server → client | 32 | sent in answer to every request of the accepted nonce |
| `CONNECT_REJECT` | 3 | server → client | 16 | sent in answer to every rejected request |
| `INPUT` | 4 | client → server | 12 … 1,176 | unreliable; redundancy of the previous `net.redundantInputTicks` blocks |
| `SNAPSHOT` | 5 | server → client | 20 … 1,020 | unreliable; each fragment self-contained |
| `KEEPALIVE` | 6 | both | 8 | unreliable |
| `DISCONNECT` | 7 | both | 8 | sent three times, 10 ms apart |

### CONNECT_REQUEST

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | client nonce: 8 bytes from `Zenith_Platform::FillOSRandom` (row NT1 (c)), never 0 (redrawn when 0) |
| 8 | 16 bytes | build ID (`Zenith_GetRunningBuildID`, `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header") |
| 24 | `uint64` | composition hash (`Zenith_GameComposition::m_ulCompositionHash`) |
| 32 | `uint64` | content hash (the cooked-content hash, as in the `ZRPL` header) |
| 40 | `uint32` | `sim.tickHz` |
| 44 | `uint8` | ECS backend (`Zenith_BackendID`) |
| 45 | `uint8` | physics backend (`Zenith_BackendID`, 0 when no physics module) |
| 46 | `uint16` | reserved |
| 48 | `uint64` | replication table hash (`Zenith_NetReplicationTable::GetHash`) |
| 56 | `uint64` | proxy table hash (§ "Proxy prefab table") |

### CONNECT_ACCEPT

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | the client nonce being answered |
| 8 | `uint16` | the client's peer index (1 … 65,534) |
| 10 | `uint16` | input delay ticks (the server's `net.inputDelayTicks`) |
| 12 | `uint16` | snapshot interval ticks (`net.snapshotIntervalTicks`) |
| 14 | `uint16` | reserved |
| 16 | `uint64` | the server's last completed tick when the accept was queued |
| 24 | `uint64` | reserved |

The packet header carries the new session ID: 8 bytes from `FillOSRandom`, never 0.

### CONNECT_REJECT

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | the client nonce being answered (0 when the request was too malformed to read it) |
| 8 | `uint8` | `Zenith_NetRejectReason` (wire values 1 … 8 only) |
| 9 | `uint8` | detail: for `NET_REJECT_REASON_IDENTITY` the `Zenith_ReplayIdentityField` value that differed first (build ID, composition, content, ECS backend, physics backend, tick rate, in that order); 0 otherwise |
| 10 | `uint16` | reserved |
| 12 | `uint32` | reserved |

### INPUT

| Offset | Type | Field |
|---|---|---|
| 0 | `uint32` | cumulative digests reported: the number of distinct complete ticks the client has applied and listed in any INPUT of this session, including this one |
| 4 | `uint8` | digest pair count d, 0 … 4 |
| 5 | `uint8` | block count b, 0 … 4 |
| 6 | `uint16` | reserved |
| 8 | `uint32` | session base sequence B: the first input-command sequence the client sends in this session (§ "Client command capture") |
| 12 | d pairs | each: `uint64` server tick; `uint32` digest (CRC-32C of that tick's concatenated fragment bodies in fragment-index order); 12 bytes; ascending tick |
| 12 + 12d | b blocks | each: `uint64` target server tick; `uint16` command count c; `uint16` reserved; then c commands of 18 bytes (§ "Command wire encoding" of `Zenith2/Docs/Design/NET/Foundations.md`) |

Blocks are in ascending target tick; a block's commands are in ascending sequence; the total command count over all blocks is at most `uNET_MAX_COMMANDS_PER_DATAGRAM` (64), and the body fits 1,176 bytes: with no digest pair one block holds the full 64 (12 + 12 + 64 × 18 = 1,176), with four pairs one block holds ⌊(1,176 − 12 − 48 − 12) / 18⌋ = 61, and four pairs with four blocks leave room for 59. Every command's player slot is 0 at Stage 3.

### SNAPSHOT

The SNAPSHOT body is one fragment; it is also, behind the 4-byte publication prefix, the payload the client publishes (§ "Fragment publication"), so it is at most `uNET_MAX_FRAGMENT_BODY_BYTES` (1,020).

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | server tick of the capture |
| 8 | `uint8` | fragment index, 0 … count − 1 |
| 9 | `uint8` | fragment count, 1 … 32 |
| 10 | `uint16` | entity record count r |
| 12 | `uint16` | destroy count d, 0 … 64 |
| 14 | `uint16` | reserved |
| 16 | `uint32` | the low 32 bits of the replication table hash |
| 20 | d × ULEB128 | destroyed network IDs, ascending |
| … | r records | entity records, ascending network ID |

Entity record:

| Field | Encoding | Rule |
|---|---|---|
| network ID | ULEB128, 1 … 10 bytes | non-zero; strictly greater than the previous record's in the fragment |
| flags | `uint8` | bit 0: an owner peer follows; bits 1–7 zero |
| proxy index | `uint8` | 0 … proxy table count − 1 |
| owner peer | `uint16` | present when flags bit 0 is set (the owner is not 0) |
| presence mask | `uint32` | bit i set: replication table entry i's component is present on the entity and its value follows; bits at or above the table's entry count zero |
| values | per present entry, in table order | `NET_CODEC_POSITION_MM`: three `int32` (x, y, z) millimetres, 12 bytes; `NET_CODEC_QUAT_SMALLEST_THREE`: one `uint32`, § "Quantisation" |

### KEEPALIVE

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | the sender's session world's last completed tick |

### DISCONNECT

| Offset | Type | Field |
|---|---|---|
| 0 | `uint8` | `Zenith_NetDisconnectReason` |
| 1 | 7 bytes | reserved |

### Unsigned LEB128

A value is written 7 bits at a time, least significant group first, bit 7 set on every byte but the last. A reader accepts at most 10 bytes, refuses a 10th byte whose value exceeds 1 (a value above 2^64 − 1) and refuses an over-long encoding (a final byte 0x00 after the first), so every value has exactly one encoding and a fragment's bytes are a function of its content. `Test_NetWire_ULEB128RoundTripAndCanonical` covers 0, 1, 127, 128, 2^14 − 1, 2^14, 2^48 − 1, 2^63, 2^64 − 1, every over-long form of 0 and 1, an 11-byte input and a 10th byte of 2.

### Wire limits

| Limit | Value | Constant |
|---|---|---|
| datagram | 24 … 1,200 bytes | `uNET_MAX_DATAGRAM_BYTES` |
| message body | 1,176 bytes | `uNET_MAX_MESSAGE_BYTES` |
| commands per datagram | 64 (reached by one block and no digest pair) | `uNET_MAX_COMMANDS_PER_DATAGRAM` |
| INPUT blocks | 4 | `uNET_MAX_INPUT_BLOCKS` |
| INPUT digest pairs | 4 | `uNET_MAX_DIGEST_PAIRS` |
| fragment body | 1,020 bytes | `uNET_MAX_FRAGMENT_BODY_BYTES` |
| fragments per tick | 32 | `uNET_MAX_FRAGMENTS_PER_TICK` |
| destroys per fragment | 64 | `uNET_MAX_DESTROYS_PER_FRAGMENT` |
| destroys per capture | 256 (four fragments) | `uNET_MAX_DESTROYS_PER_CAPTURE` |
| replicated entities per capture | 512 | `uNET_PROOF_MAX_REPLICATED_ENTITIES` |
| replication table entries | 32 | `uNET_PROOF_MAX_REPLICATED_PROPERTIES` |
| proxy table | 16 | `uNET_MAX_PROXY_PREFABS` |
| ULEB128 | 10 bytes | — |
| publication payload | 1,024 bytes | `uNET_MAX_PUBLICATION_BYTES` |
| datagrams received per frame | 512 | `uNET_MAX_DATAGRAMS_PER_PUMP` |
| NET save section | exactly 24 bytes | `uNET_SAVE_SECTION_BYTES` |

These rows replace the Stage 3 "network messages" row of `Zenith2/Docs/Architecture.md` → "Security architecture" (cross-doc item X394) and are SEC's parser-limit inventory entries for NET (row NT4); each is a constant of `Zenith_NetTypes.h`, tested at the limit and at limit + 1 ([Z2-N-ARCH-953]) by the tests of `Zenith2/Docs/Design/NET/Tests.md` → "Wire format and fuzz tests". The limit + 1 cases that no encoder can produce (65 commands, a 1,025-byte publication, a 25-byte section, a 17-entry proxy set, a 33-entry table, a 513th datagram in one pump) are built by hand at the decoder or registration level.

### Packet validation order

`Zenith_NetReadPacket` and the transport check, in this order, stopping at the first failure; every failure drops the datagram, increments one counter and, for the first failure of each kind per second, logs one `net.wire` line with the sender address:

1. size 24 … 1,200 (`net.packets.malformed`);
2. magic (`net.packets.foreign`: not a Zenith2 datagram);
3. CRC-32C (`net.packets.malformed`);
4. protocol version: a different version in a CONNECT_REQUEST is answered by CONNECT_REJECT `VERSION`; any other is dropped (`net.packets.version`);
5. message kind 1 … 7 and flags 0 (`net.packets.malformed`);
6. body length: exact for CONNECT_REQUEST (64), CONNECT_ACCEPT (32), CONNECT_REJECT (16), KEEPALIVE (8), DISCONNECT (8); for INPUT and SNAPSHOT the structural decode below (`net.packets.malformed`);
7. session: a server drops a non-request from an address that is not a connected peer, or whose session ID differs from the peer's (`net.packets.foreign`); a client drops anything whose session ID is not its session's, except an ACCEPT or REJECT answering its current nonce;
8. sequence: inside a session, a sequence already seen in the 64-packet window, or older than the window, is dropped (`net.packets.duplicate`);
9. the kind's decoder: every count, length and offset checked against the remaining body with overflow-checked arithmetic, every reserved byte 0, ULEB128 canonical, network IDs ascending, the presence mask within the table, the proxy index within the proxy table, the fragment index below the count, the low hash bits equal to the local table's (`net.packets.malformed`, or `net.packets.table` for the hash).

A peer that sends `uNET_MALFORMED_DISCONNECT_COUNT` (8) malformed packets within one second is disconnected with `NET_DISCONNECT_REASON_PROTOCOL_ERROR`. No step asserts ([Z2-N-ARCH-951], [Z2-N-CONTRACT-472]).

## Session state machines

### Client session states

`Zenith_NetWorldState::m_eSessionState`, which `Net.SessionState` reads, is a pure function of the world's role and the session events `Net.Session` has applied from the recorded session inbox since the world's attach or its last restore, and of nothing else: the transport writes it never. Its initial value at attach is identical live and under `--replay`: `OFFLINE` for the offline role, `LISTENING` for a server, `CONNECTING` for a client. `Net.Session` (phase 1) applies each inbox event in completion-ID order: CONNECTED → `CONNECTED`; DISCONNECTED → `DISCONNECTED`; FAILED → `FAILED` (a server sees no CONNECTED, DISCONNECTED or FAILED event); the server's aggregate follows § "Server aggregate state". A restore sets it by the rule of `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore".

The client transport's own states (prologue only; what the transport does, never what scripts read):

| Transport state \ event | `Start` (the first prologue after the session world's first run of `Net.Session`) | retry timer | ACCEPT (current nonce) | REJECT (current nonce) | connect timeout | SNAPSHOT, KEEPALIVE, INPUT received | DISCONNECT received | 8 malformed in 1 s | peer timeout | world swapped out or restored | shutdown |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `IDLE` | the world's proxy-table flag (written by the first run of `Net.Session` after attach) is invalid: publish FAILED (`PROXY_TABLE`), → `DONE`; a table entry not ACTIVE (read here, in the prologue): publish FAILED (`PROXY_NOT_RESIDENT`), → `DONE`; port open fails: publish FAILED (`SOCKET`), → `DONE`; else open the port, send REQUEST, → `CONNECTING` | — | — | — | — | — | — | — | — | → `DONE`, nothing published | nothing |
| `CONNECTING` | — | send REQUEST (same nonce) | adopt session ID, peer index, offset (§ "Target tick mapping"); publish CONNECTED; → `CONNECTED` | publish FAILED with the reason; → `DONE` | publish FAILED (`CONNECT_TIMEOUT`); → `DONE` | dropped by the session check (`net.packets.foreign`) | dropped (no session yet) | dropped and counted (no peer to disconnect) | — | close; nothing published; → `DONE` | close; nothing published |
| `CONNECTED` | — | — | duplicate: ignored | ignored | — | deliver; refresh last-heard | publish DISCONNECTED with the reason; → `DONE` | send DISCONNECT (`PROTOCOL_ERROR`); publish DISCONNECTED (`PROTOCOL_ERROR`); → `DONE` | publish DISCONNECTED (`TIMEOUT`); → `DONE` | send DISCONNECT (`WORLD_TRANSITION` or `RESTORED`); nothing published (a swapped-out world never ticks again; a restored world's state was set by the restore); → `DONE` | send DISCONNECT (`CLIENT_QUIT`) |
| `DONE` | — | — | ignored | ignored | — | dropped | ignored | ignored | — | — | nothing |

- Every "publish" allocates one `ASYNC_SOURCE_NETWORK` completion ID in the prologue, from the session-event reserve of § "Fragment publication" so a fragment backlog never refuses it, and parks the session event of § "Fragment publication"; the world sees it at the next tick boundary, so `Net.SessionState` and `Net.SessionChanged` follow only recorded events and are identical under `--replay`, where the transport does not exist.
- Nothing of this table runs inside `RunFrame`: no phase system opens a port, sends a datagram, allocates a completion ID or reads live asset state (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Allocation sites and acceptance" step 2).
- A client never reconnects at Stage 3: `DONE` is final for the process.

### Server peer states

The server binds at module start (`Zenith2/Docs/Design/NET.md` → "Module start"); a bind failure fails startup, so a running server's transport always listens. Each peer slot (`net.maxPeers`, 1 at Stage 3):

| Slot state \ event | valid REQUEST, new nonce | REQUEST, nonce of this slot | INPUT or KEEPALIVE | 8 malformed in 1 s | peer timeout | DISCONNECT received | session world detached or restored | shutdown | linger elapsed |
|---|---|---|---|---|---|---|---|---|---|
| `FREE` | the proxy table is not yet built (before the first run of `Net.Session` after attach): dropped, counted `net.packets.early` (the client retries); checks pass: issue peer index, draw session ID, send ACCEPT, publish PEER_JOINED, → `CONNECTED`; a check fails: send REJECT | — | dropped (`net.packets.foreign`) | counted | — | dropped | — | — | — |
| `CONNECTED` | another client while the only slot is taken: send REJECT `SERVER_FULL` | send ACCEPT again (the same session, index and offset fields) | update last-heard; deliver | send DISCONNECT (`PROTOCOL_ERROR`); publish PEER_LEFT (`PROTOCOL_ERROR`); → `LINGER` | publish PEER_LEFT (`TIMEOUT`); → `LINGER` | publish PEER_LEFT (the reason); → `LINGER` | send DISCONNECT (`WORLD_TRANSITION` or `RESTORED`); nothing published; → `LINGER` | send DISCONNECT (`SERVER_SHUTDOWN`); nothing published | — |
| `LINGER` (1,000 ms) | REJECT `SERVER_FULL` | dropped (`net.packets.foreign`) | dropped | ignored | — | dropped | — | nothing | → `FREE` |

The linger absorbs the old session's in-flight datagrams so they are never mistaken for a new peer.

### Server aggregate state

`Net.Session` keeps, from the recorded events alone, the set of joined peers: PEER_JOINED adds the peer, PEER_LEFT removes it. The server's `m_eSessionState` is `CONNECTED` while the set is non-empty and `LISTENING` while it is empty, from attach on (so a server that lost its one peer reads `LISTENING` again, never `DISCONNECTED`); `Net.Peers` returns the set in ascending order. A PEER_LEFT for a peer not in the set is counted (`net.publications.malformed`) and changes nothing.

### Handshake rules

- The server checks a request in this order and rejects at the first difference: protocol version (`VERSION`), build ID, composition hash, content hash, ECS backend, physics backend, tick rate (`IDENTITY` with the field), replication table hash (`REPLICATION_TABLE`), proxy table hash (`PROXY_TABLE`), a free slot (`SERVER_FULL`), a peer index below 65,535 (`PEER_INDEX_EXHAUSTED`), an attached session world (`NOT_ACCEPTING`: the session world is detached, swapped out or the process is shutting down).
- Before the server's session world has run `Net.Session` once after attach (§ "Proxy prefab table") its proxy table does not exist; a request then is dropped and counted (`net.packets.early`), never rejected, so a client started at the same moment as its server joins at its next retry instead of failing finally.
- Equal build, composition and content identities make the two processes the same game, so every reflected type, schema and configuration default agrees; the two table hashes check the session content of the startup scenes, which may differ between roles.
- The accept is idempotent: a repeated request with the slot's nonce gets an ACCEPT with the same session ID, peer index, input delay and snapshot interval, carrying the server's current last completed tick, so the client's offset (§ "Target tick mapping") is computed from the ACCEPT it adopts and a lost ACCEPT costs one retry interval without making the client's targets lag.

### Timeouts and keepalive

| Timer | Value | Effect |
|---|---|---|
| retry | `net.retryIntervalMs` (250) | client resends CONNECT_REQUEST while `CONNECTING` |
| connect timeout | `net.connectTimeoutMs` (10,000), from `Start` | client publishes FAILED (`CONNECT_TIMEOUT`) |
| keepalive | `net.keepaliveIntervalMs` (250) | a side that sent nothing to its peer for this long sends KEEPALIVE |
| peer timeout | `net.peerTimeoutMs` (5,000) | a side that heard nothing valid from its peer for this long ends the session (`TIMEOUT`) |
| linger | `uNET_LINGER_MS` (1,000) | server slot back to `FREE` |

Timers read `Zenith_GetMonotonicTimeUs` in the prologue and in `Net.Send` only (`Zenith2/Docs/Design/NET/Foundations.md` → "Clock discipline").

## Input path

### Client command capture

`Zenith_NetCommandSource::ObserveTickCommands(t, commands)` runs on the client's `Input.Actions` node every tick, live and in replay; the client's bridge answers `ReplacesLocalInput()` false, so `commands` is the client's own mapped input (`Zenith2/Docs/Design/NET/Foundations.md` → "Command source contract"). In every run it records the sequence of the last command it observed (none before the first). While `m_bAdopted` is set — module memory the transport writes in the prologue that adopts the ACCEPT, so it is set before tick c0's `Input.Actions` runs, unlike the world's `m_eSessionState`, which `Net.Session` writes in the same phase with no order against `Input.Actions` — and the run is live, it appends the block `{ client tick t, commands }` to the world's outbound ring: `uNET_MAX_INPUT_BLOCKS` + `net.redundantInputTicks` + 60 = 67 blocks, each holding up to 64 commands in its own slot of one 67 × 64 × 20-byte arena; a block above 64 commands keeps the rest in the shared burst pool, one pool of 4,096 × 20 bytes for the whole ring (not one per tick), `QUEUE_FULL_POLICY_DROP_NEWEST`, its drops counted in `net.client.outbound.dropped` (a dropped command never reaches the server, which declares its sequence lost after the hold). A tick above 61 commands is split across datagrams by § "Client INPUT emission". Before adoption and under `--replay` it appends nothing; the commands still reach the client's own simulation and its replay log through PLAT.

The session base sequence B is fixed when the transport adopts the ACCEPT, in the same prologue that sets `m_bAdopted`: the sequence after the last command observed, or 0 (PLAT's first sequence value, row NT3) when none was observed. Every INPUT of the session carries B, so tick c0's commands, the first block, start at B. The server's loss accounting relies on PLAT's sequence being gapless (row NT3).

### Target tick mapping

At the prologue in which the client adopts an ACCEPT, it computes the tick offset o:

```text
c0 = sink.GetNextTick()                       // the client tick whose boundary publishes the CONNECTED event
s0 = accept.server_tick + accept.input_delay_ticks
o  = int64(s0) − int64(c0)                    // target server tick of client tick c = c + o
```

The offset is published inside the CONNECTED session event, so the client's replay log records it and the verifier can map ticks (§ "Command log verification"). The first block sent is client tick c0's. Both processes tick at the same `sim.tickHz` (§ "Tick-rate agreement" of `Zenith2/Docs/Design/NET/Foundations.md`), so on an idle machine a block reaches the server about `input_delay_ticks` − 1 ticks before its target; the offset is fixed for the session at Stage 3 (Stage 7 synchronises the clocks, `Zenith2/Docs/Design/NET/Stage7.md` → "Clock synchronisation").

### Client INPUT emission

In `Net.Collect@W<tag>` (the session world's epilogue stage) and `Net.Send`:

1. Take every block appended since the last send (one per tick that ran this frame).
2. For each new block, newest last, build an INPUT whose blocks are the new block preceded by up to `net.redundantInputTicks` previous blocks (oldest first), dropping the oldest redundant blocks while the body would exceed 1,176 bytes or 64 commands.
3. A new block with more than 61 commands is sent as several INPUTs, each one block of the same target tick holding the next run of at most 61 commands in sequence order, without redundancy.
4. Every INPUT carries B, the cumulative digest count and the digest ring's pairs (§ "Acknowledgements and digests").
5. A frame that completed a tick's fragments but has no new block sends an INPUT with no block, so every complete tick is reported within one frame; with nothing else sent for `net.keepaliveIntervalMs`, a KEEPALIVE is sent.

### Server command intake

`OnInput` (prologue, main thread) for the connected peer:

1. Process the digest pairs and the cumulative count (§ "Acknowledgements and digests").
2. The first INPUT of the session sets the window's next expected sequence E to the INPUT's session base sequence B (0 for a client that sent nothing before it connected, PLAT's first value); any subsequent INPUT whose B differs from the session's is malformed. Commands the client produced before it connected carry sequences below B: they were never sent and are neither lost nor late.
3. For each block and each command in it: a command whose player slot is not 0 makes the whole packet malformed; a sequence below E is a duplicate (`net.server.duplicate_commands`, dropped); a target tick more than `uNET_SERVER_COMMAND_WINDOW_TICKS` (128) ahead of the session world's next tick is dropped (`net.server.commands_too_early`); otherwise the command is inserted into the window, keyed by sequence (a sequence already in the window is a duplicate), with its target tick.

The window is a ring of 128 tick slots indexed by target tick modulo 128, each holding up to 64 commands in its own slot and the rest in one burst pool of 4,096 commands shared by all slots (the per-tick replay limit is 4,096), plus a sequence-ordered index of pending commands (a fixed array of 8,192 entries). Both the pool and the index are `QUEUE_FULL_POLICY_DROP_NEWEST`, counted in `net.server.window.dropped`; a dropped command's sequence is declared lost after the hold.

### Server command window

The window releases commands strictly in sequence order, so the server never applies a subsequent command before an earlier one. E is the next expected sequence (§ "Server command intake" step 2); tick indices are far below 2^63, so the differences are computed in `sint64`:

```text
Release(t, axOut):                                 // called by ProvideTickCommands(t, axOut), phase 1 of server tick t
  count = 0
  while count < axOut.size:
    n = the pending command with the smallest sequence (the heap's top); if absent: break
    if n.sequence == E:
        if int64(n.target) ≤ int64(t):
            pop n; axOut[count++] = n; E += 1
            if int64(n.target) < int64(t): net.late_commands += 1
            continue
        break                                       // the next command is early: wait for its tick
    // n.sequence > E: sequences E … n.sequence − 1 are missing
    if int64(t) − int64(n.target) ≥ int64(net.server.commandHoldTicks):
        net.server.lost_commands += n.sequence − E; E = n.sequence; continue   // n is released by the next iteration
    break                                           // hold the gap, including when n is still early
  return count                                      // ascending sequence
```

- A command released at its target tick is on time; one released at a subsequent tick is late and counted in `net.late_commands`, the counter Architecture names ([Z2-N-ARCH-786]).
- A gap waits until `net.server.commandHoldTicks` (4) ticks after the waiting command's target, then the missing sequences are declared lost; an early command behind a gap never triggers the loss (the difference is negative). With the client's three redundant blocks a gap needs four consecutive lost datagrams.
- A release that fills `axOut` leaves the rest pending; they are released at the next tick, late.
- Release is O(k log k) for k released commands; the sequence index is a binary heap over the fixed array.

### Command delivery to phase 1

`ProvideTickCommands(t, axOut)` returns `Release(t, axOut)` (at most `axOut.size` commands; the rest stay pending and are released at the next tick, counted late). PLAT's `Input.Actions` applies them, rebuilds the world's action state and logs them at step 5, so the server's replay log holds exactly the commands each tick applied, in sequence order. The server never invents, reorders or re-quantises a command.

## Snapshot path

### Capture at the snapshot point

`Net.Capture` is a C++ system of the server's session world in phase 11 (`PHASE_SAVE_SNAPSHOT`), every `net.snapshotIntervalTicks`-th tick:

```cpp
xBuilder = xContext.GetSystems().AddSystem({ .m_szName = "Net.Capture", .m_ePhase = PHASE_SAVE_SNAPSHOT,
	.m_pfnRun = &Zenith_NetCapture_Run, .m_pUserData = pxWorldState, .m_eAffinity = JOB_AFFINITY_ANY, .m_uCostHint = 4 });
xBuilder.Read(Zenith_NetIdentityComponent::xTYPE_ID)
	.ReadResource(Zenith_NetWorldStateResource::xTYPE_ID)
	.WriteResource(Zenith_NetCaptureResource::xTYPE_ID)          // owning phase SaveSnapshot (cross-doc item X396)
	.Query(xReplicatedQuery, m_xSlot);                           // rows carrying Zenith.Net.Identity
for (const Zenith_NetReplicationEntry& xEntry : pxTable->GetEntries())
	xBuilder.ReadAnyRow(xEntry.m_xType, Zenith_Span<const Zenith_PropertyID>(&xEntry.m_xProperty, 1));
```

Steps, at server tick t:

1. Collect every row of the query whose identity has `NET_IDENTITY_FLAG_REPLICATE`, a non-zero ID and no `NET_IDENTITY_FLAG_PROXY` into an arena list `{ ID, handle, proxy prefab ID, owner }`; sort ascending by network ID.
2. Map each entry's proxy prefab to its index in the proxy table (§ "Proxy prefab table"); an entity whose prefab is not in the table is skipped and counted (`net.server.unlisted_proxy_prefabs`; logged once per prefab).
3. Rotate the list to start at the capture cursor: the first entry whose ID is at or above the cursor (wrapping to the start); the cursor is 0 at attach. The rotated list ascends except at the wrap, where `Encode` starts a new fragment, so every fragment's IDs ascend.
4. For each entry and each table entry: read the property of the entity through random access (`Zenith2/Docs/Design/ECS/Queries.md` → "Resolve and random access"); absent component → presence bit clear; present → quantise (§ "Quantisation").
5. Take the destroys of this capture from the pending-destroy list (§ "Fragment encoding").
6. Encode fragments (§ "Fragment encoding") into the capture ring slot `t mod 8` (`uNET_CAPTURE_RING_TICKS`), recording `{ t, fragment count, body lengths, digest }`; the digest is the CRC-32C of the concatenated bodies in index order.
7. Set the cursor: when the encoding stopped before an entry (512 entries or the fragment budget), the cursor becomes that entry's ID, so the next capture starts at the first entry left out; otherwise it returns to 0. A capture that stopped early also counts `net.server.snapshot_truncated`. Every entity is therefore captured within ⌈count / k⌉ captures, k the number a capture holds, whatever the truncation.
8. Proof mode: decode the slot's fragments with the client's decoder and compare every decoded value with the quantisation of the same entity's property read in step 4 in this tick; a difference counts `net.proof.capture_mismatches` (P6); the capture chain becomes `Zenith_Hash64` of (the previous chain, t, the digest), starting from 0, and at the module instance's destruction the server logs one line `net.proof: capture chain server <capture count> <chain hex>` at INFO in category `net.proof` (§ "Replay verification"). The line reaches the log file because the destructor runs at shutdown before DEVT detaches its outputs, and the chunk `Detach` hands over holds every record logged before it (`Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown", X245 (e); `Zenith2/Docs/Design/DEVT/Console.md` → "Log file sink", `Test_LogFileSink_NoWriteAfterDetach`; DEVT confirms, row NT26).

Phase 11 is read-only for components ([Z2-N-ARCH-476] as amended by cross-doc item X396); the capture writes only its own resource. `Net.Capture` runs live and under `--replay`, so the server's replay recomputes the fragments it sent. The cursor is module state, not simulation state, and a function of the sequence of captures, so a replay reproduces it. `Net.Capture` does not run on clients or offline.

### Proxy prefab table

The proxy table maps a proxy prefab to a one-byte index both peers agree on. It is built by the first run of `Net.Session` after the session world's attach — what NET's text calls the session world's first tick; a restored world resumes at a tick index above 0, so the rule names the run, not an index — from component data alone:

1. Find the entities carrying `Zenith.Net.ProxyPrefabSet`. None: an empty table (a server can still run; a client's transport fails with `PROXY_TABLE` at `Start`). More than one: the table is invalid (`net.proxy_table.invalid`, logged with both simulation IDs).
2. Take its non-null entries in array order; a repeated prefab is invalid.
3. Entry i is index i; the table hash is `Zenith_Hash64` over the 16-byte persistent IDs in index order.
4. Write the table, its hash and a validity flag into the world state.

No simulation system reads live asset state: the residency of the proxy prefabs (each entry's state ACTIVE, `Zenith_AssetRegistry::GetState`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetRegistry") is checked by the client transport in the prologue at `Start` (§ "Client session states"), and its outcome reaches the world only as a recorded FAILED event. The set is part of the startup scene, so its prefabs are hard dependencies loaded before step 17 completes, and the check fails only for broken content.

A server entity's proxy prefab is its `Zenith.Net.Identity` property 2: the prefab clients spawn for it. The server's authoritative prefab and the proxy prefab are different assets (the proxy has no physics body, controller, AI or gameplay behaviour; § "Scene and prefabs").

### Quantisation

Both codecs round to nearest with ties to even (`std::nearbyint` under the fixed rounding mode of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", subsection "Floating-point environment"), compute in binary32 with no contraction, and use only `+ − × ÷`, `std::nearbyint` and `std::sqrt` of a non-negative value, all correctly rounded on every platform, so both peers quantise and dequantise bit-identically ([Z2-N-ARCH-888]); no transcendental function is used.

`NET_CODEC_POSITION_MM`, for each component v of a `Zenith_Vector3`:

```text
if v is not finite: q = 0; net.quantise.non_finite += 1
else:
  m = v × 1000.0f                                   // binary32 multiply
  q = nearbyint(m)                                  // ties to even
  q = clamp(q, −8388607, 8388607); a clamp increments net.quantise.clamped
dequantise: v' = float(q) / 1000.0f                 // q is exact in binary32 (|q| < 2^23); correctly rounded division
```

The range is ±8,388.607 m at 1 mm, the range in which `float(q)` and `m` stay below 2^23. The largest input that is not clamped is 8,388.6064453125 m (q = 8,388,606): the next binary32 value, `float(8388.607)` = 8,388.607421875, gives the product 8,388,607.5, which ties to even 8,388,608, so ±8,388.607 m itself is clamped to ±8,388,607 and counted in `net.quantise.clamped`. The error |v' − v| is at most 0.5 mm (rounding to the millimetre) + 0.25 mm (the binary32 product, below 2^23 its ulp is at most 0.5) + 0.49 mm (representing v' in binary32, ulp 2^−10 m near 8 km) = 1.24 mm, and the measured worst over 4,000,000 samples across the range is 0.98 mm. Stage 7 replaces the codec with declared per-property ranges (`Zenith2/Docs/Design/NET/Stage7.md` → "Property codecs").

`NET_CODEC_QUAT_SMALLEST_THREE`, for a `Zenith_Quaternion` (x, y, z, w):

```text
if any component is not finite: emit the identity's encoding; net.quantise.non_finite += 1
n2 = ((x² + y²) + z²) + w²; if n2 = 0: identity; else divide each by std::sqrt(n2)
k  = the index (0 = x, 1 = y, 2 = z, 3 = w) of the largest |component|; ties take the lowest index
if component k < 0: negate all four                  // canonical sign: the largest component is positive
for each of the three others a, in index order:     // each lies in [−1/√2, 1/√2]
  u = nearbyint((a × fSQRT2 + 1.0f) × 511.5f), clamped to 0 … 1023
packed = (k << 30) | (u0 << 20) | (u1 << 10) | u2
dequantise: a' = ((float(u) / 511.5f) − 1.0f) / fSQRT2 for the three;
            c = std::sqrt(max(0, ((1 − a0'²) − a1'²) − a2'²)) at index k
```

`fSQRT2` is the binary32 constant `1.41421353816986083984375f`. The angular error is at most 0.0048 rad: three components each within half a step (1 / (511.5 √2) ≈ 0.0014) plus the reconstructed fourth. The analytic worst, all four components equal and the three sent ones rounded the same way by half a step, is 0.0047988 rad; a hill-climb over the codec reaches 0.004784 rad (u = 874, 872, 874 offset by −0.4999 of a step); random rotations reach about 0.004 rad (0.0041 to 0.0043 rad as the worst of 1,000,000 Box–Muller samples, depending on the generator). Method: the codec emulated per operation in binary32 with ties to even, the angle of rotation measured in binary64 as 4 · asin(‖q − s·q′‖ / 2) with s the sign of the dot product (a binary32 `acos` of the dot product moves by about 5e-5 rad per ulp at this angle, far above the bound's slack).

NET never requantises a dequantised value. A dequantised rotation does not always requantise to the same word (near a tie between two components, for example a yaw near 90°, the reconstructed largest component can lose the tie), and a dequantised position above 8,192.011 m can requantise one millimetre off; neither matters, because every equality NET checks compares dequantised bits with the dequantisation of the same word (§ "Client apply", § "Proof assertions"), which holds by construction: dequantisation is a pure function of the word. The readback C1 performs also relies on `SetProperty` storing a VEC3 or QUAT property's bytes verbatim, an ECS and SCENE assumption (row NT25); the spawn path writes position and rotation through `SetProperty` on the ticket handle too (§ "Client apply"), so no normalising helper is on the checked path. `Test_NetQuantise_PositionGoldenVectors` and `Test_NetQuantise_QuatGoldenVectors` check hand-computed encodings (0, ±0.0005 m and ±0.0015 m ties rounding to even, ±8,388.6064453125 m unclamped, ±8,388.607 m clamped and counted, NaN, identity, a 90° rotation about each axis, a quaternion with a negative largest component, the tie of two equal components).

### Fragment encoding

Destroys: each pending-destroy entry (`Zenith2/Docs/Design/NET/Foundations.md` → "Network ID map") holds a remaining-send count, 30 (`uNET_DESTROY_REPEAT_TICKS`) when queued. A capture takes at most `uNET_MAX_DESTROYS_PER_CAPTURE` (256) entries with a remaining count above 0, oldest destroy tick first, then ascending ID; each taken entry's count decreases by 1, and an entry at 0 leaves the list. So a flood of destroys is spread across captures and every destroy is still sent in 30 captures, counted from its first.

```text
Encode(t, entries rotated at the cursor, destroys (≤ 256, ascending ID)):
  fragments = [new fragment(t)]                     // header 20 bytes, reserved zero, table hash low bits
  for each destroy ID:
    if current has 64 destroys or no room for 10 more bytes: start a new fragment
    append ULEB128(ID); current.d += 1               // at most 4 fragments carry destroys (256 / 64)
  for each entry (at most 512):
    size = |ULEB128(ID)| + 1 + 1 + (owner ≠ 0 ? 2 : 0) + 4 + Σ wire bytes of the present entries
    if current body + size > 1020 or (current.r > 0 and ID < the previous record's ID):   // full, or the cursor's wrap
        if 32 fragments exist: stop before this entry (the cursor resumes here) ; break
        start a new fragment
    append the record; current.r += 1
  set every fragment's count; return fragments
```

The fragment count never exceeds 32: destroys occupy at most 4 fragments (256 entries of at most 10 bytes, at most 64 each), and records stop when a 33rd fragment would be needed. Records never span fragments; a record is at most 10 + 1 + 1 + 2 + 4 + 32 × 12 = 402 bytes, so every record fits an empty fragment, and the 28 or more fragments left to records hold at least 54 of them (two per fragment, less at most one partly filled fragment at the wrap). The new fragment at the wrap keeps IDs strictly ascending inside every fragment, as the SNAPSHOT layout and the decoder require. A tick's fragments are ordered and independent: a client can apply any subset. The encoding is a pure function of (t, entries, cursor, destroys, table), so two captures of the same state produce identical bytes, live and in replay.

### Snapshot send

`Net.Collect@W<tag>` (the server's session world, epilogue) copies every capture-ring slot captured this frame into the module's per-world send slot; `Net.Send` queues each fragment as one SNAPSHOT datagram to the connected peer, ticks in ascending order, fragments in index order. The capture ring keeps the last eight captures, so a frame with up to eight catch-up ticks loses none; a frame cannot run more, because NET refuses to start a server whose `sim.maxCatchUpTicks` exceeds 8 (`net: a server needs sim.maxCatchUpTicks ≤ 8`, `MODULE_ERROR_REGISTRATION`; the key's range is 1 … 64, `Zenith2/Docs/Design/JOBS.md` → "Configuration keys").

### Client fragment intake

`OnSnapshotFragment` (prologue, main thread), for a fragment that passed § "Packet validation order":

1. A fragment whose tick is 64 or more ticks older than the newest fragment tick received is dropped (`net.client.fragments_too_old`); otherwise record the fragment index in the tick's receive bitmap (a 64-entry ring keyed by server tick).
2. While the outstanding count is below `m_uMaxInFlight` − `uNET_SESSION_EVENT_RESERVE` (§ "Fragment publication"), allocate an `ASYNC_SOURCE_NETWORK` completion ID from the session world's publication sink (`Zenith_ModulePublicationSink::AllocateCompletionID`, `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Allocation sites and acceptance"). Otherwise, or on `PUBLICATION_SINK_ERROR_FULL`, the fragment is dropped (`net.client.fragments_dropped_full`) and its tick stays incomplete; the client never retries, because the next tick's fragment supersedes it.
3. `Park` the publication of § "Fragment publication".
4. When every index of a tick has been parked, the tick is complete: its digest (CRC-32C of the bodies in index order) joins the client's digest ring (§ "Acknowledgements and digests").

Under `--replay` no fragment is received and nothing is allocated: the sink refuses a prologue allocation while replaying ([Z2-N-SCRIPT-341]), and the recorded publications arrive at their recorded ticks instead.

### Fragment publication

<!-- assumption: pending SCRIPT -->
NET's source kind is `ASYNC_SOURCE_NETWORK`, requested from SCRIPT's registry (row NT2, cross-doc item X392); SCRIPT assigns its value in `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry", and NET's code names it by enumerator alone ([Z2-N-SCRIPT-925]). NET declares it on each session world's module publication sink at `AttachWorld`: `DeclareSource(ASYNC_SOURCE_NETWORK, { m_uMaxInFlight = net.client.maxFragmentsInFlight (256) for a client, 64 for a server, m_uMaxResultBytes = 1024, m_uSites = MODULE_SOURCE_SITE_PROLOGUE })` and `SetSourceHandler(ASYNC_SOURCE_NETWORK, instance)`; every publication is `PUBLICATION_KIND_GENERIC_RECORDED`. A client's declaration reserves 256 × 1,024 = 256 KiB of result arena, inside `ulMODULE_PUBLICATION_MAX_ARENA_BYTES`. Session events keep a reserve of `uNET_SESSION_EVENT_RESERVE` (8, the session inbox's size) by MOD's reserve pattern (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Parking a result"): a fragment (and, at Stage 7, a message piece or a `PEER_VIEW` record) is allocated only while `GetOutstandingCount(ASYNC_SOURCE_NETWORK)` is below `m_uMaxInFlight` − 8, and a session event may use the rest, so a paused world's fragment backlog never makes a DISCONNECTED or FAILED event refuse.
<!-- /assumption -->

Publication payload, little-endian:

| Offset | Type | Field |
|---|---|---|
| 0 | `uint8` | record type: 1 snapshot fragment, 2 session event |
| 1 | `uint8` | record version: 1 |
| 2 | `uint16` | reserved, 0 |
| 4 | … | type 1: the SNAPSHOT body (20 … 1,020 bytes); type 2: the 28-byte session event below |

Session event (28 bytes):

| Offset | Type | Field |
|---|---|---|
| 0 | `uint8` | event: 1 CONNECTED, 2 DISCONNECTED, 3 FAILED, 4 PEER_JOINED, 5 PEER_LEFT (Stage 7 adds 6 OFFSET_CHANGED and 7 RECONNECTING, `Zenith2/Docs/Design/NET/Stage7.md` → "Clock synchronisation") |
| 1 | `uint8` | reason (`Zenith_NetRejectReason` for FAILED, `Zenith_NetDisconnectReason` for DISCONNECTED and PEER_LEFT, else 0) |
| 2 | `uint16` | peer index (the local index for CONNECTED, the remote one for PEER_JOINED and PEER_LEFT, else 0) |
| 4 | `sint64` | client tick offset o (CONNECTED), else 0 |
| 12 | `uint64` | session ID (CONNECTED, PEER_JOINED), else 0 |
| 20 | `uint64` | the accept's server tick (CONNECTED), else 0 |

`OnPublished` (tick-boundary step 1 of the session world, in completion-ID order) validates the prefix and copies type 1 into the client world state's fragment inbox and type 2 into the session inbox; a payload that fails validation (a malformed recorded log) is dropped and counted (`net.publications.malformed`). It writes no component: the inboxes are module memory read by the phase-1 systems of the same tick.

### Client apply

`Net.ClientApply`, phase 1 of every client tick, `After("Net.Session")`, declaring `EmitsCommands()`, `EmitsEvent(Net.ControlledEntityChanged)`, a read of `Zenith.Net.Identity`, `ReadAnyRow` of every table entry, and a write of `Zenith.Net.WorldState`. One invocation, merge key simulation ID 0. It allocates no completion ID and reads no asset state. First the proof check C1 (§ "Proof assertions"), then for each fragment of the inbox, in inbox order:

1. Decode with full validation (the bytes come from the log under `--replay`); a failure drops the fragment (`net.publications.malformed`).
2. Destroys: for each ID, a mapped entity is destroyed with `Zenith_CommandBuffer::Destroy` (`net.client.proxy_destroys`); a pending spawn of that ID is cancelled by destroying its ticket handle; an unknown ID is counted (`net.client.unknown_destroys`). Each destroy, mapped or not, enters or refreshes the tombstone table `{ ID, fragment tick }`; an entry leaves it once the newest applied fragment tick is 64 or more ticks past it, matching the intake's 64-tick age limit. Its capacity is 64 × `uNET_MAX_DESTROYS_PER_CAPTURE` = 16,384 entries of 16 bytes (256 KiB), the most distinct destroys 64 captures can carry, so no live tombstone is ever evicted.
3. Records: skip a record whose ID has a tombstone with a fragment tick at or above the record's (`net.client.fragments_stale`), so a reordered older fragment never resurrects a destroyed proxy; skip a record for an entity whose last applied server tick is at or above the fragment's tick (`net.client.fragments_stale`). Otherwise:
   - unknown ID and not pending: when the world's map is at capacity, spawn nothing and count `net.client.spawns_refused_map_full` (so a full map never spawns a proxy per tick); otherwise `Zenith_RecordInstantiatePrefabEx` of the proxy table's prefab at the record's dequantised position and rotation (`m_bAddTransformComponents` true; `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header"), then `SetProperty` of position and rotation on the ticket handle with the same dequantised values (the verbatim path C1 checks), then `AddComponent` of `Zenith.Net.Identity` on the ticket handle with property 1 = the ID, 2 = the prefab, 3 = the owner, 4 = `NET_IDENTITY_FLAG_REPLICATE | NET_IDENTITY_FLAG_PROXY`; the ID enters the pending-spawn table with the ticket handle (`net.client.proxy_spawns`); a null ticket (prefab not resident) counts `net.client.proxy_not_resident`;
   - pending ID: `SetProperty` on the ticket handle for each present entry;
   - mapped ID: `SetProperty` on the entity for each present entry (the dequantised value), and `SetProperty` of property 3 when the owner changed;
   - record the entity's last applied server tick and, per present entry, the applied word and the dequantised floats' bits (for C1).
4. After all fragments: if the entity whose owner equals the local peer differs from the current controlled entity, emit `Net.ControlledEntityChanged { Entity, Previous }` and update it.

Commands are applied at the sync point after phase 1, so every proxy shows the newest received server state from phase 2 on. The next tick's `Net.IdentityIndex` indexes the spawned proxies and removes them from the pending table. A proxy's `Zenith.LocalTransform` is written only through these commands (`Zenith2/Docs/Design/NET/Foundations.md` → "Authority on clients"). The apply tables (applied ticks and values, pending spawns, tombstones, the controlled entity) are module memory, never saved; a restore clears them (`Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore").

### Acknowledgements and digests

- The client keeps a ring of the last 4 complete ticks it parked, `{ server tick, digest }`, in completion order, and a cumulative count of distinct complete ticks entered into it this session; every INPUT carries the count and the ring's pairs, sorted by tick at emission. Because the ring is in completion order, the ticks no INPUT has carried yet are always its newest suffix. In the steady case (at most one tick completes per INPUT) each pair rides four consecutive INPUTs and survives the loss of three; a burst frame's completions ride one or two INPUTs, so losing those can leave a tick unreported, which P3 then reports. When a fifth tick completes in one pump while the ring holds four ticks not yet carried by any INPUT, the transport first sends an INPUT with no block from the prologue (sending there is the client's request path already, § "Pump and send discipline"), so every complete tick enters at least one INPUT; `net.proof.digests_unreported` counts a pair that left the ring unsent (0 by construction; C4 holds it).
- The server keeps the digests of its last 64 captures (`uNET_ACK_DIGEST_RING_TICKS`). Each pair names a tick: a tick already verified is skipped; a tick within the ring is verified, equal digests counting `net.proof.ack_verified` (distinct ticks) and different ones `net.proof.ack_mismatches`; a tick older than the ring counts `net.proof.ack_unverifiable`; a tick the server never captured is malformed.
- At the end of the session the server compares its distinct verified count with the last cumulative count it received: equal means every tick the client applied completely was verified (P3).
- Each first verification of a tick also yields a round-trip estimate (the send time of the tick's last fragment to the INPUT's arrival), kept as an exponential average (α = 1/8) in the gauge `net.session.rtt_ms`. Digests change no simulation state on either side.

## Proof mode

### Proof assertions

With `net.proof.mode` true (development builds; forced false in Shipping), NET checks the proof's claims inside each process and reports a failure through the runtime's test assertion sink, so the process exits with code 5 (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Exit codes"):

<!-- assumption: pending MOD -->
`Zenith_ModuleStartContext::GetTestAssertionSink()` returns `Zenith_TestAssertionSink*` (null in Shipping), whose `void RecordFailure(Zenith_StringView strReason)` is `ANY_THREAD`, logs the reason under `runtime.diagnostics` and forwards to `Zenith_Runtime::RecordTestAssertionFailure()` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Test assertion failures and world shutdown requests"); MOD confirms that a call from `~Zenith_NetModuleInstance` (shutdown step 6) still sets the exit code, the count being read after step 6 (row NT6).
<!-- /assumption -->

| # | Side | Assertion | When checked |
|---|---|---|---|
| P1 | server | `net.proof.ack_mismatches` is 0: every reported complete tick's received bytes equal the bytes the server encoded | each verification (fails at the first mismatch, with the tick) |
| P2 | server | at least `net.proof.minConnectedTicks` (300) session ticks ran with the peer connected | at the module instance's destruction |
| P3 | server | the distinct verified ticks equal the cumulative count of the last INPUT received, `net.proof.ack_unverifiable` is 0, and the verified count is at least half of the captures in the connected window, ⌊P2's count / `net.snapshotIntervalTicks`⌋ / 2 | destruction |
| P4 | server | `net.late_commands` is 0 and `net.server.lost_commands` is 0, when `net.proof.assertTiming` | destruction |
| P5 | both | `net.packets.malformed` and `net.publications.malformed` are 0 | destruction |
| P6 | server | `net.proof.capture_mismatches` is 0: every capture decodes to the quantisation of the authoritative properties of its tick (§ "Capture at the snapshot point" step 8) | each capture |
| C1 | client | at the start of every `Net.ClientApply`, every proxy property applied by the previous tick's apply reads back bit-equal to the dequantisation of the word applied (no other writer changed it) | every tick; `net.proof.apply_mismatches` |
| C2 | client | the session reached `CONNECTED` and at least `net.proof.minConnectedTicks` ticks applied at least one fragment | destruction |
| C3 | client | `net.client.proxy_not_resident`, `net.client.spawns_refused_map_full` and `net.ids.duplicates` are 0 | destruction |
| C4 | client | `net.proof.digests_unreported` is 0 | destruction |

Together P1, P3 and P6 state [Z2-N-ARCH-786]'s transform criterion for every tick the client holds completely: the server encoded the quantised authoritative state of tick T (P6), the client received exactly those bytes (P1) and every such tick was verified (P3), and C1 shows each proxy holds the decoded values. Each failure's reason is one line, for example `net.proof: P4 failed: net.late_commands=3`. Under `--replay` the transport-dependent checks (P1–P5, C2–C4) do not run; P6, C1 and the capture chain line do whenever the replay's command line sets `net.proof.mode` (a key outside the simulation-affecting list), because they are pure functions of simulation state.

### Proof outcome

The integration driver reads exit codes, the two replay logs and each process's development log as copied when the process exited (§ "Driver script" step 4): exit 0 from both processes means every assertion held. A failure prints the `net.proof`, `net.session` and `runtime.diagnostics` records of the failing process, read from DEVT's log file sink in the game's user root, `user:/devtools/logs/*.jsonl` (`Zenith2/Docs/Design/DEVT/Console.md` → "Log file sink"), together with the process's captured standard output and standard error (§ "Driver script" step 4).

## Loopback content

The proof drives `Games/WorkloadCharacters` (ANIM's workload game, selection `Animation`, `Gameplay`, `Net`, `Physics` at Stage 3; `Zenith2/Docs/Design/MOD.md` → "Workload games"). NET's Stage 3 content ticket adds these files to it, coordinated with ANIM through row NT10; nothing else of the workload game changes, and the W-Characters workload scene is untouched.

### Scene and prefabs

| File | Content |
|---|---|
| `Games/WorkloadCharacters/Scenes/NetLoopback.scene.json` | a ground plane with its collider; the player entity (`Zenith.Gameplay.Player`, slot 0) and its camera director; the entity `NetSession` carrying `Zenith.Net.ProxyPrefabSet` with entry 0 = `Prefabs/CharacterProxy.prefab.json`, and the behaviour `Scripts/Net/LoopbackDirector.luau` |
| `Games/WorkloadCharacters/Assets/Prefabs/CharacterProxy.prefab.json` | the character's skinned mesh, skeleton and animation graph in its idle state, `Zenith.LocalTransform`; no rigid body, no character controller, no root-motion graph, no behaviour, no `Zenith.Net.Identity` |
| the existing character prefab | gains `Zenith.Net.Identity` with property 2 = `CharacterProxy` (all its other components unchanged) |

### Loopback scripts

`Scripts/Net/LoopbackDirector.luau`, a Late behaviour on `NetSession` with declared state `{ Characters: fixed array of 64 entity handles, Spawned: bool, Owned: entity, PendingPeer: number }`:

| Role | Reaction |
|---|---|
| server, first tick | spawns 64 characters from the character prefab in an 8 × 8 grid at x, z ∈ {−7, −5, …, 7} m, y = 0, facing +Z, recording the 64 handles; `Spawned = true` |
| server, every tick | each character except index 0 walks a circle of radius 1.5 m about its spawn point at 1 m/s through the gameplay desired-velocity intent, so transforms change every tick deterministically; when `PendingPeer` is set and `Spawned` is true, performs the join reaction below and clears it |
| server, `Net.PeerJoined` | when `Spawned` is false (a join published at the first tick, before the spawn's sync point) stores the peer in `PendingPeer`; otherwise `Net.SetOwner(ctx, Characters[0], peer)` and `Gameplay.Possess(ctx, player, Characters[0])` (player slot 0 drives it with the peer's commands) |
| server, `Net.PeerLeft` | `Gameplay.Unpossess(ctx, player)` |
| client, `Net.ControlledEntityChanged` | `Gameplay.Possess(ctx, player, event.Entity)`, so the client's camera follows its proxy |
| offline | spawns the 64 characters and possesses index 0 itself (the content runs without a network) |

The scripts use only the bindings of `Zenith2/Docs/Design/NET/PublicAPI.md` → "Script-facing API" and GAME's gameplay bindings (`Zenith2/Docs/Design/GAME/Scripting.md` → "Binding functions").

### Input script

`Games/WorkloadCharacters/Tests/NetLoopback.input.json` (the format of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format", `exclusive` true): over 600 ticks, `Gameplay.Move` set to (0, 1) at tick 30, (1, 0) at 150, (0, −1) at 270, (−1, 0) at 390, (0, 0) at 510; `Gameplay.Jump` pressed at ticks 100, 300 and 500 each released at the 4th tick after its press. It yields 11 commands (five slice-end `Gameplay.Move` commands and six `Gameplay.Jump` edges), and every command is reachable by the comparison of § "Command log verification".

## The loopback suite

### Suite target

| Item | Value |
|---|---|
| directory | `Zenith2/Tests/NetLoopback/` |
| `CMakeLists.txt` | `zenith_add_integration_tests(NetLoopback DEPENDS ZenithSave ZenithPlatform ZenithNet ZenithCore SUPPORTED ZENITH_WINDOWS)` (the only line, [Z2-N-ARCH-157]) |
| executable | `Zenith_Integration_NetLoopback`: the log verifier (doctest, skipped cases launched by the driver) |
| driver | `Zenith2/Tests/NetLoopback/Run-NetLoopback.ps1` |
| registration | two CTest entries in the test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` (row NT11, owner MOD), on `win` test presets (`win-debug`, `win-release`), label `z2-net-loopback`, timeout 300 s, `FIXTURES_REQUIRED` on the cook fixture of `WorkloadCharacters` that the game's own boot tests require (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Game test registration"), `RUN_SERIAL TRUE` (no other test runs beside either entry, which gives Architecture's "idle machine" under the `Test` step's `ctest --parallel`; a `RESOURCE_LOCK` alone would serialise only its holders), each running `pwsh -NoProfile -File <driver> -GameExe $<TARGET_FILE:WorkloadCharacters> -Verifier $<TARGET_FILE:Zenith_Integration_NetLoopback> -AssetsRoot ${ZENITH_OUT}/Cooked/WorkloadCharacters/windows -OutDir ${ZENITH_OUT}/test-runs/NetLoopback/<config>/<name> -Case <name>`, so each case owns its output directory |

The two CTest entries are `Test_Net_LoopbackReplicatesTransformsAndCommands` (Architecture's test, [Z2-N-ARCH-786]) and `Test_Net_LoopbackLogsReplayBitIdentical`. Both run in the `Test` gate step on every change that rebuilds `WorkloadCharacters` or the suite.

### Driver script

`Run-NetLoopback.ps1 -GameExe <path> -Verifier <path> -AssetsRoot <dir> -OutDir <dir> -Case <name>`. Every launch passes the common arguments A = `--headless --assets-root <AssetsRoot> --set content.startupScene=content:/Scenes/NetLoopback.scene.json --set net.proof.mode=true --set devtools.log.file=true --timeout-seconds 120` (`devtools.log.file` pinned so the driver does not depend on the development default; the driver never raises `log.categoryLevels` for `net.proof` above INFO); none of these keys is simulation-affecting, so a replay is given them as the recording was. The two live launches also pass L = `--set sim.maxCatchUpTicks=8` (the server's limit of § "Snapshot send"); that key is simulation-affecting, so each replay takes it from its log header (`Zenith2/Docs/Design/JOBS.md` → "Configuration keys") and the replay lines do not repeat it:

1. Delete and recreate `<OutDir>` (the case's own directory); record the start time and list `user:/devtools/logs/`; choose a free UDP port p in 40,000 … 49,999 (bind-probe with `System.Net.Sockets.UdpClient` on 127.0.0.1, five attempts).
2. Start the server: `<GameExe> A L --set net.role=server --set net.port=<p> --record <OutDir>/server.zrpl --exit-after-ticks 600`.
3. Start the client at once: `<GameExe> A L --set net.role=client --set net.connect=127.0.0.1:<p> --record <OutDir>/client.zrpl --input-script <game>/Tests/NetLoopback.input.json --exit-after-ticks 600`.
4. Capture each process's standard output and standard error; wait for both (at most 150 s); a process still running is killed and the case fails. As each process exits, copy the `.jsonl` files that appeared in `user:/devtools/logs/` since its launch into `<OutDir>/logs/<role>/` at once, before another process's start can apply the sink's newest-eight-runs retention, and read only the copies from then on. Print both exit codes; either non-zero fails the case and prints that process's captured output and its `net.proof`, `net.session` and `runtime.diagnostics` records from the `user:/devtools/logs/*.jsonl` files of the game's user root (the host path of `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots") (the copies of step 4); the two processes write separate files, one stem per run.
5. Case `Test_Net_LoopbackReplicatesTransformsAndCommands`: run `<Verifier> --test-case=Test_NetLoopbackVerify_CommandLogs --no-skip` with the environment variable `ZENITH_NET_LOOPBACK_DIR=<OutDir>`; its exit code is the case's.
6. Case `Test_Net_LoopbackLogsReplayBitIdentical`: run `<GameExe> A --replay <OutDir>/server.zrpl` and `<GameExe> A --replay <OutDir>/client.zrpl`; each must exit 0 (a tick-hash mismatch is exit code 5 through SAVE's verifier, `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay verification"); each replay's new log files are copied into `<OutDir>/logs/replay-<role>/` as it exits; then compare the `net.proof: capture chain` line of the live server with that of the server replay: equal count and equal chain. Each server process's copies must hold exactly one chain line; none or more than one fails the case and prints the file names. The driver tells the processes' files apart by the listing before and after each launch (the two replays run one after the other) and by the role each line names.

The server records `net.role=server` and the client `net.role=client` in their replay headers (simulation-affecting), so each replay runs in its recorded role with no `--set net.role`; a client replay needs no `net.connect` (`Zenith2/Docs/Design/NET.md` → "Module start").

<!-- assumption: pending MOD, JOBS -->
P4 (no late or lost command) relies on headless frame pacing (row NT7). If MOD declines NT7, the driver passes `--set net.proof.assertTiming=false` on both live launches, and the verifier's checks of § "Command log verification" keep the command criterion.
<!-- /assumption -->

### Command log verification

`Test_NetLoopbackVerify_CommandLogs` (in `Tests/NetLoopback/Zenith_NetLoopbackVerify_Tests.cpp`, `* doctest::skip()`), with `Zenith_ReplayLogReader` (`Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header"):

1. Open both logs; both have one world session.
2. From the client log, find the first `ASYNC_SOURCE_NETWORK` publication whose payload is a CONNECTED session event; read its offset o and its publishing tick c0. None: fail ("client never connected").
3. Client-sent set: every input command of client ticks c with c ≥ c0, each mapped to target tick c + o; drop those whose target exceeds the server log's last tick minus `net.server.commandHoldTicks` (they could not have been applied before the server exited).
4. Server-received set: every input command of every server tick.
5. Assert the two sets are equal as sequences ordered by sequence number (action ID, sequence and value bytes equal), every sequence appears once, the first sequence equals the client's first sent sequence, and every command's server tick equals its target tick (no late command on an idle machine; P4 asserted it in-process).
6. Assert the server log contains at least 300 ticks inside [c0 + o, its last tick].

The transform half of Architecture's criterion is the in-process P1, P3, P6 and C1 of § "Proof assertions", carried by the exit codes.

### Replay verification

`Test_Net_LoopbackLogsReplayBitIdentical` replays each log offline in its recorded role: the server replay re-applies the logged remote commands and recomputes every capture; the client replay re-publishes the recorded fragments and session events and re-applies them. SAVE's verifier compares every tick hash with the recording; a mismatch is exit code 5. The tick hash covers simulation state (entities, components, events, played-back commands; `Zenith2/Docs/Design/ECS/Scheduling.md` → "World state hash"), not the capture resource, so the driver also compares the capture chain of the live server with its replay's (one line each, the hash of every capture's tick and digest in order); together they show that the simulation and every byte the server encoded replay identically. The case is the evidence for Z2-I-NET-04 (replayable commands) across a real network run.
