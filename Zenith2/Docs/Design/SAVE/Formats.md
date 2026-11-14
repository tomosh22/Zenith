# SAVE design — formats

This file is part of the SAVE design family. The entry document is `Zenith2/Docs/Design/SAVE.md`. It holds every byte layout SAVE writes or reads: the `ZSAV` save container and its section payloads, the `ZSJN` save journal, the file names under the user mount, the `ASYNC_SOURCE_SAVE` publication payloads, SAVE's use of Architecture's `ZRPL` replay log, and the migration fixture text formats. The types that read and write them are `Zenith2/Docs/Design/SAVE/PublicAPI.md`.

## Scope of this file

- The `ZSAV` file, version 1.0: header, section table, section kinds and every section payload.
- The `ZSJN` journal, version 1.
- The directory and file names of saves, backups, temporaries and journals.
- The payloads SAVE publishes under `ASYNC_SOURCE_SAVE`.
- What SAVE writes into a `ZRPL` log (Architecture owns the record layout) and the restored-world convention.
- The last-profile record.
- The migration fixture formats (text, per the binary-artefact ruling of this programme: fixtures are generated at test time).
- The format limits every reader enforces.

## Conventions for every SAVE format

<!-- elaboration -->
Every layout in this file is this design's own (the plan fixes only that saves are versioned, journaled, integrity-checked and migrated forward); the conventions below apply to all of them.

- Every integer is little-endian and written byte by byte with shifts; no C++ object's bytes are copied into a file (Z2-D-038).
- Floats are written as their IEEE-754 bit patterns (`uint32`, `uint64`).
- Text is UTF-8 without a terminator, preceded by its length; an invalid UTF-8 sequence is `SAVE_ERROR_CORRUPT`.
- Persistent IDs are 16 bytes: `m_ulHigh` then `m_ulLow`, each little-endian (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions").
- Component, event and resource values are ECS binary field-set records (`Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record"), with SAVE's entity reference form of § "Saved entity references".
- Every reader treats its input as untrusted ([Z2-N-ARCH-951]): every count, length and offset is checked against the remaining input with overflow-checked arithmetic before use, and a violation is a `Zenith_SaveError`, never an assert.
- Reserved fields are written as zero; a reader ignores them (so a subsequent minor version may give them meaning) except where this file says a non-zero value is `SAVE_ERROR_CORRUPT`.

## CRC-32C

<!-- elaboration -->
`Zenith_SaveCrc32c` is CRC-32C (Castagnoli): reflected polynomial `0x82F63B78`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`, bytes processed least-significant bit first. The implementation is table-driven slicing-by-8 (eight 256-entry `uint32` tables generated at compile time by a `constexpr` function), with no hardware intrinsic, so every platform computes the same value from portable code.

| Input | CRC-32C |
|---|---|
| empty | `0x00000000` |
| ASCII `123456789` | `0xE3069283` |
| 32 bytes of `0x00` | `0x8A9136AA` |
| 32 bytes of `0xFF` | `0x62A8AB43` |
| bytes `0x00, 0x01, …, 0x1F` | `0x46DD794E` |

These are the RFC 3720 (iSCSI) appendix vectors plus the standard check value; `Test_SaveCrc32c_KnownVectors` asserts them, and `Test_SaveCrc32c_UpdateEqualsWhole` asserts that `Zenith_SaveCrc32cUpdate` over any split of an input equals the whole-input value. The checksum is an integrity check against storage corruption and torn writes, not an authenticity check: saves are user files (`Zenith2/Docs/Architecture.md` → "Security architecture", trust table row "user files").

## The ZSAV file

```text
offset 0            128              128 + 56·n
       ┌────────────┬────────────────┬──────────┬───────────┬───────────────────────────────┐
       │ header     │ section table  │ METADATA │ THUMBNAIL │ per-world sections …          │
       │ 128 bytes  │ n × 56 bytes   │ (stored) │ (optional)│ world 0, then world 1, …      │
       └────────────┴────────────────┴──────────┴───────────┴───────────────────────────────┘
```

### File header

| Offset | Size | Field | Rule (a violation is the error named) |
|---|---|---|---|
| 0 | 4 | magic | bytes `5A 53 41 56` ("ZSAV"); otherwise `SAVE_ERROR_BAD_MAGIC` |
| 4 | 2 | format major | equal to `uSAVE_FORMAT_VERSION_MAJOR` (1); greater: `SAVE_ERROR_VERSION_NEWER`; smaller: `SAVE_ERROR_VERSION_MAJOR` |
| 6 | 2 | format minor | at most `uSAVE_FORMAT_VERSION_MINOR`; greater: `SAVE_ERROR_VERSION_NEWER` |
| 8 | 4 | header bytes | 128; otherwise `SAVE_ERROR_CORRUPT` |
| 12 | 4 | header flags | `uSAVE_HEADER_FLAG_HAS_THUMBNAIL` (bit 0), `_HAS_REPLAY_POSITION` (bit 1), `_COHERENT_ONLY` (bit 2: some participant reported `SAVE_FIDELITY_COHERENT` for this capture, PublicAPI.md "Zenith_SaveParticipant header"); other bits ignored |
| 16 | 1 | save kind | a `Zenith_SaveKind` from 1 to 5 (6 only inside a `ZRPL` CHECKPOINT); otherwise `SAVE_ERROR_CORRUPT` |
| 17 | 1 | ECS backend | a `Zenith_BackendID`; informative |
| 18 | 2 | section count n | 1 … `uSAVE_MAX_SECTIONS` (1,024); otherwise `SAVE_ERROR_CORRUPT` |
| 20 | 4 | reserved | 0 |
| 24 | 8 | sequence | ≥ 1 and strictly increasing per profile across every kind (the autosave rotation and conflict ordering key); exactly 0 for kind 6 (`SAVE_KIND_REPLAY_CHECKPOINT`), which belongs to no profile; any other combination is `SAVE_ERROR_CORRUPT`. An imported file keeps the sequence its own device wrote, and the profile's next sequence is raised above it (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot import") |
| 32 | 8 | UTC seconds | wall-clock time of the snapshot (`Zenith_GetUtcTime()` of `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks", converted to seconds); 0 for kind 6; informative, never part of a determinism comparison |
| 40 | 16 | build ID | `Zenith_GetRunningBuildID()` of the writer |
| 56 | 8 | composition hash | `Zenith_GameComposition::m_ulCompositionHash` |
| 64 | 8 | content hash | the mount manifest's `contentHash` |
| 72 | 8 | tick index | the primary world's tick after which the snapshot was taken |
| 80 | 8 | file bytes | the whole file's length, at most `ulSAVE_MAX_FILE_BYTES` (256 MiB); a different input length is `SAVE_ERROR_TRUNCATED` |
| 88 | 4 | replay world ordinal | the `WORLD_BEGIN` ordinal of [Z2-N-ARCH-789], `0xFFFFFFFF` without a replay position |
| 92 | 4 | reserved | 0 |
| 96 | 8 | replay tick | the tick of that position, 0 without one |
| 104 | 2 | release major | from `Zenith_BuildIdentity::m_strReleaseLine` "MAJOR.MINOR" |
| 106 | 2 | release minor | as above |
| 108 | 4 | section table CRC | CRC-32C of the n × 56 table bytes; mismatch `SAVE_ERROR_CHECKSUM` |
| 112 | 8 | own completion ID | the `ASYNC_SOURCE_SAVE` completion ID of the request that wrote this file (0 for an out-of-tick, resume, exit or replay-checkpoint image); an ordinary restore completes that entry with success (§ "Async section") |
| 120 | 4 | own request serial | that request's serial (`Zenith_SaveResult::m_uRequestSerial`), 0 when the own completion ID is 0; with the header's kind, the METADATA slot name, the file bytes and the tick index it rebuilds the request's own result payload byte for byte (§ "Publication payloads") |
| 124 | 4 | header CRC | CRC-32C of bytes 0 … 123; mismatch `SAVE_ERROR_CHECKSUM` |

The header CRC is checked first, then the magic and versions (a corrupted magic therefore reports `SAVE_ERROR_CHECKSUM` when the CRC also fails, and `SAVE_ERROR_BAD_MAGIC` only for a well-formed non-ZSAV header). A file shorter than 128 bytes is `SAVE_ERROR_TRUNCATED`.

### Section table

n entries of 56 bytes from offset 128, in the order the builder lays the sections out (§ "The ZSAV file" diagram): METADATA, THUMBNAIL when present, then for each world index ascending the world's sections in ascending (kind, key).

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 2 | kind | a `Zenith_SaveSectionKind`; an unknown kind is kept as unknown carry, or refused with `SAVE_ERROR_CORRUPT` when the REQUIRED flag is set |
| 2 | 2 | section version | ≥ 1; the payload's own version (each section kind and each participant counts its own) |
| 4 | 2 | world index | `0xFFFF` for METADATA and THUMBNAIL; otherwise below the METADATA world count |
| 6 | 2 | flags | bit 0 `uSAVE_SECTION_FLAG_REQUIRED`; other bits ignored |
| 8 | 8 | key | the participant key for PARTICIPANT sections; 0 otherwise |
| 16 | 8 | offset | from the file start; at least 128 + 56·n; offset + stored bytes ≤ file bytes |
| 24 | 24 | compression | one `Zenith_CompressedSectionDesc` (`Zenith2/Docs/Design/COMPRESS.md` → "Compressed section descriptor on disk"), decoded with `Zenith_DecodeCompressedSectionDesc` |
| 48 | 4 | stored CRC | CRC-32C of the stored bytes |
| 52 | 4 | decoded CRC | CRC-32C of the decoded bytes |

Further rules: exactly one METADATA section, at index 0; at most one THUMBNAIL, at index 1 when present and only with `HAS_THUMBNAIL`; sections do not overlap and appear in ascending offset; for every world index w below the world count there is exactly one WORLD, one ENTITIES, one EVENTS, one ASYNC and one RESOURCES section; no (kind, world index, key) appears twice. METADATA and THUMBNAIL are always codec NONE so a listing reads them from the prefix without a codec. A section's decoded size is at most 256 MiB, and the sum of decoded sizes at most 1 GiB (`uSAVE_MAX_DECODED_TOTAL_BYTES`), checked before any decode. Every violation is `SAVE_ERROR_CORRUPT` with the section index in `m_ulDetail`.

### Section kinds

| Kind | Value | Scope | Version | Required flag | Codec | Payload |
|---|---|---|---|---|---|---|
| `SAVE_SECTION_METADATA` | 1 | file | 1 | set | NONE | § "Metadata section" |
| `SAVE_SECTION_THUMBNAIL` | 2 | file | 1 | clear | NONE | § "Thumbnail section" |
| `SAVE_SECTION_WORLD` | 3 | world | 1 | set | the save codec | § "World section" |
| `SAVE_SECTION_ENTITIES` | 4 | world | 1 | set | the save codec | § "Entities section" |
| `SAVE_SECTION_EVENTS` | 5 | world | 1 | set | the save codec | § "Events section" |
| `SAVE_SECTION_ASYNC` | 6 | world | 1 | set | the save codec | § "Async section" |
| `SAVE_SECTION_RESOURCES` | 7 | world | 1 | set | the save codec | § "Resources section" |
| `SAVE_SECTION_UNKNOWN_CARRY` | 8 | world | 1 | clear | the save codec | § "Unknown carry section" |
| `SAVE_SECTION_PARTICIPANT` | 16 | world | the participant's | the participant's `uSAVE_PARTICIPANT_FLAG_REQUIRED` | the save codec | § "Participant section framing" |

The save codec is `save.compressionCodec` (`zstd` by default, level `save.compressionLevel`, 3 by default, at most 3 on Android per the COMPRESS memory guidance) with `m_bStoreIfNotSmaller = true`; a section that does not shrink is stored.

## Section payloads

### Metadata section

| Field | Encoding | Rule |
|---|---|---|
| slot name | `uint8` length, bytes | passes `Zenith_ParseSaveSlotName` for the header's kind |
| display name | `uint8` length, bytes | at most 128 bytes |
| playtime seconds | `uint64` | the primary world's playtime ticks (WORLD offset 32) ÷ `sim.tickHz`, rounded down; playtime continues across scene transitions and loads because each new world starts from its predecessor's (or the save's) playtime, while the tick index restarts |
| world count | `uint32` | 1 … 16 |
| pair count | `uint16` | 0 … 32 |
| pairs | `uint8` key length, key bytes, `uint16` value length, value bytes | keys 1 … 64 bytes of `[A-Za-z0-9_.]`, strictly ascending byte order; values at most 256 bytes; a key beginning `zenith.` other than the three engine keys is `SAVE_ERROR_CORRUPT` |

Nothing may follow the last pair (`SAVE_ERROR_CORRUPT`). The game sets at most `uSAVE_MAX_GAME_METADATA_PAIRS` (29) pairs through `Save.SetMetadata` or `Zenith_SaveRequest::m_axMetadata`, none with a key beginning `zenith.` (scripts: ZS5106; C++: `SAVE_ERROR_INVALID_DESC`); the writer adds the three engine pairs `zenith.location` (the base scene's display name), `zenith.release` (the release line text) and `zenith.profile` (the active profile key), so a file holds at most 32 pairs and no game pair can collide with or override an engine one.

### Thumbnail section

`uint16` width (1 … 512), `uint16` height (1 … 288), `uint32` pixel format (1 = RGBA8, sRGB), then width × height × 4 bytes, rows top to bottom. Any other format value or a length mismatch is `SAVE_ERROR_CORRUPT`. SAVE never produces pixels: the game's thumbnail provider (the runtime over RPROD's screenshot readback, absent headless) supplies them with the request, and a save without one simply has no THUMBNAIL section.

### World section

<!-- elaboration -->
96 bytes, fixed:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | world tag at the snapshot |
| 2 | 2 | flags: bit 0 the world is the primary (input) world; bit 1 the world was paused at the snapshot; bit 2 engine autosave disabled by `Save.SetAutosaveEnabled(false)`; other bits 0 |
| 4 | 4 | reserved |
| 8 | 8 | world seed |
| 16 | 8 | tick index t (the state after tick t's end-of-tick playback) |
| 24 | 8 | next simulation ID |
| 32 | 8 | playtime ticks: the world's playtime base (its predecessor's playtime at creation, or the save's at a restore) plus the ticks it ran up to and including t |
| 40 | 16 | base scene persistent ID (the scene the world was created from) |
| 56 | 8 | base scene content hash |
| 64 | 4 | entity count (equal to the ENTITIES section's count) |
| 68 | 4 | event count (equal to the EVENTS section's count) |
| 72 | 4 | participant section count |
| 76 | 1 | ECS backend (`Zenith_BackendID`; informative) |
| 77 | 3 | reserved |
| 80 | 4 | request serial counter (`uint32`, the next save request serial of the world; the width of every serial SAVE publishes) |
| 84 | 4 | resource count (equal to the RESOURCES section's count) |
| 88 | 8 | outstanding autosave completion ID: the engine autosave of the world allocated and not yet published at t, 0 for none (§ "Autosave scheduling" of Algorithms.md: the due rule reads it, so a restore under a publication source decides autosaves exactly as the recorded run) |

A mismatch between a count here and the section it describes is `SAVE_ERROR_CORRUPT`. The next completion ID lives in the ASYNC section alone (one copy, no precedence rule needed).

### Entities section

<!-- elaboration -->
`uint32` entity count E, then E records in ascending simulation ID:

| Field | Encoding |
|---|---|
| simulation ID | `uint64`, ≥ 1, strictly ascending, below the WORLD section's next simulation ID |
| persistent ID | 16 bytes; all zero for a runtime-only entity |
| parent simulation ID | `uint64`; 0 for a root; otherwise the simulation ID of another record of this section |
| component count C | `uint16`, at most 256 |
| flags | `uint16`: bit 0 the record ends with unknown-carry components (see below) |
| component bytes | `uint32`, the byte length of the C records that follow |
| components | C binary field-set records in ascending type ID, each with `bSerializedOnly` capture (transient properties skipped, `PROPERTY_FLAG_SERIALIZED` off); a component is written unless its type has at least one property and every property is transient, so an empty tag component (a type with no properties, `Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record") is a record with no fields and its presence restores |

Entities marked pending destroy at the snapshot point are not written: the snapshot describes the state after tick t's end-of-tick playback, which removes them (`Zenith2/Docs/Architecture/FramePhases.md` → "End-of-tick playback"). Components whose type the running registry lacks were never created at the previous load; they are re-emitted from the world's unknown carry at the end of their entity's record (flag bit 0), still in ascending type ID among themselves, so a component of a type removed in one build survives a load and a save in it and returns in a build that has the type again. A record whose parent names no record is `SAVE_ERROR_CORRUPT`; a parent cycle is refused at restore (`SAVE_ERROR_RESTORE`).

### Saved entity references

<!-- elaboration -->
Every `ENTITY`-typed value inside a field-set record of a save (components, events, participant sections written with `WriteEntityReference`) is 16 bytes in one of three forms, distinguished by bytes 8 … 15:

| Form | Bytes 0 … 7 | Bytes 8 … 15 | Meaning |
|---|---|---|---|
| null | 0 | 0 | the null handle, a stale handle at capture, or a handle to an entity pending destroy at the snapshot point (the uninterrupted run sees that handle go stale at end-of-tick playback, so null is what it observes; not counted as unresolved) |
| save-local | the target's simulation ID, ≠ 0 | 0 | a runtime-only entity (null persistent ID) of the same world |
| persistent | `m_ulHigh` | `m_ulLow` (bit 63 always set) | an entity with a persistent ID |

Generated and derived persistent IDs always set bit 63 of `m_ulLow` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions"; `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"), so the three forms never collide. At restore, a save-local reference resolves through the restored simulation-ID map and a persistent one through `FindByPersistentID`; an unresolvable reference becomes null, is counted in `save.restore.unresolved_references` and is logged once per (type, property). Asset references are the asset's persistent ID through the asset value kind's `m_pfnToPersistentID` and `m_pfnConstructFromPersistentID` hooks (`Zenith2/Docs/Design/ECS/Reflect.md` → "Value-kind hooks"); an asset that no longer exists resolves to the kind's null handle with the same counter and log.

At restore, every record is applied in SPAWNING with entity references deferred (left null, the (entity, type) recorded when any stored reference of the record is not null); LINKING then re-applies the field set of every recorded (entity, type) with full resolution and issues one `SetProperty` for every `ENTITY`-typed top-level property of that type, taken from the type table, and one `SetContainer` for every container property whose element type holds an `ENTITY` value anywhere (the container is applied whole, after the simulation-ID map is complete). Entity references inside container elements are therefore resolved exactly like top-level ones.

### Events section

<!-- elaboration -->
`uint32` event count (at most `ecs.maxEventsPerTick`, the configured value of Z2-B-GAME-003, since the carried events are one tick's emissions), then each carried event (phase 9 and 10 emissions of tick t, which the next tick delivers, [Z2-N-ARCH-478]) in delivery order:

| Field | Encoding |
|---|---|
| event type ID | `uint64` |
| merge key | `uint32` system order, `uint64` simulation ID, `uint32` invocation ordinal, `uint32` sequence (20 bytes) |
| record bytes | `uint32` |
| payload | one binary field-set record of the event's properties |

The merge keys are restored with the events, so the restored world delivers them in phase 9 of tick t + 1 in exactly the order the uninterrupted run delivers them, which the tick hash's event log covers (`Zenith2/Docs/Design/ECS/Scheduling.md` → "World state hash"). An event type the registry lacks is dropped at restore with `save.restore.unknown_events` counted and logged.

### Async section

<!-- elaboration -->
| Field | Encoding |
|---|---|
| next completion ID | `uint64` |
| unpublished count U | `uint32` |
| in-flight count F | `uint32` |
| U unpublished results | `uint64` completion ID; `uint8` source kind; `uint8` publication kind; `uint16` reserved; `uint32` owner module; `uint64` owner simulation ID; `uint32` payload bytes (at most the source kind's declared maximum); payload |
| F in-flight requests | `uint64` completion ID; `uint8` source kind; 7 reserved bytes; `uint32` owner module; `uint64` owner simulation ID |

Both lists are in ascending completion ID. Unpublished results are those completed since tick t's boundary and not yet published, each with its payload exactly as the table held it (a recomputed kind's payload too, zero length or not: SAVE never drops or rebuilds it); in-flight entries are requests issued and not completed. An image written for a request with a completion ID holds that request among its in-flight entries; an out-of-tick, resume, exit-autosave or replay-checkpoint image (own completion ID 0) may hold no `ASYNC_SOURCE_SAVE` entry at all.

At restore the job decides one `Zenith_SaveAsyncDisposition` per entry before any participant runs (state PARTICIPANTS), from the entry, the union of the registered participants' `GetReissuedSourceKinds` (each source kind declared by at most one participant of the world) and `Zenith_WorldRestoreDesc::m_bUnderPublicationSource`, and the hooks apply them (`Zenith_SaveWorldHooks::RestoreAsyncState`):

| Entry | Ordinary restore (a load, a resume) | Restore under a publication source (a seek, a restored replay session) |
|---|---|---|
| unpublished, `PUBLICATION_KIND_GENERIC_RECORDED` | PUT_BACK_COMPLETED with its saved payload; publishes at the boundary of tick t + 1, as in the uninterrupted run | LEAVE_IN_FLIGHT: its only publication is the recorded one the source supplies |
| unpublished, a recomputed kind | PUT_BACK_COMPLETED with its saved payload; any state outside the payload that its publication handler reads (ANIM's parked binding and chunk successes: the asset or block they name) is made resident by the owning participant before `IsRestoreReady` returns true, so the publication at t + 1 finds it exactly as the uninterrupted run did | the same |
| in flight, a source kind a participant declared | LEAVE_IN_FLIGHT; the participant re-issues it in `RestoreSection` under the recorded completion ID and marks it (`Zenith_SaveReissueSet::MarkReissued`); an entry still unmarked when PARTICIPANTS ends is completed with the zero-length payload below and counted in `save.restore.async_failed` | the same; a GENERIC_RECORDED result then comes from the log and the live completion is discarded (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table") |
| in flight, `ASYNC_SOURCE_SAVE`, its ID the header's own completion ID | COMPLETE_AT_RESTORE with the request's own success payload, rebuilt byte for byte from the header (operation WRITE, the header's kind, result 0, flags 0, the own request serial, bytes = the header's file bytes, snapshot tick = the header's tick index, the METADATA slot name), since that write evidently succeeded; it publishes at the boundary of t + 1 (the delivery rule of `Zenith2/Docs/Design/SAVE.md` → "Reproducibility boundaries") | LEAVE_IN_FLIGHT: the recorded publication completes it at its recorded tick |
| in flight, `ASYNC_SOURCE_SAVE`, any other ID (a co-captured request, a pending or deferred one, a load, listing or delete) | COMPLETE_AT_RESTORE with a 64-byte payload: operation and kind 0, result `SAVE_ERROR_CANCELLED`, serial 0, bytes 0, snapshot tick t, empty slot — its write belonged to the previous process or world; publishes at t + 1 | LEAVE_IN_FLIGHT |
| in flight, any other source kind no participant declared | COMPLETE_AT_RESTORE with the zero-length `PUBLICATION_KIND_GENERIC_RECORDED` payload, "abandoned at a restore", which every source kind's decoder maps to its own failure (assumption pending SCRIPT, row SV3); counted in `save.restore.async_failed` | LEAVE_IN_FLIGHT |

Before applying the dispositions the job counts the image's `ASYNC_SOURCE_SAVE` entries (unpublished plus in flight); a count above the restoring world's declared capacity `uSaveMaxOutstanding` (a save written by a release with larger `save.maxQueuedRequests` or `save.maxRequestsPerTick`) fails the restore with `SAVE_ERROR_RESTORE` from `RestoreAsyncState`, `m_ulDetail` = that count, and the log line names both keys (`Zenith2/Docs/Design/SAVE.md` → "Asynchronous source capacity"); an image of this build never exceeds it.

Under a publication source the snapshot's own bookkeeping therefore never publishes a recorded result twice, whether the recorded run had completed it before the checkpoint tick or not. The source kinds of the engine as declared today: `Zenith.Scene.AdditiveSet` declares `ASYNC_SOURCE_SCENE_LOAD` (8) and re-requests each LOADING or QUEUED instance under its recorded ID (§ "Participant section framing"); ANIM's `Zenith.Animation.Instances` declares `ASYNC_SOURCE_ANIMATION` (9) and re-issues its "issued, not completed" bindings and chunk reads under their recorded IDs (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Save state"); every other kind (`ASYNC_SOURCE_WORLD_LOAD` of a transition in flight among them, whose world-host record the restored world does not have) is abandoned at an ordinary restore.

### Resources section

<!-- elaboration -->
One per world. Every world resource of the world (`Zenith_WorldInspector::ListResources`, assumption pending ECS, row SV2 (h)) whose type has at least one serialized property is written; a resource whose properties are all transient (SAVE's own `Zenith.SaveSnapshotBuffer` among them) is not. ENV's `Zenith.Environment.State` (Z2-R-ENV-099), PLAT's `Zenith_SimulationInputContexts` and GAME's `Zenith.Runtime.WorldState` (its `ContinuityTickBase`, cross-area row GA11 (e)) are saved this way, with no participant.

| Field | Encoding |
|---|---|
| resource count R | `uint32`, at most 4,096 |
| per resource, ascending type ID | `uint64` type ID; `uint32` record bytes; one binary field-set record of the resource's serialized properties (`bSerializedOnly` capture through `ReadResource`, containers element by element, entity references in the save's reference form) |

At restore (state RESOURCES, after EVENTS and before PARTICIPANTS, so every participant and the first tick see restored resources): for each record whose type the world has, read the resource's current value (`ReadResource`), apply the record's field set with full reference resolution (migrating it first through `Zenith_MigrateFieldSet` when its version is older), and write the whole value back with `Zenith_CommandBuffer::SetResource` out of tick (row SV2 (i)); transient properties keep the value the resource's owner set at world creation. A record whose type the world lacks is kept in the unknown carry (`Zenith_SaveUnknownResource`), counted in `save.restore.unknown_resources`, logged once, and written again by the next save. A resource of the world that the save lacks keeps its creation value.

### Unknown carry section

Present only when the world's `Zenith_SaveUnknownCarry` holds sections (components ride inside ENTITIES, § "Entities section"):

| Field | Encoding |
|---|---|
| section count | `uint32` |
| sections | the 56-byte section-table entry as it was read (kind, version, world index, flags, key, with the compression descriptor rewritten to codec NONE and both CRCs recomputed over the decoded bytes), then `uint32` decoded bytes, then the decoded bytes |

At load these are put back into the world's carry; at the next save they are written again as their own sections (not nested), so a section of a participant that the running game does not register survives the round trip. A carried section whose kind is now understood (the participant exists again) is handed to it at restore.

### Participant section framing

The decoded bytes of a PARTICIPANT section are exactly what the participant wrote through `Zenith_SaveSectionWriter`; the entry carries its key and version, and SAVE adds no framing. A participant that owns an asynchronous source writes, after its own state, `uint32` in-flight count and per in-flight request its completion ID and the request parameters it needs to re-issue it.

The bridge participants' payloads (each participant's fidelity rule is `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations"):

| Participant (key from the name) | Version | Payload |
|---|---|---|
| `Zenith.Script.States` | 1 | `uint32` count, then per entry of `Zenith_ScriptWorld::GetStateSnapshot()` in its order (source ID, simulation ID): 16-byte entity persistent ID (zero if none), `uint64` simulation ID, `uint64` source ID, `uint8` flags (bit 0 `m_bRunSuspended`, bit 1 `m_bRecoveryPending`, bit 2 `m_bQuarantinedOrDisabled`; bits 3–7 are written 0, and a set one is `SAVE_ERROR_CORRUPT`), 3 reserved bytes, `uint32` blob bytes (0 allowed: an instance with no committed checkpoint, which restores its declared defaults), the ZBST checkpoint blob (`Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility" and SCRIPT's "Checkpoint format"). Restore rebuilds each `Zenith_ScriptSavedState` with its three flags and its blob (an empty `m_xCheckpoint` for 0 bytes) and passes them to `RestoreSavedStates` (`Zenith2/Docs/Design/SCRIPT/HotReload.md` → "Recovery invocations and exact restore" step 6) |
| `Zenith.Scene.AdditiveSet` | 1 | `uint32` next instance ID; `uint32` instance count; per instance in ascending instance ID: `uint32` instance ID, 16-byte scene persistent ID, `uint8` state (ACTIVE or FAILED; LOADING and QUEUED instances are recorded as their pending load: state `LOADING` and re-requested at restore under the completion ID that follows), 3 reserved bytes, `uint64` load completion ID (the instance's in-flight `ASYNC_SOURCE_SCENE_LOAD` ID for a LOADING record, 0 otherwise), `uint64` activation hook tag (0 for none; a re-requested instance carries it, so STREAM's hook sees the reload as its own; the field `Zenith_AdditiveLoadDesc::m_ulHookTag` is STREAM's pending ask of SCENE, assumption fenced in the entry document's "Assumed interfaces from other areas"), `uint8` duplicate flag, `uint8` unknown-type policy, 2 reserved bytes, `uint32` root count, roots as `uint64` simulation IDs |

`Zenith.Scene.AdditiveSet`'s `CollectExcludedEntities` lists the entities of every instance that is still activating (the instance is recorded as `LOADING` and re-requested at restore, so the capture is COHERENT) and of every instance whose unload is in progress (the instance is not recorded: the restored world simply has the unload completed); STREAM excludes nothing of its own (row ST14), so these are the snapshot's only exclusions besides those of module participants.

## The ZSJN journal

<!-- elaboration -->
One journal per slot write, at `<directory>/<slot>.z2journal`, rewritten atomically (PLAT `WriteAtomic`) at each state change; 176 bytes:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | bytes `5A 53 4A 4E` ("ZSJN") |
| 4 | 2 | version | 1 |
| 6 | 1 | state | a `Zenith_SaveJournalState` (1 … 4) |
| 7 | 1 | flags | `uSAVE_JOURNAL_FLAG_*` |
| 8 | 8 | sequence | the new save's header sequence |
| 16 | 1 | slot length | 1 … 32 |
| 17 | 32 | slot bytes | zero-padded |
| 49 | 7 | reserved | 0 |
| 56 | 32 | old digest | SHA-256 of the old save file, all zero when none existed |
| 88 | 32 | new digest | SHA-256 of the new save file |
| 120 | 8 | old bytes | |
| 128 | 8 | new bytes | |
| 136 | 1 | save kind | `Zenith_SaveKind` |
| 137 | 35 | reserved | 0 |
| 172 | 4 | CRC | CRC-32C of bytes 0 … 171 |

Any rule violation, including a slot name that fails `Zenith_ParseSaveSlotName` or a file whose stem differs from the slot name, is `SAVE_ERROR_JOURNAL_CORRUPT`. Digests are `Zenith_Sha256Digest` values (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256"), the digest Contracts' journal table names; the CRC protects the journal itself.

## File names and directories

<!-- elaboration -->
All paths are on the user mount (`user:/`, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots"). Saves never write under `Games/<Name>/` ([Z2-N-CONTRACT-553]).

| Path | Content |
|---|---|
| `saves/<profile>/` | one directory per profile key: `local`, or 16 hex digits of the account (§ "Zenith_SaveTypes header" in PublicAPI.md) |
| `saves/<profile>/<slot>.zsave` | the save |
| `saves/<profile>/<slot>.zsave.bak` | the previous valid save of the slot (one, replaced after each successful write) |
| `saves/<profile>/<slot>.zsave.z2tmp` | the new save during a journaled write (step 3) |
| `saves/<profile>/<slot>.z2journal` | the journal during a write |
| `saves/<profile>/<slot>.zsave.corrupt` | the last save of the slot that failed validation at a load (one per slot, replaced) |
| `saves/<profile>/<slot>.z2journal.conflict`, `.z2journal.corrupt` | a journal recovery could not apply, kept for diagnosis |
| `saves/resume.zsave` | PLAT's resume state (`Zenith2/Docs/Design/PLAT/Android.md` → "Resume-state policy"): no `.bak`, its own journal `saves/resume.z2journal` |
| `saves/last_profile.zslp` | the last-profile record (§ "Last profile record") |
| `saves/online/` and everything below it | reserved to ONLINE (Stage 4): its device record, sync manifests and offline store (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Conventions for every ONLINE format") |

**The `saves/online/` reservation** (A3 row ON22 (a), orchestrator ruling ONLINE-R10-PATHS). `online` is never a profile key: a key is `local` or 16 hexadecimal digits, and `SetActiveProfile`, `BeginProfileSwitch` and the last-profile reader refuse any other. SAVE builds no path below `saves/online/`, and every enumeration it runs is a `ListFiles` of `saves/` (start-up journal recovery of the resume journal) or of `saves/<profile>/` (recovery, the slot table, listings), which lists files directly inside the directory and no subdirectory (`Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveStorage header"). So no SAVE listing, journal recovery, stale-temporary sweep, `.corrupt` handling, slot table, free-space check or delete reads, lists, counts or removes anything below it.

Slot names are lowercase ASCII from `[a-z0-9_-]`, so names are unique on case-insensitive file systems and valid everywhere. PLAT's `WriteAtomic` adds its own `<target>.z2atomic` temporary beneath each write and sweeps those at `Zenith_AsyncIO::Create`; SAVE never touches `.z2atomic` files, and PLAT never touches `.z2tmp` or `.z2journal` files (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes").

On Android the saves directory lives in app-specific internal storage; the Gradle template's `dataExtractionRules` and `fullBackupContent` include `saves/` and exclude the literal paths `saves/resume.zsave`, `saves/last_profile.zslp`, `saves/online/device.zodv` and `saves/online/device.zodv.z2atomic` from device backup and transfer (cross-area rows SV15 (b), ON22 (b), MOD's Android packaging). Android rule paths take no wildcard, so SAVE takes option (1) of cross-area row M53: a restored `*.z2journal`, `*.z2tmp` or `*.corrupt` is present and benign, because start-up journal recovery and the stale-temporary sweep handle the first two (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Journal recovery") and no load or listing reads a `.corrupt` file.

### Last profile record

<!-- elaboration -->
`saves/last_profile.zslp`, 28 bytes, written with `WriteAtomic` by every successful `Zenith_SaveSystem::SetActiveProfile` and read at startup step 11 by `Zenith_ReadLastSaveProfile`, so the save system starts in the profile that was active when the process last ran and a resume file written under an account profile is checked against that profile, not against `local` before sign-in:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `5A 53 4C 50` ("ZSLP") |
| 4 | 2 | version 1 |
| 6 | 1 | key length (5 or 16) |
| 7 | 1 | reserved, 0 |
| 8 | 16 | key bytes (`Zenith_SaveProfileKey`), zero-padded |
| 24 | 4 | CRC-32C of bytes 0 … 23 |

Any violation (length, magic, version, key characters outside `[0-9a-f]` for 16 bytes or not `local` for 5, CRC) makes the reader return `local`; the record is never fatal.

## Publication payloads

<!-- elaboration -->
Every `ASYNC_SOURCE_SAVE` result is published as `PUBLICATION_KIND_GENERIC_RECORDED` (its outcome depends on storage), so a replay publishes the recorded payload at the recorded tick ([Z2-N-ARCH-783]). The payload begins with this 64-byte header:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | operation (`Zenith_SaveOperation`) |
| 1 | 1 | kind (`Zenith_SaveKind`) |
| 2 | 1 | result code (0 or a `Zenith_SaveErrorCode`) |
| 3 | 1 | flags: bit 0 loaded from the backup |
| 4 | 4 | request serial |
| 8 | 8 | bytes written or read |
| 16 | 8 | snapshot tick |
| 24 | 1 | slot length |
| 25 | 32 | slot bytes, zero-padded |
| 57 | 7 | reserved |

`SAVE_OPERATION_LIST` appends `uint16` slot count (at most 64), `uint16` reserved, and per slot, in byte order of slot name, one 120-byte record (the full display name is in the slot list `Zenith_SaveSystem::TakeSlotList` returns to C++):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | slot length |
| 1 | 32 | slot bytes, zero-padded |
| 33 | 1 | kind (`Zenith_SaveKind`) |
| 34 | 1 | state (`Zenith_SaveSlotState`) |
| 35 | 1 | flags: bit 0 has a thumbnail; bit 1 `COHERENT_ONLY` |
| 36 | 2 | format major |
| 38 | 2 | format minor |
| 40 | 8 | sequence |
| 48 | 8 | UTC seconds |
| 56 | 8 | playtime seconds |
| 64 | 8 | file bytes |
| 72 | 1 | display length (at most 45) |
| 73 | 45 | display name bytes, truncated at a UTF-8 boundary to at most 45 bytes, zero-padded |
| 118 | 2 | reserved, 0 |

The largest payload is 64 + 4 + 64 × 120 = 7,748 bytes, below `uSAVE_MAX_PUBLICATION_BYTES` (16,384), the maximum result size the bridge declares for the source kind (`Zenith2/Docs/Design/SAVE.md` → "Asynchronous source capacity").

`SAVE_OPERATION_REFUSED_BATCH` (5) is the one publication of a tick's C++ requests over `save.maxRequestsPerTick` (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Request intake" step 4): header with kind 0, result `SAVE_ERROR_REQUEST_LIMIT`, serial = the first refused request's, bytes = the number refused, snapshot tick = the tick, empty slot; then `uint16` listed count L (at most 4,000), `uint16` reserved, and L `uint32` serials of the refused requests in merge order (the first 4,000 when more were refused; the count in the header field is exact). The largest is 64 + 4 + 16,000 = 16,068 bytes ≤ 16,384. Scripts observe it as one `Zenith.SaveCompleted` with `Operation` 5 and `Bytes` the count (only C++ systems issue such requests).

An ordinary restore rebuilds the own request's success payload from the header (§ "Async section"): operation WRITE, kind, result 0, flags 0, serial = own request serial (header offset 120), bytes = file bytes, snapshot tick = tick index, slot from METADATA — the same 64 bytes the writer completed the request with, because the writer fills exactly those fields from the same values (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Image assembly and compression" step 7).

A zero-length payload is the "abandoned at a restore" publication of § "Async section"; `Zenith_DecodeSaveResult` reads it as the request's operation completing with `SAVE_ERROR_CANCELLED`.

## Replay log usage by SAVE

The record layout of `ZRPL` is Architecture's (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations"); this section states only what SAVE's writer puts in it and the one convention SAVE adds.

### Records SAVE writes

| Record | Written when | By |
|---|---|---|
| file header | `Zenith_ReplayLogWriter::Create` | the writer, from `Zenith_ReplayHeaderInfo`; flags bit 0 set when the run records with `replay.checkpointIntervalTicks` above 0 |
| `REPLAY_RECORD_WORLD_BEGIN` | the startup world's creation (step 16); for a transition's new world, at the swap in prologue step 7, after the old world's `WORLD_SWAP` (cross-area row GA11 (c)) | `BeginWorld` |
| `REPLAY_RECORD_TICK` | at `CommitFrame` after the tick's hash is known | the assembler of the world: flags, tick, input commands (18 bytes each, PLAT's encoding), publications, tick hash |
| `REPLAY_RECORD_WORLD_SWAP` | the swap in prologue step 7 | `SwapWorld` |
| `REPLAY_RECORD_CHECKPOINT` | phase 11 of every tick whose index is a multiple of `replay.checkpointIntervalTicks`, committed right after that tick's `TICK` record (it allocates no completion ID and changes no world state, so recording never shifts the simulation); and right after a restored world's `WORLD_BEGIN` | `AddCheckpoint`: a one-world ZSAV image of kind `SAVE_KIND_REPLAY_CHECKPOINT` (codec NONE, metadata slot `replay`, no thumbnail, header sequence 0 and UTC seconds 0, so the record bytes are a pure function of the world state; `COHERENT_ONLY` set when the capture is COHERENT) |

`CommitFrame` appends, in this order: pending `WORLD_SWAP` and `WORLD_BEGIN` records in prologue order, then the frame's completed `TICK` records (each followed by its `CHECKPOINT` when one was taken) in ascending (world session ordinal, tick index), so the log's bytes do not depend on the order in which worker threads finished worlds.

### Restored-world convention

Architecture's `WORLD_BEGIN` row starts a new world's tick index at 0 and states this convention for a world created from a save (`Zenith2/Docs/Architecture.md` → "Fixed ticks and replayable commands", the `REPLAY_RECORD_WORLD_BEGIN` row; cross-doc item X199, row SV1): such a world (a player load, a resume, a replay seek's restore) does not start at 0, and a replay needs the restored state, not a file the replayer cannot see. SAVE writes such a world as `WORLD_BEGIN` (scene ID and content hash of the save's base scene, the saved seed) immediately followed by a `REPLAY_RECORD_CHECKPOINT` of the same world tag with tick index t, before any `TICK` of that world; the reader marks the session `m_bRestored` with `m_ulFirstTick = t + 1`, and a replay restores the checkpoint instead of loading the scene.

The checkpoint's bytes are a fresh capture, never the source file: after the restore job reached DONE and at the swap (after `SwapWorld` and `BeginWorld`), `Zenith_RuntimeReplay_LogRestoredWorld` captures the restored world out of tick on the main thread into a one-world `SAVE_KIND_REPLAY_CHECKPOINT` image (codec NONE, current format version, sequence 0, UTC seconds 0, metadata slot `replay`, world count 1) and passes it to `AddCheckpoint`. So the record does not depend on the source file's kind, format minor (an older save was migrated in memory), world count, compression, timestamp or profile sequence, and a replayer that never saw the user's file restores exactly the world the recorded run continued from. This section is the SAVE-side elaboration of that row: the reader's `m_bRestored` and `m_ulFirstTick`, the capture point and every field of the checkpoint image.

### Replay limits

| Limit | Value | Violation |
|---|---|---|
| input commands per tick | 4,096 | the writer drops the excess (newest first) and sets `REPLAY_TICK_FLAG_NON_REPLAYABLE`; the reader refuses more with `REPLAY_ERROR_LIMIT` |
| publications per tick | 4,096 | as above |
| one record | 256 MiB (`uREPLAY_MAX_RECORD_BYTES`) | `REPLAY_ERROR_LIMIT` |
| one checkpoint image | min(`uREPLAY_MAX_RECORD_BYTES` − 14, half the writer's commit queue: 32 MiB by default) | the writer skips the checkpoint (`save.replay.checkpoints_skipped`); nothing else changes |
| whole log | `replay.maxLogBytes`, 512 MiB by default | reader: `REPLAY_ERROR_LIMIT` before allocating; writer: recording stops before the limit (`REPLAY_ERROR_SINK_FULL`, the committed prefix kept) |
| simulation-affecting keys | 1,024 | `REPLAY_ERROR_LIMIT` |
| world sessions | 65,535 | `REPLAY_ERROR_LIMIT` |

A record whose header or payload runs past the end of the input ends the log at the record's start offset (an interrupted recording keeps every complete record); a record whose payload is internally inconsistent (counts against its length) is `REPLAY_ERROR_TRUNCATED` at its offset.

## Migration fixture formats

<!-- elaboration -->
Fixtures are text committed under `Zenith2/Engine/ZenithSave/Tests/Fixtures/Migration/Save/`; the binary files a test loads are generated at test time into `${ZENITH_OUT}/test-fixtures/SAVE/` by the fixture-setup case `Test_SaveFixture_GenerateMigrationSet` (suite `z2-fixture-setup`), registered by the test registry as the CTest entry `SAVE_MigrationFixtures` with `FIXTURES_SETUP SaveMigrationFixtures` and `ENVIRONMENT ZENITH2_SAVE_MIGRATION_FIXTURES=${ZENITH_OUT}/test-fixtures/SAVE` ([Z2-N-ARCH-257]); every consuming case is in suite `z2-save-cooked`, one entry `ZenithSave_Tests[z2-save-cooked]` with `FIXTURES_REQUIRED SaveMigrationFixtures`, and `Zenith_Fuzz_SaveImage`'s smoke reads its corpus seeds from the same set. Nothing is committed in binary form; the one exception is § "Frozen fixture writers" (last paragraph).

### State description file

`v<major>_<minor>.state.json`, schema `zenith2.save-fixture` version 1, parsed with CORE's JSON reader under the default limits:

```json
{
	"schema": "zenith2.save-fixture",
	"schemaVersion": 1,
	"saveVersion": [1, 0],
	"kind": "Manual",
	"slot": "fixture-a",
	"header": {
		"sequence": 7,
		"utcSeconds": 1767225600,
		"buildId": "000102030405060708090a0b0c0d0e0f",
		"compositionHash": "0123456789abcdef",
		"contentHash": "fedcba9876543210",
		"release": [1, 0],
		"ownCompletionId": 8,
		"ownRequestSerial": 2
	},
	"metadata": { "displayName": "Fixture A", "playtimeSeconds": 20, "pairs": { "zenith.location": "Hall" } },
	"worlds": [
		{
			"tag": 1, "primary": true, "seed": "00000000000004d2", "tick": 1200,
			"nextSimulationId": 41, "nextCompletionId": 9, "requestSerial": 3,
			"baseScene": "4f1c2a8e-6b0d-4e57-9a3c-1d2e3f405162", "baseSceneContentHash": "1122334455667788",
			"entities": [
				{ "simulationId": 1, "persistentId": "9a1b2c3d-4e5f-4a6b-8c7d-8e9fa0b1c2d3", "parent": 0,
				  "components": { "Test.Counter": { "version": 1, "fields": { "1": 5, "2": "ready" } } } },
				{ "simulationId": 7, "persistentId": null, "parent": 1,
				  "components": { "Test.Link": { "version": 2, "fields": { "1": { "simulationId": 1 } } } } }
			],
			"events": [ { "type": "Test.Ping", "key": [3, 7, 0, 1], "fields": { "1": 2 } } ],
			"async": { "unpublished": [], "inFlight": [ { "id": 7, "source": "Module", "owner": 7, "module": 0 },
			                                            { "id": 8, "source": "Save", "owner": 0, "module": 0 } ] },
			"participants": [ { "name": "Zenith.Test.Counter", "version": 1, "hex": "0500000000000000" } ]
		}
	]
}
```

`header.ownCompletionId` and `header.ownRequestSerial` are the header fields at offsets 112 and 120 (both default 0; a non-zero own completion ID requires an `inFlight` entry of source `Save` with that ID, which `v1_0` has, so the fixture exercises the own-entry rebuild of § "Async section"). An async entry's `source` is its `Zenith_AsyncSourceKind` enumerator without the `ASYNC_SOURCE_` prefix, in PascalCase (`Module`, `Save`, `SceneLoad`, …). `metadata.pairs` is the file's complete pair list, written raw by the fixture generator in ascending key order: engine keys (`zenith.location`, `zenith.release`, `zenith.profile`) are allowed and the builder's engine-pair insertion is bypassed, so the state file controls every pair the METADATA section holds (a `zenith.` key other than the three engine keys is refused by the generator, as the parser would refuse it).

Component fields use the JSON field-set form keyed by decimal property ID (`Zenith2/Docs/Design/ECS/Reflect.md` → "JSON field-set form"); an `ENTITY` value is `null`, `{ "simulationId": n }` or `{ "persistentId": "<uuid>" }`. The test types (`Test.Counter`, `Test.Link`, `Test.Ping`, `Zenith.Test.Counter`) are registered by the fixture suite itself, each with a migration chain whose steps the fixtures exercise.

### Expected output file

`v<major>_<minor>.expected.json`, schema `zenith2.save-fixture-expected` version 1: the restored world dumped canonically after loading, migrating and restoring the generated file with the current build — `saveVersion` (always the current version), per world its counters, every entity by simulation ID with every component at its current version in JSON field-set form, the events in delivery order, the async lists, each participant's restored state as its dump string, and `warnings` (the exact `MIGRATION_WARNING_UNKNOWN_FIELD` and `save.restore.*` diagnostics expected, in order). The test compares the dump byte for byte with canonical JSON writing (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical rules").

### Frozen fixture writers

For the current format version the generator is the production writer (`Zenith_SaveImageBuilder` over a world built from the state file). When a diff bumps the current version from (M, m) to (M, m + 1), it copies the then-current encoding functions into `Zenith2/Engine/ZenithSave/Tests/Frozen/V<M>_<m>/Zenith_SaveFixtureWriter_V<M>_<m>.cpp` (a compiled source of `ZenithSave_Tests`, never under `Tests/Support/`, which outside ZenithCore is header-only, [Z2-N-ARCH-255]; function `Zenith_WriteSaveFixture_V<M>_<m>(const Zenith_JsonDocument&, Zenith_Arena&)`), adds its entry to the writer table in `Zenith2/Engine/ZenithSave/Tests/Frozen/Zenith_SaveFixtureWriters.cpp`, and adds `v<M>_<m+1>.state.json` and its expected file. A frozen `ZRPL` writer of a replay format version n lives the same way in `Zenith2/Engine/ZenithSave/Tests/Frozen/Replay_V<n>/Zenith_ReplayFixtureWriter_V<n>.cpp`. A frozen writer is never edited afterwards: `Test_SaveFixture_FrozenWriterSelfConsistent` decodes each frozen writer's output with the current reader's version-(M, m) path and requires a successful migration to current, and reviewers check frozen files under `reviewPaths`. At each shipped release (Stage 6), a `needs-human` recording ticket also commits the release build's own output for every fixture state under `Tests/Fixtures/Migration/Save/Shipped/<release>/<slot>.zsave` (binary, recorded and checked by a person, the recording-ticket shape of this programme's binary-artefact ruling), which `Test_SaveMigration_ShippedFilesLoad` loads.

## Format limits

| Limit | Value | Where enforced |
|---|---|---|
| save file | 256 MiB | header parse; the builder (`SAVE_ERROR_IMAGE_TOO_LARGE`); PLAT's `io.maxWriteBytes` |
| sections | 1,024 | header parse; builder |
| worlds per save | 16 | METADATA parse; builder |
| one decoded section | 256 MiB | table parse |
| decoded total | 1 GiB | table parse, before any decode |
| entities per world section | 16,777,216 | ENTITIES parse |
| components per entity | 256 | ENTITIES parse |
| field-set record depth | 5 | ECS's decoder ([Z2-N-ARCH-951]) |
| events per world | `ecs.maxEventsPerTick` (Z2-B-GAME-003's configured value) | EVENTS parse |
| async entries per world | 65,536 each list | ASYNC parse |
| resources per world section | 4,096 | RESOURCES parse |
| listing prefix | 128 KiB (`uSAVE_LISTING_PREFIX_BYTES`; it holds the header, a 1,024-entry table and a maximal METADATA section) | listing |
| publication payload | 16 KiB (the largest SAVE writes: 16,068 bytes, a full limit batch) | the bridge's source declaration |
| metadata pairs | 32 per file (29 game + 3 engine); no game key beginning `zenith.` | METADATA parse; builder; intake |
| journal | exactly 176 bytes | journal decode |
| last-profile record | exactly 28 bytes | `Zenith_ReadLastSaveProfile` |

Every limit is tested at the limit (accepted) and at the limit + 1 (refused) ([Z2-N-ARCH-953]; `Zenith2/Docs/Design/SAVE/Tests.md` → "Format and codec tests"). Architecture's parser-limit table carries them (`Zenith2/Docs/Architecture.md` → "Parser limits", the `saves` and `replay logs` rows; cross-doc item X207); this table, with § "Replay limits", is the full list a worker implements.
