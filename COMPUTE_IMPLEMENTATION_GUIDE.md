# Compute Shader Implementation Guide for Zenith Engine

This document outlines all the changes needed to add compute shader support to the Zenith engine.

## Files Created
✅ c:\dev\Zenith\Zenith\Flux\Enums.h - Added RENDER_ORDER_COMPUTE_TEST
✅ c:\dev\Zenith\Zenith\Flux\ComputeTest\Flux_ComputeTest.h
✅ c:\dev\Zenith\Zenith\Flux\ComputeTest\Flux_ComputeTest.cpp
✅ c:\dev\Zenith\Zenith\Flux\Shaders\ComputeTest\ComputeTest.comp
✅ c:\dev\Zenith\Zenith\Flux\Shaders\ComputeTest\ComputeTest_Display.vert  
✅ c:\dev\Zenith\Zenith\Flux\Shaders\ComputeTest\ComputeTest_Display.frag

## Required Infrastructure Changes

### 1. Zenith_Vulkan_Shader.h/cpp
Add compute shader support to the shader class.

#### In Header (Zenith_Vulkan_Shader.h):
```cpp
class Zenith_Vulkan_Shader
{
public:
    // ... existing members ...
    
    // ADD THIS:
    void InitialiseCompute(const std::string& strCompute);
    vk::ShaderModule m_xCompShaderModule;
    char* m_pcCompShaderCode = nullptr;
    uint64_t m_pcCompShaderCodeSize = 0;
};
```

#### In Implementation (Zenith_Vulkan_Shader.cpp):
```cpp
void Zenith_Vulkan_Shader::InitialiseCompute(const std::string& strCompute)
{
    std::string strComputePath = strCompute + ".spv";
    std::ifstream xFileComp(strComputePath, std::ios::binary | std::ios::ate);
    
    if (xFileComp.is_open())
    {
        m_pcCompShaderCodeSize = xFileComp.tellg();
        m_pcCompShaderCode = new char[m_pcCompShaderCodeSize];
        xFileComp.seekg(0);
        xFileComp.read(m_pcCompShaderCode, m_pcCompShaderCodeSize);
        xFileComp.close();
        
        m_xCompShaderModule = CreateShaderModule(m_pcCompShaderCode, m_pcCompShaderCodeSize);
        m_uStageCount = 1;
    }
    else
    {
        Zenith_Error("Failed to open compute shader: %s", strComputePath.c_str());
    }
}
```

### 2. Zenith_Vulkan_Pipeline.h/cpp
Add compute pipeline builder class.

#### In Header (Zenith_Vulkan_Pipeline.h):
```cpp
class Zenith_Vulkan_ComputePipelineBuilder
{
public:
    Zenith_Vulkan_ComputePipelineBuilder();
    
    Zenith_Vulkan_ComputePipelineBuilder& WithShader(const Zenith_Vulkan_Shader& shader);
    Zenith_Vulkan_ComputePipelineBuilder& WithLayout(vk::PipelineLayout layout);
    
    void Build(Zenith_Vulkan_Pipeline& pipelineOut);
    
private:
    const Zenith_Vulkan_Shader* m_pxShader = nullptr;
    vk::PipelineLayout m_xLayout;
};
```

#### In Implementation (Zenith_Vulkan_Pipeline.cpp):
```cpp
Zenith_Vulkan_ComputePipelineBuilder::Zenith_Vulkan_ComputePipelineBuilder()
{
}

Zenith_Vulkan_ComputePipelineBuilder& Zenith_Vulkan_ComputePipelineBuilder::WithShader(const Zenith_Vulkan_Shader& shader)
{
    m_pxShader = &shader;
    return *this;
}

Zenith_Vulkan_ComputePipelineBuilder& Zenith_Vulkan_ComputePipelineBuilder::WithLayout(vk::PipelineLayout layout)
{
    m_xLayout = layout;
    return *this;
}

void Zenith_Vulkan_ComputePipelineBuilder::Build(Zenith_Vulkan_Pipeline& pipelineOut)
{
    vk::PipelineShaderStageCreateInfo stageInfo = vk::PipelineShaderStageCreateInfo()
        .setStage(vk::ShaderStageFlagBits::eCompute)
        .setModule(m_pxShader->m_xCompShaderModule)
        .setPName("main");
    
    vk::ComputePipelineCreateInfo pipelineInfo = vk::ComputePipelineCreateInfo()
        .setStage(stageInfo)
        .setLayout(m_xLayout);
    
    vk::Result result = Zenith_Vulkan::GetDevice().createComputePipelines(
        VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineOut.m_xPipeline);
    
    if (result != vk::Result::eSuccess)
    {
        Zenith_Error("Failed to create compute pipeline");
    }
}
```

### 3. Zenith_Vulkan_CommandBuffer.h/cpp
Add dispatch command support.

#### In Header (Zenith_Vulkan_CommandBuffer.h):
```cpp
class Zenith_Vulkan_CommandBuffer
{
public:
    // ... existing members ...
    
    // ADD THESE:
    void BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void BindStorageImage(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint);
    void ImageBarrier(
        Zenith_Vulkan_Texture* pxTexture,
        vk::PipelineStageFlags eSrcStage,
        vk::PipelineStageFlags eDstStage,
        vk::AccessFlags eSrcAccess,
        vk::AccessFlags eDstAccess,
        vk::ImageLayout eOldLayout,
        vk::ImageLayout eNewLayout
    );
};
```

#### In Implementation (Zenith_Vulkan_CommandBuffer.cpp):
```cpp
void Zenith_Vulkan_CommandBuffer::BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
    m_pxCurrentPipeline = pxPipeline;
    GetCurrentCmdBuffer().bindPipeline(vk::PipelineBindPoint::eCompute, pxPipeline->m_xPipeline);
}

void Zenith_Vulkan_CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    PrepareDrawCallDescriptors(); // Bind descriptors first
    GetCurrentCmdBuffer().dispatch(groupCountX, groupCountY, groupCountZ);
}

void Zenith_Vulkan_CommandBuffer::BindStorageImage(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint)
{
    Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
    
    if (pxTexture != m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint])
    {
        m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
        m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint] = pxTexture;
    }
    m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {pxTexture, nullptr};
}

void Zenith_Vulkan_CommandBuffer::ImageBarrier(
    Zenith_Vulkan_Texture* pxTexture,
    vk::PipelineStageFlags eSrcStage,
    vk::PipelineStageFlags eDstStage,
    vk::AccessFlags eSrcAccess,
    vk::AccessFlags eDstAccess,
    vk::ImageLayout eOldLayout,
    vk::ImageLayout eNewLayout)
{
    vk::ImageSubresourceRange range = vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    
    vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier()
        .setSrcAccessMask(eSrcAccess)
        .setDstAccessMask(eDstAccess)
        .setOldLayout(eOldLayout)
        .setNewLayout(eNewLayout)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(pxTexture->GetImage())
        .setSubresourceRange(range);
    
    GetCurrentCmdBuffer().pipelineBarrier(
        eSrcStage, eDstStage,
        vk::DependencyFlags(),
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}
```

### 4. Flux_Types.h (Texture Builder)
Add storage image support.

```cpp
struct Flux_TextureBuilder
{
    // ... existing members ...
    bool m_bStorage = false;  // ADD THIS for compute shader write access
};
```

### 5. Zenith_Vulkan_Texture.cpp
Modify texture creation to support storage images.

In the texture creation code, add:
```cpp
vk::ImageUsageFlags eUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

// ADD THIS:
if (xBuilder.m_bStorage)
{
    eUsage |= vk::ImageUsageFlagBits::eStorage;
}
```

### 6. Flux_CommandList.h/cpp
Add new command types for compute.

#### In Header:
```cpp
struct Flux_CommandBindComputePipeline : public Flux_Command
{
    Zenith_Vulkan_Pipeline* m_pxPipeline;
    Flux_CommandBindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline) : m_pxPipeline(pxPipeline) {}
    void Execute(void* pxCommandBuffer) override;
};

struct Flux_CommandDispatch : public Flux_Command
{
    uint32_t m_uGroupCountX, m_uGroupCountY, m_uGroupCountZ;
    Flux_CommandDispatch(uint32_t x, uint32_t y, uint32_t z) 
        : m_uGroupCountX(x), m_uGroupCountY(y), m_uGroupCountZ(z) {}
    void Execute(void* pxCommandBuffer) override;
};

struct Flux_CommandBindStorageImage : public Flux_Command
{
    Flux_Texture* m_pxTexture;
    uint32_t m_uBindPoint;
    Flux_CommandBindStorageImage(Flux_Texture* pxTex, uint32_t uBind)
        : m_pxTexture(pxTex), m_uBindPoint(uBind) {}
    void Execute(void* pxCommandBuffer) override;
};
```

#### In Implementation:
```cpp
void Flux_CommandBindComputePipeline::Execute(void* pxCommandBuffer)
{
    Zenith_Vulkan_CommandBuffer* pxCmdBuf = (Zenith_Vulkan_CommandBuffer*)pxCommandBuffer;
    pxCmdBuf->BindComputePipeline(m_pxPipeline);
}

void Flux_CommandDispatch::Execute(void* pxCommandBuffer)
{
    Zenith_Vulkan_CommandBuffer* pxCmdBuf = (Zenith_Vulkan_CommandBuffer*)pxCommandBuffer;
    pxCmdBuf->Dispatch(m_uGroupCountX, m_uGroupCountY, m_uGroupCountZ);
}

void Flux_CommandBindStorageImage::Execute(void* pxCommandBuffer)
{
    Zenith_Vulkan_CommandBuffer* pxCmdBuf = (Zenith_Vulkan_CommandBuffer*)pxCommandBuffer;
    Zenith_Vulkan_Texture* pxTex = Flux::Platform_GetTexture(m_pxTexture);
    pxCmdBuf->BindStorageImage(pxTex, m_uBindPoint);
}
```

### 7. Descriptor Set Layout
Update descriptor set layout builder to support storage images.

In `Zenith_Vulkan_Pipeline.cpp`, in `FromSpecification`:
```cpp
case(DESCRIPTOR_TYPE_TEXTURE):
    xBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    break;
case(DESCRIPTOR_TYPE_STORAGE_IMAGE):  // ADD THIS CASE
    xBinding.setDescriptorType(vk::DescriptorType::eStorageImage);
    break;
```

And add to Flux_Enums.h:
```cpp
enum DescriptorType
{
    DESCRIPTOR_TYPE_BUFFER,
    DESCRIPTOR_TYPE_TEXTURE,
    DESCRIPTOR_TYPE_STORAGE_IMAGE,  // ADD THIS
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE,
    DESCRIPTOR_TYPE_UNBOUNDED_TEXTURES,
    DESCRIPTOR_TYPE_MAX
};
```

### 8. Descriptor Writing for Storage Images
In `Zenith_Vulkan_CommandBuffer.cpp`, in `PrepareDrawCallDescriptors`, handle storage images:

```cpp
// After the texture binding loop, add storage image handling:
for (uint32_t i = 0; i < MAX_BINDINGS; i++)
{
    Zenith_Vulkan_Texture* pxTex = m_xBindings[uDescSet].m_xTextures[i].first;
    if (!pxTex) continue;
    
    vk::DescriptorType eDescType = m_pxCurrentPipeline->m_xRootSig.GetDescriptorType(uDescSet, i);
    
    if (eDescType == vk::DescriptorType::eStorageImage)
    {
        vk::DescriptorImageInfo& xInfo = xImageInfos.at(uImageCount)
            .setImageView(pxTex->GetImageView())
            .setImageLayout(vk::ImageLayout::eGeneral);  // Storage images use GENERAL layout
        
        vk::WriteDescriptorSet& xWrite = xImageWrites.at(uImageCount)
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDstSet(m_axCurrentDescSet[uDescSet])
            .setDstBinding(i)
            .setDescriptorCount(1)
            .setPImageInfo(&xInfo);
        
        uImageCount++;
    }
}
```

### 9. Integration into Flux_Graphics
In Flux_Graphics.cpp:

```cpp
#include "Flux/ComputeTest/Flux_ComputeTest.h"

void Flux_Graphics::Initialise()
{
    // ... existing initialization ...
    
    Flux_ComputeTest::Initialise();  // ADD THIS
}

void Flux_Graphics::Render()
{
    // ... existing render code ...
    
    Flux_ComputeTest::Run();  // ADD THIS before or after other render calls
}
```

### 10. Shader Compilation
Compile the shaders to SPIR-V. You'll need glslc or glslangValidator:

```batch
cd c:\dev\Zenith\Zenith\Flux\Shaders\ComputeTest
glslc ComputeTest.comp -o ComputeTest.comp.spv
glslc ComputeTest_Display.vert -o ComputeTest_Display.vert.spv
glslc ComputeTest_Display.frag -o ComputeTest_Display.frag.spv
```

## Testing
Once all infrastructure is in place:
1. The compute shader will execute and write UV coordinates to the output texture
2. The display pass will sample the texture and render it
3. You should see a gradient from black (top-left) to yellow (bottom-right)
   - Red increases left to right (U coordinate)
   - Green increases top to bottom (V coordinate)

## Summary
This implementation adds:
- ✅ Compute shader loading and pipeline creation
- ✅ Compute dispatch commands
- ✅ Storage image support for compute write access
- ✅ Image layout transitions between compute and graphics
- ✅ A test case that writes to a texture in compute and displays it

The architecture follows the existing Zenith patterns for graphics pipelines,
command lists, and resource management.
