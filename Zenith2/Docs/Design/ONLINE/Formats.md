# ONLINE — formats

## Scope of this file

This file is part of the ONLINE design family; the entry document is `Zenith2/Docs/Design/ONLINE.md`. It specifies every byte layout ONLINE reads or writes: the online catalogue authoring document and its cooked form, the script request, result and notification payloads of `ASYNC_SOURCE_ONLINE`, the backend completion payloads, the cloud envelope and cloud index, the sync manifest, the device record, and the offline backend's files. The algorithms that use them are `Zenith2/Docs/Design/ONLINE/Accounts.md` and `Zenith2/Docs/Design/ONLINE/CloudSave.md`.

## Conventions for every ONLINE format

- Integers are little-endian and unaligned; a reader copies them out with `memcpy`-free byte assembly (`Zenith2/Docs/Design/CORE.md` → "Public API", byte readers), never by casting a pointer.
- Every binary file and cloud object begins with a 4-byte ASCII magic and a `uint16` format version; a reader refuses a newer version (`ONLINE_ERROR_CORRUPT`, rule "version") and reads every older version it ever shipped (each format's version history is listed with it; version 1 is the only version at Stage 4). Refusing a newer cloud index is not treating it as absent: the reconcile classifies it as the NEWER_FORMAT view, under which this build never writes the cloud (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view").
- Header checks use `Zenith_SaveCrc32c` (`Zenith2/Docs/Design/SAVE/Formats.md` → "CRC-32C"); content identity uses SHA-256 (`Zenith_Sha256Digest`, `Zenith2/Docs/Design/CORE/Maths.md`), because a cloud copy's identity must be collision-resistant across devices.
- Strings are length-prefixed UTF-8 (a `uint8` length for names, a `uint16` length where stated), never NUL-terminated, and validated against their grammar on read.
- Every parser validates every size, count and offset against the input length with overflow-checked arithmetic before use, returns `ONLINE_ERROR_CORRUPT` with `m_uDetail` the number of the failed rule in its table, and never asserts ([Z2-N-ARCH-951]); each has a libFuzzer target (`Zenith2/Docs/Design/ONLINE/Tests.md` → "Format and fuzz tests").
- No ONLINE format holds an account identifier, a display name of a player, a token or a purchase receipt; the save display names of envelopes and index entries are game-authored text. The device ID is a random identity of one installation, not of a person; the install secret exists only in the device record.
- File paths are virtual paths on the user mount (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots"), written with `Zenith_AsyncIO::WriteAtomic` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"), and every one passes PLAT's rule R10 (a `user:/` path's first component is `config`, `saves`, `cache` or `logs`; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order"), by orchestrator ruling ONLINE-R10-PATHS: ONLINE's durable state (the device record, the sync manifests, the offline backend's achievement files and cloud store) lives in the ONLINE-owned directory `user:/saves/online/`, and derivable state, of which ONLINE has none today (every ONLINE file is either durable or rebuilt in memory), would live under `user:/cache/online/`. ONLINE never writes a SAVE file and never touches a profile directory `saves/<profile>/`, `saves/resume.zsave` or `saves/last_profile.zslp`.

<!-- assumption: pending SAVE (row ON22) -->
SAVE reserves `saves/online/` for ONLINE: `online` is never a profile key (`Zenith_MakeSaveProfileKey` yields 16 hexadecimal digits, and the local key is `local`), and no SAVE enumeration, journal recovery, listing, quota count or cleanup reads, lists or deletes anything below it. <!-- /assumption -->

On Android the Gradle template's backup and transfer rules include `saves/` and exclude, among SAVE's own files, the literal path `saves/online/device.zodv` (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Backup and transfer rules", landed for row ON18 (d)); Android rule paths are literal files or directories, with no wildcard, so the one other ONLINE file that must not travel is named literally too: `saves/online/device.zodv.z2atomic`, the temporary `WriteAtomic` writes beside the record during a rewrite (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes" W4), which briefly holds the install secret (requested from MOD through row ON22 (b) as amended; `Zenith_AsyncIO::Create` sweeps a stale one at the next start). The device record holds the install secret and the device ID, which a restored backup would clone onto another device; the sync manifests and the offline store travel with the saves they describe.

## Online catalogue document

One per game, at `Zenith2/Games/<Name>/Online/online.json` by convention (any content root is accepted). Stable type name `Zenith.Online.CatalogueDocument`, schema tag `zenith2.online`, schema version 1; an authoring document in the sense of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", migrated through SCENE's document framework (`Zenith2/Docs/Design/SCENE/DocumentFramework.md`), with unknown fields preserved.

### Document schema

```json
{
	"schema": "zenith2.online",
	"version": 1,
	"guid": "5b1f8c2e-0d7a-4c33-9b6e-2f4a1c8d9e70",
	"achievements": [
		{ "id": "first_steps", "kind": "standard", "hidden": false,
		  "steam": { "name": "ACH_FIRST_STEPS" }, "googlePlay": { "id": "CgkI8aaaaaaaEAIQAQ" } },
		{ "id": "collector", "kind": "incremental", "steps": 50,
		  "steam": { "name": "ACH_COLLECTOR", "progressStat": "STAT_COLLECTED" }, "googlePlay": { "id": "CgkI8aaaaaaaEAIQAg" } }
	],
	"entitlements": [
		{ "id": "dlc.expansion_isles", "steam": { "appId": 480 }, "googlePlay": { "productId": "expansion_isles" }, "offline": { "granted": true } }
	],
	"cloud": {
		"kinds": ["manual", "quick", "autosave"],
		"manualPolicy": "ask",
		"autosavePolicy": "preferNewestUtc",
		"maxSlots": 32
	}
}
```

| Key | Type | Rule (a violation is a validation error naming the JSON pointer) |
|---|---|---|
| `schema` | string | exactly `zenith2.online` |
| `version` | integer | 1 … the current schema version; older versions migrate |
| `guid` | string | a `Zenith_PersistentID` in its 36-character form ([Z2-N-CONTRACT-067]); the asset's GUID |
| `achievements` | array, 0 … 1,024 | IDs unique |
| `achievements[].id` | string | `^[a-z][a-z0-9_.]{0,63}$` |
| `achievements[].kind` | string | `standard` or `incremental` |
| `achievements[].steps` | integer | required for `incremental`, 2 … 1,000,000; absent for `standard` |
| `achievements[].hidden` | bool | optional, default false |
| `achievements[].steam.name` | string | optional; 1 … 128 bytes of `[A-Za-z0-9_]`; unique among achievements |
| `achievements[].steam.progressStat` | string | required when `steam.name` is present and `kind` is `incremental`, forbidden otherwise; 1 … 128 bytes of `[A-Za-z0-9_]` |
| `achievements[].googlePlay.id` | string | optional; 1 … 128 bytes of `[A-Za-z0-9_-]`; unique |
| `entitlements` | array, 0 … 256 | IDs unique |
| `entitlements[].id` | string | `^[a-z][a-z0-9_.]{0,63}$` (PACK's entitlement grammar, `Zenith2/Docs/Design/PACK/Formats.md` → "Package manifest tables") |
| `entitlements[].steam.appId` | integer | optional; 1 … 4,294,967,295; unique |
| `entitlements[].googlePlay.productId` | string | optional; 1 … 128 bytes of `[a-z0-9_.]`, beginning with a letter or digit (Play's product-ID rule); unique |
| `entitlements[].offline.granted` | bool | optional, default false |
| `cloud.kinds` | array of strings | subset of `manual`, `quick`, `autosave`, `checkpoint`; default `["manual", "quick", "autosave"]`; `resume` is refused |
| `cloud.manualPolicy`, `cloud.autosavePolicy` | string | `ask`, `preferNewestUtc`, `preferLongestPlaytime`, `preferLocal`, `preferCloud`; defaults `ask` and `preferNewestUtc` |
| `cloud.maxSlots` | integer | 1 … 64, default 32 |

### Validation rules

`Zenith_ValidateOnlineCatalogueDocument` (`ZenithAuthoring`, `Zenith2/Docs/Design/ONLINE.md` → "Authoring and cooking"), which the cook step `online.catalogue` and the editor's document view both call, runs these after the JSON parser limits of `Zenith2/Docs/Architecture.md` → "Security architecture" and reports every failure, each as `online.json: <pointer>: <message>`:

| # | Rule |
|---|---|
| C1 | every key of the schema table has its type and range |
| C2 | IDs unique within `achievements` and within `entitlements` |
| C3 | Steam names, Steam app IDs, Play achievement IDs and Play product IDs unique within their kind |
| C4 | `steps` present exactly for `incremental` |
| C5 | `progressStat` present exactly for an incremental achievement with a Steam name |
| C6 | `cloud.kinds` holds no duplicate and no value outside the four |
| C7 | the document's `guid` equals its `.meta` GUID |
| C8 | a warning, not an error: an achievement or entitlement with neither a Steam nor a Play mapping (it exists on the offline backend alone) |

### Migration chain

Version 1 is the first version; the chain is empty. Its fixture is `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Online/CatalogueDocument/v1.json` (a hand-written document exercising every key) with `v1.expected.json` beside it (the same document, as the migration framework writes it back), under `ZenithAuthoring_Tests`' fixture root by the rule of `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 2 (orchestrator ruling AUTHORING-CONTRACT); the document type and its migration steps live in `ZenithAuthoring`, whose test target is the only one that can link them. Every subsequent version adds one step registered with the document type by `Zenith_RegisterOnlineDocumentTypes` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md`) and one fixture pair `v<N>.json`, `v<N>.expected.json`, per [Z2-N-CONTRACT-552]'s per-type fixture rule; core-doc item X336 adds the type to that rule's list.

## Cooked online catalogue

The cook writes, through `Zenith_CompileOnlineCatalogue` (`ZenithAuthoring`), one cooked asset file of the online catalogue kind (requested from ASSET, cross-area row ON11; magic `ZONC`, extension `.zonc`, format version `uONLINE_CATALOGUE_FORMAT_VERSION` = 1) in ASSET's container (`Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file"): the 64-byte header, then exactly one section of kind 1 (`ONLINE_CATALOGUE_SECTION_BODY`), stored uncompressed. The section body:

| Offset | Size | Field | Rule (`Zenith_OnlineCatalogue::Parse` refuses a violation with this rule number) |
|---|---|---|---|
| 0 | 4 | magic | `5A 4F 4E 43` ("ZONC") (1) |
| 4 | 2 | version | 1 (2) |
| 6 | 2 | achievement count A | ≤ 1,024 (3) |
| 8 | 2 | entitlement count E | ≤ 256 (3) |
| 10 | 1 | synced kind mask | bits 0 … 3 only (4) |
| 11 | 1 | manual policy | 1 … 5 (4) |
| 12 | 1 | autosave policy | 1 … 5 (4) |
| 13 | 1 | max slots | 1 … 64 (4) |
| 14 | 2 | reserved | 0 (5) |
| 16 | 4 | string pool bytes S | ≤ 1,048,576 (6) |
| 20 | 4 | CRC-32C of bytes 0 … 19 | (7) |
| 24 | 24 × A | achievement records | below; IDs strictly ascending in byte order (8) |
| 24 + 24A | 16 × E | entitlement records | below; IDs strictly ascending (8) |
| 24 + 24A + 16E | S | string pool | UTF-8 strings referenced by (offset, length) pairs; every reference inside the pool (9); every ID passes its grammar (10) |

Achievement record (24 bytes):

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | ID offset | (9) |
| 4 | 1 | ID length | 1 … 64 (10) |
| 5 | 1 | kind | 1 or 2 (11) |
| 6 | 1 | flags | bit 0 hidden; other bits 0 (5) |
| 7 | 1 | Steam name length | 0 … 128 (11) |
| 8 | 4 | total steps | 0 for standard, 2 … 1,000,000 for incremental (11) |
| 12 | 4 | Steam name offset | (9) |
| 16 | 1 | Steam progress-stat length | 0 … 128; non-zero exactly for an incremental achievement with a Steam name (11) |
| 17 | 1 | Play ID length | 0 … 128 (11) |
| 18 | 2 | reserved | 0 (5) |
| 20 | 4 | Steam progress-stat offset | the Play ID's bytes follow the progress stat's immediately in the pool (9) |

Entitlement record (16 bytes): `uint32` ID offset, `uint8` ID length (1 … 64), `uint8` flags (bit 0 offline-granted), `uint8` Play product-ID length (0 … 128), `uint8` reserved 0, `uint32` Steam app ID (0: none), `uint32` Play product-ID offset.

The asset's content hash (ASSET's header field) is the DDC key; `Zenith_OnlineCatalogue::GetContentHash` is `Zenith_Hash64` of the section body.

## Script request payloads

The payload of a `SIDE_EFFECT_KIND_ASYNC_REQUEST` record the `Online` bindings write; at most `uONLINE_REQUEST_MAX_BYTES` (80). Byte 0 is the source kind, as the runtime's multiplexer requires (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime").

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `ASYNC_SOURCE_ONLINE` (the registry's value) |
| 1 | 1 | operation (`Zenith_OnlineScriptOp`, below) |
| 2 | 2 | argument bytes N |
| 4 | N | arguments |

| Op | Value | Arguments |
|---|---|---|
| `ONLINE_SCRIPT_OP_QUERY_IDENTITY` | 1 | none |
| `ONLINE_SCRIPT_OP_REQUEST_SIGN_IN` | 2 | `uint8` interactive (0 or 1) |
| `ONLINE_SCRIPT_OP_QUERY_ENTITLEMENT` | 3 | `uint8` ID length, ID |
| `ONLINE_SCRIPT_OP_UNLOCK_ACHIEVEMENT` | 4 | `uint8` ID length, ID |
| `ONLINE_SCRIPT_OP_SET_ACHIEVEMENT_STEPS` | 5 | `uint8` ID length, ID, `uint32` steps |
| `ONLINE_SCRIPT_OP_INCREMENT_ACHIEVEMENT` | 6 | `uint8` ID length, ID, `uint32` delta |
| `ONLINE_SCRIPT_OP_QUERY_ACHIEVEMENT` | 7 | `uint8` ID length, ID |
| `ONLINE_SCRIPT_OP_QUERY_CLOUD_SLOT` | 8 | `uint8` slot length, slot |
| `ONLINE_SCRIPT_OP_RESOLVE_CLOUD_CONFLICT` | 9 | `uint8` slot length, slot, `uint8` `Zenith_OnlineResolveChoice`, the 41-byte conflict token (below; all zero for DEFER) |
| `ONLINE_SCRIPT_OP_SYNC_CLOUD_NOW` | 10 | none |
| `ONLINE_SCRIPT_OP_REPAIR_CLOUD_INDEX` | 11 | none |

The largest request is RESOLVE_CLOUD_CONFLICT: 4 + 1 + 32 + 1 + 41 = 79 bytes, within 80.

`Zenith_DecodeOnlineScriptRequest` rules: R1 length ≥ 4; R2 byte 0 is `ASYNC_SOURCE_ONLINE`; R3 op 1 … 11; R4 4 + N equals the payload length; R5 the arguments of the op exactly fill N; R6 IDs pass `Zenith_IsValidOnlineCatalogueID` and slots `Zenith_ParseSaveSlotName`; R7 numeric ranges of `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API". The bindings validate before recording, so a decode failure means a defect or a corrupt record; the session completes such a request `ONLINE_STATUS_REFUSED` and counts `online.requests.decode_failed`.

## Script result payloads

Published under `ASYNC_SOURCE_ONLINE` with `PUBLICATION_KIND_GENERIC_RECORDED`, at most `uONLINE_RESULT_MAX_BYTES` (256). Every result begins with an 8-byte header:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | record kind: 1 (`ONLINE_RECORD_RESULT`) |
| 1 | 1 | operation (the request's op) |
| 2 | 1 | status (`Zenith_OnlineStatus`) |
| 3 | 1 | reserved, 0 |
| 4 | 4 | bind generation (low 32 bits) at completion |

Then the body of the operation (present for every status; zero-filled when the status is not OK):

| Op | Body |
|---|---|
| QUERY_IDENTITY, REQUEST_SIGN_IN | `uint8` signed in, `uint8` backend kind, `uint16` reserved (4 bytes) |
| QUERY_ENTITLEMENT | `uint8` `Zenith_OnlineEntitlementState`, 3 reserved (4 bytes) |
| UNLOCK_ACHIEVEMENT | `uint8` already unlocked, 3 reserved (4 bytes) |
| SET_ACHIEVEMENT_STEPS, INCREMENT_ACHIEVEMENT, QUERY_ACHIEVEMENT | `uint32` steps, `uint32` total, `uint8` unlocked, 3 reserved (12 bytes) |
| QUERY_CLOUD_SLOT | `uint8` `Zenith_OnlineCloudSlotState`, `uint8` `Zenith_OnlineConflictReason`, 2 reserved, the 41-byte conflict token (zero unless the slot is in CONFLICT) (45 bytes) |
| RESOLVE_CLOUD_CONFLICT | `uint8` choice applied (0 when none), `uint8` new slot length (0 … 32), new slot bytes (0 … 32) |
| SYNC_CLOUD_NOW | `uint16` conflicts present after the sync, `uint16` transfers performed (4 bytes) |
| REPAIR_CLOUD_INDEX | `uint16` slots in the repaired index, 2 reserved (4 bytes) |

A zero-length payload is "abandoned at a restore" (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section"). `Zenith_DecodeOnlineResultForScript` maps it to `{ status = "abandoned" }`, maps a well-formed record to the fields of `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API", and returns false for any other shape (a record kind other than 1, a length not matching the op), which halts the world as an internal inconsistency (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table"), because only ONLINE's encoder or a replay log that recorded it produces these bytes.

## Notification payloads

Published under `ASYNC_SOURCE_ONLINE` with `PUBLICATION_KIND_GENERIC_RECORDED` from completion IDs ONLINE allocates in the frame prologue ([Z2-N-SCRIPT-341]; orchestrator ruling Q89 (a): prologue allocations are recorded kinds only, and allocate nothing under `--replay`). Every notification begins with an 8-byte header:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | record kind: 2 (`ONLINE_RECORD_NOTIFICATION`) |
| 1 | 1 | notification kind (below) |
| 2 | 2 | reserved, 0 |
| 4 | 4 | bind generation (low 32 bits) |

| Kind | Value | Body |
|---|---|---|
| `ONLINE_NOTIFY_IDENTITY_CHANGED` | 1 | `uint8` signed in, `uint8` reason (1 signed in, 2 signed out, 3 switched, 4 sign-in failed, 5 attached: the identity a world finds when it attaches to a bound session), `uint8` backend kind, `uint8` flags (bit 0: previous-profile session), `uint8` display-name length (0 … 64), display-name bytes |
| `ONLINE_NOTIFY_ENTITLEMENTS_CHANGED` | 2 | `uint64` entitlement revision, `uint16` granted count, 2 reserved |
| `ONLINE_NOTIFY_CLOUD_CONFLICT` | 3 | `uint8` slot length (1 … 32), slot (32 bytes, zero-padded), `uint8` save kind, `uint8` reason, `uint8` flags (bit 0: the cloud copy came from this device), `uint8` reserved, `uint64` local UTC seconds, `uint64` cloud UTC seconds, `uint64` local playtime seconds, `uint64` cloud playtime seconds, the 41-byte conflict token (below), `uint8` cloud display-name length (0 … 128), the cloud copy's save display name |
| `ONLINE_NOTIFY_CLOUD_SLOT_REPLACED` | 4 | `uint8` slot length, slot (32 bytes, zero-padded), `uint8` save kind, `uint8` cause (1 download, 2 keep cloud, 3 keep both, 4 deleted in cloud), `uint8` new-slot length (0 … 32), new slot (32 bytes, zero-padded) |
| `ONLINE_NOTIFY_CLOUD_STATUS` | 5 | `uint8` status (1 available, 2 unavailable, 3 disabled, 4 quota exceeded, 5 degraded, 6 index unreadable, 7 read only), `uint8` reason (0 none, 1 network, 2 user setting, 3 app setting, 4 quota, 5 profile switch failed, 6 index invalid, 7 index foreign, 8 index missing, 9 index newer), 2 reserved |

The **conflict token** (`Zenith_OnlineConflictToken`, 41 bytes, the inputs a conflict record was built from, `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Conflict detection"): offset 0 `uint8` kinds (bits 0 … 1 local: 1 absent, 2 present; bits 2 … 3 cloud: 1 absent, 2 entry, 3 deletion record; other bits 0), 1 the first 16 bytes of the local payload hash (zero when absent), 17 `uint64` the cloud entry's or record's commit serial (0 when absent), 25 the first 16 bytes of the cloud hash (the entry's payload hash or the record's deleted hash; zero when absent). Two tokens are equal when all 41 bytes are; the 128-bit prefixes make an accidental match between different inputs negligible, and a match can only make a resolution act on the very inputs it names. Rule T1: the kinds byte has both fields in range and no other bit set, and every field of an absent side is zero.

The largest notification body is CLOUD_CONFLICT: 8 + 1 + 32 + 4 + 32 + 41 + 1 + 128 = 247 bytes, within 256. `Zenith_DecodeOnlineNotification` rules: N1 length ≥ 8; N2 record kind 2; N3 kind 1 … 5; N4 the body length equals the kind's fixed part plus the lengths its length fields declare (IDENTITY_CHANGED: 5 + the display-name length; CLOUD_CONFLICT: 111 + the cloud display-name length; the other kinds are fixed); N5 enumerated bytes in range (IDENTITY_CHANGED reason 1 … 5; CLOUD_STATUS status 1 … 7, reason 0 … 9); N6 string lengths within their maxima and inside the payload; N7 slot names pass `Zenith_ParseSaveSlotName`; N8 a CLOUD_CONFLICT token passes rule T1. A failure (possible only from a corrupt replay log) drops the notification and counts `online.events.decode_failed`.

## Backend completion payloads

The `m_xPayload` of a `Zenith_OnlineCompletion` with `ONLINE_STATUS_OK`; every other status carries an empty payload. The session validates each as untrusted backend data (rules B1 … B6); a violation completes the request as `ONLINE_STATUS_BACKEND_ERROR` with `ONLINE_ERROR_CORRUPT` and counts `online.backend.bad_payload`.

| Kind | Payload |
|---|---|
| SIGN_IN_SILENT, SIGN_IN_INTERACTIVE, SIGN_OUT, SWITCH_ACCOUNT | empty (the identity arrives through `OnIdentityChanged`) |
| QUERY_ENTITLEMENTS | `uint32` count (B1: equal to the request's native-ID count), then count × `uint8` `Zenith_OnlineEntitlementState` in request order (B2: 1 … 3) |
| QUERY_ACHIEVEMENTS | `uint32` count (B1), then count × { `uint8` unlocked (0 or 1), 3 reserved, `uint32` steps } in request order (B3: steps ≤ the catalogue total) |
| UNLOCK_ACHIEVEMENT | `uint8` already unlocked |
| SET_ACHIEVEMENT_STEPS | `uint32` steps now stored (B3), `uint8` unlocked |
| CLOUD_LIST | `uint32` count (B4: ≤ `uONLINE_MAX_CLOUD_LIST_OBJECTS`, 1,024: the index, every slot's referenced object and the garbage awaiting collection), then per object { `uint8` name length (1 … 100), name, `uint64` stored bytes (B5: ≤ `GetMaxCloudObjectBytes()`), `uint64` modification UTC seconds, 32-byte revision }; a listing of more objects is `BACKEND_ERROR`, which leaves the reconcile's view UNKNOWN (READ_FAILED) |
| CLOUD_READ | `uint64` stored bytes B (B5, B6: ≤ the read buffer), 32-byte revision, then the B bytes (the payload span points into the request's read buffer) |
| CLOUD_WRITE | 32-byte new revision |
| CLOUD_DELETE | empty |

### Cloud object names

| Object | Name |
|---|---|
| a slot's envelope | `z2.<slot>.<b>.<h>`: `<slot>` the SAVE slot name (`[a-z0-9_-]`, 1 … 32 bytes), `<b>` the index serial the uploader read at U3 (its base serial) as 16 lower-case hexadecimal digits, `<h>` the first 16 lower-case hexadecimal digits of the payload SHA-256; at most 2 + 1 + 32 + 1 + 16 + 1 + 16 = 69 bytes |
| the cloud index | `z2.index` |

Envelope names are content-addressed and unique per upload: two uploads of one slot share a name only when they read the same index serial and carry identical bytes (with overwhelming probability; the uploader compares the payload hash before relying on an existing object, `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence" U5), so an object is never overwritten with different bytes and the object a committed index entry references is never replaced. `Zenith_ParseOnlineCloudObjectName(name) → { slot, base serial, hash prefix }` accepts exactly this grammar (`z2.`, a slot passing `Zenith_ParseSaveSlotName` for any kind, `.`, 16 lower-case hexadecimal digits, `.`, 16 lower-case hexadecimal digits) and refuses `z2.index` and every other name. The names are valid on every backend: Steam remote-storage file names (`zenith2/<name>`, 77 bytes, below Steam's 260), Play snapshot unique names (`[a-zA-Z0-9._~-]` up to 100 characters), and offline file names.

The Steam backend stores these as remote-storage file names `zenith2/<name>`; the Play backend as snapshot unique names; the offline backend as files `<cloud root>/<store key>/<name>` (§ "Offline cloud store"). No other object is written; an object found by CLOUD_LIST whose name is neither `z2.index` nor an envelope name is ignored (it may belong to a newer release), never deleted.

## Cloud envelope

The stored form of one slot in the cloud: a 320-byte header followed by the exact bytes of the slot's ZSAV file (`Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSAV file"), so the cloud copy is the same save byte for byte.

| Offset | Size | Field | Rule (`Zenith_ParseOnlineEnvelope` refuses a violation with this rule number) |
|---|---|---|---|
| 0 | 4 | magic | `5A 43 4C 44` ("ZCLD") (1) |
| 4 | 2 | version | 1 (2) |
| 6 | 2 | header bytes | 320 (3) |
| 8 | 8 | payload bytes P | 128 … 268,435,456 (SAVE's file limit); 320 + P equals the object length (4) |
| 16 | 32 | payload SHA-256 | equals `Zenith_Sha256Digest` of the payload (5; checked by the downloader and the index repair, which have the payload) |
| 48 | 1 | save kind | 1 … 4 (6) |
| 49 | 1 | writer backend kind | 1 … 3 (6) |
| 50 | 1 | slot length | 1 … 32 (7) |
| 51 | 1 | profile key length | 5 or 16 (9) |
| 52 | 32 | slot name | zero-padded; passes `Zenith_ParseSaveSlotName` (7); equals the object name's slot (7, checked by the caller, which has the name) |
| 84 | 16 | profile key | the `Zenith_SaveProfileKey` bytes, zero-padded; equals the active profile at download (9, checked by the caller) |
| 100 | 1 | display-name length | 0 … 128 (`uSAVE_MAX_DISPLAY_NAME_BYTES`) (11) |
| 101 | 3 | reserved | 0 (8) |
| 104 | 8 | save sequence | the ZSAV header's sequence (informative) |
| 112 | 8 | UTC seconds | the ZSAV header's UTC seconds |
| 120 | 8 | playtime seconds | the METADATA playtime |
| 128 | 4 | save format version | major in the high 16 bits, minor in the low 16 (10: refused for download when newer or of another major; the slot becomes CLOUD_NEWER; checked by the caller) |
| 132 | 4 | reserved | 0 (8) |
| 136 | 8 | writer device ID | the uploading installation's device ID |
| 144 | 8 | base serial | the index serial the uploader read at U3; equals the object name's `<b>` and the index entry's base serial (14, checked by the caller) |
| 152 | 128 | save display name | the METADATA display name, whole (SAVE's limit is 128 bytes, so it is never truncated), zero-padded (UTF-8, validated, 11) |
| 280 | 36 | reserved | 0 (8) |
| 316 | 4 | CRC-32C of bytes 0 … 315 | (12) |

The ZSAV payload's own METADATA `zenith.profile` pair must equal the envelope's profile key (rule 13, checked by SAVE's import, `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Download sequence"), so a cloud object can never install under another account's profile even if an index or envelope were copied between accounts.

## Cloud index

The object `z2.index`: the list of slots the cloud holds and the log of its most recent commits, written by the session after every slot upload and deletion and by the explicit index repair, with a conditional write where the backend supports it. Its revision is the concurrency token of the whole cloud; its serial orders committed versions.

| Offset | Size | Field | Rule (`Zenith_ParseOnlineCloudIndex`) |
|---|---|---|---|
| 0 | 4 | magic | `5A 43 49 58` ("ZCIX") (1) |
| 4 | 2 | version | 1 (2: 0 refused; an object of at least 48 bytes (the fixed header before the commit log) whose magic is `ZCIX` and whose version is above 1 never reaches the parser, because the classifier checks length and magic and then reads bytes 4 … 5 and classifies it NEWER_FORMAT; an object with a wrong magic or under 48 bytes is INVALID by rule 1 or 9 whatever bytes 4 … 5 hold, `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view") |
| 6 | 2 | entry count n | ≤ 128, of which at most 64 live entries and at most 64 deletion records (3) |
| 8 | 16 | profile key bytes, zero-padded | the writer's active profile; compared with the active profile by the caller after parsing (a difference is the FOREIGN_PROFILE view), never by the parser, which has no profile parameter |
| 24 | 1 | profile key length | 5 or 16 (4) |
| 25 | 1 | flags | bit 0 repaired (written by `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index repair"; cleared by the next ordinary write); bit 1 repair floor (diagnostic only: it selects a conflict's log line and changes no decision; set by the repair with its floor; copied by every ordinary and record-dropping write; cleared by the write whose log overflow sets a new floor); other bits 0 (5) |
| 26 | 6 | reserved | 0 (5) |
| 32 | 8 | index serial | ≥ 1 (10); every writer sets it to the serial it read plus 1 (0 read for an EMPTY view); the repair sets it to 1 + the largest base serial it keeps |
| 40 | 4 | CRC-32C of the whole object with this field read as zero | (6) |
| 44 | 4 | reserved | 0 (5) |
| 48 | 8 | log floor | the greatest commit serial the log no longer represents: 0 at creation, the dropped pair's serial when the log overflows, the repair's own serial after a repair; ≤ the index serial (12) |
| 56 | 2 | log count m | 0 … 128 (11) |
| 58 | 6 | reserved | 0 (5) |
| 64 | 16 × 128 | commit log | m pairs { `uint64` commit serial, `uint64` commit token }, serials strictly ascending, each > the log floor and ≤ the index serial, every token non-zero (12); the unused tail of the 128 pairs is zero (5) |
| 2,112 | 256 × n | entries | below; slot names strictly ascending (7) |

Entry (256 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | slot length (1 … 32) |
| 1 | 32 | slot name, zero-padded (passes `Zenith_ParseSaveSlotName`, rule 8) |
| 33 | 1 | save kind (1 … 4, rule 8) |
| 34 | 1 | writer backend kind (1 … 3, rule 8): in a deletion record, the deleting writer's backend kind |
| 35 | 1 | display-name length (0 … 128, rule 8) |
| 36 | 32 | payload SHA-256 |
| 68 | 8 | payload bytes (128 … 268,435,456 for a live entry, 0 for a deletion record, rule 8) |
| 76 | 8 | UTC seconds |
| 84 | 8 | playtime seconds |
| 92 | 8 | writer device ID |
| 100 | 8 | base serial: the `<b>` of the entry's object name; less than its commit serial (rule 10); 0 in a deletion record |
| 108 | 4 | save format version |
| 112 | 128 | save display name, zero-padded (UTF-8, validated, rule 8) |
| 240 | 1 | flags: bit 0 deleted (a deletion record), bit 1 repaired (an entry the index repair wrote); other bits 0 (rule 8) |
| 241 | 7 | reserved, 0 (rule 5) |
| 248 | 8 | commit serial: the index serial of the write that last wrote this entry or record; ≥ 1 and ≤ the index serial (rule 10) |

A **deletion record** (flags bit 0) keeps the slot name, the save kind, the deleting writer's backend kind, the payload SHA-256 of the copy it deleted, the UTC seconds of the deletion and its commit serial; its payload bytes, playtime, device ID, base serial, format version and display name are 0 (rule 8). It references no object. A slot has at most one entry or one record (rule 7: slot names strictly ascending across both).

Rule 8: every enumerated byte in range and every length inside its field. The object's length is exactly 2,112 + 256 n (rule 9). Rule 10: the index serial is ≥ 1, every commit serial is ≤ the index serial, and every live entry's base serial is less than its commit serial.

### Commit tokens and the commit log

The commit token of a write of slot S with payload hash h (a live entry, deleted = 0) or of a deletion record of S that deleted h (deleted = 1) is LE64 of the first 8 bytes of `Zenith_Sha256Digest("z2.commit" ‖ uint8 slot length ‖ slot bytes ‖ the 32 bytes of h ‖ uint8 deleted)`, with 0 mapped to 1, computed by `Zenith_ComputeOnlineCommitToken`. Every committing write (U6, `DELETE_CLOUD`) copies the log of the index it read at U3 (an empty log with floor 0 for an EMPTY view), appends its own pair (its commit serial, its token) and, when the count would exceed 128, drops the oldest pair, sets the log floor to that pair's serial and clears the repair-floor flag; a write that drops deletion records copies the log, the floor and the flag unchanged (it bumps the index serial without a pair, so it moves no base out of the log); the index repair writes an empty log with the floor equal to its own serial and the repair-floor flag set. The log is the lineage evidence the decision table reads (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"), and positive evidence only: a device's base is proven to be in the history of the index it reads exactly when the log holds the pair (the base serial, the token of the base); a base serial at or below the floor, an overflow floor (128 or more committing writes ago) or a repair floor alike, is untestable (`HORIZON`), and the decision treats it exactly as another history; the repair-floor flag only names the cause in the log. The entry's object name is derived, never stored: `z2.<slot>.<base serial as 16 hexadecimal digits>.<the first 16 hexadecimal digits of the payload SHA-256>` (§ "Cloud object names"). An index that fails any rule, or whose profile key differs, or that is missing while envelope objects exist, is an UNKNOWN view under which nothing is deleted, downloaded, uploaded or rewritten; it is never "treated as absent" and never "rebuilt by the next upload" (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view", "Index repair").

## Sync manifest

`user:/saves/online/<profile key>/sync.<backend>.zosm`, one per profile key and backend, `<backend>` the lower-case `BACKEND_NAME` (`offline`, `steam`, `googleplay`), so a game that moves between backends keeps each backend's bases in its own file: the session's record of the last state both sides agreed on (the base of the three-way decision). Written atomically after every change of a slot's base or state.

| Offset | Size | Field | Rule (`Zenith_ParseOnlineSyncManifest`) |
|---|---|---|---|
| 0 | 4 | magic | `5A 4F 53 4D` ("ZOSM") (1) |
| 4 | 2 | version | 1 (2) |
| 6 | 1 | backend kind | 1 … 3 (3) |
| 7 | 1 | reserved | 0 (4) |
| 8 | 2 | entry count n | ≤ 64 (5) |
| 10 | 2 | reserved | 0 (4) |
| 12 | 4 | CRC-32C of the whole file with this field read as zero | (6) |
| 16 | 104 × n | entries | slot names strictly ascending (7) |

Entry (104 bytes; version 1 is unshipped, so the layout grew in place):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | slot length (1 … 32) |
| 1 | 32 | slot name, zero-padded (passes `Zenith_ParseSaveSlotName`, rule 8) |
| 33 | 1 | save kind (1 … 4, rule 8) |
| 34 | 1 | state: `Zenith_OnlineCloudSlotState`, persisted only as IN_SYNC, CONFLICT, SKIPPED_VERSION, CLOUD_NEWER, TOO_LARGE or NOT_SYNCED; a transient state (CLOUD_UNKNOWN and ERROR included) is written as the state it started from (rule 8) |
| 35 | 1 | conflict reason (0 … 4, rule 8) |
| 36 | 32 | base payload SHA-256: the ZSAV bytes both sides last agreed on; all zero means no base |
| 68 | 1 | flags: bit 0 tombstone (SAVE reported the slot deleted after it was synced, `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"); bit 0 is set only when the base is non-zero; other bits 0 (rule 8) |
| 69 | 3 | reserved, 0 (rule 4) |
| 72 | 8 | local sequence: the ZSAV header sequence of the local file whose hash is the base (the reconcile's fast path) |
| 80 | 8 | local file bytes of that file |
| 88 | 8 | base serial: the commit serial of the index entry that recorded the base (U7: the serial the upload committed; D5 and ADOPT: the entry's commit serial); 0 exactly when the base is all zero (rule 8) |
| 96 | 8 | previous base token: the commit token (deleted 0) of the base this slot held before the current one, written by U7, D5 and ADOPT whenever they change the base hash; 0 for none |

A manifest whose backend byte differs from its file name's backend is invalid (rule 3), so one backend's bases are never read as another's.

A missing or invalid manifest means "no base and no tombstone for any slot": the reconcile then sees every slot as never synced, which can only produce FIRST_SYNC conflicts, adoptions, uploads of local-only slots and downloads of cloud-only slots, never a silent overwrite or a deletion (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table").

## Device record

`user:/saves/online/device.zodv`, 64 bytes, never a cloud object, never logged, and excluded from Android device backup and transfer (cross-area rows ON18 (d), ON22 (b)), because it holds the install secret:

| Offset | Size | Field | Rule (`Zenith_ParseOnlineDeviceRecord`) |
|---|---|---|---|
| 0 | 4 | magic | `5A 4F 44 56` ("ZODV") (1) |
| 4 | 2 | version | 1 (2) |
| 6 | 2 | reserved | 0 (3) |
| 8 | 8 | device ID | non-zero (4) |
| 16 | 32 | install secret S | not all zero (4) |
| 48 | 12 | reserved | 0 (3) |
| 60 | 4 | CRC-32C of bytes 0 … 59 | (5) |

Created on first start: 40 bytes from the OS random source (`Zenith_Platform::FillOSRandom`, assumption pending PLAT, row ON19 (b)), the first 8 the device ID (a zero draw is replaced by 1) and the next 32 the install secret, written with `WriteAtomic`. The device ID is presentation data and never reaches simulation; the install secret keys the account key PLAT's signals carry (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys") and is never written anywhere else. A missing or invalid record is replaced by a new one, logged `online: device record replaced`, which makes the next cloud conflict report its cloud copy as "not from this device" and gives this install new account keys, which nothing compares across processes. A replacement that cannot be written fails `Create` with `ONLINE_ERROR_IO`, and an OS random source that fails fails it with `ONLINE_ERROR_BACKEND` (`Test_OnlineSession_Create_FailsAtDeviceRecord_Unwinds`), because a secret that changes every process would still be safe but a device ID that changes every process would report every cloud copy as foreign to this device.

## Offline achievement file

`user:/saves/online/offline/<store key>/achievements.zoac`, where `<store key>` is the offline account's **store key**: the first 8 bytes of `Zenith_Sha256Digest("zenith2.offline-store:" ‖ account ID)`, each byte written as two lower-case hexadecimal digits, byte 0 first (`offline:alice` → `2796e9763c2cb976`, the first 8 bytes of SHA-256 of the 35 bytes `zenith2.offline-store:offline:alice`), a fixed, unkeyed derivation, so the offline store survives a replaced device record; written by the offline backend after every change and read at sign-in. The offline backend keys its files by store key rather than by save profile or by the keyed account key, because it sits below ZenithSave's consumers and needs no save type, and because its files must outlive the install secret. Offline account names are configuration values a developer chose (`online.offline.accounts`), not a person's account, and the store key reveals nothing the save directory of the same account does not.

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `5A 4F 41 43` ("ZOAC") (1) |
| 4 | 2 | version | 1 (2) |
| 6 | 2 | entry count n | ≤ 1,024 (3) |
| 8 | 4 | CRC-32C of the whole file with this field read as zero | (4) |
| 12 | 4 | reserved | 0 (5) |
| 16 | 72 × n | entries | IDs strictly ascending (6) |

Entry (72 bytes): `uint8` ID length (1 … 64), ID (64 bytes zero-padded, rule 7: the grammar), `uint8` unlocked (0 or 1, rule 8), 2 reserved, `uint32` steps. An entry whose ID the current catalogue lacks is kept on rewrite (a catalogue may regain it). An invalid file is renamed to `achievements.zoac.corrupt` (replacing an older one) and the backend starts with no unlocked achievement for that account, logging `online: offline achievements unreadable (rule <n>); starting empty` (the account is not named).

## Offline cloud store

A directory `user:/saves/online/cloud/<cloud root>/<store key>/` per account, where `<cloud root>` is `online.offline.cloudRoot`, exactly one path component of `[a-z0-9_-]`, 1 … 32 bytes, and not a name PLAT's rule R9 reserves (within the grammar: `con`, `prn`, `aux`, `nul`, `com0` … `com9`, `lpt0` … `lpt9`; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Reserved device names") (so two configured roots simulate two clouds; the whole path passes PLAT's rules R1 … R10, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order") and `<store key>` the offline account's store key (§ "Offline achievement file"). It holds one file per cloud object, named as the object, each a 64-byte header followed by the object's bytes, written whole with `WriteAtomic` so a file and its revision never disagree:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `5A 4F 43 4F` ("ZOCO") (1) |
| 4 | 2 | version | 1 (2) |
| 6 | 2 | reserved | 0 (3) |
| 8 | 8 | store serial | the backend's next serial when written: one more than the largest serial among the account's objects (4: non-zero) |
| 16 | 8 | object bytes B | ≤ `ulONLINE_MAX_CLOUD_OBJECT_BYTES`; the file length is exactly 64 + B (5) |
| 24 | 32 | object SHA-256 | of the B bytes (6: verified on read) |
| 56 | 4 | reserved | 0 (3) |
| 60 | 4 | CRC-32C of bytes 0 … 59 | (7) |

The object's revision (`Zenith_OnlineCloudRevision`) is `Zenith_Sha256Digest` of header bytes 0 … 59, so it changes with every write and is stable across reads. A listing reads the 64-byte header of each file (`Zenith_AsyncIO::Read` of 64 bytes), so its cost is one small read per object. A file failing a rule is listed as absent and logged once; it is never deleted by the backend. The algorithms are `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline cloud store algorithm".

## Format limits

| Format | Limits (a violation is `ONLINE_ERROR_CORRUPT`, tested at the limit and at limit + 1) |
|---|---|
| online catalogue document | the JSON limits of `Zenith2/Docs/Architecture.md` → "Security architecture"; 1,024 achievements; 256 entitlements; IDs 64 bytes; native IDs 128 bytes |
| cooked online catalogue | section body ≤ 24 + 24 × 1,024 + 16 × 256 + 1,048,576 bytes |
| script request payload | 80 bytes |
| result and notification payloads | 256 bytes |
| backend completion payload | CLOUD_LIST ≤ 1,024 objects (`uONLINE_MAX_CLOUD_LIST_OBJECTS`); CLOUD_READ ≤ `online.cloud.maxTransferBytes` |
| cloud object name | 69 bytes (§ "Cloud object names") |
| cloud envelope | 320 + 268,435,456 bytes |
| cloud index | 2,112 + 256 × 128 = 34,880 bytes (a 2,112-byte header with a 128-pair commit log, 64 entries and 64 deletion records); commit log ≤ 128 pairs |
| sync manifest | 16 + 104 × 64 = 6,672 bytes |
| offline achievement file | 16 + 72 × 1,024 bytes |
| offline cloud object | 64 + `ulONLINE_MAX_CLOUD_OBJECT_BYTES` bytes |
| device record | 64 bytes |

These rows join the "Parser limits" table of `Zenith2/Docs/Architecture.md` → "Security architecture" through core-doc item X322 as amended by X358 and superseded for its values by X374 (the rows are taken from this table as it stands when the core patch applies; cross-area row ON12) ([Z2-N-ARCH-953] keeps the table and the constants in one diff).
