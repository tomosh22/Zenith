# Photorealism program

The goal this document serves: **frames from RenderTest and Zenithmon that read as
photographs rather than as renders.** It records what was actually wrong, what
changed, and — most importantly — the two or three places where a plausible-looking
number was hiding a defect, because those are the ones that will be re-introduced.

Everything here is measured. Where a constant is tuned, the measurement that chose
it is written down beside it, in the file that holds it.

---

## 0. The instrument comes first

Nothing in this program can be judged from a description, so the first deliverable
was a repeatable way to LOOK at both games.

```
pwsh -NoProfile -File Tools\phototour_run.ps1 -Game RenderTest -Tag baseline
pwsh -NoProfile -File Tools\phototour_run.ps1 -Game Zenithmon  -Tag after -Compare baseline
```

* `RT_PhotoTour` (`Games/RenderTest/Tests/PhotoTour.cpp`) and `ZM_PhotoTour_Test`
  (`Games/Zenithmon/Tests/ZM_AutoTests_PhotoTour.cpp`) are **manual-only,
  graphics-required** automated tests. Each parks a probe camera at a fixed list of
  poses, lets TAA and auto-exposure settle, and dumps one swapchain TGA per pose.
* Every pose is **derived**, never absolute: Dawnmere's poses come from the
  placement headers and the LIVE terrain height, the interiors' from the room
  blockouts, RenderTest's portraits from the player transform. A layout change
  re-frames the shot instead of burying the camera in a hill.
* The editor's viewport rectangle is written beside each TGA as a `.rect` sidecar;
  `Tools/phototour_crop.py` cuts the scene out of the editor chrome and builds
  per-shot PNGs, a contact sheet, and side-by-side comparisons against another tag.
* The tours suppress UI quads, text, debug primitives and the viewport overlays, so
  what is captured is the WORLD.

`--window-size <W>x<H>` (new, `Zenith_CommandLine`) drives the capture resolution;
the tours run at 3840x2160.

> **★ TWO TOUR RUNS ARE NOT A CONTROLLED A/B.** The wind phase is driven by
> `GetTimeSeconds()`, which accumulates from process start, and the number of boot
> frames varies with what a boot has to bake — so the grass sways differently
> between runs. Measured on two runs with IDENTICAL settings, the noise floor is
> mean |diff| **0.011 to 0.032** on grass-heavy poses, which is as large as or
> larger than the effect of switching terrain shadows off entirely. A whole-frame
> change (exposure, colour) still reads through that; a localised one does not.
>
> For a controlled comparison use an **in-run A/B**, which captures the same pose
> twice a settle apart with only the feature flipped:
> `--phototour-terrain-shadows=ab` writes `<pose>.tga` and
> `<pose>__noterrainshadow.tga`; `--phototour-shadows=ab` does the same for the
> WHOLE shadow system and writes `<pose>__noshadow.tga`. Either takes `0` / `1`
> instead of `ab` to force one arm for a whole run. On the same poses that measured
> 0.9-1.3x the noise floor across two runs, the paired capture measures **2 to 7x**
> it.

> **★ AND AN A/B IS THE SECOND THING TO REACH FOR, NOT THE FIRST.** `--ds-debug=N`
> renders a single G-buffer or lighting term directly, needs **no rebuild**, and
> answers "why" rather than "did it change": `7` = G-buffer world normal, `8` =
> `NdotL`, `9` = sun shadow factor, plus `2` depth, `3` albedo, `5` roughness,
> `6` AO, `10` shading model, `11` IBL ambient (`Flux_DeferredShading.cpp`;
> the debug-variable twin is `Render/DeferredShading/DebugMode`). §1.10 is the
> cautionary tale: the terrain-shadow regression was one `--ds-debug=8` run away
> the whole time, and the A/B built first could only ever show that the ground did
> not change.

> **★ NEVER CHANGE RENDER STATE BELOW A `RequestDump` CALL.**
> `Zenith_AutomatedTestRunner::Tick()` runs BEFORE the frame's render, and
> `Flux_Screenshot`'s pending dump is consumed at EndFrame of the SAME frame — so a
> flip made after the request lands in the very frame being captured. The in-run A/B
> originally flipped shadows immediately after queuing the base shot, which put a
> shadows-OFF frame in the base file and a shadows-ON frame in `__noshadow`: the
> pair was **inverted**. Both tours now flip one frame later, in dedicated one-frame
> `AltFlip` / `AltRestore` phases.
>
> The mean-diff magnitude cannot reveal this — `|a-b|` is symmetric, so an inverted
> pair measures exactly as well as a correct one. Verify against FORCED arms
> instead: capture `--phototour-shadows=1` and `=0`, then check that the base shot
> matches the `=1` run. Measured after the fix, on the 10 poses that have any shadow
> content at all: base-vs-ON 1.2–10.6 against base-vs-OFF 5.0–25.0, every pose
> correct. (The 11th, `rt_tennis_spectator`, has a ground-truth shadow signal of
> 0.265 — below the wind noise floor — so it reads as a coin flip and means nothing.
> Check a pose's ON-vs-OFF signal before believing its verdict.)

> **A tour is judged by the captures it can prove, and by a PINNED exit signature.**
> A pre-existing teardown fault (the world-reset path records command-buffer scratch
> after the last per-frame reset, so it overflows a worker partition at *any*
> partition size — raising the buffer 1 MB -> 4 MB -> 16 MB just moved the number in
> the assert) fires AFTER the test reports PASSED and every screenshot is on disk.
> `phototour_run.ps1` forgives that ONE fault, and only when its signature
> (`Worker N scratch buffer overflow`) is in the log; any other nonzero exit is
> reported with the log tail. It also warns on any `still held with N refs` line
> from the asset registry, and the crop step fails when it finds no readable
> captures. Fixing the teardown fault is worth doing; it is not a rendering bug and
> it never affected a capture.
>
> ★ **It used to forgive EVERY nonzero exit once `DONE` appeared**, which is a
> fail-open of the kind `Tools/tick_gates.ps1` exists to prevent: `DONE` is logged
> before engine shutdown, so every teardown regression after it was invisible. It
> hid a real one — grass texture handles outliving the asset registry, because
> `Flux_GrassImpl::ReleaseAssetReferences()` was never wired into
> `Flux_RendererImpl::ReleaseAssetReferences()`. The log had been printing
> "still held with 4 refs" for all three grass textures the whole time.
>
> ★ **A tag directory holds ONE run.** A reused `-Tag` is cleared first: the
> cropper enumerates every `.tga` in the directory, so a stale capture is presented
> as part of the new run AND satisfies the disk check below — a run that captured
> nothing could verify clean against its predecessor's output. A `-Compare` that
> does not pair every shot on both sides now fails naming the unmatched ones
> (`-AllowPartialCompare` for a deliberate asymmetry); zero pairs used to exit 0
> having compared nothing.
>
> ★ **And a queued screenshot is not a written file.** `RequestDump` only sets a
> pending flag, so the shot counter proves the tour ASKED for N captures. Both tours
> now `file_size` every requested path in `Verify` and name any that are missing or
> empty.

---

## 1. The defects that mattered most

Ranked by how much of the frame they were wrong about. Each was found by an audit
of the actual code, then confirmed on pixels.

### 1.1 The sky and the sun were quantised to 8 bits

`MRT_FORMAT_DIFFUSE` is `RGBA8`, and every sky pass wrote its radiance there. The
sun disc was generated at `intensity * 5` = 35 and clamped to 1.0; the sky's whole
HDR range collapsed. Nothing in the sky could bloom, glare, or roll off, and the
auto-exposure was metering a sky that could never exceed 1.0 — so the histogram
derivation in `Flux_HDRImpl.h`, which reasoned from "the sky is bounded by the
anchor 7", was describing a quantity that never reached the buffer.

**Fixed** by routing sky radiance through the **emissive** MRT (already `RGBA16F`)
and returning it from the deferred pass for `depth == 1` pixels. The 8-bit diffuse
copy is kept as a clamped preview for the G-buffer debug views.

### 1.2 The sun was a zero-solid-angle delta light with no disc radiance

Two halves of the same error:

* `RenderSunDisk` returned the **irradiance** anchor as a radiance, making the
  visible disc ~4 orders of magnitude too dim relative to the sky. It now divides
  by the solar solid angle (`E / (pi * sin^2 theta)`), so the disc is ~2.5e4
  against a sky of 1-7 — and its edge is anti-aliased over one pixel of angular
  width instead of a hard `if`.
* `CookTorrance_Directional` treated the sun as a delta direction, so on any smooth
  surface the highlight collapsed to a sub-pixel spike of magnitude ~1e6. It now
  widens the GGX lobe by the disc's angular radius and renormalises by
  `(alpha/alpha')^2` (Karis sphere-light), which is what turns that spike into the
  soft solar highlight a camera records — and stops it flickering through TAA.

The solar angular radius had **three different values** in the tree (0.013 in a
debug var, 0.018 in the shadow struct, 0.00935 in the atmosphere). It is now the
physical 0.00935 everywhere.

### 1.3 SSAO was over-estimating occlusion by up to 4x on every silhouette

`Flux_SSAO.slang` skipped sky and off-screen taps **and removed them from the
denominator**. A pixel on a shoulder against the sky with 8 geometry taps and 24
sky taps divided its occlusion by 8 instead of 32. A sky tap is an *unoccluded*
direction, not a missing one: the fix is one line (count the tap, then `continue`
past the accumulation), and it lifted the ambient across every silhouette and the
whole frame border, which had carried a dark AO vignette.

### 1.4 Auto-exposure could not see the highlights it was clipping

Three compounding problems, and the third only became visible once the first two
were fixed:

1. **Dark pixels were dropped from the histogram** (`luminance >= 0.001`), so a
   shadowed frame was metered by its bright pixels alone.
2. **No metering weighting** — a sky-heavy frame underexposed the ground. Now
   centre-weighted 3:2:1, which is a camera's default pattern.
3. **The histogram domain topped out at luminance 7.8.** Once 1.1 and 1.2 landed,
   the sun disc, the sky and every sunlit white surface shared the top bin, so
   *no percentile above ~0.9 could distinguish them*. The domain is now
   -10 -> +16 (top 65536), which covers the disc.

On top of that, average-only metering has no idea how far the brightest real
content sits above the average. **Highlight protection** now derives a second
exposure that places the near-maximum (99th percentile) at a level the tone curve
can hold, and takes the smaller of the two — so it can only ever protect
highlights, never brighten. The headroom (6.0) was chosen by evaluating the shipped
AgX curve: scene 6.0 -> 0.905 display, 16.0 -> 0.995.

> **Probe the near-maximum, not a mid percentile.** The first attempt protected the
> 95th percentile and fixed the clipping — by dragging the entire mid-tone range
> down with it (frame mean 0.375 -> 0.348). The surface that clips is by definition
> in the top few per cent; anything lower trades the whole image for it.

Measured on the RenderTest campus overview: blown pixels 0.06% (before) -> 4.58%
(after the ambient and sky fixes, correctly exposed but clipping) -> **0.07%**
(with highlight protection), at a *higher* frame mean than the over-protected
attempt.

### 1.5 Bloom was thresholded, unexposed, and unbounded

`dbg_fHDRBloomThreshold = 3.0` was an absolute, pre-exposure luminance — so what
counted as "bright" drifted with the exposure — and bloom was added AFTER exposure,
making its relative strength vary inversely with it. Bloom is scene-referred
veiling glare: it is light, so it is exposed like light. It is now thresholdless
(`0.0`) and scaled by exposure at composite.

That change alone made the frame worse, which is the interesting part: with a
physically-scaled sun disc, an unclamped thresholdless bloom spreads the disc's
energy over every mip and veils the whole image to white. **The bloom SOURCE is now
clamped** (24.0) — bounding a radiometric outlier's contribution, not the scene.

### 1.6 Terrain cast no shadows at all

`Flux_TerrainImpl::RenderToShadowMap` was `STUBBED` and its call site commented out.
Mountains cast nothing into valleys, cliffs never shaded their own feet, and a low
sun did nothing to the landscape. Terrain now culls per cascade (one slot per
cascade in a second indirect buffer, filled by the same compute dispatch that culls
the camera view) and draws indirect into every cascade. `Terrain` moved before
`Shadows` in `RegisterDefaultFeatures` so the graph edges exist;
`ValidateProducerBeforeConsumer` stays at zero. **No render-graph passes were
added.**

It is togglable at runtime like every other caster: **`Render/Shadows/Terrain
Casts Shadows`**. The switch resolves to ZERO active cascade slots rather than
just skipping the draw, so it removes the per-cascade cull work too; an early-out
in the draw alone would still pay four cascades of chunk culling every frame.
That decision is the pure `Flux_TerrainShadowActiveCascades`, so the toggle's
effect is pinned by a unit test rather than living as an inline `&&`. Terrain was
the last caster without a switch — the only way to remove its shadow used to be
disabling the terrain itself, which removes the receiver too and so cannot
isolate what the casting contributes.

### 1.7 Every albedo was ~2.2x too bright

No BC-compressed colour texture was ever sRGB-decoded: the exporter emitted only
UNORM BC formats, and `ZM_SynthBakeAlbedoBC1` baked the OETF into the bytes with a
comment claiming "BC1 has no sRGB variant" — which is false. The four BC sRGB
formats now exist end to end, the exporter carries an explicit
`TextureColourSpace`, and the mip chain filters sRGB content in linear.

Two smaller defects fell out of the same audit: `stb_compress_dxt_block` was being
asked for punch-through alpha with `bHasAlpha ? 0 : 0` (it never did punch-through
at all — a real 3-colour-mode encoder was written), and **every procedural asset
texture shipped with ONE mip** because the runtime mip-generation path had been
deleted. Rocks, logs, bark, leaves, bushes and the terrain's packed
roughness/metallic all sparkled at distance for want of a mip chain.

`mipLodBias` is now -0.5 on the anisotropic samplers: it was zeroed years ago
*because there was no TAA*, and the comment still said "reintroduce once TAA
lands". TAA has been the shipping default AA since.

### 1.8 Things that simply did not exist

* **Aerial perspective.** Grep found zero hits, in any spelling. The default fog
  was a constant-colour exponential at falloff 7.5e-5 — 7% fog at the far plane, in
  a colour unrelated to the sky. `Flux_Fog.slang` is now a real aerial-perspective
  integration of the same Rayleigh + Mie medium the sky is rendered with, with
  altitude-aware optical depths, so distant terrain loses contrast and shifts blue
  exactly as the sky above it does.
* **Clouds.** A cloudless sky is the most reliably synthetic thing in an outdoor
  frame. There is now a domain-warped fBm cloud layer in the sky pass, lit with
  Beer-Lambert + powder toward the sun and occluding the sun disc.
* **Lens imperfections.** Vignetting, chromatic aberration and luminance-weighted
  grain. A perfectly clean, perfectly rectilinear image is a tell.
* **Micro-shadowing** (material AO occluding direct light at grazing angles),
  **specular horizon occlusion** (a normal-mapped reflection vector that dips below
  the surface cannot see the sky), and **a distance fade on the sun shadow** (it
  used to stop dead at 300 m).

### 1.9 Grass glowed in the dark

Grass wrote its sun transmission into the **emissive** channel, which the deferred
pass adds after the shadow blend and explicitly "bypasses lighting and shadows
entirely". A blade standing in a tree's shadow glowed at full strength. Subsurface
receivers now resolve the sun shadow even when sun-averted (they transmit), and
their emissive is attenuated by it.

The subsurface tint was also hard-coded to skin (`0.80, 0.32, 0.24`) and applied to
grass, which painted every backlit leaf pink. Scattered light now takes the
surface's own hue, and a view-dependent transmission lobe was added.

---

### 1.10 The terrain stopped receiving ANY shadow, and I introduced it

The most expensive defect of the program, and the one that survived longest,
because every instrument I reached for first was the wrong one.

**The symptom.** Dawnmere read as shadowless: no house shadow on the plaza, no
tree or boulder shadow on the grass, and a plaza roughly half as bright as it
had been. The user reported it from memory of the pre-work build, against my
own written conclusion that the absence was pre-existing.

**The cause was a guess of mine.** §1.7's export pass added
`Zenith_Tools_TextureExport::HintFromFilename`, which routed any texture whose
basename contained `normal` to **BC5** — two channels, R+G, with Z reconstructed
at sample time. That is the engine's standing contract, spelled out in
`Common/Material.slang`'s `SampleNormalMap` and honoured by every mesh path.
**The two terrain G-buffer shaders never honoured it.** They still read

```slang
float3 xNormalMap0 = GetBindlessTexture(...).Sample(xUV0).xyz * 2.0 - 1.0;
```

BC5 returns blue = 0, so `.z` decoded to **-1** and the blended tangent-space
normal pointed straight INTO the surface. `NdotL` went to zero, and the
deferred pass's `if (bViewShadows && (fNdotL > 0.0 || bTransmits))` therefore
never ran the CSM branch at all: the shadow factor stayed 1.0 across every
terrain pixel in the game. The terrain also lost its entire direct sun term,
which is the halved plaza brightness. Fixed by reconstructing Z in both
`Flux_Terrain_ToGBuffer.slang` and `Flux_Terrain_ToGBufferVelocity.slang`
(measured: plaza `NdotL` 0.9/255 -> 191/255).

**Why it hid so well.** The terrain was the ONLY receiver affected, and nothing
else about the frame looked broken:

* Meshes, foliage and grass all shadowed correctly, so "shadows work" was true
  everywhere you looked for it.
* The buildings were still perfectly good CASTERS — the chimney's shadow lands
  on the roof, every window frame and eave has its contact shadow — so the
  caster half of the system answered "yes" to every test.
* The G-buffer normal is not something a frame shows you directly. The terrain
  still had normal-mapped surface detail from IBL, so it did not read as flat.

**Three method errors, each of which cost real time.**

1. **I answered "is it still doing X" without re-testing the baseline.** I
   checked one crop of one pose and concluded the absence was pre-existing. The
   pre-work capture of `dawnmere_home_approach` shows an unmistakable lamp-post
   shadow across the plaza. One crop is not a baseline.
2. **I reasoned about sun geometry from the sun disc in frame.** It reads far
   higher than it is under any camera pitch, and I twice talked myself out of a
   real finding with "the shadow must be hidden behind its caster".
3. **I reached for the A/B before the debug views.** `--ds-debug=9` (shadow
   factor), `=8` (NdotL) and `=7` (G-buffer normal) already existed and needed
   no rebuild. Mode 8 answers this in one run: the plaza reads 0.9/255. The
   whole-shadow-system A/B I built instead was sound but could only show that
   the ground did not change — never why.

**The guess is gone, not patched.** Fixing the decode would have left a filename
heuristic deciding how every texture in the engine is encoded, which is a defect
waiting for its second victim. `HintFromFilename` and its five unit tests are
deleted, and the answer is now DECLARED:

* `TextureUsage` — `BASE_COLOUR` / `BASE_COLOUR_MASKED` / `NORMAL_MAP` /
  `LINEAR_DATA` / `UNCOMPRESSED_COLOUR` / `UNCOMPRESSED_DATA` — is the vocabulary,
  and `ResolveUsage` is the single mapping from a usage to (compression, colour
  space). There is no second place a usage becomes a format.
* Each walked asset root carries a committed `TextureUsage.ztexdecl`: one
  `<path> <USAGE>` line per source texture, `#` comments, matched
  case-insensitively and separator-insensitively. `.ztexdecl` is re-included in
  `.gitignore` beside `.zscen` and `.znavmesh`, because a declaration a fresh
  clone lacks is a declaration somebody re-guesses.
* **An undeclared texture is not exported**, and the boot names it and the line
  to add. A malformed line, an unknown usage token or a duplicate path fails the
  whole manifest rather than being skipped; a declared path with no file is
  reported as a stale entry. There is no tolerant mode, because every tolerance
  here is a silent wrong export.

The five deleted tests all PASSED, on every case they covered, while the code
they pinned broke the game. That is the lesson worth keeping: they pinned what
the guess was asked to do, and never that its consumer agreed. The replacements
pin the usage→format mapping, the token round-trip, and that an unlisted path
comes back "no" — including for a name the old heuristic would have been
confident about.

**And the decode now has ONE site.** `Common/Material.slang` grew
`UnpackNormalMapTS`; `SampleNormalMap` and both terrain shaders call it. The
terrain cannot use `SampleNormalMap` directly (it blends four layer normals
before applying the TBN), but the part that has to agree with the exporter is
shared.

Verified end to end: a cold re-export reports `35 exported, 0 undeclared, 35
declarations`, and every `.ztxtr` on disk matches its declared usage — the three
terrain `normal.jpg` sets are `BC5_RG_UNORM`, the `diffuse` sets are
`BC1_RGB_SRGB`, and the data maps are `BC1_RGB_UNORM`.

## 2. What is calibrated, and against what

| Constant | Value | Chosen by |
|---|---|---|
| `fHIGHLIGHT_HEADROOM` (`Flux_Adaptation.slang`) | 6.0 | Evaluating the shipped AgX curve: 6.0 -> 0.905 display, 16.0 -> 0.995 |
| `fHIGHLIGHT_PERCENTILE` | 0.99 | The sun disc, speculars and emissives are meant to clip; ordinary surfaces are not |
| `fHDR_HISTOGRAM_LOG_LUMINANCE_RANGE` | 26.0 | Must cover the sun disc radiance `E / (pi sin^2 theta)`, pinned by a unit test |
| `fBLOOM_SOURCE_CLAMP` (`Flux_BloomThreshold.slang`) | 24.0 | Bounds the disc's contribution without bounding the scene |
| `CLOUD_COVERAGE` (`Flux_Atmosphere.slang`) | 0.62 | The fBm's **measured** distribution (mean 0.47, p90 0.63, max 0.82), not the nominal [0,1] |
| `CLOUD_SCALE` | 0.0013 | At 0.00028 the base octave's wavelength was ~3.5 km, so two or three cells spanned the whole sky and it read as haze |
| Default sun direction | `(-0.55, -0.50, -0.67)` | ~30 deg elevation. The old ~46 deg is the flattest, least photographic angle there is |
| Default Mie scale | 4.0 | A real horizon is pale and the sun wears an aureole; 1.0 read as a vacuum-clean navy sky |
| Terrain roughness floor | `lerp(0.55, 1.0, r)` | The photo sets dip to ~0.3, and under a low sun those patches glint like wet glass across a hillside |

> **★ CALIBRATE AGAINST THE MEASURED DISTRIBUTION, NOT THE NOMINAL RANGE.** The
> cloud layer was invisible on its first two runs. The coverage cut was written
> against a nominal fBm range of [0,1]; the actual distribution has mean 0.47 and
> max 0.82, so a threshold of 0.54 left 99% of the sky under alpha 0.05. Replicating
> the noise function in Python and printing the histogram took one command and
> replaced two blind iterations.

---

## 3. How to verify a change

1. `pwsh -NoProfile -File Tools\zenith.ps1 build <Game> --config Vulkan_vs2022_Release_Win64_True`
   — shader-only edits need no rebuild (tools builds compile `.slang` at boot), but
   anything in a header does.
2. Run the tour with a new tag and `-Compare` against the previous one.
3. **Measure, do not squint.** Per-shot frame mean, p99 and blown-pixel fraction
   separate "darker" from "clipping" in one line, and region means separate "the
   rock is black" from "the rock is fine and the thumbnail is contrasty" — which is
   exactly the mistake this program made once and caught with a region probe.
4. A `Null_` unit run for the pins; a Vulkan exe reports a different count. Bump
   **every** game's row in `Tools/unit_baselines.json`, each OBSERVED — an engine
   or `Tools/` unit moves Zenithmon, Combat and RenderTest alike, because they all
   boot the same suite.
5. **When something is missing rather than merely different, isolate the TERM
   before comparing frames.** `--ds-debug=N` (§0) renders one term with no rebuild;
   an in-run `--phototour-shadows=ab` / `--phototour-terrain-shadows=ab` pair
   attributes a difference to one feature. Frame-to-frame diffs answer "did it
   change", never "why".
6. **"Is it STILL doing X" is a question about the baseline — go and re-measure the
   baseline.** §1.10 was called pre-existing off one crop of one pose; the pre-work
   capture of the same pose has an unmistakable lamp-post shadow across the plaza.
   One crop is not a baseline, and a shadow tucked behind its own caster looks
   exactly like no shadow.
7. **Never read the sun's elevation off the disc in frame.** Under camera pitch it
   reads far higher than it is. `Zenith_GetDefaultSunDirection()` is the number;
   Dawnmere authors no `Zenith_SunComponent`, so it takes that default, while both
   Zenithmon interiors set their own.

---

## 4. Known gaps

* **The teardown scratch overflow** (see §0). Cosmetic, but it makes every windowed
  run exit non-zero.
* **SSGI remains off by default**, and correctly so: it *replaces* the sky
  irradiance with a single screen-space albedo bounce rather than adding indirect
  on top of it, so turning it on makes shadowed geometry darker and flatter. It
  should become occlusion + colour bleed before it ships on.
* **Ozone is absent** from the atmosphere (`TODO(ozone)` carries a full plan), which
  is why twilight lacks the Chappuis band.
* **No local light shadows.** Every interior lamp leaks through walls. This is the
  largest remaining structural gap for interiors.
* **No depth of field, no motion blur** — the velocity MRT already exists and is
  populated, so motion blur is nearly free whenever it is wanted.
* **The IBL cube is a single distant probe with no local occlusion**, so an interior
  receives full open-sky irradiance modulated only by a 0.5 m-radius SSAO.

* **Nothing pairs a texture's exported compression with the decode its shader
  assumes.** §1.10 was exactly that gap. It is narrowed rather than closed: the
  decode now has one site (`UnpackNormalMapTS`) and the encode is declared per
  texture, so the two can only disagree if somebody open-codes a sample again.
  Nothing mechanically stops that — a FluxCompiler lint that rejects a raw
  `Sample(...).xyz * 2.0 - 1.0` outside `Common/Material.slang` would, in the
  same shape as the existing spine lint.

