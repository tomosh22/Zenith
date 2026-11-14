# NET requirement catalogue

## Purpose

The requirement catalogue of the NET area (multiplayer): every obligation of `Zenith2/Docs/Design/NET.md` and its section files, one testable requirement each, mapped to the inventory items Z2-I-NET-01 … Z2-I-NET-16 of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1". The foundations (Z2-I-NET-01 … 05) are confirmed in Stage 1 and implemented with the loopback proof (Z2-I-NET-06) in Stage 3; the deferred Stage 7 tree (Z2-I-NET-07 … 14, Z2-D-073) is catalogued in full in `Zenith2/Docs/Requirements/NET/Stage7.md`, which continues this numbering (Z2-R-NET-108 … Z2-R-NET-158); local multiplayer and split-screen (Z2-I-NET-15, 16) are excluded (Z2-D-076). A requirement's stage is the stage whose acceptance it serves.

## Requirements

### Z2-R-NET-001 Foundations confirmed in Stage 1

- **Statement:** The Stage 1 multiplayer-foundations specification ticket must confirm, or amend in the same diff, the network ID layout and allocation order, the authority table, the tick-agreement rule, the command encoding and the headless compositions against the Stage 1 rulings it names, and must append the confirmation ruling to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN`.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02, Z2-I-NET-03, Z2-I-NET-04, Z2-I-NET-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-097, D-PHASES (open, Stage 1), D-SCRIPT-TXN (open, Stage 1), D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the specification ticket's DecisionLog entry and edited design sections, checked by the `Coverage` gate step in acceptance mode `-Stage 1`
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Specification confirmation in Stage 1"; `Zenith2/Docs/Design/NET.md` → "Stage 1"

### Z2-R-NET-002 Foundation seams queued in the core documents

- **Statement:** The Stage 1 specification ticket must list, in its ruling, each of cross-doc items X391 to X399 and rows NT1, NT2, NT3, NT5, NT6 and NT7 as applied or still queued, so no Stage 3 NET ticket becomes claimable on a seam that does not exist.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02, Z2-I-NET-03, Z2-I-NET-04, Z2-I-NET-05, Z2-I-NET-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the ruling's list; the `Coverage` gate step in acceptance mode `-Stage 1` resolving every cited core-doc heading
- **Design:** `Zenith2/Docs/Design/NET.md` → "Assumed interfaces pending other areas"; `Zenith2/Docs/Design/NET/Foundations.md` → "Specification confirmation in Stage 1"

### Z2-R-NET-003 ZenithNet module library

- **Statement:** `ZenithNet` must be a `KIND MODULE` static library with `MODULE_NAME Net`, `SUPPORTED ZENITH_GPU`, the `DEPENDS` of "Libraries NET depends on" and exactly the root headers of "Header list", with no `Internal/<Platform>/` directory.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Libgraph`, `SourceLayout` and `CompileCoverage` gate steps; `Test_NetModule_RegistrationShape` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Targets"; `Zenith2/Docs/Design/NET.md` → "Libraries NET depends on"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Header list"

### Z2-R-NET-004 Public header rules across ZenithNet

- **Statement:** Every ZenithNet public header must carry one contract block per type, name no STL or platform socket type, and compile alone.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-041, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` and `Libgraph` gate steps
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Rules across the ZenithNet headers"

### Z2-R-NET-005 Module entry points and registrations

- **Statement:** `Zenith_Module_Net_RegisterTypes` must register the two components, the payload struct, the four event types, the world resources `Zenith.Net.WorldState` (owning phase Input) and `Zenith.Net.Capture` (owning phase SaveSnapshot), the world-independent resource `Zenith.Net.Transport` and every Stage 3 configuration key, and `Zenith_Module_Net_Start` must perform the steps of "Module start" in order, unwinding on failure.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_NetModule_RegistrationShape`, `Test_NetModuleInstance_Create_FailsAtConfig_Unwinds`, `Test_NetModuleInstance_Create_FailsAtTable_Unwinds`, `Test_NetModuleInstance_Create_FailsAtTransport_Unwinds`, `Test_NetModuleInstance_Create_FailsAtPort_Unwinds` (`Test` gate step). This requirement deliberately keeps registration and start together, because one test fixture asserts both and neither is observable alone.
- **Design:** `Zenith2/Docs/Design/NET.md` → "Module start"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_Module_Net header"

### Z2-R-NET-006 World attach and detach

- **Statement:** `AttachWorld` must create the per-world state, map, systems, listener and participant for every play world, bind exactly one session world for a server or client, and `DetachWorld` of the session world must end the session with no completion ID outstanding.
- **Covers:** Z2-I-NET-01, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ModuleStartsWithoutDevice`, `Test_NetSession_WorldSwapEndsSession` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "World attach and detach"

### Z2-R-NET-007 Network ID value type

- **Statement:** `Zenith_NetworkID` must be Architecture's 8-byte layout verbatim (bits 48–63 peer, bits 0–47 counter, 0 invalid) with the comparison operators and the three helper functions of "Zenith_NetworkID header".
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetworkID_LayoutAndHelpers` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkID header"; `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID layout"

### Z2-R-NET-008 Network messages carry network IDs, never handles

- **Statement:** No datagram, publication payload or NET save section may contain an entity handle, a stable simulation ID or an entity's persistent ID; entities travel only as network IDs and proxy prefabs as proxy-table indices.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_SnapshotLimits`, `Test_NetPublication_PayloadLayout`, `Test_NetIdentity_SectionCorruptRefused` (layouts asserted byte for byte; `Test` gate step); review against [Z2-N-CONTRACT-064]
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID layout"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Rules across the ZenithNet headers"

### Z2-R-NET-009 Peer index issuance

- **Statement:** The server must be peer 0 and must issue each join the highest issued index plus one, never return a left peer's index, continue from the restored highest index after a restore, and reject a join needing index 65,535 with `NET_REJECT_REASON_PEER_INDEX_EXHAUSTED`.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_HandshakeOverMemoryPort`, `Test_NetIdentity_PeerIndexContinuesAfterRestore`, `Test_NetSession_RejectReasons` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Peer index issuance"

### Z2-R-NET-010 Deterministic network ID allocation

- **Statement:** On a server or offline world, network IDs must be allocated by `Net.IdentityIndex` in ascending stable simulation ID from the world's allocator, identically for 1, 2, 8 and all workers, forced compaction and two chunk capacities on every ECS backend; a client must allocate none at Stage 3.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_AssignsInSimulationIDOrder`, `Test_NetIdentity_IndependentOfWorkersAndLayout`, `Test_NetIdentity_OfflineAllocatesClientDoesNot` (`Test` gate step, every backend)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID allocation"; `Zenith2/Docs/Design/NET/Foundations.md` → "Identity index system"

### Z2-R-NET-011 Network IDs never reused

- **Statement:** A network ID counter must only increase, must never reissue a value, and on reaching 2^48 − 1 must stop assigning with `net.ids.exhausted` counted and one log line.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_CounterExhaustionStopsAssigning` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID allocation"

### Z2-R-NET-012 Identity index system

- **Statement:** `Net.IdentityIndex` must be a phase-1 system declaring a write of `Zenith.Net.Identity` property 1 and of `Zenith.Net.WorldState`, must process change-filtered rows idempotently in ascending simulation ID, and must commit the rows it writes.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_MovedRowIsIdempotent`, `Test_NetModule_RegistrationShape` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Identity index system"

### Z2-R-NET-013 Network ID map container

- **Statement:** `Zenith_NetworkIDMap` must have the fixed capacity, open-addressing layout, error results and ascending-only enumeration of its contract block, allocate only at `Create`, and never expose iteration order.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_NetworkIDMap_InsertFindErase`, `Test_NetworkIDMap_FullAndDuplicate`, `Test_NetworkIDMap_CopyAscending`, `Test_NetworkIDMap_Create_FailsAtTable_Unwinds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkIDMap header"; `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID map"

### Z2-R-NET-014 Map writers and destroy handling

- **Statement:** The map must be written only by `Net.IdentityIndex`, NET's destroy-pending lifecycle listener and the save participant, and on a server a destroy must erase the ID at its sync point and queue it in the pending-destroy list until 30 captures have carried it.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_DestroyErasesAndQueues`, `Test_NetIdentity_MapFullLeavesUnindexed` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID map"

### Z2-R-NET-015 Duplicate network IDs

- **Statement:** A non-zero ID already mapped to another live entity must be re-keyed on a server or offline world (the higher simulation ID gets a fresh ID next tick) and ignored on a client, each counted in `net.ids.duplicates` with the logged line.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_DuplicateRekeyedOnServer`, `Test_NetIdentity_DuplicateIgnoredOnClient` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Duplicate and unknown IDs"

### Z2-R-NET-016 Unknown network IDs never assert

- **Statement:** An ID named by a datagram or publication that the map does not hold must be dropped and counted, never asserted.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_ApplySpawnsUpdatesDestroys` (unknown destroy case), `Zenith_Fuzz_NetSnapshotFragment` (`Test` and `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Duplicate and unknown IDs"

### Z2-R-NET-017 Identity across save and restore

- **Statement:** NET's `Zenith.Net.Identity` save participant must write the exactly-24-byte version-1 section with the session state and the session-started flag at capture, restore the allocator and rebuild the map, raise a counter or peer index below a restored ID, refuse a section that is not 24 bytes, has a role byte above 2, a state or started byte out of range or inconsistent, or a non-zero reserved byte, and reset when the section is absent, so every entity keeps its ID and the next assignment matches the original run.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_SaveRestoreKeepsIDsAndCounter`, `Test_NetIdentity_RestoreRaisesLowCounter`, `Test_NetIdentity_RestoreWithoutSectionResets`, `Test_NetIdentity_SectionCorruptRefused`, `Zenith_Fuzz_NetIdentitySection` (`Test`, `Sanitizers` gate steps); `Test_NetIdentity_HighestPeerWrittenFromRecordedEvents` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore"

### Z2-R-NET-018 Client proxies excluded from saves

- **Statement:** On a client world the participant must exclude every entity carrying `NET_IDENTITY_FLAG_PROXY` from every capture.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_ClientProxiesExcludedFromSave` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore"

### Z2-R-NET-019 Network IDs reproduce under replay

- **Statement:** A replay must reproduce every world's map contents at every tick with no record of its own: server and offline assignments recomputed, client proxy IDs taken from recorded publications.
- **Covers:** Z2-I-NET-01, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_ReplayReproducesAssignments`, `Test_NetClient_ReplayReproducesProxies` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Identity in replays"

### Z2-R-NET-020 Authority semantics per role

- **Statement:** Each `Zenith_NetworkAuthority` value must behave per role as the table of "Authority values and meanings" states: SERVER captured by the server and applied to client proxies, OWNER_CLIENT treated as SERVER until Stage 7, LOCAL never captured or applied.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAuthority_OwnerClientNote`, `Test_NetAuthority_LocalReplicatedIsError`, `Test_NetServer_CaptureEncodesAuthoritativeState` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Authority values and meanings"

### Z2-R-NET-021 No client-to-server state at Stage 3

- **Statement:** No message kind of wire format version 1 may carry component state from a client to a server; a client may affect server state only through input commands.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_InputLimits` (INPUT carries commands, digest pairs and the base sequence only), `Test_NetWire_ExactBodyLengths` (`Test` gate step); review of the message table
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Authority values and meanings"; `Zenith2/Docs/Design/NET/Loopback.md` → "Message kinds"

### Z2-R-NET-022 Authority declaration rules

- **Statement:** `Zenith_NetReplicationTable::Build` must report the six diagnostics of "Declaration rules" with their severities and must treat data-defined and C++ components identically.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAuthority_NonComponentIsError`, `Test_NetAuthority_UnencodableIsWarning`, `Test_NetAuthority_TooManyIsError`, `Test_NetAuthority_NetTypesNotReplicated`, `Test_NetAuthority_SchemaSameAsCpp` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Declaration rules"

### Z2-R-NET-023 Replication table order and hash

- **Statement:** The replication table must hold the encodable replicated properties sorted by (type ID, property ID) with the codecs of "Replication table construction", and its hash must be `Zenith_Hash64` over the 14-byte entry records in that order.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAuthority_TableOrderAndHash` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Replication table construction"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetReplicationTable header"

### Z2-R-NET-024 Authority validated at start in every configuration

- **Statement:** Module start must build the replication table in every configuration including Shipping and every role, and an error diagnostic must fail start with `MODULE_ERROR_REGISTRATION` and every diagnostic logged.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAuthority_ValidatedInShipping`, `Test_NetAuthority_LocalReplicatedIsError` (`Test` gate step, `win-shipping-test`)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Authority validation at module start"

### Z2-R-NET-025 Owner peer and Net.SetOwner

- **Statement:** `Zenith.Net.Identity` property 3 must change only through the `Net.SetOwner` command, whose playback must apply on server and offline worlds and drop, with its counter and reason, a malformed payload, a foreign, stale or identity-less target, a peer above 65,534 and every request on a client.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSetOwner_PlaybackRules`, `Test_NetBindings_SetOwnerBuffered` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Ownership and the owner peer"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Command kind payloads"

### Z2-R-NET-026 Client proxies written only by NET

- **Statement:** On a client, a proxy's replicated properties must be written only by `Net.ClientApply` commands, so at the start of every apply each value it applied in the previous tick reads back bit-equal to the dequantisation of the applied word, and proxy prefabs must carry no rigid body, character controller, root-motion graph or behaviour.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_AppliedValuesExact`, `Test_NetQuantise_YawTieBandBitEquality`, `Test_NetAuthoring_ProxyPrefabRules` (`Test` gate step); proof assertion C1 in the loopback suite
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Authority on clients"; `Zenith2/Docs/Design/NET.md` → "Cook steps"

### Z2-R-NET-027 Authority census rows

- **Statement:** Every NET component, resource and event type must have its row in `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the diff that registers it, matching its registered authorities.
- **Covers:** Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetModule_RegistrationShape` (`Test` gate step); review of the census diff
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Authority census rows"

### Z2-R-NET-028 Network authority report

- **Statement:** The development console command `net.authority` must print one line per replicated component type with its authority, properties, codecs and wire bytes, then the warnings, then the table hash.
- **Covers:** Z2-I-NET-02, Z2-I-NET-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAuthority_ReportListsTableInOrder` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Network authority report"

### Z2-R-NET-029 NET components

- **Statement:** `Zenith.Net.Identity` (32 bytes, four properties) and `Zenith.Net.ProxyPrefabSet` (256 bytes, one fixed array of 16 prefab handles) must be registered with the property IDs, value types, authorities, flags, defaults and lifecycle hooks of "Zenith_NetComponents header".
- **Covers:** Z2-I-NET-01, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_NetModule_RegistrationShape` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetComponents header"

### Z2-R-NET-030 Tick-rate agreement

- **Statement:** A server must reject a CONNECT_REQUEST whose `sim.tickHz` differs from its own with `NET_REJECT_REASON_IDENTITY` and detail `REPLAY_IDENTITY_FIELD_TICK_RATE`.
- **Covers:** Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetTicks_TickRateMismatchRejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Tick-rate agreement"

### Z2-R-NET-031 Network roles need real-time ticks

- **Statement:** Module start must refuse `sim.ticksPerFrame` other than 0 for the server and client roles and `sim.maxCatchUpTicks` above 8 for a server, and must accept both for offline and replay runs.
- **Covers:** Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** Z2-B-JOBS-002
- **Acceptance evidence:** `Test_NetTicks_ExternalTickModeRefusedForNetworkRoles`, `Test_NetTicks_MaxCatchUpAboveEightRefused` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Tick-rate agreement"; `Zenith2/Docs/Design/NET/Loopback.md` → "Snapshot send"

### Z2-R-NET-032 Tick-indexed messages

- **Statement:** Every simulation-relevant field of a NET message must name a session-world tick index as the table of "Tick-indexed messages" states, and tick indices of two processes must be related only through the client's offset.
- **Covers:** Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_TargetTickOffset`, `Test_NetWire_PacketHeaderRoundTrip` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Tick-indexed messages"

### Z2-R-NET-033 Catch-up and paused frames

- **Statement:** A catch-up frame must run every NET phase system once per tick and send every captured tick and every observed command block; a paused session world must run none and must apply a passed block at the first tick after resuming, counted late.
- **Covers:** Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** Z2-B-JOBS-002
- **Acceptance evidence:** `Test_NetServer_CaptureRingSendsEveryCatchUpTick`, `Test_NetCommands_ClientObservesEveryTick`, `Test_NetInput_LateCommandCounted` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Catch-up and paused frames"

### Z2-R-NET-034 NET simulation reads no clock

- **Statement:** No NET phase system may read a clock, an environment variable or a thread identity; wall-clock time may be read only by the transport in frame-prologue step 1 and in `Net.Send`.
- **Covers:** Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` gate step (rule F14, the clock calls); `CMake_Net_PhaseSystemsReadNoClockOrEnvironment` (`Test` gate step), which fails on any clock, environment or thread-identity call in the four phase-system sources; `Test_NetIdentity_IndependentOfWorkersAndLayout`, `Test_NetServer_CaptureIndependentOfWorkers` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Clock discipline"

### Z2-R-NET-035 Remote commands enter only through phase 1

- **Statement:** A remote peer's input must reach a server world only through PLAT's input command bridge as phase-1 input commands applied to player slot 0 and logged at step 5; a world whose bridge answers `ReplacesLocalInput()` true (the server) must map no local transitions, and a client world, whose bridge answers false, must keep its local input.
- **Covers:** Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetCommands_ServerAppliesRemoteToSlotZero`, `Test_NetCommands_ClientLocalInputStillMapped`, `Test_NetInput_ServerLogHoldsAppliedCommands` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Remote commands enter phase 1"

### Z2-R-NET-036 Input command bridge per role

- **Statement:** NET's bridge must implement `ReplacesLocalInput` (true on a server, false on a client), `ProvideTickCommands` (server: the window's release) and `ObserveTickCommands` (client: the outbound ring), be registered only for a session world, and under `--replay` receive observe calls only.
- **Covers:** Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetCommands_ClientObservesEveryTick`, `Test_NetCommands_ClientLocalInputStillMapped`, `Test_NetCommands_ReplayCallsObserveNotProvide` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Command source contract"

### Z2-R-NET-037 Command wire encoding equals the replay encoding

- **Statement:** A command must cross the network in the 18-byte `ZRPL` command record, byte-identical to SAVE's writer's, and the server must apply exactly the value bits the client emitted.
- **Covers:** Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_CommandRecordMatchesReplayEncoding` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Command wire encoding"

### Z2-R-NET-038 Networked logs replay without a network

- **Statement:** A server log and a client log must each replay offline in their recorded roles, with NET opening no socket, to tick hashes equal to the recording.
- **Covers:** Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ReplayCreatesNoTransport` (`Test` gate step); `Test_Net_LoopbackLogsReplayBitIdentical` (loopback suite, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Command replay"; `Zenith2/Docs/Design/NET/Loopback.md` → "Replay verification"

### Z2-R-NET-039 NET runs unchanged headless

- **Statement:** `ZenithNet` must declare no dependency on ZenithFlux, ZenithRHI or ZenithAudio and must start, attach and tick identically in the headless configuration.
- **Covers:** Z2-I-NET-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ModuleStartsWithoutDevice` (`Test` gate step); `Libgraph` gate step
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "What a headless run contains"

### Z2-R-NET-040 Headless server composition

- **Statement:** A server must be the game's Player started headless with `net.role=server`, binding at module start, attaching the startup world as the session world, and sending DISCONNECT `SERVER_SHUTDOWN` to every peer at shutdown step 6.
- **Covers:** Z2-I-NET-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ServerBindsLoopbackOnly`, `Test_NetSession_ShutdownSendsDisconnect` (`Test` gate step); the loopback suite
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Headless server composition"

### Z2-R-NET-041 Headless client composition

- **Statement:** A client must run the same composition with `net.role=client` and `net.connect`, headless or windowed, and a windowed client must render its proxies like any entity.
- **Covers:** Z2-I-NET-05, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** the loopback suite (headless); `Test_NetClient_WindowedRendersProxies` (`needs-gpu`, `gpu-reason:requires-graphics-test`, Stage 3 acceptance)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Headless client composition"

### Z2-R-NET-042 Headless frame pacing relied on (NT7)

- **Statement:** NET relies on MOD's frame-loop rule of row NT7, which MOD's own requirement carries: a headless real-time process paces at most 1 ms per iteration through `Zenith_SleepUs` while JOBS's time until the next tick is non-zero. If MOD declines NT7, the loopback driver must pass `--set net.proof.assertTiming=false` and the command criterion must rest on the verifier's checks.
- **Covers:** Z2-I-NET-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** MOD's frame-loop pacing test, once row NT7 lands (`Test` gate step); until then, or if NT7 is declined, `Test_NetLoopbackVerify_CommandLogs` with `net.proof.assertTiming=false` on the driver's live lines
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Headless frame pacing"

### Z2-R-NET-043 Stage 3 Shipping guard

- **Statement:** In a Shipping build, module start must force a server or client role offline with the logged line, create no transport and force `net.proof.mode` false, and the transport's entry symbol must be on the Shipping forbidden-symbol list.
- **Covers:** Z2-I-NET-05, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ShippingForcesOffline`, `Test_NetProof_ForcedOffInShipping` (`Test` gate step, `win-shipping-test`); the `Libgraph` Shipping symbol check once row NT9 lands
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles"

### Z2-R-NET-044 Production port over PLAT sockets

- **Statement:** The production port must adapt PLAT's datagram socket with the error mapping of "Production port over PLAT sockets", always loopback-only, refusing any bind address other than `127.0.0.1`.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ServerBindsLoopbackOnly` (`Test` gate step); the loopback suite
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Production port over PLAT sockets"

### Z2-R-NET-045 Memory port pair

- **Statement:** `Zenith_CreateNetMemoryPortPair` must connect two ports through two bounded rings that deliver in send order with the sender's address and drop the newest when full.
- **Covers:** Z2-I-NET-06, Z2-I-NET-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_NetMemoryPort_OrderFullAndLimits`, `Test_NetSession_HandshakeOverMemoryPort` and every session test (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Memory port"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetDatagramPort header"

### Z2-R-NET-046 Pump and send discipline

- **Statement:** The transport must receive at most 512 datagrams in frame-prologue step 1, after checking every session world's restore mark, and send at most 512 in `Net.Send`, the two sites never overlapping, and must do nothing offline or under `--replay`.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_PumpBoundedAt512`, `Test_NetSession_SendBoundedAt512`, `Test_NetIdentity_RestoreClearsApplyTables` (the restore mark), `Test_NetHeadless_ReplayCreatesNoTransport` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Pump and send discipline"

### Z2-R-NET-047 Packet header and integrity

- **Statement:** Every datagram must carry the 20-byte header with magic `ZNP1`, protocol version 1, kind, zero flags, session ID and sequence, followed by its body and a CRC-32C trailer, 24 … 1,200 bytes in total.
- **Covers:** Z2-I-NET-06, Z2-I-NET-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_PacketHeaderRoundTrip`, `Test_NetWire_CrcRejectsCorruption`, `Test_NetWire_SizeLimits` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Packet header"; `Zenith2/Docs/Design/NET/Loopback.md` → "Packet integrity"

### Z2-R-NET-048 Connect messages

- **Statement:** CONNECT_REQUEST (64 bytes), CONNECT_ACCEPT (32) and CONNECT_REJECT (16) must have exactly the fields of their tables, little-endian with zero reserved bytes.
- **Covers:** Z2-I-NET-06, Z2-I-NET-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_ExactBodyLengths`, `Test_NetWire_ReservedBytesMustBeZero`, `Test_NetSession_RejectReasons` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "CONNECT_REQUEST"; `Zenith2/Docs/Design/NET/Loopback.md` → "CONNECT_ACCEPT"; `Zenith2/Docs/Design/NET/Loopback.md` → "CONNECT_REJECT"

### Z2-R-NET-049 INPUT message

- **Statement:** INPUT must carry the cumulative count of reported complete ticks, 0 … 4 ascending (tick, digest) pairs, the session base sequence and 0 … 4 ascending blocks of 18-byte commands, at most 64 commands and 1,176 body bytes, every player slot 0.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_InputLimits` (`Test` gate step); `Zenith_Fuzz_NetInput` (`Sanitizers -Preset win-asan`)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "INPUT"

### Z2-R-NET-050 SNAPSHOT message

- **Statement:** A SNAPSHOT fragment must have the 20-byte header, ascending ULEB128 destroys and ascending entity records with flags, proxy index, owner, presence mask and codec values, at most 1,020 bytes.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_SnapshotLimits` (`Test` gate step); `Zenith_Fuzz_NetSnapshotFragment` (`Sanitizers -Preset win-asan`)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "SNAPSHOT"

### Z2-R-NET-051 KEEPALIVE and DISCONNECT

- **Statement:** KEEPALIVE must carry the sender's last completed tick and DISCONNECT a disconnect reason and seven zero bytes, each exactly 8 body bytes.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_ExactBodyLengths`, `Test_NetSession_KeepaliveKeepsIdleSessionAlive` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "KEEPALIVE"; `Zenith2/Docs/Design/NET/Loopback.md` → "DISCONNECT"

### Z2-R-NET-052 Canonical ULEB128

- **Statement:** ULEB128 values must be written least significant group first and read with at most 10 bytes, refusing values above 2^64 − 1 and over-long encodings.
- **Covers:** Z2-I-NET-06, Z2-I-NET-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_ULEB128RoundTripAndCanonical` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Unsigned LEB128"

### Z2-R-NET-053 Wire limits tested at the limit

- **Statement:** Every limit of "Wire limits" must be a constant of `Zenith_NetTypes.h` tested at the limit and at limit + 1.
- **Covers:** Z2-I-NET-06, Z2-I-NET-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_SizeLimits`, `Test_NetWire_InputLimits` (64 and 65 commands), `Test_NetWire_SnapshotLimits`, `Test_NetSession_PumpBoundedAt512` and `Test_NetSession_SendBoundedAt512` (512 and 513 datagrams), `Test_NetProxyTable_BuildRules` (16 distinct prefabs; a 17th cannot be authored because the component is a 16-element fixed array, which SCENE's schema refuses), `Test_NetAuthority_TooManyIsError` (32 and 33 entries), `Test_NetPublication_PayloadLayout` (1,024 and 1,025 bytes), `Test_NetIdentity_SectionCorruptRefused` (24 and 25 bytes), `Test_NetInput_BurstPoolFullDropsNewest` (4,096 and 4,097 burst commands) (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits"

### Z2-R-NET-054 Packet validation never asserts

- **Statement:** Received datagrams must be validated in the order of "Packet validation order", each failure dropped and counted, at most one log line per kind per second, never an assert or out-of-bounds read.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_SlidingWindowDropsDuplicates`, `Test_NetSession_ForeignAndOldPacketsDropped`, `Test_NetDiagnostics_LogRateLimited` (`Test` gate step); `Zenith_Fuzz_NetPacket` (`Sanitizers -Preset win-asan`; full duration in Stage 3 acceptance)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order"

### Z2-R-NET-055 Malformed burst disconnects the peer

- **Statement:** A peer sending 8 malformed packets within one second must be disconnected with `NET_DISCONNECT_REASON_PROTOCOL_ERROR`.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWire_MalformedBurstDisconnects` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order"

### Z2-R-NET-056 Client session state machine

- **Statement:** The client transport must follow the state × event table of "Client session states" exactly, publishing from the prologue each transition the table marks, never reconnecting at Stage 3, and dropping SNAPSHOT, KEEPALIVE and INPUT received before it is connected.
- **Covers:** Z2-I-NET-06, Z2-I-NET-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_HandshakeOverMemoryPort`, `Test_NetSession_LostAcceptRetried`, `Test_NetSession_ConnectTimeout`, `Test_NetSession_ProxyNotResidentFails`, `Test_NetSession_PreConnectedTransportStatesDropTraffic` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client session states"

### Z2-R-NET-057 Server peer state machine

- **Statement:** Each server peer slot must follow the table of "Server peer states", including the 1,000 ms linger that drops the old session's datagrams, and the server's session state must be `CONNECTED` while a joined peer has not left and `LISTENING` otherwise.
- **Covers:** Z2-I-NET-06, Z2-I-NET-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_LingerDropsOldSession`, `Test_NetSession_PeerTimeoutBothSides`, `Test_NetSession_StateIsFunctionOfRecordedEvents` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Server peer states"

### Z2-R-NET-058 Handshake rules

- **Statement:** The server must check a request in the order of "Handshake rules", reject at the first difference with its reason and detail (`NOT_ACCEPTING` only when no session world is attached), drop and count a request that arrives before its session world's first tick, and answer a repeated request with an identical ACCEPT.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_RejectReasons`, `Test_NetSession_LostAcceptRetried`, `Test_NetSession_JoinAtFirstPrologue` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Handshake rules"

### Z2-R-NET-059 Timeouts and keepalive

- **Statement:** Retries, connect timeout, keepalive, peer timeout and linger must use the values of "Timeouts and keepalive" and read the clock only outside simulation.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_ConnectTimeout`, `Test_NetSession_PeerTimeoutBothSides`, `Test_NetSession_KeepaliveKeepsIdleSessionAlive` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Timeouts and keepalive"

### Z2-R-NET-060 Session events are recorded publications

- **Statement:** Every session transition a world sees must arrive as an `ASYNC_SOURCE_NETWORK` publication of the 28-byte session event, allocated from the session-event reserve of 8 in-flight slots and parked in the frame prologue, and must become `Net.SessionChanged`, `Net.PeerJoined` or `Net.PeerLeft` in phase 1.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPublication_PayloadLayout`, `Test_NetEvents_SessionEventsDelivered`, `Test_NetClient_SessionEventSurvivesFullFragmentBacklog` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication"; `Zenith2/Docs/Design/NET/Loopback.md` → "Client session states"

### Z2-R-NET-061 Client command capture

- **Statement:** While the transport has adopted an ACCEPT (`m_bAdopted`, set in the adopting prologue) and the run is live, the client bridge must append each tick's commands as one outbound block, tick c0's commands first, append nothing before adoption or under `--replay`, keep commands above a block's 64 in the shared burst pool (dropping the newest when full), and fix the session base sequence as the sequence after the last command it observed before adoption (0 when none).
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetCommands_ClientObservesEveryTick`, `Test_NetInput_BurstPoolFullDropsNewest` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client command capture"

### Z2-R-NET-062 Target tick mapping

- **Statement:** On adopting an ACCEPT the client must compute the offset o = accept tick + input delay − its next tick, publish it in the CONNECTED event, and target each subsequent block at client tick + o.
- **Covers:** Z2-I-NET-06, Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_TargetTickOffset` (`Test` gate step); `Test_NetLoopbackVerify_CommandLogs`
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Target tick mapping"

### Z2-R-NET-063 Client INPUT emission

- **Statement:** Each new block must be sent in an INPUT with up to `net.redundantInputTicks` previous blocks, splitting a block above 61 commands, carrying the session base sequence and the digest ring, an INPUT with no block must report a completed tick when no block is due, and a KEEPALIVE must replace an INPUT when nothing was sent for the keepalive interval.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_RedundantBlocks`, `Test_NetDigests_EveryCompleteTickReported`, `Test_NetSession_KeepaliveKeepsIdleSessionAlive` (`Test` gate step). This requirement deliberately keeps the four emission rules together, because they are one encoder whose output a single test observes.
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client INPUT emission"

### Z2-R-NET-064 Server command intake

- **Statement:** The server must set the window's next expected sequence from the session's first INPUT, drop sequences below it as duplicates, commands more than 128 ticks early and packets with a non-zero player slot, and insert every other command into its window by sequence.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_DuplicatesAndTooEarlyDropped`, `Test_NetInput_FirstSequenceSetsBaseline`, `Test_NetWire_InputLimits` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Server command intake"

### Z2-R-NET-065 Server command window releases in sequence

- **Statement:** The window must release commands strictly in sequence order at or after their target tick, at most the output span's size per tick, count late releases in `net.late_commands`, hold a gap until `net.server.commandHoldTicks` after the waiting command's target (in signed tick arithmetic, so an early command behind a gap waits) and then count `net.server.lost_commands`.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_WindowReleasesInSequence`, `Test_NetInput_LateCommandCounted`, `Test_NetInput_GapHeldThenLost`, `Test_NetInput_EarlyCommandBehindGapWaits`, `Test_NetInput_ReleaseBoundedByOutput` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Server command window"

### Z2-R-NET-066 Command delivery to phase 1

- **Statement:** `ProvideTickCommands` must return exactly the window's release for the tick, and the server log must hold exactly the commands each tick applied.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_ServerLogHoldsAppliedCommands` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Command delivery to phase 1"

### Z2-R-NET-067 Capture at the snapshot point

- **Statement:** `Net.Capture` must run in phase 11 of the server's session world every snapshot interval, capturing replicated, non-proxy, non-zero-ID entities in ascending ID from the capture cursor, at most 512, with at most 256 pending destroys, into the 8-slot capture ring.
- **Covers:** Z2-I-NET-06, Z2-I-NET-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServer_CaptureEncodesAuthoritativeState`, `Test_NetServer_CaptureSkipsProxiesAndUnflagged`, `Test_NetFragments_TruncationCursorCoversTail`, `Test_NetFragments_DestroyListRepeated` (`Test` gate step). This requirement deliberately keeps the capture's selection rules together, because they define one deterministic list whose order every test observes.
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point"

### Z2-R-NET-068 Capture independent of workers and layout

- **Statement:** The capture's bytes must be identical for 1, 2, 8 and all workers, forced compaction and two chunk capacities on every ECS backend.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServer_CaptureIndependentOfWorkers` (`Test` gate step, every backend)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point"; `Zenith2/Docs/Design/NET.md` → "Determinism obligations"

### Z2-R-NET-069 Proxy prefab table

- **Statement:** The proxy table must be built by the first run of `Net.Session` after the session world's attach from the one `Zenith.Net.ProxyPrefabSet` by component data alone, with indices in array order and a hash over the prefabs' persistent IDs, invalid for zero or two set entities or a repeated prefab; the proxies' residency must be checked by the client transport in the prologue, a non-ACTIVE entry publishing FAILED (`PROXY_NOT_RESIDENT`).
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetProxyTable_BuildRules`, `Test_NetSession_ProxyNotResidentFails` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Proxy prefab table"

### Z2-R-NET-070 Position quantisation

- **Statement:** `NET_CODEC_POSITION_MM` must round each component of v × 1000 to nearest with ties to even into an integer clamped to ±8,388,607 (±8,388.607 m), map non-finite values to 0 with a counter, dequantise as `float(q) / 1000.0f`, and stay within 1.24 mm of the input over the range.
- **Covers:** Z2-I-NET-06, Z2-I-NET-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetQuantise_PositionGoldenVectors`, `Test_NetQuantise_PropertyRoundTrip` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Quantisation"

### Z2-R-NET-071 Quaternion quantisation

- **Statement:** `NET_CODEC_QUAT_SMALLEST_THREE` must normalise, choose the largest component by lowest index on ties, negate for a negative largest, and pack three 10-bit components rounded to nearest with ties to even with the 2-bit index, using only correctly rounded operations so both peers agree bit for bit, within 0.0048 rad of the input.
- **Covers:** Z2-I-NET-06, Z2-I-NET-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetQuantise_QuatGoldenVectors`, `Test_NetQuantise_PropertyRoundTrip`, `Test_NetQuantise_YawTieBandBitEquality` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Quantisation"

### Z2-R-NET-072 Deterministic fragment encoding

- **Statement:** Fragments must be encoded by the greedy algorithm of "Fragment encoding", destroys and records counted in one budget of at most 32 fragments per tick, records never spanning fragments, the bytes a pure function of the captured state and the cursor.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetFragments_EncodeDeterministic`, `Test_NetFragments_DestroyFloodStaysWithin32Fragments` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment encoding"

### Z2-R-NET-073 Snapshot send

- **Statement:** Every capture of a frame must be sent in `Net.Send` in ascending tick and fragment order, one SNAPSHOT datagram per fragment.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServer_CaptureRingSendsEveryCatchUpTick` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Snapshot send"

### Z2-R-NET-074 Client fragment intake

- **Statement:** For each valid fragment no more than 63 ticks older than the newest received, the client must update the tick's receive bitmap, allocate one `ASYNC_SOURCE_NETWORK` completion ID and park the fragment, record the tick's digest when it completes, drop the fragment when the sink is full, and allocate nothing under `--replay`.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_IntakePublishesInArrivalOrder`, `Test_NetClient_SinkFullDropsFragment`, `Test_NetClient_FragmentTooOldDropped` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client fragment intake"

### Z2-R-NET-075 NET asynchronous source and payload

- **Statement:** NET must declare `ASYNC_SOURCE_NETWORK` per session world as a prologue-site, recorded-only source of at most 1,024 bytes with the in-flight capacities of "Fragment publication", park every allocated ID exactly once, and publish only the two payload record types of that section.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPublication_PayloadLayout`, `Test_NetClient_IntakePublishesInArrivalOrder` (`Test` gate step); `Zenith_Fuzz_NetPublication` (`Sanitizers -Preset win-asan`)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication"; `Zenith2/Docs/Design/NET.md` → "Asynchronous source and misuse block"

### Z2-R-NET-076 Client apply

- **Statement:** `Net.ClientApply` must apply the tick's published fragments in inbox order as destroy, prefab-instantiation and `SetProperty` commands with the identity of a proxy, so every proxy shows the newest received server state from phase 2.
- **Covers:** Z2-I-NET-06, Z2-I-NET-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_ApplySpawnsUpdatesDestroys`, `Test_NetClient_AppliedValuesExact`, `Test_NetClient_ReplayReproducesProxies` (`Test` gate step, every backend)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply"

### Z2-R-NET-077 Stale fragments and pending spawns

- **Statement:** Client apply must skip a record older than the entity's last applied tick or than a tombstone of its ID, must spawn a new ID at most once, applying further records of the same tick boundary to its ticket, and must spawn nothing while the world's map is full.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_StaleFragmentSkipped`, `Test_NetClient_PendingSpawnNotDuplicated`, `Test_NetClient_TombstoneBlocksResurrection`, `Test_NetClient_MapFullSpawnsNothing` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply"

### Z2-R-NET-078 Controlled entity event

- **Statement:** Client apply must emit `Net.ControlledEntityChanged` exactly when the proxy owned by the local peer changes, with the previous entity.
- **Covers:** Z2-I-NET-06, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_ControlledEntityEvent` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply"

### Z2-R-NET-079 Acknowledgements and digests

- **Statement:** The client must report every complete tick it applied, with the CRC-32C digest of its fragment bodies, through the 4-pair digest ring (kept in completion order, emitted sorted by tick) and the cumulative count of every INPUT, and the server must verify each reported tick once within its 64-capture ring, counting verified, mismatched and unverifiable ticks, changing no simulation state.
- **Covers:** Z2-I-NET-06, Z2-I-NET-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAck_DigestVerifiedAndMismatch`, `Test_NetDigests_EveryCompleteTickReported` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Acknowledgements and digests"

### Z2-R-NET-080 Proof assertions in-process

- **Statement:** With `net.proof.mode`, the server must check P1–P6 and the client C1–C4 of "Proof assertions" and report each failure through the test assertion sink with its reason line, so that every tick the client held completely is shown equal to the quantised authoritative state of that tick.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetProof_AssertionsReportThroughSink`, `Test_NetServer_CaptureSelfCheckP6` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Proof assertions"

### Z2-R-NET-081 Proof outcome through exit codes

- **Statement:** A proof failure must make its process exit with code 5 and print the `net.proof` records, and a run in which every assertion held must exit 0.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetProof_AssertionsReportThroughSink` (the negative case: one failing assertion recorded from the module instance's destructor gives exit code 5); the loopback suite's exit-code checks (`Test_Net_LoopbackReplicatesTransformsAndCommands`) for the positive case
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Proof outcome"

### Z2-R-NET-082 Loopback scene and prefabs

- **Statement:** `Games/WorkloadCharacters` must gain `Scenes/NetLoopback.scene.json`, the proxy prefab `CharacterProxy` and the identity on the character prefab, exactly as "Scene and prefabs" lists, leaving the W-Characters workload scene unchanged.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `net.validate` passing in the cook of WorkloadCharacters (the `Build` gate step); the loopback suite
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Scene and prefabs"

### Z2-R-NET-083 Loopback scripts

- **Statement:** `Scripts/Net/LoopbackDirector.luau` must spawn the 64-character grid, move 63 of them deterministically, give character 0 to the joining peer and possess it on both sides, and run offline without a network.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** the loopback suite; `Game_WorkloadCharacters_Boot` with the loopback scene offline (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Loopback scripts"

### Z2-R-NET-084 Loopback input script

- **Statement:** `Tests/NetLoopback.input.json` must drive `Gameplay.Move` and `Gameplay.Jump` at the ticks of "Input script", exclusive, over 600 ticks.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** the loopback suite (the client's recorded commands)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Input script"

### Z2-R-NET-085 Loopback suite target and registration

- **Statement:** `Zenith2/Tests/NetLoopback/` must be one `zenith_add_integration_tests` call, and the test registry must register the two loopback CTest entries on the `win` test presets with label `z2-net-loopback`, each `RUN_SERIAL TRUE`, with its own output directory `…/<config>/<Case>`, and requiring the cook fixture of `WorkloadCharacters`.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `SourceLayout` gate step; the two CTest entries discovered by the `Test` gate step
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Suite target"

### Z2-R-NET-086 Loopback driver

- **Statement:** `Run-NetLoopback.ps1` must start the server and client with the command lines of "Driver script" (`--assets-root` and `devtools.log.file=true` on every launch), wait at most 150 s, copy each process's new `user:/devtools/logs/` files into its case directory as the process exits, fail on any non-zero exit code with that process's captured output and its `net.proof`, `net.session` and `runtime.diagnostics` records from the copies, require exactly one capture-chain line per server process, and run the case's verification.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Net_LoopbackReplicatesTransformsAndCommands`, `Test_Net_LoopbackLogsReplayBitIdentical` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Driver script"

### Z2-R-NET-087 Command log verification

- **Statement:** The verifier must find the client's CONNECTED event and offset, map every client command from that tick on to its target tick, and assert that the server log holds exactly the reachable client commands in sequence order, starting at the client's first sent sequence, at their target ticks, over at least 300 server ticks.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetLoopbackVerify_CommandLogs`, `Test_NetLoopbackVerify_FailsWithoutConnection`, `Test_NetLoopbackVerify_DetectsMissingCommand` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Command log verification"

### Z2-R-NET-088 Loopback replicates transforms and commands

- **Statement:** The two-process loopback of `Games/WorkloadCharacters` must replicate transforms and commands: both processes exit 0 with every proof assertion held and the command logs verified.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Net_LoopbackReplicatesTransformsAndCommands` (`Test` gate step; Stage 3 acceptance)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "The loopback suite"; `Zenith2/Docs/Design/NET/Loopback.md` → "Command log verification"

### Z2-R-NET-089 Loopback logs replay bit-identically

- **Statement:** The server and client logs of a loopback run must each replay offline to exit 0 with no tick-hash mismatch, and the server replay's capture chain must equal the live server's.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Net_LoopbackLogsReplayBitIdentical` (`Test` gate step; Stage 3 acceptance)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Replay verification"

### Z2-R-NET-090 Net script bindings

- **Statement:** The `Net` namespace must expose the nine functions of "Script-facing API" with their contexts, effects and return values, every read binding naming its resource and component.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_NetBindings_ReadFunctions`, `Test_NetBindings_SetOwnerBuffered` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Script-facing API"

### Z2-R-NET-091 Net misuse codes

- **Statement:** NET's bindings must raise ZS3401–ZS3404 for the triggers of "Misuse codes" with the catalogued text, never an assert, from the block ZS3400–ZS3499.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_NetBindings_MisuseCodes` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Misuse codes"; `Zenith2/Docs/Design/NET.md` → "Asynchronous source and misuse block"

### Z2-R-NET-092 Net event types

- **Statement:** The four world-level event types must be registered with the fields of "Event types" and delivered in phase 9 of the tick their phase-1 emitter ran.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_NetEvents_SessionEventsDelivered`, `Test_NetClient_ControlledEntityEvent` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Event types"

### Z2-R-NET-093 Net configuration keys

- **Statement:** The nineteen Stage 3 keys must be registered with the types, ranges, defaults and flags of "Configuration keys", `net.role` and `net.maxNetworkedEntities` simulation-affecting.
- **Covers:** Z2-I-NET-05, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetModule_RegistrationShape` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/PublicAPI.md` → "Configuration keys"

### Z2-R-NET-094 Net error enumeration

- **Statement:** `Zenith_NetErrorCode` must hold the codes of "Error enumerations" with the stated meanings, and `Zenith_NetError` must be the 8-byte record with no text.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` gate step; the error cases of the wire, session and map tests (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Error enumerations"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetErrors header"

### Z2-R-NET-095 No threads and no locks at Stage 3

- **Statement:** NET must start no thread and hold no mutex at Stage 3, and every cross-thread hand-off must be ordered by the frame structure as "Cross-thread hand-offs" states.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `CMake_Net_NoThreadOrMutexInSources` (`Test` gate step), which fails on any thread or mutex construction in ZenithNet's sources; `ForbiddenCheck` gate step for the STL forms
- **Design:** `Zenith2/Docs/Design/NET.md` → "Threads NET owns"; `Zenith2/Docs/Design/NET.md` → "Lock-order entries"; `Zenith2/Docs/Design/NET.md` → "Cross-thread hand-offs"

### Z2-R-NET-096 Startup and shutdown order

- **Statement:** NET must start at steps 15 and 16 and, at shutdown step 6, send DISCONNECT three times, close its port and evaluate the proof outcome before its instance is freed.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_ShutdownSendsDisconnect`, `Test_NetProof_AssertionsReportThroughSink` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Startup and shutdown"

### Z2-R-NET-097 NET determinism obligations

- **Statement:** Every NET phase system's result must depend only on simulation state, recorded publications and the tick's commands, and NET's only paths into a world must be input commands and recorded publications.
- **Covers:** Z2-I-NET-04, Z2-I-NET-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_IndependentOfWorkersAndLayout`, `Test_NetServer_CaptureIndependentOfWorkers`, `Test_NetClient_ReplayReproducesProxies` (`Test` gate step); `Test_Net_LoopbackLogsReplayBitIdentical`
- **Design:** `Zenith2/Docs/Design/NET.md` → "Determinism obligations"

### Z2-R-NET-098 Net counters

- **Statement:** Every counter of "Counters" must be registered with its kind, and NET's queues must publish their `.full`, `.dropped` and `.high_water` counters.
- **Covers:** Z2-I-NET-06, Z2-I-NET-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_CountersRegistered` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Counters"

### Z2-R-NET-099 Net zones and log categories

- **Statement:** NET must emit the zones of "Profiling zones" and log under the four categories of "Log categories and messages" with those message forms and no personal data.
- **Covers:** Z2-I-NET-06, Z2-I-NET-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_StatusCommandFormat`, `Test_NetDiagnostics_LogRateLimited` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Profiling zones"; `Zenith2/Docs/Design/NET.md` → "Log categories and messages"

### Z2-R-NET-100 Net console, rows and frame dump

- **Statement:** Development builds must register `net.status`, `net.authority` and `net.ids`, the `Net` overlay rows and the `net` frame-dump section, all absent from Shipping.
- **Covers:** Z2-I-NET-06, Z2-I-NET-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_StatusCommandFormat`, `Test_NetDiagnostics_FrameDumpSection` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Console commands"; `Zenith2/Docs/Design/NET.md` → "On-screen profiler rows"; `Zenith2/Docs/Design/NET.md` → "Frame dump section"

### Z2-R-NET-101 Net document validation

- **Statement:** `Zenith_ValidateNetDocument` must report rules N1–N5 of "Cook steps" with their severities.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_NetAuthoring_RejectsAuthoredNetworkID`, `Test_NetAuthoring_ProxyPrefabRules` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Cook steps"; `Zenith2/Docs/Design/NET.md` → "What NET authors"

### Z2-R-NET-102 Net cook step

- **Statement:** ZenithCook must register the step `net.validate`, run it on every cooked scene and prefab, and fail the cook on an error diagnostic.
- **Covers:** Z2-I-NET-01, Z2-I-NET-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetCook_StepRegisteredAndRuns` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Cook steps"

### Z2-R-NET-103 Net editor surfaces

- **Statement:** The editor must show both NET components in the reflected inspector with NetworkID hidden, and play-in-editor worlds must run offline at Stage 3.
- **Covers:** Z2-I-NET-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetModule_RegistrationShape` (the editor-hidden flag), `Test_NetHeadless_ModuleStartsWithoutDevice` (offline attach of a non-runtime world) (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Editor surfaces"

### Z2-R-NET-104 Net failure behaviours

- **Statement:** Each failure of "Failure table" must produce exactly its stated behaviour, recoverable failures never ending the run, each row asserted by the test the table names.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the tests of the Failure table's Test column (`Test` gate step); the budget row by its per-system budget ticket's run
- **Design:** `Zenith2/Docs/Design/NET.md` → "Failure table"

### Z2-R-NET-105 Stage 3 NET budget rows allocated

- **Statement:** The `needs-human` Stage 3 per-system budget ticket for NET must allocate the pending rows N1–N5 from measured runs and commit them to `Zenith2/Docs/Budgets.md`.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-012, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the ruling's Budgets.md rows and DecisionLog entry, checked by the `Coverage` gate step in acceptance mode `-Stage 3`
- **Design:** `Zenith2/Docs/Design/NET.md` → "Pending row needs"

### Z2-R-NET-106 Local multiplayer excluded

- **Statement:** Zenith2 must implement no local multiplayer: one player slot per process outside NET's Stage 7 remote slots.
- **Covers:** Z2-I-NET-15
- **Stage:** S1
- **Classification:** excluded
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** none (excluded; rationale Z2-D-076)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Explicit exclusions"

### Z2-R-NET-107 Split-screen excluded

- **Statement:** Zenith2 must implement no split-screen: render views are never per player.
- **Covers:** Z2-I-NET-16
- **Stage:** S1
- **Classification:** excluded
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** none (excluded; rationale Z2-D-076)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Explicit exclusions"

### Z2-R-NET-159 Session world capture fidelity

- **Statement:** The `Zenith.Net.Identity` participant's `GetCaptureFidelity` must return COHERENT for a server or client world once its session has started and EXACT for an offline world and for a session world before its session starts.
- **Covers:** Z2-I-NET-01, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_SessionWorldCaptureIsCoherent` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore"

### Z2-R-NET-160 Restore ends a live session and keeps a pre-session one

- **Statement:** When the captured session had started, `RestoreSection` must clear the session world's apply tables, pending spawns, tombstones, command window, next expected sequence and joined peers, set a client's session state to DISCONNECTED (`RESTORED`) and a server's to LISTENING, clear `m_bSessionStarted` and move the restore mark, so the next prologue ends the transport's live session; when it had not started, it must restore the captured session state as it is without moving the mark; neither case publishes an event.
- **Covers:** Z2-I-NET-01, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_NetIdentity_RestoreClearsApplyTables`, `Test_NetIdentity_RestoreOfPreSessionClientKeepsConnecting` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore"; `Zenith2/Docs/Design/NET/Loopback.md` → "Client session states"

### Z2-R-NET-161 Session state is a function of recorded events

- **Statement:** `Zenith_NetWorldState::m_eSessionState` must be written only by `Net.Session` from the recorded session events and by `RestoreSection`, with the initial value of its role at attach, so `Net.SessionState` reads the same value at every tick live and under `--replay`.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_StateIsFunctionOfRecordedEvents` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client session states"; `Zenith2/Docs/Design/NET/Loopback.md` → "Server peer states"; `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetTypes header"

### Z2-R-NET-162 No transport or live asset read inside the frame's phases

- **Statement:** No NET phase system may open a port, send or receive a datagram, allocate a completion ID or read live asset state; these happen only in frame-prologue step 1 and `Net.Send`, and reach a world only as recorded publications.
- **Covers:** Z2-I-NET-06, Z2-I-NET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_ProxyNotResidentFails` (no phase-1 system read the asset registry), `Test_NetSession_StateIsFunctionOfRecordedEvents`, `Test_NetClient_IntakePublishesInArrivalOrder` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client session states"; `Zenith2/Docs/Design/NET/Loopback.md` → "Pump and send discipline"

### Z2-R-NET-163 Command window baseline

- **Statement:** The server must set the next expected sequence to the session base sequence of the first INPUT and must count none of the client's pre-connection commands as lost or late; its loss accounting relies on PLAT's per-slot sequence being gapless (row NT3, pending PLAT).
- **Covers:** Z2-I-NET-04, Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInput_FirstSequenceSetsBaseline` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Server command intake"; `Zenith2/Docs/Design/NET/Loopback.md` → "Client command capture"

### Z2-R-NET-164 A client log replays without a connect address

- **Statement:** Under `--replay`, `Zenith_Module_Net_Start` must skip the `net.connect` check, the `sim.ticksPerFrame` and `sim.maxCatchUpTicks` refusals and the transport, so a client log replays with no `net.connect`.
- **Covers:** Z2-I-NET-04, Z2-I-NET-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ReplayOfClientLogStartsWithoutConnect`, `Test_NetHeadless_ReplayCreatesNoTransport` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET.md` → "Module start"

### Z2-R-NET-165 Destroys bounded per capture

- **Statement:** A capture must carry at most 256 pending destroys, oldest first, each destroy carried by exactly 30 captures from its first, so no capture exceeds 32 fragments however many entities are destroyed.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetFragments_DestroyFloodStaysWithin32Fragments`, `Test_NetFragments_DestroyListRepeated` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment encoding"; `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID map"

### Z2-R-NET-166 Truncated captures resume at the first entity left out

- **Statement:** A capture stopped by the 512-entity cap or the fragment budget must set the capture cursor to the first entity it left out, so every entity is captured within ⌈count / k⌉ captures.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetFragments_TruncationCursorCoversTail` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point"

### Z2-R-NET-167 Every complete tick verified

- **Statement:** In proof mode the server's distinct verified ticks must equal the cumulative count of the last INPUT received, with no unverifiable tick, and the client must leave no complete tick unreported.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDigests_EveryCompleteTickReported`, `Test_NetProof_AssertionsReportThroughSink` (P3, C4) (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Acknowledgements and digests"; `Zenith2/Docs/Design/NET/Loopback.md` → "Proof assertions"

### Z2-R-NET-168 Capture self-check and capture chain

- **Statement:** In proof mode `Net.Capture` must decode its own fragments every capture and compare them with the quantisation of the authoritative properties of that tick (P6), and the server must log one capture-chain line at destruction.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServer_CaptureSelfCheckP6` (`Test` gate step); `Test_Net_LoopbackLogsReplayBitIdentical` (the chain comparison)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point"; `Zenith2/Docs/Design/NET/Loopback.md` → "Replay verification"

### Z2-R-NET-169 Destroyed proxies are never resurrected

- **Statement:** A client must keep a tombstone of every destroyed ID for 64 ticks of fragment time with capacity for every destroy 64 captures can carry, and drop fragments 64 or more ticks older than the newest received.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_TombstoneBlocksResurrection`, `Test_NetClient_FragmentTooOldDropped` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply"; `Zenith2/Docs/Design/NET/Loopback.md` → "Client fragment intake"

### Z2-R-NET-170 A full client map spawns no proxies

- **Statement:** When the client world's map is at capacity, `Net.ClientApply` must spawn no proxy and count `net.client.spawns_refused_map_full`, so no proxy is spawned twice.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClient_MapFullSpawnsNothing` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply"

### Z2-R-NET-171 Joins before the server's first tick succeed

- **Statement:** A request reaching the server before its session world's first run of `Net.Session` after attach must be dropped and counted as `net.packets.early`, never rejected, so a client started with its server joins at its next retry, and the loopback director must defer a join it receives before its spawn has run.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_JoinAtFirstPrologue` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Handshake rules"; `Zenith2/Docs/Design/NET/Loopback.md` → "Loopback scripts"

### Z2-R-NET-172 Pump bounded per frame

- **Statement:** The transport must dispatch at most 512 received datagrams per frame prologue, leaving the rest queued for the next.
- **Covers:** Z2-I-NET-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_PumpBoundedAt512` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Loopback.md` → "Pump and send discipline"; `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits"

## Coverage of inventory items

Every item maps to at least one requirement of this file or of `Zenith2/Docs/Requirements/NET/Stage7.md`:

| Item | Requirements |
|---|---|
| Z2-I-NET-01 | Z2-R-NET-001, Z2-R-NET-002, Z2-R-NET-003, Z2-R-NET-004, Z2-R-NET-005, Z2-R-NET-006, Z2-R-NET-007, Z2-R-NET-008, Z2-R-NET-009, Z2-R-NET-010, Z2-R-NET-011, Z2-R-NET-012, Z2-R-NET-013, Z2-R-NET-014, Z2-R-NET-015, Z2-R-NET-016, Z2-R-NET-017, Z2-R-NET-018, Z2-R-NET-019, Z2-R-NET-029, Z2-R-NET-090, Z2-R-NET-091, Z2-R-NET-101, Z2-R-NET-102, Z2-R-NET-103, Z2-R-NET-159, Z2-R-NET-160 |
| Z2-I-NET-02 | Z2-R-NET-001, Z2-R-NET-002, Z2-R-NET-005, Z2-R-NET-018, Z2-R-NET-020, Z2-R-NET-021, Z2-R-NET-022, Z2-R-NET-023, Z2-R-NET-024, Z2-R-NET-025, Z2-R-NET-026, Z2-R-NET-027, Z2-R-NET-028, Z2-R-NET-029, Z2-R-NET-078, Z2-R-NET-090, Z2-R-NET-091, Z2-R-NET-101, Z2-R-NET-102, Z2-R-NET-126 |
| Z2-I-NET-03 | Z2-R-NET-001, Z2-R-NET-002, Z2-R-NET-030, Z2-R-NET-031, Z2-R-NET-032, Z2-R-NET-033, Z2-R-NET-034, Z2-R-NET-062, Z2-R-NET-097, Z2-R-NET-117 |
| Z2-I-NET-04 | Z2-R-NET-001, Z2-R-NET-002, Z2-R-NET-019, Z2-R-NET-035, Z2-R-NET-036, Z2-R-NET-037, Z2-R-NET-038, Z2-R-NET-060, Z2-R-NET-061, Z2-R-NET-065, Z2-R-NET-066, Z2-R-NET-068, Z2-R-NET-075, Z2-R-NET-087, Z2-R-NET-089, Z2-R-NET-097, Z2-R-NET-128, Z2-R-NET-159, Z2-R-NET-161, Z2-R-NET-162, Z2-R-NET-163, Z2-R-NET-164 |
| Z2-I-NET-05 | Z2-R-NET-001, Z2-R-NET-002, Z2-R-NET-039, Z2-R-NET-040, Z2-R-NET-041, Z2-R-NET-042, Z2-R-NET-043, Z2-R-NET-093, Z2-R-NET-149, Z2-R-NET-164 |
| Z2-I-NET-06 | Z2-R-NET-002, Z2-R-NET-006, Z2-R-NET-041, Z2-R-NET-043, Z2-R-NET-044, Z2-R-NET-045, Z2-R-NET-046, Z2-R-NET-047, Z2-R-NET-048, Z2-R-NET-049, Z2-R-NET-050, Z2-R-NET-051, Z2-R-NET-052, Z2-R-NET-053, Z2-R-NET-054, Z2-R-NET-055, Z2-R-NET-056, Z2-R-NET-057, Z2-R-NET-058, Z2-R-NET-059, Z2-R-NET-060, Z2-R-NET-061, Z2-R-NET-062, Z2-R-NET-063, Z2-R-NET-064, Z2-R-NET-065, Z2-R-NET-066, Z2-R-NET-067, Z2-R-NET-068, Z2-R-NET-069, Z2-R-NET-070, Z2-R-NET-071, Z2-R-NET-072, Z2-R-NET-073, Z2-R-NET-074, Z2-R-NET-075, Z2-R-NET-076, Z2-R-NET-077, Z2-R-NET-078, Z2-R-NET-079, Z2-R-NET-080, Z2-R-NET-081, Z2-R-NET-082, Z2-R-NET-083, Z2-R-NET-084, Z2-R-NET-085, Z2-R-NET-086, Z2-R-NET-087, Z2-R-NET-088, Z2-R-NET-089, Z2-R-NET-090, Z2-R-NET-092, Z2-R-NET-093, Z2-R-NET-094, Z2-R-NET-095, Z2-R-NET-096, Z2-R-NET-098, Z2-R-NET-099, Z2-R-NET-100, Z2-R-NET-104, Z2-R-NET-105, Z2-R-NET-160, Z2-R-NET-161, Z2-R-NET-162, Z2-R-NET-163, Z2-R-NET-165, Z2-R-NET-166, Z2-R-NET-167, Z2-R-NET-168, Z2-R-NET-169, Z2-R-NET-170, Z2-R-NET-171, Z2-R-NET-172 |
| Z2-I-NET-07 | Z2-R-NET-048, Z2-R-NET-056, Z2-R-NET-057, Z2-R-NET-108, Z2-R-NET-109, Z2-R-NET-110, Z2-R-NET-111, Z2-R-NET-112, Z2-R-NET-113, Z2-R-NET-114, Z2-R-NET-115, Z2-R-NET-116, Z2-R-NET-117, Z2-R-NET-118, Z2-R-NET-119, Z2-R-NET-120, Z2-R-NET-129, Z2-R-NET-141, Z2-R-NET-158 |
| Z2-I-NET-08 | Z2-R-NET-067, Z2-R-NET-076, Z2-R-NET-116, Z2-R-NET-121, Z2-R-NET-122, Z2-R-NET-123, Z2-R-NET-124, Z2-R-NET-125, Z2-R-NET-126, Z2-R-NET-127, Z2-R-NET-128, Z2-R-NET-129, Z2-R-NET-158 |
| Z2-I-NET-09 | Z2-R-NET-130, Z2-R-NET-131, Z2-R-NET-132, Z2-R-NET-133, Z2-R-NET-134, Z2-R-NET-135, Z2-R-NET-136, Z2-R-NET-158 |
| Z2-I-NET-10 | Z2-R-NET-122, Z2-R-NET-137, Z2-R-NET-138, Z2-R-NET-139, Z2-R-NET-140, Z2-R-NET-158 |
| Z2-I-NET-11 | Z2-R-NET-047, Z2-R-NET-052, Z2-R-NET-053, Z2-R-NET-070, Z2-R-NET-071, Z2-R-NET-123, Z2-R-NET-124, Z2-R-NET-141, Z2-R-NET-142, Z2-R-NET-143, Z2-R-NET-144, Z2-R-NET-145, Z2-R-NET-146 |
| Z2-I-NET-12 | Z2-R-NET-119, Z2-R-NET-147, Z2-R-NET-148, Z2-R-NET-149, Z2-R-NET-150 |
| Z2-I-NET-13 | Z2-R-NET-045, Z2-R-NET-151, Z2-R-NET-152, Z2-R-NET-153 |
| Z2-I-NET-14 | Z2-R-NET-028, Z2-R-NET-079, Z2-R-NET-098, Z2-R-NET-099, Z2-R-NET-100, Z2-R-NET-154, Z2-R-NET-155, Z2-R-NET-156, Z2-R-NET-157 |
| Z2-I-NET-15 | Z2-R-NET-106 |
| Z2-I-NET-16 | Z2-R-NET-107 |

## Excluded and deferred

| Capability | Classification | Rationale | Decision |
|---|---|---|---|
| local multiplayer (several players on one device) | excluded (Z2-R-NET-106) | Zenith2 is single-player first; remote players are NET's Stage 7 slots | Z2-D-076 |
| split-screen (several player views) | excluded (Z2-R-NET-107) | render views are never per player | Z2-D-076 |
| transport/session, replication, prediction/reconciliation, interest management, serialization, dedicated server builds, network simulation, network diagnostics | deferred to Stage 7, every requirement ticketed now (`Zenith2/Docs/Requirements/NET/Stage7.md`) | foundations first; the full tree waits for single-player production | Z2-D-073 |
| host migration, peer-to-peer topologies, NAT traversal, cross-platform sessions | no inventory item | one server per session; peers run the same build | Z2-D-073 |
