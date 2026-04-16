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

// TODO: Unify barrier generation. Graph-level barrier synthesis was deleted
// because it was untested and unused by the Vulkan backend (which computes its
// own barriers in RecordCommandBuffersTask). Re-introduce graph-driven barriers
// with proper tests before wiring to the backend.

struct Flux_RenderGraph_Pass
{
#ifdef ZENITH_TOOLS
    std::string m_strName;
#endif
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
    Flux_CommandList* m_pxCommandList = nullptr;

    // Debug-name accessor that compiles in all configurations. Tools builds return the real
    // name; shipping builds return a placeholder so Zenith_Assert(...) sites keep compiling
    // without pulling std::string weight into release binaries.
#ifdef ZENITH_TOOLS
    const char* DebugName() const { return m_strName.c_str(); }
#else
    const char* DebugName() const { return "<release>"; }
#endif

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

    // Transient resources — graph-owned allocation. Declare in SetupRenderGraph;
    // the graph allocates the backing Flux_RenderAttachment at Compile() time.
    // Handles are invalidated on Clear() / recompile.
    // Global toggle for transient resource allocation. When false, CreateTransient
    // still returns a valid handle but the backing attachment is allocated immediately
    // as a standalone resource (same as subsystem-owned). Subsystems should check
    // AreTransientsEnabled() in SetupRenderGraph to decide whether to use the
    // transient or owned path.
    bool AreTransientsEnabled() const { return m_bTransientsEnabled; }
    void SetTransientsEnabled(bool bEnabled) { m_bTransientsEnabled = bEnabled; }
    bool m_bTransientsEnabled = true; // Public for debug variable binding

    Flux_TransientHandle CreateTransient(const Flux_TransientTextureDesc& xDesc);
    void ReadTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV);
    void WriteTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV);
    Flux_RenderAttachment& GetTransientAttachment(Flux_TransientHandle xHandle);
    const Flux_RenderAttachment& GetTransientAttachment(Flux_TransientHandle xHandle) const;

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
    // Transient resources owned by the graph (heap-allocated for pointer stability —
    // Flux_GraphResource stores raw pointers to the m_xAttachment, so the address
    // must not move when new transients are added to the vector).
    struct TransientResource
    {
        Flux_TransientTextureDesc m_xDesc;
        Flux_RenderAttachment m_xAttachment;
        bool m_bAllocated = false;
    };
    Zenith_Vector<TransientResource*> m_axTransients;
    void AllocateTransients();
    void DestroyTransients();

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
    // Clear tracking is keyed on (void*, uMip, uLayer) packed into a u_int64 so that a cubemap whose 42 (mip, face)
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
    void ResolveClearFlags();
    void AssertMutable(const char* szFn);
    void MarkPassesAsComputeOrGraphics();
    void CallPrepareCallbacks();
    void RecordCommandLists();
    void SubmitRecordedLists();
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
