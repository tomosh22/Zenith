# Zenithmon Status

**Last updated:** 2026-08-18

**Board:** `ZM` on the agent board. **The work items live there** — epics, stories,
tasks, bugs, blockers, sprints and releases — and [Board.md](Board.md) explains the
split. What stays HERE is the three things a board cannot be the authority for:

1. **The pinned unit baselines**, immediately below. An authority you need a browser
   to read is not one a gate can be reconciled against.
2. **The committed-asset hashes**, which are what a determinism proof is checked
   against.
3. **The current task**, for a session starting cold — and for a loop worker, which
   has no network and reads this directory off disk with the Read tool.

The S0-S7 narrative that used to fill the back half of this file moved VERBATIM to
[History.md](History.md) on 2026-08-18, so this file can hold to the ~25-line budget
its own template in `AgentBriefing.md` §2.3 specifies. Nothing was deleted.

**★ LIVE PIN (UPDATED 2026-08-23):
ZM boot `3394`; engine boot (Null Combat) `1650`; Null RenderTest `1741`; registry **68**.**

> **★ R1-3 (ZM-21 / ZM-D-203) — OBSERVED 2026-08-23.** `Null_vs2022_Debug_Win64_True`
> reported **`3389 ran / 3387 passed / 0 failed`** (2 skipped), so `Tools/unit_baselines.json`
> moved `Zenithmon` **3388 -> 3389** — the +1 is the four-gate payload needle
> (`ZM_CommittedSceneBytes/EverySeamGatePayloadIsAuthoredExactlyOnceInItsOwnScene`).
> Engine `1650` and Null RenderTest `1741` are UNMOVED and were **inferred, not measured** —
> the diff is confined to `Games/Zenithmon/**`, so no backend-neutral engine unit moved.
>
> **★★ TWO NARRATION NUMBERS ON THE LINE ABOVE WERE STALE BEFORE THIS SLICE, and neither is
> gated.** The boot figure read `3387` while `Tools/unit_baselines.json` — the file the gate
> actually reads — held `3388`. The cause is now traced: `28046d81` (ZM-20) bumped the
> manifest `3384 -> 3388`, a +4, while its narration said "+3" and landed on 3387 — the
> manifest took the MEASURED number and the prose took ARITHMETIC. This is the SECOND time
> this block has run one behind the manifest (see the +11 narrative below for the first).
> `registry` was **correct at 67** and is now **68** with `ZM_SeamRoundTrip_Test`.
>
> **★ COUNT THE REGISTRY FROM A RUN, NOT FROM `grep`.** A first pass at this block put 71
> there, from `grep -c ZENITH_AUTOMATED_TEST_REGISTER Games/Zenithmon/Tests`. The engine
> registers **68** (`zenith test` discovers 9 PASS + 59 MISSING = 68). The three extra hits
> are **comments** in `ZM_AutoTests_TrainerSight.cpp:35,44,57`, each of which says in words
> that the registry count does NOT move — so a comment denying a test was added is counted
> as a test.
>
> That matters beyond this block, because **`Tools/doc_lint.ps1`'s C1 uses the same oracle**
> ("Count `ZENITH_AUTOMATED_TEST_REGISTER` call sites. Each maps to one registered test"),
> so its `registerCount` is 71 against a true 68. C1 only fails a doc that OVERSTATES
> (`$claimed -gt $registerCount`), so an inflated oracle makes it *more* permissive: a doc
> could claim 71 and pass. The `registry **N**` form does not match C1's regexes either
> (`N/M passing`, `N tests passing|registered|...`), so this line is unchecked regardless.

> **★ R1-4 (ZM-22 / ZM-D-204) — OBSERVED 2026-08-24.** `Null_vs2022_Debug_Win64_True`
> reported **`3394 ran / 3392 passed / 0 failed`** (2 skipped), so `Tools/unit_baselines.json`
> moved `Zenithmon` **3389 -> 3394**. The +5 are all `ZENITH_TEST` boot units:
> `WorldSpec_Route1EncounterRatePinnedAt20`, `BiomeForScene_Route1IsMeadowAndIdTagCorrect`,
> and three in the new `ZM_Tests_RouteEncounterSeam.cpp`. **Registry UNMOVED at 68** — the
> slice adds no `ZENITH_AUTOMATED_TEST_REGISTER`. Engine `1650` and Null RenderTest `1741`
> UNMOVED and inferred, not measured: no file under `Zenith/` was touched.
**★ +11 on EVERY game across two ENGINE tickets, no `ZM_*` unit added.**
3354/1638/1729 -> **3360/1644/1735** (ZM-49, +6: the terrain COLLISION-height
query `TryGetGroundHeightAt` -- 4 m quads, NOT the rendered ground) ->
**3365/1649/1740** (ZEN-2, +4 then +1: its chunk-granularity reject, plus the
recorder test a review found missing) -> **3366/1650/1741**, an engine +1 that
moved `Tools/unit_baselines.json` and was never narrated here, which is why this
block read one behind the manifest on all three rows until 2026-08-22 ->
**3384**/1650/1741 (ZM-27, +18 `ZM_*` units: 14 `ZM_GroundItem`, +3 in the save
migration TU, +1 in the schema TU) -> **3388**/1650/1741 (ZM-20, committed-bytes needles
for Dawnmere, Route1 and Thornacre) -> **3389**/1650/1741 (ZM-21/R1-3, +1: the four-gate
payload needle) -> **3394**/1650/1741 (ZM-22/R1-4, +5: the ROUTE1 rate pin, the biome
id-tag self-check, and three synthetic roll-seam units). Each number OBSERVED on `Null_`.

★★ **THIS CHAIN READ `3387` FOR ZM-20 UNTIL 2026-08-23, AND THAT WAS WRONG BY ONE.**
`28046d81` moved `Tools/unit_baselines.json` **3384 -> 3388** — a +4 — while this prose
narrated "+3 needles" and landed on 3387. So the miscount was INSIDE one ticket: the
manifest took the measured number and the sentence took the arithmetic, which is exactly
the failure the `$never` note in `unit_baselines.json` warns about ("never from arithmetic
on the previous number"). It is the SECOND time this block has run one behind the manifest
(the first is recorded two lines above), and nothing gates the pair — see the double-star
note at the top of this file.
★ ZM-27 measured **3384** from a real `Null_` Zenithmon run; 1650 and 1741 are
CARRIED from the manifest, not re-observed -- its diff is confined to
`Games/Zenithmon/**`, so no backend-neutral engine unit moved and `zagent gates`
unioned in no engine gates.
★ **A backend-neutral ENGINE ticket moves this block and may not touch it** --
ZEN-2 is `category: Engine`, so its gate list never opened a ZM doc and left
this line reading 3360/1644/1735 until someone noticed.
**★ The GATE reads `Tools/unit_baselines.json`, not this block** (2026-08-21) --
bump the number there, in the same commit as the tests that moved it. This block
is the human narration of WHY it moved and is read by no gate, so a stale line
here reds nothing; a stale number in the manifest reds a REQUIRED check with zero
failing tests. The warp machine now has a FRAME BUDGET (ZM-D-200, +5 units; measured real barrier dwell is **0 frames**, so 3600/1800/600 can only fire on a genuine hang). Before it, Q-2026-08-15-002 closed (`9b5a401b`, +3 `ZM_FollowCamera` boot units; the camera acquires the player by COMPONENT now -- no scene byte moved). Before it, R1-2 phase 2 step 2 shipped
`Route1.zscen` + `Thornacre.zscen` and the measured-ground split: registry 65 -> **67** (the two
region ground oracles), boot units **UNMOVED at 3346** (the split renamed constants and migrated
their callers; it added no unit). Observed `3346 ran / 3344 passed / 0 failed / 2 skipped` and
`67 passed / 0 failed`. **Seven committed scenes now**, the five older ones byte-for-byte unchanged.
The PREVIOUS block, still accurate for its own change: R1-2 phase 2 step 1
(the Dawnmere route-seam ground oracle) added +1 boot unit and +1 automated test; observed
`3346 ran / 3344 passed / 0 failed / 2 skipped` and `65 passed / 0 failed`, with the whole
`Assets/` tree untouched. The PREVIOUS pin block, still accurate for its own change, read:
ZM boot `3345`, registry **64**, observed
2026-08-15 on a clean `Null_vs2022_Debug_Win64_True` Zenithmon build after S8 item
2 slice **R1-1** and its review follow-up
(`3345 ran / 3343 passed / 0 failed / 2 skipped`). The follow-up added **+1**
(`ZM_WorldTraversal.Thornacre_SettledCameraStandsAboveGroundBehindTheArrival` --
the camera unit Thornacre's header comments already claimed existed, while its
five camera constants had no reader); its other four fixes are CLAUSES inside
existing units and move no count. R1-1 itself added
**+16 ZM units and no engine units**: 4 `ZM_SceneRegistry` (the new enumerable
scene-registration table), 8 `ZM_WorldTraversal` Route 1 placement units, 3
`ZM_WorldTraversal` Thornacre stub units, 1 `ZM_TerrainRecipeSet` material unit.
Registry UNMOVED at 64 -- R1-1 adds no automated test. Engine UNMOVED at 1638 --
no file under `Zenith/` was touched. **R1-1 authored no scene and moved no
committed `.zscen` byte**, so it needed no windowed authoring boot.
The earlier walk this program:
**3277 -> 3280** (SC-A, +3) **-> 3295** (SC-B, +15: 14 pure `ZM_Starter` units +
1 `FrontEnd.zscen` needle) **-> 3299** (SC-C, +4: three `ZM_WorldTraversal`
placement units + 1 `ProfLab.zscen` needle) **-> 3307** (SC-D, +8 lab-site units)
**-> 3312** (SC-E, +5: 1 `Dawnmere.zscen` seam needle + 4 `ZM_WorldTraversal`)
**-> 3327** (SC-F, +15: 14 `ZM_Intro` + 1 `ZM_Data` gate-polarity unit)
**-> 3328** (Q-2026-08-15-001, +1 `ZM_WorldTraversal` diagonal-walk-up unit)
**-> 3344** (S8 item 2 slice R1-1, +16 as broken down above)
**-> 3345** (R1-1 review follow-up, +1 Thornacre camera unit).
Registry 61 -> 62 (`ZM_DawnmereLabGroundTruth_Test`) -> 63 (`ZM_LabRoundTrip_Test`)
-> 64 (`ZM_IntroBeat_Test`).
Engine UNMOVED at 1638 -- no slice touched a file under `Zenith/`.

**★★ FIXED 2026-08-21 (ZM-50) -- REVERSED: a red naming `GraphComponent::ThousandEntityUpdateBenchmark` now means INVESTIGATE, never re-run quiet.** It asserts `Zenith_BehaviourGraph::NeedsUpdateDispatch()` deterministically -- the ON_UPDATE merge gate that keeps an idle graph off the dispatch path before any snapshot allocation, fixed at graph construction. A ratio bound was rejected first: quiet-box idle/active measured **0.702** (1.922 vs 2.738 ms/frame), against ~1.0 for a fully broken gate.
**★ One exception:** the active phase's own absolute `ZENITH_ASSERT_LT(fPerFrameMs, 20.0, ...)` guard (large margin, O(n^2)/allocation-storm catch) is unchanged -- a red naming IT is still worth a quiet re-run first. History: Questions.md Q-2026-08-14-001, DecisionLog ZM-D-192.
**★ NEVER PIN FROM A `Vulkan_` EXE:** the same tree reported **3332** on Vulkan
against **3295** on Null, a standing +37 gap.
**★ `registry` read 55 here and that was ALSO stale** -- this block was never
updated when the input program's WP3b grew the automated suite 55 -> 61 with the
six `ZM_Touch*` tests, even though the prose LOWER IN THIS SAME FILE said so
explicitly. Corrected to the enumerated count. (That paragraph named
`zm-tests.yml -Baseline 3328` and `run_unit_gate.ps1` default 1638 as "the current
pins"; both moved to `Tools/unit_baselines.json` -- see the note at the top.)

**★★ 3276 -> 3277 IS A FIX-FORWARD, NOT A FEATURE BUMP. THE `zm-tests` GATE WAS
RED ON master BEFORE THIS COMMIT.** `3aeaa2d4` ("share the grass/rock ground sets
as engine assets") added exactly one ZM unit --
`ZM_TerrainRecipeSet.DawnmereMeadowSamplesTheSharedEngineGrassSet` in
`Tests/ZM_Tests_TerrainRecipeSet.cpp` -- and did not move the pin. The gate
asserts `ran == Baseline` EXACTLY (`Tools/run_unit_gate.ps1`: `$fullSuite =
($ran -eq $Baseline)`), so a suite that GREW fails the gate exactly like one that
shrank: observed `[unit_gate] baseline NOT met (wanted 3276 ran, 0 failed; got
'... 3277 ran, 3275 passed, 0 failed, 2 skipped')`, exit 1, with **zero failing
tests**. Engine default stays 1638 -- `3aeaa2d4` touched no file under `Zenith/`.
**★ THIS IS THE THIRD RECORDED INSTANCE OF THE SAME FAILURE MODE** (ZM-D-173 found
the pin reading 2804 against a real 2809; d0b400c8 moved the pin but skipped its
History entry, so the changelog jumped 3238 -> 3276 with no derivation -- both
backfilled in `zm-tests.yml` by this commit). The lesson is not "remember to bump
the number": it is that **the only way to know this gate is green is to RUN it**,
because a passing local `zenith test` says nothing about it -- `zenith test`
passes `--skip-unit-tests` and never executes a single `ZM_*` boot unit.

The earlier move from 3238/1600 was **+38 ZM / +38 ENGINE units** from the terrain indirect-count
compatibility plan (Phase 1/2/6/7): 27
Flux_IndirectDraw policy/ABI/batch-planner tests, 5 CommandLine tests for the
new `--indirect-count-mode` flag, 3 FluxTerrain tests for the shared
indirect-command ABI / allocation boundary, plus 3 RenderGraph reset/cull/
cyclic-seed barrier tests.
The previous move from 3228/1590 is **+10 ENGINE units** (no ZM units)
from the input program's closing WP6: `Zenith_UserSettings` (the persisted
profile-override store; SaveData init is now ENGINE-owned — Zenithmon.cpp no
longer calls `Zenith_SaveData::Initialise`) + the SYSTEM_BACK device-layer
flag that keeps Android's Back-consume decision layer-clean. The input
program (WP0-WP6) is COMPLETE: ZM's windowed suite is 61 tests (sole known
failure `ZM_InteriorTintPixels_Test`), its controls table lives in
`Source/ZM_Bindings.h` (see CLAUDE.md ## Controls).

The PREVIOUS move, 3224/1586 -> 3228/1590, was **+4 ENGINE units** (no ZM units)
from the input program's WP4a — the B10 graph action nodes
(OnActionPressed/Released/Held, ReadActionAxis1D/2D) + builder factories that
Combat's migration rides on; ZM's graphs can adopt them at leisure. The move from 3211 is **+13 ZM units** (engine UNMOVED at
1586) from the input program's WP3b — Zenithmon's migration onto the engine
action layer: `ZM_Bindings.h` (three profiles P_KEYBOARD/P_TOUCH/P_GAMEPAD,
the full C2 binding table incl. the PAD column and SYSTEM_BACK on CANCEL),
`ZM_TouchLayoutController` (ECS order 114; OVERWORLD/DIALOGUE/MENU/BATTLE/
TITLE contexts retargeting the four authored touch widgets), and the deletion
of `ZM_InputActions.h`. The windowed automated suite grew 55 → **61** (six
`ZM_Touch*` tests); its sole failure remains `ZM_InteriorTintPixels_Test`.
`FrontEnd.zscen` was deliberately re-baked (+812 B: the hidden `ZM_TouchRoot`
entity with four virtual widgets + the controller) and is byte-stable across
authoring boots (SHA pinned during the WP3b gate).

**★★ CURRENT COMMITTED-ASSET HASHES -- RE-OBSERVED AND RE-PROVEN 2026-08-15 (before slice
R1-2).** Every row below was taken from the file on disk at `f7e42a01` and then PROVEN by
**two consecutive windowed `Vulkan_vs2022_Debug_Win64_True` authoring boots**
(`--automated-test ZM_Boot_Test --skip-unit-tests`), both reporting
`warmMask=0x7, queueMask=0x0, sceneAuthoring=AUTHOR_DAWNMERE` -- i.e. Dawnmere authoring
genuinely RAN, not `DEFERRED` -- after which `git status` over the WHOLE
`Games/Zenithmon/Assets/` tree was **empty**. Authoring is deterministic on this tree today.

> **★ THREE OF THE FIVE ROWS WERE STALE, AND THAT IS THE FIFTH RECORDED INSTANCE OF THIS
> PATTERN IN THIS REPO** (cf. the ZM boot pin being stale three times, and Q-2026-08-02-001,
> where a stale row was mistaken for drift). `Dawnmere.zscen` read `76E33E53...`,
> `FrontEnd.zscen` read `F7209CF5...` and `ProfLab.zscen` read `1BCAABC9...`, while the files
> really hashed as below. **None of that was drift** -- each moved in a legitimate commit
> (ProfLab at `5d9d73bf`, FrontEnd at the WP3b touch-root change, Dawnmere across several) and
> the table simply was not refreshed. **The cost of leaving it stale is real and specific: the
> next agent to run the two-boot proof for R1-2 would diff against these rows, see a
> mismatch, and start hunting a determinism bug that does not exist.** Refresh this table in
> the same commit as any deliberate re-author.

| Asset | Bytes | SHA256 |
|---|---|---|
| `Route1.zscen` | 2,253 | `09E165E0888D6213E4E031B0A3D39D0F32C2BA2B37B8E5557C2F7FD38BB353B4` (**RE-AUTHORED at R1-3**, ZM-21/ZM-D-203: +374 bytes, the `Route1SouthGate` and `Route1NorthGate` entity records appended. Two consecutive windowed Debug boots, second byte-identical. Previous value `666AC621AD11C0DEA7F6B716...`, held from R1-2 step 2.) |
| `Thornacre.zscen` | 1,923 | `DB4AC7790604F3862F67D8F0C8563C396260F9AB118ADF614275EF0314298604` (**RE-AUTHORED at R1-3**, ZM-21/ZM-D-203: +190 bytes, the `ThornacreSouthGate` entity record appended. Same two-boot proof. Previous value `A9295117F0F781D2608F33D0...`, held from R1-2 step 2.) |
| `Dawnmere.zscen` | 5,682 | `C819C84106AA42FBB6B33C892D0C339AD75E536EA01AB6B7B9891BD6FA53F2F5` (**RE-AUTHORED at R1-3**, ZM-21/ZM-D-203: +189 bytes, one `DawnmereNorthGate` entity record appended. Two consecutive windowed Debug boots, both `sceneAuthoring=AUTHOR_DAWNMERE, warmMask=0x7, queued=0`, second byte-identical. Previous value `F163F33BBA7BD8A4606AB70BF6287E819F476C605E0F961F1C353047B0801421`, held from R1-2 step 3.) |
| `Battle.zscen` | 4,965 | `1BEB0615F7FE62D9439471A4123E1D2140C0053AEC2991B659F7A03288C8C60A` (unchanged since 2026-08-05) |
| `FrontEnd.zscen` | 29,740 | `D44D540512F1C373A5D5E747CE7FA76E7D19B467F5F1563EB298E229EEFBEDB5` |
| `PlayerHome.zscen` | 1,832 | `DBBFB78311A55BBF942A7A5BF9928F43E9493A10CDA89110515A3B6A7987C780` (unchanged since 2026-08-05) |
| `ProfLab.zscen` | 2,068 | `72DA12B73AB643B44F0B9374FCD6F4CCF865ECBAED5F9B0D2832E8BD972ABB32` |
| `Dawnmere.znavmesh` | 373,412 | `DCAA84035A258B12FA23627FF719C0567018470C8055A1E0FB54D6C1F1F96E1D` (unchanged) |

**★ WHAT THIS BASELINE IS FOR.** Slice R1-2 authors two NEW scenes and re-authors Dawnmere.
Because the pipeline is proven deterministic *immediately before* that change, any byte that
moves unexpectedly during R1-2 is attributable to R1-2 and not to pre-existing instability --
which is exactly the ambiguity that cost a cycle at ZM-D-183 and again at Q-2026-08-02-001.

**SUPERSEDED (kept for the reasoning trail):** the 2026-08-05 table read `Dawnmere.zscen`
`76E33E53...` "proven by THREE boots: Debug x2 + Release x1". That proof was valid when taken;
the row is simply older than the file.

> **★ THE PROOF PROTOCOL NOW NAMES THE CONFIGURATION (the ZM-D-183 lesson).** Two boots
> with matching SHA256 prove determinism only WITHIN one build configuration -- the three
> boots that resolved Q-2026-08-02-001 were all Debug, which is exactly why ZM-D-183's
> per-config drift hid behind a green proof. This row was taken from **Debug x2 AND
> Release x1**, so it proves idempotence and cross-configuration agreement at once.
## Current task

# ════════════════════════════════════════════════════════════════════════════
# ★★★ COLD-START BLOCK -- WRITTEN 2026-08-15 FOR A NEW SESSION. READ THIS FIRST.
# ════════════════════════════════════════════════════════════════════════════

**STATE:** master is CLEAN. **Nothing here is ever PUSHED** -- the agent loop runs
`push: false`, so "pushed" was never a state this block could report; it said so anyway
until 2026-08-24.

**★ THIS BLOCK CARRIED PIN NUMBERS AND THEY WENT 44 UNITS STALE.** It read
`ZM boot pin 3345, automated registry 64, engine pin 1638` -- against an actual
3389/68/1650 at the time it was caught -- while the LIVE PIN line at the top of this file
was correct. That is a FOURTH data site in one file (LIVE PIN, the `+N` chain, the
committed-asset hash table, and this), none of them gated against each other, and this is
the one a cold session and a loop worker are told to read FIRST. **The numbers are gone
from here on purpose: read the LIVE PIN line at the top.** One home per value.

**S8 ITEM 1 IS COMPLETE AND TICKED** (six slices `864296df`..`0e0a884c`, ticked `f4d30f89`,
plus `5d9d73bf`). A new game is PARTYLESS and the starter is genuinely chosen from Professor
Aster; `ZM_IntroBeat_Test` proves it end to end across 18 phases.

**S8 ITEM 2 ("Route 1 -> town 2") IS IN PROGRESS. SLICES R1-1, R1-2 AND R1-3 ARE
COMPLETE (ZM-D-197; ZM-D-198/199/202; ZM-D-203). R1-4 IS PARTIAL (ZM-22/ZM-D-204):
the rate retune + id-tagged biome table + headless synthetic roll-seam proof are
DONE; the SCENE-ATTACH half ("encounters live on Route 1" in the literal,
in-game sense) is UNDONE and needs a windowed re-author no headless worker can
perform -- see the R1-4 row below. THE NEXT TASK is deciding how to close that
half (human/`needs-gpu` re-author, or re-scope) before R1-5.**

> **★ R1-3's SOURCE AND ITS BYTES MUST LAND TOGETHER.** The four gates are authoring
> STEPS in `Zenithmon.cpp`; the committed `.zscen` bytes only move when a **windowed
> `Vulkan_vs2022_Debug_Win64_True`** tools boot re-writes them (Dawnmere additionally
> needs `sceneAuthoring=AUTHOR_DAWNMERE`), and that boot must carry
> `--skip-unit-tests` -- the new committed-bytes clauses are RED until it runs, and a
> failing boot unit aborts the boot **before** scene authoring, so without the flag they
> block their own fix forever. **Re-observe the three `.zscen` hashes in the
> committed-asset table above in the same commit.** All three scenes move: Dawnmere
> gains one entity record, Route 1 two, Thornacre one.

### ★ WHAT R1-1 LANDED (the PURE slice -- no scene authored, no committed byte moved)
Six new files + three modified, +16 boot units (3328 -> **3344** OBSERVED), registry
unchanged at 64:
* `Source/World/ZM_Route1Placement.h`, `Source/World/ZM_ThornacrePlacement.h` -- the compiled
  anchors both scenes will be authored from (entity names, gate resolvers over the world
  table, arrival markers, camera family, gate volumes, the two trainer stations).
* `Source/World/ZM_SceneRegistry.h` + a rewritten `Project_LoadInitialScene` that **WALKS**
  the table instead of five hand-written `RegisterSceneBuildIndex` calls. Route1 (20) and
  Thornacre (3) are registered NOW, before their scenes exist -- a registration without a
  scene is inert, a scene without a registration is a permanent black screen. **This kills
  critic blocker #1 structurally.** Accepted cost: two unloadable rows in the tools editor
  toolbar until R1-2 authors the scenes.
* Slot 0 of `s_axRoute1Materials` / `s_axThornacreMaterials` now sample the shared engine
  grass set at Dawnmere's tiling (base colour WHITE -- it multiplies the sampled diffuse).

### ★★ THE THING R1-1 ALMOST SHIPPED, AND THE RULE THAT COMES OUT OF IT (ZM-D-197)
The R1-1 plan specified scene-unique player names (`"Route1Player"`), justified by
*"player resolution is by COMPONENT, never by name"*. **That is FALSE.**
`ZM_FollowCamera::ResolveTarget` uses `FindEntityByName("Player")`
(`Components/ZM_FollowCamera.cpp:390`) -- production, scene-agnostic, and the ONLY
`FindEntityByName` call in the whole game. A renamed player leaves the follow camera with no
target and `PollForCameraAndBeginFadeIn` bare-returns forever: **a permanent black screen with
no timeout, no crash and no red test.**
**RULE: the player entity is named `"Player"` in EVERY Zenithmon scene, Route 1 and Thornacre
included.** Consequence: `"Player"` is a substring of `"ZM_PlayerController"`, so committed-bytes
needles on it use the STRICTLY-MORE clause, never `== 1`, and it is excluded from any
entity-name-vs-type-name battery.
**★ AND THE PROCESS LESSON: a plan that asserts a NEGATIVE about coupling ("nothing resolves X
by name") must be made to PROVE it.** That premise rode through an otherwise rigorous
68,000-character spec because everything around it was checked in detail. Three adversarial
critics run BEFORE any code was written returned BLOCKED and caught it, plus a trainer anchor
that was geometrically unable to ever see the player, a test threshold that was arithmetically
unsatisfiable, and a miscounted unit total. **Run the critique gate before the implementers on
every remaining slice -- it cost ~700k tokens and saved a windowed re-author plus a shipped
black screen.**

### The four USER RULINGS are already recorded -- do NOT re-ask them (ZM-D-196, `c2311559`)
1. **Ground items are SPLIT OUT** of this item into their own Roadmap line. They are new
   production surface, not content.
2. **Thornacre is a TRAVERSAL STUB** -- terrain + `FromRoute1` marker + player + camera +
   return trigger, explicitly NO gym door, backed by a byte needle asserting none exists.
3. **TWO trainers, both persisted by per-trainer `ZM_STORY_FLAG_ID`.**
4. **Keep the species, halve Route 1's encounter rate to ~20/256** via a Route-1-specific
   named constant -- NEVER by editing the shared `uZM_DEFAULT_ROUTE_ENCOUNTER_RATE`.

### ★ THE FINDING THAT RESHAPES THE ITEM: ROUTE 1 IS ALREADY MOSTLY BUILT
`ZM_WorldSpec` **already ships** Route1 (build index **20**, kind ROUTE) with encounter slots
{PIPWIT 2-4 w40, NIBBIN 2-4 w40, SPARKIT 3-5 w20} at rate 40/256, Thornacre (build **3**,
TOWN), **all six** Dawnmere<->Route1<->Thornacre(<->Gym1) connection edges, and every spawn
tag those edges target. Both terrain recipes are authored AND WARM on this machine
(`Assets/Terrain/Route1` 1157 files, `Thornacre` 773). **Zero `ZM_WorldSpec` edits, zero new
build indices, zero new terrain bakes.** What remains is scene authoring, trainer content and
test coverage.

### ★★ THREE STRUCTURAL FACTS THAT DRIVE THE SEQUENCING
1. **THE SILENT-HANG TRAP IS ARMED TODAY.** `IsWarpDestinationValid(20,"FromDawnmere")`
   returns TRUE right now against an empty disk -- it reads ONLY the compiled tag list, never
   the destination scene. `WAITING_FOR_SPAWN` has NO timeout. A trigger without its marker is
   a **permanent black screen**: no crash, no red test. **Hence markers land FIRST (R1-2, zero
   triggers anywhere) and ALL FOUR triggers SECOND (R1-3). DO NOT merge or reorder those two.**
2. **`ZM_TallGrassSystem` is registered (ECS order 109) but attached to NO authored scene**, so
   no shipped scene can emit a wild encounter. The battle plumbing exists and has simply never
   had a producer. Route 1 is where it comes alive.
3. **Route 1 CANNOT be terrain-free.** `WorldSpec_TerrainByKind` reds any ROUTE/TOWN row with an
   empty terrain set, and the encounter loop is grass-density driven. Authoring needs a
   **WINDOWED `Vulkan_*_True` boot with `--skip-unit-tests`** and all three recipes warm.

### The ten slices (four need a WINDOWED authoring boot)
| id | title | +units | re-authors | deps |
|---|---|---|---|---|
| ~~R1-1~~ | ~~Placement headers + per-recipe terrain materials (PURE)~~ **DONE, +17 (not ~12), ZM-D-197** | 17 | none | -- |
| R1-2 **ph.1** | ~~Per-recipe terrain materials (split out of R1-2, PURE)~~ **DONE, ZM-D-198** | 0 | none, PROVEN | R1-1 |
| R1-2 | Author Route1 + Thornacre -- **MARKERS ONLY, zero triggers** | ~7 | creates Route1+Thornacre, re-authors Dawnmere -- **WINDOWED** | R1-1 |
| ~~R1-3~~ | ~~**All four seam triggers in ONE commit** + round-trip proof~~ **DONE, +1 (not ~4), ZM-21/ZM-D-203 -- closes critic blocker #2** | 1 | all three, DONE -- **WINDOWED**, two boots, second byte-identical | R1-2 |
| R1-4 | Wild encounters live + rate retune (ruling 4) -- **PARTIAL, ZM-22/ZM-D-204: rate + id-tag + synthetic roll-seam proof DONE; SCENE-ATTACH ("live") BLOCKED, see below** | ~2 | none (but "live" needs one -- see Decision 4, ZM-D-204) | R1-3 |
| R1-5 | Trainer DATA + placement + Npc claim-check rewrite | ~12 | none | R1-4 |
| R1-6 | Author the two trainers into Route1 | ~3 | Route1 only -- **WINDOWED** | R1-5 |
| R1-7/8 | **DROPPED from this item by ruling 1** (ground items) | -- | -- | -- |
| R1-9 | Camera-clearance coverage + re-derived budget | ~3 | none | R1-6 |
| R1-10 | Warden relocation onto Route 1 (do LAST, isolated) | ~3 | Dawnmere + Route1 -- **WINDOWED** | R1-9 |

### ★★ FIVE BLOCKERS THE CRITICS FOUND. FOLD THE FIX INTO THE SLICE BRIEF -- DO NOT REDISCOVER.
> **STATUS AFTER R1-3: #1 is RESOLVED** (the compiled registration table + its 4 boot units
> shipped; Route1 and Thornacre are registered already). **#5 is RESOLVED for the new names**
> -- they are scene-unique and collision-checked -- **EXCEPT the player, which is deliberately
> `"Player"` and must use the strictly-more needle form (ZM-D-197).** **#2 is RESOLVED**
> (ZM-D-203): `ZM_CommittedSceneBytes/EverySeamGatePayloadIsAuthoredExactlyOnceInItsOwnScene`
> needles the whole `[version][targetBuildIndex][32-byte tag]` payload per gate, and -- because
> a COUNT alone still cannot see a swap within one file -- also pins the interleaving of the two
> Route 1 payloads with their own entity names. **#3's FIX IS DONE (ZM-22/ZM-D-204):** the biome
> table is id-tagged + self-checked and the roll seam has a headless synthetic-density-map proof
> (`Tests/ZM_Tests_RouteEncounterSeam.cpp`) -- but the SLICE GOAL #3 was written against
> ("encounters live on Route 1") is not, because that needs a scene re-author of `Route1.zscen`
> no headless worker can perform (see the R1-4 row above). #4 is still OPEN and belongs to R1-9.
1. **[R1-3] The proposed hang-guard unit CANNOT EXIST as described.**
   `Project_LoadInitialScene` is a hand-written sequence of five `RegisterSceneBuildIndex`
   calls with **no enumerable table**, and it runs **AFTER** the boot-unit suite. A boot unit
   has nothing to read and can only re-spell the indices -- a second inventory that moves with
   nobody. The mutation it claims to catch (author the triggers, forget the registration) is a
   **second, independent permanent black screen**: `PollForTargetScene` is a bare
   `if (!IsTargetSceneActive()) return;` with no timeout.
   **FIX:** add to R1-1 a compiled registration table `{ZM_SCENE_ID, file stem}` and make
   `Project_LoadInitialScene` loop over it; the boot unit then walks the SAME table.
2. **[R1-3] A gate SWAP is invisible to every CI-visible test.** Both Route1 connections carry
   the SAME tag `"FromRoute1"`, so swapping SouthGate->Thornacre with NorthGate->Dawnmere
   changes not one byte any name-based needle searches for. The discriminating value is the
   target build index, emitted as a raw `u_int` immediately before the 32-byte tag buffer.
   **FIX:** needle the WHOLE serialized payload
   `[u_int version][u_int targetBuildIndex][32-byte zero-padded tag]` per gate, ==1 each.
3. **[R1-4] The slice has NO falsifiable proof.** `ZM_BattleTransition.cpp` already spells every
   `ls_aeBiome` row explicitly, so a value-equality unit passes bit-identically whether the row
   is authored or value-initialised -- it cannot see what it claims to.
   **FIX:** id-tag the biome rows (`{ZM_SCENE_ROUTE1, MEADOW}`) and assert
   `row.m_eScene == index`; plus drive the roll seam from a SYNTHETIC in-memory density map so
   one encounter proof does not skip on the gitignored bake.
   **DONE (ZM-22/ZM-D-204):** `ls_aeBiome` is now `ls_axBiome`, an id-tagged
   `{ZM_SCENE_ID, ZM_BATTLE_BIOME}` table, and `BiomeForScene` asserts the row's own tag matches
   its index; `Tests/ZM_Tests_RouteEncounterSeam.cpp` drives the density -> tile-transition ->
   grass-gate -> `RollStepForScene` composition against a hand-built 4x4 in-memory density map.
   **STILL OPEN:** this fixes the PROOF, not the CONTENT gap #3 exists to close -- see Decision 4
   in ZM-D-204 and the R1-4 row above.
4. **[R1-9] The three camera-clearance tests are HARD-SCOPED to Dawnmere** (`iCC_DAWNMERE_BUILD_INDEX = 2`,
   `LoadSceneByIndex` at three sites), so Route 1 rows would raycast Route 1 columns against
   the DAWNMERE physics world and mostly fall off it. The constants also live in an anonymous
   namespace inside `#ifdef ZENITH_INPUT_SIMULATOR`, so no boot unit can name them.
   **FIX:** extract the enum/spacings/budget into a simulator-independent pure header, then
   parameterise the tests per region (one new automated test per region).
5. **[R1-2] Byte needles are PREFIX-TRAPPED.** Component TYPE NAMES are serialized as strings,
   so `"Player"` is a substring of `"ZM_PlayerController"` and the shipped scenes author an
   entity literally named `"Player"` on an entity carrying that component -- `==1` is
   unsatisfiable. Same for `"Terrain"` vs `"ZM_TerrainGrass"`.
   **FIX:** give the new scenes' entities scene-unique names (`"Route1Player"`, ...) spelled in
   the placement headers; where a collision is unavoidable use the strictly-more/strictly-fewer
   form the committed-bytes file already documents.

### Other findings worth not rediscovering
- **The walk test must NOT traverse the spine.** Route 1's DirtLane is ~1408 m ≈ 6,000 frames
  one way; a round trip is ~12-14k against a suite max of 10,000. **Warp to each arrival marker
  and walk only the final ~20 m into each gate volume.**
- **The DirtLane is GRASS-FREE by construction** (a GRASS_ERASE phase zeroes density along every
  path; the nearest dab EDGE is ~30-130 m off-lane against a 0.5 density threshold). So
  lane-placed trainers essentially cannot lose their raise to a wild roll, and any encounter
  test must deliberately walk well off-lane onto unmeasured terrain.
- **`Npc_ExactlyOneRowNamesARegisteredTrainer` asserts `== 1u` exactly** and the second trainer
  reds it. It CANNOT just be bumped: that clause is the tripwire for the
  `ZM_TRAINER_RIVAL_VESPER == 0` value-initialisation trap. Rewrite it into a per-id claim check.
- **`ZM_Tests_StoryFlags.cpp` spells `uEXPECTED_WIRE_BIT_COUNT`** and **`Docs/SaveFormat.md`
  carries the flag registry** -- both must be edited by any slice appending a story flag.
- **No ground-truth oracle exists for Route1/Thornacre marker heights.** Dawnmere's are MEASURED
  raycasts for a reason; the Route1/Thornacre values are recipe target heights taken on faith.
  Add an oracle in R1-2 modelled on `ZM_DawnmereNpcGroundTruth_Test`.
- **Every live Route 1 test SKIPS on CI** (gitignored bake) and a skip counts as a PASS. The
  CI-visible spine of this whole item is boot units + committed-`.zscen` byte needles.
- **[R1-4, ZM-D-204] `ZM_TallGrassSystem` only ever reaches an entity via SCENE CONTENT.**
  Every existing case of it living on an entity (`ZM_AutoTests_TallGrass.cpp` and its siblings)
  runtime-attaches it BY HAND, in test code, explicitly so no scene change is needed. There is no
  production code path that attaches a gameplay component to a freshly-loaded scene's entities --
  `ZM_TerrainGrass` (its terrain sibling) reaches the entity the SAME way `ZM_TallGrassSystem`
  would have to: `Project_RegisterEditorAutomationSteps` (tools-only) + `AddStep_SaveScene`. So
  making Route 1 actually roll encounters in a real boot is a scene-authoring change to the
  ALREADY-COMMITTED `Route1.zscen` (a CHANGE, not a create), which is exactly the class of work
  ZM-D-031 / the headless publish guard keeps out of a worker's reach. It needs a windowed
  `Vulkan_*_True` re-author, by a human or a `needs-gpu` tick.

### ★★★ R1-2 IS IN FLIGHT. PHASE 1 IS DONE AND PUSHED; PHASE 2 IS PLANNED AND CRITIQUED, NOT STARTED.

**PHASE 1 COMPLETE (`0345327e`, ZM-D-198)** — per-recipe terrain materials, **proven byte-neutral**
by a windowed authoring boot (`Dawnmere.zscen` unchanged at `1DC1B639F8626725...`). It was landed
ALONE precisely so that proof was obtainable; the full slice moves three scenes on purpose and
could not have falsified its own assumption.

**PHASE 2 = the rest of R1-2:** the measured-ground split, the Route1 + Thornacre authoring
blocks, Dawnmere's `FromRoute1` marker, the ground-truth oracle, and the boot units.
**Zero code written.** A 3-lens adversarial critique returned **10 BLOCKERS** against the spec —
run it before implementing, do not re-derive it. Artefacts (session-temp, will NOT survive; the
load-bearing content is transcribed below):
`scratchpad/R1-2_SPEC.md`, `R1-2_RULINGS.md`, `R1-2_PHASE2_CRITIQUE.md`.

#### ★★ THE FOUR BLOCKERS THAT CHANGE THE PLAN
1. **THE SPEC'S AUTHORING CODE DOES NOT COMPILE — it was written BEFORE phase 1 landed.** It calls
   `ZM_FindTerrainRecipeIndex(...)` / `uZM_TERRAIN_RECIPE_INDEX_UNRESOLVED` /
   `g_aaxTerrainMaterials[idx][slot]`. Phase 1 shipped a **different** surface:
   `const MaterialHandle* ZM_GetTerrainMaterialsForRecipe(const ZM_TerrainAuthoringRecipe&)`
   (pointer-identity walk, asserts internally, returns the ROW). **Mirror the shipped Dawnmere call
   site verbatim** (`Zenithmon.cpp:3332-3337`) and do NOT add a second recipe-index mapping.
   Also `uZM_TERRAIN_MATERIAL_SLOT_COUNT` (=4) supersedes the spec's bare `iSlot < 4`.
2. **★ MEASURE DAWNMERE BEFORE TOUCHING ITS BYTES.** The spec sequences the `(512, 864)` ground
   measurement AFTER the `FromRoute1` marker is authored — so it would move a committed, twice-drifted
   file before knowing the column is authorable at all, and recovery would mean reverting a committed
   asset. **Land the seam oracle + its 1-row table FIRST, measure against the CURRENT committed
   `Dawnmere.zscen`, freeze, and only then add the marker.** If the row falls outside its band,
   report it for R1-3 and STOP — do not widen the band, do not add a Dawnmere pad (that regenerates
   the whole heightmap and invalidates all three measured tables AND the navmesh).
3. **The measured-ground tables forward-reference.** The spec puts them at the top of each placement
   header while their rows read XZ constants declared 180-290 lines lower; `constexpr` cannot
   forward-reference, so both headers fail to compile. They cannot simply move to the end either —
   accessors above them call in. Give each table's rows their own independent XZ literals, or
   restructure the header.
4. **The spec names the wrong pin sites in TWO places** (§1.F and §6 Phase 7): `Tools/run_unit_gate.ps1`
   is the **ENGINE** pin (1638) and moving it reds the engine gate; `Docs/BuildSystem.md` carries no
   ZM baseline. **Only `zm-tests.yml` + this file.** This is the THIRD spec in a row to get this wrong.

#### ★ POSITIVE FINDINGS — do not re-derive these
- **Dawnmere's `(512, 864)` IS on graded ground**, provable by reading: it sits ~4.6 m from the
  `Route` path polyline (`(512,928)->(500,760)`) against an **18 m flatten radius**. The `RouteGate`
  pad (r=30 at `512,896`) does NOT reach it — the path corridor is the whole licence. Expect ~25.6-26.5,
  like every other measured Dawnmere column. Ruling R6.2's worry is largely closed.
- **Every `AddStep_*` the plan calls EXISTS and its signature matches.** Verified against
  `Zenith/Editor/Zenith_EditorAutomation.h`.
- **The measured-ground split DOES keep both shipped R1-1 units green** — traced clause by clause.
  The two sites that MUST flip to measured or red by metres are
  `ZM_Tests_ThornacrePlacement.cpp:614` and `ZM_Tests_Route1Placement.cpp:454`.
- **The oracle's player-hit guard is sound**: `CCProbeGroundAt` takes an explicit ignore entity and
  sets `m_bHitTerrain` only when the hit IS the terrain, so a Player hit fails loudly instead of
  silently freezing a body-height ground plane.
- **Scope is honest**: registry 64 -> 67 verified by count (67 `ZENITH_AUTOMATED_TEST_REGISTER` lines
  today, 3 of which are prose).
- **Every `Zenithmon.cpp` line number in the spec is STALE** (written pre-phase-1). Insert by
  SEMANTIC landmark, never by line number. The Dawnmere marker steps go strictly between
  `AddStep_Custom(&ZM_ConfigureLabDoorTrigger)` and the rival pre-save guard — anywhere earlier
  rewrites every ZM-D-148 dense authoring-order index after it and turns a one-entity change into a
  whole-file diff.

#### ★★ STEP 1 IS DONE AND FROZEN. `Dawnmere.zscen` WAS NOT TOUCHED.
`ZM_DawnmereRouteSeamGroundTruth_Test` (registry 64 -> **65**) measured the `FromRoute1` column
`(512, 864)` against the CURRENT committed scene and it is **FROZEN at `24.36592f`**
(`hitTerrain=1`, `finalHit='DawnmereTerrain'`, `playerPresent=1` and correctly ignored).
`ZM_Interaction/RouteSeamGround_StandsOnTheFromRoute1LandmarkAndIsMeasured` pins it; boot
3345 -> **3346**.

> **★★ THE MEASUREMENT CONTRADICTED ITS OWN PREDICTION, AND THE CORRECTION MATTERS FOR THE REST
> OF THE ITEM.** The column was predicted at ~25.6-26.5, extrapolated from Dawnmere's other
> measured columns (town centre 25.99; Home/Lab 25.59-26.54), all of which sit ~+2 m ABOVE the
> recipe's 24.0 target. It measured **24.366** — target + 0.366. **It is the first measured
> Dawnmere column INSIDE A FLATTEN CORRIDOR** (4.56 m from the `Route` polyline against an 18 m
> flatten radius), and a FLATTEN dab drives ground TO the target; the other columns carry the
> hydraulic-erosion deposit pass on top. **So the ~+2 m gap is a property of UNFLATTENED ground.**
> The spec's §0.2 warned that Route1's and Thornacre's provisional constants would measure
> "metres, not ULPs" off — **every Route1/Thornacre arrival anchor sits on a flattened pad or
> lane, so expect those to come back NEAR their targets. Do not treat a near-target measurement
> there as a broken probe.**

#### ★★ STEP 2 IS DONE (`67f146c4`, ZM-D-199). WHAT REMAINS OF R1-2 IS **STEP 3**.
`Route1.zscen` and `Thornacre.zscen` are committed and two-boot proven; the measured-ground split
shipped with **eight OBSERVED literals**; registry 65 -> **67**; boot units unmoved at **3346**.

#### ★★ STEP 3 IS DONE (ZM-20, ZM-D-202). **R1-2 IS COMPLETE.**
Dawnmere re-authored windowed on a `Vulkan_vs2022_Debug_Win64_True` boot:
`sceneAuthoring=AUTHOR_DAWNMERE, warmMask=0x7, queued=0` — **not `DEFERRED`**, which
does nothing and looks successful. `1dc1b639…` -> **`f163f33b…`**, and a second
identical boot reproduced `f163f33b…` exactly, so the bytes come from compiled
constants. `git status`: exactly ONE tracked asset modified; the other six committed
scenes are byte-unchanged. Boot units **3384 -> 3387** (+3 needles), 0 failed —
and the Dawnmere needle was RED BY DESIGN until the re-author landed, so its going
green is what proves the marker and its inbound tag are in the bytes.

Next in the R1 chain is **R1-3** (ZM-21): the four seam triggers, in one commit.

<details>
<summary>The step 3 procedure, kept for R1-3 and for any later re-author</summary>

**STEP 3 — DONE. It was the risky one: it re-authors Dawnmere.**
1. Add the `FromRoute1` arrival marker to the Dawnmere authoring block, at the column already
   FROZEN by step 1 (`24.36592f`, `(512, 864)`). Its INBOUND tag is `"FromRoute1"` — resolve it
   from the world table (Route1's connections offer it), never spell it.
   ★ **INSERT BY SEMANTIC LANDMARK**: strictly between `AddStep_Custom(&ZM_ConfigureLabDoorTrigger)`
   and the rival pre-save guard. Scene files use DENSE authoring-order indices (ZM-D-148), so an
   insertion anywhere earlier renumbers everything after it and turns a one-entity change into a
   whole-file diff.
   ★ Still **ZERO TRIGGERS** — the marker only. Dawnmere's outbound gate is R1-3's.
2. Add the committed-bytes needles for all three scenes to `Tests/ZM_Tests_CommittedSceneBytes.cpp`
   (they could not land in step 2 — two of their target files did not exist yet). Required forms:
   `"Player"` and `"FromRoute1"` take the **STRICTLY-MORE** clause, never a bare equals-one
   (`"Player"` is a substring of `"ZM_PlayerController"`; `"FromRoute1"` prefixes any
   `FromRoute1Spawn`-style name). **Add `CountNameOccurrences("ZM_WarpTrigger") == 0`** on both new
   scenes — the existing zero-trigger clauses needle only the declared gate NAMES, so a trigger
   authored under any other name is invisible, and zero-triggers is this item's core safety ruling.
3. Re-author windowed; `git status` must show **exactly one** modified asset (`Dawnmere.zscen`).
   Second identical boot; hashes identical. Then the unit gate, then the batch.
> ★ If `Dawnmere.zscen` is not byte-stable, READ THE DIFF SHAPE BEFORE THEORISING and never just
> re-commit: rotation-only drift beside a bit-identical position is ZM-D-183 (the authored value
> moved); position+rotation together on a body-carrying entity is ZM-D-179 (the serializer wrote
> the live Jolt pose).
>
> ★ **THE CROSS-CONFIG LEG HAS QUIETLY LAPSED, AND R1-3 INHERITS THAT.** The
> 2026-08-05 baseline above used **Debug x2 + Release x1**; 2026-08-15 and ZM-20
> both used Debug x2 only. For ZM-20's entity that is defensible on structure
> rather than luck -- it carries no rotation, no scale and no collider, so it
> touches neither `glm::angleAxis` nor a Jolt body, and its three floats are
> compiled constants with no arithmetic between the table and the transform
> (ZM-D-183 is exactly a Debug/Release authoring divergence on THIS file, so the
> exemption has to be argued, not assumed). **An entity that carries a rotation,
> a scale or a collider does NOT get that exemption and owes the Release boot.**
> R1-3 authors trigger volumes; read this line before deciding two boots is enough.
>
> ★ **The authoring boot MUST carry `--skip-unit-tests`.** The needle for the marker you are
> adding is RED until the re-author lands, and a failing boot unit aborts the boot BEFORE scene
> authoring runs — so without the flag the unit blocks its own fix, forever. `zenith test` passes
> it for you; a bare exe does not. A bare windowed exe also never EXITS (`--exit-after-frames` is
> ignored without `--automated-test`), so drive the boot with a short automated test:
> `zenithmon.exe --automated-test ZM_Boot_Test --skip-unit-tests`.

</details>

#### ★ ORCHESTRATOR BOOT SEQUENCE FOR PHASE 2 (the critics' corrected ordering)
0. `Build\regen.ps1` (new files), build both configs.
1. ~~**Seam oracle + Dawnmere 1-row table only** -> measure -> freeze.~~ **DONE, see above.**
2. Land the split + both authoring blocks -> **windowed `Vulkan_*_True` boot with
   `--skip-unit-tests`** -> expect seven "Saved scene to" lines. **DO NOT COMMIT those bytes.**
   > **★ CHANGED BY STEP 1's MEASUREMENT: seed the provisional rows with each recipe's TARGET
   > HEIGHT, not the `-1000000.0f` sentinel.** The critics flagged that a sentinel row authors a
   > DYNAMIC player capsule a million metres under the world, and offered the recipe target only
   > as a fallback if a boot destabilised. Step 1 promotes it to the default: the seam column
   > measured target + 0.366 because it is flattened, and **every** Route1/Thornacre arrival
   > anchor sits on a flattened pad or lane, so the recipe target is a provisional value good to
   > well under a metre rather than an arbitrary one. The freeze still replaces it with the
   > OBSERVED literal — nothing is derived — but the provisional boot stays physically sane.
   > Record the deviation from the spec in the commit.
3. Run the oracles -> read the PASTE lines -> every one must report `hitTerrain=1` and
   `finalHit='<Region>Terrain'`, never `'Player'`.
4. Paste measured values, rebuild, re-run the oracles green.
5. Re-author windowed; `git status` must show exactly three asset paths.
6. Second identical boot; hashes identical. **Only now** run `run_unit_gate.ps1` — it boots with
   `--exit-after-unit-tests` and cannot carry `--skip-unit-tests`, so running it earlier tells you
   nothing while the needles point at files that do not exist.
7. `zenith test Zenithmon --headless`; confirm the registry against the LIVE PIN line at the
   top of this file, and read the count off the RUN (`N PASS` + `N MISSING` = registered),
   **never off `grep -c ZENITH_AUTOMATED_TEST_REGISTER`** — that oracle counts comments
   mentioning the macro and currently over-reports by 3 (see the LIVE PIN block). This step
   used to hardcode `67`, which was true for R1-2 and wrong the moment R1-3 landed.
> ★ Every boot in steps 2-4 MUST carry `--skip-unit-tests`: a failing boot unit aborts the boot
> BEFORE scene authoring runs, and this slice ships units that are RED BY DESIGN until the freeze
> closes. Without the flag the tests block their own fix forever.
> ★ In steps 2-3 the authored Player is a DYNAMIC capsule at the sentinel (~y -999,999). Watch those
> boots for physics complaints; if one destabilises, seed the tables with the recipe target for the
> provisional author ONLY and record the deviation — never silently clamp.

#### Smaller corrections to fold in
- **Add `CountNameOccurrences("ZM_WarpTrigger") == 0` to both new scenes' needles.** The proposed
  zero-trigger clauses needle only the two declared gate NAMES, so a trigger authored under any other
  name is invisible — and zero-triggers is this slice's core safety ruling.
- U3's stated catch is misattributed: it tests the accessors, not the wiring. The pair that actually
  catches a mis-wired marker is U8's `"FromDawnmere" == 1` AND `"FromRoute1" == 0` on the committed bytes.
- Clause (4) of `Route1_GateVolumesAdmitNoStepOverAndSitOnTheGround` is arithmetically forced
  (`Min().y == ground` identically). Keep it, but say so in the comment — do not present the re-point
  as a strengthening.
- `RGSceneIsActive` compares an `int` build index against a `u_int`; cast explicitly or MSVC warns
  under `/WX`. U11 needs `<string>` added to `ZM_Tests_CommittedSceneBytes.cpp`.
- **Accepted cost, stated not hidden:** Route1 and Thornacre are the first committed Zenithmon
  scenes that **cannot be load-verified in CI** (their tests skip on the gitignored bake). The
  windowed boots are the only proof they load — confirm during them that each scene loads and the
  follow camera resolves `"Player"`, rather than only counting saves.
- `ZM_DawnmerePlacement.h:737-748` still calls the Lab ground table an "INVALID PLACEHOLDER ... RED
  until they do" while the `.cpp` is frozen with ten measured literals; `ZM_ThornacrePlacement.h:239`
  still says "re-measure in R1-3" when R1-2 does it. Fix both opportunistically.

### ★ THE ONE FOLLOW-UP R1-1 LEFT OPEN FOR R1-2 (do not rediscover it)
- **`fZM_ROUTE1_PROVISIONAL_GROUND_Y` and its Thornacre twin are UNMEASURED** -- they are the
  recipes' `m_fTargetHeight`, not surfaces anyone probed. They are only legitimate while every
  anchor sits inside a flattened pad, which the R1-1 units assert. **R1-2 must land the raycast
  ground oracle (model it on `ZM_DawnmereNpcGroundTruth_Test`) and re-freeze the anchors as
  MEASURED literals.** Move an anchor off its pad before that lands and the constant becomes a
  lie no compiled-constant unit can see.

> The other two follow-ups are **CLOSED** by R1-1's review pass (same day): Thornacre gained the
> camera unit its header comments already claimed
> (`Thornacre_SettledCameraStandsAboveGroundBehindTheArrival`, with a sign-flipped-pitch
> anti-vacuity arm), and both suites' duplicated component-type tables now pin their counts
> (15 game / 17 engine) as literals, so a one-sided registration reds the other file.
>
> **The durable fix has since landed, via ZM-57: `Tests/ZM_Tests_ComponentTypeNames.h`.** It
> holds the one copy of each table; both suites include it and alias the arrays under their own
> existing local names, so the loops that walk them are unchanged. The pinned-literal
> assertions in both files stay -- they now catch drift between the shared header and the REAL
> registries (`Zenith_ComponentMeta_Registration.cpp`, `Zenithmon.cpp`) rather than drift
> between two independent copies. It landed before a third name battery existed, as a
> standalone debt payoff rather than the "next slice that needs one" trigger this note
> originally expected.
>
> The same pass also killed a clause that could not red: U8 compared
> `fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET` against a term-for-term re-computation of its own
> definition, so both sides moved together on any edit. It is now an independent
> two-capsule-radii floor. **That species of defect has now been found three times in this
> codebase (ZM-D-183, the self-referential-guard note, and here) -- check for it by reflex in
> every review.** The rambler's 0.129 m margin against that floor is recorded in place, so a
> body-contract re-tune reds as a known-tight anchor rather than a mystery.

# ════════════════════════════════════════════════════════════════════════════
# END COLD-START BLOCK. Historical context for earlier stages follows.
# ════════════════════════════════════════════════════════════════════════════

---

## Notes for next agent

* **The next task is R1-2 step 3** (`ZM-20`) — the only part of R1-2 still owed, and
  the risky one: it re-authors Dawnmere. The cold-start block above is its brief.
* **It is `windowed`.** A headless run may CREATE a `.zscen` but never CHANGE one, so
  the loop cannot take it and will not try — the label is filtered out of the claim
  query in SQL. Run it yourself, windowed, `Vulkan_*_True`, `--skip-unit-tests`.
* **Everything after it is BLOCKED behind it**, mechanically:
  `ZM-20 → ZM-21 → ZM-22 → ZM-23 → ZM-24 → ZM-25 → ZM-26 → ZM-28 → ZM-29 → ZM-30`,
  and `ZM-30` (the S8 go/no-go) blocks all five S9 stories. `zagent blocked --project ZM`
  prints the graph; the loop refuses any ticket whose predecessor is unfinished.
* **A dirty tree blocks the loop entirely.** Its precondition check treats uncommitted
  changes as fatal rather than something to work around — it has to, since Zenithmon
  is `branching: "direct"` and commits in place. Leave `master` clean.
