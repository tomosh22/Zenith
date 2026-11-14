# SAVE design — Save games & replay

This is the entry document of the SAVE design family: versioned saves, checkpoints, persistent world and entity state, atomic writes, corruption recovery, slot metadata, migration fixtures, asynchronous saving, input recording and replays, reproducibility boundaries, consistent snapshots while jobs and streaming run, replay seeking and version compatibility, and the cross-system failure scenarios. The requirement catalogue is `Zenith2/Docs/Requirements/SAVE.md` (Stages 1 to 3, the coverage of inventory items and the exclusions) with `Zenith2/Docs/Requirements/SAVE/Stage4Plus.md` (Stages 4 and 6), one numbering. Engine library: `ZenithSave` (`Zenith2/Docs/Architecture.md` → "Library dependency graph", layer 5).

## Purpose and scope

### What SAVE owns

| Capability | Where specified |
|---|---|
| the `ZSAV` save container: header, section table, CRC-32C integrity, per-section compression descriptors | `Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSAV file" |
| the world snapshot at the save-snapshot point (phase 11) and out of tick, through reflection, for every ECS backend | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Phase-11 snapshot" |
| the incremental restore of a world from a save | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Restore pipeline" |
| the journaled atomic save write and its start-up recovery, the one-`.bak` backup policy ([Z2-N-CONTRACT-553]) | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Journaled write sequence", "Journal recovery" |
| save kinds and slots (manual, quick, autosave rotation, checkpoint, resume), slot metadata and thumbnails, listings, deletes, profiles per account | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot policy" |
| asynchronous saving: capture in phase 11, compression and I/O on a dedicated thread, results published at a tick boundary | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Image assembly and compression" |
| save migration: the format chain per `(major, minor)`, component migration through ECS, participant sections, the fixtures | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Migration pipeline" |
| corruption recovery: verification, backup fallback, quarantine | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Verification and corruption recovery" |
| every reflected world resource with a serialized property (ENV's `Zenith.Environment.State`, PLAT's `Zenith_SimulationInputContexts`, GAME's `Zenith.Runtime.WorldState` among them), saved per world in the RESOURCES section | `Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section" |
| the participant interface through which modules save state that lives outside reflection (physics bodies, animation graphs, navigation, cinematic playback, in-flight requests), and the per-capture fidelity each participant reports | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header" |
| persistent world and entity state: what SAVE guarantees for the state of unloaded content, which lives in components (STREAM's cell deltas) and reflected resources (Stage 4) | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Persistent world and entity state" |
| the `ZRPL` replay log's writer, reader, index, verifier and seeking, and the running build ID (the format is Architecture's) | `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header" |
| the resume save of PLAT's process-death policy: format, snapshot, journaled write, identity reader, restore | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Resume save and restore" |
| the `Save` Luau binding module and the `Zenith.Save*` events | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Script-facing API" |
| the SAVE files of the runtime bridge (`ZenithRuntime/Internal/Save/`) | § "Runtime bridge files"; `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations" |

### What SAVE does not own

| Not owned | Owner | Basis |
|---|---|---|
| the `ZRPL` record layout and the replay rules of Architecture | ARCH (Architecture.md); SAVE implements them | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" (orchestrator ruling: Architecture owns the record layout) |
| the authoring-document migration framework, editor autosave and crash recovery of documents | SCENE (`ZenithDocument`), EDIT | Z2-D-048; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Why a separate library" |
| when to write the resume state, its deadline and the relaunch decision | PLAT | `Zenith2/Docs/Design/PLAT/Android.md` → "Resume-state policy" |
| world transitions (the swap to a restored world) | GAME | Z2-D-080; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition" |
| what each module saves about itself beyond its reflected components and resources (physics bodies, animation graphs, navigation, cinematics) | each module, through `Zenith_SaveParticipant` | [Z2-N-ARCH-078] |
| which properties of a component or resource are serialized | the type's owner, through `PROPERTY_FLAG_SERIALIZED` | `Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record" |
| cloud saves and the cloud-save conflict model | ONLINE (S4 adapter, S6 backends) | Z2-D-074 |
| networked replication and dedicated servers (SAVE provides only the command log format the loopback proof records) | NET | Z2-D-073 |
| installed-update and Play Asset Delivery migration of content | PACK | Z2-D-027 |
| console save storage | deferred with consoles | Z2-D-071 |
| saves of third-party mods | excluded with modding | Z2-D-075 |
| quest, dialogue and inventory state as engine features: they are Luau gameplay state in components and behaviour state, saved generically | game-side | Z2-D-077 |

### Inventory items covered

<!-- INV-TABLE -->
| Item | Text | Stage | Requirements |
|---|---|---|---|
| Z2-I-SAVE-01 | versioned saves | S3 | Z2-R-SAVE-001, Z2-R-SAVE-002, Z2-R-SAVE-003, Z2-R-SAVE-004, Z2-R-SAVE-023, Z2-R-SAVE-024, Z2-R-SAVE-026, Z2-R-SAVE-030, Z2-R-SAVE-031, Z2-R-SAVE-043, Z2-R-SAVE-044, Z2-R-SAVE-045, Z2-R-SAVE-046, Z2-R-SAVE-050, Z2-R-SAVE-056, Z2-R-SAVE-057, Z2-R-SAVE-076, Z2-R-SAVE-077, Z2-R-SAVE-082, Z2-R-SAVE-083, Z2-R-SAVE-084, Z2-R-SAVE-095, Z2-R-SAVE-096, Z2-R-SAVE-102, Z2-R-SAVE-103, Z2-R-SAVE-154, Z2-R-SAVE-157, Z2-R-SAVE-161, Z2-R-SAVE-169, Z2-R-SAVE-176 |
| Z2-I-SAVE-02 | checkpoints | S3 | Z2-R-SAVE-064, Z2-R-SAVE-066, Z2-R-SAVE-067, Z2-R-SAVE-068, Z2-R-SAVE-172 |
| Z2-I-SAVE-03 | persistent world state | S4 | Z2-R-SAVE-122, Z2-R-SAVE-123, Z2-R-SAVE-124, Z2-R-SAVE-125, Z2-R-SAVE-127, Z2-R-SAVE-167 |
| Z2-I-SAVE-04 | persistent entity state | S4 | Z2-R-SAVE-122, Z2-R-SAVE-125, Z2-R-SAVE-126, Z2-R-SAVE-127 |
| Z2-I-SAVE-05 | atomic writes | S3 | Z2-R-SAVE-032, Z2-R-SAVE-033, Z2-R-SAVE-035, Z2-R-SAVE-036, Z2-R-SAVE-037, Z2-R-SAVE-038, Z2-R-SAVE-039, Z2-R-SAVE-040, Z2-R-SAVE-042, Z2-R-SAVE-093, Z2-R-SAVE-094, Z2-R-SAVE-136, Z2-R-SAVE-137, Z2-R-SAVE-138, Z2-R-SAVE-171 |
| Z2-I-SAVE-06 | corruption recovery | S3 | Z2-R-SAVE-023, Z2-R-SAVE-025, Z2-R-SAVE-027, Z2-R-SAVE-033, Z2-R-SAVE-034, Z2-R-SAVE-041, Z2-R-SAVE-052, Z2-R-SAVE-069, Z2-R-SAVE-071, Z2-R-SAVE-073, Z2-R-SAVE-074, Z2-R-SAVE-080, Z2-R-SAVE-081, Z2-R-SAVE-106, Z2-R-SAVE-107, Z2-R-SAVE-109, Z2-R-SAVE-137, Z2-R-SAVE-140, Z2-R-SAVE-141, Z2-R-SAVE-161 |
| Z2-I-SAVE-07 | slot metadata | S3 | Z2-R-SAVE-028, Z2-R-SAVE-029, Z2-R-SAVE-063, Z2-R-SAVE-064, Z2-R-SAVE-065, Z2-R-SAVE-069, Z2-R-SAVE-070, Z2-R-SAVE-071, Z2-R-SAVE-072, Z2-R-SAVE-097, Z2-R-SAVE-098, Z2-R-SAVE-102, Z2-R-SAVE-109, Z2-R-SAVE-160, Z2-R-SAVE-162, Z2-R-SAVE-174, Z2-R-SAVE-175 |
| Z2-I-SAVE-08 | save migration fixtures | S3 | Z2-R-SAVE-031, Z2-R-SAVE-041, Z2-R-SAVE-051, Z2-R-SAVE-053, Z2-R-SAVE-085, Z2-R-SAVE-086, Z2-R-SAVE-087, Z2-R-SAVE-088, Z2-R-SAVE-089, Z2-R-SAVE-090, Z2-R-SAVE-091, Z2-R-SAVE-092, Z2-R-SAVE-139, Z2-R-SAVE-146 |
| Z2-I-SAVE-09 | async saving | S3 | Z2-R-SAVE-022, Z2-R-SAVE-040, Z2-R-SAVE-043, Z2-R-SAVE-054, Z2-R-SAVE-055, Z2-R-SAVE-058, Z2-R-SAVE-059, Z2-R-SAVE-060, Z2-R-SAVE-061, Z2-R-SAVE-062, Z2-R-SAVE-068, Z2-R-SAVE-075, Z2-R-SAVE-076, Z2-R-SAVE-099, Z2-R-SAVE-100, Z2-R-SAVE-101, Z2-R-SAVE-104, Z2-R-SAVE-106, Z2-R-SAVE-108, Z2-R-SAVE-135, Z2-R-SAVE-142, Z2-R-SAVE-143, Z2-R-SAVE-144, Z2-R-SAVE-145, Z2-R-SAVE-149, Z2-R-SAVE-151, Z2-R-SAVE-154, Z2-R-SAVE-155, Z2-R-SAVE-156, Z2-R-SAVE-170, Z2-R-SAVE-171, Z2-R-SAVE-173, Z2-R-SAVE-175 |
| Z2-I-SAVE-10 | input recording | S3 | Z2-R-SAVE-001, Z2-R-SAVE-002, Z2-R-SAVE-005, Z2-R-SAVE-006, Z2-R-SAVE-007, Z2-R-SAVE-008, Z2-R-SAVE-010, Z2-R-SAVE-011, Z2-R-SAVE-014, Z2-R-SAVE-015, Z2-R-SAVE-016, Z2-R-SAVE-147, Z2-R-SAVE-153, Z2-R-SAVE-159 |
| Z2-I-SAVE-11 | reproducibility boundaries | S3 | Z2-R-SAVE-009, Z2-R-SAVE-010, Z2-R-SAVE-012, Z2-R-SAVE-017, Z2-R-SAVE-020, Z2-R-SAVE-030, Z2-R-SAVE-046, Z2-R-SAVE-047, Z2-R-SAVE-049, Z2-R-SAVE-057, Z2-R-SAVE-066, Z2-R-SAVE-078, Z2-R-SAVE-079, Z2-R-SAVE-105, Z2-R-SAVE-110, Z2-R-SAVE-111, Z2-R-SAVE-148, Z2-R-SAVE-150, Z2-R-SAVE-152, Z2-R-SAVE-176 |
| Z2-I-SAVE-12 | consistent save snapshots while jobs and streaming are active | S4 | Z2-R-SAVE-128, Z2-R-SAVE-129, Z2-R-SAVE-156, Z2-R-SAVE-166, Z2-R-SAVE-167, Z2-R-SAVE-173 |
| Z2-I-SAVE-13 | replay seeking | S4 | Z2-R-SAVE-113, Z2-R-SAVE-114, Z2-R-SAVE-115, Z2-R-SAVE-116, Z2-R-SAVE-117, Z2-R-SAVE-118, Z2-R-SAVE-150, Z2-R-SAVE-158, Z2-R-SAVE-165, Z2-R-SAVE-167, Z2-R-SAVE-168 |
| Z2-I-SAVE-14 | replay version compatibility | S6 | Z2-R-SAVE-012, Z2-R-SAVE-013, Z2-R-SAVE-092, Z2-R-SAVE-111, Z2-R-SAVE-119, Z2-R-SAVE-120, Z2-R-SAVE-121, Z2-R-SAVE-139, Z2-R-SAVE-146 |
| Z2-I-SAVE-15 | deterministic replays, including script failures | S3 | Z2-R-SAVE-004, Z2-R-SAVE-005, Z2-R-SAVE-007, Z2-R-SAVE-008, Z2-R-SAVE-009, Z2-R-SAVE-018, Z2-R-SAVE-019, Z2-R-SAVE-020, Z2-R-SAVE-021, Z2-R-SAVE-022, Z2-R-SAVE-047, Z2-R-SAVE-048, Z2-R-SAVE-049, Z2-R-SAVE-060, Z2-R-SAVE-077, Z2-R-SAVE-079, Z2-R-SAVE-112, Z2-R-SAVE-115, Z2-R-SAVE-147, Z2-R-SAVE-148, Z2-R-SAVE-149, Z2-R-SAVE-152, Z2-R-SAVE-153, Z2-R-SAVE-158, Z2-R-SAVE-164, Z2-R-SAVE-166, Z2-R-SAVE-168 |
| Z2-I-SAVE-16 | cross-system failure scenario: saving during streaming with active animation and physics | S4 | Z2-R-SAVE-130, Z2-R-SAVE-163 |
| Z2-I-SAVE-17 | cross-system failure scenario: an interrupted write during that save | S4 | Z2-R-SAVE-131 |
| Z2-I-SAVE-18 | cross-system failure scenario: restoring a consistent world | S4 | Z2-R-SAVE-050, Z2-R-SAVE-128, Z2-R-SAVE-132, Z2-R-SAVE-133, Z2-R-SAVE-134, Z2-R-SAVE-135, Z2-R-SAVE-157, Z2-R-SAVE-163, Z2-R-SAVE-164, Z2-R-SAVE-165, Z2-R-SAVE-169 |

The exact per-item mapping is `Zenith2/Docs/Requirements/SAVE.md` → "Coverage of inventory items" (over both catalogue files). Z2-I-SAVE-10 and -15 are served by Stage 2 requirements as well, because MOD's Stage 2 headless-equivalence test records and replays through SAVE's writer and reader (`Zenith2/Docs/Design/MOD.md` → "Device loss and headless").

### Document family

| File | Headings |
|---|---|
| `Zenith2/Docs/Design/SAVE.md` (this file) | the skeleton below (with the added sections "Asynchronous source capacity", "Cross-thread hand-offs" and "Scenario tests that consume SAVE"), plus "Cross-area notes" |
| `Zenith2/Docs/Design/SAVE/PublicAPI.md` | Scope of this file · Header list · Zenith_SaveTypes header · Zenith_SaveFormat header · Zenith_SaveStorage header · Zenith_SaveJournal header · Zenith_SaveParticipant header · Zenith_WorldSnapshot header · Zenith_SaveMigration header · Zenith_SaveSystem header · Zenith_SaveRegistration header · Runtime bridge declarations · Script-facing API |
| `Zenith2/Docs/Design/SAVE/ReplayAPI.md` | Scope of this file · Zenith_ReplayLog header |
| `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` | Scope of this file · Runtime bridge declarations · Synchronous restore driver (moved out of `PublicAPI.md` in revision 2, which keeps a pointer under its old heading) |
| `Zenith2/Docs/Design/SAVE/Formats.md` | Scope of this file · Conventions for every SAVE format · CRC-32C · The ZSAV file (File header, Section table, Section kinds) · Section payloads (Metadata section, Thumbnail section, World section, Entities section, Saved entity references, Events section, Async section, Resources section, Unknown carry section, Participant section framing) · The ZSJN journal · File names and directories (Last profile record) · Publication payloads · Replay log usage by SAVE (Records SAVE writes, Restored-world convention, Replay limits) · Migration fixture formats (State description file, Expected output file, Frozen fixture writers) · Format limits |
| `Zenith2/Docs/Design/SAVE/Algorithms.md` | Scope of this file · Request intake · Phase-11 snapshot (Snapshot capture) · Out-of-tick snapshot · Image assembly and compression · Journaled write sequence · Journal recovery · Slot policy · Autosave scheduling · Listing slots · Load pipeline (Script loads and the world host) · Verification and corruption recovery · Migration pipeline · Restore pipeline · Resume save and restore · Checkpoints · Persistent world and entity state · Profile switching · Replay recording · Replay playback · Replay mode · Replay seeking · Resuming a replay from a save · Replay verification · Shutdown sequence · State machines (Write request state machine, Journal state machine, Load operation state machine, Restore job state machine, Replay writer state machine) |
| `Zenith2/Docs/Design/SAVE/Tests.md` | Scope of this file · Test targets and locations · Format and codec tests · Journal and power-loss tests · Snapshot and restore tests · Migration and fixture tests · Save system tests · Replay tests · Runtime integration tests · Cross-system failure tests · Platform storage tests · Fuzz targets · Death tests · Benchmarks and budget harness · Gate steps per test group |

## Dependencies

### Libraries SAVE depends on

`ZenithSave` declares exactly its Architecture row ([Z2-N-ARCH-071]): `ZenithCore`, `ZenithReflect`, `ZenithECS`, `ZenithScene`, `ZenithPlatform`, `ZenithCompression`, `ZenithProfiling`.

| Library | Used for |
|---|---|
| `ZenithCore` | `Zenith_Result`, spans, string views, `Zenith_Heap`, `Zenith_Arena`, `Zenith_Owned`, `Zenith_Thread`, `Zenith_Semaphore`, `Zenith_BoundedQueue`, `Zenith_OperationTable` and handles, `Zenith_Sha256Digest`, `Zenith_PersistentID`, `Zenith_GetUtcTime`, the configuration schema, logging, the memory governor's consumer interface, the fault injector |
| `ZenithReflect` | `Zenith_TypeRegistry`, `Zenith_TypeInfo`, field sets and their binary codec, `Zenith_ReferenceResolver`, `Zenith_MigrateFieldSet`, value-kind hooks, phases, the script binding ABI for the `Save` module |
| `ZenithECS` | `Zenith_WorldSimulation` (`ApplyOutOfTick`, `RestoreSimulationState`, `HashTickState`, `GetInspector`), `Zenith_WorldInspector` (entities, components and, by row SV2, world resources), `Zenith_CommandBuffer` (`SpawnWithSimulationID`, the out-of-tick resource write of row SV2), the system registry, the tick-boundary handler interface |
| `ZenithScene` | the additive scene set and loader types the bridge participant reads (the participant itself lives in the bridge) |
| `ZenithPlatform` | `Zenith_AsyncIO`, `Zenith_VirtualFileSystem`, `Zenith_InputCommand`, `Zenith_HostFile` and `Zenith_CommandLineFileGrant`, `Zenith_ResumeIdentity` |
| `ZenithCompression` | `Zenith_CompressSection`, `Zenith_DecompressSection`, `Zenith_CompressionCodecSet`, the 24-byte section descriptor |
| `ZenithProfiling` | zones and counters |

`ZenithSave` deliberately does not depend on `ZenithScript` (the same layer, [Z2-N-ARCH-072]), `ZenithAssets` (asset references go through the asset value kind's hooks in `ZenithReflect`), `ZenithJobs` (SAVE schedules nothing and starts plain threads) or `ZenithDocument` (SAVE writes its own binary journal; cross-area row S17's offer is declined in row SV11). Every collaboration with a library above it goes through an interface SAVE declares (§ "Upward-callback interfaces SAVE declares").

### Headings this design relies on

| Document → heading | What SAVE takes from it |
|---|---|
| `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" | the `ZRPL` format, its enumerations and its rules [Z2-N-ARCH-783] to [Z2-N-ARCH-792] |
| `Zenith2/Docs/Architecture.md` → "Security architecture" | the save and replay parser limits; untrusted input ([Z2-N-ARCH-951]) |
| `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" | what a replay reproduces; [Z2-N-ARCH-884], [Z2-N-ARCH-885] |
| `Zenith2/Docs/Architecture.md` → "Memory governor" | the consumer interface; the Simulation shedding row (sheds nothing) |
| `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases" | phase 11 and [Z2-N-ARCH-477], [Z2-N-ARCH-503], [Z2-N-ARCH-504] |
| `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary" | the tick boundary steps; carried events ([Z2-N-ARCH-478]) |
| `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", "The frame loop" | where the bridge creates, attaches and commits |
| `Zenith2/Docs/Architecture/PlatformLayer.md` → "Process death and restore" | the resume file and identities |
| `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" | save migration ([Z2-N-CONTRACT-524] to [Z2-N-CONTRACT-526]), the journaled write ([Z2-N-CONTRACT-540], [Z2-N-CONTRACT-542], [Z2-N-CONTRACT-553]) |
| `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" | cancellation completion, bounded queues, shutdown ownership order |
| `Zenith2/Docs/Contracts.md` → "Errors and results" | error records ([Z2-N-CONTRACT-480]); script misuse ([Z2-N-CONTRACT-473]) |
| `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results" | publication at tick boundaries, completion IDs |
| `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptStateSnapshot header", "Zenith_ScriptHost header", "Zenith_ScriptAsync header" | the script state read and restore, the publication observer and source, the asynchronous result table |
| `Zenith2/Docs/Design/SCRIPT/HotReload.md` → "Behaviour state in saves" | the moments SAVE reads and restores behaviour state |
| `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_FieldSet header", "Binary field-set record", "Value-kind hooks" | capture, encode, decode and apply; asset references |
| `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldSimulation header", "Zenith_WorldInspector header", "Zenith_CommandBuffer header" | the snapshot reads and the restore writes |
| `Zenith2/Docs/Design/ECS/Scheduling.md` → "Out-of-tick playback", "World state hash" | restore ordering; the tick hash |
| `Zenith2/Docs/Design/COMPRESS.md` → "Compressed section descriptor on disk", "How cooked formats embed the descriptor", "Memory per context" | section compression |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md` → "Zenith_CompressedSection.h", "Zenith_CompressionCodecSet.h" | the section functions |
| `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API", "Atomic writes", "Cancellation", "Low-disk detection and behaviour", "Command-line host files" | the storage operations under SAVE's journal |
| `Zenith2/Docs/Design/PLAT/Android.md` → "Resume-state policy", "Resume write sequence", "Relaunch decision", "Resume identity" | the resume contract |
| `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation", "Replay and scripted input" | the input commands SAVE logs |
| `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256", "Zenith_PersistentID functions" | digests, persistent IDs |
| `Zenith2/Docs/Design/CORE/Threading.md` → "Threads", "Zenith_OperationHandle implementation" | the writer threads, operations |
| `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue" | the writer queues |
| `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks", "Configuration store", "Simulation-affecting list" | UTC time, keys, replay configuration |

### Upward-callback interfaces SAVE declares

| Interface | Implemented by | Purpose |
|---|---|---|
| `Zenith_SaveParticipant` | the runtime bridge (script states, additive scenes); the PHYS, ANIM, AI and CINE modules (row SV17); any module whose simulation state lives outside reflected components and resources | world state outside reflection, the per-capture fidelity of that state, the asynchronous source kinds it re-issues at restore, and its readiness before the restored world's first tick (AN17) |
| `Zenith_SaveWorldHooks` | the runtime bridge, over the world's `Zenith_AsyncResultTable` | asynchronous state, completion IDs for engine autosaves |
| `Zenith_SaveCompletionSink` | the runtime bridge | allocate and complete `ASYNC_SOURCE_SAVE` results |
| `Zenith_SaveStorage` | `ZenithSave/Internal` over PLAT (production); the header-only `Tests/Support/Zenith_MemorySaveStorage.h` (power-loss tests) | every byte on disk |
| `Zenith_ReplaySink` | `ZenithSave/Internal` (host file), `Zenith_ReplayMemorySink` | replay bytes |
| `Zenith_SaveSlotListener` (Stage 4) | ONLINE's cloud engine (`Zenith_OnlineSaveListener`) | slot writes and deletes of the active profile, delivered on the main thread (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot listener"; A3 rows ON1 (c), ON16 (b)) |

Each is registered or passed by the composition root and unregistered before its implementer is destroyed ([Z2-N-ARCH-078]).

### Areas that consume SAVE

| Area | Uses | Row |
|---|---|---|
| MOD | `Zenith_ReplayLogWriter`/`Reader` under `--record`/`--replay`, the resume calls, the checkpoint request, the replay verifier's failure callback, `Zenith_GetRunningBuildID` | A5 blocker MB15; this design's § "Runtime bridge files" |
| PLAT | the resume write, `ReadResumeIdentity`, the restore; the input-command log; `Zenith_SimulationInputContexts` saved as a reflected world resource | P11, PB11, SV8 |
| SCRIPT | `GetStateSnapshot`/`RestoreSavedStates` called by the bridge participant, which saves each entry's three flags (COHERENT only when some entry has `m_bRunSuspended` or `m_bQuarantinedOrDisabled`, after SCRIPT's Q82 (b) patch Z2T-SCRIPT-114; an entry with no committed checkpoint saved with a zero-length blob); the Stage 3 adapters implementing SCRIPT's publication observer and source; the `SAVE_REQUEST` consumer, which allocates each save request's `ASYNC_SOURCE_SAVE` ID; the live-completion discard rule in replay mode | SC6, SC16, SB12, SC25, SV3, SV23 |
| ECS | the phase-11 snapshot buffer resource and system; `RestoreSimulationState` before `ApplyOutOfTick`; carried events saved; world resources read and restored | E17, E21, E25, SV2 |
| RHI | the device-loss clean exit's autosave, drained at shutdown and never cancelled | R15, RB10, SV18 |
| COMPRESS | section descriptors in every save section | C5, C11 |
| SCENE | the additive set participant; asynchronous scene-load publications are logged like any other | S10, S17 |
| STREAM | cell deltas saved as components; no participant and no exclusions (STREAM's row ST14) | SV10 |
| ENV | `Zenith.Environment.State` saved as a reflected world resource (Z2-R-ENV-099) | SV17 |
| PHYS, ANIM, AI, CINE | participants for state outside reflection; per-capture fidelity (a COHERENT physics backend reports COHERENT); the re-issue declaration of a participant that owns an asynchronous source (ANIM: `ASYNC_SOURCE_ANIMATION`); restore readiness (ANIM's `IsRestoreReady` and `ListRestoreWaits`) | SV17, AN17 |
| GAME | the load-from-save transition: the prepared save, the restore job (stepped until DONE, through PARTICIPANTS_READY), the two completion IDs of a `Save.Load` and the linked-ID refusal path, the replay reader's lookahead and `FindPublication`, `Zenith.Runtime.WorldState` saved as a reflected world resource, the checkpoint lease released before the old world is unbound | SV7, GA11 |
| NET | the `ZRPL` input-command block as the loopback command record; its participant section `Zenith.Net.Identity` version 1, the only NET data in a save (a server saves its world as any world, a client excludes its proxies), whose capture is COHERENT while a session is live and whose restore resets the apply tables (orchestrator ruling NET-R1-RULINGS F7): ordinary participant fidelity, no SAVE format change | Architecture's loopback proof; NT22 |
| ONLINE | slot metadata and sequence for the cloud-save conflict model; profile switching on account change; from Stage 4 slot export and import, the slot listener, the binding profile and `SAVE_ERROR_PROFILE_MISMATCH`, the intake freeze of a profile switch, profile keys never in diagnostics, and the `saves/online/` reservation | SV12, ON1, ON16, ON22 |

### Assumed interfaces from other areas

<!-- assumption: pending ECS -->
SAVE assumes from ECS (cross-area row SV2): (a) a phase-11 system obtains the world's inspector through `Zenith_SystemContext::GetSnapshotInspector()`, valid only in `PHASE_SAVE_SNAPSHOT` systems, with the inspector's read-only rule; (b) `Zenith_WorldInspector::IsPendingDestroy(Zenith_EntityHandle)`; (c) `Zenith_WorldInspector::ListCarriedEvents(uint32 uFirst, Zenith_Span<Zenith_CarriedEventView>)` returning each carried event's type ID, merge key and payload in its type's layout, in delivery order; (d) `Zenith_CommandBuffer::RecordEvent(Zenith_TypeID, Zenith_ByteSpan xLayoutBytes, const Zenith_MergeKey&)` honoured out of tick, appending to the carried queue with the given key; (e) `Zenith_SystemBuilder::ReadAllComponents()` and `ReadAllResources()` for the snapshot system; (f) tick t's delivered-event and played-back-command logs retained until tick t + 1's boundary begins, so `HashTickState` of tick t can run there and out of tick before the next frame; (g) `Zenith_CommandBuffer::SpawnWithSimulationID` as Z2T-ECS-063 adds it; (h) `Zenith_WorldInspector::ListResources(Zenith_Span<Zenith_TypeID> axOut) const` (every world resource of the world, ascending type ID, returning the count) and `ReadResource(Zenith_TypeID, Zenith_MutableByteSpan) const` (the resource gathered into its reflected layout, containers as `Zenith_ContainerRef`, as `ReadComponent` does); (i) `Zenith_CommandBuffer::SetResource(Zenith_TypeID, Zenith_ByteSpan xLayoutBytes)`, honoured out of tick only, writing every property of the world's resource of that type from a value in its reflected layout (SAVE builds that value from the resource's current value with the saved serialized properties applied, so transient properties keep the value their owner set).
<!-- /assumption -->

<!-- assumption: pending SCRIPT -->
SAVE assumes from SCRIPT (rows SV3 and SV23, the second answered by SCRIPT's row SC25; the Q82 (b) rules are core text since cross-doc item X267 ([Z2-N-SCRIPT-274], [Z2-N-SCRIPT-928] to [Z2-N-SCRIPT-930]), as SAVE reads them in `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations": a recovery invocation consumes no invocation ordinal and records under a reserved recovery merge-key namespace, `Zenith_ScriptSavedState` gains `m_bRunSuspended`, `m_bRecoveryPending` and `m_bQuarantinedOrDisabled`, and an instance with no committed checkpoint is an entry with an empty checkpoint, implemented by ticket Z2T-SCRIPT-114): `Zenith_AsyncResultTable` gains `ListUnpublished`, `ListInFlight`, `RestoreUnpublished` and `RestoreInFlight` (out of tick); `AllocateCompletionID` is also callable on the world's phase-11 snapshot node, for engine autosaves only, which the frame structure serialises after every merge node of the tick and before the next tick boundary; an `ASYNC_SOURCE_SAVE` result has no awaiting instance (scripts observe it through events), so the table releases it, and returns its in-flight capacity, when its publication handler returns; and a zero-length `PUBLICATION_KIND_GENERIC_RECORDED` payload is the "abandoned at a restore" publication every source kind's decoder maps to its own failure (§ "Async section" of `Zenith2/Docs/Design/SAVE/Formats.md`).
<!-- /assumption -->

<!-- assumption: pending SCENE -->
SAVE assumes from SCENE (row SV4): `Zenith_AdditiveSceneSet` exposes its instance records for capture (`ForEachInstance`, including a LOADING or QUEUED instance's in-flight `ASYNC_SOURCE_SCENE_LOAD` completion ID) and a restore entry `RestoreInstances(Zenith_Span<const Zenith_SavedSceneInstance>, uint32 uNextInstanceID)` out of tick, which re-requests each LOADING instance under the recorded completion ID `Zenith_SavedSceneInstance::m_ulCompletionID` without allocating a new one (row SV4 (e)), and marks an instance ACTIVE only after every entity of it exists; SCENE's handler ignores a publication of an ID no instance waits for. The activation hook tag a re-requested instance carries (`Zenith_AdditiveLoadDesc::m_ulHookTag`) is STREAM's pending ask of SCENE (`Zenith2/Docs/Design/STREAM/PublicAPI.md`), not yet a SCENE member; SAVE stores it and passes it back unchanged.
<!-- /assumption -->

SAVE's composition is core text since cross-doc items X199 to X201 (rows SV1, SV5 to SV9, SV24): the restored-world convention of `Zenith2/Docs/Design/SAVE/Formats.md` → "Restored-world convention" is the `REPLAY_RECORD_WORLD_BEGIN` row of `Zenith2/Docs/Architecture.md` → "Fixed ticks and replayable commands", beside the tick-rate identity field of [Z2-N-ARCH-784]; startup step 11 creates the save system after the codec set and maps its failure to exit code 4 (under `--replay` without reading `saves/last_profile.zslp`), and step 17 runs no resume decision under `--replay`, while under `--record` a successful resume closes the step-16 session with `SwapWorld` and logs the restored world with `BeginWorld` and `Zenith_RuntimeReplay_LogRestoredWorld` (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay recording"); step 14 registers the `SAVE_REQUEST` consumer and the `Save.Request` command kind; step 16 and prologue step 7 attach SAVE to each world after every module attached; frame-loop step 3 calls `Zenith_ReplayLogWriter::CommitFrame`; the device-loss clean exit requests its autosave after the last `RunFrame` returns; shutdown step 4 begins SAVE's drain (on that exit the exit autosave is waited for at most 6 s from its submission and then abandoned with a 1 s rollback wait, inside the 10 s of [Z2-N-CONTRACT-484]; it is never cancelled by `BeginShutdown`) and step 7b destroys the save system before `Zenith_AsyncIO`; Contracts' shutdown table names the two SAVE threads (`Zenith.ReplayWrite` at step 4, `Zenith.SaveWriter` at step 7b). The startup steps are `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" (steps 11, 14, 16 and 17), the frame-loop steps and the exit autosave `Zenith2/Docs/Architecture/CompositionRoot.md` → "The frame loop" (steps 1, 3 and 4, [Z2-N-ARCH-880]), `RegisterSaveParticipant` `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" ([Z2-N-ARCH-879]), the shutdown rows `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" (steps 4 and 7b), and the step-17 replay rule MOD's `Zenith2/Docs/Design/MOD.md` → "Step construction notes" (row SV24).

<!-- assumption: pending GAME -->
SAVE assumes from GAME (row SV25): a save-source transition record in `ACTIVATING` whose restore job has not reached DONE within `runtime.startupLoadTimeoutMs` of entering `ACTIVATING` fails `RESTORE` through GAME's failure procedure (the isolated world destroyed, the linked world-load ID completed with the failure payload), logging `runtime.world: world <tag>: save restore not ready after <ms> ms (participant <key>)` with `Zenith_WorldRestoreJob::GetFirstUnreadyParticipantKey()`. Until GAME adopts it, a frame-driven restore that never becomes ready is bounded only by a cancel request on the transition's ticket (§ "Failure handling").
<!-- /assumption -->

## Library and file layout

### ZenithSave target

```cmake
# Zenith2/Engine/ZenithSave/CMakeLists.txt
zenith_add_library(ZenithSave
	DEPENDS ZenithCore ZenithReflect ZenithECS ZenithScene ZenithPlatform ZenithCompression ZenithProfiling
	TEST_DEPENDS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT ZenithCompression_Zlib ZenithCompression_Zstd)
```

| Path (under `Zenith2/Engine/ZenithSave/`) | Contents |
|---|---|
| `Zenith_SaveTypes.h`, `Zenith_SaveFormat.h`, `Zenith_SaveStorage.h`, `Zenith_SaveJournal.h`, `Zenith_SaveParticipant.h`, `Zenith_WorldSnapshot.h`, `Zenith_SaveMigration.h`, `Zenith_SaveSystem.h`, `Zenith_SaveRegistration.h` | the public headers of `Zenith2/Docs/Design/SAVE/PublicAPI.md`; `Zenith_ReplayLog.h`, that of `Zenith2/Docs/Design/SAVE/ReplayAPI.md` |
| `Internal/Zenith_SaveCrc32c.cpp` | CRC-32C, slicing-by-8, constexpr tables |
| `Internal/Zenith_SaveTypes.cpp` | slot names, profile keys, `Zenith_ToString` |
| `Internal/Zenith_SaveHeader.cpp`, `Internal/Zenith_SaveImageBuilder.cpp`, `Internal/Zenith_SaveImageView.cpp`, `Internal/Zenith_SaveMetadata.cpp` | the container |
| `Internal/Zenith_SaveJournal.cpp`, `Internal/Zenith_SaveRecovery.cpp` | the journal and recovery |
| `Internal/Zenith_AsyncIOSaveStorage.h`, `.cpp` | the production storage over PLAT |
| `Internal/Zenith_SaveReferenceResolver.cpp`, `Internal/Zenith_WorldSnapshotter.cpp`, `Internal/Zenith_SaveSectionIO.cpp` | capture and the section reader and writer |
| `Internal/Zenith_WorldRestoreJob.cpp` | restore |
| `Internal/Zenith_SaveMigration.cpp`, `Internal/Zenith_SaveFormatMigrations.cpp` | the format chain (empty step table at 1.0) |
| `Internal/Zenith_SaveSystem.cpp`, `Internal/Zenith_SaveWorldBinding.h`, `.cpp`, `Internal/Zenith_SaveIntake.cpp`, `Internal/Zenith_SaveWriter.cpp`, `Internal/Zenith_SaveSlots.cpp`, `Internal/Zenith_SaveLoad.cpp`, `Internal/Zenith_SaveListing.cpp`, `Internal/Zenith_SaveProfiles.cpp` | the service and the writer thread |
| `Internal/Zenith_ReplayLogWriter.cpp`, `Internal/Zenith_ReplayLogReader.cpp`, `Internal/Zenith_ReplayVerifier.cpp`, `Internal/Zenith_ReplayHostFileSink.cpp`, `Internal/Zenith_ReplayMemorySink.cpp` | replays |
| `Internal/Zenith_SaveRegistration.cpp`, `Internal/Zenith_SaveBindings.cpp` | types, keys, the `Save` binding module |
| `Internal/Zenith_SaveDiagnostics.h`, `.cpp` | counters, zones, log categories, the `save.dump` command |
| `Internal/Windows/Zenith_BuildID_Windows.cpp` | the PE CodeView RSDS GUID of `__ImageBase` (the module the function is linked into: the game executable, which links the engine statically) |
| `Internal/Android/Zenith_BuildID_Android.cpp`, `Internal/Linux/Zenith_BuildID_Linux.cpp` | the ELF object that contains `Zenith_GetRunningBuildID` itself: `dladdr(reinterpret_cast<const void*>(&Zenith_GetRunningBuildID), &xInfo)` gives that object's load base `dli_fbase`; `dl_iterate_phdr` visits every loaded object and selects the one whose `dlpi_addr` plus the `p_vaddr` of its first `PT_LOAD` segment equals `dli_fbase` (on Android the game's `.so` inside `app_process64`, never the zygote executable `dl_iterate_phdr` reports first); its `PT_NOTE` segments are walked for the note of type `NT_GNU_BUILD_ID` (3) and name `"GNU"`, and the first 16 bytes of its descriptor are the ID (a shorter descriptor is zero-padded) |
| `Tests/` | `Zenith2/Docs/Design/SAVE/Tests.md` → "Test targets and locations" |

<!-- elaboration -->
The build ID is the linker's own image identity (the CodeView RSDS GUID on Windows, the `NT_GNU_BUILD_ID` note on ELF platforms) rather than a generated constant, so a rebuilt executable never carries a stale ID; reading it from the object that contains the function, not from the process's first object, is what makes it the game's ID on Android, where the game is a shared object loaded into `app_process64`. The function is stateless and never logs: `Zenith_RuntimeSave_CreateSystem` calls it once at startup step 11, keeps the value in the bridge's state object and logs `save.replay: executable has no build ID` there when it is all zero, so no static flag is needed ([Z2-N-CONTRACT-141]).

### Runtime bridge files

<!-- elaboration -->
SAVE owns these files inside `Zenith2/Engine/ZenithRuntime/Internal/Save/` (MOD owns the directory and the runtime's startup, frame-loop and shutdown files that call them, as GAME owns `ZenithRuntime/Internal/Zenith_WorldHost.cpp`):

| File | Stage | Contents |
|---|---|---|
| `Zenith_RuntimeReplayCore.h`, `Zenith_RuntimeReplayCore.cpp` | 2 | `Zenith_RuntimeReplayCoreRecorder` and `Zenith_RuntimeReplayCorePlayer`: `WORLD_BEGIN`/`WORLD_SWAP` records, each tick's input commands and tick hash, the verifier wiring, the replay clock cap; they implement only ECS's `Zenith_TickBoundaryHandler` and include no `ZenithScript` header, so MOD's Stage 2 headless-equivalence test (Z2-R-MOD-074, ticket Z2T-MOD-029) records and replays through them |
| `Zenith_RuntimeReplayScript.h`, `Zenith_RuntimeReplayScript.cpp` | 3 | `Zenith_RuntimeReplayPublicationObserver` (implements `Zenith_ScriptPublicationObserver`, forwards each tick's publications to the core recorder's writer) and `Zenith_RuntimeReplayPublicationSource` (implements `Zenith_ScriptPublicationSource` over the core player's reader); the world-halt forwarding; `Zenith_RuntimeReplay_SeekTo` and `Zenith_RuntimeReplay_ResumeFromSave` |
| `Zenith_RuntimeSave.h`, `Zenith_RuntimeSave.cpp` | 3 | creation at startup step 11, world attach (bind, participants, the `Save.Snapshot` and `Save.EmitResults` systems, the source declaration of § "Asynchronous source capacity"), the completion sink, the publication handler and its per-world result list, the `SAVE_REQUEST` consumer, the `Save.Request` command kind, the prologue pump, the synchronous restore driver, the device-loss and thermal entry points, shutdown |
| `Zenith_RuntimeSaveParticipants.cpp` | 3 | `Zenith_ScriptStateSaveParticipant`, `Zenith_AdditiveScenesSaveParticipant`, `Zenith_RuntimeSaveWorldHooks` |
| `Zenith_RuntimeSaveLoad.cpp` | 4 | the hand-off of a finished script `Save.Load` to GAME's world host (`Zenith_WorldHost_BeginTransitionFromSave`) and its replay form, with GAME's save-load transition |

Their declarations are `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations".

The Stage 2 ticket boundary with Z2T-MOD-029: SAVE's Stage 2 bridge ticket lands `Zenith_RuntimeReplayCore.*` and its tests; MOD's ticket calls `Zenith_RuntimeReplayCore_Create` from the runtime's `--record`/`--replay` handling and owns `Test_Runtime_HeadlessMatchesWindowedSimulation`. At Stage 3 the script adapters attach to the core objects without changing their Stage 2 behaviour: a log recorded before Stage 3 simply carries no publications.

### Asynchronous source capacity

<!-- elaboration -->
The bridge declares `DeclareSource(ASYNC_SOURCE_SAVE, uSaveMaxOutstanding, uSAVE_MAX_PUBLICATION_BYTES)` on each world's table, with `uSaveMaxOutstanding = 4 × save.maxQueuedRequests + 2 × save.maxRequestsPerTick + 3` (43 with the defaults Q = 8, R = 4), derived per [Z2-N-CONTRACT-342] from the most SAVE completion IDs of one world that can be allocated and not yet published at once. A SAVE result is released when its handler returns (row SV3), and nothing of W publishes between two of W's tick boundaries, so at any instant the unpublished IDs are those **not completed at W's last boundary** plus those **admitted since it**:

| Part | Holder | Bound | Why |
|---|---|---|---|
| not completed at the last boundary | pending writes (queued, not yet captured; the engine autosave, once allocated, is one of them) | Q | intake step 8 completes a request at once when the pending list is full, and the autosave of phase-11 step 1 is appended inside that limit or completed QUEUE_FULL |
| | writes inside captured jobs, not yet completed | 2Q | two capture arenas, one job each, a job taking at most the whole pending list |
| | loads, listings and deletes: the operation queue plus the running one | Q + 1 | the queue's capacity Q, plus the one operation the writer popped |
| admitted before the next boundary | script requests | R | SCRIPT's capacity class Z2-B-SCRIPT-025 admits at most R per tick, whatever intake then does with them, and the bridge's consumer allocates exactly one `ASYNC_SOURCE_SAVE` ID for each admitted request on the committing merge node (the metadata and autosave-flag records carry no capacity demand and allocate no ID) |
| | C++ requests with their own ID | R | intake step 4 refuses every C++ request above R before it allocates an ID |
| | the tick's limit batch | 1 | one ID for every over-limit C++ request of the tick |
| | the engine autosave of this tick's phase 11 | 1 | at most one per tick (and none while one is outstanding, § "Autosave scheduling" of `Zenith2/Docs/Design/SAVE/Algorithms.md`) |

Total Q + 2Q + (Q + 1) + R + R + 1 + 1 = 4Q + 2R + 3. A request admitted and completed in the same window (an intake refusal) is counted once, among the admitted; a released arena re-captured in the same window does not exceed the bound, because only one phase 11 of W runs per window and the requests it captures were already counted as pending or admitted. Out-of-tick submissions, the resume write, the exit autosave and replay checkpoints allocate no completion ID. Intake also keeps the world's own deterministic count of SAVE completion IDs allocated and not yet published (`m_uOutstanding`: incremented at intake in merge order, decremented by the publication handler at the tick boundary, both recorded, so a run and its replay agree) and refuses before allocating when it equals `uSaveMaxOutstanding`; by the table that refusal is unreachable, so it is counted as a defect (`save.requests.unpublishable`) and `Test_SaveSystem_OutstandingCapacityBound` saturates every holder at once, with more C++ commands in one tick than the limit, and asserts every request still publishes.

The capacity is declared from the running build's Q and R, which a game may lower between releases. A save written under larger values can hold more `ASYNC_SOURCE_SAVE` entries (unpublished plus in flight) than the restoring world's `uSaveMaxOutstanding`; `Zenith_SaveWorldHooks::RestoreAsyncState` then fails the restore with `SAVE_ERROR_RESTORE`, `m_ulDetail` = the saved SAVE-entry count, and logs `save: restore refused: <n> ASYNC_SOURCE_SAVE entries in flight exceed this build's limit <m> (save.maxQueuedRequests, save.maxRequestsPerTick)` (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section").

### Test and fuzz targets

`ZenithSave_Tests` (every unit group), four fuzz targets `Zenith_Fuzz_SaveImage`, `Zenith_Fuzz_SaveJournal`, `Zenith_Fuzz_ReplayLog`, `Zenith_Fuzz_SaveMetadata`, death cases registered by the test registry, and the benchmark `Zenith_Bench_Save` with its smoke; locations and links in `Zenith2/Docs/Design/SAVE/Tests.md` → "Test targets and locations".

### Integration suites

`Zenith2/Tests/SaveRuntime/` (Stage 3: script requests, resume, device-loss autosave, record and replay; Stage 4: loads through GAME's save-load transition, seeking, resumed replays) and `Zenith2/Tests/SaveCrossSystem/` (Stage 4: the cross-system failure scenarios), each one `zenith_add_integration_tests` call. The Stage 2 bridge's own cases live in `Zenith2/Engine/ZenithRuntime/Tests/Save/Zenith_RuntimeReplayCore_Tests.cpp`, inside `ZenithRuntime_Tests`.

### Supported matrix

`ZenithSave` declares the default `ZENITH_ALL` (`win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`), as `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix" lists for the CPU-only engine libraries. The build ID readers are per platform in `Internal/Windows|Android|Linux/`. The integration suites follow `ZenithRuntime`'s `ZENITH_GPU` matrix narrowed to host presets.

## Public API

### API rules

- Every declaration is in `Zenith2/Docs/Design/SAVE/PublicAPI.md` (and, for `Zenith_ReplayLog.h`, `Zenith2/Docs/Design/SAVE/ReplayAPI.md`) with its contract block; fallible functions return `Zenith_Result<T, Zenith_SaveError>` or `Zenith_Result<T, Zenith_ReplayError>`.
- `Zenith_SaveSystem` is a concrete class created by its own `Create` (it is not a replaceable subsystem with backends, so it has no interface and no factory family, `Zenith2/Docs/Contracts.md` → "API boundary").
- The only virtual seams are the six interfaces of § "Upward-callback interfaces SAVE declares".
- No public header includes a third-party header; `<atomic>` is the one standard header beyond CORE's (an allowed header).

### Header summary

| Header | Purpose | Stage |
|---|---|---|
| `Zenith_ReplayLog.h` | replays, build ID | 2 |
| `Zenith_SaveTypes.h` | versions, kinds, errors, slots, profiles | 2 (`Zenith_ReplayLog.h` includes it) |
| `Zenith_SaveFormat.h` | the container, CRC-32C | 3 |
| `Zenith_SaveStorage.h`, `Zenith_SaveJournal.h` | storage and the journaled write | 3 |
| `Zenith_SaveParticipant.h`, `Zenith_WorldSnapshot.h` | capture and restore | 3 |
| `Zenith_SaveMigration.h` | the format chain | 3 |
| `Zenith_SaveSystem.h` | the service | 3 |
| `Zenith_SaveRegistration.h` | types, keys, bindings | 3 |

### Handle kinds used

| Handle kind (`Zenith2/Docs/Contracts.md` → "Handle kinds") | How SAVE uses it |
|---|---|
| persistent IDs | serialized as 16 bytes; the save-local reference form is not a persistent ID and never leaves a save |
| world handles (entities, components) | never serialized: references become persistent IDs or save-local simulation-ID references, and restore maps them back |
| shared asset handles | serialized as the asset's persistent ID through the value-kind hooks |
| operation handles | every C++ write, load, listing and delete (`SubmitOutOfTick`, `SubmitResumeWrite`, `SubmitExitAutosave`, `BeginLoad`, `BeginLoadPath`, `BeginListSlots`, `BeginDelete`) and, from Stage 4, every export and import (`BeginExportSlot`, `BeginImportSlot`) is a `Zenith_OperationHandle` from SAVE's `Zenith_OperationTable` (capacity `save.maxQueuedRequests + 8`); results are taken through `TakePreparedLoad`, `TakeSlotList` and `TakeExportedSlot`. Script requests and engine autosaves carry completion IDs instead and are observed through publications |
| device handles | none |

### Configuration keys

<!-- elaboration -->
Registered by `Zenith_RegisterSaveConfigKeys` at startup step 6 (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"):

| Key | Type | Default | Range | Allowed layers | Restart | Simulation-affecting |
|---|---|---|---|---|---|---|
| `save.compressionCodec` | enum `none`, `zlib`, `zstd` | `zstd` | — | default, platform, game | no | no |
| `save.compressionLevel` | int | 3 | 1 … 9 (the range both save codecs accept: zlib's 1 … 9 is the narrower; zstd's 10 … 19 cost seconds per save and are never wanted) | default, platform, game | no | no |
| `save.imageArenaBytes` | int | 67,108,864 (Windows), 33,554,432 (the `android` platform layer) | 4 MiB … 256 MiB | default, platform, game | yes | no |
| `save.maxQueuedRequests` | int | 8 | 1 … 32 | default, game | yes | no |
| `save.maxRequestsPerTick` | int | 4 | 1 … 16 | default, game | yes | yes |
| `save.autosaveIntervalTicks` | int | 0 (off) | 0 … 2,147,483,647 | default, game, user | no | yes |
| `save.autosaveSlotCount` | int | 3 | 1 … 10 | default, game | no | no |
| `save.shutdownDrainMs` | int | 5,000 | 0 … 30,000 | default, platform | no | no |
| `save.restoreBudgetUs` | int | 4,000 | 500 … 100,000 | default, platform | no | no |
| `save.retainCheckpoint` | bool | true | — | default, platform, game | no | no |
| `save.thumbnails` | bool | true | — | default, platform, game, user | no | no |
| `replay.checkpointIntervalTicks` | int | 0 | 0 … 2,147,483,647 | default, command line | yes | no |
| `replay.flushIntervalMs` | int | 1,000 | 100 … 60,000 | default, command line | no | no |
| `replay.maxLogBytes` | int | 536,870,912 | 16 MiB … 4 GiB | default, command line | no | no |

`save.maxRequestsPerTick` and `save.autosaveIntervalTicks` change which requests a tick admits and when autosaves happen, so they are simulation-affecting and written into every replay header ([Z2-N-ARCH-930]).

- `save.maxRequestsPerTick` is the value of Z2-B-SCRIPT-025 "Save-request capacity per tick" (hard, BR-MERGE): SCRIPT's merge accepts or rejects script save requests against it as the capacity class `SCRIPT_CAPACITY_CLASS_SAVE_REQUESTS` (`Zenith2/Docs/Design/SCRIPT/Transactions.md` → "Capacity acceptance across ECS and SCRIPT classes"), and SAVE's intake applies the same number to C++ `Save.Request` commands (`SAVE_ERROR_REQUEST_LIMIT`). Until the Stage 1 budget ticket for SCRIPT adopts the row's value, the key's default 4 is the provisional value; when the row is adopted, the key's default becomes the row's value in the same diff (cross-doc item X202 records the Budgets half).
- Lowering `save.maxQueuedRequests` or `save.maxRequestsPerTick` across releases lowers the declared `ASYNC_SOURCE_SAVE` capacity (§ "Asynchronous source capacity"), so a save written by an earlier release with more SAVE results in flight than the new capacity is unloadable (`SAVE_ERROR_RESTORE`, both keys named in the log); a game lowers either only when its saves cannot hold that many.
- On Android the save system clamps the effective level to `iSAVE_ANDROID_MAX_COMPRESSION_LEVEL` (3), COMPRESS's per-context memory guidance for the S24 (`Zenith2/Docs/Design/COMPRESS.md` → "Memory per context"), and `Zenith_SaveSystem::Create` logs the clamp once when the configured level is higher (the save system is created once, so the log needs no flag). Compressed bytes are never part of a determinism comparison, so the clamp changes no replay.

## Data structures and formats

### In-memory structures

<!-- elaboration -->
| Structure | Owner | Contents | Size |
|---|---|---|---|
| world binding (`Internal/Zenith_SaveWorldBinding.h`) | `Zenith_SaveSystem`, one per bound world | world pointer, hooks, participants (registration order, at most 32) and the union of their declared re-issue kinds, pending request list (`save.maxQueuedRequests` entries) and its intake arena of `save.maxQueuedRequests × 12 KiB` (at most 12 KiB copied per request: a 128-byte display name and 29 maximal metadata pairs fit), request serial counter (`uint32`), the C++ request count of the current tick, the tick's limit batch (its completion ID, refused count, first 4,000 serials: 16 KiB), `m_uOutstanding` (SAVE completion IDs allocated and not yet published), the outstanding autosave completion ID (0 for none), autosave flag, the per-tick metadata list (at most 29 pairs), playtime base and first tick, thumbnail copy (≤ 589,824 bytes; read only on the main thread and by the capture, which copies it into the capture arena), a pointer to the world's checkpoint holder, unknown carry, replay writer pointer, its own `Zenith_ReentrancyGuard`, and (Stage 4) the binding profile recorded at `BindWorld` | ~113 KiB with the defaults + the thumbnail |
| checkpoint holder (`Internal/Zenith_SaveWorldBinding.h`, `Zenith_SaveCheckpointHolder`) | `Zenith_SaveSystem` (a list of holders, not the binding), one per world that has had a CHECKPOINT | the pending and retained checkpoint slot pair (§ "Cross-thread hand-offs"), the main-thread lease count, a detached flag set by `UnbindWorld` while leased | 32 bytes + the retained checkpoints |
| capture arenas | the save system | three `Zenith_Arena`s, 1 MiB blocks, each capped at `save.imageArenaBytes`: indices 0 and 1 for ordinary captures, index 2 for the resume write and the exit autosave; each holds a capture's sections and its thumbnail copy; their availability is the atomic mask of § "Cross-thread hand-offs" | 0 … 3 × the cap (index 2 is used only while backgrounding or at a device-loss exit) |
| assembly arena | the writer thread | one arena: the compressed file plus compression scratch | ≤ the cap + scratch bound |
| load arena | the writer thread | the read file and the decoded image tree of one load | ≤ 2 × the cap |
| priority slot | the save system | a `Zenith_BoundedQueue<Zenith_SaveWriteJob>` of capacity 1, `QUEUE_FULL_POLICY_REJECT`, for the resume write or the exit autosave (whichever holds arena 2); the writer pops it first | 96 bytes |
| writer queue | the save system | `Zenith_BoundedQueue<Zenith_SaveWriteJob>`, capacity 2 (one capture job per ordinary arena), `QUEUE_FULL_POLICY_REJECT`; popped after the priority slot | 2 × 96 bytes |
| operation queue | the save system | `Zenith_BoundedQueue<Zenith_SaveOperationJob>` for loads, listings, deletes and (Stage 4) exports and imports, capacity `save.maxQueuedRequests`, `REJECT`; popped after the writer queue | |
| transfer arena (Stage 4) | the save system | one `Zenith_Arena` in 1 MiB blocks: the one running or untaken export's file bytes, or the one import's copied file (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot export and import"); owned by the writer while the operation runs and by the main thread from its completion until the bytes are taken or released | ≤ 256 MiB |
| slot notice queue (Stage 4) | the save system | `Zenith_BoundedQueue<Zenith_SaveSlotNotice>` (slot, kind, the written slot's info with its display name copied, or a deletion), capacity `2 × save.maxQueuedRequests + 2`; the writer waits rather than drops (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot listener"); drained by `PumpMainThread` | ≤ 66 × 224 bytes |
| profile state (Stage 4) | the save system | the active profile, the pending switch target (none, or a key), the slot listener pointer and its atomic notices-on flag; written on the main thread with no frame running | |
| wake semaphore | the save system | one `Zenith_Semaphore`, released once by every push to any of the three queues and by `BeginShutdown`; the writer thread waits on it with a 100 ms timeout and then tries the queues in priority order | |
| slot table | the writer thread | per slot of the active profile: name, kind, sequence, format version, size, state | ≤ 1,024 entries × 64 bytes |
| operation table | the save system | `Zenith_OperationTable`, `m_uMaxOperations = save.maxQueuedRequests + 8` | CORE's slot size |
| replay assemblers | `Zenith_ReplayLogWriter` | per world session: tick records awaiting their hash (at most 64 ticks), input commands, publications | bounded by the per-tick limits |

Z2-B-SAVE-001 "Save serialization buffers" is the sum of the capture, assembly, load and (Stage 4) transfer arenas, the retained checkpoints and the thumbnails (`GetResidentBytes` of the memory consumer).

### Formats summary

| Format | Magic, version | Specified in |
|---|---|---|
| save container | `ZSAV`, format 1.0 | `Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSAV file" |
| save journal | `ZSJN`, version 1 | `Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSJN journal" |
| replay log | `ZRPL`, version 1 (Architecture's) | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations"; SAVE's use in `Zenith2/Docs/Design/SAVE/Formats.md` → "Replay log usage by SAVE" |
| publication payloads | 64-byte header | `Zenith2/Docs/Design/SAVE/Formats.md` → "Publication payloads" |
| migration fixtures | `zenith2.save-fixture` 1, `zenith2.save-fixture-expected` 1 | `Zenith2/Docs/Design/SAVE/Formats.md` → "Migration fixture formats" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Section |
|---|---|---|
| request intake | O(1) per request | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Request intake" |
| phase-11 snapshot | O(entities × components) | "Phase-11 snapshot" |
| image assembly | O(bytes) | "Image assembly and compression" |
| journaled write | O(new + old bytes) | "Journaled write sequence" |
| journal recovery | O(journals × file bytes) | "Journal recovery" |
| listing | O(slots) prefix reads | "Listing slots" |
| load and verification | O(file bytes) | "Load pipeline", "Verification and corruption recovery" |
| migration | O(sections × steps) | "Migration pipeline" |
| restore | O(entities × components), budgeted per frame | "Restore pipeline" |
| replay record, play, seek, verify | O(ticks) | "Replay recording", "Replay playback", "Replay seeking", "Replay verification" |

### State machine index

Write request, journal, load operation, restore job and replay writer: `Zenith2/Docs/Design/SAVE/Algorithms.md` → "State machines".

### Tick walkthrough of SAVE work

1. **Prologue step 7** (main thread): `Zenith_RuntimeSave_PumpMainThread` promotes finished retained checkpoints and hands finished script loads to GAME's world host; load-from-save restores step within `save.restoreBudgetUs` (GAME drives them); restored and swapped worlds are logged to the replay writer at the swap.
2. **Tick boundary** of each world: the recorder (or player) hashes the previous tick and records (or verifies) it; SCRIPT publishes `ASYNC_SOURCE_SAVE` results in completion-ID order; the bridge's handler appends each to the world's bridge-side result list and calls `Zenith_SaveSystem::OnResultPublished` (it writes no world state).
3. **Phase 1**: `Save.EmitResults` emits `Zenith.SaveCompleted`, `Zenith.SaveSlotListed` and `Zenith.SaveSlotListDone` events from the result list in completion-ID order (delivered in phase 9); PLAT's input system logs the tick's commands.
4. **Phases 2 and 10**: behaviours call `Save.*`; at their merge nodes the `SAVE_REQUEST` consumer allocates each request's `ASYNC_SOURCE_SAVE` completion ID and queues the requests in merge order; C++ `Save.Request` commands queue at sync-point playback.
5. **Phase 11**: `Save.Snapshot` decides a due autosave (allocating its completion ID), captures the pending requests into a free capture arena and hands the job to `Zenith.SaveWriter`; under `--record`, a due replay checkpoint is captured too (it allocates nothing and changes no world state).
6. **Asynchronously**: the writer thread assembles, compresses and writes through the journal, then completes each request through the sink (in replay mode it touches no storage and completes nothing; § "Replay mode" of `Zenith2/Docs/Design/SAVE/Algorithms.md`).
7. **Frame-loop step 3** (main thread): `CommitFrame` sends the frame's completed replay records to the sink.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_SaveSystem` | `ANY_THREAD_EXCEPT(…)` as its contract block lists: world and profile management on the main thread; intake on the world's merge and sync-point nodes; capture on its phase-11 node; `OnResultPublished` on its tick-boundary node. Its callbacks enter the guard of the context they run in: the world binding's guard for a world's nodes and its main-thread out-of-tick captures, the writer guard on `Zenith.SaveWriter`, the system guard for main-thread calls tied to no world, so no guard is entered by two threads at once |
| `Zenith_SaveStorage` and its users (journal, recovery, listing, loads) | `OWNER_THREAD`: `Zenith.SaveWriter`; the main thread only while the writer holds no job and every queue is empty (`Create` before the thread starts, `SetActiveProfile`, whose `SAVE_ERROR_PROFILE_BUSY` precondition guarantees it, and `ReadResumeIdentity` at startup step 17, before any request exists) |
| `Zenith_WorldSnapshotter` | `OWNER_THREAD`: the phase-11 node, or the main thread out of tick |
| `Zenith_WorldRestoreJob`, `Zenith_PreparedSave`, `Zenith_RetainedCheckpointLease` | `MAIN_THREAD`, no frame of the world running (a frame-driven job in prologue step 7, or the synchronous restore driver of `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Synchronous restore driver") |
| `Zenith_ReplayLogWriter` | `ANY_THREAD_EXCEPT(Create, BeginWorld, SwapWorld, CommitFrame, Close)`; `AddCheckpoint` on the phase-11 node or on the main thread out of tick |
| `Zenith_ReplayLogReader` | `ANY_THREAD` after `Open` |
| `Zenith_ReplayVerifier` | `ANY_THREAD_EXCEPT(Create)` |

### Threads SAVE owns

| Thread | Started by | Joined by | Blocks on |
|---|---|---|---|
| `Zenith.SaveWriter` (priority `THREAD_PRIORITY_BELOW_NORMAL`, 1 MiB stack) | `Zenith_SaveSystem::Create` | the save system's destructor, shutdown step 7b | the wake semaphore (100 ms timeout; then `TryPop` on the priority slot, the writer queue and the operation queue, in that order), and the storage's completion polls (`Zenith_SleepUs(250)`) |
| `Zenith.ReplayWrite` (only with the host-file sink) | `Zenith_ReplayLogWriter::Create` | `Close`, shutdown step 4 | its chunk queue's `PopWait`, `Zenith_HostFile::Append` and `Flush` |

<!-- elaboration -->
Two dedicated threads rather than jobs: a backgrounded Android process pumps `WaitForEvents` and runs no frame, so the resume write cannot ride the job system, and blocking storage polls are legal only off job workers ([Z2-N-CONTRACT-315]). Neither thread ever runs a job or touches world state. Both are rows of Contracts' shutdown table (row SV9, X201).

### Cross-thread hand-offs

<!-- elaboration -->
Every datum that crosses between the frame's nodes, the main thread and `Zenith.SaveWriter` moves through one of these, and nothing else is shared:

| Datum | Producer → consumer | Protocol |
|---|---|---|
| capture arena ownership | phase-11 nodes of any world, or the main thread out of tick → the writer thread → back | `std::atomic<uint32> m_uFreeArenaMask` (bit i set = arena i free; 0b111 at `Create`). Acquire: load (relaxed), pick the lowest set bit among the allowed indices (0 and 1 for ordinary captures, 2 for the priority slot), `compare_exchange_weak` clearing it with `memory_order_acquire`, retry on failure; no set bit → no arena (the capture is deferred, § "Phase-11 snapshot" of `Zenith2/Docs/Design/SAVE/Algorithms.md`). Release: the writer resets the arena, then `fetch_or(bit, memory_order_release)`, so the next acquirer sees the reset arena. Phase-11 nodes of different worlds run concurrently and contend only on this word |
| a capture job | the acquiring node → the writer | `TryPush` into the writer queue (or the priority slot) after the capture filled the arena; the queue's own release/acquire hand-off publishes the arena's bytes; the push cannot fail because a job exists only for an acquired arena and each queue holds one job per arena it serves |
| a pending request list | merge nodes and sync points of W → W's phase-11 node | W's binding only; the frame graph orders them (no two run concurrently for one world); the writer never reads it |
| completions | the writer, and W's merge, sync-point and phase-11 nodes → the world | the sink's `Complete` into W's own SCRIPT table, which publishes at W's next tick boundary ([Z2-N-CONTRACT-093]); calls for different worlds and from the writer run concurrently, each into its own world's table, and each enters its own guard (the world binding's, or the writer guard), never a shared one |
| the thumbnail | the main thread (`SetThumbnail`, between frames) → the writer | the capture copies it into the capture arena (§ "Phase-11 snapshot" step 10 of `Zenith2/Docs/Design/SAVE/Algorithms.md`), so the writer reads only the arena's copy, published by the writer queue's hand-off; the binding's copy is touched by the main thread between frames and by W's phase-11 node, which never overlap |
| `m_uOutstanding`, the outstanding autosave ID | intake and phase 11 of W ↔ the tick-boundary handler of W | W's binding only, frame-ordered; the writer never touches them |
| the retained checkpoint | the writer → the main thread | each binding has `std::atomic<Zenith_PreparedSave*> m_pxPendingCheckpoint` and a main-thread-only `Zenith_Owned<Zenith_PreparedSave> m_pxRetainedCheckpoint` with a lease count. After a CHECKPOINT write succeeds the writer builds a new `Zenith_PreparedSave` in its own arena and `exchange`s it into the pending slot (`memory_order_acq_rel`), destroying any pending one it displaced (never leased, so never borrowed). `PumpMainThread` (prologue step 7) `exchange`s the pending slot with null (`memory_order_acquire`) and promotes it to retained only when the lease count is 0; while a lease is held, it puts the object back into the pending slot (CAS from null; if the writer filled it meanwhile, the newer one wins and the older is destroyed on the main thread). `AcquireRetainedCheckpoint` returns a `Zenith_RetainedCheckpointLease` that increments the count and exposes `Zenith_PreparedSave&` (non-const, so `BeginWorldRestore` works); its destructor decrements. A restore job borrowing a leased checkpoint therefore never sees it destroyed. The slot pair and the lease count live in the world's checkpoint holder, owned by the save system: `SetActiveProfile` is refused while any lease is alive, and `UnbindWorld` of a leased world detaches the holder (destroyed by the last lease's destructor) instead of destroying it, so neither a profile switch during GAME's "retry from checkpoint" nor the old world's unbind at the retry's swap can free the checkpoint or the memory the lease points at (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Checkpoints") |
| slot notices (Stage 4) | the writer → the main thread | the notice queue's own release/acquire hand-off; the writer pushes only while the notices-on flag (`std::atomic<bool>`, stored with `memory_order_release` by `SetSlotListener` and `BeginShutdown`, loaded with `memory_order_acquire`) is set; `PumpMainThread` releases the wake semaphore after popping, which ends the writer's backpressure wait (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot listener") |
| the transfer arena (Stage 4) | the main thread ↔ the writer | an import's bytes are copied before its job is pushed (the queue's hand-off publishes them); an export's bytes are published by the operation's completion; the main thread releases the arena (take, handle destruction, profile switch) before a further export or import can be begun, so the two threads never hold it at once |
| statistics | every thread → any reader | relaxed atomics, read through `GetStats` |

### Frame-graph nodes SAVE runs

| Node | Subgraph, phase | Declared access | Affinity |
|---|---|---|---|
| `Save.Snapshot@W<tag>` (a phase-11 system registered by the bridge through `RegisterSnapshotSystem`) | TICK, phase 11 | `ReadAllComponents()`, `ReadAllResources()`; reads declared by each participant; write of `Zenith.SaveSnapshotBuffer` (owning phase 11, [Z2-N-ARCH-503], row E17) | any worker |
| `Save.EmitResults@W<tag>` (bridge) | TICK, phase 1 | `EmitsEvent` of the three `Zenith.Save*` events; it reads the bridge's own per-world result list (not a world resource), which only the world's tick-boundary handler writes, earlier in the same tick, so no declared access is needed (PLAT's SC20 interim pattern) | any worker |
| the `SAVE_REQUEST` consumer | the merge nodes of phases 2 and 10 | none of its own (it touches SAVE's world binding only) | any worker |
| the `Save.Request` command playback | sync-point playback step 7 | — | the sync point's |
| the publication handler (`Zenith_RuntimeSavePublicationHandler`) | the tick boundary node, tick-boundary step 1 | — (it writes no world state: the bridge list and SAVE's binding counters only) | the boundary's |
| recorder and player tick-boundary handlers | the tick boundary node | — | the boundary's |

### Lock-order entries

SAVE adds no `Zenith_Mutex`: its queues are `Zenith_BoundedQueue`s (no lock-order row, `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"), the arena mask, the pending-checkpoint slots, the cancel tokens and the statistics are atomics with the orders of § "Cross-thread hand-offs", and each world binding is touched only by nodes the frame graph orders. The operation table takes CORE's `uLOCK_LEVEL_OPERATION_TABLE` (380) inside `Issue` alone; SAVE holds no lock while calling `Zenith_AsyncIO` (whose queue lock is 610).

### Startup and shutdown

| Point | Action |
|---|---|
| step 6 | `Zenith_RegisterSaveTypes`, `Zenith_RegisterSaveConfigKeys` |
| step 11, after the codec set | `Zenith_RuntimeSave_CreateSystem`: `Zenith_GetRunningBuildID` (kept in the bridge state; an all-zero ID is logged once here), `Zenith_ReadLastSaveProfile` (the initial profile; not called under `--replay`, whose initial profile is `local`), `Zenith_CreateAsyncIOSaveStorage`, `Zenith_SaveSystem::Create` (journal recovery of `saves/` and the initial profile runs synchronously, results in `GetLastRecoveryReport`, each logged); with `--record`, the replay writer and the Stage 2 core recorder; with `--replay`, the reader, the identity check, the verifier and the core player |
| step 14 | `Zenith_RuntimeSave_RegisterBindings`: the `SAVE_REQUEST` consumer, the `Save` bindings, the `Save.Request` command kind |
| step 16, prologue step 7 | `Zenith_RuntimeSave_AttachWorld` for each new world, after every module attached (so every participant is registered before the snapshot system declares their access) |
| step 17 | the resume decision's `ReadResumeIdentity` and its synchronous restore (`Zenith_RuntimeSave_RestoreSynchronously`, `runtime.startupLoadTimeoutMs`), and under `--record` the restored world's log records (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay recording"); under `--replay` the runtime makes no resume decision at all (row SV24) |
| between the last `RunFrame` and shutdown step 4 | `Zenith_RuntimeSave_RequestAutosaveForExit` when the exit is RHI's device-loss clean exit (it preempts the running ordinary write and starts the 6 s exit budget) |
| shutdown steps 4, 6, 7b | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Shutdown sequence" |

Each bridge function that can fail returns `Zenith_Result<…, Zenith_SaveError>`, and the composition root maps it:

| Function | Failure | Mapping |
|---|---|---|
| `Zenith_RuntimeSave_CreateSystem` | `SAVE_ERROR_INVALID_DESC`, `SAVE_ERROR_OUT_OF_MEMORY`, `SAVE_ERROR_THREAD_CREATE`, a replay reader or writer error (`SAVE_ERROR_IO` carrying the replay error's OS code) | exit code 4 ("a required device or platform service could not be created"); a replay identity mismatch is exit code 5 (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay playback") |
| `Zenith_RuntimeSave_RegisterBindings` | a binding, consumer or command-kind registration refused (`SAVE_ERROR_INVALID_DESC`) | exit code 3, as every step-14 registration failure |
| `Zenith_RuntimeSave_AttachWorld` | `BindWorld`, `RegisterParticipant` or `RegisterSnapshotSystem` failed; every earlier step of the attach is undone in reverse | at step 16: exit code 3 (MOD's initial-world attach rule, `MODULE_ERROR_REGISTRATION`); in a transition: the world host's attach-failure rule (the new world is destroyed, `WORLD_TRANSITION_FAILURE_WORLD_CREATE`) |
| `Zenith_RuntimeSave_RequestAutosaveForExit` | no primary world bound, arena 2 busy with a resume write, or a capture error | logged; the exit proceeds with exit code 6 and no autosave |
| `Zenith_RuntimeSave_RequestCheckpoint` | `SAVE_ERROR_QUEUE_FULL` (no free arena) or a capture error | logged and counted; retried at the next thermal step only |

A recovery CONFLICT or a corrupt journal is never a startup failure.

### Cancellation

| Operation | Cancellation point | Completion |
|---|---|---|
| write | before assembly and before each section's compression (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Image assembly and compression"), before each section's decode in journal step 0b's verification of the old file, before journal steps 1, 2, 3 and between `WriteAtomic` chunks (PLAT); the longest uncancellable unit is one storage call, one section's compression or one section's decode (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Journaled write sequence") | CANCELLED after the temporary and journal are removed; the slot keeps its old files |
| write at or after step 4 | ignored | SUCCEEDED (an operation that completes before it observes the cancellation reports success: `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", cancellation completion) |
| load | before and during the read, before decoding, before migration | CANCELLED after its arenas are released on the writer thread |
| listing, delete | before each file | CANCELLED; a delete stops between files (the slot stays listable) |
| export, import (Stage 4) | an export as a load; an import as a write (before journal step 4 only) | CANCELLED, the transfer arena released; a cancelled import leaves the slot's old files |
| resume write | as a write; PLAT cancels at its deadline | as a write |
| exit autosave | never cancelled by `BeginShutdown`; waited for at shutdown step 4 until `uSAVE_EXIT_AUTOSAVE_BUDGET_MS` (6,000 ms) after its submission, then cancelled with rollback and waited for at most `uSAVE_EXIT_ROLLBACK_WAIT_MS` (1,000 ms): 7 s of the clean exit's 10 s ([Z2-N-CONTRACT-484]), the derivation in `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Shutdown sequence" | as a write |
| ordinary work at a device-loss exit | every queued job, and the running ordinary write or operation, are cancelled when the exit autosave is submitted, so it never queues behind them; the ordinary drain of `save.shutdownDrainMs` is skipped | CANCELLED (a write already at journal step 4 completes SUCCEEDED) |
| an operation handle destroyed while incomplete | requests cancellation and detaches ([Z2-N-CONTRACT-091]) | as above; nobody observes it |

### Error enumerations

SAVE error codes (`Zenith_SaveErrorCode`):

| Code | Returned when | Recoverable |
|---|---|---|
| `SAVE_ERROR_INVALID_DESC` | a desc or request field outside its rule (a C++ request with more than 29 metadata pairs or a key beginning `zenith.`; a participant declaring a re-issue kind already declared, `ASYNC_SOURCE_SAVE`, or a kind at or above the capacity) | caller fix |
| `SAVE_ERROR_OUT_OF_MEMORY` | an arena, queue or table allocation failed | yes: the operation fails |
| `SAVE_ERROR_THREAD_CREATE` | the writer thread could not start | startup exit code 4 |
| `SAVE_ERROR_INVALID_SLOT_NAME`, `SAVE_ERROR_RESERVED_SLOT_NAME` | a slot name outside the rules | yes |
| `SAVE_ERROR_QUEUE_FULL`, `SAVE_ERROR_REQUEST_LIMIT` | too many queued (or no free capture arena for an out-of-tick submission, or, Stage 4, an export or import already running or untaken), or too many C++ requests in one tick (published once per tick as the limit batch, `SAVE_OPERATION_REFUSED_BATCH`) | yes |
| `SAVE_ERROR_SHUTTING_DOWN` | after `BeginShutdown` | no further saves |
| `SAVE_ERROR_IMAGE_TOO_LARGE`, `SAVE_ERROR_SECTION_LIMIT` | a capture or image above its limits | yes: that save fails |
| `SAVE_ERROR_SERIALIZE` | a field-set capture failed (`m_ulDetail` = type ID) | yes |
| `SAVE_ERROR_PARTICIPANT` | a participant failed (`m_ulDetail` = key) | yes |
| `SAVE_ERROR_COMPRESSION` | a compression or decompression error | yes (load: backup fallback) |
| `SAVE_ERROR_IO`, `SAVE_ERROR_LOW_DISK`, `SAVE_ERROR_DISK_FULL` | storage failures (`m_uOsCode`) | yes: nothing half-written |
| `SAVE_ERROR_CANCELLED` | cancellation before the commit point | yes |
| `SAVE_ERROR_NOT_FOUND` | no such slot or file | yes |
| `SAVE_ERROR_BAD_MAGIC`, `SAVE_ERROR_TRUNCATED`, `SAVE_ERROR_CHECKSUM`, `SAVE_ERROR_CORRUPT` | validation failures | yes: backup fallback |
| `SAVE_ERROR_VERSION_NEWER`, `SAVE_ERROR_VERSION_MAJOR` | a save of a newer format, or of another major ([Z2-N-CONTRACT-524]) | yes: refused, listed |
| `SAVE_ERROR_MIGRATION` | a migration step failed | yes: the load fails, the file is untouched |
| `SAVE_ERROR_SLOT_NEWER_VERSION` | a write over a newer-version save without `m_bOverwriteNewer` | yes |
| `SAVE_ERROR_RESTORE` | an `ApplyOutOfTick` error, a parent cycle, a save holding more `ASYNC_SOURCE_SAVE` entries than this build's declared capacity (§ "Asynchronous source capacity"), or a synchronous restore whose participants are not ready within its timeout | yes: the restored world is destroyed |
| `SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING` | a REQUIRED section without its participant | yes |
| `SAVE_ERROR_JOURNAL_CORRUPT`, `SAVE_ERROR_RECOVERY_CONFLICT` | recovery outcomes reported for a slot | yes: files kept for diagnosis |
| `SAVE_ERROR_PROFILE_BUSY` | a profile switch while work is queued or running or a retained-checkpoint lease is alive | retry next frame |
| `SAVE_ERROR_WORLD_NOT_BOUND` | a request for an unbound world | dropped and logged |
| `SAVE_ERROR_IDENTITY_MISMATCH` | a resume image of another profile; (Stage 4) an imported file whose `zenith.profile` pair is not the active profile's | yes: discarded |
| `SAVE_ERROR_SUPPRESSED_IN_REPLAY` | the internal outcome of a write, delete, listing or load in replay mode | never published: the writer completes nothing in replay mode and the recorded publication is the only one |
| `SAVE_ERROR_SUPERSEDED` | a subsequent request of the same slot in the same tick, while the earlier one was still pending | yes |
| `SAVE_ERROR_REFUSED_IN_REPLAY` | a C++ `BeginLoad`, `BeginLoadPath`, `BeginListSlots`, `BeginDelete`, `SubmitResumeWrite`, `SubmitExitAutosave`, `ReadResumeIdentity` or `SetActiveProfile`, or (Stage 4) `BeginProfileSwitch`, `BeginExportSlot` or `BeginImportSlot`, in replay mode | yes: nothing touched |
| `SAVE_ERROR_TRANSITION_BUSY` | a script `Save.Load` whose world already holds a transition or another load in flight (its linked `ASYNC_SOURCE_WORLD_LOAD` ID could not be allocated) | yes: nothing queued; the world stays |
| `SAVE_ERROR_PROFILE_MISMATCH` (38, Stage 4) | a request, due autosave, out-of-tick, resume or exit write of a world bound under a profile other than the active one, or than a pending switch's target; an export or import while a switch to another profile is pending (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Profile switch intake freeze") | yes: nothing written; the world returns to one bound under the new profile |

Replay error codes (`Zenith_ReplayErrorCode`): `INVALID_DESC` (desc rule), `OUT_OF_MEMORY`, `IO` (sink or file), `BAD_MAGIC`, `VERSION` (a format version the reader does not read), `TRUNCATED` (an inconsistent record), `LIMIT` (a parser limit of `Zenith2/Docs/Design/SAVE/Formats.md` → "Replay limits"), `IDENTITY` (with the field, [Z2-N-ARCH-784]), `TICK_ORDER`, `UNKNOWN_WORLD`, `NOT_FOUND`, `SINK_FULL`, `THREAD_CREATE`, `CHECKPOINT` (a seek checkpoint failed to restore). Every one is a result; none asserts on log bytes.

### Luau errors versus asserts

Script misuse of the `Save` module always raises the controlled Luau errors of `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Script-facing API" (ZS5101 to ZS5103, ZS5105 and ZS5106, in SAVE's block of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges") in every build ([Z2-N-CONTRACT-473]). Untrusted bytes (save files, journals, replay logs) never assert. Asserts are reserved for programmer errors: a storage call on a job worker, a restore step during a frame, destruction without `BeginShutdown`, a replay `Close` with a pending hash, a non-increasing tick index in a recorder call.

### Determinism obligations

- Requests enter in merge order, serials and completion IDs are assigned in that order, and every SAVE result is a `PUBLICATION_KIND_GENERIC_RECORDED` publication at a tick boundary in completion-ID order (Z2-D-053), so a replay reproduces every script-visible save outcome without touching storage.
- Every completion ID and serial SAVE allocates is a function of simulation state and recorded publication ticks alone: script requests carry the ID the bridge's consumer allocated on the committing merge node, in merge order (SCRIPT's commit hands the consumer 0 for a `SAVE_REQUEST` record); C++ requests are allocated at sync-point playback in merge order after the per-tick limit (the over-limit ones share the tick's limit batch ID); an engine autosave is allocated on the phase-11 node of its due tick, whether or not a capture arena is free and whether or not the run records (§ "Autosave scheduling" of `Zenith2/Docs/Design/SAVE/Algorithms.md`). Writer progress decides only when a result is completed, and that is recorded as its publication tick.
- Two **live** runs of the same inputs with different storage timing may therefore differ in SAVE publication ticks, and through the autosave due rule (which reads the previous autosave's publication) in which ticks allocate autosaves; from the first such difference their tick hashes may differ. A **replay** never does: it takes every SAVE publication, and so every autosave decision, from the log (Z2-R-SAVE-149). Every SAVE test that compares two live runs holds their publication ticks equal with `Zenith_MemorySaveStorage`'s write gate (`Zenith2/Docs/Design/SAVE/Tests.md` → "Save system tests"); every other comparison is record-then-replay.
- The delivery rule fixes only `ASYNC_SOURCE_SAVE`. Every other source whose completion time depends on I/O or scheduling (ANIM's bindings and chunk reads, streaming and scene loads, path requests, physics queries) publishes at a tick a live run does not reproduce, and an ordinary restore re-issues its in-flight requests at restore time. So a **live-versus-live** comparison after an ordinary restore holds only when no request of such a source is in flight at the snapshot or issued in the compared window (`Test_Restore_ContinuationMatchesUninterrupted`'s world issues none); every other continuation is checked against the **original run's recording**: the run records with `--record`, and a replay resumed from the save (`Zenith_RuntimeReplay_ResumeFromSave`) reproduces the recorded tick hashes, taking every publication, SAVE's included, from the log, with no delivery rule or write gate (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Scenario tests" names the original run as the reference).
- Out-of-tick submissions (the thermal checkpoint, menu saves of a paused world, the exit autosave), the resume write and replay checkpoints allocate no completion ID and no serial and change no world state, so recording, thermal steps and backgrounding never shift a subsequent completion ID.
- The decoded snapshot is a pure function of the world's state after the snapshot tick, independent of workers, chunk layout and scheduling (Z2-D-019); the header's timestamp and sequence are taken on the writer thread and never reach simulation.
- Autosave times are a function of the tick index, the saved autosave flag and the recorded publication tick of the previous autosave.
- A restore applies entities in simulation-ID order and restores the simulation-ID counter, the completion counter, carried events with their merge keys, reflected world resources and asynchronous state; a restore from an EXACT save therefore continues bit-identically under the delivery rule of § "Reproducibility boundaries" (live against live only under the condition of the bullet above; against the recording always, through a resumed replay), and a restore from a COHERENT save continues consistently.
- Phase 11 records no command and writes nothing but its snapshot buffer ([Z2-N-ARCH-503], [Z2-N-ARCH-504]); a participant's `CaptureSection` and `GetCaptureFidelity` change no simulation state.
- D-PHASES refinement (open, Stage 1) may tighten phase 11's rules and D-SCRIPT-TXN refinement (open, Stage 1) the publication rules; SAVE's revalidation for Stage 3 is gated on both. The tick hash digests through `Zenith_Hash64Stream`, whose algorithm is D-SEL-HASH (open, Stage 1); a change of it is a replay format change but not a save format change (saves use CRC-32C and SHA-256).

### Reproducibility boundaries

| Reproduced bit for bit (same build, backend, content, inputs) | Not reproduced |
|---|---|
| every tick's ECS state, events and commands; behaviour state; asynchronous publications (recorded or recomputed) | another ECS or physics backend, another platform, another build (refused, [Z2-N-ARCH-784]) |
| every save's published result (recorded); the decoded sections, which are a pure function of the world state at the snapshot tick | which tick a capture deferred for lack of a free arena snapshots (arena availability is writer timing), and so a deferred capture's sections; the save file's header timestamp and sequence; compressed bytes across a compression library version change |
| a world restored from an EXACT save or replay checkpoint, under the delivery rule below | a restore from a COHERENT save (header flag `COHERENT_ONLY`: consistent, not identical); the SAVE publication ticks of two live runs with different storage timing |
| ticks up to the first `NON_REPLAYABLE` tick | the wall-clock watchdog's fatal events and anything after them ([Z2-N-ARCH-790]) |
| | presentation: rendering, audio output, UI layout, thermal steps, memory-pressure shedding ([Z2-N-ARCH-812]), save I/O timing |

**Fidelity is a property of each capture** (orchestrator ruling on the scope of replay seeking and resumed replays, recorded as a DecisionLog.md entry by SAVE's Stage 4 seeking ticket; X205 carries the Architecture wording). A capture is EXACT only when every participant's `GetCaptureFidelity` returns `SAVE_FIDELITY_EXACT` for it; otherwise the header carries `COHERENT_ONLY`. In particular:

| Capture | Fidelity | Why |
|---|---|---|
| a captured behaviour with a `Run` coroutine suspended mid-body (`Zenith.Script.States`, `Zenith_ScriptSavedState::m_bRunSuspended`) | COHERENT | a restore never restores an execution position ([Z2-N-SCRIPT-393]): the coroutine restarts from the phase recorded in the declared state ([Z2-N-SCRIPT-395]), so its next steps can differ from the uninterrupted run's |
| an instance quarantined, or of a module disabled or disabling (`Zenith_ScriptSavedState::m_bQuarantinedOrDisabled`) | COHERENT | the restore makes a recovery the uninterrupted run did not make at that tick: module disables and recovery counters are not saved (`Zenith2/Docs/Design/SCRIPT/HotReload.md` → "Recovery invocations and exact restore" step 7; row SC25) |
| behaviour state with no suspended `Run` and no quarantined or disabled instance (a pending recovery, `m_bRecoveryPending`, included; an instance with no committed checkpoint, saved with a zero-length blob, included) | EXACT | an instance whose entry carries none of the three flags restores ACTIVE (or DORMANT) with no recovery invocation; a pending recovery runs `OnRecover` ([Z2-N-SCRIPT-394]) at the same tick under the reserved recovery ordinal, which consumes no invocation ordinal of [Z2-N-SCRIPT-274], so every instance keeps the uninterrupted run's merge keys (orchestrator ruling Q82 (b) as SC25 delivers it; the SCRIPT post-signoff patch Z2T-SCRIPT-114 is fenced in `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations", rows SV23 and SC25) |
| an additive instance LOADING or QUEUED at the snapshot (`Zenith.Scene.AdditiveSet`) | COHERENT | the re-requested load completes under a new completion ID at a tick the uninterrupted run did not see |
| a participant whose backend cannot continue bit-identically (a COHERENT physics backend) | COHERENT | the backend's own rule, through the participant |
| everything else (no suspended `Run`, no quarantined or disabled instance, no in-flight additive load, every module participant EXACT) | EXACT | entities, events, resources, behaviour states and asynchronous state restore exactly |

**The delivery rule.** A save cannot record the tick at which its own result is published: that tick is decided after the file is committed, by the writer's timing. So an ordinary restore (a load, a resume) publishes every `ASYNC_SOURCE_SAVE` entry that was in flight at the snapshot — the request that wrote the file with its own success payload, rebuilt byte for byte from the header (own completion ID and own request serial); any other with `SAVE_ERROR_CANCELLED` — at the boundary of tick t + 1, and marks them delivered there (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section"). An EXACT restore therefore continues bit-identically to the uninterrupted run **in which those entries published the same payloads at the boundary of tick t + 1**. That holds for an out-of-tick capture (`SubmitOutOfTick` between ticks t and t + 1: own completion ID 0) when no `ASYNC_SOURCE_SAVE` entry is in flight at the capture (nothing pending, captured or being written; `IsDrained()` true), and for a request-written save whose reference run completed the write before tick t + 1's boundary with no other SAVE request in flight (a test arranges it with `Zenith_MemorySaveStorage`'s write gate, opened and drained with `IsDrained()` between ticks t and t + 1). A replay resumed from the save and a seek take every SAVE publication, the save's own included, from the log at its recorded tick, so they need no such rule. The rule covers SAVE's own source only: a live-versus-live comparison also needs no request of another I/O- or scheduling-timed source in flight at the snapshot or issued in the compared window (§ "Determinism obligations"), and a world that streams, animates or path-finds is compared against its recording through a resumed replay instead. Every hash-comparing test states which of these it uses (`Zenith2/Docs/Design/SAVE/Tests.md`).

What each consumer of fidelity then promises:

| Consumer | EXACT save | COHERENT save |
|---|---|---|
| a load (`Test_Restore_ContinuationMatchesUninterrupted`) | the continuation's tick hashes equal those of the uninterrupted run under the delivery rule, when no request of another I/O- or scheduling-timed source is in flight at the snapshot or issued in the compared window; otherwise the equality is checked by a replay resumed from the save against the original run's recording | entity set, component values, resources, behaviour states and participant coherence equal the saved ones; no hash equality is claimed |
| replay seeking (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay seeking") | strategy CHECKPOINT through the latest EXACT checkpoint at or before the target | the checkpoint is skipped; strategy FULL replays from the session's start (correct, slower) |
| a replay resumed from a save ([Z2-N-ARCH-789]) | continues from the saved position comparing every tick hash | continues from the saved position with the recorded inputs and publications, compares no hash, and reports `m_bHashesCompared = false` |
| Architecture's scenario `Test_Scenario_SaveAtSnapshotDuringStreaming` (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Scenario tests") | asserts the tick-hash reproduction: a replay resumed from the save reproduces the original run's recorded tick hashes | asserts SAVE's coherence assertions instead (X205) |

`Test_SaveRuntime_RecordReplayRoundTrip`, `Test_Replay_DifferentBuildRefused`, `Test_SaveRuntime_ReplayStopsBeforeNonReplayable`, `Test_Restore_ContinuationMatchesUninterrupted` and `Test_SaveRuntime_RunBehaviourWritesCoherentOnly` prove the boundary from both sides.

## Diagnostics and profiling

### Counters

Published through `Zenith_DispatchCounter` with their kind ([Z2-N-CONTRACT-155]):

| Counter | Kind | Meaning |
|---|---|---|
| `save.writes.completed`, `save.writes.failed`, `save.writes.cancelled`, `save.writes.superseded` | CUMULATIVE | write outcomes |
| `save.loads.completed`, `save.load.from_backup`, `save.loads.failed` | CUMULATIVE | load outcomes |
| `save.bytes_written_delta`, `save.bytes_read_delta` | DELTA | storage traffic |
| `save.requests.refused` | CUMULATIVE | intake refusals (queue full, limit — each member of a limit batch counts —, name, transition busy, and from Stage 4 profile mismatch) |
| `save.metadata.dropped` | CUMULATIVE | `Save.SetMetadata` keys ignored because the tick's list already held 29 keys |
| `save.restore.readiness_wait_us` | GAUGE | the latest restore's time in PARTICIPANTS_READY |
| `save.requests.unpublishable` | CUMULATIVE | requests refused before allocation because `m_uOutstanding` reached `uSaveMaxOutstanding`, or completion IDs refused by the table, the bridge consumer's allocation for a script request included (a defect: unreachable by the bound of § "Asynchronous source capacity") |
| `save.outstanding.high_water` | CUMULATIVE | the highest `m_uOutstanding` of any world |
| `save.pending.depth` | GAUGE | pending (not yet captured) writes over every world |
| `save.writer_queue.full`, `save.writer_queue.dropped`, `save.writer_queue.high_water` | CUMULATIVE | the bounded-queue triple ([Z2-N-CONTRACT-345]) of the writer queue and the priority slot |
| `save.operation_queue.full`, `save.operation_queue.dropped`, `save.operation_queue.high_water` | CUMULATIVE | the triple of the operation queue |
| `save.replay.commit_queue.full`, `save.replay.commit_queue.dropped`, `save.replay.commit_queue.high_water` | CUMULATIVE | the triple of the replay writer's chunk queue |
| `save.snapshot.deferred` | CUMULATIVE | captures delayed for a free arena |
| `save.autosave.allocated` | CUMULATIVE | engine autosaves allocated at a due tick |
| `save.autosave.profile_refused` | CUMULATIVE | Stage 4: due autosaves allocated and completed `SAVE_ERROR_PROFILE_MISMATCH` (§ "Error enumerations") |
| `save.slot_notices.full`, `save.slot_notices.dropped`, `save.slot_notices.high_water` | CUMULATIVE | Stage 4: the triple of the slot notice queue; `full` counts the writer's backpressure waits and `dropped` stays 0, because the writer waits rather than drops |
| `save.snapshot.orphaned` | CUMULATIVE | entities saved as roots because their parent was excluded |
| `save.snapshot.last_us`, `save.write.last_us` | GAUGE | the latest capture and write (assembly + journal) times |
| `save.write.latency_us`, `save.write.last_bytes`, `save.load.last_us`, `save.restore.us_per_10k_entities` | GAUGE | the latest write's intake-to-completion time and file size, the latest load's time, and the latest restore's time per 10,000 entities (the pending rows of § "Pending row needs") |
| `save.arena.resident_bytes` | GAUGE | Z2-B-SAVE-001's live value |
| `save.restore.unresolved_references`, `save.restore.unknown_components`, `save.restore.unknown_resources`, `save.restore.unknown_events`, `save.restore.async_failed` | CUMULATIVE | restore anomalies |
| `save.fidelity.coherent` | CUMULATIVE | captures written with `COHERENT_ONLY` |
| `save.recovery.rolled_back`, `.rolled_forward`, `.restored_backup`, `.conflict`, `.journal_corrupt`, `.stale_temporaries` | CUMULATIVE | recovery outcomes |
| `save.replay.bytes_committed_delta` | DELTA | replay traffic |
| `save.replay.limit_drops`, `save.replay.write_errors`, `save.replay.checkpoints_skipped`, `save.replay.mismatches` | CUMULATIVE | replay anomalies |
| `save.replay.unverified_ticks` | CUMULATIVE | ticks run without hash comparison (resumed from a COHERENT save) |

### Profiling zones

`Save.Snapshot` (the phase-11 body, measured by Z2-B-SAVE-002), `Save.Snapshot.Entities`, `Save.Snapshot.Participants`, `Save.Assemble`, `Save.Compress`, `Save.Journal`, `Save.Load`, `Save.Verify`, `Save.Migrate`, `Save.Restore.Step`, `Save.Replay.Commit`, `Save.Replay.Hash`.

### Log categories and messages

Categories `save` (`xLOG_CATEGORY_SAVE`), `save.journal`, `save.replay`, all compiled minimum `LOG_LEVEL_TRACE`:

| Level | Message |
|---|---|
| info | `save: wrote '<slot>' (<kind>, <bytes> bytes, tick <t>, <ms> ms)` |
| info | `save: loaded '<slot>' (format <M>.<m>, migrated from <M>.<m0>)` |
| warning | `save: '<slot>' failed validation (<error>, section <kind> world <i>); trying the backup` |
| warning | `save: '<slot>' loaded from its backup; the damaged save was kept as <slot>.zsave.corrupt` |
| error | `save: write of '<slot>' failed: <error> (os <code>)` |
| info / warning | `save: recovered '<slot>': <outcome> (found <state>)` |
| warning | `save: <n> unresolved references in '<slot>' (<type>.<property> first)` |
| warning | `save: component type <id> unknown; kept as carried data (<n> entities)` |
| info | `save: profile switched to <kind>`, `<kind>` being `local` or `account` (Stage 4 wording: no SAVE line names a profile key, `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Profile switching") |
| info | `save: profile switch to <kind> pending` (Stage 4) |
| info | `save.replay: recording to <path>`; `save.replay: replaying <path> (<n> sessions, <t> ticks)` |
| error | `replay: tick <t> of world session <o> diverged: recorded <h1>, replayed <h2>` |
| warning | `save.replay: executable has no build ID` (logged by `Zenith_RuntimeSave_CreateSystem`, once per process) |
| info | `save: '<slot>' written COHERENT_ONLY (<participant> reported coherent)` |
| info | `save.replay: resumed from '<slot>' at (<ordinal>, <tick>) without hash comparison: the save is COHERENT_ONLY` |
| warning | `save: request refused: <n> results outstanding (limit <m>)` |
| warning | `save: script request dropped: no ASYNC_SOURCE_SAVE completion ID` (logged by the bridge's consumer) |
| error | `save: restore refused: <n> ASYNC_SOURCE_SAVE entries in flight exceed this build's limit <m> (save.maxQueuedRequests, save.maxRequestsPerTick)` |
| warning | `save: metadata key '<key>' ignored: 29 keys already set this tick` |
| warning | `save: <n> C++ save requests over the per-tick limit <m> refused at tick <t>` (once per limit batch) |
| error | `save: restore of world <tag> not ready after <ms> ms (participant <key>)` |
| warning | `save: exit autosave abandoned after <ms> ms; the previous autosave is kept` |

### On-screen profiler rows

The SAVE panel of the on-screen profiler shows: queued writes, the running write's progress and slot, the latest capture time (with the Z2-B-SAVE-002 row value once allocated), the arena resident bytes against Z2-B-SAVE-001, and, under `--record` or `--replay`, the committed log bytes, the current session and tick, and the verified tick count.

### Debug commands and dumps

`save.dump` (development builds, the console) prints the world bindings (with `m_uOutstanding`, the fidelity of the last capture and, from Stage 4, the binding profile as `local` or `account`, never its key), the slot table, the latest recovery report and the operation table; `save.dump <slot>` prints a slot's header, section table and metadata from a `ReadPrefix` of `uSAVE_LISTING_PREFIX_BYTES`, as a listing does, without decoding a compressed section or loading the save. The frame dump carries the SAVE panel's values.

### Telemetry

Each completed write and load emits one telemetry record `{kind, bytes, capture µs, write µs, outcome code, from backup}`; recovery outcomes at startup are one record each.

## Authoring and cooking

### What SAVE authors

SAVE has no editor-authored documents and no cooked assets: saves are runtime outputs. The authoring that touches SAVE is (a) the game's choice of save kinds, slot names and metadata in Luau through the `Save` module, (b) the `save.*` configuration keys in `game.json`'s configuration layer, and (c) STREAM's persistence components on authored entities (Stage 4), which the editor's inspector shows like any component (EDIT).

### Cook steps

None of its own: `Zenith_RegisterSaveTypes` registers SAVE's types at startup, and the cook's type generation emits their `.d.luau` like every engine type (`Zenith2/Docs/Design/ECS.md` → "Cook steps").

### Migration hooks

Saves migrate forward at load ([Z2-N-ARCH-605]): the format chain of `Zenith_SaveMigrationRegistry`, the component chains through `Zenith_MigrateFieldSet`, the participant sections through each participant, behaviour states through SCRIPT's `Migrate` steps. A schema change of a component or a behaviour state needs no SAVE change; a change of a SAVE-owned section layout is a format minor bump with its step and fixture (`Zenith2/Docs/Design/SAVE/Formats.md` → "Frozen fixture writers").

### Editor surfaces

EDIT owns the UI. SAVE gives the editor: play-in-editor saves and loads through the same `Zenith_SaveSystem` under the editor's own profile key `Zenith_MakeSaveProfileKey("editor:<play world tag>")` (16 hex digits like any account key, so play sessions never touch player saves), the `save.dump` output, and the replay viewer's `Zenith_RuntimeReplay_SeekTo`.

## Failure handling

| Failure | Behaviour |
|---|---|
| missing save file | `SAVE_ERROR_NOT_FOUND`; the listing omits the slot |
| corrupt or torn save | verification fails; the backup loads if valid (the damaged file renamed `.corrupt`); otherwise the load fails and nothing is renamed |
| corrupt journal | renamed `.z2journal.corrupt`; temporaries removed; the slot lists from its files |
| power loss or process kill during a write | the journal and PLAT's atomic writes leave the old save or the new save, never a mix; recovery runs at the next start |
| power loss during recovery | recovery is idempotent; the next run converges |
| low disk | refused before anything is written (`SAVE_ERROR_LOW_DISK`), the game is told through `Zenith.SaveCompleted` and PLAT's `Platform.LowDisk` event |
| disk full mid-write | the temporary is deleted, the old save stands (`SAVE_ERROR_DISK_FULL`) |
| allocation failure in capture, assembly or load | that operation fails `SAVE_ERROR_OUT_OF_MEMORY`; the frame continues |
| a save larger than the arena or 256 MiB | `SAVE_ERROR_IMAGE_TOO_LARGE`, published |
| a newer-version save | listed NEWER_VERSION; never loaded; never overwritten without `m_bOverwriteNewer` |
| a save of another major | listed INCOMPATIBLE; refused recoverably |
| an unknown component type, event type or section | kept as carried data or dropped (events) with a warning; the save still loads |
| a required participant missing | the load fails `SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING` |
| a migration step failure | the load fails; the file is untouched |
| a restore failure | the isolated world is destroyed; the current world stays |
| cancellation | the operation releases what it holds on the writer thread and completes CANCELLED |
| device loss | after the last `RunFrame` returns, the clean exit calls `Zenith_RuntimeSave_RequestAutosaveForExit`, which cancels every queued job and the running ordinary write (rollback before journal step 4), captures the primary world out of tick into arena 2 at once and returns the operation handle; shutdown step 4 never cancels it early, waits for it until 6 s after its submission, then cancels it with rollback and waits at most 1 s more, so SAVE's share of the 10 s clean exit ([Z2-N-CONTRACT-484]) is 7 s and the exit ends with code 6 rather than the fatal fallback; the save path touches no GPU state |
| shutdown with a write in flight | drained for `save.shutdownDrainMs`, then cancelled with rollback |
| replay mode | script writes, deletes, listings and loads perform no storage access and complete nothing, the recorded publications being the results; C++ loads, listings and deletes and the resume, resume-identity, exit-autosave and profile calls are refused (`SAVE_ERROR_REFUSED_IN_REPLAY`); journal recovery, the last-profile read and the slot-table build are skipped, and the runtime makes no resume decision, so no user file is read, written or deleted (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay mode") |
| a restored world not ready (a participant's `IsRestoreReady` keeps returning false) | a frame-driven restore keeps waiting before the world's first tick, bounded only by a cancel request on the transition's ticket (`WORLD_TRANSITION_FAILURE_CANCELLED`, which destroys the isolated world): GAME's world host has no transition timeout, so the transition, its linked world-load ID and the world's single world-load slot stay held until then (cross-area row SV25 asks GAME to fail a save-source `ACTIVATING` not DONE within `runtime.startupLoadTimeoutMs` with `RESTORE`, logging `GetFirstUnreadyParticipantKey`; assumption fenced in § "Assumed interfaces from other areas"); the synchronous driver fails after its timeout with `SAVE_ERROR_RESTORE` naming the participant (a resume: `RESUME_DISCARD_REASON_RESTORE_FAILED`; a seek: `REPLAY_ERROR_CHECKPOINT`) |
| a script `Save.Load` refused, cancelled or failing | its `ASYNC_SOURCE_SAVE` ID publishes the error and its linked `ASYNC_SOURCE_WORLD_LOAD` ID publishes GAME's `WORLD_TRANSITION_FAILURE_PREPARE`, so the world's single world-load slot is released; with a transition already in flight, `SAVE_ERROR_TRANSITION_BUSY` and nothing is queued |
| replay sink error or full commit queue | recording stops; the committed prefix stays readable |
| replay identity mismatch | exit code 5 naming the field |
| replay divergence | exit code 5 naming the first diverging tick |
| budget breach | per row: Z2-B-SAVE-001 is BR-GOVERNOR (the governor's response, SAVE sheds nothing); Z2-B-SAVE-002 and the pending SAVE rows are BR-ACCEPT (the stage acceptance that measures the row is blocked until it is fixed or re-budgeted); Z2-B-SCRIPT-025 is BR-MERGE (the script request is rejected at the merge) |

## Test strategy

### Test kinds

| Kind | Where |
|---|---|
| unit and property tests | `ZenithSave_Tests`, per backend where world state is involved |
| power-loss and fault-injection tests | the journal group, over the in-memory storage and by killing a child process on the host |
| migration fixture tests | text fixture states, from which the binary saves are generated at test time into `${ZENITH_OUT}/test-fixtures/SAVE/` ([Z2-N-ARCH-257]) |
| integration tests | `Zenith_Integration_SaveRuntime`, `Zenith_Integration_SaveCrossSystem` |
| determinism tests | snapshot bytes and restored continuations under 1, 2, 8 and all workers, forced compaction, two chunk capacities, per backend |
| fuzzing | four libFuzzer targets |
| death tests | four `z2-death-assert` cases |
| device runs | the S24 kill-during-write, backup rules and installed-update runs (`needs-human`) |
| benchmarks | `Zenith_Bench_Save` |

### Required named tests summary

Every test is named with its assertion in `Zenith2/Docs/Design/SAVE/Tests.md`; the groups and the gate step running each are `Zenith2/Docs/Design/SAVE/Tests.md` → "Gate steps per test group". The corruption, power-loss and atomic-replace cases are `Zenith2/Docs/Design/SAVE/Tests.md` → "Journal and power-loss tests"; the version-migration cases `Zenith2/Docs/Design/SAVE/Tests.md` → "Migration and fixture tests"; the platform-storage cases `Zenith2/Docs/Design/SAVE/Tests.md` → "Platform storage tests".

### Scenario tests that consume SAVE

Architecture's `Test_Scenario_SaveAtSnapshotDuringStreaming` (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Scenario tests", [Z2-N-ARCH-562]; GAME owns its implementation on `Streamed.scene.json`, Stage 4) saves in phase 10 while cells stream and characters and bodies run. Its save is COHERENT under § "Reproducibility boundaries" whenever a cell is still loading at the snapshot, a scripted behaviour is suspended mid-`Run` or an instance is quarantined or of a disabled module: the scenario then asserts the snapshot conditions and SAVE's coherence assertions (no half-streamed cell, no duplicated or lost entity, physics and animation from one tick, the interrupted write leaving the previous save intact). It asserts the tick-hash reproduction only for a variant whose capture is EXACT (no suspended `Run`, no quarantined or disabled instance, no in-flight additive load, an EXACT physics backend), and against the original run's recording, which is the reference Architecture's sentence names ("the original run"): the run records with `--record`, the phase-10 save carries its replay position, and a replay resumed from the save (`Zenith_RuntimeReplay_ResumeFromSave`) reproduces the original run's recorded tick hashes from that tick, asserting `m_bHashesCompared` and `m_bVerified`. No delivery rule or write gate is needed: the resumed replay takes every publication, the save's own and the streaming, animation and path results included, from the log at its recorded tick, while two live runs over streaming and animation publish those at I/O-dependent ticks and could not agree. Architecture states this since cross-doc item X205 (the scenario's row in `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Scenario tests", and [Z2-N-ARCH-789]), and this paragraph is SAVE's elaboration of it. SAVE's own counterparts are `Test_SaveCrossSystem_SaveDuringStreamingWithAnimationAndPhysics` and `Test_SaveCrossSystem_PhysicsAndAnimationCoherent`.

## Budget rows

Columns as `Zenith2/Docs/Budgets.md` → "Per-system rows" states them (measurement procedure, platforms, tolerance, workload, breach rule, producer).

### Rows SAVE owns

| Row | Name | Status | Unit | Measurement procedure | Platforms | Tolerance | Workload | Breach rule | Producer | SAVE's measured quantity |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-SAVE-001 | Save serialization buffers | pending | MB | MP-SUBMEM | Win, S24 | default | W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for SAVE | `save.arena.resident_bytes`: the capture, assembly and load arenas, the retained and pending checkpoints and the thumbnails (`GetResidentBytes` of the memory consumer, `MEMORY_SUBSYSTEM_SIMULATION`) |
| Z2-B-SAVE-002 | Save-snapshot point CPU per requested save | pending | ms | MP-CPUZONE | Win, S24 | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for SAVE | the `Save.Snapshot` zone on frames that save ([Z2-N-BUDGET-224]) |

### Rows SAVE is measured against

| Row | Owner | Why SAVE is measured against it |
|---|---|---|
| Z2-B-PLAT-004 Resume-state write duration | PLAT | the resume write (out-of-tick snapshot + assembly + journal) must finish inside PLAT's 3,000 ms deadline and its row |
| Z2-B-SCRIPT-014 Checkpoint serialization CPU per tick | SCRIPT | the behaviour checkpoints SAVE copies are produced by SCRIPT within that row |
| Z2-B-SCRIPT-025 Save-request capacity per tick | SCRIPT | its value is `save.maxRequestsPerTick` (§ "Configuration keys"); script requests above it are merge rejections (BR-MERGE), C++ requests above it `SAVE_ERROR_REQUEST_LIMIT`; SAVE declares no second limit |
| Z2-B-CORE-007 World-transition peak | CORE | a load from a save holds the prepared save (the decoded image) and the restoring isolated world beside the live world; GAME reserves the peak for the transition, and the prepared save's arena is part of what it reserves |
| Z2-B-GAME-003 Event queue capacity per tick | GAME | a restore re-delivers the saved carried events in tick t + 1; the EVENTS parse limit is `ecs.maxEventsPerTick`, the configured value of this row, so a valid save never holds more events than one tick may carry (`Zenith2/Docs/Design/SAVE/Formats.md` → "Format limits") |
| Z2-B-SHIP-015 to Z2-B-SHIP-018 hitch rows | SHIP | a save frame counts toward hitches like any frame ([Z2-N-BUDGET-224]) |
| Z2-B-CORE-001, Z2-B-CORE-002 | CORE | the process totals the save arenas are part of |

### Pending row needs

New rows this design needs, allocated by the Stage 1 per-system budget ticket for SAVE (cross-area row SV13). They are Budgets rows since cross-doc item X202: Z2-B-SAVE-003, Z2-B-SAVE-005 and Z2-B-SAVE-006 in `Zenith2/Docs/Budgets.md` → "Latency and duration rows" and Z2-B-SAVE-004 in `Zenith2/Docs/Budgets.md` → "Package and storage rows", each an MP-COUNTER row whose statistic is the maximum over the run ([Z2-N-BUDGET-465]). Each counter is published with its kind (§ "Counters"):

| Row | Unit | Measurement procedure | Platforms | Tolerance | Workload | Breach rule | Budgets section | SAVE's measured quantity |
|---|---|---|---|---|---|---|---|---|
| Z2-B-SAVE-003 Save write duration per save | ms | MP-COUNTER, maximum over the run | Win, S24 | default | W-OpenWorld (its scripted manual save and autosaves) | BR-ACCEPT | Latency and duration rows | `save.write.latency_us`: from intake to completion of one write, capture wait and journal included |
| Z2-B-SAVE-004 Save file size per slot | MB | MP-COUNTER, maximum over the run | Win, S24 | default | WS-SLICE | BR-ACCEPT | Package and storage rows | `save.write.last_bytes`: the file size of each completed write |
| Z2-B-SAVE-005 Save load duration per save | ms | MP-COUNTER, maximum over the run | Win, S24 | default | W-OpenWorld (one load from its scripted save) | BR-ACCEPT | Latency and duration rows | `save.load.last_us`: read, verification, decode and migration of one load, on the writer thread |
| Z2-B-SAVE-006 World restore duration per 10,000 entities | ms | MP-COUNTER, maximum over the run | Win, S24 | default | W-OpenWorld (the same load) | BR-ACCEPT | Latency and duration rows | `save.restore.us_per_10k_entities`: the restore job's summed `Step` time divided by its entity count × 10,000 |

The replay log needs no row: logs exist only in development and CI runs under `--record`, and their size is bounded by `replay.maxLogBytes`, a configuration limit enforced by the reader and writer, not a product budget.

## Stage plan

### Stage 1

- The `needs-human` ruling "Stage 1 per-system budget ticket for SAVE" allocates Z2-B-SAVE-001 to Z2-B-SAVE-006 (Z2-B-SAVE-003 to Z2-B-SAVE-006 are the rows of § "Pending row needs") from reference-engine and published data; SAVE has no harness half (`Zenith2/Docs/Budgets.md` → "Per-system producer shape").
- No SAVE decision is open; SAVE's Stage 3 revalidation ticket is gated on D-PHASES refinement (open, Stage 1) and D-SCRIPT-TXN refinement (open, Stage 1); its Stage 2 replay slice on D-SEL-HASH (open, Stage 1) through the tick hash. SAVE's Stage 3 ticket for the `Zenith.Script.States` fidelity rule and section flags is blocked by SCRIPT's Q82 (b) post-signoff patch ticket Z2T-SCRIPT-114 (rows SV23 and SC25; cross-doc row X267).

### Stage 2

- API-only slice: `Zenith_ReplayLog.h` with every declaration and contract block (pulled forward).
- The replay writer, reader, index, verifier, host-file and memory sinks, the running build ID, and their tests and fuzz target.
- The Stage 2 bridge slice `Zenith_RuntimeReplayCore.*` (§ "Runtime bridge files"): a recorder and player that write and read `WORLD_BEGIN`, `WORLD_SWAP` and `TICK` records with input commands and tick hashes (the tick hash is `HashTickState` alone until SCRIPT's contribution joins it in Stage 3), feed phase 1's input system from the log, cap the clock and verify. It depends only on Stage 2 producers (ECS's `Zenith_TickBoundaryHandler` and `HashTickState`, PLAT's input commands, MOD's runtime), never on a SCRIPT interface, so MOD's headless-equivalence test (`Test_Runtime_HeadlessMatchesWindowedSimulation`, Z2-R-MOD-074) records and replays through it in Stage 2.
- Pull-forward candidates with fix version Stage 3, scheduled in the Stage 2 sprint: CRC-32C, the ZSAV container and its fuzz target, the journal and recovery over the storage interface, which depend on no Stage 2 decision.

### Stage 3

Everything else of save and replays except the Stage 4 rows below: the snapshot (with reflected world resources and per-capture fidelity) and restore, the save system and writer thread, slots and kinds, autosaves, checkpoints, listings, C++ loads (`BeginLoad`, verification, migration, restore jobs, `Test_Restore_*`), corruption recovery, migration and its fixtures, the resume path with PLAT and MOD, the runtime bridge and its participants, the `Save` bindings and events except `Save.Load`, the Stage 3 replay adapters (`Zenith_RuntimeReplayScript.*`: publications recorded and supplied, the SCRIPT checkpoint contribution to the tick hash, the replay-mode storage rules), the recording rules and position, the script-failure replays, diagnostics, the benchmark, the S24 kill-during-write device run. Stage 3 acceptance collects save migration with fixtures and interrupted writes (`Zenith2/Docs/Roadmap.md` → "Acceptance evidence").

### Stage 4

The `Save.Load` binding and its hand-off to GAME's save-load transition (`Zenith_RuntimeSaveLoad.cpp`, with GAME's Stage 4 transition source `WORLD_TRANSITION_SOURCE_SAVE`), the linked-ID refusal path, and the replay reader's lookahead and `FindPublication` GAME uses; persistent world and entity state with STREAM (cell deltas in components, re-requested loading instances with their hook tags); exclusions for consistent snapshots while streaming; replay checkpoints, the restored-world checkpoint convention, seeking and resume-from-save with their fidelity rules, and the DecisionLog.md entry recording the per-capture fidelity ruling (appended by the seeking ticket); profile switching with ONLINE's account flow, and ONLINE's seams (the binding profile and `SAVE_ERROR_PROFILE_MISMATCH`, the intake freeze, the same-profile no-op and key-free diagnostics, the `saves/online/` reservation, the slot listener, slot export and import); the cross-system failure suite with STREAM, PHYS and ANIM; OpenWorldSlice's persistent-world saves.

### Stage 6

Replay version compatibility across a format bump; the shipped-build fixture recording (`needs-human`) and its load test; the Android backup rules and installed-update device runs; the fuzz targets for their full duration in the security evidence; the per-kind compatibility policy demonstrated for saves ([Z2-N-CONTRACT-551]).

### Stages 5 and 7

Stage 5 has no SAVE work. Stage 7's networked multiplayer (Z2-D-073) reuses the `ZRPL` command block; its replication formats are NET's and add no SAVE requirement now. NET's design meets SAVE's deferred line on replication snapshots and dedicated-server persistence without a SAVE format change (A3 row NT22): a server saves its authoritative world as any world, a client excludes its proxies, and NET's participant section `Zenith.Net.Identity` is the only NET data in a save.

### API-only slices

| Slice | Stage (sprint) | Lands |
|---|---|---|
| `Zenith_ReplayLog.h` | 2 | every replay declaration with its contract block, the header alone, compiled by `HeaderSelfContained` and checked by `ApiContractCheck` |
| `Zenith_SaveTypes.h`, `Zenith_SaveFormat.h`, `Zenith_SaveStorage.h`, `Zenith_SaveJournal.h` | 2 (fix version 3) | the container and journal declarations |
| `Zenith_SaveParticipant.h`, `Zenith_WorldSnapshot.h`, `Zenith_SaveSystem.h` | 3 | the interfaces modules implement, first, so PHYS, ANIM, AI and CINE participant tickets can start |

## Decisions

### Settled decisions

| Decision | How it governs SAVE |
|---|---|
| Z2-D-048 | saves migrate forward from every shipped version of the current major release, with a fixture per version; save writes are journaled |
| Z2-D-038 | the binary formats never depend on C++ layout |
| Z2-D-019 | replays reproduce runs per build and backend; snapshots are layout-independent |
| Z2-D-053 | save results become visible at tick boundaries in completion-ID order |
| Z2-D-080 | the save-snapshot point is phase 11 |
| Z2-D-073 | replayable input commands, shared with the multiplayer foundations |
| Z2-D-016, Z2-D-023 | script save requests are transactional side effects; behaviour state is restored with `OnRecover` |
| Z2-D-004 | every section names its codec; zlib and zstd both readable |
| Z2-D-045, Z2-D-047 | results for recoverable errors; one contract block per type |
| Z2-D-046 | the save system is created and passed by the composition root; no globals |
| Z2-D-040, Z2-D-041 | ownership through `Zenith_Owned` and operation handles; CORE containers |
| Z2-D-010 | the S24 is the Android validation device for the device runs |
| Z2-D-027 | installed-update migration is tested with Play's flow in Stage 6 |
| Z2-D-074 | cloud saves are ONLINE's; SAVE gives profiles and slot metadata |
| Z2-D-012 | SAVE's rows are allocated in Stage 1 |

### Open decisions

D-PHASES refinement (open, Stage 1) — phase 11's contract; D-SCRIPT-TXN refinement (open, Stage 1) — publication; D-SEL-HASH (open, Stage 1) — the tick hash's digest.

The per-capture fidelity rule of § "Reproducibility boundaries" is an orchestrator ruling on a plan promise (Q70 (a): replay seeking, resumed replays and Architecture's save scenario for a COHERENT save), narrowed by the orchestrator ruling Q82 (b) (behaviour state alone does not make a capture COHERENT; only a suspended `Run` coroutine or an in-flight additive load does, once SCRIPT's recovery invocation consumes no invocation ordinal) and widened back by the orchestrator's acceptance of row SC25 (an instance quarantined or of a disabled or disabling module also makes `Zenith.Script.States` COHERENT, and the section accepts zero-length blobs). Neither is an open decision of this design; both are binding now, and SAVE's Stage 4 seeking ticket appends them to `Zenith2/Docs/DecisionLog.md` as one entry with its own `Z2-D-` number in the ticket diff (`Zenith2/Docs/DecisionLog.md` → "How this log is written"). A human may still override either.

### Selection decisions SAVE owns

None: the container, journal and checksum are specified here; the codec is COMPRESS's per-asset choice, fixed for saves by `save.compressionCodec`.

### Justification records

<!-- elaboration -->
CRC-32C is an algorithm, not custom infrastructure (no container, allocator, heap wrapper or threading primitive), so it carries no justification record, like SHA-256 in CORE (`Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH"). SAVE adds no custom container: its queues are CORE's `Zenith_BoundedQueue`.

## Requirement index

<!-- REQ-INDEX -->
| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-SAVE-001 | ZenithSave library target and declared dependencies | S2 | SAVE.md → "ZenithSave target"; SAVE.md → "Libraries SAVE depends on"; SAVE.md → "Supported matrix" |
| Z2-R-SAVE-002 | Contract blocks and results on every SAVE header | S2 | SAVE.md → "API rules"; SAVE/PublicAPI.md → "Header list" |
| Z2-R-SAVE-003 | String-free error records | S2 | SAVE/PublicAPI.md → "Zenith_SaveTypes header"; SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE.md → "Error enumerations" |
| Z2-R-SAVE-004 | Script-dependent SAVE code lives in the runtime bridge | S3 | SAVE.md → "Runtime bridge files"; SAVE/RuntimeBridge.md → "Runtime bridge declarations"; SAVE.md → "Libraries SAVE depends on" |
| Z2-R-SAVE-005 | Replay header written per Architecture | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE/Formats.md → "Records SAVE writes"; SAVE/Algorithms.md → "Replay recording" |
| Z2-R-SAVE-006 | Tick record layout and input command encoding | S2 | SAVE/Formats.md → "Records SAVE writes"; SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-007 | Publications recorded in completion-ID order | S2 | SAVE/Algorithms.md → "Replay recording"; SAVE/Formats.md → "Records SAVE writes" |
| Z2-R-SAVE-008 | Replay records committed in a deterministic order | S2 | SAVE/Formats.md → "Records SAVE writes"; SAVE/Algorithms.md → "Replay recording" |
| Z2-R-SAVE-009 | Tick hash recorded at the next tick boundary | S2 | SAVE/Algorithms.md → "Replay recording"; SAVE/Algorithms.md → "Replay verification" |
| Z2-R-SAVE-010 | Per-tick limits mark a tick non-replayable | S2 | SAVE/Formats.md → "Replay limits"; SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-011 | Interrupted recordings stay readable | S2 | SAVE/Formats.md → "Replay limits"; SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-012 | Replay identity check names the mismatched field | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE/Algorithms.md → "Replay playback" |
| Z2-R-SAVE-013 | Unknown replay records skipped by length | S2 | SAVE/Formats.md → "Replay limits"; SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-014 | World sessions and swaps in the log | S2 | SAVE/Formats.md → "Records SAVE writes"; SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-015 | Host-file sink with its writer thread | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE.md → "Threads SAVE owns"; SAVE/Algorithms.md → "Replay recording" |
| Z2-R-SAVE-016 | A sink error stops recording and keeps the prefix | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE/Algorithms.md → "State machines" |
| Z2-R-SAVE-017 | Running build ID | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE.md → "ZenithSave target" |
| Z2-R-SAVE-018 | Replay verifier reports the first mismatch | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE/Algorithms.md → "Replay verification" |
| Z2-R-SAVE-019 | Replay playback supplies inputs and verifies hashes | S2 | SAVE/Algorithms.md → "Replay playback"; SAVE/RuntimeBridge.md → "Runtime bridge declarations"; SAVE.md → "Runtime bridge files" |
| Z2-R-SAVE-020 | Replay stops before a non-replayable tick | S3 | SAVE/Algorithms.md → "Replay playback"; SAVE.md → "Reproducibility boundaries" |
| Z2-R-SAVE-021 | Ticks with script failures replay identically | S3 | SAVE/Algorithms.md → "Replay recording"; SAVE/Algorithms.md → "Replay playback"; SAVE/Tests.md → "Runtime integration tests" |
| Z2-R-SAVE-022 | Saves under replay publish the recorded result | S3 | SAVE/Algorithms.md → "Image assembly and compression"; SAVE/Algorithms.md → "Replay playback" |
| Z2-R-SAVE-023 | ZSAV header layout and CRC | S3 | SAVE/Formats.md → "File header"; SAVE/PublicAPI.md → "Zenith_SaveFormat header" |
| Z2-R-SAVE-024 | Section table with one descriptor per section | S3 | SAVE/Formats.md → "Section table"; SAVE/Formats.md → "Section kinds"; SAVE/PublicAPI.md → "Zenith_SaveFormat header" |
| Z2-R-SAVE-025 | CRC-32C implementation | S3 | SAVE/Formats.md → "CRC-32C"; SAVE/PublicAPI.md → "Zenith_SaveFormat header" |
| Z2-R-SAVE-026 | The codec comes from each section's descriptor | S3 | SAVE/Formats.md → "Section kinds"; SAVE/Algorithms.md → "Image assembly and compression" |
| Z2-R-SAVE-027 | Save format limits and untrusted input | S3 | SAVE/Formats.md → "Format limits"; SAVE/Formats.md → "Conventions for every SAVE format"; SAVE/Tests.md → "Fuzz targets" |
| Z2-R-SAVE-028 | Slot metadata section | S3 | SAVE/Formats.md → "Metadata section"; SAVE/PublicAPI.md → "Zenith_SaveFormat header" |
| Z2-R-SAVE-029 | Optional thumbnail section | S3 | SAVE/Formats.md → "Thumbnail section"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-030 | Image bytes are deterministic | S3 | SAVE/Algorithms.md → "Image assembly and compression"; SAVE/Formats.md → "The ZSAV file" |
| Z2-R-SAVE-031 | Save version readability rule | S3 | SAVE/PublicAPI.md → "Zenith_SaveFormat header"; SAVE/Formats.md → "File header" |
| Z2-R-SAVE-032 | Journaled save write sequence | S3 | SAVE/Algorithms.md → "Journaled write sequence"; SAVE/PublicAPI.md → "Zenith_SaveJournal header"; SAVE/Formats.md → "The ZSJN journal" |
| Z2-R-SAVE-033 | One backup per save | S3 | SAVE/Algorithms.md → "Journaled write sequence"; SAVE/Algorithms.md → "Slot policy"; SAVE/Formats.md → "File names and directories" |
| Z2-R-SAVE-034 | An invalid current save never replaces the backup | S3 | SAVE/Algorithms.md → "Journaled write sequence" |
| Z2-R-SAVE-035 | Journal recovery table | S3 | SAVE/Algorithms.md → "Journal recovery"; SAVE/PublicAPI.md → "Zenith_SaveJournal header" |
| Z2-R-SAVE-036 | Recovery converges when re-run | S3 | SAVE/Algorithms.md → "Journal recovery" |
| Z2-R-SAVE-037 | Power loss at every step leaves old or new | S3 | SAVE/Algorithms.md → "Journaled write sequence"; SAVE/Algorithms.md → "Journal recovery"; SAVE/Tests.md → "Journal and power-loss tests" |
| Z2-R-SAVE-038 | Process kill during a real write | S3 | SAVE/Tests.md → "Journal and power-loss tests"; SAVE/Algorithms.md → "Journal recovery" |
| Z2-R-SAVE-039 | Low disk refuses before writing | S3 | SAVE/Algorithms.md → "Journaled write sequence"; SAVE.md → "Failure handling" |
| Z2-R-SAVE-040 | Write cancellation before the commit point only | S3 | SAVE/Algorithms.md → "Journaled write sequence"; SAVE.md → "Cancellation" |
| Z2-R-SAVE-041 | Newer-version saves are not overwritten implicitly | S3 | SAVE/Algorithms.md → "Journaled write sequence"; SAVE/PublicAPI.md → "Zenith_SaveJournal header" |
| Z2-R-SAVE-042 | Save storage confined to the user mount | S3 | SAVE/Formats.md → "File names and directories"; SAVE/PublicAPI.md → "Zenith_SaveStorage header" |
| Z2-R-SAVE-043 | Phase-11 snapshot system | S3 | SAVE/Algorithms.md → "Phase-11 snapshot"; SAVE.md → "Frame-graph nodes SAVE runs" |
| Z2-R-SAVE-044 | Every serialized component captured through reflection | S3 | SAVE/Algorithms.md → "Snapshot capture"; SAVE/Formats.md → "Entities section" |
| Z2-R-SAVE-045 | Entity references survive through save-local references | S3 | SAVE/Formats.md → "Saved entity references"; SAVE/PublicAPI.md → "Zenith_WorldSnapshot header" |
| Z2-R-SAVE-046 | Pending-destroy entities are not saved | S3 | SAVE/Algorithms.md → "Snapshot capture"; SAVE/Formats.md → "Entities section" |
| Z2-R-SAVE-047 | Carried events saved with merge keys | S3 | SAVE/Formats.md → "Events section"; SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-048 | Asynchronous state saved and restored | S3 | SAVE/Formats.md → "Async section"; SAVE/Formats.md → "Publication payloads"; SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-049 | Snapshot bytes are independent of workers and layout | S3 | SAVE/Algorithms.md → "Snapshot capture"; SAVE.md → "Determinism obligations" |
| Z2-R-SAVE-050 | Participant interface for state outside reflection | S3 | SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; SAVE.md → "Upward-callback interfaces SAVE declares" |
| Z2-R-SAVE-051 | Participants restore in order and absent sections reset | S3 | SAVE/Algorithms.md → "Restore pipeline"; SAVE/PublicAPI.md → "Zenith_SaveParticipant header" |
| Z2-R-SAVE-052 | A required participant must be present | S3 | SAVE/Algorithms.md → "Restore pipeline"; SAVE/Formats.md → "Unknown carry section" |
| Z2-R-SAVE-053 | Unknown components and sections are carried | S3 | SAVE/Formats.md → "Unknown carry section"; SAVE/Formats.md → "Entities section"; SAVE/PublicAPI.md → "Zenith_WorldSnapshot header" |
| Z2-R-SAVE-054 | Image budget enforced | S3 | SAVE/Algorithms.md → "Snapshot capture"; SAVE.md → "In-memory structures" |
| Z2-R-SAVE-055 | Snapshot CPU within its row | S4 | SAVE.md → "Rows SAVE owns"; SAVE/Algorithms.md → "Phase-11 snapshot"; SAVE/Tests.md → "Benchmarks and budget harness" |
| Z2-R-SAVE-056 | Out-of-tick snapshot of a paused world | S3 | SAVE/Algorithms.md → "Out-of-tick snapshot"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-057 | Requests queued in merge order | S3 | SAVE/Algorithms.md → "Request intake"; SAVE/Algorithms.md → "Phase-11 snapshot" |
| Z2-R-SAVE-058 | Per-tick request limit | S3 | SAVE/Algorithms.md → "Request intake"; SAVE/Formats.md → "Publication payloads"; SAVE/PublicAPI.md → "Script-facing API"; SAVE.md → "Configuration keys" |
| Z2-R-SAVE-059 | Queue-full and superseded results | S3 | SAVE/Algorithms.md → "Request intake"; SAVE/Algorithms.md → "Write request state machine" |
| Z2-R-SAVE-060 | Save results published at a tick boundary | S3 | SAVE/Formats.md → "Publication payloads"; SAVE/Algorithms.md → "Write request state machine"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-061 | The frame never waits for save I/O | S3 | SAVE/Algorithms.md → "Image assembly and compression"; SAVE.md → "Threads SAVE owns" |
| Z2-R-SAVE-062 | Capture deferral when arenas are busy | S3 | SAVE/Algorithms.md → "Phase-11 snapshot"; SAVE.md → "In-memory structures" |
| Z2-R-SAVE-063 | Slot names validated | S3 | SAVE/PublicAPI.md → "Zenith_SaveTypes header"; SAVE/Formats.md → "File names and directories" |
| Z2-R-SAVE-064 | Slot policy per save kind | S3 | SAVE/Algorithms.md → "Slot policy" |
| Z2-R-SAVE-065 | Autosave rotation | S3 | SAVE/Algorithms.md → "Slot policy"; SAVE/Algorithms.md → "Autosave scheduling" |
| Z2-R-SAVE-066 | Deterministic autosave schedule and flag | S3 | SAVE/Algorithms.md → "Autosave scheduling"; SAVE/Formats.md → "World section" |
| Z2-R-SAVE-067 | Checkpoint saves retained in memory | S3 | SAVE/Algorithms.md → "Checkpoints"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-068 | Checkpoint and autosave entry points for the runtime | S3 | SAVE/RuntimeBridge.md → "Runtime bridge declarations"; SAVE/Algorithms.md → "Shutdown sequence"; SAVE.md → "Failure handling" |
| Z2-R-SAVE-069 | Slot listing states | S3 | SAVE/Algorithms.md → "Listing slots"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-070 | Listing reads a prefix only | S3 | SAVE/Algorithms.md → "Listing slots" |
| Z2-R-SAVE-071 | Slot deletion order | S3 | SAVE/Algorithms.md → "Slot policy" |
| Z2-R-SAVE-072 | Save sequences strictly increase | S3 | SAVE/Algorithms.md → "Image assembly and compression"; SAVE/Formats.md → "File header" |
| Z2-R-SAVE-073 | Load verification and backup fallback | S3 | SAVE/Algorithms.md → "Verification and corruption recovery"; SAVE/Algorithms.md → "Load operation state machine" |
| Z2-R-SAVE-074 | Damaged saves quarantined | S3 | SAVE/Algorithms.md → "Verification and corruption recovery"; SAVE/Formats.md → "File names and directories" |
| Z2-R-SAVE-075 | Load cancellation releases on the writer thread | S3 | SAVE/Algorithms.md → "Load pipeline"; SAVE.md → "Cancellation" |
| Z2-R-SAVE-076 | Incremental restore within a frame budget | S3 | SAVE/Algorithms.md → "Restore pipeline"; SAVE/Algorithms.md → "Restore job state machine"; SAVE/PublicAPI.md → "Zenith_WorldSnapshot header" |
| Z2-R-SAVE-077 | Restore preserves simulation identity | S3 | SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-078 | Restored state independent of batch size | S3 | SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-079 | Exact restores continue bit-identically under the delivery rule | S3 | SAVE.md → "Reproducibility boundaries"; SAVE/Algorithms.md → "Restore pipeline"; SAVE/Formats.md → "Async section" |
| Z2-R-SAVE-080 | A failed restore leaves the world for destruction | S3 | SAVE/Algorithms.md → "Restore pipeline"; SAVE.md → "Failure handling" |
| Z2-R-SAVE-081 | Unresolved references become null | S3 | SAVE/Formats.md → "Saved entity references"; SAVE.md → "Counters" |
| Z2-R-SAVE-082 | Behaviour states saved through the script participant | S3 | SAVE/Formats.md → "Participant section framing"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-083 | Reflected world resources saved | S3 | SAVE/Formats.md → "Resources section"; SAVE/Algorithms.md → "Restore pipeline"; SAVE/Algorithms.md → "Snapshot capture" |
| Z2-R-SAVE-084 | Additive scene set saved through a participant | S3 | SAVE/Formats.md → "Participant section framing"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-085 | Format migration chain one step per minor | S3 | SAVE/Algorithms.md → "Migration pipeline"; SAVE/PublicAPI.md → "Zenith_SaveMigration header" |
| Z2-R-SAVE-086 | A chain gap fails at creation | S3 | SAVE/Algorithms.md → "Migration pipeline"; SAVE/PublicAPI.md → "Zenith_SaveMigration header" |
| Z2-R-SAVE-087 | Component data migrated at restore | S3 | SAVE/Algorithms.md → "Migration pipeline"; SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-088 | Saves of another major refused recoverably | S3 | SAVE/Algorithms.md → "Migration pipeline"; SAVE/Algorithms.md → "Listing slots" |
| Z2-R-SAVE-089 | A migration fixture per save version | S3 | SAVE/Formats.md → "State description file"; SAVE/Formats.md → "Expected output file"; SAVE/Tests.md → "Migration and fixture tests" |
| Z2-R-SAVE-090 | Frozen fixture writers | S3 | SAVE/Formats.md → "Frozen fixture writers" |
| Z2-R-SAVE-091 | A version bump brings its fixture | S3 | SAVE/Formats.md → "Frozen fixture writers"; SAVE/Tests.md → "Migration and fixture tests" |
| Z2-R-SAVE-092 | Shipped-build save fixtures | S6 | SAVE/Formats.md → "Frozen fixture writers"; SAVE/Tests.md → "Migration and fixture tests" |
| Z2-R-SAVE-093 | Resume state written out of tick without a backup | S3 | SAVE/Algorithms.md → "Resume save and restore"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-094 | A cancelled resume write never tears the file | S3 | SAVE/Algorithms.md → "Resume save and restore"; SAVE/Algorithms.md → "Journaled write sequence" |
| Z2-R-SAVE-095 | Resume identity reader | S3 | SAVE/Algorithms.md → "Resume save and restore"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-096 | Resume never migrated and bound to its profile | S3 | SAVE/Algorithms.md → "Resume save and restore"; SAVE/Formats.md → "Last profile record" |
| Z2-R-SAVE-097 | Profile directories | S3 | SAVE/PublicAPI.md → "Zenith_SaveTypes header"; SAVE/Formats.md → "File names and directories" |
| Z2-R-SAVE-098 | Profile switch isolation | S4 | SAVE/Algorithms.md → "Profile switching"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-099 | Shutdown drains then cancels | S3 | SAVE/Algorithms.md → "Shutdown sequence"; SAVE.md → "Startup and shutdown" |
| Z2-R-SAVE-100 | SAVE threads joined before asynchronous I/O is destroyed | S3 | SAVE.md → "Threads SAVE owns"; SAVE/Algorithms.md → "Shutdown sequence" |
| Z2-R-SAVE-101 | Memory consumer reports and sheds nothing | S3 | SAVE.md → "In-memory structures"; SAVE.md → "Rows SAVE owns" |
| Z2-R-SAVE-102 | The Save binding module | S3 | SAVE/PublicAPI.md → "Script-facing API"; SAVE/PublicAPI.md → "Zenith_SaveRegistration header" |
| Z2-R-SAVE-103 | Save misuse raises Luau errors | S3 | SAVE/PublicAPI.md → "Script-facing API"; SAVE.md → "Luau errors versus asserts" |
| Z2-R-SAVE-104 | Save result events | S3 | SAVE/PublicAPI.md → "Zenith_SaveRegistration header"; SAVE.md → "Frame-graph nodes SAVE runs" |
| Z2-R-SAVE-105 | A rolled-back invocation requests nothing | S3 | SAVE/PublicAPI.md → "Script-facing API"; SAVE/Algorithms.md → "Request intake" |
| Z2-R-SAVE-106 | SAVE counters | S3 | SAVE.md → "Counters" |
| Z2-R-SAVE-107 | SAVE log categories and messages | S3 | SAVE.md → "Log categories and messages" |
| Z2-R-SAVE-108 | SAVE zones and profiler rows | S3 | SAVE.md → "Profiling zones"; SAVE.md → "On-screen profiler rows" |
| Z2-R-SAVE-109 | Save dump command | S3 | SAVE.md → "Debug commands and dumps" |
| Z2-R-SAVE-110 | Reproducibility boundaries | S3 | SAVE.md → "Reproducibility boundaries"; SAVE/Algorithms.md → "Replay verification" |
| Z2-R-SAVE-111 | Replays of another build refused | S3 | SAVE/Algorithms.md → "Replay playback"; SAVE.md → "Reproducibility boundaries" |
| Z2-R-SAVE-112 | Saves record the replay position | S3 | SAVE/Formats.md → "File header"; SAVE/Algorithms.md → "Image assembly and compression" |
| Z2-R-SAVE-113 | Resuming a replay from a save | S4 | SAVE/Algorithms.md → "Resuming a replay from a save"; SAVE/Formats.md → "File header"; SAVE.md → "Reproducibility boundaries" |
| Z2-R-SAVE-114 | Replay checkpoints at the configured interval | S4 | SAVE/Formats.md → "Records SAVE writes"; SAVE/Algorithms.md → "Replay recording" |
| Z2-R-SAVE-115 | Restored worlds in the replay log | S4 | SAVE/Formats.md → "Restored-world convention"; SAVE/Algorithms.md → "Replay playback" |
| Z2-R-SAVE-116 | Seeking through a checkpoint equals a full replay | S4 | SAVE/Algorithms.md → "Replay seeking"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-117 | Seeking falls back for coherent checkpoints | S4 | SAVE/Algorithms.md → "Replay seeking"; SAVE.md → "Reproducibility boundaries" |
| Z2-R-SAVE-118 | In-flight work re-issued at a seek | S4 | SAVE/Algorithms.md → "Replay seeking"; SAVE/Formats.md → "Async section" |
| Z2-R-SAVE-119 | Replay readers accept every format version of the release | S6 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE/Formats.md → "Replay limits" |
| Z2-R-SAVE-120 | Checkpoints of older logs load as saves | S6 | SAVE/PublicAPI.md → "Zenith_SaveSystem header"; SAVE/Algorithms.md → "Migration pipeline" |
| Z2-R-SAVE-121 | Replay compatibility across a format bump | S6 | SAVE/Formats.md → "Frozen fixture writers"; SAVE/Tests.md → "Replay tests" |
| Z2-R-SAVE-122 | Persistence components saved like every component | S4 | SAVE/Algorithms.md → "Persistent world and entity state"; SAVE/Formats.md → "Entities section" |
| Z2-R-SAVE-123 | Loading instances re-requested with their hook tag | S4 | SAVE/Formats.md → "Participant section framing"; SAVE/Algorithms.md → "Persistent world and entity state" |
| Z2-R-SAVE-124 | World-level state saved through resources and module participants | S4 | SAVE/Algorithms.md → "Persistent world and entity state"; SAVE/PublicAPI.md → "Zenith_SaveParticipant header" |
| Z2-R-SAVE-125 | Unload save load and reload reproduce state | S4 | SAVE/Algorithms.md → "Persistent world and entity state"; SAVE/Tests.md → "Cross-system failure tests" |
| Z2-R-SAVE-126 | Destroyed persistent entities stay destroyed | S4 | SAVE/Algorithms.md → "Persistent world and entity state"; SAVE/Tests.md → "Cross-system failure tests" |
| Z2-R-SAVE-127 | Persistent gameplay state of the open-world slice | S4 | SAVE/Algorithms.md → "Persistent world and entity state"; SAVE.md → "Stage 4" |
| Z2-R-SAVE-128 | Inconsistent owners excluded from snapshots | S4 | SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; SAVE/Algorithms.md → "Phase-11 snapshot" |
| Z2-R-SAVE-129 | Phase-11 snapshots consistent while jobs run | S4 | SAVE/Algorithms.md → "Phase-11 snapshot"; SAVE/Algorithms.md → "Out-of-tick snapshot" |
| Z2-R-SAVE-130 | Saving during streaming with animation and physics | S4 | SAVE/Tests.md → "Cross-system failure tests"; SAVE/Algorithms.md → "Phase-11 snapshot" |
| Z2-R-SAVE-131 | An interrupted write during that save | S4 | SAVE/Tests.md → "Cross-system failure tests"; SAVE/Algorithms.md → "Journal recovery" |
| Z2-R-SAVE-132 | No half-streamed cells after a restore | S4 | SAVE/Tests.md → "Cross-system failure tests"; SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-133 | No duplicated or lost entities | S4 | SAVE/Tests.md → "Cross-system failure tests" |
| Z2-R-SAVE-134 | Physics and animation coherent after a restore | S4 | SAVE/Tests.md → "Cross-system failure tests"; SAVE.md → "Reproducibility boundaries" |
| Z2-R-SAVE-135 | A thousand save and load cycles leave nothing behind | S4 | SAVE/Tests.md → "Cross-system failure tests" |
| Z2-R-SAVE-136 | User mount round trip without stray files | S3 | SAVE/Tests.md → "Platform storage tests"; SAVE/Formats.md → "File names and directories" |
| Z2-R-SAVE-137 | Kill during a write on the S24 | S3 | SAVE/Tests.md → "Platform storage tests" |
| Z2-R-SAVE-138 | Android backup rules | S6 | SAVE/Formats.md → "File names and directories"; SAVE/Tests.md → "Platform storage tests" |
| Z2-R-SAVE-139 | Saves survive an installed update | S6 | SAVE/Tests.md → "Platform storage tests"; SAVE/Algorithms.md → "Migration pipeline" |
| Z2-R-SAVE-140 | Save journal replay and metadata fuzz targets | S3 | SAVE/Tests.md → "Fuzz targets" |
| Z2-R-SAVE-141 | Fuzzing for the full duration before shipping | S6 | SAVE/Tests.md → "Fuzz targets" |
| Z2-R-SAVE-142 | SAVE death tests | S3 | SAVE/Tests.md → "Death tests"; SAVE.md → "Luau errors versus asserts" |
| Z2-R-SAVE-143 | Save benchmark and smoke | S3 | SAVE/Tests.md → "Benchmarks and budget harness" |
| Z2-R-SAVE-144 | SAVE budget rows allocated in Stage 1 | S1 | SAVE.md → "Rows SAVE owns"; SAVE.md → "Stage 1" |
| Z2-R-SAVE-145 | New SAVE budget rows registered | S1 | SAVE.md → "Pending row needs" |
| Z2-R-SAVE-146 | Save compatibility policy demonstrated | S6 | SAVE/Algorithms.md → "Migration pipeline"; SAVE.md → "Stage 6" |
| Z2-R-SAVE-147 | Replay playback supplies recorded publications | S3 | SAVE/Algorithms.md → "Replay playback"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-148 | Per-capture fidelity sets COHERENT_ONLY | S3 | SAVE.md → "Reproducibility boundaries"; SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; SAVE/Algorithms.md → "Snapshot capture" |
| Z2-R-SAVE-149 | Autosaves replay identically whatever the writer does | S3 | SAVE/Algorithms.md → "Autosave scheduling"; SAVE.md → "Determinism obligations" |
| Z2-R-SAVE-150 | Recording changes no simulation state | S4 | SAVE/Algorithms.md → "Replay recording"; SAVE/Algorithms.md → "Request intake"; SAVE.md → "Determinism obligations" |
| Z2-R-SAVE-151 | Asynchronous source capacity bounded | S3 | SAVE.md → "Asynchronous source capacity"; SAVE/Algorithms.md → "Request intake" |
| Z2-R-SAVE-152 | Replay mode touches no user storage | S3 | SAVE/Algorithms.md → "Replay mode"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-153 | Stage 2 replay core without SCRIPT | S2 | SAVE.md → "Runtime bridge files"; SAVE.md → "Stage 2"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-154 | Bridge failures reach the composition root | S3 | SAVE.md → "Startup and shutdown"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-155 | Save system creation validates and unwinds | S3 | SAVE/PublicAPI.md → "Zenith_SaveSystem header"; SAVE.md → "Startup and shutdown" |
| Z2-R-SAVE-156 | Cross-thread hand-offs of arenas and checkpoints | S3 | SAVE.md → "Cross-thread hand-offs"; SAVE/Algorithms.md → "Checkpoints" |
| Z2-R-SAVE-157 | Script loads hand off with two completion IDs | S4 | SAVE/Algorithms.md → "Script loads and the world host"; SAVE/Algorithms.md → "Load pipeline"; SAVE/PublicAPI.md → "Script-facing API" |
| Z2-R-SAVE-158 | Replay reader lookahead for world transitions | S4 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-159 | Replay writer stops at its limits without blocking | S2 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; SAVE/Formats.md → "Replay limits" |
| Z2-R-SAVE-160 | Last active profile persisted | S3 | SAVE/Formats.md → "Last profile record"; SAVE/Algorithms.md → "Profile switching" |
| Z2-R-SAVE-161 | Format limits tested at and above the limit | S3 | SAVE/Formats.md → "Format limits"; SAVE/Tests.md → "Format and codec tests" |
| Z2-R-SAVE-162 | LIST publication record layout | S3 | SAVE/Formats.md → "Publication payloads" |
| Z2-R-SAVE-163 | Scenario save test runs under the fidelity rule | S4 | SAVE.md → "Scenario tests that consume SAVE"; SAVE.md → "Reproducibility boundaries" |
| Z2-R-SAVE-164 | Restored worlds wait for participant readiness | S3 | SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; SAVE/PublicAPI.md → "Zenith_WorldSnapshot header"; SAVE/Algorithms.md → "Restore pipeline"; SAVE/Algorithms.md → "Restore job state machine" |
| Z2-R-SAVE-165 | Synchronous restores service participant waits | S3 | SAVE/RuntimeBridge.md → "Synchronous restore driver"; SAVE/Algorithms.md → "Resume save and restore"; SAVE/Algorithms.md → "Replay seeking" |
| Z2-R-SAVE-166 | Participants declare the source kinds they re-issue | S3 | SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; SAVE/Formats.md → "Async section"; SAVE/Algorithms.md → "Restore pipeline" |
| Z2-R-SAVE-167 | Loading additive instances re-issued under recorded IDs | S4 | SAVE/Formats.md → "Participant section framing"; SAVE/Formats.md → "Async section"; SAVE/RuntimeBridge.md → "Runtime bridge declarations" |
| Z2-R-SAVE-168 | Replay publication lookup by completion ID | S4 | SAVE/ReplayAPI.md → "Zenith_ReplayLog header" |
| Z2-R-SAVE-169 | Refused script loads release the world load slot | S4 | SAVE/Algorithms.md → "Script loads and the world host"; SAVE/Algorithms.md → "Request intake"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-170 | Over-limit C++ requests share one limit batch | S3 | SAVE/Algorithms.md → "Request intake"; SAVE/Algorithms.md → "Phase-11 snapshot"; SAVE/Formats.md → "Publication payloads"; SAVE.md → "Asynchronous source capacity" |
| Z2-R-SAVE-171 | Exit autosave preempts ordinary writes | S3 | SAVE/Algorithms.md → "Shutdown sequence"; SAVE/PublicAPI.md → "Zenith_SaveSystem header"; SAVE.md → "Cancellation" |
| Z2-R-SAVE-172 | Checkpoint leases block profile switches and outlive unbinding | S3 | SAVE/Algorithms.md → "Checkpoints"; SAVE/Algorithms.md → "Profile switching"; SAVE.md → "Cross-thread hand-offs"; SAVE/PublicAPI.md → "Zenith_SaveSystem header" |
| Z2-R-SAVE-173 | Callbacks enter per-context reentrancy guards | S3 | SAVE/PublicAPI.md → "Zenith_SaveSystem header"; SAVE.md → "Thread affinity per type"; SAVE.md → "Cross-thread hand-offs" |
| Z2-R-SAVE-174 | Metadata pairs copied at intake with a reserved engine prefix | S3 | SAVE/Algorithms.md → "Request intake"; SAVE/Formats.md → "Metadata section"; SAVE/PublicAPI.md → "Script-facing API"; SAVE/PublicAPI.md → "Zenith_SaveTypes header" |
| Z2-R-SAVE-175 | Thumbnails copied into the capture arena | S3 | SAVE/Algorithms.md → "Phase-11 snapshot"; SAVE.md → "Cross-thread hand-offs"; SAVE.md → "In-memory structures" |
| Z2-R-SAVE-176 | Own result payload rebuilt from the header | S3 | SAVE/Formats.md → "File header"; SAVE/Formats.md → "Publication payloads"; SAVE/Algorithms.md → "Image assembly and compression" |

## Cross-area notes

Rows SV1 to SV25 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (plus SAVE's answers recorded in ANIM's row AN17 and GAME's row GA11) list what SAVE needs from, or answers to, ARCH, MOD, CONTRACT, ECS, SCRIPT, SCENE, PLAT, GAME, STREAM, PHYS, ANIM, AI, CINE, ENV, ONLINE, SHIP, BUDGET, PROC, RHI, CORE and COMPRESS; each assumption block of this family names its row.
