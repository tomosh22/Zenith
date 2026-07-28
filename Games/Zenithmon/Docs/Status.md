# Zenithmon Status

**Last updated:** 2026-07-28

**★ CURRENT BASELINE -- USE THESE NUMBERS, not the older ones quoted further
down this file (ZM numbers OBSERVED 2026-07-28 on a fresh build of both configs
after SC8):** ZM headless registry **47 passed / 0 failed**; ZM boot unit gate
**2703 ran / 2702 passed / 0 failed / 1 skipped** (`zm-tests.yml` pinned to
2703); engine boot unit gate, Null Combat, **1164 ran / 1163 passed / 0 failed /
1 skipped** (`run_unit_gate.ps1` default, unchanged by SC6/SC7/SC8 -- none of
them touches a `Zenith/` file). The 1 skipped in each is the quarantined
`GraphComponent::RegistryWideNodeRoundTrip` (task_726cc81d).

**★ THE REGISTRY MOVED AT SC8 (46 -> 47) BUT NOT AT SC7, AND BOTH ARE CORRECT.**
SC7's three new end-to-end phases live INSIDE the existing
`ZM_TrainerSightWalkUp_Test` registration, so its count stayed 46 while boot
units moved +13. SC8 adds a genuinely NEW registration
(`ZM_RivalVesperAuthored_Test`) because its subject is a different entity making
a different claim. Before "fixing" a registry count that did not move, check
whether the coverage rode an existing registration.

**★ SIXTH TRIPWIRE -- A NEGATIVE CONTROL PROVES NOTHING UNTIL IT FLIPS.** SC8's plan
called for one: make the new test fail against the old scene bytes, re-author, watch
it pass. It DID fail first -- and then **failed again after the re-author**. A
fail-then-fail is not a control, it is a masked defect. Run ONLY the "after" and one
red test gets blamed on the re-author not working; run ONLY the "before" and a
coincidence gets recorded as proof. **It is the PAIR that carries the information.**
The masked defect here was real and is documented in ZM-D-156: an AABB collider
forces its body to identity rotation, wiping the authored yaw out of the SAVED
BYTES while every boot unit stayed green.

**★ AND ITS COROLLARY: A UNIT SUITE CANNOT SEE A SCENE-BYTE DEFECT.** SC8's boot
units reason about the COMPILED placement constants -- position, bearing, clearances
-- all of which were correct (`placementErrX/Z = 0.0000`) while the authored rotation
on disk was identity. Only the windowed round trip reads what was actually saved.
When a sub-commit writes a scene file, the windowed run is not a formality.

**★ FIFTH TRIPWIRE -- A SURVIVING MUTATION HAS TWO POSSIBLE CAUSES AND YOU MUST
DETERMINE WHICH.** (SC8 reproduced this a second time: reverting the collider to AABB
SURVIVED, not because the facing clause lacks teeth, but because the mutation cannot
manifest without RE-AUTHORING the scene -- which a battery does not do. The clause's
teeth were proven live instead: AABB-authored scene FAILS, OBB-authored scene PASSES.) SC7's battery had two survivals with OPPOSITE meanings. One was a
real coverage gap: transposing the challenge-lines selector's null-check and clamp
survived because the only pair that discriminates them is `(nullptr, count > cap)`
and the clause used a count UNDER the cap -- **its failure text named the exact
hazard while its fixture could not reach it.** The other was a BAD MUTATION:
re-spelling the shared node-type-name constant redded nothing, and correctly so,
because every site reads that one constant so the two sides cannot diverge (the real
hazard is a hard-coded literal at one site, which does red). **Calling both "no
teeth" would have libelled a good test; calling both "bad mutation" would have
shipped a hole.** Ask, every time: *can this mutation actually produce the behaviour
the test is meant to catch?*

**★ FOURTH TRIPWIRE -- A MUTATION HARNESS MUST PARSE THE OBSERVED RESULT, NOT
THE EXPECTATION.** SC6's battery script scraped `(\d+) failed` from
`run_unit_gate.ps1`'s output and matched the **"wanted 2682 ran, 0 failed"**
clause instead of the observed tally, so all three unit mutations were initially
labelled SURVIVED -- i.e. "these tests have no teeth" -- when every one of them
had in fact redded. The `baseline NOT met` line and the pass-count delta
(2681 -> 2680 / 2679 / 2680) are what settled it. This is the same
passes-because-it-never-really-checked defect the project hunts in game code,
reproduced in the tooling: anchor result parsing to the OBSERVED line, and never
to a string the harness also prints when describing what it wanted.

*How they moved this session:* 2547 -> **2589** was the 07-27 engine commits
(harness world reset, harness dt pinning, navmesh RNG determinism) adding units,
and `e687d095` left BOTH baselines one short, so `engine-gate` and `zm-tests`
were RED on master until **ZM-D-149** fixed them forward (1163 -> 1164,
2588 -> 2589). 2589 -> **2607** is SC2's own +18 units (ZM-D-150), and
2607 -> **2638** is SC3's +31 (ZM-D-151), 2638 -> **2644** is SC4's +6
(ZM-D-152), and 2644 -> **2657** is SC5's +13 (ZM-D-153, which also took the
automated registry **44 -> 45**).

**★ THIRD TRIPWIRE -- A MUTATION BATTERY MUST CHECK THE BUILD'S EXIT CODE.** Two
SC4 batteries mutated the gate to `if (false)` / `if (true)` and then to a
hard-coded flag argument. Both FAILED TO COMPILE (unreachable `return`, and an
unreferenced formal parameter -- C2220, warnings are errors here), so the gate
booted the STALE exe and reported a perfectly green 2644/2643/0. That is
indistinguishable at a glance from "the mutation did not red, so the test has no
teeth" -- i.e. it would have libelled a good test. Mutations must keep every
parameter referenced (transpose arguments, invert a returned expression) AND the
harness must hard-fail on a non-zero build exit.

**★ SECOND GATE TRIPWIRE:** a new `.cpp` under `Games/Zenithmon/` needs
`Build\regen.ps1` BEFORE the build, and the failure mode is SILENT. Sharpmake
globs the game tree at GENERATION time and everything it emits is gitignored, so
a skipped regen builds GREEN with the new TU simply absent -- and if nothing else
references its symbols there is no link error either. **The unit-count delta is
the only proof the regen took** (SC2 expected +18 and got +18; SC3 expected +31
and got +31).

**★ GATE-ORDER TRIPWIRE (cost a wasted 300 s boot to rediscover):** the boot unit
gate must run AFTER `zenith test <Game> --headless`, never before, on a freshly
built Null exe. `zenith test` heals the output directory's DLLs; a fresh `Null_*`
build lacks `libcurl-d.dll`, so booting the exe first dies in the loader with
ZERO stdout and the gate reports the misleading "no 'Unit tests complete' line in
boot output" rather than a load failure.

**Stage:** **S7 (save/load, story flags, trainer battles) ACTIVE. Item 1 (full schema-v1 codec) COMPLETE (SC1-SC2, ZM-D-135/136). Item 2 (story flags + save integration) COMPLETE (SC1-SC6, ZM-D-137..142). Item 3 IN PROGRESS: SC1 eval spike DONE (ZM-D-144), SC1b DONE (ZM-D-146/147), SC2 DONE (ZM-D-150). NEXT = SC3** (the pure sight-cone predicate + the trainer `ZM_BattleConfig` helper). S0-S6 remain complete. S7 requires no human intervention; the next human stop remains the S8 vertical-slice go/no-go.
**Build:** GREEN on the ZM-D-148 diff (scene authoring made boot-shape-independent; all four ZM scenes now TRACKED) on top of SC1b commit B (ZM-D-147 -- baked navmesh persistence). Engine-wide, so it owed and got the full gate: `Build\regen.ps1` GREEN + `zenith regen --check` in sync; engine lib + SentinelECS/Physics/AI (all three exes exit 0); Zenithmon Vulkan_True + Null_True; Combat / CityBuilder / DevilsPlayground / RenderTest / TilePuzzle Null_True.
**Tests (commit B):** Null batches, ALL 0-failed: **ZM 44/44** (registry 42 -> 44; both new navmesh tests RUN, not skipped), CB **45/45**, DP **158/158**, RT 9/9, Combat 14/14. Full **windowed Vulkan ZM 44/44, 0 skipped, 0 failed**. Boot unit gates on the NULL exes: engine **1093 -> 1121** (Combat) and ZM **2515 -> 2546** -- both pinned from the OBSERVED line. Windowed RenderTest 8 passed / 1 failed, only the documented pre-existing `RT_TennisDeterminismDigest` (Q-2026-07-21-002). Ratchets (`architecture,lints` and `complexity`) are **byte-identical to a pristine-HEAD worktree** -- both stay pre-existing RED, nothing added; two findings this commit DID introduce (an `Editor/` include and a `g_xEngine` reach from EntityComponent) were fixed, not allow-listed. **Asset-less CI condition reproduced locally** (`Zenith/Assets` hidden): ZM 44/44 and both unit gates unchanged; restored by MERGE and `diff -rq`-verified, since the run re-created only 60 of the 89 files and a naive rename-back would have clobbered the tree. **Teeth mutation-proven ×6** (see ZM-D-147; m1 re-run on the final build reds exactly the 3 serialization units).

**★ ZM-D-148 (2026-07-26, user-directed follow-up):** scene files no longer encode PROCESS-GLOBAL entity slot indices -- `Zenith_Entity::WriteToDataStream` now emits dense authoring-order file indices (`Zenith_EntityFileIndexMap`). Reproduced first (a boot with the unit suite vs `--skip-unit-tests`): Battle, PlayerHome **and Dawnmere** all differed, so the scene ZM-D-147 committed was itself at risk. After the fix all four hash identically across both boot shapes and survive a full windowed batch byte-unchanged, so **all four scenes are tracked** and `.zscen` is re-included repo-wide. The loader needed no change (file indices were always opaque keys) and OLD scenes still load -- proven by Combat 14/14, CB 45/45, DP 158/158 against their un-re-authored files. Baselines **1121 -> 1122** / **2546 -> 2547**; mutation-proven (reverting the writer reds exactly `Scene::SceneBytesAreIndependentOfSlotAllocation`). **RenderTest diagnosed too:** the "2 failed" is 1 real failure + a synthetic `<batch:exit=1>` harness marker, and `RT_TennisDeterminismDigest` is a TEST-ISOLATION defect -- it produces the EXACT pinned digest standalone and a stable wrong one only in batch, on both backends; RenderTest registers no `BetweenTestsHook`. Both logged questions are now RESOLVED.

**CI (post-push, `a6f8b048`):** `zm-tests`, `engine-gate`, `dp-tests`, `cb-tests`, `shader-validation`, `doc-lint`, `Memory Gate` all **GREEN** -- every gate that was green before this commit still is, first attempt, no fix-forward needed. `Complexity Gate` + `layering-gate` remain red and were verified **byte-identical to the previous commit's own CI logs** (the same pre-existing `Zenith_TerrainComponent` findings), so this commit added nothing to either. **★ The claim that mattered, confirmed on the runner:** on a fresh checkout whose ONLY Zenithmon assets are the two committed files, `ZM_NavmeshAsset_Test` (5 frames) and `ZM_DawnmereHeadless_Test` (3 frames) both **RAN and PASSED** -- not skipped -- inside a 44/44 suite, with the boot gate at **2546 ran / 2545 passed / 0 failed**. Navigation is CI-verifiable for the first time.

**Prior (commit A):** **Headless now means the `Null_*` build config -- there is no `--headless` flag.** Null batches, ALL 0-failed: **ZM 42/42 (31 RUN, was 3)**, CB **45/45 (45 RUN)**, DP **158/158 (138 RUN)**, RT 9/9, Combat 14/14. Full **windowed Vulkan ZM 42/42, 0 skipped, 0 failed**; windowed RenderTest 8/1 with only the documented pre-existing `RT_TennisDeterminismDigest` red (Q-2026-07-21-002) and `TerrainEditorSmoke` GREEN. Boot unit gates on the NULL exes: engine **1093** (Combat) and ZM **2515** -- both pinned from the OBSERVED line (`run_unit_gate.ps1` default and `zm-tests.yml`). The -14 vs the Vulkan counts (1103 / 2525) is exactly the Vulkan-only test set (11 `Flux_SlangProbes` + 3 `DetermineImageViewType`); the +4 is this commit's new terrain units. **★ Teeth mutation-proven:** leaving `Zenith_Null_MemoryManager::InitialiseIndirectBuffer`'s VRAM handle invalid reds EXACTLY `Terrain::CullingResourceInitSurvivesOnCurrentBackend` (1093/1091/1) and nothing else; restored -> 1093/1092/0. Ratchets: `architecture,lints` findings are **byte-identical to pristine HEAD** (all 3 pre-existing); `complexity` was ALREADY RED on master (same 2 cognitive findings) and moves duplicate clusters 9 -> 10, the accepted cost of the deliberate D3D12/Null twinning.
**CI (post-push, 3 commits -- `2628add3` then two fix-forwards):** `zm-tests`, `dp-tests`, `cb-tests`, `engine-gate`, `scaffold-smoke`, `shader-validation`, `doc-lint`, `Memory Gate` all **GREEN**. `Complexity Gate` + `layering-gate` remain red -- **pre-existing on master** (confirmed from CI history across the preceding commits AND from a pristine-HEAD worktree that reproduces the identical findings). **★ THREE CI-ONLY DEFECTS the local gate could not see**, all because this machine has `Zenith/Assets` baked while a fresh checkout does not (it is gitignored): (1) `Import-Module Build\...` without a leading `.\` is read as a MODULE NAME, killing four gates; (2) unset pinned cubemap/water textures reached the recorder's `BindSRV` assert -- a process KILL -- now falling back to the procedural white texture; (3) `Zenith_AssetHandle` caches only SUCCESS, so the missing default font was re-resolved EVERY frame by `Zenith_FontAsset::GetActiveOrDefaultMetrics` (~25k attempts in 180 s) and dp/cb TIMED OUT rather than failing. **★ The method that found them: reproduce CI's condition locally** (`Zenith/Assets` temporarily hidden, batches re-run) rather than iterating through 20-minute CI round trips -- CB 45/45, DP 158/158, ZM 42/42, zero asserts under that condition. Two asserts observed and deliberately NOT scoped in: asset-less Combat (missing `.zanim`) and RenderTest (null `LOAD_MODEL`); no workflow runs an automated batch for either game.

## Current task

**★★ S7 ITEM 3 AND ITEM 4 ARE COMPLETE (SC8, ZM-D-156). THE NEXT STEP IS A HUMAN
GATE: THE S8 VERTICAL-SLICE GO/NO-GO.** Do not start S8 work. The loop's standing
order is to hard-stop here and report. `SC8_Plan.md` was consumed and DELETED in that
commit.

**What the vertical actually does now, end to end:** the player walks around Dawnmere;
rival Vesper stands AUTHORED in the town at (490, 524) facing the spawn approach; when
the player enters his 8 m / 60-degree forward cone with an unblocked line of sight he
speaks a challenge line, the battle fades in, the player fights him, and on a win takes
+500 money and sets `RIVAL1_DEFEATED` -- after which he never re-engages, and that
silence survives a save/reload.

**★ THE HONEST "WHAT IS STILL MISSING" LIST -- read this BEFORE signing the go/no-go.
The vertical is real but it is not the full mainline beat, and it should not be
oversold:**
1. **The trainer LOSS / whiteout path has never been executed by any automated test** --
   and it is the branch an honest human playthrough hits FIRST (L5 Grass starter vs L5
   Fire rival, with Catch and Run both gated off). SC8 makes losing SAFE (the placement
   clears the respawn by 49 m, pinned by a boot unit) but not PROVEN. The new test
   installs an over-levelled lead precisely to avoid it.
2. **There is no visual "spotted" beat at all** -- no exclamation mark, no camera cut, no
   approach walk. The rival notices the player silently until the dialogue box appears.
3. **The rival is visually indistinguishable from the four townsfolk.** `ZM_GreyboxVisual`
   paints one fixed grey and `ZM_NpcData::m_eHuman` is consumed by NOTHING today.
4. **One-monster trainer battles only.** `uZM_TRAINER_BATTLEABLE_PARTY == 1` because the
   engine has NO forced switch on faint. Vesper's row is one member so the clamp is a
   no-op for him, **but S8's gym cannot ship until the replacement path does.**
5. **Route 1 does not exist**, so the GDD's canonical location for rival battle 1
   ("Route 1, L5") does not either. The Dawnmere placement is a RECORDED deviation
   (Q-D / ZM-D-156) carrying a re-placement debt.
6. **The challenge `.bgraph` is gitignored**, so a fresh CI checkout loses the BARK and
   keeps the BATTLE (SC7's deliberate fail-open). The bark beat is exercised locally
   only, never in CI.
7. **No test proves the authored Vesper carries no graph slot at RUNTIME** -- it cannot,
   because the runtime attach is idempotent by path. The property rests on an
   authoring-time assert plus the boot-stability check.

**★ WHAT SC8 BINDS FOR ANY FUTURE SCENE WORK:**
- **An AABB collider DESTROYS an authored rotation.** `Zenith_ColliderComponent` forces
  an AABB body to identity (axis-aligned by definition) and the physics-to-transform sync
  writes it back. Any authored entity that must FACE anywhere needs
  `COLLISION_VOLUME_TYPE_OBB`. Vesper's call site carries a "do not tidy this back to
  AABB" warning; heed it.
- **A sub-commit that rewrites a committed scene owes the full operational proof:** a
  windowed `_True` boot logging `sceneAuthoring=AUTHOR_DAWNMERE` (a `DEFERRED` boot
  silently does nothing and looks successful), SHA256, a SECOND authoring boot, identical
  hashes, only the intended asset changed, and a re-hash after the batches to prove no
  play-session save baked a `Zenith_GraphComponent` payload in.
- **`ZM_TRAINER_RIVAL_VESPER == 0`** -- a `ZM_NpcData` row omitting its trailing
  initializer silently becomes the rival, with no compiler warning.

**Prior: S7 item 3 SC7 COMPLETE (ZM-D-155).** The game's FIRST `.bgraph` -- the trainer
challenge bark -- landed with ZERO scene bytes and ZERO new ECS orders.
`SC7_Plan.md` was consumed and DELETED in that commit, as it instructed.
**NEXT = SC8: rival Vesper authored in Dawnmere + trainer-id persistence.**

**★ SC7's SCOPE WAS CONTRADICTED ACROSS TWO BINDING DOCS AND THE USER DECIDED IT
(2026-07-28). Do not re-open this.** `DecisionLog.md`'s ZM-D-143 sequence block and
`Questions.md` **Q-B** both defined SC7 as the trainer-DEFEAT beat (`SetStoryFlag`
-> `AwardPrizeMoney`, fired from SC5's win callsite), while `SC6_Plan.md` -- and
therefore `Roadmap.md` and this file -- said the spot-bark beat. Both were
implementable and Q-B was a USER-ADOPTED default, so it was escalated to the user
rather than resolved by an agent. **Verdict: the bark**, because the defeat beat is
already shipped and test-locked in C++ as SC5's `ZM_ApplyTrainerResultToGameState`.
The full ruling, including the honest caveat that NEITHER shape gives the graph "one
genuine decision", is recorded against Q-B in `Questions.md`. The
defeat-beat-as-graph is NOT scheduled.

**The honest one-line description of what the vertical does in-game after SC7:** "a
trainer who sees you speaks, then battles you." There is still no walk-up-to-you
approach -- the battle starts from where the player stands -- and no trainer is
placed in a scene yet. Both are SC8's.

**What SC7 pinned that SC8 must not re-litigate:**
- **★ AUTHORED VESPER MUST NOT CARRY AN ATTACHED GRAPH SLOT.** The graph is attached
  at RUNTIME from `TickTrainerSight`, an `OnUpdate`-only path; `Zenith_Core.cpp:138`
  gates `Scenes().Update` on `EditorMode::Playing` and the boot authoring pass runs
  Stopped, so a `Zenith_GraphComponent` cannot exist during `AddStep_SaveScene`.
  `AddStep_AttachGraph` on `ZM_MenuRoot` was REJECTED (it would add an order-60
  payload to the committed `FrontEnd.zscen`). Vesper picks the graph up through the
  same runtime attach and `Dawnmere.zscen` never moves.
- **The battle is GRAPH-INDEPENDENT and FAILS OPEN.** `.bgraph` files are gitignored
  and tools-authored, so a `_False`/Android build or a fresh CI checkout has none.
  The FSM's `CHALLENGING` state raises the encounter anyway when
  `m_fChallengeConfirmSeconds` expires. **That window's polarity is DELIBERATELY
  OPPOSITE to `m_fRaiseConfirmSeconds`** (degenerate raise window = stay silent;
  degenerate challenge window = raise immediately). Do NOT unify them into a shared
  helper -- a unit pins both side by side precisely to stop that.
- **`m_bChallengeAvailable` DEFAULTS FALSE**, which is what keeps all 16 SC6 FSM
  units passing unmodified and stops a silent trainer paying half a second of dead
  air. `ZM_TRAINER_ROUTE1_RAMBLER` ships ZERO lines on purpose as the production
  instance of that arm.
- **A three-node `Query -> Branch -> Bark` graph was REJECTED** and should stay
  rejected: the only branchable condition ("does this trainer have lines?") must
  already be decided in C++ so the FSM can skip its window, and a graph duplicating
  a C++ gate is less correct. The graph owns the BEAT, not a decision.
- **Order 112 < 113 is load-bearing**: MenuStack closes/unfreezes at 112 and the
  withheld `Dispatch` fires at 113 in the SAME frame (measured
  `barkToBattleFrames=1`). SC7 added NO new freeze owner and SC8 must not either.

**★ A CORRECTED FALSE CLAIM, worth remembering because it was stated authoritatively
in two files:** `GetUnresolvedCount() == 0u` CANNOT catch a typo'd node-type name for
an in-process build. `Zenith_GraphBuilder::Node` looks the type up in the registry
and, on a miss, logs "unknown node type", latches `m_bErrors` and returns id 0 -- the
node never reaches the definition, so the count stays 0 and `Build()` returns false
instead. **The real guards are `Build() == true` / `HasErrors() == false`.**
`GetUnresolvedCount()` earns its keep only against a stale or hand-edited LOADED
`.bgraph`.

**Still true and still the headline battle-engine shortfall (Shortfalls 1.2): the
engine has NO forced switch on faint**, so `uZM_TRAINER_BATTLEABLE_PARTY = 1` in
`Source/Battle/ZM_TrainerBattle.h` clamps every trainer to its authored lead. Raise
that constant ONLY in the commit that adds the replacement path. S8 needs it before
it can show a credible gym.

**What SC6 pinned that SC7-SC8 must not re-litigate:**
- **Cheap-gate-first IS the cost control.** There is no raycast budget anywhere in
  this engine, so the SC3 pure cone runs FIRST and the ray is issued ONLY on a cone
  pass. Do not add an unconditional raycast, and do not add a raycast budget.
- **`TickTrainerSight` runs BEFORE `UpdateWander` and OUTSIDE its
  `if (!m_bWanderEnabled) return;` bail.** SC8's Vesper is STATIONARY, so folding
  the call inside the walker leaves every stationary trainer permanently blind
  **while every unit test still passes** (the units drive the FSM directly, not
  through `OnUpdate`). Mutation M1 exists solely to pin this -- do not "tidy" it.
- **The flagged/flagless gate asymmetry is deliberate** (Q-2026-07-28-001, now
  ANSWERED): a row with a defeat flag keys on that flag and ignores the latch; a
  flagless row keys on the process-global session latch, because
  `ZM_IsStoryFlagSet(state, NONE)` reads false forever and the prize would
  otherwise be farmable. Losing to Vesper leaves him re-battleable; losing to the
  rambler does not. It is documented in the gate's own header so nobody "fixes" it.
- **Only ONE body can be filtered per raycast.** The trainer is ignored by id and
  the player's capsule is excused by comparing `RaycastResult::m_xHitEntity` -- no
  distance tolerance. Fail polarity: non-finite CLOSED, no live simulation OPEN,
  coincident endpoints clear WITHOUT casting.
- **`TryResolveActivePlayerPose` is GONE** (no-legacy). Use the public
  `ZM_InteractionRuntime::TryResolveActivePlayer`.
- SC6 adds **no second freeze owner**; `ZM_BattleTransition::OnTrainerEncounterEvent`
  + `TryParkOverworldPlayer` still own that seam.

**★ THE OPEN DEBT SC8 OWNS: an authored trainer id does NOT survive save/reload.**
SC6 serializes nothing (`uSERIALIZATION_VERSION` stays `2u`) because the
per-component size prefix is computed from what is written, so one new field would
grow the five `ZM_Interactable` payloads inside the COMMITTED `Dawnmere.zscen`.
**Verified, not assumed:** a full windowed Vulkan batch left `Games/Zenithmon/Assets`
byte-clean. **SC8 should prefer the zero-byte route** -- a `ZM_TRAINER_ID` column at
the END of `ZM_NpcData`, derived in `OnStart` -- over a v3 payload bump.

**Two limitations recorded honestly, not papered over:**
1. **There is no occlusion coverage in Dawnmere itself.** The occlusion proof is a
   hermetic boot unit against an explicitly created box; the end-to-end test walks a
   CLEAR line. Terrain is NOT an occluder on a fresh CI checkout (its physics
   geometry is a gitignored baked asset), so "a wall blocks a trainer in the actual
   town" is reasoned, not measured. Do not claim otherwise.
2. **Another NPC's static AABB, or the `HomeDoorTrigger` box, counts as an
   occluder.** Arguably correct, but SC8 must keep it in mind when placing Vesper:
   a trainer behind the door trigger will appear inexplicably blind.

**Still true and still the headline battle-engine shortfall (Shortfalls 1.2): the
engine has NO forced switch on faint**, so `uZM_TRAINER_BATTLEABLE_PARTY = 1` in
`Source/Battle/ZM_TrainerBattle.h` clamps every trainer to its authored lead. Raise
that constant ONLY in the commit that adds the replacement path. S8 needs it before
it can show a credible gym.

**★ READ THIS BEFORE SC6 OR ANY BATTLE WORK: the engine has NO forced switch on
faint, so a trainer fields exactly ONE monster.** `m_uActiveSlot` is written only
by `Begin` and `DoSwitch` (voluntary switch / a move's forced-switch secondary),
`ResolveTurn` ends the battle only on a whole-party scan, and `SubmitAction`
opens with `Zenith_Assert(!xActive.IsFainted(), ...)` -- so a multi-monster side
whose active faints neither ends nor advances, and the next action is a hard
process break in EVERY configuration. SC5 clamps every trainer's fielded party to
its authored LEAD via `uZM_TRAINER_BATTLEABLE_PARTY = 1` in
`Source/Battle/ZM_TrainerBattle.h`; the roster keeps its multi-member rows.
**Raise that constant ONLY in the commit that adds the replacement path.** This
is the headline battle-engine shortfall (Shortfalls 1.2) and it must land before
S8 can show a credible gym.

**Also open for SC6:** a flagless trainer row (`ZM_TRAINER_ROUTE1_RAMBLER`, which
carries `ZM_STORY_FLAG_NONE` on purpose) has no defeat-flag brake, so SC6's
re-engagement gate must key on a runtime latch for such rows -- see
Q-2026-07-28-001.

**Prior:** SC4 COMPLETE (ZM-D-152) -- the HUD Run-gate. It was the prerequisite for SC5: trainer forced-battle entry (`ZM_OnTrainerEncounter` + a 2nd `ZM_BattleTransition` subscription, leaving the wild validation path untouched), the `ZM_BattleDirector` trainer arm building the fixed enemy party and passing the row's `ZM_AI_TIER` to `Begin`, and `ZM_ApplyTrainerResultToGameState` (win -> `AddMoney` + set the defeat flag; loss -> the existing whiteout). Rides ECS orders 110/111.**

**SC4 removed the last blocker:** a trainer battle can now be begun without the
menu being able to break the process. SC5 is the first sub-commit that may
actually call `BuildTrainerBattleConfig()` in production.

**What SC4 pinned that SC5-SC8 must not re-litigate:** the Run gate is the
CONFIRM-boundary guard in `MenuConfirm`'s `ACTION_ROOT` arm, never the button
colour (the dim tint is cosmetic, labelled as such in three places); the root
list and every cursor index are flee-INDEPENDENT and
`MenuRootItemCount`/`MenuRootItemAtIndex`/`MenuItemCount` keep byte-identical
bodies (a removal gate could make `ZM_AutoTests_BattleMenu.cpp`'s DOWN-press loop
non-terminating); and the rule is read through
`ZM_BattleDirectorCore::IsFleeAllowed()` off the live config, never copied into
the UI. **Also discovered: `ZM_MakeTowerBattleConfig()` has long shipped
`m_bCanFlee = false`** with no production caller, so the Battle Tower would have
inherited the identical latent break the moment S11 wires it -- SC4 pre-empts
both.

**What SC3 pinned that SC4-SC8 must not re-litigate:** there is exactly ONE
facing-cone test in the game (`ZM_IsFacingXZ`, shared by the interaction picker
and the trainer sight predicate) and ONE flattening policy (`ZM_FlattenXZ`) --
do not hand-write a second dot product; the sight tuning has THREE parameters
(range, half-angle cosine, and an absolute vertical band -- see the ruling in
ZM-D-151); occlusion is NOT in the pure predicate and enters at SC6 as a glue-
layer probe filter; and the trainer config is built beside the wild one, never
by modifying it.

**Prior:** SC2 COMPLETE (ZM-D-150) -- the `ZM_TrainerData` roster + `ZM_STORY_FLAG_RIVAL1_DEFEATED` at wire bit 6.

**What SC2 pinned that SC3-SC8 must not re-litigate:** a trainer party row stores
ONLY `{species, level}` per member (`ZM_BuildWildEnemySpec` derives the rest with
zero randomness, so that pair already IS a fixed team); the roster is TWO rows on
purpose (Vesper carries a defeat flag, "Rambler Perrin" carries
`ZM_STORY_FLAG_NONE`, so both column shapes stay covered); every accessor is
TOTAL and returns the inert `UNKNOWN` row rather than asserting; and
`ZM_IsMilestoneStoryFlag` was deliberately NOT extended to the new flag --
uncalled surface ships with its producer at SC5/SC6.

**Prior:** S7 item 3 SC1b COMPLETE -- both commits landed (ZM-D-146 Null backend, ZM-D-147 navmesh persistence).

**Commit A shipped the platform SC1b needs, and it is engine-wide:**
- **`Zenith/Null` -- a GPU-less render backend, and headless is now a BUILD CONFIG.** `--headless` is deleted end-to-end. A `Null_*` config defines `ZENITH_NULL_RENDERER`, compiles `Zenith/Null` instead of Vulkan and hides the window. The difference that matters: the old flag SKIPPED the render paths, the Null backend RUNS them against no-ops -- so a headless run exercises the same code a windowed one does. `zenith build|test <G> --headless` survives as a CONFIG SELECTOR; **test discovery always uses the Null exe**. Compile-time checks use the constexpr `Zenith_IsNullRenderer()`.
- **Q-2026-07-21-001 CLOSED.** The headless-terrain assert was never about missing baked content (already graceful) -- it was the culling-buffer allocation, and it reproduced with terrain FULLY BAKED. CityBuilder now authors its terrain entity in EVERY config and runs 45/45.
- **`m_bRequiresGraphics` re-audited: 74 -> 25.** The flag now means "the assertions read GPU-produced output". **Zenithmon's headless suite went from executing 3 of 42 tests to 31** -- its old "42/42 green" was covering three tests, because a skip counts as a pass.

**Commit B (ZM-D-147) shipped the reusable ENGINE feature, with Zenithmon as its first consumer:**
- **`Zenith_NavMeshBaker`** (tools-time: generate -> serialize -> write -> **read back + memcmp**, because `WriteFile` returns void so the bytes on disk are the only truthful success signal), **`Zenith_NavMeshStats`** (the numbers the editor panel formats, so it cannot drift from the mesh), a **hardened validating `Zenith_NavMesh` load path** (every count/index checked BEFORE any `Reserve` or indexed `Get`; a refused load leaves an EMPTY mesh, never a half-populated one), and **`Zenith_NavMeshComponent` at ENGINE order 96** which OWNS its mesh -- no cache, no statics, lifetime = the scene's -- plus a full TOOLS debugging panel (stats, six visualisation toggles, point + path probes). **+28 engine units.**
- **Zenithmon consumes it:** `Source/Nav/ZM_NavBake` bakes Dawnmere at 16 m from SC1's pure coverage grid; the bake step sits in the ALWAYS-RUN FrontEnd authoring section (the Dawnmere block needs warm terrain, which a fresh clone's first boot does not have) and is skipped on Null builds, so CI loads COMMITTED bytes and never re-authors them.
- **TRACKED: `Dawnmere.znavmesh` + `Dawnmere.zscen` only** -- the four-scene plan was cut on evidence, see the churn finding below.
- **Registry 42 -> 44**, both new tests RUNNING headless (no `RequestSkip`, no `m_bRequiresGraphics` -- a deliberate TestPlan C6 deviation, because a committed asset's absence is a defect and a skip counts as a pass).

**★ THE HEADLINE: the adversarial review found FIVE real defects that a fully green gate had missed.** Six games building, 44/44 headless and windowed, 2546 boot units, ratchets byte-identical to pristine HEAD -- and still: (1) the reader required `neighbourCount == vertexCount`, but **`StitchPortalAt` deliberately appends a phantom neighbour past the vertex count** and DP's `DPDoor` uses it, so baking and reloading a stitched mesh would have died process-level **on correct data** (the baker's read-back `memcmp` can never catch this class -- it compares bytes, it does not re-parse); (2) `SetAssetRef` + the deferred `OnStart` **double-loaded**, freeing and re-allocating and dangling any cached `GetNavMesh()`; (3) `m_bMovedOut` was never cleared by the load path, leaking on a revived moved-from component; (4) a failed bake **left the corrupt file on disk** -- and it writes straight into the committed asset; (5) two tests could not fail (a literal `ZENITH_ASSERT_TRUE(true, ...)`, and comments claiming a component-count check detects a MESH leak, which it cannot). All five fixed, with a new regression unit pinning the phantom-neighbour round trip.

**★ AND the `.zscen` determinism probe was under-powered.** It repeated ONE boot shape three times and reported byte-identical scenes, so all four were staged for commit. The review found `Battle.zscen`/`PlayerHome.zscen` had since gone `AM` (4 and 12 entity-index-shaped bytes). Mechanism, confirmed by experiment: **authoring bakes in entity indices assigned during that boot, and the boot-time unit suite allocates entities first** -- re-running the original boot shape reproduced the staged bytes exactly. So any commit adding an entity-creating boot unit re-authors different scene bytes. **Consequence:** FrontEnd/Battle/PlayerHome stay ignored (CI re-authors them anyway); `Dawnmere.zscen` is tracked by EXACT PATH because it is the one scene a Null/CI boot never authors. Logged as Q-2026-07-25-001.

Then SC2 `ZM_TrainerData` + `ZM_STORY_FLAG_RIVAL1_DEFEATED` -> SC3 sight cone -> SC4 HUD Run-gate -> SC5 forced-battle entry -> SC6 sight FSM/occlusion glue -> SC7 first `.bgraph` -> SC8 rival Vesper. Eight design defaults are logged in **Q-2026-07-24-002**. ECS order 113 remains the last occupied GAME order (next free **114**); the navmesh component took an ENGINE order (96). Continue autonomously; the next human stop is the **S8 vertical-slice go/no-go**.

**PER-SC GATE -- run in this exact order, every time:** `Build\regen.ps1` (ONLY when a new .cpp or folder was added) -> `zenith build Zenithmon` -> `zenith build Zenithmon --headless` (the Null exe every gate below runs) -> `zenith test Zenithmon --headless` -> `Tools\run_unit_gate.ps1 -Exe <NULL exe> -Baseline <N> -TimeoutSec 300` (the 300 s timeout-kill is EXPECTED -- `--exit-after-frames` only applies while the test harness is stepping, so a no-test boot never exits on its own) -> full windowed `zenith test Zenithmon`. **Two standing tripwires:** (a) never write a PREDICTED unit count into `zm-tests.yml` -- only the OBSERVED one; (b) the engine baseline (now **1121**) moves only when an explicitly-scoped engine change owns the cross-game gate.

**S7 ITEM 2 SUB-COMMIT PLAN (6 total -- COMPLETE):**
- **SC1 DONE (ZM-D-137)** -- `ZM_StoryFlags` identity registry + flag-gated NPC lines + the `Npc_Warden` row.
- **SC2 DONE (ZM-D-138)** -- `ZM_SaveSlots`, the typed slot/disk layer over the frozen codec (Save0-2 + Auto).
- **SC3 DONE (ZM-D-139)** -- world-position capture, resume placement, quit-to-FrontEnd, the milestone autosave latch.
- **SC4 DONE (ZM-D-140)** -- the save/load slot presenter, manual Save0-2 flow and root-menu Save/Quit.
- **SC5 DONE (ZM-D-141)** -- the title menu, New Game, transactional Continue, the slot-operation test observer, and the disk-authentic Continue gate (`ZM_SaveContinue_Test`).
- **SC6 DONE (ZM-D-142)** -- the milestone-autosave producer gate `ZM_MilestoneAutosave_Test` (test-only; the disk-backed restoration gate landed with SC5). See "Last completed".

**S7 ITEM 1 (complete):** SC1 (ZM-D-135) froze `ZM_GameState`'s LAYOUT -- reach it with named free functions, never new members -- and SC2 (ZM-D-136) froze the pure transactional 11-module schema-v1 codec plus the exact **824-byte** v1 artifact. Every incompatible change from here owes a version bump + a literal historical-blob migration test IN THE SAME COMMIT.

**Architecture (fixed, do not re-litigate):** exactly **ONE ECS order is consumed for interaction -- 113 (`ZM_Interactable`)**; the NPC walker is a by-value member of it and `ZM_InteractionRuntime` is a by-value member of `ZM_PlayerController`. **Next free ECS order: 114.** Screens are by-value non-ECS presenters on `ZM_UI_MenuStack` (order 112), so a new screen is one arm per dispatch switch and costs no ECS order. **Save layering is FOUR tiers and the DIRECTORY IS THE BOUNDARY:** `Source/Party/ZM_GameState` (frozen model) -> `Source/Core/ZM_SaveSchema` (pure ZMSV codec, names no file or slot) -> `Source/Save/ZM_SaveSlots` (slots, files, the engine save layer; adds nothing to the payload) -> `Source/Save/ZM_ResumePoint` + `ZM_Autosave` (the pure placement and autosave DECISIONS, naming no ECS type), with `ZM_GameStateManager` as the one impure ECS/physics/scene reach on top. Interaction is a forward CONE, never a raycast -- not for a headless reason (**physics IS live headless**; ZM-D-127 corrected that false claim) but because the cone stays pure and unit-testable; S7's trainer occlusion ray enters as a probe filter in the GLUE layer, leaving the pure picker untouched. Five authored Dawnmere NPCs only (villager / Trade Post clerk / Caretaker / wanderer / **warden**) -- populated towns are S9/S10. **"Trade Post"**, never "Mart", in data/entity/asset names. NO RNG in the walker (TestPlan C8).

**S6 CLOSURE RULING (ZM-D-134, still binding):** behaviour graphs and navmesh-driven wandering were deferred to S7 -- `ZM_GraphAuthoring` is not written and S6 ships a bounded 3-arm C++ role dispatch behind one `Interact()` seam. `Zenith_NavMeshGenerator::GenerateFromGeometry` is terrain-capable when supplied suitable triangles, but `Zenith_AINavGeometry::GenerateFromScene` does **not** harvest streamed terrain geometry or a heightfield. S7 item 3 owns the first useful graph integration plus the terrain-triangle/grid-coverage and `.znavmesh` evaluation. `MasterPlan.md` is historical/read-only.

**PER-SC GATE -- run in this exact order, every time:** `Build\regen.ps1` (ONLY when a new .cpp or folder was added) -> `zenith build Zenithmon` -> `zenith test Zenithmon --headless` (heals DLLs) -> `Tools\run_unit_gate.ps1 -Exe ... -Baseline <N> -TimeoutSec 300` (the 300 s timeout-kill is EXPECTED) -> full windowed `zenith test Zenithmon`. **Two standing tripwires:** (a) never write a PREDICTED unit count into `zm-tests.yml` -- only the OBSERVED one from the boot log; (b) the engine baseline **1121 must remain unchanged** unless an explicitly-scoped engine change owns the cross-game gate.

## Last completed

**S7 item 3 SC5 -- TRAINER FORCED-BATTLE ENTRY + PRIZE/DEFEAT WRITE-BACK
(ZM-D-153).** The vertical's core. New pure leaf
`Source/Battle/ZM_TrainerBattle.{h,cpp}` (party built from the row through the
shipped deterministic `ZM_BuildWildEnemySpec`; a domain-salted FNV-1a seed
provably disjoint from the wild seed space) lets boot units prove the party, tier
and seed WITHOUT starting a battle. `ZM_OnTrainerEncounter` + a SECOND
`ZM_BattleTransition` subscription (order 110) leave the wild event, validator and
subscriber untouched; a 5-line trainer PREFIX to `ZM_BattleDirector::RunSetup`
(order 111) dispatches and returns, leaving the shipped wild body byte-unchanged.
`ZM_ApplyTrainerResultToGameState` routes through the SAME
`ZM_ClassifyBattleResult` the wild path uses and credits via `AddMoney` (the sole
cap enforcer), reporting the OBSERVED delta so a saturated credit is honest.
`ZM_GameState` gains no member. +13 units (**2644 -> 2657**), +1 automated test
(registry **44 -> 45**, running headless in 86 frames), windowed **45/45**.
Mutation-proven: party bound 2 reds 2 units, inverted win check reds 5, doubled
prize reds 3.

**★ THE REVIEW FOUND A LATENT PROCESS BREAK, NOT A STYLE ISSUE:** the engine has
no faint-replacement, and SC2's roster already authored a 2-member trainer, so
SC5 as first written would have broken the process on the first enemy KO. See
the clamp note above. **A second finding killed a vacuous assertion:** the
"row's AI tier reached `Begin`" clause could not fail, because Vesper's GREEDY is
simultaneously the wild arm's literal and the pre-`Begin` default -- fixed by a
second round trip on a RANDOM-tier row plus two anti-vacuity guards.

Prior: **S7 item 3 SC4 -- THE HUD RUN-GATE (ZM-D-152).** A REFUSE-IN-PLACE guard in
`MenuConfirm`'s `ACTION_ROOT` arm, consulting the new total pure predicate
`MenuRootItemIsAllowed(eItem, bCanCatch, bCanFlee)` and returning the established
`{CONFIRM_NONE}` refusal BEFORE the Fight/Catch/Run if-chain -- so a forbidden
entry can never become a `ZM_BattleAction`, however the cursor got there and
whether or not anything was rendered. `ZM_BattleDirectorCore::IsFleeAllowed()`
surfaces the rule off the live config (sibling of `IsCatchAllowed()`, ZM-D-131).
5 files modified, no new TU (no regen owed), +6 units (**2638 -> 2644**),
headless and windowed both **44/44**.

**Review closed the hole that mattered:** nothing proved a MOVE still submits
when `m_bCanFlee` is false -- the only action a trainer battle has left once Run
is gated -- so hoisting the guard to the top of `MenuConfirm` would have left the
suite green while making a no-flee battle UNPLAYABLE. **Mutation-proven
two-sided:** transposing the two flags reds 6 units, inverting the RUN arm reds
10.

Prior: **S7 item 3 SC3 -- THE PURE TRAINER SIGHT CONE + THE ONE SHARED CONE PRIMITIVE
(ZM-D-151).** `Source/Interaction/ZM_TrainerSightLogic.{h,cpp}` ships
`ZM_TrainerSightTuning` and two TOTAL predicates (forward-vector and quaternion
forms). The cone maths was **EXTRACTED, not duplicated**: `ZM_FlattenXZ` and the
new `ZM_IsFacingXZ` were promoted out of `ZM_InteractionLogic.cpp`'s anonymous
namespace to external linkage, and the picker's fused block became one
`if (!ZM_IsFacingXZ(...)) { continue; }`. **All 44 shipped interaction units were
left untouched and stayed green** -- that is the behaviour-preservation net.
`ZM_BattleDirector::BuildTrainerBattleConfig()` lands beside the wild helper
(which is not touched by one character) and deliberately has NO CALLER until
SC4's Run-gate. +31 units, baseline **2607 -> 2638**; headless and windowed both
**44/44**.

**★ THE MUTATION BATTERY RETRACTED A CLAIM RATHER THAN CONFIRMING ONE.** Three
mutations, each rebuilt and re-gated: dropping `std::fabs` from the sight
vertical band RED, flipping that band's `>` to `>=` RED -- and respelling the
extracted comparison `dot >= min` back to `!(dot < min)` **GREEN, when the
authored comments asserted in three files that this was "THE ONE DELIBERATE
BEHAVIOUR CHANGE SC3 MAKES"**. It is not: on every input the suite exercises,
non-finite included, the two spellings answer the same, so the extraction is not
known to differ from the pre-SC3 block on ANY input. All three comment blocks
were corrected to the measured result (and claim no mechanism, since none was
measured); the unit was kept and re-documented as a fail-closed CONTRACT pin
rather than a mutation pin. **Do not restore the retracted narrative.**

Prior: **S7 item 3 SC2 -- THE `ZM_TrainerData` ROSTER + `ZM_STORY_FLAG_RIVAL1_DEFEATED`
(ZM-D-150).** Pure, headless, `Games/Zenithmon`-only: zero engine files, zero new
ECS orders (114 still next-free), no save-schema version bump.
`Source/Data/ZM_TrainerData.{h,cpp}` ships the append-only `ZM_TRAINER_ID`
(`ZM_TRAINER_NONE = ZM_TRAINER_COUNT`, so one `<` rejects the sentinel and all
garbage together), a deduced-bound `s_axTrainers[]` under a row-count
`static_assert`, and four TOTAL accessors that return an inert `UNKNOWN` row with
a NON-fatal `Zenith_Error` instead of asserting. Two rows: **Vesper** (one L5
KINDLET -- the Fire counterpart to the Grass FERNFAWN starter -- 500 prize,
`RIVAL1_DEFEATED`, GREEDY) and **"Rambler Perrin"** (two L4 Route-1 species, 120
prize, `ZM_STORY_FLAG_NONE`, RANDOM), because one row would leave the
no-defeat-flag column arm untested. `ZM_STORY_FLAG_RIVAL1_DEFEATED = 6u` is dense
and append-only; `COUNT`/`NONE` follow to 7 automatically and the name-table
`static_assert` makes a forgotten registry row a COMPILE error.
**+18 boot units** (17 `ZM_Data` + 1 `ZM_Story` freezing all seven wire bits as
hand-typed literals), baseline **2589 -> 2607**; headless registry unmoved at
**44** (correct -- a pure data sub-commit adds no automated test); full windowed
**44/44**. **Adversarial review killed a toothless unit before it landed**
(`Trainer_EveryPartyMemberHasAMoveAtItsAuthoredLevel` was a tautology:
`ZM_BuildWildEnemySpec` copies species/level straight through and `ZM_Learnsets`
always emits a level-1 STAB pick) and caught that two new TUs make
`Build\regen.ps1` MANDATORY -- no other TU references the new accessors, so
without it the build goes green with the whole table silently absent. **The +18
delta is the proof regen took; +1 would have meant it did not.**

Prior: **S7 item 3 SC1b -- BAKED NAVMESH PERSISTENCE AS A REUSABLE ENGINE FEATURE
(ZM-D-147); SC1b COMPLETE.** Engine: `Zenith_NavMeshBaker` (bake + read-back
verify), `Zenith_NavMeshStats`, a hardened validating `Zenith_NavMesh` load path
(bool-returning, asserts + logs + defined failure at every rejection, all
validation ordered ahead of every `Reserve`/`Get`), `Zenith_NavMeshComponent` at
ENGINE order **96** owning its mesh with a full TOOLS debugging panel, a
flags-driven `DebugDraw`, `ZENITH_NAVMESH_EXT`, and **+28 engine units**.
Zenithmon: `Source/Nav/ZM_NavBake`, the always-run bake step, the authored
Dawnmere holder entity, **+3 boot units** and **+2 automated tests** (registry
**42 -> 44**, both RUNNING headless). Tracked: `Dawnmere.znavmesh` (SHA256
`A783FB0A...`) and `Dawnmere.zscen` (`7337853F...`). Baselines **1093 -> 1121**
(engine) and **2515 -> 2546** (ZM boot), both from the OBSERVED line. Six
mutations proved teeth. **The review pass found five real defects behind a fully
green gate** -- the `StitchPortalAt` phantom-neighbour invariant (which would
have killed DP's stitched meshes on reload), a `SetAssetRef`/`OnStart`
double-load, an un-cleared `m_bMovedOut`, a failed bake leaving a corrupt file
on the tracked asset, and two tests that could not fail -- all fixed, plus the
`.zscen` churn finding that cut the tracked-scene set from four to one.

Prior: **S7 item 3 SC1 -- NAVMESH TERRAIN-SOURCE EVALUATION SPIKE (ZM-D-144).** Pure,
headless. `Source/Nav/ZM_NavEval.{h,cpp}` harvests a FLAT coverage grid (2
tris/quad) at Dawnmere's sampled ground height (25.98577) over the recipe's
**1024 m export sub-rect** and feeds the raw soup to
`Zenith_NavMeshGenerator::GenerateFromGeometry` -> a walkable navmesh (~4225 polys
at a 16 m cell). It NEVER constructs a live `Zenith_TerrainComponent` (sidesteps
Q-2026-07-21-001's headless VRAM assert) and reads NO disk asset; it fails closed
(NEVER asserts on args) on any cell size finer than the generator's `iMaxDim=1024`
clamp (recommended floor ~1.0 m). **`.znavmesh` persistence + runtime routing
DEFERRED** (Q-A) -- this is the EVALUATION, answered YES. 4 `ZM_Nav` boot units
(band **3969..4489** matched the real generator output; fail-closed 0.3 m
rejection + 8 m control; all-vertical -> zero walkable + upward control; the
1024 m sub-rect vs the 4096 m grid). **Teeth mutation-proven** (winding flip ->
units 1/2-ctrl/3-ctrl red, unit 4 green; restored). Boot **2521 -> 2525 / 2524 /
0 / 1**; `zm-tests.yml` 2521 -> 2525; headless registry unchanged **42/42**;
engine ref **1103** untouched. **NEXT = SC2.**

Prior: **S7 item 2 SC6 -- THE MILESTONE-AUTOSAVE TEST CLOSURE; S7 ITEM 2 COMPLETE
(ZM-D-142).** Landed the windowed `ZM_MilestoneAutosave_Test`
(`Tests/ZM_AutoTests_SaveAutosave.cpp`), the milestone-autosave PRODUCER gate --
a **TEST-ONLY** change (no production or engine code ships). Found IN FLIGHT and
uncommitted on master at session start (a prior session's work); an orchestrated
forensic assessment (spec + build-correctness + test-teeth lenses,
cross-verified) found it build-correct with **NO phantom seam** (unlike SC5's
in-flight file) and genuinely passable, so it went straight to the gate.

**What it pins BEYOND SC3** (which sampled only the autosave counter + the Auto
slot STATUS enum): (1) FULL DURABLE STATE -- a real `SCENE_ENTERED` arrival's
Auto slot is `ReadState`'d back FROM DISK and field-compared to a scrambled live
state (party/boxes/dex/story bits/badges/money/world position+yaw), not just
`ProbeSlot==READY`; (2) EXACTLY-ONCE twinned with SC5's slot observer -- one
`WRITE_STATE` on AUTO (==1) alongside the +1 counter; (3) POSITIVE DRAIN-PHASE --
the write is attributed to the `OnUpdate` IDLE drain via menu-term isolation
(`blocker==NONE` with the ROOT menu open); (4) ATTRIBUTABLE BLOCKED REAL ARRIVAL
-- a genuine arrival under a live non-transition blocker with a proven-capturable
player writes NOTHING (byte-identical Auto file, zero WRITE, EMPTY); (5) LATCH
RE-ARM + consume-before-attempt (`NoRetryWatch` -- no disk-hammering retry).

**Disk-authentic + stack-safe:** `ProbeSlot`/`ReadState` fall through to the real
file (the RAM readback stash is never staged), defeating the `DontDestroyOnLoad`
hazard; 12 per-phase driver functions with the two large `ZM_GameState`
instances as FILE-SCOPE globals avoid SC5's 1.31 MB monolithic-Step overflow.

**★ THE TEETH ARE MUTATION-PROVEN.** Five production mutations, each rebuilt in
isolation, all confirmed RED, then reverted + re-gated GREEN: DROP-CAPTURE reds
the disk-content/scene-tag/resume-valid asserts (the SC3 status-only green hole)
while counter+READY stay green; DROP MENU CONSULT reds the menu-probe +
blocked-arrival asserts; CONSUME-AFTER-SUCCESS reds `NoRetryWatch`; DELETE THE
DRAIN reds the `SamplePositive` poll deadline early at **45 frames**; DOUBLE-WRITE
reds the observer `traceExact==1`. Two `counterDelta` booleans are tautological
given their poll gates -- no false-green (the deadline carries the teeth),
deliberately not churned (matching ZM-D-141's low/low disposition).

**Evidence.** Regen GREEN (new untracked TU); build GREEN; headless **42/42, 0
failed** (registry **41 -> 42**; the windowed test skips headless); boot unit
gate **2521 / 2520 / 0 / 1 documented skip -- NO delta** (only a
`ZENITH_AUTOMATED_TEST_REGISTER`, so `zm-tests.yml` is NOT bumped and the engine
reference **1103** is unchanged); focused windowed **134 frames**; full windowed
**42/42 passed, 0 failed, 0 skipped, 0 zero-frame** (42 clean result JSONs); save
directory EMPTY; no stray `zenithmon.exe`. **★ CI-INVISIBILITY:**
`m_bRequiresGraphics=true`, so the headless backstop skips it -- teeth exist only
under the LOCAL windowed gate with `--filter ZM_MilestoneAutosave_Test`.
**Contracts held:** no production code changed; `ZM_SaveSchema` + 824-byte v1
golden, `ZM_GameState` layout, `ZM_SaveSlots` framing, and the latch/drain
byte-untouched; `uSERIALIZATION_VERSION` 1; **no new ECS order (114 next-free)**;
a phantom `Zenith_TerrainComponent.cpp` working-tree entry (blob byte-identical
to HEAD) was restored, not committed.

Prior: **S7 item 2 SC5 -- THE TITLE MENU, NEW GAME, TRANSACTIONAL CONTINUE, THE
SLOT-OPERATION TEST OBSERVER, AND THE DISK-AUTHENTIC CONTINUE GATE (ZM-D-141).**
Found IN FLIGHT and uncommitted on master (runtime complete; the observer seam
the new windowed test referenced had never been written, the test was logically
unpassable and partly vacuous); finished, gate-proven and landed by this
session. `Source/UI/ZM_UI_TitleMenu.{h,cpp}` is the FrontEnd title presenter --
by-value, non-ECS, one arm per dispatch switch on `ZM_UI_MenuStack` (order 112),
so **113 remains last occupied and 114 next-free**. TITLE is AMBIENT: auto-raised
only on a settled FrontEnd with an empty stack and no warp/battle owning the
screen, and force-closed the instant any of that stops being true, so it can
never fight the pause menu or a transition. Continue is visible iff ANY slot
probes non-EMPTY (DAMAGED counts, matching `AnySlotOccupied`); New Game is
always live; navigation is rebuilt every Present so no link targets a hidden
Continue, and focus is repaired onto the live default.

**Continue is transactional and reads disk exactly once.**
`ZM_GameStateManager::RequestContinue` runs `ZM_SaveSlots::ReadState` into a
LOCAL candidate, `QueueResume(candidate.m_xWorldPosition)` through SC3's
ordinary validated placement path, then publishes the candidate LAST; any
failure returns the exact `Zenith_ErrorCode` with live state and the transition
machine untouched. The Yes/No load prompt arms only from LOAD mode against a
row that still probes READY; YES performs the one definitive `RequestContinue`.
`RequestNewGame()` builds the starter, queues the Dawnmere warp, and publishes
the starter LAST. No second codec, slot reader, or placement path; LOAD remains
ungated by the overworld-only SAVE predicate.

**The slot layer gained a test-only operation observer** --
`ZM_SAVE_SLOT_OPERATION_FOR_TESTS` {PROBE_SLOT, READ_STATE, WRITE_STATE}, a
plain-function-pointer `SetOperationObserverForTests`, one event ON ENTRY per
public API call (a refused attempt is still observed once) -- because the
disk-authentic claim needs disk-layer evidence twinned with the menu's
`GetLoadReadCount()`. The global defaults nullptr so shipped behaviour is
byte-for-byte unchanged when unset, and `DeleteAllSlotsForTests()` clears it
FIRST (the between-tests hook needed no edit; TestPlan C3 holds).

**★ THE HEADLINE DEFECT: the monolithic-`Step` idiom hit the stack reserve.**
`Step_ZMSaveContinue`'s 29 phases aggregated ~six `ZM_GameState` locals
(~150-200 KB each: 6-mon party + 16x30 box storage) into a measured
**1,312,136-byte** /Od frame against the exe's **1 MB** stack reserve -- the
process died in `__chkstk` on the FIRST Step call (exit -1073741571 =
STATUS_STACK_OVERFLOW, diagnosed via crash-dump analysis). Fixed structurally:
28 per-phase driver functions (each <= ~2 `ZM_GameState`), a thin dispatch
Step, and a `SCResetGameState` helper for Setup. **Standing rule: any
multi-phase automated test touching `ZM_GameState` uses per-phase functions,
never one giant Step.**

**Evidence.** **6** `ZM_Title` + **2** `ZM_MenuStack` boot units took the gate
**2513 -> 2521**: **2521 ran / 2520 passed / 0 failed / 1 documented skip**;
`zm-tests.yml` bumped from the OBSERVED line. Registry **40 -> 41** with
`ZM_SaveContinue_Test` (**247 frames**): real-input New Game publishes a fresh
starter over an installed canary; a busy queue refuses `RequestContinue` with
exactly one READ + `QUEUE_FULL`; after quit-to-title, Continue stays visible
with ONLY a DAMAGED slot on disk; the Auto fixture is restored from
pre-deletion bytes; DAMAGED/EMPTY rows refuse with a plain line, never an armed
choice; pre-Yes the live state is still the scramble; the Yes window performs
exactly ONE `READ_STATE` on AUTO and ZERO writes; published state equals the
saved fixture and NOT the scramble; the restored pose lands within 0.05/0.10/
0.05 of saved, >= 2 m from both TownCenter and the scramble pose. Regen GREEN;
build GREEN; headless **41/41**; focused `ZM_SaveContinue_Test` **247**,
`ZM_RootQuitAndBlockedSave_Test` **158**, `ZM_SaveMenuFlow_Test` **98**; full
windowed **41/41 passed, 0 failed, 0 skipped, 0 zero-frame**; save directory
EMPTY. Adversarial review CLEAN (two low/low nits: per-frame `SetFocusable`
writes in title `Present`; `std::filesystem` in the test-only fixture path --
both logged, deliberately not churned). **Contracts held:** `ZM_SaveSchema` +
824-byte v1 golden byte-untouched; `ZM_GameState` layout frozen; `ZM_SaveSlots`
framing + write-answers-from-re-probe untouched; `uSERIALIZATION_VERSION` stays
1; no new ECS order.

**Item-2 boundary (now closed):** SC6 (ZM-D-142) shipped
`ZM_MilestoneAutosave_Test`, completing all six sub-commits; the disk-backed
restoration gate had landed here as `ZM_SaveContinue_Test`. **S7 item 2 is
COMPLETE.**

Prior: **S7 item 2 SC4 -- THE SAVE/LOAD SLOT PRESENTER, MANUAL SAVE FLOW AND
ROOT-MENU SAVE/QUIT (ZM-D-140).** `Source/UI/ZM_UI_SaveSlots.{h,cpp}` is the ONE
by-value, non-ECS presenter for both SAVE and LOAD: four always-present rows
backed by uncached `EMPTY / READY / DAMAGED` probes plus Back; SAVE mode writes
only Save0-2 (EMPTY immediately, READY/DAMAGED only after an input-driven Yes/No
overwrite confirmation; DAMAGED is surfaced, never repaired/deleted/auto-
overwritten); `Auto` is read-only in SAVE and loadable when READY in LOAD;
`ResolveLiveSaveBlocker` is checked at SAVE opening AND at the irreversible
`blocker -> CaptureWorldPosition -> WriteState` boundary; ROOT gained Save/Quit
with immediate focused-Save rehome to Quit under a live blocker; Quit is an
ACTION (Yes -> SC3's playerless `RequestQuitToFrontEnd()`). Evidence: **23**
save-screen + **5** menu-stack units (boot **2513 / 2512 / 0 / 1**), registry
**40**, headless **40/40**, focused **98**/**146** frames, full windowed
**40/40**, save dir empty.

Prior: **S7 item 2 SC3 -- WORLD-POSITION CAPTURE, RESUME PLACEMENT, QUIT-TO-FRONTEND AND
THE MILESTONE AUTOSAVE LATCH (ZM-D-139).**
New `Source/Save/ZM_ResumePoint.{h,cpp}` (pure validation + world-position
construction + the yaw conversions) and `Source/Save/ZM_Autosave.{h,cpp}` (the
autosave policy + its one live entry point), plus ~450 added lines on
`Components/ZM_GameStateManager.{h,cpp}` (capture, the resume latch, the
playerless destination, the autosave drain). **Before this,
`ZM_GameState::m_xWorldPosition` was written by NO runtime code anywhere in the
tree, and nothing recorded which spawn tag the player had arrived at** --
`m_szTargetSpawnTag` is the tag of an IN-FLIGHT warp and `ResetTransitionState`
memsets it the moment the warp finishes.

**The pure/impure split is why any of it is testable.** Validation, world-position
construction, the yaw conversions and the autosave predicate name NO ECS type,
component, scene handle or physics body, so the whole decision surface is pinned
by headless boot units with no scene loaded (`ZM_SpawnPoint::IsTagValid` is passed
IN as a bool for exactly that reason). Everything impure -- the unique player, its
body pose, the active scene, `ResolveLiveSaveBlocker` -- lives on the manager and
calls DOWN. Every function in both new TUs is TOTAL: the units feed them NaNs,
oversized tags, the UNSET sentinel and unresolvable build indices ON PURPOSE, and
`ZM_ValidateResume` evaluates SCENE -> TAG -> TRANSFORM so `ZM_GetWorldSpec`
(which asserts fatally on `ZM_SCENE_NONE`) is only ever reached after the index
resolved.

**`SaveFormat.md`'s transform-vs-spawn-tag TBD is RESOLVED: TRANSFORM-FIRST,
SPAWN-TAG FALLBACK -- and the fallback costs nothing because it is already on the
path.** A resume rides the ORDINARY validated `TryQueueWarp`: same fade, same
single load, same marker placement. `INVALID_TRANSFORM` is RECOVERABLE (scene+tag
are a complete destination, so the marker placement simply stands, with no second
placement path to keep in sync); `INVALID_SCENE`/`INVALID_TAG` refuse. The
interesting tag failure is a tag ANOTHER scene offers -- it passes grammar and the
warp validator, then WEDGES the transition in `WAITING_FOR_SPAWN` forever -- which
is why validity is checked against the destination's `ZM_WorldSpec` tag list.

**Two conventions that must never be mixed.** The captured position is the capsule
CENTRE (`Zenith_Physics::GetBodyPosition`, written straight back on restore);
spawn MARKERS store FEET and `CalculateSpawnCenter` adds the 0.9 m half-extent for
the authored 1.8 m player -- the two meet in that one function and nowhere else,
and confusing them is a silent 0.9 m sink/float. And the restore uses
**`SetBodyPosition`, NEVER `TeleportBody`**, because TeleportBody forces IDENTITY
rotation and would discard the yaw; rotation is written AFTER position, and the
restored yaw SURVIVES `ZM_PlayerController`'s per-frame `EnforceUpright` because
that rebuilds a Y-axis-only quaternion and PRESERVES yaw -- so restoring facing is
a real contract, not best effort. Yaw is `atan2` of the quaternion-rotated +Z,
never `glm::eulerAngles` (which collapses past 90 degrees off +Z). Placement sits
AFTER the marker teleport and BEFORE the camera barrier, and ends with
`SyncPhysicsPoseAndInvalidate` because `SetBodyPosition`/`SetBodyRotation`
deliberately do NOT fire the pose-changed hook `TeleportBody` does. Capture is
transactional, and falls back to the scene's FIRST offered tag when no transition
recorded an arrival -- load-bearing, not defensive: the boot path and every direct
`LoadSceneByIndex` enter without a warp, and the codec rejects an empty tag on a
set scene index, so such a game would otherwise be UNSAVEABLE.

**Quit-to-FrontEnd needed TWO bypasses, not one.** FrontEnd authors no Player, no
`ZM_SpawnPoint` and no `ZM_FollowCamera`, and `AdvanceFadeIn` carries two
INDEPENDENT barriers: `TryResolveFrozenTargetPlayer` (which bounces the state back
to `WAITING_FOR_SPAWN`, which bounces straight back) and, separately,
`HasUniqueReadyFollowCamera`. Patching only the spawn poll would have left the
transition ping-ponging forever on a permanently opaque screen. The playerless
flag is latched ONLY on `TryQueueWarp`'s accept line and cleared only by
`ResetTransitionState`; the arrival tail is SHARED by both paths so there is
exactly one, not two that drift.

**The autosave latch is edge-triggered and drained from `OnUpdate`.** It asks
SC2's blocker policy rather than re-deriving one, comparing against `NONE` rather
than listing arms (so a blocker appended later is honoured with no edit), and adds
only "no menu open" on top. It cannot fire from the fade-in tail --
`ResolveLiveSaveBlocker` consults `IsWarpInProgress()`, true for EVERY non-IDLE
state, so an in-tail autosave always resolves `WARP` and silently never saves --
so the arrival latches and `OnUpdate` drains once IDLE, consuming the latch BEFORE
the attempt (a retry is a disk-hammering loop, not a recovery). All five milestone
arms ship in the enum so it never needs renumbering, but exactly ONE
(`SCENE_ENTERED`) is LIVE today.

**★★ DEFECT 1 -- THE RESUME LATCH WAS SPENT BY THE FIRST PLACEMENT ATTEMPT.** It
was cleared BEFORE validating and applying the pose. But
`PollForSpawnAndPlacePlayer` can run MORE THAN ONCE per transition -- both
`AdvanceFadeIn` and `PollForCameraAndBeginFadeIn` push the state back to
`WAITING_FOR_SPAWN` when the frozen player id stops matching -- and every pass
re-runs the marker teleport. On any second pass that teleport would stand as the
final placement while the resume no longer applied, silently dropping the player
on the default spawn with nothing in the log: green today, flaky by construction.
Fixed by letting the latch die with the TRANSITION (`ResetTransitionState` already
clears it on both the success and cancel paths), so it can neither outlive its
transition nor retry forever, and every entry re-validates the pose.

**★★ DEFECT 2 -- THE MILESTONE-AUTOSAVE PRODUCER WAS PINNED BY NOTHING.** Deleting
the whole drain block left every test green, and `ZM_GetAutosaveCount()` was read
by NO test at all. Closed by asserting the counter delta AND the Auto slot's
probed status in both windowed tests (+1/READY on arrival, +0/EMPTY across the
quit).

**★★ DEFECT 3 -- THE QUIT-TO-TITLE "MUST NOT AUTOSAVE" ASSERTIONS COULD NOT CATCH
THE MUTATIONS THEIR OWN COMMENT CLAIMED.** The refusal there is OVER-DETERMINED:
the blocker policy refuses first, but deleting that policy (or whitelisting the
not-overworld blocker) just falls through to the playerless capture guard, so the
counter never moves either way and the test stays green. The comment was corrected
to say what the assertions really pin, AND a genuine integration negative was
added: with the player alive in Dawnmere and a warp in progress, the test proves
capture WOULD have succeeded, then proves `ZM_TryAutosave` refused and the counter
did not move (`blocker=3` WARP, `captureWouldWork=true`, `refused=true`, autosaves
`0->0`). The positive half is the arrival's `+1`, so neither half is satisfiable
by an autosave that never fires.

**★★ DEFECT 4 -- A FUNCTION SHIPPED WITH NO CALLER AND NO TEST** (a story-flag
autosave trigger resolver, not in the SC3 spec). Deleted; it lands WITH its real
producer, in the sub-commit that first calls it. **Also fixed:** a world-extent
unit that bracketed the guard only to `(512, 1e9]` -- the constant could have been
loosened from 4096 to 1e8 with everything still green -- now bracketed by a
hand-written 5000.0f rejection fixture; a comment naming the wrong guard as the
rejecting mechanism; three stale line citations; a missing explicit include; and a
one-frame transform-cache staleness after the pose write.

**Convention drift, honestly scoped:** three save-area test files were using
`std::vector` (forbidden -- "no `std::` containers, use `Zenith_Vector`") and were
converted to `Zenith_Vector<u_int8>` or a fixed array. A FOURTH,
`Tests/ZM_Tests_SaveSchema.cpp`, predates this work, was left OUT OF SCOPE and is
tracked separately -- the drift is **not** fully cleared.

**Evidence.** +27 pure `ZM_Save` units in `Tests/ZM_Tests_ResumePoint.cpp` and 2
new registered windowed tests in `Tests/ZM_AutoTests_SaveResume.cpp`. Regen GREEN;
build GREEN; boot **2485 / 2484 / 0 / 1**; `zm-tests.yml` bumped **2458 -> 2485**
from the OBSERVED line; engine reference **1103 UNCHANGED** (no engine file
touched, so no cross-game sweep was owed); headless **38/0**; windowed **38/0/0**,
zero skips; registered automated tests **36 -> 38**; save directory verified EMPTY.
`ZM_QuitToFrontEnd_Test` **38 frames** (players 1->0, loads 0->1, peak alpha 1.0,
final alpha 0.0, final state IDLE, autosaves 0->0, Auto slot EMPTY).
`ZM_ResumePlacement_Test` **236 frames**: spawn (512.000, 26.886, 480.000),
captured (518.092, 27.149, 471.476), planarErr 0.0000 (< 0.050), vertErr 0.0000
(< 0.100), yawErr 0.0000 (< 0.050), **10.477 m from the spawn** (> 2.000 required,
so the restore is provably NOT the spawn teleport), loads 1->2 (a REAL scene load,
so the value came from disk and not from RAM survival), autosaves 0->1, Auto slot
READY. **Mutation-verified:** suppressing the `SetBodyPosition` write turns
`ZM_ResumePlacement_Test` RED; restored, green. **Contracts held:**
`ZM_SaveSchema` untouched (824-byte golden unchanged), `ZM_SaveSlots` consumed as
shipped, `ZM_GameState` layout untouched, no new ECS order (next free **114**), no
`uSERIALIZATION_VERSION` bump -- every new manager member is SESSION state. No
commit, push or CI result is claimed.

---

Prior: **S7 item 2 SC2 -- THE TYPED SLOT/DISK LAYER OVER THE FROZEN CODEC
(ZM-D-138).** `Source/Save/ZM_SaveSlots.{h,cpp}` sits ON TOP of the ZMSV codec and
adds nothing to the payload: four slots (`ZM_SAVE_SLOT_0/1/2` + `AUTO`), a
three-state probe (`EMPTY`/`READY`/`DAMAGED`), typed `WriteState`/`ReadState`
returning `Zenith_Status`, `AnySlotOccupied`/`AnySlotReady`, `DeleteSlotFile`, and
one pure save-blocker predicate with a fixed precedence. The header names no ECS
type, scene, UI element or component; the .cpp's ONE live reach is
`ResolveLiveSaveBlocker`. **The one framing it adds:** `[u32 little-endian ZMSV
byte length][ZMSV blob]`, written BYTE BY BYTE, because `ZM_SaveSchema::Read`
demands an EXACT length while the engine's two Load paths DISAGREE about
`GetCapacity()` (disk wraps an exactly-payload-sized buffer; the staged readback
hands over a default-constructed OWNING stream whose capacity is the whole
1024-byte allocation). It carries no magic or version and leaves the **824-byte**
v1 golden untouched. **Two orderings are load-bearing:** the payload is STAGED AND
VALIDATED before `Zenith_SaveData::Save` is called at all (Save creates the file
the instant it is called), and Save's return is DISCARDED (a literal `true` on
every path), so success comes ONLY from a RE-PROBE. A DAMAGED slot is surfaced,
NEVER repaired, deleted or auto-overwritten; `AnySlotOccupied` COUNTS a damaged
slot while `AnySlotReady` is stricter. **Four defects, all now standing rules
below:** the test-slot interlock was keyed on `IsAutomatedTestRun()`, false during
the boot run -- simultaneously a total coverage hole (every disk unit skipped, and
a skip counts as a PASS) and a data-loss hazard against the player's real save
files; the layer's own rejection branches were dead to the tests (a disk byte-flip
fails the ENGINE CRC before the read callback runs); the verify re-probe was
unpinned (mutation-verified RED once a fixture reached it); and the
too-small-for-a-prefix guard had zero coverage until a hand-built `.zsave` with a
matching CRC reached it. +33 units; boot **2458 / 2457 / 0 / 1**; windowed
**36/0/0**.

Prior: **S7 item 2 SC1 -- STORY-FLAG IDENTITY REGISTRY + FLAG-GATED NPC LINES
(ZM-D-137).** `Source/Data/ZM_StoryFlags.{h,cpp}` gives the ZM-D-135 bitset an
identity: a SAVE-STABLE `enum ZM_STORY_FLAG_ID : u_int` whose **value IS the
persisted bit index in save-schema module 4**, six flags dense from zero, APPEND
ONLY, with a DEDUCED-bound compiled row table (a missing row is a COMPILE error),
free-function accessors (because `ZM_GameState` is frozen) and a TOTAL fail-closed
`ZM_StoryGatePasses`. `ZM_IsMilestoneStoryFlag` is authored beside it and remains
UNCONSUMED -- SC3's autosave ships its own trigger enum, and the story-flag
producer lands with the S8 story beats. First gameplay consumer: `ZM_NpcData`
gains three fields APPENDED AT THE END, a pure `ZM_SelectNpcLines` picks the set,
and the `ZM_NPC_RAISE_DIALOGUE` arm reads LIVE flags; **gating selects CONTENT,
never which seam a role talks through.** Fifth Dawnmere NPC `Npc_Warden` is the
first gated row. **Three defects, all now standing rules below:** a
`Zenith_Assert` on a unit-pinned input destroyed the WHOLE boot gate; the new NPC
row silently disarmed a roster unit by supplying a second TALKER; and the gated
branch was pinned by nothing until two mutation-verified phases went onto the
HEADLESS `ZM_NpcDispatch_Test`. +33 units; boot **2425 / 2424 / 0 / 1**; windowed
**36/0/0** with every S6 frame count held exactly.

Prior: **S7 item 1 SC2 -- SCHEMA-V1 CODEC FREEZE (ZM-D-136).**
`ZM_SaveSchema::{Write,Read}` is the pure inner-payload boundary over
`ZM_GameState`: 11 ordered independently length-framed modules, explicit
little-endian fixed widths, schema/module version 1, a 61-byte monster record,
append-transactional writes and exact-length transactional reads. Dex accepts
current/older roster counts and rejects newer with `VERSION_MISMATCH`; StoryFlags
writes its high-water count; Options is a counted uint16 TLV list requiring
exactly one known text-speed tag. The codec owns no slots, disk I/O, ECS or scene
behavior. 29 schema + 2 literal-golden units took the gate to **2392 / 2391 / 0 /
1**; the complete v1 golden is exactly **824 bytes** and represents v1, not a
fabricated v0 migration. All five builds, headless **36/0**, windowed **36/0/0**
and a complete cross-game sweep were green. Because the codec must distinguish a
growable owned stream from a fixed wrapped buffer, engine `Zenith_DataStream`
gained the read-only `OwnsData()` query -- that moved engine 1097 -> 1103.

Prior: **S7 item 1 SC1 -- DURABLE-MODEL FREEZE (ZM-D-135).** `ZM_GameState` owns
the complete module inventory: party plus deterministic transactional 16x30 boxes,
seen/caught dex, 4096 story bits, 8 badges, bag/full-width money, daycare, tower
current/best/seed, unset world position and NORMAL-default options. Catch
placement is party-first then first-free box while dex marking remains invariant;
caught battle records normalize `ABILITY_NONE` to the species regular ability. 18
new `ZM_Save` units (**2361 / 2360 / 0 / 1**). **The layout is frozen: reach it
with free functions, never new members.**

Prior: **S6 item 3 SC9 -- FULL STAGE CLOSURE (ZM-D-134); S6 COMPLETE.** The
five-build matrix, boot **2343 / 2342 / 0 / 1**, headless **36/0**, windowed
**36/0/0** and the six exact S6 filters (UI **158**, Talk **85**, Shop **286**,
Heal **315**, Interact **749**, Wander **830**) were green and non-skipped.

Prior: **S6 item 3 SC8 -- THE AUTHORED WANDERER (ZM-D-133).** `ZM_NpcWalkerLogic`
is a deterministic pure two-waypoint walker (fixed points, arrival dwell, explicit
halt, no RNG, XZ-only steering, patrol velocity preserving the body's Y);
`ZM_Interactable` serialization went **v2** with v1 data as a stationary
fail-closed fallback. **★ Its first placement intersected the ONE-SIDED terrain
mesh**, letting the dynamic capsule penetrate from the non-colliding side and fall
instead of patrol -- fixed with real clearance above sampled terrain, never by
pinning Y or teleporting. `Npc_Wanderer` sits at **x=540, z=476..484**.

Prior: **RENDERTEST CANARY RESTORED + ECS DUPLICATE-ORDER ENGINE GAP CLOSED
(ZM-D-132).** RenderTest's terrain is `_True`-baked (seed 1337), so
`TerrainEditorSmoke` PASSES windowed as a real canary. **★ HEADLESS still asserts
`Invalid buffer VRAM handle` at
`Zenith_TerrainComponent::InitializeCullingResources()`** -- an open ENGINE gap
(Q-2026-07-21-001), so terrain has no CI coverage on a GPU-less runner. **★ Also:
an engine baseline is pinned in a script DEFAULT (`Tools/run_unit_gate.ps1`) as
well as in per-game workflow args.**

Prior: **KNOWN-BUG SWEEP (ZM-D-131) -- four verified defects fixed, GAME-ONLY:**
the traversal drive picked keys in the WRONG FRAME in both remaining copies (both
now project onto the LIVE camera basis; `ZM_PlayerHomeRoundTrip` moved 673 ->
**831** frames); `ZM_NpcDispatch_Test` now asserts WHICH screen each role raises;
and the battle menu's unconditional Catch is gated on `IsCatchAllowed()`, pinned
by a key-edge unit added after the review caught the fix was UNPINNED.

Prior (S6 item 3, condensed -- full detail in DecisionLog ZM-D-124..130):
**SC7** `ZM_S6InteractGate_Test` in **749** frames; its finding -- **movement is
CAMERA-RELATIVE, so world-space key choice is correct only for a single leg from
rest** -- is the standing rule below. **SC6** `ZM_NpcShop_Test` (286) +
`ZM_NpcHeal_Test` (315) in one TU carrying the shared `WalkContext`/`TickWalk`
machine at fixed **1/60**. **SC5** three NPCs authored into the real Dawnmere
block + `ZM_NpcTalk_Test` (85); its blocker -- an NPC dead centre on a corridor
stopped the capsule 108 m short, `DriveTowardXZ` having NO obstacle avoidance --
is the standing check-traversal-routes rule. **SC4** `ZM_Interactable` (order 113)
+ `ZM_InteractionRuntime` + the headless `ZM_NpcDispatch_Test`; corrected the
false "physics is dead headless" premise by mutation. **SC3** the `ZM_NpcData`
content table with both row caps pinned to their UI limits at COMPILE time (both
UI guards are ALL-OR-NOTHING -- an oversized list is REJECTED, not truncated).
**SC2** the pure candidate picker (nearest wins, ties to the lowest index).
**SC1** `ZENITH_KEY_E` + pure `ZM_ShouldInteract` returning a REASON +
`ZM_InputActions.h` as the single source of every binding.

Prior (S6 item 2, condensed -- ZM-D-112..122): the consolidated `ZM_S6UIGate_Test`
(item 2 COMPLETE); Care Center heal as a dialogue yes/no CHOICE; shop
`ZM_ShopLogic` + `ZM_UI_Shop`; `ZM_UI_Bag`; `ZM_UI_Dex`; `ZM_UI_Party` +
generalized screen dispatch; `ZM_Bag` + money; `ZM_UI_DialogueBox`;
`ZM_UI_MenuStack` + an engine ECS fix; `Zenith_UIGridLayoutGroup`; and the S5
STAGE GATE sign-off.

## Notes for next agent (S7)

- **★★ NEW -- A PASSING PROBE IS NOT EVIDENCE OF DETERMINISM IF YOU ONLY VARIED
  THE REPETITION COUNT.** SC1b's `.zscen` probe ran the SAME boot shape three
  times, got byte-identical files, and cleared four scenes for tracking. A
  different boot shape falsified it within the hour: scene authoring bakes in
  entity indices assigned during that boot, and the boot-time unit suite
  allocates entities first, so **adding one entity-creating boot unit re-authors
  different scene bytes**. Before a probe clears a decision, ask what the claim
  is quantified over ("every boot") and vary THAT, not the number of runs. Only
  `Dawnmere.zscen` is tracked, by exact path; the three scenes CI re-authors for
  itself stay ignored (Q-2026-07-25-001).
- **★★ NEW -- A READ-BACK `memcmp` CANNOT VALIDATE A FORMAT.** The navmesh baker
  verifies its write by re-reading the file and comparing bytes, which is the
  only truthful success signal available (`WriteFile` returns void) -- but it
  never re-PARSES. That is exactly why a reader rule that was too strict
  (`neighbourCount == vertexCount`, which `StitchPortalAt` legitimately breaks)
  sailed through every bake. If a writer and a reader can disagree, something in
  the loop has to actually parse.
- **★★ NEW -- CHECK WHETHER A MUTATION LANDED ON A LIVE FIELD BEFORE CONCLUDING
  "NO TEETH".** Corrupting the middle byte of the committed `.znavmesh` left the
  test green -- because that byte is a per-polygon cached centre, which
  `ComputeSpatialData` recomputes on every load. The format documents those as
  dead bytes. Re-aimed at a vertex INDEX it kills the run at the assert; re-aimed
  at a vertex COORDINATE it produces a clean assertion failure. An inert mutation
  is a question, not an answer.

- **★★ NEW -- A MONOLITHIC AUTOMATED-TEST `Step` IS ONE STACK FRAME, AND /Od
  MAKES THAT FRAME THE SUM OF EVERY LOCAL IN EVERY PHASE.** SC5's
  `Step_ZMSaveContinue` aggregated ~six `ZM_GameState` locals (~150-200 KB each:
  6-mon party + 16x30 boxes) into a measured **1,312,136-byte** frame against
  the exe's **1,048,576-byte** stack reserve -- exit -1073741571
  (STATUS_STACK_OVERFLOW) in `__chkstk` on the FIRST Step call, before any test
  logic ran. Diagnose with the crash dump (`%LOCALAPPDATA%\CrashDumps` + cdb
  `.ecxr; kc`) -- the harness only reports `FAIL exit=-1073741571` with no
  result JSON. The fix is structural, not a budget: one driver function PER
  PHASE in the anonymous namespace (each holding <= ~2 `ZM_GameState`), the
  Step reduced to a thin switch. `ZM_AutoTests_SaveContinue.cpp` is now the
  canonical shape. Runtime frames on the save path each hold at most ONE
  `ZM_GameState` (~150-200 KB) -- safe, but never put two in one function.
- **★★ NEW -- THE SLOT-OPERATION OBSERVER EXISTS FOR DISK-LAYER ATTRIBUTION.**
  `ZM_SaveSlots::SetOperationObserverForTests` fires exactly one
  `ZM_SAVE_SLOT_OPERATION_FOR_TESTS` event ON ENTRY per public `ProbeSlot` /
  `ReadState` / `WriteState` call (a refused attempt still counts once; a
  write's verify re-probe is a separate PROBE event on the same slot; deletes
  are NOT observed). It defaults nullptr (shipped behaviour byte-unchanged) and
  `DeleteAllSlotsForTests()` clears it FIRST, so the between-tests hook resets
  it without naming it. A 4-row screen refresh = exactly 4 PROBE events in
  ordinal order; the whole title-Continue Yes window = 8 probes + exactly 1
  READ on AUTO + 0 writes (probes are refresh cadence -- record, never
  exact-pin).
- **★★ NEW -- A LATCH CONSUMED AT THE TOP OF A FUNCTION ITS OWN STATE MACHINE CAN
  RE-ENTER IS A RACE WAITING TO HAPPEN.** SC3's resume latch was cleared before the
  pose was validated and applied, but `PollForSpawnAndPlacePlayer` runs MORE THAN
  ONCE per transition (both `AdvanceFadeIn` and `PollForCameraAndBeginFadeIn` push
  the state back to `WAITING_FOR_SPAWN` when the frozen player id stops matching),
  and every pass re-runs the marker teleport -- so a spent latch silently leaves
  the player on the default spawn, only on the runs where the bounce happens.
  Before writing "consume the latch here", ask which callers can re-enter this
  function within the SAME logical operation. Prefer tying the latch's lifetime to
  the OPERATION (SC3: `ResetTransitionState`, which already runs on both the
  success and cancel paths) and making every entry re-validate, so re-application
  is idempotent and there is still no retry-forever risk.
- **★★ NEW -- WHEN TWO INDEPENDENT GUARDS BOTH PRODUCE THE RIGHT ANSWER, A TEST
  THAT ONLY OBSERVES THE OUTCOME PINS NEITHER.** SC3's quit-to-title "must NOT
  autosave" assertions were satisfied by an OVER-DETERMINED refusal (the blocker
  policy first, the playerless capture guard behind it), so every mutation their
  comment claimed to catch left them green. The fix is BOTH halves: say honestly
  in the comment what the assertion really pins, and add a negative staged so
  exactly ONE guard can be responsible -- SC3 proves the capture WOULD have
  succeeded on the same frame, then proves the blocker refused and the counter did
  not move. Pair every such negative with a POSITIVE (the same trigger DOES fire
  once the blocker clears), or the negative is satisfiable by a feature that never
  runs at all.
- **★★ NEW -- `Zenith_Vector`'s SINGLE-ARGUMENT CONSTRUCTOR TAKES A CAPACITY, NOT
  A SIZE**, unlike `std::vector`. `Zenith_Vector<T> x(n)` is EMPTY with room for n;
  use `Resize(n)` for n live elements, and note there is no range-insert (SC3 added
  a local `AppendBytes` loop). `Tests/ZM_Tests_SaveSchema.cpp` is the one remaining
  save-area file still on `std::vector` and is tracked separately -- convert it
  with that trap in mind. No `std::` containers anywhere else.
- **★★ THE `DontDestroyOnLoad` RAM-SURVIVAL HAZARD IS NOW PINNED, BUT THE PATTERN
  IS PERMANENT.** `ZM_GameStateManager`'s FrontEnd re-author path destroys the
  duplicate rather than reseeding (`OnStart` retires the duplicate entity and
  returns), so the live `ZM_GameState` survives quit-to-title ENTIRELY IN RAM and
  a naive "save -> quit -> continue" test passes GREEN against a Continue that
  reads ZERO bytes from disk. SC5's `ZM_SaveContinue_Test` is the canonical
  inoculation: scramble the live state, PROVE the scramble took, and assert the
  published state equals the saved fixture AND NOT the scramble, twinned with
  the slot-layer observer's exactly-one-READ / zero-WRITE window. Any FUTURE
  disk-backed test (SC6's autosave work included) must copy that shape; SC3's
  pose-based proof works for the same reason (a scene reload genuinely destroys
  and rebuilds the body).
- **★ SCHEMA V1 IS FROZEN (ZM-D-136), `ZM_GameState`'s LAYOUT IS FROZEN
  (ZM-D-135), AND THE SLOT LAYER IS ITS ONLY DISK OWNER (ZM-D-138).**
  Preserve the exact 11-module order, fixed widths, 61-byte monster encoding,
  statuses and transactional cursor/destination behavior in `SaveFormat.md`; every
  incompatible change owes a real version bump + a literal historical-blob
  migration test in the same commit. There is no v0. Reach the frozen model with
  named FREE FUNCTIONS (the `ZM_StoryFlags` pattern), never new members. Do NOT
  fold slot, ECS, scene or UI concerns down into `ZM_SaveSchema`, `ZM_SaveSlots`,
  `ZM_ResumePoint` or `ZM_Autosave` -- the directory IS the boundary, and the pure
  Save TUs name no ECS type on purpose (that is what makes them boot-unit
  testable). Pass live ECS answers IN as plain bools, as `RequestResume` does with
  `ZM_SpawnPoint::IsTagValid`.
- **★ THE SLOT LAYER'S CONTRACTS SC5-SC6 MUST NOT BREAK:** the 4-byte
  little-endian length prefix exists because the engine's two load paths disagree
  about `GetCapacity()` while the codec demands an EXACT length -- never pass
  `GetCapacity()` as that length and never branch on `OwnsData()`. `WriteState`
  answers from its RE-PROBE, never from `Zenith_SaveData::Save`'s return (a
  literal `true` on every path). A DAMAGED slot is SURFACED, never repaired or
  auto-overwritten, and COUNTS as occupied for Continue visibility.
  `ResolveLiveSaveBlocker` is the ONE permission predicate. SC4's menu save asks
  it at BOTH SAVE opening and the irreversible boundary, with the fixed order
  `blocker -> CaptureWorldPosition -> WriteState`; SC5/SC6 must preserve that and
  must not gate LOAD on the overworld-only SAVE predicate.
- **★ A HIDDEN FOCUSED ROOT ENTRY MUST BE REHOMED BEFORE INPUT DISPATCH.** SC4
  proves the live transition: focus Save, make a WARP blocker arise, hide and
  unfocus Save, move focus immediately to Quit, and rebuild both navigation
  directions across live entries only. Leaving focus on the hidden element makes
  focused-name dispatch stale; leaving an explicit link pointed at it swallows the
  arrow press because the engine performs no spatial fallback.
- **★ SC3'S PLACEMENT CONTRACTS (now recorded in `SaveFormat.md`):** saved
  positions are the capsule CENTRE while spawn markers store FEET, and
  `CalculateSpawnCenter` is the ONLY place the two meet (mixing them is a silent
  0.9 m sink/float). Restore with `SetBodyPosition` + `SetBodyRotation`, NEVER
  `TeleportBody` (it forces identity rotation and eats the yaw), rotation AFTER
  position, and follow it with `SyncPhysicsPoseAndInvalidate` -- those two setters
  deliberately do not fire the pose-changed hook `TeleportBody` does. Yaw is
  `atan2` of the quaternion-rotated +Z; `glm::eulerAngles(q).y` collapses past 90
  degrees off +Z and is banned. Placement runs AFTER the marker teleport and BEFORE
  the camera barrier.
- **★ STORY-FLAG INDICES ARE WIRE FORMAT.** `Source/Data/ZM_StoryFlags.h` is the
  authoritative index registry: append only, dense from zero, never renumber or
  reuse. Module 4 sizes itself from the highest SET index, so one sparse index
  costs ceil bytes in EVERY save forever. Reserve a flag by adding a row, never by
  leaving a gap; renaming a debug name is free.
- **★ A TOTAL FUNCTION MUST NEVER `Zenith_Assert` ON ITS ARGUMENTS.**
  `Zenith/Core/Zenith.h:138` defines `ZENITH_ASSERT` unconditionally immediately
  ABOVE its own `#ifdef ZENITH_ASSERT`, so the definition at `:140` always wins
  and `Zenith_DebugBreak()` fires in EVERY configuration. Units run at BOOT, so an
  assert on an input a unit deliberately supplies kills the process and **the
  whole boot gate is lost -- no "Unit tests complete" line prints at all**, which
  reads as a build failure rather than one red unit. Diagnose mis-authored data
  with a non-fatal `Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)` -- there is no
  `LOG_CATEGORY_GAME` -- and return the defined fail-closed answer. This is why
  `ZM_ValidateResume` orders itself SCENE -> TAG -> TRANSFORM: `ZM_GetWorldSpec`
  asserts on `ZM_SCENE_NONE`, so it may only be reached after the index resolved.
- **★ A NEW BRANCH IS PINNED BY NOTHING UNTIL A FIXTURE REACHES IT, AND A LOWER
  LAYER WILL HAPPILY REJECT YOUR FIXTURE FIRST.** The reusable levers:
  `Zenith_SaveData::SetReadbackForTest` stages bytes consulted AHEAD of the file
  and BYPASSES the engine CRC gate that otherwise rejects a corrupted fixture
  before the game's read callback runs; a hand-built `.zsave` with a matching
  `ComputeCRC32` is the only way to reach the disk path's exact-capacity
  behaviour; and every such fixture needs a WELL-FORMED CONTROL ARM so the verdict
  is attributable to YOUR layer. Related: bracket a numeric guard from BOTH sides
  with HAND-WRITTEN literals -- SC3's extent unit only tested 1e9, which left the
  4096 constant loosenable to 1e8 with everything green, and an expectation
  spelled against the production constant can never fail at all.
- **★ ADDING A DATA ROW CAN DISARM AN EXISTING TEST WITHOUT TOUCHING IT.** A fifth
  NPC row gave a roster unit a second TALKER, so that unit's own advertised
  mutation stopped redding it. When you append to a compiled data table, re-read
  every unit that WALKS it and ask what its stated mutation still proves. Fixture
  rows a test depends on must be a named, explicitly-spelled subset, not "whatever
  the table happens to contain".
- **★ `zenith test <Game>` runs the EXISTING exe -- it does NOT relink after an
  ENGINE-lib change.** For cross-game engine regression you MUST
  `zenith build <Game>` FIRST, THEN test, or you validate a STALE exe.
- **★★ THE TRAVERSAL DRIVE MUST BE CAMERA-RELATIVE.** Player movement is
  camera-relative and `ZM_FollowCamera` re-aims from a LAGGING camera-to-player
  vector, so the world-space meaning of W/A/S/D rotates as the player turns.
  Picking walk keys from raw world dx/dz is correct ONLY for a single leg from
  rest. `Tests/ZM_AutoTests_NpcServices.cpp` is the canonical source for any new
  walk (ZM-D-130/131) -- there is NO shared test header (`Tests/` is `.cpp` only)
  and every sibling helper sits in a per-file anonymous namespace, so copy the
  camera-relative drive, the stall watchdog and the physics-motion evidence
  (SC3's `ZM_AutoTests_SaveResume.cpp` does exactly that, and deliberately leaves
  the NPC-approach stages behind rather than shipping dead code).
- **★ CHECK EXISTING TRAVERSAL ROUTES BEFORE PLACING ANYTHING SOLID.**
  `DriveTowardXZ` has no obstacle avoidance, so a solid AABB on a corridor an
  existing windowed test walks blind will kill that test at its frame cap. SC1's
  warden derives every separation explicitly (18 m off the z=480 Home corridor,
  34 m off the x=512 spawn corridor, 20.0 m to the nearest NPC) -- copy that
  style, and re-derive from scratch if anything moves. The same arithmetic applies
  to warp SENSORS: SC3's walk target sits ~130 m from `HomeDoorTrigger` because
  `ZM_WarpTrigger` resets its overlap latch in `OnStart`, so a freshly loaded
  scene starts UNLATCHED and a pose restored inside that volume instantly
  re-warps.
- **★ MUTATION-TEST any test you claim has teeth** -- break the thing it should
  catch, rebuild, confirm RED (exit=1), restore, re-gate. Six consecutive
  sub-commits; four times it revealed the test proved nothing.
- **★ The recurring review win is finding tests that CANNOT FAIL** -- loops
  bounded by a count just asserted zero; "unchanged" asserts on state never
  populated; a totality test whose expectation calls the function under test; an
  expectation DERIVED from the production table it is meant to pin; a producer no
  assertion observes at all (SC3's autosave counter). Check for vacuity EXPLICITLY
  on every SC, naming per unit the change that would red it. Corollary: **never
  ship a function with no caller and no test** -- it lands with its producer.
- **★ THE RECURRING DEFECT CLASS: confidently-worded FALSE claims inside
  argumentative comment passages.** SC1 found seven copies of one (`QueueLines`
  "would crash" on a null array -- it rejects it and the real consequence is a
  MUTE NPC); SC2 found a "would walk far past the buffer" claim that ignored the
  frozen codec's own equivalent bound; SC3 found a comment naming the wrong guard
  as the rejecting mechanism, plus three stale line citations. Verify every claim
  a comment makes about another function BY READING that function, and re-check
  every `File.cpp:NNN` citation you copy forward.
- **★ UI element ownership:** `AddElement` pushes into BOTH `m_xAllElements` and
  `m_xRootElements` while `Clear()` deletes only `m_xAllElements`, so a merely-
  `AddChild`'d element **LEAKS** and an `AddElement`+`AddChild` element is walked
  TWICE -- `Zenith_UICanvas::ReparentElement` is the only correct path. And
  `SetVisible` notifies the parent (a grid re-runs layout), so write child
  visibility **ONLY ON CHANGE**, in Hide as well as Present.
- **★ ENGINE UI NAV RULE:** never wire bake-time `SetNavigation` links into a pool
  whose members are shown/hidden at RUNTIME -- `NavigateDown` consults the explicit
  link FIRST and only falls back to the spatial search when it is null, so a link
  into a hidden element silently swallows the press. `SetNavigation` is also not
  serialized, so bake-time links exist only in tools builds.
- **★ Windowed-gate rule:** a test that PARKS the canvas focus programmatically
  proves NOTHING about navigation -- drive real arrow-key edges, deadline-guarded,
  with flags that **DEFAULT TO FAILING**, and give every waiting phase its OWN
  deadline and diagnostic (being ended by the harness frame cap says nothing about
  WHERE the test stalled).
- **★ The dialogue-answer trap, BOTH halves:** (a) a prompt raised over an EMPTY
  stack pops to empty on resolve, which `CloseMenu()`s and `Reset()`s the box,
  clearing its answer, all in ONE `OnUpdate` -- read
  `ZM_UI_MenuStack::GetLastDialogueAnswer()`, NEVER `GetDialogue().GetChoice()`.
  (b) that host latch is NOT per-test state, so also assert
  `!IsDialogueAwaitingChoice()` before trusting it; do NOT "capture before and
  require a change".
- **★ ANY per-test mutation of persistent state must be RESTORED on EVERY exit
  path**, including a mid-phase failure -- and **a new stateful game component
  MUST be wired into the between-tests hook** in `Zenithmon.cpp` (SC2 added
  `ZM_SaveSlots::DeleteAllSlotsForTests()` there, BEFORE
  `Zenith_SaveData::ClearForTest`). Watch WHERE the reset sits: SC3's autosave
  counter is a PROCESS global, so `ZM_ResetAutosaveForTests()` had to go ABOVE
  `ResetRuntimeStateForTests`'s no-manager early-out, or a batched run that has
  just force-loaded FrontEnd inherits the previous test's count.
- **★ Dispatch by the FOCUSED ELEMENT'S NAME**, never `SetOnClick(this)` -- a
  `this` userdata dangles on ECS pool relocation. Screen presenters are NON-ECS
  `Source/UI/` classes owned BY VALUE by the MenuStack.
- **★ Positional aggregate tables: append new columns AT THE END** -- every
  `ZM_NpcData` row is a positional initializer, so a mid-struct field shifts each
  trailing value one column left with NO compile error.
- **★ No teleportation for movement** -- use `Zenith_Physics`, never
  `SetPosition`, **even in tests**. (A one-time load/warp SPAWN placement is not
  movement; that is the one sanctioned direct body write, and SC3 keeps it inside
  the transition machine.)
- **★ CI-VISIBILITY:** `zm-tests` runs HEADLESS and a SKIP counts as a PASS, so
  every `m_bRequiresGraphics = true` test (the walk-up family; SC3's
  `ZM_ResumePlacement_Test` / `ZM_QuitToFrontEnd_Test`; and SC4's
  `ZM_SaveMenuFlow_Test` / `ZM_RootQuitAndBlockedSave_Test`) is carried by the
  LOCAL WINDOWED gate only. CI-visible coverage must live in a headless test or a
  boot unit.
- **Diagnosing a windowed test:** game `Zenith_Log(LOG_CATEGORY_UNITTEST)` output
  is NOT in harness stdout -- run the exe directly, `<exe> --automated-test <Name>
  --exit-after-frames N` (pair the flags; bare idles forever). Visual evidence:
  `Flux_Screenshot::RequestDump` in a Step -> BGRA TGA -> PNG.
- **New files -> `Build\regen.ps1`.** Sharpmake globs each game's tree recursively
  and the generated projects are gitignored, so a new FOLDER without a regen
  link-fails in a way that looks like a code defect (SC2's `Source/Save/`).
- **Working model:** MASTER-ONLY (ZM-D-031); the LOCAL gate is the authority;
  `zm-tests` is a post-push backstop (fix forward on red, never revert or
  force-push). Only the orchestrator builds/tests/commits; subagents author and
  never build. Sweep stray `zenithmon.exe` processes before ending. NEVER commit
  baked assets or `Build/artifacts`. `zenith test Combat` drops a stray
  `EnemyBase.zpfb` in the repo root -- delete it, never commit it.
- **Open Questions:** Q-2026-07-21-001 (ENGINE: terrain sets up GPU culling
  resources with no headless guard); Q-2026-07-21-002 (RenderTest
  `RT_TennisDeterminismDigest` fails windowed); Q-2026-07-17-001
  (`ZM_BattleTransition::BiomeForScene` is a hard-coded table); Q-2026-07-12-003
  (`ZM_BattleAI`: file location, no-Struggle, tunable thresholds).
- **The next VISUAL hard-stop is the S8 vertical-slice go/no-go** (manual
  playthrough sign-off). S6/S7 have no visual gate -- the loop runs through them
  automatically.
