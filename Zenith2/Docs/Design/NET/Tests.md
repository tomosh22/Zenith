# NET design — tests

This file is part of the NET design family. The entry document is `Zenith2/Docs/Design/NET.md`. It lists every NET test target and every named test with the case it asserts. Every test case name is unique programme-wide (ruling TEST-NAME-UNIQUE) and starts `Test_Net`; workers never run or record anything (ruling WORKER-EVIDENCE): every expectation below is derivable from the specification and written by hand, or computed in the test, and fixtures are generated at test time (rulings GOLDEN-FILES, Q55).

## Scope of this file

| Group | Heading |
|---|---|
| targets and where each test lives | "Test targets" |
| Stage 3 unit and component tests | "Identity tests" … "Diagnostics tests" |
| the loopback suite | "Proof and loopback suite tests" |
| Stage 7 | "Stage 7 test catalogue" |
| gate steps | "Gate steps per test group" |

## Test targets

| Target | Created by | Sources | Presets | Notes |
|---|---|---|---|---|
| `ZenithNet_Tests` | `zenith_add_library(ZenithNet …)` | `Zenith2/Engine/ZenithNet/Tests/*.cpp` | `win`, `win-asan` (the module's `ZENITH_GPU` matrix narrowed to the host presets) | links ZenithNet, ZenithTestSupport; `TEST_DEPENDS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT` for the world tests, each run per backend through `#if ZENITH_TEST_LINKS_<TARGET>` |
| `Zenith_Fuzz_NetPacket`, `Zenith_Fuzz_NetSnapshotFragment`, `Zenith_Fuzz_NetInput`, `Zenith_Fuzz_NetPublication`, `Zenith_Fuzz_NetIdentitySection` | the test registry, one per `Tests/Fuzz/<Stem>.cpp` | `Zenith2/Engine/ZenithNet/Tests/Fuzz/` | `win-asan` | 60-second smoke; full duration in stage acceptance ([Z2-N-ARCH-952]) |
| `Zenith_Integration_NetLoopback` | `zenith_add_integration_tests(NetLoopback …)` | `Zenith2/Tests/NetLoopback/Zenith_NetLoopbackVerify_Tests.cpp` | `win` | the log verifier; its cases are skipped unless launched by the driver |
| the two loopback CTest entries | the test registry (row NT11) | `Zenith2/Tests/NetLoopback/Run-NetLoopback.ps1` | `win-debug`, `win-release` | label `z2-net-loopback` |
| `ZenithAuthoring_Tests` (NET's part) | EDIT's `ZenithAuthoring` | `Zenith2/Tools/Authoring/Tests/Net/Zenith_AuthoringNet_Tests.cpp` | `win` (`ZENITH_TOOLS_HOST`) | the authoring check |
| `ZenithCook_Tests` (NET's part) | ZenithCook | `Zenith2/Tools/ZenithCook/Tests/Zenith_NetCook_Tests.cpp` | `win` | the cook step |
| `CMake_Net_NoThreadOrMutexInSources`, `CMake_Net_PhaseSystemsReadNoClockOrEnvironment` | the test registry (row NT11) through SHIP's `zenith_register_script_test`, each a `cmake -P` run (precedent `ZenithCore.TranscendentalNoCrtCall`) | `Zenith2/Engine/ZenithNet/Tests/Source/Zenith_NetSourceChecks.cmake` | `win` | source checks over `Zenith2/Engine/ZenithNet` excluding `Tests/`, matching whole identifiers with comments and string literals stripped ([Z2-N-CONTRACT-416]'s convention). The first fails on `Zenith_Thread`, `Zenith_Mutex`, `Zenith_SharedMutex`, `std::thread` or `std::mutex` in any source. The second fails on `Zenith_GetMonotonicTimeUs`, `getenv`, `_wgetenv`, `GetEnvironmentVariableW`, `std::this_thread`, `GetCurrentThreadId`, `pthread_self`, `Zenith_GetCurrentThreadID` or `Zenith_AssetRegistry` in the sources on the simulation path: `Internal/Zenith_NetIdentityIndex.cpp`, `Zenith_NetSession.cpp`, `Zenith_NetClientApply.cpp`, `Zenith_NetCapture.cpp`, `Zenith_NetCommandSource.cpp` (inside `Input.Actions`), `Zenith_NetCommands.cpp` (playback), `Zenith_NetLifecycleListener.cpp` (sync point), `Zenith_NetSaveParticipant.cpp` (restore), `Zenith_NetQuantise.cpp` and `Zenith_NetWire.cpp` (the codecs the apply uses). Each script then re-invokes itself through `execute_process` on a generated directory holding one source with each token and asserts that run fails, and on one with the tokens only in comments and strings and asserts that run passes |

Support headers are header-only (ruling X184): `Zenith2/Engine/ZenithNet/Tests/Support/Zenith_NetTestWorld.h` (builds a world with the NET module's types, systems and per-world state on a chosen ECS backend without a runtime, through ECS's `Zenith_WorldSimulation` and a `Zenith_SystemRegistry`), `Zenith_NetTestPeers.h` (a server transport and a client transport over one `Zenith_NetMemoryPortPair` with a manual millisecond clock) and `Zenith_NetWireBuilders.h` (hand-built datagrams for the wire tests).

## Identity tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetworkID_LayoutAndHelpers` | `Zenith_NetIdentity_Tests.cpp` | `Zenith_MakeNetworkID(0, 1).m_ulValue == 1`; `(1, 7)` is `0x0001000000000007`; peer and counter round-trip for peers 0, 1, 65,534 and counters 1, 2^48 − 1; counter 0 and 2^48 give the invalid ID; `sizeof == 8` |
| `Test_NetworkIDMap_InsertFindErase` | `Zenith_NetworkIDMap_Tests.cpp` | insert 1,000 IDs, find each, erase every third, find the rest, the erased are absent, count matches |
| `Test_NetworkIDMap_FullAndDuplicate` | same | capacity 1,024: the 1,025th insert is `NET_ERROR_MAP_FULL`; inserting a present key is `NET_ERROR_DUPLICATE_NETWORK_ID`; neither changes the map |
| `Test_NetworkIDMap_CopyAscending` | same | after inserts in a scrambled order, `CopyAscending` returns strictly ascending IDs with their handles |
| `Test_NetworkIDMap_Create_FailsAtTable_Unwinds` | same | with the test heap's countdown at 1, `Create` returns `NET_ERROR_OUT_OF_MEMORY` and leaks nothing ([Z2-N-CONTRACT-341]) |
| `Test_NetIdentity_AssignsInSimulationIDOrder` | `Zenith_NetIdentity_Tests.cpp` | a server world spawns 5 identities in one tick in a merge order that differs from their slot order: after the next tick their IDs are peer-0 counters 1 … 5 in ascending simulation ID; per ECS backend |
| `Test_NetIdentity_IndependentOfWorkersAndLayout` | same | the same scenario with 1, 2, 8 and all workers, forced compaction and chunk capacities 64 and 256: identical IDs ([Z2-N-ARCH-886]); per backend |
| `Test_NetIdentity_OfflineAllocatesClientDoesNot` | same | an offline world assigns peer-0 IDs; a client world leaves non-proxy identities at 0 and logs the proxy-with-zero defect once |
| `Test_NetIdentity_MovedRowIsIdempotent` | same | forced compaction after assignment: the next pass changes no ID, inserts nothing, `net.ids.assigned` unchanged |
| `Test_NetIdentity_DuplicateRekeyedOnServer` | same | two entities restored with the same ID: the higher simulation ID is re-keyed to a fresh ID at the next tick; `net.ids.duplicates == 1`; the log line text |
| `Test_NetIdentity_DuplicateIgnoredOnClient` | same | the same on a client world: the newcomer is not indexed |
| `Test_NetIdentity_DestroyErasesAndQueues` | same | destroying a networked entity on a server erases its ID at the sync point and appends `{ ID, tick, 30 }` to the pending-destroy list; the entry leaves the list after the 30th capture that carried it |
| `Test_NetIdentity_CounterExhaustionStopsAssigning` | same | an allocator restored at 2^48 − 2: two more entities get the last two IDs, the third stays 0, `net.ids.exhausted == 1`, one log line |
| `Test_NetIdentity_MapFullLeavesUnindexed` | same | `net.maxNetworkedEntities` 1,024 and 1,025 identities: the 1,025th by simulation ID is unindexed, `net.ids.map_full` counted |
| `Test_NetIdentity_SaveRestoreKeepsIDsAndCounter` | `Zenith_NetIdentitySave_Tests.cpp` | save at tick 50 after 20 assignments, restore into a new world: every entity has its ID, the map holds 20, the next assignment is counter 21 |
| `Test_NetIdentity_RestoreRaisesLowCounter` | same | a section whose counter is below a restored ID's counter: the counter is raised to that counter + 1 and logged |
| `Test_NetIdentity_RestoreWithoutSectionResets` | same | `m_bSectionPresent` false: the allocator is `{1, 0, 0}` and restored IDs are indexed |
| `Test_NetIdentity_SectionCorruptRefused` | same | sections of 23 and 25 bytes (24 accepted), version 2, counter 0, counter 2^48, role byte 3, state byte 6, a state inconsistent with the role (CONNECTING for a server), started byte 2, a started byte inconsistent with the state (1 with CONNECTING), and a non-zero byte at each of offsets 15, 20, 21, 22 and 23: each is refused as corrupt |
| `Test_NetIdentity_ClientProxiesExcludedFromSave` | same | a client world's capture excludes every proxy and saves its non-proxy entities |
| `Test_NetIdentity_PeerIndexContinuesAfterRestore` | same | a restored server with highest issued peer 3 issues 4 to the next join |
| `Test_NetIdentity_SessionWorldCaptureIsCoherent` | same | the participant's `GetCaptureFidelity` is EXACT for an offline world and for a server and a client world before their first session event, and COHERENT for the server after PEER_JOINED and the client after CONNECTED (and after the session ends); the save of the connected client has `uSAVE_HEADER_FLAG_COHERENT_ONLY` |
| `Test_NetIdentity_RestoreClearsApplyTables` | same | a connected client world with proxies, pending spawns and tombstones, and a server world with a pending window and a joined peer, each restored from its own save: every apply table, the window, the next expected sequence and the peer set are empty; `Net.SessionState` reads `Disconnected` (client, reason `RESTORED`) and `Listening` (server) at the next tick; the next prologue compares the restore mark before dispatching any datagram, so an INPUT queued on the memory port before that prologue does not enter the cleared window, and sends DISCONNECT `RESTORED`; no session event is published |
| `Test_NetIdentity_RestoreOfPreSessionClientKeepsConnecting` | same | an EXACT save of a client world captured while `CONNECTING` (started byte 0), restored into a fresh client process at startup: `Net.SessionState` reads `Connecting` at the next tick, as in the uninterrupted run, the restore mark does not move, no DISCONNECT is sent, and the client then connects; a server captured `LISTENING` before any join restores `LISTENING` with the same peer issuance |
| `Test_NetIdentity_HighestPeerWrittenFromRecordedEvents` | same | a recorded server run with one join, saved after the join both live and during the replay of its log: both sections carry `m_uHighestPeerIssued` 1; a client's `m_uLocalPeer` equals the CONNECTED event's peer in both |
| `Test_NetIdentity_ReplayReproducesAssignments` | same | a recorded offline run with 200 spawns over 100 ticks, replayed: the map's `CopyAscending` equals the live run's at every tick |

## Authority tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetAuthority_TableOrderAndHash` | `Zenith_NetAuthority_Tests.cpp` | three test components registered in two different orders give identical entries in (type ID, property ID) order and an identical hash, equal to a hash computed in the test from the 14-byte records |
| `Test_NetAuthority_LocalReplicatedIsError` | same | a LOCAL component with a replicated property: `Build` fails `NET_ERROR_AUTHORITY` with `NET_AUTHORITY_DIAG_LOCAL_REPLICATED` naming it |
| `Test_NetAuthority_NonComponentIsError` | same | a replicated property on a struct type: `NET_AUTHORITY_DIAG_NON_COMPONENT` |
| `Test_NetAuthority_UnencodableIsWarning` | same | a replicated `f32` property: a warning, `Build` succeeds, the property is not in the table |
| `Test_NetAuthority_TooManyIsError` | same | 32 encodable replicated properties build a 32-entry table; 33 fail with `NET_AUTHORITY_DIAG_TOO_MANY` |
| `Test_NetAuthority_OwnerClientNote` | same | an OWNER_CLIENT component: a note; its properties are in the table as for SERVER |
| `Test_NetAuthority_NetTypesNotReplicated` | same | a registry where `Zenith.Net.Identity` property 3 carries the replicated flag: `NET_AUTHORITY_DIAG_NET_TYPE_REPLICATED` |
| `Test_NetAuthority_SchemaSameAsCpp` | same | a data-defined component and its C++ twin produce identical entries and diagnostics ([Z2-N-ARCH-592]) |
| `Test_NetAuthority_ValidatedInShipping` | same | in the `ShippingTest` configuration, `Build` runs and an error still fails Start |
| `Test_NetAuthority_ReportListsTableInOrder` | same | `net.authority`'s output for three test components: exact text, ending with the hash line |
| `Test_NetModule_RegistrationShape` | `Zenith_NetModule_Tests.cpp` | after `Zenith_Module_Net_RegisterTypes`: the two components with the property IDs, value types, authorities (1 Input, 2–4 CommandsOnly), flags (1 editor-hidden) and network authorities of "Authority census rows"; the struct, the four events, the two world resources with owning phases Input and SaveSnapshot, and the world-independent resource `Zenith.Net.Transport`; every key of "Configuration keys" with its range, default and flags; `Net.IdentityIndex` and `Net.ClientApply` declare `EmitsCommands` |
| `Test_NetModuleInstance_Create_FailsAtConfig_Unwinds`, `Test_NetModuleInstance_Create_FailsAtTable_Unwinds`, `Test_NetModuleInstance_Create_FailsAtTransport_Unwinds`, `Test_NetModuleInstance_Create_FailsAtPort_Unwinds` | `Zenith_NetModule_Tests.cpp` | each construction step of `Zenith_NetModuleInstance::Create` failed alone (an invalid key, the table's allocation, the transport's allocation, the port's open through the test heap's countdown and an occupied port): the step's error, zero leaks from the test heap, no registration left ([Z2-N-CONTRACT-341]) |
| `Test_NetSetOwner_PlaybackRules` | `Zenith_NetCommands_Tests.cpp` | `Net.SetOwner` sets property 3 on a server world; a stale target, a target without identity, peer 65,535, a 23-byte payload and any request on a client world are dropped and counted, each with its log reason |

## Tick and command tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetTicks_ExternalTickModeRefusedForNetworkRoles` | `Zenith_NetModule_Tests.cpp` | Start with `net.role=server` and `sim.ticksPerFrame=1` fails `MODULE_ERROR_REGISTRATION` with the message; offline with the same keys starts |
| `Test_NetTicks_MaxCatchUpAboveEightRefused` | same | a server with `sim.maxCatchUpTicks=9` fails Start |
| `Test_NetTicks_TickRateMismatchRejected` | `Zenith_NetSession_Tests.cpp` | a client at 60 Hz and a server at 30 Hz: REJECT `IDENTITY` detail `REPLAY_IDENTITY_FIELD_TICK_RATE`; the client publishes FAILED |
| `Test_NetWire_CommandRecordMatchesReplayEncoding` | `Zenith_NetWire_Tests.cpp` | 50 commands of every action type written through `Zenith_ReplayLogWriter` into a memory sink and through the INPUT encoder: the 18-byte records are byte-identical |
| `Test_NetCommands_ServerAppliesRemoteToSlotZero` | `Zenith_NetInput_Tests.cpp` | a server world with the NET bridge (`ReplacesLocalInput()` true) and a scripted window, plus an injected local transition: the world's local slice is empty, and phase 1's action state equals the state `Zenith_ApplyInputCommands` produces from the window's commands alone |
| `Test_NetCommands_ClientLocalInputStillMapped` | same | a client world with the NET bridge (`ReplacesLocalInput()` false) and an injected local transition: the command reaches the client's action state, its replay log's tick block (memory sink) and, once the transport has adopted an ACCEPT, the outbound ring |
| `Test_NetCommands_ClientObservesEveryTick` | same | a client world over a catch-up frame of 3 ticks: three outbound blocks with the tick commands in order; none before the transport adopts the ACCEPT (`m_bAdopted`); tick c0's commands are in the first block and every sequence is at least B; and the session base sequence is the one after the last command observed before CONNECTED (0 when none) |
| `Test_NetCommands_ReplayCallsObserveNotProvide` | same | under a publication source (replay), `ProvideTickCommands` is never called and `ObserveTickCommands` is called every tick |

## Headless tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetHeadless_ModuleStartsWithoutDevice` | `Zenith_NetModule_Tests.cpp` | a headless test-access runtime with the Net module: Start, attach and 60 ticks succeed; no renderer, audio device or window was created |
| `Test_NetHeadless_ServerBindsLoopbackOnly` | same | `net.bindAddress=0.0.0.0` fails Start with `NET_ERROR_ADDRESS_REFUSED`; `127.0.0.1` binds |
| `Test_NetHeadless_ShippingForcesOffline` | same (`ShippingTest`) | `net.role=server` in a Shipping test build runs offline, logs the line, creates no port |
| `Test_NetHeadless_ReplayCreatesNoTransport` | same | under `--replay` with a recorded `net.role=server`, Start creates no transport and the systems run |
| `Test_NetHeadless_ReplayOfClientLogStartsWithoutConnect` | same | under `--replay` of a generated client log (header `net.role=client`, `sim.ticksPerFrame` absent, no `net.connect`): Start succeeds, creates no transport, skips the `net.connect` and `sim.ticksPerFrame` checks, and the client systems run; the same configuration live fails Start naming `net.connect` |
| `Test_NetClient_WindowedRendersProxies` | `Zenith2/Tests/NetLoopback/Zenith_NetLoopbackRender_Tests.cpp` | `needs-gpu` (`gpu-reason:requires-graphics-test`): a windowed client with 4 proxies records 4 skinned draws in the main view |

## Wire format and fuzz tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetWire_PacketHeaderRoundTrip` | `Zenith_NetWire_Tests.cpp` | every kind's header fields round-trip; the hand-built bytes of a KEEPALIVE equal the encoder's (a golden written in the test from the table) |
| `Test_NetWire_CrcRejectsCorruption` | same | flipping any one byte of a valid datagram makes it malformed (all 24 + n positions) |
| `Test_NetWire_SizeLimits` | same | 23 and 1,201 bytes refused; 24 and 1,200 accepted |
| `Test_NetWire_ExactBodyLengths` | same | each fixed-size kind with its length ± 1 refused |
| `Test_NetWire_ReservedBytesMustBeZero` | same | every reserved byte of every kind set to 1 makes the packet malformed |
| `Test_NetWire_InputLimits` | same | 64 commands in one block with no digest pair (a 1,176-byte body) accepted; 65 refused (a 1,194-byte body passed to the INPUT decoder directly); 61 commands with four digest pairs accepted; 5 blocks refused; 5 digest pairs refused; blocks out of target order refused; a non-zero player slot refused; a session base sequence different from the session's refused |
| `Test_NetWire_SnapshotLimits` | same | 32 fragments accepted, a count of 33 refused; index ≥ count refused; 64 destroys accepted, 65 refused; a body of 1,020 accepted, 1,021 refused; descending IDs refused; a presence bit beyond the table refused; a proxy index beyond the table refused; a hash mismatch counted as `net.packets.table` |
| `Test_NetWire_ULEB128RoundTripAndCanonical` | same | the cases of `Zenith2/Docs/Design/NET/Loopback.md` → "Unsigned LEB128" |
| `Test_NetWire_SlidingWindowDropsDuplicates` | same | sequences 1 … 70 delivered with 5 duplicates and 3 reordered: duplicates and those older than the 64-window dropped, reordered within it accepted |
| `Test_NetSession_PumpBoundedAt512` | `Zenith_NetSession_Tests.cpp` | 513 valid datagrams queued on the memory port before one prologue: the pump dispatches 512, the 513th is dispatched by the next prologue, none is dropped |
| `Test_NetSession_SendBoundedAt512` | same | 513 datagrams queued for one `Net.Send`: 512 are sent, the 513th in the next frame's `Net.Send`, and `net.send.deferred` + 1 |
| `Test_NetMemoryPort_OrderFullAndLimits` | same | a memory port pair delivers datagrams in send order with the sender's address; a full ring drops the newest and counts it; ring capacities 1 and 65,536 are accepted and 0 and 65,537 refused with `NET_ERROR_INVALID_CONFIG`, as are equal addresses; a datagram of 1,200 bytes is delivered and one of 1,201 refused with `NET_ERROR_LIMIT` |
| `Test_NetWire_MalformedBurstDisconnects` | `Zenith_NetSession_Tests.cpp` | 8 malformed packets within one second from the connected peer disconnect it with `PROTOCOL_ERROR`; 7 do not |
| `Test_NetQuantise_PositionGoldenVectors` | `Zenith_NetQuantise_Tests.cpp` | the vectors of `Zenith2/Docs/Design/NET/Loopback.md` → "Quantisation" with hand-computed integers: 0; 0.0005 m → 0 and 0.0015 m → 2 (ties to even); −0.0015 m → −2; ±8,388.6064453125 m → ±8,388,606 unclamped; ±8,388.607 m (binary32 8,388.607421875, product 8,388,607.5, tie to even 8,388,608) → ±8,388,607 with `net.quantise.clamped` + 1; NaN → 0 with `net.quantise.non_finite` + 1; dequantisation equals `float(q) / 1000.0f` |
| `Test_NetQuantise_QuatGoldenVectors` | same | the quaternion vectors with hand-computed packed words: identity, a 90° rotation about each axis, a quaternion whose largest component is negative (encoded as its negation), two equal largest components (the lower index wins); each round trip within 0.0048 rad; q and −q encode identically |
| `Test_NetQuantise_PropertyRoundTrip` | same | (i) every integer q with \|q\| ≤ 8,192,010 dequantises and quantises back to q (exhaustive); (ii) 100,000 seeded random positions within ±8,388.607 m dequantise within 1.24 mm; (iii) 100,000 seeded random unit quaternions dequantise within 0.0048 rad, the angle measured in binary64 as 4 · asin(‖q − s·q′‖ / 2) with s the sign of the dot product (never a binary32 `acos`, whose ulp at this angle exceeds the bound's slack), plus the constructed case u = (874, 872, 874) offset −0.4999 of a step, which reaches 0.004784 rad; (iv) dequantising the same word twice gives the same bits, for every word of both codecs drawn |
| `Test_NetQuantise_YawTieBandBitEquality` | same | yaw rotations sampled every 0.0001° across 89° … 91° and 269° … 271° (the band where the reconstructed largest component can lose its tie): the C1 comparison of a stored dequantised rotation with the dequantisation of its word never reports a mismatch, and every error is within 0.0048 rad; the test also counts, and does not assert on, the words that do not requantise to themselves. It compares a pure function with itself and cannot fail while dequantisation stays pure: it is a guard against a future impure or state-dependent dequantiser, not evidence of storage (that is `Test_NetClient_AppliedValuesExact`) |
| `Test_NetFragments_EncodeDeterministic` | `Zenith_NetSnapshot_Tests.cpp` | the same entries encoded twice give identical bytes; a record never spans fragments; 64 entities of the loopback table fit 2 fragments; each body ≤ 1,020 |
| `Test_NetFragments_TruncationCursorCoversTail` | same | 600 entities: the first capture holds the 512 lowest IDs and sets the cursor to the 513th; the second starts there and wraps, every fragment of the wrapped capture is decoded through `Zenith_NetDecodeSnapshotFragment` without error, the wrap begins a new fragment and each fragment's IDs ascend; every entity appears within two captures; a capture stopped by the fragment budget (entities with 32 replicated entries) resumes at the first record it left out, and over ⌈count / k⌉ captures every entity appears; `net.server.snapshot_truncated` counted per stopped capture |
| `Test_NetFragments_DestroyFloodStaysWithin32Fragments` | same | 5,000 networked entities destroyed in one tick with 64 survivors: every capture has at most 32 fragments and at most 256 destroys; each destroyed ID is carried by exactly 30 captures, counted from its first; the survivors appear in every capture |
| `Test_NetFragments_DestroyListRepeated` | same | one destroy appears in the next 30 captures and not in the 31st |
| `Test_NetPublication_PayloadLayout` | `Zenith_NetPublication_Tests.cpp` | the 4-byte prefix and the 28-byte session event layout byte for byte; a payload with a bad prefix is dropped and counted; a 1,024-byte fragment payload is accepted and a 1,025-byte one refused at the decoder |
| `Zenith_Fuzz_NetPacket` (fuzz) | `Tests/Fuzz/Zenith_Fuzz_NetPacket.cpp` | `Zenith_NetReadPacket` and every message decoder on arbitrary bytes: no crash, no read outside the input, no assert under ASan |
| `Zenith_Fuzz_NetSnapshotFragment`, `Zenith_Fuzz_NetInput`, `Zenith_Fuzz_NetPublication`, `Zenith_Fuzz_NetIdentitySection` (fuzz) | `Tests/Fuzz/` | the fragment decoder, the INPUT decoder, the publication payload decoder and the participant's `RestoreSection` parser, as above |

## Session tests

All over `Zenith_NetTestPeers.h` (memory ports, manual clock).

| Test | File | Asserts |
|---|---|---|
| `Test_NetSession_HandshakeOverMemoryPort` | `Zenith_NetSession_Tests.cpp` | client `Start` → REQUEST → ACCEPT: client `CONNECTED` with peer 1, the server slot `CONNECTED`, CONNECTED and PEER_JOINED published once each |
| `Test_NetSession_LostAcceptRetried` | same | the first ACCEPT dropped: the client resends after 250 ms and receives an ACCEPT with the same session ID, peer index, delay and interval but the server's current last completed tick; the client's offset is computed from that retry's tick, so its first block targets a tick at least `net.inputDelayTicks` − 1 ahead of the server |
| `Test_NetSession_RejectReasons` | same | one case each: version, each identity field in order, replication table, proxy table, server full (a second client), not accepting (the server's session world detached); each detail byte; the client publishes FAILED with the reason |
| `Test_NetSession_ConnectTimeout` | same | no server: after 10,000 ms of manual clock the client publishes FAILED (`CONNECT_TIMEOUT`) and its transport is `DONE` |
| `Test_NetSession_PeerTimeoutBothSides` | same | the client goes silent: the server publishes PEER_LEFT (`TIMEOUT`) after 5,000 ms; the server goes silent: the client publishes DISCONNECTED (`TIMEOUT`) |
| `Test_NetSession_KeepaliveKeepsIdleSessionAlive` | same | no traffic but keepalives for 60 s of manual clock: still connected; keepalives every 250 ms |
| `Test_NetSession_LingerDropsOldSession` | same | after the peer leaves, its in-flight packets within 1 s are dropped as foreign and a new client is rejected `SERVER_FULL` until the linger ends |
| `Test_NetSession_ShutdownSendsDisconnect` | same | destroying the server instance sends DISCONNECT `SERVER_SHUTDOWN` three times; the client publishes DISCONNECTED with that reason |
| `Test_NetSession_WorldSwapEndsSession` | same | a world transition on the client sends DISCONNECT `WORLD_TRANSITION` and attaches no session to the new world |
| `Test_NetSession_ForeignAndOldPacketsDropped` | same | a packet from an unknown address, with a wrong session ID, or with a replayed sequence is dropped with its counter |
| `Test_NetSession_ProxyNotResidentFails` | same | a client whose proxy prefab is not ACTIVE when its transport starts (the prologue after its first tick) publishes FAILED (`PROXY_NOT_RESIDENT`) and sends nothing; no phase-1 system read the asset registry |
| `Test_NetSession_StateIsFunctionOfRecordedEvents` | same | a live server and client over memory ports, each recording, and the replay of each log: `Net.SessionState` read by a test system in phase 2 is identical at every tick live and in replay — `Listening` on the server before the join and after the peer leaves, `Connected` while joined; `Connecting`, `Connected`, `Disconnected` on the client |
| `Test_NetSession_JoinAtFirstPrologue` | same | a client started in the same frame as its server: its first request reaches the server before the server's first tick and is dropped as `net.packets.early`; the retry after 250 ms is accepted; PEER_JOINED is published, and a test behaviour that defers its reaction until its first-tick spawn has run receives it |
| `Test_NetSession_PreConnectedTransportStatesDropTraffic` | same | a client in `CONNECTING` receiving SNAPSHOT, KEEPALIVE and INPUT drops each by the session check (`net.packets.foreign`) and stays `CONNECTING`; 8 malformed packets in one second before CONNECTED are counted and disconnect nothing |

## Input path tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetInput_TargetTickOffset` | `Zenith_NetInput_Tests.cpp` | accept at server tick 100 with delay 4 received when the client's next tick is 37: offset 67; client tick 37's block targets 104 |
| `Test_NetInput_RedundantBlocks` | same | each INPUT carries the new block and the 3 previous; with 61 commands in one tick, a lone block; with 100, two INPUTs of 61 and 39; every INPUT carries the digest ring's pairs |
| `Test_NetInput_WindowReleasesInSequence` | same | blocks arriving out of order are released in sequence order at their targets; nothing is released before its target |
| `Test_NetInput_LateCommandCounted` | same | a block arriving 2 ticks after its target is released at the next tick; `net.late_commands == ` its command count |
| `Test_NetInput_GapHeldThenLost` | same | a missing sequence holds the subsequent commands 4 ticks past the waiting command's target, then counts `net.server.lost_commands` and releases |
| `Test_NetInput_FirstSequenceSetsBaseline` | same | a session whose base sequence is 7 (seven commands observed before CONNECTED): commands 7 … 9 release at their targets, `net.server.lost_commands` and `net.late_commands` stay 0; a session with base sequence 0 releases its first command 0 |
| `Test_NetInput_EarlyCommandBehindGapWaits` | same | sequence 5 missing and sequence 6 targeting tick t + 3: at ticks t … t + 3 nothing is declared lost (the signed difference is negative); 5 arriving at t + 2 releases 5 then 6 at their targets |
| `Test_NetInput_ReleaseBoundedByOutput` | same | 10 commands due at tick t and an output span of 4: four are released at t, the rest at t + 1 and t + 2 in sequence order, each counted late |
| `Test_NetInput_BurstPoolFullDropsNewest` | same | a client tick of 4,160 commands fills its 64-command slot and the 4,096-command burst pool, the 4,161st is dropped (`net.client.outbound.dropped` + 1); on the server, 4,096 + 64 commands for one target tick fill a slot and the pool and one more is dropped (`net.server.window.dropped` + 1), its sequence lost after the hold |
| `Test_NetInput_DuplicatesAndTooEarlyDropped` | same | a sequence below the next expected one counts duplicates; a target 129 ticks ahead counts too-early |
| `Test_NetInput_ServerLogHoldsAppliedCommands` | same | a server world recording to a memory sink: every tick's logged commands equal the commands the window released that tick |

## Snapshot path tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetServer_CaptureEncodesAuthoritativeState` | `Zenith_NetSnapshot_Tests.cpp` | a server world with 10 identities moved by a test system in phase 6: the phase-11 capture's decoded values equal the quantised `Zenith.LocalTransform` values of that tick; per backend |
| `Test_NetServer_CaptureIndependentOfWorkers` | same | the capture bytes are identical with 1, 2, 8 and all workers, forced compaction and two chunk capacities ([Z2-N-CONTRACT-356]) |
| `Test_NetServer_CaptureSkipsProxiesAndUnflagged` | same | entities without `REPLICATE`, with `PROXY` or with ID 0 are absent; an unlisted proxy prefab is skipped and counted once |
| `Test_NetServer_CaptureRingSendsEveryCatchUpTick` | same | a frame of 3 ticks sends 3 ticks' fragments in ascending tick order |
| `Test_NetServer_CaptureSelfCheckP6` | same | in proof mode the capture decodes its own fragments and matches the quantised authoritative values every tick (`net.proof.capture_mismatches` 0); a test hook that corrupts one encoded value counts one mismatch; the capture chain line at destruction has the capture count and the chain computed in the test |
| `Test_NetProxyTable_BuildRules` | same | one set entity with two prefabs: indices 0, 1 and the hash over their IDs; one with 16 distinct prefabs (the component's full array): indices 0 … 15; two set entities, a repeated prefab and an empty set are each invalid |
| `Test_NetClient_IntakePublishesInArrivalOrder` | `Zenith_NetClient_Tests.cpp` | three fragments received in a frame are published at the next tick boundary in completion-ID order with the 4-byte prefix |
| `Test_NetClient_SinkFullDropsFragment` | same | with an in-flight capacity of 32 and 40 fragments in one frame: 32 published, 8 counted in `net.client.fragments_dropped_full` |
| `Test_NetClient_SessionEventSurvivesFullFragmentBacklog` | same | a paused client world (no tick boundary) with fragments arriving until the outstanding count reaches `m_uMaxInFlight` − 8: further fragments are dropped, then a DISCONNECT from the server publishes DISCONNECTED from the reserve, and the world reads `Disconnected` at its first tick after resuming |
| `Test_NetClient_ApplySpawnsUpdatesDestroys` | same | a fragment with a new ID spawns the proxy at the record's transform with the identity (PROXY flag); the next updates it by `SetProperty`; a destroy removes it; per backend |
| `Test_NetClient_StaleFragmentSkipped` | same | a fragment of tick 10 after tick 12 was applied leaves the proxy at tick 12's values; `net.client.fragments_stale` counted |
| `Test_NetClient_TombstoneBlocksResurrection` | same | a proxy destroyed by the fragment of tick 12, then a reordered fragment of tick 10 naming it: no proxy is spawned (`net.client.fragments_stale`); after 64 more ticks the tombstone has left the table |
| `Test_NetClient_MapFullSpawnsNothing` | same | a client world with `net.maxNetworkedEntities` 16 and 17 records in every fragment: 16 proxies spawn once, the 17th never spawns (`net.client.spawns_refused_map_full` counted per tick) and no proxy is spawned twice |
| `Test_NetClient_FragmentTooOldDropped` | same | a fragment 64 ticks older than the newest received is dropped in the prologue (`net.client.fragments_too_old`) and never published |
| `Test_NetClient_PendingSpawnNotDuplicated` | same | two fragments of the same new ID published at one boundary spawn one proxy and apply the second's values to the ticket |
| `Test_NetClient_ControlledEntityEvent` | same | a record whose owner becomes the local peer emits `Net.ControlledEntityChanged` once with the previous entity |
| `Test_NetClient_AppliedValuesExact` | same | after apply, each proxy's `Zenith.LocalTransform` equals the dequantised record values bit for bit |
| `Test_NetClient_ReplayReproducesProxies` | same | a recorded client run replayed through a publication source: proxy set and transforms equal at every tick |
| `Test_NetAck_DigestVerifiedAndMismatch` | `Zenith_NetSession_Tests.cpp` | a complete tick reported: `net.proof.ack_verified` + 1, a repeat of the same pair changes nothing; a corrupted digest: `net.proof.ack_mismatches` + 1; a pair older than 64 captures: unverifiable; a pair for a tick never captured: malformed |
| `Test_NetDigests_EveryCompleteTickReported` | same | a catch-up frame completing 8 ticks in one pump: the client sends a digest-only INPUT before the fifth enters the ring, the server verifies all 8, its distinct verified count equals the client's cumulative count, and `net.proof.digests_unreported` is 0; ticks completing out of tick order are carried in completion order and emitted sorted by tick; in the steady case (one completion per INPUT) dropping any three consecutive INPUTs loses no pair, and dropping the one INPUT that carried a burst's pairs makes P3 report the shortfall |

## Proof and loopback suite tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetProof_AssertionsReportThroughSink` | `Zenith_NetProof_Tests.cpp` | in a test-access runtime with a recording assertion sink, each of P1–P6 and C1–C4 failing alone reports exactly its reason line; all holding reports nothing; and in a test-access runtime with the real sink, one failing assertion recorded from the module instance's destructor makes the run's exit code 5 (the negative case of the exit-code path) |
| `Test_NetProof_ForcedOffInShipping` | same (`ShippingTest`) | `net.proof.mode=true` in Shipping is forced false with a warning |
| `Test_Net_LoopbackReplicatesTransformsAndCommands` | the driver, case 1 | two processes per `Zenith2/Docs/Design/NET/Loopback.md` → "Driver script": both exit 0 (every proof assertion held in-process), and `Test_NetLoopbackVerify_CommandLogs` passes ([Z2-N-ARCH-786]) |
| `Test_Net_LoopbackLogsReplayBitIdentical` | the driver, case 2 | the server and the client logs of a fresh loopback run each replay to exit 0 with no tick-hash mismatch, and the server replay's capture chain line equals the live server's, each server process's copied logs holding exactly one chain line |
| `Test_NetLoopbackVerify_CommandLogs` | `Zenith2/Tests/NetLoopback/Zenith_NetLoopbackVerify_Tests.cpp` (skipped unless launched) | the six checks of `Zenith2/Docs/Design/NET/Loopback.md` → "Command log verification" |
| `Test_NetLoopbackVerify_FailsWithoutConnection` | same, not skipped | given generated logs with no CONNECTED event (written in the test through `Zenith_ReplayLogWriter`), the verification routine reports "client never connected" |
| `Test_NetLoopbackVerify_DetectsMissingCommand` | same, not skipped | generated logs where the server lacks one sequence: the routine reports that sequence |

## Script surface tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetBindings_ReadFunctions` | `Zenith_NetBindings_Tests.cpp` | in a server, client and offline world: `Net.Role`, `Net.SessionState`, `Net.LocalPeer`, `Net.Peers`, `Net.NetworkID`, `Net.FindEntity`, `Net.OwnerPeer`, `Net.IsLocallyControlled` return the documented values, including `nil` cases |
| `Test_NetBindings_SetOwnerBuffered` | same | `Net.SetOwner` in a behaviour changes property 3 from the next phase; a failing behaviour after the call leaves it unchanged (the command was rolled back) |
| `Test_NetBindings_MisuseCodes` | same | ZS3401 (`Net.FindEntity` with counter 0, 2^48, 1.5 or peer 65,535), ZS3402 (client world), ZS3403 (no identity), ZS3404 (`Net.SetOwner` with peer 65,535 or −1; a `SetOwner(…, 65535)` raises ZS3404, never ZS3401) each raise the catalogued text, never an assert |
| `Test_NetEvents_SessionEventsDelivered` | `Zenith_NetEvents_Tests.cpp` | recorded session publications produce `Net.SessionChanged`, `Net.PeerJoined`, `Net.PeerLeft` in phase 9 of the publishing tick with the documented fields |

## Diagnostics tests

| Test | File | Asserts |
|---|---|---|
| `Test_NetDiagnostics_CountersRegistered` | `Zenith_NetDiagnostics_Tests.cpp` | every counter of `Zenith2/Docs/Design/NET.md` → "Counters" exists in the profiler's registry with its kind |
| `Test_NetDiagnostics_StatusCommandFormat` | same | `net.status` on a connected server and client: the exact lines |
| `Test_NetDiagnostics_FrameDumpSection` | same | the `net` frame-dump section's JSON keys for a server world with one peer |
| `Test_NetDiagnostics_LogRateLimited` | same | 100 malformed packets in one second log one `net.wire` line per kind |
| `Test_NetAuthoring_RejectsAuthoredNetworkID` | `Zenith2/Tools/Authoring/Tests/Net/Zenith_AuthoringNet_Tests.cpp` | a scene document with `Zenith.Net.Identity` property 1 non-zero is an error naming the entity; 0 passes |
| `Test_NetAuthoring_ProxyPrefabRules` | same | a proxy prefab carrying a rigid body, a character controller, a behaviour or an identity is an error each; a clean one passes; a scene with two `Zenith.Net.ProxyPrefabSet` entities is an error |
| `Test_NetCook_StepRegisteredAndRuns` | `Zenith2/Tools/ZenithCook/Tests/Zenith_NetCook_Tests.cpp` | the step `net.validate` is registered and fails the cook of a generated scene with an authored network ID |

## Stage 7 test catalogue

| Test | Target | Asserts |
|---|---|---|
| `Test_NetTransportConformance_ReliableOrderedUnderProfiles` | `ZenithNet_TransportConformance` (per candidate) | 10,000 reliable messages under `none`, `broadband`, `mobile`, `hostile` (manual clock, seeded): all delivered once, in order |
| `Test_NetTransportConformance_UnreliableSequencedDropsOld` | same | an older sequenced message after a newer one is never delivered |
| `Test_NetTransportConformance_FragmentReassembly` | same | 64 KiB messages reassemble under loss; a fifth concurrent group is refused |
| `Test_NetTransportConformance_RttEstimate` | same | under `broadband` the estimate converges within 10% of 60 ms in 5 s |
| `Test_NetTransportConformance_BandwidthBudget` | same | sent bytes per second stay within the budget ± one burst |
| `Test_NetHandshakeV2_ChallengeStateless` | `ZenithNet_Tests` | requests without responses allocate no slot; a forged cookie is refused; the previous epoch's cookie is accepted |
| `Test_NetHandshakeV2_RateLimits` | same | the 11th request per second from one address is dropped and counted |
| `Test_NetSecurity_UnauthenticatedRefusedInShipping` | same (`ShippingTest`) | a Stage 7 Shipping server refuses a non-loopback session without the credential the ruling requires |
| `Test_NetClock_LeadConvergesAndNudges` | same | with RTT stepping from 30 to 120 ms, the lead converges to the target within 2 s and nudges never exceed 2 ms per frame |
| `Test_NetReplication_DeltaRoundTripProperty` | same | 1,000 seeded random property histories: delta-encoded against random acknowledged baselines, the client reconstructs every snapshot exactly |
| `Test_NetReplication_CreateBeforeState` | same | under reordering, no state is applied before its CREATE |
| `Test_NetReplication_PriorityOrderDeterministic` | same | the same server state, budget and acks give the same entity order and bytes |
| `Test_NetReplication_JoinInProgressFullState` | same | a peer joining at tick 500 holds every relevant entity's exact state after its first snapshot |
| `Test_NetReplication_OwnerProposalsValidated` | same | in-range proposals applied, out-of-range rejected and corrected |
| `Test_NetReplication_PerPeerPlayerSlots` | same | two peers' commands drive slots 1 and 2 independently |
| `Test_NetReplication_WorldTransitionKeepsSession` | same | a server transition: peers reload, ack ready, receive CREATEs, keep their slots |
| `Test_NetCodecs_GoldenVectors` | same | each codec of `Zenith2/Docs/Design/NET/Stage7.md` → "Property codecs" against hand-computed bit strings |
| `Test_NetCodecs_PropertyRoundTrip` | same | seeded random values per value type round-trip within the declared precision |
| `Test_NetBits_ReaderNeverOverreads` | same | every truncation of a valid bit stream is refused without reading past the buffer |
| `Test_NetPrediction_CorridorNoMispredictionAtFixedLatency` | `Zenith_Integration_NetPrediction` | the `PredictionCorridor` scenario at a constant 100 ms simulated latency: zero mispredictions |
| `Test_NetPrediction_CorrectionReplaysToServerState` | same | an injected server correction: after reconciliation the predicted state equals the server's state advanced by the buffered commands |
| `Test_NetPrediction_SmoothingIsPresentationOnly` | same | the visual offset changes no simulation state and decays to zero |
| `Test_NetPrediction_StallsBeyondLimit` | same | without acks for 17 ticks, `MaxPredictedTicks` 16 stalls new commands |
| `Test_NetInterpolation_RemoteProxyBetweenSnapshots` | same | a remote proxy's presented position lies on the segment between its two buffered snapshots |
| `Test_NetInterest_RadiusAndHysteresis` | `ZenithNet_Tests` | entering at R creates, leaving beyond 1.2 R for 2 s destroys, oscillating at R churns nothing |
| `Test_NetInterest_AlwaysRelevantAndOwned` | same | owned and always-relevant entities stay relevant at any distance |
| `Test_NetInterest_CapacityKeepsNearest` | same | above `maxRelevantPerPeer`, the nearest are kept |
| `Test_NetInterest_StreamingViewersPerPeer` | `Zenith_Integration_NetStreaming` | a server with two distant peers keeps both peers' cells resident |
| `Test_NetServerBuild_LinksNoRhiBackend` | the CMake fixtures | the `<Name>Server` executable's link closure has no `ZenithRHI_*` backend |
| `Test_NetServerBuild_BootsAndServes` | the loopback driver, server-build variant | the dedicated server executable serves the loopback client to the same proof outcome |
| `Test_NetSimulation_DeterministicWithSeed` | `ZenithNet_Tests` | the same seed, manual clock and sends drop, duplicate and delay the same datagrams |
| `Test_NetSimulation_ProfilesWithinTolerance` | same | over 100,000 datagrams each profile's measured loss and latency lie within 10% of its parameters |
| `Test_NetSimulation_RefusedInShipping` | same (`ShippingTest`) | `Create` returns `NET_ERROR_NOT_IN_SHIPPING` |
| `Test_NetDiagnostics_PeerStatisticsPanel` | same | the `Net` panel rows for two peers |
| `Test_NetDiagnostics_PacketCaptureFormat` | same | a capture file's header and records byte for byte; `net-dump` decodes it |
| `Test_NetSession_Stage7StatesFollowTables` | `ZenithNet_Tests` | every cell of the Stage 7 client and server state tables of `Zenith2/Docs/Design/NET/Stage7.md` → "Stage 7 state machines", driven over memory ports with a manual clock: the state, the published event, the datagram sent and, for each failure cell, the `Zenith_NetErrorCode` of "Transport and session error codes"; the Android-background case (a paused session world timed out after `net.peerTimeoutMs`) |
| `Test_NetSession_ReconnectAfterDisconnect` | same | a Stage 7 client disconnected by a timeout reconnects through `Net.Connect` with a new nonce, is `CONNECTED` again under a new peer index (indices are never reused) and receives a full state before its first snapshot |
| `Test_NetReplication_BroadcastTargets` | same | a replicated event sent with target `"All"`, `"Owner"` (an entity argument owned by peer 2) and peer index 3 reaches exactly those peers, each emitting it in phase 1 |
| `Test_NetReference_StringAndAssetTablesAddBeforeUse` | same | a string or asset reference is never sent before its table entry is acknowledged; under reordering the receiver never sees an unknown index |
| `Test_NetReference_UnresolvedEntityIsNullAndCounted` | same | an entity reference to an ID the client does not hold decodes as the null handle and counts `net.client.unresolved_references` |
| `Test_NetSerialization_LimitsAtAndBeyond` | same | each Stage 7 limit of `Zenith2/Docs/Design/NET/Stage7.md` → "Serialization limits" accepted at the limit and refused at limit + 1 |
| `Zenith_Fuzz_NetBitStream` (fuzz) | `Tests/Fuzz/Zenith_Fuzz_NetBitStream.cpp` | the bit reader and every Stage 7 codec on arbitrary bytes: no crash, no read outside the input, no assert under ASan |
| `Zenith_Fuzz_NetMessageV2` (fuzz) | `Tests/Fuzz/Zenith_Fuzz_NetMessageV2.cpp` | every wire format version 2 message decoder and the piece reassembly on arbitrary bytes, as above |
| `Test_NetDiagnostics_DebugViewsAndDump` | same | the Stage 7 debug views (relevance, priority, prediction error) and the `net` frame-dump section's Stage 7 keys for a server with two peers |
| `Test_NetHandshakeV2_ResponseNeverLargerThanRequest` | same | for every request size the challenge response is at most the request's bytes (no amplification) |
| `Test_NetHandshakeV2_TotalRateAndBanWindow` | same | the 1,001st request in one second across all addresses is dropped and counted in `net.handshake.rate_limited`; a source with 8 failed cookie checks in 10 s is ignored for 30 s and accepted again after it |
| `Test_NetTransportConformance_UnreliableGroupTimeout` | `ZenithNet_TransportConformance` (per candidate) | an unreliable fragment group missing a piece is dropped 1 s after its first piece and counted |
| `Test_NetReplication_BaselineRingAndCap` | `ZenithNet_Tests` | the server keeps the 32 newest snapshots per peer as baselines; an acknowledgement older than the ring forces a full state; `net.server.baselineBytesPerPeer` caps the ring's bytes, dropping the oldest snapshots first |
| `Test_NetPrediction_OnlyOwnedPredictedEntities` | same | a client predicts exactly the owned entities carrying `Zenith.Net.Predicted`; an owned entity without `Zenith.Net.Predicted` is interpolated; a system declaring `NotPredicted()` is not re-run by a reconciliation |
| `Test_NetCodecs_PrecisionDeclarationChangesTableHash` | same | changing one property's declared range or precision changes the replication table hash, and peers with different declarations are rejected `REPLICATION_TABLE` |
| `Test_NetPrediction_ReplayReproducesCorrections` | `Zenith_Integration_NetPrediction` | a recorded Stage 7 client run with clock offset changes and corrections, replayed: the `OFFSET_CHANGED` events and the corrections recur at the same ticks and every tick hash matches |
| `Test_NetReplication_RewindQueryReplays` | `ZenithNet_Tests` | a server whose script calls `Net.RewoundRaycast` for a peer: under replay the `PEER_VIEW` records give the same view tick and the same hit at every tick; a test system that moves the target in the live world after the history copy was taken changes no query result (the query never reads the live world) |
| `Test_NetPublication_Stage7PiecesReassemble` | same | a 65,536-byte message is published as 65 pieces of at most 1,024 bytes each (prefix, 4-byte piece header, 1,016 data bytes) at one tick boundary and reassembles in the phase-1 intake; with too little in-flight capacity a reliable message waits in the receive queue and an unreliable one is dropped and counted, and no completion ID is left unparked; a piece count above 65, a missing piece or an inconsistent count is dropped as malformed |
| `Test_NetWireV2_VersionsRejectEachOther` | same | a version-1 request to a version-2 server is answered by a version-1 `CONNECT_REJECT` `VERSION` and vice versa; a version-2 datagram's message framing `{ kind, channel, length }` and its 16-byte AEAD tag are checked, a truncated frame or a wrong tag dropped and counted |

## Gate steps per test group

| Group | Gate step |
|---|---|
| every `ZenithNet_Tests` case, the authoring and cook cases | `Test` (`win-debug`, `win-release`); `win-asan` through the `Sanitizers` step when the change touches the `Zenith2Memory` paths |
| fuzz targets | `Sanitizers -Preset win-asan` (60-second smoke); stage acceptance (full duration) |
| the two loopback entries and the verifier | `Test` (`win-debug`, `win-release`, label `z2-net-loopback`); Stage 3 acceptance |
| `Test_NetClient_WindowedRendersProxies` | a `needs-gpu` run in Stage 3 acceptance |
| `ShippingTest` cases | `Test` in the `win-shipping-test` test preset ([Z2-N-ARCH-231]) |
| Stage 7 catalogue | `Test`; the transport conformance per candidate in the D-SEL-NETTRANSPORT harness; Stage 7 acceptance |
| registration shape, headers, contracts | `HeaderSelfContained`, `ApiContractCheck`, `Libgraph`, `ForbiddenCheck` |
