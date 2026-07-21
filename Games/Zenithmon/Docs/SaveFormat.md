# Zenithmon -- Save Format (ZM_SaveSchema)

The versioned save-schema CONTRACT for Zenithmon. **Normative from S0; binary
schema v1 shipped at S7 item 1 SC2** ([Roadmap.md](Roadmap.md)). Every later
change to persistent state must update this document and the codec together;
the migration gate below is binding from v1 onward.

The v1 module order, field order, widths, encodings and validation rules below
are frozen by `Tests/ZM_Tests_SaveSchema.cpp` plus the independent literal-byte
golden in `Tests/ZM_Tests_SaveMigration.cpp`. They are no longer proposals.

**Current S7 status:** `Games/Zenithmon/Source/Core/ZM_SaveSchema.{h,cpp}` is a
pure game-payload codec for the complete durable `ZM_GameState` inventory. It
does not name files or slots, touch ECS/scenes, or call `Zenith_SaveData`.
Schema v1 therefore completes the first Roadmap item; slot I/O, manual save,
continue and milestone autosave belong to the next S7 item.

Public API (`ZM_SaveSchema.h`):

```cpp
Zenith_Status Write(const ZM_GameState& xState, Zenith_DataStream& xOutStream);
Zenith_Status Read(Zenith_DataStream& xInStream, uint64_t ulByteLength,
	ZM_GameState& xOutState);
```

## Engine framing (Zenith_SaveData)

Zenithmon will persist the inner payload through `Zenith_SaveData`, initialised at boot as
`Zenith_SaveData::Initialise("Zenithmon")` (wired since S0 in
`Zenithmon.cpp`). S7 SC2 does not yet connect the codec to slots. When that
integration lands, the engine wraps every payload in its own header:

- magic `'ZENS'`
- format version (engine)
- game version
- CRC32 over the payload
- payload size
- timestamp

The planned files live at `%APPDATA%/Zenith/Zenithmon/<slot>.zsave`.

| Slot name | Purpose |
|---|---|
| `Save0` / `Save1` / `Save2` | Manual slots -- menu-save anywhere in the overworld |
| `Auto` | Autosave -- written only by milestone triggers, never by the manual flow |

`LoadEx` can report `SUCCESS / FILE_NOT_FOUND / BAD_MAGIC / VERSION_MISMATCH /
CORRUPT_DATA`; once wired, the engine layer rejects wrong-magic, bad-CRC and
truncated outer files before the game payload is parsed. Everything below
concerns only the game payload inside that future wrapper.

Test hygiene: `Zenith_SaveData::ClearForTest` is registered as a
between-tests hook from S0, so save writes in one batched test can never leak
into the next (TestPlan.md convention C3).

### S3 runtime traversal streams are not this save schema

As of ZM-D-056/057, `ZM_GameStateManager`, `ZM_SpawnPoint`, and `ZM_WarpTrigger`
have fixed version-1 **ECS scene-component** streams. They persist authored
component configuration in `.zscen` data: the manager writes only its version,
the spawn point writes its fixed 32-byte tag, and the trigger writes its target
build index plus fixed 32-byte tag. Live transition state (queued/waiting state,
source/destination Player IDs, latch state, issued-load count, fade alpha,
camera-readiness/fade-in phase, and overlay visibility) is never serialized.
Deserializing/reusing the manager resets those fields instead of resuming a
half-transition. The `WarpFade` UIOverlay itself is ordinary scene authoring on
the persistent root, not player progress. The manager's `DontDestroyOnLoad`
lifetime is runtime scene persistence, not durable player-save persistence.
None of this is `ZM_SaveSchema`; durable game-payload schema v1 is the separate
pure codec described below.

## Payload structure

The game payload is an inner header followed by an ordered sequence of
**framed modules**. Each module is independently versioned and length-framed
so a reader can validate or later migrate one module without changing sibling
module boundaries. Schema v1 requires all 11 modules exactly once in the
ordered sequence below; unknown, missing, duplicate or reordered modules are
corrupt data rather than a forward-compatible skip.

Inner header:

| Field | v1 type | Notes |
|---|---|---|
| `magic` | uint32 | `'ZMSV'` -- reads as `0x56534D5A` little-endian. Catches "not a Zenithmon save" before any field reads. |
| `schemaVersion` | uint32 | Exactly 1. Every other value returns `VERSION_MISMATCH`; there is no invented v0 reader. |
| `moduleCount` | uint32 | Exactly 11. |

Each module:

| Field | v1 type | Notes |
|---|---|---|
| `moduleId` | uint32 | Stable enum value from the table below; never reused. |
| `moduleVersion` | uint32 | Exactly 1 for every v1 module. Any other value returns `VERSION_MISMATCH`. |
| `byteLength` | uint32 | Payload bytes that follow. The reader MUST land exactly `byteLength` bytes after parsing; a mismatch is CORRUPT. |
| payload | ... | Module fields, in the documented order. |

All multi-byte integers are encoded explicitly little-endian; the codec does
not depend on the host representation or `Zenith_DataStream`'s templated
operators. Floats are IEEE-754 single-precision bit patterns written as
little-endian uint32 values. Enums use the fixed unsigned widths documented
here, never the compiler-chosen underlying type.

### Stream and transaction contract

- `Write` validates the complete source and serializes into a private owned
  stream before touching the destination. On success it appends one complete
  payload at the destination's current cursor and preserves all prefix bytes.
  An invalid destination/source returns `INVALID_ARGUMENT`; an impossible size
  or insufficient fixed external buffer returns `OUT_OF_MEMORY`. These
  returned failures leave destination bytes and cursor unchanged.
- `Zenith_DataStream::OwnsData()` is the minimal engine seam that distinguishes
  growable owned storage from wrapped fixed-capacity storage. Owned output may
  grow. A wrapped external output is capacity-preflighted before the append, so
  it succeeds only when the whole payload fits and never enters DataStream's
  assert-on-overflow path. Engine allocator exhaustion for an owned stream
  remains the engine's fatal/asserting policy rather than a recoverable codec
  path.
- `Read` parses exactly `ulByteLength` bytes beginning at the input cursor into
  a temporary `ZM_GameState`. On complete success it publishes that state and
  advances the input cursor by exactly the supplied length. Any failure leaves
  both the input cursor and caller's destination state unchanged. Zero length,
  a length beyond remaining capacity, per-module under/over-consumption, or a
  top-level trailing byte returns `CORRUPT_DATA`.
- Bad inner magic returns `BAD_MAGIC`. Unsupported global/module versions, and
  a same-v1 Dex count newer than the current roster, return
  `VERSION_MISMATCH`. All other malformed fields/framing return `CORRUPT_DATA`.
  Source-state validation failures on write return `INVALID_ARGUMENT`.

## Module order (locked)

| # | Module | Contents |
|---|---|---|
| 1 | Party | Up to 6 `ZM_Monster` instances |
| 2 | Boxes | 16 boxes x 30 slots |
| 3 | Dex | Seen + caught bitsets |
| 4 | StoryFlags | Story-flag bitset |
| 5 | Badges | Badge bitmask |
| 6 | Bag | Item id + count entries |
| 7 | Money | Balance |
| 8 | Daycare | Parents, egg, aggregate hatch progress |
| 9 | Tower | Battle Tower current/best streak + procedural seed |
| 10 | WorldPos | Scene build index + spawn tag + transform |
| 11 | Options | Player options |

## ZM_Monster instance record

Shared by Party, Boxes and Daycare. Every record is exactly **61 bytes** in v1.
The in-memory record intentionally has no held-item field and no per-monster
egg-step counter: hatch progress belongs to Daycare.

| Field | v1 type | Notes |
|---|---|---|
| `speciesId` | uint16 | One-based wire id: concrete zero-based `ZM_SPECIES_ID + 1`; 0 and values above the current species table reject. |
| `level` | uint8 | 1..100 |
| `exp` | uint32 | Consistent with level under the species' exp curve (checked on read) |
| `ivs[6]` | uint8 x6 | 0..31 each; HP/Atk/Def/SpA/SpD/Spe order |
| `evs[6]` | uint8 x6 | 0..252 each; total <= 510 (checked on read) |
| `nature` | uint8 | 0..24 |
| `abilitySlot` | uint8 | 0 = species regular ability, 1 = species hidden ability; all other values reject. A caught battle authoring value of `ZM_ABILITY_NONE` normalizes to the regular ability before becoming durable. |
| `status` | uint8 | Major status only (none/sleep/poison/toxic/burn/paralysis/freeze). Volatile statuses are battle-scoped and NEVER saved. |
| `moveIds[4]` | uint16 x4 | 0 = empty; a real move writes its zero-based concrete enum value + 1. Values above `ZM_MOVE_COUNT` reject. |
| `moveCurrentPP[4]` | uint8 x4 | Current PP per slot; independently durable and <= its stored maximum. An empty slot requires `{current,max}={0,0}`. |
| `moveMaxPP[4]` | uint8 x4 | Maximum PP per slot; independently durable rather than reconstructed. A real move with `{current,max}={0,0}` is valid under v1's current<=max invariant. |
| `currentHp` | uint32 | Current instance HP, including 0 for fainted; validated against the record's derived maximum HP. |
| `gender` | uint8 | Concrete durable `ZM_GENDER` value. |
| `friendship` | uint8 | 0..255; a new record defaults to 0. |
| `flags` | uint8 | bit 0 = IS_EGG, bit 1 = IS_SHINY; remaining bits reserved (write 0, reject non-zero) |
| `nickname` | char[16] | Printable ASCII (`0x20..0x7E`), NUL-terminated and zero-padded; empty is valid and displays the species name. |

## Module layouts

### 1. Party

| Field | v1 type | Notes |
|---|---|---|
| `count` | uint8 | 0..6 |
| records | ZM_Monster x count | Slot order preserved |

### 2. Boxes

| Field | v1 type | Notes |
|---|---|---|
| `boxCount` | uint8 | Writes 16. Written explicitly so a future box-count change is data, not layout. |
| `slotsPerBox` | uint8 | Writes 30, same reasoning. The initial reader rejects anything other than 16x30. |
| per slot: `occupied` | uint8 | 0 or 1 |
| per slot: record | ZM_Monster | Present only when occupied |

### 3. Dex

| Field | v1 type | Notes |
|---|---|---|
| `speciesCount` | uint16 | Writer emits the current roster count. Reader accepts the current count or any smaller count and zero-fills later species. A count above current but <=512 returns `VERSION_MISMATCH`; >512 is `CORRUPT_DATA`. |
| `seen` | ceil(speciesCount/8) bytes | Bit N = species id N+1 seen |
| `caught` | ceil(speciesCount/8) bytes | Same indexing; caught implies seen. Unused high bits of each final byte pair must be zero. |

### 4. StoryFlags

| Field | v1 type | Notes |
|---|---|---|
| `flagCount` | uint16 | Writer emits a high-water count: zero when empty, otherwise highest set flag index + 1; maximum 4096. A reader accepts any count <=4096 and zero-fills the unencoded tail. Reordering/removing assigned indices is a versioned codec change. |
| `flags` | ceil(flagCount/8) bytes | Bit `N%8` is flag N; unused high bits in the final byte must be zero. |

### 5. Badges

| Field | v1 type | Notes |
|---|---|---|
| `badgeMask` | uint8 | Bits 0..7 = badges 1..8 |

### 6. Bag

| Field | v1 type | Notes |
|---|---|---|
| `entryCount` | uint16 | |
| entries | { `itemId` uint16, `count` uint16 } x entryCount | Zero-based item id, strictly ascending and unique; id must exist, count is 1..`uZM_BAG_MAX_STACK_COUNT` (999), and zero-count entries are never written. |

### 7. Money

| Field | v1 type | Notes |
|---|---|---|
| `money` | uint32 | The full value is durable. `uZM_MONEY_CAP` (999999) is a gameplay credit ceiling, not a load-time validity cap: an imported value above it is preserved, and `AddMoney` credits nothing until spending brings it below the cap. |

### 8. Daycare

| Field | v1 type | Notes |
|---|---|---|
| `parentCount` | uint8 | 0..2 |
| parents | ZM_Monster x parentCount | |
| `eggPresent` | uint8 | 0 or 1 |
| egg | ZM_Monster | Present only when eggPresent; must have IS_EGG set |
| `eggStepsRemaining` | uint32 | 0 when no egg. This is aggregate Daycare state, never a field on `ZM_Monster`. |

### 9. Tower

| Field | v1 type | Notes |
|---|---|---|
| `currentStreak` | uint32 | Must be <= `bestStreak`. |
| `bestStreak` | uint32 | Retained separately from current streak across losses and new runs. |
| `seed` | uint64 | Full procedural run seed (`ZM_TowerRun::m_ulSeed`); required to reproduce the run. |

### 10. WorldPos

| Field | v1 type | Notes |
|---|---|---|
| `sceneBuildIndex` | uint32 | `uZM_WORLD_SCENE_UNSET` means no resume position yet; otherwise it must be a scene registered in `ZM_WorldSpec` (checked on read). |
| `spawnTag` | char[32] | Empty/NUL-filled with an unset scene. Otherwise uses the `ZM_SpawnPoint` grammar: 1-31 printable ASCII bytes (`0x20..0x7E`), NUL-padded, resolving in the target scene's WorldSpec row. |
| `position` | float x3 | Player world position |
| `yaw` | float | Player facing |

Load-time placement is a one-time spawn (the same rule as warp spawn tags),
not gameplay teleportation. Exact precedence between the saved transform and
the spawn tag when they disagree (e.g. after a terrain re-bake moves ground
height) is TBD at S7 -- the intended rule is transform-first with spawn-tag
fallback on validation failure.

### 11. Options

Exists in the initial module inventory so option additions never shift later
modules. The payload is a counted uint16 TLV sequence:

| Field | v1 type | Notes |
|---|---|---|
| `fieldCount` | uint16 | Number of TLVs that follow. The count must consume the module exactly. |
| per field: `tag` | uint16 | Tag 1 is `textSpeed`; unknown tags are skipped only within their bounded length. |
| per field: `byteLength` | uint16 | Value length. Tag 1 requires exactly 1. |
| per field: value | byte[byteLength] | Tag 1 stores `SLOW / NORMAL / FAST` as uint8. Exactly one tag-1 field is required; duplicate tag 1 and unknown-only payloads reject. New/starter state defaults to `NORMAL`. |

## Sanity caps -- corrupt saves fail LOUDLY, never UB

Every read is bounds-checked BEFORE any allocation or field consumption. A
blob violating any cap is rejected with a logged one-liner naming the module
and field -- never a crash, never a partial apply, never silent clamping.

| Check | Rule |
|---|---|
| Truncation | Every read verifies remaining bytes first; mid-field EOF rejects |
| Module framing | Reader must consume exactly `byteLength` bytes per module; mismatch rejects |
| Party count | <= 6 |
| Box grid | Exactly 16 x 30 in the initial schema |
| Dex species count | <= 512 (comfortably above the ~150 ship count) |
| Story flag count | <= 4096 |
| Bag entries | <= 512; item ids must exist in the item table |
| Monster fields | species/move ids resolve; level 1..100 and EXP matches the growth curve; IVs <= 31; each EV <=252 and total <=510; current PP <= stored max PP (real `{0,0}` valid; empty slots require `{0,0}`); current HP <= derived max HP; ability slot/nature/status/gender valid; friendship <=255; reserved flag bits zero; nickname printable, NUL-terminated and zero-padded |
| Tower | current streak <= best streak |
| WorldPos | all four floats finite; scene/spawn-tag pair resolves, or unset scene has an all-zero tag |
| Options | counted TLVs stay module-bounded; exactly one valid text-speed tag is present |
| Strings | Fixed-size buffers only; no length-prefixed heap reads |

The later slot layer must surface a failed payload as damaged and must not
auto-overwrite or silently reset it. That UI/slot behavior is not implemented
by this pure codec; v1 provides the status and atomic destination semantics the
slot layer will consume.

## Migration policy

- **Today there is one real schema: v1.** The reader accepts global v1 plus
  module v1 and rejects every other global/module version with
  `VERSION_MISMATCH`. There is no fake v0 blob, v0 reader or migration path.
- **Gate rule (binding, from the approved plan):** ANY schema change -- global
  or per-module, field added/removed/widened/reordered -- ships a version bump
  PLUS a canned-blob migration test in the SAME commit. No exceptions; this is
  a merge gate from the moment v1 ships at S7.
- **Canned blobs are compiled C byte arrays** in
  `Tests/ZM_Tests_SaveMigration.cpp` -- never disk assets. The initial
  compatibility artifact is the complete literal **824-byte v1 golden**. One
  test compares every byte emitted by the canonical writer; a second decodes
  the literal, asserts every represented field, and re-encodes the same bytes.
  It is a v1 compatibility/golden pair, not a claim that v0 existed. Future
  versions add one literal blob per historical version and a real migration.
- Module `byteLength` framing means a migration can absorb a dropped trailing
  field without touching sibling modules.
- Versions above current are rejected; no forward-compatible global/module
  read path exists. A future migration chain may introduce a minimum-supported
  floor, but no such machinery is present in v1.
- Documented no-migration growth paths (by construction): appending species
  (Dex writes its count), appending story flags (StoryFlags writes its count),
  appending options (tagged fields), appending bag item IDs (ids validated,
  not positional).

## When saves happen (next S7 slice; not yet wired)

- **Menu-save anywhere** in the overworld (pause menu), to a chosen manual
  slot. Saving is disallowed mid-battle and during scripted cutscene beats --
  battle state is deliberately not serializable (the battle engine is a
  transient seeded state machine; whiteout/victory always resolves before a
  save can occur).
- **Autosave at milestones**, always to `Auto`, never to a manual slot. The
  milestone list is finalized at S7; planned triggers: badge earned, entering
  a new scene after a story beat, League entry, tower streak banked.

## What is NOT saved

- Battle-in-progress state (see above -- saves cannot occur mid-battle).
- `ZM_GameState::m_bPendingWhiteout` -- a transient loss-to-warp latch consumed
  by runtime coordination before saving is allowed.
- NPC/trainer positions and graph state -- respawned from `ZM_WorldSpec` and
  scene data on load; defeat state persists via StoryFlags.
- Grass/render/streaming state -- rebuilt per scene load.
- Anything derivable from the data tables (species stats, move data, ...) --
  tables are compiled code, only instance state is persisted.

## Tests that lock this contract (S7; see TestPlan.md 5.7)

- `Tests/ZM_Tests_SaveSchema.cpp` contains **29** pure `ZM_Save` tests:
  maximal/empty/egg-only round trips; append-write and exact-slice-read
  transactionality; status mapping; every-byte truncation; exact header,
  module order/version/length and trailing-byte framing; monster/Dex/story/
  bag/daycare/tower/world/options field domains; raw move and world-float wire
  oracles; older/current/newer Dex-count policy; StoryFlags high-water output;
  and counted-options TLV compatibility/rejection.
- `Tests/ZM_Tests_SaveMigration.cpp` contains **2** v1 compatibility tests over
  the independent literal **824-byte** array: canonical writer byte equality,
  and literal decode + field assertions + byte-identical re-encode. No fake v0
  migration is represented.
- SC2's observed combined boot gate is **2392 ran / 2391 passed / 0 failed / 1
  skipped**; the engine-only reference remains **1103**. All five Zenithmon
  configurations, headless automation **36/0**, and full windowed automation
  **36/0/0** were green; the automated registry remains 36.
- Still planned for the slot/UI slice: `ZM_SaveContinue_Test` -- save, quit to
  FrontEnd, continue and assert position/party/flags exactly -- plus the
  milestone autosave trigger test.
