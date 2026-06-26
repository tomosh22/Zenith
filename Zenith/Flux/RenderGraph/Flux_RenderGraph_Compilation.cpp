#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"

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
        case RESOURCE_ACCESS_UNDEFINED:           return "UNDEFINED";
        case RESOURCE_ACCESS_READ_SRV:            return "READ_SRV";
        case RESOURCE_ACCESS_READ_DEPTH:          return "READ_DEPTH";
        case RESOURCE_ACCESS_WRITE_RTV:           return "WRITE_RTV";
        case RESOURCE_ACCESS_WRITE_DSV:           return "WRITE_DSV";
        case RESOURCE_ACCESS_WRITE_UAV:           return "WRITE_UAV";
        case RESOURCE_ACCESS_READWRITE_UAV:      return "READWRITE_UAV";
        case RESOURCE_ACCESS_READ_INDIRECT_ARG:   return "READ_INDIRECT_ARG";
        case RESOURCE_ACCESS_READ_BUFFER_SRV:     return "READ_BUFFER_SRV";
        case RESOURCE_ACCESS_HOST_TRANSFER_WRITE: return "HOST_TRANSFER_WRITE";
    }
    return "???";
}

void Flux_RenderGraph::ValidateOrphanedReads() const
{
    for (Zenith_HashMap<void*, ResourceTraffic>::Iterator it(m_xTraffic); !it.Done(); it.Next())
    {
        void* pKey = it.GetKey();
        const ResourceTraffic& xTraffic = it.GetValue();
        if (xTraffic.m_xReaders.GetSize() == 0 || xTraffic.m_xWriters.GetSize() != 0) continue;

        const Flux_RenderGraph_Resource* pxRes = m_xResources.TryGet(pKey);
        const char* sz = (pxRes != nullptr) ? pxRes->m_xResource.GetName().c_str() : "<unknown>";
        Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_RenderGraph: resource '%s' (ptr=%p) read but never written. Reader passes:",
            sz, pKey);
        for (Zenith_Vector<u_int>::Iterator itR(xTraffic.m_xReaders); !itR.Done(); itR.Next())
        {
            Flux_RenderGraph_Pass* pxReaderPass = m_xPasses.Get(itR.GetData());
            Zenith_Error(LOG_CATEGORY_RENDERER, "  - Pass %u: '%s'", itR.GetData(), pxReaderPass->DebugName());
        }
        Zenith_Assert(false, "Flux_RenderGraph: resource '%s' (ptr=%p) read but never written (see reader pass list above)",
            sz, pKey);
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

// Producer-before-consumer invariant. The edge-builder (FindBestWriter) links a
// reader to a writer of the SAME resource only when that writer is declared
// EARLIER in the setup walk. If a resource has readers but every writer is
// declared LATER, the read-after-write edge AND the barrier the graph would
// synthesise are silently dropped — and ValidateOrphanedReads does NOT catch it
// (a writer exists, just too late). This makes that case loud.
//
// Runs once per Compile (not per-frame), so the cost is negligible. Exemptions
// that are NOT bugs: a reader that is also a writer (read-modify-write self-
// orders) and a reader that directly DependsOn one of the writers. Enforced as a
// Zenith_Check (shipping-survivable: logs loudly, doesn't crash) — confirmed
// clean across the full 115-pass graph. (Only DIRECT DependsOn is exempted; a
// reader ordered solely via a transitive DependsOn chain would log a benign
// false positive — none exist today.)
void Flux_RenderGraph::ValidateProducerBeforeConsumer() const
{
    for (Zenith_HashMap<void*, ResourceTraffic>::Iterator it(m_xTraffic); !it.Done(); it.Next())
    {
        const ResourceTraffic& xTraffic = it.GetValue();
        const Zenith_Vector<u_int>& xWriters = xTraffic.m_xWriters;
        const Zenith_Vector<u_int>& xReaders = xTraffic.m_xReaders;
        if (xReaders.GetSize() == 0 || xWriters.GetSize() == 0) continue; // orphan reads handled by ValidateOrphanedReads

        for (Zenith_Vector<u_int>::Iterator itR(xReaders); !itR.Done(); itR.Next())
        {
            const u_int uReader = itR.GetData();

            bool bExempt = false;
            for (Zenith_Vector<u_int>::Iterator itW(xWriters); !itW.Done(); itW.Next())
            {
                const u_int uWriter = itW.GetData();
                if (uWriter == uReader) { bExempt = true; break; }  // read-modify-write self-orders
                if (uWriter < uReader)  { bExempt = true; break; }  // has an earlier writer (edge will form)
            }
            if (bExempt) continue;

            // No earlier (or self) writer. Legitimate only if the reader explicitly
            // DependsOn one of the writers (direct dependency provides the ordering).
            const Flux_RenderGraph_Pass* pxReader = m_xPasses.Get(uReader);
            bool bDependsOnWriter = false;
            for (Zenith_Vector<u_int>::Iterator itD(pxReader->m_xExplicitDependencies); !itD.Done() && !bDependsOnWriter; itD.Next())
                for (Zenith_Vector<u_int>::Iterator itW(xWriters); !itW.Done(); itW.Next())
                    if (itD.GetData() == itW.GetData()) { bDependsOnWriter = true; break; }
            if (bDependsOnWriter) continue;

            const Flux_RenderGraph_Resource* pxRes = m_xResources.TryGet(it.GetKey());
            const char* szRes = (pxRes != nullptr) ? pxRes->m_xResource.GetName().c_str() : "<unknown>";
            Zenith_Check(false,
                "Flux_RenderGraph: pass '%s' reads '%s' but every writer is declared LATER in the setup walk "
                "— the read-after-write edge + barrier are dropped. Declare the producer before the consumer, "
                "or add DependsOn(producer).",
                pxReader->DebugName(), szRes);
        }
    }
}

void Flux_RenderGraph::Validate() const
{
    ValidateOrphanedReads();
    ValidateProducerBeforeConsumer();
    ValidateUnusedTransients();

    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!IsPassEffectivelyEnabled(pxP)) continue;

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
    m_xEdgeSet.Clear();
}

void Flux_RenderGraph::BuildAdjacencyFromTraffic()
{
    for (Zenith_HashMap<void*, ResourceTraffic>::Iterator it(m_xTraffic); !it.Done(); it.Next())
    {
        const ResourceTraffic& xTraffic = it.GetValue();
        const Zenith_Vector<u_int>& xWriters = xTraffic.m_xWriters;
        const Zenith_Vector<u_int>& xReaders = xTraffic.m_xReaders;
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
        if (!m_xEdgeSet.Contains(ulKey))
        {
            m_xEdgeSet.Insert(ulKey);
            m_xAdjacency.Get(axWriters.Get(w)).PushBack(axWriters.Get(w + 1));
            m_xInDegree.Get(axWriters.Get(w + 1))++;
        }
    }
}

void Flux_RenderGraph::AddEdgeIfNew(u_int uFrom, u_int uTo)
{
    u_int64 ulKey = (static_cast<u_int64>(uFrom) << 32) | uTo;
    if (!m_xEdgeSet.Contains(ulKey))
    {
        m_xEdgeSet.Insert(ulKey);
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
        if (!IsPassEffectivelyEnabled(pxP)) continue;
        for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
        {
            u_int d = it.GetData();
            if (IsPassEffectivelyEnabled(m_xPasses.Get(d))) AddEdgeIfNew(d, i);
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
        if (IsPassEffectivelyEnabled(m_xPasses.Get(i)) && m_xInDegree.Get(i) == 0)
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
        if (IsPassEffectivelyEnabled(m_xPasses.Get(i))) uEnabledCount++;

    if (uProcessed != uEnabledCount)
    {
        // Find and report one pass that's stuck in a cycle (in-degree > 0) to
        // give the programmer a concrete starting point, rather than just
        // "Cycle detected!".
#ifdef ZENITH_TOOLS
        for (u_int i = 0; i < uN; i++)
        {
            if (IsPassEffectivelyEnabled(m_xPasses.Get(i)) && m_xInDegree.Get(i) > 0)
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
        if (!IsPassEffectivelyEnabled(pxP)) continue;
        Zenith_Assert(pxP->m_uTopologicalOrder != UINT32_MAX,
            "Flux_RenderGraph::TopologicalSort: enabled pass '%s' (index %u) never got a topological order",
            pxP->DebugName(), i);
    }

    return true;
}

//=============================================================================
// Transient allocation + lifetime, aliasing pack, clear flags, barrier
// synthesis, and the Compile() orchestrator — moved here from
// Flux_RenderGraph.cpp so this TU owns the WHOLE compile phase.
//=============================================================================

// Defined beside the other transient-size helpers below; AllocateTransients
// needs it first.
static u_int64 EstimateTransientImageSize(const Flux_TransientTextureDesc& xDesc);

void Flux_RenderGraph::AllocateTransients()
{
    // If the packer opened any aliasing pools, allocate them first — each
    // pool's VRAM must exist before per-transient CreateAliasedImageVRAM
    // calls can bind images into it.
    u_int64 ulPoolBytesTotal = 0;
    for (u_int p = 0; p < m_axAliasPools.GetSize(); p++)
    {
        AliasPool& xPool = m_axAliasPools.Get(p);
        if (xPool.m_ulTotalSize == 0) continue; // pool with no occupants — shouldn't happen but skip safely
        xPool.m_xPoolVRAM = g_xEngine.FluxMemory().CreateAliasPoolVRAM(xPool.m_ulTotalSize, xPool.m_ulMaxAlignment);
        Zenith_Assert(xPool.m_xPoolVRAM.IsValid(),
            "Flux_RenderGraph::AllocateTransients: pool %u alloc failed (size=%llu)",
            p, static_cast<unsigned long long>(xPool.m_ulTotalSize));
        ulPoolBytesTotal += xPool.m_ulTotalSize;
    }

    // Log the aliasing-savings headline — total pool VRAM vs what a standalone
    // allocation per transient would have consumed. The standalone baseline
    // uses the same size estimator as the packer so the comparison is apples-
    // to-apples.
    if (m_axAliasPools.GetSize() > 0)
    {
        u_int64 ulStandaloneBytes = 0;
        u_int uAliasedCount = 0;
        for (u_int i = 0; i < m_axTransients.GetSize(); i++)
        {
            const TransientResource* pxT = m_axTransients.Get(i);
            if (pxT->m_uAliasPoolIndex == UINT32_MAX) continue;
            ulStandaloneBytes += EstimateTransientImageSize(pxT->m_xDesc);
            uAliasedCount++;
        }
        const u_int64 ulSavedBytes = (ulStandaloneBytes > ulPoolBytesTotal)
            ? ulStandaloneBytes - ulPoolBytesTotal : 0;
        Zenith_Log(LOG_CATEGORY_RENDERER,
            "RenderGraph: Transient aliasing packed %u transients into %u pools; %llu KiB pooled vs %llu KiB standalone (%llu KiB saved, %.1f%%)",
            uAliasedCount,
            m_axAliasPools.GetSize(),
            static_cast<unsigned long long>(ulPoolBytesTotal >> 10),
            static_cast<unsigned long long>(ulStandaloneBytes >> 10),
            static_cast<unsigned long long>(ulSavedBytes >> 10),
            ulStandaloneBytes > 0 ? (100.0 * static_cast<double>(ulSavedBytes) / static_cast<double>(ulStandaloneBytes)) : 0.0);
    }

    // Per-transient allocation. Two paths:
    //   - m_uAliasPoolIndex != UINT32_MAX → aliased into a pool; create a
    //     VkImage bound at the packer's offset, then run the view-creation
    //     logic via BuildColourFromAliasedVRAM / BuildDepthStencilFromAliasedVRAM.
    //   - m_uAliasPoolIndex == UINT32_MAX → standalone allocation (aliasing
    //     disabled, or transient was never referenced by a pass).
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
        xBuilder.m_uNumLayers = pxT->m_xDesc.m_uNumLayers;

        const bool bAliased = (pxT->m_uAliasPoolIndex != UINT32_MAX);

        if (bAliased)
        {
            const AliasPool& xPool = m_axAliasPools.Get(pxT->m_uAliasPoolIndex);
            // Build the surface info the aliased image wants (mirror the non-aliased path).
            Flux_SurfaceInfo xInfo;
            xInfo.m_uWidth       = pxT->m_xDesc.m_uWidth;
            xInfo.m_uHeight      = pxT->m_xDesc.m_uHeight;
            xInfo.m_uDepth       = pxT->m_xDesc.m_uDepth;
            xInfo.m_eFormat      = pxT->m_xDesc.m_eFormat;
            xInfo.m_eTextureType = pxT->m_xDesc.m_eTextureType;
            xInfo.m_uNumMips     = pxT->m_xDesc.m_uNumMips;
            xInfo.m_uNumLayers   = (pxT->m_xDesc.m_eTextureType == TEXTURE_TYPE_CUBE) ? 6u : pxT->m_xDesc.m_uNumLayers;
            xInfo.m_uMemoryFlags = pxT->m_xDesc.m_uMemoryFlags;

            Flux_VRAMHandle xAliasedVRAM = g_xEngine.FluxMemory().CreateAliasedImageVRAM(
                xInfo, xPool.m_xPoolVRAM, pxT->m_ulAliasOffset);
            Zenith_Assert(xAliasedVRAM.IsValid(),
                "Flux_RenderGraph::AllocateTransients: aliased image creation failed for transient %u (pool=%u offset=%llu)",
                i, pxT->m_uAliasPoolIndex, static_cast<unsigned long long>(pxT->m_ulAliasOffset));

            if (pxT->m_xDesc.m_bIsDepthStencil)
                xBuilder.BuildDepthStencilFromAliasedVRAM(pxT->m_xAttachment, "Transient DS (aliased)", xAliasedVRAM);
            else
                xBuilder.BuildColourFromAliasedVRAM(pxT->m_xAttachment, "Transient (aliased)", xAliasedVRAM);
        }
        else
        {
            if (pxT->m_xDesc.m_bIsDepthStencil)
                xBuilder.BuildDepthStencil(pxT->m_xAttachment, "Transient DS");
            else
                xBuilder.BuildColour(pxT->m_xAttachment, "Transient");
        }

        Zenith_Assert(pxT->m_xAttachment.m_xVRAMHandle.IsValid(),
            "Flux_RenderGraph::AllocateTransients: allocation failed for transient %u", i);
        pxT->m_bAllocated = true;
    }
}

void Flux_RenderGraph::ReleaseTransientAllocations()
{
    // Same shape as DestroyTransients (attachments first, then pools with +1
    // frame delay), but we do NOT touch m_axTransients — subsystems hold
    // Flux_TransientHandle values that index into it and must stay stable
    // across re-Compiles. m_bAllocated gets reset so AllocateTransients
    // rebuilds the attachment next phase.
    for (u_int i = 0; i < m_axTransients.GetSize(); i++)
    {
        TransientResource* pxT = m_axTransients.Get(i);
        if (pxT->m_bAllocated)
        {
            Flux_RenderAttachmentBuilder::Destroy(pxT->m_xAttachment);
            pxT->m_bAllocated = false;
        }
    }

    for (u_int p = 0; p < m_axAliasPools.GetSize(); p++)
    {
        AliasPool& xPool = m_axAliasPools.Get(p);
        if (!xPool.m_xPoolVRAM.IsValid()) continue;
        g_xEngine.FluxMemory().QueueVRAMDeletion(
            xPool.m_xPoolVRAM,
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            /*uExtraFrameDelay*/ 1);
    }
    m_axAliasPools.Clear();
}


void Flux_RenderGraph::BuildResourceTraffic()
{
    m_xTraffic.Clear();
    for (u_int uPass = 0; uPass < m_xPasses.GetSize(); uPass++)
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPass);
        if (!IsPassEffectivelyEnabled(pxPass)) continue;
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

// ValidateOrphanedReads — moved to Flux_RenderGraph_Compilation.cpp

// ValidateUnusedTransients, ValidatePassBasics, ValidatePassAttachmentCounts,
// Validate, ValidatePassMemoryFlagCompatibility, InitAdjacencyData,
// BuildAdjacencyFromTraffic, AddReaderWriterEdges, IsAlsoWriter, FindBestWriter,
// AddWriterChainEdges, AddEdgeIfNew, AddExplicitDependencies, TopologicalSort
// — all moved to Flux_RenderGraph_Compilation.cpp.

void Flux_RenderGraph::ComputeResourceLifetimes()
{
    // Lifetime values are stored as TOPOLOGICAL ORDER indices (positions in
    // m_xExecutionOrder), NOT pass-declaration indices. The aliasing packer
    // treats m_uFirstWrite / m_uLastRead / m_uLastWrite as execution-time-
    // ordered values (T1 ends before T2 starts ⇒ they can share memory),
    // and that's only true if we use the topological position. Two passes
    // from different subsystems may have non-overlapping pass indices but
    // interleave in execution order — for example SSR_Upsample (pass idx 2)
    // can run between SSGI_Upsample (pass idx 5) and SSGI_DenoiseH (pass
    // idx 6), making the SSR upsampled and SSGI resolved transients look
    // non-overlapping by declaration index when they actually overlap in
    // execution. Pre-fix, that caused the packer to alias them, and SSR's
    // writes trampled SSGI's resolved buffer before DenoiseH could read it.
    //
    // Disabled-pass handling: this function is callable both at Compile
    // time (where m_xExecutionOrder contains only enabled passes — the
    // disabled-pass check is a no-op) and at Execute time when
    // m_bEnabledMaskDirty triggers a barrier re-synth. In the latter case
    // we MUST skip disabled passes so lifetimes reflect what will actually
    // run this frame; otherwise SynthesizeAliasingBarriers would resolve
    // pxPrior->m_uLastUse to a disabled pass and read a src access that
    // doesn't reflect the resource's true GPU state.
    //
    // The reset loop at the top makes this idempotent — every call rebuilds
    // lifetimes from scratch rather than accumulating onto stale state.

    for (Zenith_HashMap<void*, Flux_RenderGraph_Resource>::Iterator it(m_xResources); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Resource& xRes = it.GetValueMutable();
        xRes.m_uFirstWrite = UINT32_MAX;
        xRes.m_uLastRead   = UINT32_MAX;
        xRes.m_uLastWrite  = UINT32_MAX;
    }

    u_int uTopo = 0;
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next(), ++uTopo)
    {
        u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        if (!IsPassEffectivelyEnabled(pxPass)) continue;
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xWrite = it.GetData();
            void* pRes = xWrite.m_xResource.GetVoidPtr();
            Flux_RenderGraph_Resource* pxRes = m_xResources.TryGet(pRes);
            Zenith_Assert(pxRes != nullptr, "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (uTopo < pxRes->m_uFirstWrite) pxRes->m_uFirstWrite = uTopo;
            if (pxRes->m_uLastWrite == UINT32_MAX || uTopo > pxRes->m_uLastWrite)
                pxRes->m_uLastWrite = uTopo;
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            Flux_RenderGraph_Resource* pxRes = m_xResources.TryGet(pRes);
            Zenith_Assert(pxRes != nullptr, "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (pxRes->m_uLastRead == UINT32_MAX || uTopo > pxRes->m_uLastRead)
                pxRes->m_uLastRead = uTopo;
        }
    }
}

// Build a Flux_SurfaceInfo matching a transient's desc so we can probe the
// backend for the image's exact memory requirements.

static Flux_SurfaceInfo TransientDescToSurfaceInfo(const Flux_TransientTextureDesc& xDesc)
{
    Flux_SurfaceInfo xInfo;
    xInfo.m_uWidth       = xDesc.m_uWidth;
    xInfo.m_uHeight      = xDesc.m_uHeight;
    xInfo.m_uDepth       = xDesc.m_uDepth;
    xInfo.m_eFormat      = xDesc.m_eFormat;
    xInfo.m_eTextureType = xDesc.m_eTextureType;
    xInfo.m_uNumMips     = xDesc.m_uNumMips;
    xInfo.m_uNumLayers   = (xDesc.m_eTextureType == TEXTURE_TYPE_CUBE) ? 6u : (xDesc.m_uNumLayers > 0 ? xDesc.m_uNumLayers : 1u);
    xInfo.m_uMemoryFlags = xDesc.m_uMemoryFlags;
    return xInfo;
}

// Last-resort heuristic kept for the extremely unlikely case that
// ProbeImageMemoryRequirements fails to create an image. The 2× factor + 1 MiB
// round-up intentionally overestimates; a genuine allocation failure is far
// better than a subtle bind mismatch.
static u_int64 FallbackTransientImageSize(const Flux_TransientTextureDesc& xDesc)
{
    const u_int uBpp = xDesc.m_bIsDepthStencil
        ? 4u
        : ColourFormatBytesPerPixel(xDesc.m_eFormat);

    const u_int64 ulLayers = (xDesc.m_eTextureType == TEXTURE_TYPE_CUBE) ? 6u : (xDesc.m_uNumLayers > 0 ? xDesc.m_uNumLayers : 1u);
    const u_int64 ulDepth  = (xDesc.m_eTextureType == TEXTURE_TYPE_3D) ? xDesc.m_uDepth : 1u;
    const u_int64 ulBase   = static_cast<u_int64>(xDesc.m_uWidth) * xDesc.m_uHeight * ulDepth * ulLayers * uBpp;
    const u_int64 ulWithMips = (xDesc.m_uNumMips > 1) ? ((ulBase * 4 + 2) / 3) : ulBase;
    const u_int64 ulWithHeadroom = ulWithMips * 2;
    constexpr u_int64 ulALIGNMENT = 1024ull * 1024ull;
    return (ulWithHeadroom + ulALIGNMENT - 1) & ~(ulALIGNMENT - 1);
}

// Returns the image's actual memory size (aligned up) from a driver probe, or
// a 2× fallback on probe failure. Writes the probed alignment into ulAlignmentOut
// (0 on probe failure — callers then fall back to the 64 KB default in
// CreateAliasPoolVRAM).
static u_int64 EstimateTransientImageSize(const Flux_TransientTextureDesc& xDesc, u_int64& ulAlignmentOut)
{
    const Flux_SurfaceInfo xInfo = TransientDescToSurfaceInfo(xDesc);
    u_int64 ulProbedSize = 0;
    u_int64 ulProbedAlignment = 0;
    g_xEngine.FluxMemory().ProbeImageMemoryRequirements(xInfo, ulProbedSize, ulProbedAlignment);

    ulAlignmentOut = ulProbedAlignment;
    if (ulProbedSize == 0)
    {
        Zenith_Log(LOG_CATEGORY_RENDERER,
            "RenderGraph: ProbeImageMemoryRequirements failed for %ux%u format=%d — using 2x fallback size heuristic",
            xDesc.m_uWidth, xDesc.m_uHeight, (int)xDesc.m_eFormat);
        return FallbackTransientImageSize(xDesc);
    }

    // Round the probed size up to its own alignment so concatenating in a pool
    // keeps successive images aligned without extra gap calculations.
    if (ulProbedAlignment > 0)
    {
        const u_int64 ulMask = ulProbedAlignment - 1;
        return (ulProbedSize + ulMask) & ~ulMask;
    }
    return ulProbedSize;
}

// Overload retained for call sites that don't care about alignment (diagnostics
// summing standalone-bytes for the "pooled vs standalone" log line).
static u_int64 EstimateTransientImageSize(const Flux_TransientTextureDesc& xDesc)
{
    u_int64 ulDiscard = 0;
    return EstimateTransientImageSize(xDesc, ulDiscard);
}


void Flux_RenderGraph::SynthesizeAliasingBarriers()
{
    // Clear previous compile's aliasing-barrier vectors so the recompile
    // starts from a clean state.
    for (u_int p = 0; p < m_xPasses.GetSize(); p++)
    {
        m_xPasses.Get(p)->m_xAliasingBarriers.Clear();
    }

    // No work when aliasing is disabled — no pools exist.
    if (m_axAliasPools.GetSize() == 0)
        return;

    // Return the access used for pxT in the pass at uPassIdx. Asserts on
    // failure to find the transient in the pass: the aliasing barrier is only
    // emitted when we already know the transient's lifetime covers uPassIdx,
    // so failing to locate its access here means the lifetime tables and the
    // pass's declared reads/writes have diverged — a bug that would otherwise
    // silently map to TopOfPipe (no-op barrier).
    auto FindAccessForTransientInPass = [this](const TransientResource* pxT, u_int uPassIdx) -> ResourceAccess
    {
        Zenith_Assert(uPassIdx != UINT32_MAX, "FindAccessForTransientInPass: sentinel pass index reached aliasing barrier synthesis");
        const Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        const void* pTarget = static_cast<const void*>(&pxT->m_xAttachment);
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            if (it.GetData().m_xResource.GetVoidPtr() == pTarget)
                return it.GetData().m_eAccess;
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            if (it.GetData().m_xResource.GetVoidPtr() == pTarget)
                return it.GetData().m_eAccess;
        }
        Zenith_Assert(false, "FindAccessForTransientInPass: transient '%s' not found in pass '%s' reads/writes — lifetime tables disagree with pass usage",
            "<transient>", pxPass->DebugName());
        return RESOURCE_ACCESS_UNDEFINED;
    };

    // For each pool, track the "current occupant" as we walk the execution
    // order. When a new transient from the same pool becomes active (its
    // first-write pass), the pass needs an aliasing barrier IF the prior
    // occupant was a DIFFERENT transient. Single-occupant pools never need
    // an aliasing barrier.
    struct PoolCurrentOccupant { u_int m_uTransientIndex = UINT32_MAX; };
    Zenith_Vector<PoolCurrentOccupant> axCurrent;
    axCurrent.Reserve(m_axAliasPools.GetSize());
    for (u_int p = 0; p < m_axAliasPools.GetSize(); p++)
        axCurrent.PushBack(PoolCurrentOccupant{});

    // Lifetime values on TransientResource are TOPOLOGICAL ORDER indices
    // (see ComputeResourceLifetimes for why). Iterate by topological order
    // and compare m_uFirstWrite / m_uLastUse against uTopo, then translate
    // back to pass indices when calling FindAccessForTransientInPass
    // (which looks pass usage up by pass index).
    u_int uTopo = 0;
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next(), ++uTopo)
    {
        const u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        if (!IsPassEffectivelyEnabled(pxPass)) continue;

        for (u_int u = 0; u < m_axTransients.GetSize(); u++)
        {
            TransientResource* pxT = m_axTransients.Get(u);
            if (pxT->m_uAliasPoolIndex == UINT32_MAX) continue;  // standalone
            if (pxT->m_uFirstWrite != uTopo) continue;            // not its first-use pass (topo order)

            const u_int uPool = pxT->m_uAliasPoolIndex;
            const u_int uPriorOccupant = axCurrent.Get(uPool).m_uTransientIndex;
            if (uPriorOccupant != UINT32_MAX && uPriorOccupant != u)
            {
                // Memory hand-off from a different transient — push a per-
                // transient barrier entry so the backend emits one tight
                // vk::MemoryBarrier per hand-off. Passes that are the first
                // user of multiple aliased transients push multiple entries;
                // pre-fix this overwrote a single scalar pair and lost all
                // but the last src/dst mask.
                const TransientResource* pxPrior = m_axTransients.Get(uPriorOccupant);
                const u_int uPriorLastTopo = pxPrior->m_uLastUse;
                const u_int uPriorLastPassIdx = m_xExecutionOrder.Get(uPriorLastTopo);
                Flux_RenderGraph_AliasingBarrier xBarrier;
                xBarrier.m_eSrcAccess = FindAccessForTransientInPass(pxPrior, uPriorLastPassIdx);
                xBarrier.m_eDstAccess = FindAccessForTransientInPass(pxT,     uPassIdx);
                xBarrier.m_bSrcIsDepth = (xBarrier.m_eSrcAccess == RESOURCE_ACCESS_WRITE_DSV
                                       || xBarrier.m_eSrcAccess == RESOURCE_ACCESS_READ_DEPTH);
                xBarrier.m_bDstIsDepth = (xBarrier.m_eDstAccess == RESOURCE_ACCESS_WRITE_DSV
                                       || xBarrier.m_eDstAccess == RESOURCE_ACCESS_READ_DEPTH);
                pxPass->m_xAliasingBarriers.PushBack(xBarrier);
            }
            axCurrent.Get(uPool).m_uTransientIndex = u;
        }
    }
}

u_int64 Flux_RenderGraph::MakeTransientMemorySignature(const Flux_TransientTextureDesc& xDesc)
{
    // Pack (texture-type, depth-stencil flag, memory-flags, format) into one
    // u_int64 so two transients with matching signatures are known to have
    // compatible memory requirements (same heap + alignment class).
    //
    // Bits (low to high):
    //    [0..7]   : texture type (enum, fits in 8 bits)
    //    [8]      : depth-stencil flag
    //    [9..40]  : memory flags (u_int, 32 bits)
    //    [41..56] : format (enum, fits in 16 bits)
    //    [57..63] : array layer count (7 bits; an array depth must NOT share a
    //               pool slot with a single-layer transient even if everything
    //               else matches — its image + views differ)
    // Everything else (dimensions, mip count) is NOT in the signature — the
    // packer handles those by computing pool size from per-occupant allocations.
    const u_int64 ulTextureType = static_cast<u_int64>(xDesc.m_eTextureType) & 0xFFull;
    const u_int64 ulDepthFlag   = xDesc.m_bIsDepthStencil ? 1ull : 0ull;
    const u_int64 ulMemFlags    = static_cast<u_int64>(xDesc.m_uMemoryFlags) & 0xFFFFFFFFull;
    const u_int64 ulFormat      = static_cast<u_int64>(xDesc.m_eFormat) & 0xFFFFull;
    const u_int64 ulLayers      = static_cast<u_int64>(xDesc.m_uNumLayers > 0 ? xDesc.m_uNumLayers : 1u) & 0x7Full;
    return  ulTextureType
         | (ulDepthFlag  << 8)
         | (ulMemFlags   << 9)
         | (ulFormat     << 41)
         | (ulLayers     << 57);
}

void Flux_RenderGraph::ComputeTransientLifetimes()
{
    // Copy the existing per-attachment lifetime data (computed over void*
    // resource keys by ComputeResourceLifetimes) into each TransientResource
    // so the aliasing packer can iterate m_axTransients directly without an
    // extra map lookup. Transients that no enabled pass touched keep their
    // sentinel UINT32_MAX — the packer treats those as not-referenced and
    // skips them.
    //
    // m_uLastUse is the max of {last read, last write}, both topological-
    // order indices. For a transient with multiple writes (e.g. a UAV
    // ping-pong target), the last write must extend the lifetime — using
    // first-write here would let the packer alias a second transient into
    // the period between the first write and a later write, and the second
    // transient's writes would then trample the first transient's late
    // writes. For a write-only transient (no reads) the lifetime collapses
    // to the last write so the packer's strict-greater overlap check
    // doesn't need a UINT32_MAX special case.
    for (u_int u = 0; u < m_axTransients.GetSize(); u++)
    {
        TransientResource* pxT = m_axTransients.Get(u);
        pxT->m_uFirstWrite = UINT32_MAX;
        pxT->m_uLastUse    = UINT32_MAX;

        // TrackResource adds entries keyed on the attachment pointer. If the
        // transient was never read or written in any enabled pass it won't be
        // in m_xResources at all, in which case the sentinels stay.
        void* pRes = static_cast<void*>(&pxT->m_xAttachment);
        const Flux_RenderGraph_Resource* pxRes = m_xResources.TryGet(pRes);
        if (pxRes == nullptr)
            continue;

        const Flux_RenderGraph_Resource& xRes = *pxRes;
        pxT->m_uFirstWrite = xRes.m_uFirstWrite;

        // Last-use = max(last read, last write), tolerant of UINT32_MAX
        // sentinels on either side. Falls back to m_uFirstWrite if both
        // are sentinel (which only happens if the transient was tracked
        // via TrackResource but had every access stripped — e.g. all its
        // writers/readers were disabled when ComputeResourceLifetimes ran).
        u_int uLastUse = pxT->m_uFirstWrite;
        if (xRes.m_uLastRead != UINT32_MAX && (uLastUse == UINT32_MAX || xRes.m_uLastRead > uLastUse))
            uLastUse = xRes.m_uLastRead;
        if (xRes.m_uLastWrite != UINT32_MAX && (uLastUse == UINT32_MAX || xRes.m_uLastWrite > uLastUse))
            uLastUse = xRes.m_uLastWrite;
        pxT->m_uLastUse = uLastUse;
    }
}

// Build a sort-order over referenced transients, ascending by m_uFirstWrite.
// Drives the greedy packer (interval-coloring works best when intervals are
// seen in start-time order). Insertion sort is fine — N is small (~30).
void Flux_RenderGraph::SortTransientsByLifetime(Zenith_Vector<u_int>& axSortedIndices) const
{
    axSortedIndices.Reserve(m_axTransients.GetSize());
    for (u_int u = 0; u < m_axTransients.GetSize(); u++)
    {
        const TransientResource* pxT = m_axTransients.Get(u);
        if (pxT->m_uFirstWrite == UINT32_MAX) continue; // unreferenced → keep standalone
        axSortedIndices.PushBack(u);
    }
    for (u_int i = 1; i < axSortedIndices.GetSize(); i++)
    {
        const u_int uCur = axSortedIndices.Get(i);
        const u_int uCurFirst = m_axTransients.Get(uCur)->m_uFirstWrite;
        u_int j = i;
        while (j > 0 && m_axTransients.Get(axSortedIndices.Get(j - 1))->m_uFirstWrite > uCurFirst)
        {
            axSortedIndices.Get(j) = axSortedIndices.Get(j - 1);
            j--;
        }
        axSortedIndices.Get(j) = uCur;
    }
}

// Greedy first-fit packer. For each transient (in first-write order), scan
// existing pools with matching memory signature; reuse the first pool whose
// max-last-use is strictly less than the new first-write. Otherwise open a new
// pool. Tracks per-pool max-last-use in a parallel occupancy vector — only the
// max matters because transients are added in first-write order, so the only
// overlap risk is against a prior occupant whose last-use extends past the new
// first-write.
//
// ComputeTransientLifetimes collapsed the "last read" and "last write when
// never read" sentinels into a single m_uLastUse, so the comparison below is
// a clean strict-greater without a UINT32_MAX special case.
void Flux_RenderGraph::PackTransientsIntoPools(const Zenith_Vector<u_int>& axSortedIndices)
{
    struct PoolOccupancy
    {
        u_int64 m_ulSignature;
        u_int   m_uMaxLastUse;
    };
    Zenith_Vector<PoolOccupancy> axOccupancy;

    for (u_int idx = 0; idx < axSortedIndices.GetSize(); idx++)
    {
        const u_int u = axSortedIndices.Get(idx);
        TransientResource* pxT = m_axTransients.Get(u);
        const u_int64 ulSig = MakeTransientMemorySignature(pxT->m_xDesc);

        u_int uAssignedPool = UINT32_MAX;
        for (u_int p = 0; p < axOccupancy.GetSize(); p++)
        {
            if (axOccupancy.Get(p).m_ulSignature != ulSig) continue;
            // Strict-greater: equal is an overlap (writer and reader both touch
            // memory in the same pass-ordering slot).
            if (pxT->m_uFirstWrite > axOccupancy.Get(p).m_uMaxLastUse)
            {
                uAssignedPool = p;
                break;
            }
        }

        if (uAssignedPool == UINT32_MAX)
        {
            AliasPool xNewPool;
            xNewPool.m_ulSignature  = ulSig;
            xNewPool.m_ulTotalSize  = 0; // ComputePoolSizes will fill this in.
            xNewPool.m_uMemoryFlags = pxT->m_xDesc.m_uMemoryFlags;
            m_axAliasPools.PushBack(xNewPool);

            PoolOccupancy xOcc;
            xOcc.m_ulSignature = ulSig;
            xOcc.m_uMaxLastUse = pxT->m_uLastUse;
            axOccupancy.PushBack(xOcc);

            uAssignedPool = m_axAliasPools.GetSize() - 1;
        }
        else
        {
            PoolOccupancy& xOcc = axOccupancy.Get(uAssignedPool);
            if (pxT->m_uLastUse > xOcc.m_uMaxLastUse)
                xOcc.m_uMaxLastUse = pxT->m_uLastUse;

            // Defence-in-depth: the signature already includes m_bIsDepthStencil
            // (bit 8 of MakeTransientMemorySignature). This assertion trips if
            // that ever regresses and depth + colour transients land in the
            // same pool — would otherwise surface as sporadic
            // vmaCreateAliasingImage2 failures on drivers with distinct
            // depth/colour memoryTypeBits.
            const AliasPool& xPool = m_axAliasPools.Get(uAssignedPool);
            bool bPoolIsDepth = false;
            for (u_int vv = 0; vv < m_axTransients.GetSize(); vv++)
            {
                const TransientResource* pxOther = m_axTransients.Get(vv);
                if (pxOther->m_uAliasPoolIndex == uAssignedPool)
                {
                    bPoolIsDepth = pxOther->m_xDesc.m_bIsDepthStencil;
                    break;
                }
            }
            Zenith_Assert(bPoolIsDepth == pxT->m_xDesc.m_bIsDepthStencil,
                "AssignAliasingGroups: pool %u mixes depth-stencil (%d) and colour (%d) transients — signature logic regressed (signature 0x%llx).",
                uAssignedPool, bPoolIsDepth ? 1 : 0, pxT->m_xDesc.m_bIsDepthStencil ? 1 : 0,
                static_cast<unsigned long long>(xPool.m_ulSignature));
        }

        pxT->m_uAliasPoolIndex = uAssignedPool;
        pxT->m_ulAliasOffset   = 0; // ComputePoolSizes runs second; offsets stay 0 (lifetimes don't overlap).
    }
}

// Per-pool: size = max(occupant size); alignment = max(per-image alignment).
// All occupants bind at offset 0 — the packer has already guaranteed lifetimes
// don't overlap, so at any moment only one transient is "live" in the pool
// memory. This is the actual aliasing saving: 3 × 16 MiB transients become
// one 16 MiB pool, not 48 MiB.
void Flux_RenderGraph::ComputePoolSizes()
{
    for (u_int p = 0; p < m_axAliasPools.GetSize(); p++)
    {
        AliasPool& xPool = m_axAliasPools.Get(p);
        u_int64 ulMaxSize = 0;
        u_int64 ulMaxAlignment = 0;
        for (u_int u = 0; u < m_axTransients.GetSize(); u++)
        {
            TransientResource* pxT = m_axTransients.Get(u);
            if (pxT->m_uAliasPoolIndex != p) continue;
            u_int64 ulPerImageAlignment = 0;
            const u_int64 ulSize = EstimateTransientImageSize(pxT->m_xDesc, ulPerImageAlignment);
            if (ulSize > ulMaxSize) ulMaxSize = ulSize;
            if (ulPerImageAlignment > ulMaxAlignment) ulMaxAlignment = ulPerImageAlignment;
            pxT->m_ulAliasOffset = 0;
        }
        xPool.m_ulTotalSize    = ulMaxSize;
        xPool.m_ulMaxAlignment = ulMaxAlignment;
    }
}

// Greedy interval coloring packer: for each memory-signature bin, sort
// transients by m_uFirstWrite and assign each to the first existing pool
// whose occupants do NOT overlap the new transient's lifetime; allocate a
// new pool when none fits. Pure pointer math — no Vulkan calls — so unit
// tests can exercise this without a live device. Runs after lifetime
// computation; no-op when aliasing is disabled.
//
// Algorithm cost: O(N * P) where N is number of transients and P is peak
// concurrent transients per bin. For a typical deferred frame (~30 transients,
// ~4-6 concurrent peak) that's sub-microsecond.
void Flux_RenderGraph::AssignAliasingGroups()
{
    // Tear down any previous aliasing state before deriving the new one. The
    // pool set is re-computed every Compile (pool count, size, and per-transient
    // assignments can all change when the aliasing toggle flips or the pass
    // graph changes), so the prior Compile's pools + aliased VkImages must be
    // queued for deferred deletion — otherwise each toggle of aliasing leaks
    // the entire previous pool set. Transient attachments are reset too,
    // forcing AllocateTransients to rebuild them against the new layout.
    ReleaseTransientAllocations();

    // Reset per-transient pool assignments. Non-aliased (toggle-off) path
    // leaves these as UINT32_MAX which AllocateTransients reads as "allocate
    // standalone".
    for (u_int u = 0; u < m_axTransients.GetSize(); u++)
    {
        TransientResource* pxT = m_axTransients.Get(u);
        pxT->m_uAliasPoolIndex = UINT32_MAX;
        pxT->m_ulAliasOffset   = 0;
    }

    const bool bAliasingActive = m_bAliasingEnabled
        && g_xEngine.FluxMemory().SupportsTransientAliasing();
    if (!bAliasingActive)
        return;

    Zenith_Vector<u_int> axSortedIndices;
    SortTransientsByLifetime(axSortedIndices);
    PackTransientsIntoPools(axSortedIndices);
    ComputePoolSizes();
}

// Build a Flux_BarrierKey for (pointer, mip, layer). Used by clear-flag tracking
// to grant "first writer" status per (attachment, mip, face), and by the
// barrier-state map in SynthesizeBarriers. The key holds the FULL pointer (no
// 48-bit truncation) alongside a u_int32 that packs (layer<<16)|mip. The
// static_asserts pin the packing so bumping FLUX_MAX_MIPS or FLUX_MAX_LAYERS
// above the 16-bit limit fails the build instead of producing silent
// collisions at runtime.
static_assert(FLUX_MAX_MIPS <= 0xFFFFu,
    "Flux_BarrierKey packs mip index into 16 bits. If FLUX_MAX_MIPS grows, widen m_uMipLayerPacked or split the struct.");
static_assert(FLUX_MAX_LAYERS <= 0xFFFFu,
    "Flux_BarrierKey packs layer index into 16 bits. If FLUX_MAX_LAYERS grows, widen m_uMipLayerPacked or split the struct.");

static inline Flux_BarrierKey MakeBarrierKey(void* pRes, u_int uMip, u_int uLayer)
{
    Zenith_Assert(uMip   < FLUX_MAX_MIPS,   "MakeBarrierKey: mip index %u >= FLUX_MAX_MIPS (%u)",   uMip,   FLUX_MAX_MIPS);
    Zenith_Assert(uLayer < FLUX_MAX_LAYERS, "MakeBarrierKey: layer index %u >= FLUX_MAX_LAYERS (%u)", uLayer, FLUX_MAX_LAYERS);

    Flux_BarrierKey xKey;
    xKey.m_pRes = pRes;
    xKey.m_uMipLayerPacked = (uLayer << 16) | (uMip & 0xFFFFu);
    return xKey;
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
    m_xAttachmentNeedsClear.Clear();
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bIsCompute || !IsPassEffectivelyEnabled(pxPass)) continue;
        if (pxPass->m_uNumColourAttachments == 0 && !pxPass->m_xDepthStencil.IsValid()) continue;
        for (uint32_t i = 0; i < pxPass->m_uNumColourAttachments; i++)
        {
            const Flux_RenderGraph_AttachmentRef& rxRef = pxPass->m_axColourAttachments[i];
            const Flux_BarrierKey ulKey = MakeBarrierKey(rxRef.m_xResource.GetVoidPtr(), rxRef.m_uMip, rxRef.m_uLayer);
            if (bool* pbExisting = m_xAttachmentNeedsClear.TryGet(ulKey))
                *pbExisting = *pbExisting || pxPass->m_bRequestsClear;
            else
                m_xAttachmentNeedsClear.Insert(ulKey, pxPass->m_bRequestsClear);
        }
        if (pxPass->m_xDepthStencil.IsValid())
        {
            const Flux_RenderGraph_AttachmentRef& rxDepth = pxPass->m_xDepthStencil;
            const Flux_BarrierKey ulKey = MakeBarrierKey(rxDepth.m_xResource.GetVoidPtr(), rxDepth.m_uMip, rxDepth.m_uLayer);
            if (bool* pbExisting = m_xAttachmentNeedsClear.TryGet(ulKey))
                *pbExisting = *pbExisting || pxPass->m_bRequestsClear;
            else
                m_xAttachmentNeedsClear.Insert(ulKey, pxPass->m_bRequestsClear);
        }
    }
}

void Flux_RenderGraph::AssignClearFlags()
{
    m_xAttachmentClearAssigned.Clear();
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bIsCompute || !IsPassEffectivelyEnabled(pxPass)) continue;
        if (pxPass->m_uNumColourAttachments == 0 && !pxPass->m_xDepthStencil.IsValid()) continue;
        bool bNeedsClear = false;
        for (uint32_t i = 0; i < pxPass->m_uNumColourAttachments; i++)
        {
            const Flux_RenderGraph_AttachmentRef& rxRef = pxPass->m_axColourAttachments[i];
            const Flux_BarrierKey ulKey = MakeBarrierKey(rxRef.m_xResource.GetVoidPtr(), rxRef.m_uMip, rxRef.m_uLayer);
            if (const bool* pbNeed = m_xAttachmentNeedsClear.TryGet(ulKey); pbNeed && *pbNeed) { bNeedsClear = true; break; }
        }
        if (!bNeedsClear && pxPass->m_xDepthStencil.IsValid())
        {
            const Flux_RenderGraph_AttachmentRef& rxDepth = pxPass->m_xDepthStencil;
            const Flux_BarrierKey ulKey = MakeBarrierKey(rxDepth.m_xResource.GetVoidPtr(), rxDepth.m_uMip, rxDepth.m_uLayer);
            if (const bool* pbNeed = m_xAttachmentNeedsClear.TryGet(ulKey); pbNeed && *pbNeed) bNeedsClear = true;
        }
        if (!bNeedsClear) continue;
        bool bIsFirstWriter = false;
        for (uint32_t i = 0; i < pxPass->m_uNumColourAttachments; i++)
        {
            const Flux_RenderGraph_AttachmentRef& rxRef = pxPass->m_axColourAttachments[i];
            const Flux_BarrierKey ulKey = MakeBarrierKey(rxRef.m_xResource.GetVoidPtr(), rxRef.m_uMip, rxRef.m_uLayer);
            if (!m_xAttachmentClearAssigned.Contains(ulKey))
            { m_xAttachmentClearAssigned.Insert(ulKey); bIsFirstWriter = true; }
        }
        if (pxPass->m_xDepthStencil.IsValid())
        {
            const Flux_RenderGraph_AttachmentRef& rxDepth = pxPass->m_xDepthStencil;
            const Flux_BarrierKey ulKey = MakeBarrierKey(rxDepth.m_xResource.GetVoidPtr(), rxDepth.m_uMip, rxDepth.m_uLayer);
            if (!m_xAttachmentClearAssigned.Contains(ulKey))
            { m_xAttachmentClearAssigned.Insert(ulKey); bIsFirstWriter = true; }
        }
        pxPass->m_bClearTargets = bIsFirstWriter;
    }
}


bool Flux_RenderGraph::Compile()
{
    Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Flux_RenderGraph::Compile: must be called from main thread");
    if (!m_bDirty) return m_bCompiled;
    if (m_xPasses.GetSize() == 0) { m_bCompiled = true; m_bDirty = false; m_bEnabledMaskDirty = false; return true; }
    BuildResourceTraffic();
    Validate();
    if (!TopologicalSort()) return false;
    ComputeResourceLifetimes();
    ComputeTransientLifetimes();
    AssignAliasingGroups();
    InferPassAttachments();
    MarkPassesAsComputeOrGraphics();
    ResolveClearFlags();
    AllocateTransients();

    // Post-allocation sanity checks. By this point every transient attachment
    // has a valid VRAM handle and surface info — anything that couldn't be
    // validated at declaration time (because the attachment wasn't built yet)
    // is checked now.
    for (u_int i = 0; i < m_axTransients.GetSize(); i++)
    {
        const TransientResource* pxT = m_axTransients.Get(i);
        Zenith_Assert(pxT->m_bAllocated,
            "Flux_RenderGraph::Compile: transient #%u failed to allocate", i);
        Zenith_Assert(pxT->m_xAttachment.m_xSurfaceInfo.m_uNumMips == pxT->m_xDesc.m_uNumMips,
            "Flux_RenderGraph::Compile: transient #%u mip count mismatch (desc %u, attachment %u)",
            i, pxT->m_xDesc.m_uNumMips, pxT->m_xAttachment.m_xSurfaceInfo.m_uNumMips);
    }
    // Re-run the memory-flag compatibility check on every pass now that
    // transient attachments have real surface info.
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        const Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!IsPassEffectivelyEnabled(pxP)) continue;
        ValidatePassMemoryFlagCompatibility(pxP);
    }

    SynthesizeBarriers();
    SynthesizeAliasingBarriers();

    m_bCompiled = true; m_bDirty = false; m_bEnabledMaskDirty = false;
#ifdef ZENITH_DEBUG
    Zenith_Log(LOG_CATEGORY_RENDERER, "RenderGraph: Compiled %u passes", m_xExecutionOrder.GetSize());
#endif
    return true;
}

// ---- Barrier synthesis ---------------------------------------------------
//
// Model
//   The graph is the sole barrier authority. SynthesizeBarriers walks the
//   execution order, tracks per-(image, mip, layer) access state, and
//   populates each pass's m_xPrologueBarriers with the transitions needed
//   to put every declared subresource into the right layout BEFORE the
//   pass runs. The Vulkan backend emits those barriers via
//   Zenith_Vulkan_CommandBuffer::ImageTransition outside any active render
//   pass scope and DOES NOT emit any transitions of its own (the previous
//   TransitionTargetsForRenderPass / TransitionTargetsAfterRenderPass path
//   was deleted along with this rewrite).
//
//   First touch defaults to UNDEFINED → ImageTransition turns that into a
//   discard transition, which is correct for the first frame and idempotent
//   for transients reused across frames.
//
//   Render-pass initialLayout / finalLayout are now identical (the working
//   layout of the access — COLOR_ATTACHMENT / DEPTH_ATTACHMENT / DEPTH_RO),
//   so the render pass itself never transitions layouts. The graph puts
//   the image in the right layout, the render pass uses it, the next pass's
//   prologue barriers move it elsewhere if needed.

// Layout the resource MUST be in when this pass starts executing. With the
// graph owning all transitions, the required pre-pass layout is just the
// access's natural layout — no special pre-staging for backend assumptions.
static ResourceAccess RequiredPrePassAccess(ResourceAccess eAccess)
{
    switch (eAccess)
    {
        case RESOURCE_ACCESS_READ_SRV:           return RESOURCE_ACCESS_READ_SRV;           // SHADER_READ_ONLY
        case RESOURCE_ACCESS_READ_DEPTH:         return RESOURCE_ACCESS_READ_DEPTH;         // DEPTH_READ_ONLY
        case RESOURCE_ACCESS_WRITE_RTV:          return RESOURCE_ACCESS_WRITE_RTV;          // COLOR_ATTACHMENT
        case RESOURCE_ACCESS_WRITE_DSV:          return RESOURCE_ACCESS_WRITE_DSV;          // DEPTH_STENCIL_ATTACHMENT
        case RESOURCE_ACCESS_WRITE_UAV:          return RESOURCE_ACCESS_WRITE_UAV;          // GENERAL
        case RESOURCE_ACCESS_READWRITE_UAV:      return RESOURCE_ACCESS_READWRITE_UAV;      // GENERAL
        case RESOURCE_ACCESS_READ_INDIRECT_ARG:  return RESOURCE_ACCESS_READ_INDIRECT_ARG;  // buffer-only, no layout
        case RESOURCE_ACCESS_READ_BUFFER_SRV:    return RESOURCE_ACCESS_READ_BUFFER_SRV;    // buffer-only, no layout
        case RESOURCE_ACCESS_HOST_TRANSFER_WRITE: return RESOURCE_ACCESS_HOST_TRANSFER_WRITE; // buffer-only, never appears as a destination
        case RESOURCE_ACCESS_UNDEFINED:          return RESOURCE_ACCESS_UNDEFINED;
    }
    Zenith_Assert(false, "RequiredPrePassAccess: unknown access %d", (int)eAccess);
    return RESOURCE_ACCESS_UNDEFINED;
}

// Layout the resource is in AFTER this pass executes. With the render pass's
// finalLayout matching its initialLayout (no auto-transition), the resource
// stays in the working layout of its access — so this is an identity function
// today. The indirection is kept so a future layout-shifting render pass
// (e.g. auto-transitioning attachments to SHADER_READ at finalLayout) has a
// single hook to update, rather than touching every call site in the barrier
// synthesiser.
static ResourceAccess PostPassAccess(ResourceAccess eAccess)
{
    return eAccess;
}

// READWRITE_UAV and WRITE_UAV both map to GENERAL — treat them as one layout.
// READ_SRV from a previous WRITE_UAV needs an explicit transition
// (GENERAL → SHADER_READ_ONLY) so they are NOT the same layout.
static bool SameLayout(ResourceAccess a, ResourceAccess b)
{
    auto Norm = [](ResourceAccess e) -> ResourceAccess
    {
        if (e == RESOURCE_ACCESS_READWRITE_UAV) return RESOURCE_ACCESS_WRITE_UAV;
        return e;
    };
    return Norm(a) == Norm(b);
}

// True if this access *writes* to the subresource (or potentially does, in
// the case of READWRITE_UAV). Same-layout transitions still need a barrier
// when either side is a write, to satisfy WAW / RAW hazards even though the
// layout itself doesn't change (e.g. two consecutive RTV writes to the same
// attachment in separate render passes need a ColorAttachmentOutput→
// ColorAttachmentOutput memory barrier).
static bool AccessIsWrite(ResourceAccess e)
{
    return e == RESOURCE_ACCESS_WRITE_RTV
        || e == RESOURCE_ACCESS_WRITE_DSV
        || e == RESOURCE_ACCESS_WRITE_UAV
        || e == RESOURCE_ACCESS_READWRITE_UAV
        || e == RESOURCE_ACCESS_HOST_TRANSFER_WRITE;
}

// File-local result of DecideBarrierNeeded. Unifies the image and buffer
// paths of SynthesizeBarriers so the barrier predicate lives in one place.
struct BarrierDecision
{
    bool           m_bNeedsBarrier = false;
    ResourceAccess m_eSrcAccess    = RESOURCE_ACCESS_UNDEFINED;
    ResourceAccess m_eDstAccess    = RESOURCE_ACCESS_UNDEFINED;
};

// Buffers have no layout (pure memory barriers), so they skip the layout
// match test. Images emit on any layout change, WAW, RAW, or first-touch-
// after-undefined; the caller must have resolved eRequired via
// RequiredPrePassAccess and sampled eSrc from the live state map.
static BarrierDecision DecideBarrierNeeded(ResourceAccess eSrc,
                                           ResourceAccess eRequired,
                                           bool bWriteAccess,
                                           bool bIsBuffer)
{
    const bool bSrcIsWrite    = AccessIsWrite(eSrc);
    const bool bLayoutMatches = bIsBuffer ? true : SameLayout(eSrc, eRequired);
    BarrierDecision xDecision;
    xDecision.m_bNeedsBarrier = !bLayoutMatches || bWriteAccess || bSrcIsWrite;
    xDecision.m_eSrcAccess    = eSrc;
    xDecision.m_eDstAccess    = eRequired;
    return xDecision;
}

namespace
{
    // Rolling per-subresource resource-access tracker used during barrier synthesis.
    struct BarrierStateTracker
    {
        Zenith_HashMap<Flux_BarrierKey, ResourceAccess> m_xImageState;
        Zenith_HashMap<Flux_Buffer*, ResourceAccess> m_xBufferState;

        ResourceAccess QueryImageState(void* pRes, u_int uMip, u_int uLayer) const
        {
            const Flux_BarrierKey ulKey = MakeBarrierKey(pRes, uMip, uLayer);
            const ResourceAccess* pe = m_xImageState.TryGet(ulKey);
            return (pe == nullptr) ? RESOURCE_ACCESS_UNDEFINED : *pe;
        }
        void SetImageState(void* pRes, u_int uMip, u_int uLayer, ResourceAccess e)
        {
            m_xImageState.Insert(MakeBarrierKey(pRes, uMip, uLayer), e);
        }
        ResourceAccess QueryBufferState(Flux_Buffer* pxBuffer) const
        {
            const ResourceAccess* pe = m_xBufferState.TryGet(pxBuffer);
            return (pe == nullptr) ? RESOURCE_ACCESS_UNDEFINED : *pe;
        }
        void SetBufferState(Flux_Buffer* pxBuffer, ResourceAccess e)
        {
            m_xBufferState.Insert(pxBuffer, e);
        }
    };

    // Compute the subresource range for an image usage, honouring ALL_MIPS / ALL_LAYERS.
    // Returns false if the usage isn't image-like.
    bool ResolveSubresourceRange(const Flux_RenderGraph_ResourceUsage& rxUsage,
                                 u_int& uOutBaseMip, u_int& uOutMipCount,
                                 u_int& uOutBaseLayer, u_int& uOutLayerCount)
    {
        if (!rxUsage.m_xResource.IsImageLike()) return false;
        const Flux_SurfaceInfo& rxInfo = rxUsage.m_xResource.GetSurfaceInfo();
        uOutBaseMip = rxUsage.m_uMipLevel;
        uOutMipCount = (rxUsage.m_uMipCount == FLUX_RG_ALL_MIPS) ? rxInfo.m_uNumMips : rxUsage.m_uMipCount;
        uOutBaseLayer = rxUsage.m_uLayer;
        uOutLayerCount = (rxUsage.m_uLayerCount == FLUX_RG_ALL_LAYERS) ? rxInfo.m_uNumLayers : rxUsage.m_uLayerCount;
        return true;
    }

    // Buffer barrier emission: pure memory + execution barriers (no layout
    // transitions). A barrier is needed any time either side is a write
    // (RAW, WAW, WAR); read-after-read collapses to no barrier. The
    // Flux_RenderGraph_Barrier mip/layer fields stay at their (0, 1, 0, 1)
    // defaults — the backend buffer-barrier emitter ignores them.
    void ProcessBufferAccess(Flux_RenderGraph_Pass* pxPass,
                             const Flux_RenderGraph_ResourceUsage& rxUsage,
                             BarrierStateTracker& xTracker)
    {
        Flux_Buffer* pxBuffer = rxUsage.m_xResource.AsBuffer();
        if (pxBuffer == nullptr) return;
        // Reject the resource if the underlying VRAM isn't allocated yet
        // (shouldn't happen post-AllocateTransients).
        if (!pxBuffer->m_xVRAMHandle.IsValid()) return;

        const ResourceAccess eRequired = RequiredPrePassAccess(rxUsage.m_eAccess);
        const ResourceAccess ePost = PostPassAccess(rxUsage.m_eAccess);
        const bool bWriteAccess = AccessIsWrite(rxUsage.m_eAccess);

        const ResourceAccess eSrc = xTracker.QueryBufferState(pxBuffer);
        const BarrierDecision xDecision = DecideBarrierNeeded(eSrc, eRequired, bWriteAccess, /*bIsBuffer*/ true);
        if (xDecision.m_bNeedsBarrier)
        {
            Flux_RenderGraph_Barrier xBarrier;
            xBarrier.m_xResource = rxUsage.m_xResource;
            xBarrier.m_eSrcAccess = xDecision.m_eSrcAccess;
            xBarrier.m_eDstAccess = xDecision.m_eDstAccess;
            pxPass->m_xPrologueBarriers.PushBack(xBarrier);
        }
        xTracker.SetBufferState(pxBuffer, ePost);
    }

    // Image barrier emission: emits a prologue barrier per (mip, layer) where
    // current layout ≠ required OR either side is a write. Updates per-subresource state.
    void ProcessImageAccess(Flux_RenderGraph_Pass* pxPass,
                            const Flux_RenderGraph_ResourceUsage& rxUsage,
                            BarrierStateTracker& xTracker)
    {
        u_int uBaseMip, uMipCount, uBaseLayer, uLayerCount;
        if (!ResolveSubresourceRange(rxUsage, uBaseMip, uMipCount, uBaseLayer, uLayerCount)) return;

        Flux_VRAMHandle xHandle = rxUsage.m_xResource.GetVRAMHandle();
        if (!xHandle.IsValid()) return;

        const ResourceAccess eRequired = RequiredPrePassAccess(rxUsage.m_eAccess);
        const ResourceAccess ePost = PostPassAccess(rxUsage.m_eAccess);
        const bool bWriteAccess = AccessIsWrite(rxUsage.m_eAccess);
        void* pRes = rxUsage.m_xResource.GetVoidPtr();

        for (u_int uLayer = uBaseLayer; uLayer < uBaseLayer + uLayerCount; uLayer++)
        {
            for (u_int uMip = uBaseMip; uMip < uBaseMip + uMipCount; uMip++)
            {
                const ResourceAccess eSrc = xTracker.QueryImageState(pRes, uMip, uLayer);
                const BarrierDecision xDecision = DecideBarrierNeeded(eSrc, eRequired, bWriteAccess, /*bIsBuffer*/ false);
                if (xDecision.m_bNeedsBarrier)
                {
                    Flux_RenderGraph_Barrier xBarrier;
                    xBarrier.m_xResource = rxUsage.m_xResource;
                    xBarrier.m_uBaseMip = uMip;
                    xBarrier.m_uMipCount = 1;
                    xBarrier.m_uBaseLayer = uLayer;
                    xBarrier.m_uLayerCount = 1;
                    xBarrier.m_eSrcAccess = xDecision.m_eSrcAccess;
                    xBarrier.m_eDstAccess = xDecision.m_eDstAccess;
                    pxPass->m_xPrologueBarriers.PushBack(xBarrier);
                }
                xTracker.SetImageState(pRes, uMip, uLayer, ePost);
            }
        }
    }

    // Dispatch a single usage to the buffer or image barrier path based on kind.
    void ProcessUsageAccess(Flux_RenderGraph_Pass* pxPass,
                            const Flux_RenderGraph_ResourceUsage& rxUsage,
                            BarrierStateTracker& xTracker)
    {
        if (rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
            ProcessBufferAccess(pxPass, rxUsage, xTracker);
        else
            ProcessImageAccess(pxPass, rxUsage, xTracker);
    }

    // Cross-frame cyclic seed (buffers only). The graph is compiled ONCE and the
    // SAME pass order is re-executed every frame, so a persistent buffer
    // physically carries its contents AND its last access across the frame
    // boundary. But SynthesizeBarriers rebuilds from an empty tracker, so without
    // this the FIRST access of a buffer in the frame sees UNDEFINED state and gets
    // a UNDEFINED->access barrier whose source stage is TOP_OF_PIPE — it waits for
    // NOTHING, in particular not for that buffer's read/write in the PREVIOUS
    // frame. With MAX_FRAMES_IN_FLIGHT>1 the GPU work of consecutive frames
    // overlaps (the per-frame fence only frees the slot from frame N-1, and no
    // semaphore chains frame N->N+1), so a per-frame GPU write of a persistent
    // buffer can race the prior frame's read of it (e.g. the instanced-mesh
    // reset/cull rewriting the visible-index / indirect-args while the prior
    // frame's indirect draw still reads them -> WRITE_AFTER_READ -> flicker).
    //
    // Seeding each buffer's state with its LAST PostPassAccess this frame makes the
    // first access emit the correct cyclic WAR/WAW barrier (its source IS the
    // prior frame's final access, because the order is identical). A pipeline
    // barrier's source scope covers all earlier-submitted queue work, so this
    // orders frame N+1's first write after frame N's last read of the same buffer.
    // Read-only buffers seed to a read state -> first access is also a read ->
    // read-after-read collapses to no barrier, so nothing spurious is added.
    //
    // Buffers ONLY: there are no transient buffers (CreateTransient is
    // texture-only), so every graph buffer is a stable subsystem-owned Flux_Buffer.
    // Images are excluded — transient images alias by pool and their cross-frame /
    // handoff synchronisation is owned by the aliasing barriers + layout tracking,
    // which a naive cyclic seed would fight.
    //
    // INVARIANT (cheap-toggle path): the seed reflects the CURRENT frame's enabled
    // mask, and is correct only because the EXECUTED pass order is byte-identical to
    // the prior frame for the buffers it touches. The SetEnabled cheap toggle re-runs
    // SynthesizeBarriers WITHOUT a topological re-sort (Flux_RenderGraph_Execution.cpp
    // m_bEnabledMaskDirty path), so on the frame a toggle flips, a buffer whose LAST
    // accessor is that togglable pass would get a seed that disagrees with the
    // in-flight prior-frame GPU access → a missing/wrong-src cross-frame barrier. This
    // is safe TODAY because every SetEnabled site toggles IMAGE-only passes (Fog/IBL/
    // SSGI/SSR/Skybox) and the seed only touches buffers — no seeded buffer has a
    // togglable accessor. If you ever add a SetEnabled-togglable pass that reads or
    // writes a persistent Flux_Buffer, route that toggle through MarkDirty() (full
    // Compile + re-seed) like Fog's technique swap does, NOT the cheap mask toggle.
    void SeedCyclicBufferState(const Flux_RenderGraph_ResourceUsage& rxUsage,
                               BarrierStateTracker& xTracker)
    {
        if (rxUsage.m_xResource.GetKind() != Flux_GraphResourceKind::Buffer)
            return;
        Flux_Buffer* pxBuffer = rxUsage.m_xResource.AsBuffer();
        if (pxBuffer == nullptr || !pxBuffer->m_xVRAMHandle.IsValid())
            return;
        xTracker.SetBufferState(pxBuffer, PostPassAccess(rxUsage.m_eAccess));
    }
}

void Flux_RenderGraph::SynthesizeBarriers()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
        m_xPasses.Get(i)->m_xPrologueBarriers.Clear();

    // Barrier synthesis rebuilds state from scratch on every Compile / re-synth,
    // never incrementally. Per-subresource image state and per-buffer state live
    // in the tracker.
    BarrierStateTracker xTracker;

    // Cross-frame cyclic seed (buffers only) — see SeedCyclicBufferState. Walk the
    // SAME pass order (and reads-before-writes per pass) as the main loop below so
    // each persistent buffer's seeded state is EXACTLY its final access this frame;
    // the main loop's first access then sees that prior-frame state and emits the
    // cross-frame WAR/WAW barrier that the old "fresh UNDEFINED every frame"
    // behaviour silently dropped. Images are intentionally not seeded here.
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(itE.GetData());
        if (!IsPassEffectivelyEnabled(pxPass)) continue;
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
            SeedCyclicBufferState(it.GetData(), xTracker);
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
            SeedCyclicBufferState(it.GetData(), xTracker);
    }

    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(itE.GetData());
        if (!IsPassEffectivelyEnabled(pxPass)) continue;

        // Reads first so READWRITE_UAV (appearing as a Read declaration when the
        // caller used the read-modify-write convention) sets state to the RMW
        // layout before any subsequent write tries to transition again.
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
            ProcessUsageAccess(pxPass, it.GetData(), xTracker);
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
            ProcessUsageAccess(pxPass, it.GetData(), xTracker);
    }
}

void Flux_RenderGraph::MarkPassesAsComputeOrGraphics()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        pxP->m_bIsCompute = (pxP->m_uNumColourAttachments == 0 && !pxP->m_xDepthStencil.IsValid());
    }
}

// Execute / Record / Submit / InferPassAttachments + the thread-local
// recording-context subsystem (tls_pxCurrentRecordingPass/Graph,
// CurrentPassScope, Flux_RenderGraph_RecordPassTask) live in
// Flux_RenderGraph_Execution.cpp. AssertBoundResourceDeclared and
// GetCurrentRecordingPass move with them because they read the tls_ state.

// Execute(), CallPrepareCallbacks(), RecordCommandLists(),
// SubmitRecordedLists(), InferPassAttachments(), GetCurrentRecordingPass(),
// CurrentPassScope ctor/dtor, and AssertBoundResourceDeclared() all live in
// Flux_RenderGraph_Execution.cpp (see that file for the tls_ recording state
// they share with the Flux_RenderGraph_RecordPassTask task function).
