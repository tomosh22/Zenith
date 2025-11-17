# Flux Rendering Architecture - Deep Dive

## Overview

Flux is Zenith's platform-agnostic rendering abstraction layer. It provides a command-buffer-based API that translates to Vulkan calls while hiding platform-specific details from game code.

## Design Philosophy

**Goals:**
1. **Platform Independence:** Game code doesn't touch Vulkan directly
2. **Deferred Execution:** Commands recorded in command lists, executed later
3. **Parallel Recording:** Multiple threads can record commands simultaneously
4. **Cache Friendly:** Data-oriented design, minimal indirection
5. **Zero Overhead:** Abstractions compile away, no runtime cost

## Core Concepts

### Command Lists

**Flux_CommandList** is a dynamically-sized buffer of rendering commands.

**Storage Layout:**
```
[CommandType:1byte][CommandData:Nbytes][CommandType:1byte][CommandData:Nbytes]...
```

**Example:**
```cpp
Flux_CommandList cmdList("MyPass");
cmdList.AddCommand<Flux_CommandSetPipeline>(myPipeline);
cmdList.AddCommand<Flux_CommandSetVertexBuffer>(myVB);
cmdList.AddCommand<Flux_CommandSetIndexBuffer>(myIB);
cmdList.AddCommand<Flux_CommandPushConstant>(&pushData, sizeof(pushData));
cmdList.AddCommand<Flux_CommandDrawIndexed>(numIndices);
```

**Iteration (Fast Path):**
```cpp
void IterateCommands(Flux_CommandBuffer* cmdBuf) {
    u_int cursor = 0;
    while (cursor < m_uCursor) {
        Flux_CommandType type = *reinterpret_cast<Flux_CommandType*>(&m_pcData[cursor]);
        
        switch(type) {
            case FLUX_COMMANDTYPE__SET_PIPELINE:
                (*reinterpret_cast<Flux_CommandSetPipeline*>(&m_pcData[cursor + 1]))(cmdBuf);
                cursor += sizeof(Flux_CommandSetPipeline) + 1;
                break;
            // ... other cases
        }
    }
}
```

**Memory Management:**
- Starts at 32 bytes
- Doubles when full
- Reuses allocation between frames via `Reset()`

### Target Setup

**Flux_TargetSetup** defines render targets for a pass.

**Structure:**
```cpp
struct Flux_TargetSetup {
    Flux_RenderAttachment m_axColourAttachments[FLUX_MAX_TARGETS]; // Up to 8
    Flux_RenderAttachment* m_pxDepthStencil;  // Optional, not owned
};
```

**Render Attachment:**
```cpp
struct Flux_RenderAttachment {
    Flux_SurfaceInfo m_xSurfaceInfo;  // Format, dimensions
    Flux_VRAMHandle m_xVRAMHandle;    // GPU resource handle
    
    // Views for different usages
    Flux_ShaderResourceView m_pxSRV;  // Sampling in shaders
    Flux_UnorderedAccessView m_pxUAV; // Read/write in compute
    Flux_RenderTargetView m_pxRTV;    // Color attachment
    Flux_DepthStencilView m_pxDSV;    // Depth/stencil
};
```

**View Types (D3D-style abstraction):**
- **SRV (Shader Resource View):** Read-only texture sampling
- **UAV (Unordered Access View):** Read-write access (compute shaders)
- **RTV (Render Target View):** Color attachment in render pass
- **DSV (Depth Stencil View):** Depth/stencil attachment

**Building Attachments:**
```cpp
Flux_RenderAttachmentBuilder builder;
builder.m_eFormat = TEXTURE_FORMAT_RGBA16F;
builder.m_uWidth = 1920;
builder.m_uHeight = 1080;
builder.m_uMemoryFlags = MEMORY_FLAGS__DEVICE_LOCAL;

Flux_RenderAttachment attachment;
builder.BuildColour(attachment, "MyTarget");

// Now use in target setup
targetSetup.m_axColourAttachments[0] = attachment;
```

### Render Orders

Commands submitted to Flux are categorized by **RenderOrder**:

```cpp
enum RenderOrder {
    RENDER_ORDER_MEMORY_UPDATE,      // 0 - GPU uploads (buffers, textures)
    RENDER_ORDER_COMPUTE_PRE,        // 1 - Pre-frame compute (e.g., culling)
    RENDER_ORDER_GBUFFER,            // 2 - Geometry buffer fill
    RENDER_ORDER_SHADOWS,            // 3 - Shadow map rendering
    RENDER_ORDER_DEFERRED_SHADING,   // 4 - Lighting pass
    RENDER_ORDER_SKY,                // 5 - Skybox
    RENDER_ORDER_FORWARD,            // 6 - Transparent objects
    RENDER_ORDER_PARTICLES,          // 7 - Particle systems
    RENDER_ORDER_POST_PROCESS,       // 8 - Post-processing
    RENDER_ORDER_UI,                 // 9 - UI / ImGui
    RENDER_ORDER_MAX                 // 10
};
```

**Submission:**
```cpp
Flux::SubmitCommandList(&cmdList, targetSetup, RENDER_ORDER_GBUFFER);
```

**Storage:**
```cpp
// In Flux class (static)
Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>> 
    s_xPendingCommandLists[RENDER_ORDER_MAX];
```

**Thread Safety:**
- Submission protected by mutex
- Each render order has its own vector
- Cleared after frame submission

## Frame Flow

### High-Level Overview

```
Game Code (multiple systems)
    ? (Submit command lists)
Flux::s_xPendingCommandLists[RENDER_ORDER_MAX]
    ? (PrepareFrame)
Flux_WorkDistribution (divide work among workers)
    ? (RecordCommandBuffersTask)
8x Worker Threads
    ? (Each records to Zenith_Vulkan_CommandBuffer)
8x vk::CommandBuffer
    ? (Submit in order)
GPU Execution
```

### Detailed Steps

#### 1. Command List Submission (Game Code)

**Example from Flux_StaticMeshes:**
```cpp
void Flux_StaticMeshes::Render() {
    Flux_CommandList* cmdList = GetCommandList();
    cmdList->Reset(true);  // Clear targets
    
    // Record commands
    cmdList->AddCommand<Flux_CommandSetPipeline>(s_pxPipeline);
    
    for (auto& mesh : meshes) {
        cmdList->AddCommand<Flux_CommandSetVertexBuffer>(mesh.vb);
        cmdList->AddCommand<Flux_CommandSetIndexBuffer>(mesh.ib);
        cmdList->AddCommand<Flux_CommandPushConstant>(&mesh.transform, sizeof(matrix));
        cmdList->AddCommand<Flux_CommandDrawIndexed>(mesh.indexCount);
    }
    
    // Submit
    Flux::SubmitCommandList(cmdList, s_xTargetSetup, RENDER_ORDER_GBUFFER);
}
```

#### 2. Work Distribution (PrepareFrame)

**Called from:** `Zenith_Vulkan::EndFrame()`

**Purpose:** Divide command lists across worker threads

**Algorithm:**
```cpp
bool Flux::PrepareFrame(Flux_WorkDistribution& dist) {
    // Count total commands
    u_int totalCommands = 0;
    for (u_int ro = 0; ro < RENDER_ORDER_MAX; ro++) {
        for (auto& [cmdList, targetSetup] : s_xPendingCommandLists[ro]) {
            totalCommands += cmdList->GetCommandCount();
        }
    }
    
    if (totalCommands == 0)
        return false;  // No work
    
    // Distribute
    u_int commandsPerWorker = totalCommands / FLUX_NUM_WORKER_THREADS;
    u_int commandsAssigned = 0;
    
    for (u_int worker = 0; worker < FLUX_NUM_WORKER_THREADS; worker++) {
        u_int workerStart = commandsAssigned;
        u_int workerEnd = (worker == FLUX_NUM_WORKER_THREADS - 1) 
            ? totalCommands 
            : workerStart + commandsPerWorker;
        
        // Find (renderOrder, index) for workerStart
        FindPosition(workerStart, dist.auStartRenderOrder[worker], dist.auStartIndex[worker]);
        
        // Find (renderOrder, index) for workerEnd
        FindPosition(workerEnd, dist.auEndRenderOrder[worker], dist.auEndIndex[worker]);
        
        commandsAssigned = workerEnd;
    }
    
    return true;
}
```

**Example Distribution:**
```
Total Commands: 1000
Workers: 8
Commands per worker: 125

Worker 0: RO=2, Idx=0   to RO=2, Idx=50   (125 commands)
Worker 1: RO=2, Idx=50  to RO=2, Idx=100  (125 commands)
Worker 2: RO=2, Idx=100 to RO=3, Idx=25   (125 commands, spans render orders)
...
Worker 7: RO=9, Idx=50  to RO=9, Idx=100  (125 commands)
```

#### 3. Parallel Recording (RecordCommandBuffersTask)

**Invocation:**
```cpp
Zenith_TaskArray recordingTask(
    ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
    RecordCommandBuffersTask,
    &workDistribution,
    FLUX_NUM_WORKER_THREADS,
    true  // Submitting thread joins
);
Zenith_TaskSystem::SubmitTaskArray(&recordingTask);
recordingTask.WaitUntilComplete();
```

**Task Function:**
```cpp
void RecordCommandBuffersTask(void* pData, u_int workerIndex, u_int numWorkers) {
    Flux_WorkDistribution* dist = static_cast<Flux_WorkDistribution*>(pData);
    
    // Get worker's command buffer (persistent, one per worker per frame)
    Zenith_Vulkan_CommandBuffer& cmdBuf = 
        Zenith_Vulkan::s_pxCurrentFrame->GetWorkerCommandBuffer(workerIndex);
    
    cmdBuf.BeginRecording();
    
    // Get assigned range
    u_int startRO = dist->auStartRenderOrder[workerIndex];
    u_int startIdx = dist->auStartIndex[workerIndex];
    u_int endRO = dist->auEndRenderOrder[workerIndex];
    u_int endIdx = dist->auEndIndex[workerIndex];
    
    // Iterate assigned command lists
    Flux_TargetSetup currentTarget;
    
    for (u_int ro = startRO; ro <= endRO; ro++) {
        auto& cmdLists = Flux::s_xPendingCommandLists[ro];
        
        u_int idxStart = (ro == startRO) ? startIdx : 0;
        u_int idxEnd = (ro == endRO) ? endIdx : cmdLists.GetSize();
        
        for (u_int i = idxStart; i < idxEnd; i++) {
            const Flux_CommandList* cmdList = cmdLists.Get(i).first;
            Flux_TargetSetup& targetSetup = cmdLists.Get(i).second;
            
            // Manage render passes
            bool bClear = cmdList->RequiresClear();
            bool bTargetChanged = (targetSetup != currentTarget);
            bool bIsCompute = (targetSetup == Flux_Graphics::s_xNullTargetSetup);
            
            if (bIsCompute) {
                // Compute pass - end current render pass
                if (cmdBuf.m_xCurrentRenderPass != VK_NULL_HANDLE) {
                    cmdBuf.EndRenderPass();
                    TransitionTargetsAfterRenderPass(cmdBuf, currentTarget, ...);
                }
                cmdList->IterateCommands(&cmdBuf);
            }
            else if (bTargetChanged || bClear || cmdBuf.m_xCurrentRenderPass == VK_NULL_HANDLE) {
                // Start new render pass
                if (cmdBuf.m_xCurrentRenderPass != VK_NULL_HANDLE) {
                    cmdBuf.EndRenderPass();
                    TransitionTargetsAfterRenderPass(cmdBuf, currentTarget, ...);
                }
                
                TransitionTargetsForRenderPass(cmdBuf, targetSetup, ..., bClear);
                cmdBuf.BeginRenderPass(targetSetup, bClear, ...);
                currentTarget = targetSetup;
                
                cmdList->IterateCommands(&cmdBuf);
            }
            else {
                // Continue current render pass
                cmdList->IterateCommands(&cmdBuf);
            }
        }
    }
    
    // Finalize
    if (cmdBuf.m_xCurrentRenderPass != VK_NULL_HANDLE) {
        cmdBuf.EndRenderPass();
        TransitionTargetsAfterRenderPass(cmdBuf, currentTarget, ...);
    }
    
    cmdBuf.GetCurrentCmdBuffer().end();
}
```

**Key Points:**
- Each worker has dedicated command buffer (no contention)
- Render passes managed automatically
- Image transitions inserted before/after render passes
- Compute passes handled separately (no render pass)

#### 4. Image Layout Transitions

**Purpose:** Vulkan requires explicit image layout transitions

**Transition Points:**

**Before Render Pass (TransitionTargetsForRenderPass):**
```cpp
void TransitionTargetsForRenderPass(..., bool bClear) {
    vk::ImageLayout oldLayout = bClear 
        ? vk::ImageLayout::eUndefined  // Don't care about previous contents
        : vk::ImageLayout::eShaderReadOnlyOptimal;  // Preserve from previous use
    
    // Color attachments
    TransitionColorTargets(
        cmdBuf, targetSetup,
        oldLayout ? vk::ImageLayout::eColorAttachmentOptimal,
        access: eShaderRead ? eColorAttachmentWrite,
        stage: eFragmentShader ? eColorAttachmentOutput | eEarlyFragmentTests
    );
    
    // Depth attachment (if not clearing)
    if (!bClear && hasDepth) {
        TransitionDepthStencilTarget(
            cmdBuf, targetSetup,
            eShaderReadOnlyOptimal ? eDepthStencilReadOnlyOptimal,
            access: eShaderRead ? eDepthStencilAttachmentRead,
            stage: eFragmentShader ? eEarlyFragmentTests
        );
    }
}
```

**After Render Pass (TransitionTargetsAfterRenderPass):**
```cpp
void TransitionTargetsAfterRenderPass(...) {
    // Color attachments
    TransitionColorTargets(
        cmdBuf, targetSetup,
        eColorAttachmentOptimal ? eShaderReadOnlyOptimal,
        access: eColorAttachmentWrite ? eShaderRead,
        stage: eColorAttachmentOutput | eLateFragmentTests ? eFragmentShader | eComputeShader
    );
    
    // Depth attachment
    TransitionDepthStencilTarget(
        cmdBuf, targetSetup,
        eDepthStencilReadOnlyOptimal ? eShaderReadOnlyOptimal,
        access: eDepthStencilAttachmentRead ? eShaderRead,
        stage: eLateFragmentTests ? eFragmentShader | eComputeShader
    );
}
```

**Why These Layouts:**
- **Undefined:** Don't care (clearing), GPU can optimize
- **ColorAttachmentOptimal:** Writing color data
- **DepthStencilReadOnlyOptimal:** Depth testing without writing (deferred shading)
- **ShaderReadOnlyOptimal:** Sampling in shaders (general purpose)

**Access Masks:**
- **ColorAttachmentWrite:** Blend/write to color target
- **DepthStencilAttachmentRead:** Read depth for depth test
- **ShaderRead:** Sample texture in shader

**Pipeline Stages:**
- **ColorAttachmentOutput:** Color blend stage
- **EarlyFragmentTests:** Depth test before fragment shader
- **LateFragmentTests:** Depth test after fragment shader
- **FragmentShader:** Fragment shader execution
- **ComputeShader:** Compute shader execution

#### 5. Command Buffer Submission

**After recording completes:**
```cpp
void Zenith_Vulkan::EndFrame() {
    // ... recording ...
    
    // Collect all worker command buffers
    std::vector<vk::CommandBuffer> cmdBuffers;
    for (u_int i = 0; i < FLUX_NUM_WORKER_THREADS; i++) {
        cmdBuffers.push_back(s_pxCurrentFrame->GetWorkerCommandBuffer(i).GetCurrentCmdBuffer());
    }
    
    // Submit in order (maintains render order)
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = cmdBuffers.size();
    submitInfo.pCommandBuffers = cmdBuffers.data();
    // ... semaphores ...
    
    s_axQueues[COMMANDTYPE_GRAPHICS].submit(submitInfo, VK_NULL_HANDLE);
    
    // Clear pending lists
    for (u_int i = 0; i < RENDER_ORDER_MAX; i++) {
        Flux::s_xPendingCommandLists[i].Clear();
    }
}
```

**Submission Order:**
- Worker 0's command buffer
- Worker 1's command buffer
- ...
- Worker 7's command buffer

**Why this works:**
- Work distribution is contiguous
- Worker 0 always gets earliest commands
- Worker 7 always gets latest commands
- GPU executes in submission order

## Command Buffer (Zenith_Vulkan_CommandBuffer)

### Persistent State

```cpp
class Zenith_Vulkan_CommandBuffer {
    vk::CommandBuffer m_xCmdBuffer;           // Vulkan command buffer
    vk::RenderPass m_xCurrentRenderPass;      // Active render pass
    Flux_Pipeline* m_pxCurrentPipeline;       // Bound pipeline
    
    // Descriptor set caching
    std::unordered_map<u_int, DescriptorSetInfo> m_xBoundDescriptorSets;
    
    u_int m_uWorkerIndex;  // Which worker thread owns this
};
```

### Descriptor Set Management

**Problem:** Descriptor set allocation is expensive

**Solution 1: Caching**
```cpp
void BindSRV(const Flux_ShaderResourceView* srv, u_int bindPoint, ...) {
    // Check cache
    auto it = m_xBoundDescriptorSets.find(bindPoint);
    if (it != m_xBoundDescriptorSets.end()) {
        DescriptorSetInfo& cached = it->second;
        
        // Same descriptor? Skip bind
        if (cached.srv == srv && cached.sampler == sampler) {
            return;  // Already bound
        }
    }
    
    // Allocate new descriptor set
    vk::DescriptorSet descSet = AllocateDescriptorSet(bindPoint);
    
    // Update
    vk::WriteDescriptorSet write;
    write.dstSet = descSet;
    write.dstBinding = bindPoint;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &imageInfo;
    device.updateDescriptorSets(1, &write, 0, nullptr);
    
    // Bind
    m_xCmdBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipelineLayout,
        0,  // First set
        1,  // Count
        &descSet,
        0,  // Dynamic offsets
        nullptr
    );
    
    // Cache
    m_xBoundDescriptorSets[bindPoint] = {srv, sampler, descSet};
}
```

**Solution 2: Per-Worker Descriptor Pools**
```cpp
// In Zenith_Vulkan_PerFrame
vk::DescriptorPool m_axDescriptorPools[FLUX_NUM_WORKER_THREADS];

// Allocate from worker's pool
vk::DescriptorSetAllocateInfo allocInfo;
allocInfo.descriptorPool = s_pxCurrentFrame->GetDescriptorPoolForWorkerIndex(m_uWorkerIndex);
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &layout;

vk::DescriptorSet descSet = device.allocateDescriptorSets(allocInfo)[0];
```

**Solution 3: Frame-Based Reset**
```cpp
void Zenith_Vulkan_PerFrame::BeginFrame() {
    // Reset all worker pools
    for (vk::DescriptorPool& pool : m_axDescriptorPools) {
        device.resetDescriptorPool(pool);
    }
    
    // No need to free individual sets
}
```

**Result:**
- No contention between workers (separate pools)
- Fast allocation (pool pre-allocated)
- Automatic cleanup (reset every frame)
- Reduced redundant binds (caching)

### Render Pass Management

**Render Pass Creation:**
```cpp
vk::RenderPass CreateRenderPass(const Flux_TargetSetup& setup, bool bClear) {
    // Check cache
    RenderPassKey key = ComputeKey(setup, bClear);
    if (s_xRenderPassCache.contains(key))
        return s_xRenderPassCache[key];
    
    // Create attachments
    std::vector<vk::AttachmentDescription> attachments;
    
    for (auto& colorAttachment : setup.m_axColourAttachments) {
        if (colorAttachment.m_xSurfaceInfo.m_eFormat == TEXTURE_FORMAT_NONE)
            break;
        
        vk::AttachmentDescription desc;
        desc.format = ConvertFormat(colorAttachment.m_xSurfaceInfo.m_eFormat);
        desc.samples = vk::SampleCountFlagBits::e1;
        desc.loadOp = bClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
        desc.storeOp = vk::AttachmentStoreOp::eStore;
        desc.initialLayout = bClear ? vk::ImageLayout::eUndefined : vk::ImageLayout::eColorAttachmentOptimal;
        desc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
        
        attachments.push_back(desc);
    }
    
    // Depth attachment
    if (setup.m_pxDepthStencil) {
        vk::AttachmentDescription desc;
        desc.format = ConvertFormat(setup.m_pxDepthStencil->m_xSurfaceInfo.m_eFormat);
        desc.loadOp = bClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
        desc.storeOp = vk::AttachmentStoreOp::eStore;
        desc.initialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        desc.finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        
        attachments.push_back(desc);
    }
    
    // Create subpass
    std::vector<vk::AttachmentReference> colorRefs;
    for (u_int i = 0; i < numColorAttachments; i++) {
        colorRefs.push_back({i, vk::ImageLayout::eColorAttachmentOptimal});
    }
    
    vk::AttachmentReference depthRef = {
        numColorAttachments,
        vk::ImageLayout::eDepthStencilAttachmentOptimal
    };
    
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = colorRefs.size();
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;
    
    // Create render pass
    vk::RenderPassCreateInfo rpInfo;
    rpInfo.attachmentCount = attachments.size();
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    
    vk::RenderPass renderPass = device.createRenderPass(rpInfo);
    
    // Cache
    s_xRenderPassCache[key] = renderPass;
    return renderPass;
}
```

**Framebuffer Creation:**
```cpp
vk::Framebuffer CreateFramebuffer(vk::RenderPass renderPass, const Flux_TargetSetup& setup) {
    std::vector<vk::ImageView> attachments;
    
    // Color attachments
    for (auto& colorAttachment : setup.m_axColourAttachments) {
        if (colorAttachment.m_xSurfaceInfo.m_eFormat == TEXTURE_FORMAT_NONE)
            break;
        attachments.push_back(colorAttachment.m_pxRTV.m_xImageView);
    }
    
    // Depth attachment
    if (setup.m_pxDepthStencil) {
        attachments.push_back(setup.m_pxDepthStencil->m_pxDSV.m_xImageView);
    }
    
    vk::FramebufferCreateInfo fbInfo;
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = attachments.size();
    fbInfo.pAttachments = attachments.data();
    fbInfo.width = setup.m_axColourAttachments[0].m_xSurfaceInfo.m_uWidth;
    fbInfo.height = setup.m_axColourAttachments[0].m_xSurfaceInfo.m_uHeight;
    fbInfo.layers = 1;
    
    return device.createFramebuffer(fbInfo);
}
```

## Pipeline Management

### Pipeline Specification

```cpp
struct Flux_PipelineSpecification {
    Flux_Shader* m_pxShader;  // Vertex, fragment, etc.
    
    Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];  // Per-target blending
    
    bool m_bDepthTestEnabled;
    bool m_bDepthWriteEnabled;
    DepthCompareFunc m_eDepthCompareFunc;  // Less, LessEqual, Greater, etc.
    TextureFormat m_eDepthStencilFormat;
    
    Flux_PipelineLayout m_xPipelineLayout;  // Descriptor set layouts
    Flux_VertexInputDescription m_xVertexInputDesc;  // Vertex attributes
    
    Flux_TargetSetup* m_pxTargetSetup;  // Render targets
    LoadAction m_eColourLoadAction;
    StoreAction m_eColourStoreAction;
    LoadAction m_eDepthStencilLoadAction;
    StoreAction m_eDepthStencilStoreAction;
    
    bool m_bWireframe;
    bool m_bUsePushConstants;
    bool m_bUseTesselation;
    
    bool m_bDepthBias;
    float m_fDepthBiasConstant;
    float m_fDepthBiasSlope;
    float m_fDepthBiasClamp;
};
```

### Pipeline Creation

```cpp
Flux_Pipeline* CreateGraphicsPipeline(const Flux_PipelineSpecification& spec) {
    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    
    // Vertex input
    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vertexInput.vertexBindingDescriptionCount = spec.m_xVertexInputDesc.bindings.GetSize();
    vertexInput.pVertexBindingDescriptions = spec.m_xVertexInputDesc.bindings.GetDataPointer();
    vertexInput.vertexAttributeDescriptionCount = spec.m_xVertexInputDesc.attributes.GetSize();
    vertexInput.pVertexAttributeDescriptions = spec.m_xVertexInputDesc.attributes.GetDataPointer();
    
    // Rasterization
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.polygonMode = spec.m_bWireframe 
        ? vk::PolygonMode::eLine 
        : vk::PolygonMode::eFill;
    rasterization.cullMode = vk::CullModeFlagBits::eBack;
    rasterization.frontFace = vk::FrontFace::eCounterClockwise;
    rasterization.depthBiasEnable = spec.m_bDepthBias;
    rasterization.depthBiasConstantFactor = spec.m_fDepthBiasConstant;
    rasterization.depthBiasSlopeFactor = spec.m_fDepthBiasSlope;
    rasterization.depthBiasClamp = spec.m_fDepthBiasClamp;
    
    // Depth stencil
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = spec.m_bDepthTestEnabled;
    depthStencil.depthWriteEnable = spec.m_bDepthWriteEnabled;
    depthStencil.depthCompareOp = ConvertDepthFunc(spec.m_eDepthCompareFunc);
    
    // Color blending (per attachment)
    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments;
    for (u_int i = 0; i < FLUX_MAX_TARGETS; i++) {
        if (spec.m_pxTargetSetup->m_axColourAttachments[i].m_xSurfaceInfo.m_eFormat == TEXTURE_FORMAT_NONE)
            break;
        
        vk::PipelineColorBlendAttachmentState blendState;
        blendState.blendEnable = spec.m_axBlendStates[i].m_bEnabled;
        blendState.srcColorBlendFactor = ConvertBlendFactor(spec.m_axBlendStates[i].m_eSrcColor);
        blendState.dstColorBlendFactor = ConvertBlendFactor(spec.m_axBlendStates[i].m_eDstColor);
        blendState.colorBlendOp = ConvertBlendOp(spec.m_axBlendStates[i].m_eColorOp);
        blendState.srcAlphaBlendFactor = ConvertBlendFactor(spec.m_axBlendStates[i].m_eSrcAlpha);
        blendState.dstAlphaBlendFactor = ConvertBlendFactor(spec.m_axBlendStates[i].m_eDstAlpha);
        blendState.alphaBlendOp = ConvertBlendOp(spec.m_axBlendStates[i].m_eAlphaOp);
        blendState.colorWriteMask = vk::ColorComponentFlagBits::eR | 
                                     vk::ColorComponentFlagBits::eG | 
                                     vk::ColorComponentFlagBits::eB | 
                                     vk::ColorComponentFlagBits::eA;
        
        blendAttachments.push_back(blendState);
    }
    
    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.attachmentCount = blendAttachments.size();
    colorBlending.pAttachments = blendAttachments.data();
    
    // Shader stages
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = 
        spec.m_pxShader->GetStageInfos();
    
    // Create pipeline
    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = spec.m_xPipelineLayout.GetVulkanLayout();
    pipelineInfo.renderPass = GetOrCreateRenderPass(spec);
    pipelineInfo.subpass = 0;
    
    vk::Pipeline vkPipeline = device.createGraphicsPipelines(
        VK_NULL_HANDLE,  // Pipeline cache
        1,
        &pipelineInfo
    ).value[0];
    
    return new Flux_Pipeline(vkPipeline, spec);
}
```

## Performance Optimizations

### 1. Command List Reuse

**Problem:** Allocating command lists every frame is expensive

**Solution:**
```cpp
class Flux_StaticMeshes {
    static Flux_CommandList s_xCommandList;
};

void Flux_StaticMeshes::Render() {
    // Reuse same memory allocation
    s_xCommandList.Reset(true);
    
    // Record new commands
    s_xCommandList.AddCommand<...>(...);
    
    // Submit
    Flux::SubmitCommandList(&s_xCommandList, ...);
}
```

### 2. Descriptor Set Caching

**Metrics:**
- Without cache: ~5000 descriptor sets allocated per frame
- With cache: ~500 descriptor sets allocated per frame
- **90% reduction**

**Debug Variable:**
```cpp
DEBUGVAR bool dbg_bUseDescSetCache = true;
DEBUGVAR bool dbg_bOnlyUpdateDirtyDescriptors = true;
DEBUGVAR u_int dbg_uNumDescSetAllocations = 0;
```

### 3. Parallel Recording

**Scaling:**
- 1 thread: 100% work
- 8 threads: ~12.5% work per thread
- **Near-linear speedup** (minimal overhead)

**Bottlenecks:**
- Work distribution calculation (negligible)
- Queue submission (serial, but fast)

### 4. Push Constants

**Problem:** Updating buffers for per-draw data is slow

**Solution:** Push constants (direct GPU writes)
```cpp
struct PushConstantData {
    glm::mat4 modelMatrix;
    uint32_t materialIndex;
    // ... up to 128 bytes
};

cmdList->AddCommand<Flux_CommandPushConstant>(&pushData, sizeof(pushData));
```

**Advantages:**
- No buffer allocation
- No buffer update
- No descriptor set
- **~10x faster** than constant buffers for small data

### 5. Bindless Textures

**Problem:** Binding textures individually is slow

**Solution:** Single large descriptor set with all textures
```cpp
// One-time setup
vk::DescriptorSetLayout layout;  // 1000 textures
vk::DescriptorSet bindlessSet;

// Usage in shader
layout(set = 0, binding = 0) uniform sampler2D textures[1000];

vec4 color = texture(textures[pushConstants.textureIndex], uv);
```

**Advantages:**
- No per-draw texture binds
- Texture changes = update push constant index
- **Massive reduction** in descriptor updates

## Compute Shader Support

### Compute Command List

```cpp
Flux_CommandList computeList("Culling");
computeList.Reset(false);  // No clear

// Bind compute pipeline
computeList.AddCommand<Flux_CommandBindComputePipeline>(cullingPipeline);

// Bind input/output buffers
computeList.AddCommand<Flux_CommandBindSRV>(&meshBuffer.m_xSRV, 0);
computeList.AddCommand<Flux_CommandBindUAV>(&visibilityBuffer.m_xUAV, 1);

// Dispatch
u_int numGroups = (numMeshes + 63) / 64;  // 64 threads per group
computeList.AddCommand<Flux_CommandDispatch>(numGroups, 1, 1);

// Submit to compute render order
Flux::SubmitCommandList(&computeList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_COMPUTE_PRE);
```

### Compute Pipeline Creation

```cpp
Flux_Pipeline* CreateComputePipeline(Flux_Shader* shader, Flux_PipelineLayout layout) {
    vk::ComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.stage = shader->GetComputeStageInfo();
    pipelineInfo.layout = layout.GetVulkanLayout();
    
    vk::Pipeline vkPipeline = device.createComputePipelines(
        VK_NULL_HANDLE,
        1,
        &pipelineInfo
    ).value[0];
    
    return new Flux_Pipeline(vkPipeline);
}
```

### Synchronization

**Between Compute and Graphics:**
```cpp
// Compute writes to buffer
// Graphics reads from buffer

// Pipeline barrier
vk::BufferMemoryBarrier barrier;
barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
barrier.dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
barrier.buffer = visibilityBuffer.GetVulkanBuffer();
barrier.size = VK_WHOLE_SIZE;

cmdBuffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,   // Src stage
    vk::PipelineStageFlagBits::eDrawIndirect,    // Dst stage
    vk::DependencyFlags(),
    0, nullptr,
    1, &barrier,
    0, nullptr
);
```

## Future Improvements

### 1. Indirect Drawing
- GPU-driven rendering
- Frustum culling on GPU
- Reduce CPU overhead

### 2. Async Compute
- Overlap compute with graphics
- Use dedicated compute queue
- More granular synchronization

### 3. Ray Tracing
- RT pipeline support
- Acceleration structure management
- Hybrid rasterization + RT

### 4. Mesh Shaders
- Replace vertex+geometry shaders
- More flexible geometry processing
- Better performance for procedural geometry

### 5. Bindless Everything
- Extend beyond textures
- Buffers, samplers, etc.
- Eliminate descriptor updates entirely

## Conclusion

Flux provides a high-performance, platform-agnostic rendering abstraction that:
- **Scales with CPU cores** via parallel command recording
- **Minimizes driver overhead** via descriptor caching, push constants, bindless textures
- **Simplifies game code** via command list API
- **Maintains flexibility** via render orders and target setups

The architecture is designed for modern GPUs and multi-core CPUs, achieving near-optimal utilization of both.
