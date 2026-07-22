# Zenithmon -- Save Format (ZM_SaveSchema)

The versioned save-schema CONTRACT for Zenithmon. **Normative from S0; binary
schema v1 shipped at S7 item 1 SC2** ([Roadmap.md](Roadmap.md)). Every later
change to persistent state must update this document and the codec together;
the migration gate below is binding from v1 onward.

The v1 module order, field order, widths, encodings and validation rules below
are frozen by `Tests/ZM_Tests_SaveSchema.cpp` plus the independent literal-byte
golden in `Tests/ZM_Tests_SaveMigration.cpp`. They are no longer proposals.

**Current S7 status:** `Games/Zenithmon/Source/Core/ZM_SaveSchema.{h,cpp}` is a
pure game-payload codec for the complete durable `ZM_GameState` inventory; it
names no file, slot, ECS type or scene. Slot identity and disk I/O are owned by
`Source/Save/ZM_SaveSlots.{h,cpp}` (S7 item 2 SC2, ZM-D-138; see "Slot layer"
below). SC3 (ZM-D-139) shipped world-position capture/resume and the
edge-triggered milestone-autosave foundation. SC4 (ZM-D-140) shipped the manual
Save0-2 UI and root Save/Quit without changing one persistent byte. The title
menu and Continue remain SC5; SC6 closes disk-backed restoration and milestone
autosave coverage.

Public API (`ZM_SaveSchema.h`):

```cpp
Zenith_Status Write(const ZM_GameState& xState, Zenith_DataStream& xOutStream);
Zenith_Status Read(Zenith_DataStream& xInStream, uint64_t ulByteLength,
	ZM_GameState& xOutState);
```

## Engine framing (Zenith_SaveData)

Zenithmon persists the inner payload through `Zenith_SaveData`, initialised at boot as
`Zenith_SaveData::Initialise("Zenithmon")` (wired since S0 in `Zenithmon.cpp`)
and driven by `ZM_SaveSlots` since S7 item 2 SC2. The engine wraps every payload
in its own header, and that header is the ENGINE's alone:

- magic `'ZENS'`
- format version (engine)
- game version
- CRC32 over the payload
- payload size
- timestamp

Slot files live at `%APPDATA%/Zenith/Zenithmon/<slot>.zsave`.

| Slot name | Purpose |
|---|---|
| `Save0` / `Save1` / `Save2` | Manual slots -- menu-save anywhere in the overworld |
| `Auto` | Autosave -- written only by milestone triggers, never by the manual flow |

`LoadEx` can report `SUCCESS / FILE_NOT_FOUND / BAD_MAGIC / VERSION_MISMATCH /
CORRUPT_DATA`; the engine layer rejects wrong-magic, bad-CRC and truncated outer
files before the game payload is parsed. Everything below concerns the game
payload inside that wrapper (plus the slot layer's prefix -- see "Slot layer").

Test hygiene: `Zenith_SaveData::ClearForTest` is a between-tests hook from S0
(TestPlan.md C3). It clears only the in-memory write log and readback stash, so
`ZM_SaveSlots::DeleteAllSlotsForTests()` runs immediately before it since SC2.

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

#### Index registry (authoritative: `Source/Data/ZM_StoryFlags.h`)

The wire format above says nothing about WHICH beat each bit means. That
mapping is owned by `enum ZM_STORY_FLAG_ID : u_int` in
`Games/Zenithmon/Source/Data/ZM_StoryFlags.h` (ZM-D-137): **the enum value IS
the bit index written by this module.** A flag index is durable save surface the
moment a save containing it exists, so the registry is governed by two rules.

- **Append only.** New flags are added immediately before
  `ZM_STORY_FLAG_COUNT`. Never reorder, never renumber, and never reuse a
  retired value -- any such reassignment silently changes the meaning of
  existing saves and is therefore a versioned codec change under the migration
  policy below, not an edit to the header. Debug names are not persisted, so
  RENAMING a flag is free while renumbering one is not.
  `ZM_STORY_FLAG_NONE` aliases `ZM_STORY_FLAG_COUNT`, is a sentinel for
  "no flag" in authored data, and is NEVER persisted.
- **Dense from zero, and that density is a STORAGE contract rather than
  tidiness.** Because this module sizes itself from the highest SET index
  (`flagCount` = highest-set-index + 1, then ceil(flagCount/8) bytes), a single
  sparse allocation -- say index 4000 -- would add ~500 bytes to EVERY save this
  game ever writes, in every slot, forever. Those bytes cannot be reclaimed by
  later tidying: shrinking the allocation renumbers indices, which is exactly
  the versioned codec change the rule above forbids doing casually. Reserve a
  future flag by adding a row to the registry, never by leaving a numeric gap.

Appending flags remains a no-migration growth path (this module writes its own
count), which is what makes dense append-only allocation cheap and any other
allocation pattern expensive.

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
not gameplay teleportation. SETTLED at S7 SC3 (ZM-D-139): precedence is
transform-first with spawn-tag fallback on validation failure. `position` is
the capsule CENTRE (spawn markers store FEET, +0.9 half-extent); `yaw` restores
facing as a real contract, written after placement so `EnforceUpright` keeps it.

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

The slot layer surfaces a failed payload as DAMAGED and never auto-overwrites,
repairs or silently resets it; classifying a slot performs no write and no
delete. That policy lives in `ZM_SaveSlots` (see "Slot layer" below); v1 provides
the status and atomic destination semantics that layer consumes.

## Slot layer (`ZM_SaveSlots`) -- settled at S7 item 2 SC2 (ZM-D-138)

`Games/Zenithmon/Source/Save/ZM_SaveSlots.{h,cpp}` is the only code in the game
that names a save file. The directory boundary is the ownership boundary:

| Layer | Owns |
|---|---|
| `Zenith_SaveData` (engine) | The file: `'ZENS'` magic, engine format version, the game-version field, CRC32 over the payload, payload size, timestamp, and the save directory |
| `ZM_SaveSlots` (this layer) | Slot identity/naming and ONE framing field -- the 4-byte length prefix below. It adds nothing else to the payload |
| `ZM_SaveSchema` (codec) | Every byte of the ZMSV payload, exactly as frozen above |

**On-disk inventory.** Four slots, whose ordinals pick their file stems and are
therefore APPEND ONLY -- reordering the enum renames every existing save. The
roles are the table under "Engine framing": `Save0`/`Save1`/`Save2` are the
MANUAL slots and `Auto` is written only by milestone triggers. That split is a
POLICY owned by the caller; to the storage layer `Auto` is an ordinary slot in
every respect. Player-facing copy is "Slot 1".."Slot 3" and "Auto". Under an
automated-test run (or a test's explicit opt-in) every stem gains a `_Test`
suffix, so units never address `Save0.zsave` and friends.

**The 4-byte little-endian length prefix.** Inside the engine's payload region,
this layer writes:

```
[uint32 little-endian ZMSV byte length][ZMSV blob ...]
```

The prefix sits OUTSIDE the frozen v1 payload -- the literal 824-byte golden is
unaffected -- and it carries no magic and no version, because ZMSV's own magic
and schema version sit immediately after it. It exists because `ZM_SaveSchema::
Read` requires an EXACT `ulByteLength` (any slack is rejected as trailing bytes)
while the engine's two load paths disagree about `Zenith_DataStream::
GetCapacity()`: the disk path wraps a stream whose capacity IS the header's
declared payload size, whereas the staged-readback path used by tests hands the
callback a default-constructed OWNING stream whose capacity is its whole
allocation. `GetCapacity()` can therefore never be used as the codec's length,
and branching on `OwnsData()` would couple the game to which engine path is
running. The prefix makes both paths present the codec an identical exact length.
It is written and read byte by byte, so it never inherits host byte order or
struct padding, and it is UNTRUSTED on read: a payload too small to hold it, a
zero length, or a length beyond the bytes actually available is `CORRUPT_DATA`
(defence in depth -- the codec applies an equivalent bound itself).

**Slot status and write semantics.**

- `ProbeSlot` classifies a slot as `EMPTY` (no file), `READY` (file present and
  its ZMSV payload decoded) or `DAMAGED` (file present but the outer file or the
  payload was rejected). Three states, not a bool: collapsing "no file" and
  "unreadable file" is how a New Game silently clobbers a recoverable save. It
  re-reads disk every call, decodes into a scratch state, and never writes.
- Occupancy and readiness are different questions. A DAMAGED slot COUNTS as
  occupied (so a damaged save can never make Continue disappear); only a `READY`
  slot is loadable.
- `WriteState` stages and validates the whole payload BEFORE calling
  `Zenith_SaveData::Save`, because Save creates the file the moment it is called
  -- so a rejected state leaves the slot exactly as it was, with not even a
  zero-length file. It then answers ONLY from a verify RE-PROBE of what landed,
  never from Save's return value. Statuses: `SUCCESS`, `INVALID_ARGUMENT` (bad
  slot id or a state the codec rejects), `OUT_OF_MEMORY` (the payload could not
  be staged), `FILE_NOT_FOUND` (the write claimed success but no file exists) and
  `CORRUPT_DATA` (a file landed but the verify probe did not decode it).
- `ReadState` is transactional end to end: on ANY failure the destination
  `ZM_GameState` is byte-for-byte unchanged, inheriting the codec's guarantee.
  It reports `FILE_NOT_FOUND / BAD_MAGIC / VERSION_MISMATCH / CORRUPT_DATA /
  SUCCESS`, plus `INVALID_ARGUMENT` for a bad slot id.
- The engine header's game-version field is stamped with this layer's own
  constant and is never used as a gate: the real version gate is the ZMSV schema
  version inside the payload, and a second independent version axis would be a
  gate with no owner.

## Player-facing slot policy -- settled at S7 item 2 SC4 (ZM-D-140)

`Games/Zenithmon/Source/UI/ZM_UI_SaveSlots.{h,cpp}` is a by-value non-ECS
presenter on `ZM_UI_MenuStack`, not another persistence layer. One presenter
serves SAVE and LOAD, and every opening re-probes all four disk slots so its rows
show the exact `EMPTY / READY / DAMAGED` classification above.

- **SAVE is manual Save0-2 only.** An EMPTY manual slot writes immediately.
  READY and DAMAGED manual slots require an input-driven Yes/No overwrite
  confirmation. A DAMAGED save remains visibly labelled Damaged; SC4 never
  repairs or deletes it and never overwrites it automatically. `Auto` remains a
  visible, read-only row in the manual flow and is still written only by milestone
  autosave.
- **LOAD is readiness-based, not manual-slot-based.** A READY Save0-2 or Auto row
  resolves to a confirm-load action; EMPTY and DAMAGED rows are not loadable.
  SC4 ships that action seam and proves Auto remains visible/focusable on
  FrontEnd, but intentionally performs no load. The title menu and Continue
  consume the seam in SC5.
- **There is one live SAVE permission predicate.** Root Save and SAVE opening use
  `ZM_SaveSlots::ResolveLiveSaveBlocker`; the irreversible write boundary asks it
  again because the context can change while the screen or overwrite prompt is
  open. The only permitted order is `ResolveLiveSaveBlocker ->
  CaptureWorldPosition -> WriteState`. A blocked boundary returns before capture,
  write, latches or result UI. LOAD is not gated by this overworld-only SAVE
  predicate, because it must work on FrontEnd.
- **Blocked root presentation fails closed.** If a blocker becomes live while
  Save itself is focused, Save is hidden, made unfocusable and removed from both
  navigation directions, and focus is immediately rehomed to Quit. Quit's No/Yes
  flow is separate from slot persistence: No returns to ROOT; Yes invokes the
  SC3 playerless quit-to-FrontEnd transition.

None of this changes the engine wrapper, the slot-layer prefix, the ZMSV payload,
the eleven-module order, the 824-byte v1 artifact, `ZM_GameState` layout or ECS
serialization order.

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

## When saves happen (runtime policy; SC3/SC4 shipped)

- **Manual menu save shipped at S7 SC4 (ZM-D-140).** The pause-menu SAVE screen
  targets Save0-2 only and follows the action/confirmation policy above. Saving
  is disallowed off-overworld, mid-battle, mid-warp and while a whiteout is
  pending; battle state is deliberately not serializable (the battle engine is
  a transient seeded state machine; whiteout/victory always resolves before a
  save can occur). The canonical blocker is checked at screen opening and again
  immediately before `CaptureWorldPosition -> WriteState`.
- **Autosave at milestones**, always to `Auto`, never to a manual slot. The
  edge-triggered latch and first live arrival producer shipped at S7 SC3
  (ZM-D-139). It is gated by the same SC2 blocker policy plus "no menu open", so
  it is refused mid-battle, mid-warp, on a non-overworld scene, or while a menu
  is open -- quit-to-title (a playerless destination) never autosaves. Milestone
  flags are `ZM_IsMilestoneStoryFlag` (`WARDEN_CLEARED / ROUTE1_OPEN /
  GYM1_DEFEATED`); their producers plus the planned badge/story-scene, League and
  tower-bank triggers land only with the gameplay that can emit them. The current
  scene-arrival trigger drains once per frame from `ZM_GameStateManager::OnUpdate`
  while the scene is IDLE and overworld. SC6 still owes the milestone-autosave
  closure test; declaring the trigger vocabulary is not claiming every producer.

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
- Item 1 SC2's observed combined boot gate was **2392 ran / 2391 passed / 0
  failed / 1 skipped**; the engine-only reference remains **1103**. All five
  Zenithmon configurations, headless automation **36/0**, and full windowed
  automation **36/0/0** were green; the automated registry remains 36.
- `Tests/ZM_Tests_SaveSlots.cpp` contains **33** `ZM_Save` tests over the slot
  layer (item 2 SC2): the four distinct file stems, their `_Test` aliases and the
  totality of every name map; manual-vs-Auto classification and display copy;
  empty/ready/damaged probing including a damaged slot left byte-identical across
  a probe and a read; a maximal all-eleven-module round trip; per-slot and
  Auto-vs-manual independence; overwrite-rather-than-append; the recorded payload
  being exactly `[u32 length][ZMSV]` with a little-endian prefix; decode through
  the real staged-readback seam; the four prefix/payload rejection shapes
  (oversized prefix, prefix longer than the payload, a hand-built disk file whose
  payload is too small to hold the prefix, and a staged payload with a corrupt
  inner magic); out-of-range write/read rejection; an invalid state rejected
  WITHOUT creating a file; the verify re-probe (mutation-verified); delete
  semantics; occupancy vs readiness; and all sixteen save-blocker combinations.
  Observed boot gate **2458 ran / 2457 passed / 0 failed / 1 skipped**, engine
  reference **1103** unchanged, headless **36/0**, full windowed **36/0/0**.
- Item 2 SC3 adds **27** pure `ZM_Save` units in
  `Tests/ZM_Tests_ResumePoint.cpp` plus the graphics-gated
  `ZM_ResumePlacement_Test` and `ZM_QuitToFrontEnd_Test`: resume validity,
  capsule-centre position/yaw construction, transform-first/tag-fallback
  placement, the live blocker/autosave policy, real scene-reload placement and
  the two-barrier playerless quit. Observed boot **2485 / 2484 / 0 / 1**;
  headless **38/0** and full windowed **38/0/0**, with the two focused windowed
  tests carrying the graphics-only behavior and the save directory empty
  afterwards.
- Item 2 SC4 adds **23** `ZM_Save` units in
  `Tests/ZM_Tests_SaveSlotScreen.cpp` and **5** `ZM_MenuStack` units: the complete
  SAVE/LOAD action matrix, Auto's manual-read-only/LOAD-visible split, damaged-row
  non-mutation, reprobe/label/name totality, the six-item root resolver and the
  title-load singleton seam. `ZM_SaveMenuFlow_Test` (**98 frames**) locks the real
  immediate write, confirmed overwrite, invalid targets and irreversible-boundary
  blocker; `ZM_RootQuitAndBlockedSave_Test` (**146 frames**) locks Quit No/Yes,
  focused-Save rehome under a live WARP blocker and READY Auto on FrontEnd LOAD.
  Observed gate: regen/build green; boot **2513 ran / 2512 passed / 0 failed / 1
  documented skip**; headless discovery/gate **40/40**; full windowed **40/40
  passed, 0 failed, 0 skipped, 0 zero-frame**; save directory empty; exact-diff
  check green. No commit, push or CI result is claimed yet.
- **Still owed:** SC5 title menu + Continue consumes the READY-slot LOAD seam.
  SC6's `ZM_SaveContinue_Test` must save, quit to FrontEnd, deliberately scramble
  the persistent live state and prove that scramble took, then Continue and assert
  position/party/flags restored exactly from DISK; it also closes the milestone
  autosave test. Without the scramble, `DontDestroyOnLoad` can let a Continue that
  reads zero bytes pass green.
