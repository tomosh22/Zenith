# GAME — gameplay framework

The entry document of the GAME design family. GAME is the gameplay framework of Zenith2: the game and world lifecycle, cameras, the reusable gameplay primitives (tags, events, state machines, timers, gameplay actions, players and possession, abilities, attributes, effects), their Luau integration, the frame-phase and authority contract (D-PHASES) with the lifecycle commands, and the flagship slice OpenWorldSlice's game-side samples and acceptance package. The requirement catalogue is `Zenith2/Docs/Requirements/GAME.md`.

## Purpose and scope

### What GAME owns

| Subject | Where it is specified |
|---|---|
| the frame-phase contract's stewardship, the authority census, the D-PHASES analysis and ruling, the lifecycle command matrix, the per-command and scenario tests | `Zenith2/Docs/Design/GAME/PhaseContract.md` |
| the `ZenithGameplay` module: limits and enumerations, gameplay time, component registration, tags, events, state machines, timers, gameplay actions, players and possession, ownership, the request queue and the rules system | `Zenith2/Docs/Design/GAME/Framework.md` |
| abilities, attributes and effects | `Zenith2/Docs/Design/GAME/Abilities.md` |
| cameras: rigs, directors, collision, blending, cuts, cinematic suspension | `Zenith2/Docs/Design/GAME/Cameras.md` |
| GAME's content in `ZenithRuntime`: world creation for transitions, the transition record, swap, carry-over, retired results, pause, the world state resource, the `Runtime.*` command kinds, the `Game` namespace, the lifecycle shutdown and teardown | `Zenith2/Docs/Design/GAME/WorldHost.md` |
| the `Gameplay` binding namespace and the `@gameplay` Luau library | `Zenith2/Docs/Design/GAME/Scripting.md` |
| OpenWorldSlice: the four Luau samples, their data and tests, the acceptance package | `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` |
| every GAME test | `Zenith2/Docs/Design/GAME/Tests.md` |

### Explicit exclusions

| Excluded | Decision | Where it lives instead |
|---|---|---|
| quest, dialogue, inventory and interaction as engine features | Z2-D-077 | Luau inside `Zenith2/Games/OpenWorldSlice/Scripts/` (`Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Gameplay sample rules") |
| local multiplayer and split-screen | Z2-D-076 | none: one slot-0 player per world |
| networked player slots and owner-client authority | Z2-D-073 | NET at Stage 7 (the deferred requirement "Player slots and ownership under replication") |
| XR camera output | Z2-D-072 | Stage 7 (the deferred requirement "XR-ready camera output") |
| the lifecycle commands' implementations (spawn and destroy playback, teleport, ragdoll, cinematics) | Z2-D-080 | ECS, SCENE, PHYS, ANIM, CINE; GAME owns the contract and the tests |
| the render camera and view requests | Z2-D-080 | RPROD's `Zenith.Render.Camera` and camera extractor (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera extractor") |
| the frame loop, startup and process shutdown | Z2-D-081 | MOD (`Zenith2/Docs/Design/MOD.md` → "Frame loop driver", "Shutdown sequence") |
| a per-world time scale | Z2-D-080 | the simulation clock's process-wide `sim.timeScale` (JOBS) |

### Inventory items covered

| Item | Subject | Stage | Specified in |
|---|---|---|---|
| Z2-I-GAME-01 | game lifecycle | S3 | WorldHost.md "Game flow state machine", "Game command kinds"; § "Zenith_Module_Gameplay header" |
| Z2-I-GAME-02 | world lifecycle | S3 | WorldHost.md "World lifecycle state machine", "World creation", "World state resource" |
| Z2-I-GAME-03 | cameras | S3 | Cameras.md |
| Z2-I-GAME-04 | tags | S3 | Framework.md "Gameplay tags" |
| Z2-I-GAME-05 | events | S3 | Framework.md "Gameplay events" |
| Z2-I-GAME-06 | reusable state primitives | S3 | Framework.md "State machines"; Scripting.md "State machine helper" |
| Z2-I-GAME-07 | reusable ability primitives | S3 | Abilities.md "Abilities" |
| Z2-I-GAME-08 | timers | S3 | Framework.md "Timers", "Gameplay time" |
| Z2-I-GAME-09 | script integration | S3 | Scripting.md |
| Z2-I-GAME-10 | samples without hard-coding one game's rules | S4 | OpenWorldSlice.md "Gameplay sample rules" |
| Z2-I-GAME-11 … 14 | interaction, quest, dialogue, inventory (game-side) | S4 | OpenWorldSlice.md "Interaction", "Quest", "Dialogue", "Inventory" |
| Z2-I-GAME-15 | player ownership | S3 | Framework.md "Ownership" |
| Z2-I-GAME-16 | player possession | S3 | Framework.md "Possession algorithm" |
| Z2-I-GAME-17 | camera collision | S3 | Cameras.md "Collision query interface", "Collision resolution" |
| Z2-I-GAME-18 | camera blending | S3 | Cameras.md "Blending" |
| Z2-I-GAME-19 | async gameplay actions | S3 | Framework.md "Gameplay actions"; Scripting.md "Async helpers" |
| Z2-I-GAME-20 | attributes | S4 | Abilities.md "Attributes" |
| Z2-I-GAME-21 | effects | S4 | Abilities.md "Effects" |
| Z2-I-GAME-22 | the frame-phase and authority contract (D-PHASES) | S1 | PhaseContract.md |
| Z2-I-GAME-23 … 32 | the ten lifecycle commands | S2–S4 | PhaseContract.md "Lifecycle command contract matrix"; WorldHost.md "World transition", "Shutdown and teardown" |

### Document family

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Design/GAME/PhaseContract.md` | Scope of this file · Contract stewardship · Phase placement of GAME systems · Authority census · Refinement candidates · The D-PHASES ruling · Lifecycle command contract matrix · Per-command tests · Scenario suite · Phase contract budget rows |
| `Zenith2/Docs/Design/GAME/Framework.md` | Scope of this file · Gameplay limits and enumerations · Gameplay time · Component and resource registration · Gameplay tags · Gameplay events · State machines · Timers · Gameplay actions · Players and possession · The request queue · The rules system · The world events system · Gameplay memory accounting |
| `Zenith2/Docs/Design/GAME/Abilities.md` | Scope of this file · Abilities · Attributes · Effects · Ordering within a tick |
| `Zenith2/Docs/Design/GAME/Cameras.md` | Scope of this file · Camera model · Camera components · Collision query interface · Camera director system · Camera math functions · Camera bindings · Camera debug view |
| `Zenith2/Docs/Design/GAME/WorldHost.md` | Scope of this file · World host files and split with MOD · World host state · World lifecycle state machine · Game flow state machine · World creation · World transition · Carry-over · Retired world asynchronous results · Pause arbitration · World state resource · Game command kinds · Game script bindings · Shutdown and teardown · World host counters and logs |
| `Zenith2/Docs/Design/GAME/Scripting.md` | Scope of this file · Binding registration · Binding functions · Misuse errors · Library layout · Async helpers · State machine helper · Effect and ability spec builders · Message helpers · Read helpers |
| `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` | Scope of this file · Slice layout and ticket rule · Gameplay sample rules · Sample data tables · Sample tests · Acceptance package |
| `Zenith2/Docs/Design/GAME/Tests.md` | Test files and targets · ZenithGameplay unit tests · Camera tests · World host tests · Lifecycle per-command tests · Script library tests |

### Conventions

Every C++ name is Zenith Hungarian (Z2-D-034's root conventions as `Zenith2/CLAUDE.md` adopts them); every public type carries one contract block (Z2-D-047); recoverable errors are `Zenith_Result` and script misuse is a Luau error, never an assert (Z2-D-045). "Tick t" in a gameplay rule is a gameplay tick (`Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay time"). Assumptions about interfaces of areas not yet signed off are fenced `<!-- assumption: pending <AREA> -->` and each names its row in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows GA1–GA23). Signed integers are `sint32` (Contracts' spelling; `Zenith_Types.h` declares only the unsigned `uint8` … `uint64`). A boolean stored in a component or record is a `uint8` with the `b` prefix (`m_bActive`, `m_bFollowPossessedPawn`) holding 0 or 1, because a reflected property needs a fixed one-byte layout on every compiler; any other value reads as true and is never written by GAME. Public functions take and return `bool`.

## Dependencies

### Libraries GAME code depends on

| Library | GAME code | Edges |
|---|---|---|
| `ZenithGameplay` | the module | ZenithRuntime, ZenithScene, ZenithECS, ZenithReflect, ZenithCore — exactly `Zenith2/Docs/Architecture.md` → "Library dependency graph"'s row; no ZenithJobs edge, so no GAME system fans out sub-jobs, and no ZenithFlux edge, so the render camera is reached by stable name (`Zenith2/Docs/Design/GAME/Cameras.md` → "Declared access") |
| `ZenithRuntime` (MOD's library) | `Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD" | the runtime's own edges; GAME adds none |
| `Zenith2/Tests/Lifecycle`, `Zenith2/Tests/LifecycleCharacters`, `Zenith2/Tests/LifecycleOpenWorld` | the three stage integration suites | Stage 2: ZenithRuntime, ZenithPlatform (for PLAT's co-resident lifecycle file) and the three ECS backends; Stage 3: + ZenithGameplay, ZenithPhysics, ZenithPhysics_Jolt, ZenithAnimation; Stage 4: + ZenithCinematics, ZenithStreaming, ZenithAI (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures") |
| `Zenith2/Games/OpenWorldSlice` | Luau and content only | the modules of `Zenith2/Docs/Design/MOD.md` → "Repository games" |

### Headings this area relies on

| Owner | Heading | Used for |
|---|---|---|
| ARCH | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases", "Sync points and the tick boundary" | phase placement, playback order |
| ARCH | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition", "Shutdown", "Listener interface", "Lifecycle command summary", "Scenario tests" | the commands GAME's tests prove |
| ARCH | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue" | prologue step 7 |
| ARCH | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" | entry points, contexts |
| ECS | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"; `Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks", "Capacity acceptance"; `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldSimulation header", "Zenith_CommandBuffer header", "Zenith_EntityHandle header" | systems, merges, pause, command buffers |
| ECS | `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder" | registration |
| ECS | `Zenith2/Docs/Design/ECS/Queries.md` → "Own-row and any-row reads", "Resolve and random access"; `Zenith2/Docs/Design/ECS/Tests.md` → "Conformance suite" | the row-ownership discipline of both GAME nodes; ECS's distinct spawn and destroy conformance cases |
| SAVE | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"; `Zenith2/Docs/Design/SAVE/Formats.md` → "Restored-world convention", "Resources section"; `Zenith2/Docs/Design/SAVE.md` → "Reproducibility boundaries" | the save source, the replay lookahead, the serialized continuity base, the scenario's per-capture fidelity |
| SCRIPT (core) | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" | `ASYNC_SOURCE_WORLD_LOAD` (6) and `ASYNC_SOURCE_SCENE_LOAD` (8), values SCRIPT assigns |
| SCENE | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_TransformHierarchy header", "Zenith_SceneLoader header", "Zenith_SceneCapture header", "Zenith_SceneWorld header", "Zenith_LifecycleListener header"; `Zenith2/Docs/Design/SCENE/Loading.md` → "Scene transitions", "Carry-over", "Cancellation"; `Zenith2/Docs/Design/SCENE/Transforms.md` → "Propagation system", "Teleport" | transforms, transitions, capture |
| SCRIPT | `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptAsync header", "Zenith_ScriptWorld header"; `Zenith2/Docs/Scripting/Bindings.md` → "What AddFunction generates", "Public binding types" | async results, bindings |
| MOD | `Zenith2/Docs/Design/MOD.md` → "ZenithRuntime files", "Module lifecycle state machine", "Frame loop driver", "Shutdown sequence", "Registration journal and undo", "The gametest format", "Repository games" | the runtime half |
| RPROD | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration", "Camera cuts" | camera output |
| CORE | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion", "Coordinate systems", "Zenith_Hash64"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" | maths, seed, peak |
| PLAT | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format", "Script-facing input API" | playthrough replay, input contexts |
| BUDGET | `Zenith2/Docs/Budgets.md` → "Flagship slice acceptance package", "Per-system rows" | acceptance package, rows |

### Upward-callback interfaces

GAME implements four interfaces declared by lower layers and declares none of its own in GAME libraries:

| Interface | Declared by | Implemented by | Where |
|---|---|---|---|
| `Zenith_SceneTransitionSink` | ZenithScene | the world host | `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition request intake" |
| `Zenith_AsyncPublicationHandler` | ZenithScript | the world host (`ASYNC_SOURCE_WORLD_LOAD`) | `Zenith2/Docs/Design/GAME/WorldHost.md` → "Publication payloads" |
| `Zenith_LifecycleListener` | ZenithScene | `Zenith_GameplayLifecycleListener` | `Zenith2/Docs/Design/GAME/Framework.md` → "Declared access" |
| `Zenith_CollisionQuery` | ZenithScene (content specified by GAME, pending SCENE) | the Physics module; GAME is the reader | `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface" |

### Assumed interfaces from pending documents

| Assumption | Owner | Row | Fence |
|---|---|---|---|
| a declared-but-absent world resource reads as null and does not fail `Freeze`; ECS stops publishing `world.async_discarded` (the host is its one publisher) | ECS | GA1 | Cameras.md "Collision query interface"; WorldHost.md "Retired world asynchronous results" |
| the call that makes an entity current for `GetChunkOfEntity` in a C++ system | ECS | GA16 | Framework.md "Row ownership in the rules node" |
| `Zenith_CollisionQuery.h` and the resource type `Zenith.CollisionQuery` (PHYS's half is answered: `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CollisionQuery" adopts GA3 (a)); SCENE's discarded preparations counted through the retired table; a `Park` after the world's destruction reaching the retired table through the scene sink ("Publication service") | SCENE | GA2 | Cameras.md "Collision query interface"; WorldHost.md "Retired world asynchronous results" |
| SCENE's "Publication service" states that a `Park` after the world's destruction reaches the retired table through the scene sink (the producer obligation, X255, MOD's sink binding, X274 (3), and the teardown order, X223 and X274 (1), are applied core and MOD text) | SCENE | GA2 (d), cross-doc item X274 (4) | WorldHost.md "Retired world asynchronous results" |
| per-bound-entity cinematic lifecycle events | CINE | GA4 | Cameras.md "Cinematic suspension" |
| `FindCommandKind`, `GetWorldControl`, the factored world creation, the teardown table, the quit flag, `Zenith_Runtime::RecordTestAssertionFailure()`, `Zenith_FrameLoop_PrecompileSwap`, the shutdown seams, the move of `StepActivations`, the GAME runtime registrations, the test hooks; MB16's Stage 2 producer and the shared `Internal/Zenith_WorldHost.h` | MOD | GA5, GA23 | Framework.md "Command kinds and payloads"; WorldHost.md |
| input-script recording and cross-swap ticks; Z2T-PLAT-044's Lifecycle call carrying the union `DEPENDS`; stable names for PLAT's five input resources | PLAT | GA6, GA21, GA22 | OpenWorldSlice.md "Acceptance package"; PhaseContract.md "Suite target and fixtures", "Census of signed-off designs" |
| `DetachAsyncResults`, `Retire`, `GetInFlightCount`, `GetDiscardedCount`; the `@gameplay` alias; the ZS41xx misuse codes | SCRIPT | GA10 | WorldHost.md "Retired world asynchronous results"; Scripting.md "Library layout" |
| the replay form's null-save call of `Zenith_WorldHost_BeginTransitionFromSave` for a recorded Result 0 with no session after the swap (the rest of GA11 is answered: `FindPublication` in `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header", the reworded replay sentence (f) in `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"; GAME adopts SAVE's SV25 restore timeout) | SAVE | GA11 (g) | WorldHost.md "Transition sources", "Failed save loads" |

### Areas that consume GAME

| Consumer (A5 blocker row) | What it needs, by its name | Answered by |
|---|---|---|
| ECS (EB6) | "GAME: Rule D-PHASES refinement" and "Stage 1 per-system budget ticket for GAME" (Z2-B-GAME-003, `ecs.maxEventsPerTick`, `ECS_CAPACITY_CLASS_EVENT_RECORDS`) | `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Ruling ticket", "Event queue capacity" |
| SCRIPT (SB9) | the same two tickets | same |
| SCRIPT (SB19) | OpenWorldSlice's Luau quest, dialogue and inventory samples (Stage 4) and each flagship slice's playthrough script (Stage 6) | the producers by exact title (`Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"): "OpenWorldSlice quest sample", "OpenWorldSlice dialogue sample", "OpenWorldSlice inventory sample" (with "OpenWorldSlice interaction sample") at Stage 4; OpenWorldSlice's playthrough script is "OpenWorldSlice playthrough integration" (its Luau asserts and segment markers, Stage 6) with the recording tickets "OpenWorldSlice playthrough recording S01–S04" and "OpenWorldSlice full playthrough recording"; CinematicSlice's and Game2D's playthrough scripts are CINE's and TWOD's (`Zenith2/Docs/Design/MOD.md` → "Repository games"), not GAME's |
| PLAT (PB7) | "Rule D-PHASES refinement" | `Zenith2/Docs/Design/GAME/PhaseContract.md` → "What the ruling unblocks" |
| MOD (MB16) | `Zenith_WorldHost_BeginTransition(Zenith_RuntimeState&, uint16 uFromWorldTag, const Zenith_WorldTransitionDesc&)` in `ZenithRuntime/Internal/Zenith_WorldHost.cpp`; the swap half of `Test_Runtime_ExitAfterTicks_RunsExactlyN` | the Stage 2 producer "GAME world host: lifecycle shutdown and teardown order", which Z2T-MOD-024 is blocked by: it lands the declaration and a Stage 2 definition that refuses with `RUNTIME_ERROR_TRANSITION_IN_PROGRESS`, and "GAME world host: transition intake and record" (S3) replaces the body (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD"; GA23); the swap half of MOD's test is extended by "GAME world host: swap and carry-over" (`Zenith2/Docs/Design/GAME/Tests.md` → "World host tests"); the full declaration `Zenith_Result<Zenith_OperationHandle, Zenith_RuntimeError> Zenith_WorldHost_BeginTransition(Zenith_RuntimeState& xState, uint16 uFromWorldTag, const Zenith_WorldTransitionDesc& xDesc)` in `Internal/Zenith_WorldHost.h` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD"; MOD's call site is unaffected by GAME's defining file `Internal/Zenith_WorldTransition.cpp`); "Transition request intake", "Swap procedure" (the primary-world tick count continues summed across swaps) |
| MOD (MB15) | `Zenith_Runtime::RecordTestAssertionFailure()` as the one exit-5 path | GAME's test-failure playback and replay divergence both call it and keep no counter of their own (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Game command kinds", "Replay of a transition") |
| SCENE (S1, ScB14) | the ruling on `Zenith.LocalTransform`'s owning phase | candidate R4 of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Refinement candidates" |
| ANIM (AN7) | the owner of phase-4 root-motion application | candidate R13 (the Q72 ruling): PHYS's controller node and ANIM's own phase-4 node; GAME none (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Refinement candidates"; GA15) |
| STREAM (ST18) | sources on the pawn and the camera; readiness reads; census rows; scenario support | `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census rows added by subsequent designs", "Scenario WorldTransitionWithInFlightAsync" |
| UI (GA13) | MENU semantics: a script sets it, UI's `resumeWorld` clears it | `Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration" |
| SAVE (SV7, SV21) | the load-from-save transition and its seams; `ContinuityTickBase` serialized; the handling of a load that fails before or after the hand-off | `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition sources", "World state resource", "Swap procedure", "Failed save loads", "Publication payloads" |
| SCENE (ScB19) | the world host's transition half (ruling WORLD-SWAP-OWNER): the `Zenith_SceneTransitionSink` implementation, isolated-world creation in prologue step 7, the swap and the Z2-B-CORE-007 peak reservation at intake | Z2T-SCENE-074 blocks on "GAME world host: swap and carry-over", which is itself blocked by "GAME world host: transition intake and record"; the first lands the sink, the intake with its reservation, the record and isolated-world creation, the second the swap (§ "Ticket slices and ordering" item 3; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition request intake", "World creation", "Swap procedure") |
| LIGHT (L17) | the camera cut when a teleport moves the active camera's target | `Zenith2/Docs/Design/GAME/Cameras.md` → "Output and cuts" |

## Library and file layout

### Target summary

```cmake
# Zenith2/Engine/ZenithGameplay/CMakeLists.txt
zenith_add_library(ZenithGameplay KIND MODULE MODULE_NAME Gameplay
	DEPENDS ZenithRuntime ZenithScene ZenithECS ZenithReflect ZenithCore
	SUPPORTED ZENITH_GPU)
```

First stage S3 (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping"). The target's `Tests/` folder builds `ZenithGameplay_Tests` (`Zenith2/Docs/Design/GAME/Tests.md` → "Test files and targets"). `ZENITH_SUPPORTED` matrix: `ZENITH_GPU` (Windows and Android, every module), so the module is in every preset that builds games.

### ZenithGameplay files

| Path under `Zenith2/Engine/ZenithGameplay/` | Contents |
|---|---|
| `Zenith_Module_Gameplay.h` | § "Zenith_Module_Gameplay header" |
| `Zenith_GameplayTypes.h` | limits and enumerations (`Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay limits and enumerations") |
| `Zenith_GameplayComponents.h` | tags, timers, state machines, actions, player, pawn, owner components |
| `Zenith_GameplayAbilities.h` | ability, attribute and effect specs, records, components |
| `Zenith_GameplayCameras.h` | rig and director components, camera enums |
| `Zenith_GameplayEvents.h` | the seventeen event structs |
| `Zenith_GameplayMath.h` | tag matching, ownership resolution, attribute evaluation, camera math, spec validation |
| `Internal/Zenith_GameplayModule.cpp` | `RegisterTypes`, `Start`, the module instance's `AttachWorld`/`DetachWorld` |
| `Internal/Zenith_GameplayWorldState.h`, `.cpp` | the per-world state (queue, query slots, caches) |
| `Internal/Zenith_GameplayRequests.h`, `.cpp` | payload structs, recording helpers, playback functions |
| `Internal/Zenith_GameplayRequestQueue.h` | the queue resource |
| `Internal/Zenith_GameplayRules.cpp`, `Internal/Zenith_GameplayRulesTags.cpp`, `…Timers.cpp`, `…States.cpp`, `…Actions.cpp`, `…Possession.cpp`, `…Abilities.cpp`, `…Effects.cpp`, `…Attributes.cpp` | the rules system split by primitive (the Complexity gate's file limits) |
| `Internal/Zenith_GameplayWorldEvents.cpp` | `Gameplay.WorldEvents` |
| `Internal/Zenith_GameplayCameras.cpp`, `Internal/Zenith_GameplayCameraRigs.cpp` | `Gameplay.Cameras` and rig evaluation |
| `Internal/Zenith_GameplayLifecycleListener.cpp` | the listener |
| `Internal/Zenith_GameplayMath.cpp` | the pure functions |
| `Internal/Zenith_GameplayScriptApi.h`, `Internal/Zenith_GameplayScriptBindings.cpp` | the `Gameplay` namespace |
| `Internal/Zenith_GameplayConsole.cpp` | `gameplay.camera.debug` and the camera overlay |
| `Internal/Zenith_GameplayLog.h` | log category `gameplay` |
| `Scripts/*.luau`, `Scripts/Tests/*.test.luau` | the `@gameplay` library (`Zenith2/Docs/Design/GAME/Scripting.md` → "Library layout") |
| `Tests/*.cpp`, `Tests/Fixtures/` | `Zenith2/Docs/Design/GAME/Tests.md` → "ZenithGameplay unit tests", "Camera tests" |

`ZenithGameplay` has no `Internal/Windows|Android|Linux/` files: nothing it does differs by platform.

### ZenithRuntime GAME files

`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD" lists every file and function; `Zenith2/Engine/ZenithScene/Zenith_CollisionQuery.h` is specified by GAME and owned by SCENE (GA2).

### Integration suite and the slice

`Zenith2/Tests/Lifecycle/` (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures") and `Zenith2/Games/OpenWorldSlice/` (`Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Slice layout and ticket rule").

## Public API

### Header map

| Header | Declares | Specified in |
|---|---|---|
| `ZenithGameplay/Zenith_Module_Gameplay.h` | the two entry points | § "Zenith_Module_Gameplay header" |
| `ZenithGameplay/Zenith_GameplayTypes.h` | `uGAMEPLAY_*` limits, every gameplay enumeration, `Zenith_GameplayError` | `Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay limits and enumerations" |
| `ZenithGameplay/Zenith_GameplayComponents.h` | `Zenith_GameplayTagsComponent`, `…TimersComponent`, `…StateMachinesComponent`, `…ActionsComponent`, `…PlayerComponent`, `…PawnComponent`, `…OwnerComponent` and their records | Framework.md "Tag component", "State machines", "Timers", "Action component", "Player and pawn components", "Ownership" |
| `ZenithGameplay/Zenith_GameplayAbilities.h` | `Zenith_GameplayAbilitySpec`, `…AbilityRecord`, `…AbilitiesComponent`, `…AttributesComponent`, `…EffectSpec`, `…ActiveEffect`, `…EffectsComponent` | `Zenith2/Docs/Design/GAME/Abilities.md` |
| `ZenithGameplay/Zenith_GameplayCameras.h` | `Zenith_GameplayCameraRigKind`, `…CameraBlendCurve`, `…CameraRigComponent`, `…CameraDirectorComponent` | `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera components" |
| `ZenithGameplay/Zenith_GameplayEvents.h` | the event structs | Framework.md "Event types" |
| `ZenithGameplay/Zenith_GameplayMath.h` | `Zenith_GameplayTagMatches`, `…TagsHasMatch`, `…TagsFindExact`, `…IsValidTagText`, `…ResolveOwningPlayer`, `…EvaluateAttribute`, `…ValidateAbilitySpec`, `…ValidateEffectSpec`, `…BlendWeight`, `…LagAlpha`, `…WrapAngle`, `…OrbitRotation`, `…ArmPosition`, `Zenith_GameplayCurrentTick` | Framework.md, Abilities.md, Cameras.md "Camera math functions" |
| `ZenithRuntime/Zenith_WorldControl.h`, `ZenithRuntime/Zenith_WorldState.h` | `Zenith_WorldPauseReason`, `Zenith_WorldControl`, `Zenith_WorldOrigin`, `Zenith_WorldStateResource`, `Zenith_CarryOverComponent` | WorldHost.md "Pause arbitration", "World state resource", "Carry-over" |
| `ZenithScene/Zenith_CollisionQuery.h` (pending SCENE) | `Zenith_CollisionSweepDesc`, `Zenith_CollisionHit`, `Zenith_CollisionQuery` | Cameras.md "Collision query interface" |

### Zenith_Module_Gameplay header

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_Module_Gameplay.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_Gameplay
/// Ownership: RegisterTypes registers GAME's components, the request-queue resource, events, command payload structs and
///   configuration keys; Start creates the module instance the runtime owns.
/// Thread affinity: MAIN_THREAD, at startup steps 6 and 15.
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION for a rejected registration or an invalid gameplay.* value (logged by the context or the
///   module); MODULE_ERROR_OUT_OF_MEMORY when the instance cannot be allocated.
/// Version: since Z2 Stage 3. Module entry points per [Z2-N-ARCH-168].
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Gameplay_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Gameplay_Start(
	Zenith_ModuleStartContext& xContext);
```

| Entry point step | Action | Failure |
|---|---|---|
| RegisterTypes 1 | the twelve components, the request-queue resource, the seventeen events, the ten payload structs and the two spec value structs, in `Zenith2/Docs/Design/GAME/Framework.md` → "Component and resource registration" order | `MODULE_ERROR_REGISTRATION` |
| RegisterTypes 2 | the configuration key `gameplay.maxRequestsPerTick` (§ "Configuration keys") | `MODULE_ERROR_REGISTRATION` |
| Start 1 | read and validate the configuration; allocate the module instance | `MODULE_ERROR_REGISTRATION` (`GAMEPLAY_ERROR_INVALID_CONFIG`), `MODULE_ERROR_OUT_OF_MEMORY` |
| Start 2 | register the ten command kinds (`Gameplay.Tags` … `Gameplay.Possess`, phases 1–10) | `MODULE_ERROR_REGISTRATION` |
| Start 3 | register the binding table, the memory consumer and the console command | `MODULE_ERROR_REGISTRATION` |
| Start 4 | when `GetProfilerOverlay()` is non-null, register the `game` tab (§ "On-screen profiler rows"); a refused row panel (`PROFILER_ERROR_INVALID_DESC`) is a defect of the row table | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 1 | resolve the command-kind IDs (first world), `Zenith.Render.Camera` and its two properties by stable name, `Zenith.LocalTransform`, `Zenith.TransformHierarchy` | `GAMEPLAY_ERROR_MISSING_TYPE` → `MODULE_ERROR_REGISTRATION` |
| AttachWorld 2 | create the world state and its queue (`capacity × 128` bytes), add the queue resource | `MODULE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_REGISTRATION` |
| AttachWorld 3 | add `Gameplay.WorldEvents`, `Gameplay.Rules`, `Gameplay.Cameras` and the lifecycle listener — only when `Zenith_WorldAttachContext::GetWorldPurpose()` is `WORLD_PURPOSE_PLAY`; in a world of purpose `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` the step adds nothing (steps 1 and 2 still run, so the bindings resolve and the request queue exists; no system drains it, and it is released with the world) | `MODULE_ERROR_REGISTRATION` |

**Edit and preview worlds.** No gameplay-framework system, listener or world-events node runs in a world of purpose `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` (cross-area rows ED7, EdB25 and ED37; the purpose is the world creator's, `Zenith_WorldCreateDesc::m_ePurpose`, read through `GetWorldPurpose`, `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "World attach context lookups"), so the camera director never writes the editor's camera and no timer, ability or rule advances while a scene is edited; a play world created by the editor has purpose PLAY and runs everything. `Test_GameplayModule_SchedulePlacement` carries the SUBCASE that proves it.

Each step has a fault point swept by `Test_GameplayModule_StartAttach_FaultSweep`; the runtime undoes registrations through the journals (`Zenith2/Docs/Design/MOD.md` → "Registration journal and undo"). `DetachWorld` releases the world state and clears the tag slot of the module's world table.

### Handle kinds used

`Zenith_EntityHandle` (every entity reference in components, payloads and events), `Zenith_StringID` (every name), `Zenith_OperationHandle` (world transitions), `Zenith_AssetHandle<Zenith_Scene>` (transition targets), `Zenith_PersistentID` (carry-over and the world state's scenes) — all `Zenith2/Docs/Contracts.md` → "Handle kinds".

### Rules across the GAME API

- A rule-owned property changes only through a request kind; a script's direct write is a schedule error ([Z2-N-ARCH-483]).
- Every event and request is keyed by simulation ID and merge order, never by storage order.
- No GAME type holds a pointer into another world, and every payload carries its target's world tag in the handle.
- No GAME code reads real time, except the pause deadline, which changes no simulation state.

## Data structures and formats

### Format index

GAME defines no binary file format. Its persisted data is reflected component data (saved, cooked and migrated by SCENE and SAVE through the registry), the behaviour state of the OpenWorldSlice samples (SCRIPT's format), and two text formats it consumes: the `zenith2.input-script` JSON of `Playthrough.replay` (PLAT's format) and the Markdown tables of the acceptance package (`Zenith2/Docs/Budgets.md` → "Flagship slice acceptance package").

### In-memory sizes that matter

| Structure | Bytes | Count |
|---|---|---|
| `Zenith_GameplayTagsComponent` | 84 | per tagged entity |
| `Zenith_GameplayAttributesComponent` | 328 | per entity with attributes |
| `Zenith_GameplayEffectsComponent` | 904 | per entity with effects |
| `Zenith_GameplayAbilitiesComponent` | 648 | per entity with abilities |
| `Zenith_GameplayTimersComponent` | 264 | per entity with timers |
| `Zenith_GameplayStateMachinesComponent` | 104 | per entity with machines |
| `Zenith_GameplayActionsComponent` | 520 (eight 64-byte action records) | per entity with actions |
| player, pawn, owner | 24, 16, 16 | per entity |
| `Zenith_GameplayCameraRigComponent`, `…DirectorComponent` | 144, 120 | per rig, per output camera |
| `Zenith_GameplayQueuedRequest` × `gameplay.maxRequestsPerTick` | 128 × 16,384 = 2 MiB | per world |
| `Zenith_WorldStateResource` | 64 | per world |
| `Zenith_WorldTransitionSinkSlot` | 4,128 (16 + 256 × 16 + 8 + 4 + 4, `static_assert`ed) | per world |
| a possession follow-up record, an action probe, a follow-list entry | 32, 48, 24 | node arena of `Gameplay.Rules`: at most 3 follow-ups at once, one probe per foreign-subject action, one entry per player |
| `Zenith_WorldTransitionRecord` | ≈ 4,300 (256 persistent IDs) | one per process |

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading |
|---|---|
| request playback and drain | Framework.md "Playback into the queue", "Rules algorithm" |
| tag counting and matching | Framework.md "Tag reference counting", "Tag matching" |
| timers, state machines, actions | Framework.md "Timers", "State machines", "Action evaluation" |
| possession and ownership | Framework.md "Possession algorithm", "Ownership" |
| abilities, attributes, effects | Abilities.md "Ability activation algorithm", "Current value evaluation", "Effect application algorithm", "Effect expiry and periods" |
| cameras | Cameras.md "Director algorithm" and its subsections |
| transitions, carry-over, pause, teardown | WorldHost.md "Prologue step 7 procedure", "Swap procedure", "Carry-over", "Pause arbitration", "Teardown order algorithm" |

### State machine index

| Machine | Heading |
|---|---|
| world lifecycle | WorldHost.md "World lifecycle state machine" |
| game flow | WorldHost.md "Game flow state machine" |
| transition record | WorldHost.md "Transition record state machine" |
| ability state (READY, ACTIVE, COOLDOWN) | Abilities.md "Ability activation algorithm", "Ability end and expiry" |
| action state | Framework.md "Action evaluation" |

### Tick-level flow of GAME work

| Phase | GAME node | Work |
|---|---|---|
| prologue step 7 | world host | pause masks, transition intake, activation, swaps, retired tables |
| 1 | `Gameplay.WorldEvents` | `WorldStarted` at the first tick |
| 2, 10 | behaviours and chunk systems | record requests and messages through the `Gameplay` bindings |
| sync points 1–10 | playback | requests appended to the queue |
| 7 | `Gameplay.Rules`, then `Gameplay.Cameras` | the rules algorithm; the camera director, output by commands |
| sync point 7 | playback | camera output applied before phase 8 |
| 9 | ECS delivery | gameplay events reach Late scripts |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| gameplay components, events, specs, payloads | `IMMUTABLE` values; rows follow their authority |
| `Zenith_GameplayRequestQueue` | written by one world's sync-point node at a time; drained by `Gameplay.Rules` |
| `Zenith_WorldControl` | `ANY_THREAD` |
| `Zenith_WorldStateResource` | main thread out of tick; readers in any phase |
| `Zenith_CollisionQuery` | `ANY_THREAD` for readers outside phase 5 |
| world host functions | `MAIN_THREAD`, no frame running; the sink on the world's sync-point node |

### Frame-graph placement

`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Phase placement of GAME systems" lists every node with its phase, affinity and declared access; all three systems are `JOB_AFFINITY_ANY` single nodes without sub-jobs.

### Lock-order entries

None. The world host's cross-thread state is atomics; the queue and the sink slots rely on the per-world serial order of sync points.

### Construction and shutdown

Module: § "Zenith_Module_Gameplay header". World host: `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation", "Shutdown and teardown". A world's GAME state is destroyed by the module's `DetachWorld` inside the teardown step `MODULES`.

### Error enumerations

| Enumeration | Header | Values |
|---|---|---|
| `Zenith_GameplayError` | `Zenith_GameplayTypes.h` | `GAMEPLAY_ERROR_OUT_OF_MEMORY = 1`, `REGISTRATION`, `MISSING_TYPE`, `INVALID_CONFIG` |
| `Zenith_GameplayEffectRejectReason`, `…AbilityFailure`, `…AbilityEndReason`, `…EffectRemoveReason`, `…ActionState` | `Zenith_GameplayTypes.h` | reported in events, not as results |
| `Zenith_WorldTransitionFailure` | `ZenithRuntime/Internal/Zenith_WorldTransition.h` | 1–13, the transition operation's failure code (12 `SCHEDULE`: the new world's `Freeze` or the swap's trial compile; 13 `IN_PROGRESS`: an ID-holding sink or `Save.Load` request taken while another world's transition is in flight) |
| `Zenith_RuntimeErrorCode` | MOD's | `RUNTIME_ERROR_TRANSITION_PEAK_REFUSED`, `…_IN_PROGRESS`, `…_UNKNOWN_WORLD` returned by `RequestWorldTransition` |

### Luau errors and asserts

Script misuse raises ZS4101–ZS4112 (`Zenith2/Docs/Design/GAME/Scripting.md` → "Misuse errors") and the `Game` namespace's ZS4113–ZS4117 (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Game script bindings"); semantic failures are events and counters. Asserts guard programmer errors only: a direct `BeginTransition` with a null scene, a null prepared save handed to `BeginTransitionFromSave` outside a replay, a world-load publication a normal run cannot produce (rows 2, 4 and 6 of `Zenith2/Docs/Design/GAME/WorldHost.md` → "Publication payloads", `ZENITH_ASSERT`), a state transition outside a table (`ZENITH_UNREACHABLE`). A fifth or subsequent retired result table still waiting on a producer is not an assert: it is held in the overflow list, logged and counted (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results").

## Diagnostics and profiling

### Counters

| Counter | Kind | Meaning |
|---|---|---|
| `gameplay.requests.dropped`, `.stale`, `.missing_component`, `.malformed`, `.no_world` | CUMULATIVE | request outcomes at playback and drain |
| `gameplay.requests.high_water.<tag>` | GAUGE | the queue's high-water mark per tick |
| `gameplay.events.rejected` | CUMULATIVE | entities whose events the merge rejected |
| `gameplay.tags.full`, `.count_saturated`, `.remove_absent` | CUMULATIVE | tag outcomes |
| `gameplay.state.full`, `.redundant`; `gameplay.timers.full`, `.fires_clamped`; `gameplay.actions.full`, `.duplicate_name` | CUMULATIVE | primitive outcomes |
| `gameplay.possession.rejected`, `.redundant`; `gameplay.owner.chain_too_long` | CUMULATIVE | possession and ownership |
| `gameplay.abilities.full`, `.invalid_spec`, `.end_inactive`; `gameplay.effects.unknown_attribute`; `gameplay.attributes.non_finite`, `.full`, `.rejected` | CUMULATIVE | ability, effect and attribute outcomes |
| `gameplay.camera.cuts`, `.no_rig`, `.lost_target`, `.clamped`, `.bad_kind`, `.non_finite_input`, `.no_collision_provider`, `.lost_director`, `.player_mismatch`, `.rig_contended` | CUMULATIVE | camera outcomes (`.lost_director` and `.player_mismatch` from `Gameplay.Rules` step 9; `.rig_contended` from `Zenith2/Docs/Design/GAME/Cameras.md` → "Rig selection") |
| the world host's counters | — | `Zenith2/Docs/Design/GAME/WorldHost.md` → "World host counters and logs" |

### On-screen profiler rows

GAME owns the `game` tab (title "Gameplay", owner area GAME, tab rank 60) of DEVT's overlay. Under orchestrator ruling DEVT-AREA-REGISTRATION the Gameplay module registers it itself: `Start` step 4 calls `Zenith_ModuleStartContext::GetProfilerOverlay()` and, when it is non-null (development builds, not headless), `AddRowPanel(Zenith_ProfilerRowPanelDesc{ "game", "Gameplay", "GAME", 60, axPANEL_ROWS_GAME })`, keeping the returned token in the module instance so the tab goes with the module; a null overlay registers nothing (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Built-in panel catalogue"; cross-area row DV31). The rows are one `inline constexpr Zenith_ProfilerRowDesc axPANEL_ROWS_GAME[]` in `Zenith2/Engine/ZenithGameplay/Internal/Zenith_GameplayPanelRows.h`, in `Zenith_ProfilerRowDesc` field order; a ticket that adds a GAME counter the tab should show adds its row here and to that table in the same diff:

| Label | Expression | Kind | Unit | Scale | Limit | Budget row | Exactness |
|---|---|---|---|---|---|---|---|
| `Gameplay rules` | `zone_ms("Gameplay.Rules@W*")` | VALUE | MILLISECONDS | 1 | — | — | exact |
| `Gameplay cameras` | `zone_ms("Gameplay.Cameras@W*")` | VALUE | MILLISECONDS | 1 | — | — | exact |
| `Requests dropped` | `gameplay.requests.dropped_delta` | SPARKLINE | COUNT | 1 | — | — | exact |
| `Events rejected` | `gameplay.events.rejected_delta` | SPARKLINE | COUNT | 1 | — | — | exact |
| `Camera cuts` | `gameplay.camera.cuts_delta` | SPARKLINE | COUNT | 1 | — | — | exact |
| `Transitions` | `world.transitions.requested` | VALUE | COUNT | 1 | — | — | exact |
| `Transitions failed` | `world.transitions.failed + world.transitions.cancelled` | VALUE | COUNT | 1 | — | — | exact |
| `Async results discarded` | `world.async_discarded` | VALUE | COUNT | 1 | — | — | exact |
| `Retired tables in overflow` | `world.async_retired_overflow` | VALUE | COUNT | 1 | — | — | exact |

The world host's counters (ZenithRuntime, no module of its own) are read by name in the same tab; no row reads per-world gauges, which the counter group lists.

### Log categories

`gameplay` (module: rejected requests with their reason, authoring errors such as two slot-0 players, camera configuration problems), `runtime.world` and `runtime.game` (world host). Every message text is quoted where its rule is specified.

### Profiler zones

`Gameplay.Rules`, `Gameplay.Cameras`, `Gameplay.WorldEvents` (node zones by GRAPH's trampoline), inner zones `Gameplay.Rules.Requests`, `.Timers`, `.Effects`, `.Abilities`, `.Actions`, `.Attributes`, and the world host's zones. The on-screen profiler shows the three nodes in the CPU view and the queue high-water mark in the gameplay counter group.

### Debug views and console commands

<!-- assumption: pending DEVT -->
DEVT's development console offers a registration call taking a command name, a handler with user data and a one-line help text, absent in Shipping (the same assumption as `Zenith2/Docs/Design/GRAPH.md` → "Console commands"). GAME registers through it, from `Internal/Zenith_GameplayConsole.cpp`, and unregisters in the module instance's destructor. Cross-area row GA14.
<!-- /assumption -->

| Command | Effect |
|---|---|
| `gameplay.camera.debug <0\|1>` | the camera overlay (`Zenith2/Docs/Design/GAME/Cameras.md` → "Camera debug view") |
| `gameplay.dump <entity simulation ID>` | logs every gameplay component of the entity, field by field, under `gameplay` |
| `gameplay.requests.stats` | logs the per-world queue capacity, count, high-water mark and drops |
| `world.pause <tag> <0\|1>` | adds or removes `WORLD_PAUSE_REASON_DEBUGGER` on a world |

### Configuration keys

| Key | Type, range | Default | Simulation-affecting | Read at |
|---|---|---|---|---|
| `gameplay.maxRequestsPerTick` | u32, 256 … 262,144 | 16,384 (until the pending row "Gameplay request capacity per tick" is allocated) | yes | `AttachWorld` |
| `world.transitionReserveFloorBytes` | u64, 0 … 2⁴⁰ | 33,554,432 | no | each transition intake (registered by GAME's runtime file through the runtime's key registration, GA5 (g)) |

GAME also supplies the value of ECS's `ecs.maxEventsPerTick` through Z2-B-GAME-003 (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Event queue capacity"); it does not register that key.

## Authoring and cooking

### What GAME authors

Gameplay components are authored in scenes and prefabs like any reflected component (SCENE's documents); the editor's inspector edits them through reflection (EDIT). Camera rigs are entities with a rig component; a level's player start is a pawn with `PAWN_FLAG_AUTO_POSSESS`. GAME authors no asset kind.

### Editor surfaces

EDIT's generic inspector shows every gameplay component; the camera overlay (§ "Debug views and console commands") is GAME's only visual tool. GAME requires of EDIT only that play worlds are created with origin `WORLD_ORIGIN_EDITOR_PLAY` through the world creation desc (GA5).

### Cook steps

None of GAME's own: components cook with scenes (SCENE), the `@gameplay` Luau library is compiled by SCRIPT's cook step with the module's source root, and the OpenWorldSlice samples cook with the game's scripts.

### Migration

A layout change of a gameplay component is a component version step with migration ops registered beside the type (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder"); OpenWorldSlice's behaviour states migrate through `StateVersion` and `Migrate` (SCRIPT).

## Failure handling

### Failure table

| Failure | Behaviour | Test |
|---|---|---|
| allocation failure at start or attach | unwind, mapped module error, startup exit code per MOD | `Test_GameplayModule_StartAttach_FaultSweep` |
| missing engine type at attach | `GAMEPLAY_ERROR_MISSING_TYPE` → `MODULE_ERROR_REGISTRATION` | `Test_GameplayModule_MissingRenderCameraFailsAttach` |
| request queue full | drop newest, counted, `RequestsDropped` next tick | `Test_GameplayRequests_QueueFullDropsNewest` |
| malformed or stale request | skipped, counted | `Test_GameplayRequests_MalformedNeverAsserts` |
| event capacity exceeded at merge | the entity's events of that tick lost, rows kept, counted and logged | `Test_GameplayRules_EventCapacityRejectKeepsRows` |
| component full (tags, effects, abilities, timers, machines, actions) | request rejected with its event and counter | the primitive tests |
| script misuse | Luau error, nothing recorded | `Test_GameplayBindings_Golden`, `Test_WorldHost_GameBindingMisuse` |
| no collision provider | camera without collision, counted once | `Test_Cameras_NoCollisionProvider` |
| lost camera target | rig falls back to its own pose, counted | `Test_Cameras_RigSelection` |
| transition peak refused | request refused, nothing loaded | `Test_WorldTransition_PeakRefused` |
| preparation, activation or restore failure | isolated world destroyed, old world unchanged, operation FAILED | `Test_WorldTransition_FaultSweep` |
| swap capture failure | swap aborted whole | `Test_WorldTransition_SwapFailureLeavesOldWorld` |
| schedule error of the new world, or trial compile failure at the swap | transition `FAILED` with `SCHEDULE`, old world untouched | `Test_WorldTransition_ScheduleFailureLeavesOldWorld` |
| replay divergence at a transition | record `FAILED` with `REPLAY_DIVERGED`, `Zenith_Runtime::RecordTestAssertionFailure()`, shutdown after the frame, exit code 5 | `Test_WorldTransition_ReplayDivergenceExit5` |
| replay whose live peak reservation is refused where the recording's succeeded | proceeds unreserved, logged and counted | `Test_WorldTransition_ReplayIntakeFollowsLog` |
| replay of a recorded transition failure | the record ends at the recorded tick through the failure application procedure, isolated world destroyed and reservation released; no divergence | `Test_WorldTransition_ReplayIntakeFollowsLog` |
| script `Save.Load` that fails before or after the hand-off | the requesting world untouched; the world-load ID completed once with the failure code; everything the host took released in the procedure's order; its replay exits 0 | `Test_WorldTransition_FailedSaveLoadLeavesWorld`, `Test_WorldTransition_ReplayIntakeFollowsLog` |
| a producer completing after its world was destroyed (a background job, or a module's own `DetachWorld`) | reaches the world's table through a sink retired with it, discarded and counted; never freed memory | `Test_WorldTransition_RetiredResultsDiscarded` cases (i), (ii) |
| a fifth or subsequent retired result table still waiting on a producer | held in the overflow list, warned and counted; never released early, never a stop | `Test_WorldTransition_RetiredResultsDiscarded` |
| device loss | no GAME work depends on the device; cameras keep writing components | — (covered by RPROD's device-loss tests) |
| budget breach of Z2-B-GAME-003 | `BR-MERGE`: deterministic rejection with a diagnostic | `Test_GameplayRules_EventCapacityRejectKeepsRows` |

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/GAME/Tests.md` → "Test files and targets". The Test gate step runs `ZenithGameplay_Tests`, `ZenithRuntime_Tests` and the Lifecycle suites of the stages reached, headless; the Sanitizers gate step runs the fault sweeps under `win-asan` and the determinism tests under `linux-tsan`; Libgraph checks the target's edges; the OpenWorldSlice gametests run in the `win` presets (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Game test registration"). No GAME test is `needs-gpu` except the Stage 7 XR case `Test_XRGameplay_DirectorDrivesStereoPair`, which needs an OpenXR runtime on the GPU and is skipped (exit code 77) on a host without one.

### Test catalogue index

| Group | Heading |
|---|---|
| framework, abilities, cameras | Tests.md "ZenithGameplay unit tests", "Camera tests" |
| world host | Tests.md "World host tests" |
| lifecycle commands and scenarios | PhaseContract.md "Per-command tests", "Scenario suite"; Tests.md "Lifecycle per-command tests" |
| Luau library | Tests.md "Script library tests" |
| OpenWorldSlice | OpenWorldSlice.md "Sample tests" |

## Budget rows

### Rows owned

| Row | Name | Status | Unit | Procedure | Workloads | Breach | Producer | Specified in |
|---|---|---|---|---|---|---|---|---|
| Z2-B-GAME-001 | Movement arbitration phase CPU | pending | ms | MP-CPUZONE | W-Characters, W-OpenWorld, W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for GAME | PhaseContract.md "Movement arbitration phase CPU" |
| Z2-B-GAME-002 | Event delivery CPU | pending | ms | MP-CPUZONE | WS-FRAME | BR-ACCEPT | same | PhaseContract.md "Event delivery CPU" |
| Z2-B-GAME-003 | Event queue capacity per tick | pending | events | MP-COUNTER | W-Dense, WS-SLICE | BR-MERGE, 0 (hard) | same | PhaseContract.md "Event queue capacity" |

### Rows measured against

| Row | Why |
|---|---|
| Z2-B-CORE-007 World-transition peak | the reservation of every transition |
| Z2-B-ECS-001 Component storage per world | gameplay components are component storage; the event arena check of Z2-B-GAME-003 |
| Z2-B-AI-002 AI and navigation phase CPU | `Gameplay.Rules` and `Gameplay.Cameras` run in phase 7, whose whole span this row measures (W-Characters, W-OpenWorld); GAME's two nodes are budgeted inside it, and the pending "Gameplay framework CPU" row below is reported beside it, never summed into CPU-SIM a second time (cross-area rows GA8 and GA18) |
| Z2-B-SHIP-019, Z2-B-SHIP-020, Z2-B-SHIP-023 | OpenWorldSlice's bug bar, defect cap and playthrough duration |

### Row needs recorded as pending

| Need (no ID until allocated) | Unit | Procedure | Value it fixes | Producer |
|---|---|---|---|---|
| "Gameplay framework CPU": the summed spans of `Gameplay.Rules` and `Gameplay.Cameras` per frame, p95 — a sub-row of Z2-B-AI-002 with breach class BR-REPORT, excluded from every bound because its spans lie inside Z2-B-AI-002's phase-7 span (the pattern of [Z2-N-BUDGET-239]'s extraction row); `Gameplay.WorldEvents` (phase 1, one comparison per tick) is shown in the profiler only | ms | MP-CPUZONE | GAME's share of Z2-B-AI-002 on W-OpenWorld and W-Characters, which the AI ruling must leave | Stage 1 per-system budget ticket for GAME |
| "Gameplay request capacity per tick" | requests | MP-COUNTER (`gameplay.requests.high_water`) | `gameplay.maxRequestsPerTick` (default 16,384) | same |
| "Gameplay framework memory per world" | MB | MP-SUBMEM (the module's memory consumer) | the queue plus world-host records per world | same |

## Stage plan

### Stage table

| Stage | GAME delivers | Gating decisions |
|---|---|---|
| S1 | D-PHASES analysis and ruling; the GAME budget ruling (Z2-B-GAME-001…003 and the three pending rows); the OpenWorldSlice acceptance package definition | D-PHASES (open, Stage 1), Z2-D-084 |
| S2 | the spawn, destroy and shutdown per-command tests (with the probe module); the lifecycle shutdown and teardown order; the Stage 2 refusing definition of `Zenith_WorldHost_BeginTransition`; the Stage 2 Lifecycle suite (`SpawnAndDestroySameTick`, its replay matrix, `Test_Gameplay_AuthorityCensusMatchesRegistry`); census rows of S2 designs | Z2-D-080 |
| S3 | `ZenithGameplay` with tags, events, state machines, timers, actions, players, possession, ownership, abilities, cameras, bindings and the `@gameplay` library; the world host's transitions, carry-over, pause, `Game` namespace; the teleport and transition per-command tests; the Stage 3 suite LifecycleCharacters (the ragdoll per-command test, `TeleportDuringRagdoll`, its replay matrix and census test) | the D-PHASES ruling |
| S4 | attributes and effects; cinematic suspension; the Stage 4 suite LifecycleOpenWorld (the two cinematic per-command tests, `SkipCinematicMidBlend`, `WorldTransitionWithInFlightAsync`, `SaveAtSnapshotDuringStreaming`, its replay matrix and census test); the save-load transition; the four OpenWorldSlice samples and their integration; the S01–S04 recording (`needs-human`) and the first playable acceptance run | Z2-D-077, Z2-D-021, Z2-D-083 |
| S5 | none of GAME's own beyond regressions | — |
| S6 | the S05–S08 playthrough integration, the full recording (`needs-human`), the Stage 6 acceptance run and the sign-off | Z2-D-021, Z2-D-083 |
| S7 | player slots under replication; XR camera output | Z2-D-073, Z2-D-072 |

### API-only slices

The first S3 ticket lands the public headers of § "Header map" with their contract blocks and `static_assert`ed sizes, registered types and an empty rules system, so consumers (EDIT's inspector, OpenWorldSlice authoring, SCRIPT's generated types) build against the final API before the algorithms land. The collision query header lands as an API-only SCENE ticket with PHYS's implementation following.

### Ticket slices and ordering

1. S1: "D-PHASES refinement analysis" → "Rule D-PHASES refinement" (`needs-human`); "Stage 1 flagship acceptance ticket for OpenWorldSlice" (`needs-human`); "Stage 1 per-system budget ticket for GAME" (`needs-human`, no harness half), `blocks`-linked from "Stage 1 per-system budget ticket for CORE", "… for JOBS" and "… for RPROD" ([Z2-N-BUDGET-023]) and from the three tickets whose figures its derivation consumes: "Stage 1 workload scale ticket for W-Dense" and "Stage 1 flagship acceptance ticket for OpenWorldSlice" (Z2-B-GAME-003's E_peak, `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Event queue capacity") and "Stage 1 workload scale ticket for W-Characters" (Z2-B-GAME-001, → "Movement arbitration phase CPU"); `Zenith2/Docs/Budgets.md` → "Producer tickets" gains the three in its `Blocked by` cell through cross-doc item X254 (cross-area row GA8).
2. S2: "GAME world host: lifecycle shutdown and teardown order" — the teardown table with its nine steps (`Internal/Zenith_WorldTeardown.h`, which MOD's `Zenith_WorldHost_DestroyWorld` walks), `Zenith_WorldHost_ShutdownWorlds`, and `Internal/Zenith_WorldTransition.cpp` with the Stage 2 definition of `Zenith_WorldHost_BeginTransition` (no allocation; logs `runtime.world: world transitions are not available before Stage 3`; returns `Zenith_RuntimeError{ RUNTIME_ERROR_TRANSITION_IN_PROGRESS }`), so it is MB16's Stage 2 producer and lands before MOD's composition-root ticket Z2T-MOD-024, which is blocked by it (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD"; GA23) → per-command tests for spawn, destroy, shutdown with the probe module → "Lifecycle scenario suite: Stage 2" (whose `Zenith2/Tests/Lifecycle/CMakeLists.txt` call is the union shared with Z2T-PLAT-044, `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures").
3. S3: API-only headers → registration and queue → rules by primitive (tags, timers, states, actions, possession, abilities; one ticket each) → cameras (rig evaluation, director, collision, blends, cuts) → bindings and `@gameplay` → "GAME world host: transition intake and record" (replacing the Stage 2 body of `Zenith_WorldHost_BeginTransition`: the sink, the direct and sink intake with the peak reservation, the record state machine, isolated-world creation and freeze, the failure application procedure, the publication handler's live rows, `Zenith_RegisterGameFlowTypes`) → "GAME world host: swap and carry-over" (the swap procedure with the trial compile, carry-over, the world state writes, the swap test hook, and the swap case of MOD's `Test_Runtime_ExitAfterTicks_RunsExactlyN`, an extension of MOD's test, not a new case) → "GAME world host: retired results and replay" (retirement, release and overflow, the sinks taken into the retired entries, teardown step `ASYNC_RESULTS`, the replay rows of the handler, intake in a replay, divergence) → "GAME world host: pause and Game namespace" (pause arbitration, the `Runtime.*` command kinds, the `Game` bindings and the "Code ranges" update) → teleport and transition per-command tests → "Lifecycle scenario suite: Stage 3 characters" (the ragdoll per-command test, `TeleportDuringRagdoll`).
4. S4: attributes, effects → cinematic suspension → "Lifecycle scenario suite: Stage 4 open world" → "GAME world host: save-load transition" (`Zenith_WorldHost_BeginTransitionFromSave`, the restore, failed save loads) → "OpenWorldSlice interaction sample", "OpenWorldSlice inventory sample", "OpenWorldSlice quest sample", "OpenWorldSlice dialogue sample" (game-side leaves under `Scripts/`) → "OpenWorldSlice sample integration" → "OpenWorldSlice playthrough recording S01–S04" (`needs-human`) → the Stage 4 acceptance run (`Zenith2Acceptance`).
5. S6: "OpenWorldSlice playthrough integration" → "OpenWorldSlice full playthrough recording" (`needs-human`) → the Stage 6 acceptance run and the sign-off tickets (`needs-human`).

## Decisions

### Settled decisions

| Decision | Bearing on GAME |
|---|---|
| Z2-D-080 | the twelve phases, per-field authority, commands at sync points, default arbitration precedence; GAME owns its D-PHASES refinement |
| Z2-D-077 | quest, dialogue, inventory and interaction are Luau inside OpenWorldSlice |
| Z2-D-021 | OpenWorldSlice is one of three equally binding flagship slices |
| Z2-D-019, Z2-D-053 | determinism; asynchronous results at tick boundaries in completion-ID order |
| Z2-D-002, Z2-D-024, Z2-D-081 | games have no C++; modules are static libraries; one executable per game |
| Z2-D-016, Z2-D-023 | Luau transactions; recovery through serialized state and `OnRecover` |
| Z2-D-045, Z2-D-046, Z2-D-047 | errors, no global state, contract blocks |
| Z2-D-076, Z2-D-073, Z2-D-072 | scope exclusions and deferrals |
| Z2-D-012, Z2-D-084, Z2-D-085, Z2-D-097 | budgets, measurement and ruling tickets, living docs, DecisionLog appends |
| Z2-D-083 | stage acceptance suites run outside the loop: the OpenWorldSlice acceptance runs |

### Open decisions

D-PHASES (open, Stage 1), owned by GAME: `Zenith2/Docs/Design/GAME/PhaseContract.md` → "The D-PHASES ruling" gives its analysis ticket, ruling ticket, candidates R1–R13, acceptance rule and consumers.

### Decisions GAME owns

The D-PHASES ruling is GAME's only decision. The design choices this family makes without a decision entry are reversible within GAME's code: the per-row rules sweep over a due-tick queue (Framework.md "Rules algorithm"), gameplay ticks over rebasing (Framework.md "Gameplay time"), camera output by commands in phase 7 (Cameras.md "Output and cuts"), and one transition per process (WorldHost.md "Transition record state machine").

## Requirement index

Paths are relative to `Zenith2/Docs/Design/`.

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-GAME-001 | ZenithGameplay library declaration | GAME.md → "Target summary"; GAME.md → "Libraries GAME code depends on" |
| Z2-R-GAME-002 | Module entry points | GAME.md → "Zenith_Module_Gameplay header"; GAME/Framework.md → "Component and resource registration" |
| Z2-R-GAME-003 | Start and attach unwind on failure | GAME.md → "Construction and shutdown"; GAME.md → "Failure table" |
| Z2-R-GAME-004 | No game rule names in the engine | GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/Tests.md → "ZenithGameplay unit tests" |
| Z2-R-GAME-005 | Missing engine types fail the attach | GAME/Cameras.md → "Declared access"; GAME/Framework.md → "Gameplay limits and enumerations" |
| Z2-R-GAME-006 | Gameplay memory consumer | GAME/Framework.md → "Gameplay memory accounting" |
| Z2-R-GAME-007 | D-PHASES analysis files | GAME/PhaseContract.md → "Analysis ticket"; GAME/PhaseContract.md → "Census format"; GAME/PhaseContract.md → "Refinement candidates" |
| Z2-R-GAME-008 | D-PHASES ruling recorded | GAME/PhaseContract.md → "Ruling ticket"; GAME/PhaseContract.md → "Change control" |
| Z2-R-GAME-009 | Every engine component property has an owning phase | GAME/PhaseContract.md → "Census of signed-off designs"; GAME/PhaseContract.md → "Census format"; GAME/Tests.md → "Lifecycle per-command tests" |
| Z2-R-GAME-010 | Contested fields carry an arbitration rule | GAME/PhaseContract.md → "Census of signed-off designs"; GAME/PhaseContract.md → "Refinement candidates" |
| Z2-R-GAME-011 | Census extended by every subsequent design | GAME/PhaseContract.md → "Census rows added by subsequent designs" |
| Z2-R-GAME-012 | GAME systems placed by the contract | GAME/PhaseContract.md → "Phase placement of GAME systems"; GAME/Framework.md → "Declared access"; GAME/Cameras.md → "Declared access" |
| Z2-R-GAME-013 | Event queue capacity row derivation | GAME/PhaseContract.md → "Event queue capacity"; GAME.md → "Rows owned" |
| Z2-R-GAME-014 | Phase CPU rows allocated | GAME/PhaseContract.md → "Movement arbitration phase CPU"; GAME/PhaseContract.md → "Event delivery CPU" |
| Z2-R-GAME-015 | Pending GAME row needs filed | GAME.md → "Row needs recorded as pending" |
| Z2-R-GAME-016 | Spawn per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-017 | Destroy per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-018 | Teleport per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-019 | Ragdoll enter per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-020 | Ragdoll recover per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-021 | Cinematic start per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-022 | Cinematic interrupt per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-023 | Cinematic skip restore per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-024 | World transition per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-025 | Shutdown per-command test | GAME/PhaseContract.md → "Lifecycle command contract matrix"; GAME/PhaseContract.md → "Per-command tests" |
| Z2-R-GAME-026 | Scenario teleport during ragdoll | GAME/PhaseContract.md → "Scenario suite"; GAME/PhaseContract.md → "Suite target and fixtures"; GAME/PhaseContract.md → "Scenario TeleportDuringRagdoll" |
| Z2-R-GAME-027 | Scenario skip cinematic mid-blend | GAME/PhaseContract.md → "Scenario suite"; GAME/PhaseContract.md → "Suite target and fixtures"; GAME/PhaseContract.md → "Scenario SkipCinematicMidBlend" |
| Z2-R-GAME-028 | Scenario spawn and destroy in one tick | GAME/PhaseContract.md → "Scenario suite"; GAME/PhaseContract.md → "Suite target and fixtures"; GAME/PhaseContract.md → "Scenario SpawnAndDestroySameTick" |
| Z2-R-GAME-029 | Scenario transition with in-flight results | GAME/PhaseContract.md → "Scenario suite"; GAME/PhaseContract.md → "Suite target and fixtures"; GAME/PhaseContract.md → "Scenario WorldTransitionWithInFlightAsync" |
| Z2-R-GAME-030 | Scenario save during streaming | GAME/PhaseContract.md → "Scenario suite"; GAME/PhaseContract.md → "Suite target and fixtures"; GAME/PhaseContract.md → "Scenario SaveAtSnapshotDuringStreaming" |
| Z2-R-GAME-031 | Scenario replay matrix | GAME/PhaseContract.md → "Replay matrix" |
| Z2-R-GAME-032 | World lifecycle state machine | GAME/WorldHost.md → "World lifecycle state machine" |
| Z2-R-GAME-033 | Game flow state machine | GAME/WorldHost.md → "Game flow state machine" |
| Z2-R-GAME-034 | Isolated world creation | GAME/WorldHost.md → "World creation" |
| Z2-R-GAME-035 | Deterministic world seed derivation | GAME/WorldHost.md → "World creation"; GAME/WorldHost.md → "Replay of a transition" |
| Z2-R-GAME-036 | World state resource constant within a world | GAME/WorldHost.md → "World state resource" |
| Z2-R-GAME-037 | World started event | GAME/Framework.md → "The world events system" |
| Z2-R-GAME-038 | Transition intake refusals | GAME/WorldHost.md → "Transition request intake" |
| Z2-R-GAME-039 | Peak reservation before loading | GAME/WorldHost.md → "Transition record state machine" |
| Z2-R-GAME-040 | Script transition requests through the sink | GAME/WorldHost.md → "Transition request intake" |
| Z2-R-GAME-041 | Transition record state machine | GAME/WorldHost.md → "Transition record state machine" |
| Z2-R-GAME-042 | Load completion published at a tick boundary | GAME/WorldHost.md → "Publication payloads" |
| Z2-R-GAME-043 | Swap in prologue step 7 | GAME/WorldHost.md → "Prologue step 7 procedure"; GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-044 | Swap is all or nothing | GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-045 | Transition cancellation | GAME/WorldHost.md → "Transition failure and cancellation" |
| Z2-R-GAME-046 | Paused old world defers its swap | GAME/WorldHost.md → "Pause arbitration"; GAME/WorldHost.md → "Transition record state machine" |
| Z2-R-GAME-047 | Transition replay | GAME/WorldHost.md → "Replay of a transition" |
| Z2-R-GAME-048 | Replay records of a swap | GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-049 | Carry-over root list | GAME/WorldHost.md → "Carry-over" |
| Z2-R-GAME-050 | Carry-over values preserved | GAME/WorldHost.md → "Carry-over" |
| Z2-R-GAME-051 | Gameplay time continues across a swap | GAME/Framework.md → "Gameplay time"; GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-052 | Results of a swapped-out world discarded | GAME/WorldHost.md → "Retired world asynchronous results" |
| Z2-R-GAME-053 | Transition from a save | GAME/WorldHost.md → "Transition sources"; GAME/WorldHost.md → "Transition record state machine" |
| Z2-R-GAME-054 | Transition fault sweep | GAME/WorldHost.md → "Transition failure and cancellation" |
| Z2-R-GAME-055 | Pause reasons and application | GAME/WorldHost.md → "Pause arbitration" |
| Z2-R-GAME-056 | Gameplay pause deadline | GAME/WorldHost.md → "Pause arbitration" |
| Z2-R-GAME-057 | Pause does not change results | GAME/WorldHost.md → "Pause arbitration" |
| Z2-R-GAME-058 | Quit from a script | GAME/WorldHost.md → "Game command kinds"; GAME/WorldHost.md → "Shutdown request" |
| Z2-R-GAME-059 | Script test failure report | GAME/WorldHost.md → "Game command kinds"; GAME/WorldHost.md → "Game script bindings" |
| Z2-R-GAME-060 | Game namespace misuse | GAME/WorldHost.md → "Game script bindings" |
| Z2-R-GAME-061 | Lifecycle shutdown of each world | GAME/WorldHost.md → "World lifecycle shutdown" |
| Z2-R-GAME-062 | Teardown order | GAME/WorldHost.md → "Teardown order algorithm" |
| Z2-R-GAME-063 | Camera components registered | GAME/Cameras.md → "Zenith_GameplayCameraRigComponent"; GAME/Cameras.md → "Zenith_GameplayCameraDirectorComponent" |
| Z2-R-GAME-064 | Rig selection | GAME/Cameras.md → "Rig selection" |
| Z2-R-GAME-065 | Camera follows the possessed pawn | GAME/Cameras.md → "Rig selection"; GAME/Framework.md → "Possession algorithm"; GAME/Framework.md → "Rules algorithm" |
| Z2-R-GAME-066 | Same-tick target pose | GAME/Cameras.md → "Target pose evaluation" |
| Z2-R-GAME-067 | Rig kinds | GAME/Cameras.md → "Rig kind evaluation" |
| Z2-R-GAME-068 | Look input integration | GAME/Cameras.md → "Look input integration" |
| Z2-R-GAME-069 | Camera smoothing | GAME/Cameras.md → "Smoothing" |
| Z2-R-GAME-070 | Camera collision | GAME/Cameras.md → "Collision resolution" |
| Z2-R-GAME-071 | Camera without collision provider | GAME/Cameras.md → "Collision query interface"; GAME/Cameras.md → "Collision resolution" |
| Z2-R-GAME-072 | Collision query interface | GAME/Cameras.md → "Collision query interface" |
| Z2-R-GAME-073 | Camera blending | GAME/Cameras.md → "Blending" |
| Z2-R-GAME-074 | Camera cut on teleport | GAME/Cameras.md → "Output and cuts" |
| Z2-R-GAME-075 | Scripted camera cut | GAME/Cameras.md → "Output and cuts"; GAME/Cameras.md → "Camera bindings" |
| Z2-R-GAME-076 | Camera output visible the same tick | GAME/Cameras.md → "Output and cuts" |
| Z2-R-GAME-077 | Cinematic camera suspension | GAME/Cameras.md → "Cinematic suspension" |
| Z2-R-GAME-078 | Camera debug view | GAME/Cameras.md → "Camera debug view"; GAME.md → "Debug views and console commands" |
| Z2-R-GAME-079 | Tag text and hierarchical matching | GAME/Framework.md → "Tag component"; GAME/Framework.md → "Tag matching" |
| Z2-R-GAME-080 | Tag reference counting | GAME/Framework.md → "Tag reference counting" |
| Z2-R-GAME-081 | Tag capacity | GAME/Framework.md → "Tag reference counting" |
| Z2-R-GAME-082 | Gameplay event types | GAME/Framework.md → "Event types" |
| Z2-R-GAME-083 | Event order is merge order | GAME/Framework.md → "Event types"; GAME/Framework.md → "Rules algorithm" |
| Z2-R-GAME-084 | Event capacity rejection keeps row writes | GAME/Framework.md → "Rules algorithm" |
| Z2-R-GAME-085 | Generic message event | GAME/Framework.md → "The message event"; GAME/Scripting.md → "Message helpers" |
| Z2-R-GAME-086 | Request command kinds | GAME/Framework.md → "Command kinds and payloads" |
| Z2-R-GAME-087 | Request queue bounds | GAME/Framework.md → "Queue record format"; GAME/Framework.md → "Playback into the queue" |
| Z2-R-GAME-088 | Request drain order | GAME/Framework.md → "Playback into the queue" |
| Z2-R-GAME-089 | Malformed requests never assert | GAME/Framework.md → "Playback into the queue"; GAME/Framework.md → "Rules algorithm" |
| Z2-R-GAME-090 | Rules algorithm order | GAME/Framework.md → "Rules algorithm"; GAME/Framework.md → "Row ownership in the rules node" |
| Z2-R-GAME-091 | Gameplay time | GAME/Framework.md → "Gameplay time" |
| Z2-R-GAME-092 | Gameplay determinism across backends | GAME/Framework.md → "Rules algorithm" |
| Z2-R-GAME-093 | Gameplay replay | GAME/Framework.md → "Rules algorithm" |
| Z2-R-GAME-094 | State machine primitive | GAME/Framework.md → "State machines" |
| Z2-R-GAME-095 | State machine Luau helper | GAME/Scripting.md → "State machine helper" |
| Z2-R-GAME-096 | Timer primitive | GAME/Framework.md → "Timers" |
| Z2-R-GAME-097 | Timer fire clamp | GAME/Framework.md → "Timers" |
| Z2-R-GAME-098 | Action kinds | GAME/Framework.md → "Action evaluation" |
| Z2-R-GAME-099 | Action names and retention | GAME/Framework.md → "Action component"; GAME/Framework.md → "Action retention" |
| Z2-R-GAME-100 | Async await helper | GAME/Scripting.md → "Async helpers" |
| Z2-R-GAME-101 | Player and pawn components | GAME/Framework.md → "Player and pawn components" |
| Z2-R-GAME-102 | Possession algorithm | GAME/Framework.md → "Possession algorithm" |
| Z2-R-GAME-103 | Auto-possession | GAME/Framework.md → "Possession algorithm" |
| Z2-R-GAME-104 | Destroyed pawn unpossessed | GAME/Framework.md → "Possession algorithm" |
| Z2-R-GAME-105 | Owning player resolution | GAME/Framework.md → "Ownership"; GAME/Scripting.md → "Read helpers" |
| Z2-R-GAME-106 | Ability activation | GAME/Abilities.md → "Ability activation algorithm" |
| Z2-R-GAME-107 | Ability cooldown and expiry | GAME/Abilities.md → "Ability end and expiry" |
| Z2-R-GAME-108 | Ability spec validation | GAME/Abilities.md → "Ability component"; GAME/Scripting.md → "Effect and ability spec builders" |
| Z2-R-GAME-109 | Attribute definition and requests | GAME/Abilities.md → "Attribute component"; GAME/Abilities.md → "Attribute requests" |
| Z2-R-GAME-110 | Attribute evaluation | GAME/Abilities.md → "Current value evaluation" |
| Z2-R-GAME-111 | Effect application and stacking | GAME/Abilities.md → "Effect application algorithm"; GAME/Abilities.md → "Effect stacking" |
| Z2-R-GAME-112 | Effect expiry and periods | GAME/Abilities.md → "Effect expiry and periods" |
| Z2-R-GAME-113 | Effect spec validation | GAME/Abilities.md → "Effect specification"; GAME/Scripting.md → "Effect and ability spec builders" |
| Z2-R-GAME-114 | Gameplay binding namespace | GAME/Scripting.md → "Binding registration"; GAME/Scripting.md → "Binding functions" |
| Z2-R-GAME-115 | Gameplay binding misuse codes | GAME/Scripting.md → "Misuse errors" |
| Z2-R-GAME-116 | Gameplay Luau library | GAME/Scripting.md → "Library layout" |
| Z2-R-GAME-117 | Read helpers | GAME/Scripting.md → "Read helpers" |
| Z2-R-GAME-118 | Camera Luau helpers | GAME/Cameras.md → "Camera bindings" |
| Z2-R-GAME-119 | Interaction sample | GAME/OpenWorldSlice.md → "Interaction"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/OpenWorldSlice.md → "Sample tests" |
| Z2-R-GAME-120 | Inventory sample | GAME/OpenWorldSlice.md → "Inventory"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/OpenWorldSlice.md → "Sample tests" |
| Z2-R-GAME-121 | Quest sample | GAME/OpenWorldSlice.md → "Quest"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/OpenWorldSlice.md → "Sample tests" |
| Z2-R-GAME-122 | Dialogue sample | GAME/OpenWorldSlice.md → "Dialogue"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/OpenWorldSlice.md → "Sample tests" |
| Z2-R-GAME-123 | Sample data tables | GAME/OpenWorldSlice.md → "Sample data tables" |
| Z2-R-GAME-124 | Samples survive save and transition | GAME/OpenWorldSlice.md → "Sample tests"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule" |
| Z2-R-GAME-125 | Samples use only framework APIs | GAME/OpenWorldSlice.md → "Gameplay sample rules"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule" |
| Z2-R-GAME-126 | Sample integration ticket | GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/OpenWorldSlice.md → "Sample tests" |
| Z2-R-GAME-127 | OpenWorldSlice acceptance package defined | GAME/OpenWorldSlice.md → "Acceptance package" |
| Z2-R-GAME-128 | Playthrough replay as text | GAME/OpenWorldSlice.md → "Acceptance package"; GAME/OpenWorldSlice.md → "Slice layout and ticket rule" |
| Z2-R-GAME-129 | First playable playthrough | GAME/OpenWorldSlice.md → "Acceptance package" |
| Z2-R-GAME-130 | Full playthrough | GAME/OpenWorldSlice.md → "Acceptance package" |
| Z2-R-GAME-131 | Playthrough smoke in CI | GAME/OpenWorldSlice.md → "Acceptance package" |
| Z2-R-GAME-132 | Feature checklist coverage | GAME/OpenWorldSlice.md → "Acceptance package" |
| Z2-R-GAME-133 | Gameplay counters | GAME.md → "Counters" |
| Z2-R-GAME-134 | World host counters and logs | GAME/WorldHost.md → "World host counters and logs" |
| Z2-R-GAME-135 | Configuration keys | GAME.md → "Configuration keys" |
| Z2-R-GAME-136 | Failure table behaviour | GAME.md → "Failure table" |
| Z2-R-GAME-137 | Player slots and ownership under replication | GAME/Framework.md → "Player and pawn components"; GAME.md → "Explicit exclusions" |
| Z2-R-GAME-138 | XR-ready camera output | GAME/Cameras.md → "Camera model"; GAME.md → "Explicit exclusions" |
| Z2-R-GAME-139 | New world schedule validated before the swap | GAME/WorldHost.md → "World creation"; GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-140 | Replay intake follows the log | GAME/WorldHost.md → "Intake in a replay"; GAME/WorldHost.md → "Replay of a transition" |
| Z2-R-GAME-141 | Rules node row ownership | GAME/Framework.md → "Row ownership in the rules node"; GAME/Cameras.md → "Declared access"; GAME/Cameras.md → "Director algorithm" |
| Z2-R-GAME-142 | Changed attribute rows re-evaluated | GAME/Framework.md → "Rules algorithm"; GAME/Abilities.md → "Attribute component" |
| Z2-R-GAME-143 | Continuity tick base saved with the world | GAME/WorldHost.md → "World state resource"; GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-144 | Camera follow target from the rules pass | GAME/Framework.md → "Rules algorithm"; GAME/Cameras.md → "Rig selection" |
| Z2-R-GAME-145 | Cinematic interrupt blends out | GAME/PhaseContract.md → "Per-command tests"; GAME/Cameras.md → "Cinematic suspension" |
| Z2-R-GAME-146 | World-load publications keyed by world and completion ID | GAME/WorldHost.md → "Publication payloads"; GAME/WorldHost.md → "World creation" |
| Z2-R-GAME-147 | One failure procedure releases a transition in order | GAME/WorldHost.md → "Failure application procedure"; GAME/WorldHost.md → "Transition record state machine" |
| Z2-R-GAME-148 | Recorded failures and in-flight recordings replay without divergence | GAME/WorldHost.md → "Intake in a replay"; GAME/WorldHost.md → "Replay of a transition"; GAME/WorldHost.md → "Prologue step 7 procedure" |
| Z2-R-GAME-149 | A failed save load leaves the requesting world unchanged | GAME/WorldHost.md → "Failed save loads"; GAME/WorldHost.md → "Failure application procedure" |
| Z2-R-GAME-150 | Retired tables are never released early and never stop the run | GAME/WorldHost.md → "Retired world asynchronous results" |
| Z2-R-GAME-151 | Census comparison is stage-aware and excludes test types | GAME/PhaseContract.md → "Census format"; GAME/Tests.md → "Lifecycle per-command tests" |
| Z2-R-GAME-152 | Same-drain attribute reads use the evaluated value | GAME/Abilities.md → "Ability activation algorithm"; GAME/Framework.md → "Action evaluation" |
| Z2-R-GAME-153 | One selecting director per rig per tick | GAME/Cameras.md → "Rig selection"; GAME/Cameras.md → "Director algorithm" |
| Z2-R-GAME-154 | Playthrough integration gametests | GAME/OpenWorldSlice.md → "Slice layout and ticket rule"; GAME/OpenWorldSlice.md → "Sample tests" |
| Z2-R-GAME-155 | Transition entry point defined from Stage 2 | GAME/WorldHost.md → "World host files and split with MOD"; GAME.md → "Ticket slices and ordering" |
| Z2-R-GAME-156 | Primary-world ticks counted across a swap | GAME/Tests.md → "World host tests"; GAME/WorldHost.md → "Swap procedure" |
| Z2-R-GAME-157 | A save restore that never becomes ready fails | GAME/WorldHost.md → "Transition record state machine"; GAME/WorldHost.md → "Prologue step 7 procedure" |
| Z2-R-GAME-158 | Single step runs exactly n ticks | GAME/WorldHost.md → "Single step" |
| Z2-R-GAME-159 | No gameplay framework in edit and preview worlds | GAME.md → "Zenith_Module_Gameplay header" |
| Z2-R-GAME-160 | OpenWorldSlice menus, saving and accessible-flow route | GAME/OpenWorldSlice.md → "Menus, save and accessible-flow route" |
| Z2-R-GAME-161 | OpenWorldSlice online content | GAME/OpenWorldSlice.md → "Online content" |
| Z2-R-GAME-162 | No carry-over across a save profile change | GAME/WorldHost.md → "Carry-over" |
| Z2-R-GAME-163 | Voice locale as a game simulation input | GAME/OpenWorldSlice.md → "Voice locale and character parts" |
| Z2-R-GAME-164 | Time of day and weather in the slice | GAME/OpenWorldSlice.md → "Time of day and weather" |

## Reconciliation items

| # | Item | Proposed resolution | Routed to |
|---|---|---|---|
| G1 | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Scenario tests" places every per-command test in `Engine/ZenithRuntime/Tests/`, but the ragdoll and cinematic tests need modules above the runtime | the two run in `Zenith2/Tests/Lifecycle/` under unchanged names in the stage suites (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix"); the suite becomes three stage suites; ECS's conformance counterparts keep distinct names | cross-doc item X224 (GA7 (a)), applied |
| G2 | [Z2-N-ARCH-560] lists transforms before the module phases in the teardown order, but the scene world state holds the dispatcher modules' listeners are registered with | the scene world state is torn down after the modules (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Teardown order algorithm"); the bullet gains "the scene world state follows the modules' detach", and the shutdown point becomes "when the current frame-loop iteration returns" | cross-doc item X223 (GA7 (b)), applied; D-PHASES candidate R12 |
| G3 | `Zenith_ModuleStartContext::RegisterCommandKind` returns `void`, so a module cannot learn its kind IDs | `Zenith_WorldAttachContext::FindCommandKind` (GA5 (c)) | MOD |
| G4 | a module cannot pause its own world | `Zenith_WorldAttachContext::GetWorldControl` (GA5 (a)) | MOD |

## Cross-area notes

Rows GA1–GA23 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` record every need GAME has of ECS, SCENE, PHYS, CINE, MOD, PLAT, ARCH, BUDGET, RPROD, SCRIPT, SAVE, STREAM, UI, DEVT, ANIM and AI, each with the assumption fence that depends on it.
