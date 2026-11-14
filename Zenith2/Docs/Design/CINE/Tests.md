# CINE tests

## Scope of this file

Every test target, test file and named test of the CINE area, with the case each asserts and the gate step that runs it. Names follow `Zenith2/Docs/Contracts.md` → "Test naming" (test ID `Test_<Type>_<Behaviour>`, doctest name `"<Library>/<Type>/<Behaviour>"`); every name below is unique programme-wide (TEST-NAME-UNIQUE: CINE's types are prefixed `Cinematic`, `Cinematics`, `Video`, `FluxVideo`, `CineCapture` or `CineExport`, and no name reuses the `Test_Capture_*` or `Test_CaptureConvert_*` families of other areas). Tests are pass or fail only and pin no count ([Z2-N-CONTRACT-408]). Fixtures are generated at test time under `${ZENITH_OUT}/test-fixtures/CINE/` or are hand-written spec-derivable text (Q55, GOLDEN-FILES): no binary golden is committed. Test doubles implement non-final interfaces only, selected through a declared seam (FINAL-DOUBLES); test support outside ZenithCore is header-only (X184). Zenith2 has no Null RHI backend: every test that needs a renderer creates a device on WARP (D3D12) or lavapipe (Vulkan) (`Zenith2/Docs/Design/RHI.md` → "Headless CI devices", "CI windows for GPU tests"), which is not `needs-gpu` (X170); a headless run (no renderer) exercises everything above the renderer.

## Test targets

| Target | Directory | Links | Gate steps |
|---|---|---|---|
| `ZenithCinematics_Tests` | `Zenith2/Engine/ZenithCinematics/Tests/` | `ZenithCinematics`, its declared dependencies, `ZenithTestSupport`, doctest; may include `ZenithCinematics/Internal/` (the module's test construction seam, the pumped video thread); a WARP or lavapipe device with a hidden window for the cases marked "device" | `Test` (every preset that builds it), `Sanitizers` (`win-asan`), `Coverage` |
| `ZenithFlux_Tests` (CINE's files) | `Zenith2/Engine/ZenithFlux/Tests/Video/` | as ZenithFlux's own; a WARP or lavapipe device | `Test`, `BackendParity` (the render cases on both backends) |
| `ZenithAuthoring_Tests` (CINE's files) | `Zenith2/Tools/Authoring/Tests/Cinematics/` | `ZenithAuthoring` (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 2) | `Test` (`win`, `ZENITH_TOOLS_HOST`) |
| `ZenithCook_Tests` (CINE's files) | `Zenith2/Tools/ZenithCook/Tests/Cinematics/` | ZenithCook's objects (the cook steps, the importer, the MP4 reader) | `Test` (`win`) |
| `ZenithEditor_Tests` (CINE's files) | `Zenith2/Tools/ZenithEditor/Tests/Cinematics/` | the editor's test target (ED15) | `Test` (`win`) |
| `Zenith_Integration_Cinematics` | `Zenith2/Tests/Cinematics/` | `ZenithRuntime` and the Animation, Audio, Cinematics, Physics, Gameplay modules; a WARP or lavapipe device with a hidden window for the cases marked "device" | `Test` |
| `Zenith_Fuzz_CinematicLoader`, `Zenith_Fuzz_VideoLoader` | `Zenith2/Engine/ZenithCinematics/Tests/Fuzz/CinematicLoader.cpp`, `VideoLoader.cpp` | libFuzzer in the `win-asan` preset (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets") | the fuzz smoke of `Sanitizers -Preset win-asan` (60 s per target; seeds built in code from `Tests/Support/Zenith_CinematicSeedFiles.h`, no fixture required) |
| `Zenith_Bench_CinematicEvaluate`, `Zenith_Bench_VideoDecode` | `Zenith2/Benchmarks/Budgets/CINE/` | nanobench | the Stage 1 per-system budget ticket for CINE and acceptance runs; never a gate |
| `Zenith_Bench_VideoDecoderSelection` | `Zenith2/Benchmarks/VideoDecoderSelection/` | nanobench, `ffmpeg-libavcodec` | its CTest `--smoke` run; measurements only in the `needs-human` ruling ticket (`Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria") |

Test support headers (header-only, X184), in `Zenith2/Engine/ZenithCinematics/Tests/Support/`:

- `Zenith_CinematicFixtures.h` — document builders, and the **fixture writer**: it writes cooked `.zcine` and `.zvid` bytes directly from builder values, field by field per `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked cinematic file" and "Cooked video file" (spec-derivable goldens, GOLDEN-FILES). It is the only CINE test writer of cooked files, and `Test_CinematicCook_FixtureWriterMatchesCookStep` (ZenithCook_Tests) cooks every fixture document with `cine.sequence` and asserts byte equality, so the two writers cannot drift.
- `Zenith_TestVideoDecoder.h` — the test decoder and `Zenith_CreateTestVideoDecoder` (`Zenith2/Docs/Design/CINE/Video.md` → "Zenith_VideoDecoder interface").
- `Zenith_CinematicTestWorld.h` — a headless runtime with the Cinematics module constructed through `Zenith_CinematicsModuleDesc` and recording Animation and Audio command kinds (production kinds registered by a test module that logs the records they play back), at 60 Hz.
- `Zenith_CinematicSeedFiles.h` — the fuzz seeds built in code.

## Module and registration tests

File `Tests/Zenith_CinematicsModule_Tests.cpp`.

| Test | Asserts |
|---|---|
| `Test_CinematicsModule_RegisterTypes_RegistersEveryType` | the 6 payload structs, the 4 structs (BindingEntry, Ticket, InstanceID, LifecyclePayload), the component, 4 events and the resource are registered with the stable names and property IDs of `Zenith2/Docs/Design/CINE/PublicAPI.md` |
| `Test_CinematicsModule_Start_RegistersElevenKinds` | `FindByName` finds the 11 `Cinematics.*` kinds with the phase masks and maximum payload sizes of the kind table |
| `Test_CinematicsModule_Start_FailsAtStep1_Unwinds` … `Step7` (seven tests) | the fault point `uFAULT_POINT_CINEMATICS_START_FIRST + step − 1`, injected through `Zenith_CinematicsModuleDesc::m_pxFaults`, returns the step's error and leaves no registration behind |
| `Test_CinematicsModule_Attach_FailsAtStep1_Unwinds` … `Step4` (four tests) | as above for `AttachWorld` with `uFAULT_POINT_CINEMATICS_ATTACH_FIRST` |
| `Test_CinematicsModule_Headless_NoVideoNoThread` | with no renderer, Start creates no decoder and no thread; extraction writes nothing |
| `Test_CinematicsModule_ConfigKeys_RangesValidated` | each `cine.*` key outside its range is a configure error naming the key; the f32 keys convert to the expected integer µs |
| `Test_CinematicsModule_InternalKinds_RefusedFromOtherSystems` | a `Cinematics.Begin`, `End`, or a start with `uCINE_START_FLAG_FROM_PLAYER`, recorded by a system other than `Cinematics.Evaluate`, is dropped and counted |
| `Test_CinematicsModule_Schedule_FreezesWithPhysicsAndAnimation` | a world with the Physics, Animation and Cinematics modules freezes and compiles with no `ecs.schedule` error (in particular no `SCHEDULE_ERROR_CROSS_ENTITY_READ`); one tick of a cinematic driving a physics body's `Zenith.LocalTransform` and a player row reached by handle then runs with the own-row marking active (`Physics.SyncTransforms` writes the same columns), and the cinematic value reaches arbitration and the player's `Instance` is written (each entity made current before `GetChunkOfEntity`) |
| `Test_CinematicsModule_Attach_DeclaresSourceAndHandler` | `AttachWorld` declares `ASYNC_SOURCE_CINEMATICS` on the world's sink with `Zenith_ModuleSourceDesc{ 64, 24, MODULE_SOURCE_SITE_MERGE_CHAIN }` and sets CINE's handler; a second world's attach reuses no handler entry of a live world; after `DetachWorld` the entry's record pointer is null |
| `Test_CinematicsModule_Shutdown_JoinsVideoThread` (device: WARP or lavapipe renderer, test decoder through the seam) | shutdown calls `DisableVideo`, closes every player, joins `Zenith.VideoDec` and destroys the decoder in that order |

## Unit tests

| File | Test | Asserts |
|---|---|---|
| `Zenith_CinematicTime_Tests.cpp` | `Test_CinematicTime_SixtyHzFifteenTicks_Exactly250ms` | 15 advances at 60 Hz, rate 1, give 250,000 µs exactly |
| | `Test_CinematicTime_RateQ16_RoundsHalfUp` | the advance of every rate in a table of 32 rates matches the formula |
| | `Test_CinematicTime_OneHourAt1000Hz_NoOverflow` | the scaled value at the limit is exact |
| | `Test_CinematicTime_SubTickOffset_InRange` | the worked rows of `Zenith2/Docs/Design/CINE/Playback.md` → "Time arithmetic": 60 Hz, key 10,000 µs → 0.010 s; rate 2 → 0.005 s; a key at the tick start → 0; at the tick end → the largest float below 1/60; a carried item → 0; the post-wrap row (D = 1 s, S₀ = 5.95 × 10⁷ before the wrap, a = 10⁶, a key at 0) → f = 0.5, 0.0083 s; offsets order keys within a tick |
| | `Test_CinematicTime_FireItemKeys_TimelineOrder` | item keys sort by (time, track, item) and the window `[next, s_cur]` after a cap resumes at the first unprocessed item |
| `Zenith_CinematicEvaluator_Tests.cpp` | `Test_CinematicEvaluator_Constant_HoldsValue`, `Test_CinematicEvaluator_Linear_Interpolates`, `Test_CinematicEvaluator_Cubic_MatchesHermite` | per interpolation, against an independent double-precision oracle within 1e-6 |
| | `Test_CinematicEvaluator_BeforeFirstAfterLast_Clamp` | values outside the keys |
| | `Test_CinematicEvaluator_SlerpGroup_ShortestArc` | slerp groups take the shorter arc; per-channel groups stay within 90° after cook alignment |
| | `Test_CinematicEvaluator_Slerp_MatchesDeterministicFormula` | for 1,000 seeded quaternion pairs (antipodal, identical and near-identical pairs among them, seed printed) and u in {0, 0.25, 0.5, 1}, `Zenith_CinematicSlerp` equals bit for bit the formula of "Curve channels and interpolation" evaluated in the test with `Zenith_Atan2` and `Zenith_Sin`, including the normalised-blend branch below 10⁻⁶ |
| | `Test_CinematicEvaluator_SectionWeights_SmoothstepEases` | section weights at ease boundaries |
| | `Test_CinematicEvaluator_OverlapEight_NormalisedBlend` | eight overlapping transform sections blend normalised |
| | `Test_CinematicEvaluator_KeysInRange_HalfOpen` | (from, to] semantics, the "before 0" sentinel, an empty range |
| | `Test_CinematicEvaluator_SameInputs_BitIdentical` | 10,000 random samples evaluate to identical bits twice (seed printed) |
| `Zenith_CinematicPayload_Tests.cpp` | `Test_CinematicPayload_ByPropertyName_MatchesLayout` | a payload built by reflection equals the C++ struct's bytes for `Zenith.Audio.PostEventPayload`-shaped test types |
| | `Test_CinematicPayload_UnknownProperty_DisablesKey` | `CINEMATIC_ERROR_PAYLOAD`, counted once |
| | `Test_CinematicPayload_Substitutions_Patched` | `$binding`, `$instance`, `$time` |
| `Zenith_CinematicCommands_Tests.cpp` | `Test_CinematicCommands_StartPayload_RoundTripsByReflection` | record, serialise, deserialise, equal (the tag as text) |
| | `Test_CinematicCommands_TicketFromMergeKey_Unique` | two starts in one buffer and in two buffers get distinct tickets; the key hash is `Zenith_Hash64` of the 20 little-endian bytes |
| | `Test_CinematicCommands_SixtyFiveBindings_OverflowRejected` | the overflow bit is set and playback rejects |
| | `Test_CinematicCommands_NonFiniteFloats_RejectedAtPlayback` | NaN and infinite rate, blends and start time record the sentinel and the start fails `CINEMATIC_ERROR_INVALID_ARGUMENT` |
| | `Test_CinematicCommands_BindingsSortedByHash` | the recorded bindings are in ascending name hash whatever the caller order |
| `Zenith_CinematicAudioIDs_Tests.cpp` | `Test_CinematicAudioIDs_NativeBitsClear_Deterministic` | bit 0 and bit 63 clear, never 0, identical across runs |
| | `Test_CinematicAudioIDs_DistinctPerSection` | 10,000 (instance, track, section) triples give no collision; caption source IDs are below 2⁵³ |

## Format and cook tests

| File | Test | Asserts |
|---|---|---|
| `Zenith_CinematicLoader_Tests.cpp` | `Test_CinematicLoader_ValidFile_Loads` | a fixture written by the fixture writer loads; every accessor of `Zenith_Cinematic` returns the authored values; marker names are interned |
| | `Test_CinematicLoader_CorruptL1` … `CorruptL12` (twelve tests) | each check fails with `CINEMATIC_ERROR_CORRUPT` and its check number |
| | `Test_CinematicLoader_LimitsAtAndAboveLimit` | each limit of "Sequence header section" (the 65,536 sections and the 8 fire-on-skip keys among them) at the limit loads and at limit + 1 fails |
| | `Test_CinematicLoader_FutureVersion_Refused` | `CINEMATIC_ERROR_VERSION` |
| | `Test_CinematicLoader_FindBinding_LogN` | `FindBinding` on 64 bindings, every name and an absent one |
| | `Test_CinematicValidation_MatchesLoader` | `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile` return the loader's code and check number for every corruption fixture |
| `Zenith_VideoLoader_Tests.cpp` | `Test_VideoLoader_ValidFile_Loads`, `Test_VideoLoader_CorruptV1` … `CorruptV7` | as above for `.zvid` |
| | `Test_VideoLoader_FindFrameAtOrBefore_PresentationOrder` | with B-frame reorder depth 2 |
| `Zenith2/Tools/ZenithCook/Tests/Cinematics/Zenith_CinematicCook_Tests.cpp` | `Test_CinematicCook_TwoCharactersFixture_CooksAndLoads` | document → cooked → loaded, values equal the document's |
| | `Test_CinematicCook_FixtureWriterMatchesCookStep` | every fixture document cooked by `cine.sequence` equals the fixture writer's bytes |
| | `Test_CinematicCook_EveryRule_Reported` | one document per rule of "Cook validation rules" yields that rule ID with the JSON pointer |
| | `Test_CinematicCook_AllFindingsReported` | a document with five errors reports five |
| | `Test_CinematicCook_SubsequenceFlatten_ShiftsAndRenames` | times shifted, clipped, bindings renamed, names prefixed |
| | `Test_CinematicCook_SubsequenceCycle_Refused` | |
| | `Test_CinematicCook_DdcKey_ChangesWithReferencedSchema` | changing a referenced component's schema changes the key; an unrelated schema does not |
| | `Test_CinematicCook_Deterministic_ByteIdentical` | two cooks of one document are byte-identical |
| | `Test_CinematicCook_RotationSignAlignment` | keys flipped, reported as info |
| | `Test_CinematicCook_LipSyncWindowTable` | a dialogue face clip whose published `anim.clip.markers` hold three `char.lipsync.<tag>` markers and windowEnd events cooks a 3-record window table (hash, T, e in µs) and three window-end items; a 17th window is `cine.cook.lipsync_windows`; a window whose end is past the section end gets no item |
| `Zenith2/Tools/Authoring/Tests/Cinematics/Zenith_CinematicDocument_Tests.cpp` | `Test_CinematicDocument_RoundTrip_Canonical` | load, write, byte-identical |
| | `Test_CinematicDocument_UnknownMembers_Preserved` | Z2-D-048 |
| `Zenith2/Tools/ZenithCook/Tests/Cinematics/Zenith_VideoImport_Tests.cpp` | `Test_VideoImport_Remux_BitstreamPreserved` | a generated MP4 (test codec in an `avc1`-shaped sample entry) remuxes bit for bit |
| | `Test_VideoImport_LimitsRefused` | size, duration, reorder depth, profile, a 10-bit profile for an Android game |
| | `Test_VideoImport_AudioExtracted_SubAsset` | a sound sub-asset intermediate is produced |
| | `Test_VideoImport_Mp4Bounds_Refused` | box depth, box count, sample count limits |
| | `Test_VideoImport_Mp4Mutations_RefusedWithoutCrash` | 20,000 seeded byte mutations of `SyntheticMp4` (seed printed) each return a result; none asserts |

## Playback tests

File `Tests/Zenith_CinematicPlayback_Tests.cpp` unless noted; each runs in `Zenith_CinematicTestWorld` at 60 Hz.

| Test | Asserts |
|---|---|
| `Test_CinematicPlayback_StartResident_BeginsInline` | a start of a prepared sequence begins at its playback: snapshot taken, START dispatched once with a null entity and once per bound entity, `Started` delivered in phase 9 of the same tick; the Begin tick writes no cinematic value |
| `Test_CinematicPlayback_StartNotResident_BindsThenBegins` | `BINDING` → publication at the boundary t → t + 1, where the publication handler adds the bound-set entry and resolves the bindings with the boundary access → `PREPARING` → `Cinematics.Begin` recorded by the Evaluate of t + 1 and played at the sync after phase 1; a binding error at the handler queues `StartFailed` (delivered in phase 9 of t + 1) and releases the instance |
| `Test_CinematicPlayback_PreloadRelease_PinCounts` | two preloads and one release keep the sequence resident; the last release with no instance frees it; a 33rd distinct preload is refused and counted |
| `Test_CinematicPlayback_AbandonedLoad_CompletedOnce` | a start cancelled while its load is in flight completes the completion ID once with the abandoned payload, drops the late ASSET record and counts no discard ([Z2-N-SCRIPT-931]) |
| `Test_CinematicPlayback_DetachWithLoadInFlight_CompletesIDs` | `DetachWorld` completes every allocated ID through the sink before it returns, dispatches no lifecycle event and restores nothing, with a playing instance present |
| `Test_CinematicMemory_PressureLevels_Shed` (`Tests/Zenith_CinematicMemory_Tests.cpp`, device) | forced WARNING trims every player's decode-ahead to 2 and counts `cine.video.shed_trimmed_delta`; forced CRITICAL stops invisible players and counts `cine.video.shed_stopped_delta`; every world hash equals the unforced run's; no sequence or pin is released |
| `Test_CinematicPlayback_SpawnableBinding_SpawnsAndReleases` (Stage 5) | a spawnable slot's prefab entity exists from Begin to release |
| `Test_CinematicPlayback_BlendIn_WeightKOverFifteen` | after k ticks the weight is the float nearest k/15; `PLAYING` at k = 15 |
| `Test_CinematicPlayback_ZeroBlendIn_FullWeightFromFirstAdvance` | a start with blend-in 0 enters `PLAYING` at weight 1 and writes the full track value from the tick after Begin |
| `Test_CinematicPlayback_NaturalEnd_BlendsOutThenEnds` | `BLENDING_OUT` at the duration, time held, END dispatched when the weight reaches 0, the end tick written, restore at the next boundary |
| `Test_CinematicPlayback_Loop_WrapsAndFiresKeysOnce` | keys at the wrap fire once per loop, also when the cap splits the wrap; a looping start of a sequence with `D × H` below `ulCINE_MIN_LOOP_SCALED` fails `CINEMATIC_ERROR_INVALID_ARGUMENT`, and at exactly the minimum a rate-8 advance wraps once with `m_iTimeScaled < D·H` |
| `Test_CinematicPlayback_Interrupt_FromPartialWeight` | an interrupt at weight 2/15 falls linearly from 2/15 |
| `Test_CinematicPlayback_InterruptedBlendOut_FiresNothing` | an audio section, a subtitle section and an event key that start inside the blend-out of an interrupted instance: none is recorded or queued; `m_ulNextItemKey` follows `(s_cur + 1) << 26`; the transform track keeps following the advancing time at the falling weight; a `COMPLETED` blend-out is unaffected |
| `Test_CinematicPlayback_InterruptWhileBinding_CancelsSilently` | no lifecycle event, `Ended` with `CANCELLED`, ticket resolves to `CINEMATIC_ERROR_CANCELLED` |
| `Test_CinematicPlayback_Skip_EndStateOnceFireOnSkipOnce` | the end state written once; `fireOnSkip` keys queued in timeline order once; others not |
| `Test_CinematicPlayback_SkipAfterPhase6_EndStateAtBoundary` | a skip recorded in phase 10 writes the arbitrated end state at the boundary |
| `Test_CinematicPlayback_SkipSideTable_RecordedAfterRestore` | a skip with one fire-on-skip command key and one playing audio section: in tick t + 1 both records are played back (the command key, `Audio.StopInstance`), a caption cancel is delivered, and the instance is `RELEASED` |
| `Test_CinematicPlayback_SkipIsTerminal_NoEndEvent` | SKIP dispatched, no END follows |
| `Test_CinematicPlayback_Pause_HoldsTimeAndWeight`, `Test_CinematicPlayback_Resume_RestoresState` | `Resume` returns to the state `Pause` left, at the paused weight (from `BLENDING_IN`, `PLAYING` and `BLENDING_OUT`) |
| `Test_CinematicPlayback_SetRate_Zero_HoldsTime` | |
| `Test_CinematicPlayback_InstanceLimit_StartFailed` | the 17th start fails with `CINEMATIC_ERROR_INSTANCE_LIMIT` |
| `Test_CinematicPlayback_TicketResolves_WindowThenExpires` | pending before playback, resolved for ticks t..t+2, expired after |
| `Test_CinematicPlayback_TicketOverflow_DroppedAndExpires` | a 65th start within the window is dropped, counted, queues no event and its ticket resolves `CINEMATIC_ERROR_TICKET_EXPIRED` from the next tick |
| `Test_CinematicPlayback_BindingErrors_EachCode` | unknown, duplicate, required and type errors with their details |
| `Test_CinematicPlayback_TakeOver_MovesSnapshot` | a second instance binding the same entity takes it over at the boundary (the first writes it through tick t, the second from t + 1, blending from the captured base); the first ends INTERRUPTED through the End path when no slot remains (END once, audio stopped); the final restore returns the pre-first-cinematic value |
| `Test_CinematicPlayback_TakeOver_SnapshotFailure_LeavesHolderIntact` | with the pool full, the second start fails `CINEMATIC_ERROR_SNAPSHOT_LIMIT`; the first still holds and writes the entity, and its restore returns the pre-cinematic value |
| `Test_CinematicPlayback_TakeOver_NoTakeoverFlag_Refused` | |
| `Test_CinematicPlayback_TakeOver_EndedHolder_NotTakenOver` | a start in the end tick t of the holder (from its `Ended` handler in phase 9) takes the holder's pre-cinematic values into its snapshot without marking it and captures the holder's end-tick output as its take-over base at the boundary; both restores write the same bits; an unflagged position does not move between ticks t and t + 1 by more than the track's own per-tick change |
| `Test_CinematicPlayback_BoundEntityDestroyed_DroppedContinues` | [Z2-N-ARCH-556]: the slot dropped at the destroy playback, snapshot entries released, the sequence continues |
| `Test_CinematicPlayback_LastRequiredDestroyed_EndsFailed` | |
| `Test_CinematicPlayback_PrepareTimeout_BeginsOrFails` | both settings of `cine.prepareTimeoutFails` |
| `Test_CinematicPlayback_PlayerComponent_AutostartsOnce` | play-on-start starts once; `Instance` and `State` written (through `GetChunkOfEntity` on the current player); a save and load does not start it again; a failing start writes `State = ENDED` and `LastError` and is never retried; with the phase-1 merge capacity forced below the autostart buffer's demand for one tick, the row stays `NONE` with no player-start entry and autostarts at the next tick; with the ticket table full, the dropped start's entry expires after `uCINE_TICKET_WINDOW_TICKS` and writes `State = ENDED`, `LastError = CINEMATIC_ERROR_TICKET_EXPIRED`, freeing the entry |
| `Test_CinematicPlayback_CommandCap_DefersNotDrops` | an instance exceeding 512 records resumes next tick with every key fired once, in key order |
| `Test_CinematicPlayback_MergeCapacityRejection_RerunsWhole` | with the phase's merge capacity forced below the instance's demand, its buffer is rejected whole, nothing staged is applied, and the next tick re-runs the same items once; a playing seek and a paused seek whose tick buffer is rejected keep the pre-seek time and the pending seek, record nothing committed, and the next tick re-applies the whole seek (`PlaySlot` for entering sections, no jumped key fired), with no advance in the rejected tick |
| `Test_CinematicPlayback_OutboxFull_DefersEvents` | 300 keys in one tick: 256 emitted, 44 next tick, order preserved |
| `Test_CinematicPlayback_OutboxLateEntries_EmittedNextTickOnce` | a start failure played after phase 10 is delivered exactly once, in phase 9 of t + 1; an entry queued after phase 2 is delivered in phase 9 of the same tick |
| `Test_CinematicPlayback_EmitRejected_ReEmittedOnce` | the phase-8 merge capacity forced below one emission group's demand in tick t: that group is rejected (and the instance's later groups with it), counted in `cine.events.emit_rejected_delta`; every entry of that instance is delivered exactly once, in phase 9 of t + 1, in queue order; no other instance's entry is lost or duplicated; the removal at t + 1 removes exactly the accepted positions |
| `Test_CinematicPlayback_OutboxBound_SixtyFourSkipsOneTick` | `cine.maxInstancesPerWorld = 64`: 64 playing instances, each of a sequence with 8 fire-on-skip keys, all skipped in one tick: no assert, the store plus reservations never exceed 960, every event is delivered exactly once; with the phase-8 capacity forced to 0 for 10 ticks, starts are dropped with `cine.commands.outbox_full_delta` and nothing is lost when the capacity returns |
| `Zenith_CinematicSnapshot_Tests.cpp`: `Test_CinematicSnapshot_WrittenAndRestoreSets_Union` | the snapshot holds the written set and every restore-flagged property, in (type, property) order |
| `Test_CinematicSnapshot_RestoreFlagged_BitExact` | after restore the flagged property equals the snapshot bit for bit; the unflagged keeps the end value ([Z2-N-ARCH-555]) |
| `Test_CinematicSnapshot_LimitExceeded_StartFails` | entries, bytes and pool limits; no partial snapshot |
| `Test_CinematicSnapshot_ContainerProperty_RoundTrips` | a restore-flagged container property is snapshotted with `ReadContainer` and restored element for element with `Zenith_TickBoundaryAccess::SetContainer`; with the container heap exhausted at the boundary the entry is skipped and counted in `cine.snapshot.restore_skipped_delta` |
| `Test_CinematicSnapshot_RestoreSkipsRemovedComponent` | a component removed during the cinematic is skipped and counted (`cine.snapshot.restore_skipped_delta`); a component added after Begin is not restored |

## Track tests

File `Tests/Zenith_CinematicTracks_Tests.cpp`.

| Test | Asserts |
|---|---|
| `Test_CinematicTracks_Transform_WinsArbitration` | a cinematic write beats physics and animation writers of `Zenith.LocalTransform` in phase 6 |
| `Test_CinematicTracks_Transform_SpacesParentWorldAnchor` | each space produces the expected local transform for a parented entity, from the committed `Zenith.WorldTransform` |
| `Test_CinematicTracks_Transform_UndrivenChannelsUntouched` | |
| `Test_CinematicTracks_Camera_CutIncrementsCutRequest` | a cut key, a seek across a cut, and no cut at Begin without `cutIn`; the last evaluated cut index lives in the cut channel's late-cache entry and survives a save, so a seek across a cut after a load counts exactly one |
| `Test_CinematicTracks_Camera_LensAndLightingByName` | FovY, focus distance, aperture reach their properties via phase-10 commands; the driven CameraLighting IDs are exactly 1, 3–13, 16–20; absent types are a counted no-op |
| `Test_CinematicTracks_Late_UnchangedValueRecordsNothing` | a value equal to the late cache records nothing; a rejected ApplyLate buffer records the value again next tick |
| `Test_CinematicTracks_Animation_PlaySlotRecordedPhase1` | `Animation.PlaySlot` with the cinematic tag, blend-in, start time; visible to phase 3 of the same tick (recording ANIM log) |
| `Test_CinematicTracks_Animation_RateChange_ResendsSlotTime` | |
| `Test_CinematicTracks_Animation_PhysicsWeight_CinematicSource` | `Zenith.Animation.Pose.PhysicsWeight` written with the cinematic source |
| `Test_CinematicTracks_Audio_PostEventTimeLockedSubTick` | the payload's flags, tick, sub-tick offset and instance ID |
| `Test_CinematicTracks_Audio_PauseStopsResumeReposts` | stop on pause and on a rate other than 1, re-post (a fresh ID) and seek on resume and on rate 1; a non-time-locked voice keeps playing through the pause; a `stopAtEnd = false` voice past its section's end is re-posted from the audio list at resume; a time-locked dialogue section that starts at rate 0.5 joins the audio list with bit 2 and posts nothing, and the return to rate 1 posts it with a fresh ID sought to `(s_cur − sectionStart)`; a non-time-locked section starting at 0.5 posts at once |
| `Test_CinematicTracks_Audio_PauseResumeWithinFade_Replays` | with the offline Audio module: a pause and a resume in one tick, and a rate 1 → 2 → 1 within 50 ms, each stop the voice and re-post it with a new ID; `audio.instances.id_collisions_delta` stays 0 and the voice is audible after the resume |
| `Test_CinematicTracks_Camera_EndTickLensLeftToDirector` | with GAME's director on the camera entity, the end tick records no CINE `SetProperty` of `Zenith.Render.Camera` 4, 6, 7 or CameraLighting; the director's phase-10 values are the ones extracted in the end tick |
| `Test_CinematicTracks_Event_FiresInPhase9SameTick` | an event key in (s_prev, s_cur] is delivered in phase 9 of the tick |
| `Test_CinematicTracks_Dialogue_PrepareLeadThenPlay` | `PrepareDialogue` at start − lead with its prepare record, `PlayDialogue` time-locked at start, caption cue queued with the duration in ticks |
| `Test_CinematicTracks_Dialogue_LipSyncWindowByVoiceLocale` | a dialogue face clip with three windows (`en`, `fr`, `de`): with `m_xVoiceLocale` `fr`, `PlaySlot` on `cine.face` carries start time T_fr and tag `ANIM_SLOT_TAG_CINEMATIC \| ANIM_SLOT_TAG_FULL_RATE`, and the window-end item records `StopSlot` (0.1 s) at `sectionStart + (e_fr − T_fr)` in phase 1 while the other windows' items record nothing; `fr-CA` resolves to `fr`, `pt` and the empty ID to window 0; a save and load mid-line keeps the window; a face clip with no lip-sync marker plays from 0 for the section |
| `Test_CinematicTracks_Subtitle_CaptionCueFields` | every `Zenith_UICaptionCue` field and the source ID |
| `Test_CinematicTracks_Subtitle_SeekReEmitsRemaining` | a seek into a section cancels and re-emits the cue with the remaining duration |
| `Test_CinematicTracks_Subtitle_PauseSeekSkipKeepsCaptionInSync` | a pause cancels the cue and the resume re-queues the remaining duration; a rate change 1 → 2 cancels and re-queues half the remaining ticks (`REQUEUE_CAPTIONS`), 2 → 0 cancels without a re-queue, 0 → 1 re-queues; a seek out of the section cancels it; a skip, an interrupt and an end each leave no CINE cue in UI's queue; the caption list's expiry equals UI's `m_ulStartTick + duration` for every listed cue; a skip-fired cue (bit 51) survives the skip's own cancel |
| `Test_CinematicTracks_Property_CurveAndStep` | |
| `Test_CinematicTracks_Command_ByNameAndPayload` | a test module's kind recorded with a reflected payload |
| `Test_CinematicTracks_Command_MissingKind_DisabledOnce` | |
| `Test_CinematicTracks_Overlay_ExtractedOnly` | overlay records in the render world, nothing in simulation |
| `Test_CinematicTracks_Marker_EventAndFind` | the marker event carries the interned name and index; `FindMarker` finds it by name |
| `Test_CinematicDiagnostics_CountersPublished` (`Tests/Zenith_CinematicDiagnostics_Tests.cpp`) | every counter of the entry file's "Counters" table is published per world with the expected values over a scripted run |
| `Test_CinematicDiagnostics_ConsoleCommandsRecordRequests` | `cine.play`, `cine.skip`, `cine.seek` record their commands through the world's input-command stream at the next tick and appear in the replay log; while the world records or replays a command that cannot be recorded refuses itself |

## Blending and seeking tests

File `Tests/Zenith_CinematicBlend_Tests.cpp`.

| Test | Asserts |
|---|---|
| `Test_CinematicBlend_InFromSnapshot` | written values are `lerp(snapshot, track, w)` during the blend-in |
| `Test_CinematicBlend_OutUnflaggedHolds` | an unflagged property holds `lerp(B_in, C, w₀)` (C for a natural end) through the blend-out; no jump at END |
| `Test_CinematicBlend_InterruptDuringBlendIn_UnflaggedNoPop` | an interrupt at 2/15: every tick's unflagged value differs from the previous one by no more than the track's own per-tick change |
| `Test_CinematicBlend_OutFlaggedReachesSnapshot` | a flagged property is written at the snapshot bits in the end tick (asserted bit for bit), no non-cinematic source wins that row in the end tick, and the restore writes identical bits |
| `Test_CinematicBlend_TakeOverBase_IsCommittedValue` | the taking instance blends in from the value the holder wrote in the take-over tick |
| `Test_CinematicBlend_PauseDuringBlendOut_NoPop` | a pause at blend-out weight 0.6 of an interrupted instance: every written value (flagged and unflagged) is identical in the tick before the pause, every paused tick and the tick after the resume, apart from the blend-out's own per-tick change; an interrupt while so paused is ignored and keeps w₀ |
| `Test_CinematicBlend_TakeOverInterruptDuringBlendIn_FlaggedNoPop` | a taken-over entity (B_in ≠ B) interrupted at weight 2/15 of N's blend-in: its flagged value at the first blend-out tick differs from the last blend-in tick by no more than the blend's own per-tick change, falls to B and is written as B in the end tick |
| `Test_CinematicSeek_Forward_SetsSlotTimesNoNotifies` | `SetSlotTime` for continuing sections, `PlaySlot` for entering ones, `StopSlot` for leaving ones |
| `Test_CinematicSeek_FireEventsFlag_RespectsFireOnSeek` | with and without the flag; backwards seeks fire none |
| `Test_CinematicSeek_TwoSeeksBeforeEvaluate_LaterWins` | |
| `Test_CinematicSeek_MicrosecondExact` | `Zenith_RecordSeekCinematicUs` to a marker's time lands on it exactly at a one-hour duration |
| `Test_CinematicScrub_PausedMatchesPlayedTick` | the values written at a scrubbed time equal those written by a played tick at the same time; two consecutive scrubs while paused each apply in the next tick (seek-pending cleared, `m_iTimeScaled = g × H`, `SetSlotTime` recorded, no audio post, no caption cue), and a resume after them re-posts the audio and re-queues the captions at the second target |

## Determinism and save tests

File `Zenith2/Tests/Cinematics/Zenith_CinematicDeterminism_Tests.cpp` (integration). Every save test saves at the point named, loads, runs N ticks and asserts that every per-tick world hash equals the uninterrupted run's (Q70 (a), Q82 (b)); UI's caption queue, which UI does not save, is compared by its visible cues (key and expiry tick). The save fixtures place every caption section and every seek target on a tick boundary of the test's 60 Hz, where a re-queue after the load computes the same expiry tick as the original cue, so the comparison is exact.

| Test | Asserts |
|---|---|
| `Test_CinematicDeterminism_WorkerCounts_IdenticalHashes` | the TwoCharacters playthrough with 1, 2, 4 and 8 workers yields identical per-tick world hashes |
| `Test_CinematicDeterminism_Replay_ReproducesSkipAndSeek` | a recorded run with a skip, a seek and a rate change replays to identical hashes |
| `Test_CinematicDeterminism_LoadPublicationTick_FromLog` | a binding completion published at tick N in the record is published at N in the replay, whatever the load time |
| `Test_CinematicSave_MidBlendRoundTrip` | save at weight 7/15, run 60 ticks |
| `Test_CinematicSave_DuringBinding_ReissuesLoad` | the recorded completion ID is re-issued and marked |
| `Test_CinematicSave_SkipSideTable_Survives` | a save between a skip playback and the next Evaluate records the side work once after load |
| `Test_CinematicSave_SkipAfterPhase6_EndStateSaved` | a skip played after phase 6 then a save in phase 11: the boundary after load writes the end state |
| `Test_CinematicSave_TakeOverMidBlend_RoundTrip` | a save in the take-over tick (base capture pending) and one mid blend-in of the taking instance |
| `Test_CinematicSave_OutboxLateEntries_Survive` | an entry queued after phase 10 then a save: emitted once in phase 8 of the next tick after load |
| `Test_CinematicSave_PausedState_Restored` | a save while paused from `BLENDING_IN`: `Resume` after load returns to `BLENDING_IN` at the paused weight |
| `Test_CinematicSave_PrepareRecords_Restored` | a save while preparing a dialogue line: begins at the same tick after load |
| `Test_CinematicSave_PlayerLink_Restored` | a save of a player-started instance: the player row keeps `Instance` and `State` after load |
| `Test_CinematicSave_MidDialogue_AudioRepostedAfterLoad` (with the offline Audio module) | the dialogue line and the time-locked music are re-posted with fresh IDs (the next post serials) and sought to the sequence time in the first tick after load; their caption is re-queued with the remaining duration; an instance saved while paused re-posts its time-locked voices only at its resume, and a non-time-locked voice at the first tick; an instance saved `PAUSED` re-posts its non-time-locked (bit-3) voices in the first tick after the load and its time-locked ones and captions only at its resume |
| `Test_CinematicSave_InterruptTick_AudioStaysStopped` (with the offline Audio module) | a save in the tick of an interrupt (side work `STOP_AUDIO` and, after the load, `RESUME_PRESENTATION` both pending): after the load the music is not re-posted and no `Audio.StopInstance` of a lost voice is recorded; the world hashes equal the uninterrupted run's |
| `Test_CinematicSave_BoundSet_StartBeginsInlineAfterLoad` | a sequence bound through a still-held `RELEASED` instance record, then a save: a start of the same sequence after the load begins inline at its playback, in the same tick as in the uninterrupted run |
| `Test_CinematicSave_EmitRejectedEntries_Survive` | a save in a tick whose phase-8 merge rejected an emission group: after the load the rejected entries are emitted once in tick t + 1 and nothing accepted is emitted twice |
| `Test_CinematicSave_AcceptedCueExpiry_Applied` | a 1-tick caption cue accepted in the save tick: after the load its caption-list entry carries the expiry `t + 1`, is removed at t + 1, and a later `PAUSE_PRESENTATION` emits no cancel for it, exactly as in the uninterrupted run |
| `Test_CinematicCaptions_InterruptThenPauseResume_NoResurrection` | an interrupt cancels a playing subtitle; a pause and resume during the interrupted blend-out re-queue nothing; a load of the instance in its interrupted blend-out re-queues nothing; a pause and resume of an uninterrupted instance re-queues exactly the captions it had shown (cancelled by presentation), with the remaining duration |
| `Test_CinematicSave_ContentChanged_EndsFailedRestores` | a re-cooked sequence at load: END dispatched once with `FAILED`, GAME's director resumed, the snapshot restored |
| `Test_CinematicSave_IsRestoreReady_WaitsResidency` | |

## Script tests

File `Zenith2/Tests/Cinematics/Zenith_CinematicScript_Tests.cpp`.

| Test | Asserts |
|---|---|
| `Test_CinematicScript_StartResolveEnded` | a Luau behaviour starts a sequence, resolves the ticket next tick, receives `Ended` |
| `Test_CinematicScript_StartFailedEvent_CarriesError` | `GetStartError` returns the code name |
| `Test_CinematicScript_Misuse_EachCode` | each row ZS2701 … ZS2712 raises its code with its message tail (codes read from the catalogue at test time, so the test pins no number) |
| `Test_CinematicScript_MarkerAndTagMatchInLuau` | a behaviour matches a marker by name (`event.Marker == "Shot02"`), finds an instance by tag string, and matches `Started` to its ticket with `SameTicket`; no ZS1504 is raised |
| `Test_CinematicScript_StaleInstance_NotMisuse` | a stale instance records and is dropped at playback |
| `Test_CinematicScript_RequestsAreTransactional` | a failing invocation's `Cinematics.Start` is rolled back with the transaction |

## Video tests

All in `ZenithCinematics_Tests` with the test decoder through the module seam and the video thread in test mode (`PumpOnce` on the test's thread), except where noted.

| File | Test | Asserts |
|---|---|---|
| `Tests/Zenith_VideoPlayer_Tests.cpp` | `Test_VideoPlayer_StateMachine_EveryTransition` | each row of "Video players"; including `CLOSED` arriving in `FAILED` (→ `FREE`), absence of draw records in `OPENING` and `SUSPENDED` (→ `CLOSING`), and the seek trigger reading `m_xPending`'s pts (no seek before the epoch's first frame) |
| | `Test_VideoPlayer_FifthPlayer_Refused` | |
| | `Test_VideoPlayer_SeekBackward_KeyframeThenDiscard` | lands on the target frame; a seek and an OPEN to a media time between two frames push first the frame due at m (p ≤ m) and present it at once; a paused scrub with constant m shows that frame |
| | `Test_VideoPlayer_StreamErrors_ThreeFail` | |
| | `Test_VideoPlayer_SuspendResume_Seeks` | `OnStopAudio` and `OnResumeAudio` of the frame listener suspend and resume through the lifecycle ring |
| | `Test_VideoPlayer_DeviceLoss_RecreatesThenFails` | one reported device loss recreates the stream; a second within 2 s fails the player |
| | `Test_VideoPlayer_PausedRenderWorld_KeepsPlayerOpen` | a repeated render world of a paused world keeps the player `RUNNING` |
| | `Test_VideoPlayer_BackwardSeekFullPool_NoDeadlock` | with the pool full (frame ring at the decode-ahead limit, the look-ahead slot valid), a backward seek drains the ring and the slot into the return ring, increments the epoch, reaches `RUNNING` at the target frame, and every buffer index is returned exactly once; a forward seek presents no frame of the old epoch |
| | `Test_VideoPlayer_SlotReuse_FreshRings` | a player slot closed and reopened constructs new frame and return rings (no second `Init` of a ring) and its old frames never reach the new player |
| | `Test_VideoPlayer_StateChangesOnlyFromEvents` | OPENED, OPEN_FAILED, CLOSED, SUSPENDED, RESUMED, DEVICE_LOST and FAILED each reach the player table through the event ring and change the state exactly as "Video players" says; the video thread never writes the player table |
| `Tests/Zenith_VideoFrameSelection_Tests.cpp` | `Test_VideoFrameSelection_NewestAtOrBefore` | the look-ahead slot: superseded frames released into the return ring, the newest at or before m returned once |
| | `Test_VideoFrameSelection_SingleRelease` | over 1,000 frames with skips, repeats and seeks, every buffer index is pushed into the return ring exactly once per decode (by selection, Flux's `ReleaseFrame` or the seek drain), and `AcquireFrame` returns false while no newer frame is ready |
| | `Test_VideoFrameSelection_Late_HoldsLastFrame` | counted |
| `Tests/Zenith_VideoThread_Tests.cpp` | `Test_VideoThread_SpscHandOff_Stress` (`z2-stress`, `win-asan`, a real thread) | 10,000 frames across the frame and return rings, each received once in order, every buffer returned, no sanitizer report |
| | `Test_VideoThread_CommandRingFull_Retries` | a full command ring keeps the command for the next frame and counts `cine.video.command_full_delta`; the ring's dropped count stays 0 |
| | `Test_VideoThread_CloseOfUnopenedStream_AnswersClosed` | a CLOSE of a stream that failed to open, was already closed or never opened pushes `CLOSED` without calling `CloseStream`; OPEN initialises the free list with every pool index |
| | `Test_VideoThread_DrainAtEnd_ReceivesHeldFrames` | with the test decoder at reorder depth 2, `Drain` is called once after the last access unit, `Receive` returns the two held frames, then the stream is at end; a seek after the drain flushes and decodes again |
| | `Test_VideoThread_RingCapacity_PowerOfTwo` | `decodeAheadFrames` 6 gives a frame ring of 8 and never more than 6 frames in it, a pool of 9 and a return ring of 16; at WARNING never more than 2 |
| | `Test_VideoThread_EventRing_NeverFull` | with the render-submit side not draining for 10 frames while 64 commands and a SUSPEND are queued, the video thread stops popping work when fewer than 5 event entries are free, pushes no event into a full ring, and resumes when drained; no event is lost |
| `Zenith2/Tests/Cinematics/Zenith_VideoSync_Tests.cpp` (integration, device) | `Test_VideoSync_OfflineOffsetWithinOneFrame` | presented frame time versus offline-rendered audio position ≤ one frame period + 5 ms for every tick of a 10 s clip |
| `Tests/Zenith_VideoIsolation_Tests.cpp` (device) | `Test_VideoIsolation_DecodeStallChangesNoWorldHash` | a decoder stalled for 30 frames leaves every world hash identical to a run without video |
| `Engine/ZenithFlux/Tests/Video/Flux_VideoConvert_Tests.cpp` (device) | `Test_FluxVideoConvert_Bt709Limited_Reference` | NV12 test planes convert to the reference RGBA16F within 1/1024 (CPU reference implemented in the test) |
| | `Test_FluxVideoConvert_P010Bt2020Pq_Reference` | |
| | `Test_FluxVideoConvert_I420MatchesNv12` | |
| | `Test_FluxVideoConvert_CropRegionOnly` | a 1920 × 1088 frame with a 1920 × 1080 crop converts exactly the crop; the 8 extra rows never appear |
| `Flux_VideoComposite_Tests.cpp` (device) | `Test_FluxVideoComposite_FitModes` | letterbox, fill, stretch rectangles |
| | `Test_FluxVideoComposite_OverlayFadeAndLetterbox` | |
| | `Test_FluxVideoComposite_EncodeOutputSdrHdrConsistent` | an SDR video's encoded value matches UI's encoding on SDR and scRGB outputs |
| | `Test_FluxVideoComposite_NoVideo_NoPassRecorded` | |
| `Flux_VideoSurface_Tests.cpp` (device) | `Test_FluxVideoSurface_FindByName_NullWithoutFrame` | |
| | `Test_FluxVideoSurface_DeviceLoss_Recreated` | |
| | `Test_FluxVideoUpload_RingFull_SkipsFrame` | a frame larger than the ring's remaining budget is skipped, the previous frame stays, the counter increments |

Platform decoder conformance (`Tests/Zenith_VideoDecoderPlatform_Tests.cpp`, labelled `requiresHardwareVideo`; in CI they are skipped-as-not-run and reported as such, never as passed; their recorded run on the reference desktop and the S24 is `needs-human` evidence of the acceptance and D-SEL-VIDEO ruling tickets): `Test_VideoDecoderPlatform_H264High1080p_DecodesAllFrames` (with the crop applied; on Windows the activated transform is synchronous — `MF_TRANSFORM_ASYNC` absent or FALSE — and an asynchronous entry of the enumeration is skipped, and the caps report whether the DXGI device manager was accepted), `Test_VideoDecoderPlatform_Seek_LandsOnTarget`, `Test_VideoDecoderPlatform_UnsupportedProfile_Refused`, `Test_VideoDecoderPlatform_Android10Bit_Refused`.

## Capture tests

File `Zenith2/Tests/Cinematics/Zenith_CineCapture_Tests.cpp` (integration, development configuration, device: WARP or lavapipe renderer with a hidden window, the offline audio backend, `sim.ticksPerFrame = 1`, a test development file writer that applies DEVT's limits and can fail a completion).

| Test | Asserts |
|---|---|
| `Test_CineCapture_StateMachine_EveryTransition` | |
| `Test_CineCapture_RequiresCaptureConfiguration` | `CINEMATIC_ERROR_CAPTURE_STATE` in real-time mode, with an audio backend other than offline while audio is requested, and with `cine.capture.enabled` false |
| `Test_CineCapture_OneFramePerTick_NoGaps` | 120 ticks give frames 0..119 |
| `Test_CineCapture_ReadbackPoolFull_PausesAndRetries` | no tick missing, no duplicate frame, the world advances no tick while the frame is pending |
| `Test_CineCapture_DeferredFrame_RetriedNoGap` | a forced `FLUX_READBACK_ERROR_FRAME_DEFERRED` and a forced `FRAME_FAILED` retry the same tick with the gate held; frames stay contiguous |
| `Test_CineCapture_AudioFramesMatchTicks` | 1,000 ticks at 60 Hz give 800,000 audio frames across the chunks; held frames render none |
| `Test_CineCapture_SubtitlesFrameIndexed` | cues and cancels carry their frame indices; the file carries keys and no text; with one emission group rejected in tick t, its cues are logged once, at the frame of t + 1 |
| `Test_CineCapture_VideoSectionStart_WaitsForFirstFrame` | a capture across a video section's start: the frames of the ticks while the player is `OPENING` are held (`cine.capture.video_wait_delta`) until its first frame is ready, then captured with the frame for their media time; a seek inside the section waits likewise; the captured frames are identical with a decoder stalled for 20 frames; a section whose media times fall between video frames (30 fps content at 60 Hz) and a seek to an off-frame target are captured without waiting beyond the decode of the due frame |
| `Test_CineCapture_VideoWait_FailsAfterLimit` | a player that never becomes ready fails the capture with `CINEMATIC_ERROR_CAPTURE_STATE` after 300 held frames, releasing the gate |
| `Test_CineCapture_BeginWithoutWriter_Refused` | `Begin` with a null development file writer returns `CINEMATIC_ERROR_CAPTURE_STATE` |
| `Test_CineCapture_WriteFailure_Failed` | a refused write and a completion failure (the writer's failure count rising) each move the capture to `FAILED` |
| `Test_CineCapture_AbsentInShipping` (`ShippingTest` configuration) | the module holds no capture and the `cine.capture.*` keys are not registered |

## Export tests

File `Zenith2/Tools/Authoring/Tests/Cinematics/Zenith_CineExport_Tests.cpp`.

| Test | Asserts |
|---|---|
| `Test_CineExport_PngAndVttDeterministic` | two exports of a generated capture are byte-identical; PNG decodes to the TGA pixels; the WAV is the chunks behind the specified header |
| `Test_CineExport_WebVttTimes` | cue times from frames at 60 and 30 fps |
| `Test_CineExport_WebVttTextResolved` | with a generated `Localization/en-GB/` of two `.strings.json` documents, each cue's text is its key's `text`; a missing key writes the key and is listed in `export.json`'s `unresolvedKeys`; an empty directory writes every key |
| `Test_CineExport_AudioAboveWavLimit_Refused` | a manifest whose `audio.frames` exceeds `ulCINE_CAPTURE_MAX_AUDIO_FRAMES` is refused with `CINEMATIC_ERROR_CAPTURE_IO` before any audio chunk is read |
| `Test_CineExport_CorruptManifest_Refused` | a bad manifest, a missing frame and a short audio chunk each fail with `CINEMATIC_ERROR_CORRUPT` naming the item |
| `Test_CineExport_EncodeWithoutAdmittedEncoder_Refused` | `CINEMATIC_ERROR_VIDEO_UNSUPPORTED` |
| `Test_CineExport_ExportJsonWrittenLast` | an interrupted export has no `export.json` |
| `Test_CinematicExtraction_YieldsCaptionKeys` | `Zenith_ListCinematicCaptionKeys` yields every subtitle, speaker and dialogue caption key with its JSON pointer |

## Editor tests

Files `Zenith2/Tools/ZenithEditor/Tests/Cinematics/Zenith_EditorSequencer_Tests.cpp` (model only, no ImGui).

| Test | Asserts |
|---|---|
| `Test_EditorSequencerModel_EveryCommand_UndoRoundTrip` | each command of "Sequencer commands" applies and undoes to byte-identical documents |
| `Test_EditorSequencer_ExtensionRegistersHandlers` | the extension registers the panel type, the document handler, the kind 14 asset handler and the player inspector; a duplicate is refused |
| `Test_EditorSequencerModel_RenameBinding_RewritesPlayers` | the scene's players are rewritten in the same batch |
| `Test_EditorSequencerModel_ScrubRecordsSeek` | playhead moves record one microsecond seek each into the preview world |
| `Test_EditorSequencerModel_RecordCameraKey_FromViewport` | |
| `Test_EditorSequencerModel_CookDiagnostics_SelectMember` | a cook finding's JSON pointer selects the member |
| `Test_EditorSequencer_AuthoringWalkthrough_Automation` | the walkthrough of `Zenith2/Docs/Design/CINE/Authoring.md` → "Authoring walkthrough" (steps 1–9) replays through editor automation and produces a document that cooks and plays |

## Integration tests

File `Zenith2/Tests/Cinematics/Zenith_CinematicIntegration_Tests.cpp`.

| Test | Asserts |
|---|---|
| `Test_CinematicIntegration_AnimSlotsRealModule` | with the real Animation module, the bound skeleton's pose follows the clip; on skip the inertial take avoids a pose jump (ANIM's assertion of its own listener) |
| `Test_CinematicIntegration_AudioRealModuleOffline` | with the real Audio module on the offline backend, the posted voices start at the sub-tick offsets and stop at skip |
| `Test_CinematicIntegration_CameraDirectorSuspends` | GAME's director suspends at START, resumes at END or SKIP, and blends from the cinematic camera's final pose |
| `Test_CinematicIntegration_PhysicsControllerAdoptsPose` | a controller character bound by a transform track ends where the track ends, without a teleport |
| `Test_CinematicIntegration_CaptionReachesUI` | a subtitle cue is presented by UI in the tick it fires and removed by its cancel |
| `Test_CinematicIntegration_VfxSpawnFromCommandTrack` (Stage 5, with the VFX module) | a `VFX.Spawn` command key spawns its effect at the key's tick with the reflected payload |

GAME owns `Test_Lifecycle_Cinematic_SkipRestoresGameplayState`, `Test_Lifecycle_Cinematic_InterruptBlendsOut` and `Test_Scenario_SkipCinematicMidBlend` (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"); CINE's implementation tickets list them as the acceptance they must pass and do not duplicate them.

## Benchmarks

| Benchmark | Measures | Row |
|---|---|---|
| `Zenith_Bench_CinematicEvaluate` | the per-tick CPU of the five systems over the CinematicSlice's heaviest sequence (8 bound characters, 120 tracks), workload W-Cinematic | `Z2-B-CINE-002` |
| `Zenith_Bench_VideoDecode` | video thread CPU per frame and decode-ahead memory, 1080p30 | CN12 pending rows, `Z2-B-CINE-001` |
| `Zenith_Bench_FluxVideoComposite` | GPU time of `VideoConvert` and `VideoComposite` at 1080p | CN12 pending row |

## Fixtures

Every binary fixture is generated at test time by `Zenith_CinematicFixtures.h` into `${ZENITH_OUT}/test-fixtures/CINE/` (setup case `CINE_CookedFixtures`, suite `z2-cine-cooked`, variable `ZENITH2_CINE_COOKED_FIXTURES`, [Z2-N-ARCH-257]):

| Fixture | Content |
|---|---|
| `TwoCharacters` | the document of `Zenith2/Docs/Design/CINE/Formats.md` → "Example document" and its cooked bytes; GAME's suite keeps its own committed copy of the document text and cooks it through ZenithCook in its own fixture setup (row CN7 (d)), so GAME's suite needs no include of CINE's `Tests/Support/` (`ZenithCook_Tests`, a dependent library's tests, may include it, [Z2-N-ARCH-255]) |
| `AllTracks` | one track of every kind, with sections and keys at tick boundaries and between them |
| `Limits` | documents at every limit and at limit + 1 |
| `TestCodecClip` | a valid `.zvid` labelled `VIDEO_CODEC_H264` with a minimal `avcC` record, whose length-prefixed access units carry a 16-byte test payload (frame index, presentation time, a pixel seed) that only `Zenith_TestVideoDecoder` understands and expands into planes encoding the frame index, stored at 1920 × 1088 with a 1920 × 1080 crop; 10 s at 30 fps, keyframe every 30 frames, reorder depth 2 (the loader validates framing, never NAL contents, so the file passes V1–V7 unchanged) |
| `SyntheticMp4` | an MP4 wrapping `TestCodecClip`'s frames in an `avc1`-shaped entry, with a 48 kHz PCM `sowt` audio track (generated in `ZenithCook_Tests`' own setup from the same builder values) |
| `Capture120` | a capture directory of 120 generated frames, two audio chunks and subtitles |

## Gate steps

| Step | CINE work it runs |
|---|---|
| `Build` (`win-debug`, `win-release`, `android-arm64`) | every CINE target the preset builds |
| `Test` | every target above except the hardware-labelled and benchmark ones; the device cases on WARP or lavapipe |
| `Sanitizers -Preset win-asan` | `ZenithCinematics_Tests` (the video ring stress among them), the two fuzz targets; `linux-tsan` does not build ZenithCinematics (`ZENITH_GPU`), and the ring's TSan coverage is CORE's |
| `ApiContractCheck`, `HeaderSelfContained` | every public header of `Zenith2/Docs/Design/CINE/PublicAPI.md`, `Zenith_AuthoringCinematics.h` among them |
| `ForbiddenCheck` | no ZenithAudio, ZenithVFX, ZenithUI or ZenithGameplay include in ZenithCinematics; third-party (Media Foundation, NDK media, FFmpeg) only under `Internal/<Platform>/` or `Internal/FFmpeg/` |
| `Libgraph` | ZenithCinematics' declared dependencies; ZenithAuthoring's CINE edges |
| `SourceLayout` | CINE's ZenithAuthoring files only in `Tools/Authoring/Internal/Cinematics/` and the root header |
| `ShaderChecks` | `Zenith2/Shaders/Video/*.slang` |
| `BackendParity` | the Flux video tests on Vulkan and D3D12 |
| `Coverage` | CINE's requirement IDs cited by these tests |
