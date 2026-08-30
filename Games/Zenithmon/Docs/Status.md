# Zenithmon Status

**Last updated:** 2026-08-28

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

**★ LIVE PIN (UPDATED 2026-08-30):
ZM boot `3554`; engine boot (Null Combat) `1748`; Null RenderTest `1839`; registry **70**.**

> **★ ZM +16 (2026-08-30), the Dawnmere BUILDING FACADE + INTERIOR overhaul and the game-wide PBR pass, Zenithmon-only.**
> `ZM_BuildingGen` stopped emitting one box with a flat 256^2 picture on it and
> now emits FOUR SURFACE MESHES per building (wall / roof / trim / glass), each
> with its own tiling world-scaled UVs and its own four-map PBR set (albedo,
> BC5 normal, packed roughness/metallic, AO). Six units migrated and nine are
> new: the surface tables, the asset-kind algebra, the shell-metric derivation,
> the site-fixed jitter suppression, surface registration, world-UV density,
> the map-set structure, texture domain isolation, and the clause that pins the
> two Dawnmere buildings inside their blockout colliders. Plus two facade
> placement units and `EveryGameComponentIsInBothRegistries`. OBSERVED at
> `3554 ran / 3552 passed / 0 failed`, 2 skipped, from a
> `Null_vs2022_Debug_Win64_True` run. Nothing under `Zenith/**` was touched, so
> the Combat and RenderTest rows do NOT move.
>
> **★ AND `ZM_InteriorTintPixels_Test` IS GREEN, HONESTLY.** The last open
> `requiresGraphics` rot item measured a red/blue gap of **0.121** against a 0.15
> floor while the two rooms were the same grey blockout box under one hue nudge.
> They are now made of different materials and lit by differently-coloured lamps:
> OBSERVED PlayerHome 1.5258, ProfLab 0.7809, **gap 0.7449** -- five times the
> floor, which was NOT moved.

> **★ ZM +8 (2026-08-30, ZM-D-218 follow-up), Zenithmon-only.** The eight
> duplicated copies of the automated suites' walk driver became ONE --
> `Tests/ZM_TestWalkDrive.{h,cpp}` -- and the arithmetic is now a PURE function
> with its own suite, `ZM_WalkDrive` (`Tests/ZM_Tests_WalkDrive.cpp`, 8 units):
> the camera basis, the fixed points, the anti-vacuity half that fails if the
> quantisation is ever removed, the dead zone as the arrival condition, opposed
> keys, totality, and planarity. OBSERVED at
> `3538 ran / 3536 passed / 0 failed`, 2 skipped, from a
> `Null_vs2022_Debug_Win64_True` run. Nothing under `Zenith/**` was touched, so
> the Combat and RenderTest rows do NOT move.

> **★ ZM +0 (the v8 compaction itself, 2026-08-30, ZM-D-218).** The Dawnmere v8 compaction adds NO units and
> the pin does NOT move: it re-derives three clauses that had rotted into Z-gap
> proxies and re-records constants, all inside suites that already existed.
> OBSERVED at `3530 ran / 3528 passed / 0 failed`, 2 skipped, from a
> `Null_vs2022_Debug_Win64_True` run AFTER the change. Registry stays **70**;
> 69 pass and the one failure is the pre-existing, tracked
> `ZM_InteriorTintPixels_Test` (interior lighting, nothing to do with Dawnmere).

> **ZM +13 (2026-08-30, ZM-D-217), Zenithmon-only.** The Dawnmere v7 shrink and
> its scenery layer add ONE new suite, `ZM_Dressing`
> (`Tests/ZM_Tests_DawnmereDressing.cpp`, 13 units): the tree-clump keep-out
> clearances, the two keep-out variants and their divergence, the scatter table's
> well-formedness and the accessors' totality. OBSERVED at
> `3530 ran / 3528 passed / 0 failed`, 2 skipped, from a
> `Null_vs2022_Debug_Win64_True` run, and confirmed through
> `Tools/run_unit_gate.ps1 -Game Zenithmon`. Nothing under `Zenith/**` was
> touched, so the Combat and RenderTest rows do NOT move -- the figures quoted
> above for them are `Tools/unit_baselines.json`'s, carried forward and NOT
> re-observed on this tree.
>
> ★ THE PREVIOUS LINE HERE SAID `3498` AND THE GATE'S OWN FILE SAID `3517`.
> `Tools/unit_baselines.json` is the authority and was right; this narration had
> drifted by 19 across the ScriptTest commits, which added backend-neutral engine
> units that move every game's row. Re-read the JSON before trusting this block.

> **ENGINE-SIDE +36 (2026-08-28), Zenithmon code untouched.** Three shared prop-set
> unit suites landed in `Tools/` — rocks (+13), deadwood (+14), wind-animated
> bushes (+9) — and every game boots the same engine suite, so all three rows moved
> together. Each number OBSERVED from its own `Null_vs2022_Debug_Win64_True` run
> (`3498 ran / 3496 passed / 0 failed`, 2 skipped); registry unchanged. The two
> earlier bumps (3475, 3489) were narrated only in the manifest's git history —
> this line was stale against the manifest from 3475 on, which C7 caught.

> **★★★ BEFORE THE NEXT LAYOUT CHANGE, READ
> [MapLayoutPlaybook.md](MapLayoutPlaybook.md).** Dawnmere has been re-laid-out
> four times and every one of them rediscovered something on that page the hard
> way. It carries the order of operations, the "if you change X you must
> re-derive Y" table, and fifteen traps indexed by SYMPTOM. Everything below
> this line is the narrative; the playbook is the procedure.

> **★★ THE HOUSE AND THE LAB ARE 56 m APART -- v8, OBSERVED 2026-08-30 (ZM-D-218).**
> **ZM PIN 3530 -> 3530 (unchanged).** Zenithmon-only; nothing under `Zenith/**`.
>
> v7 shrank the sheet and compacted the content, and the walk from the player's
> house to Aster's lab was still 135 m. v8 halves the sheet again and re-plans the
> town centre around that ONE distance.
>
> * **4 x 4 chunks = 256 x 256 m** (was 6x6 = 384x384), 16 chunks not 36, **52**
>   required bake outputs not 112. Terrain manifest **v7 -> v8**. The sheet is now
>   44% of v7's area and 18% of v5's.
> * **Home door -> Lab door 135.0 m -> 56.1 m, a 58% cut**, which was the ask.
>   spawn->Home 64.0 -> 48.8 m, spawn->Lab 76.9 -> 52.2 m, plaza->route boundary
>   192 -> 128 m. Everything HUMAN-scale is untouched again.
> * **The structural unlock was ending the Lab lane at the FORECOURT** instead of
>   behind the building. v7's Lab was pushed far out on a diagonal by a coupling
>   nobody had named: `LabDirtPath_ClearsTheShellByTheShippedCameraClamp` wanted
>   camera clearance BEHIND the shell, so the lane had to run past it. Ending the
>   lane at the door removes the coupling; the clause's anti-vacuity half was
>   RE-DERIVED (the lane must reach the door and arrive from the -Z side) rather
>   than deleted.
> * **Rival Vesper moved to (80, 60)** -- due west of the town-centre spawn, on
>   the same Z -- and his frozen facing was re-derived to yaw `atan2f(40, 0)` =
>   pi/2, whose two quaternion words are both `0x3F3504F3`.
>
> ★★ **THE MOST USEFUL THING v8 MEASURED: THE WALK-UP'S DRIVER DOES NOT WALK IN
> A STRAIGHT LINE, AND THE RIVAL'S FACING IS DERIVED FROM ONE.** `DriveTowardXZ`
> in `Tests/ZM_AutoTests_RivalVesper.cpp` is CAMERA-RELATIVE and quantised to
> eight directions -- it holds W/A/S/D, never a steering angle -- while the camera
> swings to follow the player's own heading. The player therefore walks a pursuit
> curve that cuts the corner, and the lateral offset grows with the length of the
> walk. The first v8 placement put the rival at (80, 88), a 48.8 m diagonal, and
> the player arrived **4.44 m to one side** -- a 31-degree bearing error against a
> cone that admits 30. MEASURED, closing in:
>
> | gap | coneDot | | gap | coneDot |
> |---|---|---|---|---|
> | 24.0 m | 0.974 | | 10.3 m | 0.853 **out** |
> | 12.0 m | 0.888 | | 5.0 m | 0.787 |
> | 8.5 m | 0.853 **out** | | 0.8 m | 0.543 |
>
> (the cone admits `>= 0.86603`.) The dot DECAYS as he closes, because the player
> is arriving from the side he drifted to. So the rival was correctly placed,
> exactly facing, armed, WATCHING, with the player driven to **0.077 m** of him --
> and permanently blind, with every clause the test already printed GREEN. Three
> plausible hypotheses (props on the sight line, the approach being too long,
> unflattened ground under a dynamic capsule) were each expensive to falsify and
> none of them was it. **A due-west or 45-degree-diagonal target is a FIXED POINT
> of that driver** -- both camera orientations drive the same straight line -- so
> the cone dot now holds flat at ~0.95 the whole way in.
>
> ★ **AND THE DIAGNOSTIC IS PERMANENT NOW.** `RVLogSightGateBreakdown` prints the
> four FSM inputs, the cone arithmetic and the camera basis on every failing exit
> of the approach, because none of them was reachable through an accessor and all
> of them are pure functions of state the test already held.
>
> ★★ **THREE DEAD Z-GAP PROXIES RETIRED IN ONE PASS.** Every one read
> `|npcZ - townCentreZ| > k` on the reasoning that a traversal corridor is a line
> of constant Z. None has been since ZM-D-173. In the compacted town they are
> wrong in BOTH directions: one failed the warden at a 4 m Z gap while he stands
> 18.0 m from the leg measured properly, and all three would have passed an NPC
> standing ON a diagonal. They now measure perpendicular distance to the legs in
> `ZM_DawnmereDressing.h`'s table -- the one place those legs are described, and
> the same table the scenery keep-out reads, so a body and a boulder are held to
> one definition of "the corridor".
>
> ★ **A TREE IS NOT SUBJECT TO THE PROP KEEP-OUT.** The tree brush is a TERRAIN
> tool and knows nothing about `ZM_DawnmereDressing`, so `SouthWestLobe` reached
> to within 2.4 m of the armed rival while every scatter clause was green. A trunk
> on his line makes `ZM_ProbeTrainerSightLine` report blocked and he never reacts.
> `TreeClumps_AreEntirelyOutsideTheTownKeepOut` measures against the HARD
> clearance, which carries his sight radius, and is what reds if it creeps back.
>
> ★ **A DRIVE LEG BELONGS IN THAT TABLE ONLY IF SOME TEST DRIVES IT.** v8 briefly
> carried an invented `townCentre->routeArrival` leg -- the whole length of the
> route lane -- on the assumption that `ZM_SeamRoundTrip_Test` walks north out of
> town. It does not: it WARPS onto the departure scene's spawn tag and drives
> ~12 m from there. The invented leg sterilised an 84 m strip for scenery and, once
> the roster started measuring against the table, failed the VILLAGER, who has
> stood on the lane centreline since S6 and is on nothing anything drives.

> **★★ DAWNMERE IS 40% OF ITS OLD AREA AND HAS SCENERY -- v7, OBSERVED 2026-08-30 (ZM-D-217).**
> **ZM PIN 3517 -> 3530.** Zenithmon-only; nothing under `Zenith/**`.
>
> The complaint was that Dawnmere read as empty, and it was two separate
> problems. **The map was too big for its content:** 576 x 640 m with the plaza
> and the two buildings 128 m apart and a 416 m unbroken run from the plaza to
> the north boundary. **And it had no scenery at all:** every entity in it was a
> blockout, a marker or a person. v7 answers both.
>
> * **6 x 6 chunks = 384 x 384 m** (was 9x10 = 576 x 640), 36 chunks not 90,
>   112 required bake outputs not 274. Terrain manifest **v6 -> v7**.
> * **The content is COMPACTED, not translated** -- v5 -> v6 was a pure translate
>   and left the town exactly as sparse as it had been. plaza->Home 128.9 -> 65.9 m,
>   plaza->Lab 134.1 -> 75.5 m, plaza->route boundary 416 -> 192 m, town core width
>   352 -> 194 m. Everything HUMAN-scale (door apertures, jamb widths, sensor
>   depths, shell envelopes, the 12 m gate-to-arrival separation) is untouched.
> * **~770 instanced props + ~440 trunk/leaf tree pairs**, all from the SHARED
>   engine sets (`Meshes/{ProceduralTree,Rocks,FallenTrees,Bushes}`) -- the same
>   sets RenderTest dresses its campus with. No Zenithmon-owned art.
>   `Dawnmere.zscen` **25 entities / 6,026 bytes -> 38 / 113,627**.
> * **All 23 measured ground columns re-frozen** from a warm v7 bake, and the
>   frozen rival facing re-derived (yaw `atan2f(24, -40)`).
>
> ★★ **THE MOST USEFUL THING v7 MEASURED: A PAD DOES NOT FLATTEN ANYTHING.**
> Zenith_TerrainEditor's FLATTEN kernel moves a texel
> `(target - h) * falloff * strength * 0.35` per dab and a PAD contributes exactly
> two dabs, so it converges at most 58% even at its own centre. The Plaza pad has
> NEVER flattened the town square; v6 read within 0.85 m of target because its
> base noise happened to sit near 24 m there. v7's first bake exposed it as a
> 3.15 m spread across the roster. The fix is three **SetHeight SHELVES** in the
> landform table, all at strength < 0.5 -- 0.5 SATURATES and assigns the target
> outright, which two separate units caught (the W5 spread clause at 1.0, the lab
> no-repeated-rows clause at 0.6). Both clauses were left alone and the DATA moved.
>
> ★ **The scenery is kept off every walked line BY GEOMETRY, in a unit.**
> `Source/World/ZM_DawnmereDressing.h` computes the keep-out from the recipe's own
> pads and paths, the placement header's anchors and markers, and the seven blind
> `DriveTowardXZ` legs; `ZM_Tests_DawnmereDressing.cpp` walks every leg end to
> end. That hazard had been PROSE in `Zenithmon.cpp` since S6. There are TWO
> variants and the collider decides: colliding props take pads/paths at their
> FLATTEN radius, collider-free foliage at their DIRT radius, because a bush
> cannot wedge a blind drive and the only rule left for it is "stay off the paving".
>
> ★ **Two columns do NOT read the 24.0 target and that is a finding, not a
> tolerance:** the route seam (22.325) and the north gate (22.681) sit outside the
> town shelf and are corrected only by path flatten dabs. It falsifies a claim
> both `ZM_DawnmerePlacement.h` and its north-gate block made -- "a flatten dab
> drives ground TO the recipe target" -- and both are corrected. Left as measured
> rather than shelved flat: 1.7 m of descent over the 128 m to the seam is a 1.3%
> grade, i.e. a road, and the per-column tables are exactly what lets the seam
> entities sit correctly on it.
>
> ★★ **AND ONE THING THIS SLICE GOT WRONG AND UNDID: CHECK WHICH SCENE A CAPTURE
> IS OF.** A screenshot of "the north walk" showed a player at the bottom of a
> rock-splatted ravine, which produced a plausible diagnosis (a graded lane's
> shoulder is steeper than the hills, so the auto-splat's 18-degree `Stone` rule
> paints it as cliff) and two real terrain changes: a fourth shelf over the
> corridor and the route's flatten radius 14 -> 22. Both worked -- the seam
> columns came up from 22.33/22.68 to 23.92/23.90. **The frame was ROUTE 1, not
> Dawnmere**: `ZM_SeamRoundTrip_Test` warps out of town on its second leg, and
> Route 1's recipe was never touched by any of this. The tell was there and was
> read late: the capture came back PIXEL-IDENTICAL across two substantive terrain
> edits and a forced re-bake. Both changes are REVERTED rather than kept with the
> justification quietly rewritten, and `Dawnmere.zscen` hashing back to its
> pre-experiment value is the proof the revert was complete. The boot log prints
> `[TerrainPhysics] context='...<scene>.zscen'` on every warp.
>
> Proof: two windowed `Vulkan_vs2022_Debug_Win64_True` boots with
> `--skip-unit-tests`, `warmMask=0x7`, `sceneAuthoring=AUTHOR_DAWNMERE`; the
> second reported `[ScenePublish] IDENTICAL` for all seven scenes. All four
> ground-truth oracles green, 70/70 automated tests pass, and
> `Tools/run_unit_gate.ps1 -Game Zenithmon` reports PASS at 3530.

> **★ THE LANES ARE PAVED — the shared CLAY ground set, OBSERVED 2026-08-28 (ZM-D-216).**
> **NO PIN MOVED.** Zenithmon-only in effect (the one engine-tree edit is RenderTest's
> RM-packer list), the existing ground-slot units were WIDENED again rather than
> duplicated, so ZM stays at **3462** (`3462 ran / 3460 passed / 0 failed`, 2 skipped,
> `Null_vs2022_Debug_Win64_True`) and the registry stays at **70**.
>
> Splat slot 2 of all three outdoor recipes — the `Dirt` slot the path and pad specs
> paint via `m_fDirtRadius` — now samples `engine:Textures/Terrain/Clay/` with a
> **WHITE** base colour. Slot 3 (Heath / Hedgerow / Wildflower) is the last flat-colour
> slot in the game.
>
> ★★ **IT TILES AT 3.6, NOT 0.9, AND THAT IS DELIBERATE.** Grass and rock are
> non-repeating photographic ground, so their 16 m repeat (tiling 0.9) hides. The clay
> map is a **regular 6x6 grid of paving slabs**, so its repeat is an architectural
> dimension the player measures directly: at 0.9 each slab would be 15.9/6 = **2.6 m**
> and every lane would read as a chessboard. 3.6 gives a 3.97 m repeat and a **0.66 m**
> slab. The unit that polices tile size therefore carries **two windows** — 14-18 m for
> ground, 3.5-4.5 m for paving — with a `static_assert` that they do not overlap, so a
> later "tidy these to one number" cannot pass.
>
> **THE THREE OUTDOOR `.zscen` FILES MOVED AGAIN** — see the hash table below; +164
> bytes each, entity counts unchanged, same two-boot proof as the rock set.
>
> ★ **The Clay set had to be BUILT first, and the second stage is easy to miss.** Stage 1
> (`ExportAllTextures`, `Zenith/Core/Zenith_Engine.cpp`) converts every jpg under the
> engine assets tree to a BC1 `.ztxtr` on ANY tools boot. Stage 2 — the packed
> roughness+metallic map the terrain shader samples as `xRM.gb` — is written ONLY by
> `RenderTest_PackTerrainRoughnessMetallic`, from a hand-maintained list of set
> directories in `Games/RenderTest/RenderTest.cpp`. A set missing from that list has
> three of its four maps and falls back silently on the fourth, so the ground-slot unit
> now checks all four maps exist for the clay row too.

> **★ THE STEEPS STOPPED BEING A FLAT COLOUR — the shared ROCK ground set, OBSERVED 2026-08-27 (ZM-D-215).**
> **NO PIN MOVED.** Zenithmon-only (nothing under `Zenith/**`), and the two existing
> ground-slot units were WIDENED rather than duplicated, so ZM stays at **3462**
> (`3462 ran / 3460 passed / 0 failed`, 2 skipped, `Null_vs2022_Debug_Win64_True`) and
> the registry stays at **70**.
>
> Splat slot 1 of all three outdoor recipes — Dawnmere's `Stone`, Thornacre's
> `Drystone`, Route 1's `Chalk` — now samples `engine:Textures/Terrain/Rock/` at
> tiling 0.9 with a **WHITE** base colour, exactly as slot 0 has sampled the shared
> grass set since `3aeaa2d4`. That commit moved BOTH sets into the engine tree; only
> the grass half ever gained a second consumer, so the rock half was a shared asset
> with one game in it. This overrides "R1-1 ruling 8: slot 1 stays flat".
>
> ★ **The base colour is the trap, not the texture ref.** `Flux_Terrain_ToGBuffer ->
> SampleDiffuseWithBaseColor` MULTIPLIES base colour into the sampled diffuse, so
> leaving `Stone` at 0.34/0.36/0.33 would have dimmed the rock maps to a third and read
> as a rendering bug rather than as a data error.
>
> **THE THREE OUTDOOR `.zscen` FILES MOVED** — see the hash table below. Each grew by
> exactly **164 bytes** with its entity count unchanged: four texture-ref paths on one
> material payload. **A `Null_` boot cannot re-author them** —
> `ZM_DetermineTerrainBakeQueueResult` returns `HEADLESS` before it inspects a manifest,
> so `sceneAuthoring=DEFERRED` and only the four interiors compare. That is ZM's terrain
> bake being `needs-gpu`, NOT the engine publish guard, which ZEN-6 already removed.

> **★ PER-INSTANCE COLLIDERS FOR INSTANCED MESHES — the engine change, OBSERVED 2026-08-27.**
> **ALL THREE BOOT PINS MOVED +24**, which is the shape an ENGINE change has: the 24 new
> units are backend-neutral (19 `InstancedMesh`, 1 `Maths`, 3 `Physics`, 1 `Core`), so
> every game that boots the engine suite counts them. ZM **3438 -> 3462**
> (`3462 ran / 3460 passed / 0 failed`, 2 skipped), Combat **1669 -> 1693**, RenderTest
> **1760 -> 1784** — each OBSERVED on its own `Null_vs2022_Debug_Win64_True` exe, none
> inferred by arithmetic from another. The registry is untouched at **70**: no Zenithmon
> GAME component was added.
>
> `Zenith_InstancedMeshComponent` can now declare a per-instance collider, and a group
> that does gets one STATIC Jolt capsule per live instance, owned by the component rather
> than by an entity apiece. Nothing in Zenithmon enables it — the config defaults to NONE
> and every hook early-returns on a single enum compare — so no ZM scene, asset or
> behaviour changed. The consumer is RenderTest's 2520 painted tree trunks.
>
> ★ The pin bump is the whole of Zenithmon's exposure, and it is exactly the case this
> file exists for: a green ZM suite with a stale number here reds `doc_lint -Game
> Zenithmon` (C7) with zero failing tests.

> **★ THE THREE OUTDOOR MAPS SHRANK TO FIT THEIR CONTENT. OBSERVED 2026-08-27.**
> **NO PIN MOVED**, and that is the headline: this is a Zenithmon-only change
> (nothing under `Zenith/**`), it adds no unit and retires none, so ZM stays at
> **3438** (`3438 ran / 3436 passed / 0 failed`, 2 skipped, measured on
> `Null_vs2022_Debug_Win64_True`) and the registry stays at **70**. The engine and
> RenderTest rows are untouched for the same reason.
>
> Each map now carries its OWN grid instead of a 16-wide slice of a fixed 4096 m
> world, sized to what is authored on it:
>
> | Map | Was | Now | Chunks | Content translated by |
> |---|---|---|---|---|
> | Dawnmere | 16x16 | **9x10** (576 x 640 m) | 256 -> **90** | (-232, -320) |
> | Thornacre | 16x16 | **13x15** (832 x 960 m) | 256 -> **195** | (-100, -16) |
> | Route 1 | 16x24 | **11x24** (704 x 1536 m) | 384 -> **264** | (-184, 0), X only |
>
> **896 -> 549 baked chunks, a 39% cut in the terrain asset tree** (2,706 -> 1,665
> files). Route 1 shrinks in X only because its LENGTH is the point of it; every
> waypoint's distance along the route is exactly what it was.
>
> ★ **THE OFFSETS ARE MULTIPLES OF 4 ON PURPOSE.** The physics mesh is built at
> density divisor 4, so an anchor whose X and Z are both multiples of 4 is a
> SHARED VERTEX of the render and physics meshes and samples exactly rather than
> by interpolation — the property `ZM_DawnmerePlacement.h`'s ZM-D-186 note relies
> on. -230 would have put every Dawnmere anchor mid-quad; -232 keeps 512/384/640
> on 280/152/408, at a cost of two metres of empty edge.
>
> ★ **THE LANDSCAPES WERE RE-AUTHORED, NOT TRANSLATED.** Every hill was sized to
> fill a 1024 m square (radii 160-190, one of them spelled "850 + 174 = 1024" to
> touch the east boundary), so translating them would have put five of six centres
> outside the new worlds. They are re-placed and re-scaled to frame each town at
> the new size, and the erosion radii scale with the axis that shrank so
> "region-only" means the same thing at three scales. **A dab must fit ENTIRELY
> inside its terrain** — `AssertTerrainAuthoringPlanContained` checks centre +/-
> radius — which is why no ridge hugs an edge.
>
> ★ **EVERY MEASURED HEIGHT IN ALL THREE MAPS WAS RE-MEASURED** from its own
> oracle against the new bake (the Dawnmere roster, Home, Lab and both route-seam
> rows; Thornacre's two; Route 1's nine). Dawnmere's ground changed CHARACTER, not
> just value: every column now reads within a few tenths of the 24 m flatten
> target and two read BELOW it, where the old map had unflattened columns sitting
> ~+2 m up on erosion deposit. `Dawnmere.znavmesh` re-baked with it (4,225 -> 1,517
> polygons at the same 16 m cell size — the domain moved, not the resolution).
>
> ★ **ONE TEST IS LEFT RED AND IT IS NOT A COORDINATE.**
> `ZM_InteriorTintPixels_Test` reads the two interiors' floor red/blue ratios as
> 1.1888 (PlayerHome) and 1.0684 (ProfLab), a gap of **0.1204** against its 0.15
> floor. The gap is POSITIVE and in the asserted direction, deterministic across
> runs, and neither interior scene's bytes changed here (both report
> `[ScenePublish] IDENTICAL` on every boot). The clause's own instructions forbid
> shrinking the floor to make it pass — "if the two bands OVERLAP, RECORD THAT" —
> and ZM-56 marks the judgement `needs-human`, so it is recorded here rather than
> adjusted. See the R1 notes below.

> **★ CONFIGURABLE TERRAIN DIMENSIONS — the engine change, OBSERVED 2026-08-26.**
> An ENGINE ticket whose units are backend-neutral, so **all three rows moved by
> +18 and all three were MEASURED**: ZM **3420 -> 3438** (`3438 ran / 3436 passed /
> 0 failed`, 2 skipped), Combat **1651 -> 1669** (`1669 / 1668 / 0`, 1 skipped),
> Null RenderTest **1742 -> 1760** (`1760 / 1759 / 0`, 1 skipped). CityBuilder,
> TilePuzzle and DevilsPlayground moved with them (`1670` / `1669` / `1670`).
> Registry UNCHANGED at **70**.
>
> Chunk world size, vertex spacing and grid extent are per-terrain
> (`Zenith_TerrainDimensions`, `Zenith/Core/`) rather than compile-time constants,
> and a baked set now carries a REQUIRED `TerrainDims.zdata` manifest recording
> the dimensions its chunks were quantised against. **Zenithmon still bakes every
> recipe at the DEFAULT dimensions**, so the chunk bytes are unchanged; what moved
> is `uZM_TERRAIN_MANIFEST_VERSION` **4 -> 5** (a v4 set has no manifest and the
> loader refuses it as stale) and the three required-output counts by exactly one
> each — Dawnmere/Thornacre **771 -> 772**, Route 1 **1155 -> 1156** — for that one
> new file. All three sets were re-baked windowed
> (`--zm-force-terrain-bake`, 44.6 s / 56.4 s / 65.1 s, `result=SUCCESS`,
> `manifest finalized`).
>
> **All three terrain scenes were re-authored: +16 bytes each, and EXACTLY 16.**
> That is the component's v5 serialization tail (one float + three u_int) appended
> after the complete v4 payload and nothing else — the proof that the default
> dimension arithmetic is floating-point-IDENTICAL to the constants it replaced.
> The four non-terrain scenes (`FrontEnd`, `PlayerHome`, `Battle`, `ProfLab`) came
> back `[ScenePublish] IDENTICAL` in the same boot, and a third boot reported all
> SEVEN identical. Re-observe the committed-asset table below.

> **★ ZEN-6 — the headless `.zscen` publish guard is REMOVED — OBSERVED 2026-08-26.**
> An ENGINE ticket, and its unit is backend-neutral, so **all three rows moved by +1 and all
> three were MEASURED, none inferred**: ZM **3419 -> 3420** (`3420 ran / 3418 passed /
> 0 failed`, 2 skipped), Combat **1650 -> 1651** (`1651 / 1650 / 0`), Null RenderTest
> **1741 -> 1742** (`1742 / 1741 / 0`, 1 skipped). Registry UNCHANGED at **70**.
> `Zenith_TerrainEditor::EnsureTreeEntities` no longer refuses to run on the Null backend,
> so a headless tools boot now authors an ENTITY-COMPLETE world and
> `Zenith_Editor::SaveActiveScene` no longer needs to refuse a save that would change a
> committed asset.
> **What this means for Zenithmon: a scene-authoring ticket no longer needs `needs-gpu`
> merely because it re-authors a committed `.zscen`.** That was the reason six of the ten
> tickets on the S8 critical path sat behind the label.
> ★ **PROVEN, not assumed.** A `Null_*_True` boot re-authored all four ZM scenes and
> reported `[ScenePublish] IDENTICAL` for every one — `FrontEnd`, `PlayerHome`, `Battle`
> and `ProfLab` — with `git status` clean afterwards and ZERO `CHANGED` verdicts. The same
> boot on RenderTest reproduced its committed scene at **82 entities / 361,753 bytes**,
> byte-for-byte, which is the whole file including the two instanced-tree entities whose
> loss (~323 KB of 361 KB) is why the guard existed.
> ★ Note the FIRST attempt at this proof was worthless and looked identical to success: the
> unit-gate boot exits before the editor-automation queue drains, so it authors nothing and
> leaves a clean tree. The proof requires `--automated-test <T> --skip-unit-tests`.

> **★ ZM-70 / G1-3 — Bloom Badge + Verdant Lash on a leader win — OBSERVED 2026-08-26.**
> `Null_vs2022_Debug_Win64_True` reported **`3419 ran / 3417 passed / 0 failed`** (2 skipped),
> so `Tools/unit_baselines.json` moved `Zenithmon` **3415 -> 3419**. The +4 are the reward
> units in `Tests/ZM_Tests_Party.cpp`: the production Fenna case, the by-ROW mechanism driven
> by a non-Fenna fixture (with its fail-closed loss arm), repeat-win idempotency for BOTH the
> badge and the item, and a save/load ROUND TRIP that reads back off a freshly-decoded
> `ZM_GameState` rather than off the mutated original. The existing Vesper reward test was STRENGTHENED with badge/item
> neutrality rather than duplicated, so it moves no count.
> Combat (`1650`) and Null RenderTest (`1741`) are UNCHANGED and were NOT re-measured: the
> diff touches no engine file. Stated as INFERRED, not observed.
> Registry UNCHANGED at **70** — every new unit is a boot unit and no
> `ZENITH_AUTOMATED_TEST_REGISTER` call site was added.
> **This slice authored nothing:** no `.zscen` byte moved, no save-schema change, and no new
> `ZM_GameState` state — the badge goes through the shipped `AwardBadge` mask (module 5) and
> the TM through `ZM_Bag::Add` (module 6).

> **★ ZM-28 / G1-1 — Gym 1 DATA + placement header — OBSERVED 2026-08-25.**
> `Null_vs2022_Debug_Win64_True` reported **`3415 ran / 3413 passed / 0 failed`** (2 skipped),
> so `Tools/unit_baselines.json` moved `Zenithmon` **3401 -> 3415**. The +14 is 13 units in
> the new `Tests/ZM_Tests_Gym1Placement.cpp` (the maze solvability BFS and its three
> anti-vacuity arms, the shell/hedge/camera/exit/leader geometry, accessor totality, the
> badge table, and one cross-table unit that the leader, the badge and the teach-move all
> describe the same gym) plus `Fenna_AuthoredValuesAreExact` in `ZM_Tests_TrainerData.cpp`.
> Combat (`1650`) and Null RenderTest (`1741`) are UNCHANGED and were NOT re-measured: the
> diff touches no engine file, registers no component and renames no component type, so
> nothing backend-neutral moved. Stated as INFERRED, not observed.
> The registry is UNCHANGED at **70** — every new unit is a boot unit, and this slice adds
> no `ZENITH_AUTOMATED_TEST_REGISTER` call site.
> Three pinned counts moved with the data and are part of the +14's context rather than of
> its arithmetic: `uEXPECTED_TRAINERS` 2 -> 3, `uEXPECTED_ITEMS` 90 -> 91, `uEXPECTED_MOVES`
> 218 -> 219. Each is spelled in its TEST rather than read off the table, on purpose, so a
> roster change is a failure rather than a silently-agreeing tautology.
> **This slice authored nothing:** no `.zscen` byte moved, and the committed-asset hash
> table below is untouched.

> **★ ZM-67 — ground-item props to AAA quality — OBSERVED 2026-08-25.**
> `Null_vs2022_Debug_Win64_True` reported **`3401 ran / 3399 passed / 0 failed`** (2 skipped),
> so `Tools/unit_baselines.json` moved `Zenithmon` **3399 -> 3401**. The +2 is
> `PropGen_GroundItemPickupRowsAreCentreAnchoredAndDistinct` (every roster row's anchor
> measured both ways round, the ITEM volume bound, and pairwise mesh/texture/palette
> distinctness) and `Presentation_ModelChoiceIsTotalAndCollapsesWhenNotTakeable`
> (totality of `ZM_GroundItemPropModel`, the collapse to the spent model, and agreement
> with the item table's own `ZM_ITEM_CATEGORY`).
> Combat (`1650`) and Null RenderTest (`1741`) are UNCHANGED and were NOT re-measured:
> the diff touches no engine file, registers no component and renames no component type,
> so nothing backend-neutral moved. Stated as inferred, not observed.
> Neither new unit asserts a colour distance chosen by whoever picked the colours:
> `PropGen_…Distinct` **measures and logs** the achieved palette separations and asserts
> only inequality and structure, and `Presentation_ModelChoiceIsTotal…` logs the id →
> model mapping and asserts totality and collapse, which are structural equalities.
> The registry moved **69 -> 70** with `ZM_GroundItemPropCapture_Test`, the windowed
> before/after capture harness this ticket's visual sign-off is judged from. It is
> `m_bRequiresGraphics = true`, so it is SKIPPED-AS-PASSED on the Null backend and CI
> can never see it rot — which is why every clause in it is a hard failure with a
> diagnostic rather than a log line.

> **★ ZM-27 follow-ups (a) + (b) — ZM-D-207 — OBSERVED 2026-08-24.**
> `Null_vs2022_Debug_Win64_True` reported **`3399 ran / 3397 passed / 0 failed`** (2 skipped),
> so `Tools/unit_baselines.json` moved `Zenithmon` **3395 -> 3399**. The +4 is three
> Route 1 prop placement units (`Route1_GroundItemPropAnchorsCoverTheWholeRegistry`,
> `Route1_GroundItemPropsAreReachableFromTheWalkedLane`,
> `Route1_GroundItemPropsStandOnTheirOwnMeasuredColumn`) plus the committed-bytes needle
> `Route1CarriesTheThreeGroundItemProps`.
> The registry moved **68 -> 69** with `ZM_GroundItemProp_Test`, COUNTED FROM THE RUN
> (`zenith test Zenithmon --headless` reported "69 tests measured", exit 0) and not from a grep.
>
> Engine `1650` and Null RenderTest `1741` are UNMOVED and were **inferred, not measured** —
> `git status` confirms the diff is confined to `Games/Zenithmon/**` plus the unprotected pin
> file, so no backend-neutral engine unit moved.
>
> **★ WHAT THIS SLICE ACTUALLY CLOSED.** ZM-27 shipped the ground-item mechanism with
> `ZM_TryPickUpGroundItem` having **zero production callers** — its first Definition-of-Done
> line ("a world prop can be picked up into the bag") was unmet and the ticket parked at
> *In Review*. It is met now: `ZM_GroundItemProp` at ECS order 115, three measured anchors,
> three prop entities in the committed `Route1.zscen`, and `ZM_GroundItemProp_Test` driving a
> real pickup. **The scope had been cut against a constraint that did not exist** — see
> ZM-D-207, and the corrected `windowed` / `needs-gpu` text in `AgentBriefing.md` and
> `Board.md`.

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
> **★ AND A CORRECTION, because the first version of this block got it wrong.** It claimed
> `Tools/doc_lint.ps1`'s C1 shared the same flaw and read 71. **It does not.** C1's regex is
> `ZENITH_AUTOMATED_TEST_REGISTER\s*\(` — it requires the OPENING PAREN, which the three
> comment mentions do not have — so C1 has always read the correct **68**. The inflated count
> was the bare `grep`, and attributing it to C1 was exactly the mistake this block warns
> against: asserting a number without checking the thing that produces it.
>
> What WAS true of C1: it only fails a doc that OVERSTATES (`$claimed -gt $registerCount`),
> and understating is the direction drift always travels, since every new test raises the true
> count while the prose stays put. And `registry **N**` matched none of its regexes. **Both are
> now closed by `doc_lint` check C7** (2026-08-24), which reconciles this LIVE PIN block against
> `Tools/unit_baselines.json` and against the registry count, with EQUALITY in both directions.
> It is scoped to this block on purpose — the history chain below is legitimately full of
> superseded numbers.

> **★ ZM-65 / ZM-D-206 — ZM-D-203 §5's DEVIATION IS CLOSED. OBSERVED 2026-08-24.**
> `DawnmereNorthGate` now has its own measured route-seam row instead of borrowing the
> `FromRoute1` arrival column 12 m south. **Measured `24.29772`** —
> `ZM_DawnmereRouteSeamGroundTruth_Test` against a warm Dawnmere bake:
> `name=DawnmereNorthGate paste=24.29772f xz=(512.000, 876.000)`, `hitTerrain=1`,
> `finalHit='DawnmereTerrain'`, `resolved=1`, `playerPresent=1` and correctly ignored.
> The same run re-read row 0 at `tableError=0.00000`, so the probe agreed with the
> 2026-08-15 freeze on the column that had not moved — which is what makes this a
> measurement of the gate column rather than of a drifted probe.
> `Dawnmere.zscen` re-authored, **±0 bytes** (a float moved in place); two consecutive
> windowed boots, second byte-identical, oracle PASSED on both. `Route1.zscen` and
> `Thornacre.zscen` byte-identical in the same boots.
>
> **★★ THE NUMBER ARGUES AGAINST THE TICKET'S OWN PREMISE, AND FOR ZM-D-203 DECISION 1.**
> The ticket reasoned from the other regions' 12 m deltas (Thornacre 0.254, Route 1 south
> 0.475, Route 1 north 0.962) that Dawnmere's gate must be materially mis-seated, and
> ZM-D-206 predicted **−0.37 m**. The truth is **−0.068 m** — right in sign, wrong by 5×,
> and **inside the oracle's own 0.150 m tolerance**. So a derived row would have PASSED the
> ground-truth check had one ever pointed at this column: the deviation was never
> detectable by watching the value, only as a MISSING ROW. That is exactly Decision 1's
> claim that a rule living only in prose is what lets a seam ship half-built, and it is the
> real answer to "was this worth a ticket rather than a comment" — the cost was not the
> error, it was that nothing in the repo could have told you the error was small.
>
> **★ DO NOT GENERALISE 0.068 INTO "SEAM PAIRS ARE CLOSE" — AND DO NOT EXPLAIN IT EITHER.**
> The rule stands on measurement, not on a mechanism. An earlier draft of this block claimed
> the other regions' pairs "differ in flatten CONTAINMENT" while this one does not; **that was
> fabricated and the recipe data inverts it**, as the ZM-65 review found. Thornacre's pair sits
> inside the `RouteGate` pad `{512,96} r=30` (`ZM_ThornacrePlacement.h` says so in as many
> words), and both Route 1 pairs sit inside their own r=30 gate pads *and* the `DirtLane`
> corridor — all three comparison pairs are **identically** contained on both columns. The
> Dawnmere pair is the only one whose columns differ in containment at all, and it moved the
> **least**. The honest statement: four measured 12 m seam pairs span 0.068–0.962 m and nothing
> in this repo accounts for the spread.

> **★ R1-4 COMPLETE (ZM-66 / ZM-D-205) — OBSERVED 2026-08-24.**
> `Null_vs2022_Debug_Win64_True` reported **`3395 ran / 3393 passed / 0 failed`** (2 skipped), so
> `Tools/unit_baselines.json` moved `Zenithmon` **3394 -> 3395**. The +1 is
> `ZM_CommittedSceneBytes/Route1CarriesTheTallGrassSystemAndThornacreAndDawnmereDoNot`.
> **Registry UNMOVED at 68** — no automated test added. Engine `1650` and Null RenderTest `1741`
> UNMOVED and inferred, not measured.
>
> **★★ THE RESULT THAT MATTERED IS WHAT DID *NOT* MOVE.** `ZM_TallGrassSystem` was attached at
> Route 1's **call site**, not inside `ZM_QueueTerrainHostEntity` — the helper Route 1 and
> **Thornacre** share. The obvious implementation would have given Thornacre tall grass too,
> changing a scene ZM-D-196 rules a deliberate TRAVERSAL STUB, and it would still have looked
> correct. Two windowed boots returned `Thornacre.zscen` and `Dawnmere.zscen` BYTE-IDENTICAL with
> exactly one asset modified. That is the proof, and the new committed-bytes clause is what keeps
> it true: it asserts the type name occurs **once** in Route1 and **zero** times in the other two.

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
id-tag self-check, and three synthetic roll-seam units) -> **3395**/1650/1741 (ZM-66, +1: the
Route1-carries-TallGrassSystem committed-bytes clause). Each number OBSERVED on `Null_`.

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
| `Route1.zscen` | 3,146 | `F3E9A4613E8942F17E80A2AB6BD699F22B97E71CBB2AFC317D796DDF75795110` (**RE-AUTHORED for the shared CLAY ground set, 2026-08-28, ZM-D-216**: splat slot 2 (`Dirt` -- the lanes and pads) stopped being a flat brown and now samples `engine:Textures/Terrain/Clay/` at tiling **3.6**, not 0.9, with a WHITE base colour. **Entity count UNCHANGED; the file grew by exactly 164 bytes** (2,982 -> 3,146) -- the same signature the rock slot produced one day earlier: four texture-ref paths appended to ONE material payload and nothing else. Two windowed `Vulkan_vs2022_Debug_Win64_True` boots with `--skip-unit-tests` (`--automated-test ZM_Boot_Test`, so the process exits on its own), all three recipes warm (`warmMask=0x7`, `sceneAuthoring=AUTHOR_DAWNMERE`); the second reported `[ScenePublish] IDENTICAL` for all seven scenes. Previous value `3BE837CBEB0CA5E0...` at 2,982 bytes, held from the rock-set re-author.) |
| `Thornacre.zscen` | 2,267 | `878C9F02E95476974F44EFDE6B2E34D3FC82911F6C15BFEE8B5F88234A213297` (**RE-AUTHORED for the shared CLAY ground set, 2026-08-28, ZM-D-216**: splat slot 2 (`Dirt` -- the lanes and pads) stopped being a flat brown and now samples `engine:Textures/Terrain/Clay/` at tiling **3.6**, not 0.9, with a WHITE base colour. **Entity count UNCHANGED; the file grew by exactly 164 bytes** (2,103 -> 2,267) -- the same signature the rock slot produced one day earlier: four texture-ref paths appended to ONE material payload and nothing else. Two windowed `Vulkan_vs2022_Debug_Win64_True` boots with `--skip-unit-tests` (`--automated-test ZM_Boot_Test`, so the process exits on its own), all three recipes warm (`warmMask=0x7`, `sceneAuthoring=AUTHOR_DAWNMERE`); the second reported `[ScenePublish] IDENTICAL` for all seven scenes. Previous value `094BADF64CF6FB95...` at 2,103 bytes, held from the rock-set re-author.) |
| `Dawnmere.zscen` | 79,066 | `FC85CA58B14AA38B2A5834EBA614ED0C612B2CF1C4743E07F303C23458BFB2D2` (**RE-AUTHORED for the BUILDING FACADE OVERHAUL, 2026-08-30**: the Home and Lab greybox shells became real generated buildings. The four blockouts per building keep their exact authored centres, scales and AABB colliders and simply lose `ZM_GreyboxVisual`; two new visual-only entities, `DawnmereHomeFacade` and `DawnmereLabFacade`, carry a `ZM_BuildingFacade` component that loads the generated multi-surface `.zmodel` at runtime. **40 entities, up 2 from v8; the file MOVED BY ONE BYTE** (79,067 -> 79,066) because eight dropped component payloads very nearly pay for two new entities. NO measured ground row, camera-clearance clause, door trigger or pad was re-derived, and none needed to be: the physics blockouts did not move. Proven byte-identical across two consecutive windowed `Vulkan_vs2022_Debug_Win64_True` boots. Previous value `A666F5DF3ADC9AF0...` at 79,067 bytes, held from the v8 compaction.) |
| `Battle.zscen` | 4,965 | `1BEB0615F7FE62D9439471A4123E1D2140C0053AEC2991B659F7A03288C8C60A` (unchanged since 2026-08-05) |
| `FrontEnd.zscen` | 29,740 | `D44D540512F1C373A5D5E747CE7FA76E7D19B467F5F1563EB298E229EEFBEDB5` |
| `PlayerHome.zscen` | 3,000 | `C50470EF5A1FEAF01EB785D9C9466E199BA3FB917F555780AE51D1D4C92F6875` (**RE-AUTHORED for the INTERIOR overhaul, 2026-08-30**: the seven shell blocks keep their exact centres, scales and colliders and lose `ZM_GreyboxVisual`; a `PlayerHomeShell` entity carries the generated four-surface room model, five pieces of furniture stand in it -- SOLID, each with its own AABB static body -- and three warm point lights light it. **11 -> 20 entities, 1,832 -> 3,000 bytes.** Proven byte-identical across two consecutive tools boots. Previous value `DBBFB78311A55BBF...` at 1,832 bytes, held since 2026-08-05.) |
| `ProfLab.zscen` | 3,542 | `8800FCB1E236354E9E4A44BFA4770E4928E288B055CA2794198749471689A6B0` (**RE-AUTHORED for the INTERIOR overhaul, 2026-08-30**: same split as PlayerHome -- collider-only blocks, a `ProfLabShell` entity with the generated room model, six pieces of laboratory furniture (solid) and four cool point lights. **12 -> 23 entities, 2,068 -> 3,542 bytes.** Proven byte-identical across two consecutive tools boots.) |
| `Dawnmere.znavmesh` | 25,892 | `5E725579C3B38197596976BF883235B01A334F8294433CEE5E8BC9A67671FFD8` (**RE-BAKED at the v8 compaction, 2026-08-30, ZM-D-218**: 324 vertices / 289 polygons over a 256 x 256 m sheet. The bake cell size is UNCHANGED -- the polygon count fell because the SHEET did, which is the whole point of the compaction.) |

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

**S8 ITEM 2 ("Route 1 -> town 2") IS IN PROGRESS. SLICES R1-1, R1-2, R1-3 AND
R1-4 ARE COMPLETE (ZM-D-197; ZM-D-198/199/202; ZM-D-203; ZM-D-204 + ZM-66/ZM-D-205).
R1-4's scene-attach half closed at ZM-66/ZM-D-205: `ZM_TallGrassSystem` is now
authored onto Route 1's terrain entity ONLY (Zenithmon.cpp, mirroring the
`Terrain`/`ZM_TerrainGrass` block; Thornacre and Dawnmere are untouched), proven
by a new committed-bytes needle in `Tests/ZM_Tests_CommittedSceneBytes.cpp`. The
windowed re-author of `Route1.zscen` and the observed pin/hash update are this
commit's mechanical follow-up -- see the callout below. THE NEXT TASK is R1-5
(trainer DATA + placement + the `Npc_ExactlyOneRowNamesARegisteredTrainer`
rewrite).**

> **★ R1-3's SOURCE AND ITS BYTES MUST LAND TOGETHER.** The four gates are authoring
> STEPS in `Zenithmon.cpp`; the committed `.zscen` bytes only move when a **windowed
> `Vulkan_vs2022_Debug_Win64_True`** tools boot re-writes them (Dawnmere additionally
> needs `sceneAuthoring=AUTHOR_DAWNMERE`), and that boot must carry
> `--skip-unit-tests` -- the new committed-bytes clauses are RED until it runs, and a
> failing boot unit aborts the boot **before** scene authoring, so without the flag they
> block their own fix forever. **Re-observe the three `.zscen` hashes in the
> committed-asset table above in the same commit.** All three scenes move: Dawnmere
> gains one entity record, Route 1 two, Thornacre one.

> **★ ZM-66/ZM-D-205's SOURCE AND ITS BYTES MUST LAND TOGETHER TOO.** The scene-attach
> step is one authoring line (`AddStep_AddComponent("ZM_TallGrassSystem")`) appended
> immediately after Route 1's `ZM_QueueTerrainHostEntity(...)` call and before the next
> `AddStep_CreateEntity`, landing it on the terrain host entity after `ZM_TerrainGrass`
> (ZM-D-148 append-only ordering) -- **Route 1 ONLY**, never inside the shared helper
> (which would also move Thornacre, a traversal STUB by ZM-D-196) and never inside
> Dawnmere's inline block. The committed `Route1.zscen` bytes only move once a windowed
> `Vulkan_vs2022_Debug_Win64_True` tools boot re-writes them; `Thornacre.zscen` and
> `Dawnmere.zscen` must come back byte-identical -- the new committed-bytes clause
> (`Route1CarriesTheTallGrassSystemAndThornacreAndDawnmereDoNot`) asserts exactly that
> split and is RED BY DESIGN until the re-author runs (carry `--skip-unit-tests`, same
> reason as every other R1-2/R1-3 needle above). **Re-observe Route1.zscen's row in the
> committed-asset table above (bytes + SHA256) and the LIVE PIN line in the same commit
> -- both are left as the orchestrator's OBSERVED numbers, deliberately not guessed here.**

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
2. **`ZM_TallGrassSystem` is registered (ECS order 109) and, as of ZM-66/ZM-D-205, authored onto
   Route 1's terrain entity** (Zenithmon.cpp, Route 1 ONLY -- Thornacre and Dawnmere are
   untouched, ZM-D-196). The battle plumbing existed with no producer since S5; Route 1 is where
   it comes alive, once the windowed re-author moves `Route1.zscen`'s committed bytes.
3. **Route 1 CANNOT be terrain-free.** `WorldSpec_TerrainByKind` reds any ROUTE/TOWN row with an
   empty terrain set, and the encounter loop is grass-density driven. Authoring needs a
   **WINDOWED `Vulkan_*_True` boot with `--skip-unit-tests`** and all three recipes warm.

### The ten slices (four need a WINDOWED authoring boot)

> **★ THE `deps` COLUMN IS A MIRROR, NOT THE AUTHORITY.** `Roadmap.md` records
> that this column is exactly what the board's `BLOCKS` links replaced — *"the
> R1-x chain used to be a `deps` column in a table inside `Status.md`, which
> nothing could enforce, so the loop was free to claim R1-6 before R1-5 existed"*.
> The links are what the claim query actually reads; this column is kept only
> because the table is how a human reads the slice in one go. **Read
> `zagent blocked --project ZM` for the real graph**, and if the two ever
> disagree, the links win and this column is stale. Editing this column changes
> nothing; `zagent link … --reason "…"` and `zagent unlink` are the gestures that
> do. Every edge now carries its reasoning, audited 2026-08-25.

| id | title | +units | re-authors | deps (mirror — see above) |
|---|---|---|---|---|
| ~~R1-1~~ | ~~Placement headers + per-recipe terrain materials (PURE)~~ **DONE, +17 (not ~12), ZM-D-197** | 17 | none | -- |
| R1-2 **ph.1** | ~~Per-recipe terrain materials (split out of R1-2, PURE)~~ **DONE, ZM-D-198** | 0 | none, PROVEN | R1-1 |
| R1-2 | Author Route1 + Thornacre -- **MARKERS ONLY, zero triggers** | ~7 | creates Route1+Thornacre, re-authors Dawnmere -- **WINDOWED** | R1-1 |
| ~~R1-3~~ | ~~**All four seam triggers in ONE commit** + round-trip proof~~ **DONE, +1 (not ~4), ZM-21/ZM-D-203 -- closes critic blocker #2** | 1 | all three, DONE -- **WINDOWED**, two boots, second byte-identical | R1-2 |
| R1-4 | Wild encounters live + rate retune (ruling 4) -- **DONE: ZM-22/ZM-D-204 (rate + id-tag + synthetic roll-seam proof) + ZM-66/ZM-D-205 (scene-attach: `ZM_TallGrassSystem` authored onto Route 1's terrain entity, Route 1 ONLY)** | ~2 | Route1 only -- **WINDOWED** (ZM-66) | R1-3 |
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
> Route 1 payloads with their own entity names. **#3 is now FULLY RESOLVED:** the biome
> table is id-tagged + self-checked and the roll seam has a headless synthetic-density-map proof
> (`Tests/ZM_Tests_RouteEncounterSeam.cpp`, ZM-22/ZM-D-204), and the SLICE GOAL #3 was written
> against ("encounters live on Route 1") is now source-complete too (ZM-66/ZM-D-205:
> `ZM_TallGrassSystem` authored onto Route 1's terrain entity; the windowed re-author is this
> commit's mechanical follow-up). #4 is still OPEN and belongs to R1-9.
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
   **CLOSED (ZM-66/ZM-D-205):** the CONTENT gap this line used to name is now source-complete --
   `ZM_TallGrassSystem` is authored onto Route 1's terrain entity, Route 1 ONLY (never inside the
   shared `ZM_QueueTerrainHostEntity` helper, which would also move Thornacre). See Decision 4 in
   ZM-D-204 for why the gap existed, ZM-D-205 for the closing reasoning, and the R1-4 row above.
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
  ALREADY-COMMITTED `Route1.zscen` (a CHANGE, not a create), which USED to be the class of work
  the headless publish guard kept out of a worker's reach — it needed a windowed
  `Vulkan_*_True` re-author. **★ NO LONGER TRUE as of ZEN-6 (2026-08-26): the guard is
  removed and a headless boot may change a committed scene.** Left in place because it
  records why the work stalled at the time.
  **CLOSED at the source level by ZM-66/ZM-D-205**: `AddStep_AddComponent("ZM_TallGrassSystem")`
  now exists at Route 1's authoring call site (Route 1 ONLY). What remains is exactly the
  mechanical windowed re-author + observed pin/hash this bullet already described -- this
  commit's own follow-up, not a new decision.

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
`ZM_Interaction/RouteSeamGround_EachRowStandsOnItsOwnAnchorAndIsMeasured` pins it; boot
3345 -> **3346**. (That unit was named `…StandsOnTheFromRoute1LandmarkAndIsMeasured` when this
block was written; **ZM-65 renamed it**, because a second row — `DawnmereNorthGate` — mirrors no
`FromRoute1` landmark and the old name asserted something false of it. The rename is recorded
here rather than left dangling: the historical claim above is unchanged and still true.)

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

* **`ZM-20` (R1-2 step 3) is DONE**, along with R1-3 and R1-4. The next task is
  **R1-5 (`ZM-23`)** — trainer DATA + placement + Npc claim-check rewrite.
* **`windowed` no longer exists.** It was split into `needs-gpu` and `needs-human`, and
  `ZM-20` carries `needs-gpu`. **That label gates NOTHING** — a GPU is assumed available
  and the loop claims such a ticket like any other; it says only that the deliverable
  needs a `Vulkan_*_True` build and a windowed run rather than the `--headless` `Null_`
  config. **★ ZEN-6 (2026-08-26) REMOVED the `.zscen` publish guard entirely**, so
  "it re-authors a committed scene" is no longer a reason to label anything `needs-gpu`;
  when this was written the guard was `if constexpr (Zenith_IsNullRenderer())` in
  `Zenith_Editor.cpp` and compiled OUT of a Vulkan build, so even then it never protected
  against an absent human. Read this line as *how to build*, not as
  *run it yourself*. The pin still comes from a `Null_` run: **Vulkan to author, Null to
  verify and pin.**
* **Everything after it is BLOCKED behind it**, mechanically:
  `ZM-20 → ZM-21 → ZM-22 → ZM-23 → ZM-24 → ZM-25 → ZM-26 → ZM-28 → ZM-29 → ZM-30`,
  and `ZM-30` (the S8 go/no-go) blocks all five S9 stories. `zagent blocked --project ZM`
  prints the graph; the loop refuses any ticket whose predecessor is unfinished.
* **A dirty tree blocks the loop entirely.** Its precondition check treats uncommitted
  changes as fatal rather than something to work around — it has to, since Zenithmon
  is `branching: "direct"` and commits in place. Leave `master` clean.
