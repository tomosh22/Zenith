# CINE requirement catalogue

## Purpose

This catalogue lists every testable obligation of the CINE area (cinematics, the sequencer, video, capture and export), one requirement per obligation, each with the inventory items it covers, its stage, classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design family is `Zenith2/Docs/Design/CINE.md` and `Zenith2/Docs/Design/CINE/*.md`; the inventory is `Zenith2/Docs/Requirements/Inventory.md` (items Z2-I-CINE-01 to Z2-I-CINE-16, all required at Stage 4). CINE also owns the implementation half of GAME's cinematic lifecycle items Z2-I-GAME-28, Z2-I-GAME-29 and Z2-I-GAME-30, whose contract and tests GAME owns.

## Requirements

### Z2-R-CINE-001 Module entry points and registration

- **Statement:** `Zenith_Module_Cinematics_RegisterTypes` registers exactly the types, events, resource and configuration keys of the CINE public API, and `Zenith_Module_Cinematics_Start` registers the two kind loaders, the eleven command kinds, the binding table, the memory consumer and (with a renderer) the video path, each failure returning its module error after unwinding.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_RegisterTypes_RegistersEveryType`, `Test_CinematicsModule_Start_RegistersElevenKinds`, `Test_CinematicsModule_Start_FailsAtStep1_Unwinds` … `Step7`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"

### Z2-R-CINE-002 World attach and detach

- **Statement:** `AttachWorld` creates one world record, resolves the command kinds, adds the world resources `Zenith.Cinematics.World` (owned by `PHASE_INPUT`) and `Zenith.Cinematics.Emission` (owned by phase 8), declares the asynchronous source with its handler on the world's sink, adds the five systems, the listener, the tick-boundary handler, the participant and the extractor, unwinding on any failure; `DetachWorld` frees every instance without lifecycle event and without restore, completes every allocated completion ID and frees the record.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** `Test_CinematicsModule_Attach_FailsAtStep1_Unwinds` … `Step4`, `Test_CinematicsModule_Attach_DeclaresSourceAndHandler`, `Test_CinematicPlayback_DetachWithLoadInFlight_CompletesIDs`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"; `Zenith2/Docs/Design/CINE/Playback.md` → "World transition and shutdown"

### Z2-R-CINE-003 Headless operation

- **Statement:** Without a renderer the module creates no decoder and no thread, runs every system, and writes no render-world record.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_Headless_NoVideoNoThread`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ExtractRender"

### Z2-R-CINE-004 Configuration keys validated

- **Statement:** Every `cine.*` key is registered with its type, default and range, a value outside its range is a configure error naming the key, and the simulation-affecting keys are recorded in the replay header.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_ConfigKeys_RangesValidated`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Configuration keys"

### Z2-R-CINE-005 Public headers carry contracts

- **Statement:** Every CINE public header is self-contained, every fallible function returns `Zenith_Result`, and every public type carries one contract block.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps over the headers of `Zenith2/Docs/Design/CINE/PublicAPI.md`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Rules across the CINE API"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Scope of this file"

### Z2-R-CINE-006 No forbidden module includes

- **Statement:** `ZenithCinematics` includes no ZenithAudio, ZenithVFX, ZenithUI, ZenithGameplay or ZenithPhysics header and links only the dependencies of its row.
- **Covers:** Z2-I-CINE-04, Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` and `Libgraph` gate steps
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Library dependencies"

### Z2-R-CINE-007 Error enumeration

- **Statement:** `Zenith_CinematicError` is the struct form of [Z2-N-CONTRACT-480]: a `Zenith_CinematicErrorCode` of the 25 `CINEMATIC_ERROR_*` values with fixed meanings and a 16-bit detail, and the `constexpr` `Zenith_ToString` returns each value's name.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_BindingErrors_EachCode`, `Test_CinematicScript_StartFailedEvent_CarriesError`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicErrors header"

### Z2-R-CINE-008 Command kinds registered with phase masks

- **Statement:** The eleven `Cinematics.*` kinds are registered with the payload types, maximum payload sizes and allowed phase masks of the kind table, and their payloads serialise by reflection.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_CinematicsModule_Start_RegistersElevenKinds`, `Test_CinematicCommands_StartPayload_RoundTripsByReflection`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Command payloads"

### Z2-R-CINE-009 Lifecycle recorders take the kind table

- **Statement:** `Zenith_RecordStartCinematic`, `Zenith_RecordInterruptCinematic` and `Zenith_RecordSkipCinematic` take the world's `Zenith_CinematicCommandKinds` and each append exactly one record, never failing beyond buffer capacity.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-14, Z2-I-CINE-15, Z2-I-GAME-28 (GAME), Z2-I-GAME-29 (GAME), Z2-I-GAME-30 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_CinematicCommands_StartPayload_RoundTripsByReflection`; GAME's `Test_Lifecycle_Cinematic_SkipRestoresGameplayState`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicCommands header"

### Z2-R-CINE-010 Tickets unique and resolvable

- **Statement:** A ticket is four `uint32` fields: the halves of `Zenith_Hash64` of the 20 little-endian bytes of its recording buffer's base merge key with the record count as sequence, the sequence, and the recording tick's low 32 bits; no two starts share a ticket, and `ResolveTicket` returns PENDING in the recording tick before playback, the instance ID from playback until the end of tick t + 2, and EXPIRED after or when its start was dropped because the 64-entry ticket table was full.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCommands_TicketFromMergeKey_Unique`, `Test_CinematicPlayback_TicketResolves_WindowThenExpires`, `Test_CinematicPlayback_TicketOverflow_DroppedAndExpires`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicTypes header"; `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"

### Z2-R-CINE-011 Start payload validated as untrusted

- **Statement:** The start playback rejects a payload with the overflow bit, an unknown flag, a rate above 8, a blend above 60 s, the sentinel a NaN or infinite float records, or a null sequence, records the error in the ticket entry and queues `Zenith.Cinematics.StartFailed`, never asserting.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCommands_SixtyFiveBindings_OverflowRejected`, `Test_CinematicCommands_NonFiniteFloats_RejectedAtPlayback`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"

### Z2-R-CINE-012 Internal kinds refused from other systems

- **Statement:** A `Cinematics.Begin` or `Cinematics.End` record, or a start flagged `uCINE_START_FLAG_FROM_PLAYER`, whose merge key's system order is not the one `Cinematics.Evaluate` stores in the world record is dropped and counted in `cine.commands.rejected_delta`.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_InternalKinds_RefusedFromOtherSystems`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Command payloads"

### Z2-R-CINE-013 Instance limit

- **Statement:** A start beyond `cine.maxInstancesPerWorld` live instances fails with `CINEMATIC_ERROR_INSTANCE_LIMIT` through the ticket and `StartFailed`.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** `Test_CinematicPlayback_InstanceLimit_StartFailed`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"; `Zenith2/Docs/Design/CINE.md` → "Capacities and constants"

### Z2-R-CINE-014 Cinematic document schema

- **Statement:** A `.cinematic.json` document follows the root, binding, track, channel, section, key, marker and subsequence schema of the formats file with integer-microsecond times, and the cook refuses any document that violates it.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCook_EveryRule_Reported`; `Test` gate step, `ZenithCook_Tests`; `Test_CinematicDocument_RoundTrip_Canonical`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Cinematic document"

### Z2-R-CINE-015 Document migration preserves unknown members

- **Statement:** The cinematic document type is registered with its canonical member order, migrates by single-version steps with preview, backup and journaled write, and preserves unknown members.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicDocument_UnknownMembers_Preserved`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Document migration"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Document type registration"

### Z2-R-CINE-016 Cooked cinematic layout

- **Statement:** A `.zcine` file uses ASSET's header with magic `ZCIN` and kind 14 and the twelve section kinds with the record sizes and fields of the formats file.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicLoader_ValidFile_Loads`, `Test_CinematicCook_TwoCharactersFixture_CooksAndLoads`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked cinematic file"

### Z2-R-CINE-017 Cinematic load validation

- **Statement:** The kind loader applies checks L1–L12 after ASSET's checks C1–C12 and fails a violating file with `CINEMATIC_ERROR_CORRUPT` and the check number.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicLoader_CorruptL1` … `CorruptL12`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Load validation"

### Z2-R-CINE-018 Cinematic format limits at and above limit

- **Statement:** Every limit of the sequence header (65,536 sections and 8 fire-on-skip keys among them) loads at the limit and fails at limit + 1, and a future format version fails with `CINEMATIC_ERROR_VERSION`.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicLoader_LimitsAtAndAboveLimit`, `Test_CinematicLoader_FutureVersion_Refused`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Sequence header section"

### Z2-R-CINE-019 Loaders fuzzed

- **Statement:** The cinematic loader and the video loader are `win-asan` fuzz targets that run without a sanitizer report for 60 s per gate run, and the host-only MP4 reader survives 20,000 seeded mutations returning a result without asserting.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_CinematicLoader`, `Zenith_Fuzz_VideoLoader` (`Sanitizers -Preset win-asan` gate step); `Test_VideoImport_Mp4Mutations_RefusedWithoutCrash`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Load validation"; `Zenith2/Docs/Design/CINE/Tests.md` → "Test targets"

### Z2-R-CINE-020 Runtime type of kind 14

- **Statement:** `Zenith_Cinematic` is the runtime object of `ASSET_KIND_CINEMATIC` with the accessors of its header, immutable after load, with binary search over bindings and markers.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicLoader_ValidFile_Loads`, `Test_CinematicLoader_FindBinding_LogN`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicAsset header"; `Zenith2/Docs/Design/CINE/Formats.md` → "Runtime layout"

### Z2-R-CINE-021 Cook step cine.sequence

- **Statement:** The `cine.sequence` step migrates, flattens, resolves names against the game's type registry and composition, validates, hashes and writes the cooked file, recording hard and provenance dependencies.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-CONTENT-003
- **Acceptance evidence:** `Test_CinematicCook_TwoCharactersFixture_CooksAndLoads`, `Test_CinematicCook_SubsequenceFlatten_ShiftsAndRenames`, `Test_CinematicCook_FixtureWriterMatchesCookStep`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step"

### Z2-R-CINE-022 Cook reports every finding

- **Statement:** The cook reports every validation finding of a document with its rule ID and JSON pointer, not only the first.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCook_AllFindingsReported`, `Test_CinematicCook_EveryRule_Reported`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook validation rules"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook error codes"

### Z2-R-CINE-023 Cook deterministic with schema-sensitive key

- **Statement:** Two cooks of one document are byte-identical, and the derived-data key changes exactly when the document, a subsequence or a referenced component schema changes.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCook_Deterministic_ByteIdentical`, `Test_CinematicCook_DdcKey_ChangesWithReferencedSchema`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step"

### Z2-R-CINE-024 Subsequences flattened at cook

- **Statement:** Subsequences are flattened with shifted, clipped times, renamed bindings and prefixed names to a depth of at most 4, and a cycle fails the cook.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCook_SubsequenceFlatten_ShiftsAndRenames`, `Test_CinematicCook_SubsequenceCycle_Refused`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Subsequences"

### Z2-R-CINE-025 Video import

- **Statement:** The ZenithCook importer `cine.video` remuxes an MP4's H.264 or HEVC elementary stream bit for bit, extracts its audio as a sound sub-asset, and enforces the size, duration, reorder-depth, profile and box limits, refusing a 10-bit profile for a game that targets Android.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-VIDEO (open, Stage 2)
- **Budgets:** Z2-B-CONTENT-003
- **Acceptance evidence:** `Test_VideoImport_Remux_BitstreamPreserved`, `Test_VideoImport_AudioExtracted_SubAsset`, `Test_VideoImport_LimitsRefused`, `Test_VideoImport_Mp4Bounds_Refused`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Video importer"; `Zenith2/Docs/Design/CINE/Formats.md` → "Video import settings"

### Z2-R-CINE-026 Cooked video layout and validation

- **Statement:** A `.zvid` file uses magic `ZVID`, kind 21 and the six sections of the formats file with an uncompressed bitstream, and the loader applies checks V1–V7.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoLoader_ValidFile_Loads`, `Test_VideoLoader_CorruptV1` … `CorruptV7`, `Test_VideoLoader_FindFrameAtOrBefore_PresentationOrder`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked video file"; `Zenith2/Docs/Design/CINE/Formats.md` → "Video load validation"

### Z2-R-CINE-027 Instance state machine

- **Statement:** An instance moves only along the transitions of the instance state table, and every event not listed for its state is counted and dropped.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-14, Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_StartResident_BeginsInline`, `Test_CinematicPlayback_NaturalEnd_BlendsOutThenEnds`, `Test_CinematicPlayback_Pause_HoldsTimeAndWeight`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Instance state machine"

### Z2-R-CINE-028 Begin inline for prepared sequences

- **Statement:** A start whose sequence is resident, bound and prepared begins in its own playback: snapshot taken, state BLENDING_IN at the start time with weight 0, START dispatched, `Started` delivered in phase 9 of the same tick.
- **Covers:** Z2-I-CINE-01, Z2-I-GAME-28 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_StartResident_BeginsInline`; GAME's `Test_Scenario_SkipCinematicMidBlend`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"; `Zenith2/Docs/Design/CINE/Playback.md` → "Playback model"

### Z2-R-CINE-029 Tracks write from the tick after Begin

- **Statement:** Tracks first advance and record in phase 1 of the tick after Begin, and `Cinematics.ApplyTransforms` and `Cinematics.ApplyLate` write nothing for an instance in its Begin tick.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_BlendIn_WeightKOverFifteen`, `Test_CinematicPlayback_StartResident_BeginsInline`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Playback model"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-030 Per-bound-entity lifecycle dispatch

- **Statement:** Each cinematic lifecycle event is dispatched once with a null entity and once per bound slot with that slot's entity, carrying a `Zenith_CinematicLifecyclePayload`.
- **Covers:** Z2-I-CINE-14, Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_StartResident_BeginsInline`, `Test_CinematicIntegration_CameraDirectorSuspends`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvents header"

### Z2-R-CINE-031 Binding resolution errors

- **Statement:** Binding resolution reports `CINEMATIC_ERROR_BINDING_UNKNOWN`, `CINEMATIC_ERROR_BINDING_DUPLICATE`, `CINEMATIC_ERROR_BINDING_TYPE` and `CINEMATIC_ERROR_BINDING_REQUIRED` with the binding or slot index, failing the start without a lifecycle event.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_BindingErrors_EachCode`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Binding resolution"

### Z2-R-CINE-032 Asynchronous binding through a publication source

- **Statement:** Sequence and video loads take completion IDs of `ASYNC_SOURCE_CINEMATICS` from the world's module publication sink in the merge of `Cinematics.Evaluate` (deferring on `PUBLICATION_SINK_ERROR_FULL`, allocating nothing on `PUBLICATION_SINK_ERROR_DETACHED`); the source is declared per world at AttachWorld with `Zenith_ModuleSourceDesc{ 64, 24, MODULE_SOURCE_SITE_MERGE_CHAIN }` and `SetSourceHandler`, and the publications reach CINE's `Zenith_ModulePublicationHandler` at tick-boundary step 1 through MOD's forwarder.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_CinematicsModule_Attach_DeclaresSourceAndHandler`, `Test_CinematicPlayback_StartNotResident_BindsThenBegins`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicDeterminism_LoadPublicationTick_FromLog`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding"

### Z2-R-CINE-033 Preload pins

- **Statement:** `Cinematics.Preload` pins a sequence and issues its load, `Cinematics.Release` drops one pin, and a sequence leaves memory only when no instance and no pin holds it.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** `Test_CinematicPlayback_StartNotResident_BindsThenBegins`, `Test_CinematicPlayback_PreloadRelease_PinCounts`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding"

### Z2-R-CINE-034 Abandoned loads completed

- **Statement:** A need whose waiters are all gone is abandoned in the next `Cinematics.Evaluate`: `RequestCancel`, its completion ID completed once through the sink with the zero-length abandoned payload, its late ASSET record dropped, no discard counted by CINE; `DetachWorld` completes every outstanding ID before it returns.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_InterruptWhileBinding_CancelsSilently`, `Test_CinematicPlayback_AbandonedLoad_CompletedOnce`, `Test_CinematicPlayback_DetachWithLoadInFlight_CompletesIDs`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding"

### Z2-R-CINE-035 Preparation window and timeout

- **Statement:** An instance begins only when the clips and dialogue of its first `cine.prepareLookaheadSeconds` are ready or prepared for the audio lead, or after `cine.prepareTimeoutTicks`, when it begins anyway or fails per `cine.prepareTimeoutFails`.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-03, Z2-I-CINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_PrepareTimeout_BeginsOrFails`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Preparation"

### Z2-R-CINE-036 Prefetch ahead of sections

- **Statement:** While playing, clip and dialogue prefetches for sections starting within the lookahead are recorded exactly once each.
- **Covers:** Z2-I-CINE-03, Z2-I-CINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Dialogue_PrepareLeadThenPlay`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-037 Integer time advance

- **Statement:** Sequence time advances by `(1,000,000 × rateQ16 + 32,768) >> 16` scaled units per tick, so 15 ticks at 60 Hz and rate 1 advance exactly 250,000 µs.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTime_SixtyHzFifteenTicks_Exactly250ms`, `Test_CinematicTime_RateQ16_RoundsHalfUp`, `Test_CinematicTime_OneHourAt1000Hz_NoOverflow`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Time arithmetic"

### Z2-R-CINE-038 Keys fire on half-open ranges

- **Statement:** Keys fire when their time is in (s_prev, s_cur], the first advance after Begin includes the start time, and a looping instance fires each key once per loop.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicEvaluator_KeysInRange_HalfOpen`, `Test_CinematicPlayback_Loop_WrapsAndFiresKeysOnce`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-039 Sub-tick offsets

- **Statement:** Every audio and dialogue record carries its fire item's sub-tick offset `float(f / H)` with `f = double(k·H − S₀) / double(a)` clamped to [0, 1], itself clamped below 1/H, and the schedule-subtick flag.
- **Covers:** Z2-I-CINE-04, Z2-I-CINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTime_SubTickOffset_InRange`, `Test_CinematicTracks_Audio_PostEventTimeLockedSubTick`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Time arithmetic"; `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"

### Z2-R-CINE-040 Curve interpolation

- **Statement:** Channels sample constant, linear and cubic Hermite keys, clamp outside the keys, and match a double-precision oracle within 1e-6.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicEvaluator_Constant_HoldsValue`, `Test_CinematicEvaluator_Linear_Interpolates`, `Test_CinematicEvaluator_Cubic_MatchesHermite`, `Test_CinematicEvaluator_BeforeFirstAfterLast_Clamp`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Curve channels and interpolation"

### Z2-R-CINE-041 Rotation interpolation takes the shortest arc

- **Statement:** Rotations interpolate per channel after cook sign alignment, or by slerp per group, and never take the longer arc.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicEvaluator_SlerpGroup_ShortestArc`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicCook_RotationSignAlignment`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Curve channels and interpolation"

### Z2-R-CINE-042 Evaluator bit-identical

- **Statement:** The public evaluator returns bit-identical results for identical inputs and is the code `Cinematics.Evaluate` and the playbacks use.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicEvaluator_SameInputs_BitIdentical`, `Test_CinematicScrub_PausedMatchesPlayedTick`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvaluator header"

### Z2-R-CINE-043 Record cap defers

- **Statement:** An instance whose tick buffer reaches 512 records, or whose next item the outbox does not admit, stops processing fire items and resumes at the next tick from the first unprocessed item key, every item fired once in key order.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_CinematicPlayback_CommandCap_DefersNotDrops`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-044 Instance buffers merge in instance order

- **Statement:** `Cinematics.Evaluate` records each instance's side work into `Commands(0, 1 + i)` and its tick records into `Commands(0, 65 + i)` for instance index i in ascending instance ID, so CINE's records merge side work first, then tick records, each in instance order.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicDeterminism_WorkerCounts_IdenticalHashes`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-045 Transform tracks win arbitration

- **Statement:** Transform tracks write `Zenith.LocalTransform` 1–3 of bound entities in phase 6 with `WRITER_SOURCE_CINEMATIC`, taking precedence over every other writer source.
- **Covers:** Z2-I-CINE-01, Z2-I-GAME-28 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Transform_WinsArbitration`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"; `Zenith2/Docs/Design/CINE/Playback.md` → "Transform tracks"

### Z2-R-CINE-046 Transform spaces

- **Statement:** Transform tracks evaluate in parent, world or anchor space as the transform track table defines and write only the driven channels.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Transform_SpacesParentWorldAnchor`, `Test_CinematicTracks_Transform_UndrivenChannelsUntouched`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Transform tracks"

### Z2-R-CINE-047 One writer per row

- **Statement:** No two instances write one entity's row in a tick, and no two tracks of one instance drive the same component channel of one entity.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_TakeOver_MovesSnapshot`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicCook_EveryRule_Reported` (`cine.cook.duplicate_driver`); `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"

### Z2-R-CINE-048 Camera pose lens and lighting

- **Statement:** Camera tracks write the camera pose in phase 6 and FovY, near, far and the camera lighting properties found by stable name through phase-10 `SetProperty` commands, a missing type being a counted no-op.
- **Covers:** Z2-I-CINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-LIGHT-012
- **Acceptance evidence:** `Test_CinematicTracks_Camera_LensAndLightingByName`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate"

### Z2-R-CINE-049 Camera cuts

- **Statement:** A change of cut index between ticks, or a seek across a cut, increments `Zenith.Render.Camera` CutRequest once, and Begin requests a cut only for a `cutIn` track.
- **Covers:** Z2-I-CINE-02, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Camera_CutIncrementsCutRequest`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks"

### Z2-R-CINE-050 Camera director handover

- **Statement:** While a cinematic holds a camera the director writes nothing, and it resumes at END or SKIP blending from the cinematic camera's final pose.
- **Covers:** Z2-I-CINE-02, Z2-I-CINE-14, Z2-I-CINE-15, Z2-I-GAME-29 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicIntegration_CameraDirectorSuspends`; GAME's `Test_Cameras_CinematicSuspension`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"

### Z2-R-CINE-051 Animation sections drive cinematic slots

- **Statement:** Animation sections record `Animation.PlaySlot` with the cinematic tag, the section blends, rate and start time in phase 1, reaching ANIM's phase 3 the same tick, and `Animation.StopSlot` at their end.
- **Covers:** Z2-I-CINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_CinematicTracks_Animation_PlaySlotRecordedPhase1`, `Test_CinematicIntegration_AnimSlotsRealModule`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks"

### Z2-R-CINE-052 Slot time kept in step

- **Statement:** A rate change, a seek and a resume each re-send the slot clip time with `Animation.SetSlotTime`, so ANIM's clip time equals the sequence time.
- **Covers:** Z2-I-CINE-03, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Animation_RateChange_ResendsSlotTime`, `Test_CinematicSeek_Forward_SetsSlotTimesNoNotifies`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks"

### Z2-R-CINE-053 Physics weight written by the cinematic

- **Statement:** An animation track's physics-weight channel writes `Zenith.Animation.Pose.PhysicsWeight` in phase 6 with the cinematic source.
- **Covers:** Z2-I-CINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Animation_PhysicsWeight_CinematicSource`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Field authority census rows"

### Z2-R-CINE-054 Audio sections posted by kind name

- **Statement:** Audio sections record `Audio.PostEvent` by registered kind name with a reflected payload carrying the tick, sub-tick offset, time-lock and attachment flags and a deterministic native instance ID, and `Audio.StopInstance` at their end.
- **Covers:** Z2-I-CINE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Audio_PostEventTimeLockedSubTick`, `Test_CinematicIntegration_AudioRealModuleOffline`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Reflected payloads"

### Z2-R-CINE-055 Deterministic audio instance IDs

- **Statement:** CINE's audio instance IDs are `Zenith_Hash64` of the domain-salted instance serial and the track and section, with bits 0 and 63 clear and never 0.
- **Covers:** Z2-I-CINE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicAudioIDs_NativeBitsClear_Deterministic`, `Test_CinematicAudioIDs_DistinctPerSection`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"

### Z2-R-CINE-056 Audio follows pause and rate

- **Statement:** Pausing an instance or leaving rate 1 stops its live time-locked voices with a 50 ms fade and marks them stopped in the audio list, and a time-locked section that starts while the rate is not 1 joins the list marked stopped without posting; resuming at rate 1 or returning to rate 1 re-posts exactly the marked entries with fresh deterministic IDs and seeks them to the sequence time; non-time-locked voices play on.
- **Covers:** Z2-I-CINE-04, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Audio_PauseStopsResumeReposts`, `Test_CinematicTracks_Audio_PauseResumeWithinFade_Replays`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"

### Z2-R-CINE-057 Event keys emitted the same tick

- **Statement:** Event keys are queued in timeline order with payloads built by reflection and substitutions patched, emitted by `Cinematics.EmitEvents` in phase 8, and delivered in phase 9 of the tick they fire.
- **Covers:** Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_CinematicTracks_Event_FiresInPhase9SameTick`, `Test_CinematicPayload_Substitutions_Patched`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Event tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents"

### Z2-R-CINE-058 Reflected payloads

- **Statement:** A payload authored by property name is written at the registered type's layout offsets over the type's defaults, and an unknown property, a wrong value kind or an oversize payload disables the key and is counted once.
- **Covers:** Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPayload_ByPropertyName_MatchesLayout`, `Test_CinematicPayload_UnknownProperty_DisablesKey`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Reflected payloads"

### Z2-R-CINE-059 Outbox deferral

- **Statement:** `Cinematics.Evaluate` holds at most 256 stored outbox entries, deferring further track items to the next tick in key order and a side buffer whose events do not fit, counted, never dropped; playback and handler entries come from each instance's reservation of 10, so the store of 256 + 10 × `cine.maxInstancesPerWorld` + 64 entries never overflows.
- **Covers:** Z2-I-CINE-05, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_CinematicPlayback_OutboxFull_DefersEvents`, `Test_CinematicPlayback_OutboxBound_SixtyFourSkipsOneTick`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents"; `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"

### Z2-R-CINE-060 Dialogue prepared then played time-locked

- **Statement:** A dialogue section records `Audio.PrepareDialogue` the audio lead before its start and `Audio.PlayDialogue` time-locked with its sub-tick offset at its start.
- **Covers:** Z2-I-CINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Dialogue_PrepareLeadThenPlay`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks"

### Z2-R-CINE-061 Facial performance with dialogue

- **Statement:** A dialogue section's face clip plays in slot `cine.face` with the section's start and duration.
- **Covers:** Z2-I-CINE-08, Z2-I-CINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Dialogue_PrepareLeadThenPlay`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks"

### Z2-R-CINE-062 Caption cues from dialogue and subtitles

- **Statement:** Dialogue and subtitle sections queue one `Zenith.Runtime.CaptionCueEvent` at their start with every caption field, the duration in ticks scaled by the rate, and a source ID below 2⁵³, delivered to UI in phase 9 of the same tick.
- **Covers:** Z2-I-CINE-09, Z2-I-CINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_CinematicTracks_Subtitle_CaptionCueFields`, `Test_CinematicIntegration_CaptionReachesUI`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks"

### Z2-R-CINE-063 Captions follow seeks

- **Statement:** A seek out of a caption's section cancels it, a seek into a section re-queues it with the remaining duration, and a pause, a rate change, a skip, an interrupt and an end cancel or re-queue it by source ID, so the caption on screen matches the sequence time.
- **Covers:** Z2-I-CINE-09, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Subtitle_SeekReEmitsRemaining`, `Test_CinematicTracks_Subtitle_PauseSeekSkipKeepsCaptionInSync`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing"

### Z2-R-CINE-064 Property tracks

- **Statement:** Property tracks resolve their component and property by stable name, record curve or step values as `SetProperty` commands in phase 10, and are refused at cook for arbitrated properties.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Property_CurveAndStep`, `Test_CinematicCook_EveryRule_Reported` (`cine.cook.arbitrated_property`); `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Property tracks"

### Z2-R-CINE-065 Command tracks

- **Statement:** Command keys record any registered kind allowed in phase 1 by name with a reflected payload, and a kind absent from the build disables the track, counted and logged once.
- **Covers:** Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Command_ByNameAndPayload`, `Test_CinematicTracks_Command_MissingKind_DisabledOnce`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Command tracks"

### Z2-R-CINE-066 VFX keys from command tracks

- **Statement:** Command tracks naming `VFX.Spawn` record it with a reflected `Zenith.VFX.SpawnPayload` for CinematicSlice's set-pieces.
- **Covers:** Z2-I-CINE-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicIntegration_VfxSpawnFromCommandTrack` with the VFX module (Stage 5); `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Command tracks"; `Zenith2/Docs/Design/CINE.md` → "Stage 5"

### Z2-R-CINE-067 Overlay tracks extracted only

- **Statement:** Overlay fade and letterbox values are extracted into the render world and never written into simulation state.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Overlay_ExtractedOnly`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Overlay tracks"; `Zenith2/Docs/Design/CINE/Formats.md` → "Render-world records"

### Z2-R-CINE-068 Markers

- **Statement:** Marker keys emit `Zenith.Cinematics.Marker`, and markers are found by name hash for seeks.
- **Covers:** Z2-I-CINE-05, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Marker_EventAndFind`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Marker tracks"

### Z2-R-CINE-069 Section eases and overlap

- **Statement:** Section weights are the product of smoothstep eases, at most 8 sections overlap on a track, and overlapping transform and property sections blend normalised.
- **Covers:** Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicEvaluator_SectionWeights_SmoothstepEases`, `Test_CinematicEvaluator_OverlapEight_NormalisedBlend`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Section blending"

### Z2-R-CINE-070 Blend-in from the snapshot

- **Statement:** During BLENDING_IN every written value is `lerp(B_in, track, w)` with B_in the snapshot (or, for a taken-over entity, the captured take-over base) and w the exact ratio of elapsed to blend-in world time.
- **Covers:** Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicBlend_InFromSnapshot`, `Test_CinematicPlayback_BlendIn_WeightKOverFifteen`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Blending"

### Z2-R-CINE-071 Blend-out holds unflagged values

- **Statement:** During BLENDING_OUT and in the end tick an unflagged written property holds `lerp(B_in, C, w₀)` with w₀ the weight the blend-out started from, so an interrupt during the blend-in does not jump, and a restore-flagged one falls to its snapshot value.
- **Covers:** Z2-I-CINE-07, Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicBlend_OutUnflaggedHolds`, `Test_CinematicBlend_InterruptDuringBlendIn_UnflaggedNoPop`, `Test_CinematicBlend_OutFlaggedReachesSnapshot`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Blending"

### Z2-R-CINE-072 Natural end holds the last frame

- **Statement:** At its duration a non-looping instance clamps its time, enters BLENDING_OUT with reason COMPLETED, and records `Cinematics.End` when the weight reaches 0.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_NaturalEnd_BlendsOutThenEnds`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"; `Zenith2/Docs/Design/CINE.md` → "Rulings this design records"

### Z2-R-CINE-073 Take-over between instances

- **Statement:** A beginning instance binding an entity held by another merges the holder's snapshot entries into a staging block, commits the take-over only after its snapshot succeeded, takes the row from the next tick with a base captured at the boundary, and ends the holder through the End path when no slot remains; `uCINE_START_FLAG_NO_TAKEOVER` refuses instead, and an ended holder is not taken over.
- **Covers:** Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_TakeOver_MovesSnapshot`, `Test_CinematicPlayback_TakeOver_SnapshotFailure_LeavesHolderIntact`, `Test_CinematicPlayback_TakeOver_NoTakeoverFlag_Refused`, `Test_CinematicPlayback_TakeOver_EndedHolder_NotTakenOver`, `Test_CinematicBlend_TakeOverBase_IsCommittedValue`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Take-over of a bound entity"

### Z2-R-CINE-074 Interrupt blends out from the current weight

- **Statement:** An interrupt of a begun instance enters BLENDING_OUT with reason INTERRUPTED, falls linearly from its current weight over its blend-out, dispatches INTERRUPT and fades its audio; during that blend-out its continuous tracks follow the advancing time but it processes no fire item, so no section starts or ends, no key fires and no caption is queued.
- **Covers:** Z2-I-CINE-14, Z2-I-GAME-29 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_Interrupt_FromPartialWeight`, `Test_CinematicPlayback_InterruptedBlendOut_FiresNothing`; GAME's `Test_Lifecycle_Cinematic_InterruptBlendsOut`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback"

### Z2-R-CINE-075 Interrupt ends then restores

- **Statement:** An interrupted instance dispatches END when its weight reaches 0, writes its end tick, and restores at the following tick boundary.
- **Covers:** Z2-I-CINE-14, Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** GAME's `Test_Lifecycle_Cinematic_InterruptBlendsOut`; `Test_CinematicPlayback_NaturalEnd_BlendsOutThenEnds`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "End playback"

### Z2-R-CINE-076 Cancel before Begin

- **Statement:** An interrupt or skip during BINDING or PREPARING cancels the instance with no lifecycle event, `Ended` with reason CANCELLED, and the ticket error `CINEMATIC_ERROR_CANCELLED`.
- **Covers:** Z2-I-CINE-14, Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_InterruptWhileBinding_CancelsSilently`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"

### Z2-R-CINE-077 Skip evaluates the end state once

- **Statement:** A skip writes every bound track's end state once, the arbitrated values applied in phase 6 of the skip tick, or at the boundary when the skip played after phase 6.
- **Covers:** Z2-I-CINE-15, Z2-I-GAME-30 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_Skip_EndStateOnceFireOnSkipOnce`, `Test_CinematicPlayback_SkipAfterPhase6_EndStateAtBoundary`; GAME's `Test_Scenario_SkipCinematicMidBlend`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"

### Z2-R-CINE-078 Fire-on-skip keys in timeline order

- **Statement:** A skip fires exactly the remaining keys flagged `fireOnSkip` (at most 8 per sequence), in key order, its events delivered in phase 9 of the skip tick when it played before phase 8 and in phase 9 of the next tick otherwise.
- **Covers:** Z2-I-CINE-15, Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_Skip_EndStateOnceFireOnSkipOnce`; GAME's `Test_Scenario_SkipCinematicMidBlend`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"

### Z2-R-CINE-079 Skip side work

- **Statement:** The fire-on-skip commands, audio stops and caption cancels of a skip are side work in the instance record, outside the instance block, recorded by the next `Cinematics.Evaluate` into the instance's side buffer whatever its state, `RELEASED` included, and saved.
- **Covers:** Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_SkipSideTable_RecordedAfterRestore`, `Test_CinematicSave_SkipSideTable_Survives`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Side work"

### Z2-R-CINE-080 Skip is terminal

- **Statement:** A skip dispatches SKIP and never an END after it.
- **Covers:** Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_SkipIsTerminal_NoEndEvent`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"; `Zenith2/Docs/Design/CINE.md` → "Rulings this design records"

### Z2-R-CINE-081 Skip leaves no pose pop

- **Statement:** A skip produces no visible pose jump on bound characters: ANIM's inertial take continues from the last output.
- **Covers:** Z2-I-CINE-15, Z2-I-CINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-SHIP-021
- **Acceptance evidence:** `Test_CinematicIntegration_AnimSlotsRealModule`; the sign-off item of [Z2-N-BUDGET-435] in `Games/CinematicSlice/Acceptance/SignOff.md`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"

### Z2-R-CINE-082 Snapshot written and restore sets

- **Statement:** Begin snapshots, in (type, property) order, every property a slot's tracks write and every property its entity's components flag `restoreAfterCinematic`.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSnapshot_WrittenAndRestoreSets_Union`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"

### Z2-R-CINE-083 Restore at the tick boundary

- **Statement:** At step 2 of a tick boundary every `ENDED` instance whose end tick is at or before the tick just finished has every restore-flagged property written back bit for bit from the snapshot and is released.
- **Covers:** Z2-I-CINE-16, Z2-I-GAME-30 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSnapshot_RestoreFlagged_BitExact`; GAME's `Test_Lifecycle_Cinematic_SkipRestoresGameplayState`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Tick-boundary handler"; `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"

### Z2-R-CINE-084 Unflagged properties keep the final value

- **Statement:** Properties written by tracks and not flagged `restoreAfterCinematic` keep the cinematic's final value after the restore.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSnapshot_RestoreFlagged_BitExact`; GAME's `Test_Scenario_SkipCinematicMidBlend`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"

### Z2-R-CINE-085 No cinematic writes after restore

- **Statement:** After the restore boundary no CINE system writes any row of the released instance's entities.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** GAME's `Test_Scenario_SkipCinematicMidBlend`; `Test_CinematicSnapshot_RestoreFlagged_BitExact`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Tick-boundary handler"

### Z2-R-CINE-086 Snapshot limits

- **Statement:** An instance block exceeding 4,096 snapshot entries, 256 KiB or the world pool fails the start with `CINEMATIC_ERROR_SNAPSHOT_LIMIT` before any allocation and leaves no partial snapshot.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** `Test_CinematicSnapshot_LimitExceeded_StartFails`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"

### Z2-R-CINE-087 Container properties snapshotted

- **Statement:** Restore-flagged container properties are snapshotted with `ReadContainer` and restored element for element at the tick boundary with `Zenith_TickBoundaryAccess::SetContainer`; a container the heap cannot serve at the boundary is skipped and counted.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSnapshot_ContainerProperty_RoundTrips`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"

### Z2-R-CINE-088 Destroyed bindings dropped

- **Statement:** A bound entity destroyed during a cinematic is dropped from its bindings at the destroy playback with its snapshot entries released and the sequence continuing, and the instance ends FAILED when a slot flagged `uCINE_BINDING_FLAG_REQUIRED` is dropped and no other slot with that flag is bound.
- **Covers:** Z2-I-CINE-16, Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_BoundEntityDestroyed_DroppedContinues`, `Test_CinematicPlayback_LastRequiredDestroyed_EndsFailed`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Lifecycle listener"; `Zenith2/Docs/Design/CINE/Playback.md` → "Instance state machine"

### Z2-R-CINE-089 Seek applied before the advance

- **Statement:** A seek playback stores a clamped target that the next `Cinematics.Evaluate` applies before advancing (or, for a paused instance, without advancing), the second of two seeks winning; the seek's new time, item key, prefetch bits and cleared flags are staged with the tick buffer, so a rejected buffer leaves the seek pending and the pre-seek time.
- **Covers:** Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSeek_TwoSeeksBeforeEvaluate_LaterWins`, `Test_CinematicPlayback_MergeCapacityRejection_RerunsWhole`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Seek playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing"

### Z2-R-CINE-090 Seek updates slots audio and captions

- **Statement:** A seek plays, re-times or stops animation slots without notifies; seeks a live voice whose section stays active with `Audio.SeekInstance` alone, stops the voices of sections it leaves and posts entering sections with fresh IDs; and cancels and re-queues the captions at the target.
- **Covers:** Z2-I-CINE-06, Z2-I-CINE-03, Z2-I-CINE-04, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSeek_Forward_SetsSlotTimesNoNotifies`, `Test_CinematicTracks_Subtitle_SeekReEmitsRemaining`, `Test_CinematicScrub_PausedMatchesPlayedTick`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing"

### Z2-R-CINE-091 Seek fires keys only on request

- **Statement:** Keys in a jumped interval fire only with the seek's fire-events flag and the key's `fireOnSeek` flag, and a backwards seek fires none.
- **Covers:** Z2-I-CINE-06, Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSeek_FireEventsFlag_RespectsFireOnSeek`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing"

### Z2-R-CINE-092 Scrub equals play

- **Statement:** A seek of a `PAUSED` instance is applied by the next `Cinematics.Evaluate` without an advance, and the paused instance seeked to time t writes exactly the values a played tick at t writes; its audio re-posts and caption re-queues wait for the resume.
- **Covers:** Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicScrub_PausedMatchesPlayedTick`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing"

### Z2-R-CINE-093 Rate pause and resume

- **Statement:** `SetRate` accepts [0, 8] with 0 holding time, `Pause` holds time and weight and records the state it left, and `Resume` returns to that state at the paused weight.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_Pause_HoldsTimeAndWeight`, `Test_CinematicPlayback_Resume_RestoresState`, `Test_CinematicPlayback_SetRate_Zero_HoldsTime`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Rate pause and resume playback"

### Z2-R-CINE-094 Determinism across workers

- **Statement:** A cinematic run produces identical per-tick world hashes with 1, 2, 4 and 8 workers.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicDeterminism_WorkerCounts_IdenticalHashes`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules"

### Z2-R-CINE-095 Replay reproduces cinematic runs

- **Statement:** A replay of a recorded run with starts, a skip, a seek and a rate change reproduces every tick hash, load publication ticks included.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-06, Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicDeterminism_Replay_ReproducesSkipAndSeek`, `Test_CinematicDeterminism_LoadPublicationTick_FromLog`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules"

### Z2-R-CINE-096 Video isolated from simulation

- **Statement:** No decoder state, decode timing or frame availability changes any world hash.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoIsolation_DecodeStallChangesNoWorldHash`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Scope of this file"; `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules"

### Z2-R-CINE-097 Exact save participant

- **Statement:** The participant `Zenith.Cinematics.Playback` captures with EXACT fidelity every instance record and block (snapshot, take-over bases, late cache, end state, prefetch bits), the paused-from state, prepare records, the player link, side work, the audio list, tickets, player starts, pins, in-flight needs and the unemitted outbox, every entity through `WriteEntityReference`, so a save restored and run N ticks equals the uninterrupted run.
- **Covers:** Z2-I-CINE-16, Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSave_MidBlendRoundTrip`, `Test_CinematicSave_SkipAfterPhase6_EndStateSaved`, `Test_CinematicSave_TakeOverMidBlend_RoundTrip`, `Test_CinematicSave_OutboxLateEntries_Survive`, `Test_CinematicSave_PausedState_Restored`, `Test_CinematicSave_PrepareRecords_Restored`, `Test_CinematicSave_PlayerLink_Restored`, `Test_CinematicSave_AcceptedCueExpiry_Applied`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"; `Zenith2/Docs/Design/CINE/Formats.md` → "Save section"

### Z2-R-CINE-098 Save re-issues in-flight loads

- **Statement:** A restore re-issues in-flight loads with their recorded completion IDs and reports readiness only when every held sequence is resident.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSave_DuringBinding_ReissuesLoad`, `Test_CinematicSave_IsRestoreReady_WaitsResidency`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"

### Z2-R-CINE-099 Changed content at load

- **Statement:** A restore whose sequence content hash differs from the resident asset's cancels a binding or preparing instance and ends a begun one FAILED through a recorded `Cinematics.End` that dispatches END once, then restores its snapshot at the next boundary.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSave_ContentChanged_EndsFailedRestores`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Save section"

### Z2-R-CINE-100 Player component

- **Statement:** `Zenith.Cinematics.Player` (relocatable, Bindings a `Zenith_ContainerRef`) starts its sequence once when `PlayOnStart` is set, the autostart committed only when the merge accepts its buffer (retried at the next tick otherwise) and the start playback reading its bindings and `Self`; `Cinematics.Evaluate` writes `Instance`, `State` and on failure `LastError` with `State` ENDED through the own-row path, an autostart dropped before its ticket entry expires with `CINEMATIC_ERROR_TICKET_EXPIRED`, and the player never retries or starts again after a load.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_PlayerComponent_AutostartsOnce`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicComponents header"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-101 Census rows

- **Statement:** CINE's writes follow the field authority census rows of the playback file, which the authority census tests compare with the registry.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** GAME's `Test_AuthorityCensus_OpenWorldModules`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Field authority census rows"

### Z2-R-CINE-102 Luau request functions

- **Statement:** The `Cinematics` request functions record their command kinds as buffered side effects of the invoking transaction, rolled back when it fails, with bindings sorted by name hash and the ticket computed from the invocation's command buffer at the call.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-14, Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** Z2-B-SCRIPT-023, Z2-B-SCRIPT-024
- **Acceptance evidence:** `Test_CinematicScript_StartResolveEnded`, `Test_CinematicScript_RequestsAreTransactional`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface"

### Z2-R-CINE-103 Luau queries

- **Statement:** The `Cinematics` query functions read `Zenith.Cinematics.World` and return the documented values, `Resolve` returning nil while pending or failed, `GetStartError` the failure's code name and `GetMarkerTime` a marker's time by name.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicScript_StartResolveEnded`, `Test_CinematicScript_StartFailedEvent_CarriesError`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface"

### Z2-R-CINE-104 Catalogued misuse errors

- **Statement:** Each misuse row ZS2701 to ZS2712 of CINE's block ZS2700–ZS2799 raises its code with its message tail, and a stale non-null instance is never misuse.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicScript_Misuse_EachCode`, `Test_CinematicScript_StaleInstance_NotMisuse`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Misuse codes"

### Z2-R-CINE-105 Cinematic events for scripts

- **Statement:** `Started`, `Ended` with its reason, `Marker` and `StartFailed` are emitted as ECS events with the property IDs of the events header, every property exact in Luau (names and tags as strings, tickets as four 32-bit fields).
- **Covers:** Z2-I-CINE-05, Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_CinematicScript_StartResolveEnded`, `Test_CinematicTracks_Marker_EventAndFind`, `Test_CinematicScript_MarkerAndTagMatchInLuau`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvents header"

### Z2-R-CINE-106 Memory consumer and shedding

- **Statement:** The memory consumer reports CINE's CPU and VRAM bytes for `Z2-B-CINE-001` and implements the Cinematics row of Architecture's shedding table: at WARNING decode-ahead trimmed to 2, at CRITICAL invisible players stopped, never a sequence, a pin or simulation state, every world hash unchanged.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** `Test_CinematicMemory_PressureLevels_Shed`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Memory consumer"

### Z2-R-CINE-107 Evaluation CPU budget

- **Statement:** The five CINE systems together stay within `Z2-B-CINE-002` on Windows and the S24 for W-Cinematic.
- **Covers:** Z2-I-CINE-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CINE-002
- **Acceptance evidence:** `Zenith_Bench_CinematicEvaluate` and the W-Cinematic MP-CPUZONE evidence record in `Games/CinematicSlice/Acceptance/Budgets.md`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Rows CINE owns"; `Zenith2/Docs/Design/CINE/Tests.md` → "Benchmarks"

### Z2-R-CINE-108 Cinematic memory budget

- **Statement:** CINE's resident memory stays within `Z2-B-CINE-001` on Windows (RAM and VRAM) and the S24 for W-Cinematic.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** the W-Cinematic MP-SUBMEM evidence record in `Games/CinematicSlice/Acceptance/Budgets.md`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Rows CINE owns"

### Z2-R-CINE-109 Pending budget rows exist

- **Statement:** The pending rows "Video decode CPU", "Video conversion and composite GPU" and "Cinematic instances per world" exist in Budgets.md with the producer "Stage 1 per-system budget ticket for CINE".
- **Covers:** Z2-I-CINE-10, Z2-I-CINE-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CINE-001, Z2-B-CINE-002
- **Acceptance evidence:** the Budgets.md rows added by core-doc item X294; the `Coverage` gate step
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Pending row needs"

### Z2-R-CINE-110 D-SEL-VIDEO harness and ruling

- **Statement:** The worker ticket "D-SEL-VIDEO decoder harness" builds `Zenith_Bench_VideoDecoderSelection` with candidates A and B and its smoke run; the `needs-human` ticket "D-SEL-VIDEO ruling" measures them on the reference desktop and the S24 against V-C1 to V-C9 and picks per platform by the acceptance rule.
- **Covers:** Z2-I-CINE-10
- **Stage:** S2
- **Classification:** research
- **Decisions:** D-SEL-VIDEO (open, Stage 2), Z2-D-097
- **Budgets:** Z2-B-CINE-001
- **Acceptance evidence:** the harness smoke run (`Test` gate step); the `needs-human` ruling ticket's `Zenith2/Docs/Research/CINE/VideoDecoderSelection.md` and DecisionLog entry
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria"

### Z2-R-CINE-111 Decoder interface

- **Statement:** Every backend implements `Zenith_VideoDecoder` and is called only from the video thread, and a test backend covers every test above it.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-VIDEO (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoPlayer_StateMachine_EveryTransition`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Zenith_VideoDecoder interface"

### Z2-R-CINE-112 Windows platform decoder

- **Statement:** The Media Foundation backend decodes H.264 High 1080p through the synchronous Microsoft decoder MFT (an asynchronous MFT is never activated) given the DXGI device manager of a private D3D11 device for DXVA decode, sends the begin-streaming and start-of-stream messages after the output type, maps `Flush` and `Drain` to the flush, end-of-stream and drain messages, copies only the display aperture of the output texture-array slice with `CopySubresourceRegion`, and outputs NV12 or P010.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-VIDEO (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** automated half: `Test_VideoDecoderPlatform_H264High1080p_DecodesAllFrames` (`requiresHardwareVideo`, skipped-as-not-run in CI); its run record on the reference desktop is `needs-human` evidence of the acceptance and ruling tickets
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Platform decoder backend"

### Z2-R-CINE-113 Android platform decoder

- **Statement:** The AMediaCodec backend decodes 8-bit H.264 High 1080p in synchronous mode through an AImageReader, maps surface timestamps to presentation times, applies the crop rectangle, copies I420 or NV12 frames, and recreates a reclaimed codec once.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-VIDEO (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** automated half: `Test_VideoDecoderPlatform_H264High1080p_DecodesAllFrames`, `Test_VideoDecoderPlatform_Android10Bit_Refused` (`requiresHardwareVideo`, skipped-as-not-run in CI); the S24 device run record is `needs-human` evidence
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Platform decoder backend"

### Z2-R-CINE-114 Video thread hand-off without locks

- **Statement:** The video thread exchanges commands, lifecycle requests, stream events, frames and released buffers only through five kinds of `Zenith_SpscRing` channel (command, lifecycle and event rings per module, frame and return rings per player) with power-of-two capacities that never refuse a push, takes no lock and never blocks a job; only the render-submit thread changes player state, and a player slot's rings are constructed at `OPENING` and destroyed at `FREE`.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoThread_SpscHandOff_Stress`, `Test_VideoThread_CommandRingFull_Retries`, `Test_VideoThread_RingCapacity_PowerOfTwo`, `Test_VideoThread_EventRing_NeverFull`, `Test_VideoPlayer_StateChangesOnlyFromEvents`, `Test_VideoPlayer_SlotReuse_FreshRings`; `Test` gate step, `ZenithCinematics_Tests` and `Sanitizers -Preset win-asan`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Video thread"

### Z2-R-CINE-115 Video thread lifetime

- **Statement:** `Zenith.VideoDec` is created at module Start and joined at shutdown after `DisableVideo` and the closing of every player.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_Headless_NoVideoNoThread`, `Test_CinematicsModule_Shutdown_JoinsVideoThread`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Threads CINE owns"; `Zenith2/Docs/Design/CINE/Video.md` → "Video thread"

### Z2-R-CINE-116 Video player state machine

- **Statement:** Video players follow the player state table, at most four play at once, a fifth section draws nothing and is counted, and players close by draw-record absence over two consumed render worlds.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoPlayer_StateMachine_EveryTransition`, `Test_VideoPlayer_FifthPlayer_Refused`, `Test_VideoPlayer_PausedRenderWorld_KeepsPlayerOpen`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Video players"

### Z2-R-CINE-117 Video seek

- **Statement:** A video OPEN or seek targets the presentation time of the frame due at the media time (`FindFrameAtOrBefore`), drains the frame ring and the look-ahead slot into the return ring, increments the player's seek epoch, decodes from the keyframe at or before the target and discards frames below it, so the first pushed frame is presentable at once even between frames; a frame of an older epoch is never presented, and a backward seek with a full pool cannot deadlock.
- **Covers:** Z2-I-CINE-10, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoPlayer_SeekBackward_KeyframeThenDiscard`, `Test_VideoPlayer_BackwardSeekFullPool_NoDeadlock`; `Test` gate step, `ZenithCinematics_Tests`; `Test_VideoDecoderPlatform_Seek_LandsOnTarget` (`requiresHardwareVideo`)
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Video players"

### Z2-R-CINE-118 Frame selection

- **Statement:** Each frame presents, through a one-slot look-ahead over the frame ring, the newest decoded frame whose presentation time is at or before the media time, returning it to Flux only when it is newer than the last one returned and holding the last upload when late; every buffer is released exactly once, by frame selection for frames it never returned and by Flux's `ReleaseFrame` for frames it did.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoFrameSelection_NewestAtOrBefore`, `Test_VideoFrameSelection_Late_HoldsLastFrame`, `Test_VideoFrameSelection_SingleRelease`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Frame selection"

### Z2-R-CINE-119 Video conversion

- **Statement:** `VideoConvert` converts NV12, P010 and I420 planes with the stream's range, matrix and transfer to linear RGBA16F within 1/1024 of the CPU reference.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxVideoConvert_Bt709Limited_Reference`, `Test_FluxVideoConvert_P010Bt2020Pq_Reference`, `Test_FluxVideoConvert_I420MatchesNv12`; `Test` and `BackendParity` gate steps, `ZenithFlux_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "GPU side"

### Z2-R-CINE-120 Video composite

- **Statement:** `VideoComposite` draws the video layers with their placement, fit and opacity and then the overlay, between tonemapping and the UI composite, encoding through the output encoder so SDR video matches on SDR and HDR outputs, and is not recorded without video or overlay.
- **Covers:** Z2-I-CINE-10, Z2-I-CINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxVideoComposite_FitModes`, `Test_FluxVideoComposite_OverlayFadeAndLetterbox`, `Test_FluxVideoComposite_EncodeOutputSdrHdrConsistent`, `Test_FluxVideoComposite_NoVideo_NoPassRecorded`; `Test` and `BackendParity` gate steps, `ZenithFlux_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "VideoComposite pass"

### Z2-R-CINE-121 Video surfaces

- **Statement:** A video section with a surface name publishes its player's texture under the name hash, null until a frame exists and recreated after device loss.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxVideoSurface_FindByName_NullWithoutFrame`, `Test_FluxVideoSurface_DeviceLoss_Recreated`; `Test` and `BackendParity` gate steps, `ZenithFlux_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Texture surfaces"

### Z2-R-CINE-122 Video upload within the ring budget

- **Statement:** Video frames upload through the upload ring within its frame budget, and a frame that does not fit is skipped and counted.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_FluxVideoUpload_RingFull_SkipsFrame` in `Flux_VideoSurface_Tests.cpp`; `Test` and `BackendParity` gate steps, `ZenithFlux_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "GPU side"

### Z2-R-CINE-123 Audio and video sync bound

- **Statement:** Presented video frames stay within one frame period plus 5 ms of the time-locked audio position.
- **Covers:** Z2-I-CINE-10, Z2-I-CINE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoSync_OfflineOffsetWithinOneFrame` (device); `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Audio and video sync"

### Z2-R-CINE-124 Device audio and video sync

- **Statement:** On the S24 the measured audio-video offset of CinematicSlice's video stays within ±45 ms plus the display path latency the run reports.
- **Covers:** Z2-I-CINE-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-SHIP-021
- **Acceptance evidence:** `needs-human`: the S24 device run record in `Games/CinematicSlice/Acceptance/BugBar.md`; its automated half is `Test_VideoSync_OfflineOffsetWithinOneFrame` (`Test` gate step, `Zenith_Integration_Cinematics`)
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Audio and video sync"

### Z2-R-CINE-125 Video failure isolation

- **Statement:** Every video failure (asset load, unsupported stream, stream error, device loss or reclaim, allocation) leaves the sequence playing, never fails a start, and leaves the simulation unaffected, logged and counted as the failure table states.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoPlayer_StreamErrors_ThreeFail`, `Test_VideoPlayer_DeviceLoss_RecreatesThenFails`, `Test_VideoIsolation_DecodeStallChangesNoWorldHash`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Video failure handling"

### Z2-R-CINE-126 Android suspend and resume

- **Statement:** The module frame listener's `OnStopAudio` and `OnResumeAudio` suspend video players and release their codecs, then recreate them and seek to the current media time.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** automated half: `Test_VideoPlayer_SuspendResume_Seeks`; `Test` gate step, `ZenithCinematics_Tests`; the S24 device run record is `needs-human` evidence
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Android lifecycle"

### Z2-R-CINE-127 Capture state machine

- **Statement:** Capture follows the capture state table and requires `cine.capture.enabled`, external ticks with one tick per frame, a capture tick gate, and the offline audio backend when audio is captured.
- **Covers:** Z2-I-CINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_StateMachine_EveryTransition`, `Test_CineCapture_RequiresCaptureConfiguration`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Capture state machine"

### Z2-R-CINE-128 Frame-exact capture

- **Statement:** A capture is lockstep: a tick advances only after the previous tick's frame was read back with every video section showing its ready frame and submitted, and a refused, deferred or failed readback, a busy writer, or a video player not yet ready holds the tick gate and retries the same tick, so frames have no gap or duplicate and do not depend on decode timing.
- **Covers:** Z2-I-CINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-RHI-011
- **Acceptance evidence:** `Test_CineCapture_OneFramePerTick_NoGaps`, `Test_CineCapture_ReadbackPoolFull_PausesAndRetries`, `Test_CineCapture_DeferredFrame_RetriedNoGap`, `Test_CineCapture_VideoSectionStart_WaitsForFirstFrame`, `Test_CineCapture_VideoWait_FailsAfterLimit`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm"

### Z2-R-CINE-129 Capture audio and subtitles

- **Statement:** A capture writes the offline-rendered audio as per-second chunks with exactly each captured tick's frame count and none for a held frame, and the caption cues and cancels indexed by frame.
- **Covers:** Z2-I-CINE-11, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_AudioFramesMatchTicks`, `Test_CineCapture_SubtitlesFrameIndexed`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm"; `Zenith2/Docs/Design/CINE/Formats.md` → "Capture output"

### Z2-R-CINE-130 Capture output format

- **Statement:** A capture directory `user:/devtools/captures/<name>/` holds the TGA frames, the audio chunks, the subtitle file and the manifest, written last, through the development file writer.
- **Covers:** Z2-I-CINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_OneFramePerTick_NoGaps` (format assertions); `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Capture output"

### Z2-R-CINE-131 Capture failure

- **Statement:** A refused write, or a completion failure the development file writer counts, moves the capture to FAILED, logs the path and cancels the outstanding readback.
- **Covers:** Z2-I-CINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_WriteFailure_Failed`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Capture state machine"

### Z2-R-CINE-132 Capture absent in Shipping

- **Statement:** In Shipping and `ShippingTest` the module creates no capture and registers no `cine.capture.*` key.
- **Covers:** Z2-I-CINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_AbsentInShipping` (`ShippingTest` configuration); `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm"

### Z2-R-CINE-133 Export package

- **Statement:** Export writes PNG or copied frames, the WAV assembled from the audio chunks behind its specified header, WebVTT subtitles, and `export.json` last, byte-identically for identical inputs, refusing a capture with a missing frame or chunk.
- **Covers:** Z2-I-CINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CineExport_PngAndVttDeterministic`, `Test_CineExport_WebVttTimes`, `Test_CineExport_ExportJsonWrittenLast`, `Test_CineExport_CorruptManifest_Refused`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Export algorithm"; `Zenith2/Docs/Design/CINE/Formats.md` → "Export output"

### Z2-R-CINE-134 Export validation

- **Statement:** Export refuses a corrupt capture manifest with `CINEMATIC_ERROR_CORRUPT` and an encode request without an admitted encoder with `CINEMATIC_ERROR_VIDEO_UNSUPPORTED`.
- **Covers:** Z2-I-CINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-VIDEO (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_CineExport_CorruptManifest_Refused`, `Test_CineExport_EncodeWithoutAdmittedEncoder_Refused`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Export algorithm"

### Z2-R-CINE-135 Sequencer panel registered

- **Statement:** The Cinematics editor extension registers the Sequencer panel type, the document handler for cinematic documents, the kind 14 asset handler and the player inspector.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-EDIT-001
- **Acceptance evidence:** `Test_EditorSequencer_ExtensionRegistersHandlers`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer panel"

### Z2-R-CINE-136 Sequencer commands undoable

- **Statement:** Every sequencer command produces document edits that undo and redo to byte-identical documents.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSequencerModel_EveryCommand_UndoRoundTrip`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer commands"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer model"

### Z2-R-CINE-137 Binding rename rewrites players

- **Statement:** Renaming a binding rewrites every scene player that names it in the same batch edit.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSequencerModel_RenameBinding_RewritesPlayers`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer commands"

### Z2-R-CINE-138 Preview through the runtime

- **Statement:** The sequencer previews by running the cooked sequence in a preview world with recorded resume, pause, rate and seek commands, scrubbing recording one seek per playhead change.
- **Covers:** Z2-I-CINE-13, Z2-I-CINE-06, Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSequencerModel_ScrubRecordsSeek`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Preview session"

### Z2-R-CINE-139 Camera pilot and key recording

- **Statement:** Piloting binds the viewport camera to the camera track's binding, and `RecordCameraKey` keys pose, FovY and focus distance at the playhead.
- **Covers:** Z2-I-CINE-13, Z2-I-CINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSequencerModel_RecordCameraKey_FromViewport`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Camera pilot and key recording"

### Z2-R-CINE-140 Cook diagnostics in the sequencer

- **Statement:** A cook finding's JSON pointer selects the offending member in the sequencer.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSequencerModel_CookDiagnostics_SelectMember`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook error codes"

### Z2-R-CINE-141 Authoring walkthrough automated

- **Statement:** The authoring walkthrough replays through editor automation into a document that cooks and plays.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-EDIT-001
- **Acceptance evidence:** `Test_EditorSequencer_AuthoringWalkthrough_Automation`; `Test` gate step, `ZenithEditor_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Authoring walkthrough"

### Z2-R-CINE-142 Caption keys extracted

- **Statement:** Every caption key of a cinematic document reaches UI's localization extraction, through ZenithCook's extraction source over ZenithAuthoring's `Zenith_ListCinematicCaptionKeys`, with its document path and JSON pointer, and a missing key is a cook warning.
- **Covers:** Z2-I-CINE-09, Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicCook_EveryRule_Reported` (`cine.cook.caption_key`; `Test` gate step, `ZenithCook_Tests`), `Test_CinematicExtraction_YieldsCaptionKeys` (`Test` gate step, `ZenithAuthoring_Tests`)
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "Localization extraction"

### Z2-R-CINE-143 Counters and zones

- **Statement:** CINE publishes the counters and profiler zones of the diagnostics section per world.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CINE-002
- **Acceptance evidence:** `Test_CinematicDiagnostics_CountersPublished`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Counters"; `Zenith2/Docs/Design/CINE.md` → "Profiler zones"

### Z2-R-CINE-144 Console commands and panel

- **Statement:** Development builds register the `cine.*` console commands, which record their requests as commands at the next tick, and the Cinematics panel provider.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicDiagnostics_ConsoleCommandsRecordRequests`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Console commands"; `Zenith2/Docs/Design/CINE.md` → "On-screen panel"

### Z2-R-CINE-145 CinematicSlice game

- **Statement:** `Zenith2/Games/CinematicSlice` selects the modules of its `game.json`, contains no C++, and carries the four sequences, the video and the director script of the slice table.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-13, Z2-I-GAME-28 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-002
- **Budgets:** Z2-B-PACK-003, Z2-B-CONTENT-003
- **Acceptance evidence:** automated half: the `Configure` gate step for `zenith_add_game(CinematicSlice)`; the first-playable milestone run record is `needs-human` evidence, and `Assets/Video/Flashback.mp4` comes from a `needs-human` content ticket
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice"

### Z2-R-CINE-146 CinematicSlice set-pieces

- **Statement:** CinematicSlice's set-pieces drive VFX and physics events from command tracks within the classified simulations.
- **Covers:** Z2-I-CINE-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-021, D-CLASS-SIM (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** the slice's Stage 5 milestone run record
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice"

### Z2-R-CINE-147 CinematicSlice acceptance

- **Statement:** CinematicSlice's acceptance package exercises start, interrupt and skip with restore, a mid-cinematic save and load and a capture of the intro, and covers every sign-off item of [Z2-N-BUDGET-435].
- **Covers:** Z2-I-CINE-14, Z2-I-CINE-15, Z2-I-CINE-16, Z2-I-CINE-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-SHIP-019, Z2-B-SHIP-021, Z2-B-SHIP-023
- **Acceptance evidence:** the `Games/CinematicSlice/Acceptance/` package (Playthrough.md, Playthrough.replay, FeatureChecklist.md, Budgets.md, BugBar.md, SignOff.md) in the `Zenith2Acceptance` suite
- **Design:** `Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice"

### Z2-R-CINE-148 Spawnable bindings

- **Statement:** A binding flagged spawnable spawns its entity from a prefab at Begin and destroys it at release.
- **Covers:** Z2-I-CINE-01
- **Stage:** S5
- **Classification:** deferred
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_SpawnableBinding_SpawnsAndReleases` (Stage 5); `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "Stage 5"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicAsset header"

### Z2-R-CINE-149 Outbox emission boundary

- **Statement:** Every outbox entry is emitted exactly once: entries queued by `Cinematics.Evaluate` or at the sync points after phases 1–7 in phase 8 of their tick, entries queued at the sync points after phases 8–10 or by the tick-boundary handler in phase 8 of the next tick; `Cinematics.EmitEvents` writes nothing and `Cinematics.Evaluate` removes only emitted entries.
- **Covers:** Z2-I-CINE-05, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_OutboxLateEntries_EmittedNextTickOnce`, `Test_CinematicSave_OutboxLateEntries_Survive`; `Test` gate step, `ZenithCinematics_Tests`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents"

### Z2-R-CINE-150 End tick written

- **Statement:** The tick whose End playback ends an instance (COMPLETED, INTERRUPTED or FAILED) is written by `Cinematics.ApplyTransforms` and `Cinematics.ApplyLate` at weight 0: restore-flagged properties at their snapshot bits, unflagged ones at their held value, so no gameplay writer wins the row in that tick and the restore writes identical bits.
- **Covers:** Z2-I-CINE-07, Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicBlend_OutFlaggedReachesSnapshot`, `Test_CinematicPlayback_NaturalEnd_BlendsOutThenEnds`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "End tick"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"

### Z2-R-CINE-151 Zero blend-in at full weight

- **Statement:** A start with a zero blend-in enters `PLAYING` at weight 1, and `PLAYING` holds weight 1, so the sequence writes the full track value from the tick after Begin.
- **Covers:** Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_ZeroBlendIn_FullWeightFromFirstAdvance`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-152 Presentation resumed after load

- **Statement:** After a restore, presentation resume re-posts from the saved audio list, never from section activity: lost non-time-locked entries in the first tick, even while paused; time-locked entries when the instance runs at rate 1 and is not paused; each with a fresh deterministic ID followed by `Audio.SeekInstance`; it re-posts nothing when `STOP_AUDIO` is pending or the content changed, and re-queues, from the caption list, exactly the captions the instance had listed whose sections are active, because AUDIO and UI save no voice and no caption; a skipped side-work bit is consumed.
- **Covers:** Z2-I-CINE-04, Z2-I-CINE-08, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSave_MidDialogue_AudioRepostedAfterLoad`, `Test_CinematicSave_InterruptTick_AudioStaysStopped`, `Test_CinematicCaptions_InterruptThenPauseResume_NoResurrection`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"; `Zenith2/Docs/Design/CINE/Playback.md` → "Side work"

### Z2-R-CINE-153 Schedule freezes with Physics and Animation

- **Statement:** Every CINE system declares, at attach, reads that no system of its phase writes (`Zenith.WorldTransform` for anchors and parents, `Zenith.Render.Camera` 10, both ANIM resources), so a world with the Physics, Animation and Cinematics modules freezes with no schedule error.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_Schedule_FreezesWithPhysicsAndAnimation`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Systems"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate"

### Z2-R-CINE-154 Script-visible identifiers exact in Luau

- **Statement:** No property or return value a script reads is a 64-bit hash: names and tags are `Zenith_StringID` strings, tickets four `uint32` fields, instance IDs serials below 2⁵³, and caption source IDs below 2⁵³, so no CINE value raises ZS1504.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicScript_MarkerAndTagMatchInLuau`, `Test_CinematicAudioIDs_DistinctPerSection`; `Test` gate step, `Zenith_Integration_Cinematics`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Scope of this file"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvents header"

### Z2-R-CINE-155 ZenithAuthoring contract conformance

- **Statement:** CINE's ZenithAuthoring code is one root header `Zenith_AuthoringCinematics.h` and `Tools/Authoring/Internal/Cinematics/` sources naming no ZenithCook or ZenithEditor type and compiling no other library's `Internal/` source; its cook steps, importer and MP4 reader are in ZenithCook's `Internal/Cinematics/`, whose validators call the public `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile`.
- **Covers:** Z2-I-CINE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `SourceLayout`, `Libgraph` and `ApiContractCheck` gate steps; `Test_CinematicValidation_MatchesLoader`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE.md` → "ZenithAuthoring and ZenithCook files"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Tool-side headers"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook steps"

### Z2-R-CINE-156 Decoded frames cropped to the display region

- **Statement:** Every decoded frame carries its display crop (the MFT aperture, the AImage crop rectangle), and upload and conversion read only the crop, so an aligned decoder height never shows its padding rows.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxVideoConvert_CropRegionOnly`; `Test` gate step, `ZenithFlux_Tests`; `Test_VideoDecoderPlatform_H264High1080p_DecodesAllFrames` (`requiresHardwareVideo`)
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Platform decoder backend"; `Zenith2/Docs/Design/CINE/Video.md` → "GPU side"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Flux_Video header"

### Z2-R-CINE-157 Decoder test seam and pumped video thread

- **Statement:** The module's internal construction desc selects the decoder factory and a fault injector, so tests use the test decoder and fail every Start and Attach step, and the video thread's `PumpOnce` runs its loop on the test's thread for every state-machine test.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_Start_FailsAtStep1_Unwinds` … `Step7`, `Test_VideoPlayer_StateMachine_EveryTransition`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"; `Zenith2/Docs/Design/CINE/Video.md` → "Video thread"; `Zenith2/Docs/Design/CINE/Video.md` → "Zenith_VideoDecoder interface"

### Z2-R-CINE-158 Captions cancelled and re-queued by source

- **Statement:** Every caption CINE queues carries a source ID and an entry of the instance's 16-entry caption list with the expiry tick UI computes (applied before a save for cues accepted in the save tick), and CINE queues `Zenith.Runtime.CaptionCancelEvent` for every listed, unexpired caption on pause, a rate change, seek-out, skip, interrupt and end; a pause or rate change keeps the cancelled entries marked, and a resume, a rate change between non-zero rates or a load re-queues exactly the marked entries whose sections are active, with the remaining duration at the current rate, so a caption an interrupt, skip or end removed never returns; rate 0 cancels without re-queuing.
- **Covers:** Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Subtitle_PauseSeekSkipKeepsCaptionInSync`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicIntegration_CaptionReachesUI`, `Test_CinematicCaptions_InterruptThenPauseResume_NoResurrection`, `Test_CinematicSave_AcceptedCueExpiry_Applied`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks"

### Z2-R-CINE-159 Merge capacity rejection re-runs whole

- **Statement:** CINE's merge functions apply `TryAccept` to each instance buffer; a rejected buffer is discarded with everything staged with it, a seek's time fields included, counted, and its fire items or seek re-run from the same committed state at the next tick, never partly applied.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_CinematicPlayback_MergeCapacityRejection_RerunsWhole`, `Test_CinematicTracks_Late_UnchangedValueRecordsNothing`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate"

### Z2-R-CINE-160 Restore skips removed components

- **Statement:** A restore entry whose component the entity no longer carries is skipped and counted in `cine.snapshot.restore_skipped_delta`, and a component added after Begin is not restored.
- **Covers:** Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSnapshot_RestoreSkipsRemovedComponent`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"

### Z2-R-CINE-161 Own-row writes of rows reached by handle

- **Statement:** `Cinematics.ApplyTransforms` and `Cinematics.Evaluate` write a bound entity's transform and physics weight and a player row only after making that entity the invocation's current entity and reaching its row through `GetChunkOfEntity` with a query slot that declares the write, so the own-row-only columns ECS marks are non-null for the row written and no other row is written.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicsModule_Schedule_FreezesWithPhysicsAndAnimation`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Systems"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"

### Z2-R-CINE-162 Emission acceptance and re-emission

- **Statement:** `Cinematics.EmitEvents` emits the due outbox entries in emission groups of at most 64 entries of one instance, its merge function accepts or rejects each group (rejecting an instance's later groups after a rejection) and records the accepted storage positions in `Zenith.Cinematics.Emission`; `Cinematics.Evaluate` removes only accepted entries, so a rejected group is emitted again at the next tick in order and every event is delivered exactly once.
- **Covers:** Z2-I-CINE-05, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_CinematicPlayback_EmitRejected_ReEmittedOnce`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicSave_EmitRejectedEntries_Survive`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents"; `Zenith2/Docs/Design/CINE/Playback.md` → "Emission acceptance"; `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"

### Z2-R-CINE-163 Outbox sized from the instance pool

- **Statement:** The outbox holds 256 + 10 × `cine.maxInstancesPerWorld` + 64 entries; every instance reserves 10 entries at allocation for its start outcome, `Ended` and fire-on-skip events, a start is admitted only with 10 free entries (1 when the pool is full) and otherwise dropped and counted, and no append can find the store full.
- **Covers:** Z2-I-CINE-05, Z2-I-CINE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-GAME-003, Z2-B-CINE-001
- **Acceptance evidence:** `Test_CinematicPlayback_OutboxBound_SixtyFourSkipsOneTick`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"; `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"

### Z2-R-CINE-164 Fresh audio IDs for every post

- **Statement:** Every audio and dialogue post of an instance, first or re-post, carries the deterministic native ID hashed from the instance serial, the track and section and the instance's post serial, so no post repeats a live ID (AUDIO refuses those), including a stop and re-post within the 50 ms stop fade.
- **Covers:** Z2-I-CINE-04, Z2-I-CINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Audio_PauseResumeWithinFade_Replays`, `Test_CinematicAudioIDs_DistinctPerSection`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Side work"

### Z2-R-CINE-165 Pause and blend-out continuity

- **Statement:** A `PAUSED` instance is written with the blend rule of the state it left at the paused weight, a restore-flagged value blends out as `lerp(B, lerp(B_in, C, w₀), w / w₀)`, and an interrupt of an instance paused from its blend-out is ignored, so no pause, resume or interrupt of a taken-over entity during its blend-in changes a written value in one tick beyond the blend's own per-tick change.
- **Covers:** Z2-I-CINE-07, Z2-I-CINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicBlend_PauseDuringBlendOut_NoPop`, `Test_CinematicBlend_TakeOverInterruptDuringBlendIn_FlaggedNoPop`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Blending"; `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback"

### Z2-R-CINE-166 Ended holder base capture

- **Statement:** A start that binds an entity held by an instance in its end tick captures the holder's end-tick output as its blend-in base at the following tick-boundary step 2.2, before the holder's restore, without marking the holder, so a chained cutscene does not move its unflagged properties back to their pre-cinematic values.
- **Covers:** Z2-I-CINE-07, Z2-I-CINE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_TakeOver_EndedHolder_NotTakenOver`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Take-over of a bound entity"; `Zenith2/Docs/Design/CINE/Playback.md` → "Tick-boundary handler"

### Z2-R-CINE-167 Deterministic slerp

- **Statement:** Every slerp in CINE is computed in binary64 with CORE's deterministic `Zenith_Atan2` and `Zenith_Sin` and rounded to binary32, falling back to a normalised linear blend below a sine of 10⁻⁶, and no simulation-affecting CINE code calls a CRT transcendental.
- **Covers:** Z2-I-CINE-01, Z2-I-CINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicEvaluator_Slerp_MatchesDeterministicFormula`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Curve channels and interpolation"; `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules"

### Z2-R-CINE-168 Bound set saved

- **Statement:** The world record's bound set (at most 96 entries: GUID, content hash, template, holders) is the single source of whether a sequence is bound in the world; it is saved, and the restore rebuilds each entry's template from the resident asset before the first tick, so a start after a load begins inline exactly when the uninterrupted run's did.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicSave_BoundSet_StartBeginsInlineAfterLoad`; `Test` gate step, `Zenith_Integration_Cinematics`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Bound set"; `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"; `Zenith2/Docs/Design/CINE/Formats.md` → "Save section"

### Z2-R-CINE-169 Bindings resolved by the publication handler

- **Statement:** An instance waiting for its sequence's publication has its bindings resolved by CINE's publication handler at tick-boundary step 1 through the boundary's `Zenith_TickBoundaryAccess`, moving to `PREPARING` or failing with its ticket entry and `StartFailed`; the next `Cinematics.Evaluate` checks preparation and records `Cinematics.Begin`.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_StartNotResident_BindsThenBegins`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding"; `Zenith2/Docs/Design/CINE/Playback.md` → "Binding resolution"

### Z2-R-CINE-170 Video events and single-owner player state

- **Statement:** The video thread reports `OPENED`, `OPEN_FAILED`, `CLOSED`, `SUSPENDED`, `RESUMED`, `DEVICE_LOST` and `FAILED` through a 128-entry event ring that never refuses a push, and only the render-submit thread changes a player's state, in response to those events and to the draw records.
- **Covers:** Z2-I-CINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VideoPlayer_StateChangesOnlyFromEvents`, `Test_VideoThread_EventRing_NeverFull`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Video.md` → "Video thread"; `Zenith2/Docs/Design/CINE/Video.md` → "Video players"

### Z2-R-CINE-171 Capture subtitles resolved at export

- **Statement:** A capture writes CINE's own caption keys only, and export resolves each cue's text from the game's string table documents of the chosen locale, writing the key and listing it as unresolved when it is missing.
- **Covers:** Z2-I-CINE-12, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_SubtitlesFrameIndexed`; `Test` gate step, `Zenith_Integration_Cinematics`; `Test_CineExport_WebVttTextResolved`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Subtitle file"; `Zenith2/Docs/Design/CINE/Formats.md` → "Export output"

### Z2-R-CINE-172 Capture and export audio limit

- **Statement:** A capture with audio forces `End` before its audio exceeds 536,870,907 sample frames, and export refuses audio above the `uint32` WAV size limit with `CINEMATIC_ERROR_CAPTURE_IO`.
- **Covers:** Z2-I-CINE-11, Z2-I-CINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CineExport_AudioAboveWavLimit_Refused`; `Test` gate step, `ZenithAuthoring_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Formats.md` → "Capture directory layout"; `Zenith2/Docs/Design/CINE/Formats.md` → "Export output"; `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm"

### Z2-R-CINE-173 Ended camera lens left to the director

- **Statement:** In an instance's end tick `Cinematics.ApplyLate` records no lens or lighting channel of its camera slot, so the camera director GAME resumes at END is the only phase-10 writer of those properties in that tick.
- **Covers:** Z2-I-CINE-02, Z2-I-GAME-29 (GAME)
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Camera_EndTickLensLeftToDirector`; `Test` gate step, `ZenithCinematics_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "End tick"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate"

### Z2-R-CINE-174 Lip-sync face clips follow the voice locale

- **Statement:** A dialogue section's face clip starts in slot `cine.face` at the window the simulation voice-locale tag selects from the cooked window table (exact tag, language subtag, window 0), with `ANIM_SLOT_TAG_CINEMATIC | ANIM_SLOT_TAG_FULL_RATE`, and stops at that window's end through a window-end fire item; the tag is passed at cinematic start, saved with the instance, and the empty tag plays window 0.
- **Covers:** Z2-I-CINE-08, Z2-I-CINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicTracks_Dialogue_LipSyncWindowByVoiceLocale`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicCook_LipSyncWindowTable`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks"; `Zenith2/Docs/Design/CINE/Formats.md` → "Section table section"; `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step"

### Z2-R-CINE-175 Looping minimum duration

- **Statement:** A looping start of a sequence with `D × H` below 8,000,000 (the advance of rate 8) fails with `CINEMATIC_ERROR_INVALID_ARGUMENT`, so one wrap per advance always suffices, and the cook warns with `cine.cook.loop_too_short` naming the minimum at the game's tick rate.
- **Covers:** Z2-I-CINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CinematicPlayback_Loop_WrapsAndFiresKeysOnce`; `Test` gate step, `ZenithCinematics_Tests`; `Test_CinematicCook_EveryRule_Reported`; `Test` gate step, `ZenithCook_Tests`
- **Design:** `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook validation rules"

## Coverage of inventory items

| Item | Name | Requirements |
|---|---|---|
| Z2-I-CINE-01 | timeline sequencing | Z2-R-CINE-001, Z2-R-CINE-002, Z2-R-CINE-003, Z2-R-CINE-004, Z2-R-CINE-005, Z2-R-CINE-007, Z2-R-CINE-008, Z2-R-CINE-009, Z2-R-CINE-010, Z2-R-CINE-011, Z2-R-CINE-012, Z2-R-CINE-013, Z2-R-CINE-014, Z2-R-CINE-016, Z2-R-CINE-017, Z2-R-CINE-018, Z2-R-CINE-019, Z2-R-CINE-020, Z2-R-CINE-021, Z2-R-CINE-023, Z2-R-CINE-024, Z2-R-CINE-027, Z2-R-CINE-028, Z2-R-CINE-029, Z2-R-CINE-031, Z2-R-CINE-032, Z2-R-CINE-033, Z2-R-CINE-034, Z2-R-CINE-035, Z2-R-CINE-037, Z2-R-CINE-038, Z2-R-CINE-040, Z2-R-CINE-041, Z2-R-CINE-042, Z2-R-CINE-043, Z2-R-CINE-044, Z2-R-CINE-045, Z2-R-CINE-046, Z2-R-CINE-047, Z2-R-CINE-064, Z2-R-CINE-067, Z2-R-CINE-072, Z2-R-CINE-088, Z2-R-CINE-093, Z2-R-CINE-094, Z2-R-CINE-095, Z2-R-CINE-097, Z2-R-CINE-098, Z2-R-CINE-100, Z2-R-CINE-101, Z2-R-CINE-102, Z2-R-CINE-103, Z2-R-CINE-104, Z2-R-CINE-105, Z2-R-CINE-106, Z2-R-CINE-107, Z2-R-CINE-108, Z2-R-CINE-109, Z2-R-CINE-143, Z2-R-CINE-144, Z2-R-CINE-145, Z2-R-CINE-148, Z2-R-CINE-153, Z2-R-CINE-154, Z2-R-CINE-159, Z2-R-CINE-161, Z2-R-CINE-167, Z2-R-CINE-168, Z2-R-CINE-169, Z2-R-CINE-175 |
| Z2-I-CINE-02 | camera tracks | Z2-R-CINE-040, Z2-R-CINE-041, Z2-R-CINE-048, Z2-R-CINE-049, Z2-R-CINE-050, Z2-R-CINE-120, Z2-R-CINE-139, Z2-R-CINE-161, Z2-R-CINE-173 |
| Z2-I-CINE-03 | animation tracks | Z2-R-CINE-035, Z2-R-CINE-036, Z2-R-CINE-051, Z2-R-CINE-052, Z2-R-CINE-053, Z2-R-CINE-061, Z2-R-CINE-081, Z2-R-CINE-090, Z2-R-CINE-174 |
| Z2-I-CINE-04 | audio tracks | Z2-R-CINE-006, Z2-R-CINE-039, Z2-R-CINE-054, Z2-R-CINE-055, Z2-R-CINE-056, Z2-R-CINE-090, Z2-R-CINE-123, Z2-R-CINE-152, Z2-R-CINE-164 |
| Z2-I-CINE-05 | event tracks | Z2-R-CINE-006, Z2-R-CINE-038, Z2-R-CINE-057, Z2-R-CINE-058, Z2-R-CINE-059, Z2-R-CINE-065, Z2-R-CINE-066, Z2-R-CINE-068, Z2-R-CINE-078, Z2-R-CINE-091, Z2-R-CINE-105, Z2-R-CINE-146, Z2-R-CINE-149, Z2-R-CINE-154, Z2-R-CINE-162, Z2-R-CINE-163 |
| Z2-I-CINE-06 | timeline scrubbing | Z2-R-CINE-042, Z2-R-CINE-049, Z2-R-CINE-052, Z2-R-CINE-056, Z2-R-CINE-063, Z2-R-CINE-068, Z2-R-CINE-089, Z2-R-CINE-090, Z2-R-CINE-091, Z2-R-CINE-092, Z2-R-CINE-093, Z2-R-CINE-095, Z2-R-CINE-117, Z2-R-CINE-138, Z2-R-CINE-144 |
| Z2-I-CINE-07 | timeline blending | Z2-R-CINE-029, Z2-R-CINE-047, Z2-R-CINE-069, Z2-R-CINE-070, Z2-R-CINE-071, Z2-R-CINE-072, Z2-R-CINE-073, Z2-R-CINE-138, Z2-R-CINE-150, Z2-R-CINE-151, Z2-R-CINE-165, Z2-R-CINE-166, Z2-R-CINE-167 |
| Z2-I-CINE-08 | dialogue sync | Z2-R-CINE-035, Z2-R-CINE-036, Z2-R-CINE-039, Z2-R-CINE-060, Z2-R-CINE-061, Z2-R-CINE-062, Z2-R-CINE-152, Z2-R-CINE-164, Z2-R-CINE-174 |
| Z2-I-CINE-09 | subtitle sync | Z2-R-CINE-059, Z2-R-CINE-062, Z2-R-CINE-063, Z2-R-CINE-090, Z2-R-CINE-129, Z2-R-CINE-142, Z2-R-CINE-149, Z2-R-CINE-152, Z2-R-CINE-158, Z2-R-CINE-162, Z2-R-CINE-171 |
| Z2-I-CINE-10 | video playback | Z2-R-CINE-003, Z2-R-CINE-019, Z2-R-CINE-025, Z2-R-CINE-026, Z2-R-CINE-032, Z2-R-CINE-096, Z2-R-CINE-106, Z2-R-CINE-108, Z2-R-CINE-109, Z2-R-CINE-110, Z2-R-CINE-111, Z2-R-CINE-112, Z2-R-CINE-113, Z2-R-CINE-114, Z2-R-CINE-115, Z2-R-CINE-116, Z2-R-CINE-117, Z2-R-CINE-118, Z2-R-CINE-119, Z2-R-CINE-120, Z2-R-CINE-121, Z2-R-CINE-122, Z2-R-CINE-123, Z2-R-CINE-124, Z2-R-CINE-125, Z2-R-CINE-126, Z2-R-CINE-156, Z2-R-CINE-157, Z2-R-CINE-170 |
| Z2-I-CINE-11 | capture | Z2-R-CINE-127, Z2-R-CINE-128, Z2-R-CINE-129, Z2-R-CINE-130, Z2-R-CINE-131, Z2-R-CINE-132, Z2-R-CINE-147, Z2-R-CINE-172 |
| Z2-I-CINE-12 | export | Z2-R-CINE-133, Z2-R-CINE-134, Z2-R-CINE-171, Z2-R-CINE-172 |
| Z2-I-CINE-13 | cinematics visual authoring | Z2-R-CINE-014, Z2-R-CINE-015, Z2-R-CINE-021, Z2-R-CINE-022, Z2-R-CINE-135, Z2-R-CINE-136, Z2-R-CINE-137, Z2-R-CINE-138, Z2-R-CINE-139, Z2-R-CINE-140, Z2-R-CINE-141, Z2-R-CINE-142, Z2-R-CINE-145, Z2-R-CINE-155 |
| Z2-I-CINE-14 | cinematic interruption | Z2-R-CINE-009, Z2-R-CINE-027, Z2-R-CINE-030, Z2-R-CINE-050, Z2-R-CINE-074, Z2-R-CINE-075, Z2-R-CINE-076, Z2-R-CINE-102, Z2-R-CINE-147, Z2-R-CINE-165 |
| Z2-I-CINE-15 | cinematic skip | Z2-R-CINE-009, Z2-R-CINE-027, Z2-R-CINE-030, Z2-R-CINE-050, Z2-R-CINE-076, Z2-R-CINE-077, Z2-R-CINE-078, Z2-R-CINE-079, Z2-R-CINE-080, Z2-R-CINE-081, Z2-R-CINE-095, Z2-R-CINE-102, Z2-R-CINE-147, Z2-R-CINE-163 |
| Z2-I-CINE-16 | restoring gameplay state after a sequence | Z2-R-CINE-071, Z2-R-CINE-075, Z2-R-CINE-082, Z2-R-CINE-083, Z2-R-CINE-084, Z2-R-CINE-085, Z2-R-CINE-086, Z2-R-CINE-087, Z2-R-CINE-088, Z2-R-CINE-097, Z2-R-CINE-099, Z2-R-CINE-147, Z2-R-CINE-150, Z2-R-CINE-160, Z2-R-CINE-166 |
| Z2-I-GAME-28 | cinematic lifecycle (GAME owns the contract; CINE the implementation) | Z2-R-CINE-009, Z2-R-CINE-028, Z2-R-CINE-045, Z2-R-CINE-145 |
| Z2-I-GAME-29 | cinematic lifecycle (GAME owns the contract; CINE the implementation) | Z2-R-CINE-009, Z2-R-CINE-050, Z2-R-CINE-074, Z2-R-CINE-173 |
| Z2-I-GAME-30 | cinematic lifecycle (GAME owns the contract; CINE the implementation) | Z2-R-CINE-009, Z2-R-CINE-077, Z2-R-CINE-083 |

## Excluded and deferred

| Capability | Classification | Rationale | Decision |
|---|---|---|---|
| plugin or downloadable video decoders | excluded | modules and backends are static libraries selected per game; no dynamic plugins | Z2-D-024 |
| network-streamed video, DRM-protected video | excluded | single-player scope with packaged content; no inventory item asks for it | Z2-D-024 |
| runtime video encoding (in a Player) | excluded | export encodes on the host tool only; shipping Players decode only | Z2-D-024 |
| HEVC playback on both platforms | deferred to the D-SEL-VIDEO ruling (Stage 2) | admitted only when the harness records support on both platforms; H.264 is the Stage 4 baseline | D-SEL-VIDEO (open, Stage 2) |
| FFmpeg backend | deferred to the D-SEL-VIDEO ruling (Stage 2) | compiled only if the ruling admits it, with the LGPL static-linking ruling | D-SEL-VIDEO (open, Stage 2) |
| MP4 encoding in export | deferred to the D-SEL-VIDEO ruling (Stage 2) | criterion V-C9; Z2-R-CINE-134 refuses it until then | D-SEL-VIDEO (open, Stage 2) |
| spawnable bindings | deferred to Stage 5 | Z2-R-CINE-148; the binding flag bit is reserved and always 0 in Stage 4 | none |
| VFX keys and set-pieces | Stage 5 | Z2-R-CINE-066, Z2-R-CINE-146, Z2-R-CINE-148; VFX lands in Stage 5 | Z2-D-021 |
| the D-SEL-VIDEO harness | research (Stage 2) | Z2-R-CINE-110 | D-SEL-VIDEO (open, Stage 2) |
