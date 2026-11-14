# EDIT design — editor file formats

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies every file the editor itself defines: per-user session, layout and preferences files, the autosave file and the recovery manifest, the session lock line, the thumbnail file, the clipboard document, the live-link endpoint file and the editor trash layout. The automation script and report formats are in `Zenith2/Docs/Design/EDIT/Automation.md` → "Automation script format" and "Automation report". None of these is an authoring document: none is tracked by git (each lives under a `.editor/` directory, under `${ZENITH_OUT}`, or in the per-user directory), none is cooked, and none is read by a Player.

## Scope of this file

| File | Heading |
|---|---|
| common rules for every editor file | "Common rules" |
| `Games/<Name>/.editor/user/session.json` | "Session file" |
| `Games/<Name>/.editor/user/layout.json` | "Layout file" |
| `<user dir>/preferences.json` | "Preferences file" |
| `<root state>/autosave/<id>.zautosave` (`<root state>`: `Games/<Name>/.editor/`, or `Games/<Name>/.editor/roots/<Library>/` for an engine content root) | "Autosave file" |
| `<root state>/recovery.json` | "Recovery manifest" |
| `Games/<Name>/.editor/session.lock` | "Session lock line" |
| `${ZENITH_OUT}/EditorCache/<Name>/Thumbnails/<guid>-<key>.zthumb` | "Thumbnail file" |
| the clipboard text | "Clipboard document" |
| `Games/<Name>/.editor/user/endpoint.json` | "Endpoint file" |
| `Games/<Name>/.editor/trash/` | "Editor trash layout" |
| PACK's binary format registry rows | "Format registry rows" |

## Common rules

<!-- elaboration -->
- JSON files: UTF-8 without a BOM, LF line endings, written with CORE's `Zenith_JsonWriter` in its pretty style (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"), keys in the order the tables below give, read with `Zenith_JsonDocument::Parse` under the default `Zenith_JsonLimits`; every file carries `"$schema"` (a `zenith2.editor-…` tag) and `"schemaVersion"` (an integer from 1).
- Versioning: a file of an older version is upgraded on read by the editor's chain for that file (one function per version step in `Internal/Formats/Zenith_EditorFileMigrations.cpp`), and rewritten at its next save; a file of a newer version is not read: the editor uses defaults for it, logs `editor: <file> was written by a newer editor (version n); using defaults`, and does not overwrite it until the user changes the corresponding state, so a newer editor's per-user state survives a session of an older one.
- Unknown members of a known version are kept and re-emitted in position (the editor reads into a `Zenith_JsonDocument` and edits it in place), as authoring documents keep theirs.
- Binary files (the autosave file and the thumbnail file): little-endian, every field fixed width at the offset its table gives, starting with a four-byte magic, a `uint16` version from 1 and a `uint16` header size; every count and offset is checked against the file size before use; the payload is compressed with a codec named by its COMPRESS codec ID (`Zenith2/Docs/Design/COMPRESS.md` → "Codec ID registry"; `COMPRESSION_CODEC_ID_ZSTD` level 1 by default, `COMPRESSION_CODEC_ID_NONE` for incompressible data) and protected by a SHA-256 of its uncompressed bytes (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256"); a file failing any check is treated as absent and reported, never asserted on. The readers and writers are in `Internal/Formats/Zenith_EditorAutosaveFile.cpp` and `Zenith_EditorThumbnailFile.cpp`, and none of their layouts depends on a C++ struct layout (Z2-D-038).
- Writes: a temporary file `<name>.tmp`, flushed, then renamed over the target with `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`, so a reader never sees a partial file.

## Session file

`Games/<Name>/.editor/user/session.json`, schema `zenith2.editor-session` version 1, written at shutdown step 5 and every 60 s while the session runs (so a crash keeps a recent copy):

| Key | Type | Meaning |
|---|---|---|
| `$schema`, `schemaVersion` | string, integer | `zenith2.editor-session`, 1 |
| `activeScenes` | array of strings | the active scene set's storage paths, the active scene first |
| `openDocuments` | array of objects `{ "path", "root", "panel" }` | documents open in panels, with the panel type that showed them |
| `selection` | array of objects | the main selection's `Zenith_EditorObjectRef`s: `{ "kind", "document", "object", "subID", "sub" }`, persistent IDs in their canonical text form |
| `hidden`, `locked` | arrays of persistent IDs | hierarchy flags per entity |
| `expanded` | array of persistent IDs | expanded hierarchy rows |
| `sceneViews` | array of objects | per scene view: `{ "viewportOrdinal", "camera": { "position": [x, y, z], "yawDegrees", "pitchDegrees", "projection": "perspective" or "orthographic", "fovDegrees", "orthoHeight", "flySpeed" }, "viewMenu": { "debugView", "showGrid", "showIcons" } }` |
| `bookmarks` | object keyed by scene path | arrays of 9 camera objects or null |
| `gizmo` | object | `{ "mode", "space", "pivot", "snap": { "translate": [on, increment], "rotate": [on, increment], "scale": [on, increment], "surface", "alignNormal", "mode" } }` |
| `play` | object | `{ "saveBeforePlay", "maxWorlds" }` overrides of the preferences for this game |

## Layout file

`Games/<Name>/.editor/user/layout.json`, schema `zenith2.editor-layout` version 1:

| Key | Type | Meaning |
|---|---|---|
| `$schema`, `schemaVersion` | | `zenith2.editor-layout`, 1 |
| `imguiIni` | string | ImGui's docking settings text (`ImGui::SaveIniSettingsToMemory`), restored with `LoadIniSettingsFromMemory` before the first frame |
| `panels` | array of objects `{ "type", "instance", "open", "state" }` | the open panels in docking order and each panel's `SaveState` object |
| `displayScale` | number | the display scale the layout was saved at (sizes are rescaled when it differs) |

`--layout default` ignores the file and uses the default layout of `Zenith2/Docs/Design/EDIT/Viewports.md` → "Shell layout and docking".

## Preferences file

`<user dir>/preferences.json` (default user directory `%LOCALAPPDATA%\Zenith2\Editor`, overridable with `--user-dir`), schema `zenith2.editor-preferences` version 1: one object `values` mapping each preference key (`editor.camera.scheme`, `editor.autosave.intervalSeconds`, …; the full list is `Zenith2/Docs/Design/EDIT.md` → "Editor configuration keys") to its value, and one object `shortcuts` mapping a command ID to an array of chord strings (`"Ctrl+Shift+S"`, modifiers in the order `Ctrl`, `Shift`, `Alt`, then the key's name). A value outside its key's declared range or of the wrong kind is ignored with a log line and the default used. Preferences are per user and per machine, never per game, except the overrides the session file carries.

## Autosave file

`<root state>/autosave/<document ID>.zautosave` (`<root state>` as `Zenith2/Docs/Design/EDIT.md` → "Untracked editor files" states; the document ID in the 32-hex-digit form without dashes), one per dirty document:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | magic | `ZAUT` |
| 4 | 2 | version | 1 |
| 6 | 2 | header bytes | 128 |
| 8 | 4 | flags | bit 0: the document is a multi-file document (a terrain); other bits 0 |
| 12 | 1 | codec | `Zenith_CompressionCodecID` of the payload |
| 13 | 3 | reserved | 0 |
| 16 | 16 | document ID | the persistent ID in its binary form |
| 32 | 8 | document type ID | `Zenith_TypeID` of the registry type |
| 40 | 8 | state ID | the document's state ID when autosaved |
| 48 | 8 | autosave time | UTC, 100-ns intervals since 1601-01-01 (`GetSystemTimePreciseAsFileTime`) |
| 56 | 32 | base SHA-256 | the digest of the document's file the autosaved state is based on (the disk digest at the time); all zero for a document never saved |
| 88 | 8 | payload offset | 128 |
| 96 | 8 | payload stored bytes | |
| 104 | 8 | payload raw bytes | |
| 112 | 16 | reserved | 0 |
| 128 | … | payload | compressed |

The raw payload is a sequence of entries, one per file of the document (one for most documents; the terrain document and each of its dirty tile documents for a terrain): `uint32 pathBytes`, the storage path's UTF-8 bytes, `uint64 contentBytes`, the file's canonical bytes, `uint8[32]` the base SHA-256 of that file. The file ends with a trailer of 32 bytes, the SHA-256 of the raw payload. A reader checks the magic, the version (above 1: newer, skipped and reported), the header size, the offsets and sizes against the file size, decompresses with the codec named, checks the trailer digest and every entry length; any failure is "damaged".

## Recovery manifest

`<root state>/recovery.json`, schema `zenith2.editor-recovery` version 1:

| Key | Type | Meaning |
|---|---|---|
| `$schema`, `schemaVersion` | | `zenith2.editor-recovery`, 1 |
| `session` | object | `{ "pid", "startedUtc", "editorBuildId" }` of the session that wrote it |
| `entries` | array of objects | per autosaved document: `{ "document", "type", "path", "file", "stateID", "autosavedUtc", "baseSha256" }` (`file` the `.zautosave` name, `baseSha256` 64 lowercase hex digits), in byte order of `path` |

The manifest is an index: recovery trusts the `.zautosave` files, not the manifest, so an autosave written after the manifest's last rewrite is still found by listing the directory.

## Session lock line

`Games/<Name>/.editor/session.lock` holds one JSON line written by the holder after taking the lock (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Session lock and crash detection"): `{"pid": <u32>, "startedUtc": "<YYYYMMDDTHHMMSSZ>", "user": "<USERNAME>"}` and LF. The line names the holder in the refusal message only; the lock is the open handle, never the content.

## Thumbnail file

`${ZENITH_OUT}/EditorCache/<Name>/Thumbnails/<GUID>-<key>.zthumb` (`<key>` the 16-hex-digit cache key of `Zenith2/Docs/Design/EDIT/Panels.md` → "Thumbnail generation"):

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | magic | `ZTHB` |
| 4 | 2 | version | 1 |
| 6 | 2 | header bytes | 64 |
| 8 | 2 | width | pixels, 1 … 512 |
| 10 | 2 | height | pixels, 1 … 512 |
| 12 | 1 | pixel format | 1 = RGBA8 sRGB |
| 13 | 1 | codec | `Zenith_CompressionCodecID` of the pixels |
| 14 | 2 | reserved | 0 |
| 16 | 8 | cache key | the key in the file name |
| 24 | 8 | pixels stored bytes | |
| 32 | 32 | pixels SHA-256 | of the raw pixels (width × height × 4 bytes) |
| 64 | … | pixels | row-major, top row first |

A file whose key differs from the current key is stale and is overwritten; a damaged file is deleted and regenerated.

## Clipboard document

The system-clipboard text of a copy (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Copy cut paste and duplicate"), schema `zenith2.editor-clipboard` version 1:

| Key | Type | Meaning |
|---|---|---|
| `$schema`, `schemaVersion` | | `zenith2.editor-clipboard`, 1 |
| `game` | string | the game the objects came from |
| `kind` | string | `scene-objects`, `json-nodes` or `assets` |
| `objects` | array | `scene-objects`: entity and instance objects in the scene document schema's form (`Zenith2/Docs/Design/SCENE/Format.md` → "Scene document schema"), roots first; `json-nodes`: `{ "pointer", "value" }`; `assets`: GUID strings |

Pasting accepts the document only when `$schema`, a known version and `kind` match the target; the text is untrusted (it may come from another application) and is parsed under the default limits and validated object by object.

## Endpoint file

`Games/<Name>/.editor/user/endpoint.json`, schema `zenith2.editor-endpoint` version 1, written when the live-link endpoint opens and deleted when it closes: `{ "port": <u16>, "token": "<64 lowercase hex digits>", "pid": <u32> }`. The token is 32 bytes from the operating system's cryptographic random source (`BCryptGenRandom`), new per session; the file is created with an access-control list granting the current user alone (`Zenith2/Docs/Design/EDIT/Automation.md` → "Live link endpoint").

## Editor trash layout

`Games/<Name>/.editor/trash/<UTC timestamp>/<game-relative path>` holds every file an editor delete moved, and every file the undo of a create, duplicate or import moved (the source and its `.meta`), with `trash.json` in each timestamp directory (`zenith2.editor-trash` version 1: `{ "operation", "files": [ { "path", "sha256" } ] }`), so a record's undo (of a delete) or redo (of a create, duplicate or import) can find and verify its files: the record names its timestamp directory, and moving files back out removes their entries from that `trash.json` (and the directory, once empty). Only the "Empty editor trash" command deletes these files.

## Format registry rows

<!-- assumption: pending PACK (row PK1) -->
The editor's two binary formats join PACK's registry `Zenith2/Tools/Package/format-registry.json` (`Zenith2/Docs/Design/PACK/Formats.md` → "Binary format registry", "Format registry file") in the PACK-registry ticket Z2T-EDIT-121 once PACK's registry file exists (the thumbnail format lands at Stage 3 and the autosave format at Stage 4, before or beside it; each refusal test lands with its format), with these rows:

| Magic | Extension | Owner | Version | Compatibility class | Specified in | Refusal test |
|---|---|---|---|---|---|---|
| `ZAUT` | `.zautosave` | EDIT | 1 | `transient` (recovery state of an editing session, deleted at a clean exit or a save, never shipped) | `Zenith2/Docs/Design/EDIT/Formats.md` → "Autosave file" | `Test_EditorAutosave_DamagedFileRejected` (a version 2 file is treated as damaged: kept for inspection, never read) |
| `ZTHB` | `.zthumb` | EDIT | 1 | `user-state` (an editor cache under `${ZENITH_OUT}/EditorCache/`; a refused file is deleted and regenerated) | `Zenith2/Docs/Design/EDIT/Formats.md` → "Thumbnail file" | `Test_EditorFormats_ThumbnailFileRoundTrip` (a version 2 file is refused, deleted and regenerated) |

Neither magic collides with a row of PACK's table today; a collision found when the row is added is resolved by changing EDIT's magic in that diff, PACK's rule for the subsequent design. The editor's JSON files are not binary formats and have no rows.
<!-- /assumption -->
