#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Flux.h"
#include <unordered_map> // #TODO: Replace with engine hash map
#include <unordered_set> // #TODO: Replace with engine hash set
#include <string>

using Flux_RenderGraph_OnRecordFunc = void(*)(Flux_CommandList*, void*);
using Flux_RenderGraph_OnPrepareFunc = void(*)(void*);

// Sentinel constants for "all mips" / "all layers" when declaring a
// subresource usage range. Map onto VK_REMAINING_* in the backend.
constexpr u_int FLUX_RG_ALL_MIPS   = UINT32_MAX;
constexpr u_int FLUX_RG_ALL_LAYERS = UINT32_MAX;

struct Flux_RenderGraph_ResourceUsage
{
    Flux_GraphResource m_xResource;
    ResourceAccess m_eAccess = RESOURCE_ACCESS_READ_SRV;
    u_int m_uMipLevel = 0;
    u_int m_uMipCount = 1;
    u_int m_uLayer = 0;
    u_int m_uLayerCount = 1;
};

struct Flux_RenderGraph_Resource
{
    Flux_GraphResource m_xResource;
    u_int m_uFirstWrite = UINT32_MAX;
    u_int m_uLastRead = UINT32_MAX;
};

// Image barrier metadata carried from graph compile into the platform layer.
// Subresource range fields mirror the usage's declared (uMip/uMipCount/uLayer/uLayerCount)
// so the backend can construct a tight vk::ImageSubresourceRange instead of
// collapsing the whole image with VK_REMAINING_*.
struct Flux_RenderGraph_ImageBarrier
{
    Flux_GraphResource m_xResource;
    ResourceAccess m_ePrevAccess = RESOURCE_ACCESS_UNDEFINED;
    ResourceAccess m_eNewAccess = RESOURCE_ACCESS_READ_SRV;
    u_int m_uMipLevel = 0;
    u_int m_uMipCount = 1;
    u_int m_uLayer = 0;
    u_int m_uLayerCount = 1;
    bool m_bDiscard = false;
};

struct Flux_RenderGraph_Pass
{
    std::string m_strName;
    Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xReads;
    Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xWrites;
    Zenith_Vector<u_int> m_xExplicitDependencies;
    Flux_RenderGraph_OnRecordFunc m_pfnOnRecord = nullptr;
    Flux_RenderGraph_OnPrepareFunc m_pfnOnPrepare = nullptr;
    void* m_pUserData = nullptr;
    u_int m_uTopologicalOrder = UINT32_MAX;
    bool m_bIsCompute = false;
    bool m_bEnabled = true;
    bool m_bRequestsClear = false;
    bool m_bClearTargets = false;
    Flux_RenderGraph_AttachmentRef m_axColourAttachments[FLUX_MAX_TARGETS];
    uint32_t m_uNumColourAttachments = 0;
    Flux_RenderGraph_AttachmentRef m_xDepthStencil;
    Zenith_Vector<Flux_RenderGraph_ImageBarrier> m_xPrologueBarriers;
    Zenith_Vector<Flux_RenderGraph_ImageBarrier> m_xEpilogueBarriers;
    Flux_CommandList* m_pxCommandList = nullptr;

    void EmitPrologueBarriers(vk::CommandBuffer xCmdBuf, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage) const;

    ~Flux_RenderGraph_Pass()
    {
        delete m_pxCommandList;
        m_pxCommandList = nullptr;
    }
};

class Flux_RenderGraph
{
public:
    Flux_RenderGraph() = default;
    ~Flux_RenderGraph() { Clear(); }

    u_int AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord, void* pUserData = nullptr, u_int uInitialCapacity = 4096);

    // 2D image resources
    void Read(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV, u_int uMip = 0, u_int uMipCount = FLUX_RG_ALL_MIPS);
    void Write(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV, u_int uMip = 0, u_int uMipCount = 1);

    // Cubemap image resources. Layer is the face index (0..5); layer count defaults
    // cover "whole cube" for reads and "single face" for writes (the IBL pattern).
    void Read(u_int uPassIndex, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV,
              u_int uMip = 0, u_int uMipCount = FLUX_RG_ALL_MIPS,
              u_int uLayer = 0, u_int uLayerCount = FLUX_RG_ALL_LAYERS);
    void Write(u_int uPassIndex, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV,
               u_int uMip = 0, u_int uMipCount = 1,
               u_int uLayer = 0, u_int uLayerCount = 1);

    // Buffers
    void ReadBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess);
    void WriteBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess);

    void DependsOn(u_int uDependentPass, u_int uDependencyPass);
    void SetEnabled(u_int uPassIndex, bool bEnabled);
    void SetPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare);
    void SetClear(u_int uPassIndex, bool bClearTargets);
    bool Compile();
    void Execute();
    void MarkDirty();
    bool IsDirty() const { return m_bDirty; }
    void Clear();

    const Zenith_Vector<Flux_RenderGraph_Pass*>& GetPasses() const { return m_xPasses; }
    const Zenith_Vector<u_int>& GetExecutionOrder() const { return m_xExecutionOrder; }
    const std::unordered_map<void*, Flux_RenderGraph_Resource>& GetResources() const { return m_xResources; }

private:
    Zenith_Vector<Flux_RenderGraph_Pass*> m_xPasses;
    // #TODO: Replace std::unordered_map with engine hash map
    std::unordered_map<void*, Flux_RenderGraph_Resource> m_xResources;
    Zenith_Vector<u_int> m_xExecutionOrder;
    bool m_bCompiled = false;
    bool m_bDirty = true;
    bool m_bEnabledMaskDirty = false;

    struct ResourceTraffic { Zenith_Vector<u_int> m_xWriters; Zenith_Vector<u_int> m_xReaders; };
    // #TODO: Replace std::unordered_map with engine hash map
    std::unordered_map<void*, ResourceTraffic> m_xTraffic;
    // Barrier state keyed on (void*, uMip, uLayer) packed into a u_int64 so that
    // per-face / per-mip writes correctly emit UNDEFINED→layout transitions
    // instead of getting short-circuited by a pointer-only cache.
    // #TODO: Replace std::unordered_map with engine hash map
    std::unordered_map<u_int64, ResourceAccess> m_xBarrierState;
    // Clear tracking is keyed on (void*, uMip, uLayer) packed into a u_int64 —
    // same scheme as m_xBarrierState — so that a cubemap whose 42 (mip, face)
    // subresources are each written by a different pass correctly grants every
    // pass "first writer" status for *its* subresource. A ptr-only key would
    // grant first-writer to only the first of the 42 passes and leave the
    // other 41 with bClearTargets=false, driving the backend to transition
    // SHADER_READ_ONLY_OPTIMAL → COLOR_ATTACHMENT_OPTIMAL on subresources that
    // were never written and are actually still in UNDEFINED layout — which
    // the Vulkan validator (correctly) rejects.
    // #TODO: Replace std::unordered_map with engine hash map
    std::unordered_map<u_int64, bool> m_xAttachmentNeedsClear;
    // #TODO: Replace std::unordered_set with engine hash set
    std::unordered_set<u_int64> m_xAttachmentClearAssigned;
    // #TODO: Replace std::unordered_set with engine hash set
    std::unordered_set<u_int64> m_xEdgeSet;
    Zenith_Vector<Zenith_Vector<u_int>> m_xAdjacency;
    Zenith_Vector<u_int> m_xInDegree;
    Zenith_Vector<u_int> m_xQueue;

    Flux_RenderGraph_Pass* GetPass(u_int uIdx) const;
    void TrackResource(const Flux_GraphResource& xResource);
    void AddResourceUsage(u_int uPassIndex, const Flux_GraphResource& xResource, ResourceAccess eAccess,
                          u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount, bool bWrite);
    void BuildResourceTraffic();
    void Validate() const;
    bool TopologicalSort();
    void ComputeResourceLifetimes();
    void GenerateBarriers();
    void ResolveClearFlags();
    void AssertMutable(const char* szFn);
    void MarkPassesAsComputeOrGraphics();
    void CallPrepareCallbacks();
    void RecordCommandLists();
    void SubmitRecordedLists();
    void GenerateImageBarriers(Flux_RenderGraph_Pass& rxPass, const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, bool bIsRead);
    void InitAdjacencyData();
    void BuildAdjacencyFromTraffic();
    void AddReaderWriterEdges(const Zenith_Vector<u_int>& axReaders, const Zenith_Vector<u_int>& axWriters);
    bool IsAlsoWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters);
    u_int FindBestWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters);
    void AddWriterChainEdges(const Zenith_Vector<u_int>& axWriters);
    void AddEdgeIfNew(u_int uFrom, u_int uTo);
    void AddExplicitDependencies();
    void InferPassAttachments();
    void CollectClearRequirements();
    void AssignClearFlags();

    friend void Flux_RenderGraph_RecordPassTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex);
};

namespace std { template<> struct hash<Flux_GraphResource> { size_t operator()(const Flux_GraphResource& xRes) const { return static_cast<size_t>(xRes.GetHash()); } }; }
