#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Zenith_PlatformGraphics_Include.h"
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
#ifdef ZENITH_TOOLS
    pxPass->m_strName = szName;
#endif
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
    m_xAttachmentNeedsClear.clear();
    m_xAttachmentClearAssigned.clear();
    m_xEdgeSet.clear();
    m_xAdjacency.Clear();
    m_xInDegree.Clear();
    m_xExecutionOrder.Clear();
    DestroyTransients();
    m_bCompiled = false;
    m_bDirty = true;
    m_bEnabledMaskDirty = false;
}

// ---- Transient resource management ----------------------------------------

Flux_TransientHandle Flux_RenderGraph::CreateTransient(const Flux_TransientTextureDesc& xDesc)
{
    AssertMutable("CreateTransient");
    Zenith_Assert(xDesc.m_uWidth > 0 && xDesc.m_uHeight > 0, "Flux_RenderGraph::CreateTransient: zero-size texture");

    TransientResource* pxTransient = new TransientResource();
    pxTransient->m_xDesc = xDesc;
    pxTransient->m_bAllocated = false;

    Flux_TransientHandle xHandle;
    xHandle.m_uIndex = m_axTransients.GetSize();
    m_axTransients.PushBack(pxTransient);
    return xHandle;
}

void Flux_RenderGraph::ReadTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess)
{
    Zenith_Assert(xHandle.IsValid() && xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::ReadTransient: invalid handle");
    Read(uPassIndex, m_axTransients.Get(xHandle.m_uIndex)->m_xAttachment, eAccess);
}

void Flux_RenderGraph::WriteTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess)
{
    Zenith_Assert(xHandle.IsValid() && xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::WriteTransient: invalid handle");
    Write(uPassIndex, m_axTransients.Get(xHandle.m_uIndex)->m_xAttachment, eAccess);
}

Flux_RenderAttachment& Flux_RenderGraph::GetTransientAttachment(Flux_TransientHandle xHandle)
{
    Zenith_Assert(xHandle.IsValid() && xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::GetTransientAttachment: invalid handle %u (size %u)", xHandle.m_uIndex, m_axTransients.GetSize());
    // NOTE: m_bAllocated may be false during SetupRenderGraph — transients are allocated
    // in Compile() AFTER all passes are registered. The returned pointer is stable
    // (heap-allocated TransientResource) so the graph can safely store it for resource
    // tracking. VRAM validity should be asserted at render time, not here.
    return m_axTransients.Get(xHandle.m_uIndex)->m_xAttachment;
}

const Flux_RenderAttachment& Flux_RenderGraph::GetTransientAttachment(Flux_TransientHandle xHandle) const
{
    Zenith_Assert(xHandle.IsValid() && xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::GetTransientAttachment: invalid handle %u (size %u) (const)", xHandle.m_uIndex, m_axTransients.GetSize());
    return m_axTransients.Get(xHandle.m_uIndex)->m_xAttachment;
}

void Flux_RenderGraph::AllocateTransients()
{
    // Allocate backing Flux_RenderAttachment for each transient that isn't yet allocated.
    // Future: pack non-overlapping lifetimes into shared VMA heaps for aliasing.
    for (u_int i = 0; i < m_axTransients.GetSize(); i++)
    {
        TransientResource* pxT = m_axTransients.Get(i);
        if (pxT->m_bAllocated) continue;

        Flux_RenderAttachmentBuilder xBuilder;
        xBuilder.m_uWidth = pxT->m_xDesc.m_uWidth;
        xBuilder.m_uHeight = pxT->m_xDesc.m_uHeight;
        xBuilder.m_eFormat = pxT->m_xDesc.m_eFormat;
        xBuilder.m_uNumMips = pxT->m_xDesc.m_uNumMips;
        xBuilder.m_uMemoryFlags = pxT->m_xDesc.m_uMemoryFlags;
        xBuilder.m_eTextureType = pxT->m_xDesc.m_eTextureType;
        xBuilder.m_uDepth = pxT->m_xDesc.m_uDepth;

        if (pxT->m_xDesc.m_bIsDepthStencil)
            xBuilder.BuildDepthStencil(pxT->m_xAttachment, "Transient DS");
        else
            xBuilder.BuildColour(pxT->m_xAttachment, "Transient");

        Zenith_Assert(pxT->m_xAttachment.m_xVRAMHandle.IsValid(),
            "Flux_RenderGraph::AllocateTransients: allocation failed for transient %u", i);
        pxT->m_bAllocated = true;
    }
}

void Flux_RenderGraph::DestroyTransients()
{
    for (u_int i = 0; i < m_axTransients.GetSize(); i++)
    {
        TransientResource* pxT = m_axTransients.Get(i);
        if (pxT->m_bAllocated)
        {
            Flux_RenderAttachmentBuilder::Destroy(pxT->m_xAttachment);
        }
        delete pxT;
    }
    m_axTransients.Clear();
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
            Zenith_Assert(pRes, "Flux_RenderGraph::BuildResourceTraffic: pass '%s' has null write", pxPass->DebugName());
            m_xTraffic[pRes].m_xWriters.PushBack(uPass);
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            Zenith_Assert(pRes, "Flux_RenderGraph::BuildResourceTraffic: pass '%s' has null read", pxPass->DebugName());
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
            // Log which passes read this orphaned resource to help identify it
            Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_RenderGraph: resource '%s' (ptr=%p) read but never written. Reader passes:",
                sz, xPair.first);
            for (Zenith_Vector<u_int>::Iterator itR(xPair.second.m_xReaders); !itR.Done(); itR.Next())
            {
                Flux_RenderGraph_Pass* pxReaderPass = m_xPasses.Get(itR.GetData());
                Zenith_Error(LOG_CATEGORY_RENDERER, "  - Pass %u: '%s'", itR.GetData(), pxReaderPass->DebugName());
            }
            Zenith_Assert(false, "Flux_RenderGraph: resource '%s' (ptr=%p) read but never written (see reader pass list above)",
                sz, xPair.first);
        }
    }
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;
        bool bIsGfx = pxP->m_uNumColourAttachments > 0 || pxP->m_xDepthStencil.IsValid();
        if (bIsGfx) Zenith_Assert(pxP->m_xWrites.GetSize() > 0, "Flux_RenderGraph: graphics pass '%s' has no Write()", pxP->DebugName());
        for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
        {
            u_int d = it.GetData();
            Zenith_Assert(d < m_xPasses.GetSize(), "Flux_RenderGraph: pass '%s' invalid dep %u", pxP->DebugName(), d);
        }
        if (pxP->m_xWrites.GetSize() > 0) Zenith_Assert(pxP->m_pfnOnRecord, "Flux_RenderGraph: pass '%s' has writes but no callback", pxP->DebugName());
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

// Pack a (pointer, mip, layer) triple into a single u_int64 for use as a
// subresource key. Used by clear-flag tracking to grant "first writer" status
// per (attachment, mip, face). Low 48 bits = pointer, bits 48-55 = mip, bits 56-63 = layer.
static inline u_int64 MakeBarrierKey(void* pRes, u_int uMip, u_int uLayer)
{
    return (reinterpret_cast<u_int64>(pRes) & 0x0000FFFFFFFFFFFFull)
         | (static_cast<u_int64>(uMip   & 0xFFu) << 48)
         | (static_cast<u_int64>(uLayer & 0xFFu) << 56);
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
    AllocateTransients();
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
                    "Flux_RenderGraph: depth/stencil writes require a 2D Flux_RenderAttachment (pass '%s')", pxPass->DebugName());
                pxPass->m_xDepthStencil =
                    Flux_RenderGraph_AttachmentRef(rxUsage.m_xResource, rxUsage.m_uMipLevel, rxUsage.m_uLayer);
            }
        }
    }
}

// EmitPrologueBarriers was deleted — see TODO in Flux_RenderGraph.h.
// Barriers are computed by RecordCommandBuffersTask in the Vulkan backend.