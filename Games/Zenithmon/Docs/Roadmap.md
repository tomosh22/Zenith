# Zenithmon Roadmap -- S0..S12 Stage Plan (living checklist)

**Document purpose:** the single source of truth for "what's next". One section per stage; agents pick the next unchecked task on the active track and execute. Checkbox discipline: `[x]` = the PR is merged AND CI is green AND Status.md is updated; `[~]` = WIP this session; `[ ]` = pending. Never tick on a partial implementation.

**Stage gate (applies to EVERY stage, plus the per-stage additions listed below):** build green + unit tests green + `zenith test Zenithmon --headless` green + stage-scoped windowed `--filter` runs + CI `zm-tests` green + the stage's listed visual check + Docs updated (Roadmap.md, Shortfalls.md, and any affected format docs in the same PR).

**Critical path:** S0 -> S3 -> S5 -> S8 -> S9 -> S10 -> S12.

**Parallelism:** S1+S2 (pure headless logic) run alongside S3+S4; S6 alongside late S5; S11 alongside late S10. MSBuild dispatch is SERIAL (known mspdbsrv constraint) -- parallelism is code-authoring, not builds.

**Engine changes** (E1..E5, all additive + back-compatible, each with unit tests + RenderTest boot regression): E1 terrain-set paths + E2 rect export land first in S3; E3 typewriter + E5 grass reset land in S5; E4 grid layout lands in S6. See Shortfalls.md "Known engine gaps" for the whys.

---

## S0 -- Skeleton, harness, CI, Docs (S-M) -- COMPLETE (2026-07-10)

- [x] Scaffold via `zenith new Zenithmon` (`Zenithmon.zproj` + `Zenithmon.cpp` + game component + boot characterization test from `Build/Templates/NewGame`, then regen)
- [x] Engine change: game-name validators (`Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` + `ZenithHub_GameScan::ValidateName`) narrowed from blanket `Zenith*`/`Sentinel*` prefixes to a PascalCase word boundary; shared pinned vectors (`Tools/ZenithCli/Tests/name_validation_cases.txt`) + buildsystem tests updated (suite 45 / 0)
- [x] Flesh out `Zenithmon.cpp` `Project_*` entry points; `ZM_GameComponent` registered `"ZM_Game"` (order 100, S0 placeholder bobbing cube)
- [x] FrontEnd scene via editor automation (build index 0: camera + "Zenithmon" title text + game component; tools-build authored, git-ignored)
- [x] Hello tests on the unified `zenith test` harness: 2 unit tests (`Tests/ZM_Tests_Boot.cpp`) + 1 automated test (`ZM_Boot_Test`, `Tests/ZM_AutoTests_Boot.cpp`)
- [x] Between-tests hook + `Zenith_SaveData::Initialise("Zenithmon")` at boot (`Zenith_SaveData::ClearForTest` registered from S0)
- [x] `.github/workflows/zm-tests.yml` (dp-tests clone: zenith-setup, regen -UseDotnet, Vulkan_True build, D3D12_False link proof, DLL copies, headless boot check, `zenith test Zenithmon --headless`, results artifact)
- [x] `Docs/` seeded (Status / Roadmap / Questions / Shortfalls living docs + the reference docs per the plan's Docs table)
- [x] First PR from `zenithmon/s0-skeleton` opened and merged with `zm-tests` green (PR #143, rebase-merged as `4c35f55d` + `4e57c680`, all 10 checks green; en route, PR #144 `0844689e` fixed the 3 PRE-EXISTING master-red gates: engine-gate baseline 1053->1068, layering-gate Flux_HDR hoists, scaffold-smoke regen dotnet-fallback + lfs + SDK override)
- [x] `zm-tests` registered as a required branch-protection check (2026-07-10, user-directed via API -- master had NO protection at all, so classic protection was created: contexts `[zm-tests]`, strict=false, enforce_admins=false; see CIPolicy.md section 4)

*Gate:* **MET 2026-07-10** -- exe boots windowed + headless; `zenith test Zenithmon --headless` exits 0; first PR through zm-tests green.

## S1 -- Data core (M) -- parallel with S3/S4

- [x] `ZM_Types.h` + `ZM_TypeChart` (18 types) -- 18-type `enum ZM_TYPE` + golden-locked 18x18 chart + dual-type product; 9 `ZM_Data` unit tests (PR #147). Also wired the boot unit suite into CI (ZM-D-019).
- [x] `ZM_SpeciesData` (152 species: archetype + evo stage + size class + family seed + stats + learnsets) -- structural roster (id/name/types/archetype/evo/family/rarity + derived size/seed; PR #148 ZM-D-020) + derived base stats (PR #149 ZM-D-021) + derived level-up learnsets (`ZM_Learnsets.h`, `ZM_GetSpeciesLearnset`; PR #151 ZM-D-023). 24 `ZM_Data` tests (16 species + 8 learnset). Base stats + learnsets are systematic placeholders for the S11 balance pass; TM/tutor learnsets come with `ZM_ItemData`.
- [x] `ZM_MoveData` (218 moves as table rows over a 57-kind `ZM_MOVE_EFFECT` enum) -- data + schema only (`ZM_MOVE_ID`/`ZM_MOVE_CATEGORY`/`ZM_MOVE_TARGET`/`ZM_MOVE_EFFECT` + per-row power/acc/PP/priority/crit/contact/effect+chance+magnitude); 16 `ZM_Data` tests incl. every-effect-kind-used coverage lock (PR #150, ZM-D-022). The `ZM_MoveExecutor` that interprets the effect enum is S2.
- [x] `ZM_ItemData` (90 items + 25 TMs) -- compiled `ZM_ItemData` table over a 9-value `ZM_ITEM_CATEGORY` (ball/medicine/battle/held/berry/evo/TM/key/field) + 34-kind `ZM_ITEM_EFFECT`; per-row buy/sell/effect+param/consumable/taught-move; TMs reference real `ZM_MOVE_ID`s. Data + schema only (bag/battle executor is S2/S5). 11 `ZM_Data` tests incl. every-effect-kind coverage (PR #152, ZM-D-024).
- [x] `ZM_AbilityData` stubs (50 abilities: id/name/description + `ZM_ABILITY_HOOK` surface bitmask; fn-pointer hook bodies deferred to S2, ZM-D-026) + `ZM_NatureData` (25: exact 5x5 raised/lowered grid + `ZM_GetNatureStatPercent`, ZM-D-025). 12 `ZM_Data` tests (PR #153).
- [x] `ZM_StatCalc` (Gen-III+ integer stat formulas: HP + other-five with nature %, all integer-exact) + `ZM_BattleRNG` (PCG32 seeded RNG: Next/RandBelow/RandRange/Chance, deterministic). 10 `ZM_Data` tests incl. stat golden vectors + a golden PCG32 stream (PR #154, ZM-D-027).
- [x] `ZM_WorldSpec` skeleton (the keystone declarative world table) -- schema (`ZM_SCENE_ID`/`ZM_SCENE_KIND`/`ZM_SceneConnection`/`ZM_EncounterSlot`/`ZM_WorldSpec` row: name/build-index/kind/terrain/connections+spawn-tags/encounters) + an 8-scene proving set (FrontEnd/Battle/Dawnmere/Route1/Thornacre/2 interiors/Gym1). 11 `ZM_Data` referential-integrity tests (warps + spawn tags + encounters resolve, reachable from FrontEnd). Full world appended at S9/S10 (PR #156, ZM-D-029).
- [x] `ZM_DataRegistry` (name->ID lookups for species/move/item/ability/nature/scene, NONE on miss) + `ZM_Tests_DataRegistry` cross-table schema enforcer (name round-trips + every evolution/TM/encounter/learnset ref resolves). Closes S1 (PR #157, ZM-D-030).

*Gate:* ~90 unit tests (chart matrix vs golden, stat/exp formula vectors, registry integrity). **MET 2026-07-10** -- 102 `ZM_*` `ZM_Data` unit tests (boot suite 1172 ran / 0 failed); no visual check for S1, so the loop proceeds to S2.

## S2 -- Battle engine headless (L) -- parallel with S3/S4, after S1

- [x] `ZM_BattleState` + `ZM_BattleEngine` (Begin(config,seed) -> SubmitAction -> ResolveTurn; append-only `ZM_BattleEvent` stream, no UI/string formatting in the engine) -- **box 1 (keystone)**: `Source/Battle/` (ZM_BattleTypes/BattleMonster/BattleEvent/DamageCalc/BattleState/BattleEngine) + 14 `ZM_Battle` unit tests incl. an offline-oracle-derived exact-event-stream characterization test + 50-battle fuzz-invariant smoke. Ships a REAL minimal Gen-V `ZM_DamageCalc` (plain damaging moves) so box 1 is end-to-end testable to a faint; 3-architect design panel + reviewer pass. Arch = ZM-D-032.
- [x] `ZM_MoveExecutor` (one executor switch over the effect enum) + `ZM_DamageCalc` (EXTEND the box-1 minimal `ZM_CalcDamage`: burn/weather/screen inputs already seamed in) + `ZM_CatchCalc` + `ZM_StatusLogic` (major + volatile statuses; documented cuts stay cut) -- **box 2 COMPLETE** via 6 ordered sub-commits SC1-SC6 (ZM-D-033/034/035): executor seam, stat-stage effects, delivery variants + field/screen setters, 6 majors, 10 volatiles + Endure/Swagger/forced-switch, and `ZM_CatchCalc` + pre-move SWITCH/ITEM/RUN + the 2,000-battle soak. 228 `ZM_Battle` tests; move-only goldens byte-identical throughout.
- [x] Abilities via per-hook fn-pointer structs (50 shipped) + weather (rain/sun/sand/snow) -- **box 3 COMPLETE** via 5 ordered sub-commits SC1-SC5 (ZM-D-036/039/040/041/042): weather core, 6 switch-in abilities, 20 damage/stat/type-interaction, 15 contact/status-try/stat-veto/accuracy, and 9 turn-end/faint/quickdraw + the all-50 realization gate. Every one of the 50 ability rows realizes its declared hook mask (QUICKDRAW the sole engine-side-only row); zero-perturbation preserved throughout (NONE-actor goldens byte-identical); locked by SC1-SC5 behavioral tests + the all-50 coverage gate. Boot unit baseline 1510.
- [x] `ZM_ExpAndLevel` -- **box 4 COMPLETE** (ZM-D-043): four integer exp curves; derived per-species growth/base-exp/EV/evolution seams; current cumulative EXP; modern per-opponent party share (living participants full, living nonparticipants half, full EV yield); EV 252/510 caps; mid-battle level/stat/move learning; terminal level-evolution queue + pure one-edge `Evolve()`. Default-off progression preserves legacy streams/RNG byte-identically. Locked by 67 new tests (45 `ZM_Data`, 22 `ZM_Battle`); boot baseline 1577.
- [x] `ZM_BattleAI` tiers RANDOM / GREEDY / SMART / CHAMPION -- **box 5 COMPLETE** (ZM-D-044): pure side-effect-free `ZM_ChooseAction(const ZM_BattleState&, ZM_SIDE, ZM_AI_TIER, ZM_BattleRNG&)` in `Source/Battle/ZM_BattleAI.{h,cpp}`. RANDOM (uniform over the legal set), GREEDY (argmax deterministic expected-damage x accuracy), SMART (KO -> hopeless-switch -> heal -> GREEDY cascade), CHAMPION (deterministic 2-ply scalar-HP lookahead with a modeled GREEDY reply; beats greedy in the priority trap). Takes its OWN rng + reads state `const` -> zero perturbation of the battle RNG/state/events (box-1..4 goldens byte-identical). Locked by 28 `ZM_Battle` tests (incl. CHAMPION reply-model + speed-tie discrimination); boot baseline 1605.
- [x] `ZM_Breeding` + `ZM_Daycare` logic; `ZM_BattleTower` logic (level-50 clamp, rentals, streak scaling, AI escalation) -- **box 6 COMPLETE** via SC1 (ZM-D-045) + SC2 (ZM-D-046). SC1 `ZM_Breeding`/`ZM_Daycare`: deterministic egg-gen (offspring = base-evo of the mother; K-IV inheritance [3, or 5 w/ Heirloom Knot]; Stasis-Stone nature lock; mother ability; base-evo L1 learnset) + daycare (deposit <=2, 1 exp/step, 256-step egg threshold); reduced model on the shipped data (archetype = egg-group proxy; no gender / egg-groups / egg-moves -- Q-2026-07-12-004). SC2 `ZM_BattleTower`: pure logic (L50 clamp via `ZM_BuildBattleMonster`; streak->AI-tier bands 7/21/35 + one-tier boss bump consuming `ZM_AI_TIER`; procedural-by-seed 3-mon opponent teams; streak settle; `ZM_MakeTowerBattleConfig`) -- produces the setup + settles a streak, a caller runs the battle (Q-2026-07-12-005). 58 new tests (33 SC1 `ZM_Data` + 23 SC2 `ZM_Data` + 2 SC2 `ZM_Battle`); boot baseline 1663.

*Gate:* ~370 unit tests incl. scripted seeded scenario battles with exact expected event streams + 2,000-battle fuzz soak (termination < 500 turns, HP/PP/boost invariants). **PASSED 2026-07-12 (ZM-D-047):** boxes 1-6 all landed; boot unit gate 1663 ran / 0 failed (593 `ZM_*` game units = 209 `ZM_Data` + 384 `ZM_Battle` incl. the box-1 exact-event-stream scenario characterizations + the box-2 2,000-battle soak); the full 4-config Vulkan matrix (Debug/Release x True/False) + the `D3D12_vs2022_Debug_Win64_False` null-backend link proof all build green. No windowed/visual check in the S2 gate -> no GATE-WAIT. **S2 COMPLETE.**

*Post-S2 (user-directed 2026-07-12, ZM-D-048):* **feature-complete breeding + gender** completes the box-6 SC1 reduced model to the full mainline breeding scope (Scope.md Section 1 already locks "breeding/eggs/daycare" + "mainline mechanics"). SC-A gender foundation (13 tests) + SC-B real egg groups + GLOOPET Ditto-analog + gendered compatibility (ZM-D-049; 22 new + 24 re-baselined) + SC-C egg moves + ability/hidden-ability inheritance + a derived hatch-cycle accessor (ZM-D-050; 20 new tests) ALL DONE -- **breeding is FEATURE-COMPLETE** (gender + ratios, real egg groups, GLOOPET Ditto, gendered compatibility, IV/nature/ability/hidden-ability + egg-move inheritance, hatch cycles). Boot baseline 1718. Shiny/Masuda deferred to S5+ (ZM-D-048).

## S3 -- First overworld (L) -- critical path

- [x] Engine E1: serialized terrain-set name on `Zenith_TerrainComponent` (default "" = legacy `Terrain/`) replaces all 6 hard-coded path sites; strict contained paths, `AddStep_TerrainSetAssetSet`, staged editor bake-target, backward-compatible v1-v4 serialization, 7 engine tests, and RenderTest/CityBuilder/DevilsPlayground regressions are green (2026-07-12, ZM-D-051)
- [x] Engine E2: inclusive anchor-containing `AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` with transactional stale-mesh cleanup, shared terrain chunk layout, and terminal/tolerant missing-HIGH streaming (3 engine tests + full engine regression matrix green; 2026-07-13, ZM-D-052)
- [x] Home Village (`Dawnmere`) terrain baked via deterministic `ZM_TerrainAuthoring` recipe (seed `0x7BF32CA4`, world `0..1024`, 16x16 chunks); warm scene boot regenerates/uploads exactly 200,159 grass blades from 5,133 terrain triangles on both first load and reload; 772-file terrain family + `Dawnmere.zscen` remain git-ignored; no trees in this first terrain deliverable (2026-07-13, ZM-D-053)
- [ ] Measure terrain bake time with 3 real scenes BEFORE committing to ~25 terrains (Questions.md Q-2026-07-09-002)
- [ ] `ZM_PlayerController` (Jolt capsule, velocity-driven) + `ZM_FollowCamera` + `ZM_InputActions`
- [ ] Persistent `ZM_GameStateManager` (`DontDestroyOnLoad`) + `ZM_WarpTrigger`/`ZM_SpawnPoint` spawn-tag respawn
- [ ] Player home interior + door warp round trip (SINGLE loads + fade)

*Gate:* E1/E2 unit tests + RenderTest still boots green (terrain regression); windowed automated test walks the village + door round trip via input-simulator state-setters; visual check terrain/grass/camera.

## S4 -- Asset generators (L) -- parallel with S1/S2

- [ ] `ZM_GenCommon` (seeded RNG + loft toolkit) + `ZM_TextureSynth`
- [ ] `ZM_CreatureGen` -- all 8 archetypes (QUADRUPED/BIPED/AVIAN/SERPENT/AQUATIC/INSECTOID/BLOB/FLOATER-PLANTOID), mesh+skeleton via StickFigure loft, palettes/patterns, shiny variants, dex icons (the single biggest work item, ~7-8.5k lines)
- [ ] `ZM_CreatureAnimGen` -- 6 clips per archetype template, instantiated + exported per species (~900 .zanim)
- [ ] `ZM_HumanGen` -- one shared skeleton + one shared 9-clip set; ~35 models
- [ ] `ZM_BuildingGen` / `ZM_PropGen` -- ~30 building models + ~25 props
- [ ] `ZM_BakeManifest` guards (generator-version stamp + file-existence; byte-identical re-bake invariant)

*Gate:* generator unit tests (determinism = same-seed byte-identical; winding/bounds/weights-sum/bone-caps; shiny differs; clip channels match skeleton); windowed gallery scene showing every species animating (batched species smoke tests); visual sign-off on a sampled dozen.

## S5 -- Battle integration slice (L) -- critical path

- [ ] Battle scene (build index 1, world offset (0,-2000,0), enclosing dome + platforms, ~6 baked biome prop sets swapped at runtime)
- [ ] `ZM_EncounterZone` + `ZM_TallGrassSystem` (own CPU density copy, 1 m tile quantization, per-route encounter rolls) + engine E5 grass-reset hygiene
- [ ] Additive load / `SetScenePaused` / camera + HUD switch round trip; grass cleared entering interiors/battle
- [ ] `ZM_BattleDirector` (event-stream interpreter, `zm_instant_battles` DebugVariable) + `ZM_UI_BattleHUD` + engine E3 typewriter
- [ ] Catch / exp / faint / whiteout applied to GameState

*Gate:* windowed tests -- walk grass until encounter (rigged RNG), win via scripted input, assert exp + exact overworld resume; catch test; screenshot check for overworld bleed-through at offset (Questions.md Q-2026-07-09-003).

## S6 -- Dialogue, menus, NPCs, shops (M) -- parallel with late S5

- [ ] Engine E4: `Zenith_UIGridLayoutGroup` (fixed columns/cell size)
- [ ] `ZM_UI_DialogueBox` / `ZM_UI_MenuStack` / `ZM_UI_Party` / `ZM_UI_Bag` / `ZM_UI_Dex` / `ZM_UI_Shop`
- [ ] `ZM_Interactable` + NPC graphs via `ZM_GraphAuthoring`; `ZM_NpcWalker` (navmesh wanderers)
- [ ] Care Center heal + mart buy/sell

*Gate:* UI-state unit tests + automated: talk / buy / heal / open-every-menu via focus navigation.

## S7 -- Save/load, story flags, trainer battles (M)

- [ ] Full `ZM_SaveSchema` (versioned per-module Read/Write: party, boxes 16x30, dex bits, story-flag bitset, badges, bag, money, daycare/egg, tower streak, position/scene/spawn tag) round trip + canned-blob migration tests
- [ ] Story-flag gating (roadblocks, NPC lines); menu-save anywhere + autosave at milestones (slots Save0-2 + Auto)
- [ ] Trainer sight-cone (forward cone + occlusion raycast) -> freeze input -> approach -> dialogue -> forced battle -> defeat flags + prize money
- [ ] Rival battle 1

*Gate:* automated save -> quit-to-FrontEnd -> continue restores position/party/flags exactly.

## S8 -- Vertical slice, go/no-go (M) -- critical path CHECKPOINT

- [ ] Intro -> lab -> starter choice
- [ ] Route 1 (encounters / trainers / items) -> town 2
- [ ] Gym 1 (layout puzzle-lite + leader + badge + teach-move reward)
- [ ] `ZM_AutoTests_Slice` mini-playthrough (CB_HumanSession-style flat Act script + PROBEs, ~4-6k frames, windowed)

*Gate:* mini-playthrough new-game -> Badge 1 green; manual visual playthrough sign-off. **Checkpoint before content scale-up.**

## S9 -- World buildout A (XL) -- critical path

- [ ] Towns 3-6, Routes 2-8, Gyms 2-4, interiors (content = `ZM_WorldSpec` rows + recipes, not bespoke code)
- [ ] Encounter tables per route
- [ ] Field evolution / stones; TM items
- [ ] Daycare / breeding / eggs (step counting on the manager)
- [ ] Weather zones (WorldSpec-driven, feeds battle modifiers)

*Gate:* per-region automated traversal tests (every warp edge walked, one scripted battle per route); bake-determinism check (re-run tools boot -> zero diffs).

## S10 -- World buildout B (XL) -- critical path

- [ ] Towns 7-11, Routes 9-15, Gyms 5-8
- [ ] Victory Road; League (4 Elite rooms + Champion)
- [ ] Full rival arc; badge-gate audit

*Gate:* traversal tests for all remaining scenes; automated Elite-4 gauntlet with overleveled scripted team.

## S11 -- Post-game (M) -- parallel with late S10

- [ ] Champion rematch row
- [ ] Battle Tower (interior scene + streak/rental logic + AI escalation, boss every 7th)
- [ ] Balance pass driven by headless AI-vs-AI simulation stats

*Gate:* headless 100-streak simulation invariants; automated 7-battle tower run.

## S12 -- Full playthrough & hardening (L) -- critical path

- [ ] Per-chapter segment tests in the batch suite
- [ ] `ZM_AutoTests_Playthrough`: full new-game -> Champion scripted run (CB_HumanSession pattern + `zm_instant_battles`, `m_bManualOnly`, run explicitly at this gate)
- [ ] Perf pass (grass counts vs 2M cap, load times, suite runtime <= budget)
- [ ] Save-migration audit; fix backlog

*Gate:* full suite green + playthrough bot completes + budgets met.

---

**End state (from the plan):** ~500+ headless unit tests (~370 battle/data + generators + save + world integrity), ~60-100 automated tests (scene traversals, battle smokes, UI flows, segment playthroughs) in a minutes-scale headless batch + bounded windowed set, 1 slice playthrough + 1 full playthrough bot, bake-determinism check. 4-config matrix check at major gates (Debug/Release x True/False; D3D12_False link check) per repo norm.
