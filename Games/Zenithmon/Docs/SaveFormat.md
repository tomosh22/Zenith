# Zenithmon -- Save Format (ZM_SaveSchema)

The versioned save-schema CONTRACT for Zenithmon. **Normative from S0;
implementation lands at S7** ([Roadmap.md](Roadmap.md)). The doc exists before
the code by design: every stage that adds persistent state extends this doc
and the schema in the same PR, and the migration gate rule below applies from
the first shipped version onward.

Module order and field PRESENCE below are locked by the approved plan. Exact
integer widths marked "proposed" are pinned when `ZM_SaveSchema` lands at S7
and are then frozen by the golden-blob tests; anything genuinely undecided is
marked TBD with its stage.

**Current S7 status:** SC1 now supplies the complete durable **in-memory**
inventory in `ZM_GameState`; it does not implement the byte codec. The proposed
wire rows below therefore describe presence and validation intent only. SC1
assigns no schema/module version, final byte offsets, or golden blob, and does
not freeze a proposed integer width.

## Engine framing (Zenith_SaveData)

Zenithmon persists through `Zenith_SaveData`, initialised at boot as
`Zenith_SaveData::Initialise("Zenithmon")` (wired since S0 in
`Zenithmon.cpp`). The engine wraps every payload in its own header:

- magic `'ZENS'`
- format version (engine)
- game version
- CRC32 over the payload
- payload size
- timestamp

Files live at `%APPDATA%/Zenith/Zenithmon/<slot>.zsave`.

| Slot name | Purpose |
|---|---|
| `Save0` / `Save1` / `Save2` | Manual slots -- menu-save anywhere in the overworld |
| `Auto` | Autosave -- written only by milestone triggers, never by the manual flow |

`LoadEx` reports `SUCCESS / FILE_NOT_FOUND / BAD_MAGIC / VERSION_MISMATCH /
CORRUPT_DATA`; the engine layer already rejects wrong-magic, bad-CRC, and
truncated files before the game payload is parsed. Everything below concerns
the game payload inside that wrapper.

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
None of this implements or versions `ZM_SaveSchema`; the first game-save schema
remains S7.

## Payload structure

The game payload is an inner header followed by an ordered sequence of
**framed modules**. Each module is independently versioned and length-framed
so a reader can validate, skip, or migrate one module without understanding
the rest -- the same framing pattern that lets the engine's per-component
serialization absorb schema drift.

Inner header:

| Field | Proposed type | Notes |
|---|---|---|
| `magic` | uint32 | `'ZMSV'` -- reads as `0x56534D5A` little-endian. Catches "not a Zenithmon save" before any field reads. |
| `schemaVersion` | uint32 | Global version assigned when the first byte codec and its golden coverage land; SC1 does not define it. |
| `moduleCount` | uint32 | Number of framed modules that follow. |

Each module:

| Field | Proposed type | Notes |
|---|---|---|
| `moduleId` | uint32 | Stable enum value from the table below; never reused. |
| `moduleVersion` | uint32 | Bumped independently per module. |
| `byteLength` | uint32 | Payload bytes that follow. The reader MUST land exactly `byteLength` bytes after parsing; a mismatch is CORRUPT. |
| payload | ... | Module fields, in the documented order. |

All multi-byte values little-endian (matches `Zenith_DataStream`'s
memcpy-based operators on the x86/ARM-LE targets); floats IEEE 754
single-precision. Enums are serialized as fixed-width unsigned ints, never as
the compiler-chosen underlying type.

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

Shared by Party, Boxes, and Daycare. Field presence mirrors the SC1
`ZM_Monster` durable record; row order and widths remain proposed until the
codec/golden step. The in-memory record intentionally has no held-item field
and no per-monster egg-step counter: hatch progress belongs to Daycare.

| Field | Proposed type | Notes |
|---|---|---|
| `speciesId` | uint16 | Proposed one-based wire id: 1..species count, 0 invalid; the codec maps from the concrete zero-based `ZM_SPECIES_ID`. Validated against `ZM_DataRegistry`. |
| `level` | uint8 | 1..100 |
| `exp` | uint32 | Consistent with level under the species' exp curve (checked on read) |
| `ivs[6]` | uint8 x6 | 0..31 each; HP/Atk/Def/SpA/SpD/Spe order |
| `evs[6]` | uint8 x6 | 0..252 each; total <= 510 (checked on read) |
| `nature` | uint8 | 0..24 |
| `abilitySlot` | uint8 | Proposed wire form, 0 = regular and 1 = hidden, derived from the concrete in-memory `ZM_ABILITY_ID`. A battle authoring value of `ZM_ABILITY_NONE` normalizes to the species' regular ability before becoming durable. |
| `status` | uint8 | Major status only (none/sleep/poison/toxic/burn/paralysis/freeze). Volatile statuses are battle-scoped and NEVER saved. |
| `moveIds[4]` | uint16 x4 | Proposed wire mapping reserves 0 for an empty slot; concrete move ids are validated against the move table. |
| `moveCurrentPP[4]` | uint8 x4 | Current PP per slot; independently durable and <= its stored maximum. |
| `moveMaxPP[4]` | uint8 x4 | Maximum PP per slot; independently durable rather than reconstructed from current PP. |
| `currentHp` | uint32 | Current instance HP, including 0 for fainted; validated against the record's derived maximum HP. |
| `gender` | uint8 | Concrete durable `ZM_GENDER` value. |
| `friendship` | uint8 | 0..255; a new record defaults to 0. |
| `flags` | uint8 | bit 0 = IS_EGG, bit 1 = IS_SHINY; remaining bits reserved (write 0, reject non-zero) |
| `nickname` | char[16] | ASCII, NUL-padded, NUL-termination enforced on read; empty = display the species name |

## Module layouts

### 1. Party

| Field | Proposed type | Notes |
|---|---|---|
| `count` | uint8 | 0..6 |
| records | ZM_Monster x count | Slot order preserved |

### 2. Boxes

| Field | Proposed type | Notes |
|---|---|---|
| `boxCount` | uint8 | Writes 16. Written explicitly so a future box-count change is data, not layout. |
| `slotsPerBox` | uint8 | Writes 30, same reasoning. The initial reader rejects anything other than 16x30. |
| per slot: `occupied` | uint8 | 0 or 1 |
| per slot: record | ZM_Monster | Present only when occupied |

### 3. Dex

| Field | Proposed type | Notes |
|---|---|---|
| `speciesCount` | uint16 | Written explicitly (~150 at ship) so adding species later just grows the bitsets -- no migration for that growth pattern. |
| `seen` | ceil(speciesCount/8) bytes | Bit N = species id N+1 seen |
| `caught` | ceil(speciesCount/8) bytes | Same indexing; caught implies seen (checked on read) |

### 4. StoryFlags

| Field | Proposed type | Notes |
|---|---|---|
| `flagCount` | uint16 | Written explicitly and <= the fixed 4096-index in-memory capacity; appending assigned flags grows the used bitset without reshaping `ZM_GameState`. Reordering/removing assigned indices is a versioned codec change. |
| `flags` | ceil(flagCount/8) bytes | |

### 5. Badges

| Field | Proposed type | Notes |
|---|---|---|
| `badgeMask` | uint8 | Bits 0..7 = badges 1..8 |

### 6. Bag

| Field | Proposed type | Notes |
|---|---|---|
| `entryCount` | uint16 | |
| entries | { `itemId` uint16, `count` uint16 } x entryCount | itemId validated against the item table; count is 1..`uZM_BAG_MAX_STACK_COUNT` (999), and zero-count entries are never written. |

### 7. Money

| Field | Proposed type | Notes |
|---|---|---|
| `money` | uint32 | The full value is durable. `uZM_MONEY_CAP` (999999) is a gameplay credit ceiling, not a load-time validity cap: an imported value above it is preserved, and `AddMoney` credits nothing until spending brings it below the cap. |

### 8. Daycare

| Field | Proposed type | Notes |
|---|---|---|
| `parentCount` | uint8 | 0..2 |
| parents | ZM_Monster x parentCount | |
| `eggPresent` | uint8 | 0 or 1 |
| egg | ZM_Monster | Present only when eggPresent; must have IS_EGG set |
| `eggStepsRemaining` | uint32 | 0 when no egg. This is aggregate Daycare state, never a field on `ZM_Monster`. |

### 9. Tower

| Field | Proposed type | Notes |
|---|---|---|
| `currentStreak` | uint32 | |
| `bestStreak` | uint32 | Retained separately from current streak across losses and new runs. |
| `seed` | uint64 | Full procedural run seed (`ZM_TowerRun::m_ulSeed`); required to reproduce the run. |

### 10. WorldPos

| Field | Proposed type | Notes |
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
modules. Each option is a tagged field so additions are append-only.

| Field | Proposed type | Notes |
|---|---|---|
| `textSpeed` | uint8 | `SLOW / NORMAL / FAST`; new and starter states default to `NORMAL`. |

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
| Monster fields | species/move ids resolve in `ZM_DataRegistry`; level 1..100; IVs <= 31; EV total <= 510; current PP <= stored max PP; current HP <= derived max HP; friendship <= 255; gender valid; reserved flag bits zero; nickname NUL-terminated |
| Strings | Fixed-size buffers only; no length-prefixed heap reads |

A slot that fails to load is surfaced to the UI as damaged and is NOT
auto-overwritten or silently reset -- the player (or test) sees the failure,
and the other slots remain usable. There is no fall-back-to-default for a
progress save.

## Migration policy

- The loader reads the inner magic, then the global `schemaVersion`, then the
  per-module versions. Migrations walk forward one version at a time
  (V1 -> V2 -> ...) until current, then the current reader runs.
- **Gate rule (binding, from the approved plan):** ANY schema change -- global
  or per-module, field added/removed/widened/reordered -- ships a version bump
  PLUS a canned-blob migration test in the SAME commit. No exceptions; this is
  a merge gate from the moment v1 ships at S7.
- **Canned blobs are compiled C byte arrays** in
  `Tests/ZM_Tests_SaveMigration.cpp` -- never disk assets. Baked assets are
  git-ignored and the CI runner has no `Assets/`, so blob-as-code is the only
  form the CI backbone can execute (TestPlan.md conventions C6/C7). One blob
  per historical version, each asserting the migrated result field-by-field.
- Module `byteLength` framing means a migration can absorb a dropped trailing
  field without touching sibling modules.
- Versions above current (a save from a newer build) are rejected -- no
  forward-compatible read path. A `kMIN_SUPPORTED` floor may be introduced
  later to cap the migration chain; until then every shipped version migrates.
- Documented no-migration growth paths (by construction): appending species
  (Dex writes its count), appending story flags (StoryFlags writes its count),
  appending options (tagged fields), appending bag item IDs (ids validated,
  not positional).

## When saves happen

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

- `ZM_Save` round-trip equality: construct a maximal state (full party, eggs,
  part-filled boxes, flags, bag, daycare, streak), save, load, assert
  field-by-field equality.
- Corruption robustness: truncated / bad inner magic / framing mismatch /
  every sanity-cap violation -> rejected with the documented status, process
  healthy afterwards.
- Version mismatch: future-version blob rejected.
- Canned-blob migrations: one per historical version (grows over time).
- Edge cases: empty party (new game), egg-only daycare, all-empty boxes,
  zero-flag save.
- Automated: `ZM_SaveContinue_Test` -- save, quit to FrontEnd, continue,
  assert position/party/flags restore exactly; plus an autosave-trigger test.
