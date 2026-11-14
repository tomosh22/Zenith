# NET design — public API

This file is part of the NET design family. The entry document is `Zenith2/Docs/Design/NET.md`; this file declares every public root header of the `ZenithNet` module library, the internal declarations other NET files name, the script-facing API, the event types, the command kind payloads, the configuration keys and the misuse codes. The algorithms these declarations serve are `Zenith2/Docs/Design/NET/Foundations.md` (identity, authority, ticks, commands, headless) and `Zenith2/Docs/Design/NET/Loopback.md` (the Stage 3 transport, wire format and proof); the Stage 7 declarations are `Zenith2/Docs/Design/NET/Stage7.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| rules every NET header follows | "Rules across the ZenithNet headers" |
| the header inventory | "Header list" |
| identity types | "Zenith_NetworkID header", "Zenith_NetworkIDMap header" |
| shared value types, roles, states, limits | "Zenith_NetTypes header" |
| errors | "Zenith_NetErrors header" |
| components, events, command payloads | "Zenith_NetComponents header" |
| the authority and replication table | "Zenith_NetReplicationTable header" |
| the datagram port seam | "Zenith_NetDatagramPort header" |
| module entry points | "Zenith_Module_Net header" |
| internal types named across NET files | "Internal declarations" |
| Luau | "Script-facing API", "Event types", "Misuse codes" |
| commands | "Command kind payloads" |
| configuration | "Configuration keys" |

## Rules across the ZenithNet headers

- Every public type carries one contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type"; a declaration that departs from its block carries one `/// @deviation` line.
- Every fallible public function returns a `Zenith_Result` (`Zenith2/Docs/Contracts.md` → "Errors and results").
- No public header names an STL type, a platform socket type, a Winsock or BSD header, or any `Internal/` header of another library (`Zenith2/Docs/Contracts.md` → "API boundary"); the OS socket is PLAT's and reaches NET only through `Zenith_NetDatagramPort`.
- No handle kind marked "never" in `Zenith2/Docs/Contracts.md` → "Handle kinds" is written into a datagram, a publication payload or a save section by NET: entities travel as `Zenith_NetworkID`, assets as `Zenith_PersistentID` or a proxy-table index.
- Every error type is the enumeration-plus-record form of `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy": a code numbered from 1 and a record of at most 32 bytes with no text; the text is logged at the point of failure.
- Every multi-byte integer NET writes to the wire, to a publication payload or to a save section is little-endian and written field by field; no struct is copied to bytes.
- The headers compile alone (`HeaderSelfContained` gate step).

## Header list

| Header (`Zenith2/Engine/ZenithNet/`) | Declares | Stage |
|---|---|---|
| `Zenith_NetworkID.h` | `Zenith_NetworkID` (the declaration of `Zenith2/Docs/Architecture.md` → "Multiplayer foundations", verbatim), `Zenith_MakeNetworkID`, `Zenith_GetNetworkIDPeer`, `Zenith_GetNetworkIDCounter`, `ulNETWORK_ID_COUNTER_MAX` | S3 |
| `Zenith_NetTypes.h` | `Zenith_NetRole`, `Zenith_NetSessionState`, `Zenith_NetRejectReason`, `Zenith_NetDisconnectReason`, `Zenith_NetAddress`, `Zenith_NetSessionInfo`, the limit constants, `Zenith_ToString` overloads | S3 |
| `Zenith_NetErrors.h` | `Zenith_NetErrorCode`, `Zenith_NetError` | S3 |
| `Zenith_NetworkIDMap.h` | `Zenith_NetworkIDMap` | S3 |
| `Zenith_NetComponents.h` | `Zenith_NetIdentityComponent`, `Zenith_NetIdentityFlags`, `Zenith_NetProxyPrefabSetComponent`, the four event structs, `Zenith_NetSetOwnerRequest`, the stable-name and property-ID constants | S3 |
| `Zenith_NetReplicationTable.h` | `Zenith_NetReplicationCodec`, `Zenith_NetReplicationEntry`, `Zenith_NetAuthorityDiagnosticCode`, `Zenith_NetAuthorityDiagnostic`, `Zenith_NetReplicationTable` | S3 |
| `Zenith_NetDatagramPort.h` | `Zenith_NetDatagram`, `Zenith_NetDatagramPort`, `Zenith_NetMemoryPortDesc`, `Zenith_NetMemoryPortPair`, `Zenith_CreateNetMemoryPortPair` | S3 |
| `Zenith_Module_Net.h` | `Zenith_Module_Net_RegisterTypes`, `Zenith_Module_Net_Start` | S3 |
| `Zenith_NetTransportBackend.h` | the Stage 7 transport contract (`Zenith2/Docs/Design/NET/Stage7.md` → "Transport layer contract") | S7 |
| `Zenith_NetSimulatedPort.h` | the Stage 7 network simulator (`Zenith2/Docs/Design/NET/Stage7.md` → "Simulated port") | S7 |
| `Zenith_NetStatistics.h` | the Stage 7 per-peer statistics (`Zenith2/Docs/Design/NET/Stage7.md` → "Per-peer statistics") | S7 |

No other root header exists in `ZenithNet`; a header added by a Stage 7 ticket edits this table in the same diff.

## Zenith_NetworkID header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetworkID.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract value Zenith_NetworkID
/// Ownership: Plain 8-byte value; a session-scoped replicated identity, distinct from world handles, stable simulation IDs
///   and persistent IDs (Zenith2/Docs/Contracts.md → "Handle kinds", [Z2-N-CONTRACT-068]).
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None. A value names an entity only through a world's Zenith_NetworkIDMap; an ID whose entity was destroyed
///   resolves to the null handle and is never reissued for another entity.
/// Units: Bits 48–63: the allocating peer index (0 = the server); bits 0–47: that peer's counter, from 1.
/// Errors: None; IsValid is false for 0, which is never issued.
/// Version: since Z2 Stage 3. Serialized: in Zenith.Net.Identity property 1 (saves, replays), in datagrams as unsigned LEB128,
///   in publication payloads as unsigned LEB128; the bit layout is part of NET wire format version 1.
/// @endcontract
struct Zenith_NetworkID
{
	uint64 m_ulValue = 0;   // bits 48–63: the allocating peer (0 = the server); bits 0–47: that peer's counter

	constexpr bool IsValid() const { return m_ulValue != 0; }
	friend constexpr bool operator==(const Zenith_NetworkID&, const Zenith_NetworkID&) = default;
	friend constexpr bool operator<(const Zenith_NetworkID& xA, const Zenith_NetworkID& xB)
	{
		return xA.m_ulValue < xB.m_ulValue;
	}
};
static_assert(sizeof(Zenith_NetworkID) == 8);

/// @contract functions Zenith_NetworkIDFunctions
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: uPeer is a peer index; ulCounter is a per-peer counter value.
/// Errors: Zenith_MakeNetworkID with ulCounter 0 or above ulNETWORK_ID_COUNTER_MAX returns the invalid ID; nothing asserts.
/// Version: since Z2 Stage 3.
/// @endcontract
inline constexpr uint64 ulNETWORK_ID_COUNTER_MAX = (1ull << 48) - 1ull;   // 281,474,976,710,655
constexpr Zenith_NetworkID Zenith_MakeNetworkID(uint16 uPeer, uint64 ulCounter)
{
	return (ulCounter == 0 || ulCounter > ulNETWORK_ID_COUNTER_MAX)
		? Zenith_NetworkID{}
		: Zenith_NetworkID{ (static_cast<uint64>(uPeer) << 48) | ulCounter };
}
constexpr uint16 Zenith_GetNetworkIDPeer(Zenith_NetworkID xID) { return static_cast<uint16>(xID.m_ulValue >> 48); }
constexpr uint64 Zenith_GetNetworkIDCounter(Zenith_NetworkID xID) { return xID.m_ulValue & ulNETWORK_ID_COUNTER_MAX; }
```

`Zenith_NetworkID` and its first member are exactly Architecture's; the comparison operators and the three functions are NET's additions. Peer 0 counter 1 is `0x0000000000000001`; peer 1 counter 7 is `0x0001000000000007`.

## Zenith_NetTypes header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithNet/Zenith_NetworkID.h"

/// @contract enum Zenith_NetRole
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; the configuration spellings are "offline", "server", "client"; enumerators keep their meaning.
/// @endcontract
enum Zenith_NetRole : uint8
{
	NET_ROLE_OFFLINE = 0,   // network IDs allocated by peer 0; no transport (the default; every single-player run)
	NET_ROLE_SERVER = 1,    // authoritative; listens; allocates as peer 0; captures snapshots
	NET_ROLE_CLIENT = 2,    // connects; applies snapshots to proxies; sends input commands; allocates nothing at Stage 3
};

/// @contract enum Zenith_NetSessionState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; the script spellings are the enumerator suffixes in upper camel case ("Offline", "Listening", …).
/// @endcontract
enum Zenith_NetSessionState : uint8
{
	// The value is a function of the world's role and the session events Net.Session applied from the recorded inbox
	// (Zenith2/Docs/Design/NET/Loopback.md → "Client session states"); identical live and under --replay.
	NET_SESSION_STATE_OFFLINE = 0,        // NET_ROLE_OFFLINE (every world of the offline role, from attach)
	NET_SESSION_STATE_LISTENING = 1,      // server: no peer joined (the initial server state, and again after the last peer left)
	NET_SESSION_STATE_CONNECTING = 2,     // client: from attach until CONNECTED or FAILED is applied
	NET_SESSION_STATE_CONNECTED = 3,      // client: accepted; server: at least one joined peer has not left
	NET_SESSION_STATE_DISCONNECTED = 4,   // client: the session ended after it was connected, or the world was restored
	NET_SESSION_STATE_FAILED = 5,         // client: the session never connected (reject, timeout, socket or proxy-table failure)
};

/// @contract enum Zenith_NetRejectReason
/// Ownership: Plain enumeration; the uint8 of CONNECT_REJECT and of the FAILED session event.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. A reader treats an unknown value as NET_REJECT_REASON_MALFORMED.
/// Version: since Z2 Stage 3; part of NET wire format version 1; values are never renumbered.
/// @endcontract
enum Zenith_NetRejectReason : uint8
{
	NET_REJECT_REASON_NONE = 0,
	NET_REJECT_REASON_VERSION = 1,               // protocol version differs
	NET_REJECT_REASON_IDENTITY = 2,              // build ID, composition, content, backend or tick rate differs (detail byte names it)
	NET_REJECT_REASON_REPLICATION_TABLE = 3,     // the replication table hashes differ
	NET_REJECT_REASON_PROXY_TABLE = 4,           // the proxy prefab table hashes differ
	NET_REJECT_REASON_SERVER_FULL = 5,           // every peer slot is taken
	NET_REJECT_REASON_PEER_INDEX_EXHAUSTED = 6,  // the server issued peer index 65,534 already
	NET_REJECT_REASON_NOT_ACCEPTING = 7,         // no session world is attached on the server (detached, swapped out, or shutting down)
	NET_REJECT_REASON_MALFORMED = 8,             // the request failed validation
	NET_REJECT_REASON_CONNECT_TIMEOUT = 9,       // local only (never on the wire): no answer within net.connectTimeoutMs
	NET_REJECT_REASON_SOCKET = 10,               // local only: the socket could not be opened
	NET_REJECT_REASON_PROXY_NOT_RESIDENT = 11,   // local only: a client proxy prefab was not ACTIVE when the transport started
};

/// @contract enum Zenith_NetDisconnectReason
/// Ownership: Plain enumeration; the uint8 of DISCONNECT and of the DISCONNECTED and PEER_LEFT session events.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. A reader treats an unknown value as NET_DISCONNECT_REASON_PROTOCOL_ERROR.
/// Version: since Z2 Stage 3; part of NET wire format version 1; values are never renumbered.
/// @endcontract
enum Zenith_NetDisconnectReason : uint8
{
	NET_DISCONNECT_REASON_NONE = 0,
	NET_DISCONNECT_REASON_CLIENT_QUIT = 1,        // the client's process is shutting down
	NET_DISCONNECT_REASON_SERVER_SHUTDOWN = 2,    // the server's process is shutting down
	NET_DISCONNECT_REASON_TIMEOUT = 3,            // nothing received within net.peerTimeoutMs
	NET_DISCONNECT_REASON_WORLD_TRANSITION = 4,   // the session world was swapped out (Stage 3 ends the session)
	NET_DISCONNECT_REASON_PROTOCOL_ERROR = 5,     // 8 malformed packets from the peer within one second
	NET_DISCONNECT_REASON_KICKED = 6,             // Stage 7: the server removed the peer
	NET_DISCONNECT_REASON_RESTORED = 7,           // the session world was restored from a save (the restore ends the session)
};

/// @contract value Zenith_NetAddress
/// Ownership: Plain 8-byte value.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: m_uIPv4 in host byte order (127.0.0.1 is 0x7F000001); m_uPort a UDP port.
/// Errors: None; Zenith_ParseNetAddress returns NET_ERROR_INVALID_CONFIG for text that is not "<a>.<b>.<c>.<d>:<port>".
/// Version: since Z2 Stage 3 (IPv4 only; IPv6 is Stage 7, Zenith2/Docs/Design/NET/Stage7.md → "Transport layer contract").
/// @endcontract
struct Zenith_NetAddress
{
	uint32 m_uIPv4 = 0;
	uint16 m_uPort = 0;
	uint16 m_uReserved = 0;
	constexpr bool IsLoopback() const { return (m_uIPv4 >> 24) == 127u; }
	friend constexpr bool operator==(const Zenith_NetAddress&, const Zenith_NetAddress&) = default;
};
static_assert(sizeof(Zenith_NetAddress) == 8);

/// @contract value Zenith_NetSessionInfo
/// Ownership: Plain value copied out of the module's per-world state.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None; a copy, stale after the next frame prologue.
/// Units: Ticks; peers.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetSessionInfo
{
	Zenith_NetRole m_eRole = NET_ROLE_OFFLINE;
	Zenith_NetSessionState m_eState = NET_SESSION_STATE_OFFLINE;
	uint16 m_uLocalPeer = 0xFFFFu;              // 0 on the server and offline; the accepted index on a client; 0xFFFF before
	uint16 m_uConnectedPeers = 0;
	uint16 m_uReserved = 0;
	uint64 m_ulSessionID = 0;                   // 0 when not connected
	sint64 m_iClientTickOffset = 0;             // client: target server tick = client tick + offset; 0 elsewhere
};

inline constexpr uint16 uNET_PROTOCOL_VERSION = 1;
inline constexpr uint32 uNET_PACKET_MAGIC = 0x31504E5Au;              // bytes 'Z' 'N' 'P' '1'
inline constexpr uint32 uNET_MAX_DATAGRAM_BYTES = 1200;
inline constexpr uint32 uNET_PACKET_HEADER_BYTES = 20;
inline constexpr uint32 uNET_PACKET_TRAILER_BYTES = 4;                // CRC-32C
inline constexpr uint32 uNET_MAX_MESSAGE_BYTES = 1176;                // 1,200 − 20 − 4
inline constexpr uint32 uNET_MAX_COMMANDS_PER_DATAGRAM = 64;          // Architecture's parser limit
inline constexpr uint32 uNET_MAX_INPUT_BLOCKS = 4;
inline constexpr uint32 uNET_MAX_DIGEST_PAIRS = 4;                  // (tick, digest) pairs per INPUT
inline constexpr uint32 uNET_COMMAND_WIRE_BYTES = 18;                 // the ZRPL command record
inline constexpr uint32 uNET_PUBLICATION_PREFIX_BYTES = 4;
inline constexpr uint32 uNET_MAX_PUBLICATION_BYTES = 1024;            // uMODULE_PUBLICATION_MAX_RESULT_BYTES
inline constexpr uint32 uNET_MAX_FRAGMENT_BODY_BYTES = 1020;          // 1,024 − the 4-byte publication prefix
inline constexpr uint32 uNET_MAX_FRAGMENTS_PER_TICK = 32;
inline constexpr uint32 uNET_MAX_DESTROYS_PER_FRAGMENT = 64;
inline constexpr uint32 uNET_MAX_DESTROYS_PER_CAPTURE = 256;          // four fragments; the rest wait for the next captures
inline constexpr uint32 uNET_PROOF_MAX_REPLICATED_ENTITIES = 512;     // Stage 3 capture cap per tick
inline constexpr uint32 uNET_PROOF_MAX_REPLICATED_PROPERTIES = 32;    // Stage 3 replication table cap (the presence mask is 32 bits)
inline constexpr uint32 uNET_MAX_PROXY_PREFABS = 16;
inline constexpr uint32 uNET_MAX_PEERS_STAGE3 = 1;
inline constexpr uint16 uNET_PEER_INDEX_SERVER = 0;
inline constexpr uint16 uNET_PEER_INDEX_NONE = 0xFFFFu;
inline constexpr uint16 uNET_PEER_INDEX_MAX = 0xFFFEu;
inline constexpr uint32 uNET_CAPTURE_RING_TICKS = 8;
inline constexpr uint32 uNET_ACK_DIGEST_RING_TICKS = 64;
inline constexpr uint32 uNET_SERVER_COMMAND_WINDOW_TICKS = 128;
inline constexpr uint32 uNET_DESTROY_REPEAT_TICKS = 30;               // captures that carry each destroy, from its first
inline constexpr uint32 uNET_CLIENT_TOMBSTONE_TICKS = 64;
inline constexpr uint32 uNET_SAVE_SECTION_BYTES = 24;
inline constexpr uint32 uNET_SESSION_EVENT_RESERVE = 8;                // in-flight slots kept for session events (the inbox size)
inline constexpr uint32 uNET_MAX_DATAGRAMS_PER_PUMP = 512;
inline constexpr uint32 uNET_MAX_DATAGRAMS_PER_SEND = 512;
inline constexpr uint32 uNET_REPLAY_WINDOW_PACKETS = 64;
inline constexpr uint32 uNET_MALFORMED_DISCONNECT_COUNT = 8;
inline constexpr uint32 uNET_LINGER_MS = 1000;

constexpr const char* Zenith_ToString(Zenith_NetRole eRole);                       // "offline", "server", "client"
constexpr const char* Zenith_ToString(Zenith_NetSessionState eState);              // "Offline", "Listening", …
constexpr const char* Zenith_ToString(Zenith_NetRejectReason eReason);             // "version", "identity", …
constexpr const char* Zenith_ToString(Zenith_NetDisconnectReason eReason);         // "client-quit", …
Zenith_Result<Zenith_NetAddress, struct Zenith_NetError> Zenith_ParseNetAddress(Zenith_StringView strText);
uint32 Zenith_FormatNetAddress(Zenith_NetAddress xAddress, Zenith_Span<char> xOut22);   // "127.0.0.1:27870", returns length
```

`Zenith_ParseNetAddress` accepts exactly four decimal octets 0–255 without leading zeros (except `0`), a colon and a decimal port 1–65,535; anything else, including whitespace, is `NET_ERROR_INVALID_CONFIG`. The `Zenith_ToString` spellings of the reject and disconnect reasons are lower-case kebab of the enumerator suffix (`peer-index-exhausted`, `world-transition`).

## Zenith_NetErrors header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_NetErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. Zenith2/Docs/Design/NET.md → "Error enumerations" gives each code's meaning and who returns it.
/// Version: since Z2 Stage 3; codes 32 and above are Stage 7; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_NetErrorCode : uint8
{
	NET_ERROR_INVALID_CONFIG = 1,
	NET_ERROR_OUT_OF_MEMORY = 2,
	NET_ERROR_SOCKET = 3,
	NET_ERROR_BIND_FAILED = 4,
	NET_ERROR_ADDRESS_REFUSED = 5,
	NET_ERROR_NOT_IN_SHIPPING = 6,
	NET_ERROR_REPLAYING = 7,
	NET_ERROR_MALFORMED_PACKET = 8,
	NET_ERROR_UNKNOWN_MESSAGE = 9,
	NET_ERROR_VERSION_MISMATCH = 10,
	NET_ERROR_SESSION_MISMATCH = 11,
	NET_ERROR_LIMIT = 12,
	NET_ERROR_IDENTITY_MISMATCH = 13,
	NET_ERROR_TABLE_MISMATCH = 14,
	NET_ERROR_SERVER_FULL = 15,
	NET_ERROR_CONNECT_TIMEOUT = 16,
	NET_ERROR_PEER_TIMEOUT = 17,
	NET_ERROR_REJECTED = 18,
	NET_ERROR_NETWORK_ID_EXHAUSTED = 19,
	NET_ERROR_PEER_INDEX_EXHAUSTED = 20,
	NET_ERROR_DUPLICATE_NETWORK_ID = 21,
	NET_ERROR_UNKNOWN_NETWORK_ID = 22,
	NET_ERROR_PROXY_NOT_RESIDENT = 23,
	NET_ERROR_UNENCODABLE_PROPERTY = 24,
	NET_ERROR_WRONG_ROLE = 25,
	NET_ERROR_PORT_CLOSED = 26,
	NET_ERROR_WOULD_BLOCK = 27,
	NET_ERROR_MAP_FULL = 28,
	NET_ERROR_AUTHORITY = 29,
	// Stage 7 (Zenith2/Docs/Design/NET/Stage7.md → "Transport and session error codes")
	NET_ERROR_AUTH_FAILED = 32,
	NET_ERROR_CHALLENGE_FAILED = 33,
	NET_ERROR_CHANNEL_FULL = 34,
	NET_ERROR_RELIABLE_WINDOW_FULL = 35,
	NET_ERROR_REASSEMBLY = 36,
	NET_ERROR_BANDWIDTH = 37,
	NET_ERROR_PREDICTION_HISTORY = 38,
	NET_ERROR_INTEREST_CAPACITY = 39,
	NET_ERROR_SERVER_BUILD_DEVICE = 40,
	NET_ERROR_TRANSPORT_BACKEND = 41,
	NET_ERROR_RATE_LIMITED = 42,
};

/// @contract value Zenith_NetError
/// Ownership: Plain 8-byte value; the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE; members non-const: Zenith_Result and error records copy-assign it.
/// Invalidation: None.
/// Units: m_uOsCode is the OS socket error (WSAGetLastError, errno) or 0.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetError
{
	Zenith_NetErrorCode m_eCode;
	uint8 m_uDetail = 0;            // a Zenith_NetRejectReason, a Zenith_ReplayIdentityField value or a limit index; 0 otherwise
	uint16 m_uPeer = 0xFFFFu;       // the peer concerned, or 0xFFFF
	uint32 m_uOsCode = 0;
};
static_assert(sizeof(Zenith_NetError) == 8);
constexpr const char* Zenith_ToString(Zenith_NetErrorCode eCode);   // "invalid-config", "socket", …
```

## Zenith_NetworkIDMap header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetworkIDMap.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithNet/Zenith_NetworkID.h"
#include "ZenithNet/Zenith_NetErrors.h"

class Zenith_Heap;

/// @contract container Zenith_NetworkIDMap
/// Ownership: One per world, created by the Net module at AttachWorld from the general heap with a fixed capacity
///   (net.maxNetworkedEntities) and destroyed at DetachWorld; owned by the world's Zenith_NetWorldState.
/// Thread affinity: PHASE(Input). Insert and Erase run only on the Net.IdentityIndex node and on the world's sync-point node
///   (the lifecycle listener); Find, Contains and GetCount run on any node that declared a read of the Zenith.Net.WorldState
///   resource, which the schedule orders after Net.IdentityIndex inside phase 1.
/// Invalidation: A handle returned by Find is the one stored at the last Insert; it goes stale like any entity handle.
/// Units: Entries.
/// Errors: Insert returns NET_ERROR_MAP_FULL at capacity and NET_ERROR_DUPLICATE_NETWORK_ID for a present key; Find of an
///   absent or invalid ID returns the null handle; nothing asserts on data.
/// Version: since Z2 Stage 3. Never serialized: rebuilt from Zenith.Net.Identity components at every restore.
/// Alignment: The table's slots are 16-byte records aligned to 8.
/// Overflow: Capacity is fixed at creation (1,024 … 1,048,576); the table never grows.
/// Allocation failure: Create returns NET_ERROR_OUT_OF_MEMORY and allocates nothing else; no other member allocates.
/// Complexity: Insert, Erase, Find, Contains O(1) expected (open addressing, linear probing, load factor at most 0.5);
///   ForEachAscending O(n log n) into the caller's arena.
/// @endcontract
class Zenith_NetworkIDMap
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_NetworkIDMap>, Zenith_NetError> Create(uint32 uCapacity, Zenith_Heap& xHeap);
	Zenith_EntityHandle Find(Zenith_NetworkID xID) const;
	bool Contains(Zenith_NetworkID xID) const;
	uint32 GetCount() const;
	uint32 GetCapacity() const;
	Zenith_Result<void, Zenith_NetError> Insert(Zenith_NetworkID xID, Zenith_EntityHandle xEntity);
	void Replace(Zenith_NetworkID xID, Zenith_EntityHandle xEntity);   // a moved or re-indexed entity; no-op when absent
	bool Erase(Zenith_NetworkID xID);                     // false when absent
	void Clear();
	// Writes every (ID, handle) in ascending ID into axOut (capacity at least GetCount()); returns the count written.
	uint32 CopyAscending(Zenith_Span<Zenith_NetworkID> axIDs, Zenith_Span<Zenith_EntityHandle> axEntities) const;
};
```

The table stores `{ uint64 key; Zenith_EntityHandle value; }` slots, capacity rounded up to the next power of two at or above twice `uCapacity`; the hash is `Zenith_Hash64` of the 8 little-endian key bytes (D-SEL-HASH, open, Stage 1); erase uses backward-shift deletion, so no tombstone exists. Iteration order is never observable: the only enumeration is `CopyAscending`, which sorts by key, so every consumer that enumerates (capture, the save participant, the console) sees ascending network ID ([Z2-N-ARCH-884]).

## Zenith_NetComponents header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetComponents.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithScene/Zenith_SceneAssets.h"
#include "ZenithNet/Zenith_NetworkID.h"

/// @contract enum Zenith_NetIdentityFlags
/// Ownership: Plain bit flags stored in Zenith.Net.Identity property 4.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bits.
/// Errors: None; unknown bits are cleared by Net.IdentityIndex and counted in net.ids.bad_flags.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_NetIdentityFlags : uint8
{
	NET_IDENTITY_FLAG_NONE = 0,
	NET_IDENTITY_FLAG_REPLICATE = 1u << 0,   // the server captures the entity (default on)
	NET_IDENTITY_FLAG_PROXY = 1u << 1,       // set only by a client's Net.ClientApply on the proxies it spawns
};

/// @contract value Zenith_NetIdentityComponent
/// Ownership: Stored by the world's ECS backend as component "Zenith.Net.Identity" (32 bytes, alignment 8); the asset
///   handle member carries a reference, so the type registers C++ lifecycle hooks ([Z2-N-ARCH-645]).
/// Thread affinity: PHASE(Input) for property 1 (Net.IdentityIndex writes it in place); every other property is CommandsOnly.
/// Invalidation: None.
/// Units: m_uOwnerPeer is a peer index.
/// Errors: None; invalid contents are diagnosed by Net.IdentityIndex (Zenith2/Docs/Design/NET/Foundations.md → "Duplicate and unknown IDs").
/// Version: since Z2 Stage 3. Serialized through the component's field set (saves, replay checkpoints, cooked scenes).
/// @endcontract
struct Zenith_NetIdentityComponent
{
	uint64 m_ulNetworkID = 0;                                 // 1 NetworkID (Input), U64, editor-hidden; 0 = not yet assigned
	Zenith_AssetHandle<Zenith_Prefab> m_xProxyPrefab;          // 2 ProxyPrefab (CommandsOnly), ASSET kind prefab
	uint16 m_uOwnerPeer = 0;                                  // 3 OwnerPeer (CommandsOnly), U16; 0 = the server
	uint8 m_uFlags = NET_IDENTITY_FLAG_REPLICATE;             // 4 Flags (CommandsOnly), U8, Zenith_NetIdentityFlags
	uint8 m_auReserved[5] = {};

	static constexpr const char* szSTABLE_NAME = "Zenith.Net.Identity";
	static constexpr uint32 uPROP_NETWORK_ID = 1;
	static constexpr uint32 uPROP_PROXY_PREFAB = 2;
	static constexpr uint32 uPROP_OWNER_PEER = 3;
	static constexpr uint32 uPROP_FLAGS = 4;
	static const Zenith_TypeID xTYPE_ID;                       // Zenith_Hash64("Zenith.Net.Identity")
};
static_assert(sizeof(Zenith_NetIdentityComponent) == 32);

/// @contract value Zenith_NetProxyPrefabSetComponent
/// Ownership: Stored by the ECS backend as component "Zenith.Net.ProxyPrefabSet" (256 bytes); authored in a session scene
///   on exactly one entity; the asset references make the proxy prefabs scene dependencies, so they are resident after the
///   scene loads (Zenith2/Docs/Design/NET/Loopback.md → "Proxy prefab table").
/// Thread affinity: PHASE(Input) reads; CommandsOnly writes.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a second set entity, an empty set, or more than one entry naming the same prefab is diagnosed at the first
///   tick (net.proxy_table.invalid) and the session fails with NET_REJECT_REASON_PROXY_TABLE semantics locally.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetProxyPrefabSetComponent
{
	Zenith_AssetHandle<Zenith_Prefab> m_axPrefabs[16];         // 1 Prefabs (CommandsOnly), FIXED_ARRAY of 16 ASSET; null entries ignored

	static constexpr const char* szSTABLE_NAME = "Zenith.Net.ProxyPrefabSet";
	static constexpr uint32 uPROP_PREFABS = 1;
	static const Zenith_TypeID xTYPE_ID;
};
static_assert(sizeof(Zenith_NetProxyPrefabSetComponent) == 256);

/// @contract value Zenith_NetSessionChangedEvent
/// Ownership: The payload of the world-level event type "Net.SessionChanged" (4 bytes).
/// Thread affinity: IMMUTABLE; members non-const: the module fills the payload field by field before emitting it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetSessionChangedEvent
{
	uint8 m_uState = 0;     // 1 State: Zenith_NetSessionState
	uint8 m_uReason = 0;    // 2 Reason: Zenith_NetRejectReason when FAILED, Zenith_NetDisconnectReason when DISCONNECTED, else 0
	uint16 m_uPeer = 0;     // 3 Peer: the local peer index (client) or 0
};

/// @contract value Zenith_NetPeerJoinedEvent
/// Ownership: The payload of the world-level event type "Net.PeerJoined" (4 bytes), emitted by a server's Net.Session.
/// Thread affinity: IMMUTABLE; members non-const: the module fills the payload field by field before emitting it.
/// Invalidation: None.
/// Units: m_uPeer is a peer index.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetPeerJoinedEvent { uint16 m_uPeer = 0; uint16 m_uReserved = 0; };                         // 1 Peer

/// @contract value Zenith_NetPeerLeftEvent
/// Ownership: The payload of the world-level event type "Net.PeerLeft" (4 bytes), emitted by a server's Net.Session.
/// Thread affinity: IMMUTABLE; members non-const: the module fills the payload field by field before emitting it.
/// Invalidation: None.
/// Units: m_uPeer is a peer index; m_uReason a Zenith_NetDisconnectReason.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetPeerLeftEvent { uint16 m_uPeer = 0; uint8 m_uReason = 0; uint8 m_uReserved = 0; };      // 1 Peer, 2 Reason

/// @contract value Zenith_NetControlledEntityChangedEvent
/// Ownership: The payload of the world-level event type "Net.ControlledEntityChanged", emitted by a client's Net.ClientApply.
/// Thread affinity: IMMUTABLE; members non-const: the module fills the payload field by field before emitting it.
/// Invalidation: The handles follow the world's handle rules; either may be stale by the time the event is read in phase 9.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetControlledEntityChangedEvent
{
	Zenith_EntityHandle m_xEntity;     // 1 Entity: the proxy now owned by the local peer, or null
	Zenith_EntityHandle m_xPrevious;   // 2 Previous: the previously owned proxy, or null
};

/// @contract value Zenith_NetSetOwnerRequest
/// Ownership: The 24-byte payload of the module command kind "Net.SetOwner" (registered TYPE_KIND_STRUCT "Zenith.Net.SetOwnerRequest").
/// Thread affinity: IMMUTABLE; members non-const: a registered command payload that reflection reads and writes field by field.
/// Invalidation: None.
/// Units: m_uPeer is a peer index.
/// Errors: None; playback drops a stale target, a target without Zenith.Net.Identity, a peer above uNET_PEER_INDEX_MAX and a
///   request recorded on a client (Zenith2/Docs/Design/NET/Foundations.md → "Ownership and the owner peer").
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetSetOwnerRequest
{
	uint8 m_eOp = 1;                  // 1 SET
	uint8 m_auReserved[7] = {};
	Zenith_EntityHandle m_xTarget;
	uint16 m_uPeer = 0;
	uint8 m_auReserved2[6] = {};
};
static_assert(sizeof(Zenith_NetSetOwnerRequest) == 24);
```

The stable names of the three event types after `Net.SessionChanged` are `Net.PeerJoined`, `Net.PeerLeft` and `Net.ControlledEntityChanged`; their script names are the same strings without the `Net.` prefix inside the `Net` namespace of "Event types".

## Zenith_NetReplicationTable header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetReplicationTable.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_TypeInfo.h"
#include "ZenithNet/Zenith_NetErrors.h"

class Zenith_TypeRegistry;
class Zenith_Heap;

/// @contract enum Zenith_NetReplicationCodec
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (values 1 and 2, the proof codecs); Stage 7 adds the generic codecs from 16
///   (Zenith2/Docs/Design/NET/Stage7.md → "Property codecs"). Values are part of the replication table hash.
/// @endcontract
enum Zenith_NetReplicationCodec : uint8
{
	NET_CODEC_NONE = 0,
	NET_CODEC_POSITION_MM = 1,           // PROPERTY_TYPE_VEC3: three int32 millimetres
	NET_CODEC_QUAT_SMALLEST_THREE = 2,   // PROPERTY_TYPE_QUAT: 2-bit index and three 10-bit components in one uint32
};

/// @contract value Zenith_NetReplicationEntry
/// Ownership: Plain 24-byte value owned by its Zenith_NetReplicationTable.
/// Thread affinity: IMMUTABLE; members non-const: Build fills the table's entries field by field.
/// Invalidation: With its table.
/// Units: m_uWireBytes in bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetReplicationEntry
{
	Zenith_TypeID m_xType;                   // a TYPE_KIND_COMPONENT type with network authority SERVER or OWNER_CLIENT
	Zenith_PropertyID m_xProperty;           // a property flagged PROPERTY_FLAG_REPLICATED
	Zenith_PropertyValueType m_eValueType;
	Zenith_NetReplicationCodec m_eCodec;
	uint16 m_uWireBytes = 0;                // 12 for POSITION_MM, 4 for QUAT_SMALLEST_THREE
	uint32 m_uReserved = 0;
};
static_assert(sizeof(Zenith_NetReplicationEntry) == 24);

/// @contract enum Zenith_NetAuthorityDiagnosticCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_NetAuthorityDiagnosticCode : uint8
{
	NET_AUTHORITY_DIAG_LOCAL_REPLICATED = 1,       // error: a replicated property on a NETWORK_AUTHORITY_LOCAL component
	NET_AUTHORITY_DIAG_NON_COMPONENT = 2,          // error: a replicated property on a type that is not a component
	NET_AUTHORITY_DIAG_UNENCODABLE = 3,            // warning: a replicated property whose value type has no Stage 3 codec
	NET_AUTHORITY_DIAG_TOO_MANY = 4,               // error: more than uNET_PROOF_MAX_REPLICATED_PROPERTIES encodable entries
	NET_AUTHORITY_DIAG_OWNER_CLIENT_STAGE3 = 5,    // note: an OWNER_CLIENT component, treated as SERVER until Stage 7
	NET_AUTHORITY_DIAG_NET_TYPE_REPLICATED = 6,    // error: a property of Zenith.Net.Identity or Zenith.Net.ProxyPrefabSet is replicated
};

/// @contract value Zenith_NetAuthorityDiagnostic
/// Ownership: Plain 24-byte value; written into the caller's span by Zenith_NetReplicationTable::Build.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetAuthorityDiagnostic
{
	Zenith_NetAuthorityDiagnosticCode m_eCode;
	uint8 m_bError = 0;                     // 1 for codes 1, 2, 4 and 6
	uint16 m_uReserved = 0;
	Zenith_PropertyID m_xProperty;
	Zenith_TypeID m_xType;
	uint64 m_ulReserved = 0;
};

/// @contract type Zenith_NetReplicationTable
/// Ownership: Built once by the Net module at Start from the frozen type registry, owned by the module instance; the cook's
///   NET check builds its own copy (Zenith2/Docs/Design/NET.md → "Cook steps").
/// Thread affinity: IMMUTABLE after Build; private members non-const: Build fills them after allocating the table.
/// Invalidation: None while its module instance lives; a registry hot-reload window never changes component types in a
///   Player ([Z2-N-ARCH-650]).
/// Units: Entries; bytes.
/// Errors: Build returns NET_ERROR_AUTHORITY when any error diagnostic exists (logged, one line each), NET_ERROR_OUT_OF_MEMORY;
///   warnings and notes do not fail Build.
/// Version: since Z2 Stage 3. The hash is part of the CONNECT_REQUEST of wire format version 1.
/// @endcontract
class Zenith_NetReplicationTable
{
public:
	// Writes at most axDiagnostics.size diagnostics and sets uDiagnosticCountOut to the number found (which may be larger).
	static Zenith_Result<Zenith_Owned<Zenith_NetReplicationTable>, Zenith_NetError> Build(
		const Zenith_TypeRegistry& xTypes, Zenith_Span<Zenith_NetAuthorityDiagnostic> axDiagnostics,
		uint32& uDiagnosticCountOut, Zenith_Heap& xHeap);
	Zenith_Span<const Zenith_NetReplicationEntry> GetEntries() const;   // ascending (type ID, property ID)
	Zenith_Span<const Zenith_TypeID> GetComponentTypes() const;        // ascending, unique
	uint64 GetHash() const;
	uint32 GetMaxRecordValueBytes() const;                            // Σ m_uWireBytes
	Zenith_NetworkAuthority GetAuthority(Zenith_TypeID xType) const;   // NETWORK_AUTHORITY_LOCAL for absent types
};
```

## Zenith_NetDatagramPort header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_NetDatagramPort.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithNet/Zenith_NetTypes.h"
#include "ZenithNet/Zenith_NetErrors.h"

class Zenith_Heap;

/// @contract value Zenith_NetDatagram
/// Ownership: Plain value; m_xBytes borrows the caller's receive buffer.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: m_xBytes is valid until the next Receive on the same port.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetDatagram
{
	Zenith_NetAddress m_xFrom;
	Zenith_ByteSpan m_xBytes;
};

/// @contract interface Zenith_NetDatagramPort
/// Ownership: Implemented by NET's production port over PLAT's datagram socket (Internal/Zenith_NetPlatformPort.cpp), by
///   the memory port of Zenith_CreateNetMemoryPortPair and, from Stage 7, by Zenith_NetSimulatedPort; owned by the NET
///   transport that opened it, destroyed before the module instance.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. NET calls Receive only in frame-prologue step 1 (main thread) and Send only in
///   frame-prologue step 1 (the handshake datagrams, a DISCONNECT and a client's digest-only INPUT), in the Net.Send engine
///   epilogue stage and in the shutdown sequence, which never overlap (Zenith2/Docs/Design/NET.md → "Cross-thread hand-offs").
/// Invalidation: After Close, every member returns NET_ERROR_PORT_CLOSED.
/// Units: Bytes; datagrams of at most uNET_MAX_DATAGRAM_BYTES.
/// Errors: Send: NET_ERROR_LIMIT (more than 1,200 bytes), NET_ERROR_ADDRESS_REFUSED (a non-loopback destination at Stage 3),
///   NET_ERROR_WOULD_BLOCK (the OS or ring buffer is full; the datagram is dropped and counted by the caller),
///   NET_ERROR_SOCKET (OS code). Receive: NET_ERROR_WOULD_BLOCK when nothing is queued, NET_ERROR_SOCKET. Received bytes are
///   untrusted.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_NetDatagramPort
{
public:
	virtual ~Zenith_NetDatagramPort() = default;
	virtual Zenith_NetAddress GetLocalAddress() const = 0;
	virtual Zenith_Result<void, Zenith_NetError> Send(Zenith_NetAddress xTo, Zenith_ByteSpan xBytes) = 0;
	// Non-blocking. Copies at most xBuffer.size bytes; a larger datagram is truncated by the OS and reported NET_ERROR_LIMIT.
	virtual Zenith_Result<Zenith_NetDatagram, Zenith_NetError> Receive(Zenith_MutableByteSpan xBuffer) = 0;
	virtual void Close() = 0;
};

/// @contract value Zenith_NetMemoryPortDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Datagrams.
/// Errors: Zenith_CreateNetMemoryPortPair returns NET_ERROR_INVALID_CONFIG for a capacity outside 1 … 65,536 or equal addresses.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_NetMemoryPortDesc
{
	uint32 m_uCapacityDatagrams = 1024;        // each direction's ring, QUEUE_FULL_POLICY_DROP_NEWEST
	Zenith_NetAddress m_xAddressA = { 0x7F000001u, 40001 };
	Zenith_NetAddress m_xAddressB = { 0x7F000001u, 40002 };
};

/// @contract type Zenith_NetMemoryPortPair
/// Ownership: Owns two connected in-process ports and their two rings (capacity × 1,200 bytes each, from the general heap).
/// Thread affinity: OWNER_THREAD. Both ports are used by one thread at a time (NET's session and snapshot tests).
/// Invalidation: The ports are destroyed with the pair; Close of one side makes the other side's Send succeed and drop.
/// Units: Datagrams.
/// Errors: As Zenith_NetDatagramPort; a full ring is NET_ERROR_WOULD_BLOCK and counts in the pair's GetDropped.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3. A production implementation selected by configuration (ruling FINAL-DOUBLES), not a test double.
/// @endcontract
class Zenith_NetMemoryPortPair
{
public:
	Zenith_NetDatagramPort& GetPortA();
	Zenith_NetDatagramPort& GetPortB();
	uint64 GetDropped() const;
	// Detaches one port as an owned object, so a transport can own it; the pair keeps the ring alive until both are destroyed.
	Zenith_Owned<Zenith_NetDatagramPort> TakePortA();
	Zenith_Owned<Zenith_NetDatagramPort> TakePortB();
};
Zenith_Result<Zenith_Owned<Zenith_NetMemoryPortPair>, Zenith_NetError> Zenith_CreateNetMemoryPortPair(
	const Zenith_NetMemoryPortDesc& xDesc, Zenith_Heap& xHeap);
```

The rings are CORE `Zenith_RingBuffer`s of fixed 1,200-byte slots plus a length and a source address per slot (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer"); a datagram sent on port A is received on port B with `m_xFrom` = address A, in send order.

## Zenith_Module_Net header

```cpp
// Zenith2/Engine/ZenithNet/Zenith_Module_Net.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_Net
/// Ownership: RegisterTypes registers into the caller's registry; Start returns the module instance the runtime owns.
/// Thread affinity: MAIN_THREAD (startup steps 6 and 15).
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION (a rejected type, key, system or command registration, an authority error of the
///   replication table, an invalid configuration value), MODULE_ERROR_OUT_OF_MEMORY; each is logged with its cause first.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Net_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Net_Start(
	Zenith_ModuleStartContext& xContext);
```

`RegisterTypes` registers, in this order: the components `Zenith.Net.Identity` and `Zenith.Net.ProxyPrefabSet`; the struct `Zenith.Net.SetOwnerRequest`; the four event types; the world resources `Zenith.Net.WorldState` (owning phase Input) and `Zenith.Net.Capture` (owning phase SaveSnapshot); the world-independent resource `Zenith.Net.Transport`, the one resource the engine-wide epilogue stage `Net.Send` writes ([Z2-N-ARCH-442]); every key of "Configuration keys" through `xTypes.GetConfigSchema()`. Each component and event calls `ScriptName` ([Z2-N-ARCH-652]): `NetIdentity`, `NetProxyPrefabSet`, `NetSessionChanged`, `NetPeerJoined`, `NetPeerLeft`, `NetControlledEntityChanged`. The algorithm of `Start` and of each attach is `Zenith2/Docs/Design/NET.md` → "Module start" and → "World attach and detach".

## Internal declarations

These live under `Zenith2/Engine/ZenithNet/Internal/` and are visible to NET's own sources and tests alone.

### Zenith_NetModuleInstance

```cpp
// Internal/Zenith_NetModuleInstance.h
class Zenith_NetModuleInstance final : public Zenith_ModuleInstance, public Zenith_ModuleFrameListener,
	public Zenith_ModulePublicationHandler
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_NetModuleInstance>, Zenith_ModuleError> Create(
		Zenith_ModuleStartContext& xContext, const Zenith_NetModuleConfig& xConfig);
	~Zenith_NetModuleInstance() override;                 // sends DISCONNECT, closes the port, evaluates proof outcome
	Zenith_Result<void, Zenith_ModuleError> AttachWorld(Zenith_WorldAttachContext& xContext) override;
	void DetachWorld(uint16 uWorldTag) override;
	void OnPlatformEvents(Zenith_Span<const Zenith_PlatformEvent> axEvents) override;   // Net.Pump, prologue step 1
	void OnPublished(const Zenith_ModulePublication& xPublication, Zenith_TickBoundaryAccess& xWorld) override;
private:
	Zenith_NetModuleConfig m_xConfig;                              // the parsed keys, frozen at Start
	Zenith_Owned<Zenith_NetReplicationTable> m_pxTable;
	Zenith_Owned<Zenith_NetTransport> m_pxTransport;               // null offline and under --replay
	Zenith_NetWorldState* m_apxWorlds[4096] = {};                  // indexed by world tag
	uint16 m_uSessionWorldTag = 0;                                // the world bound to the session; 0 when none
	Zenith_TestAssertionSink* m_pxAssertions = nullptr;            // null in Shipping (assumption pending MOD, row NT6)
	Zenith_NetProofState m_xProof;
};
```

### Zenith_NetWorldState

```cpp
// Internal/Zenith_NetWorldState.h
struct Zenith_NetIDAllocator                      // 16 bytes; saved by the Zenith.Net.Identity participant
{
	uint64 m_ulNextCounter = 1;                  // the next counter of the local peer
	uint16 m_uLocalPeer = 0;                     // 0 on the server and offline; the accepted index on a client
	uint16 m_uHighestPeerIssued = 0;             // server: the highest peer index ever issued in this world's lineage
	uint32 m_uReserved = 0;
};

struct Zenith_NetWorldState                       // one per attached world; the Zenith.Net.WorldState resource points here
{
	uint16 m_uWorldTag = 0;
	Zenith_NetRole m_eRole = NET_ROLE_OFFLINE;
	bool m_bIsSessionWorld = false;
	Zenith_NetSessionState m_eSessionState = NET_SESSION_STATE_OFFLINE;   // written by Net.Session from recorded events and by
	                                                                       // RestoreSection; initial value by role at attach
	bool m_bSessionStarted = false;               // a server's first PEER_JOINED or a client's CONNECTED was applied (fidelity)
	bool m_bProxyTableValid = false;              // written by Net.Session at the first tick; read by the client transport's Start
	uint32 m_uRestoreMark = 0;                   // incremented by RestoreSection; the prologue ends the live session when it moves
	Zenith_NetIDAllocator m_xAllocator;
	Zenith_Owned<Zenith_NetworkIDMap> m_pxMap;
	Zenith_NetPendingDestroyList m_xPendingDestroys;      // server: destroyed IDs with the tick and the remaining capture count
	Zenith_NetSessionInbox m_xSessionInbox;               // session publications of this tick boundary (at most 8)
	Zenith_NetProxyTable m_xProxyTable;                   // built at the first tick from Zenith.Net.ProxyPrefabSet
	Zenith_NetServerWorldState* m_pxServer = nullptr;     // NET_ROLE_SERVER: command window, connected peers
	Zenith_NetClientWorldState* m_pxClient = nullptr;     // NET_ROLE_CLIENT: inboxes, pending spawns, applied ticks, tombstones
	Zenith_NetWorldCounters m_xCounters;
};
```

### Zenith_NetTransport

```cpp
// Internal/Zenith_NetTransport.h
class Zenith_NetTransport
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_NetTransport>, Zenith_NetError> Create(const Zenith_NetTransportDesc& xDesc,
		Zenith_Owned<Zenith_NetDatagramPort> pxPort, Zenith_Heap& xHeap);
	// Prologue step 1: receives at most uNET_MAX_DATAGRAMS_PER_PUMP datagrams, runs the session timers, hands decoded
	// messages to xSink in arrival order.
	void Pump(uint64 ulNowMs, Zenith_NetTransportSink& xSink);
	// Net.Send: encodes and sends the frame's queued outgoing messages; at most uNET_MAX_DATAGRAMS_PER_SEND datagrams.
	void Flush(uint64 ulNowMs);
	void QueueSnapshotFragment(uint16 uPeer, Zenith_ByteSpan xFragmentBody);
	void QueueInput(const Zenith_NetInputMessage& xMessage);
	void Disconnect(uint16 uPeer, Zenith_NetDisconnectReason eReason);   // sends DISCONNECT 3 times, 10 ms apart
	const Zenith_NetTransportCounters& GetCounters() const;
};
```

`Zenith_NetTransportSink` is the internal interface the module instance implements (`OnConnected`, `OnRejected`, `OnPeerJoined`, `OnPeerLeft`, `OnInput`, `OnSnapshotFragment`, `OnAck`); its call order and the state machines that drive it are `Zenith2/Docs/Design/NET/Loopback.md` → "Session state machines".

### Zenith_NetWireCodec

```cpp
// Internal/Zenith_NetWire.h
enum Zenith_NetMessageKind : uint8
{
	NET_MESSAGE_CONNECT_REQUEST = 1, NET_MESSAGE_CONNECT_ACCEPT = 2, NET_MESSAGE_CONNECT_REJECT = 3,
	NET_MESSAGE_INPUT = 4, NET_MESSAGE_SNAPSHOT = 5, NET_MESSAGE_KEEPALIVE = 6, NET_MESSAGE_DISCONNECT = 7,
};
struct Zenith_NetPacketHeader { uint32 m_uMagic; uint16 m_uVersion; uint8 m_uKind; uint8 m_uFlags; uint64 m_ulSessionID; uint32 m_uSequence; };
uint32 Zenith_NetWritePacket(const Zenith_NetPacketHeader& xHeader, Zenith_ByteSpan xBody, Zenith_MutableByteSpan xOut);
Zenith_Result<Zenith_NetPacketView, Zenith_NetError> Zenith_NetReadPacket(Zenith_ByteSpan xDatagram);
Zenith_Result<Zenith_NetConnectRequest, Zenith_NetError> Zenith_NetDecodeConnectRequest(Zenith_ByteSpan xBody);
// … one encode and one decode per message kind, and Zenith_NetDecodeSnapshotFragment (Zenith2/Docs/Design/NET/Loopback.md)
uint32 Zenith_NetWriteULEB128(uint64 ulValue, uint8 (&auOut)[10]);
Zenith_Result<uint64, Zenith_NetError> Zenith_NetReadULEB128(Zenith_ByteSpan xIn, uint32& uInOutOffset);
```

### Zenith_NetQuantise

```cpp
// Internal/Zenith_NetQuantise.h
struct Zenith_NetQuantiseCounters { uint32 m_uClamped = 0; uint32 m_uNonFinite = 0; };
void Zenith_NetQuantisePositionMm(const Zenith_Vector3& xIn, sint32 (&aiOut)[3], Zenith_NetQuantiseCounters& xCounters);
Zenith_Vector3 Zenith_NetDequantisePositionMm(const sint32 (&aiIn)[3]);
uint32 Zenith_NetQuantiseQuatSmallestThree(const Zenith_Quaternion& xIn, Zenith_NetQuantiseCounters& xCounters);
Zenith_Quaternion Zenith_NetDequantiseQuatSmallestThree(uint32 uPacked);
```

### Zenith_NetCommandSource

The server's and the client's implementation of PLAT's per-world input command bridge (assumption pending PLAT, row NT3; `Zenith2/Docs/Design/NET/Foundations.md` → "Command source contract"):

```cpp
// Internal/Zenith_NetCommandSource.h
class Zenith_NetCommandSource final : public Zenith_InputCommandBridge
{
public:
	// True for a server world (the remote peer's commands replace the local slice), false for a client world.
	bool ReplacesLocalInput() const override;
	// Server: the commands of the peer that drives player slot 0 for ulTick, in sequence order (Foundations.md "Command source contract").
	uint32 ProvideTickCommands(uint64 ulTick, Zenith_Span<Zenith_InputCommand> axOut) override;
	// Client: copies the tick's encoded commands into the outbound ring (Loopback.md "Client command capture").
	void ObserveTickCommands(uint64 ulTick, Zenith_Span<const Zenith_InputCommand> axCommands) override;
};
```

## Script-facing API

The bindings are registered by `Internal/Zenith_NetBindings.cpp` through `Zenith_ModuleStartContext::RegisterScriptBindings` as one `Zenith_ScriptBindingTable` for the namespace `Net` (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header"). Every read binding names the resource `Zenith.Net.WorldState` (and, where marked, the component `Zenith.Net.Identity`), so a module that calls it lists them in its `Uses` key.

| Luau function | Contexts | Effect | Returns or does |
|---|---|---|---|
| `Net.Role(ctx)` | system, behaviour | read world | `"Offline"`, `"Server"` or `"Client"` |
| `Net.SessionState(ctx)` | system, behaviour | read world | the `Zenith_ToString(Zenith_NetSessionState)` spelling |
| `Net.LocalPeer(ctx)` | system, behaviour | read world | the local peer index, or `nil` before a client is accepted |
| `Net.Peers(ctx)` | behaviour | read world | an array of the connected peer indices in ascending order (empty on a client and offline) |
| `Net.NetworkID(ctx, entity)` | system, behaviour | read world, `Zenith.Net.Identity` | two numbers `peer, counter`, or `nil` when the entity has no identity or ID 0 |
| `Net.FindEntity(ctx, peer, counter)` | system, behaviour | read world | the `Entity` the world's map holds, or `nil` |
| `Net.OwnerPeer(ctx, entity)` | system, behaviour | read `Zenith.Net.Identity` | a number, or `nil` without an identity |
| `Net.IsLocallyControlled(ctx, entity)` | system, behaviour | read world, `Zenith.Net.Identity` | `true` when the identity's owner peer equals the local peer (offline and server: owner 0); `false` without an identity |
| `Net.SetOwner(ctx, entity, peer)` | system, behaviour | buffered (`uSIDE_EFFECT_MASK_FIELD_COMMAND`, module command kind `Net.SetOwner`) | nothing; effective from the next phase after the issuing phase's sync point |

- A counter is returned as a Luau number: counters never exceed 2^48 − 1, below 2^53, so the value is exact.
- `Net.FindEntity` and `Net.SetOwner` validate their numeric arguments before anything else: for `Net.FindEntity` a non-integer or negative argument, a peer above 65,534 or a counter of 0 or above 2^48 − 1 is ZS3401; for `Net.SetOwner` a peer that is not an integer 0 … 65,534 is ZS3404; each is the catalogued misuse error of "Misuse codes", never an assert ([Z2-N-SCRIPT-926] block).
- `Net.SetOwner` from a world whose role is `NET_ROLE_CLIENT` raises ZS3402 at the call, so nothing is buffered.
- Under `--replay` every binding returns what it returns live, because every value it reads is simulation state restored from the log.

## Event types

| Event type | Script name | Fields | Emitted by |
|---|---|---|---|
| `Net.SessionChanged` | `NetSessionChanged` | `State` (`"Offline"` … `"Failed"`), `Reason` (a reject or disconnect reason spelling, or `"none"`), `Peer` (number) | `Net.Session`, phase 1, on every state change published at the tick boundary |
| `Net.PeerJoined` | `NetPeerJoined` | `Peer` | `Net.Session` (server) |
| `Net.PeerLeft` | `NetPeerLeft` | `Peer`, `Reason` | `Net.Session` (server) |
| `Net.ControlledEntityChanged` | `NetControlledEntityChanged` | `Entity`, `Previous` | `Net.ClientApply` (client), when the proxy whose owner peer is the local peer changes |

Every event is world-level (emitted through `Zenith_SystemContext::Events(0)`), merged at the sync point after phase 1 and delivered in phase 9 of the same tick ([Z2-N-ARCH-478]); the field order is the struct order of "Zenith_NetComponents header".

## Command kind payloads

| Kind name | Payload struct (bytes) | Ops | Allowed phases | Playback |
|---|---|---|---|---|
| `Net.SetOwner` | `Zenith_NetSetOwnerRequest` (24) | `SET = 1` | 1–10 (`m_uAllowedPhaseMask = 0x7FE`) | `Zenith2/Docs/Design/NET/Foundations.md` → "Ownership and the owner peer" |

## Configuration keys

Registered by `Zenith_Module_Net_RegisterTypes` through `xTypes.GetConfigSchema()` with CORE's key descriptors (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"; grammar `Zenith2/Docs/Design/PLAT/Configuration.md` → "Key grammar"). "Restart" marks a restart-required key; "Sim" a simulation-affecting key, which is also restart-required (PLAT's rule 1) and is written into the replay header.

| Key | Type and range | Default | Flags | Meaning |
|---|---|---|---|---|
| `net.role` | string: `offline`, `server`, `client` | `offline` | Sim | the run's role (Zenith2/Docs/Design/NET/Foundations.md → "Headless server composition") |
| `net.bindAddress` | string, an IPv4 address | `127.0.0.1` | Restart | the server's bind address and the client's local bind address; Stage 3 refuses anything but `127.0.0.1` |
| `net.port` | u32, 1,024 … 65,535 | 27870 | Restart | the server's UDP port |
| `net.connect` | string, `<ipv4>:<port>` | empty | Restart | the client's server address; Stage 3 refuses a non-loopback address |
| `net.inputDelayTicks` | u32, 1 … 16 | 4 | Restart | the client's command lead in ticks (Zenith2/Docs/Design/NET/Loopback.md → "Target tick mapping") |
| `net.redundantInputTicks` | u32, 0 … 3 | 3 | Restart | previous ticks' command blocks repeated in each INPUT |
| `net.snapshotIntervalTicks` | u32, 1 … 16 | 1 | Restart | the server captures every n-th tick |
| `net.connectTimeoutMs` | u32, 1,000 … 60,000 | 10000 | Restart | the client gives up connecting |
| `net.retryIntervalMs` | u32, 50 … 2,000 | 250 | Restart | CONNECT_REQUEST repeat interval |
| `net.peerTimeoutMs` | u32, 1,000 … 60,000 | 5000 | Restart | a peer silent for this long is disconnected |
| `net.keepaliveIntervalMs` | u32, 50 … 5,000 | 250 | Restart | a side that sent nothing for this long sends KEEPALIVE |
| `net.maxPeers` | u32, 1 … 1 (Stage 3) | 1 | Restart | peer slots of a server |
| `net.maxNetworkedEntities` | u32, 1,024 … 1,048,576 | 65536 | Sim | each world's `Zenith_NetworkIDMap` capacity |
| `net.server.commandHoldTicks` | u32, 0 … 16 | 4 | Restart | ticks a command waits for a missing earlier sequence (Loopback.md "Server command window") |
| `net.client.maxFragmentsInFlight` | u32, 32 … 4,096 | 256 | Restart | the client's `ASYNC_SOURCE_NETWORK` in-flight capacity per world |
| `net.socketBufferBytes` | u32, 65,536 … 8,388,608 | 1048576 | Restart | requested OS send and receive buffer sizes |
| `net.proof.mode` | bool | false | Restart | the Stage 3 proof assertions (Loopback.md "Proof assertions"); refused (forced false with a warning) in Shipping |
| `net.proof.minConnectedTicks` | u32, 1 … 100,000 | 300 | Restart | the proof's minimum connected ticks |
| `net.proof.assertTiming` | bool | true | Restart | whether the proof asserts zero late and zero lost commands |

## Misuse codes

The block ZS3400–ZS3499 is NET's, reserved under ruling MISUSE-BLOCKS-NEW-AREAS-3 (SCOPE holds ZS3200–ZS3299; SEC, PROC and MAINT register no block, MAINT having withdrawn its row X388) and recorded through cross-doc item X391 in `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges". ZS3401–ZS3404 are Stage 3's; ZS3405 and above are held for the Stage 7 bindings `Net.Connect`, `Net.Broadcast` and `Net.RewoundRaycast`, whose signatures and codes the Stage 7 revalidation ticket writes (`Zenith2/Docs/Design/NET/Stage7.md` → "Stage 7 bindings").

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS3401 | MISUSE | `Net.FindEntity` with a peer or counter that is not an integer in range | `Net.FindEntity expects a peer 0 … 65534 and a counter 1 … 281474976710655 for argument <Index>` |
| ZS3402 | MISUSE | `Net.SetOwner` called in a world whose role is client | `Net.SetOwner runs on the server; this world's role is client` |
| ZS3403 | MISUSE | `Net.SetOwner` on an entity without `Zenith.Net.Identity` | `entity <SimId> has no NetIdentity; add the component in its prefab` |
| ZS3404 | MISUSE | `Net.SetOwner` with a peer argument that is not an integer 0 … 65,534 | `Net.SetOwner expects a peer 0 … 65534, got <Value>` |
