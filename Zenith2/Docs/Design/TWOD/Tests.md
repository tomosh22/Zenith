# TWOD design: test catalogue

Section file of `Zenith2/Docs/Design/TWOD.md`. It names every TWOD test with the case it asserts, its file and target, and the gate step that runs it; the test support code; the render tests; the D-SEL-2DPHYS scenario suite; the integration suite; the fuzz targets; the benchmarks; the device runs.

## Scope of this file

Test names map `Test_<Type>_<Behaviour>` ↔ `"Zenith2D/<Type>/<Behaviour>"` (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`), and every name here is unique programme-wide (TEST-NAME-UNIQUE). Every rule below holds for every TWOD test:

- No test reads a committed binary file (the orchestrator's no-binary-artefact ruling): atlases, images, tilemaps and cooked files are generated in the test from seeds and literal descriptions under `${ZENITH_OUT}/test-fixtures/TWOD/`; the only committed fixtures are the text migration documents of `Zenith2/Docs/Design/TWOD/Formats.md → "Document types and migration"`.
- Expected values are derived in the test from the specification (formulas, reference implementations in test support, oracles computed from the records), never recorded from a run, so no golden file needs a recorded refresh and no ticket asks a worker to record a count or a digest.
- No test doubles a concrete class; the one substituted interface is the physics backend seam, a declared interface: the scenario suite compares the two candidates' production implementations through it, and `Test_TwoDSave_CoherentDowngrade` wraps the winner's production implementation in `Zenith_TwoDNonExactBackend`, which forwards every member and reports `SupportsExactState` false (the recording-implementation form the orchestrator's FINAL-DOUBLES ruling admits).
- GPU tests run on WARP and lavapipe and are not `needs-gpu` (X170); validation layers are on and a validation message fails the test.

## Test targets and files

| Target | Directory | Built in | Gate step |
|---|---|---|---|
| `Zenith2D_Tests` | `Zenith2/Engine/Zenith2D/Tests/` | win-debug, win-release, win-asan, android-arm64, android-x86_64 (Zenith2D is `ZENITH_GPU`) | `Test` (win-debug, win-release); `Sanitizers -Preset win-asan` |
| TWOD's files of `ZenithFlux_Tests` | `Zenith2/Engine/ZenithFlux/Tests/Sprite2D/` | as ZenithFlux_Tests | `Test` on WARP and lavapipe |
| TWOD's files of `ZenithAuthoring_Tests` | `Zenith2/Tools/Authoring/Tests/TwoD/` | win-debug, win-release (`ZENITH_TOOLS_HOST`) | `Test` |
| TWOD's file of `ZenithCook_Tests` | `Zenith2/Tools/ZenithCook/Tests/Zenith_TwoDCook_Tests.cpp` | win-debug, win-release | `Test` |
| TWOD's files of `ZenithEditor_Tests` | `Zenith2/Tools/ZenithEditor/Tests/2D/` | win-debug, win-release | `Test` |
| `Zenith_Integration_TwoD` | `Zenith2/Tests/TwoD/` | win-debug, win-release | `Test` (GPU cases on WARP and lavapipe) |
| `Zenith_Integration_TwoDDeterminism` | `Zenith2/Tests/Determinism/TwoD/` (TWOD's member of the determinism suites of [Z2-N-ARCH-886]; its own target because ECS's `Zenith_Integration_Determinism` links no module) | win-debug, win-release | `Test` |
| `Zenith_Fuzz_TwoDAsset`, `Zenith_Fuzz_TwoDDocument`, `Zenith_Fuzz_TwoDSaveSection` | `Zenith2/Engine/Zenith2D/Tests/Fuzz/`, `Zenith2/Tools/Authoring/Tests/TwoD/Fuzz/` | the fuzz preset | the fuzzing schedule of `Zenith2/Docs/Architecture.md → "Security architecture"` |
| `Zenith_Bench_Physics2DSelection` | `Zenith2/Benchmarks/Physics2DSelection/` | win-debug, win-release, android-arm64 | its registered CTest `Test_Physics2DSelection_HarnessRuns` in `Test` (win-debug, win-release); the measured runs belong to the `needs-human` measurement run ticket |
| `Zenith_Bench_TwoD` | `Zenith2/Benchmarks/TwoD/` | win-release, android-arm64 | the budget runs of "Benchmarks" |

## Test support

All header-only, under `Zenith2/Engine/Zenith2D/Tests/Support/` (X184):

| Header | Provides |
|---|---|
| `Zenith_TwoDTestImages.h` | seeded procedural RGBA8 images: `Zenith_TwoDTestImageRamp(w, h)` (every 8-bit value per channel along x), `Zenith_TwoDTestImageNoise(seed, w, h)`, `Zenith_TwoDTestImageSheet(seed, cellW, cellH, cols, rows)` (cells with distinct colours and a one-texel outline) |
| `Zenith_TwoDTestCookedWriter.h` | writes `.zatlas`, `.zsanim`, `.ztileset`, `.ztilemap`, `.zrig2d` bytes from literal C++ descriptions, field by field per `Zenith2/Docs/Design/TWOD/Formats.md`, independently of ZenithAuthoring's compiler (which `Test_TwoDCompile_MatchesTestWriter` checks against it) |
| `Zenith_TwoDTestWorld.h` | a world with Zenith2D attached on the Custom backend plus `Zenith_TwoDTestWorld::Tick(n)`, built on ECS's test world support. Its constructor takes `const Zenith_TwoDTestWorldDesc&`, whose `Zenith_PfnTwoDTestRegister m_pfnRegister = nullptr` (`using Zenith_PfnTwoDTestRegister = Zenith_Result<void, Zenith_ReflectError> (*)(Zenith_TypeRegistry& xTypes);`) is the test's registration hook: when non-null it runs after `Zenith_Module_2D_RegisterTypes` and before the registry's `Freeze`, and an error result or a failed `Freeze` fails the test with doctest's `REQUIRE`, naming the error code. On the Box2D path the test world starts no Physics module; under the Jolt outcome the J-path ticket switches it to start the Physics module on Jolt with `physics.planarWorld` true (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"`) |
| `Zenith_TwoDReferenceSnap.h` | the CPU reference of `Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"` and "Canvas geometry": the same binary64 operations in the same order (the remainders `rx`, `ry`, never a division by `s`) and the signed `floor_div` composite index, so it agrees with the renderer bit for bit at every rounding boundary; the oracle of the snapping and render tests |
| `Zenith_TwoDReferenceSkeleton.h` | the cutout skeleton evaluation in double precision with explicit matrices, the oracle of `Test_Skeleton2D_PoseMatchesReference` |
| `Zenith_TwoDNonExactBackend.h` | a `Zenith_Physics2DBackend` that owns the selected production backend, forwards every member to it and returns false from `SupportsExactState` (so `SaveState` writes nothing), the only seam implementation outside the production one |
| `Zenith_TwoDReferenceOutline.h` | a brute-force tile outline tracer (per cell edge, then merged), the oracle of `Test_TileCollision_OutlineMatchesReference` |
| `Zenith_TwoDTestForeignBody.h` | Box2D path only: the stand-in for PHYS's 3D body, `struct Zenith_TwoDTestForeignRigidBody { uint8 m_uUnused = 0; static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true; static constexpr Zenith_PropertyID uPROP_UNUSED{ 1 }; }`, and `inline Zenith_Result<void, Zenith_ReflectError> Zenith_TwoDRegisterForeignRigidBody(Zenith_TypeRegistry& xTypes)`, passed as the test world's `m_pfnRegister`, which calls `xTypes.BeginComponent<Zenith_TwoDTestForeignRigidBody>("Zenith.Physics.RigidBody", 1).DefaultAuthority(PHASE_LATE_SCRIPTS).ScriptName("TwoDTestForeignRigidBody").Property(Zenith_TwoDTestForeignRigidBody::uPROP_UNUSED, "Unused", &Zenith_TwoDTestForeignRigidBody::m_uUnused).EndProperty().End()` and returns success (registration errors surface at `Freeze`: the authority satisfies rule 3 and the explicit script name rule 1 of `Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"`, so the stand-in coexists with TWOD's `RigidBody2D`). Under the Jolt outcome the Physics module registers the real `Zenith.Physics.RigidBody` in every physics test world, so the J-path ticket makes the three tests that use the stand-in add PHYS's real component instead and deletes this header in the same diff. Ingest steps 2 and 3 of `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"` test only the presence of a component of that stable name, so the stand-in drives exactly the production branch; it is a reflected data type, not a double of a concrete class or of a seam (FINAL-DOUBLES), and no PHYS module, library or header is linked, so the dual-body tests build and run on the Box2D path |

## Unit tests

### Types components and registration

`Zenith_TwoDTypes_Tests.cpp`, `Zenith_TwoDRegistration_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_TwoDTypes_ErrorCodesStable` | every `TWOD_ERROR_*`, `PHYSICS2D_ERROR_*` and `FLUX_SPRITE2D_ERROR_*` value equals its declared number and `Zenith_ToString` names it |
| `Test_TwoDRegistration_ComponentsAndProperties` | the nine components register with their stable names, script names (`FindTypeByScriptName` of each "Script name" returns that component), property IDs, defaults, ranges and authority phases of `Zenith2/Docs/Design/TWOD.md → "Component table"` (`Zenith.TwoD.Joint` property 20 `WorldAnchor` included) and round-trip through scene serialization |
| `Test_TwoDRegistration_ConfigKeys` | every key of `Zenith2/Docs/Design/TWOD.md → "Configuration keys"` registers with its default, range, layers and simulation-affecting flag |
| `Test_TwoDRegistration_EventsAndPayloads` | the seven events, the five payloads (24, 32, 24, 24 and 32 bytes), the three script value structs, the element struct `Zenith.TwoD.AnimatorParameter2D` and the three world resources register, each with the script name of the table in `Zenith2/Docs/Design/TWOD.md → "World resources events and commands"`; payload sizes equal the `static_assert`ed sizes |
| `Test_TwoDCommands_KindsResolvedAtAttach` | after the first `AttachWorld` the five IDs of `Zenith_TwoDCommandKinds` are non-zero and equal `FindByName` of each name; every recorder given a zero-ID struct records nothing, returns success and counts `twod.command.unresolved_kind_delta` |
| `Test_TwoDSchedule_AccessCompiles` | the module's systems compile into the world schedule with the declared access and no conflict, in the phases of `Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement"`, in a world holding every TWOD component type and the `Zenith_TwoDTestForeignBody.h` stand-in, with no PHYS module, plus a test-registered phase-6 `Zenith.LocalTransform` writer of `WRITER_SOURCE_PHYSICS` beside `TwoD.Physics2DSyncTransforms` (distinct writer sources compile). Planar-world case, Jolt outcome only, added by the J-path ticket after PHYS's TD10 patch (ruling TD10-CONDITIONAL): with the Physics module started on Jolt and `physics.planarWorld` true, `TwoD.Physics2DStep`'s `WriteResource Zenith.Physics.PlanarWorld` compiles beside PHYS's own phase-5 nodes with no conflict, and with `physics.planarWorld` false the attach fails with `MODULE_ERROR_REGISTRATION` naming the missing resource |
| `Test_TwoDModule_StartUnwindsOnFailure` | a failure injected at each of `Zenith_Module_2D_Start`'s steps and each `AttachWorld` step leaves no allocation and no registration behind. World-capacity case (Box2D outcome): the attach fault point of step 1 injects `PHYSICS2D_ERROR_WORLD_CAPACITY` as `b2CreateWorld`'s table-full outcome (no 129 real worlds are created), and the test asserts the attach fails with that code, the world's state is unwound and a following attach without the fault succeeds |

### Formats and loaders

`Zenith_TwoDFormats_Tests.cpp`, `Zenith_TwoDLoaders_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_TwoDFormats_ReaderChecks` | each check of `Zenith2/Docs/Design/TWOD/Formats.md → "Cooked reader checks"` fails a generated file corrupted in the field it covers, with that check's number |
| `Test_TwoDFormats_LimitsAtAndAbove` | every row of "Parser limits" accepted at the limit and refused at limit + 1 |
| `Test_TwoDFormats_TileCellSymmetries` | the eight flip and transpose combinations map (u, v) as `Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding"` states; bits 19–31 read empty |
| `Test_TwoDLoaders_AtlasNameLookup` | `FindFrame` by `Zenith_StringID` returns every frame of a 10,000-frame generated atlas and fails for absent names |
| `Test_TwoDLoaders_TilemapAuthoredCell` | `GetAuthoredCell` returns the written cell for every cell of a generated 4-layer map and empty outside its chunks |
| `Test_TwoDLoaders_FailureIsResult` | each kind loader returns `TWOD_ERROR_CORRUPT_ASSET` or `TWOD_ERROR_VERSION_MISMATCH` and never asserts |

### Sprites sorting and extraction

`Zenith_Sprite2DExtract_Tests.cpp` (Zenith2D_Tests) and `Zenith_Sprite2DSort_Tests.cpp`, `Zenith_Sprite2DBatch_Tests.cpp` (ZenithFlux_Tests, CPU only):

| Test | Asserts |
|---|---|
| `Test_Sprite2DRecord_Layout` | `Flux_Sprite2DRecord` is 72 bytes and `Flux_Sprite2DTilemapDraw` 72 with the declared offsets |
| `Test_Sprite2DExtract_OriginsCameraRelative` | one scene extracted twice, once as authored around a camera at (0, 0) m and once with every entity and the camera offset by (10,000, −10,000) m, stores bit-identical camera-relative origins. Fixture rule: every sprite position, pivot offset and camera coordinate is a multiple of 2⁻¹⁰ m with magnitude below 16,384 m after the offset, so it is exact in binary32 at both magnitudes (the binary32 spacing in [8,192, 16,384) m is 2⁻¹⁰ m; an ordinary decimal such as 0.3 m is not: (float)10,000.3 − 10,000 = 0.2998046875 while (float)0.3 = 0.300000012) |
| `Test_Sprite2DExtract_RecordsMatchComponents` | each sprite's record carries its interpolated world transform, frame rectangle, pivot, tint, flips, layer and order |
| `Test_Sprite2DExtract_SlotFullDropsWholeList` | a list that does not fit the slot is refused whole (`twod.extract.slot_full_delta`) |
| `Test_TilemapExtract_ChunkRecordShape` | chunk records hold exactly the changed chunks with their revision, in the order of `Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"` |
| `Test_Sprite2DSort_KeyFields` | each field of the 64-bit key orders as specified, including the y key and the ID tie-break; two sprites 0.002 m apart in y keep their order while the camera moves in 10⁻⁴ m steps |
| `Test_Sprite2DSort_IndependentOfChunkLayout` | identical content in 1, 7 and 64 chunks sorts to identical record orders |
| `Test_Sprite2DBatch_TextureChangeKeepsBatch` | a page change does not split a batch |
| `Test_Sprite2DBatch_BlendChangeSplits` | a blend or program change splits, and nothing else does |
| `Test_Sprite2DRenderer_WritesAfterReservation` | through the real upload ring, `PrepareFrame` declares and `WriteFrame` writes at the reserved offset; a deferred frame draws no 2D content |
| `Test_SpriteStreamSet_LayoutsMatchTable` | the three SPRITE stream-sets match `Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite stream-sets"` |

### Canvas and snapping

`Zenith_Sprite2DCanvasGeometry_Tests.cpp` (ZenithFlux_Tests, CPU):

| Test | Asserts |
|---|---|
| `Test_Sprite2DCanvasGeometry_Rules` | rules 1–6 of "Canvas geometry" for outputs 320 × 180 … 7,680 × 4,320 in each scale mode, against `Zenith_TwoDReferenceSnap.h` |
| `Test_Sprite2DSnap_TexelCameraRounds` | TEXEL snap rounds the camera to whole reference pixels |
| `Test_Sprite2DSnap_SpriteCornerRounds` | a `PixelSnap` sprite's corner lands on whole reference pixels, including corners at the ±0.5 boundary at PPU 16, 24 and 100 |
| `Test_Sprite2DSnap_SubpixelOffsetsAndMargin` | SUBPIXEL moves the composite offset by the camera's fraction and the canvas keeps its 1-pixel margin |
| `Test_Sprite2DSnap_FarFromOrigin` | the same snapped canvas positions, bit for bit, for the scene at the origin and the same scene and camera offset by (±10,000, ±10,000) m (the camera-relative rule), with every position, pivot offset and camera coordinate a multiple of 2⁻¹⁰ m (the fixture rule of `Test_Sprite2DExtract_OriginsCameraRelative`) |
| `Test_Sprite2DComposite_SubpixelMarginRows` | with `oy = k − 1` at k = 6 the composite's first `oy` output rows read canvas row 0 and its last column reads the right margin; no index leaves the canvas |
| `Test_Camera2D_ReferenceWorldRoundTrip` | `Zenith_Camera2D_WorldToReference` and `ReferenceToWorld` invert each other within 1 × 10^-9 m |

### Animation and skeletons

`Zenith_Sprite2DFlipbook_Tests.cpp`, `Zenith_SpriteAnimator_Tests.cpp`, `Zenith_Skeleton2D_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_Sprite2DFlipbook_FrameFormula` | loop, once and ping-pong frame selection at every frame boundary ± 1 tick |
| `Test_Sprite2DFlipbook_PausedWritesNothing` | a paused animator writes no sprite change |
| `Test_Sprite2DFlipbook_SpeedZeroHolds` | speed 0 holds the frame and emits no event |
| `Test_SpriteAnimator_Transitions` | each condition operator and exit time fires as specified |
| `Test_SpriteAnimator_AnyBeforeOwn` | ANY-state transitions are evaluated before the state's own |
| `Test_SpriteAnimator_OneTransitionPerTick` | at most one transition per tick |
| `Test_SpriteAnimator_TriggerConsumedOnlyByFiringTransition` | a trigger survives until a transition using it fires |
| `Test_SpriteAnimator_CommandsResolveAtPlayback` | commands recorded while the set is not resident, resident, and with an unknown name or a kind mismatch: the first two apply identically once resolved at playback, the others drop with `twod.anim.command_dropped_delta` |
| `Test_Animation2DEvents_OrderAndWrap` | events fire once per crossing in order, including across a loop wrap and a multi-frame tick |
| `Test_Animation2DEvents_IndependentOfWorkers` | the event stream is identical with 1, 2 and 8 workers |
| `Test_SpriteAnimationHotReload_RemapsByName` | a reloaded animation keeps the state by name and restarts a removed state at the default |
| `Test_Skeleton2D_PoseMatchesReference` | bone matrices equal the reference within 1 × 10^-5 m and 1 × 10^-6 rad for 1,000 seeded poses |
| `Test_Skeleton2D_SlotDrawOrderTrack` | the draw-order track reorders slots and the extractor emits them in the new order |

### Tilemaps

`Zenith_TilemapRuntime_Tests.cpp`, `Zenith_TileCollision_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_TilemapRuntime_EffectiveCell` | overlay over authored, empty outside bounds |
| `Test_TilemapRuntime_OverlayCapacity` | edits beyond `twod.tilemap.maxEditedChunks` are dropped deterministically with the counter |
| `Test_TileEditPlayback_MergeOrder` | SetTile and FillTiles from several behaviours apply in merge order, independently of residency |
| `Test_TilemapHotReload_KeepsEdits` | a reloaded map keeps its overlay |
| `Test_TileCollision_OutlineMatchesReference` | the marching-squares chains equal the reference outline for 200 seeded maps |
| `Test_TileCollision_NoBumpAcrossChunks` | a box sliding across chunk seams at 1 … 20 m/s never gains upward velocity (ghost vertices) |
| `Test_TileCollision_MovedMapFollows` | a moved map carries a resting body, keeps its chain shape IDs and reports the cell under a ray hit on a merged segment |
| `Test_TilemapDraw_ParallaxFormula` | a layer at parallax (0.5, 0) draws its chunk instances at the formula's positions under a moving camera; it generates no collision |

### Physics

`Zenith_Physics2D_Tests.cpp`, `Zenith_Physics2DEvents_Tests.cpp`, `Zenith_Physics2DQuery_Tests.cpp` (on the selected backend):

| Test | Asserts |
|---|---|
| `Test_Physics2DSync_WritesOnlyAwakeBodies` | sleeping bodies' transforms are not written |
| `Test_Physics2DSync_KeepsZAndScale` | z and scale survive sync |
| `Test_Physics2DSync_ParentedBody` | a parented body's local transform equals `inverse(W_parent) · W_body` |
| `Test_Physics2DIngest_DualBodySkipped` | an entity with both physics bodies gets no 2D body and counts `twod.physics.dual_body_delta`; the 3D body is the `Zenith_TwoDTestForeignBody.h` stand-in, so the test needs no PHYS module |
| `Test_Physics2DIngest_LaterPhysicsBodyRemoves2DBody` | adding `Zenith.Physics.RigidBody` (the stand-in) to an entity with a 2D body removes the 2D body at the next tick with the counter |
| `Test_Physics2DExternal_SevenFloatCompare` | a rotation differing only in x is adopted once; a bit-equal rewrite of all seven floats is not |
| `Test_Physics2DCommands_PendingOrder` | impulses, forces and velocity sets apply in list order; a full list drops with the counter |
| `Test_Physics2DEvents_SortedByKey` | contact and trigger events are sorted by record key |
| `Test_Physics2DEvents_IndependentOfWorkers` | identical event streams with 1, 2 and 8 workers |
| `Test_Physics2DEvents_TriggerReach` | a sensor overlapping a dynamic, a kinematic, a static body, a tile chain and another sensor emits exactly two enter events |
| `Test_Physics2DOneWay_PassFromBelowLandFromAbove` | a body passes up through and lands on a one-way platform |
| `Test_Physics2DOneWay_DropThroughCommand` | the drop-through command passes the body down |
| `Test_Physics2DCCD_BulletThroughThinWall` | a 0.05 m-radius circle at 300 m/s and 60 Hz passes neither a 0.02 m static wall nor, as a bullet, a 0.02 m dynamic box |
| `Test_Physics2DSleep_RestAndWake` | a stack sleeps within the sleep time and wakes on contact |
| `Test_Physics2DQuery_TieBreakDeterministic` | equal-fraction hits resolve by simulation ID; of 100 hits on one ray the 64 kept are the 64 smallest and the truncation bit is set |
| `Test_Physics2DQuery_ConcurrentReads` | queries from 8 jobs during the read phase return identical results |
| `Test_Physics2DQuery_ConcurrentReadsStress` | 32 sub-jobs × 10,000 casts under `Sanitizers -Preset win-asan` (the race-detector gap: Zenith2D is never built in linux-tsan) report nothing and return equal results |
| `Test_Physics2DJoint_BreakEmitsEvent` | a 150 N revolute joint holds a 10 kg box at rest and breaks exactly once, with the event, under a 1,000 N·s impulse |
| `Test_Physics2DOriginShift_ContinuesSeamlessly` | a shift of (2,048, −2,048) m keeps query results equal minus S within one rounding, changes no external-write or `static_moved` counter and keeps a resting body at rest; a world-anchored joint destroyed and re-created after the shift (its property 20 `WorldAnchor` rewritten to the current-frame point) keeps its world anchor within one binary32 rounding; a body-to-body joint's AnchorA and AnchorB bits are unchanged by the shift and it is not re-created (`Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D origin shift"`) |

### Save and memory

`Zenith_TwoDSave_Tests.cpp` (the replay test `Test_TwoDDeterminism_Replay` is in `Zenith_Integration_TwoDDeterminism`, "Integration suite"):

| Test | Asserts |
|---|---|
| `Test_TwoDSave_ExactRoundTrip` | save at tick 300, restore, run 300 ticks: bit-identical to the uninterrupted run, when `SupportsExactState` is true. Ground-body case (EXACT backend): shift the origin by (2,048, 0) m before the save, create a null-`BodyB` joint two ticks after the restore, and assert the per-tick world-state hashes equal the uninterrupted run's (the 80-byte header's `g` restored; with `g` lost the joint anchors 2,048 m away) |
| `Test_TwoDSave_ExactRoundTripWithPendingCommands` | a save taken in a tick whose phase 10 recorded impulses, a kinematic target, an `AnimatorPlay` on a skeletal entity and edits in two chunks continues bit-identically, with no external-write counter change in the first restored tick |
| `Test_TwoDSave_CoherentDowngrade` | over a backend whose `SupportsExactState` is false, `GetCaptureFidelity` returns COHERENT, the save carries `uSAVE_HEADER_FLAG_COHERENT_ONLY`, and the restore recreates bodies at the recorded poses and velocities with no duplicate `ContactBegan` |
| `Test_TwoDSave_CorruptSectionRefused` | each corrupted field of the save section yields `SAVE_ERROR_CORRUPT`, never an assert |
| `Test_TwoDMemoryConsumer_ReportsAndShedsNothing` | resident bytes reported and unchanged under forced WARNING and CRITICAL |

### Script bindings

`Zenith_TwoDScript_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_TwoDScript_FunctionsBound` | every function of `Zenith2/Docs/Design/TWOD.md → "Luau surface"` is bound with its access class |
| `Test_TwoDScript_MisuseCodes` | each row of `Zenith2/Docs/Design/TWOD.md → "Luau errors and asserts"` raises its code and text, never an assert |
| `Test_TwoDScript_BufferedCommandsApplyNextSync` | `TwoD.SetTile`, `TwoD.Play` and `TwoD.ApplyImpulse` take effect at the next sync point, not in the call |

## Render tests

GPU, on WARP and lavapipe, oracles computed in the test. The ZenithFlux-only ones are TWOD's files of `ZenithFlux_Tests` (`Tests/Sprite2D/`); the ones that need the module are GPU cases of `Zenith_Integration_TwoD` (`Zenith2/Tests/TwoD/Render/`):

| Test | Asserts |
|---|---|
| `Test_Sprite2DRender_PixelExactCanvas` | the ramp atlas drawn at scales 1, 3 and 6 into an `R8G8B8A8_SRGB` target equals the CPU mapping byte for byte |
| `Test_Sprite2DRender_FlipsAndTileSymmetries` | sprites and tiles under the eight symmetries equal the reference mapping byte for byte |
| `Test_Sprite2DRender_SortOrderVisible` | overlapping sprites of different layers, orders and y keys show the expected top colour at every overlap pixel |
| `Test_Sprite2DRender_BlendModes` | alpha, additive, multiply and premultiplied blends equal the linear-light formulas within 1/255 |
| `Test_Sprite2DRender_CompositeLetterbox` | the letterbox bars are the clear colour, the canvas region is the canvas scaled by k with no filtering |
| `Test_Sprite2DRender_WorldSpaceDepthTest` | a world-space sprite behind an opaque mesh is hidden where covered, on standard-Z and reversed-Z views |
| `Test_Sprite2DRender_TileChunkScatterAndEdit` | a tile edit appears in the next frame's image and only the edited chunk is uploaded |
| `Test_Sprite2DRender_HDR10Encoding` | in HDR10 output a white canvas encodes to PQ of `r.hdr.uiNits` (Stage 5) |
| `Test_Sprite2DRender_UIOverCanvas` | UI drawn after `FLUX_EXTENSION_POINT_BEFORE_UI` covers the canvas |
| `Test_Sprite2DRender_HeadlessAddsNoPass` | on the Null RHI the 2D extensions add no pass and the simulation still runs |
| `Test_Sprite2DRender_DeviceLossRecovers` | after an injected device loss, with 48 wanted chunks and `maxChunkUploadsPerFrame` 64, the GPU pool and table are rebuilt from the CPU mirrors and the first recovered frame's image equals the pre-loss image; with 200 wanted chunks the image equals it by the fourth frame |
| `Test_Sprite2DRender_CanvasOverScene` | a sprite of alpha 0.5 in each blend mode over a 3D scene colour equals the linear-light oracle within 1/255 after the over composite |
| `Test_Sprite2DRender_PoolPressureRecovers` | a 64-chunk pool under a panning camera over a 400-chunk map: wanted chunks upload within the paced bound, only unwanted chunks are evicted, refusals are counted only when the wanted set exceeds the pool, and the image recovers when the camera returns |
| `Test_Sprite2DRender_WorldTilemapWantsFootprint` | a world-space tilemap of 16,384 chunks under a perspective world view with a 4,096-chunk pool: the wanted set equals the chunks of the frustum footprint on the layer plane plus one (computed independently in the test from the view-projection), only those upload, no refusal is counted while the footprint fits the pool, and the same map drawn on a pure-2D view's canvas follows the canvas rule. Three further cases, each wanted set computed independently in the test: a section with a canvas whose camera is at (100, 50) m, seen by a non-pure-2D view (the editor scene view), wants the footprint's chunks at the map's absolute position, not offset by (100, 50) m; a canvas-space map drawn by a view without a canvas wants its footprint's chunks and draws them; an infinite-far perspective camera tilted 80° from the layer plane's normal, whose horizon is in view, wants the map's authored bounds and uploads nearest-first |
| `Test_Sprite2DRender_WorldSpriteInPure2DView` | a `SPRITE2D_SPACE_WORLD` sprite in a pure-2D view appears in the output at its sort position |
| `Test_Sprite2DParity_CanvasScene` | the parity scene `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/Sprite2DCanvas/` (ramp atlas, the four blend modes, the eight symmetries, two tile layers) renders on Vulkan and D3D12 (WARP and lavapipe; the `BackendParity` comparison, [Z2-N-REND-941]) and the two canvases compare in two regions, each pixel's region fixed by the scene: the pixel-exact subset (opaque, untinted, point-sampled texels of the ramp and of the eight symmetries and both tile layers, which "Pixel exactness rules" makes byte-exact) byte for byte; every pixel covered by a semi-transparent texel, a tint or the ADDITIVE or MULTIPLY blend within 1/255 per channel of the encoded value, TWOD's declared per-feature parity tolerance for the 2D canvas (its row of `Zenith2/Docs/Rendering.md → "Capability matrix"`, requested as cross-doc item X348). The directory holds text only (Q55): the scene document `Sprite2DCanvas.scene.json` and `Generator.md`, the generator description that names the seeds and the support functions (`Zenith_TwoDTestImageRamp`, `Zenith_TwoDTestCookedWriter.h`) the test calls to generate the ramp atlas and the tilemap under `${ZENITH_OUT}/test-fixtures/TWOD/` at test time |

## Cook and authoring tests

| Test | File | Asserts |
|---|---|---|
| `Test_SpriteAtlasPacker_Deterministic` | `Zenith_SpriteAtlasPacker_Tests.cpp` | identical bytes for 1,000 generated frames, twice and in shuffled document order |
| `Test_SpriteAtlasPacker_NoOverlapAndPadding` | same | no overlap of padded rectangles; every frame inside its page |
| `Test_SpriteAtlasPacker_ExtrudeReplicatesEdges` | same | extrude texels equal their edge texels |
| `Test_SpriteAtlasPacker_TrimKeepsPivot` | same | a trimmed frame draws at the same place as the untrimmed one |
| `Test_TwoDCompile_MatchesTestWriter` | `Zenith_TwoDCompile_Tests.cpp` | the four document compilers' sections equal `Zenith_TwoDTestCookedWriter.h`'s for the same descriptions |
| `Test_TwoDValidation_EveryRule` | `Zenith_TwoDDocuments_Tests.cpp` | one failing document per validation rule, with the rule's message and severity |
| `Test_TwoDMigration_Fixtures` | same | every committed v1 fixture migrates to the current version and equals its expected document |
| `Test_TwoDDocuments_CanonicalRoundTrip` | same | load and save of every generated document is byte-identical; unknown members are preserved |
| `Test_TilesetAutoTile_BlobReduction` | `Zenith_TilesetAutoTile_Tests.cpp` | the 256 masks reduce to the 47 blob classes and each resolves to its rule's tile |
| `Test_TwoDSceneChecks_EveryRule` | `Zenith_TwoDSceneChecks_Tests.cpp` | each scene check fires on its scene |
| `Test_TwoDCook_AtlasImportAndCook` | `Zenith_TwoDCook_Tests.cpp` | generated PNGs → importer → page textures with usage `COLOUR_EXACT`, one mip, and a `.zatlas` the runtime loads |
| `Test_TwoDCook_Deterministic` | same | clean and incremental cooks give byte-identical TWOD files |
| `Test_TwoDCook_ImageEditRecooksDependents` | same | editing an image re-cooks the atlas and the animation, tileset and rig that name it, and nothing else |
| `Test_TwoDCook_ValidatorRunsReader` | same | the registered validator refuses a corrupted file the step wrote (injected) |

## Editor tests

`Zenith2/Tools/ZenithEditor/Tests/2D/`:

| Test | Asserts |
|---|---|
| `Test_TwoDEditor_ExtensionRegisters` | the extension's panels, handlers, inspectors and commands register with no duplicate |
| `Test_TwoDEditor_PaintStrokeUndo` | a brush stroke, a rectangle fill and a bucket fill each undo and redo to identical documents |
| `Test_TwoDEditor_TerrainBrushResolves` | painting a terrain re-resolves the 3 × 3 neighbourhood through the auto-tile rules |
| `Test_TwoDEditor_PlayModePaintUsesCommands` | painting in a play world records `TwoD.SetTile` commands and leaves the document unchanged |
| `Test_TwoDEditor_GameViewMatchesPlayer` | the game view at an integer scale is byte-identical to a Player render of the same world (WARP and lavapipe) |
| `Test_TwoDEditor_WorkflowRehearsal` | the automation script of `Zenith2/Docs/Design/TWOD/Authoring.md → "2D authoring workflow"` runs green |

## D-SEL-2DPHYS scenario suite

`Zenith2/Benchmarks/Physics2DSelection/Scenarios/`, run by `Zenith_Bench_Physics2DSelection --scenarios` against both candidates through the seam; each scenario of the table is a pass/fail check derived from physics, except the two cost scenarios of "D-SEL-2DPHYS cost procedure", which produce the values the Rows field compares. The scales are not constants of this design: the harness reads the Game2D scale from the content scale the Stage 1 flagship acceptance ticket for Game2D commits (`Zenith2/Docs/Research/Workloads/W-Game2D.md`, row TD17) and the W-UI2D scale from UI's Stage 1 workload scale ticket (`Zenith2/Docs/Research/Workloads/W-UI2D.md`, [Z2-N-BUDGET-195]); TWOD's proposed inputs to those tickets are in `Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"` and "UI2D sample content".

| Scenario | Pass condition | Gate (G1, G2) | After the ruling |
|---|---|---|---|
| `Stacking` | a 20-box pyramid settles and sleeps within 5 s with no box drifting more than 0.01 m | G1 | `Test_Physics2DScenario_Stacking` |
| `JointDistance`, `JointRevolute`, `JointPrismatic`, `JointWeld`, `JointWheel`, `JointMotor` | each joint holds its constraint within 0.005 m or 0.01 rad over 600 ticks, limits and motors as specified | G1 | `Test_Physics2DScenario_JointDistance`, `…_JointRevolute`, `…_JointPrismatic`, `…_JointWeld`, `…_JointWheel`, `…_JointMotor` |
| `OneWay` | `Test_Physics2DOneWay_PassFromBelowLandFromAbove` and drop-through | G1 | `Test_Physics2DScenario_OneWay` |
| `ContinuousCollision` | `Test_Physics2DCCD_BulletThroughThinWall` | G1 | `Test_Physics2DScenario_ContinuousCollision` |
| `Sensors` | enter and exit events exactly once per crossing of dynamic and kinematic bodies, none for a static body, a tile chain or another sensor (`Test_Physics2DEvents_TriggerReach`) | G1 | `Test_Physics2DScenario_Sensors` |
| `Sleeping` | `Test_Physics2DSleep_RestAndWake` | G1 | `Test_Physics2DScenario_Sleeping` |
| `KinematicPlatform` | a box rides a moving kinematic platform with less than 0.01 m slip per second | G1 | `Test_Physics2DScenario_KinematicPlatform` |
| `ChainSeams` | `Test_TileCollision_NoBumpAcrossChunks`, on each candidate's own chain realisation (ghost vertices for B, mesh chains with enhanced internal edge removal for J; `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"`) | G1 | `Test_Physics2DScenario_ChainSeams` |
| `Queries` | ray, circle and box queries return the analytic hits within 1 × 10^-4 m, and the bounded collector keeps the 64 smallest of 100 | G1 | `Test_Physics2DScenario_Queries` |
| `Determinism` | two runs on one build are bit-identical, on Windows and on the S24 | G2 | `Test_Physics2DScenario_DeterminismWindows` (the Windows half; the S24 half is a device record) |
| `ExactSave` | save and restore at tick 300 continue bit-identically (`SupportsExactState`) | recorded, not a gate: it breaks ties after the rows | `Test_Physics2DScenario_ExactSave`, which asserts bit-identical continuation when the winner supports exact state and asserts COHERENT capture fidelity otherwise |
| `CostGame2D`, `CostUI2D` | none: cost scenarios ("D-SEL-2DPHYS cost procedure"); they produce the zone p95 and peak resident bytes the Rows field compares, and assert only that the scene builds and runs | not a gate | none: after the ruling the rows are measured on the module by `Zenith_Bench_TwoD` ("Benchmarks") |

- `Test_Physics2DSelection_HarnessRuns` (the harness ticket's registered CTest, `Test` step, win-debug and win-release) runs every pass/fail scenario on both candidates at its own size for 60 ticks, and `CostGame2D` and `CostUI2D` on both candidates at their smoke counts for 60 ticks after a 10-tick warm-up ("D-SEL-2DPHYS cost procedure"). It asserts that every (candidate, scenario) pair completes and writes one result row to `${ZENITH_OUT}/physics2d-selection/smoke.json`, that `FindSite("Physics2DSelection.Tick")` is non-zero after the first cost tick (a zero site means the profiler hooks were not installed and no zone recorded), that each cost row carries a `Physics2DSelection.Tick` zone count equal to its measured tick count (one zone per tick) and a non-zero peak resident byte value, that the harness's own oracles (the analytic query hits, the reference outline) agree with themselves, and that nothing asserts, crashes or reports a sanitizer error. It asserts nothing about which candidate passes or what a cost is: that is data for the ruling.
- The measured run is the `needs-human` measurement run ticket of `Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"`: every pass/fail scenario on both candidates, the S24 half of `Determinism`, and `CostGame2D` and `CostUI2D` at the committed scales on the reference desktop and the S24 under "D-SEL-2DPHYS cost procedure", written to `Zenith2/Docs/Research/Physics2DSelection/<date>.md`.
- After the ruling the `Test_Physics2DScenario_*` cases of the last column run on the winner in `Zenith_Integration_TwoD` (`Test` step). They are the scenarios the `Test` step can run: the S24 half of `Determinism` and every cost are measured only by device runs and the `needs-human` budget runs ("Benchmarks", "Device runs").

### D-SEL-2DPHYS cost procedure

The one measurement the Rows field of `Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"` compares, defined so that two operators measure the same thing on either candidate.

**Harness main.** The target is `zenith_add_benchmark(Physics2DSelection DEPENDS Zenith2D ZenithSave ZenithProfiling ZenithCore THIRD_PARTY box2d jolt SUPPORTED ZENITH_WINDOWS ZENITH_ANDROID)` (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests"`). Its `main` (`Zenith2/Benchmarks/Physics2DSelection/Internal/Zenith_Physics2DSelectionMain.cpp`) runs, in this order, the Player's startup steps 1 and 2 that a zone needs (`Zenith2/Docs/Design/DEVT/Profiler.md → "Creation and shutdown"`, the Player list):

1. `Zenith_ApplyCrtMathMode()` as its first statement: `_set_FMA3_enable(0)` on Windows x64 (ruling X314-FMA3; `Zenith2/Docs/Design/CORE/Maths.md → "The CRT maths mode"`; the assumption block of `Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"`);
2. construct the root `Zenith_Heap` on its stack and `Zenith_InstallRootHeap(&xHeap)`;
3. `Zenith_Profiler::Create(Zenith_GetPlayerProfilerDesc(Zenith_GetBuildPlatform(), Zenith_GetBuildConfiguration(), Zenith_IsSanitizerBuild()))`, whose desc starts in `PROFILER_MODE_CAPTURE` in every non-Shipping build; on failure write `Physics2DSelection: profiler creation failed: <error>` with `Zenith_WriteDiagnosticLine`, uninstall the heap and return 4;
4. `Zenith_ProfilerHooks xHooks{}`, `xProfiler->FillHooks(xHooks)` and `Zenith_InstallProfilerHooks(&xHooks)`; the log members stay null (the harness creates no log router, so its log records are discarded and every setup failure below is written with `Zenith_WriteDiagnosticLine`);
5. `xProfiler->RegisterThreadLane(0, "Physics2DSelection")`, which binds the harness thread as lane 0 (no job system exists to register it);
6. run the selected scenarios, then `Zenith_InstallProfilerHooks(nullptr)`, destroy the profiler, `Zenith_InstallRootHeap(nullptr)` and destroy the heap; the exit code is 0 when every scenario ran, 1 otherwise.

**Zone.** Each simulated tick of a cost scenario runs inside one `ZENITH_PROFILE_SCOPE("Physics2DSelection.Tick")` (`Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_ProfileZone header"`), followed by one `EndFrame(xInputs)` with a value-initialised `Zenith_ProfilerFrameInputs` whose `m_ulFrameIndex` is the tick index and `m_uTickCount` is 1 (every other member keeps its default: no frame timeline, scale 10000, not excluded), so one profiler frame is one tick; the zone closes before `EndFrame`, so `EndFrame`'s carry rule never moves it to the next frame. The zone encloses exactly the seam calls one `TwoD.Physics2DStep` would make for that tick, in this order:

1. the tick's `CreateBody`, `AddShape`, `AddChain`, `CreateJoint`, `DestroyBody`, `RemoveShape`, `DestroyJoint`, `SetBodyTransform`, `SetBodyVelocity` and `SetAwake` calls, then its `ApplyImpulse`, `ApplyAngularImpulse`, `ApplyForce`, `ApplyTorque` and `SetGravity` calls;
2. `Step(1/60 s, 4)`;
3. `DrainContacts`; candidate J's sensor `CollideShape` pass after `Update` runs inside its `Step` or `DrainContacts` and candidate B's sensor work inside `b2World_Step`, so neither candidate's sensor work falls outside the zone;
4. `GetBodyState` for every body that is awake after the step or changed sleep state in it;
5. `GetJointReactionForce` for every joint with `BreakForce > 0` (the cost scenes have none).

The sample of one tick is the zone's inclusive time, `Zenith_ProfilerZoneStat::m_ulTotalNs` of the site `FindSite("Physics2DSelection.Tick")` in `GetZoneStats()`, read after that tick's `EndFrame`. A cost scenario runs ticks 0 … 2,099 and samples ticks 300 … 2,099 (1,800 samples after a 300-tick warm-up); the report gives p50 and p95 by the nearest-rank method of MP-CPUZONE (`Zenith2/Docs/Budgets.md → "Measurement procedure catalogue"`), in ms to 0.01 ms. Memory is the maximum over ticks 0 … 2,099 of `GetResidentBytes() + GetProcessResidentBytes()` read after each tick, in MB (2^20 bytes) to 0.01 MB. The harness thread is the profiler's lane 0, and no other work runs in the process during a measured run.

**Comparison.** `CostGame2D`'s p95 and peak bytes are compared with TWOD-004's and TWOD-005's committed W-Game2D values and `CostUI2D`'s with their W-UI2D values, per platform (reference desktop, S24). TWOD-004's own procedure also spans ingest, write-back and sync, which the harness does not run, so each committed value is an upper bound for the harness value. "Meets the rows" is the harness value at most the committed value in all eight comparisons (two scenarios × two rows × two platforms), and the report labels each comparison an upper-bound check.

**Scene construction.** Both cost scenarios build the same seeded scene from three counts read from the committed workload file: `B` dynamic bodies, `E` static tile unit edges and `J` joints. The harness reads them from the file's `## 2D physics scale` section, a two-column table `| Count | Value |` whose rows are named exactly `dynamic bodies`, `static tile unit edges` and `joints` with plain integer values (rows TD16 and TD17 ask the two scale tickets for that section), and the report copies the file path, the SHA-256 of its bytes and the three counts. `CostGame2D` reads them from `Zenith2/Docs/Research/Workloads/W-Game2D.md` (row TD17); `CostUI2D` reads them from `Zenith2/Docs/Research/Workloads/W-UI2D.md` (row TD16), with `E` and `J` 0 when that file commits no tile edges or joints. A missing file, a missing `B`, `J > B` or `E` below the bin's edge count fails the scenario's setup with `PHYSICS2D_ERROR_INVALID_DESC` and a `Zenith_WriteDiagnosticLine` line naming the file and the count. The smoke counts of `Test_Physics2DSelection_HarnessRuns` are fixture constants of that test, not the measurement scale: `CostGame2D` B = 200, E = 2,000, J = 20; `CostUI2D` B = 50, E = 0, J = 0 (when E is 0 the bin check is skipped and the bin of step 2 is built from three static boxes of the same floor and wall extents instead of tiles).

Every random choice is `u(k) = (SplitMix64(0x2D2D2D2D2D2D2D2D + k) >> 40) × 2^-24` in [0, 1), where `SplitMix64(z)` is `z += 0x9E3779B97F4A7C15; z = (z ^ (z >> 30)) × 0xBF58476D1CE4E5B9; z = (z ^ (z >> 27)) × 0x94D049BB133111EB; return z ^ (z >> 31)` in wrapping 64-bit arithmetic, so the scene is identical on both candidates and every platform. With cell size 0.5 m, tick 1/60 s, 4 substeps, gravity (0, −9.81) m/s² and the other `twod.physics.*` defaults:

1. **Grid bodies.** `G = B − J` bodies in a grid of `C = ⌈√G⌉` columns and `Rw = ⌈G / C⌉` rows at 1.0 m pitch. Body i (0-based, row-major from the bottom-left) is centred at `(2.0 + (i mod C) + 0.1 × (u(2i) − 0.5), 2.5 + ⌊i / C⌋ + 0.1 × (u(2i + 1) − 0.5))` m, rotation 0, its shape chosen by `i mod 3`: 0 a box 0.8 × 0.8 m, 1 a circle of radius 0.4 m, 2 a capsule of segment length 0.4 m and radius 0.25 m; density 1 kg/m², friction 0.6, restitution 0, `Bullet` false, `SleepAllowed` true.
2. **Bin.** One static tilemap layer at map origin (0, 0) whose FULL cells are a floor band x ∈ [0, W), y ∈ [0, 2) and two walls x ∈ [0, 2) and x ∈ [W − 2, W) for y ∈ [2, H), with `W = 2C + 8` and `H = 2Rw + 24` cells. Its outline has `E_bin = 2W + 4H − 4` unit edges.
3. **Pegs.** The remaining `R = E − E_bin` unit edges are `P = ⌊R / 4⌋` isolated FULL cells (4 unit edges each) on the lattice x = W + 8 + 2a, y = 2b (cells), a = 0 … 127, b = 0, 1, …, in row-major order, followed on the same lattice by `R − 4P` (0 … 3) ONE_WAY_UP cells (1 unit edge each). No body reaches them: they are the share of a level's static terrain that lies away from the action.
4. **Chains.** `K = ⌈J / 10⌉` chains. Chain k (0-based) hangs from the world at `(1.0 + (k + 0.5) × (C + 2) / K, 0.5H − 0.5)` m: 10 links (the last chain `J − 10(K − 1)`), each a box 0.2 × 0.5 m of density 1 kg/m², link 0 joined to the ground body (`BodyB` null) and link n to link n − 1 by a revolute joint at their shared end, with no limit, no motor and `BreakForce` 0. The links are the `J` bodies of `B` that are not in the grid; the lowest link ends at y = Rw + 6.5 m, 5 m above the centre of the grid's top row.
5. **Activity.** At every tick t ≥ 300 with `(t − 300) mod 60 = 0`, every grid body i with `⌊u(1,000,000 + 65,536 × (t − 300) / 60 + i) × 10⌋ = 0` (about 10 % of `G`) receives `ApplyImpulse(m_i × (0, 4) kg·m/s)` at its last position read by `GetBodyState`, so that share keeps waking while the pile below it sleeps.

At the smoke counts `CostGame2D` has G = 180, C = 14, Rw = 13, W = 36, H = 50, E_bin = 268, R = 1,732, P = 433 pegs and K = 2 chains; at TWOD's proposed Game2D scale (2,000 / 20,000 / 200, row TD17) it would have C = 43, W = 94, H = 108, E_bin = 616 and P = 4,846, but the harness reads the committed numbers, never these. `CostGame2D` and `CostUI2D` gate nothing and assert nothing about a candidate.

## Integration suite

`Zenith_Integration_TwoD` (`Zenith2/Tests/TwoD/`) boots a runtime with the 2D module:

| Test | Asserts |
|---|---|
| `Test_TwoDIntegration_CookThenLoad` | a generated project cooked by ZenithCook loads every TWOD kind and draws its first frame |
| `Test_TwoDIntegration_RegistryFreezesWithEngineTypes` | the runtime boots with the 2D module selected, so one registry holds every engine type (RPROD's `Zenith.Render.Camera` included) beside every TWOD type, and startup `Freeze` succeeds with no `REFLECT_ERROR_BAD_NAME`; `FindTypeByScriptName("Camera2D")` returns `Zenith.TwoD.Camera` and every name of TWOD's two script-name tables resolves to its own type. `Zenith2D_Tests` cannot reach RPROD's `Internal/` registration function, so this assertion lives here. The case with the Physics module selected (and PHYS's types in the same registry) is added by the J-path ticket, the only path on which a TWOD suite links PHYS; the every-module freeze is MOD's integration test (X353-SCRIPTNAMES) |
| `Test_TwoDIntegration_BootWithout2DModule` | a game without the module builds and boots with no TWOD symbol linked |
| `Test_TwoDIntegration_StreamingTilemaps` | tilemaps activated and deactivated with their scenes release chunks and pool slots |
| `Test_TwoDIntegration_SaveLoadMidLevel` | a save mid-level restores tile edits, animator states and bodies |
| `Test_Physics2DScenario_Stacking`, `…_JointDistance`, `…_JointRevolute`, `…_JointPrismatic`, `…_JointWeld`, `…_JointWheel`, `…_JointMotor`, `…_OneWay`, `…_ContinuousCollision`, `…_Sensors`, `…_Sleeping`, `…_KinematicPlatform`, `…_ChainSeams`, `…_Queries`, `…_DeterminismWindows`, `…_ExactSave` | the scenario suite's last column, on the winner (sixteen cases, each its own CTest) |
| `Test_TwoDIntegration_Game2DBoots` | Game2D's cooked content boots headless on the Null RHI, runs 600 ticks of its first level's scripted path and exits with code 0 |

`Zenith_Integration_TwoDDeterminism` (`Zenith2/Tests/Determinism/TwoD/Zenith_TwoDDeterminism_Tests.cpp`, TWOD's member of the determinism suites of [Z2-N-ARCH-886], linking the 2D module and the three ECS backends):

| Test | Asserts |
|---|---|
| `Test_TwoDDeterminism_Replay` | the 600-tick log of the 2D determinism scene of `Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"` replays with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend, with bit-identical per-tick world-state hashes per backend |

## Fuzz targets

| Target | Input | Calls |
|---|---|---|
| `Zenith_Fuzz_TwoDAsset` | arbitrary bytes | each TWOD kind's reader after `Zenith_ParseCookedHeader` |
| `Zenith_Fuzz_TwoDDocument` | arbitrary JSON | `Zenith_ValidateTwoDDocument` and the four compilers |
| `Zenith_Fuzz_TwoDSaveSection` | arbitrary bytes | the save participant's section reader |

Each runs without a crash, a sanitizer report or an assert; seed corpora are generated from the test writer.

## Benchmarks

`Zenith_Bench_TwoD` measures the TWOD rows of `Zenith2/Docs/Design/TWOD.md → "Budget rows"` on W-UI2D and W-Game2D with the procedures their rows name (MP-CPUZONE spans `TwoD.Extract`, `TwoD.Animate`, `TwoD.AnimateSkeletons`, the 2D preparation in `Flux.BuildGraph` step 4 and `WriteFrame`; GPU pass timings of the 2D passes; resident bytes), on the reference desktop and the S24. Its loop ticket's evidence is its registered `--smoke` CTest `Test_TwoDBench_SmokeRuns` (one second of each workload's scene headless on the Null RHI, every zone and counter present in the output); the measured values are recorded by the `needs-human` TWOD budget-run ticket (the Stage 1 per-system budget ticket for TWOD and, at each subsequent stage acceptance, its re-run ticket), never by a worker.

## Device runs

Device steps are `needs-human` acceptance records, not gate steps: the Game2D playthrough on the S24 (pixel-exact output at the device's integer scale, the budget rows of W-Game2D), the WorkloadUI2D run on the S24 and the x86_64 emulator boot of Game2D. Their records are the flagship acceptance evidence of `Zenith2/Docs/Design/TWOD.md → "Stage plan"`.

## Gate steps and labels

| Gate step | What TWOD contributes |
|---|---|
| `Build` | Zenith2D in every `ZENITH_GPU` preset; the TWOD files of ZenithFlux, ZenithAuthoring, ZenithCook and ZenithEditor; the harness in win-debug, win-release and android-arm64 |
| `Test` | every unit, render, cook, editor and integration test above, the harness CTest and `Test_TwoDBench_SmokeRuns` |
| `Sanitizers -Preset win-asan` | `Zenith2D_Tests`, `Test_Physics2DQuery_ConcurrentReadsStress` included |
| `Sanitizers -Preset linux-tsan` | nothing: Zenith2D is `ZENITH_GPU` and never built there, a documented race-detector gap ("Physics" above) |
| `BackendParity` (from Stage 3, [Z2-N-REND-940]; TWOD's ZenithFlux and shader diffs fall in `Zenith2Render`) | `Test_Sprite2DParity_CanvasScene`: the 2D parity scene's canvas compared between Vulkan and D3D12, byte for byte on the pixel-exact subset and within the declared 1/255-per-channel tolerance on blended and tinted pixels ([Z2-N-REND-941]) |
| `ShaderChecks` | the four Sprite2D programs and their reflected parameter blocks |
| `ApiContractCheck`, `HeaderSelfContained` | every public header of `Zenith2/Docs/Design/TWOD/PublicAPI.md` |
| `ForbiddenCheck` | Box2D and Jolt headers only under the harness's `Internal/` and, after the ruling, Zenith2D's `Internal/Physics2D/Box2D/`; no mutable statics |
| `Libgraph` | the declared `DEPENDS` of `Zenith2/Docs/Design/TWOD.md → "Library and file layout"` |
