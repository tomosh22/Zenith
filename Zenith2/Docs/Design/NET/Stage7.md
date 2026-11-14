# NET design — Stage 7 networked multiplayer and dedicated servers

This file is part of the NET design family. The entry document is `Zenith2/Docs/Design/NET.md`. It specifies the deferred Stage 7 tree of Z2-D-073 in full, ticketed now: transport and session, replication, prediction and reconciliation, interest management, serialization, dedicated server builds, network simulation and diagnostics. Every Stage 7 mechanism builds on the Stage 1 foundations of `Zenith2/Docs/Design/NET/Foundations.md`, which do not change, and replaces the Stage 3 proof mechanisms of `Zenith2/Docs/Design/NET/Loopback.md` (transport version 1, the proof snapshot, single-peer possession).

Four selection and classification decisions govern this file, each open at Stage 7 and registered in `Zenith2/Docs/DecisionLog.md` → "Open decisions" through cross-doc item X397: D-SEL-NETTRANSPORT (open, Stage 7), D-NET-SESSION-AUTH (open, Stage 7), D-NET-PREDICTION-MODEL (open, Stage 7) and D-NET-SERVER-PLATFORM (open, Stage 7). Where this file specifies behaviour that a ruling selects, it is written for the default candidate named in "Selection decisions NET owns" of the entry document, and the Stage 7 revalidation ticket for (NET, Stage 7, those four decisions) rewrites the dependent tickets after the rulings.

## Scope of this file

| Inventory item | Headings |
|---|---|
| Z2-I-NET-07 transport/session | "Transport and session" and its sub-sections |
| Z2-I-NET-08 replication | "Replication" and its sub-sections |
| Z2-I-NET-09 prediction/reconciliation | "Prediction and reconciliation" and its sub-sections |
| Z2-I-NET-10 interest management | "Interest management" and its sub-sections |
| Z2-I-NET-11 serialization | "Serialization" and its sub-sections |
| Z2-I-NET-12 dedicated server builds | "Dedicated server builds" and its sub-sections |
| Z2-I-NET-13 network simulation | "Network simulation" and its sub-sections |
| Z2-I-NET-14 diagnostics | "Network diagnostics" and its sub-sections |
| all eight | "Stage 7 state machines", "Stage 7 tests" |

## Stage 7 architecture overview

```text
 client process                                          server process (dedicated or listen)
 ┌──────────────────────────────┐                        ┌──────────────────────────────────────┐
 │ phase 1  Input.Actions ──────┼─ ObserveTickCommands ─▶│ (INPUT v2, channel UNRELIABLE)        │
 │          Net.Predict (rerun) │                        │ phase 1  Input.Actions per peer slot  │
 │          Net.ClientApply ◀───┼── SNAPSHOT v2 deltas ──│ phase 11 Net.Replicate (per peer)     │
 │ tick boundary: publications  │   CREATE/DESTROY (REL) │ interest grid, priority, bandwidth    │
 │ render: interpolated proxies │◀─ SESSION control ────▶│ transport backend (D-SEL-NETTRANSPORT)│
 └──────────────────────────────┘                        └──────────────────────────────────────┘
```

| Concern | Stage 3 | Stage 7 |
|---|---|---|
| transport | `Zenith_NetTransport`, wire version 1, loopback only, one peer | `Zenith_NetTransportBackend` over the selected library, wire version 2, any interface, 64 peers |
| sessions | unauthenticated, fixed offset | challenge-response, authenticated per D-NET-SESSION-AUTH, clock synchronisation |
| state to clients | every entity every tick, proof codecs | per-peer relevant entities, deltas against acknowledged baselines, generic codecs, priority under a bandwidth budget |
| input to server | one peer drives slot 0 | each peer drives its own player slot |
| client simulation | proxies apply server state only | owned entities predicted and reconciled; remote proxies interpolated |
| process shape | the game's Player headless | also a dedicated server executable |

Invariants kept from Stage 3: input reaches a server world only as phase-1 input commands logged in its replay; state reaches a client world only as recorded `ASYNC_SOURCE_NETWORK` publications applied by phase-1 commands; network IDs, the authority table and the per-world map are unchanged.

## Transport and session

### Transport selection

D-SEL-NETTRANSPORT (open, Stage 7) chooses the library under `Zenith_NetTransportBackend`:

| Candidate | Licence | Reliability, fragmentation | Encryption | Threading | Notes |
|---|---|---|---|---|---|
| 1. NET's own reliability layer over `Zenith_NetDatagramPort` (the Stage 3 transport extended per the sub-sections below) | Zenith2 | per "Channels and reliability" and "Fragmentation and reassembly" | through the D-NET-SESSION-AUTH primitive | none of its own: prologue and `Net.Send` only | no new dependency |
| 2. Valve GameNetworkingSockets | BSD-3-Clause | reliable and unreliable messages, fragmentation, congestion control | built in (AES-GCM, Curve25519) | one service thread | protobuf and OpenSSL or libsodium dependencies |
| 3. ENet | MIT | reliable and unreliable channels, fragmentation | none | none | lightweight; encryption would be NET's |
| 4. yojimbo with netcode | BSD-3-Clause | reliable messages over netcode connections | built in (libsodium) | none | connect tokens fit D-NET-SESSION-AUTH candidate 2 |

Criteria: builds with the pinned toolchains for Windows and Android arm64 (NDK r27c, 16 KB pages); confined to `ZenithNet/Internal` (Z2-D-082); a thread, if any, is joined in shutdown step 6 and never blocks a job (Z2-D-006); a port injection point so `Zenith_NetSimulatedPort` and the memory port work underneath it; per-connection memory; CPU per 1,000 messages on the S24; licence. Acceptance rule: the transport conformance suite `ZenithNet_TransportConformance` (§ "Stage 7 tests") passes under all four simulation profiles; among passing candidates, the one with the lowest median S24 CPU per 1,000 messages over 10 repetitions wins, a third-party candidate only when it is at least 20% cheaper than candidate 1 or supplies the encryption the D-NET-SESSION-AUTH ruling requires and candidate 1 lacks; candidate 1 wins ties. The harness is an ordinary ticket under `Zenith2/Benchmarks/NetTransport/`; the ruling is a `needs-human` ticket that runs it on the reference hardware and commits `Zenith2/Docs/Research/NetTransport/` (Z2-D-084).

### Transport layer contract

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetTransportBackend.h (Stage 7)
enum Zenith_NetChannel : uint8
{
	NET_CHANNEL_UNRELIABLE = 0,           // INPUT v2, KEEPALIVE: newest wins, no resend
	NET_CHANNEL_UNRELIABLE_SEQUENCED = 1, // SNAPSHOT v2: older than the newest received is dropped
	NET_CHANNEL_RELIABLE_ORDERED = 2,     // CREATE, DESTROY, session control, string table, replicated events
};

struct Zenith_NetTransportEvent
{
	uint8 m_eKind = 0;                   // 1 CONNECTED, 2 DISCONNECTED, 3 MESSAGE
	Zenith_NetChannel m_eChannel = NET_CHANNEL_UNRELIABLE;
	uint16 m_uPeer = 0;
	uint8 m_uReason = 0;                 // Zenith_NetDisconnectReason for DISCONNECTED
	uint8 m_auReserved[3] = {};
	Zenith_ByteSpan m_xMessage;           // MESSAGE: valid until the next Poll
};

/// @contract interface Zenith_NetTransportBackend
/// Ownership: Created by the selected backend's factory inside ZenithNet/Internal; owned by the module instance.
/// Thread affinity: EXTERNALLY_SYNCHRONISED: Poll in frame-prologue step 1, Send and Flush in Net.Send, Disconnect in either
///   and at shutdown; never concurrently. A backend's own service thread, if any, never calls into ZenithNet.
/// Invalidation: Event message spans are valid until the next Poll.
/// Units: Bytes; milliseconds of the monotonic clock.
/// Errors: NET_ERROR_CHANNEL_FULL, NET_ERROR_RELIABLE_WINDOW_FULL, NET_ERROR_LIMIT (a message above 65,536 bytes),
///   NET_ERROR_TRANSPORT_BACKEND (the library's own failure, its code in m_uOsCode), NET_ERROR_PORT_CLOSED.
/// Reentrancy: NOT_APPLICABLE. Poll returns events; it invokes no callbacks.
/// Version: since Z2 Stage 7.
/// @endcontract
class Zenith_NetTransportBackend
{
public:
	virtual ~Zenith_NetTransportBackend() = default;
	virtual Zenith_Result<void, Zenith_NetError> Listen(const Zenith_NetListenDesc& xDesc) = 0;
	virtual Zenith_Result<void, Zenith_NetError> Connect(const Zenith_NetConnectDesc& xDesc) = 0;
	virtual uint32 Poll(uint64 ulNowMs, Zenith_Span<Zenith_NetTransportEvent> axOut) = 0;
	virtual Zenith_Result<void, Zenith_NetError> Send(uint16 uPeer, Zenith_NetChannel eChannel, Zenith_ByteSpan xMessage) = 0;
	virtual void Flush(uint64 ulNowMs) = 0;
	virtual void Disconnect(uint16 uPeer, Zenith_NetDisconnectReason eReason) = 0;
	virtual Zenith_NetPeerStatistics GetStatistics(uint16 uPeer) const = 0;
};
```

`Zenith_NetListenDesc` holds the bind address (IPv4 or IPv6 through `Zenith_NetAddress` widened to 20 bytes with a family byte at Stage 7), the port, `net.maxPeers` (1 … 64) and the authentication descriptor of § "Session security"; `Zenith_NetConnectDesc` holds the server address and the join credential. The Stage 7 PLAT socket extension (row NT1 (d)) lifts `m_bLoopbackOnly` and adds IPv6.

### Connection handshake version 2

Wire format version 2 (§ "Wire format version 2") replaces version 1's single request and answer with a stateless challenge, so a spoofed source address cannot make the server allocate a slot or send more bytes than it received:

| Step | Message | Body | Server state |
|---|---|---|---|
| 1 | client → `CONNECT_REQUEST_V2` | nonce (8), protocol version (2), the identity fields of version 1 (build, composition, content, backends, tick rate, table hashes; 64), padding to 1,000 bytes | none |
| 2 | server → `CHALLENGE` | nonce echo (8), cookie (32): HMAC-SHA-256 (RFC 2104 over CORE's `Zenith_Sha256`, `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256"; no library of the D-NET-SESSION-AUTH ruling is needed for it, whichever candidate wins) of (client address, client nonce, 30-second epoch) under a 32-byte server secret drawn at start from `FillOSRandom`; at most 64 bytes in total | none |
| 3 | client → `CHALLENGE_RESPONSE` | nonce (8), cookie (32), join credential (D-NET-SESSION-AUTH; at most 1,024 bytes) | the cookie is recomputed for this and the previous epoch; a match allocates a slot |
| 4 | server → `CONNECT_ACCEPT_V2` | version 1's fields plus the peer's player slot, the session's string table generation and the server's tick at send | `CONNECTED` |

Rate limits: at most 10 requests per second per source address and 1,000 per second in total, beyond which requests are dropped and counted in `net.handshake.rate_limited`; a source with 8 failed cookie checks in 10 s is ignored for 30 s.

### Channels and reliability

Candidate 1's reliability layer (the default; the others map their own mechanisms onto the same channel semantics):

- Every datagram carries a 16-bit sequence, a 16-bit ack (the newest sequence received) and a 32-bit ack bitfield of the 32 sequences before it; sequence comparison is modulo 2^16 with the half-range rule.
- RTT: on each newly acked sequence, sample = now − send time; `rtt = rtt + (sample − rtt) / 8`, `rttvar = rttvar + (|sample − rtt| − rttvar) / 4`; retransmission timeout `rto = max(50 ms, rtt + 4 × rttvar)`.
- Reliable-ordered messages carry a 16-bit message ID; the send window is 256 unacked messages per peer (`NET_ERROR_RELIABLE_WINDOW_FULL` beyond it, which the caller treats as a peer that cannot keep up: disconnect with `TIMEOUT` after `net.peerTimeoutMs`); a message is resent when its datagram is not acked within `rto`, at most every `rto`, in ascending ID; the receiver delivers in ID order, buffering out-of-order messages (window 256).
- Unreliable-sequenced: the receiver drops a message older than the newest delivered on that channel.
- Several messages are packed into one datagram up to 1,200 bytes; reliable resends are packed before new messages.

### Fragmentation and reassembly

A message above one datagram's payload (1,100 bytes after headers) is split into fragments `{ fragment group ID u16, index u8, count u8 }`, count at most 60 (65,536 bytes); unreliable groups are dropped whole if any fragment misses within 1 s, reliable groups resend missing fragments; at most 4 groups per peer are reassembled at once (`NET_ERROR_REASSEMBLY` for a fifth, which is dropped); reassembly memory is 4 × 65,536 bytes per peer.

### Congestion and bandwidth control

Each peer has a send budget, a token bucket of `net.peerBandwidthKiBps` (32 … 4,096, default 256) KiB/s with a 100 ms burst; on measured loss above 5% over 1 s, or RTT above twice its 10-second minimum, the budget halves (not below 32 KiB/s) and recovers by 10% per second after 2 s without loss. Reliable traffic is sent before snapshot traffic, which fills what remains (§ "Priority and bandwidth allocation"). The budget never changes simulation: it decides what is sent, and what is received is recorded.

### Session lifecycle

| Server state | Meaning |
|---|---|
| `STARTING` | the module started; the socket is not yet bound |
| `LISTENING` | accepting joins |
| `SHUTTING_DOWN` | every peer sent DISCONNECT (`SERVER_SHUTDOWN`); no join is accepted |

Peer states (server side): `FREE` → `CHALLENGED` (cookie sent; no slot held) → `CONNECTED` → `LINGER` (1 s) → `FREE`. Client states: `OFFLINE` → `CONNECTING` → `CHALLENGED` → `CONNECTED` → `DISCONNECTED`, or `FAILED` from `CONNECTING` and `CHALLENGED`. Unlike Stage 3, a client may reconnect: `DISCONNECTED` → `CONNECTING` on a script call `Net.Connect(ctx, address)` (behaviour context, buffered as a module command, refused on a server). The command's playback only records the request in module memory; the next prologue starts the handshake and publishes a `RECONNECTING` session event (value 7), and `Net.Session`, applying that recorded event, is the writer of the world's `CONNECTING` — so the transition has a recorded writer like every other. The full tables are § "Stage 7 state machines".

### Peer slots and join in progress

- A server holds `net.maxPeers` slots (1 … 64). Peer indices are issued as at Stage 3 (`Zenith2/Docs/Design/NET/Foundations.md` → "Peer index issuance").
- A peer that joins a running session receives, on the reliable channel and before any snapshot, the string table (§ "Reference encoding"), then one CREATE per relevant entity with its full state as baseline 0; its first snapshot is a delta against baseline 0.
- A listen server (a windowed game that is also the server) is a server whose local player is slot 0 with peer index 0; it needs no extra mechanism, because its local input is already phase-1 input: its world's bridge answers `ReplacesLocalInput()` false for peer 0's local player (`Zenith2/Docs/Design/NET/Foundations.md` → "Command source contract"), and remote peers' commands are added to their own slots.

### Clock synchronisation

The Stage 3 fixed offset becomes an estimated one:

1. Every SNAPSHOT v2 carries the server tick and the server's receive time of the newest INPUT it processed from this peer.
2. The client estimates the server's current tick as `snapshot tick + (rtt / 2) / tick duration` and keeps a 16-sample median of the lead `target(c) − estimate`.
3. The target lead is `ceil((rtt / 2 + 2 × jitter) / tick) + 1` ticks, at least 1, at most 16.
4. When the median lead differs from the target by more than one tick for 1 s, the client shifts its simulation clock's accumulator by at most 2 ms per frame (`Zenith_SimulationClock::NudgeAccumulatorUs(sint64)`, JOBS, row NT21), so its ticks run slightly early or late; tick duration never changes, so simulation stays fixed-tick and replays reproduce it (the nudges change only which frame a tick runs in).
5. The offset o is then recomputed as the lead plus the current estimate, in the prologue, and published as an `OFFSET_CHANGED` session event: event value 6 in the 28-byte session event of `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication", the new offset in the `sint64` at offset 4, the first client tick it applies to in the `uint64` at offset 20, every other field 0; `Net.Session` writes it into `Zenith.Net.WorldState`, and every simulation read of the mapping c(T) (reconciliation, the target ticks of INPUT blocks' recorded copies) reads that field, so a client replay maps ticks exactly as the live run did. Blocks keep their sequence numbers, so a changed offset never reorders commands (the window releases by sequence).

### Session security

D-NET-SESSION-AUTH (open, Stage 7) decides how a server knows who joined and how datagrams are protected:

| Candidate | Credential | Protection |
|---|---|---|
| 1. platform identity tickets | a Steam authentication session ticket or a Play Games server auth code, obtained through ONLINE (row NT18) and validated by the server through the same backend | AEAD keys from an X25519 exchange inside the handshake |
| 2. signed join tokens | an Ed25519-signed token `{ server address, expiry, player key, session key }` issued by a matchmaking or hosting service outside the engine; the server holds the public key | AEAD (XChaCha20-Poly1305) under the token's session key |
| 3. direct connect with a session password | a password the host sets; a SPAKE2-style exchange derives keys | AEAD under the derived key |

Criteria: works without a game-specific backend service for the flagship slices; no secret stored in the client package; licence and audit status of the cryptographic library (a pinned, audited library admitted to the manifest by the ruling, as `Zenith2/Docs/Architecture.md` → "Security architecture" requires for update signatures). The ruling is an analysis ruling with no measurement harness, so per-packet cost is not a criterion; the chosen library's cost is measured afterwards against the Stage 7 NET rows. Acceptance rule: candidate 2 by default; candidate 1 is adopted only if ONLINE's backends expose ticket validation for both stores; candidate 3 only for LAN sessions in addition to the chosen one. Whatever is chosen, Stage 7 Shipping builds accept a non-loopback session only when it is authenticated and encrypted; Stage 3's Shipping guard (`Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles") is lifted in the same diff as the ruling's implementation, and the parser limits, rate limits and the 8-malformed-packets rule stay.

### Transport and session error codes

| Code | Meaning |
|---|---|
| `NET_ERROR_AUTH_FAILED` | the join credential was invalid, expired or for another server |
| `NET_ERROR_CHALLENGE_FAILED` | the cookie did not match this or the previous epoch |
| `NET_ERROR_CHANNEL_FULL` | a channel's send queue (1,024 messages) is full |
| `NET_ERROR_RELIABLE_WINDOW_FULL` | 256 reliable messages are unacknowledged |
| `NET_ERROR_REASSEMBLY` | a fifth fragment group, or a group that timed out |
| `NET_ERROR_BANDWIDTH` | a single reliable message larger than one second of the peer's budget |
| `NET_ERROR_RATE_LIMITED` | the handshake rate limit dropped a request |
| `NET_ERROR_TRANSPORT_BACKEND` | the selected library reported a failure |

## Replication

### Replication model

Server-authoritative state replication: each server tick that is a send tick for a peer (every `net.snapshotIntervalTicks`), `Net.Replicate` (phase 11, read-only, writing the `Zenith.Net.Capture` resource) builds for that peer a delta snapshot of the entities relevant to it (§ "Relevancy sets per peer"), each entity's replicated properties encoded against the newest baseline the peer acknowledged, ordered by priority and cut at the peer's budget. Creates and destroys travel on the reliable channel; snapshots on the unreliable-sequenced channel. The client publishes every received message as recorded publications and applies it in phase 1, as at Stage 3, in pieces (§ "Message publication in pieces").

### Message publication in pieces

A Stage 7 message is up to 65,536 bytes, and a module publication at most 1,024 (`uMODULE_PUBLICATION_MAX_RESULT_BYTES`, `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Allocation sites and acceptance"); NET splits the message inside its own payload into pieces of at most 1,020 bytes behind the 4-byte prefix (a 4-byte piece header and up to 1,016 data bytes), so the sink's limit and arena are unchanged:

| Offset | Type | Field |
|---|---|---|
| 0 | 4 bytes | the Stage 3 prefix with record type 3 (message piece) and record version 1 |
| 4 | `uint16` | message ID: the client's per-session counter of published messages, wrapping |
| 6 | `uint8` | piece index, 0 … count − 1 |
| 7 | `uint8` | piece count, 1 … 65 |
| 8 | ≤ 1,016 bytes | the message's bytes from index × 1,016 |

- A 65,536-byte message is ⌈65,536 / 1,016⌉ = 65 pieces (the last one 512 bytes); every publication stays at most 1,024 bytes, so the replay log's records are the Stage 3 size.
- The client's prologue reserves a message's piece count against its own in-flight count (NET is the kind's only allocator, so it knows the outstanding IDs against the declared `m_uMaxInFlight`) before allocating any of its IDs, then allocates and parks every piece in the same prologue call; a refused reservation leaves a reliable message in the transport's receive queue for the next frame and drops an unreliable one (`net.client.messages_deferred`, `net.client.messages_dropped_full`). So every piece of a message reaches the same tick boundary, and no completion ID is left unparked ([Z2-N-SCRIPT-931]).
- The phase-1 intake (`Net.ClientApply`) reassembles each message from the inbox in completion-ID order before decoding it; a message whose pieces are not all present, out of order or inconsistent (a recorded log edited by hand) is dropped and counted (`net.publications.malformed`).
- The server's sink carries Stage 7's server-side publications: at most one `PEER_VIEW` per peer per frame (a frame that processes several INPUT v2 of one peer publishes only the newest view tick) and each owner-client proposal record of an INPUT v2 (at most 16 per INPUT); a proposal record above 1,016 bytes is published as message pieces like any message. The server declares `m_uMaxInFlight` = 64 + `net.maxPeers` × 17 (1,152 at 64 peers, 1.125 MiB of the 8 MiB arena) instead of Stage 3's 64, with the same session-event reserve.
- `net.client.maxFragmentsInFlight` keeps its range (at most 4,096 pieces, 4 MiB of the 8 MiB publication arena); the Stage 7 revalidation ticket sets its default from the measured traffic of `Net.Replicate` (row N7 of `Zenith2/Docs/Design/NET.md` → "Pending row needs").


### Entity scopes and relevance hand-off

| Event for peer p | Message | Client action |
|---|---|---|
| entity becomes relevant to p (or p joins) | `CREATE { network ID, proxy index, owner, full state }` (reliable) | spawn the proxy |
| entity stops being relevant to p | `DESTROY { network ID, reason OUT_OF_SCOPE }` (reliable) | destroy the proxy |
| entity destroyed on the server | `DESTROY { network ID, reason DESTROYED }` (reliable) | destroy the proxy |
| state of a relevant entity | delta in SNAPSHOT v2 | `SetProperty` commands |

A snapshot never names an entity whose CREATE the peer has not acknowledged; a CREATE is always ordered before the first state of its entity.

### Entity create and destroy

CREATE's full state is every table entry present on the entity, encoded with the generic codecs (§ "Property codecs"); the client's proxy prefab is the proxy table entry, as at Stage 3, extended to 255 entries (one byte). DESTROY is idempotent on the client. A destroy and a create of the same network ID never both happen, because IDs are never reused.

### Property replication

- The replication table grows to every replicated property of every component (`Zenith2/Docs/Design/NET/Foundations.md` → "Replication table construction" with the Stage 3 VEC3/QUAT restriction lifted), at most 1,024 entries, each with a codec of § "Property codecs".
- Change detection: per peer and entity, a 1,024-bit dirty set; `Net.Replicate` marks an entry dirty when the property's row change version (`Zenith2/Docs/Design/ECS/Queries.md` → "Row change bits and CommitRowChanges") is newer than the version recorded when the peer's acknowledged baseline was captured; a quantised value equal to the baseline's is not sent even when dirty.
- Arbitrated properties replicate their arbitrated result, never a staging column.

### Delta compression against acknowledged baselines

- The server keeps, per peer, a ring of the last 32 sent snapshots (snapshot ID u16, the per-entity quantised values it contained).
- The client acknowledges the newest snapshot ID it applied in every INPUT v2.
- An entity's record in snapshot n is encoded against its values in the peer's acknowledged snapshot b (the newest acknowledged snapshot that contained the entity), as a per-entry presence bit followed by the codec's delta form (§ "Property codecs"); with no acknowledged baseline in the ring, against the CREATE state.
- Memory: 32 snapshots × relevant entities × table bytes per peer, capped by `net.server.baselineBytesPerPeer` (default 4 MiB); beyond the cap the oldest snapshots are dropped first.

### Priority and bandwidth allocation

Per peer and relevant entity, an accumulator a: each send tick `a += priority × ticks since last sent`, priority = `base × distance factor × owner factor`, base 1 (a component may declare `Zenith.Net.Priority` with property `Base`, 0.1 … 16), distance factor `1 / (1 + (d / 20 m)²)` (d to the peer's controlled entity, computed with `+ − × ÷` only), owner factor 4 for the peer's own entities. Entities are encoded in descending a (ties by ascending network ID) until the peer's per-tick byte budget (`budget / tickHz`) is spent; an encoded entity's a resets to 0. The order is deterministic for given server state, budget and acknowledgements.

### Owner-client authority

For components declaring `NETWORK_AUTHORITY_OWNER_CLIENT`:

1. The owning client's world writes them itself (its systems run on its owned entities).
2. Each tick the client appends to INPUT v2 a proposal record per owned entity: the entity's network ID and the generic encoding of its OWNER_CLIENT replicated properties.
3. The server's `Net.ApplyProposals` (phase 1, after `Input.Actions`) validates each proposal with the component's registered validator (`Zenith_NetProposalValidator`, a function pointer a module registers per component type through the Net module's start context extension; the default accepts a change no larger than the property's declared `maxDeltaPerTick`), and records `SetProperty` for accepted values; a rejected proposal is counted (`net.server.proposals_rejected`) and the server's value is sent back to the owner as a correction.
4. Proposals reach server simulation as recorded publications of the server world (they are external input), so the server's replay reproduces them.

### Replicated events

`Net.Broadcast(ctx, eventName, fields, target)` (server behaviours and systems; buffered module command `Net.Broadcast`; § "Stage 7 bindings"): at playback the server encodes a registered event type's fields by reflection and sends it on the reliable channel to `"All"`, `"Owner"` (the owner peer of an entity argument) or a peer index; the receiving client publishes it and `Net.ClientApply` emits the event into its world in phase 1. Client-to-server gameplay intent is never an event: it is input commands or owner-client proposals.

### Stage 7 bindings

Stage 7 adds three functions to the nine of `Zenith2/Docs/Design/NET/PublicAPI.md` → "Script-facing API"; the Stage 7 revalidation ticket writes their signature rows and misuse texts in the same format, inside the codes ZS3405 and above of NET's block (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Misuse codes"):

| Luau function | Contexts | Effect | Returns or does |
|---|---|---|---|
| `Net.Connect(ctx, address)` | behaviour | buffered (module command `Net.Connect`) | nothing; a client in `DISCONNECTED` or `FAILED` reconnects (§ "Session lifecycle"); refused on a server |
| `Net.Broadcast(ctx, eventName, fields, target)` | system, behaviour | buffered (module command `Net.Broadcast`) | nothing; server only (§ "Replicated events") |
| `Net.RewoundRaycast(ctx, peer, origin, direction, length)` | system, behaviour | read world (`Zenith.Net.WorldState`'s recorded view ticks and the rewind history copy) | a hit table `{ Entity, Position, Normal, Distance }`, or `nil`; server only (§ "Prediction limits") |

### Player slots and per-peer input

- Peer p's commands are applied to player slot p on the server (peer 0, a listen server's local player, is slot 0). The server's intake rewrites each command's player slot from 0 to p; the client still records slot 0.
- PLAT's simulation input keeps one action-state table per player slot (row NT17, pending PLAT at Stage 7), and `Input.*` bindings read the table of the calling entity's owning player.
- GAME creates one `Zenith.Gameplay.Player` per connected peer with `PlayerSlot = p`, through its Stage 7 requirement "Player slots and ownership under replication" (Z2-R-GAME-137), and maps an owning player to `OwnerPeer` = p.

### Replication with world transitions

A server-initiated world transition sends `TRANSITION { target scene persistent ID, transition ID }` reliably; clients load the target scene (proxies only), destroy every proxy of the old world, reply `TRANSITION_READY`; the server keeps the session and its peer slots, begins the new world's replication with CREATEs once a peer is ready, and holds that peer's commands until then. The session world binding moves with the input world.

## Prediction and reconciliation

### Prediction model selection

D-NET-PREDICTION-MODEL (open, Stage 7):

| Candidate | Mechanism | Cost on a correction at 150 ms RTT (9 ticks) |
|---|---|---|
| 1. owned-entity re-simulation | the client simulates its owned entities ahead; on a correction it restores those entities' predicted state to the server's at tick T and re-runs phases 2–8 for them alone for T+1 … now, against a frozen copy of everything else | 9 × (owned entities' systems) |
| 2. full-world rollback | the client keeps a per-tick out-of-tick capture of its whole world (SAVE's out-of-tick snapshot) and on a correction restores tick T and re-runs every phase for 9 ticks | 9 × the whole world |
| 3. controller-only prediction | only `Zenith.Physics.CharacterController` movement is predicted, by PHYS's controller sweep run ahead with buffered commands | 9 controller sweeps |

Criteria: S24 CPU at 150 ms RTT within the Stage 7 NET prediction row; misprediction rate on the deterministic scenario `PredictionCorridor` (the scene `Games/WorkloadCharacters/Scenes/PredictionCorridor.scene.json`: one possessed character running a scripted 60-second route of straight runs, 90° turns, jumps and two moving obstacles, driven by `Tests/PredictionCorridor.input.json`; authored by the Stage 7 prediction harness ticket and run by the suite `Zenith2/Tests/NetPrediction/` through the simulated port's profiles); intrusion into other areas (systems must be re-runnable for a subset of entities); interaction with the determinism guarantee (a client's replay must still reproduce its run). Acceptance rule: candidate 1 by default; candidate 2 only if candidate 1's re-run subset cannot express a flagship-slice mechanic and candidate 2 fits the row; candidate 3 only if neither fits. The text below is written for candidate 1.

### Predicted entities

- An entity is predicted on a client when its owner peer is the local peer and it carries `Zenith.Net.Predicted` (a NET component a game adds to a prefab; properties: `MaxPredictedTicks` 1 … 32, default 16).
- The client's world simulates a predicted entity with its own commands each tick, exactly as the server will; its replicated properties are not overwritten by snapshots directly but go through reconciliation.
- The prediction sub-schedule is the systems of phases 2–8 whose query includes the predicted entity's components; a system opts out with `Zenith_SystemBuilder::NotPredicted()` (ECS extension, row NT19 (b)).

### Command acknowledgement

Every SNAPSHOT v2 tells each peer the newest command sequence the server applied for it (`last applied sequence`) and the server tick at which it was applied. The client keeps its commands in a ring of 64 ticks and discards those at or below the acknowledged sequence.

### Reconciliation algorithm

```text
on snapshot for server tick T with server values S for predicted entity e:
  P = the client's predicted values of e at the client tick c(T) mapped to T (history ring, 64 ticks; c read from the recorded offset)
  if every component of P is within the declared precision of dequantise(S): done   // compared in the decoded domain
                                                                                     // (rotation: within the codec's angular bound)
  else:
    net.client.mispredictions += 1
    write S into e's replicated properties (prediction world copy)
    for t in c(T)+1 … current tick − 1: re-run the prediction sub-schedule for e with the buffered commands of t
    record the visual error offset = old predicted pose − new predicted pose (presentation only)
```

The re-run happens inside phase 1 of the current tick (`Net.Predict`, after `Net.ClientApply`), so the rest of the tick sees the corrected prediction; re-runs are bounded by `MaxPredictedTicks`.

### Correction smoothing

The visual error offset decays as `offset × 0.5^(dt / 50 ms)` per frame in presentation (render extraction adds it to the entity's extracted transform; it never enters simulation state), and is dropped when below 1 mm and 0.1°.

### Remote entity interpolation

Remote proxies (not predicted) are presented at `estimated server time − interpolation delay`, delay = `2 × snapshot interval + jitter`, interpolating between the two buffered snapshot states around that time (positions linearly, rotations by normalised lerp); presentation reads SCENE's interpolation data path (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Interpolation data"); missing data is extrapolated for at most 250 ms, then the proxy holds its last state.

### Prediction limits

- A client never predicts more than `MaxPredictedTicks` ahead of the newest acknowledged command; beyond it, it stops sending new commands for the entity and counts `net.client.prediction_stalls`.
- History memory per predicted entity: 64 ticks × its replicated properties' bytes; `NET_ERROR_PREDICTION_HISTORY` when the configured cap (`net.client.predictionHistoryBytes`, default 1 MiB) is exceeded, and the entity falls back to interpolation.
- Server-side rewind for hit queries: the server keeps 32 ticks of transform history for entities flagged `Zenith.Net.Rewindable`; a game script queries `Net.RewoundRaycast(ctx, peer, origin, direction, length)`, which tests against the poses at the peer's view tick through PHYS's query API on a history copy, never the live world. The view tick derives from the network clock, so it reaches simulation only as a recorded publication: for each INPUT v2 it processes from peer p, the server's prologue publishes a `PEER_VIEW` record `{ peer, view tick }` (record type 4, 16 bytes behind the prefix), `Net.Session` writes the newest view tick per peer into `Zenith.Net.WorldState`, and `Net.RewoundRaycast` reads it there; a server replay therefore answers every query as the live run did.

## Interest management

### Relevance grid

The server keeps a uniform grid over world XZ of `net.interest.cellSizeM` (8 … 256, default 32 m) per session world, rebuilt each send tick from replicated entities' world positions in O(n); cell coordinates are `floor(x / size)` as `int32`.

### Relevancy sets per peer

An entity is relevant to peer p when any of:

1. its owner peer is p;
2. it carries `Zenith.Net.AlwaysRelevant`;
3. it lies within `net.interest.radiusM` (16 … 2,000, default 150 m) of p's controlled entity, tested cell first (cells within the radius) then by exact distance;
4. it is a hierarchy descendant of a relevant entity.

At most `net.interest.maxRelevantPerPeer` (default 2,048) entities are relevant; beyond it the nearest are kept (`NET_ERROR_INTEREST_CAPACITY`, counted).

### Hysteresis and priority

An entity becomes relevant at radius R and stops at 1.2 R, after 2 s outside, so an entity on the boundary does not churn CREATE/DESTROY; relevance changes per peer are capped at 64 CREATEs per send tick (the rest wait, nearest first).

### Streaming integration

A server keeps streamed cells resident around every connected peer's controlled entity, not only the local camera: NET passes up to `net.maxPeers` streaming viewers to STREAM (row NT16), so server simulation never runs in an unloaded cell a client stands in. Clients stream around their own camera as in single-player.

## Serialization

### Wire format version 2

Magic `ZNP2` (`0x32504E5A`), protocol version 2; the packet header of version 1 widened by the reliability fields of § "Channels and reliability" (sequence u16, ack u16, ack bits u32) and, after the handshake, an AEAD tag of 16 bytes per D-NET-SESSION-AUTH. Messages inside a datagram are framed `{ u8 kind, u8 channel, u16 length }`. Version 1 and version 2 peers never interoperate: a version-2 server answers a version-1 request with a version-1 `CONNECT_REJECT` `VERSION`, and vice versa.

### Bit writer and reader

`Zenith_NetBitWriter` and `Zenith_NetBitReader` (`Internal/Zenith_NetBits.h`): least-significant-bit-first packing into little-endian 32-bit words; `WriteBits(uint32 uValue, uint32 uBits)` for 1 … 32 bits, `WriteVarUInt`, `WriteZigZag`; the reader checks every read against the remaining bits and returns `NET_ERROR_MALFORMED_PACKET` beyond them, never reading past the buffer. Both are deterministic: the same values give the same bits on every platform.

### Property codecs

| Value type | Full form | Delta form (against baseline) |
|---|---|---|
| BOOL | 1 bit | 1 bit (sent only when different) |
| I8 … I64, U8 … U64, ENUM | ranged bits when the property declares a range (⌈log2(max − min + 1)⌉), else zig-zag varint | zig-zag varint of the difference |
| F32, F64 | quantised to the declared precision over the declared range (bits = ⌈log2((max − min) / precision + 1)⌉), else 32 or 64 raw bits | the quantised difference as a zig-zag varint |
| VEC2, VEC3, VEC4 | per component as F32 (a position property with no declaration uses the Stage 3 codec: 1 mm over ±8,388.607 m; a larger world declares its range, § "Replication precision declarations") | per component |
| QUAT | smallest three at the declared bits per component (default 10, range 8 … 15) | full form when changed |
| COLOUR | 4 × 8 bits (UNORM) | full form when changed |
| STRING_ID | the session string table index (§ "Reference encoding") | full form when changed |
| PERSISTENT_ID | 128 bits | full form when changed |
| ENTITY | the referenced entity's network ID as varint (0 for null or an unnetworked entity) | full form when changed |
| ASSET | the session asset table index, or 128-bit persistent ID when absent from the table | full form when changed |
| FIXED_ARRAY | the element codec per element | per element with a changed-element bitmask |
| STRUCT | the field set's codecs in property-ID order | per field |
| CONTAINER | count (varint, at most the property's max count) then elements; at most 1,024 bytes per property | full form when changed |

Every codec has a golden-vector test and a property-based round-trip test (§ "Stage 7 tests").

### Replication precision declarations

A replicated property may declare `{ precision, min, max, bits }` for its codec: in a schema as the field key `replication` (`"replication": { "precision": 0.01, "min": -100, "max": 100 }`), in C++ as `Zenith_PropertyBuilder::Replication(const Zenith_ReplicationQuantisation&)` — an ECS schema and builder extension (row NT19 (a)). Undeclared properties use the type's default form. A declaration change changes the table hash, so mismatched builds are rejected at the handshake.

### Reference encoding

- The session string table maps every `Zenith_StringID` a replicated property or event sends to a 16-bit index; new strings are added by a reliable `STRING_TABLE_ADD { index, UTF-8 bytes }` sent before first use; at most 4,096 entries of at most 255 bytes each per session.
- The session asset table maps asset persistent IDs to 16-bit indices the same way (at most 4,096).
- Entity references are network IDs; a reference to an entity the receiving peer does not hold resolves to the null handle on the client and counts `net.client.unresolved_references`.

### Protocol versioning

The protocol version is 1 at Stage 3 and 2 at Stage 7; a change to any message layout bumps it in the same diff; there is no negotiation, because both peers must run the same build (the identity check).

### Serialization limits

| Limit | Value |
|---|---|
| message | 65,536 bytes |
| messages per datagram | 32 |
| replication table entries | 1,024 |
| property encoded size | 1,024 bytes |
| string and asset tables | 4,096 entries each; strings 255 bytes |
| CREATEs per send tick per peer | 64 |
| relevant entities per peer | `net.interest.maxRelevantPerPeer` (default 2,048) |
| proposal records per INPUT | 16 |
| handshake credential | 1,024 bytes |
| publication piece | 1,016 data bytes behind an 8-byte header, 65 pieces per message (§ "Message publication in pieces") |

These join SEC's parser-limit inventory with the Stage 3 rows (row NT4).

## Dedicated server builds

### Server build target

- `game.json` gains `net.dedicatedServer` `{ "enabled": true, "platforms": ["windows"] }` (MOD's schema, row NT20 (a)); `zenith_add_game` then also creates the executable `<Name>Server` from the same composition with: no RHI backend linked (the generated composition's RHI thunks absent), `runtime.headless` forced true, `net.role` defaulting to `server`, the Audio module in its null-device mode, and the development console absent in Shipping.
- The server target links ZenithFlux because ZenithRuntime declares it, but never creates the renderer (startup steps 12 and 13 skipped as for every headless run); `Test_NetServerBuild_LinksNoRhiBackend` asserts the link closure through libgraph's report.
- A gate step for the server build is a Stage 7 PROC `needs-human` hand-off (a protected gate edit, `Zenith2/Docs/Roadmap.md` → "Stage 7 Later expansion").

### Server platforms

D-NET-SERVER-PLATFORM (open, Stage 7): (1) Windows x64 only, built in the existing `win-release` configuration; (2) Windows plus Linux x86-64, a new `linux-server` configure preset on the `linux-tsan` host's toolchain without sanitizers, with its own SHIP pins and `CompileCoverage` matrix entry. Criteria: hosting cost (most server hosting is Linux), the maintenance of a third production platform against Z2-D-010, toolchain pins, CI time. Acceptance rule: (1) unless a flagship slice's hosting target requires Linux; adopting (2) needs a decision amending Z2-D-010's platform list, recorded with the ruling.

### Server runtime differences

| Aspect | Dedicated server |
|---|---|
| window, input devices, audio output | none |
| render extraction | phase 12 extractors run (render paths run headless, ruling of RPROD r3 finding 4), the render subgraph never does |
| local player | none: slot 0 is unused, peers start at slot 1 |
| frame pacing | headless pacing (`Zenith2/Docs/Design/NET/Foundations.md` → "Headless frame pacing") |
| logging | DEVT's log file sink; one `net` summary line per minute (peers, bandwidth, tick time) |
| saves | SAVE as for any world; the server is the one peer that saves the authoritative world |

### Server operation

- Configuration: the command line and the game configuration layer (`--set net.port=…`, `--set net.maxPeers=…`); no editor link.
- Shutdown: the platform's termination signal (Windows `CTRL_CLOSE_EVENT` and `CTRL_BREAK_EVENT`, PLAT's lifecycle) requests shutdown; peers receive DISCONNECT `SERVER_SHUTDOWN`; exit code 0.
- Development builds expose the console over PLAT's development listener; Shipping server builds have no remote administration (`Zenith2/Docs/Architecture.md` → "Security architecture").

## Network simulation

### Simulated port

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetSimulatedPort.h (Stage 7)
struct Zenith_NetSimulationProfile
{
	uint32 m_uLatencyMs = 0;            // one-way, 0 … 2,000
	uint32 m_uJitterMs = 0;             // uniform ±, 0 … 1,000
	uint16 m_uLossPerMille = 0;         // 0 … 1,000
	uint16 m_uDuplicatePerMille = 0;
	uint16 m_uReorderPerMille = 0;      // a reordered datagram gets latency + 2 × jitter + 10 ms
	uint16 m_uReserved = 0;
	uint32 m_uBandwidthKiBps = 0;       // 0 = unlimited; token bucket, 50 ms burst
	uint16 m_uBurstEnterPerMille = 0;   // Gilbert–Elliott: good → bad per datagram
	uint16 m_uBurstExitPerMille = 0;    // bad → good
	uint16 m_uBurstLossPerMille = 0;    // loss while bad
	uint16 m_uReserved2 = 0;
};
/// @contract type Zenith_NetSimulatedPort
/// Ownership: Owns the wrapped port and a delay queue of at most 4,096 datagrams (QUEUE_FULL_POLICY_DROP_NEWEST).
/// Thread affinity: EXTERNALLY_SYNCHRONISED, as Zenith_NetDatagramPort.
/// Invalidation: SetProfile applies to datagrams sent after the call.
/// Units: Milliseconds; per-mille rates; KiB per second.
/// Errors: As the wrapped port; Create refuses a profile outside the ranges (NET_ERROR_INVALID_CONFIG) and refuses in Shipping
///   (NET_ERROR_NOT_IN_SHIPPING).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 7. Development builds only.
/// @endcontract
class Zenith_NetSimulatedPort final : public Zenith_NetDatagramPort
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_NetSimulatedPort>, Zenith_NetError> Create(
		Zenith_Owned<Zenith_NetDatagramPort> pxInner, const Zenith_NetSimulationProfile& xSend, const Zenith_NetSimulationProfile& xReceive,
		uint64 ulSeed, Zenith_PfnNetClock pfnClock, void* pClockUserData, Zenith_Heap& xHeap);
	void SetProfiles(const Zenith_NetSimulationProfile& xSend, const Zenith_NetSimulationProfile& xReceive);
	// Zenith_NetDatagramPort members: Send enqueues with a computed release time; Receive first releases due datagrams.
};
```

### Simulation profiles

| Profile (`net.sim.profile`) | Latency | Jitter | Loss | Duplicate | Reorder | Bandwidth | Bursts |
|---|---|---|---|---|---|---|---|
| `none` | 0 | 0 | 0 | 0 | 0 | unlimited | none |
| `lan` | 1 ms | 0 | 0 | 0 | 0 | unlimited | none |
| `broadband` | 30 ms | 5 ms | 5 ‰ | 0 | 1 ‰ | 2,048 KiB/s | none |
| `mobile` | 80 ms | 30 ms | 20 ‰ | 5 ‰ | 10 ‰ | 256 KiB/s | enter 5 ‰, exit 200 ‰, loss 500 ‰ |
| `hostile` | 200 ms | 80 ms | 100 ‰ | 20 ‰ | 50 ‰ | 64 KiB/s | enter 20 ‰, exit 100 ‰, loss 800 ‰ |

The profile applies to both directions; `net.sim.sendLatencyMs` and the other per-field keys override single fields; the console command `net.sim <profile>` switches at run time.

### Simulation determinism

The simulator draws from `Zenith_RandomSource(net.sim.seed, 0x4E45545349u)` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource") one draw per decision in a fixed order per datagram (loss, duplicate, reorder, jitter), so for the same seed, clock source and send sequence the same datagrams are dropped, duplicated and delayed. Tests pass a manual clock (`Zenith_PfnNetClock`), which makes the whole simulation a pure function of the test; the live clock gives realistic behaviour without that guarantee. Simulation never touches either peer's simulation state: what each peer receives is recorded as always.

## Network diagnostics

### Per-peer statistics

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetStatistics.h (Stage 7)
struct Zenith_NetPeerStatistics
{
	uint16 m_uPeer = 0;
	uint8 m_eState = 0;                   // the peer's session state
	uint8 m_uReserved = 0;
	uint32 m_uRttMs = 0, m_uRttVarMs = 0;
	uint32 m_uLossPerMille = 0;           // over the last second
	uint32 m_uSendBytesPerSecond = 0, m_uReceiveBytesPerSecond = 0;
	uint32 m_uBudgetBytesPerSecond = 0;
	uint32 m_uRelevantEntities = 0;
	uint32 m_uSnapshotBytesLast = 0;
	uint32 m_uReliableUnacked = 0;
	uint32 m_uMispredictions = 0;         // client side, cumulative
	uint32 m_uCommandLeadTicks = 0;
	uint32 m_uLateCommands = 0, m_uLostCommands = 0;
};
```

Statistics are presentation state: read on the main thread between frames, never by simulation.

### Network panel and console

- On-screen profiler panel `Net` (DEVT's panel providers, `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers"): one row per peer with the fields above, the session state, and a 10-second graph of RTT and bandwidth.
- Console commands (development builds; `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"): `net.status`, `net.peers`, `net.sim <profile>|off`, `net.kick <peer>`, `net.capture start|stop`, `net.relevance <peer>` (toggles the relevance debug view), `net.authority` (Stage 3).

### Packet capture

`net.capture start` writes every sent and received datagram of the session to `devfiles:/net/<session>-<timestamp>.znetcap` through DEVT's development-file writer (row NT24): header `{ magic "ZNCP", u16 version 1, u16 reserved, u64 session ID }`, then records `{ u64 monotonic µs, u8 direction (0 sent, 1 received), u8 reserved, u16 peer, u32 length, bytes }`; at most 64 MiB per file, then capture stops. The format is readable by the offline tool `ZenithDevTool net-dump` (DEVT's development host CLI), which decodes both wire versions.

### Replication debug views

Debug draw (`Zenith2/Docs/Design/DEVT/DebugDraw.md`): the relevance radius and grid cells around a peer; each relevant entity's priority as a coloured marker; client-side correction error vectors and interpolation buffer depth per proxy.

### Network frame dump

DEVT's frame dump gains the section `net` (`Zenith2/Docs/Design/DEVT/Console.md` → "DEVT frame dump"): per peer, the snapshot built this frame (entities, bytes, baselines), the command window state, and the transport counters; registered through `Zenith_ModuleStartContext::GetDevFrameDump` at Start.

## Stage 7 state machines

Client session (Stage 7):

| State \ event | connect request | CHALLENGE | ACCEPT v2 | REJECT | timeout | DISCONNECT | `Net.Connect` | shutdown |
|---|---|---|---|---|---|---|---|---|
| `OFFLINE` | send REQUEST v2, → `CONNECTING` | ignored | ignored | ignored | — | ignored | → `CONNECTING` | nothing |
| `CONNECTING` | resend every 250 ms | send RESPONSE, → `CHALLENGED` | ignored | → `FAILED` | → `FAILED` (`CONNECT_TIMEOUT`) | ignored | ignored | close |
| `CHALLENGED` | — | resend RESPONSE | → `CONNECTED`, publish | → `FAILED` | → `FAILED` | ignored | ignored | close |
| `CONNECTED` | — | ignored | ignored | ignored | → `DISCONNECTED` (`TIMEOUT`) | → `DISCONNECTED` | ignored | send DISCONNECT |
| `DISCONNECTED`, `FAILED` | — | ignored | ignored | ignored | — | ignored | → `CONNECTING` (new nonce) | nothing |

As at Stage 3, `Net.SessionState` is a function of the role and the recorded session events alone (`Zenith2/Docs/Design/NET/Loopback.md` → "Client session states"): every transition below that a script can observe is published from the prologue as a session event, and the Stage 7 events add `OFFSET_CHANGED` (6, § "Clock synchronisation") and `RECONNECTING` (7, § "Session lifecycle") to the Stage 3 five.

Server peer slot (Stage 7): `FREE` + valid RESPONSE with a free slot → `CONNECTED` (publish PEER_JOINED, send ACCEPT v2, then string table and CREATEs); `CONNECTED` + timeout, DISCONNECT, 8 malformed packets in 1 s or `net.kick` → `LINGER` (publish PEER_LEFT); `LINGER` + 1 s → `FREE`. A CHALLENGE holds no slot.

Proxy (client, Stage 7): `ABSENT` + CREATE → `SPAWN_PENDING` (spawn recorded); `SPAWN_PENDING` + indexed → `LIVE`; `LIVE` + delta → `LIVE` (commands); `LIVE` or `SPAWN_PENDING` + DESTROY → `DESTROY_PENDING` → `ABSENT` at the end of the tick.

Predicted entity (client, candidate 1): `REMOTE` + owner becomes local and `Zenith.Net.Predicted` present → `PREDICTED`; `PREDICTED` + correction → `REPLAYING` (inside `Net.Predict`) → `PREDICTED`; `PREDICTED` + `MaxPredictedTicks` exceeded → `STALLED` (no new commands) → `PREDICTED` on the next acknowledgement; `PREDICTED` + owner changes → `REMOTE`.

## Stage 7 tests

The Stage 7 test catalogue is `Zenith2/Docs/Design/NET/Tests.md` → "Stage 7 test catalogue"; every Stage 7 requirement of `Zenith2/Docs/Requirements/NET/Stage7.md` names its tests there.
