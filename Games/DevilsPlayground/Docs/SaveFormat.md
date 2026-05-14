# DP_RunState Save Format

The MVP-1.10 save/load contract for DevilsPlayground. Defines the binary blob layout, the version-management policy, and the fail-soft loader rules.

## Where the code lives

- **`Source/DP_Save.h`** — `DP_RunState` struct, `DP_HeldItemEntry`, `DP_ScentEntry`, schema-version constants, public `Save` / `TryLoad` API.
- **`Source/DP_Save.cpp`** — serialiser + deserialiser implementation.
- **`Tests/Test_P1Save_RoundTripMeta.cpp`** — round-trip equality.
- **`Tests/Test_P1Save_RobustToCorruption.cpp`** — fail-soft on truncated / wrong-magic / empty blobs.
- **`Tests/Test_P1Save_VersionMismatchFallsBackToDefault.cpp`** — fail-soft on future and ancient versions.

## Schema-version contract

Every blob starts with **8 bytes**:

| Offset | Size | Field            | Notes |
|-------:|-----:|------------------|-------|
| 0      | 4    | `magic`          | `0x44505352` = `'DPSR'` little-endian. Catches "not a DP save" before any field reads. |
| 4      | 4    | `schemaVersion`  | `kCURRENT_SCHEMA_VERSION` at write time. Today: `1`. |

The loader reads magic, then version. **Both** must validate before any field reads start.

## Migration policy

Each future schema version SHOULD implement a migration function from N-1 to N. A version's loader walks the version chain forward (V1 → V2 → V3 …) applying each migration in turn until the blob is at the current version, then runs the V-current field reader.

- **Failed migration** at any step falls back to default (`DP_RunState{}`). Logged as a one-liner.
- **Versions below `kMIN_SUPPORTED_SCHEMA_VERSION`** are rejected outright with a one-line log. The MIN floor exists so we can drop support for very old formats without the migration chain growing unbounded.
- **Versions above `kCURRENT_SCHEMA_VERSION`** (a save from a newer build) are rejected with a one-line log. There's no forward-compatible read path.

Today both `kMIN_SUPPORTED` and `kCURRENT` are `1`. When V2 lands, the migration chain (`V1 -> V2`) is the only piece added; the policy here doesn't change.

## V1 blob layout

After the 8-byte header:

| Offset | Size | Field                          |
|-------:|-----:|--------------------------------|
| +0     | 4    | `possessedVillager.index`      |
| +4     | 4    | `possessedVillager.generation` |
| +8     | 4    | `possessedLife`                |
| +12    | 4    | `heldCount`                    |
| +16    | `heldCount * 20` | held entries (20 bytes each: villagerIndex/villagerGen/itemIndex/itemGen/tag) |
| ...    | 4    | `scentCount`                   |
| ...    | `scentCount * 12` | scent entries (12 bytes each: villagerIndex/villagerGen/scent) |
| ...    | 4    | `objectivesMask`               |
| ...    | 4    | `dawnTimerRemaining`           |

All multi-byte values are little-endian (matches `Zenith_DataStream`'s `memcpy`-based `operator<<` on x86/ARM-LE targets). Floats are IEEE 754 single-precision.

### Why `DP_ItemTag` is serialised as `uint32_t`

The enum's underlying type isn't explicitly declared (defaults to `int`, which is platform-defined width). Casting to `uint32_t` for serialisation pins the byte layout regardless of compiler choice, and lets future tag additions stay byte-compatible.

## Sanity caps

- `kMAX_ENTRIES = 1024` — applied to BOTH `heldCount` and `scentCount`. A blob claiming millions of entries is rejected before any allocation. 1024 is comfortably above the 17-villager MVP ceiling.
- Truncation detection — every read checks `HasBytes(stream, neededBytes)` first. A buffer that ends mid-field rejects with `false`; no read past the end of the buffer.

## What's NOT saved

- **Villager state machine** (Idle/Possessed/Fainted/Dead). The Faint state introduced by MVP-1.4 is a transient cooldown; saves typically happen at "safe" moments where no villager is fainted. The TODO for V2 is to round-trip villager states + faint-recovery timers.
- **NavMesh / scene state**. The scene index is implied (only GameLevel = build index 1 is saveable in MVP).
- **Priest blackboard state**. Priest BT re-enters from scratch on load. Acceptable trade-off because priest state recomputes within a few frames from the live perception system.
- **Audio / fog state**. Both are re-derived from villager / lighting state in `OnUpdate`.

These will all get version-bumped fields in V2 as the design intent stabilises.

## Live-state capture and apply (future work)

The MVP-1.10 PR ships the serialisation **data path** only. The "capture live state into a `DP_RunState`" and "apply a `DP_RunState` to live state" functions are gated on:

1. Public setters in `DP_Player` for the held-items table and scent table (currently only writable via `SetHeldItem` and `TryVoluntaryPossessSwitch` -- both have side effects).
2. The UI flow for **Save Game** / **Load Game** menu entries.

Until those land, the round-trip is testable via `Test_P1Save_RoundTripMeta` (manual construction of `DP_RunState`) but no menu hooks expose it to the player.
