#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Multithreading/Zenith_Multithreading.h"

// Hard cap on pass count. Chosen to be generous (current engine sits around
// 60 passes); a runaway loop registering passes trips the assert before it
// chews all of heap / the topo-sort adjacency vectors blow up.
static constexpr u_int kMaxPassCount = 256;

// Monotonic graph-instance ID counter. Starts at 1 so 0 stays reserved as the
// "no graph" sentinel on default-constructed handles. Bumped by the
// Flux_RenderGraph constructor for every new instance.
u_int Flux_RenderGraph::s_uNextGraphInstanceID = 1;

// Forward-declare the transient-size estimator used by AllocateTransients
// and AssignAliasingGroups. Defined further down beside the other aliasing
// helpers; the forward decl lets the earlier functions call it.
static u_int64 EstimateTransientImageSize(const Flux_TransientTextureDesc& xDesc);

// ---- Access-mode classification helpers --------------------------------
// Centralised so assertions across AddResourceUsage, Validate, bind-time
// checks all agree on what a given ResourceAccess means. Any new access mode
// added to Flux_Enums.h must be classified here or the default branch trips.
static bool IsReadAccess(ResourceAccess eAccess)
{
    switch (eAccess)
    {
        case RESOURCE_ACCESS_READ_SRV:
        case RESOURCE_ACCESS_READ_DEPTH:
        case RESOURCE_ACCESS_READWRITE_UAV:
        case RESOURCE_ACCESS_READ_INDIRECT_ARG:
        case RESOURCE_ACCESS_READ_BUFFER_SRV:
            return true;
        case RESOURCE_ACCESS_UNDEFINED:
        case RESOURCE_ACCESS_WRITE_RTV:
        case RESOURCE_ACCESS_WRITE_DSV:
        case RESOURCE_ACCESS_WRITE_UAV:
        case RESOURCE_ACCESS_HOST_TRANSFER_WRITE:
            return false;
    }
    Zenith_Assert(false, "IsReadAccess: unknown ResourceAccess %d", (int)eAccess);
    return false;
}

static bool IsWriteAccess(ResourceAccess eAccess)
{
    switch (eAccess)
    {
        case RESOURCE_ACCESS_WRITE_RTV:
        case RESOURCE_ACCESS_WRITE_DSV:
        case RESOURCE_ACCESS_WRITE_UAV:
        case RESOURCE_ACCESS_READWRITE_UAV:
        case RESOURCE_ACCESS_HOST_TRANSFER_WRITE:
            return true;
        case RESOURCE_ACCESS_UNDEFINED:
        case RESOURCE_ACCESS_READ_SRV:
        case RESOURCE_ACCESS_READ_DEPTH:
        case RESOURCE_ACCESS_READ_INDIRECT_ARG:
        case RESOURCE_ACCESS_READ_BUFFER_SRV:
            return false;
    }
    Zenith_Assert(false, "IsWriteAccess: unknown ResourceAccess %d", (int)eAccess);
    return false;
}

static bool IsAccessLegalForKind(ResourceAccess eAccess, Flux_GraphResourceKind eKind)
{
    switch (eKind)
    {
        case Flux_GraphResourceKind::Image:
        case Flux_GraphResourceKind::ImageCube:
            // Image-like resources support every texture access mode. The two
            // buffer-only access values are explicitly rejected so a misuse
            // (declaring an indirect-arg read on an image) trips immediately
            // instead of silently passing barrier synthesis.
            return eAccess != RESOURCE_ACCESS_UNDEFINED
                && eAccess != RESOURCE_ACCESS_READ_INDIRECT_ARG
                && eAccess != RESOURCE_ACCESS_READ_BUFFER_SRV;
        case Flux_GraphResourceKind::Buffer:
            // Buffers can't be render targets or depth-stencil attachments; they
            // support SRV (structured read), UAV (compute write), read-modify-write
            // UAV, plus the buffer-only access modes for indirect-arg reads,
            // read-only structured buffers, and host transfer writes (synthetic
            // predecessor for staging-buffer uploads — see Flux_Enums.h).
            return eAccess == RESOURCE_ACCESS_READ_SRV
                || eAccess == RESOURCE_ACCESS_WRITE_UAV
                || eAccess == RESOURCE_ACCESS_READWRITE_UAV
                || eAccess == RESOURCE_ACCESS_READ_INDIRECT_ARG
                || eAccess == RESOURCE_ACCESS_READ_BUFFER_SRV
                || eAccess == RESOURCE_ACCESS_HOST_TRANSFER_WRITE;
    }
    return false;
}

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

// Subresource ranges [uMipA, uMipA+uMipACount) vs [uMipB, uMipB+uMipBCount)
// and same for layers. Returns true if any (mip, layer) pair is in both.
static bool SubresourceRangesOverlap(u_int uMipA, u_int uMipACount, u_int uLayerA, u_int uLayerACount,
                                     u_int uMipB, u_int uMipBCount, u_int uLayerB, u_int uLayerBCount)
{
    // Treat FLUX_RG_ALL_* as a maximal range so it overlaps with any concrete range.
    auto RangesOverlap = [](u_int a0, u_int aN, u_int b0, u_int bN) {
        if (aN == FLUX_RG_ALL_MIPS || bN == FLUX_RG_ALL_MIPS) return true;
        return (a0 < b0 + bN) && (b0 < a0 + aN);
    };
    return RangesOverlap(uMipA, uMipACount, uMipB, uMipBCount)
        && RangesOverlap(uLayerA, uLayerACount, uLayerB, uLayerBCount);
}

// ---- AddResourceUsage validation helpers --------------------------------
// Split out of AddResourceUsage so the dispatcher reads top-down: caller-intent
// check, kind-legality, then per-kind structural checks, then conflict scan.
// READWRITE_UAV is the only access mode legal in both directions (read-modify-
// write); other modes must agree with whether Read() or Write() invoked us.
static void ValidateAccessMode(ResourceAccess eAccess, bool bWrite, u_int uPassIndex)
{
    if (eAccess == RESOURCE_ACCESS_READWRITE_UAV) return;
    if (bWrite)
    {
        Zenith_Assert(IsWriteAccess(eAccess),
            "Flux_RenderGraph::Write: access '%s' is not a write mode (pass %u)",
            AccessToString(eAccess), uPassIndex);
    }
    else
    {
        Zenith_Assert(IsReadAccess(eAccess),
            "Flux_RenderGraph::Read: access '%s' is not a read mode (pass %u)",
            AccessToString(eAccess), uPassIndex);
    }
}

// Image / image-cube subresource bounds + depth-format match + memory-flag
// compatibility. Skipped until the attachment has been built (transient
// resources defer these checks to Validate() after AllocateTransients).
static void ValidateImageSubresource(const Flux_GraphResource& xResource, ResourceAccess eAccess,
                                     u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount,
                                     u_int uPassIndex, const char* szCaller)
{
    const Flux_SurfaceInfo& rxInfo = xResource.GetSurfaceInfo();
    const bool bInfoKnown = rxInfo.m_uWidth > 0;
    if (!bInfoKnown) return;

    if (uMipCount != FLUX_RG_ALL_MIPS)
    {
        Zenith_Assert(uMip < rxInfo.m_uNumMips,
            "Flux_RenderGraph::%s: mip %u out of range (attachment has %u mips), pass %u",
            szCaller, uMip, rxInfo.m_uNumMips, uPassIndex);
        Zenith_Assert(uMip + uMipCount <= rxInfo.m_uNumMips,
            "Flux_RenderGraph::%s: mip range [%u,%u) exceeds attachment mip count %u, pass %u",
            szCaller, uMip, uMip + uMipCount, rxInfo.m_uNumMips, uPassIndex);
    }
    // For cubemaps, layer must fit in 6 faces; for 2D, layer must be 0 with count 1.
    const u_int uMaxLayers = (xResource.GetKind() == Flux_GraphResourceKind::ImageCube) ? rxInfo.m_uNumLayers : 1u;
    if (uLayerCount != FLUX_RG_ALL_LAYERS)
    {
        Zenith_Assert(uLayer < uMaxLayers,
            "Flux_RenderGraph::%s: layer %u out of range (attachment has %u layers), pass %u",
            szCaller, uLayer, uMaxLayers, uPassIndex);
        Zenith_Assert(uLayer + uLayerCount <= uMaxLayers,
            "Flux_RenderGraph::%s: layer range [%u,%u) exceeds attachment layer count %u, pass %u",
            szCaller, uLayer, uLayer + uLayerCount, uMaxLayers, uPassIndex);
    }

    if (eAccess == RESOURCE_ACCESS_WRITE_DSV || eAccess == RESOURCE_ACCESS_READ_DEPTH)
    {
        Zenith_Assert(IsDepthFormat(rxInfo.m_eFormat),
            "Flux_RenderGraph::%s: depth access '%s' on non-depth format (pass %u)",
            szCaller, AccessToString(eAccess), uPassIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_RTV)
    {
        Zenith_Assert(!IsDepthFormat(rxInfo.m_eFormat),
            "Flux_RenderGraph::%s: colour write 'WRITE_RTV' on depth-format attachment (pass %u) — use WRITE_DSV",
            szCaller, uPassIndex);
    }

    if (eAccess == RESOURCE_ACCESS_READ_SRV)
    {
        Zenith_Assert(rxInfo.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
            "Flux_RenderGraph::%s: READ_SRV requires MEMORY_FLAGS__SHADER_READ on attachment (pass %u)",
            szCaller, uPassIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_UAV || eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(rxInfo.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::%s: '%s' requires MEMORY_FLAGS__UNORDERED_ACCESS on attachment (pass %u)",
            szCaller, AccessToString(eAccess), uPassIndex);
    }
}

// Buffers have no mip/layer geometry; sole structural check is non-zero size.
static void ValidateBufferUsage(const Flux_GraphResource& xResource, u_int uMip, u_int uMipCount,
                                u_int uLayer, u_int uLayerCount, u_int uPassIndex, const char* szCaller)
{
    Zenith_Assert(uMip == 0 && uMipCount == 1, "Flux_RenderGraph::%s: buffers have no mip levels (pass %u)", szCaller, uPassIndex);
    Zenith_Assert(uLayer == 0 && uLayerCount == 1, "Flux_RenderGraph::%s: buffers have no array layers (pass %u)", szCaller, uPassIndex);
    const Flux_Buffer* pxBuf = xResource.AsBuffer();
    Zenith_Assert(pxBuf->m_ulSize > 0, "Flux_RenderGraph::%s: buffer has zero size (pass %u)", szCaller, uPassIndex);
}

// Scan existing pass usages for two failure modes:
//   1. Same-direction overlap with conflicting access (e.g. WRITE_RTV + WRITE_UAV
//      on the same subresource): can't serve both in one pass.
//   2. Opposite-direction overlap (any Read overlapping any Write on the same
//      subresource): must be declared once as RESOURCE_ACCESS_READWRITE_UAV
//      instead — paired Read/Write would emit a redundant intra-pass barrier.
static void CheckSubresourceConflicts(Flux_RenderGraph_Pass* pxPass, const Flux_GraphResource& xResource,
                                      ResourceAccess eAccess, u_int uMip, u_int uMipCount,
                                      u_int uLayer, u_int uLayerCount, bool bWrite,
                                      u_int uPassIndex, const char* szCaller)
{
    const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxExisting = bWrite ? pxPass->m_xWrites : pxPass->m_xReads;
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxExisting); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_ResourceUsage& rxPrev = it.GetData();
        if (rxPrev.m_xResource != xResource) continue;
        if (!SubresourceRangesOverlap(rxPrev.m_uMipLevel, rxPrev.m_uMipCount, rxPrev.m_uLayer, rxPrev.m_uLayerCount,
                                      uMip, uMipCount, uLayer, uLayerCount)) continue;
        Zenith_Assert(rxPrev.m_eAccess == eAccess,
            "Flux_RenderGraph::%s: pass %u declares overlapping %s on same resource with conflicting access '%s' vs '%s'",
            szCaller, uPassIndex, szCaller, AccessToString(rxPrev.m_eAccess), AccessToString(eAccess));
    }
    const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxOther = bWrite ? pxPass->m_xReads : pxPass->m_xWrites;
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxOther); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_ResourceUsage& rxPrev = it.GetData();
        if (rxPrev.m_xResource != xResource) continue;
        if (!SubresourceRangesOverlap(rxPrev.m_uMipLevel, rxPrev.m_uMipCount, rxPrev.m_uLayer, rxPrev.m_uLayerCount,
                                      uMip, uMipCount, uLayer, uLayerCount)) continue;
        Zenith_Assert(false,
            "Flux_RenderGraph: pass %u '%s' declares overlapping read and write on resource '%s' "
            "(prev '%s' mips[%u,%u) layers[%u,%u), new '%s' mips[%u,%u) layers[%u,%u)) — "
            "declare read-modify-write exactly once as RESOURCE_ACCESS_READWRITE_UAV on either "
            "Read() or Write() (not both), or narrow the subresource ranges so they don't overlap",
            uPassIndex, pxPass->DebugName(), xResource.GetName().c_str(),
            AccessToString(rxPrev.m_eAccess), rxPrev.m_uMipLevel,
                (rxPrev.m_uMipCount == FLUX_RG_ALL_MIPS ? rxPrev.m_uMipLevel + 99 : rxPrev.m_uMipLevel + rxPrev.m_uMipCount),
                rxPrev.m_uLayer,
                (rxPrev.m_uLayerCount == FLUX_RG_ALL_LAYERS ? rxPrev.m_uLayer + 99 : rxPrev.m_uLayer + rxPrev.m_uLayerCount),
            AccessToString(eAccess), uMip,
                (uMipCount == FLUX_RG_ALL_MIPS ? uMip + 99 : uMip + uMipCount),
                uLayer,
                (uLayerCount == FLUX_RG_ALL_LAYERS ? uLayer + 99 : uLayer + uLayerCount));
    }
}

void Flux_RenderGraph::AssertMutable(const char* szFn)
{
    Zenith_Assert(!m_bCompiled || m_bDirty, "Flux_RenderGraph::%s called after Compile() without MarkDirty()", szFn);
}

Flux_PassHandle Flux_RenderGraph::MakePassHandle(u_int uIndex) const
{
    Flux_PassHandle xHandle;
    xHandle.m_uIndex = uIndex;
    xHandle.m_uGeneration = m_uGeneration;
    xHandle.m_uGraphInstanceID = m_uGraphInstanceID;
    return xHandle;
}

void Flux_RenderGraph::AssertPassHandleValid(Flux_PassHandle xPass, const char* szCaller) const
{
    Zenith_Assert(xPass.IsValid(),
        "Flux_RenderGraph::%s: uninitialised pass handle", szCaller);
    Zenith_Assert(xPass.m_uGraphInstanceID == m_uGraphInstanceID,
        "Flux_RenderGraph::%s: pass handle belongs to a different graph instance (handle instance %u, this graph instance %u)",
        szCaller, xPass.m_uGraphInstanceID, m_uGraphInstanceID);
    Zenith_Assert(xPass.m_uGeneration == m_uGeneration,
        "Flux_RenderGraph::%s: stale pass handle (gen %u, graph gen %u) — handle was issued before a Clear()/rebuild",
        szCaller, xPass.m_uGeneration, m_uGeneration);
    Zenith_Assert(xPass.m_uIndex < m_xPasses.GetSize(),
        "Flux_RenderGraph::%s: pass index %u out of range (have %u)",
        szCaller, xPass.m_uIndex, m_xPasses.GetSize());
}

void Flux_RenderGraph::AssertTransientHandleValid(Flux_TransientHandle xHandle, const char* szCaller) const
{
    Zenith_Assert(xHandle.IsValid(),
        "Flux_RenderGraph::%s: uninitialised transient handle", szCaller);
    Zenith_Assert(xHandle.m_uGraphInstanceID == m_uGraphInstanceID,
        "Flux_RenderGraph::%s: transient handle belongs to a different graph instance (handle instance %u, this graph instance %u)",
        szCaller, xHandle.m_uGraphInstanceID, m_uGraphInstanceID);
    Zenith_Assert(xHandle.m_uGeneration == m_uGeneration,
        "Flux_RenderGraph::%s: stale transient handle (gen %u, graph gen %u) — handle was issued before a Clear()/rebuild",
        szCaller, xHandle.m_uGeneration, m_uGeneration);
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::%s: transient index %u out of range (have %u)",
        szCaller, xHandle.m_uIndex, m_axTransients.GetSize());
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
        xRes.m_uLastRead   = UINT32_MAX;
        xRes.m_uLastWrite  = UINT32_MAX;
        m_xResources[pRes] = xRes;
    }
}

Flux_PassBuilder Flux_RenderGraph::AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord, void* pUserData, u_int uInitialCapacity)
{
    AssertMutable("AddPass");
    Zenith_Assert(szName != nullptr, "Flux_RenderGraph::AddPass: null name");
    Zenith_Assert(szName[0] != '\0', "Flux_RenderGraph::AddPass: empty name — all passes must have a non-empty debug label");
    Zenith_Assert(pfnOnRecord != nullptr, "Flux_RenderGraph::AddPass: null record callback for pass '%s'", szName);
    Zenith_Assert(uInitialCapacity > 0, "Flux_RenderGraph::AddPass: command-list capacity must be > 0 for pass '%s'", szName);
    Zenith_Assert(m_xPasses.GetSize() < kMaxPassCount, "Flux_RenderGraph::AddPass: exceeded hard pass cap (%u) when adding '%s'", kMaxPassCount, szName);
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::AddPass: must be called from main thread ('%s')", szName);
#ifdef ZENITH_TOOLS
    // Duplicate-name detection: two passes sharing a name makes the ImGui panel
    // and assertion messages ambiguous, and usually indicates a copy-paste bug in
    // a subsystem's SetupRenderGraph. Only enforced in tools builds because
    // m_strName is tools-only.
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Zenith_Assert(m_xPasses.Get(i)->m_strName != szName,
            "Flux_RenderGraph::AddPass: duplicate pass name '%s' (already used by pass %u)", szName, i);
    }
#endif
    Flux_RenderGraph_Pass* pxPass = new Flux_RenderGraph_Pass();
#ifdef ZENITH_TOOLS
    pxPass->m_strName = szName;
#endif
    pxPass->m_pfnOnRecord = pfnOnRecord;
    pxPass->m_pUserData = pUserData;
    pxPass->m_pxCommandList = new Flux_CommandList(szName, uInitialCapacity);
    m_xPasses.PushBack(pxPass);
    const u_int uIdx = m_xPasses.GetSize() - 1;
    Zenith_Assert(m_xPasses.Get(uIdx) == pxPass, "Flux_RenderGraph::AddPass: pass index mismatch after PushBack ('%s')", szName);
    return Flux_PassBuilder(this, MakePassHandle(uIdx));
}

// ---- Flux_PassBuilder --------------------------------------------------
// All chain methods are &&-qualified, return Flux_PassBuilder&& via
// std::move(*this), and check the canary first. The graph's own handle-
// validation assertions run inside each forwarded call, so the builder adds
// only the canary check on top.

void Flux_PassBuilder::AssertAlive(const char* szCaller) const
{
    // Force a canary read even when Zenith_Assert compiles away in shipping
    // builds. Under ASan / MSan this load trips on use-after-free of the
    // builder's storage; without the volatile the compiler would delete the
    // whole body when the assert macro expands to nothing.
    const volatile u_int32 uCanary = m_uCanary;
    Zenith_Assert(uCanary == CANARY_LIVE,
        "Flux_PassBuilder::%s: canary mismatch (got 0x%08X, expected 0x%08X) — builder accessed after its temporary was destroyed. "
        "The `auto& x = xGraph.AddPass(...).Foo();` pattern captures a reference to a dying temporary; chain inline, or capture the Flux_PassHandle via implicit conversion and use the graph's Read/Write helpers.",
        szCaller, static_cast<u_int32>(uCanary), CANARY_LIVE);
}

Flux_PassBuilder&& Flux_PassBuilder::Reads(Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount) &&
{
    AssertAlive("Reads(Image)");
    m_pxGraph->Read(m_xPass, xImage, eAccess, uMip, uMipCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::Writes(Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount) &&
{
    AssertAlive("Writes(Image)");
    m_pxGraph->Write(m_xPass, xImage, eAccess, uMip, uMipCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::Reads(Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess,
                                           u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount) &&
{
    AssertAlive("Reads(Cube)");
    m_pxGraph->Read(m_xPass, xImage, eAccess, uMip, uMipCount, uLayer, uLayerCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::Writes(Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess,
                                            u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount) &&
{
    AssertAlive("Writes(Cube)");
    m_pxGraph->Write(m_xPass, xImage, eAccess, uMip, uMipCount, uLayer, uLayerCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::ReadsBuffer(Flux_Buffer& xBuffer, ResourceAccess eAccess) &&
{
    AssertAlive("ReadsBuffer");
    m_pxGraph->ReadBuffer(m_xPass, xBuffer, eAccess);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::WritesBuffer(Flux_Buffer& xBuffer, ResourceAccess eAccess) &&
{
    AssertAlive("WritesBuffer");
    m_pxGraph->WriteBuffer(m_xPass, xBuffer, eAccess);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::ReadsTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess) &&
{
    AssertAlive("ReadsTransient");
    m_pxGraph->ReadTransient(m_xPass, xHandle, eAccess);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::WritesTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess) &&
{
    AssertAlive("WritesTransient");
    m_pxGraph->WriteTransient(m_xPass, xHandle, eAccess);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::ReadsTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount) &&
{
    AssertAlive("ReadsTransient(subres)");
    m_pxGraph->ReadTransient(m_xPass, xHandle, eAccess, uMip, uMipCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::WritesTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount) &&
{
    AssertAlive("WritesTransient(subres)");
    m_pxGraph->WriteTransient(m_xPass, xHandle, eAccess, uMip, uMipCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::DependsOn(Flux_PassHandle xDependency) &&
{
    AssertAlive("DependsOn");
    m_pxGraph->DependsOn(m_xPass, xDependency);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::ClearTargets(bool bClear) &&
{
    AssertAlive("ClearTargets");
    m_pxGraph->SetClear(m_xPass, bClear);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::Prepare(Flux_RenderGraph_OnPrepareFunc pfnPrepare) &&
{
    AssertAlive("Prepare");
    m_pxGraph->SetPrepare(m_xPass, pfnPrepare);
    return std::move(*this);
}

void Flux_PassBuilder::SetUserData(void* pData)
{
    AssertAlive("UserData");
    m_pxGraph->AssertPassHandleValid(m_xPass, "Flux_PassBuilder::UserData");
    Flux_RenderGraph_Pass* pxPass = m_pxGraph->m_xPasses.Get(m_xPass.m_uIndex);
    pxPass->m_pUserData = pData;
}

void Flux_RenderGraph::AddResourceUsage(u_int uPassIndex, const Flux_GraphResource& xResource, ResourceAccess eAccess,
                                        u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount, bool bWrite)
{
    // Handle validity is checked at the public API layer by AssertPassHandleValid,
    // which this private helper's callers (Read/Write/*) invoke before calling.
    // Keep the index check here as defense in depth.
    const char* szCaller = bWrite ? "Write" : "Read";
    AssertMutable(szCaller);
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(),
        "Flux_RenderGraph::%s: invalid pass index %u (have %u)", szCaller, uPassIndex, m_xPasses.GetSize());
    Zenith_Assert(uMipCount > 0, "Flux_RenderGraph::%s: mip count must be > 0", szCaller);
    Zenith_Assert(uLayerCount > 0, "Flux_RenderGraph::%s: layer count must be > 0", szCaller);
    Zenith_Assert(xResource.IsValid(), "Flux_RenderGraph::%s: resource is null", szCaller);

    ValidateAccessMode(eAccess, bWrite, uPassIndex);
    Zenith_Assert(IsAccessLegalForKind(eAccess, xResource.GetKind()),
        "Flux_RenderGraph::%s: access '%s' illegal for resource kind %d (pass %u)",
        szCaller, AccessToString(eAccess), (int)xResource.GetKind(), uPassIndex);

    if (xResource.IsImageLike())
    {
        ValidateImageSubresource(xResource, eAccess, uMip, uMipCount, uLayer, uLayerCount, uPassIndex, szCaller);
    }
    else if (xResource.GetKind() == Flux_GraphResourceKind::Buffer)
    {
        ValidateBufferUsage(xResource, uMip, uMipCount, uLayer, uLayerCount, uPassIndex, szCaller);
    }

    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    CheckSubresourceConflicts(pxPass, xResource, eAccess, uMip, uMipCount, uLayer, uLayerCount, bWrite, uPassIndex, szCaller);

    TrackResource(xResource);
    Flux_RenderGraph_ResourceUsage xUsage;
    xUsage.m_xResource = xResource;
    xUsage.m_eAccess = eAccess;
    xUsage.m_uMipLevel = uMip;
    xUsage.m_uMipCount = uMipCount;
    xUsage.m_uLayer = uLayer;
    xUsage.m_uLayerCount = uLayerCount;
    if (bWrite) pxPass->m_xWrites.PushBack(xUsage);
    else pxPass->m_xReads.PushBack(xUsage);
}

void Flux_RenderGraph::Read(Flux_PassHandle xPass, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AssertPassHandleValid(xPass, "Read(Image)");
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, 0, 1, false);
}

void Flux_RenderGraph::Write(Flux_PassHandle xPass, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AssertPassHandleValid(xPass, "Write(Image)");
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, 0, 1, true);
}

void Flux_RenderGraph::Read(Flux_PassHandle xPass, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess,
                            u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount)
{
    AssertPassHandleValid(xPass, "Read(Cube)");
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, uLayer, uLayerCount, false);
}

void Flux_RenderGraph::Write(Flux_PassHandle xPass, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess,
                             u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount)
{
    AssertPassHandleValid(xPass, "Write(Cube)");
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(xImage), eAccess, uMip, uMipCount, uLayer, uLayerCount, true);
}

void Flux_RenderGraph::ReadBuffer(Flux_PassHandle xPass, Flux_Buffer& xBuffer, ResourceAccess eAccess)
{
    AssertPassHandleValid(xPass, "ReadBuffer");
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(xBuffer), eAccess, 0, 1, 0, 1, false);
}

void Flux_RenderGraph::WriteBuffer(Flux_PassHandle xPass, Flux_Buffer& xBuffer, ResourceAccess eAccess)
{
    AssertPassHandleValid(xPass, "WriteBuffer");
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(xBuffer), eAccess, 0, 1, 0, 1, true);
}

void Flux_RenderGraph::DependsOn(Flux_PassHandle xDependentPass, Flux_PassHandle xDependencyPass)
{
    AssertMutable("DependsOn");
    AssertPassHandleValid(xDependentPass, "DependsOn (dependent)");
    AssertPassHandleValid(xDependencyPass, "DependsOn (dependency)");
    Zenith_Assert(xDependentPass != xDependencyPass,
        "Flux_RenderGraph::DependsOn: self-dependency on pass %u", xDependentPass.m_uIndex);
    // Catch accidental duplicate DependsOn calls — harmless but usually indicates
    // a copy-paste or lifetime bug (subsystem re-running SetupRenderGraph without MarkDirty).
    Flux_RenderGraph_Pass* pxDependent = GetPass(xDependentPass.m_uIndex);
    for (Zenith_Vector<u_int>::Iterator it(pxDependent->m_xExplicitDependencies); !it.Done(); it.Next())
    {
        Zenith_Assert(it.GetData() != xDependencyPass.m_uIndex,
            "Flux_RenderGraph::DependsOn: duplicate edge %u -> %u", xDependencyPass.m_uIndex, xDependentPass.m_uIndex);
    }
    pxDependent->m_xExplicitDependencies.PushBack(xDependencyPass.m_uIndex);
}

void Flux_RenderGraph::SetEnabled(Flux_PassHandle xPass, bool bEnabled)
{
    // Deliberately NOT AssertMutable — toggling enabled is allowed post-Compile;
    // the graph re-runs ResolveClearFlags() + SynthesizeBarriers() in Execute()
    // when m_bEnabledMaskDirty.
    AssertPassHandleValid(xPass, "SetEnabled");
    Flux_RenderGraph_Pass* pxPass = GetPass(xPass.m_uIndex);
    if (pxPass->m_bEnabled != bEnabled) { pxPass->m_bEnabled = bEnabled; m_bEnabledMaskDirty = true; }
}

void Flux_RenderGraph::SetAliasingEnabled(bool bEnabled)
{
    // Needs a full recompile — the aliasing phase changes the pool layout and
    // the backing VRAM of every transient, so MarkDirty forces the next
    // Compile to tear down and re-allocate the transient set.
    if (m_bAliasingEnabled == bEnabled) return;
    m_bAliasingEnabled = bEnabled;
    MarkDirty();
}

void Flux_RenderGraph::SetClear(Flux_PassHandle xPass, bool bClearTargets)
{
    AssertMutable("SetClear");
    AssertPassHandleValid(xPass, "SetClear");
    GetPass(xPass.m_uIndex)->m_bRequestsClear = bClearTargets;
}

void Flux_RenderGraph::SetPrepare(Flux_PassHandle xPass, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare)
{
    AssertMutable("SetPrepare");
    AssertPassHandleValid(xPass, "SetPrepare");
    Zenith_Assert(pfnOnPrepare != nullptr,
        "Flux_RenderGraph::SetPrepare: null callback for pass %u", xPass.m_uIndex);
    Zenith_Assert(GetPass(xPass.m_uIndex)->m_pfnOnPrepare == nullptr,
        "Flux_RenderGraph::SetPrepare: prepare callback already set for pass %u (would silently overwrite)", xPass.m_uIndex);
    GetPass(xPass.m_uIndex)->m_pfnOnPrepare = pfnOnPrepare;
}

void Flux_RenderGraph::MarkDirty() { m_bDirty = true; }

void Flux_RenderGraph::Clear()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++) delete m_xPasses.Get(i);
    m_xPasses.Clear();
    m_xResources.clear();
    m_xTraffic.clear();
    m_xAttachmentNeedsClear.Clear();
    m_xAttachmentClearAssigned.Clear();
    m_xEdgeSet.Clear();
    m_xAdjacency.Clear();
    m_xInDegree.Clear();
    m_xExecutionOrder.Clear();
    DestroyTransients();
    m_bCompiled = false;
    m_bDirty = true;
    m_bEnabledMaskDirty = false;
    // Bump the generation so any Flux_PassHandle / Flux_TransientHandle issued
    // by the previous build can't be silently reused to address the new build's
    // passes/transients. AssertPassHandleValid / AssertTransientHandleValid
    // will trip on stale handles. Start at 1 and wrap with saturation-like
    // avoidance of 0 so default-constructed handles (generation=0) always
    // compare unequal to any issued handle.
    m_uGeneration++;
    // Skip 0 on wrap so default-constructed handles are always rejected.
    if (m_uGeneration == 0) m_uGeneration = 1;
}

// ---- Transient resource management ----------------------------------------

Flux_TransientHandle Flux_RenderGraph::CreateTransient(const Flux_TransientTextureDesc& xDesc)
{
    AssertMutable("CreateTransient");
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::CreateTransient: must be called from main thread");
    Zenith_Assert(xDesc.m_uWidth > 0 && xDesc.m_uHeight > 0, "Flux_RenderGraph::CreateTransient: zero-size texture (%ux%u)", xDesc.m_uWidth, xDesc.m_uHeight);
    Zenith_Assert(xDesc.m_uDepth >= 1, "Flux_RenderGraph::CreateTransient: depth must be >= 1 (got %u)", xDesc.m_uDepth);
    Zenith_Assert(xDesc.m_uNumMips >= 1, "Flux_RenderGraph::CreateTransient: mip count must be >= 1 (got %u)", xDesc.m_uNumMips);
    Zenith_Assert(xDesc.m_uNumMips <= FLUX_MAX_MIPS, "Flux_RenderGraph::CreateTransient: mip count %u exceeds FLUX_MAX_MIPS (%u)", xDesc.m_uNumMips, FLUX_MAX_MIPS);
    Zenith_Assert(xDesc.m_eFormat != TEXTURE_FORMAT_NONE, "Flux_RenderGraph::CreateTransient: TEXTURE_FORMAT_NONE not allowed");
    // Depth/stencil flag must agree with the format. Prevents silent mismatches
    // where BuildDepthStencil runs on a colour format or vice versa.
    if (xDesc.m_bIsDepthStencil)
    {
        Zenith_Assert(IsDepthFormat(xDesc.m_eFormat),
            "Flux_RenderGraph::CreateTransient: m_bIsDepthStencil=true but format is not a depth format");
    }
    else
    {
        Zenith_Assert(!IsDepthFormat(xDesc.m_eFormat),
            "Flux_RenderGraph::CreateTransient: colour transient but format is a depth format — set m_bIsDepthStencil=true");
    }
    // 3D depth > 1 only makes sense for TEXTURE_TYPE_3D.
    if (xDesc.m_uDepth > 1)
    {
        Zenith_Assert(xDesc.m_eTextureType == TEXTURE_TYPE_3D,
            "Flux_RenderGraph::CreateTransient: depth=%u requires TEXTURE_TYPE_3D", xDesc.m_uDepth);
    }

    TransientResource* pxTransient = new TransientResource();
    pxTransient->m_xDesc = xDesc;
    pxTransient->m_bAllocated = false;

    Flux_TransientHandle xHandle;
    xHandle.m_uIndex = m_axTransients.GetSize();
    xHandle.m_uGeneration = m_uGeneration;
    xHandle.m_uGraphInstanceID = m_uGraphInstanceID;
    m_axTransients.PushBack(pxTransient);
    return xHandle;
}

void Flux_RenderGraph::ReadTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess)
{
    AssertPassHandleValid(xPass, "ReadTransient");
    AssertTransientHandleValid(xHandle, "ReadTransient");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    if (eAccess == RESOURCE_ACCESS_READ_SRV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
            "Flux_RenderGraph::ReadTransient: READ_SRV requires MEMORY_FLAGS__SHADER_READ in transient desc (pass %u)", xPass.m_uIndex);
    }
    if (eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::ReadTransient: READWRITE_UAV requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)", xPass.m_uIndex);
    }
    Read(xPass, pxT->m_xAttachment, eAccess);
}

void Flux_RenderGraph::WriteTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess)
{
    AssertPassHandleValid(xPass, "WriteTransient");
    AssertTransientHandleValid(xHandle, "WriteTransient");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    if (eAccess == RESOURCE_ACCESS_WRITE_UAV || eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::WriteTransient: '%s' requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)",
            AccessToString(eAccess), xPass.m_uIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_DSV)
    {
        Zenith_Assert(pxT->m_xDesc.m_bIsDepthStencil,
            "Flux_RenderGraph::WriteTransient: WRITE_DSV on a non-depth-stencil transient (pass %u)", xPass.m_uIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_RTV)
    {
        Zenith_Assert(!pxT->m_xDesc.m_bIsDepthStencil,
            "Flux_RenderGraph::WriteTransient: WRITE_RTV on a depth-stencil transient (pass %u) — use WRITE_DSV",
            xPass.m_uIndex);
    }
    Write(xPass, pxT->m_xAttachment, eAccess);
}

void Flux_RenderGraph::ReadTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AssertPassHandleValid(xPass, "ReadTransient (subres)");
    AssertTransientHandleValid(xHandle, "ReadTransient (subres)");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    Zenith_Assert(uMipCount == FLUX_RG_ALL_MIPS || uMip + uMipCount <= pxT->m_xDesc.m_uNumMips,
        "Flux_RenderGraph::ReadTransient: mip range [%u,%u) exceeds transient desc mip count %u",
        uMip, uMip + uMipCount, pxT->m_xDesc.m_uNumMips);
    if (eAccess == RESOURCE_ACCESS_READ_SRV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
            "Flux_RenderGraph::ReadTransient: READ_SRV requires MEMORY_FLAGS__SHADER_READ in transient desc (pass %u)", xPass.m_uIndex);
    }
    // Route through Read() so all common assertions (access/kind compatibility,
    // duplicate declaration, memory-flag checks once allocated) apply uniformly.
    Read(xPass, pxT->m_xAttachment, eAccess, uMip, uMipCount);
}

void Flux_RenderGraph::WriteTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AssertPassHandleValid(xPass, "WriteTransient (subres)");
    AssertTransientHandleValid(xHandle, "WriteTransient (subres)");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    // Mirror ReadTransient's FLUX_RG_ALL_MIPS short-circuit — without it, passing
    // the sentinel overflows uMip + uMipCount and the assert message becomes
    // garbage. Concrete ranges still get the bounds check.
    Zenith_Assert(uMipCount == FLUX_RG_ALL_MIPS || uMip + uMipCount <= pxT->m_xDesc.m_uNumMips,
        "Flux_RenderGraph::WriteTransient: mip range [%u,%u) exceeds transient desc mip count %u",
        uMip, uMip + uMipCount, pxT->m_xDesc.m_uNumMips);
    if (eAccess == RESOURCE_ACCESS_WRITE_UAV || eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::WriteTransient: '%s' requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)",
            AccessToString(eAccess), xPass.m_uIndex);
    }
    Write(xPass, pxT->m_xAttachment, eAccess, uMip, uMipCount);
}

Flux_RenderAttachment& Flux_RenderGraph::GetTransientAttachment(Flux_TransientHandle xHandle)
{
    AssertTransientHandleValid(xHandle, "GetTransientAttachment");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    // If the graph is already compiled, the transient MUST be allocated — any
    // caller that receives a reference with zero surface info is about to deref
    // garbage (views all invalid). Setup-time callers (pre-Compile) get the
    // reference for later use; their VRAM-dependent operations run at execute time.
    if (m_bCompiled)
    {
        Zenith_Assert(pxT->m_bAllocated,
            "Flux_RenderGraph::GetTransientAttachment: handle %u not allocated post-Compile — did you Clear() the graph?", xHandle.m_uIndex);
    }
    return pxT->m_xAttachment;
}

const Flux_RenderAttachment& Flux_RenderGraph::GetTransientAttachment(Flux_TransientHandle xHandle) const
{
    AssertTransientHandleValid(xHandle, "GetTransientAttachment(const)");
    const TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    if (m_bCompiled)
    {
        Zenith_Assert(pxT->m_bAllocated,
            "Flux_RenderGraph::GetTransientAttachment(const): handle %u not allocated post-Compile", xHandle.m_uIndex);
    }
    return pxT->m_xAttachment;
}

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
        xPool.m_xPoolVRAM = Flux_MemoryManager::CreateAliasPoolVRAM(xPool.m_ulTotalSize, xPool.m_ulMaxAlignment);
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
            xInfo.m_uNumLayers   = 1;
            xInfo.m_uMemoryFlags = pxT->m_xDesc.m_uMemoryFlags;

            Flux_VRAMHandle xAliasedVRAM = Flux_MemoryManager::CreateAliasedImageVRAM(
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
        Flux_VRAM* pxPoolVRAM = Flux_PlatformAPI::GetVRAM(xPool.m_xPoolVRAM);
        Flux_MemoryManager::QueueVRAMDeletion(
            pxPoolVRAM, xPool.m_xPoolVRAM,
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            /*uExtraFrameDelay*/ 1);
    }
    m_axAliasPools.Clear();
}

void Flux_RenderGraph::DestroyTransients()
{
    // Destroy per-transient attachments first (includes all their views). For
    // aliased images this queues only the image for destruction; the pool's
    // allocation is freed separately below with an extra frame of delay so
    // the deferred-deletion ring guarantees aliased images are destroyed
    // strictly before the pool's VkDeviceMemory is freed.
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

    // Queue aliasing pool VRAMs for deletion. uExtraFrameDelay=1 pushes the
    // pool's destruction a frame later than any image queued above — required
    // because ProcessDeferredDeletions uses RemoveSwap which doesn't preserve
    // within-frame order, and freeing VkDeviceMemory while an image is still
    // bound to it trips the Vulkan validator.
    for (u_int p = 0; p < m_axAliasPools.GetSize(); p++)
    {
        AliasPool& xPool = m_axAliasPools.Get(p);
        if (!xPool.m_xPoolVRAM.IsValid()) continue;
        Flux_VRAM* pxPoolVRAM = Flux_PlatformAPI::GetVRAM(xPool.m_xPoolVRAM);
        Flux_MemoryManager::QueueVRAMDeletion(
            pxPoolVRAM, xPool.m_xPoolVRAM,
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            /*uExtraFrameDelay*/ 1);
    }
    m_axAliasPools.Clear();
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

    for (auto& xPair : m_xResources)
    {
        xPair.second.m_uFirstWrite = UINT32_MAX;
        xPair.second.m_uLastRead   = UINT32_MAX;
        xPair.second.m_uLastWrite  = UINT32_MAX;
    }

    u_int uTopo = 0;
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next(), ++uTopo)
    {
        u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        if (!pxPass->m_bEnabled) continue;
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xWrite = it.GetData();
            void* pRes = xWrite.m_xResource.GetVoidPtr();
            auto itRes = m_xResources.find(pRes);
            Zenith_Assert(itRes != m_xResources.end(), "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (uTopo < itRes->second.m_uFirstWrite) itRes->second.m_uFirstWrite = uTopo;
            if (itRes->second.m_uLastWrite == UINT32_MAX || uTopo > itRes->second.m_uLastWrite)
                itRes->second.m_uLastWrite = uTopo;
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            auto itRes = m_xResources.find(pRes);
            Zenith_Assert(itRes != m_xResources.end(), "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (itRes->second.m_uLastRead == UINT32_MAX || uTopo > itRes->second.m_uLastRead)
                itRes->second.m_uLastRead = uTopo;
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
    xInfo.m_uNumLayers   = (xDesc.m_eTextureType == TEXTURE_TYPE_CUBE) ? 6u : 1u;
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

    const u_int64 ulLayers = (xDesc.m_eTextureType == TEXTURE_TYPE_CUBE) ? 6u : 1u;
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
    Flux_MemoryManager::ProbeImageMemoryRequirements(xInfo, ulProbedSize, ulProbedAlignment);

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
        if (!pxPass->m_bEnabled) continue;

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
    // Everything else (dimensions, mip count) is NOT in the signature — the
    // packer handles those by computing pool size from per-occupant allocations.
    const u_int64 ulTextureType = static_cast<u_int64>(xDesc.m_eTextureType) & 0xFFull;
    const u_int64 ulDepthFlag   = xDesc.m_bIsDepthStencil ? 1ull : 0ull;
    const u_int64 ulMemFlags    = static_cast<u_int64>(xDesc.m_uMemoryFlags) & 0xFFFFFFFFull;
    const u_int64 ulFormat      = static_cast<u_int64>(xDesc.m_eFormat) & 0xFFFFull;
    return  ulTextureType
         | (ulDepthFlag  << 8)
         | (ulMemFlags   << 9)
         | (ulFormat     << 41);
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
        auto itRes = m_xResources.find(pRes);
        if (itRes == m_xResources.end())
            continue;

        const Flux_RenderGraph_Resource& xRes = itRes->second;
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
        && Flux_MemoryManager::SupportsTransientAliasing();
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
        if (pxPass->m_bIsCompute || !pxPass->m_bEnabled) continue;
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
        if (pxPass->m_bIsCompute || !pxPass->m_bEnabled) continue;
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
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::Compile: must be called from main thread");
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
        if (!pxP->m_bEnabled) continue;
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
    // #TODO: Replace std::unordered_map with engine hash map.
    struct BarrierStateTracker
    {
        std::unordered_map<Flux_BarrierKey, ResourceAccess> m_xImageState;
        std::unordered_map<Flux_Buffer*, ResourceAccess> m_xBufferState;

        ResourceAccess QueryImageState(void* pRes, u_int uMip, u_int uLayer) const
        {
            const Flux_BarrierKey ulKey = MakeBarrierKey(pRes, uMip, uLayer);
            auto it = m_xImageState.find(ulKey);
            return (it == m_xImageState.end()) ? RESOURCE_ACCESS_UNDEFINED : it->second;
        }
        void SetImageState(void* pRes, u_int uMip, u_int uLayer, ResourceAccess e)
        {
            m_xImageState[MakeBarrierKey(pRes, uMip, uLayer)] = e;
        }
        ResourceAccess QueryBufferState(Flux_Buffer* pxBuffer) const
        {
            auto it = m_xBufferState.find(pxBuffer);
            return (it == m_xBufferState.end()) ? RESOURCE_ACCESS_UNDEFINED : it->second;
        }
        void SetBufferState(Flux_Buffer* pxBuffer, ResourceAccess e)
        {
            m_xBufferState[pxBuffer] = e;
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
}

void Flux_RenderGraph::SynthesizeBarriers()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
        m_xPasses.Get(i)->m_xPrologueBarriers.Clear();

    // Barrier synthesis rebuilds state from scratch on every Compile / re-synth,
    // never incrementally. Per-subresource image state and per-buffer state live
    // in the tracker.
    BarrierStateTracker xTracker;

    // Seed buffer state for any buffers that were host-written between this
    // synth and the previous one. The first pass that reads such a buffer
    // gets a TransferWrite→ShaderRead barrier emitted in its prologue —
    // memory availability for the host transfer write that the memory-submit
    // semaphore covers only execution-wise. See MarkBufferHostWritten and the
    // RESOURCE_ACCESS_HOST_TRANSFER_WRITE comment in Flux_Enums.h.
    for (u_int u = 0; u < m_xHostWrittenBuffers.GetSize(); u++)
    {
        Flux_Buffer* pxBuf = m_xHostWrittenBuffers.Get(u);
        if (pxBuf != nullptr) xTracker.SetBufferState(pxBuf, RESOURCE_ACCESS_HOST_TRANSFER_WRITE);
    }
    m_xHostWrittenBuffers.Clear();

    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(itE.GetData());
        if (!pxPass->m_bEnabled) continue;

        // Reads first so READWRITE_UAV (appearing as a Read declaration when the
        // caller used the read-modify-write convention) sets state to the RMW
        // layout before any subsequent write tries to transition again.
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
            ProcessUsageAccess(pxPass, it.GetData(), xTracker);
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
            ProcessUsageAccess(pxPass, it.GetData(), xTracker);
    }
}

void Flux_RenderGraph::MarkBufferHostWritten(const Flux_Buffer& xBuffer)
{
    Flux_Buffer* pxBuf = const_cast<Flux_Buffer*>(&xBuffer);
    // Idempotent within a frame — the same buffer can be marked multiple
    // times safely (e.g. if multiple uploaders touch it before the consumer
    // pass runs).
    bool bAlreadyHostWritten = false;
    for (u_int u = 0; u < m_xHostWrittenBuffers.GetSize(); u++)
    {
        if (m_xHostWrittenBuffers.Get(u) == pxBuf) { bAlreadyHostWritten = true; break; }
    }
    if (!bAlreadyHostWritten) m_xHostWrittenBuffers.PushBack(pxBuf);

    // Persistent record so the orphaned-read validator knows this buffer's
    // writes happen outside the graph and Read declarations are legal even
    // without a graph-side writer. Idempotent — the set stabilises after
    // the first frame in which each buffer is marked.
    for (u_int u = 0; u < m_xExternallyWrittenBuffers.GetSize(); u++)
    {
        if (m_xExternallyWrittenBuffers.Get(u) == pxBuf) return;
    }
    m_xExternallyWrittenBuffers.PushBack(pxBuf);
}

void Flux_RenderGraph::UnmarkBufferHostWritten(const Flux_Buffer& xBuffer)
{
    Flux_Buffer* pxBuf = const_cast<Flux_Buffer*>(&xBuffer);
    for (u_int u = 0; u < m_xExternallyWrittenBuffers.GetSize(); u++)
    {
        if (m_xExternallyWrittenBuffers.Get(u) == pxBuf)
        {
            m_xExternallyWrittenBuffers.RemoveSwap(u);
            break;
        }
    }
    // Also drop any pending per-frame seed for this buffer — uncommon since
    // teardown typically happens between frames, but guards against the
    // teardown-during-Prepare case.
    for (u_int u = 0; u < m_xHostWrittenBuffers.GetSize(); u++)
    {
        if (m_xHostWrittenBuffers.Get(u) == pxBuf)
        {
            m_xHostWrittenBuffers.RemoveSwap(u);
            break;
        }
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