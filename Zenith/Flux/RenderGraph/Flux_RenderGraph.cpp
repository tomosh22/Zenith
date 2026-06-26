#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "TaskSystem/Zenith_TaskSystem.h"

#include <cstring> // strcmp — FindPass + force-disable overlay membership tests

// Hard cap on pass count. Chosen to be generous (current engine sits around
// 60 passes); a runaway loop registering passes trips the assert before it
// chews all of heap / the topo-sort adjacency vectors blow up.
static constexpr u_int kMaxPassCount = 256;

// Monotonic graph-instance ID counter. Starts at 1 so 0 stays reserved as the
// "no graph" sentinel on default-constructed handles. Bumped by the
// Flux_RenderGraph constructor for every new instance.
u_int Flux_RenderGraph::s_uNextGraphInstanceID = 1;


// ---- Access-mode classification helpers --------------------------------
// Centralised so assertions across AddResourceUsage, Validate, bind-time
// checks all agree on what a given ResourceAccess means. Any new access mode
// added to Flux_Enums.h must be classified here or the default branch trips.
//
// Note: IsReadAccess and IsWriteAccess are intentionally kept as separate
// fully-enumerated switches rather than collapsed into a single bitmask
// table. The compiler-warning-via-missing-case path forces every new
// ResourceAccess value to be explicitly classified in BOTH directions —
// trying to share a table or bitmask would lose that enforcement.
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
    // Layer bound = the resource's actual layer count: 6 for cubes, N for a 2D
    // array attachment (e.g. CSM cascades), 1 for an ordinary 2D image.
    const u_int uMaxLayers = (rxInfo.m_uNumLayers > 0) ? rxInfo.m_uNumLayers : 1u;
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
    if (!m_xResources.Contains(pRes))
    {
        Flux_RenderGraph_Resource xRes;
        xRes.m_xResource = xResource;
        xRes.m_uFirstWrite = UINT32_MAX;
        xRes.m_uLastRead   = UINT32_MAX;
        xRes.m_uLastWrite  = UINT32_MAX;
        m_xResources.Insert(pRes, xRes);
    }
}

Flux_PassBuilder Flux_RenderGraph::AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord, void* pUserData)
{
    AssertMutable("AddPass");
    Zenith_Assert(szName != nullptr, "Flux_RenderGraph::AddPass: null name");
    Zenith_Assert(szName[0] != '\0', "Flux_RenderGraph::AddPass: empty name — all passes must have a non-empty debug label");
    Zenith_Assert(pfnOnRecord != nullptr, "Flux_RenderGraph::AddPass: null record callback for pass '%s'", szName);
    Zenith_Assert(m_xPasses.GetSize() < kMaxPassCount, "Flux_RenderGraph::AddPass: exceeded hard pass cap (%u) when adding '%s'", kMaxPassCount, szName);
    Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Flux_RenderGraph::AddPass: must be called from main thread ('%s')", szName);
#ifdef ZENITH_RUNTIME_CHECKS
    // Duplicate-name detection: two passes sharing a name makes the ImGui panel
    // and assertion messages ambiguous, and usually indicates a copy-paste bug in
    // a subsystem's SetupRenderGraph. Enforced in all RUNTIME_CHECKS builds (which
    // includes shipping _False) because FindPass is now public and a public,
    // shipping-facing name lookup must not be able to resolve an ambiguous name.
    // Compares the always-on m_szName via strcmp (m_strName is tools-only).
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Zenith_Assert(m_xPasses.Get(i)->m_szName == nullptr || strcmp(m_xPasses.Get(i)->m_szName, szName) != 0,
            "Flux_RenderGraph::AddPass: duplicate pass name '%s' (already used by pass %u)", szName, i);
    }
#endif
    Flux_RenderGraph_Pass* pxPass = new Flux_RenderGraph_Pass();
    // Always-on identity (present in every config). m_szName is the AddPass label;
    // m_szOwner is the current setup-step owner so the effective-enabled overlay
    // can force-disable by owner. Both are static-lifetime string pointers.
    pxPass->m_szName = szName;
    pxPass->m_szOwner = m_szCurrentSetupOwner;
#ifdef ZENITH_TOOLS
    pxPass->m_strName = szName;
#endif
    pxPass->m_pfnOnRecord = pfnOnRecord;
    pxPass->m_pUserData = pUserData;
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

Flux_PassBuilder&& Flux_PassBuilder::ReadsTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount) &&
{
    AssertAlive("ReadsTransient(layer)");
    m_pxGraph->ReadTransient(m_xPass, xHandle, eAccess, uMip, uMipCount, uLayer, uLayerCount);
    return std::move(*this);
}

Flux_PassBuilder&& Flux_PassBuilder::WritesTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount) &&
{
    AssertAlive("WritesTransient(layer)");
    m_pxGraph->WriteTransient(m_xPass, xHandle, eAccess, uMip, uMipCount, uLayer, uLayerCount);
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

Flux_PassHandle Flux_RenderGraph::FindPass(const char* szName) const
{
    Zenith_Assert(szName != nullptr, "Flux_RenderGraph::FindPass: null name");
    Flux_PassHandle xFound; // invalid by default
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        const Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(i);
        if (pxPass->m_szName != nullptr && strcmp(pxPass->m_szName, szName) == 0)
        {
            // Duplicate-name guard: a public, shipping-facing lookup must never
            // resolve an ambiguous name to a usable handle. AddPass already
            // Zenith_Checks duplicates under ZENITH_RUNTIME_CHECKS, but defend
            // here too — return invalid on the second hit.
            Zenith_Check(!xFound.IsValid(),
                "Flux_RenderGraph::FindPass: duplicate pass name '%s' — ambiguous lookup, returning invalid handle", szName);
            if (xFound.IsValid()) return Flux_PassHandle();
            xFound = MakePassHandle(i);
        }
    }
    return xFound;
}

bool Flux_RenderGraph::IsPassEffectivelyEnabled(const Flux_RenderGraph_Pass* pxPass) const
{
    // base bit AND not force-disabled by owner AND not force-disabled by name.
    // When both override vectors are empty this is exactly m_bEnabled.
    return pxPass->m_bEnabled
        && !(pxPass->m_szOwner != nullptr && IsOwnerForceDisabled(pxPass->m_szOwner))
        && !(pxPass->m_szName  != nullptr && IsPassForceDisabled(pxPass->m_szName));
}

// --- Force-disable overlay -------------------------------------------------
// Membership helper: index of szValue in xVec by strcmp, or UINT32_MAX on miss.
static u_int FindStringIndex(const Zenith_Vector<const char*>& xVec, const char* szValue)
{
    for (u_int i = 0; i < xVec.GetSize(); i++)
    {
        if (xVec.Get(i) != nullptr && strcmp(xVec.Get(i), szValue) == 0) return i;
    }
    return UINT32_MAX;
}

void Flux_RenderGraph::SetOwnerForceDisabled(const char* szOwner, bool bForceDisabled)
{
    Zenith_Assert(szOwner != nullptr, "Flux_RenderGraph::SetOwnerForceDisabled: null owner");
    const u_int uExisting = FindStringIndex(m_axForceDisabledOwners, szOwner);
    const bool bAlready = (uExisting != UINT32_MAX);
    if (bForceDisabled == bAlready) return; // no change
    if (bForceDisabled) m_axForceDisabledOwners.PushBack(szOwner);
    else                m_axForceDisabledOwners.Remove(uExisting);
    // Order changed → the topological sort (which reads the effective predicate)
    // must rebuild so force-disabled passes drop out of / return to the order.
    MarkDirty();
}

void Flux_RenderGraph::SetPassForceDisabled(const char* szPassName, bool bForceDisabled)
{
    Zenith_Assert(szPassName != nullptr, "Flux_RenderGraph::SetPassForceDisabled: null pass name");
    const u_int uExisting = FindStringIndex(m_axForceDisabledPassNames, szPassName);
    const bool bAlready = (uExisting != UINT32_MAX);
    if (bForceDisabled == bAlready) return; // no change
    if (bForceDisabled) m_axForceDisabledPassNames.PushBack(szPassName);
    else                m_axForceDisabledPassNames.Remove(uExisting);
    MarkDirty();
}

bool Flux_RenderGraph::IsOwnerForceDisabled(const char* szOwner) const
{
    return FindStringIndex(m_axForceDisabledOwners, szOwner) != UINT32_MAX;
}

bool Flux_RenderGraph::IsPassForceDisabled(const char* szPassName) const
{
    return FindStringIndex(m_axForceDisabledPassNames, szPassName) != UINT32_MAX;
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

std::string Flux_RenderGraph::GetPassOrderDescription() const
{
    std::string strResult;
    strResult.reserve(m_xExecutionOrder.GetSize() * 32);
    for (u_int i = 0; i < m_xExecutionOrder.GetSize(); ++i)
    {
        const u_int uPassIndex = m_xExecutionOrder.Get(i);
        const Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIndex);
        if (i > 0)
        {
            strResult += " -> ";
        }
        strResult += pxPass->DebugName();
        if (!IsPassEffectivelyEnabled(pxPass))
        {
            strResult += " (disabled)";
        }
    }
    return strResult;
}

void Flux_RenderGraph::Clear()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++) delete m_xPasses.Get(i);
    m_xPasses.Clear();
    m_xResources.Clear();
    m_xTraffic.Clear();
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
    // Reset the transient per-walk owner tag, but DELIBERATELY do NOT clear the
    // force-disable overlay vectors (m_axForceDisabledOwners /
    // m_axForceDisabledPassNames): a game-level override must survive
    // Clear()/rebuild so it doesn't have to be reasserted on every recompile. A
    // game clears its own override explicitly on feature shutdown.
    m_szCurrentSetupOwner = nullptr;
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
    Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Flux_RenderGraph::CreateTransient: must be called from main thread");
    Zenith_Assert(xDesc.m_uWidth > 0 && xDesc.m_uHeight > 0, "Flux_RenderGraph::CreateTransient: zero-size texture (%ux%u)", xDesc.m_uWidth, xDesc.m_uHeight);
    Zenith_Assert(xDesc.m_uDepth >= 1, "Flux_RenderGraph::CreateTransient: depth must be >= 1 (got %u)", xDesc.m_uDepth);
    Zenith_Assert(xDesc.m_uNumMips >= 1, "Flux_RenderGraph::CreateTransient: mip count must be >= 1 (got %u)", xDesc.m_uNumMips);
    Zenith_Assert(xDesc.m_uNumMips <= FLUX_MAX_MIPS, "Flux_RenderGraph::CreateTransient: mip count %u exceeds FLUX_MAX_MIPS (%u)", xDesc.m_uNumMips, FLUX_MAX_MIPS);
    Zenith_Assert(xDesc.m_uNumLayers >= 1, "Flux_RenderGraph::CreateTransient: layer count must be >= 1 (got %u)", xDesc.m_uNumLayers);
    Zenith_Assert(xDesc.m_uNumLayers <= FLUX_MAX_ATTACHMENT_LAYERS, "Flux_RenderGraph::CreateTransient: layer count %u exceeds FLUX_MAX_ATTACHMENT_LAYERS (%u)", xDesc.m_uNumLayers, FLUX_MAX_ATTACHMENT_LAYERS);
    // Array transients (m_uNumLayers > 1) are only wired for the 2D depth-stencil
    // path today (the CSM cascade array). Colour/cube/3D array transients would
    // need per-layer RTV/UAV plumbing in BuildColour — add it when a use case lands.
    if (xDesc.m_uNumLayers > 1)
    {
        Zenith_Assert(xDesc.m_bIsDepthStencil && xDesc.m_eTextureType == TEXTURE_TYPE_2D,
            "Flux_RenderGraph::CreateTransient: m_uNumLayers > 1 is only supported for 2D depth-stencil transients (got type %d, depth=%d)",
            (int)xDesc.m_eTextureType, xDesc.m_bIsDepthStencil ? 1 : 0);
    }
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

// Layer-aware transient subresource declarations. Route through AddResourceUsage
// directly (NOT Read/Write(Flux_RenderAttachment), which force layer 0) so an
// array transient can declare a single-layer access (e.g. one CSM cascade).
void Flux_RenderGraph::ReadTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount)
{
    AssertPassHandleValid(xPass, "ReadTransient (layer)");
    AssertTransientHandleValid(xHandle, "ReadTransient (layer)");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    Zenith_Assert(uMipCount == FLUX_RG_ALL_MIPS || uMip + uMipCount <= pxT->m_xDesc.m_uNumMips,
        "Flux_RenderGraph::ReadTransient: mip range [%u,%u) exceeds transient desc mip count %u",
        uMip, uMip + uMipCount, pxT->m_xDesc.m_uNumMips);
    Zenith_Assert(uLayerCount == FLUX_RG_ALL_LAYERS || uLayer + uLayerCount <= pxT->m_xDesc.m_uNumLayers,
        "Flux_RenderGraph::ReadTransient: layer range [%u,%u) exceeds transient desc layer count %u",
        uLayer, uLayer + uLayerCount, pxT->m_xDesc.m_uNumLayers);
    if (eAccess == RESOURCE_ACCESS_READ_SRV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
            "Flux_RenderGraph::ReadTransient: READ_SRV requires MEMORY_FLAGS__SHADER_READ in transient desc (pass %u)", xPass.m_uIndex);
    }
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(pxT->m_xAttachment), eAccess, uMip, uMipCount, uLayer, uLayerCount, false);
}

void Flux_RenderGraph::WriteTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount)
{
    AssertPassHandleValid(xPass, "WriteTransient (layer)");
    AssertTransientHandleValid(xHandle, "WriteTransient (layer)");
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    Zenith_Assert(uMipCount == FLUX_RG_ALL_MIPS || uMip + uMipCount <= pxT->m_xDesc.m_uNumMips,
        "Flux_RenderGraph::WriteTransient: mip range [%u,%u) exceeds transient desc mip count %u",
        uMip, uMip + uMipCount, pxT->m_xDesc.m_uNumMips);
    Zenith_Assert(uLayerCount == FLUX_RG_ALL_LAYERS || uLayer + uLayerCount <= pxT->m_xDesc.m_uNumLayers,
        "Flux_RenderGraph::WriteTransient: layer range [%u,%u) exceeds transient desc layer count %u",
        uLayer, uLayer + uLayerCount, pxT->m_xDesc.m_uNumLayers);
    if (eAccess == RESOURCE_ACCESS_WRITE_DSV)
    {
        Zenith_Assert(pxT->m_xDesc.m_bIsDepthStencil,
            "Flux_RenderGraph::WriteTransient: WRITE_DSV on a non-depth-stencil transient (pass %u)", xPass.m_uIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_UAV || eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::WriteTransient: '%s' requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)",
            AccessToString(eAccess), xPass.m_uIndex);
    }
    AddResourceUsage(xPass.m_uIndex, Flux_GraphResource(pxT->m_xAttachment), eAccess, uMip, uMipCount, uLayer, uLayerCount, true);
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
        g_xEngine.FluxMemory().QueueVRAMDeletion(
            xPool.m_xPoolVRAM,
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            Flux_ImageViewHandle(), Flux_ImageViewHandle(),
            /*uExtraFrameDelay*/ 1);
    }
    m_axAliasPools.Clear();
}
