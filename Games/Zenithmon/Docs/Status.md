# Zenithmon Status

**Last updated:** 2026-07-21
**Stage:** **S7 (save/load, story flags, trainer battles) ACTIVE. Item 1 (full schema-v1 codec) COMPLETE (SC1-SC2, ZM-D-135/136). Item 2 (story flags + save integration) IN PROGRESS -- SC1 of 6 DONE (ZM-D-137); NEXT is SC2, the typed slot/disk layer over the frozen codec.** S0-S6 remain complete. S7 requires no human intervention; the next human stop remains the S8 vertical-slice go/no-go.
**Build:** GREEN on the S7 item 2 SC1 gate -- `Build\regen.ps1` GREEN (two new `.cpp` files, no new directory) then `zenith build Zenithmon` (`Vulkan_vs2022_Debug_Win64_True`) GREEN. SC1 touched no engine file, so the five-configuration serial matrix and the cross-game regression sweep were not owed (they were last run in full at the item 1 SC2 boundary and were green there). The local gate is the direct-`master` landing authority; CI is the asynchronous post-push backstop, and no CI result is claimed here.
**Tests:** boot unit gate **2425 ran / 2424 passed / 0 failed / 1 skipped** (the 1 skip is the pre-existing unrelated `GraphComponent::RegistryWideNodeRoundTrip` quarantine), **+33** over the previous 2392; `zm-tests.yml` bumped **2392 -> 2425** from the OBSERVED boot line. The separate engine-only reference remains **1103** and `Tools/run_unit_gate.ps1`'s default is untouched. The registered automated-test count is unchanged at **36**: headless **36/0**, then full windowed **36/0/0** with zero skips and no zero-frame tests. S7 has no visual or human gate.

## Current task

**S7 item 2 SC2 -- the typed slot/disk layer over the frozen schema-v1 codec -- is NEXT.** Reuse `ZM_SaveSchema` exactly as ZM-D-136 froze it: do not redesign the codec, do not fold slot or ECS concerns back into it, and do not invent a historical v0 migration. ECS serialization order 113 remains the last occupied game order, so the next free order is **114**. Continue autonomously through S7; there is no human intervention point until the **S8 vertical-slice go/no-go**.

**S7 ITEM 2 SUB-COMMIT PLAN (6 total):**
- **SC1 DONE (ZM-D-137)** -- `ZM_StoryFlags` identity registry + flag-gated NPC lines + the `Npc_Warden` row. See "Last completed".
- **SC2 NEXT** -- a typed slot/disk layer over the frozen codec (Save0-2 + Auto).
- **SC3** -- world-position capture + resume placement + quit-to-FrontEnd + the autosave latch.
- **SC4** -- the save-slot screen and root-menu Save/Quit.
- **SC5** -- the title menu and Continue.
- **SC6** -- the S7 stage-gate windowed test (save -> quit-to-FrontEnd -> continue restores position/party/flags exactly) plus an autosave milestone test.

**S7 ITEM 1 STATUS (complete):**
- **SC1 DONE (ZM-D-135)** -- the durable in-memory `ZM_GameState` model freeze + 18 `ZM_Save` units. Its LAYOUT IS FROZEN: add named free functions beside it, never new members, unless a version bump is being paid for.
- **SC2 DONE (ZM-D-136)** -- the pure transactional 11-module schema-v1 codec, 29 schema + 2 literal-golden units, and the exact **824-byte** v1 artifact. Every incompatible change from here owes a version bump + a literal historical-blob migration test IN THE SAME COMMIT.

**Architecture (fixed, do not re-litigate):** exactly **ONE ECS order is consumed for interaction -- 113 (`ZM_Interactable`)**; the NPC walker is a by-value member of it and `ZM_InteractionRuntime` is a by-value member of `ZM_PlayerController` (already on every Player in every scene, so there is no per-scene `AddStep_AddComponent` to forget). **Next free ECS order: 114.** Screens are by-value non-ECS presenters on `ZM_UI_MenuStack` (order 112), so a new screen is one arm per dispatch switch and costs no ECS order. Interaction is a forward CONE, never a raycast -- not for a headless reason (`HasActiveSimulation()` is merely `m_pxPhysicsSystem != nullptr` and `Physics().Initialise()` is unconditional, so **physics IS live headless**; ZM-D-127 corrected the old false claim) but because the cone stays pure and unit-testable; S7's trainer occlusion ray enters as a probe filter in the GLUE layer, leaving the pure picker untouched. Five authored Dawnmere NPCs only (villager / Trade Post clerk / Caretaker / wanderer / **warden**) -- populated towns are S9/S10. **"Trade Post"**, never "Mart", in data/entity/asset names. NO RNG in the walker (TestPlan C8).

**S6 CLOSURE RULING (ZM-D-134, still binding):** behaviour graphs and navmesh-driven wandering were deliberately deferred to S7. `ZM_GraphAuthoring` is not written; S6 ships a bounded 3-arm C++ role dispatch behind one `Interact()` seam. `Zenith_NavMeshGenerator::GenerateFromGeometry` is terrain-capable when supplied suitable triangles, but `Zenith_AINavGeometry::GenerateFromScene` does **not** harvest streamed terrain geometry or a heightfield; it represents static colliders as box geometry. S7 item 3 owns the first useful graph integration plus the terrain-triangle/grid-coverage and `.znavmesh` evaluation. `MasterPlan.md` is historical/read-only, not the source of current shipped truth.

**PER-SC GATE -- run in this exact order, every time:** `Build\regen.ps1` (ONLY when a new .cpp or folder was added) -> `zenith build Zenithmon` -> `zenith test Zenithmon --headless` (heals DLLs) -> `Tools\run_unit_gate.ps1 -Exe ... -Baseline <N> -TimeoutSec 300` (the 300 s timeout-kill is EXPECTED) -> full windowed `zenith test Zenithmon`. **Two standing tripwires:** (a) never write a PREDICTED unit count into `zm-tests.yml` -- only the OBSERVED one from the boot log; (b) the engine baseline **1103 must remain unchanged** unless an explicitly-scoped engine change owns the cross-game gate.

## Last completed

**S7 item 2 SC1 -- STORY-FLAG IDENTITY REGISTRY + FLAG-GATED NPC LINES (ZM-D-137).**
New `Source/Data/ZM_StoryFlags.{h,cpp}` gives the ZM-D-135 bitset an identity:
a SAVE-STABLE `enum ZM_STORY_FLAG_ID : u_int` whose **value IS the persisted bit
index in save-schema module 4**. Six flags are allocated densely from zero
(`INTRO_LEFT_HOME` 0, `MET_PROFESSOR` 1, `STARTER_RECEIVED` 2, `WARDEN_CLEARED`
3, `ROUTE1_OPEN` 4, `GYM1_DEFEATED` 5), then `ZM_STORY_FLAG_COUNT` and
`ZM_STORY_FLAG_NONE`, which aliases COUNT and is NEVER persisted. Allocation is
APPEND ONLY -- reordering or reusing a retired value is a versioned codec change.
**Density is a storage contract, not tidiness:** module 4 writes a u16 count of
highest-set-index+1 then ceil(count/8) bytes, so ONE sparse index would add those
bytes to EVERY save forever and could not be reclaimed without a version bump.
The compiled `const ZM_StoryFlagInfo s_axFlags[]` table has a **DEDUCED bound**
plus a row-count `static_assert`, so a missing or extra row is a COMPILE error
rather than a silently zero-initialised row. Accessors are FREE FUNCTIONS
(`ZM_SetStoryFlag` / `ZM_IsStoryFlagSet`, overloaded on `ZM_GameState` and on
`ZM_StoryFlagSet`) precisely because `ZM_GameState` is frozen: naming a flag
costs zero wire change. `ZM_StoryGate { m_eFlag = NONE; m_bRequireSet = true; }`
plus a TOTAL, FAIL-CLOSED `ZM_StoryGatePasses` means a default-constructed gate
is unconditional, so a data row that GAINS the field keeps its old behaviour by
construction. `ZM_IsMilestoneStoryFlag` (WARDEN_CLEARED / ROUTE1_OPEN /
GYM1_DEFEATED) is authored beside the registry so the milestone list cannot drift
-- nothing consumes it yet; it is reserved for SC3's autosave hook.

**The first gameplay consumer.** Before SC1 there was **not one gameplay consumer
of the story-flag bitset anywhere in the tree** -- an index was a raw literal
inside a test. `ZM_NpcData` gains three fields **APPENDED AT THE END**
(`m_xLineGate`, `m_paszGatedLines`, `m_uGatedLineCount`) -- last on purpose,
because every row is a POSITIONAL aggregate initializer and a mid-struct
insertion shifts each trailing value one column left, the first casualty being
`m_bWanders` (the Wanderer would stop patrolling with no compile error). A pure
`ZM_SelectNpcLines` picks the set, and `ZM_Interactable`'s
`ZM_NPC_RAISE_DIALOGUE` arm reads the LIVE flags via
`ZM_GameStateManager::TryGetGameState` and routes through it (a manager-less
context is treated as an all-clear set, so require-SET gates fail closed and
require-CLEAR gates pass -- exactly what a fresh save answers). **`ZM_RaiseKindForRole`
and `ZM_NPC_RAISE_KIND` are UNCHANGED: gating selects CONTENT, it never re-routes
which seam a role talks through.** A fifth Dawnmere NPC, `ZM_NPC_ROUTE_WARDEN` /
entity `Npc_Warden`, is the first gated row.

**★★ DEFECT 1 -- A FATAL ASSERT ON A UNIT-PINNED INPUT DESTROYED THE ENTIRE BOOT
GATE.** `ZM_StoryGatePasses` first did `Zenith_Assert(false, ...)` on an
unregistered id; the unit `Gate_OutOfRangeFailsClosed` deliberately passes id 13
to pin fail-closed behaviour. **`Zenith_Assert` is NOT compiled out anywhere** --
`Zenith/Core/Zenith.h:138` defines `ZENITH_ASSERT` unconditionally immediately
ABOVE its own `#ifdef ZENITH_ASSERT`, so the real definition at `:140` always
wins and calls `Zenith_DebugBreak()` in every configuration. The whole
`ZENITH_TEST` suite runs at BOOT before the scene loads, so the process died and
**no "Unit tests complete" line was printed at all** -- the loss was the whole
2425-unit gate, not one red unit. RULE: totality and a defensive assert are
mutually exclusive; a total function pinned by a unit must RETURN its defined
answer, diagnosing mis-authored data with a non-fatal
`Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)` and logging nothing for a legitimate
sentinel. (There is no `LOG_CATEGORY_GAME`.)

**★★ DEFECT 2 -- THE NEW NPC SILENTLY DISARMED AN EXISTING TEST.** Adding the
Warden put a SECOND `ZM_NPC_ROLE_TALKER` into the fixture behind
`GateRoster_PlacedNpcsCoverEveryRole`, so that unit's own advertised mutation
(re-roll `ZM_NPC_VILLAGER` to SHOPKEEP) stopped redding it -- the Warden covered
TALKER while the interaction gate's talk beat would have started raising a shop.
Fixed by splitting the three BEAT NPCs (villager = talk, clerk = buy,
caretaker = heal, each with its expected raise kind SPELLED IN THE TEST rather
than read back off the row) from the merely-PLACED roster, and asserting role
coverage over the beat table. RULE: adding a data row can weaken an existing
test's teeth without touching that test.

**★★ DEFECT 3 -- THE GATED BRANCH WAS PINNED BY NOTHING.** `ZM_SelectNpcLines`
returns the ordinary lines verbatim for any row with a null gated array, and
every pre-existing row is ungated, so reverting the dispatch arm to the old
one-liner left all 33 new units AND the full 36-test windowed suite GREEN.
Closed by two new phases on the HEADLESS (therefore CI-visible)
`ZM_NpcDispatch_Test` that drive a real interact edge at the warden row with the
flag clear then set, asserting the queued line COUNT **and the FIRST LINE TEXT**
-- the text clause is load-bearing because both line sets have 2 entries, so a
count-only assertion would not catch a selector that always returns the ordinary
set. **Mutation-verified: with the arm reverted the test goes RED (exit=1) and is
green again with the source restored.** The test restores the flag it mutates on
every exit path, including a mid-phase failure that leaves it set.

**Also corrected pre-commit (the recurring false-claim-in-an-argumentative-passage
defect class):** SEVEN comment sites justified the selector's null guard by
claiming `ZM_UI_DialogueBox::QueueLines` would CRASH on a `(null, non-zero)`
pair. It does not -- `ZM_UI_DialogueBox.cpp:68` rejects a null array as the first
disjunct of its first guard, before any dereference; the real consequence is a
refused push and a completely MUTE NPC, the same outcome as the over-cap case.
Both sanitisers were KEPT; only the rationale was wrong. The sparse-index
rationale in the story-flag units was premised on a fixed-bound table the code no
longer uses. And the warden's placement comment claimed he stood "on the north
road out of town" when he is ~37 m from the Route polyline and ~1 m from the
authored Home walkway centreline -- his fiction and lines were rewritten to match
where he actually stands; **his position was NOT moved** (his coordinates are
separately derived against every traversal constraint and the windowed suite
passed green at them). A note records that when a real route is authored, a
road-blocking warden should be re-placed onto the route polyline with every
separation re-derived from scratch.

**Evidence.** +33 units: 18 `ZM_Story` in the new `Tests/ZM_Tests_StoryFlags.cpp`,
13 `ZM_Data` over the gate columns and selector, net +2 in
`Tests/ZM_Tests_Interactable.cpp`. Registered automated tests unchanged at **36**
(the new coverage is two extra phases inside an existing headless test). Regen
GREEN; `zenith build Zenithmon` GREEN; boot **2425 / 2424 / 0 / 1**;
`zm-tests.yml` bumped **2392 -> 2425** from the OBSERVED line; engine reference
**1103 UNCHANGED**; headless **36/0**; full windowed **36/0/0**, zero skips, no
zero-frame tests. Pre-existing S6 windowed tests held their historical frame
counts EXACTLY -- `ZM_PlayerHomeRoundTrip` **831**, `ZM_NpcWander` **830**,
`ZM_S6InteractGate` **749**, `ZM_NpcHeal` **315**, `ZM_NpcShop` **286** -- so the
fifth authored NPC perturbed neither the nearest-wins interaction picker nor any
traversal route. **Contracts held:** `ZM_SaveSchema` untouched (the 824-byte v1
golden is unchanged), `ZM_GameState` layout untouched, no new ECS order (next
free still **114**), no `uSERIALIZATION_VERSION` bump, no engine file touched
(so baseline 1103 stands and no cross-game regression was owed). No commit, push
or CI result is claimed.

---

Prior: **S7 item 1 SC2 -- SCHEMA-V1 CODEC FREEZE (ZM-D-136).**
`ZM_SaveSchema::{Write,Read}` is the pure inner-payload boundary over
`ZM_GameState`: 11 ordered independently length-framed modules, explicit
little-endian fixed widths, schema/module version 1, a 61-byte monster record,
append-transactional writes and exact-length transactional reads. Dex accepts
current/older roster counts and rejects newer counts with `VERSION_MISMATCH`;
StoryFlags writes its high-water count; Options is a counted uint16 TLV list that
skips bounded unknown tags but requires exactly one known text-speed tag. The
codec owns no slots, disk I/O, ECS or runtime scene behavior. 29 schema units + 2
literal-golden compatibility units took the gate to **2392 / 2391 / 0 / 1**;
engine **1103**. The independent complete v1 golden is exactly **824 bytes** and
represents v1, not a fabricated v0 migration. Regen, all five Zenithmon builds,
headless **36/0** and full windowed **36/0/0** were green, plus a complete
cross-game sweep (three Sentinels; Combat **1103/1102/0/1** + 14/0; DP
**1104/1103/0/1** + 158/0; CityBuilder **1104/1103/0/1** + 45/0; RenderTest
canaries boot **1 frame** / terrain **151 frames**; scaffold smoke 11/0). Because
the codec must distinguish a growable owned stream from a fixed wrapped buffer,
engine `Zenith_DataStream` gained the read-only `OwnsData()` query plus
ownership-transfer units -- that is what moved engine 1097 -> 1103.

Prior: **S7 item 1 SC1 -- DURABLE-MODEL FREEZE (ZM-D-135).** `ZM_GameState` owns
the complete module inventory: party plus deterministic transactional 16x30
boxes, seen/caught dex, 4096 story bits, 8 badges, bag/full-width money, daycare,
tower current/best/seed, unset world position and NORMAL-default options.
`ZM_Monster` adds zero-default friendship and a zeroed 16-byte nickname; caught
battle records normalize `ABILITY_NONE` to the species regular ability. Catch
placement is party-first then first-free box while dex marking remains invariant.
18 new `ZM_Save` units (**2361 / 2360 / 0 / 1**); engine **1103**. **The layout is
frozen: reach it with free functions, never new members.**

Prior: **S6 item 3 SC9 -- FULL STAGE CLOSURE (ZM-D-134); S6 COMPLETE.** Fresh
local evidence passed the five-build serial matrix, boot **2343 / 2342 / 0 / 1**,
headless **36/0** and full windowed **36/0/0**. The six exact S6 windowed filters
passed non-skipped: UI **158**, Talk **85**, Shop **286**, Heal **315**, Interact
**749**, Wander **830**. S6 has no visual gate, so it closed without human
intervention. It also settled the two bounded deferrals (graphs + navmesh to S7)
and ruled `MasterPlan.md` historical/read-only.

Prior: **S6 item 3 SC8 -- THE AUTHORED WANDERER (ZM-D-133).** `ZM_NpcWalkerLogic`
is a deterministic pure two-waypoint walker: fixed authored points, arrival
dwell, explicit halt, no RNG, XZ-only steering, patrol velocity that preserves
the body's existing Y velocity. `ZM_Interactable` serialization went **v2** for
authored patrol configuration with v1 data as a stationary fail-closed fallback;
runtime cursor/dwell state deliberately restarts from point zero on load. The
runtime contract is a dynamic capsule driven through body linear velocity;
opening **its own** dialogue halts the patrol and closing it resumes.
`Npc_Wanderer` sits at **x=540, z=476..484**. **★ Its first placement intersected
the ONE-SIDED terrain mesh**, letting the dynamic capsule penetrate from the
non-colliding side and fall instead of patrol -- fixed with real clearance above
sampled terrain, never by pinning Y or teleporting. 18 pure units + the 830-frame
windowed halt/resume proof.

Prior: **RENDERTEST CANARY RESTORED + THE ECS DUPLICATE-ORDER ENGINE GAP CLOSED
(ZM-D-132).** RenderTest's terrain is `_True`-baked (12,313 files / 1.78 GB from
seed 1337), so `TerrainEditorSmoke` -- the terrain/grass canary the engine-change
gate names -- PASSES windowed and was used as a real canary.
**★ With terrain fully baked, HEADLESS still asserts `Invalid buffer VRAM handle`
at `Zenith_TerrainComponent::InitializeCullingResources()`** -- the terrain path
sets up GPU-driven culling with no headless guard. That is an open ENGINE gap
(Q-2026-07-21-001), not an asset problem, and it means terrain has no CI coverage
on a GPU-less runner. The engine fix: `Zenith_ComponentMetaRegistry::Finalize()`
now detects duplicate serialization orders (tie-break the sort on type name,
`Zenith_Error` per colliding pair, gate on a `Zenith_Check`), +6 units hosted
ENGINE-SIDE because the ECS leaf may not include the test framework. **★ Lesson:
an engine baseline is pinned in a script DEFAULT (`Tools/run_unit_gate.ps1`,
consumed argument-less by `engine-gate.yml` and `test_scaffold.ps1`) as well as
in per-game workflow args.**

Prior: **KNOWN-BUG SWEEP (ZM-D-131) -- four verified defects fixed, GAME-ONLY.**
(1)+(2) the traversal drive picked keys in the WRONG FRAME in both remaining
copies; both now project onto the LIVE camera basis (`ZM_PlayerHomeRoundTrip`
moved 673 -> **831** frames, deterministic across three runs at 54% headroom).
(3) `ZM_NpcDispatch_Test` -- the ONLY CI-visible interaction test -- now asserts
WHICH screen each role raises, mutation-proven. (4) the battle menu offered Catch
unconditionally, ignoring `m_bCanCatch`; now gated on `IsCatchAllowed()` and
pinned by a real `UpdateMenu` key-edge unit after the review caught the fix was
UNPINNED.

Prior (S6 item 3, condensed -- full detail in DecisionLog ZM-D-124..130):
**SC7 (ZM-D-130)** `ZM_S6InteractGate_Test` passes in **749** frames in one
uninterrupted session (talk + buy + heal + open-every-menu, each reached by a
real `SimulateKeyPress(ZENITH_KEY_E)`); its root-cause finding -- **movement is
CAMERA-RELATIVE, so world-space key choice is correct only for a single leg from
rest** -- is the standing rule below. Its review also produced the mutation test
that FAILED to bite: a single confirm on a running typewriter only COMPLETES the
reveal and never advances a line, so the negative now emits six spaced interact
edges. **SC6 (ZM-D-129)** `ZM_NpcShop_Test` (286) + `ZM_NpcHeal_Test` (315) in
one new TU carrying the shared `WalkContext`/`TickWalk` machine at fixed **1/60**;
the shop test asserts the screen carries the clerk's OWN runtime-read
`ZM_NpcData` stock. **SC5 (ZM-D-128)** three NPCs authored into the real
Dawnmere block + `ZM_NpcTalk_Test` (85 frames); its blocker -- an NPC placed dead
centre on the `ZM_PlayerHomeRoundTrip` corridor stopped the capsule 108 m short,
because `DriveTowardXZ` has NO obstacle avoidance -- is the standing
check-traversal-routes rule. **SC4 (ZM-D-127)** `ZM_Interactable` (order 113) +
`ZM_InteractionRuntime` + the headless `ZM_NpcDispatch_Test`; corrected the false
"physics is dead headless" premise by mutation. **SC3 (ZM-D-126)** the
`ZM_NpcData` content table with both row caps pinned to their UI limits at
COMPILE time (both UI guards are ALL-OR-NOTHING -- an oversized list is REJECTED,
not truncated, so a drifted cap makes an NPC silently mute or its shop refuse to
open). **SC2 (ZM-D-125)** the pure candidate picker: XZ-only range with a
separate absolute height band, inclusive distance/band/cone, most-specific-last
reject reporting via a high-water mark, nearest wins with ties to the lowest
index. **SC1 (ZM-D-124)** `ZENITH_KEY_E` + pure `ZM_ShouldInteract` returning a
REASON (append-only enum, fixed unit-pinned blocker precedence) +
`ZM_InputActions.h` as the single source of every binding, walked by both the
live readers and the collision units.

Prior (S6 item 2, condensed): SC9 the consolidated `ZM_S6UIGate_Test`
(ZM-D-122, **item 2 COMPLETE**); SC8 Care Center heal as a dialogue yes/no CHOICE
(ZM-D-121); SC7 shop `ZM_ShopLogic` + `ZM_UI_Shop` (ZM-D-120); SC6 `ZM_UI_Bag`
(ZM-D-119); SC5 `ZM_UI_Dex` (ZM-D-118); SC4 `ZM_UI_Party` + generalized screen
dispatch (ZM-D-117); SC3 `ZM_Bag` + money (ZM-D-116); SC2 `ZM_UI_DialogueBox`
(ZM-D-115); SC1 `ZM_UI_MenuStack` + an engine ECS fix (ZM-D-114); S6 item 1 E4
`Zenith_UIGridLayoutGroup` (ZM-D-113); S5 STAGE GATE SIGNED OFF (ZM-D-112).

## Notes for next agent (S7)

- **★ NEW -- A TOTAL FUNCTION MUST NEVER `Zenith_Assert` ON ITS ARGUMENTS.**
  `Zenith/Core/Zenith.h:138` defines `ZENITH_ASSERT` unconditionally immediately
  ABOVE its own `#ifdef ZENITH_ASSERT`, so the definition at `:140` always wins
  and `Zenith_DebugBreak()` fires in EVERY configuration. Units run at BOOT
  before the scene loads, so an assert on an input a unit deliberately supplies
  kills the process partway through and **the whole boot gate is lost -- no
  "Unit tests complete" line prints at all**, which reads as a build/harness
  failure rather than one red unit. Diagnose mis-authored data with a non-fatal
  `Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)` -- there is no `LOG_CATEGORY_GAME`
  -- and return the defined fail-closed answer.
- **★ NEW -- ADDING A DATA ROW CAN DISARM AN EXISTING TEST WITHOUT TOUCHING IT.**
  A fifth NPC row gave a roster unit a second TALKER, so that unit's own
  advertised mutation stopped redding it. Whenever you append to a compiled data
  table, re-read every unit that WALKS that table and ask what its stated
  mutation still proves. Fixture rows a test depends on must be a named,
  explicitly-spelled subset (the "beat" table), not "whatever the table happens
  to contain".
- **★ NEW -- A NEW BRANCH WITH A BENIGN FALLBACK IS PINNED BY NOTHING.** Because
  the line selector returns the ordinary lines for every ungated row, reverting
  the whole dispatch arm left 33 new units and the full windowed suite green.
  When a feature's default path is indistinguishable from the old behaviour, the
  ONLY proof is a fixture that takes the new path, and its assertion must
  distinguish the two outputs by CONTENT (both warden line sets have 2 entries --
  a count-only assertion proves nothing). Put it in a HEADLESS test so CI sees it.
- **★ SCHEMA V1 IS FROZEN (ZM-D-136) AND SO IS `ZM_GameState`'s LAYOUT
  (ZM-D-135).** Preserve the exact 11-module order, fixed widths, 61-byte monster
  encoding, statuses and transactional cursor/destination behavior in
  `SaveFormat.md`; every incompatible change owes a real version bump + a literal
  historical-blob migration test in the same commit. There is no v0. Reach the
  frozen model with named FREE FUNCTIONS (the `ZM_StoryFlags` pattern), never new
  members. Do not fold slot or ECS concerns back into the pure codec.
- **★ STORY-FLAG INDICES ARE WIRE FORMAT.** `Source/Data/ZM_StoryFlags.h` is the
  authoritative index registry: append only, dense from zero, never renumber or
  reuse. Module 4 sizes itself from the highest SET index, so one sparse index
  costs ceil bytes in EVERY save forever and cannot be reclaimed without a
  version bump. Reserve a flag by adding a row, never by leaving a gap. Renaming
  a debug name is free.
- **★ `zenith test <Game>` runs the EXISTING exe -- it does NOT relink after an
  ENGINE-lib change.** For cross-game engine regression you MUST
  `zenith build <Game>` FIRST, THEN test, or you validate a STALE exe. If scoped
  work is expected to be game-only and this arises, verify whether an engine file
  was touched.
- **★★ THE TRAVERSAL DRIVE MUST BE CAMERA-RELATIVE.** Player movement is
  camera-relative and `ZM_FollowCamera` re-aims from a LAGGING camera-to-player
  vector, so the world-space meaning of W/A/S/D rotates as the player turns.
  Picking walk keys from raw world dx/dz is correct ONLY for a single leg from
  rest. Use `Tests/ZM_AutoTests_NpcServices.cpp` as the canonical source for any
  new walk (ZM-D-130/131).
- **★ REUSE the shared walk machine.** `Tests/ZM_AutoTests_NpcServices.cpp` owns
  ONE parameterised `WalkContext` / `TickWalk` machine. There is NO shared test
  header (`Tests/` is `.cpp` only) and every sibling helper sits in a per-file
  anonymous namespace, so a NEW TU cannot reach any of it -- which is exactly why
  another hand-rolled copy of `DriveTowardXZ` must not be written.
- **★ CHECK EXISTING TRAVERSAL ROUTES BEFORE PLACING ANYTHING SOLID.**
  `DriveTowardXZ` has no obstacle avoidance, so a solid AABB on a corridor an
  existing windowed test walks blind will kill that test at its frame cap. SC1's
  warden derives every separation explicitly (18 m off the z=480 Home corridor,
  34 m off the x=512 spawn-to-villager corridor, 20.0 m to the nearest NPC) --
  copy that derivation style, and re-derive from scratch if anything moves.
- **★ MUTATION-TEST any test you claim has teeth** -- break the thing it should
  catch, rebuild, confirm RED (exit=1), restore, rebuild, re-gate. This has paid
  off in four consecutive sub-commits; twice it revealed the test proved nothing.
- **★ The recurring review win is finding tests that CANNOT FAIL** -- loops
  bounded by a count just asserted zero; "unchanged" asserts on state never
  populated; a totality test whose expectation calls the function under test.
  Check for vacuity EXPLICITLY on every SC, and for each unit name the source
  change that would make it fail.
- **★ THE RECURRING DEFECT CLASS: confidently-worded FALSE claims inside
  argumentative comment passages.** SC1 found seven copies of one
  (`QueueLines` "would crash" on a null array -- it rejects it at
  `ZM_UI_DialogueBox.cpp:68` and the real consequence is a MUTE NPC). Verify
  every claim a comment makes about another function's behaviour by reading that
  function, not by reasoning about it. A wrong rationale can survive alongside a
  correct guard for a long time.
- **★ UI element ownership:** `AddElement` pushes into BOTH `m_xAllElements` and
  `m_xRootElements`; `Clear()` deletes only `m_xAllElements`, so a merely-
  `AddChild`'d element **LEAKS** and an `AddElement`+`AddChild` element is walked
  TWICE. `Zenith_UICanvas::ReparentElement` is the only correct path. And
  `SetVisible` notifies the parent (a grid re-runs layout), so write child
  visibility **ONLY ON CHANGE** -- in Hide as well as Present.
- **★ ENGINE UI NAV RULE:** never wire bake-time `SetNavigation` links into a
  pool whose members are shown/hidden at RUNTIME. `NavigateDown` consults the
  explicit link FIRST and falls back to the spatial search ONLY when it is null,
  so a link into a hidden element silently swallows the press. `SetNavigation` is
  also not serialized, so bake-time links exist only in tools builds.
- **★ Windowed-gate rule:** a test that PARKS the canvas focus programmatically
  proves NOTHING about navigation. Drive real arrow-key edges, deadline-guarded,
  with flags that **DEFAULT TO FAILING** so a phase that never runs fails.
- **★ The dialogue-answer trap, BOTH halves:** (a) a prompt raised over an EMPTY
  stack pops to empty on resolve, which `CloseMenu()`s and `Reset()`s the box,
  clearing its stored answer, all in ONE `OnUpdate` -- read
  `ZM_UI_MenuStack::GetLastDialogueAnswer()`, NEVER `GetDialogue().GetChoice()`.
  (b) that host latch is NOT per-test state, so a test must ALSO assert
  `!IsDialogueAwaitingChoice()` before trusting it. Do NOT "capture before and
  require a change" -- a prior test ending on the same answer makes that a false
  failure.
- **★ ANY per-test mutation of persistent state must be RESTORED on EVERY exit
  path**, including a mid-phase failure. `ZM_GameStateManager` outlives a test,
  so SC1's warden phases capture `WARDEN_CLEARED` once and restore it in a
  teardown that runs even when a phase failed with the flag set. Same family as
  the `ResetRuntimeStateForTests` rule: **a new stateful game component MUST be
  wired into the between-tests hook** in `Zenithmon.cpp` or batched tests inherit
  its state.
- **★ Dispatch by the FOCUSED ELEMENT'S NAME**, never `SetOnClick(this)` -- a
  `this` userdata dangles on ECS pool relocation. All screen presenters are
  NON-ECS `Source/UI/` classes owned BY VALUE by the MenuStack, re-resolving
  elements by name each frame.
- **★ Positional aggregate tables: append new columns AT THE END.** Every
  `ZM_NpcData` row is a positional initializer, so a mid-struct field shifts each
  trailing value one column left with no compile error -- `m_bWanders` would
  swallow the gate and the Wanderer would stop patrolling silently.
- **★ No teleportation for movement** -- use `Zenith_Physics`, never
  `SetPosition`, **even in tests**.
- **★ CI-VISIBILITY:** `zm-tests` runs HEADLESS and a SKIP counts as a PASS, so
  every `m_bRequiresGraphics = true` test (the whole walk-up family) is carried by
  the LOCAL WINDOWED gate only. New coverage that must be CI-visible has to live
  in a headless test -- which is why SC1's gate proof went into
  `ZM_NpcDispatch_Test` rather than a new windowed test.
- **Diagnosing a windowed test:** game `Zenith_Log(LOG_CATEGORY_UNITTEST)`
  diagnostics are NOT in harness stdout -- run the exe directly,
  `<exe> --automated-test <Name> --exit-after-frames N` (pair the flag with
  `--automated-test`; bare idles forever). Windowed visual evidence via
  `Flux_Screenshot::RequestDump` in a test Step -> BGRA TGA -> PNG; the CLI
  `--screenshot-frame N` is fragile.
- **New files -> `Build\regen.ps1`.** Sharpmake globs `/Zenith` + each game's
  tree recursively; the generated `.vcxproj`/`.filters` are gitignored, so a new
  FOLDER without a regen link-fails in a way that looks like a code defect.
- **Working model:** MASTER-ONLY (ZM-D-031); the LOCAL gate is the authority;
  `zm-tests` is a post-push backstop (fix forward on red, never revert or
  force-push). Only the orchestrator builds/tests/commits; subagents author and
  never build. Sweep stray `zenithmon.exe` processes before ending. NEVER commit
  baked assets or `Build/artifacts` (git-ignored). A Combat test writes a stray
  `EnemyBase.zpfb` to the repo root during `zenith test Combat` -- delete it if it
  appears, never commit it.
- **Open Questions:** Q-2026-07-21-001 (ENGINE: terrain sets up GPU culling
  resources with no headless guard); Q-2026-07-21-002 (RenderTest
  `RT_TennisDeterminismDigest` fails windowed); Q-2026-07-17-001
  (`ZM_BattleTransition::BiomeForScene` is a hard-coded table, not a
  `ZM_WorldSpec` column); Q-2026-07-12-003 (`ZM_BattleAI`: three in-scope
  rulings -- file location, no-Struggle, tunable thresholds).
- **The next VISUAL hard-stop is the S8 vertical-slice go/no-go** (manual
  playthrough sign-off). S6/S7 have no visual gate -- the loop runs through them
  automatically.
