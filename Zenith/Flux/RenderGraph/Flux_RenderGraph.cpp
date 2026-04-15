#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Multithreading/Zenith_Multithreading.h"

void Flux_RenderGraph::AssertMutable(const char* szFn)
{
    Zenith_Assert(!m_bCompiled || m_bDirty, "Flux_RenderGraph::%s called after Compile() without MarkDirty()", szFn);
}

Flux_RenderGraph_Pass* Flux_RenderGraph::GetPass(u_int uIdx) const
{
    Zenith_Assert(uIdx < m_xPasses.GetSize(), "Flux_RenderGraph: pass index %u out of bounds", uIdx);
    return m_xPasses.Get(uIdx);
}

void Flux_RenderGraph::TrackResource(const Flux_GraphResource& xResource)
{
    void* pRes = xResource.GetVoidPtr();
    Zenith_Assert(pRes != nullptr, "Flux_RenderGraph::TrackResource: null resource");
    if (m_xResources.find(pRes) == m_xResources.end())
    {
        Flux_RenderGraph_Resource xRes;
        xRes.m_xResource = xResource;
        xRes.m_uFirstWrite = UINT32_MAX;
        xRes.m_uLastRead = UINT32_MAX;
        m_xResources[pRes] = xRes;
    }
}

u_int Flux_RenderGraph::AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord, void* pUserData, u_int uInitialCapacity)
{
    AssertMutable("AddPass");
    Zenith_Assert(szName && pfnOnRecord, "Flux_RenderGraph::AddPass: null name or callback");
    Flux_RenderGraph_Pass* pxPass = new Flux_RenderGraph_Pass();
    pxPass->m_strName = szName;
    pxPass->m_pfnOnRecord = pfnOnRecord;
    pxPass->m_pUserData = pUserData;
    pxPass->m_pxCommandList = new Flux_CommandList(szName, uInitialCapacity);
    m_xPasses.PushBack(pxPass);
    return m_xPasses.GetSize() - 1;
}

void Flux_RenderGraph::AddResourceUsage(u_int uPassIndex, const Flux_GraphResource& xResource, ResourceAccess eAccess,
                                        u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount, bool bWrite)
{
    AssertMutable(bWrite ? "Write" : "Read");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph: invalid pass index %u", uPassIndex);
    Zenith_Assert(uMipCount > 0, "Flux_RenderGraph: mip count must be > 0");
    Zenith_Assert(uLayerCount > 0, "Flux_RenderGraph: layer count must be > 0");
    TrackResource(xResource);
    Flux_RenderGraph_ResourceUsage xUsage;
    xUsage.m_xResource = xResource;
    xUsage.m_eAccess = eAccess;
    xUsage.m_uMipLevel = uMip;
    xUsage.m_uMipCount = uMipCount;
    xUsage.m_uLayer = uLayer;
    xUsage.m_uLayerCount = uLayerCount;
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    if (bWrite) pxPass->m_xWrites.PushBack(xUsage);
    else pxPass->m_xReads.PushBack(xUsage);
}

void Flux_RenderGraph::Read(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AddResourceUsage(uPassIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, 0, 1, false);
}

void Flux_RenderGraph::Write(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AddResourceUsage(uPassIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, 0, 1, true);
}

void Flux_RenderGraph::Read(u_int uPassIndex, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess,
                            u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount)
{
    AddResourceUsage(uPassIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, uLayer, uLayerCount, false);
}

void Flux_RenderGraph::Write(u_int uPassIndex, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess,
                             u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount)
{
    AddResourceUsage(uPassIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, uLayer, uLayerCount, true);
}

void Flux_RenderGraph::ReadBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess)
{
    AddResourceUsage(uPassIndex, Flux_GraphResource(xBuffer), eAccess, 0, 1, 0, 1, false);
}

void Flux_RenderGraph::WriteBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess)
{
    AddResourceUsage(uPassIndex, Flux_GraphResource(xBuffer), eAccess, 0, 1, 0, 1, true);
}

void Flux_RenderGraph::DependsOn(u_int uDependentPass, u_int uDependencyPass)
{
    AssertMutable("DependsOn");
    Zenith_Assert(uDependentPass < m_xPasses.GetSize() && uDependencyPass < m_xPasses.GetSize(), "Flux_RenderGraph::DependsOn: invalid pass index");
    Zenith_Assert(uDependentPass != uDependencyPass, "Flux_RenderGraph::DependsOn: self-dependency on pass %u", uDependentPass);
    GetPass(uDependentPass)->m_xExplicitDependencies.PushBack(uDependencyPass);
}

void Flux_RenderGraph::SetEnabled(u_int uPassIndex, bool bEnabled)
{
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph::SetEnabled: invalid pass index %u", uPassIndex);
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    if (pxPass->m_bEnabled != bEnabled) { pxPass->m_bEnabled = bEnabled; m_bEnabledMaskDirty = true; }
}

void Flux_RenderGraph::SetClear(u_int uPassIndex, bool bClearTargets)
{
    AssertMutable("SetClear");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph::SetClear: invalid pass index %u", uPassIndex);
    GetPass(uPassIndex)->m_bRequestsClear = bClearTargets;
}

void Flux_RenderGraph::SetPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare)
{
    AssertMutable("SetPrepare");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph::SetPrepare: invalid pass index %u", uPassIndex);
    GetPass(uPassIndex)->m_pfnOnPrepare = pfnOnPrepare;
}

void Flux_RenderGraph::MarkDirty() { m_bDirty = true; }

void Flux_RenderGraph::Clear()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++) delete m_xPasses.Get(i);
    m_xPasses.Clear();
    m_xResources.clear();
    m_xTraffic.clear();
    m_xBarrierState.clear();
    m_xAttachmentNeedsClear.clear();
    m_xAttachmentClearAssigned.clear();
    m_xEdgeSet.clear();
    m_xAdjacency.Clear();
    m_xInDegree.Clear();
    m_xExecutionOrder.Clear();
    m_bCompiled = false;
    m_bDirty = true;
    m_bEnabledMaskDirty = false;
}

void Flux_RenderGraph::BuildResourceTraffic()
{
    m_xTraffic.clear();
    for (u_int uPass = 0; uPass < m_xPasses.GetSize(); uPass++)
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPass);
        if (!pxPass->m_bEnabled) continue;
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xWrite = it.GetData();
            void* pRes = xWrite.m_xResource.GetVoidPtr();
            Zenith_Assert(pRes, "Flux_RenderGraph::BuildResourceTraffic: pass '%s' has null write", pxPass->m_strName.c_str());
            m_xTraffic[pRes].m_xWriters.PushBack(uPass);
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            Zenith_Assert(pRes, "Flux_RenderGraph::BuildResourceTraffic: pass '%s' has null read", pxPass->m_strName.c_str());
            m_xTraffic[pRes].m_xReaders.PushBack(uPass);
        }
    }
}

void Flux_RenderGraph::Validate() const
{
    for (auto& xPair : m_xTraffic)
    {
        if (xPair.second.m_xReaders.GetSize() > 0 && xPair.second.m_xWriters.GetSize() == 0)
        {
            auto it = m_xResources.find(xPair.first);
            const char* sz = (it != m_xResources.end()) ? it->second.m_xResource.GetName().c_str() : "<unknown>";
            Zenith_Assert(false, "Flux_RenderGraph: resource '%s' read but never written", sz);
        }
    }
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;
        bool bIsGfx = pxP->m_uNumColourAttachments > 0 || pxP->m_xDepthStencil.IsValid();
        if (bIsGfx) Zenith_Assert(pxP->m_xWrites.GetSize() > 0, "Flux_RenderGraph: graphics pass '%s' has no Write()", pxP->m_strName.c_str());
        for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
        {
            u_int d = it.GetData();
            Zenith_Assert(d < m_xPasses.GetSize(), "Flux_RenderGraph: pass '%s' invalid dep %u", pxP->m_strName.c_str(), d);
        }
        if (pxP->m_xWrites.GetSize() > 0) Zenith_Assert(pxP->m_pfnOnRecord, "Flux_RenderGraph: pass '%s' has writes but no callback", pxP->m_strName.c_str());
    }
}

void Flux_RenderGraph::InitAdjacencyData()
{
    const u_int uN = m_xPasses.GetSize();
    m_xAdjacency.Clear(); m_xAdjacency.Reserve(uN);
    m_xInDegree.Clear(); m_xInDegree.Reserve(uN);
    for (u_int i = 0; i < uN; i++) { m_xAdjacency.EmplaceBack(); m_xInDegree.PushBack(0); }
    m_xEdgeSet.clear();
}

void Flux_RenderGraph::BuildAdjacencyFromTraffic()
{
    for (auto& xP : m_xTraffic)
    {
        const Zenith_Vector<u_int>& xWriters = xP.second.m_xWriters;
        const Zenith_Vector<u_int>& xReaders = xP.second.m_xReaders;
        AddReaderWriterEdges(xReaders, xWriters);
        AddWriterChainEdges(xWriters);
    }
}

void Flux_RenderGraph::AddReaderWriterEdges(const Zenith_Vector<u_int>& axReaders, const Zenith_Vector<u_int>& axWriters)
{
    for (Zenith_Vector<u_int>::Iterator itR(axReaders); !itR.Done(); itR.Next())
    {
        u_int uReader = itR.GetData();
        if (IsAlsoWriter(uReader, axWriters)) continue;
        u_int uBestWriter = FindBestWriter(uReader, axWriters);
        if (uBestWriter != UINT32_MAX) AddEdgeIfNew(uBestWriter, uReader);
    }
}

bool Flux_RenderGraph::IsAlsoWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters)
{
    for (Zenith_Vector<u_int>::Iterator itW(axWriters); !itW.Done(); itW.Next())
        if (itW.GetData() == uReader) return true;
    return false;
}

u_int Flux_RenderGraph::FindBestWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters)
{
    u_int uBestWriter = UINT32_MAX;
    for (Zenith_Vector<u_int>::Iterator itW(axWriters); !itW.Done(); itW.Next())
    {
        u_int uWriter = itW.GetData();
        if (uWriter >= uReader) break;
        uBestWriter = uWriter;
    }
    return uBestWriter;
}

void Flux_RenderGraph::AddWriterChainEdges(const Zenith_Vector<u_int>& axWriters)
{
    for (u_int w = 0; w + 1 < axWriters.GetSize(); w++)
    {
        u_int64 ulKey = (static_cast<u_int64>(axWriters.Get(w)) << 32) | axWriters.Get(w + 1);
        if (m_xEdgeSet.find(ulKey) == m_xEdgeSet.end())
        {
            m_xEdgeSet.insert(ulKey);
            m_xAdjacency.Get(axWriters.Get(w)).PushBack(axWriters.Get(w + 1));
            m_xInDegree.Get(axWriters.Get(w + 1))++;
        }
    }
}

void Flux_RenderGraph::AddEdgeIfNew(u_int uFrom, u_int uTo)
{
    u_int64 ulKey = (static_cast<u_int64>(uFrom) << 32) | uTo;
    if (m_xEdgeSet.find(ulKey) == m_xEdgeSet.end())
    {
        m_xEdgeSet.insert(ulKey);
        m_xAdjacency.Get(uFrom).PushBack(uTo);
        m_xInDegree.Get(uTo)++;
    }
}

void Flux_RenderGraph::AddExplicitDependencies()
{
    const u_int uN = m_xPasses.GetSize();
    for (u_int i = 0; i < uN; i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;
        for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
        {
            u_int d = it.GetData();
            if (m_xPasses.Get(d)->m_bEnabled) AddEdgeIfNew(d, i);
        }
    }
}

bool Flux_RenderGraph::TopologicalSort()
{
    InitAdjacencyData();
    BuildAdjacencyFromTraffic();
    AddExplicitDependencies();
    
    m_xExecutionOrder.Clear();
    Zenith_Vector<u_int> xQueue;
    const u_int uN = m_xPasses.GetSize();
    
    for (u_int i = 0; i < uN; i++)
        if (m_xPasses.Get(i)->m_bEnabled && m_xInDegree.Get(i) == 0)
            xQueue.PushBack(i);
    
    u_int uProcessed = 0;
    for (u_int qf = 0; qf < xQueue.GetSize(); qf++)
    {
        u_int uCurrent = xQueue.Get(qf);
        uProcessed++;
        m_xExecutionOrder.PushBack(uCurrent);
        m_xPasses.Get(uCurrent)->m_uTopologicalOrder = m_xExecutionOrder.GetSize() - 1;
        for (Zenith_Vector<u_int>::Iterator it(m_xAdjacency.Get(uCurrent)); !it.Done(); it.Next())
        {
            u_int uNeighbor = it.GetData();
            if (--m_xInDegree.Get(uNeighbor) == 0)
                xQueue.PushBack(uNeighbor);
        }
    }
    
    u_int uEnabledCount = 0;
    for (u_int i = 0; i < uN; i++)
        if (m_xPasses.Get(i)->m_bEnabled) uEnabledCount++;
    
    if (uProcessed != uEnabledCount)
    {
        Zenith_Assert(false, "Flux_RenderGraph: Cycle detected!");
        return false;
    }
    
    return true;
}

void Flux_RenderGraph::ComputeResourceLifetimes()
{
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xWrite = it.GetData();
            void* pRes = xWrite.m_xResource.GetVoidPtr();
            auto itRes = m_xResources.find(pRes);
            Zenith_Assert(itRes != m_xResources.end(), "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (uPassIdx < itRes->second.m_uFirstWrite) itRes->second.m_uFirstWrite = uPassIdx;
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            auto itRes = m_xResources.find(pRes);
            Zenith_Assert(itRes != m_xResources.end(), "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (itRes->second.m_uLastRead == UINT32_MAX || uPassIdx > itRes->second.m_uLastRead)
                itRes->second.m_uLastRead = uPassIdx;
        }
    }
}

void Flux_RenderGraph::GenerateBarriers()
{
    m_xBarrierState.clear();
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        pxPass->m_xPrologueBarriers.Clear();
        pxPass->m_xEpilogueBarriers.Clear();
        GenerateImageBarriers(*pxPass, pxPass->m_xReads, true);
        GenerateImageBarriers(*pxPass, pxPass->m_xWrites, false);
    }
}

// Pack a (pointer, mip, layer) triple into a single u_int64 for use as a
// barrier-state map key. The low 48 bits hold the resource pointer, the next
// 8 bits the mip index, and the top 8 bits the layer index. 48 bits is
// sufficient for all pointers on current x64 / ARM64 systems, and the mip /
// layer slots comfortably cover anything we emit (max 15 mips, 6 faces).
static inline u_int64 MakeBarrierKey(void* pRes, u_int uMip, u_int uLayer)
{
    return (reinterpret_cast<u_int64>(pRes) & 0x0000FFFFFFFFFFFFull)
         | (static_cast<u_int64>(uMip   & 0xFFu) << 48)
         | (static_cast<u_int64>(uLayer & 0xFFu) << 56);
}

void Flux_RenderGraph::GenerateImageBarriers(Flux_RenderGraph_Pass& rxPass, const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, bool bIsRead)
{
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxUsages); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
        if (!rxUsage.m_xResource.IsImageLike()) continue;
        void* pRes = rxUsage.m_xResource.GetVoidPtr();

        // Resolve the concrete (mip, layer) range. Sentinel values get
        // expanded against the surface's actual mip / layer counts so that
        // barrier tracking is subresource-accurate.
        const Flux_SurfaceInfo& rxInfo = rxUsage.m_xResource.GetSurfaceInfo();
        const u_int uTotalMips   = rxInfo.m_uNumMips;
        const u_int uTotalLayers = rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::ImageCube ? 6u : rxInfo.m_uNumLayers;

        const u_int uMipStart   = rxUsage.m_uMipLevel;
        const u_int uMipCount   = (rxUsage.m_uMipCount == FLUX_RG_ALL_MIPS)   ? (uTotalMips   - uMipStart)   : rxUsage.m_uMipCount;
        const u_int uLayerStart = rxUsage.m_uLayer;
        const u_int uLayerCount = (rxUsage.m_uLayerCount == FLUX_RG_ALL_LAYERS) ? (uTotalLayers - uLayerStart) : rxUsage.m_uLayerCount;

        Zenith_Assert(uMipStart + uMipCount <= uTotalMips, "Flux_RenderGraph: mip range [%u, %u) exceeds mip count %u", uMipStart, uMipStart + uMipCount, uTotalMips);
        Zenith_Assert(uLayerStart + uLayerCount <= uTotalLayers, "Flux_RenderGraph: layer range [%u, %u) exceeds layer count %u", uLayerStart, uLayerStart + uLayerCount, uTotalLayers);

        // Walk every (mip, layer) pair the usage covers. Emit one barrier
        // per distinct ePrev within a contiguous (mip, layer) run. The
        // simplest first-pass grouping is per-pair; passes that cover a
        // whole mip chain or whole cube still end up with correct layouts,
        // just with more barriers than strictly necessary. Future
        // optimisation: merge runs of identical ePrev into a single barrier
        // with a wider subresource range.
        for (u_int uMip = uMipStart; uMip < uMipStart + uMipCount; uMip++)
        {
            for (u_int uLayer = uLayerStart; uLayer < uLayerStart + uLayerCount; uLayer++)
            {
                u_int64 ulKey = MakeBarrierKey(pRes, uMip, uLayer);
                auto itState = m_xBarrierState.find(ulKey);
                const bool bFirst = itState == m_xBarrierState.end();
                const ResourceAccess ePrev = bFirst ? RESOURCE_ACCESS_UNDEFINED : itState->second;
                if (bFirst || ePrev != rxUsage.m_eAccess)
                {
                    Flux_RenderGraph_ImageBarrier xB;
                    xB.m_xResource = rxUsage.m_xResource;
                    xB.m_ePrevAccess = ePrev;
                    xB.m_eNewAccess = rxUsage.m_eAccess;
                    xB.m_uMipLevel = uMip;
                    xB.m_uMipCount = 1;
                    xB.m_uLayer = uLayer;
                    xB.m_uLayerCount = 1;
                    xB.m_bDiscard = bFirst || (!bIsRead && rxPass.m_bClearTargets);
                    rxPass.m_xPrologueBarriers.PushBack(xB);
                }
                m_xBarrierState[ulKey] = rxUsage.m_eAccess;
            }
        }
    }
}

void Flux_RenderGraph::ResolveClearFlags()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++) m_xPasses.Get(i)->m_bClearTargets = false;
    CollectClearRequirements();
    AssignClearFlags();
}

void Flux_RenderGraph::CollectClearRequirements()
{
    // Clear tracking is keyed on (void*, uMip, uLayer) using MakeBarrierKey so
    // that two passes writing (mip=0, face=0) and (mip=1, face=2) of the same
    // cube image each get independent clear-requirement slots. The ptr-only
    // scheme used previously collapsed the whole cube into one slot, which
    // broke the 42-prefilter-pass IBL case: only the first writer saw the
    // clear-request, and the other 41 passes' (mip, face) subresources stayed
    // UNDEFINED at creation but the backend tried to transition them from
    // SHADER_READ_ONLY_OPTIMAL.
    m_xAttachmentNeedsClear.clear();
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bIsCompute || !pxPass->m_bEnabled) continue;
        if (pxPass->m_uNumColourAttachments == 0 && !pxPass->m_xDepthStencil.IsValid()) continue;
        for (uint32_t i = 0; i < pxPass->m_uNumColourAttachments; i++)
        {
            const Flux_RenderGraph_AttachmentRef& rxRef = pxPass->m_axColourAttachments[i];
            u_int64 ulKey = MakeBarrierKey(rxRef.m_xResource.GetVoidPtr(), rxRef.m_uMip, rxRef.m_uLayer);
            if (m_xAttachmentNeedsClear.find(ulKey) == m_xAttachmentNeedsClear.end())
                m_xAttachmentNeedsClear[ulKey] = pxPass->m_bRequestsClear;
            else
                m_xAttachmentNeedsClear[ulKey] = m_xAttachmentNeedsClear[ulKey] || pxPass->m_bRequestsClear;
        }
        if (pxPass->m_xDepthStencil.IsValid())
        {
            const Flux_RenderGraph_AttachmentRef& rxDepth = pxPass->m_xDepthStencil;
            u_int64 ulKey = MakeBarrierKey(rxDepth.m_xResource.GetVoidPtr(), rxDepth.m_uMip, rxDepth.m_uLayer);
            if (m_xAttachmentNeedsClear.find(ulKey) == m_xAttachmentNeedsClear.end())
                m_xAttachmentNeedsClear[ulKey] = pxPass->m_bRequestsClear;
            else
                m_xAttachmentNeedsClear[ulKey] = m_xAttachmentNeedsClear[ulKey] || pxPass->m_bRequestsClear;
        }
    }
}

void Flux_RenderGraph::AssignClearFlags()
{
    m_xAttachmentClearAssigned.clear();
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bIsCompute || !pxPass->m_bEnabled) continue;
        if (pxPass->m_uNumColourAttachments == 0 && !pxPass->m_xDepthStencil.IsValid()) continue;
        bool bNeedsClear = false;
        for (uint32_t i = 0; i < pxPass->m_uNumColourAttachments; i++)
        {
            const Flux_RenderGraph_AttachmentRef& rxRef = pxPass->m_axColourAttachments[i];
            u_int64 ulKey = MakeBarrierKey(rxRef.m_xResource.GetVoidPtr(), rxRef.m_uMip, rxRef.m_uLayer);
            auto itNeed = m_xAttachmentNeedsClear.find(ulKey);
            if (itNeed != m_xAttachmentNeedsClear.end() && itNeed->second) { bNeedsClear = true; break; }
        }
        if (!bNeedsClear && pxPass->m_xDepthStencil.IsValid())
        {
            const Flux_RenderGraph_AttachmentRef& rxDepth = pxPass->m_xDepthStencil;
            u_int64 ulKey = MakeBarrierKey(rxDepth.m_xResource.GetVoidPtr(), rxDepth.m_uMip, rxDepth.m_uLayer);
            auto itNeed = m_xAttachmentNeedsClear.find(ulKey);
            if (itNeed != m_xAttachmentNeedsClear.end() && itNeed->second) bNeedsClear = true;
        }
        if (!bNeedsClear) continue;
        bool bIsFirstWriter = false;
        for (uint32_t i = 0; i < pxPass->m_uNumColourAttachments; i++)
        {
            const Flux_RenderGraph_AttachmentRef& rxRef = pxPass->m_axColourAttachments[i];
            u_int64 ulKey = MakeBarrierKey(rxRef.m_xResource.GetVoidPtr(), rxRef.m_uMip, rxRef.m_uLayer);
            if (m_xAttachmentClearAssigned.find(ulKey) == m_xAttachmentClearAssigned.end())
            { m_xAttachmentClearAssigned.insert(ulKey); bIsFirstWriter = true; }
        }
        if (pxPass->m_xDepthStencil.IsValid())
        {
            const Flux_RenderGraph_AttachmentRef& rxDepth = pxPass->m_xDepthStencil;
            u_int64 ulKey = MakeBarrierKey(rxDepth.m_xResource.GetVoidPtr(), rxDepth.m_uMip, rxDepth.m_uLayer);
            if (m_xAttachmentClearAssigned.find(ulKey) == m_xAttachmentClearAssigned.end())
            { m_xAttachmentClearAssigned.insert(ulKey); bIsFirstWriter = true; }
        }
        pxPass->m_bClearTargets = bIsFirstWriter;
    }
}

bool Flux_RenderGraph::Compile()
{
    if (!m_bDirty) return m_bCompiled;
    if (m_xPasses.GetSize() == 0) { m_bCompiled = true; m_bDirty = false; m_bEnabledMaskDirty = false; return true; }
    BuildResourceTraffic();
    Validate();
    if (!TopologicalSort()) return false;
    ComputeResourceLifetimes();
    InferPassAttachments();
    MarkPassesAsComputeOrGraphics();
    ResolveClearFlags();
    GenerateBarriers();
    m_bCompiled = true; m_bDirty = false; m_bEnabledMaskDirty = false;
#ifdef ZENITH_DEBUG
    Zenith_Log(LOG_CATEGORY_RENDERER, "RenderGraph: Compiled %u passes", m_xExecutionOrder.GetSize());
#endif
    return true;
}

void Flux_RenderGraph::MarkPassesAsComputeOrGraphics()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        pxP->m_bIsCompute = (pxP->m_uNumColourAttachments == 0 && !pxP->m_xDepthStencil.IsValid());
    }
}

struct Flux_RenderGraph_RecordTaskData { Flux_RenderGraph* m_pxGraph; u_int m_uPassIndex; };

void Flux_RenderGraph_RecordPassTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex)
{
    auto* pxData = static_cast<Flux_RenderGraph_RecordTaskData*>(pData);
    Flux_RenderGraph* pxGraph = pxData[uInvocationIndex].m_pxGraph;
    const u_int uPassIndex = pxData[uInvocationIndex].m_uPassIndex;
    (void)uWorkerIndex;
    const Zenith_Vector<u_int>& xExecOrder = pxGraph->GetExecutionOrder();
    const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = pxGraph->GetPasses();
    Flux_RenderGraph_Pass& xPass = *xPasses.Get(xExecOrder.Get(uPassIndex));
    xPass.m_pxCommandList->Reset();
    if (!xPass.m_bEnabled || !xPass.m_pfnOnRecord) return;
    xPass.m_pfnOnRecord(xPass.m_pxCommandList, xPass.m_pUserData);
}

void Flux_RenderGraph::Execute()
{
    Zenith_Assert(m_bCompiled, "Flux_RenderGraph::Execute: must call Compile() first");
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::Execute: must be called from main thread");
    if (m_xExecutionOrder.GetSize() == 0) return;
    if (m_bEnabledMaskDirty) { ResolveClearFlags(); m_bEnabledMaskDirty = false; }
    CallPrepareCallbacks();
    RecordCommandLists();
    SubmitRecordedLists();
}

void Flux_RenderGraph::CallPrepareCallbacks()
{
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bEnabled && pxPass->m_pfnOnPrepare) pxPass->m_pfnOnPrepare(pxPass->m_pUserData);
    }
}

void Flux_RenderGraph::RecordCommandLists()
{
    const u_int uNumPasses = m_xExecutionOrder.GetSize();
    if (uNumPasses == 0) return;
    
    auto* pxTaskData = static_cast<Flux_RenderGraph_RecordTaskData*>(Zenith_MemoryManagement::Allocate(sizeof(Flux_RenderGraph_RecordTaskData) * uNumPasses));
    for (u_int i = 0; i < uNumPasses; i++)
    {
        pxTaskData[i].m_pxGraph = this;
        pxTaskData[i].m_uPassIndex = i;
    }
    
    Zenith_TaskArray xTasks(ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS, Flux_RenderGraph_RecordPassTask, pxTaskData, uNumPasses, true);
    Zenith_TaskSystem::SubmitTaskArray(&xTasks);
    xTasks.WaitUntilComplete();
    Zenith_MemoryManagement::Deallocate(pxTaskData);
}

void Flux_RenderGraph::SubmitRecordedLists()
{
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (!pxPass->m_bEnabled || !pxPass->m_pfnOnRecord) continue;
        if (pxPass->m_pxCommandList->GetCommandCount() == 0 && !pxPass->m_bClearTargets) continue;
        bool bDepthReadOnly = false;
        if (pxPass->m_xDepthStencil.IsValid())
        {
            void* pDepthRes = pxPass->m_xDepthStencil.m_xResource.GetVoidPtr();
            for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator itR(pxPass->m_xReads); !itR.Done(); itR.Next())
            {
                if (itR.GetData().m_xResource.GetVoidPtr() == pDepthRes) { bDepthReadOnly = true; break; }
            }
        }
        Flux::SubmitCommandList(pxPass->m_pxCommandList,
            pxPass->m_axColourAttachments, pxPass->m_uNumColourAttachments,
            pxPass->m_xDepthStencil,
            pxPass->m_bClearTargets, bDepthReadOnly, pxPass);
    }
}

void Flux_RenderGraph::InferPassAttachments()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(i);
        pxPass->m_uNumColourAttachments = 0;
        pxPass->m_xDepthStencil = Flux_RenderGraph_AttachmentRef();
        for (uint32_t t = 0; t < FLUX_MAX_TARGETS; t++)
            pxPass->m_axColourAttachments[t] = Flux_RenderGraph_AttachmentRef();
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
            if (!rxUsage.m_xResource.IsImageLike()) continue;
            if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_RTV)
            {
                if (pxPass->m_uNumColourAttachments < FLUX_MAX_TARGETS)
                {
                    pxPass->m_axColourAttachments[pxPass->m_uNumColourAttachments++] =
                        Flux_RenderGraph_AttachmentRef(rxUsage.m_xResource, rxUsage.m_uMipLevel, rxUsage.m_uLayer);
                }
            }
            else if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_DSV)
            {
                // Depth attachments are never cubemaps in this engine — they always
                // bind as a 2D depth/stencil view. Assert to catch accidental misuse.
                Zenith_Assert(rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Image,
                    "Flux_RenderGraph: depth/stencil writes require a 2D Flux_RenderAttachment (pass '%s')", pxPass->m_strName.c_str());
                pxPass->m_xDepthStencil =
                    Flux_RenderGraph_AttachmentRef(rxUsage.m_xResource, rxUsage.m_uMipLevel, rxUsage.m_uLayer);
            }
        }
    }
}

void Flux_RenderGraph_Pass::EmitPrologueBarriers(vk::CommandBuffer xCmdBuf, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage) const
{
    if (m_xPrologueBarriers.GetSize() == 0) return;

    std::vector<vk::ImageMemoryBarrier> axBarriers;
    axBarriers.reserve(m_xPrologueBarriers.GetSize());

    for (Zenith_Vector<Flux_RenderGraph_ImageBarrier>::Iterator it(m_xPrologueBarriers); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_ImageBarrier& rxBarrier = it.GetData();
        if (!rxBarrier.m_xResource.IsImageLike()) continue;

        Flux_VRAMHandle xVRAMHandle = rxBarrier.m_xResource.GetVRAMHandle();
        if (xVRAMHandle.AsUInt() == UINT32_MAX) continue;

        Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
        if (!pxVRAM) continue;

        vk::ImageLayout eOldLayout;
        vk::ImageLayout eNewLayout;
        vk::AccessFlags eSrcAccess;
        vk::AccessFlags eDstAccess;

        switch (rxBarrier.m_ePrevAccess)
        {
        case RESOURCE_ACCESS_UNDEFINED:
            eOldLayout = vk::ImageLayout::eUndefined;
            eSrcAccess = vk::AccessFlags();
            break;
        case RESOURCE_ACCESS_READ_SRV:
            eOldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            eSrcAccess = vk::AccessFlagBits::eShaderRead;
            break;
        case RESOURCE_ACCESS_READ_DEPTH:
            eOldLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
            eSrcAccess = vk::AccessFlagBits::eDepthStencilAttachmentRead;
            break;
        case RESOURCE_ACCESS_WRITE_RTV:
            eOldLayout = vk::ImageLayout::eColorAttachmentOptimal;
            eSrcAccess = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case RESOURCE_ACCESS_WRITE_DSV:
            eOldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            eSrcAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case RESOURCE_ACCESS_WRITE_UAV:
            eOldLayout = vk::ImageLayout::eGeneral;
            eSrcAccess = vk::AccessFlagBits::eShaderWrite;
            break;
        default:
            eOldLayout = vk::ImageLayout::eUndefined;
            eSrcAccess = vk::AccessFlags();
            break;
        }

        switch (rxBarrier.m_eNewAccess)
        {
        case RESOURCE_ACCESS_READ_SRV:
            eNewLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            eDstAccess = vk::AccessFlagBits::eShaderRead;
            break;
        case RESOURCE_ACCESS_READ_DEPTH:
            eNewLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
            eDstAccess = vk::AccessFlagBits::eDepthStencilAttachmentRead;
            break;
        case RESOURCE_ACCESS_WRITE_RTV:
            eNewLayout = vk::ImageLayout::eColorAttachmentOptimal;
            eDstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case RESOURCE_ACCESS_WRITE_DSV:
            eNewLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            eDstAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case RESOURCE_ACCESS_WRITE_UAV:
            eNewLayout = vk::ImageLayout::eGeneral;
            eDstAccess = vk::AccessFlagBits::eShaderWrite;
            break;
        default:
            eNewLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            eDstAccess = vk::AccessFlagBits::eShaderRead;
            break;
        }

        // Aspect: depth transitions use Depth, everything else Color. Cube faces
        // are always colour — depth cubemaps aren't used in this engine.
        const bool bIsDepth = (rxBarrier.m_eNewAccess == RESOURCE_ACCESS_WRITE_DSV
                            || rxBarrier.m_eNewAccess == RESOURCE_ACCESS_READ_DEPTH
                            || rxBarrier.m_ePrevAccess == RESOURCE_ACCESS_WRITE_DSV
                            || rxBarrier.m_ePrevAccess == RESOURCE_ACCESS_READ_DEPTH);
        const vk::ImageAspectFlags eAspect = bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

        // Map FLUX_RG_ALL_* sentinels to VK_REMAINING_* so "whole image" usages
        // continue to transition the full subresource set without per-subresource
        // fan-out (which GenerateImageBarriers already avoided at declaration time
        // by emitting 1x1 per-pair barriers when ranges were finite).
        const uint32_t uMipLevel   = rxBarrier.m_uMipLevel;
        const uint32_t uMipCount   = (rxBarrier.m_uMipCount == FLUX_RG_ALL_MIPS)     ? VK_REMAINING_MIP_LEVELS   : rxBarrier.m_uMipCount;
        const uint32_t uLayerStart = rxBarrier.m_uLayer;
        const uint32_t uLayerCount = (rxBarrier.m_uLayerCount == FLUX_RG_ALL_LAYERS) ? VK_REMAINING_ARRAY_LAYERS : rxBarrier.m_uLayerCount;

        vk::ImageSubresourceRange xSubRange(eAspect, uMipLevel, uMipCount, uLayerStart, uLayerCount);

        axBarriers.push_back(vk::ImageMemoryBarrier()
            .setSrcAccessMask(eSrcAccess)
            .setDstAccessMask(eDstAccess)
            .setOldLayout(eOldLayout)
            .setNewLayout(eNewLayout)
            .setImage(pxVRAM->GetImage())
            .setSubresourceRange(xSubRange));
    }

    if (!axBarriers.empty())
    {
        xCmdBuf.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(),
            0, nullptr, 0, nullptr,
            static_cast<uint32_t>(axBarriers.size()), axBarriers.data());
    }
}