#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"

//=============================================================================
// RenderGraph Compilation
//
// Extracted from Flux_RenderGraph.cpp: validation + adjacency + topological
// sort. These run during Compile() only, never on the hot path.
//
// All shared state (m_xPasses, m_xTraffic, m_xResources, m_axTransients,
// m_xAdjacency, m_xInDegree, m_xEdgeSet, m_xExecutionOrder) is reachable
// via member access because every function is a class method. The only
// file-static helper needed (AccessToString) is copied here rather than
// exposed through an internal header — it's a tiny switch-on-enum.
//=============================================================================

// Local copy of the file-static helper defined in Flux_RenderGraph.cpp. Kept
// here to avoid introducing a shared internal header for a 10-line function.
static const char* AccessToString(ResourceAccess eAccess)
{
    switch (eAccess)
    {
        case RESOURCE_ACCESS_UNDEFINED:      return "UNDEFINED";
        case RESOURCE_ACCESS_READ_SRV:       return "READ_SRV";
        case RESOURCE_ACCESS_READ_DEPTH:     return "READ_DEPTH";
        case RESOURCE_ACCESS_WRITE_RTV:      return "WRITE_RTV";
        case RESOURCE_ACCESS_WRITE_DSV:      return "WRITE_DSV";
        case RESOURCE_ACCESS_WRITE_UAV:      return "WRITE_UAV";
        case RESOURCE_ACCESS_READWRITE_UAV:  return "READWRITE_UAV";
    }
    return "???";
}

void Flux_RenderGraph::ValidateOrphanedReads() const
{
    for (auto& xPair : m_xTraffic)
    {
        if (xPair.second.m_xReaders.GetSize() == 0 || xPair.second.m_xWriters.GetSize() != 0) continue;

        auto it = m_xResources.find(xPair.first);
        const char* sz = (it != m_xResources.end()) ? it->second.m_xResource.GetName().c_str() : "<unknown>";
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

// Transients must appear in at least one Read/Write across ALL passes (enabled
// or not). Scans raw pass lists rather than m_xTraffic — disabled fog variants
// still own their transients legally; what's NOT legal is declaring a transient
// no pass references at all (typically a wiring mistake in SetupRenderGraph).
void Flux_RenderGraph::ValidateUnusedTransients() const
{
    u_int uUnusedTransients = 0;
    for (u_int i = 0; i < m_axTransients.GetSize(); i++)
    {
        const TransientResource* pxT = m_axTransients.Get(i);
        const void* pResConst = static_cast<const void*>(&pxT->m_xAttachment);
        bool bReferenced = false;
        for (u_int p = 0; p < m_xPasses.GetSize() && !bReferenced; p++)
        {
            const Flux_RenderGraph_Pass* pxP = m_xPasses.Get(p);
            for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxP->m_xReads); !it.Done(); it.Next())
            {
                if (it.GetData().m_xResource.GetVoidPtr() == pResConst) { bReferenced = true; break; }
            }
            if (bReferenced) break;
            for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxP->m_xWrites); !it.Done(); it.Next())
            {
                if (it.GetData().m_xResource.GetVoidPtr() == pResConst) { bReferenced = true; break; }
            }
        }
        if (!bReferenced)
        {
            Zenith_Error(LOG_CATEGORY_RENDERER,
                "Flux_RenderGraph::Validate: transient #%u (%ux%ux%u fmt=%d depth=%d mips=%u memFlags=0x%x) declared via CreateTransient but no pass (enabled or not) references it",
                i, pxT->m_xDesc.m_uWidth, pxT->m_xDesc.m_uHeight, pxT->m_xDesc.m_uDepth,
                (int)pxT->m_xDesc.m_eFormat, (int)pxT->m_xDesc.m_bIsDepthStencil,
                pxT->m_xDesc.m_uNumMips, pxT->m_xDesc.m_uMemoryFlags);
            uUnusedTransients++;
        }
    }
    Zenith_Assert(uUnusedTransients == 0,
        "Flux_RenderGraph::Validate: %u transient(s) declared but never referenced by any pass (see log for details)",
        uUnusedTransients);
}

// Per-pass invariants that don't depend on attachment counts: graphics passes
// must have at least one Write(), explicit dependencies must reference valid
// pass indices, and any pass with writes must have a record callback.
void Flux_RenderGraph::ValidatePassBasics(const Flux_RenderGraph_Pass* pxP) const
{
    const bool bIsGfx = pxP->m_uNumColourAttachments > 0 || pxP->m_xDepthStencil.IsValid();
    if (bIsGfx)
    {
        Zenith_Assert(pxP->m_xWrites.GetSize() > 0, "Flux_RenderGraph: graphics pass '%s' has no Write()", pxP->DebugName());
    }
    for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
    {
        u_int d = it.GetData();
        Zenith_Assert(d < m_xPasses.GetSize(), "Flux_RenderGraph: pass '%s' invalid dep %u", pxP->DebugName(), d);
    }
    if (pxP->m_xWrites.GetSize() > 0)
    {
        Zenith_Assert(pxP->m_pfnOnRecord, "Flux_RenderGraph: pass '%s' has writes but no callback", pxP->DebugName());
    }
}

// Count and bound-check the pass's RTV / DSV / UAV writes. RTV count must fit
// within FLUX_MAX_TARGETS (InferPassAttachments silently drops extras — a real
// bug). DSV count must be at most 1. Mixing attachment writes with UAV writes
// is technically legal (frag stage can write UAVs) but unusual; force an
// explicit conscious choice by failing here.
void Flux_RenderGraph::ValidatePassAttachmentCounts(const Flux_RenderGraph_Pass* pxP) const
{
    u_int uRTVCount = 0;
    u_int uDSVCount = 0;
    u_int uUAVWriteCount = 0;
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator itW(pxP->m_xWrites); !itW.Done(); itW.Next())
    {
        const Flux_RenderGraph_ResourceUsage& rxUsage = itW.GetData();
        if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_RTV) uRTVCount++;
        else if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_DSV) uDSVCount++;
        else if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_UAV || rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV) uUAVWriteCount++;
    }
    Zenith_Assert(uRTVCount <= FLUX_MAX_TARGETS,
        "Flux_RenderGraph: pass '%s' has %u RTV writes, exceeds FLUX_MAX_TARGETS (%u)",
        pxP->DebugName(), uRTVCount, FLUX_MAX_TARGETS);
    Zenith_Assert(uDSVCount <= 1,
        "Flux_RenderGraph: pass '%s' has %u depth writes; at most 1 allowed",
        pxP->DebugName(), uDSVCount);
    Zenith_Assert(!(uRTVCount + uDSVCount > 0 && uUAVWriteCount > 0),
        "Flux_RenderGraph: pass '%s' mixes attachment writes (%u RTV + %u DSV) with UAV writes (%u). Split into graphics + compute passes.",
        pxP->DebugName(), uRTVCount, uDSVCount, uUAVWriteCount);
}

void Flux_RenderGraph::Validate() const
{
    ValidateOrphanedReads();
    ValidateUnusedTransients();

    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;

        ValidatePassBasics(pxP);
        ValidatePassAttachmentCounts(pxP);
        // Post-allocation memory-flag/access compatibility runs after
        // AllocateTransients so transient attachments have valid surface info.
        ValidatePassMemoryFlagCompatibility(pxP);
    }
}

void Flux_RenderGraph::ValidatePassMemoryFlagCompatibility(const Flux_RenderGraph_Pass* pxP) const
{
    auto Check = [pxP](const Flux_RenderGraph_ResourceUsage& rxUsage, const char* szDir)
    {
        if (!rxUsage.m_xResource.IsImageLike()) return;
        const Flux_SurfaceInfo& rxInfo = rxUsage.m_xResource.GetSurfaceInfo();
        // Skip if the attachment hasn't been built yet (transient pre-allocation,
        // or owned buffer built later on a resize callback). m_uNumMips defaults
        // to 1 so we can't use that to detect "unset"; width is the reliable
        // sentinel for "no Build* call has run".
        if (rxInfo.m_uWidth == 0) return;
        if (rxUsage.m_eAccess == RESOURCE_ACCESS_READ_SRV)
        {
            Zenith_Assert(rxInfo.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
                "Flux_RenderGraph::Validate: pass '%s' declares %s READ_SRV on attachment '%s' without MEMORY_FLAGS__SHADER_READ (memFlags=0x%x, fmt=%d, %ux%u mips=%u)",
                pxP->DebugName(), szDir, rxUsage.m_xResource.GetName().c_str(),
                rxInfo.m_uMemoryFlags, (int)rxInfo.m_eFormat, rxInfo.m_uWidth, rxInfo.m_uHeight, rxInfo.m_uNumMips);
        }
        if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_UAV || rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV)
        {
            Zenith_Assert(rxInfo.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
                "Flux_RenderGraph::Validate: pass '%s' declares %s %s on attachment '%s' without MEMORY_FLAGS__UNORDERED_ACCESS (memFlags=0x%x, fmt=%d, %ux%u mips=%u)",
                pxP->DebugName(), szDir, AccessToString(rxUsage.m_eAccess), rxUsage.m_xResource.GetName().c_str(),
                rxInfo.m_uMemoryFlags, (int)rxInfo.m_eFormat, rxInfo.m_uWidth, rxInfo.m_uHeight, rxInfo.m_uNumMips);
        }
    };
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxP->m_xReads); !it.Done(); it.Next())
        Check(it.GetData(), "read");
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxP->m_xWrites); !it.Done(); it.Next())
        Check(it.GetData(), "write");
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
        if (uBestWriter != UINT32_MAX)
        {
            Zenith_Assert(uBestWriter < uReader,
                "Flux_RenderGraph::AddReaderWriterEdges: best writer %u is not strictly less than reader %u — FindBestWriter broken",
                uBestWriter, uReader);
            Zenith_Assert(uBestWriter < m_xPasses.GetSize() && uReader < m_xPasses.GetSize(),
                "Flux_RenderGraph::AddReaderWriterEdges: out-of-range pass index writer=%u reader=%u (have %u)",
                uBestWriter, uReader, m_xPasses.GetSize());
            AddEdgeIfNew(uBestWriter, uReader);
        }
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

    // Reset topological order markers so stale values from a previous Compile()
    // can't leak into the new sort.
    for (u_int i = 0; i < uN; i++)
        m_xPasses.Get(i)->m_uTopologicalOrder = UINT32_MAX;

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
            Zenith_Assert(m_xInDegree.Get(uNeighbor) > 0,
                "Flux_RenderGraph::TopologicalSort: in-degree underflow on pass %u (corrupted adjacency)", uNeighbor);
            if (--m_xInDegree.Get(uNeighbor) == 0)
                xQueue.PushBack(uNeighbor);
        }
    }

    u_int uEnabledCount = 0;
    for (u_int i = 0; i < uN; i++)
        if (m_xPasses.Get(i)->m_bEnabled) uEnabledCount++;

    if (uProcessed != uEnabledCount)
    {
        // Find and report one pass that's stuck in a cycle (in-degree > 0) to
        // give the programmer a concrete starting point, rather than just
        // "Cycle detected!".
#ifdef ZENITH_TOOLS
        for (u_int i = 0; i < uN; i++)
        {
            if (m_xPasses.Get(i)->m_bEnabled && m_xInDegree.Get(i) > 0)
            {
                Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_RenderGraph: pass '%s' has unresolved in-degree %u (part of a cycle)",
                    m_xPasses.Get(i)->DebugName(), m_xInDegree.Get(i));
            }
        }
#endif
        Zenith_Assert(false, "Flux_RenderGraph: Cycle detected — %u of %u enabled passes sorted. See log for cycle members.",
            uProcessed, uEnabledCount);
        return false;
    }

    // Sanity: every enabled pass must have received a topo order.
    for (u_int i = 0; i < uN; i++)
    {
        const Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;
        Zenith_Assert(pxP->m_uTopologicalOrder != UINT32_MAX,
            "Flux_RenderGraph::TopologicalSort: enabled pass '%s' (index %u) never got a topological order",
            pxP->DebugName(), i);
    }

    return true;
}
