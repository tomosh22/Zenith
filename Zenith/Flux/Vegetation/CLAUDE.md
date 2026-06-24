# Flux Grass & Vegetation Pipeline

## Overview

Procedural grass rendering system with GPU instancing, wind animation, and LOD-based density management. Designed for large outdoor scenes with millions of grass blades rendered efficiently.

## Architecture

```
[Terrain Chunk Loaded]
         |
         v
   Generate Grass
   (procedural placement)
         |
         v
   [Instance Buffer]
   (position, rotation, height, color)
         |
    +----+----+
    |         |
    v         v
 Frustum   LOD
 Culling   Selection
    |         |
    +----+----+
         |
         v
   [GPU Instanced Draw]
         |
         v
   Wind Animation
   (vertex shader)
         |
         v
   [HDR Target with Depth]
```

## Files

| File | Purpose |
|------|---------|
| `Flux_GrassImpl.h` | `Flux_GrassImpl` class declaration, instance struct, configuration (accessed via `g_xEngine.Grass()`) |
| `Flux_Grass.cpp` | Implementation - chunk management, rendering |
| `Flux_Grass_Shaders.h` | Shader program decls owned by the Grass feature |

## Shaders

| Shader | Location | Purpose |
|--------|----------|---------|
| `Flux_Grass.slang` | `Shaders/Vegetation/` | Slang module with vertex + fragment stages and inline wind animation (helpers ported from the old `Flux_Wind.fxh`) |

## Configuration Constants

```cpp
namespace GrassConfig
{
    uBLADES_PER_SQM = 50;              // Density at LOD0
    fLOD0_DISTANCE = 20.0f;            // Full detail
    fLOD1_DISTANCE = 50.0f;            // Reduced density
    fLOD2_DISTANCE = 100.0f;           // Billboard
    fMAX_DISTANCE = 200.0f;            // Culled
    fCHUNK_SIZE = 64.0f;               // Matches terrain
    uMAX_INSTANCES_PER_CHUNK = 65536;  // Per-chunk instance limit
    uMAX_VISIBLE_CHUNKS = 64;          // Simultaneous active chunks
    uMAX_TOTAL_INSTANCES = 2000000;    // 2M blades max
}
```

## Per-Blade Instance Data (32 bytes)

```cpp
struct GrassBladeInstance
{
    Vector3 m_xPosition;     // World position (12 bytes)
    float m_fRotation;       // Y-axis rotation (4 bytes)
    float m_fHeight;         // Blade height (4 bytes)
    float m_fWidth;          // Blade width (4 bytes)
    float m_fBend;           // Initial bend (4 bytes)
    uint m_uColorTint;       // Packed RGBA8 (4 bytes)
};
```

## Debug Variables (via Zenith_DebugVariables)

| Path | Type | Description |
|------|------|-------------|
| `Flux/Grass/DebugMode` | uint | Debug visualization mode (0-9) |
| `Flux/Grass/DensityScale` | float | Density multiplier (0-5) |
| `Flux/Grass/MaxDistance` | float | Maximum render distance (50-500) |
| `Flux/Grass/WindStrength` | float | Wind intensity (0-5) |
| `Flux/Grass/ShowChunkGrid` | bool | Show chunk wireframes |
| `Flux/Grass/FreezeLOD` | bool | Lock LOD selection |
| `Flux/Grass/ForcedLOD` | uint | Forced LOD level (0-3) |

Enable, WindEnabled, and CullingEnabled are controlled via `Zenith_GraphicsOptions` (`m_bGrassEnabled` / `m_bGrassWindEnabled` / `m_bGrassCullingEnabled`), not registered as debug variables.

## Debug Modes

```cpp
GRASS_DEBUG_NONE             // Normal rendering
GRASS_DEBUG_LOD_COLORS       // Green/Yellow/Orange/Red by LOD
GRASS_DEBUG_CHUNK_BOUNDS     // Wireframe chunk boundaries
GRASS_DEBUG_DENSITY_HEAT     // Blue=sparse, Red=dense
GRASS_DEBUG_WIND_VECTORS     // Wind direction arrows
GRASS_DEBUG_CULLING_RESULT   // Green=visible, Red=culled
GRASS_DEBUG_BLADE_NORMALS    // Normal direction arrows
GRASS_DEBUG_HEIGHT_VARIATION // Color by blade height
GRASS_DEBUG_PLACEMENT_MASK   // Terrain grass/rock mask
GRASS_DEBUG_BUFFER_USAGE     // Memory stats overlay
```

## Wind Animation

The wind system uses layered sine waves for natural movement: primary wave (large scale), secondary wave (medium scale), and gust modulation. Waves are based on `dot(worldPos, windDir)` with different frequencies and time offsets. Wind displacement is applied in the vertex shader, scaled by blade height (more movement at tip, none at base).

## LOD System

| LOD | Distance | Behavior |
|-----|----------|----------|
| LOD0 | 0-20m | Full density, full geometry |
| LOD1 | 20-50m | 50% density |
| LOD2 | 50-100m | 25% density, simplified |
| LOD3 | 100-200m | 12.5% density |
| Culled | 200m+ | Not rendered |

Density reduction happens at generation time by skipping blades based on distance to camera.

## Pass placement

Grass renders in a forward `"Grass"` pass that runs **after** DeferredShading (the HDR clear + lighting have already happened), which declares:

- `Reads(scene depth as RESOURCE_ACCESS_READ_DEPTH)` — binds scene depth as a READ-ONLY depth attachment, so blades depth-test against the opaque scene without writing depth
- `Writes(HDR scene target)` — blends lit grass fragments directly into the HDR scene (not the G-buffer)

Topological sort derives this placement from the declared Reads/Writes (no explicit ordering enum). The pass does NOT clear either attachment — both carry live scene contents.

This placement gives:
- Proper depth testing against the already-rendered opaque scene
- Grass is lit/translucent in the forward shader (self-shades via translucency)
- Can cast shadows (future)

## Integration Points

**Uses:**
- `Flux_Graphics::GetHDRSceneTarget()` / scene depth for the forward pass
- `Flux_Graphics::m_xFrameConstantsBuffer` for camera/sun
- Terrain mesh geometry for procedural blade placement

**Terrain Integration:**
Grass generation is triggered by handing the subsystem the terrain mesh geometry. `GenerateFromTerrain` distributes blades across the mesh triangles (sampling the optional painted density map at each centroid / blade):

```cpp
// xTerrainMesh is the baked terrain Flux_MeshGeometry
g_xEngine.Grass().GenerateFromTerrain(xTerrainMesh);
```

**Painted density map** (terrain editor / `GrassDensity.ztxtr`): a `[0,1]` map, row-major over the terrain's world footprint, multiplied into placement density inside `GenerateFromTerrain`:

```cpp
g_xEngine.Grass().SetDensityMap(pfData, uWidth, uHeight, fWorldSize); // data is COPIED; nullptr/0 clears
float fDensity = g_xEngine.Grass().SampleDensityMap(fWorldX, fWorldZ); // bilinear; 1.0 when no map set
```

## Initialization Order

In the **feature-registry order** (`Flux_FeatureRegistry.cpp` `RegisterDefaultFeatures()`), Grass is registered after `Terrain` and `DeferredShading` but **before** `HDR`, so Grass initializes before `Flux_HDR` (init order is dependency-safe beyond "FluxGraphics first" — see [Flux/CLAUDE.md](../CLAUDE.md)).

The load-bearing ordering is the **render-graph declaration order**: the `"Grass"` pass declares after DeferredShading (so it renders over the lit HDR scene once DeferredShading has cleared and filled the HDR target) but before Fog/Particles (so atmosphere composites over the blades). Terrain is registered before Grass because `GenerateFromTerrain` consumes the terrain mesh for placement.

## Common Operations

### Configure from game code:
```cpp
g_xEngine.Grass().SetDensityScale(1.5f);  // 150% density
g_xEngine.Grass().SetWindStrength(2.0f);   // Strong wind
g_xEngine.Grass().SetWindDirection(Vector2(1.0f, 0.3f));
```

### Get stats:
```cpp
u_int uBlades = g_xEngine.Grass().GetVisibleBladeCount();
float fMB = g_xEngine.Grass().GetBufferUsageMB();
```

## Performance Budget

| Metric | Target |
|--------|--------|
| Draw calls | 1 (instanced) |
| Blades rendered | Up to 2M |
| GPU time | ~2.0ms (1080p) |
| VRAM | ~100MB for 2M instances |
| Instance buffer | 64MB (2M * 32 bytes) |

## Grass Blade Mesh

Simple quad with 4 vertices:
```
  (-0.2, 1)---(0.2, 1)   <- Narrow tip
       |   \   |
       |    \  |
       |     \ |
  (-0.5, 0)---(0.5, 0)   <- Wide base
```

Oriented along Y-axis, rotated per-instance.

## Future Work

- GPU-based procedural placement (compute shader)
- Shadow casting support
- Terrain normal alignment
- Multiple grass types/textures
- Interactive bending (player/animals)
- Seasonal color variation
