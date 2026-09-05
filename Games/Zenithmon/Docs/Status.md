# Zenithmon Status

**Last updated:** 2026-09-01

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

**★ LIVE PIN (UPDATED 2026-09-05):
ZM boot `3730`; engine boot (Null Combat) `1875`; Null RenderTest `1976`; registry **72**.**

> All three OBSERVED from `Null_` runs after the humanoid rig became T-POSED and
> the `.zbind` sidecar was removed (`Tools/unit_baselines.json` is the authority
> and carries the same three numbers). The counts went DOWN, which is worth saying
> out loud because a gate asserting `ran == baseline` reds either way: the rebind
> path and its tests were deleted along with the `.zbind` parser's, and the
> orientation tests that replaced them are fewer.

> **★ +1 ZM ONLY (2026-09-04, third bump of the day) — THE FROZEN SAVE-MIGRATION
> OFFSET.** Zenithmon 3697 -> **3698**, OBSERVED from a `Null_` run; engine pins
> UNMOVED (this one is game-side) and registry unchanged at 72.
>
> `HistoricalBodyHalfHeightIsFrozenIndependentlyOfGameplayTuning` asserts a LITERAL
> `0.9` for `ZM_SaveSchema::fHISTORICAL_BODY_HALF_HEIGHT` and deliberately does NOT
> compare it against `fZM_HUMAN_BODY_HALF_HEIGHT`. The two are equal today and are
> different FACTS: one is how tall a human is now, the other is the centre-to-feet
> distance the v1/v2 save writers actually used, fixed forever. A test comparing them
> would pass through exactly the character retune it exists to catch, because both
> sides would move together.
>
> The ZM-D-223 coordinate migration also moved onto a FROZEN canned blob
> (`auV2ResumeGolden`, 842 bytes) per SaveFormat.md's binding policy — the previous
> fixture aged a freshly written payload, which proves only that the reader agrees
> with the writer.

> **★ +4 ZM / +3 EVERY GAME (2026-09-04, second bump of the day) — THE REVIEW FIXES
> FOR ZM-D-223.** Zenithmon 3693 -> **3697**, Combat 1841 -> **1844**, RenderTest
> 1942 -> **1945**, each OBSERVED from its own `Null_` run; registry unchanged at 72.
>
> **+3 of it is ENGINE and therefore moves every game's row**, which is the whole
> reason all three numbers change together: `Zenith_ModelComponent.Tests.inl` is new
> and compiles into `zenith.lib`, so Combat and RenderTest boot it too. The fourth is
> Zenithmon's own `MigrationV2ToV3_BodyCentreWorldPositionBecomesFeet`.
>
> **What the four pin, and why they were missing.** Review found the model-space
> offset was dropped by BOTH of `Zenith_ModelComponent`'s move operations — and
> component pools relocate on growth, so a human that had already loaded reverted to
> a zero offset and rendered half underground, permanently: the offset is
> re-established only on a model LOAD, and `ZM_GreyboxVisual` early-returns while the
> model it loaded is still the one it wants. `ModelSpaceOffsetSurvivesPoolGrowth`
> drives real pool growth and ASSERTS the relocation happened (a run that never grew
> would otherwise pass having tested nothing); it was mutation-proved against the
> unfixed move constructor.

> **★ -3 ZENITHMON-ONLY (2026-09-04) — THE HUMAN RIG IS THE ENGINE'S NOW (ZM-D-223).**
> Zenithmon 3696 -> **3693**, OBSERVED from a `Null_` run. Engine pins UNMOVED
> (Combat 1841, RenderTest 1942) and registry unchanged at 72: the two engine
> additions this needed — `Zenith_ModelComponent::SetModelSpaceOffset` and
> `Zenith_ColliderComponent::SetExplicitShapeOffset` — carry no units of their own,
> and no automated test was added or removed.
>
> Net -3 = **six tests deleted, three added**. The six asserted a rig this game no
> longer owns: `HumanGen_SharedSkeletonWellFormed`,
> `HumanGen_PerModelBonesMatchShared`, `HumanGen_ClipChannelsMatchSharedSkeleton`,
> `HumanGen_ClipTimingAndPlaybackPolicy`, `HumanGen_ClipDeterminismAndSensitivity`
> and `HumanGen_BindSpaceCentreAnchored` — the last of which pinned the OPPOSITE of
> what now has to be true. They were deleted rather than weakened, because a test
> that asserts a property of an engine asset from a game that never touches it can
> only pass trivially: against v6's empty bone array, four of the six would have.
>
> The three added are the ones that can still fail:
> `HumanGen_RigMatchesStickFigure` opens the real
> `engine:Meshes/StickFigure/StickFigure.zskel` and pins that its first sixteen
> bones are still the sixteen this game's loft weights, at those sixteen indices —
> the load-bearing premise of the whole migration, and the one thing nothing else
> in this game would notice breaking. `HumanGen_BindSpaceIsRigSpace` measures the
> SHIPPED mesh rather than `ZM_MeasureHumanBody`'s own build, so a returning anchor
> pass cannot move both sides and agree with itself.
> `HumanGen_ModelOffsetPlacesFeetOnOrigin` states the placement equation end to end
> in metres. All three were mutation-checked before being trusted.
>
> Human bakes are stale on every tree: `uZM_HUMANGEN_VERSION` 5 -> 6 invalidates
> them, and `game:Humans/Shared/` is gone rather than rewritten.

> **★ +1 SHARED (2026-09-03, second bump of the day) — TEXTURE USAGE IS DECLARED,
> NOT INFERRED.** Zenithmon 3695 -> **3696**, Combat 1840 -> **1841**, RenderTest
> 1941 -> **1942**, each OBSERVED from its own `Null_` run; registry unchanged at
> 72 (no new automated test — `ZM_PhotoTour_Test` only gained a flag).
>
> Net +1 = **six new tests replacing five deleted ones**, in
> `Tools/Zenith_Tools_TextureExport.Tests.inl`, which compiles into `zenith.lib`
> for every game. The five deleted ones pinned a FILENAME HEURISTIC that chose a
> texture's compression and colour space from its basename. **All five passed
> throughout, on every case they covered, while the code they pinned broke the
> game**: the heuristic correctly re-encoded the terrain normal maps as BC5, the
> two terrain G-buffer shaders still decoded three channels, the terrain's shading
> normal pointed underground, `NdotL` went to zero and **every terrain pixel in
> Zenithmon stopped receiving shadows** — reported as "the Dawnmere houses stopped
> casting shadows onto the ground". A test over a heuristic measures the heuristic,
> not the system.
>
> The guess is deleted, not patched: usage is now DECLARED per texture in a
> committed `TextureUsage.ztexdecl` per asset root, an undeclared texture is not
> exported, and the six replacements pin the usage->format mapping, the token
> round-trip, and that an unlisted path comes back "no". Full account:
> `Docs/design/Photorealism.md` §1.10; contract:
> `Zenith/AssetHandling/CLAUDE.md`.

> **★ ALL THREE ROWS MOVE (+64 engine, 2026-09-03) — the photorealism program.**
> Every number OBSERVED from that game's own `Null_vs2022_Debug_Win64_True` run.
> The **+64 shared** rows are engine- and tools-side (texture colour space and the
> BC sRGB formats, coverage-preserving and sRGB-correct mip chains, BC1
> punch-through alpha, the terrain shadow-cull slot arithmetic and its on/off
> toggle, HDR exposure, and
> the generated leaf/bush/grass/rock/tennis asset suites, which compile into
> `zenith.lib` for every game). Combat 1776 -> **1840** and RenderTest 1867 ->
> **1941** (+74: the shared 64 plus 10 RenderTest-only tennis-court units).
> Zenithmon 3603 -> **3695** (+92: the shared 64 plus 28 of its own — building
> generator, interior generator, prop UV islands, texture synth).
> Registry 71 -> **72**: `ZM_PhotoTour_Test` (manual-only, graphics-required).
>
> ★ **THE TWO GRASS-TABLE UNITS THAT WERE RED ARE FIXED.**
> `FluxGrassTypeTable::SerializeRoundTripsExactly` and
> `ReadRejectsGarbageAndLeavesTableUntouched` asserted that a bindless texture
> SLOT round-trips through the file. It must not: `FluxGrassTextureSlot` says a
> type binds by asset PATH and "the slot number is a descriptor allocation that
> changes every boot, so it never reaches the file", and `ReadFromDataStream`
> resets it for exactly that reason. A file carrying the integer would point a
> type at whatever texture occupied that slot next run. The tests now assert the
> PATH round-trips and the slot comes back UNBOUND; the entry comparison stays a
> memcmp (so an unserialized new field still fails) with only those three
> runtime fields normalised out. **All three games: 0 failed.**
>
> Full record of the rendering work: `Docs/design/Photorealism.md`.

> **★ ALL THREE ROWS MOVE AGAIN (+13 each, 2026-09-01) — the editor delight pass.**
> Thirteen ENGINE units landed with the editor overhaul (`Zenith/Editor/
> Zenith_EditorCommands.Tests.inl`: entity-snapshot restore, undoable delete /
> duplicate / rename / reparent / component add-remove, the inspector undo tracker,
> `Record` + composite semantics, prefs round-trip + clamps, hierarchy / component
> search, the viewport axis projection, gizmo snapping and local-space axes). No
> Zenithmon code changed; every game boots the same engine suite, so the three rows
> move together. Observed on each game's own `Null_vs2022_Debug_Win64_True` run:
> ZM `3603 ran, 0 failed, 2 skipped`, Combat `1776 ran, 0 failed, 1 skipped`,
> RenderTest `1867 ran, 0 failed, 1 skipped`. The skips are pre-existing.
>
> **★ ALL THREE ROWS MOVE (2026-08-30), and that is the tell that this one touched
> `Zenith/**`.** The `.glb` import path is ENGINE-side (`Tools/` compiles into
> `zenith.lib` in every configuration), so its 7 units land in every game's boot
> suite: Combat +7 and RenderTest +7 with no game code changed at all. The other
> 13 are Zenithmon-only. Every number below is OBSERVED from its own
> `Null_vs2022_Debug_Win64_True` run, never arithmetic on the previous one.
>
> **ZM +20 — the hand-made bed.** `Assets/Props/Bed/Bed.glb` (a gltfpack export,
> `EXT_meshopt_compression` + `KHR_mesh_quantization`) replaces the generated bed
> in PlayerHome. Three pieces, three suites:
>
> * `MeshoptDecode` (**7**, engine-side, `Tools/Zenith_Tools_MeshoptDecode.Tests.inl`)
>   — **Assimp cannot open a gltfpack file at all**: it rejects the extension's
>   fallback buffer with *"buffer with non-zero length missing the uri attribute"*
>   before reading a vertex, so there was no degraded import to fall back on. The
>   vertex/index codecs and both vertex filters are pinned BYTE-FOR-BYTE against
>   upstream meshoptimizer's own output, because a plausible misreading of this
>   format decodes most of a mesh correctly and then diverges. That is not
>   hypothetical: the golden vectors caught an inverted sign in the OCTAHEDRAL
>   fold — the one path the bed itself does not use — which would have corrupted
>   half the shading of the next asset that did.
> * `ZM_PropFit` (**6**, `Tests/ZM_Tests_PropFit.cpp`) — the authoring now MEASURES
>   each prop's baked mesh and derives a uniform scale + ground lift from it, so a
>   model of any authored size lands at its roster size standing on the floor. The
>   bed arrives 1.00 x 0.38 x 0.72 m and origin-centred against a 2.0 x 1.2 x 0.7
>   row; authored at identity it was a half-size bed sunk to its mattress.
>   OBSERVED in-game: scale 2.0010, ground y 0.3806, **2.000 x 0.761 x 1.447 m**.
> * `ZM_FollowCameraCeiling` (**7**, `Tests/ZM_Tests_FollowCameraCeiling.cpp`) —
>   the boom put the lens ~3.9 m up in a room whose ceiling slab starts at 3.0 m,
>   so the player could not see inside their own house. **No raycast could have
>   found it**: the interior shell is visual-only, so the slab has no collider.
>   The camera resolves the room's height from the shell entity in its own scene
>   and slides the lens along the boom until it clears. OBSERVED live lens y
>   **2.650 m** against a 3.00 m ceiling.
>
> OBSERVED `3574 ran / 3572 passed / 0 failed`, 2 skipped; Combat `1755`,
> RenderTest `1846`.

> **★★ ZM +0 (2026-08-31) -- the SECOND import, and the pins did NOT move.** A
> reader expecting a bump should read this paragraph rather than assume a missed
> one. `Assets/Props/Table/Table.glb` went through the pipeline built for the bed
> with **no code change at all**: the importer found it, decoded it, and wrote the
> bundle; `ZM_ComputePropFit` measured it and the authoring scaled it. OBSERVED
> model **0.6382 x 0.5376 x 0.9995 m** -> scale **1.4007**, ground y **0.3765** ->
> **0.8939 x 0.7530 x 1.4000 m**, in BOTH rooms that stand one (`HomeTable` and
> `LabTable`), so `PlayerHome.zscen` and `ProfLab.zscen` both moved again.
>
> The unit count is unchanged because both test edits were REPLACEMENTS.
> `ZM_PropFit/ImportedBedIsScaledAndStoodOnTheFloor` became
> `ImportedAssetsAreScaledAndStoodOnTheFloor`, a table-driven unit over both
> imports -- **and it had to, because the bed-shaped assertion was FALSE for the
> table.** It read "the fitted WIDTH equals the roster width", which holds for the
> bed and does not for the table: the table's longest MODEL axis is Z while its
> longest ROSTER number is the 1.4 m width, so it fits on Z. The invariant that
> holds for both, and for whatever lands next, is LONGEST TO LONGEST.
>
> `ZM_BedShowcase_Test` likewise became **`ZM_ImportedPropShowcase_Test`** (file
> `Tests/ZM_AutoTests_ImportedPropShowcase.cpp`), driven by a subject roster
> rather than one hard-coded prop: both imports stand in PlayerHome, so a second
> test would have paid a second scene load and a second 600-frame room budget to
> photograph a prop already in the first one's frame. Seven shots now
> (player_view, player_view_unclamped, room_wide, and two per subject). Adding a
> row to `axIPS_SUBJECTS` is the whole cost of the next asset. Registry stays
> **71** -- a rename, not an addition.
>
> ★ **The table's dark top is the ASSET, not the pipeline, and that was MEASURED
> rather than reasoned.** It renders as a dark-stained top on light timber legs
> with a cool sheen, which looks at first like a metallic-map error: the table's
> metallic channel is a uniform **0.29** where the bed's is 0.018. Forcing the
> metallic factor to 0 changed nothing. Unbinding the roughness/metallic map
> entirely turned the top from dark NAVY to dark BROWN -- so the darkness is the
> albedo (a dark-stained top, which the UV atlas confirms) and the blue cast is
> the glossy ~0.33 roughness picking up the cool interior IBL. Both correct.
>
> ★ **`HomeLampTable` looked like it was blowing out `HomeChair`** -- a white
> block on the left of that day's `room_wide`. Left alone on the reasoning that
> the lamp is legitimately beside its furniture and the affected prop was a
> generated greybox, unlike `HomeLampBedside`, which sat INSIDE the bed's volume.
> **That call held: AB-PROP-03 landed the same day and the blowout went with it**
> -- it was a flat untextured box saturating, not a lamp too close. The same light
> now reads as a warm rim along the chair's slats. Recorded because the wrong
> lesson ("move the lamp") was one decision away.

> **★ ZM +0 (2026-08-31, later) -- the THIRD import, and again no pin moved.**
> `Assets/Props/Chair/Chair.glb` needed no code change and no new test: a row in
> `axIPS_SUBJECTS` and a row in `ZM_PropFit`'s asset table. OBSERVED model
> **0.6152 x 0.9980 x 0.5371 m** -> scale **1.0020**, ground y **0.5000** ->
> **0.6164 x 1.0000 x 0.5382 m**, in both rooms that stand one (`HomeChair`,
> `LabChair`).
>
> ★ **It fits on Y, and that completes a set worth keeping.** The bed fits on X,
> the table on Z and the chair on Y -- so the three imports between them cover
> every axis the "longest to longest" rule can pick. Any implementation that
> hard-codes an axis now fails two of three; a single-asset test would have let
> two through. `ZM_Tests_PropFit` says so where the table is declared.
>
> ★ **And it is the first delivery that was already the right size** -- scale
> 1.0020, a near-identity. The fit handles that as the no-op it is, which is the
> property that lets it run on every prop rather than only the mis-sized ones.
>
> OBSERVED `3574 ran / 3572 passed / 0 failed`, 2 skipped. Nine capture shots now
> (three fixed + two per subject). Registry stays **71**.

> **★★ ZM +0 (2026-08-31) -- NO INTERIOR PROP HAD EVER BEEN ROTATED, and a chair
> is what made that visible.** Asked whether the chair was meant to sit 90 degrees
> to the table, the answer turned out to be two stacked defects, neither of which
> could be seen without the other:
>
> 1. **The AABB collider was eating every furniture rotation.**
>    `Zenith_ColliderComponent` forces an AABB body to `JPH::Quat::sIdentity()`,
>    and the physics->transform sync writes that identity back over the authored
>    rotation **into the saved scene bytes** -- ZM-D-156, already paid for once on
>    rival Vesper. `HomeBed`, `HomeShelf`, `LabShelf` and both lab counters were
>    authored with a quarter turn and stood square to the room. MEASURED: the live
>    `HomeBed` transform read `(w 1.00000, y 0.00000)` where the dressing table
>    says `YAW90`. Fixed by authoring furniture as `COLLISION_VOLUME_TYPE_OBB` --
>    the same box, differing only in applying the rotation.
> 2. **The yaw constants named HALF the angle they applied.** The block defined
>    `(w, y) = (cos a, sin a)`; a quaternion is `(cos(a/2), axis*sin(a/2))`, so the
>    old `YAW90` was a HALF turn and the old `YAW45` was a QUARTER. Renamed to
>    `YAW90` / `YAW180` with the arithmetic spelled out, and the old `YAW45` is
>    gone rather than left as a third spelling of 90 degrees.
>
> ★ **Each hid the other.** A constant that applies twice its stated angle cannot
> be caught by looking at a room whose props are never rotated at all; and a
> discarded rotation cannot be caught while every prop is a symmetric greybox box
> whose yaw is unobservable. AB-PROP-03 is the first prop with a FRONT.
>
> `HomeChair` now faces the table (`YAW90`, model +X -> world -Z, MEASURED as
> `faces (0.000, 0.000, -1.000)`); `LabChair`, which stands at no table, faces the
> room. `HomeBed` finally runs ALONG the -X wall as its own placement comment
> always said it should -- world footprint `1.447 x 0.761 x 2.000` where it was
> `2.000 x 0.761 x 1.447`.
>
> ★ **And the harness stopped ignoring rotation, which is why any of this is
> visible.** It measured world SIZE and skipped the quaternion, so it would have
> photographed a prop facing the wrong way and reported nothing but a correct
> size. It now recovers the yaw FROM the quaternion -- not from the constant that
> produced it, which is the only reason the naming defect showed up -- and logs
> the rotated world footprint.
>
> OBSERVED `3574 ran / 3572 passed / 0 failed`, batch `71 passed / 0 failed`,
> both scenes re-authored and republishing `IDENTICAL` on the next boot -- which
> is itself the proof the rotation now ROUND-TRIPS instead of being written back
> over. `PlayerHome.zscen` and `ProfLab.zscen` are re-authored (both
> keep their entity count and byte count; two consecutive boots publish
> `IDENTICAL`). `HomeLampBedside` moved out of the bed's own volume — it sat 0.39 m
> above the mattress and saturated it — with intensity, range and colour untouched.

> **★★★ ZM +3 / Combat +3 / RenderTest +3 (2026-08-31) -- THE TANGENT FRAMES OF
> EVERY IMPORTED PROP WERE NaN, and removing the legacy asset readers that hid it.**
> Asked whether the normals were correct, the mesh normals and the normal MAPS
> both checked out; the TANGENTS did not. `Zenith_MeshAsset::GenerateTangents`
> rejected a triangle whose UV determinant was below `1e-4` -- a UV *AREA*, so
> really a cap on how finely a mesh may be unwrapped. On a 2048^2 atlas a
> 30x30-texel triangle is already at that cutoff.
>
> MEASURED before the fix: the cutoff skipped **57-90%** of each imported prop's
> triangles, and every vertex whose faces were all skipped kept a zero accumulated
> tangent that `glm::normalize` turned into NaN -- Bed **2065**/3606,
> Table **2666**/3515, Chair **1341**/3735, Shelf **5524**/7353. Predicted starved
> counts matched the NaN counts to the vertex.
>
> ★ **IT RENDERED, WHICH IS WHY IT SURVIVED.** `Flux_PackVertices` sanitises a
> non-finite direction to the semantic's canonical default, so those vertices
> silently took the WORLD-CONSTANT tangent `(1,0,0)` and their normal map was
> applied in a basis unrelated to the surface. Where the normal is near +/-X,
> `cross(N,T)` collapses entirely -- **625** such vertices on the Table alone.
> Nothing failed; the lighting was just wrong.
>
> ★ **GENERATED PROPS WERE NEVER AFFECTED**, which is why a latent bug of this size
> survived: their box faces have huge UV triangles, so 0 of 24-60 tripped the
> cutoff. The defect needed a finely-unwrapped mesh to appear, and the first one
> arrived with AB-PROP-01.
>
> ★★ **AND THERE WAS A SECOND NaN SITE, IN A DIFFERENT PRODUCER.** A repo-wide sweep
> of every renderable baked mesh found one more file carrying a NaN tangent -- a
> DevilsPlayground Blacksmith mesh, 1 vertex of 710 -- and deleting it did NOT fix
> it: it came back NaN with a perfectly finite normal. That mesh does not go through
> `GenerateTangents` at all. `Zenith_Tools_MeshExport` copies
> `aiProcess_CalcTangentSpace`'s output and called `glm::normalize` on it unchecked,
> so a vertex Assimp could not solve became NaN on the way to disk.
>
> Both producers now go through **one** function, `Zenith_MeshAsset::MakeValidTangent`
> -- unit, orthogonal to the normal, deterministic fallback -- because "what a valid
> tangent is" was being answered twice and got a different answer each time. The
> repo-wide scan is now **clean: no NaN tangents in any baked mesh on disk.**
>
> Fixed by rejecting only what the guard exists to reject -- a division that cannot
> produce a finite direction -- plus a deterministic orthonormal FALLBACK for a
> vertex with no usable parameterisation (still reached by 1 vertex of the Table
> and 14 of the Shelf). AFTER: **0 NaN** across all four, tangents unit to 1.5e-7
> and orthogonal to 3.3e-6, and agreeing with an INDEPENDENT equal-weight
> reference to a median **0.012-0.091 degrees**. Five new units in
> `Zenith_MeshAsset.Tests.inl`; the first fails on the old code.
>
> ★★ **THE NORMAL MAPS AND MESH NORMALS WERE ALREADY RIGHT**, and that is worth
> recording because it is where the search would naturally have gone. All four
> maps are 2048^2, centred (mean R/G 0.501/0.498), with **0.000%** of texels below
> z=0 so the engine's `z = +sqrt(1-x^2-y^2)` loses nothing; BC5 export is faithful
> (R/G correlate 0.85-0.96 against source on signal-carrying texels, cross-channel
> ~0, no V-flip). Mesh normals are unit to 1.2e-7, outward, and agree with the
> winding on 98-100% of triangles.
>
> ★ **AND THE UNIFORM `w = -1` HANDEDNESS IS NOT A BUG.** It reads like a global
> green-channel inversion. glTF's V axis points DOWN the image, so the true +V
> bitangent is `-cross(N,T)`, and an OpenGL-convention green channel needs exactly
> `+cross(N,T)` -- which is what the engine bakes. Every generated prop shows the
> same signature, which is the tell that it is the convention rather than the
> imports.

> **★★★ NO LEGACY ASSET READERS REMAIN (2026-08-31).** The investigation surfaced
> that `.zmesh` was being read through a "no envelope => assume the old layout"
> branch, and the whole family went with it:
>
> * `Zenith_ReadAssetStreamVersion` no longer falls back to a bare version word --
>   the stream envelope is MANDATORY.
> * Mesh, Model, Skeleton and Material REFUSE a non-current schema instead of
>   logging and parsing the payload as if it were current.
> * The material v2-v4 reader is gone (field order, the `bTransparent` bool mapped
>   onto the blend enum, `aeLegacySlots`), with its two tests.
> * Skeleton's `uVersion >= 2` branch and its now-vestigial version parameter.
> * The texture loader's legacy single-mip payload, its `bV2` flag, and
>   `LoadFromFile`'s `bCreateMips`.
>
> **Nothing is stranded by any of it**: every asset file is regenerable bake output
> under the `**/Assets/**` gitignore, so an older layout is a stale bake. 123 stale
> files were deleted and rewritten by the next tools boot.
>
> ★ **ELEVEN WRITERS EMITTED THE HEADERLESS LAYOUT**, four of them byte-identical
> copies of each other (the Bush / FallenTree / Rock / Tree prop exporters), plus
> the tools texture export, the terrain editor's maps, `Grid.ztxtr`'s inline
> self-heal, RenderTest's packed RM, TilePuzzle's icon generator, CityBuilder's
> icon AND heightmap writers, ScriptTest's 1x1 colour textures and Combat's.
>
> ★★ **THE LAST FIVE WERE ONLY FOUND BY BOOTING EVERY GAME, ONE AT A TIME.**
> Compiling all seven proves nothing about whether their assets load, and each
> unfixed writer silently RE-CREATED its stale file on its own next boot -- so a
> repo scan came back clean, then dirty again after the next game ran. The check
> that actually closes the loop is: fix every writer, boot all seven, THEN scan.
> Done: **0 headerless `.ztxtr` after all seven have booted.** All now go through the envelope, and the
> four duplicates were deleted in favour of the one exporter that owns the format.
> The v2 layout gained a real level COUNT rather than requiring a full mip chain,
> which is what let a single-mip texture stop needing a second format -- the MSDF
> font atlas needs exactly that, since mips break its median reconstruction.

> **★★★ `.zmesh` WAS TWO FORMATS. IT IS ONE NOW (2026-08-31).**
> `Zenith_MeshAsset` (stream envelope + schema) and `Flux_MeshGeometry` (its own
> element table, no version field) shared the extension, read by different loaders.
> The legacy branch was the only thing making that survivable; deleting it turned
> the collision into a visible error.
>
> `Zenith_MeshAsset` KEEPS `.zmesh`, alongside `.zmtrl` / `.zskel` / `.zmodel` /
> `.ztxtr`. **`Flux_MeshGeometry` moved to `.zgeom`** -- terrain chunks
> (`Render_*` / `Render_LOW_*` / `Physics_*`), the StickFigure, Combat's primitive
> geometry and the shared prop sets. ~140 source sites moved.
>
> ★ **IT WAS A RENAME, NOT A RE-BAKE.** The bytes were already the right format;
> only the extension was wrong. **14,498** files moved to `.zgeom` and **416**
> stayed `.zmesh`, so no terrain was rebuilt. VERIFIED on disk afterwards: every
> `.zmesh` carries the envelope with the mesh type id and every `.zgeom` carries
> none.
>
> OBSERVED `3577 ran / 3575 passed / 0 failed`, 2 skipped; Combat `1758`,
> RenderTest `1849` -- all three from their own `Null_` runs. **+5 new tangent
> units, -2 deleted legacy-material units = +3 on every row**, which is the
> signature of an engine change.

> **★★★ ZM +4 (2026-09-01, later) -- EVERY GENERATED PROP IS NOW PLACED IN GAME,
> which is a RULING and not a preference.** *"If the game generates an asset then
> it should be placed in game, unless there is a glb asset to replace, in which
> case the glb asset must be used."* Both halves are mechanical now.
> `Dawnmere.zscen` goes **48 -> 76 entities**, 80686 -> 85199 bytes, the only
> scene that moved. Pin **3586 -> 3590**, OBSERVED.
>
> ★★★ **TWELVE OF TWENTY-EIGHT ROSTER ROWS WERE RENDERED NOWHERE**, and nothing
> said so -- the count had to be produced by grepping, and the first attempt at it
> was wrong twice over (it called the six battle-dome dressing sets unused when
> `ZM_BattleArena.cpp` places all six, and missed the three ground items). That is
> the whole reason `Source/World/ZM_PropPlacement.h` exists: the answer is a
> FUNCTION anything can call, and `ZM_Dressing/EveryGeneratedPropIsPlacedInGame`
> refuses a row that answers NONE. OBSERVED: **28 props -- 6 interior, 13
> Dawnmere, 6 battle arena, 3 ground item, 0 NOWHERE.**
>
> ★★ **THE COST OF THE OLD STATE WAS NOT COSMETIC.** A `.glb` dropped onto a row
> nothing places replaces a model that still does not appear, so the import looks
> finished and renders nowhere. AB-PROP-07 arrived exactly that way -- LampPost
> had never been placed in any scene, so importing art for it was half a job and
> the placement row was the other half. That is now impossible to repeat silently.
>
> ★★ **THE SECOND HALF HAS ITS OWN GUARD, and it had already failed once.**
> `ZM_Gen/ImportedPropsUseTheirGlbAndNotTheGenerator` asserts that every prop with
> a `.glb` beside it has an IMPORTED baked mesh. The threshold is derived, not
> picked: measured across the roster, the largest GENERATED prop is **120**
> vertices (fence and bridge sections -- box compositions) and the smallest
> IMPORTED one is **3515** (the table), a 29x gap, so 1000 sits ~8x above every
> generator and ~3.5x below every import. OBSERVED: 7 imported props, 3515-7353
> verts, all using their `.glb`.
>
> ★★ **THE 28 COORDINATES WERE CHOSEN AGAINST A VALIDATED MODEL, not by eye and
> not by build-and-see.** An offline replica of `ZM_DawnmereBodyAnchorClearance`
> was checked FIRST against the eight barrel and lamp-post figures the engine had
> already reported -- it reproduces all eight to **0.4 mm** -- and every row was
> then picked to clear the margin before one was compiled. All 28 passed the
> engine's own clause on the first run, worst 1.992 m against a 1.00 m margin.
> **Validate the instrument on answers you already have, then use it.**
>
> ★ **WHAT WENT WHERE.** Notice board and two sign posts on the plaza's north
> edge; two lantern posts up the route lane; a four-section timber fence west of
> the home and a four-section dry-stone wall east of the lab; two small rocks, two
> large and a boulder on the outskirts; two low and two high ledges on the rising
> ground south of town; and two three-section crossings flanking the lane.
>
> ★★ **A FENCE'S SPACING IS ITS OWN ROSTER LENGTH, and that is now checked.**
> `ZM_ComputePropFit` scales every delivery so its longest axis lands exactly on
> the roster's longest number, so centres one roster-length apart abut whatever
> mesh arrives -- OBSERVED, the generated fence comes in at scale 0.9238, i.e. the
> generator's +/-4% jitter being corrected onto exactly 2.0 m.
> `FenceRunSpacingMatchesTheRosterLength` refuses a stale spacing after a
> re-roster, and it keys on WIDTH for a fence and DEPTH for a bridge because the
> two families tile along different axes.
>
> ★ **THE ANCHOR RULE MOVED ONTO THE ROW.** The old clause asserted one rule for
> the whole table -- "every outdoor prop stands against a building wall" -- true
> of the four barrels it was written for and false of everything since. Inferring
> it from `ZM_PROP_KIND` fails too: a lamp post flanks a doorway and a lantern
> post lines a lane, and both are `ZM_PROP_KIND_LAMP`. Each row now states
> `FREE` / `BUILDING_WALL` / `BUILDING_CORNER` and the unit checks THAT claim.
> OBSERVED: 36 outdoor props, 4 against a wall, 4 past a corner, 28 free-standing.
>
> ★ **THE FOUR FROZEN YAW VALUES GOT ONE HOME** (`ZM_PropData.h`), because there
> are two placement tables now and a frozen constant copied into the second is how
> two tables start disagreeing.
>
> ★★ **THIS MAP HAS NO WATERCOURSE, AND THE TWO BRIDGE RUNS SAY SO** rather than
> pretending otherwise. `ZM_GetDawnmereTerrainRecipe` has four pads, three paths
> and no water feature, so those six sections are dry crossings over the low
> ground either side of the lane -- honest as a culvert or a boardwalk, thin as a
> "bridge". They are placed because the ruling says a generated asset is placed;
> the right home is a stream, the day the terrain has one.
>
> ★ **The showcase gained a SECOND Dawnmere pose**, sharing the scene with the
> first. A room row is a POSE, not a scene, so the plan now compares SCENES before
> reloading -- the second wide costs nothing where a second load would have cost
> the 600-frame room deadline again. It exists because the headless clauses prove
> a prop is placed, clears the keep-out and is inside the terrain, and none of them
> can tell whether it RENDERS or is half-sunk. OBSERVED: 19 distinct prop models
> loaded, **zero** load failures.
>
> Gates: ZM **3590 ran / 0 failed**, `zenith test Zenithmon` **71 passed / 0
> failed**, doc_lint green, `ZM_ImportedPropShowcase_Test` PASSED with **24
> captures**, every scene republishes IDENTICAL on a second boot. Combat and
> RenderTest are untouched: every new unit is Zenithmon-side.

> **★★★ ZM +6 (2026-09-01) -- the SEVENTH import, AB-PROP-07 LampPost, and the
> first prop whose ENTITY OWNS A LIGHT.** Four lamp posts flank Dawnmere's two
> doorways, each with a point light AT ITS BULB rather than at its feet.
> `Dawnmere.zscen` goes **44 -> 48 entities**, 79720 -> 80686 bytes, and is the
> only scene that moved. Pin **3580 -> 3586**, OBSERVED.
>
> ★★★ **THE IMPORT HAD NEVER SURVIVED A SINGLE BOOT, AND THE SUITE WAS GREEN.**
> `ZM_Gen/PropBake_StaticModelFilesLandAndNoRig` hard-coded `ZM_PROP_LAMP_POST`
> and called `ZM_BakeProp` UNCONDITIONALLY -- an arbitrary representative, chosen
> when every prop in the roster was generated. The `.glb` import runs earlier in
> the same boot and writes the same paths, so between the import log line and the
> unit tally the mesh went **6623 verts -> 72** and every texture **2.8 MB ->
> 11 KB**, every boot, with nothing failing. The first measurement taken off that
> file was of a greybox wearing an imported asset's name.
>
> ★ **THE FIX IS NOT "POINT IT AT ANOTHER PROP"**, which re-arms the trap for
> whichever row is imported next. The test is about the GENERATOR, so its subject
> must be a prop the generator owns -- which is a property of the TREE, not a
> constant. It now resolves the first roster row with no `.glb` beside it
> (OBSERVED: `FenceWood`) and SKIPS if every prop has been imported, because that
> day baking one would destroy an asset. `ZM_EnsurePropBaked` was never the
> culprit: it is warm-safe and does nothing when a bundle is present.
>
> ★★★ **THE LIGHT OFFSET WAS WORLD-SPACE, AND IS NOW MODEL-SPACE.**
> `Zenith_LightComponent` already had a position offset;
> `GetWorldPosition` added it with `xPos += m_xPositionOffset`, ignoring the
> entity's rotation and scale entirely. NOTHING in any game used it, so the defect
> could not show -- and it shows the instant a light shares an entity with a
> MODEL, because "inside the lantern head" is a statement about the mesh:
>
> * **SCALE** is the one that bites with no rotation at all. `ZM_ComputePropFit`
>   scales this post by **3.0059** (a model 0.998 m tall onto a 3.0 m roster row),
>   so a world-space offset would have to be typed POST-scale and would silently
>   move the bulb out of the lantern the day the asset is re-exported -- exactly
>   the coupling `ZM_PropFit.h` exists to remove.
> * **ROTATION**: every interior prop carries an authored yaw and the outdoor
>   table can too; a world-space offset leaves the bulb behind when the post turns.
>
> It is `m_xLocalPositionOffset` now, applied as `pos + rot * (offset * scale)` --
> the order the transform itself composes. The rename is deliberate: a silent
> change of meaning under an unchanged name is the shape this repo keeps paying
> for. Five new units pin it (unused offset moves nothing, scale, rotation, the
> compose ORDER -- invisible whenever either factor is the identity -- and the
> stream round trip). OBSERVED live: model offset `(0, 0.3771, 0)` x 3.0059 ->
> world y **26.473** on an entity at 25.339, i.e. **2.634 m above the terrain**.
>
> ★★ **THE EDITOR CAN NOW PLACE A BULB WITHOUT A BUILD.** The offset panel was
> three world-space numbers with no indication of where the light ended up or what
> magnitude was sensible -- placing one inside a lantern meant typing a value,
> building, looking, and typing another. It now shows, whenever the entity also
> owns a model: the offset in the MODEL's units at **1 mm per drag pixel** (the old
> step moved it 10 cm a pixel, straight through the casing), the model's LOCAL
> BOUNDS, the offset as a FRACTION of them, the resolved WORLD position the
> renderer will use, whether the point is **inside the mesh at all** (a light
> outside its own casing is the whole bug, and it is said out loud rather than
> left to be noticed in a render), and a one-click *Centre of bounds* to drag from.
> A light on its OWN entity -- how every interior lamp in this game is authored --
> has no model, and simply gets none of the extra read-outs.
> `AddStep_SetLightPositionOffset` is the authoring verb, and it ENABLES the
> offset as well as setting it: an offset authored and left switched off is a lamp
> silently at its entity origin.
>
> ★★ **WHERE THE BULB IS BELONGS TO THE PROP, NOT THE PLACEMENT.**
> `ZM_GetPropBulb` is keyed by `ZM_PROP_ID`: every lamp post in the world has its
> bulb in the same place on the model, and where a post STANDS is the placement's
> business. Putting the offset on a placement row would copy one measurement into
> every row that used it, and the second copy is where they start disagreeing.
> MEASURED off the decoded mesh -- the lantern head is the flare above a shaft of
> radius ~0.017, widening to **0.0898 at y +0.399..+0.419** with a finial above,
> and the bulb is the AREA-WEIGHTED centroid of the glass between the bracket
> collar and the roof brim: **y = +0.3771, 87.8% of the model's height**. Area
> weighted so a densely tessellated rim cannot drag the answer. X and Z are ZERO
> rather than the centroid's own (+0.0025, +0.0037), which is a quarter-centimetre
> of mesh asymmetry: a bulb belongs on the post's axis and adopting those would be
> reading noise as intent.
>
> ★ **THE LANTERN POST IS DELIBERATELY NOT IN THAT TABLE.** It is a different
> asset with a different head, still generated, and its bulb has never been
> measured -- inheriting the lamp post's offset would put a light at a point on a
> mesh nobody has looked at.
>
> ★★★ **EVERY LIGHT IN THE GAME WAS RE-TUNED, and the lamp post was only where
> it was noticed.** The house was worse than the lamp: MEASURED as the largest
> connected near-white blob in each subject's own capture, `bed_three_quarter`
> carried a **114 px** halo at 2.96% of frame against the lamp post's 97 px.
>
> ★ **NOTHING WAS CLIPPING ANYWHERE** -- 0.00% of every frame above luminance 250,
> peaks under it, so the tonemapper was holding and none of this was "blown out"
> in the usual sense. Flux extracts bloom above a PRE-EXPOSURE HDR luminance of
> **3.0** (`Flux_HDR.cpp`), which is ABSOLUTE and scene-referred: the auto-exposure
> cannot rescue a light that pushes its surroundings past it. That is also why the
> cut was nearly free on screen -- scaling a room's lights together moved the
> room-wide frame's mean luminance 103.3 -> 98.2, i.e. not at all, because the
> exposure simply adapts. "Dimmer means darker" does not apply and was not what
> limited the tuning.
>
> ★ **THE TARGET CAME FROM THE SCENE.** ProfLab's tubes measured a **26 px** halo
> before any of this and read as a lamp that glows slightly, so "slight" already
> had a value in this game and everything was tuned onto it. OBSERVED, before ->
> after:
>
> | capture | before | after |
> |---|---|---|
> | `bed_three_quarter` (PlayerHome) | 114 px / 2.96% | **0** |
> | `bed_detail`, `table_detail`, `shelf_three_quarter`, `counter_west_detail` | 0-? | **0** |
> | `room_wide_PlayerHome` | 78 px / 1.47% | **32.7 px / 0.24%** |
> | `room_wide_ProfLab` | 26 px / 0.15% | **8.4 px / 0.02%** |
> | `lamppost_three_quarter` | 66 px / 1.00% | **35.0 px / 0.28%** |
> | `lamppost_detail` | 97 px / 2.13% | **63.3 px / 0.91%** |
> | `room_wide_Dawnmere` | 39.5 px | **17.6 px** |
>
> Values: LampPost **1200 -> 60 lm**; PlayerHome ceiling **950 -> 130**, bedside
> **360 -> 50**, table **330 -> 45**; ProfLab tubes **1150 -> 160**.
>
> ★★ **THE LEVER IS SUB-LINEAR, which is why this took four values and not one.**
> The halo is a blurred source, so its RADIUS grows far slower than the intensity:
> each ~3x cut bought only ~15 px of diameter on the lamp post (96.7 -> 79.9 ->
> 63.3). A first guess of "reduce it a bit" (1200 -> 500) was worth almost
> nothing, and only measuring a sweep showed that.
>
> ★★ **THE LAB WAS SCALED TOO, THOUGH IT WAS ALREADY IN RANGE.** Its tubes only
> measured well because they hang at 3.2 m in a 20 x 16 m room; cutting the house
> alone would have left the lab over **4x** brighter per unit floor area instead of
> the designed **1.7x** (ZM-D-176) -- a deliberate relationship broken as a side
> effect of fixing something else. A uniform scale also leaves
> `ZM_InteriorTintPixels_Test`'s red/blue measurement untouched, since that is a
> RATIO and the scale cancels.
>
> ★ **THE PHOTOMETRIC COST IS REAL AND ACCEPTED.** 130 lm is a nightlight, not a
> ceiling pendant; 60 lm is a dim street lamp. The bloom threshold was raised
> 1.0 -> 3.0 for the brighter SUN calibration and has never been re-derived for
> point-source fixtures indoors. Honest figures belong here the day it is, and
> they should go back up TOGETHER so the rooms keep their ratio.
>
> ★★ **AND THE LAMP POST'S REMAINING HALO IS NOT REMOVABLE BY THIS NUMBER.** The
> bulb is INSIDE its own lantern -- the glass sits 0.1-0.27 m from a point source,
> and `ComputePointAttenuation`'s `1/(4*pi*d^2)`, clamped only at
> `MIN_LIGHT_DISTANCE` = 1 cm, puts those surfaces ~350x the source intensity
> whatever it is. Driving the head itself under the threshold would need
> single-digit MILLILUMENS. The real fix is an emissive lantern material plus a dim
> spill light, or a light with a source RADIUS so the near field stops exploding --
> both engine work, neither a number in a table.
>
> ★ **The lamps stand OFF the front corners, not against the walls**, and the X
> values are what the keep-out leaves rather than what looks nice: a first draft
> at +/-5.5 m off each building's axis was INSIDE the 6 m blind drive leg and the
> 8 m arrival marker. Level with a frontage, a lamp has to be ~10 m off the axis
> to clear both -- which happens to put it just past the building's corner, where
> a street lamp belongs. OBSERVED clearances **3.927 m** (home) and **5.200 m**
> (lab) against a 1.00 m margin. The placement unit had to grow with them:
> "anchored to a building" means AGAINST a wall for furniture and PAST a corner
> for a lamp, and the row's KIND selects which, because the ground in between is
> the door approach.
>
> ★ **Their yaw is free by SYMMETRY, not by measurement** -- unlike the barrel's.
> The lantern head is a flare of revolution about the post's axis (its radius
> profile is a function of height alone) and the model carries no bracket or arm
> to point.
>
> ★★ **ALL THREE PINS MOVE, and that is the tell that this one touched
> `Zenith/**`.** The five light-offset units are ENGINE-side, so they land in
> every game's boot suite: Combat **1758 -> 1763** and RenderTest **1849 -> 1854**
> with no game code changed at all. The sixth is Zenithmon's bulb table. Every
> number is OBSERVED from its own `Null_vs2022_Debug_Win64_True` run, never
> arithmetic on another.
>
> Gates: ZM **3586 ran / 0 failed**, Combat **1763**, RenderTest **1854**, 0
> failed each (`Tools/unit_baselines.json` bumped from those three runs),
> `zenith test Zenithmon` **71 passed / 0 failed**, doc_lint green,
> `ZM_ImportedPropShowcase_Test` PASSED with **23 captures**, and every scene
> republishes IDENTICAL on a second boot.

> **★★★ ZM +3 (2026-09-01, later still) -- THE FIRST ROSTER PROP PLACED
> ANYWHERE OUTDOORS.** Four barrels now stand against Dawnmere's two building
> walls. `Dawnmere.zscen` goes **40 -> 44 entities, 79066 -> 79720 bytes**, and it
> is the only scene that moved. Zenithmon-only: pin **3577 -> 3580**, OBSERVED,
> the three new units below.
>
> ★★ **HALF THE ROSTER IS PLACED IN NO SCENE AT ALL**, and Dawnmere's ~500 props
> are not a counter-example: `ZM_DawnmereScatterGroup` instances the shared ENGINE
> sets under `Zenith/Assets/Meshes/{Rocks,FallenTrees,Bushes}`, and its own
> comment says "Nothing in this table is Zenithmon-owned art". So this is a new
> mechanism, not a new row in an old one.
>
> ★★★ **THIS BLOCK FIRST SAID "22 OF THE 28" AND WAS WRONG TWICE OVER** -- it
> listed the six biome dressing sets among the unused when `ZM_BattleArena.cpp`
> places all six, and it missed the three ground items entirely. COUNTED properly,
> by grepping every non-generator reference to each id:
>
> * **USED (16)** -- six interior furniture rows, LampPost and Barrel outdoors,
>   six `DRESSING_*` (one per battle-dome biome, `ZM_BattleArena.cpp`), three
>   `ITEM_*` (`ZM_GroundItem.cpp`).
> * **PLACED NOWHERE (12)** -- FenceWood, FenceStone, SignPost, TownBoard,
>   LanternPost, BridgePlank, BridgeStone, LedgeLow, LedgeHigh, RockSmall,
>   RockLarge, Boulder.
>
> Every one of the twelve is an outdoor fixture or scatter piece -- exactly the
> families this map gets from the shared engine sets instead. They cost nothing at
> runtime (`ZM_BakeAllAssets` has no shipped caller, `ZM_EnsurePropBaked` is
> warm-safe); two automated tests bake the whole roster, which is why all 28
> folders sit on disk. **A .glb dropped onto one of them replaces a model nothing
> renders**, which is why AB-PROP-07 needed a placement row and AB-PROP-05/06 did
> not.
>
> ★★ **AUTHORED, NOT SCATTERED, AND THAT DECIDES THE KEEP-OUT.** The scatter draws
> a position, tests it and keeps it -- right for things that GREW or FELL where
> they are, wrong for a barrel, which a person puts upright against a wall. The
> consequence is the interesting part: the hard keep-out is two halves ANDed, and
> only ONE of them is a safety property.
>
> * **graded ground** (pads and paths at flatten radius) is a proxy for the
>   judgement a randomly-drawn point cannot exercise. Applying it to a hand-placed
>   prop would refuse **both building pads** -- precisely the ground a barrel
>   belongs on, and nowhere else.
> * **body anchors** (blind drive legs, NPC anchors, warp markers, the seam gate)
>   is the safety one, and applies in full: `DriveTowardXZ` has no obstacle
>   avoidance, so a collider on a blind leg wedges a traversal into its frame cap
>   with a failure naming a DISTANCE rather than the blocker (playbook 3.4).
>
> The two were already separate functions internally; the second is now a public
> entry point (`ZM_DawnmereBodyAnchorClearance`) with the argument on it. OBSERVED
> clearances: **1.809 m** for the two home rows, **2.823 m** for the two lab rows,
> against a 1.00 m margin -- and the margin is set above the barrel's FITTED
> half-diagonal, not its roster row's, because the roster understates this model by
> 27%.
>
> ★ **THE CORNERS ARE WHERE THE CLEARANCE AND THE FICTION AGREE.** Both buildings
> are entered on their -Z face and both door approaches are blind legs with a 6 m
> radius, so the middle of each frontage is spoken for -- and a barrel by the
> corner of a house is where one would actually be anyway. Each CENTRE stands
> 0.70 m off its wall, which puts the barrel's own surface 0.25 m off it. Ground
> heights are SAMPLED at authoring time from the same standalone terrain-editor
> session the scatter uses (23.9001 / 24.0069 / 24.0923 / 24.1010 m), so the
> authored Y is a terrain height plus the fit's lift rather than a typed number.
>
> ★★ **AND THE SHOWCASE HARNESS STOPPED BEING INTERIOR-ONLY, because its own claim
> had become false.** That file says "adding a row to axIPS_SUBJECTS is the whole
> cost of a new asset"; the first subject standing on TERRAIN was the row that was
> not. Three assumptions were interior-only and are now per-room flags or explicit
> data:
>
> * **the clamp** -- an interior has walls to keep a lens inside; reading a room
>   spec for Dawnmere folds every eye into a 15.5 x 11.5 m box on the town's
>   ORIGIN, 100 m from the subject and still pointing at it.
> * **the floor** -- indoors an authored prop's Y IS the fit's ground lift, and the
>   fit clause asserts it. Outdoors that number is a sampled terrain height the
>   test cannot re-derive, so the SCALE half still asserts and the implied ground
>   is LOGGED (OBSERVED 23.9001, matching the authoring's own line exactly).
> * **the wide pose** -- was three numbers, with eye X assumed 0 and eye Y computed
>   from a ceiling. Six now. Both interior rows carry exactly what those
>   expressions produced, so no interior framing moved.
>
> Subjects key on a room ROW INDEX rather than a `ZM_INTERIOR_ROOM`, because there
> is no enumerator for "outside, in Dawnmere" and the row is what actually carries
> the scene, the camera and the flags.
>
> ★ **A NEGATIVE AZIMUTH, AND THE REASON IS THAT NOTHING CLAMPS IT BACK.** The
> outdoor subject is the home's WEST barrel, so its lens swings toward -X/+Z. Aim
> it positive and the eye is INSIDE the house -- and outdoors the room clamp that
> would have caught that indoors is off by construction, so the shot would be of
> the inside of a wall with every assertion passing.
>
> ★ **ZM_InteriorFurniture NOW WEARS A NAME THAT DOES NOT DESCRIBE EVERY USER**, and
> that is recorded as debt on the class rather than fixed here. Its job -- "wear the
> prop your NAME resolves to, then re-size the collider to it" -- is as true of a
> barrel against a house as of a bed in a bedroom, and each dressing header owns
> the resolver for its OWN table (making the interior one walk the Dawnmere table
> would make an interior room include a town), so only the composition is new.
> Renaming the class changes the component-name string `PlayerHome.zscen` and
> `ProfLab.zscen` serialize: a re-author of both plus matched meta AND editor
> registry edits, where a component in one registry but not the other authors a
> scene WITHOUT it, byte-stably, every gate green. Worth doing; not in the change
> that first places a prop outdoors.
>
> Gates: ZM **3580 ran / 0 failed** (`Tools/unit_baselines.json` bumped from this
> OBSERVED run), `zenith test Zenithmon` **71 passed / 0 failed** -- which is the
> real proof the clearances hold, since eleven of those suites traverse Dawnmere --
> doc_lint green, `ZM_ImportedPropShowcase_Test` PASSED with **21 captures**, and
> every scene republishes IDENTICAL on a second boot, `Dawnmere.zscen` included.

> **★★ ZM +0 (2026-09-01, later) -- the SIXTH import, AB-PROP-06 Barrel, and the
> first prop measured to have NO FACING.** `Assets/Props/Barrel/Barrel.glb` was
> already in the tree and already LIVE before this round began -- the import walks
> the whole assets tree at every tools boot -- so `PlayerHome.zscen` had moved by
> exactly HomeBarrel's scale and lift with nothing else touched. This round is the
> verification that was missing, not the integration. OBSERVED model
> **0.8921 x 0.9995 x 0.8931 m** -> scale **1.0005**, ground y **0.5000** ->
> **0.8925 x 1.0000 x 0.8935 m**. It stands in BOTH rooms (`HomeBarrel`,
> `LabBarrel`), and **`AB-PROP-01` through `AB-PROP-06` are now all ticked** --
> section 7's "if exactly one thing were commissioned" answer, delivered in full.
>
> ★★ **A SCALE OF ~1 IS NOT "DELIVERED TO SIZE", AND THIS IS THE ASSET THAT PROVES
> IT.** 1.0005 is the nearest the identity of the six -- nearer than the chair's
> 1.0020 -- and the barrel is still **27% OVER its roster row in plan**: 0.892 x
> 0.893 against 0.7 x 0.7. The fit promises the LONGEST axis and nothing else at
> any scale. A single scale number read as a fit report gets this delivery exactly
> backwards, which is why it earns a row in `ZM_PropFit`'s table despite adding
> neither a new axis (Y, like the chair and shelf) nor a new magnitude.
>
> ★★ **"IT IS A BARREL, SO ITS YAW DOES NOT MATTER" IS A GUESS. IT WAS MEASURED.**
> Both barrels share `YAW0`, which is the same shape as the shelf's and the
> counters' shared constants -- and those were both WRONG, chosen while the prop
> was a symmetric greybox. The only thing separating "free" from "nobody checked"
> is evidence, so it was collected with the same effort a front would have been. A
> stencilled brand, a bung on one stave or a hasp would each supply a facing that
> no shape measurement can see, and each is the kind of detail a commissioned asset
> arrives with unannounced. OBSERVED: the body is a **solid of revolution to within
> 2.3%** peak deviation from a circle across all eight height bands; the base
> colour carries only stave grain and iron hoops -- **15% spread over 36 azimuth
> bins, dark ones SCATTERED** rather than clustered, no mark anywhere in the
> 2048^2 map; the sole asymmetry is a lid plug **0.040 m off the axis**, 9% of the
> barrel's radius, on a horizontal face. So the yaw is genuinely free, `YAW0` is
> left, and `ZM_InteriorDressing.h` says so ON the row -- **do not "fix" it**, and
> do not read the +X-front convention as applying to a model with no +X face.
>
> ★ **Tangents and textures on the same checks as the counter.** 0 non-finite T/N/B
> of 4386, 0 zero-length, worst |dot(T, N)| = 0.0000. Three real maps at 2048^2,
> no `occlusionTexture`, and its ORM R channel is a constant **255** -- nothing
> dropped. Roughness (G) mean **0.885** and metallic (B) mean **0.023**: rough
> dielectric timber, the opposite end of the range from the counter's 0.326 / 0.386
> steel-framed bench, out of the same importer with no swizzle.
>
> ★ **THE LID IS ON, AND THAT WAS CHECKED RATHER THAN TAKEN FROM THE PROMPT.** The
> row asks for "lid on" and the render shows an open-looking rim with a dark
> interior, which is what an open barrel looks like too. A cross-section cannot
> tell them apart -- collapsing an axis turns a RING into a filled band -- so the
> highest surface was measured per radial ring instead: **r 0.09..0.32 tops out at
> y +0.4305**, a closed head recessed **69 mm** below the rim, with the staves and
> top hoop standing proud of it (r 0.32..0.41 reaching +0.476) and the plug at the
> centre. The base is the control: every ring bottoms at -0.4509, a flat closed
> disc. So the dark interior is the inside of a 69 mm chime, which is what
> coopering looks like.
>
> ★ **AND THE BLUE IN THAT RECESS IS THE ROOM, NOT THE ASSET.** It reads as an
> obvious defect on a warm-lit prop. MEASURED: pixels with B > R+4 are **0.27%**
> of the barrel's three-quarter frame, all of them dark (mean RGB 30, 35, 46). The
> control is the rest of the roster in the same room -- bed **0.82%**, chair
> 0.72%, shelf 1.33%, table **4.38%**, the room wide 0.93% and the live player view
> **10.98%** -- so the barrel has LESS of it than every other PlayerHome subject
> but one. Deeply-occluded geometry falling back to the cool ambient where the warm
> point lights cannot reach is a property of this room's lighting, and a saturated
> hue in a small dark region reads far louder than its 0.27% deserves.
>
> ★ **Its two capture poses disagree on elevation more than any other subject's**
> (24 and 14 degrees), and that is the subject rather than a style knob: a barrel's
> LID is horizontal and reads only from above, its STAVES and HOOPS are vertical
> and read only from the side. It is also captured in PlayerHome, where it costs
> the run 264 frames and no extra scene load.
>
> ★ **Two stale claims on `ArtBrief.md` were fixed in passing**, both of which had
> outlived six deliveries: section 8 still said *"It does not describe an import
> pipeline, because none exists ... commissioning any row means building one
> first"* (the top of the same page has said otherwise since AB-PROP-01), and
> section 7 still recommended a commission that has since been completed in full.
>
> Gates: ZM `3577 ran / 0 failed` (unchanged), `zenith test Zenithmon` **71 passed
> / 0 failed**, doc_lint green, `ZM_ImportedPropShowcase_Test` PASSED with **18
> captures**, and all seven scenes republish IDENTICAL. `PlayerHome.zscen` and
> `ProfLab.zscen` are the only assets that moved across both rounds.

> **★★ ZM +0 (2026-09-01) -- the FIFTH import, AB-PROP-05 Counter, and the first
> prop whose TWO PLACEMENTS NEEDED DIFFERENT YAWS.**
> `Assets/Props/Counter/Counter.glb` needed no pipeline change and no new unit: a
> row in `ZM_PropFit`'s asset table, two rows in `axIPS_SUBJECTS`, and two
> corrected yaws. OBSERVED model **0.4321 x 0.6079 x 0.9995 m** -> scale
> **2.2011** (the largest correction of the five), ground y **0.6690** ->
> **0.9511 x 1.3381 x 2.2000 m**. `ZM_PROP_COUNTER` stands only in ProfLab, so
> **`ProfLab.zscen` is the only scene this moved** -- 23 entities, 3542 bytes
> before and after, because swapping one frozen quaternion for another is the
> same number of bytes.
>
> ★ **ONE YAW PER ASSET STOPPED BEING ENOUGH.** Both benches said `YAW90`, chosen
> while this prop was a symmetric greybox box -- the same blind spot the shelf's
> yaw came out of, except that a shelf appears twice on the SAME wall and these
> two do not. `LabCounterWest` is at x = -8.20 and `LabCounterEast` at x = +8.20,
> against opposite walls, and a bench's back belongs to its own wall. `YAW90` is
> additionally worse than "turned around": it puts the 2.2 m length on the X axis
> and stands each bench END-ON to its wall, 2.2 m out into the room. They are
> `YAW0` and `YAW180` now, and the harness recovers each angle from the LIVE
> quaternion (`2*atan2(q.y, q.w)`) rather than from the constant that produced it:
> OBSERVED west `quat (w 1.00000, y 0.00000) = 0.0 deg, model +X faces
> (1.000, 0.000, 0.000)`, east `quat (w 0.00000, y 1.00000) = 180.0 deg, model +X
> faces (-1.000, 0.000, 0.000)` -- both fronts into the room -- and world
> footprint **0.951 x 1.338 x 2.200 m** on each, i.e. the length along Z, along
> the wall.
>
> ★★ **THE SHELF'S FACING INSTRUMENT ANSWERS NOTHING HERE, AND THAT IS THE
> TRANSFERABLE PART.** "Which extreme has a continuous panel against it" works on
> a bookshelf because a bookshelf has a flat back; run it on this model and the
> best of the six faces reaches **11%**, against the shelf's **90%**. A lab bench
> is a cabinet with an overhanging top, not a panel. What settled it was the two
> features the ArtBrief row's own prompt names: **232 of 5661 vertices sit ABOVE
> the worktop plane**, confined to x in [-0.159, -0.125] of a model spanning
> +/-0.216 -- the shallow lip, which is at the BACK -- and the worktop **overhangs
> the cabinet body by 20 mm on +X and 0 mm on -X** -- a nosing, which is at the
> FRONT. Both answer +X, so this delivery agrees with the four before it, but the
> earlier method would have reported no facing at all. **Reproduce the previous
> asset's published numbers before trusting a measurement tool on a new one**: the
> first rasteriser written for this gave the shelf 6.7% where the commit says
> 90.2%, and it was wrong (fixed sample count per triangle, so a large flat panel
> under-fills), not the shelf.
>
> ★ **THE TANGENT FRAMES WERE CHECKED, NOT ASSUMED.** `MakeValidTangent` landed
> with AB-PROP-04 and this is the first asset imported after it: OBSERVED over
> `Counter.zmesh`, **0 non-finite tangents, normals or bitangents of 5661, 0
> zero-length tangents, worst |dot(T, N)| = 0.0000**. The barrel reports the same.
>
> ★ **EVERY TEXTURE IN THE FILE REACHES THE RENDERER, AND THERE ARE THREE.** The
> `.glb` declares `baseColorTexture`, `metallicRoughnessTexture` and
> `normalTexture`, all **2048^2**, and **no `occlusionTexture`** -- and the
> occlusion channel is not merely undeclared, it is **empty**: the R channel of
> that PNG is a constant **255** across the whole map, one distinct value. So the
> 68-byte `Counter_ao.ztxtr` is the importer's neutral-white 4x4 and nothing was
> dropped. Roughness (G) and metallic (B) are real -- **127 and 176 distinct
> values** -- which is the first import where metallic carries a steel frame
> rather than one flat number, and the RM map is written with NO swizzle because
> Flux's `SampleRoughnessMetallic` reads exactly `.gb`.
>
> ★★ **THE SHOWCASE HARNESS IS KEYED BY ROOM NOW.** Every subject through the
> shelf stood in PlayerHome, so `ZM_AutoTests_ImportedPropShowcase` loaded that one
> scene and resolved every roster row inside it; a `LabCounter` row added as-is
> fails `IPSResolveRoom` and burns the 600-frame deadline without saying why. The
> two rejected alternatives are worth naming: moving a bench into a bedroom changes
> the GAME to suit the harness, and a second showcase file duplicates the phase
> machine, capture, projection and verification -- leaving two places to add the
> sixth asset to. **The first four subjects pay nothing**: rooms are captured in
> order, PlayerHome finishes before ProfLab loads, and its framing is byte-for-byte
> the same table it was. ProfLab takes a wide of its own (eye z = 6.60, derived:
> both benches at x = +/-8.20 need `8.20 / (z + 3.00) < tan(48.6 deg)`, so
> z > 4.22) and NOT the follow-camera pair, which is about the ceiling clamp rather
> than any asset. **BOTH benches are photographed** -- the first time one prop
> takes two rows -- because a single subject signs off exactly half of a claim
> whose whole content is that the two differ.
>
> OBSERVED `ZM_ImportedPropShowcase_Test: PASSED (2114 frames)`, **16 captures**
> in `Build/artifacts/zenithmon/visual_audit/`. Gates: ZM `3577 ran / 0 failed`
> (unchanged -- a row in an existing table adds no unit), `zenith test Zenithmon`
> **71 passed / 0 failed**, registry unchanged, and **all seven scenes republish
> IDENTICAL** on a second boot. Combat and RenderTest are untouched: the whole diff
> is inside `Games/Zenithmon/`.
>
> ★ **A `Barrel.glb` IS ALSO IN THE TREE AND IS ALREADY LIVE.** It is not part of
> this round and `AB-PROP-06` is deliberately NOT ticked, but the import path walks
> the whole assets tree at every tools boot, so it replaced the generated barrel on
> its own: `PlayerHome.zscen` moved by exactly HomeBarrel's scale
> (**1.00951 -> 1.00049**) and ground y (**0 -> 0.5**), and nothing else. That asset
> imports soundly on the same checks as this one (0 non-finite tangents, three real
> maps, constant-255 occlusion), so the change is a working replacement rather than
> a breakage -- but **AB-PROP-06 still needs its own facing measurement, roster row
> and captures before it can be ticked**.

> **★★ ZM +0 (2026-08-31, later still) -- the FOURTH import, AB-PROP-04 Shelf,
> and the first prop whose FACING was measured rather than assumed.**
> `Assets/Props/Shelf/Shelf.glb` needed no pipeline change and no new test: a row
> in `axIPS_SUBJECTS`, a row in `ZM_PropFit`'s asset table, and a corrected yaw.
> OBSERVED model **0.2637 x 0.9980 x 0.4863 m** -> scale **2.0039**, ground y
> **1.0000** -> **0.5284 x 2.0000 x 0.9746 m**, in both rooms that stand one
> (`HomeShelf`, `LabShelf`), so `PlayerHome.zscen` and `ProfLab.zscen` both moved.
>
> ★ **THE AUTHORED YAW WAS WRONG, AND ONLY A REAL MODEL COULD SHOW IT.** Both
> shelves were authored `YAW90`, chosen when this prop was a symmetric greybox box
> whose yaw no picture could contradict -- the same blind spot that hid the two
> rotation defects the chair exposed. `YAW90` stands a bookshelf SIDE-ON to the
> wall, open face down the room, 0.97 m of it protruding.
>
> The facing was MEASURED off the decoded mesh rather than inferred from the
> commission prompt: rasterised into a (Y, Z) grid, **90.2%** of the footprint has
> material within 15 mm of the **-X** extreme -- a continuous full-height,
> full-width back panel -- against **38.0%** on +X, which is instead broken into
> the open bays the books and jars are modelled into. So the front is +X, like
> every import before it, and `YAW0` is what leaves it there. Both rooms are
> `YAW0` now, and the harness recovers the angle from the LIVE quaternion
> (`2*atan2(q.y, q.w)`), never from the constant that produced it: OBSERVED
> `authored quat (w 1.00000, y 0.00000) = 0.0 deg yaw; model +X faces
> (1.000, 0.000, 0.000)`.
>
> ★ **+X IS A PROPERTY OF FOUR DELIVERIES, NOT A RULE ANYTHING ENFORCES.** No
> artist was told it and no gate checks it. `ZM_InteriorDressing.h` says so where
> the convention is stated and `ArtBrief.md` 0.1 says it where a future commission
> would be written. Measure the next one.
>
> ★ **EVERY TEXTURE IN THE FILE REACHES THE RENDERER, AND THERE ARE THREE.** The
> `.glb` declares `baseColorTexture`, `normalTexture` and
> `metallicRoughnessTexture`, all **2048^2**, and **no `occlusionTexture`** -- so
> the 68-byte `Shelf_ao.ztxtr` is the importer's documented neutral-white 4x4, not
> a dropped map, and it is 68 bytes for the bed, table and chair too. glTF packs
> roughness in G and metallic in B and Flux's `SampleRoughnessMetallic` reads
> exactly `.gb`, so the RM map is written with NO swizzle. OBSERVED means over
> that image: **G (roughness) 0.4675, B (metallic) 0.1007** -- a mostly-dielectric
> timber, unlike the table's uniform 0.29 metallic.
>
> ★★ **AND THE R CHANNEL -- WHERE glTF WOULD FOLD AN OCCLUSION MAP -- IS
> UNIFORMLY 1.0.** That is the check worth recording, because "no
> `occlusionTexture` declared" would NOT by itself prove none was delivered: the
> ORM convention hides occlusion in the metallic-roughness image's red channel,
> which the importer does not read. It is 1.0 everywhere, so there is no baked
> occlusion anywhere in this file and the neutral white is exactly equivalent
> (`lerp(1, ao, strength) = 1`). Checked because "4 texture(s)" in the import log
> counts the placeholder and would read as a full set either way.
>
> ★ **IT IS THE LARGEST FIT CORRECTION SO FAR AND THE WORST PROPORTION MATCH.**
> Scale 2.0039 against the chair's 1.0020, out of the same branch-free expression.
> Fitted to its 2.0 m roster height it is 0.53 x 0.97 in plan where the row asks
> for 1.2 x 0.4 -- left VISIBLE per `ZM_PropFit.h`'s uniform-scale ruling rather
> than squashed, because a per-axis squash would shear the modelled-in books.
>
> ★ **AND `room_wide` STOPPED BEING A COVERAGE GUARANTEE**, which its comment
> claimed ("the 65-degree lens covers both long walls"). MEASURED from that fixed
> pose: bed 33.4 deg off-axis, table 36.4, chair 42.7, against a 48.6 deg
> horizontal half-frame -- but the shelf, hard against the -X wall, sits at
> **60.0 deg** and is 11 deg outside it. No axial pose fixes that (the room is
> 15.5 m wide and 11.5 m deep; seeing x = -6.9 at z = +0.9 from the centre line
> needs a ~114 deg lens) and the FOV is not touched, on that file's own terms: a
> player standing in the doorway cannot see the shelf either. The comment now says
> CONTEXT, and coverage stays with the two aimed shots each subject owns -- those
> are the ones that assert a subject is actually in frame. Eleven capture shots
> now (three fixed + two per subject).
>
> OBSERVED `3574 ran / 3572 passed / 0 failed`, 2 skipped; batch
> `71 passed / 0 failed`; `doc_lint` all checks pass; all seven scenes republish
> `IDENTICAL` on the next boot. **No pin moved** -- both test edits were table
> rows. Registry stays **71**. (The same tree reports 3611 from the Vulkan boot,
> the +37 gap `Tools/unit_baselines.json` already documents.)

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
