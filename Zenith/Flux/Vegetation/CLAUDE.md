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
| `Flux_Grass.h` | Class declaration, instance struct, configuration |
| `Flux_Grass.cpp` | Implementation - chunk management, rendering |

## Shaders

| Shader | Location | Purpose |
|--------|----------|---------|
| `Flux_Grass.vert` | `Shaders/Vegetation/` | Vertex shader with wind animation |
| `Flux_Grass.frag` | `Shaders/Vegetation/` | Fragment shader with lighting |
| `Flux_Wind.fxh` | `Shaders/Vegetation/` | Wind animation functions |

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
| `Flux/Grass/Enable` | bool | Enable/disable grass |
| `Flux/Grass/DebugMode` | uint | Debug visualization mode (0-9) |
| `Flux/Grass/DensityScale` | float | Density multiplier (0-5) |
| `Flux/Grass/MaxDistance` | float | Maximum render distance (50-500) |
| `Flux/Grass/WindEnabled` | bool | Enable wind animation |
| `Flux/Grass/WindStrength` | float | Wind intensity (0-5) |
| `Flux/Grass/CullingEnabled` | bool | Enable frustum culling |
| `Flux/Grass/ShowChunkGrid` | bool | Show chunk wireframes |
| `Flux/Grass/FreezeLOD` | bool | Lock LOD selection |
| `Flux/Grass/ForcedLOD` | uint | Forced LOD level (0-3) |

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
| Culled | 100m+ | Not rendered |

Density reduction happens at generation time by skipping blades based on distance to camera.

## Render Order

Grass renders during the foliage pass:

```cpp
RENDER_ORDER_FOLIAGE   // After opaque meshes, before fog
```

This ensures:
- Proper depth testing against terrain
- Grass receives fog/aerial perspective
- Can cast shadows (future)

## Integration Points

**Uses:**
- `Flux_HDR::GetHDRSceneTargetSetupWithDepth()` for depth testing
- `Flux_Graphics::s_xFrameConstantsBuffer` for camera/sun
- Terrain chunk load/unload events for grass generation

**Terrain Integration:**
```cpp
// In terrain loading code
void OnChunkLoaded(const Vector3& xCenter, float fSize)
{
    Flux_Grass::OnTerrainChunkLoaded(xCenter, fSize);
}

void OnChunkUnloaded(const Vector3& xCenter)
{
    Flux_Grass::OnTerrainChunkUnloaded(xCenter);
}
```

## Initialization Order

Grass must be initialized AFTER:
- `Flux_HDR` (renders to HDR target)
- `Flux_Terrain` (uses terrain data for placement)

```cpp
Flux_Terrain::Initialise();
Flux_Grass::Initialise();       // After terrain
```

## Common Operations

### Configure from game code:
```cpp
Flux_Grass::SetDensityScale(1.5f);  // 150% density
Flux_Grass::SetWindStrength(2.0f);   // Strong wind
Flux_Grass::SetWindDirection(Vector2(1.0f, 0.3f));
```

### Get stats:
```cpp
u_int uBlades = Flux_Grass::GetVisibleBladeCount();
float fMB = Flux_Grass::GetBufferUsageMB();
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
