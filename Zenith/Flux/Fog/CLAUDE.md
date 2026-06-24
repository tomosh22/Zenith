# Volumetric Fog System

## Overview

Multi-technique volumetric fog rendering system with runtime technique switching via the `Graphics/Fog/Technique` graphics option (`Zenith_GraphicsOptions::Get().m_uVolFogTechnique`).

**All techniques are spatial-only** (no temporal effects, history buffers, or reprojection).

## Files

| File | Purpose |
|------|---------|
| `Flux_Fog.cpp` / `Flux_FogImpl.h` | Orchestrator - technique selection and initialization |
| `Flux_Fog_Shaders.h` | Shader declarations for the whole fog system (`Flux_FogShaders::apxALL`) |
| `Flux_VolumeFog.cpp` / `Flux_VolumeFogImpl.h` | Shared infrastructure - noise textures, froxel grids |
| `Flux_GodRaysFog.cpp` / `Flux_GodRaysFogImpl.h` | Technique 3: Screen-space god rays |
| `Flux_RaymarchFog.cpp` / `Flux_RaymarchFogImpl.h` | Technique 2: Ray marching with procedural noise |
| `Flux_FroxelFog.cpp` / `Flux_FroxelFogImpl.h` | Technique 1: Froxel-based volumetric fog |

## Techniques

| ID | Name | Files | Performance | Description |
|----|------|-------|-------------|-------------|
| 0 | Simple Exponential | Flux_Fog.cpp | <0.5ms | Basic distance-based fog |
| 1 | Froxel-based | Flux_FroxelFog.cpp | 1-3ms | Camera-aligned 3D grid, per-froxel lighting |
| 2 | Ray Marching | Flux_RaymarchFog.cpp | 2-4ms | Per-pixel ray march with 3D noise |
| 3 | God Rays | Flux_GodRaysFog.cpp | <1ms | Screen-space radial blur |

## Debug Variables

### Technique Selection
Technique is a `Zenith_GraphicsOptions` flag (`Zenith_GraphicsOptions::Get().m_uVolFogTechnique`), registered as `Graphics/Fog/Technique` — NOT under the `Render/Volumetric Fog/` debug-variable tree. The Debug Mode below is a genuine debug variable registered by `Flux_Fog.cpp`.
```
Graphics/Fog/Technique              [uint: 0-3]
Render/Volumetric Fog/Debug Mode    [uint: 0-23]
```

### Shared Parameters
Registered by `Flux_VolumeFog.cpp` (shared froxel/raymarch infrastructure):
```
Render/Volumetric Fog/Shared/Colour          [vec4]
Render/Volumetric Fog/Shared/Density         [float]
Render/Volumetric Fog/Shared/Scattering      [float]
Render/Volumetric Fog/Shared/Absorption      [float]
```

Note: the **Simple** technique has its OWN parameters, registered separately by `Flux_Fog.cpp` under `Render/Fog/` (NOT under `Shared/`):
```
Render/Fog/Colour                            [vec4]
Render/Fog/Density                           [float]
Render/Fog/Phase G                           [float]
```

### Technique-Specific
```
Render/Volumetric Fog/Froxel/Debug Slice Index  [uint]
Render/Volumetric Fog/Froxel/Near Z             [float]
Render/Volumetric Fog/Froxel/Far Z              [float]
Render/Volumetric Fog/Froxel/Phase G            [float]
Render/Volumetric Fog/Froxel/Noise Scale        [float]
Render/Volumetric Fog/Froxel/Noise Speed        [float]
Render/Volumetric Fog/Froxel/Height Base        [float]
Render/Volumetric Fog/Froxel/Height Falloff     [float]
Render/Volumetric Fog/Froxel/Shadow Bias        [float]
Render/Volumetric Fog/Froxel/Shadow Cone Radius [float]

Render/Volumetric Fog/Raymarch/Step Count        [uint]
Render/Volumetric Fog/Raymarch/Noise Scale       [float]
Render/Volumetric Fog/Raymarch/Noise Speed       [float]
Render/Volumetric Fog/Raymarch/Max Distance      [float]
Render/Volumetric Fog/Raymarch/Height Falloff    [float]
Render/Volumetric Fog/Raymarch/Phase G           [float]
Render/Volumetric Fog/Raymarch/Shadow Bias       [float]
Render/Volumetric Fog/Raymarch/Shadow Cone Radius [float]

Render/Volumetric Fog/God Rays/Sample Count     [uint]
Render/Volumetric Fog/God Rays/Decay            [float]
Render/Volumetric Fog/God Rays/Exposure         [float]
Render/Volumetric Fog/God Rays/Density          [float]
Render/Volumetric Fog/God Rays/Weight           [float]
```

## Debug Visualization Modes

Set `Render/Volumetric Fog/Debug Mode` to visualize internals:

### Shared (1-2)
| Mode | Visualization |
|------|---------------|
| 1 | 3D noise texture slice |
| 2 | Blue noise texture |

### Froxel (3-8)
| Mode | Visualization |
|------|---------------|
| 3 | Density grid slice |
| 4 | Density max projection |
| 5 | Lighting grid slice |
| 6 | Scattering heat map |
| 7 | Extinction visualization |
| 8 | Shadow sample positions |

### Raymarch (9-12)
| Mode | Visualization |
|------|---------------|
| 9 | Step count heat map |
| 10 | Accumulated density |
| 11 | Noise sample values |
| 12 | Jitter pattern |

### God Rays (13-15)
| Mode | Visualization |
|------|---------------|
| 13 | Light source mask |
| 14 | Occlusion test |
| 15 | Radial sample weights |

## Shaders

Located in `Shaders/Fog/`:

### Includes
Shared shader helpers are Slang modules under `Shaders/Common/` (the old `*.fxh` HLSL headers were ported to `.slang`):
| File | Purpose |
|------|---------|
| `Common/Volumetric.slang` | Beer-Lambert, phase functions, froxel utilities |

### Per-Technique
Each per-stage file is a single `.slang` module (vertex/fragment/compute entry points within one file), not separate `.frag`/`.comp` files:
| File | Pass |
|------|------|
| `Flux_Fog.slang` | Simple exponential fog |
| `Flux_GodRays.slang` | God rays radial blur |
| `Flux_RaymarchFog.slang` | Ray marching application |
| `Flux_FroxelFog_Inject.slang` | Density injection |
| `Flux_FroxelFog_Light.slang` | Lighting accumulation |
| `Flux_FroxelFog_Apply.slang` | Froxel application |

## Pass placement

Volume fog registers **six** passes with the render graph up front — one `Fog_Simple`, three Froxel passes (`Fog_FroxelInject` / `Fog_FroxelLight` / `Fog_FroxelApply`), one `Fog_Raymarch` and one `Fog_GodRays`. `ApplyTechniqueSelectionToGraph` toggles which passes are enabled per frame (only the active technique's passes run). Ordering comes from declared Read/Write dependencies, not from any enum. The Froxel rows below are its three passes:

| Pass | Type | Reads | Writes |
|------|------|-------|--------|
| Density injection | compute | (input parameters only) | froxel density volume |
| Lighting pass | compute | froxel density volume, shadow cascades | froxel lighting and scattering volumes |
| Application pass | graphics | froxel lighting and scattering volumes, scene depth | HDR scene |

The application pass naturally runs after deferred shading writes the HDR scene and before tonemap reads it.

### Game override (generic)

A game that ships its own fog (e.g. DevilsPlayground's `DP_Fog`) disables the engine fog **generically** via the render graph's force-disable overlay — `xGraph.SetOwnerForceDisabled("Fog", true)` — which masks all fog passes by owner (`"Fog"` is this feature's setup-step name) WITHOUT mutating their base enable bits. The fog orchestrator's `ApplyTechniqueSelectionToGraph` keeps setting those base bits per the active technique every frame regardless; the overlay masks them while the override is held, and the engine technique returns intact the moment the game lifts it. There is no longer any fog-specific override flag or `SetExternallyOverridden` API — see `Flux/RenderGraph/CLAUDE.md` (Game render features & generic pass disable).

## Architecture

### Orchestration Pattern

`Flux_FogImpl::ApplyTechniqueSelectionToGraph()` selects the technique each frame by toggling pass enable bits based on `Zenith_GraphicsOptions::Get().m_uVolFogTechnique`:

```cpp
const u_int uTechnique = Zenith_GraphicsOptions::Get().m_uVolFogTechnique;
xGraph.SetEnabled(m_xSimpleFogPass,    uTechnique == 0);
xGraph.SetEnabled(m_xFroxelInjectPass, uTechnique == 1);
xGraph.SetEnabled(m_xFroxelLightPass,  uTechnique == 1);
xGraph.SetEnabled(m_xFroxelApplyPass,  uTechnique == 1);
xGraph.SetEnabled(m_xRaymarchPass,     uTechnique == 2);
xGraph.SetEnabled(m_xGodRaysPass,      uTechnique == 3);
```

Each pass's execute callback (`ExecuteSimpleFog`, `ExecuteFroxelInject`, `ExecuteFroxelLight`, `ExecuteFroxelApply`, `ExecuteRaymarch`, `ExecuteGodRays`) is invoked by the render graph in topological order for whichever passes are enabled.

### Shared Resources

`Flux_VolumeFog` provides:
- 3D noise texture (64^3 FBM built from gradient/Perlin-style noise)
- Blue noise texture (64x64)
- Froxel grids (three 160x90x64 volumes: density, lighting, and scattering)

### Technique Pattern

Most techniques (Raymarch, God Rays) follow the pattern:
```cpp
void Initialise();                      // Create resources
void BuildPipelines();                  // (Re)build pipelines (hot-reload)
void Reset();                           // Clear state on scene change
void Render(Flux_CommandBuffer*);       // Main render function
```

The Froxel technique is the exception: instead of a single `Render()`, it uses a three-pass pattern — `RenderInject()` / `RenderLight()` / `RenderApply()` (`Flux_FroxelFogImpl.h`) — matching its three render-graph passes.

## Adding New Techniques

1. Create `Flux_NewTechnique.cpp` / `Flux_NewTechniqueImpl.h` following existing pattern
2. Add include and initialization call in `Flux_Fog.cpp`
3. Register a pass and toggle its enable bit in `Flux_FogImpl::ApplyTechniqueSelectionToGraph()`
4. Add debug variables in `Initialise()`
5. Add `.slang` shaders to `Shaders/Fog/` and declare them in `Flux_Fog_Shaders.h`
6. Update this documentation

## Key Algorithms

### Beer-Lambert Extinction
`transmittance = exp(-density * distance)`

### Henyey-Greenstein Phase Function
Standard HG phase function with asymmetry parameter `g` controlling forward/backward scattering bias. Defined in `Shaders/Common/Volumetric.slang`.

### Froxel Exponential Depth
`linearDepth = nearZ * pow(farZ / nearZ, sliceNorm)` - exponential depth distribution concentrates slices near camera.

## Performance Notes

- Froxel technique uses 160x90x64 grid by default (adjustable)
- Ray marching defaults to 64 steps (adjustable)
- God rays is cheapest for simple directional light effects

## Important: View Space Convention

**The engine uses +Z forward in view space** (not -Z like OpenGL convention).

When reconstructing linear depth from the depth buffer:
```glsl
vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
vec4 viewPos = inverse(g_xProjMat) * clipPos;
viewPos /= viewPos.w;
float linearDepth = viewPos.z;  // Use positive Z, NOT -viewPos.z
```

For world position reconstruction, use `GetWorldPosFromDepthTex()` from the `Shaders/Common/Frame.slang` module, which handles this correctly.

## Debugging Tips

### Fog Not Visible - Systematic Debugging

1. **Test blending works**: Output constant color (e.g., `vec4(0.2, 0.4, 0.8, 0.5)`)
   - If visible: blending pipeline is correct
   - If not visible: check render target setup, blend state

2. **Visualize fog alpha**: Output `vec4(fogAlpha, 0.0, 0.0, 1.0)`
   - Red = fog accumulating correctly
   - Black = extinction not working (check density, depth reconstruction)

3. **Visualize raw depth**: Output `vec4(depth, depth, depth, 1.0)`
   - Should show scene shapes
   - If solid color: depth buffer not bound correctly

4. **Test linear depth sign**: Output `vec4(linearDepth > 0 ? 1 : 0, 0, linearDepth < 0 ? 1 : 0, 1)`
   - Red = positive Z (correct for this engine)
   - Blue = negative Z (wrong sign, see View Space Convention above)

### 3D Texture Issues

If 3D textures only show valid data in first few Z slices:
- Check staging buffer copy uses correct depth (not hardcoded to 1)
- Verify `m_uDepth` is stored and used in `FlushStagingBuffer()`
- Use debug slice visualization (mode 3) to inspect each Z layer

### Froxel Grid Debugging

- Mode 3: View density at specific Z slice (use Debug Slice Index)
- Mode 4: Max projection shows if any slices have density
- If max projection is black but density should exist: check inject shader
- If single slice works but max is black: only one slice has data (3D texture upload issue)
