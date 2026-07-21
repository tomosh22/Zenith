# Zenithmon Status

**Last updated:** 2026-07-21
**Stage:** **S7 (save/load, story flags, trainer battles) ACTIVE. Item 1 (full schema-v1 codec) COMPLETE (SC1-SC2, ZM-D-135/136). Item 2 (story flags + save integration) IN PROGRESS -- SC1 and SC2 of 6 DONE (ZM-D-137/138); NEXT is SC3, world-position capture + resume placement + quit-to-FrontEnd + the autosave latch.** S0-S6 remain complete. S7 requires no human intervention; the next human stop remains the S8 vertical-slice go/no-go.
**Build:** GREEN on the S7 item 2 SC2 gate -- `Build\regen.ps1` GREEN (a NEW `Source/Save/` directory plus two new `.cpp` files, so the regen was genuinely owed) then `zenith build Zenithmon` (`Vulkan_vs2022_Debug_Win64_True`) GREEN. SC2 touched no engine file, so the five-configuration serial matrix and the cross-game sweep were not owed (both last ran in full, green, at the item 1 SC2 boundary). The local gate is the direct-`master` landing authority; CI is the post-push backstop and no CI result is claimed here.
**Tests:** boot unit gate **2458 ran / 2457 passed / 0 failed / 1 skipped** (the 1 skip is the pre-existing unrelated `GraphComponent::RegistryWideNodeRoundTrip` quarantine -- itself the proof that none of the 33 new DISK units skipped), **+33** over the previous 2425; `zm-tests.yml` bumped **2425 -> 2458** from the OBSERVED boot line (an intermediate PREDICTION of 2429 was wrong and was caught by observing). The engine-only reference remains **1103** and `Tools/run_unit_gate.ps1`'s default is untouched. The registered automated-test count is unchanged at **36**: headless **36/0**, then full windowed **36/0/0**, zero skips, no zero-frame tests. `%APPDATA%/Zenith/Zenithmon` was verified EMPTY afterwards -- no residue, and no shipping-named slot file was ever created. S7 has no visual or human gate.

## Current task

**S7 item 2 SC3 -- world-position capture + resume placement + quit-to-FrontEnd + the autosave latch -- is NEXT.** Reuse `ZM_SaveSchema` exactly as ZM-D-136 froze it and `ZM_SaveSlots` exactly as ZM-D-138 shipped it: no codec redesign, no ECS/scene/UI concerns folded down into either layer, no invented v0 migration. `ZM_IsMilestoneStoryFlag` (WARDEN_CLEARED / ROUTE1_OPEN / GYM1_DEFEATED) was authored in SC1 and is still unconsumed -- it is the autosave hook's list -- and `ZM_SaveSlots::ResolveLiveSaveBlocker` is the ONE predicate both the autosave latch and SC4's menu-save must ask. ECS order 113 remains the last occupied game order, so the next free order is **114**. Continue autonomously; the next human stop is the **S8 vertical-slice go/no-go**.

**S7 ITEM 2 SUB-COMMIT PLAN (6 total):**
- **SC1 DONE (ZM-D-137)** -- `ZM_StoryFlags` identity registry + flag-gated NPC lines + the `Npc_Warden` row.
- **SC2 DONE (ZM-D-138)** -- `ZM_SaveSlots`, the typed slot/disk layer over the frozen codec (Save0-2 + Auto). See "Last completed".
- **SC3 NEXT** -- world-position capture + resume placement + quit-to-FrontEnd + the autosave latch.
- **SC4** -- the save-slot screen and root-menu Save/Quit.
- **SC5** -- the title menu and Continue.
- **SC6** -- the S7 stage-gate windowed test (save -> quit-to-FrontEnd -> continue restores position/party/flags exactly) plus an autosave milestone test.

**S7 ITEM 1 (complete):** SC1 (ZM-D-135) froze `ZM_GameState`'s LAYOUT -- reach it with named free functions, never new members -- and SC2 (ZM-D-136) froze the pure transactional 11-module schema-v1 codec plus the exact **824-byte** v1 artifact. Every incompatible change from here owes a version bump + a literal historical-blob migration test IN THE SAME COMMIT.

**Architecture (fixed, do not re-litigate):** exactly **ONE ECS order is consumed for interaction -- 113 (`ZM_Interactable`)**; the NPC walker is a by-value member of it and `ZM_InteractionRuntime` is a by-value member of `ZM_PlayerController` (already on every Player in every scene). **Next free ECS order: 114.** Screens are by-value non-ECS presenters on `ZM_UI_MenuStack` (order 112), so a new screen is one arm per dispatch switch and costs no ECS order. **Save layering is three tiers and the DIRECTORY IS THE BOUNDARY:** `Source/Party/ZM_GameState` (frozen model) -> `Source/Core/ZM_SaveSchema` (pure ZMSV codec, names no file or slot) -> `Source/Save/ZM_SaveSlots` (slots, files, the engine save layer; adds nothing to the payload). Interaction is a forward CONE, never a raycast -- not for a headless reason (**physics IS live headless**; ZM-D-127 corrected that false claim) but because the cone stays pure and unit-testable; S7's trainer occlusion ray enters as a probe filter in the GLUE layer, leaving the pure picker untouched. Five authored Dawnmere NPCs only (villager / Trade Post clerk / Caretaker / wanderer / **warden**) -- populated towns are S9/S10. **"Trade Post"**, never "Mart", in data/entity/asset names. NO RNG in the walker (TestPlan C8).

**S6 CLOSURE RULING (ZM-D-134, still binding):** behaviour graphs and navmesh-driven wandering were deferred to S7 -- `ZM_GraphAuthoring` is not written and S6 ships a bounded 3-arm C++ role dispatch behind one `Interact()` seam. `Zenith_NavMeshGenerator::GenerateFromGeometry` is terrain-capable when supplied suitable triangles, but `Zenith_AINavGeometry::GenerateFromScene` does **not** harvest streamed terrain geometry or a heightfield. S7 item 3 owns the first useful graph integration plus the terrain-triangle/grid-coverage and `.znavmesh` evaluation. `MasterPlan.md` is historical/read-only.

**PER-SC GATE -- run in this exact order, every time:** `Build\regen.ps1` (ONLY when a new .cpp or folder was added) -> `zenith build Zenithmon` -> `zenith test Zenithmon --headless` (heals DLLs) -> `Tools\run_unit_gate.ps1 -Exe ... -Baseline <N> -TimeoutSec 300` (the 300 s timeout-kill is EXPECTED) -> full windowed `zenith test Zenithmon`. **Two standing tripwires:** (a) never write a PREDICTED unit count into `zm-tests.yml` -- only the OBSERVED one from the boot log; (b) the engine baseline **1103 must remain unchanged** unless an explicitly-scoped engine change owns the cross-game gate.

## Last completed

**S7 item 2 SC2 -- THE TYPED SLOT/DISK LAYER OVER THE FROZEN CODEC (ZM-D-138).**
New `Source/Save/ZM_SaveSlots.{h,cpp}` -- a NEW directory, so the regen was owed
-- sits ON TOP of the ZMSV codec frozen by ZM-D-136 and adds nothing to the
payload: four slots (`ZM_SAVE_SLOT_0/1/2` manual + `AUTO`), a three-state probe
(`EMPTY` / `READY` / `DAMAGED`), typed `WriteState` / `ReadState` returning
`Zenith_Status`, `AnySlotOccupied` / `AnySlotReady`, `DeleteSlotFile`, and a pure
save-blocker predicate with a fixed precedence. **The header names no ECS type,
scene, UI element or component**; the .cpp's ONE live-state reach is
`ResolveLiveSaveBlocker` at the very bottom, which forwards four bools into the
pure predicate above it. Position capture, quit-to-title, autosave policy and the
save screen are later sub-commits and deliberately do not exist here.

**The one piece of framing this layer adds.** The engine payload region is
`[u32 little-endian ZMSV byte length][ZMSV blob]`, written BYTE BY BYTE so the
field cannot inherit host byte order or struct padding. It exists because
`ZM_SaveSchema::Read` demands an EXACT length while the engine's two Load paths
DISAGREE about `Zenith_DataStream::GetCapacity()`: the disk path wraps an
exactly-payload-sized buffer (capacity == payload) while the staged-readback path
hands the callback a DEFAULT-CONSTRUCTED OWNING stream whose capacity is the whole
1024-byte allocation. The prefix makes both paths byte-identical to the codec,
carries no magic or version (ZMSV's own sit four bytes later), and is framing
AROUND the frozen payload -- the **824-byte** v1 golden is untouched.
`uGAME_VERSION = 1` goes into the ENGINE header only; `LoadEx` never inspects it.

**Two orderings in `WriteState` are load-bearing.** (1) The payload is STAGED AND
VALIDATED before `Zenith_SaveData::Save` is called at all, because Save creates
the file the instant it is called -- validating inside the write callback would
leave a zero-length file behind for every rejected state. (2) Save's return value
is DISCARDED (a literal `true` on every path: `WriteToFile` is void and
`Zenith_FileAccess::WriteFile` only logs), so the answer comes ONLY from a
RE-PROBE -- this layer's sole evidence that the save landed and is loadable. A
DAMAGED slot is surfaced, NEVER repaired, deleted or auto-overwritten; `ProbeSlot`
is deliberately uncached and decodes into a scratch state it throws away. Three
states, not a bool, because "no file" and "unreadable file" demand OPPOSITE UI --
and for the same reason `AnySlotOccupied` COUNTS a damaged slot while
`AnySlotReady` is stricter. Name maps are TOTAL and every name is a compile-time
literal, because `Zenith_SaveData::BuildSlotPath` does ZERO sanitisation.

**★★ DEFECT 1 -- THE TEST-SLOT INTERLOCK WAS KEYED ON A FLAG THAT IS NEVER SET
DURING THE BOOT UNIT RUN** (`Zenith_CommandLine::IsAutomatedTestRun()`; see the
first note below for why it is always false there), which was simultaneously a
TOTAL COVERAGE HOLE and a DATA-LOSS HAZARD. **Consequence A:** every disk unit hit
`ZENITH_SKIP`, and a skip COUNTS AS A PASS, so the entire storage half of this
sub-commit was green-by-skip and could not fail. **Consequence B:** that same
interlock is all that stops those units writing and then DELETING the real
`Save0`/`Save1`/`Save2`/`Auto` files -- and the boot suite runs on EVERY boot,
including an ordinary interactive run. Fixed by making the redirection EXPLICIT: a
test-only setter plus a default-FALSE file-scope flag that the tests' RAII
`ZM_SlotDiskScope` sets on entry and clears on exit; the scope VERIFIES the names
moved off their shipping stems BEFORE it deletes anything; the per-test skip became
a HARD FAILURE; and a unit (declared first so it RUNS LAST -- registration
prepends) asserts the redirection defaults OFF, so a leaked redirect reds.

**★★ DEFECT 2 -- THE LAYER'S OWN REJECTION BRANCHES WERE DEAD TO THE TESTS.**
DAMAGED was only ever manufactured by flipping a byte on disk, which fails the
ENGINE's CRC inside `LoadEx` before the read callback is even invoked -- so every
payload-rejection branch this layer owns could have been deleted with all units
still green. Closed by staging a readback whose inner ZMSV magic is corrupt: the
stash is consulted AHEAD of the file, bypassing the CRC gate entirely.

**★★ DEFECT 3 -- THE VERIFY RE-PROBE WAS UNPINNED.** Deleting it and returning
success straight after Save left every unit green. Closed by staging a corrupt
readback for the TARGET slot BEFORE the write, so the file that lands is good and
only a layer that BELIEVES its re-probe reports `CORRUPT_DATA`; three non-vacuity
asserts confirm the write reached the engine, that a file landed (the CORRUPT_DATA
arm, not FILE_NOT_FOUND) and that the poison never leaked into the written bytes.
**Mutation-verified: making `WriteState` accept a DAMAGED verdict turns the gate
RED (1 failed); restored, green.**

**★★ DEFECT 4 -- THE TOO-SMALL-FOR-A-PREFIX GUARD HAD ZERO COVERAGE** and was
unreachable from any staged fixture (their capacity is always the full 1024-byte
allocation, and the CRC-flip helper preserves the declared length). Closed by a
unit driving the DISK path with a hand-built `.zsave` declaring a 2-byte payload
with a MATCHING CRC, plus a well-formed control arm through the same builder so
the verdict is attributable to this layer's guard rather than an engine gate.
Deleting the guard asks a 2-byte stream for 4 bytes -- a FATAL bounds assert.

**Also corrected pre-commit (the recurring false-claim class):** a comment claimed
an unbounded prefix "would walk far past the buffer" -- FALSE, the frozen codec
applies an equivalent bound itself, so this layer's check is DEFENCE IN DEPTH; its
unit now states that honest scope. Plus a stale doc citation at four sites, a
miscounted unit total, and a stale line citation.

**Evidence.** +33 `ZM_Save` units in `Tests/ZM_Tests_SaveSlots.cpp` (6 pure
name/policy, 27 disk-scoped) covering the name maps and their totality, the three
probe verdicts, a maximal eleven-module round trip, per-slot and Auto
independence, overwrite-not-append, prefix+blob framing and little-endianness,
staged decode through the REAL engine seam, all four prefix/payload rejection
shapes, out-of-range/invalid-state rejection with NO file created, delete
semantics, occupancy vs readiness, and all SIXTEEN blocker combinations by hand.
`Zenithmon.cpp`'s between-tests hook now calls
`ZM_SaveSlots::DeleteAllSlotsForTests()` BEFORE `Zenith_SaveData::ClearForTest`.
Regen GREEN; build GREEN; boot **2458 / 2457 / 0 / 1**; `zm-tests.yml` bumped
**2425 -> 2458** from the OBSERVED line; engine reference **1103 UNCHANGED** (no
engine file touched, so no cross-game sweep was owed); headless **36/0**; windowed
**36/0/0**, zero skips; save directory verified EMPTY afterwards. **Contracts
held:** `ZM_SaveSchema` untouched (824-byte golden unchanged), `ZM_GameState`
layout untouched, no new ECS order (next free **114**), no
`uSERIALIZATION_VERSION` bump. No commit, push or CI result is claimed.

---

Prior: **S7 item 2 SC1 -- STORY-FLAG IDENTITY REGISTRY + FLAG-GATED NPC LINES
(ZM-D-137).** `Source/Data/ZM_StoryFlags.{h,cpp}` gives the ZM-D-135 bitset an
identity: a SAVE-STABLE `enum ZM_STORY_FLAG_ID : u_int` whose **value IS the
persisted bit index in save-schema module 4**, six flags dense from zero, APPEND
ONLY, with a DEDUCED-bound compiled row table (a missing row is a COMPILE error),
free-function accessors (because `ZM_GameState` is frozen) and a TOTAL fail-closed
`ZM_StoryGatePasses` whose default-constructed gate is unconditional.
`ZM_IsMilestoneStoryFlag` is authored beside it and still UNCONSUMED. First
gameplay consumer: `ZM_NpcData` gains three fields APPENDED AT THE END, a pure
`ZM_SelectNpcLines` picks the set, and the `ZM_NPC_RAISE_DIALOGUE` arm reads LIVE
flags; **gating selects CONTENT, never which seam a role talks through.** Fifth
Dawnmere NPC `Npc_Warden` is the first gated row. **Three defects, all now
standing rules below:** a `Zenith_Assert` on a unit-pinned input destroyed the
WHOLE boot gate; the new NPC row silently disarmed a roster unit by supplying a
second TALKER; and the gated branch was pinned by nothing until two
mutation-verified phases went onto the HEADLESS `ZM_NpcDispatch_Test`. +33 units;
boot **2425 / 2424 / 0 / 1**; windowed **36/0/0** with every S6 frame count held
exactly (`ZM_PlayerHomeRoundTrip` **831**, `ZM_NpcWander` **830**,
`ZM_S6InteractGate` **749**, `ZM_NpcHeal` **315**, `ZM_NpcShop` **286**).

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

- **★★ NEW -- NEVER INFER "AM I UNDER TEST" FROM A PROCESS FLAG THE RUN IN
  QUESTION DOES NOT SET, AND NEVER LET A SAFETY INTERLOCK DEGRADE TO A SKIP.**
  SC2's `"_Test"` slot redirection was keyed on
  `Zenith_CommandLine::IsAutomatedTestRun()`, FALSE for the boot unit suite:
  `Tools/run_unit_gate.ps1` passes only `--headless --exit-after-frames 120`, a
  developer launch passes nothing, and `zenith test <Game>` (the only caller that
  sets `--all-automated-tests`) also passes `--skip-unit-tests`. That one wrong
  key was BOTH a total coverage hole (every disk unit skipped, and **a skip counts
  as a PASS**) and a data-loss hazard (the interlock is all that stops a boot-time
  unit deleting the player's real `Save0/1/2/Auto`). Make the interlock EXPLICIT
  (test-only setter + default-false static, driven by an RAII scope), have the
  scope VERIFY it took effect BEFORE touching disk, make the per-test check a HARD
  FAILURE rather than a skip, and add a unit asserting the default is OFF --
  declared FIRST in the TU, because `Zenith_TestRunner::RegisterTest` PREPENDS, so
  declaration order is the REVERSE of execution order.
- **★★ NEW -- A NEW BRANCH IS PINNED BY NOTHING UNTIL A FIXTURE REACHES IT, AND A
  LOWER LAYER WILL HAPPILY REJECT YOUR FIXTURE FIRST.** SC2 shipped three branches
  no test could execute (see "Last completed" defects 2-4). The reusable levers:
  `Zenith_SaveData::SetReadbackForTest` stages bytes consulted AHEAD of the file
  and so BYPASSES the engine CRC gate that otherwise rejects a corrupted fixture
  before the game's read callback runs at all; a hand-built `.zsave` with a
  matching `Zenith_SaveData::ComputeCRC32` is the only way to reach the disk path's
  exact-capacity behaviour; and every such fixture needs a WELL-FORMED CONTROL ARM
  so the verdict is attributable to YOUR layer, not an engine gate. Same family as
  SC1's benign-fallback lesson: when a new path's default output is
  indistinguishable from the old behaviour, only a fixture that takes the new path
  proves anything, and it must distinguish the outputs by CONTENT.
- **★★ NEW -- SC3/SC6 HAZARD: `ZM_GameStateManager` IS `DontDestroyOnLoad` AND ITS
  FrontEnd RE-AUTHOR PATH DESTROYS THE DUPLICATE RATHER THAN RESEEDING**
  (`OnStart` retires the duplicate entity and returns), so the live `ZM_GameState`
  survives quit-to-title ENTIRELY IN RAM and a naive "save -> quit -> continue"
  test passes GREEN against a Continue that reads ZERO bytes from disk. SC6's gate
  test MUST scramble the live state (or otherwise prove the load path ran) before
  continuing, or it proves nothing.
- **★ SCHEMA V1 IS FROZEN (ZM-D-136), `ZM_GameState`'s LAYOUT IS FROZEN
  (ZM-D-135), AND THE SLOT LAYER IS NOW ITS ONLY DISK OWNER (ZM-D-138).**
  Preserve the exact 11-module order, fixed widths, 61-byte monster encoding,
  statuses and transactional cursor/destination behavior in `SaveFormat.md`; every
  incompatible change owes a real version bump + a literal historical-blob
  migration test in the same commit. There is no v0. Reach the frozen model with
  named FREE FUNCTIONS (the `ZM_StoryFlags` pattern), never new members. Do NOT
  fold slot, ECS, scene or UI concerns down into `ZM_SaveSchema` or `ZM_SaveSlots`
  -- the directory IS the boundary.
- **★ THE SLOT LAYER'S CONTRACTS SC3-SC6 MUST NOT BREAK:** the 4-byte
  little-endian length prefix exists because the engine's two load paths disagree
  about `GetCapacity()` while the codec demands an EXACT length -- never pass
  `GetCapacity()` as that length and never branch on `OwnsData()`. `WriteState`
  answers from its RE-PROBE, never from `Zenith_SaveData::Save`'s return (a
  literal `true` on every path). A DAMAGED slot is SURFACED, never repaired or
  auto-overwritten, and COUNTS as occupied for Continue visibility.
- **★ STORY-FLAG INDICES ARE WIRE FORMAT.** `Source/Data/ZM_StoryFlags.h` is the
  authoritative index registry: append only, dense from zero, never renumber or
  reuse. Module 4 sizes itself from the highest SET index, so one sparse index
  costs ceil bytes in EVERY save forever. Reserve a flag by adding a row, never by
  leaving a gap; renaming a debug name is free. `ZM_IsMilestoneStoryFlag` is the
  autosave hook's list and is still unconsumed.
- **★ A TOTAL FUNCTION MUST NEVER `Zenith_Assert` ON ITS ARGUMENTS.**
  `Zenith/Core/Zenith.h:138` defines `ZENITH_ASSERT` unconditionally immediately
  ABOVE its own `#ifdef ZENITH_ASSERT`, so the definition at `:140` always wins
  and `Zenith_DebugBreak()` fires in EVERY configuration. Units run at BOOT, so an
  assert on an input a unit deliberately supplies kills the process and **the
  whole boot gate is lost -- no "Unit tests complete" line prints at all**, which
  reads as a build failure rather than one red unit. Diagnose mis-authored data
  with a non-fatal `Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)` -- there is no
  `LOG_CATEGORY_GAME` -- and return the defined fail-closed answer. (Main-thread
  guards are the safe exception: boot units run on the main thread.)
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
  rest. Use `Tests/ZM_AutoTests_NpcServices.cpp` as the canonical source for any
  new walk (ZM-D-130/131) and REUSE its one parameterised `WalkContext` /
  `TickWalk` machine -- there is NO shared test header (`Tests/` is `.cpp` only)
  and every sibling helper sits in a per-file anonymous namespace, which is
  exactly why another hand-rolled `DriveTowardXZ` must not be written.
- **★ CHECK EXISTING TRAVERSAL ROUTES BEFORE PLACING ANYTHING SOLID.**
  `DriveTowardXZ` has no obstacle avoidance, so a solid AABB on a corridor an
  existing windowed test walks blind will kill that test at its frame cap. SC1's
  warden derives every separation explicitly (18 m off the z=480 Home corridor,
  34 m off the x=512 spawn corridor, 20.0 m to the nearest NPC) -- copy that
  style, and re-derive from scratch if anything moves.
- **★ MUTATION-TEST any test you claim has teeth** -- break the thing it should
  catch, rebuild, confirm RED (exit=1), restore, re-gate. Five consecutive
  sub-commits; three times it revealed the test proved nothing.
- **★ The recurring review win is finding tests that CANNOT FAIL** -- loops
  bounded by a count just asserted zero; "unchanged" asserts on state never
  populated; a totality test whose expectation calls the function under test; an
  expectation DERIVED from the production table it is meant to pin. Check for
  vacuity EXPLICITLY on every SC, naming per unit the change that would red it.
- **★ THE RECURRING DEFECT CLASS: confidently-worded FALSE claims inside
  argumentative comment passages.** SC1 found seven copies of one (`QueueLines`
  "would crash" on a null array -- it rejects it and the real consequence is a
  MUTE NPC); SC2 found a "would walk far past the buffer" claim that ignored the
  frozen codec's own equivalent bound. Verify every claim a comment makes about
  another function BY READING that function.
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
  with flags that **DEFAULT TO FAILING**.
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
  `Zenith_SaveData::ClearForTest`).
- **★ Dispatch by the FOCUSED ELEMENT'S NAME**, never `SetOnClick(this)` -- a
  `this` userdata dangles on ECS pool relocation. Screen presenters are NON-ECS
  `Source/UI/` classes owned BY VALUE by the MenuStack.
- **★ Positional aggregate tables: append new columns AT THE END** -- every
  `ZM_NpcData` row is a positional initializer, so a mid-struct field shifts each
  trailing value one column left with NO compile error.
- **★ No teleportation for movement** -- use `Zenith_Physics`, never
  `SetPosition`, **even in tests**.
- **★ CI-VISIBILITY:** `zm-tests` runs HEADLESS and a SKIP counts as a PASS, so
  every `m_bRequiresGraphics = true` test (the walk-up family) is carried by the
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
