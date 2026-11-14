# Zenith2 capability inventory

The capability inventory is the input to requirement coverage: every Zenith2 capability, one ID per item, each classified and staged. Every item is covered by one or more requirements in its area catalogue, `Zenith2/Docs/Requirements/<CODE>.md`, and every required or deferred requirement is covered by leaf implementation tickets and an integration or acceptance ticket.

## Purpose and versioning

### What the inventory is
- This is **Inventory v1**. Its scope is the approved v1 capability scope, verbatim: a table of scope rows, each naming an area, its code and a required-scope statement. Every scope row's statement is split into items, one capability per item, with the approved wording kept and only lightly normalised (a shared qualifier repeated onto each item it applies to, articles adjusted). Nothing is dropped, and nothing is added beyond the expansion rule below.
- Splitting rule: a cell is split at every semicolon; a clause that lists distinct capabilities (separated by commas, slashes or "and") is split again, so each item is one capability that a requirement can cover and a test can prove. A qualifier that governs a whole list ("with sanitizer integration", "as explicitly classified extensions", "as Luau samples only", "and never corrupts it") travels with every item it governs. A slash-pair naming one mechanism (undo/redo, transport/session, prediction/reconciliation) stays one item, and a parenthetical clause stays with its item.
<!-- elaboration -->
- Expansion rule: a scope statement that points at a specification section — GPU lifetime and sync, the vertex data format, two-phase HiZ occlusion culling, render views, the full frame for every path, the universal GPU performance rules — is expanded into that section's top-level deliverables, each an item, and Notes names the governing doc section (for example `Rendering.md → "GPU lifetime and synchronisation"`). A qualifier drawn from such a section, rather than from the scope statement itself, goes in Notes, never in Item.
- The "extended" scope rows fold into the table of the code they carry, continuing that code's numbering: PHYS (extended) into PHYS, GAME (extended) into GAME, AI (extended) into AI, AUDIO (extended) into AUDIO, CINE (extended) into CINE, UI (extended) into UI, SAVE (extended) into SAVE; the Memory governor row folds into CORE. An item that came from an extended row says so in its Notes.
- IDs are `Z2-I-<CODE>-NN`: the area code from "Area codes", then a two-digit sequence within the code, in scope-row order. An ID is never reused or renumbered; see "Change rule".
- Inventory v1 holds **706 items** in **41 areas**: 681 required, 12 deferred, 9 excluded, 4 game-side.

### Columns
| Column | Meaning |
|---|---|
| ID | `Z2-I-<CODE>-NN`, stable. Requirements cite it in their "inventory items covered" field |
| Item | The capability, in the approved scope wording |
| Classification | `required`, `deferred`, `excluded` or `game-side` (below) |
| Stage | The stage whose sprint delivers the item, `S0` to `S7`; `S7` for every deferred item; `—` for excluded items |
| Notes | Governing decisions (`Z2-D-NNN`), open decisions by name with their stage, the source row where it is an extended row, and "stage inferred" where the stage is inferred |

### Classifications
| Classification | Meaning | Tickets |
|---|---|---|
| required | Delivered by the stage named in Stage | Detailed leaf tickets in that stage's sprint (or pulled forward), with the stage as fix version |
| deferred | Delivered in Stage 7, with a full requirement tree now | Detailed leaf tickets in the `Stage 7 — Later expansion` sprint (slug `z2-stage-7-later-expansion`), fix version `Z2 Stage 7` |
| excluded | Not delivered by the engine; recorded with a rationale and a decision ID in Notes | None |
| game-side | Delivered as Luau gameplay modules inside OpenWorldSlice, never in the engine; Notes carries Z2-D-077 | Covered by OpenWorldSlice Luau-sample requirements in `Zenith2/Docs/Requirements/GAME.md`: leaf tickets under the GAME epic, fix version `Z2 Stage 4`, touching only `Zenith2/Games/OpenWorldSlice/Scripts/`, plus an integration ticket in the OpenWorldSlice acceptance definition; never engine tickets (Z2-D-067) |

<!-- elaboration -->
The game-side coverage rule above details the classification; the coverage gate step fails an engine ticket that covers a game-side item (Z2-D-067).

### How stages were assigned
- An item's stage is taken from the Roadmap's stage list (`Zenith2/Docs/Roadmap.md → "Stage overview"`), which names capability families per stage: Stage 1 decisions, specifications and measurements; Stage 2 foundation; Stage 3 the playable authoring slice; Stage 4 production content and worlds; Stage 5 rendering and simulation completion; Stage 6 shipping qualification; Stage 7 deferred expansion.
- Where an item is named by the stage list, or by a settled decision, its stage is that one. A decision-shaped item (a D-name, a classification, a scope boundary) is staged at its ruling's stage; the capability it governs is staged where it is delivered.
<!-- elaboration -->
- Where the stage list names only the item's family, the item takes the stage of the first family member it depends on or serves, and its Notes say "stage inferred". Those assignments are the first place a reviewer or the area sign-off looks, and the area catalogue may move one only through the change rule.
- A ticket's sprint may be earlier than its item's stage: the pull-forward rule schedules independent work early while keeping the fix version (`Zenith2/Docs/Roadmap.md → "Pull-forward rule"`).

## Area codes

Every item belongs to exactly one area code, and every area code has one epic (`Z2 <CODE> — <Area name>`), one design doc (`Zenith2/Docs/Design/<CODE>.md`) and one requirement catalogue (`Zenith2/Docs/Requirements/<CODE>.md`). Anything spanning areas is owned by one area and names the others in its `## Dependencies`.

| Code | Area name | Description | Scope rows folded in | Items |
|---|---|---|---|---|
| CORE | Core & mathematics | Ownership, the general heap, arenas and pools, containers, strings, numerics, clocks, diagnostics, configuration primitives, and the cross-system memory governor. | Core & mathematics (CORE); Memory governor (CORE) | 30 |
| JOBS | Jobs & frame scheduling | The work-stealing scheduler that executes the compiled CPU frame-graph schedule, its sub-jobs, continuations, time stepping and shutdown. | Jobs & frame scheduling (JOBS) | 15 |
| PLAT | Platform & input | Windowing, input, the virtual filesystem and async I/O, configuration layering, the Android lifecycle, thermal adaptation and account changes, over SDL3. | Platform & input (PLAT) | 30 |
| ECS | Reflection & ECS | ZenithReflect and the neutral Zenith_ECS API with its three backends, runtime type registration and data-defined components. | Reflection & ECS (ECS) | 17 |
| SCENE | Scenes & prefabs | Scene documents, authoring identities, prefabs with overrides and variants, loading, transform propagation and transitions. | Scenes & prefabs (SCENE) | 10 |
| STREAM | World streaming | Runtime world streaming: partitioning, cell loading, cancellation, large coordinates, cross-cell references and coordinated subsystem streaming. | World streaming (STREAM) | 11 |
| ASSET | Asset pipeline | The asset database, import and cook, the derived-data cache, validation, provenance, async loading, residency and decode back-pressure. | Asset pipeline (ASSET) | 17 |
| PACK | Packaging & content updates | Versioned cooked formats, archives, chunking, install layouts, Android App Bundles and Play Asset Delivery, patches, DLC and interrupted-update recovery. | Packaging & content updates (PACK) | 16 |
| RHI | RHI & render graph | The neutral RHI with Vulkan and D3D12 backends: GPU lifetime and synchronisation, allocation, presentation, capabilities, validation and recovery. | RHI & render graph (RHI) | 22 |
| GEOM | Geometry & materials | The vertex data format and codec, LOD, instancing, meshlets, skinning, morphs, material permutations, transparency, decals, terrain geometry and texture streaming. | Geometry & materials (GEOM) | 22 |
| LIGHT | Lighting & image quality | Shadows, probes, global illumination, reflections, volumetrics, temporal history, antialiasing and upscaling, exposure, colour and HDR output, and camera effects. | Lighting & image quality (LIGHT) | 16 |
| ENV | Environment | Terrain authoring, foliage, water, sky, weather and time of day, procedural placement, and generated collision and navigation. | Environment (ENV) | 13 |
| ANIM | Animation | Skeletal animation: import, compression, graphs, layers, events, root motion, retargeting, IK, facial animation and ragdoll transitions, with visual authoring. | Animation (ANIM) | 16 |
| PHYS | Physics | The neutral Zenith_Physics API with Jolt and PhysX backends, conformance and determinism, collision, queries, controllers, constraints, vehicles, ragdolls, and classified specialist simulations. | Physics (PHYS); Physics & simulation (extended) (PHYS) | 29 |
| AI | AI & navigation | Navmesh baking and streaming, pathfinding, avoidance and crowds, perception, blackboards, behaviour authoring, debugging and reproducible scenarios. | AI & navigation (AI); AI (extended) (AI) | 17 |
| GAME | Gameplay framework | Game and world lifecycle, cameras, reusable gameplay primitives, script integration, the frame-phase contract and lifecycle commands; quest, dialogue, inventory and interaction live only in OpenWorldSlice. | Gameplay framework (GAME); Gameplay (extended) (GAME) | 32 |
| SAVE | Save games & replay | Versioned saves, checkpoints, persistent state, atomic writes and recovery, migration, input recording, replays and cross-system failure scenarios. | Save games & replay (SAVE); Persistence (extended) (SAVE) | 18 |
| AUDIO | Audio | Streaming and decode, voices, mixing, spatialization, acoustics, music, dialogue, device and focus handling, real-time constraints, authoring and profiling. | Audio (AUDIO); Audio (extended) (AUDIO) | 25 |
| UI | UI, localization, accessibility | Retained UI and layout, focus navigation, text shaping and bidi, localization, scaling, captions, remapping, accessibility features and end-to-end accessible flows, with visual authoring. | UI, localization, accessibility (UI); UI & accessibility (extended) (UI) | 30 |
| VFX | VFX | CPU and GPU particles, ribbons and trails, collision, culling, sorting, LOD, event integration and budgets, authored in code with previews. | VFX (VFX) | 13 |
| CINE | Cinematics & media | Timeline sequencing with camera, animation, audio and event tracks, scrubbing and blending, dialogue and subtitle sync, video playback, capture and export, interruption and state restore, with visual authoring. | Cinematics & media (CINE); Cinematics (extended) (CINE) | 16 |
| EDIT | Editor & collaboration | The editor foundation and tools: undo, selection, gizmos, asset browser, previews, prefab editing, recovery, play worlds, source control, merge-friendly files, extension APIs, migration UI and usability acceptance. | Editor & collaboration (EDIT) | 21 |
| DEVT | Developer tools & quality | The on-screen profiler, Tracy, the libgraph and complexity gates, profiling, remote Android diagnostics, crash dumps, breadcrumbs, logging, validation, fuzzing, static analysis and dashboards. | Developer tools & quality (DEVT) | 26 |
| SHIP | Shipping & maintenance | Reproducible builds, the CI and device matrix, packaging and signing, release manifests, symbols, the dependency and licence inventory, compatibility policy and release documentation. | Shipping & maintenance (SHIP) | 13 |
| ADVGFX | Advanced GFX | The classification of advanced rendering programmes: ray tracing, virtualized geometry, virtual texturing, hair/groom and the path-traced reference. | Advanced GFX (ADVGFX) | 5 |
| NET | Multiplayer | Multiplayer foundations (network IDs, authority, fixed ticks, commands, headless execution, loopback proof) now, full networking deferred, local multiplayer excluded. | Multiplayer (NET) | 16 |
| MOD | Engine modules & projects | The in-repo game model: zenith_add_game, game.json, static optional modules with composition-root registration, and project creation without C++. | Engine modules & projects (MOD) | 13 |
| GRAPH | Graph core & CPU frame graph | The shared ZenithGraph core and the CPU frame graph built on it, with a conformance suite shared with Flux. | Graph core & CPU frame graph (GRAPH) | 14 |
| COMPRESS | Compression | The neutral Zenith_Compression API with zlib and zstd backends, streaming and one-shot modes, deterministic output, codec IDs, conformance and benchmarks. | Compression (COMPRESS) | 9 |
| SCRIPT | Scripting | Luau integration: compilation, modules, generated bindings, debugging, sandboxing, transactions, stateless systems, behaviour state and recovery, hot reload, VMs, budgets, profiling, the exception firewall and bytecode compatibility. | Scripting (SCRIPT) | 23 |
| TWOD | 2D | First-class 2D: sprites and batching, atlases, tilemaps, 2D animation and physics, pixel-perfect rendering, 2D authoring and the UI2D sample. | 2D (TWOD) | 10 |
| CONTENT | Content production | The supported-DCC matrix, coordinate and unit conventions, import presets, override-preserving reimport, recoverable batch operations, reference repair, importer isolation, shared DDC, semantic merges, distributed cooking and provenance. | Content production (CONTENT) | 31 |
| WORLD | World authoring | Editable world partitions, gameplay and data layers, concurrent editing, reproducible procedural generation and baking workflows, kept separate from runtime streaming. | World authoring (WORLD) | 9 |
| RPROD | Rendering production | The production renderer features: HiZ culling, indirect draw compaction, render views, automatic barriers, transient aliasing, offline shaders and permutation limits, PSO precaching, frame pacing, GPU timestamps, quality tiers, every path's full frame and the universal GPU rules. | Rendering production (RPROD) | 32 |
| CHAR | Character production | Rig and control authoring, pose debugging, animation budgeting, modular characters, animation/physics authority, facial and lip-sync workflows, motion-matching and hair decisions, and the complete character workflow. | Character production (CHAR) | 16 |
| ONLINE | Online services | The Zenith_OnlineServices adapter with an offline implementation, identity, entitlements, achievements, cloud saves, account changes, and the Steam and Google Play backends. | Online services (ONLINE) | 14 |
| SEC | Security & robustness | Trust boundaries for scripts, assets, remote tools and updates, parser limits, fuzzing, path-traversal protection, authenticated updates and shipping-build lockdown. | Security & robustness (SEC) | 10 |
| MAINT | Engine maintenance | API and version policy, dependency-upgrade rehearsals, deprecated-API removal, subsystem ownership, gate baseline and profile upkeep, and keeping sample games building. | Engine maintenance (MAINT) | 8 |
| RESEARCH | Research | The measurement methodology for the ECS-backend and shading-path comparisons, and its reports as decision records. | Research (RESEARCH) | 7 |
| SCOPE | Platform scope | The recorded scope boundaries: consoles, VR/XR, modding and local multiplayer, each with a decision ID, and the rule that deferred scopes get full requirement trees now. | Platform scope (SCOPE) | 11 |
| PROC | Process & gates | Every protected harness, configuration and gate edit, each a needs-human PROC ticket. | Process & gates (PROC) | 3 |
| | | | **Total** | **706** |

## Inventory v1

One table per area code, in the order of "Area codes".

### CORE Core & mathematics

Ownership, the general heap, arenas and pools, containers, strings, numerics, clocks, diagnostics, configuration primitives, and the cross-system memory governor.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-CORE-01 | ownership through `Zenith_Owned` | required | S2 | Z2-D-040; contract defined in S1 |
| Z2-I-CORE-02 | generational handles | required | S2 | for shared references; Z2-D-040 |
| Z2-I-CORE-03 | the general heap | required | S2 | Z2-D-040; backing is D-SEL-HEAP (open, Stage 1) |
| Z2-I-CORE-04 | arenas | required | S2 | Z2-D-040 |
| Z2-I-CORE-05 | pools | required | S2 | Z2-D-040 |
| Z2-I-CORE-06 | sanitizer integration | required | S2 | of the general heap, arenas and pools; Z2-D-011, Z2-D-040; ASan poisoning design in S1 |
| Z2-I-CORE-07 | containers | required | S2 | Z2-D-041; pulled forward into the S1 sprint where independent |
| Z2-I-CORE-08 | strings | required | S2 | stage inferred |
| Z2-I-CORE-09 | Unicode | required | S2 | stage inferred |
| Z2-I-CORE-10 | numerical conventions | required | S2 | stage inferred |
| Z2-I-CORE-11 | units | required | S2 | stage inferred |
| Z2-I-CORE-12 | coordinate systems | required | S2 | stage inferred |
| Z2-I-CORE-13 | random streams | required | S2 | counter-based per-invocation derivation, Z2-D-019; stage inferred |
| Z2-I-CORE-14 | clocks | required | S2 | stage inferred |
| Z2-I-CORE-15 | diagnostics | required | S2 | asserts and logging primitives, Z2-D-045; stage inferred |
| Z2-I-CORE-16 | configuration | required | S2 | layering itself is PLAT; stage inferred |
| Z2-I-CORE-17 | allocation-failure behaviour | required | S2 | `Zenith_TryNew`, OOM handler, Z2-D-040 |
| Z2-I-CORE-18 | a cross-system memory governor | required | S2 | the memory governor core |
| Z2-I-CORE-19 | per-subsystem memory budget: rendering | required | S1 | from Budgets.md; allocated by the Stage 1 per-system budget ticket for RPROD and RHI; enforced by the memory governor once the subsystem lands (Z2-I-CORE-26) |
| Z2-I-CORE-20 | per-subsystem memory budget: streaming | required | S1 | from Budgets.md; allocated by the Stage 1 per-system budget ticket for STREAM; enforced by the memory governor once the subsystem lands (Z2-I-CORE-26) |
| Z2-I-CORE-21 | per-subsystem memory budget: audio | required | S1 | from Budgets.md; allocated by the Stage 1 per-system budget ticket for AUDIO; enforced by the memory governor once the subsystem lands (Z2-I-CORE-26) |
| Z2-I-CORE-22 | per-subsystem memory budget: scripts | required | S1 | from Budgets.md; allocated by the Stage 1 per-system budget ticket for SCRIPT; enforced by the memory governor once the subsystem lands (Z2-I-CORE-26) |
| Z2-I-CORE-23 | per-subsystem memory budget: editor caches | required | S1 | from Budgets.md; allocated by the Stage 1 per-system budget ticket for EDIT; enforced by the memory governor once the subsystem lands (Z2-I-CORE-26) |
| Z2-I-CORE-24 | pressure levels (normal/warning/critical) derived from the OS | required | S2 | memory governor core |
| Z2-I-CORE-25 | pressure levels (normal/warning/critical) derived from the budgets | required | S2 | memory governor core |
| Z2-I-CORE-26 | defined shedding per subsystem at each pressure level | required | S4 | each subsystem implements its shedding; all present once streaming lands; stage inferred |
| Z2-I-CORE-27 | transition peak budgeted explicitly: streaming double-residency | required | S4 | stage inferred |
| Z2-I-CORE-28 | transition peak budgeted explicitly: world transitions | required | S4 | stage inferred |
| Z2-I-CORE-29 | transition peak budgeted explicitly: texture swaps | required | S4 | reallocate-and-swap, Z2-D-058; stage inferred |
| Z2-I-CORE-30 | tests that force each memory pressure level | required | S2 | extended as each subsystem gains shedding |

### JOBS Jobs & frame scheduling

The work-stealing scheduler that executes the compiled CPU frame-graph schedule, its sub-jobs, continuations, time stepping and shutdown.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-JOBS-01 | a work-stealing scheduler on `Zenith_Multithreading` | required | S2 | Z2-D-006 |
| Z2-I-JOBS-02 | scheduler driven by the GRAPH schedule | required | S2 | Z2-D-006 |
| Z2-I-JOBS-03 | dynamic parallel-for sub-jobs | required | S2 | Z2-D-006 |
| Z2-I-JOBS-04 | continuations | required | S2 | Z2-D-006 |
| Z2-I-JOBS-05 | no fibers | required | S2 | Z2-D-006 |
| Z2-I-JOBS-06 | no blocking waits | required | S2 | Z2-D-006 |
| Z2-I-JOBS-07 | cancellation | required | S2 |  |
| Z2-I-JOBS-08 | thread affinity | required | S2 | main thread, render-submit thread, Z2-D-006 |
| Z2-I-JOBS-09 | contention diagnostics | required | S2 | stage inferred |
| Z2-I-JOBS-10 | fixed simulation steps | required | S2 | shared with NET fixed ticks |
| Z2-I-JOBS-11 | interpolation | required | S2 |  |
| Z2-I-JOBS-12 | pause | required | S2 |  |
| Z2-I-JOBS-13 | time scaling | required | S2 |  |
| Z2-I-JOBS-14 | bounded catch-up | required | S2 |  |
| Z2-I-JOBS-15 | orderly shutdown | required | S2 | drain the frame, join workers in reverse dependency order |

### PLAT Platform & input

Windowing, input, the virtual filesystem and async I/O, configuration layering, the Android lifecycle, thermal adaptation and account changes, over SDL3.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-PLAT-01 | action mapping | required | S3 | input-action design kept XR-ready, Z2-D-072 |
| Z2-I-PLAT-02 | rebinding | required | S3 |  |
| Z2-I-PLAT-03 | input contexts | required | S3 |  |
| Z2-I-PLAT-04 | controllers | required | S3 |  |
| Z2-I-PLAT-05 | hotplug | required | S3 | controllers and input devices |
| Z2-I-PLAT-06 | haptics | required | S3 |  |
| Z2-I-PLAT-07 | touch | required | S3 |  |
| Z2-I-PLAT-08 | gestures | required | S3 |  |
| Z2-I-PLAT-09 | IME | required | S3 | text entry for UI; stage inferred |
| Z2-I-PLAT-10 | DPI | required | S2 | Platform layer; stage inferred |
| Z2-I-PLAT-11 | multiple windows | required | S3 | editor viewports; stage inferred |
| Z2-I-PLAT-12 | a virtual filesystem | required | S2 |  |
| Z2-I-PLAT-13 | mount precedence (package, patch, loose, user) | required | S2 |  |
| Z2-I-PLAT-14 | path-traversal protection | required | S2 | SEC trust boundary |
| Z2-I-PLAT-15 | async I/O | required | S2 |  |
| Z2-I-PLAT-16 | async I/O priorities | required | S2 |  |
| Z2-I-PLAT-17 | async I/O cancellation | required | S2 |  |
| Z2-I-PLAT-18 | low-disk detection | required | S2 | stage inferred |
| Z2-I-PLAT-19 | low-disk behaviour: refuse writes safely | required | S2 | stage inferred |
| Z2-I-PLAT-20 | low-disk behaviour: surface the condition to the game | required | S2 | stage inferred |
| Z2-I-PLAT-21 | configuration layering (engine default → platform → game → quality tier → user → command line) | required | S2 |  |
| Z2-I-PLAT-22 | configuration provenance | required | S2 |  |
| Z2-I-PLAT-23 | Android lifecycle | required | S2 | minimal game builds on Android in S2 |
| Z2-I-PLAT-24 | Android surface recovery | required | S3 | S24 suspend/resume in acceptance evidence; stage inferred |
| Z2-I-PLAT-25 | process death on Android: save a resumable state on background | required | S3 | needs save/load; stage inferred |
| Z2-I-PLAT-26 | process death on Android: restore on relaunch | required | S3 | needs save/load; stage inferred |
| Z2-I-PLAT-27 | thermal adaptation: the thermal-headroom API | required | S5 | per-platform quality tiers land in S5; stage inferred |
| Z2-I-PLAT-28 | thermal adaptation: headroom drives the quality tier | required | S5 | stage inferred |
| Z2-I-PLAT-29 | thermal adaptation: headroom drives dynamic resolution | required | S5 | stage inferred |
| Z2-I-PLAT-30 | account changes | required | S4 | rebinding flows with ONLINE; stage inferred |

### ECS Reflection & ECS

ZenithReflect and the neutral Zenith_ECS API with its three backends, runtime type registration and data-defined components.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-ECS-01 | stable type IDs | required | S2 | Z2-D-020 |
| Z2-I-ECS-02 | stable property IDs | required | S2 | Z2-D-020 |
| Z2-I-ECS-03 | schema evolution | required | S2 | migration framework, Z2-D-048 |
| Z2-I-ECS-04 | component lifecycle | required | S2 |  |
| Z2-I-ECS-05 | deferred structural changes | required | S2 | command buffers played back at sync points |
| Z2-I-ECS-06 | query invalidation | required | S2 | structural epochs, invocation-scoped views |
| Z2-I-ECS-07 | change tracking | required | S2 | per-column versions |
| Z2-I-ECS-08 | world isolation | required | S2 |  |
| Z2-I-ECS-09 | scheduling contracts | required | S2 | declared access for the CPU frame graph |
| Z2-I-ECS-10 | all-backend conformance | required | S2 | Z2-D-039; custom, flecs, EnTT |
| Z2-I-ECS-11 | representative benchmarks | required | S2 | research measurement in RESEARCH |
| Z2-I-ECS-12 | runtime type registration | required | S2 | Z2-D-020 |
| Z2-I-ECS-13 | data-defined component schemas (`*.schema.json` only) | required | S2 | Z2-D-020; format specified in S1 |
| Z2-I-ECS-14 | `.d.luau` generated from data-defined schemas | required | S2 | Z2-D-020 |
| Z2-I-ECS-15 | data-defined components stored natively in all three backends | required | S2 | EnTT via a dynamic-storage adapter (flagged proposal P1) |
| Z2-I-ECS-16 | data-defined components identical to C++-registered types for every consumer | required | S2 |  |
| Z2-I-ECS-17 | per-field phase-authority declarations | required | S2 | Z2-D-080 |

### SCENE Scenes & prefabs

Scene documents, authoring identities, prefabs with overrides and variants, loading, transform propagation and transitions.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-SCENE-01 | stable authoring identities | required | S2 | prerequisite of canonical serialization (Z2-D-038, Z2-I-CONTENT-28); editor use in S3 (editor foundation) |
| Z2-I-SCENE-02 | nested prefabs | required | S4 |  |
| Z2-I-SCENE-03 | prefab overrides | required | S4 |  |
| Z2-I-SCENE-04 | prefab variants | required | S4 |  |
| Z2-I-SCENE-05 | reference repair | required | S4 | stage inferred |
| Z2-I-SCENE-06 | additive scenes | required | S3 | stage inferred |
| Z2-I-SCENE-07 | transactional loading | required | S3 | stage inferred |
| Z2-I-SCENE-08 | transform propagation | required | S2 | frame phase 8, Z2-D-080; stage inferred |
| Z2-I-SCENE-09 | scene transitions | required | S3 | world-transition lifecycle command; stage inferred |
| Z2-I-SCENE-10 | missing-type handling | required | S2 | serialization and basic assets; stage inferred |

### STREAM World streaming

Runtime world streaming: partitioning, cell loading, cancellation, large coordinates, cross-cell references and coordinated subsystem streaming.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-STREAM-01 | spatial partitioning | required | S4 |  |
| Z2-I-STREAM-02 | async cell loading | required | S4 |  |
| Z2-I-STREAM-03 | priorities | required | S4 | of cell loads |
| Z2-I-STREAM-04 | cancellation | required | S4 | of cell loads; decomposed per the streaming-cancellation example |
| Z2-I-STREAM-05 | large-coordinate handling | required | S4 |  |
| Z2-I-STREAM-06 | cross-cell references | required | S4 |  |
| Z2-I-STREAM-07 | persistent entities | required | S4 |  |
| Z2-I-STREAM-08 | HLOD | required | S4 |  |
| Z2-I-STREAM-09 | coordinated physics streaming | required | S4 |  |
| Z2-I-STREAM-10 | coordinated navigation streaming | required | S4 | library is D-SEL-NAV (open, Stage 1/2) |
| Z2-I-STREAM-11 | coordinated audio streaming | required | S4 |  |

### ASSET Asset pipeline

The asset database, import and cook, the derived-data cache, validation, provenance, async loading, residency and decode back-pressure.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-ASSET-01 | asset database | required | S2 | basic assets |
| Z2-I-ASSET-02 | asset dependency graph | required | S2 | basic assets |
| Z2-I-ASSET-03 | import | required | S2 | basic assets |
| Z2-I-ASSET-04 | reimport | required | S3 | full asset pipeline; first required at S3 (moved from S4 by Z2-D-131): Stage 3 acceptance evidence requires reimport and clean versus incremental cook equivalence; reimport through the cook server stays S4 at requirement granularity |
| Z2-I-ASSET-05 | stable GUIDs | required | S2 |  |
| Z2-I-ASSET-06 | importer settings | required | S2 | importer `.meta` settings are authoring documents, Z2-D-048 |
| Z2-I-ASSET-07 | derived-data cache | required | S4 |  |
| Z2-I-ASSET-08 | derived-data cache invalidation | required | S4 |  |
| Z2-I-ASSET-09 | incremental cooking | required | S4 |  |
| Z2-I-ASSET-10 | asset validation | required | S4 |  |
| Z2-I-ASSET-11 | asset provenance | required | S4 |  |
| Z2-I-ASSET-12 | missing assets | required | S3 | missing or corrupt content is acceptance evidence; first required at S3 (moved from S4 by Z2-D-131): Stage 3 acceptance evidence requires missing or corrupt content; retry after mount changes stays S4 at requirement granularity |
| Z2-I-ASSET-13 | corrupt assets | required | S3 | first required at S3 (moved from S4 by Z2-D-131): Stage 3 acceptance evidence requires missing or corrupt content; the full corruption fixture sweep in the integration suite stays S4 at requirement granularity |
| Z2-I-ASSET-14 | async loading | required | S2 | async I/O lands in S2; stage inferred |
| Z2-I-ASSET-15 | residency budgets | required | S4 |  |
| Z2-I-ASSET-16 | decompression back-pressure: bounded decode queues | required | S4 |  |
| Z2-I-ASSET-17 | decompression back-pressure: I/O throttled by decode capacity | required | S4 |  |

### PACK Packaging & content updates

Versioned cooked formats, archives, chunking, install layouts, Android App Bundles and Play Asset Delivery, patches, DLC and interrupted-update recovery.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-PACK-01 | versioned binary formats | required | S2 | Z2-D-038 |
| Z2-I-PACK-02 | archives | required | S4 | stage inferred |
| Z2-I-PACK-03 | compression | required | S2 | in packages; ZenithCompression, Z2-D-004 |
| Z2-I-PACK-04 | chunking | required | S4 | stage inferred |
| Z2-I-PACK-05 | installation layouts | required | S6 |  |
| Z2-I-PACK-06 | Android App Bundles | required | S6 | Z2-D-027 |
| Z2-I-PACK-07 | Play Asset Delivery asset packs | required | S6 | Z2-D-027 |
| Z2-I-PACK-08 | bundletool split-APK testing | required | S6 | Z2-D-027 |
| Z2-I-PACK-09 | installed-update migration | required | S6 | Z2-D-027 |
| Z2-I-PACK-10 | patch manifests | required | S6 |  |
| Z2-I-PACK-11 | DLC manifests | required | S6 |  |
| Z2-I-PACK-12 | update compatibility | required | S6 | cooked data compatible only within one release line, Z2-D-048 |
| Z2-I-PACK-13 | interrupted-update recovery | required | S6 |  |
| Z2-I-PACK-14 | interrupted content installation: PAD pack download interrupted, then resumed | required | S6 |  |
| Z2-I-PACK-15 | interrupted content installation: rolled back | required | S6 |  |
| Z2-I-PACK-16 | interrupted content installation: the game left in a consistent state | required | S6 |  |

### RHI RHI & render graph

The neutral RHI with Vulkan and D3D12 backends: GPU lifetime and synchronisation, allocation, presentation, capabilities, validation and recovery.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-RHI-01 | GPU lifetime: completion tracking, a timeline semaphore per queue, and retirement of a resource or descriptor slot only once every queue's last use of it has completed | required | S2 | expanded from GPU lifetime and sync, `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`; both RHIs |
| Z2-I-RHI-02 | GPU lifetime: descriptor slots reused only after that retirement | required | S2 | `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"` |
| Z2-I-RHI-03 | GPU lifetime: frames in flight, configurable 2–3 | required | S2 | `Z2-B-RHI-008` frames in flight |
| Z2-I-RHI-04 | GPU lifetime: an upload ring that stalls until space frees and grows only up to a hard cap | required | S2 | `Z2-B-RHI-006` upload ring hard cap, `Z2-B-RHI-007` upload ring stall frames |
| Z2-I-RHI-05 | GPU lifetime: an upload request larger than the cap split, or refused with an error; exhaustion reported as a telemetry counter | required | S2 | `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"` |
| Z2-I-RHI-06 | GPU lifetime: readbacks as operation handles owned by the requester, resolved after fence completion, with a documented lifetime | required | S2 | `Zenith2/Docs/Contracts.md → "Handle kinds"` |
| Z2-I-RHI-07 | GPU sync: tracked resource kinds (imported resources, persistent histories, subresource ranges, read-modify-write access, queue-family ownership transfers, aliasing barriers) | required | S2 | Z2-D-056; `Zenith2/Docs/Rendering.md → "Barrier synthesis"` |
| Z2-I-RHI-08 | GPU sync: GPU scene tables and pools following the GPU Scene model, tables read-only and dynamic content reached through pools | required | S3 | Z2-D-058; `Zenith2/Docs/Rendering.md → "GPU scene tables and pools"`; GPU-driven drawing lands in S3; stage inferred |
| Z2-I-RHI-09 | allocation | required | S2 | VMA, D3D12MA |
| Z2-I-RHI-10 | residency | required | S4 | residency budgets, `Z2-B-RHI-003` GPU residency budget per heap; stage inferred |
| Z2-I-RHI-11 | GPU memory: heap alignment and placement rules per API | required | S2 | `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`; stage inferred |
| Z2-I-RHI-12 | GPU memory: responses to memory pressure | required | S4 | with the CORE memory governor; stage inferred |
| Z2-I-RHI-13 | presentation | required | S2 | swapchain; frame pacing is RPROD |
| Z2-I-RHI-14 | capability negotiation | required | S2 | Vulkan 1.3 profile, SM 6.6, Z2-D-013 |
| Z2-I-RHI-15 | validation | required | S2 | sync validation and GBV clean, Z2-D-056 |
| Z2-I-RHI-16 | graph inspection | required | S2 | ZenithGraph JSON dump, frame dump; stage inferred |
| Z2-I-RHI-17 | captures | required | S3 | stage inferred |
| Z2-I-RHI-18 | queue scheduling | required | S2 | graph queue assignment |
| Z2-I-RHI-19 | recovery diagnostics | required | S6 | recovery; `Zenith2/Docs/Rendering.md → "Recovery"` |
| Z2-I-RHI-20 | recovery: swapchain recreation | required | S2 | needed with presentation; `Zenith2/Docs/Rendering.md → "Recovery"`; stage inferred |
| Z2-I-RHI-21 | recovery: device loss, with breadcrumbs and a recover-or-clean-exit policy | required | S3 | `Zenith2/Docs/Rendering.md → "Recovery"`; moved from S6 to S3 by Z2-D-131: Stage 3 acceptance evidence requires resize and device-loss recovery; Z2-I-RHI-19 stays S6 |
| Z2-I-RHI-22 | recovery: PSO-cache invalidation keyed on driver and version | required | S3 | with PSO precaching; stage inferred |

### GEOM Geometry & materials

The vertex data format and codec, LOD, instancing, meshlets, skinning, morphs, material permutations, transparency, decals, terrain geometry and texture streaming.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-GEOM-01 | vertex data format: Slang-declared streams | required | S2 | expanded from the vertex data format, `Zenith2/Docs/Rendering.md → "Vertex data format"`; Z2-D-062 |
| Z2-I-GEOM-02 | vertex data format: the shared CPU/GPU codec | required | S2 | `ZenithVertexFormat`, Z2-D-062 |
| Z2-I-GEOM-03 | vertex data format: a position stream | required | S2 | dedicated stream 0; Z2-D-062 |
| Z2-I-GEOM-04 | vertex data format: QTangent normals | required | S2 | Z2-D-062 |
| Z2-I-GEOM-05 | vertex data format: octahedral normals | required | S2 | Z2-D-062 |
| Z2-I-GEOM-06 | vertex data format: UNORM16 UVs with scale and bias | required | S2 | Z2-D-062 |
| Z2-I-GEOM-07 | vertex data format: 16-bit indices with mesh splitting | required | S2 | Z2-D-062 |
| Z2-I-GEOM-08 | vertex data format: precision-checked fallbacks | required | S2 | Z2-D-062 |
| Z2-I-GEOM-09 | the D-VTX-TANGENTLESS experiment | required | S3 | D-VTX-TANGENTLESS (open, Stage 3); QTangent is the default until the ruling |
| Z2-I-GEOM-10 | LOD generation | required | S4 | content production; stage inferred |
| Z2-I-GEOM-11 | LOD selection | required | S3 | stage inferred |
| Z2-I-GEOM-12 | instancing | required | S3 | GPU-driven indirect draws; stage inferred |
| Z2-I-GEOM-13 | meshlets | required | S5 | visibility-buffer path; stage inferred |
| Z2-I-GEOM-14 | skinning | required | S3 | compute skinning into the skinned-vertex pool; stage inferred |
| Z2-I-GEOM-15 | morph targets | required | S3 | stage inferred |
| Z2-I-GEOM-16 | material permutations | required | S3 | capped by the RPROD permutation limit; stage inferred |
| Z2-I-GEOM-17 | transparency | required | S3 | transparent forward pass; stage inferred |
| Z2-I-GEOM-18 | decals | required | S3 | clustered decals in Forward+; stage inferred |
| Z2-I-GEOM-19 | terrain geometry | required | S3 | moved from S4 to S3 by Z2-D-131: W-Outdoor is authored and measured at full scale in Stage 3 |
| Z2-I-GEOM-20 | texture streaming | required | S4 | reallocate-and-swap, Z2-D-058 |
| Z2-I-GEOM-21 | sampler policy | required | S2 | static sampler table, Z2-D-054; stage inferred |
| Z2-I-GEOM-22 | advanced surface models | required | S5 | advanced lighting and materials |

### LIGHT Lighting & image quality

Shadows, probes, global illumination, reflections, volumetrics, temporal history, antialiasing and upscaling, exposure, colour and HDR output, and camera effects.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-LIGHT-01 | directional-light shadows | required | S3 | Forward+ frame; stage inferred |
| Z2-I-LIGHT-02 | local-light shadows | required | S3 | shadow views, Z2-D-059; stage inferred |
| Z2-I-LIGHT-03 | probes | required | S4 | capture views; stage inferred |
| Z2-I-LIGHT-04 | baked GI | required | S5 | advanced lighting; stage inferred |
| Z2-I-LIGHT-05 | dynamic GI | required | S5 | validated against its own error budget, Z2-D-063; stage inferred; platform scope: SSGI on WIN_HIGH and WIN_EPIC, sky-SH and probe recapture elsewhere (human ruling Q41 (a), 2026-10-16) |
| Z2-I-LIGHT-06 | reflections | required | S3 | SSR in the Forward+ frame; stage inferred |
| Z2-I-LIGHT-07 | volumetrics | required | S3 | froxel fog in the Forward+ frame; stage inferred |
| Z2-I-LIGHT-08 | temporal-history management | required | S2 | independent per-view histories, Z2-D-059 |
| Z2-I-LIGHT-09 | antialiasing | required | S3 | TAA; no MSAA in any path, Z2-D-035; stage inferred |
| Z2-I-LIGHT-10 | upscaling | required | S3 | quality-tier setting; stage inferred |
| Z2-I-LIGHT-11 | dynamic resolution | required | S3 | 75% floor, Z2-D-012; stage inferred |
| Z2-I-LIGHT-12 | exposure | required | S3 | stage inferred |
| Z2-I-LIGHT-13 | colour management | required | S3 | stage inferred |
| Z2-I-LIGHT-14 | HDR | required | S5 | HDR output: SDR, HDR10/scRGB output transforms; stage inferred |
| Z2-I-LIGHT-15 | motion blur | required | S5 | stage inferred |
| Z2-I-LIGHT-16 | depth of field | required | S5 | stage inferred |

### ENV Environment

Terrain authoring, foliage, water, sky, weather and time of day, procedural placement, and generated collision and navigation.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-ENV-01 | terrain sculpting | required | S3 | visual authoring, Z2-D-079; moved from S4 to S3 by Z2-D-131: W-Outdoor is authored and measured at full scale in Stage 3 |
| Z2-I-ENV-02 | terrain painting | required | S4 | visual authoring, Z2-D-079 |
| Z2-I-ENV-03 | foliage placement | required | S3 | moved from S4 to S3 by Z2-D-131: W-Outdoor is authored and measured at full scale in Stage 3 |
| Z2-I-ENV-04 | foliage wind | required | S4 |  |
| Z2-I-ENV-05 | water | required | S4 |  |
| Z2-I-ENV-06 | atmosphere | required | S3 | moved from S4 to S3 by Z2-D-131: W-Outdoor is authored and measured at full scale in Stage 3 |
| Z2-I-ENV-07 | clouds | required | S4 |  |
| Z2-I-ENV-08 | weather | required | S4 |  |
| Z2-I-ENV-09 | day/night | required | S3 | moved from S4 to S3 by Z2-D-131: W-Outdoor is authored and measured at full scale in Stage 3 |
| Z2-I-ENV-10 | procedural placement | required | S3 | moved from S4 to S3 by Z2-D-131: W-Outdoor is authored and measured at full scale in Stage 3 |
| Z2-I-ENV-11 | collision generation | required | S4 |  |
| Z2-I-ENV-12 | navigation generation | required | S4 | D-SEL-NAV (open, Stage 1/2) |
| Z2-I-ENV-13 | environment streaming integration | required | S4 |  |

### ANIM Animation

Skeletal animation: import, compression, graphs, layers, events, root motion, retargeting, IK, facial animation and ragdoll transitions, with visual authoring.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-ANIM-01 | skeleton import | required | S3 |  |
| Z2-I-ANIM-02 | animation compression | required | S4 | stage inferred |
| Z2-I-ANIM-03 | animation streaming | required | S4 | stage inferred |
| Z2-I-ANIM-04 | blend trees | required | S3 | motion matching is D-SEL-MOTIONMATCH (open, Stage 3) |
| Z2-I-ANIM-05 | state machines | required | S3 |  |
| Z2-I-ANIM-06 | additive layers | required | S3 |  |
| Z2-I-ANIM-07 | masks | required | S3 |  |
| Z2-I-ANIM-08 | synchronization | required | S3 |  |
| Z2-I-ANIM-09 | animation events | required | S3 |  |
| Z2-I-ANIM-10 | root motion | required | S3 | extraction in phase 3, application in phase 4, Z2-D-080 |
| Z2-I-ANIM-11 | retargeting | required | S4 | character production; stage inferred |
| Z2-I-ANIM-12 | IK | required | S4 | stage inferred |
| Z2-I-ANIM-13 | motion warping | required | S4 | stage inferred |
| Z2-I-ANIM-14 | facial animation | required | S4 | stage inferred |
| Z2-I-ANIM-15 | ragdoll transitions | required | S3 | ragdoll enter/recover lifecycle command |
| Z2-I-ANIM-16 | animation visual authoring | required | S4 | Z2-D-079; editor workflows |

### PHYS Physics

The neutral Zenith_Physics API with Jolt and PhysX backends, conformance and determinism, collision, queries, controllers, constraints, vehicles, ragdolls, and classified specialist simulations.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-PHYS-01 | the neutral `Zenith_Physics` API | required | S3 | Z2-D-003 |
| Z2-I-PHYS-02 | the Jolt backend | required | S3 | Z2-D-003 |
| Z2-I-PHYS-03 | the PhysX backend | required | S3 | Z2-D-003; Android per D-SEL-PHYSX-ANDROID (open, Stage 1) |
| Z2-I-PHYS-04 | a shared physics conformance suite | required | S3 | Z2-D-003 |
| Z2-I-PHYS-05 | per-backend determinism | required | S3 | Z2-D-003, Z2-D-019 |
| Z2-I-PHYS-06 | collision cooking | required | S3 |  |
| Z2-I-PHYS-07 | collision filtering | required | S3 |  |
| Z2-I-PHYS-08 | physics queries | required | S3 | async results at tick boundaries, Z2-D-053 |
| Z2-I-PHYS-09 | triggers | required | S3 |  |
| Z2-I-PHYS-10 | character controllers | required | S3 |  |
| Z2-I-PHYS-11 | constraints | required | S3 |  |
| Z2-I-PHYS-12 | joints | required | S3 |  |
| Z2-I-PHYS-13 | vehicles | required | S4 | stage inferred |
| Z2-I-PHYS-14 | ragdolls | required | S3 |  |
| Z2-I-PHYS-15 | physics debug visualization | required | S3 |  |
| Z2-I-PHYS-16 | physics streaming | required | S4 | coordinated with STREAM |
| Z2-I-PHYS-17 | cloth, as an explicitly classified extension: classified with an owner and a library candidate (D-CLASS-SIM) | required | S2 | classification pending D-CLASS-SIM (open, Stage 2); this row is the classification ruling; the capability's own classification and stage are set by the ruling under the Change rule; no capability tickets beyond the ruling ticket until then; merges the extended row's cloth item |
| Z2-I-PHYS-18 | soft bodies, as an explicitly classified extension: classified with an owner and a library candidate (D-CLASS-SIM) | required | S2 | classification pending D-CLASS-SIM (open, Stage 2); this row is the classification ruling; the capability's own classification and stage are set by the ruling under the Change rule; merges the extended row's soft-bodies item |
| Z2-I-PHYS-19 | destruction, as an explicitly classified extension: classified with an owner and a library candidate (D-CLASS-SIM) | required | S2 | classification pending D-CLASS-SIM (open, Stage 2); this row is the classification ruling; the capability's own classification and stage are set by the ruling under the Change rule; merges the extended row's destruction item |
| Z2-I-PHYS-20 | specialist simulations, as explicitly classified extensions | required | S5 | delivery of whatever D-CLASS-SIM (open, Stage 2) classifies required: the in-scope specialist simulations |
| Z2-I-PHYS-21 | CCD | required | S3 | extended row; stage inferred |
| Z2-I-PHYS-22 | substepping | required | S3 | extended row; stage inferred |
| Z2-I-PHYS-23 | contact ordering | required | S3 | extended row; determinism; stage inferred |
| Z2-I-PHYS-24 | event ordering | required | S3 | extended row; determinism; stage inferred |
| Z2-I-PHYS-25 | physical materials | required | S3 | extended row; stage inferred |
| Z2-I-PHYS-26 | physical interactions | required | S3 | extended row; stage inferred |
| Z2-I-PHYS-27 | moving platforms | required | S3 | extended row; stage inferred |
| Z2-I-PHYS-28 | large-coordinate behaviour | required | S4 | extended row; with STREAM; stage inferred |
| Z2-I-PHYS-29 | determinism boundaries | required | S3 | extended row; Z2-D-019; stage inferred |

### AI AI & navigation

Navmesh baking and streaming, pathfinding, avoidance and crowds, perception, blackboards, behaviour authoring, debugging and reproducible scenarios.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-AI-01 | navmesh baking | required | S4 | D-SEL-NAV (open, Stage 1/2) |
| Z2-I-AI-02 | navmesh streaming | required | S4 |  |
| Z2-I-AI-03 | dynamic obstacles | required | S4 |  |
| Z2-I-AI-04 | off-mesh links | required | S4 |  |
| Z2-I-AI-05 | async pathfinding | required | S4 | results at tick boundaries, Z2-D-053 |
| Z2-I-AI-06 | avoidance | required | S4 |  |
| Z2-I-AI-07 | crowds | required | S4 |  |
| Z2-I-AI-08 | perception | required | S4 |  |
| Z2-I-AI-09 | blackboards | required | S4 |  |
| Z2-I-AI-10 | behaviour authoring | required | S4 |  |
| Z2-I-AI-11 | state authoring | required | S4 |  |
| Z2-I-AI-12 | AI debugging | required | S4 |  |
| Z2-I-AI-13 | reproducible AI scenarios | required | S4 | Z2-D-019 |
| Z2-I-AI-14 | AI simulation LOD | required | S4 | extended row |
| Z2-I-AI-15 | navigation for different agent sizes | required | S4 | extended row |
| Z2-I-AI-16 | flying navigation | required | S4 | extended row |
| Z2-I-AI-17 | swimming navigation | required | S4 | extended row |

### GAME Gameplay framework

Game and world lifecycle, cameras, reusable gameplay primitives, script integration, the frame-phase contract and lifecycle commands; quest, dialogue, inventory and interaction live only in OpenWorldSlice.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-GAME-01 | game lifecycle | required | S3 | stage inferred |
| Z2-I-GAME-02 | world lifecycle | required | S3 | stage inferred |
| Z2-I-GAME-03 | cameras | required | S3 | stage inferred |
| Z2-I-GAME-04 | tags | required | S3 | stage inferred |
| Z2-I-GAME-05 | events | required | S3 | gameplay events (tags/events); stage inferred |
| Z2-I-GAME-06 | reusable state primitives | required | S3 | stage inferred |
| Z2-I-GAME-07 | reusable ability primitives | required | S3 | stage inferred |
| Z2-I-GAME-08 | timers | required | S3 | stage inferred |
| Z2-I-GAME-09 | script integration | required | S3 | Luau gameplay |
| Z2-I-GAME-10 | samples without hard-coding one game's rules | required | S4 | stage inferred |
| Z2-I-GAME-11 | interaction, as a Luau sample only | game-side | S4 | extended row; Z2-D-077; owned by OpenWorldSlice; covered by an OpenWorldSlice Luau-sample requirement in GAME |
| Z2-I-GAME-12 | quest, as a Luau sample only | game-side | S4 | extended row; Z2-D-077; owned by OpenWorldSlice; covered by an OpenWorldSlice Luau-sample requirement in GAME |
| Z2-I-GAME-13 | dialogue, as a Luau sample only | game-side | S4 | extended row; Z2-D-077; owned by OpenWorldSlice; covered by an OpenWorldSlice Luau-sample requirement in GAME |
| Z2-I-GAME-14 | inventory, as a Luau sample only | game-side | S4 | extended row; Z2-D-077; owned by OpenWorldSlice; covered by an OpenWorldSlice Luau-sample requirement in GAME |
| Z2-I-GAME-15 | player ownership | required | S3 | extended row; stage inferred |
| Z2-I-GAME-16 | player possession | required | S3 | extended row; stage inferred |
| Z2-I-GAME-17 | camera collision | required | S3 | extended row; stage inferred |
| Z2-I-GAME-18 | camera blending | required | S3 | extended row; stage inferred |
| Z2-I-GAME-19 | async gameplay actions | required | S3 | extended row; stage inferred |
| Z2-I-GAME-20 | attributes | required | S4 | extended row; stage inferred |
| Z2-I-GAME-21 | effects | required | S4 | extended row; stage inferred |
| Z2-I-GAME-22 | the frame-phase and authority contract (D-PHASES) | required | S1 | extended row; Z2-D-080; D-PHASES (open, Stage 1) refines; implemented in S2 with the CPU frame graph |
| Z2-I-GAME-23 | lifecycle command: spawn | required | S2 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-24 | lifecycle command: destroy | required | S2 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-25 | lifecycle command: teleport | required | S3 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-26 | lifecycle command: ragdoll enter | required | S3 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-27 | lifecycle command: ragdoll recover | required | S3 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-28 | lifecycle command: cinematic start | required | S4 | extended row; Z2-D-080; with CINE; stage inferred |
| Z2-I-GAME-29 | lifecycle command: cinematic interrupt | required | S4 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-30 | lifecycle command: cinematic skip with state restore | required | S4 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-31 | lifecycle command: world transition | required | S3 | extended row; Z2-D-080; stage inferred |
| Z2-I-GAME-32 | lifecycle command: shutdown | required | S2 | extended row; Z2-D-080; stage inferred |

### SAVE Save games & replay

Versioned saves, checkpoints, persistent state, atomic writes and recovery, migration, input recording, replays and cross-system failure scenarios.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-SAVE-01 | versioned saves | required | S3 | save/load |
| Z2-I-SAVE-02 | checkpoints | required | S3 |  |
| Z2-I-SAVE-03 | persistent world state | required | S4 | OpenWorldSlice persistent-world saves; stage inferred |
| Z2-I-SAVE-04 | persistent entity state | required | S4 | stage inferred |
| Z2-I-SAVE-05 | atomic writes | required | S3 |  |
| Z2-I-SAVE-06 | corruption recovery | required | S3 |  |
| Z2-I-SAVE-07 | slot metadata | required | S3 |  |
| Z2-I-SAVE-08 | save migration fixtures | required | S3 | one per shipped save version, Z2-D-048; demonstrated S6 |
| Z2-I-SAVE-09 | async saving | required | S3 |  |
| Z2-I-SAVE-10 | input recording | required | S3 | shared with replayable commands, Z2-D-073 |
| Z2-I-SAVE-11 | reproducibility boundaries | required | S3 | Z2-D-019 |
| Z2-I-SAVE-12 | consistent save snapshots while jobs and streaming are active (at the D-PHASES snapshot point) | required | S4 | extended row; phase 11, Z2-D-080; D-PHASES (open, Stage 1) refines; stage inferred |
| Z2-I-SAVE-13 | replay seeking | required | S4 | extended row; stage inferred |
| Z2-I-SAVE-14 | replay version compatibility | required | S6 | extended row; migration compatibility; stage inferred |
| Z2-I-SAVE-15 | deterministic replays per the determinism guarantee, including script failures | required | S3 | extended row; Z2-D-019 |
| Z2-I-SAVE-16 | cross-system failure scenario: saving during streaming with active animation and physics | required | S4 | extended row; stage inferred |
| Z2-I-SAVE-17 | cross-system failure scenario: an interrupted write during that save | required | S4 | extended row; stage inferred |
| Z2-I-SAVE-18 | cross-system failure scenario: restoring a consistent world (no half-streamed cells, no duplicated or lost entities, coherent physics and animation state) | required | S4 | extended row; stage inferred |

### AUDIO Audio

Streaming and decode, voices, mixing, spatialization, acoustics, music, dialogue, device and focus handling, real-time constraints, authoring and profiling.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-AUDIO-01 | audio streaming | required | S3 | codec is D-SEL-CODEC (open, Stage 2) |
| Z2-I-AUDIO-02 | audio decode | required | S3 | D-SEL-CODEC (open, Stage 2) |
| Z2-I-AUDIO-03 | voice budgets | required | S3 |  |
| Z2-I-AUDIO-04 | buses | required | S3 |  |
| Z2-I-AUDIO-05 | mixing | required | S3 |  |
| Z2-I-AUDIO-06 | spatialization | required | S3 |  |
| Z2-I-AUDIO-07 | occlusion | required | S4 | stage inferred |
| Z2-I-AUDIO-08 | reverb | required | S4 | stage inferred |
| Z2-I-AUDIO-09 | music transitions | required | S4 | stage inferred |
| Z2-I-AUDIO-10 | dialogue audio | required | S4 | localization; stage inferred |
| Z2-I-AUDIO-11 | localized audio | required | S4 | localization; stage inferred |
| Z2-I-AUDIO-12 | audio device changes | required | S3 |  |
| Z2-I-AUDIO-13 | Android audio focus | required | S3 | S24 audio interruption in acceptance evidence |
| Z2-I-AUDIO-14 | real-time audio thread constraints | required | S3 |  |
| Z2-I-AUDIO-15 | audio profiling | required | S3 |  |
| Z2-I-AUDIO-16 | audio authoring | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-17 | audio auditioning | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-18 | virtual voices | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-19 | acoustic zones | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-20 | animation/audio clock sync | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-21 | complete audio workflow: import | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-22 | complete audio workflow: bank/event authoring | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-23 | complete audio workflow: mix | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-24 | complete audio workflow: audition in context | required | S4 | extended row; stage inferred |
| Z2-I-AUDIO-25 | audio workflow usability acceptance | required | S4 | extended row; scripted walkthrough; stage inferred |

### UI UI, localization, accessibility

Retained UI and layout, focus navigation, text shaping and bidi, localization, scaling, captions, remapping, accessibility features and end-to-end accessible flows, with visual authoring.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-UI-01 | retained UI | required | S3 | runtime is D-SEL-UIRT (open, Stage 1/2) |
| Z2-I-UI-02 | UI layout | required | S3 |  |
| Z2-I-UI-03 | UI navigation | required | S3 |  |
| Z2-I-UI-04 | UI focus | required | S3 |  |
| Z2-I-UI-05 | text shaping | required | S3 | D-SEL-TEXT (open, Stage 1/2) |
| Z2-I-UI-06 | font fallback | required | S3 | D-SEL-TEXT (open, Stage 1/2) |
| Z2-I-UI-07 | bidirectional text | required | S4 | bidi library in D-SEL-TEXT (open, Stage 1/2); stage inferred |
| Z2-I-UI-08 | localization extraction | required | S4 | localization |
| Z2-I-UI-09 | pluralization | required | S4 | localization |
| Z2-I-UI-10 | UI scaling | required | S3 | stage inferred |
| Z2-I-UI-11 | captions | required | S4 | stage inferred |
| Z2-I-UI-12 | input remapping | required | S3 | with PLAT rebinding; stage inferred |
| Z2-I-UI-13 | accessibility settings | required | S4 | stage inferred |
| Z2-I-UI-14 | UI visual authoring | required | S4 | Z2-D-079; editor workflows; stage inferred |
| Z2-I-UI-15 | large-list virtualization | required | S4 | extended row; stage inferred |
| Z2-I-UI-16 | world-space UI | required | S3 | extended row; drawn in the transparent pass; stage inferred |
| Z2-I-UI-17 | input arbitration | required | S3 | extended row; stage inferred |
| Z2-I-UI-18 | semantic accessibility information | required | S4 | extended row; stage inferred |
| Z2-I-UI-19 | narration: screen-reader hooks | required | S4 | extended row; stage inferred |
| Z2-I-UI-20 | narration: TTS hooks | required | S4 | extended row; stage inferred |
| Z2-I-UI-21 | reduced motion | required | S4 | extended row; stage inferred |
| Z2-I-UI-22 | colour-blind modes | required | S4 | extended row; stage inferred |
| Z2-I-UI-23 | subtitles and captions | required | S4 | extended row; presentation of the captions item; stage inferred |
| Z2-I-UI-24 | localization stress testing: pseudo-localization | required | S4 | extended row; stage inferred |
| Z2-I-UI-25 | localization stress testing: long strings | required | S4 | extended row; stage inferred |
| Z2-I-UI-26 | localization stress testing: RTL | required | S4 | extended row; stage inferred |
| Z2-I-UI-27 | accessibility end-to-end flow: Game2D and OpenWorldSlice menus, settings and first 10 minutes completed with a screen reader | required | S4 | extended row; scripted acceptance run; first playable milestones; stage inferred |
| Z2-I-UI-28 | accessibility end-to-end flow: the same completed with remapped controls | required | S4 | extended row; scripted acceptance run; stage inferred |
| Z2-I-UI-29 | accessibility end-to-end flow: the same completed with reduced motion | required | S4 | extended row; scripted acceptance run; stage inferred |
| Z2-I-UI-30 | accessibility end-to-end flow: the same completed with a colour-blind mode | required | S4 | extended row; scripted acceptance run; stage inferred |

### VFX VFX

CPU and GPU particles, ribbons and trails, collision, culling, sorting, LOD, event integration and budgets, authored in code with previews.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-VFX-01 | CPU particles | required | S5 | VFX |
| Z2-I-VFX-02 | GPU particles | required | S5 | particle pool, Z2-D-058 |
| Z2-I-VFX-03 | ribbons | required | S5 |  |
| Z2-I-VFX-04 | trails | required | S5 |  |
| Z2-I-VFX-05 | particle collision | required | S5 |  |
| Z2-I-VFX-06 | particle bounds | required | S5 |  |
| Z2-I-VFX-07 | particle culling | required | S5 |  |
| Z2-I-VFX-08 | particle sorting | required | S5 |  |
| Z2-I-VFX-09 | VFX LOD | required | S5 |  |
| Z2-I-VFX-10 | VFX event integration | required | S5 |  |
| Z2-I-VFX-11 | VFX budgets | required | S5 |  |
| Z2-I-VFX-12 | previews for code-authored effects | required | S5 | Z2-D-079 |
| Z2-I-VFX-13 | inspectors for code-authored effects | required | S5 | Z2-D-079 |

### CINE Cinematics & media

Timeline sequencing with camera, animation, audio and event tracks, scrubbing and blending, dialogue and subtitle sync, video playback, capture and export, interruption and state restore, with visual authoring.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-CINE-01 | timeline sequencing | required | S4 |  |
| Z2-I-CINE-02 | camera tracks | required | S4 |  |
| Z2-I-CINE-03 | animation tracks | required | S4 |  |
| Z2-I-CINE-04 | audio tracks | required | S4 |  |
| Z2-I-CINE-05 | event tracks | required | S4 |  |
| Z2-I-CINE-06 | timeline scrubbing | required | S4 |  |
| Z2-I-CINE-07 | timeline blending | required | S4 |  |
| Z2-I-CINE-08 | dialogue sync | required | S4 |  |
| Z2-I-CINE-09 | subtitle sync | required | S4 |  |
| Z2-I-CINE-10 | video playback | required | S4 | D-SEL-VIDEO (open, Stage 2) |
| Z2-I-CINE-11 | capture | required | S4 |  |
| Z2-I-CINE-12 | export | required | S4 |  |
| Z2-I-CINE-13 | cinematics visual authoring | required | S4 | Z2-D-079 |
| Z2-I-CINE-14 | cinematic interruption | required | S4 | extended row; lifecycle command, Z2-D-080 |
| Z2-I-CINE-15 | cinematic skip | required | S4 | extended row; lifecycle command |
| Z2-I-CINE-16 | restoring gameplay state after a sequence | required | S4 | extended row; pre-cinematic snapshot of owned fields |

### EDIT Editor & collaboration

The editor foundation and tools: undo, selection, gizmos, asset browser, previews, prefab editing, recovery, play worlds, source control, merge-friendly files, extension APIs, migration UI and usability acceptance.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-EDIT-01 | undo/redo transactions | required | S3 | editor foundation (command system, undo, document ownership) |
| Z2-I-EDIT-02 | selection | required | S3 | editor foundation |
| Z2-I-EDIT-03 | gizmos | required | S3 | minimal editor; stage inferred |
| Z2-I-EDIT-04 | snapping | required | S3 | stage inferred |
| Z2-I-EDIT-05 | asset browser | required | S3 | stage inferred |
| Z2-I-EDIT-06 | previews | required | S3 | `VIEW_KIND_PREVIEW`, Z2-D-059; stage inferred |
| Z2-I-EDIT-07 | prefab editing | required | S4 | prefabs; stage inferred |
| Z2-I-EDIT-08 | autosave | required | S4 | stage inferred |
| Z2-I-EDIT-09 | recovery | required | S4 | crash recovery (autosave/recovery); acceptance evidence; stage inferred |
| Z2-I-EDIT-10 | multi-world play | required | S3 | independent play worlds; stage inferred |
| Z2-I-EDIT-11 | source-control integration | required | S4 | editor workflows; stage inferred |
| Z2-I-EDIT-12 | merge-friendly files | required | S4 | canonical serialization lands S2 in SCENE; stage inferred |
| Z2-I-EDIT-13 | automation APIs | required | S4 | stage inferred |
| Z2-I-EDIT-14 | extension APIs | required | S4 | static modules only, Z2-D-024; stage inferred |
| Z2-I-EDIT-15 | authoring-document migration UI: upgrade preview | required | S4 | Z2-D-048; complete coverage in S4 |
| Z2-I-EDIT-16 | authoring-document migration UI: backup | required | S4 | Z2-D-048 |
| Z2-I-EDIT-17 | authoring-document migration UI: resume | required | S4 | Z2-D-048 |
| Z2-I-EDIT-18 | usability acceptance walkthrough with time target: build a scene | required | S4 | scripted task walkthrough; stage inferred |
| Z2-I-EDIT-19 | usability acceptance walkthrough with time target: author a prefab variant | required | S4 | stage inferred |
| Z2-I-EDIT-20 | usability acceptance walkthrough with time target: iterate a script | required | S4 | stage inferred |
| Z2-I-EDIT-21 | usability acceptance walkthrough with time target: reimport an asset | required | S4 | stage inferred |

### DEVT Developer tools & quality

The on-screen profiler, Tracy, the libgraph and complexity gates, profiling, remote Android diagnostics, crash dumps, breadcrumbs, logging, validation, fuzzing, static analysis and dashboards.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-DEVT-01 | in-game on-screen profiler | required | S2 | Z2-D-005; core in S2 |
| Z2-I-DEVT-02 | in-editor on-screen profiler | required | S3 | Z2-D-005; with the minimal editor; stage inferred |
| Z2-I-DEVT-03 | profiler view: CPU frame-graph timeline | required | S2 | Z2-D-005 |
| Z2-I-DEVT-04 | profiler view: per-pass GPU timings | required | S2 | Z2-D-005; as soon as the render graph lands |
| Z2-I-DEVT-05 | profiler view: frame-time history and hitches | required | S2 | Z2-D-005 |
| Z2-I-DEVT-06 | profiler view: memory including aliasing savings | required | S2 | Z2-D-005, Z2-D-057 |
| Z2-I-DEVT-07 | profiler view: counters | required | S2 | Z2-D-005 |
| Z2-I-DEVT-08 | profiler view: Luau time and GC | required | S3 | Z2-D-005; Luau gameplay lands S3; stage inferred |
| Z2-I-DEVT-09 | on-screen profiler on Windows and the S24 | required | S3 | S24 frame in S3; stage inferred |
| Z2-I-DEVT-10 | on-screen profiler runtime toggle | required | S2 | Z2-D-005 |
| Z2-I-DEVT-11 | Tracy integration | required | S2 | stage inferred |
| Z2-I-DEVT-12 | libgraph gate in CI | required | S0 | Z2-D-007 |
| Z2-I-DEVT-13 | complexity gate in CI | required | S0 | Z2-D-007 |
| Z2-I-DEVT-14 | CPU profiling | required | S2 | stage inferred |
| Z2-I-DEVT-15 | GPU profiling | required | S2 | stage inferred |
| Z2-I-DEVT-16 | memory profiling | required | S2 | stage inferred |
| Z2-I-DEVT-17 | asset profiling | required | S4 | stage inferred |
| Z2-I-DEVT-18 | remote Android diagnostics | required | S3 | stage inferred |
| Z2-I-DEVT-19 | crash dumps | required | S3 | D-SEL-CRASH (open, Stage 2); stage inferred |
| Z2-I-DEVT-20 | crash symbols | required | S3 | symbol retention is SHIP; stage inferred |
| Z2-I-DEVT-21 | GPU fault breadcrumbs | required | S3 | device-loss breadcrumbs; stage inferred |
| Z2-I-DEVT-22 | structured logging | required | S2 | stage inferred |
| Z2-I-DEVT-23 | content validation | required | S4 | stage inferred |
| Z2-I-DEVT-24 | fuzzing | required | S2 | win-asan + libFuzzer, Z2-D-011; stage inferred |
| Z2-I-DEVT-25 | static analysis | required | S2 | clang-tidy in acceptance builds; stage inferred |
| Z2-I-DEVT-26 | regression dashboards | required | S3 | mid-sprint integration check results; stage inferred |

### SHIP Shipping & maintenance

Reproducible builds, the CI and device matrix, packaging and signing, release manifests, symbols, the dependency and licence inventory, compatibility policy and release documentation.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-SHIP-01 | reproducible builds | required | S2 | manifest and setup script in S0, Z2-D-008 |
| Z2-I-SHIP-02 | CI matrix | required | S0 | `zenith2-gates.yml`, Z2-D-031 |
| Z2-I-SHIP-03 | device matrix | required | S6 | S24 validated only, Z2-D-010 |
| Z2-I-SHIP-04 | packaging | required | S6 | packaged Windows and S24 builds |
| Z2-I-SHIP-05 | signing | required | S6 | signed APKs |
| Z2-I-SHIP-06 | release manifests | required | S6 |  |
| Z2-I-SHIP-07 | symbol retention | required | S6 |  |
| Z2-I-SHIP-08 | dependency inventory | required | S6 | `ThirdParty/manifest.json`, Z2-D-082 |
| Z2-I-SHIP-09 | licence inventory | required | S6 |  |
| Z2-I-SHIP-10 | dependency updates | required | S6 |  |
| Z2-I-SHIP-11 | compatibility policy | required | S6 | demonstrated with migration fixtures, Z2-D-048 |
| Z2-I-SHIP-12 | documentation | required | S6 | release docs |
| Z2-I-SHIP-13 | upgrade guides | required | S6 |  |

### ADVGFX Advanced GFX

The classification of advanced rendering programmes: ray tracing, virtualized geometry, virtual texturing, hair/groom and the path-traced reference.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-ADVGFX-01 | RT | deferred | S7 | hardware ray tracing; Z2-D-043 |
| Z2-I-ADVGFX-02 | virtualized geometry | deferred | S7 | Z2-D-043 |
| Z2-I-ADVGFX-03 | virtual texturing | required | S4 | Z2-D-043 |
| Z2-I-ADVGFX-04 | hair/groom | excluded | — | Z2-D-043, Z2-D-078; hair alternatives classified in CHAR |
| Z2-I-ADVGFX-05 | path-traced reference | required | S5 | Z2-D-037, Z2-D-043 |

### NET Multiplayer

Multiplayer foundations (network IDs, authority, fixed ticks, commands, headless execution, loopback proof) now, full networking deferred, local multiplayer excluded.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-NET-01 | stable network IDs | required | S1 | Z2-D-073; architecture S1, proven by the S3 loopback |
| Z2-I-NET-02 | authority model | required | S1 | Z2-D-073 |
| Z2-I-NET-03 | fixed ticks | required | S1 | Z2-D-073; shared with JOBS fixed steps |
| Z2-I-NET-04 | replayable commands | required | S1 | Z2-D-073; shared with SAVE input recording |
| Z2-I-NET-05 | headless execution | required | S1 | Z2-D-073; runtime without Flux, Audio or windowing |
| Z2-I-NET-06 | loopback proof | required | S3 | Z2-D-073; two-process transform and command replication |
| Z2-I-NET-07 | transport/session | deferred | S7 | Z2-D-073; full requirement tree ticketed now |
| Z2-I-NET-08 | replication | deferred | S7 | Z2-D-073 |
| Z2-I-NET-09 | prediction/reconciliation | deferred | S7 | Z2-D-073 |
| Z2-I-NET-10 | interest management | deferred | S7 | Z2-D-073 |
| Z2-I-NET-11 | serialization | deferred | S7 | network serialization; Z2-D-073 |
| Z2-I-NET-12 | dedicated server builds | deferred | S7 | Z2-D-073 |
| Z2-I-NET-13 | network simulation | deferred | S7 | Z2-D-073 |
| Z2-I-NET-14 | diagnostics | deferred | S7 | network diagnostics; Z2-D-073 |
| Z2-I-NET-15 | local multiplayer | excluded | — | Z2-D-076 |
| Z2-I-NET-16 | split-screen | excluded | — | Z2-D-076 |

### MOD Engine modules & projects

The in-repo game model: zenith_add_game, game.json, static optional modules with composition-root registration, and project creation without C++.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-MOD-01 | in-repo games under `Zenith2/Games/` statically linking the engine | required | S2 | Z2-D-002; model specified in S1 |
| Z2-I-MOD-02 | `zenith_add_game` | required | S2 | Z2-D-002 |
| Z2-I-MOD-03 | `game.json`: module selection | required | S2 | Z2-D-002, Z2-D-024 |
| Z2-I-MOD-04 | `game.json`: backend selection | required | S2 | Z2-D-002 |
| Z2-I-MOD-05 | `game.json`: platform selection | required | S2 | Z2-D-002 |
| Z2-I-MOD-06 | static optional modules | required | S2 | Z2-D-024 |
| Z2-I-MOD-07 | composition-root registration of modules | required | S2 | Z2-D-024, Z2-D-081 |
| Z2-I-MOD-08 | project creation CLI with no C++ | required | S2 | `Tools/NewGame`; Z2-D-002; stage inferred |
| Z2-I-MOD-09 | project creation wizard with no C++ | required | S4 | an editor wizard; Z2-D-002; editor workflows; stage inferred |
| Z2-I-MOD-10 | dynamic plugins | excluded | — | Z2-D-024 |
| Z2-I-MOD-11 | installed SDK | excluded | — | Z2-D-002 |
| Z2-I-MOD-12 | out-of-repo projects | excluded | — | Z2-D-002 |
| Z2-I-MOD-13 | third-party modding | excluded | — | Z2-D-075 |

### GRAPH Graph core & CPU frame graph

The shared ZenithGraph core and the CPU frame graph built on it, with a conformance suite shared with Flux.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-GRAPH-01 | `ZenithGraph`: access declarations | required | S2 | Z2-D-006; specified in S1 |
| Z2-I-GRAPH-02 | `ZenithGraph`: hazard edges | required | S2 | Z2-D-006 |
| Z2-I-GRAPH-03 | `ZenithGraph`: versioned handles | required | S2 |  |
| Z2-I-GRAPH-04 | `ZenithGraph`: topological sort with critical-path priority | required | S2 |  |
| Z2-I-GRAPH-05 | `ZenithGraph`: cycle diagnostics | required | S2 |  |
| Z2-I-GRAPH-06 | `ZenithGraph`: lifetime intervals | required | S2 | GPU aliasing and CPU frame-arena reuse |
| Z2-I-GRAPH-07 | `ZenithGraph`: JSON dump | required | S2 | for the on-screen profiler and graph viewer |
| Z2-I-GRAPH-08 | CPU frame graph: compiled schedule | required | S2 | Z2-D-006 |
| Z2-I-GRAPH-09 | CPU frame graph: dynamic sub-jobs | required | S2 | Z2-D-006 |
| Z2-I-GRAPH-10 | CPU frame graph: continuations with no fibers | required | S2 | Z2-D-006 |
| Z2-I-GRAPH-11 | CPU frame graph: affinity | required | S2 | Z2-D-006 |
| Z2-I-GRAPH-12 | CPU frame graph: render-extraction pipelining | required | S2 | Z2-D-050 |
| Z2-I-GRAPH-13 | CPU frame graph: deterministic ordering | required | S2 | Z2-D-019 |
| Z2-I-GRAPH-14 | a graph conformance suite shared with Flux | required | S2 |  |

### COMPRESS Compression

The neutral Zenith_Compression API with zlib and zstd backends, streaming and one-shot modes, deterministic output, codec IDs, conformance and benchmarks.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-COMPRESS-01 | the neutral `Zenith_Compression` API | required | S2 | Z2-D-004 |
| Z2-I-COMPRESS-02 | zlib backend | required | S2 | Z2-D-004 |
| Z2-I-COMPRESS-03 | zstd backend | required | S2 | Z2-D-004 |
| Z2-I-COMPRESS-04 | streaming mode | required | S2 |  |
| Z2-I-COMPRESS-05 | one-shot mode | required | S2 |  |
| Z2-I-COMPRESS-06 | deterministic output | required | S2 | per codec |
| Z2-I-COMPRESS-07 | codec IDs in cooked headers | required | S2 |  |
| Z2-I-COMPRESS-08 | a compression conformance suite | required | S2 |  |
| Z2-I-COMPRESS-09 | compression benchmarks | required | S2 |  |

### SCRIPT Scripting

Luau integration: compilation, modules, generated bindings, debugging, sandboxing, transactions, stateless systems, behaviour state and recovery, hot reload, VMs, budgets, profiling, the exception firewall and bytecode compatibility.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-SCRIPT-01 | Luau compilation | required | S3 | Luau gameplay; lifetime specs in S1 |
| Z2-I-SCRIPT-02 | Luau bytecode | required | S3 |  |
| Z2-I-SCRIPT-03 | module loading | required | S3 |  |
| Z2-I-SCRIPT-04 | generated bindings | required | S3 |  |
| Z2-I-SCRIPT-05 | generated `.d.luau` | required | S3 | data-defined schema `.d.luau` lands S2 in ECS |
| Z2-I-SCRIPT-06 | the DAP debugger | required | S3 | client is D-LUAUDBG (open, Stage 1) |
| Z2-I-SCRIPT-07 | sandboxing | required | S3 |  |
| Z2-I-SCRIPT-08 | per-invocation transactions | required | S3 | Z2-D-016, Z2-D-051 |
| Z2-I-SCRIPT-09 | the stateless-system enforcement (D-LUAU-STATELESS) | required | S3 | D-LUAU-STATELESS (open, Stage 1); prototype in S1 |
| Z2-I-SCRIPT-10 | the explicit behaviour-state model | required | S3 | Z2-D-023 |
| Z2-I-SCRIPT-11 | `OnRecover` | required | S3 | Z2-D-023 |
| Z2-I-SCRIPT-12 | quarantine | required | S3 | Z2-D-023 |
| Z2-I-SCRIPT-13 | last-good restore | required | S3 | Z2-D-023 |
| Z2-I-SCRIPT-14 | hot reload | required | S3 |  |
| Z2-I-SCRIPT-15 | migration | required | S3 | hot-reload state migration, with rollback on error |
| Z2-I-SCRIPT-16 | per-worker VMs | required | S3 | Z2-D-018 |
| Z2-I-SCRIPT-17 | deterministic ordering rules | required | S3 | Z2-D-019 |
| Z2-I-SCRIPT-18 | script GC budgets | required | S3 | per-world GC step budgets |
| Z2-I-SCRIPT-19 | script CPU budgets | required | S3 | safepoint count, Z2-D-052 |
| Z2-I-SCRIPT-20 | script memory budgets | required | S3 | caps through the allocator |
| Z2-I-SCRIPT-21 | script profiling | required | S3 |  |
| Z2-I-SCRIPT-22 | the exception firewall | required | S3 | Z2-D-017; Luau linkage verified in S1 |
| Z2-I-SCRIPT-23 | shipping bytecode version compatibility | required | S6 | stage inferred |

### TWOD 2D

First-class 2D: sprites and batching, atlases, tilemaps, 2D animation and physics, pixel-perfect rendering, 2D authoring and the UI2D sample.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-TWOD-01 | sprites | required | S3 | core 2D module; Z2-D-078 |
| Z2-I-TWOD-02 | sprite batching through the render graph | required | S3 |  |
| Z2-I-TWOD-03 | atlases | required | S3 |  |
| Z2-I-TWOD-04 | tilemaps | required | S4 | 2D production features; stage inferred |
| Z2-I-TWOD-05 | 2D animation | required | S4 | stage inferred |
| Z2-I-TWOD-06 | 2D physics (selection ticket D-SEL-2DPHYS) | required | S3 | D-SEL-2DPHYS (open, Stage 2); stage inferred |
| Z2-I-TWOD-07 | pixel-perfect cameras | required | S3 | stage inferred |
| Z2-I-TWOD-08 | pixel-perfect rendering | required | S3 | stage inferred |
| Z2-I-TWOD-09 | 2D authoring in the editor | required | S4 | stage inferred |
| Z2-I-TWOD-10 | the UI2D sample | required | S4 | W-UI2D workload, Game2D; stage inferred |

### CONTENT Content production

The supported-DCC matrix, coordinate and unit conventions, import presets, override-preserving reimport, recoverable batch operations, reference repair, importer isolation, shared DDC, semantic merges, distributed cooking and provenance.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-CONTENT-01 | a supported-DCC feature matrix | required | S3 | Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-02 | DCC matrix: glTF 2.0 extensions | required | S3 | fastgltf; Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-03 | DCC matrix: FBX via a conversion path if selected | required | S4 | D-SEL-FBX (open, Stage 4) |
| Z2-I-CONTENT-04 | DCC matrix per-feature support: skins | required | S3 | Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-05 | DCC matrix per-feature support: morphs | required | S4 |  |
| Z2-I-CONTENT-06 | DCC matrix per-feature support: animation | required | S3 | Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-07 | DCC matrix per-feature support: materials | required | S3 | Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-08 | DCC matrix per-feature support: cameras | required | S4 |  |
| Z2-I-CONTENT-09 | DCC matrix per-feature support: lights | required | S4 |  |
| Z2-I-CONTENT-10 | coordinate conventions: axis conversion on import | required | S4 |  |
| Z2-I-CONTENT-11 | coordinate conventions: handedness conversion on import | required | S4 |  |
| Z2-I-CONTENT-12 | unit conventions: unit conversion on import | required | S4 |  |
| Z2-I-CONTENT-13 | coordinate and unit conventions tested per DCC | required | S4 |  |
| Z2-I-CONTENT-14 | import presets | required | S4 |  |
| Z2-I-CONTENT-15 | reimport that preserves authored overrides | required | S3 | Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-16 | recoverable batch operations: journaled | required | S4 |  |
| Z2-I-CONTENT-17 | recoverable batch operations: resumable | required | S4 |  |
| Z2-I-CONTENT-18 | recoverable batch operations: with a report | required | S4 |  |
| Z2-I-CONTENT-19 | asset rename with reference repair | required | S4 |  |
| Z2-I-CONTENT-20 | asset move with reference repair | required | S4 |  |
| Z2-I-CONTENT-21 | asset delete with reference repair | required | S4 |  |
| Z2-I-CONTENT-22 | importer crash isolation (out-of-process importers) | required | S3 | Stage 3 workloads (W-Characters, W-Outdoor) import real content and Stage 3 acceptance requires reimport and corrupt-content evidence; the item's later parts are Stage 4 requirements (moved from S4 by Z2-D-132) |
| Z2-I-CONTENT-23 | shared DDC operation | required | S4 |  |
| Z2-I-CONTENT-24 | shared DDC failure mode: an unreachable cache falls back to a local cook and never corrupts it | required | S4 |  |
| Z2-I-CONTENT-25 | shared DDC failure mode: a corrupt cache falls back to a local cook and never corrupts it | required | S4 |  |
| Z2-I-CONTENT-26 | shared DDC failure mode: a stale cache falls back to a local cook and never corrupts it | required | S4 |  |
| Z2-I-CONTENT-27 | semantic merges: a structure-aware 3-way merge driver for scenes, prefabs and schemas | required | S4 |  |
| Z2-I-CONTENT-28 | semantic merges depend on canonical serialization (stable key order, one ID-keyed child per line) | required | S2 | a Stage 2 SCENE requirement |
| Z2-I-CONTENT-29 | semantic merges: nested-prefab overrides and array-ordering conflicts fall back to an explicit textual-conflict marker | required | S4 |  |
| Z2-I-CONTENT-30 | distributed cooking | required | S4 |  |
| Z2-I-CONTENT-31 | content provenance | required | S4 |  |

### WORLD World authoring

Editable world partitions, gameplay and data layers, concurrent editing, reproducible procedural generation and baking workflows, kept separate from runtime streaming.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-WORLD-01 | editable world partitions | required | S4 |  |
| Z2-I-WORLD-02 | gameplay layers | required | S4 |  |
| Z2-I-WORLD-03 | data layers | required | S4 |  |
| Z2-I-WORLD-04 | concurrent editing granularity | required | S4 |  |
| Z2-I-WORLD-05 | reproducible procedural generation | required | S4 |  |
| Z2-I-WORLD-06 | baking workflow: lighting | required | S4 | baked GI itself is S5 in LIGHT |
| Z2-I-WORLD-07 | baking workflow: navigation | required | S4 | D-SEL-NAV (open, Stage 1/2) |
| Z2-I-WORLD-08 | baking workflow: HLOD | required | S4 |  |
| Z2-I-WORLD-09 | authoring contracts separate from runtime streaming | required | S4 |  |

### RPROD Rendering production

The production renderer features: HiZ culling, indirect draw compaction, render views, automatic barriers, transient aliasing, offline shaders and permutation limits, PSO precaching, frame pacing, GPU timestamps, quality tiers, every path's full frame and the universal GPU rules.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-RPROD-01 | two-phase HiZ occlusion culling at instance level | required | S3 | Z2-D-060; RPROD core; expanded from two-phase HiZ occlusion culling, `Zenith2/Docs/Rendering.md → "Hierarchical-Z occlusion culling"` |
| Z2-I-RPROD-02 | HiZ pyramid: linear view-space depth in RG16F min/max, built in one single-pass-downsampler dispatch | required | S3 | Z2-D-060; `Z2-B-RPROD-007` HiZ pyramid build GPU |
| Z2-I-RPROD-03 | HiZ occlusion test: projected rectangle and nearest depth against the farthest pyramid depth at the 2×2 mip; near-plane and degenerate cases kept | required | S3 | Z2-D-060 |
| Z2-I-RPROD-04 | HiZ for shadow views against a cascade-depth pyramid where the capability tier enables it | required | S3 | Z2-D-060; stage inferred |
| Z2-I-RPROD-05 | HiZ diagnostics: per-view per-phase counts, pyramid and culled-bounds overlay, projection and mip-selection unit tests, a zero-draw occlusion render test | required | S3 | Z2-D-060 |
| Z2-I-RPROD-06 | two-phase HiZ occlusion culling at meshlet level | required | S5 | visibility-buffer path |
| Z2-I-RPROD-07 | GPU-driven indirect draw compaction | required | S3 | RPROD core |
| Z2-I-RPROD-08 | render views: view identity, per-view resources and independent histories | required | S2 | Z2-D-059; basic view identity; expanded from render views, `Zenith2/Docs/Rendering.md → "Render views"` |
| Z2-I-RPROD-09 | render views: advanced multi-view (many simultaneous captures and probes, stereo-ready paths) | required | S5 | Z2-D-059; RPROD completion |
| Z2-I-RPROD-10 | automatic barrier synthesis | required | S2 | Z2-D-056; render graph |
| Z2-I-RPROD-11 | transient aliasing | required | S2 | Z2-D-057 |
| Z2-I-RPROD-12 | transient aliasing runtime toggle | required | S2 | Z2-D-057 |
| Z2-I-RPROD-13 | offline-only shader compilation | required | S2 | Z2-D-025; shader system |
| Z2-I-RPROD-14 | enumerated shader permutations | required | S2 | Z2-D-025 |
| Z2-I-RPROD-15 | shader-permutation limits | required | S3 | RPROD core; `Z2-B-RPROD-024` shader permutation limit per game |
| Z2-I-RPROD-16 | PSO precaching | required | S3 | RPROD core; Z2-D-025 |
| Z2-I-RPROD-17 | the missing-PSO fallback | required | S3 | RPROD core; Z2-D-025; `Z2-B-RPROD-025` missing-PSO events per qualification run |
| Z2-I-RPROD-18 | frame pacing | required | S3 | RPROD core; `Z2-B-RPROD-020` frame pacing present-interval deviation |
| Z2-I-RPROD-19 | per-pass GPU timestamps | required | S2 | inserted by the render graph, Z2-D-005 |
| Z2-I-RPROD-20 | per-platform quality tiers | required | S5 | RPROD completion |
| Z2-I-RPROD-21 | the full frame for Forward+ | required | S3 | Z2-D-035; expanded from the full frame for every path, `Zenith2/Docs/Rendering.md → "Frame anatomy Forward+"` |
| Z2-I-RPROD-22 | the full frame for Forward | required | S5 | Z2-D-035; `Zenith2/Docs/Rendering.md → "Frame anatomy Forward"` |
| Z2-I-RPROD-23 | the full frame for Deferred | required | S5 | Z2-D-035; `Zenith2/Docs/Rendering.md → "Frame anatomy Deferred"` |
| Z2-I-RPROD-24 | the full frame for Visibility buffer | required | S5 | Z2-D-035; `Zenith2/Docs/Rendering.md → "Frame anatomy Visibility buffer"` |
| Z2-I-RPROD-25 | GPU performance rule: no empty render passes, merge similar passes (cull, fold clears, merge only barrier-free compatible passes) | required | S2 | Z2-D-014; expanded from the universal GPU performance rules, `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"`; graph unit tests; stage inferred |
| Z2-I-RPROD-26 | GPU performance rule: keep early-Z, with the `ShaderChecks` discard/clip and `SV_Depth` lint | required | S3 | Z2-D-014; `ShaderChecks` in `Zenith2Render`; stage inferred |
| Z2-I-RPROD-27 | GPU performance rule: D24S8 by default, D32F only with a precision justification or the capability fallback | required | S2 | Z2-D-014, Z2-D-061; graph compile check; stage inferred |
| Z2-I-RPROD-28 | GPU performance rule: no full 32-bit render targets unless on the justified list | required | S2 | Z2-D-014; graph compile check; stage inferred |
| Z2-I-RPROD-29 | GPU performance rule: no unnecessarily bound attachments (declared-shader-set check, record-time development check, write mask 0) | required | S3 | Z2-D-014; stage inferred |
| Z2-I-RPROD-30 | GPU performance rule: half precision with native 16-bit types | required | S3 | Z2-D-014; `ShaderChecks` float/half ratio report; stage inferred |
| Z2-I-RPROD-31 | GPU performance rule: back-face culling for opaque, with the PSO two-sided lint | required | S3 | Z2-D-014; stage inferred |
| Z2-I-RPROD-32 | GPU performance rule: compressed vertex data, 16-bit indices and a position stream | required | S2 | Z2-D-014, Z2-D-062; cook checks and generated layout tables; the format items are Z2-I-GEOM-01 to Z2-I-GEOM-08 |

### CHAR Character production

Rig and control authoring, pose debugging, animation budgeting, modular characters, animation/physics authority, facial and lip-sync workflows, motion-matching and hair decisions, and the complete character workflow.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-CHAR-01 | rig authoring | required | S4 | character production |
| Z2-I-CHAR-02 | control authoring | required | S4 |  |
| Z2-I-CHAR-03 | pose debugging | required | S4 |  |
| Z2-I-CHAR-04 | animation update budgeting | required | S4 | significance-based update-rate reduction; an implementation ticket, Z2-D-012 |
| Z2-I-CHAR-05 | modular characters | required | S4 |  |
| Z2-I-CHAR-06 | animation/physics authority | required | S3 | arbitration per Z2-D-080; stage inferred |
| Z2-I-CHAR-07 | facial animation workflow | required | S4 |  |
| Z2-I-CHAR-08 | lip-sync workflow | required | S4 |  |
| Z2-I-CHAR-09 | decision on motion matching (D-SEL-MOTIONMATCH) | required | S3 | D-SEL-MOTIONMATCH (open, Stage 3) |
| Z2-I-CHAR-10 | decision on hair alternatives: hair excluded; cards/shells classified | required | S4 | hair/groom excluded, Z2-D-078; cards/shells classification in the CHAR catalogue; stage inferred |
| Z2-I-CHAR-11 | complete character workflow: import | required | S4 |  |
| Z2-I-CHAR-12 | complete character workflow: rig/retarget | required | S4 |  |
| Z2-I-CHAR-13 | complete character workflow: author the animation graph | required | S4 |  |
| Z2-I-CHAR-14 | complete character workflow: preview | required | S4 |  |
| Z2-I-CHAR-15 | complete character workflow: in-game | required | S4 |  |
| Z2-I-CHAR-16 | character workflow usability acceptance as a timed scripted walkthrough | required | S4 |  |

### ONLINE Online services

The Zenith_OnlineServices adapter with an offline implementation, identity, entitlements, achievements, cloud saves, account changes, and the Steam and Google Play backends.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-ONLINE-01 | the `Zenith_OnlineServices` adapter API | required | S4 | Z2-D-074 |
| Z2-I-ONLINE-02 | an offline implementation | required | S4 | Z2-D-074 |
| Z2-I-ONLINE-03 | identity | required | S4 |  |
| Z2-I-ONLINE-04 | entitlements | required | S4 |  |
| Z2-I-ONLINE-05 | achievements | required | S4 |  |
| Z2-I-ONLINE-06 | cloud-save conflict model | required | S4 |  |
| Z2-I-ONLINE-07 | account change mid-session: sign-out | required | S4 |  |
| Z2-I-ONLINE-08 | account change mid-session: switch user | required | S4 |  |
| Z2-I-ONLINE-09 | account change: saves rebind | required | S4 |  |
| Z2-I-ONLINE-10 | account change: entitlements rebind | required | S4 |  |
| Z2-I-ONLINE-11 | account change: no cross-account leakage | required | S4 |  |
| Z2-I-ONLINE-12 | cloud-save conflict resolution flows | required | S4 |  |
| Z2-I-ONLINE-13 | Steam backend | required | S6 | Z2-D-074 |
| Z2-I-ONLINE-14 | Google Play backend | required | S6 | Z2-D-074 |

### SEC Security & robustness

Trust boundaries for scripts, assets, remote tools and updates, parser limits, fuzzing, path-traversal protection, authenticated updates and shipping-build lockdown.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-SEC-01 | trust boundary: scripts | required | S1 | security architecture in S1; hardening evidence S6 |
| Z2-I-SEC-02 | trust boundary: assets | required | S1 |  |
| Z2-I-SEC-03 | trust boundary: remote tools | required | S1 |  |
| Z2-I-SEC-04 | trust boundary: updates | required | S1 |  |
| Z2-I-SEC-05 | parser limits | required | S2 | serialization and basic assets; stage inferred |
| Z2-I-SEC-06 | parser fuzzing | required | S2 | win-asan + libFuzzer; stage inferred |
| Z2-I-SEC-07 | path-traversal protection | required | S2 | virtual filesystem; stage inferred |
| Z2-I-SEC-08 | authenticated update manifests | required | S6 | patching; stage inferred |
| Z2-I-SEC-09 | no debugger exposed in shipping builds | required | S3 | DAP server absent from shipping builds; stage inferred |
| Z2-I-SEC-10 | no remote tools exposed in shipping builds | required | S3 | stage inferred |

### MAINT Engine maintenance

API and version policy, dependency-upgrade rehearsals, deprecated-API removal, subsystem ownership, gate baseline and profile upkeep, and keeping sample games building.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-MAINT-01 | supported API policy | required | S2 | source API deprecation, Z2-D-048; stage inferred |
| Z2-I-MAINT-02 | version policy | required | S2 | stage inferred |
| Z2-I-MAINT-03 | dependency-upgrade rehearsals | required | S6 | dependency updates; stage inferred |
| Z2-I-MAINT-04 | deprecated-API removal | required | S6 | one fix version after deprecation; stage inferred |
| Z2-I-MAINT-05 | subsystem ownership | required | S0 | one area epic per code, Z2-D-049 |
| Z2-I-MAINT-06 | libgraph baseline upkeep | required | S0 | `libgraph_baseline.json`, Z2-D-007 |
| Z2-I-MAINT-07 | complexity-profile upkeep | required | S0 | `complexity_profile.json`, Z2-D-007 |
| Z2-I-MAINT-08 | representative sample games kept building | required | S2 | first minimal game under `Games/` in S2; stage inferred |

### RESEARCH Research

The measurement methodology for the ECS-backend and shading-path comparisons, and its reports as decision records.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-RESEARCH-01 | the research methodology for ECS backends | required | S1 | Z2-D-064 |
| Z2-I-RESEARCH-02 | the research methodology for shading paths | required | S1 | Z2-D-064 |
| Z2-I-RESEARCH-03 | native vs adapter measurement | required | S5 | Z2-D-064; harness from S2 benchmarks; stage inferred |
| Z2-I-RESEARCH-04 | repetition procedure | required | S1 | at least 10 repetitions, medians, 95% CIs |
| Z2-I-RESEARCH-05 | CI procedure for research runs | required | S1 | controlled conditions; stage inferred |
| Z2-I-RESEARCH-06 | raw-data retention | required | S5 | under `Zenith2/Docs/Research/`; stage inferred |
| Z2-I-RESEARCH-07 | conclusions as decisions | required | S5 | the Stage 5 research report; stage inferred |

### SCOPE Platform scope

The recorded scope boundaries: consoles, VR/XR, modding and local multiplayer, each with a decision ID, and the rule that deferred scopes get full requirement trees now.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-SCOPE-01 | consoles (SDK-gated) | deferred | S7 | Z2-D-071 |
| Z2-I-SCOPE-02 | console tickets specified to the level possible without an NDA | required | S1 | an obligation on the Stage 1 SCOPE decision tickets; Z2-D-071 |
| Z2-I-SCOPE-03 | console tickets blocked by a `needs-human` SDK-access ticket per platform | required | S1 | an obligation on the Stage 1 SCOPE decision tickets; the console implementation tickets themselves are S7 (Z2-I-SCOPE-01); Z2-D-071 |
| Z2-I-SCOPE-04 | VR/XR via OpenXR | deferred | S7 | Z2-D-072 |
| Z2-I-SCOPE-05 | XR tickets specified against OpenXR | required | S1 | an obligation on the Stage 1 SCOPE decision tickets; Z2-D-072 |
| Z2-I-SCOPE-06 | multi-view kept XR-ready | required | S2 | Z2-D-072; `VIEW_KIND_STEREO` reserved; stage inferred |
| Z2-I-SCOPE-07 | input kept XR-ready | required | S3 | Z2-D-072; action-based input; stage inferred |
| Z2-I-SCOPE-08 | modding | excluded | — | Z2-D-075 |
| Z2-I-SCOPE-09 | local multiplayer | excluded | — | Z2-D-076 |
| Z2-I-SCOPE-10 | each scope boundary recorded with a decision ID | required | S1 | the SCOPE decision tickets |
| Z2-I-SCOPE-11 | deferred scopes get full requirement trees now | required | S1 | Z2-D-070 |

### PROC Process & gates

Every protected harness, configuration and gate edit, each a needs-human PROC ticket.

| ID | Item | Classification | Stage | Notes |
|---|---|---|---|---|
| Z2-I-PROC-01 | every protected harness edit, as a `needs-human` ticket | required | S0 | Z2-D-128; PROC tickets in Stages 2, 3 and 5 add further gate lines; closed by a human actor, Z2-D-103 |
| Z2-I-PROC-02 | every protected config edit, as a `needs-human` ticket | required | S0 | Z2-D-128 |
| Z2-I-PROC-03 | every protected gate edit, as a `needs-human` ticket | required | S0 | Z2-D-128 |

## Change rule

- Inventory v1 changes only through a recorded decision — an entry appended to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN` in the same diff (Z2-D-097) — plus a version bump of this file: v1 becomes v2, the version line in "Purpose and versioning" is bumped and the heading "Inventory v1" is renamed to the new version, in the same diff that records the decision; the coverage gate step's stale-cite report then flags every ticket that cites the old heading for revalidation. Requirement coverage (Z2-D-067) and normative traceability (Z2-D-066) both read this file, so an unrecorded edit would silently change what coverage proves.
- A change that challenges a settled scope decision (Z2-D-071 to Z2-D-078, Z2-D-043) is a flagged proposal first (Z2-D-022), recorded in `Zenith2/Docs/Roadmap.md → "Flagged proposals"`; nothing changes until the human approves it.
- IDs are never reused or renumbered. A new item is appended to its area's table with the next free number. A removed item keeps its row, with Classification `excluded`, Stage `—`, and the removing decision's ID in Notes, so every requirement that cites it still resolves.
- Reclassifying an item (for example `required` to `deferred`, or a D-CLASS-SIM (open, Stage 2) outcome) edits its Classification and Stage and names the decision in Notes. The spec-revalidation tickets for the affected (area, stage, decision set) then reconcile the tickets that cover it.
- An open decision's ruling that fixes an item's scope (D-CLASS-SIM (open, Stage 2), D-SEL-MOTIONMATCH (open, Stage 3), D-SEL-PHYSX-ANDROID (open, Stage 1), and D-SEL-EnTT-SHIPPING (flagged proposal P1, Stage 5) if P1 is approved) is a change under this rule.
- The coverage gate step fails when an item is not mapped by a requirement, and when an excluded item lacks a rationale and a decision ID.
<!-- elaboration -->
- Before the docs and config commit (Z2-D-115) no filed ticket cites an inventory stage, so a stage correction ruled during the documentation session is recorded by one session-ruling entry and made in place inside v1, with no version bump and no heading rename; the correction's Notes name the entry. Z2-D-131 is that entry for Z2-I-RHI-21, Z2-I-ASSET-04, -12, -13, Z2-I-ENV-01, -03, -06, -09, -10 and Z2-I-GEOM-19, and Z2-D-132 for Z2-I-CONTENT-01, -02, -04, -06, -07, -15 and -22. From that commit on, every change follows the rule above in full.
