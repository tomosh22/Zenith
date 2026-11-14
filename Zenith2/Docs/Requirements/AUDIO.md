# AUDIO requirement catalogue

This is the requirement catalogue of the AUDIO area (the Audio module `ZenithAudio`: the output device, the real-time mixer, sounds and banks, decoding and streaming, voices and their budgets, buses and mixing, spatialisation, occlusion, zones and reverb, music, dialogue and localized audio, the audio clock, Android audio focus, profiling, the audio cook, the audio editor surfaces and the complete audio workflow). Each requirement is one testable obligation with its inventory coverage, stage, classification, governing decisions, budget rows, acceptance evidence and the design headings of `Zenith2/Docs/Design/AUDIO.md` and its split files that specify it.

## Purpose

The catalogue decomposes every AUDIO item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" into requirements `Z2-R-AUDIO-NNN`, the unit that leaf and integration tickets cover (`Zenith2/Docs/Roadmap.md` → "Board conventions"). It also holds AUDIO's half of seven items owned by other areas, cited by bare ID in the Covers line, with their owners in "Items of other areas with an AUDIO half" below: Z2-I-CORE-21 (the audio memory budget), Z2-I-STREAM-11 (coordinated audio streaming), Z2-I-CINE-04 (audio tracks), Z2-I-CINE-09 (subtitle sync), Z2-I-UI-11 and Z2-I-UI-23 (captions, subtitles) and Z2-I-NET-05 (headless execution). Stages name the stage whose acceptance each requirement serves; a requirement whose governing decisions are settled may be scheduled early by the pull-forward rule (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule"). Requirements are `required` except the D-SEL-CODEC harness and ruling and the cross-platform hash comparison, which are `research`. Unit tests run in the `Test` gate step on the win-debug and win-release presets (and `Sanitizers -Preset win-asan` where stated); ZenithAudio is never built for `linux-tsan`. Device runs are `needs-human` records on the stage acceptance ticket (`Zenith2/Docs/Design/AUDIO.md` → "Device runs").

## Requirements

### Z2-R-AUDIO-001 ZenithAudio library target

- **Statement:** ZenithAudio is one module library declared with `zenith_add_library(ZenithAudio KIND MODULE MODULE_NAME Audio DEPENDS ZenithRuntime ZenithPlatform ZenithAssets ZenithScene ZenithECS ZenithReflect ZenithJobs ZenithCore ZenithProfiling THIRD_PARTY miniaudio SUPPORTED ZENITH_GPU)`, with its public headers at the library root, everything else under `Internal/`, and no dependency on any other module.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** The `Configure`, `Build -Preset win-debug`, `Build -Preset win-release`, `Build -Preset android-arm64`, `SourceLayout` and `Libgraph` gate steps pass with the library present.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "ZenithAudio target"; `Zenith2/Docs/Design/AUDIO.md` → "Public root headers"; `Zenith2/Docs/Design/AUDIO.md` → "Libraries AUDIO depends on"

### Z2-R-AUDIO-002 miniaudio confined configured and heap-routed

- **Statement:** miniaudio is compiled in the single unit `Internal/Device/miniaudio_impl.c` with decoding, encoding, generation, the resource manager, the node graph and the engine disabled, only the WASAPI, AAudio and null backends enabled and runtime linking disabled; its headers appear only under `Internal/`, its allocation callbacks route to the thread-safe root heap, its notification and log callbacks only push to the multi-producer backend notification queue, and AUDIO calls it only from the control thread while holding nothing; its role is the output device layer plus its format conversion when the endpoint is not 48 kHz, as `Zenith2/Docs/Architecture.md` → "Third-party dependencies" and the 2026-10-09 amendment of Z2-D-082 state (accepted flagged proposal AUDIO-1).
- **Covers:** Z2-I-AUDIO-12, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-046, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** The `Libgraph` (third-party confinement) and `ForbiddenCheck` (no dynamic loading) steps pass; `Test_AudioDevice_MiniaudioNullBackendRunsCallback` and `Test_AudioSystem_ShutdownJoinsThreads` (every allocation freed through the test heap) pass in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "ZENITH_SUPPORTED matrix and miniaudio configuration"; `Zenith2/Docs/Design/AUDIO/Device.md` → "miniaudio backend"

### Z2-R-AUDIO-003 Module entry points and start registrations

- **Statement:** `Zenith_Module_Audio_RegisterTypes` registers the components, payload structs, script value types, enums and configuration keys at startup step 6, and `Zenith_Module_Audio_Start` performs the eleven start steps in order at step 15: create the audio system, register both kind loaders before `CloseRegistration`, the twelve command kinds, the `Zenith.Audio` bindings (with the `ASYNC_SOURCE_AUDIO` result decoder from Stage 4), the audio-trigger consumer, the notification source and its submitter on the runtime's request multiplexer (Stage 4), the memory consumer, the frame listener and the engine-wide `Audio.Mix` stage, then load the master bank and start the device.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioComponents_RegisteredWithAuthority`, `Test_AudioScript_ConsumerRegisteredBeforeSeal` and `Test_AudioIntegration_HeadlessGame300Ticks` pass in the `Test` step (win-debug, win-release); `Test_FrameGraph_AudioMixAfterEveryWorldUpdate` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Module entry points"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Binding registration"

### Z2-R-AUDIO-004 Audio system creation unwinds on failure

- **Statement:** `Zenith_AudioSystem::Create` performs its six creation steps in order, each with a fault point, and a failure at any step destroys exactly what the earlier steps created and returns that step's error, leaving no allocation outstanding.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_Create_FailsAtEachStep_Unwinds` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Creation steps"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"

### Z2-R-AUDIO-005 Required audio device maps to exit code 4

- **Statement:** When `audio.device.required` is true and no output device reaches RUNNING within 2,000 ms of `Start`, the module start fails with `MODULE_ERROR_DEVICE` (exit code 4); when it is false (the default) a missing device never fails the start.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_RequiredDeviceFailsStart` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"

### Z2-R-AUDIO-006 Headless null-device path

- **Statement:** A headless start ([Z2-N-ARCH-867]) creates no miniaudio context and no output device, mixes through the `Zenith.AudioNul` null pacer at real-time rate, and runs voices, events, virtualisation and budget counters exactly as a run with the null backend does; `Audio.Mix` runs every frame headless too.
- **Covers:** Z2-I-AUDIO-03, Z2-I-AUDIO-05, Z2-I-NET-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_HeadlessUsesNullPacer` and `Test_AudioIntegration_HeadlessGame300Ticks` pass, and MOD's `Test_Runtime_HeadlessSkipsDevice` passes with Audio selected, in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Headless null-device path"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Null pacer thread loop"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Backend selection"

### Z2-R-AUDIO-007 Audio worlds attach and the world limit

- **Statement:** `AttachWorld` creates the world's audio state and command queue, registers the queue as the world resource `Zenith.Audio.CommandQueue`, enters the world in the tag table and adds `Audio.EventBridge` and `Audio.Update@W<tag>`; at most eight audio worlds are enabled; a world beyond them still registers its queue and nodes but gets no voice or instance tables (its commands dropped and counted, logged once), and the `DetachWorld` of an enabled world enables the lowest-tag disabled world at once.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_NinthWorldDisabled` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"; `Zenith2/Docs/Design/AUDIO.md` → "Following the presenting world"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "World frame inputs"

### Z2-R-AUDIO-008 Shutdown and detach order

- **Statement:** Module destruction at shutdown step 6 stops the device and the null pacer, joins `Zenith.AudioCtl`, `Zenith.AudioDec` and `Zenith.AudioNul`, frees all sample memory and drops every asset handle; `DetachWorld`, on the main thread outside frames, removes the world from the tag table, bumps its world generation, releases its handles and frees its instance, voice and request tables at once with no wait, while the memory the device may read stays protected by the released epoch and the world's device slots are freed in `Audio.Mix` as their fades end.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_ShutdownJoinsThreads` and `Test_AudioSystem_DetachFreesTablesAtOnce` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Shutdown and detach"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"

### Z2-R-AUDIO-009 Audio.Update runs once per world per frame

- **Statement:** `Audio.Update@W<tag>` is one epilogue stage node per audio world registered with `AddEpilogueStage`, declares reads of the audio components, `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform`, `ReadAnyRow` for followed entities, `ReadResource` of `Zenith.Runtime.WorldState` and the sole-consumer `WriteResource` of `Zenith_AudioCommandQueue`, runs exactly once per frame whatever the number of ticks, concurrently with that world's extraction, never waits, and reads or writes only its own world's audio state (no voice slot, stream slot, cache, feedback ring or other process structure).
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-AUDIO-002
- **Acceptance evidence:** `Test_FrameGraph_AudioUpdateOncePerFrame` (frames with 0, 1 and 4 ticks) in the `Test` step (win-debug, win-release); the `Libgraph` step.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Frame-graph placement"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step"

### Z2-R-AUDIO-010 Cross-world mixing in the engine-wide Audio.Mix stage

- **Statement:** Feedback draining, loader hand-off, primary-world choice, real-voice selection and device-slot assignment, stream-slot and cache allocation, mixer control, the snapshot publication, retirement and counter publication run once per frame in `Audio.Mix`, the engine-wide EPILOGUE stage-13 node registered with `AddEngineEpilogueStage` and ordered after every world's `Audio.Update` ([Z2-N-ARCH-442]), in world-tag order, so the mix is a function of the worlds' frame records alone and independent of worker scheduling, world completion order and worker count; a world whose node is not yet scheduled contributes nothing without stalling it, and no interim form runs the mix inside a world's node.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioUpdate_ThreeWorldsBitExactAcrossWorkerCounts`, `Test_AudioUpdate_WorldWithoutNodeDoesNotStall`, `Test_FrameGraph_AudioMixAfterEveryWorldUpdate`, `Test_AudioOffline_BitExactAcrossWorkerCounts` and `Test_AudioVoices_SecondaryWorldShare` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm"

### Z2-R-AUDIO-011 Command queue drains in merge order

- **Statement:** `Zenith_AudioCommandQueue` receives commands from sync-point playback of the audio command kinds and from the script consumer in merge order, sync points in phase order and ticks in order, and `Audio.Update` applies them in exactly that order.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCommands_QueueOrderIsMergeOrder` and `Test_AudioIntegration_ReplayIdenticalCommands` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommandQueue header"; `Zenith2/Docs/Design/AUDIO.md` → "Command record layout"

### Z2-R-AUDIO-012 Command queue full policy is drop newest

- **Statement:** A full command queue drops the incoming command and counts it in `audio.commands.dropped_delta`; the queue's capacity is `audio.commands.capacity`, a power of two in [256, 65536], allocated once at attach.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-SCRIPT-023, Z2-B-JOBS-002
- **Acceptance evidence:** `Test_AudioCommandQueue_FullDropsNewest` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommandQueue header"; `Zenith2/Docs/Design/AUDIO.md` → "Breach-forcing tests"

### Z2-R-AUDIO-013 Stage 3 audio command kinds and payload validation

- **Statement:** AUDIO registers the six Stage 3 `Audio.*` command kinds (`Audio.PostEvent`, `Audio.StopInstance`, `Audio.StopEntity`, `Audio.SetParameter`, `Audio.SetGlobalParameter` and `Audio.SetBusPaused`) with 64-byte reflected payload structs carrying `Zenith_EntityHandle` entity references and phases 1 to 10 allowed; every playback validates its payload as untrusted data, finds the world's queue in AUDIO's tag table by `Zenith_WorldPlaybackAccess::GetWorldTag()` ([Z2-N-ARCH-716]), drops and counts an invalid payload or a world without audio, and never asserts; native systems record them through the `Zenith_AudioRecord*` helpers. The six Stage 4 kinds are Z2-R-AUDIO-170.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCommands_PlaybackValidatesPayload` in the `Test` step (win-debug, win-release) and the `Zenith_Fuzz_TriggerPayload` corpus run in `Sanitizers -Preset win-asan`; `Test_AudioCommands_PlaybackFindsWorldByTag` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command kinds"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command payloads"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Recording helpers"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"

### Z2-R-AUDIO-014 Stage 3 audio components registered

- **Statement:** The listener, emitter and bank components register under `Zenith.Audio.*` stable names, version 1, with the fixed property IDs and ranges of the design and authority `PHASE_COMMANDS_ONLY`, and serialise by reflection; the ticket that registers them adds their three rows to GAME's authority census in its own diff (GAME change-control rule 3). The zone, portal and occluder components are Z2-R-AUDIO-171.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioComponents_RegisteredWithAuthority` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component conventions"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component registration"

### Z2-R-AUDIO-015 Listener selection and attenuation position

- **Statement:** Each audio world uses its highest-priority active listener (ties to the lowest entity index), takes orientation from the listener entity's world rotation, and measures distance and occlusion from the position blended toward the attenuation entity by `AttenuationBlend`.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_AttenuationEntityBlend` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioListenerComponent"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space"

### Z2-R-AUDIO-016 Emitter reconciliation

- **Statement:** Every frame an auto-play emitter without an instance gets one, parameters push to its instance with a 50 ms ramp, an event change restarts it when configured, and a removed emitter row stops its instance with its stop fade.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioEmitter_AutoPlayAndStopOnRemove` and `Test_AudioIntegration_EmitterInScene` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioEmitterComponent"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step"

### Z2-R-AUDIO-017 Bank residency through data

- **Statement:** A bank is resident while an emitter's bank, a `Zenith.Audio.Bank` row, the master bank or the always-loaded list holds it; posting an event of an unloaded bank plays nothing and is counted, never starting a load on the trigger path. AUDIO's retention cache, the fifth holder, is Z2-R-AUDIO-172.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioIntegration_EmitterInScene` and `Test_AudioDiagnostics_WarnOncePerId` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank dependencies and residency"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioBankComponent"

### Z2-R-AUDIO-018 Audio configuration keys

- **Statement:** AUDIO registers every `audio.*` key of the design with its type, default, range and allowed layers, none simulation-affecting, and reads them into `Zenith_AudioSettings` at start; headless forces the null pacer and an unrequired device.
- **Covers:** Z2-I-AUDIO-04, Z2-I-AUDIO-05, Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioConfig_KeysPresentationOnly` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSettings"

### Z2-R-AUDIO-019 Platform configuration fragments

- **Statement:** AUDIO's platform configuration fragments `Internal/Config/Platform/windows.config.json` and `android.config.json` carry `audio.voices.realMax`, `audio.mixer.overrunSharePercent` and `audio.commands.capacity` per platform from Z2-B-AUDIO-004, Z2-B-AUDIO-003 and Z2-B-AUDIO-010, merged by PLAT's `plat.config` step, each value citing its row; reverb quality and the occlusion ray cap per tier are Z2-R-AUDIO-173.
- **Covers:** Z2-I-AUDIO-03, Z2-I-AUDIO-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AUDIO-003, Z2-B-AUDIO-004, Z2-B-AUDIO-010
- **Acceptance evidence:** The `plat.config` cook of the fragments succeeds in the `Test` step (`Test_AudioConfig_KeysPresentationOnly` reads the merged values).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Internal files"; `Zenith2/Docs/Design/AUDIO.md` → "The Stage 1 AUDIO budget ruling"

### Z2-R-AUDIO-020 Audio error enumeration and contract blocks

- **Statement:** AUDIO's fallible public functions return `Zenith_Result` with the 8-byte `Zenith_AudioError` record whose code enumeration starts at 1; every public type carries a complete contract block.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The `ApiContractCheck` and `HeaderSelfContained` gate steps pass.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Error enumerations"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioError"; `Zenith2/Docs/Design/AUDIO.md` → "Result rules"

### Z2-R-AUDIO-021 Deterministic audio maths

- **Statement:** `Zenith_AudioMath` computes dB, gain, cents, exp2, log2, sin, cos, atan2, asin and acos with the published binary32 coefficient tables and reductions (Cody–Waite by π/2 with sine and cosine kernels on [−π/4, π/4], the tan(π/8) arctangent identity, round-to-nearest exp2 on [−0.5, 0.5], the atanh-form log2), no fused multiply-add and no `<cmath>` transcendental call, within the stated bounds (exp2 1.5 × 10^-7 relative, sin and cos 1.5 × 10^-7, atan2 4 × 10^-7 rad, asin and acos 6 × 10^-7 rad, the dB round trip 2.5 × 10^-5 dB), with bit-identical results for identical inputs.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMath_DbGainRoundTrip`, `Test_AudioMath_AgainstDoubleReference`, `Test_AudioMath_TrigErrorBounds` and `Test_AudioMath_BitIdenticalAcrossCalls` in the `Test` step (win-debug, win-release); `Test_AudioMath_CoefficientBitsMatchTable` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Decibel and gain conversions"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Deterministic trigonometry"

### Z2-R-AUDIO-022 Audio identifiers are hashes of names

- **Statement:** Event and dialogue-line IDs are `Zenith_Hash64` of the UTF-8 path, bus, parameter, snapshot, music-state and marker IDs its low 32 bits; instance IDs are never 0 and fall in three disjoint classes by two marker bits: native IDs have bit 0 and bit 63 clear, script IDs bit 0 set and bit 63 clear, AUDIO's internal anonymous IDs bit 0 clear and bit 63 set.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioTypes_IdsAreHash64OfNames` and `Test_AudioTypes_NativeInstanceIdNeverScriptId` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Identifiers"; `Zenith2/Docs/Design/AUDIO.md` → "Handle and identifier kinds"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"

### Z2-R-AUDIO-023 Sound file format version 1 and codec identifiers

- **Statement:** A `.zsnd` file is an ASSET cooked file with magic `ZSND`, kind 10 and format version 1 whose sections are exactly the header, samples, seek table, markers and codec setup sections of the design, stored with section codec 0, with the audio codec ID (1 PCM16, 2 IMA ADPCM, 3 and 4 reserved for D-SEL-CODEC) in AUDIO's own header.
- **Covers:** Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004, D-SEL-CODEC (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSoundFile_ParsesEveryCodecAndLayout` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file sections"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound header section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "PCM16 sample section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "IMA ADPCM sample section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Codec identifiers"

### Z2-R-AUDIO-024 Sound file checks

- **Statement:** The sound parser applies checks SND1 to SND12 in order before using any field, enforces the parser limits of eight channels and 192 kHz, and fails a load with `AUDIO_ERROR_CORRUPT_DATA` and the check number, never asserting on any input.
- **Covers:** Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSoundFile_RejectsEachCheck` in the `Test` step (win-debug, win-release) and the `Zenith_Fuzz_SoundFile` corpus run in `Sanitizers -Preset win-asan`; `Test_AudioCook_SeekTableLoopEntryPassesSnd8` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks"

### Z2-R-AUDIO-025 Bank file format version 1

- **Statement:** A `.zbank` file is an ASSET cooked file with magic `ZBNK`, kind 11 and format version 1 holding the header, sound reference, event, node, curve, music, dialogue, embedded mixer, string table and locale variant sections with the record layouts of the design, every field at its documented offset with natural alignment of every 64-bit field in the record and in the file (the embedded mixer's nine tables each preceded by a `u64` count) and reserved bytes zero, so each record's fields sum to its stated size.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_BankFlattensNodes` and `Test_AudioCook_MasterBankEmbedsMixer` in the `Test` step; `Test_AudioCook_RecordLayoutsSumToSizes` and `Test_AudioCook_EventEnumFieldsRoundTrip` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank sections"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Event table section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Node table section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Embedded mixer section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Locale variant section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record"

### Z2-R-AUDIO-026 Bank file checks

- **Statement:** The bank parser applies checks BNK1 to BNK11 before use, including the range of both 3-bit enumeration fields of the event record and every mixer table count against its cap, and fails the load with the check number, never asserting on any input.
- **Covers:** Z2-I-AUDIO-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBankFile_RejectsEachCheck` and `Test_AudioCook_EventEnumFieldsRoundTrip` in the `Test` step (win-debug, win-release) and the `Zenith_Fuzz_BankFile` corpus run in `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks"

### Z2-R-AUDIO-027 Sound kind loader

- **Statement:** The kind-10 loader requests the resident prefix (and the samples into an AUDIO-owned buffer for resident sounds), validates, builds an immutable record from the sound record pool, reports resident bytes under `MEMORY_SUBSYSTEM_AUDIO`, pushes `SOUND_ACTIVATED` to the loader hand-off and activates; a corrupt sound activates as the silent NULL_OBJECT fallback; `Unload` pushes `SOUND_UNLOADED`, acknowledges at once and leaves the memory to retirement by the released epoch.
- **Covers:** Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-AUDIO-001
- **Acceptance evidence:** `Test_AudioSoundLoader_ActivatesResidentPcm`, `Test_AudioSoundLoader_CorruptBecomesNullObject`, `Test_AudioSoundLoader_StreamedLoadsPrefixOnly` and `Test_AudioSoundLoader_UnloadRetiresAfterEpoch` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Sound loader"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_SoundAsset"

### Z2-R-AUDIO-028 Bank kind loader and process event index

- **Statement:** The kind-11 loader validates and builds an immutable bank record with its full tables (no bank is ever dormant), pushes `BANK_ACTIVATED` or `BANK_UNLOADED` to the loader hand-off from any ASSET slice concurrently, and `Audio.Mix` alone updates the bank list, rebuilds the process event index, keeps the first-joined bank's event on a duplicate ID (counted, warned once) and publishes the index.
- **Covers:** Z2-I-AUDIO-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBankLoader_EventIndexSorted` and `Test_AudioBankLoader_DuplicateEventKeepsFirst` in the `Test` step (win-debug, win-release); `Test_AudioBankLoader_ConcurrentActivationsHandOff` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_SoundBankAsset"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Event and parameter lookup"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Loader hand-off"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication"

### Z2-R-AUDIO-029 Master bank at start with built-in fallback

- **Statement:** The module start loads the master bank at CRITICAL priority with `ServiceUntilComplete` (5,000 ms) outside frames; a failure keeps the built-in master-only layout with one bus per volume category and `Master/Audition`, logs an error and does not fail the start.
- **Covers:** Z2-I-AUDIO-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_MasterBankFailureUsesBuiltInLayout` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Master bank at start"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Built-in buses"

### Z2-R-AUDIO-030 Load mode rule

- **Statement:** The cook resolves `auto` load modes by duration: ≤ 1.5 s resident PCM16, ≤ 10 s resident IMA ADPCM, ≤ 30 s resident compressed with the ruled codec, longer sounds, music segments and dialogue lines streamed; before the D-SEL-CODEC ruling it emits ADPCM and streamed PCM16 instead of the codec.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-CODEC (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_LoadModeRule` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Load mode rule"

### Z2-R-AUDIO-031 Decoded-sample cache

- **Statement:** Cache-eligible resident compressed sounds play from a least-recently-used decoded int16 cache capped by `audio.cache.decodedBytes`; a miss plays from a memory stream while a decode-thread job fills the cache, and eviction never frees an entry a voice references.
- **Covers:** Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-AUDIO-001
- **Acceptance evidence:** `Test_AudioCache_HitMissEviction` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Decoded-sample cache"

### Z2-R-AUDIO-032 IMA ADPCM decoding

- **Statement:** IMA ADPCM decodes 256-frame blocks independently with the standard step and index tables, on the device thread for resident sounds and on the decode thread for streams, and equals the cook encoder's reconstruction bit for bit.
- **Covers:** Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioAdpcm_DecodeMatchesReferenceEncoderRoundTrip` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "IMA ADPCM decoding"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "IMA ADPCM sample section"

### Z2-R-AUDIO-033 Stream decoder interface

- **Statement:** Every streamed or compressed decode goes through `Zenith_AudioStreamDecoder` created by `Zenith_CreateAudioStreamDecoder`, on the decode thread only, with codec errors returned as `AUDIO_ERROR_DECODE` and never asserted; the device thread never runs a codec.
- **Covers:** Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-CODEC (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDecode_SeekIsSampleAccurate` and `Test_AudioRealTime_SourceSetIncludeWhitelist` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioStreamDecoder header"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Codec decoder interface"

### Z2-R-AUDIO-034 Sample-accurate seeking and seamless loops

- **Statement:** Seeking any codec to frame f produces exactly frame f (seek-table entry plus discarded pre-roll), and a loop wrap produces the loop end followed by the loop start with no gap or repeated frame; PCM16 and IMA ADPCM are bit-equal to a continuous decode, and a stateful codec's output after its pre-roll is within −60 dBFS of it (ruling Q92).
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02, Z2-I-AUDIO-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-CODEC (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDecode_SeekIsSampleAccurate` and `Test_AudioDecode_LoopIsSeamless` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Seeking and loop points"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Seek table section"

### Z2-R-AUDIO-035 D-SEL-CODEC decode benchmark harness

- **Statement:** The D-SEL-CODEC harness benchmark `Zenith_Bench_AudioCodec` decodes generated signals encoded by both candidates at the stated bitrates and reports decode CPU per voice (judged against the pending row "Audio stream decode CPU", because decode runs on `Zenith.AudioDec`), sample-accurate seek time, loop accuracy and encoded size per minute on the reference desktop and the S24 into `Zenith2/Docs/Research/AudioCodec/report.json`.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02
- **Stage:** S2
- **Classification:** research
- **Decisions:** D-SEL-CODEC (open, Stage 2), Z2-D-084
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** `Zenith_Bench_AudioCodec --smoke` in the `Test` step and the committed report.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Benchmarks"

### Z2-R-AUDIO-036 D-SEL-CODEC ruling

- **Statement:** The `needs-human` D-SEL-CODEC ruling applies the quality criterion (ABX at 96 kbit/s stereo and 48 kbit/s mono, 16 trials per clip, a clip failed at ≥ 12 of 16 correct, a candidate out when it fails more than 10% of clips at 96 kbit/s) and then the acceptance rule (decode CPU against the pending decode row, exact-frame seek within 5 ms, seamless loops, a permissive licence, package size against Z2-B-PACK-001, ties to the smaller binary), and its diff appends the ruling to the DecisionLog (amending the elaborated rule's CPU row references), the manifest, the CMake `THIRD_PARTY` list and the codec table.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02
- **Stage:** S2
- **Classification:** research
- **Decisions:** D-SEL-CODEC (open, Stage 2), Z2-D-084, Z2-D-097
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** The ruling entry in `Zenith2/Docs/DecisionLog.md` and the report's corpus hashes, on the ruling ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria"; `Zenith2/Docs/Design/AUDIO.md` → "Selection decisions AUDIO owns"

### Z2-R-AUDIO-037 Ruled codec integrated

- **Statement:** The codec D-SEL-CODEC selects is decoded by the third `Zenith_AudioStreamDecoder` implementation, encoded by the `audio.sound` step, accepted by the loaders under its reserved ID and confined to `Internal/`; the unruled candidate's ID stays invalid.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-CODEC (open, Stage 2), Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDecode_SeekIsSampleAccurate` and `Test_AudioDecode_LoopIsSeamless` over the codec, and the `Libgraph` step.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Codec decoder interface"

### Z2-R-AUDIO-038 Stream slots state machine

- **Statement:** Each of at most `audio.streams.max` stream slots follows the IDLE, OPENING, PREBUFFERING, PLAYING, STARVED, SEEKING and CLOSING transitions of the design, its state written only by `Audio.Mix` (M7 decisions and M1 decode events, Z2-R-AUDIO-163), and a streamed voice becomes eligible for real selection only once the slot's PREBUFFERED event has been drained.
- **Covers:** Z2-I-AUDIO-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_StateMachine` (stereo and 5.1 streams), `Test_AudioStream_StartAfterPrebuffer` (every read delayed by the scripted read source) and `Test_AudioStream_MixDecodeChannelStress` in the `Test` step (win-debug, win-release); the stress test also in `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream start latency"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"

### Z2-R-AUDIO-039 Stream refill through raw-range reads with incremental decode

- **Statement:** Streams read 64 KiB chunks with `Zenith_AssetReader::ReadRange` into a chunk buffer pair, always one chunk ahead of the one being decoded, and decode the current chunk one packet at a time only while the ring has room for a whole packet, carrying an incomplete tail of at most 8,193 bytes into the next buffer's 8,208-byte carry area, so no decoded frame is held outside the ring, none is lost or duplicated, and the buffer an outstanding read targets is never touched before its operation completes.
- **Covers:** Z2-I-AUDIO-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_StartAfterPrebuffer` and `Test_AudioDecode_LoopIsSeamless` in the `Test` step (win-debug, win-release); `Test_AudioStream_ChunkLargerThanRingDecodesIncrementally` and `Test_AudioStream_CarryAreaHoldsLongestTail` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm"

### Z2-R-AUDIO-040 Stream starvation never blocks the device

- **Statement:** An empty stream ring makes its voice output silence for the missing frames with one underrun record per episode; `Audio.Mix` then moves the slot to STARVED and pushes STARVE, so the decode thread raises the stream's reads to CRITICAL until it answers STARVED_RECOVERED at the prebuffer, and a voice starved for 500 ms is virtualised.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_UnderrunOutputsSilence` (a read withheld by the scripted read source) and `Test_AudioStream_StarvationRaisesPriority` (the scripted read source reports each read's priority) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream priorities and starvation"; `Zenith2/Docs/Design/AUDIO.md` → "Stream starvation"

### Z2-R-AUDIO-041 Stream cancellation on close

- **Statement:** Closing a stream (a CLOSE request from `Audio.Mix`) makes the decode thread request cancellation of its outstanding read through the read source and keep the destination buffer and ring until the read reports a terminal status before it answers CLOSED; `Audio.Mix` alone retires the stream block, after CLOSED and at its epoch; shutdown waits for outstanding reads at most 2,000 ms.
- **Covers:** Z2-I-AUDIO-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_CancelOnClose` (a read withheld by the scripted read source) and `Test_AudioStream_MixDecodeChannelStress` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"; `Zenith2/Docs/Design/AUDIO.md` → "Cancellation"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"

### Z2-R-AUDIO-042 Stream slot limit and stealing

- **Statement:** When every stream slot is busy a new streamed voice takes the slot of the lowest-ranked virtual streamed voice, and stays virtual (counted) when none is virtual.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_SlotStealing` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"

### Z2-R-AUDIO-043 Virtual streamed voices release and reopen

- **Statement:** A streamed voice virtual for 500 ms releases its slot and reopens at its virtual playhead with a sample-accurate seek and a 10 ms fade-in when selected real; music segments and dialogue lines keep their streams.
- **Covers:** Z2-I-AUDIO-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_StateMachine` and `Test_AudioVoices_VirtualBehaviours` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Virtual streamed voices"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices"

### Z2-R-AUDIO-044 Audio memory consumer

- **Statement:** AUDIO registers one memory consumer for `MEMORY_SUBSYSTEM_AUDIO` whose resident bytes sum sample memory, bank records, the cache, stream rings, mixer buffers and pending retirements, published as `audio.memory.bytes`.
- **Covers:** Z2-I-CORE-21, Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AUDIO-001
- **Acceptance evidence:** `Test_AudioMemory_ResidentBytesMatchAllocations` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Memory consumer"

### Z2-R-AUDIO-045 Audio shedding under memory pressure

- **Statement:** At WARNING AUDIO reduces stream prebuffers to 2,048 frames and purges the decoded-sample cache to 50%; at CRITICAL it stops virtual voices and releases its own handles on banks with no playing voice; shedding never changes simulation-visible state.
- **Covers:** Z2-I-CORE-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AUDIO-001
- **Acceptance evidence:** `Test_AudioMemory_SheddingAtEachLevel` (BR-GOVERNOR) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding"; `Zenith2/Docs/Design/AUDIO.md` → "Allocation failure and memory pressure"

### Z2-R-AUDIO-046 Bus tree and block signal flow

- **Statement:** The mixer renders 256-frame blocks at 48 kHz through a bus tree of at most 64 buses and depth 8 rooted at `Master`, processing buses in post-order with insert effects, gain ramp and duck gain before summing into the parent.
- **Covers:** Z2-I-AUDIO-04, Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMixer_BusTreeSumsInPostOrder` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Bus hierarchy"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Signal flow per block"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step"

### Z2-R-AUDIO-047 Aux sends and returns

- **Statement:** A voice has at most two post-fader sends to at most four aux-return buses, whose processors are preallocated at layout build.
- **Covers:** Z2-I-AUDIO-04, Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMixer_SendsReachAuxReturns` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Aux sends and returns"

### Z2-R-AUDIO-048 Mix snapshots

- **Statement:** Up to eight active mix snapshots blend bus volume offsets additively and filter cutoffs by minimum or maximum in log frequency with linear fade weights, and never affect the `Audition` bus.
- **Covers:** Z2-I-AUDIO-04, Z2-I-AUDIO-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMixSnapshot_BlendsOverrides` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mix snapshots"

### Z2-R-AUDIO-049 Ducking

- **Statement:** Ducking rules reduce a bus while its sidechain bus's 10 ms RMS exceeds the threshold, with the rule's attack, hold and release, multiple rules adding in dB and floored at −60 dB.
- **Covers:** Z2-I-AUDIO-04, Z2-I-AUDIO-10, Z2-I-AUDIO-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDucking_AttackHoldRelease` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Ducking"

### Z2-R-AUDIO-050 User volume categories

- **Statement:** The six user volume keys each scale the bus their category maps to (master the output) with a 50 ms ramp, applied from the configuration store's user-change callback.
- **Covers:** Z2-I-AUDIO-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMixer_UserVolumeRampsCategory` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "User volume categories"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"

### Z2-R-AUDIO-051 Output layouts and downmix

- **Statement:** The mixer renders in stereo, headphones, 5.1 or 7.1, `auto` following the device channel count, and folds multichannel sources with the ITU-R BS.775 matrices.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMixer_DownmixMatricesUnitPower` and `Test_AudioMixer_OutputLayoutFollowsDevice` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Output layouts and downmix"

### Z2-R-AUDIO-052 Block FIFO serves any callback size

- **Statement:** The mixer serves device callbacks of any frame count from a one-block FIFO by render-then-copy, rendering a block only into an empty FIFO, so the FIFO never holds more than 256 frames, a callback renders exactly ⌈max(0, uFrames − b0) / 256⌉ blocks, and the output is identical whatever the backend's callback size.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRealTime_CallbackSizesAnyFrameCount` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread"; `Zenith2/Docs/Design/AUDIO/Device.md` → "AAudio configuration"

### Z2-R-AUDIO-053 Hermite resampler

- **Statement:** Each voice is resampled by cubic Hermite interpolation on a 32.32 fixed-point position with the pitch ratio ramped per block, clamped to [1/16, 16], reading across loop points.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioResampler_HermiteMatchesReference` and `Test_AudioResampler_LoopCrossesSeamlessly` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Resampler"

### Z2-R-AUDIO-054 Filters and bus effects

- **Statement:** Voice low-pass and high-pass filters and the bus EQ, compressor and delay effects implement the RBJ biquads, the soft-knee feed-forward compressor and the damped feedback delay of the design.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBiquad_ResponseMatchesRbj`, `Test_AudioEq_BandsMatchRbj`, `Test_AudioCompressor_StaticCurve` and `Test_AudioDelay_TimeAndFeedback` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Biquad filters"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Bus effects"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Equaliser"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Compressor"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Delay effect"

### Z2-R-AUDIO-055 Master limiter and final clamp

- **Statement:** The master limiter keeps every output frame at or below its ceiling with no lookahead, and the final clamp limits samples to [−1, 1] and replaces NaN and infinity by 0, counting them.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioLimiter_NeverExceedsCeiling` and `Test_AudioMixer_FinalClampClearsNaN` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Limiter and final clamp"

### Z2-R-AUDIO-056 Click-free ramps and fades

- **Statement:** Gain, filter and pan targets ramp across each block, and starts, stops, steals and real-virtual changes are faded (10 ms, at least 5 ms, 20 ms), so no step above −60 dBFS appears in the output.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_StartStopFadesNoClick` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Gain and pan ramps"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Fades and click suppression"

### Z2-R-AUDIO-057 Instances and voice tables

- **Statement:** Instances live in per-world tables of `audio.voices.instanceMax` entries and voices in per-world tables of `uAUDIO_MAX_VOICES_PER_WORLD` entries allocated at attach, each world with its own start-sequence counter; device voice slots (256, 16 of them the audition world's) are assigned only by `Audio.Mix`, only to real voices; instance states follow the design's table.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_InstanceTableFullRefuses` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Instances voices and slots"

### Z2-R-AUDIO-058 Deterministic event node evaluation

- **Statement:** Posts evaluate the event's node tree with random draws from `Zenith_RandomSource` keyed by the instance seed and node index, the seed being the instance ID for an identified post and `Zenith_Hash64` of (world tag, recording tick, the command's ordinal among its own tick's drained commands) for an anonymous one, so consecutive anonymous posts differ and identical commands in a replay yield identical voices at any frame cadence.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioEvents_RandomNodeDeterministicPerInstance` and `Test_AudioEvents_NodeKinds` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Node table section"

### Z2-R-AUDIO-059 Audibility and priority ordering

- **Statement:** Each frame every voice's audibility is computed from the documented terms, and candidates are ordered by audible first, event priority, 0.5 dB-quantised audibility, the older instance (lower world tag, then lower per-world start sequence) and the lower voice index, a total order independent of gathering order.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-AUDIO-004
- **Acceptance evidence:** `Test_AudioVoices_WorkedExampleSelection` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Audibility estimate"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Voice selection worked example"

### Z2-R-AUDIO-060 Real-voice limit defers excess voices

- **Statement:** At most L = min(`audio.voices.realMax`, 240) game-world voices are real (the audition world's 16 slots are outside L); every candidate kept virtual by the limit is counted in `audio.voices.deferred_delta` and re-evaluated every frame; the deferral count, priority rule 4 between audibility and instance age, orders deferred candidates for the slots that are free in a frame, so the longest-deferred candidate takes a freed slot whatever its instance's age, and it never displaces a real voice that hysteresis or the minimum real time protects (no flapping).
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-AUDIO-004
- **Acceptance evidence:** `Test_AudioVoices_RealLimitDefersExcess` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"; `Zenith2/Docs/Design/AUDIO.md` → "Budget breaches"

### Z2-R-AUDIO-061 Hysteresis minimum real time and per-bus limits

- **Statement:** A real voice is displaced only by a candidate louder by `audio.voices.stealHysteresisDb` or of higher priority, keeps its slot for `audio.voices.minRealMs`, and per-bus real-voice limits apply to a bus and its ancestors.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_HysteresisPreventsFlapping`, `Test_AudioVoices_MinRealTimeHonoured` and `Test_AudioVoices_PerBusLimit` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm"

### Z2-R-AUDIO-062 Instance limits steal modes and cooldown

- **Statement:** A post beyond an event's per-world instance limit stops the OLDEST, QUIETEST or FARTHEST instance with a 20 ms fade or is refused under NONE, and a post within the event's cooldown is refused and counted.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_StealModes` and `Test_AudioVoices_CooldownRefusesRepeat` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Stealing and instance limits"

### Z2-R-AUDIO-063 Virtual voice playheads

- **Statement:** A virtual voice advances its playhead by elapsed time times pitch, wrapping loops, and resumes at that position with a 10 ms fade-in when made real again.
- **Covers:** Z2-I-AUDIO-03, Z2-I-AUDIO-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_VirtualPlayheadAdvances` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices"

### Z2-R-AUDIO-064 Virtual voice behaviours

- **Statement:** The event's virtual behaviour CONTINUE, PAUSE, RESTART or KILL governs a virtual voice, KILL finishing an instance whose voices stay virtual for 500 ms.
- **Covers:** Z2-I-AUDIO-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_VirtualBehaviours` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices"

### Z2-R-AUDIO-065 Primary world and secondary worlds share the voice limit

- **Statement:** `Audio.Mix` makes the updated world with the highest `SessionOrdinal` of `Zenith.Runtime.WorldState` (ties to the lowest tag) the primary world, limited only by L; every other updated world is secondary and takes at most `audio.voices.secondaryWorldRealMax` real voices out of L; every updated world's real voices are mixed, and the audition world has 16 reserved slots it neither yields nor takes from game worlds. Ignoring a secondary world's music commands is Z2-R-AUDIO-174.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_SecondaryWorldShare` in the `Test` step (win-debug, win-release); `Test_AudioVoices_AuditionSlotsReserved` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"

### Z2-R-AUDIO-066 Listener space convention

- **Statement:** Listener-space vectors follow the right-handed +Y-up, −Z-forward convention; positions and rotations are the presented ones, interpolated between `Zenith.PreviousWorldTransform` and `Zenith.WorldTransform` with the frame's alpha ([Z2-N-ARCH-415]), and velocities are tick-to-tick differences over the tick duration (zero on a teleport).
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_StereoPanConstantPower` in the `Test` step (win-debug, win-release); `Test_AudioSpatial_PositionsInterpolatedWithAlpha` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space"

### Z2-R-AUDIO-067 Distance attenuation curves

- **Statement:** Attenuation follows the none, inverse, linear, logarithmic and custom curves of the preset between its minimum and maximum distance with the end fade, and a voice beyond the maximum distance is inaudible and virtual.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_AttenuationCurves` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Distance attenuation curves"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Attenuation presets"

### Z2-R-AUDIO-068 Air absorption and cones

- **Statement:** Air absorption lowers the cutoff to max(1000, 20000 · 2^(−r/D)) Hz and cones interpolate between inner and outer gain in dB by the emitter's +Z axis.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_AirAbsorptionCutoff` and `Test_AudioSpatial_ConeGain` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Air absorption"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Cones"

### Z2-R-AUDIO-069 Doppler

- **Statement:** Doppler pitch follows ratio = (c + v_L·n)/(c + v_E·n) with the tick-to-tick velocities clamped to 100 m/s, raised to the event's factor, clamped to [0.5, 2], smoothed over 50 ms and reset by teleports and origin shifts, so it is constant between ticks and free of tick-rate ripple at any display rate.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_DopplerRatio` in the `Test` step (win-debug, win-release); `Test_AudioSpatial_DopplerSteadyAt30HzTicks144HzFrames` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Doppler"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space"

### Z2-R-AUDIO-070 Stereo and headphone panning

- **Statement:** Stereo uses constant-power panning folded to the front with up to 3 dB rear attenuation and a rear low-pass; headphones add an interaural delay of up to 14 frames.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_StereoPanConstantPower` and `Test_AudioSpatial_HeadphoneInterauralDelay` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Panning to stereo and headphones"

### Z2-R-AUDIO-071 Surround panning

- **Statement:** 5.1 and 7.1 outputs use pairwise constant-power panning over the speaker azimuths of the design, the centre limited for non-dialogue voices and LFE fed only by the event's LFE send.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_VbapSurround` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Panning to 5.1 and 7.1"

### Z2-R-AUDIO-072 Spread stereo and multichannel sources

- **Statement:** Spread renders three weighted points up to uniform power at 360°, stereo sources pan as two points, multichannel sources play as unpositioned beds, and positional multichannel sounds downmix to mono first.
- **Covers:** Z2-I-AUDIO-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_SpreadReachesUniform` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Spread and source size"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Multichannel sources"

### Z2-R-AUDIO-073 Occlusion by occluder boxes

- **Statement:** Occlusion tests the segment from the attenuation position to the emitter against `Zenith.Audio.Occluder` boxes through an 8 m hash grid, summing transmission losses to a floor of −48 dB with the minimum cutoff, smoothed over 150 ms, with no dependency on the physics module.
- **Covers:** Z2-I-AUDIO-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioOcclusion_SegmentThroughBoxes` and `Test_AudioOcclusion_SmoothingTime` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occluder geometry"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion ray test"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion filter mapping"

### Z2-R-AUDIO-074 Occlusion test cap per frame

- **Statement:** At most `audio.occlusion.raysPerFrame` occlusion tests run per frame in round-robin order, each voice at most every 4 frames, and untested voices keep their previous result (BR-DEFER).
- **Covers:** Z2-I-AUDIO-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioOcclusion_RaysPerFrameCap` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion ray test"

### Z2-R-AUDIO-075 Feedback delay network reverb

- **Statement:** Reverb returns use the 8-line (high) or 4-line (low) feedback delay network with the base delays, RT60 line gains, Hadamard mixing, damping, pre-delay, diffusion and early reflections of the design, with size changes crossfaded over 500 ms.
- **Covers:** Z2-I-AUDIO-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioReverb_DecayMatchesRt60` and `Test_AudioReverb_PresetChangeCrossfades` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Reverb presets"

### Z2-R-AUDIO-076 Reverb quality per tier

- **Statement:** `audio.reverb.quality` selects the 4-line or 8-line network per quality tier through AUDIO's quality fragments, matching the audio features rows of the QualityMatrix.
- **Covers:** Z2-I-AUDIO-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioReverb_DecayMatchesRt60` at both qualities and the merged tier fragments in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb"; `Zenith2/Docs/Design/AUDIO.md` → "Stage 5"

### Z2-R-AUDIO-077 Acoustic zone selection

- **Statement:** The zone of a point is the highest-priority zone box containing it (ties to the smallest volume, then the lowest simulation ID), with a blend band of `fadeDistance` inside the boundary.
- **Covers:** Z2-I-AUDIO-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioZones_PrioritySelection` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Zone selection"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioZoneComponent"

### Z2-R-AUDIO-078 Portals and wall loss

- **Statement:** A voice in another zone is heard through a directly connecting open portal from the portal's direction with the openness loss, and otherwise with the listener zone's wall loss and cutoff, evaluating one portal hop.
- **Covers:** Z2-I-AUDIO-19, Z2-I-AUDIO-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioZones_PortalPath` and `Test_AudioZones_WallLossWithoutPortal` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Portals and propagation"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioPortalComponent"

### Z2-R-AUDIO-079 Zone reverb and ambience

- **Statement:** The zone reverb return crossfades to the listener zone's preset over 500 ms, voices send at the zone's wet level, and zone ambience loops start on entry and fade on exit, blended across the band.
- **Covers:** Z2-I-AUDIO-08, Z2-I-AUDIO-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioZones_ReverbCrossfadeOnZoneChange` and `Test_AudioZones_AmbienceFollowsListener` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb crossfade"

### Z2-R-AUDIO-080 Music segments states and transitions

- **Statement:** Music plays streamed segments of states on the `Music` bus and moves between states by the most specific transition rule, computing IMMEDIATE, NEXT_BEAT, NEXT_BAR, NEXT_GRID, NEXT_CUE and SEGMENT_END boundaries from the device clock and skipping a boundary closer than the prebuffer time.
- **Covers:** Z2-I-AUDIO-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMusic_EveryRuleComputesBoundary` and `Test_AudioMusic_RequestDuringPendingReplaces` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Music model"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Music section"

### Z2-R-AUDIO-081 Sample-accurate music transitions

- **Statement:** A transition's target downbeat lands on its computed device frame exactly, the target stream prepared by a sample-accurate seek before it.
- **Covers:** Z2-I-AUDIO-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMusic_TransitionLandsOnBarFrame` (offline renderer) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling"

### Z2-R-AUDIO-082 Music stingers

- **Statement:** A stinger plays on the next beat or at its transition frame over the music, ducking the segment by 3 dB when its event asks to.
- **Covers:** Z2-I-AUDIO-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMusic_StingerOnBeat` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Stingers"

### Z2-R-AUDIO-083 Dialogue line playback

- **Statement:** `Audio.PlayDialogue` loads the line's sound for the active locale on demand at HIGH priority and plays it when prebuffered, a new line on one speaker entity replaces the previous with a 100 ms fade, and a start more than 300 ms late is counted.
- **Covers:** Z2-I-AUDIO-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDialogue_SameSpeakerReplaces` and `Test_AudioDialogue_LateCounted` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue lines"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Dialogue table section"

### Z2-R-AUDIO-084 Dialogue locale selection

- **Statement:** The dialogue locale is `audio.dialogue.locale`, else `ui.locale` when registered, else the platform's preferred locale, and a line's sound is the exact tag, else the language subtag, else the bank's default entry.
- **Covers:** Z2-I-AUDIO-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDialogue_LocaleFallback` and `Test_AudioDialogue_LocaleSourceOrder` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection"

### Z2-R-AUDIO-085 Locale variant banks

- **Statement:** A locale-neutral base bank lists locale variant banks as soft references; on activation and on every locale change `Audio.Mix` requests the variant matching the active locale (exact tag, then language subtag), whose events replace the base bank's while active, releases the previous variant only after the new one activated, and keeps the base events when no variant matches; no bank loads dormant and no loader is re-run for an active asset.
- **Covers:** Z2-I-AUDIO-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBankLoader_LocaleVariantSwitch` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Locale variant section"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"

### Z2-R-AUDIO-086 Dialogue ducking

- **Statement:** A ducking line reduces `Music` and `SFX` by its duck amount for its duration with 50 ms attack and 400 ms release.
- **Covers:** Z2-I-AUDIO-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDialogue_DucksMusic` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Ducking"

### Z2-R-AUDIO-087 Caption feed for UI and cinematics

- **Statement:** AUDIO records a caption entry per dialogue line start and per captioned sound into the per-world caption feed exposed through `Zenith.Audio.WhenCaption`, keyed by the line ID text for UI's `UI.ShowCaption`, and the `audio.bank` cook step contributes every subtitle and caption key to UI's localization extraction through `Zenith_UIExtractionSink` (row UI20).
- **Covers:** Z2-I-AUDIO-10, Z2-I-UI-11, Z2-I-UI-23, Z2-I-CINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioScript_NotificationsPublishAtTickBoundary` (the caption case) in the `Test` step (win-debug, win-release); `Test_AudioCook_BankContributesSubtitleKeys` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"

### Z2-R-AUDIO-088 Device clock and trigger scheduling

- **Statement:** `Audio.Mix` fits device frames against monotonic time from one clock-sample feedback record per callback, and a trigger with the sub-tick flag recorded in tick k at τ starts at F(now) + round((S − S_r)·48000) + the A/V offset, with S = (k − 1)·Δ + τ and S_r = (K − 1 + α)·Δ by the convention tied to [Z2-N-ARCH-415], clamped to one block … 100 ms ahead; otherwise it starts at the next block.
- **Covers:** Z2-I-AUDIO-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioClock_LinearFitTracksDevice`, `Test_AudioClock_SubTickTriggersKeepSpacing`, `Test_AudioClock_ScheduleClampAndLateCount` and `Test_AudioClock_AvSyncOffsetShiftsStarts` in the `Test` step (win-debug, win-release), and `Test_AudioIntegration_AnimNotifyFootsteps` in the integration suite; `Test_AudioClock_TriggerAlignsWithPresentedTime` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "The device clock"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Scheduling a trigger"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Engine event types bridged"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Feedback record"

### Z2-R-AUDIO-089 Time-locked voices

- **Statement:** A time-locked voice ignores drift up to 240 frames, nudges its pitch by ±0.5% up to 2,048 frames and resyncs by a crossfaded seek beyond, pauses with its world and jumps on `Audio.SeekInstance`.
- **Covers:** Z2-I-AUDIO-20, Z2-I-CINE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioClock_TimeLockedDriftCorrection` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices"

### Z2-R-AUDIO-090 World pause and time scale

- **Statement:** A paused world pauses the voices of buses marked to pause with it after a 20 ms fade while UI, music and dialogue keep playing, and a simulation time scale multiplies the pitch of time-scaled buses, clamped to [0.25, 2].
- **Covers:** Z2-I-AUDIO-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioClock_PauseAndTimeScale` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Pause and time scale"

### Z2-R-AUDIO-091 AUDIO threads

- **Statement:** AUDIO creates exactly the control (`Zenith.AudioCtl`), decode (`Zenith.AudioDec`) and null pacer (`Zenith.AudioNul`, the only `THREAD_PRIORITY_TIME_CRITICAL` thread when no device is open) threads, never blocks a job worker, and only one of the device thread and the pacer mixes at a time with a continuous block counter.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_ShutdownJoinsThreads` and `Test_AudioDevice_LossHandsOverToPacer` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Thread table"; `Zenith2/Docs/Design/AUDIO.md` → "Threads AUDIO owns"

### Z2-R-AUDIO-092 Control thread owns the device

- **Statement:** Only the control thread calls miniaudio's context and device init, start, stop and uninit, draining one single-producer ring each from the main thread, the backend notifications and the focus bridge.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_MiniaudioNullBackendRunsCallback` and `Test_AudioDevice_TransitionTable` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Control thread loop"

### Z2-R-AUDIO-093 Device state machine

- **Statement:** The device state machine defines all 150 cells of its ten states and fifteen events, with who mixes in each state (OPENING keeps the null pacer only when entered from LOST or REROUTING), and an event outside a state's handled set leaves the state unchanged and is counted.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_TransitionTable` in the `Test` step (win-debug, win-release); `Test_AudioDevice_PermanentFocusLossRecoversOnWindowFocus` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "States"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Events"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table"

### Z2-R-AUDIO-094 Device loss hand-over and retry

- **Statement:** An unexpected device stop hands the mix to the null pacer within one block period and retries the open at 0, 1, 2, 4 and 8 s and then every 8 s, retrying immediately on an endpoint or route arrival.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_LossHandsOverToPacer` and `Test_AudioDevice_RetryScheduleAfterLoss` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Retry schedule"; `Zenith2/Docs/Design/AUDIO.md` → "Device loss and change"

### Z2-R-AUDIO-095 Reroute and format change

- **Statement:** A reroute to an endpoint with the same channel count continues without reopening, and one with another channel count or a user layout change reopens with the new layout and a 10 ms fade-in.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_RerouteSameFormatContinues` and `Test_AudioDevice_FormatChangeReopens` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Endpoint changes on Windows"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table"

### Z2-R-AUDIO-096 Backend selection

- **Statement:** `audio.device.backend` selects miniaudio with WASAPI or AAudio (`auto`), miniaudio's null backend, the null pacer or the offline backend, headless forcing the null pacer.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_MiniaudioNullBackendRunsCallback` and `Test_AudioSystem_HeadlessUsesNullPacer` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Backend selection"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Null backend"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Offline backend"

### Z2-R-AUDIO-097 Windows WASAPI output and endpoint changes

- **Statement:** On Windows the device uses WASAPI shared mode with MMCSS Pro Audio, a 480-frame requested period and automatic stream routing, follows default-endpoint changes, and recovers from unplugging the only endpoint.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** Device run record AUDIO-DR-05 on the reference desktop and the WASAPI period ruling, on the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "WASAPI configuration"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Endpoint changes on Windows"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records"

### Z2-R-AUDIO-098 Windows mute when unfocused

- **Statement:** With `audio.muteWhenUnfocused` the master fades to silence over 100 ms on window focus loss and back on focus gain.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_MuteWhenUnfocused` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Focus on Windows"

### Z2-R-AUDIO-099 Android AAudio output and buffer adaptation

- **Statement:** On Android AUDIO opens AAudio in low-latency shared mode (exclusive on request), usage GAME, float, 48 kHz, stereo, with the buffer at least max(2, ceil((256 + burst)/burst)) bursts so it always covers one block plus one burst, raised one burst per xrun increase up to max(8, minimum + 4) and lowered after 60 s without one.
- **Covers:** Z2-I-AUDIO-12, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** Z2-B-AUDIO-003
- **Acceptance evidence:** Device run record AUDIO-DR-01 on the S24 and the AAudio burst ruling, on the Stage 3 acceptance ticket; the `Build -Preset android-arm64` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "AAudio configuration"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Buffer sizing and xrun adaptation"

### Z2-R-AUDIO-100 Android route changes

- **Statement:** Wired, Bluetooth and USB route changes on Android are followed by miniaudio's reroute or by LOST with an immediate retry, and playback continues on the new route within 500 ms.
- **Covers:** Z2-I-AUDIO-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** Device run record AUDIO-DR-02 on the S24.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Device changes on Android"

### Z2-R-AUDIO-101 Android audio focus behaviour

- **Statement:** Focus loss maps to INTERRUPTED (transient and permanent), a can-duck loss to a −12 dB master duck, a gain to a reopen with a 50 ms fade-in; after a permanent loss focus is re-requested at the next RESUME_AUDIO, at every window focus gain and, when the window kept focus, at the first input transition (at most once per 2 s, the default the Stage 3 focus ruling confirms), so a foreground game recovers when the user returns to it or touches it; focus is abandoned at STOP_AUDIO and SHUTDOWN.
- **Covers:** Z2-I-AUDIO-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_FocusStates` in the `Test` step (win-debug, win-release); `Test_AudioDevice_PermanentFocusLossRecoversOnWindowFocus` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio focus"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Events"; `Zenith2/Docs/Design/AUDIO.md` → "Rulings AUDIO records in its own tickets"

### Z2-R-AUDIO-102 Android focus Java bridge

- **Statement:** The focus listener is the Java class `org.zenith2.audio.Zenith2AudioFocus`, loaded through the activity's class loader with natives registered by `RegisterNatives` and the owner passed as a `jlong`, holding no static state; its Java signatures match the native registration table, and a missing class leaves AUDIO running without focus handling with one warning (packaging and its APK check are MOD's and PROC's, pending).
- **Covers:** Z2-I-AUDIO-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioFocusBridge_JavaMatchesNativeRegistration` in the `Test` step (win-debug, win-release); device run record AUDIO-DR-03 on the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Focus Java bridge"; `Zenith2/Docs/Design/AUDIO.md` → "Internal Android files"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"

### Z2-R-AUDIO-103 S24 audio interruption acceptance

- **Statement:** On the S24 a phone call, an alarm and another media app each interrupt game audio and the game resumes on focus gain with voices in position.
- **Covers:** Z2-I-AUDIO-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** Device run record AUDIO-DR-03 on the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Device runs"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records"

### Z2-R-AUDIO-104 Lifecycle stop and resume

- **Statement:** `OnStopAudio` fades and stops the device within 30 ms and pauses the mixer clock, and `OnResumeAudio` reopens it with voices continuing from their positions.
- **Covers:** Z2-I-AUDIO-12, Z2-I-AUDIO-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_StopResumeAudio` in the `Test` step (win-debug, win-release) and device run record AUDIO-DR-04 on the S24.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Lifecycle on Android"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table"

### Z2-R-AUDIO-105 Output latency measured and reported

- **Statement:** AUDIO measures output latency from AAudio timestamps on Android and the WASAPI stream latency on Windows, plus buffered frames, and reports it in the status, the counter `audio.device.latency_us`, the profiler row and the frame dump.
- **Covers:** Z2-I-AUDIO-12, Z2-I-AUDIO-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDevice_LatencyReported` in the `Test` step (win-debug, win-release) and device run record AUDIO-DR-01.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency measurement"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency reporting"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency budget composition"

### Z2-R-AUDIO-106 No allocation on the device thread

- **Statement:** Code reachable from the mixer callback never allocates or frees memory, enforced by the Debug no-allocation scope around every callback.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRealTime_NoAllocationInCallback` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread"

### Z2-R-AUDIO-107 No locks waits logs or hooks on the device thread

- **Statement:** Code reachable from the mixer callback takes no lock, waits on nothing, never sleeps or yields, logs nothing, calls no profiler hook, performs no I/O and makes no system call except the monotonic clock read and at most one `Zenith_GetCurrentThreadID` per device thread, enforced by the include and identifier whitelist scan (which admits exactly the listed ZenithCore and ZenithJobs headers).
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRealTime_SourceSetIncludeWhitelist` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time source set enforcement"; `Zenith2/Docs/Design/AUDIO.md` → "Real-time source set"

### Z2-R-AUDIO-108 Lock-free channels and complete snapshots

- **Statement:** Control and device exchange data only through CORE's `Zenith_TripleBuffer` (the complete mixer snapshot, dropped snapshots losing nothing) and `Zenith_SpscRing` (feedback, stream rings, capture, the main and focus control rings, each with one producer and one consumer at a time) plus the slot-state table and epoch atomics; the multi-producer backend notifications and loader hand-off use CORE's `Zenith_BoundedQueue`; the main thread reaches `Audio.Mix` only through the main request ring and the main-state triple buffer; and every owner publishes its queue's `.full`, `.dropped` and `.high_water`.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRtChannels_StressSnapshotAndFeedback` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`; CORE's `linux-tsan` stress tests of the primitives.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer snapshot layout"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record"

### Z2-R-AUDIO-109 Sample memory retirement by released epoch

- **Statement:** Memory the device may read is freed only when its retirement epoch E (the first snapshot built without it) is at or below the device's released epoch R = min(A, min over fading slots of L), L being the epoch of the last applied snapshot that listed the fading voice, so no sample buffer, cache entry, stream ring or layout is freed while a live or fading device voice reads it, even when the triple buffer supersedes snapshots.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRetirement_FreesOnlyAfterEpochAcknowledged`, `Test_AudioRetirement_NothingFreedDuringFade`, `Test_AudioRetirement_SupersededSnapshotKeepsFadingMemory`, `Test_AudioRetirement_FullListDefersRelease` and `Test_AudioSoundLoader_UnloadRetiresAfterEpoch` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"

### Z2-R-AUDIO-110 Floating-point environment on AUDIO threads

- **Statement:** Every AUDIO thread runs with the floating-point environment of [Z2-N-CONTRACT-360]: the control, decode and pacer threads apply it at start, and the mixer callback applies it whenever `Zenith_IsExpectedFloatEnvironment()` reports a changed thread.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRealTime_FloatEnvironmentAppliedOnNewThread` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Floating-point setup on the device thread"

### Z2-R-AUDIO-111 Snapshot application is bounds-safe

- **Statement:** The device clamps every index and count it reads from a snapshot, so no snapshot content can cause an out-of-bounds access on the device thread.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRealTime_SnapshotApplyClampsIndices` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan` and the `Zenith_Fuzz_MixerSnapshot` corpus run.
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread"

### Z2-R-AUDIO-112 Mixer callback timing instrument and its one threshold

- **Statement:** The mixer callback measures its share, the callback duration divided by the duration of the audio it rendered (blocks × 256 / 48,000 s, or the delivered frames when it rendered none), and counts an overrun exactly when the share exceeds `audio.mixer.overrunSharePercent`, the same threshold as the Z2-B-AUDIO-003 breach; the per-frame and run maxima and the overrun count are published as counters with relaxed atomics only.
- **Covers:** Z2-I-AUDIO-14, Z2-I-AUDIO-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-AUDIO-003
- **Acceptance evidence:** `Test_AudioMixer_OverrunCounted` in the `Test` step (win-debug, win-release); `Test_AudioMixer_CallbackShareWithShortBursts` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Measuring the mixer callback row"; `Zenith2/Docs/Design/AUDIO.md` → "Counters"

### Z2-R-AUDIO-113 Thirty-minute S24 soak without overruns

- **Statement:** A 30-minute S24 run at the slice's voice load has zero mixer callback overruns and zero stream underruns.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AUDIO-003
- **Acceptance evidence:** Device run record AUDIO-DR-06 on the Stage 6 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records"; `Zenith2/Docs/Design/AUDIO.md` → "Stage 6"

### Z2-R-AUDIO-114 Offline renderer

- **Statement:** `Zenith_AudioOfflineRenderer` drives the same mixer from the caller's thread with no device thread, rendering exactly 48,000 × seconds frames per simulated duration with remainder carry.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioOffline_FramesPerTickExact` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Offline backend"

### Z2-R-AUDIO-115 Bit-exact offline mixing

- **Statement:** Two offline renders of one scenario on one build and platform, including streamed music and dialogue, on-demand loads, cache fills and a locale variant switch, produce bit-identical output whatever the asset reader's timing and the job worker count.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioOffline_BitExactAcrossRuns` (stream reads through the scripted read source, jittered from a different seed per run; loads through the real `Zenith_AssetLoader`) and `Test_AudioOffline_BitExactAcrossWorkerCounts` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer arithmetic rules"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource"

### Z2-R-AUDIO-116 Device path matches the offline path

- **Statement:** The real-time path's output equals the offline renderer's output for the same snapshot consumption log, replayed one block per call with every stream voice rewritten to the test-only replay feed (voice source kind 5), which serves the available frames, then silence for a recorded underrun, and skips the missing frames once.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioOffline_MatchesDevicePathGivenConsumptionLog` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Consumption log record"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Test access"

### Z2-R-AUDIO-117 Audio is presentation only

- **Statement:** No simulation-visible value depends on the device, voice selection, mixer timing or meters: scripts cannot query audio state, notifications are recorded publications, and replays drain identical audio command sequences.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDeterminism_ReplayProducesSameInstances` and `Test_AudioIntegration_ReplayIdenticalCommands` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Determinism contract"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"

### Z2-R-AUDIO-118 Cross-platform mixer hash comparison

- **Statement:** The Stage 3 cross-platform comparison renders the bit-exact scenario on Windows x64 and the S24 and records whether the hashes match in `Zenith2/Docs/Research/Audio/CrossPlatformHash.md`, gating nothing.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** research
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** The committed report `Zenith2/Docs/Research/Audio/CrossPlatformHash.md`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer arithmetic rules"

### Z2-R-AUDIO-119 Zenith.Audio Luau namespace at Stage 3

- **Statement:** AUDIO registers the Stage 3 rows of the `Zenith.Audio` function table, the `AudioEvent`, `AudioInstance` and `AudioLine` value types and their argument validation through `Zenith_RegisterAudioScriptBindings`, with no dependency on ZenithScript, and returns deterministic instance IDs `NextRandomDraw() | 1`. The Stage 4 rows are Z2-R-AUDIO-175.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioScript_PostReturnsDeterministicInstance` and `Test_AudioScript_ConsumerRegisteredBeforeSeal` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Script value types"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Binding registration"

### Z2-R-AUDIO-120 Script audio triggers are transactional

- **Statement:** Every buffered `Zenith.Audio` call appends one audio-trigger side effect that reaches AUDIO only when its entity transaction commits, delivered through `Zenith_AudioScriptConsumer` in merge order.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioScript_RollbackDropsTrigger` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script trigger payloads"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Script side-effect payloads"

### Z2-R-AUDIO-121 Audio trigger capacity enforced at the merge

- **Statement:** Each trigger demands one slot of `SCRIPT_CAPACITY_CLASS_AUDIO_TRIGGERS`, so an entity exceeding the per-tick capacity is rejected at the merge point (BR-MERGE) and its triggers never reach the consumer.
- **Covers:** Z2-I-AUDIO-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** Z2-B-SCRIPT-023
- **Acceptance evidence:** `Test_AudioScript_TriggerCapacityRejectsAtMerge` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Script side-effect payloads"; `Zenith2/Docs/Design/AUDIO.md` → "Budget breaches"

### Z2-R-AUDIO-122 Audio-trigger side-effect consumer registration

- **Statement:** AUDIO registers `Zenith_AudioScriptConsumer` for `SIDE_EFFECT_KIND_AUDIO_TRIGGER` through `Zenith_ModuleStartContext::SetScriptSideEffectConsumer` before the binding table is sealed, and the consumer keeps per-world state keyed by world tag and cannot fail.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioScript_ConsumerRegisteredBeforeSeal` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Binding registration"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"

### Z2-R-AUDIO-123 Stage 3 script misuse raises catalogued errors

- **Statement:** Script argument kinds and counts are SCRIPT's dispatcher's errors; the Stage 3 bindings' thunks raise the catalogued misuse codes ZS2201–ZS2207 of AUDIO's block ZS2200–ZS2299, reserved in `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges", for out-of-range values, empty handles, invalid paths and names and non-finite numbers, as controlled Luau errors, never asserts. The Stage 4 bindings' codes and ZS2208 are Z2-R-AUDIO-175.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioScript_MisuseCodesRaised` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes"; `Zenith2/Docs/Design/AUDIO.md` → "Luau errors versus asserts"

### Z2-R-AUDIO-124 Script audio notifications

- **Statement:** `WhenFinished`, `WhenMarker`, `WhenCaption` and `WhenDeviceChanged` record `SIDE_EFFECT_KIND_ASYNC_REQUEST` requests of source `ASYNC_SOURCE_AUDIO` (value 10 from SCRIPT's registry, never declared by AUDIO) whose completion IDs SCRIPT assigns at commit; the runtime's multiplexer routes them to AUDIO's submitter, AUDIO parks each 16-byte result as `PUBLICATION_KIND_GENERIC_RECORDED` through the single module publication sink, and the result publishes at a tick boundary in completion-ID order, decoded by AUDIO's registered decoder; replays publish the recorded payloads and offline runs complete on the rendered clock.
- **Covers:** Z2-I-AUDIO-10, Z2-I-AUDIO-17, Z2-I-AUDIO-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_AudioScript_NotificationsPublishAtTickBoundary` in the `Test` step (win-debug, win-release) and `Test_AudioIntegration_DoorScriptPostsAndChains` in the integration suite; `Test_AudioScript_NotificationTickExactOffline` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script examples"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"

### Z2-R-AUDIO-125 Data-driven event bridges

- **Statement:** The mixer document's bridges map ECS event types read by stable name to audio posts (action 0), global parameters from a numeric property with the documented conversions (action 1) or global parameters set to a constant (action 2); `Audio.EventBridge` is registered at every attach in phase 9 without a module edge ([Z2-N-ARCH-074]), and a live layout edit that changes the bridged type set (`m_uDetail` 1) or a bridge's match, value, time or entity property name (`m_uDetail` 2) is refused while worlds are attached.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBridge_PostsMatchedEvents` and `Test_AudioBridge_UnknownTypeDeactivates` in the `Test` step (win-debug, win-release); `Test_AudioBridges_LiveEditChangingTypesRefused` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bridge declarations"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bridge system"

### Z2-R-AUDIO-126 Weather ambience through effect prefabs

- **Statement:** Weather audio needs no module edge: ENV's weather effect prefabs carry `Zenith.Audio.Emitter` components played like any emitter, and constant-value bridges keyed on the weather-change event's `ToNameHash` set the `Rain`, `Snow` and `Wind` global parameters with a ramp (value bridges reading the event's intensities once ENV adds them, pending ENV).
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioIntegration_WeatherPrefabAmbience` in the integration suite; `Test_AudioBridges_ConstantValueWeatherBridge` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Engine event types bridged"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"

### Z2-R-AUDIO-127 Region-bound emitters

- **Statement:** An emitter whose `Zenith.Streaming.RegionBound` flag `InActiveRegion` is false keeps its instance with every voice held virtual and resumes in place when the flag becomes true, and zones, portals and occluders whose flag is false are ignored, the flag read by stable name.
- **Covers:** Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioEmitter_RegionBoundInactive` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Region-bound emitters"

### Z2-R-AUDIO-128 Cell unload fades without freeing playing memory

- **Statement:** When a streamed cell unloads, its emitters' instances stop with their fades and no sample memory is freed while a voice still plays it.
- **Covers:** Z2-I-STREAM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioIntegration_StreamedCellUnloadFades` in the integration suite.
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Cells unloading under playing voices"

### Z2-R-AUDIO-129 Origin shifts cause no Doppler

- **Statement:** A world-origin shift resets every velocity estimate for that frame, so no pitch change results.
- **Covers:** Z2-I-STREAM-11, Z2-I-AUDIO-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSpatial_OriginShiftNoDoppler` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Origin shifts"

### Z2-R-AUDIO-130 Audio counters and profiler rows

- **Statement:** `Audio.Mix` publishes every counter of the counter table (`uAUDIO_COUNTER_COUNT` entries, including the `.full`, `.dropped` and `.high_water` triple of every queue AUDIO owns) once per frame, never from the device thread, and contributes the "Audio" row group to DEVT's on-screen profiler.
- **Covers:** Z2-I-AUDIO-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** Z2-B-AUDIO-002, Z2-B-AUDIO-004
- **Acceptance evidence:** `Test_AudioCounters_PublishedPerFrame` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Counters"; `Zenith2/Docs/Design/AUDIO.md` → "On-screen profiler rows"; `Zenith2/Docs/Design/AUDIO.md` → "Telemetry"

### Z2-R-AUDIO-131 Audio log categories

- **Statement:** AUDIO logs only from control-side threads in its five `audio.*` categories, repeated warnings of one kind logging once per ID per session and counting thereafter.
- **Covers:** Z2-I-AUDIO-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDiagnostics_WarnOncePerId` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Log categories"

### Z2-R-AUDIO-132 Debug views frame dump and capture

- **Statement:** Development builds draw the audio debug views through DEVT's `Zenith_DebugDraw`, the frame dump carries an `audio` member, and the development capture writes the final output to a WAV file on the user mount from a ring allocated only when capture is enabled.
- **Covers:** Z2-I-AUDIO-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDiagnostics_FrameDumpMember` and `Test_AudioCapture_WritesWav` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Debug views"; `Zenith2/Docs/Design/AUDIO.md` → "Frame-dump and capture entries"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Capture file"

### Z2-R-AUDIO-133 Meters and voice inspector

- **Statement:** `Zenith_AudioSystem` provides per-bus peak and RMS meters (published by the device in meter frames through their own triple buffer, Z2-R-AUDIO-165), master momentary and short-term loudness and the voice inspector listing every live voice.
- **Covers:** Z2-I-AUDIO-15, Z2-I-AUDIO-17, Z2-I-AUDIO-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_MetersPeakRms` and `Test_AudioSystem_VoiceInspectorListsVoices` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Status and meters"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Profiler and voice inspector"

### Z2-R-AUDIO-134 Audio cook step registration and validators

- **Statement:** `Zenith_RegisterAudioCookSteps` registers `audio.sound`, `audio.bank` and `audio.eventIndex`, the kind-10 and kind-11 validators, and the bank-memory and duration content rules. The validators call ZenithAudio's public `Zenith_AudioValidateSoundFile` and `Zenith_AudioValidateBankFile`, which wrap the parser the loaders run and report the loaders' check numbers, so ZenithCook includes no `Internal/` header of ZenithAudio.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_ValidatorMatchesLoader` and `Test_AudioCook_ContentRulesMeasureBanks` in the `Test` step; `Test_AudioFileValidate_MatchesLoaderChecks` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Step registration"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Cooked file validation"

### Z2-R-AUDIO-135 Importer intermediate contract

- **Statement:** The `audio.sound` step reads CONTENT's `decoded-audio` intermediate in the layout AUDIO specifies and the import settings keys of the design, rejecting an invalid intermediate with an error naming the asset and field.
- **Covers:** Z2-I-AUDIO-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_SoundDeterministic` (on generated intermediates) in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio importer boundary"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound import settings"

### Z2-R-AUDIO-136 Deterministic sound cook

- **Statement:** `audio.sound` downmixes, resamples, trims, measures, normalises, dithers with a GUID-seeded generator and encodes so that one intermediate and one settings set always cook to identical bytes, publishing the properties banks read.
- **Covers:** Z2-I-AUDIO-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_SoundDeterministic` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.sound step"

### Z2-R-AUDIO-137 Cook resampling loudness and normalisation

- **Statement:** The cook resamples with a 64-tap, 256-phase Kaiser-windowed sinc of at least 90 dB stopband attenuation, measures BS.1770-4 integrated loudness and true peak and normalises to the target without the true peak exceeding −1 dBTP.
- **Covers:** Z2-I-AUDIO-21, Z2-I-AUDIO-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_ResamplerStopband`, `Test_AudioCook_LoudnessOfReferenceSine` and `Test_AudioCook_NormaliseRespectsTruePeak` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook-time resampling"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Loudness measurement"

### Z2-R-AUDIO-138 Bank cook from documents

- **Statement:** `audio.bank` opens the bank document (and, for the master bank, the mixer document) through `OpenSourceDocument`, flattens node trees, reads sound properties as cook dependencies, declares hard and soft references and writes the bank file.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_BankFlattensNodes` and `Test_AudioCook_MasterBankEmbedsMixer` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"

### Z2-R-AUDIO-139 Cook validation rules and the event index

- **Statement:** The audio cook enforces every validation rule of the design, including event paths unique across the game's banks except locale variants, and writes `Audio/events.zevi` for development event names.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_ValidationRules` and `Test_AudioCook_DuplicateEventAcrossBanksFails` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook validation rules"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.eventIndex step"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook error codes"

### Z2-R-AUDIO-140 Bank and mixer document types

- **Statement:** `Zenith.Audio.BankDocument` and `Zenith.Audio.MixerDocument` are registered by `Zenith_RegisterAudioDocumentTypes`, declared in `ZenithAuthoring`'s `Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h` and called by ZenithCook and ZenithEditor (ruling Q84 (a)), at version 1 with the schemas of the design, preserving unknown members and migrating one step per version bump.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22, Z2-I-AUDIO-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDocuments_RegisteredAndMigrate` (`ZenithAuthoring_Tests`) in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types"; `Zenith2/Docs/Design/AUDIO.md` → "ZenithAuthoring AUDIO files"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Document migrations"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank document schema"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer document schema"

### Z2-R-AUDIO-141 Bank and event editor

- **Statement:** The editor's bank and event panel edits events, node trees, curves, music and dialogue tables and sound import settings through the document framework with undo and journaled save.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioEditor_BankEditUndoRoundTrip` in the `Test` step and the workflow walkthrough.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bank and event editor"

### Z2-R-AUDIO-142 Mixer console with live changes

- **Statement:** The mixer console edits the mixer document and applies each change to the running mix within 100 ms through `ApplyMixerLayout`, with mute and solo as bus overrides.
- **Covers:** Z2-I-AUDIO-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_ApplyMixerLayoutLive` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Mixer console"

### Z2-R-AUDIO-143 Audition panel

- **Statement:** The audition panel plays events, sounds and dialogue lines on the `Audition` bus with live parameters and a positional pad, with no game world and from unsaved edits cooked in process by `Zenith_AudioCookBankPreview` into audition-only preview banks (Z2-R-AUDIO-168).
- **Covers:** Z2-I-AUDIO-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_AuditionWithoutWorld` and `Test_AudioSystem_MainChannelsOrderAndBounds` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition panel"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"

### Z2-R-AUDIO-144 Audition in context

- **Statement:** In play-in-editor an audition whose desc names the running world (`m_uContextWorldTag`) and whose position the panel writes each frame through `SetAuditionPosition` is spatialised against that world's listener in every frame the world was updated, and against the virtual origin listener otherwise (after the world detaches), routed to `Audition` and kept out of game logic, beside the live mixer and voice inspector.
- **Covers:** Z2-I-AUDIO-24, Z2-I-AUDIO-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_AuditionInContextRoutesToAuditionBus` in the `Test` step (win-debug, win-release) and the workflow walkthrough.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition in context"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels"

### Z2-R-AUDIO-145 Complete audio workflow walkthrough

- **Statement:** The scripted walkthrough of import, bank and event authoring, mixing and audition in context runs inside ZenithEditor with generated source files, its elapsed time meets Z2-B-AUDIO-005, and its usability observations are recorded.
- **Covers:** Z2-I-AUDIO-21, Z2-I-AUDIO-22, Z2-I-AUDIO-23, Z2-I-AUDIO-24, Z2-I-AUDIO-25
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-AUDIO-005
- **Acceptance evidence:** The MP-WALKTHROUGH evidence record and usability notes on the Stage 4 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Workflow steps"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Workflow walkthrough script"

### Z2-R-AUDIO-146 Stage 1 AUDIO budget ruling

- **Statement:** The `needs-human` Stage 1 AUDIO budget ruling allocates Z2-B-AUDIO-001 to Z2-B-AUDIO-005 and the seven pending rows with every Budgets column, reconciles the memory row with ContentSpecs, writes the adopted real-voice values and `audio.mixer.overrunSharePercent` into AUDIO's fragments citing the rows, and appends its ruling to the DecisionLog in its own diff.
- **Covers:** Z2-I-CORE-21, Z2-I-AUDIO-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-097, Z2-D-012
- **Budgets:** Z2-B-AUDIO-001, Z2-B-AUDIO-002, Z2-B-AUDIO-003, Z2-B-AUDIO-004, Z2-B-AUDIO-005
- **Acceptance evidence:** `Zenith2/Docs/Research/Budgets/Stage1/AUDIO/allocation.json`, its report and the DecisionLog ruling, on the ruling ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "The Stage 1 AUDIO budget ruling"; `Zenith2/Docs/Design/AUDIO.md` → "Pending new rows"

### Z2-R-AUDIO-147 Slice budget runs meet the AUDIO rows

- **Statement:** In the flagship slices' budget runs on Windows and the S24, audio resident memory stays within Z2-B-AUDIO-001, `Audio.Update` CPU summed over worlds within Z2-B-AUDIO-002, no mixer callback exceeds Z2-B-AUDIO-003's share of its period, and the peak real voice count stays within Z2-B-AUDIO-004.
- **Covers:** Z2-I-CORE-21, Z2-I-AUDIO-03, Z2-I-AUDIO-05, Z2-I-AUDIO-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AUDIO-001, Z2-B-AUDIO-002, Z2-B-AUDIO-003, Z2-B-AUDIO-004
- **Acceptance evidence:** The MP-SUBMEM, MP-CPUZONE and MP-COUNTER evidence records of the WS-SLICE budget runs (`audio.memory.bytes`, `audio.update.us_delta`, `audio.mixer.callback_share_run_max`, `audio.mixer.overruns`, `audio.voices.real_peak`) on the stage acceptance ticket, with `Zenith_Bench_AudioMixer`.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Rows AUDIO owns"; `Zenith2/Docs/Design/AUDIO.md` → "Measuring the mixer callback row"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Benchmarks"

### Z2-R-AUDIO-148 Package size with the audio codec

- **Statement:** The codec library and cooked audio of a shipping build fit the package rows Z2-B-PACK-001 measures.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02
- **Stage:** S6
- **Classification:** required
- **Decisions:** D-SEL-CODEC (open, Stage 2)
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** The Stage 6 package size report on the acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Rows AUDIO is measured against"; `Zenith2/Docs/Design/AUDIO.md` → "Stage 6"

### Z2-R-AUDIO-149 S24 memory-pressure run with audio shedding

- **Statement:** The S24 memory-pressure device run shows AUDIO's WARNING and CRITICAL shedding in its counters with continued playback.
- **Covers:** Z2-I-CORE-21
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-AUDIO-001
- **Acceptance evidence:** Device run record AUDIO-DR-07 on the Stage 6 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding"

### Z2-R-AUDIO-150 Loader hand-off to Audio.Mix

- **Statement:** Kind loaders running concurrently on ASSET's slices hand activated and unloaded records to `Audio.Mix` only through CORE's lock-free `Zenith_BoundedQueue` of 65,536 entries with records from fixed pools with lock-free free lists, so at most 33,280 events are ever outstanding, the queue never fills, and no loader touches AUDIO's control state.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-02, Z2-I-AUDIO-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBankLoader_ConcurrentActivationsHandOff` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Loader hand-off"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"

### Z2-R-AUDIO-151 Event index published by pointer swap

- **Statement:** `Audio.Mix` publishes each rebuilt event index with a release store of its pointer, readers load it once with acquire per call or node, and a replaced index is freed only at the second `Audio.Mix` after the swap, so `FindEvent` from AUDIO's epilogue nodes and from the main thread outside the EPILOGUE subgraph never reads freed memory.
- **Covers:** Z2-I-AUDIO-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioEventIndex_ReaderKeepsOldIndexUntilRetired` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Event and parameter lookup"

### Z2-R-AUDIO-152 Dropped feedback is reconciled

- **Statement:** A voice start or end whose feedback record a full ring dropped is recovered in `Audio.Mix` from the device's slot-state table (generation and state per slot): a slot playing, fading or ended for the assigned generation acknowledges the start, a slot ended or free ends the voice; each is counted in `audio.feedback.reconciled_delta`, so no slot leaks, no start flag stays set and no voice is reported ended twice.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRetirement_ReconcilesDroppedEndRecord`, `Test_AudioRetirement_ReconcilesDroppedStartRecord` and `Test_AudioRtChannels_StressSnapshotAndFeedback` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels"

### Z2-R-AUDIO-153 Audition voices are reserved

- **Statement:** The audition world, advanced inside `Audio.Mix` from the main request ring and the main-state triple buffer, owns 16 of the 256 device voice slots, so auditions play with no game world attached and beside a running game without displacing or being displaced by game voices.
- **Covers:** Z2-I-AUDIO-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_AuditionSlotsReserved` and `Test_AudioSystem_AuditionWithoutWorld` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"

### Z2-R-AUDIO-154 Subtitle and caption keys reach UI's localization extraction

- **Statement:** The `audio.bank` cook step adds every dialogue line's subtitle key and every event caption key to UI's extraction through `Zenith_UIExtractionSink` with the bank document's path, the JSON line and an `audio.line:` or `audio.event:` context, and adds nothing when no UI module is selected.
- **Covers:** Z2-I-AUDIO-10, Z2-I-AUDIO-11, Z2-I-UI-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_BankContributesSubtitleKeys` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions"

### Z2-R-AUDIO-155 Released epoch follows the last applied listing snapshot

- **Statement:** Each device voice slot records L, the epoch of the last snapshot the device applied that listed it; the device stores R = A with no fading slot, else min(A, min over fading slots of L), and the worked case of a superseded snapshot (5 lists v, 6 drops it with E = 6, 6 superseded, 7 applied) keeps v's memory allocated (R = 5) until v's fade ends.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRetirement_SupersededSnapshotKeepsFadingMemory` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step"

### Z2-R-AUDIO-156 Main-thread setters reach Audio.Mix through two declared channels

- **Statement:** The main thread's override, layout, user-volume, focus-mute and inspector members reach `Audio.Mix` only through a 64-record `Zenith_SpscRing<Zenith_AudioMainRequest>` that cannot fill (at most 37 requests outstanding, counting the audition requests of Z2-R-AUDIO-176, because `StopAudition` pushes at most once per active audition slot generation and is otherwise a no-op; a failed push asserted) and a `Zenith_TripleBuffer<Zenith_AudioMainState>` of the complete 2,304-byte main-thread state, with acknowledgement atomics back and the counters `audio.main.requests.full`, `.dropped`, `.high_water` and `audio.main.state.dropped`; the main thread builds every live layout, and `Audio.Mix` publishes it in the next snapshot.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_MainChannelsOrderAndBounds` and `Test_AudioSystem_ApplyMixerLayoutLive` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"

### Z2-R-AUDIO-157 Offline servicing completes asynchronous work before every frame

- **Statement:** Under the offline backend no decode thread runs, and `Zenith_AudioOfflineRenderer::ServiceBeforeFrame` completes, before each frame's `Audio.Update` and `Audio.Mix`, the decode requests `Audio.Mix` pushed, every asset load AUDIO requested, every stream slot's reads (waited for through the read source's `ServiceUntilComplete`) and refill and every pending cache fill, answering each through the decode event ring, each wait bounded by 5,000 ms with a timeout reported and counted, so offline output depends on no wall-clock quantity.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioOffline_BitExactAcrossRuns` (the scripted read source jittered per run) and `Test_AudioOffline_ServiceTimeoutReported` (a read the scripted read source withholds) in the `Test` step (win-debug, win-release); the production read wait is `Zenith_AssetReader::ServiceUntilComplete` (row AU26, Z2T-ASSET-029), exercised by `Test_AudioStream_AssetReadSourceReadsCookedFile`.
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm"

### Z2-R-AUDIO-158 Stream rings hold whole frames for every channel count

- **Statement:** A stream ring is a `Zenith_SpscRing<sint16>` of N samples, N the smallest power of two at least `audio.streams.ringFrames` × channels, pushed and popped in whole frames only, holding at most ⌊N / channels⌋ frames, so a stream of any admitted channel count (1, 2, 4, 6 or 8) opens without a capacity check failure.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_SurroundRingPowerOfTwo` and `Test_AudioStream_StateMachine` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels"

### Z2-R-AUDIO-159 Audio follows the presenting world through world transitions

- **Statement:** Because a transition's target world attaches while the old world is live but runs no `Audio.Update` until the swap, and the swap writes its higher `SessionOrdinal` before the old world detaches, `Audio.Mix` mixes only the old world's voices before the swap frame and only the new world's from it; a failed transition leaves the old world primary, and a world attached beyond the eighth is enabled at the next detach of an enabled world.
- **Covers:** Z2-I-AUDIO-03, Z2-I-AUDIO-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_FollowsWorldTransition` and `Test_AudioSystem_NinthWorldDisabled` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Following the presenting world"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners"

### Z2-R-AUDIO-160 Audio.Mix runs with no world attached

- **Statement:** `Audio.Mix` runs every frame with no world attached, headless included, so the audition, the feedback drain and slot reconciliation, the loader hand-off and event index rebuild, the snapshot publication and retirement never depend on a world.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_AudioMixAfterEveryWorldUpdate` (no world attached) and `Test_AudioSystem_AuditionWithoutWorld` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Frame-graph placement"

### Z2-R-AUDIO-161 Cooked enumeration fields hold every enumerator

- **Statement:** The event record's steal mode and virtual behaviour are 3-bit fields holding every enumerator value 1 to 4 as is, and BNK5 rejects 0 and 5 to 7, so no enumerator of either enumeration is unstorable in the permanent bank format.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioCook_EventEnumFieldsRoundTrip` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Event table section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks"

### Z2-R-AUDIO-162 The device reads a valid snapshot before the first Audio.Mix

- **Statement:** `Zenith_AudioSystem::Create` writes the empty snapshot (epoch 0, no voice, the built-in layout) into all three slots of the snapshot triple buffer, and the device treats epoch 0 as no snapshot, so blocks rendered before the first `Audio.Mix` start nothing, dereference a valid layout and output silence.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_FirstBlockBeforeFirstMix` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Creation steps"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step"

### Z2-R-AUDIO-163 Audio.Mix and the decode thread exchange stream and cache work through two declared rings

- **Statement:** `Audio.Mix` and the decode thread share stream slots and cache fills only through a 256-record `Zenith_SpscRing<Zenith_AudioDecodeRequest>` (OPEN, SEEK, STARVE, CLOSE, CACHE_FILL; at most 3 outstanding per slot and 32 cache fills, so at most 128 records) and a 512-record `Zenith_SpscRing<Zenith_AudioDecodeEvent>` (at most 256 unread), neither of which can fill (a failed push asserted), with the `.full`, `.dropped` and `.high_water` counters of both; `Audio.Mix` owns every slot's state and allocates, retires and releases every stream block and handle, and the decode thread keeps only private refill state and frees nothing.
- **Covers:** Z2-I-AUDIO-01, Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_MixDecodeChannelStress` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`; `Test_AudioCounters_PublishedPerFrame` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Decode thread loop"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"

### Z2-R-AUDIO-164 Stream reads go through the AUDIO read seam

- **Statement:** Every stream read is submitted, polled, cancelled, released and (offline) waited for through the AUDIO-internal interface `Zenith_AudioReadSource`, whose production implementation forwards to `Zenith_AssetReader` (`ReadRange`, `GetError`, `ServiceUntilComplete`), so tests substitute only AUDIO's own interface (FINAL-DOUBLES) while asset loads keep using the real `Zenith_AssetLoader`.
- **Covers:** Z2-I-AUDIO-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioStream_AssetReadSourceReadsCookedFile` and `Test_AudioStream_ChunkLargerThanRingDecodesIncrementally` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Test access"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Scripted read source"

### Z2-R-AUDIO-165 Bus meters never occupy the feedback ring

- **Statement:** The device publishes bus meters every 4th block in a 1,040-byte meter frame through a `Zenith_TripleBuffer<Zenith_AudioMeterFrame>` that `Audio.Mix` acquires in M1, counting superseded frames in `audio.meters.dropped`, and pushes only voice, underrun, marker and clock-sample records into the feedback ring, so a 300 ms main-thread stall drops no marker record.
- **Covers:** Z2-I-AUDIO-14, Z2-I-AUDIO-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRtChannels_MeterFloodKeepsMarkers` and `Test_AudioSystem_MetersPeakRms` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Formats.md` → "Meter frame record"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Feedback record"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Status and meters"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels"

### Z2-R-AUDIO-166 The effective prebuffer fits the guaranteed ring fill

- **Statement:** `audio.streams.ringFrames` is at least 16,384 and `Zenith_AudioReadSettings` clamps the effective prebuffer to min(`audio.streams.prebufferFrames`, `audio.streams.ringFrames` − 8,192), logging one warning when it clamps, while `Create` rejects a hand-built desc whose prebuffer exceeds the ring minus 8,192 with `AUDIO_ERROR_INVALID_DESC`, so every prebuffer is reachable whatever the sound's packet frames.
- **Covers:** Z2-I-AUDIO-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioConfig_PrebufferClampedToRingFill` and `Test_AudioConfig_KeysPresentationOnly` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSettings"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks"

### Z2-R-AUDIO-167 A full retire list defers and never frees early

- **Statement:** When the 4,096-entry retire list is full, the control side keeps the memory it was about to retire referenced by its owner record, never reuses that owner, counts `audio.retire.deferred` and retries the append in the next `Audio.Mix`'s M9, so nothing is freed before its epoch and nothing leaks.
- **Covers:** Z2-I-AUDIO-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioRetirement_FullListDefersRelease` in the `Test` step (win-debug, win-release) and `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"; `Zenith2/Docs/Design/AUDIO.md` → "Counters"

### Z2-R-AUDIO-168 Preview banks stay out of the event index

- **Statement:** A bank whose header carries the audition-only bit (written only by `Zenith_AudioCookBankPreview`) never joins the bank list or the process event index, builds no layout and requests no locale variant; auditions reach its events by record index, so a preview never duplicates, shadows or warns about the events of the bank it previews.
- **Covers:** Z2-I-AUDIO-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_AuditionWithoutWorld` (the preview case: no `audio.events.duplicate_delta`) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank header section"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition panel"

### Z2-R-AUDIO-169 The editor preview cook equals the cook

- **Statement:** `ZenithAuthoring` provides `Zenith_AudioWriteBankFile`, `Zenith_AudioWriteMixerSection`, `Zenith_AudioCookBankPreview` and `Zenith_AudioCookMixerSection` in `Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h` (ruling Q84 (a)); `audio.bank` and the editor's preview call the same writers, so a preview's sections equal the cook's byte for byte except the bank header's audition-only bit, and the mixer console's section equals the master bank's mixer section.
- **Covers:** Z2-I-AUDIO-16, Z2-I-AUDIO-17, Z2-I-AUDIO-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioAuthoring_PreviewCookMatchesCook` in the `Test` step.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types"; `Zenith2/Docs/Design/AUDIO.md` → "ZenithAuthoring AUDIO files"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Mixer console"

### Z2-R-AUDIO-170 Stage 4 audio command kinds

- **Statement:** AUDIO registers the six Stage 4 `Audio.*` command kinds (`Audio.SetMixSnapshot`, `Audio.MusicSetState`, `Audio.MusicStinger`, `Audio.PlayDialogue`, `Audio.PrepareDialogue` and `Audio.SeekInstance`), each with the change that implements it, through `Zenith_AudioStage4CommandKindDescs`, with the same 64-byte reflected payloads, phases 1 to 10, untrusted-payload validation, tag-table lookup, drop-and-count rule and `Zenith_AudioRecord*` helpers as the Stage 3 kinds of Z2-R-AUDIO-013, so all twelve kinds are registered from Stage 4.
- **Covers:** Z2-I-AUDIO-05, Z2-I-AUDIO-09, Z2-I-AUDIO-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioMusic_EveryRuleComputesBoundary`, `Test_AudioDialogue_SameSpeakerReplaces`, `Test_AudioMixSnapshot_BlendsOverrides` and `Test_AudioClock_TimeLockedDriftCorrection` in the `Test` step (win-debug, win-release), each posting its kinds through the recording helpers; the `Zenith_Fuzz_TriggerPayload` corpus over all twelve kinds in `Sanitizers -Preset win-asan`.
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command kinds"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command payloads"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"

### Z2-R-AUDIO-171 Zone portal and occluder components registered

- **Statement:** The zone, portal and occluder components (`Zenith.Audio.Zone`, `Zenith.Audio.Portal` and `Zenith.Audio.Occluder`) register in `Zenith_RegisterAudioComponents` under their stable names, version 1, with the fixed property IDs and ranges of the design and authority `PHASE_COMMANDS_ONLY`, serialise by reflection, and are read by `Audio.Update`'s frame building; the ticket that registers them adds their three rows to GAME's authority census in its own diff (GAME change-control rule 3).
- **Covers:** Z2-I-AUDIO-07, Z2-I-AUDIO-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioComponents_RegisteredWithAuthority` (the six-component case) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component conventions"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component registration"

### Z2-R-AUDIO-172 Bank retention cache

- **Statement:** AUDIO's retention cache holds a bank AUDIO itself requested: the active locale variant of an active base bank stays resident while its base is active, and any other bank AUDIO requested stays resident for `audio.banks.retainSeconds` after its last use; while the cache holds a bank, that bank is resident in the sense of Z2-R-AUDIO-017.
- **Covers:** Z2-I-AUDIO-11, Z2-I-AUDIO-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioBankLoader_RetentionExpires` and `Test_AudioBankLoader_LocaleVariantSwitch` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank dependencies and residency"

### Z2-R-AUDIO-173 Quality-tier configuration fragments

- **Statement:** AUDIO's quality-tier configuration fragments `Internal/Config/Quality/<tier>.config.json`, one per tier of the "Audio features" section of `Zenith2/Docs/QualityMatrix.md`, set `audio.reverb.quality`, `audio.voices.realMax` (never above the platform fragment's value) and `audio.occlusion.raysPerFrame` to the tier's values, merged by PLAT's quality layer; a value out of range is rejected per key with the lower layer kept, and the values cite the budget rows they come from.
- **Covers:** Z2-I-AUDIO-03, Z2-I-AUDIO-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AUDIO-004
- **Acceptance evidence:** `Test_AudioConfig_KeysPresentationOnly` (the per-tier merged values) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Internal files"; `Zenith2/Docs/Design/AUDIO.md` → "The Stage 1 AUDIO budget ruling"

### Z2-R-AUDIO-174 Secondary worlds' music commands are ignored

- **Statement:** A secondary world's `Audio.MusicSetState` and `Audio.MusicStinger` commands are ignored and counted, so only the primary world of Z2-R-AUDIO-065 drives the one process music player.
- **Covers:** Z2-I-AUDIO-03, Z2-I-AUDIO-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioVoices_SecondaryWorldShare` (the music case) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Music model"

### Z2-R-AUDIO-175 Stage 4 Zenith.Audio bindings and ZS2208

- **Statement:** AUDIO adds the Stage 4 rows of the `Zenith.Audio` function table to `Zenith_RegisterAudioScriptBindings` (PURE `Line`, `Snapshot` and `MusicState`; BUFFERED `SetMixSnapshot`, `SetMusicState`, `PlayStinger`, `PlayDialogue`, `PrepareDialogue`, `Seek` and `PostTimeLocked`), each BUFFERED row appending one audio trigger and `PlayDialogue` and `PostTimeLocked` returning deterministic instance IDs; their thunks raise ZS2201–ZS2207 as the Stage 3 thunks do, and the notification functions raise ZS2208 for an empty or over-47-byte marker name, as controlled Luau errors, never asserts.
- **Covers:** Z2-I-AUDIO-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioScript_Stage4BindingsRecordTriggers` and the Stage 4 cases of `Test_AudioScript_MisuseCodesRaised` (ZS2208 included) in the `Test` step (win-debug, win-release); `Test_AudioIntegration_Stage4BindingsFromScript` in the integration suite.
- **Design:** `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes"

### Z2-R-AUDIO-176 Audition requests reach Audio.Mix through the main channels

- **Statement:** `Audition`, `StopAudition`, `StopAllAuditions`, `SetAuditionParameter` and `SetAuditionPosition` reach `Audio.Mix` only through the main-thread request ring and main state of Z2-R-AUDIO-156 (the start and stop requests through the ring, parameters and positions through the main state), within that ring's bound of 37 outstanding requests.
- **Covers:** Z2-I-AUDIO-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioSystem_MainChannelsOrderAndBounds` (the audition cases) and `Test_AudioSystem_AuditionWithoutWorld` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"

### Z2-R-AUDIO-177 Recorded effective dialogue locale

- **Statement:** Each audio world holds the serialized resource `Zenith.Audio.DialogueLocale` whose `EffectiveTag` is the game's voice-locale override (`Zenith.Audio.SetVoiceLocale`) when set, else the derived presentation tag (`audio.dialogue.locale`, `ui.locale`, `m_acPreferredLocale`); a change of the derived tag, and a differing one at the first tick after a restore, enters simulation only as a recorded `Zenith.Audio.DialogueLocaleChanged` publication applied at a tick boundary; dialogue resolution, CHAR, CINE and GAME read the effective tag through AUDIO's accessors (ruling GmB33-DIALOGUE-LOCALE).
- **Covers:** Z2-I-AUDIO-10, Z2-I-AUDIO-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AudioDialogueLocale_DerivedChangeIsRecorded`, `Test_AudioDialogueLocale_OverrideWins` and `Test_AudioDialogueLocale_SavedAndRestored` in the `Test` step (win-debug, win-release); `Test_AudioIntegration_DialogueLocaleReplayed` in the integration suite.
- **Design:** `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Recorded dialogue locale"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection"

## Coverage of inventory items

| Item | Name | Stage | Requirements |
|---|---|---|---|
| Z2-I-AUDIO-01 | audio streaming | S3 | Z2-R-AUDIO-003, Z2-R-AUDIO-017, Z2-R-AUDIO-030, Z2-R-AUDIO-034, Z2-R-AUDIO-035, Z2-R-AUDIO-036, Z2-R-AUDIO-037, Z2-R-AUDIO-038, Z2-R-AUDIO-039, Z2-R-AUDIO-040, Z2-R-AUDIO-041, Z2-R-AUDIO-042, Z2-R-AUDIO-148, Z2-R-AUDIO-150, Z2-R-AUDIO-157, Z2-R-AUDIO-158, Z2-R-AUDIO-163, Z2-R-AUDIO-164, Z2-R-AUDIO-166 |
| Z2-I-AUDIO-02 | audio decode | S3 | Z2-R-AUDIO-023, Z2-R-AUDIO-024, Z2-R-AUDIO-027, Z2-R-AUDIO-030, Z2-R-AUDIO-031, Z2-R-AUDIO-032, Z2-R-AUDIO-033, Z2-R-AUDIO-034, Z2-R-AUDIO-035, Z2-R-AUDIO-036, Z2-R-AUDIO-037, Z2-R-AUDIO-148, Z2-R-AUDIO-150 |
| Z2-I-AUDIO-03 | voice budgets | S3 | Z2-R-AUDIO-006, Z2-R-AUDIO-010, Z2-R-AUDIO-012, Z2-R-AUDIO-019, Z2-R-AUDIO-042, Z2-R-AUDIO-044, Z2-R-AUDIO-057, Z2-R-AUDIO-059, Z2-R-AUDIO-060, Z2-R-AUDIO-061, Z2-R-AUDIO-062, Z2-R-AUDIO-063, Z2-R-AUDIO-065, Z2-R-AUDIO-121, Z2-R-AUDIO-146, Z2-R-AUDIO-147, Z2-R-AUDIO-159, Z2-R-AUDIO-173, Z2-R-AUDIO-174 |
| Z2-I-AUDIO-04 | buses | S3 | Z2-R-AUDIO-018, Z2-R-AUDIO-029, Z2-R-AUDIO-046, Z2-R-AUDIO-047, Z2-R-AUDIO-048, Z2-R-AUDIO-049, Z2-R-AUDIO-050 |
| Z2-I-AUDIO-05 | mixing | S3 | Z2-R-AUDIO-001, Z2-R-AUDIO-003, Z2-R-AUDIO-006, Z2-R-AUDIO-007, Z2-R-AUDIO-009, Z2-R-AUDIO-011, Z2-R-AUDIO-013, Z2-R-AUDIO-014, Z2-R-AUDIO-016, Z2-R-AUDIO-018, Z2-R-AUDIO-021, Z2-R-AUDIO-046, Z2-R-AUDIO-047, Z2-R-AUDIO-051, Z2-R-AUDIO-052, Z2-R-AUDIO-053, Z2-R-AUDIO-054, Z2-R-AUDIO-055, Z2-R-AUDIO-056, Z2-R-AUDIO-114, Z2-R-AUDIO-115, Z2-R-AUDIO-116, Z2-R-AUDIO-117, Z2-R-AUDIO-118, Z2-R-AUDIO-119, Z2-R-AUDIO-120, Z2-R-AUDIO-122, Z2-R-AUDIO-123, Z2-R-AUDIO-126, Z2-R-AUDIO-147, Z2-R-AUDIO-156, Z2-R-AUDIO-157, Z2-R-AUDIO-159, Z2-R-AUDIO-160, Z2-R-AUDIO-170, Z2-R-AUDIO-175 |
| Z2-I-AUDIO-06 | spatialization | S3 | Z2-R-AUDIO-014, Z2-R-AUDIO-015, Z2-R-AUDIO-051, Z2-R-AUDIO-066, Z2-R-AUDIO-067, Z2-R-AUDIO-068, Z2-R-AUDIO-069, Z2-R-AUDIO-070, Z2-R-AUDIO-071, Z2-R-AUDIO-072, Z2-R-AUDIO-129 |
| Z2-I-AUDIO-07 | occlusion | S4 | Z2-R-AUDIO-073, Z2-R-AUDIO-074, Z2-R-AUDIO-078, Z2-R-AUDIO-171 |
| Z2-I-AUDIO-08 | reverb | S4 | Z2-R-AUDIO-019, Z2-R-AUDIO-075, Z2-R-AUDIO-076, Z2-R-AUDIO-079, Z2-R-AUDIO-173 |
| Z2-I-AUDIO-09 | music transitions | S4 | Z2-R-AUDIO-034, Z2-R-AUDIO-080, Z2-R-AUDIO-081, Z2-R-AUDIO-082, Z2-R-AUDIO-170, Z2-R-AUDIO-174 |
| Z2-I-AUDIO-10 | dialogue audio | S4 | Z2-R-AUDIO-049, Z2-R-AUDIO-083, Z2-R-AUDIO-086, Z2-R-AUDIO-087, Z2-R-AUDIO-124, Z2-R-AUDIO-154, Z2-R-AUDIO-170, Z2-R-AUDIO-177 |
| Z2-I-AUDIO-11 | localized audio | S4 | Z2-R-AUDIO-084, Z2-R-AUDIO-085, Z2-R-AUDIO-154, Z2-R-AUDIO-172, Z2-R-AUDIO-177 |
| Z2-I-AUDIO-12 | audio device changes | S3 | Z2-R-AUDIO-002, Z2-R-AUDIO-005, Z2-R-AUDIO-018, Z2-R-AUDIO-092, Z2-R-AUDIO-093, Z2-R-AUDIO-094, Z2-R-AUDIO-095, Z2-R-AUDIO-096, Z2-R-AUDIO-097, Z2-R-AUDIO-098, Z2-R-AUDIO-099, Z2-R-AUDIO-100, Z2-R-AUDIO-104, Z2-R-AUDIO-105 |
| Z2-I-AUDIO-13 | Android audio focus | S3 | Z2-R-AUDIO-101, Z2-R-AUDIO-102, Z2-R-AUDIO-103, Z2-R-AUDIO-104 |
| Z2-I-AUDIO-14 | real-time audio thread constraints | S3 | Z2-R-AUDIO-001, Z2-R-AUDIO-002, Z2-R-AUDIO-004, Z2-R-AUDIO-008, Z2-R-AUDIO-009, Z2-R-AUDIO-020, Z2-R-AUDIO-040, Z2-R-AUDIO-052, Z2-R-AUDIO-091, Z2-R-AUDIO-099, Z2-R-AUDIO-106, Z2-R-AUDIO-107, Z2-R-AUDIO-108, Z2-R-AUDIO-109, Z2-R-AUDIO-110, Z2-R-AUDIO-111, Z2-R-AUDIO-112, Z2-R-AUDIO-113, Z2-R-AUDIO-116, Z2-R-AUDIO-147, Z2-R-AUDIO-152, Z2-R-AUDIO-155, Z2-R-AUDIO-158, Z2-R-AUDIO-162, Z2-R-AUDIO-163, Z2-R-AUDIO-165, Z2-R-AUDIO-167 |
| Z2-I-AUDIO-15 | audio profiling | S3 | Z2-R-AUDIO-105, Z2-R-AUDIO-112, Z2-R-AUDIO-130, Z2-R-AUDIO-131, Z2-R-AUDIO-132, Z2-R-AUDIO-133, Z2-R-AUDIO-165 |
| Z2-I-AUDIO-16 | audio authoring | S4 | Z2-R-AUDIO-017, Z2-R-AUDIO-022, Z2-R-AUDIO-025, Z2-R-AUDIO-026, Z2-R-AUDIO-028, Z2-R-AUDIO-058, Z2-R-AUDIO-125, Z2-R-AUDIO-134, Z2-R-AUDIO-138, Z2-R-AUDIO-139, Z2-R-AUDIO-140, Z2-R-AUDIO-141, Z2-R-AUDIO-150, Z2-R-AUDIO-151, Z2-R-AUDIO-161, Z2-R-AUDIO-169, Z2-R-AUDIO-172 |
| Z2-I-AUDIO-17 | audio auditioning | S4 | Z2-R-AUDIO-124, Z2-R-AUDIO-133, Z2-R-AUDIO-143, Z2-R-AUDIO-144, Z2-R-AUDIO-153, Z2-R-AUDIO-156, Z2-R-AUDIO-160, Z2-R-AUDIO-168, Z2-R-AUDIO-169, Z2-R-AUDIO-176 |
| Z2-I-AUDIO-18 | virtual voices | S4 | Z2-R-AUDIO-043, Z2-R-AUDIO-063, Z2-R-AUDIO-064 |
| Z2-I-AUDIO-19 | acoustic zones | S4 | Z2-R-AUDIO-077, Z2-R-AUDIO-078, Z2-R-AUDIO-079, Z2-R-AUDIO-126, Z2-R-AUDIO-171 |
| Z2-I-AUDIO-20 | animation/audio clock sync | S4 | Z2-R-AUDIO-088, Z2-R-AUDIO-089, Z2-R-AUDIO-090, Z2-R-AUDIO-124, Z2-R-AUDIO-125 |
| Z2-I-AUDIO-21 | complete audio workflow: import | S4 | Z2-R-AUDIO-134, Z2-R-AUDIO-135, Z2-R-AUDIO-136, Z2-R-AUDIO-137, Z2-R-AUDIO-145 |
| Z2-I-AUDIO-22 | complete audio workflow: bank/event authoring | S4 | Z2-R-AUDIO-022, Z2-R-AUDIO-025, Z2-R-AUDIO-058, Z2-R-AUDIO-138, Z2-R-AUDIO-139, Z2-R-AUDIO-140, Z2-R-AUDIO-141, Z2-R-AUDIO-145, Z2-R-AUDIO-161 |
| Z2-I-AUDIO-23 | complete audio workflow: mix | S4 | Z2-R-AUDIO-048, Z2-R-AUDIO-049, Z2-R-AUDIO-133, Z2-R-AUDIO-137, Z2-R-AUDIO-140, Z2-R-AUDIO-142, Z2-R-AUDIO-145, Z2-R-AUDIO-169 |
| Z2-I-AUDIO-24 | complete audio workflow: audition in context | S4 | Z2-R-AUDIO-144, Z2-R-AUDIO-145 |
| Z2-I-AUDIO-25 | audio workflow usability acceptance | S4 | Z2-R-AUDIO-145 |

Items of other areas with an AUDIO half:

| Item | Owner | Requirements |
|---|---|---|
| Z2-I-CORE-21 | CORE | Z2-R-AUDIO-044, Z2-R-AUDIO-045, Z2-R-AUDIO-146, Z2-R-AUDIO-147, Z2-R-AUDIO-149 |
| Z2-I-STREAM-11 | STREAM | Z2-R-AUDIO-127, Z2-R-AUDIO-128, Z2-R-AUDIO-129 |
| Z2-I-CINE-04 | CINE | Z2-R-AUDIO-089 |
| Z2-I-CINE-09 | CINE | Z2-R-AUDIO-087 |
| Z2-I-UI-11 | UI | Z2-R-AUDIO-087 |
| Z2-I-UI-23 | UI | Z2-R-AUDIO-087, Z2-R-AUDIO-154 |
| Z2-I-NET-05 | NET | Z2-R-AUDIO-006 |

## Excluded and deferred

No AUDIO inventory item is excluded or deferred: all twenty-five are `required` at S3 or S4 and covered above. The following capabilities are outside every AUDIO item and are not specified; each would need a new inventory item and decision:

| Capability | Status | Rationale |
|---|---|---|
| HRTF binaural rendering | excluded from v1 | headphone output uses the constant-power law with an interaural delay (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Panning to stereo and headphones"); no item asks for HRTF |
| convolution reverb | excluded from v1 | the feedback delay network covers Z2-I-AUDIO-08 at fixed cost on the S24 |
| ambisonics and object-based spatial audio (Windows Sonic, Dolby Atmos) | excluded from v1 | channel-based 2.0, 5.1 and 7.1 output covers Z2-I-AUDIO-06 |
| geometric or ray-traced acoustics | excluded from v1 | zones, portals and occluder boxes cover Z2-I-AUDIO-07 and Z2-I-AUDIO-19 |
| voice chat and networked audio | excluded (Z2-D-073) | networked play is Stage 7 scope; audio is presentation and never replicated |
| split-screen multiple listeners | excluded (Z2-D-076) | one local player |
| choosing a specific output endpoint | excluded from v1 | the default endpoint is always followed (`Zenith2/Docs/Design/AUDIO/Device.md` → "Endpoint changes on Windows") |
