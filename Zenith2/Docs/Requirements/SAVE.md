# SAVE requirement catalogue

## Purpose

This catalogue breaks the SAVE inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "SAVE Save games & replay", including the Persistence (extended) row) into testable requirements `Z2-R-SAVE-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it and the gate step or acceptance suite that runs it, and the headings of the SAVE design family that specify it. The design's "Requirement index" (`Zenith2/Docs/Design/SAVE.md` → "Requirement index") is the reverse map. SAVE owns every requirement here; where the evidence is another area's test (MOD's runtime tests, SCRIPT's replay matrix, PLAT's resume tests, the participants of the modules), that area is named in the evidence and in `Zenith2/Docs/Design/SAVE.md` → "Areas that consume SAVE".

A requirement's stage is the stage whose acceptance it serves (its fix version); a requirement may be scheduled earlier by the pull-forward rule. The replay requirements are Stage 2 because MOD's Stage 2 headless-equivalence test records and replays through them.

The catalogue is two files with one numbering: this file holds the purpose, the requirements of Stages 1 to 3, the coverage of inventory items (over both files) and the exclusions; `Zenith2/Docs/Requirements/SAVE/Stage4Plus.md` holds the requirements of Stages 4 and 6.

## Requirements

### Z2-R-SAVE-001 ZenithSave library target and declared dependencies

- **Statement:** `Zenith2/Engine/ZenithSave/CMakeLists.txt` is one `zenith_add_library(ZenithSave DEPENDS ZenithCore ZenithReflect ZenithECS ZenithScene ZenithPlatform ZenithCompression ZenithProfiling …)` call with exactly the Architecture row's dependencies, matrix `ZENITH_ALL`, and no dependency on `ZenithScript`, `ZenithAssets`, `ZenithJobs` or `ZenithDocument`.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (declared edges against `declared_deps.json`, no used-but-undeclared edge) and `CompileCoverage` on the Stage 2 replay slice ticket.
- **Design:** Zenith2/Docs/Design/SAVE.md → "ZenithSave target"; Zenith2/Docs/Design/SAVE.md → "Libraries SAVE depends on"; Zenith2/Docs/Design/SAVE.md → "Supported matrix"

### Z2-R-SAVE-002 Contract blocks and results on every SAVE header

- **Statement:** Every namespace-scope struct, class and enum of the ten `ZenithSave` root headers carries its own contract block immediately before it in the Contracts grammar, every type that invokes a callback states `Reentrancy`, every fallible public function returns `Zenith_Result<…, Zenith_SaveError>` or `Zenith_Result<…, Zenith_ReplayError>`, and every header compiles alone.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps over every `ZenithSave` root header, on each ticket that adds a header.
- **Design:** Zenith2/Docs/Design/SAVE.md → "API rules"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Header list"

### Z2-R-SAVE-003 String-free error records

- **Statement:** `Zenith_SaveError` is a 16-byte and `Zenith_ReplayError` a 24-byte trivially copyable record whose first member is its code, carrying no string or view; context text is logged at the point of failure.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `static_assert`s in the headers; `Test_SaveHeader_RejectsEachViolation` and `Test_ReplayLog_IdentityMismatchNamesField` read the records' fields (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveTypes header"; Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE.md → "Error enumerations"

### Z2-R-SAVE-004 Script-dependent SAVE code lives in the runtime bridge

- **Statement:** Every SAVE type that calls ZenithScript (the script-state participant, the world hooks over the asynchronous result table, the completion sink, the publication handler, the `SAVE_REQUEST` consumer, the Stage 3 replay publication observer and source) is in `Zenith2/Engine/ZenithRuntime/Internal/Save/`, the Stage 2 replay core there includes no `ZenithScript` header, and no `ZenithSave` file includes a `ZenithScript` header.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (no `ZenithSave` → `ZenithScript` edge) on the bridge ticket; `ForbiddenCheck` include scan.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Runtime bridge files"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"; Zenith2/Docs/Design/SAVE.md → "Libraries SAVE depends on"

### Z2-R-SAVE-005 Replay header written per Architecture

- **Statement:** `Zenith_ReplayLogWriter::Create` writes Architecture's `ZRPL` header exactly: magic, format version 1, flags, the 16-byte build ID, composition hash, content hash, ECS and physics backend IDs, tick rate and the simulation-affecting configuration list in key byte order.
- **Covers:** Z2-I-SAVE-10, Z2-I-SAVE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_HeaderRoundTrip` (`Test` step, `ZenithSave_Tests`).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE/Formats.md → "Records SAVE writes"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"

### Z2-R-SAVE-006 Tick record layout and input command encoding

- **Statement:** Each `REPLAY_RECORD_TICK` carries world tag, flags, tick index, the tick's input commands as 18-byte records in (player slot, sequence) order, its publications in completion-ID order and the tick hash, byte for byte as Architecture's record table states.
- **Covers:** Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_TickRecordLayout` against an independently built byte string (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Records SAVE writes"; Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-007 Publications recorded in completion-ID order

- **Statement:** The recorder records every publication of a world's tick boundary, with its kind, source kind and the payload of every recorded kind, in completion-ID order in that tick's record.
- **Covers:** Z2-I-SAVE-10, Z2-I-SAVE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_TickRecordLayout`; `Test_SaveRuntime_RecordReplayRoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"; Zenith2/Docs/Design/SAVE/Formats.md → "Records SAVE writes"

### Z2-R-SAVE-008 Replay records committed in a deterministic order

- **Statement:** `CommitFrame` appends a frame's completed records in ascending (world session ordinal, tick index), after the frame's swap and begin records, so the log bytes do not depend on the order workers finished worlds.
- **Covers:** Z2-I-SAVE-10, Z2-I-SAVE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_CommitOrderIndependentOfWorlds` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Records SAVE writes"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"

### Z2-R-SAVE-009 Tick hash recorded at the next tick boundary

- **Statement:** The hash of tick t is computed from `HashTickState` of the world after tick t's end-of-tick playback plus SCRIPT's checkpoint contribution, at tick t + 1's boundary before any publication (or out of tick at a swap or shutdown), and a tick's record is committed only once its hash is set.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_PendingHashHoldsRecord`; `Test_SaveRuntime_RecordReplayRoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay verification"

### Z2-R-SAVE-010 Per-tick limits mark a tick non-replayable

- **Statement:** A tick with more than 4,096 input commands or 4,096 publications records the first 4,096 of each, sets `REPLAY_TICK_FLAG_NON_REPLAYABLE` and counts `save.replay.limit_drops`; the reader refuses a record above either limit with `REPLAY_ERROR_LIMIT`.
- **Covers:** Z2-I-SAVE-10, Z2-I-SAVE-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_LimitMarksNonReplayable`; `Zenith_Fuzz_ReplayLog` (`Test` step; `Sanitizers -Preset win-asan`).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Replay limits"; Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-011 Interrupted recordings stay readable

- **Statement:** A log cut at any byte offset opens with every complete record before the cut, reports `IsTruncated`, and is never read past its end.
- **Covers:** Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_TruncatedLogReadable` (every offset of the last record) (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Replay limits"; Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-012 Replay identity check names the mismatched field

- **Statement:** `CheckIdentity` compares build ID, composition hash, content hash, ECS backend, physics backend and tick rate in that order and returns `REPLAY_ERROR_IDENTITY` naming the first mismatch, which the runtime turns into exit code 5 with that name.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_IdentityMismatchNamesField`; `Test_Replay_DifferentBuildRefused` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"

### Z2-R-SAVE-013 Unknown replay records skipped by length

- **Statement:** The reader skips a record of an unknown kind, and unknown trailing bytes of a known record, by the record's byte length.
- **Covers:** Z2-I-SAVE-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_UnknownRecordSkipped` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Replay limits"; Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-014 World sessions and swaps in the log

- **Statement:** Every world creation appends `WORLD_BEGIN` and every transition `WORLD_SWAP` naming the old world's last executed tick then the new `WORLD_BEGIN`, and the reader exposes one session per `WORLD_BEGIN` with its ordinal, tag, seed, scene, tick range and swap tick.
- **Covers:** Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_WorldSwapSessions` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Records SAVE writes"; Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-015 Host-file sink with its writer thread

- **Statement:** Under `--record`, the host-file sink appends committed bytes on `Zenith.ReplayWrite`, flushes every `replay.flushIntervalMs` and at `Close`, which joins the thread; the sink opens only through the command-line grant and refuses in Shipping.
- **Covers:** Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_HostFileSinkFlushes` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE.md → "Threads SAVE owns"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"

### Z2-R-SAVE-016 A sink error stops recording and keeps the prefix

- **Statement:** A sink error sets the writer failed, counts `save.replay.write_errors`, drops subsequent records and leaves the committed prefix readable, and the run continues.
- **Covers:** Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_SinkErrorStopsRecording` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "State machines"

### Z2-R-SAVE-017 Running build ID

- **Statement:** `Zenith_GetRunningBuildID` returns the 16-byte CodeView RSDS GUID of `__ImageBase` on Windows and, on Android and Linux, the first 16 bytes of the `NT_GNU_BUILD_ID` note of the ELF object that contains the function itself (found with `dladdr` and matched by load base in `dl_iterate_phdr`, so on Android it is the game's shared object, not `app_process64`); it is stateless, returns the same value on every call, returns the all-zero ID when the image carries none and never logs (the runtime logs that case once at startup step 11).
- **Covers:** Z2-I-SAVE-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_BuildID_RunningIsStable` (`win` and `linux-tsan`), `Test_BuildID_MatchesOwnObjectNote` (`linux-tsan`) (`Test` step); the Android value is read in the Stage 3 S24 device run record (recorded by the `needs-human` S24 device ticket).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE.md → "ZenithSave target"

### Z2-R-SAVE-018 Replay verifier reports the first mismatch

- **Statement:** `Zenith_ReplayVerifier::Check` records the first tick whose replayed hash differs from the recorded one and calls the desc's callback exactly once for it, so the composition root can record a test assertion failure (exit code 5).
- **Covers:** Z2-I-SAVE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayVerifier_FirstMismatchReported` (`Test` step); `Test_SaveRuntime_ReplayMismatchExitsFive` (`Test` step, `Zenith_Integration_SaveRuntime`); MOD's `Test_Runtime_HeadlessMatchesWindowedSimulation`.
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay verification"

### Z2-R-SAVE-019 Replay playback supplies inputs and verifies hashes

- **Statement:** Under `--replay`, the Stage 2 core player feeds phase 1 each tick's input commands from the log, caps the clock so no tick beyond the log or at or after a `NON_REPLAYABLE` tick runs, and verifies every tick hash, depending on no SCRIPT interface.
- **Covers:** Z2-I-SAVE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimeReplayCore_ReplayVerifiesWithoutScript` (`Test` step, `ZenithRuntime_Tests`); MOD's `Test_Runtime_HeadlessMatchesWindowedSimulation` (Z2-R-MOD-074).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"; Zenith2/Docs/Design/SAVE.md → "Runtime bridge files"

### Z2-R-SAVE-020 Replay stops before a non-replayable tick

- **Statement:** A replay runs every tick before the first `NON_REPLAYABLE` tick, stops there, reports that tick and exits with code 5.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ReplayStopsBeforeNonReplayable` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"; Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"

### Z2-R-SAVE-021 Ticks with script failures replay identically

- **Statement:** Replays of the script fault-matrix scenario (script error, budget exhaustion, snapshot-cap overrun, merge-time rejection) reproduce every tick hash with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend.
- **Covers:** Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Replay_ScriptFailureTickReplaysIdentically` (`Test` step, `Zenith_Integration_SaveRuntime`), with SCRIPT's `Test_Script_Determinism_ReplayMatrix`.
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"; Zenith2/Docs/Design/SAVE/Tests.md → "Runtime integration tests"

### Z2-R-SAVE-022 Saves under replay publish the recorded result

- **Statement:** In replay mode a save request is captured but never written and never completed live, and the world receives the recorded result at its recorded tick as its only publication.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ReplayModeSuppressesWrites`; `Test_SaveRuntime_SaveDuringReplayUsesRecordedResult` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"

### Z2-R-SAVE-023 ZSAV header layout and CRC

- **Statement:** Every save file begins with the 128-byte ZSAV header of the table "File header", little-endian, ending with the CRC-32C of bytes 0 to 123; the parser checks the CRC before any other field, and the header sequence is at least 1 for every kind except kind 6, whose sequence and UTC seconds are 0.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveHeader_RoundTrip`, `Test_SaveHeader_RejectsEachViolation` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "File header"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveFormat header"

### Z2-R-SAVE-024 Section table with one descriptor per section

- **Statement:** The section table holds one 56-byte entry per section with its kind, version, world index, flags, key, offset, one 24-byte `Zenith_CompressedSectionDesc` and the CRC-32C of its stored and decoded bytes, and the reader enforces every rule of "Section table".
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSectionTable_RejectsEachViolation`, `Test_SaveImageBuilder_LayoutOrder`, `Test_SaveImageView_DecodeChecksBothCrcs` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Section table"; Zenith2/Docs/Design/SAVE/Formats.md → "Section kinds"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveFormat header"

### Z2-R-SAVE-025 CRC-32C implementation

- **Statement:** `Zenith_SaveCrc32c` computes CRC-32C (Castagnoli, reflected 0x82F63B78) with portable slicing-by-8 tables, matching the five vectors of "CRC-32C" and equal under any split through `Zenith_SaveCrc32cUpdate`.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrc32c_KnownVectors`, `Test_SaveCrc32c_UpdateEqualsWhole` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "CRC-32C"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveFormat header"

### Z2-R-SAVE-026 The codec comes from each section's descriptor

- **Statement:** Sections are compressed with `save.compressionCodec` at `save.compressionLevel` (stored when not smaller), METADATA and THUMBNAIL are always stored, and a reader decodes every section through `Zenith_DecompressSection` with the codec its descriptor names, so zlib and zstd saves both load.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveImageBuilder_CompressesAndStores`, `Test_SaveImageBuilder_BothCodecs`, `Test_SaveImageBuilder_CompressFaultInjected` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Section kinds"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"

### Z2-R-SAVE-027 Save format limits and untrusted input

- **Statement:** Every save reader enforces the table "Format limits" with overflow-checked arithmetic and returns a `Zenith_SaveError` for any violation, never an assert or an out-of-bounds read.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_SaveImage`, `Zenith_Fuzz_SaveMetadata` (`Sanitizers -Preset win-asan` smoke); the rejection tests of "Format and codec tests".
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Format limits"; Zenith2/Docs/Design/SAVE/Formats.md → "Conventions for every SAVE format"; Zenith2/Docs/Design/SAVE/Tests.md → "Fuzz targets"

### Z2-R-SAVE-028 Slot metadata section

- **Statement:** Every save carries a stored METADATA section at index 0 with slot name, display name (at most 128 bytes), playtime (the primary world's playtime ticks, which continue across scene transitions and loads, divided by `sim.tickHz`), world count and at most 32 sorted key/value pairs, which the builder refuses and the parser rejects outside those limits.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMetadata_LimitsAndOrder`, `Test_SaveSystem_PlaytimeContinuesAcrossWorlds` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Metadata section"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveFormat header"

### Z2-R-SAVE-029 Optional thumbnail section

- **Statement:** A save carries a THUMBNAIL section (RGBA8, at most 512 × 288) exactly when the world has a thumbnail set through `SetThumbnail`; headless runs and worlds without one write none.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveThumbnail_RoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Thumbnail section"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-030 Image bytes are deterministic

- **Statement:** Building the same captured sections with the same header values produces byte-identical files.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveImageView_DeterministicBytes` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"; Zenith2/Docs/Design/SAVE/Formats.md → "The ZSAV file"

### Z2-R-SAVE-031 Save version readability rule

- **Statement:** A reader accepts format major `uSAVE_FORMAT_VERSION_MAJOR` with any minor at or below the current minor, reports a newer version `SAVE_ERROR_VERSION_NEWER` and another major `SAVE_ERROR_VERSION_MAJOR`, both as recoverable results.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveHeader_RejectsEachViolation`, `Test_SaveMigration_OtherMajorRefusedRecoverably` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveFormat header"; Zenith2/Docs/Design/SAVE/Formats.md → "File header"

### Z2-R-SAVE-032 Journaled save write sequence

- **Statement:** A save write performs steps 0a to 5 of "Journaled write sequence": journal PREPARED, backup, temporary, COMMITTING with an atomic rename, journal deleted, each file write through PLAT's atomic write.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_WriteNewSlot`, `Test_SaveJournal_WriteReplacesAndBacksUp`, `Test_SaveJournal_EncodeDecodeRoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveJournal header"; Zenith2/Docs/Design/SAVE/Formats.md → "The ZSJN journal"

### Z2-R-SAVE-033 One backup per save

- **Statement:** A successful write of a slot with a valid previous save leaves exactly that previous save as `<slot>.zsave.bak`, and the resume kind keeps no backup.
- **Covers:** Z2-I-SAVE-05, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_WriteReplacesAndBacksUp`, `Test_SaveJournal_ResumeKindHasNoBackup` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Slot policy"; Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"

### Z2-R-SAVE-034 An invalid current save never replaces the backup

- **Statement:** Before writing, the journaled write validates the current save fully (header, table, and every section decoded once with both CRCs checked); when it fails validation, the write sets `OLD_INVALID`, skips the backup step and leaves the existing backup unchanged.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_InvalidOldNeverOverwritesBackup` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"

### Z2-R-SAVE-035 Journal recovery table

- **Statement:** Start-up recovery handles each journal state as the table of "Journal recovery" states: roll back PREPARED and BACKED_UP, roll forward a WRITTEN journal with a valid temporary, finish or roll back COMMITTING, restore a torn target from its backup, and keep conflicting or corrupt journals renamed for diagnosis.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_CommittingTornTargetRestoresBackup`, `Test_SaveJournal_ConflictKeepsFiles`, `Test_SaveJournal_CorruptJournalQuarantined`, `Test_SaveJournal_StaleTemporaryRemoved` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journal recovery"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveJournal header"

### Z2-R-SAVE-036 Recovery converges when re-run

- **Statement:** Recovery interrupted at any action and run again leaves the same files as one uninterrupted recovery, because every action is idempotent and the journal is deleted last.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_RecoveryInterruptedAndRerunConverges` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journal recovery"

### Z2-R-SAVE-037 Power loss at every step leaves old or new

- **Statement:** For a power loss injected before each journal step and after the rename, with and without a previous save, recovery leaves the target exactly the old save or exactly the new one, a valid backup, no temporary and no journal.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_InterruptedAtEachStep_Recovers` (14 cases, the save instance of [Z2-N-CONTRACT-542]) (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Journal recovery"; Zenith2/Docs/Design/SAVE/Tests.md → "Journal and power-loss tests"

### Z2-R-SAVE-038 Process kill during a real write

- **Statement:** On the headless host with the real asynchronous I/O, a child process killed at each fault point of a save write leaves, after recovery in a fresh process, the old or the new save.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_RealStorage_PowerLossByProcessKill` (`Test` step, `win` and `linux-tsan`).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Journal and power-loss tests"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Journal recovery"

### Z2-R-SAVE-039 Low disk refuses before writing

- **Statement:** A save write checks `QueryFreeBytes() ≥ 2·|N| + |O| + GetLowDiskMarginBytes()` (PLAT's `io.lowDiskMarginBytes` on real storage) before writing anything and otherwise fails `SAVE_ERROR_LOW_DISK` with no file created or changed.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_LowDiskWritesNothing`, `Test_SaveJournal_DiskFullMidWrite` (`Test` step); `Test_SavePlatform_LowDiskEventSurfaced` (`Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"; Zenith2/Docs/Design/SAVE.md → "Failure handling"

### Z2-R-SAVE-040 Write cancellation before the commit point only

- **Statement:** A write cancelled before journal steps 1, 2 or 3 completes CANCELLED with the slot unchanged, and a cancellation requested from step 4 on is ignored and the write completes SUCCEEDED.
- **Covers:** Z2-I-SAVE-05, Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_CancelBeforeEachStep` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"; Zenith2/Docs/Design/SAVE.md → "Cancellation"

### Z2-R-SAVE-041 Newer-version saves are not overwritten implicitly

- **Statement:** A write over a save of a newer format version fails `SAVE_ERROR_SLOT_NEWER_VERSION` and writes nothing unless the request sets `m_bOverwriteNewer`.
- **Covers:** Z2-I-SAVE-08, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_NewerSlotRefused` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveJournal header"

### Z2-R-SAVE-042 Save storage confined to the user mount

- **Statement:** Every path SAVE reads or writes is a validated user-mount virtual path under `saves/`, built only from validated slot names and profile keys, and no save path is ever under `Games/<Name>/`.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_SavePlatform_PathsConfinedToUserMount` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveStorage header"

### Z2-R-SAVE-043 Phase-11 snapshot system

- **Statement:** Each bound world has one phase-11 system `Save.Snapshot` declaring reads of every serialized component type and of each participant's resources and a write of `Zenith.SaveSnapshotBuffer` alone, which records no command.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_WriteAtPhase11` (`Test` step); the schedule compile of every integration suite (a forbidden write is a schedule-compile error).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"; Zenith2/Docs/Design/SAVE.md → "Frame-graph nodes SAVE runs"

### Z2-R-SAVE-044 Every serialized component captured through reflection

- **Statement:** The snapshot writes, for every saved entity, every component unless its type has at least one property and every property is transient (so a zero-property tag component is a record with no fields), as a binary field-set record in ascending type ID, captured with `bSerializedOnly`, for C++-registered and data-defined types alike on every ECS backend; a capture error fails the save with `SAVE_ERROR_SERIALIZE` and the type ID.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-039, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_RoundTripAllPropertyTypes`, `Test_Snapshot_TagComponentsRestored`, `Test_Snapshot_TransientPropertiesSkipped`, `Test_Snapshot_CaptureFieldSetErrorFails`, per backend (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Snapshot capture"; Zenith2/Docs/Design/SAVE/Formats.md → "Entities section"

### Z2-R-SAVE-045 Entity references survive through save-local references

- **Statement:** An entity reference is saved as its target's persistent ID, as the save-local simulation-ID form for a runtime-only target, or as null for a stale handle, and restores to the restored target.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveEntityReference_Forms`, `Test_Snapshot_RuntimeEntityReferencesSurvive`, `Test_Snapshot_StaleReferenceSavedAsNull` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Saved entity references"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_WorldSnapshot header"

### Z2-R-SAVE-046 Pending-destroy entities are not saved

- **Statement:** Entities pending destroy at the snapshot point are not saved, and a reference to one is saved as null without counting it as unresolved.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_PendingDestroyExcluded`, `Test_Snapshot_StaleReferenceSavedAsNull` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Snapshot capture"; Zenith2/Docs/Design/SAVE/Formats.md → "Entities section"

### Z2-R-SAVE-047 Carried events saved with merge keys

- **Statement:** The events emitted in phases 9 and 10 of the snapshot tick are saved with their type, merge key and fields, and the restored world delivers them in phase 9 of the next tick in the original order.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_CarriedEventsRestoredInOrder` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Events section"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-048 Asynchronous state saved and restored

- **Statement:** A save records the world's next completion ID (in ASYNC only), its unpublished results with their payloads verbatim and its in-flight requests; an ordinary restore republishes unpublished results with their saved payloads at the first tick boundary, leaves in flight every request of a source kind a participant declared (re-issued by it under the same completion ID, or abandoned when it does not mark it), completes the image's own `ASYNC_SOURCE_SAVE` request (the header's own completion ID) with its success payload rebuilt byte for byte from the header, any other in-flight `ASYNC_SOURCE_SAVE` request with `SAVE_ERROR_CANCELLED` and any other undeclared one with the zero-length abandoned payload; an image written for a request with a completion ID holds that request in flight, an out-of-tick, resume, exit-autosave or replay-checkpoint image holds none; a restore under a publication source keeps every GENERIC_RECORDED and SAVE entry in flight so its only publication is the recorded one.
- **Covers:** Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_AsyncStateRestored`, `Test_Snapshot_AsyncStateUnderPublicationSource` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Async section"; Zenith2/Docs/Design/SAVE/Formats.md → "Publication payloads"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-049 Snapshot bytes are independent of workers and layout

- **Statement:** The decoded sections of a world's snapshot are byte-identical with 1, 2, 8 and all workers, with forced compaction and with a different chunk capacity, on each backend.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_BytesIndependentOfWorkersAndLayout` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Snapshot capture"; Zenith2/Docs/Design/SAVE.md → "Determinism obligations"

### Z2-R-SAVE-050 Participant interface for state outside reflection

- **Statement:** Every library or module whose simulation state lives outside reflected components and world resources saves and restores it through one `Zenith_SaveParticipant` per world, with a stable name, a key derived by SHA-256, a section version, declared phase-11 reads and a per-capture fidelity, and its capture members change no simulation state.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_ParticipantOrderAndAbsence`, `Test_Snapshot_FidelityPerCapture` (`Test` step); each module's participant tests (PHYS, ANIM, AI, CINE).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; Zenith2/Docs/Design/SAVE.md → "Upward-callback interfaces SAVE declares"

### Z2-R-SAVE-051 Participants restore in order and absent sections reset

- **Statement:** Participants restore in registration order after every entity and world resource exists, and a participant without a section in the save receives `m_bSectionPresent = false` and resets.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_ParticipantOrderAndAbsence` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"

### Z2-R-SAVE-052 A required participant must be present

- **Statement:** A save whose REQUIRED section has no registered participant fails to restore with `SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING`, and a non-required unmatched section is carried and written again.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_RequiredParticipantMissing` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE/Formats.md → "Unknown carry section"

### Z2-R-SAVE-053 Unknown components and sections are carried

- **Statement:** A component of a type the registry lacks and a section the build does not understand are kept in the world's unknown carry, logged once, and written unchanged by the next save of the world.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_UnknownComponentCarried`, `Test_SaveMigration_UnknownFieldsPreservedInCarry` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Unknown carry section"; Zenith2/Docs/Design/SAVE/Formats.md → "Entities section"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_WorldSnapshot header"

### Z2-R-SAVE-054 Image budget enforced

- **Statement:** A capture or image above `save.imageArenaBytes` or 256 MiB fails `SAVE_ERROR_IMAGE_TOO_LARGE` with the world unchanged and the failure published.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SAVE-001
- **Acceptance evidence:** `Test_Snapshot_ImageTooLarge`, `Test_SaveImageBuilder_TooLarge` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Snapshot capture"; Zenith2/Docs/Design/SAVE.md → "In-memory structures"

### Z2-R-SAVE-056 Out-of-tick snapshot of a paused world

- **Statement:** A save of a paused world requested between frames is captured at once out of tick through the world's inspector and equals the phase-11 capture of the same state.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_OutOfTickPausedWorld` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Out-of-tick snapshot"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-057 Requests queued in merge order

- **Statement:** Script and C++ save requests are queued in merge order, receive per-world serials and completion IDs in that order, are copied at intake (at most 12 KiB per request), and are serialized in phase 11 of the tick they were queued in unless the capture is deferred for lack of a free arena.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_RequestsInMergeOrder`, `Test_SaveSystem_WriteAtPhase11` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"

### Z2-R-SAVE-058 Per-tick request limit

- **Statement:** The per-tick save-request capacity is one number, `save.maxRequestsPerTick`, the value of Z2-B-SCRIPT-025: SCRIPT's merge rejects a script request over it as an entity-level failure (no SAVE error and no Luau error), and SAVE's intake refuses a C++ request over it before allocating any completion ID of its own, publishing every over-limit request of the tick together as one `SAVE_OPERATION_REFUSED_BATCH` result with `SAVE_ERROR_REQUEST_LIMIT`, the exact count and the first 4,000 serials.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** Z2-B-SCRIPT-025
- **Acceptance evidence:** `Test_SaveSystem_PerTickLimit` (`Test` step); SCRIPT's merge-rejection test for `SCRIPT_CAPACITY_CLASS_SAVE_REQUESTS` (BR-MERGE).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE/Formats.md → "Publication payloads"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/SAVE.md → "Configuration keys"

### Z2-R-SAVE-059 Queue-full and superseded results

- **Statement:** A request beyond `save.maxQueuedRequests` completes `SAVE_ERROR_QUEUE_FULL`, and of two writes of one slot in one tick the first, while still pending, completes `SAVE_ERROR_SUPERSEDED` and only the second is written; a request already captured is never superseded.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_QueueFull`, `Test_SaveSystem_SupersededInSameTick` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Write request state machine"

### Z2-R-SAVE-060 Save results published at a tick boundary

- **Statement:** Every save, listing, delete and load outcome is completed into the world's asynchronous result table as `ASYNC_SOURCE_SAVE` with the 64-byte payload and published at a tick boundary in completion-ID order, recorded in the replay log.
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_WriteAtPhase11`, `Test_SaveRuntime_ScriptRequestWritesAndPublishes` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Publication payloads"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Write request state machine"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-061 The frame never waits for save I/O

- **Statement:** Compression and every storage operation of a save run on `Zenith.SaveWriter`, so no frame waits on SAVE beyond the phase-11 capture.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_FrameNeverWaits` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"; Zenith2/Docs/Design/SAVE.md → "Threads SAVE owns"

### Z2-R-SAVE-062 Capture deferral when arenas are busy

- **Statement:** When both ordinary capture arenas are held by the writer, pending requests (a due autosave included, with the completion ID and serial it was given at its due tick) stay queued and are captured at the next phase 11 with a free arena, counted in `save.snapshot.deferred`, and their results name the subsequent snapshot tick.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SAVE-001
- **Acceptance evidence:** `Test_SaveSystem_DeferredCaptureWhenArenasBusy` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"; Zenith2/Docs/Design/SAVE.md → "In-memory structures"

### Z2-R-SAVE-063 Slot names validated

- **Statement:** `Zenith_ParseSaveSlotName` accepts 1 to 32 bytes of `[a-z0-9_-]` not starting with `-` or `_`, refuses the Windows reserved device names (`con`, `prn`, `aux`, `nul`, `com1`…`com9`, `lpt1`…`lpt9`) for every kind, and refuses the reserved engine names for manual saves.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSlotName_Rules` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveTypes header"; Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"

### Z2-R-SAVE-064 Slot policy per save kind

- **Statement:** Manual, quick, autosave, checkpoint and resume saves use the directory, slot name, backup and retention of the table "Slot policy".
- **Covers:** Z2-I-SAVE-07, Z2-I-SAVE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_AutosaveRotation`, `Test_SaveSystem_CheckpointRetained`, `Test_SaveJournal_ResumeKindHasNoBackup` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Slot policy"

### Z2-R-SAVE-065 Autosave rotation

- **Statement:** Autosaves rotate over `save.autosaveSlotCount` slots, choosing the first slot with no valid save and otherwise the lowest sequence.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_AutosaveRotation` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Slot policy"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Autosave scheduling"

### Z2-R-SAVE-066 Deterministic autosave schedule and flag

- **Statement:** An engine autosave is due in phase 11 of a tick that is a multiple of `save.autosaveIntervalTicks` on the primary world while its autosave flag is enabled and no autosave completion ID of the world is allocated and unpublished; its completion ID and serial are allocated on that phase-11 node whatever the writer's progress, arena availability or recording, so its identity is a function of simulation state and recorded publication ticks alone: a replay allocates exactly the recorded run's autosaves, and two live runs agree when their SAVE results publish at the same ticks; the flag is saved with the world.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_AutosaveDeterministicIdentity` (gated storage, 1 and 8 workers), `Test_SaveSystem_AutosaveIdentityReplaysAcrossWriterSpeeds` (record then replay), `Test_SaveSystem_AutosaveRotation` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Autosave scheduling"; Zenith2/Docs/Design/SAVE/Formats.md → "World section"

### Z2-R-SAVE-067 Checkpoint saves retained in memory

- **Statement:** A checkpoint save writes the `checkpoint` slot and hands the world's decoded image to the main thread, where `AcquireRetainedCheckpoint` returns a lease that pins the world's checkpoint holder, so a world restores from it without I/O and a newer checkpoint never replaces or frees one that is leased.
- **Covers:** Z2-I-SAVE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** Z2-B-SAVE-001
- **Acceptance evidence:** `Test_SaveSystem_CheckpointRetained`, `Test_SaveSystem_RetainedCheckpointLeaseDefersReplacement` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Checkpoints"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-068 Checkpoint and autosave entry points for the runtime

- **Statement:** The thermal `RequestCheckpointSave` requests a checkpoint through `SubmitOutOfTick` with no completion ID, and the device-loss clean exit requests, after the last `RunFrame`, an out-of-tick autosave that preempts the running ordinary write, is never cancelled by `BeginShutdown`, and is waited for at most `uSAVE_EXIT_AUTOSAVE_BUDGET_MS` (6,000 ms) from its submission and then abandoned with rollback within `uSAVE_EXIT_ROLLBACK_WAIT_MS` (1,000 ms), so it is written before exit code 6 inside the 10 s clean exit of [Z2-N-CONTRACT-484].
- **Covers:** Z2-I-SAVE-02, Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_DeviceLossAutosave`, `Test_SaveRuntime_ThermalCheckpoint`, `Test_SaveSystem_ExitAutosaveSurvivesShutdown` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/SAVE.md → "Failure handling"

### Z2-R-SAVE-069 Slot listing states

- **Statement:** A listing reports every slot of the active profile in byte order of name with its kind, format version, sequence, timestamps, playtime, size, display name and one of the five slot states.
- **Covers:** Z2-I-SAVE-07, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ListSlotsStates`, `Test_SaveRuntime_ListSlotsEvents` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Listing slots"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-070 Listing reads a prefix only

- **Statement:** A listing reads at most `uSAVE_LISTING_PREFIX_BYTES` (128 KiB, enough for a header, a 1,024-entry section table and a maximal METADATA section) of each file and decodes no compressed section.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ListReadsPrefixOnly`, `Test_SaveListing_MaximalPrefixFits` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Listing slots"

### Z2-R-SAVE-071 Slot deletion order

- **Statement:** Deleting a slot removes the save, then its backup, then its quarantined copy, so an interruption leaves a slot that lists as recoverable or empty, never corrupt.
- **Covers:** Z2-I-SAVE-07, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_DeleteSlotOrder` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Slot policy"

### Z2-R-SAVE-072 Save sequences strictly increase

- **Statement:** Every save of a profile takes a header sequence strictly above every earlier save of that profile, across kinds and across restarts.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_SequenceMonotonic` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"; Zenith2/Docs/Design/SAVE/Formats.md → "File header"

### Z2-R-SAVE-073 Load verification and backup fallback

- **Statement:** A load verifies the header, table and both CRCs of every section and, on a validation failure, loads the slot's backup when it verifies, reporting `m_bFromBackup`.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_LoadFallsBackToBackup`, `Test_SaveSystem_LoadBothCorruptFails` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Verification and corruption recovery"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Load operation state machine"

### Z2-R-SAVE-074 Damaged saves quarantined

- **Statement:** A save that failed validation while its backup loaded is renamed `<slot>.zsave.corrupt`, replacing an older quarantined copy, and the next write of the slot keeps the backup.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_LoadFallsBackToBackup` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Verification and corruption recovery"; Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"

### Z2-R-SAVE-075 Load cancellation releases on the writer thread

- **Statement:** A load cancelled during the read or migration releases its arenas on the writer thread before completing CANCELLED.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_LoadCancelReleases` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Load pipeline"; Zenith2/Docs/Design/SAVE.md → "Cancellation"

### Z2-R-SAVE-076 Incremental restore within a frame budget

- **Statement:** A restore advances through the states of "Restore pipeline" on the main thread out of tick, returning after `save.restoreBudgetUs` once a batch completes, until DONE or FAILED.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Restore_BudgetRespected` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore job state machine"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_WorldSnapshot header"

### Z2-R-SAVE-077 Restore preserves simulation identity

- **Statement:** A restore calls `RestoreSimulationState` before any playback and spawns every entity with its saved simulation ID and persistent ID, so the next spawn continues the saved counter.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_SimulationIDsPreserved`, `Test_Restore_ResetsChangeTracking`, `Test_Snapshot_HierarchyRestored` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-078 Restored state independent of batch size

- **Statement:** The restored world is identical for every entities-per-batch value.
- **Covers:** Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Restore_BatchSizeDoesNotChangeState` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-079 Exact restores continue bit-identically under the delivery rule

- **Statement:** A world restored from an EXACT save (no suspended `Run` coroutine, no quarantined or disabled instance, no in-flight additive load, every participant EXACT for that capture) continues with per-tick hashes equal, on each ECS backend, to the uninterrupted run in which every `ASYNC_SOURCE_SAVE` entry in flight at the snapshot published at the boundary of tick t + 1 with the payload the restore gives it; this holds for an out-of-tick capture taken with no `ASYNC_SOURCE_SAVE` entry in flight (nothing pending, captured or being written; `IsDrained()` true) and for a request-written save whose reference run completed the write before that boundary with no other SAVE request in flight; a live-versus-live comparison further holds only when no request of a source whose completion time depends on I/O or scheduling is in flight at the snapshot or issued in the compared window, and any other EXACT continuation is checked by a replay resumed from the save against the original run's recording.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Restore_ContinuationMatchesUninterrupted` (out-of-tick and gated request-written cases, plus the late-gate negative case, over a world that issues no request of another timed source) (`Test` step); `Test_SaveCrossSystem_PhysicsAndAnimationCoherent` (the EXACT variant, compared through a resumed replay against the recording; Stage 4).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE/Formats.md → "Async section"

### Z2-R-SAVE-080 A failed restore leaves the world for destruction

- **Statement:** Any restore error moves the job to FAILED with the error, the caller destroys the isolated world, and nothing leaks.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Restore_FailureLeavesWorldForDestruction` (`Test` step, Debug leak table).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE.md → "Failure handling"

### Z2-R-SAVE-081 Unresolved references become null

- **Statement:** An entity or asset reference that cannot be resolved at restore becomes null, is counted in `save.restore.unresolved_references` and is logged once per (type, property); entity references inside container elements resolve exactly like top-level ones.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_RuntimeEntityReferencesSurvive`, `Test_Restore_ContainerEntityReferencesResolved`, `Test_Snapshot_ExclusionsDropSubtrees` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Saved entity references"; Zenith2/Docs/Design/SAVE.md → "Counters"

### Z2-R-SAVE-082 Behaviour states saved through the script participant

- **Statement:** The `Zenith.Script.States` participant writes every entry of `GetStateSnapshot()` in (source ID, simulation ID) order with its flags byte (`m_bRunSuspended`, `m_bRecoveryPending`, `m_bQuarantinedOrDisabled`; a set reserved bit is `SAVE_ERROR_CORRUPT`) and its ZBST checkpoint, a zero-length blob for an instance with no committed checkpoint, and restores them with `RestoreSavedStates` after every entity exists, passing the flags, so each restored instance whose behaviour declares `OnRecover` and whose entry is flagged runs its recovery invocation once and every other restores with none (an empty blob restoring the declared defaults without `OnCreate`); it reports the capture COHERENT exactly when some entry has `m_bRunSuspended` or `m_bQuarantinedOrDisabled` (orchestrator ruling Q82 (b) as row SC25 delivers it; blocked by SCRIPT's post-signoff patch Z2T-SCRIPT-114).
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-023, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_RunBehaviourWritesCoherentOnly` (suspended `Run`, quarantined, disabling-module, `OnUpdate`-only and returned-`Run` cases, the suspended-`Run` and never-checkpointed round trips and the reserved-bit case) (`Test` step, `Zenith_Integration_SaveRuntime`); `Test_SaveRuntime_LoadTransitionsWorld` (Stage 4); SCRIPT's `Test_ScriptSaves_SaveReloadMigrate` (ticket Z2T-SCRIPT-098), `Test_Script_State_RestoreWithoutSuspendedRunIsExact` and `Test_Script_State_RestoreSuspendedRunRecovers` (ticket Z2T-SCRIPT-114).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Participant section framing"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-083 Reflected world resources saved

- **Statement:** Every world resource whose type has at least one serialized property (ENV's `Zenith.Environment.State`, PLAT's `Zenith_SimulationInputContexts`, GAME's `Zenith.Runtime.WorldState` among them) is saved in the world's RESOURCES section with its serialized properties and restored out of tick before the participants and the first tick, transient properties keeping their creation value; a saved resource the world lacks is carried and written again.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_WorldResourcesRoundTrip` (`Test` step); ENV's `Test_TimeOfDay_SaveRestore` (Z2-R-ENV-099).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Resources section"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Snapshot capture"

### Z2-R-SAVE-084 Additive scene set saved through a participant

- **Statement:** The `Zenith.Scene.AdditiveSet` participant saves every active instance with its scene and root simulation IDs and the next instance ID, excludes the entities of activating and unloading instances, records activating ones as LOADING with their in-flight load completion ID and re-requests them at restore under that recorded ID (declaring `ASYNC_SOURCE_SCENE_LOAD`), and reports the capture COHERENT whenever an instance was loading or queued.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_RunBehaviourWritesCoherentOnly` (the loading-instance case) (`Test` step); `Test_SaveCrossSystem_LoadingInstanceReRequestedWithHookTag`, `Test_SaveRuntime_AdditiveLoadReissuedUnderRecordedID` (Stage 4).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Participant section framing"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-085 Format migration chain one step per minor

- **Statement:** Saves of every minor version of the current major migrate forward through `Zenith_MigrateSaveImage`, applying one pure step per minor in order.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMigration_StepsRunInOrder`, `Test_SaveMigration_FailedStepLeavesFile` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveMigration header"

### Z2-R-SAVE-086 A chain gap fails at creation

- **Statement:** `Zenith_SaveMigrationRegistry::Freeze` fails `SAVE_ERROR_MIGRATION` naming the first minor without a step, which fails `Zenith_SaveSystem::Create`.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMigration_RegistryFreezeDetectsGap` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveMigration header"

### Z2-R-SAVE-087 Component data migrated at restore

- **Statement:** Every saved component and event record whose version is older than its type's migrates through `Zenith_MigrateFieldSet` at restore, including script ops through the save-loading VM factory, and a newer version fails `SAVE_ERROR_MIGRATION`.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMigration_ComponentVersionsMigrate`, `Test_SaveMigration_ScriptOpThroughVmFactory` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-088 Saves of another major refused recoverably

- **Statement:** A save of another major version is listed INCOMPATIBLE and refused with a recoverable result, never an assert.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMigration_OtherMajorRefusedRecoverably`, `Test_SaveSystem_ListSlotsStates` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Listing slots"

### Z2-R-SAVE-089 A migration fixture per save version

- **Statement:** For every save format version from 1.0 there is a committed text state file and expected file, the file is generated at test time with that version's writer, and the loaded, migrated and restored world equals the expected dump byte for byte.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMigration_FixturePerVersion` (`Test` step, suite `z2-save-cooked`, `FIXTURES_REQUIRED SaveMigrationFixtures` generated by `Test_SaveFixture_GenerateMigrationSet` into `${ZENITH_OUT}/test-fixtures/SAVE/`, [Z2-N-ARCH-257]).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "State description file"; Zenith2/Docs/Design/SAVE/Formats.md → "Expected output file"; Zenith2/Docs/Design/SAVE/Tests.md → "Migration and fixture tests"

### Z2-R-SAVE-090 Frozen fixture writers

- **Statement:** A format bump copies the previous version's encoder into `Zenith2/Engine/ZenithSave/Tests/Frozen/V<M>_<m>/`, a compiled test source (never `Tests/Support/`, which is header-only outside ZenithCore), which is never edited afterwards and whose output must migrate to the current version.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveFixture_FrozenWriterSelfConsistent` (`Test` step); review of frozen files under `reviewPaths`.
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Frozen fixture writers"

### Z2-R-SAVE-091 A version bump brings its fixture

- **Statement:** A diff that raises the save format version adds the new version's state and expected files, or the fixture registry test fails.
- **Covers:** Z2-I-SAVE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveFixture_VersionBumpNeedsFixture` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Frozen fixture writers"; Zenith2/Docs/Design/SAVE/Tests.md → "Migration and fixture tests"

### Z2-R-SAVE-093 Resume state written out of tick without a backup

- **Statement:** `SubmitResumeWrite` snapshots the paused primary world out of tick into arena 2 as a one-world image of kind RESUME at `user:/saves/resume.zsave`, pushed into the priority slot ahead of every queued job and written by the journaled write without a backup.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ResumeWriteAndRestore` (`Test` step); MOD's `Test_RuntimeResume_WrittenOnBackground`.
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Resume save and restore"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-094 A cancelled resume write never tears the file

- **Statement:** A resume write cancelled at PLAT's deadline leaves the previous resume file or no file, never a partial one.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveJournal_CancelBeforeEachStep` (`Test` step); PLAT's `Test_ResumeState_DeadlineAbandonsAndCleans` (`Zenith2/Docs/Design/PLAT/Android.md`, `Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Resume save and restore"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Journaled write sequence"

### Z2-R-SAVE-095 Resume identity reader

- **Statement:** `ReadResumeIdentity` reads only the 128-byte prefix and returns the build ID, composition hash, content hash and tick, or an error PLAT reports as unreadable.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ResumeWriteAndRestore` (`Test` step); MOD's resume tests (ticket Z2T-MOD-058).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Resume save and restore"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-096 Resume never migrated and bound to its profile

- **Statement:** A resume image loads only at the current format version and only under the profile that wrote it, compared against the save system's initial profile from the last-profile record, so an account resume restores before the platform reports the sign-in; otherwise the load fails (`SAVE_ERROR_IDENTITY_MISMATCH` for the profile) and the startup scene loads.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ResumeOtherProfileRefused`, `Test_SaveRuntime_ResumeAccountProfileBeforeSignIn` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Resume save and restore"; Zenith2/Docs/Design/SAVE/Formats.md → "Last profile record"

### Z2-R-SAVE-097 Profile directories

- **Statement:** Saves of each account live under `saves/<profile>/`, where the profile key is `local` or the first 8 bytes of the account ID's SHA-256 in hexadecimal.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveProfileKey_Derivation` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveTypes header"; Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"

### Z2-R-SAVE-099 Shutdown drains then cancels

- **Statement:** At shutdown step 4 the save system refuses new requests and cancels queued work except the exit autosave; on an ordinary exit it lets the running write finish for at most `save.shutdownDrainMs` and then cancels what still runs with rollback; on the device-loss exit the ordinary work was already cancelled when the exit autosave was submitted, and the exit autosave is waited for until 6 s after its submission and then cancelled with a rollback wait of at most 1 s, so SAVE uses at most 7 s of the 10 s clean exit.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ShutdownDrainsThenCancels`, `Test_SaveSystem_ExitAutosaveSurvivesShutdown` (`Test` step); `Test_SaveRuntime_DeviceLossAutosave` (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/SAVE.md → "Startup and shutdown"

### Z2-R-SAVE-100 SAVE threads joined before asynchronous I/O is destroyed

- **Statement:** `Zenith.SaveWriter` and `Zenith.ReplayWrite` are joined by their owners before `Zenith_AsyncIO` is destroyed, and neither thread ever runs a job or touches world state.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** MOD's shutdown-order test with SAVE present; `Test_SaveDeath_DestroyWithoutShutdown` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Threads SAVE owns"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Shutdown sequence"

### Z2-R-SAVE-101 Memory consumer reports and sheds nothing

- **Statement:** The save system registers one `MEMORY_SUBSYSTEM_SIMULATION` consumer reporting its arenas, retained checkpoints and thumbnails, and changes no behaviour at any pressure level.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SAVE-001
- **Acceptance evidence:** `Test_SaveSystem_MemoryConsumerReportsArenas` (`Test` step); CORE's `Test_MemoryGovernor_ForcedCriticalShedsEachConsumer` lists SAVE as shedding nothing.
- **Design:** Zenith2/Docs/Design/SAVE.md → "In-memory structures"; Zenith2/Docs/Design/SAVE.md → "Rows SAVE owns"

### Z2-R-SAVE-102 The Save binding module

- **Statement:** The `Save` module offers `Request`, `Quick`, `Checkpoint`, `SetAutosaveEnabled`, `SetMetadata`, `ListSlots` and `Delete` from Stage 3 and `Load` from Stage 4, each request appending one `SIDE_EFFECT_KIND_SAVE_REQUEST` record in SCRIPT's save-request capacity class with the documented payload, through the Luau-free binding ABI; `SetMetadata` and `SetAutosaveEnabled` append theirs with a zero capacity demand, so they never count against Z2-B-SCRIPT-025, and the bridge's consumer allocates the `ASYNC_SOURCE_SAVE` completion ID of each request record (operations 1–4) on the committing merge node, since SCRIPT's commit hands it 0.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ScriptRequestWritesAndPublishes` (including the consumer-allocated completion ID), `Test_SaveRuntime_ListSlotsEvents`, `Test_SaveRuntime_MetadataDoesNotConsumeRequestCapacity` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveRegistration header"

### Z2-R-SAVE-103 Save misuse raises Luau errors

- **Statement:** Each misuse of the misuse table of "Script-facing API" raises its ZS51xx Luau error (kind MISUSE) in every build and never asserts; ZS5104 is retired and never reassigned.
- **Covers:** Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_MisuseRaisesLuauErrors` (`Test` step, Debug and Release).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/SAVE.md → "Luau errors versus asserts"

### Z2-R-SAVE-104 Save result events

- **Statement:** Published SAVE results are appended to the bridge's per-world result list by a publication handler that writes no world state, and become `Zenith.SaveCompleted`, `Zenith.SaveSlotListed` and `Zenith.SaveSlotListDone` events emitted by `Save.EmitResults` in phase 1 of the tick whose boundary published them, in completion-ID order, and delivered in phase 9 of that tick.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ScriptRequestWritesAndPublishes`, `Test_SaveRuntime_ListSlotsEvents`, `Test_SaveRuntime_ResultsEmittedInCompletionOrder` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveRegistration header"; Zenith2/Docs/Design/SAVE.md → "Frame-graph nodes SAVE runs"

### Z2-R-SAVE-105 A rolled-back invocation requests nothing

- **Statement:** A behaviour invocation that requests a save and then fails requests nothing: no file, no event.
- **Covers:** Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_RolledBackInvocationRequestsNothing` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"

### Z2-R-SAVE-106 SAVE counters

- **Statement:** SAVE publishes every counter of the table "Counters" with its counter kind, with a separate bounded-queue triple per queue (writer queue, operation queue, replay commit queue).
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** The counter assertions inside the save-system, journal and replay tests (`Test` step); the on-screen profiler capture of the Stage 3 S24 device run (recorded by the `needs-human` S24 device ticket).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Counters"

### Z2-R-SAVE-107 SAVE log categories and messages

- **Statement:** SAVE logs under `save`, `save.journal` and `save.replay` with the levels and texts of "Log categories and messages".
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The log-capture assertions of `Test_SaveSystem_LoadFallsBackToBackup` and `Test_SaveJournal_ConflictKeepsFiles` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Log categories and messages"

### Z2-R-SAVE-108 SAVE zones and profiler rows

- **Statement:** SAVE emits the zones of "Profiling zones" and the on-screen profiler shows the SAVE panel rows in development builds.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_WriteAtPhase11` (the `Save.Snapshot` zone recorded by the test profiler sink) (`Test` step); DEVT's panel test.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Profiling zones"; Zenith2/Docs/Design/SAVE.md → "On-screen profiler rows"

### Z2-R-SAVE-109 Save dump command

- **Statement:** `save.dump` prints the world bindings, slot table, latest recovery report and operation table, and `save.dump <slot>` a slot's header, table and metadata from a listing-sized prefix read, decoding no compressed section and loading nothing.
- **Covers:** Z2-I-SAVE-06, Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ListSlotsStates` also runs the dump and checks its lines (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Debug commands and dumps"

### Z2-R-SAVE-110 Reproducibility boundaries

- **Statement:** Replays and exact restores reproduce exactly the left column of "Reproducibility boundaries" and nothing in its right column affects a tick hash.
- **Covers:** Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_RecordReplayRoundTrip`, `Test_Restore_ContinuationMatchesUninterrupted`, `Test_SaveRuntime_ThermalCheckpoint` (`Test` step); CORE's `Test_MemoryGovernor_ShedsNoSimulationState`. MOD's Stage 5 `Test_RuntimeThermal_TickHashesUnchanged` (ticket Z2T-MOD-059) re-proves at Stage 5 the thermal boundary on device and is not this Stage 3 requirement's evidence.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay verification"

### Z2-R-SAVE-111 Replays of another build refused

- **Statement:** A replay whose build ID differs from the running build is refused with exit code 5 naming the build ID.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Replay_DifferentBuildRefused` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"; Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"

### Z2-R-SAVE-112 Saves record the replay position

- **Statement:** A save written during a recorded run stores the world session ordinal and tick in its header.
- **Covers:** Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_IdentityRecordedInHeader` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "File header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"

### Z2-R-SAVE-136 User mount round trip without stray files

- **Statement:** A save written through the real asynchronous I/O lands under `user:/saves/local/`, loads, and leaves no `.z2atomic`, `.z2tmp` or journal behind.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SavePlatform_UserMountRoundTrip` (`Test` step, headless host).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Platform storage tests"; Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"

### Z2-R-SAVE-137 Kill during a write on the S24

- **Statement:** On the S24, killing the process and forcing a power-off during a 64 MiB save, 20 times each, always relaunches into the old or the new save.
- **Covers:** Z2-I-SAVE-05, Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveDevice_S24_KillDuringWrite` device run record under `Zenith2/Docs/Research/Save/` (`needs-human` device ticket; Stage 3 acceptance).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Platform storage tests"

### Z2-R-SAVE-140 Save journal replay and metadata fuzz targets

- **Statement:** The four libFuzzer targets exercise the save, journal, replay and metadata parsers with no crash, assert or out-of-bounds access in their smoke runs.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_SaveImage`, `Zenith_Fuzz_SaveJournal`, `Zenith_Fuzz_ReplayLog`, `Zenith_Fuzz_SaveMetadata` 60-second smokes (`Sanitizers -Preset win-asan`).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Fuzz targets"

### Z2-R-SAVE-142 SAVE death tests

- **Statement:** The four SAVE programmer errors of "Death tests" stop the process through the asserting path.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveDeath_StorageCallOnJobWorker`, `Test_SaveDeath_DestroyWithoutShutdown`, `Test_SaveDeath_RestoreStepInFrame`, `Test_SaveDeath_ReplayCloseWithPendingHash` (`Test` step, `z2-death-assert`).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Death tests"; Zenith2/Docs/Design/SAVE.md → "Luau errors versus asserts"

### Z2-R-SAVE-143 Save benchmark and smoke

- **Statement:** `Zenith_Bench_Save` measures capture, assembly, compression and restore cost and peak arena bytes at 10,000, 100,000 and 1,000,000 entities, and `Bench_Save_Smoke` runs in the `Test` step.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SAVE-001, Z2-B-SAVE-002
- **Acceptance evidence:** `Bench_Save_Smoke` (`Test` step); reports under `Zenith2/Docs/Research/Save/` (the human-run measurement ticket, `needs-human`).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Benchmarks and budget harness"

### Z2-R-SAVE-144 SAVE budget rows allocated in Stage 1

- **Statement:** The Stage 1 per-system budget ticket for SAVE gives Z2-B-SAVE-001 and Z2-B-SAVE-002 adopted values, appends the ruling to DecisionLog.md and allocates the pending rows SAVE requests.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SAVE-001, Z2-B-SAVE-002
- **Acceptance evidence:** The `needs-human` ruling ticket's Budgets.md rows and DecisionLog.md entry, checked by the coverage gate step in acceptance mode `-Stage 1`.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Rows SAVE owns"; Zenith2/Docs/Design/SAVE.md → "Stage 1"

### Z2-R-SAVE-145 New SAVE budget rows registered

- **Statement:** The pending SAVE rows "Save write duration per save", "Save file size per slot", "Save load duration per save" and "World restore duration per 10,000 entities" are registered with SAVE as owner and every Budgets column (measurement procedure, platforms, tolerance, workload, breach rule) as "Pending row needs" lists them, pending until the Stage 1 ruling; no replay-log row exists.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** The Budgets.md rows and "Row index" entries added by the core-doc batch from cross-doc item X202; the coverage gate step.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Pending row needs"

### Z2-R-SAVE-147 Replay playback supplies recorded publications

- **Statement:** From Stage 3, under `--replay`, tick-boundary step 1 publishes the recorded payloads of recorded kinds at their recorded ticks through `Zenith_RuntimeReplayPublicationSource`, and the tick hash adds SCRIPT's checkpoint contribution on both sides.
- **Covers:** Z2-I-SAVE-10, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_RecordReplayRoundTrip`, `Test_SaveRuntime_SaveDuringReplayUsesRecordedResult` (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-148 Per-capture fidelity sets COHERENT_ONLY

- **Statement:** A capture's fidelity is the weakest `GetCaptureFidelity` any participant returns for it, and the image header carries `COHERENT_ONLY` exactly when it is COHERENT; the script participant is COHERENT for any capture holding an entry with `m_bRunSuspended` (a suspended `Run` coroutine) or `m_bQuarantinedOrDisabled` (a quarantined instance, or one of a disabled or disabling module) and EXACT otherwise, a pending recovery included, and the additive-set participant is COHERENT for any capture with a loading or queued instance.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_FidelityPerCapture` (`Test` step, `ZenithSave_Tests`); `Test_SaveRuntime_RunBehaviourWritesCoherentOnly` (`Test` step, `Zenith_Integration_SaveRuntime`); SCRIPT's `Test_Script_State_RestoreWithoutSuspendedRunIsExact` and `Test_Script_State_RestoreSuspendedRunRecovers` (ticket Z2T-SCRIPT-114).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Snapshot capture"

### Z2-R-SAVE-149 Autosaves replay identically whatever the writer does

- **Statement:** A recorded run with engine autosaves and a storage that delays every write replays with identical tick hashes, publications and next completion ID, because no completion ID depends on writer progress, arena availability or recording.
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_AutosaveReplaysIdentically` (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Autosave scheduling"; Zenith2/Docs/Design/SAVE.md → "Determinism obligations"

### Z2-R-SAVE-151 Asynchronous source capacity bounded

- **Statement:** The bridge declares `ASYNC_SOURCE_SAVE` with an in-flight capacity of `4 × save.maxQueuedRequests + 2 × save.maxRequestsPerTick + 3`, derived as the IDs not completed at the last boundary (pending, two captured jobs, the operation queue and its running operation) plus those admitted before the next (script requests, C++ requests with their own ID, the limit batch, the autosave); intake refuses before allocating on the deterministic outstanding count, and every admitted request publishes exactly once.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_OutstandingCapacityBound` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Asynchronous source capacity"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"

### Z2-R-SAVE-152 Replay mode touches no user storage

- **Statement:** In replay mode no write, delete, listing or load touches storage, the writer completes nothing, journal recovery, the last-profile read and the slot-table build are skipped, the runtime makes no resume decision, and C++ loads, listings and deletes and the resume, resume-identity, exit-autosave and profile calls are refused with `SAVE_ERROR_REFUSED_IN_REPLAY`, so a replay reads, writes or deletes no user file from startup to shutdown.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ReplayModeSuppressesWrites`, `Test_SaveRuntime_ReplayTouchesNoStorage` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay mode"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-153 Stage 2 replay core without SCRIPT

- **Statement:** The Stage 2 bridge slice `Zenith_RuntimeReplayCore.*` records and replays `WORLD_BEGIN`, `WORLD_SWAP` and `TICK` records with input commands and `HashTickState` hashes, implementing only ECS's tick-boundary handler and including no `ZenithScript` header, so MOD's Stage 2 headless-equivalence test runs through it.
- **Covers:** Z2-I-SAVE-10, Z2-I-SAVE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimeReplayCore_RecordsInputsAndHashes` (`Test` step, `ZenithRuntime_Tests`); `Libgraph` (no `ZenithScript` include in the core files); MOD's `Test_Runtime_HeadlessMatchesWindowedSimulation`.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Runtime bridge files"; Zenith2/Docs/Design/SAVE.md → "Stage 2"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-154 Bridge failures reach the composition root

- **Statement:** Every fallible bridge function returns a `Zenith_Result`: a save-system creation failure is exit code 4, a binding registration failure exit code 3, an attach failure of the startup world exit code 3 and of a transition's world the world host's attach-failure rule, each unwinding what it built.
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_BridgeFailuresMapToExitCodes` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Startup and shutdown"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-155 Save system creation validates and unwinds

- **Statement:** `Zenith_SaveSystem::Create` refuses every null required pointer and out-of-range value with `SAVE_ERROR_INVALID_DESC`, reports a thread-creation failure as `SAVE_ERROR_THREAD_CREATE`, and a failure at any construction step unwinds exactly what was built with no leak.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_CreateValidatesDesc`, `Test_SaveSystem_CreateUnwindsEachStep` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"; Zenith2/Docs/Design/SAVE.md → "Startup and shutdown"

### Z2-R-SAVE-156 Cross-thread hand-offs of arenas and checkpoints

- **Statement:** Capture arenas are acquired and released only through the atomic free-arena mask with acquire and release orders, so concurrent phase-11 nodes of different worlds never share an arena; the retained checkpoint crosses from the writer to the main thread only through the pending-checkpoint slot of the world's checkpoint holder and the lease; the thumbnail reaches the writer only as a copy in the capture arena.
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-SAVE-001
- **Acceptance evidence:** `Test_SaveSystem_ConcurrentWorldCapturesShareArenas` (`Sanitizers -Preset linux-tsan` and `Test` step), `Test_SaveSystem_RetainedCheckpointLeaseDefersReplacement` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE.md → "Cross-thread hand-offs"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Checkpoints"

### Z2-R-SAVE-159 Replay writer stops at its limits without blocking

- **Statement:** The replay writer's commit queue rejects when full, the log never exceeds `replay.maxLogBytes`, and a checkpoint image above the checkpoint bound is skipped; a full queue or the size limit stops recording with the committed prefix readable and never blocks the main thread.
- **Covers:** Z2-I-SAVE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_CommitQueueFullStopsRecording`, `Test_ReplayLog_MaxLogBytesStopsRecording`, `Test_ReplayLog_CheckpointAboveBoundSkipped` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE/Formats.md → "Replay limits"

### Z2-R-SAVE-160 Last active profile persisted

- **Statement:** Every successful profile switch writes the 28-byte last-profile record atomically, and startup creates the save system in that profile, falling back to `local` for a missing or invalid record.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveLastProfile_RoundTripAndFallback`, `Test_SaveSystem_ProfileSwitchIsolation` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Last profile record"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Profile switching"

### Z2-R-SAVE-161 Format limits tested at and above the limit

- **Statement:** Every SAVE parser limit of "Format limits" is accepted at the limit and refused at the limit plus one, as a `Zenith_SaveError`, never an assert.
- **Covers:** Z2-I-SAVE-06, Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveFormat_ParserLimitsAtAndAboveLimit`, `Test_SaveHeader_RejectsEachViolation`, `Test_SaveSectionTable_RejectsEachViolation` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Format limits"; Zenith2/Docs/Design/SAVE/Tests.md → "Format and codec tests"

### Z2-R-SAVE-162 LIST publication record layout

- **Statement:** Each slot of a LIST publication is the exact 120-byte record of "Publication payloads", so the largest payload is 7,748 bytes, and a zero-length SAVE payload decodes as `SAVE_ERROR_CANCELLED`.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SavePublication_ListRecordLayout` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Publication payloads"

### Z2-R-SAVE-164 Restored worlds wait for participant readiness

- **Statement:** A restore job reaches DONE only after every registered participant's `IsRestoreReady` returned true in registration order in state `WORLD_RESTORE_STATE_PARTICIPANTS_READY` (a participant that returned true is not asked again), and no tick of the restored world runs before DONE.
- **Covers:** Z2-I-SAVE-18, Z2-I-SAVE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Restore_ParticipantReadinessHoldsDone` (`Test` step, `ZenithSave_Tests`); ANIM's `Test_AnimSave_RestoreBindsBeforeFirstTick`.
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_WorldSnapshot header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore job state machine"

### Z2-R-SAVE-165 Synchronous restores service participant waits

- **Statement:** A restore outside a frame (the resume at startup, a seek, a resumed replay, a replay's restored session) runs through `Zenith_RuntimeSave_RestoreSynchronously`, which services every operation the unready participants list in `ListRestoreWaits` with `Zenith_AssetLoader::ServiceUntilComplete` between Steps and fails with `SAVE_ERROR_RESTORE` naming the first unready participant after `runtime.startupLoadTimeoutMs`.
- **Covers:** Z2-I-SAVE-18, Z2-I-SAVE-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_SynchronousRestoreServicesWaits` (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Synchronous restore driver"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Resume save and restore"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay seeking"

### Z2-R-SAVE-166 Participants declare the source kinds they re-issue

- **Statement:** A participant declares through `GetReissuedSourceKinds` the asynchronous source kinds whose in-flight requests it re-issues under their recorded completion IDs; registration refuses a kind another participant of the world declared, `ASYNC_SOURCE_SAVE` or a kind outside the registry capacity; the restore leaves every declared in-flight entry in flight and abandons each one the participant did not mark re-issued.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ReissueDeclarationsExclusive`, `Test_Snapshot_AsyncStateRestored` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; Zenith2/Docs/Design/SAVE/Formats.md → "Async section"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-170 Over-limit C++ requests share one limit batch

- **Statement:** The C++ requests of one tick over `save.maxRequestsPerTick` are refused before any completion ID of their own is allocated and are published together under the tick's one limit-batch completion ID, completed at phase-11 step 2 with the exact refused count and the first 4,000 serials, so a tick admits at most `save.maxRequestsPerTick` + 1 C++ completion IDs whatever the number of commands.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_PerTickLimit`, `Test_SaveSystem_OutstandingCapacityBound` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"; Zenith2/Docs/Design/SAVE/Formats.md → "Publication payloads"; Zenith2/Docs/Design/SAVE.md → "Asynchronous source capacity"

### Z2-R-SAVE-171 Exit autosave preempts ordinary writes

- **Statement:** `SubmitExitAutosave` cancels every queued SAVE job and the running ordinary write or operation before it captures, so the exit autosave never waits behind an ordinary write, and an ordinary write it preempts rolls back unless it reached journal step 4.
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ExitAutosaveSurvivesShutdown` (the preemption case), `Test_SaveRuntime_DeviceLossAutosave` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Shutdown sequence"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"; Zenith2/Docs/Design/SAVE.md → "Cancellation"

### Z2-R-SAVE-172 Checkpoint leases block profile switches and outlive unbinding

- **Statement:** `SetActiveProfile` is refused with `SAVE_ERROR_PROFILE_BUSY` while any retained-checkpoint lease is alive, and `UnbindWorld` of a world with a live lease frees the binding but leaves its checkpoint holder alive until the last lease ends, so no restore job borrowing a leased checkpoint ever reads freed memory.
- **Covers:** Z2-I-SAVE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_RetainedCheckpointLeaseDefersReplacement` (profile and unbind cases, `win-asan`) (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Checkpoints"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Profile switching"; Zenith2/Docs/Design/SAVE.md → "Cross-thread hand-offs"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-173 Callbacks enter per-context reentrancy guards

- **Statement:** Every save-system callback enters exactly one guard of its invoking context — the world binding's guard on that world's nodes and its main-thread out-of-tick captures, the writer guard on `Zenith.SaveWriter`, the system guard for main-thread calls tied to no world — so no `Zenith_ReentrancyGuard` is entered by two threads at once and concurrent world captures are TSan-clean.
- **Covers:** Z2-I-SAVE-09, Z2-I-SAVE-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ConcurrentWorldCapturesShareArenas` (`Sanitizers -Preset linux-tsan` and `Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"; Zenith2/Docs/Design/SAVE.md → "Thread affinity per type"; Zenith2/Docs/Design/SAVE.md → "Cross-thread hand-offs"

### Z2-R-SAVE-174 Metadata pairs copied at intake with a reserved engine prefix

- **Statement:** A request's metadata is the world's per-tick list as it stands at that request's intake (with a C++ request's own pairs replacing equal keys), copied with the request; the game sets at most 29 pairs, a key beginning `zenith.` is refused (ZS5106 for scripts, `SAVE_ERROR_INVALID_DESC` for C++), a 30th distinct key in one tick is ignored and counted, and the writer adds the three engine pairs for at most 32 per file; a script's `Save.SetMetadata` consumes no save-request capacity (zero capacity demand), so the 29-key list and the 30th-key rule are reachable from scripts.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_MetadataCopiedAtIntake`, `Test_SaveMetadata_LimitsAndOrder`, `Test_SaveRuntime_MetadataDoesNotConsumeRequestCapacity` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE/Formats.md → "Metadata section"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveTypes header"

### Z2-R-SAVE-175 Thumbnails copied into the capture arena

- **Statement:** A capture copies the world's thumbnail into its capture arena, so the writer never reads the binding's copy that `SetThumbnail` may overwrite between frames.
- **Covers:** Z2-I-SAVE-07, Z2-I-SAVE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ThumbnailCopiedAtCapture` (`Sanitizers -Preset linux-tsan` and `Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"; Zenith2/Docs/Design/SAVE.md → "Cross-thread hand-offs"; Zenith2/Docs/Design/SAVE.md → "In-memory structures"

### Z2-R-SAVE-176 Own result payload rebuilt from the header

- **Statement:** Every request-written image carries its own completion ID and own request serial in the header, from which an ordinary restore rebuilds the request's success payload byte-equal to the payload the writer completed the request with.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_AsyncStateRestored` (the own-entry case) (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "File header"; Zenith2/Docs/Design/SAVE/Formats.md → "Publication payloads"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Image assembly and compression"

## Coverage of inventory items

Requirements of either catalogue file; a requirement of Stage 4 or 6 is in `Zenith2/Docs/Requirements/SAVE/Stage4Plus.md`.

| Item | Text | Inventory stage | Requirements |
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

Every SAVE inventory item is classified required (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1"), and every requirement here is required.

## Excluded and deferred

No SAVE inventory item is excluded or deferred. Related scope that this catalogue does not cover, each with its owner and decision:

| Scope | Disposition | Decision |
|---|---|---|
| saves of third-party mods | excluded with modding | Z2-D-075 |
| per-player saves for local multiplayer and split-screen | excluded with local multiplayer | Z2-D-076 |
| console save storage and certification rules | deferred with consoles to Stage 7, specified by the SCOPE console tickets | Z2-D-071 |
| networked replication snapshots and dedicated-server persistence | deferred to Stage 7 with NET; SAVE's `ZRPL` command block is the only shared format | Z2-D-073 |
| cloud saves and their conflict resolution | ONLINE (adapter S4, backends S6), using SAVE's slot metadata and sequence | Z2-D-074 |
| editor autosave and crash recovery of authoring documents | EDIT over SCENE's document framework | Z2-D-048 |
| quest, dialogue and inventory save formats | game-side Luau state, saved generically as components and behaviour state | Z2-D-077 |
