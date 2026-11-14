# AUDIO design: ZenithAudio

The design of the AUDIO area: the Audio module `ZenithAudio`. It owns the audio output device (through miniaudio), the real-time mixer and its device thread, sounds and sound banks as asset kinds, decoding and streaming of audio data, events, voices and their budgets, buses and mixing, spatialisation, occlusion, acoustic zones and reverb, music transitions, dialogue and localized audio, the audio/animation clock, the device-change and Android audio-focus behaviour, audio profiling, the audio cook steps, the audio editor surfaces and the complete audio workflow.

<!-- informative -->
Prior art: the old engine has no audio system. Its only audio code is `Zenith/Core/Zenith_AudioBus.h`, a recording stub that stores one-shot emissions (name, position, loudness, radius, frame) in a per-test vector so gameplay tests can assert audio effects with no backend. AUDIO keeps the idea (tests observe audio through recorded data, here the offline renderer, the consumption log and the counters) and none of its structure (a namespace of mutable statics, a test-only conditional).
<!-- /informative -->

This file is the entry of the AUDIO family. It holds the scope, the dependencies, the file layout, the top-level contracts (threads, frame-graph placement, the real-time rules, errors), the failure table, the budget rows, the stage plan, the decisions and the requirement index. The detail lives in the seven split files listed in § "Document family".

## Purpose and scope

### What AUDIO owns

| Capability | Owner type or file | Specified in |
|---|---|---|
| the Audio module: registration, start, per-world attach | `Zenith_Module_Audio.h`, `Internal/Zenith_AudioModule.cpp` | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_Module_Audio header" |
| the audio system object: device, mixer, voices, banks, meters, audition | `Zenith_AudioSystem` | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSystem header" |
| the audio output device on Windows (WASAPI) and Android (AAudio) through miniaudio; the null and offline backends | `Internal/Device/` | `Zenith2/Docs/Design/AUDIO/Device.md` → "Device backends" |
| the device state machine: open, reroute, loss, retry, suspend, focus interruption | `Internal/Device/Zenith_AudioDeviceStateMachine.cpp` | `Zenith2/Docs/Design/AUDIO/Device.md` → "Device state machine" |
| Android audio focus and its Java bridge | `Internal/Android/` | `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio focus" |
| the real-time mixer, its block graph and the device-thread contract | `Internal/RealTime/` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer graph"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time contract" |
| events, instances, voices, voice budgets, priorities, virtual voices | `Internal/Control/Zenith_AudioVoiceManager.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Voice management" |
| buses, sends, mix snapshots, ducking, user volume categories | `Internal/Control/Zenith_AudioMixerControl.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer graph" |
| the DSP chain: resampler, filters, EQ, compressor, limiter, delay, reverb | `Internal/RealTime/Dsp/` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "DSP chain" |
| spatialisation: attenuation, air absorption, cones, Doppler, panning to 2.0/5.1/7.1 | `Internal/Control/Zenith_AudioSpatialiser.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Spatialisation" |
| occlusion, acoustic zones, portals and zone reverb | `Internal/Control/Zenith_AudioAcoustics.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Acoustic zones" |
| music segments, states and beat-synchronous transitions | `Internal/Control/Zenith_AudioMusic.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Music" |
| dialogue lines, localized audio and locale selection | `Internal/Control/Zenith_AudioDialogue.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue and localized audio" |
| the audio clock and animation/audio synchronisation | `Internal/Control/Zenith_AudioClock.cpp` | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Clock and synchronisation" |
| asset kinds 10 `Audio.Sound` and 11 `Audio.Bank`: runtime types, loaders, cooked formats | `Zenith_AudioAssets.h`, `Internal/Loading/` | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Sound and bank loaders" |
| decoding (PCM16, IMA ADPCM, the D-SEL-CODEC codec) and streaming of large audio | `Zenith_AudioStreamDecoder.h`, `Internal/Decode/`, `Internal/Streaming/` | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Decoders"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Streaming" |
| the audio components and their reflection registration | `Zenith_AudioComponents.h` | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioComponents header" |
| the audio command kinds, their payloads and `Zenith_AudioCommandQueue` | `Zenith_AudioCommands.h`, `Zenith_AudioCommandQueue.h` | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommands header" |
| the `Zenith.Audio` Luau namespace and the audio-trigger side-effect consumer | `Internal/Script/` | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Luau surface" |
| event bridges from other modules' ECS events to audio events | `Internal/Control/Zenith_AudioEventBridge.cpp` | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Event bridges" |
| the cook steps `audio.sound`, `audio.bank` and `audio.eventIndex` | `Zenith2/Tools/ZenithCook/Internal/Audio/` | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook steps" |
| the audio document types, the bank and mixer section writers both tools call, and the in-process preview cook (ruling Q84 (a)) | `Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h`, `Zenith2/Tools/Authoring/Internal/Audio/` | § "ZenithAuthoring AUDIO files"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types" |
| the audio editor surfaces, auditioning and audition in context | `Zenith2/Tools/ZenithEditor/Internal/Audio/` | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces" |
| the offline renderer: deterministic, bit-exact rendering for tests and captures | `Zenith_AudioOfflineRenderer.h` | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header" |
| audio counters, profiler rows, log categories, captures | `Internal/Diagnostics/` | § "Diagnostics and profiling" |

### Explicit exclusions

| Not owned by AUDIO | Owner | Basis |
|---|---|---|
| the decoding of source files (WAV, FLAC, OGG, MP3) into PCM at import, and the import settings user interface | CONTENT (the "audio" importer, `Zenith2/Docs/Design/ASSET/Import.md` → "Boundary with CONTENT importers") | Z2-D-079; AUDIO consumes the importer's intermediate (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio importer boundary") |
| loading, residency, the catalogue, asset I/O and the cook driver | ASSET | AUDIO registers its kind loaders and cook steps through ASSET's registration functions |
| world streaming: which cells load and when | STREAM (`Zenith2/Docs/Design/STREAM.md` (pending)) | AUDIO follows STREAM's data contract (§ "Areas AUDIO depends on") |
| the general-purpose block compression codecs (zlib, zstd) | COMPRESS | Z2-D-004; audio codecs are not general-purpose block codecs, so audio sample data is never stored as a COMPRESS section codec (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Codec identifiers") |
| the platform layer and SDL3; the platform's lifecycle state machine | PLAT | `Zenith2/Docs/Design/PLAT.md` → "Explicit exclusions" names audio devices and focus as AUDIO's, and PLAT tells the listener to stop and resume audio |
| captions and subtitles on screen | UI (Z2-I-UI-11, Z2-I-UI-23) | AUDIO raises caption requests through Luau notifications and dialogue line data (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions") |
| cinematic audio tracks and subtitle sync | CINE (Z2-I-CINE-04, Z2-I-CINE-09) | CINE drives AUDIO through the command kinds and the bridge of `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Event bridges" |
| lip-sync | CHAR (Z2-I-CHAR-08) | AUDIO exposes dialogue line markers and the voice clock only |
| quest, dialogue trees, interaction | game Luau modules (Z2-D-077) | AUDIO plays dialogue lines; it never decides which line |
| networked audio, voice chat | none in v1 (Z2-D-073) | networked play is Stage 7 scope; AUDIO is presentation and never replicated |
| the audio memory budget's value | BUDGET (`Zenith2/Docs/Budgets.md` → "Per-system rows") | Z2-I-CORE-21 is CORE's item; AUDIO reports and sheds |
| HRTF binaural rendering, convolution reverb, ambisonics, object-based spatial audio (Windows Sonic, Dolby Atmos) | not in v1 | § "Excluded and deferred" of `Zenith2/Docs/Requirements/AUDIO.md` |

### Inventory items covered

AUDIO covers every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", table AUDIO, all classified required: Z2-I-AUDIO-01 (audio streaming), -02 (audio decode), -03 (voice budgets), -04 (buses), -05 (mixing), -06 (spatialization), -12 (audio device changes), -13 (Android audio focus), -14 (real-time audio thread constraints) and -15 (audio profiling) at S3; -07 (occlusion), -08 (reverb), -09 (music transitions), -10 (dialogue audio), -11 (localized audio), -16 (audio authoring), -17 (audio auditioning), -18 (virtual voices), -19 (acoustic zones), -20 (animation/audio clock sync), -21 to -24 (the complete audio workflow: import, bank and event authoring, mix, audition in context) and -25 (audio workflow usability acceptance) at S4. The item-to-requirement table is `Zenith2/Docs/Requirements/AUDIO.md` → "Coverage of inventory items".

### Items of other areas with an AUDIO half

| Item | Owner | AUDIO's half |
|---|---|---|
| Z2-I-CORE-21 per-subsystem memory budget: audio | CORE | the memory consumer for `MEMORY_SUBSYSTEM_AUDIO`, the shedding policy, the allocation input to the Stage 1 ruling (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Memory consumer") |
| Z2-I-STREAM-11 coordinated audio streaming | STREAM | region-bound emitters, fades on cell unload, bank dependencies in audio-role cells (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Coordination with world streaming") |
| Z2-I-CINE-04 audio tracks | CINE | the command kinds and time-locked voices a cinematic track drives (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices") |
| Z2-I-CINE-09 subtitle sync | CINE | dialogue line markers and the device clock (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "The device clock") |
| Z2-I-UI-11 captions, Z2-I-UI-23 subtitles and captions | UI | caption data per dialogue line and sound caption tags (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions") |
| Z2-I-NET-05 headless execution | NET | the null-device headless path: voices, events and budgets run with no output device (§ "Headless null-device path") |

### Document family

| File | Contents |
|---|---|
| `Zenith2/Docs/Design/AUDIO.md` (this file) | scope, dependencies, layout, top-level contracts, failure handling, budgets, stage plan, decisions, requirement index |
| `Zenith2/Docs/Design/AUDIO/PublicAPI.md` | every public header in full: Scope of this file; Zenith_Module_Audio header; Zenith_AudioTypes header; Zenith_AudioMath header; Zenith_AudioAssets header; Zenith_AudioComponents header; Zenith_AudioCommands header; Zenith_AudioCommandQueue header; Zenith_AudioSystem header; Zenith_AudioOfflineRenderer header; Zenith_AudioStreamDecoder header; Zenith_AudioScriptApi header; Configuration keys header; Internal interfaces |
| `Zenith2/Docs/Design/AUDIO/Formats.md` | Scope of this file; Sound file; Sound bank file; Codec identifiers; Authoring documents; Runtime records |
| `Zenith2/Docs/Design/AUDIO/Mixer.md` | Scope of this file; Mixer graph; Voice management; DSP chain; Spatialisation; Occlusion; Acoustic zones; Music; Dialogue and localized audio; Clock and synchronisation; Determinism |
| `Zenith2/Docs/Design/AUDIO/Device.md` | Scope of this file; Threads; Real-time contract; Device backends; Device state machine; Windows output; Android output; Latency |
| `Zenith2/Docs/Design/AUDIO/Streaming.md` | Scope of this file; Sound and bank loaders; Load modes; Decoders; Streaming; Coordination with world streaming; Memory accounting |
| `Zenith2/Docs/Design/AUDIO/Authoring.md` | Scope of this file; Import; Cook steps; Document types; Luau surface; Event bridges; Editor surfaces; Complete audio workflow |
| `Zenith2/Docs/Design/AUDIO/Tests.md` | Scope of this file; Test support; Unit tests; Integration suite; Fuzz targets; Benchmarks; Device run records |

### Glossary

| Term | Meaning |
|---|---|
| event | an authored, named playable thing in a bank: a tree of container nodes over sounds, with properties and parameter curves; identified by `Zenith_AudioEventID`, the `Zenith_Hash64` of its path |
| instance | one playing occurrence of an event, identified by `Zenith_AudioInstanceID`; owns 1 to 8 voices |
| voice | one sound being played by an instance; real (mixed by the device thread) or virtual (tracked, advanced, not mixed) |
| bus | a mixing node; a voice outputs to exactly one bus; buses form a tree rooted at `Master` |
| mix snapshot | a named, blendable set of bus overrides (volume, filter), not to be confused with the mixer snapshot |
| mixer snapshot | the complete state record Audio.Update publishes to the device thread once per frame (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record") |
| block | 256 frames at 48 kHz, the mixer's unit of work |
| frame | one sample per output channel; frame counts are `uint64` from device start |
| audio world | the per-world AUDIO state (listener, instances, emitters, zones); a process mixes at most 8 audio worlds |
| control side | everything that runs outside the device thread: `Audio.Update@W<tag>` (per world), `Audio.Mix` (process-wide, once per frame), the control thread, the decode thread |

## Dependencies

### Libraries AUDIO depends on

`ZenithAudio` is a module (layer 7, `Zenith2/Docs/Architecture.md` → "Engine modules and game projects"). It declares, exactly as Architecture's module table:

| Dependency | Used for |
|---|---|
| ZenithRuntime | `Zenith_ModuleInstance`, `Zenith_ModuleStartContext`, `Zenith_WorldAttachContext` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces") |
| ZenithPlatform | configuration (`Zenith_Config`), the preferred locale, the Android JNI handles (§ "Assumed interfaces pending other areas") |
| ZenithAssets | kind loaders, the asset reader for streams, the loader for on-demand sounds, handles |
| ZenithScene | `Zenith.WorldTransform` (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Transform components") |
| ZenithECS | systems, epilogue stages, command kinds, events, world resources |
| ZenithReflect | component, struct and enum registration; the Luau-free binding ABI |
| ZenithJobs | the job context of the epilogue stages (`Audio.Update@W<tag>`, `Audio.Mix`), and `Zenith_FloatEnvironment.h` (`Zenith_ApplyFloatEnvironment`, [Z2-N-CONTRACT-360]) on AUDIO's own threads and the device thread; AUDIO creates no jobs |
| ZenithCore | heap, owned pointers, containers, threads, semaphores, atomics, logging, configuration schema, the governor, maths, `Zenith_Hash64`, `Zenith_RandomSource` |
| ZenithProfiling | zones and counters on control-side threads only |
| miniaudio (third party, MIT-0, `Internal/` only) | the output device layer on Windows and Android, its format conversion when the endpoint does not run at 48 kHz, and its null backend for CI (the role `Zenith2/Docs/Architecture.md` → "Third-party dependencies" gives it since the Z2-D-082 amendment of 2026-10-09, ruling AUDIO-1; § "ZENITH_SUPPORTED matrix and miniaudio configuration") |
| the D-SEL-CODEC codec library (third party, added by the ruling ticket) | streamed and compressed audio decode |

AUDIO depends on no other module ([Z2-N-ARCH-850], [Z2-N-ARCH-074]): physics, animation, environment, cinematics, UI and streaming collaborate with it through components, ECS events read by stable name and command kinds.

### Core document sections AUDIO relies on

| Section | What AUDIO takes from it |
|---|---|
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Audio update" | the epilogue node `Audio.Update@W<tag>`, the drained `Zenith_AudioCommandQueue`, the snapshot channel, [Z2-N-ARCH-434], [Z2-N-ARCH-435] |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure" | the epilogue subgraph, stage 12 (per world) and stage 13 (engine-wide, group 0) |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Engine stage nodes" | the shape of an engine-wide EPILOGUE node (`Assets.Service`, `Render.Seal`), which `Audio.Mix` follows as a module-registered stage-13 node ([Z2-N-ARCH-442]) |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Simulation clock" | the tick duration and the interpolation alpha used for trigger scheduling and for interpolating audio positions ([Z2-N-ARCH-415]) |
| `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" | step 15 module start, headless start ([Z2-N-ARCH-867]), the canonical module order |
| `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" | the start and attach contexts, `Zenith_ModuleError` |
| `Zenith2/Docs/Architecture/CompositionRoot.md` → "Exit codes" | exit code 4 for "audio when required" |
| `Zenith2/Docs/Architecture/FramePhases.md` → "Phase-specific rules" | engine stages outside the phase schema ([Z2-N-ARCH-502]); event delivery in phase 9 ([Z2-N-ARCH-478]) |
| `Zenith2/Docs/Architecture.md` → "Memory governor" | the consumer interface and the subsystem `MEMORY_SUBSYSTEM_AUDIO` |
| `Zenith2/Docs/Architecture.md` → "Shedding per subsystem" | the Audio row: WARNING and CRITICAL shedding |
| `Zenith2/Docs/Architecture.md` → "Parser limits" | channels ≤ 8, sample rate ≤ 192 kHz, declared lengths checked against payload |
| `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" | thread affinity, bounded queues, the job rules |
| `Zenith2/Docs/Contracts.md` → "Lock ordering table" | AUDIO adds no lock (§ "Lock-order entries") |
| `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" | module subsystems destroyed at step 6, module-owned threads joined by the module |
| `Zenith2/Docs/Contracts.md` → "Floating-point environment" | FTZ/DAZ and no contraction, applied on every AUDIO thread |
| `Zenith2/Docs/Contracts.md` → "Errors and results" | `Zenith_Result`, error records, untrusted input never asserts |
| `Zenith2/Docs/Contracts.md` → "Custom infrastructure justification record" | the record for the lock-free channels (§ "Assumed interfaces pending other areas") |
| `Zenith2/Docs/Contracts.md` → "Third-party confinement" | miniaudio and the codec under `Internal/` |
| `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results" | the audio-trigger capacity class (Z2-B-SCRIPT-023) and asynchronous publications |
| `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" | `ASYNC_SOURCE_AUDIO` (value 10, provisional, assigned by SCRIPT's registry; [Z2-N-SCRIPT-924], [Z2-N-SCRIPT-925]); AUDIO cites it and declares no value |
| `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" | AUDIO's block of misuse codes ([Z2-N-SCRIPT-926]; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes") |
| `Zenith2/Docs/Scripting/Bindings.md` → "Public binding types" | `Zenith_ScriptBindingTable`, `Zenith_ScriptSideEffectConsumer`, `SIDE_EFFECT_KIND_AUDIO_TRIGGER` |
| `Zenith2/Docs/Budgets.md` → "Per-system rows" | the AUDIO rows and [Z2-N-BUDGET-231] |

### Areas AUDIO depends on

| Area | Headings relied on | Use |
|---|---|---|
| CORE | `Zenith2/Docs/Design/CORE/Threading.md` → "Threads"; `Zenith2/Docs/Design/CORE/Threading.md` → "Condition variables and semaphores"; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state"; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer"; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"; `Zenith2/Docs/Design/CORE/Maths.md` → "Units"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Levels and categories"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Consumer registration" | threads with `THREAD_PRIORITY_TIME_CRITICAL`, semaphores, containers, units (decibels are specified here), right-handed +Y up with view forward −Z, hashing of event paths, deterministic randomness, logging, config keys, the memory consumer, the loader hand-off queue, `Zenith_GetMonotonicTimeNs` |
| PLAT | `Zenith2/Docs/Design/PLAT.md` → "Explicit exclusions"; `Zenith2/Docs/Design/PLAT/Android.md` → "Zenith_AppLifecycle API"; `Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Registration API"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Engine and game sub-sources" | `OnStopAudio`/`OnResumeAudio` ordering, key registration, AUDIO's platform and quality-tier config fragments |
| MOD | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping" | the Audio row and the plain Start form |
| ECS | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"; `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"; `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandKinds header"; `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_World header" (`Zenith_WorldPlaybackAccess`, `Zenith_EntityTable::GetSimulationID`); `Zenith2/Docs/Design/ECS/Scheduling.md` → "Epilogue and extractors"; `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder" | the bridge system, the epilogue stage, command kinds, component registration, `ReadAnyRow` for followed entities, the playback access and its `GetWorldTag` (`Zenith2/Docs/Architecture/ECSModel.md` → "Playback access and supporting types", [Z2-N-ARCH-716]) |
| SCENE | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Transform components" | `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform`, interpolated with the frame's alpha for emitter, listener, zone and occluder positions and differenced for velocities; the `TELEPORTED` flag disables Doppler for one frame |
| ASSET | `Zenith2/Docs/Design/ASSET.md` → "Asset kinds"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetReader header"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Registration functions"; `Zenith2/Docs/Design/ASSET/Formats.md` → "Header layout"; `Zenith2/Docs/Design/ASSET/Formats.md` → "Section table"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookStepDesc"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Source documents in cook steps"; `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types" | kinds 10 and 11, loaders, raw-range reads, cook steps, document types |
| SCRIPT | `Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Side-effect consumer registration"; `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header" | the audio-trigger consumer (SB22) and the asynchronous-request submitter, registered through the start context; the result decoder (`SetAsyncResultDecoder`) |
| MOD | `Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring"; `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Golden template files" | the runtime's `Zenith_AsyncRequestMultiplexer` routes `ASYNC_SOURCE_AUDIO` requests to AUDIO's submitter; the module publication sink (ruling Q76 (a), pending MOD); the Gradle copy of AUDIO's Java source (pending MOD) |
| ENV | `Zenith2/Docs/Design/ENV/WaterWeather.md` → "Effect prefabs"; `Zenith2/Docs/Design/ENV/WaterWeather.md` → "Weather events for gameplay" | weather prefabs carry AUDIO emitters; the weather-change event is bridged (EN13); its rain, snow and wind values are pending ENV (§ "Assumed interfaces pending other areas") |
| GAME | `Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "World lifecycle state machine"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure" | the resource `Zenith.Runtime.WorldState` (`SessionOrdinal`) that selects the primary audio world; the order in which a transition attaches, swaps and detaches worlds, which `Audio.Mix` follows (§ "Following the presenting world") |
| JOBS | `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Header list" | the epilogue nodes' job context and `Zenith_FloatEnvironment.h` |
| STREAM | `Zenith2/Docs/Design/STREAM.md` (pending) | the region flag `Zenith.Streaming.RegionBound`, `STREAMING_GRID_ROLE_AUDIO`, origin shifts (rows ST10 and AU-prefixed rows of the cross-area notes) |
| ANIM | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationEvents header" | the `Zenith.Animation.Notify` event (`Zenith_AnimNotifyEvent`: `Entity` a `Zenith_EntityHandle`, `NameHash`, `TimeIntoTick`, `Flags` with bit 1 = audio), bridged to audio events |
| UI | `Zenith2/Docs/Design/UI.md` (pending) | caption display; the UI locale key; row UI20 (subtitle keys, answered in `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions") |
| CONTENT | not yet designed | the audio importer and its intermediate |
| DEVT | `Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header"; `Zenith2/Docs/Design/DEVT.md` → "Counters" | the debug views draw through `Zenith_DebugDraw`; the on-screen profiler reads AUDIO's counters |
| EDIT | `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Audio tools" | the editor shell that hosts AUDIO's panels; the `ZenithAuthoring` tool library (ruling Q84 (a)) that holds AUDIO's document types, section writers and preview cook, realised as `Zenith2/Tools/Authoring/Internal/Audio/` plus the root header `Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h` (row ED41) |

### Areas that consume AUDIO

| Area | What it uses |
|---|---|
| ENV | `Zenith.Audio.Emitter` in weather effect prefabs; the bridge of `Zenith.Environment.WeatherChanged` (EN13) |
| GAME and game Luau | the `Zenith.Audio` namespace; listener on the camera entity |
| ANIM | footsteps and notifies bridged through the event bridge |
| CINE | command kinds `Audio.PostEvent`, `Audio.Stop`, `Audio.SetParameter`, `Audio.MusicSetState`; time-locked voices |
| UI | dialogue line caption data; UI sounds as 2D events on the `UI` bus |
| STREAM | nothing called: AUDIO reads STREAM's data |
| EDIT | `Zenith_AudioSystem`'s audition and meter API |
| DEVT | AUDIO's counters and the profiler row group |
| SAVE | nothing: audio state is presentation and never saved (`Zenith2/Docs/Design/SAVE.md` lists audio output among presentation state) |

### Upward interfaces AUDIO declares

AUDIO is at the top of the layer DAG for its subject; it declares no callback interface for lower layers. It implements interfaces declared below it: `Zenith_ModuleInstance` (ZenithRuntime), `Zenith_AssetKindLoader` twice (ZenithAssets), `Zenith_MemoryConsumer` (ZenithCore), `Zenith_ScriptSideEffectConsumer` twice (ZenithReflect: the audio-trigger consumer and the asynchronous-request submitter), and `Zenith_ModuleFrameListener` (ZenithRuntime, `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces").

### Assumed interfaces pending other areas

**Applied core interfaces (X232 (a)–(c), X238; core-doc patch d, 2026-10-09).** Four interfaces AUDIO previously fenced as assumptions are now core text, and AUDIO cites them instead of restating them:

| Interface | Core text | AUDIO's use |
|---|---|---|
| `Zenith_ModuleFrameListener` (`OnStopAudio`, `OnResumeAudio`, `OnPlatformEvents`) and `Zenith_ModuleStartContext::RegisterFrameListener` | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"; [Z2-N-ARCH-879]: "the runtime forwards a frame listener's calls in canonical module order (`OnStopAudio` and `OnResumeAudio` from its `Zenith_AppLifecycleListener` inside `Platform.Pump`, `OnPlatformEvents` after prologue step 1)" | `Zenith_AudioModule`'s listener pushes STOP_AUDIO, RESUME_AUDIO and WINDOW_FOCUS_GAINED to the control thread's request ring, records `PLATFORM_EVENT_WINDOW_FOCUS_LOST`/`_GAINED` into the main-thread state (§ "Main thread to Audio.Mix channels"), releases finished audition slots, and returns within 50 µs |
| `Zenith_WorldAttachContext::GetSimulationClock()` | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" ("valid until DetachWorld for this world") | `Audio.Update` reads the tick duration Δ and the interpolation alpha α (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Simulation clock") to schedule triggers (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Scheduling a trigger") and to interpolate positions (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space") |
| `Zenith_ModuleStartContext::AddEngineEpilogueStage` and `Zenith_EngineEpilogueStageDesc` | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"; [Z2-N-ARCH-442]: "a group-0 stage-13 node compiled into every frame schedule from the next compile on, in every configuration, headless included and with no world attached"; [Z2-N-ARCH-434] | `Audio.Mix` (§ "Audio.Mix step by step") |
| `Zenith_WorldPlaybackAccess::GetWorldTag` | `Zenith2/Docs/Architecture/ECSModel.md` → [Z2-N-ARCH-716]: "its `GetWorldTag` names the world being played back, the one key by which a module command's playback … finds its per-world module state" | command playback finds the world's `Zenith_AudioCommandQueue` (§ "World tag table") |

X232 (d) (dropping "audio when not headless" from CompositionRoot's step 4) was not applied, because PLAT's answer to row AU5 is open; it changes nothing AUDIO does (the PLAT assumption below). Every AUDIO ticket that registers `Audio.Mix`, the frame listener or reads the clock is blocked by the ZenithRuntime (MOD) ticket that implements these members; AUDIO keeps no interim single-world form (§ "Audio.Mix step by step").

<!-- assumption: pending PLAT -->
**JNI handles for the focus bridge.** `Zenith_Platform::GetAndroidJavaVM()` and `Zenith_Platform::GetAndroidActivity()` return `void*` (the `JavaVM*` and the activity `jobject` as a global reference), ANY_THREAD, valid from `Create` to destruction, null on Windows and Linux. AUDIO never includes SDL headers; it needs no `SDL_INIT_AUDIO` either, because miniaudio enumerates devices itself (`m_bInitAudioSubsystem` may stay false for every target).
<!-- /assumption -->

<!-- assumption: pending CORE -->
**Real-time channels (row AU6).** Two header-only wait-free primitives in ZenithCore, because ZenithAudio is a `ZENITH_GPU` library that is never built in the `linux-tsan` tree, and a concurrency primitive needs `linux-tsan` coverage ([Z2-N-ARCH-173], [Z2-N-CONTRACT-357]). They are specified completely enough for CORE to add them as written, and by ruling Q80 this `Zenith_SpscRing` is the one single-producer single-consumer primitive of the programme: AUDIO's channels and DEVT's per-thread profiler and debug-draw rings both use it (DEVT drops its own `Zenith_ProfilerSpscRing` and its justification record), so this block is the single specification both cite; its contract is a strict superset of what DEVT's ring needed (fixed-size trivially copyable records, DROP_NEWEST, `Zenith_QueueStats`, no hook).

```cpp
// Engine/ZenithCore/Zenith_SpscRing.h (assumed)
/// @contract container Zenith_SpscRing
/// Ownership: Borrows caller-owned storage of exactly N elements, N a power of two in [2, 2^20], given to Init;
///   allocates nothing, ever. Neither copyable nor movable (it holds atomics the two sides address).
/// Thread affinity: ONE PRODUCER AT A TIME and ONE CONSUMER AT A TIME, the two concurrent with each other. The
///   producing thread may differ between calls (for example whichever job worker runs a graph node in a frame)
///   provided every producer call made by one thread happens-before every producer call made by the next one (a
///   frame-graph edge, a semaphore post and wait, or another release/acquire pair hands the role over); the same
///   rule holds for the consumer. Producer calls: TryPush, PushSpan, GetWritableCount. Consumer calls: TryPop,
///   PopSpan, GetReadableCount. GetCapacity and GetStats are ANY_THREAD.
/// Invalidation: None; elements are copied in and copied out, never referenced in place.
/// Units: Counts in elements.
/// Errors: TryPush returns false when the ring holds N elements (the owner's declared policy is
///   QUEUE_FULL_POLICY_DROP_NEWEST: the ring counts the drop); TryPop returns false when empty; Init with a
///   capacity that is not a power of two in [2, 2^20], or a second Init, is ZENITH_CHECK; any other member before
///   Init is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Source API (header-only).
/// Alignment: the producer index and the consumer index live on separate 64-byte cache lines; storage at alignof(T).
/// Overflow: indices are free-running uint32; N divides 2^32, so wrapping is harmless.
/// Allocation failure: not applicable (no allocation).
/// Complexity: O(1) per element, wait-free; PushSpan and PopSpan copy at most two contiguous runs.
/// Locks: none. No profiler hook, no log, no system call: usable on the audio device thread and inside profiler hooks.
/// @endcontract
template<typename T>                            // static_assert(std::is_trivially_copyable_v<T>)
class Zenith_SpscRing
{
public:
	Zenith_SpscRing() = default;
	Zenith_SpscRing(const Zenith_SpscRing&) = delete;
	Zenith_SpscRing& operator=(const Zenith_SpscRing&) = delete;
	void Init(Zenith_MutableSpan<T> axStorage);                     // before either side's first call
	bool TryPush(const T& xValue);                     // producer
	uint32 PushSpan(Zenith_Span<const T> axValues);   // producer: as many as fit, in order; the rest counted as dropped
	bool TryPop(T& xOut);                              // consumer
	uint32 PopSpan(Zenith_MutableSpan<T> axOut);      // consumer: up to the span size
	uint32 GetReadableCount() const;                  // consumer
	uint32 GetWritableCount() const;                  // producer
	uint32 GetCapacity() const;                       // N
	Zenith_QueueStats GetStats() const;                // ANY_THREAD, relaxed reads
private:
	T* m_pxStorage = nullptr;
	uint32 m_uMask = 0;                                  // N − 1; 0 before Init
	alignas(64) std::atomic<uint32> m_uWrite{ 0 };       // producer-owned, read by the consumer
	uint32 m_uCachedRead = 0;                            // producer-owned copy of m_uRead
	std::atomic<uint64> m_ulDropped{ 0 };                // producer increments (relaxed)
	alignas(64) std::atomic<uint32> m_uRead{ 0 };        // consumer-owned, read by the producer
	uint32 m_uCachedWrite = 0;                           // consumer-owned copy of m_uWrite
	std::atomic<uint32> m_uHighWater{ 0 };               // consumer updates (relaxed)
};
```

Memory orders, per operation (the only atomics are the two indices and the two statistics):

| Operation | Steps |
|---|---|
| `TryPush` | `w = m_uWrite.load(relaxed)`; if `w − m_uCachedRead == N`, refresh `m_uCachedRead = m_uRead.load(acquire)`; still full → `m_ulDropped.fetch_add(1, relaxed)`, return false; else copy into `storage[w & mask]`, `m_uWrite.store(w + 1, release)`, return true |
| `PushSpan` | as `TryPush` for the whole span: free = `N − (w − cachedRead)` (refreshed once with acquire when smaller than the span), copy `min(free, size)` in at most two runs, one `store(w + copied, release)`, add the uncopied count to `m_ulDropped` |
| `TryPop` | `r = m_uRead.load(relaxed)`; if `r == m_uCachedWrite`, refresh `m_uCachedWrite = m_uWrite.load(acquire)`; still empty → false; else copy out of `storage[r & mask]`, update the high water with `m_uCachedWrite − r` (relaxed store when larger), `m_uRead.store(r + 1, release)` |
| `PopSpan` | as `TryPop` for up to the span size, one acquire refresh, one release store |
| `GetReadableCount` / `GetWritableCount` | `m_uWrite.load(acquire) − r` / `N − (w − m_uRead.load(acquire))` |
| `GetStats` | `{ m_ulFullEvents = 0, m_ulDropped = m_ulDropped.load(relaxed), m_uHighWater = m_uHighWater.load(relaxed) }` |

Capacity semantics: the ring holds exactly N elements (full when `write − read == N`), not N − 1. Counters ([Z2-N-CONTRACT-345]): the ring calls no hook (it runs where hooks are forbidden); its OWNER publishes `<queue>.full` (always 0 under DROP_NEWEST, published for uniformity), `<queue>.dropped` and `<queue>.high_water` from `GetStats` on a control-side thread, which satisfies the obligation for every ring AUDIO owns (§ "Counters").

```cpp
// Engine/ZenithCore/Zenith_TripleBuffer.h (assumed)
/// @contract container Zenith_TripleBuffer
/// Ownership: Borrows caller-owned storage of exactly three T given to the constructor; allocates nothing; not
///   copyable or movable. The buffer never writes slot contents: the owner initialises all three slots to a valid value
///   before the consumer's first GetFront (AUDIO: the empty snapshot of epoch 0, Formats.md "Mixer snapshot record").
/// Thread affinity: ONE PRODUCER AT A TIME (BeginWrite, PublishWrite) and ONE CONSUMER AT A TIME (AcquireLatest,
///   GetFront), handed over exactly as Zenith_SpscRing's roles; GetStats ANY_THREAD.
/// Invalidation: The reference BeginWrite returns is valid until PublishWrite; the reference GetFront returns until
///   the next AcquireLatest that returns true.
/// Units: None.
/// Errors: A span whose size is not 3 is ZENITH_CHECK. It is the depth-1 QUEUE_FULL_POLICY_DROP_OLDEST queue of
///   [Z2-N-ARCH-434]: a publish that replaces an unconsumed one counts the older as dropped.
/// Version: since Z2 Stage 2. Source API (header-only).
/// Alignment: the shared index byte on its own 64-byte cache line.
/// Overflow: the publish and drop counts are uint64.
/// Allocation failure: not applicable.
/// Complexity: O(1), wait-free; no copy at publish or acquire.
/// Locks: none; no hook, no log, no system call.
/// @endcontract
template<typename T>                            // static_assert(std::is_trivially_copyable_v<T>)
class Zenith_TripleBuffer
{
public:
	explicit Zenith_TripleBuffer(Zenith_MutableSpan<T> axSlots);   // exactly 3, each already initialised by the owner; slot 0 is the initial front
	T& BeginWrite();               // producer: the back slot; calling it again before PublishWrite returns the same slot
	void PublishWrite();                         // producer: the back slot becomes the newest
	bool AcquireLatest();          // consumer: true when a newer slot became the front
	const T& GetFront() const;     // consumer: slot 0's contents until the first acquire
	Zenith_QueueStats GetStats() const;   // dropped = publishes superseded unread; high water 1
private:
	T* m_pxSlots = nullptr;
	uint8 m_uBack = 2;                                   // producer-owned
	uint8 m_uFront = 0;                                  // consumer-owned
	alignas(64) std::atomic<uint8> m_uMiddle{ 1 };       // slot index in bits 0–1, bit 2 = FRESH
	std::atomic<uint64> m_ulPublished{ 0 }, m_ulSuperseded{ 0 };
};
```

`PublishWrite`: `prev = m_uMiddle.exchange(m_uBack | FRESH, acq_rel)`; when `prev & FRESH`, `m_ulSuperseded.fetch_add(1, relaxed)`; `m_uBack = prev & 3`; `m_ulPublished.fetch_add(1, relaxed)`. `AcquireLatest`: when `(m_uMiddle.load(relaxed) & FRESH) == 0` return false; `prev = m_uMiddle.exchange(m_uFront, acq_rel)`; `m_uFront = prev & 3`; return true (the exchange's acquire half makes the producer's writes to that slot visible). The initial state is front 0, middle 1 (not fresh), back 2.

Justification records ([Z2-N-CONTRACT-278]; appended by CORE's landing ticket in the shape of `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records", provisional until the measurement ruling):

| Record | Standard alternative | Improvement claimed | Benchmark (`Zenith_Bench_CoreContainers`) | Acceptance |
|---|---|---|---|---|
| `Custom threading primitive: Zenith_SpscRing` | CORE's `Zenith_BoundedQueue<T>` (MPMC, a CAS per push, 64-byte cells, rejects DROP_OLDEST) | wait-free, no CAS, no per-cell padding, no hook: usable on the audio device thread and in profiler hooks | `SpscRing_PushPop_16B`, `SpscRing_PushPop_192B` against `Zenith_BoundedQueue` at one producer and one consumer | median push plus pop ≤ 50% of the bounded queue's on the reference desktop and the S24; memory per element = `sizeof(T)`; `linux-tsan` clean |
| `Custom threading primitive: Zenith_TripleBuffer` | a `Zenith_Mutex`-protected double buffer (forbidden on the device thread) or `Zenith_BoundedQueue` (has no DROP_OLDEST) | wait-free latest-value hand-over with no copy | `TripleBuffer_PublishAcquire_53KB` | median `AcquireLatest` + `GetFront` ≤ 20 ns on the S24; `linux-tsan` clean |

CORE's tests (`Zenith2/Engine/ZenithCore/Tests/`, the `Test` step and `Sanitizers -Preset linux-tsan` and `win-asan`): `Test_SpscRing_FifoOrderAndExactCapacity` (N pushes succeed, the N+1th returns false and counts one drop, pops return the pushed order), `Test_SpscRing_IndicesWrapPast32Bits` (both indices started near 2^32 through the test-access seed), `Test_SpscRing_SpansAcrossTheWrap`, `Test_SpscRing_InitRejectsBadCapacity` (`z2-death-check`), `Test_SpscRing_StressRoleHandOver` (10^7 sequence-numbered elements with 2, 8 and hardware-concurrency threads: with 2 one producer and one consumer; with more the producer role and the consumer role each rotate among threads every 4,096 elements through a semaphore chain; every element received once, in order), `Test_TripleBuffer_InitialFrontIsSlotZero`, `Test_TripleBuffer_LatestWinsAndCountsSuperseded`, `Test_TripleBuffer_RepeatedBeginWriteReturnsSameSlot`, `Test_TripleBuffer_StressNoTornRead` (a producer writing self-checksummed 4 KiB values, the consumer verifying every acquired value, the producer role rotating among workers, 10^6 publishes, 2/8/hardware threads).
<!-- /assumption -->

<!-- assumption: pending CORE -->
**A no-allocation scope (row AU6).** A field `m_uNoAllocationDepth` (`uint32`) in CORE's Debug thread state (`Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state"), present in every configuration like every other field of that struct ([Z2-N-CONTRACT-444]) and maintained and checked wherever `ZENITH_ASSERTS_ENABLED=1`; declared in `Engine/ZenithCore/Zenith_NoAllocationScope.h`:

```cpp
void Zenith_EnterNoAllocationScope();   // ++depth on the calling thread
void Zenith_LeaveNoAllocationScope();   // --depth; ZENITH_ASSERT depth > 0
class Zenith_NoAllocationScope { public: Zenith_NoAllocationScope(); ~Zenith_NoAllocationScope(); };   // RAII pair
```

While the depth is non-zero, `Zenith_Heap::TryAllocate`, `Allocate` and `Free` on that thread are `ZENITH_ASSERT` failures naming the scope. The mixer callback holds the scope for its whole body, so any allocation reached from the device thread fails every test run in an asserting preset. CORE's test: `Test_NoAllocationScope_HeapAllocateAsserts` (`z2-death-assert`, in CORE's threading defect tests).
<!-- /assumption -->

<!-- assumption: pending MOD, SCRIPT -->
**Script notifications through the single module publication sink (ruling Q76 (a); Stage 4).** Script requests of `Zenith.Audio.When…` are `SIDE_EFFECT_KIND_ASYNC_REQUEST` records of source kind `ASYNC_SOURCE_AUDIO` (value 10, provisionally assigned by `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; AUDIO declares no value). Their completion IDs are allocated by SCRIPT at commit on the world's merge chain ([Z2-N-SCRIPT-334]); MOD's `Zenith_AsyncRequestMultiplexer` routes them to AUDIO's submitter, which AUDIO registers with `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, …)` after `DeclareScriptAsyncSource(ASYNC_SOURCE_AUDIO, uAUDIO_NOTIFY_RESULT_BYTES)` (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"). AUDIO allocates no completion ID itself and has no C++-originated publication: it only parks results, through the single module publication sink MOD's post-signoff patch specifies (`Zenith_WorldAttachContext::GetPublicationSink()`, its `Park(ulCompletionID, uSourceKind, uPublicationKind, xPayload)`), with `PUBLICATION_KIND_GENERIC_RECORDED`, at most Z2-B-SCRIPT-026's audio value (default 32) in flight per world, 16-byte results. AUDIO needs no `DeclareSource` call on the sink beyond what `DeclareScriptAsyncSource` already sizes. The full path is `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications". The S3 surface has no notification and does not depend on this.
<!-- /assumption -->

<!-- assumption: pending STREAM -->
**Region flag and audio role (row ST10).** AUDIO reads the component `Zenith.Streaming.RegionBound` property 2 `InActiveRegion` (bool) by stable name and the world resource `Zenith.WorldOrigin` (SCENE's `Zenith_WorldOrigin`, its `m_uRebaseCount`), and audio content of a streamed world lives in cells of `STREAMING_GRID_ROLE_AUDIO` (value 4) whose bank components are hard dependencies of the cell scene (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Region-bound emitters").
<!-- /assumption -->

<!-- assumption: pending ENV -->
**Weather values on the weather-change event (row AU14).** `Zenith_WeatherChangedEvent` (`Zenith2/Docs/Design/ENV.md`) carries today only the from and to name hashes, the transition ticks and the arrival flag. AUDIO needs three more reflected `F32` properties, `RainIntensity`, `SnowIntensity` and `WindSpeed` (the target state's values, 0 … 1, 0 … 1 and metres per second), so mixer bridges can set the `Rain`, `Snow` and `Wind` global parameters from the event. Until ENV adds them, weather reaches the mix through constant-value bridges keyed on `ToNameHash` (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Engine event types bridged"), which need nothing from ENV.
<!-- /assumption -->

<!-- assumption: pending MOD, PROC -->
**Packaging the Java focus bridge (row AU4).** MOD's Gradle template (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring") copies every selected module's `Engine/<Lib>/Internal/Android/Java/**` into the generated project's Java sources and emits the keep rule `-keep class org.zenith2.** { native <methods>; public <init>(...); public *; }`, with a golden case of `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Golden template files" for a game selecting Audio (a MOD follow-up). The `ApkCheck` assertion that the class is present in the APK's dex is a protected-gate edit (`Zenith2/Tools/Gates/**`), recorded as a PROC hand-off by AUDIO's ticket writer, and the matrix wording is X233 (b). No AUDIO requirement rests on either: Z2-R-AUDIO-102 is proven by AUDIO's own source test and device run AUDIO-DR-03.
<!-- /assumption -->

**Animation notifies (row AN15, answered by ANIM).** `Zenith.Animation.Notify` is the ECS event type of `Zenith_AnimNotifyEvent` (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationEvents header"), delivered in phase 9. AUDIO's bridge reads it through reflection by the stable property names `Entity` (a `Zenith_EntityHandle`, copied into the post payload as is: AUDIO's payloads carry entity handles, so no simulation-ID conversion is needed), `NameHash`, `Flags` (bit 1, mask `0x02`, marks audio notifies; ANIM's order is gameplay, audio, VFX, warp window) and `TimeIntoTick` (seconds into the tick; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Engine event types bridged").

## Library and file layout

### ZenithAudio target

```cmake
# Zenith2/Engine/ZenithAudio/CMakeLists.txt
zenith_add_library(ZenithAudio
	KIND MODULE
	MODULE_NAME Audio
	DEPENDS ZenithRuntime ZenithPlatform ZenithAssets ZenithScene ZenithECS ZenithReflect ZenithJobs ZenithCore ZenithProfiling
	THIRD_PARTY miniaudio
	SUPPORTED ZENITH_GPU
	TEST_DEPENDS ZenithAudio
	TEST_LABELS audio)
```

The helper is `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"; `SUPPORTED ZENITH_GPU` is the module matrix of `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix" (win-debug, win-release, win-asan, android-arm64; never linux-tsan). The D-SEL-CODEC ruling ticket appends the chosen codec library to `THIRD_PARTY` and to `Zenith2/ThirdParty/manifest.json` (owner ZenithAudio, runtime, windows and android), and no other ticket does.

### Public root headers

| Header | Declares |
|---|---|
| `Zenith2/Engine/ZenithAudio/Zenith_Module_Audio.h` | `Zenith_Module_Audio_RegisterTypes`, `Zenith_Module_Audio_Start`, `Zenith_Module_Audio_GetSystem` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioTypes.h` | constants, identifiers, enumerations, `Zenith_AudioError` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioMath.h` | deterministic dB and gain conversions, trigonometry |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioAssets.h` | `Zenith_SoundAsset`, `Zenith_SoundBankAsset`, event lookup, `Zenith_AudioCookedSection`, `Zenith_AudioValidateSoundFile`, `Zenith_AudioValidateBankFile` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioComponents.h` | the six components and `Zenith_RegisterAudioComponents` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioCommands.h` | command kinds, payload structs, recording helpers |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioCommandQueue.h` | `Zenith_AudioCommandQueue` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioSystem.h` | `Zenith_AudioSystemDesc`, `Zenith_AudioSystem`, status, meters, audition |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioOfflineRenderer.h` | `Zenith_AudioOfflineRenderer` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioStreamDecoder.h` | `Zenith_AudioStreamDecoder`, `Zenith_CreateAudioStreamDecoder` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioScriptApi.h` | the Luau value types and `Zenith_RegisterAudioScriptBindings` |
| `Zenith2/Engine/ZenithAudio/Zenith_AudioConfig.h` | `Zenith_RegisterAudioConfigKeys` and the key name constants |

### Internal files

| Path under `Zenith2/Engine/ZenithAudio/Internal/` | Contents |
|---|---|
| `Zenith_AudioModule.h/.cpp` | `Zenith_AudioModule` (the `Zenith_ModuleInstance`), Start, AttachWorld, DetachWorld |
| `Zenith_AudioSystem.cpp` | `Zenith_AudioSystem` creation steps and members |
| `Control/Zenith_AudioUpdate.cpp` | the `Audio.Update@W<tag>` stage function (per-world steps U1–U7) |
| `Control/Zenith_AudioMix.cpp` | the `Audio.Mix` engine stage function (process steps M1–M11) |
| `Control/Zenith_AudioWorld.h/.cpp` | per-world state: listener, instances, emitters, zones, command intake |
| `Control/Zenith_AudioVoiceManager.h/.cpp` | instances, voices, audibility, real-voice selection, stealing, virtualisation |
| `Control/Zenith_AudioEventPlayer.h/.cpp` | event node evaluation: random, sequence, switch, blend, layered |
| `Control/Zenith_AudioMixerControl.h/.cpp` | buses, sends, mix snapshots, ducking, user volumes |
| `Control/Zenith_AudioSpatialiser.h/.cpp` | attenuation, cones, Doppler, panning matrices |
| `Control/Zenith_AudioAcoustics.h/.cpp` | occluders, the occlusion grid, zones, portals |
| `Control/Zenith_AudioMusic.h/.cpp` | music states, transition scheduling, stingers |
| `Control/Zenith_AudioDialogue.h/.cpp` | dialogue tables, locale resolution, on-demand line loads |
| `Control/Zenith_AudioClock.h/.cpp` | device clock estimate, trigger scheduling, time-locked voices |
| `Control/Zenith_AudioEventBridge.h/.cpp` | the `Audio.EventBridge` phase-9 system |
| `Control/Zenith_AudioMainChannels.h/.cpp` | the main request ring, the main-state triple buffer, the audition table, the acknowledgement atomics and the voice-inspector triple buffer (§ "Main thread to Audio.Mix channels") |
| `Control/Zenith_AudioControlLayout.h/.cpp` | `Zenith_AudioControlLayout`: the control-side ID tables of a mixer layout, published by pointer swap and frame-retired |
| `Control/Zenith_AudioSnapshotWriter.h/.cpp` | builds the mixer snapshot; drains the feedback ring and reconciles the slot-state table (in `Audio.Mix` only) |
| `RealTime/Zenith_AudioMixer.h/.cpp` | the block mixer called by the device callback |
| `RealTime/Zenith_AudioRtVoice.h/.cpp` | device-side voice state and sample fetch |
| `RealTime/Zenith_AudioRtChannels.h` | the snapshot triple buffer, feedback ring, meter-frame triple buffer, applied and released epochs, the slot-state table, the callback-timing atomics |
| `RealTime/Zenith_AudioReplayFeed.h/.cpp` | the test-only voice source kind 5 (a replay feed) and its fetch rule (§ "Bit-exact offline mixing") |
| `RealTime/Dsp/Zenith_AudioResampler.h/.cpp` | Hermite resampler |
| `RealTime/Dsp/Zenith_AudioBiquad.h/.cpp` | RBJ biquads, one-pole filters |
| `RealTime/Dsp/Zenith_AudioDynamics.h/.cpp` | compressor, limiter, ducking sidechain |
| `RealTime/Dsp/Zenith_AudioReverb.h/.cpp` | the FDN reverb |
| `RealTime/Dsp/Zenith_AudioDelay.h/.cpp` | the delay effect |
| `RealTime/Dsp/Zenith_AudioEq.h/.cpp` | the 4-band equaliser |
| `RealTime/Zenith_AudioAdpcm.h/.cpp` | IMA ADPCM block decode (used by both the device thread for resident ADPCM and the decode thread) |
| `Device/Zenith_AudioDeviceBackend.h` | the internal backend interface |
| `Device/Zenith_AudioMiniaudioBackend.h/.cpp` | miniaudio context and device; WASAPI, AAudio and miniaudio's null backend |
| `Device/Zenith_AudioNullBackend.h/.cpp` | the null pacer backend |
| `Device/Zenith_AudioOfflineBackend.h/.cpp` | the offline backend (frames pulled by the offline renderer) |
| `Device/Zenith_AudioDeviceStateMachine.h/.cpp` | the device state machine and the control thread |
| `Device/Zenith_AudioMiniaudioAllocator.cpp` | miniaudio allocation callbacks routed to the root heap ([Z2-N-CONTRACT-240]) |
| `Device/miniaudio_impl.c` | the single miniaudio implementation unit with the configuration of § "ZENITH_SUPPORTED matrix and miniaudio configuration" |
| `Decode/Zenith_AudioDecodeThread.h/.cpp` | the decode thread: it drains the decode request ring, keeps each slot's private refill state, runs stream refills and cache-fill jobs and pushes decode events; the same steps called synchronously by `ServiceBeforeFrame` under the offline backend |
| `Decode/Zenith_AudioDecodeChannels.h/.cpp` | `Zenith_AudioDecodeRequest`, `Zenith_AudioDecodeEvent` and the two rings between `Audio.Mix` and the decode thread (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels") |
| `Decode/Zenith_AudioCodecDecoder.cpp` | the D-SEL-CODEC decoder behind `Zenith_AudioStreamDecoder` |
| `Decode/Zenith_AudioPcmDecoder.cpp` | PCM16 and ADPCM implementations of `Zenith_AudioStreamDecoder` |
| `Streaming/Zenith_AudioStreamSlots.h/.cpp` | stream slots and their state (owned by `Audio.Mix`), the stream blocks (ring, chunk buffers, packet scratch, decoder, asset handle) `Audio.Mix` allocates and retires |
| `Streaming/Zenith_AudioReadSource.h` | `Zenith_AudioReadSource`, AUDIO's internal read seam (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource") |
| `Streaming/Zenith_AudioAssetReadSource.h/.cpp` | `Zenith_AudioAssetReadSource`, the production read source over `Zenith_AssetReader` |
| `Streaming/Zenith_AudioSampleCache.h/.cpp` | the decoded-sample cache |
| `Loading/Zenith_AudioSoundLoader.h/.cpp` | the kind-10 loader |
| `Loading/Zenith_AudioBankLoader.h/.cpp` | the kind-11 loader, bank tables, event index |
| `Loading/Zenith_AudioFileParse.h/.cpp` | the section parsers shared by the loaders, the public validators and the fuzz targets |
| `Loading/Zenith_AudioFileValidate.cpp` | `Zenith_AudioValidateSoundFile` and `Zenith_AudioValidateBankFile`, thin wrappers over the parsers, which the cook's validators call (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Cooked file validation") |
| `Loading/Zenith_AudioRetirement.h/.cpp` | deferred frees gated by the device's released epoch (sample memory) or by frame count (bank tables, event indices) |
| `Loading/Zenith_AudioLoaderHandoff.h/.cpp` | the loader → `Audio.Mix` hand-off (a `Zenith_BoundedQueue` of record pointers) and the sound and bank record pools |
| `Loading/Zenith_AudioEventIndex.h/.cpp` | the process event index, rebuilt and published by `Audio.Mix` |
| `Script/Zenith_AudioScriptBindings.cpp` | the `Zenith.Audio` binding table |
| `Script/Zenith_AudioScriptConsumer.h/.cpp` | `Zenith_AudioScriptConsumer` (the audio-trigger side-effect consumer) |
| `Script/Zenith_AudioAsyncSubmitter.h/.cpp` | `Zenith_AudioAsyncSubmitter` (the `ASYNC_SOURCE_AUDIO` request consumer) and the result decoder |
| `Diagnostics/Zenith_AudioCounters.h/.cpp` | counter publication, profiler rows, log categories |
| `Diagnostics/Zenith_AudioCapture.h/.cpp` | the development output capture |
| `Memory/Zenith_AudioMemoryConsumer.h/.cpp` | the governor consumer and shedding |
| `Content/Audio/master.bank.json`, `Content/Audio/master.mixer.json` | the engine default master bank and mixer layout |
| `Config/Platform/windows.config.json`, `Config/Platform/android.config.json` | AUDIO's platform-layer fragments (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Engine and game sub-sources") |
| `Config/Quality/<tier>.config.json` | AUDIO's quality-tier fragments (reverb quality, real-voice cap, occlusion rays) |

### Internal Windows files

`Zenith2/Engine/ZenithAudio/Internal/Windows/Zenith_AudioWindowsEndpoints.cpp` holds the endpoint-change notification glue miniaudio's WASAPI backend needs beyond its own auto-routing (none in the S3 design: the file holds only the MMCSS task registration of the control thread's reopen path and the `audio.muteWhenUnfocused` window-focus hookup). No other Windows-only source exists.

### Internal Android files

| Path | Contents |
|---|---|
| `Zenith2/Engine/ZenithAudio/Internal/Android/Zenith_AudioFocusAndroid.h/.cpp` | the native half of the focus bridge: `RegisterNatives`, the JNI callbacks, the focus mailbox |
| `Zenith2/Engine/ZenithAudio/Internal/Android/Zenith_AudioAAudioTuning.cpp` | buffer sizing, xrun adaptation and timestamp reads through miniaudio's AAudio stream handle |
| `Zenith2/Engine/ZenithAudio/Internal/Android/Java/org/zenith2/audio/Zenith2AudioFocus.java` | the Java half of the focus bridge, copied into the Player's Gradle project by MOD's template (cross-area row to MOD) |

### Real-time source set

The device thread runs only code under `Internal/RealTime/` (including `RealTime/Dsp/`) plus the backend callback trampolines in `Device/`, whose bodies call `Zenith_AudioMixer::RenderBlocks` and nothing else. `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time source set enforcement" defines the include whitelist and the scanning test.

### Tests files

| Path under `Zenith2/Engine/ZenithAudio/Tests/` | Contents |
|---|---|
| `Zenith_AudioMath_Tests.cpp` … `Zenith_AudioScript_Tests.cpp` | the unit tests of `Zenith2/Docs/Design/AUDIO/Tests.md` → "Unit tests", one file per row of its table |
| `Support/Zenith_AudioTestSignals.h` | generated test signals (Q55) |
| `Support/Zenith_AudioTestHeap.h` | a dedicated real `Zenith_Heap` per test with fault injection and statistics (`Zenith_Heap` is a concrete class, not an interface, so nothing wraps it) |
| `Support/Zenith_AudioScriptedBackend.h` | the scripted test backend (implements the internal backend interface) |
| `Support/Zenith_AudioScriptedReadSource.h` | the scripted read source (implements `Zenith_AudioReadSource` over in-memory cooked fixtures: scripted per-read delay, seeded out-of-order completion, withhold and release, FAILED and CANCELLED injection) |
| `Support/Zenith_AudioReferenceRenderer.h` | the double-precision reference renderer for DSP tests |
| `Fuzz/SoundFile.cpp`, `Fuzz/BankFile.cpp`, `Fuzz/MixerSnapshot.cpp`, `Fuzz/TriggerPayload.cpp` | fuzz targets `Zenith_Fuzz_SoundFile`, `Zenith_Fuzz_BankFile`, `Zenith_Fuzz_MixerSnapshot`, `Zenith_Fuzz_TriggerPayload` (win-asan) |
| `Examples/Zenith_AudioSystem_Example.cpp` | the example the `Zenith_AudioSystem` contract block names |
| `Zenith2/Tests/Audio/` | the integration suite `zenith_add_integration_tests(Audio DEPENDS ZenithAudio ZenithRuntime)` (`Zenith2/Docs/Design/AUDIO/Tests.md` → "Integration suite") |

Every file under `Tests/Support/` is a header ([Z2-N-ARCH-255]; X184); the fixtures tests need on disk (cooked sounds and banks from generated signals) are generated at test time under `${ZENITH_OUT}/test-fixtures/AUDIO/` ([Z2-N-ARCH-257]), never committed.

### Tool-side files

| Path | Contents |
|---|---|
| `Zenith2/Tools/ZenithCook/Internal/Audio/Zenith_AudioCookSteps.h/.cpp` | `Zenith_RegisterAudioCookSteps`, the three steps and the kind-10 and kind-11 validators (over ZenithAudio's public `Zenith_AudioValidateSoundFile` and `Zenith_AudioValidateBankFile`); `audio.bank` calls the section writers of § "ZenithAuthoring AUDIO files" |
| `Zenith2/Tools/ZenithCook/Internal/Audio/Zenith_AudioCookResample.cpp` | the windowed-sinc cook resampler |
| `Zenith2/Tools/ZenithCook/Internal/Audio/Zenith_AudioCookLoudness.cpp` | EBU R128 loudness measurement |
| `Zenith2/Tools/ZenithCook/Internal/Audio/Zenith_AudioCookEncode.cpp` | PCM16, ADPCM and codec encoders |
| `Zenith2/Tools/ZenithCook/Internal/Audio/Zenith_AudioCookSoundProperties.cpp` | `Zenith_AudioCookSoundPropertySource`, the cook's `Zenith_AudioSoundPropertySource` over `Zenith_CookContext::ReadDependencyOutput` |
| `Zenith2/Tools/ZenithEditor/Internal/Audio/` | the panels of `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces", and the editor's `Zenith_AudioSoundPropertySource` over loaded `Zenith_SoundAsset`s |

The `audio.sound` encoders, the cook resampler and the loudness measurement stay in ZenithCook: only the cook runs them. Everything both ZenithCook and ZenithEditor call is in `ZenithAuthoring` (below), because a tool can neither include another tool's `Internal/` nor link a tool executable (non-negotiable 1; row ED19).

### ZenithAuthoring AUDIO files

Ruling Q84 (a) places the tool-side code both ZenithCook and ZenithEditor call in the static tool library `ZenithAuthoring` (`SUPPORTED ZENITH_TOOLS_HOST`, outside every Shipping closure; core rows X257), one owner per subdirectory. AUDIO follows the layout `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs" states, which realises the ruling's `Tools/Authoring/<Area>/` as `Internal/<Area>/` plus one public root header per owner, because [Z2-N-ARCH-161] admits compiled sources only at a library's root and under `Internal/` (EDIT's row ED41; ANIM's post-round move follows the same table). `ZenithAuthoring` already declares `ZenithAudio` in its `DEPENDS` (EDIT's Stage 4 column), so AUDIO's move adds no dependency edge.

| Path | Contents |
|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h` | the public root header, included as `#include "Authoring/Zenith_AuthoringAudio.h"`: `Zenith_RegisterAudioDocumentTypes`, `Zenith_AudioSoundProperties`, `Zenith_AudioSoundPropertySource`, `Zenith_AudioBankWriteResult`, `Zenith_AudioWriteBankFile`, `Zenith_AudioWriteMixerSection`, `Zenith_AudioCookBankPreview`, `Zenith_AudioCookMixerSection` (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types") |
| `Zenith2/Tools/Authoring/Internal/Audio/Zenith_AudioDocumentTypes.cpp` | `Zenith_RegisterAudioDocumentTypes` (moved from `Tools/ZenithCook/Internal/Audio/`) |
| `Zenith2/Tools/Authoring/Internal/Audio/Zenith_AudioBankWriter.h/.cpp` | `audio.bank` steps 2 and 4: validation, node flattening, IDs, the sections of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound bank file" |
| `Zenith2/Tools/Authoring/Internal/Audio/Zenith_AudioMixerWriter.h/.cpp` | the embedded mixer section writer (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Embedded mixer section") |
| `Zenith2/Tools/Authoring/Internal/Audio/Zenith_AudioPreviewCook.cpp` | `Zenith_AudioCookBankPreview` and `Zenith_AudioCookMixerSection` |
| `Zenith2/Tools/Authoring/Tests/Zenith_AuthoringAudio_Tests.cpp` | `Test_AudioDocuments_RegisteredAndMigrate` (`ZenithAuthoring_Tests`, `Test` step) |

ZenithCook declares `ZenithAuthoring` in its `DEPENDS` in the first owner's move (`Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs"); AUDIO's move ticket is that owner's move when it lands first, and otherwise adds nothing to ZenithCook's line. The EDIT tickets that call the audio panels' preview cook are blocked by AUDIO's move ticket (EDIT's rule for every owner).

### Benchmarks and content files

| Path | Contents |
|---|---|
| `Zenith2/Benchmarks/AudioCodec/` | the D-SEL-CODEC decode benchmark (`zenith_add_benchmark(AudioCodec …)`, `Zenith_Bench_AudioCodec`, `--smoke` in CTest) |
| `Zenith2/Benchmarks/AudioMixer/` | the mixer callback and Audio.Update benchmark (`Zenith_Bench_AudioMixer`) |
| `Zenith2/Benchmarks/Budgets/AUDIO/` | nothing: AUDIO has no Stage 1 harness half (`Zenith2/Docs/Budgets.md` → "Per-system producer shape") |

No binary audio clip is committed anywhere in the engine (Q55): every test signal is generated at test time from `Zenith_AudioTestSignals.h`; the codec listening corpus is supplied by the `needs-human` ruling ticket and recorded by hash only (§ "D-SEL-CODEC candidates and criteria").

### ZENITH_SUPPORTED matrix and miniaudio configuration

`ZenithAudio` declares `SUPPORTED ZENITH_GPU`, so it builds in every preset of that matrix (`Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix": win-debug, win-release, win-asan, android-arm64 and android-x86_64) and never in linux-tsan; its tests run in the `Test` step's Windows presets and in `Sanitizers -Preset win-asan`.

<!-- elaboration -->
The plan gave miniaudio the role "audio device, mixing and decode base"; the flagged proposal AUDIO-1 narrowed it, and the orchestrator accepted it on 2026-10-09. `Zenith2/Docs/Architecture.md` → "Third-party dependencies" now reads "audio output device layer (WASAPI, AAudio, null), plus format conversion when the endpoint is not 48 kHz; decoding, node graph, engine and resource manager compiled out", and Z2-D-082 carries the dated amendment. AUDIO compiles miniaudio's decoders, node graph, engine and resource manager out and uses its device layer and its data converter (sample-rate conversion when the endpoint is not 48 kHz, `Zenith2/Docs/Design/AUDIO/Device.md` → "miniaudio backend").

<!-- informative -->
Rationale: (1) bit-exact offline mixing (§ "Bit-exact offline mixing") needs a mixer whose every operation, accumulation order and transcendental function AUDIO controls, which miniaudio's node graph does not promise across versions; (2) the real-time contract (no allocation, no lock, the source-set scan of `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time source set enforcement") must be enforceable on every line the device thread runs, which a third-party graph cannot be scanned for; (3) determinism of voice selection and of event evaluation (Z2-D-019) lives above any mixer anyway; (4) decoding stays with the D-SEL-CODEC library and AUDIO's own PCM16 and ADPCM paths so the codec ruling changes one file. The narrowed role was the flagged proposal AUDIO-1; X234 (a) applied it to Architecture's row with the 48 kHz conversion clause.
<!-- /informative -->

The single implementation unit `Internal/Device/miniaudio_impl.c` defines, before including `miniaudio.h`:

| Define | Why |
|---|---|
| `MA_NO_DECODING`, `MA_NO_ENCODING`, `MA_NO_GENERATION`, `MA_NO_RESOURCE_MANAGER`, `MA_NO_NODE_GRAPH`, `MA_NO_ENGINE` | AUDIO owns decoding, mixing and the graph; the device layer alone is used |
| `MA_ENABLE_ONLY_SPECIFIC_BACKENDS`, `MA_ENABLE_WASAPI`, `MA_ENABLE_AAUDIO`, `MA_ENABLE_NULL` | WASAPI on Windows, AAudio on Android, the null backend everywhere for the CI device-path tests |
| `MA_NO_RUNTIME_LINKING` | ForbiddenCheck forbids dynamic loading outside platform internals; AAudio is linked (`aaudio`), WASAPI through `ole32` and `avrt` |
| `MA_NO_THREADING` is not defined | miniaudio's WASAPI and AAudio backends own the device thread |

The scaffold ticket verifies these option names against the pinned miniaudio release and records any rename as a ruling in its own diff (§ "Rulings AUDIO records in its own tickets"). The allocation callbacks (`ma_allocation_callbacks`) route to the root heap through `Zenith_AudioMiniaudioAllocator.cpp`; the root heap is thread-safe, so the callbacks are correct whichever miniaudio thread calls them.

AUDIO does not rely on any statement about which threads miniaudio uses internally. The design holds under the weakest premise: miniaudio may allocate, take its own mutexes, re-initialise a stream (automatic WASAPI default-device routing, AAudio disconnect recovery) and raise notifications and log callbacks on threads of its own. Therefore (a) every notification and log callback only pushes a record into the multi-producer backend queue and posts the control semaphore (`Zenith2/Docs/Design/AUDIO/Device.md` → "Control thread loop"); (b) AUDIO calls miniaudio only from the control thread and never while it holds anything a miniaudio thread could wait for (AUDIO holds no `Zenith_Mutex` at all, § "Lock-order entries"); (c) the data callback runs only AUDIO's real-time set. The questions the miniaudio configuration ruling answers against the pinned release (which threads re-initialise streams on a reroute; whether a reroute can call the data callback from a new thread, which § "Mixer callback step by step" step 1 already tolerates; whether allocation happens outside init and uninit) are listed in § "Rulings AUDIO records in its own tickets".

## Public API

### API map

| Header | Types and functions | Thread affinity | Specified in |
|---|---|---|---|
| `Zenith_Module_Audio.h` | `Zenith_Module_Audio_RegisterTypes`, `Zenith_Module_Audio_Start`, `Zenith_Module_Audio_GetSystem` | MAIN_THREAD | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_Module_Audio header" |
| `Zenith_AudioTypes.h` | `Zenith_AudioEventID`, `Zenith_AudioInstanceID`, `Zenith_AudioBusID`, `Zenith_AudioParameterID`, `Zenith_AudioMixSnapshotID`, enums, `Zenith_AudioError` | IMMUTABLE | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioTypes header" |
| `Zenith_AudioMath.h` | `Zenith_AudioDbToGain`, `Zenith_AudioGainToDb`, `Zenith_AudioCentsToRatio`, `Zenith_AudioSin`, `Zenith_AudioCos`, `Zenith_AudioAtan2` | ANY_THREAD | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioMath header" |
| `Zenith_AudioAssets.h` | `Zenith_SoundAsset`, `Zenith_SoundBankAsset`, `Zenith_AudioEventInfo`; `Zenith_AudioValidateSoundFile`, `Zenith_AudioValidateBankFile` | IMMUTABLE after activation; the two validators ANY_THREAD | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioAssets header"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Cooked file validation" |
| `Zenith_AudioComponents.h` | `Zenith_AudioListenerComponent`, `Zenith_AudioEmitterComponent`, `Zenith_AudioBankComponent`, `Zenith_AudioZoneComponent`, `Zenith_AudioPortalComponent`, `Zenith_AudioOccluderComponent` | ECS rules | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioComponents header" |
| `Zenith_AudioCommands.h` | `Zenith_AudioCommandKind`, payload structs, `Zenith_AudioRecord*` helpers | OWNER_THREAD (the recording system) | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommands header" |
| `Zenith_AudioCommandQueue.h` | `Zenith_AudioCommandQueue` | world-serial (§ "Thread affinity per type") | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommandQueue header" |
| `Zenith_AudioSystem.h` | `Zenith_AudioSystemDesc`, `Zenith_AudioSystem`, `Zenith_AudioStatus`, `Zenith_AudioMeters`, `Zenith_AudioAuditionDesc` | MAIN_THREAD except the noted getters | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSystem header" |
| `Zenith_AudioOfflineRenderer.h` | `Zenith_AudioOfflineRenderer` | OWNER_THREAD | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header" |
| `Zenith_AudioStreamDecoder.h` | `Zenith_AudioStreamDecoder`, `Zenith_CreateAudioStreamDecoder` | OWNER_THREAD (the decode thread) | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioStreamDecoder header" |
| `Zenith_AudioScriptApi.h` | `Zenith_AudioEventRef`, `Zenith_AudioInstanceRef`, `Zenith_RegisterAudioScriptBindings` | MAIN_THREAD (registration) | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioScriptApi header" |
| `Zenith_AudioConfig.h` | `Zenith_RegisterAudioConfigKeys`, key constants | MAIN_THREAD | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys header" |

### Contract summary per type

Every public type carries its full contract block in its header (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"); `Zenith2/Docs/Design/AUDIO/PublicAPI.md` writes each one out. The ownership rules in short:

| Type | Ownership | Invalidation |
|---|---|---|
| `Zenith_AudioSystem` | `Zenith_Owned` from `Create`; owned by `Zenith_AudioModule` (or by a test or tool) | destroyed at shutdown step 6 after every world detached |
| `Zenith_SoundAsset`, `Zenith_SoundBankAsset` | owned by AUDIO's loaders; reached through `Zenith_AssetHandle` | sample memory retired only after the device epoch passes (`Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement") |
| components | rows of the world's ECS storage | ECS rules |
| `Zenith_AudioCommandQueue` | one per audio world, owned by the module, registered as a world resource | detach |
| `Zenith_AudioOfflineRenderer` | `Zenith_Owned` from `Create`, borrows a `Zenith_AudioSystem` created with the offline backend | before its system |
| `Zenith_AudioStreamDecoder` | `Zenith_Owned` from `Zenith_CreateAudioStreamDecoder` | before the stream slot that owns it |

### Handle and identifier kinds

| Identifier | Representation | Lifetime | Collision rule |
|---|---|---|---|
| `Zenith_AudioEventID` | `uint64` = `Zenith_Hash64` of the UTF-8 event path (`"Footsteps/Grass"`) | stable across builds and cooks | two events with one ID across loaded banks: the second-loaded bank's event is refused (`AUDIO_ERROR_DUPLICATE_EVENT`) and counted |
| `Zenith_AudioInstanceID` | `uint64`, never 0; low bit 1 for script-assigned IDs, 0 for native-assigned | one instance | a post with a live ID is refused and counted (`audio.instances.id_collisions`) |
| `Zenith_AudioBusID`, `Zenith_AudioParameterID`, `Zenith_AudioMixSnapshotID` | `uint32` = low 32 bits of `Zenith_Hash64` of the name | the mixer layout | duplicates refused at mixer load |
| voice slot | `uint16` index + `uint16` generation, internal | one voice | generation check on every snapshot apply |
| sound and bank | `Zenith_AssetHandle<Zenith_SoundAsset>`, `Zenith_AssetHandle<Zenith_SoundBankAsset>` (`Zenith2/Docs/Contracts.md` → "Handle kinds") | asset references | ASSET's |

### Result rules

Fallible functions return `Zenith_Result<T, Zenith_AudioError>` (`Zenith2/Docs/Contracts.md` → "Errors and results"); the replaceable piece (the stream decoder) is created by a factory returning `Zenith_Result<Zenith_Owned<Zenith_AudioStreamDecoder>, Zenith_AudioError>`. Untrusted input (cooked files, authoring documents, command payloads, script arguments, platform callbacks) never reaches an assert.

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| `.zsnd` cooked sound (magic `ZSND`, kind 10) | cooked binary | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file" |
| `.zbank` cooked bank (magic `ZBNK`, kind 11) | cooked binary | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound bank file" |
| audio codec identifiers | enum values in the cooked sound | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Codec identifiers" |
| `*.bank.json` | authoring document `Zenith.Audio.BankDocument` | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank document schema" |
| `*.mixer.json` | authoring document `Zenith.Audio.MixerDocument` | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer document schema" |
| import settings keys | asset settings of the audio importer | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound import settings" |
| mixer snapshot | in-memory record, control → device | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record" |
| feedback record | in-memory record, device → control | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Feedback record" |
| output capture | development WAV file on the user mount | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Capture file" |
| consumption log | test record of snapshot consumption per block | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Consumption log record" |

All multi-byte cooked fields are little-endian, all cooked files use ASSET's 64-byte header and 40-byte section entries (`Zenith2/Docs/Design/ASSET/Formats.md` → "Header layout", "Section table"), and every AUDIO section is stored with section codec 0 (none): sample data is already coded by an audio codec, and bank tables are small.

### Capacities and constants

The structural maxima of the design. They size fixed arrays and are compile-time constants in `Zenith_AudioTypes.h`; budget values (how many voices a slice may use, how many megabytes) are Budgets rows, never these constants ([Z2-N-BUDGET-001]).

| Constant | Value | Meaning |
|---|---|---|
| `uAUDIO_MIX_SAMPLE_RATE` | 48000 | the mixer's internal rate in Hz; every cooked sound is 48 kHz or 24 kHz |
| `uAUDIO_MIX_BLOCK_FRAMES` | 256 | frames per mixer block (5.333 ms) |
| `uAUDIO_MAX_OUTPUT_CHANNELS` | 8 | 7.1 |
| `uAUDIO_MAX_SOURCE_CHANNELS` | 8 | channels of one sound (the parser limit of `Zenith2/Docs/Architecture.md` → "Parser limits") |
| `uAUDIO_MAX_REAL_VOICES` | 256 | device-side voice slots |
| `uAUDIO_AUDITION_REAL_VOICES` | 16 | device slots reserved for the audition world; game worlds share the other 240 |
| `uAUDIO_MAX_VOICES_PER_WORLD` | 2048 | real plus virtual voices of one audio world (each world owns its own voice table) |
| `uAUDIO_MAX_INSTANCES_PER_WORLD` | 1024 | playing instances in one audio world |
| `uAUDIO_MAX_VOICES_PER_INSTANCE` | 8 | voices one instance may own |
| `uAUDIO_MAX_WORLDS` | 8 | audio worlds mixed at once |
| `uAUDIO_MAX_BUSES` | 64 | buses in the mixer layout, `Master` included |
| `uAUDIO_MAX_BUS_DEPTH` | 8 | levels below `Master` |
| `uAUDIO_MAX_EFFECTS_PER_BUS` | 4 | insert effects per bus |
| `uAUDIO_MAX_AUX_REVERBS` | 4 | reverb returns |
| `uAUDIO_MAX_SENDS_PER_VOICE` | 2 | aux sends per voice |
| `uAUDIO_MAX_STREAMS` | 32 | stream slots |
| `uAUDIO_MAX_MIX_SNAPSHOTS` | 32 | mix snapshots defined; 8 active at once |
| `uAUDIO_MAX_DUCKING_RULES` | 16 | ducking rules |
| `uAUDIO_MAX_GLOBAL_PARAMETERS` | 64 | global parameters |
| `uAUDIO_MAX_INSTANCE_PARAMETERS` | 8 | parameters one instance holds |
| `uAUDIO_MAX_EVENT_NODES` | 64 | container nodes per event |
| `uAUDIO_MAX_EVENT_DEPTH` | 8 | node tree depth |
| `uAUDIO_MAX_CURVE_POINTS` | 8 | points of one parameter or attenuation curve |
| `uAUDIO_MAX_BANKS` | 256 | loaded banks |
| `uAUDIO_MAX_ZONES_PER_WORLD` | 256 | acoustic zones |
| `uAUDIO_MAX_PORTALS_PER_WORLD` | 512 | portals |
| `uAUDIO_MAX_OCCLUDERS_PER_WORLD` | 4096 | occluder boxes |
| `uAUDIO_COMMAND_QUEUE_CAPACITY_MAX` | 65536 | the largest `audio.commands.capacity` accepts |
| `uAUDIO_FEEDBACK_RING_CAPACITY` | 1024 | device → control feedback records |
| `uAUDIO_MAX_SOUND_RECORDS` | 16384 | sound records in the loader pool (a load beyond it fails with `AUDIO_ERROR_CAPACITY`) |
| `uAUDIO_LOADER_HANDOFF_CAPACITY` | 65536 | the loader hand-off queue: a power of two ≥ 2 × (`uAUDIO_MAX_SOUND_RECORDS` + `uAUDIO_MAX_BANKS`), so it never fills (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Loader hand-off") |
| `uAUDIO_NOTIFY_RESULT_BYTES` | 16 | the result payload of one script notification |
| `uAUDIO_MAIN_REQUEST_CAPACITY` | 64 | records of the main request ring, a power of two above its proven bound of 37 (§ "Main thread to Audio.Mix channels") |
| `uAUDIO_MAX_AUDITIONS` | 16 | audition slots of the main-thread audition table (one instance each; their voices share the 16 reserved device slots) |
| `uAUDIO_DECODE_REQUEST_CAPACITY` | 256 | records of the `Audio.Mix` → decode thread request ring, a power of two above its proven bound of 128 (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels") |
| `uAUDIO_DECODE_EVENT_CAPACITY` | 512 | records of the decode thread → `Audio.Mix` event ring, a power of two above its proven bound of 256 (same section) |
| `uAUDIO_MAX_CACHE_FILLS` | 32 | cache-fill requests outstanding at once (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Decoded-sample cache") |
| `uAUDIO_MAX_PACKET_FRAMES` | 8192 | the largest packet frames a sound header admits; it bounds the effective prebuffer (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys") |
| `uAUDIO_COUNTER_COUNT` | 65 | entries of `Zenith_AudioCounters` (the rows of § "Counters", in table order) |

### Numeric conventions

- Gains are linear `float`; levels shown or authored are decibels relative to full scale, `float`, suffix `Db` (`m_fVolumeDb`), with −144 dB treated as silence (gain 0). Decibels are AUDIO's unit in the units table of `Zenith2/Docs/Design/CORE/Maths.md` → "Units".
- Pitch is authored in cents (`m_fPitchCents`, 1200 per octave) and applied as a ratio `2^(cents/1200)` through `Zenith_AudioCentsToRatio`.
- Frequencies are `float` Hz (`m_fCutoffHz`); times are `uint32` milliseconds in authoring (`m_uFadeMs`) and `uint64` frames at 48 kHz at runtime.
- Distances are metres; velocities metres per second; directions follow `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems" (right-handed, +Y up, view forward −Z, model front +Z).
- Mixer buffers are planar `float` in [−1, 1] nominal; the output conversion clamps to [−1, 1] after the limiter.

### Command record layout

Commands reach `Zenith_AudioCommandQueue` as 64-byte records, `Zenith_AudioQueuedCommand`, one per command kind playback or script trigger:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `m_eKind` (`Zenith_AudioCommandKind`) |
| 1 | 1 | `m_uSource`: 0 native command, 1 script trigger, 2 event bridge, 3 audition |
| 2 | 2 | `m_uFlags` (the payload header's kind-specific flags) |
| 4 | 4 | `m_fTimeIntoTick`: seconds into the recording tick (0 when unknown) |
| 8 | 8 | `m_ulTick`: the recording tick index |
| 16 | 48 | the payload body after its 16-byte header, `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command payloads" (entity references are `Zenith_EntityHandle` values, 8 bytes) |

### Mixer snapshot layout

The mixer snapshot is the one record of state the device thread reads. It is complete (not a delta), so a dropped snapshot loses nothing ([Z2-N-ARCH-435]); its layout is `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record" and its size is a 256-byte header, 256 voice records of 192 bytes and 64 bus records of 64 bytes: 53,504 bytes, three of which form the triple buffer. Effect parameters and the bus topology are not in it: they live in an immutable real-time layout object the snapshot points to, replaced (and the old one retired by the released epoch) only when the mixer layout changes. Exactly one snapshot is built per frame, by `Audio.Mix`, from every audio world's state.

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Specified in |
|---|---|---|
| Audio.Update step by step | O(commands + instances + voices) per world | § "Audio.Update step by step" |
| Audio.Mix step by step | O(V log V) over every world's candidate voices, V ≤ 8 × 2048 | § "Audio.Mix step by step" |
| mixer callback step by step | O(real voices × block) | § "Mixer callback step by step" |
| real-voice selection | O(V log V), V the candidates of every world | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm" |
| stealing and instance limits | O(instances of the event) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Stealing and instance limits" |
| event node evaluation | O(nodes ≤ 64) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Node table section" |
| attenuation, cones, Doppler, panning | O(1) per voice | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Spatialisation" |
| occlusion | O(cells traversed) per ray, rays capped per frame | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion ray test" |
| zone selection and portal propagation | O(zones + portals of the listener zone) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Acoustic zones" |
| music transition scheduling | O(1) per transition | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling" |
| stream refill | O(streams) per decode tick | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm" |
| trigger scheduling | O(1) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Scheduling a trigger" |
| cook resampling and loudness | O(samples × taps) | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook-time resampling" |

### Audio.Update step by step

<!-- elaboration -->
The work is split in two nodes so that nothing two worlds share is touched while worlds run concurrently. `Audio.Update@W<tag>` (`Internal/Control/Zenith_AudioUpdate.cpp`, one invocation per audio world per frame, EPILOGUE stage 12, concurrent with that world's extraction and with other worlds' epilogue nodes) reads and writes only its own world's `Zenith_AudioWorld`: the world's command queue, instance table, voice table (`uAUDIO_MAX_VOICES_PER_WORLD` entries, allocated at attach), start-sequence counter, random-pick rings, sequence counters, emitter map, intent list and frame record. It reads two process structures and writes none: the event index published by the previous `Audio.Mix` (immutable, `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication") and the active control layout's ID tables (`Zenith_AudioControlLayout`, immutable). It never touches the voice-slot table, stream slots, the decoded-sample cache, the feedback ring, the retire list, the loader hand-off, the mixer control state or the asset loader. Its steps:

1. U1 Read the world's simulation state through the declared access (§ "Frame-graph placement"): the listener, emitter, zone, portal and occluder components with `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` of their entities, the transforms of followed entities by handle, the `Zenith.Runtime.WorldState` resource's `SessionOrdinal`, and the simulation clock's alpha α and tick duration Δ. Presented positions are `Zenith_Lerp(previous, current, α)` and rotations `Zenith_Slerp(previous, current, α)`, the interpolation extraction applies ([Z2-N-ARCH-415]); velocities are the tick-to-tick difference (current − previous) / Δ, which is constant across the frames between two ticks (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space").
2. U2 Drain `Zenith_AudioCommandQueue` in queue order (merge order within each sync point, sync points in phase order, ticks in order, [Z2-N-ARCH-434]) and apply each command to the world: posts create instances (with event node evaluation and its deterministic random draws, seeded per instance, `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"), stops schedule fades, parameter and state commands set targets. A command that needs a process resource (a mix snapshot, ducking, a music state or stinger, a dialogue load, a bus pause) is appended to the world's intent list (capacity `audio.commands.capacity`, the same as the queue, so it cannot fill before the queue does) for `Audio.Mix`. Unknown event IDs, unloaded banks and refused IDs increment the world's counters; nothing fails.
3. U3 Reconcile emitter components: an emitter with `m_bAutoPlay` whose instance is absent gets an instance (held virtual while its `InActiveRegion` flag is false); an emitter row that disappeared stops its instance with its stop fade.
4. U4 Advance every instance by the frame's elapsed presentation time `m_dFrameSeconds` (not simulation time): virtual playheads, fades, sequence and music-cue schedulers of this world, parameter smoothing.
5. U5 Spatialise every voice of the world against its listener: distance, attenuation, cone, occlusion target, zone sends, Doppler ratio, pan matrix (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Spatialisation").
6. U6 Compute each voice's audibility and candidacy flags (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Audibility estimate") and write the world's frame record: the candidate list in voice-index order, the world's counters, the session ordinal, the listener state U1 read (valid flag, presented position and rotation, velocity, attenuation position; the listener an audition in context is spatialised against, § "Audio.Mix step by step" M3), and `m_ulUpdatedFrame` = the frame index.
7. U7 Return. The stage never waits and publishes nothing to the device.

U1–U7 are a pure function of the world's previous state, its queued commands, its component inputs and the frame's presentation time, so the per-world result is independent of which worker ran it and of the order in which worlds finished.

### Audio.Mix step by step

`Audio.Mix` (`Internal/Control/Zenith_AudioMix.cpp`) is AUDIO's engine-wide EPILOGUE node: group 0, stage 13, registered once at module start with `Zenith_ModuleStartContext::AddEngineEpilogueStage` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), and, as [Z2-N-ARCH-442] states, "compiled into every frame schedule from the next compile on, in every configuration, headless included and with no world attached", ordered "after every world's epilogue stage named `m_szAfterWorldStage`" (here `"Audio.Update"`). Every step below therefore runs every frame with no world attached too (editor startup, menus before the first world, headless tools that run the frame loop): the audition (M3), the feedback drain and slot reconciliation (M1), the loader hand-off and event index rebuild (M2), the snapshot publication (M8) and retirement (M9) never depend on a world. A world whose stage is not in the compiled schedule (attached in prologue step 7 of this frame) has no edge to it and is simply skipped until its first update (`m_ulUpdatedFrame` older than this frame). Frame N+1's EPILOGUE starts only after frame N's has completed ([Z2-N-ARCH-386]: "Iteration k+1 begins only after every subgraph of iteration k … has completed"), so `Audio.Mix` of frame N and `Audio.Update` of frame N+1 never run concurrently. Its steps, single-threaded on one worker, iterating worlds in ascending world tag and the audition world (tag `0xFFFF`) last:

1. M1 Drain the feedback ring (the one consumer: the ring's consumer role is held by `Audio.Mix` alone; successive frames' workers hand it over through the frame graph's edges), route each record by its slot's owner in the slot-ownership table (world index and world generation, voice index) to that world's voice (started, ended with its end frame, marker crossed, underrun) or to the clock (clock sample); a record whose owner world has detached since the slot was assigned (its world generation no longer matches) updates only the slot-ownership table, so a slot of a destroyed world is freed when its fade ends and nothing is written into a world that no longer exists. Then reconcile the device's slot-state table against the ownership table: a slot the device reports in state 1 (playing), 2 (fading out) or 3 (ended) for the generation AUDIO assigned acknowledges the voice's start even when no "started" record arrived (the start flag is cleared, `audio.feedback.reconciled_delta` + 1); a slot reported 3 (ended) or 0 (free) for that generation, with no ended record received, is ended as if the record had arrived (end frame = the voice's control-side playhead, `audio.feedback.reconciled_delta` + 1) (`Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"). Then `AcquireLatest` on the meter-frame triple buffer and smooth the bus meters (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Status and meters"). Then drain the decode event ring (the one consumer, handed over between frames as the feedback ring is) and apply each event to its stream slot or cache entry by slot or entry index and generation, discarding an event whose generation is not the current one: OPENED, PREBUFFERED and STARVED_RECOVERED advance the slot state; READ_FAILED and DECODE_FAILED end the slot's voice with a 5 ms fade (`AUDIO_ERROR_IO`, `AUDIO_ERROR_DECODE`) and close the slot; CLOSED appends the slot's stream block (ring, chunk buffers, packet scratch, decoder) to the retire list with the slot's recorded E, releases its asset handle and returns the slot to IDLE; CACHE_FILLED marks the entry ready and CACHE_FAILED retires it, either releasing the sound's handle the entry held (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels").
2. M2 Drain the loader hand-off queue (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Loader hand-off"): activated banks join the bank list, unloaded banks leave it, unloaded sounds are marked dead; when the bank list changed, rebuild the event index and publish it (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication"); queue the replaced index, removed bank tables and dead sound records for retirement.
3. M3 Consume the main thread's two channels (§ "Main thread to Audio.Mix channels"): pop every request of the main request ring in order (audition start, stop, stop-all, layout swap), then `AcquireLatest` on the main-state triple buffer and apply what changed since the state last applied (user volumes, the focus mute, bus overrides, audition parameters and positions, the voice-inspector switch); then run the audition world's U2–U6 (so audition plays with no game world attached and beside one in play-in-editor), and store the acknowledgements the main thread reads back. In the audition world's U5, a slot whose request named a context world W (`m_uContextWorldTag` ≠ 0xFFFF) is spatialised at its main-state position against W's listener, read from W's frame record (U6), when W was updated this frame (its `m_ulUpdatedFrame` is this frame); otherwise (W not updated, detached, or never named) against the virtual listener at the origin facing −Z. Its velocity is 0 (no Doppler) and its output bus is `Audition` in both cases (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition in context").
4. M4 Select the primary world: among the worlds updated this frame, the one with the highest `SessionOrdinal`, ties to the lowest world tag; every other updated world is secondary (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners"). With no world updated there is no primary world, and only the audition world is mixed. § "Following the presenting world" shows that this rule follows GAME's world transition.
5. M5 Apply every world's intent list in world-tag order and, within a world, in list order: mix snapshot activations, ducking targets, bus pauses, music state and stinger requests (the primary world's only; a secondary world's are counted in `audio.music.ignored_delta`), dialogue load and prepare requests (`Zenith_AssetLoader::RequestLoad`, issued here in this order, so asset requests are deterministic too).
6. M6 Real-voice selection over every updated world's candidates and the audition world's (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm"), assigning device voice slots and generations from the slot table in the deterministic order the algorithm defines.
7. M7 Process resources in world-tag then voice order, as the one producer of the decode request ring (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"): first close every stream slot whose owner world generation no longer matches (its world detached, § "Shutdown and detach"); then stream-slot opens, steals, seeks, starvation raises and closes (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"), each allocating what it hands over (an open's stream block) before pushing its request; decoded-sample cache lookups and cache-fill requests (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Decoded-sample cache"); music players' stream preparation. Slot states change only here and in M1; the decode thread never writes one.
8. M8 Mixer control: mix snapshot weights, ducking, user volumes and the focus mute, bus gains and bus overrides; build the mixer snapshot into the triple buffer's back slot (its epoch = the previous epoch + 1; its layout pointer the active real-time layout, which is the new one from the first snapshot after a layout swap adopted in M3 or a master-bank layout built in M2, so the old layout's retirement epoch E is this snapshot's epoch) and `PublishWrite` it; write the voice-inspector frame when the inspector is enabled; post the decode semaphore, after every M7 push, so the decode thread's next wait returns at once (never under the offline backend, § "Bit-exact offline mixing", where `ServiceBeforeFrame` consumes the requests).
9. M9 Retire: free every entry whose epoch is ≤ the device's released epoch, and every frame-retired entry two `Audio.Mix` runs old (the replaced event index, removed bank tables, replaced control layouts: `Zenith_AudioControlLayout`, the control-side ID tables `Audio.Update` reads, is published by the same release pointer swap as the event index and retired by frame count, never by epoch, because no device thread reads it).
10. M10 Apply a pending memory-pressure level (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding").
11. M11 Publish the counters (§ "Counters"), including each world's counters summed in world-tag order and every owned queue's `.full`, `.dropped` and `.high_water`.

Determinism: every input of M1–M11 is either a world's frame record (deterministic, above), the feedback and slot-state data (a function of the device's consumption of earlier snapshots, which the consumption log captures for the offline replay), or the loader hand-off (asset completion timing, presentation only, never visible to simulation, Z2-D-019); the iteration order is fixed (world tag, then voice index). Real-voice selection is therefore a function of the frame's world records alone and never of worker scheduling, world completion order or worker count; `Test_AudioUpdate_ThreeWorldsBitExactAcrossWorkerCounts` renders three worlds with 1, 2 and 8 workers and asserts one output hash, `Test_AudioUpdate_WorldWithoutNodeDoesNotStall` attaches a world whose stage is not yet scheduled and asserts that `Audio.Mix` publishes every frame and the world contributes no voice until its first update, and `Test_AudioSystem_AuditionWithoutWorld` asserts non-silent output from an audition with no world attached.

The offline renderer and the tests that run without a frame graph call the same two functions in the same order through `Zenith_AudioSystem::UpdateWorld` and `Zenith_AudioSystem::RunMix` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSystem"). There is no interim form in which `Audio.Mix` runs inside a world's node: the round-2 single-world fallback is deleted, because the engine-wide stage it stood in for is core text ([Z2-N-ARCH-442]) and every ticket that registers `Audio.Mix` is blocked by the ZenithRuntime ticket implementing it.

### Following the presenting world

<!-- elaboration -->
The primary world of M4 is the world GAME presents, through every world transition, because of the order in which GAME's world host attaches, swaps and detaches worlds. AUDIO conforms to these statements of `Zenith2/Docs/Design/GAME/WorldHost.md`:

- `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation", order 6: the isolated target world is created with "attach every started module in canonical order", while the old world is still `LIVE`; AUDIO's `AttachWorld` runs for it then (tables, queue, `Audio.Update@W<tag>` registered).
- `Zenith2/Docs/Design/GAME/WorldHost.md` → "World lifecycle state machine": an `ACTIVATING` world "is being filled in prologue slices", and only a `LIVE` world has "its nodes … in the frame schedule". So the target's `Audio.Update` does not run before the swap, its frame record is never stamped with the current frame, and M4 never considers it.
- `Zenith2/Docs/Design/GAME/WorldHost.md` → "Swap procedure", step 5: "Write N's world state resource …: origin, session ordinal + 1"; step 7: "`Zenith_WorldHost_DestroyWorld(O)` … The modules' `DetachWorld` calls cancel their own operations for O"; step 9: "Lifecycle N → `LIVE`; mark the schedule dirty, so step 8 of the same prologue calls `EmitInto` for N and rebuilds the frame graph". All of it runs in prologue step 7, on the main thread with no frame running.

Consequence, frame by frame, for a swap in the prologue of frame F (old world O, new world N):

| Frame | Worlds updated | M4 primary | What is heard |
|---|---|---|---|
| F − 1 and earlier | O (N attached but not scheduled) | O | O's voices |
| F (prologue step 7) | — | — | `DetachWorld(O)`: O's instances stop, its tables are freed (§ "Shutdown and detach"), its device slots fade in the device's 5 ms stop fade once the snapshot of frame F omits them |
| F (epilogue) | N (first update; U1 reads its `SessionOrdinal`, one more than O's) | N | N's emitters start in U3; O's slots are freed as their fades end (M1, owner generation mismatch) |

No frame mixes neither world except when N has nothing audible yet, and no frame mixes both; the process music player (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Music model") keeps its current state across the change and obeys N's music commands from frame F on. A failed or cancelled transition destroys N before it is ever `LIVE` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition failure and cancellation"); AUDIO's `DetachWorld(N)` frees its tables and O stays primary. A save-source swap follows the same swap procedure. A world GAME creates for play-in-editor is primary exactly when its `SessionOrdinal` is the highest among updated worlds (EDIT is not designed; the rule is M4's, unchanged). The world limit cannot silence a transition: every world, the ninth included, registers its queue and `Audio.Update@W<tag>` at attach; a world beyond `uAUDIO_MAX_WORLDS` audio worlds attaches without voice and instance tables (its node drains its queue and discards it, `audio.commands.no_world_delta`), and the `DetachWorld` of an enabled world enables the lowest-tag disabled world at once (its tables are allocated on the main thread outside frames; an allocation failure leaves it disabled and logs once), so a target world attached as the ninth while eight are live is enabled when O detaches in the same swap.

`Test_AudioSystem_FollowsWorldTransition` drives GAME's swap order with two attached worlds on the offline backend (N attached while O updates; N's resource ordinal one higher; `DetachWorld(O)` then N's first update) and asserts that every frame before the swap mixes only O's voices, the swap frame's snapshot lists only N's voices and the audition's, O's slots are freed after their fades, and a failed transition (N detached before it updates) leaves O primary; `Test_AudioSystem_NinthWorldDisabled` asserts the late enabling.

### Mixer callback step by step

<!-- elaboration -->
The device callback (`Zenith_AudioMixer::RenderBlocks(float* pfInterleaved, uint32 uFrames, uint32 uChannels)`), on the backend's device thread, inside a `Zenith_NoAllocationScope`:

1. Read the monotonic clock (`Zenith_GetMonotonicTimeNs`) for the timing instrument and push one clock-sample feedback record; check the floating-point environment with `Zenith_IsExpectedFloatEnvironment()` (a register read, no system call) on every callback, and only when it returns false (the first callback on a new thread) apply it and read `Zenith_GetCurrentThreadID` once (`Zenith2/Docs/Design/AUDIO/Device.md` → "Floating-point setup on the device thread").
2. Serve the request from the one-block FIFO by render-then-copy, one block at a time: with `uRemaining = uFrames` and `uBuffered` the frames left in the FIFO from earlier callbacks, loop while `uRemaining > 0`: when `uBuffered == 0`, render one block (steps 3–8) into the FIFO (`uBuffered = 256`, read offset 0); copy `n = min(uBuffered, uRemaining)` interleaved frames out; `uBuffered −= n`, `uRemaining −= n`. Invariant: 0 ≤ `uBuffered` ≤ 256 at every step, because a block is rendered only into an empty FIFO; so the FIFO's storage is exactly one block (256 frames × 8 channels × 4 bytes = 8 KiB, planar, preallocated) for every callback size, and a callback renders exactly ⌈max(0, uFrames − b0) / 256⌉ ≤ ⌈uFrames / 256⌉ blocks, b0 ∈ [0, 255] being the frames left at entry. Worked cases: the default 480-frame WASAPI period with b0 = 0 renders 2 blocks and leaves 32; the next callback copies 32, renders 2 blocks and leaves 64 (never more than 256 buffered); a 1,024-frame callback renders 4 blocks; a 96-frame AAudio burst renders 0 or 1. The frames left between callbacks (0 … 255) are the FIFO's whole latency contribution (`Zenith2/Docs/Design/AUDIO/Device.md` → "Latency budget composition").
3. At the start of each block, `AcquireLatest()` on the snapshot triple buffer; if a newer snapshot arrived and its epoch is not 0, diff it against the device voice table: start voices whose slot generation is new (honouring `m_ulStartFrame`), set new targets and record the snapshot's epoch as the slot's last listed epoch L on every listed voice, and begin a 5 ms fade-out on voices the snapshot no longer lists; such a voice keeps L, the epoch of the last applied snapshot that listed it, until its fade ends (`Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"). Epoch 0 is the empty snapshot all three slots hold before the first `Audio.Mix` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Creation steps"): it starts nothing and the device renders silence through the built-in layout. With no new snapshot the previous one stays in effect ([Z2-N-ARCH-435]).
4. For each real voice: fetch source frames (resident PCM16, resident ADPCM block decode, decoded cache, or the stream ring), resample to 48 kHz with the Hermite resampler at the ramped pitch ratio, apply the voice's low-pass and high-pass biquads, multiply by the ramped gain and the ramped pan matrix, accumulate into its bus buffer and into its send buffers.
5. Process buses bottom-up in the snapshot's topological order: insert effects, bus gain ramp, ducking gain, then sum into the parent; aux returns (reverbs, delay) process their send buffers and sum into their output bus.
6. At `Master`: master effects, the limiter, the final clamp to [−1, 1], the user master volume, the downmix to the device layout.
7. Write feedback records (voice started, voice ended with its end frame, stream ring underrun, marker crossed) with `TryPush`; a full ring drops the record and counts it; every 4th block write the bus meters into the meter-frame triple buffer's back slot and `PublishWrite` it (never into the feedback ring, `Zenith2/Docs/Design/AUDIO/Formats.md` → "Meter frame record"); update the slot-state table for every slot whose state changed and store the released epoch (release).
8. Advance the device frame counter by 256 and store it with release order.
9. After the last block, read the clock again and update the callback-timing atomics (§ "Measuring the mixer callback row").

The callback never allocates, locks, logs, waits or calls a profiler hook (`Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread").

### State machine index

| State machine | Specified in |
|---|---|
| device state machine (CLOSED, OPENING, RUNNING, REROUTING, LOST, SUSPENDED, INTERRUPTED, CLOSING, NULL_DEVICE, OFFLINE) | `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table" |
| Android focus (GAIN, LOSS_TRANSIENT, LOSS_TRANSIENT_CAN_DUCK, LOSS) | `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio focus" |
| instance lifecycle (STARTING, PLAYING, STOPPING, VIRTUAL, FINISHED) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Instances voices and slots" |
| voice residency (REAL, FADING_TO_VIRTUAL, VIRTUAL, FADING_TO_REAL) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices" |
| stream slot (IDLE, OPENING, PREBUFFERING, PLAYING, STARVED, SEEKING, CLOSING) | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots" |
| music transition (STABLE, PENDING, CROSSFADING) | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_AudioSystem` | MAIN_THREAD for creation, `CreateWorld`, `DestroyWorld`, audition and every setter; `UpdateWorld` once per world per frame from that world's epilogue node; `RunMix` once per frame from `Audio.Mix`, after every `UpdateWorld` of the frame; `FindEvent` from AUDIO's epilogue nodes and from the main thread outside the EPILOGUE subgraph; `GetVoiceInspector` MAIN_THREAD (the inspector triple buffer's one consumer); ANY_THREAD for `GetStatus`, `GetMeters` and `GetCounters` (atomic snapshots) |
| `Zenith_AudioCommandQueue` | world-serial: appended by the world's sync-point playback and merge nodes and drained by `Audio.Update@W<tag>`, which the frame graph never runs concurrently for one world; two worlds' queues are independent |
| `Zenith_AudioScriptConsumer` | ANY_THREAD per SCRIPT's contract; keeps per-world state keyed by world tag |
| kind loaders | ANY_THREAD per ASSET's contract, one call at a time per asset, possibly concurrently for different assets; they touch only their record and push it to the multi-producer loader hand-off |
| `Zenith_AudioAsyncSubmitter` | ANY_THREAD per SCRIPT's consumer contract (merge nodes); per-world request lists keyed by world tag, each written only by its own world's merge chain |
| `Zenith_AudioMixer` and everything in `Internal/RealTime/` | DEVICE_THREAD (the backend's callback thread); the offline renderer's owning thread in offline mode |
| `Zenith_AudioStreamDecoder` | OWNER_THREAD: the decode thread from a slot's OPEN to its CLOSED; created and retired by `Audio.Mix` (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels") |
| `Zenith_AudioOfflineRenderer` | OWNER_THREAD |
| the components | ECS rules; authority `PHASE_COMMANDS_ONLY` for every property (AUDIO never writes them from a system) |

### Threads AUDIO owns

| Thread | Name (≤ 15 bytes) | Priority | Created by | Purpose |
|---|---|---|---|---|
| device thread | backend-owned (WASAPI worker, AAudio callback, miniaudio null) | the backend's (WASAPI registers MMCSS "Pro Audio"); AAudio's real-time callback | miniaudio | the mixer callback |
| null pacer | `Zenith.AudioNul` | `THREAD_PRIORITY_TIME_CRITICAL` | `Zenith_AudioNullBackend` | calls the mixer at the real-time rate when no device is open (headless, device lost) |
| control | `Zenith.AudioCtl` | `THREAD_PRIORITY_NORMAL` | `Zenith_AudioSystem::Create` | owns the miniaudio context and device; open, close, reopen, retry, focus handling |
| decode | `Zenith.AudioDec` | `THREAD_PRIORITY_HIGH` | `Zenith_AudioSystem::Create` | stream refills, compressed decode into the cache, `ReadRange` submissions |

`THREAD_PRIORITY_TIME_CRITICAL` is reserved for the audio device thread alone (`Zenith2/Docs/Design/CORE/Threading.md` → "Threads"); the null pacer is that thread when no device is open, and exactly one of the two exists at any time. `Zenith2/Docs/Design/AUDIO/Device.md` → "Thread table" gives each loop.

### Frame-graph placement

| Node | Kind | Phase or stage | Declared access |
|---|---|---|---|
| `Audio.EventBridge` | C++ system | `PHASE_EVENTS` (9) | `ReadsEvent` for every bridged event type the mixer layout names; reads of the emitting entity's `Zenith.WorldTransform`; `EmitsCommands` |
| `Audio.Update@W<tag>` | epilogue stage (`FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY`) | EPILOGUE, stage 12, group = the world tag | `Read` of the six audio components, `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` through queries over listeners, emitters, zones, portals and occluders; `ReadAnyRow` of `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` (properties 1 and 2) for followed and attenuation entities, resolved by `Zenith_EntityHandle` through `Zenith_SystemContext::GetChunkOfEntity` on that query slot (a stale handle resolves to an error: the entity vanished); a read of the component `Zenith.Streaming.RegionBound` and `ReadResource` of `Zenith.WorldOrigin` when those types are registered; `ReadResource` of `Zenith.Runtime.WorldState`; `WriteResource(Zenith_AudioCommandQueue)` as sole consumer ([Z2-N-ARCH-727]) |
| `Audio.Mix` | engine-wide epilogue stage (`FRAME_NODE_KIND_STAGE`, `JOB_AFFINITY_ANY`) | EPILOGUE, stage 13, group 0 | a write of the world-independent resource `Zenith.Audio.System` and nothing else; generated `After` edges from every `Audio.Update@W<tag>` ([Z2-N-ARCH-442]) |
| the device callback | outside the frame graph | — | the snapshot triple buffer only ([Z2-N-ARCH-434], [Z2-N-ARCH-435]) |

The command kinds' playback functions run at the sync points of the phases in their allowed mask (phases 1–10) and append to the queue; the script consumer appends on the world's merge nodes in merge order. Epilogue stages are exempt from authority (they write no component); `Test_FrameGraph_AudioUpdateOncePerFrame` (in `Zenith2/Engine/ZenithAudio/Tests/`) asserts one invocation per world per frame, including frames that run no tick and frames that run several, running concurrently with that world's extraction (both only read), and `Test_FrameGraph_AudioMixAfterEveryWorldUpdate` asserts `Audio.Mix` runs once per frame after every world's `Audio.Update`, with no world attached too.

### Main thread to Audio.Mix channels

<!-- elaboration -->
The `MAIN_THREAD` members of `Zenith_AudioSystem` that change what the mix does (`Audition`, `StopAudition`, `StopAllAuditions`, `SetAuditionParameter`, `SetAuditionPosition`, `SetBusOverride`, `ClearBusOverrides`, `ApplyMixerLayout`, `OnUserVolumeChanged`, `OnWindowFocusChanged`, `SetVoiceInspectorEnabled`) may run while the EPILOGUE subgraph runs (editor UI, configuration callbacks), so they never write state `Audio.Mix` reads. They reach `Audio.Mix` through exactly two channels, and `Audio.Mix` answers through atomics and one triple buffer; nothing else crosses between the main thread and `Audio.Mix`. `OnStopAudio`, `OnResumeAudio` and the Android window-focus event go to the control thread's request ring instead (`Zenith2/Docs/Design/AUDIO/Device.md` → "Control thread loop").

| Channel | Type and capacity | Producer → consumer | Records | Full policy | Counters |
|---|---|---|---|---|---|
| main request ring | `Zenith_SpscRing<Zenith_AudioMainRequest>` over 64 records of 128 bytes (8 KiB, allocated at create) | main thread → `Audio.Mix` M3 | audition start, audition stop, stop all, layout swap | cannot fill (rule 4); a failed `TryPush` is `ZENITH_ASSERT` (an AUDIO defect) | `audio.main.requests.full` (always 0), `audio.main.requests.dropped` (0, asserted), `audio.main.requests.high_water` |
| main state | `Zenith_TripleBuffer<Zenith_AudioMainState>`, three slots of 2,304 bytes | main thread → `Audio.Mix` M3 | the complete main-thread-owned mix state | a newer publish supersedes an unread one (DROP_OLDEST, depth 1); the state is complete, so superseding loses nothing | `audio.main.state.dropped` (superseded publishes) |
| acknowledgements | `std::atomic<uint64> m_ulMainAppliedSequence` and `std::atomic<uint32> m_auAuditionSlotState[16]` (audition generation << 8, or-ed with the state: 0 free, 1 starting, 2 playing, 3 ended); release stores by M3, acquire loads by the main thread | `Audio.Mix` → main thread | — | — | — |
| voice inspector | `Zenith_TripleBuffer<Zenith_AudioInspectorFrame>` (three slots of 16 + 2,048 × 48 bytes, allocated when the inspector is first enabled) | `Audio.Mix` M8 → main thread (`GetVoiceInspector`, the one consumer) | the voice list of the last frame | DROP_OLDEST, depth 1 | `audio.inspector.dropped` |

`Zenith_AudioMainRequest` (Internal, 128 bytes, trivially copyable):

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | kind: 1 AUDITION_START, 2 AUDITION_STOP, 3 AUDITION_STOP_ALL, 4 LAYOUT_SWAP |
| 1 | u8 | audition slot (0 … 15; 0xFF for kinds 3 and 4) |
| 2 | u16 | flags: bit 0 positional, bit 1 loop, bit 2 raw sound (no event) |
| 4 | u32 | fade ms (kind 2: the stop fade; kind 3: the stop fade of every audition) |
| 8 | u64 | request sequence: strictly increasing from 1 per system |
| 16 | u64 | kinds 1 and 2: the audition instance ID; kind 4: the new `const Zenith_AudioRtLayout*` |
| 24 | u64 | kind 1: the event ID (0 for a raw sound); kind 4: the new `const Zenith_AudioControlLayout*` |
| 32 | u32 | kind 1: the bank record index (0xFFFFFFFF none) |
| 36 | u32 | kind 1: the sound record index for a raw sound (0xFFFFFFFF none) |
| 40 | f32 [3] | kind 1: the position (with flag bit 0) |
| 52 | u32 | kind 1: the slot's audition generation |
| 56 | u16 | kind 1: the context world tag (`m_uContextWorldTag` of the desc; 0xFFFF none) |
| 58 | 6 bytes | 0 |
| 64 | (u32, f32) [8] | kind 1: the initial parameter IDs and values (64 bytes; ID 0 unused) |

`Zenith_AudioMainState` (Internal, 2,304 bytes): offset 0 `u64` state sequence (from 1); 8 `u32` flags (bit 0 focus muted by `audio.muteWhenUnfocused`, bit 1 voice inspector enabled); 12 `u32` bus override count (≤ 64); 16 `f32` user volume dB [6] (master, music, SFX, dialogue, ambience, UI; 24 bytes); 40 24 bytes 0; 64 bus overrides [64] of 16 bytes (`u32` bus ID, `u8` mute, `u8` solo, `u16` 0, `f32` trim dB, `u32` 0; 1,024 bytes); 1,088 audition parameters [16 slots][8] of (`u32` parameter ID, `f32` value) (1,024 bytes); 2,112 audition positions [16 slots] of `f32` [3] (192 bytes: the slot's position, in its context world's space when the slot names one, else relative to the virtual listener; written by `Audition` from the desc and by `SetAuditionPosition`). 2,304 = 2,112 + 192, a multiple of 16; every `u64` is at offset 0.

Rules:

1. The main thread owns a master copy of `Zenith_AudioMainState` and the audition table (16 slots: the instance ID, the audition generation, the slot's `Zenith_AudioAuditionDesc` with its two asset handles, free or active). Every state setter updates the master copy, increments its sequence, copies it whole into `BeginWrite()` and calls `PublishWrite()`: one 2,304-byte copy per call. M3 applies a state only when its sequence is higher than the last one applied, by difference: user volumes ramp over 50 ms and the focus mute over 100 ms; bus overrides replace the previous set; audition parameters apply to the slot's instance with the 50 ms parameter ramp; audition positions replace the slot's position at once (the audition panel moves it every frame, so no ramp is needed beyond the voice's own pan ramp).
2. `Audition` takes a free slot (none free → `AUDIO_ERROR_CAPACITY`), resolves the desc's bank or sound handle on the main thread to its record index (the handle stays in the main-thread slot, so the record cannot retire while the request is in flight or the audition plays), increments the slot's audition generation, assigns the instance ID (`Zenith_AudioMakeNativeInstanceID(0xFFFF, generation)`, `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Identifiers") and pushes AUDITION_START. `StopAudition` pushes AUDITION_STOP only when the instance's slot is active for that instance's audition generation and the main-thread audition table records no unacknowledged stop for it (the table's per-slot stop flag, set by the push and cleared when the slot is freed); otherwise, including an unknown, ended or already-stopping instance ID, it is a no-op. `StopAllAuditions` pushes AUDITION_STOP_ALL (rule 4) and sets the stop flag of every active slot. `SetAuditionPosition` and `SetAuditionParameter` write the master state only (rule 1) and are no-ops for an unknown or ended instance. A slot is freed, and its handles released, only when the main thread reads `m_auAuditionSlotState[slot]` = (its generation, 3 ended); it reads the 16 states at the start of every main-request member and in the frame listener's `OnPlatformEvents` each prologue.
3. `ApplyMixerLayout` builds both immutable objects on the main thread from the cooked mixer section (parse, the BNK9 checks, allocation from AUDIO's heap: `Zenith_AudioRtLayout` for the device, `Zenith_AudioControlLayout` for the control side), checks the bridged event-type set against the main thread's copy of the active layout's set (the master bank's at first, read from its immutable bank record through the handle the module holds; then the last accepted live layout's) and refuses a change of that set while worlds are attached (`AUDIO_ERROR_MIXER_LAYOUT`, `m_uDetail` 1, both objects freed at once), and likewise a change of any bridge's match, value, time or entity property name (`m_uDetail` 2: the bridge system resolved those offsets at attach, `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bridge system"), refuses with `AUDIO_ERROR_CAPACITY` while 4 layout swaps are unacknowledged (their sequences above `m_ulMainAppliedSequence`), and otherwise pushes LAYOUT_SWAP and returns success. M3 adopts it: the control layout by release pointer swap (the old one frame-retired, M9), the real-time layout in this frame's snapshot (M8; the old one epoch-retired). A swap is never dropped, so no built layout leaks.
4. Bound that makes the ring unfillable: an AUDITION_START needs a free slot and a slot is freed only after its end is acknowledged, so at most 16 starts are outstanding, and, because `StopAudition` pushes at most once per active slot generation (rule 2), at most 16 stops; `StopAllAuditions` pushes nothing while its previous STOP_ALL is unacknowledged, so at most 1 is outstanding; at most 4 LAYOUT_SWAP are outstanding (rule 3). 16 + 16 + 1 + 4 = 37 < 64 records.
5. M3 pops every request in order and applies them in sequence order, then stores `m_ulMainAppliedSequence` (release) with the sequence of the last request applied, and, after the audition world's U2–U6, the slot states (release) of every audition slot whose state changed.

`Test_AudioSystem_MainChannelsOrderAndBounds` pushes 37 outstanding requests with no mix (high water 37, no drop), runs one `Audio.Mix` (every request applied in order, the acknowledgements stored), publishes 100 states between two mixes (one applied, 99 counted in `audio.main.state.dropped`, the last one's values in effect), refuses a fifth unacknowledged layout swap with `AUDIO_ERROR_CAPACITY`, and calls `StopAudition` 1,000 times for one instance, then for an ended and an unknown instance, between two mixes (one AUDITION_STOP pushed, no assert, high water unchanged); `Test_AudioSystem_AuditionWithoutWorld` and `Test_AudioSystem_ApplyMixerLayoutLive` run through these channels.

### The real-time thread contract

The device thread is the hard real-time part of the engine. Its contract, restated here as the top-level obligation and expanded in `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time contract":

- It never allocates, frees, locks a mutex, waits on a semaphore or condition variable, joins, sleeps, logs, calls a profiler hook, calls into miniaudio's device management, performs I/O or calls into any library outside `Internal/RealTime/`.
- It communicates only through the snapshot triple buffer (in), the feedback ring (out), the applied and released epochs and the slot-state table (out), and relaxed or acquire/release atomics for the callback-timing instrument.
- A missing snapshot makes it mix the previous one; a missing stream frame makes it output silence for that voice and report an underrun; it never waits for data.
- Its CPU share of the audio it renders is Z2-B-AUDIO-003, a hard row where one overrun is a breach ([Z2-N-BUDGET-231]); § "Measuring the mixer callback row" defines the quantity and the one threshold.

### Lock-order entries

AUDIO adds no row to `Zenith2/Docs/Contracts.md` → "Lock ordering table": it takes no `Zenith_Mutex`. The control thread and decode thread wait on their own `Zenith_Semaphore`s (not lock-order locks), and every cross-thread channel is a wait-free primitive or CORE's lock-free `Zenith_BoundedQueue`. Whatever miniaudio locks internally, on whatever thread, AUDIO never holds anything when it calls miniaudio and never waits for a miniaudio thread from the device thread, so no ordering exists between AUDIO and miniaudio's locks (§ "ZENITH_SUPPORTED matrix and miniaudio configuration").

### Startup and attach

`Zenith_Module_Audio_RegisterTypes` (startup step 6) registers the components, structs, enums, the command payload structs and the configuration keys. `Zenith_Module_Audio_Start` (step 15, canonical module order) performs, in order: (1) read the configuration; (2) create `Zenith_AudioSystem` (heap blocks, threads, the backend: null when `IsHeadless()`); (3) register both kind loaders with `Zenith_AssetRegistry::RegisterKindLoader` before `CloseRegistration`; (4) register the command kinds; (5) register the script bindings, whose table carries the result decoder for `ASYNC_SOURCE_AUDIO` (`SetAsyncResultDecoder`, before `Seal`; Stage 4); (6) register the audio-trigger consumer (`SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_AUDIO_TRIGGER, …)`); (7) declare the notification source (`DeclareScriptAsyncSource(ASYNC_SOURCE_AUDIO, uAUDIO_NOTIFY_RESULT_BYTES)`) and register the submitter on the runtime's request multiplexer (`SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, xSubmitter)`; Stage 4); (8) register the memory consumer and the frame listener; (9) register `Audio.Mix` with `AddEngineEpilogueStage` (`m_szName` `"Audio.Mix"`, `m_szAfterWorldStage` `"Audio.Update"`, `m_pUserData` the system; [Z2-N-ARCH-442], [Z2-N-ARCH-879]); (10) request the master bank and service it to completion (`ServiceUntilComplete`, 5,000 ms); (11) start the device. Each failing step unwinds what the previous steps created (`Zenith2/Docs/Contracts.md` → "Partial initialisation failure") and returns `MODULE_ERROR_REGISTRATION`, `MODULE_ERROR_OUT_OF_MEMORY` or, only when `audio.device.required` is true, `MODULE_ERROR_DEVICE` (exit code 4). `AttachWorld` creates the audio world (its instance and voice tables from AUDIO's heap) and its queue, registers the queue as a world resource and the world in AUDIO's tag → index table (open addressing over 16 cells keyed by the `uint16` world tag, written only by `AttachWorld` and `DetachWorld` on the main thread outside frames and read without a lock by command playback, which finds its world through `Zenith_WorldPlaybackAccess::GetWorldTag()` ([Z2-N-ARCH-716]), and by the epilogue nodes, both ordered after those writes by the frame structure; a tag with no audio world drops the command and counts `audio.commands.no_world_delta`), adds `Audio.EventBridge` (always, with an empty bridge set when the layout has no bridges, `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bridge system") and the epilogue stage; at most `uAUDIO_MAX_WORLDS` worlds are enabled audio worlds; a world beyond them still gets its queue, its tag-table entry, `Audio.EventBridge` and `Audio.Update@W<tag>`, but no voice or instance tables (audio disabled: its node drains and discards its queue, `audio.commands.no_world_delta`; logged once), and the `DetachWorld` of an enabled world enables the lowest-tag disabled world at once (§ "Following the presenting world").

### Shutdown and detach

`DetachWorld` (main thread, outside frames, so no `Audio.Update` or `Audio.Mix` is running) stops every instance of the world, removes the world from the tag table and bumps its world generation so the next `Audio.Mix` neither reads nor lists it, releases the asset handles its voices and instances hold, and frees the world's instance, voice and request tables at once. No wait is needed: those tables are control-side only (the device reads snapshots, sample memory, cache entries, stream rings and the real-time layout, never a world table), and the memory the device may still read is protected independently, because a sound whose last handle drops is unloaded through the loader hand-off and retired with the epoch of the first snapshot that omits it (`Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"). `DetachWorld` touches no stream slot, because a slot's state belongs to `Audio.Mix` and its buffers to the decode thread's refill until CLOSED: the bumped world generation makes the next `Audio.Mix` close every slot that world held in M7 (a CLOSE through the decode request ring, the slot entering CLOSING), and M1 retires the slot's stream block and releases its asset handle when the decode thread answers CLOSED (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"). The device fades the world's slots in its 5 ms stop fade once a snapshot omits them; M1 frees those slots when their fades end (owner generation mismatch, § "Audio.Mix step by step"). Module destruction at shutdown step 6 (`Zenith2/Docs/Contracts.md` → "Shutdown ownership order"): stop the device and the null pacer (the control thread uninitialises miniaudio), set the decode thread's stop flag (a `std::atomic<bool>` stored with release, separate from the request ring, whose producer is `Audio.Mix` and which no longer runs) and post its semaphore, join `Zenith.AudioCtl`, `Zenith.AudioDec` (after the cancellation wait of § "Cancellation") and `Zenith.AudioNul`, free sample memory, drop asset handles; the runtime then undoes the module's registrations in reverse order ([Z2-N-ARCH-851]).

### Cancellation

Stream reads are submitted through AUDIO's read source (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource"), whose production implementation holds the `Zenith_OperationHandle` `Zenith_AssetReader::ReadRange` returns; closing a stream calls the read source's `RequestCancel` and keeps the destination buffer until the read reports a terminal status ([Z2-N-CONTRACT-091]), and only then does the decode thread answer CLOSED. On-demand sound loads for dialogue are cancelled the same way when the line is stopped before it starts. Shutdown cancels every outstanding operation and waits for completion on the decode thread, bounded by 2,000 ms, after which it logs the leak and proceeds.

### Error enumerations

```cpp
enum Zenith_AudioErrorCode : uint8
{
	AUDIO_ERROR_INVALID_DESC = 1,      // a desc or argument outside its documented range
	AUDIO_ERROR_OUT_OF_MEMORY,         // a heap allocation failed
	AUDIO_ERROR_DEVICE_OPEN_FAILED,    // the backend could not open an output device (m_uOsCode: the backend's result)
	AUDIO_ERROR_DEVICE_LOST,           // the device stopped unexpectedly
	AUDIO_ERROR_THREAD_CREATE_FAILED,
	AUDIO_ERROR_CORRUPT_DATA,          // a cooked section failed a check (m_uDetail: the check number, Formats.md)
	AUDIO_ERROR_UNSUPPORTED_FORMAT,    // a codec, channel count or rate this build does not decode
	AUDIO_ERROR_UNKNOWN_EVENT,
	AUDIO_ERROR_DUPLICATE_EVENT,
	AUDIO_ERROR_CAPACITY,              // a fixed capacity of "Capacities and constants" was reached (m_uIndex: which)
	AUDIO_ERROR_NOT_RUNNING,           // a call that needs a started system
	AUDIO_ERROR_IO,                    // a stream read failed
	AUDIO_ERROR_DECODE,                // the codec rejected a packet
	AUDIO_ERROR_MIXER_LAYOUT,          // the mixer document is invalid (m_uIndex: the bus or rule index); a refused live edit
	                                   // (m_uDetail 1: the bridged event types changed; 2: a bridge's property names changed)
};
struct Zenith_AudioError
{
	Zenith_AudioErrorCode m_eCode = AUDIO_ERROR_INVALID_DESC;
	uint8 m_uDetail = 0;
	uint16 m_uIndex = 0;
	uint32 m_uOsCode = 0;
};
static_assert(sizeof(Zenith_AudioError) == 8);
constexpr const char* Zenith_ToString(Zenith_AudioErrorCode eCode);
```

Loader failures use `Zenith_AssetLoadContext::Fail(uLoaderCode)` with `uLoaderCode = static_cast<uint16>(AUDIO_ERROR_CORRUPT_DATA) << 8 | check number`. The cook's own codes are `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook error codes".

### Luau errors versus asserts

| Situation | Outcome |
|---|---|
| a script passes an argument of the wrong Luau type, a wrong argument count, or an entity of another world | SCRIPT's dispatcher raises its own codes (ZS1511, ZS1510, ZS1102) before AUDIO's thunk runs |
| a script passes a value outside AUDIO's documented range (volume, pitch, fade or ramp time, a default-constructed `AudioEvent` or `AudioInstance`) | one of AUDIO's catalogued codes, a controlled Luau error in the caller's transaction (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes") |
| a script posts an event no loaded bank defines | no error: the trigger is recorded; `Audio.Update` counts `audio.events.unknown_delta` and logs the hash once per ID per session |
| the per-tick audio trigger capacity is exhausted | BR-MERGE at the merge point (Z2-B-SCRIPT-023), as SCRIPT defines |
| `Zenith.Audio.Event` called with an empty path, a path longer than 255 bytes or one outside the path syntax | AUDIO's path misuse code |
| a native system records a malformed payload | `ZENITH_ASSERT` in the recording helper (engine code is not untrusted input) |
| a cooked bank fails a check | the loader fails the asset (`ASSET_KIND` fallback policy FAIL); never an assert |

## Diagnostics and profiling

### Counters

Published once per frame by `Audio.Mix` step M11 through `Zenith_DispatchCounter` (never by the device thread, which only updates atomics the control side reads). Delta counters end in `_delta` ([Z2-N-CONTRACT-155], [Z2-N-CONTRACT-156]). `Zenith_AudioCounters::m_aulValues` has exactly `uAUDIO_COUNTER_COUNT` entries, one per row below in table order.

| Counter | Kind | Meaning |
|---|---|---|
| `audio.voices.real` | GAUGE | real voices mixed in the last block |
| `audio.voices.real_peak` | GAUGE | maximum real voices over the run (Z2-B-AUDIO-004's measurement) |
| `audio.voices.virtual` | GAUGE | virtual voices |
| `audio.voices.deferred_delta` | DELTA | voices that wanted to be real and were kept virtual by the real-voice limit (BR-DEFER) |
| `audio.voices.stolen_delta` | DELTA | voices stolen |
| `audio.instances.live` | GAUGE | live instances, all worlds |
| `audio.instances.refused_delta` | DELTA | posts refused (instance limit with steal mode NONE, capacity) |
| `audio.instances.id_collisions_delta` | DELTA | posts refused for a live instance ID |
| `audio.events.unknown_delta` | DELTA | posts of an unknown event ID |
| `audio.commands.queued_delta` | DELTA | commands drained |
| `audio.commands.dropped_delta` | DELTA | commands dropped by a full queue (DROP_NEWEST) |
| `audio.mixer.callback_us_max` | GAUGE | the longest callback since the previous publication, µs |
| `audio.mixer.callback_share_max` | GAUGE | the largest callback share since the previous publication, in hundredths of a percent (§ "Measuring the mixer callback row"; Z2-B-AUDIO-003) |
| `audio.mixer.callback_share_run_max` | GAUGE | the largest callback share since `Start`, hundredths of a percent (the row's statistic) |
| `audio.mixer.overruns` | CUMULATIVE | callbacks whose share exceeded `audio.mixer.overrunSharePercent`, the one threshold the row uses |
| `audio.mixer.snapshots_dropped` | CUMULATIVE | snapshots superseded before the device read them |
| `audio.mixer.snapshot_reuse_blocks` | CUMULATIVE | blocks mixed with no new snapshot |
| `audio.feedback.dropped` | CUMULATIVE | feedback records dropped by a full ring (the ring's `.dropped`) |
| `audio.feedback.full` | CUMULATIVE | always 0 (DROP_NEWEST), published for uniformity ([Z2-N-CONTRACT-345]) |
| `audio.feedback.high_water` | GAUGE | the feedback ring's high-water mark |
| `audio.feedback.reconciled_delta` | DELTA | voice ends recovered from the slot-state table after a dropped record |
| `audio.commands.full` | CUMULATIVE | full events of the command queues, summed over worlds |
| `audio.commands.high_water` | GAUGE | the largest command-queue high-water mark over worlds |
| `audio.commands.invalid_delta` | DELTA | payloads dropped by playback validation |
| `audio.commands.no_world_delta` | DELTA | commands for a world with audio disabled |
| `audio.control.dropped` | CUMULATIVE | control requests coalesced away in the three control channels (`.full` is 0, `.high_water` below) |
| `audio.control.high_water` | GAUGE | the largest control-channel high-water mark |
| `audio.loader.handoff.high_water` | GAUGE | the loader hand-off queue's high-water mark (its `.full` and `.dropped` are 0 by construction and asserted) |
| `audio.capture.dropped` | CUMULATIVE | capture blocks dropped by a full capture ring |
| `audio.capture.full` | CUMULATIVE | always 0 (DROP_NEWEST), published for uniformity |
| `audio.capture.high_water` | GAUGE | the capture ring's high-water mark (stream rings never drop: their producer pushes only what fits, so they publish no triple) |
| `audio.events.duplicate_delta` | DELTA | events refused because another active bank defines the ID |
| `audio.dialogue.late_delta` | DELTA | dialogue lines started more than 300 ms after their post |
| `audio.music.ignored_delta` | DELTA | music commands of a secondary world |
| `audio.streams.start_timeouts_delta` | DELTA | instances that gave up waiting 2,000 ms for a prebuffer |
| `audio.mixer.nan_frames` | CUMULATIVE | frames in which the final clamp replaced a NaN or infinity |
| `audio.mixer.pacer_skips` | CUMULATIVE | blocks the null pacer skipped after falling 4 blocks behind |
| `audio.device.ignored_events` | CUMULATIVE | device state-machine events ignored by the state's row |
| `audio.streams.active` | GAUGE | open streams |
| `audio.streams.underruns` | CUMULATIVE | stream ring underruns |
| `audio.streams.read_bytes_delta` | DELTA | bytes read by streams this frame |
| `audio.decode.us_delta` | DELTA | decode thread busy time this frame |
| `audio.memory.bytes` | GAUGE | resident bytes reported to the governor |
| `audio.cache.bytes` | GAUGE | decoded-sample cache bytes |
| `audio.device.state` | GAUGE | `Zenith_AudioDeviceState` value |
| `audio.device.reopens` | CUMULATIVE | successful reopens after loss or change |
| `audio.device.latency_us` | GAUGE | the measured output latency |
| `audio.device.underruns` | CUMULATIVE | backend xruns reported by miniaudio or AAudio |
| `audio.update.us_delta` | DELTA | `Audio.Update` CPU summed over worlds plus `Audio.Mix` CPU (Z2-B-AUDIO-002) |
| `audio.occlusion.rays_delta` | DELTA | occlusion segment tests this frame |
| `audio.sync.late_delta` | DELTA | scheduled starts that arrived after their target frame |
| `audio.main.requests.full` | CUMULATIVE | always 0: the main request ring cannot fill (§ "Main thread to Audio.Mix channels" rule 4), published for uniformity ([Z2-N-CONTRACT-345]) |
| `audio.main.requests.dropped` | CUMULATIVE | always 0 and asserted (the ring's `.dropped`) |
| `audio.main.requests.high_water` | GAUGE | the main request ring's high-water mark (at most 37) |
| `audio.main.state.dropped` | CUMULATIVE | main-state publishes superseded before `Audio.Mix` read them (nothing is lost: each state is complete) |
| `audio.inspector.dropped` | CUMULATIVE | voice-inspector frames superseded before the main thread read them |
| `audio.offline.service_timeouts` | CUMULATIVE | offline servicing waits that reached their 5,000 ms bound (§ "Bit-exact offline mixing"; 0 under every other backend) |
| `audio.decode.requests.full` | CUMULATIVE | always 0: the decode request ring cannot fill (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"), published for uniformity ([Z2-N-CONTRACT-345]) |
| `audio.decode.requests.dropped` | CUMULATIVE | always 0 and asserted (the request ring's `.dropped`) |
| `audio.decode.requests.high_water` | GAUGE | the decode request ring's high-water mark (at most 128) |
| `audio.decode.events.full` | CUMULATIVE | always 0: the decode event ring cannot fill (same section), published for uniformity |
| `audio.decode.events.dropped` | CUMULATIVE | always 0 and asserted (the event ring's `.dropped`) |
| `audio.decode.events.high_water` | GAUGE | the decode event ring's high-water mark (at most 256) |
| `audio.meters.dropped` | CUMULATIVE | meter frames superseded before `Audio.Mix` acquired them (the meter-frame triple buffer's superseded count; display only, nothing else is lost) |
| `audio.retire.deferred` | CUMULATIVE | retirements postponed because the 4,096-entry retire list was full; each is retried in the next `Audio.Mix` (`Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement") |

### On-screen profiler rows

AUDIO contributes the row group "Audio" to DEVT's on-screen profiler (Z2-D-005): Update ms (Z2-B-AUDIO-002), Callback share max (Z2-B-AUDIO-003), Real/Virtual voices (Z2-B-AUDIO-004), Streams and underruns, Memory MB (Z2-B-AUDIO-001), Device state and latency ms, Commands dropped. Each row shows the budget value beside the measurement once the row is adopted. The rows read the counters above; DEVT owns the rendering.

### Log categories

| Category | Name | Compiled minimum |
|---|---|---|
| `xLOG_CATEGORY_AUDIO_DEVICE` | `audio.device` | `LOG_LEVEL_INFO` |
| `xLOG_CATEGORY_AUDIO_VOICES` | `audio.voices` | `LOG_LEVEL_WARNING` |
| `xLOG_CATEGORY_AUDIO_CONTENT` | `audio.content` | `LOG_LEVEL_WARNING` |
| `xLOG_CATEGORY_AUDIO_STREAM` | `audio.stream` | `LOG_LEVEL_WARNING` |
| `xLOG_CATEGORY_AUDIO_SCRIPT` | `audio.script` | `LOG_LEVEL_WARNING` |

Declared with `ZENITH_LOG_CATEGORY` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Levels and categories"); every log call is on a control-side thread. Repeated warnings of one kind (an unknown event ID, an underrun of one stream) log once per ID per session and are counted thereafter.

### Debug views

Development builds draw, through DEVT's `Zenith_DebugDraw` (`Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header"), from `Audio.Mix` on the control side: emitter positions with their attenuation spheres (min and max distance), the listener frame, occlusion segments coloured by loss, zone boxes and portal quads, and the voice list overlay (instance, event path, bus, real or virtual, audibility in dB, distance). Toggled by `audio.debug.view` (enum `off`, `emitters`, `occlusion`, `zones`, `voices`).

### Frame-dump and capture entries

The frame dump receives an `audio` member: device state, latency, the real and virtual voice lists with their audibility, bus gains, active mix snapshots, streams and their ring fill. The development capture (`audio.capture.seconds` > 0, or the editor's capture button) writes the final output to `user:/captures/audio_<timestamp>.wav` from a capture ring the device thread fills (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Capture file"); the capture ring is allocated when capture is enabled, never on the device thread.

### Telemetry

The per-frame telemetry record gets `audio.update.us_delta`, `audio.mixer.callback_share_max`, `audio.voices.real` and `audio.streams.underruns`; a device-state change, a focus change and a reroute each emit one telemetry event with the old and new state and the backend's result code.

## Authoring and cooking

### Authoring summary

AUDIO's authoring is document-based (Z2-D-038): bank documents (`*.bank.json`) hold events, music and dialogue tables; one mixer document (`*.mixer.json`) holds the bus tree, effects, sends, mix snapshots, ducking rules, global parameters, attenuation and reverb presets and the bridged event list. Source audio files are imported by CONTENT's importer. The editor surfaces are AUDIO's (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces"), hosted by the EDIT shell.

### Cook steps summary

| Step | Phase | Kind | Reads | Writes |
|---|---|---|---|---|
| `audio.sound` | `COOK_PHASE_ASSET` | 10 `Audio.Sound` | CONTENT's decoded-audio intermediate and the asset's settings | `.zsnd` |
| `audio.bank` | `COOK_PHASE_ASSET` | 11 `Audio.Bank` | the bank document (and, for the master bank, the mixer document) through `OpenSourceDocument`; each sound's published properties | `.zbank`, hard and soft references |

Registered by `Zenith_RegisterAudioCookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)` in `Zenith2/Tools/ZenithCook/Internal/Audio/` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookStepDesc"), with `audio.eventIndex` as the third step. `audio.bank` writes its file through `Zenith_AudioWriteBankFile` and `Zenith_AudioWriteMixerSection` of `ZenithAuthoring` (§ "ZenithAuthoring AUDIO files"), the functions the editor's preview cook calls too, so a preview and a cook of one document produce the same sections. Details: `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook steps".

### Editor surfaces summary

The bank and event editor, the mixer console, the audition panel, audition in context (play-in-editor with live mixing) and the voice inspector and profiler, all specified in `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces"; the scripted workflow walkthrough is `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Workflow walkthrough script".

### Migration hooks

`Zenith.Audio.BankDocument` and `Zenith.Audio.MixerDocument` are registered by `Zenith_RegisterAudioDocumentTypes(Zenith_MigrationRegistry&)` (tool side: declared in `Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h` of `ZenithAuthoring`, ruling Q84 (a), and called by ZenithCook and ZenithEditor, which both link that library), listed in `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types" by AUDIO's first document ticket (cross-area row to ASSET). Each starts at version 1; every schema change bumps the version and adds a migration step in the same diff (Z2-D-048). The six components are reflected types with version 1 and `Migration` steps on change (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder"). Cooked formats re-cook: a format version change bumps `m_uFormatVersion` of the kind loader desc.

## Failure handling

### Failure table

| Failure | Detected by | Behaviour |
|---|---|---|
| no output device at start | control thread, OPENING → OPEN_FAILED | NULL_DEVICE with the null pacer; retry schedule; voices and events run; exit code 4 only when `audio.device.required` |
| device lost while running (unplugged, driver reset, `AAUDIO_ERROR_DISCONNECTED`) | miniaudio stop notification on the control thread | LOST: the null pacer takes over within one block period, the mix continues silently, retries 0 s, 1 s, 2 s, 4 s, 8 s, then every 8 s |
| default endpoint changed (Windows) | miniaudio reroute notification | REROUTING; same layout: continues; new channel count or rate: reopen with a 10 ms fade-in |
| output route changed (Android headset, Bluetooth) | AAudio disconnect | treated as loss: reopen on the new default route |
| corrupt `.zsnd` | sound loader checks | the asset activates as the NULL_OBJECT fallback (silence, length 0); `audio.content` warning |
| corrupt `.zbank` | bank loader checks | the asset fails (kind policy FAIL); its events are unknown; the master bank failing at start uses the built-in master-only layout |
| unknown event | Audio.Update | counted, logged once, nothing plays |
| allocation failure on a control thread | `TryAllocate` | the operation fails with `AUDIO_ERROR_OUT_OF_MEMORY`: a post is refused and counted, a stream is not opened, a cache fill is skipped |
| memory pressure WARNING | governor callback | prebuffers to minimum (2,048 frames), decoded-sample cache purged to 50% |
| memory pressure CRITICAL | governor callback | virtual voices stopped; banks with no playing voice released (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding") |
| stream starvation | device thread underrun record | silence for the missing frames; the stream's I/O priority raised to CRITICAL; a starved stream for 500 ms is virtualised |
| stream read failure | decode thread, read FAILED: a READ_FAILED event drained by `Audio.Mix` M1 | the stream's voice ends with a 5 ms fade and the slot closes; `audio.stream` warning; the instance finishes |
| decode failure | codec returns an error: a DECODE_FAILED event | as a read failure |
| mixer callback overrun | callback timing | counted; a breach of Z2-B-AUDIO-003; the output still plays (the backend inserts silence) |
| command queue full | append | DROP_NEWEST, `audio.commands.dropped_delta` |
| feedback ring full | device thread `TryPush` false | record dropped and counted; `Audio.Mix` step M1 recovers voice ends from the slot-state table (`audio.feedback.reconciled_delta`); a start stays listed until acknowledged by its record or by the slot-state table showing its generation playing, fading or ended, so a dropped "started" record is reconciled too |
| focus loss on Android | focus bridge | INTERRUPTED (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio focus"); after a permanent loss focus is requested again at the next window focus gain or resume |
| app background | `OnStopAudio` | SUSPENDED: the device stopped, the mixer clock paused |
| ninth audio world | AttachWorld | audio disabled for that world (queue and node registered, no voice tables), logged once; enabled at the next `DetachWorld` of an enabled world |

### Missing and corrupt content

Every cooked section passes the checks of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks" and "Bank file checks" before any field is used; the parsers are fuzzed (§ "Sanitizer and fuzz runs"). Declared lengths are checked against the payload, channels against 8 and rates against 192 kHz before any arithmetic on them (`Zenith2/Docs/Architecture.md` → "Parser limits").

### Allocation failure and memory pressure

The device thread never allocates; every buffer it touches is allocated by the control side before it is published and freed only after the device's released epoch shows that no live or fading device voice can still read it. Control-side allocation uses `Zenith_Heap::TryAllocate` and fails the operation on null; the creation of `Zenith_AudioSystem` unwinds on every failing step and the sweep test `Test_AudioSystem_Create_FailsAtEachStep_Unwinds` exercises each.

### Device loss and change

The device state machine (`Zenith2/Docs/Design/AUDIO/Device.md` → "Device state machine") makes the mix independent of the device: the mixer clock advances on whichever thread calls it, voices keep their playheads, and a reopen resumes with a 10 ms fade-in. Android-specific changes (route, focus, lifecycle) are in `Zenith2/Docs/Design/AUDIO/Device.md` → "Android output".

### Stream starvation

`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream priorities and starvation".

### Budget breaches

| Row | Breach code | Behaviour and the forcing test |
|---|---|---|
| Z2-B-AUDIO-001 | BR-GOVERNOR | the governor raises the level; AUDIO sheds; `Test_AudioMemory_SheddingAtEachLevel` |
| Z2-B-AUDIO-002 | BR-ACCEPT | stage acceptance blocked; measured by the AudioMixer benchmark and MP-CPUZONE |
| Z2-B-AUDIO-003 | BR-ACCEPT, hard | one overrun is a breach ([Z2-N-BUDGET-231]); `Test_AudioMixer_OverrunCounted` forces an overrun with the scripted backend |
| Z2-B-AUDIO-004 | BR-DEFER, hard | voices beyond the real-voice limit stay virtual and are re-evaluated every frame; a slot that becomes free goes to the longest-deferred candidate, whatever its instance's age, while hysteresis and minimum real time keep the real voices (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm"); `Test_AudioVoices_RealLimitDefersExcess` |
| Z2-B-AUDIO-005 | BR-ACCEPT | the walkthrough's acceptance |
| Z2-B-SCRIPT-023 | BR-MERGE | SCRIPT's merge rejects; AUDIO's consumer never sees the rejected trigger; `Test_AudioScript_TriggerCapacityRejectsAtMerge` |

## Test strategy

### Test files and suites

| Suite | Location | Gate step |
|---|---|---|
| unit tests | `Zenith2/Engine/ZenithAudio/Tests/` (`ZenithAudio_Tests`) | `Test` (win-debug, win-release), `Sanitizers -Preset win-asan` |
| integration suite | `Zenith2/Tests/Audio/` | `Test` |
| fuzz targets | `Zenith2/Engine/ZenithAudio/Tests/Fuzz/` | `Sanitizers -Preset win-asan` (corpus run) |
| benchmarks | `Zenith2/Benchmarks/AudioMixer/`, `Zenith2/Benchmarks/AudioCodec/` | `--smoke` in `Test`; full runs in budget runs |
| cook tests | `Zenith2/Tools/ZenithCook/Tests/` (the audio step cases) | `Test` |
| authoring tests | `Zenith2/Tools/Authoring/Tests/` (`ZenithAuthoring_Tests`: the audio document types) | `Test` |
| device runs | the S24 and the reference desktop, recorded on the stage acceptance ticket | stage acceptance (`needs-human`) |

Every named test, the case it asserts and its file are listed in `Zenith2/Docs/Design/AUDIO/Tests.md`; test names follow `"ZenithAudio/<Type>/<Behaviour>"` ↔ `Test_<Type>_<Behaviour>` (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates").

### Test gates

`Configure`, `Build -Preset win-debug|win-release|android-arm64`, `Test`, `SourceLayout`, `Libgraph` (no module edge, miniaudio and codec confined), `ForbiddenCheck` (no runtime linking), `ApiContractCheck`, `HeaderSelfContained`, `Format`, `Complexity`, `Sanitizers -Preset win-asan`, and `ApkCheck` in its current form (16 KB alignment). The real-time include whitelist is AUDIO's own unit test, not a gate step. The Java focus class's presence in the APK is not yet a gate assertion: it is a PROC hand-off and X233 (b) (assumption pending MOD and PROC in § "Assumed interfaces pending other areas"); until it lands, `Test_AudioFocusBridge_JavaMatchesNativeRegistration` and device run AUDIO-DR-03 are the evidence.

### Bit-exact offline mixing

The offline renderer drives the same `Zenith_AudioMixer` from simulation time instead of a device: each simulation tick renders `48000 / tickRate` frames (a tick rate that does not divide 48,000 carries the remainder frame by frame). Bit-exactness is asserted within one build and platform; the cross-platform comparison (Windows x64 against the S24) is recorded as a measured report, not gated (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer arithmetic rules").

<!-- elaboration -->
**Offline servicing makes it deterministic by construction.** Under every other backend three things happen on wall-clock time: the decode thread's refills and cache fills, ASSET's reads, and asset activations (banks, dialogue lines, locale variants, audition banks). Each changes what the control side decides: whether a streamed voice is a candidate (Mixer.md: a streamed candidate needs its stream slot "in state PLAYING", which only a drained PREBUFFERED event sets), whether a cache-eligible voice plays from the cache or a memory stream, and when a bank's events are known. Under the OFFLINE backend none of them is left to timing:

1. No decode thread is created (creation step 4 is skipped for `AUDIO_BACKEND_OFFLINE`) and `Audio.Mix` posts no decode semaphore, so nothing refills a stream ring or fills the cache concurrently with the mix or the render. The decode request ring and the decode event ring still exist: `Audio.Mix` produces requests and consumes events exactly as on every other backend, and `ServiceBeforeFrame` takes the decode thread's two roles (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"), handed over because each `ServiceBeforeFrame` returns before the frame it precedes starts and that frame completes before the next `ServiceBeforeFrame` begins, both on the owner thread.
2. Before each frame's `Audio.Update` and `Audio.Mix`, the owner thread calls `Zenith_AudioOfflineRenderer::ServiceBeforeFrame()` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header"), on the main thread outside frames or on a test or tool thread that is not a job worker. It performs, in this order, each step to completion:
   0. pop every decode request `Audio.Mix` pushed in the previous frame, in ring order, and apply it to the slot's or entry's private state exactly as the decode thread's loop step 2 does (`Zenith2/Docs/Design/AUDIO/Device.md` → "Decode thread loop");
   a. every asset load AUDIO itself requested and has not yet seen complete (the issued-load list of `Audio.Mix` steps M5 and M7: dialogue lines, locale variant banks, audition banks), in request order: `Zenith_AssetLoader::ServiceUntilComplete(load, 5000)`; the loaders' activations push to the loader hand-off, which the coming `Audio.Mix` drains in M2;
   b. every stream slot with private refill state, in ascending slot order: wait for its outstanding read with the read source's `ServiceUntilComplete(xRead, 5000)`, which the production read source forwards to `Zenith_AssetReader::ServiceUntilComplete` (below), then run the refill algorithm (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm") on the owner thread, submitting and servicing each next read in turn (a memory stream reads nothing), until fewer than P frames of room remain in the ring (P as that algorithm defines it) or a non-looping sound has no frame left, and push the slot's decode events (OPENED, PREBUFFERED, STARVED_RECOVERED, a failure, CLOSED); a slot in PREBUFFERING or SEEKING therefore always has its PREBUFFERED event in the ring before the mix, whose M1 drains it before M6 selects, and a CLOSING slot is serviced only to its read's terminal status and answered CLOSED;
   c. every pending cache-fill job in request order, decoded whole (no 2 ms cap), each answered CACHE_FILLED or CACHE_FAILED;
   d. a timeout or failure in a, b or c ends the affected voice exactly as the live path does (`AUDIO_ERROR_IO` or `AUDIO_ERROR_DECODE`), counts `audio.offline.service_timeouts` for a timeout, and makes `GetOutputHash` report the run as not bit-exact (`Zenith_AudioOfflineRenderer::IsDeterministic` false), so a test asserts on a failed servicing rather than on a flaky hash.
3. Then the frame runs (`UpdateWorld` per world and `RunMix`, through the frame graph or directly) and `Render` renders the frame's frames from the rings filled in step 2. Within the frame nothing refills a ring; a ring holds at least `audio.streams.ringFrames` − P frames at frame start and a frame renders at most 0.25 s, so a stream underruns only when its pitch ratio × frames exceeds the ring's guaranteed fill F − P (F = `audio.streams.ringFrames`), which is itself a deterministic function of the snapshot and the command history.

So at every `Audio.Mix` the inputs are: the command history, the component inputs, the frame times, and asset state that is complete for every request issued earlier — no wall-clock quantity. Requests issued by other areas (a scene loading a bank) are deterministic in these tests because the scenarios load them before the first rendered frame; a capture of a running game (the editor's render-to-file) is deterministic for AUDIO's own requests and as deterministic as ASSET's servicing for the rest.

The read wait of step b is ASSET's answer to row AU26: `Zenith_AssetReader::ServiceUntilComplete(const Zenith_OperationHandle& xRead, uint32 uTimeoutMs)`, with the rules of `Zenith_AssetLoader::ServiceUntilComplete` (`ASSET_ERROR_INVALID_REQUEST` inside any job body, `ASSET_ERROR_TIMEOUT` with the read still running, the read's error record on FAILED or CANCELLED), declared and implemented by Z2T-ASSET-029 (Stage 2); the loader's member is not widened. `Zenith_AudioAssetReadSource::ServiceUntilComplete` calls it and reports `ASSET_ERROR_TIMEOUT` as step d's timeout. The ticket that implements the offline backend and `ServiceBeforeFrame` is blocked by Z2T-ASSET-029.

<!-- assumption: pending ASSET -->
Z2T-ASSET-029's same-diff design update adds `Zenith_AssetReader::ServiceUntilComplete` to `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetReader header"; until that ticket lands, the member is specified by the ticket and by row AU26's answer, not yet by ASSET's design text.
<!-- /assumption -->

**The consumption-log replay restores what the device read.** `Test_AudioOffline_MatchesDevicePathGivenConsumptionLog` runs the null pacer path, records per block the snapshot epoch the block applied and every stream underrun (stream slot, missing frames) (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Consumption log record"), archives every published snapshot, and keeps the live system alive with retirement suspended (`Zenith_AudioSystemTestAccess::SuspendRetirement`), so resident sample data and cache entries the archived snapshots point at stay valid and unchanged (they are immutable once published). The replay renders block by block (`RenderBlocks` with 256 frames per call, so the block FIFO never carries frames between calls) and applies the archived snapshot of each recorded epoch at its block. Before the first block, every archived voice record whose source kind is 4 (stream ring) is rewritten to source kind 5, the test-only replay feed (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Voice record"), pointing at a `Zenith_AudioReplayFeed` (`Internal/RealTime/Zenith_AudioReplayFeed.h`: a linear int16 buffer the test owns, a read cursor, and m, the underrun frames of the current block). The buffer holds a synchronous decode of that stream's sound from the archived voice's start offset (the decoder's output is a pure function of the packet bytes and the seek entry, so the frames equal those the live decode thread pushed). Before each block the replay sets each feed's m from the consumption log's kind-1 records of that block for its stream slot (0 when there is none). The device-side fetch rule of kind 5, in `Internal/RealTime/`: for a demand of d frames in a block, serve min(d − m, the frames left in the buffer) frames from the buffer at the cursor, then silence for the rest of the demand, and advance the cursor by d, so the m missing frames are skipped exactly as the live voice skipped them when they arrived. The stream-ring path does not run in the replay, so its skip logic cannot skip the missing frames a second time, and the real-time include and identifier scan covers the feed's code like every other file under `Internal/RealTime/`. The output hash equals the live run's.

**Tests.** `Test_AudioOffline_BitExactAcrossRuns` renders a scripted 10 s scenario (generated signals, 64 voices, every DSP effect, a streamed music transition with two segments, a streamed dialogue line requested on demand, two cache-eligible RESIDENT_COMPRESSED sounds posted before their cache fills, and a locale variant bank switch) twice, each run with the scripted read source (`Zenith2/Docs/Design/AUDIO/Tests.md` → "Scripted read source") jittered from a different seed (stream reads completing 0 to 20 ms of wall time after submission and out of request order; asset loads go through the real `Zenith_AssetLoader`), and asserts identical `Zenith_Hash64` of the output and `IsDeterministic`; `Test_AudioOffline_BitExactAcrossWorkerCounts` repeats it with 1, 2 and 8 job workers; `Test_AudioUpdate_ThreeWorldsBitExactAcrossWorkerCounts` does so with three attached worlds; `Test_AudioOffline_ServiceTimeoutReported` makes the scripted read source withhold one read past 5,000 ms and asserts the voice ends, the counter and `IsDeterministic` false; `Test_AudioOffline_MatchesDevicePathGivenConsumptionLog` (above) runs a scenario with a streamed voice and one forced underrun.

### Headless null-device path

`Test_AudioSystem_HeadlessUsesNullPacer` starts the module headless: no miniaudio context is created, `Zenith.AudioNul` mixes at real-time rate, 300 ticks of posts produce voices, virtualisation and budget counters identical to a windowed run of the same scenario with the null backend, and `Test_Runtime_HeadlessSkipsDevice` (MOD's) stays green with Audio selected. `Test_AudioDevice_MiniaudioNullBackendRunsCallback` exercises the real miniaudio device path with miniaudio's null backend in CI.

### Device runs

| Run | Platform | Asserts |
|---|---|---|
| output and latency | S24, reference desktop | measured output latency recorded; a 1 kHz click train captured through a loopback cable or the device's own capture matches the expected period |
| headset and Bluetooth route change | S24 | reroute within 500 ms; no crash; continuous playback after the change |
| audio focus: phone call, alarm, other media app | S24 | INTERRUPTED and resume per § "Audio focus" of Device.md; the S24 audio-interruption acceptance item (Z2-I-AUDIO-13) |
| background and foreground | S24 | SUSPENDED and resume; no audio while backgrounded |
| endpoint change and unplug | reference desktop | REROUTING and LOST paths |
| 30 min soak | S24 | zero callback overruns at the slice's voice load (Z2-B-AUDIO-003) |

### Sanitizer and fuzz runs

`win-asan` runs every unit test and the four fuzz targets over their seed corpora (seeds generated by the cook tests from generated signals, never committed audio clips). ZenithAudio has no `linux-tsan` tree; the concurrency primitives it relies on are CORE's and carry CORE's `linux-tsan` stress tests (§ "Assumed interfaces pending other areas"); AUDIO's own cross-thread protocol is tested by `Test_AudioRtChannels_StressSnapshotAndFeedback` under win-asan with 10^6 frames, the `Audio.Mix` ↔ decode thread channels by `Test_AudioStream_MixDecodeChannelStress` under win-asan with 10^5 open, seek and close cycles, and the loader hand-off by `Test_AudioBankLoader_ConcurrentActivationsHandOff`.

### Breach-forcing tests

Per [Z2-N-BUDGET-041]: `Test_AudioVoices_RealLimitDefersExcess` (BR-DEFER), `Test_AudioMemory_SheddingAtEachLevel` (BR-GOVERNOR), `Test_AudioCommandQueue_FullDropsNewest` (the pending BR-DROP row), `Test_AudioScript_TriggerCapacityRejectsAtMerge` (BR-MERGE of Z2-B-SCRIPT-023), `Test_AudioMixer_OverrunCounted` (the hard callback row).

## Budget rows

### Rows AUDIO owns

| ID | Name | Status | Procedure | Platform | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|
| Z2-B-AUDIO-001 | Audio banks, streams and mixer | pending | MP-SUBMEM, `MEMORY_SUBSYSTEM_AUDIO` | Win, S24 | WS-SLICE | BR-GOVERNOR | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-002 | Audio update CPU | pending | MP-CPUZONE on `Audio.Update` (summed over worlds) | Win, S24 | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-003 | Audio mixer callback CPU per period | pending | MP-COUNTER: AUDIO's callback-timing instrument read as counters, statistic MAXIMUM over the run, unit "% of the audio the callback rendered" ([Z2-N-BUDGET-231], X235 (b) and X275; § "Measuring the mixer callback row") | Win, S24 | WS-SLICE | BR-ACCEPT (0 tolerance, hard) | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-004 | Real voice count | pending | MP-COUNTER on `audio.voices.real_peak` | Win, S24 | WS-SLICE | BR-DEFER (hard) | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-005 | Walkthrough complete audio workflow | pending | MP-WALKTHROUGH | Win | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |

Z2-B-AUDIO-001 is a member of the `MEM-WIN-RAM` allocation sum and of the S24 total; Z2-B-AUDIO-002 is a member of `CPU-SIM`; Z2-B-AUDIO-003 is excluded from the frame bounds because the mixer thread has its own hard row (`Zenith2/Docs/Budgets.md` → "Allocation sums").

### Rows AUDIO is measured against

| ID | Name | How AUDIO contributes |
|---|---|---|
| Z2-B-SCRIPT-023 | Audio trigger capacity per tick | the capacity class AUDIO's triggers consume; sizes `audio.commands.capacity` |
| Z2-B-SCRIPT-026 | Per-source asynchronous request capacity in flight | the audio notification source of Stage 4 (assumption pending SCRIPT) |
| Z2-B-JOBS-002 | Bounded catch-up fixed steps per frame | multiplies the per-tick trigger capacity into the per-frame queue size |
| Z2-B-CORE-001, Z2-B-CORE-002 | S24 process RSS, Windows process RAM | the audio subsystem's share |
| Z2-B-SHIP-001 | Frame time p95 | `Audio.Update` runs in the frame |
| Z2-B-PACK-001 | AAB base module download size | the codec library and cooked audio; a D-SEL-CODEC criterion |
| Z2-B-RPROD-027 | Input-to-photon latency | AUDIO's output latency is reported beside it for A/V comparison, not summed into it |
| Z2-B-EDIT-004 | Asset reimport | reimport of an audio source through CONTENT's importer and `audio.sound` |

### Pending new rows

Rows this design needed that `Zenith2/Docs/Budgets.md` did not hold, each with the producer "Stage 1 per-system budget ticket for AUDIO" (cross-area row to BUDGET). Cross-doc item X235 (a) added them: Z2-B-AUDIO-006 and Z2-B-AUDIO-012 in `Zenith2/Docs/Budgets.md` → "Latency and duration rows" ([Z2-N-BUDGET-466]) and Z2-B-AUDIO-007 to Z2-B-AUDIO-011 in `Zenith2/Docs/Budgets.md` → "Engine service CPU rows" ([Z2-N-BUDGET-469]):

Every column of `Zenith2/Docs/Budgets.md` → "Per-system rows" is given; values are the Stage 1 ruling's (status `pending` until it rules):

| Row | Unit | Procedure | Statistic | Tolerance | Platform | Workloads | Breach | Status | Producer |
|---|---|---|---|---|---|---|---|---|---|
| Z2-B-AUDIO-006 Audio output latency | ms | MP-COUNTER on `audio.device.latency_us`, read once per 100 ms after a 10 s warm-up (each reading is already the control thread's median of its last 9 device measurements, `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency measurement") | p95 of those readings over the run | 10% | Win (wired), S24 (speaker) | WS-SLICE | BR-ACCEPT | pending | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-007 Audio stream decode CPU | ms per frame | MP-COUNTER on `audio.decode.us_delta` | p95 over frames | 10% | Win, S24 | WS-SLICE | BR-ACCEPT | pending | same |
| Z2-B-AUDIO-008 Streamed voice count | count | MP-COUNTER on `audio.streams.active` | maximum | 0 (hard) | Win, S24 | WS-SLICE | BR-DEFER | pending | same |
| Z2-B-AUDIO-009 Audio instance capacity per world | count | MP-COUNTER on `audio.instances.live` | maximum | 0 (hard) | Win, S24 | WS-SLICE | BR-REFUSE | pending | same |
| Z2-B-AUDIO-010 Audio command queue capacity per frame | commands | MP-COUNTER on `audio.commands.high_water` | maximum | 0 (hard) | Win, S24 | WS-SLICE | BR-DROP | pending | same |
| Z2-B-AUDIO-011 Audio occlusion tests per frame | rays | MP-COUNTER on `audio.occlusion.rays_delta` | maximum | 0 (hard) | Win, S24 | WS-SLICE | BR-DEFER | pending | same |
| Z2-B-AUDIO-012 Audio stream underruns per run | count | MP-COUNTER on `audio.streams.underruns` | total over the run | 0 (hard) | Win, S24 | WS-SLICE | BR-ACCEPT | pending | same |

### The Stage 1 AUDIO budget ruling

AUDIO is in the "no harness" group of `Zenith2/Docs/Budgets.md` → "Per-system producer shape": the `needs-human` ruling ticket "Stage 1 per-system budget ticket for AUDIO", blocked by the CORE, JOBS and RPROD rulings, allocates Z2-B-AUDIO-001 to Z2-B-AUDIO-005 from published and reference-engine data (published mixer costs of comparable middleware on mobile SoCs, the S24's AAudio low-latency burst sizes, ContentSpecs' audio table), commits `Zenith2/Docs/Research/Budgets/Stage1/AUDIO/allocation.json` and its report, reconciles the memory row with `Zenith2/Docs/ContentSpecs.md`'s `## Audio` table ([Z2-N-BUDGET-416]), writes the adopted real-voice values into AUDIO's platform and quality-tier config fragments (`audio.voices.realMax`, citing the row ID in the fragment's comment member), and appends its ruling to `Zenith2/Docs/DecisionLog.md` in its own diff (Z2-D-097). It also rules the pending rows above and writes the value of `audio.mixer.overrunSharePercent` (the share Z2-B-AUDIO-003 allows) into AUDIO's platform fragments, citing the row.

### Measuring the mixer callback row

<!-- elaboration -->
MP-CPUZONE uses profiler zones, which the device thread may not call, so Z2-B-AUDIO-003 is measured by AUDIO's own instrument. The measured quantity, per callback c:

share(c) = d(c) / a(c)

with d(c) the callback's duration (`Zenith_GetMonotonicTimeNs` at entry and exit: a vDSO or `QueryPerformanceCounter` read, no lock) and a(c) the duration of the audio the callback produced work for: b(c) × 256 / 48,000 s when it rendered b(c) ≥ 1 blocks, and f(c) / 48,000 s (the frames it delivered) when it rendered none and only copied from the FIFO.

Why the rendered audio and not the callback period: with the 256-frame block FIFO, a backend that calls back with fewer frames than a block (AAudio delivers one burst per call, well under 256 frames on the S24) renders a whole block in some callbacks and none in others. Dividing such a callback's work by its own short period would report more than 100% with no underrun anywhere, so a hard row with zero tolerance would breach by construction. The device keeps playing as long as the mixer produces audio faster than real time, which is share < 100% averaged over the device buffer; AUDIO guarantees the buffer covers one block plus one burst (`Zenith2/Docs/Design/AUDIO/Device.md` → "Buffer sizing and xrun adaptation"), so one block's render may straddle several short callbacks without an underrun. A real underrun is still counted separately as `audio.device.underruns` (the backend's xruns).

One threshold: `audio.mixer.overrunSharePercent` (config, default 100 until the Stage 1 ruling writes the row's value) is both the breach of Z2-B-AUDIO-003 and the definition of an overrun: `audio.mixer.overruns` counts callbacks with share(c) above it. The instrument stores, with relaxed atomics, the maximum share since the previous publication, the maximum since `Start`, the maximum d(c) and the overrun count; `Audio.Mix` publishes them as `audio.mixer.callback_share_max`, `audio.mixer.callback_share_run_max`, `audio.mixer.callback_us_max` and `audio.mixer.overruns`. The row's statistic is the MAXIMUM of share(c) over the run (`audio.mixer.callback_share_run_max` at the run's end), with zero overruns; the row's procedure is MP-COUNTER over this instrument ([Z2-N-BUDGET-231], cross-doc item X235 (b)). `Test_AudioMixer_CallbackShareWithShortBursts` drives the scripted backend with 96-frame callbacks and a scripted clock: a block costing 3 ms reports 56.25% (3 / 5.333) and no overrun, although each 96-frame period is 2 ms; a block costing 6 ms reports 112.5% and one overrun.

## Stage plan

### Stage 1

- The Stage 1 AUDIO budget ruling (§ "The Stage 1 AUDIO budget ruling").
- AUDIO consumes D-SEL-HASH (open, Stage 1) for event IDs.
- Pull-forward candidate into the Stage 1 sprint (fix version Stage 3): the `Zenith_AudioMath.h` functions and their tests (ZenithCore only). The `.zsnd` and `.zbank` parsers and their fuzz targets need ASSET's header and loader types, which exist since Stage 2, so they belong to the Stage 2 API slice's follow-up, never Stage 1.

### Stage 2

- D-SEL-CODEC: the harness ticket (the AudioCodec benchmark) and the `needs-human` ruling ticket (§ "D-SEL-CODEC candidates and criteria").
- API-only slice: `Zenith_AudioTypes.h`, `Zenith_AudioAssets.h`, `Zenith_AudioComponents.h`, `Zenith_AudioCommands.h`, `Zenith_AudioScriptApi.h` with contract blocks and no implementation, so ENV, CINE, UI, ANIM and GAME tickets can reference the names.

### Stage 3

The Stage 3 deliverable "audio" of `Zenith2/Docs/Roadmap.md` → "Stage 3 Playable authoring slice", in this order:

1. The library target, miniaudio integration, the device backends (miniaudio, null, offline), the control and decode threads, the real-time channels (blocked by CORE's `Zenith_SpscRing` and `Zenith_TripleBuffer`), `Audio.Update` and `Audio.Mix` (blocked by the ZenithRuntime ticket that implements `AddEngineEpilogueStage` ([Z2-N-ARCH-442]), `RegisterFrameListener` and `GetSimulationClock` ([Z2-N-ARCH-879]); there is no single-world interim form).
2. The mixer: block engine, buses, sends, the DSP chain with the biquads, EQ, compressor, limiter and delay; user volume categories; output layouts and downmix.
3. Sounds and banks: both loaders, PCM16 and ADPCM decode, the decoded-sample cache, the cook steps for sounds and banks (events, the mixer document, attenuation presets).
4. Events, instances, voices, voice budgets, priorities and stealing, audibility; spatialisation (attenuation, cones, Doppler, panning to stereo, 5.1 and 7.1).
5. Streaming with the D-SEL-CODEC codec; memory accounting and shedding.
6. Commands, `Zenith_AudioCommandQueue`, `Audio.Update`, the components; the Luau namespace (posts, stops, parameters) and the script consumer.
7. The device state machine on Windows and Android: device changes, loss and retry, background, Android audio focus with its Java bridge.
8. Real-time contract enforcement (source set scan, allocation-count tests), counters, profiler rows, log categories, capture.
9. The offline renderer and the bit-exact tests (blocked by Z2T-ASSET-029, which adds `Zenith_AssetReader::ServiceUntilComplete`, row AU26); the headless null-device path.
10. Device runs: S24 focus and interruption, route change, latency; reference desktop endpoint change.

### Stage 4

The Stage 4 audio items: occlusion, reverb (FDN aux returns, zone reverb), acoustic zones and portals, music transitions and stingers, dialogue and localized audio (locale variant banks), virtual-voice behaviours beyond S3's (pause and restart modes, streamed virtual voices), animation/audio clock sync (trigger scheduling, time-locked voices), event bridges (constant-value weather bridges; the value-reading form after ENV adds its weather values), Luau notifications (`ASYNC_SOURCE_AUDIO` through MOD's module publication sink), mix snapshots and ducking, the editor surfaces, auditioning, audition in context, and the complete audio workflow with its walkthrough (Z2-B-AUDIO-005).

### Stage 5

Quality-tier integration: `audio.reverb.quality`, the occlusion ray cap and the real-voice cap per tier land in AUDIO's quality fragments as the tiers of the "Audio features" section of `Zenith2/Docs/QualityMatrix.md` (pending, written by the Stage 1 QualityMatrix ticket) are fixed.

### Stage 6

Shipping qualification: the 30 min S24 soak with zero callback overruns, the memory-pressure device run with audio shedding, package size against Z2-B-PACK-001 with the codec and cooked audio, the final latency measurement.

### Stage 7

Nothing is planned for AUDIO; object-based spatial audio and HRTF remain excluded (`Zenith2/Docs/Requirements/AUDIO.md` → "Excluded and deferred").

### API-only slices

| Slice | Stage | Unblocks |
|---|---|---|
| `Zenith_AudioTypes.h`, `Zenith_AudioAssets.h` | 2 | ASSET's kind table rows, CINE, UI |
| `Zenith_AudioComponents.h` | 2 | ENV's weather prefabs (EN13), STREAM's audio role, GAME's camera listener |
| `Zenith_AudioCommands.h`, `Zenith_AudioScriptApi.h` | 2 | CINE's audio tracks, SCRIPT's generated types |
| `Zenith_AudioSystem.h` (audition and meters) | 3 | EDIT's audio panels |

## Decisions

### Settled decisions governing AUDIO

| ID | Governs |
|---|---|
| Z2-D-024 | Audio is an optional, statically linked module selected in `game.json` (the no-module-edge rule AUDIO also follows is [Z2-N-ARCH-074], not this decision) |
| Z2-D-019 | determinism: audio is presentation; nothing simulation-visible depends on the device, voices or meters |
| Z2-D-053 | asynchronous audio notifications become visible at tick boundaries in completion-ID order |
| Z2-D-080 | phases: the bridge runs in phase 9, Audio.Update is an epilogue stage outside the phase schema |
| Z2-D-016, Z2-D-051 | script triggers are side effects of an entity transaction, committed or rolled back with it |
| Z2-D-038 | JSON authoring documents, versioned binary cooked files |
| Z2-D-048 | document migration, cooked re-cook |
| Z2-D-040, Z2-D-041 | ownership idioms; the lock-free channels need justification records |
| Z2-D-045, Z2-D-046 | results not exceptions; no mutable global state (miniaudio and the JNI bridge hold no statics; the JNI native methods receive their owner as a `jlong`) |
| Z2-D-082 | miniaudio and the codec pinned, integrated, confined to `Internal/` |
| Z2-D-004 | COMPRESS owns block codecs; audio codecs are AUDIO's |
| Z2-D-010 | Windows and the S24 as the platforms whose devices AUDIO targets |
| Z2-D-012 | the budget rows |
| Z2-D-005 | the on-screen profiler rows |
| Z2-D-077 | dialogue decisions are game Luau; AUDIO plays lines |
| Z2-D-079 | audio authoring is visual (editor surfaces over documents) |
| Z2-D-084, Z2-D-097 | the D-SEL-CODEC harness and ruling split; rulings appended in the ticket diff |
| Z2-D-002 | games contain no C++: every audio behaviour a game needs is reachable from documents and Luau |
| Z2-D-073 | networked multiplayer deferred to Stage 7: no networked audio or voice chat in v1 (headless execution without an audio device is [Z2-N-ARCH-867], not a decision) |

### Open decisions AUDIO depends on

| Decision | Stage | AUDIO's use |
|---|---|---|
| D-SEL-CODEC | open, Stage 2 | the streamed and compressed audio codec (AUDIO owns it) |
| D-SEL-HASH | open, Stage 1 | `Zenith_Hash64` for event, bus and parameter IDs |
| D-PHASES refinement | open, Stage 1 | the bridge's phase-9 placement |
| D-SCRIPT-TXN refinement | open, Stage 1 | the audio-trigger side-effect rules |

### Selection decisions AUDIO owns

AUDIO owns D-SEL-CODEC (`Zenith2/Docs/Architecture.md` → "Named selection decisions").

### D-SEL-CODEC candidates and criteria

| Aspect | Rule |
|---|---|
| candidates | (1) Opus (libopus, the `opusfile`-free raw packet path with AUDIO's own Ogg-free container); (2) Vorbis (libvorbis or stb_vorbis-class decoder) |
| harness ticket | an ordinary TASK building `Zenith2/Benchmarks/AudioCodec/`: decodes generated test signals (sines, swept sines, pink noise, impulses, synthesised speech-like formants from `Zenith_AudioTestSignals.h`) encoded at 64, 96 and 128 kbit/s stereo and 32, 48 kbit/s mono; measures decode CPU per voice per second of audio on the reference desktop and the S24 (single core, `THREAD_PRIORITY_HIGH`, 30 s per signal, 5 repetitions, median), seek time to 100 random positions with each seek's frame alignment and decoded-output error against a full decode (the sample-accuracy measure of `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Seeking and loop points", ruling Q92), the same measure at a loop point, and encoded size per minute; writes `Zenith2/Docs/Research/AudioCodec/report.json` |
| ruling ticket | `needs-human`: the human supplies a listening corpus (not committed; its file names and SHA-256 hashes go into the report), runs an ABX comparison at the candidate bitrates, and rules |
| quality criterion | an ABX run by the ruling's human at 96 kbit/s stereo and 48 kbit/s mono over the listening corpus: 16 trials per clip and bitrate per candidate against the PCM16 reference; a candidate fails a clip when the listener identifies X correctly in ≥ 12 of 16 trials (one-sided binomial p ≤ 0.038); a candidate failing more than 10% of the clips at 96 kbit/s stereo is ruled out; a candidate that passes at the lower bitrates as well is preferred on size |
| acceptance rule | among candidates passing the quality criterion: the codec whose decode CPU for the slice's streamed voice count fits the pending row "Audio stream decode CPU" (codec decode runs on `Zenith.AudioDec`, inside neither `Audio.Update` nor the mixer callback) on the S24, whose seeks are sample-accurate as `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Seeking and loop points" defines it (exact frame alignment and a decoded-output error of at most −60 dBFS after the pre-roll; bit-equality is required only of the stateless decoders, ruling Q92) with a median seek time within 5 ms, whose loops meet the same measure, whose licence is permissive (BSD-style, no copyleft), and whose package contribution fits Z2-B-PACK-001; a tie goes to the smaller decoder binary. The DecisionLog's elaborated rule names Z2-B-AUDIO-002 and Z2-B-AUDIO-003 for decode CPU; the ruling ticket amends those row references to the decode row in its own diff, as the log allows a ruling to amend an elaborated rule |
| consumers | AUDIO (streaming and decode), PACK (package size), CINE (dialogue and video audio) |
| what the ruling ticket edits | `Zenith2/Docs/DecisionLog.md` (the ruling), `Zenith2/ThirdParty/manifest.json`, `Zenith2/Engine/ZenithAudio/CMakeLists.txt` (`THIRD_PARTY`), and the codec ID of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Codec identifiers" that becomes valid in cooked files |

Both candidate IDs are reserved in the codec table now; only the ruled one is accepted by the loaders and the cook after the ruling, and `Zenith_AudioStreamDecoder` keeps the codec behind one interface, so the ruling changes one implementation file and two table rows.

### Rulings AUDIO records in its own tickets

| Ruling | Ticket stage | Question it settles |
|---|---|---|
| miniaudio configuration | 3 | the exact option defines of the pinned miniaudio release equal to § "ZENITH_SUPPORTED matrix and miniaudio configuration"; and, read from that release's source: which threads re-initialise a stream on a WASAPI default-device change and on an AAudio disconnect, whether the data callback can then arrive on a new thread, on which threads notification and log callbacks arrive, and whether miniaudio allocates outside init and uninit. The design is correct for every answer; the ruling records them so the threading table of `Zenith2/Docs/Design/AUDIO/Device.md` → "Thread table" names real threads |
| WASAPI period on the reference desktop | 3 | the shared-mode period miniaudio obtains (default 10 ms) and whether `audio.device.periodFrames` = 480 is honoured |
| AAudio burst and buffer on the S24 | 3 | the burst size `AAudioStream_getFramesPerBurst` reports at 48 kHz in low-latency shared mode and the steady-state buffer the xrun adaptation settles at |
| Android focus ducking and permanent loss | 3 | whether the S24 issues `AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK` to a game with `setWillPauseWhenDucked(false)` or ducks it automatically, and the resulting INTERRUPTED behaviour; and, for `AUDIOFOCUS_LOSS` while the game stays in the foreground, whether the re-request on `PLATFORM_EVENT_WINDOW_FOCUS_GAINED` restores audio on the S24; and, for a permanent loss taken while the game window keeps focus (a Bluetooth media button, a background playlist started from a notification or a wearable), which produces neither `PLATFORM_EVENT_WINDOW_FOCUS_GAINED` nor RESUME_AUDIO, which fallback ships: (a) re-request focus on the first `PLATFORM_EVENT_INPUT_TRANSITION` the frame listener delivers while INTERRUPTED with `m_bFocusLostPermanently` (at most once per 2 s), the design's default, because a touch means the user is playing again; (b) a periodic re-request every 30 s while foreground, rejected by default because it would interrupt the media the user chose; (c) none, staying silent until the user leaves and returns. The run of AUDIO-DR-03 records the behaviour of each case |

## Requirement index

<!-- REQ-INDEX -->

Every requirement of `Zenith2/Docs/Requirements/AUDIO.md` and the design headings that specify it:

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-AUDIO-001 | ZenithAudio library target | S3 | `Zenith2/Docs/Design/AUDIO.md` → "ZenithAudio target"; `Zenith2/Docs/Design/AUDIO.md` → "Public root headers"; `Zenith2/Docs/Design/AUDIO.md` → "Libraries AUDIO depends on" |
| Z2-R-AUDIO-002 | miniaudio confined configured and heap-routed | S3 | `Zenith2/Docs/Design/AUDIO.md` → "ZENITH_SUPPORTED matrix and miniaudio configuration"; `Zenith2/Docs/Design/AUDIO/Device.md` → "miniaudio backend" |
| Z2-R-AUDIO-003 | Module entry points and start registrations | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Module entry points"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Binding registration" |
| Z2-R-AUDIO-004 | Audio system creation unwinds on failure | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Creation steps"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach" |
| Z2-R-AUDIO-005 | Required audio device maps to exit code 4 | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach" |
| Z2-R-AUDIO-006 | Headless null-device path | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Headless null-device path"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Null pacer thread loop"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Backend selection" |
| Z2-R-AUDIO-007 | Audio worlds attach and the world limit | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"; `Zenith2/Docs/Design/AUDIO.md` → "Following the presenting world"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "World frame inputs" |
| Z2-R-AUDIO-008 | Shutdown and detach order | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Shutdown and detach"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement" |
| Z2-R-AUDIO-009 | Audio.Update runs once per world per frame | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Frame-graph placement"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step" |
| Z2-R-AUDIO-010 | Cross-world mixing in the engine-wide Audio.Mix stage | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm" |
| Z2-R-AUDIO-011 | Command queue drains in merge order | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommandQueue header"; `Zenith2/Docs/Design/AUDIO.md` → "Command record layout" |
| Z2-R-AUDIO-012 | Command queue full policy is drop newest | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioCommandQueue header"; `Zenith2/Docs/Design/AUDIO.md` → "Breach-forcing tests" |
| Z2-R-AUDIO-013 | Stage 3 audio command kinds and payload validation | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command kinds"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command payloads"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Recording helpers"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach" |
| Z2-R-AUDIO-014 | Stage 3 audio components registered | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component conventions"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component registration" |
| Z2-R-AUDIO-015 | Listener selection and attenuation position | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioListenerComponent"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space" |
| Z2-R-AUDIO-016 | Emitter reconciliation | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioEmitterComponent"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step" |
| Z2-R-AUDIO-017 | Bank residency through data | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank dependencies and residency"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioBankComponent" |
| Z2-R-AUDIO-018 | Audio configuration keys | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSettings" |
| Z2-R-AUDIO-019 | Platform configuration fragments | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Internal files"; `Zenith2/Docs/Design/AUDIO.md` → "The Stage 1 AUDIO budget ruling" |
| Z2-R-AUDIO-020 | Audio error enumeration and contract blocks | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Error enumerations"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioError"; `Zenith2/Docs/Design/AUDIO.md` → "Result rules" |
| Z2-R-AUDIO-021 | Deterministic audio maths | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Decibel and gain conversions"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Deterministic trigonometry" |
| Z2-R-AUDIO-022 | Audio identifiers are hashes of names | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Identifiers"; `Zenith2/Docs/Design/AUDIO.md` → "Handle and identifier kinds"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table" |
| Z2-R-AUDIO-023 | Sound file format version 1 and codec identifiers | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file sections"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound header section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "PCM16 sample section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "IMA ADPCM sample section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Codec identifiers" |
| Z2-R-AUDIO-024 | Sound file checks | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks" |
| Z2-R-AUDIO-025 | Bank file format version 1 | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank sections"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Event table section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Node table section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Embedded mixer section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Locale variant section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record" |
| Z2-R-AUDIO-026 | Bank file checks | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks" |
| Z2-R-AUDIO-027 | Sound kind loader | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Sound loader"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_SoundAsset" |
| Z2-R-AUDIO-028 | Bank kind loader and process event index | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_SoundBankAsset"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Event and parameter lookup"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Loader hand-off"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication" |
| Z2-R-AUDIO-029 | Master bank at start with built-in fallback | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Master bank at start"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Built-in buses" |
| Z2-R-AUDIO-030 | Load mode rule | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Load mode rule" |
| Z2-R-AUDIO-031 | Decoded-sample cache | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Decoded-sample cache" |
| Z2-R-AUDIO-032 | IMA ADPCM decoding | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "IMA ADPCM decoding"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "IMA ADPCM sample section" |
| Z2-R-AUDIO-033 | Stream decoder interface | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioStreamDecoder header"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Codec decoder interface" |
| Z2-R-AUDIO-034 | Sample-accurate seeking and seamless loops | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Seeking and loop points"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Seek table section" |
| Z2-R-AUDIO-035 | D-SEL-CODEC decode benchmark harness | S2 | `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Benchmarks" |
| Z2-R-AUDIO-036 | D-SEL-CODEC ruling | S2 | `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria"; `Zenith2/Docs/Design/AUDIO.md` → "Selection decisions AUDIO owns" |
| Z2-R-AUDIO-037 | Ruled codec integrated | S3 | `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Codec decoder interface" |
| Z2-R-AUDIO-038 | Stream slots state machine | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream start latency"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels" |
| Z2-R-AUDIO-039 | Stream refill through raw-range reads with incremental decode | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm" |
| Z2-R-AUDIO-040 | Stream starvation never blocks the device | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream priorities and starvation"; `Zenith2/Docs/Design/AUDIO.md` → "Stream starvation" |
| Z2-R-AUDIO-041 | Stream cancellation on close | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"; `Zenith2/Docs/Design/AUDIO.md` → "Cancellation"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels" |
| Z2-R-AUDIO-042 | Stream slot limit and stealing | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots" |
| Z2-R-AUDIO-043 | Virtual streamed voices release and reopen | S4 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Virtual streamed voices"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices" |
| Z2-R-AUDIO-044 | Audio memory consumer | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Memory consumer" |
| Z2-R-AUDIO-045 | Audio shedding under memory pressure | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding"; `Zenith2/Docs/Design/AUDIO.md` → "Allocation failure and memory pressure" |
| Z2-R-AUDIO-046 | Bus tree and block signal flow | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Bus hierarchy"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Signal flow per block"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step" |
| Z2-R-AUDIO-047 | Aux sends and returns | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Aux sends and returns" |
| Z2-R-AUDIO-048 | Mix snapshots | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mix snapshots" |
| Z2-R-AUDIO-049 | Ducking | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Ducking" |
| Z2-R-AUDIO-050 | User volume categories | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "User volume categories"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys" |
| Z2-R-AUDIO-051 | Output layouts and downmix | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Output layouts and downmix" |
| Z2-R-AUDIO-052 | Block FIFO serves any callback size | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread"; `Zenith2/Docs/Design/AUDIO/Device.md` → "AAudio configuration" |
| Z2-R-AUDIO-053 | Hermite resampler | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Resampler" |
| Z2-R-AUDIO-054 | Filters and bus effects | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Biquad filters"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Bus effects"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Equaliser"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Compressor"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Delay effect" |
| Z2-R-AUDIO-055 | Master limiter and final clamp | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Limiter and final clamp" |
| Z2-R-AUDIO-056 | Click-free ramps and fades | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Gain and pan ramps"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Fades and click suppression" |
| Z2-R-AUDIO-057 | Instances and voice tables | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Instances voices and slots" |
| Z2-R-AUDIO-058 | Deterministic event node evaluation | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Node table section" |
| Z2-R-AUDIO-059 | Audibility and priority ordering | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Audibility estimate"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Voice selection worked example" |
| Z2-R-AUDIO-060 | Real-voice limit defers excess voices | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"; `Zenith2/Docs/Design/AUDIO.md` → "Budget breaches" |
| Z2-R-AUDIO-061 | Hysteresis minimum real time and per-bus limits | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Real voice selection algorithm" |
| Z2-R-AUDIO-062 | Instance limits steal modes and cooldown | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Stealing and instance limits" |
| Z2-R-AUDIO-063 | Virtual voice playheads | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices" |
| Z2-R-AUDIO-064 | Virtual voice behaviours | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Virtual voices" |
| Z2-R-AUDIO-065 | Primary world and secondary worlds share the voice limit | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step" |
| Z2-R-AUDIO-066 | Listener space convention | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space" |
| Z2-R-AUDIO-067 | Distance attenuation curves | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Distance attenuation curves"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Attenuation presets" |
| Z2-R-AUDIO-068 | Air absorption and cones | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Air absorption"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Cones" |
| Z2-R-AUDIO-069 | Doppler | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Doppler"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Listener space" |
| Z2-R-AUDIO-070 | Stereo and headphone panning | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Panning to stereo and headphones" |
| Z2-R-AUDIO-071 | Surround panning | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Panning to 5.1 and 7.1" |
| Z2-R-AUDIO-072 | Spread stereo and multichannel sources | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Spread and source size"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Multichannel sources" |
| Z2-R-AUDIO-073 | Occlusion by occluder boxes | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occluder geometry"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion ray test"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion filter mapping" |
| Z2-R-AUDIO-074 | Occlusion test cap per frame | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Occlusion ray test" |
| Z2-R-AUDIO-075 | Feedback delay network reverb | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Reverb presets" |
| Z2-R-AUDIO-076 | Reverb quality per tier | S5 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb"; `Zenith2/Docs/Design/AUDIO.md` → "Stage 5" |
| Z2-R-AUDIO-077 | Acoustic zone selection | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Zone selection"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioZoneComponent" |
| Z2-R-AUDIO-078 | Portals and wall loss | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Portals and propagation"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioPortalComponent" |
| Z2-R-AUDIO-079 | Zone reverb and ambience | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb crossfade" |
| Z2-R-AUDIO-080 | Music segments states and transitions | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Music model"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Music section" |
| Z2-R-AUDIO-081 | Sample-accurate music transitions | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling" |
| Z2-R-AUDIO-082 | Music stingers | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Stingers" |
| Z2-R-AUDIO-083 | Dialogue line playback | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue lines"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Dialogue table section" |
| Z2-R-AUDIO-084 | Dialogue locale selection | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection" |
| Z2-R-AUDIO-085 | Locale variant banks | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Locale variant section"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader" |
| Z2-R-AUDIO-086 | Dialogue ducking | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Ducking" |
| Z2-R-AUDIO-087 | Caption feed for UI and cinematics | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step" |
| Z2-R-AUDIO-088 | Device clock and trigger scheduling | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "The device clock"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Scheduling a trigger"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Engine event types bridged"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Feedback record" |
| Z2-R-AUDIO-089 | Time-locked voices | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices" |
| Z2-R-AUDIO-090 | World pause and time scale | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Pause and time scale" |
| Z2-R-AUDIO-091 | AUDIO threads | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Thread table"; `Zenith2/Docs/Design/AUDIO.md` → "Threads AUDIO owns" |
| Z2-R-AUDIO-092 | Control thread owns the device | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Control thread loop" |
| Z2-R-AUDIO-093 | Device state machine | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "States"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Events"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table" |
| Z2-R-AUDIO-094 | Device loss hand-over and retry | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Retry schedule"; `Zenith2/Docs/Design/AUDIO.md` → "Device loss and change" |
| Z2-R-AUDIO-095 | Reroute and format change | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Endpoint changes on Windows"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table" |
| Z2-R-AUDIO-096 | Backend selection | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Backend selection"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Null backend"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Offline backend" |
| Z2-R-AUDIO-097 | Windows WASAPI output and endpoint changes | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "WASAPI configuration"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Endpoint changes on Windows"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records" |
| Z2-R-AUDIO-098 | Windows mute when unfocused | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Focus on Windows" |
| Z2-R-AUDIO-099 | Android AAudio output and buffer adaptation | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "AAudio configuration"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Buffer sizing and xrun adaptation" |
| Z2-R-AUDIO-100 | Android route changes | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Device changes on Android" |
| Z2-R-AUDIO-101 | Android audio focus behaviour | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio focus"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Events"; `Zenith2/Docs/Design/AUDIO.md` → "Rulings AUDIO records in its own tickets" |
| Z2-R-AUDIO-102 | Android focus Java bridge | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Focus Java bridge"; `Zenith2/Docs/Design/AUDIO.md` → "Internal Android files"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas" |
| Z2-R-AUDIO-103 | S24 audio interruption acceptance | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Device runs"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records" |
| Z2-R-AUDIO-104 | Lifecycle stop and resume | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Lifecycle on Android"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table" |
| Z2-R-AUDIO-105 | Output latency measured and reported | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency measurement"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency reporting"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Latency budget composition" |
| Z2-R-AUDIO-106 | No allocation on the device thread | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread" |
| Z2-R-AUDIO-107 | No locks waits logs or hooks on the device thread | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time source set enforcement"; `Zenith2/Docs/Design/AUDIO.md` → "Real-time source set" |
| Z2-R-AUDIO-108 | Lock-free channels and complete snapshots | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer snapshot layout"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record" |
| Z2-R-AUDIO-109 | Sample memory retirement by released epoch | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement" |
| Z2-R-AUDIO-110 | Floating-point environment on AUDIO threads | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Floating-point setup on the device thread" |
| Z2-R-AUDIO-111 | Snapshot application is bounds-safe | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Rules for code on the device thread" |
| Z2-R-AUDIO-112 | Mixer callback timing instrument and its one threshold | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Measuring the mixer callback row"; `Zenith2/Docs/Design/AUDIO.md` → "Counters" |
| Z2-R-AUDIO-113 | Thirty-minute S24 soak without overruns | S6 | `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records"; `Zenith2/Docs/Design/AUDIO.md` → "Stage 6" |
| Z2-R-AUDIO-114 | Offline renderer | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Offline backend" |
| Z2-R-AUDIO-115 | Bit-exact offline mixing | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer arithmetic rules"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource" |
| Z2-R-AUDIO-116 | Device path matches the offline path | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Consumption log record"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Test access" |
| Z2-R-AUDIO-117 | Audio is presentation only | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Determinism contract"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table" |
| Z2-R-AUDIO-118 | Cross-platform mixer hash comparison | S3 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Mixer arithmetic rules" |
| Z2-R-AUDIO-119 | Zenith.Audio Luau namespace at Stage 3 | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Script value types"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Binding registration" |
| Z2-R-AUDIO-120 | Script audio triggers are transactional | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script trigger payloads"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Script side-effect payloads" |
| Z2-R-AUDIO-121 | Audio trigger capacity enforced at the merge | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Script side-effect payloads"; `Zenith2/Docs/Design/AUDIO.md` → "Budget breaches" |
| Z2-R-AUDIO-122 | Audio-trigger side-effect consumer registration | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Binding registration"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach" |
| Z2-R-AUDIO-123 | Stage 3 script misuse raises catalogued errors | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes"; `Zenith2/Docs/Design/AUDIO.md` → "Luau errors versus asserts" |
| Z2-R-AUDIO-124 | Script audio notifications | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script examples"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas" |
| Z2-R-AUDIO-125 | Data-driven event bridges | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bridge declarations"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bridge system" |
| Z2-R-AUDIO-126 | Weather ambience through effect prefabs | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Engine event types bridged"; `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas" |
| Z2-R-AUDIO-127 | Region-bound emitters | S4 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Region-bound emitters" |
| Z2-R-AUDIO-128 | Cell unload fades without freeing playing memory | S4 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Cells unloading under playing voices" |
| Z2-R-AUDIO-129 | Origin shifts cause no Doppler | S4 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Origin shifts" |
| Z2-R-AUDIO-130 | Audio counters and profiler rows | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Counters"; `Zenith2/Docs/Design/AUDIO.md` → "On-screen profiler rows"; `Zenith2/Docs/Design/AUDIO.md` → "Telemetry" |
| Z2-R-AUDIO-131 | Audio log categories | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Log categories" |
| Z2-R-AUDIO-132 | Debug views frame dump and capture | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Debug views"; `Zenith2/Docs/Design/AUDIO.md` → "Frame-dump and capture entries"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Capture file" |
| Z2-R-AUDIO-133 | Meters and voice inspector | S4 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Status and meters"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Profiler and voice inspector" |
| Z2-R-AUDIO-134 | Audio cook step registration and validators | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Step registration"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Cooked file validation" |
| Z2-R-AUDIO-135 | Importer intermediate contract | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio importer boundary"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound import settings" |
| Z2-R-AUDIO-136 | Deterministic sound cook | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.sound step" |
| Z2-R-AUDIO-137 | Cook resampling loudness and normalisation | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook-time resampling"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Loudness measurement" |
| Z2-R-AUDIO-138 | Bank cook from documents | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step" |
| Z2-R-AUDIO-139 | Cook validation rules and the event index | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook validation rules"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.eventIndex step"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook error codes" |
| Z2-R-AUDIO-140 | Bank and mixer document types | S3 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types"; `Zenith2/Docs/Design/AUDIO.md` → "ZenithAuthoring AUDIO files"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Document migrations"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank document schema"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer document schema" |
| Z2-R-AUDIO-141 | Bank and event editor | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bank and event editor" |
| Z2-R-AUDIO-142 | Mixer console with live changes | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Mixer console" |
| Z2-R-AUDIO-143 | Audition panel | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition panel"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API" |
| Z2-R-AUDIO-144 | Audition in context | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition in context"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels" |
| Z2-R-AUDIO-145 | Complete audio workflow walkthrough | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Workflow steps"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Workflow walkthrough script" |
| Z2-R-AUDIO-146 | Stage 1 AUDIO budget ruling | S1 | `Zenith2/Docs/Design/AUDIO.md` → "The Stage 1 AUDIO budget ruling"; `Zenith2/Docs/Design/AUDIO.md` → "Pending new rows" |
| Z2-R-AUDIO-147 | Slice budget runs meet the AUDIO rows | S4 | `Zenith2/Docs/Design/AUDIO.md` → "Rows AUDIO owns"; `Zenith2/Docs/Design/AUDIO.md` → "Measuring the mixer callback row"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Benchmarks" |
| Z2-R-AUDIO-148 | Package size with the audio codec | S6 | `Zenith2/Docs/Design/AUDIO.md` → "Rows AUDIO is measured against"; `Zenith2/Docs/Design/AUDIO.md` → "Stage 6" |
| Z2-R-AUDIO-149 | S24 memory-pressure run with audio shedding | S6 | `Zenith2/Docs/Design/AUDIO/Tests.md` → "Device run records"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding" |
| Z2-R-AUDIO-150 | Loader hand-off to Audio.Mix | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Loader hand-off"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step" |
| Z2-R-AUDIO-151 | Event index published by pointer swap | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Event and parameter lookup" |
| Z2-R-AUDIO-152 | Dropped feedback is reconciled | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels" |
| Z2-R-AUDIO-153 | Audition voices are reserved | S4 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Priority ordering" |
| Z2-R-AUDIO-154 | Subtitle and caption keys reach UI's localization extraction | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Dialogue ducking and captions" |
| Z2-R-AUDIO-155 | Released epoch follows the last applied listing snapshot | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step" |
| Z2-R-AUDIO-156 | Main-thread setters reach Audio.Mix through two declared channels | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API" |
| Z2-R-AUDIO-157 | Offline servicing completes asynchronous work before every frame | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioOfflineRenderer header"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm" |
| Z2-R-AUDIO-158 | Stream rings hold whole frames for every channel count | S3 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels" |
| Z2-R-AUDIO-159 | Audio follows the presenting world through world transitions | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Following the presenting world"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners" |
| Z2-R-AUDIO-160 | Audio.Mix runs with no world attached | S3 | `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO.md` → "Frame-graph placement" |
| Z2-R-AUDIO-161 | Cooked enumeration fields hold every enumerator | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Event table section"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks" |
| Z2-R-AUDIO-162 | The device reads a valid snapshot before the first Audio.Mix | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Creation steps"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer snapshot record"; `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step" |
| Z2-R-AUDIO-163 | Audio.Mix and the decode thread exchange stream and cache work through two declared rings | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Decode thread loop"; `Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots" |
| Z2-R-AUDIO-164 | Stream reads go through the AUDIO read seam | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Test access"; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm"; `Zenith2/Docs/Design/AUDIO/Tests.md` → "Scripted read source" |
| Z2-R-AUDIO-165 | Bus meters never occupy the feedback ring | S3 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Meter frame record"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Feedback record"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Status and meters"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels" |
| Z2-R-AUDIO-166 | The effective prebuffer fits the guaranteed ring fill | S3 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSettings"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks" |
| Z2-R-AUDIO-167 | A full retire list defers and never frees early | S3 | `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement"; `Zenith2/Docs/Design/AUDIO.md` → "Counters" |
| Z2-R-AUDIO-168 | Preview banks stay out of the event index | S4 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"; `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank header section"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition panel" |
| Z2-R-AUDIO-169 | The editor preview cook equals the cook | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types"; `Zenith2/Docs/Design/AUDIO.md` → "ZenithAuthoring AUDIO files"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Mixer console" |
| Z2-R-AUDIO-170 | Stage 4 audio command kinds | S4 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command kinds"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command payloads"; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach" |
| Z2-R-AUDIO-171 | Zone portal and occluder components registered | S4 | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component conventions"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Component registration" |
| Z2-R-AUDIO-172 | Bank retention cache | S4 | `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank dependencies and residency" |
| Z2-R-AUDIO-173 | Quality-tier configuration fragments | S5 | `Zenith2/Docs/Design/AUDIO.md` → "Internal files"; `Zenith2/Docs/Design/AUDIO.md` → "The Stage 1 AUDIO budget ruling" |
| Z2-R-AUDIO-174 | Secondary worlds' music commands are ignored | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Secondary worlds and listeners"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Music model" |
| Z2-R-AUDIO-175 | Stage 4 Zenith.Audio bindings and ZS2208 | S4 | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes" |
| Z2-R-AUDIO-176 | Audition requests reach Audio.Mix through the main channels | S4 | `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels"; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API" |
| Z2-R-AUDIO-177 | Recorded effective dialogue locale | S4 | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Recorded dialogue locale"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection" |
