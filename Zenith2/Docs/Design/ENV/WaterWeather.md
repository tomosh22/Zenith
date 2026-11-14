# ENV design: water and weather

Section file of `Zenith2/Docs/Design/ENV.md`. It specifies water (bodies, wave synthesis, the deterministic CPU height query gameplay uses, surface rendering, underwater rendering, the advanced Stage 5 tiers and the documented error budget that validates the approximation, per `Zenith2/Docs/Rendering.md → "Validation roles"`), and weather (the weather document, `Zenith_WeatherSystem`, the state machine as a table, deterministic transitions, and the effects weather has on materials, particles, audio and gameplay).

## Scope of this file

| Concern | Heading |
|---|---|
| water body kinds and the water document | "Water model" |
| runtime API | "Zenith_WaterSystem header" |
| Gerstner waves, validation, time base, the height query | "Wave synthesis" |
| surface nodes, the GPU side, `Material_Water`, optics, reflections, shorelines | "Water rendering" |
| submersion and the underwater composite | "Underwater" |
| FFT ocean, rivers, caustics | "Advanced water" |
| the error budget | "Water error budget" |
| the weather document and parameters | "Weather model" |
| runtime API | "Zenith_WeatherSystem header" |
| the table, draws, blending | "Weather state machine" |
| materials, particles, audio, gameplay | "Weather effects" |
| Stage 5 weather tiers | "Weather tiers" |

## Water model

### Water body kinds

| Kind | Shape | Stage | Waves | Notes |
|---|---|---|---|---|
| `WATER_BODY_KIND_OCEAN` | an infinite plane at a height, optionally bounded by a rectangle | S4 | Gerstner set (S4); FFT spectrum tier (S5, desktop) | at most one per world |
| `WATER_BODY_KIND_LAKE` | a closed XZ polygon (at most 256 vertices, simple, counter-clockwise, its bounds at most 4,096 m per axis, so the 10-level quadtree with 10-bit node indices covers it; a larger polygon is `ENV_ERROR_INVALID_DESC` at activation) at a height | S4 | Gerstner set (small amplitudes) | many per world |
| `WATER_BODY_KIND_RIVER` | a spline with per-point width and height | S5 | flow-mapped normal scrolling, no displacement | many per world |

A water body is an entity with `Zenith_WaterBodyComponent` (in a scene or a streamed cell): its kind, its surface height (or the river spline asset), its shape, and a `Zenith_AssetHandle` of kind `Environment.Water` naming its water document.

### Water document

`*.water.json`, an authoring document:

```json
{
	"schemaVersion": 1,
	"id": "a17c…",
	"waves": [
		{ "wavelength": 60.0, "amplitude": 0.45, "directionDegrees": 20.0, "steepness": 0.35, "phase": 0.0 },
		{ "wavelength": 23.0, "amplitude": 0.18, "directionDegrees": 55.0, "steepness": 0.30, "phase": 1.3 }
	],
	"windDriven": true,
	"absorption": [ 0.45, 0.09, 0.06 ],
	"scattering": [ 0.002, 0.004, 0.005 ],
	"indexOfRefraction": 1.333,
	"foam": { "shoreDepth": 0.6, "crestThreshold": 0.55, "texture": "…guid" },
	"normalDetail": { "texture": "…guid", "scale": 0.08, "speed": 0.4 },
	"underwaterFogDensityScale": 1.0,
	"fft": null
}
```

| Key | Rule |
|---|---|
| `waves` | 0 … 8 Gerstner components ("Gerstner sum"); wavelength 0.5 … 1,000 m, amplitude ≥ 0 |
| `windDriven` | true: amplitudes scale with the weather's wind speed by `min(2, (speed / 8 m/s)²)`, applied in phase 1 so the scale is part of the deterministic state |
| `absorption`, `scattering` | per metre, linear Rec.709 RGB; the Beer–Lambert medium of "Optical model" and of the underwater medium |
| `indexOfRefraction` | 1.0 … 2.0 |
| `foam`, `normalDetail` | presentation parameters; textures are GEOM textures of classes MASK and NORMAL |
| `fft` | Stage 5: `{ "spectrum": "jonswap", "windSpeed", "fetch", "resolution": 128 · 256, "patchSize" }`, used by the FFT tier on desktop tiers; `null` keeps the Gerstner set on every tier |

## Zenith_WaterSystem header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_WaterSystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

/// @contract value Zenith_GerstnerWave
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: an element of Zenith_WaveSet::m_axWaves, which the document reader
///   fills in place.
/// Invalidation: None.
/// Units: Metres; radians; m_fSteepness is the dimensionless Q of the Gerstner formula.
/// Errors: Validated by Zenith_ValidateWaveSet.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_GerstnerWave
{
	float m_fWavelength = 10.0f;
	float m_fAmplitude = 0.0f;
	float m_fDirectionX = 1.0f;          // unit XZ direction of travel
	float m_fDirectionZ = 0.0f;
	float m_fSteepness = 0.0f;
	float m_fPhase = 0.0f;
};

/// @contract value Zenith_WaveSet
/// Ownership: Plain value of at most uENV_MAX_GERSTNER_WAVES (8) waves.
/// Thread affinity: IMMUTABLE. Members non-const: a member of documents and runtime state that are filled field by
///   field.
/// Invalidation: None.
/// Units: As Zenith_GerstnerWave.
/// Errors: Validated by Zenith_ValidateWaveSet.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WaveSet
{
	Zenith_GerstnerWave m_axWaves[uENV_MAX_GERSTNER_WAVES];
	uint32 m_uWaveCount = 0;
	float m_fAmplitudeScale = 1.0f;      // the wind-driven scale of the current tick
};

/// @contract value Zenith_WaterSample
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Metres; m/s; m_xNormal unit.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WaterSample
{
	float m_fSurfaceHeight = 0.0f;       // the displaced surface height above the queried XZ
	Zenith_Vector3 m_xNormal;
	Zenith_Vector3 m_xVelocity;          // the surface particle's velocity (buoyancy drag, floating debris)
	float m_fDepth = 0.0f;               // surface height minus terrain height, when the terrain is resident; else +inf
	uint32 m_uBodyIndex = 0;
};

/// @contract type Zenith_WaterWorldResource
/// Ownership: One per world, owned by the Environment module instance and registered as the world resource
///   "Zenith.Environment.WaterWorld" with owning phase PHASE_INPUT.
/// Thread affinity: PHASE(Input) for writes (Environment.UpdateWaterBodies alone); reads from any system declaring
///   ReadResource, in any phase.
/// Invalidation: The body set and each body's wave amplitude scale change only at phase 1.
/// Units: Metres; seconds of simulation time (tick × tick duration).
/// Errors: Sample returns ENV_ERROR_OUT_OF_BOUNDS where no body covers the point; never asserts.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WaterWorldResource
{
public:
	// The water surface over (fX, fZ) at the resource's current tick; deterministic per build.
	Zenith_Result<Zenith_WaterSample, Zenith_EnvironmentError> Sample(float fX, float fZ) const;
	bool IsSubmerged(const Zenith_Vector3& xPoint) const;
	uint32 GetBodyCount() const;
	uint64 GetTick() const;
};

/// @contract functions Zenith_WaterFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres; seconds.
/// Errors: Zenith_ValidateWaveSet returns ENV_ERROR_WAVESET_INVALID with the wave index; the evaluation functions assume a
///   validated set (ZENITH_ASSERT).
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_EnvironmentError> Zenith_ValidateWaveSet(const Zenith_WaveSet& xWaves);
// The displaced position of the undisplaced surface point (fX0, fZ0) at time dTime.
Zenith_Vector3 Zenith_EvaluateGerstner(const Zenith_WaveSet& xWaves, float fBaseHeight, float fX0, float fZ0,
	double dTimeSeconds);
// The surface height above (fX, fZ): "Height query by fixed-point iteration".
Zenith_WaterSample Zenith_SampleGerstnerSurface(const Zenith_WaveSet& xWaves, float fBaseHeight, float fX,
	float fZ, double dTimeSeconds);
```

### Fluid query provider

PHYS's buoyancy reads the water surface through `Zenith_FluidQuery` (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_FluidQuery"`, cross-area row PH8), an interface in ZenithScene that SCENE declares, because neither module may depend on the other ([Z2-N-ARCH-850]; ZenithEnvironment and ZenithPhysics are both layer-7 modules with no edge between them, and ZenithScene is a declared dependency of both). ENV implements it per world:

```cpp
// Zenith2/Engine/ZenithEnvironment/Internal/Water/Zenith_EnvironmentFluidQuery.h
#pragma once
#include "ZenithScene/Zenith_FluidQuery.h"

class Zenith_WaterWorldResource;

constexpr float fENV_FLUID_DENSITY_KG_PER_M3 = 1000.0f;   // ENV's water has no per-body density

/// @contract type Zenith_EnvironmentFluidQuery
/// Ownership: One per world, owned by the module's per-world state beside the Zenith_WaterWorldResource it borrows;
///   added by AttachWorld 1 as the world resource "Zenith.FluidQuery" (owning phase PHASE_INPUT).
/// Thread affinity: As Zenith_FluidQuery: ANY_THREAD for readers in phases 2 to 12 that declare ReadResource of it.
/// Invalidation: None while the world lives.
/// Units: Metres; m/s; kg/m³.
/// Errors: Sample returns false where Zenith_WaterWorldResource::Sample returns ENV_ERROR_OUT_OF_BOUNDS; never asserts.
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_EnvironmentFluidQuery final : public Zenith_FluidQuery
{
public:
	explicit Zenith_EnvironmentFluidQuery(const Zenith_WaterWorldResource& xWater);
	bool Sample(float fX, float fZ, Zenith_FluidSample& xSampleOut) const override;

private:
	const Zenith_WaterWorldResource* m_pxWater = nullptr;
};
```

`Sample` calls `Zenith_WaterWorldResource::Sample(fX, fZ)`. On success it writes `m_fSurfaceHeight`, `m_xSurfaceNormal` (the sample's `m_xNormal`), `m_xFlowVelocity` (its `m_xVelocity`) and `m_fDensity` = `fENV_FLUID_DENSITY_KG_PER_M3`, and returns true; on `ENV_ERROR_OUT_OF_BOUNDS` it returns false and leaves `xSampleOut` unchanged. The answer is the water resource's at its current tick, so it is bit-identical to a direct query on every worker. `Environment.UpdateWaterBodies` declares `WriteResource` of `Zenith.FluidQuery` beside `Zenith.Environment.WaterWorld`, so a reader declaring `ReadResource(Zenith_FluidQuery::xTYPE_ID)` is ordered after the phase-1 update by declared access, and a second writer is a schedule error. A world without the Environment module has no `Zenith.FluidQuery` resource.

## Wave synthesis

### Gerstner sum

For an undisplaced surface point `x0 = (x0, z0)` at base height `H` and time `t` (seconds), each wave `i` with wavelength `L_i`, amplitude `A_i` (times the set's amplitude scale), direction `D_i`, steepness `Q_i` and phase `φ_i` has wave number `k_i = 2π / L_i`, angular frequency `ω_i = sqrt(g k_i)` with `g = 9.81 m/s²` (deep-water dispersion), and argument `θ_i = k_i (D_i · x0) − ω_i t + φ_i`. The displaced point is:

`P = (x0 + Σ Q_i A_i D_i.x cos θ_i,  H + Σ A_i sin θ_i,  z0 + Σ Q_i A_i D_i.z cos θ_i)`

The normal is the normalised `(−Σ D_i.x k_i A_i cos θ_i, 1 − Σ Q_i k_i A_i sin θ_i, −Σ D_i.z k_i A_i cos θ_i)`; the surface particle velocity is the time derivative of `P`. The sum runs in wave-index order in `float`, with `θ_i` reduced modulo 2π in `double` on the CPU and computed from the reduced time on the GPU ("Time base").

### Wave set validation

`Zenith_ValidateWaveSet` (and the cook) refuse: more than 8 waves; a non-finite value; a wavelength outside [0.5, 1,000] m; a direction not unit within 1e-4; and `Σ Q_i k_i A_i > 1` (at the maximum amplitude scale 2), the condition under which the Gerstner surface folds over itself.

### Time base

Water time is simulation time, `tick × tickSeconds`, the same on every worker. The CPU reduces `ω_i t` modulo 2π in `double` per wave; the GPU receives, per wave, the phase offset `(φ_i − ω_i t) mod 2π` computed on the CPU in `double` for the frame's interpolated time and written as `float`, so neither side evaluates `sin` of a large argument.

### Height query by fixed-point iteration

A Gerstner surface moves points horizontally, so the height above `(x, z)` is found by solving `x0 + d(x0) = x` for the undisplaced point: start with `x0 = (x, z)`, iterate `x0 ← (x, z) − d(x0)` four times (`d` the horizontal displacement), then evaluate `P(x0)`. With `Σ Q_i k_i A_i ≤ 1` the iteration contracts; four steps bring the horizontal residual below 1 mm for the validated sets (`Test_Water_HeightQuery_ResidualBelow1mm`). The query uses a fixed four iterations (never a convergence test), so its cost and its result are deterministic.

## Water rendering

### Water node selection

Water surfaces are drawn as GPU-scene instances of one engine grid mesh (`WaterGrid`, "Water GPU side") selected per body on `Flux.BuildGraph` by `Flux_WaterRenderer` with the quadtree rule of `Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection"`: leaf node 8 m, LOD distance ratio r = 8 (so `D_l = 8 m × 2^l × 8`), at most 10 levels (root 4,096 m), morph over the last 30% of each level's distance, distances horizontal from the LOD reference view's position, nodes clipped to the body's shape (a lake's node wholly outside its polygon is not emitted).

- Crack-freeness: with r ≥ 8 the three facts of GEOM's argument (`Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement"`) hold unchanged, because they use only node selection and the grid morph: adjacent nodes differ by at most one level, and on a shared edge the finer node is fully morphed (k = 1) and the coarser not at all (k = 0), so both compute the same undisplaced XZ with the same kernel operations. A water vertex's displacement is the Gerstner sum at its undisplaced XZ ("Gerstner sum"), a function of that XZ alone, so shared edge vertices displace to bit-identical points. r = 2, which revision 0 stated, breaks the one-level fact (a node's parent may reach `D_l (1 + 2√2 / r) = 2.41 D_l > 2 D_l`) and is withdrawn.
- A lake's quadtree origin is its polygon's bounds minimum snapped down to 8 m, with the fewest levels whose root covers the bounds. The ocean's origin is `O = (floor(p / 4,096 m + 0.5) − 1) × 4,096 m` per axis for the reference position p: the 4,096 m grid line nearest p, less one root. So the 2 × 2 roots of 4,096 m form the root square `[O, O + 8,192 m)²`, whose centre `O + 4,096 m` lies within 2,048 m of p on each axis, and at least 2,048 m of node-covered ocean lies on every side of the camera. When O moves (p crosses a 4,096 m half-line) every ocean node's key changes and the node set is replaced in one frame.
- The ocean's Gerstner displacement, horizontal and vertical, is multiplied by `saturate(e / 256 m)`, e the undisplaced vertex's distance to the root square's boundary, so the square's edge vertices are undisplaced and lie exactly on the skirt's inner edge. The factor is a function of the undisplaced XZ alone, so the crack-free argument above holds unchanged. It applies to rendering only: gameplay's `Sample` keeps the full wave set everywhere, and the fade zone lies at least 1,792 m from the camera.
- A node that stays selected keeps its instance, so the per-frame GPU-scene change is the nodes that entered or left. Outside the root square the skirt's eight instances ("Water GPU side") draw the ocean at its base height; every XZ point within 129,024 m of the camera is drawn by exactly one node or skirt quad (`Test_Water_OceanCoverageNoGap`).
- The skirt's inner edges are single 8,192 m edges, while the boundary nodes (levels 5 to 7 at that distance) put a vertex every 8 to 32 m on them: these T-junctions are accepted, not subdivided away. Every T-vertex lies exactly on the skirt edge (both are undisplaced at the boundary, at the same base height, on 8 m multiples from O, which binary32 represents exactly up to 2²⁴ m), the boundary is at least 2,048 m from the camera, where a rasterisation crack is at most a sub-pixel gap along one line of a transparent surface under aerial perspective; `Test_EnvRender_WaterSurfaceCrossBackend` bounds it with its boundary case (at most 0.01% of the pixels along the projected boundary).

### Material_Water

`Material_Water` (`Zenith2/Shaders/Materials/Water.slang`) is a `transparent`-domain material, `twoSided` (the underside is visible from below), `blend` `premultiplied`, with `transparency.refraction` set, drawn in the transparent forward pass sorted back to front with clustered lighting and fog applied per pixel ([Z2-N-REND-249]); its vertex modifier (the hook of `Zenith2/Docs/Design/ENV/Foliage.md → "Material_Foliage"`, pending RPROD) decodes the node from the instance's custom data, positions the grid vertex with the node's morph toward the next coarser grid, and adds the Gerstner displacement of "Gerstner sum" from the body's record in the frame's water-body table with the wave phases of "Time base", for the current and the previous frame's phases (velocity). Refraction reads the opaque colour copy the transparent pass provides; the depth through the water column comes from the scene depth at the refracted pixel.

### Water GPU side

| Element | Specification |
|---|---|
| material | the water cook (the importer and step `env.water`, `Zenith2/Docs/Design/ENV/Bake.md → "Parameter document cook"`) writes, per water document, a generated material document and cooks it through GEOM's material cook as the document's sub-asset `material:water` (its `DeclareProduct` key, `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`): module `Materials/Water.slang`, type `Material_Water`, domain `transparent`, `twoSided` true, blend `premultiplied`, `transparency.refraction` true; parameters `m_xAbsorption` and `m_xScattering` (per metre, float3), `m_fIndexOfRefraction`, `m_fFoamShoreDepth`, `m_fFoamCrestThreshold`, `m_fNormalDetailScale`, `m_fNormalDetailSpeed`, `m_fUnderwaterFogScale`; textures `m_xFoam` (`mask`) and `m_xNormalDetail` (`normal`). The `.zwater` records the material's GUID. A body therefore gets its `Flux_MaterialID` like any material asset, and GEOM re-patches its textures on streaming swaps |
| meshes | two engine-content meshes, each a GEOM `zenith2.grid-mesh` text document (`Zenith2/Engine/ZenithEnvironment/Content/Meshes/WaterGrid.gridmesh.json` and `WaterSkirtQuad.gridmesh.json`, imported by GEOM's `geom-grid-mesh` importer, `Zenith2/Docs/Design/GEOM.md → "Fixtures"`; no glTF or other binary is committed, ruling Q55): `WaterGrid` is exactly `{ "schema": "zenith2.grid-mesh", "version": 1, "quadsX": 32, "quadsZ": 32 }` (33 × 33 vertices at the integer positions (a, 0, b), a, b in [0, 32], 2,048 triangles in the importer's fixed order) and `WaterSkirtQuad` exactly `{ "schema": "zenith2.grid-mesh", "version": 1, "quadsX": 1, "quadsZ": 1 }` (4 vertices, 2 triangles); neither names a `fixedStreamSet`, so the cook's stream-set choice runs as for any positions-only mesh. The meshes carry positions only: `Material_Water` reads no mesh normal and no UV0 (its normal is the Gerstner normal of "Wave synthesis" blended with the detail normals, which it samples at world XZ × `m_fNormalDetailScale`) |
| handles | the module requests the two meshes at world attach and a body's material when the body's `.zwater` activates; `Flux_RenderWaterBodyDelta` carries all three asset handles, and ENV's GPU side keeps copies while the body lives, so a `Flux_MeshID` and `Flux_MaterialID` resolve on the build node ([Z2-N-ARCH-416]: the render world holds asset handles) |
| body table | per frame, ENV's GPU side writes one `Env_WaterBodyRecord` per live body (at most `uENV_MAX_RENDER_WATER_BODIES` = 256; further bodies are not drawn and count `Flux.Environment.WaterBodiesDropped`) into an upload-ring allocation that `Env_FrameEnvironment::m_xWaterBodies` names: base height, amplitude scale, wave count, and per wave direction, wave number, amplitude, steepness and the current and previous phase offsets |
| node instances | `Flux_WaterRenderer` inserts and removes node instances directly in the GPU scene on the build node, as GEOM's terrain renderer does (`Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection"`): primitive = `WaterGrid`'s, material = the body's, flags `FLUX_INSTANCE_FLAG_TRANSPARENT` (never `CASTS_SHADOW`), local-to-world `T(originX + x × size, base height, originZ + z × size) × S(size / 32, 1, size / 32)` with `size = 8 m × 2^level`, instance key `Zenith_Hash64U64(level | x << 4 | z << 14, bodyKey)` |
| custom data | bits 0–3 the node level (0 … 9, 15 for the skirt), bits 4–13 the node x index (for the skirt, its quad index 0 … 7 of the table below) and bits 14–23 the node z index (0 for the skirt) in units of the level's node size from the body's quadtree origin, bits 24–31 the body's index in the frame's water-body table |
| skirt | an ocean body gets eight skirt instances of `WaterSkirtQuad`, the flat square annulus of 8 axis-aligned quads over the lattice x_0 … x_3 = z_0 … z_3 = −32, −1, 1, 32 (units of 4,096 m) with the centre quad omitted: quad q = 0 … 7 is cell (i, j), i, j in 0 … 2, of that 3 × 3 lattice, taken in ascending (j, i) order skipping (1, 1), and its local-to-world is `T(C.x + 4,096 m × x_i, base height, C.z + 4,096 m × z_j) × S(4,096 m × (x_{i+1} − x_i), 1, 4,096 m × (z_{j+1} − z_j))` with C = `O + 4,096 m` per axis the root square's centre, instance key `Zenith_Hash64U64(15 \| q << 4, bodyKey)`, custom data level 15 and quad index q, so the inner edges are exactly the root square's boundary (single 8,192 m edges, design ruling E19) and the outer edge is 131,072 m from that centre (at least 129,024 m from the camera; a main view whose far distance exceeds that draws no ocean beyond it); the modifier adds no displacement to it. An ocean bounded by a rectangle (`Zenith_WaterBodyComponent` half extents) has every node and skirt vertex clamped per axis into the rectangle by the modifier, which clips the axis-aligned quads exactly, and a node or skirt quad wholly outside it collapses to zero area |
| REMOVE | a body REMOVE removes every node instance and the eight skirt instances, frees the body's table index, and drops the render side's copies of the three handles; the module releases its handles after the REMOVE is extracted |

### Optical model

| Term | Model |
|---|---|
| transmittance through the column | Beer–Lambert, `T = exp(−σ_t × d)`, `σ_t = absorption + scattering` per channel, `d` the refracted path length through water |
| in-scatter | single scattering from the sun and sky ambient: `L_s = (σ_s / σ_t) × (1 − T) × (E_sun × p(θ) × T_sun + E_ambient)`, isotropic phase `p = 1/4π` |
| reflectance | Schlick's approximation with `F0 = ((n − 1) / (n + 1))²` from `indexOfRefraction` |
| surface normal | the Gerstner normal blended with the two scrolled detail normal maps |
| foam | shoreline depth below `foam.shoreDepth` and crest height above `foam.crestThreshold × Σ A_i`, masked by the foam texture |

### Reflections by tier

| Tier class | Reflection |
|---|---|
| S24 tiers, desktop low | the sky-view LUT and the nearest IBL probe (LIGHT) at the reflected direction |
| desktop high (Stage 5) | a planar reflection `VIEW_KIND_TEXTURE` view of the ocean or largest visible lake plane at half resolution (`Zenith2/Docs/Rendering.md → "Render views"`), round-robined with the view budget; the other bodies use the S4 path |

### Shoreline and depth fade

Where the water column is shallower than 0.05 m (scene depth against the water surface), the surface fades out over 0.2 m of depth so the shore line does not alias; foam is added from the same depth.

## Underwater

### Submersion test

For each main, editor and texture view request of the world, the environment extractor evaluates `Zenith_WaterWorldResource::IsSubmerged` at the camera's near-plane centre. The query answers at the resource's current tick, the last tick the frame ran, never at the frame's interpolated time: the extractor is a phase-12 reader of a phase-1 resource, and the interpolation between two ticks moves the surface by less than the 0.25 m waterline band below, where the composite decides per pixel anyway. A submerged view gets one `Flux_RenderEnvironmentView` record (`Zenith2/Docs/Design/ENV.md → "Render-world records"`) whose payload is LIGHT's `Flux_ViewMediumOverride`: `m_ulViewID` the `Flux_MakeViewID(kind, owner, sub-index)` of the request's identity, computed exactly as RPROD's camera extractor computes it (`Zenith2/Docs/Design/RPROD/Extractors.md → "View request fill"`, row RP25), so the record and the request name the same view; the body's scattering and absorption × `underwaterFogDensityScale`; and the surface height at the camera. The record adds the body key and `m_bWaterline`, set when the camera is within 0.25 m of the surface, which enables the per-pixel split below. A view above water gets no record. At most 16 records are written per world (`uFLUX_ENV_MAX_SUBMERGED_VIEWS`, ENV's own per-world cap; LIGHT keeps the first `uFLUX_MAX_VIEW_MEDIUM_OVERRIDES` = 16 accepted records per frame across worlds in `Flux_LightingFrame::m_axViewMediumOverrides`, `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"`): when more views are submerged, the 16 nearest the environment anchor (ties to the lower view ID) get one, and each other increments `environment.water.submerged_views_dropped` and draws without the underwater medium that frame.

### Underwater composite pass

`Env_AddUnderwater` (`Zenith2/Shaders/Environment/Passes/Underwater.slang` → `Flux_UnderwaterParams`, a fullscreen pass on the graphics queue after the transparent and particle passes and before the common epilogue, in ENV's GPU side in ZenithFlux, pending RPROD for its frame slot) applies to every pixel whose view ray starts under water (all pixels when the view has a `Flux_RenderEnvironmentView` record without `m_bWaterline`; per pixel against the water plane at the camera when `m_bWaterline` is set): `L' = L × T(d) + L_s(d)` with the water document's medium and `d` the distance to the pixel (capped at the underwater visibility of 60 m), in `half`, writing HDR colour (R11G11B10F, [Z2-N-REND-801]). The pass is not added when no view is submerged.

### Underwater medium

While a view is submerged, the froxel fog's height fog medium for that view is replaced by the water body's medium (`absorption`, `scattering` × `underwaterFogDensityScale`, isotropic phase, and the surface height at the camera), carried by the view's `Flux_RenderEnvironmentView` record, whose payload LIGHT copies into `Flux_LightingFrame::m_axViewMediumOverrides` on the render side (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"`, `Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`; the per-view medium cannot be an environment block, because the camera moves after phase 1; pending LIGHT, rows EN17 and EN23), so transparents and particles under water are fogged consistently; the composite pass above then only adds the part beyond the froxel far distance. Caustics and underwater light shafts are Stage 5 ("Caustics").

## Advanced water

### FFT ocean tier

Stage 5, desktop high only: a JONSWAP spectrum from the water document's `fft` block is synthesised per frame by a compute inverse FFT (`Env_AddOceanFFT`, 2 × `resolution`² complex values, radix-2 Stockham, three dispatches for height and the two horizontal displacements) into a displacement and normal texture tiled over `patchSize`; the ocean's vertex modifier samples it instead of the Gerstner sum. Gameplay keeps querying the Gerstner set (the FFT's CPU equivalent would be a 256² FFT per tick): the FFT tier is presentation, and its height differs from the queried height by at most the error-budget row below, which is why buoyancy uses a document whose Gerstner set approximates the spectrum (the cook fits one and reports the fit error).

### River flow

Stage 5: river splines (`Zenith_WaterBodyComponent` kind RIVER, a spline asset with width and height per point) are cooked into a strip mesh and a flow map; the surface is flat per segment, its normal detail scrolled along the flow direction with the two-phase flow-map technique (two phases half a period apart, blended to hide the reset); the CPU query returns the strip height and the flow velocity.

### Caustics

Stage 5, desktop tiers: an animated caustic texture (the product `texture:caustics` that the water importer declares for each water document and generates from the constants of `Zenith2/Tools/ZenithCook/Internal/Environment/Zenith_CausticsGenerator.h`, versioned by `uENV_CAUSTICS_ALGORITHM_VERSION` in the importer's version; no source file and no document type of its own) projected along the sun direction onto opaque surfaces under the water plane within 20 m of depth, attenuated by the column transmittance, added in the underwater composite's lighting term (for the camera below water) and by an ENV decal-style pass over the terrain under water (for the camera above).

## Water error budget

Water is an approximation validated against its own documented budget, not against the path tracer ([Z2-N-REND-947]). Each row is tested on the water validation scenes (`Zenith2/Engine/ZenithFlux/Tests/Environment/Scenes/Water*`, "Error-budget tests" in `Zenith2/Docs/Design/ENV/Tests.md`):

| Quantity | Reference | Budget | Test |
|---|---|---|---|
| GPU vertex displacement vs the CPU `Zenith_EvaluateGerstner` at 4,096 grid points outside the ocean's fade zone | the CPU function in `double` | ≤ 1 mm per axis for amplitudes up to 4 m | `Test_Water_GpuDisplacementMatchesCpu` |
| height query residual | exact inversion by 64-step bisection in test code | ≤ 1 mm | `Test_Water_HeightQuery_ResidualBelow1mm` |
| Schlick reflectance vs the exact unpolarised Fresnel for `n = 1.333` | closed form in test code | ≤ 0.025 absolute over [0°, 89°] | `Test_Water_SchlickWithinBudget` |
| column transmittance in `half` vs `exp(−σ d)` in `double` | closed form | ≤ 2^−9 relative | `Test_Water_TransmittanceHalfPrecision` |
| analytic normal vs central differences of the CPU surface | finite differences at 1 mm | ≤ 1e-3 per component | `Test_Water_NormalMatchesFiniteDifference` |
| FFT tier height vs its fitted Gerstner set (Stage 5) | the fitted set | the pending row "Ocean FFT to query-set height error" | `Test_Water_FftFitWithinBudget` |
| underwater composite vs a CPU single-scattering integrator over a flat seabed | 1,024-step CPU march in test code | FLIP mean within the pending row "Underwater composite error" | `Test_Underwater_CompositeMatchesReference` |

## Weather model

### Weather document

`*.weather.json`, an authoring document holding the world's weather table:

```json
{
	"schemaVersion": 1,
	"id": "e2b9…",
	"initialState": "Clear",
	"states": [
		{ "name": "Clear", "dwellSeconds": [ 600, 1800 ],
		  "params": { "cloudCoverage": 0.15, "cloudDensity": 0.4, "windSpeed": 3.0, "windDirectionDegrees": 210,
		              "gustStrength": 0.2, "gustFrequency": 0.1, "turbulence": 0.1, "precipitation": "none",
		              "precipitationRate": 0.0, "wetnessTarget": 0.0, "snowTarget": 0.0, "fogDensityScale": 1.0,
		              "hazeScale": 1.0, "lightningPerMinute": 0.0 },
		  "effects": { "prefabs": [] } },
		{ "name": "Rain", "dwellSeconds": [ 300, 900 ], "params": { "…": "…" },
		  "effects": { "prefabs": [ "0c2d…-guid" ] } }
	],
	"transitions": [
		{ "from": "Clear", "to": "Overcast", "weight": 3, "durationSeconds": 240 },
		{ "from": "Overcast", "to": "Rain", "weight": 2, "durationSeconds": 120 }
	]
}
```

| Key | Rule |
|---|---|
| `states` | 1 … 32 states, unique names (the stable IDs are the name hashes `Zenith_Hash64(name)`; renaming is a migration step) |
| `dwellSeconds` | `[min, max]`, 1 … 86,400 s, min ≤ max |
| `params` | the table "Weather state parameters" |
| `effects.prefabs` | prefab GUIDs spawned while the state holds ("Effect prefabs"); `effects` has no other member: weather sound plays through `Zenith.Audio.Emitter` components on these prefabs and AUDIO's data-driven `WeatherChanged` bridge (ruling AU14), so a document carrying `effects.audio` reads it as an unknown member |
| `transitions` | per `from` state at least one outgoing transition unless the state is terminal (no outgoing: it holds until a command); `weight` 1 … 1,000; `durationSeconds` 1 … 3,600; `from` and `to` distinct and existing |
| `initialState` | an existing state |

### Weather state parameters

| Parameter | Range | Consumed by |
|---|---|---|
| `cloudCoverage`, `cloudDensity` | 0 … 1 | clouds (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud model"`) |
| `windSpeed` (m/s), `windDirectionDegrees`, `gustStrength`, `gustFrequency` (Hz), `turbulence` | 0 … 60; 0 … 360; 0 … 2; 0.01 … 2; 0 … 1 | the wind state (`Zenith2/Docs/Design/ENV/Foliage.md → "Global wind state"`) |
| `precipitation` | `none` · `rain` · `snow` | effect prefabs, wetness or snow targets |
| `precipitationRate` | mm/h, 0 … 100 | presentation and gameplay events |
| `wetnessTarget`, `snowTarget` | 0 … 1 | the surface wetness and snow cover the world converges to ("Presentation parameters") |
| `fogDensityScale`, `hazeScale` | 0 … 10 | LIGHT's height fog density and the atmosphere's Mie scattering (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Weather modulation of the atmosphere"`) |
| `lightningPerMinute` | 0 … 60 | lightning events (Stage 5 flashes) |

## Zenith_WeatherSystem header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_WeatherSystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

/// @contract value Zenith_WeatherParams
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: the weather runtime blends and reassigns its current copy every tick.
/// Invalidation: None.
/// Units: As the table "Weather state parameters"; angles in radians after load.
/// Errors: None; validated at load.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WeatherParams
{
	float m_fCloudCoverage = 0.0f;
	float m_fCloudDensity = 0.0f;
	float m_fWindSpeed = 0.0f;
	float m_fWindDirection = 0.0f;           // radians, 0 = +Z, positive toward +X
	float m_fGustStrength = 0.0f;
	float m_fGustFrequency = 0.1f;
	float m_fTurbulence = 0.0f;
	Zenith_PrecipitationKind m_ePrecipitation = PRECIPITATION_KIND_NONE;
	float m_fPrecipitationRate = 0.0f;
	float m_fWetnessTarget = 0.0f;
	float m_fSnowTarget = 0.0f;
	float m_fFogDensityScale = 1.0f;
	float m_fHazeScale = 1.0f;
	float m_fLightningPerMinute = 0.0f;
};

/// @contract value Zenith_WeatherTransitionDesc
/// Ownership: Plain value inside a loaded Zenith_WeatherTable.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while an asset handle to the table is held.
/// Units: Ticks for the duration (converted from the document's seconds at load with the world's tick rate).
/// Errors: None after load ("Reader validation" in Bake.md).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WeatherTransitionDesc
{
	uint32 m_uFromState = 0;                // index into the table's states
	uint32 m_uToState = 0;
	uint32 m_uWeight = 1;                   // 1 … 1,000
	uint32 m_uReserved = 0;
	uint64 m_ulDurationTicks = 1;           // ≥ 1
};

/// @contract type Zenith_WeatherTable
/// Ownership: Owned by the Zenith_WeatherAsset that the Environment.Weather kind loader builds (Bake.md "Runtime asset
///   objects"), reached through Zenith_WeatherAsset::GetTable; the asset registry owns the asset through its handle.
/// Thread affinity: IMMUTABLE after load.
/// Invalidation: Valid while an asset handle to it is held.
/// Units: Ticks for dwell and durations (converted from seconds at load with the world's tick rate).
/// Errors: None after load; the loader validates ("Reader validation" in Bake.md).
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WeatherTable
{
public:
	uint32 GetStateCount() const;
	uint32 FindState(uint64 ulNameHash) const;             // uENV_WEATHER_STATE_NONE when absent
	const Zenith_WeatherParams& GetParams(uint32 uState) const;
	uint32 GetInitialState() const;
	Zenith_Span<const Zenith_WeatherTransitionDesc> GetTransitionsFrom(uint32 uState) const;   // file order
	uint64 GetDwellMinTicks(uint32 uState) const;
	uint64 GetDwellMaxTicks(uint32 uState) const;
};

/// @contract value Zenith_WeatherRuntimeState
/// Ownership: Plain value, part of Zenith_EnvironmentState; serialized in saves by its reflected properties.
/// Thread affinity: PHASE(Input) for writes; IMMUTABLE to readers within a tick.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 4. Its reflected properties are saved; a change is a save migration step.
/// @endcontract
struct Zenith_WeatherRuntimeState
{
	uint32 m_uCurrentState = 0;
	uint32 m_uTargetState = 0;              // equals m_uCurrentState when not transitioning
	uint64 m_ulStateEnteredTick = 0;
	uint64 m_ulDwellEndTick = 0;            // when the next transition is chosen
	uint64 m_ulTransitionStartTick = 0;
	uint64 m_ulTransitionTicks = 0;         // 0 when not transitioning
	uint64 m_ulDrawCounter = 0;             // the weather stream's counter
	bool m_bHeld = false;                    // a FORCE with hold: no automatic transitions until RELEASE
	Zenith_WeatherParams m_xBlended;         // the parameters in effect this tick
	float m_fWetness = 0.0f;                 // the converging surface wetness
	float m_fSnow = 0.0f;                    // the converging snow cover
};

/// @contract functions Zenith_WeatherFunctions
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: Zenith_StepWeather returns ENV_ERROR_WEATHER_STATE_UNKNOWN for a command naming no state and leaves the state
///   unchanged; nothing asserts on content.
/// Version: since Z2 Stage 4.
/// @endcontract
// One tick of the state machine: applies the tick's commands in merge order, then the automatic events, then blending
// and convergence. Deterministic in (table, state, commands, world seed, tick).
Zenith_Result<void, Zenith_EnvironmentError> Zenith_StepWeather(const Zenith_WeatherTable& xTable,
	Zenith_Span<const Zenith_EnvironmentCommand> axCommands, uint64 ulWorldSeed, uint64 ulTick, uint32 uTickHz,
	Zenith_WeatherRuntimeState& xState, Zenith_Span<Zenith_WeatherChangedEvent> axEventsOut, uint32& uEventCountOut);
Zenith_WeatherRuntimeState Zenith_InitialWeatherState(const Zenith_WeatherTable& xTable, uint64 ulWorldSeed);
```

`Zenith_WeatherSystem` is internal (`Internal/Weather/Zenith_WeatherSystem.h`): the module's per-world owner of the table asset handle, called by `Environment.Advance` in phase 1. `Zenith_EnvironmentCommand` (the weather kinds `ENVIRONMENT_COMMAND_REQUEST_WEATHER`, `ENVIRONMENT_COMMAND_FORCE_WEATHER` and `ENVIRONMENT_COMMAND_RELEASE_WEATHER`, with a state name hash, a transition duration in seconds converted to ticks at playback, and the hold flag), `Zenith_WeatherChangedEvent` and `Zenith_PrecipitationKind` are declared in `Zenith_EnvironmentTypes.h` (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentTypes header"`), which this header includes, so it compiles alone; `Zenith_WeatherTransitionDesc` is declared above.

## Weather state machine

### States and events

The machine's state is (`current`, `target`, `transitioning`, `held`). Events, handled in this order within one tick:

| Event | Source |
|---|---|
| `LOAD` | a save restore or world start: the state comes from the save, or `Zenith_InitialWeatherState` |
| `FORCE(s, d, hold)` | an `ENVIRONMENT_COMMAND_FORCE_WEATHER` command (script, editor, cinematic), applied in merge order |
| `REQUEST(s, d)` | an `ENVIRONMENT_COMMAND_REQUEST_WEATHER` command |
| `RELEASE` | an `ENVIRONMENT_COMMAND_RELEASE_WEATHER` command |
| `TRANSITION_END` | `tick ≥ transitionStart + transitionTicks` while transitioning |
| `DWELL_END` | `tick ≥ dwellEnd` while not transitioning and not held |

### Transition table

| State \ Event | `FORCE(s, d, hold)` | `REQUEST(s, d)` | `RELEASE` | `TRANSITION_END` | `DWELL_END` |
|---|---|---|---|---|---|
| steady (`current = target`), not held | start transition to `s` over `d` ticks (`d = 0`: switch now); `held = hold` | start transition to `s` over `d` | no effect | not possible | choose a successor ("Dwell and choice draws") and start a transition to it over its duration; a terminal state re-draws its dwell and stays |
| steady, held | as not held (a new force replaces the hold) | refused: `environment.weather.requests_refused_held` and a log line | `held = false`; draw a new dwell from now | not possible | not raised while held |
| transitioning, not held | restart from the current blended parameters toward `s` over `d` | restart toward `s` over `d` (blending continues from the current blend) | no effect | `current = target`; emit the arrival event; draw the dwell | not raised while transitioning |
| transitioning, held | as the row above | refused as when steady and held | `held = false`; the transition continues | as the not-held row (the state stays held) | not raised |

Every transition start and every arrival emits one `Zenith_WeatherChangedEvent` (`from`, `to`, `transitionTicks`, `bArrived`, and the target state's rain intensity, snow intensity, wind speed and wind direction: `m_fPrecipitationRate` into the rain or snow field by the target's `m_ePrecipitation`, the other 0).

### Dwell and choice draws

The weather stream is `Zenith_RandomSource(worldSeed, Zenith_Hash64("Zenith.Environment.Weather"))`, its counter stored in `m_ulDrawCounter` and saved with the state:

- dwell: `dwell = Range(dwellMin, dwellMax)` in ticks (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_RandomSource"`, Lemire range), `dwellEnd = tick + dwell`;
- successor: `r = Range(0, Σweights − 1)`; walk the state's transitions in file order, subtracting weights, and pick the first whose cumulative weight exceeds `r`.

Draws happen only on `DWELL_END`, `TRANSITION_END` and `RELEASE`, in the event order above, so the sequence of weather is a pure function of (table, seed, commands with their ticks); a replay reproduces it, and it is identical for every worker count because the machine runs in one node (Z2-D-019).

### Blending parameters during a transition

With `a = (tick − transitionStart) / transitionTicks` clamped to [0, 1] and `s = a² (3 − 2a)`, each scalar parameter of `m_xBlended` is `lerp(from, to, s)`; wind direction blends along the shorter arc; `precipitation` switches to the target's kind at `a ≥ 0.5` (to `none` first when the kinds differ, then to the new kind); a restarted transition blends from the current `m_xBlended`. Surface wetness and snow converge toward the blended targets at fixed rates, `wetness += clamp(target − wetness, −1/1,800, +1/600)` per second of simulation time (wet in 10 minutes of rain, dry in 30), and the same for snow with rates 1/3,600 and 1/7,200.

## Weather effects

### Effect prefabs

While a state holds (or is the transition target past `a ≥ 0.5`), the module keeps one instance of each of its `effects.prefabs` spawned, parented to the world's environment anchor entity (the entity carrying `Zenith_EnvironmentAnchorComponent`, typically the camera rig; without one, at the world origin): rain and snow emitters (VFX), rain and wind ambience emitters (AUDIO), lightning controllers (GAME samples). Spawning and destroying go through structural commands recorded by `Environment.Advance` in phase 1 and played back at its sync point ([Z2-N-ARCH-489]), so effect prefabs are ordinary deterministic entities whose world transforms follow the anchor by design; nothing ENV simulates reads them, and the anchor changes no other simulation result (`Test_EnvironmentAnchor_FocusAndFallback`). ENV has no dependency on VFX or AUDIO, which see their own components on those entities (design ruling E6 in the entry).

### Presentation parameters

`Environment.Advance` writes the blended cloud, fog, haze, wetness and snow values into the environment blocks' cloud, atmosphere, fog and weather members each tick (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Extraction steps"`), which the engine environment extractor copies into the render environment; they change presentation only.

### Shared wetness function

```slang
// Zenith2/Shaders/Common/Environment.slang
[FluxShared] struct Env_WeatherParams     // 16 bytes, the render environment's weather block
{
	float m_fWetness;
	float m_fSnow;
	float m_fPrecipitationRate;
	float m_fReserved;
};

// Zenith2/Shaders/Environment/Weather.slang (imports Common/Environment.slang)
// Applied at the end of a material's Evaluate: darkens porous albedo, lowers roughness toward a water film, and blends
// snow onto upward-facing surfaces (Stage 4: snow as albedo and roughness; Stage 5: accumulation masks).
void Env_ApplySurfaceWeather(inout Flux_SurfaceData xSurface, Env_WeatherParams xWeather, half fPorosity, half fExposure);
```

| Term | Rule |
|---|---|
| darkening | `albedo × lerp(1, 0.35 + 0.65 (1 − fPorosity), wetness × fExposure)` |
| roughness | `lerp(roughness, 0.05, wetness × fExposure × (1 − fPorosity × 0.5))` |
| snow | `m = saturate((n_WS.y − 0.6) / 0.3) × snow × fExposure`; albedo `lerp(albedo, 0.9, m)`, roughness `lerp(roughness, 0.6, m)` |

`fExposure` (1 outdoors, 0 under cover) is 1 in Stage 4 for every surface; the Stage 5 precipitation occlusion ("Weather tiers") supplies a real value. `Material_TerrainLayered`, `Material_Foliage` and, through a GEOM edit, `Material_Standard` call it (pending GEOM, cross-area note EN8).

### Weather events for gameplay

`Zenith_WeatherChangedEvent` is a registered `TYPE_KIND_EVENT` ("Zenith.Environment.WeatherChanged"): emitted by `Environment.Advance`, merged at the sync point after phase 1 and delivered in phase 9 of the same tick ([Z2-N-ARCH-478]); Luau behaviours and GAME systems read it (lightning, NPC reactions, audio stingers), and AUDIO's `WeatherChanged` bridge sets its `Rain`, `Snow` and `Wind` global parameters from the event's `m_fRainIntensity`, `m_fSnowIntensity` and `m_fWindSpeed` (`Zenith2/Docs/Design/AUDIO/Authoring.md`).

## Weather tiers

| Tier | Stage | Feature |
|---|---|---|
| base (every platform) | S4 | the table, blending, wind, clouds, fog and haze modulation, wetness and snow as uniform material terms, effect prefabs |
| puddles | S5, desktop | a puddle mask (terrain layer alpha plus a world-space noise) where `wetness > 0.5`: flattened normals, mirror roughness, rain ripples from an animated normal texture |
| snow accumulation | S5, desktop and S24 high | a snow-depth term driving terrain and foliage blends with a noise edge, and a vertex offset on terrain up to 5 cm |
| precipitation occlusion | S5, desktop and S24 high | a top-down depth capture (`VIEW_KIND_CAPTURE`, 512², updated round-robin in 64 m steps around the camera) giving `fExposure` per surface point and masking rain particles (VFX) under roofs |
| lightning flashes | S5 | a brief directional light pulse and sky flash on each lightning event |
