# LIGHT design: lights, clustering and the shared lighting contract

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies the light model and its units, the light record encodings LIGHT adds to Rendering's light table, the render-world light delta, the frame light and decal list, the froxel light clusters, the single-source `EvaluateLighting` contract, the BRDF and its energy terms, and the deferred lighting pass. The pass tables (programs, parameter fields, targets, formats, queues, budget rows) are Rendering's and are cited, never restated: `Zenith2/Docs/Rendering/PerPass/Common.md → "LightDecalListBuild"`, `Zenith2/Docs/Rendering/PerPass/Common.md → "LightClustering"`, `Zenith2/Docs/Rendering/PerPass/Common.md → "DeferredLighting"` and `Zenith2/Docs/Rendering.md → "Single-source lighting"`.

## Scope of this file

| Topic | Heading |
|---|---|
| light kinds, units and conversions | "Light units" |
| the light table record per light type | "Light record encodings" |
| the render-world light delta and the CPU light mirror | "Light extraction and the CPU light mirror" |
| the frame light and decal list | "Light and decal list build" |
| froxel clusters | "Light clustering" |
| `EvaluateLighting`, its inputs and outputs | "The EvaluateLighting contract" |
| the BRDF, energy conservation and precision | "BRDF" |
| punctual attenuation, spot cones, sphere and area lights | "Light attenuation and shape" |
| lighting channels | "Lighting channels" |
| advanced surface lobes | "Advanced surface lobe integration" |
| deferred and visibility-buffer lighting | "Deferred lighting pass" |
| the cooked lighting lookup tables | "Lighting lookup tables" |
| `Flux_Lights.h` | "Flux_Lights header" |

## Light units

Every light intensity in the render world, the light table and the shaders is photometric, in the unit of the table below; colour is a separate linear Rec.709 tint whose maximum component is 1.0 (`Zenith2/Docs/Design/CORE/Maths.md → "Units"` delegates light units to LIGHT).

<!-- elaboration -->
| Light type (`Flux_LightType`) | Quantity stored in `m_xColourIntensity` | Unit | Authoring units accepted by extraction | Conversion to the stored unit |
|---|---|---|---|---|
| `FLUX_LIGHT_TYPE_DIRECTIONAL` | illuminance at normal incidence | lux (lm/m²) | lux | identity |
| `FLUX_LIGHT_TYPE_POINT` | luminous intensity | candela (cd) | candela; lumens | `I = Φ / (4π)` |
| `FLUX_LIGHT_TYPE_SPOT` | luminous intensity on the axis | candela | candela; lumens | `I = Φ / (2π · (1 − cos θ_outer))`, θ_outer the outer half-angle |
| `FLUX_LIGHT_TYPE_RECT` (Stage 5) | luminance of the emitting face | nits (cd/m²) | nits; lumens | `L = Φ / (π · w · h)` for a one-sided Lambertian emitter; two-sided `L = Φ / (2π · w · h)` |
| `FLUX_LIGHT_TYPE_DISC` (Stage 5) | luminance of the emitting face | nits | nits; lumens | `L = Φ / (π · π r²)`; two-sided halves it |

- A point or spot light with `m_fSourceRadius > 0` is a sphere light: its intensity stays candela (the sphere's luminance is `I / (π r²)`), and "Light attenuation and shape" widens its specular lobe.
- Exposure uses EV100 (`Zenith2/Docs/Design/LIGHT/PostProcess.md → "Exposure units"`); a pre-exposed radiance is `L × fPreExposure`, where `fPreExposure = 1 / (1.2 × 2^EV100)`.
- Colour temperature authoring (Kelvin) is converted by extraction to a linear Rec.709 tint with `Flux_KelvinToLinearRec709`: the CIE 1931 Planckian locus chromaticity of the Kang et al. 2002 cubic fit (1,667 K to 25,000 K), converted to XYZ at Y = 1 and then to Rec.709 with the matrix of `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Colour-space matrices"`, negative components clamped to 0 and the result normalised so its maximum component is 1.0; out-of-range temperatures clamp to the range. The fit, with `T` the clamped temperature in kelvin and every term evaluated in `double` before the result is narrowed: `x = −0.2661239e9/T³ − 0.2343589e6/T² + 0.8776956e3/T + 0.179910` for `T ≤ 4,000`, else `x = −3.0258469e9/T³ + 2.1070379e6/T² + 0.2226347e3/T + 0.240390`; `y = −1.1063814x³ − 1.34811020x² + 2.18555832x − 0.20219683` for `T ≤ 2,222`, `y = −0.9549476x³ − 1.37418593x² + 2.09137015x − 0.16748867` for `2,222 < T ≤ 4,000`, else `y = 3.0817580x³ − 5.87338670x² + 3.75112997x − 0.37001483`; then `X = x / y`, `Y = 1`, `Z = (1 − x − y) / y`. The fit lies off the daylight locus, so 6,500 K is not white: it is (1, 0.9436, 0.9928) after normalisation, 1,667 K is (1, 0.1758, 0) (blue clamped) and 25,000 K is (0.3774, 0.5124, 1).

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Lights.h (excerpt; full header in "Flux_Lights header")
float Flux_LumensToCandelaPoint(float fLumens);                                  // Φ / 4π
float Flux_LumensToCandelaSpot(float fLumens, float fOuterHalfAngleRadians);     // Φ / (2π(1 − cos θ))
float Flux_LumensToNitsRect(float fLumens, float fWidthMetres, float fHeightMetres, bool bTwoSided);
float Flux_LumensToNitsDisc(float fLumens, float fRadiusMetres, bool bTwoSided);
Zenith_Colour Flux_KelvinToLinearRec709(float fKelvin);
```

| Degenerate input | Result |
|---|---|
| `fOuterHalfAngleRadians` ≤ 1e-4 | clamped to 1e-4 before the conversion |
| a width, height or radius ≤ 1e-4 m | clamped to 1e-4 m |
| a negative or non-finite lumen value | 0, and the extractor logs the `flux.lighting` warning `light <key>: invalid intensity` once per light |

## Light record encodings

Rendering fixes the 64-byte `Flux_LightRecord` (`Zenith2/Docs/Rendering/GpuResources.md → "Light record (64 bytes)"`) and delegates further types to this file. LIGHT keeps the byte layout and defines, per `m_uType`, how each field is read.

| Offset | Field | Directional (0) | Point (1) | Spot (2) | Rect (3), Stage 5 | Disc (4), Stage 5 |
|---|---|---|---|---|---|---|
| 0 | `m_xPositionWS` | unused (0) | centre | apex | centre of the rectangle | centre of the disc |
| 12 | `m_fRange` | `FLT_MAX` | range, m | range, m | range, m | range, m |
| 16 | `m_xDirectionWS` | direction the light travels (the light's local −Z) | unused (0, 0, −1) | axis (local −Z) | emitting normal (local −Z) | emitting normal (local −Z) |
| 28 | `m_uType` | 0 | 1 | 2 | 3 | 4 |
| 32 | `m_xColourIntensity` | tint × lux | tint × cd | tint × cd | tint × nits | tint × nits |
| 44 | `m_fSourceRadius` | angular radius of the sun disc, radians (default 0.00465) | sphere radius, m | sphere radius, m | the rectangle's tangent (local +X), octahedral `snorm16x2` packed into the 32 bits (`Octahedral.slang`) | disc radius, m |
| 48 | `m_fSpotCosInner` | 0 | 0 | cos θ_inner | half width, m | cos θ_inner of the optional barn cone (1.0 when absent) |
| 52 | `m_fSpotCosOuter` | 0 | 0 | cos θ_outer | half height, m | cos θ_outer of the optional barn cone (−1.0 when absent) |
| 56 | `m_uShadowIndex` | first cascade's `Flux_ShadowViewRecord` | first of 6 face records | the spot's record | the rect's record (a spot-like perspective view) | the disc's record |
| 60 | `m_uFlags` | Rendering's bits 0–2 and 8–15 plus LIGHT's bits below | | | | |

| `m_uFlags` bits | Name | Owner | Meaning |
|---|---|---|---|
| 0 | `FLUX_LIGHT_FLAG_CASTS_SHADOW` | Rendering | casts shadows when a shadow view was allocated this frame |
| 1 | `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` | Rendering | honoured for directional lights only; set on another type, it is cleared by extraction and counted in `Flux.Lights.FlagIgnored` |
| 2 | `FLUX_LIGHT_FLAG_VOLUMETRIC` | Rendering | the light injects into the froxel volume |
| 3 | `FLUX_LIGHT_FLAG_TWO_SIDED` | LIGHT | rect and disc lights emit from both faces |
| 4 | `FLUX_LIGHT_FLAG_NO_SPECULAR` | LIGHT | the light contributes diffuse only |
| 5–7 | reserved | LIGHT | zero; no flag names a sun or a moon |
| 8–15 | lighting-channel mask | Rendering | see "Lighting channels" |
| 16–23 | volumetric scale | LIGHT | `unorm8`; the injection multiplies the light by `2 × value / 255` (so 128 ≈ 1.0) |
| 24–31 | reserved | LIGHT | zero |

- `Flux_EncodeLightRecord` (below) is the one CPU function that writes a `Flux_LightRecord`; RHI's `Flux_GPUScene::ApplyDeltas` calls it for every accepted light delta and the scatter upload carries its output unchanged ([Z2-N-REND-531]).
- The sun and the moon occupy the two reserved light-table slots of "Sun and moon slots". Which body a record is follows from its slot alone: the sun and moon roles of the directional list are positional ("Light and decal list build"), so no flag bit carries a role, and a role never depends on a bit that an authored light could carry or that a writer could forget.
- A rect light's tangent is re-orthogonalised against its normal by extraction before packing; the bitangent is `cross(normal, tangent)`.

## Light extraction and the CPU light mirror

The render world carries lights as deltas (`Flux_RenderWorldSection::m_axLightDeltas`, `Zenith2/Docs/Rendering.md → "Purpose and scope"`). LIGHT declares the delta record, its key and the two functions that validate and encode it. The engine light extractor (RPROD's `Internal/Zenith_EngineExtractors.cpp`, `Zenith2/Docs/Design/MOD.md → "ZenithRuntime files"`) produces the deltas from the light components of `Zenith2/Docs/Design/LIGHT.md → "Light component schemas"`; the sun and the moon are not deltas but the render environment's two slots, which the engine environment extractor fills from ENV's celestial state (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Sun and moon illuminance"`, "Sun and moon slots" below).

### Light keys

<!-- elaboration -->
A key identifies one light for the lifetime of its world and is the tie-break of every light ordering in this family. Keys come from exactly two functions, whose outputs are disjoint by bit 63:

| Function | Used by | Value |
|---|---|---|
| `Flux_MakeEntityLightKey(xEntity, uWorldTag)` | the engine light extractor, for a light component's entity | `Zenith_Hash64(the 16 bytes of the entity's Zenith_PersistentID, seed = uWorldTag)` with bit 63 cleared |
| `Flux_MakeNamedLightKey(strName, uWorldTag)` | an engine-synthesised light with no entity: ENV's sun (`"Zenith.Environment.Sun"`) and moon (`"Zenith.Environment.Moon"`) | `Zenith_Hash64(strName, seed = uWorldTag)` with bit 63 set |

The hash is CORE's `Zenith_Hash64` (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Hash64"`) on the D-SEL-HASH (open, Stage 1) path; a D-SEL-HASH change moves every key at once, which changes no persistent data because keys are never serialized. Names beginning `Zenith.` are reserved for engine areas; ENV's extractor calls `Flux_MakeNamedLightKey` rather than hashing the name itself, so the sun key is the same bytes on both sides (answers cross-area row EN6, row L31).

### The delta record

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Lights.h
enum Flux_LightType : uint8
{
	FLUX_LIGHT_TYPE_DIRECTIONAL = 0,
	FLUX_LIGHT_TYPE_POINT = 1,
	FLUX_LIGHT_TYPE_SPOT = 2,
	FLUX_LIGHT_TYPE_RECT = 3,     // Stage 5
	FLUX_LIGHT_TYPE_DISC = 4,     // Stage 5
	FLUX_LIGHT_TYPE_COUNT
};

enum Flux_LightDeltaOp : uint8 { FLUX_LIGHT_DELTA_OP_ADD = 1, FLUX_LIGHT_DELTA_OP_UPDATE, FLUX_LIGHT_DELTA_OP_REMOVE };

// struct Flux_RenderLightKey { uint64 m_ulValue = 0; }; is declared in the leaf header Flux_ShadowAtlas.h

struct Flux_RenderLightDelta   // 112 bytes; a render-world value, IMMUTABLE after Render.Seal
{
	Flux_RenderLightKey m_xKey;
	Flux_LightDeltaOp m_eOp;
	Flux_LightType m_eType;
	uint8 m_uLightingChannels;          // bit per channel; default 0x01
	uint8 m_uVolumetricScale;           // unorm8, 128 = 1.0
	uint32 m_uFlags;                    // FLUX_LIGHT_FLAG_* bits 0–4 of "Light record encodings"; 5–31 zero
	Zenith_Vector3 m_xPositionWS;
	float m_fRange;                      // metres; > 0
	Zenith_Vector3 m_xDirectionWS;       // unit; the light's local −Z in world space
	float m_fSourceRadius;               // metres (point, spot, disc) or radians (directional)
	Zenith_Vector3 m_xTangentWS;         // unit; rect only
	float m_fIntensity;                  // in the type's stored unit ("Light units"), already converted
	Zenith_Colour m_xTint;               // linear Rec.709, alpha ignored; max component 1.0
	float m_fSpotInnerHalfAngle;         // radians
	float m_fSpotOuterHalfAngle;         // radians, ≥ inner
	float m_fWidth;                      // rect, metres
	float m_fHeight;                     // rect, metres
	float m_fShadowPriorityBias;         // multiplies the shadowed-light priority; default 1
	uint16 m_uShadowMaxTileTexels;      // 0 = the tier's maximum; else a power of two clamping the atlas tile size
	uint16 m_uReserved;                 // zero
	float m_fShadowNormalOffsetTexels;   // default 1.5
	float m_fShadowDepthBias;            // light-space metres; default 0.0
};
static_assert(sizeof(Flux_RenderLightDelta) == 112);
static_assert(alignof(Flux_RenderLightDelta) == 8);
```

<!-- elaboration -->
| Offset | Size | Field |
|---|---|---|
| 0 | 8 | `m_xKey` |
| 8 | 1 | `m_eOp` |
| 9 | 1 | `m_eType` |
| 10 | 1 | `m_uLightingChannels` |
| 11 | 1 | `m_uVolumetricScale` |
| 12 | 4 | `m_uFlags` |
| 16 | 12 | `m_xPositionWS` (`sizeof(Zenith_Vector3) == 12`) |
| 28 | 4 | `m_fRange` |
| 32 | 12 | `m_xDirectionWS` |
| 44 | 4 | `m_fSourceRadius` |
| 48 | 12 | `m_xTangentWS` |
| 60 | 4 | `m_fIntensity` |
| 64 | 16 | `m_xTint` (`sizeof(Zenith_Colour) == 16`, `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Colour"`) |
| 80 | 4 | `m_fSpotInnerHalfAngle` |
| 84 | 4 | `m_fSpotOuterHalfAngle` |
| 88 | 4 | `m_fWidth` |
| 92 | 4 | `m_fHeight` |
| 96 | 4 | `m_fShadowPriorityBias` |
| 100 | 2 | `m_uShadowMaxTileTexels` |
| 102 | 2 | `m_uReserved` |
| 104 | 4 | `m_fShadowNormalOffsetTexels` |
| 108 | 4 | `m_fShadowDepthBias` |
| 112 | — | end; no tail padding (112 is a multiple of the 8-byte alignment of `m_xKey`) |

`Test_LightDelta_Layout` asserts the size, the alignment and every offset with `offsetof`. The record is a render-world value and is never serialized ([Z2-N-ARCH-644] does not apply), so its layout may change in any build; the test pins it so the render-world slot arena's per-record cost stays what the budget assumed.

### Validation

`Flux_ValidateLightDelta(const Flux_RenderLightDelta&)` is a pure function returning `Zenith_Result<Flux_RenderLightDelta, Flux_LightingError>`: the accepted delta with ignored flags cleared, or `FLUX_LIGHTING_ERROR_INVALID_LIGHT`.

| Rule | Result |
|---|---|
| a type outside `Flux_LightType`, a non-finite field, a non-unit direction (`|length − 1| > 1e-3`), range ≤ 0 (non-directional), an outer angle below the inner angle or at least π/2, a width or height ≤ 0 (rect), a negative intensity | refused |
| `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` on a non-directional light; `FLUX_LIGHT_FLAG_TWO_SIDED` on a point, spot or directional light; a reserved flag bit (5–7, 24–31) set | the flag is cleared, the delta accepted, `Flux.Lights.FlagIgnored` counted |
| a `REMOVE` | only the key is read; never refused |

The environment slots are validated by `Flux_ValidateEnvironmentLight(const Flux_RenderLightDelta& xSlot)`, a pure function returning the same `Zenith_Result`: every rule above, the flag-clearing rules included (a set reserved bit, 5–7 or 24–31, is cleared and counted in `Flux.Lights.FlagIgnored` exactly as for a delta), plus type `FLUX_LIGHT_TYPE_DIRECTIONAL`; a refusal is `FLUX_LIGHTING_ERROR_INVALID_LIGHT`. It takes no role argument: the slot's role is its index (0 the sun, 1 the moon), and no flag bit is checked for a role.

### Division of the delta work with the GPU scene

<!-- assumption: pending RHI, row L30 -->
RHI's `Flux_GPUScene::ApplyDeltas` consumes `m_axLightDeltas` (`Zenith2/Docs/Rendering.md → "The render world hand-off"`, `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`) and owns the light table's slots. This design assumes it calls LIGHT's two pure functions for each light delta and adds two members, `uint32 FindLightSlot(Flux_RenderLightKey xKey) const` (`uFLUX_GPU_SCENE_INVALID_SLOT` = `0xFFFFFFFF` when the key holds no slot) and `void StageLightRecord(uint32 uSlot, const Flux_LightRecord& xRecord)` (a whole-record staged write on the build node, last write wins per slot like every staged record), plus `ReserveLightSlots` of "Sun and moon slots". `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"` declares none of the three today and its `ApplyDeltas` text does not yet call LIGHT's two functions: row L30 of the cross-area notes is the open request, and every LIGHT ticket that calls one of the three cites L30 and is blocked by RHI's revision.
<!-- /assumption -->

| Step (`Flux.BuildGraph`, render-world consumption, sections in ascending world-tag order, deltas in merged order, [Z2-N-REND-008]) | Owner | Work |
|---|---|---|
| 1 | RHI, `Flux_GPUScene::ApplyDeltas` | per light delta: `Flux_ValidateLightDelta`; a refused delta is skipped (the previous record of its key, if any, stays). `ADD`: allocate a light-table slot (RHI's slot allocator); the staged record is `Flux_EncodeLightRecord(accepted, 0xFFFFFFFF)`; at the table maximum the light is not rendered (`FLUX_GPU_SCENE_ERROR_TABLE_FULL`, `Flux.GPUScene.TableFull`). `UPDATE`: re-encode into the key's slot; an unknown key is an `ADD`. `REMOVE`: the deferred free ([Z2-N-REND-538]); an unknown key is ignored |
| 2 | LIGHT, `Flux_LightingSystem::ApplyRenderWorld`, immediately after step 1 | the same traversal over the CPU light mirror (a `Zenith_Array<Flux_CPULight>` plus an open-addressing `Zenith_HashMap<uint64, uint32>` from key to mirror index): the same `Flux_ValidateLightDelta` (a pure function, so the same verdict as step 1); a refused delta is logged once per key at `LOG_LEVEL_WARNING` (`flux.lighting`, `light <key>: invalid`) and counted in `Flux.Lights.Invalid`; `ADD`/`UPDATE` store the accepted delta, `m_uTableSlot = FindLightSlot(key)` and `m_bChangedThisFrame = true`; a key with no slot is kept with `m_bDropped = true` and counted in `Flux.Lights.Dropped`; an `UPDATE` of an unknown key counts `Flux.Lights.UpdateUnknown`, a `REMOVE` of an unknown key `Flux.Lights.RemoveUnknown`; `REMOVE` erases the entry |
| 3 | LIGHT, `Flux_LightingSystem::PlanShadows` (view-list construction) | for every light with `m_bChangedThisFrame`, or whose shadow index or casts-shadow flag differs from the previous frame's, `StageLightRecord(slot, Flux_EncodeLightRecord(delta, uShadowIndex))` with this frame's shadow index (`0xFFFFFFFF` when unshadowed); because it runs after step 1 and a staged record is last-write-wins, the record uploaded this frame always carries this frame's shadow state ([Z2-N-REND-604]) |
| 4 | LIGHT, `Flux_LightingSystem::EndFrame` | on a submitted frame only (`bFrameSubmitted`), `m_bChangedThisFrame = false` for every entry; on a frame that submits nothing the flags are kept, so the next built frame restages those lights (`Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"`) |

### Sun and moon slots

<!-- assumption: pending RPROD, ENV, RHI, X125, row L30 -->
The sun and the moon (Stage 3, Z2-D-131, the Q45 ruling core-doc batch 4 applied: under the orchestrator's Q45 ruling the Stage 3 W-Outdoor deliverable wins and ENV fills the slots from Stage 3; a frame whose world has no ENV celestial state has no slots, and the brightest scene directional light then takes the sun role) are not light deltas: `Flux_RenderEnvironment` carries them as the explicit slots `Flux_RenderLightDelta m_xSun` and `m_xMoon` with `m_bHasSun` and `m_bHasMoon` (`Zenith2/Docs/Rendering.md → "Render environment"`, RPROD row RP17, the orchestrator's ruling on cross-doc item X125 (applied by core-doc batch 4)), filled by the one environment writer from ENV's celestial state with the keys `Flux_MakeNamedLightKey("Zenith.Environment.Sun", uWorldTag)` and `…("Zenith.Environment.Moon", uWorldTag)`. Row L31 of the cross-area notes records LIGHT's side.
<!-- /assumption -->

| Rule | Value |
|---|---|
| table slots | `Flux_LightingSystem::Create` reserves light-table slots 0 and 1 through `Flux_GPUScene::ReserveLightSlots(2)` (pending RHI, row L30); no delta ever allocates them, and a full-state resync, which discards and re-allocates every slot, keeps slots 0 and 1 reserved and never hands one to a re-added delta |
| slot flag contract | LIGHT's authority over the light model, which ENV's writer must meet (rows L31 and L39): the sun slot carries `FLUX_LIGHT_FLAG_CASTS_SHADOW`, `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` and `FLUX_LIGHT_FLAG_VOLUMETRIC`; the moon slot carries `FLUX_LIGHT_FLAG_VOLUMETRIC`, plus `FLUX_LIGHT_FLAG_CASTS_SHADOW` when ENV sets it (above 1 lux); every other bit of either slot's `m_uFlags` is zero. The role is the slot, so no bit is a role and a missing bit never makes a slot invalid: each of the three bits is honoured as on any directional light (a slot without casts-shadow is unshadowed, one without volumetric injects no fog, "Scattering injection" of Volumetrics.md). `ApplyRenderWorld` clears `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` on the moon slot (slot 1, by position) and counts `Flux.Lights.FlagIgnored`, so the contact mask of "Contact shadows" of Shadows.md, which is traced for one direction, is never the moon's |
| each frame | `ApplyRenderWorld` validates each slot with `Flux_ValidateEnvironmentLight` ("Validation"; a slot that is not `FLUX_LIGHT_TYPE_DIRECTIONAL` or fails a refusal rule is inactive for the frame, logged once, counted in `Flux.Lights.Invalid`; a cleared bit leaves it active); an active slot whose bytes changed since the previous frame is staged as `StageLightRecord(0 or 1, Flux_EncodeLightRecord(slot, uShadowIndex))` in step 3 of the table above; a slot that becomes inactive is staged once as the inactive record |
| inactive record | `m_uType = FLUX_LIGHT_TYPE_COUNT` (5), `m_xColourIntensity = 0`, `m_uShadowIndex = 0xFFFFFFFF`: the list build skips it |
| mirror | the two slots are mirror entries like any light (keys, luminous power `+∞`, shadow state), so shadow selection and every tie-break treat them uniformly |
| worlds | with several worlds, only the first section in world-tag order whose environment has `m_bHasSun` (or `m_bHasMoon`) fills the slot; a subsequent world's sun or moon is ignored and counted in `Flux.Lights.DirectionalDropped` (one world lights a frame's sky) |

A full-state extraction (`Flux_RenderWorld::m_bFullState`: the resync after a cancelled iteration and device-loss recovery, `Zenith2/Docs/Design/GRAPH.md → "Shutdown"` row G20, cross-doc item X57) makes step 1 discard and re-allocate every slot except the two reserved ones (RHI's rule with row L30's exception) and step 2 clear the mirror, reset every light's tile-size hysteresis and re-add every light of that frame; the sun and moon slots are restaged from the frame's environment; probe and GI deltas follow the same rule in their managers (`Test_Lighting_FullStateKeepsReservedSlots`).

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Lights.h (listed again in "Flux_Lights header")
/// @contract value Flux_CPULight
/// Ownership: An entry of Flux_LightingSystem's CPU light mirror (a Zenith_Array<Flux_CPULight>); public because
///   Flux_ComputeShadowPriority and the tests read it.
/// Thread affinity: OWNER_THREAD, the Flux.BuildGraph node; read-only outside Flux_LightingSystem.
/// Invalidation: A reference is invalid after the next ApplyRenderWorld (the array may grow or erase).
/// Units: Lumens, texels, frames; the delta's units.
/// Errors: None.
/// Version: since Z2 Stage 3. The hysteresis members and m_uLastShadowIndex hold the last SUBMITTED frame's values
///   (Zenith2/Docs/Design/LIGHT.md "Deferred and failed frames").
/// @endcontract
struct Flux_CPULight
{
	Flux_RenderLightDelta m_xDelta;    // the last accepted ADD or UPDATE
	uint32 m_uTableSlot;              // uFLUX_GPU_SCENE_INVALID_SLOT when m_bDropped
	uint32 m_uWorldTag;
	float m_fLuminousPower;            // lumens, recomputed on every update: the shadowed-light priority input
	uint32 m_uShadowTileTexels;       // hysteresis state of Zenith2/Docs/Design/LIGHT/Shadows.md "Atlas tile sizing"
	uint32 m_uShadowTileFramesOutside;
	uint32 m_uShadowFramesUnshadowed; // frames since the light last had a shadow view (reset at 60)
	uint32 m_uLastShadowIndex;        // the shadow index uploaded last frame
	bool m_bChangedThisFrame;
	bool m_bDropped;
};

Zenith_Result<Flux_RenderLightDelta, Flux_LightingError> Flux_ValidateLightDelta(const Flux_RenderLightDelta& xDelta);
Flux_LightRecord Flux_EncodeLightRecord(const Flux_RenderLightDelta& xDelta, uint32 uShadowIndex);
```

## Light and decal list build

The pass is `Zenith2/Docs/Rendering/PerPass/Common.md → "LightDecalListBuild"` (budget `Z2-B-LIGHT-001`, queue G). It runs once per frame over the union of every non-shadow view's frustum.

| Buffer | Layout (little-endian `u32` words) |
|---|---|
| `m_xLightList` | word 0 local count N; word 1 directional count D (at most `uFLUX_MAX_DIRECTIONAL_LIGHTS` = 2); words 2–3 directional light-table indices (`0xFFFFFFFF` when absent); words 4 … 4 + N − 1 local light-table indices in ascending order |
| `m_xDecalList` | word 0 count M; words 1 … M decal-record indices in ascending record order (the order of `Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application"`) |

| Step | Rule |
|---|---|
| inputs | the light table, the frame's decal records and `m_xViewFrusta`, the frame-arena array of the non-shadow views' frustum planes that `Flux_AddLightDecalList` declares at setup and writes at order 5 (`Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`) |
| dispatch | two groups of 1,024 threads: group 0 lights, group 1 decals; thread t handles records t, t + 1,024, t + 2,048 and t + 3,072 (the light table's initial capacity is 4,096 desktop and 1,024 mobile, [Z2-N-REND-537]; the frame decal capacity is `uFLUX_MAX_FRAME_DECALS` = 4,096); a light table grown beyond 4,096 records adds one pass iteration per further 4,096 |
| light test | directional lights always pass; point, spot, rect and disc lights pass when their bounding volume intersects at least one frustum of `m_xViewFrusta`: the range sphere for points; the cone's bounding sphere (Wronski 2016) for spots; the range sphere clipped to the emitting half-space for one-sided rect and disc lights, the full sphere for two-sided ones |
| decal test | the decal box's bounding sphere against the frusta, then the box against each frustum's planes (p-vertex test) |
| compaction | each thread writes a 0/1 flag per record into groupshared memory; a groupshared exclusive prefix sum over the 4,096 flags (four 1,024-element blocks) gives each passing record its output position, so the output order is the ascending index order on every GPU and backend |
| directional order | the passing directional lights are ordered: the sun slot (light-table slot 0) when active, then the moon slot (slot 1) when active, then the scene's directional lights by the luminance of `m_xColourIntensity` descending, ties by lower index; words 2–3 take the first two, and word 1 carries the roles: bits 0–7 the directional count D, bit 8 set when entry 0 has the sun role, bit 9 set when entry 1 has the moon role. Entry 0 has the sun role when it is the sun slot, or, with no active sun slot and the atmosphere enabled (the plain-data field `uint32 m_uAtmosphereEnabled` of `Flux_LightListParams`, from `Flux_AtmosphereParams::m_bEnabled` of the frame's atmosphere block or LIGHT's default when the block is invalid; X148), when it is the brightest scene directional light (a scene authored without ENV's sun keeps a lit sky); entry 1 has the moon role only when it is the moon slot. "The sun" in every consumer is the entry with the sun role, "the moon" the entry with the moon role |
| directional overflow | lights beyond the first two are dropped this frame and counted in `Flux.Lights.DirectionalDropped`; the sun and moon slots are therefore never dropped by an authored directional light, which is X125's conflict rule |

## Light clustering

The pass is `Zenith2/Docs/Rendering/PerPass/Common.md → "LightClustering"` (budget `Z2-B-LIGHT-003`, queue A). It runs per view on every path except Forward.

### Cluster grid

<!-- elaboration -->
The grid and the per-cluster capacities are fixed per capability tier: Rendering's `LightClustering` table fixes the grid ([Z2-N-REND-800]), so neither is a configuration key. They are compile-time constants in `Flux_Lights.h`, selected by `Flux_CapabilityTier` (the ZenithFlux alias of RHI's `Zenith_RHICapabilityTier`, `FLUX_CAPABILITY_TIER_DESKTOP`/`_MOBILE` aliasing `ZENITH_RHI_CAPABILITY_TIER_DESKTOP`/`_MOBILE`, cross-doc item X73 (applied by core-doc batch 4)).

| Parameter | `FLUX_CAPABILITY_TIER_DESKTOP` | `FLUX_CAPABILITY_TIER_MOBILE` | Constant |
|---|---|---|---|
| grid x × y × z | 16 × 9 × 24 | 8 × 5 × 16 | `uFLUX_CLUSTER_GRID_DESKTOP_X/Y/Z`, `uFLUX_CLUSTER_GRID_MOBILE_X/Y/Z` |
| near of slice 0 | the view's near plane | the view's near plane | — |
| far of the last slice | `min(viewFar, 2,000 m)` | `min(viewFar, 1,000 m)` | `fFLUX_CLUSTER_FAR_DESKTOP_METRES`, `fFLUX_CLUSTER_FAR_MOBILE_METRES` |
| maximum lights per cluster | 128 | 48 | `uFLUX_CLUSTER_MAX_LIGHTS_DESKTOP`, `…_MOBILE` |
| maximum decals per cluster | 32 | 8 | `uFLUX_CLUSTER_MAX_DECALS_DESKTOP`, `…_MOBILE` |
| maximum reflection probes per cluster (Stage 4) | 8 | 4 | `uFLUX_CLUSTER_MAX_PROBES_DESKTOP`, `…_MOBILE` |

`Flux_GetClusterLayout(Flux_CapabilityTier)` returns the column as a `Flux_ClusterLayout` value; RPROD's `render.quality.clusterGrid` key is withdrawn for the same reason (row L28).

- Slice k (0 ≤ k < Z) spans the view depths `[n · (f / n)^(k / Z), n · (f / n)^((k + 1) / Z)]` (exponential); a depth beyond f maps to slice Z − 1, so every visible point has a cluster.
- The tile size in render pixels is `ceil(renderWidth / X) × ceil(renderHeight / Y)`; pixels of the last column or row outside the grid map to the last tile.
- Shader mapping: `slice = clamp(floor(log2(z / n) × Z / log2(f / n)), 0, Z − 1)`, computed in `float`; `Clusters.slang` exports `Flux_ClusterIndex(uint2 xPixel, float fLinearDepth, Flux_ClusterMapping xMap)` and every consumer (opaque, transparent, particles, fog, deferred lighting, SSR) uses it. `Flux_ClusterMapping` (Slang only, built by `Flux_MakeClusterMapping(m_xClusterGrid, m_xClusterTileSize, m_xClusterDepthRange)` at the pass entry) holds X, Y, Z, the tile size and n, f, and every caller's parameter struct carries the three plain-data fields that supply them (the row "every `Flux_ClusterIndex` caller" of `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`): `Zenith_RHIUInt3 m_xClusterGrid` (the column above), `Zenith_RHIUInt2 m_xClusterTileSize` (`ceil(renderExtent / (X, Y))` of the view's render extent in that frame, computed on the CPU) and `Zenith_RHIFloat2 m_xClusterDepthRange` (n = the view's near plane, f = `min(viewFar, fFLUX_CLUSTER_FAR_…_METRES)`), all three written by `Flux_AddLightClustering`'s caller from the same `Flux_ClusterLayout` and view values as the `build` step uses, so a consumer's mapping equals the grid the clusters were built for.

### Cluster list layout

`m_xClusterBounds`: per cluster 32 bytes, `float3 min; float pad; float3 max; float pad` in view space, with cluster index `x + X × (y + Y × z)`.

`m_xClusterLists`, per view:

| Region | Content |
|---|---|
| headers | per cluster 8 bytes: `u32 m_uFirstIndex` (= clusterIndex × C, C = maxLights + maxDecals + maxProbes), `u16 m_uLightCount`, `u8 m_uDecalCount`, `u8 m_uProbeCount` (0 until Stage 4) |
| indices | per cluster C `u16` entries: lights (light-table indices, ascending), then decals (decal-record indices, ascending), then probes (probe-record indices, ascending); unused entries are never read |

A fixed per-cluster capacity makes the layout independent of the scene and of thread scheduling, so the list contents are byte-identical across runs and backends; the memory is 3,456 × (8 + 336) = 1,188,864 bytes desktop and 640 × (8 + 120) = 81,920 bytes mobile per view.

### Build and assignment steps

| Step (`uPERM_STEP`) | Algorithm |
|---|---|
| build | one thread per cluster: the four corners of the cluster's screen-tile rectangle at both slice depths, unprojected with `m_xInvProjection`, give the view-space AABB; an orthographic view unprojects the rectangle directly |
| assign | one group of 64 threads per (x, y) tile, looping over the frame light list in chunks of 64: thread i tests light `chunkBase + i` against the tile's Z cluster AABBs and writes a Z-bit mask to groupshared memory; then, for each slice, a groupshared prefix over the 64 masks appends the passing lights in ascending light-list order until `maxLights`; decals repeat this over the decal list, restricted to slices whose depth range intersects the tile's `[minDepth, maxDepth]` read from `m_xDepthMinMax` at the mip where one texel covers one tile (decals affect opaque surfaces only); probes (Stage 4) repeat it over the frame probe list |
| light–AABB tests | point: sphere–AABB (squared distance to the box ≤ r²); spot: the cone test of Wronski 2016 (apex, axis, angle and range against the AABB's bounding sphere), falling back to the sphere test for outer half-angles above 70°; rect and disc: sphere–AABB, and for one-sided lights the AABB not entirely behind the emitting plane; directional lights are never clustered |
| decal and probe tests | the decal box or probe influence box transformed to view space as an OBB, tested against the cluster AABB with the 15-axis separating-axis test |
| overflow | a cluster whose passing count exceeds its capacity keeps the first entries in ascending index order, which is deterministic, and increments `Flux.Lights.ClusterOverflow` (lights), `Flux.Decals.ClusterOverflow` or `Flux.Probes.ClusterOverflow` through the counter readback of `Zenith2/Docs/Design/LIGHT.md → "Counters"` |

## The EvaluateLighting contract

`EvaluateLighting` is single-source (`Zenith2/Docs/Rendering.md → "Single-source lighting"`, [Z2-N-REND-694]): every path calls the same code, and the only path-specific input is the `Flux_LightSource` implementation. LIGHT owns the modules under `Zenith2/Shaders/Lighting/`.

| Module | Exports |
|---|---|
| `EvaluateLighting.slang` | `Flux_LightingInput`, `Flux_LightingOutput`, `Flux_LightSource`, `Flux_ClusterLightSource`, `Flux_PerObjectLightSource`, `EvaluateLighting`, `EvaluateLightingEx` |
| `BRDF.slang` | the terms of "BRDF", `Flux_BRDFTerms`, `Flux_EvaluateBRDF`, `Flux_SampleDFG` |
| `Clusters.slang` | `Flux_ClusterIndex`, `Flux_ClusterHeader`, cluster-list iteration |
| `Shadows.slang` | `Flux_SampleShadow` per light type (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow sampling in shaders"`) |
| `IBL.slang` | `Flux_EvaluateIBLSpecular`, `Flux_EvaluateIBLDiffuse`, probe blending (`Zenith2/Docs/Design/LIGHT/Reflections.md → "IBL evaluation"`) |
| `Fog.slang` | `Flux_ApplyFroxelFog` (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Fog application"`) |
| `Decals.slang` | clustered and per-object decal application to `Flux_SurfaceData`, implementing `Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application"` verbatim |
| `IndirectDiffuse.slang` (Stage 5) | `Flux_EvaluateIndirectDiffuse` over sky SH, irradiance volumes, lightmaps and SSGI (`Zenith2/Docs/Design/LIGHT/GI.md → "Indirect diffuse selection"`) |
| `AreaLights.slang` (Stage 5) | the LTC evaluation of "Light attenuation and shape" |

<!-- assumption: pending RPROD, X103 -->
`Flux_LightingInput` and `Flux_LightSource` are Rendering's; LIGHT needs the members marked "added" below and `EvaluateLightingEx` beside the specified `EvaluateLighting`. This is item (c) of cross-doc item X103, which core-doc batch 4 applied. The need is row L3 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`; the field list is `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`.
<!-- /assumption -->

```slang
// Zenith2/Shaders/Lighting/EvaluateLighting.slang
struct Flux_LightingInput
{
	float3 m_xPositionWS;
	float3 m_xViewDirWS;               // unit, surface to camera
	float2 m_xScreenUV;
	float m_fLinearDepth;              // view-space metres
	half m_fScreenSpaceAO;             // 1 when SSAO is off
	half m_fContactShadow;             // 1 when contact shadows are off
	uint2 m_xPixel;                    // render-extent pixel
	float3 m_xGeometricNormalWS;       // added: unit interpolated vertex normal (shadow normal offset, clear coat)
	half3 m_xIndirectIrradiance;       // added: irradiance E (lux, not pre-exposed) from Flux_EvaluateIndirectDiffuse; the sky SH when nothing else applies
	uint m_uLightingChannels;          // added: the instance's channel mask ("Lighting channels")
	uint m_uFrameIndex;                // added: the frame index, for noise
	float m_fPreExposure;              // added: this frame's pre-exposure multiplier ("Light units")
};

struct Flux_LightingOutput
{
	half3 m_xRadiance;                 // pre-exposed outgoing radiance: direct + environment specular + indirect diffuse + emissive
	half3 m_xEnvSpecularWeight;        // E_tot(μv) × specular occlusion: the weight SSR composites with
	half3 m_xEnvSpecular;              // the pre-exposed probe or sky specular that SSR replaces where it hits
};

struct Flux_DirectionalLight          // added: one directional entry of the frame light list
{
	Flux_LightRecord m_xRecord;       // m_xLightTable[word 2 + i of m_xLightList]
	uint m_uRoles;                    // bit 0 the sun role, bit 1 the moon role (word 1 bits 8 and 9 of m_xLightList)
};

struct Flux_LightingAtmosphere        // added: what step 3 needs besides the light records
{
	Texture2D<float4> m_xTransmittanceLUT;    // Flux_FrameShared::m_xTransmittanceLUT, 256 × 64
	Texture2D<float> m_xCloudShadowMap;       // the pass's [Optional] m_xCloudShadowMap (slot 0's null substitute when unset)
	Flux_AtmosphereGPU m_xAtmosphere;         // Common/Media.slang: geometry, media, m_uEnabled
	float4 m_xCloudShadowMapping;             // origin x, origin z, 1 / extent (0: no map, factor 1), outside transmittance
};

interface Flux_LightSource
{
	uint GetLightCount(Flux_LightingInput xIn);
	Flux_LightRecord GetLight(Flux_LightingInput xIn, uint uIndex);
	uint GetDirectionalCount(Flux_LightingInput xIn);                          // added: word 1 bits 0–7 of m_xLightList, at most 2
	Flux_DirectionalLight GetDirectional(Flux_LightingInput xIn, uint uIndex); // added: uIndex < GetDirectionalCount
	Flux_LightingAtmosphere GetAtmosphere(Flux_LightingInput xIn);             // added
	uint GetProbeCount(Flux_LightingInput xIn);                                // added: Stage 4 reflection probes
	Flux_ReflectionProbeRecord GetProbe(Flux_LightingInput xIn, uint uIndex);  // added
};

Flux_LightingOutput EvaluateLightingEx<TLights : Flux_LightSource>(Flux_SurfaceData xSurface, Flux_LightingInput xIn, TLights xLights);
half3 EvaluateLighting<TLights : Flux_LightSource>(Flux_SurfaceData xSurface, Flux_LightingInput xIn, TLights xLights)
{
	return EvaluateLightingEx(xSurface, xIn, xLights).m_xRadiance;
}
```

### Evaluation order

`EvaluateLightingEx` performs exactly these steps, in this order, on every path:

| Step | Operation |
|---|---|
| 1 | derive `Flux_BRDFTerms` from `Flux_SurfaceData` ("BRDF"): diffuse albedo `albedo × (1 − metallic)`, `F0 = lerp(0.16 × specular², albedo, metallic)`, `α = max(roughness², 0.002)` |
| 2 | `FLUX_SHADING_MODEL_UNLIT`: return `emissive × fPreExposure` with zero weights, and stop |
| 3 | `xAtm = xLights.GetAtmosphere(xIn)`; for each `i < xLights.GetDirectionalCount(xIn)` (at most 2, in the directional order of "Light and decal list build"), `xDir = xLights.GetDirectional(xIn, i)`: skip on a channel mismatch; the illuminance `xDir.m_xRecord.m_xColourIntensity` × `Flux_SampleSunTransmittance(xAtm, xIn.m_xPositionWS, −xDir.m_xRecord.m_xDirectionWS)` when `xDir.m_uRoles ≠ 0` and `xAtm.m_xAtmosphere.m_uEnabled` (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sun transmittance"`; 1 otherwise) × `Flux_SampleShadow` × the contact shadow (only for i = c, the first directional entry in frame-list order whose record carries `FLUX_LIGHT_FLAG_CONTACT_SHADOWS`, which is the entry `ContactShadows` traced its mask for, `Zenith2/Docs/Design/LIGHT/Shadows.md → "Contact shadows"`; every other entry, including a second one carrying the bit, takes 1) × the cloud shadow (bit 0 of `m_uRoles` only: `Flux_SampleCloudShadow(xAtm, xIn.m_xPositionWS)` of `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Cloud shadow on the sun"`, 1 when `xAtm.m_xCloudShadowMapping.z` is 0) × the BRDF |
| 4 | for each light `GetLight(i)`, `i < GetLightCount()`, in order: skip a record of `FLUX_LIGHT_TYPE_DIRECTIONAL` (step 3 evaluated every directional light from the frame list; the clusters hold none, and Rendering's per-object lists keep the directional light, `Zenith2/Docs/Rendering/PerPass/Forward.md → "PerObjectLightLists"`, so this skip is what prevents counting it twice on Forward; a kept directional record still occupies one of the object's per-object slots, whose cap is `Z2-B-RPROD-013` (pending, proposed 8), so Forward evaluates at most cap − 1 local lights per object, cap − 2 when the frame list holds two directional entries); skip on a channel mismatch; shape and attenuation ("Light attenuation and shape"); the shadow when `m_uShadowIndex ≠ 0xFFFFFFFF`; the BRDF |
| 5 | environment specular: the probe and sky blend of `Zenith2/Docs/Design/LIGHT/Reflections.md → "IBL evaluation"`, times `E_tot(μv)` and the specular occlusion |
| 6 | indirect diffuse: `m_xIndirectIrradiance / π × diffuseAlbedo × (1 − E_tot(μv)) × multiBounceAO(ao)`, where `ao = min(xSurface.m_fAO, xIn.m_fScreenSpaceAO)` |
| 7 | add `emissive`, then multiply the sum by `fPreExposure` |
| 8 | fill `m_xEnvSpecularWeight` and `m_xEnvSpecular` (pre-exposed) |

- Direct lighting is never multiplied by ambient occlusion; contact shadows and shadow maps are its occlusion terms.
- Accumulation across lights is `float`; the per-light BRDF products are `half` except the terms "BRDF" keeps in `float` ([Z2-N-REND-922]).
- Froxel fog is applied by the caller after `EvaluateLighting` (`Flux_ApplyFroxelFog`), never inside it, so transparent and particle passes share the same fog code.
- The function is pure: no UAV, no groupshared, no discard, no derivative instruction (gradients live in `Flux_MaterialInput`), so it is callable from pixel and compute stages alike.

### The two light sources

| Implementation | Paths | Enumeration |
|---|---|---|
| `Flux_ClusterLightSource` | Forward+, Deferred, visibility buffer, transparent forward with `bPERM_CLUSTERED` | the cluster of `Flux_ClusterIndex(xIn.m_xPixel, xIn.m_fLinearDepth, xMap)`, `xMap` from the pass's cluster fields ("Cluster grid"): `GetLightCount` returns the header's light count and `GetLight(i)` loads the light record at the i-th index; probes likewise from the probe entries |
| `Flux_PerObjectLightSource` | Forward | the visible instance's per-object list of `Zenith2/Docs/Rendering/PerPass/Forward.md → "PerObjectLightLists"`; probes (Stage 4): at most 4 per object, selected by the decal rule (projected overlap) into the same record |

Both implementations are Slang structs whose members are the calling pass's fields, built at the pass's entry by `Flux_MakeClusterLightSource(…)` or `Flux_MakePerObjectLightSource(…)` in `EvaluateLighting.slang` from the parameter struct: the cluster lists or per-object lists, `m_xLightTable`, `m_xLightList`, `m_xProbeRecords`, the cluster grid, and a `Flux_LightingAtmosphere` filled from `m_xTransmittanceLUT`, `m_xCloudShadowMap`, `m_xAtmosphere` and `m_xCloudShadowMapping` (the rows "every lighting pass" of `Zenith2/Docs/Design/LIGHT.md → "Rendering table additions pending X103"`, X148). The directional part is path-independent: both implement `GetDirectionalCount` as `(m_xLightList.Load(4) & 0xFF)`, `GetDirectional(i)` as the record at `m_xLightList.Load(8 + 4 i)` with roles `(m_xLightList.Load(4) >> (8 + i)) & 1` placed in bit i (entry 0 can only hold the sun role, entry 1 only the moon role), and `GetAtmosphere` as the stored member. So `EvaluateLightingEx` stays a pure function of its three arguments, and `EvaluateLighting` keeps Rendering's three-argument signature.

The constructors, declared by the Stage 2 stub slice and never changed by the implementation (each light-source struct stores its arguments as members of the same name with `m_` and the type prefix):

```slang
// Zenith2/Shaders/Lighting/EvaluateLighting.slang
Flux_LightingAtmosphere Flux_MakeLightingAtmosphere(Texture2D<float4> xTransmittanceLUT, Texture2D<float> xCloudShadowMap,
	Flux_AtmosphereGPU xAtmosphere, float4 xCloudShadowMapping);
Flux_ClusterLightSource Flux_MakeClusterLightSource(ByteAddressBuffer xClusterLists, ByteAddressBuffer xLightTable,
	ByteAddressBuffer xLightList, StructuredBuffer<Flux_ReflectionProbeRecord> xProbeRecords, uint uProbeRecordBase,
	uint3 xClusterGrid, uint2 xClusterTileSize, float2 xClusterDepthRange, Flux_LightingAtmosphere xAtmosphere);
Flux_PerObjectLightSource Flux_MakePerObjectLightSource(ByteAddressBuffer xPerObjectLights, uint uVisibleIndex,
	ByteAddressBuffer xLightTable, ByteAddressBuffer xLightList, StructuredBuffer<Flux_ReflectionProbeRecord> xProbeRecords,
	uint uProbeRecordBase, Flux_LightingAtmosphere xAtmosphere);
```

## BRDF

<!-- elaboration -->
The BRDF is energy-conserving and reciprocal by construction, so the analytical material tests of `Zenith2/Docs/Rendering.md → "Independent analytical material tests"` hold exactly up to LUT quantisation. The coupled diffuse term is an elaboration within LIGHT's authority over the light model (design choice LR3 of `Zenith2/Docs/Design/LIGHT.md → "Design choices this family makes"`), not a ruling: the specification fixes single-source lighting ([Z2-N-REND-694]) and Rendering's analytical tests ([Z2-N-REND-949], [Z2-N-REND-950]), not the diffuse model, and "Closed-form reductions" shows how those tests are met.

| Term | Definition |
|---|---|
| normal distribution | `D(h) = α² / (π ((n·h)² (α² − 1) + 1)²)` (GGX) |
| visibility | `V(l, v) = 0.5 / (n·l √((n·v)² (1 − α²) + α²) + n·v √((n·l)² (1 − α²) + α²))` (height-correlated Smith, Heitz 2014) |
| Fresnel | `F(v·h) = F0 + (F90 − F0)(1 − v·h)⁵`, `F90 = saturate(50 × dot(F0, 1/3))` |
| single-scatter specular | `f_ss = D · V · F` |
| directional albedo tables | `A(μ, α)` and `B(μ, α)` with `∫ f_ss cos θ_l dl = F0·A(μ) + F90·B(μ)`, μ = n·v; their cosine-weighted averages `Aavg(α) = 2∫A μ dμ` and `Bavg(α) = 2∫B μ dμ` |
| white-albedo energy | `E_w(μ) = A(μ) + B(μ)`, `E_wavg = Aavg + Bavg` |
| multi-scatter specular (Kulla–Conty) | `F_avg = F0 + (F90 − F0)/21` (the cosine-weighted hemispherical average `2∫F(μ) μ dμ` of the Schlick term with the same F90, linear in F0 and F90); `F_ms = F_avg² · E_wavg / (1 − F_avg (1 − E_wavg))`; `f_ms(l, v) = saturate(1 − E_w(μl)) · saturate(1 − E_w(μv)) / (π · max(1 − E_wavg, 1e-4)) · F_ms` |
| total specular directional albedo | `E_tot(μ) = F0·A(μ) + F90·B(μ) + (1 − E_w(μ))·F_ms`; `E_totavg = F0·Aavg + F90·Bavg + (1 − E_wavg)·F_ms` (exact, because every term is linear in the averages) |
| diffuse | `f_d(l, v) = diffuseAlbedo · saturate(1 − E_tot(μl)) · saturate(1 − E_tot(μv)) / (π · max(1 − E_totavg, 1e-4))`, and `f_d = 0` exactly when `diffuseAlbedo = 0` (every channel) |
| full BRDF | `f = f_ss + f_ms + f_d` |

- White furnace: for any albedo ≤ 1, `∫ f cos = E_tot(μv) + diffuseAlbedo · (1 − E_tot(μv)) ≤ 1`, with equality for a white dielectric.
- Energy limits: the two denominators vanish together with their numerators. With F0 = F90 = 1 (a white metal: metallic 1, albedo 1) `F_avg = 1`, `F_ms = E_wavg / (1 − (1 − E_wavg)) = 1`, `E_totavg = E_wavg + (1 − E_wavg) × 1 = 1`, so the unguarded diffuse term is 0/0; its `diffuseAlbedo` is 0 there, which the exact-zero rule returns. At the α floor `E_wavg` approaches 1, so `f_ms` is 0/0 in the limit, and table quantisation can push `1 − E_wavg` or `1 − E_totavg` to or below 0; the `max(…, 1e-4)` denominators and `saturate` numerators keep every term finite and non-negative, and the error they introduce is below the `1e-3` furnace bound. The CPU reference BRDF applies the same guards in the same places, and `Test_BRDF_WhiteFurnace`, `Test_BRDF_Reciprocity` and `Test_BRDF_MultiScatterMetalEnergy` include F0 = 1 and α = 0.002 with a finite-output assertion.
- Reciprocity: every term is symmetric in l and v.
- IBL uses the same `E_tot(μv)` for its specular weight and the same `(1 − E_tot(μv))` for its diffuse weight, so the IBL furnace (a uniform environment of radiance 1 around a white surface) returns 1 up to LUT and prefilter quantisation.
- Precision: `n·h`, `D`, `V` and `α²` are `float`, because `α² = 4e-6` at the α floor underflows half precision; `F`, colours, the albedo tables and the products with light colour are `half`.
- `α` is clamped to at least 0.002 (perceptual roughness 0.045) for analytic lights, so a perfectly smooth surface reflects a punctual light as a finite highlight.
- Horizon: `n·l` and `n·v` are clamped to at least 1e-4 before the visibility term; back-facing lights (`n·l ≤ 0`) contribute nothing except through the subsurface lobe of "Advanced surface lobe integration".
- Multi-bounce AO (Jimenez 2016): per channel of the diffuse albedo ρ, `a = 2.0404 ρ − 0.3324`, `b = −4.7951 ρ + 0.6417`, `c = 2.7552 ρ + 0.6903`, `ao' = max(ao, ((ao · a + b) · ao + c) · ao)`.
- Specular occlusion (Lagarde and de Rousiers 2014): `so = saturate((n·v + ao)^(exp2(−16 × roughness − 1)) − 1 + ao)`, applied to the probe and sky specular only, never to SSR hits.

### Closed-form reductions

Rendering's named analytical tests ([Z2-N-REND-950]) run against this BRDF through the independent CPU reference of `Zenith2/Engine/ZenithFlux/Tests/ReferenceBRDF/` and the shader harness `Zenith2/Shaders/Tests/Lighting/BRDFHarness.slang`. Each closed-form case is a configuration in which the full BRDF reduces exactly to the textbook expression:

| Test | Configuration | Reduction | Bound |
|---|---|---|---|
| `Test_BRDF_LambertClosedForm` | `specular = 0`, `metallic = 0`, so F0 = 0; any roughness; albedo ρ ∈ {0.18, 0.5, 1} | F90 = `saturate(50 × dot(0, 1/3))` = 0, so F ≡ 0 and `f_ss = D·V·F = 0`; `F_avg = 0 + (0 − 0)/21 = 0`, so `F_ms = 0` and `f_ms = 0`; `E_tot(μ) = 0·A + 0·B + (1 − E_w)·0 = 0` for every μ and `E_totavg = 0`; hence `f_d = ρ · (1 − 0)(1 − 0) / (π (1 − 0)) = ρ / π` and `f = ρ / π` exactly, independent of l, v and roughness | the reference and the shader harness each return `ρ / π` within 1e-6 absolute at the 16 × 16 fixed (μl, μv) sample points and 3 azimuths (float evaluation; the DFG table is not sampled when F0 = F90 = 0, because both table terms are multiplied by 0) |
| `Test_BRDF_GGXNormalisation` | the D term alone | `∫_Ω D(h) (n·h) dω_h = 1` for every α | 1 ± 1e-3 at α ∈ {0.002, 0.05, 0.25, 0.5, 1} by 2²⁰-sample stratified integration in `double` |
| `Test_BRDF_WhiteFurnace` | ρ = 1, `metallic = 0`, `specular = 0.5` (F0 = 0.04) | `∫ f cos = E_tot(μv) + (1 − E_tot(μv)) = 1` | "Analytical BRDF tests" of `Zenith2/Docs/Design/LIGHT/Tests.md` |
| `Test_BRDF_Reciprocity` | any | every term symmetric in l and v | as that table |

A diffuse-only surface on the Forward, Forward+, Deferred and visibility-buffer paths therefore renders exactly the Lambertian image the path tracer's Lambert material produces, which is what `PT_AnalyticPoint` compares.

## Light attenuation and shape

| Light | Illuminance at a surface point p with normal n |
|---|---|
| directional | `E = lux × saturate(n·l)`, l = −direction; the sun disc widens the specular lobe as a sphere light whose `r / d` is the angular radius |
| point | `E = I × W(d) × saturate(n·l) / max(d², 1e-4)`, `W(d) = saturate(1 − (d / range)⁴)²` (windowed inverse square, Karis 2013) |
| spot | the point formula × `S(cos θ)`, `S = t²`, `t = saturate((cos θ − cosOuter) / (cosInner − cosOuter))` |
| sphere lights (`m_fSourceRadius` r > 0) | diffuse: the sphere form factor with horizon handling of Lagarde and de Rousiers 2014 (`illuminanceSphereOrDisk`); specular: the representative-point direction toward the point of the sphere closest to the reflection ray, `α' = saturate(α + r / (2d))`, and D scaled by `(α / α')²` (Karis 2013) |
| rect (Stage 5) | linearly transformed cosines (Heitz et al. 2016) over the 4-edge polygon clipped to the horizon, with the cooked LTC tables of "Lighting lookup tables"; diffuse is the LTC with the identity matrix; the result is multiplied by `W(d)` with d the distance to the rectangle's centre |
| disc (Stage 5) | the rect method over an 8-edge regular polygon inscribed in the disc, with the luminance multiplied by `π r² / A₈`, `A₈ = 2√2 r²`, so the emitted power is the disc's; barn cones multiply by `S(cos θ)` measured from the disc centre |

Shadows of sphere and area lights use their shadow view's filtered comparison with the penumbra estimate of `Zenith2/Docs/Design/LIGHT/Shadows.md → "Filtering"`; the path-tracer comparison scenes measure the error of that approximation (`Zenith2/Docs/Design/LIGHT/Tests.md → "Path-tracer comparison scenes"`).

## Lighting channels

<!-- assumption: pending RPROD, X103 -->
The instance record's `m_uLayerMask` (`Zenith2/Docs/Rendering/GpuResources.md → "Instance record (128 bytes)"`) carries "render layers and lighting channels" without a bit assignment. LIGHT assumes lighting channels occupy bits 24–31 (8 channels; default channel 0, so bit 24 set) and render layers bits 0–23 (design choice LR4); the need is row L4 and item (d) of X103 (applied by core-doc batch 4).
<!-- /assumption -->

- A light affects a surface when `((lightFlags >> 8) & 0xFF) & ((layerMask >> 24) & 0xFF)` is non-zero; directional lights obey the same rule.
- Transparent, particle and fog passes use the channel mask of their instance (particles: the emitter's instance); the froxel volume injects every light whose mask contains channel 0.
- The channel mask never changes shadow casting: an object casts shadows for every shadowed light regardless of channels.

## Advanced surface lobe integration

GEOM defines the Stage 5 lobes and their `Flux_SurfaceData` fields (`Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"`); LIGHT integrates them into `EvaluateLightingEx` without a new program permutation (the model is a link-time property of the material type on the forward paths and a per-pixel branch on the model bits in deferred lighting).

| Model | Integration in steps 3–6 of "Evaluation order" |
|---|---|
| clear coat | per light: base terms × `(1 − F_c(v·h) × clearCoat)` plus the coat's GGX lobe (F0 0.04, the coat roughness, the geometric normal `m_xGeometricNormalWS`); environment: the coat's IBL specular with its own `E` from the DFG table, the base environment terms attenuated by `(1 − F_c(n·v) × clearCoat)` |
| anisotropy | GGX with GEOM's `α_t`, `α_b` and the anisotropic height-correlated visibility; the IBL uses the bent reflection vector of McAuley 2015 (the normal bent toward the anisotropy direction by `|a| × (1 − roughness)`) |
| sheen (cloth) | the Charlie distribution and Neubelt–Pettineo visibility; base diffuse and specular scaled by `1 − max(sheenColour) × E_sheen(n·v)` from the sheen albedo table of "Lighting lookup tables"; IBL sheen = prefiltered radiance at roughness `sheenRoughness` × `sheenColour × E_sheen(n·v)` |
| subsurface | GEOM's wrapped diffuse and thickness-attenuated back transmission per light; the back term takes the light's shadow sample at the surface point |

Each model passes the white-furnace and reciprocity tests of the CPU reference BRDF (`Zenith2/Docs/Design/LIGHT/Tests.md → "Analytical BRDF tests"`), with the reference written independently of `BRDF.slang` (GEOM cross-area row GM9).

## Deferred lighting pass

The pass is `Zenith2/Docs/Rendering/PerPass/Common.md → "DeferredLighting"` (budgets `Z2-B-LIGHT-008` on Deferred and `Z2-B-LIGHT-009` on the visibility buffer, queue G, Stage 5).

| Step | Rule |
|---|---|
| threads | one per render pixel in 8 × 8 groups; a pixel whose depth equals the view's far clear value (`Flux_DepthModeOps`) writes zero and stops (the sky pass draws those texels) |
| G-buffer decode | GB0 albedo (sRGB view, hardware decode) and material AO; GB1 octahedral world normal (`Octahedral.slang`); GB2 roughness, metallic, specular, shading-model byte; GB3 emissive; GB4 (Stage 5, when a visible material uses an extended model, GEOM row GM9) the extended parameters; the result is the same `Flux_SurfaceData` the forward paths hold in registers ([Z2-N-REND-244]) |
| lighting input | position from depth and the inverse view-projection; the geometric normal equals the shading normal (the G-buffer stores no separate geometric normal); the indirect irradiance from `Flux_EvaluateIndirectDiffuse` without the lightmap source, because the G-buffer carries no UV1 (`Zenith2/Docs/Design/LIGHT/GI.md → "Lightmaps on the deferred paths"`) |
| evaluation | `EvaluateLightingEx<Flux_ClusterLightSource>`, then `Flux_ApplyFroxelFog` when `bPERM_FOG` |
| output | pre-exposed radiance into `m_xSceneColour` (R11G11B10F, `[FullCoverage]`) |

The SSR composite of the Deferred and visibility-buffer paths reconstructs `m_xEnvSpecularWeight` from the G-buffer (F0 and roughness are available there), so deferred lighting writes no specular-weight target; the forward paths write one only when the view's `FLUX_VIEW_FEATURE_SSR` bit is set (`Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR composite"`).

## Lighting lookup tables

Cooked engine content under `Zenith2/Engine/ZenithFlux/Internal/Content/Lighting/`, produced by LIGHT's cook step `Cook_LightingLUTs` (`Zenith2/Docs/Design/LIGHT.md → "Cook steps"`) and loaded at renderer startup as `Flux_PersistentReadOnly` textures.

| LUT | Size and format | Content | Consumers |
|---|---|---|---|
| `DFG` | 128 × 128, `R16G16B16A16_FLOAT` | R `A(μ, r)`, G `B(μ, r)`, B `Aavg(r)`, A `Bavg(r)`; x = μ = n·v, y = perceptual roughness r, both at texel centres `(i + 0.5)/128`; computed by the cook with 4,096 GGX VNDF samples per texel (Heitz 2018) from the CPU reference of "BRDF" | every lighting path (`m_xBRDFLUT` of the opaque and deferred lighting passes) |
| `SheenAlbedo` (Stage 5) | 32 × 32, `R16_FLOAT` | `E_sheen(μ, sheenRoughness)` of GEOM's sheen lobe | sheen integration |
| `LTCMatrix`, `LTCMagnitude` (Stage 5) | 64 × 64, `R16G16B16A16_FLOAT` each | the inverse LTC matrix (4 coefficients) and the magnitude, Fresnel and horizon terms, fitted by the cook against the CPU reference GGX | rect and disc lights |

- The DFG table is sampled with `uFLUX_SAMPLER_LINEAR_CLAMP_NO_MIP` (9) at `uv = (x × 127 + 0.5) / 128` per axis.
- A cook-time check evaluates the DFG table's `E_tot` for F0 ∈ {0, 0.04, 0.5, 1} at every texel and requires `E_tot ≤ 1 + 1e-3`; a failure is `COOK_ERROR_LIGHTING_LUT` naming the texel.

## Flux_Lights header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Lights.h
#pragma once
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithFlux/Flux_LightingErrors.h"
#include "ZenithFlux/Flux_RenderGraph.h"
#include "ZenithFlux/Flux_ShadowAtlas.h"                // Flux_RenderLightKey (the leaf header declares it)
#include "Flux/Shaders/Flux_SharedTypes.generated.h"   // Flux_LightRecord (ZenithShadersGenerated)

/// @contract enum Flux_LightType
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a value outside the enumeration in a delta is FLUX_LIGHTING_ERROR_INVALID_LIGHT at extraction.
/// Version: since Z2 Stage 3; RECT and DISC since Z2 Stage 5. The values are GPU-visible in m_uType and never renumbered.
/// @endcontract
enum Flux_LightType : uint8 { /* the enumerators of "Light extraction and the CPU light mirror" */ };

/// @contract enum Flux_LightDeltaOp
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a value outside the enumeration is FLUX_LIGHTING_ERROR_INVALID_LIGHT (light deltas) or
///   FLUX_LIGHTING_ERROR_INVALID_PROBE (probe deltas, which reuse it).
/// Version: since Z2 Stage 3. Never serialized.
/// @endcontract
enum Flux_LightDeltaOp : uint8 { FLUX_LIGHT_DELTA_OP_ADD = 1, FLUX_LIGHT_DELTA_OP_UPDATE, FLUX_LIGHT_DELTA_OP_REMOVE };

/// @contract enum Flux_LightFlag
/// Ownership: Plain enumeration of the m_uFlags bits of "Light record encodings" (bits 8–31 are fields, not flags).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the validation functions clear or refuse misplaced bits.
/// Version: since Z2 Stage 3. GPU-visible bit positions, never renumbered; bits 5–7 and 24–31 are reserved and name no role.
/// @endcontract
enum Flux_LightFlag : uint32
{
	FLUX_LIGHT_FLAG_CASTS_SHADOW = 1u << 0,      // Rendering's bit
	FLUX_LIGHT_FLAG_CONTACT_SHADOWS = 1u << 1,   // Rendering's bit
	FLUX_LIGHT_FLAG_VOLUMETRIC = 1u << 2,        // Rendering's bit
	FLUX_LIGHT_FLAG_TWO_SIDED = 1u << 3,
	FLUX_LIGHT_FLAG_NO_SPECULAR = 1u << 4,       // bits 5–7 reserved: the sun and moon roles are slot positions
};

/// @contract value Flux_RenderLightDelta
/// Ownership: Written by extractors into render-world storage through Flux_RenderWorldWriter::AddLightDelta.
/// Thread affinity: IMMUTABLE after Render.Seal; written by one extractor partition before it.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: Invalid after the render-world slot holding it is reset (Zenith2/Docs/Rendering.md "Purpose and scope").
/// Units: Metres, radians, the stored intensity unit of "Light units"; colour linear Rec.709.
/// Errors: Validated by Flux_ValidateLightDelta (FLUX_LIGHTING_ERROR_INVALID_LIGHT) in Flux_GPUScene::ApplyDeltas and
///   Flux_LightingSystem::ApplyRenderWorld alike; never asserted.
/// Version: since Z2 Stage 3. A render-world value; never serialized, so the layout may change in any build.
/// @endcontract
struct Flux_RenderLightDelta { /* the members of "The delta record" */ };

// Flux_RenderLightKey: declared in Flux_ShadowAtlas.h with its contract block (Zenith2/Docs/Design/LIGHT/Shadows.md
// "Flux_ShadowAtlas header"), so the leaf header RPROD's view headers include needs nothing from this one.

struct Flux_CPULight { /* the members and contract block of "Sun and moon slots" */ };

/// @contract value Flux_ClusterLayout
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Clusters; metres; entries per cluster.
/// Errors: None.
/// Version: since Z2 Stage 3. The values are the compile-time constants of "Cluster grid".
/// @endcontract
struct Flux_ClusterLayout
{
	uint32 m_uGridX, m_uGridY, m_uGridZ;
	float m_fFarMetres;
	uint32 m_uMaxLights, m_uMaxDecals, m_uMaxProbes;
};

/// @contract functions Flux_LightFunctions
/// Ownership: Pure functions over values, plus feature functions that add passes to the graph they receive.
/// Thread affinity: ANY_THREAD for the conversions; the feature functions run on the Flux.BuildGraph node.
/// Invalidation: None.
/// Units: As "Light units"; angles in radians.
/// Errors: Conversions clamp degenerate input as the table "Degenerate input" states; feature functions add no pass
///   when their feature is disabled and report graph errors through Flux_RenderGraph::Compile.
/// Version: since Z2 Stage 3.
/// @endcontract
Flux_RenderLightKey Flux_MakeEntityLightKey(const Zenith_PersistentID& xEntity, uint32 uWorldTag);
Flux_RenderLightKey Flux_MakeNamedLightKey(Zenith_StringView strName, uint32 uWorldTag);
Zenith_Result<Flux_RenderLightDelta, Flux_LightingError> Flux_ValidateLightDelta(const Flux_RenderLightDelta& xDelta);
Zenith_Result<Flux_RenderLightDelta, Flux_LightingError> Flux_ValidateEnvironmentLight(const Flux_RenderLightDelta& xSlot);   // Stage 3 (Z2-D-131, the Q45 ruling); "Validation"
Flux_LightRecord Flux_EncodeLightRecord(const Flux_RenderLightDelta& xDelta, uint32 uShadowIndex);
float Flux_LumensToCandelaPoint(float fLumens);
float Flux_LumensToCandelaSpot(float fLumens, float fOuterHalfAngleRadians);
float Flux_LumensToNitsRect(float fLumens, float fWidthMetres, float fHeightMetres, bool bTwoSided);
float Flux_LumensToNitsDisc(float fLumens, float fRadiusMetres, bool bTwoSided);
Zenith_Colour Flux_KelvinToLinearRec709(float fKelvin);
float Flux_LightLuminousPower(const Flux_RenderLightDelta& xDelta);   // lumens; +infinity for directional lights
Flux_ClusterLayout Flux_GetClusterLayout(Flux_CapabilityTier eTier);

inline constexpr uint32 uFLUX_MAX_DIRECTIONAL_LIGHTS = 2;
inline constexpr uint32 uFLUX_MAX_FRAME_DECALS = 4096;
inline constexpr uint32 uFLUX_CLUSTER_GRID_DESKTOP_X = 16, uFLUX_CLUSTER_GRID_DESKTOP_Y = 9, uFLUX_CLUSTER_GRID_DESKTOP_Z = 24;
inline constexpr uint32 uFLUX_CLUSTER_GRID_MOBILE_X = 8, uFLUX_CLUSTER_GRID_MOBILE_Y = 5, uFLUX_CLUSTER_GRID_MOBILE_Z = 16;
inline constexpr float fFLUX_CLUSTER_FAR_DESKTOP_METRES = 2000.0f, fFLUX_CLUSTER_FAR_MOBILE_METRES = 1000.0f;
inline constexpr uint32 uFLUX_CLUSTER_MAX_LIGHTS_DESKTOP = 128, uFLUX_CLUSTER_MAX_LIGHTS_MOBILE = 48;
inline constexpr uint32 uFLUX_CLUSTER_MAX_DECALS_DESKTOP = 32, uFLUX_CLUSTER_MAX_DECALS_MOBILE = 8;
inline constexpr uint32 uFLUX_CLUSTER_MAX_PROBES_DESKTOP = 8, uFLUX_CLUSTER_MAX_PROBES_MOBILE = 4;

void Flux_AddLightDecalList(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
Flux_RGBuffer Flux_AddLightClustering(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddDeferredLighting(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
```

The three feature-function signatures are Rendering's (`Zenith2/Docs/Rendering/PerPass/Common.md → "LightDecalListBuild"`, `→ "LightClustering"`, `→ "DeferredLighting"`); LIGHT implements them in `Zenith2/Engine/ZenithFlux/Internal/Passes/LightList/`, `Internal/Passes/LightClustering/` and `Internal/Passes/DeferredLighting/`.
