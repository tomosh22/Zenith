# PLAT requirement catalogue: packaging and tooling primitives

This file continues `Zenith2/Docs/Requirements/PLAT.md`, in one numbering. It holds the requirements for the platform primitives that PACK's update and delivery flows call, the test hooks and Android services that PACK, MOD and AUDIO need, the directory-grant reads that DEVT and MOD need, and input-script recording for GAME's and TWOD's playthrough recordings. The format, classification rules and coverage conventions are those of the entry file's "Purpose"; the inventory coverage of these requirements is listed in the entry file's "Coverage of inventory items".

## Requirements

### Z2-R-PLAT-168 Public update-manifest verifier and its three halves

- **Statement:** `Zenith_UpdateManifest.h` declares `Zenith_DecodeUpdateManifest` (U1–U3, U7), `Zenith_CheckUpdateManifestTarget` (U4, U5, U8), `Zenith_VerifyUpdateManifestFiles` (U9) and `Zenith_VerifyUpdateManifest`, which runs the three in order. The verifier accepts both manifest kinds, checks the directory name against the manifest, reports each check failure as a distinct `Zenith_UpdateVerifyError`, allocates nothing, never asserts on untrusted bytes, and reads no field before the signature verifies.
- **Covers:** Z2-I-PLAT-13, Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateManifest_DecodeRules`, `Test_UpdateManifest_TargetChecks`, `Test_UpdateManifest_FileChecks` and `Test_UpdateManifest_HalvesComposeToWhole`, run in the `Test` step (win-debug, win-release, linux-tsan); `Zenith_Fuzz_UpdateManifest` runs in the `Sanitizers -Preset win-asan` step.
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Update manifest API"

### Z2-R-PLAT-169 File identity stamps and the patch mount's stamp skip

- **Statement:** The directory source takes a 48-byte `Zenith_FileIdentityStamp` from an open handle: the size, the last-write and change times in nanoseconds since the Unix epoch, the volume, and the 128-bit file ID. It takes stamps only on the supported volume kinds and answers false everywhere else. With a stamp lookup, `Zenith_VerifyUpdateManifestFiles` skips the hash of a listed file only when the file's settled stamp equals its record in all five fields under the same manifest digest. It reports a file whose stamp changes between its first and last read as `UPDATE_VERIFY_ERROR_FILE_CHANGED`. It never compares or records a stamp within 100 ms of the current time. The patch mount reads `m_pxUpdateStamps` only during `Create`. The signature and target checks run at every start.
- **Covers:** Z2-I-PLAT-13, Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_DirectorySource_IdentityStampMapping` and `Test_PatchMount_StampSkipAndRehash`, run in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "File identity stamps"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification"

### Z2-R-PLAT-170 Directory-source factory and reads from an unmounted source

- **Statement:** `Zenith_CreateDirectoryMountSource` creates the read-only directory source over a host directory outside the four mount roots. The source applies the escape check and the case policy to every open. `Zenith_AsyncIO::ReadFromSource` reads from a source that is not mounted. It uses the priorities, chunking, preemption, cancellation and completion records of `Read`, and it resolves only through the given source. `AddSource` and `RemoveSource` are admitted in the frame prologue, in the closing action of startup step 11 and in shutdown step 4. `Zenith_BuildIdentity` carries both content hashes. `m_ulPackageContentHash` is never changed by a patch. `m_ulContentHash` becomes the mounted patch's result content hash.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-15, Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_DirectorySource_PublicFactoryRules`, `Test_AsyncIO_ReadFromSourceUnmounted` and `Test_PatchMount_DirectoryNameAndBaseContent`, run in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount sources"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Reads from an unmounted source"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API"

### Z2-R-PLAT-171 Base-path override for test install trees

- **Statement:** When `Zenith_PlatformDesc::m_strBasePathOverride` is non-empty, `Zenith_Platform::GetBasePath()` returns it, with one trailing `/`, instead of `SDL_GetBasePath`. The override is validated at creation step 1. A Player and a tool pass an empty view.
- **Covers:** Z2-I-PLAT-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_BasePathOverride`, run in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc"

### Z2-R-PLAT-172 Android Java context for libraries above the platform

- **Statement:** On Android, `Zenith_Platform::GetAndroidJavaContext()` returns the `JavaVM*` and global references to SDL's activity and to that activity's class loader. The handles are created at creation step 6 and deleted by the platform's destructor. The accessor is ANY_THREAD. Off Android, or in a process without an activity, the accessor returns `PLATFORM_ERROR_NOT_AVAILABLE`. No library above ZenithPlatform calls SDL to reach JNI.
- **Covers:** Z2-I-PLAT-23, Z2-I-PACK-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_AndroidJavaContext`, run in the `Test` step and in the S24 HWASan device run of every test executable; the Libgraph step's third-party owner rule (no SDL3 include outside `ZenithPlatform/Internal/`).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Android Java context"

### Z2-R-PLAT-173 Presentation-only connectivity signal

- **Statement:** `Zenith_Platform::GetConnectivityState()` and `GetConnectivityRevision()` publish, at each pump, the default network's validated-internet state. The state comes from `NotifyNetworkConnectivityHintChange` on Windows and from the Gradle template's connectivity callback class on Android; it is UNKNOWN on Linux and in headless mode. The revision counts OFFLINE → ONLINE transitions only. A source that cannot register leaves the state UNKNOWN and is never a failure. Simulation never reads the signal.
- **Covers:** Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_ConnectivityRevision`, run in the `Test` step (win-debug, win-release, linux-tsan); the Android source is exercised by PACK's S24 interrupted-download device runs.
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity"

### Z2-R-PLAT-174 Directory-grant reads

- **Statement:** `Zenith_OpenFileInCommandLineHostDirectory` opens one existing regular file of at most 1 MiB inside a directory grant. An evidence grant admits exactly `rows.json` and `fingerprint.json`. A read-only grant (`--script-migration-fixtures`) admits a one- or two-component relative path. Each component passes the path rules, and the path is checked by the escape check and the case policy. `Zenith_ListCommandLineHostDirectory` lists a read-only grant's regular files to depth 2, sorted by byte order. It follows no link and returns at most 1,024 entries. No function writes into a read-only grant. Both reads are refused in Shipping.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CommandLineHostDirectory_EvidenceReads` and `Test_CommandLineHostDirectory_ReadOnlyListAndOpen`, run in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Directory-grant reads"

### Z2-R-PLAT-175 Input-script ticks across world swaps and exact command bits

- **Statement:** An input script's `tick` is the run's input tick: input-world ticks counted from 0 and summed across world-transition swaps, which `Zenith_InputSystem::GetInputTickCount()` reports. Version 2 of `zenith2.input-script` adds the optional `bits` key, which replays a command's 8 value bytes exactly. Version 1 files load unchanged.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_InputScript_Version2Bits`, run in the `Test` step (win-debug, win-release, linux-tsan); `Test_InputScript_RecordReplayRoundTrip` in the `Zenith2/Tests/Input` integration suite (win-debug).
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format"

### Z2-R-PLAT-176 Input-script recording

- **Statement:** In non-Shipping builds, the console commands `input.recordScript <name>` and `input.stopRecordScript` record the input world's phase-1 action commands, one event per command, as a version 2 input script. The file is written through the development file writer. The recording holds at most 262,144 events, and a written document is at most 16 MiB; every dropped command is counted. Recording never changes a command. Replaying the file from startup with `exclusive: true` reproduces the recorded commands tick for tick.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_InputScript_RecorderLimitsAndStates`, run in the `Test` step (win-debug, win-release, linux-tsan); `Test_InputScript_RecordReplayRoundTrip` in the `Zenith2/Tests/Input` integration suite (win-debug).
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input-script recording"
