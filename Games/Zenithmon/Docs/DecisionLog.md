# Zenithmon -- Decision Log

**Purpose:** Append-only record of every non-trivial decision made during
Zenithmon development. Future agents grep this when investigating "why was X
done this way?". Scope changes MUST land here as a user decision before any
implementation (see [Scope.md](Scope.md) Section 4).

**Format:** One entry per decision, **newest first**. Fields per entry:
date, id, decision, why, tests-that-lock-it, reversibility. Ids are `ZM-D-NNN`,
assigned in chronological order (so the highest number is at the top of the file).

**What counts as non-trivial:** anything involving trade-offs, anything another
system depends on, any engine-side change, any scope or convention ruling.
Tuning-value changes go in git history, not here.

---

## 2026-07-14 -- ZM-D-064 -- S4 ZM_CreatureGen SC5a: FLOATER-PLANTOID builder + all-152 coverage gate (all 8 archetypes wired)

- **Trigger:** the FINAL archetype builder + the "coverage complete" milestone. Authored by one subagent against the frozen seam; the orchestrator wired the last switch case, added the all-152 gate, gated serially, ran a reviewer.
- **Decision:** SC5a adds the FLOATER-PLANTOID builder + per-archetype test, wires the last ZM_GetArchetypeBuilder case, and adds CreatureGen_AllSpeciesBuildable to the shared harness. FLOATER-PLANTOID: a fixed 10-bone floating creature -- a 3-node bulb body (Spine00..02 [root], centred above the ground so it FLOATS), a Head crown, and 6 RADIAL round tendrils (Tendril0..5) via a builder-local ZM_FloaterAppendTendril helper (modeled on ZM_AvianAppendWing). ROUND (Rx==Rz) tendrils were chosen over flat petals because the loft rings are axis-aligned and cannot rotate -- a round section is rotation-invariant so the 6-fold radial symmetry reads cleanly; the 6 angles are FIXED constants k*(2pi/6), never rng. NO legs. The floating invariant (mesh min-Y bound > 0) is structural and reviewer-verified analytically (lowest tendril tip ~0.215*fS). With all 8 archetypes wired, CreatureGen_AllSpeciesBuildable asserts EVERY of the 152 species resolves to a non-null builder -- proving the switch covers every ZM_ARCHETYPE and the generic 12-invariant harness now runs over the FULL dex.
- **Reviewer:** no blockers/majors -- determinism, the floating invariant (traced), the local tendril helper (real signatures, no dangling pointer, no flat-washer), foundation-fidelity, the all-152 gate + dispatch, conventions, and scope ALL clean. A few PRE-EXISTING stale doc-comments ("SC1 wires ONLY QUADRUPED" in ZM_CreatureGen.h / ZM_Tests_CreatureGen.cpp / the ZM_BakeAllCreatures skip note) are now inaccurate -- cosmetic, deferred to SC5b/SC5c which edit those files anyway (no behavior impact; the code paths are SC-agnostic).
- **Tests-that-lock-it:** boot unit gate **1842 -> 1846** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too; +3 FLOATER-PLANTOID tests + 1 all-152 gate). The generic 12-invariant harness now covers ALL 152 species. `zenith test Zenithmon --headless` 6/0. No stale-test churn.
- **Milestone:** creature MESH + skeleton + albedo/shiny/dex-icon generation is now FEATURE-COMPLETE IN-MEMORY for the full dex. Remaining S4: SC5b = the deferred .zmtrl/.zmodel bundle bake in ZM_BakeCreature (needed to make baked creatures scene-loadable); SC5c = bake-all + the windowed species-gallery visual gate (the S4 GATE hard-stop for user sign-off).
- **Reversibility:** High. New per-archetype code + 1 switch case + 1 test; no baked assets. Golden pins per builder documented in the .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-063 -- S4 ZM_CreatureGen SC4: INSECTOID + BLOB archetype builders

- **Trigger:** next SC of ZM_CreatureGen after SC3. Two archetypes -- the bone-count EXTREMES -- authored in parallel by disjoint subagents; the orchestrator wired the dispatch switch, gated serially, ran a reviewer.
- **Decision:** SC4 adds the INSECTOID and BLOB builders + per-archetype tests, wired into the explicit ZM_GetArchetypeBuilder switch (no header edit). INSECTOID (the HIGH-limb extreme): a fixed 19-bone bug -- Spine00..03 segmented thorax/abdomen [root], Head, SIX 2-bone legs via ZM_AppendLimb (LegL0/L1/L2/R0/R1/R2 -- exactly 6 '...Up' roots, parented to thorax/mid/rear spine), and 2 antennae via ZM_AppendHorn -- comfortably <=30 (asserted in-builder). BLOB (the LOW-bone extreme): a fixed 4-bone gelatinous body -- a 3-node ZM_AppendSpineTube driving m_fBellyRound (-> ZM_LoftRing m_fSuperEllipse) for a soft box-rounded silhouette + a single crown Nub via ZM_AppendHorn, no limbs. Both draw all rng up-front (MESH then SKELETON) via ZM_MakeGenRNG (the 6-leg + antenna loops REUSE pre-drawn values -- no per-iteration draws), scale by m_fSizeScale, keep topology IDENTICAL across evo stages (elaboration scales sizes only), and never finalise/bake.
- **Reviewer:** no blockers/majors/minors -- 3 cosmetic nits only (NOT fixed: a comment conflating m_fBellyRound/m_fSuperEllipse [code correct]; two slightly-loose test-subset comments; a guarded auSpine[uSpineCount-3] latent-underflow that mirrors the Quadruped idiom and cannot trigger given the >=4-segment assert). Determinism, the INSECTOID bone budget (19, asserted <=30, exactly 6 leg roots, valid parenting), BLOB [2,4] bones + the real m_fBellyRound field, foundation-fidelity, conventions, and scope ALL verified clean.
- **Tests-that-lock-it:** boot unit gate **1836 -> 1842** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too). The universal 12-invariant harness now also runs over the INSECTOID + BLOB species; each archetype adds a structural bone assert (INSECTOID: single Spine root + Head + EXACTLY 6 leg '...Up' roots + antennae + <=30 total; BLOB: single root + total bones in [2,4] + zero limb bones). `zenith test Zenithmon --headless` 6/0. No stale-test churn.
- **Reversibility:** High. New per-archetype code under Source/Gen/ + Tests/; the only shared-file touch is the 2 switch cases. No baked assets. Golden pins per builder documented in each .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-062 -- S4 ZM_CreatureGen SC3: SERPENT + AQUATIC archetype builders

- **Trigger:** next SC of ZM_CreatureGen after SC2. Two archetypes authored in parallel by disjoint subagents against the frozen seam; the orchestrator wired the dispatch switch, gated serially, and ran a reviewer.
- **Decision:** SC3 adds the SERPENT and AQUATIC builders + per-archetype tests, wired into the explicit ZM_GetArchetypeBuilder switch (no header edit -- all 8 builders were declared at SC2). SERPENT: a limbless upright/rearing snake -- a fixed 12-bone skeleton (Spine00..05 six-vertebra body tube [root], Head with a +Z snout, Tail00..02 tapering to a point, HornL/R brow frills) built ENTIRELY from the shared kit (no local loft helper needed -- every serpent part is a round Y-swept shape). AQUATIC: a fixed 8-bone fish (Spine00..02 streamlined body [root], Head, FinDorsal, FinPecL/R, FinCaudal) using the kit PLUS one archetype-LOCAL helper ZM_AquaticAppendFin (a flat thin-Rx/broad-Rz blade generalized from AVIAN's ZM_AvianAppendWing to serve up- AND down-swept fins; single-bone ring skin, valid winding both sweep directions, no flat-washer, no dangling ring pointer). Both draw all rng up-front (MESH then SKELETON) via ZM_MakeGenRNG, scale by m_fSizeScale, keep bone topology IDENTICAL across evo stages (elaboration scales horn/fin SIZE only), stay <=30 bones, and never finalise/bake.
- **Reviewer:** no blockers/majors/minors -- determinism, the AQUATIC local fin helper (verified against the real ZM_GenCommon loft signatures), SERPENT limblessness + within-cap spine chain, foundation-fidelity, conventions, and scope ALL clean. Two benign observations (no action): the builders locally re-derive the kit's spine-node world formula + mirror the kit's private add-bone helper -- the sanctioned anon-namespace pattern (kit internals are un-linkable), bind-pose-neutral.
- **Tests-that-lock-it:** boot unit gate **1830 -> 1836** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too). The universal 12-invariant harness now also runs over the SERPENT + AQUATIC species; each archetype adds a structural bone assert (SERPENT: single Spine root + Spine/Tail chain + Head + ZERO limb '...Up' bones; AQUATIC: single Spine root + Head + dorsal/2 pectoral/caudal fin bones). `zenith test Zenithmon --headless` 6/0. No stale-test churn -- the SC2 SC-agnostic dispatch test absorbed the newly-wired archetypes.
- **Reversibility:** High. New per-archetype code under Source/Gen/ + Tests/; the only shared-file touch is the 2 switch cases. No baked assets. Golden pins per builder (jitter ranges, proportions, bone layout) documented in each .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-061 -- S4 ZM_CreatureGen SC2: BIPED + AVIAN archetype builders

- **Trigger:** next SCs of ZM_CreatureGen after SC1 (QUADRUPED). Two archetypes authored in parallel by disjoint subagents against the now-frozen seam; the orchestrator wired the dispatch switch, gated serially, and ran a reviewer.
- **Decision:** SC2 adds the BIPED and AVIAN archetype builders (each ALONE in ZM_CreatureArchetype_<Name>.cpp) + per-archetype tests, wired into the explicit ZM_GetArchetypeBuilder switch. BIPED: a fixed 14-bone upright skeleton (Spine00..03 root chain -> Head, ArmL/R Up/Lo from the shoulders, LegL/R Up/Lo from the pelvis, a dorsal Crest) built entirely from the shared kit. AVIAN: a fixed 13-bone skeleton (Spine00..02 root -> Head -> Beak, WingL/R, LegL/R Up/Lo, Tail00..01) from the kit PLUS one archetype-LOCAL helper ZM_AvianAppendWing (a flat thin-Rx/broad-Rz swept wing blade the round-tube ZM_AppendLimb cannot express; single-bone ring skin, outward winding, no flat-washer, verified against the real loft signatures). Both draw all randomness up-front in a fixed MESH-then-SKELETON order via ZM_MakeGenRNG, scale by m_fSizeScale, keep bone topology IDENTICAL across evo stages (elaboration scales crest/beak/wing/tail SIZE only), stay <=30 bones, and never finalise/bake.
- **Seam finalisation (tech-lead append, now complete):** the frozen ZM_CreatureGen.h had declared only the QUADRUPED builder; SC2 appends the remaining 7 builder declarations -- a pure, transparent append (changes no existing declaration). This is the ONE sanctioned change to the frozen header; SC3-SC5 add ONLY a new .cpp + one switch case, never a header edit.
- **Test-harness fix (orchestrator, single-writer):** the SC1 dispatch test CreatureGen_ArchetypeDispatch hard-coded "only QUADRUPED wired" and reddened once BIPED/AVIAN landed; rewritten SC-AGNOSTIC (dispatch is pure + total over ZM_ARCHETYPE, QUADRUPED always routes, the ZM_ARCHETYPE_COUNT sentinel does not, >=1 archetype + >=1 species buildable) so it never goes stale as the wired set grows. The generic ZM_Gen harness auto-covers the new BIPED/AVIAN species (it loops non-null builders).
- **Reviewer:** no blockers/majors -- determinism, the AVIAN local wing helper, foundation-fidelity, and scope all clean. One MINOR fixed in this commit: the BIPED per-archetype tests gained a top-level HasBipedBuilder() early-return (mirroring AVIAN) so they no-op rather than false-fail if BIPED were ever unwired.
- **Tests-that-lock-it:** boot unit gate **1824 -> 1830** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too). The universal 12-invariant harness now also runs over the BIPED + AVIAN species; each archetype adds a structural bone assert. `zenith test Zenithmon --headless` 6/0.
- **Reversibility:** High. New per-archetype code under Source/Gen/ + Tests/; the header append + 2 switch cases + the dispatch-test rewrite are the only shared-file touches. No baked assets. Golden pins per builder (jitter ranges, reference proportions, bone layout) documented in each .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-060 -- S4 ZM_CreatureGen SC1: frozen seam + core driver + shared kit + QUADRUPED reference builder

- **Trigger:** next unchecked S4 task ("ZM_CreatureGen -- all 8 archetypes ..."), the single biggest S4 work item (~7-8.5k lines). Deliberately broken into sub-commits SC1..SC5 rather than one landing. Design produced by a survey -> 3-architect panel -> synthesis workflow; the frozen header was vetted by the orchestrator against the real foundation (all `ZM_GEN_DOMAIN` members present, every symbol/signature confirmed), then authored by parallel subagents (foundations -> [quadruped builder + tests]) and gated serially by the orchestrator.
- **Decision:** SC1 lands the FROZEN public seam `Source/Gen/ZM_CreatureGen.h`, the core driver `ZM_CreatureGen.cpp`, the shared append-kit `ZM_CreatureArchetypeCommon.{h,cpp}`, the ONE reference archetype (QUADRUPED, `ZM_CreatureArchetype_Quadruped.cpp`), and the full generic test harness. The seam:
  - `ZM_ResolveCreatureRecipe(id)` reads `ZM_SpeciesData` ONCE into a read-only `ZM_CreatureRecipe` that PRE-DERIVES one PCG seed per `ZM_GEN_DOMAIN` into `m_aulDomainSeed[]`. An archetype builder receives only `(ZM_GenMesh&, const ZM_CreatureRecipe&)` and reaches randomness ONLY through `ZM_MakeGenRNG(recipe, domain)` -- the entropy door (`ZM_GenRNG`'s default ctor is deleted upstream), so the determinism contract is STRUCTURAL, not a review convention.
  - Dispatch is an EXPLICIT switch `ZM_GetArchetypeBuilder(archetype)` (never a self-registering table -- MSVC static-init dead-strip would silently drop archetypes from the static-linked TUs). SC1 wires ONLY QUADRUPED; every other archetype returns `nullptr`, so the generic harness auto-grows coverage as each SC adds a case.
  - `ZM_BuildCreatureMesh` owns the ONE finalise order (`ZM_GenGenerateTangents -> ZM_GenNormalizeSkinWeights`; analytic loft normals kept, never regenerated), debug-asserts `ZM_ValidateGenMesh` at the 30-bone creature cap. `ZM_BuildCreature` produces the full in-memory `ZM_Creature` bundle (mesh+skeleton + BC1 albedo + hue-rotated shiny + flat dex icon). The equality/hash trio + `ZM_ValidateCreature` (fills every S4 gate flag) make the mandated tests one-liners.
  - The kit binds each authored ring to a SINGLE bone (`blendB=0`), so the loft's Catmull-Rom subdivision is the ONLY source of the <=2-bone blends -- influence count provably never exceeds 2. Helpers: `ZM_AppendSpineTube/Limb/Tail/Horn/EllipsoidHead` + `ZM_SizeClassScale` + `ZM_FormatBoneName`.
  - QUADRUPED builds a fixed 18-bone skeleton (Spine00..03 root chain -> Head, LegFL/FR/HL/HR Up/Lo, Tail00..02, HornL/HornR), with IDENTICAL bone names/order/count across all evo stages so index-keyed clips transfer; elaboration (`m_uEvoStage-1`) only grows horn size, never topology.
- **Golden-pinned by SC1 (a change = `uZM_CREATUREGEN_VERSION` bump + cold family re-bake):** the size-class scale curve TINY .45 / SMALL .70 / MEDIUM 1.00 / LARGE 1.50 / HUGE 2.20; the primary-type -> pattern-kind table (STRIPES{FIRE,ELECTRIC,BRAWL,DRAKE} / SPOTS{GRASS,VENOM,SWARM,FEY} / GRADIENT{WATER,ICE,SKY,MIND} / BELLY{NORMAL,EARTH,STONE,IRON} / NONE{PHANTOM,UMBRAL}) + tier-scaled pattern params; the shiny hue band [80,280) deg (SHINY domain, `m_fJitter` stays 0/dormant); eye U/V 0.5/0.35 + radius [0.05,0.075] (EYE domain); dex-icon = box-downsample of albedo over a primary-type-tinted background (DEX_ICON domain, tint mix [0.25,0.35]); the asset scheme `game:Creatures/<Name>/<Name><suffix>.<ext>`; and the QUADRUPED jitter ranges / reference proportions / horn-size-by-tier curve. Domain usage: ALBEDO drives the single `ZM_SynthCreatureAlbedo` rng; PATTERN is reserved and UNCONSUMED in SC1 (pattern KIND derived purely, no draw).
- **Deferred to SC5 (logged):** `ZM_BakeCreature` (TOOLS-only) bakes mesh+skeleton (`ZM_GenBakeMesh`) + albedo/shiny (`ZM_SynthBakeAlbedoBC1` x2) + icon (`ZM_SynthBakeIconBC1`); the `.zmtrl` (normal + shiny child material) and `.zmodel` bundle writes are a commented `// SC5:` TODO -- a baked SC1 species is not yet a scene-loadable finished creature. The other 7 archetype builders + the all-152 coverage gate + the windowed species-gallery visual gate are SC2 (BIPED+AVIAN) / SC3 (SERPENT+AQUATIC) / SC4 (INSECTOID+BLOB) / SC5 (FLOATER-PLANTOID + gate + gallery).
- **Tests-that-lock-it:** boot unit gate **1804 -> 1824** (0 failed; +20 `ZM_Gen` creature units across `Tests/ZM_Tests_CreatureGen.cpp` + `Tests/ZM_Tests_CreatureArchetype_Quadruped.cpp`; baseline bumped in `.github/workflows/zm-tests.yml` too). A generic parameterized harness runs the 12 universal invariants over every buildable (QUADRUPED-in-SC1) species -- same-seed byte determinism + equal content hash; per-domain seed isolation (all 152); outward winding; non-degenerate bounds within a sane box; weights sum-to-1; <=2 influences; bone caps <=30 & <=100; in-range indices + well-formed single-root/parent<child skeleton; shiny differs same-dims + shared mesh; icon non-empty + >=2 distinct texels; distinct-species + stage1-vs-stage3 sensitivity; and evo-stage topology IDENTICAL (bone count + per-index names, over every multi-stage family) -- each build guarded on a non-null builder (ZM_BuildCreature is void + asserts a non-null builder). Plus golden-locks for the size-scale curve, the asset-path ref scheme + truncation, `ZM_FormatBoneName`, and the shiny band. Reviewer verdict: no blockers/majors -- correctness, determinism, and foundation-fidelity verified against the real loft/synth source (`ZM_LerpRingSkin`/`ZM_EmitAndStitch`).
- **Reconciliations (found during authoring, no rework):** `ZM_BuildCreature` is `void` + asserts a non-null builder (un-wired archetypes proven via `ZM_GetArchetypeBuilder==nullptr`, never by invoking the build); include convention is game-root-relative (`#include "Zenithmon/Source/Gen/..."`, matching the shipped modules); `ZM_CreatureAssetPath` returns the canonical `game:` ref while a tools-only mirror writes the FS path under `GAME_ASSETS_DIR`.
- **Reversibility:** High. All new `ZM_`-prefixed code under `Source/Gen/` + `Tests/`; no engine/foundation edits; no baked assets. The one hard commitment is the golden-pinned generation algorithm (above). The header is FROZEN after SC1 (append-only, tech-lead-only) so the 7 remaining archetype authors work disjointly against it.

---

## 2026-07-14 -- ZM-D-059 -- S4 asset-gen foundation: ZM_GenCommon + ZM_TextureSynth frozen (pure library + tools-only bake bridges)

- **Trigger:** first unchecked S4 task ("ZM_GenCommon (seeded RNG + loft toolkit) + ZM_TextureSynth"). Design produced by a survey -> 3-architect panel -> synthesis workflow; approved by the orchestrator after verifying the load-bearing engine seams exist.
- **Decision:** The two S4 foundation modules live under `Games/Zenithmon/Source/Gen/` (not `Tools/`). The PURE deterministic library -- `ZM_GenRNG`, seed derivation (`ZM_GenHashName`/`ZM_GEN_DOMAIN`/`ZM_GenDeriveSeed`), `ZM_GenNoise`, the loft toolkit building into a POD `ZM_GenMesh`, `ZM_GenImage`, and all texel synthesis -- is compiled in EVERY config with **no `ZENITH_TOOLS` guard** and zero engine-asset/GPU/disk coupling. Only the disk-bake bridges (`ZM_GenBakeMesh`, `ZM_SynthBake*`) are `#ifdef ZENITH_TOOLS`, each with a non-tools inline no-op so `_False` builds link. `ZM_GenRNG` WRAPS `ZM_BattleRNG`'s golden PCG32 (ZM-D-027) -- never re-implements it -- deletes its default ctor to force explicit ID-derived seeding, and adds a fixed integer->float `NextFloat01`. The loft fills a `ZM_GenMesh` whose SoA buffers mirror `Zenith_MeshAsset`; the bake bridge copies element-wise without re-deriving normals/tangents. Seeds derive ONLY from stable IDs/names via `ZM_GenHashName` (byte-identical to `ZM_TerrainAuthoring`'s `ZM_Fnv1a32`, pinned by a test) folded with a **PINNED** `ZM_GEN_DOMAIN` enum. `ZM_CreatureGen`/anim/human/building/prop and the versioned `ZM_BakeManifest` are separate later boxes; these modules only expose a `uZM_GENCOMMON_VERSION` / synth version constant.
- **Why:** the S4 gate requires `ZM_Gen` units to run headless in the CI backbone (GPU-less; determinism is a TESTED invariant). Keeping the pure library always-compiled (the `ZM_TerrainAuthoring` pure-policy precedent) lets the tests run with no `ZENITH_TOOLS`/GPU/disk dependency and keeps the registered unit count identical across configs. Wrapping `ZM_BattleRNG` avoids golden-constant drift. Building into a POD that mirrors the asset layout makes "what we test in-memory == what Export bakes", so in-memory S4 determinism implies on-disk S9 determinism.
- **Golden-pinned NOW (hard to reverse -- frozen in the first commit):** the `ZM_GEN_DOMAIN` enum ordering + `ZM_GenDeriveSeed` fold. Changing them later invalidates every future baked asset (full cold re-bake), so both are locked by `Seed_DeriveIsStableGolden`.
- **Accepted risk (logged):** transcendentals (`sinf`/`cosf`/`powf` in ring points, superellipse, hue-rotate) are not bit-portable across compilers/ISAs. Bounded by win64-only + a single CI toolchain + `/fp:fast` forbidden; an Android bring-up would need per-arch golden re-baselining or an integer angle table.
- **Deferred open questions (decide when the consuming box is authored, NOT blocking):** dex-icon recipe ownership, fixed-vs-per-family shiny hue angle (>=30deg for BC1 safety), dual-type palette blend ratio, achromatic-shiny saturation/lightness fallback, and `ZM_BakeManifest` per-family vs per-species granularity. Provisional recommendations recorded in the design artifact.
- **Tests-that-lock-it:** 31 `ZENITH_TEST(ZM_Gen, ...)` cases (boot baseline **1773 -> 1804**) across `Tests/ZM_Tests_GenCommon.cpp` (RNG same-seed + battle-golden-wrap + seed-derivation golden + domain disjointness + FNV anchor + noise determinism + loft byte-identical/winding/bounds/weights-sum/bone-caps/seam/subdiv-t0/topology + validator self-check) and `Tests/ZM_Tests_TextureSynth.cpp` (texel byte-identical + family-seed differs + shiny-differs-same-dims + 18-type palette + blend order + pack determinism/sRGB + normal-from-height + rect/eye decals + stripes/spots determinism).
- **Implementation notes (found during integration/review):** (1) the loft's wall winding is Y-oriented -- `ZM_EmitAndStitch` flips the stitch winding per adjacent ring-pair when the sweep ASCENDS in Y (`bFlip = cur.Y > prev.Y`) so `cross(C-A,B-A)` stays OUTWARD for the repo cull rule, matching the caps' existing Y-based orientation (the winding test failed first on ascending fixtures; per-pair flip is robust for non-monotonic chains; ΔY==0 documented as a degenerate precondition, no hard assert). (2) `ZM_GenHashName` restates the FNV-1a arithmetic locally rather than including `ZM_TerrainAuthoring.h` (which drags `ZM_WorldSpec.h`), pinned equal by `Hash_Fnv1aMatchesTerrainAnchor` + the `"A"->0xC40BF6CC` literal. (3) reviewer verdict SHIP-WITH-FIXES applied in the same landing: real tangent/normal-regen determinism coverage (was vacuous size-0 compare), `uSegs>=3` and `iParent>=-1` asserts, and `ZM_SynthHueRotate`'s achromatic threshold raised to `2/255` (above 8-bit quantization) to keep its internal differs-assert from firing on near-grey inputs.
- **Reversibility:** High. All new `ZM_`-prefixed code under `Source/Gen/` + `Tests/`; no engine files change; no baked assets exist yet. The one hard commitment is the golden-pinned seed derivation (above).

---

## 2026-07-13 -- ZM-D-058 -- Dawnmere grass made visible: per-region grass chunks + central town lawn

- **Trigger:** the S3 human visual review REJECTED the Dawnmere exterior -- no
  visible grass. Runtime instrumentation (not theory) showed grass generated
  200,159 blades and uploaded, but only ~720 were drawn from the town-center
  spawn.
- **Root cause (two compounding):** (1) `Flux_GrassImpl::GenerateFromTerrain`
  built a SINGLE terrain-spanning chunk; `UpdateVisibleChunks` picked one LOD for
  it from the camera->AABB-centroid distance (~150 m) -> the whole map rendered
  at LOD3 (~12.5%). (2) All six Dawnmere grass dabs were peripheral, so the spawn
  sat in a grass-free hole (nearest grass ~150 m away).
- **Engine decision (shared Flux change):** partition generated blades into a
  world-space chunk GRID (`GrassConfig::fCHUNK_SIZE` = 64 m) -- counting-sort the
  instance array contiguous per cell, shuffle within a cell (so LOD reduction
  samples the whole cell), and give each chunk exact bounds. `ExecuteRender` now
  issues one instanced `DrawIndexed` per visible chunk with `firstInstance` = the
  chunk's base offset; `Flux_Grass.slang` `vsMain` gains `SV_StartInstanceLocation`
  and indexes `InstanceBuffer[SV_InstanceID + startInstance]` (SV_InstanceID does
  not include firstInstance -- mirrors `Flux_Terrain_ToGBuffer`). Grass near the
  camera now renders at LOD0; distant regions LOD-down / cull independently.
- **Game decision:** two central town-lawn grass dabs (centres (512,470) r150 and
  (512,610) r130) so grass surrounds the `TownCenter` spawn; the plaza/home pads
  and paths still erase their paved footprints in the grass-erase phase.
  `fGRASS_DENSITY_SCALE` 0.15 -> 0.70. Dawnmere grass is DECORATIVE only -- the
  `ZM_TerrainGrass` component is the rendering bridge; tall-grass encounter
  gameplay is S5's `ZM_TallGrassSystem`/`ZM_EncounterZone` and is ROUTE-only.
- **Why:** a Pokemon starting town reads as grassy; the single-chunk LOD was a
  latent renderer bug (any camera far from a big terrain's grass centroid loses
  all grass) that also benefits RenderTest.
- **Tests that lock it:** new `SpawnVisible` phase in `ZM_GrassRegeneration_Test`
  asserts `GetVisibleBladeCount() > 0` from the spawn camera (the old test proved
  generate+upload but never on-screen visibility -- the exact gap the human eye
  caught). Golden terrain-recipe assertions in `ZM_Tests_TerrainAuthoring`
  updated for the two new dabs (grass dab count 6->8, plan size 1022->1024,
  GRASS_FILL phase 6->8). Dawnmere now bakes/uploads 573,693 blades / 8,098
  triangles. Re-gated all-green (5-config builds, boot units 1772/0, 6/6
  automated, RenderTest `TerrainEditorSmoke`). Grass `.spv` regenerated via
  FluxCompiler (git-LFS + shader-validation).
- **Reversibility:** high -- density scale and dabs are recipe data; the chunk
  grid is a self-contained rewrite of `GenerateFromTerrain`/`ExecuteRender`.
- **Status:** APPROVED by the user 2026-07-13 ("those three screenshots look
  fine") -- the S3 visual gate is SIGNED OFF on the fresh `new_01/02/03` set;
  committed to master.

## 2026-07-13 -- ZM-D-057 -- PlayerHome round trip uses an opaque camera barrier and globally ordered persistent fade

- **Scene/content decision:** build index **40** is now the always-authored,
  terrain-independent `PlayerHome` interior. Its collidable procedural greybox
  shell, scene-owned Player/main follow camera, `Door` feet marker at
  `(0,0,3.5)`, and live `PlayerHomeExitTrigger -> (2,"FromHome")` are the first
  real interior route. Dawnmere adds a replaceable greybox home shell, the
  `FromHome` feet marker at `(384,26.590313,482)`, and live
  `HomeDoorTrigger -> (40,"Door")`. `ZM_GreyboxVisual` claims serialization
  order **107** (next free 108) and reconstructs a unit-cube model/material per
  runtime scene generation; it is deliberately replaceable S3 blockout, not a
  new baked-asset family or final S4 art.
- **Fade/readiness decision:** the persistent `ZM_GameStateRoot` owns one exact
  full-screen black `WarpFade` UIOverlay at sort order **10000**. The manager,
  not the widget, advances alpha over **0.20 s** each way. Accepted input freezes
  immediately; `QUEUED` cannot issue its one SINGLE load below alpha 1. After
  the replacement Player is placed and both body velocities/controller intent
  are zeroed, the manager remains opaque through `WAITING_FOR_CAMERA`. Fade-in
  begins only when exactly one active-scene `ZM_FollowCamera` entity also owns a
  Camera component, targets that exact Player generation, and is the active
  scene's main camera. Input unlocks only at alpha 0. Losing the scene, Player,
  camera readiness, or exact overlay during the transition returns to or stays
  opaque with all active Players frozen. This makes fade a fail-closed traversal
  safety boundary, not decoration.
- **Engine UI-order decision:** element sort order is global across loaded
  canvases for quad rendering. `Zenith_UICanvas` forwards its current element
  key into the shared Flux quad queue; upload preparation stable-sorts ascending
  by key, with submission index as the equal-key tie-break. Raw/non-UI callers
  retain key 0. The fixed **1,024-quad** queue drops the newest excess quad and
  warns once per frame, preserving already-queued overlays without overflow.
  Flux Text retains the highest-sort active overlay clip across canvas
  collection (equal keys remain last-writer-wins). `DiscardPendingFrame` is the
  single disabled/reset boundary for both the legacy and render-graph Text
  paths: it drains the pending queue, background/foreground/total counters, and
  overlay clip together, so re-enabling Text cannot replay stale submissions or
  inherit an old clip. This closes the cross-scene case where the persistent
  canvas is visited before a later active-scene HUD that would otherwise draw
  over or replace the fade's text clip.
- **Engine asset-lifecycle decision:** `Flux_ModelInstance::SetMaterial` retains
  material overrides through its owning `MaterialHandle`; callers do not add a
  second manual reference. `Zenith_ModelComponent` v8 deserialization therefore
  keeps each decoded material alive with its temporary registry handle only
  until a model instance accepts ownership. A procedural empty-model record has
  no receiving instance, so the temporary material becomes reclaimable instead
  of leaking one reference on every Dawnmere/PlayerHome greybox reload.
- **Engine instantaneous-overlay decision:** for a zero/negative fade duration,
  `Zenith_UIOverlay::Show` synchronously snaps current dim alpha to configured
  dim alpha, including an already-showing repair, while `Hide` synchronously
  clears current alpha/visibility and restores sibling interaction.
  Positive-duration animation is unchanged. This makes the rendered overlay's
  opacity agree with manager alpha during the same hitch-sized update instead
  of requiring a later UI update before black is actually submitted.
- **Automation decision:** `ZM_PlayerHomeRoundTrip_Test` drives real controller
  input at a deterministic **1/30 presentation dt** while the engine retains its
  ordinary fixed physics substeps. It performs FrontEnd -> Dawnmere bootstrap,
  runs to and collides with the Dawnmere door, enters build 40 at `Door`, runs to
  and collides with the exit, and returns to `FromHome`. Each leg asserts
  fade-out/opaque-load/camera-barrier/fade-in ordering, exactly one load, scene
  and entity generation replacement, persistent manager identity, exact XZ and
  spawn-centre placement, zero motion/intent before unlock, no terrain grass in
  the interior, and grass/camera recovery outside. The final physics-contact
  settlement permits at most 5 cm downward Y while XZ stays exact.
- **Tests that lock it:** four new `ZM_WorldTraversal` T0 cases bring that
  category to **16**. `FadeAdvanceClampsInvalidDtAndRuntimeReset` also exercises
  the production global queue sort, equal-key stability, actual UICanvas key
  forwarding, capacity guard, text-clip arbitration, complete disabled/reset
  queue/counter/clip drain in both Text paths, clean re-enable, owning material
  retain/release, transient-material registry reclamation after procedural
  empty-model deserialization, and direct zero-duration overlay Show/Hide. The
  opaque-before-load case now crosses the full 0.20 s fade in one **0.25 s**
  manager tick, renders the real manager canvas inside the load callback, and
  requires an actually submitted sort-10000 alpha-1 quad before load permission.
  The other cases lock unique exact-camera readiness, fail-closed dependency
  loss, alpha-zero unlock, and runtime-state serialization omission. These
  lifecycle assertions extend existing cases rather than adding registrations;
  the category remains **16** and the P1 registry remains **6**.
- **Definitive post-overlay-hitch automated gate (local authority):** regen
  **2.401 s**. All five win64 targets are green: Vulkan Debug Tools=true
  **11.225 s**, Debug Tools=false **11.755 s**, Release Tools=true **11.213 s**,
  Release Tools=false **11.031 s**, and D3D12 Debug Tools=false **7.656 s**.
  Boot units are **1773 ran / 1772 passed / 0 failed / 1 skipped**, with
  **180.640 s** helper wall under the canonical watchdog; the workflow baseline
  is 1773, `ZM_WorldTraversal` is 16, and the P1 registry is 6. Headless ran all
  **6/6** in **1.590 s** wall: two semantic passes (`ControllerHarness` **142
  frames / 25.100 ms**, Boot **1 / 0.018 ms**) plus exactly four
  graphics-required skips. Windowed: `ZM_WarpInfrastructure_Test` **29 /
  2008.714 ms** (**14.869 s** wall; one frame removed by synchronous opacity),
  `ZM_GrassRegeneration_Test` **11 / 2579.674 ms** (**15.125 s** wall),
  `ZM_DawnmerePlayerCamera_Test` **117 / 6212.128 ms** (**18.712 s** wall), and
  `ZM_PlayerHomeRoundTrip_Test` **673 / 14662.601 ms** (**27.514 s** wall).
  RenderTest rebuilt in **6.192 s**; `EngineBootShutdownSmoke` passed **1 /
  28.606 ms** (**40.622 s** wall) and `TerrainEditorSmoke` **151 / 5291.193
  ms** (**46.025 s** wall). The ignored
  `Build/artifacts/zenithmon/s3/final/post_overlay_hitch_fix/` root contains **12
  parsed JSON / 12 passed / 0 failed**, with exactly the four expected headless
  graphics skips. The instantaneous Show/Hide and real-quad assertions add no
  registrations or baseline drift.
- **Visual-gate boundary:** stable ignored evidence is
  `Build/artifacts/zenithmon/s3/visual/01_dawnmere_exterior_terrain_grass_camera.png`,
  `02_playerhome_interior.png`, and
  `03_dawnmere_return_camera_reacquired.png`. Provenance is capture
  `capture_final_posthitch_20260713_183717` from the definitive binary: the
  round-trip test passed **673 frames / 14619.2 ms** with exit 0, and all three
  ignored/inspected PNGs are valid **1280x720**. Their respective SHA-256
  values are
  `9FEFA6E1B20CB9F1647F19A0416FCD6A80ACA653EB6EEEFE6A86DD722790A1DF`,
  `13104E86246748BF58AF200DFAC213C2A6B6595A81086E30346B75857280B90E`, and
  `B0D49B1CE41ACB98AA184E55ECB1531D34DC76009C3BED0CBD67CCD61C3B4B41`.
  The automated gate is green, but the stage remains **NOT COMPLETE** until the
  user reviews those captures and a separate sign-off decision is appended.
  S4/S5 do not begin while the
  `GATE-WAIT: S3 visual sign-off` marker remains.
- **Reversibility:** the build-40 scene, greybox visuals, trigger geometry, and
  0.20 s timing are localized/additive. Build index, spawn tags, component order,
  and v1 scene streams are compatibility contracts. Global quad/text ordering is
  engine-wide but preserves raw key-0 and equal-key historical order; reverting
  it would reintroduce the persistent-cross-canvas occlusion bug and requires a
  different global overlay compositor first.

## 2026-07-13 -- ZM-D-056 -- Persistent spawn-tag traversal uses a manager-only root and generation-exact scene entities

- **Decision / ownership:** register `ZM_GameStateManager`, `ZM_SpawnPoint`, and
  `ZM_WarpTrigger` at unique serialization orders **104 / 105 / 106**.
  FrontEnd authors a non-transient, manager-only `ZM_GameStateRoot`; the
  authoritative manager calls `DontDestroyOnLoad` on that root. Player and
  camera remain destination-scene-owned and are replaced by SINGLE loads. The
  singleton stores a generation-bearing `Zenith_EntityID`, rejects missing or
  ambiguous lookup, and retires a duplicate manager in `OnStart` while
  preserving the live authority.
- **Transition contract:** a request must be idle and resolve an exact WorldSpec
  target-build/spawn-tag pair. Its source must contain exactly one valid
  active-scene dynamic-capsule Player, except for the deliberate playerless
  **FrontEnd build-index-0 direct-request** path used before a Player exists.
  Acceptance freezes the source immediately and records its full ID. The state
  machine advances `QUEUED -> WAITING_FOR_SCENE -> WAITING_FOR_SPAWN`, issuing
  exactly one `SCENE_LOAD_SINGLE` on the next manager update. A replacement
  Player freezes itself from `ZM_PlayerController::OnStart` while any transition
  is active, so component order cannot leak an input frame before placement.
- **Spawn / motion contract:** a tag is 1-31 printable ASCII bytes
  (`0x20..0x7E`) in a NUL-padded 32-byte buffer and lookup requires exactly one
  exact-case marker in the loaded destination scene. A marker transform denotes
  **feet**. Dawnmere authors `TownCenterSpawn` at
  **(512,25.98577,480)**; the Player's scale-derived 0.9 m capsule half-extent
  yields centre **(512,26.88577,480)**. Resolution performs the allowed one-time
  body teleport, zeros linear and angular velocity, resets controller runtime
  state, enables movement, and returns the manager to idle. Missing/duplicate
  markers, invalid bodies, or scene-generation changes remain frozen and
  waiting rather than placing ambiguously.
- **Trigger contract:** `ZM_WarpTrigger::OnStart` reasserts its collider as a
  sensor. Collision entry is accepted only when the other entity's full ID is
  the unique valid active-scene dynamic-capsule Player; additive-scene,
  duplicate, malformed, bodyless, foreign-body, and slot-reused candidates
  fail closed. A successful overlap latches exactly once; only collision exit
  by that exact generation-bearing ID clears it.
- **Serialization boundary:** all three components have fixed v1 **scene
  component** streams. The manager stream is version-only; spawn/trigger streams
  persist authored tags and target build data. Queued/waiting transition state,
  frozen IDs, load counts, and trigger latches are runtime-only. This is not the
  durable S7 `ZM_SaveSchema` and does not ship player save/load.
- **Tests that lock it:** exactly **12** `ZM_WorldTraversal` T0 tests cover the
  singleton/state machine, transactionality, tag/stream boundaries, real sensor
  pass-through, reset/re-entry latch behavior, feet-derived placement, motion
  reset, destination `OnStart` freeze, duplicate retirement, and scene/entity
  slot-reuse generations. The boot gate is **1769 ran / 1768 passed / 0 failed /
  1 skipped**. All **5** P1 tests register: headless Boot + ControllerHarness
  pass while Warp/Grass/Dawnmere skip for graphics. Windowed evidence is
  `ZM_WarpInfrastructure_Test` **4 frames / 885.7 ms**,
  `ZM_GrassRegeneration_Test` **11 / 1927.5 ms**, and
  `ZM_DawnmerePlayerCamera_Test` **117 / 5043.5 ms**. All four Vulkan
  Debug/Release x Tools true/false builds plus the D3D12 Debug Tools=false link
  proof are green.
- **Boundary / next:** the current P1 makes a direct FrontEnd manager request;
  there is no fade, PlayerHome/build-40 scene, or authored live trigger yet.
  PlayerHome plus the Dawnmere round trip/fade is the next Roadmap box. The hard
  human visual gate waits until that box and the full S3 automated gate are
  complete, so this milestone is not a stop point.
- **Why / reversibility:** keeping only the manager persistent avoids carrying a
  Jolt body or camera through SINGLE scene teardown. Exact generation checks and
  fail-closed uniqueness prevent stale pool slots or additive content from
  triggering/placing the wrong Player. Orders, streams, and authoring data are
  now compatibility contracts; trigger geometry, fade presentation, and future
  WorldSpec edges remain additive behind the same manager API.

## 2026-07-13 -- ZM-D-055 -- Scene-owned velocity controller and fixed follow camera make Dawnmere traversable

- **Decision:** complete the S3 input/controller/camera Roadmap box with three
  game-local seams. `ZM_InputActions` is a stateless layer over raw Zenith keys:
  WASD and arrows resolve movement axes (opposites cancel), Enter/Space confirm,
  Escape/Backspace cancel, M/Tab menu, and either Shift runs. The controller,
  not the input reader, normalizes camera-relative diagonal movement. This keeps
  today's fixed mapping replaceable when rebinding eventually lands.
- **Player body and movement contract:** `ZM_PlayerController` is serialization
  order **102** and owns an upright dynamic generic Jolt capsule derived from
  the authored transform scale **(0.8,1.8,0.8)**. It drives camera-relative
  **horizontal-world speed** at **4 m/s walk / 7 m/s run**,
  rotates the visual transform toward travel, and writes an optional animator
  `Speed` parameter. Invalid or nonpositive dt is a true no-op for controller
  state, animation, body velocity and facing. Slopes through **45 degrees** are
  accepted and steeper-surface uphill drive is blocked. On a grounded walkable
  downslope, only the tangent-required downward velocity is added for adhesion;
  a stronger fall or positive step-assist rise is preserved. Step
  assist requires a lower obstruction, clear upper probe and walkable landing
  no more than **0.40 m** above ground, then applies one bounded upward-velocity
  assist. Gameplay motion never teleports the body with `SetPosition`.
- **Camera contract:** `ZM_FollowCamera` is serialization order **103** on the
  scene-owned main-camera entity. It captures the authored yaw (Dawnmere uses
  yaw 0), looks back to a player pivot, and follows through an omega-8 critically
  damped spring with a 5.5 m arm, 3.0 m camera height, 0.6 m pivot height and
  65-degree FOV. Physics collision clamps the arm with 0.2 m padding and a 1.0 m
  minimum before it springs back outward. It caches only the generation-bearing
  Player `EntityID`, validates both its generation and owning scene every late
  update, rejects a still-live cached target moved to another scene, and
  reacquires by the scene-local `Player` name after SINGLE reload; neither
  Player nor camera persists across that reload.
- **Dawnmere centre ruling and diagnosis:** author the Player centre at
  **(512,26.9,480)**. The baked terrain physics sample at the exact XZ is
  **Y=25.98577**, not the nominal Y=24 landmark value. With a **0.9 m** capsule
  half-extent, the original Y=24.9 centre began below the baked surface, so the
  1.05 m downward ground probe could never classify ground. At Y=26.9 the feet
  begin near Y=26.0 and the surface is about 0.914 m below centre, inside the
  probe. This is a deterministic generator-authored preview placement only;
  `ZM_SpawnPoint`/`ZM_WarpTrigger` will own semantic arrival tags next.
- **Tests that lock it:** exactly **20** new T0 tests in
  `ZM_Tests_Overworld.cpp`: **5 input / 4 controller / 5 live physics / 4 camera
  / 2 ECS-serialization**. The boot gate is **1757 ran / 1756 passed / 0 failed
  / 1 skipped**. The automated registry is now four: Boot and the asset-free
  `ZM_ControllerHarness_Test` pass headless; graphics-required Grass and the
  asset-guarded `ZM_DawnmerePlayerCamera_Test` skip headless as designed. The
  player/camera windowed integration passed in **117 frames / 4990.3 ms** and
  the focused grass lifecycle passed in **11 frames / 1924.3 ms**. All four
  Vulkan Debug/Release x Tools true/false builds and the D3D12 Debug Tools=false
  link proof are green.
- **Stage sequencing:** the next box is persistent `ZM_GameStateManager` plus
  `ZM_SpawnPoint`/`ZM_WarpTrigger`; PlayerHome and the door round trip follow.
  The human S3 visual gate is deliberately deferred until both are complete and
  the full S3 automated walk/door round-trip gate is green.
- **Reversibility:** all three seams are game-local and additive; both ECS
  components have version-1-empty serialized payloads. Speeds, camera geometry/spring values,
  step bounds and the generator-authored centre are isolated constants or
  authoring values. Rebinding can replace the action readers without changing
  controller policy. Persistence/warp work can replace the preview centre with
  spawn-tag placement without changing the body or camera contracts.

---

## 2026-07-13 -- ZM-D-054 -- Three-recipe terrain measurement validates the cropped per-scene plan

- **Decision:** close Roadmap's three-real-scene terrain-bake measurement and
  Q-2026-07-09-002. The fixed-order measurement registry contains the real
  WorldSpec terrain recipes Dawnmere (town, 16x16 / 256 chunks), Thornacre
  (town, 16x16 / 256 chunks), and Route1 (route, 16x24 / 384 chunks). Each has
  its own stable seed, authored plan, isolated asset-set name, exact output
  enumeration, atomic warm marker, and selected-force CLI path. These recipes
  continue the one-terrain-set-per-outdoor-scene requirement; no shared terrain
  sheet or bake-pipeline optimization is needed before the next S3 task.
- **Measured evidence:** calibrated direct process walls under the same harness
  were **59.035 s / 69.979 s / 80.804 s** for Dawnmere / Thornacre / Route1;
  the production recipe timers, which begin before reset and complete in the
  terminal action, reported **42.588 s / 53.657 s / 64.541 s**. Their families
  contain **256 / 256 / 384 chunks**, **772 / 772 /
  1,156 files**, and **204,684,116 / 204,684,116 / 262,985,940 bytes**. The
  three-sample total is **896 chunks, 2,700 files, and 672,354,172 bytes**.
  A separate all-warm same-harness boot took **16.874 s**, validated every
  family, and queued zero terrain recipes. ZM-D-053's original standalone
  Dawnmere observation (**63.671 s** cold, **14.614 s** warm graphics) remains
  historical evidence; 59.035 s is the later calibrated rerun, not a rewrite.
- **Projection:** a town wall mean of 64.507 s and Route1's 80.804 s give the
  11-town + 14-route planning model **24,676 files / 5,933,328,436 bytes**
  (5.933 GB / 5.526 GiB), a conservative repeated-process **30m 40.833s**, and
  a one-boot/net estimate of **23m 55.857s** after subtracting the shared
  16.874-second warm baseline per sample and adding it once. Scaling the
  internal timers and adding one baseline cross-checks at **24m 09.796s**. The
  GDD's exact 11-town + 15-route sensitivity is **25,832 files /
  6,196,314,376 bytes** (6.196 GB / 5.771 GiB), **32m 01.637s** repeated,
  **24m 59.787s** one-boot/net, and **25m 14.337s** by internal-timer
  cross-check.
- **Why:** both conservative terrain projections fall within the existing
  30-50 minute full-cold planning range, the net estimates are about 24-25
  minutes, and file volume matches the prior ~25k estimate. This resolves the
  risk sufficiently to continue S3. It does **not** prove the eventual
  all-assets cold bake: that 30-50 minute target includes the unbuilt S4 asset
  families, there is no explicit byte cap, and "seconds" warm is qualitative
  rather than a numeric SLA. The model has two towns but only one route, assumes
  later recipes stay in the 16x16 / 16x24 crop classes, and is a planning bound
  rather than a statistical confidence bound. The `~25` planning count and
  GDD's exact 26 outdoor scenes therefore remain explicit primary/sensitivity
  cases. Thornacre and Route1 have measured terrain families only: this decision
  adds no playable scenes, world connectivity, trees, or dressed content.
- **Tests that lock it:** five new `ZM_TerrainRecipeSet` units lock the exact
  three-row WorldSpec registry/order, distinct documented plans, deterministic
  contained plans ending with grass erase, unique contained output sets plus
  pure AUTO/FORCE_ALL/FORCE_SELECTED queue policy, and per-recipe marker counts
  with missing/empty-output invalidation. Regeneration, the default build, all
  four Vulkan Debug/Release x Tools true/false configurations, and D3D12 Debug
  Tools=false are green. The boot gate is **1737 ran / 1736 passed / 0 failed /
  1 skipped**; Zenithmon headless is **2/2** without terrain mutation. Each
  selected cold bake exited 0 and published its validated marker; the all-warm
  boot queued zero terrain recipes. The windowed grass regression still observes
  exactly **200,159 blades from 5,133 triangles** on both Dawnmere loads.
- **Reversibility:** the registry and projection assumptions are additive and
  can be extended or remeasured when later representative recipes exist.
  Recipe tuning may update measured estimates and requires either a forced bake
  or a manifest-version bump when the required-output count is unchanged; only
  a count change invalidates the existing marker automatically. Baked terrain
  remains ignored.
  If the eventual all-assets bake breaches budget, ZM-D-037 still requires an
  optimization pass (parallel/cached/profile-guided export), never shared
  terrain sets.

---

## 2026-07-13 -- ZM-D-053 -- Dawnmere deterministic terrain bake and scene-owned grass regeneration

- **Decision:** the S3 Home Village is the `Dawnmere` terrain set and scene. Its
  immutable `ZM_TerrainAuthoring` recipe derives seed `0x7BF32CA4` from the
  WorldSpec terrain-set name, authors within world coordinates `0..1024`, and
  exports inclusive chunks `(0,0)..(15,15)`. The bounded family is exactly 256
  `Render`, 256 `Render_LOW`, and 256 `Physics` meshes plus `Height.ztxtr`,
  `Splatmap_RGBA.ztxtr`, and `GrassDensity.ztxtr`.
- **Bake/warm contract:** a cold or forced boot invalidates the old completion
  state before queueing the deterministic recipe. Its terminal action requires
  all 771 non-empty outputs, writes a 12-byte little-endian marker (`ZMTR`,
  version 1, required-output count 771) to a temporary file, and atomically
  renames it to `ZM_TerrainRecipe.manifest`. The completed terrain family is
  therefore 772 files including the marker. Scene authoring waits until a later
  warm boot, which requires both the valid marker and every required output,
  then writes the ignored `Assets/Scenes/Dawnmere.zscen`. The observed cold bake
  was **63.671 s** and the observed warm graphics boot was **14.614 s**. This is
  one data point only; the Roadmap's three-real-scene bake-time measurement and
  extrapolation remain the next task.
- **Grass contract:** `ZM_TerrainGrass` is serialized on the Dawnmere terrain
  entity. OnAwake it owns and validates the exact 1024x1024 CPU density map;
  headless boots stop there without touching Flux. Graphics boots wait a bounded
  300 frames for terrain physics, reset prior grass state, apply density scale
  0.15, and generate from the terrain physics geometry. OnDestroy clears both
  the Flux instances and density map. The first load and same-process reload each
  regenerated and uploaded exactly **200,159 blades from 5,133 triangles** with
  no accumulation, and returning to FrontEnd left no Dawnmere grass. Trees are
  deliberately absent from this first terrain deliverable.
- **Why:** this is the first real consumer of E1/E2 and proves that a deterministic
  per-scene cropped terrain family can be baked, warm-gated, loaded, reloaded, and
  cleaned without committing generated assets or leaking renderer-owned grass.
  Deferring scene authoring to the warm boot prevents a partial cold bake from
  being serialized as a valid world scene.
- **Tests that lock it:** three `ZM_TerrainAuthoring` units lock recipe identity,
  bounds/order/determinism, output enumeration, marker contents, missing-output
  invalidation, and containment; one `ZM_Grass` unit locks density decoding,
  sampling, path construction, and failure clearing. The windowed
  `ZM_GrassRegeneration_Test` locks the first-load/reload counts and FrontEnd
  cleanup. Full boot gate is **1732 ran / 1731 passed / 0 failed / 1 skipped**;
  all four Vulkan Debug/Release x Tools true/false builds plus the D3D12 Debug
  Tools=false link proof pass. Zenithmon headless reports **2/2** (the graphics
  grass test is skipped as designed), CityBuilder is **45/45**,
  DevilsPlayground is **158/158**, and RenderTest windowed
  `EngineBootShutdownSmoke` + `TerrainEditorSmoke` both pass.
- **Reversibility:** recipe tuning is localized but requires a generator-version
  bump/rebake once downstream placement depends on it. The marker format is
  versioned and can evolve; old or incomplete markers already force a cold bake.
  Removing the scene-owned grass lifecycle would require a replacement that
  preserves headless safety, reload determinism, and teardown cleanup.

## 2026-07-13 -- ZM-D-052 -- Engine E2: bounded terrain export and terminal sparse-HIGH streaming

- **Decision:** `AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` exports
  inclusive chunk coordinates. Bounds are accepted only when
  `0 <= min <= max < 64` on both axes and the rectangle contains the required
  `(0,0)` LOW/physics anchor; invalid input is rejected without clamping,
  swapping, opening a standalone editor, cleanup, or output. A successful
  operation removes direct `.zmesh` files once from the E1-canonical target and
  writes exactly `Render`, `Render_LOW`, and `Physics` for each requested
  coordinate (`3 * area` files), preserving textures, unrelated files, nested
  directories, and sibling terrain sets. It crops files only: the fixed 4096 m
  terrain grid/density remain the deferred E6 limitation.
- **Shared format contract:** `Flux_TerrainExportRect` is the single signed,
  transactional bounds/enumeration contract used by tools, editor, and tests.
  `Flux_TerrainVertexLayout` is the single HIGH chunk element order/size/stride
  contract used by exporter and streaming reader; canonical HIGH chunks are
  exactly 4,225 vertices and 24,576 indices at a 28-byte stride.
- **Sparse-stream contract:** missing, truncated, malformed, wrong-layout, or
  incompatible HIGH sources are validated by a bounded non-asserting reader
  before any eviction, allocation, residency, dirty-state, or stats mutation.
  The first failure warns and latches `SOURCE_UNAVAILABLE`; later frames skip it
  without consuming the 32-source-probe budget. Successful uploads remain capped
  at 8 per frame. Terrain registration/regeneration resets the latch so a new bake
  retries, and existing LOW fallback/legacy behavior is unchanged.
- **Why:** full 64x64 bakes produce roughly 12k files per terrain and make the
  required one-set-per-outdoor-scene plan impractical. Rectangular file crops make
  S3 authoring measurable while terminal sparse streaming prevents expected holes
  from evicting valid data, thrashing disk, or starving valid chunks behind the
  frame attempt budget.
- **Tests that lock it:** exactly three engine tests cover inclusive counts and
  enumeration, invalid transactionality, four-argument automation and production
  routing; missing/wrong-layout source mutation/allocator safety; and the
  32-missing-then-valid terminal skip/no-retry schedule. Full boot gate is
  **1728 ran / 1727 passed / 0 failed / 1 skipped** (Zenithmon 1725 -> 1728;
  engine default 1075 -> 1078). Regeneration, all four Vulkan Debug/Release x
  Tools true/false builds, the D3D12 Debug Tools=false link proof, Zenithmon 1/1,
  CityBuilder 45/45, DevilsPlayground 158 tests, and RenderTest windowed
  `EngineBootShutdownSmoke` + `TerrainEditorSmoke` all pass.
- **Reversibility:** additive for authoring: the old full-grid APIs remain intact.
  Rect-authored terrain sets depend on sparse-stream tolerance, so removing it
  would require rebaking every set full-grid.

## 2026-07-12 -- ZM-D-051 -- Engine E1: serialized, contained per-terrain asset sets with staged editor bakes

- **Decision:** `Zenith_TerrainComponent` serialization v4 appends a terrain-set
  name. The empty name is the exact backward-compatible legacy layout
  (`<game-assets>/Terrain/`, with legacy textures under `Textures/Terrain/`);
  a named set resolves to `<game-assets>/Terrain/<Set>/` and co-locates its
  textures and chunk meshes there. Valid names use the ASCII grammar
  `[A-Za-z0-9][A-Za-z0-9_-]{0,63}`. Resolution requires strict component-wise
  containment, and invalid serialized v4 names fall back safely to legacy.
  All terrain load, collision, low/high-LOD streaming, regeneration, and the two
  CityBuilder state consumers now use the resolved per-component directory.
- **Editor/tool contract:** the terrain editor owns a staged asset-set value;
  changing it never live-reroutes initialized terrain. A successful full bake
  commits that value immediately before synchronous regeneration. Clean/different
  targets reload persisted data, while reopening the same dirty target resumes
  its CPU maps and undo state. Production cleanup canonicalizes root and target,
  refuses symlink/junction escape, is non-recursive, and removes only direct
  `.zmesh` files. `AddStep_TerrainSetAssetSet` copies its argument, validates and
  preflights the selected component transactionally, and stamps a fresh selected
  Terrain component so a later scene save persists the choice.
- **Why:** Zenithmon requires one isolated terrain asset family per outdoor scene,
  while existing games and v1-v3 scenes must retain byte-compatible legacy path
  semantics. Staging prevents an editor draft from invalidating live render or
  physics resources; strict path/canonical checks make the destructive bake step
  safe at its filesystem boundary.
- **Tests that lock it:** exactly seven new engine unit tests cover grammar and
  transactional setters; path boundaries and state propagation; move isolation;
  v4 prefix/round-trip/invalid fallback; exact v1-v3 compatibility; editor
  staging, cleanup isolation, default reload, and dirty-session resume; and
  automation argument ownership/action execution/component serialization. Full
  boot gate is **1725 ran / 1724 passed / 0 failed / 1 skipped** (Zenithmon
  baseline 1718 -> 1725; engine default 1068 -> 1075). The four Vulkan
  Debug/Release x Tools true/false builds and D3D12 Debug Tools=false link proof
  pass. `zenith test` passes for Zenithmon (1/1), CityBuilder (45/45), and
  DevilsPlayground (158 tests); RenderTest windowed `EngineBootShutdownSmoke` and
  `TerrainEditorSmoke` both pass.
- **Reversibility:** additive for existing content: empty/v1-v3 assets remain on
  the legacy layout. Removing named sets later would require retaining the v4
  reader or migrating every scene that stores a non-empty set.

## 2026-07-12 -- ZM-D-050 -- Breeding SC-C: egg moves + ability/hidden-ability inheritance + hatch cycles (FEATURE-COMPLETE BREEDING)

- **Decision:** the final sub-commit of the feature-complete-breeding expansion
  (ZM-D-048). Adds derived species abilities {regular, hidden}, egg moves, and a
  hatch-cycle accessor, and wires ability/hidden-ability + egg-move inheritance into
  `ZM_GenerateEgg`. **Breeding is now FEATURE-COMPLETE** (mainline mechanics: gender +
  ratios, real egg groups, GLOOPET Ditto-analog, gendered compatibility, IV
  [Heirloom Knot] + nature [Stasis Stone] inheritance, ability + hidden-ability
  inheritance, egg moves, hatch cycles). Cosmetic shiny/Masuda remains DEFERRED to
  S5+ (ZM-D-048).
- **Derived accessors (no stored columns):** `ZM_GetSpeciesAbilities(id)` ->
  `{m_eRegular, m_eHidden}` (distinct; regular from the types[0] ability pool at
  `familySeed%count`, hidden from the types[1]-or-archetype pool at
  `(familySeed>>5)%count`, distinctness fixup). `ZM_GetSpeciesEggMoves(id)` -> up to 6
  own-type moves not in the species' level-up learnset (legendary empty), id order.
  `ZM_GetSpeciesHatchCycles(id)` -> rarity base {10/15/25/40} + size class (0..4);
  data-only (overworld step-driving deferred to S9).
- **Inheritance in ZM_GenerateEgg:** (ability) if the mother carries HER species'
  hidden ability, the offspring gets its own species' hidden ability with probability
  `uZM_BREED_HIDDEN_INHERIT_PCT` (60%, a `Chance(60,100)` draw) else its regular;
  otherwise the offspring copies the mother's ability with NO draw. This hidden draw
  is APPENDED AFTER the gender draw (order IV -> nature -> gender -> [conditional
  hidden]) and fires ONLY for a hidden-carrying mother, so every pre-existing fixture
  (mother with a non-hidden ability + empty movesets) is byte-identical. (egg moves,
  RNG-FREE) after the base-evo L1 learnset fills the slots, each offspring egg move
  that EITHER parent knows fills the first empty slot, 4-cap, no eviction.
- **Notes:** the ability-derivation accessor is ALSO usable to give wild/tower mons a
  real ability (they currently use `ABILITY_NONE`) -- NOT retrofitted here to keep
  SC-C breeding-scoped; a small optional follow-up. Egg-move derivation uses "own-type
  moves minus level-up learnset" as a tractable, deterministic realization of
  "egg-group cohort minus learnset" (documented; a cohort shares the species' types).
- **Tests:** 20 new `ZM_Data` tests (ability derivation distinct/exact; ability
  inheritance regular-copy zero-draw lock-step + hidden ~60% over a fixed seed list +
  golden-seed IV/nature/gender non-perturbation; egg-move derivation
  disjoint-from-learnset + exact; egg-move inheritance placement/control/4-cap/RNG-free;
  hatch cycles). 0 existing tests edited (purely additive). Adversarial review:
  SAFE-TO-COMMIT (2 non-blocking notes: a loose hidden-rate distribution band on an
  otherwise well-covered mechanism, and exact-ability tests that pin the derivation
  mechanism not the pool contents -- both acceptable). Boot unit baseline 1698 -> 1718.
- **Reversibility:** additive; the ability pools, egg-move derivation, hidden-inherit
  rate (60%), and hatch-cycle formula are S11-tunable. **FEATURE-COMPLETE BREEDING is
  DONE:** ZM-D-045 (reduced) -> ZM-D-048 (gender) -> ZM-D-049 (egg groups / Ditto /
  gendered compat) -> ZM-D-050 (egg moves / abilities / hatch).

## 2026-07-12 -- ZM-D-049 -- Breeding SC-B: real egg groups + GLOOPET Ditto-analog + gendered compatibility

- **Decision:** part of the feature-complete-breeding expansion (ZM-D-048).
  Replaces the archetype-as-egg-group proxy with a real DERIVED egg-group
  taxonomy, adds a Ditto-analog, and makes breeding compatibility gender-aware.
- **Egg groups (DERIVED, no stored column):** `ZM_GetSpeciesEggGroups(id)` ->
  `ZM_EggGroups{ m_uCount, m_aeGroups[2] }` over `ZM_EGG_GROUP` (FIELD / HUMANOID /
  FLYING / DRAGON / WATER / BUG / AMORPHOUS / PLANT / MINERAL / FAIRY / NO_EGGS /
  UNIVERSAL). Legendary -> {NO_EGGS}. Else primary from archetype (QUADRUPED->FIELD,
  BIPED->HUMANOID, AVIAN->FLYING, SERPENT->DRAGON, AQUATIC->WATER, INSECTOID->BUG,
  BLOB->AMORPHOUS, FLOATER_PLANTOID->PLANT); secondary from the FIRST type slot that
  maps (GRASS->PLANT, DRAKE->DRAGON, FEY->FAIRY, PHANTOM->AMORPHOUS, WATER->WATER,
  STONE/IRON/EARTH->MINERAL, SKY->FLYING), added only if != primary -- if the first
  mapping slot equals the primary the species stays single-group (slot 1 is not then
  consulted).
- **Ditto-analog:** GLOOPET (F40, BLOB, NORMAL, COMMON, GENDERLESS) is the UNIVERSAL
  breeder via `ZM_IsUniversalBreeder`. It breeds with ANY non-legendary partner
  ignoring gender + egg group; the offspring follows the non-universal parent's line.
  Two universals, or universal + legendary, are incompatible.
- **Compatibility (gender-aware):** `ZM_AreSpeciesCompatible(a,b)` (species-level):
  not legendary, not both-universal, one-universal OR share an egg group.
  `ZM_AreCompatible(specA,specB)` (spec-level): species-compatible AND (universal OR
  exactly one MALE + one FEMALE). A GENDERLESS non-universal parent is never
  compatible (only a universal breeder can breed with a genderless species).
  `ZM_DaycarePairCompatible` is now gender-aware (delegates to `ZM_AreCompatible`).
- **Offspring parent role:** `ZM_GenerateEgg(A,B,...)` derives the mother =
  non-universal partner (if one side is universal) else the FEMALE parent; the mother
  supplies the base-evo offspring species, the copied ability, and the `RandBelow(2)
  ==0` IV source. RNG draw order/bounds are UNCHANGED (IV -> nature -> gender); only
  which parent each draw reads changes, so IV/nature goldens stay byte-identical
  wherever the female parent is the previous mother. A precondition assert fires on an
  invalid pair. Legacy `ZM_GetBreedingGroup` deleted (no-legacy mandate).
- **Tests:** 22 new `ZM_Data` tests (egg-group derivation incl. the primary-blocks-
  secondary single-group case, universal breeder, species + gendered compatibility,
  offspring parent role, daycare) + 24 existing breeding tests re-baselined to
  gendered/GLOOPET fixtures. The re-baseline is LEGITIMATE (verified by review): the
  female parent is set to the previous mother, so IV/nature golden VALUES are
  byte-identical AND still match the unchanged independent offline oracle -- only the
  parent-declaration lines changed. The genderless-blob test was rebuilt as
  GLOOPET x blob. Adversarial review: SAFE-TO-COMMIT (2 low findings closed pre-commit:
  a PUFFSEED single-group coverage assert + a stale-comment reword). Boot unit baseline
  1676 -> 1698.
- **Reversibility:** additive; the egg-group mapping + the GLOOPET designation are
  data-derivation choices, S11-tunable. SC-C (egg moves + ability/hidden-ability
  inheritance + hatch-cycle data) completes the feature.

## 2026-07-12 -- ZM-D-048 -- Feature-complete breeding + gender (user-directed; fulfilling the mainline breeding scope). SC-A gender foundation.

- **Decision (user, 2026-07-12):** breeding is completed to the FULL mainline
  feature set. This is NOT a scope widening -- Scope.md Section 1 already locks in
  "breeding/eggs/daycare" under "mainline MECHANICS only"; the box-6 SC1 model
  (ZM-D-045) was a documented REDUCTION (Q-2026-07-12-004) that under-delivered
  against that scope. The user directed full gender + feature-complete breeding, so
  the reductions are removed. **Resolves Q-2026-07-12-004.**
- **Confirmed boundary decisions (user, 2026-07-12):** (a) Ditto-analog = designate
  the existing genderless species **GLOOPET** as a UNIVERSAL breeder (breeds with
  anything; offspring follows the non-Ditto parent) -- no roster change; (b) hidden
  abilities = ADD a derived second (hidden) ability slot + inheritance (also gives
  wild mons a real derived ability for the first time); (c) shiny + Masuda method =
  DEFERRED to S5+ (cosmetic/display, no headless-battle effect; a clean append when
  the dex/UI exists) -- deferred, not cut; (d) egg hatch cycles = add a DERIVED
  hatch-cycle accessor/data now; the overworld step-driving to actually hatch lands
  at S9.
- **Delivery:** three test-first sub-commits on top of box-6 SC1 -- SC-A gender
  foundation (this entry), SC-B (real egg groups + GLOOPET Ditto + gendered
  compatibility + offspring = female/non-Ditto parent), SC-C (egg moves +
  ability/hidden-ability inheritance + the derived hatch-cycle accessor). All
  additive: new per-species data are DERIVED accessors (the 152 species const rows
  stay untouched), and new RNG draws APPEND after existing ones, so no existing
  golden shifts.
- **SC-A gender-foundation contract:** `enum ZM_GENDER { MALE, FEMALE, GENDERLESS }`
  + `enum ZM_GENDER_RATIO` (8 buckets) in `ZM_SpeciesData.h`; `m_eGender` appended
  LAST on `ZM_BattleMonsterSpec` + `ZM_BattleMonster` (default GENDERLESS,
  POD-append-safe). DERIVED accessors: `ZM_GetSpeciesGenderRatio` (LEGENDARY /
  BLOB-archetype -> GENDERLESS; RARE -> 7:1 male; else `(familySeed>>3)%4` spread
  over EVEN / 3:1M / 3:1F), `ZM_GenderRatioFemaleThresholdOutOf8` (7:1M->1, 3:1M->2,
  EVEN->4, 3:1F->6, 7:1F->7; fixed ratios -> NO_ROLL sentinel), `ZM_RollGender`
  (fixed ratios draw nothing; graded do one `RandBelow(8)`, female iff draw <
  threshold). `ZM_GenerateEgg` rolls the egg gender from the OFFSPRING ratio as the
  LAST rng draw (order IVs -> nature -> gender) so IV/nature goldens stay
  byte-identical; `ZM_GenerateTowerTeam` rolls gender in a SECOND pass so tower
  species/nature goldens stay byte-identical. `ZM_BuildBattleMonster` stays pure and
  copies gender. (Only GENDERLESS/MALE_7_1/EVEN/MALE_3_1/FEMALE_3_1 are reachable
  from the current 152 species; the other ratio buckets are intentional headroom.)
- **Tests:** 13 `ZM_Data` gender tests in `ZM_Tests_Breeding.cpp` (ratio derivation
  with premises self-guarded via `ZM_GetSpeciesData`, the threshold table, roll
  determinism + zero/one-draw lock-step + distribution bands, egg gender via an
  INDEPENDENT oracle, and a non-perturbation regression guard re-asserting the
  pre-gender IV/nature goldens). Adversarial review: SAFE-TO-COMMIT. Boot unit
  baseline 1663 -> 1676.
- **S7 forward note:** when party/daycare save-load is added at S7, the serializer
  MUST include `m_eGender` (nothing serializes these structs today -> no current
  regression).
- **Reversibility:** additive; the derivation rules + the hidden-ability inheritance
  rate + the ratio thresholds are S11-tunable. SC-B + SC-C complete the feature.

## 2026-07-12 -- ZM-D-047 -- S2 stage gate PASSED (all battle logic complete)

- **Decision:** the S2 stage gate is PASSED; S2 -- the complete deterministic
  headless battle system -- is DONE. All six S2 boxes landed (ZM-D-032..046):
  battle engine + append-only event stream (box 1), move/status/catch/switch
  executor (box 2), 50 abilities + weather (box 3), exp/EV/level/evolution
  (box 4), four-tier `ZM_BattleAI` (box 5), breeding/daycare + Battle Tower
  logic (box 6).
- **Gate evidence (2026-07-12, this session):**
  - Boot unit gate: **1663 ran / 1662 passed / 0 failed / 1 skipped** (the skip is
    the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). The suite
    includes the box-1 offline-oracle exact-event-stream scenario
    characterizations and the box-2 2,000-battle fuzz soak (termination < 500
    turns + HP/PP/boost invariants). Game-unit inventory: **209 `ZM_Data` + 384
    `ZM_Battle` + 2 `ZM_Boot`**; the Roadmap "~370 unit tests" target is exceeded.
  - Automated headless batch: `zenith test Zenithmon --headless` = 1/1
    (`ZM_Boot_Test`), exit 0.
  - Build matrix: the full 4-config Vulkan matrix
    (`Vulkan_vs2022_{Debug,Release}_Win64_{True,False}`) + the
    `D3D12_vs2022_Debug_Win64_False` null-backend link proof ALL build green.
  - No windowed or visual check is defined for the S2 gate, so no GATE-WAIT
    applies -- the S2 gate is fully automated and self-signed.
- **Reversibility:** n/a (a gate-results record). Next stage **S3** (first
  overworld) begins the engine-change (E1/E2 terrain) + terrain-bake +
  VISUAL-gate phase -- the user's standing hard-stop-at-visual-gates order
  resumes at the S3 gate.

## 2026-07-12 -- ZM-D-046 -- S2 box-6 SC2: `ZM_BattleTower` logic (BOX 6 COMPLETE)

- **Decision:** `ZM_BattleTower` ships as pure, deterministic, headless LOGIC in
  `Source/Battle/ZM_BattleTower.{h,cpp}` -- no globals/statics-with-state, no UI,
  and it makes NO engine calls itself. It PRODUCES clamped teams + an AI tier + a
  battle config and SETTLES a streak from a bool result; a CALLER drives the
  actual battle via `ZM_BattleEngine` + `ZM_ChooseAction`. This keeps the tower a
  pure logic layer, fully testable without a live battle.
- **Level-50 clamp:** `ZM_ClampSpecToTowerLevel` copies the spec, sets `m_uLevel
  = 50` + `m_uCurExp = UNSPECIFIED`, and preserves species/IVs/EVs/nature/ability/
  moves/override verbatim; the six stats recompute via the locked
  `ZM_BuildBattleMonster` path (>50 clamps down, <50 scales up, full-HP start).
  The original spec is never built, so an out-of-range input level cannot trip the
  build's [1,100] assert. `ZM_ClampPartyToTowerLevel` maps element-wise.
- **Streak -> difficulty:** `ZM_TowerBaseTierForStreak` = [0,7) RANDOM / [7,21)
  GREEDY / [21,35) SMART / [35,inf) CHAMPION (monotonic). Boss = `(streak+1) % 7
  == 0`. `ZM_TowerAITierForStreak` = base tier + ONE step on a boss, capped at
  CHAMPION (never COUNT/NONE). NOTE: the bumped AI tier is NOT globally monotonic
  -- it dips to the base tier after each interior boss (streak 13 -> SMART, 14 ->
  GREEDY); the header documents this. This RESOLVES an internal inconsistency in
  the spec's §5 "worked values" (which implied a two-tier jump) -- the code + tests
  use the ONE-tier rule (streak 20 -> SMART). Rarity ceiling rises COMMON ->
  UNCOMMON (>=7) -> RARE (>=21); never LEGENDARY.
- **Team gen (deterministic, LOCKED draw order):** eligible = dex in ascending id
  with `rarity != LEGENDARY && rarity <= ceiling`; per output slot (default 3):
  (A) species via `RandBelow(eligibleCount)` rejected until distinct from earlier
  slots, (B) one `RandBelow(ZM_NATURE_COUNT)` nature after the species; IVs 31,
  EVs 0, ability NONE, moves = up-to-four highest-level learnset entries at level
  <= 50. Per-battle seed = `run.m_ulSeed + 0x9E3779B97F4A7C15 * (streak+1)`. (43
  non-legendary COMMON species >> team size 3, and an `eligible >= count` assert
  guards it, so the distinct-pick loop always terminates.)
- **Advance/config:** `ZM_TowerAdvance` -- win increments current + raises the
  best-streak high-water; loss resets current to 0 (best preserved); returns the
  new current. `ZM_MakeTowerBattleConfig` -- level cap 50, trainer=true,
  wild/catch/flee=false, awardExp=false.
- **First consumer of ZM-D-044:** the tower is the first real consumer of
  `ZM_AI_TIER` / `ZM_ChooseAction`. FUTURE-INTEGRATION NOTE: `ZM_ChooseAction`
  returns a MOVE for a FAINTED active (it enumerates the fainted active's moves),
  so an S5+ battle-integration caller must submit the forced SWITCH itself on a
  faint rather than delegate that turn to the chooser (or box 5's chooser should
  later special-case a fainted active). The box-6 engine smokes side-step this by
  running a controlled 1v1 (the battle ends on the first KO). Flagged
  Q-2026-07-12-005. No production bug today (the tower never runs battles).
- **Tests that lock it:** 25 tests in `Tests/ZM_Tests_BattleTower.cpp` -- 23
  `ZM_Data` (L50-clamp stat-parity via the build oracle, streak/boss/tier band
  boundaries + one-tier bump, rarity bands, team-gen goldens via an INDEPENDENT
  §7 oracle + legality, advance win/loss/best, config fields) + 2 `ZM_Battle`
  engine round-trip smokes (a real 1v1 tower battle to termination, no exp
  awarded). Adversarial review: SAFE-TO-COMMIT (one non-blocking header-comment
  inaccuracy fixed -- the AI tier is bounded but not monotonic). Boot unit baseline
  1638 -> 1663 (`zm-tests.yml`).
- **Reversibility:** additive + standalone; all numeric knobs (7/21/35 tier
  thresholds, team size 3, rarity bands, the exp-off + NONE-ability team defaults)
  are S11-tunable named constants with zero API/golden impact if retuned. **BOX 6
  COMPLETE** (SC1 breeding/daycare = ZM-D-045 + SC2 tower = ZM-D-046). All S2
  battle-logic boxes (1-6) are now done; next is the S2 automated stage gate.

## 2026-07-12 -- ZM-D-045 -- S2 box-6 SC1: `ZM_Breeding` + `ZM_Daycare` (reduced deterministic breeding on the shipped data)

- **Decision:** `ZM_Breeding` + `ZM_Daycare` ship as pure, deterministic, headless
  free functions in `Source/Battle/ZM_Breeding.{h,cpp}` + `ZM_Daycare.{h,cpp}` --
  no globals/statics-with-state, no UI, no overworld dependency (that integration
  is later). All stochastic behavior flows through a caller-supplied
  `ZM_BattleRNG`; the daycare step/egg model is fully RNG-free.
- **Reduced model on shipped data (no new columns):** the species table carries
  NO egg-group, gender, hatch-cycle, egg-move, or species->ability data. So the
  **archetype** field is the egg-group proxy; there is **no gender** (first parent
  = "mother" by convention); ability is copied from the mother; egg moves = the
  base-evolution's level-1 learnset. Faithful reductions, not cuts of in-scope
  features -- logged Q-2026-07-12-004; each is additive if the data model later
  grows the field (adding gender/egg-groups is a Scope.md data-model expansion).
- **Compatibility:** both parents non-legendary AND sharing an archetype.
- **Offspring species:** the base (lowest) evolution of the MOTHER, derived by a
  roster-bounded backward scan over the shipped forward `m_eEvolvesTo` chain (no
  new pre-evo column; terminates on any cyclic/malformed table). Sentinel-safe:
  `ZM_SPECIES_NONE == ZM_SPECIES_COUNT`, so an index-0 predecessor is not mistaken
  for "not found."
- **Inheritance (LOCKED RNG draw order -- goldens depend on it):** (A) species
  (no draw); (B) IVs -- K = Heirloom-Knot?5:3; phase 1 picks K distinct stat
  indices via `RandBelow(6)` (reject dups); phase 2 iterates stats 0..5, each
  inherited stat draws `RandBelow(2)` for the parent, each fresh stat draws
  `RandBelow(32)`; (C) nature -- Stasis-Stone (everstone) locked value with NO
  draw, else `RandBelow(25)`, ALWAYS AFTER the IV rolls so the everstone can't
  shift IVs; (D) ability = mother's; (E) moves = base-evo L1 learnset; (F) level
  1, EVs 0, `m_uCurExp = uZM_EXP_UNSPECIFIED`. `ZM_ITEM_HEIRLOOMKNOT` /
  `ZM_ITEM_STASISSTONE` are the destiny-knot / everstone analogs (already in
  `ZM_ItemData`), passed via `ZM_BreedingParams`.
- **Daycare:** capacity 2; `Deposit` fills the first free slot (returns the index,
  or capacity when full) and normalizes an UNSPECIFIED exp to the level floor;
  `Step` gives each occupied slot 1 exp/step (level via `ZM_LevelForExp`, capped
  L100, 64-bit intermediate -- no wrap) and advances the egg counter by the step
  count ONLY for a compatible occupied pair, saturating at the 256-step threshold
  where `bEggAvailable` flips true (deterministic, no RNG); `Withdraw` returns the
  leveled spec, clears the slot, resets egg progress; `CollectEgg` calls
  `ZM_GenerateEgg` and resets the counter/flag, leaving parents deposited.
- **Tests that lock it:** 33 `ZM_Data` behavioral tests in
  `Tests/ZM_Tests_Breeding.cpp` -- base-evo derivation (+ an all-species
  terminates-at-stage-1 invariant), compatibility (premises self-guarded via
  `ZM_GetSpeciesData`), exact IV/nature goldens from an INDEPENDENT offline PCG32
  oracle (proving draw order: same-seed identity, knot 5-vs-3, everstone-does-not-
  shift-IVs), mother-ability / base-evo-learnset, and the daycare
  deposit/step/level/255-vs-256-egg-boundary/withdraw/collect model. Adversarial
  review: SAFE-TO-COMMIT (independent oracle guards draw order; base-evo sentinel
  + daycare saturation verified). Minor non-blocking coverage note: no test steps
  a compatible pair strictly PAST 256 in one call (the `>=` branch is covered by
  exact-256 steps; impl saturates correctly). Boot unit baseline 1605 -> 1638
  (`zm-tests.yml`).
- **Reversibility:** additive + standalone; the reduced-data rulings (gender, egg
  groups, egg moves, hidden abilities) are all additive if the data model grows;
  thresholds (256 steps, K=3/5) are named constants. SC2 (`ZM_BattleTower`, which
  consumes `ZM_BattleAI`) completes box 6. No golden/locked-contract impact.

## 2026-07-12 -- ZM-D-044 -- S2 box-5: `ZM_BattleAI` four-tier pure chooser (BOX 5 COMPLETE)

- **Decision:** `ZM_BattleAI` ships as a pure, side-effect-free action chooser in
  `Source/Battle/ZM_BattleAI.{h,cpp}`: `ZM_BattleAction ZM_ChooseAction(const
  ZM_BattleState& xState, ZM_SIDE eSide, ZM_AI_TIER eTier, ZM_BattleRNG& xAIRng)`.
  It reads `xState` through `const&`, takes its OWN caller-supplied `xAIRng`
  (distinct from `xState.m_xRNG`), never calls Submit/Resolve/DoSwitch/
  MoveExecutor, emits no events, mutates no state, and never advances the battle
  RNG. Only the RANDOM tier draws (from `xAIRng`). => choosing an action perturbs
  nothing; all box-1..4 golden streams + subsequent RNG output stay byte-identical.
  The chooser is standalone this box; wiring the engine to call it for the
  opponent side lands when battle integration needs it (S5+).
- **`ZM_AI_TIER` enum** (RANDOM/GREEDY/SMART/CHAMPION + COUNT, NONE=COUNT
  sentinels) is defined in `ZM_BattleAI.h`; the locked `ZM_BattleTypes.h` is NOT
  edited (its reserved comment stays a comment).
- **Legal-action set (deterministic order):** each move slot with PP>0, then
  SWITCH to each living non-active bench member. No ITEM/RUN. Assumes >=1 legal
  action (no Struggle fallback -- pre-existing engine gap, tracked Q-2026-07-12-003).
- **Tier contracts:** RANDOM = uniform over the ordered legal set via
  `xAIRng.RandBelow(n)`. GREEDY = argmax of deterministic `damage(roll=92,
  no-crit, STAB + type-effectiveness) x hit%`, tie-break lowest slot; status /
  zero-power / immune moves score 0; switches only when no legal MOVE exists.
  SMART = fixed cascade: (1) guaranteed KO -- a sure-hit (ALWAYS_HITS or base
  acc>=100) damaging move whose worst-case `roll=85` damage >= target curHP,
  best GREEDY score wins; (2) hopeless switch when `effIn>=200 && effOut<=100`,
  to the switchable bench member with the strictly-smallest incoming
  effectiveness; (3) heal when `curHP*2 < maxHP` and a HEAL_HALF/REST move has
  PP; (4) else GREEDY. CHAMPION = deterministic 2-ply on scalar HP (no
  `ZM_BattleState` clone): model the opponent's single GREEDY reply once, then
  per candidate own move resolve order by priority bracket then effective speed
  (SPEED stage-scaled, /4 if paralyzed -- mirrors `ZM_BattleEngine`), exact
  speed-tie modeled opponent-first (conservative); apply deterministic mean
  damage to two scalars (a KO suppresses the reply); score
  `V = (oppFaint?+100000:0) - (meFaint?+100000:0) + hpMe - hpOpp`; pick max V,
  tie-break GREEDY score then lowest slot. Beats the naive greedy line in the
  priority-trap (slower AI, both moves KO, only the +priority move wins).
- **File-location deviation:** placed under `Source/Battle/` (co-located with the
  nine sibling battle systems), NOT MasterPlan's `Source/AI/` -- no `Source/AI/`
  dir exists and one file does not justify one; trivial + additive to relocate.
  Logged Q-2026-07-12-003.
- **Tests that lock it:** 28 `ZM_Battle` behavioral tests in
  `Tests/ZM_Tests_BattleAI.cpp` (4 RANDOM, 6 GREEDY, 8 SMART, 6 CHAMPION, 4
  API/contract), each pinning a specific action with oracle-computed construction
  preconditions (from the real `ZM_CalcDamage`/`ZM_EffectivenessPercent`/
  `ZM_ApplyStatStage`). Includes a non-perturbation guard (snapshots the battle
  RNG by value, asserts 8 identical post-call draws), a CHAMPION reply-model
  discrimination test (3-way opponent-move separation so a slot-0 / max-power
  reply model fails), and a CHAMPION exact-speed-tie test. Adversarial review:
  SAFE-TO-COMMIT. Boot unit baseline 1577 -> 1605 (`zm-tests.yml`).
- **Reversibility:** additive + standalone; the SMART thresholds and the GREEDY
  roll are S11-tunable named constants; the engine-integration call site is a
  later separate change. No golden or locked-contract impact.

## 2026-07-12 -- ZM-D-043 -- S2 box-4: modern party-share EXP / EV / level / move-learn / terminal evolution (BOX 4 COMPLETE)

- **Decision:** `ZM_ExpAndLevel` ships as deterministic, integer-only progression in
  `Source/Battle/ZM_ExpAndLevel.{h,cpp}`, integrated behind default-false
  `ZM_BattleConfig::m_bAwardExp`. The award-off path changes no progression or
  participation state, draws no RNG, and emits none of `EXP_GAINED`, `LEVEL_UP`,
  `MOVE_LEARNED`, or `EVOLUTION_QUEUED`; legacy streams and subsequent RNG output
  remain byte-identical.
- **Curve + derived-data contract:** FAST=`4L^3/5`, MEDIUM_FAST=`L^3`,
  MEDIUM_SLOW=`6L^3/5 - 15L^2 + 100L - 140` clamped non-negative, and
  SLOW=`5L^3/4`; every curve has L1=0 and caps at L100. Growth derives from rarity
  (COMMON/UNCOMMON/RARE/LEGENDARY -> FAST/MEDIUM_FAST/MEDIUM_SLOW/SLOW). Base EXP
  yield is `max(1, BST*2/5)`. EV yield targets the species' highest base stat
  (ties use the lowest `ZM_STAT` index) for 1/2/3 points by evolution stage.
  Level evolution derives as stage-1 -> stage-2 at L16, stage-2 -> stage-3 at L36,
  final -> none. These remain S11-tunable accessors, not new species-table columns.
- **Award + modern party-share contract:** gross wild EXP is
  `max(1, floor(baseYield*defeatedLevel/7))`; trainer battles apply
  `floor(gross*3/2)` after the `/7` division. Each defeated opponent keeps its own
  bitset of opposing slots that were active against it. Every living participant
  receives an independent full gross award; every living nonparticipant receives
  `max(1, floor(gross/2))`. There is no shared pool or remainder. Fainted
  recipients receive neither EXP nor EV. Award order is participants by ascending
  slot, then nonparticipants by ascending slot. The side mask defaults to
  player-only but can explicitly enable another side.
- **EV contract:** every living recipient receives the defeated species' full EV
  yield even when its EXP share is half. EVs normalize deterministically in
  HP/Atk/Def/SpA/SpD/Spe order, with 252 per-stat and 510 total caps. EV mutation
  happens before EXP/level stat recomputation; a level-capped recipient still gains
  EVs when no EXP can be credited.
- **Current EXP + level contract:** `ZM_BattleMonsterSpec::m_uCurExp` uses an
  explicit UNSPECIFIED sentinel. Omitted input derives the declared level's curve
  floor; supplied values clamp into that level's cumulative-EXP band. Awards clamp
  to the configured cap (`0` = natural L100), emit the credited amount and new
  cumulative total, process every crossed level, recompute all six stats from the
  override-aware base stats, and preserve missing HP instead of fully healing.
- **Move-learning ruling:** each crossed level processes its learnset entries. A
  new move fills the first empty slot with full table PP and emits `MOVE_LEARNED`;
  already-known moves are silent. If all four slots are full, the move is skipped
  with no replacement, pending-choice state, or event. Interactive replacement is
  S5/S6 presentation work.
- **Evolution ruling:** box 4 supports level-trigger evolution only. A monster that
  levelled during battle queues at most one immediate evolution edge during terminal
  settlement, after `TURN_END` and directly before `BATTLE_END`; species never
  mutates mid-battle. Pure `ZM_Evolve()` applies one eligible edge, reloads target
  base stats, recomputes stats, preserves missing HP, and clears transient queue and
  levelled flags. Item/stone evolution is deferred to S9; trade evolution is out of
  scope; friendship evolution is unspecified. No generic trigger schema is reserved.
- **Faint/event ordering:** direct/recoil/contact-result faints are swept after the
  move phase; weather/status/volatile/ability-chip faints are swept after end-of-turn
  damage and abilities but before `TURN_END`. Per-defeated-monster credit state makes
  both sweeps exactly-once. Existing event ordinals and the event POD are unchanged.
- **Tests-that-lock-it:** **67** new tests (**45 `ZM_Data` + 22 `ZM_Battle`**) cover
  literal curve vectors/inverses, hostile bounds, derived accessors, wild/trainer
  awards, EV caps/normalization, current-EXP bands, single/multi-level restats and HP
  carry, move learning, pure one-edge evolution, exact direct and EOT faint streams,
  per-opponent ledgers, recipient order, fainted recipients, EV-before-restat and
  EV-at-cap, reserve move use, ledger reset, terminal evolution/requeue, exactly-once
  credit, and award-off stream/state/RNG identity. Local evidence: regen GREEN;
  Vulkan Debug Tools=True build GREEN; boot baseline **1510 -> 1577** = **1577 ran /
  1576 passed / 0 failed / 1 skipped**; headless automated suite **1/1**; adversarial
  production and test re-reviews GREEN.
- **Reversibility:** additive but golden-sensitive. The default-false gate keeps
  removal localized, but curve formulas, sharing semantics, recipient/event order,
  and payloads now define deterministic goldens and require a new decision if changed.

## 2026-07-12 -- ZM-D-042 -- S2 box-3 SC5: turn-end / faint / quickdraw abilities + all-50 gate (BOX 3 COMPLETE)

- **Decision:** SC5 is the FINAL box-3 slice -- it installs the last **9** ability
  rows (34, 35, 36, 41-46), closing all 50. Roster + slots:
  - **TURN_END heals (`pfnOnTurnEnd`):** RAINBASK(41)/SUNBASK(42)/ICEBOUND(43) heal
    maxHP/16 in RAIN/SUN/SNOW; ROOTFEED(45) unconditional maxHP/16; TOXICTHRIVE(44)
    maxHP/8 while POISON/TOXIC AND its poison chip is skipped.
  - **FAINT branch:** BLOODRUSH(34) `pfnOnDealtFaint` -> own ATTACK +1 on downing a
    foe; LASTSPITE(35) `pfnOnContact` bSelfFainted -> attacker used-move PP->0;
    AFTERSHOCK(36) `pfnOnContact` bSelfFainted -> chip attacker maxHP/4 [+FAINT].
  - **QUICKDRAW(46):** engine-side 30% -> +1 move-order priority; row stays `{}`
    (MODIFY_STAT realized engine-side -- the sole all-null-pfn row by design).
- **Orchestrator rulings (resolved the spec's flagged ambiguities):** (1) **BLOODRUSH
  fires on ANY damaging KO** by the holder's move (contact OR non-contact), not
  contact-only -- matches the FAINT-mask semantics + GDD "downs a foe"; guarded on
  (defender fainted this hit) AND (attacker still alive) so recoil/counter self-KOs
  don't trigger it. (2) **QUICKDRAW emits `ABILITY_TRIGGER` on a successful proc**
  (consistency + testability; the draw is gated on a live holder so NONE goldens are
  unperturbed). (3) A non-serialized **`m_uOtherMoveSlot`** field was added to
  `ZM_AbilityContext` (a view field, NOT the event POD -> save format unaffected) so
  LASTSPITE can identify the attacker's used move.
- **Seams:** a file-local `g_DispatchTurnEndAbilities` free fn (behavior-equivalent to
  a member method; keeps `ZM_BattleEngine.h` out of scope) dispatched at **EoT step 6**
  -- AFTER both PLAYER-then-ENEMY status ticks, BEFORE the final `TURN_END`, skipping
  fainted actives, zero RNG; the QUICKDRAW proc in `ResolveMovePhase` (gated on a live
  holder, PLAYER-then-ENEMY, move-ORDER only -- flee/`g_EffectiveSpeed` untouched); a
  new `g_ApplyDealtFaintReaction` after both contact sites in `ZM_MoveExecutor`; and an
  ability-gated TOXICTHRIVE poison-chip skip in `ZM_StatusLogic::EndOfTurn` (TOXIC ramp
  counter frozen while thriving).
- **Heal + weather-chip encoding:** each heal emits `ABILITY_TRIGGER` then `HEAL`
  (`m_iAmount`=heal, `m_iAux`=new HP); a full-HP holder emits nothing; heal is min 1,
  capped to missing HP. The LOCKED EoT order means a SAND/SNOW chip (maxHP/8 for a
  non-immune active; SNOW-immune = ICE) lands FIRST, then the heal -- so an ICEBOUND
  holder in SNOW nets `-maxHP/8 + maxHP/16`; the heal test now models and asserts that
  chip-then-heal ordering (a review-caught test-fixture bug that assumed heal-only was
  fixed before landing; production was correct).
- **Contracts preserved:** zero-perturbation for NONE actors -- the ~260 box-1/SC1-SC4
  goldens stay byte-identical (confirmed at the gate); `ZM_BattleEvent` POD append-only
  (no new kind/field -- reuses HEAL/ABILITY_TRIGGER/STAT_STAGE_CHANGED/FAINT); RNG draw
  order + EoT order unchanged. Independently adversarially reviewed: production correct
  across all 9 abilities + all 4 new seams, zero-perturbation audit PASS, the all-50
  gate genuinely proves realization (not vacuous), no false-confidence tests.
- **Tests-that-lock-it:** **19** net-new unit tests (boot baseline **1491 -> 1510**,
  bumped in `.github/workflows/zm-tests.yml`) -- 18 behavioral (per-ability
  positive+control incl. full-HP/min-1/cap-to-missing heal edges, the fainted-active
  guard, TOXICTHRIVE net-heal + chip-skip, BLOODRUSH contact AND non-contact KO + the
  +6 cap + recoil-self-KO guard, LASTSPITE/AFTERSHOCK bSelfFainted, QUICKDRAW
  proc/order/flee-unaffected, a NONE-actor zero-draws/zero-events invariant, and a
  2,000-battle ability+weather soak) + the **all-50 realization gate** (3 coverage
  tests: every row realizes its declared mask, the 6+20+15+9 SC sets partition all 50
  exactly once, and QUICKDRAW is the SOLE all-null-pfn row).
- **BOX 3 COMPLETE.** All 50 ability rows shipped across SC1-SC5; the Roadmap S2 box-3
  line is ticked. Reversibility: additive -- SC5 only APPENDED, so earlier goldens
  never shifted.

## 2026-07-11 -- ZM-D-041 -- S2 box-3 SC4: contact / status-try / stat-veto / accuracy abilities (15 rows)

- **Decision:** SC4 installs the next **15** ability rows as live hook
  function-pointers, reusing the SC2/SC3 `ZM_AbilityHooks` / `ZM_AbilityContext`
  / 50-row table. Rows and slots:
  - **CONTACT (`pfnOnContact`, rows 7-10):** STATICVEIL 30% -> paralyze attacker,
    CINDERSKIN 30% -> burn, BARBSKIN 30% -> poison, THORNMAIL -> chip attacker
    **maxHP/8** (min 1, NO RNG draw).
  - **STATUS_TRY (`pfnPreventMajor` / `pfnPreventVolatile`, rows 28-33):** WAKEFUL
    (SLEEP), PUREBLOOD (POISON+TOXIC), THAWHEART (FREEZE), LIMBERLITHE (PARALYSIS),
    COLDBLOOD (BURN), OWNPACE (CONFUSED volatile).
  - **stat-drop-veto (`pfnPreventStatDrop`, rows 25/48/26):** IRONWILL + GUARDIAN
    veto ANY foe-sourced stat DROP; KEENEYE vetoes ONLY a foe ACCURACY drop.
  - **ACCURACY (`pfnBypassAccuracy`, rows 27/49):** DEADAIM always auto-hits;
    TRUESHOT auto-hits only when weather != NONE.
  The three FAINT-branch abilities (BLOODRUSH 34, LASTSPITE 35, AFTERSHOCK 36)
  are deferred to **SC5** (spec check corrected an earlier assumption that put them
  in SC4).
- **Seams:** a new **E4 `g_ApplyContactReactions`** in `ZM_MoveExecutor::Execute`
  (fires once per move, only for a contact move that connected, skips if the
  attacker is already fainted [R-C2], dispatches only the DEFENDER's `pfnOnContact`
  -- the attacker-side `pfnOnDealtFaint` is SC5); an **M5 accuracy-bypass wrapper**
  around the existing accuracy check (the pre-existing draw is only WRAPPED, not
  reordered, so a NONE actor's `<100`-acc draw is byte-identical and the
  `>=100`/ALWAYS_HITS short-circuit is untouched); a new **state+events `ApplyMajor`
  overload** in `ZM_StatusLogic.{h,cpp}` (parallel to SC2's `ApplyStatChange`
  overload) so contact abilities status the attacker from an ability context. The
  stat-drop veto seam was already live (SC2) -- SC4 only adds the three predicate bodies.
- **R-S1 -- refinement of ZM-D-036 §4.5 (orchestrator ruling):** the STATUS_TRY
  veto+emit lives **inside** `ApplyMajor` / `ApplyVolatile`, with
  `CanApplyMajor` / `CanApplyVolatile` kept **pure** -- NOT in the predicates as
  §4.5 read literally. Rationale: `CanApplyVolatile` is ALSO the secondary-effect
  preflight in `g_ApplyDamagingSecondary`, so a predicate-level veto would suppress
  an OWNPACE holder's confuse-proc RNG draw, breaking the locked roll-then-veto
  order and the NONE-ability goldens. The chosen placement is behavior-equivalent
  to §4.5's intent (Resolution 4) and mirrors the SC2 `ApplyStatChange` seam.
- **R-C1 (event encoding):** Thornmail reuses `ABILITY_TRIGGER`'s free fields
  (`m_iAmount` = chip, `m_iAux` = attacker new HP, `m_iTag` = THORNMAIL) + a trailing
  `FAINT` iff lethal -- no new event kind. A STATUS_TRY block emits `ABILITY_TRIGGER`
  (blocked-status ordinal in `m_iAmount`) and NO `STATUS_APPLIED`; a stat-drop veto
  emits `ABILITY_TRIGGER` with `m_iAmount=0`; an accuracy bypass emits NOTHING
  (observable only as auto-hit).
- **Contracts preserved:** zero-perturbation for NONE actors -- RNG sequence and
  events byte-identical, so the ~260 box-1/SC1-SC4 goldens stay green; each contact
  proc is exactly one `RandBelow(100) < 30` per contact hit for a LIVE proc ability;
  `ZM_BattleEvent` POD stays append-only; RNG draw order + EoT order untouched.
  Independently adversarially reviewed: no production/test defects, zero-perturbation
  audit PASS, no false-confidence tests, coverage gates correct.
- **Tests-that-lock-it:** **15** behavioral battle tests (paired identical-seed
  positive+control, stochastic seeds picked by a PCG32 oracle, `0xB32A`-`0xB365`)
  plus the two coverage-invariant gates advanced SC3 -> SC4 (all 15 slots realize
  their declared masks; `pfnOnTurnEnd` / `pfnOnDealtFaint` and BLOODRUSH/LASTSPITE/
  AFTERSHOCK stay asserted null for SC5). Boot baseline **1476 -> 1491** (bumped in
  `.github/workflows/zm-tests.yml` this commit).
- **Reversibility:** additive. The Roadmap S2 box-3 line stays UNCHECKED until SC5
  installs the turn-end / faint / quickdraw abilities and adds the all-50
  complete-realization gate; SC4 only APPENDS so earlier goldens never shift.

## 2026-07-11 -- ZM-D-040 -- S2 box-3 SC3: damage / stat / type-interaction abilities (20 rows)

- **Decision:** SC3 installs the next **20** ability rows as live hook
  function-pointers, reusing the SC2 `ZM_AbilityHooks` aggregate / `ZM_AbilityContext`
  / 50-row table unchanged. Rows and the hook slot each realizes:
  VERDANTSURGE / EMBERSURGE / TIDALSURGE / HIVESURGE -> `pfnModifyDamageDealt`
  (own STAB-type move **x3/2** at **<=1/3 HP**, type-gated); SKYWARDGRACE /
  AQUIFER / DYNAMO / GRAZER -> `pfnTypeInteraction` (return 1 = immune,
  2 = absorb + heal **maxHP/4**); CINDERDRINK -> BOTH `pfnTypeInteraction`
  (fire absorb-heal) AND `pfnModifyDamageDealt` (own FIRE **x3/2**, no HP gate);
  BEDROCK / BLUBBER / SOLIDCORE / HEAVYPLATE / GOSSAMER / DOWNDRAFT ->
  `pfnModifyDamageTaken` reducers (SOLIDCORE x3/4 on super-effective;
  HEAVYPLATE/GOSSAMER x2/3 on PHYSICAL/SPECIAL; BLUBBER x1/2 FIRE|ICE;
  DOWNDRAFT x1/2 SKY; BEDROCK per its SC-earlier body); SUNCHASER / STREAMLINE /
  GRITSTRIDE / RIMESTRIDE / FERVOR -> `pfnModifyStat` (weather/status-gated stat
  boosts; FERVOR = ATTACK x3/2 while statused).
- **Seam placement (ZM-D-036):** ability damage mods apply AFTER `ZM_CalcDamage`
  (crit/effectiveness/weather/screen/burn already baked in), dealt-then-taken,
  with a `>=1` floor so a resist ability never zeroes a nonzero hit.
  `pfnTypeInteraction` resolves on the DEFENDER before the chart-immunity gate
  and before any crit/roll draw. The move-order speed seam
  (`g_EffectiveSpeedForMoveOrder`) affects the equal-priority ORDER branch ONLY;
  `DoRunAction`/flee keeps raw `g_EffectiveSpeed` (ZM-D-036(c)).
- **Contracts preserved:** no hook draws RNG -> a NONE-ability actor's stream is
  byte-identical; `ZM_BattleEvent` POD, RNG draw order, and EoT order are
  untouched (append-only discipline holds). The 4 weather-speed abilities realize
  their WEATHER bit engine-side (weather read inside the modify-stat body), not as
  a pfn (matches ZM-D-036(a)).
- **Tests-that-lock-it:** SC3 adds **30** `ZM_*` unit tests (boot baseline
  **1446 -> 1476**, bumped in `.github/workflows/zm-tests.yml` this commit) --
  the SC3 mask-realization + installation-state invariants (all 20 masks fully
  realized; every live slot maps to a declared bit) plus **20** behavioral
  battle tests, each a paired identical-seed positive+control (ability on vs off /
  wrong type -> exact delta, correct trigger count, RNG unperturbed). FERVOR is
  pinned by stat-INPUT equivalence (a x3/2-boosted twin), not formula
  re-derivation; the four absorb abilities drive the `ZM_Box3SC3AssertAbsorb`
  helper (previously dead scaffold, now wired -- no dead SC3 scaffolding remains).
  Reviewed by two independent read-only passes: production 20/20 correct with no
  bugs; the missing-behavioral-test gap was caught and closed before landing.
- **Reversibility:** additive. The Roadmap S2 box-3 line stays UNCHECKED until
  SC5 installs the remaining ability rows and adds the all-50 complete-realization
  check; earlier goldens never shift because SC3 only APPENDS.

## 2026-07-11 -- ZM-D-039 -- S2 box-3 SC2: ability-hook infrastructure + SWITCH_IN abilities

- **Decision:** The executable ability surface is a plain-function-pointer
  `ZM_AbilityHooks` aggregate with **12 slots over the existing 11
  `ZM_ABILITY_HOOK` bits**, plus a compiled `const` **50-row** table keyed by the
  stable `ZM_ABILITY_ID`. `ZM_AbilityContext` is a non-owning view of the
  engine-owned battle state, append-only event sink, owner side, and opposing
  side. `ZM_GetAbilityHooks` returns the stable row for every real id and safely
  returns `nullptr` for `ZM_ABILITY_NONE` or any out-of-range id; callers never
  feed the sentinel into the asserting metadata accessor. SC2 installs only the
  six SWITCH_IN rows (DAUNTINGROAR, the four `*CALLER`s, and PRESSUREAURA); the
  remaining slots stay null until their ordered SC3-SC5 implementations.
- **Switch-in contract:** ability dispatch runs immediately **after** the
  corresponding `SWITCH_IN` event. `Begin` resolves PLAYER lead + hook, then
  ENEMY lead + hook; `DoSwitch` uses the same post-event dispatch. RAINCALLER,
  SUNCALLER, SANDCALLER, and SNOWCALLER set their weather for exactly five
  turns, emit `WEATHER_CHANGED` then `ABILITY_TRIGGER`, and treat an already-
  identical weather as a strict no-op (no refresh and no events). Later
  switch-ins overwrite a different weather and preserve the previous-weather
  event tag.
- **Daunting Roar / Pressure Aura contract:** Daunting Roar lowers the opposing
  active's ATTACK through the promoted shared
  `ZM_StatusLogic::ApplyStatChange` clamp/event seam, then announces its
  `ABILITY_TRIGGER`; the `bFromFoe` parameter reserves the SC4 stat-drop-veto
  hook without changing NONE-ability behavior. A move targeting the opposing
  PRESSUREAURA holder costs **2 PP**, clamped at zero. SELF and FIELD targets,
  and opponent-target moves against a NONE-ability defender, continue to cost
  exactly 1 PP. Pressure Aura announces on switch-in rather than adding a
  per-move event.
- **Determinism + staged coverage:** SC2 adds **zero RNG draws**. A NONE ability
  still adds zero events, draws, or state changes, and the pre-box-3 stream is
  byte-identical. Coverage is deliberately staged: every currently installed
  row must realize each declared bit via a live pfn or the explicit engine-side
  exception table, every live pfn must have a declared bit, the exact 12-ability
  WEATHER whitelist rejects unrelated declarations, and no fake future hook is
  installed merely to satisfy coverage. SC3 and SC4 extend the installed set;
  the all-50 complete-realization gate belongs to SC5.
- **Why:** the table preserves the repo's no-`std::function` rule, stable data ids,
  append-only event protocol, and deterministic NONE path while giving later
  ability slices one typed dispatch surface. Centralizing stat mutation avoids
  duplicating clamp/event semantics between moves and switch-in hooks.
- **Tests that lock it:** **18 new unit cases** (5 `ZM_Data`, 13 `ZM_Battle`)
  cover the 12-slot/sentinel table contract, staged and converse realization
  rules, the explicit engine-side rosters, all four weather callers, Begin and
  DoSwitch ordering, overwrite and identical-weather anti-refresh, Daunting
  Roar + shared clamp/event encoding, Pressure Aura's 2-PP path + zero clamp and
  its SELF/FIELD/NONE-defender 1-PP negatives, non-SWITCH_IN silence, and the
  exact NONE event/RNG stream. Local evidence: Vulkan tools build GREEN; boot
  unit gate **1446 registered = 1445 passed / 0 failed / 1 skipped**; headless
  automated boot **1/1**; implementation/test reviews GREEN after both
  SHOULD-FIX test gaps were closed.
- **Reversibility:** high and localized. Remove the new hook table/context,
  switch-in dispatch, Pressure Aura M1 predicate, shared stat wrapper, and SC2
  tests/baseline to return to the SC1 weather-only state. The shared stat helper
  is additive and may be retained independently; no save schema, event ordinal,
  data-table id, or RNG phase changed.

---

## 2026-07-11 -- ZM-D-038 -- Terrain fixed-size/fixed-density limitation accepted as deferred engine TODO (E6)

- **Decision:** Zenith's terrain system cannot currently give different terrain instances
  different world-space extents while holding triangle density (metres/vertex) constant --
  `Flux_TerrainConfig::CHUNK_GRID_SIZE`/`CHUNK_SIZE_WORLD`/`TERRAIN_SIZE` are global
  `static constexpr` values (`Flux_TerrainConfig.h:27-36`), so every terrain is a fixed
  4096x4096 m grid, and density (`fLowLODDensity`, `Zenith_TerrainComponent.cpp:493`) is
  likewise a fixed global, not a per-instance field. E2's rect export only crops that same
  fixed grid (a bake-time/file-count optimization), it does not resize it. This was
  investigated on request (a route and a city should be different world-space sizes at the
  same density) and confirmed as a real gap, already implicitly acknowledged in the E2
  rationale ("compile-time constants pervade streaming/grass") but not previously called
  out as its own tracked item.
  User has **accepted the Zenithmon plan as-is with this limitation** (routes and the
  city will all bake at the same fixed 4096x4096 grid/density for now) rather than block
  or descope S3+ on it. Logged as **Shortfalls.md E6 / MasterPlan.md engine-changes E6**:
  DEFERRED, revisit as a dedicated engine initiative after Zenithmon ships.
- **Why:** fixing this properly (per-instance serialized grid/density fields, dynamically
  sized streaming-manager GPU/CPU buffers, parametrized culling/streaming-radius/grass math)
  is a materially larger engine change than anything else in scope for Zenithmon, and
  Zenithmon does not strictly need it to ship (every outdoor scene can still be its own
  terrain SET per E1 -- it's the per-set world-space size/density that's inflexible).
- **How to apply going forward:** every terrain-touching task for the rest of Zenithmon
  development (S3 first-overworld, S9/S10 world buildout, any terrain-authoring or
  streaming work) must treat the 4096x4096 fixed grid/density as a hard current constraint
  -- do NOT author content or write game-side code that assumes per-scene size/density will
  become configurable mid-project, and do not build a content-side workaround that
  papers over it (e.g. do not silently scale down non-terrain content to fake a smaller
  world, do not hand-roll a second grid system in game code). If a route's designed
  world-space size doesn't fit the fixed grid, that's a WorldSpec/authoring-scale decision
  to route through Questions.md, not a silent workaround.
- **Tests that lock it:** none (this is a documented limitation, not a coded rule) --
  E1/E2's existing unit tests + RenderTest boot regression remain the only terrain-system
  coverage until E6 is picked up.
- **Reversibility:** N/A (acceptance of a known limitation, not an implementation). E6
  itself, when eventually done, is additive per the standard engine-change convention.

---

## 2026-07-11 -- ZM-D-037 -- Terrain bake-time fallback: optimize the pipeline, not shared terrain sheets

- **Decision:** One terrain set per outdoor scene/route is a **hard requirement**, not
  negotiable against bake time. The previously documented fallback for Q-2026-07-09-002
  ("multiple routes share one terrain sheet" if the S3 bake-time measurement comes back too
  slow) is **retracted**. If the S3 measurement (bake 3 real scenes, extrapolate to ~25)
  shows bakes are too slow, the fallback is instead to **optimize the terrain bake
  pipeline** -- e.g. parallelize chunk export across cores/processes, incrementalize/cache
  unchanged chunks so re-bakes aren't full cold bakes, profile the actual hot path and cut
  it -- not to reduce the terrain-set count below one-per-scene.
- **Why:** shared terrain sheets across routes/scenes is a visible content compromise (reused
  ground geometry/texturing across supposedly distinct locations) that the user considers
  unacceptable for a ~25-outdoor-scene RPG; user directive overrides the prior cost-driven
  fallback plan.
- **Tests that lock it:** none yet -- S3's terrain-bake measurement task (Roadmap.md) must
  report against this constraint; if bakes measure too slow, the S3 follow-up is a bake-
  pipeline optimization pass, not a terrain-set reduction.
- **Reversibility:** high -- no implementation has happened yet under the old fallback;
  this is a plan correction ahead of S3, recorded in Questions.md Q-2026-07-09-002,
  MasterPlan.md risk #1, Shortfalls.md, and AssetManifest.md section 6.3.

---

## 2026-07-11 -- ZM-D-036 -- S2 box-3 execution plan (abilities + weather) + SC1 weather core

- **Decision:** Box 3 ("Abilities via per-hook fn-pointer structs (~50) + weather
  rain/sun/sand/snow") ships as **5 ordered sub-commits**, mirroring how box 2 (ZM-D-033)
  landed: **SC1** weather core (this commit); **SC2** ability hook infra + SWITCH_IN
  abilities; **SC3** damage-dealt/taken + modify-stat + type-immunity abilities; **SC4**
  status-try + contact + stat-veto + accuracy abilities; **SC5** turn-end + faint + quickdraw
  + fuzz soak. Abilities are a `const` fn-pointer hook table keyed by `ZM_ABILITY_ID`
  (repo mandate: `std::function` -> plain fn pointers), dispatched at documented turn-loop
  seams; the `ZM_AbilityData.m_uHookMask` (ZM-D-026) stays the coverage/declaration record.
- **Keystone determinism invariant (holds for all of box 3):** a `ZM_ABILITY_NONE`,
  weather-`NONE`, status-free actor pulls **ZERO** new RNG draws and emits **ZERO** new
  events, so box-1/2 goldens and the 2,000-battle soak stay byte-identical. New draws are
  added only at documented points and only when the relevant ability/weather is present.
  The `ZM_BattleEvent` POD is append-only: the 5 box-3 ordinals 26-30
  (`ABILITY_TRIGGER`/`WEATHER_CHANGED`/`WEATHER_DAMAGE`/`SCREEN_SET`/`SCREEN_EXPIRED`) were
  already reserved -- box 3 lights them, adds no ordinal and **no new field** (all payloads
  fit the existing 8 scalars). Ability damage multipliers apply **outside** the pure
  `ZM_CalcDamage` (post-calc in `ApplyDamagingHit`), so its isolated pure-fn goldens never move.
- **Box-3-wide design rulings (from the 3-survey / synth / adversarial-critique design panel;
  full design archived by the session):** (a) the ability-hook **coverage invariant** is
  "every set `ZM_ABILITY_HOOK` bit is *realized* by an enumerated mechanism -- a live pfn
  slot OR a documented engine-side handler", NOT "every bit has a backing pfn" (subsumes the
  weather-bit condition + the QUICKDRAW/DAUNTINGROAR/BLOODRUSH engine-side realizations);
  (b) STATUS_TRY abilities **emit `ABILITY_TRIGGER` on a successful block** (SC4); (c) flee
  (`DoRunAction`) keeps the un-ability-modified speed -- ability speed mods apply only to
  move-order resolution (SC3); (d) the confusion-self-hit `ZM_CalcDamage` stays un-weathered
  (typeless; avoids a box-2 golden shift).
- **SC1 (this commit) implements weather core, no abilities:** the weather **damage
  multiplier** via the existing `uWeatherNum/uWeatherDen` seam in `ApplyDamagingHit` (RAIN:
  WATER x3/2, FIRE x1/2; SUN: FIRE x3/2, WATER x1/2; SAND/SNOW/NONE = 1/1); the **end-of-turn
  weather chip** (new `ResolveWeatherEndOfTurn`, resolved FIRST in `ResolveEndOfTurnPhase`
  before the status ticks) -- SAND/SNOW only, `maxHP/8` min 1, PLAYER-active then ENEMY-active,
  SAND immune = EARTH/STONE/IRON, SNOW immune = ICE, underflow-clamped, `WEATHER_DAMAGE`
  [+ `FAINT`]; **weather + screen countdown/expiry** (`ResolveWeatherEndOfTurn` +
  new `ResolveScreenEndOfTurn`) with `WEATHER_CHANGED`/`SCREEN_EXPIRED`; **`WEATHER_CHANGED` /
  `SCREEN_SET` emission** in `g_ApplyField` (move setters now announce; overwrite re-announces);
  and a new shared free fn `ZM_BattleMonsterHasType` (promotes the file-static `g_HasType`
  logic). Event encodings per the design's section 2.6 (e.g. `WEATHER_CHANGED{m_uSide=ZM_SIDE_COUNT,
  m_iAmount=newWeather, m_iAux=turns, m_iTag=prevWeather}`). Zero new RNG draws; `ZM_DamageCalc`
  untouched.
- **Sanctioned golden churn:** exactly 4 box-2 tests that asserted box-3 events are NOT
  emitted / a MOVE_USED-only stream were tightened (`ZM_CheckWeatherSetter`,
  `Weather_SetterEmitsOnlyMoveUsed` -> `...MoveUsedThenWeatherChanged`, the two `Screen_*Wall_Sets*`);
  every other box-1/2 golden stays byte-identical (verified: 0 failed).
- **Why fn-pointers + post-calc ability mults + append-only events:** preserves the locked
  box-1/2 goldens and the deterministic RNG draw order (ZM-D-032/033) while giving each of the
  50 declared abilities a real body; keeps `ZM_CalcDamage` a pure, independently-golden fn.
- **Tests that lock it:** the `Box3SC1_*` block in `Tests/ZM_Tests_Battle.cpp` (28 cases:
  pure-seam + end-to-end weather multiplier vectors; SAND/SNOW chip incl. immunity, min-1
  clamp, chip-KO `FAINT` + underflow branch, skip-fainted-active, PLAYER-then-ENEMY order,
  chip-before-status ordering; weather + physical/special screen countdown/expiry with full
  event encodings; overwrite re-announce; and the
  `Box3SC1_WeatherFree_ScreenFree_AddsNoEventsDrawsOrState` zero-draws/zero-events regression
  wall). Boot unit gate **1400 -> 1428** (bumped in `.github/workflows/zm-tests.yml` this
  commit). Independent test/impl authoring (blind parallel authors) + a 2-lens adversarial
  review (impl-correctness + test-tautology) gated the commit.
- **Reversibility:** moderate. Weather logic is localized to `ResolveWeather/ScreenEndOfTurn`
  + the `ApplyDamagingHit` seam + `g_ApplyField`; the shared `ZM_BattleMonsterHasType` is
  additive. Reverting SC1 alone would re-inert weather but leave the reserved event ordinals.

## 2026-07-11 -- ZM-D-035 -- S2 box-2 SC6: capture / flee math + pre-move SWITCH/ITEM/RUN (box 2 COMPLETE)

- **Decision:** SC6 is the final box-2 sub-commit (ZM-D-033). It adds `ZM_CatchCalc`
  (new `Source/Battle/ZM_CatchCalc.{h,cpp}`) and the engine's pre-move
  SWITCH / ITEM(catch) / RUN actions in `ResolvePreMovePhase`, and promotes the box-1
  50-battle smoke to the **deterministic 2,000-battle soak**. Every move-only turn
  stays BYTE-IDENTICAL: the pre-move phase is inert unless a side submits a non-MOVE
  action, and the move phase only draws the speed tie-break when BOTH sides submit
  MOVE (the box-1 path is unchanged). Box 2 is now complete.
- **Capture math (LOCKED; integer, no floating point -- an internal choice within the
  ZM-D-033 Q2 contract, like box-1's rounding sub-decisions):** base rate from
  `ZM_RARITY` (COMMON 190 / UNCOMMON 120 / RARE 45 / LEGENDARY 3, via
  `ZM_CatchCalc::BaseCatchRate` -- NO new S1 column). Ball bonus = the item row's
  catch param x10 (`ZM_ItemData`); PRIMEORB's param 255 is the guaranteed-capture
  sentinel (master-ball analog). Status bonus (Gen-IV, Q2): sleep/freeze x5/2,
  paralysis/poison/toxic/burn x3/2, none x1. Modified value
  `a = (3*maxHP - 2*curHP) * rate * ballX10 / (3*maxHP*10)`, then `a = a*num/den`
  (min 1). `a >= 255` (or a guaranteed ball) captures with ZERO draws. Otherwise the
  Gen-III/IV integer shake gate `b = 1048560 / isqrt(isqrt(16711680 / a))` gates four
  `RandBelow(65536) < b` checks, stopping at the first failure; four passes == caught.
  Conditional ball bonuses (net/dusk/quick) are DEFERRED (Shortfalls.md 1.2).
- **Flee math (LOCKED):** `selfSpeed >= oppSpeed` (or opp 0) is a guaranteed escape,
  no draw; otherwise `f = (selfSpeed*128 / oppSpeed + 30*attempt) mod 256` gates one
  `RandBelow(256) < f` check. `attempt` is 1-based and ramps across repeated runs.
  Effective speed is the same stat-stage/paralysis fold used for turn order.
- **Pre-move phase contract (ZM-D-035):** pre-move actions resolve BEFORE any move in
  fixed **PLAYER-then-ENEMY** order (the EOT side-order convention). A successful
  capture ends the battle with the CATCHING side as winner; a successful flee ends it
  with NO winner (`ZM_SIDE_COUNT`). Either closes the turn directly: `TURN_END` then
  `BATTLE_END` (skipping the move + end-of-turn phases), so the turn stays balanced.
  A voluntary SWITCH reuses the SC5 `DoSwitch` primitive; a TRAPPED active reports
  `MOVE_FAILED(TRAPPED)` and an otherwise-illegal destination reports
  `MOVE_FAILED(NO_SWITCH_TARGET)` -- either way that side does not move. When exactly
  one side submits MOVE, the move phase runs only that mover with NO tie-break draw.
- **Event encodings (append-only; box-1 goldens unchanged):** `CATCH_SHAKE`
  (m_iAmount = shake index 1..4, m_iTag = ball id) once per wobble; `CATCH_RESULT`
  (m_iAmount = caught 1/0, m_iAux = shake count, m_iTag = ball id); `FLEE` /
  `FLEE_FAILED` (side = the runner). `ZM_MOVE_FAIL_TRAPPED` appended to
  `ZM_MOVE_FAIL_REASON`. ITEM in SC6 supports ball items only (medicine/battle items
  are box 5); ITEM/RUN require a wild config.
- **Why:** turn order puts run/item/switch before moves (GDD), so the pre-move seam is
  the correct attachment point; gating every new draw on non-MOVE actions keeps the
  ~200 box-1/SC1-SC5 goldens byte-identical while lighting the CATCH/FLEE event kinds.
  Pure integer catch/flee math keeps deterministic replay; the offline oracle
  (`scratchpad/zm_catch_ref.py`, an independent PCG32 reimplementation) derived every
  expected `a`/`b`/shake/flee value so the tests are not engine echoes.
- **Tests that lock it:** 21 SC6 `ZM_Battle` cases -- catch base rate / status
  multipliers / ball param+guaranteed / modified-value + shake-probability vectors;
  Roll (non-guaranteed caught+escaped, guaranteed-by-value, guaranteed-by-ball,
  legendary-full-HP escape); flee odds + roll (guaranteed + computed both ways); engine
  catch (guaranteed ends battle, non-guaranteed 4-shake, escape continues the turn);
  engine run (guaranteed + failed); engine switch (voluntary, trapped, invalid target);
  and the move-only wild==trainer byte-identity check. Plus the promoted
  `Fuzz_Soak_2000Battles_Invariants` (2000 seeded battles, half wild with periodic
  catch/run; termination < 500 turns + HP/PP/stage/stream invariants). Boot unit gate
  1400 ran / 0 failed (baseline 1379 -> 1400).
- **Reversibility:** moderate. Localized to `Source/Battle/` (+ the two new files),
  but the catch/flee formulas, the guaranteed-ball sentinel, the pre-move side order,
  the catch/flee winner semantics, and the CATCH/FLEE event encodings now define
  deterministic replay goldens -- change them only with a new decision + updated oracle.

## 2026-07-11 -- ZM-D-034 -- S2 box-2 SC5 volatile, Endure, and switch contract

- **Decision:** SC5 completes the GDD's exact ten battle-local volatile bits in
  `ZM_BattleMonster::m_uVolatileMask`: `CONFUSED`, `FLINCH`, `LEECH_SEED`,
  `PROTECT`, `CHARGE`, `SEMI_INVULN`, `RECHARGE`, `LOCK`, `TRAP`, and `TAUNT`.
  `ENDURE` is deliberately **not** an eleventh volatile: it is the separate
  one-turn `m_bEndureThisTurn` flag. `ZM_StatusLogic` owns apply/end/reset,
  counters, metadata, PHASE-G gates, and end-of-turn processing; the executor
  owns effect routing and M-phase intercepts. Every application emits
  `VOLATILE_APPLIED` (`m_iAmount=duration`, `m_iAux=volatile bit`), every explicit
  expiry emits `VOLATILE_ENDED` (`m_iAux=volatile bit`), and FLINCH cancellation
  additionally has its dedicated `FLINCH` event.
- **Duration/counter contract:** counters are independent of the major-status
  counter. CONFUSED draws `RandRange(1,4)` on a successful new application and
  counts G5 action attempts (decrement after the pass/self-hit result, then end at
  zero). TRAP draws `RandRange(4,5)`, chips on every EOT including its application
  turn, and decrements only after a surviving tick. TAUNT is 3 EOTs, no draw.
  LOCK_IN draws 2-3 total executed uses: its establishing hit is use one, so the
  stored remaining counter starts at `duration-1`; each later use that reaches M1
  consumes one even if it then misses/is intercepted/is immune, while a PHASE-G
  cancellation consumes neither PP nor a locked use; zero PP ends the lock early.
  RECHARGE is one G1-cancelled action. FLINCH and PROTECT end at EOT (a repeated
  Protect refreshes and re-emits APPLIED without an intermediate ENDED); Endure
  clears silently at EOT. CHARGE/SEMI_INVULN do not count down at EOT: turn one
  spends PP + emits MOVE_USED then stores the slot; the forced release emits
  MOVE_USED without a second PP and clears both bits before M2/M3 intercepts. A
  cancelling G gate clears a pending charge/semi-invulnerable pair.
- **Gate/intercept + RNG contract:** the ZM-D-033 order is now fully live:
  `G1 RECHARGE -> G2 FREEZE -> G3 SLEEP -> G4 FLINCH -> G5 CONFUSE -> G6
  PARALYSIS`, first cancellation wins, before PP/MOVE_USED. G1/G4 draw nothing.
  G5 draws `RandBelow(100)<33` once per confused action; self-hit then draws only
  `RandRange(85,100)` and applies 40-power typeless physical damage (no accuracy,
  crit, STAB, type, weather, or screen; burn still halves it), tagged as volatile
  damage and able to faint the user. M0 TAUNT blocks STATUS moves before PP;
  M2 PROTECT intercepts opponent-target moves; M3 SEMI_INVULN intercepts the same;
  M4 establishes two-turn state before accuracy. Duplicate/type-blocked volatile
  secondaries and FORCE_SWITCH with no target are preflighted **before** the E3
  proc gate, so they draw nothing; chance >=100 draws nothing; a fainted defender
  receives no secondary. A volatile-free battle therefore adds zero draws, events,
  or state changes and preserves the box-1 goldens.
- **Individual mechanics:** Leech Seed rejects GRASS, stores its source side, and
  persists until switch; Protect targets self; Endure clamps otherwise-lethal
  direct move damage (ordinary, multi-hit, fixed, half-HP, OHKO) to 1 HP but does
  not protect from recoil/confusion/EOT damage. Swagger applies the Attack stage
  change first and then CONFUSED; either component may succeed independently, and
  MOVE_FAILED is emitted only when both are blocked. RECHARGE/LOCK establish only
  after a connecting standard damaging hit (a KO still counts); charge and
  semi-invulnerability use the submitted stored move slot.
- **End-of-turn + tag contract:** the box-3 weather slot remains reserved first;
  box 2 then processes sides in fixed **PLAYER-then-ENEMY order (not speed order)**.
  Each side runs `major chip -> Leech Seed -> Trap -> cleanup/expiry`; after both
  sides, the engine emits `TURN_END`. Leech/Trap are max-HP/8, minimum 1. A major-status KO suppresses
  later chip sources for that side but cleanup still runs; a Trap KO emits no later
  expiry. Leech heals the source side's current living active by actual HP restored,
  so a source switch redirects the drain. Damage-event `m_iTag` uses a major-status
  ordinal on `STATUS_DAMAGE` for major chip and the disjoint
  `0x10000 | volatileBit` domain on CONFUSED `DAMAGE_DEALT` and Leech/Trap
  `STATUS_DAMAGE`. `VOLATILE_APPLIED.m_iTag` is `sourceSide+1` for Leech
  Seed (zero for every other volatile), avoiding PLAYER's numeric-zero ambiguity.
- **Switch/forced-switch contract:** `ZM_MoveResult` carries a requested forced
  side/slot from the executor to the engine. A primary FORCE_SWITCH chooses the
  lowest eligible non-active living slot or emits `MOVE_FAILED(NO_SWITCH_TARGET)`;
  a damaging secondary applies only after damage, never on KO, and silently skips
  both proc draw and switch when no eligible slot exists. Engine-owned
  `ZM_BattleEngine::DoSwitch` validates side/destination, silently clears the
  outgoing monster's volatiles/counters/charge+lock slots/leech source, all stages,
  crit stage, and Endure, while preserving HP, PP, major status, and its major
  counter; it then changes the active slot and emits `SWITCH_IN`. If the first
  mover force-switches the side queued second, that old monster's queued action is
  skipped. SC6 voluntary switching reuses this primitive; it does not duplicate
  reset semantics.
- **Why:** the ten effects share ordering, duration, event, and switch-reset
  invariants, so implementing them as isolated move arms would drift deterministic
  replay and leak state across switches. Centralized lifecycle ownership plus an
  engine-owned switch primitive keeps every draw conditional, makes event streams
  presentation-complete, and gives SC6 one safe attachment point for voluntary
  switching and Trap restrictions.
- **Tests that lock it:** 52 SC5 cases in `ZM_Tests_Battle.cpp` cover default
  state/event encodings and tag domains; every apply/block/duration/draw path;
  confusion self-hit/pass/burn/faint; flinch timing; Leech/Trap EOT + exact stream;
  Taunt/Protect/Endure; charge/semi/recharge/lock; Swagger; DoSwitch reset;
  primary/damaging forced switch + queued-action skip; fixed side/EOT order; full
  G/M precedence; and `VolatileFree_SC5AddsNoEventsDrawsOrState`. The box-1 exact
  stream remains unchanged. Verified gate: regen check green,
  `Vulkan_vs2022_Debug_Win64_True` build green, **1379 ran / 1378 passed / 0 failed
  / 1 skipped**, and headless automated suite **1/1**.
- **Reversibility:** moderate. The implementation is localized to
  `Source/Battle/`, but the bit values, event encodings/tags, duration semantics,
  guarded draw order, EOT order, and switch-reset behavior now define deterministic
  replay goldens and downstream presentation. Change them only with a new decision
  plus updated oracle/event-stream tests.

## 2026-07-11 -- ZM-D-033 -- S2 box-2 execution plan: 6 ordered sub-commits + the LOCKED augmented RNG draw order (PHASE G/M/E)

- **Decision:** box 2 (ZM_MoveExecutor + full DamageCalc/CatchCalc/StatusLogic,
  the bulk of S2's ~370 tests) is decomposed into **6 independently-shippable
  sub-commits**, executed in order, each keeping the box-1 goldens BYTE-IDENTICAL
  (every new gate/draw is conditional on state box 1 never sets -- status-free,
  stage-0, volatile-mask 0 -- and every new `ZM_BATTLE_EVENT` kind is append-only).
  Synthesized from a 2-planner design workflow; full plan archived this session in
  scratchpad `S2_Box2_Plan.md` (ephemeral -- the load-bearing contract is captured
  here). Sub-commit order (each builds + passes + commits alone):
  - **SC1** Executor seam (PURE refactor): move `ExecuteMove`'s body into
    `ZM_MoveExecutor::Execute` / `ApplyDamagingHit` over a `ZM_MoveContext`; switch
    has only `NONE` + `default`; box-1 27-event golden byte-identical. New files
    `Source/Battle/ZM_MoveExecutor.{h,cpp}`. No new events.
  - **SC2** Stat-stage effects (all LOWER_*/RAISE_* + accuracy/evasion) + STATUS-
    category vs damaging-secondary dispatch. Lights `STAT_STAGE_CHANGED`,
    `MOVE_FAILED`.
  - **SC3** Delivery variants (MULTI_HIT/DOUBLE_HIT/RECOIL/DRAIN/HEAL_HALF/
    FIXED_LEVEL/HALVE_HP/OHKO) + field/screen/hazard SETTERS (state-only) + screen
    damage reduction (`bScreen`). Lights `MULTI_HIT/RECOIL/DRAIN/HEAL`.
  - **SC4** `ZM_StatusLogic` majors (6) + burn damage reduction (`bBurnedPhysical`)
    + paralysis speed x1/4. New files `ZM_StatusLogic.{h,cpp}`. Appends `m_iTag`
    to `ZM_BattleEvent` (Q1). Lights `STATUS_APPLIED/DAMAGE/CURED`.
  - **SC5** Volatiles (all 10) + SWAGGER + FORCE_SWITCH + a `DoSwitch` primitive.
    Lights `FLINCH/VOLATILE_APPLIED/VOLATILE_ENDED`.
  - **SC6** Pre-move actions (SWITCH/ITEM/RUN in `ResolvePreMovePhase`) +
    `ZM_CatchCalc` (Gen-III/IV 4-shake) + promote the smoke to the **2000-battle
    fuzz soak**. New files `ZM_CatchCalc.{h,cpp}`. Lights `CATCH_SHAKE/CATCH_RESULT/
    FLEE/FLEE_FAILED`.
- **LOCKED augmented RNG draw order (the master contract -- every box-2 golden is
  defined by it; the offline oracle mirrors it exactly).** Attacker fully resolves
  before defender; the pre-phase `RandBelow(2)` speed tie-break (only on exact
  effective-speed tie) is unchanged. Draws are pulled ONLY when the guard holds, so
  box-1 (status-free, stage-0) pulls EXACTLY box-1's draws:
  - **PHASE G (pre-move gates, BEFORE PP/MOVE_USED; first trigger cancels, spending
    NO PP + emitting NO MOVE_USED -- Q3):** G1 RECHARGE (0 draws) -> G2 FREEZE
    (`RandBelow(100)<20` thaw) -> G3 SLEEP (counter, 0 draws) -> G4 FLINCH (0 draws)
    -> G5 CONFUSE (counter; `RandBelow(100)<33` -> self-hit = 40-power typeless
    PHYSICAL drawing its own `RandRange(85,100)`) -> G6 PARALYSIS (`RandBelow(100)<25`
    full-para).
  - **PHASE M:** M0 TAUNT-blocks-STATUS-category -> M1 spend PP + emit `MOVE_USED`
    -> M2 PROTECT intercept -> M3 SEMI_INVULN-target-vanished -> M4 two-turn
    charge/semi-invuln turn-1 set -> M5 ACCURACY (only if the move can miss; effAcc
    via acc/eva stages) -> M6 IMMUNITY (damaging/fixed/ohko only).
  - **PHASE E:** single-hit damaging = crit `RandBelow(24)` (skip if critStage>=2)
    -> roll `RandRange(85,100)` -> `ApplyDamagingHit` (sets `bBurnedPhysical`/
    `bScreen` at the call site) -> recoil/drain self-effect appended AFTER -> E3
    secondary proc `RandBelow(100)<chance` (chance>=100 no draw); apply-time
    duration draws SLEEP `RandRange(1,3)`, CONFUSE `RandRange(1,4)`, TRAP
    `RandRange(4,5)`, TOXIC counter=1. MULTI_HIT = hit-count `RandBelow(8)` ->
    {2,2,2,3,3,3,4,5} then per-hit (crit,roll). DOUBLE_HIT fixed 2. FIXED_LEVEL/
    HALVE_HP/OHKO no crit/roll. STATUS-category primary no crit/roll.
- **Weather line (box-2 vs box-3 rule):** box 2 SETS weather/screen/hazard field
  state + applies the ability-independent burn (`bBurnedPhysical`) and screen
  (`bScreen`, crit bypasses) damage reductions, but emits NO weather/screen event
  and does NOT count down. Box 3 owns the weather DAMAGE multiplier
  (`uWeatherNum/Den`), the end-of-turn weather chip, the weather/screen countdown +
  expiry, and ALL of `WEATHER_CHANGED/WEATHER_DAMAGE/SCREEN_SET/SCREEN_EXPIRED/
  ABILITY_TRIGGER`. End-of-turn global order reserves the weather-chip slot FIRST:
  `[weather chip (box 3, absent in box-2 tests) -> per side player-then-enemy in
  speed order: major chip -> leech-seed -> trap chip -> volatile-counter expiries
  -> TURN_END]`. Box-2 full-stream goldens use battles with NO weather set.
- **Ratified open questions + constants (internal engine choices within locked
  scope, like box-1's rounding/crit sub-decisions):**
  1. **Q1:** append `int m_iTag = 0` to `ZM_BattleEvent` (+ a trailing defaulted
     `ZM_MakeEvent` param) as the `STATUS_DAMAGE` source-status discriminator
     (ZM-D-009 append pattern; box-1 goldens stay equal). Lands in SC4.
  2. **Q2:** catch base rate derived from `ZM_RARITY` (COMMON 190 / UNCOMMON 120 /
     RARE 45 / LEGENDARY 3) via `ZM_BaseCatchRate` (NO new S1 data column);
     Gen-IV status bonuses (sleep/freeze 2.5x, para/poison/toxic/burn 1.5x).
  3. **Q3:** an action-time gate cancel spends NO PP and emits NO `MOVE_USED`.
  - **Constants:** percentage gates `RandBelow(100)<t` (thaw 20, full-para 25,
    confusion self-hit 33); multi-hit `RandBelow(8)`->{2,2,2,3,3,3,4,5}; durations
    sleep 1-3 (REST=2), confuse 1-4, trap 4-5, taunt 3, lock-in 2-3; chips burn/
    poison/leech/trap 1/8, toxic n/16; paralysis speed x1/4; crit unchanged 1/24 x1.5.
- **Why:** every box-2 golden event stream is defined by the exact draw order;
  locking it ONCE here prevents drift across the ~275 box-2 tests and lets each
  sub-commit's Python oracle reproduce streams independently before the engine is
  trusted. The ordering ships each dependency before its consumer (statuses before
  the effects that inflict them; burn WITH its damage-halving; pre-move actions +
  the soak last, once the full effect surface exists so the soak terminates).
- **Tests that lock it:** `ZM_Tests_Battle.cpp` box-2 additions per sub-commit +
  the box-1 golden re-run (`Scenario_NibbinVsStrayling_ExactStream`), which MUST
  stay byte-identical through every sub-commit; the 2000-battle soak at SC6.
- **Reversibility:** moderate. The draw order + the `m_iTag` append define all
  box-2 goldens (expensive to change); sub-commit boundaries are independent, so a
  later sub-commit can be re-planned without disturbing landed ones.

## 2026-07-10 -- ZM-D-032 -- S2 battle-engine keystone architecture (box 1): ZM_BattleState / ZM_BattleEngine / flat ZM_BattleEvent stream / ZM_BattleMonster

- **Decision:** the S2 battle engine is built as box 1 of ~6, establishing the
  type architecture the other five boxes extend without reshaping. Chosen shape
  (synthesized from a 3-architect design panel, spec archived in the session
  scratchpad `S2_Box1_Design.md`):
  - **`ZM_BattleMonster`** = the mutable in-battle instance (deep-owned BY VALUE
    inside the battle; the bare name `ZM_Monster` is reserved for a future
    persistent party type). Built by `ZM_BuildBattleMonster(ZM_BattleMonsterSpec)`;
    the spec carries IV/EV/nature/moves/ability **plus an optional base-stat
    override** so goldens are pencil-verifiable and survive the ZM-D-021 base-stat
    re-tune.
  - **`ZM_BattleState`** = `m_axSides[2]` (each a `ZM_BattleSide` with a
    `Zenith_Vector<ZM_BattleMonster>` party + active slot + reserved
    screens/hazards) + `ZM_FieldState` (weather reserved, turn counter) + the
    **`ZM_BattleRNG` living IN state** (so snapshot/clone/replay is one object).
    The append-only event stream lives in the **engine** (output, not state).
  - **`ZM_BattleEvent`** = a flat 7-scalar-field POD (`kind,side,slot,moveId,
    speciesId,iAmount,iAux`) with a defaulted `operator==`, built through a shared
    `ZM_MakeEvent` factory used by both the engine AND the golden vectors. Later
    boxes only APPEND event kinds and APPEND fields (default 0), so box-1 golden
    streams never shift (ZM-D-009 append-only discipline). Effectiveness is
    emitted as SEPARATE `SUPER_EFFECTIVE`/`NOT_EFFECTIVE`/`IMMUNE` events; neutral
    is silent.
  - **`ZM_BattleEngine`** = `Begin(config, playerSpecs,n, enemySpecs,n, seed, seq)`
    -> `SubmitAction(side, action)` -> `ResolveTurn()`; `IsOver()`/`GetWinnerSide()`
    /`GetEvents()`/`GetState()`. Box 1 executes MOVE actions only; SWITCH/ITEM/RUN,
    the ~60-effect `ZM_MoveExecutor`, statuses, abilities, weather, exp/AI/tower are
    later-box seams left as empty phase hooks (`ResolvePreMovePhase`,
    `ExecuteMove`, `ResolveEndOfTurnPhase`).
  - **Box 1 ships a REAL minimal Gen-V `ZM_CalcDamage`** (pure function, RNG drawn
    by the engine and passed in) so box 1 is end-to-end testable to a faint; the
    full effect executor stays in box 2.
- **Ratified sub-decisions (the design panel's 3 open questions, all internal
  engine choices within locked scope -- no user decision required):**
  1. **Rounding = pure integer floor** at every `x3/2` and `xpercent` step (matches
     the S1 `ZM_StatCalc` idiom; simplest to mirror in the offline Python oracle).
     NOT Gen-V round-half-down. Locked before the first scenario golden is committed.
  2. **Crit = 1/24 rate, x1.5 multiplier** -- honoring the GDD 7.1 explicit numbers
     over canonical Gen-V (x2, level-table rate). The GDD wins on conflict.
  3. **Draw discipline:** no accuracy draw when effective accuracy >= 100 (sure-hit
     short-circuit); no crit draw at critStage >= 2; per-move draw order
     `accuracy(if it can miss) -> crit RandBelow(24) -> roll RandRange(85,100)`,
     attacker-then-defender, one `RandBelow(2)` tie-break only on exact
     effective-speed ties. Modifier order `base -> weather -> crit -> random ->
     STAB -> type -> burn -> screen`.
- **Why:** every one of S2's ~370 later unit tests asserts against the
  `ZM_BattleEvent` stream, so the event shape + the deterministic draw order are
  the load-bearing contract -- getting them wrong churns the whole suite. A flat
  append-only POD + a shared factory makes later boxes additive; deep-owned
  monsters make a battle a pure function of `(specs, config, seed)` (bit-exact
  replay, clone-for-AI lookahead, save-mid-battle). Verified against the shipped S1
  data layer before authoring: Nibbin + Strayling are both mono-NORMAL, Rambash is
  NORMAL/PHYSICAL power 45 / acc 100, `ZM_NATURE_FERAL`/`ZM_ITEM_NONE`/
  `ZM_ABILITY_NONE` all exist, `ZM_TypeChart::GetDualTypeEffectiveness` returns the
  exact float product.
- **Tests that lock it:** `ZM_Tests_Battle.cpp` (category `ZM_Battle`) --
  `Scenario_NibbinVsStrayling_ExactStream` (the characterization bedrock: full
  event stream vs an offline PCG32+stat+damage Python oracle validated against the
  S1 golden vectors), `Damage_GenVGoldenVectors`, `Effectiveness_PercentMapping`,
  `TurnOrder_SpeedPriorityTie`, `Engine_BeginEmitsHeader`, and
  `Fuzz_Smoke_50Battles_Invariants` (scaffold for the S2 2000-battle soak).
- **Reversibility:** moderate. The event-field set and draw order are the
  expensive-to-change parts (they define every golden); the type/file layout and
  the minimal damage path are localised to `Source/Battle/`. Reserved event kinds
  and the `x3/2` seam fields mean box 2-6 attach without touching box-1 goldens.

## 2026-07-10 -- ZM-D-031 -- Master-only workflow: all work committed DIRECTLY to master; no branches, no PRs, no worktrees

- **Decision (user-directed):** all Zenithmon work is committed DIRECTLY to the
  `master` branch and pushed with `git push origin master`. Feature branches,
  pull requests, and git worktrees are no longer created -- `git checkout -b`,
  `gh pr create`, and worktree creation are forbidden for this project. This
  supersedes the branch + PR + auto-merge flow of ZM-D-028 (which itself
  superseded "wait for all checks green"). The authoritative gate is the LOCAL
  verification run before every push (`zenith build` + the boot unit gate
  `run_unit_gate.ps1` at the exact baseline + `zenith test --headless`); the
  `zm-tests` CI workflow still runs post-push on master (`push: [master]`
  trigger) as a BACKSTOP only. On a red post-push run, fix forward with another
  direct commit to master (never revert shipped history, force-push master, or
  `gh run rerun`).
- **Why:** the user's explicit direction. It removes all PR/branch/merge
  latency and the stacked-PR conflict hazard (squash-merging a parent orphaned
  its stacked child at the top of the append-only DecisionLog -- see the #156/#157
  conflict resolution 2026-07-10). Direct-to-master is viable here because the
  repo owner's credential can push to master (branch protection was created with
  `enforce_admins=false` precisely to preserve the owner's direct-push workflow,
  ZM-D-016), and the local gate is a full build + the same unit + headless suites
  CI would run. Trade-off accepted by the user: no pre-merge CI gate and no PR
  review; the local gate is the sole pre-push authority, so it MUST be run green
  before every push.
- **Tests that lock it:** none executable; the contract is the rewritten
  StartPrompts.md (prompt 0 WORKING MODEL + ITERATION PROTOCOL + COMMIT+PUSH,
  prompts 1-4, Notes), the updated AgentBriefing.md workflow sections, the
  Status.md working-model note, and this entry. The local-gate discipline is
  enforced per-commit by the build + unit-gate + headless runs.
- **Reversibility:** trivial to revert the policy (re-enable branches/PRs by
  editing StartPrompts.md + AgentBriefing.md), but commits already landed on
  master stay on master.

## 2026-07-10 -- ZM-D-030 -- ZM_DataRegistry (name->ID lookups + cross-table enforcer) closes S1

- **Decision:** `ZM_DataRegistry` (`Source/Data/ZM_DataRegistry.{h,cpp}`) adds
  reverse name->ID lookups for every S1 table -- `ZM_FindSpeciesByName` /
  `...Move` / `...Item` / `...Ability` / `...Nature` / `...Scene` -- each an exact
  case-sensitive scan returning the id or that table's NONE sentinel (NONE for
  null/empty too). Linear scan (tables are <= ~220 rows; a hash index is a
  trivial later swap behind the same signatures). This is the last S1 box; the
  accompanying `ZM_Tests_DataRegistry` suite is the cross-table schema enforcer.
- **Why:** the reverse lookups are needed by save/load (names <-> ids across
  versions), WorldSpec/scene authoring, and debug/console tooling; and S1 needs a
  single place that asserts the tables are MUTUALLY consistent (per-table suites
  each check their own table, but nothing checked the references BETWEEN tables).
- **Tests that lock it:** `Tests/ZM_Tests_DataRegistry.cpp` (category `ZM_Data`, 9
  cases) -- name round-trip for all six tables (`Find(GetName(id)) == id`),
  unknown/null/empty -> NONE, and the cross-table resolves: every species
  evolution target, every TM's taught move, every WorldSpec encounter species, and
  every derived learnset move resolve to real rows. Boot suite 1172 ran / 0 failed;
  baseline bumped 1163 -> 1172. **S1 gate MET** (102 `ZM_*` unit tests vs the ~90
  target; no visual check for S1).
- **Reversibility:** easy -- additive `Source/Data/` files; swapping the linear
  scans for a hash index is behind the stable Find signatures.

## 2026-07-10 -- ZM-D-029 -- ZM_WorldSpec ships as SCHEMA + an 8-scene proving set; the full world is appended at S9/S10

- **Decision:** the keystone world table (ZM-D-005) lands its SCHEMA plus a small
  proving set, not the full world. `ZM_WorldSpec` (`Source/Data/ZM_WorldSpec.{h,
  cpp}`): one row per scene -- id / name / build index / `ZM_SCENE_KIND` (9 kinds:
  frontend/town/route/interior/gym/battle/tower/league/victory_road) / terrain set
  / warp connections (`ZM_SceneConnection` = target scene + spawn tag) / offered
  spawn tags / encounter table (`ZM_EncounterSlot` = species + level band +
  weight). Per-scene connection/tag/encounter arrays are static, referenced by
  pointer + count. The 8 proving scenes (FrontEnd 0, Battle 1, Dawnmere, Route 1,
  Thornacre, Player's Home, Aster's Lab, Gym 1) exercise every column. Accessors:
  `ZM_GetWorldSpec` / `ZM_GetSceneName` / `ZM_FindSceneByBuildIndex` /
  `ZM_SceneKindToString`. The full ~40-scene world is authored at S9/S10 by
  APPENDING rows (`ZM_SCENE_ID` is save-stable, append-only).
- **Why:** everything from S3 on (warps, encounters, gating, terrain authoring)
  flows through this table, so the schema + a referential-integrity test suite
  must exist first -- it is the enforcer that keeps ~40 scenes honest before any
  are baked. Shipping only the schema + proving set keeps S1 headless-data-only
  while locking the structure S3+ builds against.
- **Tests that lock it:** `Tests/ZM_Tests_WorldSpec.cpp` (category `ZM_Data`, 11
  cases) -- index self-consistency, unique names, unique build indices anchored
  (FrontEnd 0 / Battle 1), valid kinds, terrain-by-kind (outdoor has terrain,
  indoor does not), spawn tags non-empty + unique per scene, **every connection
  resolves to a real target + a spawn tag that target offers**, encounters
  route-only with real species + valid level bands + positive weight, **every
  non-Battle scene reachable from FrontEnd**, build-index round-trip, accessor +
  ToString. The graph was pre-validated offline before building. Boot suite 1163
  ran / 0 failed; baseline bumped 1152 -> 1163.
- **Reversibility:** easy -- additive `Source/Data/` files; the world grows by
  appending rows. Build-index assignments are cheap to change until S3 warps start
  referencing them through this table.

## 2026-07-10 -- ZM-D-028 -- Loop policy: local gate is the quality bar; auto-merge on zm-tests green; do NOT wait on / idle-watch CI

- **Decision (user-directed):** the autonomous loop must not sit blocking on CI.
  The authoritative verification of new behaviour is the LOCAL gate run before
  pushing -- `zenith build Zenithmon` + the boot unit gate
  (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, which runs the ZM_* unit
  tests that `zenith test` skips) + `zenith test Zenithmon --headless`. After
  opening the PR the loop enables GitHub auto-merge
  (`zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`), which lands the
  PR unattended the moment the sole required check `zm-tests` passes; it does NOT
  idle-watch checks, wait for the slower discipline gates (dp-tests/cb-tests/
  engine-gate/...), or launch a blocking `pr checks --watch`. The CI window is
  spent designing/prototyping the next task instead.
- **Why:** the full CI suite is ~25-30 min (dp-tests is the long pole) and was
  re-confirming behaviour the local gate already proves -- pure latency. `zm-tests`
  (~11 min) is the only machine-required check, so auto-merge on it is the fastest
  compliant path. `--admin`/bypass merges are BLOCKED by the harness permission
  classifier (verified 2026-07-10), so "nothing merges red" still holds -- zm-tests
  must actually go green; auto-merge just removes the human/agent wait.
- **Tests that lock it:** none executable; the contract is the updated
  StartPrompts.md (prompt 0 EXECUTION/PR+MERGE, prompts 1/2, Notes) + this entry.
  The local-gate discipline is enforced by the per-PR unit-gate + headless runs.
- **Reversibility:** trivial -- edit StartPrompts.md. Note this supersedes the
  prior "watch checks ... merge once ALL checks are green" wording in prompt 0
  (which implied waiting for the full suite).

## 2026-07-10 -- ZM-D-027 -- ZM_StatCalc (Gen-III+ integer formulas) + ZM_BattleRNG (PCG32) are the sanctioned math + randomness

- **Decision:** two pure-logic modules close most of the S1 formula surface.
  `ZM_StatCalc` (`Source/Data/ZM_StatCalc.{h,cpp}`) is the Gen-III+ integer stat
  formula -- `HP = ((2*base+IV+EV/4)*level)/100 + level + 10`; the other five =
  `(((2*base+IV+EV/4)*level)/100 + 5) * naturePercent/100`, truncating divisions,
  nature multiplier applied last via `ZM_GetNatureStatPercent` (ZM-D-025). No
  floating point. `ZM_BattleRNG` (`Source/Data/ZM_BattleRNG.h`, header-only) is a
  PCG32 generator (64-bit state, 32-bit output): `Next` / unbiased `RandBelow` /
  `RandRange` / `Chance` / `ChancePercent`, deterministic from a seed. It is the
  ONLY sanctioned randomness in game logic (never rand()/std::random).
- **Why:** the battle engine (S2) is seeded and must replay bit-for-bit, so both
  the stat math and the RNG must be integer-exact and reproducible. Landing them
  in S1 (with golden vectors) gives S2 a locked, tested foundation. PCG32 is small,
  fast, statistically strong, and trivially reproducible -- the standard choice for
  a deterministic game RNG. Default-constructed RNGs are fixed-seeded so an
  unseeded instance is never a hidden nondeterminism source.
- **Tests that lock it:** `Tests/ZM_Tests_StatCalc.cpp` (4) -- HP + other-stat
  golden vectors across level/IV/EV/nature, nature dispatch (HP nature-independent;
  raise/lower/unaffected stats), monotonicity. `Tests/ZM_Tests_BattleRNG.cpp` (6)
  -- **a golden 8-value stream pinning the exact PCG32 algorithm**, same-seed
  determinism, distinct-seed divergence, RandBelow/RandRange bounds, and the
  Chance/ChancePercent contract + ~50% frequency. All expected values were
  precomputed in a scratchpad model and matched on the first build. Boot suite
  1152 ran / 0 failed; baseline bumped 1142 -> 1152.
- **Reversibility:** easy -- additive `Source/Data/` files, no other module
  depends on them yet. The PCG32 constants + seeding sequence are load-bearing
  (the golden stream pins them); changing them is a deliberate golden-vector edit.

## 2026-07-10 -- ZM-D-026 -- ZM_AbilityData ships roster + metadata + a declared HOOK-SURFACE bitmask; fn-pointer hook bodies are S2

- **Decision:** the ~50-ability Roadmap sub-box lands as a compiled `const
  ZM_AbilityData` table (50 rows: id / name / description / `m_uHookMask`) plus an
  `ZM_ABILITY_HOOK` enum of 11 hook points as bit flags (SWITCH_IN / MODIFY_STAT /
  MODIFY_DAMAGE_DEALT / MODIFY_DAMAGE_TAKEN / STATUS_TRY / CONTACT / TURN_END /
  FAINT / ACCURACY / WEATHER / TYPE_IMMUNITY). Each ability declares WHICH hooks it
  will implement via the bitmask; the actual per-hook fn-pointer struct + bodies
  are deferred to S2. `ZM_AbilityHasHook(id, hook)` queries the surface.
- **Why:** the plan calls abilities "fn-pointer hook structs", but the hook
  signatures need the battle-state types (`ZM_BattleState`/`ZM_BattleEvent`) that
  do not exist until S2 -- wiring speculative signatures now would only churn
  (repo mandate: no legacy/compat). The bitmask is the non-speculative S1 slice:
  it fixes the roster + names + descriptions + each ability's hook surface (what
  the S2 executor must wire), is fully testable today, and references no
  not-yet-existing types. Mirrors the "data now, executor later" pattern used for
  moves (ZM-D-022) and items (ZM-D-024).
- **Tests that lock it:** `Tests/ZM_Tests_Abilities.cpp` (category `ZM_Data`, 6
  cases) -- index self-consistency (count == 50), unique names, non-empty
  descriptions, masks non-zero with no stray bits, **every hook bit used by >= 1
  ability**, and `ZM_AbilityHasHook` agreeing with the raw mask + name accessor.
  Boot suite 1142 ran / 0 failed.
- **Reversibility:** easy -- additive `Source/Data/` files; `ZM_ABILITY_ID` order
  is append-only. S2 grows the row with the fn-pointer struct (or a parallel hook
  table) keyed by id; the mask stays as the coverage/declaration record.

## 2026-07-10 -- ZM-D-025 -- ZM_NatureData is the exact 25-nature 5x5 grid (real table, not derived)

- **Decision:** the 25 natures land as a compiled `const ZM_NatureData` table
  (id / name / raised stat / lowered stat) that is exactly the 5x5 grid of
  (raised, lowered) pairs over the five non-HP stats (ATTACK / DEFENSE / SPATTACK /
  SPDEFENSE / SPEED); the five diagonal entries (raised == lowered) are the neutral
  natures. `ZM_GetNatureStatPercent(nature, stat)` returns the integer multiplier
  110 / 90 / 100 that `ZM_StatCalc` applies as `(stat * percent) / 100`.
- **Why:** natures are a small, exact, closed set (unlike the derived base-stat /
  learnset placeholders) -- 25 rows, one per stat pairing -- so a real hand-authored
  table is correct and final, not a placeholder. The percent helper keeps the
  x11/10 and x9/10 nature maths integer-exact and in one place for the S1 stat
  formula (box 6).
- **Tests that lock it:** `Tests/ZM_Tests_Natures.cpp` (category `ZM_Data`, 6
  cases) -- index self-consistency, unique names, raised/lowered always non-HP,
  **every (raised, lowered) pair present exactly once + exactly 5 neutral**, the
  110/90/100 percent contract (incl. HP always 100), and the name accessor.
- **Reversibility:** trivial -- additive `Source/Data/` files; names are flavour,
  the pairing is fixed by the mechanic.

## 2026-07-10 -- ZM-D-024 -- ZM_ItemData ships as data + schema only (90 items over a 34-kind effect enum); the bag/battle logic is S2/S5

- **Decision:** the ~80-item Roadmap box lands as a compiled `const ZM_ItemData`
  table (90 items) plus its schema -- `ZM_ITEM_ID` (90, save-stable), a 9-value
  `ZM_ITEM_CATEGORY` (ball / medicine / battle / held / berry / evo / TM / key /
  field), and a 34-kind `ZM_ITEM_EFFECT` executor tag. Each row carries category,
  buy + sell price, effect kind + a kind-specific param, a consumable flag, and
  (for TMs) a taught `ZM_MOVE_ID`. Rows are INERT: the bag/use/held-hook/catch
  logic that interprets `ZM_ITEM_EFFECT` is S2 (held items, catch math) / S5 (bag
  UI), mirroring the MoveData boundary (ZM-D-022). The 25 TMs each reference a
  real move; **this is the TM/tutor learnset seam** the learnset box deferred
  (ZM-D-023). Original names; no Nintendo IP.
- **Why:** items are battle-core scope (Scope.md); catching (S2/S5), the mart
  (S6), held items in battle (S2), and TM teaching all reference this table, so it
  must exist before them. Splitting data from the ~34-arm item executor keeps this
  a reviewable data drop. The effect enum is sized so each future per-effect
  handler has a data subject (a tested coverage invariant).
- **Tests that lock it:** `Tests/ZM_Tests_Items.cpp` (category `ZM_Data`, 11
  cases) -- index self-consistency (count == 90), unique names, valid
  category/effect enums, price sanity (sell <= buy) + key-item contract
  (priceless + effectless), consumable-flag-matches-category, TM-teaches-a-real-
  move (and only TMs teach), ball-only CATCH with multiplier >= 1.0x, stat/type
  param ranges, every effect kind used, every category populated, accessor +
  ToString contracts. The roster was validated offline before building. Boot suite
  1130 ran / 0 failed; zm-tests baseline bumped 1119 -> 1130.
- **Reversibility:** easy -- additive `Source/Data/` files; `ZM_ITEM_ID` order is
  append-only (save-stable). Per-item tuning (prices, effect params) is
  git-history, not decisions.

## 2026-07-10 -- ZM-D-023 -- Species learnsets are systematically DERIVED (placeholder), completing the ZM_SpeciesData box

- **Decision:** per-species level-up learnsets are computed by
  `ZM_GetSpeciesLearnset(id)` (`Source/Data/ZM_Learnsets.{h,cpp}`), not stored:
  it partitions the move table into the species' STAB / secondary-type / universal
  NORMAL damaging buckets + a shared status bucket, sorts the damaging buckets by
  effective power (fixed-damage moves read as high power so they land late),
  teaches a same-type damaging move at level 1, then round-robins damaging moves
  with a capped minority of status moves, spreading levels 1..~50 and sizing the
  list by evolution stage (12 / 14 / 16; single-stage finals 16). Returned by
  value as a fixed-capacity `ZM_Learnset` (max 16). This ticks the `ZM_SpeciesData`
  Roadmap box `[x]` (roster ZM-D-020 + base stats ZM-D-021 + learnsets here).
- **Why:** identical reasoning to base stats (ZM-D-021) -- real movepools are an
  S11 balance concern, and hand-authoring ~150 arbitrary placeholder movepools in
  one commit buys nothing over a deterministic, type-appropriate,
  referentially-valid derivation that unblocks S2 (which builds a monster's
  moveset from species + level). The accessor signature is the stable seam for a
  stored table later. TM/tutor compatibility is deferred to `ZM_ItemData`.
- **Tests that lock it:** `Tests/ZM_Tests_Learnsets.cpp` (category `ZM_Data`, 8
  cases) -- count bounded [4,16] + every entry a real move, level-ordered +
  in-range + something learnable by L5, type-appropriate (own type(s) or NORMAL),
  has a STAB move + an early damaging move, no duplicate moves, status a minority,
  deterministic, and learnset size non-decreasing along an evolution chain. The
  derivation was pre-validated across all 152 species offline before building.
  Boot suite 1119 ran / 0 failed; zm-tests baseline bumped 1111 -> 1119.
- **Reversibility:** easy -- replace the accessor body with a stored per-species
  table; no caller sees a difference. Additive `Source/Data/` files.

## 2026-07-10 -- ZM-D-022 -- ZM_MoveData ships as data + schema only (218 moves over a 57-kind effect enum); the executor is S2

- **Decision:** the ~220-move Roadmap box lands as a compiled `const ZM_MoveData`
  table (218 rows) plus its schema -- `ZM_MOVE_ID` (218 + save-stable
  `ZM_MOVE_COUNT`/`ZM_MOVE_NONE`), `ZM_MOVE_CATEGORY` (physical/special/status),
  `ZM_MOVE_TARGET` (opponent/self/field), and `ZM_MOVE_EFFECT` (57 executor tags).
  Each row carries type, category, power, accuracy, PP, priority, crit stage,
  contact, effect kind + proc chance + a kind-specific magnitude, and target. The
  rows are INERT: no behaviour, no damage/status pipeline -- the single
  `ZM_MoveExecutor` switch that interprets `ZM_MOVE_EFFECT` is deferred to S2
  (ZM-D-010). Original names throughout; the GDD-7.2 cuts (Substitute / Encore /
  Transform / weight moves) have no enum value.
- **Why:** moves are the dependency the species learnsets (the remaining
  `ZM_SpeciesData` sub-box) and the whole S2 battle engine reference, so the table
  must exist before either. Splitting data from the executor mirrors the
  battle-engine boundary already set in ZM-D-010 and keeps this PR a reviewable
  data drop rather than data + a 57-arm interpreter. The effect enum is sized so
  every S2 per-effect scenario (TestPlan 5.2) has a data subject; a tested
  coverage invariant guarantees no effect kind is dead.
- **Tests that lock it:** `Tests/ZM_Tests_Moves.cpp` (category `ZM_Data`, 16
  cases) -- index self-consistency (row i.m_eId == i, count == 218), unique
  non-empty names, valid type/category/target/effect enums, power<->category rule
  (status + fixed-damage powerless; else power in [10,250]), accuracy in [0,100]
  with own-side moves always-hit, PP/priority/crit ranges, effect-chance
  bi-conditional (chance 0 iff effect NONE) + status-moves-always-act, target
  derivable from category+effect, stat-magnitude [1,3], **every effect kind used
  >= 1**, every type has a move, category spread, priority/crit presence,
  accessor + ToString contracts. Boot suite 1111 ran / 0 failed; zm-tests baseline
  bumped 1095 -> 1111 in the same PR.
- **Reversibility:** easy -- additive `Source/Data/` files; the `ZM_MOVE_ID` order
  is append-only (save-stable). Per-move tuning values are git-history, not
  decisions; the struct may gain fields (e.g. contact already present) as S2
  needs them.

## 2026-07-10 -- ZM-D-021 -- Species base stats are systematically DERIVED (placeholder), not hand-tuned

- **Decision:** `ZM_GetSpeciesBaseStats(id)` computes the six base stats from a
  per-archetype stat profile (8 body-plan rows summing ~300) scaled by an
  evolution-stage factor (single-stage finals read as fully evolved) and a rarity
  factor, then a deterministic per-family emphasis/dock drawn from the family seed
  (bump one stat, dock another). Stats are NOT stored per-row and are NOT
  balance-tuned. Added the `ZM_STAT` enum + `ZM_BaseStats` struct.
- **Why:** hand-authoring 152 x 6 balanced stats in one commit is huge and
  error-prone, and balance is explicitly an S11 concern (headless AI-vs-AI). A
  systematic, deterministic derivation unblocks S2 (scripted battles need fixed
  base stats) and differentiates species by archetype AND family. It is trivially
  superseded by a stored per-species table in a later balance pass -- the accessor
  signature is the stable seam.
- **Tests that lock it:** `Tests/ZM_Tests_Species.cpp` `BaseStats_*` (5) --
  in-range [1,255]; totals banded by evolution role (stage-1 base 250-360,
  single-stage >=480, legendary >=560, global 250-700); every stat non-decreasing
  + BST strictly increasing along an evolution chain; archetype shapes (AVIAN
  faster than BLOB, BLOB bulkier than AVIAN, BIPED hits harder than FLOATER);
  family variety (>=60 distinct stat blocks). Boot suite 1095 ran / 0 failed.
- **Reversibility:** easy -- replace the accessor body with a stored table; no
  caller sees a difference.

## 2026-07-10 -- ZM-D-020 -- ZM_SpeciesData decomposed: structural roster first (152 species), base stats + learnsets deferred

- **Decision:** the ~150-species SpeciesData Roadmap box is split across
  increments on the same box. Increment 1 (this PR): the `ZM_SpeciesData` schema
  + supporting enums (`ZM_ARCHETYPE`/`ZM_RARITY`/`ZM_SIZE_CLASS`/`ZM_SPECIES_ID`)
  + the full 152-species STRUCTURAL roster (id / name / type(s) / archetype /
  evo-stage / evolves-to / family / rarity) transcribed from GDD section 5, with
  size class + family seed as rule-derived accessors. DEFERRED to later
  increments on this box: per-species base stats (a design pass) and learnsets
  (need `ZM_MoveData`, box 3). The Roadmap box stays a WIP (`[~]`).
- **Why:** 152 species with hand-designed base stats + every field in one commit
  is a large, error-prone, hard-to-review PR; a structural-roster-first split is
  standard practice for big data tables and is dependency-correct (learnsets
  reference moves that do not exist yet). The roster is the foundation that
  MoveData learnsets, WorldSpec encounters, DataRegistry, and S4 asset gen all
  reference.
- **Tests that lock it:** `Tests/ZM_Tests_Species.cpp` (category `ZM_Data`) -- 11
  integrity tests: count==152 + index self-consistency, unique names, valid
  types, evolution-graph shape (stage+1, same family/archetype/rarity, no
  self-loop), families well-formed (linear chains, one species per stage),
  family-size distribution (40/13/6 vs GDD), base/final/legendary counts,
  archetype-family spread (18/6/7/4/6/7/5/6), every-type-on-two-families,
  family-seed consistency+uniqueness, size-class monotonicity. Boot suite 1090
  ran / 0 failed.
- **Reversibility:** easy -- additive `Source/Data/` files; the struct grows
  (base-stats + learnset fields) in follow-up increments; the `ZM_SPECIES_ID`
  order is save-stable (append-only).

## 2026-07-10 -- ZM-D-019 -- Zenithmon boot unit tests are gated in CI via a run_unit_gate.ps1 boot step (ratcheted baseline)

- **Decision:** `zm-tests.yml` gains a step that boots `zenithmon.exe` headless
  through the shared `Tools/run_unit_gate.ps1` (`-Baseline 1079`) to run the boot
  ZENITH_TEST suite (engine units + Zenithmon `ZM_*` cases) and fail on any
  failure. The baseline is an exact-count ratchet (like engine-gate): each PR that
  changes the `ZM_*` unit count -- or an engine PR that changes the engine unit
  count -- bumps the number in the same PR.
- **Why:** discovered while landing S1 -- both `zenith test` (harness default) and
  the two prior zm-tests steps pass `--skip-unit-tests`, so Zenithmon's unit tests
  (the S1/S2 gate backbone, ~460 cases at end state) NEVER ran in CI. The plan
  designates the boot unit suite as the CI backbone; DP/CB never hit this because
  they carry almost no game-side unit tests. `run_unit_gate.ps1` is the proven
  engine-gate pattern (tool-exports ON so asset-export units work; watchdog-kills
  the known tools-build idle after the units line is logged).
- **Tests that lock it:** the step itself (red on any unit failure or count !=
  baseline); validated locally = "1079 ran, 1078 passed, 0 failed, 1 skipped" (the
  1 skip is the pre-existing quarantined `GraphComponent::RegistryWideNodeRoundTrip`).
- **Reversibility:** easy -- delete the step. The baseline's coupling to the
  engine unit count is the known maintenance cost (CIPolicy.md section 1); a
  follow-up may switch to a failures-only check if the ratchet churns
  (Questions.md Q-2026-07-10-004).

## 2026-07-10 -- ZM-D-018 -- Type system: save-stable ZM_TYPE enum + golden-locked 18x18 chart with a dual-type product API

- **Decision:** the 18 types are one `enum ZM_TYPE : u_int` (`Source/Data/ZM_Types.h`)
  in the GDD-section-6 order, which is simultaneously the dex/UI order and the
  row/column order of the chart; the range is append-only (save-stable). The
  effectiveness matrix is a `const` 18x18 float table (`ZM_TypeChart.cpp`) of
  {0, 0.5, 1, 2}, mapping the standard 18-type relationships onto the original
  names. Lookups are a stateless namespace `ZM_TypeChart`:
  `GetEffectiveness(atk, def)` + `GetDualTypeEffectiveness(atk, def1, def2)`, where
  `def2 == ZM_TYPE_NONE` (== `ZM_TYPE_COUNT`) collapses to the single lookup and a
  duplicated slot is never squared.
- **Why:** types are consumed by species/moves/damage from S1 on; a save-stable
  enum + a compiled table keeps zero file I/O in headless tests (ZM-D-009) and
  makes the chart diffable. The dual-type product belongs with the chart (4x /
  0.25x / 0x matchups) and is testable before species exist.
- **Tests that lock it:** `Tests/ZM_Tests_Data.cpp` -- `TypeChart_MatchesGolden`
  (an independent golden 18x18 compiled into the TU: the two-place-change lock,
  TestPlan 5.1), `TypeChart_AllCellsLegal`, `TypeChart_ImmunityCountIsEight`, the
  GDD design-intent spot checks (`StarterTriangle`, `SecondTriangleAndGhostNormal`,
  `DrakeChecks`, `ImmunitiesAndIronWall`), `TypeChart_DualTypeProducts`,
  `Types_ToStringContract`.
- **Reversibility:** easy -- additive `Source/Data/` files, no engine change;
  reordering/renaming types is a save-migration concern only after content ships.

## 2026-07-10 -- ZM-D-017 -- Docs/ becomes a self-sufficient autonomy hub: MasterPlan committed, lifecycle-loop prompt, hard-stop visual gates, permission allowlist

- **Decision (user-directed):** the Docs directory must carry the whole
  project lifecycle with the only human inputs being (a) pasting/looping a
  StartPrompts.md prompt and (b) visual-gate sign-offs. Changes: the approved
  program plan is committed as MasterPlan.md (it previously lived only in a
  machine-local `~/.claude/plans/` file) and referenced from every start
  prompt; StartPrompts.md gains prompt 0 (idempotent lifecycle-loop iteration,
  carries the user's standing merge-on-green authorization for the loop's own
  PRs) and prompt 4 (gate sign-off); `Tools/zenith_gh.ps1` wraps gh with
  self-bootstrapping auth; a checked-in `.claude/settings.json` allowlists the
  loop's build/test/git/gh commands (exact rules user-approved).
- **Gate policy (user's explicit choice):** the loop HARD-STOPS at every
  stage's visual check (incl. S4 gallery, S8 go/no-go) -- automated gate items
  run, screenshot evidence is captured, Status.md gets a `GATE-WAIT: S<n>`
  marker, and nothing proceeds until the user's prompt-4 sign-off lands in
  this log. The loop never signs its own gates.
- **Why:** S0 proved the failure modes: the plan file was unversioned and
  machine-local; gh had no session auth; permission prompts and the
  self-merge guard stall unattended runs; `gh run rerun` cannot re-evaluate
  against new master.
- **Tests that lock it:** none executable; the contract is the prompts +
  allowlist themselves (version-controlled) and this entry.
- **Reversibility:** trivial -- edit StartPrompts.md / delete the allowlist;
  gate policy can be relaxed by a new user decision here.

## 2026-07-10 -- ZM-D-016 -- Master branch protection CREATED with `zm-tests` as the sole machine-enforced required check

- **Decision:** master had NO branch protection and no rulesets at all (the
  repo's "required checks" had been purely conventional). On the user's
  direction ("Add zm-tests yourself"), classic branch protection was created
  via the API: required status checks `[zm-tests]`, `strict=false`,
  `enforce_admins=false`, no required reviews.
- **Why:** the S0 gate requires zm-tests to actually block merges;
  `enforce_admins=false` preserves the owner's established direct-push
  workflow (agents always land via PRs, so agents are always gated). Other
  gates stay blocking-by-discipline because several are path-filtered and a
  required check that never reports deadlocks a PR.
- **Tests that lock it:** none (GitHub configuration); verified by
  `gh api repos/tomosh22/Zenith/branches/master/protection`.
- **Reversibility:** trivial (delete/edit the protection rule); recorded in
  CIPolicy.md section 4 + ManualSetupChecklist.md.

## 2026-07-10 -- ZM-D-015 -- Three pre-existing master-red CI gates fixed as a prerequisite PR rather than inherited red

- **Decision:** engine-gate, layering-gate, and scaffold-smoke had been red on
  master since 2026-07-07/08 (before Zenithmon existed). Rather than merging
  S0 with inherited red checks, they were fixed in a dedicated PR (#144,
  `0844689e`): unit baseline 1053->1068 single-sourced in
  `Tools/run_unit_gate.ps1` (test_scaffold.ps1 reuses it), `Flux_HDR.cpp`
  g_xEngine reaches reduced via the established local-hoist idiom (fixed, not
  allow-listed), and regen.ps1 given a dotnet-exec fallback on the tracked
  Sharpmake dll (+ scaffold-smoke got `lfs: true` and the standard
  `/p:WindowsTargetPlatformVersion=10.0` build override).
- **Why:** "nothing merges red" is only meaningful if master itself can go
  green; every future Zenithmon stage PR needs a green baseline.
- **Tests that lock it:** the gates themselves (all 9 checks green on #144;
  all 10 green on the rebased #143); scaffold smoke 11/0 locally.
- **Reversibility:** each fix is independent and small; the baseline bump is
  a ratchet (future engine-test additions bump it again in ONE place).

## 2026-07-09 -- ZM-D-014 -- Engine name-validation narrowed to a PascalCase word boundary so 'Zenithmon' is a legal game name

- **Decision:** `zenith new Zenithmon` was rejected by the blanket
  `Zenith*`/`Sentinel*` reserved-prefix rule in BOTH game-name validators: PS
  `Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` and C++
  `ZenithHub_GameScan::ValidateName`. Both were narrowed to a PascalCase word
  boundary: reject `Zenith`/`Sentinel` alone or followed by an uppercase letter
  or digit; a lowercase continuation (e.g. `Zenithmon`) is a distinct word and
  valid.
- **Why:** the reservation exists to protect engine/test module names
  (`ZenithECS`, `SentinelAI`, ...); `Zenithmon` collides with none of them --
  the blanket rule was broader than its intent.
- **Tests that lock it:** `Build/Tests/run_buildsystem_tests.ps1` (suite 45
  passed / 0 failed) + the shared pinned vectors in
  `Tools/ZenithCli/Tests/name_validation_cases.txt` (consumed by both
  validators) + the ZenithHub selftest.
- **Reversibility:** reverting the validators would orphan this project --
  reversible only by renaming the game.

## 2026-07-09 -- ZM-D-013 -- No per-game runner script; the unified `zenith test` harness is the only test runner

- **Decision:** Zenithmon never gets a `run_zm_tests.ps1`. All test execution --
  local, stage gates, and CI (`.github/workflows/zm-tests.yml`) -- goes through
  `zenith test Zenithmon` (`Tools/ZenithCli/ZenithCli.psm1` ->
  `ZenithTestHarness.psm1`; flags `--filter/--headless/--results-dir/--config/
  --per-process/--fail-fast`; exit codes 0 OK / 1 usage / 2 validation /
  3 generation / 4 build-or-test / 5 not-found).
- **Why:** the old per-game `Tools/run_*_tests.ps1` scripts were DELETED at
  commit `c29e28f8` in favor of the unified harness; a per-game script would be
  legacy surface on day one (repo mandate: no legacy/compat code).
- **Tests that lock it:** the `zm-tests` CI workflow invokes
  `zenith.bat test Zenithmon --headless` directly; every stage gate in
  [Roadmap.md](Roadmap.md) cites the same command.
- **Reversibility:** none needed -- a per-game script would be a policy
  violation, not an option.

## 2026-07-09 -- ZM-D-012 -- Scene build index 0 = FrontEnd.zscen, boot-authored title screen (DP convention)

- **Decision:** the game boots into `FrontEnd.zscen` at build index 0: camera +
  "Zenithmon" title text + the game component. The scene is boot-authored by
  tools builds (editor-automation steps re-author it every tools boot) and the
  baked `.zscen` is git-ignored. The build-index table follows the plan:
  0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road, 40+ interiors,
  95 Tower (exact per-scene assignments TBD at S9/S10 via ZM_WorldSpec).
- **Why:** matches the proven DevilsPlayground convention (index 0 = FrontEnd,
  boot-authored, reloaded between batched tests by the harness).
- **Tests that lock it:** `Tests/ZM_AutoTests_Boot.cpp` (`ZM_Boot_Test`) + the 2
  boot unit tests in `Tests/ZM_Tests_Boot.cpp`.
- **Reversibility:** index remaps are cheap until S3, when warps start
  referencing build indices through ZM_WorldSpec.

## 2026-07-09 -- ZM-D-011 -- S0 keeps the scaffold placeholder ZM_GameComponent (bobbing cube)

- **Decision:** `ZM_GameComponent` (registered `"ZM_Game"`, serialization
  order 100) retains the `zenith new` scaffold's bobbing-cube behaviour as the
  S0 placeholder until the S1 data core and S3 overworld systems land.
- **Why:** S0 is skeleton/harness/CI/docs only; a live registered component
  proves the registration + serialization + between-tests plumbing without
  inventing gameplay ahead of its stage.
- **Tests that lock it:** `Tests/ZM_Tests_Boot.cpp` (2 unit tests) +
  `ZM_Boot_Test` -- these pin boot health, not the cube; the placeholder is
  free to be replaced.
- **Reversibility:** trivial -- it is a placeholder by design.

## 2026-07-08 -- ZM-D-010 -- Battle engine is headless C++ (not Behaviour Graphs) with an append-only event stream

- **Decision:** the battle turn loop is a seeded, deterministic C++ state
  machine (`ZM_BattleEngine`): `Begin(config,seed)` -> `SubmitAction` ->
  `ResolveTurn()` -> append-only `ZM_BattleEvent` stream, the single source of
  truth for both tests and presentation; the engine never formats strings or
  touches UI. Behaviour graphs are glue only (menu flow, NPC events, cutscene
  beats). Origin: the approved plan, `zenithmon-pok-mon-nested-puddle.md`.
- **Why:** rule-based logic needs exact-replay determinism and full headless
  unit-test coverage; no in-repo turn-based graph reference exists.
- **Tests that lock it:** S2 gate (~370 unit tests incl. scripted seeded
  battles with exact expected event streams + 2,000-battle fuzz soak).
- **Reversibility:** low -- reversing means rewriting S2; do not revisit.

## 2026-07-08 -- ZM-D-009 -- Game data = compiled const C-array tables, not disk assets

- **Decision:** species/moves/items/abilities/natures/type chart/encounters/
  trainers/dex text live as `const` C arrays in `Source/Data/*.cpp`; the
  "assets baked to disk" mandate covers meshes/textures/anims only. Origin: the
  approved plan.
- **Why:** compile-time validated, diffable in review, zero file I/O in
  headless CI tests.
- **Tests that lock it:** the `ZM_Tests_Data` validation suite +
  `ZM_DataRegistry` integrity tests (S1 gate).
- **Reversibility:** mechanical to move tables to files later, but it would
  sacrifice the zero-I/O headless-CI property -- user decision required.

## 2026-07-08 -- ZM-D-008 -- Battle format: singles only

- **Decision:** all battles are 1v1 singles; doubles is an explicit scope cut
  (see [Scope.md](Scope.md)). Struct layout does not preclude doubles later.
  Origin: the approved plan.
- **Why:** doubles is roughly 2x targeting/AI/UI complexity for marginal value.
- **Tests that lock it:** the entire S2 battle suite assumes single active
  monster per side.
- **Reversibility:** additive later behind a new scope decision; nothing to
  unwind now.

## 2026-07-08 -- ZM-D-007 -- Overworld-to-battle = ADDITIVE battle scene at world offset (0, -2000, 0) with overworld pause

- **Decision:** encounters load the battle scene ADDITIVE at (0, -2000, 0),
  `SetScenePaused(overworld, true)`, switch camera/HUD, and `UnloadScene` on
  exit; one battle scene with ~6 swappable biome dressing sets, enclosed by a
  backdrop dome. Documented fallback if visual isolation fails: SINGLE load +
  world-state snapshot. Origin: the approved plan.
- **Why:** a SINGLE reload resets render systems + physics and re-streams
  terrain -- seconds of hitch at wild-encounter frequency.
- **Tests that lock it:** S5 gate windowed round-trip tests (exact overworld
  resume) + screenshot check for overworld bleed-through at the offset.
- **Reversibility:** medium -- the fallback path is designed and documented;
  switching costs the S5 transition work only.

## 2026-07-08 -- ZM-D-006 -- Door/route-edge transitions = SINGLE loads with spawn tags; player/camera are NOT persistent entities

- **Decision:** `ZM_WarpTrigger_Component {targetBuildIndex, spawnTag}` -> fade
  -> SINGLE load; the persistent `ZM_GameStateManager` (`DontDestroyOnLoad`)
  respawns player + follow camera at the tagged `ZM_SpawnPoint`. One-time
  placement at load is not gameplay teleportation. Origin: the approved plan.
- **Why:** SINGLE loads reset physics and would orphan a persistent Jolt body;
  respawn-at-tag is the safe pattern.
- **Tests that lock it:** S3 gate windowed door/warp round-trip test; later
  per-region traversal tests walk every warp edge (S9/S10).
- **Reversibility:** medium -- changing to persistent player entities requires
  engine work on physics-across-SINGLE-loads first.

## 2026-07-08 -- ZM-D-005 -- ZM_WorldSpec is the keystone world table

- **Decision:** one declarative compiled table describes the whole world --
  scenes (name, build index, kind, terrain set, encounter table + rate),
  connections/spawn tags, trainers, shops, gyms, story beats. Tools walk it to
  author terrains/scenes/graphs; runtime walks it for warps/encounters/gating.
  Origin: the approved plan.
- **Why:** ~40 scenes without 40 bespoke authoring functions; one source of
  truth keeps authoring, runtime, and tests in agreement.
- **Tests that lock it:** WorldSpec referential-integrity unit tests (every
  warp target, spawn tag, species, and trainer resolves) run on every PR.
- **Reversibility:** low -- everything from S3 on flows through it; treat as
  load-bearing.

## 2026-07-08 -- ZM-D-004 -- Tall grass samples a game-owned CPU copy of the density map

- **Decision:** `ZM_TallGrassSystem` loads the baked `GrassDensity.ztxtr` per
  outdoor scene, feeds `g_xEngine.Grass().SetDensityMap(...)` for rendering,
  and keeps its OWN CPU copy for gameplay sampling; player XZ quantized to 1 m
  tiles, encounter roll on tile transition where density >= 0.5; density map
  cleared on interiors/battle. Origin: the approved plan.
- **Why:** engine `SampleDensityMap` returns 1.0 when no map is set -- gameplay
  must not inherit that trap, and the grass singleton is render-owned state.
- **Tests that lock it:** S5 gate encounter tests (walk grass until encounter
  with rigged RNG); grass-state assertions at S9/S10 gates.
- **Reversibility:** low cost -- swapping to engine-side sampling is a small,
  local change if the engine semantics ever harden.

## 2026-07-08 -- ZM-D-003 -- Baked assets are git-ignored, regenerated under per-family manifest guards

- **Decision:** everything under `Games/Zenithmon/Assets/` is git-ignored (repo
  norm) and regenerated by tools builds; per-family manifest guards =
  generator-version stamp + file-existence (hardened RenderTest pattern).
  Consequence: a fresh CI checkout has NO assets, so every asset/scene-dependent
  automated test must exists-guard and `RequestSkip` (the CI-fix pattern from
  commit `94813489`). Origin: the approved plan.
- **Why:** ~30-50 min cold bake output does not belong in git; determinism makes
  the repo the recipe, not the artifact.
- **Tests that lock it:** bake-determinism gate (re-run tools boot -> zero
  diffs, byte-identical re-bake) + the CI headless suite passing on an
  assets-absent runner.
- **Reversibility:** policy-level; committing baked assets would need a repo-
  wide user decision.

## 2026-07-08 -- ZM-D-002 -- Engine changes scoped to E1-E5, all additive and back-compatible

- **Decision:** the only engine-level changes this project makes are: E1
  per-component serialized terrain-set name (replaces 6 hard-coded `Terrain/`
  path sites); E2 `AddStep_TerrainExportChunksRect` + streaming-path
  missing-chunk tolerance check; E3 `Zenith_UIText` typewriter reveal; E4
  `Zenith_UIGridLayoutGroup`; E5 grass singleton reset hygiene (wire
  `Grass().Reset()` into `ResetRenderSystems` + clear instances/flags/density
  map). Each lands with unit tests + a RenderTest boot regression check.
  Origin: the approved plan.
- **Why:** verified engine gaps (one-terrain-per-game, full-grid bake volume,
  no typewriter/grid widgets, grass state leaking across SINGLE loads) --
  scoping them up front prevents ad-hoc engine sprawl.
- **Tests that lock it:** per-change unit tests + RenderTest still boots green
  (default-path untouched) + DP/CB suites stay green.
- **Reversibility:** per-change -- each is additive with a legacy-default path,
  so individually revertable before Zenithmon content depends on it.

## 2026-07-03 -- ZM-D-001 -- Scope lock (user decisions)

- **Decision:** the in/out scope for Zenithmon is locked as recorded in
  [Scope.md](Scope.md): ~150-species dex / 18 types / 3-stage lines / rarity;
  classic 8-gym world with no Wild Area; the full battle core; extras =
  abilities, natures, IVs/EVs, weather + terrain effects, breeding; post-game =
  Champion rematch + Battle Tower. Out: audio, networking/multiplayer/trading,
  Dynamax-analog, doubles, Substitute/Encore/Transform/weight moves, open Wild
  Area.
- **Why:** product of multiple prior iteration rounds with the user; frozen to
  prevent scope creep across a ~13-stage build.
- **Tests that lock it:** [Scope.md](Scope.md) is the binding gate; stage gates
  audit shipped content against it.
- **Reversibility:** user decision only, recorded as a new entry in this log
  (Scope.md change-control rule).
