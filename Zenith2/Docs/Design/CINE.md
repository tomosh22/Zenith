# CINE — cinematics and the sequencer

## Purpose and scope

### What CINE owns

CINE owns cinematic sequences and their playback in Zenith2: the `ZenithCinematics` module (layer 9; `Zenith2/Docs/Architecture.md` → "Library dependency graph"), its cooked formats (`.zcine`, kind 14; `.zvid`, kind 21), the implementation of the cinematic lifecycle commands start, interrupt and skip and of the pre-cinematic snapshot and restore (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip"; Z2-D-080), the cinematic writer source's systems, video decoding and its GPU path (CINE-owned files in `ZenithFlux`), capture and export, CINE's part of the `ZenithAuthoring` tool library (`Zenith2/Tools/Authoring/Internal/Cinematics/` and the root header `Zenith_AuthoringCinematics.h`), its cook steps and importer in `Zenith2/Tools/ZenithCook/Internal/Cinematics/`, the sequencer panel under `Zenith2/Tools/ZenithEditor/Internal/Cinematics/`, the D-SEL-VIDEO harness `Zenith2/Benchmarks/VideoDecoderSelection/`, and the flagship game `Zenith2/Games/CinematicSlice` (Z2-D-021).

Concretely: timeline sequencing with twelve track kinds; camera tracks with lens, cuts and camera lighting; animation tracks driving ANIM's cinematic-tagged slots; audio and dialogue tracks driving AUDIO's command kinds with time-locked voices; event, marker and command tracks; subtitle and caption cues for UI; scrubbing and seeking; instance and section blending; interruption, skip and the restore of gameplay state; video playback; frame-exact capture with audio and subtitles; export; and the visual sequencer.

### Explicit exclusions

| Not CINE's | Owner | Basis |
|---|---|---|
| slot semantics, pose evaluation, inertial takes, facial rigs | ANIM | `Zenith2/Docs/Design/ANIM/Graph.md` → "Slots"; row AN9 |
| audio mixing, time-locked voice drift correction, dialogue tables, loudness | AUDIO | `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices" |
| caption presentation, localization, text layout | UI | `Zenith2/Docs/Design/UI/Accessibility.md` → "Caption cues" |
| the gameplay camera director and its suspension | GAME | `Zenith2/Docs/Design/GAME/Cameras.md` → "Cinematic suspension" |
| the lifecycle contract matrix and its per-command and scenario tests | GAME | `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests"; Z2-D-080 |
| the timeline and curve widgets, preview worlds, undo | EDIT | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Timeline" |
| particle effects played by command tracks | VFX | Stage 5 |
| spawnable bindings (entities a sequence spawns) | CINE, Stage 5 | "Stage plan"; the flag bit is reserved in Stage 4 |
| video encoding as a runtime feature, streaming video from the network, DRM | excluded | Z2-D-024 (static modules, no plugin decoders); single-player scope; no inventory item |
| dynamic plugin decoders | excluded | Z2-D-024 |

### Inventory items covered

| Item | Name | Where |
|---|---|---|
| Z2-I-CINE-01 | timeline sequencing | `Zenith2/Docs/Design/CINE/Playback.md` → "Playback model", "Systems" |
| Z2-I-CINE-02 | camera tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks" |
| Z2-I-CINE-03 | animation tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks" |
| Z2-I-CINE-04 | audio tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks" |
| Z2-I-CINE-05 | event tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Event tracks" |
| Z2-I-CINE-06 | timeline scrubbing | `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing" |
| Z2-I-CINE-07 | timeline blending | `Zenith2/Docs/Design/CINE/Playback.md` → "Blending", "Section blending" |
| Z2-I-CINE-08 | dialogue sync | `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks" |
| Z2-I-CINE-09 | subtitle sync | `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks" |
| Z2-I-CINE-10 | video playback | `Zenith2/Docs/Design/CINE/Video.md` (D-SEL-VIDEO (open, Stage 2)) |
| Z2-I-CINE-11 | capture | `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm" |
| Z2-I-CINE-12 | export | `Zenith2/Docs/Design/CINE/Video.md` → "Export algorithm" |
| Z2-I-CINE-13 | cinematics visual authoring | `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer panel" (Z2-D-079) |
| Z2-I-CINE-14 | cinematic interruption | `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback" (Z2-D-080) |
| Z2-I-CINE-15 | cinematic skip | `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback" |
| Z2-I-CINE-16 | restoring gameplay state after a sequence | `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |

### Items of other areas with a CINE half

| Item | Owner | CINE's half |
|---|---|---|
| Z2-I-GAME-28, Z2-I-GAME-29, Z2-I-GAME-30 (cinematic start, interrupt, skip lifecycle) | GAME (contract and tests) | the command kinds, playbacks, events and restore that GAME's tests exercise |
| Z2-I-ANIM cinematic skip and interrupt (Z2-R-ANIM-119) | ANIM | the lifecycle events ANIM's listener answers |
| Z2-I-AUDIO caption feed (Z2-R-AUDIO-087) | AUDIO | none beyond posting dialogue; CINE's captions come from its own tracks |

### Section files

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Design/CINE/PublicAPI.md` | Scope of this file · Zenith_Module_Cinematics header · Zenith_CinematicErrors header · Zenith_CinematicTypes header · Zenith_CinematicCommands header · Zenith_CinematicAsset header · Zenith_VideoAsset header · Zenith_CinematicComponents header · Zenith_CinematicEvents header · Zenith_CinematicsWorld header · Zenith_CinematicEvaluator header · Zenith_CinematicCapture header · Flux_Video header · Tool-side headers · Luau surface · Configuration keys |
| `Zenith2/Docs/Design/CINE/Playback.md` | Scope of this file · Playback model · World record layout · Systems · Command playback · Instance state machine · Asset binding · Preparation · Pre-cinematic snapshot · Track evaluation · Blending · Seeking and scrubbing · Determinism rules · Save and restore · Lifecycle listener · Tick-boundary handler · World transition and shutdown · Memory consumer · Field authority census rows |
| `Zenith2/Docs/Design/CINE/Formats.md` | Scope of this file · Format index · Cinematic document · Document migration · Cooked cinematic file · Video import settings · Cooked video file · Save section · Render-world records · Capture output · Export output |
| `Zenith2/Docs/Design/CINE/Video.md` | Scope of this file · Video pipeline overview · D-SEL-VIDEO candidates and criteria · Zenith_VideoDecoder interface · Platform decoder backend · FFmpeg decoder backend · Video thread · Video players · Frame selection · GPU side · VideoComposite pass · Texture surfaces · Audio and video sync · Video failure handling · Android lifecycle · Capture state machine · Capture algorithm · Export algorithm |
| `Zenith2/Docs/Design/CINE/Authoring.md` | Scope of this file · Document type registration · Video importer · Cook steps · Sequencer panel · Player component inspector · Capture and export in the editor · Localization extraction · Authoring walkthrough · CinematicSlice |
| `Zenith2/Docs/Design/CINE/Tests.md` | Scope of this file · Test targets · Module and registration tests · Unit tests · Format and cook tests · Playback tests · Track tests · Blending and seeking tests · Determinism and save tests · Script tests · Video tests · Capture tests · Export tests · Editor tests · Integration tests · Benchmarks · Fixtures · Gate steps |

### Conventions

Times are integer microseconds (`uint64` or `sint64`) in every format, payload and API except the convenience `float` seconds of `Zenith_CinematicStartDesc`, which the recorder converts once. A tick at rate H advances a scaled time by exactly 1,000,000 ("Time arithmetic" of `Zenith2/Docs/Design/CINE/Playback.md`). Hashes of names are `Zenith_Hash64` of the UTF-8 text. "Instance" is one playback of one sequence in one world; "slot" is a sequence's named binding; "section" a ranged item on a track; "key" a point item.

### Prior art

<!-- informative -->
The old engine has no cinematic system. Its only related code is the editor's animation timeline mapping (`Zenith/Editor/Zenith_AnimTimelineMath.h`), a pure seconds-to-pixels conversion; the lesson kept is that the timeline widget's mapping is a pure, separately tested function, which EDIT's `Zenith_EditorTimeline` already provides. The design otherwise follows the common shape of commercial sequencers (Unreal's Sequencer, Unity's Timeline): tracks bound to named slots, sections with eases, a pre-roll of preparation, and a restore of state on completion — adapted to Zenith2's transactional, deterministic frame (every effect a recorded command, an arbitrated write or an event).
<!-- /informative -->

## Dependencies

### Library dependencies

`ZenithCinematics` `DEPENDS` (Architecture's row plus the rows CINE requests, X289 as amended by X339 (c)): ZenithRuntime, ZenithAnimation, ZenithSave, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithCore (Architecture's current row); ZenithFlux (`Flux_VideoFrameSource`, `Flux_Renderer::EnableVideo`); ZenithJobs (`Zenith_SimulationClock`, the job context of its systems); ZenithProfiling (zones, counters, `Zenith_DevFileWriter`). It includes no ZenithPlatform header: the frame listener is ZenithRuntime's (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), `Zenith_Thread` and `Zenith_SleepUs` are ZenithCore's (`Zenith2/Docs/Design/CORE/Threading.md` → "Threads"), and the Android decoder uses the NDK directly under `Internal/Android/`; it includes no ZenithScript header either: its bindings are registered through ZenithReflect's `Zenith_ScriptBindingTable` and its publication handler implements the runtime's module-side handler interface. Layer: one above ZenithAnimation (9). Never: ZenithAudio, ZenithVFX, ZenithUI, ZenithGameplay, ZenithPhysics ([Z2-N-ARCH-074]).

CINE's part of `ZenithAuthoring` (`Zenith2/Tools/Authoring/Internal/Cinematics/` and the root header `Zenith_AuthoringCinematics.h`) adds the edges ZenithDocument, ZenithCinematics and ZenithCompression to that library in CINE's move ticket (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 4; row ED45); no edge is claimed as already declared. CINE's ZenithCook files (`Tools/ZenithCook/Internal/Cinematics/`) use ZenithCook's existing edges (every module library, ZenithAuthoring). The editor's `Internal/Cinematics/` uses ZenithEditorCore and ZenithAuthoring.

### Headings CINE relies on

| Need | Source |
|---|---|
| lifecycle contract, snapshot rules | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip", "Listener interface", "World transition" |
| phases, sync points, the tick boundary, arbitration | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases", "Playback at a sync point", "The tick boundary", "Writer sources and default precedence", "The arbitration merge" |
| module entry points and contexts | `Zenith2/Docs/Architecture.md` → "Module entry points"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" |
| systems, merge functions, command buffers and kinds, tick-boundary handlers | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header", "Zenith_SystemContext header", "Zenith_MergeContext header"; `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandBuffer header", "Zenith_CommandKinds header", "Zenith_WorldSimulation header"; `Zenith2/Docs/Design/ECS/Scheduling.md` → "Tick boundary and end-of-tick" |
| reflection layouts and property flags | `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeLayout header", "Zenith_TypeInfo header" |
| lifecycle listeners and dispatch | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleListener header" |
| asset kinds, loaders, readers, cooked headers, cook steps | `Zenith2/Docs/Design/ASSET/Formats.md` → "Kind table", "Header layout", "Checks"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader header", "Zenith_AssetReader header", "Zenith_CookedFile header"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions" |
| slots and cinematic slot commands | `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationCommands header", "Lifecycle listener"; `Zenith2/Docs/Design/ANIM/Graph.md` → "Slots" |
| audio command kinds and payloads, time-locked voices, offline rendering | `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Command kinds", "Zenith_AudioOfflineRenderer header"; `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices" |
| caption cues | `Zenith2/Docs/Design/UI/Accessibility.md` → "Caption cues" |
| camera components and suspension, lifecycle tests | `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera components", "Cinematic suspension"; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Per-command tests", "Scenario SkipCinematicMidBlend" |
| save participants | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header"; `Zenith2/Docs/Design/SAVE/Formats.md` → "Participant section framing" |
| extractors, camera cuts, readbacks, the upload ring, the epilogue | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extractor registration", "Camera cuts"; `Zenith2/Docs/Rendering/GpuResources.md` → "Readbacks", "Upload ring"; `Zenith2/Docs/Rendering/FrameAnatomy.md` → "Common epilogue" |
| simulation clock and external ticks | `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_SimulationClock header" |
| editor extensions, timeline, previews | `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface", "Extension table"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Timeline", "Curve editor"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Preview worlds" |
| console, panels, development files | `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry", "Zenith_DevFileWriter header"; `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers" |
| asynchronous sources and misuse blocks | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" |
| budgets | `Zenith2/Docs/Budgets.md` → "Per-subsystem memory rows", "Engine service CPU rows", "W-Cinematic" |

### Areas CINE depends on

ECS, SCENE, ASSET, ANIM, AUDIO (by command-kind name and reflection only), UI (by event type only), GAME (camera components by stable name; the lifecycle contract), LIGHT (camera lighting by stable name; output encoding), RPROD (video system, extractor, frame anatomy rows), SAVE, SCRIPT, MOD, JOBS, DEVT, EDIT, PLAT, COMPRESS (PNG export), PHYS (controller adoption, no API use), VFX (Stage 5, by command-kind name).

### Areas that consume CINE

GAME (lifecycle tests and its director's listener), ANIM (cinematic lifecycle events), EDIT (the sequencer in its extension table), SAVE (the participant), UI (caption cue events, video surfaces), scripts of every game (the `Cinematics` namespace), the CinematicSlice and OpenWorldSlice games.

### Interfaces CINE declares for lower layers

`Flux_VideoFrameSource` (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Flux_Video header") is declared in `ZenithFlux` and implemented in `ZenithCinematics`, so Flux calls upward through an interface it owns (`Zenith2/Docs/Contracts.md` → "Callback direction"). CINE declares no other upward interface; every other collaboration is CINE calling down or other areas observing CINE's events.

### Asynchronous source and misuse block

CINE needs one asynchronous source kind for sequence and video bindings (`Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding") and owns one misuse block for its bindings (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Misuse codes"). SCRIPT owns the source registry (ASYNC-SOURCE-REGISTRY) and has assigned CINE's kind: `ASYNC_SOURCE_CINEMATICS` (12) in `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" (row CN2 and X291 (a), both answered); CINE cites the value and defines none. The misuse block is assigned by the orchestrator's MISUSE-BLOCKS-2 ruling: **ZS2700–ZS2799** (taken around it: ZS2000 ANIM, ZS2100 UI, ZS2200 AUDIO, ZS2300 PHYS, ZS2400 TWOD, ZS2500 AI, ZS2600 PACK, ZS2800 CHAR, ZS2900 ONLINE, ZS3000 WORLD, ZS3100 CONTENT, ZS4101–4117 GAME, ZS51xx SAVE, ZS56xx VFX); CINE uses ZS2701–ZS2712, and X313 (b) (amending X291 (b)) reserves the row in `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges".

### Assumed interfaces pending other areas

<!-- assumption: pending MOD, ECS, ARCH, RPROD, AUDIO, UI, ANIM, GAME, DEVT, EDIT -->
| Interface | Owner | CINE's use | Row |
|---|---|---|---|
| a `Zenith_CommandBuffer` accessor for the tick it records in, read by the lifecycle recorders for a ticket's `m_uRecordTick` | ECS | tickets that resolve as pending or expired | CN28 |
| `Zenith_ModuleStartContext::GetAudioCapture()` (development) | MOD, AUDIO | capture audio | CN9 (a) |
| `Zenith_DevFileWriter::GetCompletionFailureCount()` | DEVT | capture write failures | CN21 |
| a follow-up `Zenith_CommandBuffer&` in `Zenith_PfnCommandPlayback` | ECS, ARCH | lifecycle dispatch from CINE's playbacks | CN6, X313 (c) second half |
| `EnableVideo`, `GetVideoSystem`, `m_pxVideo`, render-world video and overlay records, frame anatomy rows 2b and 6b and their order against TWOD's before-UI point, capture readback of the main view after a named pass, material binding of video surfaces | RPROD, REND | video and capture | CN8, X292, X313 (e) |
| ID rule confirmation, dialogue time-lock flags, the CINE instance-ID domain, the capture bridge over the offline backend, `Audio.SeekInstance` re-basing a time-locked voice's target | AUDIO | audio and dialogue tracks, capture | CN9 |
| `Zenith.Runtime.CaptionCueEvent` with a source ID, `Zenith.Runtime.CaptionCancelEvent`, the `video:` image source, the extraction source | UI, MOD | subtitles, video surfaces, localization | CN10, CN20 |
| per-bound-entity dispatch accepted, SKIP terminal, the census rows, the fixture document cooked by GAME's suite | GAME | the director, the tests | CN7 |
| slot names `cine.body` and `cine.face`; reading `Zenith.Animation.World` and `Zenith.Animation.Residency` in phase 1 | ANIM | animation tracks, preparation | CN14 |
| CINE's ZenithAuthoring layout and edges | EDIT | tool-side code | ED45 |
<!-- /assumption -->

## Library and file layout

### Target summary

| Target | Kind | `ZENITH_SUPPORTED` | Owner of the CINE files |
|---|---|---|---|
| `ZenithCinematics` | module library (`zenith_add_library`, MODULE_NAME `Cinematics`, requires `Animation`, first stage S4) | `ZENITH_GPU` (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping") | CINE |
| `ZenithCinematics_Tests` | test executable | `ZENITH_GPU` | CINE |
| `ZenithFlux` (CINE's files only) | engine library | as ZenithFlux | CINE (files listed below), RPROD (the library) |
| `ZenithAuthoring` (`Internal/Cinematics/`, `Zenith_AuthoringCinematics.h`) | tool library | `ZENITH_TOOLS_HOST` (Q84 (a), AUTHORING-CONTRACT) | CINE (EDIT owns the library) |
| `ZenithCook` (`Internal/Cinematics/`) | tool executable | as ZenithCook | CINE (ASSET owns the tool) |
| `ZenithEditor` (`Internal/Cinematics/`) | tool executable | as ZenithEditor | CINE (ED15) |
| `Zenith_Integration_Cinematics` | integration suite | `ZENITH_GPU` | CINE |
| `Zenith_Bench_VideoDecoderSelection` | benchmark (the D-SEL-VIDEO harness, Stage 2 until the Stage 4 move) | `ZENITH_WINDOWS`, `ZENITH_ANDROID` | CINE |
| `Zenith2/Games/CinematicSlice` | game | windows, android | CINE |

### ZenithCinematics public root headers

`Zenith2/Engine/ZenithCinematics/`: `Zenith_Module_Cinematics.h`, `Zenith_CinematicErrors.h`, `Zenith_CinematicTypes.h`, `Zenith_CinematicCommands.h`, `Zenith_CinematicAsset.h`, `Zenith_VideoAsset.h`, `Zenith_CinematicComponents.h`, `Zenith_CinematicEvents.h`, `Zenith_CinematicsWorld.h`, `Zenith_CinematicEvaluator.h`, `Zenith_CinematicCapture.h` (eleven; full declarations in `Zenith2/Docs/Design/CINE/PublicAPI.md`).

### ZenithCinematics internal files

`Zenith2/Engine/ZenithCinematics/Internal/`:

| Path | Content |
|---|---|
| `Module/Zenith_CinematicsModule.h/.cpp` | the module instance, `Zenith_CinematicsModuleDesc` (the test construction seam), Start and Attach steps, the world table |
| `Module/Zenith_CinematicsRegistration.cpp` | RegisterTypes, configuration keys, command kinds |
| `Module/Zenith_CinematicsFrameListener.cpp` | the frame listener: Android suspend and resume, the capture gate |
| `World/Zenith_CinematicsWorldRecord.h/.cpp` | the world record, instance pool, ticket and player-start tables, audio list |
| `World/Zenith_CinematicOutbox.h/.cpp` | the outbox (queue tick, queue point, emission, removal, admission) |
| `World/Zenith_CinematicSnapshotPool.h/.cpp` | the first-fit pool of instance blocks |
| `Systems/Zenith_CinematicEvaluateSystem.cpp` | `Cinematics.Evaluate`, its side work, fire items and merge function |
| `Systems/Zenith_CinematicApplyTransformsSystem.cpp` | `Cinematics.ApplyTransforms` |
| `Systems/Zenith_CinematicEmitEventsSystem.cpp` | `Cinematics.EmitEvents` |
| `Systems/Zenith_CinematicApplyLateSystem.cpp` | `Cinematics.ApplyLate` and its merge function |
| `Systems/Zenith_CinematicExtractor.cpp` | `Cinematics.ExtractRender` |
| `Playback/Zenith_CinematicCommandPlayback.cpp` | the eleven playback functions |
| `Playback/Zenith_CinematicSnapshot.cpp` | snapshot capture, the take-over merge and the restore |
| `Playback/Zenith_CinematicHandlers.cpp` | the lifecycle listener, the tick-boundary handler and the publication handler |
| `Tracks/Zenith_CinematicTrackEval.h/.cpp` | per-kind track evaluation |
| `Tracks/Zenith_CinematicFireItems.h/.cpp` | fire-item keys and the k-way merge |
| `Tracks/Zenith_CinematicTime.h` | time arithmetic (inline, constexpr) |
| `Tracks/Zenith_CinematicPayloadWriter.h/.cpp` | reflected payload templates |
| `Evaluator/Zenith_CinematicEvaluator.cpp` | the public evaluator |
| `Loading/Zenith_CinematicFileParse.h/.cpp`, `Loading/Zenith_VideoFileParse.h/.cpp` | the parsers behind the loaders and the public `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile` |
| `Loading/Zenith_CinematicKindLoaders.cpp` | kind 14 and 21 loaders, interning at load |
| `Save/Zenith_CinematicSaveParticipant.cpp` | the participant |
| `Script/Zenith_CinematicScriptBindings.cpp` | the `Cinematics` binding table |
| `Video/Zenith_VideoDecoder.h`, `Video/Zenith_VideoThread.h/.cpp`, `Video/Zenith_VideoPlayerTable.h/.cpp`, `Video/Zenith_VideoFrameSourceImpl.h/.cpp` | the decoder interface, the thread (`PumpOnce`) and its rings, the players, the `Flux_VideoFrameSource` implementation |
| `Capture/Zenith_CinematicCaptureImpl.cpp` | capture (created only outside Shipping) |
| `Diagnostics/Zenith_CinematicDiagnostics.cpp` | counters, zones, console commands, the panel provider |
| `Memory/Zenith_CinematicMemoryConsumer.cpp` | the memory consumer |

### ZenithCinematics platform files

| Path | Content |
|---|---|
| `Internal/Windows/Zenith_VideoDecoderMF.cpp` | the Media Foundation backend (moved from the D-SEL-VIDEO harness, `Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria") |
| `Internal/Android/Zenith_VideoDecoderMediaCodec.cpp` | the AMediaCodec backend (moved likewise) |
| `Internal/FFmpeg/Zenith_VideoDecoderFFmpeg.cpp` | compiled only after a D-SEL-VIDEO ruling admitting FFmpeg |

A Linux host build compiles no decoder backend; `Zenith_CreatePlatformVideoDecoder` returns `CINEMATIC_ERROR_VIDEO_UNSUPPORTED` there, and the module runs with video disabled (every test above the decoder uses the test backend through the seam).

### ZenithCinematics test files

`Zenith2/Engine/ZenithCinematics/Tests/` — every file of `Zenith2/Docs/Design/CINE/Tests.md` → "Test targets" under this directory, plus `Support/Zenith_CinematicFixtures.h`, `Support/Zenith_TestVideoDecoder.h`, `Support/Zenith_CinematicTestWorld.h`, `Support/Zenith_CinematicSeedFiles.h` (header-only, X184) and `Fuzz/CinematicLoader.cpp`, `Fuzz/VideoLoader.cpp`.

### ZenithFlux files owned by CINE

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithFlux/Flux_Video.h` | the public header |
| `Zenith2/Engine/ZenithFlux/Internal/Video/Flux_VideoSystem.cpp` | players' textures, upload, surfaces |
| `Zenith2/Engine/ZenithFlux/Internal/Video/Flux_VideoPasses.cpp` | `Flux_AddVideoConvert`, `Flux_AddVideoComposite` |
| `Zenith2/Engine/ZenithFlux/Tests/Video/*` | the Flux video tests |

### Shader files

`Zenith2/Shaders/Video/VideoConvert.slang` (compute, 8 × 8 groups), `Zenith2/Shaders/Video/VideoComposite.slang` (full-screen); parameter structs generated into `ZenithShadersGenerated` (Z2-D-015).

### ZenithAuthoring and ZenithCook files

CINE's tool-side code follows EDIT's ZenithAuthoring contract (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules", "ZenithAuthoring owners"; the AUTHORING-CONTRACT ruling; row ED45): only the code both ZenithCook and ZenithEditor call is in `ZenithAuthoring`; what only the cook runs stays in ZenithCook.

| Path | Library | Content |
|---|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringCinematics.h` | ZenithAuthoring (root header, `#include "Authoring/Zenith_AuthoringCinematics.h"`) | `Zenith_RegisterCinematicDocumentTypes`, `Zenith_CinematicExportDesc`, `Zenith_ExportCinematicCapture`, `Zenith_CinematicCaptionKey`, `Zenith_ListCinematicCaptionKeys` |
| `Zenith2/Tools/Authoring/Internal/Cinematics/Zenith_CinematicDocumentTypes.cpp`, `Zenith_CinematicExport.cpp`, `Zenith_CinematicPngWriter.cpp`, `Zenith_CinematicCaptionKeys.cpp` | ZenithAuthoring | registration, export (PNG, WAV, WebVTT), caption-key listing |
| `Zenith2/Tools/Authoring/Tests/Cinematics/` | `ZenithAuthoring_Tests` | document, export and extraction tests; migration fixtures, from version 2 on, in `Tests/Fixtures/Migration/Cinematics/SequenceDocument/` |
| `Zenith2/Tools/ZenithCook/Internal/Cinematics/Zenith_CinematicCookSteps.h/.cpp` | ZenithCook | `Zenith_RegisterCinematicCookSteps` (ASSET's signature), the validators over the public `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile` |
| `Zenith2/Tools/ZenithCook/Internal/Cinematics/Zenith_CinematicSequenceStep.cpp`, `Zenith_VideoCookStep.cpp`, `Zenith_VideoImporter.cpp`, `Mp4/Zenith_Mp4Reader.h/.cpp`, `Zenith_CinematicExtractionSource.cpp` | ZenithCook | the steps `cine.sequence` and `cine.video`, the importer `cine.video`, the MP4 reader, the localization extraction source |
| `Zenith2/Tools/ZenithCook/Tests/Cinematics/` | `ZenithCook_Tests` | cook, import and MP4 tests |

No tool compiles a ZenithCinematics `Internal/` source, and no CINE fuzz target lives under ZenithAuthoring or ZenithCook (fuzz targets exist in `win-asan` alone, where neither is built; the MP4 reader's robustness test is a seeded mutation test in `ZenithCook_Tests`).

### ZenithEditor files

`Zenith2/Tools/ZenithEditor/Internal/Cinematics/`: `Zenith_EditorCinematicsExtension.cpp`, `Zenith_EditorSequencerPanel.h/.cpp`, `Zenith_EditorSequencerModel.h/.cpp`, `Zenith_EditorSequencerCommands.cpp`, `Zenith_EditorCinematicPlayerInspector.cpp`, `Zenith_EditorCinematicCaptureDialog.cpp`; tests `Zenith2/Tools/ZenithEditor/Tests/Cinematics/`.

### Integration suites benchmarks and games

`Zenith2/Tests/Cinematics/` (`Zenith_Integration_Cinematics`), `Zenith2/Benchmarks/Budgets/CINE/`, `Zenith2/Games/CinematicSlice/` (`Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice").

### ZENITH_SUPPORTED matrices

`ZenithCinematics`, its tests and the integration suite declare `ZENITH_GPU` (they depend on ZenithFlux, so [Z2-N-ARCH-172] excludes them from `linux-tsan`); the video thread's hand-off uses CORE's `Zenith_SpscRing` (ruling Q80; contract `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas", row AU6), whose `linux-tsan` stress tests are CORE's, and is stressed by CINE's ring test under `win-asan`. `ZenithAuthoring` and ZenithCook are `ZENITH_TOOLS_HOST`; the D-SEL-VIDEO harness is `ZENITH_WINDOWS` and `ZENITH_ANDROID`.

## Public API

### API map

| Header | Declares | Section |
|---|---|---|
| `Zenith_Module_Cinematics.h` | entry points, fault points | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header" |
| `Zenith_CinematicErrors.h` | `Zenith_CinematicErrorCode`, `Zenith_CinematicError`, `Zenith_ToString` | "Zenith_CinematicErrors header" |
| `Zenith_CinematicTypes.h` | limits, `Zenith_CinematicInstanceID`, `Zenith_CinematicTicket`, `Zenith_CinematicBinding`, `Zenith_CinematicState`, `Zenith_CinematicEndReason`, start flags | "Zenith_CinematicTypes header" |
| `Zenith_CinematicCommands.h` | kinds, `Zenith_CinematicStartDesc`, ten recorders, six payloads | "Zenith_CinematicCommands header" |
| `Zenith_CinematicAsset.h` | `Zenith_Cinematic`, track kinds, spaces, interpolations, binding info, `Zenith_ValidateCinematicFile` | "Zenith_CinematicAsset header" |
| `Zenith_VideoAsset.h` | `Zenith_VideoAsset`, `Zenith_VideoInfo`, `Zenith_VideoCodec`, `Zenith_ValidateVideoFile` | "Zenith_VideoAsset header" |
| `Zenith_CinematicComponents.h` | `Zenith_CinematicPlayerComponent`, `Zenith_CinematicBindingEntry` | "Zenith_CinematicComponents header" |
| `Zenith_CinematicEvents.h` | four event payloads, the lifecycle payload | "Zenith_CinematicEvents header" |
| `Zenith_CinematicsWorld.h` | the world resource's read API | "Zenith_CinematicsWorld header" |
| `Zenith_CinematicEvaluator.h` | pure evaluation | "Zenith_CinematicEvaluator header" |
| `Zenith_CinematicCapture.h` | capture | "Zenith_CinematicCapture header" |
| `ZenithFlux/Flux_Video.h` | `Flux_VideoFrameSource`, `Flux_VideoSystem`, `Flux_VideoDrawDesc`, `Flux_AddVideoComposite` | "Flux_Video header" |
| `Tools/Authoring/Zenith_AuthoringCinematics.h`; ZenithCook's `Internal/Cinematics/Zenith_CinematicCookSteps.h` | registrations, export, caption keys; the cook-step registration | "Tool-side headers" |

### Rules across the CINE API

- Every fallible public function returns `Zenith_Result<T, Zenith_CinematicError>`; recorders cannot fail (playback reports).
- Untrusted input (cooked files, command payloads, script arguments, MP4 sources) never asserts; programmer errors (an index out of range on a const accessor, a missing own kind at attach) assert.
- No STL in a public header; spans are `Zenith_Span`; strings `Zenith_StringView`.
- Nothing public exposes an internal pointer that outlives an asset handle or a node run.
- Every simulation-visible value is integer or a float computed from integers in one fixed order.

### Contract summary per type

| Type | Kind | Thread affinity | Ownership |
|---|---|---|---|
| `Zenith_CinematicInstanceID`, `Zenith_CinematicTicket`, `Zenith_CinematicBinding`, payloads, event structs, `Zenith_CinematicInstanceInfo`, samples | value | `IMMUTABLE` | plain values |
| `Zenith_Cinematic`, `Zenith_VideoAsset` | type | `IMMUTABLE` | the asset slot |
| `Zenith_CinematicPlayerComponent` | type | `PHASE(PHASE_INPUT)` for 8–10 | its ECS column |
| `Zenith_CinematicsWorld` | type | `PHASE(PHASE_INPUT)` writes; const reads by declared readers | the module's world record |
| `Zenith_CinematicCapture` | type | `MAIN_THREAD` | the module instance |
| `Flux_VideoFrameSource` | interface | `RENDER_SUBMIT_THREAD` | implemented by the module, borrowed by Flux |
| `Flux_VideoSystem` | type | `RENDER_SUBMIT_THREAD` | `Flux_Renderer` |

### Handle and identifier kinds

`Zenith_AssetHandle<Zenith_Cinematic>` and `Zenith_AssetHandle<Zenith_VideoAsset>` (`Zenith2/Docs/Contracts.md` → "Zenith_AssetHandle"); `Zenith_EntityHandle` for bindings (`Zenith2/Docs/Contracts.md` → "Zenith_EntityHandle"); `Zenith_CinematicInstanceID` is a per-world serial (not a generational handle: an ID is never reused in a world, so staleness is "no such instance"); `Zenith_CinematicTicket` is a hash of the recording merge key; `Flux_ReadbackHandle` for capture readbacks (`Zenith2/Docs/Rendering/GpuResources.md` → "Readbacks").

### Command kinds

Eleven kinds, `Cinematics.Start` … `Cinematics.End`, recordable in phases 1–10 except the two internal kinds (phase 1 only); payload sizes and playbacks in `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Command payloads".

### Luau surface summary

Namespace `Cinematics`: `Start`, `Preload`, `Release`, `Interrupt`, `Skip`, `Seek`, `SetRate`, `Pause`, `Resume` (requests); `Resolve`, `GetStartError`, `GetState`, `GetTime`, `GetDuration`, `GetWeight`, `GetMarkerTime`, `IsPlaying`, `FindByTag`, `FindByEntity` (queries); `SameTicket` (pure). Events `Zenith.Cinematics.Started`, `Ended`, `Marker`, `StartFailed`. Every value a script reads is exact in a Luau number: names and tags are strings, tickets four 32-bit fields. Misuse codes ZS2701–ZS2712. `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface".

## Data structures and formats

### Format index

`Zenith2/Docs/Design/CINE/Formats.md` → "Format index": the cinematic document, the cooked cinematic file (12 section kinds), video import settings, the cooked video file (6 section kinds), the save section, render-world records, capture and export outputs.

### Capacities and constants

| Constant | Value | Configurable |
|---|---|---|
| instances per world | 16 | `cine.maxInstancesPerWorld` [1, 64] |
| bindings per sequence | 64 | no |
| tracks per sequence | 512 | no |
| channels per track | 64 | no |
| keys per channel | 65,536 | no |
| sections per track, per sequence | 4,096 (≤ 8 overlapping), 65,536 | no |
| event keys per sequence | 65,536 | no |
| fire-on-skip keys and sections per sequence | 8 | no |
| payload bytes per key | 256 | no |
| duration | 1 hour | no |
| instance block | 4,096 snapshot entries, 256 KiB | no |
| instance-block pool per world | 1 MiB | `cine.snapshotPoolBytes` |
| outbox per world | 256 + 10 × instances + 64 entries (480 at the default 16, 960 at 64); 256 entries `Cinematics.Evaluate` may hold | through `cine.maxInstancesPerWorld` |
| bound sequences per world | 96 | no |
| caption list per instance | 16 | no |
| records per instance buffer per tick | 512 | no |
| ticket entries per world, window | 64, 2 ticks | no |
| player autostarts awaiting their outcome | 64 | no |
| asset loads in flight per world | 64 | no |
| preloaded sequences per world | 32 | no |
| audio instances per world, per instance | 1,024, 256 | no |
| dialogue lines in a preparation window | 32 | no |
| subsequence depth | 4 | no |
| video players | 4 | no |
| video size, duration | 3840 × 2160, 2 hours | no |
| decode-ahead | 6 frames (frame ring of 8) | `cine.video.decodeAheadFrames` [2, 32] |
| capture size | ≤ 3840 × 2160 | per capture |

### Sequence time representation

Microseconds as `uint64` for authored and sequence times; scaled time `sint64` µs·Hz inside an instance; rate Q16.16 in [0, 8]; blend durations µs; weights `float` in [0, 1]. `Zenith2/Docs/Design/CINE/Playback.md` → "Time arithmetic".

### World resource layout

`Zenith2/Docs/Design/CINE/Playback.md` → "World record layout": about 253 KiB of fixed record (72 KiB of instances, 135 KiB of outbox, 32 KiB of audio list, 14 KiB of tables and the bound set) plus the 1 MiB instance-block pool and the templates per world with the defaults (about 632 KiB of fixed record at 64 instances).

### Instance record layout

4,608 bytes per instance, field by field in `Zenith2/Docs/Design/CINE/Playback.md` → "World record layout", with its instance block ("Instance block" there); serialized per `Zenith2/Docs/Design/CINE/Formats.md` → "Save section".

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Section |
|---|---|---|
| Evaluate (side work, advance, fire items, track records) | O(I · (N log T + R)) per tick | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| outbox emission and removal | O(entries) | "Outbox" |
| ApplyTransforms | O(bound entities × tracks per entity) | "Cinematics.ApplyTransforms" |
| binding resolution | O(B log S) | "Binding resolution" |
| snapshot, take-over merge and restore | O(P log P) | "Pre-cinematic snapshot", "Take-over of a bound entity" |
| curve sampling | O(log K) | "Curve channels and interpolation" |
| skip | O(remaining keys + bound tracks) | "Skip playback" |
| seek | O(active sections + jumped keys) | "Seeking and scrubbing" |
| cook | O(K log K) | `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step" |
| frame selection | O(frames popped) | `Zenith2/Docs/Design/CINE/Video.md` → "Frame selection" |

### Instance state machine

Nine states (`NONE`, `BINDING`, `PREPARING`, `BLENDING_IN`, `PLAYING`, `PAUSED`, `BLENDING_OUT`, `ENDED`, `RELEASED`) with the full event table in `Zenith2/Docs/Design/CINE/Playback.md` → "Instance state machine".

### Frame-graph tick walk

| Point | CINE work |
|---|---|
| prologue step 1 (main thread) | the frame listener: Android suspend and resume posts; the capture gate (`HoldNextTick`) |
| phase 1 | `Cinematics.Evaluate`: removal of the accepted outbox entries, autostarts, player outcomes, completions, side work, paused seeks, advance and fire items; its merge accepts buffers and issues loads |
| sync after phase 1 | playbacks of `Begin`, `End`, ANIM's and AUDIO's records CINE made, and any CINE command recorded in phase 1 |
| sync after phase 2 | playbacks of script-recorded starts, skips, interrupts (begin inline) |
| phase 6 | `Cinematics.ApplyTransforms` (not in the Begin tick; the end tick at weight 0) |
| phase 8 | `Cinematics.EmitEvents` (entries due this tick, in emission groups); its merge accepts groups and writes `Zenith.Cinematics.Emission` |
| phase 9 | delivery of CINE's events |
| phase 10 | `Cinematics.ApplyLate`; its `SetProperty` records apply at the sync point after phase 10 |
| phase 11 | the participant's capture (when a save runs) |
| phase 12 | `Cinematics.ExtractRender` |
| tick boundary step 1, step 2 | publications through the sink's forwarder, with binding resolution in CINE's publication handler; end-state writes, take-over base capture (active and ended holders), restores |
| render-submit | player control (the video thread's event ring), frame selection, upload, capture video readiness and readback request |
| GPU prologue 2b, epilogue 6b | `VideoConvert`, `VideoComposite` |
| `Zenith.VideoDec` | decode |

### Video player state machine summary

`FREE`, `OPENING`, `RUNNING`, `SEEKING`, `SUSPENDED`, `CLOSING`, `FAILED`; `Zenith2/Docs/Design/CINE/Video.md` → "Video players". Capture: `IDLE`, `RECORDING`, `DRAINING`, `FINISHED`, `FAILED`; "Capture state machine".

## Threading lifetime and error contract

### Thread affinity per type

As "Contract summary per type". CINE code runs on: job workers (the five systems), the world's sync-point node (playbacks, listener, tick-boundary handler), the main thread (Start, Attach, capture control, console commands), the render-submit thread (player control, frame source, uploads), and `Zenith.VideoDec`.

### Frame-graph placement

The five nodes of `Zenith2/Docs/Design/CINE/Playback.md` → "Systems", their declared access listed there; no CINE node runs Luau or blocks; none has main-thread affinity.

### Field authority census rows

`Zenith2/Docs/Design/CINE/Playback.md` → "Field authority census rows" (confirmed by the D-PHASES refinement (open, Stage 1)).

### Lock-order entries

None. CINE takes no `Zenith_Mutex`: the world record is owned by phase 1, its merge node and the sync-point and tick-boundary nodes, and `Zenith.Cinematics.Emission` by phase 8's merge; the video hand-off uses five kinds of `Zenith_SpscRing` channel (Q80; "Video thread" of `Zenith2/Docs/Design/CINE/Video.md`: the command, lifecycle and event rings per module, the frame and return rings per player); the player table is owned by the render-submit thread alone; the capture by the main thread, which reaches the render-submit thread only through the capture's report ring and its atomic tick ("Capture algorithm"). The only lock CINE code reaches is inside ZenithCore's `Zenith_StringTable::Intern` (its own short-hold level), called by the kind loaders at load, never while CINE holds anything.

### Threads CINE owns

`Zenith.VideoDec` (one per process, video-enabled configurations only), created at module Start step 6, joined at module shutdown after every player closed and before the decoder is destroyed. X293 asks Contracts to add it to "Shutdown ownership order": after the renderer has stopped calling the frame source (`DisableVideo` first), before the module instance is freed.

### Startup attach and shutdown

Start steps 1–7 and Attach steps 1–4 (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"), each unwound on failure and each with a fault point. Detach: `Zenith2/Docs/Design/CINE/Playback.md` → "World transition and shutdown". Shutdown order: `DisableVideo`, close players, stop and join the thread, destroy the decoder, unregister the frame listener, bindings, consumer and kinds in reverse.

### Cancellation

A start cancels when interrupted or skipped before Begin (`CINEMATIC_ERROR_CANCELLED`, no lifecycle event), when its world detaches (no event at all, "World transition and shutdown"), when its sequence fails to bind, or when a save it was in is restored over re-cooked content. Every completion ID CINE allocated is completed exactly once ([Z2-N-SCRIPT-931]): an abandoned need with the abandoned payload at the next Evaluate, every outstanding one at `DetachWorld` ("Asset binding" step 4). A capture ends by `End` (drains) or a write failure (fails, cancelling its readback). Video seeks cancel queued decode work by `Flush`.

### Error enumerations

`Zenith_CinematicError` (the struct form of [Z2-N-CONTRACT-480]: `Zenith_CinematicErrorCode m_eCode` with 25 values `CINEMATIC_ERROR_*`, and a 16-bit detail) in `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicErrors header". Cook findings are rule IDs (`cine.cook.*`, `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook validation rules").

### Luau errors and asserts

Script misuse raises the catalogued errors of `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Misuse codes"; world facts (a stale instance, an unknown binding at playback) are counted and reported through events, never raised and never asserted; asserts are reserved for CINE's own invariants (its own kinds at attach, internal indices).

## Diagnostics and profiling

### Counters

Per world, reset each tick unless named `_total`, read by the profiler and the panel:

| Counter | Meaning |
|---|---|
| `cine.instances.live` | live instances |
| `cine.instances.started_total`, `cine.instances.ended_total` | starts begun, ends by reason (`…ended_total.completed`, `.interrupted`, `.skipped`, `.cancelled`, `.failed`) |
| `cine.commands.recorded` | records CINE's systems made this tick |
| `cine.commands.rejected_delta`, `cine.commands.ignored_delta`, `cine.commands.no_world_delta` | dropped playbacks |
| `cine.commands.instance_cap_delta` | instances that hit the per-tick record cap |
| `cine.commands.capacity_deferred_delta` | instance buffers the merge capacity rejected (re-run whole next tick) |
| `cine.commands.ticket_overflow_delta`, `cine.commands.outbox_full_delta` | starts dropped because the ticket table was full; because the outbox could not admit them ("Outbox" rule 2, reachable only after rejected emission groups) |
| `cine.commands.no_row_delta` | own-row writes skipped because `GetChunkOfEntity` failed for the entity |
| `cine.events.queued`, `cine.events.deferred_delta`, `cine.events.emit_rejected_delta` | outbox; emission groups the phase-8 merge rejected (re-emitted next tick) |
| `cine.captions.evicted_delta` | caption list entries cancelled early because the list was full |
| `cine.snapshot.bytes`, `cine.snapshot.limit_delta`, `cine.snapshot.restore_skipped_delta` | instance-block pool use; refused blocks; restore entries skipped for a removed component |
| `cine.binding.in_flight`, `cine.binding.deferred_delta`, `cine.binding.preload_refused_delta` | loads; needs beyond the bound; preloads beyond 32 sequences |
| `cine.prepare.waiting`, `cine.prepare.timeout_delta` | preparation |
| `cine.track.missing_type`, `cine.player.clamped_delta` | degraded tracks, clamped player values |
| `cine.video.players`, `cine.video.late_delta`, `cine.video.player_limit_delta`, `cine.video.command_full_delta`, `cine.video.timestamp_mismatch_delta`, `cine.video.shed_trimmed_delta`, `cine.video.shed_stopped_delta`, `flux.video.upload_skipped_delta`, `flux.video.alloc_failed_delta` | video; the two shed counters prove the memory shedding ran ([Z2-N-ARCH-814]) |
| `cine.capture.frames_written`, `cine.capture.frames_pending`, `cine.capture.video_wait_delta` | capture; frames held because a video player was not ready |

### Profiler zones

`Cinematics.Evaluate`, `Cinematics.Evaluate.Merge`, `Cinematics.ApplyTransforms`, `Cinematics.EmitEvents`, `Cinematics.ApplyLate`, `Cinematics.ExtractRender`, `Cinematics.Playback.<Kind>`, `Cinematics.Snapshot`, `Cinematics.Restore`, `Cinematics.VideoDecode` (on `Zenith.VideoDec`), `Flux.VideoUpload`, GPU zones `VideoConvert` and `VideoComposite`. `Z2-B-CINE-002` is the sum of the five system zones (MP-CPUZONE).

### On-screen panel

A panel provider `Cinematics` (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers"): per instance a row with sequence name, state, time / duration, weight, rate, bound count and snapshot bytes; a timeline strip with the playhead and the next five keys; video players with media time, decoded-ahead frames and late count. `cine.debug.overlay` draws it without opening the profiler.

### Log categories

`cine` (instances, starts, failures), `cine.cook` (cook findings), `cine.video` (players, decoder), `cine.capture`, `cine.save`. Each once-per-cause message is rate-limited to once per world per cause.

### Console commands

Development builds (`Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"):

| Command | Effect |
|---|---|
| `cine.list` | prints the instance rows |
| `cine.play <asset> [binding=entity …]` | records a start in the focused world |
| `cine.skip <id>`, `cine.interrupt <id>`, `cine.seek <id> <seconds>`, `cine.rate <id> <rate>`, `cine.pause <id>`, `cine.resume <id>` | as the requests |
| `cine.capture.begin <name> [<w> <h>] [ui]`, `cine.capture.end` | capture (refused unless the process runs the capture configuration, `Zenith2/Docs/Design/CINE/Video.md` → "Capture state machine") |
| `cine.dump <id>` | writes the instance record and block as JSON through the development file writer (`cine/<id>.json`) |

The simulation-affecting commands (`cine.play` to `cine.resume`) follow DEVT's determinism obligation for command handlers (`Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevConsole header", `Zenith_DevCommandDesc`): each submits its CINE command through the focused world's recorded input-command stream, so it is applied at the next tick and a replay reproduces it from the log, and refuses itself with `Zenith_DevCommandOutput::Error` when the world cannot take a recorded input (`Test_CinematicDiagnostics_ConsoleCommandsRecordRequests`).

### Debug draw

With `cine.debug.overlay`: camera tracks' paths for the next 5 s as polylines, cut points as markers, anchor axes, bound entities' names.

### Frame-dump entries

DEVT's frame dump gains a `cinematics` section: per instance the info record, the outbox count and the video players (`Zenith2/Docs/Design/DEVT/Console.md` → "DEVT frame dump").

## Authoring and cooking

CINE's authoring: the document type `Zenith.Cinematics.SequenceDocument`, the importer `cine.video`, the cook steps `cine.sequence` and `cine.video` with the validation rules of "Cook validation rules", the sequencer panel with its model, commands, preview session, camera pilot and binding picker, the player inspector, capture and export in the editor, and localization extraction of caption keys: `Zenith2/Docs/Design/CINE/Authoring.md`. Migration: `Zenith2/Docs/Design/CINE/Formats.md` → "Document migration".

## Failure handling

| Failure | Behaviour |
|---|---|
| missing or failed sequence asset | the start fails with `CINEMATIC_ERROR_INVALID_ASSET` (StartFailed, ticket), no lifecycle event |
| missing or failed video asset | never fails a start: the section draws nothing, logged once |
| corrupt cooked file | the loader fails the asset (`CINEMATIC_ERROR_CORRUPT` with the check number); starts fail as above |
| a bound entity missing, stale, or lacking a component | `CINEMATIC_ERROR_BINDING_*`; a required slot fails the start, an optional one is skipped |
| a bound entity destroyed while playing | the slot is dropped ([Z2-N-ARCH-556]); the instance ends `FAILED` with a normal restore when a slot flagged `uCINE_BINDING_FLAG_REQUIRED` is dropped and no other slot with that flag is bound |
| a snapshotted component removed before the restore | that entry is skipped and counted (`cine.snapshot.restore_skipped_delta`) |
| a command kind or event type absent from the build | the track is disabled, counted and logged once; the sequence plays on |
| snapshot, instance or ticket limits | the start fails with its code, or (ticket table full) is dropped and its ticket expires |
| allocation failure (world record, pool, templates) | attach fails with `MODULE_ERROR_OUT_OF_MEMORY`, or the start fails with `CINEMATIC_ERROR_OUT_OF_MEMORY` |
| preparation timeout | begins anyway or fails, per `cine.prepareTimeoutFails` |
| the outbox admission, the record cap, or the merge capacity | deferred to the next tick whole, never dropped and never partly applied; a rejected emission group is re-emitted next tick in order |
| device loss | the GPU side recreates its textures; simulation unaffected (`Zenith2/Docs/Design/CINE/Video.md` → "Video failure handling") |
| decoder failure or reclaim | the stream is recreated once, then the player fails; simulation unaffected |
| world detach mid-cinematic | no lifecycle event (the listeners are destroyed with the world), no restore, every allocated completion ID completed |
| save content changed at load | the instance ends `FAILED` through END (dispatched once); the snapshot is restored |
| budget breach (`Z2-B-CINE-001`) | memory pressure sheds presentation only, through the Cinematics row of `Zenith2/Docs/Architecture.md` → "Shedding per subsystem" (WARNING: decode-ahead trimmed to 2; CRITICAL: invisible players stopped; X313 (a)); never a sequence, a preload pin or simulation state (`Zenith2/Docs/Design/CINE/Playback.md` → "Memory consumer") |
| capture readback refused, deferred or failed | the same tick is captured again with the tick gate held |
| a video player not ready during a capture | the gate holds until it is ready (`cine.capture.video_wait_delta`); after 300 frames the capture fails with `CINEMATIC_ERROR_CAPTURE_STATE` |
| capture write refused or failed on completion | capture fails |

## Test strategy

Every test, its file, target and gate step: `Zenith2/Docs/Design/CINE/Tests.md`. Layers: unit tests of time, fire items, evaluation, payloads, IDs and commands; format, cook and fuzz tests of both cooked formats and a seeded mutation test of the MP4 reader; playback, track, blending, seeking and snapshot tests in a headless test world with recording ANIM and AUDIO kinds; determinism, replay and per-field save tests across worker counts; script tests including every misuse row and Luau-exact identifiers; video tests with the test decoder through the module's seam and the pumped video thread, plus hardware-labelled conformance; Flux conversion and composite tests against CPU references on WARP and lavapipe (Zenith2 has no Null RHI backend; X170: not `needs-gpu`); capture tests in the integration suite on a WARP or lavapipe device with the offline audio backend; export tests; editor model tests and the automated authoring walkthrough; integration tests with the real modules. GAME's lifecycle tests are the acceptance of the lifecycle implementation.

## Budget rows

### Rows CINE owns

| Row | Name | Status | Unit | Procedure | Platforms | Workload | Breach | Producer |
|---|---|---|---|---|---|---|---|---|
| `Z2-B-CINE-001` | Cinematic timelines and video decode buffers | pending | MB | MP-SUBMEM | Win, S24 | W-Cinematic | BR-GOVERNOR | Stage 1 per-system budget ticket for CINE |
| `Z2-B-CINE-002` | Timeline evaluation CPU | pending | ms | MP-CPUZONE | Win, S24 | W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for CINE |

`Z2-B-CINE-001` is dual-resident (`MEM-WIN-RAM` and `MEM-WIN-VRAM`): CPU side = resident sequences and video sections, world records, snapshot pools and decode-ahead frames; VRAM side = video plane and surface textures. Proposed values for the producer ticket to measure against (proposals, not rows): Windows RAM 32 MB, VRAM 48 MB (two 1080p players), S24 48 MB unified; `Z2-B-CINE-002` 0.10 ms desktop, 0.25 ms S24, for the CinematicSlice's heaviest sequence. The producer is blocked by the CORE, JOBS and RPROD Stage 1 budget tickets (`Zenith2/Docs/Budgets.md` producer table).

### Rows CINE is measured against

| Row | Why |
|---|---|
| `Z2-B-ECS-004` | command buffer capacity CINE's records use |
| `Z2-B-GAME-003` | the event queue CINE's outbox feeds; progress condition: its value must admit `uCINE_EMISSION_GROUP_ENTRIES` (64) events beyond the peak of the other phase-8 emitters, so that every emission group is eventually accepted (`Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"); GAME's producer measures the row against it |
| `Z2-B-RHI-006` | the upload ring video frames use |
| `Z2-B-RHI-011` | the readback pool capture uses |
| `Z2-B-SCRIPT-023`, `Z2-B-SCRIPT-024`, `Z2-B-SCRIPT-026` | script request side effects and asynchronous capacity CINE's bindings consume |
| `Z2-B-ANIM-002` | ANIM's evaluation of cinematic slots |
| `Z2-B-LIGHT-012` | depth of field driven by camera tracks |
| `Z2-B-PACK-003` | Windows install size of CinematicSlice |
| `Z2-B-CONTENT-003` | CinematicSlice source-asset quota |
| `Z2-B-SHIP-001`, `Z2-B-SHIP-019`, `Z2-B-SHIP-021`, `Z2-B-SHIP-023` | frame rows and the flagship acceptance |
| `Z2-B-EDIT-001` | editor responsiveness with the sequencer open |

### Pending row needs

Row CN12 (queued as X294) asks the Budgets owner for three pending rows, each produced by "Stage 1 per-system budget ticket for CINE": "Video decode CPU" (ms, MP-CPUZONE, Win and S24, W-Cinematic, BR-ACCEPT; `Zenith.VideoDec`'s zone per decoded 1080p30 second), "Video conversion and composite GPU" (ms, MP-GPUPASS, Win and S24, W-Cinematic, BR-ACCEPT; `VideoConvert` + `VideoComposite`), and "Cinematic instances per world" (count, MP-COUNTER, Win and S24, W-Cinematic, BR-REFUSE; the value of `cine.maxInstancesPerWorld`).

## Stage plan

### Stage 1

The budget producer ticket (rows `Z2-B-CINE-001`, `-002` and the three pending rows) and CinematicSlice's flagship acceptance definition (content scale); D-PHASES refinement confirms CINE's census rows.

### Stage 2

D-SEL-VIDEO: the worker ticket "D-SEL-VIDEO decoder harness" (the benchmark `Zenith2/Benchmarks/VideoDecoderSelection/`, its backends and its smoke run) and the `needs-human` ticket "D-SEL-VIDEO ruling" (the measurement on the reference desktop and the S24, `Zenith2/Docs/Research/CINE/VideoDecoderSelection.md`, the DecisionLog entry); `Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria". No ZenithCinematics runtime code: the harness is a benchmark target, and its backends move into ZenithCinematics in Stage 4.

### Stage 4

Everything else of this design (the module's first stage is S4): API-only slice first, then formats and cook, playback core, tracks, snapshot and restore, save, scripts, video (candidate A until the ruling; the platform backends moved from the harness), capture and export, the sequencer, CinematicSlice's first playable milestone; `Assets/Video/Flashback.mp4` comes from a `needs-human` content ticket (Q55). CINE's A5 batch also files the `needs-human` content-production ticket "CinematicSlice character walkthrough starting content" that CHAR's row CH19 (b) asks for (`Ranger.glb`, `Humanoid.glb` with `.meta` and rig document, six dialogue WAVs, the bank, `CinematicSlice.charproject.json`, `Scenes/CharacterWalkthrough.scene.json` with the `WalkthroughDialogue` schema and behaviour, to the specification of `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"); CHAR's timed walkthrough ticket is blocked by it (binary content, Q55).

### Stage 5

Command tracks naming `VFX.Spawn` and other Stage 5 kinds; CinematicSlice set-pieces; spawnable bindings: a binding with the `prefab` member (`Zenith2/Docs/Design/CINE/Formats.md` → "Bindings") carries `uCINE_BINDING_FLAG_SPAWNABLE` and the index of its prefab dependency record in the binding record's spawn-prefab field; Begin records `Zenith_RecordInstantiatePrefab` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header") for it at the anchor's or the world origin's transform and binds the spawned entity from its spawn result at the next tick boundary; the release records `Destroy` of that entity; the slot is never snapshotted or restored; the cook rule `cine.cook.spawnable_prefab` refuses a `prefab` that is not a prefab asset or a spawnable binding a player also binds.

### Stage 6

CinematicSlice's acceptance package and sign-off ([Z2-N-BUDGET-434], [Z2-N-BUDGET-435]), device A/V sync measurement, budget sign-off of the CINE rows.

### API-only slices

| Slice | Content | Unblocks |
|---|---|---|
| 1 | `Zenith_CinematicTypes.h`, `Zenith_CinematicErrors.h`, `Zenith_CinematicCommands.h`, `Zenith_CinematicEvents.h`, `Zenith_CinematicsWorld.h`, `Zenith_Module_Cinematics.h` with contract blocks and stub definitions | GAME's lifecycle tests, ANIM's listener, scripts' generated types |
| 2 | `Zenith_CinematicAsset.h`, `Zenith_VideoAsset.h`, `Flux_Video.h` | ASSET's kind table, RPROD's video hooks |

### Ticket slices and order

| # | Slice | Depends on |
|---|---|---|
| 0 | D-SEL-VIDEO decoder harness (Stage 2, worker), then the `needs-human` D-SEL-VIDEO ruling | — |
| 1 | API-only slice 1 | ECS, SCENE, ANIM API slices |
| 2 | API-only slice 2 | 1, ASSET kinds |
| 3 | time arithmetic and evaluator | 2 |
| 4 | document type, cook step `cine.sequence`, loader, validation, fuzz | 2, ASSET cook API |
| 5 | module registration, world record, Start and Attach steps | 1, MOD assumptions |
| 6 | start, begin, interrupt, skip, end playbacks and the state machine | 5 |
| 7 | snapshot and restore, tick-boundary handler | 6 |
| 8 | asset binding, preparation, asynchronous source | 6, SCRIPT registry row |
| 9 | transform and camera tracks, ApplyTransforms, ApplyLate | 3, 6 |
| 10 | animation tracks | 9, ANIM commands |
| 11 | audio and dialogue tracks | 9, AUDIO kinds |
| 12 | event, marker, subtitle, property, command, overlay tracks, EmitEvents | 9 |
| 13 | seeking, rate, pause | 10, 11, 12 |
| 14 | save participant | 7, 8 |
| 15 | Luau bindings and misuse codes | 6, SCRIPT misuse block |
| 16 | player component | 6 |
| 17 | video asset, importer, `cine.video` step, ZenithCook's MP4 reader (written from the harness's reader) | 2, the Stage 2 harness |
| 18 | decoder interface, test decoder and the module seam, thread and rings (`PumpOnce`), players | 17 |
| 19 | Flux video system, convert and composite passes | 18, RPROD hooks |
| 20 | platform decoders (Windows, Android), moved from the harness, which this ticket deletes unless candidate B was admitted | 18, the D-SEL-VIDEO ruling |
| 21 | capture (lockstep, tick gate, file writer) | 19, AUDIO capture bridge, X313 (d), CN21 |
| 22 | export, ZenithAuthoring registration and caption keys (CINE's ZenithAuthoring move, ED45) | 21, EDIT's library-creation ticket |
| 23 | sequencer model, panel, preview | 13, EDIT framework |
| 24 | diagnostics, console, panel | 6 |
| 25 | CinematicSlice first playable | 1–24 |

Every slice lands its tests in the same ticket ([Z2-N-CONTRACT-410]).

## Decisions

### Settled decisions governing CINE

| Decision | Effect on CINE |
|---|---|
| Z2-D-002 | CinematicSlice contains no C++ |
| Z2-D-004 | cooked sections carry their codec in the descriptor |
| Z2-D-012 | budget values change only through a decision |
| Z2-D-015 | video shaders in Slang with generated structs |
| Z2-D-016 | script requests are transactional |
| Z2-D-019 | every cinematic effect is deterministic; that video, memory shedding and capture are isolated from simulation is CINE's elaboration of it |
| Z2-D-021 | CinematicSlice is a flagship |
| Z2-D-024 | static module; no plugin decoders |
| Z2-D-038 | JSON authoring, versioned binary cooked data, reflection-serialised payloads |
| Z2-D-045 | no exceptions; results and asserts |
| Z2-D-046 | no mutable global state |
| Z2-D-048 | document migration, save migration, re-cook |
| Z2-D-050 | presentation (video, overlay) through extraction |
| Z2-D-053 | publication in completion-ID order |
| Z2-D-079 | the sequencer is an editor tool |
| Z2-D-080 | the lifecycle phases and commands |
| Z2-D-084 | D-SEL-VIDEO is a harness ticket plus a `needs-human` ruling ticket |
| Z2-D-085 | design edits land in the diff of the contract or API change they describe |
| Z2-D-097 | rulings appended to the DecisionLog in the ruling ticket's diff |

### Open decisions CINE depends on

| Decision | Stage | Effect |
|---|---|---|
| D-SEL-VIDEO (open, Stage 2) | 2 | the decoder backend; CINE owns it |
| D-PHASES (open, Stage 1) | 1 | confirms the census rows |
| D-SEL-CODEC (open, Stage 2) | 2 | AUDIO's codec for dialogue and video audio |
| D-CLASS-SIM (open, Stage 2) | 2 | which simulations CinematicSlice's set-pieces may show |
| D-SEL-HASH (open, Stage 1) | 1 | the hash behind name hashes and cooked checks |

### Selection decisions CINE owns

D-SEL-VIDEO: candidates, nine criteria and the per-platform acceptance rule in `Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria"; the worker harness ticket "D-SEL-VIDEO decoder harness" and the `needs-human` ruling ticket "D-SEL-VIDEO ruling", which measures, writes `Zenith2/Docs/Research/CINE/VideoDecoderSelection.md` and appends the next free `Z2-D` number read from the log at ruling time.

### Rulings this design records

<!-- elaboration -->
| Ruling | Rationale |
|---|---|
| a natural end holds the last frame during the blend-out | the end state is what the restore leaves for unflagged properties, so holding it removes a second pop |
| SKIP is terminal: no END follows | GAME and ANIM treat SKIP as the end; a second event would double-release |
| blend-out of an unflagged property holds `lerp(B_in, C, w₀)`, the value at the weight the blend-out started from (C for a natural end) | the gameplay value is unknown to the cinematic's writer in a precedence merge, and holding the start weight keeps an interrupt during the blend-in from jumping to C |
| the end tick is written at weight 0 | the tick the weight reaches 0 is otherwise left to gameplay writers for one tick, then jumps back at the restore |
| tracks write from the tick after Begin | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip"; a Begin-tick write would pull a moving body back to its snapshot |
| take-over moves the snapshot and hands the row over at a tick boundary, committed only after the taking instance's snapshot succeeded | the final restore must return to the pre-first-cinematic state, a failed start must leave the holder intact, and two instances never write one row in one tick |
| events are emitted in phase 8, an entry queued after phase 7 in the next tick's phase 8, and an entry is removed only after the phase-8 merge accepted its group | one emitter for timeline events, skip events and captions, each delivered exactly once even when the merge rejects a group |
| a paused instance is written with the rule of the state it left, and a flagged value blends out as `lerp(B, lerp(B_in, C, w₀), w / w₀)` | neither a pause, a resume nor an interrupt during the blend-in of a taken-over entity changes a written value in one tick |
| a start in an ended holder's end tick blends in from the holder's end-tick output | a chained cutscene must not pull its characters back to where they stood before the first one |
| presentation resume re-posts from the audio list with fresh IDs | AUDIO refuses a post of a live ID and saves no voice |
| records a playback or a restore needs are side work of the next Evaluate | a playback and a tick-boundary handler cannot record commands |
| video, memory shedding and capture are presentation only | decoder timing and memory pressure differ per device and must not reach simulation |

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/CINE.md` and the design headings that specify it.

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-CINE-001 | Module entry points and registration | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header" |
| Z2-R-CINE-002 | World attach and detach | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"; `Zenith2/Docs/Design/CINE/Playback.md` → "World transition and shutdown" |
| Z2-R-CINE-003 | Headless operation | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ExtractRender" |
| Z2-R-CINE-004 | Configuration keys validated | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Configuration keys" |
| Z2-R-CINE-005 | Public headers carry contracts | `Zenith2/Docs/Design/CINE.md` → "Rules across the CINE API"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Scope of this file" |
| Z2-R-CINE-006 | No forbidden module includes | `Zenith2/Docs/Design/CINE.md` → "Library dependencies" |
| Z2-R-CINE-007 | Error enumeration | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicErrors header" |
| Z2-R-CINE-008 | Command kinds registered with phase masks | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Command payloads" |
| Z2-R-CINE-009 | Lifecycle recorders take the kind table | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicCommands header" |
| Z2-R-CINE-010 | Tickets unique and resolvable | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicTypes header"; `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback" |
| Z2-R-CINE-011 | Start payload validated as untrusted | `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback" |
| Z2-R-CINE-012 | Internal kinds refused from other systems | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Command payloads" |
| Z2-R-CINE-013 | Instance limit | `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"; `Zenith2/Docs/Design/CINE.md` → "Capacities and constants" |
| Z2-R-CINE-014 | Cinematic document schema | `Zenith2/Docs/Design/CINE/Formats.md` → "Cinematic document" |
| Z2-R-CINE-015 | Document migration preserves unknown members | `Zenith2/Docs/Design/CINE/Formats.md` → "Document migration"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Document type registration" |
| Z2-R-CINE-016 | Cooked cinematic layout | `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked cinematic file" |
| Z2-R-CINE-017 | Cinematic load validation | `Zenith2/Docs/Design/CINE/Formats.md` → "Load validation" |
| Z2-R-CINE-018 | Cinematic format limits at and above limit | `Zenith2/Docs/Design/CINE/Formats.md` → "Sequence header section" |
| Z2-R-CINE-019 | Loaders fuzzed | `Zenith2/Docs/Design/CINE/Formats.md` → "Load validation"; `Zenith2/Docs/Design/CINE/Tests.md` → "Test targets" |
| Z2-R-CINE-020 | Runtime type of kind 14 | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicAsset header"; `Zenith2/Docs/Design/CINE/Formats.md` → "Runtime layout" |
| Z2-R-CINE-021 | Cook step cine.sequence | `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step" |
| Z2-R-CINE-022 | Cook reports every finding | `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook validation rules"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook error codes" |
| Z2-R-CINE-023 | Cook deterministic with schema-sensitive key | `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step" |
| Z2-R-CINE-024 | Subsequences flattened at cook | `Zenith2/Docs/Design/CINE/Formats.md` → "Subsequences" |
| Z2-R-CINE-025 | Video import | `Zenith2/Docs/Design/CINE/Authoring.md` → "Video importer"; `Zenith2/Docs/Design/CINE/Formats.md` → "Video import settings" |
| Z2-R-CINE-026 | Cooked video layout and validation | `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked video file"; `Zenith2/Docs/Design/CINE/Formats.md` → "Video load validation" |
| Z2-R-CINE-027 | Instance state machine | `Zenith2/Docs/Design/CINE/Playback.md` → "Instance state machine" |
| Z2-R-CINE-028 | Begin inline for prepared sequences | `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"; `Zenith2/Docs/Design/CINE/Playback.md` → "Playback model" |
| Z2-R-CINE-029 | Tracks write from the tick after Begin | `Zenith2/Docs/Design/CINE/Playback.md` → "Playback model"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-030 | Per-bound-entity lifecycle dispatch | `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvents header" |
| Z2-R-CINE-031 | Binding resolution errors | `Zenith2/Docs/Design/CINE/Playback.md` → "Binding resolution" |
| Z2-R-CINE-032 | Asynchronous binding through a publication source | `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding" |
| Z2-R-CINE-033 | Preload pins | `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding" |
| Z2-R-CINE-034 | Abandoned loads completed | `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding" |
| Z2-R-CINE-035 | Preparation window and timeout | `Zenith2/Docs/Design/CINE/Playback.md` → "Preparation" |
| Z2-R-CINE-036 | Prefetch ahead of sections | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-037 | Integer time advance | `Zenith2/Docs/Design/CINE/Playback.md` → "Time arithmetic" |
| Z2-R-CINE-038 | Keys fire on half-open ranges | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-039 | Sub-tick offsets | `Zenith2/Docs/Design/CINE/Playback.md` → "Time arithmetic"; `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks" |
| Z2-R-CINE-040 | Curve interpolation | `Zenith2/Docs/Design/CINE/Playback.md` → "Curve channels and interpolation" |
| Z2-R-CINE-041 | Rotation interpolation takes the shortest arc | `Zenith2/Docs/Design/CINE/Playback.md` → "Curve channels and interpolation" |
| Z2-R-CINE-042 | Evaluator bit-identical | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvaluator header" |
| Z2-R-CINE-043 | Record cap defers | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-044 | Instance buffers merge in instance order | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-045 | Transform tracks win arbitration | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"; `Zenith2/Docs/Design/CINE/Playback.md` → "Transform tracks" |
| Z2-R-CINE-046 | Transform spaces | `Zenith2/Docs/Design/CINE/Playback.md` → "Transform tracks" |
| Z2-R-CINE-047 | One writer per row | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms" |
| Z2-R-CINE-048 | Camera pose lens and lighting | `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate" |
| Z2-R-CINE-049 | Camera cuts | `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks" |
| Z2-R-CINE-050 | Camera director handover | `Zenith2/Docs/Design/CINE/Playback.md` → "Camera tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Begin" |
| Z2-R-CINE-051 | Animation sections drive cinematic slots | `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks" |
| Z2-R-CINE-052 | Slot time kept in step | `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks" |
| Z2-R-CINE-053 | Physics weight written by the cinematic | `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Field authority census rows" |
| Z2-R-CINE-054 | Audio sections posted by kind name | `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Reflected payloads" |
| Z2-R-CINE-055 | Deterministic audio instance IDs | `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks" |
| Z2-R-CINE-056 | Audio follows pause and rate | `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks" |
| Z2-R-CINE-057 | Event keys emitted the same tick | `Zenith2/Docs/Design/CINE/Playback.md` → "Event tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents" |
| Z2-R-CINE-058 | Reflected payloads | `Zenith2/Docs/Design/CINE/Playback.md` → "Reflected payloads" |
| Z2-R-CINE-059 | Outbox deferral | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents"; `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox" |
| Z2-R-CINE-060 | Dialogue prepared then played time-locked | `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks" |
| Z2-R-CINE-061 | Facial performance with dialogue | `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks" |
| Z2-R-CINE-062 | Caption cues from dialogue and subtitles | `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks" |
| Z2-R-CINE-063 | Captions follow seeks | `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing" |
| Z2-R-CINE-064 | Property tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Property tracks" |
| Z2-R-CINE-065 | Command tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Command tracks" |
| Z2-R-CINE-066 | VFX keys from command tracks | `Zenith2/Docs/Design/CINE/Playback.md` → "Command tracks"; `Zenith2/Docs/Design/CINE.md` → "Stage 5" |
| Z2-R-CINE-067 | Overlay tracks extracted only | `Zenith2/Docs/Design/CINE/Playback.md` → "Overlay tracks"; `Zenith2/Docs/Design/CINE/Formats.md` → "Render-world records" |
| Z2-R-CINE-068 | Markers | `Zenith2/Docs/Design/CINE/Playback.md` → "Marker tracks" |
| Z2-R-CINE-069 | Section eases and overlap | `Zenith2/Docs/Design/CINE/Playback.md` → "Section blending" |
| Z2-R-CINE-070 | Blend-in from the snapshot | `Zenith2/Docs/Design/CINE/Playback.md` → "Blending" |
| Z2-R-CINE-071 | Blend-out holds unflagged values | `Zenith2/Docs/Design/CINE/Playback.md` → "Blending" |
| Z2-R-CINE-072 | Natural end holds the last frame | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"; `Zenith2/Docs/Design/CINE.md` → "Rulings this design records" |
| Z2-R-CINE-073 | Take-over between instances | `Zenith2/Docs/Design/CINE/Playback.md` → "Take-over of a bound entity" |
| Z2-R-CINE-074 | Interrupt blends out from the current weight | `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback" |
| Z2-R-CINE-075 | Interrupt ends then restores | `Zenith2/Docs/Design/CINE/Playback.md` → "End playback" |
| Z2-R-CINE-076 | Cancel before Begin | `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback" |
| Z2-R-CINE-077 | Skip evaluates the end state once | `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback" |
| Z2-R-CINE-078 | Fire-on-skip keys in timeline order | `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback" |
| Z2-R-CINE-079 | Skip side work | `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Side work" |
| Z2-R-CINE-080 | Skip is terminal | `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback"; `Zenith2/Docs/Design/CINE.md` → "Rulings this design records" |
| Z2-R-CINE-081 | Skip leaves no pose pop | `Zenith2/Docs/Design/CINE/Playback.md` → "Animation tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Skip playback" |
| Z2-R-CINE-082 | Snapshot written and restore sets | `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |
| Z2-R-CINE-083 | Restore at the tick boundary | `Zenith2/Docs/Design/CINE/Playback.md` → "Tick-boundary handler"; `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |
| Z2-R-CINE-084 | Unflagged properties keep the final value | `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |
| Z2-R-CINE-085 | No cinematic writes after restore | `Zenith2/Docs/Design/CINE/Playback.md` → "Tick-boundary handler" |
| Z2-R-CINE-086 | Snapshot limits | `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |
| Z2-R-CINE-087 | Container properties snapshotted | `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |
| Z2-R-CINE-088 | Destroyed bindings dropped | `Zenith2/Docs/Design/CINE/Playback.md` → "Lifecycle listener"; `Zenith2/Docs/Design/CINE/Playback.md` → "Instance state machine" |
| Z2-R-CINE-089 | Seek applied before the advance | `Zenith2/Docs/Design/CINE/Playback.md` → "Seek playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing" |
| Z2-R-CINE-090 | Seek updates slots audio and captions | `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing" |
| Z2-R-CINE-091 | Seek fires keys only on request | `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing" |
| Z2-R-CINE-092 | Scrub equals play | `Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing" |
| Z2-R-CINE-093 | Rate pause and resume | `Zenith2/Docs/Design/CINE/Playback.md` → "Rate pause and resume playback" |
| Z2-R-CINE-094 | Determinism across workers | `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules" |
| Z2-R-CINE-095 | Replay reproduces cinematic runs | `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules" |
| Z2-R-CINE-096 | Video isolated from simulation | `Zenith2/Docs/Design/CINE/Video.md` → "Scope of this file"; `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules" |
| Z2-R-CINE-097 | Exact save participant | `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"; `Zenith2/Docs/Design/CINE/Formats.md` → "Save section" |
| Z2-R-CINE-098 | Save re-issues in-flight loads | `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore" |
| Z2-R-CINE-099 | Changed content at load | `Zenith2/Docs/Design/CINE/Formats.md` → "Save section" |
| Z2-R-CINE-100 | Player component | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicComponents header"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-101 | Census rows | `Zenith2/Docs/Design/CINE/Playback.md` → "Field authority census rows" |
| Z2-R-CINE-102 | Luau request functions | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface" |
| Z2-R-CINE-103 | Luau queries | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface" |
| Z2-R-CINE-104 | Catalogued misuse errors | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Misuse codes" |
| Z2-R-CINE-105 | Cinematic events for scripts | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvents header" |
| Z2-R-CINE-106 | Memory consumer and shedding | `Zenith2/Docs/Design/CINE/Playback.md` → "Memory consumer" |
| Z2-R-CINE-107 | Evaluation CPU budget | `Zenith2/Docs/Design/CINE.md` → "Rows CINE owns"; `Zenith2/Docs/Design/CINE/Tests.md` → "Benchmarks" |
| Z2-R-CINE-108 | Cinematic memory budget | `Zenith2/Docs/Design/CINE.md` → "Rows CINE owns" |
| Z2-R-CINE-109 | Pending budget rows exist | `Zenith2/Docs/Design/CINE.md` → "Pending row needs" |
| Z2-R-CINE-110 | D-SEL-VIDEO harness and ruling | `Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria" |
| Z2-R-CINE-111 | Decoder interface | `Zenith2/Docs/Design/CINE/Video.md` → "Zenith_VideoDecoder interface" |
| Z2-R-CINE-112 | Windows platform decoder | `Zenith2/Docs/Design/CINE/Video.md` → "Platform decoder backend" |
| Z2-R-CINE-113 | Android platform decoder | `Zenith2/Docs/Design/CINE/Video.md` → "Platform decoder backend" |
| Z2-R-CINE-114 | Video thread hand-off without locks | `Zenith2/Docs/Design/CINE/Video.md` → "Video thread" |
| Z2-R-CINE-115 | Video thread lifetime | `Zenith2/Docs/Design/CINE.md` → "Threads CINE owns"; `Zenith2/Docs/Design/CINE/Video.md` → "Video thread" |
| Z2-R-CINE-116 | Video player state machine | `Zenith2/Docs/Design/CINE/Video.md` → "Video players" |
| Z2-R-CINE-117 | Video seek | `Zenith2/Docs/Design/CINE/Video.md` → "Video players" |
| Z2-R-CINE-118 | Frame selection | `Zenith2/Docs/Design/CINE/Video.md` → "Frame selection" |
| Z2-R-CINE-119 | Video conversion | `Zenith2/Docs/Design/CINE/Video.md` → "GPU side" |
| Z2-R-CINE-120 | Video composite | `Zenith2/Docs/Design/CINE/Video.md` → "VideoComposite pass" |
| Z2-R-CINE-121 | Video surfaces | `Zenith2/Docs/Design/CINE/Video.md` → "Texture surfaces" |
| Z2-R-CINE-122 | Video upload within the ring budget | `Zenith2/Docs/Design/CINE/Video.md` → "GPU side" |
| Z2-R-CINE-123 | Audio and video sync bound | `Zenith2/Docs/Design/CINE/Video.md` → "Audio and video sync" |
| Z2-R-CINE-124 | Device audio and video sync | `Zenith2/Docs/Design/CINE/Video.md` → "Audio and video sync" |
| Z2-R-CINE-125 | Video failure isolation | `Zenith2/Docs/Design/CINE/Video.md` → "Video failure handling" |
| Z2-R-CINE-126 | Android suspend and resume | `Zenith2/Docs/Design/CINE/Video.md` → "Android lifecycle" |
| Z2-R-CINE-127 | Capture state machine | `Zenith2/Docs/Design/CINE/Video.md` → "Capture state machine" |
| Z2-R-CINE-128 | Frame-exact capture | `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm" |
| Z2-R-CINE-129 | Capture audio and subtitles | `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm"; `Zenith2/Docs/Design/CINE/Formats.md` → "Capture output" |
| Z2-R-CINE-130 | Capture output format | `Zenith2/Docs/Design/CINE/Formats.md` → "Capture output" |
| Z2-R-CINE-131 | Capture failure | `Zenith2/Docs/Design/CINE/Video.md` → "Capture state machine" |
| Z2-R-CINE-132 | Capture absent in Shipping | `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm" |
| Z2-R-CINE-133 | Export package | `Zenith2/Docs/Design/CINE/Video.md` → "Export algorithm"; `Zenith2/Docs/Design/CINE/Formats.md` → "Export output" |
| Z2-R-CINE-134 | Export validation | `Zenith2/Docs/Design/CINE/Video.md` → "Export algorithm" |
| Z2-R-CINE-135 | Sequencer panel registered | `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer panel" |
| Z2-R-CINE-136 | Sequencer commands undoable | `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer commands"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer model" |
| Z2-R-CINE-137 | Binding rename rewrites players | `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer commands" |
| Z2-R-CINE-138 | Preview through the runtime | `Zenith2/Docs/Design/CINE/Authoring.md` → "Preview session" |
| Z2-R-CINE-139 | Camera pilot and key recording | `Zenith2/Docs/Design/CINE/Authoring.md` → "Camera pilot and key recording" |
| Z2-R-CINE-140 | Cook diagnostics in the sequencer | `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook error codes" |
| Z2-R-CINE-141 | Authoring walkthrough automated | `Zenith2/Docs/Design/CINE/Authoring.md` → "Authoring walkthrough" |
| Z2-R-CINE-142 | Caption keys extracted | `Zenith2/Docs/Design/CINE/Authoring.md` → "Localization extraction" |
| Z2-R-CINE-143 | Counters and zones | `Zenith2/Docs/Design/CINE.md` → "Counters"; `Zenith2/Docs/Design/CINE.md` → "Profiler zones" |
| Z2-R-CINE-144 | Console commands and panel | `Zenith2/Docs/Design/CINE.md` → "Console commands"; `Zenith2/Docs/Design/CINE.md` → "On-screen panel" |
| Z2-R-CINE-145 | CinematicSlice game | `Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice" |
| Z2-R-CINE-146 | CinematicSlice set-pieces | `Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice" |
| Z2-R-CINE-147 | CinematicSlice acceptance | `Zenith2/Docs/Design/CINE/Authoring.md` → "CinematicSlice" |
| Z2-R-CINE-148 | Spawnable bindings | `Zenith2/Docs/Design/CINE.md` → "Stage 5"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicAsset header" |
| Z2-R-CINE-149 | Outbox emission boundary | `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents" |
| Z2-R-CINE-150 | End tick written | `Zenith2/Docs/Design/CINE/Playback.md` → "End tick"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms" |
| Z2-R-CINE-151 | Zero blend-in at full weight | `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-152 | Presentation resumed after load | `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"; `Zenith2/Docs/Design/CINE/Playback.md` → "Side work" |
| Z2-R-CINE-153 | Schedule freezes with Physics and Animation | `Zenith2/Docs/Design/CINE/Playback.md` → "Systems"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate" |
| Z2-R-CINE-154 | Script-visible identifiers exact in Luau | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Scope of this file"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Luau surface"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicEvents header" |
| Z2-R-CINE-155 | ZenithAuthoring contract conformance | `Zenith2/Docs/Design/CINE.md` → "ZenithAuthoring and ZenithCook files"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Tool-side headers"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook steps" |
| Z2-R-CINE-156 | Decoded frames cropped to the display region | `Zenith2/Docs/Design/CINE/Video.md` → "Platform decoder backend"; `Zenith2/Docs/Design/CINE/Video.md` → "GPU side"; `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Flux_Video header" |
| Z2-R-CINE-157 | Decoder test seam and pumped video thread | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"; `Zenith2/Docs/Design/CINE/Video.md` → "Video thread"; `Zenith2/Docs/Design/CINE/Video.md` → "Zenith_VideoDecoder interface" |
| Z2-R-CINE-158 | Captions cancelled and re-queued by source | `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks" |
| Z2-R-CINE-159 | Merge capacity rejection re-runs whole | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate" |
| Z2-R-CINE-160 | Restore skips removed components | `Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot" |
| Z2-R-CINE-161 | Own-row writes of rows reached by handle | `Zenith2/Docs/Design/CINE/Playback.md` → "Systems"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyTransforms"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" |
| Z2-R-CINE-162 | Emission acceptance and re-emission | `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.EmitEvents"; `Zenith2/Docs/Design/CINE/Playback.md` → "Emission acceptance"; `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox" |
| Z2-R-CINE-163 | Outbox sized from the instance pool | `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox"; `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback" |
| Z2-R-CINE-164 | Fresh audio IDs for every post | `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks"; `Zenith2/Docs/Design/CINE/Playback.md` → "Side work" |
| Z2-R-CINE-165 | Pause and blend-out continuity | `Zenith2/Docs/Design/CINE/Playback.md` → "Blending"; `Zenith2/Docs/Design/CINE/Playback.md` → "Interrupt playback" |
| Z2-R-CINE-166 | Ended holder base capture | `Zenith2/Docs/Design/CINE/Playback.md` → "Take-over of a bound entity"; `Zenith2/Docs/Design/CINE/Playback.md` → "Tick-boundary handler" |
| Z2-R-CINE-167 | Deterministic slerp | `Zenith2/Docs/Design/CINE/Playback.md` → "Curve channels and interpolation"; `Zenith2/Docs/Design/CINE/Playback.md` → "Determinism rules" |
| Z2-R-CINE-168 | Bound set saved | `Zenith2/Docs/Design/CINE/Playback.md` → "Bound set"; `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"; `Zenith2/Docs/Design/CINE/Formats.md` → "Save section" |
| Z2-R-CINE-169 | Bindings resolved by the publication handler | `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding"; `Zenith2/Docs/Design/CINE/Playback.md` → "Binding resolution" |
| Z2-R-CINE-170 | Video events and single-owner player state | `Zenith2/Docs/Design/CINE/Video.md` → "Video thread"; `Zenith2/Docs/Design/CINE/Video.md` → "Video players" |
| Z2-R-CINE-171 | Capture subtitles resolved at export | `Zenith2/Docs/Design/CINE/Formats.md` → "Subtitle file"; `Zenith2/Docs/Design/CINE/Formats.md` → "Export output" |
| Z2-R-CINE-172 | Capture and export audio limit | `Zenith2/Docs/Design/CINE/Formats.md` → "Capture directory layout"; `Zenith2/Docs/Design/CINE/Formats.md` → "Export output"; `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm" |
| Z2-R-CINE-173 | Ended camera lens left to the director | `Zenith2/Docs/Design/CINE/Playback.md` → "End tick"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.ApplyLate" |
| Z2-R-CINE-174 | Lip-sync face clips follow the voice locale | `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks"; `Zenith2/Docs/Design/CINE/Formats.md` → "Section table section"; `Zenith2/Docs/Design/CINE/Authoring.md` → "cine.sequence step" |
| Z2-R-CINE-175 | Looping minimum duration | `Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"; `Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate"; `Zenith2/Docs/Design/CINE/Authoring.md` → "Cook validation rules" |

## Reconciliation items

Items in this design that depend on another area's pending change; each is fenced by an assumption block where it is used and listed in "Cross-area rows":

| Item | Where | Resolution |
|---|---|---|
| kind 14's runtime type name `Zenith_Cinematic` versus ASSET's `Zenith_CinematicAsset` | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicAsset header" | ASSET renames the table entry (CN1) |
| the kinds parameter of the lifecycle recorders and the four extra start-description members (the tag a `Zenith_StringID`) | `Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicCommands header" | core-doc items X290 and X339 (b) |
| CINE's ZenithAuthoring layout, cook-step home and validators | `Zenith2/Docs/Design/CINE.md` → "ZenithAuthoring and ZenithCook files" | conforms to EDIT's contract; EDIT updates its owner row's state (ED45) |
| UI's caption queue keyed by source | `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks" | UI adds the source ID and the cancel event (CN20) |
| CHAR's lip-sync playback contract for dialogue face clips (rows CH3, CH19) | `Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks" | CINE answers with the cooked window table, `m_xVoiceLocale` and the window-end fire item (CN26); ANIM publishes the clip markers (CN27) and adds `ANIM_SLOT_TAG_FULL_RATE` (CH16); CHAR lifts its `pending CINE` fence |
| UI4's emission range "phases 2–8" | `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks" | CINE emits in phase 8, inside the range (CN10) |
| AUDIO's statement that other modules reach it through components, bridges or Luau | `Zenith2/Docs/Design/CINE/Playback.md` → "Audio tracks" | CINE records AUDIO's registered kinds by name with reflected payloads and includes no AUDIO header, which [Z2-N-ARCH-074] permits; AUDIO confirms (CN9) |
| GAME's scenario sentence "the director is suspended from the start playback to the restore boundary" beside its listener resuming at SKIP | `Zenith2/Docs/Design/GAME/Cameras.md` → "Cinematic suspension" | SKIP is terminal and dispatched at the skip playback; GAME aligns the scenario sentence (CN7) |

## Cross-area rows

Rows CN1 to CN28 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` record what CINE needs from ASSET, SCRIPT, ARCH, MOD, ECS, GAME, RPROD, AUDIO, UI, PHYS, BUDGET, EDIT, ANIM, DEVT, SAVE, LIGHT, VFX, CONTRACT, PLAT, CORE and CHAR (CN22–CN25 raised by revision 2: ECS's `SetContainer` at the tick boundary, ECS's ordinal event writer, GA16's current-entity rule for CINE, CORE's deterministic transcendentals, now landed; CN26 answers CHAR's CH3 and CH19, CN27 asks ANIM for the clip markers the cook reads; CN2 and CN25 are answered). The core-document items are queued in `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`:

| Item | Target | Content |
|---|---|---|
| X289 | `Zenith2/Docs/Architecture.md` → "Library dependency graph" | the ZenithCinematics row's dependencies and layer |
| X290 | `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip" | the kinds parameter and the start-description members |
| X291 | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" | `ASYNC_SOURCE_CINEMATICS` and CINE's misuse block |
| X292 | `Zenith2/Docs/Rendering/FrameAnatomy.md` → "Common prologue", "Common epilogue" | rows 2b `VideoConvert` and 6b `VideoComposite`, and the render-world video and overlay records |
| X293 | `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" | the `Zenith.VideoDec` thread |
| X294 | `Zenith2/Docs/Budgets.md` → "Engine service CPU rows", "Rendering capacity and latency rows" | the three pending rows of "Pending row needs" |
| X313 | `Zenith2/Docs/Architecture.md` → "Shedding per subsystem"; `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces", "The frame loop"; `Zenith2/Docs/Rendering/FrameAnatomy.md` → "Common epilogue" | filed by CINE_round1: (a) the Cinematics shedding row; (b) the block ZS2700–ZS2799; (c) `AddTickBoundaryHandler` and the playback follow-up buffer; (d) the development capture tick gate; (e) the order of `VideoComposite` against TWOD's before-UI point |
| X339 | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip"; `Zenith2/Docs/Architecture.md` → "Library dependency graph" | (a) `Zenith_ModuleStartContext::GetStringTable()`, the process-lifetime table, valid until the module's shutdown; (b) amends X290: the start description's tag is `Zenith_StringID m_xTag`; (c) amends X289: no ZenithPlatform edge |
| X344 | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" | MOD+SCRIPT's sink patch: `GetPublicationSink`, `FindCommandKind` on `Zenith_WorldAttachContext` (CINE cites it; not CINE's item) |

