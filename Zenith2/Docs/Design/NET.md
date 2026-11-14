# NET — multiplayer

## Purpose and scope

### What NET owns

NET owns networked multiplayer in Zenith2 under Z2-D-073: architecture now, a proof in Stage 3, the full feature in Stage 7. Its code is the optional module `ZenithNet` (`MODULE_NAME Net`). It owns:

- **the multiplayer foundations** (Stage 1 architecture, Stage 3 code), elaborating `Zenith2/Docs/Architecture.md` → "Multiplayer foundations":
  - stable network IDs: the `Zenith_NetworkID` layout, peer-index issuance, deterministic allocation, the per-world `Zenith_NetworkIDMap`, and their stability across save, restore and replay;
  - the authority model: what each `Zenith_NetworkAuthority` value means on a server, a client and offline; the declaration rules; the replication table and its validation; the owner peer;
  - fixed ticks: tick-rate agreement between peers and tick-indexed messages;
  - replayable commands: remote input entering a server world only as phase-1 input commands, in the `ZRPL` command encoding;
  - headless execution: the headless server and client compositions;
- **the Stage 3 loopback proof**: the datagram ports, wire format version 1, the session state machines, the input and snapshot paths, the proof assertions, the loopback content in `Games/WorkloadCharacters` and the `Zenith2/Tests/NetLoopback/` suite;
- **the deferred Stage 7 tree**, fully specified now: transport and session, replication, prediction and reconciliation, interest management, serialization, dedicated server builds, network simulation and network diagnostics;
- the `Net` Luau namespace, its four event types, the `Net.SetOwner` command kind, the `net.*` configuration keys and the misuse block ZS3400–ZS3499;
- the authoring check of network components and its cook step; NET's counters, zones, log categories, console commands and frame-dump section.

### Explicit exclusions

| Not NET's | Owner or classification | Basis |
|---|---|---|
| local multiplayer and split-screen (several local players, several player views) | excluded: no requirement beyond the exclusion records | Z2-D-076 |
| the network ID and authority declarations themselves (`Zenith_NetworkID` layout, `Zenith_NetworkAuthority`, `PROPERTY_FLAG_REPLICATED`) | Architecture and ECS declare them; NET cites them verbatim | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations"; `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeInfo header" |
| the fixed-step simulation clock, catch-up and interpolation | JOBS | `Zenith2/Docs/Design/JOBS.md` → "Configuration keys"; Z2-I-JOBS-10 |
| the `ZRPL` replay log, its writer, reader and verifier, input recording | SAVE | `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header" |
| `Zenith_InputCommand`, its quantisation and the phase-1 Input system | PLAT | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| the asynchronous source registry and the module publication sink | SCRIPT, MOD | `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"; `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink" |
| the headless configuration's startup steps, `--headless` and the exit codes | MOD (composition root) | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" |
| OS sockets | PLAT (row NT1) | `Zenith2/Docs/Design/PLAT/Platform.md` → "Development services" |
| identity, entitlements, matchmaking services, presence, friends, leaderboards, voice | ONLINE (identity) or no inventory item | Z2-D-074 |
| trust boundaries and the parser-limit inventory | SEC (NET supplies its rows, row NT4) | `Zenith2/Docs/Architecture.md` → "Security architecture" |
| player slots in gameplay, possession, the request queue | GAME | `Zenith2/Docs/Design/GAME/Framework.md` → "Players and possession" |
| host migration, peer-to-peer topologies, NAT traversal, cross-platform play between Windows and Android builds | no inventory item; every session has one server, every peer runs the same build (the identity check) | Z2-D-073 |

### Inventory items covered

| Item | Name | Classification | Stage | Where |
|---|---|---|---|---|
| Z2-I-NET-01 | stable network IDs | required | S1 (architecture), S3 (code) | `Zenith2/Docs/Design/NET/Foundations.md` → "Network identity" |
| Z2-I-NET-02 | authority model | required | S1, S3 | `Zenith2/Docs/Design/NET/Foundations.md` → "Authority model" |
| Z2-I-NET-03 | fixed ticks | required | S1, S3 | `Zenith2/Docs/Design/NET/Foundations.md` → "Fixed ticks" |
| Z2-I-NET-04 | replayable commands | required | S1, S3 | `Zenith2/Docs/Design/NET/Foundations.md` → "Replayable commands" |
| Z2-I-NET-05 | headless execution | required | S1, S3 | `Zenith2/Docs/Design/NET/Foundations.md` → "Headless execution" |
| Z2-I-NET-06 | loopback proof | required | S3 | `Zenith2/Docs/Design/NET/Loopback.md` |
| Z2-I-NET-07 | transport/session | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Transport and session" |
| Z2-I-NET-08 | replication | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Replication" |
| Z2-I-NET-09 | prediction/reconciliation | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Prediction and reconciliation" |
| Z2-I-NET-10 | interest management | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Interest management" |
| Z2-I-NET-11 | serialization | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Serialization" |
| Z2-I-NET-12 | dedicated server builds | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Dedicated server builds" |
| Z2-I-NET-13 | network simulation | deferred | S7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Network simulation" |
| Z2-I-NET-14 | diagnostics | deferred | S7 (Stage 3 has the proof's diagnostics) | `Zenith2/Docs/Design/NET/Stage7.md` → "Network diagnostics"; § "Diagnostics and profiling" |
| Z2-I-NET-15 | local multiplayer | excluded | — | § "Explicit exclusions" (Z2-D-076) |
| Z2-I-NET-16 | split-screen | excluded | — | § "Explicit exclusions" (Z2-D-076) |

The coverage table of `Zenith2/Docs/Requirements/NET.md` → "Coverage of inventory items" maps each item to its requirements.

### Items of other areas with a NET half

| Item or requirement | Owner | NET's half |
|---|---|---|
| Z2-I-JOBS-10 fixed simulation steps (JOBS requirements cover Z2-I-NET-03 too) | JOBS | the tick-rate agreement and the refusal of external-tick mode for network roles (`Zenith2/Docs/Design/NET/Foundations.md` → "Tick-rate agreement") |
| SAVE input recording and the `ZRPL` command block | SAVE | the network command transport uses the same 18-byte record (`Zenith2/Docs/Design/NET/Foundations.md` → "Command wire encoding") |
| Z2-R-PLAT-142 process watchdog and PLAT's headless mode (cover Z2-I-NET-05) | PLAT | the loopback driver's `--timeout-seconds`; the headless roles |
| AUDIO's headless null device (covers Z2-I-NET-05) | AUDIO | a server game starts its audio module headless |
| Z2-R-GAME-137 player slots and ownership under replication | GAME | Stage 7 peer-to-slot routing (`Zenith2/Docs/Design/NET/Stage7.md` → "Player slots and per-peer input") |
| AI, SCENE and STREAM deferred notes (AI state replication, transform change bits, interest) | AI, SCENE, STREAM | Stage 7 replication reads their replicated properties and change versions; STREAM keeps cells around every peer (row NT16) |

### Section files

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Design/NET/PublicAPI.md` | Scope of this file · Rules across the ZenithNet headers · Header list · Zenith_NetworkID header · Zenith_NetTypes header · Zenith_NetErrors header · Zenith_NetworkIDMap header · Zenith_NetComponents header · Zenith_NetReplicationTable header · Zenith_NetDatagramPort header · Zenith_Module_Net header · Internal declarations · Script-facing API · Event types · Command kind payloads · Configuration keys · Misuse codes |
| `Zenith2/Docs/Design/NET/Foundations.md` | Scope of this file · Network identity · Authority model · Fixed ticks · Replayable commands · Headless execution · Specification confirmation in Stage 1 |
| `Zenith2/Docs/Design/NET/Loopback.md` | Scope of this file · Transport over datagram ports · Wire format version 1 · Session state machines · Input path · Snapshot path · Proof mode · Loopback content · The loopback suite |
| `Zenith2/Docs/Design/NET/Stage7.md` | Scope of this file · Stage 7 architecture overview · Transport and session · Replication · Prediction and reconciliation · Interest management · Serialization · Dedicated server builds · Network simulation · Network diagnostics · Stage 7 state machines · Stage 7 tests |
| `Zenith2/Docs/Design/NET/Tests.md` | Scope of this file · Test targets · Identity tests · Authority tests · Tick and command tests · Headless tests · Wire format and fuzz tests · Session tests · Input path tests · Snapshot path tests · Proof and loopback suite tests · Script surface tests · Diagnostics tests · Stage 7 test catalogue · Gate steps per test group |

### Conventions

"Server", "client" and "offline" are the three `Zenith_NetRole` values; "peer" is a remote process of a session; "session world" is the one world a module instance binds to the session (the startup world at Stage 3); "proxy" is a client entity spawned by NET to show a server entity; "fragment" is one SNAPSHOT message; "block" is one tick's commands inside an INPUT. Tick indices are the session world's; milliseconds are ZenithCore's monotonic clock and are read only outside simulation. Hashes of content are `Zenith_Hash64` (D-SEL-HASH, open, Stage 1); integrity checks are CRC-32C.

### Prior art

<!-- informative -->
The old engine has no networking, so the design follows established practice rather than Zenith code: server-authoritative snapshot replication with client-side prediction of the controlled entity (the Quake 3 and Overwatch models), the redundant-input and sliding-window techniques of Glenn Fiedler's networking articles, smallest-three quaternion compression, and challenge-response handshakes with stateless cookies (netcode.io, DTLS). What Zenith2 adds is that every network input becomes the same recorded input a single-player replay already uses: remote commands are phase-1 input commands, received state is recorded publications, so a multiplayer session replays offline on each side with the single-player determinism guarantee (Z2-D-019).
<!-- /informative -->

## Dependencies

### Libraries NET depends on

<!-- assumption: pending ARCH (cross-doc item X393) -->
`ZenithNet` (module, layer 7) `DEPENDS`: ZenithRuntime, ZenithSave, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithCore (Architecture's current row) and, requested through cross-doc item X393: ZenithPlatform (`Zenith_InputCommand.h`, the datagram socket of row NT1, `FillOSRandom`, the configuration facade), ZenithProfiling (zones, counters, the console and overlay types), ZenithAssets (`Zenith_AssetHandle<Zenith_Prefab>` in `Zenith.Net.Identity` and the proxy table's asset states). Every one is below layer 7, so the graph stays a DAG and the layer stays 7.
<!-- /assumption -->

| Library | Kind | Layer | Third party |
|---|---|---|---|
| ZenithNet | module (`KIND MODULE`, `MODULE_NAME Net`) | 7 | none at Stage 3; the D-SEL-NETTRANSPORT (open, Stage 7) and D-NET-SESSION-AUTH (open, Stage 7) rulings may admit one each, under `Internal/` only |

### Headings NET relies on

| Need | Source |
|---|---|
| network ID, authority, `ZRPL`, headless, the loopback proof's statement | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations"; `Zenith2/Docs/Architecture.md` → "The Stage 3 loopback proof" |
| module entry points, contexts, frame listeners, engine epilogue stages | `Zenith2/Docs/Architecture.md` → "Engine modules and game projects"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"; `Zenith2/Docs/Design/MOD.md` → "Zenith_Module.h" |
| the publication sink, prologue allocation, replay behaviour | `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Zenith_ModulePublication header", "Allocation sites and acceptance", "Replay behaviour of the sink" |
| the asynchronous source registry | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values" |
| phases, sync points, authority, commands | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases", "Per-field authority", "Commands" |
| systems, queries, change filters, random access | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header", "Zenith_SystemContext header"; `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries", "Resolve and random access" |
| reflection metadata, field sets | `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeInfo header", "Binary field-set record" |
| lifecycle listener, prefab instantiation | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Listener interface"; `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header" |
| the replay log, its reader and verifier | `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header"; `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay verification" |
| the save participant | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header"; `Zenith2/Docs/Design/SAVE/Formats.md` → "Participant section framing", "CRC-32C" |
| input commands, the Input phase system, the input script | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system", "Input commands and quantisation", "The input-script format" |
| development services, platform modes, configuration keys | `Zenith2/Docs/Design/PLAT/Platform.md` → "Development services"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Key grammar", "Registration API" |
| deterministic maths, hashing, randomness, containers, clocks, errors | `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions", "Zenith_Hash64", "Zenith_RandomSource"; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer", "Sorting and searching"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks", "Results and the error taxonomy", "Configuration store" |
| physics determinism (the server's physics state the capture reads) | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Determinism rules", "Determinism boundaries" |
| gameplay players, possession, bindings | `Zenith2/Docs/Design/GAME/Framework.md` → "Players and possession"; `Zenith2/Docs/Design/GAME/Scripting.md` → "Binding functions" |
| the authority census | `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format", "Census rows added by subsequent designs" |
| test processes, integration suites | `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes"; `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites" |
| console, overlay, frame dump | `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry", "DEVT frame dump"; `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers"; `Zenith2/Docs/Design/DEVT/Profiler.md` → "Counter registry" |
| the authoring library and cook steps | `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions" |
| account identity (Stage 7 session authentication candidate 1) | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity model" |

### Areas NET depends on

ARCH (foundations text), ECS (reflection, systems, change versions), SCRIPT (source registry), MOD (module contexts, sink, frame loop), SAVE (replay log, participants, CRC-32C), PLAT (input commands, sockets, platform modes, configuration), SCENE (prefab instantiation, transforms, the lifecycle listener), GAME (possession, census), JOBS (clock), CORE, PHYS (determinism of the state replicated), DEVT (console, overlay, dump, log sink), EDIT and ASSET (authoring library, cook steps), SHIP (the test registry, CMake), ANIM (the WorkloadCharacters game), SEC (parser limits, trust boundary), ONLINE and STREAM (Stage 7).

### Areas that consume NET

GAME (Stage 7 player slots; the loopback scripts call GAME's bindings), every game's scripts (the `Net` namespace), SAVE and the replay tools (NET's participant section and publications in logs), DEVT (NET's panel and dump), PROC (the Shipping symbol list entry and the Stage 7 server gate step), Stage 3 and Stage 7 acceptance.

### Interfaces NET declares and implements

- NET declares `Zenith_NetDatagramPort` (implemented by its own platform adapter, its memory port and, at Stage 7, its simulated port) and, at Stage 7, `Zenith_NetTransportBackend`; both sit inside ZenithNet, so no lower library needs them.
- NET implements interfaces lower libraries declare: `Zenith_ModuleInstance` and `Zenith_ModuleFrameListener` (ZenithRuntime), `Zenith_ModulePublicationHandler` (ZenithRuntime), `Zenith_LifecycleListener` (ZenithScene), `Zenith_SaveParticipant` (ZenithSave) and PLAT's `Zenith_InputCommandBridge` (assumption NT3). Every callback points downward (`Zenith2/Docs/Contracts.md` → "Layering DAG").

### Asynchronous source and misuse block

NET publishes into worlds through one asynchronous source kind, `ASYNC_SOURCE_NETWORK`, requested from SCRIPT through row NT2 and cross-doc item X392 and assigned value 15 by ruling ASYNC-SOURCE-15-16 (which also raises the registry's capacity to 32, SCRIPT's edit); NET's code names the enumerator alone, writes no number of its own and hard-codes no capacity ([Z2-N-SCRIPT-925]). The kind is sink-declared by NET per session world, site `MODULE_SOURCE_SITE_PROLOGUE`, every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most 1,024 bytes, never allocated under `--replay` (`Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication"). Its capacity column for the registry: `net.client.maxFragmentsInFlight` (256) on a client, 64 on a server. Every completion ID NET allocates is parked exactly once (ruling X255, [Z2-N-SCRIPT-931]): an allocation that succeeds is followed in the same prologue call by its `Park`. The misuse block is ZS3400–ZS3499, reserved under ruling MISUSE-BLOCKS-NEW-AREAS-3 (SCOPE holds ZS3200–ZS3299; SEC, PROC and MAINT register no block) and recorded by cross-doc item X391; codes ZS3401–ZS3404 are in use.

### Security assumptions

SEC is being drafted concurrently; NET states what it relies on and supplies its parser limits (row NT4):

| # | Assumption | NET's mechanism |
|---|---|---|
| S1 | every datagram, publication payload and save section NET reads is untrusted | validation in a fixed order, overflow-checked, never an assert; a fuzz target per parser (`Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order") |
| S2 | Stage 3 exposes no network listener outside the machine | the platform port binds `127.0.0.1` alone and refuses non-loopback destinations; the transport is absent from Shipping (`Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles") |
| S3 | Stage 3 sessions are unauthenticated | acceptable on the loopback interface; Stage 7 Shipping sessions are authenticated and encrypted (`Zenith2/Docs/Design/NET/Stage7.md` → "Session security") |
| S4 | a client cannot change server state except through input commands | no wire format version 1 message carries state from a client; owner-client proposals (Stage 7) pass a validator |
| S5 | no personal data in datagrams, logs or replays | NET sends no account identifier at Stage 3; session IDs and nonces are random values |
| S6 | a malicious peer cannot exhaust the server's memory | every buffer is fixed at start (§ "Memory sizing"); the command window, map and rings are bounded with declared full policies |

### Assumed interfaces pending other areas

<!-- assumption: pending PLAT, SCRIPT, MOD, JOBS, SCENE, ECS, DEVT, SEC, PROC, ANIM, SHIP, GAME, ONLINE, ARCH -->
| Interface or change | Owner | NET's use | Row |
|---|---|---|---|
| (a) `Zenith_DatagramSocket` (`ZenithPlatform/Zenith_DatagramSocket.h`): open, send, non-blocking receive, loopback-only flag, buffer sizes, Shipping-capable; (b) its error codes; (c) `Zenith_Platform::FillOSRandom(Zenith_MutableByteSpan)` (also requested by ONLINE, row ON19); (d) Stage 7: any interface and IPv6 | PLAT | the production port, nonces, session IDs, the Stage 7 server secret | NT1 |
| a second allow record in SEC's `confined_apis.json` for `socket`/`bind` under PLAT's `ZenithPlatform/Internal/<Platform>/` socket implementation (`Zenith2/Docs/Design/SEC/PathTraversal.md`; a protected gate file, so a `needs-human` PROC edit), so `ForbiddenCheck` passes when the socket lands | SEC, PROC | the production port | NT1 |
| `Zenith_ModuleStartContext::IsReplaying()` (ONLINE's pending request, cross-doc item X325) | MOD | the replay rule of § "Module start" | X325 |
| `ASYNC_SOURCE_NETWORK` assigned in the registry, with its capacity column and its site (prologue, recorded only) | SCRIPT | every publication | NT2 |
| `Zenith_InputCommandBridge` in `Zenith_InputCommand.h` and its two calls in the Input phase system (`ReplacesLocalInput()`, true on NET's server world and false on its client world, decides whether the mapper's local slice is emptied; `ProvideTickCommands` live only; `ObserveTickCommands` always); the per-slot sequence documented as strictly increasing and gapless from a first value of 0 | PLAT | remote commands, client capture, the command window's baseline | NT3 |
| NET's parser limits joining SEC's inventory and the trust boundary "network": PI-NET-001 replaced by five entries (packet, snapshot fragment, INPUT, publication payload, identity section) naming the stems `Zenith_Fuzz_Net{Packet,SnapshotFragment,Input,Publication,IdentitySection}`, the requested `NetDatagram` stem dropped | SEC | the limits of `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits" and Stage 7's | NT4 |
| `Zenith_WorldAttachContext::SetInputCommandBridge` | MOD | registering the bridge | NT5 |
| `Zenith_ModuleStartContext::GetTestAssertionSink` and `Zenith_TestAssertionSink::RecordFailure`, a module-visible wrapper forwarding to `Zenith_Runtime::RecordTestAssertionFailure()`, safe from `~Zenith_NetModuleInstance`, with the exit code read after shutdown step 6 (MOD to confirm the timing) | MOD | the proof assertions | NT6 |
| headless real-time frame pacing and `Zenith_SimulationClock::GetMicrosecondsUntilNextTick` | MOD, JOBS | servers and clients do not spin | NT7 |
| `PROPERTY_FLAG_REPLICATED` on `Zenith.LocalTransform` properties 1 Position and 2 Rotation | SCENE | the loopback replicates transforms | NT8 |
| `Zenith_NetTransport::Create` in `shipping_forbidden_symbols.json` (a protected gate file); SEC's listener-import scan (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Listener-import scan") told that a Stage 3 Shipping executable links no `bind` import and that Stage 7 re-rules it | PROC, SEC | the Stage 3 Shipping guard's evidence | NT9 |
| the loopback content files inside `Games/WorkloadCharacters` | ANIM | the proof's content | NT10 |
| four entries in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`: the two loopback CTest entries running the pwsh driver, each `RUN_SERIAL TRUE`, with its own `-OutDir …/<config>/<Case>` and `FIXTURES_REQUIRED` on the cook fixture of `WorkloadCharacters`; and the two `cmake -P` source checks `CMake_Net_NoThreadOrMutexInSources` and `CMake_Net_PhaseSystemsReadNoClockOrEnvironment`, registered through SHIP's `zenith_register_script_test` | MOD (owner of the registry), SHIP (consumer) | the loopback suite, the source checks | NT11 |
| `SetProperty` (and command playback of it) stores a `PROPERTY_TYPE_VEC3` or `PROPERTY_TYPE_QUAT` value of `Zenith.LocalTransform` verbatim, normalising nothing | ECS, SCENE | the proof's C1 readback | NT25 |
| a record logged by a module instance's destructor during shutdown is in the chunk the log file sink's `Detach` hands over (`Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown", X245 (e); `Test_LogFileSink_NoWriteAfterDetach`) | DEVT | the capture-chain line | NT26 |
| NET's census rows reviewed in the D-PHASES ruling; the Stage 7 slot mapping of Z2-R-GAME-137 | GAME | authority census, Stage 7 | NT12 |
| ZenithNet's `DEPENDS` widening; the parser-limit rows; the loopback proof's command lines and strengthened in-process check; the four statements of what phase 11 writes admitting a phase-11-owned module resource; the four open decisions; the pending budget rows; the frame-loop and module-interface members | ARCH, CONTRACT, BUDGET, DecisionLog | layout, limits, the proof text, the capture, decisions, budgets | cross-doc items X391 to X399 |
<!-- /assumption -->

## Library and file layout

### Targets

| Target | Kind | Directory | `ZENITH_SUPPORTED` | Stage |
|---|---|---|---|---|
| `ZenithNet` | `KIND MODULE`, `MODULE_NAME Net` | `Zenith2/Engine/ZenithNet/` | `ZENITH_GPU` (the presets of ZenithRuntime and every module, `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix") | S3 |
| `ZenithNet_Tests`, `Zenith_Fuzz_Net*` | tests | `Zenith2/Engine/ZenithNet/Tests/` | as `Zenith2/Docs/Design/NET/Tests.md` → "Test targets" | S3 |
| `Zenith_Integration_NetLoopback` | integration suite | `Zenith2/Tests/NetLoopback/` | `ZENITH_WINDOWS` | S3 |
| `<Name>Server` | game executable variant (Stage 7) | created by `zenith_add_game` | per D-NET-SERVER-PLATFORM (open, Stage 7) | S7 |

The library's `CMakeLists.txt` is the one line `zenith_add_library(ZenithNet KIND MODULE MODULE_NAME Net DEPENDS <the list of § "Libraries NET depends on"> SUPPORTED ZENITH_GPU TEST_DEPENDS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT)`. No source is excluded per configuration ([Z2-N-CONTRACT-442]); the Shipping guard is an `if constexpr`, never a build-exclude.

### ZenithNet public headers

The eight Stage 3 root headers and the three Stage 7 ones of `Zenith2/Docs/Design/NET/PublicAPI.md` → "Header list".

### ZenithNet internal files

| File | Contents |
|---|---|
| `Internal/Zenith_NetModule.cpp` | the two entry points; § "Module start" |
| `Internal/Zenith_NetModuleInstance.h`, `.cpp` | attach, detach, the frame listener (`Net.Pump`), the publication handler, shutdown |
| `Internal/Zenith_NetModuleConfig.h`, `.cpp` | key registration and parsing (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Configuration keys") |
| `Internal/Zenith_NetWorldState.h`, `.cpp` | per-world state, the resources' registration |
| `Internal/Zenith_NetworkIDMap.cpp`, `Internal/Zenith_NetworkID.cpp` | the map; address and string functions |
| `Internal/Zenith_NetIdentityIndex.cpp` | `Net.IdentityIndex`, allocation, duplicates |
| `Internal/Zenith_NetLifecycleListener.h`, `.cpp` | destroy-pending handling |
| `Internal/Zenith_NetSaveParticipant.h`, `.cpp` | the `Zenith.Net.Identity` participant |
| `Internal/Zenith_NetReplicationTable.cpp` | `Build`, diagnostics, the hash, the `net.authority` report |
| `Internal/Zenith_NetCommands.cpp` | `Net.SetOwner` registration, recording helper, playback |
| `Internal/Zenith_NetCommandSource.h`, `.cpp` | the input command bridge (server window release, client capture) |
| `Internal/Zenith_NetWire.h`, `Internal/Zenith_NetWire.cpp` | packet framing, every message codec, ULEB128 |
| `Internal/Zenith_NetQuantise.h`, `.cpp` | the two proof codecs |
| `Internal/Zenith_NetTransport.h`, `.cpp` | sessions, timers, sliding windows, send queue |
| `Internal/Zenith_NetPlatformPort.cpp`, `Internal/Zenith_NetMemoryPort.cpp` | the two ports |
| `Internal/Zenith_NetSession.cpp` | `Net.Session`: session inbox to events, the proxy table, proof counters |
| `Internal/Zenith_NetCapture.cpp` | `Net.Capture`, fragment encoding, the capture ring |
| `Internal/Zenith_NetClientApply.cpp` | `Net.ClientApply`, the pending-spawn table, apply verification |
| `Internal/Zenith_NetCollect.cpp` | `Net.Collect@W<tag>` and `Net.Send` |
| `Internal/Zenith_NetBindings.cpp`, `Internal/Zenith_NetEvents.cpp` | the `Net` namespace, the event types |
| `Internal/Zenith_NetProof.h`, `.cpp` | proof assertions and outcome |
| `Internal/Zenith_NetDevTools.cpp` | counters, console commands, the overlay rows, the frame-dump section (registered only when the start context returns them) |

### ZenithNet platform files

None. ZenithNet has no `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/` directory: every OS call is PLAT's (row NT1), so no NET file includes a platform header (rule F18) and `CompileCoverage` builds every NET source in every supported preset.

### ZenithNet test files

`Zenith2/Engine/ZenithNet/Tests/`: `Zenith_NetIdentity_Tests.cpp`, `Zenith_NetworkIDMap_Tests.cpp`, `Zenith_NetIdentitySave_Tests.cpp`, `Zenith_NetAuthority_Tests.cpp`, `Zenith_NetModule_Tests.cpp`, `Zenith_NetCommands_Tests.cpp`, `Zenith_NetWire_Tests.cpp`, `Zenith_NetQuantise_Tests.cpp`, `Zenith_NetSession_Tests.cpp`, `Zenith_NetInput_Tests.cpp`, `Zenith_NetSnapshot_Tests.cpp`, `Zenith_NetClient_Tests.cpp`, `Zenith_NetPublication_Tests.cpp`, `Zenith_NetProof_Tests.cpp`, `Zenith_NetBindings_Tests.cpp`, `Zenith_NetEvents_Tests.cpp`, `Zenith_NetDiagnostics_Tests.cpp`; `Tests/Support/Zenith_NetTestWorld.h`, `Zenith_NetTestPeers.h`, `Zenith_NetWireBuilders.h` (header-only, ruling X184); `Tests/Fuzz/Zenith_Fuzz_NetPacket.cpp`, `Zenith_Fuzz_NetSnapshotFragment.cpp`, `Zenith_Fuzz_NetInput.cpp`, `Zenith_Fuzz_NetPublication.cpp`, `Zenith_Fuzz_NetIdentitySection.cpp`; `Tests/Examples/Zenith_NetworkIDMap_Example.cpp` (the map's compiled example). No binary fixture is committed (ruling Q55): datagrams, logs and saves are built in the tests.

### Integration suite files

`Zenith2/Tests/NetLoopback/`: `CMakeLists.txt` (one `zenith_add_integration_tests` call), `Zenith_NetLoopbackVerify_Tests.cpp`, `Zenith_NetLoopbackRender_Tests.cpp` (the `needs-gpu` case), `Run-NetLoopback.ps1` (the driver, `Zenith2/Docs/Design/NET/Loopback.md` → "Driver script").

### Tool-side files

| File | Owner rule |
|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringNet.h`, `Zenith2/Tools/Authoring/Internal/Net/Zenith_AuthoringNetCheck.cpp` | `ZenithAuthoring`'s NET owner directory (ruling Q84 (a); `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"): `Zenith_ValidateNetDocument` (§ "Cook steps"); its move appends `ZenithNet` to `ZenithAuthoring`'s `DEPENDS` |
| `Zenith2/Tools/Authoring/Tests/Net/Zenith_AuthoringNet_Tests.cpp` | `ZenithAuthoring_Tests` |
| `Zenith2/Tools/ZenithCook/Internal/Net/Zenith_NetCookSteps.h`, `.cpp` | registers the step `net.validate` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions") |
| `Zenith2/Tools/ZenithCook/Tests/Zenith_NetCook_Tests.cpp` | `ZenithCook_Tests` |

### Game content files

`Games/WorkloadCharacters/Scenes/NetLoopback.scene.json`, `Assets/Prefabs/CharacterProxy.prefab.json`, `Scripts/Net/LoopbackDirector.luau`, `Tests/NetLoopback.input.json`, and the `Zenith.Net.Identity` component added to the existing character prefab (`Zenith2/Docs/Design/NET/Loopback.md` → "Loopback content"; row NT10).

### Stage 7 files

`Internal/Transport/` (the backend adapter of the selected library, the reliability layer of candidate 1, the handshake v2, rate limiting), `Internal/Replication/` (`Net.Replicate`, baselines, priority, proposals, broadcast), `Internal/Prediction/` (`Net.Predict`, history, smoothing, interpolation data), `Internal/Interest/` (grid, relevancy sets, streaming viewers), `Internal/Serialization/` (bit stream, codecs, string and asset tables, wire version 2), `Internal/Simulation/` (`Zenith_NetSimulatedPort`), `Internal/Diagnostics/` (statistics, panel, capture, debug views), with tests under `Tests/Stage7/` and `Zenith2/Tests/NetPrediction/`, `Zenith2/Tests/NetStreaming/` (`Zenith2/Docs/Design/NET/Stage7.md`).

## Public API

### API summary

| Group | Declarations | File |
|---|---|---|
| identity | `Zenith_NetworkID`, its helpers, `Zenith_NetworkIDMap` | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkID header", "Zenith_NetworkIDMap header" |
| shared values | roles, session states, reject and disconnect reasons, addresses, session info, limits | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetTypes header" |
| errors | `Zenith_NetErrorCode`, `Zenith_NetError` | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetErrors header" |
| components and events | `Zenith.Net.Identity`, `Zenith.Net.ProxyPrefabSet`, four events, the `Net.SetOwner` payload | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetComponents header" |
| authority | the replication table and its diagnostics | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetReplicationTable header" |
| ports | `Zenith_NetDatagramPort`, the memory port pair | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetDatagramPort header" |
| module | the two entry points | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_Module_Net header" |
| Stage 7 | transport backend, simulated port, statistics | `Zenith2/Docs/Design/NET/Stage7.md` |

### Handle kinds used

| Kind | Use |
|---|---|
| world handle (entity) | the map's values; command targets; event fields; never serialized by NET |
| shared asset handle | `Zenith.Net.Identity` property 2 and the proxy set; sent as a proxy-table index |
| persistent ID | the proxy table hash input; never used as an entity's network identity |
| operation handle | none at Stage 3 |
| network ID | NET's own identity, a plain value, not a handle kind ([Z2-N-CONTRACT-068]) |

### Rules across the NET API

`Zenith2/Docs/Design/NET/PublicAPI.md` → "Rules across the ZenithNet headers" applies to every public declaration; the ones that matter most to callers: every fallible function returns a `Zenith_Result` with `Zenith_NetError`, received bytes never reach an assert, and no handle crosses a process boundary.

### Module start

`Zenith_Module_Net_Start(xContext)`, startup step 15, main thread, in this order; a failing step logs its cause, unwinds what the previous steps built (the registrations are undone by the runtime's journal, bullet 851) and returns its error:

First, `bReplay = xContext.IsReplaying()` (ONLINE's pending member, cross-doc item X325, listed in § "Assumed interfaces pending other areas"). Under `--replay` the live-only checks of steps 1, 3 and 7 are skipped and no transport exists; `net.role` comes from the log header (simulation-affecting), and a replay needs no `net.connect`.

1. Parse the `net.*` keys from `xContext.GetConfig()` into `Zenith_NetModuleConfig`; a value outside its range is `MODULE_ERROR_REGISTRATION` with `net: <key>=<value>: <reason>`; when not `bReplay`, an empty `net.connect` for a client or an unparsable address is the same error.
2. Shipping: a server or client role is forced offline (`Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles"); `net.proof.mode` is forced false.
3. For a server or client when not `bReplay`: refuse `sim.ticksPerFrame` ≠ 0 and, for a server, `sim.maxCatchUpTicks` > 8 (`Zenith2/Docs/Design/NET/Foundations.md` → "Tick-rate agreement"). The default of `sim.maxCatchUpTicks` is the pending row Z2-B-JOBS-002, so a server's command line sets the key (the loopback driver passes 8).
4. Build the replication table (`Zenith2/Docs/Design/NET/Foundations.md` → "Authority validation at module start").
5. Register the `Net.SetOwner` command kind (`RegisterCommandKind`), the script bindings (`RegisterScriptBindings`), the frame listener (`RegisterFrameListener`), the engine epilogue stage `Net.Send` (`AddEngineEpilogueStage`, after world stage `Net.Collect`, writing `Zenith.Net.Transport`), and, when the start context returns them, the console commands, the overlay rows and the frame-dump section.
6. Take the test assertion sink (assumption NT6) when `net.proof.mode`.
7. For a server or client when not `bReplay`: create the transport over a production port; a server binds `net.bindAddress:net.port` here (`NET_ERROR_BIND_FAILED` is logged with the OS code and is `MODULE_ERROR_DEVICE`, exit code 4); a client's transport opens its ephemeral loopback port at `Start`, in the first frame prologue after its session world's first tick (`Zenith2/Docs/Design/NET/Loopback.md` → "Client session states").
8. Return the instance.

`Zenith_NetModuleInstance::Create`'s construction steps each have a failure-injection test (`Test_NetModuleInstance_Create_FailsAt<Step>_Unwinds`, [Z2-N-CONTRACT-341]; steps Config, Table, Transport, Port), and `Test_NetHeadless_ReplayOfClientLogStartsWithoutConnect` holds the replay rule.

### World attach and detach

`AttachWorld(xContext)` for every world of purpose `WORLD_PURPOSE_PLAY` (edit and preview worlds get the registrations of steps 1–3 only, role offline):

1. Create the world's `Zenith_NetWorldState` and its map (capacity `net.maxNetworkedEntities`); `AddWorldResource` of `Zenith.Net.WorldState` (owning phase Input) and, for a server, `Zenith.Net.Capture` (owning phase SaveSnapshot).
2. Register `Net.IdentityIndex`; the lifecycle listener (interest `LIFECYCLE_EVENT_DESTROY_PENDING`); the save participant.
3. Register `Net.Session` (phase 1, after `Net.IdentityIndex`).
4. If no session world is bound yet and the role is server or client: bind this world (the startup world at Stage 3); `DeclareSource(ASYNC_SOURCE_NETWORK, …)` and `SetSourceHandler` on its publication sink; `SetInputCommandBridge` (assumption NT5); a client registers `Net.ClientApply`, a server `Net.Capture`; both register the epilogue stage `Net.Collect`.
5. Every other world of a networked process runs offline (IDs only).

`DetachWorld(uTag)`: a session world's detach ends the session (`WORLD_TRANSITION` for a swap, nothing sent at shutdown, which the instance's destructor handles); from then on a server answers every request with `NOT_ACCEPTING`, the reason's whole meaning (no session world attached); every completion ID already allocated was parked in its prologue, so no ID is outstanding ([Z2-N-SCRIPT-931]); destroy the world state. Registrations are undone by the runtime.

### Script-facing API

The `Net` namespace, nine functions: `Zenith2/Docs/Design/NET/PublicAPI.md` → "Script-facing API".

### Events

Four world-level event types: `Zenith2/Docs/Design/NET/PublicAPI.md` → "Event types".

### Command kinds

`Net.SetOwner`: `Zenith2/Docs/Design/NET/PublicAPI.md` → "Command kind payloads"; playback `Zenith2/Docs/Design/NET/Foundations.md` → "Ownership and the owner peer".

### Configuration keys

Nineteen Stage 3 keys: `Zenith2/Docs/Design/NET/PublicAPI.md` → "Configuration keys"; the Stage 7 keys (`net.peerBandwidthKiBps`, `net.interest.*`, `net.server.baselineBytesPerPeer`, `net.client.predictionHistoryBytes`, `net.sim.*`) are registered by their Stage 7 tickets with the ranges of `Zenith2/Docs/Design/NET/Stage7.md`.

## Data structures and formats

### In-memory structures

| Structure | Size | Where |
|---|---|---|
| `Zenith_NetworkID` | 8 bytes | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkID header" |
| `Zenith_NetIdentityComponent` | 32 bytes, alignment 8 | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetComponents header" |
| `Zenith_NetProxyPrefabSetComponent` | 256 bytes | same |
| `Zenith_NetSetOwnerRequest` | 24 bytes | same |
| `Zenith_NetReplicationEntry` | 24 bytes | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetReplicationTable header" |
| `Zenith_NetIDAllocator` | 16 bytes | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Internal declarations" |
| map slot | 16 bytes | `Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetworkIDMap header" |
| server command window | 128 tick slots + an 8,192-entry sequence heap of 24-byte entries | `Zenith2/Docs/Design/NET/Loopback.md` → "Server command intake" |
| capture ring | 8 × 32 × 1,020 bytes + 8 descriptors | `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point" |
| pending-destroy entry | 24 bytes (ID, destroy tick, remaining captures) | `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID map" |
| client tombstone | 16 bytes (ID, fragment tick) | `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply" |

### Formats summary

| Format | Versioned by | Specified in |
|---|---|---|
| wire format version 1 (`ZNP1`) | the header's protocol version | `Zenith2/Docs/Design/NET/Loopback.md` → "Wire format version 1" |
| NET publication payload (snapshot fragment, session event) | the payload's record version byte | `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication" |
| `Zenith.Net.Identity` save section, version 1 | the participant's section version | `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore" |
| wire format version 2 (`ZNP2`, Stage 7) | the header's protocol version | `Zenith2/Docs/Design/NET/Stage7.md` → "Wire format version 2" |
| packet capture `.znetcap` (Stage 7) | its header version | `Zenith2/Docs/Design/NET/Stage7.md` → "Packet capture" |
| components in authoring documents | ECS's JSON field-set form | `Zenith2/Docs/Design/ECS/Reflect.md` → "JSON field-set form" |

### Memory sizing

| Item | Per | Bytes at the defaults |
|---|---|---|
| map (65,536 entities, 131,072 slots × 16) | world | 2,097,152 |
| pending-destroy list (65,536 × 24) | server session world | 1,572,864 |
| server command window (128 slots reserved up to 64 commands each × 20 = 163,840; heap 8,192 × 24 = 196,608; burst arena 4,096 × 20 = 81,920) | server session world | 442,368 |
| capture ring (8 × 32 × 1,020) + 8 descriptors of 80 bytes (tick, digest, count, 32 body lengths) | server session world | 261,760 |
| send slot (the copy `Net.Collect` makes of up to eight captures, 8 × 32 × 1,020) | server session world | 261,120 |
| client fragment inbox (256 × 1,024) | client session world | 262,144 (the sink's result arena is SCRIPT's and the same size) |
| client outbound ring (67 × 64 × 20 = 85,760) + burst arena (81,920) | client session world | 167,680 |
| client tombstones (16,384 × 16) | client session world | 262,144 |
| client applied-tick table (65,536 × 16) | client session world | 1,048,576 |
| client proof record (512 entities × 32 entries × 28 bytes; proof mode only) | client session world | 458,752 |
| transport (`Zenith.Net.Transport`: receive buffer 1,200; 512 queued datagrams × 1,200) | process | 615,600 |

At the defaults a server process holds 5,250,864 bytes (5.01 MiB), a client 4,912,048 bytes (4.68 MiB; 4.25 MiB outside proof mode), and an offline world 2,097,152 bytes (2.00 MiB, the map alone); `net.maxNetworkedEntities` scales the map, the pending-destroy list and the applied-tick table. All of it is reserved at start or attach, so no NET path allocates during a frame. The governor sees it through the module's `Zenith_MemoryConsumer` registration, whose shedding at every pressure level is none (the memory is fixed and simulation-relevant).

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| network ID allocation and the identity index pass | `Zenith2/Docs/Design/NET/Foundations.md` → "Network ID allocation", "Identity index system" |
| duplicate handling | `Zenith2/Docs/Design/NET/Foundations.md` → "Duplicate and unknown IDs" |
| identity restore | `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore" |
| replication table construction | `Zenith2/Docs/Design/NET/Foundations.md` → "Replication table construction" |
| packet validation | `Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order" |
| target tick mapping | `Zenith2/Docs/Design/NET/Loopback.md` → "Target tick mapping" |
| command window release | `Zenith2/Docs/Design/NET/Loopback.md` → "Server command window" |
| capture, quantisation, fragment encoding | `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point", "Quantisation", "Fragment encoding" |
| client apply | `Zenith2/Docs/Design/NET/Loopback.md` → "Client apply" |
| acknowledgement and digest verification | `Zenith2/Docs/Design/NET/Loopback.md` → "Acknowledgements and digests" |
| Stage 7 algorithms (reliability, delta, priority, reconciliation, interest, codecs, simulation) | `Zenith2/Docs/Design/NET/Stage7.md` |

### State machine index

| State machine | Specified in |
|---|---|
| client session (Stage 3) | `Zenith2/Docs/Design/NET/Loopback.md` → "Client session states" |
| server peer slot (Stage 3) | `Zenith2/Docs/Design/NET/Loopback.md` → "Server peer states" |
| client session, server peer slot, proxy, predicted entity (Stage 7) | `Zenith2/Docs/Design/NET/Stage7.md` → "Stage 7 state machines" |

### Frame-graph placement

| Node | Kind and placement | Affinity | Declared access | Roles |
|---|---|---|---|---|
| `Net.Pump` | the frame listener's `OnPlatformEvents`, frame-prologue step 1 (not a node) | main | none (module memory, the sink's prologue site; between frames it reads the session world state's proxy-table flag and restore mark, and the asset registry for the proxies' residency) | server, client (live only) |
| `Net.IdentityIndex` | system, phase 1 | any | writes `Zenith.Net.Identity` property 1 and `Zenith.Net.WorldState`; reads property 4; emits commands (the re-key of a duplicate) | every play world |
| `Net.Session` | system, phase 1, after `Net.IdentityIndex` | any | reads `Zenith.Net.ProxyPrefabSet`; writes `Zenith.Net.WorldState` (the session state from recorded events, the proxy table and its validity flag); emits the session events; reads no asset state and touches no transport | every play world |
| `Net.ClientApply` | system, phase 1, after `Net.Session` | any | reads `Zenith.Net.Identity` and the table entries (any row); writes `Zenith.Net.WorldState`; emits commands and `Net.ControlledEntityChanged` | client session world |
| PLAT's `Input.Actions` | system, phase 1 | any | PLAT's declaration; calls NET's bridge | every world with input |
| `Net.Capture` | system, phase 11 | any | reads `Zenith.Net.Identity`, the table entries (any row), `Zenith.Net.WorldState`; writes `Zenith.Net.Capture` | server session world |
| `Net.Collect@W<tag>` | epilogue stage of the session world | any | reads `Zenith.Net.Capture` (server) or `Zenith.Net.WorldState` (client) | session world |
| `Net.Send` | engine-wide EPILOGUE stage, group 0, after every `Net.Collect@W*` | any | writes the module's world-independent resource `Zenith.Net.Transport`, registered by `RegisterTypes` ([Z2-N-ARCH-442]) | process |
| NET's publication handler | tick-boundary step 1 | the boundary node's | module memory (the inboxes) | session world |

### Tick walkthrough server

Frame k, one tick t: prologue step 1 receives the peer's INPUTs (window insertion, every reported digest pair verified) and any join or leave (session publications parked); the tick boundary publishes them; phase 1 runs `Net.IdentityIndex` (new characters get IDs), `Net.Session` (PEER_JOINED becomes `Net.PeerJoined`), and `Input.Actions`, which takes the window's release for t through the bridge, applies and logs it; phases 2–10 simulate; phase 11's `Net.Capture` encodes the fragments of t (and, in proof mode, decodes and checks them, P6); the epilogue's `Net.Collect` and `Net.Send` send them.

### Tick walkthrough client

Frame k: prologue step 1 receives SNAPSHOT fragments, records their bitmap and the digest of every tick it completes, allocates and parks one publication each; the tick boundary of tick c publishes them in arrival order into the inbox; phase 1 runs `Net.IdentityIndex` (indexes last tick's proxy spawns), `Net.Session`, `Input.Actions` (local commands; the bridge copies them to the outbound ring), and `Net.ClientApply` (proof check C1, then the inbox's destroys, spawns and updates as commands); the sync point applies them; the epilogue sends the INPUT with tick c's block, three redundant blocks, the session base sequence and the digest ring's (tick, digest) pairs.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_NetworkID`, `Zenith_NetAddress`, `Zenith_NetSessionInfo`, the event and payload structs | `IMMUTABLE` |
| `Zenith_NetworkIDMap` | `PHASE(Input)` for writers, readers by declared resource access |
| `Zenith_NetReplicationTable` | `IMMUTABLE` after `Build` |
| `Zenith_NetDatagramPort`, `Zenith_NetTransport` | `EXTERNALLY_SYNCHRONISED`: the prologue and `Net.Send`, never together |
| `Zenith_NetMemoryPortPair` | `OWNER_THREAD` |
| `Zenith_NetModuleInstance` | `MAIN_THREAD` for attach, detach, the listener and destruction; `OnPublished` on the tick-boundary node |
| `Zenith_NetWorldState` | per member: inboxes written at the boundary and read in phase 1; the window written in the prologue and read on `Input.Actions`; the outbound ring written on `Input.Actions` and read in `Net.Collect`; the capture ring written in phase 11 and read in `Net.Collect` |

### Threads NET owns

None at Stage 3: NET never starts a thread, so it adds no row to `Zenith2/Docs/Contracts.md` → "Shutdown ownership order". At Stage 7 a transport library that runs its own service thread is joined by its backend's destruction in shutdown step 6, and the D-SEL-NETTRANSPORT ruling adds that row in its implementation diff ([Z2-N-CONTRACT-348]).

### Cross-thread hand-offs

Every hand-off is ordered by the frame structure, not by a lock: prologue (main) → `RunFrame` (tick boundary, phases, epilogue on workers) → frame-loop step 3 (main). Data written in the prologue is read inside `RunFrame`; data written inside `RunFrame` is read by `Net.Send` inside the same `RunFrame` after a declared order, or by the next prologue. No two of NET's accesses to one object can overlap, because no NET code runs on two threads at once at Stage 3; the per-world state is covered by the frame graph's hazard ordering of the declared resources, and the module is outside the `linux-tsan` matrix (`ZENITH_GPU`), so its sanitizer coverage is `win-asan`.

### Lock-order entries

None: NET holds no `Zenith_Mutex` at Stage 3 and adds no row to `Zenith2/Docs/Contracts.md` → "Lock ordering table". A Stage 7 lock, if the selected transport needs one, names its row in the ruling's diff ([Z2-N-CONTRACT-320]).

### Startup and shutdown

- Startup: § "Module start" (step 15), § "World attach and detach" (step 16).
- Shutdown step 3 drains the session world's tick; step 6 destroys the module instance: the transport sends DISCONNECT three times 10 ms apart (`SERVER_SHUTDOWN` or `CLIENT_QUIT`), closes the port, and the proof outcome is evaluated and reported through the assertion sink before the instance is freed.
- A process killed without shutdown leaves its peer to time out (`net.peerTimeoutMs`).

### Cancellation

NET issues no operation handle at Stage 3. A session ends by state transition, never by cancellation; parked publications are always published or discarded by the table (a retired world's table discards and counts them, [Z2-N-SCRIPT-931]).

### Error enumerations

`Zenith_NetErrorCode` (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetErrors header"):

| Code | Meaning | Returned by |
|---|---|---|
| `NET_ERROR_INVALID_CONFIG` | a key value out of range or unparsable; an invalid memory-port desc | Start, `Zenith_ParseNetAddress`, `Zenith_CreateNetMemoryPortPair` |
| `NET_ERROR_OUT_OF_MEMORY` | an allocation at start or attach failed | every `Create` |
| `NET_ERROR_SOCKET` | an OS socket call failed (OS code in the record) | ports |
| `NET_ERROR_BIND_FAILED` | the bind address and port are taken or unusable | the platform port |
| `NET_ERROR_ADDRESS_REFUSED` | a non-loopback bind or destination at Stage 3 | the platform port |
| `NET_ERROR_NOT_IN_SHIPPING` | a development-only facility in Shipping (the Stage 3 transport, Stage 7's simulator) | Start (as a note), `Zenith_NetSimulatedPort::Create` |
| `NET_ERROR_REPLAYING` | a transport operation under `--replay` | internal guard |
| `NET_ERROR_MALFORMED_PACKET` | a datagram or payload failed validation | the decoders |
| `NET_ERROR_UNKNOWN_MESSAGE` | a message kind outside 1 … 7 | the packet reader |
| `NET_ERROR_VERSION_MISMATCH` | a different protocol version | the packet reader |
| `NET_ERROR_SESSION_MISMATCH` | a wrong session ID or sender address | the transport |
| `NET_ERROR_LIMIT` | a parser limit exceeded | the decoders, ports |
| `NET_ERROR_IDENTITY_MISMATCH` | build, composition, content, backend or tick rate differ (field in the detail) | the handshake |
| `NET_ERROR_TABLE_MISMATCH` | replication or proxy table hashes differ | the handshake |
| `NET_ERROR_SERVER_FULL` | no free peer slot | the handshake |
| `NET_ERROR_CONNECT_TIMEOUT` | no answer within `net.connectTimeoutMs` | the client session |
| `NET_ERROR_PEER_TIMEOUT` | the peer was silent for `net.peerTimeoutMs` | both sessions |
| `NET_ERROR_REJECTED` | a CONNECT_REJECT arrived (reason in the detail) | the client session |
| `NET_ERROR_NETWORK_ID_EXHAUSTED` | the local counter passed 2^48 − 1 | allocation |
| `NET_ERROR_PEER_INDEX_EXHAUSTED` | the server issued index 65,534 | the handshake |
| `NET_ERROR_DUPLICATE_NETWORK_ID` | a map key already present | `Zenith_NetworkIDMap::Insert` |
| `NET_ERROR_UNKNOWN_NETWORK_ID` | an ID the map does not hold | the client apply, lookups |
| `NET_ERROR_PROXY_NOT_RESIDENT` | a proxy prefab not ACTIVE when the client transport starts | the client session |
| `NET_ERROR_UNENCODABLE_PROPERTY` | a replicated property without a Stage 3 codec (a warning diagnostic) | table construction |
| `NET_ERROR_WRONG_ROLE` | an operation for another role (a client `Net.SetOwner` playback) | command playback |
| `NET_ERROR_PORT_CLOSED` | a port used after `Close` | ports |
| `NET_ERROR_WOULD_BLOCK` | a non-blocking port had nothing to receive or no room to send | ports |
| `NET_ERROR_MAP_FULL` | the map is at capacity | `Zenith_NetworkIDMap::Insert` |
| `NET_ERROR_AUTHORITY` | the replication table has an error diagnostic | `Zenith_NetReplicationTable::Build` |
| codes 32 … 42 | Stage 7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Transport and session error codes" (codes 38–40: prediction history, interest capacity, a dedicated server asked for a device) |

### Reject and disconnect reasons

`Zenith_NetRejectReason` and `Zenith_NetDisconnectReason` are wire values (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Zenith_NetTypes header"); each reaches scripts through `Net.SessionChanged` and `Net.PeerLeft`, and each has its row in `Zenith2/Docs/Design/NET/Loopback.md` → "Handshake rules" and "Session state machines".

### Luau errors versus asserts

Script misuse raises the catalogued ZS3401–ZS3404 errors and never asserts; a binding called with a valid handle of an entity that lost its identity returns `nil` or `false`. Data from the network, a recorded log or a save never reaches an assert ([Z2-N-CONTRACT-472]); `ZENITH_ASSERT` guards only NET's own invariants (a prologue allocation outside the prologue, a write to the map outside its two writers).

### Determinism obligations

- Every NET phase system's result depends only on simulation state, the tick's recorded publications and the tick's commands; never on wall-clock time, arrival timing, worker count, chunk layout or slot index ([Z2-N-ARCH-881], [Z2-N-ARCH-884], [Z2-N-ARCH-885]).
- NET's only paths into a world are PLAT's input commands (server) and recorded `ASYNC_SOURCE_NETWORK` publications (client and server session events), so both logs replay bit-identically (Z2-D-019, Z2-D-053); `Net.SessionState` is a function of the role and those events, so a script that branches on it replays too. A restore is the one other transition, applied identically in a replay that restores the same save.
- The capture's bytes and the client's applied values are pure functions of simulation state, through the `+ − × ÷`, `std::nearbyint` (ties to even) and `std::sqrt` quantisation ([Z2-N-ARCH-888]).
- Tests run the world-level cases with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend ([Z2-N-CONTRACT-356]).

## Diagnostics and profiling

### Counters

Registered in the profiler's counter registry (`Zenith2/Docs/Design/DEVT/Profiler.md` → "Counter registry"); `CUMULATIVE` unless marked `GAUGE`.

| Counter | Meaning |
|---|---|
| `net.packets.sent`, `net.packets.received`, `net.bytes.sent`, `net.bytes.received` | transport traffic |
| `net.packets.malformed`, `net.packets.foreign`, `net.packets.version`, `net.packets.duplicate`, `net.packets.table`, `net.packets.early` | validation drops (`early`: a request before the server's first tick) (`Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order") |
| `net.send.deferred`, `net.send.would_block` | datagrams held to the next frame, or dropped by the OS |
| `net.late_commands` | commands applied after their target tick (Architecture's counter, [Z2-N-ARCH-786]) |
| `net.server.lost_commands`, `net.server.duplicate_commands`, `net.server.commands_too_early`, `net.server.window.dropped` | command window |
| `net.server.snapshot_truncated`, `net.server.unlisted_proxy_prefabs` | capture |
| `net.client.fragments_received`, `net.client.fragments_dropped_full`, `net.client.fragments_stale`, `net.client.fragments_too_old` | client intake |
| `net.client.proxies` (GAUGE), `net.client.proxy_spawns`, `net.client.proxy_destroys`, `net.client.unknown_destroys`, `net.client.proxy_not_resident`, `net.client.spawns_refused_map_full` | proxies |
| `net.ids.assigned`, `net.ids.duplicates`, `net.ids.map_full`, `net.ids.exhausted`, `net.ids.bad_flags`, `net.ids.pending_destroy_dropped` | identity |
| `net.quantise.clamped`, `net.quantise.non_finite` | codecs |
| `net.commands.malformed`, `net.commands.rejected` | `Net.SetOwner` playback |
| `net.publications.malformed` | payloads that failed validation at publication or apply |
| `net.proxy_table.invalid` | proxy table build failures |
| `net.session.state` (GAUGE), `net.session.rtt_ms` (GAUGE), `net.connected_ticks` | session |
| `net.proof.ack_verified`, `net.proof.ack_mismatches`, `net.proof.ack_unverifiable`, `net.proof.apply_mismatches`, `net.proof.capture_mismatches`, `net.proof.digests_unreported` | proof |

Each queue's `.full`, `.dropped` and `.high_water` counters ([Z2-N-CONTRACT-345]) are `net.server.window`, `net.client.inbox` and `net.send.queue`.

### Profiling zones

`Net.Pump`, `Net.IdentityIndex`, `Net.Session`, `Net.ClientApply`, `Net.Capture`, `Net.Collect`, `Net.Send`, and inside them `Net.Pump.Decode`, `Net.Capture.Encode`, `Net.ClientApply.Records`. Every system zone is emitted by the frame graph's trampoline; the listener's zone by NET.

### Log categories and messages

| Category | Messages |
|---|---|
| `net` | start summary `net: role=<role> session world <tag> table <n> entries hash 0x<hex>`; authority diagnostics; duplicates; exhaustion; restore lines |
| `net.session` | `net.session: listening on <address>`, `connected as peer <p> session 0x<hex> offset <o>`, `peer <p> joined`, `peer <p> left (<reason>)`, `rejected: <reason>`, `failed: <reason>` |
| `net.wire` | the first validation failure of each kind per second, with the sender |
| `net.proof` | each failed proof assertion |

### On-screen profiler rows

The overlay's counter panel shows the `net.*` counters under a `Net` group; at Stage 3 the dedicated `Net` panel is a row set: role, state, peer, RTT, packets and bytes per second each way, late and lost commands, proxies (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers"). Stage 7's panel is `Zenith2/Docs/Design/NET/Stage7.md` → "Network panel and console".

### Console commands

Development builds (`Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"): `net.status` (role, state, peer, session ID, offset, counters summary), `net.authority` (`Zenith2/Docs/Design/NET/Foundations.md` → "Network authority report"), `net.ids` (the session world's map size, the allocator, the first 32 IDs). Stage 7 adds its commands.

### Frame dump section

The section `net` of DEVT's frame dump: role, state, the allocator, map count, this frame's received and sent datagrams by kind, the command window's pending count and watermark, the capture ring's last tick and fragment count, the inbox count (`Zenith2/Docs/Design/DEVT/Console.md` → "DEVT frame dump").

### Telemetry

At shutdown NET writes one `net` summary line: connected ticks, packets and bytes each way, late and lost commands, proxies peak, malformed counts. No other telemetry exists at Stage 3; no personal data is ever logged.

## Authoring and cooking

### What NET authors

NET has no document type of its own: its two components are authored in scenes and prefabs through the generic component inspector, in ECS's JSON field-set form. Authoring rules:

- `Zenith.Net.Identity` on a prefab or scene entity that should be networked: property 1 is never authored (it is editor-hidden and must be 0 in documents); property 2 names the proxy prefab; properties 3 and 4 usually keep their defaults.
- `Zenith.Net.ProxyPrefabSet` on exactly one entity of a session scene, listing every proxy prefab that scene's networked entities name.
- A proxy prefab has no rigid body, character controller, root-motion graph, behaviour or identity.

### Editor surfaces

The reflected inspector shows both components (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface" needs no NET extension); property 1 is hidden by `PROPERTY_FLAG_EDITOR_HIDDEN`. Play-in-editor worlds run offline at Stage 3 (§ "World attach and detach").

### Cook steps

`net.validate` (`Zenith2/Tools/ZenithCook/Internal/Net/`), run on every cooked scene and prefab, calls `Zenith_ValidateNetDocument` (`ZenithAuthoring`, `Internal/Net/`):

| Rule | Diagnostic |
|---|---|
| N1 an identity with property 1 non-zero | error `net: <document> entity <authoring ID>: NetworkID is assigned at run time; remove the value` |
| N2 more than one `Zenith.Net.ProxyPrefabSet` in a scene | error |
| N3 a proxy prefab named by a set or an identity carries a rigid body, a character controller, a behaviour or an identity | error naming the component |
| N4 an identity whose proxy prefab is not in any set of the scenes that place it | warning |
| N5 the cooked registry's replication table has an error diagnostic | error (the same `Build` as the runtime's) |

### Migration hooks

None: NET's components are versioned reflected types (version 1) migrated by ECS's chain when they change (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind"); the save section has its own version (§ "Formats summary"); the wire formats are not stored data.

## Failure handling

### Failure table

| Failure | Behaviour | Test |
|---|---|---|
| a key value invalid | Start fails, exit code 3 with the key named | `Test_NetModuleInstance_Create_FailsAtConfig_Unwinds` |
| bind fails | Start fails with `MODULE_ERROR_DEVICE`, exit code 4 | `Test_NetModuleInstance_Create_FailsAtPort_Unwinds` |
| client cannot open its port, proxy table invalid or prefab not resident | the transport publishes FAILED from the prologue; the run continues offline | `Test_NetSession_ProxyNotResidentFails` |
| no server answers | FAILED (`CONNECT_TIMEOUT`) after 10 s | `Test_NetSession_ConnectTimeout` |
| rejected | FAILED with the reason | `Test_NetSession_RejectReasons` |
| a request before the server's first tick | dropped (`net.packets.early`); the client's retry joins | `Test_NetSession_JoinAtFirstPrologue` |
| peer silent | DISCONNECTED / PEER_LEFT (`TIMEOUT`) after 5 s | `Test_NetSession_PeerTimeoutBothSides` |
| malformed or foreign datagrams | dropped and counted; 8 in 1 s from the peer disconnect it | `Test_NetWire_MalformedBurstDisconnects`, `Test_NetSession_ForeignAndOldPacketsDropped` |
| more than 512 datagrams in one frame | the rest wait for the next prologue | `Test_NetSession_PumpBoundedAt512` |
| the sink is full | the fragment is dropped; the next tick supersedes it | `Test_NetClient_SinkFullDropsFragment` |
| the command window is full | the command is dropped (`net.server.window.dropped`); its sequence becomes lost after the hold | `Test_NetInput_GapHeldThenLost` |
| a destroy flood | at most 256 destroys per capture, each still repeated 30 times | `Test_NetFragments_DestroyFloodStaysWithin32Fragments` |
| the map is full | the entity is unindexed and not networked; a client spawns no proxy for it | `Test_NetIdentity_MapFullLeavesUnindexed`, `Test_NetClient_MapFullSpawnsNothing` |
| the counter or peer indices are exhausted | assignment or joins stop; counted and logged | `Test_NetIdentity_CounterExhaustionStopsAssigning` |
| a recorded payload is malformed (a corrupt log) | dropped and counted; the replay continues and its tick hashes report any divergence | `Test_NetPublication_PayloadLayout` |
| a corrupt save section | SAVE's restore fails as for any participant | `Test_NetIdentity_SectionCorruptRefused` |
| a restore of a live session world | the session ends (client DISCONNECTED `RESTORED`, server LISTENING); the save was COHERENT | `Test_NetIdentity_RestoreClearsApplyTables` |
| a restore of a session world captured before its session started | the captured state is kept; the client still joins; the save was EXACT | `Test_NetIdentity_RestoreOfPreSessionClientKeepsConnecting` |
| allocation failure | only at start and attach: `MODULE_ERROR_OUT_OF_MEMORY` | `Test_NetModuleInstance_Create_FailsAtTable_Unwinds`, `Test_NetModuleInstance_Create_FailsAtTransport_Unwinds` |
| device loss | not applicable: NET uses no device; a client's render recovery does not touch the session | `Test_NetHeadless_ModuleStartsWithoutDevice` |
| Android background (Stage 7) | the session world pauses at a tick boundary; the server times the peer out unless it returns within `net.peerTimeoutMs` | `Test_NetSession_Stage7StatesFollowTables` |
| a budget breach | no NET behaviour changes; the breach is reported by the budget run | the Stage 3 per-system budget ticket's run |

## Test strategy

### Test kinds

| Kind | Where |
|---|---|
| unit tests of codecs, the map, the table, the window | `ZenithNet_Tests` |
| world tests on each ECS backend with worker and layout variation | `ZenithNet_Tests` with `Zenith_NetTestWorld.h` |
| session tests over memory ports with a manual clock | `ZenithNet_Tests` with `Zenith_NetTestPeers.h` |
| fuzz tests of every parser | `Zenith_Fuzz_Net*` |
| the two-process loopback and its replays | the loopback suite |
| a `needs-gpu` render test | `Zenith_Integration_NetLoopback` |
| authoring and cook tests | `ZenithAuthoring_Tests`, `ZenithCook_Tests` |
| Stage 7 conformance, property and integration tests | `Zenith2/Docs/Design/NET/Tests.md` → "Stage 7 test catalogue" |

### Required named tests summary

Every named test with its asserted case is `Zenith2/Docs/Design/NET/Tests.md`; each requirement of `Zenith2/Docs/Requirements/NET.md` names its tests.

## Budget rows

### Rows NET is measured against

| Row | Use |
|---|---|
| Z2-B-PLAT-001 | Input phase CPU: the bridge's `ProvideTickCommands` and `ObserveTickCommands` run inside `Input.Actions` and count toward it |
| Z2-B-JOBS-002 | bounded catch-up ticks per frame: a server refuses a value above 8 |

### Pending row needs

Requested through cross-doc item X398 for `Zenith2/Docs/Budgets.md` → "Per-system rows"; the Budgets batch assigns their IDs:

| Proposed row | Name | Unit | Procedure | Platform | Breach | Producer |
|---|---|---|---|---|---|---|
| N1 | NET phase-1 CPU per tick (`Net.IdentityIndex` + `Net.Session` + `Net.ClientApply`) | ms | MP-CPUZONE | Win, S24 | BR-ACCEPT | the Stage 3 per-system budget ticket for NET |
| N2 | NET capture CPU per tick (`Net.Capture`) | ms | MP-CPUZONE | Win, S24 | BR-ACCEPT | the same |
| N3 | NET main-thread pump and send per frame (`Net.Pump` + `Net.Send`) | ms | MP-CPUZONE | Win, S24 | BR-ACCEPT | the same |
| N4 | NET resident memory per process | MB | MP-SUBMEM | Win, S24 | BR-GOVERNOR | the same |
| N5 | loopback snapshot bytes per tick at 64 characters | KiB | MP-COUNTER | Win | BR-ACCEPT | the same |
| N6 | server CPU per connected peer per tick | ms | MP-CPUZONE | Win (and the D-NET-SERVER-PLATFORM platform) | BR-ACCEPT | the Stage 7 per-system budget ticket for NET |
| N7 | bandwidth per peer at 64 relevant characters | KiB/s | MP-COUNTER | Win, S24 | BR-ACCEPT | the same |
| N8 | client prediction re-simulation CPU at 150 ms RTT | ms | MP-CPUZONE | Win, S24 | BR-ACCEPT | the same |
| N9 | interest management CPU per send tick at 64 peers | ms | MP-CPUZONE | Win | BR-ACCEPT | the same |

Proposed values for the producers to measure against (proposals, not rows): N1 0.05 ms desktop and 0.15 ms S24 at 64 characters; N2 0.05 ms and 0.15 ms; N3 0.1 ms; N4 6 MB; N5 2.5 KiB; N6 0.05 ms; N7 64 KiB/s; N8 2 ms on the S24; N9 0.5 ms.

## Stage plan

### Stage 1

- The specification ticket of `Zenith2/Docs/Design/NET/Foundations.md` → "Specification confirmation in Stage 1": confirms the five foundations against the Stage 1 rulings and appends the confirmation ruling to `Zenith2/Docs/DecisionLog.md` (Z2-D-097). No code.
- The cross-doc items X391 to X399 are applied by the core-doc batches; the Stage 1 ticket lists any still queued.
- Decisions it waits on: D-PHASES (open, Stage 1), D-SCRIPT-TXN (open, Stage 1), D-SEL-HASH (open, Stage 1).

### Stage 3

Module first stage S3 (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping"); slices in dependency order:

1. **API slice**: the eight public headers with declarations only, the module skeleton (RegisterTypes registers everything; Start parses keys and returns an instance that attaches nothing), the registration-shape test.
2. **Identity**: map, allocation, `Net.IdentityIndex`, lifecycle listener, save participant; after ECS's systems, change filters and SAVE's participants exist.
3. **Authority**: the replication table, Start validation, `Net.SetOwner`, census rows, `net.authority`.
4. **Wire**: codecs, ULEB128, limits, validation, the fuzz targets.
5. **Ports and transport**: the memory port, the platform port (after PLAT's socket, row NT1), sessions and timers.
6. **Input path**: the bridge (after PLAT and MOD, rows NT3 and NT5), mapping, emission, window.
7. **Snapshot path**: the proxy table, quantisation, capture, fragments, send (after cross-doc item X396).
8. **Client path**: intake, publication (after SCRIPT's registry row, NT2), apply, digests.
9. **Script surface**: bindings, events, misuse codes (after cross-doc item X391).
10. **Diagnostics**: counters, zones, console, rows, dump, the summary line.
11. **Proof mode** (after MOD's sink, row NT6).
12. **Loopback content** in WorkloadCharacters (after ANIM's workload game and SCENE's flag, rows NT10, NT8).
13. **Loopback suite** and its registry entries (after MOD's registry change, row NT11); the `needs-gpu` render test.
14. **Authoring check and cook step**.
15. **The Stage 3 per-system budget ticket for NET** (`needs-human`), allocating N1–N5.

### Stage 7

The full tree of `Zenith2/Docs/Design/NET/Stage7.md`, each subtree a set of tickets with fix version `Z2 Stage 7`, after the four NET decisions and the Stage 7 revalidation ticket; schedulable in the Stage 6 sprint where it depends on no open decision (`Zenith2/Docs/Roadmap.md` → "Stage 6 pull-forward"). Order: serialization, transport and session (after D-SEL-NETTRANSPORT, D-NET-SESSION-AUTH), network simulation, replication, interest management, prediction (after D-NET-PREDICTION-MODEL), dedicated server builds (after D-NET-SERVER-PLATFORM), diagnostics, the Stage 7 per-system budget ticket for NET (N6–N9).

### API-only slices

Stage 3 slice 1 (all public headers, no behaviour) lands before every consumer: the loopback content, the authoring check and the bindings' tests compile against it. Stage 7's `Zenith_NetTransportBackend.h`, `Zenith_NetSimulatedPort.h` and `Zenith_NetStatistics.h` each land as an API slice before their implementations.

### Ticket slices and ordering

Each slice above is one or more TASKs of about one library and 300–500 lines with its tests in the same ticket; a slice that crosses a pending row is `blocks`-linked from the producing area's ticket (declare first, implement after the callee).

## Decisions

### Settled decisions

| Decision | Governs |
|---|---|
| Z2-D-073 | the scope: foundations S1/S3, the full tree deferred to S7 |
| Z2-D-076 | local multiplayer and split-screen excluded |
| Z2-D-019 | determinism of every NET path into a world |
| Z2-D-053 | publications at tick boundaries in completion-ID order |
| Z2-D-080 | phases, per-field authority, commands at sync points |
| Z2-D-024, Z2-D-081 | NET is a static module of the game's one executable |
| Z2-D-038 | serialization never depends on C++ layout (the wire codecs, the save section) |
| Z2-D-040, Z2-D-045, Z2-D-046 | ownership, results, no mutable global state |
| Z2-D-020 | data-defined components carry network authority like C++ ones |
| Z2-D-082 | any Stage 7 third-party library is pinned and confined to `Internal/` |
| Z2-D-084 | measured decisions split into harness and ruling tickets |
| Z2-D-097 | rulings appended to the DecisionLog in the ticket's diff |

### Open decisions

Relied on: D-PHASES (open, Stage 1), D-SCRIPT-TXN (open, Stage 1), D-SEL-HASH (open, Stage 1). Owned: the four of § "Selection decisions NET owns", registered by cross-doc item X397.

### Selection decisions NET owns

| Decision | Stage | Candidates, criteria, acceptance rule | Consumers blocked |
|---|---|---|---|
| D-SEL-NETTRANSPORT | 7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Transport selection" | the Stage 7 transport, session and diagnostics tickets |
| D-NET-SESSION-AUTH | 7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Session security" | handshake v2, Shipping sessions, the server secret |
| D-NET-PREDICTION-MODEL | 7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Prediction model selection" | every prediction ticket |
| D-NET-SERVER-PLATFORM | 7 | `Zenith2/Docs/Design/NET/Stage7.md` → "Server platforms" | the server build target, its gate step and packaging |

Each measured one (D-SEL-NETTRANSPORT, D-NET-PREDICTION-MODEL) is a harness ticket under `Zenith2/Benchmarks/` plus a `needs-human` ruling ticket that commits its report under `Zenith2/Docs/Research/` (Z2-D-084); the other two are `needs-human` ruling tickets with written analyses.

## Requirement index

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-NET-001 | Foundations confirmed in Stage 1 | S1 | NET/Foundations.md → "Specification confirmation in Stage 1"; NET.md → "Stage 1" |
| Z2-R-NET-002 | Foundation seams queued in the core documents | S1 | NET.md → "Assumed interfaces pending other areas"; NET/Foundations.md → "Specification confirmation in Stage 1" |
| Z2-R-NET-003 | ZenithNet module library | S3 | NET.md → "Targets"; NET.md → "Libraries NET depends on"; NET/PublicAPI.md → "Header list" |
| Z2-R-NET-004 | Public header rules across ZenithNet | S3 | NET/PublicAPI.md → "Rules across the ZenithNet headers" |
| Z2-R-NET-005 | Module entry points and registrations | S3 | NET.md → "Module start"; NET/PublicAPI.md → "Zenith_Module_Net header" |
| Z2-R-NET-006 | World attach and detach | S3 | NET.md → "World attach and detach" |
| Z2-R-NET-007 | Network ID value type | S3 | NET/PublicAPI.md → "Zenith_NetworkID header"; NET/Foundations.md → "Network ID layout" |
| Z2-R-NET-008 | Network messages carry network IDs, never handles | S3 | NET/Foundations.md → "Network ID layout"; NET/PublicAPI.md → "Rules across the ZenithNet headers" |
| Z2-R-NET-009 | Peer index issuance | S3 | NET/Foundations.md → "Peer index issuance" |
| Z2-R-NET-010 | Deterministic network ID allocation | S3 | NET/Foundations.md → "Network ID allocation"; NET/Foundations.md → "Identity index system" |
| Z2-R-NET-011 | Network IDs never reused | S3 | NET/Foundations.md → "Network ID allocation" |
| Z2-R-NET-012 | Identity index system | S3 | NET/Foundations.md → "Identity index system" |
| Z2-R-NET-013 | Network ID map container | S3 | NET/PublicAPI.md → "Zenith_NetworkIDMap header"; NET/Foundations.md → "Network ID map" |
| Z2-R-NET-014 | Map writers and destroy handling | S3 | NET/Foundations.md → "Network ID map" |
| Z2-R-NET-015 | Duplicate network IDs | S3 | NET/Foundations.md → "Duplicate and unknown IDs" |
| Z2-R-NET-016 | Unknown network IDs never assert | S3 | NET/Foundations.md → "Duplicate and unknown IDs" |
| Z2-R-NET-017 | Identity across save and restore | S3 | NET/Foundations.md → "Identity across save and restore" |
| Z2-R-NET-018 | Client proxies excluded from saves | S3 | NET/Foundations.md → "Identity across save and restore" |
| Z2-R-NET-019 | Network IDs reproduce under replay | S3 | NET/Foundations.md → "Identity in replays" |
| Z2-R-NET-020 | Authority semantics per role | S3 | NET/Foundations.md → "Authority values and meanings" |
| Z2-R-NET-021 | No client-to-server state at Stage 3 | S3 | NET/Foundations.md → "Authority values and meanings"; NET/Loopback.md → "Message kinds" |
| Z2-R-NET-022 | Authority declaration rules | S3 | NET/Foundations.md → "Declaration rules" |
| Z2-R-NET-023 | Replication table order and hash | S3 | NET/Foundations.md → "Replication table construction"; NET/PublicAPI.md → "Zenith_NetReplicationTable header" |
| Z2-R-NET-024 | Authority validated at start in every configuration | S3 | NET/Foundations.md → "Authority validation at module start" |
| Z2-R-NET-025 | Owner peer and Net.SetOwner | S3 | NET/Foundations.md → "Ownership and the owner peer"; NET/PublicAPI.md → "Command kind payloads" |
| Z2-R-NET-026 | Client proxies written only by NET | S3 | NET/Foundations.md → "Authority on clients"; NET.md → "Cook steps" |
| Z2-R-NET-027 | Authority census rows | S3 | NET/Foundations.md → "Authority census rows" |
| Z2-R-NET-028 | Network authority report | S3 | NET/Foundations.md → "Network authority report" |
| Z2-R-NET-029 | NET components | S3 | NET/PublicAPI.md → "Zenith_NetComponents header" |
| Z2-R-NET-030 | Tick-rate agreement | S3 | NET/Foundations.md → "Tick-rate agreement" |
| Z2-R-NET-031 | Network roles need real-time ticks | S3 | NET/Foundations.md → "Tick-rate agreement"; NET/Loopback.md → "Snapshot send" |
| Z2-R-NET-032 | Tick-indexed messages | S3 | NET/Foundations.md → "Tick-indexed messages" |
| Z2-R-NET-033 | Catch-up and paused frames | S3 | NET/Foundations.md → "Catch-up and paused frames" |
| Z2-R-NET-034 | NET simulation reads no clock | S3 | NET/Foundations.md → "Clock discipline" |
| Z2-R-NET-035 | Remote commands enter only through phase 1 | S3 | NET/Foundations.md → "Remote commands enter phase 1" |
| Z2-R-NET-036 | Input command bridge per role | S3 | NET/Foundations.md → "Command source contract" |
| Z2-R-NET-037 | Command wire encoding equals the replay encoding | S3 | NET/Foundations.md → "Command wire encoding" |
| Z2-R-NET-038 | Networked logs replay without a network | S3 | NET/Foundations.md → "Command replay"; NET/Loopback.md → "Replay verification" |
| Z2-R-NET-039 | NET runs unchanged headless | S3 | NET/Foundations.md → "What a headless run contains" |
| Z2-R-NET-040 | Headless server composition | S3 | NET/Foundations.md → "Headless server composition" |
| Z2-R-NET-041 | Headless client composition | S3 | NET/Foundations.md → "Headless client composition" |
| Z2-R-NET-042 | Headless frame pacing relied on (NT7) | S3 | NET/Foundations.md → "Headless frame pacing" |
| Z2-R-NET-043 | Stage 3 Shipping guard | S3 | NET/Foundations.md → "Shipping and the headless roles" |
| Z2-R-NET-044 | Production port over PLAT sockets | S3 | NET/Loopback.md → "Production port over PLAT sockets" |
| Z2-R-NET-045 | Memory port pair | S3 | NET/Loopback.md → "Memory port"; NET/PublicAPI.md → "Zenith_NetDatagramPort header" |
| Z2-R-NET-046 | Pump and send discipline | S3 | NET/Loopback.md → "Pump and send discipline" |
| Z2-R-NET-047 | Packet header and integrity | S3 | NET/Loopback.md → "Packet header"; NET/Loopback.md → "Packet integrity" |
| Z2-R-NET-048 | Connect messages | S3 | NET/Loopback.md → "CONNECT_REQUEST"; NET/Loopback.md → "CONNECT_ACCEPT"; NET/Loopback.md → "CONNECT_REJECT" |
| Z2-R-NET-049 | INPUT message | S3 | NET/Loopback.md → "INPUT" |
| Z2-R-NET-050 | SNAPSHOT message | S3 | NET/Loopback.md → "SNAPSHOT" |
| Z2-R-NET-051 | KEEPALIVE and DISCONNECT | S3 | NET/Loopback.md → "KEEPALIVE"; NET/Loopback.md → "DISCONNECT" |
| Z2-R-NET-052 | Canonical ULEB128 | S3 | NET/Loopback.md → "Unsigned LEB128" |
| Z2-R-NET-053 | Wire limits tested at the limit | S3 | NET/Loopback.md → "Wire limits" |
| Z2-R-NET-054 | Packet validation never asserts | S3 | NET/Loopback.md → "Packet validation order" |
| Z2-R-NET-055 | Malformed burst disconnects the peer | S3 | NET/Loopback.md → "Packet validation order" |
| Z2-R-NET-056 | Client session state machine | S3 | NET/Loopback.md → "Client session states" |
| Z2-R-NET-057 | Server peer state machine | S3 | NET/Loopback.md → "Server peer states" |
| Z2-R-NET-058 | Handshake rules | S3 | NET/Loopback.md → "Handshake rules" |
| Z2-R-NET-059 | Timeouts and keepalive | S3 | NET/Loopback.md → "Timeouts and keepalive" |
| Z2-R-NET-060 | Session events are recorded publications | S3 | NET/Loopback.md → "Fragment publication"; NET/Loopback.md → "Client session states" |
| Z2-R-NET-061 | Client command capture | S3 | NET/Loopback.md → "Client command capture" |
| Z2-R-NET-062 | Target tick mapping | S3 | NET/Loopback.md → "Target tick mapping" |
| Z2-R-NET-063 | Client INPUT emission | S3 | NET/Loopback.md → "Client INPUT emission" |
| Z2-R-NET-064 | Server command intake | S3 | NET/Loopback.md → "Server command intake" |
| Z2-R-NET-065 | Server command window releases in sequence | S3 | NET/Loopback.md → "Server command window" |
| Z2-R-NET-066 | Command delivery to phase 1 | S3 | NET/Loopback.md → "Command delivery to phase 1" |
| Z2-R-NET-067 | Capture at the snapshot point | S3 | NET/Loopback.md → "Capture at the snapshot point" |
| Z2-R-NET-068 | Capture independent of workers and layout | S3 | NET/Loopback.md → "Capture at the snapshot point"; NET.md → "Determinism obligations" |
| Z2-R-NET-069 | Proxy prefab table | S3 | NET/Loopback.md → "Proxy prefab table" |
| Z2-R-NET-070 | Position quantisation | S3 | NET/Loopback.md → "Quantisation" |
| Z2-R-NET-071 | Quaternion quantisation | S3 | NET/Loopback.md → "Quantisation" |
| Z2-R-NET-072 | Deterministic fragment encoding | S3 | NET/Loopback.md → "Fragment encoding" |
| Z2-R-NET-073 | Snapshot send | S3 | NET/Loopback.md → "Snapshot send" |
| Z2-R-NET-074 | Client fragment intake | S3 | NET/Loopback.md → "Client fragment intake" |
| Z2-R-NET-075 | NET asynchronous source and payload | S3 | NET/Loopback.md → "Fragment publication"; NET.md → "Asynchronous source and misuse block" |
| Z2-R-NET-076 | Client apply | S3 | NET/Loopback.md → "Client apply" |
| Z2-R-NET-077 | Stale fragments and pending spawns | S3 | NET/Loopback.md → "Client apply" |
| Z2-R-NET-078 | Controlled entity event | S3 | NET/Loopback.md → "Client apply" |
| Z2-R-NET-079 | Acknowledgements and digests | S3 | NET/Loopback.md → "Acknowledgements and digests" |
| Z2-R-NET-080 | Proof assertions in-process | S3 | NET/Loopback.md → "Proof assertions" |
| Z2-R-NET-081 | Proof outcome through exit codes | S3 | NET/Loopback.md → "Proof outcome" |
| Z2-R-NET-082 | Loopback scene and prefabs | S3 | NET/Loopback.md → "Scene and prefabs" |
| Z2-R-NET-083 | Loopback scripts | S3 | NET/Loopback.md → "Loopback scripts" |
| Z2-R-NET-084 | Loopback input script | S3 | NET/Loopback.md → "Input script" |
| Z2-R-NET-085 | Loopback suite target and registration | S3 | NET/Loopback.md → "Suite target" |
| Z2-R-NET-086 | Loopback driver | S3 | NET/Loopback.md → "Driver script" |
| Z2-R-NET-087 | Command log verification | S3 | NET/Loopback.md → "Command log verification" |
| Z2-R-NET-088 | Loopback replicates transforms and commands | S3 | NET/Loopback.md → "The loopback suite"; NET/Loopback.md → "Command log verification" |
| Z2-R-NET-089 | Loopback logs replay bit-identically | S3 | NET/Loopback.md → "Replay verification" |
| Z2-R-NET-090 | Net script bindings | S3 | NET/PublicAPI.md → "Script-facing API" |
| Z2-R-NET-091 | Net misuse codes | S3 | NET/PublicAPI.md → "Misuse codes"; NET.md → "Asynchronous source and misuse block" |
| Z2-R-NET-092 | Net event types | S3 | NET/PublicAPI.md → "Event types" |
| Z2-R-NET-093 | Net configuration keys | S3 | NET/PublicAPI.md → "Configuration keys" |
| Z2-R-NET-094 | Net error enumeration | S3 | NET.md → "Error enumerations"; NET/PublicAPI.md → "Zenith_NetErrors header" |
| Z2-R-NET-095 | No threads and no locks at Stage 3 | S3 | NET.md → "Threads NET owns"; NET.md → "Lock-order entries"; NET.md → "Cross-thread hand-offs" |
| Z2-R-NET-096 | Startup and shutdown order | S3 | NET.md → "Startup and shutdown" |
| Z2-R-NET-097 | NET determinism obligations | S3 | NET.md → "Determinism obligations" |
| Z2-R-NET-098 | Net counters | S3 | NET.md → "Counters" |
| Z2-R-NET-099 | Net zones and log categories | S3 | NET.md → "Profiling zones"; NET.md → "Log categories and messages" |
| Z2-R-NET-100 | Net console, rows and frame dump | S3 | NET.md → "Console commands"; NET.md → "On-screen profiler rows"; NET.md → "Frame dump section" |
| Z2-R-NET-101 | Net document validation | S3 | NET.md → "Cook steps"; NET.md → "What NET authors" |
| Z2-R-NET-102 | Net cook step | S3 | NET.md → "Cook steps" |
| Z2-R-NET-103 | Net editor surfaces | S3 | NET.md → "Editor surfaces" |
| Z2-R-NET-104 | Net failure behaviours | S3 | NET.md → "Failure table" |
| Z2-R-NET-105 | Stage 3 NET budget rows allocated | S3 | NET.md → "Pending row needs" |
| Z2-R-NET-106 | Local multiplayer excluded | S1 | NET.md → "Explicit exclusions" |
| Z2-R-NET-107 | Split-screen excluded | S1 | NET.md → "Explicit exclusions" |
| Z2-R-NET-108 | Transport selection ruled | S7 | NET/Stage7.md → "Transport selection" |
| Z2-R-NET-109 | Transport backend contract | S7 | NET/Stage7.md → "Transport layer contract" |
| Z2-R-NET-110 | Stateless challenge handshake | S7 | NET/Stage7.md → "Connection handshake version 2" |
| Z2-R-NET-111 | Handshake rate limits | S7 | NET/Stage7.md → "Connection handshake version 2" |
| Z2-R-NET-112 | Channels and reliability | S7 | NET/Stage7.md → "Channels and reliability" |
| Z2-R-NET-113 | Fragmentation and reassembly | S7 | NET/Stage7.md → "Fragmentation and reassembly" |
| Z2-R-NET-114 | Congestion and bandwidth control | S7 | NET/Stage7.md → "Congestion and bandwidth control" |
| Z2-R-NET-115 | Session lifecycle with reconnect | S7 | NET/Stage7.md → "Session lifecycle"; NET/Stage7.md → "Stage 7 state machines" |
| Z2-R-NET-116 | Peer slots and join in progress | S7 | NET/Stage7.md → "Peer slots and join in progress" |
| Z2-R-NET-117 | Clock synchronisation | S7 | NET/Stage7.md → "Clock synchronisation" |
| Z2-R-NET-118 | Session security ruled | S7 | NET/Stage7.md → "Session security" |
| Z2-R-NET-119 | Shipping sessions authenticated | S7 | NET/Stage7.md → "Session security" |
| Z2-R-NET-120 | Transport and session error codes | S7 | NET/Stage7.md → "Transport and session error codes" |
| Z2-R-NET-121 | Per-peer replication model | S7 | NET/Stage7.md → "Replication model" |
| Z2-R-NET-122 | Create before state | S7 | NET/Stage7.md → "Entity scopes and relevance hand-off"; NET/Stage7.md → "Entity create and destroy" |
| Z2-R-NET-123 | Full property replication | S7 | NET/Stage7.md → "Property replication" |
| Z2-R-NET-124 | Delta compression against baselines | S7 | NET/Stage7.md → "Delta compression against acknowledged baselines" |
| Z2-R-NET-125 | Priority and bandwidth allocation | S7 | NET/Stage7.md → "Priority and bandwidth allocation" |
| Z2-R-NET-126 | Owner-client proposals | S7 | NET/Stage7.md → "Owner-client authority" |
| Z2-R-NET-127 | Replicated events | S7 | NET/Stage7.md → "Replicated events" |
| Z2-R-NET-128 | Per-peer player slots | S7 | NET/Stage7.md → "Player slots and per-peer input" |
| Z2-R-NET-129 | Replication across world transitions | S7 | NET/Stage7.md → "Replication with world transitions" |
| Z2-R-NET-130 | Prediction model ruled | S7 | NET/Stage7.md → "Prediction model selection" |
| Z2-R-NET-131 | Predicted entities | S7 | NET/Stage7.md → "Predicted entities" |
| Z2-R-NET-132 | Command acknowledgement | S7 | NET/Stage7.md → "Command acknowledgement" |
| Z2-R-NET-133 | Reconciliation | S7 | NET/Stage7.md → "Reconciliation algorithm" |
| Z2-R-NET-134 | Correction smoothing | S7 | NET/Stage7.md → "Correction smoothing" |
| Z2-R-NET-135 | Remote entity interpolation | S7 | NET/Stage7.md → "Remote entity interpolation" |
| Z2-R-NET-136 | Prediction limits and rewind queries | S7 | NET/Stage7.md → "Prediction limits" |
| Z2-R-NET-137 | Relevance grid | S7 | NET/Stage7.md → "Relevance grid" |
| Z2-R-NET-138 | Relevancy sets per peer | S7 | NET/Stage7.md → "Relevancy sets per peer" |
| Z2-R-NET-139 | Relevance hysteresis | S7 | NET/Stage7.md → "Hysteresis and priority" |
| Z2-R-NET-140 | Streaming around every peer | S7 | NET/Stage7.md → "Streaming integration" |
| Z2-R-NET-141 | Wire format version 2 | S7 | NET/Stage7.md → "Wire format version 2"; NET/Stage7.md → "Protocol versioning"; NET/Stage7.md → "Message publication in pieces" |
| Z2-R-NET-142 | Bit writer and reader | S7 | NET/Stage7.md → "Bit writer and reader" |
| Z2-R-NET-143 | Property codecs | S7 | NET/Stage7.md → "Property codecs" |
| Z2-R-NET-144 | Replication precision declarations | S7 | NET/Stage7.md → "Replication precision declarations" |
| Z2-R-NET-145 | Reference encoding | S7 | NET/Stage7.md → "Reference encoding" |
| Z2-R-NET-146 | Serialization limits and fuzzing | S7 | NET/Stage7.md → "Serialization limits" |
| Z2-R-NET-147 | Dedicated server build target | S7 | NET/Stage7.md → "Server build target" |
| Z2-R-NET-148 | Server platforms ruled | S7 | NET/Stage7.md → "Server platforms" |
| Z2-R-NET-149 | Server runtime differences | S7 | NET/Stage7.md → "Server runtime differences" |
| Z2-R-NET-150 | Server operation | S7 | NET/Stage7.md → "Server operation" |
| Z2-R-NET-151 | Simulated port | S7 | NET/Stage7.md → "Simulated port" |
| Z2-R-NET-152 | Simulation profiles | S7 | NET/Stage7.md → "Simulation profiles" |
| Z2-R-NET-153 | Deterministic network simulation | S7 | NET/Stage7.md → "Simulation determinism" |
| Z2-R-NET-154 | Per-peer statistics | S7 | NET/Stage7.md → "Per-peer statistics" |
| Z2-R-NET-155 | Network panel and console | S7 | NET/Stage7.md → "Network panel and console" |
| Z2-R-NET-156 | Packet capture | S7 | NET/Stage7.md → "Packet capture" |
| Z2-R-NET-157 | Replication debug views and dump | S7 | NET/Stage7.md → "Replication debug views"; NET/Stage7.md → "Network frame dump" |
| Z2-R-NET-158 | Stage 7 NET budget rows allocated | S7 | NET.md → "Pending row needs" |
| Z2-R-NET-159 | Session world capture fidelity | S3 | NET/Foundations.md → "Identity across save and restore" |
| Z2-R-NET-160 | Restore ends a live session and keeps a pre-session one | S3 | NET/Foundations.md → "Identity across save and restore"; NET/Loopback.md → "Client session states" |
| Z2-R-NET-161 | Session state is a function of recorded events | S3 | NET/Loopback.md → "Client session states"; NET/Loopback.md → "Server peer states"; NET/PublicAPI.md → "Zenith_NetTypes header" |
| Z2-R-NET-162 | No transport or live asset read inside the frame's phases | S3 | NET/Loopback.md → "Client session states"; NET/Loopback.md → "Pump and send discipline" |
| Z2-R-NET-163 | Command window baseline | S3 | NET/Loopback.md → "Server command intake"; NET/Loopback.md → "Client command capture" |
| Z2-R-NET-164 | A client log replays without a connect address | S3 | NET.md → "Module start" |
| Z2-R-NET-165 | Destroys bounded per capture | S3 | NET/Loopback.md → "Fragment encoding"; NET/Foundations.md → "Network ID map" |
| Z2-R-NET-166 | Truncated captures resume at the first entity left out | S3 | NET/Loopback.md → "Capture at the snapshot point" |
| Z2-R-NET-167 | Every complete tick verified | S3 | NET/Loopback.md → "Acknowledgements and digests"; NET/Loopback.md → "Proof assertions" |
| Z2-R-NET-168 | Capture self-check and capture chain | S3 | NET/Loopback.md → "Capture at the snapshot point"; NET/Loopback.md → "Replay verification" |
| Z2-R-NET-169 | Destroyed proxies are never resurrected | S3 | NET/Loopback.md → "Client apply"; NET/Loopback.md → "Client fragment intake" |
| Z2-R-NET-170 | A full client map spawns no proxies | S3 | NET/Loopback.md → "Client apply" |
| Z2-R-NET-171 | Joins before the server's first tick succeed | S3 | NET/Loopback.md → "Handshake rules"; NET/Loopback.md → "Loopback scripts" |
| Z2-R-NET-172 | Pump bounded per frame | S3 | NET/Loopback.md → "Pump and send discipline"; NET/Loopback.md → "Wire limits" |

## Cross-area notes

Rows NET filed in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (prefix NT) and the cross-doc items it filed in `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`:

| Row | To | Where NET relies on it |
|---|---|---|
| NT1 | PLAT (SEC, PROC for the allow record) | `Zenith2/Docs/Design/NET/Loopback.md` → "Production port over PLAT sockets" |
| NT2 | SCRIPT | `Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication" |
| NT3 | PLAT | `Zenith2/Docs/Design/NET/Foundations.md` → "Command source contract" |
| NT4 | SEC | `Zenith2/Docs/Design/NET.md` → "Security assumptions" |
| NT5 | MOD | `Zenith2/Docs/Design/NET/Foundations.md` → "Command source contract" |
| NT6 | MOD | `Zenith2/Docs/Design/NET/Loopback.md` → "Proof assertions" |
| NT7 | MOD, JOBS | `Zenith2/Docs/Design/NET/Foundations.md` → "Headless frame pacing" |
| NT8 | SCENE | `Zenith2/Docs/Design/NET/Foundations.md` → "Replication table construction" |
| NT9 | PROC (SEC for the listener scan) | `Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles" |
| NT10 | ANIM | `Zenith2/Docs/Design/NET/Loopback.md` → "Scene and prefabs" |
| NT11 | MOD (SHIP consumer) | `Zenith2/Docs/Design/NET/Loopback.md` → "Suite target" |
| NT12 | GAME | `Zenith2/Docs/Design/NET/Stage7.md` → "Player slots and per-peer input" |
| NT13 | EDIT | `Zenith2/Docs/Design/NET.md` → "Tool-side files" |
| NT14 | ASSET | `Zenith2/Docs/Design/NET.md` → "Cook steps" |
| NT15 | DEVT | `Zenith2/Docs/Design/NET.md` → "Console commands" |
| NT16 | STREAM | `Zenith2/Docs/Design/NET/Stage7.md` → "Streaming integration" |
| NT17 | PLAT | `Zenith2/Docs/Design/NET/Stage7.md` → "Player slots and per-peer input" |
| NT18 | ONLINE | `Zenith2/Docs/Design/NET/Stage7.md` → "Session security" |
| NT19 | ECS | `Zenith2/Docs/Design/NET/Stage7.md` → "Replication precision declarations" |
| NT20 | MOD | `Zenith2/Docs/Design/NET/Stage7.md` → "Server build target" |
| NT21 | JOBS | `Zenith2/Docs/Design/NET/Stage7.md` → "Clock synchronisation" |
| NT22 | SAVE | `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore" |
| NT23 | SCRIPT | `Zenith2/Docs/Design/NET/Stage7.md` → "Replicated events" |
| NT24 | DEVT | `Zenith2/Docs/Design/NET/Stage7.md` → "Packet capture" |
| NT25 | ECS, SCENE | `Zenith2/Docs/Design/NET/Loopback.md` → "Quantisation" |
| NT26 | DEVT | `Zenith2/Docs/Design/NET/Loopback.md` → "Capture at the snapshot point" |

| Cross-doc item | Target | Change |
|---|---|---|
| X391 | Scripting/MisuseCatalogue.md → "Code ranges" | NET's block ZS3400–ZS3499 |
| X392 | Scripting.md → "Asynchronous source kind registry" | `ASYNC_SOURCE_NETWORK` |
| X393 | Architecture.md → "Library dependency graph" | ZenithNet's `DEPENDS` widening |
| X394 | Architecture.md → "Security architecture" | NET's parser limits and the network trust row |
| X395 | Architecture.md → "The Stage 3 loopback proof" | the full command lines and the strengthened in-process transform check; [Z2-N-ARCH-786] unamended |
| X396 | Architecture/FramePhases.md [Z2-N-ARCH-476], [Z2-N-ARCH-503], [Z2-N-ARCH-731]; Design/SAVE.md → "Determinism obligations" | a phase-11-owned module resource, one edit across the four statements |
| X397 | DecisionLog.md → "Open decisions" | the four Stage 7 NET decisions |
| X398 | Budgets.md | pending rows N1–N9 |
| X399 | Architecture/CompositionRoot.md | the bridge and assertion-sink members; headless frame pacing |
