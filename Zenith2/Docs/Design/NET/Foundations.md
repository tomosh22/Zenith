# NET design — multiplayer foundations

This file is part of the NET design family. The entry document is `Zenith2/Docs/Design/NET.md`. It elaborates `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" for the five foundation items that are architecture in Stage 1 and code in Stage 3: stable network IDs, the authority model, fixed ticks, replayable commands and headless execution. Architecture's declarations (`Zenith_NetworkID`, `Zenith_NetworkAuthority`, the `ZRPL` records, the headless bullets) are cited, never re-declared; the transport and the proof are `Zenith2/Docs/Design/NET/Loopback.md`.

## Scope of this file

| Item | Headings |
|---|---|
| Z2-I-NET-01 stable network IDs | "Network identity" and its sub-sections |
| Z2-I-NET-02 authority model | "Authority model" and its sub-sections |
| Z2-I-NET-03 fixed ticks | "Fixed ticks" and its sub-sections |
| Z2-I-NET-04 replayable commands | "Replayable commands" and its sub-sections |
| Z2-I-NET-05 headless execution | "Headless execution" and its sub-sections |
| the Stage 1 confirmation of all five | "Specification confirmation in Stage 1" |

## Network identity

### Network ID layout

The value type is Architecture's `Zenith_NetworkID` in `Zenith2/Engine/ZenithNet/Zenith_NetworkID.h` (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkID header"): 64 bits, bits 48–63 the allocating peer index, bits 0–47 that peer's counter. Peer 0 is always the server, and in an offline run (`NET_ROLE_OFFLINE`) the local process is peer 0. The value 0 is the null ID and is never issued, because every counter starts at 1. A counter never exceeds `ulNETWORK_ID_COUNTER_MAX` (2^48 − 1).

| Identity (Architecture/ECSModel.md table) | NET's relation to it |
|---|---|
| entity handle | never in a datagram, a publication payload or NET's save section ([Z2-N-CONTRACT-064]); the world's map turns an ID into a handle |
| stable simulation ID | orders NET's assignment pass; never sent (it is world-local and differs between server and client worlds) |
| persistent ID | identifies authored entities and assets; NET sends a proxy prefab as an index into the session's proxy table, never as an entity's persistent ID |
| network ID | the only entity identity NET sends |

### Peer index issuance

- The server is peer 0. Clients receive a peer index in CONNECT_ACCEPT (`Zenith2/Docs/Design/NET/Loopback.md` → "CONNECT_ACCEPT").
- The server's session world keeps `Zenith_NetIDAllocator::m_uHighestPeerIssued` (0 before any join), whose one writer is `Net.Session` from the recorded events: on each PEER_JOINED it sets the field to `max(field, peer)`, so a save written during a replay carries the same value as the live run's. The transport, which answers in the prologue before that tick runs, issues `max(m_uHighestPeerIssued, the highest index it issued since the world's last restore mark) + 1`. Likewise a client's `m_uLocalPeer` is written by `Net.Session` from the CONNECTED event. A leave never returns its index, so one world lineage (the world and every save and restore of it) never issues one index twice.
- A join that would need index 65,535 (`uNET_PEER_INDEX_NONE`) is rejected with `NET_REJECT_REASON_PEER_INDEX_EXHAUSTED`. This is 65,534 joins per lineage.
- After a restore, issuance continues from the restored `m_uHighestPeerIssued` (§ "Identity across save and restore"), so a peer index found in a restored ID is never issued again to a different client.
- Stage 3 serves at most one peer (`uNET_MAX_PEERS_STAGE3`), so the first join of a fresh server is peer 1.

### Network ID allocation

```text
Allocate(state):                                   // Net.IdentityIndex only, phase 1, single-threaded per world
  if state.m_eRole == NET_ROLE_CLIENT: return null           // Stage 3 clients allocate nothing (Stage 7: predicted spawns)
  if state.m_xAllocator.m_ulNextCounter > ulNETWORK_ID_COUNTER_MAX:
      log once "net: network ID counter of peer <p> exhausted in world <w>"; net.ids.exhausted += 1; return null
  id = Zenith_MakeNetworkID(state.m_xAllocator.m_uLocalPeer, state.m_xAllocator.m_ulNextCounter)
  state.m_xAllocator.m_ulNextCounter += 1
  return id
```

- Allocation is a pure function of the allocator's state and the order of calls; the order of calls is the order of § "Identity index system", which is ascending stable simulation ID, so the IDs are identical on every run, every worker count and every chunk layout ([Z2-N-ARCH-881]).
- An ID is never reused: the counter only increases, it is saved with the world, and nothing decrements it.
- Exhaustion leaves the entity without an ID (it is not networked) and is a recoverable condition counted in `net.ids.exhausted`; 2^48 − 1 assignments at one million per second take about 8.9 years, so no test reaches it except `Test_NetIdentity_CounterExhaustionStopsAssigning`, which restores an allocator at 2^48 − 2.

### Identity index system

`Net.IdentityIndex` is a C++ system registered per world at `AttachWorld`:

```cpp
Zenith_SystemBuilder& xBuilder = xContext.GetSystems().AddSystem(Zenith_SystemDesc{
	.m_szName = "Net.IdentityIndex",
	.m_ePhase = PHASE_INPUT,
	.m_pfnRun = &Zenith_NetIdentityIndex_Run,
	.m_pUserData = pxWorldState,
	.m_eAffinity = JOB_AFFINITY_ANY,
	.m_uCostHint = 2 });
xBuilder.Write(Zenith_NetIdentityComponent::xTYPE_ID, axNetworkIDProperty)     // property 1 only (authority Input)
	.Read(Zenith_NetIdentityComponent::xTYPE_ID, axFlagsProperty)               // property 4
	.WriteResource(Zenith_NetWorldStateResource::xTYPE_ID)
	.EmitsCommands()                                                            // the re-key of a duplicate (§ "Duplicate and unknown IDs")
	.Query(xIdentityQuery, m_xIdentitySlot);                                    // all rows carrying Zenith.Net.Identity
```

Per tick, in this order (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header" for the change filter; `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"):

1. Read `P = xContext.GetPreviousReadVersion()`.
2. For every chunk of the identity query and every row whose `GetRowChangeVersions` entry is newer than P (every row when P is 0), append `{ simulation ID, entity handle, chunk ordinal, row, NetworkID, Flags }` to an arena list. A row moved by compaction or restamped by playback is a candidate again; every step below is idempotent for it.
3. Sort the list by ascending simulation ID (CORE's sort, `Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching"); simulation IDs are unique, so the order is total. Cost O(c log c) for c candidates; the arena holds 48 bytes per candidate.
4. For each candidate in that order:
   1. Unknown flag bits (above bit 1) are cleared in the candidate's copy, counted in `net.ids.bad_flags`, and the property keeps its stored value (Flags is CommandsOnly; NET never writes it here).
   2. If NetworkID is 0: on a server or offline world, `Allocate`; a non-null result is written into the row's property 1 in place (this system is the property's owning-phase writer, [Z2-N-ARCH-483]) and inserted into the map; `net.ids.assigned` + 1. A row with `NET_IDENTITY_FLAG_PROXY` and ID 0 is a client defect (a proxy spawned without its ID) and is logged once per entity, never allocated.
   3. If NetworkID is non-zero and the map holds it for the same handle: nothing (a moved row).
   4. If NetworkID is non-zero and the map does not hold it: `Insert`. `NET_ERROR_MAP_FULL` leaves the entity unindexed (`net.ids.map_full` + 1, logged once per tick).
   5. If NetworkID is non-zero and the map holds it for a different live handle: § "Duplicate and unknown IDs".
5. Commit the written rows with `CommitRowChanges` through the chunk views of step 2.

`Net.IdentityIndex` runs on every world role, headless included, and under `--replay` exactly as live. It reads no clock and no transport state.

### Network ID map

`Zenith_NetworkIDMap` (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkIDMap header") is the world's map from network ID to entity handle that Architecture names. Each attached world owns one, created at `AttachWorld` with capacity `net.maxNetworkedEntities` (default 65,536, memory 2 × 65,536 rounded to 131,072 slots × 16 bytes = 2 MiB per world). Writers and the moment each writes:

| Writer | When | Change |
|---|---|---|
| `Net.IdentityIndex` | phase 1 | `Insert` of newly assigned and newly seen IDs; `Replace` for a handle change of a restored entity |
| NET's lifecycle listener (interest `LIFECYCLE_EVENT_DESTROY_PENDING`) | step 7 of the sync point that marks the destroy (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Listener interface") | reads property 1 through `Zenith_WorldPlaybackAccess::ReadProperty`; `Erase`; on a server, appends `{ ID, tick, remaining captures 30 }` to the world's pending-destroy list |
| the save participant | `RestoreSection` | `Clear`, then one `Insert` per restored identity (§ "Identity across save and restore") |

Readers declare a read of `Zenith.Net.WorldState`, so the schedule orders them after `Net.IdentityIndex` in phase 1 and anywhere in phases 2–12. The pending-destroy list holds each entry until `uNET_DESTROY_REPEAT_TICKS` (30) captures have carried it; a capture takes at most 256 entries, oldest first, so a burst of destroys is spread over several captures and each destroy is still repeated 30 times from its first send (`Zenith2/Docs/Design/NET/Loopback.md` → "Fragment encoding"). Its capacity is `net.maxNetworkedEntities`; an overflow drops the oldest entry and counts `net.ids.pending_destroy_dropped`.

### Duplicate and unknown IDs

A duplicate is an identity whose non-zero ID the map already holds for another live entity: an authored prefab or scene with a hand-set ID (the cook refuses it, § "Cook steps" of the entry document, so only hand-edited cooked data or a corrupt save reaches the runtime), or a client proxy spawned twice through a defect.

| Role | Action for the newcomer (the candidate with the higher simulation ID, because candidates run in ascending order) | Diagnostic |
|---|---|---|
| server or offline | record a `SetProperty` of property 1 to 0 for the newcomer through the system's command buffer; the next tick's pass allocates a fresh ID for it | `net.ids.duplicates` + 1; `net: duplicate network ID <hex> on simulation IDs <a> and <b>; the newer entity is re-keyed` |
| client | the newcomer is not indexed and receives no snapshot state; `net.ids.duplicates` + 1 | the same line with "is ignored" |

An unknown ID is one a datagram or a publication names that the map does not hold: a client destroy for an entity it never created, a server command naming nothing. Unknown IDs are dropped and counted (`net.client.unknown_destroys`), never asserted ([Z2-N-ARCH-951]).

### Identity across save and restore

NET registers one save participant per world at `AttachWorld` (`Zenith_WorldAttachContext::RegisterSaveParticipant`, `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header"):

| Member | Value |
|---|---|
| `GetName` | `"Zenith.Net.Identity"` |
| `GetSectionVersion` | 1 |
| `GetFlags` | 0 (included in resume images) |
| `GetCaptureFidelity` | `SAVE_FIDELITY_COHERENT` for a capture of a server or client world once its session has started (a server's first PEER_JOINED, a client's CONNECTED, recorded in the world state); `SAVE_FIDELITY_EXACT` for an offline world and for a server or client world before its session starts |
| `DeclareAccess` | a read of `Zenith.Net.WorldState` |
| `CollectExcludedEntities` | on a client world, every entity whose identity carries `NET_IDENTITY_FLAG_PROXY`: a proxy is the server's state and is never saved by a client |
| `GetReissuedSourceKinds` | 0 |

A live session's behaviour depends on module memory no section holds: a client's proxies and its apply tables (applied ticks, pending spawns, tombstones), a server's command window, its next expected sequence and its peers. A save of such a world restores consistently but cannot continue bit-identically, so it is COHERENT, and a replay resumed from it runs without hash comparison and says so ([Z2-N-ARCH-789]). An offline world, and a session world before its session starts, holds nothing outside the section and the entities, so its capture stays EXACT. The fidelity is a pure read of the world state the capture saw.

Section layout, version 1, little-endian (`Zenith2/Docs/Design/SAVE/Formats.md` → "Participant section framing" frames it):

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | `m_ulNextCounter` |
| 8 | `uint16` | `m_uLocalPeer` |
| 10 | `uint16` | `m_uHighestPeerIssued` |
| 12 | `uint8` | role at capture (`Zenith_NetRole`, 0 … 2); informational: logged, decides nothing |
| 13 | `uint8` | session state at capture (`Zenith_NetSessionState`, 0 … 5), consistent with the role: OFFLINE for the offline role; LISTENING or CONNECTED for a server; CONNECTING, CONNECTED, DISCONNECTED or FAILED for a client |
| 14 | `uint8` | session started at capture (`m_bSessionStarted`, 0 or 1): 1 requires a state of CONNECTED, DISCONNECTED or LISTENING, 0 a state of OFFLINE, LISTENING, CONNECTING or FAILED |
| 15 | `uint8` | reserved, 0 |
| 16 | `uint32` | identity count n at capture (identities with a non-zero ID, proxies excluded); informational: compared in step 7 and logged |
| 20 | `uint32` | reserved, 0 |

`RestoreSection`:

1. A section of other than exactly 24 bytes (`uNET_SAVE_SECTION_BYTES`), a version above 1, a counter of 0 or above 2^48, a role byte above 2, a state byte above 5 or inconsistent with the role byte, a started byte above 1 or inconsistent with the state byte, or a non-zero byte at offset 15 or 20–23 is a `SAVE_ERROR_CORRUPT`-class failure returned to SAVE ([Z2-N-ARCH-951]); SAVE's restore then fails as for any participant.
2. `m_bSectionPresent` false (an older save, or the module was not selected when the save was written): reset the allocator to `{ 1, 0, 0 }` and continue at step 4.
3. Adopt the counter and the highest issued peer. The local peer is reset to 0 for a server or offline world (the restoring world is never a client of an earlier session).
4. Clear the module memory of the world's session: on a client the fragment and session inboxes, the per-entity applied ticks and values, the pending-spawn table, the tombstones and the controlled entity; on a server the command window, the next expected sequence, the joined-peer set and the capture cursor; on both the pending-destroy list.
5. Set the session state from the section and the restoring world's role, a pure function of both, so the rule is identical live and in a replay that restores the same checkpoint. When the captured session had not started (started byte 0: a captured state of OFFLINE, LISTENING, CONNECTING or FAILED), adopt the captured state as it is: the capture was EXACT, nothing of a session existed, the restore mark does not move and a client transport that has not yet connected carries on (a client resumed at startup still joins). Otherwise (started byte 1) a client world becomes `DISCONNECTED` (reason `NET_DISCONNECT_REASON_RESTORED`) and a server world `LISTENING`, `m_bSessionStarted` is cleared, and the world state's restore mark moves, from which the next frame prologue ends the transport's live session (the client sends DISCONNECT `RESTORED` and its transport is `DONE`; the server's slot sends it and lingers). A section that is absent (step 2) leaves the role's initial state. No session event is published in either case: the restore itself is the transition.
6. `Clear` the map; walk every restored entity carrying `Zenith.Net.Identity` through `Zenith_SaveRestoreContext::m_pxWorld` in ascending network ID; `Insert` each non-zero ID; a duplicate is logged and its newcomer re-keyed at the first tick by § "Duplicate and unknown IDs". Raise the counter to (the largest restored peer-0 counter + 1) when it is lower, and the highest issued peer to the largest restored peer index when it is lower, each logged as `net: restored allocator raised to <value>`; the section's value is never lowered.
7. Compare the indexed count with n; a difference is logged (`net: restore indexed <k> identities, the save recorded <n>`) and is not a failure.

The IDs themselves need no NET code to persist: property 1 is a serialized property of `Zenith.Net.Identity` and travels with every entity SAVE captures. So after a save at tick t and a restore, every entity keeps its network ID and the next assignment is the counter the original run would have used; a replay resumed from an EXACT save (an offline world, or a session world before its session, restored to its captured session state by step 5) continues bit-identically ([Z2-N-ARCH-789]; ruling Q70 (a) as amended by Q82 (b)). `Test_NetIdentity_SessionWorldCaptureIsCoherent`, `Test_NetIdentity_RestoreClearsApplyTables` and `Test_NetIdentity_RestoreOfPreSessionClientKeepsConnecting` hold the three cases of this section.

### Identity in replays

Under `--replay`, IDs are reproduced without any record of their own: the server's and the offline world's assignments are a function of simulation state, and a client's proxies take their IDs from the recorded snapshot publications. `Zenith_NetworkIDMap` contents are therefore identical at every tick of a replay, which `Test_NetIdentity_ReplayReproducesAssignments` asserts per tick.

### Identity tests

The identity tests are listed in `Zenith2/Docs/Design/NET/Tests.md` → "Identity tests".

## Authority model

### Authority values and meanings

The enumeration is Architecture's `Zenith_NetworkAuthority` (`Engine/ZenithReflect/Zenith_NetworkAuthority.h`, registered per component through `NetworkAuthority` in C++ and `networkAuthority` in schemas, default `NETWORK_AUTHORITY_SERVER`); the per-property flag is ECS's `PROPERTY_FLAG_REPLICATED` (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeInfo header"). NET defines what each value means in every role:

| Authority | Server | Client (Stage 3) | Client (Stage 7) | Offline |
|---|---|---|---|---|
| `NETWORK_AUTHORITY_SERVER` | simulates; captures replicated properties | applies snapshot values to proxies; never sends the component | as Stage 3, plus prediction of owned entities (`Zenith2/Docs/Design/NET/Stage7.md` → "Predicted entities") | simulates |
| `NETWORK_AUTHORITY_OWNER_CLIENT` | treated as SERVER (diagnostic note at start) | treated as SERVER | the owning client proposes values; the server validates and rebroadcasts (`Zenith2/Docs/Design/NET/Stage7.md` → "Owner-client authority") | simulates |
| `NETWORK_AUTHORITY_LOCAL` | never captured | never applied; simulated locally on every peer | as Stage 3 | simulates |

- The server is authoritative for every replicated property at Stage 3: no message kind of wire format version 1 carries component state from a client, so a client cannot change server state other than through input commands.
- Per-field write authority inside a world (which phase writes a property) is a separate axis and stays GAME's contract (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Authority census"); network authority decides which peer's world is the source of truth.

### Declaration rules

| Rule | Checked by | Outcome |
|---|---|---|
| a replicated property belongs to a component (`TYPE_KIND_COMPONENT`) | `Zenith_NetReplicationTable::Build` | `NET_AUTHORITY_DIAG_NON_COMPONENT`, error |
| a `NETWORK_AUTHORITY_LOCAL` component has no replicated property | the same | `NET_AUTHORITY_DIAG_LOCAL_REPLICATED`, error |
| NET's own components replicate nothing (identity travels structurally) | the same | `NET_AUTHORITY_DIAG_NET_TYPE_REPLICATED`, error |
| at Stage 3 a replicated property is `PROPERTY_TYPE_VEC3` or `PROPERTY_TYPE_QUAT` | the same | `NET_AUTHORITY_DIAG_UNENCODABLE`, warning: the property is left out of the table |
| at most 32 encodable entries | the same | `NET_AUTHORITY_DIAG_TOO_MANY`, error |
| an `OWNER_CLIENT` component | the same | `NET_AUTHORITY_DIAG_OWNER_CLIENT_STAGE3`, note |
| event schemas carry no `networkAuthority` or `replicated` key | ECS's schema validation ([Z2-N-ARCH-610]) | `SCHEMA_ERROR_UNKNOWN_KEY` |

Data-defined components follow the same rules with no difference ([Z2-N-ARCH-592], [Z2-N-ARCH-643]): `Build` reads only `Zenith_TypeInfo` and `Zenith_PropertyInfo` and never branches on `m_eOrigin`.

### Replication table construction

`Zenith_NetReplicationTable::Build(xTypes, axDiagnostics, uCount, xHeap)`, called once in `Zenith_Module_Net_Start` on the frozen registry:

1. For every type of `xTypes.GetTypes()` (byte order of stable name) and every property in ascending property ID with `PROPERTY_FLAG_REPLICATED` set, apply the rules of § "Declaration rules" and append diagnostics.
2. For each surviving property, choose the codec: `PROPERTY_TYPE_VEC3` → `NET_CODEC_POSITION_MM` (12 bytes); `PROPERTY_TYPE_QUAT` → `NET_CODEC_QUAT_SMALLEST_THREE` (4 bytes).
3. Sort the entries by (type ID, property ID) ascending — numeric order of the 64-bit type ID, then the 32-bit property ID — so both peers order them identically whatever their registration order.
4. Error diagnostics fail `Build` with `NET_ERROR_AUTHORITY`; `Start` then logs every diagnostic (`net: authority: <code> <Type>.<prop>`) and fails with `MODULE_ERROR_REGISTRATION` (startup exit code 3).
5. The table hash is `Zenith_Hash64` over, per entry, the little-endian `uint64` type ID, `uint32` property ID, `uint8` value type and `uint8` codec (14 bytes each), in table order; an empty table hashes the empty input.

Complexity O(T + P log P) for T types and P replicated properties. In the Stage 3 loopback content the table has two entries, `Zenith.LocalTransform` properties 1 (Position, POSITION_MM) and 2 (Rotation, QUAT_SMALLEST_THREE), once SCENE flags them replicated (cross-area row NT8).

### Authority validation at module start

`Zenith_Module_Net_Start` builds the table in every configuration, Shipping included, and under every role, so an authority error stops a game at its first start on any machine rather than in a multiplayer session. The diagnostics buffer is 256 entries; a registry with more logs the first 256 and the count. The validation reads nothing but the registry, so it is identical in the editor's runtime, the cook's NET check and the Player.

### Ownership and the owner peer

`Zenith.Net.Identity` property 3, `OwnerPeer`, names the peer whose input controls the entity: 0 (the server or the offline player) by default.

- It changes only through the `Net.SetOwner` command (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Command kind payloads"), recorded by `Net.SetOwner(ctx, entity, peer)` or by C++ through `Zenith_NetRecordSetOwner(Zenith_CommandBuffer&, Zenith_EntityHandle, uint16)`.
- Playback (sync point after the issuing phase, step 7, single-threaded per world): drop a payload shorter or longer than 24 bytes (`net.commands.malformed`), a target of another world, a stale target, a target without `Zenith.Net.Identity`, a peer above `uNET_PEER_INDEX_MAX` (each `net.commands.rejected` + 1 with a `net` log line naming the reason), and every request in a client world (`net.commands.rejected`, reason `client role`); otherwise write property 3 through `Zenith_WorldPlaybackAccess::SetProperty`.
- At Stage 3 the server routes the input of the one connected peer to player slot 0 (§ "Remote commands enter phase 1"); `OwnerPeer` is how the client learns which proxy it controls (`Net.ControlledEntityChanged`) and how the server's game script records which character the peer drives. GAME's ownership chain (`Zenith2/Docs/Design/GAME/Framework.md` → "Ownership") is unrelated metadata until Stage 7 maps an owning player to its peer (Z2-R-GAME-137).

### Authority on clients

On a client, `Net.ClientApply` is the only writer of a replicated property of a proxy, through `SetProperty` commands recorded in phase 1 (`Zenith2/Docs/Design/NET/Loopback.md` → "Client apply"). Proxy prefabs carry no physics body, character controller or root-motion graph, so no other system of the client writes those properties; the authoring check enforces the rule for the components it can see (`Zenith2/Docs/Design/NET.md` → "Cook steps"), and the proof's apply verification detects any other writer at run time (`Zenith2/Docs/Design/NET/Loopback.md` → "Proof assertions"). Non-proxy client entities (the local player, the camera, UI) are simulated locally and never sent.

### Authority census rows

Rows NET adds to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the diff that registers each type, in the twelve-column format of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format"; `Library` is `ZenithNet` and `Stage` is `S3`. No Lifecycle suite links ZenithNet, so the rows are checked by review and by `Test_NetModule_RegistrationShape` (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census rows added by subsequent designs").

| Stable name | Kind | Properties | Owning phase | Writer sources | Arbitration | Writers | Readers outside the owning phase |
|---|---|---|---|---|---|---|---|
| `Zenith.Net.Identity` | component | 1 NetworkID | Input | — | none | `Net.IdentityIndex` (in place, and by command for the re-key of a duplicate); `Net.ClientApply` through the spawn initialiser of a proxy | `Net.Capture` (11), `Net.ClientApply` (1, after), the `Net` bindings (2, 10) |
| `Zenith.Net.Identity` | component | 2 ProxyPrefab, 3 OwnerPeer, 4 Flags | CommandsOnly | — | none | prefab authoring; `Net.SetOwner` playback (3); `Net.ClientApply` commands (3, 4 on proxies) | `Net.Capture` (11), `Net.IdentityIndex` (1, property 4), the bindings |
| `Zenith.Net.ProxyPrefabSet` | component | 1 Prefabs | CommandsOnly | — | none | scene authoring | `Net.Session` (1, the first tick) |
| `Zenith.Net.WorldState` | resource | — | Input | — | none | `Net.IdentityIndex`, `Net.Session`, `Net.ClientApply`; the lifecycle listener and the save participant outside phases | the bindings, `Net.Capture` (11) |
| `Zenith.Net.Capture` | resource | — | SaveSnapshot | — | none | `Net.Capture` | `Net.Collect@W<tag>` (epilogue) |
| `Net.SessionChanged`, `Net.PeerJoined`, `Net.PeerLeft`, `Net.ControlledEntityChanged` | event | — | — | — | — | `Net.Session`, `Net.ClientApply` | scripts (9, 10) |

The `Zenith.Net.Capture` row is the one phase-11-owned module resource; the four statements of what phase 11 may write ([Z2-N-ARCH-476], [Z2-N-ARCH-503], [Z2-N-ARCH-731] and `Zenith2/Docs/Design/SAVE.md` → "Determinism obligations") are amended for it together by cross-doc item X396. The engine-wide epilogue stage `Net.Send` writes the one world-independent resource `Zenith.Net.Transport` that [Z2-N-ARCH-442] admits for such a stage; `Zenith_Module_Net_RegisterTypes` registers it, it belongs to no world and so has no row above, and `Test_NetModule_RegistrationShape` asserts its registration.

### Network authority report

The development console command `net.authority` (`Zenith2/Docs/Design/NET.md` → "Console commands") prints the table: one line per component type with its authority and its replicated properties, codec and wire bytes, then the warnings, then `hash 0x<16 hex>`. `Test_NetAuthority_ReportListsTableInOrder` asserts the format on a registry with three test components.

## Fixed ticks

### Tick-rate agreement

The simulation runs on fixed ticks of `sim.tickHz` (JOBS's simulation clock, `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Execution"). NET adds no clock of its own; every NET message that concerns simulation names a tick index, and peers agree on the tick duration:

- CONNECT_REQUEST carries the client's `sim.tickHz`; the server rejects a different value with `NET_REJECT_REASON_IDENTITY`, detail `REPLAY_IDENTITY_FIELD_TICK_RATE` (the same field index SAVE uses for replay identity, `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header").
- `sim.ticksPerFrame` other than 0 (external-tick mode) is refused for the roles server and client at Start (`MODULE_ERROR_REGISTRATION`, `net: net.role=<role> needs real-time ticks; sim.ticksPerFrame is <n>`), because a networked peer must advance with wall-clock time. Offline and `--replay` runs accept it: under `--replay` the check is skipped with the rest of the live-only start steps (`Zenith2/Docs/Design/NET.md` → "Module start").

### Tick-indexed messages

| Message field | Tick it names |
|---|---|
| INPUT block target tick | the server tick whose phase 1 applies the block's commands |
| SNAPSHOT server tick | the server tick whose phase 11 captured the fragment |
| CONNECT_ACCEPT server tick | the server's last completed tick when the accept was queued |
| INPUT digest pair tick | a server tick whose fragments the client received completely |
| KEEPALIVE tick | the sender's last completed tick (diagnostic) |

A tick index is the session world's `uint64` tick index (`Zenith_SystemContext::GetTickIndex`, `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"); tick indices of different processes are related only through the client's offset of `Zenith2/Docs/Design/NET/Loopback.md` → "Target tick mapping".

### Catch-up and paused frames

- A frame that runs several ticks (bounded catch-up, `sim.maxCatchUpTicks`) runs every NET phase system once per tick; the server captures every tick's state and sends every captured tick in that frame's `Net.Send`; the client's bridge observes every tick's commands and the next `Net.Send` sends one INPUT block per tick.
- A frame that runs no tick sends no snapshot and no new command block; keepalives keep the session alive (`net.keepaliveIntervalMs`).
- A paused session world (`sim.paused`) runs no NET phase system, so its tick index stands still; the server's command window keeps the peer's blocks, and a block whose target passes while paused is applied at the first tick after resuming and counted in `net.late_commands`.
- Dropped real time (ticks beyond `sim.maxCatchUpTicks`, counted in `sim.dropped_ticks`, [Z2-N-ARCH-414]) changes which server tick a client's late commands land in, never the replay of either side, because each side logs the ticks it ran.

### Clock discipline

NET's phase systems read no clock ([Z2-N-ARCH-885]): they read tick indices, the recorded publications and the command source. Wall-clock time (`Zenith_GetMonotonicTimeUs`, `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks") is read only by the transport in frame-prologue step 1 and in `Net.Send`, for timeouts, retries, keepalives and round-trip diagnostics, none of which reaches simulation except through recorded publications.

## Replayable commands

### Remote commands enter phase 1

Gameplay input reaches simulation only as replayable input commands logged at phase 1 ([Z2-N-ARCH-783]). A remote peer's input is no exception: the server's NET module supplies the remote peer's commands to the world's phase-1 `Input.Actions` system through PLAT's input command bridge (assumption pending PLAT and MOD, rows NT3 and NT5), which applies them to the world's action state exactly as local commands and appends them to the replay log at step 5 of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system". Consequences:

- On the server, a remote command and a local one are indistinguishable to gameplay, to the replay log and to the tick hash.
- At Stage 3 the server applies the one connected peer's commands to player slot 0, the slot GAME's single player owns (`Zenith2/Docs/Design/GAME/Framework.md` → "Player and pawn components"); a world whose bridge answers `ReplacesLocalInput()` true (NET's server world) maps no local transitions (its slice is empty), so local and remote commands never share a sequence space; a client world's bridge answers false, so the client's own input is mapped, applied, logged and observed as in any single-player world.
- Stage 7 routes each peer to its own player slot (`Zenith2/Docs/Design/NET/Stage7.md` → "Player slots and per-peer input").

### Command source contract

<!-- assumption: pending PLAT, MOD -->
PLAT's runtime bridge declares, in `Zenith2/Engine/ZenithPlatform/Zenith_InputCommand.h` (below every module, so the call points downward):

```cpp
/// @contract interface Zenith_InputCommandBridge
/// Ownership: Implemented by a module (NET); registered per world with Zenith_WorldAttachContext::SetInputCommandBridge at
///   AttachWorld; outlives the world's Input.Actions node.
/// Thread affinity: PHASE(Input). Called on the world's Input.Actions node, once per tick of that world.
/// Invalidation: axOut and axCommands are valid for the call.
/// Units: Ticks; commands.
/// Errors: None; ProvideTickCommands returns the count written (at most axOut.size).
/// Reentrancy: NONE. An implementation calls back into no PLAT or ECS member.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_InputCommandBridge
{
public:
	virtual ~Zenith_InputCommandBridge() = default;
	// Whether the mapper's local slice is replaced: true makes Input.Actions map no local transitions for the world (its
	// slice is empty); false keeps the local slice. Constant for the bridge's lifetime; read once per tick before step 1.
	virtual bool ReplacesLocalInput() const = 0;
	// Live runs only (never under --replay): the tick's external commands, applied after the mapper's slice (empty when
	// ReplacesLocalInput() is true) and before step 4's rebuild; logged at step 5 like local commands.
	virtual uint32 ProvideTickCommands(uint64 ulTick, Zenith_Span<Zenith_InputCommand> axOut) = 0;
	// Every run, live and replay: the tick's final command list, after step 4 (what the replay log records for the tick).
	virtual void ObserveTickCommands(uint64 ulTick, Zenith_Span<const Zenith_InputCommand> axCommands) = 0;
};
```

and MOD's `Zenith_WorldAttachContext` gains `void SetInputCommandBridge(Zenith_InputCommandBridge& xBridge)` (`MAIN_THREAD`, during `AttachWorld` alone, at most one per world, journaled and undone with the module). Under `--replay` step 1 of the Input phase system reads the log and calls `ProvideTickCommands` never; it still calls `ObserveTickCommands`.
<!-- /assumption -->

NET's implementation (`Zenith_NetCommandSource`, `Zenith2/Docs/Design/NET/PublicAPI.md` → "Internal declarations"):

| Role | `ReplacesLocalInput()` | `ProvideTickCommands(t)` | `ObserveTickCommands(t, commands)` |
|---|---|---|---|
| server | true | the commands the server command window holds for t, then any late commands, in ascending sequence, at most `axOut.size` (`Zenith2/Docs/Design/NET/Loopback.md` → "Command delivery to phase 1") | nothing |
| client | false (the client's local input is its own and is what it sends) | 0 | while the transport has adopted an ACCEPT (`m_bAdopted`, module memory written in the prologue), copies the commands into the outbound ring as the block of client tick t (`Zenith2/Docs/Design/NET/Loopback.md` → "Client command capture") |
| offline | not registered | not registered | not registered |

Stage 7's listen server answers false for peer 0's local player, whose input is already phase-1 input (`Zenith2/Docs/Design/NET/Stage7.md` → "Peer slots and join in progress").

### Command wire encoding

A command crosses the network in exactly the 18-byte record of the `ZRPL` tick block (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation"): `uint16` player slot, `uint32` action ID, `uint32` sequence, 8 bytes of quantised value, little-endian. NET's encoder and decoder are its own code over that layout, and `Test_NetWire_CommandRecordMatchesReplayEncoding` writes the same commands through SAVE's `Zenith_ReplayLogWriter` and through NET's INPUT encoder and compares the 18-byte records byte for byte. Values are never re-quantised: the server applies exactly the bits the client's encoder emitted, so the client's local action state and the server's agree to the bit.

### Command replay

| Log | What reproduces it under `--replay` |
|---|---|
| server | phase 1 reads the remote peer's commands from the log; session publications and the client's absence come from the log; NET opens no socket |
| client | phase 1 reads the client's own commands from the log; snapshot fragments and session events are recorded publications; NET opens no socket |
| offline | as any single-player replay |

`Test_Net_LoopbackLogsReplayBitIdentical` replays both logs of the loopback run and requires SAVE's replay verifier to report no tick-hash mismatch (`Zenith2/Docs/Design/NET/Loopback.md` → "Replay verification").

## Headless execution

### What a headless run contains

The headless configuration is Architecture's ([Z2-N-ARCH-785], [Z2-N-ARCH-849], [Z2-N-ARCH-867], [Z2-N-ARCH-868]): the same executable with `--headless`, startup steps 12 and 13 skipped, no window, no audio output device, no render subgraph, every simulation system unchanged. NET adds nothing to that list and needs nothing from a device: `ZenithNet` declares no dependency on ZenithFlux, ZenithRHI or ZenithAudio, its module starts identically headless (the module table's "unchanged", `Zenith2/Docs/Architecture.md` → "Engine modules and game projects"), and its only OS resource is PLAT's datagram socket, which exists in the headless platform mode (`PLATFORM_MODE_HEADLESS`, `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Platform lifecycle"; the socket itself is assumption NT1).

### Headless server composition

A Stage 3 server is the game's own Player executable started headless with `net.role=server`. Its composition, startup step by step (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"):

| Step | Server behaviour |
|---|---|
| 4 | `Zenith_Platform` in `PLATFORM_MODE_HEADLESS` (SDL events only) |
| 6 | `Zenith_Module_Net_RegisterTypes`; the `net.*` keys enter the schema; `net.role` is simulation-affecting, so `--record` writes it into the replay header |
| 9 | the job system without the render-submit thread |
| 12, 13 | skipped: no RHI device, no renderer |
| 15 | `Zenith_Module_Net_Start`: parses the keys, refuses `sim.ticksPerFrame` ≠ 0, builds the replication table, registers the frame listener, the `Net.Send` engine stage, the `Net.SetOwner` command kind, the bindings and the `ASYNC_SOURCE_NETWORK` declaration hook; creates the transport and binds `net.bindAddress:net.port` unless `IsReplaying` |
| 16 | `AttachWorld` of the startup world: per-world state, the map, the systems, the bridge, the lifecycle listener, the save participant, the publication source; the world becomes the session world |
| 17 | the startup scene loads; its `Zenith.Net.ProxyPrefabSet` makes the proxy prefabs resident |
| 19 | the frame loop: prologue step 1 pumps the socket, phases 1 and 11 run NET's systems, `Net.Send` sends; headless frame pacing (§ "Headless frame pacing") |

- Modules that need a device start in their headless modes (AUDIO's null output device, [Z2-N-ARCH-867]); a server game selects modules exactly as its client does, because the composition, content and build identities must match for a client to join (`Zenith2/Docs/Design/NET/Loopback.md` → "Handshake rules").
- Exit: `--exit-after-ticks n` ends the run after n ticks of the session world; shutdown sends DISCONNECT (`SERVER_SHUTDOWN`) to every peer before the module instance is destroyed (shutdown step 6).
- The Stage 7 dedicated server build is a separate executable shape of the same composition (`Zenith2/Docs/Design/NET/Stage7.md` → "Server build target").

### Headless client composition

The Stage 3 loopback client is also headless (the CI host has no GPU): the same steps with `net.role=client` and `net.connect`. Rendering is not part of the proof: a windowed client renders the proxies like any entity, and `Test_NetClient_WindowedRendersProxies` is a `needs-gpu` render test (`gpu-reason:requires-graphics-test`) of the Stage 3 suite.

### Headless frame pacing

<!-- assumption: pending MOD, JOBS -->
A headless run in real-time mode (`sim.ticksPerFrame` 0) has no present to pace its frames, so its frame loop would spin. NET relies on this rule of MOD's frame loop driver and JOBS's clock (row NT7), which MOD's own requirement carries: after frame-loop step 4, a headless real-time run sleeps while the simulation clock's next tick is not yet due, through `Zenith_SimulationClock::GetMicrosecondsUntilNextTick()` (JOBS) and `Zenith_SleepUs` (`Zenith2/Docs/Design/CORE/Threading.md`), pacing at most 1,000 µs per iteration and never sleeping when the value is 0. CORE gives no timer-resolution guarantee, so a wake can be late by the OS's resolution; a frame with no tick still runs the prologue, `RunFrame`, the epilogue and `EndFrame`, so a headless process uses little CPU between ticks, not none. If MOD declines NT7, the loopback driver passes `--set net.proof.assertTiming=false` (`Zenith2/Docs/Design/NET/Loopback.md` → "Driver script") and the command criterion rests on the verifier's checks alone.
<!-- /assumption -->

### Shipping and the headless roles

At Stage 3 the transport is development-only: in a Shipping build `Zenith_Module_Net_Start` forces `net.role` to `offline` when it is `server` or `client`, logs `net: net.role=<role> is unavailable in Shipping builds until Stage 7`, creates no socket and sets `NET_ERROR_NOT_IN_SHIPPING` in the startup report's module note (the run continues offline). The guard is `if constexpr (Zenith_IsShippingBuild())` in the module's start function ([Z2-N-ARCH-189]; the precedent is DEVT's guard of its development services, `Zenith2/Docs/Design/DEVT/Console.md` → "Shipping absence of development services"; no preprocessor conditional, rule F11), and the transport's entry symbol `Zenith_NetTransport::Create` is listed in `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` by a PROC hand-off (row NT9), so a Shipping executable that still links it fails the `Libgraph` step. Stage 7 lifts the guard together with authenticated sessions (`Zenith2/Docs/Design/NET/Stage7.md` → "Session security"). Network IDs, the authority table and the bindings work in Shipping in every role, because single-player Shipping games that select `Net` still allocate IDs offline.

## Specification confirmation in Stage 1

Roadmap item 16 of Stage 1 (the multiplayer foundations) is a specification ticket, not code: no ZenithNet source exists before Stage 3 (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping" gives the module's first stage as S3). The ticket:

1. reads this file and `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" against the Stage 1 rulings that touch them: D-PHASES refinement (open, Stage 1) for phase 1, phase 11 and command playback; D-SCRIPT-TXN refinement (open, Stage 1) for asynchronous publication; D-SEL-HASH (open, Stage 1) for the table hash and the map hash;
2. confirms, or amends in the same diff, the ID layout, the allocation order, the authority table, the tick-agreement rule, the command encoding and the headless composition;
3. appends one ruling to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN`, titled "Multiplayer foundations confirmed", whose nouns are those of Z2-D-073 (Z2-D-097);
4. confirms that cross-doc items X392 (the asynchronous source kind), X396 (the phase-11 capture resource) and the PLAT and MOD rows NT1, NT3, NT5, NT6 and NT7 are applied or still queued, and lists any still queued in the ruling.
