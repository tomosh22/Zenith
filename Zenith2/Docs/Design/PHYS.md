# PHYS design: physics

The design of the Physics area (PHYS): the backend-neutral physics API `Zenith_Physics` with its two backends (Jolt and PhysX), the conformance suite that holds them to one contract, per-backend determinism, collision cooking, layers and filtering, queries, triggers and contact events, the character controller, constraints and joints, ragdolls, vehicles, buoyancy, physical materials and interactions, moving platforms, streaming coordination, large coordinates, debug visualisation, and the Physics module (`ZenithPhysics`) that runs all of it inside the frame phases. This file is the entry document; the split files carry the detail.

## Purpose and scope

### What PHYS owns

| Capability | Where |
|---|---|
| the neutral backend interface `Zenith_Physics` / `Zenith_PhysicsWorld`, its value types, handles, errors and limits | `Zenith2/Docs/Design/PHYS/PublicAPI.md` → "Zenith_Physics header", `Zenith2/Docs/Design/PHYS/PublicAPI.md` → "Zenith_PhysicsWorld header" |
| the Jolt and PhysX backend libraries, their pins, build options, construction, stepping without blocking waits, and state save | `Zenith2/Docs/Design/PHYS/Backends.md` |
| the components, events, commands, world resource, script bindings, configuration keys and authority census rows of the Physics module | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` |
| the simulation inside the phases: ingest, the substep loop with its cap, write-back, transform sync, events, triggers, interactions, joints, CCD, sleeping, kinematics and platforms, region freeze, cell bodies, origin shift, buoyancy, queries, deferred queries, determinism, save and restore | `Zenith2/Docs/Design/PHYS/Simulation.md` |
| the character controller, ragdolls and vehicles | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` |
| the cooked kinds `Physics.CollisionShape` (code 16), `Physics.Material` and `Physics.Ragdoll`, their documents, importer settings and cook intermediates, deferred query payloads and the save section | `Zenith2/Docs/Design/PHYS/Formats.md` |
| the `ZenithScene` interfaces `Zenith_CollisionQuery` (GAME's camera) and `Zenith_FluidQuery` (ENV's water) that the module implements and consumes | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "ZenithScene interfaces the Physics module implements" |
| the physics debug views | "Debug views" |
| the D-SEL-PHYSX-ANDROID (open, Stage 1) harness and the D-CLASS-SIM (open, Stage 2) classification ruling | `Zenith2/Docs/Design/PHYS/Backends.md` → "PhysX on Android evaluation"; "Open decisions" |
| the PHYS budget rows and their measurement | "Budget rows" |

### Explicit exclusions

| Not PHYS's | Owner | Basis |
|---|---|---|
| 2D physics (`Zenith2D`, D-SEL-2DPHYS (open, Stage 2)) | TWOD | Z2-I-TWOD-06; PHYS's Jolt backend is a candidate only through TWOD's ruling |
| root-motion extraction and publication; applying root motion to entities without a controller | ANIM | `Zenith2/Docs/Design/ANIM/Graph.md` → "Root motion publication" (the orchestrator's Q72 ruling) |
| the animation side of ragdolls: pose ownership, blend of the animated pose, get-up clip and pose matching | ANIM | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Ragdoll transitions" |
| camera collision policy (the camera's use of a sweep) | GAME | `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface" |
| water surfaces, waves and their sampling | ENV | ENV's water resource; PHYS reads it through `Zenith_FluidQuery` |
| cell streaming, region bounds and origin rebasing decisions | STREAM | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Physics coordination" |
| navigation meshes and agent avoidance | AI | AI reads PHYS's queries only |
| cloth, soft bodies and destruction as runtime features | the owner D-CLASS-SIM names | classification at Stage 2 (Z2-I-PHYS-17, -18, -19); implementation per the ruling (Z2-I-PHYS-20) |
| GPU physics | none | both backends are built CPU-only (`PX_SUPPORT_GPU_PHYSX=0`) |

### Inventory items covered

| Item | Text | Stage | Where |
|---|---|---|---|
| Z2-I-PHYS-01 | backend-neutral physics API | S3 | `Zenith2/Docs/Design/PHYS/PublicAPI.md` |
| Z2-I-PHYS-02 | Jolt backend | S3 | `Zenith2/Docs/Design/PHYS/Backends.md` |
| Z2-I-PHYS-03 | PhysX backend | S3 (Android per D-SEL-PHYSX-ANDROID, S1) | `Zenith2/Docs/Design/PHYS/Backends.md` |
| Z2-I-PHYS-04 | backend conformance suite | S3 | `Zenith2/Docs/Design/PHYS/Tests.md` → "Conformance suite" |
| Z2-I-PHYS-05 | per-backend determinism | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Determinism rules" |
| Z2-I-PHYS-06 | collision cooking | S3 | `Zenith2/Docs/Design/PHYS/Formats.md` → "Collision shape file" |
| Z2-I-PHYS-07 | layers and filtering | S3 | "Layer configuration" |
| Z2-I-PHYS-08 | queries | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Query execution" |
| Z2-I-PHYS-09 | triggers | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Trigger semantics" |
| Z2-I-PHYS-10 | character controller | S3 | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Character move algorithm" |
| Z2-I-PHYS-11 | constraints | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Constraint kinds" |
| Z2-I-PHYS-12 | joints | S3 | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Joint properties" |
| Z2-I-PHYS-13 | vehicles | S4 | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Vehicle algorithm" |
| Z2-I-PHYS-14 | ragdolls | S3 | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll state machine" |
| Z2-I-PHYS-15 | debug visualisation | S3 | "Debug views" |
| Z2-I-PHYS-16 | physics streaming | S4 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Cell bodies", "Region freeze" |
| Z2-I-PHYS-17 | cloth classification | S2 | "Open decisions" |
| Z2-I-PHYS-18 | soft-body classification | S2 | "Open decisions" |
| Z2-I-PHYS-19 | destruction classification | S2 | "Open decisions" |
| Z2-I-PHYS-20 | specialist simulations | S5 | "Stage table" |
| Z2-I-PHYS-21 | continuous collision detection | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Continuous collision detection" |
| Z2-I-PHYS-22 | substepping | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Substep loop" |
| Z2-I-PHYS-23 | contact ordering | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Event emission" |
| Z2-I-PHYS-24 | event ordering | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Event emission" |
| Z2-I-PHYS-25 | physical materials | S3 | `Zenith2/Docs/Design/PHYS/Formats.md` → "Material file" |
| Z2-I-PHYS-26 | physical interactions | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Interaction detection" |
| Z2-I-PHYS-27 | moving platforms | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Kinematic bodies and moving platforms" |
| Z2-I-PHYS-28 | large-coordinate handling | S4 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Large coordinates" |
| Z2-I-PHYS-29 | determinism boundaries | S3 | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Determinism boundaries" |

### Cross-area items PHYS implements

Every consumer row addressed to PHYS in `findings/A3_cross_area_notes.md` and the A5 blocker rows naming PHYS, answered by the names those rows use:

| Row | Need | PHYS heading or API |
|---|---|---|
| GA3 (a) | `Zenith_CollisionQuery` per world, `AddWorldResource(Zenith.CollisionQuery, PHASE_PHYSICS, …)`, `WriteResource` on the step node, ignore-root subtree, lowest simulation ID on ties | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CollisionQuery"; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Query execution"; `Test_PhysicsQuery_ClosestHitTieBreak` |
| GA3 (b) | ragdoll enter/recover and controller move are PHYS's | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll enter procedure", "Ragdoll recover procedure", "Character move algorithm" |
| GA3 (c) | events `Zenith.Physics.TriggerEntered { Trigger, Other }` and `TriggerExited`, delivered in phase 9 | confirmed as named: `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsEvents header" |
| GA15, Q72 | the controller node applies `Zenith.Animation.RootMotion`; GAME registers no phase-4 node | `Physics.CharacterMove`; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Frame-graph placement" |
| AN6 | `Zenith.Physics.Ragdoll` (State, BlendWeight, phase 6), `Physics.RagdollBlend`, `Zenith_RagdollPoseSource`, body poses after the step, `RayCast` from phase-3 jobs, root motion only on controller rows, recover placement; (d) no ANIM reader of the controller in the census; (e) the recover weight held for one tick (the PHYS-RECOVER ruling); (f) the 0 s recover, the census readers `Animation.Significance` and `Animation.PostPhysicsBlend` of Ragdoll 1–2, `Physics.Step`'s `ReadResource` of `Zenith.Animation.Residency`, the enter playback's reads through the pose source, no entry snapshot | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_Ragdoll header", "Zenith_PhysicsWorldResource header", "Authority census rows"; `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll blend and root", "Ragdoll state machine" (closed forms, pinned binary32 expressions), "Ragdoll enter procedure"; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Frame-graph placement"; names kept exactly as AN6 adopted them |
| AN2 | the Animation → Physics module edge | accepted; ZenithPhysics declares nothing back ("Libraries PHYS code depends on") |
| EN4 | heightfield cook input from `Zenith_TerrainCollisionTile`, static collider referencing a shape asset, compound from `Zenith_FoliageCollisionSet`, the material name table, buoyancy reading water | `Zenith2/Docs/Design/PHYS/Formats.md` → "Heightfield intermediate", "Compound intermediate", "Physical-material names" (PHYS conforms to ENV's string names: the table is the material assets at `content:/Physics/Materials/<N>.physmat.json`, resolved by PHYS's cook steps, with the 32-bit ID `Zenith_MakePhysicsMaterialNameID`; rows PH9 (b) as revised, PH25, PH26); buoyancy reads `Zenith_FluidQuery`, not `Zenith_WaterWorldResource::Sample` (row PH8: no module-to-module edge) |
| ST8 | cell content colliders and triggers, region freeze, origin shift | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Cell bodies", "Region freeze" (frozen bodies also collide with nothing), "Origin shift" (applied at the sync point that plays SCENE's shift, with STREAM's keys; row PH20) |
| SV17 | one `Zenith_SaveParticipant` per world, `ZenithSave` in `DEPENDS`, `GetCaptureFidelity` | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Save and restore algorithm": SAVE's default `GetCaptureFidelity` over `uSAVE_PARTICIPANT_FLAG_COHERENT` (PhysX), `GetReissuedSourceKinds` for the physics query source, `IsRestoreReady` for resident shapes; rows PH1, PH22 |
| S1, S2 | transform sync in phase 6 with `WRITER_SOURCE_PHYSICS`, ragdoll with `WRITER_SOURCE_RAGDOLL` | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Physics-to-transform sync" |
| AS26 | the runtime object type of kind 16 | `Zenith_CollisionShapeAsset` (`Zenith2/Docs/Design/PHYS/PublicAPI.md` → "Zenith_CollisionShapeAsset header") |
| SCRIPT async source `ASYNC_SOURCE_PHYSICS_QUERY` | the results of the source kind the registry assigns (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry") | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred queries"; row PH13 |
| GA19, X255, X265 | every completion ID completed exactly once; INBOX queries completed at `DetachWorld`; no PHYS count of `world.async_discarded` | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred query state machine" (X265's replacement rows adopted) |
| EnB9 | the `ZenithPhysics` target, the heightfield and compound cook inputs, the collision product kind | "Target summary"; `Zenith2/Docs/Design/PHYS/Formats.md` → "Heightfield intermediate"; "Ticket slices and ordering" |
| CB8 | the subsystem's shedding row | PHYS sheds nothing: its first memory-consumer ticket appends a "none" row asserting unchanged resident bytes at WARNING and CRITICAL ("Failure handling") |
| MB17 | `Zenith_Module_Physics.h`, `ZenithPhysics_Jolt`, `ZenithPhysics_PhysX` | "Module start and world attach", "Ticket slices and ordering" |
| MB18 | the D-SEL-PHYSX-ANDROID ruling producer | the Stage 1 harness ticket ("Stage table") |

### Document family

| File | Content | Headings |
|---|---|---|
| `Zenith2/Docs/Design/PHYS.md` | this entry: scope, dependencies, layout, cross-cutting tables, threading, diagnostics, authoring, failure, tests, budgets, stages, decisions | the design skeleton |
| `Zenith2/Docs/Design/PHYS/PublicAPI.md` | the neutral backend API headers | one per header |
| `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` | the module's ECS-facing headers, ZenithScene interfaces, bindings, configuration, census | one per header |
| `Zenith2/Docs/Design/PHYS/Simulation.md` | the simulation algorithms and state machines | one per algorithm |
| `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` | controller, ragdoll, vehicle | one per procedure |
| `Zenith2/Docs/Design/PHYS/Formats.md` | cooked, authoring, intermediate, payload and save formats | one per format |
| `Zenith2/Docs/Design/PHYS/Backends.md` | Jolt and PhysX | one per backend topic |
| `Zenith2/Docs/Design/PHYS/Tests.md` | the test catalogue | one per test group |
| `Zenith2/Docs/Requirements/PHYS.md` | the requirement catalogue | one per requirement |

### Conventions

- Units are SI (metres, kilograms, seconds, radians, newtons); angles in authoring documents are degrees and convert at cook ([Z2-N-CONTRACT-067] for persistent IDs in JSON).
- "Simulation ID" is the entity's ECS simulation ID; every PHYS ordering that is not by key is ascending simulation ID.
- `<!-- assumption: pending AREA -->` marks an interface another area's design must still confirm; each has a row in "Reconciliation items".

## Dependencies

### Libraries PHYS code depends on

| Target | Declared `DEPENDS` | Notes |
|---|---|---|
| `ZenithPhysics` (`KIND MODULE`, `MODULE_NAME Physics`, layer 7) | ZenithRuntime, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithCore, ZenithProfiling, **ZenithAssets**, **ZenithSave**, **ZenithPlatform** | the three bold edges are missing from `Zenith2/Docs/Architecture.md` → "Library dependency graph" (row PH1): assets for the kind loaders and `Zenith_AssetHandle`, save for the participant (SV17), platform for `Zenith_Config` reads at start |
| `ZenithPhysics_Jolt` (`KIND BACKEND`, `BACKEND_OF ZenithPhysics`, `BACKEND_NAME Jolt`, `FACTORY Zenith_CreatePhysics_Jolt`) | ZenithPhysics, ZenithJobs, ZenithCore, ZenithProfiling; `THIRD_PARTY jolt` | layer 8 |
| `ZenithPhysics_PhysX` (`BACKEND_NAME PhysX`, `FACTORY Zenith_CreatePhysics_PhysX`) | ZenithPhysics, ZenithJobs, ZenithCore, ZenithProfiling; `THIRD_PARTY physx` | layer 8 |
| `ZenithCook` (PHYS's cook steps) | gains ZenithPhysics, ZenithPhysics_Jolt, ZenithPhysics_PhysX for `BakeShape` (row PH5) | tool side |

Libraries above PHYS that depend on it: ZenithAnimation (AN2), ZenithAI, ZenithGameplay, ZenithEnvironment (cook side only through ZenithCook intermediates; no runtime edge).

### Headings this area relies on

| Topic | Heading |
|---|---|
| module entry points and contexts | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" |
| modules and backends | `Zenith2/Docs/Architecture.md` → "Engine modules and game projects" |
| phases, authority, arbitration | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases", `Zenith2/Docs/Architecture/FramePhases.md` → "Writer sources and default precedence" |
| determinism | `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" |
| memory consumers | `Zenith2/Docs/Architecture.md` → "Memory governor" |
| locks and FP environment | `Zenith2/Docs/Contracts.md` → "Lock ordering table", `Zenith2/Docs/Contracts.md` → "Floating-point environment" |
| third-party pins | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency entry fields" |
| teleports and ragdoll commands | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Ragdoll enter and recover" |
| command playback and the world tag | `Zenith2/Docs/Architecture/ECSModel.md` → "Playback access and supporting types" |
| origin rebasing | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebase rule", `Zenith2/Docs/Design/STREAM/Runtime.md` → "Who reacts to a rebase", `Zenith2/Docs/Design/STREAM/Runtime.md` → "Precision target" |
| asynchronous completion and retired tables | `Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results", `Zenith2/Docs/Design/SAVE/Formats.md` → "Async section" |
| script misuse | `Zenith2/Docs/Contracts.md` → "Errors and results", `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" |
| save participants | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header" |
| root motion | `Zenith2/Docs/Design/ANIM/Graph.md` → "Root motion publication" |
| streaming coordination | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Physics coordination" |
| terrain collision | `Zenith2/Docs/Design/ENV/Bake.md` → "Heightfield extraction", `Zenith2/Docs/Design/ENV/Bake.md` → "Collidable foliage" |
| camera collision | `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface" |
| debug drawing | `Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header" |
| headless test worlds | `Zenith2/Docs/Design/MOD.md` → "Zenith_RuntimeTestAccess.h", `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem" |
| physical-material names | `Zenith2/Docs/Design/ASSET.md` → "Content root mapping", `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext" |

### Assumed interfaces

<!-- assumption: pending MOD -->
| Interface | Used for | Row |
|---|---|---|
| `Zenith_WorldAttachContext::FindCommandKind(const char* szName) → uint32` | resolving `Zenith_PhysicsCommandKinds` at the first attach (the start context registers kinds but returns no ID) | PH7 (GA5c's request) |
| `Zenith_WorldAttachContext::GetPublicationSink() → Zenith_ModulePublicationSink&`, `Park(ulCompletionID, uSourceKind, uPublicationKind, xPayload)`, valid until `DetachWorld` returns and parking into the world's table live or retired | completing deferred queries, cancelled ones at `DetachWorld` included | PH7, PH23 (the single module publication sink of the orchestrator's Q76 (a) ruling, which MOD's post-signoff patch specifies) |
| `Zenith_ModuleStartContext::GetDebugDraw() → Zenith_DebugDraw*` (null in Shipping) | `Physics.DebugDraw` | PH7 (DV12's request) |
<!-- /assumption -->

<!-- assumption: pending SAVE -->
`Zenith_SaveRestoreContext::ReportCoherentRestore(uint64 ulParticipantKey) const`, with which an EXACT participant whose restore fell back reports it (row PH22; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Save and restore algorithm").
<!-- /assumption -->

<!-- assumption: pending SCRIPT, ECS -->
PHYS's misuse block ZS2300–ZS2399, assigned by the orchestrator's MISUSE-BLOCKS-2 ruling, written into `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" by core-doc item X264 (k); `PROPERTY_FLAG_WORLD_POSITION` (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "ECS world-position flag", core-doc item X227 (a)) on CharacterController 35 TargetPosition.
<!-- /assumption -->

Settled interfaces PHYS relies on (no longer assumptions):

- `Zenith_WorldAttachContext::GetSceneState() → Zenith_SceneWorldState*` ("never null in a runtime world") and `Zenith_WorldAttachContext::RegisterSaveParticipant(Zenith_SaveParticipant&)` (forwarding to the world's `Zenith_SaveSystem::RegisterParticipant`, journaled and undone with the module), both `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence": PHYS reads the lifecycle dispatcher and the entity table through the pointer at attach and registers its participant through the context ("Module start and world attach" step 7).
- `Zenith_FluidQuery` is SCENE's (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_FluidQuery header", row PH8 applied); PHYS consumes it (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_FluidQuery"), and `Zenith_CollisionQuery` (GA2) is GAME's interface in ZenithScene that PHYS implements, so neither PHYS nor ENV gains a module-to-module edge.
- The `Scene.ShiftWorldOrigin` playback and its `LIFECYCLE_EVENT_ORIGIN_SHIFTED` dispatch are SCENE's (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shift playback", → "Origin shifted lifecycle event", row PH20 applied); the enumerator itself joins Architecture's listener enumeration through core-doc item X264 (j), the one core dependency left (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift").
- `Zenith_WorldPlaybackAccess::GetWorldTag()`, by which a module command's playback finds its per-world state (`Zenith2/Docs/Architecture/ECSModel.md` → "Playback access and supporting types", [Z2-N-ARCH-716]; core-doc item X238, applied in core-doc patch 2026-10-03d). Every PHYS playback function receives the module instance as `m_pUserData` and looks its world up by that tag (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Module world state").

<!-- assumption: pending ASSET -->
`ASSET_KIND_PHYSICS_MATERIAL` and `ASSET_KIND_PHYSICS_RAGDOLL` take the next two codes `uASSET_KIND_DEFINED_COUNT` names when PHYS's kind ticket lands (row PH4; "Asset kinds"). `Zenith_CookContext::FindAssetByLogicalPath(Zenith_StringView) const → const Zenith_AssetRecord*` over the plan's record snapshot, with which PHYS's heightfield and compound cook steps resolve physical-material names and the ragdoll cook resolves its skeleton path (row PH26; `Zenith2/Docs/Design/PHYS/Formats.md` → "Physical-material names").
<!-- /assumption -->

### Areas that consume PHYS

| Area | Uses |
|---|---|
| ANIM | ragdoll component, pose source, body poses, foot-placement ray casts |
| GAME | `Zenith_CollisionQuery`, trigger events, ragdoll lifecycle tests |
| AI | queries for perception and cover |
| SCRIPT | the `Physics` bindings and the asynchronous source `ASYNC_SOURCE_PHYSICS_QUERY` |
| STREAM | none called; PHYS reads its components by name |
| ENV | cook intermediates; `Zenith_FluidQuery` provider |
| SAVE | the physics participant |
| EDIT | collider and joint gizmos through the debug views and the component metadata |
| DEVT | debug draw, counters |

## Library and file layout

### Target summary

```cmake
zenith_add_library(ZenithPhysics KIND MODULE MODULE_NAME Physics
	DEPENDS ZenithRuntime ZenithScene ZenithECS ZenithReflect ZenithAssets ZenithSave ZenithPlatform ZenithJobs ZenithCore ZenithProfiling
	SUPPORTED ZENITH_GPU)
zenith_add_library(ZenithPhysics_Jolt KIND BACKEND BACKEND_OF ZenithPhysics BACKEND_NAME Jolt FACTORY Zenith_CreatePhysics_Jolt
	DEPENDS ZenithPhysics ZenithJobs ZenithCore ZenithProfiling THIRD_PARTY jolt SUPPORTED ZENITH_GPU)
zenith_add_library(ZenithPhysics_PhysX KIND BACKEND BACKEND_OF ZenithPhysics BACKEND_NAME PhysX FACTORY Zenith_CreatePhysics_PhysX
	DEPENDS ZenithPhysics ZenithJobs ZenithCore ZenithProfiling THIRD_PARTY physx SUPPORTED ZENITH_GPU)
```

### ZenithPhysics files

```
Zenith2/Engine/ZenithPhysics/
	<the public headers of Zenith2/Docs/Design/PHYS/PublicAPI.md → "Header list">
	Internal/
		Zenith_PhysicsBackendBase.h          the backend-shared helpers included by backends: Zenith_PhysicsHandleCodec, Zenith_PhysicsSlotTable
		                                     ("Slot allocation" of PHYS/Simulation.md), Zenith_PhysicsWorldBase (the common world state both
		                                     backends derive from: slot tables, per-slot user data and ragdoll part codes, excluded pairs,
		                                     contact-sink plumbing), the collectors' glue
		Zenith_PhysicsBackendBase.cpp
		World/       Zenith_PhysicsWorldState.h/.cpp, Zenith_PhysicsModule.cpp, Zenith_PhysicsAttach.cpp, Zenith_PhysicsConfig.cpp,
		             Zenith_PhysicsLifecycle.cpp (the lifecycle listener, origin shift included)
		Ingest/      Zenith_PhysicsIngest.cpp, Zenith_PhysicsComposition.cpp, Zenith_PhysicsPendingOps.cpp
		Step/        Zenith_PhysicsStepNode.cpp, Zenith_PhysicsWriteBack.cpp, Zenith_PhysicsSync.cpp, Zenith_PhysicsEvents.cpp
		Query/       Zenith_PhysicsQuery.cpp, Zenith_PhysicsPrimitiveCache.cpp, Zenith_PhysicsDeferredQuery.h/.cpp,
		             Zenith_PhysicsCollisionQuery.h/.cpp
		Controller/  Zenith_CharacterMove.h/.cpp
		Ragdoll/     Zenith_RagdollRuntime.cpp
		Vehicle/     Zenith_PhysicsVehicle.cpp, Zenith_PhysicsBuoyancy.cpp
		Assets/      Zenith_CollisionShapeLoader.cpp, Zenith_PhysicsMaterialLoader.cpp, Zenith_RagdollLoader.cpp
		Save/        Zenith_PhysicsSaveParticipant.cpp
		Script/      Zenith_PhysicsScriptBindings.cpp
		Debug/       Zenith_PhysicsDebugDraw.cpp
		Zenith_PhysicsLayers.h/.cpp, Zenith_PhysicsCommands.cpp
	Tests/  Support/ (header-only: scenarios, conformance harness, test world and test pose source, fake backend, state hash, seed
	        files, version-pinned save-section writers; PHYS/Tests.md "Test support"), Conformance/, Zenith_<Type>_Tests.cpp
```

### Backend library files

```
Zenith2/Engine/ZenithPhysics_Jolt/
	Zenith_Physics_Jolt.h
	Internal/ Zenith_PhysicsJolt.cpp, Zenith_PhysicsWorldJolt.cpp, Zenith_JoltAllocator.cpp, Zenith_JoltLayers.cpp,
	          Zenith_JoltContactListener.cpp, Zenith_JoltGroupFilter.cpp, Zenith_JoltShapes.cpp, Zenith_JoltHeightField.cpp,
	          Zenith_JoltConstraints.cpp, Zenith_JoltQueries.cpp, Zenith_JoltStateRecorder.cpp, Zenith_JoltStreams.cpp
	          (Zenith_JoltJobSystem.h/.cpp only if the conditional decision of PHYS/Backends.md "Conditional Jolt parallel step" admits it)
	Tests/
Zenith2/Engine/ZenithPhysics_PhysX/
	Zenith_Physics_PhysX.h
	Internal/ Zenith_PhysicsPhysX.cpp, Zenith_PhysicsWorldPhysX.cpp, Zenith_PhysXAllocator.cpp, Zenith_PhysXErrorCallback.cpp,
	          Zenith_PhysXDispatcher.cpp, Zenith_PhysXFilterShader.cpp, Zenith_PhysXEvents.cpp, Zenith_PhysXShapes.cpp,
	          Zenith_PhysXConstraints.cpp, Zenith_PhysXQueries.cpp (with Zenith_PhysXQueryFilter), Zenith_PhysXHeightField.cpp,
	          Zenith_PhysXStreams.cpp
	Tests/
Zenith2/ThirdParty/physx/CMakeLists.txt     the zenith-wrapper build (Zenith2/Docs/Design/PHYS/Backends.md → "Third-party pins")
```

### ZenithCook PHYS files

```
Zenith2/Tools/ZenithCook/Internal/Physics/
	Zenith_PhysicsCookSteps.cpp        physics.shape, physics.collision, physics.heightfield, physics.foliageCompound, physics.material, physics.ragdoll
	Zenith_PhysicsConvexHull.cpp        quickhull
	Zenith_PhysicsMeshCook.cpp          welding, active edges, Morton order
	Zenith_PhysicsIntermediates.h/.cpp  ZPHI / ZPCI writers and readers (ENV's importer calls the writers)
	Zenith_PhysicsDocuments.cpp         the three document types and their migrations
Zenith2/Tools/ZenithCook/Tests/Physics/
```

### Integration suite and benchmark files

`Zenith2/Tests/Physics/` (integration suite), `Zenith2/Benchmarks/PhysicsBackends/` (the D-SEL-PHYSX-ANDROID harness), `Zenith2/Benchmarks/Budgets/PHYS/` (the budget measurements).

### ZENITH_SUPPORTED matrix

| Target | win-debug, win-release | android-arm64 | android-x86_64 | win-asan | linux-tsan |
|---|---|---|---|---|---|
| ZenithPhysics | yes | yes | yes | yes | no (`ZENITH_GPU`) |
| ZenithPhysics_Jolt | yes | yes | yes | yes | no |
| ZenithPhysics_PhysX | yes | yes until D-SEL-PHYSX-ANDROID rules against Android, then no (`ZENITH_WINDOWS`) | as android-arm64 | yes | no |

`ZENITH_GPU` includes both Android ABIs (`Zenith2/Docs/Architecture/BuildSystem.md`); the PhysX wrapper builds for whichever Android ABIs its manifest entry lists. No preset runs a race detector over the module (`linux-tsan` builds no `ZENITH_GPU` library): `Zenith2/Docs/Design/PHYS/Tests.md` → "Gate steps for PHYS diffs" states the gap and the stress tests that stand in for it.

## Public API

### Header map

| Header | Content | Specified in |
|---|---|---|
| `Zenith_PhysicsTypes.h`, `Zenith_PhysicsErrors.h`, `Zenith_Physics.h`, `Zenith_PhysicsShape.h`, `Zenith_PhysicsMaterial.h`, `Zenith_PhysicsWorld.h`, `Zenith_PhysicsQuery.h`, `Zenith_CollisionShapeAsset.h`, `Zenith_Module_Physics.h`, the two factory headers | the neutral API | `Zenith2/Docs/Design/PHYS/PublicAPI.md` |
| `Zenith_PhysicsComponents.h`, `Zenith_PhysicsEvents.h`, `Zenith_PhysicsWorldResource.h`, `Zenith_PhysicsCommands.h`, `Zenith_Ragdoll.h`, `Zenith_CharacterController.h`, `Zenith_PhysicsVehicle.h`, `Zenith_PhysicsDebug.h` | the module's ECS-facing API | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` |

### Rules across the PHYS API

- Every recoverable failure is `Zenith_Result<…, Zenith_PhysicsError>` or a counted no-op; untrusted data (cooked files, documents, saves, script arguments) never asserts.
- No third-party type appears in a public header; backend handles are opaque 64-bit values.
- Gameplay code never calls `Zenith_PhysicsWorld` directly: it records commands, reads components and queries through `Zenith_PhysicsWorldResource`; only the module calls the backend.
- Every query result is a deterministic function of the committed state and the arguments.

### Limits and constants

| Constant | Value | Reason |
|---|---|---|
| `uPHYSICS_MAX_SUBSTEPS_PER_TICK` | 8 | the substep cap (the old engine's unbounded loop sank a resting body 0.61 m through terrain) |
| `uPHYSICS_MAX_LAYERS` | 32 | one bit per layer in a 32-bit mask |
| `uPHYSICS_MAX_BODIES_PER_WORLD` | 8,388,607 | the 23-bit body index (Jolt's own body-index range) |
| `uPHYSICS_MAX_CONSTRAINTS_PER_WORLD` | 1,048,575 | 20-bit constraint index |
| `uPHYSICS_MAX_COMPOUND_CHILDREN` / `uPHYSICS_MAX_COOKED_COMPOUND_CHILDREN` | 64 / 65,535 | runtime composition / cooked static compounds |
| `uPHYSICS_MAX_MATERIAL_PALETTE` | 256 | 8-bit palette indices |
| `uPHYSICS_MAX_CONVEX_VERTICES` | 256 | both backends' hull limits |
| `uPHYSICS_MAX_MESH_TRIANGLES` | 2,097,152 | cooked mesh size bound |
| `uPHYSICS_MAX_HEIGHTFIELD_SAMPLES` | 4,097 per side, square | heightfield bound; Jolt's heightfield takes one sample count for both axes |
| `uPHYSICS_MAX_HEIGHTFIELD_PALETTE` | 127 | PhysX's 7-bit per-triangle material index, whose value 127 is the hole |
| `uPHYSICS_MAX_WORLD_TAG` | 4,095 | the 12-bit world-tag field of body and constraint handles, the ECS world tag range |
| `uPHYSICS_MAX_QUERY_HITS` | 256 | the largest collector |
| `uPHYSICS_MAX_RAGDOLL_PARTS` | 64 | ragdoll definition bound |
| `uPHYSICS_MAX_VEHICLE_WHEELS` | 8 | vehicle bound |
| `uPHYSICS_MAX_BUOYANCY_SAMPLES` | 32 | buoyancy bound |
| `fPHYSICS_MAX_STEP_SECONDS` | 0.1 s | the longest substep a world accepts |

### Error codes

| Code | Meaning | Returned by |
|---|---|---|
| `PHYSICS_ERROR_INVALID_DESC` | a descriptor field out of range, non-finite, or inconsistent | factories, `CreateWorld`, `CreateBody`, `CreateShape`, `CreateConstraint` |
| `PHYSICS_ERROR_OUT_OF_MEMORY` | an allocation failed | every creating member |
| `PHYSICS_ERROR_BACKEND_INIT` | the third-party engine refused to initialise | factories, `CreateWorld` |
| `PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE` | a second instance of one backend kind | factories |
| `PHYSICS_ERROR_BODY_CAPACITY`, `…_CONSTRAINT_CAPACITY`, `…_SHAPE_CAPACITY` | a table is full; nothing changed | creating members |
| `PHYSICS_ERROR_INVALID_SHAPE` | a shape descriptor that cannot form a shape (degenerate hull, negative extents) | `CreateShape` |
| `PHYSICS_ERROR_NULL_HANDLE`, `…_STALE_HANDLE`, `…_WRONG_WORLD` | handle validation | every member taking a handle |
| `PHYSICS_ERROR_COOKED_CORRUPT` | cooked bytes fail a check | `CreateShapeFromCooked`, loaders |
| `PHYSICS_ERROR_COOKED_BACKEND_MISMATCH` | a backend section of another backend or data tag was offered | `CreateShapeFromCooked` (the loader then uses the neutral section) |
| `PHYSICS_ERROR_UNSUPPORTED` | the capability is absent (`SaveState` on PhysX, `BakeShape` for heightfields) | members gated by "Capability matrix" |
| `PHYSICS_ERROR_STATE_MISMATCH` | a saved state does not match the recreated world | `RestoreState` |
| `PHYSICS_ERROR_STEP_FAILED` | the backend reported an internal failure during a step | `StepOnce`, `ResumeStep` |
| `PHYSICS_ERROR_MOTION_TYPE` | an operation invalid for the body's motion type (force on a static body) | body members |
| `PHYSICS_ERROR_LAYER_CONFIG` | the layer configuration is invalid | layer parsing, `CreateWorld` |
| `PHYSICS_ERROR_RAGDOLL_INVALID` | a ragdoll definition does not fit the skeleton | the ragdoll enter procedure |
| `PHYSICS_ERROR_WORLD_ACTIVE` | the service is destroyed or reconfigured while a world lives | the service's destructor check, `CreateWorld` on a closing service |

### Capability matrix

| Capability | Jolt | PhysX |
|---|---|---|
| `PHYSICS_CAPABILITY_EXACT_STATE_RESTORE` | yes | no (COHERENT) |
| `PHYSICS_CAPABILITY_PARALLEL_STEP` | no (the single-threaded baseline; yes only if the Stage 1 decision of `Zenith2/Docs/Design/PHYS/Backends.md` → "Conditional Jolt parallel step" admits PHYS-1) | yes (drain waves) |
| `PHYSICS_CAPABILITY_BACKEND_SHAPES` | yes (hull, mesh, compound) | yes (hull, mesh) |
| `PHYSICS_CAPABILITY_ORIGIN_SHIFT` | no (per body) | yes (native) |

### Asset kinds

| Code | Enumerator | Stable name | Script name | Runtime object type | Extension | Magic | Memory subsystem | Fallback | Cacheable | GPU resident |
|---|---|---|---|---|---|---|---|---|---|---|
| 16 | `ASSET_KIND_COLLISION_SHAPE` | `Physics.CollisionShape` | `CollisionShape` | `Zenith_CollisionShapeAsset` | `.zcol` | `ZCOL` | SIMULATION | FAIL | yes | no |
| next (row PH4) | `ASSET_KIND_PHYSICS_MATERIAL` | `Physics.Material` | `PhysicsMaterial` | `Zenith_PhysicsMaterialAsset` | `.zpmat` | `ZPMT` | SIMULATION | SUBSTITUTE (the default material) | yes | no |
| next + 1 (row PH4) | `ASSET_KIND_PHYSICS_RAGDOLL` | `Physics.Ragdoll` | `Ragdoll` | `Zenith_RagdollAsset` | `.zragdoll` | `ZRGD` | SIMULATION | FAIL | yes | no |

The loaders run on asset-loader jobs, create backend shapes and materials through the service (whose create members are `ANY_THREAD`), and hold handles released when the registry retires the asset.

### Command kinds

Registered at module start through `RegisterCommandKind` (`m_szName`, payload `Zenith.Physics.BodyCommandPayload` (56 bytes) or the ragdoll descriptors, allowed phases 1–10):

| Order | Name | Recording function | Playback |
|---|---|---|---|
| 1 | `Physics.AddImpulse` | `Zenith_RecordPhysicsAddImpulse` (and `…AddAngularImpulse`, a flag of the same kind) | append a pending operation to the world's state |
| 2 | `Physics.AddForce` | `Zenith_RecordPhysicsAddForce` (and `…AddTorque`) | accumulate for the next tick's substeps |
| 3 | `Physics.SetVelocity` | `Zenith_RecordPhysicsSetVelocity` | pending operation |
| 4 | `Physics.SetSleeping` | `Zenith_RecordPhysicsSetSleeping` | pending operation |
| 5 | `Physics.EnterRagdoll` | `Zenith_RecordEnterRagdoll` | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll enter procedure" |
| 6 | `Physics.RecoverRagdoll` | `Zenith_RecordRecoverFromRagdoll` | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll recover procedure" |
| 7 | `Physics.RemoveRagdollBodies` | recorded by `Physics.RagdollBlend` when a recover completes | destroy the parts and their constraints, mark the excluded pairs dirty and insert the character into the ended list (the capsule's contacts then continue its pairs as a handover; `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll state machine") |
| 8 | `Physics.SetGravity` | `Zenith_RecordPhysicsSetGravity` | world gravity from the next tick |

Every playback function finds the world it changes by `xWorld.GetWorldTag()` in the module instance's world-state map (`m_pUserData` is the module instance, one per process; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Module world state"), so with several worlds a command never reaches another world's state. Every recording function takes `const Zenith_PhysicsCommandKinds&` (from `Zenith_PhysicsWorldResource::GetCommandKinds`), which changes the signatures `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Ragdoll enter and recover" shows (row PH6).

### Layer configuration

`physics.layerNames` is a comma-separated list of 1 … 32 names (`^[A-Za-z][A-Za-z0-9_]{0,30}$`, unique); layer i is the i-th name. `physics.layerCollisions` is a comma-separated list of pair tokens `A:B` (collide), `!A:B` (do not collide), with `*` matching every layer, applied left to right on a table that starts all-false; the table is made symmetric (`A:B` sets both bits). Examples: `*:*,!Debris:Debris,!Trigger:Trigger`. A name not declared, a malformed token, more than 1,024 tokens or more than 8 KiB is `PHYSICS_ERROR_LAYER_CONFIG` with the token's position, refused at module start (the keys require a restart). The script name table `physicsLayer` lists the names in order, so `Physics.Layer("Debris")` resolves at script compile time.

### Module start and world attach

`Zenith_Module_Physics_RegisterTypes` (startup step 6): register the components, events, value structs, world resource type, configuration keys of `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Configuration keys", the three document types and the census metadata; a refused registration is `MODULE_ERROR_REGISTRATION`.

`Zenith_Module_Physics_Start` (startup step 15), each step a fault point (`Test_PhysicsModule_Create_FailsAt<Step>_Unwinds`), unwound in reverse:

| Step | Action | Failure |
|---|---|---|
| 1 Config | read and validate every `physics.*` key; parse the layer table | `MODULE_ERROR_REGISTRATION` |
| 2 Backend | `pfnCreateBackend(desc)` (the one-parameter `Zenith_PfnCreatePhysics` of `Zenith2/Docs/Architecture/CompositionRoot.md`) | `MODULE_ERROR_BACKEND_CREATE` (the physics error logged) |
| 3 Module object | the module instance, the primitive cache, the process tables | `MODULE_ERROR_OUT_OF_MEMORY` |
| 4 Kind loaders | register the loaders of "Asset kinds" with the asset registry | `MODULE_ERROR_REGISTRATION` |
| 5 Command kinds | the eight rows of "Command kinds" | `MODULE_ERROR_REGISTRATION` |
| 6 Script | `RegisterScriptBindings` (the `Physics` namespace); `DeclareScriptAsyncSource(ASYNC_SOURCE_PHYSICS_QUERY, uPHYSICS_QUERY_RESULT_BYTES)`; `SetScriptSideEffectConsumer` for the deferred-query requests | `MODULE_ERROR_REGISTRATION` |
| 7 Memory | `RegisterMemoryConsumer` | — |

`AttachWorld` per world, unwound in reverse by `DetachWorld` or on failure:

| Step | Action |
|---|---|
| 1 | resolve `Zenith_PhysicsCommandKinds` (first attach) and the stable-name property IDs (`Zenith.Animation.RootMotion`, `Zenith.Streaming.RegionBound`, `Zenith.WorldOrigin`, `Zenith.LocalTransform`, `Zenith.WorldTransform`) |
| 2 | `CreateWorld` with the world tag and the configuration capacities |
| 3 | the world state (body table, pairs, pending operations, contact sink, deferred inbox) |
| 4 | `AddWorldResource` for `Zenith.Physics.World` and `Zenith.CollisionQuery` (owning phase `PHASE_PHYSICS`) |
| 5 | register the nodes of `Zenith2/Docs/Design/PHYS/Simulation.md` → "Frame-graph placement" and the debug epilogue stage |
| 6 | `AddLifecycleListener` ("Lifecycle listener") |
| 7 | `xContext.RegisterSaveParticipant(participant)` (the world's `Zenith_PhysicsSaveParticipant`, `Zenith2/Docs/Design/PHYS/Simulation.md` → "Save and restore algorithm") |
| 8 | insert the world state into the module instance's world-state map under the world tag |

<!-- elaboration -->
In a world whose `GetWorldPurpose()` is `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` (A3 cross-area row ED5), every step above runs, so the physics world holds the world's bodies and answers queries (the editor's surface snapping). `Physics.Step` then runs its ingest alone: it creates, recomposes and destroys bodies and applies external writes as authored poses. It runs no substep, writes nothing back, produces no contact or trigger event and advances no body. `Physics.CharacterMove` and `Physics.RagdollBlend` return at entry. The world state records the purpose at step 3 as `m_bSimulates = (purpose == WORLD_PURPOSE_PLAY)`. `Test_PhysicsModule_EditWorldHoldsBodiesWithoutStepping` asserts it on both backends: an EDIT world's falling box answers a ray cast at its authored pose and has not moved after 30 ticks, and no `Zenith.Physics.Contact` event is raised; a PLAY world's box falls.

`DetachWorld` unwinds in reverse; its first action, before any step is unwound and before the world state is destroyed, completes every INBOX deferred query (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred query state machine", cross-doc item X265), and it then removes the world from the map.

### Lifecycle listener

| Event | PHYS's action |
|---|---|
| `LIFECYCLE_EVENT_SPAWNED` | none (bodies are created at ingest from components) |
| `LIFECYCLE_EVENT_TELEPORTED` | move the body (and every ragdoll part rigidly, `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Teleport during a ragdoll") to the target; zero velocities unless the teleport keeps them; reset the controller's ground and platform state; set the written local values to the teleported local bits so the move is not seen as an external write |
| `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (dispatched by SCENE's playback, `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shifted lifecycle event") | shift the backend world and the module's position-valued state at this sync point (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift") |
| `LIFECYCLE_EVENT_DESTROY_PENDING` | mark the body for destruction at the end-of-tick playback |
| `LIFECYCLE_EVENT_DESTROYED` | destroy the body, its constraints and its ragdoll parts and their constraints; set `m_bExcludedPairsDirty` when any destroyed constraint had CollideConnected false (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Excluded pairs maintenance"); insert the entity's simulation ID into the ended list `m_aulEndedPairIDs`, so its contact and trigger pairs end at the next emission (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Event emission" step 2) |
| `LIFECYCLE_EVENT_RAGDOLL_ENTER`, `…_RECOVER` | dispatched by PHYS itself; PHYS's listener ignores them |
| cinematic events | none |

### Handle kinds used

`Zenith_PhysicsBodyHandle`, `Zenith_PhysicsConstraintHandle`, `Zenith_PhysicsShapeHandle`, `Zenith_PhysicsMaterialHandle` are generational 64-bit handles (index, generation, world or service tag) with the codec of `Zenith2/Docs/Design/PHYS/PublicAPI.md` → "Zenith_PhysicsTypes header"; `Zenith_AssetHandle<…>` for the three kinds; `Zenith_EntityHandle` in events and query results.

## Data structures and formats

### Format index

| Format | Heading |
|---|---|
| `.zcol` | `Zenith2/Docs/Design/PHYS/Formats.md` → "Collision shape file" |
| `.zpmat` | `Zenith2/Docs/Design/PHYS/Formats.md` → "Material file" |
| `.zragdoll` | `Zenith2/Docs/Design/PHYS/Formats.md` → "Ragdoll file" |
| documents | `Zenith2/Docs/Design/PHYS/Formats.md` → "Material document", "Ragdoll document", "Collision shape document" |
| importer settings | `Zenith2/Docs/Design/PHYS/Formats.md` → "Collision importer settings" |
| ENV intermediates | `Zenith2/Docs/Design/PHYS/Formats.md` → "Heightfield intermediate", "Compound intermediate" |
| deferred query payloads | `Zenith2/Docs/Design/PHYS/Formats.md` → "Deferred query payloads" |
| save section | `Zenith2/Docs/Design/PHYS/Formats.md` → "Save section layout" |
| backend payloads | `Zenith2/Docs/Design/PHYS/Backends.md` → "Backend shape payloads" |

### In-memory sizes that matter

| Record | Size |
|---|---|
| body record (module) | 128 bytes (simulation ID, handle, entity, composition hash, written pose and velocities, flags) |
| pair record | 56 bytes |
| contact record | 64 bytes |
| hit | 64 bytes |
| pending operation | 72 bytes |
| deferred request / result | 64 / 64 bytes |

At the W-Dense design defaults (65,536 bodies, 65,536 pairs, 8,192 contact records, a 65,536-entry ended list) the module's own tables hold about 13 MiB beside the backend's bytes, all counted by the memory consumer.

### Byte order and serialization rules

Little-endian, floats by bit pattern, persistent IDs in their 16-byte form, no pointer or interned ID on disk ("Common rules" of `Zenith2/Docs/Design/PHYS/Formats.md`).

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading |
|---|---|
| ingest, composition, external writes, pending operations | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Ingest algorithm" |
| substep loop | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Substep loop" |
| write-back and sync | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Write-back", "Physics-to-transform sync" |
| events, triggers, interactions | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Event emission" |
| joints and breakage | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Joint breakage" |
| character move | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Character move algorithm" |
| ragdoll procedures | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll enter procedure" |
| vehicles, buoyancy | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Vehicle algorithm", `Zenith2/Docs/Design/PHYS/Simulation.md` → "Buoyancy algorithm" |
| queries | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Query execution", "Deferred queries" |
| save and restore | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Save and restore algorithm" |
| origin shift | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift" |
| cooking | "Cook steps and derived-data keys" |

### State machine index

| Machine | Heading |
|---|---|
| body lifecycle | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Body lifecycle state machine" |
| constraint | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Constraint state machine" |
| deferred query | `Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred query state machine" |
| ragdoll | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Ragdoll state machine" |

### Tick-level flow of PHYS work

1. Sync points after phases 1–4: command playback appends pending operations and runs ragdoll enters and recovers; the lifecycle listener applies teleports and, at the sync point that plays SCENE's origin shift (sync point 1 of a rebase tick), shifts the physics world, so phases 2 to 4 see shifted transforms and a shifted world together.
2. Phase 4: `Physics.CharacterMove` moves every controller (velocity input or root motion) through collision.
3. Phase 5: `Physics.Step` ingests (creates, recomposes, destroys bodies; external writes; pending operations; the origin consistency check), runs n substeps (n ≤ 8), checks joints, writes back components, sorts the tick's contacts and emits the contact, trigger, interaction and joint events through simulation-ID merge keys for delivery in phase 9.
4. Phase 6: `Physics.SyncTransforms` writes poses (writer source Physics); `Physics.RagdollBlend` advances ragdolls (writer source Ragdoll); `Physics.DeferredQueries` completes script queries.
5. Epilogue: `Physics.DebugDraw` (development builds).

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_Physics` | create/release members `ANY_THREAD` (loader jobs); `CreateWorld` `MAIN_THREAD` |
| `Zenith_PhysicsWorld` | mutating members and steps `PHASE(Physics)` (and the main thread out of tick during a restore); queries `ANY_THREAD` while no step runs |
| `Zenith_PhysicsWorldResource` | queries from every node that declares `ReadResource`; `SetRagdollPoseSource` at attach |
| components | per the census rows (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Authority census rows") |

### Node summary

The six nodes and their access are the table of `Zenith2/Docs/Design/PHYS/Simulation.md` → "Frame-graph placement". The step node declares `WriteResource` on both world resources, so no query node runs concurrently with it.

### Lock-order entries

Two rows PHYS asks the core batch to add (row PH3):

| Level | Constant | Owner | Guards | Short-hold | Same level by address | Acquired on |
|---|---|---|---|---|---|---|
| 500 | `uLOCK_LEVEL_PHYSICS_HANDLE_TABLES` | ZenithPhysics | the shape and material handle tables' slot allocation and free lists (the service's mutex), and the primitive cache's entries, pins and index (the cache's own mutex) | yes | no | any thread (loader jobs, query nodes) |
| 510 | `uLOCK_LEVEL_PHYSICS_DISPATCH` | ZenithPhysics_PhysX | the PhysX dispatcher's task queue (push and pop only; a task never runs under it) | yes | no | PhysX drain leaves, the step node |

Row 500 is the level of two different mutexes, the service's handle-table mutex and the primitive cache's mutex. They are never held together, and the cache's mutex is never held across a call into `Zenith_Physics` (whose `CreateShape` and `ReleaseShape` take the service's mutex; `Zenith2/Docs/Design/PHYS/Simulation.md` → "Primitive cache"), so the row needs no same-level-by-address rule. Only if PHYS-1 is admitted, a third row joins through core-doc item X264 (m): 520 `uLOCK_LEVEL_PHYSICS_JOLT_JOBS` (ZenithPhysics_Jolt; the conditional Jolt job system's job pool free list and queue push and pop, no job runs under it; short-hold; not same-level by address; the step node and the Jolt drain leaves; `Zenith2/Docs/Design/PHYS/Backends.md` → "Conditional Jolt parallel step"), and a fourth: 530 `uLOCK_LEVEL_PHYSICS_CONTACT_SINK` (ZenithPhysics_Jolt; the world's contact sink's heap and dropped count and the Jolt listener's removed-contact staging, taken by the contact callbacks around `Append` and the staging push under the parallel step; nothing runs under it; short-hold; not same-level by address; the Jolt drain leaves and the step node's waiter, always with no other lock held, so never with 520; `Zenith2/Docs/Design/PHYS/Backends.md` → "Conditional Jolt parallel step", paragraph "Contact listener under the parallel step"). With the single-threaded baseline neither row exists.

No PHYS code holds any of these locks while calling into a backend step, a callback or another Zenith2 lock below 850.

### Construction and shutdown

Start and attach are the step tables of "Module start and world attach". Shutdown: `DetachWorld` per world (save participant unregistered, listener removed, nodes removed with the world's schedule, resources removed, world state destroyed, `Zenith_PhysicsWorld` destroyed), then the module instance at shutdown step 6 in reverse registration order: memory consumer unregistered, primitive cache emptied (every entry's shape released; no pin is outstanding once every world has detached), loaders' assets already retired by the registry, then the backend service (whose destructor checks that nothing is live).

### Cancellation

PHYS owns no background operation. A deferred query is cancelled by its script owner (SCRIPT's completion rules); a request whose entity or world is gone completes as failed. Frame cancellation follows [Z2-N-ARCH-412]: a cancelled schedule instance skips the bodies of nodes that have not started and still runs every started node and its continuations. PHYS checks `Zenith_JobContext::IsCancelled` in one place only, in `Physics.Step` after a substep has completed (after `StepOnce` returned not pending, or after the `ResumeStep` that completed it), never while a step is pending, so the PhysX chain never leaves `simulate()` without its `fetchResults`; once the check fires, `Physics.Step` registers no further substep, skips write-back and event emission, and returns, leaving the world as the last completed substep left it. No world is stepped after a cancelled frame: `Zenith_FrameExecutor::RequestCancel` is "shutdown and world destruction" (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Execution"), so the next call into the world is `DetachWorld`, which completes the INBOX queries and destroys the backend world with nothing pending (the tick's unread sink records and pair changes are discarded with the world state). `Test_PhysicsStep_CancelAfterSubstepDetachesClean` and the cancelled-wave case of `Test_PhysicsPhysX_ResumeStepUntilComplete` assert it. `Physics.DeferredQueries` never checks it: a started run parks every entry it executed and removes an inbox entry only when it parks it, and a run skipped before it started leaves its entries INBOX, completed by the next run or by `DetachWorld` (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred queries"). The other nodes check nothing and complete their work.

### Error enumerations

`Zenith_PhysicsError` ("Error codes"); module entry points return `Zenith_ModuleError`; loaders return the asset registry's errors with the physics code in the log line.

### Luau errors and asserts

Script misuse always raises a controlled Luau error and never asserts ([Z2-N-CONTRACT-473]): every `Physics` binding validates its arguments first and fails the call through `Zenith_ScriptFailThunk` with a catalogued code (SCRIPT's ZS1104, ZS1511 and ZS1807 from the dispatcher and loader, PHYS's own block ZS2300–ZS2399, assigned by the orchestrator's MISUSE-BLOCKS-2 ruling; the code table is `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Script bindings"). For `BUFFERED` functions a stale entity and a ragdoll call on an entity without a ragdoll component are not call-time misuse: a `BUFFERED` call reads no world state, so they are dropped at playback and counted. A `READ_WORLD` function reads the world at the call, so SCRIPT's dispatcher raises ZS1101 (stale) or ZS1102 (another world's) for its entity arguments, and `Physics.GetVelocity` of a live entity with no body returns two zero vectors. The C++ query members return false or 0 and count `physics.query.invalid_delta` for invalid arguments; that contract is for C++ callers only. `ZENITH_ASSERT` guards only engine invariants (a pending step not resumed, a node outside its phase, a `ShiftOrigin` precondition the module validated).

### Determinism obligations

The rules D1–D12 of `Zenith2/Docs/Design/PHYS/Simulation.md` → "Determinism rules" and the boundaries of `Zenith2/Docs/Design/PHYS/Simulation.md` → "Determinism boundaries", proved by `Zenith2/Docs/Design/PHYS/Tests.md` → "Determinism replay".

## Diagnostics and profiling

### Counters

| Counter | Kind | Meaning |
|---|---|---|
| `physics.bodies`, `physics.bodies.active`, `physics.frozen_bodies` | gauge | live, awake and region-frozen bodies |
| `physics.bodies.refused` | cumulative | creations refused by capacity |
| `physics.constraints` | gauge | live constraints |
| `physics.contacts_delta`, `physics.contacts.dropped_delta` | delta | contact records kept and dropped this tick |
| `physics.contacts.self_delta` | delta | self-contacts skipped this tick: records between two parts of one ragdoll (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Ragdoll part identity") |
| `physics.contacts.stale_removed_delta` | delta | removal reports of bodies that were no longer their slot's live occupant, dropped by the backend before the sink (`Zenith2/Docs/Design/PHYS/Backends.md` → "Body identity on both backends") |
| `physics.contacts.handover_delta` | delta | pairs ended by removal that another body of the same simulation ID continued in the same emission, with no event (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Event emission" step 3) |
| `physics.contacts.suspended` | gauge | contacts suspended by a sleeping body, over every pair (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Sleeping") |
| `physics.step.substeps`, `physics.step.waves` | gauge | last tick's substeps and PhysX waves |
| `physics.step.failed_delta`, `physics.step.pairs_full_delta`, `physics.step.constraints_full_delta` | delta | backend step failures and cache saturation |
| `physics.queries_delta`, `physics.query.invalid_delta`, `physics.query.ignore_truncated_delta` | delta | query traffic and misuse |
| `physics.query.primitive_uncached_delta`, `physics.query.primitive_create_failed_delta` | delta | queries that used an uncached primitive because every cache entry was pinned, and queries that returned no hit because the primitive could not be created (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Primitive cache") |
| `physics.deferred.completed_delta` | delta | script queries executed and parked |
| `physics.deferred.cancelled_at_detach` | cumulative | INBOX queries completed with the cancelled result at `DetachWorld` (PHYS's own counter; `world.async_discarded` is the world host's alone) |
| `physics.external.pose_delta`, `physics.external.velocity_delta`, `physics.static_moved_delta` | delta | external writes and moved statics |
| `physics.command.dropped_delta`, `physics.command.no_body_delta`, `physics.command.no_world_delta`, `physics.command.stale_delta`, `physics.command.unresolved_kind_delta` | delta | command playback outcomes |
| `physics.sync.parent_moved_delta` | delta | local transforms rewritten because a dynamic body's parent moved |
| `physics.controller.depenetrations_delta`, `physics.controller.invalid_delta`, `physics.controller.adopted_external_delta` | delta | controller events (`invalid_delta` includes controllers refused on an entity with its own body, `Zenith2/Docs/Design/PHYS/Simulation.md` → "Body composition" rule 10) |
| `physics.ragdoll.active`, `physics.ragdoll.enter_refused`, `physics.ragdoll.recover_ignored_delta`, `physics.ragdoll.pose_unavailable_delta` | gauge / cumulative / delta | ragdolls |
| `physics.origin_shifts` | cumulative | rebases applied |
| `physics.origin.late_shift_delta`, `physics.origin.invalid_shift_delta` | delta | defects: a shift applied at ingest instead of its sync point; a shift component above 2^24 m, applied once with one rounding (the conformance suite asserts both stay 0) |
| `physics.shapes`, `physics.shapes.built_from_neutral` | gauge / cumulative | shapes and backend-section fallbacks |
| `physics.jolt.heightfield_max_error_mm` | gauge | the largest measured runtime-versus-neutral height error over the live Jolt heightfield shapes, in millimetres (`Zenith2/Docs/Design/PHYS/Backends.md` → "Heightfield builds"); 0 on PhysX |
| `physics.resident_bytes` | gauge | the memory consumer's value |
| `physics.physx.warnings`, `physics.physx.misuse`, `physics.physx.errors`, `physics.physx.inline_tasks_delta` | cumulative / delta | PhysX callbacks and dispatcher |
| `physics.leaked_handles` | cumulative | handles released by a destructor |
| `physics.save.coherent_fallbacks` | cumulative | EXACT restores that fell back to the coherent path (each also reported to SAVE) |
| `physics.save.restore_shape_failed_delta` | delta | restored bodies whose shape asset failed to load |

### Profiler zones

`Physics.CharacterMove`, `Physics.Step` with child zones `Physics.Ingest`, `Physics.Substep` (one per substep), `Physics.Backend` (inside each substep), `Physics.WriteBack`, `Physics.Contacts`; `Physics.SyncTransforms`, `Physics.RagdollBlend`, `Physics.DeferredQueries`, `Physics.DebugDraw`; `Physics.Load.Shape`, `Physics.Load.Ragdoll`. Z2-B-PHYS-002 is measured over the phase-5 and phase-6 zones.

### On-screen profiler rows

One row group "Physics": step ms (p50/p95), substeps, active bodies, pairs, contacts kept/dropped, queries per tick, resident MiB.

### Log categories

`physics` (module and backends); `physics.cook` (ZenithCook steps). Backend failures log once per distinct code per world with the counter name.

### Debug views

`physics.debug.views` bits (`Zenith_PhysicsDebugView`, `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsDebug header"), exactly the enumeration's eleven views: shapes (colour by motion type and sleep), bounds (world AABBs), contacts (points and normals), triggers (volumes and current overlaps), constraints (frames and limits), controllers (capsule, ground normal, targets, step probe), ragdolls (parts, drive targets, blend weight), queries (the last 256 queries of the debug ring), vehicles (suspension rays, wheel contacts, slips), layers (layer index per body) and frozen (bodies frozen outside the active region). Drawn from neutral data through `Zenith_DebugDraw` by `Physics.DebugDraw`; compiled in every build and active only when the debug-draw recorder exists (never in Shipping).

### Physics dump command

The developer console command `physics.dump [world]` writes `Zenith2/out/<checkout-hash>/dumps/physics_<world>_<tick>.json`: every body (simulation ID, entity, motion type, pose, velocities, sleep, layer, shape kind), constraint, controller and ragdoll state, sorted by simulation ID, and the counters. The canonical state hash of `Zenith2/Docs/Design/PHYS/Tests.md` → "Determinism replay" is printed with it, so two runs can be compared by hash first.

### Telemetry

None beyond the counters; PHYS sends nothing off the device.

## Authoring and cooking

### What PHYS authors

| Asset | Authored as | Cooked by |
|---|---|---|
| collision shapes | `*.collision.json` documents or a mesh's `.meta` `physics` member | `physics.shape`, `physics.collision` |
| terrain collision | ENV's terrain cook writes `ZPHI` intermediates | `physics.heightfield` |
| foliage collision | ENV's foliage cook writes `ZPCI` intermediates | `physics.foliageCompound` |
| physical materials | `*.physmat.json` | `physics.material` |
| ragdolls | `*.ragdoll.json` against a skeleton | `physics.ragdoll` |
| bodies, colliders, joints, controllers, vehicles | components in scenes and prefabs (SCENE's documents) | none (scene data) |

### Spawn clearance rule

<!-- informative -->
The old engine's authored bodies spawned interpenetrating their supports and popped on the first step; Zenith2 checks at authoring time instead.
<!-- /informative -->

The editor's placement tools and the scene validation step (`physics.validateScene`, run by the scene cook) report every dynamic body whose shape, at its authored pose, penetrates a static or another dynamic body by more than `physics.penetrationSlop`, and warn about every dynamic body under an ancestor that carries a dynamic or kinematic body, an animation component or a cinematic binding (`PHYSICS_COOK_DIAG_DYNAMIC_UNDER_MOVING_PARENT`: a dynamic body is simulated in world space and does not follow a moving parent, `Zenith2/Docs/Design/PHYS/Simulation.md` → "External writes"), and warn about every entity whose `Zenith.Physics.CharacterController` is refused because the entity also carries `Zenith.Physics.RigidBody` or its own `Zenith.Physics.Collider` (`PHYSICS_COOK_DIAG_CONTROLLER_WITH_BODY`, `Zenith2/Docs/Design/PHYS/Simulation.md` → "Body composition" rule 10), and warn about every DYNAMIC rigid body whose own collider IsTrigger (`PHYSICS_COOK_DIAG_DYNAMIC_TRIGGER`: every trigger is simulated as a kinematic, discrete, always-awake body that follows its entity, so such a body never falls, `Zenith2/Docs/Design/PHYS/Simulation.md` → "Body composition" rule 9); the scene cook warns (it never moves an authored value), and the editor offers "Drop to rest", which runs a 2-second isolated simulation of the selected bodies and writes the settled poses through an undoable edit. An oriented box keeps its full rotation through this and through save (no shape is reduced to an axis-aligned box: `Test_PhysicsSync_RotatedBoxKeepsRotation[<Backend>]`).

### Editor surfaces

The component inspectors use the reflected metadata (ranges, units, enumerations); the collision, joint and controller gizmos use the debug views' geometry; the material and ragdoll documents use the generic document editor with a ragdoll preview that steps an isolated world on the editor's job system.

#### Isolated simulation for editor tools

<!-- elaboration -->
"Drop to rest" and the ragdoll preview (A3 cross-area row ED34) step bodies outside every ECS world. `Zenith_PhysicsSandbox` is a thin owner of one `Zenith_PhysicsWorld` from the process's `Zenith_Physics` service, a discarding contact sink and the bodies and constraints added to it:

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsSandbox.h
struct Zenith_PhysicsSandboxDesc
{
	Zenith_Physics* m_pxPhysics = nullptr;            // borrowed; outlives the sandbox
	Zenith_PhysicsWorldDesc m_xWorld;                 // capacities, gravity, solver; m_uWorldTag any value 1 … 4,095
	float m_fTickSeconds = 1.0f / 60.0f;              // the fixed step, (0, 0.1]
	Zenith_Vector3 m_xBoundsMin{ -1000.0f, -1000.0f, -1000.0f };   // a body whose origin leaves the box is out of bounds
	Zenith_Vector3 m_xBoundsMax{ 1000.0f, 1000.0f, 1000.0f };
	Zenith_Heap* m_pxHeap = nullptr;
};

class Zenith_PhysicsSandbox
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_PhysicsSandbox>, Zenith_PhysicsError> Create(
		const Zenith_PhysicsSandboxDesc& xDesc);
	Zenith_Result<Zenith_PhysicsBodyHandle, Zenith_PhysicsError> AddBody(const Zenith_PhysicsBodyDesc& xDesc);
	Zenith_Result<Zenith_PhysicsConstraintHandle, Zenith_PhysicsError> AddConstraint(
		const Zenith_PhysicsConstraintDesc& xDesc);
	// One fixed tick: true when it finished, false when a wave is pending (call again from a later job, the world's
	// StepOnce / ResumeStep protocol). No other member may be called while a tick is pending.
	Zenith_Result<bool, Zenith_PhysicsError> StepTick(Zenith_JobContext& xJobs);
	Zenith_Result<Zenith_PhysicsBodyState, Zenith_PhysicsError> GetBodyState(Zenith_PhysicsBodyHandle xBody) const;
	bool IsOutOfBounds(Zenith_PhysicsBodyHandle xBody) const;
	uint64 GetTicksRun() const;
};
```

- Ownership: the caller owns the sandbox. It releases its constraints, bodies and world in that order when destroyed. Shapes and materials are the caller's and must outlive the sandbox.
- Thread affinity: `EXTERNALLY_SYNCHRONISED`; one caller at a time, on any thread, with `StepTick` inside a job of the caller's job system.
- Errors: `Create` maps the world's errors; `AddBody` and `AddConstraint` after the first tick return `PHYSICS_ERROR_INVALID_DESC`; every backend error passes through unchanged. No input asserts.
- The sandbox is not a module world: it has no ECS world, no command kind, no save participant and no event. A body's state is read with `GetBodyState`, and a body is out of bounds once its origin leaves the desc's box at the end of a tick.
- `Test_PhysicsSandbox_BoxSettlesOnFloor` (both backends): a 1 m box 0.5 m above a static floor settles within 1 cm of resting height in 120 ticks. `Test_PhysicsSandbox_FallingBodyOutOfBounds`: a box with no floor is out of bounds within 600 ticks. `Test_PhysicsSandbox_AddAfterStepRefused`: `AddBody` after a tick returns `PHYSICS_ERROR_INVALID_DESC`.


### Cook steps and derived-data keys

| Step | Input | Output | Derived-data key adds |
|---|---|---|---|
| `physics.shape` | collision document (+ referenced mesh output) | `.zcol` | `uPHYSICS_COOK_VERSION` (1), the data tags of the backend sections written |
| `physics.collision` | a mesh source's importer settings | `.zcol` sub-asset `collision` | the same |
| `physics.heightfield` | `ZPHI` (physical-material names resolved to `Physics.Material` assets, `Zenith2/Docs/Design/PHYS/Formats.md` → "Physical-material names") | `.zcol` kind 7 | the same, and each resolved material's key hash (a hard reference) |
| `physics.foliageCompound` | `ZPCI` + hull assets (names resolved as above) | `.zcol` kind 8 | the same |
| `physics.material` | material document | `.zpmat` | `uPHYSICS_COOK_VERSION` |
| `physics.ragdoll` | ragdoll document + skeleton output | `.zragdoll` | the skeleton's hierarchy hash |

Every step is deterministic (no clock, no address, sorted inputs); the convex hull is computed in a fixed order independent of input order.

### Document types and migration

`Zenith.PhysicsMaterialDocument`, `Zenith.RagdollDocument`, `Zenith.CollisionShapeDocument`, each schema version 1, join the authoring document type list of [Z2-N-CONTRACT-552] in PHYS's document ticket (row PH14). Each subsequent schema version adds a migration step with preview, backup, journaled write and unknown-member preservation ([Z2-N-CONTRACT-543]); cooked files re-cook ([Z2-N-CONTRACT-529]); saves migrate forward ("Save section layout").

## Failure handling

### Failure table

| Failure | Detection | Response | Counter |
|---|---|---|---|
| backend factory fails | step 2 of module start | `MODULE_ERROR_BACKEND_CREATE`; the game does not start | log |
| body capacity reached | `CreateBody` | entity has no body; retried on its next change | `physics.bodies.refused` |
| contact sink full | `Append` | lowest keys kept | `physics.contacts.dropped_delta` |
| backend pair or contact cache full | step stats | simulation continues with Jolt's/PhysX's own dropping; the budget row tracks it | `physics.step.pairs_full_delta` |
| step failure | `StepOnce`/`ResumeStep` error | remaining substeps run; defect report | `physics.step.failed_delta` |
| corrupt cooked shape | loader | asset load fails (FAIL policy); the entity has no collider | asset registry |
| physical-material name with no table entry | `physics.heightfield`, `physics.foliageCompound` | the cook item fails (`COOK_ERROR_STEP_FAILED`, `PHYSICS_COOK_DIAG_MATERIAL_NAME_UNRESOLVED` naming the name and the tile or cell) | cook report |
| Jolt heightfield error above 0.01 m | Jolt loader's measurement | the shape is used; one warning naming the asset | `physics.jolt.heightfield_max_error_mm` |
| stale backend section | loader | neutral build | `physics.shapes.built_from_neutral` |
| ragdoll enter refused | enter procedure | stays animated | `physics.ragdoll.enter_refused` |
| invalid script argument | binding | the catalogued Luau error at the call ([Z2-N-CONTRACT-473]); nothing recorded | the script error's own reporting |
| invalid C++ query argument | query member | false/0 | `physics.query.invalid_delta` |
| EXACT restore mismatch (corrupted blob, another data tag) | `RestoreState` | the coherent restore already made stands; reported to SAVE (`ReportCoherentRestore`), so the restored session is treated as COHERENT | `physics.save.coherent_fallbacks` |
| restored shape asset failed | `IsRestoreReady` | the entity is bodiless; reported to SAVE as a coherent restore | `physics.save.restore_shape_failed_delta` |
| world detached with queries in INBOX | `DetachWorld` | each completed with the cancelled result | `physics.deferred.cancelled_at_detach` |
| memory pressure | governor | sheds nothing (Simulation row); `memory.simulation_over_budget` at CRITICAL | `physics.resident_bytes` |

PHYS's row in `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` (CB8) is "none / none": resident bytes are unchanged at WARNING and CRITICAL.

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/PHYS/Tests.md` → "Test targets" and → "Gate steps for PHYS diffs".

### Test catalogue index

| Group | Heading |
|---|---|
| levels, handles, tables, configuration, bindings, components, loaders | `Zenith2/Docs/Design/PHYS/Tests.md` → "Test levels", "Handle and table tests", "Configuration and layer tests", "Binding tests", "Component and asset tests", "Loader tests" |
| ingest, sync, events, triggers, queries | `Zenith2/Docs/Design/PHYS/Tests.md` → "Ingest and sync tests" |
| conformance and determinism | `Zenith2/Docs/Design/PHYS/Tests.md` → "Conformance suite", "Determinism replay" |
| stacking, tunnelling, substeps, large coordinates | `Zenith2/Docs/Design/PHYS/Tests.md` → "Stacking stability tests", "Tunnelling tests", "Substep and hitch tests", "Large-coordinate tests" |
| controller, ragdoll, vehicle | `Zenith2/Docs/Design/PHYS/Tests.md` → "Character controller tests", "Ragdoll tests", "Vehicle and buoyancy tests" |
| save, failure injection, cook | `Zenith2/Docs/Design/PHYS/Tests.md` → "Save and restore tests", "Failure injection tests", "Cook and format tests" |
| integration | `Zenith2/Docs/Design/PHYS/Tests.md` → "Integration suite" |

## Budget rows

### Rows owned

| Row | Name | Unit | Protocol | Platforms | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|
| Z2-B-PHYS-001 | Physics world memory | MB | MP-SUBMEM | Win, S24 | W-Characters, W-Dense, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-002 | Physics step CPU | ms | MP-CPUZONE | Win, S24 | W-Characters, W-Dense, W-OpenWorld, W-Cinematic, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for PHYS |

Z2-B-PHYS-002 is the summed span of `Physics.Step`, `Physics.SyncTransforms`, `Physics.RagdollBlend` and `Physics.DeferredQueries` per tick (excluding `Physics.CharacterMove`, which Z2-B-GAME-001 measures). MP-CPUZONE measures one node's span ("the span from node start to node completion"), so the sum of four is a definition the row itself must state; it travels to Budgets as core-doc item X264 (e) (row PH11), not as an A3 note. The producer's harness measures each backend; the row's value is the selected backend's per platform. **W-Game2D:** the column measures PHYS's four nodes when the 2D game selects the `Physics` module (D-SEL-2DPHYS (open, Stage 2) may choose Jolt through PHYS's backend); if D-SEL-2DPHYS places 2D physics in TWOD's own library, the game selects no `Physics` module, the column measures nothing, and X264 (e) asks Budgets to drop W-Game2D from the row in that ruling's diff.

### Rows measured against

| Row | PHYS's share |
|---|---|
| Z2-B-GAME-001 | `Physics.CharacterMove` inside the phase-4 span |
| Z2-B-ANIM-003 | ragdoll pose reads ANIM performs in phase 6 |
| Z2-B-SCRIPT-026 | deferred physics queries in flight |
| Z2-B-JOBS-002 | ticks per frame under catch-up, which bounds steps per frame (× 8) |
| Z2-B-GAME-003 | contact, trigger and interaction events per tick |

### Pending row needs

Requested by descriptive name (row PH11); each is a configuration default until allocated:

| Need | Default | Unit |
|---|---|---|
| Physics body capacity per world | 65,536 | bodies |
| Physics constraint capacity per world | 16,384 | constraints |
| Physics contact records per tick | 8,192 | records |
| Active ragdolls per world | 32 | ragdolls |

## Stage plan

### Stage table

| Stage | PHYS work |
|---|---|
| S1 | the third-party pinning ticket (`needs-human`: the `jolt` and `physx` manifest entries with their digests, and the audit of PhysX's in-task waits); the D-SEL-PHYSX-ANDROID harness `Zenith_Bench_PhysicsBackends` (a loop ticket: the harness, including the benchmark-only `JPH::JobSystemThreadPool` measurement, and its registered `--smoke` CTest); the harness run on Windows and the S24 with its report, the D-SEL-PHYSX-ANDROID ruling and the PHYS-1 ruling applying the trigger of `Zenith2/Docs/Design/PHYS/Backends.md` → "Conditional Jolt parallel step" (`needs-human`); the PHYS per-system budget harness (a loop ticket with its `--smoke` CTest) and the Stage 1 values of Z2-B-PHYS-001/-002 (`needs-human`) |
| S2 | the API-only slice; the D-CLASS-SIM ruling ticket classifying cloth, soft bodies and destruction (Z2-I-PHYS-17, -18, -19) with an owner and candidate each |
| S3 | the module, both backends, conformance, determinism, cooking, layers, queries, triggers, events, materials, interactions, controller, constraints, joints, ragdolls, CCD, substeps, platforms, debug views, save |
| S4 | vehicles, buoyancy and `Zenith_FluidQuery`, streaming coordination (cell bodies, region freeze), origin shift and large coordinates |
| S5 | the specialist simulations D-CLASS-SIM classifies as required (Z2-I-PHYS-20), each its own design addendum in this family |
| S6 | qualification: budget rows measured on the S24 for the selected Android backend, the full conformance and determinism suites on both platforms |

### API-only slices

Stage 2 lands `Zenith_PhysicsTypes.h`, `Zenith_PhysicsErrors.h`, `Zenith_Physics.h`, `Zenith_PhysicsShape.h`, `Zenith_PhysicsMaterial.h`, `Zenith_PhysicsWorld.h` and `Zenith_PhysicsQuery.h` with their contract blocks and header-only tests (handle codec, collector, contact sink, combine function), so ANIM, GAME and AI can compile against the API before the backends exist.

### Ticket slices and ordering

1. S1: pinning ticket (`needs-human`) → harness ticket (loop; blocked by the pin) → the harness run and the D-SEL-PHYSX-ANDROID and PHYS-1 rulings (`needs-human`, one report); budget harness (loop) → budget-value ticket (`needs-human`). Recorded run evidence lives only in the `needs-human` tickets (the orchestrator's WORKER-EVIDENCE ruling); the loop tickets' proof is their registered smoke CTests.
2. S2: API-only slice; D-CLASS-SIM ruling (human).
3. S3: `ZenithPhysics` target and module entry points (EnB9, MB17) → Jolt backend → PhysX backend → conformance suite skeleton → ingest/step/sync → events and triggers → queries and `Zenith_CollisionQuery` → materials and interactions → constraints and joints → controller (after ANIM's RootMotion component exists) → ragdolls (with ANIM's ragdoll ticket) → collision cook steps (after ENV's intermediates writer) → save participant (after SAVE's participant API) → debug views.
4. S4: vehicles → buoyancy and `Zenith_FluidQuery` (with ENV) → streaming coordination (with STREAM) → origin shift and large-coordinate tests.

## Decisions

### Settled decisions

| Decision | What it settles for PHYS |
|---|---|
| Z2-D-003 | one neutral `Zenith_Physics`, Jolt and PhysX, conformance, per-backend determinism, Stage 3 |
| Z2-D-019 | determinism per backend, never across backends |
| Z2-D-053 | deferred physics query results become visible at a tick boundary in completion-ID order |
| Z2-D-080 | the frame phases PHYS places its nodes in |
| Z2-D-082 | the third-party manifest PHYS pins Jolt and PhysX through |
| Z2-D-024 | modules and backends as libraries |
| Z2-D-006 | the CPU frame is a DAG with no fibers, so no job may wait for another: the Jolt step is single-threaded, the PhysX step uses drain waves and continuations |
| Z2-D-004 | compressed cooked sections |
| Z2-D-038 | JSON authoring documents, versioned binary cooked files, no C++ layout on disk |
| Z2-D-045 | `Zenith_Result` for recoverable errors, asserts for programmer errors only |
| Z2-D-084 | the PhysX-on-Android selection is a harness ticket plus a human ruling ticket |

### Open decisions

| Decision | Stage | PHYS's part |
|---|---|---|
| D-SEL-PHYSX-ANDROID | S1 | the harness and its report; the ruling ticket's manifest and `ZENITH_SUPPORTED` edits |
| PHYS-1 (the Q87 conditional decision) | S1 | the thread-pool measurement in the same harness report; the ruling ticket applies the trigger ("Conditional decision PHYS-1") |
| D-CLASS-SIM | S2 | the ruling ticket: cloth, soft bodies, destruction each required, deferred or excluded with an owner and a candidate (Jolt soft bodies, a custom solver); PHYS owns any capability classified as a physics feature |
| D-SEL-2DPHYS | S2 | TWOD's; PHYS supplies Jolt-in-2D measurements if TWOD's harness asks |
| D-PHASES (refinement) | S1 | GAME's; PHYS's census rows feed it |

### Flagged proposals

PHYS-1 (`findings/FLAGGED_PROPOSALS.md`): a bracketed help-while-wait exemption to the no-blocking rule for a third-party job-system adapter (Jolt's `JobSystem::WaitForJobs`), so the Jolt step could run multithreaded.

### Conditional decision PHYS-1

The orchestrator's ruling on Q87 (2026-10-09; the human may override): decide on evidence. PHYS keeps single-threaded Jolt as the compliant baseline step; the D-SEL-PHYSX-ANDROID harness adds a benchmark-only multithreaded Jolt measurement on Windows and the S24. Trigger, both outcomes and the full pre-specification of the admitted exemption (a `Zenith_JoltJobSystem` derived from `JPH::JobSystem` with its own barrier, Jolt jobs drained by sub-jobs of the step node, Jolt-internal only, no lock held across a wait and no blocking primitive, reported to the blocking-wait hook, stress and determinism tests at worker counts 1, 2, 8 and all, a recorded decision, one Contracts bullet beside [Z2-N-CONTRACT-365] and lock rows 520 and 530, the second synchronising the contact sink under the concurrent contact callbacks) are `Zenith2/Docs/Design/PHYS/Backends.md` → "Conditional Jolt parallel step"; requirements Z2-R-PHYS-132 and Z2-R-PHYS-133.

## Requirement index

The catalogue is `Zenith2/Docs/Requirements/PHYS.md`; each requirement and the PHYS headings that specify it:

| Requirement | Title | Stage | Design |
|---|---|---|---|
| Z2-R-PHYS-001 | PhysX on Android selection harness | S1 | PHYS/Backends.md → "PhysX on Android evaluation"; PHYS.md → "Open decisions" |
| Z2-R-PHYS-002 | PhysX Android ruling applied | S1 | PHYS/Backends.md → "Third-party pins"; PHYS.md → "ZENITH_SUPPORTED matrix" |
| Z2-R-PHYS-003 | Jolt and PhysX manifest pins | S1 | PHYS/Backends.md → "Third-party pins"; PHYS/Backends.md → "Build options" |
| Z2-R-PHYS-004 | PHYS per-system budget measurement | S1 | PHYS.md → "Rows owned"; PHYS.md → "Pending row needs"; PHYS/Tests.md → "Benchmarks and budget evidence" |
| Z2-R-PHYS-005 | Cloth classification | S2 | PHYS.md → "Open decisions" |
| Z2-R-PHYS-006 | Soft-body classification | S2 | PHYS.md → "Open decisions" |
| Z2-R-PHYS-007 | Destruction classification | S2 | PHYS.md → "Open decisions" |
| Z2-R-PHYS-008 | API-only slice | S2 | PHYS.md → "API-only slices"; PHYS/PublicAPI.md → "Header list" |
| Z2-R-PHYS-009 | Module library targets | S3 | PHYS.md → "Target summary"; PHYS.md → "ZenithPhysics files"; PHYS.md → "Backend library files" |
| Z2-R-PHYS-010 | Module entry points | S3 | PHYS.md → "Module start and world attach"; PHYS/PublicAPI.md → "Zenith_Module_Physics header" |
| Z2-R-PHYS-011 | Module construction unwinds | S3 | PHYS.md → "Module start and world attach"; PHYS/Tests.md → "Failure injection tests" |
| Z2-R-PHYS-012 | Neutral physics interface | S3 | PHYS/PublicAPI.md → "Zenith_Physics header"; PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"; PHYS.md → "Rules across the PHYS API" |
| Z2-R-PHYS-013 | Physics error codes | S3 | PHYS.md → "Error codes"; PHYS/PublicAPI.md → "Zenith_PhysicsErrors header" |
| Z2-R-PHYS-014 | Generational physics handles | S3 | PHYS/PublicAPI.md → "Zenith_PhysicsTypes header"; PHYS/Tests.md → "Handle and table tests" |
| Z2-R-PHYS-015 | Capacities refuse without side effects | S3 | PHYS.md → "Failure table"; PHYS/Simulation.md → "Body lifecycle state machine" |
| Z2-R-PHYS-016 | Backend factories and one instance | S3 | PHYS/Backends.md → "Jolt construction steps"; PHYS/Backends.md → "PhysX construction steps" |
| Z2-R-PHYS-017 | Jolt backend | S3 | PHYS/Backends.md → "Jolt world construction"; PHYS/Backends.md → "Jolt layer mapping"; PHYS/Backends.md → "Jolt contact listener"; PHYS/Backends.md → "Jolt queries" |
| Z2-R-PHYS-018 | Jolt step never blocks | S3 | PHYS/Backends.md → "Jolt step and the job system"; PHYS/Backends.md → "Conditional Jolt parallel step" |
| Z2-R-PHYS-019 | PhysX backend | S3 | PHYS/Backends.md → "PhysX world construction"; PHYS/Backends.md → "PhysX filter shader"; PHYS/Backends.md → "PhysX contact reporting"; PHYS/Backends.md → "PhysX error callback" |
| Z2-R-PHYS-020 | PhysX step in drain waves | S3 | PHYS/Backends.md → "PhysX step and the dispatcher"; PHYS/PublicAPI.md → "Zenith_PhysicsWorld header" |
| Z2-R-PHYS-021 | Third-party allocation routed to the root heap | S3 | PHYS/Backends.md → "Allocation and third-party global hooks" |
| Z2-R-PHYS-022 | Resident byte accounting | S3 | PHYS/Backends.md → "Resident byte accounting"; PHYS/Simulation.md → "Memory consumer" |
| Z2-R-PHYS-023 | Memory pressure sheds nothing | S3 | PHYS/Simulation.md → "Memory consumer"; PHYS.md → "Failure table" |
| Z2-R-PHYS-024 | Backend conformance suite | S3 | PHYS/Tests.md → "Conformance suite"; PHYS/Tests.md → "Scenario definitions"; PHYS/Backends.md → "Backend behaviour differences"; PHYS/Tests.md → "Test levels" |
| Z2-R-PHYS-025 | Capability matrix | S3 | PHYS.md → "Capability matrix" |
| Z2-R-PHYS-026 | Per-backend determinism across workers and layout | S3 | PHYS/Tests.md → "Determinism replay"; PHYS/Simulation.md → "Determinism rules"; PHYS/Tests.md → "Test levels" |
| Z2-R-PHYS-027 | Determinism with script failures | S3 | PHYS/Tests.md → "Determinism replay"; PHYS/Simulation.md → "Determinism rules" |
| Z2-R-PHYS-028 | Determinism rules in code | S3 | PHYS/Simulation.md → "Determinism rules"; PHYS/Backends.md → "Build options" |
| Z2-R-PHYS-029 | Determinism boundaries stated | S3 | PHYS/Simulation.md → "Determinism boundaries" |
| Z2-R-PHYS-030 | Explicit substep loop | S3 | PHYS/Simulation.md → "Substep loop" |
| Z2-R-PHYS-031 | Substep cap and hitch behaviour | S3 | PHYS/Simulation.md → "Substep loop"; PHYS/Tests.md → "Substep and hitch tests"; PHYS.md → "Limits and constants" |
| Z2-R-PHYS-032 | Physics nodes and phases | S3 | PHYS/Simulation.md → "Frame-graph placement"; PHYS/ComponentsAPI.md → "Authority census rows" |
| Z2-R-PHYS-033 | Ingest of components into bodies | S3 | PHYS/Simulation.md → "Ingest algorithm"; PHYS/Simulation.md → "Body composition" |
| Z2-R-PHYS-034 | External writes | S3 | PHYS/Simulation.md → "External writes" |
| Z2-R-PHYS-035 | Pending body operations | S3 | PHYS/Simulation.md → "Pending operations"; PHYS.md → "Command kinds" |
| Z2-R-PHYS-036 | Command kinds and recording functions | S3 | PHYS.md → "Command kinds"; PHYS/ComponentsAPI.md → "Zenith_PhysicsCommands header" |
| Z2-R-PHYS-037 | Write-back to components | S3 | PHYS/Simulation.md → "Write-back" |
| Z2-R-PHYS-038 | Physics-to-transform sync | S3 | PHYS/Simulation.md → "Physics-to-transform sync" |
| Z2-R-PHYS-039 | Sleeping | S3 | PHYS/Simulation.md → "Sleeping"; PHYS/Simulation.md → "Event emission" |
| Z2-R-PHYS-040 | Body lifecycle | S3 | PHYS/Simulation.md → "Body lifecycle state machine"; PHYS/Simulation.md → "Module world state"; PHYS/Simulation.md → "Event emission" |
| Z2-R-PHYS-041 | Lifecycle listener | S3 | PHYS.md → "Lifecycle listener"; PHYS/CharacterRagdollVehicle.md → "Teleport during a ragdoll" |
| Z2-R-PHYS-042 | Configuration keys | S3 | PHYS/ComponentsAPI.md → "Configuration keys" |
| Z2-R-PHYS-043 | Authority census rows | S3 | PHYS/ComponentsAPI.md → "Authority census rows" |
| Z2-R-PHYS-044 | Physics components | S3 | PHYS/ComponentsAPI.md → "Zenith_PhysicsComponents header"; PHYS/ComponentsAPI.md → "Rigid body properties"; PHYS/ComponentsAPI.md → "Collider properties"; PHYS/Tests.md → "Component and asset tests" |
| Z2-R-PHYS-045 | Collision shape asset kind | S3 | PHYS/Formats.md → "Collision shape file"; PHYS/PublicAPI.md → "Zenith_CollisionShapeAsset header"; PHYS.md → "Asset kinds"; PHYS/Tests.md → "Loader tests" |
| Z2-R-PHYS-046 | Neutral geometry is authoritative | S3 | PHYS/Formats.md → "Neutral geometry section"; PHYS/Formats.md → "Backend sections"; PHYS/Backends.md → "Backend data tag" |
| Z2-R-PHYS-047 | Backend shape payloads | S3 | PHYS/Backends.md → "Backend shape payloads" |
| Z2-R-PHYS-048 | Collision shape documents and importer settings | S3 | PHYS/Formats.md → "Collision shape document"; PHYS/Formats.md → "Collision importer settings"; PHYS.md → "Cook steps and derived-data keys" |
| Z2-R-PHYS-049 | Terrain heightfield collision | S3 | PHYS/Formats.md → "Heightfield intermediate"; PHYS/Formats.md → "Neutral geometry section" |
| Z2-R-PHYS-050 | Foliage compound collision | S4 | PHYS/Formats.md → "Compound intermediate" |
| Z2-R-PHYS-051 | Cooked parser limits | S3 | PHYS/Formats.md → "Parser limits"; PHYS/Formats.md → "Common rules" |
| Z2-R-PHYS-052 | Physics document types and migration | S3 | PHYS.md → "Document types and migration"; PHYS/Formats.md → "Material document" |
| Z2-R-PHYS-053 | Layer configuration | S3 | PHYS.md → "Layer configuration" |
| Z2-R-PHYS-054 | Layer filtering on both backends | S3 | PHYS/Backends.md → "Jolt layer mapping"; PHYS/Backends.md → "PhysX filter shader"; PHYS/Backends.md → "PhysX scene query filter"; PHYS/Simulation.md → "Query execution" |
| Z2-R-PHYS-055 | Script layer names | S3 | PHYS.md → "Layer configuration"; PHYS/ComponentsAPI.md → "Script bindings" |
| Z2-R-PHYS-056 | Synchronous queries | S3 | PHYS/Simulation.md → "Query execution"; PHYS/ComponentsAPI.md → "Zenith_PhysicsWorldResource header" |
| Z2-R-PHYS-057 | Query determinism | S3 | PHYS/PublicAPI.md → "Zenith_PhysicsQuery header"; PHYS/Simulation.md → "Query execution" |
| Z2-R-PHYS-058 | Collision query for cameras | S3 | PHYS/ComponentsAPI.md → "Zenith_CollisionQuery"; PHYS/Simulation.md → "Query execution" |
| Z2-R-PHYS-059 | Primitive cache | S3 | PHYS/Simulation.md → "Query execution"; PHYS.md → "Lock-order entries"; PHYS/Simulation.md → "Primitive cache" |
| Z2-R-PHYS-060 | Deferred script queries | S3 | PHYS/Simulation.md → "Deferred queries"; PHYS/Formats.md → "Deferred query payloads" |
| Z2-R-PHYS-061 | Deferred query cancellation and save | S3 | PHYS/Simulation.md → "Deferred query state machine"; PHYS/Simulation.md → "Deferred queries"; PHYS/Simulation.md → "Save and restore algorithm" |
| Z2-R-PHYS-062 | Trigger events | S3 | PHYS/Simulation.md → "Trigger semantics"; PHYS/Simulation.md → "Body composition"; PHYS/ComponentsAPI.md → "Zenith_PhysicsEvents header" |
| Z2-R-PHYS-063 | Trigger lifetime semantics | S3 | PHYS/Simulation.md → "Trigger semantics" |
| Z2-R-PHYS-064 | Contact events | S3 | PHYS/Simulation.md → "Event emission"; PHYS/Simulation.md → "Sleeping"; PHYS/ComponentsAPI.md → "Zenith_PhysicsEvents header" |
| Z2-R-PHYS-065 | Contact ordering and capacity | S3 | PHYS/PublicAPI.md → "Zenith_PhysicsWorld header"; PHYS/Simulation.md → "Event emission" |
| Z2-R-PHYS-066 | Event emission order | S3 | PHYS/Simulation.md → "Event emission" |
| Z2-R-PHYS-067 | Physical material asset | S3 | PHYS/Formats.md → "Material file"; PHYS/PublicAPI.md → "Zenith_PhysicsMaterial header"; PHYS.md → "Asset kinds"; PHYS/Tests.md → "Loader tests" |
| Z2-R-PHYS-068 | Material combination | S3 | PHYS/Backends.md → "Material combination" |
| Z2-R-PHYS-069 | Material documents | S3 | PHYS/Formats.md → "Material document"; PHYS/Tests.md → "Cook and format tests" |
| Z2-R-PHYS-070 | Physical interaction events | S3 | PHYS/Simulation.md → "Interaction detection" |
| Z2-R-PHYS-071 | Interaction cooldown | S3 | PHYS/Simulation.md → "Interaction detection" |
| Z2-R-PHYS-072 | Continuous collision detection | S3 | PHYS/Simulation.md → "Continuous collision detection"; PHYS/Backends.md → "Motion quality" |
| Z2-R-PHYS-073 | Speculative contacts for discrete bodies | S3 | PHYS/Backends.md → "Motion quality"; PHYS/Tests.md → "Tunnelling tests" |
| Z2-R-PHYS-074 | Linear speed clamp | S3 | PHYS/Simulation.md → "Continuous collision detection" |
| Z2-R-PHYS-075 | Kinematic bodies | S3 | PHYS/Simulation.md → "Kinematic bodies and moving platforms" |
| Z2-R-PHYS-076 | Moving platforms | S3 | PHYS/Simulation.md → "Kinematic bodies and moving platforms"; PHYS/CharacterRagdollVehicle.md → "Character move algorithm" |
| Z2-R-PHYS-077 | Stacking stability | S3 | PHYS/Tests.md → "Stacking stability tests"; PHYS.md → "Spawn clearance rule" |
| Z2-R-PHYS-078 | Character controller component | S3 | PHYS/ComponentsAPI.md → "Zenith_CharacterController header"; PHYS/ComponentsAPI.md → "Authority census rows" |
| Z2-R-PHYS-079 | Character move algorithm | S3 | PHYS/CharacterRagdollVehicle.md → "Character move algorithm"; PHYS/CharacterRagdollVehicle.md → "Controller tuning rules" |
| Z2-R-PHYS-080 | Root motion through the controller | S3 | PHYS/Simulation.md → "Frame-graph placement"; PHYS/CharacterRagdollVehicle.md → "Character move algorithm" |
| Z2-R-PHYS-081 | Controller interaction with bodies | S3 | PHYS/CharacterRagdollVehicle.md → "Controller interaction with bodies" |
| Z2-R-PHYS-082 | Controller teleport | S3 | PHYS/CharacterRagdollVehicle.md → "Character move algorithm"; PHYS.md → "Lifecycle listener" |
| Z2-R-PHYS-083 | Constraint kinds | S3 | PHYS/Simulation.md → "Constraint kinds"; PHYS/Backends.md → "Constraint mapping" |
| Z2-R-PHYS-084 | Constraint ordering | S3 | PHYS/Backends.md → "Constraint mapping"; PHYS/Simulation.md → "Constraint state machine" |
| Z2-R-PHYS-085 | Joint component | S3 | PHYS/ComponentsAPI.md → "Joint properties"; PHYS/Simulation.md → "Constraint state machine"; PHYS/Tests.md → "Component and asset tests" |
| Z2-R-PHYS-086 | Joint motors | S3 | PHYS/ComponentsAPI.md → "Joint properties"; PHYS/Simulation.md → "Constraint kinds" |
| Z2-R-PHYS-087 | Joint breakage | S3 | PHYS/Simulation.md → "Joint breakage" |
| Z2-R-PHYS-088 | Ragdoll asset | S3 | PHYS/Formats.md → "Ragdoll file"; PHYS/Formats.md → "Ragdoll document"; PHYS/CharacterRagdollVehicle.md → "Ragdoll definition" |
| Z2-R-PHYS-089 | Ragdoll state machine | S3 | PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine" |
| Z2-R-PHYS-090 | Ragdoll enter | S3 | PHYS/CharacterRagdollVehicle.md → "Ragdoll enter procedure" |
| Z2-R-PHYS-091 | Ragdoll blend and root | S3 | PHYS/CharacterRagdollVehicle.md → "Ragdoll blend and root"; PHYS/Simulation.md → "Frame-graph placement" |
| Z2-R-PHYS-092 | Ragdoll recover | S3 | PHYS/CharacterRagdollVehicle.md → "Ragdoll recover procedure"; PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine" |
| Z2-R-PHYS-093 | Powered ragdolls | S3 | PHYS/CharacterRagdollVehicle.md → "Powered ragdolls" |
| Z2-R-PHYS-094 | Ragdoll self-collision | S3 | PHYS/CharacterRagdollVehicle.md → "Ragdoll definition"; PHYS/Backends.md → "Constraint mapping"; PHYS/Backends.md → "Jolt layer mapping" |
| Z2-R-PHYS-095 | Ragdoll pose source and body poses | S3 | PHYS/ComponentsAPI.md → "Zenith_Ragdoll header"; PHYS/ComponentsAPI.md → "Zenith_PhysicsWorldResource header" |
| Z2-R-PHYS-096 | Teleport during a ragdoll | S3 | PHYS/CharacterRagdollVehicle.md → "Teleport during a ragdoll" |
| Z2-R-PHYS-097 | Physics debug views | S3 | PHYS.md → "Debug views"; PHYS/ComponentsAPI.md → "Zenith_PhysicsDebug header" |
| Z2-R-PHYS-098 | Physics dump | S3 | PHYS.md → "Physics dump command" |
| Z2-R-PHYS-099 | Counters and zones | S3 | PHYS.md → "Counters"; PHYS.md → "Profiler zones" |
| Z2-R-PHYS-100 | Physics save participant | S3 | PHYS/Simulation.md → "Save and restore algorithm"; PHYS/Backends.md → "Jolt state save and restore"; PHYS/Backends.md → "PhysX state save and restore" |
| Z2-R-PHYS-101 | Save section format | S3 | PHYS/Formats.md → "Save section layout" |
| Z2-R-PHYS-102 | Script bindings | S3 | PHYS/ComponentsAPI.md → "Script bindings" |
| Z2-R-PHYS-103 | Physics integration suite | S3 | PHYS/Tests.md → "Integration suite" |
| Z2-R-PHYS-104 | Vehicles | S4 | PHYS/CharacterRagdollVehicle.md → "Vehicle algorithm"; PHYS/ComponentsAPI.md → "Zenith_PhysicsVehicle header" |
| Z2-R-PHYS-105 | Buoyancy | S4 | PHYS/Simulation.md → "Buoyancy algorithm"; PHYS/Formats.md → "Buoyancy samples"; PHYS/Simulation.md → "Module world state"; PHYS/ComponentsAPI.md → "Buoyancy properties" |
| Z2-R-PHYS-106 | Fluid query interface | S4 | PHYS/ComponentsAPI.md → "Zenith_FluidQuery"; SCENE/ModuleSeams.md → "Zenith_FluidQuery header" |
| Z2-R-PHYS-107 | Streaming cell bodies | S4 | PHYS/Simulation.md → "Cell bodies" |
| Z2-R-PHYS-108 | Region freeze | S4 | PHYS/Simulation.md → "Region freeze" |
| Z2-R-PHYS-109 | Origin shift | S4 | PHYS/Simulation.md → "Origin shift"; PHYS/Backends.md → "Origin shift on Jolt"; PHYS/Simulation.md → "Module world state" |
| Z2-R-PHYS-110 | Large coordinates | S4 | PHYS/Simulation.md → "Large coordinates"; PHYS/Tests.md → "Large-coordinate tests" |
| Z2-R-PHYS-111 | Specialist simulations per classification | S5 | PHYS.md → "Stage table"; PHYS.md → "Open decisions" |
| Z2-R-PHYS-112 | Physics budget qualification | S6 | PHYS.md → "Rows owned"; PHYS/Tests.md → "Benchmarks and budget evidence" |
| Z2-R-PHYS-113 | Cooked reader fuzzing | S6 | PHYS/Tests.md → "Cook and format tests" |
| Z2-R-PHYS-114 | Connected-body collision on both backends | S3 | PHYS/Backends.md → "Constraint mapping"; PHYS/Backends.md → "Jolt layer mapping"; PHYS/Simulation.md → "Ingest algorithm"; PHYS/Simulation.md → "Excluded pairs maintenance" |
| Z2-R-PHYS-115 | Collision groups on both backends | S3 | PHYS/Backends.md → "Jolt layer mapping"; PHYS/Backends.md → "PhysX filter shader"; PHYS/Backends.md → "PhysX scene query filter" |
| Z2-R-PHYS-116 | Query-only bodies identical on both backends | S3 | PHYS/Backends.md → "Jolt layer mapping"; PHYS/Backends.md → "PhysX scene query filter"; PHYS/PublicAPI.md → "Zenith_PhysicsQuery header" |
| Z2-R-PHYS-117 | Trigger bodies kinematic and awake | S3 | PHYS/Simulation.md → "Body composition"; PHYS/Simulation.md → "Trigger semantics"; PHYS/Backends.md → "Jolt layer mapping"; PHYS.md → "Spawn clearance rule" |
| Z2-R-PHYS-118 | Dynamic bodies under moving parents | S3 | PHYS/Simulation.md → "External writes"; PHYS/Simulation.md → "Physics-to-transform sync"; PHYS.md → "Spawn clearance rule" |
| Z2-R-PHYS-119 | Parented controllers adopt in local space | S3 | PHYS/CharacterRagdollVehicle.md → "Character move algorithm"; PHYS/Simulation.md → "External writes" |
| Z2-R-PHYS-120 | Origin shift at the rebase sync point | S4 | PHYS/Simulation.md → "Origin shift"; PHYS/Simulation.md → "Ingest algorithm"; PHYS.md → "Lifecycle listener" |
| Z2-R-PHYS-121 | World-anchored joints follow their entity | S4 | PHYS/ComponentsAPI.md → "Joint properties"; PHYS/Backends.md → "Origin shift on Jolt"; PHYS/Simulation.md → "Ingest algorithm" |
| Z2-R-PHYS-122 | Deterministic slot allocation | S3 | PHYS/Simulation.md → "Slot allocation"; PHYS/Simulation.md → "Determinism rules" |
| Z2-R-PHYS-123 | Slot tables saved and restored | S3 | PHYS/Simulation.md → "Slot allocation"; PHYS/Formats.md → "Save section layout"; PHYS/PublicAPI.md → "Zenith_PhysicsWorld header" |
| Z2-R-PHYS-124 | Jolt bodies created from neutral IDs | S3 | PHYS/Backends.md → "Jolt state save and restore"; PHYS/Simulation.md → "Slot allocation" |
| Z2-R-PHYS-125 | EXACT continuation through creations after a load | S3 | PHYS/Backends.md → "Jolt state save and restore"; PHYS/Simulation.md → "Determinism boundaries" |
| Z2-R-PHYS-126 | Restore waits for resident shapes | S3 | PHYS/Simulation.md → "Save and restore algorithm" |
| Z2-R-PHYS-127 | Restore fallback reported to SAVE | S3 | PHYS/Simulation.md → "Save and restore algorithm"; PHYS.md → "Failure table" |
| Z2-R-PHYS-128 | Command playback finds its world by tag | S3 | PHYS/Simulation.md → "Module world state"; PHYS.md → "Command kinds" |
| Z2-R-PHYS-129 | Physics misuse codes | S3 | PHYS/ComponentsAPI.md → "Script bindings"; PHYS.md → "Luau errors and asserts" |
| Z2-R-PHYS-130 | Heightfield runtime fidelity per backend | S3 | PHYS/Backends.md → "Heightfield builds"; PHYS/Formats.md → "Neutral geometry section"; PHYS/Backends.md → "Jolt contact listener" |
| Z2-R-PHYS-131 | Square heightfields with a 127-entry palette | S3 | PHYS/Formats.md → "Neutral geometry section"; PHYS/Formats.md → "Heightfield intermediate"; PHYS/Formats.md → "Parser limits" |
| Z2-R-PHYS-132 | Multithreaded Jolt benchmark measurement | S1 | PHYS/Backends.md → "PhysX on Android evaluation"; PHYS/Backends.md → "Conditional Jolt parallel step" |
| Z2-R-PHYS-133 | Conditional Jolt parallel step | S1 | PHYS/Backends.md → "Conditional Jolt parallel step"; PHYS.md → "Conditional decision PHYS-1" |
| Z2-R-PHYS-134 | PhysX dispatcher progress at every worker count | S3 | PHYS/Backends.md → "PhysX step and the dispatcher" |
| Z2-R-PHYS-135 | Concurrent primitive cache under stress | S3 | PHYS/Tests.md → "Gate steps for PHYS diffs"; PHYS/Simulation.md → "Query execution"; PHYS/Simulation.md → "Primitive cache" |
| Z2-R-PHYS-136 | Excluded pairs follow every destruction path | S3 | PHYS/Simulation.md → "Excluded pairs maintenance"; PHYS/Simulation.md → "Ingest algorithm"; PHYS/PublicAPI.md → "Zenith_PhysicsWorld header" |
| Z2-R-PHYS-137 | Ragdoll part identity | S3 | PHYS/Simulation.md → "Ragdoll part identity"; PHYS/Simulation.md → "Event emission"; PHYS/Backends.md → "Body identity on both backends"; PHYS/CharacterRagdollVehicle.md → "Ragdoll definition"; PHYS/Simulation.md → "Body composition"; PHYS/CharacterRagdollVehicle.md → "Controller tuning rules" |
| Z2-R-PHYS-138 | Ragdolls saved and restored mid-fall | S3 | PHYS/Simulation.md → "Save and restore algorithm"; PHYS/Formats.md → "Save section layout" |
| Z2-R-PHYS-139 | Module-level cases run per backend | S3 | PHYS/Tests.md → "Test levels"; PHYS/Tests.md → "Test targets"; PHYS/Tests.md → "Test support" |
| Z2-R-PHYS-140 | Frame cancellation never splits a step or a park | S3 | PHYS.md → "Cancellation"; PHYS/Simulation.md → "Deferred queries"; PHYS/Simulation.md → "Deferred query state machine" |
| Z2-R-PHYS-141 | Physical-material names | S3 | PHYS/Formats.md → "Physical-material names"; PHYS/Formats.md → "Heightfield intermediate"; PHYS/Formats.md → "Compound intermediate"; PHYS/PublicAPI.md → "Zenith_PhysicsMaterial header" |
| Z2-R-PHYS-142 | Destroyed-body contact records and pair handover | S3 | PHYS/Backends.md → "Jolt contact listener"; PHYS/Backends.md → "Body identity on both backends"; PHYS/Backends.md → "PhysX contact reporting"; PHYS/Simulation.md → "Event emission" |
| Z2-R-PHYS-143 | Pairs persist while a body sleeps | S3 | PHYS/Simulation.md → "Sleeping"; PHYS/Simulation.md → "Event emission"; PHYS/Backends.md → "Jolt contact listener"; PHYS/Backends.md → "Backend behaviour differences" |
| Z2-R-PHYS-144 | Ragdolls freeze with their region | S4 | PHYS/Simulation.md → "Region freeze"; PHYS/CharacterRagdollVehicle.md → "Ragdoll state machine"; PHYS/Formats.md → "Save section layout" |

## Reconciliation items

Core-document changes travel through `reviews/CROSS_DOC_ITEMS.md`, not A3 notes (the core-doc freeze rule): ZenithSave's edge is X204, the get-up clip type X210 (3), and every other PHYS core-doc item is **X264** (a)–(i) as filed by PHYS round 1, extended by PHYS revision 1 with (j) `LIFECYCLE_EVENT_ORIGIN_SHIFTED` in Architecture's listener enumeration, (k) PHYS's misuse-code block ZS2300–ZS2399, (l) the resolution of the factory branch (PHYS conforms to CompositionRoot's one-parameter `Zenith_PfnCreatePhysics`; nothing changes there) and (m) the Contracts bullet beside [Z2-N-CONTRACT-365] that the PHYS-1 ruling ticket adds only if the exemption is admitted; by PHYS revision 2 with, in (m), the conditional lock row 520 `uLOCK_LEVEL_PHYSICS_JOLT_JOBS` of the job system that exemption builds; and by the PHYS post-round pass with (k) restated on the orchestrator's MISUSE-BLOCKS-2 ruling, which assigns ZS2300–ZS2399 to PHYS (revision 2's fallback clause is withdrawn), and, in (m), the conditional lock row 530 `uLOCK_LEVEL_PHYSICS_CONTACT_SINK`. X238 is applied (`GetWorldTag`); X255 and X265 are conformed to ("Deferred query state machine"). The cross-area rows PHYS appended (prefix PH) to `findings/A3_cross_area_notes.md`:

| Row | To | Item |
|---|---|---|
| PH1 | ARCH | ZenithPhysics `DEPENDS` gains ZenithAssets, ZenithSave, ZenithPlatform |
| PH2 | GAME, ANIM, SCENE, STREAM, ENV, SAVE | confirmations of GA3, GA15, AN6, AN2, S1, S2, ST8, EN4, SV17 by their own names |
| PH3 | CORE (lock table) | rows 500 and 510 of "Lock-order entries" |
| PH4 | ASSET | kinds `Physics.Material` and `Physics.Ragdoll` |
| PH5 | ARCH, ASSET | ZenithCook depends on ZenithPhysics and both backends |
| PH6 | ARCH | `Zenith_RecordEnterRagdoll` / `Zenith_RecordRecoverFromRagdoll` take `const Zenith_PhysicsCommandKinds&`; `m_xGetUpClip` is a `Zenith_PersistentID` |
| PH7 | MOD | the assumed context members of "Assumed interfaces" (`GetSceneState` is settled as CompositionRoot's pointer form, and PHYS conforms; `FindCommandKind`, `GetPublicationSink` and `GetDebugDraw` stay pending) |
| PH8 | SCENE, ENV | `Zenith_FluidQuery` in ZenithScene, implemented by ENV: applied by SCENE (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_FluidQuery header"); PHYS cites it |
| PH9 | ENV | ENV's importer calls PHYS's intermediate writers; (b) revised by PHYS revision 2: PHYS conforms to ENV's string physical-material names (row PH25) |
| PH10 | SHIP | the `jolt` and `physx` entries |
| PH11 | Budgets | pending capacity rows; Z2-B-PHYS-002's definition |
| PH12 | ANIM | `Animation.ApplyRootMotion` filters `WITHOUT Zenith.Physics.CharacterController` |
| PH13 | SCRIPT | `Physics.RequestQuery` and the payload of the source kind `ASYNC_SOURCE_PHYSICS_QUERY` (the request spelling is core-doc item X264 (h)) |
| PH14 | CORE (contracts) | the three document types join [Z2-N-CONTRACT-552] |
| PH15 | ARCH | PHYS's parser-limit rows |
| PH16 | PROC | `Zenith2/Engine/ZenithPhysics*/**` joins the `Zenith2Memory` paths |
| PH17 | CORE | CB8: PHYS's "none / none" shedding row |
| PH18 | MOD | MB17 and MB18 producers |
| PH19 | ENV | EnB9 producers |
| PH20 | SCENE, STREAM | `Scene.ShiftWorldOrigin`'s playback dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (with X264 (j)); CharacterController 35 carries `PROPERTY_FLAG_WORLD_POSITION`: applied by SCENE (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shift playback", "Origin shifted lifecycle event"); PHYS cites it |
| PH21 | ENV | heightfield tiles square and their palette at most 127 entries (`ZPHI`); PH8's signature is the header's `Sample(float fX, float fZ, Zenith_FluidSample&)` |
| PH22 | SAVE | `Zenith_SaveRestoreContext::ReportCoherentRestore`; PHYS's use of `GetReissuedSourceKinds`, `IsRestoreReady` and `ListRestoreWaits` confirmed |
| PH23 | MOD | the single publication sink stays valid through `DetachWorld` and parks into the world's table, live or retired |
| PH24 | ANIM, GAME | confirmation: the ragdoll weight closed forms (recover held one tick, AN6 (e)), the census without an ANIM reader of the controller (AN6 (d)), GAME's test path and its exact-advance assertion |
| PH25 | ENV | the physical-material name mapping of `Zenith2/Docs/Design/PHYS/Formats.md` → "Physical-material names": the table, the cook-time resolution by PHYS's steps, the 32-bit ID `Zenith_MakePhysicsMaterialNameID`, and the writers' name-based signatures that ENV's collision stage calls |
| PH26 | ASSET | `Zenith_CookContext::FindAssetByLogicalPath` over the plan's record snapshot, for the physical-material table and the ragdoll cook's skeleton path |
