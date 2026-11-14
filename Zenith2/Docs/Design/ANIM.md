# ANIM design: animation

The design of the Animation area (ANIM): skeletons and their one rest pose, clips with their compression and streaming, the animation graph (blend trees, state machines, additive layers, masks, slots, synchronisation, events, root motion), retargeting, inverse kinematics, motion warping, facial animation, ragdoll transitions, significance-based update-rate budgeting, the extraction of skinned poses into the render world, and the authoring documents the editor's visual tools edit. ANIM's code is the module library `ZenithAnimation`, its cook steps in `ZenithCook`, and its import helpers that CONTENT's importers call. This entry document holds the skeleton sections, the top-level contracts and the core headers; the detail is in the four section files of "Document family".

## Purpose and scope

### What ANIM owns

| Capability | Where |
|---|---|
| the skeleton asset (`Animation.Skeleton`, `.zskel`): hierarchy, names, the one rest pose, the rig annotations, the curve table | `Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton file"` |
| skeleton import: glTF skins to skeletons, the rest pose from inverse bind matrices, the one-rest-pose rule | `Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"` |
| the clip asset (`Animation.Clip`, `.zanim`): resampling, root-motion extraction, additive cooking, compression with an error bound, chunked streaming | `Zenith2/Docs/Design/ANIM/Formats.md → "Clip file"`, `Zenith2/Docs/Design/ANIM/Formats.md → "Compression algorithm"` |
| the graph asset (`Animation.Graph`, `.zagraph`) and its compiler | `Zenith2/Docs/Design/ANIM/Formats.md → "Graph file"`, `Zenith2/Docs/Design/ANIM/Graph.md → "Graph compilation"` |
| graph evaluation: blend trees and blend spaces, state machines with crossfade and inertial transitions, additive layers and masks, slots, synchronisation, events, root motion | `Zenith2/Docs/Design/ANIM/Graph.md` |
| animator instances, their asset bindings and their per-world pose stores | `Zenith2/Docs/Design/ANIM/Runtime.md → "Animator instances"`, `Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"` |
| the phase-3 evaluation, the phase-4 root-motion application to entities without a character controller, the phase-6 post-physics blend and the phase-12 skinned-pose extractor | `Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"`, `Zenith2/Docs/Design/ANIM/Runtime.md → "Render extraction"` |
| ragdoll transitions on the animation side (pose ownership, blend, recovery and pose matching) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll transitions"` |
| retargeting, IK, motion warping, facial animation, pose followers | `Zenith2/Docs/Design/ANIM/Runtime.md` |
| significance-based update-rate reduction (the animation budgeting mechanism) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Update-rate budgeting"` |
| the five animation authoring-document types and the APIs the editor's animation tools call | `Zenith2/Docs/Design/ANIM/Formats.md → "Authoring documents"`, "Authoring and cooking" |
| the W-Characters workload's scale definition, its content and the ANIM per-system budget measurement | "Stage plan", "Budget rows" |
| the Luau `Animation` bindings and the animation events scripts receive | `Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"` |

### Explicit exclusions

| Not ANIM's | Owner | Basis |
|---|---|---|
| GPU skinning, morph-target deformation, the skinned-vertex pool, skinned vertex streams and the mesh's skin section | GEOM | `Zenith2/Docs/Design/GEOM/Skinning.md → "Scope of this file"`; GEOM's skinning is linear blend skinning, and ANIM adds no GPU skinning method |
| the generic pool allocator the skinned pool uses | RHI | the RB12 ownership ruling (RHI owns the mechanism, GEOM the pool policy) |
| the render world, its writer and the engine instance extractor | RPROD, REND | `Zenith2/Docs/Rendering.md → "Purpose and scope"`; ANIM writes only its skinned-pose records |
| ragdoll bodies, constraints and their simulation; the character controller | PHYS | ANIM consumes PHYS's ragdoll state ("Assumed interfaces from pending design documents") |
| applying root motion to an entity that has a physics character controller (the controller move in phase 4) | PHYS | the orchestrator's Q72 ruling (2026-10-03); [Z2-N-ARCH-475]; ANIM applies it itself, in phase 4, to every entity without a controller ("Tick-level flow of ANIM work"); cross-area row AN6 |
| the phase contract, the authority census and the lifecycle-command contract | GAME | Z2-D-080; `Zenith2/Docs/Design/GAME/PhaseContract.md → "Authority census"` (GAME registers no phase-4 node) |
| cinematic timelines, camera tracks, scrubbing and the cinematic writer source | CINE | CINE drives ANIM through slots and commands |
| rig and control authoring workflows, pose debugging tools, modular-character workflows, lip-sync workflows, the timed character-workflow walkthrough, the D-SEL-MOTIONMATCH ruling | CHAR | Inventory items `Z2-I-CHAR-01` … `-16`; ANIM supplies the runtime and data each builds on |
| the editor panels, viewports, undo and command system | EDIT | ANIM supplies documents, edit operations and the preview ("Authoring and cooking") |
| glTF and FBX parsing, axis, handedness and unit conversion, the DCC feature matrix | CONTENT | ANIM's import helpers receive parsed, converted data; FBX is D-SEL-FBX (open, Stage 4) |
| 2D sprite animation | TWOD | Z2-D-078 |
| hair and groom simulation | excluded | Z2-D-043, Z2-D-078 |
| animation replication over a network | NET (Stage 7) | Z2-D-073; ANIM's state is a deterministic function of replicated inputs |

### Inventory items covered

All sixteen ANIM items of `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, every one required:

| Item | Capability | Stage | Item-specific requirements (`Z2-R-ANIM-NNN`; the module-wide requirements covering every item are in "Coverage of inventory items" of `Zenith2/Docs/Requirements/ANIM.md`) |
|---|---|---|---|
| `Z2-I-ANIM-01` | skeleton import| S3 | 013–026, 032, 055–057, 059, 168 |
| `Z2-I-ANIM-02` | animation compression| S4 | 036–045 |
| `Z2-I-ANIM-03` | animation streaming| S4 | 046–054, 149, 167 |
| `Z2-I-ANIM-04` | blend trees (motion matching per D-SEL-MOTIONMATCH (open, Stage 3))| S3 | 008, 011, 027–032, 055–082, 135–139, 147, 148, 150–153, 157, 168, 172, 174–180 |
| `Z2-I-ANIM-05` | state machines| S3 | 008, 054, 060, 065, 083–093, 119, 145, 147, 148, 151–153, 157, 177, 180 |
| `Z2-I-ANIM-06` | additive layers| S3 | 034, 094–096, 099 |
| `Z2-I-ANIM-07` | masks| S3 | 096–099, 168 |
| `Z2-I-ANIM-08` | synchronization| S3 | 035, 100–102 |
| `Z2-I-ANIM-09` | animation events| S3 | 035, 093, 103–107, 154, 168 |
| `Z2-I-ANIM-10` | root motion| S3 | 008, 010, 012, 033, 044, 082, 108–111, 118, 122, 130, 135, 144 |
| `Z2-I-ANIM-11` | retargeting| S4 | 024, 120–124, 167 |
| `Z2-I-ANIM-12` | IK| S4 | 009, 024, 125–129, 167 |
| `Z2-I-ANIM-13` | motion warping| S4 | 009, 130, 167 |
| `Z2-I-ANIM-14` | facial animation| S4 | 009, 025, 043, 131–134 |
| `Z2-I-ANIM-15` | ragdoll transitions| S3 | 008, 010–012, 074, 112–119, 149, 177, 180 |
| `Z2-I-ANIM-16` | animation visual authoring| S4 | 023, 035, 065, 067, 097, 124, 157, 168–173 |

### Cross-area items ANIM implements

`Z2-I-CHAR-04` (animation update budgeting, CHAR) is implemented here: `Zenith2/Docs/Budgets.md` [Z2-N-BUDGET-438] makes significance-based update-rate reduction an implementation ticket of ANIM, and its rows are ANIM's. The runtime mechanism and its measurement are `Z2-R-ANIM-140` … `Z2-R-ANIM-146` (with the reduced-rate pose interpolation of `Z2-R-ANIM-138` and the Stage 1 producer `Z2-R-ANIM-174` … `Z2-R-ANIM-176`); CHAR's character-production workflows and walkthrough use it unchanged (cross-area row AN13).

Rows answered by ANIM's bounded post-signoff patch of 2026-10-16 (`C:/dev/Zenith2-drafts/reviews/ANIM_design_patch_2026-10-16.md`): CHAR's CH15 (the curve clip intermediate writer, `Z2-R-ANIM-181`; the rate divisor function, `Z2-R-ANIM-182`; the content-rule measures, `Z2-R-ANIM-183`; the motion-matching header at Stage 3) and CH16 (`Animation.GetMarkerTime`, `PlaySlot`'s `tag` and `ANIM_SLOT_TAG_FULL_RATE`, `Z2-R-ANIM-184`); CINE's CN27 (the published `anim.clip.markers`, `Z2-R-ANIM-185`); CONTENT's CT25 (c) (skeleton and clip product fingerprints and rename recognition, `Z2-R-ANIM-186`, pending ASSET's `SetProductFingerprint`, CONTENT row CT22) and CT33 (product key escaping, carried by the import-helper tickets).

### Document family

| File | Holds | `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/ANIM.md` (this file) | the skeleton sections, the core headers, the reconciliation items | the skeleton of the A3 brief and "Reconciliation items" |
| `Zenith2/Docs/Design/ANIM/Formats.md` | cooked formats, intermediates, import conversion, compression, documents, parser limits | Scope of this file · Common format rules · Skeleton file · Clip file · Graph file · Bit-rate table · Compression algorithm · Skeleton intermediate · Clip intermediate · Import conversion · Authoring documents · Parser limits |
| `Zenith2/Docs/Design/ANIM/Graph.md` | graph model and evaluation | Scope of this file · Graph model · Parameters · Node catalogue · Evaluation algorithm · Pose blending · Blend spaces · State machines · Layers and masks · Slots · Synchronization · Animation events · Root motion · Graph compilation · Graph hot reload · Motion matching node |
| `Zenith2/Docs/Design/ANIM/Runtime.md` | runtime headers, instances, systems, features, extraction, save, bindings | Scope of this file · Zenith_SkeletonAsset header · Zenith_AnimationClip header · Zenith_AnimationGraphAsset header · Zenith_AnimationPose header · Zenith_AnimationWorld header · Zenith_AnimationCommands header · Zenith_AnimationEvents header · Zenith_AnimationIK header · Zenith_AnimationRetarget header · Zenith_AnimationPreview header · Zenith_AnimationMotionMatch header · Asset kind loaders · Asset bindings · Animator instances · Systems · Lifecycle listener · Ragdoll transitions · Inverse kinematics · Motion warping · Retargeting · Facial animation · Update-rate budgeting · Clip streaming · Render extraction · Save state · Luau bindings · Memory consumer |
| `Zenith2/Docs/Design/ANIM/Tests.md` | every named test | Scope of this file · Test targets · Asset and format tests · Compression tests · Import and cook tests · Graph tests · Blend and pose tests · State machine tests · Sync event and root motion tests · Ragdoll and lifecycle tests · IK motion warping and retarget tests · Facial and streaming tests · Budgeting and extraction tests · Module binding and failure tests · Motion matching tests · Determinism tests · Integration scenarios · Fuzz targets · Benchmarks · Fixtures and generators |
| `Zenith2/Docs/Requirements/ANIM.md` | the requirement catalogue `Z2-R-ANIM-001` … `Z2-R-ANIM-186`: the Stage 1 and Stage 3 requirements | Purpose · Requirements · Coverage of inventory items · Excluded and deferred |
| `Zenith2/Docs/Requirements/ANIM/Stage4Plus.md` | the Stage 4 and Stage 6 requirements, same numbering | Purpose · Requirements |

### Conventions

- Names are Zenith Hungarian (Z2-D-034); the module's public headers are `Engine/ZenithAnimation/Zenith_Animation*.h`, `Zenith_SkeletonAsset.h` and `Zenith_Module_Animation.h`.
- A line `<!-- elaboration -->` marks a detail this design adds; `<!-- assumption: pending <AREA> -->` blocks state an interface another area's design has not yet provided, each with its cross-area row (`C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`, rows `AN1` … `AN17`).
- Units are CORE's: metres, seconds, radians, unit quaternions; joint indices are skeleton-local; name hashes are `Zenith_Hash64` of UTF-8 names.
- Obligations here are traced to `Z2-R-ANIM-NNN` in `Zenith2/Docs/Requirements/ANIM.md`; "Requirement index" is the map.

## Dependencies

### Libraries ANIM code depends on

| Library | Why | Declared in Architecture |
|---|---|---|
| ZenithRuntime | module interfaces, start and attach contexts, `Zenith_ExtractorDesc` | yes |
| ZenithAssets | asset handles, kind loaders, the loader and reader, completion queues | yes |
| ZenithScene | `Zenith_Transform`, transform components, the lifecycle listener and teleport payload | yes |
| ZenithECS, ZenithReflect | components, world resources, systems, arbitration, commands, events, the binding ABI | yes |
| ZenithJobs | `ParallelFor` sub-jobs of the evaluation and blend nodes | yes |
| ZenithCore | containers, maths, hashing, results, the heap | yes |
| ZenithProfiling | zones and counters | yes |
| ZenithFlux | `Flux_RenderWorldWriter`, `Flux_RenderSkinnedPose`, `Flux_JointMatrix`, `Flux_MakeEntityInstanceKey`, `Flux_MeshResource` and `Zenith.Render.MeshInstance` for the extractor | yes (row AN2, X210 (1)) |
| ZenithPhysics (the API library) | the ragdoll state and pose-source interface, body poses, ray casts for foot placement, the character-controller component type the phase-4 root-motion node filters out | yes (row AN2, X210 (1)) |
| ZenithPlatform | the `Zenith_Config` facade read at module start for `animation.*` keys | yes (row AN2, X210 (1)) |

The layer of `ZenithAnimation` is 8 (one above ZenithPhysics, like ZenithAI), and ZenithCinematics, which depends on it, 9 (`Zenith2/Docs/Architecture.md` → "Declared direct dependencies"); the module list's `Animation` row carries the module dependency `Physics` (`Zenith2/Docs/Architecture.md` → "The module list"), so a game selecting Animation selects Physics ([Z2-N-ARCH-267]); every workload and flagship game that animates already selects both. `ZenithAnimation` never depends on ZenithScript (bindings go through ZenithReflect, [Z2-N-ARCH-085]; this is also why ANIM's asynchronous results travel through a module publication sink rather than the world's `Zenith_AsyncResultTable`, row AN1) nor on ZenithDocument (its document types register tool-side, in the tool library `ZenithAuthoring`, "ZenithAuthoring ANIM files"). ZenithCook already depends on every module and so on ZenithAnimation.

### Headings this area relies on

| Heading | Used for |
|---|---|
| `Zenith2/Docs/Architecture.md → "Engine modules and game projects"`, `Zenith2/Docs/Architecture.md → "Library dependency graph"` | the module row, entry points, edges and layer |
| `Zenith2/Docs/Architecture.md → "Frame phases and authority"`, `Zenith2/Docs/Architecture/FramePhases.md → "The twelve phases"`, `Zenith2/Docs/Architecture/FramePhases.md → "Per-field authority"`, `Zenith2/Docs/Architecture/FramePhases.md → "Arbitration rules"` | phase 3 and phase 6 work, authority, the pose-ownership arbitration |
| `Zenith2/Docs/Architecture/LifecycleCommands.md → "Teleport"`, `Zenith2/Docs/Architecture/LifecycleCommands.md → "Ragdoll enter and recover"`, `Zenith2/Docs/Architecture/LifecycleCommands.md → "Scenario tests"` | the animation row of teleport, ragdoll ownership blending, the scenarios |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"`, `Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"` | the extractor, the attach context |
| `Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms"`, `Zenith2/Docs/Architecture.md → "Multiplayer foundations"` | Z2-D-019 obligations, the asynchronous-result publication path and the replay log |
| `Zenith2/Docs/Architecture.md → "Memory governor"`, `Zenith2/Docs/Architecture.md → "Security architecture"` | the simulation subsystem's no-shedding rule, parser limits |
| `Zenith2/Docs/Contracts.md → "Errors and results"`, `Zenith2/Docs/Contracts.md → "Contract blocks per type"`, `Zenith2/Docs/Contracts.md → "API boundary"`, `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`, `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` | every public declaration, documents' migration, concurrency |
| `Zenith2/Docs/Rendering.md → "Purpose and scope"` | the render-world hand-off and `AddSkinnedPose` |
| `Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header"`, `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"`, `Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"` | the pose record, the mesh's skin section, morph-target names |
| `Zenith2/Docs/Design/RPROD/Extractors.md → "Instance keys"`, `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`, `Zenith2/Docs/Design/RPROD/Extractors.md → "Extraction resolver"` | the instance key, the mesh-instance component, the presentation-side resolution pattern |
| `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"`, `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"`, `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeBuilder"`, `Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration merge"`, `Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"` | systems, registration, arbitration, extractor ordering |
| `Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_CommandKinds header"`, `Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ECSObservers header"` | ANIM's command kinds; the tick-boundary access a publication handler receives |
| `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleListener header"`, `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleCommands header"`, `Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"`, `Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"` | the listener, teleport flags, `Zenith_Transform`, `Zenith.LocalTransform` and `Zenith.WorldTransform` |
| `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"` | tool-side document registration and canonical form |
| `Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoader"`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoadCompletionQueue"`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"`, `Zenith2/Docs/Design/ASSET/Loading.md → "Publication to simulation"` | kinds 7–9, loaders, binding loads, chunk reads, deterministic publication |
| `Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"`, `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`, `Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"` | the container, cook steps, published properties, products |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext header"` | sub-jobs and node arenas |
| `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Quaternion"`, `Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`, `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`, `Zenith2/Docs/Design/CORE/Maths.md → "Numerical conventions"` | maths, coordinate conventions, bit-reproducibility rules |
| `Zenith2/Docs/Design/PLAT/Configuration.md → "Simulation-affecting keys"` | the budgeting keys' layer restriction and replay recording |
| `Zenith2/Docs/Scripting/Bindings.md → "Binding registration and the thunk ABI"`, `Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"` | the `Animation` binding table and its misuse-code block |
| `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"` | the source kind `ASYNC_SOURCE_ANIMATION` |
| `Zenith2/Docs/Design/GAME/PhaseContract.md → "Census format"`, `Zenith2/Docs/Design/GAME/PhaseContract.md → "Census rows added by subsequent designs"` | ANIM's authority-census rows |
| `Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`, `Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"`, `Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay seeking"` | the save participant, per-capture fidelity, re-issued in-flight work |
| `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"` | `Flux_SelectMainCamera`, the main camera significance reads |
| `Zenith2/Docs/Budgets.md → "W-Characters"`, `Zenith2/Docs/Budgets.md → "Per-system producer shape"`, `Zenith2/Docs/Budgets.md → "Animation budgeting and PSO preparation"` | the workload, the harness half, the budgeting obligation |

### Upward-callback interfaces

ANIM declares the interfaces its callers implement, so every call points downward ([Z2-N-ARCH-078]):

| Interface | Declared in | Implemented by | Purpose |
|---|---|---|---|
| `Zenith_AnimMeshResolver` (Internal) | `Internal/Extract/Zenith_AnimMeshResolver.h` | ANIM's production resolver over `Zenith_AssetRegistry`; the test fixture | presentation-side mesh resolution for the extractor |

ANIM implements `Zenith_AssetKindLoader` (ASSET) three times, `Zenith_ModuleInstance` (ZenithRuntime), `Zenith_LifecycleListener` (ZenithScene), `Zenith_MemoryConsumer` (ZenithCore), `Zenith_SaveParticipant` (ZenithSave, rows AN8 and AN17), and PHYS's `Zenith_RagdollPoseSource` (assumed, row AN6); it stores no pointer into a higher library.

### Assumed interfaces from pending design documents

<!-- assumption: pending MOD, GEOM, RPROD, PHYS, GAME, SAVE, CINE, SCRIPT -->
| Need | Owner | ANIM's assumption | Row |
|---|---|---|---|
| deterministic publication of module asynchronous results | MOD (SCRIPT's runtime-side forwarder) | ANIM conforms to the **single module publication sink** that MOD's post-signoff patch specifies (the orchestrator's Q76 (a) ruling), and declares no sink shape of its own. ANIM's needs of it: (1) completion IDs allocated **on the world's merge chain**, as [Z2-N-SCRIPT-341] requires: in the merge function of `Animation.Significance`, which declares `ConsumesMergeCapacity` so that it gets a merge node (`Zenith2/Docs/Design/ECS/Scheduling.md → "Merge nodes and the merge chain"`), one per world, in the order the system node collected the needs (ascending simulation ID, then ascending (clip GUID, chunk) for chunk reads); and none during a save restore; (2) both publication kinds, `PUBLICATION_KIND_GENERIC_RECOMPUTED` for a success with no payload and `PUBLICATION_KIND_GENERIC_RECORDED` for a failure carrying ASSET's 24-byte `Zenith_AssetLoadResultRecord`, so the maximum result size is 24 bytes; (3) a source declaration per world at AttachWorld with `uMaxInFlight = animation.bindingQueueCapacity + animation.streaming.maxReadsInFlight` (defaults 1,024 + 256) and `uMaxResultBytes = 24`; ANIM keeps two counters from allocation to publication (binding loads ≤ `animation.bindingQueueCapacity`, so its completion queue cannot overflow; chunk reads ≤ `animation.streaming.maxReadsInFlight`), and a need beyond its counter, or refused by the sink with `ASYNC_TABLE_ERROR_FULL`, is deferred to a subsequent run in its order and allocates no ID ("Asset bindings" step 1 of `Zenith2/Docs/Design/ANIM/Runtime.md`); (4) a C++ handler called at tick-boundary step 1 in completion-ID order with the `Zenith_TickBoundaryAccess&`, the publications recorded into and replayed from the replay log; (5) re-issue of a saved in-flight request under its recorded completion ID at restore, and SAVE's restore of an unpublished result as completed (published at the boundary of tick t + 1), for which ANIM rebuilds the payload-less state itself ("Save state" of `Zenith2/Docs/Design/ANIM/Runtime.md`); (6) the producer obligation of the core-doc item X255 and GAME's row GA19: ANIM completes every completion ID it allocated exactly once, an abandoned request (its requester destroyed or rebound, its chunk no longer wanted, every request outstanding at `DetachWorld`) with the zero-length `PUBLICATION_KIND_GENERIC_RECORDED` "abandoned" payload through the sink, and counts no discard itself ("Asset bindings" step 4; core text since X255: [Z2-N-SCRIPT-931]). The source kind is `ASYNC_SOURCE_ANIMATION`, value 9 in SCRIPT's registry (`Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`, [Z2-N-SCRIPT-924], [Z2-N-SCRIPT-925]); ANIM names the enumerator and never declares a value. A sink is needed because ZenithAnimation has no ZenithScript edge and so cannot reach the world's `Zenith_AsyncResultTable` itself; ASSET's completion queue it uses directly ("Asset bindings" of `Zenith2/Docs/Design/ANIM/Runtime.md`) | AN1 |
| mesh-side skin and morph data | GEOM | `Flux_MeshResource::GetSkeletonID()`, `GetMorphTargetNameHash(uint32)`; the skinned-mesh cook compares inverse binds with the skeleton's published `restModelInverse` within `fANIM_REST_POSE_TOLERANCE` and publishes `skinShellDistance`; GEOM's cook reads `jointCount`, `parents`, `restModel`, `maxReach` by those names | AN4 |
| skinned-pose copy semantics | RPROD, REND | `Flux_RenderWorldWriter::AddSkinnedPose` copies the joint and morph-weight spans into the partition's slot arena, as ENV's typed members do | AN5 |
| ragdoll state, body poses, the pose source, ray casts, the recover placement, the controller's root-motion read | PHYS | "Ragdoll transitions" of `Zenith2/Docs/Design/ANIM/Runtime.md` (the component `Zenith.Physics.Ragdoll`, `Zenith_RagdollPoseSource`, `GetRagdollBodyPoses`, `RayCast` on `Zenith_PhysicsWorldResource`, the phase-6 blend-weight node `Physics.RagdollBlend`, the entity placement of PHYS's recover playback); the character-controller component `Zenith.Physics.CharacterController` and its phase-4 node `Physics.CharacterMove`, which reads `Zenith.Animation.RootMotion` and applies it to every row that has a controller with `Zenith_Rotate(q₀, DeltaPosition)` and no scale, consistent with DeltaPosition's pre-scaled units (Q72 ruling; "Animation.ApplyRootMotion" of `Zenith2/Docs/Design/ANIM/Runtime.md`); PHYS's RECOVERING weight schedule (`BlendWeight` non-increasing, lowered by `Physics.RagdollBlend` in phase 6 before ANIM's phase-6 nodes, reaching 0) and, by the orchestrator's PHYS-RECOVER ruling (pending PHYS revision 1), unchanged in the first phase 6 after the recover playback, so ANIM's first recover tick reads the playback's weight (1 from ACTIVE), a 0 s recover holding it at the first phase 6 and reaching 0 at the second; ANIM derives its recovery fraction from it ("Ragdoll transitions", "Ragdoll recover"); PHYS's world resource is `Zenith.Physics.World` (type `Zenith_PhysicsWorldResource`), which ANIM's systems read by that stable name; and the declarations of AN6 (f): PHYS's census row for `Zenith.Physics.Ragdoll` 1–2 names `Animation.PostPhysicsBlend` (6, after the writer) and `Animation.Significance` (3, State, the forced full rate) as readers, and `Physics.Step` (the pose source's drive reads) and the ragdoll enter playback (`GetEntryModelPose`) declare `ReadResource Zenith.Animation.Residency`, the enter playback also `Zenith.Animation.FinalPoses` | AN6 |
| the phase contract and the authority census | GAME | GAME registers no phase-4 node (its row GA15); it rules ANIM's census rows in the D-PHASES refinement (open, Stage 1), which confirms "Field authority table", and owns the census format into which ANIM's registering tickets add their rows ("Component registration") | AN7 |
| save and checkpoint contribution | SAVE | the participant interface is SAVE's settled text (`Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`, SAVE revision 2), which ANIM implements member by member ("Participant members" of `Zenith2/Docs/Design/ANIM/Runtime.md`): `GetReissuedSourceKinds` declares `ASYNC_SOURCE_ANIMATION`, every re-issued entry is marked through `Zenith_SaveReissueSet::MarkReissued`, `IsRestoreReady` and `ListRestoreWaits` drive the readiness step; the `ZenithSave` edge is Architecture's (X210 (1)); still pending: SAVE's async section keeping its treatment of unpublished ANIM results (put back as completed in an ordinary restore; a recorded one back in flight under a publication source; never re-issued, `Zenith2/Docs/Design/SAVE/Formats.md → "Async section"`) | AN8 |
| the cinematic writer and slot use | CINE | CINE's system writes `Zenith.Animation.Pose.PhysicsWeight` with `WRITER_SOURCE_CINEMATIC` while a cinematic holds a character, plays performances through cinematic-tagged slots and scrubs with `Animation.SetSlotTime` | AN9 |
| module commands from bindings | SCRIPT | a buffered binding records ANIM's command kinds through the field-command side-effect kind | AN10 |
<!-- /assumption -->

Settled since the assumption block was written: row AN17 (restore readiness) is SAVE's revision 2 text (`IsRestoreReady`, `WORLD_RESTORE_STATE_PARTICIPANTS_READY` = 7, and `ListRestoreWaits` for the synchronous restore driver), so it has no assumption row. Row AN2 is core text since cross-doc item X210 (core-doc patch e), so it has none either: (1) the ZenithAnimation row (layer 8, ZenithFlux, ZenithPhysics, ZenithPlatform and ZenithSave) and Cinematics' layer 9 in `Zenith2/Docs/Architecture.md` → "Declared direct dependencies", and the module list's Animation row requiring Physics in `Zenith2/Docs/Architecture.md` → "The module list"; (2) the "animation assets" row of `Zenith2/Docs/Architecture.md` → "Parser limits"; (3) `Zenith_RagdollRecoverDesc::m_xGetUpClip` as `Zenith_PersistentID` in `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Ragdoll enter and recover"; (4) the five animation document types in [Z2-N-CONTRACT-552]; (5) `Animation.ApplyRootMotion` in the phase-4 row of `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases".

### Areas that consume ANIM

| Area | Uses |
|---|---|
| GEOM | skinned poses in the render world; the skeleton's published cook properties |
| GAME | animation events; parameters and slots through commands and bindings; ANIM's rows of the authority census |
| PHYS | root-motion deltas in phase 4 (the character-controller node); the ragdoll pose source; the pose-ownership arbitration |
| CINE | slots, slot time, forced states, the cinematic writer source |
| AUDIO, VFX | `Zenith.Animation.Notify` events with sub-tick times (animation/audio clock sync) |
| SAVE | ANIM's save records |
| CHAR | rig documents, the debug state, pose followers, curves and pose libraries, the budgeting mechanism, the motion-matching harness |
| EDIT | documents, the preview, graph compile diagnostics, live debug state |
| SCRIPT, game scripts | the `Animation` binding table and events |
| RPROD | the W-Characters workload's content; the extractor inside `Z2-B-RPROD-003`'s span |

## Library and file layout

### Target summary

| Target | Kind | `CMakeLists.txt` | `ZENITH_SUPPORTED` |
|---|---|---|---|
| `ZenithAnimation` | `KIND MODULE`, `MODULE_NAME Animation` | `zenith_add_library(ZenithAnimation KIND MODULE MODULE_NAME Animation DEPENDS ZenithRuntime ZenithPhysics ZenithFlux ZenithPlatform ZenithSave ZenithAssets ZenithScene ZenithECS ZenithReflect ZenithJobs ZenithCore ZenithProfiling)`; the first eight of Architecture's row plus the three edges pending ARCH (row AN2) and the `ZenithSave` edge SAVE's save-participant interface needs (SAVE's row SV17, pending ARCH with the others), landed by the module skeleton ticket after the core-doc batch | `ZENITH_GPU` (every module is, [Z2-N-ARCH-173]) |
| ANIM's cook steps and import helpers in `ZenithCook` | part of the tool | ZenithCook's own call | `ZENITH_TOOLS_HOST` |
| ANIM's graph compiler and document-type registration in `ZenithAuthoring` | ANIM's part of the static tool library of ruling Q84 (a) (`Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`), linked by ZenithCook and ZenithEditor | `Zenith2/Tools/Authoring/CMakeLists.txt` (EDIT's call; ANIM's move adds `ZenithAnimation` to its `DEPENDS`, pending ARCH, X257) | `ZENITH_TOOLS_HOST` |
| `Zenith_Integration_Animation` | integration suite | `zenith_add_integration_tests(Animation DEPENDS ZenithRuntime ZenithAnimation ZenithPhysics ZenithPhysics_Jolt)` (plus `ZenithCinematics` from Stage 4) | narrowed by its dependencies ([Z2-N-ARCH-172]) |
| `Zenith_Bench_AnimEvaluation`, `Zenith_Bench_ClipSampling`, `Zenith_Bench_ClipCompression` | benchmarks | one directory each, `Zenith2/Benchmarks/AnimEvaluation/`, `Zenith2/Benchmarks/ClipSampling/`, `Zenith2/Benchmarks/ClipCompression/`, each `CMakeLists.txt` one `zenith_add_benchmark` call ([Z2-N-ARCH-157]; `zenith_add_subdirectories(Benchmarks GROUPS Budgets)` globs `Benchmarks/<Harness>/` only, [Z2-N-ARCH-156]); `ClipCompression` depends on `ZenithCook_Objects` and is `ZENITH_TOOLS_HOST` only (the `zenith_add_benchmark` row of `Zenith2/Docs/Architecture/BuildSystem.md` admits a tool's `<Tool>_Objects` in `DEPENDS`) | the presets of their dependencies |

### ZenithAnimation files

```
Zenith2/Engine/ZenithAnimation/
	CMakeLists.txt
	Zenith_Module_Animation.h          module entry points
	Zenith_AnimationErrors.h           Zenith_AnimationError, Zenith_AnimationCookDetail, Zenith_ToString overloads
	Zenith_AnimationTypes.h            constants, enumerations, small value types
	Zenith_AnimationMath.h             Zenith_DualQuaternion and its functions
	Zenith_AnimationComponents.h       the six component structs, their type IDs and property IDs
	Zenith_SkeletonAsset.h             Runtime.md "Zenith_SkeletonAsset header"
	Zenith_AnimationClip.h             Runtime.md "Zenith_AnimationClip header"
	Zenith_AnimationGraphAsset.h       Runtime.md "Zenith_AnimationGraphAsset header"
	Zenith_AnimationPose.h             Runtime.md "Zenith_AnimationPose header"
	Zenith_AnimationWorld.h            Runtime.md "Zenith_AnimationWorld header"
	Zenith_AnimationCommands.h         Runtime.md "Zenith_AnimationCommands header"
	Zenith_AnimationEvents.h           Runtime.md "Zenith_AnimationEvents header"
	Zenith_AnimationIK.h               Runtime.md "Zenith_AnimationIK header"
	Zenith_AnimationRetarget.h         Runtime.md "Zenith_AnimationRetarget header"
	Zenith_AnimationPreview.h          Runtime.md "Zenith_AnimationPreview header"
	Zenith_AnimationMotionMatch.h      Runtime.md "Zenith_AnimationMotionMatch header" (Zenith_MotionMatcher, the D-SEL-MOTIONMATCH noun)
	Internal/Module/                   Zenith_AnimationModule.cpp (entry points, start, attach, detach), Zenith_AnimationTypes.cpp (registration),
	                                   Zenith_AnimationConfig.cpp (keys), Zenith_AnimationMemory.cpp (consumer), Zenith_AnimationCommandKinds.cpp
	Internal/Assets/                   Zenith_SkeletonLoader.cpp, Zenith_ClipLoader.cpp, Zenith_GraphLoader.cpp, Zenith_AnimBindings.cpp/.h (requests, cache, handler)
	Internal/Sampling/                 Zenith_AnimBitRates.h, Zenith_AnimQuantise.h, Zenith_DecodedClipSegment.h, Zenith_ClipSampler.cpp, Zenith_RootMotion.cpp
	Internal/Graph/                    Zenith_GraphEvaluator.cpp/.h (update and pose passes), Zenith_GraphNodes.cpp, Zenith_StateMachine.cpp,
	                                   Zenith_BlendSpaces.cpp, Zenith_SyncGroups.cpp, Zenith_AnimEvents.cpp, Zenith_Inertialiser.cpp, Zenith_Slots.cpp,
	                                   Zenith_MotionMatcher.cpp (the database builder and search, with the harness, Stage 3),
	                                   Zenith_MotionMatchNode.cpp (node kind 15, Stage 4, its tickets blocked by the D-SEL-MOTIONMATCH (open, Stage 3) ruling)
	Internal/Pose/                     Zenith_PoseOps.cpp, Zenith_DualQuaternion.cpp, Zenith_ScrewPower.h (both branches of the screw coefficients), Zenith_PoseMirror.cpp
	Internal/Instances/                Zenith_AnimInstance.h/.cpp, Zenith_AnimResidency.h (Zenith_AnimationResidency, the world resource Zenith.Animation.Residency),
	                                   Zenith_AnimInstanceTable.cpp, Zenith_AnimSaveRecord.cpp, Zenith_AnimSaveParticipant.cpp
	Internal/Systems/                  Zenith_AnimSignificance.cpp, Zenith_AnimEvaluate.cpp, Zenith_AnimApplyRootMotion.cpp, Zenith_AnimPostPhysics.cpp,
	                                   Zenith_AnimLifecycle.cpp
	Internal/Ragdoll/                  Zenith_AnimRagdollSource.cpp/.h (PHYS's pose source), Zenith_AnimRagdollBlend.cpp
	Internal/IK/                       Zenith_TwoBoneIK.cpp, Zenith_ChainIK.cpp, Zenith_LookAt.cpp, Zenith_FootPlacement.cpp, Zenith_IKStage.cpp
	Internal/Features/                 Zenith_MotionWarp.cpp, Zenith_Retarget.cpp, Zenith_RetargetCache.cpp, Zenith_PoseLibrary.cpp, Zenith_AnimBudget.cpp
	Internal/Streaming/                Zenith_AnimStreamCache.cpp/.h, Zenith_AnimPrefetch.cpp
	Internal/Extract/                  Zenith_AnimMeshResolver.h, Zenith_AnimAssetMeshResolver.cpp, Zenith_ExtractPoses.cpp, Zenith_MorphRemap.cpp
	Internal/Bindings/                 Zenith_AnimationBindings.cpp (the Animation binding table)
	Internal/Debug/                    Zenith_AnimDebugDraw.cpp, Zenith_AnimDump.cpp, Zenith_AnimationTestAccess.h
	Tests/                             Zenith2/Docs/Design/ANIM/Tests.md
	Tests/Support/                     Zenith_AnimTestFixtures.h (header-only: inline generators, [Z2-N-ARCH-255])
	Tests/Fuzz/                        SkeletonFile.cpp, ClipFile.cpp, GraphFile.cpp
	Tests/Examples/                    Zenith_AnimationPose_Example.cpp, Zenith_AnimationCommands_Example.cpp
```

No ANIM source is platform-specific; there is no `Internal/Windows|Android|Linux/` directory.

### ZenithCook ANIM files

```
Zenith2/Tools/ZenithCook/Internal/Animation/
	Zenith_AnimImport.h/.cpp             Zenith_BuildSkeletons, Zenith_WriteClipIntermediates (called by CONTENT's glTF importer)
	Zenith_CookSkeleton.cpp              the step anim.skeleton (.zskel), its rig-document reading and published properties
	Zenith_CookClip.cpp                  the step anim.clip (.zanim): resampling, loop closure, root motion, additive, retargeted products
	Zenith_ClipCompressor.h/.cpp         Formats.md "Compression algorithm"
	Zenith_CookGraph.cpp                 the step anim.graph (.zagraph) and the importer anim.graphDocument: gathers the compiler's inputs
	                                     through Zenith_CookContext and calls Zenith_CompileAnimationGraph ("ZenithAuthoring ANIM files")
	Zenith_RetargetImporter.cpp          the importer anim.retarget (retarget documents to products)
	Zenith_AnimationCookTypes.h          Zenith_AnimationImportError, Zenith_AnimationCookWarning
Zenith2/Tools/ZenithCook/Tests/Animation/                     the cook and import tests of Tests.md
Zenith2/Tools/ZenithCook/Tests/Fuzz/                          AnimIntermediates.cpp, AnimDocuments.cpp
```

ZenithCook calls ANIM's registration `Zenith_RegisterAnimationDocumentTypes` and compiler from `ZenithAuthoring`, which it declares in its own `DEPENDS` (EDIT's design: the first owner's move adds it).

### ZenithAuthoring ANIM files

By the orchestrator's ruling Q84 (a) (`Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`; the core rows are X257), the code both ZenithCook and ZenithEditor call lives in the static tool library `ZenithAuthoring`, one subdirectory per owner. ANIM's part:

<!-- assumption: pending ARCH (X257) -->
```
Zenith2/Tools/Authoring/
	Zenith_AuthoringAnimation.h          ANIM's root header (below), included as "Authoring/Zenith_AuthoringAnimation.h"
	Internal/Animation/                  Zenith_GraphCompiler.cpp (Graph.md "Graph compilation": compile steps, diagnostics, Delaunay
	                                     triangulation), Zenith_AnimationDocumentTypes.cpp (the five document types, their canonical
	                                     schemas and migration chains)
	Tests/Animation/                     Zenith_GraphCompiler_Tests.cpp, Zenith_AnimDocuments_Tests.cpp (ZenithAuthoring_Tests)
	Tests/Fixtures/Migration/Animation/  one directory per document type, v<N>.json and v<N>.expected.json
```
<!-- /assumption -->

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAnimation.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"   // Zenith_MigrationRegistry, Zenith_DocumentError
#include "ZenithAnimation/Zenith_AnimationErrors.h"    // Zenith_AnimationError
#include "ZenithAnimation/Zenith_AnimationTypes.h"

/// @contract enum Zenith_AnimGraphDiagnosticCode
/// Ownership: Plain enumeration (moved here from ZenithCook's Zenith_AnimationCookTypes.h).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each value is one row of Graph.md "Compile diagnostics".
/// Version: since Z2 Stage 3. Tool-side; values keep their meaning.
/// @endcontract
enum Zenith_AnimGraphDiagnosticCode : uint16
{
	ANIM_GRAPH_DIAG_REFERENCE = 1, ANIM_GRAPH_DIAG_SKELETON_MISMATCH, ANIM_GRAPH_DIAG_NODE_SHARED, ANIM_GRAPH_DIAG_NODE_UNREACHABLE,
	ANIM_GRAPH_DIAG_POSE_STACK_DEPTH, ANIM_GRAPH_DIAG_LIMIT, ANIM_GRAPH_DIAG_PARAMETER_TYPE, ANIM_GRAPH_DIAG_BLEND_SPACE_DEGENERATE,
	ANIM_GRAPH_DIAG_RESERVED_NAME, ANIM_GRAPH_DIAG_MASK, ANIM_GRAPH_DIAG_MARKERS_MISSING, ANIM_GRAPH_DIAG_IK_DEFINITION,
};

/// @contract value Zenith_AnimGraphCompileInputs
/// Ownership: Borrowed views the caller keeps alive for the call: ZenithCook's anim.graph step fills them through its
///   Zenith_CookContext (OpenSourceDocument, ReadDependencyOutput, which record the cook inputs); the editor from its open
///   documents and the cooked properties it has read.
/// Thread affinity: IMMUTABLE for the call. Members non-const: callers default-construct it and set the fields they
///   need before passing it.
/// Invalidation: None.
/// Units: Bytes of published properties, exactly as the producing steps publish them.
/// Errors: None; missing or malformed inputs are diagnostics of the compile.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimGraphCompileInputs
{
	const Zenith_JsonDocument* m_pxGraphDocument = nullptr;              // migrated to the current schema version
	Zenith_Span<const Zenith_PersistentID> m_axMaskIDs;                  // the masks the document names, ascending GUID
	Zenith_Span<const Zenith_JsonDocument* const> m_apxMaskDocuments;    // parallel to m_axMaskIDs
	Zenith_Span<const Zenith_ByteSpan> m_axSkeletonProperties;           // the nine published properties, in Formats.md order
	Zenith_Span<const Zenith_PersistentID> m_axClipIDs;                  // every clip the document names, ascending GUID
	Zenith_Span<const Zenith_ByteSpan> m_axClipInfos;                    // anim.clipInfo, parallel to m_axClipIDs
	Zenith_ByteSpan m_xMotionDatabase;                                   // the finished ANIM_SECTION_GRAPH_MOTION_DATABASE bytes the
	                                                                     // cook step built (ruling ANIM-F19-VENUE); empty: no kind-15 node,
	                                                                     // or a caller (the editor's preview compile) that cannot build it
};

/// @contract value Zenith_AnimGraphCompileResult
/// Ownership: Owns its arrays (allocated from the caller's heap passed to Zenith_CompileAnimationGraph).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Section bytes of the ZAGR format (Formats.md "Graph sections"), in section order.
/// Errors: m_bSucceeded false when any diagnostic is an error; the sections are then empty.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimGraphCompileResult
{
	bool m_bSucceeded = false;
	Zenith_Array<uint32> m_auSectionIDs;                                // the ZAGR section kinds written
	Zenith_Array<Zenith_Array<uint8>> m_axSectionBytes;                 // parallel to m_auSectionIDs
	uint64 m_ulGraphHash = 0;
	Zenith_Array<uint8> m_auGraphInfo;                                  // the anim.graphInfo property bytes
	Zenith_Array<Zenith_AnimGraphDiagnostic> m_axDiagnostics;            // sorted (Graph.md "Compile diagnostics")
	Zenith_Array<Zenith_AnimBlendSpaceTriangulation> m_axTriangulations; // per BLEND2D node, for the editor's display
};

/// @contract functions Zenith_AuthoringAnimation
/// Ownership: The registration adds the five animation document types to the caller's registry; the compiler owns nothing
///   beyond its result.
/// Thread affinity: OWNER_THREAD — the caller's (a cook worker, or the editor's main thread or background operation).
/// Invalidation: None.
/// Units: None.
/// Errors: Registration returns the first failing RegisterDocumentType result unchanged; the compiler reports every
///   document error as a diagnostic and returns ANIMATION_ERROR_OUT_OF_MEMORY only when an allocation fails.
/// Version: since Z2 Stage 3. Tool-side; ZenithAuthoring is outside every Shipping link closure.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterAnimationDocumentTypes(Zenith_MigrationRegistry& xRegistry);
Zenith_Result<void, Zenith_AnimationError> Zenith_ListAnimationGraphInputs(const Zenith_JsonDocument& xGraphDocument,
	Zenith_Array<Zenith_PersistentID>& axMaskIDsOut, Zenith_Array<Zenith_PersistentID>& axClipIDsOut,
	Zenith_Array<Zenith_PersistentID>& axMotionMatchClipIDsOut, Zenith_PersistentID& xSkeletonOut);
Zenith_Result<Zenith_AnimGraphCompileResult, Zenith_AnimationError> Zenith_CompileAnimationGraph(
	const Zenith_AnimGraphCompileInputs& xInputs, Zenith_Heap& xHeap);

/// @contract value Zenith_AnimCurveChannelDesc
/// Ownership: Borrows the name and both spans for the writer call.
/// Thread affinity: IMMUTABLE for the call. Members non-const: callers fill arrays of it element by element before
///   passing them.
/// Invalidation: None.
/// Units: Seconds (times, source clip time, ≥ 0); values in the path's unit (metres, unit quaternion x y z w, factors, curve values).
/// Errors: None; the writer validates (below).
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_AnimCurveChannelDesc
{
	Zenith_StringView m_strName;                 // the target joint name, or for path 4 the skeleton curve name; 1 … 63 bytes (127 for a curve)
	uint8 m_uPath = 0;                          // Zenith_ImportedPath: 1 translation, 2 rotation, 3 scale, 4 curve
	uint8 m_uInterpolation = 0;                 // 1 step, 2 linear (no cubic spline: a curve or pose source has no tangents)
	Zenith_Span<const float> m_afTimes;          // strictly increasing, ≥ 0, at most the clip duration
	Zenith_Span<const float> m_afValues;         // times.size() × width (width 3, 4, 3, 1 for paths 1 … 4)
};

/// @contract value Zenith_AnimCurveClipDesc
/// Ownership: Borrows the channel span for the writer call.
/// Thread affinity: IMMUTABLE for the call. Members non-const: callers default-construct it and set the fields they
///   need before passing it.
/// Invalidation: None.
/// Units: m_fDuration seconds.
/// Errors: None; the writer validates (below).
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_AnimCurveClipDesc
{
	Zenith_PersistentID m_xSkeletonProduct;      // the skel: product the clip animates
	float m_fDuration = 0.0f;                    // the source duration D, (0, fANIM_MAX_CLIP_DURATION]
	Zenith_Span<const Zenith_AnimCurveChannelDesc> m_axChannels;   // 1 … uANIM_MAX_INTERMEDIATE_CHANNELS
};

/// @contract functions Zenith_AnimCurveClipWriter
/// Ownership: Appends to the caller's array; owns nothing.
/// Thread affinity: OWNER_THREAD (the importer's).
/// Invalidation: None.
/// Units: Bytes of the ZICL intermediate layout of Formats.md "Clip intermediate".
/// Errors: ANIMATION_ERROR_LIMIT_EXCEEDED for a channel count past uANIM_MAX_INTERMEDIATE_CHANNELS or a key total past
///   uANIM_MAX_INTERMEDIATE_KEYS; ANIMATION_ERROR_INVALID_ASSET for a name of 0 or too many bytes, an unknown path or
///   interpolation, a value span of the wrong length, a non-finite value, a duration outside (0, 3,600] s, or times not
///   strictly increasing or outside [0, D]; nothing is appended then. (Zenith_AnimationImportError is ZenithCook-internal,
///   which ZenithAuthoring may not include, ruling AUTHORING-CONTRACT.)
/// Version: since Z2 Stage 4. Tool-side (CHAR's lip-sync and pose-book importers, row CH15).
/// @endcontract
Zenith_Result<void, Zenith_AnimationError> Zenith_WriteCurveClipIntermediate(const Zenith_AnimCurveClipDesc& xClip,
	Zenith_Array<uint8>& auOut);
```

`Zenith_WriteCurveClipIntermediate` (row CH15) writes the same `ZICL` layout as ZenithCook's `Zenith_WriteClipIntermediates` (`Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"`), header flag bit 0 clear, channels in the descriptor's order, for an importer whose source has no glTF node tables (a lip-sync or pose-book clip of another source's skeleton); the caller passes the bytes to `Zenith_ImportContext::WriteIntermediate` for its `anim:` product, and the ordinary `anim.clip` step cooks them. The layout stays ANIM's: the writer lives in `Zenith2/Tools/Authoring/Internal/Animation/Zenith_AnimCurveClipWriter.cpp` and shares no code with ZenithCook's writer (ZenithAuthoring cannot include ZenithCook's `Internal/`); `Test_AnimImport_CurveClipIntermediateMatchesReader` reads its output with ZenithCook's `Zenith_ReadClipIntermediate` so the two writers cannot drift.

`Zenith_AnimGraphDiagnostic` (code, severity, node name, stable ID, message text) and `Zenith_AnimBlendSpaceTriangulation` (node stable ID, the triangles as sample-index triples) are declared in the same header with their contract blocks. A caller first calls `Zenith_ListAnimationGraphInputs` to learn the skeleton, masks and clips the document names (the cook step reads each through its context, recording the cook-input edges of "Cook steps and derived-data keys"), then `Zenith_CompileAnimationGraph` with those inputs; the cook step writes the result's sections into the `.zagraph` through its context's cooked-file writer, publishes `anim.graphInfo`, and reports each diagnostic through `Zenith_CookContext::Report`, and the editor shows the diagnostics on their nodes and the triangulations on the blend-space canvas. The compiler is a pure function of its inputs, so the cook and the editor's preview compile produce bit-identical sections for identical inputs, with one exception settled by ruling ANIM-F19-VENUE: the motion-matching database. Its build runs the clip cook's stages 2 and 3, which are ZenithCook functions the tool library may not call (ruling AUTHORING-CONTRACT), so the `anim.graph` cook step builds the section with `Zenith_MotionMatchDatabase::BuildFromFrames` and passes its bytes as `m_xMotionDatabase`, which the compiler writes as section `0x0312` and includes in the graph hash; the editor's preview compile passes an empty span, the graph loader refuses a kind-15 node without the section (`ANIMATION_ERROR_CORRUPT`), and the editor previews a graph with a kind-15 node from the cook server's output instead. ZenithAuthoring declares `ZenithAnimation` in its `DEPENDS` (the Stage 4 entry of EDIT's growth table; this move brings it forward to the Stage 3 graph-compiler ticket, the first diff that includes an ANIM header there). `ZenithAuthoring_Tests` runs the compiler and document tests of `Zenith2/Docs/Design/ANIM/Tests.md → "Import and cook tests"` in the `Test` step.

### Integration suite and benchmark files

```
Zenith2/Tests/Animation/
	CMakeLists.txt                        one zenith_add_integration_tests call
	Zenith_AnimDeterminism_Tests.cpp      Tests.md "Determinism tests"
	Zenith_AnimIntegration_Tests.cpp      Tests.md "Integration scenarios"
	Internal/Zenith_AnimSuiteWorlds.cpp   world, physics and content setup from the generators
Zenith2/Benchmarks/AnimEvaluation/         CMakeLists.txt (zenith_add_benchmark(AnimEvaluation …)), Internal/Zenith_BenchAnimEvaluationMain.cpp
Zenith2/Benchmarks/ClipSampling/           CMakeLists.txt (zenith_add_benchmark(ClipSampling …)), Internal/Zenith_BenchClipSamplingMain.cpp
Zenith2/Benchmarks/ClipCompression/        CMakeLists.txt (zenith_add_benchmark(ClipCompression DEPENDS ZenithCook_Objects …)), Internal/Zenith_BenchClipCompressionMain.cpp
Zenith2/Benchmarks/Budgets/ANIM/           the Stage 1 per-system budget harness for ANIM
Zenith2/Benchmarks/MotionMatching/         the D-SEL-MOTIONMATCH (open, Stage 3) harness
```

### ZENITH_SUPPORTED matrix

`ZenithAnimation` declares `ZENITH_GPU` (`win`, `win-asan`, `android-arm64`, `android-x86_64`); it is never in `linux-tsan`, so the parallel evaluation's stress tests run under `win-asan` and on the S24 under HWASan in stage acceptance, and ANIM's job-graph paths rely on JOBS' and ECS's TSan coverage of the scheduler they use. The tests run headless in the `Test` step.

## Public API

### Header map

| Header | Declares | Specified in |
|---|---|---|
| `Zenith_Module_Animation.h` | `Zenith_Module_Animation_RegisterTypes`, `Zenith_Module_Animation_Start` | "Zenith_Module_Animation header" |
| `Zenith_AnimationErrors.h` | `Zenith_AnimationError`, `Zenith_AnimationCookDetail` | "Zenith_AnimationErrors header" |
| `Zenith_AnimationTypes.h` | constants, enumerations | "Zenith_AnimationTypes header" |
| `Zenith_AnimationMath.h` | `Zenith_DualQuaternion` and its functions | "Zenith_AnimationMath header" |
| `Zenith_AnimationComponents.h` | the six components | "Zenith_AnimationComponents header" |
| `Zenith_SkeletonAsset.h`, `Zenith_AnimationClip.h`, `Zenith_AnimationGraphAsset.h` | asset objects, clip sampling | `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_SkeletonAsset header"`, `"Zenith_AnimationClip header"`, `"Zenith_AnimationGraphAsset header"` |
| `Zenith_AnimationPose.h` | pose views and operations | `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPose header"` |
| `Zenith_AnimationWorld.h` | the two world resources and their read API | `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"` |
| `Zenith_AnimationCommands.h`, `Zenith_AnimationEvents.h` | command recording, event payloads | `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"`, `"Zenith_AnimationEvents header"` |
| `Zenith_AnimationIK.h`, `Zenith_AnimationRetarget.h`, `Zenith_AnimationPreview.h` | solvers, retarget maps, the preview | `Zenith2/Docs/Design/ANIM/Runtime.md` |
| `Zenith_AnimationMotionMatch.h` | `Zenith_MotionMatcher` and its database | `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"` |

### Rules across the ANIM API

- Every fallible public function returns `Zenith_Result<…, Zenith_AnimationError>`; every public type carries one contract block ([Z2-N-CONTRACT-111]).
- No public header includes a third-party header, an STL type, a ZenithScript header or a ZenithDocument header.
- ANIM's simulation-visible state (instances, poses, parameters, residency) is written only by ANIM's phase-3 and phase-6 nodes, its sync-point command playback and listener, and its tick-boundary publication handler; every other writer uses a command.
- Content never reaches an assert: cooked files, intermediates, documents and script arguments fail with results or controlled Luau errors ([Z2-N-CONTRACT-472], [Z2-N-CONTRACT-473]).
- No ANIM type holds mutable global state; each world's objects belong to the module instance ([Z2-N-CONTRACT-141]).
- Every blend, sample and solve is bit-reproducible per platform and build: scalar IEEE operations in a fixed order, no fast-math ([Z2-N-ARCH-883], `Zenith2/Docs/Design/CORE/Maths.md → "Numerical conventions"`).

### Zenith_Module_Animation header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_Module_Animation.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_Animation
/// Ownership: RegisterTypes registers ANIM's reflected types, command payload types, events and configuration keys; Start
///   creates the module instance the runtime owns.
/// Thread affinity: MAIN_THREAD, at startup steps 6 and 15.
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION when a registration is rejected (logged by the context); MODULE_ERROR_OUT_OF_MEMORY
///   when the instance or a world's resources cannot be allocated.
/// Version: since Z2 Stage 3. Module entry points per [Z2-N-ARCH-168].
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Animation_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Animation_Start(
	Zenith_ModuleStartContext& xContext);
```

| Entry point step | Action | Failure |
|---|---|---|
| RegisterTypes 1 | the components of "Zenith_AnimationComponents header" (Stage 3: Animator, RootMotion, Pose; Stage 4: IKGoals, MotionWarp, PoseFollower), the struct types `Zenith.Animation.IKGoal` and `Zenith.Animation.WarpTarget`, the resource types `Zenith.Animation.Residency` and `Zenith.Animation.World` (owning phase `PHASE_ANIMATION`) and `Zenith.Animation.FinalPoses` (`PHASE_POST_PHYSICS`) ("Resource partition" of `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"`), the event types `Zenith.Animation.Notify` and `Zenith.Animation.StateChanged`, the nine command payload structs | `MODULE_ERROR_REGISTRATION` |
| RegisterTypes 2 | the `animation.*` configuration keys of "Configuration keys" through `GetConfigSchema` | `MODULE_ERROR_REGISTRATION` |
| Start 1 | allocate the module instance; read the configuration | `MODULE_ERROR_OUT_OF_MEMORY` |
| Start 2 | register the three kind loaders | `MODULE_ERROR_REGISTRATION` |
| Start 3 | register the nine command kinds of `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"`, recordable in phases 1 to 10 | `MODULE_ERROR_REGISTRATION` |
| Start 4 | register the `Animation` binding table and the memory consumer | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 1 | create the world's `Zenith_AnimationResidency`, `Zenith_AnimationWorld` and `Zenith_AnimationFinalPoses`, add them as world resources with their owning phases, create the completion queue, declare the publication source with `uMaxInFlight = animation.bindingQueueCapacity + animation.streaming.maxReadsInFlight` and `uMaxResultBytes = 24` (row AN1), install the ragdoll pose source on the physics world resource (row AN6), register the world's save participant (row AN8) | `MODULE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_REGISTRATION` |
| AttachWorld 2 | add the six systems of `Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"` with their declarations | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 3 | add the lifecycle listener | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 4 | add the extractor `Animation.ExtractPoses` (in every configuration, headless included) | `MODULE_ERROR_REGISTRATION` |

The module registers no document type (they are tool-side, "Migration and re-cook"). Each step has a failure-injection test, `Test_AnimationModule_Create_FailsAt<Step>_Unwinds` (ten tests, `Zenith2/Docs/Design/ANIM/Tests.md → "Module binding and failure tests"`); the runtime undoes the registrations in reverse ([Z2-N-ARCH-851]). `DetachWorld` cancels the world's outstanding binding and streaming operations (`RequestCancel` on each), then, before it destroys any resource, completes through the sink, in ascending completion ID, every completion ID ANIM allocated in the world and has not yet completed (issued requests, abandoned ones not yet completed) with the zero-length `PUBLICATION_KIND_GENERIC_RECORDED` abandoned payload, so the world's table never holds an ANIM ID its producer left uncompleted (the producer obligation of [Z2-N-SCRIPT-931], core-doc item X255, and GAME's row GA19; a result already parked is complete and is left to the table, which discards and counts it); ANIM counts no discard itself; then it drains the operations until each reports complete, unregisters the save participant and destroys the world's resources.

### Zenith_AnimationErrors header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_AnimationError
/// Ownership: Plain enumeration (the enumeration form of [Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The meaning of each value is its comment; it is also the uint16 code a kind loader passes to Fail.
/// Version: since Z2 Stage 3. Values keep their meaning once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_AnimationError : uint8
{
	ANIMATION_ERROR_OUT_OF_MEMORY = 1,     // an allocation failed
	ANIMATION_ERROR_CORRUPT,               // a cooked file violated a format rule (Formats.md validation tables)
	ANIMATION_ERROR_INVALID_ASSET,         // an asset is of the wrong kind or state for the call
	ANIMATION_ERROR_SKELETON_MISMATCH,     // a clip, graph or mesh is for another skeleton or another rest pose
	ANIMATION_ERROR_NO_INSTANCE,           // no animator instance for the simulation ID
	ANIMATION_ERROR_NOT_BOUND,             // the instance's assets have not been published yet
	ANIMATION_ERROR_UNKNOWN_PARAMETER,     // the bound graph has no parameter of that name
	ANIMATION_ERROR_PARAMETER_KIND,        // the parameter has another kind
	ANIMATION_ERROR_UNKNOWN_NAME,          // no slot, machine, state, curve, socket or joint of that name
	ANIMATION_ERROR_CLIP_NOT_RESIDENT,     // a needed clip segment is not resident
	ANIMATION_ERROR_RETARGET_UNSUPPORTED,  // no mapping for a required target joint
	ANIMATION_ERROR_LIMIT_EXCEEDED,        // a documented limit was exceeded
	ANIMATION_ERROR_NOT_AVAILABLE,         // a development-only call in a Shipping build
};
constexpr const char* Zenith_ToString(Zenith_AnimationError eError);

/// @contract enum Zenith_AnimationCookDetail
/// Ownership: Plain enumeration; the m_uDetail of a Zenith_CookError ANIM's steps return (COOK_ERROR_STEP_FAILED,
///   COOK_ERROR_IMPORT_FAILED) and of Zenith_AnimationImportError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; each value names the rule of Formats.md that failed.
/// Version: since Z2 Stage 3. Tool-side meaning; values keep their meaning.
/// @endcontract
enum Zenith_AnimationCookDetail : uint16
{
	ANIM_COOK_DETAIL_JOINT_LIMIT = 1, ANIM_COOK_DETAIL_HIERARCHY, ANIM_COOK_DETAIL_DUPLICATE_JOINT_NAME,
	ANIM_COOK_DETAIL_NAME_HASH_COLLISION, ANIM_COOK_DETAIL_REST_POSE_INVALID, ANIM_COOK_DETAIL_REST_POSE_SHEAR,
	ANIM_COOK_DETAIL_RIG_INVALID, ANIM_COOK_DETAIL_REST_POSE_CONFLICT, ANIM_COOK_DETAIL_LOOP_NOT_CLOSED,
	ANIM_COOK_DETAIL_ROOT_MOTION_ANCESTOR_ANIMATED, ANIM_COOK_DETAIL_ERROR_BOUND_VIOLATED, ANIM_COOK_DETAIL_SKELETON_MISMATCH,
	ANIM_COOK_DETAIL_GRAPH_INVALID, ANIM_COOK_DETAIL_DOCUMENT_INVALID, ANIM_COOK_DETAIL_RETARGET_UNSUPPORTED,
	ANIM_IMPORT_DETAIL_DUPLICATE_JOINT_NAME = 100, ANIM_IMPORT_DETAIL_REST_POSE_CONFLICT, ANIM_IMPORT_DETAIL_REST_POSE_SHEAR,
	ANIM_IMPORT_DETAIL_LIMIT,
};
constexpr const char* Zenith_ToString(Zenith_AnimationCookDetail eDetail);
```

`ANIM_COOK_WARNING_LOOP_DISCONTINUITY` and `ANIM_COOK_WARNING_MARKERS_MISSING` are the step's two warnings, and `ANIM_COOK_WARNING_RENAME_ASSUMED`, `ANIM_COOK_WARNING_RENAME_NO_EVIDENCE` and `ANIM_COOK_WARNING_RENAME_AMBIGUOUS` (values 3, 4, 5) the import helpers' rename-recognition warnings (`Zenith2/Docs/Design/ANIM/Formats.md → "Product fingerprints and rename recognition"`); all are tool-side `Zenith_AnimationCookWarning` values, reported, never failing a cook.

### Zenith_AnimationTypes header

<!-- elaboration -->
| Constant | Value | Meaning |
|---|---|---|
| `uANIM_MAX_JOINTS` | 1,024 | joints per skeleton (equals GEOM's `uGEOM_MAX_SKELETON_JOINTS`) |
| `uANIM_MAX_CURVES` | 512 | curves per skeleton |
| `uANIM_SEGMENT_FRAMES` | 16 | frames per clip segment |
| `uANIM_BIT_RATE_COUNT` | 19 | entries of the bit-rate table |
| `uANIM_POSE_STACK_DEPTH` | 16 | the pose-stack limit |
| `uANIM_MAX_ACTIVE_TRANSITIONS` | 4 | transition records per machine |
| `uANIM_MAX_ZERO_DURATION_TRANSITIONS_PER_EVALUATION` | 4 | the zero-duration chain bound |
| `uANIM_MAX_EVENTS_PER_INSTANCE_PER_EVALUATION` | 64 | the event cap |
| `uANIM_MAX_IK_GOALS`, `uANIM_MAX_WARP_TARGETS`, `uANIM_MAX_SLOTS`, `uANIM_MAX_LAYERS` | 8, 4, 8, 8 | component and graph limits |
| `uANIM_MAX_CURVE_OVERRIDES` | 8 | per instance |
| `uANIM_JOINT_ROLE_COUNT` | 54 | defined joint roles |
| `uANIM_INVALID_INDEX`, `uANIM_UNMAPPED_JOINT` | `0xFFFFFFFF`, `0xFFFFFFFE` | lookup sentinels |
| `fANIM_WEIGHT_EPSILON` | 1e-4 | the culling threshold |
| `fANIM_REST_POSE_TOLERANCE` | 1e-5 | the rest-pose agreement tolerance (relative) |
| `uANIM_SKELETON_FORMAT_VERSION`, `uANIM_CLIP_FORMAT_VERSION`, `uANIM_GRAPH_FORMAT_VERSION` | 1, 1, 1 | the cooked format versions |
| `uANIM_SAVE_RECORD_VERSION` | 1 | the save record version |
| `ANIM_SLOT_TAG_FULL_RATE`, `ANIM_SLOT_TAG_CINEMATIC` | `1 << 14`, `1 << 15` | slot tags: an active slot carrying either forces rate divisor 1 (`Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"`); bit 15 is the cinematic writer's, bit 14 any issuer's (CHAR's lip-sync contract, row CH16); bits 0 … 13 are the issuer's own |
| `uANIM_MAX_REPORTED_KEYS` | 4,096 | per world: the once-only report keys a node remembers (`Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"`, "Counter and statistics merge") |

The enumerations of the header, each with its contract block, are those the section files define: `Zenith_JointRole`, `Zenith_ChainRole`, `Zenith_IKKind`, `Zenith_CurveKind`, `Zenith_MirrorAxis`, `Zenith_TrackKind`, `Zenith_RootMotionMode` (`ROOT_MOTION_MODE_NONE`, `_XZ_YAW`, `_XYZ_YAW`, `_FULL` for clips; the animator's `ANIMATOR_ROOT_MOTION_IGNORE`, `_EXTRACT` are `Zenith_AnimatorRootMotion`), `Zenith_AdditiveReference`, `Zenith_AdditiveSpace`, `Zenith_EventFireMode`, `Zenith_AnimNodeKind`, `Zenith_AnimParameterKind`, `Zenith_ConditionOp`, `Zenith_InterruptSource`, `Zenith_BlendCurve`, `Zenith_LayerBlendMode`, the clip flags `ANIM_CLIP_FLAG_*`, the event flags `ANIM_EVENT_FLAG_*` and the animator flags `ANIMATOR_FLAG_FORCE_FULL_RATE` (bit 0), `ANIMATOR_FLAG_ALLOW_REDUCED_ROOT_MOTION` (bit 1), `ANIMATOR_FLAG_RESET_ON_ENABLE` (bit 2), `ANIMATOR_FLAG_ALWAYS_UPDATE_LAYERS` (bit 3).

### Zenith_AnimationMath header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationMath.h
#pragma once
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Span.h"

/// @contract value Zenith_DualQuaternion
/// Ownership: Plain value: a real part (the rotation) and a dual part (half the translation times the rotation).
/// Thread affinity: IMMUTABLE. Members non-const: an arithmetic value assigned by every operation that produces one.
/// Invalidation: None.
/// Units: The rigid transform's rotation (unit quaternion) and translation (metres).
/// Errors: Functions expect unit dual quaternions (ZENITH_ASSERT |real| within 1e-3 of 1); Normalize of a zero real part is
///   the checked NormalizeOrIdentity.
/// Version: since Z2 Stage 3. Layout: two Zenith_Quaternion (x, y, z, w each); never serialized.
/// @endcontract
struct Zenith_DualQuaternion
{
	Zenith_Quaternion m_xReal;
	Zenith_Quaternion m_xDual{ 0.0f, 0.0f, 0.0f, 0.0f };
};

/// @contract functions Zenith_DualQuaternionFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres and radians.
/// Errors: As Zenith_DualQuaternion; spans of unequal length are ZENITH_ASSERT.
/// Version: since Z2 Stage 3. Bit-identical per platform and build.
/// @endcontract
Zenith_DualQuaternion Zenith_MakeDualQuaternion(const Zenith_Quaternion& xRotation, const Zenith_Vector3& xTranslation);
void Zenith_DecomposeDualQuaternion(const Zenith_DualQuaternion& xDQ, Zenith_Quaternion& xRotationOut, Zenith_Vector3& xTranslationOut);
Zenith_DualQuaternion Zenith_MultiplyDualQuaternions(const Zenith_DualQuaternion& xA, const Zenith_DualQuaternion& xB);   // B then A
Zenith_DualQuaternion Zenith_InverseDualQuaternion(const Zenith_DualQuaternion& xDQ);
Zenith_DualQuaternion Zenith_NormalizeDualQuaternion(const Zenith_DualQuaternion& xDQ);
Zenith_DualQuaternion Zenith_BlendDualQuaternions(Zenith_Span<const Zenith_DualQuaternion> axInputs,
	Zenith_Span<const float> afWeights);            // sign-aligned to the first input, weighted sum, normalised
Zenith_DualQuaternion Zenith_PowDualQuaternion(const Zenith_DualQuaternion& xDQ, float fExponent);   // screw power
Zenith_Vector3 Zenith_TransformPoint(const Zenith_DualQuaternion& xDQ, const Zenith_Vector3& xPoint);
bool Zenith_IsFinite(const Zenith_DualQuaternion& xDQ);
```

| Function | Definition |
|---|---|
| `Zenith_MakeDualQuaternion(q, t)` | real `q`; dual `0.5 × (t, 0) * q` (Hamilton product, CORE's convention) |
| `Zenith_DecomposeDualQuaternion` | rotation the real part; translation the vector part of `2 × dual * conjugate(real)` |
| `Zenith_NormalizeDualQuaternion` | `n = |real|`; `real /= n`; `dual /= n`; `dual −= real × dot(real, dual)` |
| `Zenith_BlendDualQuaternions` | negate input i when `dot(real_i, real_0) < 0`; `Σ w_i × dq_i` in input order; normalise; the identity when every weight is 0 |
| `Zenith_PowDualQuaternion(dq, e)` | "Screw power" below |

<!-- elaboration -->
**Screw power.** `Zenith_PowDualQuaternion(dq, e)` raises a unit dual quaternion to a real power along its screw axis, in `double`, rounded to `float` once at the end:

1. Canonicalise the sign: a unit dual quaternion and its negation are the same rigid transform, so when `real.w < 0` negate the real and the dual part; when `real.w = 0` exactly, negate them when the first non-zero of `real.x`, `real.y`, `real.z` is negative. After this step `real.w ≥ 0`, so `dq` and `−dq` give bit-identical results and the angle is the short way round.
2. Split: `v = real.xyz`, `s = |v|` (`= sin(θ/2)`), `θ = 2 atan2(s, real.w) ∈ [0, π]`; translation `t = 2 × vector(dual * conjugate(real))`.
3. Rotation part: `real' = (k × v, cos(e θ / 2))` with `k = sin(e θ / 2) / s` when `θ ≥ 1e-3`, and the series `k = e × (1 + (1 − e²) θ² / 24)` when `θ < 1e-3` (no division by s).
4. Translation part, the screw motion's translation at fraction e written without the axis l, so no term divides by a vanishing `sin(θ/2)`: `t' = c1 × t + c2 × dot(t, v) × v + c3 × cross(v, t)`, where for `θ ≥ 1e-3`, with `a = 0.5 × (1 − cos(e θ) + cot(θ/2) × sin(e θ))` and `b = 0.5 × (cot(θ/2) × (1 − cos(e θ)) − sin(e θ))`: `c1 = a`, `c2 = (e − a) / s²`, `c3 = b / s`; and for `θ < 1e-3` the series `c1 = e + θ² × (e²/4 − e³/6 − e/12)`, `c2 = e (1 − e)(1 − 2e) / 3`, `c3 = e (e − 1) (1 + θ² (1 + 2e − 2e²) / 24)`. Measured against the closed form at the threshold (θ → 1e-3), the series' truncation error in `t'` is below `5e-14 × |t|` for e ∈ [−0.5, 2] (4.2e-14 at e = 2, dominated by c1's θ⁴ term); without c3's θ² term it would be `7.8e-12 × |t|` for e ∈ [−0.5, 1.5] and `1.25e-10 × |t|` at e = 2, the O(θ³) residue of `c3 × cross(v, t)`. The rotation series' error is below `6e-15` (5.2e-15 at e = 2). ANIM calls the function with e ∈ [0, 1] (recover offsets, the spread step 1/k) and its tests use e ∈ [−0.5, 2]; both errors are far below f32 resolution, so the float result is continuous across the branch to within its own rounding. Both branches are one internal function `Zenith_ComputeScrewCoefficients(double fTheta, double fExponent, bool bSeries)` in `Internal/Pose/Zenith_ScrewPower.h`, which `Zenith_AnimationTestAccess` exposes so a test can evaluate both at one θ. At `θ = 0` this is `t' = e t`, the pure translation scaled.
5. `dual' = 0.5 × (t', 0) * real'`; normalise (`Zenith_NormalizeDualQuaternion` in `double`), round once.

`e = 0` gives the identity and `e = 1` the canonicalised input; `Pow(dq, 1/k)` composed k times reproduces dq within 1e-5. `Test_DualQuaternion_PowCanonicalisesSign`, `Test_DualQuaternion_PowNearIdentityFinite`, `Test_DualQuaternion_PowSeriesBranchContinuity` and `Test_DualQuaternion_PowHalfTurn` cover the sign, the small-angle branch, the continuity of the two branches and θ = π (`Zenith2/Docs/Design/ANIM/Tests.md → "Blend and pose tests"`).

`Zenith2/Docs/Design/ANIM/Graph.md → "Pose blending"` (subsection "Rigid delta blending") states when ANIM uses these functions; GEOM's GPU skinning stays linear blend skinning.

### Zenith_AnimationComponents header

Engine component types are C++-registered structs (the orchestrator's ruling for engine components, the mechanism of `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`), registered by `Zenith_Module_Animation_RegisterTypes` with `BeginComponent<T>(stableName, 1)` (`Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeBuilder"`):

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationComponents.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithAnimation/Zenith_AnimationGraphAsset.h"   // complete: Zenith_AssetHandle checks the asset type in its body

/// @contract value Zenith_AnimatorComponent
/// Ownership: Component value stored through "Zenith.Animation.Animator"; the graph handle holds one reference, copied and
///   destroyed through the type's lifecycle hooks ([Z2-N-ARCH-645]).
/// Thread affinity: As every component: authority PHASE_COMMANDS_ONLY (every change is a command); read by ANIM's phase-3
///   nodes and its extractor.
/// Invalidation: None.
/// Units: m_fPlayRate a factor; m_fSignificanceBias a factor.
/// Errors: None; ANIM clamps values at use and refuses an unbindable graph ("Asset bindings").
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_AnimatorComponent
{
	Zenith_AssetHandle<Zenith_AnimationGraphAsset> m_xGraph;   // 1 Graph
	float m_fPlayRate = 1.0f;                                  // 2 PlayRate, [0, 16]
	uint8 m_uRootMotion = ANIMATOR_ROOT_MOTION_EXTRACT;       // 3 RootMotion: Zenith_AnimatorRootMotion
	bool m_bEnabled = true;                                    // 4 Enabled
	float m_fSignificanceBias = 1.0f;                          // 5 SignificanceBias, [0.01, 100]
	uint32 m_uFlags = 0;                                      // 6 Flags: ANIMATOR_FLAG_*
	static const Zenith_TypeID xTYPE_ID;                       // "Zenith.Animation.Animator"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_RootMotionComponent
/// Ownership: Component value stored through "Zenith.Animation.RootMotion"; transient (no property serialized).
/// Thread affinity: Authority PHASE_ANIMATION: written by Animation.Evaluate alone; read in phase 4 by PHYS's character-controller
///   node (rows with a controller) and by Animation.ApplyRootMotion (rows without one).
/// Invalidation: None.
/// Units: DeltaPosition in entity-local metres: the skeleton model-space displacement already multiplied component-wise by
///   the entity's Zenith.LocalTransform scale when Animation.Evaluate writes it, so every reader maps it into the parent's
///   space by the entity's rotation alone (r · DeltaPosition, as Animation.ApplyRootMotion and PHYS's Physics.CharacterMove
///   both do) and no reader scales it; DeltaRotation a unit quaternion in the entity's local frame; both at the start of the tick.
/// Errors: None.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_RootMotionComponent
{
	Zenith_Vector3 m_xDeltaPosition = xVECTOR3_ZERO;           // 1 DeltaPosition
	Zenith_Quaternion m_xDeltaRotation;                        // 2 DeltaRotation
	uint32 m_uFlags = 0;                                      // 3 Flags: bit 0 valid, bit 1 reset (teleport), bit 2 spread step
	static const Zenith_TypeID xTYPE_ID;                       // "Zenith.Animation.RootMotion"
};

/// @contract value Zenith_PoseComponent
/// Ownership: Component value stored through "Zenith.Animation.Pose"; transient.
/// Thread affinity: Authority PHASE_POST_PHYSICS, arbitrated: ARBITRATION_RULE_PRECEDENCE over (Cinematic, Ragdoll,
///   Animation); writers Animation.PostPhysicsOwnership (ANIMATION), Animation.RagdollOwnership (RAGDOLL), CINE's cinematic
///   writer (CINEMATIC); read by Animation.PostPhysicsBlend after the arbitration node.
/// Invalidation: None.
/// Units: m_fPhysicsWeight a fraction: 0 animation owns the bones, 1 the ragdoll does.
/// Errors: None.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_PoseComponent
{
	float m_fPhysicsWeight = 0.0f;                             // 1 PhysicsWeight
	static const Zenith_TypeID xTYPE_ID;                       // "Zenith.Animation.Pose"
};

/// @contract value Zenith_IKGoal
/// Ownership: Reflected struct "Zenith.Animation.IKGoal", an element of Zenith_IKGoalsComponent.
/// Thread affinity: IMMUTABLE as a value. Members non-const: a reflected type, which the type registry
///   default-constructs and writes property by property.
/// Invalidation: None.
/// Units: World metres and a unit quaternion, or offsets from m_xTargetEntity's world transform when it is set.
/// Errors: None; non-finite values make the goal inactive and are counted.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_IKGoal
{
	Zenith_Vector3 m_xPosition = xVECTOR3_ZERO;                // 1 Position
	Zenith_Quaternion m_xRotation;                             // 2 Rotation
	float m_fWeight = 0.0f;                                    // 3 Weight, [0, 1]
	uint32 m_uFlags = 0;                                      // 4 Flags: bit 0 active, bit 1 apply rotation
	Zenith_EntityHandle m_xTargetEntity;                       // 5 TargetEntity (changed through commands)
};

/// @contract value Zenith_IKGoalsComponent
/// Ownership: Component value stored through "Zenith.Animation.IKGoals".
/// Thread affinity: Authority PHASE_EARLY_SCRIPTS (behaviours and chunk systems write it in phase 2; others by command).
/// Invalidation: None.
/// Units: As Zenith_IKGoal.
/// Errors: None.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_IKGoalsComponent
{
	Zenith_IKGoal m_axGoals[uANIM_MAX_IK_GOALS];               // 1 Goals (fixed array of 8)
	static const Zenith_TypeID xTYPE_ID;                       // "Zenith.Animation.IKGoals"
};

/// @contract value Zenith_WarpTarget
/// Ownership: Reflected struct "Zenith.Animation.WarpTarget".
/// Thread affinity: IMMUTABLE as a value. Members non-const: a reflected type, which the type registry
///   default-constructs and writes property by property.
/// Invalidation: None.
/// Units: World metres and a unit quaternion.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WarpTarget
{
	uint64 m_ulWindowHash = 0;                                // 1 Window: Zenith_Hash64 of the window name (without "Warp.")
	Zenith_Vector3 m_xPosition = xVECTOR3_ZERO;                // 2 Position
	Zenith_Quaternion m_xRotation;                             // 3 Rotation
	uint32 m_uFlags = 0;                                      // 4 Flags: bit 0 active, bit 1 translation only
};

/// @contract value Zenith_MotionWarpComponent
/// Ownership: Component value stored through "Zenith.Animation.MotionWarp".
/// Thread affinity: Authority PHASE_EARLY_SCRIPTS.
/// Invalidation: None.
/// Units: As Zenith_WarpTarget.
/// Errors: None.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_MotionWarpComponent
{
	Zenith_WarpTarget m_axTargets[uANIM_MAX_WARP_TARGETS];     // 1 Targets (fixed array of 4)
	static const Zenith_TypeID xTYPE_ID;                       // "Zenith.Animation.MotionWarp"
};

/// @contract value Zenith_PoseFollowerComponent
/// Ownership: Component value stored through "Zenith.Animation.PoseFollower".
/// Thread affinity: Authority PHASE_COMMANDS_ONLY; read by the extractor.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an invalid leader is refused at extraction and counted.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_PoseFollowerComponent
{
	Zenith_EntityHandle m_xLeader;                             // 1 Leader
	static const Zenith_TypeID xTYPE_ID;                       // "Zenith.Animation.PoseFollower"
};
```

### Component registration

| Stable name | Struct | Properties (ID name) | Default authority | Serialized | Stage |
|---|---|---|---|---|---|
| `Zenith.Animation.Animator` | `Zenith_AnimatorComponent` | 1 Graph (asset `Animation.Graph`), 2 PlayRate (range 0 … 16), 3 RootMotion, 4 Enabled, 5 SignificanceBias (0.01 … 100), 6 Flags | `CommandsOnly` | yes | S3 |
| `Zenith.Animation.RootMotion` | `Zenith_RootMotionComponent` | 1 DeltaPosition, 2 DeltaRotation, 3 Flags | `Animation` | no | S3 |
| `Zenith.Animation.Pose` | `Zenith_PoseComponent` | 1 PhysicsWeight (0 … 1), arbitrated | `PostPhysics` | no | S3 |
| `Zenith.Animation.IKGoals` | `Zenith_IKGoalsComponent` | 1 Goals (8 × `Zenith.Animation.IKGoal`: 1 Position, 2 Rotation, 3 Weight, 4 Flags, 5 TargetEntity) | `EarlyScripts` | yes | S4 |
| `Zenith.Animation.MotionWarp` | `Zenith_MotionWarpComponent` | 1 Targets (4 × `Zenith.Animation.WarpTarget`: 1 Window, 2 Position, 3 Rotation, 4 Flags) | `EarlyScripts` | yes | S4 |
| `Zenith.Animation.PoseFollower` | `Zenith_PoseFollowerComponent` | 1 Leader | `CommandsOnly` | yes | S4 |

Property IDs are never reused ([Z2-N-ARCH-641]); the type IDs are `Zenith_MakeTypeID` of the stable names. A scene document that lists `Zenith.Animation.RootMotion` or `Zenith.Animation.Pose` gets them default-initialised (their properties are not serialized); the animator adds neither implicitly: the prefab or scene authoring an animated character lists all three Stage 3 components.

**Authority census rows.** Every ticket that registers one of these types adds its rows to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the same diff, in the twelve-column format of `Zenith2/Docs/Design/GAME/PhaseContract.md → "Census format"` (GAME's change-control rule 3, `Zenith2/Docs/Design/GAME/PhaseContract.md → "Census rows added by subsequent designs"`), because GAME's census test of the suite that links ZenithAnimation, `Test_AuthorityCensus_CharacterModules` (Stage 3 suite; and `Test_AuthorityCensus_OpenWorldModules` at Stage 4), fails the first build that registers a property the census lacks; the Stage 2 `Test_Gameplay_AuthorityCensusMatchesRegistry` links no module and never sees these rows (GAME's row GA20). The module-skeleton ticket adds the Stage 3 rows (the first three components, the three world resources, both events) and the change to SCENE's `Zenith.LocalTransform` row; the Stage 4 component ticket adds the last three components. The rows, with `Stage` the registering ticket's stage:

| Area | Library | Stage | Stable name | Kind | Properties | Owning phase | Writer sources | Arbitration | Writers (system names) | Readers outside the owning phase | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.Animator` | component | 1 Graph, 2 PlayRate, 3 RootMotion, 4 Enabled, 5 SignificanceBias, 6 Flags | CommandsOnly | — | none | commands (scripts, cinematics, the editor) | `Animation.Significance`, `Animation.Evaluate` (3), `Animation.ApplyRootMotion` (4), `Animation.ExtractPoses` (12) | `Zenith2/Docs/Design/ANIM.md` → "Zenith_AnimationComponents header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.RootMotion` | component | 1 DeltaPosition, 2 DeltaRotation, 3 Flags | Animation | — | none | `Animation.Evaluate` | `Physics.CharacterMove` (4, rows with a character controller), `Animation.ApplyRootMotion` (4, rows without one) | `Zenith2/Docs/Design/ANIM.md` → "Zenith_AnimationComponents header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.Pose` | component | 1 PhysicsWeight | PostPhysics | Animation, Ragdoll, Cinematic | Precedence (Cinematic, Ragdoll, Animation) | `Animation.PostPhysicsOwnership`, `Animation.RagdollOwnership`, CINE's cinematic writer | none (`Animation.PostPhysicsBlend` reads it in phase 6) | `Zenith2/Docs/Design/ANIM.md` → "Zenith_AnimationComponents header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.Residency` | resource | — | Animation | — | none | `Animation.Significance` (through its merge node); ANIM's publication handler and lifecycle listener | `Animation.PostPhysicsBlend` (6), `Animation.ExtractPoses` (12), PHYS's `Physics.Step` pose-source drive reads (5) and ragdoll enter playback (sync points), scripts' read-world bindings (2, 10), the save participant's snapshot (11) | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationWorld header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.World` | resource | — | Animation | — | none | `Animation.Evaluate` (through its merge node); ANIM's publication handler and lifecycle listener | `Animation.PostPhysicsBlend` (6), PHYS's `Physics.Step` pose-source drive reads (5), scripts' read-world bindings (2, 10), the save participant's snapshot (11) | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationWorld header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.FinalPoses` | resource | — | PostPhysics | — | none | `Animation.PostPhysicsBlend`; ANIM's publication handler and lifecycle listener | `Animation.ExtractPoses` (12), scripts' socket reads (10, and 2 of the next tick), PHYS's ragdoll enter playback through `GetEntryModelPose` (sync points), the save participant's snapshot (11) | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationWorld header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.Notify` | event | 1–11 | — | — | none | `Animation.Evaluate` (emits) | phase-9 consumers (AUDIO, VFX, GAME, scripts) | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationEvents header" |
| ANIM | `ZenithAnimation` | S3 | `Zenith.Animation.StateChanged` | event | 1–6 | — | — | none | `Animation.Evaluate` (emits) | phase-9 consumers (scripts, EDIT's debugger) | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationEvents header" |
| ANIM | `ZenithAnimation` | S4 | `Zenith.Animation.IKGoals` | component | 1 Goals | EarlyScripts | — | none | behaviours and chunk systems (2); `SetProperty` commands | `Animation.Evaluate` (3) | `Zenith2/Docs/Design/ANIM.md` → "Zenith_AnimationComponents header" |
| ANIM | `ZenithAnimation` | S4 | `Zenith.Animation.MotionWarp` | component | 1 Targets | EarlyScripts | — | none | behaviours and chunk systems (2); `SetProperty` commands | `Animation.Evaluate` (3) | `Zenith2/Docs/Design/ANIM.md` → "Zenith_AnimationComponents header" |
| ANIM | `ZenithAnimation` | S4 | `Zenith.Animation.PoseFollower` | component | 1 Leader | CommandsOnly | — | none | commands | `Animation.ExtractPoses` (12) | `Zenith2/Docs/Design/ANIM.md` → "Zenith_AnimationComponents header" |

SCENE's `Zenith.LocalTransform` row gains, in its writers column, "`Animation.ApplyRootMotion` (4) through `SetProperty`", and in its readers column "`Animation.Evaluate` (3, property 3 Scale of its own row)". ANIM reads no property of PHYS's `Zenith.Physics.CharacterController`; PHYS's census row for it drops the reader "ANIM (3, next tick)", and PHYS's `Zenith.Physics.Ragdoll` 1–2 row lists `Animation.PostPhysicsBlend` (6, after the writer) and `Animation.Significance` (3, property 1 State) beside `Animation.RagdollOwnership` (row AN6 (f)).

### Handle kinds used

| Handle | Kind | ANIM's use |
|---|---|---|
| `Zenith_AssetHandle<Zenith_SkeletonAsset>`, `<Zenith_AnimationClip>`, `<Zenith_AnimationGraphAsset>` | shared asset handle | bindings; the animator's graph |
| `Zenith_PersistentID` | persistent ID | assets named in cooked files, commands, save records; stable IDs of graph nodes and states |
| `Zenith_EntityHandle` | world handle | command targets, IK target entities, pose leaders, event payloads |
| `Zenith_OperationHandle` | operation handle | binding loads and chunk reads, owned by the world's binding and stream state |

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| `.zskel` (`ZSKL` v1) | cooked skeleton | `Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton file"` |
| `.zanim` (`ZANM` v1) | cooked clip | `Zenith2/Docs/Design/ANIM/Formats.md → "Clip file"` |
| `.zagraph` (`ZAGR` v1) | compiled graph | `Zenith2/Docs/Design/ANIM/Formats.md → "Graph file"` |
| `ZISK` v1, `ZICL` v1 | import intermediates (`anim.skeleton`, `anim.clip`) | `Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton intermediate"`, `"Clip intermediate"` |
| `.animgraph.json`, `.animmask.json`, `.animrig.json`, `.animclip.json`, `.animretarget.json` | authoring documents | `Zenith2/Docs/Design/ANIM/Formats.md → "Authoring documents"` |
| `Zenith_AnimatorSaveRecord` v1 | save state | `Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"` |

### In-memory sizes that matter

<!-- elaboration -->
| Structure | Size | Note |
|---|---|---|
| a local joint transform | 40 bytes | `Zenith_Transform` |
| a model joint matrix | 64 bytes | `Zenith_Matrix4`; 48 bytes as `Flux_JointMatrix` in the render world |
| an instance block at J = 60 | about 25 KiB | `Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout"` |
| an instance record | 512 bytes | status, bindings, rate, spread and recovery state |
| a pose-stack entry at J = 60 | 2,400 bytes | 16 entries at most, in the node arena |
| a 60-joint, 1-second, 30 Hz raw clip | about 74 KiB | 31 frames × 60 joints × 40 bytes |
| the same clip quantised at ε = 0.2 mm | typically 8 … 15 KiB | measured and reported per clip by the cook |
| the render-world pose of one instance | 48 × J + 4 × morph targets | written every frame |
| W-Characters' ANIM state | about 2.5 MiB plus clips | within `Z2-B-ANIM-001` |

### Byte order and serialization rules

Little-endian, field by field, reserved fields zero, every count and offset checked before use, codecs recorded by ASSET's section descriptors; documents canonical JSON; none of ANIM's formats depends on a C++ layout (Z2-D-038). `Zenith2/Docs/Design/ANIM/Formats.md → "Common format rules"` is the full list.

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Specified in |
|---|---|---|
| skeleton extraction and rest pose | O(N log N) nodes | `Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"` |
| clip resampling, root-motion extraction, additive cooking | O(F × channels) | same |
| clip compression | O(segments × (A × R × n × D + repairs × h²n)) | `Zenith2/Docs/Design/ANIM/Formats.md → "Compression algorithm"` |
| clip sampling | O(J + C) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationClip header"` |
| graph compilation (Delaunay included) | O(nodes + samples²) | `Zenith2/Docs/Design/ANIM/Graph.md → "Graph compilation"` |
| graph evaluation | O(active nodes × J) | `Zenith2/Docs/Design/ANIM/Graph.md → "Evaluation algorithm"` |
| state machine transitions, crossfade, inertialisation | O(candidates); O(J) per inertial take | `Zenith2/Docs/Design/ANIM/Graph.md → "State machines"` |
| sync groups | O(members × markers) | `Zenith2/Docs/Design/ANIM/Graph.md → "Synchronization"` |
| events | O(crossed events) | `Zenith2/Docs/Design/ANIM/Graph.md → "Animation events"` |
| root motion | O(contributions) | `Zenith2/Docs/Design/ANIM/Graph.md → "Root motion"` |
| ragdoll blend and recovery | O(J + bodies) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll transitions"` |
| IK solvers | O(1) two-bone; O(iterations × n) chain | `Zenith2/Docs/Design/ANIM/Runtime.md → "Inverse kinematics"` |
| motion warping | O(1) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Motion warping"` |
| retarget map and application | O(J_s + J_t) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Retargeting"` |
| significance and rates | O(N × cameras) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Update-rate budgeting"` |
| streaming windows | O(N × playing streamed clips) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Clip streaming"` |
| extraction | O(J) per mesh | `Zenith2/Docs/Design/ANIM/Runtime.md → "Render extraction"` |

### State machine index

| State machine | Specified in |
|---|---|
| animator instance lifecycle | `Zenith2/Docs/Design/ANIM/Runtime.md → "Animator instances"` |
| graph state machines (states × events) | `Zenith2/Docs/Design/ANIM/Graph.md → "State machines"` |
| slots | `Zenith2/Docs/Design/ANIM/Graph.md → "Slots"` |
| clip-chunk residency | `Zenith2/Docs/Design/ANIM/Runtime.md → "Clip streaming"` |
| ragdoll ownership (via PHYS's ragdoll state and the arbitration) | `Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll transitions"` |

### Tick-level flow of ANIM work

1. Tick boundary, step 1: ANIM's publication handler binds published assets and marks published chunks resident.
2. Phase 1 and 2 sync points: ANIM's command playback writes parameters, triggers, slots and overrides.
3. Phase 3: `Animation.Significance` (drain, reconcile, rates, streaming; it collects new requests), its merge node (completion IDs allocated and the requests issued, on the world's merge chain), then `Animation.Evaluate` (graphs, the inertialiser, warping, IK, root motion, events).
4. Phase 3 sync point: events merged; root motion visible to phase 4.
5. Phase 4: root motion is applied (the orchestrator's Q72 ruling): PHYS's character-controller node `Physics.CharacterMove` moves every entity that has a `Zenith.Physics.CharacterController` through collision (row AN6), and ANIM's `Animation.ApplyRootMotion` records the transform change of every other entity with root motion, applied at sync point 4 ("Animation.ApplyRootMotion" of `Zenith2/Docs/Design/ANIM/Runtime.md`). GAME registers no phase-4 node.
6. Phase 5: PHYS steps ragdolls with the animated pose as drive targets (row AN6).
7. Phase 6: the two ownership writers, the arbitration node, `Animation.PostPhysicsBlend`.
8. Phase 9: events delivered; phase 10 scripts react and record commands for the next tick.
9. Phase 11: ANIM's save records when a save is requested (row AN8).

### Frame-level flow of ANIM render work

Phase 12, once per frame: `Animation.ExtractPoses` writes one interpolated skinned pose per animated mesh; GEOM's skinning system consumes them on `Flux.BuildGraph` and skins on the GPU (`Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning palette computation"`). ANIM has no GPU work of its own.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_SkeletonAsset`, `Zenith_AnimationClip`, `Zenith_AnimationGraphAsset` | `ANY_THREAD`, immutable |
| `Zenith_AnimationResidency` (Internal) | `PHASE(Animation)` writes (`Animation.Significance` and its merge node); reads from declared readers |
| `Zenith_AnimationWorld` | `PHASE(Animation)` writes (`Animation.Evaluate`); reads from declared readers |
| `Zenith_AnimationFinalPoses` | `PHASE(PostPhysics)` writes; reads from declared readers |
| component structs, event payloads, `Zenith_DualQuaternion`, IK values | `IMMUTABLE` values |
| `Zenith_RetargetMap` | `ANY_THREAD` after `Create` |
| `Zenith_AnimationPreview` | `OWNER_THREAD` |
| the module instance | `MAIN_THREAD` (Start, AttachWorld, DetachWorld) |
| ANIM's listener, command playback, publication handler | the world's sync-point or tick-boundary node, one world at a time |

### Frame-graph placement

| Node | Phase | Subgraph | Sub-jobs |
|---|---|---|---|
| `Animation.Significance` | 3 | TICK | none |
| the merge node of `Animation.Significance` (ECS's, from `ConsumesMergeCapacity`) | 3 | TICK | none |
| `Animation.Evaluate` | 3 | TICK | `ParallelFor` over chunks |
| the merge node of `Animation.Evaluate` (ECS's, from `EmitsEvent`) | 3 | TICK | none |
| `Animation.ApplyRootMotion` | 4 | TICK | `ParallelFor` over chunks |
| `Animation.PostPhysicsOwnership`, `Animation.RagdollOwnership` | 6 | TICK | none |
| `Arbitrate.Zenith.Animation.Pose.PhysicsWeight` | 6 | TICK | ECS's |
| `Animation.PostPhysicsBlend` | 6 | TICK | `ParallelFor` over instances |
| `Animation.ExtractPoses` | 12 | EPILOGUE stage 12 | `ParallelFor` over chunks |

`Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"` lists every declared access; each ANIM world resource has one writer node per stage (the merge node of `Animation.Significance` for `Zenith.Animation.Residency`, that of `Animation.Evaluate` for `Zenith.Animation.World`, `Animation.PostPhysicsBlend` for `Zenith.Animation.FinalPoses`), and no ANIM system names a generated `Arbitrate.*` node in an ordering (`Test_AnimationModule_ScheduleCompiles`). `Z2-B-ANIM-002` measures phase 3's two nodes (with the merge node of `Animation.Significance`), `Z2-B-ANIM-003` phase 6's three, `Animation.ApplyRootMotion` runs inside the phase-4 span of `Z2-B-GAME-001`, and the extractor runs inside `Z2-B-RPROD-003`'s span.

### Field authority table

| Field | Authority | Writers | Readers |
|---|---|---|---|
| `Zenith.Animation.Animator.*` | `CommandsOnly` | commands (scripts, cinematics, the editor) | ANIM's nodes |
| `Zenith.Animation.RootMotion.*` | `Animation` (phase 3) | `Animation.Evaluate` | PHYS's `Physics.CharacterMove` (rows with a character controller) and ANIM's `Animation.ApplyRootMotion` (rows without), both phase 4 |
| `Zenith.LocalTransform` 1 Position, 2 Rotation (SCENE's) | `PostPhysics` | `Animation.ApplyRootMotion` through `SetProperty` commands recorded in phase 4 and applied at sync point 4 ([Z2-N-ARCH-483], [Z2-N-ARCH-489]), among SCENE's other writers | as SCENE's row |
| `Zenith.Animation.Pose.PhysicsWeight` | `PostPhysics` (phase 6), precedence arbitration (Cinematic, Ragdoll, Animation) | the ANIM ownership writers, CINE's cinematic writer | `Animation.PostPhysicsBlend` |
| `Zenith.Animation.IKGoals.Goals`, `Zenith.Animation.MotionWarp.Targets` | `EarlyScripts` (phase 2) | behaviours, chunk systems; commands from other phases | `Animation.Evaluate` |
| `Zenith.Animation.PoseFollower.Leader` | `CommandsOnly` | commands | the extractor |
| world resource `Zenith.Animation.Residency` | owning phase `Animation` | `Animation.Significance` (its merge node is the declared writer) | `Animation.Evaluate`, ANIM's phase-6 blend, the extractor, bindings, the save participant, PHYS's ragdoll drive and enter playback |
| world resource `Zenith.Animation.World` | owning phase `Animation` | `Animation.Evaluate` (its merge node is the declared writer) | `Animation.Significance` (a pre-reader), ANIM's phase-6 blend, bindings, the save participant, PHYS's ragdoll drive |
| world resource `Zenith.Animation.FinalPoses` | owning phase `PostPhysics` | `Animation.PostPhysicsBlend` | the extractor, socket reads, the save participant, PHYS's ragdoll enter playback |

ANIM's phase-3 and phase-6 nodes write no `Zenith.LocalTransform` ([Z2-N-ARCH-475]); ANIM's only transform write is the phase-4 root-motion command of entities without a character controller, and it never writes `Zenith.WorldTransform`. The D-PHASES refinement (open, Stage 1) confirms this table (row AN7); a moved phase is one registration line.

### Lock-order entries

None: ANIM takes no `Zenith_Mutex`. Cross-thread data reach ANIM only through ASSET's completion queue and operation handles, drained on `Animation.Significance`; every other state is phase- or node-owned.

### Construction and shutdown

The module instance is created at startup step 15 and destroyed in shutdown step 6 with the other module subsystems ([Z2-N-CONTRACT-348]); `DetachWorld` runs before (step 3's world teardown): it cancels the world's operations, completes every completion ID it has not completed with the abandoned payload ("Zenith_Module_Animation header"), drains the operations, then frees instance blocks, stream caches, caches and the resources. The kind loaders stay registered until the asset registry has unloaded every ANIM asset (ASSET's shutdown sequence).

### Cancellation

| Operation | Cancellation points | Completion | Partial results |
|---|---|---|---|
| binding load (`RequestLoad`) | ASSET's stages (`Zenith2/Docs/Design/ASSET/Loading.md → "Cancellation"`); ANIM cancels when the request is abandoned (its instance destroyed or rebound) | ASSET's completion record of a live request is parked with the sink; when ANIM abandons a request it completes the request's completion ID itself with the abandoned payload in that `Animation.Significance` run, and ASSET's `CANCELLED` record (never published by ASSET) is dropped (`Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"` step 4) | ASSET's |
| chunk read (`ReadSection`) | before I/O, during decode; ANIM cancels when the chunk leaves every window before its reads complete | the operation's status, polled in `Animation.Significance`; an abandoned read's completion ID is completed by ANIM with the abandoned payload in that run | the chunk block, owned by the stream cache, freed when the read reports complete |
| world detach | cancels both kinds | every completion ID ANIM has not completed is completed with the abandoned payload, then the operations are drained before the resources are freed | none remain ([Z2-N-CONTRACT-336]) |

`Test_AnimStreaming_CancelReloadStress1000` is the 1,000-cycle stress ([Z2-N-CONTRACT-339]); `Test_AnimBinding_AbandonedRequestCompletesItsID` asserts that no completion ID is left uncompleted.

### Error enumerations

`Zenith_AnimationError` (runtime), `Zenith_AnimationCookDetail` (cook and import details), `Zenith_AnimationImportError` (the import helpers' record), `Zenith_AnimGraphDiagnosticCode` (compile diagnostics, `Zenith2/Docs/Design/ANIM/Graph.md → "Compile diagnostics"`). Module entry points return `Zenith_ModuleError`; loaders report through `Zenith_AssetLoadContext::Fail` with `Zenith_AnimationError` codes.

### Luau errors and asserts

Script misuse (an unknown name, a wrong kind, a non-finite or out-of-range argument, a call needing a bound instance) raises a controlled Luau error naming the function and the argument, from the binding thunk ([Z2-N-CONTRACT-473]), with a misuse code of ANIM's block ZS2000–ZS2099 (`Zenith2/Docs/Design/ANIM/Runtime.md → "Misuse codes"`); a command that turns out invalid at playback is dropped and counted. Asserts guard ANIM's own engine-side preconditions alone: span lengths passed by engine code, indices computed from the asset, the pose-stack depth the loader verified.

### Determinism obligations

- Every evaluation of an instance depends only on that instance's state, its assets, its parameters and world state committed before the phase, so results are independent of worker count, chunk layout and scheduling (Z2-D-019, [Z2-N-ARCH-883]).
- Requests (bindings, chunks) are collected by one node in ascending simulation ID order and issued, with their completion IDs, by that node's merge function on the world's merge chain in the same order ([Z2-N-SCRIPT-341]), so completion IDs are layout-independent; their results publish at tick boundaries in completion-ID order and are replayed at the recorded ticks (Z2-D-053).
- Rate decisions read simulation state only; budgeting keys are simulation-affecting and admit no quality-tier or user layer.
- Instances are created and destroyed in ascending simulation ID order; caps refuse by that order.
- Events are emitted in a defined order through per-entity merge keys; root motion is written to the entity's own row, and the phase-4 transform commands carry each entity's own merge key, so their application order is independent of chunk and leaf order.
- Memory pressure changes no ANIM simulation state ([Z2-N-ARCH-812]).
- `Test_AnimDeterminism_WorkerAndLayoutIndependent` and `Test_AnimDeterminism_ReplayBitIdentical` are the evidence.

## Diagnostics and profiling

### Counters

Names follow [Z2-N-CONTRACT-156] (`_delta` for per-tick deltas):

| Counter | Kind | Meaning |
|---|---|---|
| `anim.instances` | gauge | instances in all worlds |
| `anim.instances.binding`, `.bound`, `.failed`, `.disabled` | gauge | per status |
| `anim.instances.refused_delta` | delta | creations refused at the cap |
| `anim.instance_alloc_failed_delta` | delta | instance blocks that could not be allocated |
| `anim.evaluations_delta`, `anim.evaluations.skipped_delta` | delta | evaluated and skipped instances |
| `anim.rate.tier1`, `.tier2`, `.tier4`, `.tier8` | gauge | instances per rate divisor |
| `anim.joints_delta`, `anim.clip_samples_delta` | delta | joints evaluated, clip samples taken |
| `anim.transitions_delta`, `anim.transitions.inertial_delta` | delta | transitions taken |
| `anim.events_delta`, `anim.events.dropped_delta` | delta | events emitted and dropped |
| `anim.parameters.rejected_delta`, `anim.commands.queued_dropped_delta` | delta | invalid parameter commands; queued commands dropped |
| `anim.blend.degenerate_delta`, `anim.sync.marker_fallback_delta` | delta | degenerate blends; marker sync fallbacks |
| `anim.ik.solves_delta`, `anim.ik.unreached_delta` | delta | IK steps run; goals not reached |
| `anim.bindings.deferred_delta` | delta | binding needs not issued this run because the binding in-flight counter is at `animation.bindingQueueCapacity` (or the sink refused) |
| `anim.stream.resident_bytes` | gauge | stream cache bytes |
| `anim.stream.requests_delta`, `.stalls_delta`, `.deferred_delta`, `.failed_delta` | delta | chunk traffic |
| `anim.loads.w<tag>.full`, `.dropped`, `.high_water` | cumulative | the completion queue's counters ([Z2-N-CONTRACT-345]) |
| `anim.extract.poses_delta`, `.dropped_delta`, `.mismatch_delta`, `.follower_refused_delta`, `.remap_overflow_delta` | delta | extraction (`remap_overflow`: rows written without morph weights because the frame needed more remap tables than `animation.extract.maxRemapTables`) |
| `anim.rootmotion.applied_delta` | delta | entities moved by `Animation.ApplyRootMotion` |
| `anim.ragdoll.recover_offset_rejected_delta` | delta | recoveries whose pose-matching offset exceeded its bound and blended without an offset |
| `anim.save.restore_bind_failed_delta` | delta | instances a restore could not rebind because an asset failed to load |
| `anim.memory_bytes` | gauge | the consumer's resident bytes |

### Profiler zones

One zone per node, named as the node (`Animation.Significance`, `Animation.Evaluate`, `Animation.ApplyRootMotion`, `Animation.PostPhysicsOwnership`, `Animation.RagdollOwnership`, `Animation.PostPhysicsBlend`, `Animation.ExtractPoses`), its sub-jobs `<node>/Leaf` (`Animation.Evaluate/Leaf`, `Animation.ApplyRootMotion/Leaf`, `Animation.PostPhysicsBlend/Leaf`, `Animation.ExtractPoses/Leaf`), `Animation.Significance.Issue` around the merge function that allocates completion IDs and issues requests, and inner zones `Animation.Evaluate.Graph`, `.Sample`, `.Warp`, `.IK`, `.Events`, plus `Animation.Publish` around the publication handler ([Z2-N-BUDGET-223]).

### On-screen profiler rows

The Animation panel shows, per world: instance counts by status and by rate tier; evaluations this tick and their phase-3 and phase-6 times against `Z2-B-ANIM-002` and `Z2-B-ANIM-003`; events and drops; stream cache bytes and stalls; ANIM's memory against its share of `Z2-B-ANIM-001`. The panel's data are `Zenith_AnimationWorld::GetStats` and the counters, drawn by DEVT's profiler.

### Log categories

`anim` (module, loaders, instances), `anim.bind` (bindings), `anim.stream` (streaming), `anim.graph` (graph runtime and hot reload), and the tool-side `anim.cook`. Each failure logs once per (entity, asset, cause), never per tick; a node that fans out leaves collects the reports in each leaf's record and its own body logs them after the last leaf in leaf order, so the first report of a key is worker-independent (`Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"`, "Counter and statistics merge").

### Debug views

Development builds draw, through DEVT's debug-draw records (row AN16): the skeleton of selected or all animated entities (bones as lines, joints as points, coloured by rate tier), the root-motion trail of the last 2 s, IK goals and reached positions, foot probes, sockets, and warp targets. The console command `anim.dump <entity>` prints `GetDebugState` (machines, states, transitions, weights, parameters, slots, rate, residency). Every view reads state; none changes simulation.

### Frame-dump entries

The frame dump gains `animation.json` per world: for each instance its simulation ID, status, graph GUID, rate, machine states, parameters and the last events, and the stream cache table.

### Telemetry

Per session: peak instances, the rate-tier histogram, total stalls, total dropped events, peak stream cache bytes; reported through DEVT's telemetry sink.

### Configuration keys

| Key | Type | Default | Range | Simulation-affecting | Layers admitted |
|---|---|---|---|---|---|
| `animation.maxInstances` | int | 4,096 | 1 … 16,384 | yes (restart) | engine, platform, game, command line |
| `animation.bindingQueueCapacity` | int | 1,024 | 64 … 65,536 | yes (restart): it bounds the binding loads in flight, so it decides which needs issue on a tick | engine, platform, game, command line |
| `animation.streaming.maxReadsInFlight` | int | 256 | 16 … 4,096 | yes (restart): it bounds the chunk reads in flight | engine, platform, game, command line |
| `animation.budget.fullRateDistance` | float | 15 m | 0 … 10,000 | yes | engine, platform, game, command line |
| `animation.budget.halfRateDistance` | float | 40 m | 0 … 10,000 | yes | same |
| `animation.budget.quarterRateDistance` | float | 80 m | 0 … 10,000 | yes | same |
| `animation.budget.maxFullRateEvaluationsPerTick` | int | 0 (no cap) | 0 … 16,384 | yes | same |
| `animation.streaming.cacheBytes` | int | 16,777,216 | 1 MiB … 1 GiB | yes | same |
| `animation.streaming.prefetchSeconds` | float | 2.0 | 0.25 … 10 | yes | same |
| `animation.streaming.keepTicks` | int | 60 | 1 … 3,600 | yes | same |
| `animation.ik.footProbeLayers` | int (mask) | 1 | 0 … 2^32 − 1 | yes | same |
| `animation.extract.maxRemapTables` | int | 1,024 | 16 … 65,536 | no | engine, platform, game, user, command line |
| `animation.debug.skeletonOverlay` | bool | false | — | no | every layer (development builds) |

Simulation-affecting keys are written into every replay header ([Z2-N-ARCH-930], `Zenith2/Docs/Design/PLAT/Configuration.md → "Simulation-affecting keys"`); two keys take their values from pending budget rows: `animation.budget.maxFullRateEvaluationsPerTick` from Z2-B-ANIM-005 "Animation full-rate evaluations per tick cap" and `animation.streaming.cacheBytes` from Z2-B-ANIM-006 "Animation stream cache bytes", both set per platform by the ANIM Stage 4 budget ruling ticket ("Pending row needs"); `animation.maxInstances` is a capacity limit, not a budget value. Cook-side settings (`anim.sampleRateHz`, `anim.compression.*`, `anim.rootMotion.*`, `anim.additive.*`, `anim.loop`, `anim.clipRange`, `anim.streaming.allowed`, `anim.streaming.thresholdBytes`, `annotation`, `rig`) are importer settings in `.meta` documents (`Zenith2/Docs/Design/ASSET/Import.md → "Settings schema"`), not configuration keys.

## Authoring and cooking

### What ANIM authors

Animation gets visual authoring (Z2-D-079): ANIM owns the documents and the operations; EDIT owns the tools that drive them.

| Authoring surface (EDIT's) | ANIM API or document it uses |
|---|---|
| graph editor: nodes, state machines, transitions, conditions, blend spaces, layers, slots, sync groups, IK steps | `.animgraph.json` through SCENE's JSON-tree edits; `Zenith_CompileAnimationGraph` for live diagnostics |
| blend-space editor | the same document; the compiler's triangulation, returned with the diagnostics, for display |
| mask editor (per-joint weights over the skeleton tree) | `.animmask.json` |
| rig setup: roles, chains, IK definitions, sockets, mirror pairs, shell distances, curves | `.animrig.json` |
| clip timeline: events, notify states, markers, curves, loop, root motion, additive and compression settings | `.animclip.json` |
| retarget setup | `.animretarget.json`; `Zenith_RetargetMap` for a live preview |
| preview viewport with parameter controls, slot triggering, scrubbing | `Zenith_AnimationPreview` (a `VIEW_KIND_PREVIEW` view of the skinned mesh, drawn by RPROD's preview path) |
| live debugging of a play-world character | `Zenith_AnimationWorld::GetDebugState`; `anim.dump` |

### Editor surfaces

EDIT's animation panels (pending EDIT, row AN11) host these operations; the edit world runs the module like a play world, and the preview recompiles a graph document in memory on each edit (no cook round trip), so what the author sees is the runtime's evaluation. Undo is EDIT's command system over the documents. CHAR's character workflow (import → rig and retarget → author the graph → preview → in game) is these surfaces in sequence.

### Cook steps and derived-data keys

ANIM's cook follows ASSET's model (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"`): importers write intermediates, and one ASSET-phase step per kind writes each cooked file:

| Importer or step | ID, version | Kind | Inputs hashed into the key |
|---|---|---|---|
| the glTF importer's animation half (CONTENT's importer calling ANIM's helpers) | CONTENT's importer ID; `uANIM_IMPORT_VERSION` = 1 joins its version | — | the source bytes, the resolved settings, the importer version |
| `anim.skeleton` | 1 | 7 | the `anim.skeleton` intermediate, the rig document's SHA-256 (opened through `OpenSourceDocument`), the step version, the platform, `uZENITH_ENGINE_COOK_VERSION`, the format version |
| `anim.clip` | 1 | 8 | the `anim.clip` intermediate, the annotation document's SHA-256, the skeleton's published properties (cook-input edges), the `skinShellDistance` property of every mesh the skeleton's `shellMeshes` property lists (cook-input edges, "Error metric" of `Zenith2/Docs/Design/ANIM/Formats.md`), the additive reference clip's `anim.rawClip`, for a retargeted product the source clip's `anim.rawClip` and the retarget document, the resolved compression settings, `uANIM_COMPRESSION_ALGORITHM_VERSION` (1), the step version, the platform, the engine cook version |
| `anim.graph` | 1 | 9 | the graph document's SHA-256, every mask document's SHA-256, the skeleton's properties, every clip's `anim.clipInfo`, for each clip of a `MOTION_MATCH` node its `anim.rawClip` (cook-input edges, the database's source frames, "Motion matching node" of `Zenith2/Docs/Design/ANIM/Graph.md`), the step version, the platform, the engine cook version |
| `anim.retarget` (importer) | 1 | — | the retarget document's bytes; declares one clip product per listed clip |

Published properties: the skeleton's nine (`Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton published properties"`); each clip's `anim.clipInfo` (duration, flags, skeleton GUID and rest pose hash, marker names), `anim.clip.markers` (the clip's markers and instant events, `Zenith2/Docs/Design/ANIM/Formats.md → "Clip published properties"`, row CN27) and `anim.rawClip` (the resampled raw tracks before root-motion extraction and loop closure, with the clip's resolved cook settings: root-motion mode and motion joint, loop flag, additive, compression and streaming settings, "Clip cook order" of `Zenith2/Docs/Design/ANIM/Formats.md`); each graph's `anim.graphInfo`. The cook report gains `animation.compression` (per clip) and `animation.graphs` (per graph: node count, pose-stack depth, instance bytes).

**Content-rule measures** (row CH15; ASSET's mechanism, `Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"`). `Zenith_RegisterAnimationCookSteps` also calls `RegisterRule` three times with constexpr `Zenith_ContentRule` records, so the character-class rows of ContentSpecs ([Z2-N-BUDGET-415]) can limit ANIM's quantities without any other area reading ANIM's cooked bytes:

| Kind | Quantity | Measure (`Zenith_PfnMeasureContent`) | Value |
|---|---|---|---|
| `ASSET_KIND_SKELETON` | `bones` | `Zenith_AnimMeasureSkeletonBones` | the joint count J, offset 0 of `ANIM_SECTION_SKEL_INFO` |
| `ASSET_KIND_SKELETON` | `ikChains` | `Zenith_AnimMeasureSkeletonIKChains` | the IK definition count, offset 12 of `ANIM_SECTION_SKEL_INFO` (each IK definition is one solved chain; the role chains of offset 10 are retargeting data, not IK) |
| `ASSET_KIND_ANIMATION_GRAPH` | `layers` | `Zenith_AnimMeasureGraphLayers` | the layer count, offset 28 of `ANIM_SECTION_GRAPH_INFO` |

Each measure reads its info section through the `Zenith_CookedFileView` the driver passes (the section table of the file the step just wrote) and returns the count as a `double`; a missing section or a section shorter than the field is `COOK_ERROR_STEP_FAILED`, an engine defect because the step wrote the file. The measures live in `Zenith2/Tools/ZenithCook/Internal/Animation/Zenith_AnimContentMeasures.cpp`.

### Migration and re-cook

- The five document types register tool-side: `Zenith_RegisterAnimationDocumentTypes(Zenith_MigrationRegistry&)` in `Zenith2/Tools/Authoring/Internal/Animation/Zenith_AnimationDocumentTypes.cpp` (the tool library `ZenithAuthoring`, ruling Q84 (a), "ZenithAuthoring ANIM files"), called by ZenithCook and ZenithEditor, with each type's `Zenith_DocumentTypeDesc` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`), canonical schema and migration chain, so `ZenithAnimation` has no `ZenithDocument` edge (the precedent of ENV's ruling E18).
- Each type is at schema version 1 when it lands (Stage 3 for the graph, mask, rig and clip documents; Stage 4 for the retarget document); every version increment ships one migration step and a fixture under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Animation/<Type>/` ([Z2-N-CONTRACT-530], [Z2-N-CONTRACT-548]); preview, backup, journaled write and unknown-field preservation are SCENE's framework ([Z2-N-CONTRACT-531] … [Z2-N-CONTRACT-535]); a new type joins [Z2-N-CONTRACT-552]'s list in the diff that introduces it (row AN2).
- Cooked files re-cook on any engine version change; a mismatched cooked version is refused by ASSET's header checks ([Z2-N-CONTRACT-528]).
- `Zenith_AnimatorSaveRecord` versions migrate forward with one step per version, as save migration requires ([Z2-N-CONTRACT-524]).

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| a skeleton, clip or graph file fails validation | the load fails with its `Zenith_AnimationError` code; ASSET applies the kind's fallback policy (skeleton and graph FAIL, clip NULL_OBJECT); animators of a failed graph go to `BINDING_FAILED` and render the rest pose when their skeleton is bound | `Test_AnimFailure_CorruptAssetsFailLoad` |
| a clip is missing or failed (NULL_OBJECT) | its clip nodes push the rest pose (identity in an additive subtree) and fire no events; counted once | `Test_AnimFailure_MissingClipRestPose` |
| a clip, graph or mesh of another skeleton or rest pose | the binding fails (`ANIMATION_ERROR_SKELETON_MISMATCH`) or the extractor refuses the mesh; one log line | `Test_AnimBinding_MismatchFailsToRestPose`, `Test_AnimExtract_SkeletonMismatchRefused` |
| a rest-pose conflict at import | the import fails naming the skins and joint; nothing is rebound or deformed | `Test_AnimImport_RestPoseConflictRefused` |
| a compression bound violation | the cook step fails | `Test_ClipCompression_ViolationFailsStep` |
| a graph document error | every error reported in one compile; the step fails; the editor shows them | `Test_GraphCompiler_AllErrorsInOneCompile` |
| an invalid command or script call | dropped and counted at playback; a controlled Luau error at record time when detectable | `Test_AnimGraph_UnknownParameterCommandDropped`, `Test_AnimationBindings_MisuseRaisesLuauError` |
| instance block allocation failure | `DISABLED`, logged, no pose; nothing else affected | `Test_AnimInstance_LifecycleTransitions` |
| the instance cap reached | creations refused in ascending simulation ID order | `Test_AnimInstance_CapRefusesInSimulationOrder` |
| a streamed chunk read fails | the clip holds at the chunk boundary; logged once; retried after a reload | `Test_AnimStreaming_FailurePublishedHolds` |
| the stream cache is full | the request defers, counted | `Test_AnimStreaming_CacheFullDefers` |
| a render-world slot is full | the pose is dropped this frame and counted | `Test_AnimExtract_SlotFullCounted` |
| device loss | none for ANIM's simulation; the next full-state extraction writes every pose as it always does | — (poses are written every frame) |
| world detach or shutdown with operations in flight | cancelled, every uncompleted completion ID completed with the abandoned payload, and drained before memory is freed | `Test_AnimStreaming_CancelOnDetach`, `Test_AnimBinding_AbandonedRequestCompletesItsID` |
| an animator destroyed or rebound while its load is in flight | the request is abandoned and its completion ID completed with the abandoned payload in the same `Animation.Significance` run; its publication is dropped | `Test_AnimBinding_AbandonedRequestCompletesItsID` |
| more binding needs or chunk reads than the in-flight bounds | the excess is deferred, in order, to subsequent runs; no ID is allocated for it and the completion queue never overflows | `Test_AnimBinding_InFlightCapDefersInOrder` |
| memory pressure | nothing is shed; simulation results are unchanged | `Test_AnimationMemory_ForcedCriticalShedsNothing` |
| a restore whose recorded asset fails to load | that instance restores `BINDING_FAILED` with one log line and `anim.save.restore_bind_failed_delta`; the restore itself completes (a content failure, so that continuation is not hash-equal) | `Test_AnimSave_RestoreBindFailureCounted` |
| a ragdoll recovery whose pose-matching offset exceeds 0.5 m or 90° | the recovery blends with no offset and counts `anim.ragdoll.recover_offset_rejected_delta` | `Test_RagdollRecover_LargeOffsetRejectedCounted` |
| a budget breach (`Z2-B-ANIM-002`, `-003` BR-ACCEPT; `-001` BR-GOVERNOR) | a BUG on the ANIM epic blocks the stage acceptance; the governor raises its level for memory | the stage acceptance's evidence records |
| a hot reload that fails to compile | the old graph keeps running; the editor shows the diagnostics | `Test_AnimGraph_HotReloadKeepsStateAndParameters` |

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/ANIM/Tests.md → "Test targets"`: `ZenithAnimation_Tests` and ANIM's ZenithCook tests run in the `Test` step on every change; `Zenith_Integration_Animation` (determinism, scenarios) runs in the `Test` step and in stage acceptance; fuzz targets run in the change-triggered `win-asan` sanitizer line and at full length in Stage 6 acceptance; `Libgraph`, `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` and `Complexity` cover the module as every library. No ANIM test needs a GPU.

### Test catalogue index

| Group | Heading in `Zenith2/Docs/Design/ANIM/Tests.md` |
|---|---|
| asset files | "Asset and format tests" |
| compression | "Compression tests" |
| import and cook | "Import and cook tests" |
| graph, blend, state machines | "Graph tests", "Blend and pose tests", "State machine tests" |
| sync, events, root motion, slots, layers | "Sync event and root motion tests" |
| ragdoll, lifecycle, instances | "Ragdoll and lifecycle tests" |
| IK, warping, retargeting | "IK motion warping and retarget tests" |
| facial, streaming | "Facial and streaming tests" |
| budgeting, extraction, bindings | "Budgeting and extraction tests" |
| module, scripts, save, diagnostics, memory, failures | "Module binding and failure tests" |
| the motion matcher and node kind 15 | "Motion matching tests" |
| determinism | "Determinism tests" |
| scenarios | "Integration scenarios" |
| fuzzing, benchmarks, fixtures | "Fuzz targets", "Benchmarks", "Fixtures and generators" |

### Fixtures

Generated at test time (`Zenith2/Docs/Design/ANIM/Tests.md → "Fixtures and generators"`): skeletons, clips, graphs and glTF sources are built by the inline generators of the header-only `Tests/Support/Zenith_AnimTestFixtures.h` ([Z2-N-ARCH-255]) and JSON text in the test sources; cooked files on disk come from the fixture set `ANIM_CookedFixtures` ([Z2-N-ARCH-257]), whose setup case runs the cook into `${ZENITH_OUT}/test-fixtures/ANIM/`; migration fixtures are JSON. No binary fixture is committed.

### Device and acceptance runs

The W-Characters runs (Stage 3 onwards) and the S24 sustained run (Stage 6) measure ANIM's rows on the reference desktop and the S24; CinematicSlice's sign-off items for facial animation and the absence of pops on cinematic skip are human sign-offs in that slice's acceptance package (`Zenith2/Docs/Budgets.md → "CinematicSlice acceptance"`). These are `needs-human` acceptance steps, not ANIM leaf tickets.

## Budget rows

### Rows owned

| Row | Name | Status | ANIM's part |
|---|---|---|---|
| `Z2-B-ANIM-001` | Animation clips, skeletons and pose buffers | pending (Stage 1 per-system budget ticket for ANIM) | every ANIM asset's resident bytes, instance blocks and records, stream caches, retarget maps, remap tables (MP-SUBMEM, `MEMORY_SUBSYSTEM_SIMULATION`) |
| `Z2-B-ANIM-002` | Animation phase CPU | pending (same) | the span of `Animation.Significance` (with its merge node) and `Animation.Evaluate` with every sub-job (MP-CPUZONE), in W-Characters, W-Cinematic, W-OpenWorld |
| `Z2-B-ANIM-003` | Post-physics blending CPU | pending (same) | the span of the phase-6 nodes |

The Stage 1 per-system budget ticket for ANIM is preceded by its harness half (`Zenith2/Benchmarks/Budgets/ANIM/`, measuring the reference engine's skeletal update at the W-Characters scale), which is `blocks`-linked from the Stage 1 workload scale ticket for W-Characters ([Z2-N-BUDGET-022]).

### Rows measured against

| Row | Why |
|---|---|
| `Z2-B-GEOM-004` Compute skinning and morph targets GPU | GEOM skins every pose ANIM extracts |
| `Z2-B-GEOM-001` Unified vertex and index pools | the skinned-vertex pool scales with ANIM's animated meshes |
| `Z2-B-RPROD-003` Render extraction CPU | ANIM's extractor runs inside its span |
| `Z2-B-PHYS-002` Physics step CPU | ragdolls and foot probes |
| `Z2-B-GAME-001` Movement arbitration phase CPU | `Animation.ApplyRootMotion` runs in phase 4 inside its span, beside PHYS's controller node |
| `Z2-B-CORE-005` Frame and graph arenas | the pose stacks in node arenas |
| `Z2-B-SHIP-001`, `Z2-B-SHIP-002` | W-Characters' frame-time rows ([Z2-N-BUDGET-200]: the per-system rows are measured unreduced) |
| `Z2-B-CHAR-001` Walkthrough complete character workflow | CHAR's timed walkthrough over ANIM's surfaces |

### Pending row needs

Recorded for the Budgets writer in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (row AN12) and applied as the core-doc item X212: the rows are Z2-B-ANIM-004 to Z2-B-ANIM-009 of `Zenith2/Docs/Budgets.md` ([Z2-N-BUDGET-467], [Z2-N-BUDGET-468]), except "Animation graph preview latency", which is deferred to cross-doc item X260 because X212 and EDIT name different producers. Each row names its producer: the rows of mechanisms that land at Stage 4 are produced by the **ANIM Stage 4 budget ruling ticket** (`needs-human`, parented to ANIM's epic, `blocks`-linked from the update-rate budgeting, streaming and compression tickets, fix version `Z2 Stage 4`), which measures them in its runs and sets the two configuration keys they fix; the Stage 1 per-system budget ticket for ANIM produces only the row whose mechanism exists at Stage 1:

| Row | Unit | Procedure | Workloads | Bound or exclusion | Producer |
|---|---|---|---|---|---|
| Z2-B-ANIM-004 Animation budgeting reduced-rate CPU | ms | MP-CPUZONE of phase 3 with significance reduction enabled | W-Characters, W-OpenWorld | reported beside `Z2-B-ANIM-002` (which [Z2-N-BUDGET-200] measures unreduced); excluded from bounds | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-005 Animation full-rate evaluations per tick cap | count | MP-COUNTER (`anim.rate.tier1`) | W-Characters, W-OpenWorld | not a bound; its value per platform is the value of `animation.budget.maxFullRateEvaluationsPerTick` | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-006 Animation stream cache bytes | MB | MP-SUBMEM (`anim.stream.resident_bytes`) | W-Cinematic, W-OpenWorld | a member of `Z2-B-ANIM-001`; its value per platform is the value of `animation.streaming.cacheBytes` | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-007 Animation stream stalls per run | count | MP-COUNTER (`anim.stream.stalls_delta`) | W-Cinematic | 0 tolerated in the flagship; BR-ACCEPT | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-008 Clip compression error | metres | the cook's measured maximum | — | a tolerance row, not a time or memory budget: the default ε and the cinematic preset | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-009 Animation extraction CPU | ms | MP-CPUZONE (`Animation.ExtractPoses`) | WS-3D | inside `Z2-B-RPROD-003`'s span; reported, excluded from bounds | Stage 1 per-system budget ticket for ANIM |
| Animation graph preview latency | ms | editor measurement | W-Editor | — | EDIT's W-Editor row tickets |

## Stage plan

### Stage table

| Stage | ANIM deliverables | Gated by |
|---|---|---|
| S1 | the Stage 1 workload scale ticket for W-Characters (`needs-human`, ANIM's epic, [Z2-N-BUDGET-195]); the Stage 1 per-system budget harness for ANIM and the ruling that allocates `Z2-B-ANIM-001` … `-003` | the CORE, JOBS and RPROD rulings ([Z2-N-BUDGET-023]) |
| S2 | none of its own; the API-only headers may be pulled into the Stage 2 sprint with fix version `Z2 Stage 3` | — |
| S3 | `Z2-I-ANIM-01`, `-04` … `-10`, `-15`: the module skeleton and components; skeleton import, cook and loader; raw clip cook, loader and sampling; bindings; the graph compiler, loader and evaluator; blend spaces; state machines with crossfade and inertial transitions; layers and masks; slots; sync groups; events; root motion and dual quaternions; ragdoll transitions; the lifecycle listener; the extractor; save records; bindings; diagnostics; the memory consumer; the determinism suite; the WorkloadCharacters content and scripted path; the D-SEL-MOTIONMATCH harness | X-rows of AN1 and AN2 (the publication sink, the edges) the registered source kind of `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`; PHYS's ragdoll, pose source and character-controller node (AN6) for the ragdoll slice and the root-motion integration scenario; SAVE's participant and restore readiness (AN8, AN17) for save records; GAME's census file (AN7) for the registering tickets; D-PHASES (open, Stage 1); D-SEL-HASH (open, Stage 1) for persisted hashes; D-SEL-JSON (open, Stage 1) for the documents |
| S4 | `Z2-I-ANIM-02`, `-03`, `-11` … `-14`, `-16` and `Z2-I-CHAR-04`: compression, streaming, retargeting, IK, motion warping, facial animation (curves, pose libraries, followers), mirroring, the retarget document, the editor-facing contracts, graph hot reload, update-rate budgeting; the motion-matching node; the ANIM Stage 4 budget ruling ticket | D-SEL-MOTIONMATCH (open, Stage 3), whose ruling ticket blocks the motion-matching node's tickets; EDIT's panels (AN11); GEOM's morph and skeleton accessors (AN4); PHYS's ray cast (AN6) |
| S5 | none: no ANIM item is Stage 5 | — |
| S6 | W-Characters sustained on the S24; fuzz targets at full duration; the save-record migration fixtures under the compatibility policy | shipping qualification |
| S7 | none: ANIM has no deferred item | — |

### API-only slices

1. `Zenith_Module_Animation.h`, `Zenith_AnimationErrors.h`, `Zenith_AnimationTypes.h`, `Zenith_AnimationMath.h`, `Zenith_AnimationComponents.h` and `Zenith_AnimationEvents.h` with contract blocks and stub definitions, so GAME, PHYS, CINE and SCRIPT tickets compile against the components, the authority table and the events (PHYS's controller node reads `Zenith_RootMotionComponent` from this slice).
2. `Zenith_SkeletonAsset.h`, `Zenith_AnimationClip.h` and `Zenith_AnimationGraphAsset.h`, so GEOM's skinned cook and CONTENT's importer tickets compile against them.
3. `Zenith_AnimationWorld.h` and `Zenith_AnimationCommands.h` before the evaluator.

### Ticket slices and ordering

1. Module skeleton and API-only headers (slice 1), after the edge X-row; this ticket registers the Stage 3 types and adds their authority-census rows ("Component registration").
2. Skeleton: import helpers, the `anim.skeleton` step with published properties, the loader (unblocks GEOM's GB26).
3. Clip: the intermediate, resampling, root-motion extraction, additive cooking, the raw format, the loader, sampling.
4. Bindings and instances: the publication path, the instance table, the pose store; then `Animation.ApplyRootMotion`, `blocks`-linked from PHYS's `Physics.CharacterMove` ticket (its tests `Test_RootMotion_EntityScaleAppliedOnce` and `Test_RootMotion_ControllerEntitiesLeftToPhysics` drive PHYS's registered node, not only its component type, row AN6).
5. Graph: documents and compiler (in `ZenithAuthoring`, after EDIT's ticket that creates the library target, ruling Q84 (a)), loader; the evaluator with clip, blend and blend-space nodes; then state machines; then layers, masks, slots; then sync groups and events; then root motion.
6. Phase 6 and ragdoll transitions; the lifecycle listener.
7. Extraction (unblocks GEOM's skinning integration).
8. Bindings, diagnostics, the memory consumer (with its row in the memory suite's shedding cases), save records and the save participant (`blocks`-linked from SAVE's participant ticket and its readiness ticket, which carry `GetReissuedSourceKinds`, `IsRestoreReady` and `ListRestoreWaits`).
9. The determinism suite; WorkloadCharacters; the motion-matching harness with `Zenith_MotionMatcher`.
10. Stage 4 in the order of the stage table: compression, streaming, retargeting, IK, warping, facial, mirroring, budgeting, hot reload, the editor contracts, the motion-matching node (after the D-SEL-MOTIONMATCH ruling ticket), and last the ANIM Stage 4 budget ruling ticket.

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-079 | animation gets visual authoring (EDIT's tools over ANIM's documents) |
| Z2-D-080 | phase 3 owns poses and root-motion extraction; phase 4 applies root motion; phase 6 owns ragdoll ↔ animation blending; per-field authority; arbitration precedence; the lifecycle commands |
| Z2-D-019 | every ANIM simulation path is worker-, layout- and scheduling-independent, including streaming stalls |
| Z2-D-053 | asset bindings and chunk residency publish at tick boundaries in completion-ID order |
| Z2-D-050 | poses reach Flux through render extraction with one frame of CPU lag |
| Z2-D-024 | ZenithAnimation is an optional static module |
| Z2-D-038, Z2-D-048 | JSON documents migrated; versioned binary cooked data re-cooked |
| Z2-D-004 | the codec of every cooked section is recorded by ASSET's descriptors |
| Z2-D-040, Z2-D-045, Z2-D-046, Z2-D-047 | ownership, errors, no globals, contract blocks |
| Z2-D-062, Z2-D-058 | the skinned vertex streams and the skinned-vertex pool ANIM's poses deform (GEOM's) |
| Z2-D-003 | ragdolls run on either physics backend, deterministic per backend |
| Z2-D-012, Z2-D-021 | W-Characters and the flagship slices ANIM is measured in |
| Z2-D-084 | the D-SEL-MOTIONMATCH harness and ruling split; the budget harness and ruling split |
| Z2-D-034 | Zenith Hungarian |

### Open decisions

| Decision | Effect on ANIM |
|---|---|
| D-SEL-MOTIONMATCH (open, Stage 3) | its Noun `Zenith_MotionMatcher` is ANIM's type ("Zenith_AnimationMotionMatch header" of `Zenith2/Docs/Design/ANIM/Runtime.md`); ANIM's harness (`Z2-R-ANIM-178`) is its measurement; its ruling ticket blocks the motion-matching node's tickets (`Z2-R-ANIM-179`); the ruling is CHAR's item `Z2-I-CHAR-09` |
| D-PHASES refinement (open, Stage 1) | confirms "Field authority table" |
| D-SEL-HASH (open, Stage 1) | every name hash and graph hash |
| D-SEL-JSON (open, Stage 1) | the readers behind ANIM's documents |
| D-SEL-PHYSX-ANDROID (open, Stage 1) | which backend runs ragdolls on the S24 |
| D-SEL-FBX (open, Stage 4) | whether FBX skins and animations reach ANIM's helpers through a conversion path |

### Design rulings owned by ANIM

| Ruling | Choice | Alternatives rejected | Reason |
|---|---|---|---|
| A1 pose data in world resources | per-instance blocks in `Zenith.Animation.World` and `.FinalPoses`; components hold fixed-size control data | poses as component containers; one entity per bone | variable joint counts, hot loops and 100 × 60 joints per tick; 6,000 bone entities would cost transform propagation for data no other system writes |
| A2 ANIM depends on Physics | a module edge Animation → Physics | a reflection-only seam; Physics → Animation | ragdoll blending and foot placement need PHYS's state and queries; PHYS needs only ANIM's pose through an interface it declares |
| A3 bindings through publication | assets bind at tick boundaries; no phase node resolves | resolving in phase 3 | load completion times would change results (ASSET's rule) |
| A4 one skeleton, one rest pose | the rest pose is the bind pose; conflicts fail import and cook | per-mesh rebinding; baking a pose change into a mesh | a pose change baked through skin weights distorts the mesh at every large joint, and no weighting fixes it |
| A5 rigid delta blending | dual quaternions for deltas, per-joint local blends for poses | linear translation with slerped rotation for deltas | the linear form moves a shared pivot by a fraction of the lever |
| A6 compression | uniform sampling, 16-frame segments, per-segment variable bit rates, hierarchical virtual-vertex error, raw root motion | curve fitting with keyframe reduction | constant-time sampling, a verifiable bound, streaming by segment |
| A7 budgeting from simulation state | camera distance and bias; keys exclude quality-tier and user layers | screen size from the renderer; tier-driven rates | determinism and replays |
| A8 residency as simulation state | windows decide eviction; publications decide arrival | pressure-driven eviction; sampling whatever is resident | identical results across runs and replays |
| A9 ragdoll ownership | a precedence-arbitrated scalar; ANIM blends | joint-level arbitration; PHYS blending poses | arbitration needs fixed-size values; the blend is pose maths |
| A10 tool-side documents | registration and the graph compiler in the tool library ZenithAuthoring, linked by ZenithCook and the editor (ruling Q84 (a), X257) | a module `ZenithDocument` edge; registration inside ZenithCook (a tool cannot link another tool's `Internal/`, row ED19) | ENV's precedent (E18); the registry is a tool object |
| A11 no dual-quaternion skinning | GEOM's linear blend skinning stays the GPU path | a DQ skinning permutation | GEOM's design and pool layout; ANIM's deltas are CPU-side |
| A12 root motion extracted in phase 3, applied in phase 4 | phase 3 publishes the delta; in phase 4 PHYS's controller node applies it to entities with a character controller and ANIM's `Animation.ApplyRootMotion` to every other entity, through `SetProperty` | applying it in phase 3 or 6; a GAME movement node; applying it to controller entities without collision | [Z2-N-ARCH-475]; the orchestrator's Q72 ruling (human may override); GAME registers no phase-4 node |
| A13 inertial blending | one inertialiser per instance, applied to the whole graph output after the layers and before IK, for inertial transitions and switches, interrupt overflow, forced states, slot clip changes, cinematic skip, motion-matching switches and hot reload | crossfade stacks without bound | bounded cost and no pops |
| A14 slots | one-shots and cinematic performances through named slots | mutating graphs at run time | compiled graphs stay immutable and shareable |
| A15 the clip type name | `Zenith_AnimationClip`, as ASSET's kind table row 8 names it (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`; ASSET applied the rename itself under the kind-table ownership ruling, and ANIM never edits ASSET's design) | `Zenith_AnimationClipAsset` | Architecture's assumed-type table names it ([Z2-N-ARCH-005]) |
| A16 ragdoll recovery offset before the ragdoll blend | the pose-matching offset moves the animated pose, which is then blended with the ragdoll pose by the arbitrated weight; PHYS places the entity root under the hips at recover, so the offset is a bounded residual; its fraction is derived from PHYS's own weight (`p = 1 − w_r / w_1`), so the offset and the weight end in the same tick | offsetting the blended result (it offsets the ragdoll's contribution twice); recovering from wherever the root was left (a slide across the floor); a clock of ANIM's own (`elapsed / T`), which can disagree with PHYS's weight and leave a residual step | `Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover"` |
| A17 EXACT restore | the save section holds every simulation-relevant instance state (poses included), restore rebinds eagerly behind SAVE's readiness step and allocates no completion ID; only saved in-flight requests are re-issued, under their recorded IDs | lazy rebinding after restore (new completion IDs, a `BINDING` first tick); declaring ANIM COHERENT | SAVE's per-capture fidelity ruling (Q70 (a)): an EXACT participant must continue bit-identically |

### Classification decisions ANIM owns

None of an inventory item: every ANIM item is required (nine at Stage 3, seven at Stage 4). One ownership decision is recorded here: the runtime of `Z2-I-CHAR-04` (animation update budgeting) is ANIM's, by [Z2-N-BUDGET-438], while the item stays CHAR's (row AN13). Motion matching's classification is the D-SEL-MOTIONMATCH ruling, whose item is CHAR's: `Z2-R-ANIM-179` (the motion-matching node) is classified required and names D-SEL-MOTIONMATCH (open, Stage 3) in its Decisions, so its tickets are blocked by the ruling ticket; a ruling that excludes motion matching changes the inventory through `Zenith2/Docs/Requirements/Inventory.md → "Change rule"`, which names this decision, and that change reclassifies the requirement in its own diff.

## Requirement index

Requirements of Stages 1 and 3 are in `Zenith2/Docs/Requirements/ANIM.md`, those of Stages 4 and 6 in `Zenith2/Docs/Requirements/ANIM/Stage4Plus.md`.

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-ANIM-001 | Module library target | S3 | Zenith2/Docs/Design/ANIM.md → "Target summary"; Zenith2/Docs/Design/ANIM.md → "ZenithAnimation files"; Zenith2/Docs/Design/ANIM.md → "ZENITH_SUPPORTED matrix" |
| Z2-R-ANIM-002 | Module entry points | S3 | Zenith2/Docs/Design/ANIM.md → "Zenith_Module_Animation header"; Zenith2/Docs/Design/ANIM.md → "Component registration" |
| Z2-R-ANIM-003 | Construction steps unwind | S3 | Zenith2/Docs/Design/ANIM.md → "Zenith_Module_Animation header"; Zenith2/Docs/Design/ANIM.md → "Construction and shutdown"; Zenith2/Docs/Design/ANIM/Tests.md → "Module binding and failure tests" |
| Z2-R-ANIM-004 | Public headers carry contracts | S3 | Zenith2/Docs/Design/ANIM.md → "Rules across the ANIM API"; Zenith2/Docs/Design/ANIM.md → "Header map" |
| Z2-R-ANIM-005 | API-only slices first | S3 | Zenith2/Docs/Design/ANIM.md → "API-only slices"; Zenith2/Docs/Design/ANIM.md → "Ticket slices and ordering" |
| Z2-R-ANIM-006 | Error enumerations | S3 | Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationErrors header"; Zenith2/Docs/Design/ANIM.md → "Error enumerations" |
| Z2-R-ANIM-007 | Headless evaluation | S3 | Zenith2/Docs/Design/ANIM.md → "Test targets and gate steps"; Zenith2/Docs/Design/ANIM/Tests.md → "Test targets" |
| Z2-R-ANIM-008 | Stage 3 components | S3 | Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"; Zenith2/Docs/Design/ANIM.md → "Component registration" |
| Z2-R-ANIM-009 | Stage 4 components | S4 | Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"; Zenith2/Docs/Design/ANIM.md → "Component registration" |
| Z2-R-ANIM-010 | Field authority | S3 | Zenith2/Docs/Design/ANIM.md → "Field authority table"; Zenith2/Docs/Design/ANIM.md → "Frame-graph placement"; Zenith2/Docs/Design/ANIM.md → "Component registration" |
| Z2-R-ANIM-011 | World resources | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM" |
| Z2-R-ANIM-012 | Dual quaternion maths | S3 | Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationMath header"; Zenith2/Docs/Design/ANIM/Graph.md → "Rigid delta blending" |
| Z2-R-ANIM-013 | Skeleton file format | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton file"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton info section"; Zenith2/Docs/Design/ANIM/Formats.md → "Joint hierarchy and rest pose sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Joint name sections" |
| Z2-R-ANIM-014 | Skeleton validation | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation" |
| Z2-R-ANIM-015 | Joint lookup | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_SkeletonAsset header"; Zenith2/Docs/Design/ANIM/Formats.md → "Joint name sections" |
| Z2-R-ANIM-016 | Skins grouped into skeletons | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton extraction from a glTF skin" |
| Z2-R-ANIM-017 | Rest pose from inverse bind matrices | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Rest pose from inverse bind matrices" |
| Z2-R-ANIM-018 | One skeleton one rest pose | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Rest pose from inverse bind matrices"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM" |
| Z2-R-ANIM-019 | Import refusals and limits | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"; Zenith2/Docs/Design/ANIM/Formats.md → "Parser limits" |
| Z2-R-ANIM-020 | Stable import products | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton intermediate"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"; Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion" |
| Z2-R-ANIM-021 | Mesh bind check against the rest pose | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton published properties" |
| Z2-R-ANIM-022 | Skeleton published properties | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton published properties"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys" |
| Z2-R-ANIM-023 | Rig document applied at cook | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"; Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys" |
| Z2-R-ANIM-024 | Rig chains IK definitions and mirroring | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema" |
| Z2-R-ANIM-025 | Curve table | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Curve table section"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPose header" |
| Z2-R-ANIM-026 | Skeleton loader | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Asset kind loaders"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_SkeletonAsset header" |
| Z2-R-ANIM-027 | Clip file format | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip file"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip info section"; Zenith2/Docs/Design/ANIM/Formats.md → "Track table section"; Zenith2/Docs/Design/ANIM/Formats.md → "Segment section" |
| Z2-R-ANIM-028 | Clip validation | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip validation" |
| Z2-R-ANIM-029 | Clip resampling | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip resampling"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate" |
| Z2-R-ANIM-030 | Loop closure | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip resampling"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order" |
| Z2-R-ANIM-031 | Clip sampling | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationClip header"; Zenith2/Docs/Design/ANIM/Formats.md → "Segment section" |
| Z2-R-ANIM-032 | Clip intermediates and raw clip property | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order" |
| Z2-R-ANIM-033 | Root motion extraction at cook | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Root motion extraction at cook"; Zenith2/Docs/Design/ANIM/Formats.md → "Root motion section"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order" |
| Z2-R-ANIM-034 | Additive clip cooking | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Additive clip cooking" |
| Z2-R-ANIM-035 | Clip annotation documents | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip annotation document schema"; Zenith2/Docs/Design/ANIM/Formats.md → "Event and marker sections" |
| Z2-R-ANIM-036 | Track classification | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Track classification" |
| Z2-R-ANIM-037 | Range reduction | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Range reduction"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip range section" |
| Z2-R-ANIM-038 | Bit-rate table and shared dequantisation | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate table"; Zenith2/Docs/Design/ANIM/Formats.md → "Segment section" |
| Z2-R-ANIM-039 | Hierarchical error metric | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Error metric" |
| Z2-R-ANIM-040 | Error bound held every frame | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Error bound verification"; Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate optimisation"; Zenith2/Docs/Design/ANIM/Formats.md → "Error metric" |
| Z2-R-ANIM-041 | Deterministic compression | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate optimisation" |
| Z2-R-ANIM-042 | Raw fallback | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate optimisation"; Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate table" |
| Z2-R-ANIM-043 | Curve compression | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Curve compression" |
| Z2-R-ANIM-044 | Root motion stays raw | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Root motion section"; Zenith2/Docs/Design/ANIM/Formats.md → "Track classification" |
| Z2-R-ANIM-045 | Compression reported and measured | S4 | Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/ANIM/Tests.md → "Benchmarks" |
| Z2-R-ANIM-046 | Stream table | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Stream table section"; Zenith2/Docs/Design/ANIM/Runtime.md → "Residency model" |
| Z2-R-ANIM-047 | Per-world stream cache | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Residency model"; Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine" |
| Z2-R-ANIM-048 | Prefetch window | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Prefetch window"; Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine" |
| Z2-R-ANIM-049 | Streaming stall holds time | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Prefetch window" |
| Z2-R-ANIM-050 | Chunk arrival at tick boundaries | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Publication"; Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings" |
| Z2-R-ANIM-051 | Chunk read failure | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"; Zenith2/Docs/Design/ANIM.md → "Failure table" |
| Z2-R-ANIM-052 | Cancellation on detach | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"; Zenith2/Docs/Design/ANIM.md → "Cancellation" |
| Z2-R-ANIM-053 | Cancel and reload stress | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"; Zenith2/Docs/Design/ANIM.md → "Cancellation" |
| Z2-R-ANIM-054 | Prefetch commands and clip readiness | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Prefetch window"; Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation" |
| Z2-R-ANIM-055 | Asset kind loaders registered | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Asset kind loaders" |
| Z2-R-ANIM-056 | Bindings published at tick boundaries | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"; Zenith2/Docs/Design/ANIM.md → "Determinism obligations"; Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Significance" |
| Z2-R-ANIM-057 | Binding mismatch falls back to rest pose | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"; Zenith2/Docs/Design/ANIM/Runtime.md → "Instance lifecycle state machine"; Zenith2/Docs/Design/ANIM.md → "Failure table" |
| Z2-R-ANIM-058 | Binding cache | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings" |
| Z2-R-ANIM-059 | No asset resolution in phase nodes | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM" |
| Z2-R-ANIM-060 | Instance table ordering | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Instance table" |
| Z2-R-ANIM-061 | Instance lifecycle | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Instance lifecycle state machine" |
| Z2-R-ANIM-062 | Instance cap | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Instance table"; Zenith2/Docs/Design/ANIM.md → "Configuration keys" |
| Z2-R-ANIM-063 | Graph file format | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Graph file"; Zenith2/Docs/Design/ANIM/Formats.md → "Graph sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Graph info section"; Zenith2/Docs/Design/ANIM/Formats.md → "Node table section"; Zenith2/Docs/Design/ANIM/Formats.md → "Parameter table section" |
| Z2-R-ANIM-064 | Graph validation | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Graph validation" |
| Z2-R-ANIM-065 | Graph document schema | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Graph document schema" |
| Z2-R-ANIM-066 | Graph compilation order | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Compile steps"; Zenith2/Docs/Design/ANIM/Graph.md → "Graph model" |
| Z2-R-ANIM-067 | Compile diagnostics | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Compile diagnostics" |
| Z2-R-ANIM-068 | Deterministic graph compilation | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Compile steps" |
| Z2-R-ANIM-069 | Parameters sorted and reserved | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Parameter kinds"; Zenith2/Docs/Design/ANIM/Graph.md → "Reserved parameters" |
| Z2-R-ANIM-070 | Parameter commands at sync points | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Parameter commands"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header" |
| Z2-R-ANIM-071 | Update pass | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Update pass"; Zenith2/Docs/Design/ANIM/Graph.md → "Evaluation algorithm" |
| Z2-R-ANIM-072 | Pose pass without heap allocation | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Pose pass"; Zenith2/Docs/Design/ANIM/Graph.md → "Pose stack" |
| Z2-R-ANIM-073 | Weight culling | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Weight culling" |
| Z2-R-ANIM-074 | Local-space pose blend keeps pivots | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Local-space blend" |
| Z2-R-ANIM-075 | N-way blend | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "N-way blend" |
| Z2-R-ANIM-076 | Local to model conversion | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Model-space conversion" |
| Z2-R-ANIM-077 | Blend1D weights | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Blend1D weights"; Zenith2/Docs/Design/ANIM/Formats.md → "Blend space section" |
| Z2-R-ANIM-078 | Blend2D triangulation | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Blend2D triangulation" |
| Z2-R-ANIM-079 | Blend2D weights | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Blend2D weights" |
| Z2-R-ANIM-080 | Switch and time-scale nodes | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending" |
| Z2-R-ANIM-081 | Slots | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Slots"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending" |
| Z2-R-ANIM-082 | Slot root motion | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Slots"; Zenith2/Docs/Design/ANIM/Graph.md → "Root motion blending" |
| Z2-R-ANIM-083 | State machine tables | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "State machine tables"; Zenith2/Docs/Design/ANIM/Formats.md → "State machine sections" |
| Z2-R-ANIM-084 | Transition conditions | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation" |
| Z2-R-ANIM-085 | Exit times | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation" |
| Z2-R-ANIM-086 | Interrupt sources | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation" |
| Z2-R-ANIM-087 | Crossfade fold | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Transition blending" |
| Z2-R-ANIM-088 | Interrupt overflow becomes inertial | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "State machine state table"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending" |
| Z2-R-ANIM-089 | Inertial blending | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate"; Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout" |
| Z2-R-ANIM-090 | Zero-duration chains | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"; Zenith2/Docs/Design/ANIM/Graph.md → "State machine state table" |
| Z2-R-ANIM-091 | Forced state and reset | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "State machine state table"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending" |
| Z2-R-ANIM-092 | Triggers | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Triggers" |
| Z2-R-ANIM-093 | State changed events | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationEvents header" |
| Z2-R-ANIM-094 | Local additive application | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Additive application" |
| Z2-R-ANIM-095 | Model-space additive application | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Additive application" |
| Z2-R-ANIM-096 | Layer blend modes | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Layer blend modes"; Zenith2/Docs/Design/ANIM/Formats.md → "Mask and layer sections" |
| Z2-R-ANIM-097 | Mask documents and compiled weights | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Mask document schema"; Zenith2/Docs/Design/ANIM/Graph.md → "Mask weights" |
| Z2-R-ANIM-098 | Model-space rotation blending | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Model-space rotation blending" |
| Z2-R-ANIM-099 | Layered blend node | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"; Zenith2/Docs/Design/ANIM/Graph.md → "Layer blend modes" |
| Z2-R-ANIM-100 | Sync group leadership | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Sync groups" |
| Z2-R-ANIM-101 | Normalised-time sync | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Normalised-time sync" |
| Z2-R-ANIM-102 | Marker sync | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Marker sync"; Zenith2/Docs/Design/ANIM/Formats.md → "Event and marker sections" |
| Z2-R-ANIM-103 | Event crossing rules | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules" |
| Z2-R-ANIM-104 | Notify state pairing | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules" |
| Z2-R-ANIM-105 | Event fire modes | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules" |
| Z2-R-ANIM-106 | Event order and cap | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules" |
| Z2-R-ANIM-107 | Event delivery | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Event delivery"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationEvents header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings" |
| Z2-R-ANIM-108 | Root motion sampling | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Root motion sampling" |
| Z2-R-ANIM-109 | Root motion blended rigidly | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Root motion blending"; Zenith2/Docs/Design/ANIM/Graph.md → "Rigid delta blending" |
| Z2-R-ANIM-110 | Root motion published never applied | S3 | Zenith2/Docs/Design/ANIM/Graph.md → "Root motion publication"; Zenith2/Docs/Design/ANIM.md → "Field authority table"; Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate" |
| Z2-R-ANIM-111 | Root motion applied in phase 4 | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.ApplyRootMotion"; Zenith2/Docs/Design/ANIM/Graph.md → "Root motion publication"; Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios" |
| Z2-R-ANIM-112 | Ragdoll ownership precedence | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.PostPhysicsOwnership"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.RagdollOwnership"; Zenith2/Docs/Design/ANIM.md → "Field authority table"; Zenith2/Docs/Design/ANIM/Runtime.md → "Systems" |
| Z2-R-ANIM-113 | Ragdoll enter | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll enter"; Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener" |
| Z2-R-ANIM-114 | Ragdoll blend | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll blend"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.PostPhysicsBlend" |
| Z2-R-ANIM-115 | Ragdoll recover facing | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover" |
| Z2-R-ANIM-116 | Ragdoll recover pivot matching | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover"; Zenith2/Docs/Design/ANIM/Graph.md → "Rigid delta blending"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.PostPhysicsBlend"; Zenith2/Docs/Design/ANIM/Runtime.md → "Systems" |
| Z2-R-ANIM-117 | Ragdoll round trip | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll transitions"; Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios" |
| Z2-R-ANIM-118 | Teleport handling | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener" |
| Z2-R-ANIM-119 | Cinematic skip and interrupt | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending" |
| Z2-R-ANIM-120 | Retarget map construction | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction" |
| Z2-R-ANIM-121 | Rest-relative retargeting | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction"; Zenith2/Docs/Design/ANIM/Runtime.md → "Runtime retarget application" |
| Z2-R-ANIM-122 | Root scale by hips height | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction" |
| Z2-R-ANIM-123 | Retargeting never edits a mesh | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Retargeting"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM" |
| Z2-R-ANIM-124 | Retarget documents and cooked products | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Retarget document schema"; Zenith2/Docs/Design/ANIM/Formats.md → "Retargeted clip cooking" |
| Z2-R-ANIM-125 | Two-bone IK | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Two-bone solver" |
| Z2-R-ANIM-126 | Chain IK | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "FABRIK solver" |
| Z2-R-ANIM-127 | Look-at IK | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Look-at solver" |
| Z2-R-ANIM-128 | Foot placement | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Foot placement" |
| Z2-R-ANIM-129 | IK stage order | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "IK stage order" |
| Z2-R-ANIM-130 | Motion warping | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Motion warping" |
| Z2-R-ANIM-131 | Curves blend with the pose | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Curves and morph targets" |
| Z2-R-ANIM-132 | Morph remap | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Morph remap"; Zenith2/Docs/Design/ANIM/Runtime.md → "Curves and morph targets" |
| Z2-R-ANIM-133 | Pose libraries | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Pose libraries" |
| Z2-R-ANIM-134 | Pose followers | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Pose followers" |
| Z2-R-ANIM-135 | Mirroring | S4 | Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"; Zenith2/Docs/Design/ANIM/Graph.md → "Root motion sampling" |
| Z2-R-ANIM-136 | Extractor placement and reads | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Extractor declared reads"; Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm" |
| Z2-R-ANIM-137 | Pose key matches the instance extractor | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm" |
| Z2-R-ANIM-138 | Pose interpolation | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Pose interpolation" |
| Z2-R-ANIM-139 | Extraction refusals | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm"; Zenith2/Docs/Design/ANIM.md → "Failure table" |
| Z2-R-ANIM-140 | Significance from simulation state | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Significance"; Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets" |
| Z2-R-ANIM-141 | Phase offsets spread evaluations | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets" |
| Z2-R-ANIM-142 | Forced full rate | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets" |
| Z2-R-ANIM-143 | Full-rate evaluation cap | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Evaluation cap" |
| Z2-R-ANIM-144 | Reduced-rate root motion | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Root motion under reduced rates" |
| Z2-R-ANIM-145 | Triggers survive skipped ticks | S4 | Zenith2/Docs/Design/ANIM/Graph.md → "Triggers"; Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets" |
| Z2-R-ANIM-146 | Reduced-rate measurement | S4 | Zenith2/Docs/Design/ANIM.md → "Pending row needs"; Zenith2/Docs/Design/ANIM/Runtime.md → "Evaluation cap"; Zenith2/Docs/Design/ANIM.md → "Configuration keys" |
| Z2-R-ANIM-147 | Animator save records | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM" |
| Z2-R-ANIM-148 | Restore after a graph edit | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"; Zenith2/Docs/Design/ANIM/Graph.md → "Graph hot reload" |
| Z2-R-ANIM-149 | Save during streaming and ragdoll | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"; Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios" |
| Z2-R-ANIM-150 | Save record migration | S6 | Zenith2/Docs/Design/ANIM.md → "Migration and re-cook"; Zenith2/Docs/Design/ANIM/Runtime.md → "Save state" |
| Z2-R-ANIM-151 | Buffered Luau setters | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings" |
| Z2-R-ANIM-152 | Read-world Luau getters | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings" |
| Z2-R-ANIM-153 | Luau misuse errors | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"; Zenith2/Docs/Design/ANIM.md → "Luau errors and asserts"; Zenith2/Docs/Design/ANIM/Runtime.md → "Misuse codes" |
| Z2-R-ANIM-154 | Events reach scripts | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"; Zenith2/Docs/Design/ANIM/Graph.md → "Event delivery" |
| Z2-R-ANIM-155 | Counters | S3 | Zenith2/Docs/Design/ANIM.md → "Counters" |
| Z2-R-ANIM-156 | Profiler zones | S3 | Zenith2/Docs/Design/ANIM.md → "Profiler zones"; Zenith2/Docs/Design/ANIM.md → "On-screen profiler rows" |
| Z2-R-ANIM-157 | Dump and debug state | S3 | Zenith2/Docs/Design/ANIM.md → "Frame-dump entries"; Zenith2/Docs/Design/ANIM.md → "Debug views"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header" |
| Z2-R-ANIM-158 | Configuration keys | S3 | Zenith2/Docs/Design/ANIM.md → "Configuration keys" |
| Z2-R-ANIM-159 | Memory consumer reports resident bytes | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Memory consumer" |
| Z2-R-ANIM-160 | Animation sheds nothing under pressure | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Memory consumer" |
| Z2-R-ANIM-161 | Failure table behaviours | S3 | Zenith2/Docs/Design/ANIM.md → "Failure table" |
| Z2-R-ANIM-162 | Parser limits | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Parser limits" |
| Z2-R-ANIM-163 | Fuzz targets | S3 | Zenith2/Docs/Design/ANIM/Tests.md → "Fuzz targets" |
| Z2-R-ANIM-164 | Fuzzing at full duration | S6 | Zenith2/Docs/Design/ANIM/Tests.md → "Fuzz targets"; Zenith2/Docs/Design/ANIM.md → "Stage table" |
| Z2-R-ANIM-165 | Worker and layout independence | S3 | Zenith2/Docs/Design/ANIM/Tests.md → "Determinism tests"; Zenith2/Docs/Design/ANIM.md → "Determinism obligations" |
| Z2-R-ANIM-166 | Replay bit-identical | S3 | Zenith2/Docs/Design/ANIM/Tests.md → "Determinism tests"; Zenith2/Docs/Design/ANIM.md → "Determinism obligations" |
| Z2-R-ANIM-167 | Determinism of Stage 4 features | S4 | Zenith2/Docs/Design/ANIM/Tests.md → "Determinism tests"; Zenith2/Docs/Design/ANIM/Runtime.md → "Publication" |
| Z2-R-ANIM-168 | Document types registered tool-side | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Document type table"; Zenith2/Docs/Design/ANIM.md → "Migration and re-cook"; Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files" |
| Z2-R-ANIM-169 | Canonical documents | S3 | Zenith2/Docs/Design/ANIM/Formats.md → "Authoring documents"; Zenith2/Docs/Design/ANIM.md → "Migration and re-cook" |
| Z2-R-ANIM-170 | Document migration fixtures | S3 | Zenith2/Docs/Design/ANIM.md → "Migration and re-cook" |
| Z2-R-ANIM-171 | Preview matches the world | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPreview header"; Zenith2/Docs/Design/ANIM.md → "Editor surfaces" |
| Z2-R-ANIM-172 | Graph hot reload | S4 | Zenith2/Docs/Design/ANIM/Graph.md → "Graph hot reload"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending" |
| Z2-R-ANIM-173 | Visual authoring workflow | S4 | Zenith2/Docs/Design/ANIM.md → "What ANIM authors"; Zenith2/Docs/Design/ANIM.md → "Editor surfaces" |
| Z2-R-ANIM-174 | W-Characters workload scale | S1 | Zenith2/Docs/Design/ANIM.md → "Stage table"; Zenith2/Docs/Design/ANIM.md → "Rows owned" |
| Z2-R-ANIM-175 | Stage 1 budget harness | S1 | Zenith2/Docs/Design/ANIM.md → "Rows owned"; Zenith2/Docs/Design/ANIM.md → "Stage table" |
| Z2-R-ANIM-176 | Stage 1 budget ruling | S1 | Zenith2/Docs/Design/ANIM.md → "Rows owned" |
| Z2-R-ANIM-177 | W-Characters measured at Stage 3 | S3 | Zenith2/Docs/Design/ANIM.md → "Device and acceptance runs"; Zenith2/Docs/Design/ANIM.md → "Rows measured against" |
| Z2-R-ANIM-178 | Motion-matching harness | S3 | Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"; Zenith2/Docs/Design/ANIM/Graph.md → "Motion matching node"; Zenith2/Docs/Design/ANIM.md → "Open decisions" |
| Z2-R-ANIM-179 | Motion-matching node | S4 | Zenith2/Docs/Design/ANIM/Graph.md → "Motion matching node"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"; Zenith2/Docs/Design/ANIM.md → "Classification decisions ANIM owns"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys" |
| Z2-R-ANIM-180 | W-Characters sustained on the S24 | S6 | Zenith2/Docs/Design/ANIM.md → "Device and acceptance runs"; Zenith2/Docs/Design/ANIM.md → "Stage table" |
| Z2-R-ANIM-181 | Curve clip intermediate writer | S4 | Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate" |
| Z2-R-ANIM-182 | Rate divisor function | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header" |
| Z2-R-ANIM-183 | Content-rule measures | S4 | Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys" |
| Z2-R-ANIM-184 | Marker lookup and the full-rate slot tag | S4 | Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"; Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets" |
| Z2-R-ANIM-185 | Published clip markers | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Clip published properties" |
| Z2-R-ANIM-186 | Skeleton and clip product fingerprints | S4 | Zenith2/Docs/Design/ANIM/Formats.md → "Product fingerprints and rename recognition" |

## Reconciliation items

The rows ANIM recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` for other areas and the core documents:

| Row | To | Need |
|---|---|---|
| AN1 | MOD, SCRIPT | conformance to the single module publication sink of MOD's post-signoff patch (Q76 (a)), with ANIM's six needs (the source kind is registered: `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`): the declared in-flight bound and the deferral under it, and the producer obligation of X255 (every allocated ID completed once, abandoned ones with the abandoned payload) |
| AN2 | ARCH | applied as X210 (core-doc patch e), with X210's numbering: (1) ZenithAnimation's edges and layer, Cinematics' layer, the module dependency on Physics; (2) the parser-limit rows; (3) the get-up clip type; (4) the document-type list of [Z2-N-CONTRACT-552]; (5) FramePhases' phase-4 node `Animation.ApplyRootMotion` |
| AN3 | ASSET | kind 8's runtime type name: applied by ASSET (row 8 names `Zenith_AnimationClip`) |
| AN4 | GEOM | `GetSkeletonID`, `GetMorphTargetNameHash`, the bind check against the rest pose, `skinShellDistance`, the property names GEOM's cook reads |
| AN5 | RPROD, REND, GEOM | `AddSkinnedPose` copies its spans; ANIM's extractor per RP29 (superseding GM15's premise of no Flux edge) |
| AN6 | PHYS | the ragdoll state component, the pose source, body poses, ray casts, the recover placement, the phase-6 weight node and its RECOVERING schedule (ANIM's recovery fraction), the character controller's root-motion application in phase 4 (Q72) with DeltaPosition's pre-scaled units, dropping ANIM from the controller's census readers, and (f) the 0 s recover under PHYS-RECOVER, the Ragdoll census row's two added ANIM readers, and `ReadResource Zenith.Animation.Residency` on `Physics.Step` and the enter playback |
| AN7 | GAME | the authority census rows and the D-PHASES confirmation (GAME's GA15) |
| AN8 | SAVE | adoption of the save participant (SV17) and its members as SAVE revision 2 settled them (re-issue declaration, `MarkReissued`, readiness, restore waits), EXACT, the `ZenithSave` edge, and the async section's treatment of ANIM's parked results (restored as completed; ANIM rebuilds their state) |
| AN9 | CINE | the cinematic writer source, slot use, scrubbing |
| AN10 | SCRIPT | module commands from buffered bindings |
| AN11 | EDIT | the animation panels |
| AN12 | BUDGET | the pending rows |
| AN13 | CHAR | the budgeting runtime, the motion-matching harness, rig documents, debug state, followers |
| AN14 | CONTENT | the import helpers' call contract and the DCC matrix rows for skins and animation |
| AN15 | AUDIO, VFX | notify events with sub-tick times |
| AN16 | DEVT | debug-draw records for the skeleton overlay |
| AN17 | SAVE | a participant restore-readiness step before the restored world's first tick: resolved by SAVE revision 2 (`IsRestoreReady`, `WORLD_RESTORE_STATE_PARTICIPANTS_READY`, plus `ListRestoreWaits`) |
