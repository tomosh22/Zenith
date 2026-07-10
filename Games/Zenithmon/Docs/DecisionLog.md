# Zenithmon -- Decision Log

**Purpose:** Append-only record of every non-trivial decision made during
Zenithmon development. Future agents grep this when investigating "why was X
done this way?". Scope changes MUST land here as a user decision before any
implementation (see [Scope.md](Scope.md) Section 4).

**Format:** One entry per decision, **newest first**. Fields per entry:
date, id, decision, why, tests-that-lock-it, reversibility. Ids are `ZM-D-NNN`,
assigned in chronological order (so the highest number is at the top of the file).

**What counts as non-trivial:** anything involving trade-offs, anything another
system depends on, any engine-side change, any scope or convention ruling.
Tuning-value changes go in git history, not here.

---

## 2026-07-10 -- ZM-D-029 -- ZM_WorldSpec ships as SCHEMA + an 8-scene proving set; the full world is appended at S9/S10

- **Decision:** the keystone world table (ZM-D-005) lands its SCHEMA plus a small
  proving set, not the full world. `ZM_WorldSpec` (`Source/Data/ZM_WorldSpec.{h,
  cpp}`): one row per scene -- id / name / build index / `ZM_SCENE_KIND` (9 kinds:
  frontend/town/route/interior/gym/battle/tower/league/victory_road) / terrain set
  / warp connections (`ZM_SceneConnection` = target scene + spawn tag) / offered
  spawn tags / encounter table (`ZM_EncounterSlot` = species + level band +
  weight). Per-scene connection/tag/encounter arrays are static, referenced by
  pointer + count. The 8 proving scenes (FrontEnd 0, Battle 1, Dawnmere, Route 1,
  Thornacre, Player's Home, Aster's Lab, Gym 1) exercise every column. Accessors:
  `ZM_GetWorldSpec` / `ZM_GetSceneName` / `ZM_FindSceneByBuildIndex` /
  `ZM_SceneKindToString`. The full ~40-scene world is authored at S9/S10 by
  APPENDING rows (`ZM_SCENE_ID` is save-stable, append-only).
- **Why:** everything from S3 on (warps, encounters, gating, terrain authoring)
  flows through this table, so the schema + a referential-integrity test suite
  must exist first -- it is the enforcer that keeps ~40 scenes honest before any
  are baked. Shipping only the schema + proving set keeps S1 headless-data-only
  while locking the structure S3+ builds against.
- **Tests that lock it:** `Tests/ZM_Tests_WorldSpec.cpp` (category `ZM_Data`, 11
  cases) -- index self-consistency, unique names, unique build indices anchored
  (FrontEnd 0 / Battle 1), valid kinds, terrain-by-kind (outdoor has terrain,
  indoor does not), spawn tags non-empty + unique per scene, **every connection
  resolves to a real target + a spawn tag that target offers**, encounters
  route-only with real species + valid level bands + positive weight, **every
  non-Battle scene reachable from FrontEnd**, build-index round-trip, accessor +
  ToString. The graph was pre-validated offline before building. Boot suite 1163
  ran / 0 failed; baseline bumped 1152 -> 1163.
- **Reversibility:** easy -- additive `Source/Data/` files; the world grows by
  appending rows. Build-index assignments are cheap to change until S3 warps start
  referencing them through this table.

## 2026-07-10 -- ZM-D-028 -- Loop policy: local gate is the quality bar; auto-merge on zm-tests green; do NOT wait on / idle-watch CI

- **Decision (user-directed):** the autonomous loop must not sit blocking on CI.
  The authoritative verification of new behaviour is the LOCAL gate run before
  pushing -- `zenith build Zenithmon` + the boot unit gate
  (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, which runs the ZM_* unit
  tests that `zenith test` skips) + `zenith test Zenithmon --headless`. After
  opening the PR the loop enables GitHub auto-merge
  (`zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`), which lands the
  PR unattended the moment the sole required check `zm-tests` passes; it does NOT
  idle-watch checks, wait for the slower discipline gates (dp-tests/cb-tests/
  engine-gate/...), or launch a blocking `pr checks --watch`. The CI window is
  spent designing/prototyping the next task instead.
- **Why:** the full CI suite is ~25-30 min (dp-tests is the long pole) and was
  re-confirming behaviour the local gate already proves -- pure latency. `zm-tests`
  (~11 min) is the only machine-required check, so auto-merge on it is the fastest
  compliant path. `--admin`/bypass merges are BLOCKED by the harness permission
  classifier (verified 2026-07-10), so "nothing merges red" still holds -- zm-tests
  must actually go green; auto-merge just removes the human/agent wait.
- **Tests that lock it:** none executable; the contract is the updated
  StartPrompts.md (prompt 0 EXECUTION/PR+MERGE, prompts 1/2, Notes) + this entry.
  The local-gate discipline is enforced by the per-PR unit-gate + headless runs.
- **Reversibility:** trivial -- edit StartPrompts.md. Note this supersedes the
  prior "watch checks ... merge once ALL checks are green" wording in prompt 0
  (which implied waiting for the full suite).

## 2026-07-10 -- ZM-D-027 -- ZM_StatCalc (Gen-III+ integer formulas) + ZM_BattleRNG (PCG32) are the sanctioned math + randomness

- **Decision:** two pure-logic modules close most of the S1 formula surface.
  `ZM_StatCalc` (`Source/Data/ZM_StatCalc.{h,cpp}`) is the Gen-III+ integer stat
  formula -- `HP = ((2*base+IV+EV/4)*level)/100 + level + 10`; the other five =
  `(((2*base+IV+EV/4)*level)/100 + 5) * naturePercent/100`, truncating divisions,
  nature multiplier applied last via `ZM_GetNatureStatPercent` (ZM-D-025). No
  floating point. `ZM_BattleRNG` (`Source/Data/ZM_BattleRNG.h`, header-only) is a
  PCG32 generator (64-bit state, 32-bit output): `Next` / unbiased `RandBelow` /
  `RandRange` / `Chance` / `ChancePercent`, deterministic from a seed. It is the
  ONLY sanctioned randomness in game logic (never rand()/std::random).
- **Why:** the battle engine (S2) is seeded and must replay bit-for-bit, so both
  the stat math and the RNG must be integer-exact and reproducible. Landing them
  in S1 (with golden vectors) gives S2 a locked, tested foundation. PCG32 is small,
  fast, statistically strong, and trivially reproducible -- the standard choice for
  a deterministic game RNG. Default-constructed RNGs are fixed-seeded so an
  unseeded instance is never a hidden nondeterminism source.
- **Tests that lock it:** `Tests/ZM_Tests_StatCalc.cpp` (4) -- HP + other-stat
  golden vectors across level/IV/EV/nature, nature dispatch (HP nature-independent;
  raise/lower/unaffected stats), monotonicity. `Tests/ZM_Tests_BattleRNG.cpp` (6)
  -- **a golden 8-value stream pinning the exact PCG32 algorithm**, same-seed
  determinism, distinct-seed divergence, RandBelow/RandRange bounds, and the
  Chance/ChancePercent contract + ~50% frequency. All expected values were
  precomputed in a scratchpad model and matched on the first build. Boot suite
  1152 ran / 0 failed; baseline bumped 1142 -> 1152.
- **Reversibility:** easy -- additive `Source/Data/` files, no other module
  depends on them yet. The PCG32 constants + seeding sequence are load-bearing
  (the golden stream pins them); changing them is a deliberate golden-vector edit.

## 2026-07-10 -- ZM-D-026 -- ZM_AbilityData ships roster + metadata + a declared HOOK-SURFACE bitmask; fn-pointer hook bodies are S2

- **Decision:** the ~50-ability Roadmap sub-box lands as a compiled `const
  ZM_AbilityData` table (50 rows: id / name / description / `m_uHookMask`) plus an
  `ZM_ABILITY_HOOK` enum of 11 hook points as bit flags (SWITCH_IN / MODIFY_STAT /
  MODIFY_DAMAGE_DEALT / MODIFY_DAMAGE_TAKEN / STATUS_TRY / CONTACT / TURN_END /
  FAINT / ACCURACY / WEATHER / TYPE_IMMUNITY). Each ability declares WHICH hooks it
  will implement via the bitmask; the actual per-hook fn-pointer struct + bodies
  are deferred to S2. `ZM_AbilityHasHook(id, hook)` queries the surface.
- **Why:** the plan calls abilities "fn-pointer hook structs", but the hook
  signatures need the battle-state types (`ZM_BattleState`/`ZM_BattleEvent`) that
  do not exist until S2 -- wiring speculative signatures now would only churn
  (repo mandate: no legacy/compat). The bitmask is the non-speculative S1 slice:
  it fixes the roster + names + descriptions + each ability's hook surface (what
  the S2 executor must wire), is fully testable today, and references no
  not-yet-existing types. Mirrors the "data now, executor later" pattern used for
  moves (ZM-D-022) and items (ZM-D-024).
- **Tests that lock it:** `Tests/ZM_Tests_Abilities.cpp` (category `ZM_Data`, 6
  cases) -- index self-consistency (count == 50), unique names, non-empty
  descriptions, masks non-zero with no stray bits, **every hook bit used by >= 1
  ability**, and `ZM_AbilityHasHook` agreeing with the raw mask + name accessor.
  Boot suite 1142 ran / 0 failed.
- **Reversibility:** easy -- additive `Source/Data/` files; `ZM_ABILITY_ID` order
  is append-only. S2 grows the row with the fn-pointer struct (or a parallel hook
  table) keyed by id; the mask stays as the coverage/declaration record.

## 2026-07-10 -- ZM-D-025 -- ZM_NatureData is the exact 25-nature 5x5 grid (real table, not derived)

- **Decision:** the 25 natures land as a compiled `const ZM_NatureData` table
  (id / name / raised stat / lowered stat) that is exactly the 5x5 grid of
  (raised, lowered) pairs over the five non-HP stats (ATTACK / DEFENSE / SPATTACK /
  SPDEFENSE / SPEED); the five diagonal entries (raised == lowered) are the neutral
  natures. `ZM_GetNatureStatPercent(nature, stat)` returns the integer multiplier
  110 / 90 / 100 that `ZM_StatCalc` applies as `(stat * percent) / 100`.
- **Why:** natures are a small, exact, closed set (unlike the derived base-stat /
  learnset placeholders) -- 25 rows, one per stat pairing -- so a real hand-authored
  table is correct and final, not a placeholder. The percent helper keeps the
  x11/10 and x9/10 nature maths integer-exact and in one place for the S1 stat
  formula (box 6).
- **Tests that lock it:** `Tests/ZM_Tests_Natures.cpp` (category `ZM_Data`, 6
  cases) -- index self-consistency, unique names, raised/lowered always non-HP,
  **every (raised, lowered) pair present exactly once + exactly 5 neutral**, the
  110/90/100 percent contract (incl. HP always 100), and the name accessor.
- **Reversibility:** trivial -- additive `Source/Data/` files; names are flavour,
  the pairing is fixed by the mechanic.

## 2026-07-10 -- ZM-D-024 -- ZM_ItemData ships as data + schema only (90 items over a 34-kind effect enum); the bag/battle logic is S2/S5

- **Decision:** the ~80-item Roadmap box lands as a compiled `const ZM_ItemData`
  table (90 items) plus its schema -- `ZM_ITEM_ID` (90, save-stable), a 9-value
  `ZM_ITEM_CATEGORY` (ball / medicine / battle / held / berry / evo / TM / key /
  field), and a 34-kind `ZM_ITEM_EFFECT` executor tag. Each row carries category,
  buy + sell price, effect kind + a kind-specific param, a consumable flag, and
  (for TMs) a taught `ZM_MOVE_ID`. Rows are INERT: the bag/use/held-hook/catch
  logic that interprets `ZM_ITEM_EFFECT` is S2 (held items, catch math) / S5 (bag
  UI), mirroring the MoveData boundary (ZM-D-022). The 25 TMs each reference a
  real move; **this is the TM/tutor learnset seam** the learnset box deferred
  (ZM-D-023). Original names; no Nintendo IP.
- **Why:** items are battle-core scope (Scope.md); catching (S2/S5), the mart
  (S6), held items in battle (S2), and TM teaching all reference this table, so it
  must exist before them. Splitting data from the ~34-arm item executor keeps this
  a reviewable data drop. The effect enum is sized so each future per-effect
  handler has a data subject (a tested coverage invariant).
- **Tests that lock it:** `Tests/ZM_Tests_Items.cpp` (category `ZM_Data`, 11
  cases) -- index self-consistency (count == 90), unique names, valid
  category/effect enums, price sanity (sell <= buy) + key-item contract
  (priceless + effectless), consumable-flag-matches-category, TM-teaches-a-real-
  move (and only TMs teach), ball-only CATCH with multiplier >= 1.0x, stat/type
  param ranges, every effect kind used, every category populated, accessor +
  ToString contracts. The roster was validated offline before building. Boot suite
  1130 ran / 0 failed; zm-tests baseline bumped 1119 -> 1130.
- **Reversibility:** easy -- additive `Source/Data/` files; `ZM_ITEM_ID` order is
  append-only (save-stable). Per-item tuning (prices, effect params) is
  git-history, not decisions.

## 2026-07-10 -- ZM-D-023 -- Species learnsets are systematically DERIVED (placeholder), completing the ZM_SpeciesData box

- **Decision:** per-species level-up learnsets are computed by
  `ZM_GetSpeciesLearnset(id)` (`Source/Data/ZM_Learnsets.{h,cpp}`), not stored:
  it partitions the move table into the species' STAB / secondary-type / universal
  NORMAL damaging buckets + a shared status bucket, sorts the damaging buckets by
  effective power (fixed-damage moves read as high power so they land late),
  teaches a same-type damaging move at level 1, then round-robins damaging moves
  with a capped minority of status moves, spreading levels 1..~50 and sizing the
  list by evolution stage (12 / 14 / 16; single-stage finals 16). Returned by
  value as a fixed-capacity `ZM_Learnset` (max 16). This ticks the `ZM_SpeciesData`
  Roadmap box `[x]` (roster ZM-D-020 + base stats ZM-D-021 + learnsets here).
- **Why:** identical reasoning to base stats (ZM-D-021) -- real movepools are an
  S11 balance concern, and hand-authoring ~150 arbitrary placeholder movepools in
  one commit buys nothing over a deterministic, type-appropriate,
  referentially-valid derivation that unblocks S2 (which builds a monster's
  moveset from species + level). The accessor signature is the stable seam for a
  stored table later. TM/tutor compatibility is deferred to `ZM_ItemData`.
- **Tests that lock it:** `Tests/ZM_Tests_Learnsets.cpp` (category `ZM_Data`, 8
  cases) -- count bounded [4,16] + every entry a real move, level-ordered +
  in-range + something learnable by L5, type-appropriate (own type(s) or NORMAL),
  has a STAB move + an early damaging move, no duplicate moves, status a minority,
  deterministic, and learnset size non-decreasing along an evolution chain. The
  derivation was pre-validated across all 152 species offline before building.
  Boot suite 1119 ran / 0 failed; zm-tests baseline bumped 1111 -> 1119.
- **Reversibility:** easy -- replace the accessor body with a stored per-species
  table; no caller sees a difference. Additive `Source/Data/` files.

## 2026-07-10 -- ZM-D-022 -- ZM_MoveData ships as data + schema only (218 moves over a 57-kind effect enum); the executor is S2

- **Decision:** the ~220-move Roadmap box lands as a compiled `const ZM_MoveData`
  table (218 rows) plus its schema -- `ZM_MOVE_ID` (218 + save-stable
  `ZM_MOVE_COUNT`/`ZM_MOVE_NONE`), `ZM_MOVE_CATEGORY` (physical/special/status),
  `ZM_MOVE_TARGET` (opponent/self/field), and `ZM_MOVE_EFFECT` (57 executor tags).
  Each row carries type, category, power, accuracy, PP, priority, crit stage,
  contact, effect kind + proc chance + a kind-specific magnitude, and target. The
  rows are INERT: no behaviour, no damage/status pipeline -- the single
  `ZM_MoveExecutor` switch that interprets `ZM_MOVE_EFFECT` is deferred to S2
  (ZM-D-010). Original names throughout; the GDD-7.2 cuts (Substitute / Encore /
  Transform / weight moves) have no enum value.
- **Why:** moves are the dependency the species learnsets (the remaining
  `ZM_SpeciesData` sub-box) and the whole S2 battle engine reference, so the table
  must exist before either. Splitting data from the executor mirrors the
  battle-engine boundary already set in ZM-D-010 and keeps this PR a reviewable
  data drop rather than data + a 57-arm interpreter. The effect enum is sized so
  every S2 per-effect scenario (TestPlan 5.2) has a data subject; a tested
  coverage invariant guarantees no effect kind is dead.
- **Tests that lock it:** `Tests/ZM_Tests_Moves.cpp` (category `ZM_Data`, 16
  cases) -- index self-consistency (row i.m_eId == i, count == 218), unique
  non-empty names, valid type/category/target/effect enums, power<->category rule
  (status + fixed-damage powerless; else power in [10,250]), accuracy in [0,100]
  with own-side moves always-hit, PP/priority/crit ranges, effect-chance
  bi-conditional (chance 0 iff effect NONE) + status-moves-always-act, target
  derivable from category+effect, stat-magnitude [1,3], **every effect kind used
  >= 1**, every type has a move, category spread, priority/crit presence,
  accessor + ToString contracts. Boot suite 1111 ran / 0 failed; zm-tests baseline
  bumped 1095 -> 1111 in the same PR.
- **Reversibility:** easy -- additive `Source/Data/` files; the `ZM_MOVE_ID` order
  is append-only (save-stable). Per-move tuning values are git-history, not
  decisions; the struct may gain fields (e.g. contact already present) as S2
  needs them.

## 2026-07-10 -- ZM-D-021 -- Species base stats are systematically DERIVED (placeholder), not hand-tuned

- **Decision:** `ZM_GetSpeciesBaseStats(id)` computes the six base stats from a
  per-archetype stat profile (8 body-plan rows summing ~300) scaled by an
  evolution-stage factor (single-stage finals read as fully evolved) and a rarity
  factor, then a deterministic per-family emphasis/dock drawn from the family seed
  (bump one stat, dock another). Stats are NOT stored per-row and are NOT
  balance-tuned. Added the `ZM_STAT` enum + `ZM_BaseStats` struct.
- **Why:** hand-authoring 152 x 6 balanced stats in one commit is huge and
  error-prone, and balance is explicitly an S11 concern (headless AI-vs-AI). A
  systematic, deterministic derivation unblocks S2 (scripted battles need fixed
  base stats) and differentiates species by archetype AND family. It is trivially
  superseded by a stored per-species table in a later balance pass -- the accessor
  signature is the stable seam.
- **Tests that lock it:** `Tests/ZM_Tests_Species.cpp` `BaseStats_*` (5) --
  in-range [1,255]; totals banded by evolution role (stage-1 base 250-360,
  single-stage >=480, legendary >=560, global 250-700); every stat non-decreasing
  + BST strictly increasing along an evolution chain; archetype shapes (AVIAN
  faster than BLOB, BLOB bulkier than AVIAN, BIPED hits harder than FLOATER);
  family variety (>=60 distinct stat blocks). Boot suite 1095 ran / 0 failed.
- **Reversibility:** easy -- replace the accessor body with a stored table; no
  caller sees a difference.

## 2026-07-10 -- ZM-D-020 -- ZM_SpeciesData decomposed: structural roster first (152 species), base stats + learnsets deferred

- **Decision:** the ~150-species SpeciesData Roadmap box is split across
  increments on the same box. Increment 1 (this PR): the `ZM_SpeciesData` schema
  + supporting enums (`ZM_ARCHETYPE`/`ZM_RARITY`/`ZM_SIZE_CLASS`/`ZM_SPECIES_ID`)
  + the full 152-species STRUCTURAL roster (id / name / type(s) / archetype /
  evo-stage / evolves-to / family / rarity) transcribed from GDD section 5, with
  size class + family seed as rule-derived accessors. DEFERRED to later
  increments on this box: per-species base stats (a design pass) and learnsets
  (need `ZM_MoveData`, box 3). The Roadmap box stays a WIP (`[~]`).
- **Why:** 152 species with hand-designed base stats + every field in one commit
  is a large, error-prone, hard-to-review PR; a structural-roster-first split is
  standard practice for big data tables and is dependency-correct (learnsets
  reference moves that do not exist yet). The roster is the foundation that
  MoveData learnsets, WorldSpec encounters, DataRegistry, and S4 asset gen all
  reference.
- **Tests that lock it:** `Tests/ZM_Tests_Species.cpp` (category `ZM_Data`) -- 11
  integrity tests: count==152 + index self-consistency, unique names, valid
  types, evolution-graph shape (stage+1, same family/archetype/rarity, no
  self-loop), families well-formed (linear chains, one species per stage),
  family-size distribution (40/13/6 vs GDD), base/final/legendary counts,
  archetype-family spread (18/6/7/4/6/7/5/6), every-type-on-two-families,
  family-seed consistency+uniqueness, size-class monotonicity. Boot suite 1090
  ran / 0 failed.
- **Reversibility:** easy -- additive `Source/Data/` files; the struct grows
  (base-stats + learnset fields) in follow-up increments; the `ZM_SPECIES_ID`
  order is save-stable (append-only).

## 2026-07-10 -- ZM-D-019 -- Zenithmon boot unit tests are gated in CI via a run_unit_gate.ps1 boot step (ratcheted baseline)

- **Decision:** `zm-tests.yml` gains a step that boots `zenithmon.exe` headless
  through the shared `Tools/run_unit_gate.ps1` (`-Baseline 1079`) to run the boot
  ZENITH_TEST suite (engine units + Zenithmon `ZM_*` cases) and fail on any
  failure. The baseline is an exact-count ratchet (like engine-gate): each PR that
  changes the `ZM_*` unit count -- or an engine PR that changes the engine unit
  count -- bumps the number in the same PR.
- **Why:** discovered while landing S1 -- both `zenith test` (harness default) and
  the two prior zm-tests steps pass `--skip-unit-tests`, so Zenithmon's unit tests
  (the S1/S2 gate backbone, ~460 cases at end state) NEVER ran in CI. The plan
  designates the boot unit suite as the CI backbone; DP/CB never hit this because
  they carry almost no game-side unit tests. `run_unit_gate.ps1` is the proven
  engine-gate pattern (tool-exports ON so asset-export units work; watchdog-kills
  the known tools-build idle after the units line is logged).
- **Tests that lock it:** the step itself (red on any unit failure or count !=
  baseline); validated locally = "1079 ran, 1078 passed, 0 failed, 1 skipped" (the
  1 skip is the pre-existing quarantined `GraphComponent::RegistryWideNodeRoundTrip`).
- **Reversibility:** easy -- delete the step. The baseline's coupling to the
  engine unit count is the known maintenance cost (CIPolicy.md section 1); a
  follow-up may switch to a failures-only check if the ratchet churns
  (Questions.md Q-2026-07-10-004).

## 2026-07-10 -- ZM-D-018 -- Type system: save-stable ZM_TYPE enum + golden-locked 18x18 chart with a dual-type product API

- **Decision:** the 18 types are one `enum ZM_TYPE : u_int` (`Source/Data/ZM_Types.h`)
  in the GDD-section-6 order, which is simultaneously the dex/UI order and the
  row/column order of the chart; the range is append-only (save-stable). The
  effectiveness matrix is a `const` 18x18 float table (`ZM_TypeChart.cpp`) of
  {0, 0.5, 1, 2}, mapping the standard 18-type relationships onto the original
  names. Lookups are a stateless namespace `ZM_TypeChart`:
  `GetEffectiveness(atk, def)` + `GetDualTypeEffectiveness(atk, def1, def2)`, where
  `def2 == ZM_TYPE_NONE` (== `ZM_TYPE_COUNT`) collapses to the single lookup and a
  duplicated slot is never squared.
- **Why:** types are consumed by species/moves/damage from S1 on; a save-stable
  enum + a compiled table keeps zero file I/O in headless tests (ZM-D-009) and
  makes the chart diffable. The dual-type product belongs with the chart (4x /
  0.25x / 0x matchups) and is testable before species exist.
- **Tests that lock it:** `Tests/ZM_Tests_Data.cpp` -- `TypeChart_MatchesGolden`
  (an independent golden 18x18 compiled into the TU: the two-place-change lock,
  TestPlan 5.1), `TypeChart_AllCellsLegal`, `TypeChart_ImmunityCountIsEight`, the
  GDD design-intent spot checks (`StarterTriangle`, `SecondTriangleAndGhostNormal`,
  `DrakeChecks`, `ImmunitiesAndIronWall`), `TypeChart_DualTypeProducts`,
  `Types_ToStringContract`.
- **Reversibility:** easy -- additive `Source/Data/` files, no engine change;
  reordering/renaming types is a save-migration concern only after content ships.

## 2026-07-10 -- ZM-D-017 -- Docs/ becomes a self-sufficient autonomy hub: MasterPlan committed, lifecycle-loop prompt, hard-stop visual gates, permission allowlist

- **Decision (user-directed):** the Docs directory must carry the whole
  project lifecycle with the only human inputs being (a) pasting/looping a
  StartPrompts.md prompt and (b) visual-gate sign-offs. Changes: the approved
  program plan is committed as MasterPlan.md (it previously lived only in a
  machine-local `~/.claude/plans/` file) and referenced from every start
  prompt; StartPrompts.md gains prompt 0 (idempotent lifecycle-loop iteration,
  carries the user's standing merge-on-green authorization for the loop's own
  PRs) and prompt 4 (gate sign-off); `Tools/zenith_gh.ps1` wraps gh with
  self-bootstrapping auth; a checked-in `.claude/settings.json` allowlists the
  loop's build/test/git/gh commands (exact rules user-approved).
- **Gate policy (user's explicit choice):** the loop HARD-STOPS at every
  stage's visual check (incl. S4 gallery, S8 go/no-go) -- automated gate items
  run, screenshot evidence is captured, Status.md gets a `GATE-WAIT: S<n>`
  marker, and nothing proceeds until the user's prompt-4 sign-off lands in
  this log. The loop never signs its own gates.
- **Why:** S0 proved the failure modes: the plan file was unversioned and
  machine-local; gh had no session auth; permission prompts and the
  self-merge guard stall unattended runs; `gh run rerun` cannot re-evaluate
  against new master.
- **Tests that lock it:** none executable; the contract is the prompts +
  allowlist themselves (version-controlled) and this entry.
- **Reversibility:** trivial -- edit StartPrompts.md / delete the allowlist;
  gate policy can be relaxed by a new user decision here.

## 2026-07-10 -- ZM-D-016 -- Master branch protection CREATED with `zm-tests` as the sole machine-enforced required check

- **Decision:** master had NO branch protection and no rulesets at all (the
  repo's "required checks" had been purely conventional). On the user's
  direction ("Add zm-tests yourself"), classic branch protection was created
  via the API: required status checks `[zm-tests]`, `strict=false`,
  `enforce_admins=false`, no required reviews.
- **Why:** the S0 gate requires zm-tests to actually block merges;
  `enforce_admins=false` preserves the owner's established direct-push
  workflow (agents always land via PRs, so agents are always gated). Other
  gates stay blocking-by-discipline because several are path-filtered and a
  required check that never reports deadlocks a PR.
- **Tests that lock it:** none (GitHub configuration); verified by
  `gh api repos/tomosh22/Zenith/branches/master/protection`.
- **Reversibility:** trivial (delete/edit the protection rule); recorded in
  CIPolicy.md section 4 + ManualSetupChecklist.md.

## 2026-07-10 -- ZM-D-015 -- Three pre-existing master-red CI gates fixed as a prerequisite PR rather than inherited red

- **Decision:** engine-gate, layering-gate, and scaffold-smoke had been red on
  master since 2026-07-07/08 (before Zenithmon existed). Rather than merging
  S0 with inherited red checks, they were fixed in a dedicated PR (#144,
  `0844689e`): unit baseline 1053->1068 single-sourced in
  `Tools/run_unit_gate.ps1` (test_scaffold.ps1 reuses it), `Flux_HDR.cpp`
  g_xEngine reaches reduced via the established local-hoist idiom (fixed, not
  allow-listed), and regen.ps1 given a dotnet-exec fallback on the tracked
  Sharpmake dll (+ scaffold-smoke got `lfs: true` and the standard
  `/p:WindowsTargetPlatformVersion=10.0` build override).
- **Why:** "nothing merges red" is only meaningful if master itself can go
  green; every future Zenithmon stage PR needs a green baseline.
- **Tests that lock it:** the gates themselves (all 9 checks green on #144;
  all 10 green on the rebased #143); scaffold smoke 11/0 locally.
- **Reversibility:** each fix is independent and small; the baseline bump is
  a ratchet (future engine-test additions bump it again in ONE place).

## 2026-07-09 -- ZM-D-014 -- Engine name-validation narrowed to a PascalCase word boundary so 'Zenithmon' is a legal game name

- **Decision:** `zenith new Zenithmon` was rejected by the blanket
  `Zenith*`/`Sentinel*` reserved-prefix rule in BOTH game-name validators: PS
  `Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` and C++
  `ZenithHub_GameScan::ValidateName`. Both were narrowed to a PascalCase word
  boundary: reject `Zenith`/`Sentinel` alone or followed by an uppercase letter
  or digit; a lowercase continuation (e.g. `Zenithmon`) is a distinct word and
  valid.
- **Why:** the reservation exists to protect engine/test module names
  (`ZenithECS`, `SentinelAI`, ...); `Zenithmon` collides with none of them --
  the blanket rule was broader than its intent.
- **Tests that lock it:** `Build/Tests/run_buildsystem_tests.ps1` (suite 45
  passed / 0 failed) + the shared pinned vectors in
  `Tools/ZenithCli/Tests/name_validation_cases.txt` (consumed by both
  validators) + the ZenithHub selftest.
- **Reversibility:** reverting the validators would orphan this project --
  reversible only by renaming the game.

## 2026-07-09 -- ZM-D-013 -- No per-game runner script; the unified `zenith test` harness is the only test runner

- **Decision:** Zenithmon never gets a `run_zm_tests.ps1`. All test execution --
  local, stage gates, and CI (`.github/workflows/zm-tests.yml`) -- goes through
  `zenith test Zenithmon` (`Tools/ZenithCli/ZenithCli.psm1` ->
  `ZenithTestHarness.psm1`; flags `--filter/--headless/--results-dir/--config/
  --per-process/--fail-fast`; exit codes 0 OK / 1 usage / 2 validation /
  3 generation / 4 build-or-test / 5 not-found).
- **Why:** the old per-game `Tools/run_*_tests.ps1` scripts were DELETED at
  commit `c29e28f8` in favor of the unified harness; a per-game script would be
  legacy surface on day one (repo mandate: no legacy/compat code).
- **Tests that lock it:** the `zm-tests` CI workflow invokes
  `zenith.bat test Zenithmon --headless` directly; every stage gate in
  [Roadmap.md](Roadmap.md) cites the same command.
- **Reversibility:** none needed -- a per-game script would be a policy
  violation, not an option.

## 2026-07-09 -- ZM-D-012 -- Scene build index 0 = FrontEnd.zscen, boot-authored title screen (DP convention)

- **Decision:** the game boots into `FrontEnd.zscen` at build index 0: camera +
  "Zenithmon" title text + the game component. The scene is boot-authored by
  tools builds (editor-automation steps re-author it every tools boot) and the
  baked `.zscen` is git-ignored. The build-index table follows the plan:
  0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road, 40+ interiors,
  95 Tower (exact per-scene assignments TBD at S9/S10 via ZM_WorldSpec).
- **Why:** matches the proven DevilsPlayground convention (index 0 = FrontEnd,
  boot-authored, reloaded between batched tests by the harness).
- **Tests that lock it:** `Tests/ZM_AutoTests_Boot.cpp` (`ZM_Boot_Test`) + the 2
  boot unit tests in `Tests/ZM_Tests_Boot.cpp`.
- **Reversibility:** index remaps are cheap until S3, when warps start
  referencing build indices through ZM_WorldSpec.

## 2026-07-09 -- ZM-D-011 -- S0 keeps the scaffold placeholder ZM_GameComponent (bobbing cube)

- **Decision:** `ZM_GameComponent` (registered `"ZM_Game"`, serialization
  order 100) retains the `zenith new` scaffold's bobbing-cube behaviour as the
  S0 placeholder until the S1 data core and S3 overworld systems land.
- **Why:** S0 is skeleton/harness/CI/docs only; a live registered component
  proves the registration + serialization + between-tests plumbing without
  inventing gameplay ahead of its stage.
- **Tests that lock it:** `Tests/ZM_Tests_Boot.cpp` (2 unit tests) +
  `ZM_Boot_Test` -- these pin boot health, not the cube; the placeholder is
  free to be replaced.
- **Reversibility:** trivial -- it is a placeholder by design.

## 2026-07-08 -- ZM-D-010 -- Battle engine is headless C++ (not Behaviour Graphs) with an append-only event stream

- **Decision:** the battle turn loop is a seeded, deterministic C++ state
  machine (`ZM_BattleEngine`): `Begin(config,seed)` -> `SubmitAction` ->
  `ResolveTurn()` -> append-only `ZM_BattleEvent` stream, the single source of
  truth for both tests and presentation; the engine never formats strings or
  touches UI. Behaviour graphs are glue only (menu flow, NPC events, cutscene
  beats). Origin: the approved plan, `zenithmon-pok-mon-nested-puddle.md`.
- **Why:** rule-based logic needs exact-replay determinism and full headless
  unit-test coverage; no in-repo turn-based graph reference exists.
- **Tests that lock it:** S2 gate (~370 unit tests incl. scripted seeded
  battles with exact expected event streams + 2,000-battle fuzz soak).
- **Reversibility:** low -- reversing means rewriting S2; do not revisit.

## 2026-07-08 -- ZM-D-009 -- Game data = compiled const C-array tables, not disk assets

- **Decision:** species/moves/items/abilities/natures/type chart/encounters/
  trainers/dex text live as `const` C arrays in `Source/Data/*.cpp`; the
  "assets baked to disk" mandate covers meshes/textures/anims only. Origin: the
  approved plan.
- **Why:** compile-time validated, diffable in review, zero file I/O in
  headless CI tests.
- **Tests that lock it:** the `ZM_Tests_Data` validation suite +
  `ZM_DataRegistry` integrity tests (S1 gate).
- **Reversibility:** mechanical to move tables to files later, but it would
  sacrifice the zero-I/O headless-CI property -- user decision required.

## 2026-07-08 -- ZM-D-008 -- Battle format: singles only

- **Decision:** all battles are 1v1 singles; doubles is an explicit scope cut
  (see [Scope.md](Scope.md)). Struct layout does not preclude doubles later.
  Origin: the approved plan.
- **Why:** doubles is roughly 2x targeting/AI/UI complexity for marginal value.
- **Tests that lock it:** the entire S2 battle suite assumes single active
  monster per side.
- **Reversibility:** additive later behind a new scope decision; nothing to
  unwind now.

## 2026-07-08 -- ZM-D-007 -- Overworld-to-battle = ADDITIVE battle scene at world offset (0, -2000, 0) with overworld pause

- **Decision:** encounters load the battle scene ADDITIVE at (0, -2000, 0),
  `SetScenePaused(overworld, true)`, switch camera/HUD, and `UnloadScene` on
  exit; one battle scene with ~6 swappable biome dressing sets, enclosed by a
  backdrop dome. Documented fallback if visual isolation fails: SINGLE load +
  world-state snapshot. Origin: the approved plan.
- **Why:** a SINGLE reload resets render systems + physics and re-streams
  terrain -- seconds of hitch at wild-encounter frequency.
- **Tests that lock it:** S5 gate windowed round-trip tests (exact overworld
  resume) + screenshot check for overworld bleed-through at the offset.
- **Reversibility:** medium -- the fallback path is designed and documented;
  switching costs the S5 transition work only.

## 2026-07-08 -- ZM-D-006 -- Door/route-edge transitions = SINGLE loads with spawn tags; player/camera are NOT persistent entities

- **Decision:** `ZM_WarpTrigger_Component {targetBuildIndex, spawnTag}` -> fade
  -> SINGLE load; the persistent `ZM_GameStateManager` (`DontDestroyOnLoad`)
  respawns player + follow camera at the tagged `ZM_SpawnPoint`. One-time
  placement at load is not gameplay teleportation. Origin: the approved plan.
- **Why:** SINGLE loads reset physics and would orphan a persistent Jolt body;
  respawn-at-tag is the safe pattern.
- **Tests that lock it:** S3 gate windowed door/warp round-trip test; later
  per-region traversal tests walk every warp edge (S9/S10).
- **Reversibility:** medium -- changing to persistent player entities requires
  engine work on physics-across-SINGLE-loads first.

## 2026-07-08 -- ZM-D-005 -- ZM_WorldSpec is the keystone world table

- **Decision:** one declarative compiled table describes the whole world --
  scenes (name, build index, kind, terrain set, encounter table + rate),
  connections/spawn tags, trainers, shops, gyms, story beats. Tools walk it to
  author terrains/scenes/graphs; runtime walks it for warps/encounters/gating.
  Origin: the approved plan.
- **Why:** ~40 scenes without 40 bespoke authoring functions; one source of
  truth keeps authoring, runtime, and tests in agreement.
- **Tests that lock it:** WorldSpec referential-integrity unit tests (every
  warp target, spawn tag, species, and trainer resolves) run on every PR.
- **Reversibility:** low -- everything from S3 on flows through it; treat as
  load-bearing.

## 2026-07-08 -- ZM-D-004 -- Tall grass samples a game-owned CPU copy of the density map

- **Decision:** `ZM_TallGrassSystem` loads the baked `GrassDensity.ztxtr` per
  outdoor scene, feeds `g_xEngine.Grass().SetDensityMap(...)` for rendering,
  and keeps its OWN CPU copy for gameplay sampling; player XZ quantized to 1 m
  tiles, encounter roll on tile transition where density >= 0.5; density map
  cleared on interiors/battle. Origin: the approved plan.
- **Why:** engine `SampleDensityMap` returns 1.0 when no map is set -- gameplay
  must not inherit that trap, and the grass singleton is render-owned state.
- **Tests that lock it:** S5 gate encounter tests (walk grass until encounter
  with rigged RNG); grass-state assertions at S9/S10 gates.
- **Reversibility:** low cost -- swapping to engine-side sampling is a small,
  local change if the engine semantics ever harden.

## 2026-07-08 -- ZM-D-003 -- Baked assets are git-ignored, regenerated under per-family manifest guards

- **Decision:** everything under `Games/Zenithmon/Assets/` is git-ignored (repo
  norm) and regenerated by tools builds; per-family manifest guards =
  generator-version stamp + file-existence (hardened RenderTest pattern).
  Consequence: a fresh CI checkout has NO assets, so every asset/scene-dependent
  automated test must exists-guard and `RequestSkip` (the CI-fix pattern from
  commit `94813489`). Origin: the approved plan.
- **Why:** ~30-50 min cold bake output does not belong in git; determinism makes
  the repo the recipe, not the artifact.
- **Tests that lock it:** bake-determinism gate (re-run tools boot -> zero
  diffs, byte-identical re-bake) + the CI headless suite passing on an
  assets-absent runner.
- **Reversibility:** policy-level; committing baked assets would need a repo-
  wide user decision.

## 2026-07-08 -- ZM-D-002 -- Engine changes scoped to E1-E5, all additive and back-compatible

- **Decision:** the only engine-level changes this project makes are: E1
  per-component serialized terrain-set name (replaces 6 hard-coded `Terrain/`
  path sites); E2 `AddStep_TerrainExportChunksRect` + streaming-path
  missing-chunk tolerance check; E3 `Zenith_UIText` typewriter reveal; E4
  `Zenith_UIGridLayoutGroup`; E5 grass singleton reset hygiene (wire
  `Grass().Reset()` into `ResetRenderSystems` + clear instances/flags/density
  map). Each lands with unit tests + a RenderTest boot regression check.
  Origin: the approved plan.
- **Why:** verified engine gaps (one-terrain-per-game, full-grid bake volume,
  no typewriter/grid widgets, grass state leaking across SINGLE loads) --
  scoping them up front prevents ad-hoc engine sprawl.
- **Tests that lock it:** per-change unit tests + RenderTest still boots green
  (default-path untouched) + DP/CB suites stay green.
- **Reversibility:** per-change -- each is additive with a legacy-default path,
  so individually revertable before Zenithmon content depends on it.

## 2026-07-03 -- ZM-D-001 -- Scope lock (user decisions)

- **Decision:** the in/out scope for Zenithmon is locked as recorded in
  [Scope.md](Scope.md): ~150-species dex / 18 types / 3-stage lines / rarity;
  classic 8-gym world with no Wild Area; the full battle core; extras =
  abilities, natures, IVs/EVs, weather + terrain effects, breeding; post-game =
  Champion rematch + Battle Tower. Out: audio, networking/multiplayer/trading,
  Dynamax-analog, doubles, Substitute/Encore/Transform/weight moves, open Wild
  Area.
- **Why:** product of multiple prior iteration rounds with the user; frozen to
  prevent scope creep across a ~13-stage build.
- **Tests that lock it:** [Scope.md](Scope.md) is the binding gate; stage gates
  audit shipped content against it.
- **Reversibility:** user decision only, recorded as a new entry in this log
  (Scope.md change-control rule).
