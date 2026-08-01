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

**Shiny.** A rare palette variant of a species: hue-rotated albedo (band
[80,280) deg) + an **INDEPENDENT** material over the same mesh, with its own
`_shiny.ztxtr` -- explicitly NOT a child of the base material (ZM-D-065; see
[AssetManifest.md](AssetManifest.md) 1.2). Rolled at monster generation.

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

**Whiteout.** The player's loss state: the whole party faints. **The rules
SHIPPED at S7 and are no longer TBD** -- `ZM_BattleWriteBack` latches
`m_bPendingWhiteout`, and `ZM_GameStateManager::OnUpdate` (once the transition
is IDLE and no battle transition is active) heals the party FULL and warps to a
FIXED destination: Dawnmere Village, build index 2, spawn tag `TownCenter`
(`uWHITEOUT_BUILD_INDEX` / `szWHITEOUT_SPAWN_TAG`). Two things the older
description got wrong: the destination is fixed, not "the last heal point"
(there is no heal-point registry), and **no money penalty is implemented** --
the write-back only CREDITS prize money on a win. The whiteout destination is
deliberately distinct from the New Game entry point (PlayerHome / `Door`,
ZM-D-176); the boot unit
`ZM_Data/NewGameEntry_DiffersFromTheWhiteoutDestination` asserts they differ in
BOTH fields so a future edit cannot quietly re-merge them.

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

**Blockout / greybox shell.** The transitional box geometry every Zenithmon
interior is built from until the S4 art pipeline dresses it: named entities each
carrying a unit-cube `ZM_GreyboxVisual` (serialization order 107) plus their own
static AABB collider, authored as centre + scale by `AddStep_*`. `PlayerHome`
and `ProfLab` ship the SAME seven-block shell -- floor, back wall, left/right
walls, a front PAIR flanking an aperture, and a lintel bridging it -- differing
only in room size (16 x 12 m vs 20 x 16 m). The entrance is an ABSENCE of
geometry, not a hinged panel, which is why every block can stay
`COLLISION_VOLUME_TYPE_AABB` (an AABB collider forces identity rotation, so
anything that must FACE somewhere needs OBB instead). Blocks create no baked
model or material file. Block ORDER is part of the scene contract: appending is
free, reordering rewrites the `.zscen` bytes (ZM-D-148).

**Boot-authored scene.** A scene re-authored every ZENITH_TOOLS boot by
editor-automation `AddStep_*` calls and saved as a `.zscen`; non-tools builds
load the baked file. FrontEnd.zscen (build index 0) is Zenithmon's first
(DecisionLog ZM-D-012). **The five authored so far are COMMITTED, not
git-ignored** -- `FrontEnd`, `Battle`, `Dawnmere`, `PlayerHome` and `ProfLab`
(ZM-D-147 / ZM-D-148 / ZM-D-174) -- which is what lets CI verify scene content
on a fresh checkout with no bake. Scene bytes are boot-shape-independent (dense
authoring-order file indices), so a boot must NOT leave a scene modified in
`git status`; one known 2-byte violation on `Dawnmere.zscen` is open as
Q-2026-08-01-002. Everything else under `Assets/` remains git-ignored.

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

**Headless.** **A BUILD CONFIG, NOT A RUNTIME FLAG** -- there is no runtime
`--headless` (`Zenith/Core/Zenith_CommandLine.h`). A `Null_*` config defines
`ZENITH_NULL_RENDERER`, compiles `Zenith/Null` instead of Vulkan, and creates
its window hidden. **Flux is NOT skipped**: every render path still RUNS against
no-op backend calls, which is what makes a headless CI run representative. Only
tests that read PIXELS (`m_bRequiresGraphics=true`) auto-skip. The `--headless`
you type is a `zenith` CLI flag that SELECTS the Null exe
(`zenith build|test Zenithmon --headless`). The CI backbone is that headless
suite; see [CIPolicy.md](CIPolicy.md) and [TestPlan.md](TestPlan.md).

**Interior tint (derived at runtime).** ZM-D-176's warm colour on PlayerHome's
seven shell blocks, so the player's bedroom stops reading as the same greybox
room as ProfLab. `ZM_GreyboxVisual` asks
`ZM_IsPlayerHomeBlockName(entityName)` at `OnStart` and substitutes
`ZM_GetPlayerHomeInteriorTintColour()` (0.61, 0.57, 0.43) for the shared
`ZM_GetHumanPaletteFallbackColour()` grey (0.52, 0.55, 0.60). **Derived, never
stored:** no `.ztxtr`, `.zmtrl` or `.zscen` byte encodes it, which is why the
change moved no `PlayerHome.zscen` byte. The shared `fZM_GREYBOX_FALLBACK_*`
grey is untouched and still worn by ProfLab's blocks and every prop. The name
match is EXACT, never a prefix -- a prefix test would also catch
`PlayerHomeCamera` / `PlayerHomeExitTrigger`.

**New-game seed.** `ZM_MakeNewGameState()`
(`Source/Party/ZM_GameState.{h,cpp}`) returns the durable state a brand-new run
starts from: money 3000, 5x Catch Orb, 3x Salve -- and **an EMPTY party and
dex**. It is partyless on purpose: ZM-D-175 split the old
`ZM_MakeStarterGameState` (**DELETED**; any doc still naming it is stale) into
this plus `ZM_ApplyStarterChoice`, so seeding a run and choosing a starter are
separate steps. Neither bumps the save schema (still v1) nor sets a story flag.
A new run now begins at PlayerHome (build index 40) on its `Door` marker
(ZM-D-176), which is deliberately NOT the whiteout destination.

**Party-emptiness gate.** `ZM_CanEnterBattle(state)`
(`Source/Party/ZM_StarterChoice.h`) -- "does the player own anything to send
out?". It keys on `!m_xParty.IsEmpty()` ALONE, by ruling, not by oversight: the
fainted-aware form would also fire in the real window between a loss latching
`m_bPendingWhiteout` and the heal running in `ZM_GameStateManager::OnUpdate`,
which would be a live behaviour change. (Note `ZM_Party::AllFainted()` answers
TRUE for an empty party, which makes the wrong form look correct.) It landed at
ZM-D-175 provably INERT.

**Placement header.** The one place a scene's authored coordinates live so the
tools authoring and the tests read the SAME numbers:
`Source/World/ZM_DawnmerePlacement.{h,cpp}`, `ZM_ProfLabPlacement.h`,
`ZM_PlayerHomePlacement.h`. All are PURE (no ECS, scene, physics, `g_xEngine`,
allocation or I/O) and are NOT `ZENITH_TOOLS`-gated, so boot units in a headless
CI build -- where `Project_RegisterEditorAutomationSteps` is compiled out
entirely -- can still read them. **The rule they exist to enforce: never
re-spell a literal from one at a call site.** A constant spelled twice cannot
red a drift, because both sides move together and the assertion becomes
decorative. Build indices are deliberately NOT in them: those live in the
compiled `ZM_WorldSpec` table.

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

**Starter choice.** `ZM_StarterChoice`
(`Source/Party/ZM_StarterChoice.{h,cpp}`, ZM-D-175): a compiled-const table in
the ZM-D-009 idiom, one row per `ZM_STARTER_CHOICE` -- Fernfawn (F01, Grass),
Kindlet (F02, Fire), Finlet (F03, Water) -- each row also naming the choice
whose species COUNTERS it (authored, not derived from `ZM_TypeChart`, so the
agreement unit is a real tripwire rather than a tautology).
`ZM_ApplyStarterChoice(state, choice)` grants exactly one
`ZM_BuildMonsterRecord(species, uZM_STARTER_LEVEL = 5)` into the party and marks
it caught (caught implies seen); it touches PARTY AND DEX ONLY and sets NO story
flag -- `ZM_STORY_FLAG_STARTER_RECEIVED` exists and is left CLEAR so the save
bytes do not move. Every accessor is TOTAL and never calls `Zenith_Assert`.
**As of ZM-D-175 the data layer and the grant exist; the professor and the
starter-choice SCREEN do not** -- S8 item 1 is in progress, not complete.

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
