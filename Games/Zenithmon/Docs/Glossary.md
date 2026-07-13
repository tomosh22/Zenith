# Zenithmon -- Glossary

**Document purpose:** Authoritative definitions for terms used across the
Zenithmon docs. New agents and the human reviewer reference this file when
terminology is ambiguous. If a term in a sibling doc means something different
than its entry here, **the entry here is the source of truth** -- update the doc
to match, not the glossary.

Terms are alphabetical within each of the two sections. Cross-references point
at sibling docs by filename ([Scope.md](Scope.md), [DecisionLog.md](DecisionLog.md),
[GameDesignDocument.md](GameDesignDocument.md), [TestPlan.md](TestPlan.md),
[AssetManifest.md](AssetManifest.md), [CIPolicy.md](CIPolicy.md)).

---

## Game / design terms

**Ability.** A passive per-monster effect implemented as a struct of function
pointers per battle hook (OnSwitchIn/OnModifyDamage/OnTurnEnd/...); ~50 ship.
Data row in `ZM_AbilityData`.

**Badge gate.** A progression lock keyed on gym badges owned: story roadblocks,
route access, and traded-monster obedience all check the badge count via story
flags. Audited world-wide at the S10 gate.

**Battle Tower.** The post-game endless-streak facility: level-50 clamp, rental
or own-team runs, opponents and AI tier escalate with streak, boss every 7th
battle. Streak persists in the save.

**Dex.** The catalogue of ~150 original species and the player's seen/caught
record (bitsets in the save). "Dex family" = one 3-stage evolution line sharing
an archetype + family seed. See [GameDesignDocument.md](GameDesignDocument.md).

**Egg group.** Breeding-compatibility category on each species; two monsters in
a shared egg group (opposite sexes) can produce an egg at the daycare.

**Encounter field.** A patch of tall grass painted beside (never on) a route's
path where wild encounters can trigger -- authored as GrassDensity brush patches
in the terrain recipe.

**Encounter table.** Per-route/zone list of species + level ranges + weights +
encounter rate, a compiled data row in `ZM_EncounterData` referenced by
`ZM_WorldSpec`.

**IV / EV.** Individual Values (fixed per-monster 0-31 per stat, rolled at
generation or inherited via breeding) and Effort Values (earned from defeating
monsters, capped, trainable). Both feed the Gen-III+ stat formulas in
`ZM_StatCalc`.

**Nature.** One of 25 per-monster modifiers applying x11/10 to one stat and
x9/10 to another (or neutral). Lockable during breeding via the everstone item.

**Priority bracket.** Turn-order tier of a move. Actions resolve run/item/switch
first, then moves by priority bracket, then effective speed, then RNG tie-break.

**Rarity tier.** Per-species classification (common through legendary-tier)
driving encounter-table weights and catch difficulty.

**Rental team.** A Battle Tower option: a generated loaner team used for a
streak run instead of the player's own party.

**Shiny.** A rare palette variant of a species: hue-rotated albedo + child
material on the same mesh. Rolled at monster generation.

**STAB.** Same-Type Attack Bonus -- the damage multiplier applied when a move's
type matches one of the user's types. Part of the Gen-V damage formula in
`ZM_DamageCalc`.

**Stat stage.** In-battle stat modifier from -6 to +6 applied on top of computed
stats (Attack, Defense, etc., plus accuracy/evasion). Reset on switch-out and at
battle end.

**Streak.** Consecutive Battle Tower wins; drives opponent scaling and AI tier
escalation, saved across sessions.

**Tall grass.** The wild-encounter mechanic: walking a 1 m tile whose grass
density is >= 0.5 rolls the route's encounter rate on each tile transition. See
"encounter field" and ZM_TallGrassSystem under engine terms (DecisionLog
ZM-D-004).

**Trainer sight cone.** An NPC trainer's detection volume: simple forward cone +
occlusion raycast (deliberately NOT the engine perception subsystem). On spot:
input freeze, approach, dialogue, forced battle.

**Whiteout.** The player's loss state: the whole party faints, the player
respawns at the last heal point with a money penalty (exact respawn/penalty
rules TBD at S7).

---

## Engine / project terms

**ADDITIVE / SINGLE load.** The two scene-load modes. SINGLE resets render
systems + physics then deserializes (used for door/route warps); ADDITIVE loads
alongside without a reset (used for the battle scene at offset (0, -2000, 0) --
DecisionLog ZM-D-007).

**Anchor chunk.** Terrain chunk (0,0). Its absence makes terrain geometry
unusable (hard-fail); any other missing chunk is skip-with-warning. Enables
rect-only chunk exports (engine change E2).

**Archetype (creature).** One of 8 generator body plans (QUADRUPED, BIPED,
AVIAN, SERPENT, AQUATIC, INSECTOID, BLOB, FLOATER-PLANTOID): one builder emits
mesh + skeleton together with a fixed bone topology so the archetype's 6 clip
templates transfer to every species using it.

**Bake manifest / stamp.** The guard that decides whether tools builds
regenerate an asset family: generator-version stamp + file-existence check
(`ZM_BakeManifest`, hardened RenderTest pattern). Valid stamps = warm boot in
seconds; see [AssetManifest.md](AssetManifest.md).

**BattleDirector.** `ZM_BattleDirector`, the presentation-side interpreter: owns
the battle engine + an event-stream cursor and maps each event to a timed visual
op (HP tween, ball shakes, typewriter text). The `zm_instant_battles`
DebugVariable skips the timing for tests.

**Between-tests hook.** `Zenith_AutomatedTestRunner::RegisterBetweenTestsHook`
callback run between batched tests (alongside a forced scene-0 reload).
Zenithmon registers game-global resets here from S0, including
`Zenith_SaveData::ClearForTest`.

**Boot-authored scene.** A scene re-authored every ZENITH_TOOLS boot by
editor-automation `AddStep_*` calls and saved as a git-ignored `.zscen`;
non-tools builds load the baked file. FrontEnd.zscen (build index 0) is
Zenithmon's first (DecisionLog ZM-D-012).

**Chunk.** Terrain streaming unit: a terrain is a 64x64 grid of 64 m chunks
(4096 m square). Zenithmon bakes only a rect subset per scene via E2; see
"anchor chunk".

**Data tables (compiled).** Zenithmon's game data as `const` C arrays in
`Source/Data/*.cpp` -- compiled into the exe, no disk I/O, schema-enforced by the
`ZM_Tests_Data` suite (DecisionLog ZM-D-009).

**Event stream.** The append-only `ZM_BattleEvent` sequence emitted by the
battle engine -- the single source of truth consumed by both unit tests (exact
expected streams) and `ZM_BattleDirector` (presentation). The engine never
formats strings or touches UI.

**Headless.** Running without graphics (`--headless`): Flux is skipped, tests
with `m_bRequiresGraphics=true` auto-skip. The CI backbone is the headless
suite; see [CIPolicy.md](CIPolicy.md) and [TestPlan.md](TestPlan.md).

**Regenerate-first.** Repo policy: everything Sharpmake emits is git-ignored, so
run `Build\regen.ps1` (or `zenith regen`) after any `.zproj`/`Sharpmake_*.cs`
change or fresh checkout before building.

**RequestSkip.** `Zenith_AutomatedTest` API marking a test skipped-with-reason
at runtime. Mandatory for asset/scene-dependent tests, which must exists-guard
first -- baked assets are git-ignored, so CI runners have no `Assets/`
(DecisionLog ZM-D-003).

**Spawn tag.** Named `ZM_SpawnPoint` marker in a scene. A `ZM_WarpTrigger`
carries a target build index + spawn tag; after the SINGLE load,
`ZM_GameStateManager` treats the marker as Player feet, places and zeroes the
replacement scene-owned Player, then holds the screen opaque until the
replacement scene-owned main follow camera targets that exact generation
(DecisionLog ZM-D-006/056/057).

**Terrain set.** Per-component serialized asset-subdirectory name for terrain
(engine change E1), e.g. `Terrain/Route01/` -- lifts the one-terrain-per-game
limit so each outdoor scene gets its own baked terrain.

**Tools build / _True config.** Build with `ZENITH_TOOLS` defined (config names
ending `_True`, e.g. `Vulkan_vs2022_Debug_Win64_True`): includes editor
automation, asset baking, and scene authoring. `_False` runtime builds load
pre-baked assets only.

**Warp fade.** The exact persistent-root `WarpFade` UIOverlay used for SINGLE
door/route transitions. The game manager advances its alpha over 0.20 s,
blocks the load until opaque, holds input through placement/camera readiness,
and unlocks only after fade-in reaches transparent. It is globally ordered at
sort 10000 across canvases and fails closed if the dependency disappears
(DecisionLog ZM-D-057).

**WorldSpec.** `ZM_WorldSpec`, the keystone compiled table describing the whole
world (scenes, connections/spawn tags, encounter tables, trainers, shops, gyms,
story beats). Tools author from it; runtime gates by it; integrity tests keep it
sound (DecisionLog ZM-D-005).

**zenith test harness.** The unified per-game test runner: `zenith test
Zenithmon` (`Tools/ZenithCli/ZenithCli.psm1` + `ZenithTestHarness.psm1`), flags
`--filter/--headless/--results-dir/--config/--per-process/--fail-fast`, exit
codes 0/1/2/3/4/5. The ONLY runner -- per-game scripts are deleted (DecisionLog
ZM-D-013).

**ZM_ prefix.** Mandatory prefix for all Zenithmon game code (types, files,
events), mirroring DP_/CB_. The scaffold component is `ZM_GameComponent`,
registered as "ZM_Game".

**zm-tests.** The Zenithmon CI gate (`.github/workflows/zm-tests.yml`, dp-tests
clone): regen, Vulkan_True build, D3D12_False link proof, headless boot check,
`zenith test Zenithmon --headless`, results artifact. Required-check
registration on master is a manual GitHub-UI step (see
[ManualSetupChecklist.md](ManualSetupChecklist.md)).
