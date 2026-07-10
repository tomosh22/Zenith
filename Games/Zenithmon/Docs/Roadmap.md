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
- [ ] `ZM_DataRegistry` (name->ID indices + validation); `ZM_Tests_Data` schema-enforcer suite

*Gate:* ~90 unit tests (chart matrix vs golden, stat/exp formula vectors, registry integrity).

## S2 -- Battle engine headless (L) -- parallel with S3/S4, after S1

- [ ] `ZM_BattleState` + `ZM_BattleEngine` (Begin(config,seed) -> SubmitAction -> ResolveTurn; append-only `ZM_BattleEvent` stream, no UI/string formatting in the engine)
- [ ] `ZM_MoveExecutor` (one executor switch over the effect enum) + `ZM_DamageCalc` + `ZM_CatchCalc` + `ZM_StatusLogic` (major + volatile statuses; documented cuts stay cut)
- [ ] Abilities via per-hook fn-pointer structs (~50 shipped) + weather (rain/sun/sand/snow)
- [ ] `ZM_ExpAndLevel` (4 exp-curve families, EVs, level-up/move-learn mid-battle; post-battle evolution via pure `Evolve()`)
- [ ] `ZM_BattleAI` tiers RANDOM / GREEDY / SMART / CHAMPION (pure fn of state+rng)
- [ ] `ZM_Breeding` + `ZM_Daycare` logic; `ZM_BattleTower` logic (level-50 clamp, rentals, streak scaling, AI escalation)

*Gate:* ~370 unit tests incl. scripted seeded scenario battles with exact expected event streams + 2,000-battle fuzz soak (termination < 500 turns, HP/PP/boost invariants).

## S3 -- First overworld (L) -- critical path

- [ ] Engine E1: serialized terrain-set name on `Zenith_TerrainComponent` (default "" = legacy `Terrain/`) replacing all 6 hard-coded path sites + `AddStep_TerrainSetAssetSet` + bake-target on the terrain editor (unit tests + RenderTest regression)
- [ ] Engine E2: `AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` + verify/add missing-chunk tolerance on the `Flux_TerrainStreamingManager` stream-in path
- [ ] Home Village terrain baked via `ZM_TerrainAuthoring` recipe; grass regenerated OnAwake
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
