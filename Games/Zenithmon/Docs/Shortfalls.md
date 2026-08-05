# Zenithmon -- Shortfalls & Gap Analysis

**Document purpose:** a frank, gap-by-gap audit of where the current `Games/Zenithmon` tree falls short of the shipping vision in [GameDesignDocument.md](GameDesignDocument.md). This is the most-accurate-current-state doc; cross-check [Status.md](Status.md) before acting on any single line.

**Scope note (2026-07-09, S0):** at S0 essentially EVERYTHING is a gap -- the project is a booting skeleton. This doc is deliberately structured per major system with a one-line current-state each, so later sessions UPDATE lines in place rather than restructure. When a stage lands, replace that system's status line and add a dated note; do not reorder sections.

**Verdict at a glance (baseline bullet re-observed 2026-07-31 at ZM-D-173; whole block
re-verified line by line 2026-08-01 -- ★ AND IT HAD GONE STALE TWICE IN ONE DAY BEFORE THAT. It was rewritten earlier on 2026-07-29 (ZM-D-165) precisely because it had been stamped
2026-07-21 and was eight days stale; by that evening it was wrong again, still asserting "S7 IS
NOT COMPLETE" and still listing SC2/SC3 as remaining work after both had landed. A verdict block
that nobody re-reads is worse than none, because it is the first thing a new session trusts --
and rewriting it once does not immunise it. Update it in the SAME commit that closes a stage.):**
- **★ S7 IS COMPLETE (2026-07-29, ZM-D-167), AND THE ROADMAP CHECKBOXES ARE THE PROOF.**
  `Roadmap.md:104` requires `forward cone + occlusion raycast -> FREEZE INPUT -> APPROACH ->
  dialogue -> forced battle -> defeat flags + prize money`. The two verbs W3 had CUT ON EVIDENCE
  (ZM-D-159) -- `freeze input` and `approach` -- shipped as SC1-SC3 (ZM-D-163/166/167), so both
  boxes at `:104` and `:172` are now honestly ticked and the gate at `:185` is annotated MET.
  **★ THE RULE THAT GOT US HERE, STILL BINDING: when prose and a checkbox disagree, the checkbox
  wins, scored against its LITERAL text.** `Status.md` once claimed "S7 COMPLETE" for five
  commits and was retracted (ZM-D-162); the corollary bit in the other direction on the same day,
  when this block went on denying a closure the boxes recorded. **And the camera cut is NOT
  claimed by that tick** -- it is absent from `:104`'s text, was a W3 aspiration only, remains
  unbuilt, and stays booked in 1.8 below.
- **Stages complete with their required gates closed:** S0-S6. S1's data core and deterministic
  headless battle engine (including feature-complete breeding/gender and Battle Tower logic); S3's
  traversable Dawnmere + live PlayerHome door round trip; S4's five procedural asset generators +
  `ZM_BakeManifest`, visually approved (ZM-D-088); S5's full overworld<->battle slice, visually
  approved (ZM-D-112); and S6's dialogue/menu/NPC/shop surface all remain complete.
- **★ THE LIVE BASELINE IS IN `Status.md`'s TOP BLOCK. READ IT, NOT THIS PARAGRAPH.** As of
  2026-08-04 (ZM-D-184, OBSERVED on a clean `Null_` build): registry **55**, ZM boot
  **2909 ran / 2907 passed / 0 failed / 2 skipped**, engine boot **1284**. (ZM-D-183
  stood at 2908 / 2906; the ZM-D-182-era figures were 2864 / 2862 / 1242.) Everything below
  this line is the HISTORY of how those numbers got there, kept because the derivations are
  the audit trail -- every figure in it is true of ITS commit and stale as a current claim.
- **Baseline as re-observed 2026-08-01 at ZM-D-179 (every figure off an
  OBSERVED line):** ZM headless registry **56 passed / 0 failed**; ZM boot unit gate
  **2849 ran / 2847 passed / 0 failed / 2 skipped** (`zm-tests.yml` pinned to **2849**) --
  ZM-D-179 added **+2 ENGINE** boot units
  (`Physics::TransformSerializationIgnoresSubEpsilonBodyPoseNoise` and
  `...FollowsAGenuineBodyMove`) and **no** automated test, so the registry is unmoved at 56
  and the ENGINE baseline moves with it. ZM-D-176 stood at boot **2847 / 2845 / 0 / 2**,
  having added **+7** boot units and **+2** automated tests (`ZM_InteriorTint_Test`,
  `ZM_InteriorTintPixels_Test`) for the PlayerHome tint and the New-Game entry-point move.
  ZM-D-175 stood at registry 54 / boot **2840 / 2838 / 0 / 2** (+15 units, registry unmoved);
  ZM-D-174 at registry 54 / boot **2825 / 2823 / 0 / 2**; the engine boot gate stood at
  **1235 / 1234 / 0 / 1** at ZM-D-174 (GAME-ONLY: it touched no file under `Zenith/`).
  ★ **THAT 1235 IS HISTORICAL AND WAS LEFT STALE HERE UNTIL 2026-08-01.** The engine gate
  moved to 1237 at ZM-D-179 and to **1242** at ZM-D-181; `run_unit_gate.ps1`'s default is
  **1242**. Status.md's top block is the live figure -- read it, not this paragraph.
  ZM-D-174 moved the registry 53 -> 54 (`ZM_ProfLabWarp_Test`) and the ZM boot baseline
  2817 -> **2825** (+8 ProfLab placement units). The prior figures, still true of their commit:
  registry 53, boot 2817 / 2815 / 0 / 2, full windowed Vulkan 53/0 with ZERO skipped.
  ZM-D-173 moved the registry 51 -> 53
  (`ZM_DawnmereHomeGroundTruth_Test` + `ZM_DawnmereCameraClearance_Test`) and both boot-unit
  baselines +8 -- four ENGINE sensor-raycast units, which every game inherits, plus two camera
  fixtures and two Home-placement units.
  **★ AND THE PRE-CHANGE ZM FIGURE WAS NOT WHAT ANY DOCUMENT SAID.** Measured on a clean
  HEAD build immediately before this work: **2809 ran, 2 skipped** -- while this bullet said
  2759, `Status.md` said 2759 and `zm-tests.yml` was pinned to **2804**. The workflow pin was
  therefore already RED on master, and the second skip had appeared unrecorded. Both are fixed
  forward here from OBSERVED lines; neither was earned by this change.
  The skips are the quarantined
  `GraphComponent::RegistryWideNodeRoundTrip` (task_726cc81d) plus one ZM-side skip.
  **Every "36 tests / 2392 units" figure elsewhere in this document is stale by construction --
  trust this line.** ★ This bullet previously read 2731 while `Status.md` read 2722 and the
  workflow was pinned to 2742: **three files, three different baselines, all stated as fact.**
  A commit that adds a boot unit updates all three or none.
  **★ AND IT DRIFTED AGAIN ONE COMMIT LATER, IN THE OTHER COLUMN.** ZM-D-169 moved the REGISTRY
  49 -> 50 and left both unit baselines alone, so the "all three or none" rule was satisfied for
  units -- and this bullet still said `registry 49` afterwards, because the rule as written only
  guards the boot-unit number. **The registry count is a fourth figure and it lives only here and
  in `Status.md`.** Fixed 2026-07-30. A commit that adds an AUTOMATED TEST updates both.
  **★ ZM-D-170 moved NONE of the four**, because it extended an existing registration rather than
  adding one and touched no `ZENITH_TEST` case. Changing nothing is the correct action when the
  count does not move -- the rule is "all of them or none", not "edit something every commit".
- **S6 (Dialogue, menus, NPCs, shops) COMPLETE.** Four authored Dawnmere NPCs are reachable by walking up and pressing the interact key: villager, Trade Post clerk, Care Center caretaker and wanderer. **[SNAPSHOT AT THE S6 CLOSE -- the roster is now SIX. S7 appended `Npc_Warden` (item 2 SC1, the story gate) and `Npc_RivalVesper` (item 3 SC8, ZM-D-156); `s_axDawnmereNpcAnchors` in `Source/World/ZM_DawnmerePlacement.cpp` has six rows and `ZM_NPC_COUNT` is 6.]** The wanderer uses a deterministic two-waypoint patrol; `ZM_Interactable` v2 persists the patrol configuration and v1 data loads as a stationary fail-closed fallback. Behaviour-graph and terrain-fed navmesh work deliberately moves to S7.
- **S7 item 1 full schema-v1 codec is green (ZM-D-135/136):** SC1's 18 durable-model units are joined by 29 schema + 2 literal-golden compatibility units. The pure codec freezes 11 ordered length-framed modules, explicit little-endian widths, transactional streams and an 824-byte v1 golden. Units are **2392 ran / 2391 passed / 0 failed / 1 skipped**; engine remains **1103**; all five Zenithmon builds, headless **36/0** and full windowed **36/0/0** passed; registry remains 36. No visual/human gate applies.
- **The ZM-D-168 follow-up LANDED 2026-07-30 as ZM-D-169** -- the SPOTTED marker is off the
  debug-primitives channel (1.8-3c below, now closed ON PIXELS) and the suite has its first
  pixel-level assertions. Because it modified `Zenith/Flux/Primitives` it took the **CROSS-GAME**
  gate: three sentinels exit 0, engine boot units **1164 / 1163 / 0 / 1 unmoved**, all five other
  games clean on Null_True. ZM headless **50/0**, full windowed **50/0 with ZERO skipped**, ZM boot
  **2742 / 2741 / 0 / 1 unmoved** -- so no baseline moved and none of the three pinned sites was
  touched.
- **★ Next autonomous work (REWRITTEN 2026-08-01 at ZM-D-177; the sentence this replaces was wrong
  on three counts and is the exact failure this file keeps recording).** It read *"S8's four content
  items (`Roadmap.md:198-201`), none of which is built"* and was "re-verified" the same day four S8
  sub-commits landed. Wrong on: (1) ZM-D-174/175/176/177 have since landed and **all four ARE S8
  content**; (2) "none of which is built" is false -- **item 1 is IN PROGRESS**; (3) the citation had
  drifted -- S8's items are `Roadmap.md:209-212` and its gate is `:214`. A confident sentence
  outranking an unticked checkbox, again.
  **TRUE POSITION AT HEAD `c9d64994`: S8 item 1 ("Intro -> lab -> starter choice",
  `Roadmap.md:209`) is IN PROGRESS and its box is deliberately UNTICKED.** Landed: the ProfLab
  interior shell + build index 41 (ZM-D-174); the starter DATA layer + seed split with
  `ZM_MakeStarterGameState` deleted (ZM-D-175); the New Game entry point moved to PlayerHome and the
  interior tinted warm (ZM-D-176, a USER ruling); the tint probe's premise corrected (ZM-D-177).
  **NOT built, and what item 1 still needs:** the Dawnmere lab EXTERIOR + door trigger + `FromLab`
  spawn; **Professor Aster** (NPC row + palette + authored into ProfLab); the **starter-choice
  SCREEN** (the model and grant exist -- only the presenter is missing); and the intro beat itself.
  Items 2-4 of S8 (`Roadmap.md:210-212`) remain wholly unbuilt. **Still
  booked here and NOT scheduled:** the camera cut (1.8-3a); the W4 palette-data re-author (rendered
  NPC distinctness back toward 0.15 via more-separated authored colours -- see 1.8-4, the ZM-D-171
  residual); and the engine's `AddLine` centring bug (`task_33ee8059`). **The general greybox shading
  gap is CLOSED at engine level (ZM-D-171)** -- vertical faces carry real sky+ground ambient now,
  measured on pixels, and ZM-D-169's NPC-only emissive workaround is deleted. There is no human gate
  during S8's content work; the next human intervention point is the S8 vertical-slice go/no-go,
  which **follows** those four items rather than preceding them, and which **no agent may sign**.
- **What's designed but unbuilt:** the remaining outdoor terrain recipes and all playable Thornacre/Route1 scene content; badge-award gameplay and the League arc (S8+); the BOX storage screen (**re-deferred S7 -> S9 in writing on 2026-07-29, ZM-D-165 / Q-2026-07-29-001, after a doc audit found the S6-era deferral about to expire unremarked**); and the broad world buildout (S9/S10). **Corrected 2026-07-29:** the save-slot / manual / continue / autosave flows are NOT unbuilt -- they SHIPPED as S7 item 2 (ZM-D-137..142), and story-flag gating and trainer battles shipped in S7 items 2-3. The inner disk-payload codec itself now exists. PlayerHome and the outdoor home shell are intentionally replaceable greyboxes, not final art. The full ~25-terrain-set world bake remains a projection (AssetManifest.md 6.3), though the four asset-generator families have been cold-baked together at family scale.
- **Locked cuts (not gaps -- see Scope.md):** no audio (engine has none), no networking/multiplayer/trading, no Dynamax-analog, battle format singles only, documented volatile-status cuts (Substitute/Encore/Transform/weight moves).

---

## 1. Per-system gaps (update in place)

### 1.1 Data core

**Status: COMPLETE (S1, 2026-07-10).**
`Source/Data/` holds the full data core as compiled `const` C arrays: the 18-type chart (golden-locked), the 152-species dex (roster + DERIVED base stats + DERIVED level-up learnsets), 218 moves over a 57-kind `ZM_MOVE_EFFECT` enum, 90 items over a 34-kind `ZM_ITEM_EFFECT` enum (incl. 25 TMs -> real moves), 50 abilities (roster + `ZM_ABILITY_HOOK` surface bitmask), 25 natures (exact 5x5 grid), `ZM_StatCalc` (Gen-III+ integer formulas) + `ZM_BattleRNG` (PCG32, deterministic), `ZM_WorldSpec` (schema + 8-scene proving set), and `ZM_DataRegistry` (name->ID lookups + cross-table enforcer). **Gaps carried FORWARD (by design, tracked):** move execution and all 50 ability hooks are live in S2; remaining item/bag/held/TM context is downstream. Base stats + learnsets remain systematic placeholders for S11 balance (ZM-D-021/023), and `ZM_WorldSpec` remains the 8-scene skeleton until S9/S10 (ZM-D-029).

### 1.2 Battle engine

**Status: S2 COMPLETE -- all battle logic + the stage gate PASSED (2026-07-12, ZM-D-047). Post-gate, feature-complete breeding + gender is DONE (user-directed, ZM-D-048/049/050): gender + ratios, real egg groups, GLOOPET Ditto-analog, gendered compatibility, ability + hidden-ability + egg-move inheritance, and derived hatch cycles all shipped across SC-A/B/C. At S2 closure the next battle-adjacent step was S5 integration; S5 has since completed.**
`Source/Battle/` now contains the deterministic append-only battle engine; the complete move/status/catch/switch executor; weather and all 50 ability realizations; `ZM_ExpAndLevel`; `ZM_BattleAI`; `ZM_Breeding` + `ZM_Daycare`; and `ZM_BattleTower`. Box 4 adds four integer curves, derived progression accessors, current cumulative EXP, modern per-opponent party share, capped EV accumulation, mid-battle level/stat/move learning, and terminal level-evolution queuing/pure mutation (67 tests; award-off event/state/RNG identity preserved). Box 5 adds `ZM_BattleAI` -- a pure, side-effect-free four-tier chooser (`ZM_ChooseAction`) that reads state `const`, draws only its own RNG (RANDOM tier only), and perturbs no battle RNG/state/event (box-1..4 goldens byte-identical); 28 `ZM_Battle` tests. Box 6 adds deterministic breeding/daycare and Battle Tower setup/settlement logic; the later feature-complete expansion supplies gender ratios, real egg groups, GLOOPET compatibility, hidden-ability and egg-move inheritance, and hatch cycles. The current inventories are **384 `ZM_Battle`** and **264 `ZM_Data`** tests. The S2-era integration/presentation follow-up shipped in S5; the mechanics gaps below remain the live battle-engine follow-ups.
**W1 count correction (ZM-D-157):** the current inventory is **388 `ZM_Battle`**
tests; the 384 figure in the S2 historical paragraph immediately above predates the
four new battle-category replacement cases. `ZM_Data` remains **264**.
**Tracked deferrals (mechanics gaps, faithfully noted -- not bugs):**
- **RESOLVED 2026-07-28 (ZM-D-157) -- forced switch on faint and complete trainer
  parties.** `ZM_BattleEngine::ResolveTurn` now resolves every non-terminal fainted
  active after `TURN_END` and the whole-party terminal scan, on PLAYER then ENEMY,
  through the canonical `DoSwitch` path (`SWITCH_IN`, switch-in ability dispatch,
  participant marking). A caller policy selects the slot; invalid/no policy falls
  back to the lowest live reserve. `ZM_BattleDirectorCore` gives enemy replacements
  to `ZM_ChooseReplacement`: RANDOM draws only from live reserves using the private
  AI RNG, while tactical tiers choose the reserve with the best greedy matchup and
  consume no RNG. Player replacement currently uses the deterministic lowest-live
  fallback. `uZM_TRAINER_BATTLEABLE_PARTY` moved from 1 to the shared full party cap
  in the same commit, and the two-member Rambler is mutation-proven end to end from
  authored row through `FAINT -> TURN_END -> SWITCH_IN` to payout. The former hard
  process break and lead-only mitigation are retired; a player choice UI remains a
  separate UX enhancement, not a mechanics blocker. The live director also consumes
  presented replacement events and reloads the reserve's arena model, so state/HUD and
  the rendered opponent cannot diverge after the switch.
- **Conditional ball bonuses not applied (SC6):** the net/dusk/quick/heal orbs use their base catch param (all x1.0 in the data) -- the type/time/turn/heal conditional multipliers are not yet computed. Faithful to the current `ZM_ItemData` (only great/ultra/prime differ), and the catch core 4-shake math is exact; revisit when a later gameplay context supplies time-of-day and turn-count inputs.
- **High-crit MOVE flag under-applied:** a move's inherent `m_uCritStage==1` (high-crit) still crits at box-1's 1/24, not 1/8 -- it is NOT folded into the `RAISE_CRIT` `m_iCritStage` counter (`{0->1/24,1->1/8,2->1/2,>=3->always}`). Deferred because the two scales differ (move `2`=guaranteed vs counter `2`=1/2) so a correct fold is a scale-reconciliation decision; land it when high-crit/RAISE_CRIT moves get real battle coverage. Golden-invisible today (every tested move is `m_uCritStage 0`).
- **Self-targeting damaging-secondary dropped on a KO:** the one self-buff secondary (Primeval Might, `RAISE_ALL` chance-10) gates its E3 proc on the DEFENDER being alive (per ZM-D-033), so a KO drops the user's self-boost. Contract-consistent (no golden desync) but debatable vs mainline; revisit when self-target secondaries get coverage.

### 1.3 Overworld

**Status: COMPLETE (S3) -- automated implementation, the definitive post-overlay-hitch local gate, AND the human visual sign-off are all done; the required S3-S5 gates are closed (see Status.md).**
Per-scene terrain isolation, safe staged targeting, bounded export, and sparse streaming exist (ZM-D-051/052). The ignored Dawnmere/Thornacre/Route1 terrain families total 896 chunks, 2,700 files, and 672,354,172 bytes; calibrated walls are 59.035 / 69.979 / 80.804 seconds, with an all-warm 16.874-second zero-terrain-recipe queue. Dawnmere has `Dawnmere.zscen`, grass regeneration, `ZM_InputActions`, an order-102 velocity-driven dynamic capsule controller, and an order-103 fixed-yaw spring/collision follow camera. FrontEnd authors the persistent `ZM_GameStateRoot` with order-104 manager plus the exact full-screen order-10000 `WarpFade`; orders 105/106 remain spawn/trigger, while order 107 `ZM_GreyboxVisual` draws replaceable procedural blocks. PlayerHome build 40 is always tools-authored with a collidable greybox shell, scene-owned Player/camera, `Door` feet marker, and live exit `(2,"FromHome")`; Dawnmere owns a home shell, `FromHome` marker, and live door `(40,"Door")`. Every accepted route freezes immediately, fades to opaque over 0.20 s, issues exactly one SINGLE, places/zeros the replacement behind black, waits for exactly one generation-matching active-scene main follow camera, fades in, then unlocks at alpha 0. Missing/ambiguous dependencies revert to or remain opaque and locked. The input-driven P1 traverses both real trigger overlaps and proves all three scene generations, placement, controller/camera recovery, terrain/grass absence inside, and recovery outside. Thornacre and Route1 remain measurement terrain families only.

### 1.4 Asset generators

**Status: COMPLETE (S4, 2026-07-16) -- all five generators shipped + the full-family visual gate signed off (ZM-D-088).**
The `ZM_GenCommon` (loft/RNG) + `ZM_TextureSynth` foundation (ZM-D-059) plus the five generators are all shipped: `ZM_CreatureGen` (v3, 152 species, skinned + animated, 15-file bundles incl. 6 clips) + `ZM_CreatureAnimGen`, `ZM_HumanGen` (v1, 34 models on ONE shared 16-bone rig + 9 shared clips), `ZM_BuildingGen` (v1, 30 static, 4-file bundles) + `ZM_PropGen` (v1, 25 static, 4-file bundles), and `ZM_BakeManifest` (ZM-D-085) -- the per-family bake guard: a 12-byte `ZMBM` stamp (magic + generator version + expected-file count) at `game:<Family>/.manifest`, written atomically after a successful `ZM_BakeAll*` and checked fail-open on warm boots. Meshes bake through the four `ZM_GenCommon` bridges: `ZM_GenBakeMesh` (own skeleton), `ZM_GenBakeSkeleton` (shared rig), `ZM_GenBakeMeshWithSharedSkeleton` (bind a shared rig), and `ZM_GenBakeStaticMesh` (no skeleton). All baked assets are git-ignored and regenerated under the manifest guard. The S4 gate is the windowed `ZM_AssetGallery_Test` -- 26 representatives across all four families (8 creatures one-per-archetype, 6 humans, 6 buildings, 6 props) on a reflective floor -- which passed windowed and was visually approved 2026-07-16 (ZM-D-088; the first capture was rejected for buildings intersecting and fixed via a width-budget `AGFitScale`, ZM-D-087). **Carried-forward notes:** the creature palette reads soft/pastel (a punchier look is an available follow-up); the loft appendage kit remains a strictly vertical-Y-sweep toolkit (non-vertical anatomy approximated via ring-centre X/Z offsets + archetype-local flat-blade helpers, AVIAN wing / AQUATIC fin / FLOATER-PLANTOID tendril), not true arbitrary-axis lofting. Master reference: the StickFigure pipeline in `Tools/Zenith_Tools_TestAssetExport.cpp`.

### 1.5 Battle integration (overworld <-> battle)

**Status: COMPLETE (S5, 2026-07-18) -- items 1-5 shipped AND the S5 STAGE gate visually SIGNED OFF by the user (ZM-D-112). ZM-D-089..112.**
The full overworld<->battle slice exists: the Battle scene (build 1, world Y=-2000) + `ZM_BattleArena` (order 108, dome+platforms+biome sets); `ZM_EncounterZone`/`ZM_TallGrassSystem` (order 109) + engine E5 grass-reset; the additive-load/pause/camera round trip + `BattleFade` (`ZM_BattleTransition` order 110); the pure `ZM_BattleDirectorCore` presenter + the `ZM_BattleDirector` ECS component (order 111) that drives an AI-vs-AI battle and calls `RequestBattleEnd`; the visible battle HUD (`ZM_UI_BattleHUD`: E3 typewriter text log + HP panels) + the interactive Fight/Catch/Run menu; and the item-5 GameState wiring -- a persistent in-memory `ZM_Party`/`ZM_GameState` (`ZM_GameStateManager`), exp/level write-back on a win, catch (add to party + dex), and loss->whiteout (heal + warp to Dawnmere). Proven by 10 windowed battle tests (walk-grass->encounter->win/catch/flee/whiteout->exact resume) + ~90 pure T0 units; the ~380 S2 battle goldens stay byte-identical throughout. **Open verification (Q-2026-07-09-003) -- now DISCHARGED:** the additive-at-(0,-2000,0) render isolation was asserted-by-automation (the round-trip clears overworld grass to 0 at IN_BATTLE + an enclosing arena dome; exact resume with drift<0.05m) and was **PIXEL-proven and user-approved at the S5 screenshot gate (ZM-D-112)**. The documented SINGLE+snapshot fallback was not needed and remains available only if a future change reintroduces bleed-through.
**S6/S7 deferrals flagged from item 5:** (a) **RESOLVED 2026-07-20 (ZM-D-131)** -- the battle menu's Catch item was shown unconditionally, ignoring `m_bCanCatch`; it is now gated on a core-surfaced `ZM_BattleDirectorCore::IsCatchAllowed()`, so a `m_bCanCatch == false` config (which `ZM_BattleTower` already produces) cannot present a Catch action and therefore cannot trip the `ZM_BattleEngine::SubmitAction` / `DoItemAction` asserts. The gate is pinned by a headless test that drives the REAL `UpdateMenu` with real key edges and asserts the emitted action is RUN, never ITEM -- **mutation-verified**: reverting the flag to a hard-coded `true` reds it. The hidden Catch row no longer leaves a layout hole (visible entries are placed by resolved entry index). (b) **RESOLVED 2026-07-21 (ZM-D-135)** -- a full-party catch now marks seen+caught and stores the record in the first free slot of the deterministic 16x30 box grid; full box insertion rejects without mutation. (c) **single-LEAD battles only** -- multi-member parties + forced-switch-on-faint remain future work (a fainted lead re-enters at the `[1,maxHP]` clamp until the whiteout heals it). (d) **CODEC RESOLVED 2026-07-21 (ZM-D-135/136), SLOT FLOW OPEN** -- the durable aggregate and pure schema-v1 payload are frozen, while Save0-2/Auto I/O, menu save, continue and autosave remain item 2.

### 1.6 UI

**Status: COMPLETE (S6, 2026-07-21). The full pause-menu tree, dialogue, party, bag, dex and shop screens are SHIPPED and walk-up-reachable from authored NPCs.**
The persistent `WarpFade`/`BattleFade` are real full-screen transition surfaces. **The battle UI now exists** (S5 item 4, ZM-D-103/104/109): `ZM_UI_BattleHUD` renders a typewriter text log (via the E3 `SetVisibleGlyphCount`, ZM-D-100) + two HP panels (species/level/HP + fill bar), and an interactive **Fight/Catch/Run** action menu with a move submenu (gapped-moveset compacted), all authored on the `BattleDirector` entity at sort order > 10001 (above the fade). The engine globally orders the shared quad queue by UI sort key across canvases, preserves equal-key submission order, drops newest beyond 1,024 with one warning per frame, and keeps the highest-sort overlay's text clip; `DiscardPendingFrame` centralizes the pending-queue/counter/clip reset across both Text paths. **Those screens now all exist** (S6 item 2, ZM-D-114..122): `ZM_UI_MenuStack` (order 112, the pause root), `ZM_UI_DialogueBox`, `ZM_UI_Party`, `ZM_UI_Dex`, `ZM_UI_Bag` + money, and `ZM_UI_Shop` + `ZM_ShopLogic`, all NON-ECS presenters owned BY VALUE by the menu stack and dispatched by FOCUSED ELEMENT NAME. Still missing: the BOX (storage) screen. **★ ITS RECORDED BLOCKER IS GONE AND THIS LINE WAS STALE:** "needs S7 persistence" was true when written, but S7 item 1 shipped the schema (boxes **16x30**, ZM-D-136) and S7 item 2 shipped the slot/Continue path, so nothing about the storage MODEL is outstanding -- only the presenter. `Roadmap.md:98` had deferred the screen INTO S7 and S7 closed without it and without any re-deferral recorded; a doc audit caught that on 2026-07-29 and it is now **re-deferred to S9 in writing** (ZM-D-165, Q-2026-07-29-001), because a box is only functional once the player can exceed a full party -- which needs S9's routes and encounter tables. Built earlier, its only reachable state is "empty".
**S6 item 3 (NPC interaction) is COMPLETE:** the interact key + pure `ZM_ShouldInteract` gate, pure candidate picker, four-row `ZM_NpcData` roster, `ZM_Interactable` (ECS order 113) + `ZM_InteractionRuntime`, four authored Dawnmere NPCs, walk-up proofs for talk / buy / heal / wander, and the consolidated `ZM_S6InteractGate_Test`. `ZM_NpcWalkerLogic` supplies deterministic two-waypoint motion configured through `ConfigureWander(...)`; serialization v2 persists the authored patrol while v1 loads stationary. Behaviour graphs and navmesh integration remain S7 work. **Next free ECS order: 114.**

**Historical SC2 dialogue-box forward-notes, reconciled after S6 (resolution status inline):**
- **RESOLVED for the shipped S6 consumer (ZM-D-121):** SC2 originally found no per-conversation completion signal. SC8's armed-choice mode subsumed completion for the Care Center through the menu stack's awaiting-choice state and last-answer latch. There is still no generic conversation-token completion callback for arbitrary future callers; add one only if an S7+ flow requires it.
- **RESOLVED (ZM-D-121):** the dialogue screen now has an armed YES/NO choice mode with focusable rows and a resolved-answer latch; the Care Center heal proves both answers.
- **The 8-line cap is cumulative per conversation, not per pending line.** The queue never compacts as lines are consumed, so `QueueLine` while active cannot be used to chunk a conversation past 8 total lines. Fine for NPC barks; if long conversations arrive, compact on advance or raise the cap.
- **RESOLVED ownership boundary (ZM-D-124):** the dialogue raise path remains intentionally ungated so battle/trainer glue can use it, while the shipped `ZM_Interactable` path owns the pure `ZM_ShouldInteract` world/transition/freeze gate.
- **RESOLVED (ZM-D-124):** walk-up interaction uses the distinct `ZENITH_KEY_E`, so the raising edge cannot be re-read as dialogue confirm in the same frame.
- **RESOLVED (ZM-D-127):** `ZM_Interactable::Interact()` logs a warning naming the NPC, id, role and refused seam, so authored interaction failures are no longer silent.

**★ UPDATE (2026-07-20, S6 item 3 SC1, ZM-D-124) -- two of the dialogue-box forward-notes above are now DISCHARGED:**
- **"the future `ZM_Interactable` owns the can-the-player-talk-right-now gate"** -- that gate now EXISTS, as the pure `ZM_ShouldInteract` in `Source/Interaction/ZM_InteractionLogic.{h,cpp}`, refusing on menu-open / not-overworld / warp-in-progress / battle-transition / player-frozen and returning WHICH blocker fired. `ZM_UI_MenuStack::PushDialogueLines` itself deliberately stays UNGATED, so the box remains usable inside the battle scene for S7 trainer dialogue.
- **"the raising confirm edge is not consumed"** -- answered by a DISTINCT key (`ZENITH_KEY_E`) plus an explicit `bMenuOpen` blocker, rather than by edge consumption. `ReadInteractPressed` is deliberately non-consuming like its confirm/cancel siblings, so mutual exclusion between consumers is always written out explicitly and never implied.
- **"Rejections are silent" is now DISCHARGED (SC4, ZM-D-127):** `ZM_Interactable::Interact()` logs a `Zenith_Warning` naming the NPC, its id, its role and the seam it tried whenever a raise seam refuses, so a mis-authored NPC is no longer a mute one with no diagnostic.
- Still OPEN from that list: **the 8-line cap is cumulative per conversation**, not per pending line -- the queue never compacts as lines are consumed, so `QueueLine` cannot chunk a conversation past 8 total. Fine for the current NPC barks (the longest authored row is 3 lines); compact on advance or raise the cap if long conversations arrive at S7/S9.

**★ SC8 follow-ups (ZM-D-121) -- BOTH CLOSED at item-2 SC9 (ZM-D-122):**
- **The heal is no longer silent.** `ApplyDialogueChoice` now queues `ZM_CareCenterHealedLine()` onto the reset, unarmed box on a YES + `HEAL_PARTY` that actually healed something, and does NOT pop -- the ordinary read-to-the-end `CLOSED` path takes it down on the next confirm. This closed a real S8 vertical-slice risk: a button that appeared to do nothing.
- **The "panel stays shown while awaiting a choice" behaviour is now pinned** by a windowed assertion over the panel + text visibility and the question string, not just the two buttons.
- Still true and worth remembering: **the dialogue box is single-tenant.** Both `OpenCareCenterPrompt` and `PushDialogueLines` refuse while a choice is armed, so an unanswered question owns the box and any future prompt source must expect a `false` return.

**★ ENGINE UI NAV RULE (SC6, ZM-D-119) -- applies to every remaining screen:** **never wire bake-time `SetNavigation` links into a widget pool whose members are shown/hidden at RUNTIME.** `Zenith_UICanvas::NavigateDown` (and its siblings) consult the explicit link FIRST and fall back to the spatial `FindNearestFocusable` **only when the link is null** -- a non-null link whose target fails `IsVisible() && IsFocusable()` is dropped with NO fallback, silently swallowing the press. Any paged/partial pool (bag rows, dex cells, party slots) therefore breaks on every partial page, which is usually the DEFAULT state. Leave such pools unlinked and let the spatial search read liveness each frame; it works because the pools share an x. Explicit links are only safe on an ALWAYS-VISIBLE set (the ROOT menu's four entries are the one such case). Secondary reason: `SetNavigation` is **not serialized** by `Zenith_UIElement::WriteToDataStream`, so bake-time links exist only in tools builds and `_True` / `_False` would navigate differently.

**★ Windowed-gate rule (SC6):** a UI test that PARKS the canvas focus programmatically before confirming a button proves nothing about navigation -- it passes with the navigation completely broken. Drive real arrow-key edges and poll the focused element's name until it reaches the target, with a deadline and a flag that DEFAULTS TO FAILING so a phase that never runs fails.

**Historical bag/economy risks surfaced before the SC7 shop; all five are now resolved and the byte-codec boundary is pinned by SC2:**
- **RESOLVED -- zero-price items:** `ZM_ShopBuy` rejects `m_uBuyPrice == 0` before any mutation, so KEY items cannot be handed out through `SpendMoney(0u)`; the sell path likewise rejects non-sellable zero-price entries.
- **RESOLVED -- atomic buy:** `ZM_Bag::CanAdd` is the single non-mutating capacity rule, and `ZM_ShopBuy` checks it before deducting money, so a per-stack-cap rejection cannot charge without delivering.
- **RESOLVED -- capped-purse sell:** `ZM_ShopSell` refuses the transaction before removing the item when the purse cannot accept the full credit.
- **RESOLVED THROUGH THE CODEC (ZM-D-135/136) -- over-cap balances:** module 7 stores and restores the full `uint32`, so an edited value may exceed the gameplay cap. `AddMoney` credits nothing rather than clamping, underflowing headroom or wrapping; spending remains valid. The codec preserves this value without normalization.
- **RESOLVED (ZM-D-135) -- `SaveFormat.md` drift:** module 6 now names `uZM_BAG_MAX_STACK_COUNT` (999), and module 7 records `uZM_MONEY_CAP` (999999) as a gameplay credit ceiling rather than a storage cap.

### 1.7 Save / persistence

**Status: S7 item 1 durable model + pure schema-v1 codec COMPLETE (2026-07-21, ZM-D-135/136); slot/UI integration remains open.**
`Zenith_SaveData::Initialise("Zenithmon")` runs at boot and the between-tests clear hook is registered (both from S0). The `DontDestroyOnLoad` `ZM_GameStateManager` owns the complete durable inventory: party, deterministic transactional 16x30 boxes, seen+caught dex, packed 4096 story bits, 8 badges, bag/full-width money, daycare parents/egg/aggregate hatch progress, Battle Tower current/best/seed, unset-or-populated world resume data, and NORMAL-default options. `ZM_Monster` includes current/max PP, current HP, gender, friendship and nickname; caught `ABILITY_NONE` normalizes to the species regular ability. `m_bPendingWhiteout` remains transient and unsaved. `ZM_SaveSchema` now encodes that inventory as explicit-LE schema v1 across 11 ordered length-framed modules, with append-atomic writes, exact-length atomic reads, strict validation/statuses and a literal 824-byte golden. **Still absent:** save-slot Read/Write through `Zenith_SaveData`, damaged-slot UI, menu save, continue and milestone autosave. There is no historical v0 migration; the first real future schema change must add a version bump + literal migration test.

### 1.8 Story / world content

**Status: broader gameplay/world connectivity not started -- slice at S8, buildout at S9/S10.**
Dawnmere's generated terrain/grass scene now connects through a real authored home-door trigger to PlayerHome build 40 and back through a real authored exit trigger; `TownCenter`, `Door`, and `FromHome` placement are all exercised at runtime. PlayerHome and the outdoor home shell are deliberate greyboxes, and this is still only one door edge. Thornacre and Route1 have measured ignored terrain families, **not** scene/content implementations. **Dawnmere now has five authored, interactable NPCs**: the four S6 townsfolk plus rival Vesper, the first live trainer. The patrol is persisted by `ZM_Interactable` v2; v1 data loads stationary. Vesper's sight/challenge/battle, win reward/flag and loss/whiteout routes are automated. There are still no live route edges, gyms beyond the data row, badge-award gameplay, broader rival arc, or League. The remaining scene/content families depend on shared WorldSpec-driven authoring.

**S7 item 3 SC8 (ZM-D-156) adds the FIRST authored trainer and named two debts; W2
(ZM-D-158) retires the loss debt, leaving only Route-1 placement open. W3 (ZM-D-159)
retires the "no visual spotted beat at all" limit, but only partly, and books what it
deliberately cut.**
Dawnmere now authors a FIFTH interactable NPC, rival Vesper, at (490, 524) facing the
spawn approach; walking into his 8 m / 60-degree cone with a clear line of sight starts a
real trainer battle, and his identity survives save/reload by a zero-byte route.

1. **ROUTE-1 RE-PLACEMENT DEBT.** GDD canon puts rival battle 1 on "Route 1 (L5)". Route 1
   does not exist in S7, so Q-D authorised authoring him in Dawnmere instead. **When Route 1
   is authored (S9/S10), move him and RE-DERIVE every separation from scratch** -- the
   current placement's clearances (caretaker 27.2 m, warden 28.6 m, villager 40.5 m, spawn
   49.2 m) are arithmetic against Dawnmere's specific geometry and none of them transfers.
2. **RESOLVED 2026-07-28 (ZM-D-158) -- TRAINER LOSS / WHITEOUT.** The independent
   `ZM_RivalVesperWhiteout_Test` physically walks the canonical level-5 Grass starter to
   authored level-5 Fire rival Vesper, uses the real HUD, and reaches a natural ENEMY win
   with Catch and Run gated. It proves no prize/flag/EXP, full HP/PP/status recovery,
   exactly one TownCenter load with an independent placement oracle, fresh trainer-row
   derivation after reload, and 200 no-input frames without re-engagement at the 49 m
   clearance. The flagged-row loss loop risk is now exercised rather than argued.
3. **PARTLY RESOLVED 2026-07-28 (ZM-D-159) -- THE VISUAL "SPOTTED" BEAT.** A trainer no
   longer notices the player in complete silence. `ZM_TRAINER_SIGHT_SPOTTED` (appended,
   session-only, serialized nowhere) holds a 0.35 s cancellable window between first
   sight and the SC7 bark, drawing an asset-free yellow exclamation mark -- one vertical
   `Flux_Primitives` line plus one sphere, sized off `fabs(scale.y)` -- above every
   sighted trainer, silent rows included. The player is never frozen, so walking out or
   behind cover cancels cleanly; a busy channel pauses without consuming the sighting; a
   corrupt duration fails open on a free tick. **STILL OPEN, and cut on evidence rather
   than overlooked:**
   - **No camera cut -- STILL UNBUILT, but ★ THE BLOCKER THIS BULLET STATED WAS DISPROVED**
     (see "THE CAMERA CUT REMAINS UNBUILT" further down this section; corrected here 2026-08-01
     so the two do not disagree). `ZM_FollowCamera::OnLateUpdate` OWNS and overwrites the camera
     every frame and there is no override stack, so an override is genuinely still needed -- but
     `ZM_FollowCamera` is a **Zenithmon component (order 103)** and the SOLE writer of the camera
     pose, so it belongs INSIDE that component and needs **no `Zenith/` change at all**. This
     bullet used to call it "a real engine/game-camera feature, not a polish item"; whoever takes
     it should not re-inherit that estimate.
   - **★ THE FIXED HEADING IS THE SHIPPED DESIGN, NOT A SHORTFALL (ZM-D-173).** The camera
     keeps the yaw its scene authored and resolves occlusion by raycasting pivot -> desired
     position. What WAS a defect is fixed: ordinary engine raycasts used to report SENSOR
     bodies, so Dawnmere's `HomeDoorTrigger` collapsed the arm at the door it exists to
     open, and the Home shell sat entirely on the camera side of that doorway. Both are
     closed, and the contract is now enforced on the real physics world by
     `ZM_DawnmereCameraClearance_Test` -- the clamped arm must keep >= 50% of the authored
     6.0008 m pivot->camera distance at **308 named samples** (measured: violations=0,
     malformed=0). **That sample table is the enforceable boundary, not a proof about every
     standable point in Dawnmere**, and it deliberately carries no rings around NPCs, since a
     live NPC may legitimately occupy the ray. New S8 areas must extend it as part of their
     authoring.
   - **RESOLVED 2026-07-29 (SC1-SC3, ZM-D-163/166/167) -- THE APPROACH WALK, AND THE COLLIDER
     CLAIM WITH IT.** This bullet read *"No approach walk. Vesper is authored stationary with an
     OBB collider (ZM-D-156: an AABB destroys his authored yaw). Moving him correctly needs
     dynamic-capsule/nav ownership, avoidance, and freeze coordination with the
     order-110/111/112/113 seam."* All of that was then built, and **he is no longer OBB** --
     SC3 moved him to `CAPSULE`/`DYNAMIC` precisely so he could be driven, and his authored yaw
     survived it (`facingAbsDot=1.00000` off the re-authored bytes). Observed end to end: he
     closes **7.575 m -> 2.105 m** into a 2.0 m standoff in **43 frames / 1.467 s** at
     **3.729 m/s**, `worstBackstep=0.0000`, with the player frozen by `ZM_TrainerCinematicLatch`
     for all 43 frames and released in 1. Stale until 2026-08-01.
   - **RESOLVED 2026-07-30 (ZM-D-169) -- 1.8-3c, THE DEBUG-CHANNEL DEPENDENCY, AND IT IS PROVEN
     WITH PIXELS RATHER THAN WITH THE SUBMIT COUNTER.** The marker no longer rides the tools/debug
     channel. `Flux_PrimitivesImpl` gained two dedicated GAMEPLAY queues plus
     `SubmitGameplayCylinderAndSphere`, `ExecuteGBuffer` drains them UNCONDITIONALLY (the early
     return now fires only when debug AND gameplay are both empty), and they render
     `GBUFFER_SHADING_UNLIT` + emissive so readability does not depend on scene lighting.
     **Observed:** `ZM_RivalVesperAuthored_Test` holds `Graphics/Primitives/Enabled` **FALSE for
     the whole run** and a frame-exact swapchain dump on a real SPOTTED frame contains the marker --
     118 marker-hue px in a 7x28 span, that hue verified UNIQUE across the entire frame (zero
     matches anywhere else). **Mutation-proven:** restoring the old
     `if (!m_bPrimitivesEnabled) return;` early return reds that clause with "reached Flux's queues
     but NOT the framebuffer: 0 marker-hue pixels" and exit 1; reverting it returns to green.
     The queue-leak half is closed for the gameplay queues by the same unconditional drain.
     **★ STILL OPEN, and narrower than this entry used to be:** the DEBUG queues are still not
     drained while the option is off, so debug instances accumulate unconsumed -- pre-existing
     behaviour, untouched, and noted in `Zenith/Flux/Primitives/CLAUDE.md`. Promoting the marker to
     a real UI/mesh surface remains unnecessary: the gameplay primitive channel IS the production
     surface now.

   The honest one-line description is now **"a trainer who sees you shows you he has, walks up
   to you, speaks, and battles you"** -- **the camera still does not move.** (Updated 2026-08-01:
   this sentence ended "-- he still does not walk to you and the camera does not move", which
   ZM-D-167's approach walk had already falsified on 2026-07-29.)
4. **RESOLVED 2026-07-29 (ZM-D-160) -- RIVAL VISUAL DISTINCTNESS.** `ZM_NpcData::m_eHuman`
   is no longer declared-and-ignored: a TOTAL palette in `Source/Gen/ZM_HumanAppearance`
   derives one flat colour per `ZM_HUMAN_ID` from the SAME outfit/hair tables the SC3
   albedo painter uses, and `ZM_GreyboxVisual` resolves sibling `ZM_Interactable` -> row
   -> `m_eHuman` -> palette. Every non-NPC blockout (walls, floors, doors, lintels, props)
   keeps the shipped grey byte for byte. Measured on the COMMITTED rival:
   `vsGrey=0.6366`, `vsNearestNpc=0.2124`, both against a 0.15 margin.
   **STILL OPEN, and found by W4 rather than introduced by it:**
   - **A ROSTER APPEARANCE COLLISION -- RESOLVED 2026-07-29 (ZM-D-164), AND THE FIX THIS
     ENTRY USED TO RECOMMEND WAS WRONG.** There were SIX authored Dawnmere NPCs and
     `Npc_Wanderer` / `Npc_Warden` BOTH stood on `ZM_HUMAN_TOWN_ELDER` -- six rows, five
     appearances, those two pixel-identical. `ZM_NPC_ROUTE_WARDEN` now names
     `ZM_HUMAN_TOWN_WARDEN`, a new append-only roster row (`OUTFIT_WORKER` + `HAIR_BLONDE`,
     `(0.5425, 0.3895, 0.1410)`) whose nearest authored neighbour is the Villager at
     **0.20661** -- clear of the 0.15 margin and above the tightest pair the town already
     ships (Caretaker/Elder, 0.20064). Zero serialized bytes, no scene re-authoring, no
     regen, and the boot count did not move.
     **★ THE CORRECTION.** This entry previously named `ZM_HUMAN_TRAINER_RANGER` as a
     one-token fix that "can only RAISE the distinct-appearance count the boot unit
     asserts, so it cannot red anything." **Both halves were false.** `TRAINER_RANGER` is
     `OUTFIT_WORKER` + `HAIR_BLACK` -> `(0.337, 0.241, 0.0876)`, only **0.0873** from the
     Villager. Applied and BUILT, it red `ZM_Data::Npc_AuthoredAppearancesAreMutuallyDistinct`
     at `ZM_Tests_NpcData.cpp:1021`. The reasoning error was treating that unit as a pure
     counter: it has a SECOND arm requiring every pair naming different ids to be >= 0.15
     apart, and a fix can red that arm while raising the count. The root cause is that the
     greybox palette is `0.45*outfit.primary + 0.25*outfit.accent + 0.30*hair` and reads
     **neither skin nor build** -- so "a visibly different outfit family" is a property of
     the SC3 albedo painter, NOT of the one flat colour. A full 49-cell (outfit x hair)
     sweep found only six cells clear all six constraints; `WORKER`+`BLONDE` was taken over
     the higher-scoring `LABCOAT`+`WHITE` (0.21547) because `LABCOAT` is the professor's
     outfit and lands 0.2286 from Aster -- nine thousandths of margin for a future collision.
     **★ THE BOOT UNIT WAS RATCHETED, NOT MERELY UPDATED.** `uMIN_DISTINCT_APPEARANCES` was
     the literal `5u` against a `ZENITH_ASSERT_GE`, so a regression straight back to the
     collision would have passed GREEN -- demonstrated, not assumed: with the fix in place
     the old bound still passes at 6 >= 5. It is now derived as `(u_int)ZM_NPC_COUNT` (full
     injectivity), so a seventh NPC duplicating an appearance reds with no literal to
     remember. The fix also makes `uSharedPairs` unreachably zero, so that zero is now an
     explicit assertion rather than a counter that can no longer teach the unit anything.
   - **★★ THE ROSTER FIX ABOVE IS REAL AND VISUALLY MOOT. THE ACTUAL DEFECT IS SHADING, AND ONLY A
     SCREENSHOT COULD SAY SO (visual audit 2026-07-29, ZM-D-168).** Every NPC blockout renders as a
     near-black slab in play. Measured off the framebuffer, vertical faces sample
     **0.004-0.055** per channel while the terrain beside them sits at **0.44**, and rendered
     pairwise separations between NPCs are **0.017-0.041** -- an order of magnitude BELOW the 0.15
     margin `Npc_AuthoredAppearancesAreMutuallyDistinct` enforces and which
     `ZM_RivalVesperAuthored_Test` reports as `vsNearestNpc=0.2124`. **Both tests are honest: they
     sample `ZM_GreyboxVisual`'s MATERIAL BASE COLOUR and never read a rendered pixel.** The
     separation guarantee holds in material space and evaporates on screen.
     **★ THE CAUSE IS NOT THE PALETTE.** The blockout's TOP face renders light grey while its sides
     are near-black, so the geometry is being shaded normally -- the sun is near-overhead and the
     greybox material has effectively no ambient/indirect term, so vertical faces receive almost no
     light. A player at eye level only ever sees vertical faces. **The fix is a shading/ambient one
     and is far cheaper than the roster work this entry previously implied** -- and W4, ZM-D-160 and
     ZM-D-164 all overstated their user-visible effect. Evidence:
     `Build/artifacts/evidence_final/02_overworld_npc_blockout.png`.
   - **RESOLVED AT ENGINE LEVEL 2026-07-30 (ZM-D-171), AND THE RESIDUAL IS NAMED.** The engine's
     ambient was made physically grounded (virtual-ground bounce in the IBL cubes, the 0.5 IBL
     intensity fudge deleted, the sun key derived from the same atmosphere the sky renders — see
     the ZM-D-171 DecisionLog entry). Measured AS SHIPPED on the DawnmereHomeShell by the NEW
     `ZM_ShellLighting_Test`: the sun-averted vertical face went luminance **0.061 → 0.1672**,
     unlit/lit ratio **0.124 → 0.3688**, and the blue-black cast collapsed (blue/red **~7 → 1.13**).
     Every blockout benefits — walls, doors, props, not just NPCs — and ZM-D-169's NPC emissive
     floor is DELETED, restoring W4's "one row, one appearance" single-source property.
     **★ THE RESIDUAL IS ART DATA, NOT LIGHTING: the authored palette's minimum RENDERED pairwise
     separation under honest lighting is 0.0763 (Caretaker/Wanderer, sun-lit faces, measured
     2026-07-30) — the 0.15 framebuffer promise was only ever met via the emissive hack.**
     `ZM_NpcRenderedPalette_Test`'s floor was re-derived to 0.04 (severed-wiring fail state
     measured 0.0003–0.0009), pinning "wiring alive + palettes render distinguishably"; raising
     rendered distinctness back to 0.15 needs MORE-SEPARATED AUTHORED PALETTE COLOURS in
     `ZM_HumanAppearance`'s outfit/hair tables — booked here, deliberately not smuggled into the
     engine commit, and never again an emissive carrier.
     ★ **ZM-D-181 SUPERSEDED THE TEST, NOT THE DEBT.** NPCs wear generated human MODELS now, so
     that framebuffer test was measuring a picture the game had stopped drawing; it was DELETED
     rather than re-baselined against uncharacterised content (registry 56 -> 55). The palette
     itself survives as the COLD-START FALLBACK's colour, so the "more-separated authored
     colours" debt above is still real — it is simply no longer about what the player normally
     sees. Nothing currently reads real pixels off an NPC body.
   - **RESOLVED 2026-07-30 (ZM-D-170) -- CREATURE MODELS AND THE BATTLE HUD, BOTH NOW PINNED BY
     PIXELS. THE AUDIT'S NULL WAS AN OBSERVATION MISS, NOT AN ABSENCE.** This entry previously read
     "UNVERIFIED BY PIXELS ... no creature model and no HP panel / text log / Fight-Catch-Run menu
     was observed in any captured frame", and offered that the ~2-3-frame lit battle window between
     fades might explain it. It did. On the first frame examined properly, **all of it draws.**
     `ZM_BattleMenuRun_Test` now reads the swapchain TGA it was **already writing** and asserts six
     arms, every threshold centred between a measured pass state and a measured fail state:
     - **HUD.** Enemy HP bar chroma **G-R +0.428 / G-B +0.334** (suppressed: +0.007 / -0.005);
       Fight / Catch / Run at luminance **0.586-0.631** against the panel interior they sit on,
       delta **+0.310..+0.355** (suppressed: 0.277-0.339, +0.000..+0.063); battle text log **522**
       glyph-white px against **0** in a same-sized control box directly above it (suppressed: 0).
     - **Creatures.** Both platforms carry a rendered `Fernfawn`: body vs its own local background
       1 m to either side **0.191 / 0.234** and **0.219 / 0.241**; the two bodies read alike to
       **0.140**; each stands **0.918 / 1.052** clear of the slab under it.
     **★ THE HOLE WAS SHARPER THAN "NO COVERAGE", AND THAT IS THE TRANSFERABLE PART.** The test had
     dwelt 90 frames in `ACTION_ROOT` and written a real capture for a full commit -- and asserted
     only `DiskFilePresent(...)` on it. **Evidence produced and never read reads as coverage and is
     not.** Grep for a capture that no assertion opens before trusting any visual claim.
     **★ AND THE SCOPE IS NARROWER THAN THIS BULLET USED TO IMPLY.** It coupled the finding to "S4's
     five procedural asset generators ship". What is pinned is the CREATURE MODEL family for **one
     species**; the other four families remain claims about test names. **★ ONE ARM WOULD NOT HAVE
     BEEN ENOUGH:** with both models dropped, the body-vs-background arm still PASSED on the player
     side (0.834 / 0.927, that point falls on pale stone) and the *placement guard* caught it at
     0.007. Full reasoning, the three mutations, and the one threshold deliberately left loose (the
     suppressed HP bar reads sky at green 0.749, so the CHROMA arms discriminate and the level floor
     is not raised to fit what happens to be behind the bar) are in ZM-D-170.
     Evidence: `Build/artifacts/zenithmon/visual_audit/battle_menu_run_root.tga`; the audit's
     original frame was `Build/artifacts/evidence_final/03_battle_arena.png`.
   - **RESOLVED 2026-07-30 (ZM-D-169) -- THE MARKER'S SHAPE, AND IT IS NOW PINNED BY PIXELS.** It
     previously read as "a gold sphere with a diagonal stroke" rather than a bar above a dot. The
     stem is now a solid `SubmitGameplayCylinderAndSphere` cylinder instead of Flux's flat debug
     LINE quad, and a swapchain capture taken on a REAL SPOTTED frame shows an upright exclamation
     mark over the rival: **118 marker-hue pixels spanning 7x28** (height 4x width) in a 1280x720
     dump. `ZM_RivalVesperAuthored_Test` now asserts both the presence AND the tall-and-narrow span,
     so the diagonal shape cannot come back silently. **★ AND THE ROOT CAUSE WAS AN ENGINE BUG, NOT
     A TUNING MISS:** `RenderLinePrimitives` translates the line quad to `m_xStart` while
     `GenerateUnitLine` spans local y in `[-1, 1]`, so `AddLine(A,B)` draws CENTRED ON A -- the stem
     asked for `top+0.55 .. top+1.20` was drawn at `top+0.225 .. top+0.875`, straight through the
     dot at `top+0.25`. That bug is still live for every other `AddLine` caller in the engine and is
     booked separately (`task_33ee8059`, documented in `Zenith/Flux/Primitives/CLAUDE.md`); Zenithmon
     merely no longer depends on it.
   - **★ THE "UNEXPLAINED NULL RESULT" IS EXPLAINED, AND THE FAULT WAS IN THE SCAN.** The audit's
     yellow-pixel scan found zero marker frames while the run logged `submits=11`, and this entry
     recorded the honest worry that a submit might not be drawing. It was drawing. **Two independent
     causes, both in the measuring apparatus:**
     1. **The predicate was wrong.** The marker submits linear `(1.0, 0.82, 0.08)` unlit at 1.5x,
        which *sounds* like "saturated yellow, almost no blue" -- but after tonemap/bloom/TAA it
        lands at `RGB(208, 182, 97)`, blue/red **0.47**, not 0.08. Every "low blue" filter rejects
        it. Re-running the same style of scan on 2026-07-29/30 reproduced the zero across **539**
        frames of two different tests while the marker was rendering perfectly.
     2. **The sampling rate was unreachable.** `Tools\capture_viewport.ps1 -IntervalMs 40` delivered
        **206 ms** at 2560x1440 and 81 ms at 1280x800 -- PNG encode dominates the loop -- so the
        header's "use 60 ms or lower" advice cannot be honoured at full size, and a 0.35 s beat gets
        1-2 samples.
     **The lesson, which is the opposite of the one this entry was heading toward: a null result
     from a hand-rolled screen scrape is evidence about the scrape.** The fix was to stop scraping
     and use the engine's frame-exact `Flux_Screenshot::RequestDump` from inside the SPOTTED frame,
     then build the colour predicate from the bytes actually written.
   - **★ DAWNMERE READS AS AN OPEN FIELD, NOT A TOWN.** Terrain, grass and height variation render
     well, but there are no buildings, paths or town structure -- the "Trade Post" and "Care Center"
     exist only as dialogue and shop logic attached to NPC blockouts standing in grass. Not a
     regression and not previously claimed as built, but it is what a first-time viewer sees, and
     the S8 vertical slice will be judged on it. Evidence: `02_overworld_npc_blockout.png`.
   - **★ THE GDD'S COUNTER-STARTER RULE IS UNIMPLEMENTED AND WAS BOOKED NOWHERE UNTIL 2026-07-29.**
     The GDD specifies that rival battle 1 uses the starter that COUNTERS the player's choice.
     Vesper's `ZM_TrainerData` row carries a FIXED L5 KINDLET instead. Neither ZM-D-156 nor ZM-D-158
     mentioned this -- it was found only when the rival-battle checkbox was audited before ticking. **It
     cannot be implemented before S8 ships the starter-choice SCREEN** (the data and grant landed at
     ZM-D-175; only the presenter is missing), because there is no
     player starter to counter. Debt against S8; the user ruled on 2026-07-29 to reword and tick
     line 161 for what actually ships and book this rather than block S7 on an S8 dependency.
   - **THE CAMERA CUT REMAINS UNBUILT, AND THE S7 TICK DOES NOT CLAIM IT.** `Roadmap.md:104`'s text
     is `cone + occlusion raycast -> freeze input -> approach -> dialogue -> forced battle -> defeat
     flags + prize money`; every one of those ships as of SC3. A cinematic camera cut was a W3
     ASPIRATION (ZM-D-159), never a Roadmap requirement, so it is neither claimed by the tick nor a
     reason to withhold it. **★ ZM-D-159's stated blocker was WRONG, though:** it recorded the cut as
     needing "a real engine/game-camera feature". A code survey found `ZM_FollowCamera` is a
     Zenithmon component (order 103) and the SOLE writer of the camera pose, so an override belongs
     INSIDE it and needs **no `Zenith/` change at all**. Whoever takes this should not re-inherit the
     engine-scale estimate.
   - **A NEW, SMALLER LIMIT TAKEN ON IN EXCHANGE.** `HumanGen_PaletteDistinctness`'s
     `aeCAST` / `uCAST_COUNT` are a HAND-MAINTAINED MIRROR of the `m_eHuman` column with no
     compiler edge to it. Deleting an id from `aeCAST` while leaving the count reds only
     *incidentally* -- MSVC value-initialises the missing slot to `0 == ZM_HUMAN_PLAYER_M`,
     which today happens to sit 0.0472 from Vesper. Had that zero-fill landed on a distant
     id the mutation would have passed clean, so that unit cannot detect "a cast id went
     missing", only "these ids collide". Trimming `aeCAST` and `uCAST_COUNT` together is
     green by construction. `Npc_AuthoredAppearancesAreMutuallyDistinct` reads the REAL
     column and is the unit that cannot be fooled this way.
   - **A LATENT PALETTE TRAP, BOOKED NOT FIXED.** `ZM_HUMAN_PROF_ASTER` (`LABCOAT`+`GREY`)
     resolves to `(0.4730, 0.5880, 0.5695)` -- only **0.0677** from the blockout fallback
     grey `(0.52, 0.55, 0.60)`, i.e. 45% of the margin. If Professor Aster is ever given an
     authored `ZM_NpcData` row, his greybox body will be indistinguishable from an UNWIRED
     one, and the vs-grey clause reds the moment he joins `aeCAST`. Untouched by ZM-D-164.
   - **THE COVERAGE BOUNDARY IS CLOSED (ZM-D-181).** `ZM_GreyboxVisual` is still a
     file-local class that cannot be NAMED from a `Tests/` TU, but boot units now drive
     it through its registered component META (`GetMetaByName("ZM_GreyboxVisual")` ->
     `m_pfnCreate` / `m_pfnOnStart`) -- see `Tests/ZM_Tests_HumanVisual.cpp`. No header
     was added; the registry was already the seam.
   - **THE BODIES ARE NO LONGER UNIT CUBES (ZM-D-181).** The six authored Dawnmere NPCs
     and the player wear the generated `ZM_HumanGen` models, animated Idle/Walk off
     commanded speed. **The honest new limit is narrower:** the model is what a WARM tree
     draws. On a cold clone with no human bake and no way to make one (a non-tools
     build), a resolved human falls back to a proportioned 0.8 x 1.8 x 0.8 palette block
     -- the exact body the game used to ship, in the same place. Its GAMEPLAY dimensions
     are identical either way, because the collider comes from the compiled contract in
     `Source/World/ZM_HumanBody.h` and never from the bake.
   - **W4's palette debt is RE-SCOPED, not closed.** The palette is now the cold-start
     fallback's colour rather than the shipped appearance, so "two authored appearances
     sit only 0.20 apart" is a claim about the fallback. It still wants more-separated
     authored colours; it is no longer what the player normally sees.
5. **RESOLVED 2026-07-29 (ZM-D-161) -- PER-NPC SAMPLED FEET HEIGHTS.** Every authored NPC,
   Vesper included, now stands on its OWN measured ground instead of one shared
   town-centre literal. **The measurement found a larger defect than the limit described:**
   the warden was authored **1.368 m** and the caretaker **1.095 m** above their own
   terrain, with a live spread of **1.782 m** under the six-NPC roster. The heights are
   MEASURED at runtime and FROZEN as compiled constants -- live sampling was rejected
   because the committed `.zscen` bytes must stay reproducible from compiled constants
   rather than from a gitignored terrain bake, and because there is no terrain physics body
   during authoring at all. `ZM_DawnmereNpcGroundTruth_Test` is the standing oracle and
   keeps "constant vs terrain" and "committed bytes vs terrain" as SEPARATE clauses so
   neither can hide the other. `Dawnmere.zscen` moved to SHA256 `3874943E...` under the
   full two-boot proof; `Dawnmere.znavmesh` did NOT move.
   **STILL OPEN:**
   - **The constants go stale if the terrain is re-baked**, and nothing reds at author
     time -- that is the accepted cost of freezing measured values, and the containment is
     that the probe test reds at the next local batch. It is the mirror of the risk that
     live sampling would have introduced, and the cheaper of the two.
   - **Only Dawnmere is measured.** Thornacre and Route1 have no authored NPCs yet; when
     they do, they need their own anchors, not this table.

### 1.9 Post-game

**Status: Battle Tower LOGIC exists (S2 box 6, 2026-07-12); the rest lands at S11.**
`ZM_BattleTower` supplies the deterministic tower logic (L50 clamp, streak scaling, AI escalation via `ZM_AI_TIER`, procedural-by-seed opponent teams, streak settlement) as of box 6. Still unbuilt: the Battle Tower SCENE + presentation/HUD, the BP/reward shop, Super/Ultra tiers, named tower trainers + lobby, the Champion rematch, and the balance-simulation tooling -- all S11.

### 1.10 Playthrough tests

**Status: broad automated coverage through S6 plus the complete S7 item 1 model/codec units; the remaining gap is slot integration and the later playthrough bots.**
Current automated coverage is **36 registered automated tests** (`zenith test Zenithmon`), plus a **2392-case boot unit suite** (**2391 passed / 0 failed / 1 skipped**). The full windowed suite is **36 passed / 0 failed / 0 skipped**, every test positive-frame; the headless suite is **36 passed / 0 failed**, with **3 semantic tests executed and 33 graphics-required tests skipped as expected**. Coverage now includes 18 `ZM_Save` durable-model contracts, 29 schema-v1 contracts and 2 literal 824-byte golden/compatibility contracts alongside boot, the controller harness, terrain/grass, warp + the PlayerHome round trip (831 frames), the full battle slice, asset galleries, S6 UI screens and S6 interaction suite. **Still missing:** save-slot/manual/continue/autosave integration tests, a segment test, `ZM_AutoTests_Slice`, and a full new-game -> Champion `ZM_AutoTests_Playthrough` bot. Suite-runtime budgets remain unproven at playthrough scale.

**★ STRUCTURAL COVERAGE CAVEAT -- read before trusting a green CI run.** Every test with `m_bRequiresGraphics = true` is **SKIPPED** in a headless run, **and the harness counts a skip as a PASS**. `zm-tests` runs headless. So a green `zm-tests` says nothing whatsoever about the windowed tests -- which is most of the gameplay-proving suite, including every walk-up-to-an-NPC test and the whole S6 interaction gate. Those are proven ONLY by the local windowed gate, and only by a run reporting `PASSED` with a **non-zero frame count**. This is a property of the harness, not a Zenithmon defect, but it is the single most likely way for this project to believe it is covered when it is not.

---

## 2. Known engine gaps being tracked (E1-E8)

All additive + back-compatible; each lands with unit tests + a RenderTest boot regression check. Sizes and rationale from the approved plan.

| # | Gap | Why it blocks Zenithmon | Lands at |
|---|-----|------------------------|----------|
| E1 | **RESOLVED 2026-07-12 (ZM-D-051):** serialized strict terrain-set name, empty exact legacy layout, all runtime/editor/automation paths routed, safe staged bake cleanup, v1-v4 compatibility | Zenithmon can isolate one terrain asset family per outdoor scene without regressing existing games | S3 |
| E2 | **RESOLVED 2026-07-13 (ZM-D-052):** inclusive anchor-containing bounded export, safe stale-mesh cleanup, and terminal/bounded missing-or-invalid HIGH streaming | Makes one cropped asset family per outdoor scene plausible; ZM-D-054 subsequently completed the three-real-recipe bake/file measurement and resolved Q-2026-07-09-002 | S3 |
| E3 | **RESOLVED 2026-07-17 (ZM-D-100):** additive visible-glyph-count typewriter reveal on `Zenith_UIText` -- `SetVisibleGlyphCount`/`GetVisibleGlyphCount`/`GetTotalGlyphCount` + the pure static `ClipToVisibleGlyphs`; default -1 = fully revealed (existing widgets byte-identical); v2->v3 serialization tolerant of old baked blobs; `Render()` clips only the submitted string (layout reserved) and stays zero-copy on the default path; 7 headless units | Every dialogue line and battle-text line needs it; belongs in the widget, not per-game hacks | S5 (SC1 of item 4) |
| E4 | **RESOLVED 2026-07-19 (ZM-D-113):** `Zenith_UIGridLayoutGroup` -- a fixed-column grid layout element alongside the existing horizontal/vertical LayoutGroup | Bag / box / dex grids are core UI surfaces | S6 |
| E5 | **RESOLVED 2026-07-16 (ZM-D-092):** `Flux_GrassImpl::Reset()` widened to a full `ClearSceneData()` (instances/flags/density map/chunks/counters) + wired into the SINGLE-load render-reset hook; +3 engine units | Leakage would put tall grass inside gyms and interiors | S5 |
| E7 | **DEFERRED future work -- navmesh OPTION B (ZM-D-145).** A RUNTIME-GENERATED overworld nav path: (a) build the navmesh on scene load from the terrain **COLLISION** mesh -- physics is live headless (ZM-D-127), so this sidesteps the render-side Q-2026-07-21-001 GPU-culling assert that blocks harvesting the live *render* terrain; (b) **TILING** so `Zenith_NavMeshGenerator`'s single-grid `iMaxDim=1024` clamp stops bounding resolution over a large domain (finer than ~1 m over 1024 m) -- this is exactly what UE (Recast `NavMeshBoundsVolume`) and Unity (`NavMeshSurface`, Terrain as a native source) ship for terrain worlds; plus (c) actual agent routing/local avoidance. Closely related to E6 (the fixed-4096 m grid). **Zenithmon does NOT need this now:** S7 trainers use straight-line approach (genre-authentic), and persistence **SHIPPED** via option C (committed disk-baked `.znavmesh` + the reusable engine bake/load feature, ZM-D-147/SC1b), which needs none of B. Lands as its own explicitly-gated ENGINE sub-commit (ZenithAI/terrain, owing the full engine-unit + RenderTest-regression + cross-game sweep) when populated-world NPC pathing/obstacle-avoidance actually needs it. | Roaming/obstacle-avoiding overworld NPC navigation in populated towns (beyond scripted straight-line trainers) | S9/S10 (or post-ship) |
| E6 | **DEFERRED, post-Zenithmon TODO.** Terrain world-space extent is a global compile-time constant (`Flux_TerrainConfig::CHUNK_GRID_SIZE`/`CHUNK_SIZE_WORLD`/`TERRAIN_SIZE`, `Flux_TerrainConfig.h:27-36`) -- every terrain is a fixed 4096x4096 m grid; density is likewise a fixed constant (`fLowLODDensity`, `Zenith_TerrainComponent.cpp:493`), not a per-instance field. E2's rect export only crops that same fixed grid, it does not resize it, so a tiny route and a large city are forced to the same world-space size today. Fix requires the grid constants to become per-instance serialized fields + dynamic GPU/CPU buffers in the streaming manager + a decoupled density field -- explicitly out of scope for Zenithmon per the E2 rationale ("compile-time constants pervade streaming/grass"). **Keep in mind on every terrain-touching task through the rest of development -- do not build content-side workarounds assuming this changes mid-project.** Revisit as a dedicated engine initiative after Zenithmon ships. | Post-S12 |
| E8 | **DEFERRED, booked 2026-08-01 (ZM-D-173), task_0515a49e.** **`Zenith_TerrainComponent` exposes no ground-height query** -- there is no `GetHeightAt(x, z)` of any name on its public surface (verified: the surface is all render/culling/material/asset-set accessors plus `HasPhysicsGeometry`/`GetPhysicsMeshGeometry`), so the ONLY way to ask where the ground is, is a physics raycast. That answers a materially different question -- *what is the first BODY below this point* -- and the difference is not filterable: **(a)** `Zenith_Physics::Raycast` / `Zenith_PhysicsQuery::RaycastIgnoring` take exactly ONE ignore entity, so two overlapping bodies over a column are unmeasurable; **(b)** restarting the ray below a hit does not rescue it, because anything STANDING on the ground has its underside AT the surface and anything deliberately embedded (the Home shell sinks 0.05 m so no visible gap opens) has it BELOW the surface; **(c)** it needs the terrain physics body STREAMED IN, so it is frame-dependent and unusable at authoring time -- the editor add path uses the deserialization ctor and never calls `LoadCombinedPhysicsGeometry`, so an authoring-time cast MISSES. **Two tiers:** TIER 1 serves the query from `GetPhysicsMeshGeometry()` as a triangle lookup (no Jolt, no body filtering -- removes the occlusion problem, still needs streaming); TIER 2 keeps or loads the heightfield (**note the runtime component holds NO height data** -- it loads baked mesh chunks, and `Terrain/<Set>/Height.ztxtr` is read only by the TOOLS editor path) and would make the query work at AUTHORING time. Lands as its own explicitly-gated ENGINE sub-commit (EntityComponent/terrain, owing the engine-unit + cross-game sweep). | **Bitten today:** ZM-D-173's Home door jambs sit on the shell's own face, so two solid bodies stand over each jamb's column; their authored heights are derived from ground sampled 0.5 m away and the residual is bounded by local relief but genuinely UNMEASURED, because the column that would measure it is the one that cannot be probed. TIER 2 would additionally retire the measure-once-freeze-as-a-constant workflow that `Source/World/ZM_DawnmerePlacement.h` (W5 + ZM-D-173) is built around. | Post-S12 (TIER 2); TIER 1 whenever a blockout a player can walk up to needs its own column measured |

---

## 3. Cross-cutting risks (tracked, not yet biting)

- **Terrain bake time / file volume -- MEASUREMENT COMPLETE (ZM-D-054):** calibrated Dawnmere/Thornacre/Route1 walls are **59.035 / 69.979 / 80.804 s** for 772 / 772 / 1,156 files. The 25-set planning model projects 24,676 files / 5.933 GB and 30m 40.833s repeated or 23m 55.857s one-boot/net; the exact-GDD 26-set sensitivity is 25,832 files / 6.196 GB and 32m 01.637s / 24m 59.787s. This closes Q-2026-07-09-002 without triggering optimization, but the full all-assets bake is still unmeasured, no byte ceiling exists, warm "seconds" is qualitative, and only one route was sampled. One set per outdoor scene remains mandatory; optimize the pipeline rather than share sets if later evidence breaches budget.
- **Encounter-transition hitch** -- additive battle design avoids terrain reload; S5 gate asserts the round-trip frame budget.
- **Battle-scene render bleed at offset** -- S5 screenshot gate; fallback SINGLE + snapshot (Questions.md Q-2026-07-09-003).
- **Test-suite runtime blowup** -- battle correctness stays headless; long playthroughs are `m_bManualOnly`; slowest-10 report + hard budget at every gate.
- **Save-schema churn** -- from S7, any schema change requires a version bump + canned-blob migration test in the same commit (gate rule).
- **Content volume (~40 scenes)** -- everything flows from `ZM_WorldSpec` + shared authoring helpers; WorldSpec integrity tests catch dangling references pre-bake.
- **★ CAMERA-RELATIVE MOVEMENT vs WORLD-SPACE TEST DRIVES -- FIXED 2026-07-20 (ZM-D-130/131), keep in mind for every new walk test.** Player movement is camera-relative (`ZM_PlayerController::BuildCameraRelativeDirection` off the main camera's facing dir) and `ZM_FollowCamera` re-aims at the player every frame from a LAGGING camera-to-player vector, so the world-space meaning of W/A/S/D **rotates as the player turns**. Every test-side `DriveTowardXZ` originally chose keys by comparing raw world dx/dz, which is correct ONLY for a single leg walked from rest. Measured failure: on a multi-leg walk needing a ~120-degree heading change the player ran a stable 45-degree WRONG heading at full run speed while holding W and died on the stall watchdog. All three copies now project the desired world direction onto the LIVE camera basis (degenerating to the old behaviour at yaw 0). **Any new traversal helper must do the same** -- copy the version in `Tests/ZM_AutoTests_NpcServices.cpp`.
- **★ WINDOWED TESTS ARE INVISIBLE TO CI, AND A SKIP COUNTS AS A PASS.** `zm-tests` runs headless, where the harness SKIPS every `m_bRequiresGraphics = true` test and records it as passed. Most of the gameplay-proving suite is windowed. A green `zm-tests` therefore does NOT mean the game works; the local windowed gate is the authority, and only a run reporting `PASSED` with a non-zero frame count is evidence. This is a harness property, not a Zenithmon defect, but it is the most likely way this project believes it is covered when it is not. Mutation-verified 2026-07-20: rewiring the NPC dispatch arms reddened the windowed tests while leaving the CI-visible one green -- which is why `ZM_NpcDispatch_Test` was hardened (ZM-D-131) to assert WHICH screen each role raises.
- **★ ENGINE GAP CLOSED 2026-07-21 (ZM-D-132): `Zenith_ComponentMetaRegistry::Finalize()` now detects duplicate serialization orders.** It previously had none: two components registered at the same order sorted arbitrarily against each other with no warning, and because the pre-sort source is a hash-map walk and `std::sort` is unstable, that ordering was not even reproducible between builds -- a silent, nondeterministic scene-output hazard, since serialization order decides the byte order components are written in. Finalize now tie-breaks the sort on the type name (making a collision at least deterministic), logs a `Zenith_Error` per colliding pair naming both components and the shared order, and emits a summary `Zenith_Check`. Six units land engine-side (the leaf cannot host them -- `ZenithECS` may depend only on `ZenithBase` and the test framework lives in `Zenith/Core`), including `DuplicateOrders_LiveRegistryHasNoCollisions`, which checks the ACTUAL registry every build ships. **Engine unit baseline 1097 -> 1103.** Zenithmon occupies orders 100-113; next free 114.
- **★ NEW ENGINE GAP, OPEN: terrain sets up GPU culling with no headless guard.** `Zenith_TerrainComponent::InitializeCullingResources()` asserts `Invalid buffer VRAM handle` in a headless run, because a headless boot short-circuits Flux and has no Vulkan device. Any terrain-bearing scene is therefore un-runnable headless, so **the terrain path has no CI coverage on a GPU-less runner**. Found while restoring the RenderTest canary; RenderTest works WINDOWED, which is how every local gate here runs. See Questions.md Q-2026-07-21-001.
- **★ RenderTest is usable as an engine canary again (2026-07-21).** Its terrain is now `_True`-baked (12,313 files / 1.78 GB, generated from seed 1337 by tools-only editor automation), the boot-time missing-chunk crash is gone, and `TerrainEditorSmoke` -- the terrain/grass canary the AgentBriefing engine-change gate names -- PASSES windowed. It was exercised as a real canary for the ZM-D-132 engine change. Residual: the windowed suite is 8/1, with `RT_TennisDeterminismDigest` failing for pre-existing, unrelated reasons (Q-2026-07-21-002).
- **Creature generator underestimation** -- its own stage (S4) with a gallery gate; archetype count can flex 8 -> 6 without touching the dex data model.
