# Volumetric Fog System

## Overview

Multi-technique volumetric fog rendering system with runtime technique switching via debug variables.

**All techniques are spatial-only** (no temporal effects, history buffers, or reprojection).

## Files

| File | Purpose |
|------|---------|
| `Flux_Fog.h/.cpp` | Orchestrator - technique selection and initialization |
| `Flux_VolumeFog.h/.cpp` | Shared infrastructure - noise textures, froxel grids |
| `Flux_GodRaysFog.h/.cpp` | Technique 3: Screen-space god rays |
| `Flux_RaymarchFog.h/.cpp` | Technique 2: Ray marching with procedural noise |
| `Flux_FroxelFog.h/.cpp` | Technique 1: Froxel-based volumetric fog |

## Techniques

| ID | Name | Files | Performance | Description |
|----|------|-------|-------------|-------------|
| 0 | Simple Exponential | Flux_Fog.cpp | <0.5ms | Basic distance-based fog |
| 1 | Froxel-based | Flux_FroxelFog.cpp | 1-3ms | Camera-aligned 3D grid, per-froxel lighting |
| 2 | Ray Marching | Flux_RaymarchFog.cpp | 2-4ms | Per-pixel ray march with 3D noise |
| 3 | God Rays | Flux_GodRaysFog.cpp | <1ms | Screen-space radial blur |

## Debug Variables

### Technique Selection
```
Render/Volumetric Fog/Technique     [uint: 0-3]
Render/Volumetric Fog/Debug Mode    [uint: 0-15]
```

### Shared Parameters
```
Render/Volumetric Fog/Shared/Colour          [vec3]
Render/Volumetric Fog/Shared/Density         [float]
Render/Volumetric Fog/Shared/Scattering      [float]
Render/Volumetric Fog/Shared/Absorption      [float]
```

### Technique-Specific
```
Render/Volumetric Fog/Froxel/Debug Slice Index  [uint]
Render/Volumetric Fog/Froxel/Near Z             [float]
Render/Volumetric Fog/Froxel/Far Z              [float]
Render/Volumetric Fog/Froxel/Phase G            [float]

Render/Volumetric Fog/Raymarch/Step Count       [uint]
Render/Volumetric Fog/Raymarch/Noise Scale      [float]
Render/Volumetric Fog/Raymarch/Max Distance     [float]

Render/Volumetric Fog/God Rays/Sample Count     [uint]
Render/Volumetric Fog/God Rays/Decay            [float]
Render/Volumetric Fog/God Rays/Exposure         [float]
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
| File | Purpose |
|------|---------|
| `Flux_VolumetricCommon.fxh` | Beer-Lambert, phase functions, froxel utilities |

### Per-Technique
| File | Pass |
|------|------|
| `Flux_Fog.frag` | Simple exponential fog |
| `Flux_GodRays.frag` | God rays radial blur |
| `Flux_RaymarchFog.frag` | Ray marching application |
| `Flux_FroxelFog_Inject.comp` | Density injection |
| `Flux_FroxelFog_Light.comp` | Lighting accumulation |
| `Flux_FroxelFog_Apply.frag` | Froxel application |

## Render Order

Render orders in `Flux_Enums.h`:

```
RENDER_ORDER_VOLUMEFOG_INJECT    → Density injection (compute)
RENDER_ORDER_VOLUMEFOG_LIGHT     → Lighting pass (compute)
RENDER_ORDER_FOG                 → Application pass (existing)
```

## Architecture

### Orchestration Pattern

`Flux_Fog::Render()` selects technique based on `dbg_uVolFogTechnique`:

```cpp
switch (dbg_uVolFogTechnique)
{
    case 0: RenderSimpleFog(); break;
    case 1: Flux_FroxelFog::Render(); break;
    case 2: Flux_RaymarchFog::Render(); break;
    case 3: Flux_GodRaysFog::Render(); break;
}
```

### Shared Resources

`Flux_VolumeFog` provides:
- 3D noise texture (64^3 Perlin-Worley)
- Blue noise texture (64x64)
- Froxel grid allocation (160x90x64)

### Technique Pattern

Each technique follows the pattern:
```cpp
void Initialise();    // Create resources, pipelines
void Reset();         // Clear state on scene change
void Render(void*);   // Main render function
```

## Adding New Techniques

1. Create `Flux_NewTechnique.h/.cpp` following existing pattern
2. Add include and initialization call in `Flux_Fog.cpp`
3. Add case to switch in `Flux_Fog::Render()`
4. Add debug variables in `Initialise()`
5. Add shaders to `Shaders/Fog/`
6. Update this documentation

## Key Algorithms

### Beer-Lambert Extinction
```glsl
transmittance = exp(-density * distance)
```

### Henyey-Greenstein Phase Function
```glsl
float HenyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
}
```

### Froxel Exponential Depth
```glsl
float linearDepth = nearZ * pow(farZ / nearZ, sliceNorm);
```

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

For world position reconstruction, use `GetWorldPosFromDepthTex()` from `Common.fxh` which handles this correctly.

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
