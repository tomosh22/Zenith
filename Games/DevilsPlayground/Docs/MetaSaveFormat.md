# DP_MetaSave format (metagame v1)

Sibling to [SaveFormat.md](SaveFormat.md) (the per-run `DP_Save` schema).
`DP_MetaSave` is the **cross-run** meta-progression profile from GDD §5.4:
the Knot balance plus the three hermit unlock tracks. Unlike `DP_Save`
(serialisation-only until the save/load UI lands), the meta state persists
to **disk from day one** via `Zenith_SaveData` under slot name **`meta`**
(`%APPDATA%/Zenith/<GameName>/meta.zsave` on Windows), wrapped in
Zenith_SaveData's own header (magic `ZENS`, format version, game version,
CRC32, payload size, timestamp).

## Payload layout (schema version 1)

All fields little-endian `uint32_t`:

```
[magic 'DPMS' = 0x44504D53]
[schema version = 1]
[knot balance]
[track count = 3]
  [Forge  unlock mask]   (Wynstan's Forge  — crafting)
  [Eye    unlock mask]   (Mereworth's Eye  — perception)
  [Breath unlock mask]   (Old Bett's Breath — movement)
[earned-unspent Knots last run]
```

## Semantics

- **Unlock masks** are 12-node-per-track bitmasks with a **prefix
  invariant**: bit N set implies bits 0..N-1 set (node N requires node
  N-1; `DP_MetaSave::TrySpendUnlock` enforces it at spend time). The
  "unlock level" of a track is therefore simply `popcount(mask)` — no
  separate level field is stored.
- **Forward compatibility**: new nodes claim higher bits, and the track
  count is written explicitly, so an old save loaded into a build with
  more nodes/tracks just shows the additions locked — no migration
  function needed for that growth pattern. Structural changes bump the
  schema version per the DP_Save migration policy.
- **Fail-soft**: bad magic / unsupported version / truncation / an insane
  track count all yield a default (fresh-profile) state; the game never
  hard-fails on a damaged meta save.

## Earning & spending

- `DP_Knots` (Source/DP_Knots.{h,cpp}) owns the per-run tally: 1 Knot per
  reagent inscribed (+ a hand-off-chain bonus; see Tuning.json's
  `metagame` section for every constant). The tally banks into the
  balance exactly once per run on `DP_OnVictory` **or** `DP_OnRunLost`
  (reagents already inscribed count even on a loss) and saves to disk.
- The Liminal scene (build index 2, `DPLiminalHub_Component`) is the only
  spender. Node N costs `node_cost_base + N * node_cost_per_node` Knots.
- Gameplay effect scales (`GetSprintDrainScale` / cooldown / fog-memory)
  are 1.0 on a fresh profile and lerp to their `*_at_full` tuning targets
  at 12/12 nodes — bots and the ratified balance matrix are unaffected
  until nodes are actually bought.

## Tests

`Test_P1MetaSave_RoundTripMeta`, `Test_P1MetaSave_RobustToCorruption`,
`Test_P1MetaSave_VersionMismatchFallsBackToDefault`,
`Test_T0MetaSave_DiskHooks`, `Test_T0Knots_EarningAndChain`,
`Test_T0Knots_RunEndBanking`, `Test_DP_LiminalHub_Spend`.
