#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Flux.h"
#include <unordered_map>
#include <unordered_set>

using Flux_RenderGraph_OnRecordFunc = void(*)(Flux_CommandList*, void*);
using Flux_RenderGraph_OnPrepareFunc = void(*)(void*);

enum class Flux_GraphResourceKind { Image, Buffer };

class Flux_GraphResource
{
public:
    Flux_GraphResource() = default;
    Flux_GraphResource(Flux_RenderAttachment& xImage) : m_eKind(Flux_GraphResourceKind::Image), m_pxImage(&xImage) {}
    Flux_GraphResource(Flux_Buffer& xBuffer) : m_eKind(Flux_GraphResourceKind::Buffer), m_pxBuffer(&xBuffer) {}

    Flux_GraphResourceKind GetKind() const { return m_eKind; }
    
    Flux_RenderAttachment* AsImage() const
    {
        Zenith_Assert(m_eKind == Flux_GraphResourceKind::Image, "Flux_GraphResource: called AsImage() on Buffer");
        return m_pxImage;
    }
    
    Flux_Buffer* AsBuffer() const
    {
        Zenith_Assert(m_eKind == Flux_GraphResourceKind::Buffer, "Flux_GraphResource: called AsBuffer() on Image");
        return m_pxBuffer;
    }

    void* GetVoidPtr() const
    {
        return m_eKind == Flux_GraphResourceKind::Image ? static_cast<void*>(m_pxImage) : static_cast<void*>(m_pxBuffer);
    }

    bool operator==(const Flux_GraphResource& rhs) const { return GetVoidPtr() == rhs.GetVoidPtr(); }
    bool operator!=(const Flux_GraphResource& rhs) const { return !(*this == rhs); }
    u_int64 GetHash() const { return reinterpret_cast<u_int64>(GetVoidPtr()); }

private:
    Flux_GraphResourceKind m_eKind = Flux_GraphResourceKind::Image;
    union { Flux_RenderAttachment* m_pxImage = nullptr; Flux_Buffer* m_pxBuffer; };
};

struct Flux_RenderGraph_ResourceUsage
{
    Flux_GraphResource m_xResource;
    ResourceAccess m_eAccess = RESOURCE_ACCESS_READ_SRV;
    u_int m_uMipLevel = 0;
    u_int m_uMipCount = 1;
};

struct Flux_RenderGraph_Resource
{
    Flux_GraphResource m_xResource;
    const char* m_szName = nullptr;
    u_int m_uFirstWrite = UINT32_MAX;
    u_int m_uLastRead = UINT32_MAX;
};

struct Flux_RenderGraph_ImageBarrier
{
    Flux_RenderAttachment* m_pxAttachment = nullptr;
    ResourceAccess m_ePrevAccess = RESOURCE_ACCESS_UNDEFINED;
    ResourceAccess m_eNewAccess = RESOURCE_ACCESS_READ_SRV;
    bool m_bDiscard = false;
};

struct Flux_RenderGraph_Pass
{
    const char* m_szName = nullptr;
    Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xReads;
    Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xWrites;
    Zenith_Vector<u_int> m_xExplicitDependencies;
    Flux_RenderGraph_OnRecordFunc m_pfnOnRecord = nullptr;
    Flux_RenderGraph_OnPrepareFunc m_pfnOnPrepare = nullptr;
    void* m_pUserData = nullptr;
    u_int m_uTopologicalOrder = UINT32_MAX;
    u_int m_uLevel = 0;
    bool m_bIsCompute = false;
    bool m_bEnabled = true;
    bool m_bRequestsClear = false;
    bool m_bClearTargets = false;
    const Flux_TargetSetup* m_pxTargetSetup = nullptr;
    Zenith_Vector<Flux_RenderGraph_ImageBarrier> m_xPrologueBarriers;
    Zenith_Vector<Flux_RenderGraph_ImageBarrier> m_xEpilogueBarriers;
    Flux_CommandList* m_pxCommandList = nullptr;

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

    void Read(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV, u_int uMip = 0, u_int uMipCount = 1);
    void Write(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV, u_int uMip = 0, u_int uMipCount = 1);
    void ReadBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess);
    void WriteBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess);
    void DependsOn(u_int uDependentPass, u_int uDependencyPass);
    void SetEnabled(u_int uPassIndex, bool bEnabled);
    void SetTargetSetup(u_int uPassIndex, const Flux_TargetSetup& xTargetSetup);
    void SetPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare);
    void SetClear(u_int uPassIndex, bool bClearTargets);
    bool Compile();
    void Execute();
    void MarkDirty();
    bool IsDirty() const { return m_bDirty; }
    void Clear();

    const Zenith_Vector<Flux_RenderGraph_Pass*>& GetPasses() const { return m_xPasses; }
    const Zenith_Vector<u_int>& GetExecutionOrder() const { return m_xExecutionOrder; }
    const Zenith_Vector<u_int>& GetLevelStarts() const { return m_xLevelStarts; }

private:
    Zenith_Vector<Flux_RenderGraph_Pass*> m_xPasses;
    std::unordered_map<void*, Flux_RenderGraph_Resource> m_xResources;
    Zenith_Vector<u_int> m_xExecutionOrder;
    Zenith_Vector<u_int> m_xLevelStarts;
    bool m_bCompiled = false;
    bool m_bDirty = true;
    bool m_bEnabledMaskDirty = false;

    struct ResourceTraffic { Zenith_Vector<u_int> m_xWriters; Zenith_Vector<u_int> m_xReaders; };
    std::unordered_map<void*, ResourceTraffic> m_xTraffic;
    std::unordered_map<Flux_RenderAttachment*, ResourceAccess> m_xBarrierState;
    std::unordered_map<const Flux_TargetSetup*, bool> m_xSetupNeedsClear;
    std::unordered_set<const Flux_TargetSetup*> m_xSetupClearAssigned;
    std::unordered_set<u_int64> m_xEdgeSet;
    Zenith_Vector<Zenith_Vector<u_int>> m_xAdjacency;
    Zenith_Vector<u_int> m_xInDegree;
    Zenith_Vector<u_int> m_xQueue;

    Flux_RenderGraph_Pass* GetPass(u_int uIdx) const;
    void TrackResource(void* pRes, const char* szName);
    void AddResourceUsage(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount, bool bWrite);
    void AddBufferUsage(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess, bool bWrite);
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
    bool CheckDepthReadOnly(const Flux_TargetSetup& rxTargetSetup, const Flux_RenderGraph_Pass& rxPass);
    void GenerateImageBarriers(Flux_RenderGraph_Pass& rxPass, const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, bool bIsRead);
    void InitAdjacencyData();
    void BuildAdjacencyFromTraffic();
    void AddReaderWriterEdges(const Zenith_Vector<u_int>& axReaders, const Zenith_Vector<u_int>& axWriters);
    bool IsAlsoWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters);
    u_int FindBestWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters);
    void AddWriterChainEdges(const Zenith_Vector<u_int>& axWriters);
    void AddEdgeIfNew(u_int uFrom, u_int uTo);
    void AddExplicitDependencies();
    bool ComputeTopologicalOrder();
    void ComputePassLevels();
    void SortByLevel();
    void BuildLevelStarts();
    void CollectClearRequirements();
    void AssignClearFlags();

    friend void Flux_RenderGraph_RecordLevelTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex);
};

namespace std { template<> struct hash<Flux_GraphResource> { size_t operator()(const Flux_GraphResource& xRes) const { return static_cast<size_t>(xRes.GetHash()); } }; }