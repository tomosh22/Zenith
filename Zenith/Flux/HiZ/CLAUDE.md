# Flux_HiZ - Hierarchical Z-Buffer System

## Overview

The Hi-Z (Hierarchical Z-Buffer) system generates a depth pyramid from the main depth buffer. This mip chain enables efficient depth-based operations like screen-space reflections (SSR), occlusion culling, and ambient occlusion.

## Architecture

### Render Order

```
RENDER_ORDER_FOLIAGE
RENDER_ORDER_HIZ_GENERATE    <- Hi-Z mip chain generation (compute)
RENDER_ORDER_SSR_RAYMARCH
RENDER_ORDER_SSR_RESOLVE
RENDER_ORDER_APPLY_LIGHTING
```

Hi-Z runs after all opaque geometry is rendered but before any effects that need depth hierarchy.

### Files

| File | Purpose |
|------|---------|
| [Flux_HiZ.h](Flux_HiZ.h) | Class declaration, constants |
| [Flux_HiZ.cpp](Flux_HiZ.cpp) | Implementation, pipeline setup |
| [../Shaders/HiZ/Flux_HiZ_Generate.comp](../Shaders/HiZ/Flux_HiZ_Generate.comp) | Compute shader for mip generation |

## Implementation Details

### Render Target

- **Format:** `R32_SFLOAT` (single-channel 32-bit float)
- **Size:** Full resolution with complete mip chain
- **Memory Flags:** `MEMORY_FLAGS__UNORDERED_ACCESS | MEMORY_FLAGS__SHADER_READ`

### Per-Mip Views

The system creates individual SRV and UAV for each mip level:

```cpp
static constexpr u_int uHIZ_MAX_MIPS = 12;  // Supports up to 4096x4096

static Flux_ShaderResourceView s_axMipSRVs[uHIZ_MAX_MIPS];   // Read from previous mip
static Flux_UnorderedAccessView_Texture s_axMipUAVs[uHIZ_MAX_MIPS];  // Write to current mip
```

### Mip Generation Algorithm

1. **Mip 0:** Copy depth from main depth buffer
2. **Mips 1-N:** Sample 2x2 from previous mip, write minimum depth

```glsl
// For mip > 0: sample 2x2 from previous mip and take minimum
vec4 xDepths;
xDepths.x = textureLod(g_xInputMip, xUV + vec2(-0.5, -0.5) * xTexelSize, 0).r;
xDepths.y = textureLod(g_xInputMip, xUV + vec2( 0.5, -0.5) * xTexelSize, 0).r;
xDepths.z = textureLod(g_xInputMip, xUV + vec2(-0.5,  0.5) * xTexelSize, 0).r;
xDepths.w = textureLod(g_xInputMip, xUV + vec2( 0.5,  0.5) * xTexelSize, 0).r;

float fMinDepth = min(min(xDepths.x, xDepths.y), min(xDepths.z, xDepths.w));
```

### Barrier Handling

The Vulkan backend automatically handles all barriers between compute dispatches. Each mip dispatch:
1. Reads from previous mip (SRV)
2. Writes to current mip (UAV)

The backend detects these transitions and inserts appropriate `vkCmdPipelineBarrier` calls.

## Public Interface

```cpp
class Flux_HiZ
{
public:
    static void Initialise();
    static void Shutdown();
    static void Reset();

    static void Render(void*);
    static void SubmitRenderTask();
    static void WaitForRenderTask();

    // Accessors for consumers (SSR, SSAO, culling)
    static Flux_ShaderResourceView& GetHiZSRV();           // Full mip chain
    static u_int GetMipCount();
    static bool IsEnabled();
};
```

## Debug Variables

| Path | Type | Description |
|------|------|-------------|
| `Flux/HiZ/Enable` | bool | Enable/disable Hi-Z generation |
| `Flux/HiZ/Textures/Mip0` | texture | Full resolution depth |
| `Flux/HiZ/Textures/Mip2` | texture | Quarter resolution |
| `Flux/HiZ/Textures/Mip4` | texture | 1/16 resolution |

## Usage

### In SSR (Ray Marching)

```glsl
// Sample Hi-Z at current mip level
float fHiZDepth = textureLod(g_xHiZTex, xSampleUV, float(uMipLevel)).r;

// Compare ray depth against Hi-Z
if (fRayDepth > fHiZDepth)
{
    // Ray is below surface at this mip level
    // Refine to finer mip or confirm hit at mip 0
}
```

### In Occlusion Culling

```glsl
// Sample conservative depth for object's screen bounds
float fMinHiZ = textureLod(g_xHiZTex, xObjectCenter, fMipForObjectSize).r;

// If object's nearest depth is behind Hi-Z, it's occluded
if (fObjectNearZ > fMinHiZ)
    discard;  // or skip rendering
```

## Performance Considerations

- Compute shader uses 8x8 workgroups
- Sequential mip dispatch (mip N depends on mip N-1)
- Total dispatches = mip count (typically 10-12 for 1080p/4K)
- GPU memory: ~1.33x main depth buffer (due to mip pyramid)
