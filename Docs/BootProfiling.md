# Boot profiling — the measured breakdown

Companion to the mechanism documented in
[Zenith/Profiling/CLAUDE.md](../Zenith/Profiling/CLAUDE.md) ("Boot capture"). That
section says how the capture works; this one says **what it found**.

Everything below is measured, not derived. The metric is the always-on
`[Profiling] BootSummary totalMs=…` line, which the profiler emits on every run —
including under `--skip-boot-capture` — precisely so a calibration run is directly
comparable with a captured one. The per-phase and per-zone numbers come from the
`--boot-profile-dump` artifacts.

## Method

- One dev machine, otherwise idle, 2026-08-03. Windowed Vulkan builds.
- Per (exe, variant): **1 discarded warm-up run + 2–3 measured runs, median reported.**
  Spread within a variant was consistently under 1% except at the 100 s+ scale
  (~2%), so the ranking below is far outside the noise.
- Runs use `--exit-after-frames 3`; the process is killed once the summary line and
  the frame-1 artifact have landed (these games idle at shutdown by design).
- **Warm** asset/bake state throughout. A cold first boot is a separate, much
  larger number — see *Cold vs warm* below.
- The workspace's tracked content was byte-identical to `HEAD` + this change for
  every run (`git status` on `Games/Zenithmon/Assets/` clean).

## Headline: boot is dominated by two opt-out-able phases

Every cell below is a **directly measured phase duration** from that config's own
boot artifact — the `UnitTests` marker pair and the `Boot InitialiseAssets` body
zone — divided by that same run's total. They are NOT cross-variant subtractions:
at the 100 s scale the unit-test phase varies by ~2% run to run, which is enough
to corrupt a subtraction between two separate runs by seconds.

| Config | Total boot (artifact run) | `UnitTests` | Tool exports | Engine-fixed remainder |
|---|---:|---:|---:|---:|
| **Zenithmon Debug** | **114,624 ms** | 100,353 ms (**87.6%**) | 7,649 ms (6.7%) | 6,425 ms (5.6%) |
| Zenithmon Release | 33,888 ms | 26,967 ms (**79.6%**) | 1,954 ms (5.8%) | 4,987 ms (14.7%) |
| **RenderTest Debug** | **52,835 ms** | 32,748 ms (**62.0%**) | 13,491 ms (25.5%) | 5,999 ms (11.4%) |
| RenderTest Release | 14,272 ms | 5,590 ms (**39.2%**) | 3,692 ms (25.9%) | 4,848 ms (34.0%) |

(Medians over all measured runs, for reference: ZM Debug 116,750 ms, ZM Release
33,897 ms, RT Debug 52,835 ms, RT Release 14,387 ms. The artifact-run totals above
are used for the percentages so each row is internally consistent.)

Variant totals the table is derived from (median ms):

| Variant | ZM Debug | ZM Release | RT Debug | RT Release |
|---|---:|---:|---:|---:|
| baseline | 116,750 | 33,897 | 52,835 | 14,387 |
| `--skip-unit-tests` | 13,931 | — | 19,821 | — |
| `--skip-tool-exports` | 104,576 | — | 35,521 | — |
| both | 6,425 | 4,987 | 5,999 | 4,848 |

**The cross-game control does its job.** The "both" column is nearly identical for
all four rows (4.8–6.4 s) — that is the engine-fixed cost, and it is the same
engine in every case. Everything above it is content and policy, not the engine.

## Drill-down

### 1. `RunAllTests` at boot — 100.4 s of a 114.6 s Zenithmon Debug boot

The single largest item by an order of magnitude. 2,891 unit tests, each with a
full scene reset. Release cuts it to 27.0 s — still 79.6% of that boot.

Its share falls as the *other* costs rise, not because the tests get cheaper: at
RenderTest Release it is 39.2%, because that config's tool exports (25.9%) and
engine-fixed floor (34.0%) are proportionally much larger. In absolute ms it is
the largest single item in three of the four configs, and second only to the
engine floor in the fourth.

This is a *policy* cost, not an engine cost: it is 0 ms with `--skip-unit-tests`,
and the automated-test harness already passes that flag by design.

#### 1a. Which tests — `--unit-test-timings[=path]`

`Zenith_TestRunner` now times every case and can dump the whole list, slowest
first, with per-test and cumulative share. It splits the cost two ways, because
they have different fixes:

- **body** — the test itself,
- **reset** — the global-state reset the *runner* performs after each test on its
  behalf (`Zenith_SceneSystem::ResetForNextTest`).

First measured over 2,892 tests on a Null Zenithmon boot: **136,267 ms total =
119,981 ms body (88.0%) + 16,285 ms reset (12.0%)**. After the fixes in §1b:
**83,527 ms**.

**The suite is not slow; ~21 tests are.** (Figures below are the ORIGINAL
measurement — the one that motivated the fixes.)

| Cost band | tests | ms | share |
|---|---:|---:|---:|
| ≥ 1000 ms | 21 | 91,970 | 67.5% |
| ≥ 500 ms | 25 | 94,759 | 69.5% |
| ≥ 100 ms | 91 | 106,285 | 78.0% |
| ≥ 10 ms | 671 | 123,949 | 91.0% |

Top 10 tests = **56.6%** of the suite; top 25 = 69.5%; top 100 = 78.6%.

| # | Test | total ms |
|---:|---|---:|
| 1 | `ZM_TerrainAuthoring::DawnmereManifestRequiresEveryOutput` | 27,061 |
| 2 | `ZM_Gen::CreatureGen_UniversalBundlePerSpecies` | 13,374 |
| 3 | `IBLEnvironment::SimulatedDayAt60FpsKeepsCompletingGenerations` | 13,041 |
| 4 | `IBLEnvironment::SimulatedDayAt30FpsNeitherRestartsNorStarves` | 6,045 |
| 5 | `ZM_Save::Truncation_EveryByteBoundaryReturnsCorruptData` | 3,784 |
| 6 | `TerrainEditor::ProceduralGenerationIsDeterministic` | 3,604 |
| 7 | `ZM_Gen::HumanGen_AppearanceAlbedoStructural` | 2,960 |
| 8 | `ZM_TerrainRecipeSet::ManifestsEncodePerRecipeCounts…` | 2,765 |
| 9 | `ECS::QuerySparseLegacyEquivalence` | 2,324 |
| 10 | `TerrainEditor::AutoSplatWeightsSumTo255` | 2,138 |

By category:

| Category | tests | ms | share |
|---|---:|---:|---:|
| `ZM_Gen` | 140 | 36,935 | 27.1% |
| `ZM_TerrainAuthoring` | 3 | 27,069 | 19.9% |
| `IBLEnvironment` | 19 | 19,479 | 14.3% |
| `TerrainEditor` | 17 | 9,093 | 6.7% |
| `Automation` | 97 | 7,041 | 5.2% |
| `ZM_Save` | 133 | 6,265 | 4.6% |

**The reset is a flat tax, and it is the floor for everything else.** Median test
*body* is **0.015 ms**; median *reset* is **5.26 ms** (mean 5.63 ms). 2,214 of 2,892
tests (77%) have a body under 1 ms, and 2,036 under 0.1 ms — so for the large
majority of the suite, the reset is essentially the entire cost. That is why no
test measures under 1 ms total despite most doing almost nothing.

Two independent levers follow, and they act on different halves:

- **Fix or gate the ~21 heavyweights** → up to 92 s (67.5%), most of it content
  generation and terrain authoring rather than engine logic. These read like
  integration tests living in the unit suite.
- **Make the per-test reset cheaper or conditional** → up to 16.3 s, and it is the
  only lever that helps the ~2,200 tests that currently cost nothing but the tax.

#### 1b. What the first lever actually recovered — measured

Five tests were changed, none removed, none weakened (test count unchanged at
2,892; **0 failed** before and after). Each change removes redundant work, not
coverage:

| Test | before | after | Δ | change |
|---|---:|---:|---:|---|
| `ZM_TerrainAuthoring::DawnmereManifestRequiresEveryOutput` | 27,061 | 1,173 | **−25,889** | O(n²) warm-check loop over all 771 outputs → 8 sampled representatives |
| `IBLEnvironment::SimulatedDayAt60Fps…` | 13,041 | 1,925 | **−11,116** | 7,200-frame horizon → 1,440; **deg/frame unchanged** |
| `IBLEnvironment::SimulatedDayAt30Fps…` | 6,045 | 695 | **−5,350** | 3,600-frame horizon → 720; deg/frame unchanged |
| `ZM_Gen::HumanGen_AppearanceAlbedoStructural` | 2,960 | 1,307 | **−1,653** | 4 generator runs per human → 2; determinism already owned by `HumanGen_SameSeedDeterminism` (which covers every id) |
| `ZM_Save::Truncation_EveryByteBoundary…` | 3,784 | 3,385 | **−399** | loop-invariant fixture hoisted out of the per-byte loop; sweep still exhaustive |
| **Edited set** | **52,891** | **8,484** | **−44,407 (−84.0%)** | |

Observed suite total went 136,267 → 83,527 ms (**−38.7%**). Note the honest
discount: only **−44.4 s is attributable** to these edits. The remaining ~8 s
appears on the 2,887 tests that were *not* touched, and two consecutive post-fix
runs differ by only 0.46% on that same set — so it is a systematic artefact of the
original measurement session (first execution of a freshly linked binary, cold OS
cache), not ordinary noise and not an effect of these changes.

**Why each is safe rather than merely faster:**

- The Dawnmere loop deleted and restored all 771 outputs, taking a full warm check
  each time — and `ZM_IsTerrainBakeWarm` stats *every* output, so it was ~594,000
  filesystem syscalls. The warm check has no per-index branch, so honouring index 0
  and index 770 honours everything between by construction; what's genuinely at risk
  is a whole *category* dropping out of enumeration, which the untouched
  `uRender==256 / uRenderLow==256 / uPhysics==256 / uTextures==3` assertions already
  cover exhaustively and for free. `ZM_TerrainRecipeSet::ManifestsEncode…` proves the
  point: same invariant, sampled, **all five recipes**, a tenth of the cost.
- The IBL tests pin defects that depend on the per-frame angular step (0.05°/frame at
  60 FPS, below the ~0.081° capture threshold; 0.1°/frame at 30 FPS, crossing it
  every frame). That step is `fDt * fDegreesPerSecond` and is **unchanged** — only the
  horizon shrank. Both bugs produced *zero* completed generations, so a lower bound
  of >20 separates them as decisively as >100 did.

**Not changed, and why.** `TerrainEditor::ProceduralGenerationIsDeterministic` and
`AutoSplatWeightsSumTo255` were on the shortlist as "use a test-sized terrain" — but
`uHEIGHTFIELD_SIZE` (4096) and `uSPLATMAP_SIZE` (2048) are *production* constants,
so there is no test-only grid to shrink. The remaining lever (dropping FBM octaves
2→1) would trade real coverage — the octave accumulation is part of what determinism
means here — for perhaps a third of the cost. Left alone deliberately.

### 2. Flux `LateInitialise` — the whole engine-fixed remainder

With units and exports skipped, `Boot InitialiseGPUAssets` **is** the boot:

| ZM Release, units+exports skipped | ms | share of 4,987 ms |
|---|---:|---:|
| `Boot InitialiseGPUAssets` | 4,433 | 89% |
| ↳ `Boot Flux LateInitialise` | 4,410 | 88% |
| `Boot InitialiseRendererAndPhysics` | 511 | 10% |
| ↳ `Boot Flux EarlyInitialise` | 508 | 10% |
| ↳↳ `Boot Flux/Backend Initialise` (device create) | 456 | 9% |
| everything else (assets, ECS, editor, project) | ~46 | ~1% |

Inside `LateInitialise`, cost is the per-feature init ladder — i.e. runtime Slang
shader compilation, one fresh session per program, no `VkPipelineCache`:

| Feature (ZM Release) | ms | | Feature | ms |
|---|---:|---|---|---:|
| Fog | 644 | | HDR | 289 |
| UnifiedMesh | 574 | | SSGI | 235 |
| Skybox | 365 | | TAA | 137 |
| Terrain | 364 | | Translucency | 121 |
| IBL | 362 | | …~20 more | < 120 each |
| SSR | 296 | | | |

**No single feature dominates** — the top six are within 2× of each other and sum
to ~2.6 s of the 4.4 s. So the per-program wrapper sweep named as a possible
follow-up in the plan is **not** warranted: this is broad, not concentrated, and
the fix is caching, not finer attribution.

### 3. Tool exports — dominated by one export, and it differs per game

| Export | ZM Debug | RT Debug |
|---|---:|---:|
| `Boot Tool Export/Test Assets` | 5,258 ms | 5,281 ms |
| `Boot Tool Export/Textures` | 1,804 ms | 7,502 ms |
| `Boot Tool Export/Meshes` | 305 ms | 379 ms |
| `Boot Tool Export/Font Atlas` | 276 ms | 323 ms |

`Test Assets` (the procedural StickFigure/Tree generators) is a fixed ~5.3 s in
both games. `Textures` is content-proportional — 4× larger for RenderTest. These
run **unconditionally** on every tools boot with no staleness check.

### 4. Cutoff → first present, including frame 1

`Zenith_Init` returning is not the end of the wait. Measured gap from the boot
cutoff to `FirstPresentSubmitted`:

| Config | cutoff → FirstPresentSubmitted | → FirstFrameCompleted |
|---|---:|---:|
| ZM Debug baseline | 355 ms | 584 ms |
| ZM Debug both-skipped | 232 ms | 460 ms |
| RT Debug baseline | 182 ms | 413 ms |
| RT Release baseline | 177 ms | 385 ms |

Frame 1 itself measured **486 ms wall clock** on a Zenithmon Debug boot, and its
per-zone table shows what is in it — `AI NavMesh Generate` **70 ms**, `Editor
Update` 88 ms, `Flux Swapchain End Frame` 130 ms. That confirms the premise the
plan was built on: **editor-automation step 1 executes inside frame 1, before
submission**, so the navmesh bake directly extends the time to first present.

### 5. The automation tail — the part that used to be invisible

431 steps, one per frame, after `Zenith_Init` returns. Total **1,143 ms**, and it
is not evenly spread:

| Step | ms | % of tail |
|---|---:|---:|
| step 295 (type 116) | 619 | 54% |
| **ZM NavMesh Bake (Dawnmere)** | 70 | 6% |
| next 8 steps | 13 each | ~1% each |

One unnamed step is **54% of the entire tail**. Naming it is a one-line change
(the named `AddStep_Custom` overload) and is the obvious next attribution step.

### 6. Capture overhead — 0.65%

| Pair | with capture | `--skip-boot-capture` | delta |
|---|---:|---:|---:|
| ZM Debug, units+exports skipped | 6,424.7 ms | 6,383.3 ms | **+41 ms (+0.65%)** |
| ZM Debug baseline | 116,749.5 ms | 115,597.7 ms | +1,152 ms (+1.0%, within run-to-run spread at this scale) |

Seal overhead (the raw-event sort) is **0.008–0.049 ms** across every artifact —
immeasurable. Across all runs: **0 truncated, 0 late, 0 unattributed, 0 ring
drops** — the capture is complete, not a sample.

### Cold vs warm

All numbers above are the **warm** steady state. The cold datapoint observed
during this work: the first `Null_` Zenithmon boot on a tree with cold terrain
bakes **exceeded the 600 s unit-gate watchdog**, versus ~180 s warm. Bake stamps
are `(version, file count)`, so a warm tree stays warm; a fresh clone pays this
once. This is consistent with the existing `zm-tests.yml` guidance to pass
explicit timeout headroom.

## Ranked fix recommendations

Ordered by measured ms recovered per unit of risk. **All of these are out of scope
for this change — they are reported, not implemented.**

1. **Gate `RunAllTests` at boot behind opt-in** — recovers **100.4 s** of a 114.6 s
   Zenithmon Debug boot (87.6%), 27.0 s of Release, 32.7 s of RenderTest Debug.
   Nothing else on this list is within an order of magnitude on the config people
   actually develop in. The flag already exists (`--skip-unit-tests`) and
   the test harness already passes it; the change is flipping the default for an
   interactive boot and keeping it on for the gate.
2. **Cache Slang compilation (or use the prebuilt `.spv` + a `VkPipelineCache`)** —
   recovers most of the **4.4 s** `LateInitialise` on *every* boot of *every* game
   and config, including ones with tests and exports already off. This is the
   floor everything else sits on, so it is the highest-value engine-side fix.
3. **Staleness-gate the tool exports** — recovers **7.6 s** (ZM Debug) / **13.5 s**
   (RT Debug). `Test Assets` (~5.3 s, fixed across both games) and `Textures`
   (1.8–7.5 s, content-proportional) are regenerated unconditionally every tools
   boot. Note this is a *larger* share than item 1 on the Release configs.
4. **Create the window hidden and show it at first present, or pump messages during
   init.** This does not make boot faster, but it is the *direct* cause of the
   white "not responding" ghost: `glfwPollEvents` is never called between window
   creation and the first main-loop iteration, which is 4.8 s at best and 114.6 s
   at worst. Cheapest visible-quality win on the list.
5. **Name the automation tail's dominant step** (step 295, 54% of the tail) via the
   named `AddStep_Custom` overload, then decide whether it can be deferred out of
   the tail. One line to attribute; the fix follows the answer.
6. **Stamp-guard the Zenithmon navmesh bake** — 70 ms inside frame 1, so it is
   small in absolute terms but sits on the critical path to first present.
7. **Async / incremental scene+asset loading with frames presented during load** —
   an actual loading screen. The largest architectural change and the last resort;
   items 1–3 remove more time for far less risk.

**Explicitly NOT recommended:** the per-shader-program attribution wrapper sweep.
Section 2 measured the compile cost as broad rather than concentrated (top six
features within 2× of each other), so finer attribution would not change the fix —
which is caching.
