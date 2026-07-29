# Zenithmon -- Shortfalls & Gap Analysis

**Document purpose:** a frank, gap-by-gap audit of where the current `Games/Zenithmon` tree falls short of the shipping vision in [GameDesignDocument.md](GameDesignDocument.md). This is the most-accurate-current-state doc; cross-check [Status.md](Status.md) before acting on any single line.

**Scope note (2026-07-09, S0):** at S0 essentially EVERYTHING is a gap -- the project is a booting skeleton. This doc is deliberately structured per major system with a one-line current-state each, so later sessions UPDATE lines in place rather than restructure. When a stage lands, replace that system's status line and add a dated note; do not reorder sections.

**Verdict at a glance (updated 2026-07-21, S7 item 1 COMPLETE):**
- **Stages complete with their required gates closed:** S0-S6; S7 is underway with item 1 closed through SC2. S1's data core and deterministic headless battle engine (including feature-complete breeding/gender and Battle Tower logic); S3's traversable Dawnmere + live PlayerHome door round trip; S4's five procedural asset generators + `ZM_BakeManifest`, visually approved (ZM-D-088); S5's full overworld<->battle slice, visually approved (ZM-D-112); and S6's dialogue/menu/NPC/shop surface all remain complete.
- **S6 (Dialogue, menus, NPCs, shops) COMPLETE.** Four authored Dawnmere NPCs are reachable by walking up and pressing the interact key: villager, Trade Post clerk, Care Center caretaker and wanderer. The wanderer uses a deterministic two-waypoint patrol; `ZM_Interactable` v2 persists the patrol configuration and v1 data loads as a stationary fail-closed fallback. Behaviour-graph and terrain-fed navmesh work deliberately moves to S7.
- **S7 item 1 full schema-v1 codec is green (ZM-D-135/136):** SC1's 18 durable-model units are joined by 29 schema + 2 literal-golden compatibility units. The pure codec freezes 11 ordered length-framed modules, explicit little-endian widths, transactional streams and an 824-byte v1 golden. Units are **2392 ran / 2391 passed / 0 failed / 1 skipped**; engine remains **1103**; all five Zenithmon builds, headless **36/0** and full windowed **36/0/0** passed; registry remains 36. No visual/human gate applies.
- **Next autonomous work:** S7 item 2, story-flag gates plus Save0-2/Auto slot wiring, manual save/continue and milestone autosave. Trainer-glue graph and terrain-fed navmesh evaluations remain later S7 work. There is no human gate during S7; the next human intervention point is the S8 vertical-slice go/no-go.
- **What's designed but unbuilt:** the remaining outdoor terrain recipes and all playable Thornacre/Route1 scene content; trainer/story-gating/badge-award gameplay and the rival/League arc (S7-S8); the BOX storage screen and the save-slot/manual/continue/autosave flows (S7); and the broad world buildout (S9/S10). The inner disk-payload codec itself now exists. PlayerHome and the outdoor home shell are intentionally replaceable greyboxes, not final art. The full ~25-terrain-set world bake remains a projection (AssetManifest.md 6.3), though the four asset-generator families have been cold-baked together at family scale.
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
The persistent `WarpFade`/`BattleFade` are real full-screen transition surfaces. **The battle UI now exists** (S5 item 4, ZM-D-103/104/109): `ZM_UI_BattleHUD` renders a typewriter text log (via the E3 `SetVisibleGlyphCount`, ZM-D-100) + two HP panels (species/level/HP + fill bar), and an interactive **Fight/Catch/Run** action menu with a move submenu (gapped-moveset compacted), all authored on the `BattleDirector` entity at sort order > 10001 (above the fade). The engine globally orders the shared quad queue by UI sort key across canvases, preserves equal-key submission order, drops newest beyond 1,024 with one warning per frame, and keeps the highest-sort overlay's text clip; `DiscardPendingFrame` centralizes the pending-queue/counter/clip reset across both Text paths. **Those screens now all exist** (S6 item 2, ZM-D-114..122): `ZM_UI_MenuStack` (order 112, the pause root), `ZM_UI_DialogueBox`, `ZM_UI_Party`, `ZM_UI_Dex`, `ZM_UI_Bag` + money, and `ZM_UI_Shop` + `ZM_ShopLogic`, all NON-ECS presenters owned BY VALUE by the menu stack and dispatched by FOCUSED ELEMENT NAME. Still missing: the BOX (storage) screen, which needs S7 persistence.
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
   - **No camera cut.** `ZM_FollowCamera::OnLateUpdate` OWNS and overwrites the camera
     every frame and there is no override stack. A cinematic cut needs camera-ownership
     arbitration -- a real engine/game-camera feature, not a polish item.
   - **No approach walk.** Vesper is authored stationary with an OBB collider (ZM-D-156:
     an AABB destroys his authored yaw). Moving him correctly needs dynamic-capsule/nav
     ownership, avoidance, and freeze coordination with the order-110/111/112/113 seam.
   - **The marker rides the DEBUG primitives channel.**
     `Zenith_GraphicsOptions::m_bPrimitivesEnabled` defaults true and Zenithmon never
     overrides it, so it renders by default -- but it is bound to a live debug variable,
     so a tools user who unchecks `Graphics/Primitives/Enabled` loses a GAMEPLAY cue, and
     `ExecuteGBuffer` early-returns before draining so the queued instances leak while a
     trainer is SPOTTED. Promoting the marker to a real UI or mesh surface is deferred.

   The honest one-line description is now **"a trainer who sees you shows you he has,
   then speaks, then battles you"** -- he still does not walk to you and the camera does
   not move.
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
   - **A COVERAGE BOUNDARY, not a gap.** `ZM_GreyboxVisual` is a file-local class in
     `Zenithmon.cpp` and cannot be named from a `Tests/` TU, so no boot unit can construct
     one. Its wiring is covered ONLY by `ZM_RivalVesperAuthored_Test`'s live material
     scan. Adding a header purely to unit-test it was rejected as the worse trade; the
     boundary is demonstrated by mutation rather than asserted.
   - The bodies are still **unit cubes**. W4 makes them distinguishable, not
     human-shaped; the generated human meshes are not wired to authored NPCs.
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

## 2. Known engine gaps being tracked (E1-E7)

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
