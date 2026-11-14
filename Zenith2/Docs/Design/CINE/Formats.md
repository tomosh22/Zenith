# CINE formats

## Scope of this file

Every file and record format CINE owns: the authoring document `*.cinematic.json`, its migration, the cooked cinematic file `.zcine` (kind 14, magic `ZCIN`), the cooked video file `.zvid` (kind 21, magic `ZVID`), CINE's save section, the render-world records of `Cinematics.ExtractRender`, and the capture and export outputs. Binary formats follow ASSET's common rules (`Zenith2/Docs/Design/ASSET/Formats.md` → "Common rules"): little-endian, written and read field by field, every count and offset checked with overflow-checked arithmetic, reserved fields zero. Both cooked files use ASSET's 64-byte header and 40-byte section table (`Zenith2/Docs/Design/ASSET/Formats.md` → "Header layout", "Section table", "Checks"); this file defines the section kinds and their contents.

## Format index

| Format | Path pattern | Version | Writer | Reader | Section |
|---|---|---|---|---|---|
| cinematic document | `Games/<Name>/Assets/**/<name>.cinematic.json` | `schemaVersion` 1 | the editor's sequencer, hand edits | the `cine.sequence` cook step, the editor | "Cinematic document" |
| cooked cinematic | `<cooked root>/<guid>.zcine` | format version 1 | `cine.sequence` | CINE's kind loader | "Cooked cinematic file" |
| video import settings | the source's `.meta`, importer `cine.video` | meta version per ASSET | the editor's import dialog | the `cine.video` importer | "Video import settings" |
| cooked video | `<cooked root>/<guid>.zvid` | format version 1 | `cine.video` | CINE's kind loader, the video thread | "Cooked video file" |
| save section | inside a save image | section version 1 | the participant | the participant | "Save section" |
| render-world records | `Flux_RenderWorld` partition | in-memory | `Cinematics.ExtractRender` | `Flux_VideoSystem`, `VideoComposite` | "Render-world records" |
| capture directory | `user:/devtools/captures/<name>/` | manifest `schemaVersion` 1 | `Zenith_CinematicCapture` | `Zenith_ExportCinematicCapture`, people | "Capture output" |
| export directory | chosen by the user | manifest `schemaVersion` 1 | `Zenith_ExportCinematicCapture` | people, video tools | "Export output" |

## Cinematic document

A cinematic document is canonical JSON written by `Zenith_JsonWriter` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_CanonicalJson header"), stable name `Zenith.Cinematics.SequenceDocument`, schema tag `zenith2.cinematic`, suffix `.cinematic.json`, current version 1. Times are integer microseconds in members ending `Us`; there is no floating-point time anywhere in the document, so a round trip is exact and a key placed at a tick boundary of any rate stays there.

### Document root

| Member | Type | Required | Rule |
|---|---|---|---|
| `$schema` | string | yes | `"zenith2.cinematic"` |
| `schemaVersion` | integer | yes | 1 |
| `id` | GUID string | yes | the asset GUID (equals the `.meta` GUID) |
| `name` | string | yes | 1–128 bytes, display only |
| `durationUs` | integer | yes | [1, 3,600,000,000] |
| `bindings` | array of binding | yes | 0–64 entries, unique `name` |
| `tracks` | array of track | yes | 0–512 entries after flattening, unique `id` |
| `markers` | array of marker | no | unique `name`, times in [0, durationUs] |
| `subsequences` | array of subsequence | no | "Subsequences" |
| `editor` | object | no | sequencer view state (zoom, scroll, collapsed tracks, snapping); never read by the cook; preserved as an unknown-to-the-cook member |

### Bindings

```json
{ "name": "Hero", "required": true, "camera": false, "components": ["Zenith.Animation.Animator"] }
```

`name` is 1–128 bytes of `[A-Za-z0-9_.-]`; `Self` is reserved for the entity carrying a `Zenith.Cinematics.Player`. From Stage 5 a binding may carry `"prefab": "<GUID>"`: the sequence spawns that prefab's instance at Begin and destroys it at the instance's release (a spawnable binding, `Zenith2/Docs/Design/CINE/Playback.md` → "Begin"); a spawnable binding is never picked in a player's `Bindings` and is never snapshotted ("Zenith_CinematicComponents header" of `Zenith2/Docs/Design/CINE/PublicAPI.md`). `components` lists the stable names of components a bound entity must carry; the cook adds every component a track of the binding writes.

### Tracks

Every track has `id` (GUID string, unique), `kind`, `name` (1–128 bytes), `muted` (bool, default false; a muted track is not cooked) and, except for event, subtitle, overlay, video, marker and command tracks without a target, `binding` (a binding name). Per kind:

| `kind` | Kind members | Children |
|---|---|---|
| `"transform"` | `space` (`"parent"`, `"world"`, `"anchor"`), `anchor` (binding name, with space anchor) | `channels`: object of channel name (`px`, `py`, `pz`, `rx`, `ry`, `rz`, `rw`, `sx`, `sy`, `sz`) to channel; `rotationMode` `"perChannel"` or `"slerp"` |
| `"camera"` | as transform, plus `cutIn` (bool) | `channels` adds `fovY`, `near`, `far`, `focalLength`, `sensorWidth`, `focusDistance`, `focusRange`, `aperture`, `shutter`, `iso`, `exposureCompensation`, `maxCoC`, and step channels `depthOfField`, `motionBlur`, `exposureMode`, `gradingLut` (asset GUID), `gradingLutB`, `lutBlend`, `vignette`, `grain`; `cuts`: array of `{ "tUs": n }` |
| `"animation"` | `slot` (default `"cine.body"`) | `sections`: `{ startUs, endUs, easeInUs, easeOutUs, clip (GUID), clipOffsetUs, rate, forceState { machine, state }?, fireOnSkip }`; `physicsWeight`: channel (optional) |
| `"audio"` | `attach` (`"binding"`, `"position"`, `"none"`), `position` [x, y, z] | `sections`: `{ startUs, endUs, easeOutUs, event (path string), volumeDb, pitchCents, timeLocked, stopAtEnd, fireOnSkip }` |
| `"event"` | none | `keys`: `{ tUs, type (event stable name), payload { property: value }, fireOnSkip, fireOnSeek }` |
| `"dialogue"` | `speaker` (binding name) | `sections`: `{ startUs, endUs, line (line ID text), faceClip (GUID)?, subtitleKey, speakerKey, speakerColour (0–15), priority (0–255), fireOnSkip }` |
| `"subtitle"` | none | `sections`: `{ startUs, endUs, key, speakerKey, kind ("subtitle", "sound"), speakerColour, priority, fireOnSkip }` |
| `"property"` | `component` (stable name), `property` (name), `interpolation` (`"curve"`, `"step"`) | `channels`: an array of channels matching the value type's lane count; for step values `keys`: `{ tUs, value }` |
| `"command"` | `commandKind` (registered name), `payloadType` (stable name) | `keys`: `{ tUs, payload { property: value }, fireOnSkip, fireOnSeek }` |
| `"overlay"` | none | `channels`: `fadeR`, `fadeG`, `fadeB`, `fadeA`, `letterbox` |
| `"video"` | none | `sections`: `{ startUs, endUs, video (GUID), mediaOffsetUs, layer, rect [x, y, w, h]?, fit ("letterbox", "fill", "stretch"), surface (string)?, opacity, pauseWithInstance }` |
| `"marker"` | none | (uses the root `markers`; a marker track groups markers for display only) |

A channel is:

```json
{ "default": 0.0, "keys": [ { "tUs": 0, "v": 0.0, "in": 0.0, "out": 0.0, "interp": "cubic" } ] }
```

`interp` is `"constant"`, `"linear"` or `"cubic"`; `in` and `out` are slopes in units per second; `v` is a finite number. Keys are sorted by `tUs` with no two equal; a channel holds at most 65,536 keys.

Payload values are typed by the target property's reflected value kind: numbers for scalars, arrays for vectors and quaternions, `"#rrggbb"` or arrays for colours, strings for string properties, GUID strings for asset references, and the substitutions `"$binding:<Name>"`, `"$instance"`, `"$time"` for entity, `U64` and `U64` properties respectively ("Event tracks" of `Zenith2/Docs/Design/CINE/Playback.md`).

### Markers

```json
{ "name": "Shot02", "tUs": 2400000, "track": "f0b9…" }
```

### Subsequences

```json
{ "id": "a3d1…", "sequence": "77c2…", "startUs": 1000000, "trimStartUs": 0, "trimEndUs": 2000000,
  "bindings": { "ChildHero": "Hero" }, "trackPrefix": "Intro/" }
```

The cook flattens a subsequence into the parent: every track of the child is copied with its times shifted by `startUs − trimStartUs`, clipped to `[startUs, startUs + (trimEndUs − trimStartUs)]`, its binding renamed through `bindings`, and its name prefixed. Depth at most `uCINE_MAX_SUBSEQUENCE_DEPTH` (4); a cycle is a cook error (`cine.cook.subsequence_cycle`). The child is a cook dependency, so editing it re-cooks every parent.

### Example document

The TwoCharacters document of the LifecycleOpenWorld fixture of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures" (abbreviated; GUIDs elided). It is spec-derivable text, so it is committed as a hand-written golden: GAME's suite keeps its copy under its own `Tests/Fixtures/` and cooks it in its fixture setup through ZenithCook (row CN7 (d)); CINE's tests write the cooked bytes from the same builder values through `Zenith_CinematicFixtures.h` (`Zenith2/Docs/Design/CINE/Tests.md` → "Fixtures"):

```json
{
	"$schema": "zenith2.cinematic",
	"schemaVersion": 1,
	"id": "5e0c…",
	"name": "TwoCharacters",
	"durationUs": 6000000,
	"bindings": [
		{ "name": "A", "required": true, "camera": false, "components": [] },
		{ "name": "B", "required": true, "camera": false, "components": [] },
		{ "name": "Camera", "required": true, "camera": true, "components": ["Zenith.Render.Camera"] }
	],
	"tracks": [
		{ "id": "01…", "kind": "animation", "name": "A body", "binding": "A", "slot": "cine.body",
		  "sections": [ { "startUs": 0, "endUs": 6000000, "easeInUs": 0, "easeOutUs": 250000, "clip": "c1…",
		                  "clipOffsetUs": 0, "rate": 1.0, "fireOnSkip": false } ] },
		{ "id": "02…", "kind": "animation", "name": "B body", "binding": "B", "slot": "cine.body",
		  "sections": [ { "startUs": 0, "endUs": 6000000, "easeInUs": 0, "easeOutUs": 250000, "clip": "c2…",
		                  "clipOffsetUs": 0, "rate": 1.0, "fireOnSkip": false } ] },
		{ "id": "03…", "kind": "camera", "name": "Shot", "binding": "Camera", "space": "world", "cutIn": false,
		  "channels": { "px": { "default": 0.0, "keys": [ { "tUs": 0, "v": 0.0, "in": 0.0, "out": 0.0, "interp": "linear" },
		                                                  { "tUs": 6000000, "v": 3.0, "in": 0.0, "out": 0.0, "interp": "linear" } ] } },
		  "cuts": [] },
		{ "id": "04…", "kind": "property", "name": "A tint", "binding": "A", "component": "Zenith.Test.RestoreProbe",
		  "property": "Tint", "interpolation": "curve",
		  "channels": [ { "default": 0.0, "keys": [ { "tUs": 0, "v": 0.0, "in": 0.0, "out": 0.0, "interp": "linear" },
		                                          { "tUs": 6000000, "v": 1.0, "in": 0.0, "out": 0.0, "interp": "linear" } ] } ] },
		{ "id": "05…", "kind": "event", "name": "Beats",
		  "keys": [ { "tUs": 4000000, "type": "Zenith.Test.CinematicBeat", "payload": { "Who": "$binding:A" },
		              "fireOnSkip": true, "fireOnSeek": false } ] }
	]
}
```

## Document migration

The document type is registered tool-side by `Zenith_RegisterCinematicDocumentTypes` (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Tool-side headers") with its canonical-member order (the order of the tables above) and the schema tag. Each version bump adds one migration step (n → n+1) in the same diff, previewed, backed up and journaled by the document framework with unknown members preserved (Z2-D-048; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_DocumentMigrator header"). Version 1 has no migration. A rename of a binding or a track is an editor operation that rewrites the document and every scene `Zenith.Cinematics.Player` whose bindings name it, never a migration step. The cook reads only the current version; an older document is migrated in memory by the cook's document storage before the step reads it (`Zenith2/Docs/Design/ASSET/Cook.md` → "Source documents in cook steps").

## Cooked cinematic file

ASSET's header with magic `ZCIN` (`5A 43 49 4E`), format version 1, kind 14. Section kinds (all stored with COMPRESS's descriptor; the cook compresses sections above 4 KiB with the build's default codec, Z2-D-004; every section has index 0):

| Section kind | Name | Record | Bytes per record | Required |
|---|---|---|---|---|
| 1 | sequence header | "Sequence header section" | 64 | yes |
| 2 | bindings | "Binding table section" | 32 | yes (may be empty) |
| 3 | drivers | "Binding table section" | 16 | yes |
| 4 | tracks | "Track table section" | 64 | yes |
| 5 | channels | "Channel table section" | 16 | yes |
| 6 | keys | "Key table section" | 24 | yes |
| 7 | sections | "Section table section" | 48 | yes |
| 8 | event keys | "Event key section" | 32 | yes |
| 9 | payloads | "Payload blob section" | variable | yes |
| 10 | strings | "String table section" | variable | yes |
| 11 | dependencies | "Dependency section" | 24 | yes |
| 12 | markers | "Marker section" | 24 | yes |

Every index into another section is a `uint32` record index checked against that section's record count; every string reference is a `uint32` byte offset into the string table pointing at a NUL-terminated UTF-8 string inside it.

### Sequence header section

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 8 | duration µs | [1, `ulCINE_MAX_DURATION_US`] |
| 8 | 2 | binding count | ≤ 64; equals the bindings section's record count |
| 10 | 2 | track count | ≤ 512 |
| 12 | 2 | marker count | ≤ 65,535 |
| 14 | 2 | flags | bit 0: has video tracks; bit 1: has command tracks; other bits 0 |
| 16 | 4 | driver count | |
| 20 | 4 | channel count | ≤ 512 × 64 |
| 24 | 4 | key count | ≤ 2²⁴ |
| 28 | 4 | section count | ≤ `uCINE_MAX_SECTIONS` (65,536) |
| 32 | 4 | event key count | ≤ `uCINE_MAX_EVENT_KEYS` |
| 36 | 4 | payload bytes | ≤ 16 MiB |
| 40 | 4 | string bytes | ≤ 4 MiB |
| 44 | 4 | dependency count | ≤ 65,536 |
| 48 | 1 | maximum overlap | maximum active sections on any track at any time, ≤ 8 |
| 49 | 1 | subsequence depth | flattened depth, ≤ 4 |
| 50 | 2 | reserved | 0 |
| 52 | 4 | name string | the document `name` |
| 56 | 8 | reserved | 0 |

### Binding table section

Binding record (32 bytes, ascending name hash — the order `FindBinding` searches):

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | name hash (`Zenith_Hash64` of the name) |
| 8 | 4 | name string |
| 12 | 4 | flags (`uCINE_BINDING_FLAG_*`) |
| 16 | 4 | first driver |
| 20 | 2 | driver count |
| 22 | 2 | authored index (the binding's position in the document, for display) |
| 24 | 4 | spawn prefab (index of the binding's prefab record in the dependency section; `0xFFFFFFFF` unless the binding carries `uCINE_BINDING_FLAG_SPAWNABLE`, Stage 5) |
| 28 | 4 | reserved |

Driver record (16 bytes): component stable-name string (4), property ID (4; 0 for a component-presence requirement only), channel mask (2; transform position 0x1, rotation 0x2, scale 0x4, other properties 0x1), flags (2; bit 0 written, bit 1 required component), reserved (4). The written drivers of a binding are the snapshot's written set (`Zenith2/Docs/Design/CINE/Playback.md` → "Pre-cinematic snapshot"); the required-component drivers are what "Binding resolution" checks. Property IDs are resolved from names at cook against the game's type registry and re-checked at attach-resolution (a mismatch disables the track, `CINEMATIC_ERROR_PAYLOAD`).

### Track table section

Track record (64 bytes, in authored order after flattening):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | kind (`Zenith_CinematicTrackKind`) |
| 1 | 1 | flags (bit 0 cutIn, bit 1 slerp rotation, bit 2 step interpolation, bit 3 timeLocked default) |
| 2 | 2 | binding index (0xFFFF none) |
| 4 | 4 | name string |
| 8 | 4 | first channel |
| 12 | 2 | channel count (≤ 64) |
| 14 | 1 | space (`CINE_TRANSFORM_SPACE_*`) |
| 15 | 1 | attach mode (audio: 0 binding, 1 position, 2 none) |
| 16 | 4 | first section |
| 20 | 4 | section count (≤ 4,096) |
| 24 | 4 | first event key |
| 28 | 4 | event key count |
| 32 | 2 | anchor binding index (0xFFFF none) |
| 34 | 2 | reserved |
| 36 | 4 | target string 1 (property track: component; command track: command kind name; animation track: slot name) |
| 40 | 4 | target string 2 (property track: property name; command track: payload type) |
| 44 | 8 | slot hash (animation and dialogue face slot), or 0 |
| 52 | 12 | position (audio attach position: three f32) |

### Channel table section

Channel record (16 bytes): first key (4), key count (4), semantic (2; the channel names of "Tracks" numbered from 1 in table order per kind), group (1; rotation group index or 0), flags (1; bit 0 step), default value (4, f32).

### Key table section

Key record (24 bytes): time µs (8), value (4, f32), in slope (4, f32, units per second), out slope (4, f32), interpolation (1; 0 constant, 1 linear, 2 cubic), reserved (3). A channel's keys are contiguous and strictly ascending in time; step channels carry only constant keys. Non-finite values are refused at cook and at load.

### Section table section

Section record (48 bytes): start µs (8), end µs (8; > start), ease-in µs (4), ease-out µs (4; ease-in + ease-out ≤ end − start), payload offset (4) and payload bytes (4) into the payload blob, flags (4; bit 0 fireOnSkip, bit 1 stopAtEnd, bit 2 timeLocked, bit 3 pauseWithInstance), dependency index (4; the clip, video or face clip, 0xFFFFFFFF none), reserved (8). A track's sections are sorted by start time.

Per-kind section payloads in the blob (little-endian, 16-byte aligned within the blob):

| Track kind | Payload |
|---|---|
| animation | clip offset µs (8), rate (4, f32), force-state machine hash (8), state hash (8), physics-weight channel index (4; 0xFFFFFFFF none), reserved (4) — 36 bytes padded to 48 |
| audio | event ID (8), volume dB (4), pitch cents (4), reserved (16) — 32 |
| dialogue | line ID (8), subtitle key string (4), speaker key string (4), speaker colour (1), priority (1), reserved (2), face clip dependency (4), lip-sync window count n (2; ≤ `uCINE_MAX_LIPSYNC_WINDOWS`, 16), reserved (6), then n window records of 24 bytes each: marker name hash (8, `Zenith_Hash64` of `char.lipsync.<tag>`), window start T µs (8), window end e µs (8), in the face clip's marker order — 32 + 24 × n, padded to 16 (`Zenith2/Docs/Design/CINE/Playback.md` → "Dialogue tracks"; load check L9 parses n and keeps every T < e) |
| subtitle | key string (4), speaker key string (4), kind (1; 0 subtitle, 1 sound), speaker colour (1), priority (1), reserved (5) — 16 |
| video | media offset µs (8), layer (4), fit (1), reserved (3), rect (16, four f32), opacity (4, f32), surface name hash (8), surface name string (4), reserved (8) — 56 padded to 64 |

### Event key section

Event key record (32 bytes): time µs (8), type string (4; event stable name for event tracks, ignored for command tracks whose type is the track's target), payload offset (4), payload bytes (2; ≤ 256 when templated), flags (2; bit 0 fireOnSkip, bit 1 fireOnSeek), patch offset (4), patch count (2), reserved (6). A track's keys are sorted by time; ties keep authored order.

### Payload blob section

Event and command payloads are stored as property lists, not as C++ layouts, because the runtime builds them by reflection ("Reflected payloads" of `Zenith2/Docs/Design/CINE/Playback.md`): a count (2), then per property a name string (4), a value kind (1; `Zenith_ValueKind`), a value length (1) and the value bytes in the property's reflected value encoding, padded to 4. Patches: per patch a property name string (4), a patch kind (1; 1 binding entity, 2 instance ID, 3 key time), a binding index (1), reserved (2).

### String table section

UTF-8, each string NUL-terminated, offset 0 is the empty string; strings are interned (one copy per distinct text). Every string is validated as UTF-8 at load.

### Dependency section

Dependency record (24 bytes, ascending GUID): persistent ID (16), asset kind (2; 6 prefab for a spawnable binding's prefab, 8 clip, 10 sound, 21 video, 14 cinematic for flattened sources recorded for provenance only), flags (2; bit 0 hard — needed for playback; bit 1 provenance only), reserved (4). `Zenith_Cinematic::GetDependencies` returns the hard ones. The catalogue entry lists the same hard dependencies (ASSET's dependency table), so a package build includes them.

### Marker section

Marker record (24 bytes, ascending time then name hash): time µs (8), name hash (8), name string (4), track index (2; 0xFFFF root), reserved (2).

### Load validation

CINE's kind loader runs after ASSET's checks C1–C12 (`Zenith2/Docs/Design/ASSET/Formats.md` → "Checks") and returns `CINEMATIC_ERROR_CORRUPT` (passed to `Zenith_AssetLoadContext::Fail` with the check number as detail) at the first failure:

| # | Check |
|---|---|
| L1 | every required section kind present exactly once, index 0 |
| L2 | each section's decoded size equals record count × record size (fixed-size sections) |
| L3 | the header's counts equal the section record counts; every limit of "Sequence header section" holds |
| L4 | every cross-section index in range; every string offset inside the table and NUL-terminated; every string valid UTF-8 |
| L5 | binding name hashes strictly ascending and equal to `Zenith_Hash64` of their strings |
| L6 | every track's kind known and its space ≤ 2 (`Zenith_CinematicTransformSpace`); channel, section and key ranges inside their sections and disjoint between tracks |
| L7 | keys strictly ascending per channel; values and slopes finite; interpolation ≤ 2 |
| L8 | sections sorted, `start < end ≤ duration`, eases fit; the maximum overlap holds |
| L9 | event keys sorted per track and ≤ duration; payload ranges inside the blob; property lists parse with known value kinds; payload bytes ≤ 256; at most `uCINE_MAX_FIRE_ON_SKIP_KEYS` (8) event keys and sections flagged `fireOnSkip` in the whole sequence; every dialogue section payload holds `32 + 24 × n` bytes with n ≤ 16 window records whose T < e |
| L10 | dependency GUIDs strictly ascending and non-null; kinds in {6, 8, 10, 14, 21} (6 only as a spawnable binding's prefab) |
| L11 | markers sorted, times ≤ duration |
| L12 | every reserved field zero; a binding's spawn prefab is `0xFFFFFFFF` unless the binding carries `uCINE_BINDING_FLAG_SPAWNABLE`, and then indexes a hard dependency record of kind 6 (prefab) |

Every check has a corruption test and every limit an at-limit and limit + 1 test (`Zenith2/Docs/Design/CINE/Tests.md` → "Format and cook tests"); the loader is a fuzz target (`Fuzz_CinematicLoader`).

### Runtime layout

The loader decodes every section into one block of the asset slot (no per-record allocation), keeps typed spans over it, and builds one index: per binding slot, the list of tracks bound to it (for `ApplyTransforms`' per-entity visit). It also interns, through the process string table the module received at Start (`Zenith_StringTable::Intern`, ANY_THREAD, `Zenith2/Docs/Design/CORE/Text.md` → "String interning"), every marker name, subtitle key, speaker key and string-valued payload property, and stores the `Zenith_StringID`s in a table indexed by string offset, so playback never interns; it counts the (track, written late property) pairs of camera and property tracks for `GetLatePropertyCount`. Resident size is the decoded section sizes plus 8 bytes per track; this is what the memory consumer reports.

## Video import settings

The `cine.video` importer (ASSET's importer interface, `Zenith2/Docs/Design/ASSET/Import.md` → "Zenith_ImporterDesc") accepts `.mp4` (ISO/IEC 14496-12 with an `avc1` or `hvc1` sample entry) and reads these `.meta` settings:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `codec` | string | `"h264"` | target codec: `"h264"` (always decodable on both platforms), `"hevc"` (admitted only when the D-SEL-VIDEO ruling records HEVC support on both platforms) |
| `transcode` | bool | false | false: remux the source bitstream; true: re-encode through the encoder D-SEL-VIDEO admits for the host |
| `maxWidth`, `maxHeight` | integer | 1920, 1080 | the cook downscales above (transcode only) |
| `audio` | string | `"extract"` | `"extract"` (cook the audio track into a sound sub-asset), `"drop"` |
| `audioBus` | string | `"Master/Cinematic"` | the AUDIO bus of the extracted sound |

A 10-bit profile (HEVC Main 10) is refused (`cine.cook.video_10bit_android`) when the game's `game.json` targets Android, because the Android backend outputs only 8-bit `AIMAGE_FORMAT_YUV_420_888` (Video.md "Platform decoder backend"). A remux keeps the elementary stream bit for bit, converting MP4 sample framing into length-prefixed NAL units; the cook refuses a source above `uCINE_MAX_VIDEO_WIDTH` × `uCINE_MAX_VIDEO_HEIGHT`, above `ulCINE_MAX_VIDEO_DURATION_US`, with B-frame reorder depth above 4, or with a profile outside {H.264 Baseline, Main, High; HEVC Main, Main 10}.

## Cooked video file

ASSET's header with magic `ZVID` (`5A 56 49 44`), format version 1, kind 21. Sections:

| Section kind | Name | Content | Compression |
|---|---|---|---|
| 1 | video header | 64 bytes, below | yes |
| 2 | decoder configuration | the codec's parameter sets | yes |
| 3 | frame index | 24 bytes per frame, decode order | yes |
| 4 | presentation order | 4 bytes per frame | yes |
| 5 | keyframe index | 4 bytes per keyframe | yes |
| 6 | bitstream | the length-prefixed NAL units, decode order | never (codec none, so `ReadRange` reads it directly) |

Video header (64 bytes): codec (1), colour primaries (1), transfer (1), matrix (1), full range (1), chroma location (1), reorder depth (1, ≤ 4), bit depth (1; 8 or 10), display width (4), display height (4) (the SPS cropping applied: the decoder's aligned output is cropped to it, Video.md "Platform decoder backend"), frame rate numerator (4), denominator (4, ≥ 1), frame count (4, ≥ 1), keyframe count (4, ≥ 1), duration µs (8), audio sound GUID (16, zero when silent), reserved (8).

Decoder configuration: H.264 — `u8 version (1); u8 profile; u8 compatibility; u8 level; u8 NAL length size minus one (3); u8 SPS count; then per SPS u16 length + bytes; u8 PPS count; then per PPS u16 length + bytes` (the `avcC` record of ISO/IEC 14496-15, stored verbatim); HEVC — the `hvcC` record verbatim.

Frame record (24 bytes, decode order): presentation time µs (8), bitstream offset (8, relative to the section start), size (4), flags (2; bit 0 keyframe (IDR), bit 1 discardable), reserved (2). Presentation order: per presentation index the decode index (u32). Keyframe index: decode indices of keyframes, ascending.

### Video load validation

| # | Check |
|---|---|
| V1 | the six sections present once, index 0; the bitstream section uncompressed |
| V2 | codec known; dimensions in [16, 3840] × [16, 2160], even; frame rate num/den in [1, 240] fps; duration ≤ two hours |
| V3 | frame index: offsets ascending and contiguous, every frame inside the bitstream section, sizes ≥ 1; the first frame a keyframe |
| V4 | presentation order is a permutation; presentation times strictly ascending in presentation order and ≤ duration |
| V5 | keyframe index strictly ascending, every entry flagged keyframe, count equals the header's |
| V6 | decoder configuration parses (counts, lengths inside the section, NAL length size 4) |
| V7 | reserved fields zero |

The loader keeps sections 1–5 resident; section 6 is read by the video thread through `Zenith_AssetReader::ReadRange` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetReader header") in decode-order runs of at most 1 MiB. NAL-unit parsing beyond the length prefix is the decoder's; CINE validates each NAL length against the frame size before handing a frame to it (a mismatch is `CINEMATIC_ERROR_VIDEO_DECODE` for that player).

## Save section

Written by the participant through `Zenith_SaveSectionWriter`, little-endian, field by field (`Zenith2/Docs/Design/SAVE/Formats.md` → "Participant section framing"). Every entity reference is written with `WriteEntityReference` (16 bytes through the save resolver, which also covers runtime-spawned entities, `Zenith2/Docs/Design/SAVE/Formats.md` → "Saved entity references") and read back with `ReadEntityReference`; every `Zenith_StringID` is written as its text (u16 length then UTF-8 bytes) and re-interned at restore; every float is its 32 bits. The section is written in phase 11 of tick t and restored before the boundary t → t + 1 runs (`Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore").

| Part | Content |
|---|---|
| world prologue | next instance serial (8); `cine.maxInstancesPerWorld` M at the save (2; used only to refuse a section whose M exceeds the running configuration's M′, `cine.save` logged; the restore rebuilds the pool and the outbox from M′, `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore"); counts: instances (2), tickets (2), player starts (2), preloads (2), needs (2), outbox entries (2), audio entries (2), bound-set entries (2) |
| tickets | per ticket entry: ticket (16: four u32), instance ID (8), error code (2), detail (2), playback tick (8) |
| player starts | per entry: player entity reference (16), ticket (16), record tick (8) |
| preloads | per preload: sequence GUID (16), pin count (2), state (1; 0 requested, 1 bound, 2 failed), reserved (1), completion ID (8, 0 none) |
| needs | per need: GUID (16), asset kind (2), state (1; 0 collected, 1 issued, 2 parked success, 3 parked failure, 4 abandoned), reserved (1), completion ID (8) |
| outbox | in storage order, per entry the next `Cinematics.Evaluate` would not remove (its acceptance bit for tick t clear: queued after phase 7 of tick t, or in an emission group the phase-8 merge rejected): event type stable name (string), queue tick (8), queue point (1), source (1), instance ID (8), payload as the event type's reflected field set (`Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record"; entity-typed properties through the save resolver) |
| audio list | per entry: instance ID (8), native audio instance ID (8), track (2), flags (2; bits 2 and 3 as saved, the restore then marks every entry per "Audio tracks"), section (4), post serial (4) |
| bound set | per entry in ascending GUID: sequence GUID (16), content hash (8), holder count (4) |
| instances | per instance record in ascending ID, `RELEASED` records included: the record below |

Instance record, in this field order:

| Group | Fields |
|---|---|
| identity | ID (8), sequence GUID (16), sequence content hash (8), ticket (16), tag (string), player entity reference (16, null when none) |
| state | state (1), end reason (1), paused-from state (1), outbox reserve (1), flags (4), rate Q16 (4), prepare ticks (4), audio post serial (4), voice-locale tag (string) |
| time | scaled time (8), previous scaled time (8), next item key (8), seek target (8), video pause offset (8) |
| blend | blend elapsed (8), blend-in scaled (8), blend-out scaled (8), weight (4), hold weight (4), paused weight (4) |
| ticks | begin tick (8), end tick (8) |
| side work | side-work bits (4), audio fade ms (4), skip-from time (8) |
| slots | binding count (2), then per slot: name hash (8), entity reference (16, null when unbound or dropped), parent entity reference (16, null for a root), slot flags (2), taken-over tick (8), first entry (2), entry count (2), first base (2), base count (2) |
| prepares | count (2), then per record: track (2), section (4), recorded tick (8) |
| captions | count (1), then per entry: source ID (8; bit 63 the cancelled-by-presentation mark), expiry tick (8), with tick t's acceptances already applied (an entry whose cue was accepted at t carries `t + duration ticks`, not `UINT64_MAX`) |
| snapshot | entry count (4), then per entry: entity reference of its slot (16), type stable name (string), property ID (4), flags (4), value in the property's reflected save encoding (ENTITY through the save resolver, STRING_ID as text, ASSET_REF as GUID, CONTAINER as count then elements, every other kind its value bytes) |
| take-over bases | count (2), then per base: snapshot entry index (4), value as for the snapshot |
| late cache | count (2), then per entry: track (2), property slot (2), valid (1), value bytes (1), value (16) |
| end state | count (2), then per value: snapshot entry index (4), value as for the snapshot |
| prefetch bits | bit count (4), then `ceil(count / 8)` bytes |

A restore whose sequence content hash differs from the resident asset's (the content was re-cooked between save and load) restores the instance as `Zenith2/Docs/Design/CINE/Playback.md` → "Save and restore" specifies in its table of re-cooked content: a `BINDING` or `PREPARING` instance is cancelled; a begun one keeps its saved state with the content-changed flag, ends `FAILED` through a recorded `Cinematics.End` at the first tick (END dispatched once), and restores its snapshot at the following boundary; `cine.save.content_changed` is logged once per instance. Unknown future section versions are refused by SAVE's framing; version 1 has no migration.

## Render-world records

`Cinematics.ExtractRender` writes into its partition (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Flux_RenderWorld header additions"):

| Record | Fields | Count |
|---|---|---|
| video draw | the player key — world tag (2), track (2), section (4), instance ID (8) —, `Flux_VideoDrawDesc` (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Flux_Video header"; `m_uPlayer` 0, filled by the player table from the key), the video asset handle and the player request flags (start, seek, stop) | ≤ 4 per world |
| overlay | fade colour (3 × f32), fade alpha (f32), letterbox fraction (f32), layer (u32) | ≤ 1 per instance, combined by the composite (maximum alpha, maximum letterbox) |

<!-- assumption: pending RPROD, REND -->
`Flux_RenderWorldWriter` gains `AddVideoDraw(const Flux_VideoDrawRecord&)` and `AddScreenOverlay(const Flux_ScreenOverlayRecord&)` (typed members copied into the partition's arena, the pattern of row AN5), and `Flux_RenderWorld` exposes the merged spans to the VideoComposite pass. Rows CN8 and X292.
<!-- /assumption -->

## Capture output

### Capture directory layout

Every capture file is written through the development file writer (`Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevFileWriter header"), which places it under `user:/devtools/` with a relative name of at most 4 segments and 128 bytes and at most `uDEV_FILE_MAX_BYTES` (64 MiB) per file:

```
user:/devtools/captures/<name>/
	frames/000000.tga … frames/NNNNNN.tga   (uncompressed 32-bit BGRA, origin top-left, one per tick; ≤ 3840 × 2160 × 4 + 18 = 33,177,618 bytes)
	audio/000000.pcm … audio/MMMMMM.pcm     (raw interleaved f32 little-endian, 48 kHz, 2 channels; each chunk exactly 1 s = 384,000 bytes except the last)
	subtitles.json                          (the caption cues of the capture)
	manifest.json                           (written last: its presence marks a capture whose every file was submitted)
```

`<name>` is at most 64 bytes of `[A-Za-z0-9_-]`, so `captures/<name>/frames/000000.tga` is 4 segments and at most 92 bytes. The longest capture is 999,999 frames (4.6 hours at 60 Hz); `End` is forced at frame 999,999 (logged). With audio the limit is lower: `End` is forced at the tick whose audio would exceed `ulCINE_CAPTURE_MAX_AUDIO_FRAMES` (536,870,907 sample frames, about 3.1 hours at 48 kHz), because `audio.wav`'s `RIFF` and `data` sizes are `uint32` ("Export output").

TGA header (18 bytes): ID length 0, colour map type 0, image type 2, colour map spec 0 × 5, origin x 0, y 0, width, height, 32 bits per pixel, descriptor 0x28 (8 alpha bits, top-left origin). Pixels are the display-referred output in the swapchain's SDR encoding (sRGB) — an HDR output is captured after tonemapping to SDR by `Flux_EncodeOutput` with the SDR mode, so captures are comparable across displays.

### Capture manifest

```json
{
	"$schema": "zenith2.cinecapture",
	"schemaVersion": 1,
	"name": "Intro",
	"width": 1920, "height": 1080, "frameRate": 60, "frames": 3600,
	"firstTick": 120, "build": "<build fingerprint>", "game": "CinematicSlice",
	"audio": { "directory": "audio", "chunks": 60, "frames": 2880000, "sampleRate": 48000, "channels": 2, "format": "f32" },
	"subtitles": "subtitles.json",
	"includesUI": false
}
```

`audio` is absent when the capture was taken with `m_bCaptureAudio` false. `audio.frames` is the exact sample-frame count (the sum of `FramesForNextTick` over the captured ticks, 800 per tick at 60 Hz).

### Subtitle file

```json
{ "$schema": "zenith2.cinesubtitles", "schemaVersion": 1,
  "cues": [ { "startFrame": 30, "durationFrames": 120, "key": "intro.line01", "speakerKey": "speaker.hero",
              "kind": "subtitle", "sourceID": 4503600164552704 } ] }
```

The file holds CINE's own captions alone (the cues and cancels `Cinematics.EmitEvents` emitted; captions a script shows with `UI.ShowCaption` are UI's and do not appear). A cue's `durationFrames` is shortened when a `Zenith.Runtime.CaptionCancelEvent` with its source ID was delivered first (the frame of the cancel minus `startFrame`). The capture writes keys only: ZenithCinematics may not include ZenithUI ([Z2-N-ARCH-074]), so the text is resolved at export, tool side ("Export output").

## Export output

`Zenith_ExportCinematicCapture` writes, into an empty output directory: `frames/NNNNNN.png` (PNG, 8-bit RGBA, zlib through COMPRESS's zlib backend, filter type 0 per row — deterministic bytes for identical frames) when `m_bWritePng`, else the TGA files copied; `audio.wav` when the manifest has `audio`: the chunks concatenated in index order behind a 44-byte WAV header (`RIFF`, size = 36 + data bytes, `WAVE`, `fmt ` chunk of 16 bytes: format 3 IEEE float, 2 channels, 48,000 Hz, byte rate 384,000, block align 8, 32 bits per sample; `data`, size = frames × 8), every field little-endian; audio above `ulCINE_CAPTURE_MAX_AUDIO_FRAMES` (the `RIFF` size would exceed 2³² − 1) is refused with `CINEMATIC_ERROR_CAPTURE_IO`, detail 0, logged `cine.export: audio exceeds the 4 GiB WAV limit`; `subtitles.vtt` (WebVTT: `WEBVTT` header, one cue per caption, `HH:MM:SS.mmm` times from frames at the manifest's rate, speaker as a `<v Speaker>` voice span) when `m_bWriteWebVtt`, each cue's text resolved by ZenithAuthoring from the game's string table documents in `m_strLocalizationDirectory`: key `namespace.name` is the `text` member of entry `name` of `<directory>/<namespace>.strings.json` (`Zenith2/Docs/Design/UI/Documents.md` → "String table document schema"; `Zenith2/Docs/Design/UI/Localization.md` → "String tables and keys"), written verbatim (CINE's captions take no message arguments); a missing key, or an empty `m_strLocalizationDirectory`, writes the key itself as the text and lists the key in `export.json`'s `unresolvedKeys`; `export.json` (schema `zenith2.cineexport`, version 1: the capture manifest plus the export options and the list of written files), written last; and `video.mp4` only when `m_bEncodeVideo` and the D-SEL-VIDEO ruling admits a host encoder ("D-SEL-VIDEO candidates and criteria" of `Zenith2/Docs/Design/CINE/Video.md`). A missing frame or chunk (a write that failed on completion) is `CINEMATIC_ERROR_CORRUPT` naming the first missing index.
