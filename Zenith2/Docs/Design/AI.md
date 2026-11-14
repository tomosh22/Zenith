# AI design: AI and navigation

The design of the AI area (AI & navigation, with the Appendix A "AI (extended)" row): navigation meshes baked offline per tile and rebuilt incrementally at run time for carving obstacles, streamed with world cells, joined by off-mesh links, searched by sliced A* inside the frame graph so every path result is deterministic, local avoidance and crowds, sparse-voxel volumes for flying and underwater agents, perception with memory, blackboards, the Luau libraries through which games author behaviours and states, simulation LOD, debugging, reproducible scenarios and the save participant — all in the module library `ZenithAI`, which runs in frame phase 7. This file is the entry document; the section files carry the detail ("Document family"). Obligations are plain prose and bullets; each is traced by a `Z2-R-AI-NNN` requirement of `Zenith2/Docs/Requirements/AI.md`. Where this family disagrees with `Zenith2/Docs/Architecture.md` or `Zenith2/Docs/Scripting.md`, they win and this family is corrected in the same diff.

## Purpose and scope

### What AI owns

| Capability | Where |
|---|---|
| the navigation data model, the tile format, the bake pipeline and its stage contracts, the runtime tile table, streaming, obstacles, off-mesh links, the path search and the path service | `Zenith2/Docs/Design/AI/Navigation.md` |
| non-ground navigation: sparse-voxel volumes for flying and underwater agents, swimming | `Zenith2/Docs/Design/AI/Volumes.md` |
| agents, path following, steering, local avoidance, crowds, locomotion output, simulation LOD, perception, blackboards, determinism rules, save and restore | `Zenith2/Docs/Design/AI/Agents.md` |
| the public headers of `ZenithAI`: types, errors, queries, build API, components, events, commands, world resources, configuration keys, census rows | `Zenith2/Docs/Design/AI/PublicAPI.md` |
| the `.znav` file (kind 15), its sections, the navigation intermediates, the two authoring documents, command, request, result and save layouts, parser limits | `Zenith2/Docs/Design/AI/Formats.md` |
| the `AI` script bindings, the path request and result, the AI misuse codes, and the Luau libraries `@ai/BehaviourTree`, `@ai/Utility`, `@ai/StateMachine`, `@ai/Agent`, `@ai/Blackboard`, `@ai/Trace` | `Zenith2/Docs/Design/AI/Scripting.md` |
| the D-SEL-NAV (open, Stage 1/2) harness, conformance scenarios and ruling consequences | `Zenith2/Docs/Design/AI/Navigation.md` → "Navigation selection" |
| the AI budget rows and their measurement | "Budget rows" |
| the AI editor extension (navigation tools, AI debugger) and the AI cook code | "Editor surfaces"; "Cook steps and derived-data keys" |

### Explicit exclusions

| Not AI's | Owner | Basis |
|---|---|---|
| decision-making as a second language, graph asset or interpreter | none: behaviours are Luau | Z2-D-016; AI ships Luau libraries only (`Zenith2/Docs/Design/AI/Scripting.md` → "Scope of this file") |
| visual behaviour or state editors | none | Z2-D-079 gives visual authoring to animation, terrain, UI and cinematics |
| quest, dialogue, inventory, interaction; squads, formations, cover and tactical slots | game-side Luau (OpenWorldSlice) | Z2-D-077; `Zenith2/Docs/Design/AI/Scripting.md` → "Authoring conventions" |
| world-visible gameplay state machines, tags, timers, abilities | GAME | `Zenith2/Docs/Design/GAME/Framework.md` → "State machines" |
| the character controller's move, physics queries, collision cooking | PHYS | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Character move algorithm"; AI reads `Zenith_PhysicsWorldResource` and writes the controller's input only through commands |
| terrain navigation geometry extraction and area flags | ENV | `Zenith2/Docs/Design/ENV/Bake.md` → "Navigation geometry extraction" |
| cell streaming, region states, origin rebasing | STREAM | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Navigation coordination" |
| the navigation baking workflow across a world partition and its orchestration UI | WORLD | Z2-I-WORLD rows (WORLD's design, pending); AI supplies the bake, the per-cell contribution and the bake panel |
| the asynchronous source registry, completion IDs, publication and the result table | SCRIPT | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" |
| networked replication of AI state | NET (Stage 7) | Z2-D-073 |
| DetourCrowd and DetourTileCache | none | `Zenith2/Docs/Design/AI/Navigation.md` → "Candidate mapping" |

### Inventory items covered

| Item | Text | Stage | Where |
|---|---|---|---|
| Z2-I-AI-01 | navmesh baking | S4 (D-SEL-NAV (open, Stage 1/2)) | `Zenith2/Docs/Design/AI/Navigation.md` → "Bake pipeline" |
| Z2-I-AI-02 | navmesh streaming | S4 | `Zenith2/Docs/Design/AI/Navigation.md` → "Runtime tile table" |
| Z2-I-AI-03 | dynamic obstacles | S4 | `Zenith2/Docs/Design/AI/Navigation.md` → "Dynamic obstacles" |
| Z2-I-AI-04 | off-mesh links | S4 | `Zenith2/Docs/Design/AI/Navigation.md` → "Off-mesh links" |
| Z2-I-AI-05 | async pathfinding | S4 (Z2-D-053) | `Zenith2/Docs/Design/AI/Navigation.md` → "Path service" |
| Z2-I-AI-06 | avoidance | S4 | `Zenith2/Docs/Design/AI/Agents.md` → "Avoidance" |
| Z2-I-AI-07 | crowds | S4 | `Zenith2/Docs/Design/AI/Agents.md` → "Crowd grid", "Agent state machine" |
| Z2-I-AI-08 | perception | S4 | `Zenith2/Docs/Design/AI/Agents.md` → "Perception" |
| Z2-I-AI-09 | blackboards | S4 | `Zenith2/Docs/Design/AI/Agents.md` → "Blackboard store" |
| Z2-I-AI-10 | behaviour authoring | S4 | `Zenith2/Docs/Design/AI/Scripting.md` → "Behaviour tree library" |
| Z2-I-AI-11 | state authoring | S4 | `Zenith2/Docs/Design/AI/Scripting.md` → "State machine library" |
| Z2-I-AI-12 | AI debugging | S4 | "Diagnostics and profiling"; "Editor surfaces" |
| Z2-I-AI-13 | reproducible AI scenarios | S4 (Z2-D-019) | `Zenith2/Docs/Design/AI/Agents.md` → "Determinism rules"; "Canonical AI state hash" |
| Z2-I-AI-14 | AI simulation LOD | S4 | `Zenith2/Docs/Design/AI/Agents.md` → "Simulation LOD" |
| Z2-I-AI-15 | navigation for different agent sizes | S4 | `Zenith2/Docs/Design/AI/Navigation.md` → "Agent classes" |
| Z2-I-AI-16 | flying navigation | S4 | `Zenith2/Docs/Design/AI/Volumes.md` |
| Z2-I-AI-17 | swimming navigation | S4 | `Zenith2/Docs/Design/AI/Volumes.md` → "Swimming" |

### Cross-area items AI implements

| Row or ruling | Need | AI heading or API |
|---|---|---|
| ST9 (STREAM) | navmesh tiles are components of NAVIGATION cells; cross-tile links when both tiles are active; agents suspended outside active regions; origin shifts | `Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles", "Link registration", "Origin shift"; `Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine" |
| SV17, X204 (SAVE) | one participant per world, `ZenithSave` in `DEPENDS`, in-flight requests re-issued under their completion IDs | `Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm" |
| GA18 (GAME) | Z2-B-AI-002 measures the whole phase-7 span, GAME's nodes included | "Rows owned" |
| ED6, ED15 (EDIT) | no agent update or path request in edit and preview worlds; AI panels under the editor's extension interface | `Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement"; "Editor surfaces" |
| AS26 (ASSET) | kind 15's runtime object type is `Zenith_NavMeshAsset` | "Asset kind" |
| Q19 (a) | script path requests capped by Z2-B-SCRIPT-026 at the merge; Z2-B-AI-003 is AI's BR-DEFER row | `Zenith2/Docs/Design/AI/Navigation.md` → "Request queue" |
| Q76 (a) | results parked through the single module publication sink | `Zenith2/Docs/Design/AI/Navigation.md` → "Publication of script results" |
| Q58 | one writer per world resource per stage | `Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement" |
| the `AI.Perception` example of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Declarations at the ECS level" | `AI.Perception` after `AI.NavRequests`, writing `Zenith_AIPerceptionState`, emitting `Zenith_AIStimulusEvent` | `Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement" |
| ENV's navigation hook | the consumer of `Zenith_NavigationGeometry` and of the `navigation:<x>_<z>` products | `Zenith2/Docs/Design/AI/Formats.md` → "Navigation source intermediate" |
| WD11, WD23 (WORLD) | the cell contribution over WORLD's types; the product declaration WORLD calls in process for its derived, anchor-relative navigation documents | `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"; `Zenith2/Docs/Design/AI/Navigation.md` → "Bake inputs" |

### Document family

| File | Content | Headings |
|---|---|---|
| `Zenith2/Docs/Design/AI.md` | this entry: scope, dependencies, layout, cross-cutting tables, threading, diagnostics, authoring, failure, tests, budgets, stages, decisions, index | the design skeleton |
| `Zenith2/Docs/Design/AI/PublicAPI.md` | every public header | Scope of this file · Header list · Zenith_AITypes header · Zenith_AIErrors header · Zenith_NavMesh header · Zenith_NavQuery header · Zenith_NavBuild header · Zenith_NavVolume header · Zenith_NavMeshAsset header · Zenith_AIComponents header · Zenith_AIEvents header · Zenith_AICommands header · Zenith_AIWorldResource header · Zenith_AIDebug header · Zenith_Module_AI header · Configuration keys · Authority census rows |
| `Zenith2/Docs/Design/AI/Navigation.md` | navigation meshes and paths | Scope of this file · Navigation data model · Bake pipeline · Runtime tile table · Dynamic obstacles · Off-mesh links · Path search · Path service · Spatial queries · Navigation selection |
| `Zenith2/Docs/Design/AI/Volumes.md` | volume navigation | Scope of this file · Volume model · Volume bake · Volume search · Volume path smoothing · Volume line of sight · Swimming · Medium transitions |
| `Zenith2/Docs/Design/AI/Agents.md` | agents, crowds, perception, blackboards | Scope of this file · Frame-graph placement · Agent runtime table · Agent state machine · Move commands · Path corridor · Steering · Avoidance · Locomotion output · Crowd grid · Simulation LOD · Perception · Blackboard store · Determinism rules · Save and restore algorithm |
| `Zenith2/Docs/Design/AI/Internals.md` | the tool-side root header and every internal type a ticket writes | Scope of this file · Zenith_AuthoringAI header · Internal declarations |
| `Zenith2/Docs/Design/AI/Scripting.md` | script surface and behaviour authoring | Scope of this file · Binding registration · Binding functions · Path request payload and result · Misuse codes · Luau library layout · Behaviour tree library · Utility library · State machine library · Agent library · Blackboard library · Debug trace helpers · Authoring conventions |
| `Zenith2/Docs/Design/AI/Formats.md` | formats | Common rules · Navigation file · Navigation source intermediate · Tile bake intermediate · Navigation settings document · Navigation document schema · Command payloads · Script request payload · Script result payloads · Save section layout · Parser limits |
| `Zenith2/Docs/Design/AI/Tests.md` | the test catalogue | Scope of this file · Test targets · Gate steps for AI diffs · one section per test group · Navigation conformance suite · Benchmarks and device runs · Fixtures |
| `Zenith2/Docs/Requirements/AI.md` | the requirement catalogue's purpose, the Stage 1 and Stage 2 requirements, the coverage of inventory items (over both files) and the exclusions | one per requirement |
| `Zenith2/Docs/Requirements/AI/Stage4Plus.md` | the Stage 4 requirements Z2-R-AI-007 … Z2-R-AI-096 (module, formats, bake, runtime tiles, obstacles, links, search, path service, agents and locomotion), same format and numbering | one per requirement |
| `Zenith2/Docs/Requirements/AI/Stage4Systems.md` | the requirements Z2-R-AI-097 onward (volumes, perception, blackboards, scripting, LOD, debugging, determinism, saves, the Stage 6 rows and every requirement from Z2-R-AI-163 on), same format and numbering | one per requirement |

### Conventions

- Units are SI; angles in authoring documents are degrees and convert at cook; durations reaching simulation are ticks.
- "Simulation ID" is the entity's stable ECS simulation ID; every AI ordering that is not by tile key or completion ID is ascending simulation ID.
- `<!-- elaboration -->` marks a design choice beyond the plan's text; `<!-- assumption: pending <AREA> -->` marks an interface another area's design must still confirm, each with a row in "Reconciliation items".

## Dependencies

### Libraries AI code depends on

| Target | Declared `DEPENDS` | Notes |
|---|---|---|
| `ZenithAI` (`KIND MODULE`, `MODULE_NAME AI`, module dependency `Physics`, layer 8) | ZenithRuntime, ZenithPhysics, ZenithScene, ZenithECS, ZenithReflect, ZenithAssets, ZenithJobs, ZenithCore, ZenithProfiling, **ZenithSave**, **ZenithPlatform**; `THIRD_PARTY recast-detour` only if D-SEL-NAV selects it | the bold edges are missing from `Zenith2/Docs/Architecture.md` → "Library dependency graph" (X204 for ZenithSave; row AI8 for ZenithPlatform, the configuration reads at start) |
| `ZenithAuthoring` (AI's subdirectory `Zenith2/Tools/Authoring/Internal/AI/`, root header `Zenith2/Tools/Authoring/Zenith_AuthoringAI.h`, declared in `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header") | gains ZenithAI in AI's first move (core-doc item X301's rule: each owner adds its module); ZenithEnvironment (the ZNSG writer's input type) and WORLD's own header in the same library are already there | the tool library of ruling Q84 (a), laid out as `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs" states; it names no ZenithCook or ZenithEditor type (X301); row AI12 |
| `ZenithCook` | AI's importers and the kind-15 step in `Zenith2/Tools/ZenithCook/Internal/AI/`, registered through `Zenith_RegisterAICookSteps` (ZenithCook's own registration, never in ZenithAuthoring); they call the authoring functions of `Zenith_AuthoringAI.h` | already depends on every module and on ZenithAuthoring |
| `ZenithEditor` | AI's extension under `Tools/ZenithEditor/Internal/Navigation/` | already depends on every module |
| `Zenith_Bench_NavigationSelection` | ZenithAI's dependencies, `THIRD_PARTY recast-detour` | benchmark target |

ZenithAI depends on no other module than ZenithPhysics ([Z2-N-ARCH-074]): it reads `Zenith.Streaming.RegionBound`, `Zenith.WorldOrigin`, `Zenith.LocalTransform` and the controller's properties by reflection ID through system declarations, never through another module's headers except ZenithPhysics's.

### Headings this area relies on

| Topic | Heading |
|---|---|
| module entry points and contexts | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" |
| modules and module dependencies | `Zenith2/Docs/Architecture.md` → "Engine modules and game projects" |
| phases, authority, sync points | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases" |
| system declarations | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Declarations at the ECS level"; `Zenith2/Docs/Architecture/ECSModel.md` → "The system context" |
| command buffers and merge order | `Zenith2/Docs/Architecture/ECSModel.md` → "Command buffers" |
| own-row and any-row reads | `Zenith2/Docs/Design/ECS/Queries.md` → "Own-row and any-row reads" |
| asynchronous results | `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results" |
| behaviour state | `Zenith2/Docs/Scripting.md` → "Behaviour state model and recovery" |
| bindings | `Zenith2/Docs/Scripting/Bindings.md` → "What AddFunction generates"; `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header" |
| determinism | `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" |
| physics queries and the controller | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsWorldResource header"; `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CharacterController header" |
| terrain navigation input | `Zenith2/Docs/Design/ENV/Bake.md` → "Navigation geometry extraction"; `Zenith2/Docs/Design/ENV/Bake.md` → "Area flags" |
| streaming | `Zenith2/Docs/Design/STREAM/Runtime.md` → "Navigation coordination" |
| assets and cooking | `Zenith2/Docs/Design/ASSET.md` → "Asset kinds"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext"; `Zenith2/Docs/Design/ASSET/Import.md` → "Zenith_ImportContext"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader" |
| saves | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header" |
| debug drawing | `Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header" |
| census | `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census rows added by subsequent designs" |
| editor extensions | `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module behaviour in edit and preview worlds" |
| third-party pins | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Selection-candidate entries" |
| maths and randomness | `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| containers and sorting | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet"; `Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching" |

### Assumed interfaces

The single module publication sink of ruling Q76 (a) is MOD's, specified in `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink" (its headings "Zenith_ModulePublication header", "Source declaration and handler registration", "Allocation sites and acceptance" and "Parking a result"); AI uses exactly its declared members and cites them, never re-specifies them:

| Member (MOD's) | AI's use |
|---|---|
| `Zenith_ModulePublicationSink::DeclareSource(uint8, const Zenith_ModuleSourceDesc&)` with `MODULE_SOURCE_SITE_MERGE_CHAIN` | attach step 3: `ASYNC_SOURCE_NAVIGATION` (value 13 of SCRIPT's registry, `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values") with `{ ai.nav.maxLoadsInFlight, 24, MODULE_SOURCE_SITE_MERGE_CHAIN }` |
| `SetSourceHandler(uint8, Zenith_ModulePublicationHandler&)` | attach step 3: the world's `Zenith_AINavPublicationHandler` (`Zenith2/Docs/Design/AI/Internals.md` → "Navigation load records") |
| `AllocateCompletionID(uint8) → Zenith_Result<uint64, Zenith_PublicationSinkError>` | `Zenith_AINavIngestMerge` only (the merge chain; never the prologue, ruling Q89 (a)); `PUBLICATION_SINK_ERROR_FULL` defers the need (`Zenith2/Docs/Design/AI/Navigation.md` → "Navigation asset requests") |
| `Park(uint64, uint8, uint8, Zenith_ByteSpan)` | script path results (`ASYNC_SOURCE_PATHFINDING`, a script source AI declares at start, which needs no handler) and navigation load results (`ASYNC_SOURCE_NAVIGATION`) |

`Zenith_WorldAttachContext::GetPublicationSink()` and `FindCommandKind(const char* szName) → uint32` (resolving `Zenith_AICommandKinds` at the first attach, PHYS's PH7 shape) are MOD's `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "World attach context lookups", whose own block is pending core-doc item X344; AI cites them there and assumes nothing beyond it.

<!-- assumption: pending MOD (row AI1) -->
| Interface | Used for | Row |
|---|---|---|
| `Zenith_ModuleStartContext::GetScriptAsyncCapacity(uint8 uSourceKind) → uint32` | P, the pathfinding value of Z2-B-SCRIPT-026 that SCRIPT applies at the merge (`Zenith_ScriptCapacityLimits::m_auAsyncInFlight[0]`), for the path inbox's capacity and the queue invariant (`Zenith2/Docs/Design/AI/Navigation.md` → "Request queue") | AI1 |
<!-- /assumption -->

`Zenith_ModuleStartContext::GetDebugDraw() → Zenith_DebugDraw&` is declared (core-doc patch f, X245 (c); `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"): the recorder exists in every build, Shipping included, and is disabled in headless runs and, in Shipping, until the overlay is shown; a disabled recorder drops every call at its first branch (`Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header"). AI stores the reference at start step 1 and never tests for null.

`Zenith_WorldAttachContext::GetWorldPurpose() → Zenith_WorldPurpose` (`WORLD_PURPOSE_PLAY`, `_EDIT`, `_PREVIEW`) is declared (core-doc item X243 (b), applied in patch 2026-10-09e; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"); AI reads it to register only `AI.NavIngest` in edit and preview worlds (EDIT's ED6; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module behaviour in edit and preview worlds").

<!-- assumption: pending SCENE, PHYS -->
| Interface | Used for | Row |
|---|---|---|
| a cook-side walk of an opened scene document (`Zenith_OpenedDocument`) listing entities with their stable component names, property values and composed world transforms | gathering static colliders and area volumes (`Zenith2/Docs/Design/AI/Navigation.md` → "Bake inputs") | AI5 |
| `Zenith_DecodeCollisionNeutralTriangles(Zenith_ByteSpan xZcol, Zenith_Arena&) → triangles` over a `.zcol` neutral geometry section, and the collider component's layer and material reference in scene data | collider triangles and their areas | AI6 |
| `LIFECYCLE_EVENT_ORIGIN_SHIFTED`, dispatched by `Scene.ShiftWorldOrigin` last in step 7 of sync point 1 with the played `Zenith_WorldOriginShift` as payload, after SCENE shifted every transform and every `PROPERTY_FLAG_WORLD_POSITION` component property (container elements are not visited, so AI shifts its one container-held position itself) | AI's shift at the sync point (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift") | AI19 (PHYS's PH20, X264 (j)) |
| a cook-side walk of a decoded cooked scene (`Zenith_SceneData` from `Zenith_DecodeCookedSceneFile`) giving the same entity, component and composed-transform listing as the document walk | reading WORLD's `navInput:` scene products in the bake (`Zenith2/Docs/Design/AI/Navigation.md` → "Bake inputs"; row WD23) | AI5, AI23 |
<!-- /assumption -->

<!-- assumption: pending CORE (core-doc item X314; ruling X314-FMA3; rows AI22, WD29) -->
CORE's follow-up to X314 declares the deterministic transcendentals in `Zenith2/Engine/ZenithCore/Zenith_Maths.h` with fixed polynomial implementations specified to the bit for binary64, under the names WORLD already uses: `Zenith_Sin(double)`, `Zenith_Cos(double)`, `Zenith_Atan2(double, double)`, and `Zenith_Exp(double)`. Every AI simulation path and tracked or baked output uses them and never a CRT transcendental: the locomotion yaw and its quaternion, the sight cones' cosines, the volume pitch, the bake's slope threshold `Zenith_Cos(maxSlope)` (a key input, computed once per class) and `AI.Logistic`, whose exact 0.5 at x = x₀ (`Test_AIUtility_ScoringAndMomentum`) needs `Zenith_Exp(0) == 1` exactly, which any range-reduced polynomial with an exact constant term gives (row AI22); a rename by CORE renames AI's calls in the same diff. Luau's own `math.*` transcendentals are SCRIPT's under the same ruling's subsequent sweep (row AI22); AI's libraries call none of them.
<!-- /assumption -->

<!-- assumption: pending ENV -->
ENV's importer stage `Environment.Navigation` writes, for each `navigation:<x>_<z>` product (kind 15), the blob `ai.navSource` by calling `Zenith_AIWriteNavSourceIntermediate` (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"), and ENV provides `Zenith_ListWaterBodiesForNavigation(const Zenith_TerrainDocumentView&, Zenith_Arena&) → Zenith_Span<const Zenith_WaterBodyShape>` for the volume bake's water prisms, which AI's cook step calls behind `Zenith_AIBakeInputSource::ListWaterBodies` (row AI3).
<!-- /assumption -->

<!-- assumption: pending SCRIPT (row AI4; core-doc item X288) -->
SCRIPT reserves ZS2500–ZS2599 for AI in `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" (ruling MISUSE-BLOCKS-2, X288); `Zenith2/Docs/Scripting.md`'s `ctx:RequestPath` is realised as `AI.RequestPath`; Z2-B-SCRIPT-026's value list gains pathfinding (ruling Q19 (a)); a ticket-result decoder's `SCRIPT_VALUE_KIND_STRUCT` field may point into its payload span (row AI4 (e)). The random draw `AI.RandomReachablePoint` needs is SCRIPT's existing `Zenith_ScriptInvocationContext::NextRandomDraw()` (`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types"), so row AI4 (d) is answered.
<!-- /assumption -->

SCRIPT's registry has assigned `ASYNC_SOURCE_NAVIGATION` the value 13 (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values", in-flight capacity `ai.nav.maxLoadsInFlight`; row AI18 answered); AI cites the enumerator and its assigned value and declares no value of its own (ruling ASYNC-SOURCE-REGISTRY).

<!-- assumption: pending SAVE, ASSET, EDIT, WORLD (rows AI2, AI11, AI12, AI14; PHYS's PH22) -->
`Zenith_SaveRestoreContext::ReportCoherentRestore(uint64 ulParticipantKey) const`, callable from `RestoreSection` and `IsRestoreReady`, with which a participant whose capture was EXACT reports that its restore is only COHERENT (PHYS's row PH22, `Zenith2/Docs/Design/PHYS/Simulation.md` → "Save and restore algorithm"), used for AI's one restore-time downgrade (`Zenith2/Docs/Design/AI/Agents.md` → "Fidelity"); ASSET keeps SOURCE_GEOMETRY navigation files out of packaged mounts (row AI2); EDIT hosts AI's shared authoring code in `ZenithAuthoring` (`Zenith2/Tools/Authoring/Internal/AI/` with the root header `Zenith_AuthoringAI.h`, ruling Q84 (a)) and AI's editor extension in `Tools/ZenithEditor/Internal/Navigation/` (row AI12); WORLD's partition cook calls `Zenith_AIMakeNavigationManifestView` and `Zenith_AIWriteCellNavigationContent` with WORLD's `Zenith_WorldProduct`, `Zenith_WorldCellBounds` and `Zenith_WorldCellContentWriter` (rows AI11, WD11).
<!-- /assumption -->

### Areas that consume AI

| Area | Uses |
|---|---|
| SCRIPT | the `AI` bindings, `ASYNC_SOURCE_PATHFINDING` results, the `@ai/` libraries |
| GAME | the Stage 4 lifecycle suite (`ZenithAI` linked), the world-transition scenario's path requests, census rows |
| STREAM | none called; NAVIGATION cells carry AI's components |
| ENV | calls AI's ZNSG writer from its importer |
| WORLD | the bake, the cell contribution, the bake panel |
| EDIT | the AI extension, the bake functions for regeneration |
| SAVE | the AI participant |
| DEVT | debug draw, counters, the `ai.dump` console command |
| OpenWorldSlice, WorkloadCharacters | AI at scale |

## Library and file layout

### Target summary

```cmake
zenith_add_library(ZenithAI KIND MODULE MODULE_NAME AI
	DEPENDS ZenithRuntime ZenithPhysics ZenithScene ZenithECS ZenithReflect ZenithAssets ZenithSave ZenithPlatform ZenithJobs
		ZenithCore ZenithProfiling
	SUPPORTED ZENITH_GPU)
# After a D-SEL-NAV ruling for Recast/Detour the call gains: THIRD_PARTY recast-detour
```

### ZenithAI files

```
Zenith2/Engine/ZenithAI/
	Zenith_AITypes.h  Zenith_AIErrors.h  Zenith_NavMesh.h  Zenith_NavQuery.h  Zenith_NavBuild.h  Zenith_NavVolume.h
	Zenith_NavMeshAsset.h  Zenith_AIComponents.h  Zenith_AIEvents.h  Zenith_AICommands.h  Zenith_AIWorldResource.h
	Zenith_AIDebug.h  Zenith_Module_AI.h
	Scripts/      BehaviourTree.luau, Utility.luau, StateMachine.luau, Agent.luau, Blackboard.luau, Trace.luau
	Internal/
		Zenith_AIWorldState.h/.cpp       the per-world state and the six internal resources (AI/Internals.md "Zenith_AIWorldState header")
		Module/      Zenith_AIModule.cpp, Zenith_AIAttach.cpp, Zenith_AIConfig.cpp, Zenith_AIRegisterTypes.cpp, Zenith_AILifecycle.cpp
		NavMesh/     Zenith_NavMeshRuntime.cpp (tile table, sorted external links), Zenith_NavIngest.cpp (and its merge function),
		             Zenith_NavLoads.cpp (load records, join list, publication handler), Zenith_NavOverlay.cpp,
		             Zenith_NavLinks.cpp, Zenith_NavTileCodec.cpp (the neutral Zenith_NavTileView), Zenith_NavMeshLoader.cpp,
		             Zenith_NavOriginShift.cpp
		Search/      Zenith_NavTileView.h, Zenith_NavSearch.cpp (the one loop of both D-SEL-NAV candidates), Zenith_NavStraightPath.cpp,
		             Zenith_NavRaycast.cpp, Zenith_NavSurface.cpp, Zenith_NavNearest.cpp, Zenith_NavRandom.cpp
		Build/       Zenith_NavRasterize.cpp, Zenith_NavFilter.cpp, Zenith_NavCompact.cpp, Zenith_NavAreas.cpp,
		             Zenith_NavRegions.cpp, Zenith_NavContours.cpp, Zenith_NavPolyMesh.cpp, Zenith_NavDetail.cpp,
		             Zenith_NavAssemble.cpp, Zenith_NavLayers.cpp, Zenith_NavCarve.cpp
		RecastDetour/  Zenith_NavRecastStages.cpp, Zenith_NavDetourBridge.cpp (dtNavMesh storage and its Zenith_NavTileView)
		               (only after a Recast/Detour ruling)
		Volume/      Zenith_NavVolumeBuild.cpp, Zenith_NavVolumeSet.cpp, Zenith_NavVolumeSearch.cpp, Zenith_NavVolumeCodec.cpp
		Paths/       Zenith_AIPathService.cpp, Zenith_AIPathConsumer.cpp (the consumer and the path inbox)
		Agents/      Zenith_AILocomotion.cpp, Zenith_AICorridor.cpp, Zenith_AISteering.cpp, Zenith_AIOrca.cpp,
		             Zenith_AIOrca3D.cpp, Zenith_AICrowdGrid.cpp, Zenith_AIOffMeshTraversal.cpp, Zenith_AISignificance.cpp
		Perception/  Zenith_AIPerception.cpp (and its merge function), Zenith_AIPerceptionLedger.cpp, Zenith_AIStimulusGrid.cpp,
		             Zenith_AINoiseBuffer.cpp
		Blackboard/  Zenith_AIBlackboardStore.cpp
		Commands/    Zenith_AICommands.cpp (recording functions and playback)
		Script/      Zenith_AIScriptApi.h, Zenith_AIScriptApi.cpp, Zenith_AIScriptBindings.cpp, Zenith_AIPathDecoder.cpp
		Save/        Zenith_AISaveParticipant.h/.cpp, Zenith_AISaveCodec.cpp, Zenith_AISearchSave.cpp (search snapshots)
		Debug/       Zenith_AIDebugDraw.cpp, Zenith_AIDump.cpp, Zenith_AITrace.cpp, Zenith_AIStateHash.cpp, Zenith_AICounters.cpp
	Tests/  Support/ (header-only), Conformance/, Fuzz/, Zenith_<Type>_Tests.cpp (Zenith2/Docs/Design/AI/Tests.md)
```

No AI source is under `Internal/Windows|Android|Linux/`: AI is platform-neutral CPU code.

### Tool-side files

```
Zenith2/Tools/Authoring/
	Zenith_AuthoringAI.h                      the root header (AI/Internals.md "Zenith_AuthoringAI header"): Zenith_AIBakeInputSource,
	                                          Zenith_AIWriteNavSourceIntermediate, Zenith_AIReadNavSourceIntermediate, Zenith_AIGatherBakeInputs,
	                                          Zenith_AIGatherVolumeBakeInputs, Zenith_AIMakeNavigationManifestView,
	                                          Zenith_AIWriteCellNavigationContent, Zenith_RegisterAIDocumentTypes
	Internal/AI/
		Zenith_AINavSourceIntermediate.cpp    the ZNSG writer and reader (ENV's importer calls the writer)
		Zenith_AIBakeInputs.cpp               scene and terrain input gathering (the cook step and the editor's in-process bake call it)
		Zenith_AICellContent.cpp              the manifest view and the per-cell contribution WORLD's partition cook calls
		Zenith_AINavigationDocuments.cpp      the two document types and their migrations (ZenithCook and the editor register them)
	Tests/AI/Zenith_AIAuthoring_Tests.cpp    ZenithAuthoring_Tests (AI/Tests.md "Test targets")
Zenith2/Tools/ZenithCook/Internal/AI/
	Zenith_AINavigationImporter.cpp           importers ai.navSettings and ai.navigation
	Zenith_AINavMeshCookStep.cpp              the kind-15 step ai.navMesh (every role)
	Zenith_AICookRegistration.h/.cpp          Zenith_RegisterAICookSteps (ZenithCook's, never ZenithAuthoring's: X301)
Zenith2/Tools/ZenithCook/Tests/AI/           the importer and step tests
```

### Editor files

```
Zenith2/Tools/ZenithEditor/Internal/Navigation/
	Zenith_AIEditorExtension.cpp      the extension (panels, gizmos, kind-15 handler, document handlers)
	Zenith_NavBakePanel.cpp           bake selected tiles in process, bake diagnostics, tile statistics
	Zenith_AIDebuggerPanel.cpp        the selected agent's state, corridor, memory, blackboard and trace
	Zenith_AIGizmos.cpp               link endpoints, obstacle shapes, area volumes, volume regions
Zenith2/Tools/ZenithEditor/Tests/Navigation/
```

### Integration suite and benchmark files

`Zenith2/Tests/AI/` (integration suite `Zenith_Integration_AI`), `Zenith2/Benchmarks/NavigationSelection/` (the D-SEL-NAV harness `Zenith_Bench_NavigationSelection`, with its own `Internal/Custom/` prototype until the ruling). AI has no `Zenith2/Benchmarks/Budgets/AI/` harness: its per-system rows are allocated from reference-engine and published data (`Zenith2/Docs/Budgets.md` → "Per-system rows").

### ZENITH_SUPPORTED matrix

| Target | win-debug, win-release | win-tools | android-arm64 | android-x86_64 | win-asan | linux-tsan |
|---|---|---|---|---|---|---|
| ZenithAI | yes | yes | yes | yes (as every `ZENITH_GPU` module) | yes | no (`ZENITH_GPU`, because ZenithPhysics and ZenithRuntime are) |
| `ZenithAI_Tests`, `ZenithAI_Conformance` | yes | yes | `ZenithAI_Tests` through the test APK | `ZenithAI_Tests` through the emulator's test APK (smoke) | yes | no |
| `Zenith_Bench_NavigationSelection` | win-release only (its smoke runs in the win-release `Test` step) | — | yes | — | — | no |

## Public API

### Header map

| Header | Content | Specified in |
|---|---|---|
| `Zenith_AITypes.h`, `Zenith_AIErrors.h` | limits, enumerations, references, filter, errors | `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_AITypes header", "Zenith_AIErrors header" |
| `Zenith_NavMesh.h`, `Zenith_NavQuery.h`, `Zenith_NavVolume.h` | the read-only navigation views and every query | `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavMesh header", "Zenith_NavQuery header", "Zenith_NavVolume header" |
| `Zenith_NavBuild.h` | the bake pipeline shared by cook, editor and carving | `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavBuild header" |
| `Zenith_NavMeshAsset.h` | the kind-15 runtime object | `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavMeshAsset header" |
| `Zenith_AIComponents.h`, `Zenith_AIEvents.h`, `Zenith_AICommands.h`, `Zenith_AIWorldResource.h`, `Zenith_AIDebug.h` | the module's ECS-facing surface | `Zenith2/Docs/Design/AI/PublicAPI.md` |
| `Zenith_Module_AI.h` | the two entry points | `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_Module_AI header" |

### Rules across the AI API

- Every recoverable failure is `Zenith_Result<…, Zenith_AIError>` or a counted no-op; untrusted data (cooked files, documents, saves, script arguments) never asserts.
- No third-party type appears in a public header; with Recast/Detour selected its types stay in `Internal/RecastDetour/`.
- Gameplay code never mutates navigation state directly: it records commands, reads components and queries through `Zenith_AIWorldResource` and `Zenith_AIBlackboardStore`; only the module's nodes and command playback write.
- Every query result is a deterministic function of the committed state and the arguments, independent of slot assignment.

### Asset kind

| Code | Enumerator | Stable name | Script name | Runtime object type | Extension | Magic | Memory subsystem | Fallback | Cacheable | GPU resident |
|---|---|---|---|---|---|---|---|---|---|---|
| 15 | `ASSET_KIND_NAV_MESH` | `AI.NavMesh` | `NavMesh` | `Zenith_NavMeshAsset` | `.znav` | `ZNAV` | SIMULATION | FAIL | no | no |

The loader (`Zenith_AssetKindLoaderDesc` `{ "AI.NavMeshLoader", uAI_NAV_FORMAT_VERSION, 0 }`) runs on asset-loader jobs: it requests the role's sections, decodes TILE and VOLUME sections into one block owned by the object, keeps LAYERS stored bytes, validates every check of `Zenith2/Docs/Design/AI/Formats.md`, reports resident bytes under `MEMORY_SUBSYSTEM_SIMULATION`, and activates an immutable object; a failure is `Fail` with the `Zenith_AIErrorCode` as loader code.

### Command kinds

Registered at module start through `RegisterCommandKind`, allowed in phases 1 … 10, payloads of `Zenith2/Docs/Design/AI/Formats.md` → "Command payloads":

| Order | Name | Recording functions | Playback |
|---|---|---|---|
| 1 | `AI.Agent` | `Zenith_RecordAIMoveTo`, `…Stop`, `…Pause`, `…SetSpeedScale`, `…SetMedium` | `Zenith2/Docs/Design/AI/Agents.md` → "Move commands" |
| 2 | `AI.Noise` | `Zenith_RecordAINoise` | append to the noise buffer (`Zenith2/Docs/Design/AI/Agents.md` → "Hearing algorithm") |
| 3 | `AI.Stimulus` | `Zenith_RecordAIStimulus` | append to the noise buffer as a damage or touch entry |
| 4 | `AI.Blackboard` | `Zenith_RecordAIBlackboardSet`, `…Clear`, `…ClearAll` | `Zenith2/Docs/Design/AI/Agents.md` → "Blackboard commands" |
| 5 | `AI.OffMeshLink` | `Zenith_RecordAICompleteOffMeshLink`, `Zenith_RecordAISetLinkEnabled` | the link traversal state machine; property 10 Enabled |
| 6 | `AI.Perception` | `Zenith_RecordAIForgetTarget`, `Zenith_RecordAIForceAware` | `Zenith2/Docs/Design/AI/Agents.md` → "Damage and touch" |

Playback keys per-world state by `Zenith_WorldPlaybackAccess::GetWorldTag()` (the kind table is per process); a command for a missing entity or component is counted (`ai.command.no_agent_delta`) and ignored.

### Module start and world attach

`Zenith_Module_AI_RegisterTypes` (startup step 6): the eleven components, `Zenith.AI.PerceivedTarget`, `Zenith.AI.PathCorners`, the four events, the six payload structs, the eight world resource types, the configuration keys and the census metadata; a refused registration is `MODULE_ERROR_REGISTRATION`. The two document types are not registered here: ZenithAI has no ZenithDocument edge, so they register only through the tool-side `Zenith_RegisterAIDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"), which ZenithCook's registration and the editor call (the ANIM precedent).

`Zenith_Module_AI_Start` (startup step 15), each step a fault point, unwound in reverse:

| Step | Action | Failure |
|---|---|---|
| 1 Config | read and validate every `ai.*` key into `Zenith_AIConfig`; parse the team names and LOD strings; read P = `GetScriptAsyncCapacity(ASYNC_SOURCE_PATHFINDING)` and refuse `ai.path.maxQueuedRequests < ai.agent.maxAgents + P` (`Zenith2/Docs/Design/AI/Navigation.md` → "Request queue") | `MODULE_ERROR_REGISTRATION` |
| 2 Kind loader | register the kind-15 loader with the asset registry | `MODULE_ERROR_REGISTRATION` |
| 3 Command kinds | the six rows of "Command kinds" | `MODULE_ERROR_REGISTRATION` |
| 4 Script | `RegisterScriptBindings` (the `AI` namespace and the path result decoder); `DeclareScriptAsyncSource(ASYNC_SOURCE_PATHFINDING, uAI_PATH_RESULT_BYTES)`; `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, …)` | `MODULE_ERROR_REGISTRATION` |
| 5 Memory | `RegisterMemoryConsumer` | — |

`AttachWorld` per world, unwound in reverse by `DetachWorld` or on failure:

| Step | Action |
|---|---|
| 1 | resolve `Zenith_AICommandKinds` (first attach) and the property IDs AI reads (`Zenith.LocalTransform`, `Zenith.WorldTransform`, `Zenith.Physics.CharacterController`, `Zenith.Streaming.RegionBound`, `Zenith.WorldOrigin`, `Zenith.Physics.World`); read the world's purpose (`GetWorldPurpose()`) |
| 2 | the world state (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AIWorldState header"): tile table (`ai.nav.maxTiles` slots), load table and completion queue `ai.loads.w<tag>` (`ai.nav.maxLoadsInFlight`), agent table (`ai.agent.maxAgents`), path service arenas (`ai.path.maxActiveRequests` × node pool), path inbox (P), grids, perception ledger, blackboard store, noise buffer, trace staging (development builds) |
| 3 | `AddWorldResource` for `Zenith.AI.World`, `Zenith.AI.Blackboards`, `Zenith.AI.CrowdGrid`, `Zenith.AI.StimulusGrid`, `Zenith.AI.PathService`, `Zenith.AI.AgentRuntime`, `Zenith.AI.PerceptionLedger`, `Zenith.AI.NoiseBuffer` (owning phase `PHASE_AI_NAVIGATION`); bind the world's `Zenith_AINavPublicationHandler` (the module's handler table entry for the world tag, created on first attach; `Zenith2/Docs/Design/AI/Internals.md` → "Navigation load records"); then, on `GetPublicationSink()`, `DeclareSource(ASYNC_SOURCE_NAVIGATION, Zenith_ModuleSourceDesc{ ai.nav.maxLoadsInFlight, 24, MODULE_SOURCE_SITE_MERGE_CHAIN })` followed by `SetSourceHandler(ASYNC_SOURCE_NAVIGATION, xHandler)` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Source declaration and handler registration"). Both are registrations: a rejection is logged by the sink and fails `AttachWorld` with `MODULE_ERROR_REGISTRATION` once it returns; at the maximum 16,384 × 24 bytes = 384 KiB (24 KiB at the default 1,024) count toward the world's 8 MiB `ulMODULE_PUBLICATION_MAX_ARENA_BYTES` |
| 4 | register the nodes of `Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement" (only `AI.NavIngest` when the purpose is `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW`) and the epilogue stage `AI.DebugDraw` |
| 5 | `AddLifecycleListener` ("Lifecycle listener") |
| 6 | register the save participant (`WORLD_PURPOSE_PLAY` worlds only) |

### Lifecycle listener

| Event | AI's action |
|---|---|
| `LIFECYCLE_EVENT_SPAWNED` | none (records are created by `AI.Locomotion` from components) |
| `LIFECYCLE_EVENT_TELEPORTED` | mark the agent record: corridor and corners dropped, stuck window restarted, a replan queued when a move is active |
| `LIFECYCLE_EVENT_DESTROY_PENDING` | none |
| `LIFECYCLE_EVENT_DESTROYED` | mark the agent record for removal and its requests for cancellation; remove the entity's blackboard records; a perceiver's memory entries naming it are removed at the next perception update |
| `LIFECYCLE_EVENT_RAGDOLL_ENTER`, `…_RECOVER` | the ragdoll rows of `Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine" |
| `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (null entity; payload the played `Zenith_WorldOriginShift`) | the shift table of `Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift", on the sync-point node of sync point 1 |
| cinematic events | none: a cinematic that drives an agent pauses it with `AI.Pause` |

### Handle kinds used

`Zenith_NavPolyRef` and `Zenith_NavVolumeRef` are world-scoped generational references, never serialized (`Zenith2/Docs/Contracts.md` → "Handle kinds"); `Zenith_AssetHandle<Zenith_NavMeshAsset>` holds navigation files; `Zenith_EntityHandle` names agents, perceivers, targets and blackboard owners; persistent IDs name assets in files. AI holds `Zenith_OperationHandle`s for exactly two things: each navigation load record's `RequestLoad` operation, from issue until its publication or abandonment (`Zenith2/Docs/Design/AI/Navigation.md` → "Navigation asset requests"), and, during a restore only, the restore-queue acquisitions `ListRestoreWaits` lends to SAVE's synchronous driver (`Zenith2/Docs/Design/AI/Agents.md` → "Restore").

## Data structures and formats

### Format index

| Format | Heading |
|---|---|
| `.znav` and its sections | `Zenith2/Docs/Design/AI/Formats.md` → "Navigation file" |
| ENV's navigation source intermediate | `Zenith2/Docs/Design/AI/Formats.md` → "Navigation source intermediate" |
| the tile bake job | `Zenith2/Docs/Design/AI/Formats.md` → "Tile bake intermediate" |
| the two authoring documents | `Zenith2/Docs/Design/AI/Formats.md` → "Navigation settings document", "Navigation document schema" |
| command, request and result payloads | `Zenith2/Docs/Design/AI/Formats.md` → "Command payloads", "Script request payload", "Script result payloads" |
| the save section | `Zenith2/Docs/Design/AI/Formats.md` → "Save section layout" |

### In-memory sizes that matter

| Record | Size |
|---|---|
| agent runtime record | 4,096 bytes (corridor 2,048, volume corners 768) |
| path search node / search arena block at 2,048 nodes | 32 bytes / B(N) = 40 N + 16 S + L(N) + L(S) with L(n) = 16 n + 8 P(n) (`Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing": nodes, heap, heap indices, tile stamps and the two reserved `Zenith_HashMap` lookups) = 245,760 bytes = 240 KiB at N = S = 2,048 (120 bytes per node); 64 active requests 15 MiB; saved search state at most 52 bytes per node, ≤ 6.5 MiB at the defaults |
| decoded tile | 4 to 12 KiB for open terrain at 32 m and 0.25 m cells, 30 to 60 KiB for built-up tiles |
| layers section (resident, compressed) | 8 to 20 KiB per carve-capable tile |
| volume node | 48 bytes; a 256 m region at 0.5 m is 9 to 18 MiB |
| crowd grid entry / stimulus grid entry | 48 / 40 bytes |
| blackboard record | 80 bytes; 65,536 records 5 MiB plus the hash index and the 24-byte-per-entry expiry index |
| perceived-target entry | 48 bytes; 16 per perceiver |
| perception ledger entry | 672 bytes per perceiver (16 emitted levels, 16 pending forgets) |
| navigation load record / join entry / supplier record | 72 / 24 / 400 bytes; at most `ai.nav.maxLoadsInFlight` records, one supplier record per navigation entity in an active region |
| trace record | 32 bytes; 64 × 256 agents = 512 KiB (development builds) |

Navigation residency is bounded by streaming: at W-OpenWorld's defaults the resident tiles are those of the active NAVIGATION cells, not the world's; every byte is counted by the memory consumer against Z2-B-AI-001.

### Byte order and serialization rules

Little-endian, floats by bit pattern, persistent IDs in their 16-byte form, no pointer, slot, polygon reference or interned ID on disk (`Zenith2/Docs/Design/AI/Formats.md` → "Common rules").

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading |
|---|---|
| bake stages 1 … 10 | `Zenith2/Docs/Design/AI/Navigation.md` → "Bake stages" |
| tile add, connect, remove, origin shift | `Zenith2/Docs/Design/AI/Navigation.md` → "Tile add procedure", "Tile connection procedure", "Tile remove procedure", "Origin shift" |
| mark and carve | `Zenith2/Docs/Design/AI/Navigation.md` → "Mark procedure", "Carve procedure" |
| link registration | `Zenith2/Docs/Design/AI/Navigation.md` → "Link registration" |
| A*, straight path, surface movement | `Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing", "Straight path extraction", "Surface movement" |
| path service tick | `Zenith2/Docs/Design/AI/Navigation.md` → "Per-tick processing" |
| spatial queries | `Zenith2/Docs/Design/AI/Navigation.md` → "Nearest point", "Navmesh raycast", "Random reachable point" |
| volume bake, search, smoothing, line of sight | `Zenith2/Docs/Design/AI/Volumes.md` |
| corridor, steering, ORCA, locomotion output, crowd grid | `Zenith2/Docs/Design/AI/Agents.md` |
| LOD, sight, hearing, memory | `Zenith2/Docs/Design/AI/Agents.md` → "Simulation LOD", "Sight algorithm", "Hearing algorithm", "Perception memory" |
| behaviour tree tick, utility selection, state machine update | `Zenith2/Docs/Design/AI/Scripting.md` |
| save and restore | `Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm" |

### State machine index

| Machine | Heading |
|---|---|
| agent | `Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine" |
| path search | `Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing" |
| path request | `Zenith2/Docs/Design/AI/Navigation.md` → "Path request state machine" |
| off-mesh link traversal | `Zenith2/Docs/Design/AI/Navigation.md` → "Link traversal state machine" |
| hierarchical state machine library | `Zenith2/Docs/Design/AI/Scripting.md` → "State machine library" |

### Tick-level flow of AI work

1. Sync points after phases 1 … 6: playback of `AI.Agent`, `AI.Noise`, `AI.Stimulus`, `AI.Blackboard`, `AI.OffMeshLink`, `AI.Perception` commands recorded in those phases; script path requests committed in phase 2 reach the path inbox at their merge; at sync point 1 of a rebase tick, after SCENE's shift, AI's listener shifts AI's state (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift").
2. Phase 7: `AI.NavIngest` (join published tiles, drain loads, collect needs, removals, obstacles, carving, links; its merge node issues the loads) → `AI.Significance` → `AI.CrowdGrid` and `AI.StimulusGrid` → `AI.NavRequests` (plans and script requests, sliced) → `AI.Perception` (its merge node: the ledger) → `AI.Locomotion` (corridor, steering, avoidance, output commands, events; its merge node: the accepted outputs).
3. Sync point after phase 7: AI's own `SetProperty` commands (controller input, kinematic transforms, applied velocity) and teleports apply; phase 8 propagates transforms; phase 9 delivers `MoveCompleted`, `OffMeshLinkReached`, `Stimulus` and `TargetForgotten`.
4. Phase 10: Late behaviours react; their AI commands apply at the sync point after phase 10 and are seen by the next tick's phase 7.
5. Phase 11: the AI participant captures when a save is requested; the tick boundary publishes script path results and navigation load results parked in phase 7, and AI's navigation handler fills the join list the next phase 7 consumes.
6. Epilogue: `AI.DebugDraw` (every build; it draws only while the recorder is enabled and `ai.debug.views` is non-zero).

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_NavMesh`, `Zenith_NavVolumeSet`, `Zenith_AIWorldResource` | const members `ANY_THREAD` for nodes declaring `ReadResource` of `Zenith.AI.World` (`ComputeStateHash` excepted: `MAIN_THREAD` with no frame of the world running); written by `AI.NavIngest` and its merge node, by the navigation publication handler at the tick boundary and by the origin-shift listener at sync point 1, never two at once |
| `Zenith_AIBlackboardStore` | const members `ANY_THREAD` for readers of `Zenith.AI.Blackboards`; written by command playback alone |
| `Zenith_NavPathSearch`, `Zenith_NavVolumeSearch` | `OWNER_THREAD`: one sub-job steps one search |
| `Zenith_NavBuild` functions | `ANY_THREAD`, each call touching only its arguments and arena |
| `Zenith_NavMeshAsset` | immutable after activation, `ANY_THREAD` |
| components | per the census rows (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Authority census rows") |
| the module instance, start and attach | `MAIN_THREAD` |

### Node summary

The seven phase-7 systems and their access are `Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement"; each internal world resource has exactly one writer in the stage.

### Lock-order entries

AI adds no row to `Zenith2/Docs/Contracts.md` → "Lock ordering table": AI takes no lock. Shared navigation state is reached only through declared frame-graph access, loader jobs build immutable objects and hand them over through ZenithAssets, playback runs at sync points with no node of the world running, and every sub-job writes only its own records.

### Construction and shutdown

Start and attach are the step tables of "Module start and world attach". Shutdown: `DetachWorld` per world (participant unregistered, listener removed, nodes removed with the world's schedule, resources removed, requests discarded, asset handles released, the world's `Zenith_AINavPublicationHandler` unbound but kept, world state destroyed), then the module instance at shutdown step 6: memory consumer unregistered, loader unregistered after the registry has retired every kind-15 asset, the navigation handler table freed with the module instance (the CINE precedent of `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding": MOD's handler contract requires a handler to outlive the world's table).

### Cancellation

AI owns no background thread. Path requests are tick state cancelled by their owners (`Zenith2/Docs/Design/AI/Navigation.md` → "Cancellation"); a navigation load whose requester goes is abandoned with `RequestCancel` and its completion ID completed once ("Navigation asset requests" in the same file). Frame cancellation (`RequestCancel` on the executor) is for shutdown and world destruction ([Z2-N-ARCH-412]): a cancelled instance skips AI node bodies and the world is destroyed, so nothing of the cancelled tick survives; `DetachWorld` cancels every load operation AI holds.

### Error enumerations

`Zenith_AIErrorCode` (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_AIErrors header"); module entry points return `Zenith_ModuleError`; the loader returns the asset registry's errors with the AI code as loader code; cook diagnostics name the `Zenith_AIErrorCode` and return `COOK_ERROR_STEP_FAILED` or `IMPORT_ERROR_SOURCE_INVALID`; the participant returns `Zenith_SaveError`.

### Luau errors and asserts

Script misuse of an AI binding raises its catalogued Luau error (`Zenith2/Docs/Design/AI/Scripting.md` → "Misuse codes") and never asserts; library definition errors fail the module's load; `ZENITH_ASSERT` guards only engine invariants (a node outside phase 7, a record written by two sub-jobs, a search stepped after `Reset`).

### Determinism obligations

Rules A1 … A10 of `Zenith2/Docs/Design/AI/Agents.md` → "Determinism rules", proved by `Zenith2/Docs/Design/AI/Tests.md` → "Save and determinism tests".

## Diagnostics and profiling

### Counters

Published per world (suffix `.<worldTag>`) once per tick through the counter hook with their kind (`Zenith2/Docs/Contracts.md` → "No mutable global state"):

| Counter | Kind | Meaning |
|---|---|---|
| `ai.nav.tiles`, `ai.nav.loads_in_flight`, `ai.nav.links`, `ai.nav.links_unresolved`, `ai.nav.rebuild_queue`, `ai.nav.resident_bytes` | gauge | loaded tiles, navigation load records from allocation to publication, links, the carve queue, navigation bytes |
| `ai.nav.tiles_refused`, `ai.nav.settings_mismatch`, `ai.nav.wrong_role`, `ai.nav.links_dropped`, `ai.nav.mark_truncated`, `ai.nav.loads_failed`, `ai.nav.origin_shifts` | cumulative | refusals, failed loads, shifts applied |
| `ai.nav.rebuilds_delta`, `ai.nav.carve_fallback_delta`, `ai.nav.carve_failed_delta`, `ai.nav.loads_deferred_delta`, `ai.nav.origin_late_delta` | delta | carving; loads beyond `ai.nav.maxLoadsInFlight`; a shift applied at ingest (a defect path) |
| `ai.path.active`, `ai.path.queued` | gauge | the path service's active set (the MP-COUNTER of Z2-B-AI-003) and queue |
| `ai.path.deferred_delta`, `ai.path.completed_delta`, `ai.path.partial_delta`, `ai.path.failed_delta`, `ai.path.cancelled_delta`, `ai.path.restarted_delta`, `ai.path.superseded_delta`, `ai.path.queue_full_delta`, `ai.path.expansions_delta`, `ai.path.consumer_defects_delta` | delta | path traffic |
| `ai.agent.count`, `ai.agent.moving` | gauge | agents |
| `ai.agent.refused`, `ai.agent.not_root` | cumulative | |
| `ai.agent.invalid_delta`, `ai.agent.stuck_delta`, `ai.agent.reemitted_delta`, `ai.agent.completion_dropped_delta`, `ai.locomotion.commands_delta`, `ai.locomotion.rejected_delta` | delta | |
| `ai.command.invalid_delta`, `ai.command.no_agent_delta`, `ai.command.dropped_delta`, `ai.command.unresolved_kind_delta` | delta | command playback outcomes |
| `ai.perception.perceivers` | gauge | |
| `ai.perception.rays_delta`, `ai.perception.rays_deferred_delta`, `ai.perception.invalid_delta`, `ai.perception.noise_dropped_delta`, `ai.perception.noise_ignored_delta`, `ai.perception.rejected_delta`, `ai.perception.reemitted_delta`, `ai.perception.forget_dropped_delta`, `ai.perception.ledger_full_delta` | delta | |
| `ai.blackboard.entries` | gauge | unexpired records |
| `ai.blackboard.full_delta`, `ai.blackboard.owner_full_delta`, `ai.blackboard.intern_full_delta` | delta | |
| `ai.trace.dropped_delta` | delta | development builds: trace records dropped by a full staging buffer |
| `ai.lod.tier0` … `ai.lod.tier3` | gauge | entities per tier |
| `ai.lod.foci_ignored`, `ai.save.coherent_fallbacks`, `ai.save.carve_mismatch` | cumulative | restore-time downgrades to COHERENT (`Zenith2/Docs/Design/AI/Agents.md` → "Fidelity") |
| `ai.resident_bytes` | gauge | the memory consumer's value |

### Profiler zones

`AI.NavIngest` with child zones `AI.NavIngest.Tiles`, `AI.NavIngest.Obstacles` and `AI.NavIngest.Carve` (one per rebuild sub-job); `AI.Significance`, `AI.CrowdGrid`, `AI.StimulusGrid`; `AI.NavRequests` with `AI.NavRequests.Step` per sub-job; `AI.Perception`, `AI.Locomotion` (sub-job zones by name); `AI.DebugDraw`; `AI.Load.NavMesh` on loader jobs. Z2-B-AI-002 is measured over the whole phase-7 span (row GA18): AI's seven systems and GAME's `Gameplay.Rules` and `Gameplay.Cameras`.

### On-screen profiler rows

One row group "AI" (a DEVT overlay tab added by AI's first counters ticket, `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers"): phase-7 ms (p50/p95), agents moving, active and queued path requests, deferrals per second, expansions per tick, rebuilds per tick, sight rays per tick, resident MiB.

### Log categories

`ai` (module, nodes, playback), `ai.nav` (tiles, links, carving), `ai.cook` (importers and step). Each distinct failure logs once per world per asset or entity with its counter name.

### Debug views

`ai.debug.views` bits (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_AIDebug header") drawn by the epilogue stage `AI.DebugDraw` through `Zenith_DebugDraw` from AI's state: navmesh by area, tiles and carved tiles, links, obstacles and blocked polygons, corridors and corners, preferred and chosen velocities, the selected agent's neighbours and ORCA half-planes, perception cones and rays, memory, noises (2 s), volume nodes around the selected agent, LOD tiers, the selected agent's trace. Compiled and registered in every build; the stage returns at once unless `Zenith_DebugDraw::IsEnabled()` and `ai.debug.views` is non-zero, and `ai.debug.views` is 0 in Shipping (`Zenith_AIConfig::m_uDebugViews`), so Shipping draws nothing for AI even when the overlay enables the recorder.

### AI dump command

The development console command `ai.dump [world]` (DEVT's console, `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry") writes `Zenith2/out/<checkout-hash>/dumps/ai_<world>_<tick>.json`: settings, loaded tiles (key, version, polygons, carved), links, obstacles, every agent (simulation ID, status, corridor as tile keys, corners, target), the path service queue and active set, perceivers' summaries and memories, blackboard records, all sorted by their keys, and prints the canonical AI state hash.

### Canonical AI state hash

`Zenith_AIWorldResource::ComputeStateHash()` (`MAIN_THREAD`, no frame of the world running) is a `Zenith_Hash64` stream over AI's module state that components do not already hold (the world hash covers components), in canonical order: the settings hash; the applied rebase count and applied origin; per loaded tile in key order its key, version, supplier (simulation ID, slot), external-link list (slot cache masked) and overlay bits; per loaded volume its asset, region and supplier; the supplier records in simulation-ID order with every slot's GUID and state; the load records in completion-ID order; the carve queue and the carved records (tile-local shapes); the obstacle applied records; per link registration in (entity, class) order; per agent record in simulation-ID order every field of the save form (corridor entries and corner references as tagged references: a polygon as (tile key, polygon), a link node as (link entity simulation ID, class), a dead link node as its tag alone, `Zenith2/Docs/Design/AI/Navigation.md` → "Search graph"); the path service inbox, queue and active set in order (descriptor, expansions, ticks, and each active search's whole snapshot with node references tagged likewise and its stamps as (tile key, version) or (volume asset GUID)); the perception ledger; the blackboard records unexpired at the current tick in (owner, key text) order; the noise buffer in order. It never reads a slot number or a pointer, so equal states hash equal across sessions (`Test_AIState_HashCanonical`); it covers exactly what the save section holds.

### Telemetry

None beyond the counters; AI sends nothing off the device.

## Authoring and cooking

### What AI authors

| Asset | Authored as | Cooked by |
|---|---|---|
| agent classes, areas, cost profiles | `*.navsettings.json` (`Zenith.NavigationSettingsDocument`) | importer `ai.navSettings`, step `ai.navMesh` (SETTINGS) |
| a navigable region | `*.navigation.json` (`Zenith.NavigationDocument`) | importer `ai.navigation`, step `ai.navMesh` (MANIFEST, TILE, VOLUME) |
| terrain navigation input | ENV's terrain importer, `navigation:<x>_<z>` products | step `ai.navMesh` (SOURCE_GEOMETRY) |
| agents, perceivers, stimulus sources, obstacles, links, area volumes, LOD foci | components in scenes and prefabs (SCENE's documents) | none (scene data) |
| behaviours and states | Luau behaviour modules using the `@ai/` libraries | SCRIPT's `script.modules` |

### Editor surfaces

AI's editor extension (`Tools/ZenithEditor/Internal/Navigation/`, registered through `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"; the "navigation" row of EDIT's extension table):

| Surface | Does |
|---|---|
| navigation bake panel | lists the project's navigation documents; bakes the selected document's tiles in process with `Zenith_NavBuildTile`, one background job per tile submitted at the tools-host priority `JOB_PRIORITY_BACKGROUND_TOOL` ([Z2-N-ARCH-443], ruling X350 (a); `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Worker loop": a tile bake runs far beyond the 2 ms background slice, and that priority is exempt from it on at most half the workers), and loads them into the edit world; shows bake diagnostics per tile and tile statistics (polygons, bytes, bake ms); requests a full cook through the cook server |
| regeneration | after a terrain stroke with `TERRAIN_REGENERATE_NAVIGATION` (`Zenith2/Docs/Design/ENV/Bake.md` → "Regeneration after edits") or a static collider edit, rebakes the affected tiles in process, one `JOB_PRIORITY_BACKGROUND_TOOL` job per tile as the bake panel does (`Test_AIEditor_RegenerateAfterTerrainStroke`); the cook's tile items run on ZenithCook's own workers under ZenithCook's rule, not this one |
| gizmos | off-mesh link endpoints, obstacle shapes, area volume boxes and volume region boxes (EDIT's component handle table rows for AI) |
| AI debugger panel | in a play world: the selected agent's state, corridor, corners, perception memory, blackboard and decision trace; a timeline of its last 64 trace records |
| inspectors | the generic reflected inspector for every AI component; the settings and navigation documents in the generic document editor with schema validation |
| kind-15 handler | opening a navigation file shows its tiles in the preview viewport |

Edit and preview worlds run `AI.NavIngest` alone (`Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement").

### Cook steps and derived-data keys

| Importer or step | Input | Output | Key adds |
|---|---|---|---|
| importer `ai.navSettings` | a settings document | intermediate `ai.navSettings` | the document's bytes, importer version |
| importer `ai.navigation` | a navigation document and the scene and terrain paths it lists (`ReadInput` for the document, references resolved to GUIDs) | the products of `Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema"; intermediates `ai.navTileJob` per product, `ai.navMappings` on the manifest | the document's bytes, the resolved GUIDs, importer version |
| step `ai.navMesh` (`COOK_PHASE_ASSET`, kind 15, `m_uVersion` 1, in process) | by intermediate: `ai.navSettings` → SETTINGS; `ai.navTileJob` → TILE or VOLUME; manifest products → MANIFEST; `ai.navSource` → SOURCE_GEOMETRY | the `.znav`; a MANIFEST item also publishes the property `ai.navMappings`, a SOURCE_GEOMETRY item `ai.navSource`; tile and volume items gather their inputs through `Zenith_AIGatherBakeInputs` / `Zenith_AIGatherVolumeBakeInputs` over the step's `Zenith_AIBakeInputSource` (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header") | `uAI_NAV_FORMAT_VERSION`; the settings hash; the class parameters; every opened scene document's SHA-256 and every dependency output's key hash (`Zenith2/Docs/Design/AI/Navigation.md` → "Bake determinism"); after a Recast/Detour ruling the pinned `recast-detour` version |

The step opens source documents, so it declares `m_bOutOfProcess` false (`Zenith2/Docs/Design/ASSET/Cook.md` → "Source documents in cook steps"). Every output is byte-identical for identical inputs across worker counts (`Test_NavCook_ByteIdenticalAcrossWorkers`).

**Product declaration input source** (AI design patch 2026-10-17, as corrected after its review). `Zenith_AIDeclareNavigationProducts` takes a `Zenith_AIBakeInputSource& xInputs` between `xSink` and `xArena`: `Zenith_AIDeclareNavigationProducts(const Zenith_JsonDocument& xDocument, Zenith_JsonNode xRoot, Zenith_StringView strManifestKey, Zenith_AINavigationProductSink& xSink, Zenith_AIBakeInputSource& xInputs, Zenith_Arena& xArena)`. The coarse occupancy pre-pass of `Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema" (the 4 m column test of the inputs' AABBs) reads one kind of bound through `xInputs` and nothing else: each terrain's `navigation:<x>_<z>` product by `ReadDependencyOutput(product, "ai.navSource")` (the source intermediate's vertex bounds plus its tile origin, in binary64 world metres). Scenes are never read by the declaration: a document whose `scenes` array is non-empty (any path or product entry) declares every tile of `bounds` for every class it bakes (every layer 0 … `uAI_NAV_MAX_LAYERS_PER_COLUMN` − 1 for a LAYERS class), because a static collider's triangles are its collision shape's geometry, which only the gather reads. The declaration may therefore name a tile with no input, never omits a tile that has one, and declares the same products whichever caller supplies the source; a declared tile whose bake yields no polygon is cooked to a TILE file with P = 0, so the manifest's hard dependencies, the cell content's tile handles and the runtime load always resolve. AI's importer `ai.navigation` implements the source over its `Zenith_ImportContext` (published properties); WORLD's world importer, calling in process, implements it over its source access and its import context's published properties (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Source access interface"). `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header" takes the signature in the ticket that declares the header, and `Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema" the pre-pass rule in the ticket that implements the declaration.

### Document types and migration

`Zenith.NavigationSettingsDocument` and `Zenith.NavigationDocument`, schema version 1 each, are registered by `Zenith_RegisterAIDocumentTypes` (ZenithAuthoring; ZenithCook's registration and the editor call it, never the module's `RegisterTypes`) and join the authoring document type list of [Z2-N-CONTRACT-552] in AI's document ticket (row AI8); every subsequent schema version adds a migration step with preview, backup, journaled write and unknown-member preservation ([Z2-N-CONTRACT-543]); cooked `.znav` files re-cook on any format or engine cook version change; the AI save section migrates forward (`Zenith2/Docs/Design/AI/Formats.md` → "Save section layout").

## Failure handling

### Failure table

| Failure | Detection | Response | Counter |
|---|---|---|---|
| corrupt or version-mismatched navigation file | loader checks | the load fails (FAIL policy); its tiles are absent; agents over them are SUSPENDED | asset registry; `ai.nav.tiles_refused` |
| settings mismatch | tile add | the file is refused | `ai.nav.settings_mismatch` |
| tile, link, obstacle or agent capacity | table full | the new item is refused; nothing else changes | `ai.nav.tiles_refused`, `ai.nav.links_dropped`, `ai.agent.refused` |
| allocation failure in a carve | rebuild error | the previous polygons stay | `ai.nav.carve_failed_delta` |
| allocation failure in a search | `Begin` error | the request finishes INVALID_REQUEST | `ai.path.failed_delta` |
| node pool exhausted, tick cap reached | search | PARTIAL result | `ai.path.partial_delta` |
| missing navmesh for an agent's class | class resolution | SUSPENDED; FAILED after 600 ticks | `ai.agent.moving` drops |
| invalid script argument | binding | the catalogued Luau error | the script runtime's error counters |
| capacity rejection at the merge | merge | that tick's commands and events are not delivered; the state written in place stays; the merge functions keep the last accepted values, so the next tick re-sends the output and re-emits every event from its ledger | `ai.locomotion.rejected_delta`, `ai.perception.rejected_delta`, `ai.agent.reemitted_delta`, `ai.perception.reemitted_delta` |
| a navigation file that fails to load at run time | its publication (PARKED_FAILED) | the tile is absent; its supplier slot becomes FAILED and is not requested again until that slot's handle changes by value or the supplier leaves and re-enters an active region; agents over it are SUSPENDED | `ai.nav.loads_failed` |
| corrupt save section, or a re-issue that SAVE's async section contradicts | participant | `SAVE_ERROR_CORRUPT`; the restore fails | SAVE's |
| content changed since the capture (a re-cooked or missing tile, a carved check that differs on another build) | restore: content check, acquisition failure, rebuilt check | the affected agents replan; the restore is reported COHERENT through `ReportCoherentRestore`; the capture itself stays EXACT | `ai.save.coherent_fallbacks`, `ai.save.carve_mismatch` |
| memory pressure | governor | sheds no simulation state; the development trace ring is released at CRITICAL | `ai.resident_bytes` |
| device loss | — | none: AI touches no device | — |
| cook: bake limit, schema error, corrupt intermediate | cook step | `COOK_ERROR_STEP_FAILED` or `IMPORT_ERROR_SOURCE_INVALID` with the AI code named | cook report |

AI's row of `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` is "none / trace ring (development builds only)": simulation bytes are unchanged at WARNING and CRITICAL (row AI17).

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/AI/Tests.md` → "Test targets" and → "Gate steps for AI diffs".

### Test catalogue index

| Group | Heading |
|---|---|
| types, tables, formats | `Zenith2/Docs/Design/AI/Tests.md` → "Type and table tests", "Runtime navmesh tests" |
| bake and cook | `Zenith2/Docs/Design/AI/Tests.md` → "Bake tests" |
| obstacles, links, paths | `Zenith2/Docs/Design/AI/Tests.md` → "Obstacle and link tests", "Path tests" |
| agents, avoidance, LOD | `Zenith2/Docs/Design/AI/Tests.md` → "Agent and avoidance tests" |
| volumes, perception, blackboards | `Zenith2/Docs/Design/AI/Tests.md` → "Volume tests", "Perception tests", "Blackboard tests" |
| scripts and libraries | `Zenith2/Docs/Design/AI/Tests.md` → "Script and library tests" |
| save, determinism, scenarios | `Zenith2/Docs/Design/AI/Tests.md` → "Save and determinism tests" |
| failure injection and fuzzing | `Zenith2/Docs/Design/AI/Tests.md` → "Failure injection tests" |
| debug and editor | `Zenith2/Docs/Design/AI/Tests.md` → "Debug and editor tests" |
| integration and conformance | `Zenith2/Docs/Design/AI/Tests.md` → "Integration suite", "Navigation conformance suite" |

## Budget rows

### Rows owned

| Row | Name | Unit | Procedure | Platforms | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|
| Z2-B-AI-001 | Navigation mesh and pathfinding memory | MB | MP-SUBMEM | Win, S24 | W-Characters, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-002 | AI and navigation phase CPU | ms | MP-CPUZONE | Win, S24 | W-Characters, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-003 | Asynchronous path requests in flight | requests | MP-COUNTER | Win, S24 | W-Characters, W-OpenWorld | BR-DEFER | Stage 1 per-system budget ticket for AI |

- Z2-B-AI-001 is the AI memory consumer's value: tiles, layers, volumes, the agent table, the path arenas, the grids, the blackboard store and the noise buffer.
- Z2-B-AI-002 is the p95 span of phase 7 per tick, which contains GAME's two phase-7 nodes (row GA18); the AI ruling allocates it knowing GAME's "Gameplay framework CPU" sub-row (BR-REPORT) is measured inside it.
- Z2-B-AI-003 is `ai.path.maxActiveRequests`: the active set of the path service, counted by `ai.path.active`; requests beyond it are deferred (BR-DEFER, `ai.path.deferred_delta`); script requests are capped earlier by Z2-B-SCRIPT-026 at the merge (ruling Q19 (a)).
- All three are pending (Z2-D-012), allocated by the Stage 1 per-system budget ticket for AI from reference-engine and published data (AI has no harness half), and first confirmed by measurement at Stage 4 in W-Characters and W-OpenWorld (`Zenith2/Docs/Roadmap.md` → "Stage 4 Production content and worlds").

### Rows measured against

| Row | AI's share |
|---|---|
| Z2-B-SCRIPT-026 | script path requests in flight (the pathfinding value Q19 (a) adds); also the path inbox's capacity and part of the queue invariant |
| Z2-B-SCRIPT-022 | the world string table's intern capacity, which blackboard keys and NAME values consume at playback; a SET whose intern fails is refused (`ai.blackboard.intern_full_delta`) |
| Z2-B-SCRIPT-015 | the checkpoint reservation that a behaviour tree's or state machine's state field occupies (≤ 3,076 bytes per tree) |
| Z2-B-GAME-003 | AI's events per tick |
| Z2-B-ECS-004 | AI's command records per tick (locomotion output, at most 4 per moving agent) |
| Z2-B-SHIP-020 | OpenWorldSlice's defect cap, which AI defects count toward |

### Pending row needs

Requested by descriptive name through cross-doc items X276 and X317 (row AI9); each is a configuration default until allocated by the Stage 1 per-system budget ticket for AI:

| Need | Default | Unit | Breach |
|---|---|---|---|
| Navigation tile rebuilds per tick | 4 (`ai.nav.maxTileRebuildsPerTick`) | tiles per world per tick | BR-DEFER |
| Path node expansions per tick | 32,768 (64 × 512) | expansions per world per tick | BR-DEFER |
| Sight rays per tick | 512 (`ai.perception.maxRaysPerTick`) | rays per world per tick | BR-DEFER |
| Agent capacity per world | 1,024 (`ai.agent.maxAgents`) | agents | BR-REFUSE |
| Noise stimuli per tick | 256 (`ai.perception.maxNoisesPerTick`) | noises per world per tick | BR-DROP |
| Navigation loads in flight | 1,024 (`ai.nav.maxLoadsInFlight`) | load records per world from allocation to publication | BR-DEFER (cross-doc item X317, which amends X276) |

## Stage plan

### Stage table

| Stage | AI work |
|---|---|
| S1 | the D-SEL-NAV harness `Zenith_Bench_NavigationSelection` with conformance scenarios N1 … N14 and its smoke CTest; the `recast-detour` selection-candidate pin; the human ruling (here or in Stage 2); the Stage 1 per-system budget ticket for AI |
| S2 | the D-SEL-NAV ruling if not made in Stage 1 (Roadmap: ruled in Stage 1 or 2) |
| S3 | none (`ZenithAI` first lands in Stage 4, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping"); the Stage 3 WorkloadCharacters selection excludes AI |
| S4 | everything else: the module, formats, bake and cook, runtime tiles and streaming, obstacles, links, search and the path service, agents and crowds, volumes, perception, blackboards, bindings and Luau libraries, LOD, debugging, the save participant, editor tools, the integration and determinism suites, the S24 unit run, first measurement of the AI rows, OpenWorldSlice AI at scale |
| S5 | none |
| S6 | qualification of the AI rows on the S24 and the reference desktop; the five fuzz targets in the acceptance sanitizer run |
| S7 | none (AI state replication is NET's, Z2-D-073) |

### API-only slices

The first Stage 4 AI ticket lands `Zenith_AITypes.h`, `Zenith_AIErrors.h`, `Zenith_NavMesh.h`, `Zenith_NavQuery.h`, `Zenith_AIComponents.h`, `Zenith_AIEvents.h`, `Zenith_AICommands.h`, `Zenith_AIWorldResource.h` and `Zenith_Module_AI.h` with contract blocks and stub definitions (queries return `AI_ERROR_NO_NAVMESH`), plus the header-only tests of the reference codecs, and the tool-side root header `Zenith2/Tools/Authoring/Zenith_AuthoringAI.h` with every declaration of `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header" and stub definitions in `Zenith2/Tools/Authoring/Internal/AI/` (every function returning `AI_ERROR_MODULE_STATE`, `Zenith_RegisterAIDocumentTypes` registering nothing and succeeding), so ENV's navigation hook consumer (`Zenith_AIWriteNavSourceIntermediate`), STREAM's cell content, WORLD's partition cook (`Zenith_AIDeclareNavigationProducts`, `Zenith_AIMakeNavigationManifestView`, `Zenith_AIWriteCellNavigationContent`), EDIT's extension and GAME's Stage 4 suite compile against AI before its implementation lands (declare first, implement after the callee); the slice adds ZenithAI to ZenithAuthoring's `DEPENDS` (X301's rule).

### Ticket slices and ordering

1. S1: pin ticket → harness ticket (blocked by the pin; scenarios N1 … N14 as functions shared with the Stage 4 conformance target; the one search loop and both storages' tile views) → D-SEL-NAV ruling (`needs-human`), which is `blocks`-linked from the AI per-system budget ruling (`needs-human`, itself blocked by the CORE, JOBS and RPROD rulings), because the acceptance rule needs Z2-B-AI-001, -002 and -003's values (X316).
2. S4, blocked by the ruling and by the revalidation ticket for (AI, Stage 4, D-SEL-NAV): API-only slice → library target and module entry points → formats, codecs and the kind loader → bake stages (the winner's) → the `Zenith_AuthoringAI.h` implementations replacing the API-only slice's stubs (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header") → importers, cook step and documents (with ENV's writer call) → navigation asset requests (after SCRIPT's `ASYNC_SOURCE_NAVIGATION` row X315 and MOD's sink) → runtime tile table, sorted connections and streaming → origin shift (after SCENE's `LIFECYCLE_EVENT_ORIGIN_SHIFTED`, X264 (j)) → MARK obstacles → layers and CARVE → links → search, straight path and queries → path service and script requests (after SCRIPT's registry and MOD's sink) → agents, state machine, corridor and steering → crowd grid and ORCA → locomotion output (velocity, kinematic and root-motion agents) → volumes and 3D avoidance → perception and the ledger → blackboards → bindings and misuse codes (after SCRIPT's ZS2500 block reservation, X288) → Luau libraries → LOD → debug views, dump and hash → save participant (after SAVE's participant API and `ReportCoherentRestore`, PH22) → editor extension and debugger → integration, determinism and scenario suites → S24 device run → Stage 4 budget confirmation.
3. S6: qualification runs (`needs-human`), fuzz targets.

## Decisions

### Settled decisions

| Decision | What it settles for AI |
|---|---|
| Z2-D-024 | AI is a static module library selected in `game.json` |
| Z2-D-006 | no job waits: searches are sliced inside a node; carve rebuilds are sub-jobs |
| Z2-D-016, Z2-D-023 | decisions are Luau behaviours; behaviour state is declared and recovered through `OnRecover` |
| Z2-D-019 | AI results are identical across workers, layouts and scheduling, including failing ticks |
| Z2-D-053 | script path results publish at tick boundaries in completion-ID order |
| Z2-D-080 | AI runs in phase 7 and writes other phases' fields through commands |
| Z2-D-051 | blackboard and output commands roll back with their entity's transaction |
| Z2-D-038, Z2-D-004 | neutral, versioned, compressed cooked navigation files, no C++ layout on disk |
| Z2-D-048 | the two documents migrate; the save section migrates forward; cooked files re-cook |
| Z2-D-045, Z2-D-047 | `Zenith_Result` errors, contract blocks |
| Z2-D-082, Z2-D-084, Z2-D-097 | the selection candidate pin, the harness plus ruling split, the ruling appended to DecisionLog.md in its diff |
| Z2-D-079 | AI authoring is code plus debugger and navigation tools, no visual behaviour editor |
| Z2-D-077 | quests and similar systems are game-side Luau, and so are squads |
| Z2-D-012 | the AI rows are pending until the Stage 1 ruling |
| Z2-D-010 | Windows and the S24 |
| Z2-D-003 | the physics queries AI's perception uses are deterministic per backend |

### Open decisions

| Decision | Stage | AI's part |
|---|---|---|
| D-SEL-NAV | 1 or 2 | the harness, the conformance scenarios, the candidate mapping and the ruling's edits (`Zenith2/Docs/Design/AI/Navigation.md` → "Navigation selection") |
| D-LUAU-STATELESS | 1 | the `@ai/` libraries pass whichever mechanism it selects |
| D-PHASES refinement | 1 | AI's census rows feed it; AI's command-only writes of the controller and transforms follow its rules |
| D-SCRIPT-TXN refinement | 1 | script path requests and AI commands follow its merge-time capacity rules |

<!-- elaboration -->
D-SEL-NAV's alternative (1) names Recast/Detour "with DetourTileCache and DetourCrowd". This design uses neither of those two components whichever candidate wins, and builds volume navigation itself, because carving and avoidance must run inside the frame phases' entity-boundary rules and Recast/Detour has no volume structure; the D-SEL-NAV acceptance rule's criteria (tiled baking and streaming, obstacles, links, agent sizes, non-ground navigation, determinism, the AI rows) are therefore measured on each candidate together with AI's carve, avoidance and volume layers. By the orchestrator's ruling AI-R1 (finding 9) both candidates also run AI's own search loop and queries over their own navmesh storage, so the selection decides the bake stages and the storage, never the search contract. Row AI8 (cross-doc items X277, X316) asks the core-doc batch to state this scope in `Zenith2/Docs/Architecture.md` → "Named selection decisions" and the DecisionLog entry; until X277 (3) and X316 are applied the DecisionLog's wording ("with DetourTileCache and DetourCrowd") stands as the decision's text, and the research report records why DetourTileCache and DetourCrowd were not measured.

### Selection decisions AI owns

| Selection | Candidates | Criteria | Acceptance rule | Consumers blocked |
|---|---|---|---|---|
| D-SEL-NAV (open, Stage 1/2) | Recast/Detour (Recast's bake stages and a `dtNavMesh` storage); a custom navmesh baker and AI's neutral storage — both under AI's one search loop and query code (ruling AI-R1) | tiled baking and streaming, dynamic obstacles, off-mesh links, agent sizes, non-ground navigation, async query cost, determinism per build (scenarios N1 … N14, N13 and N14 on each candidate as it would ship) | the candidate passing every scenario within Z2-B-AI-001, -002 and -003 on W-Characters and W-Outdoor on the S24 — measured, until W-Characters gains AI at Stage 4, through the stand-ins N7 (W-Outdoor scale) and N14 (100 agents) and as AI's share of the phase-7 span — with the lower **maintenance estimate**; Recast/Detour wins **ties**; a custom library only with a recorded measured benefit (Z2-D-041). The research report records, per candidate, the maintenance estimate as four numbers: owned source lines the candidate adds to `ZenithAI` (bake stages and storage view; the shared loop counted in neither), third-party lines pinned plus the patch count (0 by ruling AI-R1), open upstream issues touching the used API at the pin date, and the S24 porting effort in engineer-days the harness ticket measured; the lower estimate is the one lower in at least three of the four (otherwise neither is lower). A **tie** is both candidates passing every scenario and every measured AI-row share (memory per tile, AI's phase-7 share, p95 µs per 1,000 expansions, N14's p95 latency) differing by less than its 95% confidence interval ([Z2-N-ARCH-983]) | AI's Stage 4 tickets, ENV's navigation generation, WORLD's navigation baking, STREAM's navigation streaming tests |

## Requirement index

The catalogue is `Zenith2/Docs/Requirements/AI.md` (purpose, Stages 1 and 2, coverage, exclusions), `Zenith2/Docs/Requirements/AI/Stage4Plus.md` (Z2-R-AI-007 … Z2-R-AI-096) and `Zenith2/Docs/Requirements/AI/Stage4Systems.md` (Z2-R-AI-097 onward), one numbering; each requirement and the AI headings that specify it:

| Requirement | Title | Stage | Design |
|---|---|---|---|
| Z2-R-AI-001 | Navigation selection harness | S1 | AI/Navigation.md → "Harness"; AI/Navigation.md → "Conformance scenarios"; AI/Tests.md → "Benchmarks and device runs" |
| Z2-R-AI-002 | Navigation conformance scenarios defined | S1 | AI/Navigation.md → "Conformance scenarios"; AI/Tests.md → "Navigation conformance suite"; AI/Tests.md → "Fixtures" |
| Z2-R-AI-003 | Recast/Detour candidate pinned for measurement | S1 | AI/Navigation.md → "Harness"; AI.md → "Libraries AI code depends on" |
| Z2-R-AI-004 | Navigation selection scope stated | S1 | AI/Navigation.md → "Candidate mapping"; AI/Volumes.md → "Scope of this file"; AI.md → "Open decisions" |
| Z2-R-AI-005 | AI per-system budget allocation | S1 | AI.md → "Rows owned"; AI.md → "Pending row needs" |
| Z2-R-AI-006 | Navigation selection ruling applied | S2 | AI/Navigation.md → "Ruling consequences"; AI/Navigation.md → "Candidate mapping"; AI.md → "Selection decisions AI owns" |
| Z2-R-AI-007 | ZenithAI library target | S4 | AI.md → "Target summary"; AI.md → "ZenithAI files"; AI.md → "ZENITH_SUPPORTED matrix" |
| Z2-R-AI-008 | Module entry points | S4 | AI/PublicAPI.md → "Zenith_Module_AI header"; AI.md → "Module start and world attach" |
| Z2-R-AI-009 | Module start unwinds on failure | S4 | AI.md → "Module start and world attach"; AI/Tests.md → "Failure injection tests" |
| Z2-R-AI-010 | Configuration keys validated at start | S4 | AI/PublicAPI.md → "Configuration keys" |
| Z2-R-AI-011 | Edit and preview worlds run no agents | S4 | AI/Agents.md → "Frame-graph placement"; AI.md → "Module start and world attach" |
| Z2-R-AI-012 | AI error codes | S4 | AI/PublicAPI.md → "Zenith_AIErrors header"; AI.md → "Error enumerations" |
| Z2-R-AI-013 | Public API rules | S4 | AI/PublicAPI.md → "Scope of this file"; AI.md → "Rules across the AI API" |
| Z2-R-AI-014 | API-only slice | S4 | AI.md → "API-only slices"; AI/PublicAPI.md → "Header list" |
| Z2-R-AI-015 | Authority census rows | S4 | AI/PublicAPI.md → "Authority census rows"; AI/Tests.md → "Integration suite" |
| Z2-R-AI-016 | Memory consumer and shedding | S4 | AI.md → "Failure table"; AI.md → "In-memory sizes that matter" |
| Z2-R-AI-017 | Lifecycle listener | S4 | AI.md → "Lifecycle listener"; AI/Agents.md → "Agent state machine" |
| Z2-R-AI-018 | Neutral navigation file format | S4 | AI/Formats.md → "Navigation file"; AI/Formats.md → "Section kinds"; AI/Formats.md → "Info section" |
| Z2-R-AI-019 | Tile section checks | S4 | AI/Formats.md → "Tile section" |
| Z2-R-AI-020 | Navigation parser limits | S4 | AI/Formats.md → "Parser limits" |
| Z2-R-AI-021 | Navigation kind loader | S4 | AI.md → "Asset kind"; AI/PublicAPI.md → "Zenith_NavMeshAsset header" |
| Z2-R-AI-022 | Generational polygon references | S4 | AI/Navigation.md → "Polygon references"; AI/PublicAPI.md → "Zenith_AITypes header" |
| Z2-R-AI-023 | Stable tile keys | S4 | AI/Navigation.md → "Tile grid and tile keys"; AI/Navigation.md → "Tie-breaking" |
| Z2-R-AI-024 | Navigation settings and classes | S4 | AI/Navigation.md → "Agent classes"; AI/Formats.md → "Agent class object" |
| Z2-R-AI-025 | Area codes and cost profiles | S4 | AI/Navigation.md → "Area codes and costs"; AI/Formats.md → "Area table"; AI/Formats.md → "Cost profiles" |
| Z2-R-AI-026 | Navigation settings document type | S4 | AI/Formats.md → "Navigation settings document"; AI.md → "Cook steps and derived-data keys" |
| Z2-R-AI-027 | Navigation document and products | S4 | AI/Formats.md → "Navigation document schema"; AI.md → "Cook steps and derived-data keys" |
| Z2-R-AI-028 | Document migration | S4 | AI.md → "Document types and migration" |
| Z2-R-AI-029 | Bake input gathering | S4 | AI/Navigation.md → "Bake inputs" |
| Z2-R-AI-030 | Terrain navigation source intermediate | S4 | AI/Formats.md → "Navigation source intermediate"; AI/Formats.md → "Source geometry section" |
| Z2-R-AI-031 | Rasterization and filtering stages | S4 | AI/Navigation.md → "Bake stages" |
| Z2-R-AI-032 | Area volume marking | S4 | AI/Navigation.md → "Bake stages"; AI/PublicAPI.md → "Zenith_NavBuild header" |
| Z2-R-AI-033 | Region partition | S4 | AI/Navigation.md → "Bake stages" |
| Z2-R-AI-034 | Contours and polygons | S4 | AI/Navigation.md → "Bake stages" |
| Z2-R-AI-035 | Detail heights | S4 | AI/Navigation.md → "Bake stages" |
| Z2-R-AI-036 | Tile assembly | S4 | AI/Navigation.md → "Bake stages"; AI/Formats.md → "Tile section" |
| Z2-R-AI-037 | Bake limits | S4 | AI/Navigation.md → "Bake limits" |
| Z2-R-AI-038 | Bake determinism | S4 | AI/Navigation.md → "Bake determinism" |
| Z2-R-AI-039 | Incremental navigation cook | S4 | AI/Navigation.md → "Bake determinism"; AI.md → "Cook steps and derived-data keys" |
| Z2-R-AI-040 | Empty tiles | S4 | AI/Navigation.md → "Bake stages"; AI/Formats.md → "Navigation document schema" |
| Z2-R-AI-041 | Carve layers kept | S4 | AI/Formats.md → "Layers section"; AI/Navigation.md → "Bake stages" |
| Z2-R-AI-042 | Editor regeneration | S4 | AI.md → "Editor surfaces" |
| Z2-R-AI-043 | Cell navigation content | S4 | AI/Navigation.md → "Streamed tiles"; AI/Internals.md → "Zenith_AuthoringAI header"; AI/Formats.md → "Navigation document schema" |
| Z2-R-AI-044 | Tile add procedure | S4 | AI/Navigation.md → "Tile add procedure"; AI/Navigation.md → "Navigation asset requests" |
| Z2-R-AI-045 | Tile capacity | S4 | AI/Navigation.md → "Tile add procedure"; AI.md → "Failure table" |
| Z2-R-AI-046 | Tile connections in both directions | S4 | AI/Navigation.md → "Tile connection procedure"; AI/Navigation.md → "Tile remove procedure" |
| Z2-R-AI-047 | Streamed tile activation | S4 | AI/Navigation.md → "Streamed tiles"; AI/Navigation.md → "Navigation asset requests"; AI/Navigation.md → "Supplier records" |
| Z2-R-AI-048 | Navigation streaming stress | S4 | AI/Navigation.md → "Tile remove procedure" |
| Z2-R-AI-049 | Origin shift preserves navigation | S4 | AI/Navigation.md → "Origin shift" |
| Z2-R-AI-050 | Non-streamed navmesh reference | S4 | AI/Navigation.md → "Streamed tiles"; AI/Formats.md → "Manifest section" |
| Z2-R-AI-051 | Obstacle component | S4 | AI/PublicAPI.md → "Zenith_AIComponents header"; AI/Navigation.md → "Obstacle modes" |
| Z2-R-AI-052 | Mark obstacles | S4 | AI/Navigation.md → "Mark procedure" |
| Z2-R-AI-053 | Carve obstacles | S4 | AI/Navigation.md → "Carve procedure" |
| Z2-R-AI-054 | Carve fallback to mark | S4 | AI/Navigation.md → "Carve procedure"; AI/PublicAPI.md → "Zenith_AIComponents header" |
| Z2-R-AI-055 | Carve budget and order | S4 | AI/Navigation.md → "Carve procedure" |
| Z2-R-AI-056 | Carve determinism | S4 | AI/Navigation.md → "Carve procedure"; AI/Navigation.md → "Bake determinism" |
| Z2-R-AI-057 | Carving invalidates paths | S4 | AI/Navigation.md → "Polygon references"; AI/Agents.md → "Path corridor" |
| Z2-R-AI-058 | Off-mesh link component and registration | S4 | AI/Navigation.md → "Link registration"; AI/PublicAPI.md → "Zenith_AIComponents header" |
| Z2-R-AI-059 | Cross-cell links | S4 | AI/Navigation.md → "Link registration" |
| Z2-R-AI-060 | Automatic link traversal | S4 | AI/Navigation.md → "Link traversal state machine"; AI/Navigation.md → "Link kinds" |
| Z2-R-AI-061 | Scripted link traversal | S4 | AI/Navigation.md → "Link traversal state machine"; AI/Scripting.md → "Binding functions" |
| Z2-R-AI-062 | Optimal polygon search | S4 | AI/Navigation.md → "Search graph"; AI/Navigation.md → "Cost and heuristic"; AI/Navigation.md → "Search state and slicing" |
| Z2-R-AI-063 | Deterministic tie-breaking | S4 | AI/Navigation.md → "Tie-breaking" |
| Z2-R-AI-064 | Sliced search equivalence | S4 | AI/Navigation.md → "Search state and slicing"; AI/PublicAPI.md → "Zenith_NavQuery header" |
| Z2-R-AI-065 | Partial and failed paths | S4 | AI/Navigation.md → "Partial results"; AI/Navigation.md → "Search state and slicing" |
| Z2-R-AI-066 | Stale searches restart | S4 | AI/Navigation.md → "Per-tick processing"; AI/Navigation.md → "Search state and slicing" |
| Z2-R-AI-067 | Straight path extraction | S4 | AI/Navigation.md → "Straight path extraction" |
| Z2-R-AI-068 | Path service in the frame graph | S4 | AI/Navigation.md → "Per-tick processing"; AI/Agents.md → "Frame-graph placement" |
| Z2-R-AI-069 | Active request cap and deferral | S4 | AI/Navigation.md → "Per-tick processing"; AI/Navigation.md → "Request queue" |
| Z2-R-AI-070 | Request tick cap | S4 | AI/Navigation.md → "Per-tick processing" |
| Z2-R-AI-071 | Path service determinism | S4 | AI/Navigation.md → "Per-tick processing"; AI/Agents.md → "Determinism rules" |
| Z2-R-AI-072 | Agent plans superseded in place | S4 | AI/Navigation.md → "Request queue" |
| Z2-R-AI-073 | Script path requests | S4 | AI/Scripting.md → "Path request payload and result"; AI/Formats.md → "Script request payload" |
| Z2-R-AI-074 | Script path results published at a tick boundary | S4 | AI/Navigation.md → "Publication of script results"; AI/Formats.md → "Script result payloads" |
| Z2-R-AI-075 | Path result decoder | S4 | AI/Scripting.md → "Path request payload and result"; AI/Formats.md → "Script result payloads" |
| Z2-R-AI-076 | Path request cancellation | S4 | AI/Navigation.md → "Cancellation"; AI/Navigation.md → "Path request state machine" |
| Z2-R-AI-077 | Navmesh spatial queries | S4 | AI/Navigation.md → "Nearest point"; AI/Navigation.md → "Navmesh raycast"; AI/Navigation.md → "Random reachable point"; AI/Navigation.md → "Surface movement" |
| Z2-R-AI-078 | Agent component | S4 | AI/PublicAPI.md → "Agent properties"; AI/PublicAPI.md → "Zenith_AIComponents header" |
| Z2-R-AI-079 | Agent runtime table | S4 | AI/Agents.md → "Agent runtime table" |
| Z2-R-AI-080 | Move commands | S4 | AI/Agents.md → "Move commands"; AI/Formats.md → "Command payloads" |
| Z2-R-AI-081 | Agent state machine | S4 | AI/Agents.md → "Agent state machine" |
| Z2-R-AI-082 | One completion per move | S4 | AI/Agents.md → "Agent state machine"; AI/PublicAPI.md → "Zenith_AIEvents header" |
| Z2-R-AI-083 | Stuck detection | S4 | AI/Agents.md → "Stuck detection" |
| Z2-R-AI-084 | Path corridor maintenance | S4 | AI/Agents.md → "Path corridor" |
| Z2-R-AI-085 | Steering and arrival | S4 | AI/Agents.md → "Steering" |
| Z2-R-AI-086 | Reciprocal avoidance | S4 | AI/Agents.md → "ORCA constraints"; AI/Agents.md → "Linear program"; AI/Agents.md → "Neighbour selection" |
| Z2-R-AI-087 | Avoidance responsibility | S4 | AI/Agents.md → "ORCA constraints" |
| Z2-R-AI-088 | Avoidance order independence | S4 | AI/Agents.md → "Determinism rules"; AI/Agents.md → "Crowd grid" |
| Z2-R-AI-089 | Crowd grid | S4 | AI/Agents.md → "Crowd grid"; AI/Agents.md → "Frame-graph placement" |
| Z2-R-AI-090 | Crowd at the W-Characters scale | S4 | AI/Tests.md → "Integration suite"; AI/Agents.md → "Avoidance" |
| Z2-R-AI-091 | Locomotion output through the character controller | S4 | AI/Agents.md → "Locomotion output" |
| Z2-R-AI-092 | Kinematic locomotion output | S4 | AI/Agents.md → "Locomotion output" |
| Z2-R-AI-093 | Turn rate limit | S4 | AI/Agents.md → "Locomotion output" |
| Z2-R-AI-094 | Root agents only | S4 | AI/Agents.md → "Locomotion output" |
| Z2-R-AI-095 | Rejected locomotion output resent | S4 | AI/Agents.md → "Locomotion output" |
| Z2-R-AI-096 | Agent sizes as classes | S4 | AI/Navigation.md → "Agent classes"; AI/Navigation.md → "Conformance scenarios" |
| Z2-R-AI-097 | Volume model | S4 | AI/Volumes.md → "Volume model"; AI/Formats.md → "Volume section" |
| Z2-R-AI-098 | Volume bake | S4 | AI/Volumes.md → "Volume bake" |
| Z2-R-AI-099 | Volume path search | S4 | AI/Volumes.md → "Volume search" |
| Z2-R-AI-100 | Volume smoothing and line of sight | S4 | AI/Volumes.md → "Volume path smoothing"; AI/Volumes.md → "Volume line of sight" |
| Z2-R-AI-101 | Flying navigation | S4 | AI/Volumes.md → "Volume search"; AI/Volumes.md → "Swimming" |
| Z2-R-AI-102 | Three-dimensional avoidance | S4 | AI/Agents.md → "Three-dimensional avoidance" |
| Z2-R-AI-103 | Surface swimming | S4 | AI/Volumes.md → "Swimming"; AI/Navigation.md → "Area codes and costs" |
| Z2-R-AI-104 | Underwater navigation | S4 | AI/Volumes.md → "Swimming"; AI/Volumes.md → "Volume search" |
| Z2-R-AI-105 | Medium transitions | S4 | AI/Volumes.md → "Medium transitions"; AI/Agents.md → "Agent state machine" |
| Z2-R-AI-106 | Perception components | S4 | AI/PublicAPI.md → "Perception properties"; AI/PublicAPI.md → "Zenith_AIComponents header" |
| Z2-R-AI-107 | Teams | S4 | AI/Agents.md → "Senses"; AI/PublicAPI.md → "Configuration keys" |
| Z2-R-AI-108 | Sight cones | S4 | AI/Agents.md → "Sight algorithm" |
| Z2-R-AI-109 | Sight line of sight through physics | S4 | AI/Agents.md → "Sight algorithm" |
| Z2-R-AI-110 | Sight ray budget | S4 | AI/Agents.md → "Sight algorithm" |
| Z2-R-AI-111 | Awareness gain and decay | S4 | AI/Agents.md → "Awareness model"; AI/Agents.md → "Sight algorithm" |
| Z2-R-AI-112 | Hearing | S4 | AI/Agents.md → "Hearing algorithm" |
| Z2-R-AI-113 | Noise capacity | S4 | AI/Agents.md → "Hearing algorithm" |
| Z2-R-AI-114 | Damage, touch and forced awareness | S4 | AI/Agents.md → "Damage and touch" |
| Z2-R-AI-115 | Perception memory | S4 | AI/Agents.md → "Perception memory" |
| Z2-R-AI-116 | Perception events | S4 | AI/Agents.md → "Perception events"; AI/PublicAPI.md → "Zenith_AIEvents header" |
| Z2-R-AI-117 | Perception determinism | S4 | AI/Agents.md → "Determinism rules"; AI/Agents.md → "Stimulus grid" |
| Z2-R-AI-118 | Blackboard store | S4 | AI/Agents.md → "Blackboard records"; AI/Agents.md → "Blackboard commands"; AI/PublicAPI.md → "Zenith_AIWorldResource header" |
| Z2-R-AI-119 | Blackboard visibility | S4 | AI/Agents.md → "Blackboard reads" |
| Z2-R-AI-120 | Blackboard expiry | S4 | AI/Agents.md → "Blackboard reads" |
| Z2-R-AI-121 | Shared blackboards | S4 | AI/Agents.md → "Blackboard records"; AI/Agents.md → "Blackboard commands" |
| Z2-R-AI-122 | Blackboard writes are transactional | S4 | AI/Agents.md → "Blackboard commands"; AI/Scripting.md → "Binding functions" |
| Z2-R-AI-123 | Blackboard payloads carry text | S4 | AI/Formats.md → "Command payloads"; AI/Agents.md → "Blackboard reads" |
| Z2-R-AI-124 | AI script bindings | S4 | AI/Scripting.md → "Binding registration"; AI/Scripting.md → "Binding functions" |
| Z2-R-AI-125 | AI misuse codes | S4 | AI/Scripting.md → "Misuse codes" |
| Z2-R-AI-126 | Replayable navigation randomness | S4 | AI/Scripting.md → "Binding functions"; AI/Navigation.md → "Random reachable point" |
| Z2-R-AI-127 | AI Luau libraries | S4 | AI/Scripting.md → "Luau library layout" |
| Z2-R-AI-128 | Behaviour tree semantics | S4 | AI/Scripting.md → "Behaviour tree library" |
| Z2-R-AI-129 | Behaviour tree state is behaviour state | S4 | AI/Scripting.md → "Behaviour tree library" |
| Z2-R-AI-130 | Behaviour tree build limits | S4 | AI/Scripting.md → "Behaviour tree library" |
| Z2-R-AI-131 | Utility selection | S4 | AI/Scripting.md → "Utility library" |
| Z2-R-AI-132 | Hierarchical state authoring | S4 | AI/Scripting.md → "State machine library" |
| Z2-R-AI-133 | State machine state is behaviour state | S4 | AI/Scripting.md → "State machine library" |
| Z2-R-AI-134 | Agent and blackboard helpers | S4 | AI/Scripting.md → "Agent library"; AI/Scripting.md → "Blackboard library" |
| Z2-R-AI-135 | End-to-end Luau brain | S4 | AI/Tests.md → "Integration suite"; AI/Scripting.md → "Behaviour tree library" |
| Z2-R-AI-136 | Significance tiers | S4 | AI/Agents.md → "Simulation LOD" |
| Z2-R-AI-137 | Spread update ticks | S4 | AI/Agents.md → "Simulation LOD" |
| Z2-R-AI-138 | LOD reduces work by tier | S4 | AI/Agents.md → "Simulation LOD"; AI/Agents.md → "Neighbour selection" |
| Z2-R-AI-139 | LOD is simulation state | S4 | AI/Agents.md → "Simulation LOD"; AI/Agents.md → "Determinism rules" |
| Z2-R-AI-140 | AI debug views | S4 | AI.md → "Debug views"; AI/PublicAPI.md → "Zenith_AIDebug header" |
| Z2-R-AI-141 | AI dump command | S4 | AI.md → "AI dump command" |
| Z2-R-AI-142 | Decision trace | S4 | AI/Scripting.md → "Debug trace helpers" |
| Z2-R-AI-143 | Editor AI debugger | S4 | AI.md → "Editor surfaces" |
| Z2-R-AI-144 | Editor navigation tools | S4 | AI.md → "Editor surfaces" |
| Z2-R-AI-145 | AI counters, zones and profiler rows | S4 | AI.md → "Counters"; AI.md → "Profiler zones"; AI.md → "On-screen profiler rows" |
| Z2-R-AI-146 | Canonical AI state hash | S4 | AI.md → "Canonical AI state hash" |
| Z2-R-AI-147 | AI determinism replay matrix | S4 | AI/Agents.md → "Determinism rules"; AI/Tests.md → "Save and determinism tests" |
| Z2-R-AI-148 | AI determinism under script faults | S4 | AI/Agents.md → "Determinism rules" |
| Z2-R-AI-149 | Live AI runs agree | S4 | AI/Navigation.md → "Navigation asset requests"; AI/Navigation.md → "Completion gate"; AI/Agents.md → "Determinism rules" |
| Z2-R-AI-150 | Reproducible AI scenarios | S4 | AI.md → "AI dump command"; AI/Tests.md → "Save and determinism tests" |
| Z2-R-AI-151 | AI save participant | S4 | AI/Agents.md → "Save and restore algorithm"; AI/Internals.md → "Save participant class" |
| Z2-R-AI-152 | AI save section | S4 | AI/Formats.md → "Save section layout" |
| Z2-R-AI-153 | Exact AI restore | S4 | AI/Agents.md → "Fidelity"; AI/Agents.md → "Restore" |
| Z2-R-AI-154 | Restore-time coherent fallback | S4 | AI/Agents.md → "Fidelity"; AI/Agents.md → "Restore" |
| Z2-R-AI-155 | World transition discards AI requests | S4 | AI/Navigation.md → "Cancellation" |
| Z2-R-AI-156 | Corrupt and missing navigation content | S4 | AI.md → "Failure table" |
| Z2-R-AI-157 | Allocation failure during navigation work | S4 | AI.md → "Failure table" |
| Z2-R-AI-158 | Android build and device run | S4 | AI/Tests.md → "Gate steps for AI diffs"; AI.md → "ZENITH_SUPPORTED matrix" |
| Z2-R-AI-159 | AI rows confirmed at Stage 4 | S4 | AI.md → "Rows owned"; AI/Tests.md → "Benchmarks and device runs" |
| Z2-R-AI-160 | OpenWorldSlice AI at scale | S4 | AI.md → "Stage table"; AI.md → "Rows measured against" |
| Z2-R-AI-161 | Cooked reader fuzzing | S6 | AI/Tests.md → "Failure injection tests" |
| Z2-R-AI-162 | AI budget qualification | S6 | AI.md → "Rows owned"; AI/Tests.md → "Benchmarks and device runs" |
| Z2-R-AI-163 | Navigation asset requests | S4 | AI/Navigation.md → "Navigation asset requests"; AI/Navigation.md → "Completion gate"; AI/Internals.md → "Navigation load records" |
| Z2-R-AI-164 | Restore waits listed | S4 | AI/Agents.md → "Restore"; AI/Internals.md → "Save participant class" |
| Z2-R-AI-165 | Search state saved whole | S4 | AI/Agents.md → "Restore"; AI/Formats.md → "Save section layout"; AI/PublicAPI.md → "Zenith_NavQuery header" |
| Z2-R-AI-166 | Carve state restored exactly | S4 | AI/Navigation.md → "Carve procedure"; AI/Navigation.md → "Mark procedure"; AI/Agents.md → "Restore" |
| Z2-R-AI-167 | Every world-local AI position shifted | S4 | AI/Navigation.md → "Origin shift"; AI/PublicAPI.md → "Agent properties"; AI/PublicAPI.md → "Perception properties" |
| Z2-R-AI-168 | Canonical neighbour order | S4 | AI/Navigation.md → "Tile connection procedure"; AI/Navigation.md → "Surface movement" |
| Z2-R-AI-169 | Events re-derived after a rejection | S4 | AI/Agents.md → "Frame-graph placement"; AI/Agents.md → "Perception events"; AI/Internals.md → "Perception ledger" |
| Z2-R-AI-170 | Root-motion agents | S4 | AI/Agents.md → "Locomotion output"; AI/Agents.md → "Stuck detection"; AI/Scripting.md → "Authoring conventions" |
| Z2-R-AI-171 | Tool-side authoring surface | S4 | AI/Internals.md → "Zenith_AuthoringAI header"; AI.md → "Tool-side files" |
| Z2-R-AI-172 | Detail mesh limits representable | S4 | AI/Navigation.md → "Bake limits"; AI/Formats.md → "Tile section" |
| Z2-R-AI-173 | Non-agent characters avoided | S4 | AI/Agents.md → "Crowd grid"; AI/Agents.md → "ORCA constraints" |
| Z2-R-AI-174 | Path inbox and queue capacity | S4 | AI/Navigation.md → "Request queue"; AI/Internals.md → "Path request consumer" |
| Z2-R-AI-175 | Search expansion step | S4 | AI/Navigation.md → "Search state and slicing"; AI/Navigation.md → "Search graph"; AI/Navigation.md → "Tie-breaking" |
| Z2-R-AI-176 | Search limits named | S4 | AI/Navigation.md → "Search state and slicing"; AI/Navigation.md → "Partial results"; AI/Navigation.md → "Conformance scenarios" |
| Z2-R-AI-177 | Long moves continue | S4 | AI/Agents.md → "Path corridor"; AI/Agents.md → "Agent state machine"; AI/Navigation.md → "Partial results" |
| Z2-R-AI-178 | Connection overflow is a set function | S4 | AI/Navigation.md → "Tile connection procedure"; AI/Navigation.md → "Tile remove procedure" |
| Z2-R-AI-179 | Rebase bookkeeping saved | S4 | AI/Navigation.md → "Origin shift"; AI/Agents.md → "Restore"; AI/Formats.md → "Save section layout" |
| Z2-R-AI-180 | Link nodes in saved corridors | S4 | AI/PublicAPI.md → "Zenith_AITypes header"; AI/Formats.md → "Save section layout"; AI/Agents.md → "Restore" |
| Z2-R-AI-181 | Volume search stamps | S4 | AI/Volumes.md → "Volume search"; AI/PublicAPI.md → "Zenith_NavVolume header"; AI/Formats.md → "Save section layout" |
| Z2-R-AI-182 | Supplier records by value | S4 | AI/Navigation.md → "Supplier records"; AI/Navigation.md → "Tile add procedure"; AI/Formats.md → "Save section layout" |
| Z2-R-AI-183 | Deterministic transcendentals | S4 | AI.md → "Assumed interfaces"; AI/Agents.md → "Locomotion output"; AI/Agents.md → "Sight algorithm" |
| Z2-R-AI-184 | Navigation product declaration shared with WORLD | S4 | AI/Internals.md → "Zenith_AuthoringAI header"; AI/Formats.md → "Navigation document schema"; AI/Navigation.md → "Bake inputs" |
| Z2-R-AI-185 | Placed entities by world pose | S4 | AI/Navigation.md → "Obstacle modes"; AI/Agents.md → "Frame-graph placement"; AI/Agents.md → "Path corridor" |
| Z2-R-AI-186 | Dead link nodes | S4 | AI/Navigation.md → "Search graph"; AI/Navigation.md → "Search state and slicing"; AI/Formats.md → "Save section layout"; AI/Agents.md → "Restore" |

## Reconciliation items

The cross-area rows AI appended (prefix AI) to `findings/A3_cross_area_notes.md`:

| Row | To | Item |
|---|---|---|
| AI1 | MOD | conformed (post-round): the sink is cited from `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink" (handler class, `MODULE_SOURCE_SITE_MERGE_CHAIN`, `PUBLICATION_SINK_ERROR_FULL`); `FindCommandKind` and `GetPublicationSink` from its "World attach context lookups" (pending X344); `GetWorldPurpose` declared (X243 (b)); `GetDebugDraw` declared as a reference present in every build (X245 (c), core patch f); still assumed: `GetScriptAsyncCapacity` |
| AI2 | ASSET, PACK | SOURCE_GEOMETRY files kept out of packaged mounts; kind 15's runtime type `Zenith_NavMeshAsset` confirmed (AS26) |
| AI3 | ENV | the `ai.navSource` writer call from `Environment.Navigation`; the water-body listing for volume bakes |
| AI4 | SCRIPT | the misuse block ZS2500–ZS2599 (X288, MISUSE-BLOCKS-2); `ctx:RequestPath` realised as `AI.RequestPath`; pathfinding in Z2-B-SCRIPT-026; (d) answered by `NextRandomDraw`; (e) a decoder's STRUCT field pointing into its payload |
| AI5 | SCENE | the cook-side scene document walk |
| AI6 | PHYS | the cook-side neutral geometry decoder; ray casts from phase-7 nodes; the controller input written by command |
| AI7 | PROC | `Zenith2/Engine/ZenithAI/**` joins the `Zenith2Memory` category's paths |
| AI8 | ARCH, CONTRACT | ZenithAI's `DEPENDS` (ZenithSave, ZenithPlatform); AI's parser-limit rows; the two document types in [Z2-N-CONTRACT-552]; the D-SEL-NAV scope wording |
| AI9 | BUDGET | the five pending row needs; the definitions of Z2-B-AI-002 and -003 |
| AI10 | STREAM | ST9 confirmed by AI's names; the join timing is AI21's (revised in revision 2) |
| AI11 | WORLD | answers WD11: AI adopts `Zenith_WorldCellBounds`, `Zenith_WorldCellContentWriter` and `Zenith_WorldProduct` and supplies `Zenith_AIMakeNavigationManifestView`, which reads product keys only; the baking workflow drives navigation documents |
| AI12 | EDIT | the Navigation extension; `ZenithAuthoring/AI`; ED6 and ED15 answered; the component handle rows |
| AI13 | GAME | AI's census rows; GA18 answered; AI's half of the world-transition scenario |
| AI14 | SAVE | the participant `Zenith.AI.Navigation` (SV17): every capture EXACT; re-issue of both source kinds; `ListRestoreWaits` lists the restore-queue loads; `ReportCoherentRestore` (PH22) for content changed at restore |
| AI15 | DEVT | `ai.dump`, the AI overlay tab, the debug-draw epilogue stage |
| AI16 | SHIP | the `recast-detour` selection-candidate entry, licence Zlib, with no patch entry (ruling AI-R1 #9) |
| AI17 | CORE | AI's shedding row (CB8) |
| AI18 | SCRIPT | answered and conformed: `ASYNC_SOURCE_NAVIGATION` = 13 (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values"; core text X347) |
| AI19 | SCENE, STREAM | `LIFECYCLE_EVENT_ORIGIN_SHIFTED` after the shift (PH20, X264 (j)); (b) withdrawn in revision 2: AI shifts the PerceivedTarget container elements itself and SCENE changes nothing |
| AI20 | PHYS, ANIM | root-motion agents: AI records no controller input in root-motion modes and writes Agent 43, 44, 48 for the animation; the crowd grid reads controllers without agents (1, 2, 30, any-row) |
| AI21 | STREAM | "Navigation coordination": tiles requested when their cell activates, joined after their publication (two ticks at the earliest), agents SUSPENDED in between |
| AI22 | CORE, SCRIPT | AI uses CORE's `Zenith_Sin`, `Zenith_Cos`, `Zenith_Atan2`, `Zenith_Exp` (X314), and needs `Zenith_Exp(0) == 1` exactly (post-round); AI's Luau libraries call no `math.*` transcendental |
| AI23 | WORLD | answers WD23: `Zenith_AIDeclareNavigationProducts` with `Zenith_AINavigationProductSink`, scene product entries with a binary64 `offset`, volumes with `index`, cooked scene products read by the bake; the declaration also takes a `Zenith_AIBakeInputSource` WORLD implements over its source access (AI design patch 2026-10-17, "Cook steps and derived-data keys") |
| AI24 | ECS | an equal-count `SetContainer` from a lifecycle listener at a sync point rewrites in place and allocates nothing (the PerceivedTarget shift, `Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift") |
