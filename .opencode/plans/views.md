# Fully Detailed Implementation Plan: Flux_RenderAttachment 2D View Arrays

---

## Overview

This refactoring changes `Flux_RenderAttachment` to store views in 2D arrays `[mip][slice]`, enabling automatic per-mip and per-face view creation. This fixes Flux_IBL cubemap rendering and simplifies Flux_HiZ by removing manual per-mip view creation.

---

## Phase 1: Configuration Constants

### Step 1.1: Update ZenithConfig.h

**File:** `Zenith/Zenith/Core/ZenithConfig.h`

**Location:** In `// ============================================================================ // VULKAN LIMITS // ============================================================================` section, after line 97 (`FLUX_MAX_TARGETS`)

**Add:**
```cpp
static constexpr uint32_t FLUX_MAX_MIPS = 12;     // Maximum mip levels (supports up to 4096x4096)
static constexpr uint32_t FLUX_MAX_SLICES = 6;    // Maximum array layers (cubemap faces)
```

**Location:** At the end of the file, after existing legacy macros (around line 177)

**Add:**
```cpp
#ifndef FLUX_MAX_MIPS
#define FLUX_MAX_MIPS ZenithConfig::FLUX_MAX_MIPS
#endif

#ifndef FLUX_MAX_SLICES
#define FLUX_MAX_SLICES ZenithConfig::FLUX_MAX_SLICES
#endif
```

---

## Phase 2: Core Type Changes

### Step 2.1: Update Flux_RenderAttachment in Flux.h

**File:** `Zenith/Zenith/Flux/Flux.h`

**Original structure (lines ~59-70):**
```cpp
struct Flux_RenderAttachment {
    Flux_SurfaceInfo m_xSurfaceInfo;
    Flux_VRAMHandle m_xVRAMHandle;
    std::string m_strName;
    Flux_ShaderResourceView m_pxSRV;
    Flux_UnorderedAccessView_Texture m_pxUAV;
    Flux_RenderTargetView m_pxRTV;
    Flux_DepthStencilView m_pxDSV;
};
```

**Replace with:**
```cpp
struct Flux_RenderAttachment {
    Flux_SurfaceInfo m_xSurfaceInfo;
    Flux_VRAMHandle m_xVRAMHandle;
    std::string m_strName;

    Flux_ShaderResourceView m_aaxSRVs[ZenithConfig::FLUX_MAX_MIPS][ZenithConfig::FLUX_MAX_SLICES];
    Flux_UnorderedAccessView_Texture m_aaxUAVs[ZenithConfig::FLUX_MAX_MIPS][ZenithConfig::FLUX_MAX_SLICES];
    Flux_RenderTargetView m_aaxRTVs[ZenithConfig::FLUX_MAX_MIPS][ZenithConfig::FLUX_MAX_SLICES];
    Flux_DepthStencilView m_xDSV;

    Flux_ShaderResourceView& SRV(u_int uMip = 0, u_int uSlice = 0);
    const Flux_ShaderResourceView& SRV(u_int uMip = 0, u_int uSlice = 0) const;
    Flux_UnorderedAccessView_Texture& UAV(u_int uMip = 0, u_int uSlice = 0);
    Flux_RenderTargetView& RTV(u_int uMip = 0, u_int uSlice = 0);
    Flux_DepthStencilView& DSV() { return m_xDSV; }
    const Flux_DepthStencilView& DSV() const { return m_xDSV; }
};
```

---

## Phase 3: Memory Manager Helpers

### Step 3.1: Add Declarations to Zenith_Vulkan_MemoryManager.h

**File:** `Zenith/Zenith/Vulkan/Zenith_Vulkan_MemoryManager.h`

**Add after existing view creation declarations (~line 95):**
```cpp
Flux_ShaderResourceView CreateShaderResourceViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel, uint32_t uMipCount = 1);
Flux_UnorderedAccessView_Texture CreateUnorderedAccessViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel);
```

---

### Step 3.2: Implement Helpers in Zenith_Vulkan_MemoryManager.cpp

**File:** `Zenith/Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp`

**Add after `CreateRenderTargetViewForLayer` (~line 836):**

```cpp
Flux_ShaderResourceView Zenith_Vulkan_MemoryManager::CreateShaderResourceViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel, uint32_t uMipCount)
{
    Flux_ShaderResourceView xView;
    xView.m_xVRAMHandle = xVRAMHandle;
    xView.m_uBaseMip = uMipLevel;
    xView.m_uMipCount = uMipCount;

    const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
    Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
    Zenith_Assert(pxVRAM != nullptr, "GetVRAM returned null in CreateShaderResourceViewForSlice");
    if (!pxVRAM) return xView;

    vk::Format xFormat = Zenith_Vulkan::ConvertToVkFormat_Colour(xInfo.m_eFormat);

    vk::ImageViewType eViewType = vk::ImageViewType::e2D;
    if (xInfo.m_eTextureType == TEXTURE_TYPE_CUBE) eViewType = vk::ImageViewType::e2D;

    vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(uMipLevel)
        .setLevelCount(uMipCount)
        .setBaseArrayLayer(uSlice)
        .setLayerCount(1);

    vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
        .setImage(pxVRAM->GetImage())
        .setViewType(eViewType)
        .setFormat(xFormat)
        .setSubresourceRange(xSubresourceRange);

    vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
    xView.m_xImageViewHandle = RegisterImageView(xVkView);
    return xView;
}

Flux_UnorderedAccessView_Texture Zenith_Vulkan_MemoryManager::CreateUnorderedAccessViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel)
{
    Flux_UnorderedAccessView_Texture xView;
    xView.m_xVRAMHandle = xVRAMHandle;
    xView.m_uMipLevel = uMipLevel;

    const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
    Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
    Zenith_Assert(pxVRAM != nullptr, "GetVRAM returned null in CreateUnorderedAccessViewForSlice");
    if (!pxVRAM) return xView;

    vk::Format xFormat = Zenith_Vulkan::ConvertToVkFormat_Colour(xInfo.m_eFormat);

    vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(uMipLevel)
        .setLevelCount(1)
        .setBaseArrayLayer(uSlice)
        .setLayerCount(1);

    vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
        .setImage(pxVRAM->GetImage())
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(xFormat)
        .setSubresourceRange(xSubresourceRange);

    vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
    xView.m_xImageViewHandle = RegisterImageView(xVkView);
    return xView;
}
```

---

## Phase 4: Accessor Implementation

### Step 4.1: Implement Accessors in Flux_RenderTargets.cpp

**File:** `Zenith/Zenith/Flux/Flux_RenderTargets.cpp`

**Add after includes and before `void Flux_RenderAttachmentBuilder::BuildColour`:**

```cpp
Flux_ShaderResourceView& Flux_RenderAttachment::SRV(u_int uMip, u_int uSlice)
{
    Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::SRV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
    Zenith_Assert(uSlice < m_xSurfaceInfo.m_uNumLayers, "Flux_RenderAttachment::SRV: slice index %u out of bounds (num layers = %u)", uSlice, m_xSurfaceInfo.m_uNumLayers);
    return m_aaxSRVs[uMip][uSlice];
}

const Flux_ShaderResourceView& Flux_RenderAttachment::SRV(u_int uMip, u_int uSlice) const
{
    Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::SRV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
    Zenith_Assert(uSlice < m_xSurfaceInfo.m_uNumLayers, "Flux_RenderAttachment::SRV: slice index %u out of bounds (num layers = %u)", uSlice, m_xSurfaceInfo.m_uNumLayers);
    return m_aaxSRVs[uMip][uSlice];
}

Flux_UnorderedAccessView_Texture& Flux_RenderAttachment::UAV(u_int uMip, u_int uSlice)
{
    Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::UAV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
    Zenith_Assert(uSlice < m_xSurfaceInfo.m_uNumLayers, "Flux_RenderAttachment::UAV: slice index %u out of bounds (num layers = %u)", uSlice, m_xSurfaceInfo.m_uNumLayers);
    return m_aaxUAVs[uMip][uSlice];
}

Flux_RenderTargetView& Flux_RenderAttachment::RTV(u_int uMip, u_int uSlice)
{
    Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::RTV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
    Zenith_Assert(uSlice < m_xSurfaceInfo.m_uNumLayers, "Flux_RenderAttachment::RTV: slice index %u out of bounds (num layers = %u)", uSlice, m_xSurfaceInfo.m_uNumLayers);
    return m_aaxRTVs[uMip][uSlice];
}
```

---

## Phase 5: Builder Updates

### Step 5.1: Update BuildColour

**File:** `Zenith/Zenith/Flux/Flux_RenderTargets.cpp`

**BuildColour must loop over all mips and slices, populating every valid slot:**

```cpp
void Flux_RenderAttachmentBuilder::BuildColour(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
    if (xAttachment.m_xVRAMHandle.IsValid())
    {
        Zenith_Vulkan_VRAM* pxOldVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
        Flux_MemoryManager::QueueVRAMDeletion(pxOldVRAM, xAttachment.m_xVRAMHandle,
            xAttachment.m_aaxRTVs[0][0].m_xImageViewHandle, Flux_ImageViewHandle(),
            xAttachment.m_aaxSRVs[0][0].m_xImageViewHandle, xAttachment.m_aaxUAVs[0][0].m_xImageViewHandle);
    }

    Flux_SurfaceInfo xInfo;
    xInfo.m_uWidth = m_uWidth;
    xInfo.m_uHeight = m_uHeight;
    xInfo.m_uDepth = m_uDepth;
    xInfo.m_eFormat = m_eFormat;
    xInfo.m_eTextureType = m_eTextureType;
    xInfo.m_uNumMips = m_uNumMips;
    xInfo.m_uNumLayers = 1;  // 2D textures have 1 layer
    xInfo.m_uMemoryFlags = m_uMemoryFlags;

    xAttachment.m_xVRAMHandle = Flux_MemoryManager::CreateRenderTargetVRAM(xInfo);
    xAttachment.m_xSurfaceInfo = xInfo;
    xAttachment.m_strName = strName;

    // Create views for all requested mips and slices
    for (u_int uMip = 0; uMip < xInfo.m_uNumMips; uMip++)
    {
        for (u_int uSlice = 0; uSlice < xInfo.m_uNumLayers; uSlice++)
        {
            if (m_eTextureType == TEXTURE_TYPE_2D)
            {
                xAttachment.m_aaxRTVs[uMip][uSlice] = Flux_MemoryManager::CreateRenderTargetView(
                    xAttachment.m_xVRAMHandle, xInfo, uSlice);
            }

            if (uMip == 0 && uSlice == 0)
            {
                xAttachment.m_aaxSRVs[uMip][uSlice] = Flux_MemoryManager::CreateShaderResourceView(
                    xAttachment.m_xVRAMHandle, xInfo, uSlice, xInfo.m_uNumMips);
            }
            else
            {
                xAttachment.m_aaxSRVs[uMip][uSlice] = Flux_MemoryManager::CreateShaderResourceView(
                    xAttachment.m_xVRAMHandle, xInfo, uSlice, 1);
            }

            if (m_uMemoryFlags & MEMORY_FLAGS__UNORDERED_ACCESS)
            {
                xAttachment.m_aaxUAVs[uMip][uSlice] = Flux_MemoryManager::CreateUnorderedAccessView(
                    xAttachment.m_xVRAMHandle, xInfo, uSlice);
            }
        }
    }

    {
        Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
        Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: Colour Attachment '%s' VkImage=0x%llx VRAM=%u %ux%u mips=%u",
            strName.c_str(), (unsigned long long)(VkImage)pxVRAM->GetImage(),
            xAttachment.m_xVRAMHandle.AsUInt(), xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uNumMips);
    }
}
```

---

### Step 5.2: Update BuildColourCubemap

**File:** `Zenith/Zenith/Flux/Flux_RenderTargets.cpp`

**BuildColourCubemap must loop over all mips (as requested via m_uNumMips) and all 6 cubemap slices:**

```cpp
void Flux_RenderAttachmentBuilder::BuildColourCubemap(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
    if (xAttachment.m_xVRAMHandle.IsValid())
    {
        Zenith_Vulkan_VRAM* pxOldVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
        Flux_MemoryManager::QueueVRAMDeletion(pxOldVRAM, xAttachment.m_xVRAMHandle,
            xAttachment.m_aaxRTVs[0][0].m_xImageViewHandle, Flux_ImageViewHandle(),
            xAttachment.m_aaxSRVs[0][0].m_xImageViewHandle, xAttachment.m_aaxUAVs[0][0].m_xImageViewHandle);
    }

    Flux_SurfaceInfo xInfo;
    xInfo.m_uWidth = m_uWidth;
    xInfo.m_uHeight = m_uHeight;
    xInfo.m_uDepth = 1;
    xInfo.m_eFormat = m_eFormat;
    xInfo.m_eTextureType = TEXTURE_TYPE_CUBE;
    xInfo.m_uNumMips = m_uNumMips;
    xInfo.m_uNumLayers = 6;  // Cubemap has 6 faces
    xInfo.m_uMemoryFlags = m_uMemoryFlags;

    xAttachment.m_xVRAMHandle = Flux_MemoryManager::CreateRenderTargetVRAM(xInfo);
    xAttachment.m_xSurfaceInfo = xInfo;
    xAttachment.m_strName = strName;

    // Create views for all requested mips and all 6 cubemap faces
    for (u_int uMip = 0; uMip < xInfo.m_uNumMips; uMip++)
    {
        for (u_int uSlice = 0; uSlice < 6; uSlice++)
        {
            xAttachment.m_aaxRTVs[uMip][uSlice] = Zenith_Vulkan_MemoryManager::CreateRenderTargetViewForLayer(
                xAttachment.m_xVRAMHandle, xInfo, uSlice, uMip);
            xAttachment.m_aaxSRVs[uMip][uSlice] = Zenith_Vulkan_MemoryManager::CreateShaderResourceViewForSlice(
                xAttachment.m_xVRAMHandle, xInfo, uSlice, uMip, 1);
        }
    }

    if (m_uMemoryFlags & MEMORY_FLAGS__UNORDERED_ACCESS)
    {
        xAttachment.m_aaxUAVs[0][0] = Flux_MemoryManager::CreateUnorderedAccessView(xAttachment.m_xVRAMHandle, xInfo, 0);
    }

    {
        Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
        Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: Cubemap Attachment '%s' VkImage=0x%llx VRAM=%u %ux%u mips=%u layers=6",
            strName.c_str(), (unsigned long long)(VkImage)pxVRAM->GetImage(),
            xAttachment.m_xVRAMHandle.AsUInt(), xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uNumMips);
    }
}
```

---

### Step 5.3: Update BuildDepthStencil

**File:** `Zenith/Zenith/Flux/Flux_RenderTargets.cpp`

**BuildDepthStencil populates only the single DSV (not an array):**

```cpp
void Flux_RenderAttachmentBuilder::BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
    if (xAttachment.m_xVRAMHandle.IsValid())
    {
        Zenith_Vulkan_VRAM* pxOldVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
        Flux_MemoryManager::QueueVRAMDeletion(pxOldVRAM, xAttachment.m_xVRAMHandle,
            Flux_ImageViewHandle(), xAttachment.m_xDSV.m_xImageViewHandle,
            xAttachment.m_aaxSRVs[0][0].m_xImageViewHandle, Flux_ImageViewHandle());
    }

    Flux_SurfaceInfo xInfo;
    xInfo.m_uWidth = m_uWidth;
    xInfo.m_uHeight = m_uHeight;
    xInfo.m_eFormat = m_eFormat;
    xInfo.m_uNumMips = 1;
    xInfo.m_uNumLayers = 1;
    xInfo.m_uMemoryFlags = m_uMemoryFlags;

    xAttachment.m_xVRAMHandle = Flux_MemoryManager::CreateRenderTargetVRAM(xInfo);
    xAttachment.m_xSurfaceInfo = xInfo;
    xAttachment.m_strName = strName;

    xAttachment.m_xDSV = Flux_MemoryManager::CreateDepthStencilView(xAttachment.m_xVRAMHandle, xInfo, 0);
    xAttachment.m_aaxSRVs[0][0] = Flux_MemoryManager::CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, 0, xInfo.m_uNumMips);

    {
        Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
        Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: DepthStencil Attachment '%s' VkImage=0x%llx VRAM=%u %ux%u",
            strName.c_str(), (unsigned long long)(VkImage)pxVRAM->GetImage(),
            xAttachment.m_xVRAMHandle.AsUInt(), xInfo.m_uWidth, xInfo.m_uHeight);
    }
}
```

---

## Phase 6: Flux_HiZ Updates

### Step 6.1: Update Flux_HiZ.h

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.h`

**Remove lines 28-29 (static view arrays):**
```cpp
// REMOVE:
// static Flux_ShaderResourceView s_axMipSRVs[uHIZ_MAX_MIPS];
// static Flux_UnorderedAccessView_Texture s_axMipUAVs[uHIZ_MAX_MIPS];
```

**Update accessor signatures to return references:**
```cpp
static Flux_ShaderResourceView& GetMipSRV(u_int uMip);
static Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip);
```

---

### Step 6.2: Update Flux_HiZ.cpp - Static Definitions

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.cpp`

**Remove lines 15-16 (static definitions):**
```cpp
// REMOVE:
// Flux_ShaderResourceView Flux_HiZ::s_axMipSRVs[uHIZ_MAX_MIPS];
// Flux_UnorderedAccessView_Texture Flux_HiZ::s_axMipUAVs[uHIZ_MAX_MIPS];
```

---

### Step 6.3: Update Flux_HiZ.cpp - Add Assertion

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.cpp`

**In `Initialise()`, after line 49 (after `s_uMipCount` calculation):**
```cpp
Zenith_Assert(ZenithConfig::FLUX_MAX_MIPS >= uHIZ_MAX_MIPS,
    "FLUX_MAX_MIPS (%d) must be >= HiZ_MAX_MIPS (%d)",
    ZenithConfig::FLUX_MAX_MIPS, uHIZ_MAX_MIPS);
```

---

### Step 6.4: Update Flux_HiZ.cpp - Remove Manual View Creation

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.cpp`

**In `CreateHiZRenderTargets()`, remove the for-loop that manually creates per-mip SRVs and UAVs. The `BuildColour()` call now populates the arrays automatically via the new looping logic.**

---

### Step 6.5: Update Flux_HiZ.cpp - Update GetMipSRV/GetMipUAV

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.cpp`

**Update accessor implementations:**
```cpp
Flux_ShaderResourceView& Flux_HiZ::GetMipSRV(u_int uMip)
{
    Zenith_Assert(uMip < s_uMipCount, "Mip level %u out of range (max %u)", uMip, s_uMipCount);
    return s_xHiZBuffer.SRV(uMip, 0);
}

Flux_UnorderedAccessView_Texture& Flux_HiZ::GetMipUAV(u_int uMip)
{
    Zenith_Assert(uMip < s_uMipCount, "Mip level %u out of range (max %u)", uMip, s_uMipCount);
    return s_xHiZBuffer.UAV(uMip, 0);
}
```

---

### Step 6.6: Update Flux_HiZ.cpp - Update Bindings in ExecuteHiZMip

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.cpp`

**In `ExecuteHiZMip`, update lines 235 and 238 to use GetMipSRV/GetMipUAV**

---

### Step 6.7: Update Flux_HiZ.cpp - Update Deletion Queue

**File:** `Zenith/Zenith/Flux/HiZ/Flux_HiZ.cpp`

**In `DestroyHiZRenderTargets()`, remove the loop that queues per-mip view deletions**

---

## Phase 7: Flux_IBL Updates

### Step 7.1: Update Flux_IBL.h

**File:** `Zenith/Zenith/Flux/IBL/Flux_IBL.h`

**Remove lines 144-146 (unused static arrays):**
```cpp
// REMOVE:
// static Flux_RenderAttachment s_axIrradianceFaces[6];
// static Flux_RenderAttachment s_axPrefilteredFaces[6];
```

---

### Step 7.2: Update Flux_IBL.cpp - Remove Static Definitions

**File:** `Zenith/Zenith/Flux/IBL/Flux_IBL.cpp`

**Find and remove the definitions of `s_axIrradianceFaces` and `s_axPrefilteredFaces` if they exist**

---

### Step 7.3: Update Flux_IBL.cpp - View Access

**File:** `Zenith/Zenith/Flux/IBL/Flux_IBL.cpp`

**Update debug variable registration to use `.SRV()` accessor**

---

## Phase 8: Global View Access Updates

### Pattern Reference

All occurrences must change from direct member access to accessor method calls:

| Pattern | Replace With |
|---------|-------------|
| `*.m_pxSRV` | `*.SRV()` |
| `*.m_pxRTV` | `*.RTV()` |
| `*.m_pxUAV` | `*.UAV()` |
| `*.m_pxDSV` | `*.DSV()` |
| `s_axMipSRVs[mip]` | `s_xHiZBuffer.SRV(mip, 0)` |
| `s_axMipUAVs[mip]` | `s_xHiZBuffer.UAV(mip, 0)` |

### Files to Update

- Flux_Graphics.cpp
- Flux_HDR.cpp
- Flux_Skybox.cpp
- Flux_SSR.cpp
- Flux_SSGI.cpp
- Flux_SSAO.cpp
- Flux_Shadows.cpp
- Flux_Vegetation/Flux_Grass.cpp
- Flux_Terrain.cpp
- Flux_StaticMeshes.cpp
- Flux_AnimatedMeshes.cpp
- Flux_Particles.cpp
- Flux_SDFs.cpp
- Flux_Fog.cpp and Flux_Fog*.cpp
- Flux_Quads.cpp
- Zenith_Vulkan_Swapchain.cpp
- Zenith_Vulkan_CommandBuffer.cpp

---

## Phase 9: Render Graph Updates

### Step 9.1: Update Flux_RenderGraph.cpp

**File:** `Zenith/Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp`

**Update `TargetSetupToFramebuffer` to use `.RTV()` and `.DSV()` accessors**

---

## Phase 10: Compilation and Fixes

1. Compile the project
2. Fix any missed view access patterns
3. Fix any missed accessor implementations
4. Verify all arrays are properly populated
5. Runtime verification

---

## Implementation Order Summary

| Order | Phase | File |
|-------|-------|------|
| 1 | 1 | ZenithConfig.h |
| 2 | 2 | Flux.h |
| 3 | 3 | Zenith_Vulkan_MemoryManager.h/cpp |
| 4 | 4 | Flux_RenderTargets.cpp (accessors + builders) |
| 5 | 5 | Flux_HiZ.h/cpp |
| 6 | 6 | Flux_IBL.h/cpp |
| 7 | 7 | Flux_Graphics.cpp |
| 8 | 7 | Flux_HDR.cpp |
| 9 | 7 | Flux_Skybox.cpp |
| 10 | 7 | Flux_SSR.cpp |
| 11 | 7 | Flux_SSGI.cpp |
| 12 | 7 | Flux_SSAO.cpp |
| 13 | 7 | Flux_Shadows.cpp |
| 14 | 7 | Flux_Vegetation/Flux_Grass.cpp |
| 15 | 7 | Flux_Terrain.cpp |
| 16 | 7 | Flux_StaticMeshes.cpp |
| 17 | 7 | Flux_AnimatedMeshes.cpp |
| 18 | 7 | Flux_Particles.cpp |
| 19 | 7 | Flux_SDFs.cpp |
| 20 | 7 | Flux_Fog.cpp + Fog*.cpp |
| 21 | 7 | Flux_Quads.cpp |
| 22 | 7 | Zenith_Vulkan_Swapchain.cpp |
| 23 | 7 | Zenith_Vulkan_CommandBuffer.cpp |
| 24 | 8 | Flux_RenderGraph.cpp |
| 25 | 9 | Compile and fix errors |

---

## Summary of Key Changes

1. **Flux_RenderAttachment**: 2D arrays `[mip][slice]` for SRV, UAV, RTV; single DSV
2. **Accessors**: `SRV()`, `UAV()`, `RTV()`, `DSV()` with bounds checking
3. **BuildColour**: Loops over all `m_uNumMips` and `m_uNumLayers` (1 for 2D), populating every valid `[mip][slice]` slot
4. **BuildColourCubemap**: Loops over all `m_uNumMips` and 6 slices, creating RTVs/SRVs for each mip-face combination
5. **Flux_HiZ**: No static arrays; uses `s_xHiZBuffer.SRV(mip, 0)` and `s_xHiZBuffer.UAV(mip, 0)`
6. **Flux_IBL**: Removed dead `s_axIrradianceFaces` and `s_axPrefilteredFaces`
