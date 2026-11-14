# CINE authoring

## Scope of this file

How cinematics are authored and cooked (Z2-I-CINE-13, Z2-D-079): the document type registration, the video importer, the cook steps and their validation, the sequencer panel and its preview, the player component inspector, the capture and export surfaces in the editor, localization extraction of caption keys, an authoring walkthrough, and the CinematicSlice game CINE owns. Z2-D-079 makes visual authoring of cinematics an editor tool: the sequencer is a panel of the editor's extension mechanism (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"), listed in EDIT's extension table under `Internal/Cinematics/` (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension table"; row ED15), ticketed under CINE's epic; EDIT supplies the timeline widget, the curve editor, preview worlds and undo (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Timeline", "Curve editor").

## Document type registration

`Zenith_RegisterCinematicDocumentTypes(Zenith_MigrationRegistry&)` (declared in `Zenith2/Tools/Authoring/Zenith_AuthoringCinematics.h`, defined in `Zenith2/Tools/Authoring/Internal/Cinematics/Zenith_CinematicDocumentTypes.cpp`) registers one `Zenith_DocumentTypeDesc`: stable name `Zenith.Cinematics.SequenceDocument`, suffix `.cinematic.json`, schema tag `zenith2.cinematic`, current version 1, the canonical member order of `Zenith2/Docs/Design/CINE/Formats.md` → "Cinematic document", no migration steps at version 1 (`Zenith2/Docs/Design/CINE/Formats.md` → "Document migration"). ZenithCook and ZenithEditor both call it through ZenithAuthoring (Q84 (a); the layout of `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules", owner directory `Cinematics`, row ED45). Its migration fixtures, from the first version bump on, are `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Cinematics/SequenceDocument/v<N>.json` and `v<N>.expected.json`; version 1 has none. Row CN1 asks ASSET to add the row to `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types".

## Video importer

`cine.video` (`Zenith2/Tools/ZenithCook/Internal/Cinematics/Zenith_VideoImporter.cpp`; the importer runs only in ZenithCook, so it stays there under the AUTHORING-CONTRACT ruling), registered with ASSET's importer registry for the source extension `.mp4`; produces the main product `video:<name>` (kind 21) and, when the source has an audio track and `audio` is `"extract"`, a sub-asset `sound:<name>` (kind 10) whose intermediate is a 48 kHz PCM WAV handed to AUDIO's `audio.sound` step (AUDIO owns the sound's encoding, `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.sound step"). Demuxing is CINE's own ISO base media file parser (`Zenith2/Tools/ZenithCook/Internal/Cinematics/Mp4/Zenith_Mp4Reader.h/.cpp`, written from the D-SEL-VIDEO harness's reader, `Zenith2/Docs/Design/CINE/Video.md` → "D-SEL-VIDEO candidates and criteria"): boxes `ftyp`, `moov/trak/mdia/minf/stbl` (`stsd` with `avc1`/`avcC` or `hvc1`/`hvcC`, `stts`, `ctts`, `stss`, `stsz`, `stsc`, `stco`/`co64`), `mdat`; audio `mp4a` with AAC-LC decoded to PCM through the platform decoder of the host (Media Foundation on Windows) — the importer is host-only. The parser is bounded (box depth ≤ 16, box count ≤ 1,000,000, sample count ≤ 2²⁴). It has no libFuzzer target, because fuzz targets exist in `win-asan` alone and ZenithCook is `ZENITH_TOOLS_HOST` (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets"; EDIT's contract rule 2); its robustness test is `Test_VideoImport_Mp4Mutations_RefusedWithoutCrash` in `ZenithCook_Tests` (20,000 seeded byte mutations of the `SyntheticMp4` fixture, seed printed, each returning a result and none asserting). The settings are `Zenith2/Docs/Design/CINE/Formats.md` → "Video import settings".

## Cook steps

`Zenith_RegisterCinematicCookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)` (`Zenith2/Tools/ZenithCook/Internal/Cinematics/Zenith_CinematicCookSteps.h/.cpp`; cook-step registration stays in ZenithCook, because ZenithAuthoring may name no ZenithCook type, `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 3) registers, in ASSET's canonical module order (`Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions"):

| Step ID | Version | Phase | Kind | Runs after | Tool versions | Out of process |
|---|---|---|---|---|---|---|
| `cine.video` | 1 | `COOK_PHASE_ASSET` | 21 | — | the host encoder version when `transcode` | yes |
| `cine.sequence` | 1 | `COOK_PHASE_ASSET` | 14 | `cine.video`, `audio.sound`, `anim.clip` | — | no (it opens source documents and reads the type registry) |

Validators (`RegisterValidator`): kinds 14 and 21, each calling the public ZenithCinematics functions `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile` (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_CinematicAsset header", "Zenith_VideoAsset header") and converting the error's check number into the finding; no tool compiles a ZenithCinematics `Internal/` source, and the cook's structural validation is exactly the loaders' checks L1–L12 and V1–V7. Content rules (`RegisterRule`): kind 14 quantity `durationSeconds`, kind 21 quantities `pixelsPerFrame` and `megabytes`; their limits come from `Zenith2/Docs/ContentSpecs.md` (the Stage 1 ContentSpecs ticket).

### cine.sequence step

1. Open the source document with `Zenith_CookContext::OpenSourceDocument` (migrated in memory to version 1).
2. Flatten subsequences depth-first (each child opened the same way and recorded as a dependency; a cycle or a depth above 4 is `cine.cook.subsequence_cycle` / `cine.cook.subsequence_depth`).
3. Drop muted tracks; resolve every binding's driver list from its tracks; resolve component stable names and property names against the game's frozen type registry from the cook services (`cine.cook.unknown_type`, `cine.cook.unknown_property`); resolve event types, command kinds' payload types and their properties likewise (command kind names are checked against the game's composition: the kind table of the modules in `game.json`, `cine.cook.unknown_command_kind`).
4. Validate every rule of "Cook validation rules", collecting every finding (the cook reports all, not the first).
5. Hash names (`Zenith_Hash64`), intern strings, sort bindings by hash and markers by time, convert each audio event path and dialogue line to its 64-bit ID with the same hash AUDIO uses (`Zenith_Hash64` of the UTF-8 text; row CN9 confirms the rule), and convert per-channel rotation keys to sign-aligned quaternions.
6. Write the sections of `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked cinematic file" with `Zenith_CookedFileWriter` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_CookedFile header"); record hard dependencies (clips, sounds, videos, face clips) in the catalogue and provenance dependencies (subsequence sources).
7. Generate a time-locked audio section for every video section whose video has an audio sound (the sound GUID from the video's header).
8. For every dialogue section with a face clip, read the clip's `anim.clip.markers` property with `Zenith_CookContext::ReadDependencyOutput(clip, "anim.clip.markers")` (a cook-input edge, so editing the clip's markers re-cooks the sequence) and write the window table ("Section table section" of `Zenith2/Docs/Design/CINE/Formats.md`): for each marker named `char.lipsync.*` in time order, T = its time and e = the time of the first `char.lipsync.windowEnd` event after it, both converted once to integer µs with `llround(double(t) × 1e6)`; and one window-end fire item per record whose `sectionStart + (e − T)` is below the section's end.

Complexity O(K log K) for K keys. Derived-data key: the document's canonical bytes hash, every subsequence document's hash, the step version, the engine cook version and the hash of the type-registry subset the step resolved (stable names, property IDs and value kinds of every referenced type), so a component schema change re-cooks exactly the sequences that reference it.

### cine.video step

1. Read the importer's intermediate (remuxed elementary stream with sample table, or a transcoded one).
2. Build the frame index in decode order, the presentation order and the keyframe index; write the bitstream section uncompressed (codec none) and the other sections compressed.
3. Validate V1–V7 and the importer's limits.

Derived-data key: source bytes hash, the `.meta` settings, step version, and the encoder version when transcoding.

### Cook validation rules

| Rule ID | Check | Severity |
|---|---|---|
| `cine.cook.duration` | `durationUs` in [1, 3.6 × 10⁹] | error |
| `cine.cook.binding_limit` | ≤ 64 bindings; names unique and well-formed | error |
| `cine.cook.track_limit` | ≤ 512 tracks after flattening; ≤ 64 channels per track; ≤ 4,096 sections per track and ≤ 65,536 sections in all; ≤ 65,536 keys per channel; ≤ 65,536 event keys | error |
| `cine.cook.fire_on_skip_limit` | ≤ 8 event keys and sections flagged `fireOnSkip` in the flattened sequence (`uCINE_MAX_FIRE_ON_SKIP_KEYS`; the outbox bound of `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox") | error |
| `cine.cook.video_10bit_android` | a video with a 10-bit profile in a game whose `game.json` targets Android | error |
| `cine.cook.video_pause_audio` | a video section with `pauseWithInstance: false` whose video has an audio sound (its audio would stop with the instance's pause while the video plays on, `Zenith2/Docs/Design/CINE/Playback.md` → "Video tracks") | error |
| `cine.cook.loop_too_short` | a sequence shorter than `ulCINE_MIN_LOOP_SCALED / tickHz` µs at the game's tick rate (8,000,000 / 60 = 133,334 µs at 60 Hz) is refused as a looping start at run time (`Zenith2/Docs/Design/CINE/Playback.md` → "Start playback" step 1); the warning names that minimum | warning |
| `cine.cook.lipsync_windows` | a dialogue face clip carries at most 16 `char.lipsync.<tag>` markers, each followed by a `char.lipsync.windowEnd` event, read from the clip's published `anim.clip.markers` (`Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks") | error |
| `cine.cook.caption_overlap` | at most 16 subtitle and dialogue sections active at any time in the flattened sequence (`uCINE_MAX_CAPTIONS_PER_INSTANCE`, the caption list of `Zenith2/Docs/Design/CINE/Playback.md` → "Subtitle tracks") | error |
| `cine.cook.spawnable_prefab` | (Stage 5) a binding's `prefab` names a prefab asset (kind 6) of the catalogue, and no player of the game's scenes binds a spawnable binding | error |
| `cine.cook.unknown_binding` | every track's `binding` and `anchor` names a binding | error |
| `cine.cook.duplicate_driver` | no two tracks of one binding drive the same (component, property, channel) | error |
| `cine.cook.overlap_limit` | ≤ 8 sections active at any time on a track | error |
| `cine.cook.key_order` | keys strictly ascending per channel; values finite; times within the duration | error |
| `cine.cook.section_bounds` | `start < end ≤ duration`; eases fit | error |
| `cine.cook.unknown_type`, `cine.cook.unknown_property`, `cine.cook.value_kind` | property tracks, event and command payloads match the registry | error |
| `cine.cook.arbitrated_property` | a property track never targets an arbitrated property | error |
| `cine.cook.unknown_command_kind` | a command track's kind is registered by a module of the game's composition, and its allowed phases include phase 1 | error |
| `cine.cook.payload_size` | a templated payload ≤ 256 bytes | error |
| `cine.cook.subsequence_cycle`, `cine.cook.subsequence_depth` | no cycle; depth ≤ 4 | error |
| `cine.cook.missing_dependency` | every referenced clip, sound, video, face clip and dialogue line exists in the catalogue (dialogue lines through AUDIO's event index output) | error |
| `cine.cook.rotation_flip` | per-channel rotation keys sign-aligned (fixed by the cook; reported as info) | info |
| `cine.cook.camera_binding` | a camera track's binding is flagged `camera` and lists `Zenith.Render.Camera` | warning |
| `cine.cook.unused_binding` | a binding no track uses | warning |
| `cine.cook.caption_key` | every subtitle and dialogue caption key exists in the game's localization source table (UI's extraction, "Localization extraction") | warning |

### Cook error codes

Findings are reported through `Zenith_CookContext`'s diagnostics with the rule ID, the document path and the JSON pointer of the offending member (for example `/tracks/3/sections/0/endUs`), so the editor's sequencer and the asset browser select the member. Any error fails the item with `COOK_ERROR_VALIDATION`.

## Sequencer panel

`Zenith_EditorSequencerPanel` (`Zenith2/Tools/ZenithEditor/Internal/Cinematics/Zenith_EditorSequencerPanel.cpp`), registered by `Zenith_EditorCinematicsExtension` (`Internal/Cinematics/Zenith_EditorCinematicsExtension.cpp`, an entry of EDIT's extension table) as a multi-instance panel type "Sequencer" (menu path `Window/Cinematics/Sequencer`), a document handler for `Zenith.Cinematics.SequenceDocument` (opening a `.cinematic.json` opens a sequencer on it), an asset kind handler for kind 14 (create, open, thumbnail = the first camera frame rendered in a preview world) and a component inspector for `Zenith.Cinematics.Player`.

Layout: a toolbar (play, pause, stop, step back and forward one tick, loop range toggle, rate selector 0.25×–2×, the current time as `mm:ss.fff` and the tick index, snap toggle, record toggle, capture button); a binding list (left) with each binding's entity picker; the timeline (`Zenith_EditorTimeline`) with one row per track grouped by binding, showing sections as ranges, keys as diamonds, cuts as vertical bars, markers on the ruler; a curve editor (`Zenith_EditorCurveEditor`) for the selected channels; a detail inspector for the selected section or key (the JSON document editor's property editors restricted to the member).

### Sequencer model

`Zenith_EditorSequencerModel` (`Internal/Cinematics/Zenith_EditorSequencerModel.h`, no ImGui, unit-tested headless) holds the document handle, the selection, the playhead (µs), the view range, the preview instance ID and the dirty flag. Every gesture becomes a command of the model that produces JSON edits of the document (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Commands and history"), so undo and redo are the editor's; the model holds no authored state the document does not.

### Sequencer commands

| Command | Edits |
|---|---|
| `Cinematics.AddTrack(kind, binding)` | appends a track with a fresh GUID and default members |
| `Cinematics.RemoveTrack(id)` | removes it |
| `Cinematics.AddBinding(name)`, `RenameBinding(old, new)` | binding list; rename rewrites every track and every scene player that names it (a batch edit through the document framework) |
| `Cinematics.AddKey(track, channel, tUs, value)` | inserts a key, keeping order; an existing key at the time is replaced |
| `Cinematics.MoveKeys(selection, ΔtUs, Δvalue)` | moves keys; snapping to ticks of the preview rate, to keys within 6 px, to markers |
| `Cinematics.SetTangents(selection, mode)` | auto (Catmull-Rom), flat, broken |
| `Cinematics.AddSection(track, startUs, endUs, payload)`, `ResizeSection`, `MoveSection`, `RemoveSection` | sections |
| `Cinematics.AddCut(track, tUs)`, `AddMarker(name, tUs)` | camera cuts, markers |
| `Cinematics.SetDuration(us)` | duration; refuses a value that cuts off keys unless the trim option is chosen |
| `Cinematics.RecordCameraKey(track)` | keys the camera track's pose and lens from the editor viewport camera at the playhead ("Camera pilot and key recording") |
| `Cinematics.Nudge(selection, ticks)` | moves by whole ticks of the preview rate |

### Preview session

The sequencer previews in a preview world (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Preview worlds"), or in the edit world's copy when the binding entities are scene entities the user picked (the preview world loads the scene the bindings come from as a read-only copy, so previewing never changes the edit world). Playback in the preview world uses the runtime itself: the panel starts the cooked sequence (cooked in process through the cook client on every document change, debounced 250 ms) with `uCINE_START_FLAG_PAUSED`, records `Cinematics.Resume`, `Pause`, `SetRate` and `Seek` for the toolbar and the playhead, and the preview world ticks at the game's tick rate while the panel is visible. Scrubbing records one `Cinematics.Seek` per playhead change (fire-events off by default; a toolbar toggle turns on `fireOnSeek` keys). So a preview frame is the exact runtime evaluation (`Zenith2/Docs/Design/CINE/Playback.md` → "Seeking and scrubbing"); the blend preview starts the sequence from a snapshot of the preview world's bound entities with the document's blend-in.

### Camera pilot and key recording

"Pilot" binds the editor viewport camera to the camera track's binding for the preview: the viewport shows the cinematic camera and moving the viewport camera with the editor's camera controls moves it; `Cinematics.RecordCameraKey` keys position, rotation (per-channel, sign-aligned), FovY and focus distance at the playhead. With record on, every viewport camera change while paused keys automatically.

### Binding picker

A binding row's picker selects an entity of the open scene (or the preview scene) by name or by clicking it in the viewport; the choice is stored where the cinematic is used — in a `Zenith.Cinematics.Player`'s `Bindings` of the scene, or as the preview's own binding table in the sequencer's `editor` member — never in the sequence's bindings, which only name slots.

## Player component inspector

`Zenith.Cinematics.Player`'s inspector shows the sequence picker, the binding table generated from the sequence's slots (one entity picker per slot, required slots marked), blends, rate, loop and play-on-start, and buttons "Open in sequencer" and "Preview". In play mode it shows `Instance` and `State` read-only.

## Capture and export in the editor

The sequencer's capture button opens a dialog (name, size, frame rate = the game's tick rate, include UI, audio, subtitles) and runs the capture of `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm" in a child process of the game's Player started with the offline audio backend and external ticks, driven over DEVT's remote console ("Capture state machine" there), because a running audio system cannot switch to the offline backend; a progress bar shows frames written and pending, from the child's `FRAME_SUMMARY` counters. When the child exits, the dialog offers export of `user:/devtools/captures/<name>/`. "Export" runs `Zenith_ExportCinematicCapture` on a capture directory, in a background job of the editor's tool job pool; `Zenith_ExportCinematicCapture` takes no progress callback, so the dialog shows a busy indicator until the job finishes and then its result; the dialog sets `m_strLocalizationDirectory` to the game's `Localization/<locale>/` for the locale the user picks (default: the project's source locale), so the WebVTT carries resolved text.

## Localization extraction

Subtitle and dialogue caption keys reach UI's localization extraction: ZenithCook's `Internal/Cinematics/Zenith_CinematicExtractionSource.cpp` registers an extraction source for `Zenith.Cinematics.SequenceDocument` that calls ZenithAuthoring's `Zenith_ListCinematicCaptionKeys` and yields every `subtitleKey`, `speakerKey` and subtitle `key` with the document path and JSON pointer as the context (`Zenith2/Docs/Design/UI/Localization.md` → "Extraction sources"; row CN10). A missing key is the warning `cine.cook.caption_key`.

## Authoring walkthrough

The CinematicSlice "intro" sequence is the walkthrough the authoring acceptance replays through the editor's automation (`Zenith2/Docs/Design/EDIT/Automation.md` → "Command registry"), so each step is a command:

1. Create `Assets/Cinematics/Intro.cinematic.json` from the asset browser (kind 14 handler); the sequencer opens.
2. Add bindings `Hero`, `Mentor`, `ShotCamera` (camera); pick the scene's entities in the binding picker.
3. Add a camera track on `ShotCamera`; pilot the viewport and record keys at 0 s, 3 s, 6 s; add a cut at 6 s; key FovY.
4. Add animation tracks on `Hero` and `Mentor` with clip sections from the asset browser (drag and drop onto the row).
5. Add a dialogue track on `Mentor` with two lines and their caption keys; add a subtitle for a sound caption.
6. Add an event track with `Gameplay.QuestAdvance` at the end, `fireOnSkip` on.
7. Add an overlay fade-in over 0.5 s.
8. Scrub, play at 0.5×, preview the blend-in; fix the cook warnings shown in the sequencer.
9. Bind the sequence to a trigger in the scene with a `Zenith.Cinematics.Player` and a Luau behaviour that calls `Cinematics.Start` on overlap and `Cinematics.Skip` on the skip action.
10. Play in editor; skip; observe the restored gameplay state; capture 10 s and export PNG and WebVTT.

## CinematicSlice

CINE owns `Zenith2/Games/CinematicSlice` (Z2-D-021; the flagship slice of W-Cinematic, `Zenith2/Docs/Budgets.md` → "W-Cinematic"). `game.json` selects the modules Animation, Audio, Cinematics, Environment, Gameplay, Physics, UI and VFX, the custom ECS backend, Jolt, and Android plus Windows (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping"). Like every game it contains no C++ (Z2-D-002): scenes, Luau scripts, schemas, sequences, and assets.

| Content | Stage | Purpose |
|---|---|---|
| `Scenes/Arena.scene.json` and three location scenes | S4 | the playable spaces between cinematics |
| `Assets/Cinematics/Intro`, `Ambush`, `Escape`, `Finale` (`.cinematic.json`) | S4 | camera, animation, audio, dialogue, subtitle, event, overlay tracks; `Finale` carries a video section |
| `Assets/Video/Flashback.mp4` | S4 (decoded per D-SEL-VIDEO) | the video playback case; a binary source asset, so it is produced by a `needs-human` content ticket and never by a worker diff (Q55) |
| `Scripts/CinematicDirector.luau` | S4 | starts, skips and interrupts sequences; resumes gameplay |
| set-pieces (physics-driven collapse authored as a command track) | S5 | VFX and physics events from command tracks (VFX lands in Stage 5) |
| `Acceptance/` | S6 | `Playthrough.md`, `Playthrough.replay`, `FeatureChecklist.md`, `Budgets.md`, `BugBar.md`, `SignOff.md` (`Zenith2/Docs/Budgets.md`'s acceptance package) |

The slice's acceptance list is [Z2-N-BUDGET-434] and [Z2-N-BUDGET-435]; its playthrough is 20–30 minutes and exercises every cinematic, each skip and an interrupt, a save and load mid-cinematic, and a capture of the intro. The "Stage 1 flagship acceptance ticket for CinematicSlice" sets its content scale.
