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
            return true;
        case RESOURCE_ACCESS_UNDEFINED:
        case RESOURCE_ACCESS_WRITE_RTV:
        case RESOURCE_ACCESS_WRITE_DSV:
        case RESOURCE_ACCESS_WRITE_UAV:
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
            return true;
        case RESOURCE_ACCESS_UNDEFINED:
        case RESOURCE_ACCESS_READ_SRV:
        case RESOURCE_ACCESS_READ_DEPTH:
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
            // Image-like resources support every texture access mode.
            return eAccess != RESOURCE_ACCESS_UNDEFINED;
        case Flux_GraphResourceKind::Buffer:
            // Buffers can't be render targets or depth-stencil attachments; they
            // support SRV (structured read), UAV (compute write), and read-modify-write UAV.
            return eAccess == RESOURCE_ACCESS_READ_SRV
                || eAccess == RESOURCE_ACCESS_WRITE_UAV
                || eAccess == RESOURCE_ACCESS_READWRITE_UAV;
    }
    return false;
}

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
    return uIdx;
}

void Flux_RenderGraph::AddResourceUsage(u_int uPassIndex, const Flux_GraphResource& xResource, ResourceAccess eAccess,
                                        u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount, bool bWrite)
{
    const char* szCaller = bWrite ? "Write" : "Read";
    AssertMutable(szCaller);
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(),
        "Flux_RenderGraph::%s: invalid pass index %u (have %u)", szCaller, uPassIndex, m_xPasses.GetSize());
    Zenith_Assert(uMipCount > 0, "Flux_RenderGraph::%s: mip count must be > 0", szCaller);
    Zenith_Assert(uLayerCount > 0, "Flux_RenderGraph::%s: layer count must be > 0", szCaller);
    Zenith_Assert(xResource.IsValid(), "Flux_RenderGraph::%s: resource is null", szCaller);

    // Read() must only be called with a read access mode, Write() with a write
    // access mode. READWRITE_UAV is both; the caller chose Read or Write based
    // on their primary intent (writers drive clear/first-writer logic; readers
    // drive barriers). Accept READWRITE_UAV for either, but the *other* modes
    // must agree with the caller.
    // Zenith_Assert expands to `if(!(x)){...}` so an unbraced else in the
    // outer if-else would bind to the wrong branch — always brace each side.
    if (eAccess != RESOURCE_ACCESS_READWRITE_UAV)
    {
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

    Zenith_Assert(IsAccessLegalForKind(eAccess, xResource.GetKind()),
        "Flux_RenderGraph::%s: access '%s' illegal for resource kind %d (pass %u)",
        szCaller, AccessToString(eAccess), (int)xResource.GetKind(), uPassIndex);

    // For image-like resources with known dimensions, assert the declared
    // subresource range fits within the attachment. Transient attachments have
    // zero surface info until AllocateTransients runs inside Compile() — in
    // that case the bounds check is deferred to Validate() after allocation.
    // Width is the sentinel for "built": Flux_SurfaceInfo::m_uNumMips defaults
    // to 1 so can't be used to tell "unset" from "1 mip".
    if (xResource.IsImageLike())
    {
        const Flux_SurfaceInfo& rxInfo = xResource.GetSurfaceInfo();
        const bool bInfoKnown = rxInfo.m_uWidth > 0;
        if (bInfoKnown)
        {
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
        }

        // Depth-specific access modes must target a depth format. Catches a common
        // bug where WRITE_DSV is used on a colour attachment (or vice versa).
        if ((eAccess == RESOURCE_ACCESS_WRITE_DSV || eAccess == RESOURCE_ACCESS_READ_DEPTH) && bInfoKnown)
        {
            Zenith_Assert(IsDepthFormat(rxInfo.m_eFormat),
                "Flux_RenderGraph::%s: depth access '%s' on non-depth format (pass %u)",
                szCaller, AccessToString(eAccess), uPassIndex);
        }
        if (eAccess == RESOURCE_ACCESS_WRITE_RTV && bInfoKnown)
        {
            Zenith_Assert(!IsDepthFormat(rxInfo.m_eFormat),
                "Flux_RenderGraph::%s: colour write 'WRITE_RTV' on depth-format attachment (pass %u) — use WRITE_DSV",
                szCaller, uPassIndex);
        }

        // Memory-flag ↔ access compatibility (only when the attachment has been
        // built — transient checks are deferred to Compile()).
        if (bInfoKnown)
        {
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
    }
    else if (xResource.GetKind() == Flux_GraphResourceKind::Buffer)
    {
        Zenith_Assert(uMip == 0 && uMipCount == 1, "Flux_RenderGraph::%s: buffers have no mip levels (pass %u)", szCaller, uPassIndex);
        Zenith_Assert(uLayer == 0 && uLayerCount == 1, "Flux_RenderGraph::%s: buffers have no array layers (pass %u)", szCaller, uPassIndex);
        const Flux_Buffer* pxBuf = xResource.AsBuffer();
        Zenith_Assert(pxBuf->m_ulSize > 0, "Flux_RenderGraph::%s: buffer has zero size (pass %u)", szCaller, uPassIndex);
    }

    // Duplicate-declaration detection on the same pass. Writing the same mip/layer
    // twice with the same access is redundant; writing it with conflicting access
    // (e.g. WRITE_RTV + WRITE_UAV) is an actual bug — we can't serve both in one
    // pass. Scan existing usages with overlapping subresource ranges.
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
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
    // Also catch a Read that overlaps a Write (or vice-versa) unless one is
    // READWRITE_UAV on a single UAV — which is the legitimate read-modify-write
    // pattern for compute.
    const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxOther = bWrite ? pxPass->m_xReads : pxPass->m_xWrites;
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxOther); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_ResourceUsage& rxPrev = it.GetData();
        if (rxPrev.m_xResource != xResource) continue;
        if (!SubresourceRangesOverlap(rxPrev.m_uMipLevel, rxPrev.m_uMipCount, rxPrev.m_uLayer, rxPrev.m_uLayerCount,
                                      uMip, uMipCount, uLayer, uLayerCount)) continue;
        const bool bIsRMW = (eAccess == RESOURCE_ACCESS_READWRITE_UAV) || (rxPrev.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV);
        Zenith_Assert(bIsRMW,
            "Flux_RenderGraph: pass %u '%s' declares overlapping read and write on resource '%s' "
            "(prev '%s' mips[%u,%u) layers[%u,%u), new '%s' mips[%u,%u) layers[%u,%u)) — "
            "use RESOURCE_ACCESS_READWRITE_UAV for read-modify-write, or narrow subresource ranges",
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
    Zenith_Assert(uDependentPass < m_xPasses.GetSize(),
        "Flux_RenderGraph::DependsOn: dependent pass %u out of range (have %u)", uDependentPass, m_xPasses.GetSize());
    Zenith_Assert(uDependencyPass < m_xPasses.GetSize(),
        "Flux_RenderGraph::DependsOn: dependency pass %u out of range (have %u)", uDependencyPass, m_xPasses.GetSize());
    Zenith_Assert(uDependentPass != uDependencyPass,
        "Flux_RenderGraph::DependsOn: self-dependency on pass %u", uDependentPass);
    // Catch accidental duplicate DependsOn calls — harmless but usually indicates
    // a copy-paste or lifetime bug (subsystem re-running SetupRenderGraph without MarkDirty).
    Flux_RenderGraph_Pass* pxDependent = GetPass(uDependentPass);
    for (Zenith_Vector<u_int>::Iterator it(pxDependent->m_xExplicitDependencies); !it.Done(); it.Next())
    {
        Zenith_Assert(it.GetData() != uDependencyPass,
            "Flux_RenderGraph::DependsOn: duplicate edge %u -> %u", uDependencyPass, uDependentPass);
    }
    pxDependent->m_xExplicitDependencies.PushBack(uDependencyPass);
}

void Flux_RenderGraph::SetEnabled(u_int uPassIndex, bool bEnabled)
{
    // Deliberately NOT AssertMutable — toggling enabled is allowed post-Compile;
    // the graph re-runs ResolveClearFlags() in Execute() when m_bEnabledMaskDirty.
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(),
        "Flux_RenderGraph::SetEnabled: invalid pass index %u (have %u)", uPassIndex, m_xPasses.GetSize());
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    if (pxPass->m_bEnabled != bEnabled) { pxPass->m_bEnabled = bEnabled; m_bEnabledMaskDirty = true; }
}

void Flux_RenderGraph::SetClear(u_int uPassIndex, bool bClearTargets)
{
    AssertMutable("SetClear");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(),
        "Flux_RenderGraph::SetClear: invalid pass index %u (have %u)", uPassIndex, m_xPasses.GetSize());
    GetPass(uPassIndex)->m_bRequestsClear = bClearTargets;
}

void Flux_RenderGraph::SetPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare)
{
    AssertMutable("SetPrepare");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(),
        "Flux_RenderGraph::SetPrepare: invalid pass index %u (have %u)", uPassIndex, m_xPasses.GetSize());
    Zenith_Assert(pfnOnPrepare != nullptr,
        "Flux_RenderGraph::SetPrepare: null callback for pass %u", uPassIndex);
    Zenith_Assert(GetPass(uPassIndex)->m_pfnOnPrepare == nullptr,
        "Flux_RenderGraph::SetPrepare: prepare callback already set for pass %u (would silently overwrite)", uPassIndex);
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
    m_axTransients.PushBack(pxTransient);
    return xHandle;
}

void Flux_RenderGraph::ReadTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess)
{
    Zenith_Assert(xHandle.IsValid(), "Flux_RenderGraph::ReadTransient: invalid handle (uninitialised)");
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::ReadTransient: handle %u out of range (have %u transients)",
        xHandle.m_uIndex, m_axTransients.GetSize());
    // Stamp the transient's declared-read access footprint onto the desc so Validate
    // can cross-check memory flags post-allocation.
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    if (eAccess == RESOURCE_ACCESS_READ_SRV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
            "Flux_RenderGraph::ReadTransient: READ_SRV requires MEMORY_FLAGS__SHADER_READ in transient desc (pass %u)", uPassIndex);
    }
    if (eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::ReadTransient: READWRITE_UAV requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)", uPassIndex);
    }
    Read(uPassIndex, pxT->m_xAttachment, eAccess);
}

void Flux_RenderGraph::WriteTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess)
{
    Zenith_Assert(xHandle.IsValid(), "Flux_RenderGraph::WriteTransient: invalid handle (uninitialised)");
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::WriteTransient: handle %u out of range (have %u transients)",
        xHandle.m_uIndex, m_axTransients.GetSize());
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    if (eAccess == RESOURCE_ACCESS_WRITE_UAV || eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::WriteTransient: '%s' requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)",
            AccessToString(eAccess), uPassIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_DSV)
    {
        Zenith_Assert(pxT->m_xDesc.m_bIsDepthStencil,
            "Flux_RenderGraph::WriteTransient: WRITE_DSV on a non-depth-stencil transient (pass %u)", uPassIndex);
    }
    if (eAccess == RESOURCE_ACCESS_WRITE_RTV)
    {
        Zenith_Assert(!pxT->m_xDesc.m_bIsDepthStencil,
            "Flux_RenderGraph::WriteTransient: WRITE_RTV on a depth-stencil transient (pass %u) — use WRITE_DSV",
            uPassIndex);
    }
    Write(uPassIndex, pxT->m_xAttachment, eAccess);
}

void Flux_RenderGraph::ReadTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    Zenith_Assert(xHandle.IsValid(), "Flux_RenderGraph::ReadTransient (subres): invalid handle");
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::ReadTransient (subres): handle %u out of range (have %u)", xHandle.m_uIndex, m_axTransients.GetSize());
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    Zenith_Assert(uMipCount == FLUX_RG_ALL_MIPS || uMip + uMipCount <= pxT->m_xDesc.m_uNumMips,
        "Flux_RenderGraph::ReadTransient: mip range [%u,%u) exceeds transient desc mip count %u",
        uMip, uMip + uMipCount, pxT->m_xDesc.m_uNumMips);
    if (eAccess == RESOURCE_ACCESS_READ_SRV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__SHADER_READ),
            "Flux_RenderGraph::ReadTransient: READ_SRV requires MEMORY_FLAGS__SHADER_READ in transient desc (pass %u)", uPassIndex);
    }
    // Route through Read() so all common assertions (access/kind compatibility,
    // duplicate declaration, memory-flag checks once allocated) apply uniformly.
    Read(uPassIndex, pxT->m_xAttachment, eAccess, uMip, uMipCount);
}

void Flux_RenderGraph::WriteTransient(u_int uPassIndex, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    Zenith_Assert(xHandle.IsValid(), "Flux_RenderGraph::WriteTransient (subres): invalid handle");
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::WriteTransient (subres): handle %u out of range (have %u)", xHandle.m_uIndex, m_axTransients.GetSize());
    TransientResource* pxT = m_axTransients.Get(xHandle.m_uIndex);
    Zenith_Assert(uMip + uMipCount <= pxT->m_xDesc.m_uNumMips,
        "Flux_RenderGraph::WriteTransient: mip range [%u,%u) exceeds transient desc mip count %u",
        uMip, uMip + uMipCount, pxT->m_xDesc.m_uNumMips);
    if (eAccess == RESOURCE_ACCESS_WRITE_UAV || eAccess == RESOURCE_ACCESS_READWRITE_UAV)
    {
        Zenith_Assert(pxT->m_xDesc.m_uMemoryFlags & (1u << MEMORY_FLAGS__UNORDERED_ACCESS),
            "Flux_RenderGraph::WriteTransient: '%s' requires MEMORY_FLAGS__UNORDERED_ACCESS in transient desc (pass %u)",
            AccessToString(eAccess), uPassIndex);
    }
    Write(uPassIndex, pxT->m_xAttachment, eAccess, uMip, uMipCount);
}

Flux_RenderAttachment& Flux_RenderGraph::GetTransientAttachment(Flux_TransientHandle xHandle)
{
    Zenith_Assert(xHandle.IsValid(),
        "Flux_RenderGraph::GetTransientAttachment: invalid handle (uninitialised)");
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::GetTransientAttachment: invalid handle %u (size %u)", xHandle.m_uIndex, m_axTransients.GetSize());
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
    Zenith_Assert(xHandle.IsValid(),
        "Flux_RenderGraph::GetTransientAttachment(const): invalid handle (uninitialised)");
    Zenith_Assert(xHandle.m_uIndex < m_axTransients.GetSize(),
        "Flux_RenderGraph::GetTransientAttachment(const): invalid handle %u (size %u)", xHandle.m_uIndex, m_axTransients.GetSize());
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
    // -- Orphaned reads: any resource read but never written ------------------
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

    // -- Transients must appear in at least one Read/Write (in any pass, enabled
    // or not). Scans raw pass read/write lists, not m_xTraffic, because traffic
    // is built only from enabled passes — a technique that is currently disabled
    // (e.g. a fog variant) still owns its transients, which is legal. What's
    // NOT legal is declaring a transient that no pass references at all.
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

    // -- Per-pass invariants --------------------------------------------------
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

        // A pass can either be graphics (has RTV/DSV writes) or compute (no
        // attachment writes). Graphics passes with at least one RTV must declare
        // at most FLUX_MAX_TARGETS — InferPassAttachments silently drops extras,
        // which is a silent bug. Check the write list up front.
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
        // A pass mixing attachment writes (RTV/DSV) with UAV writes is legal in
        // principle (graphics pipeline can write UAVs from fragment stage) but
        // unusual; flag to force an explicit conscious choice. If this pattern
        // becomes routine, relax to a debug warning.
        Zenith_Assert(!(uRTVCount + uDSVCount > 0 && uUAVWriteCount > 0),
            "Flux_RenderGraph: pass '%s' mixes attachment writes (%u RTV + %u DSV) with UAV writes (%u). Split into graphics + compute passes.",
            pxP->DebugName(), uRTVCount, uDSVCount, uUAVWriteCount);

        // Post-allocation memory-flag/access compatibility. Runs after AllocateTransients
        // so transient attachments have valid surface info by now.
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
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::Compile: must be called from main thread");
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
        case RESOURCE_ACCESS_READ_SRV:        return RESOURCE_ACCESS_READ_SRV;        // SHADER_READ_ONLY
        case RESOURCE_ACCESS_READ_DEPTH:      return RESOURCE_ACCESS_READ_DEPTH;      // DEPTH_READ_ONLY
        case RESOURCE_ACCESS_WRITE_RTV:       return RESOURCE_ACCESS_WRITE_RTV;       // COLOR_ATTACHMENT
        case RESOURCE_ACCESS_WRITE_DSV:       return RESOURCE_ACCESS_WRITE_DSV;       // DEPTH_STENCIL_ATTACHMENT
        case RESOURCE_ACCESS_WRITE_UAV:       return RESOURCE_ACCESS_WRITE_UAV;       // GENERAL
        case RESOURCE_ACCESS_READWRITE_UAV:   return RESOURCE_ACCESS_READWRITE_UAV;   // GENERAL
        case RESOURCE_ACCESS_UNDEFINED:       return RESOURCE_ACCESS_UNDEFINED;
    }
    Zenith_Assert(false, "RequiredPrePassAccess: unknown access %d", (int)eAccess);
    return RESOURCE_ACCESS_UNDEFINED;
}

// Layout the resource is in AFTER this pass executes. With the render pass's
// finalLayout matching its initialLayout (no auto-transition), the resource
// stays in the working layout of its access.
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
        || e == RESOURCE_ACCESS_READWRITE_UAV;
}

void Flux_RenderGraph::SynthesizeBarriers()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
        m_xPasses.Get(i)->m_xPrologueBarriers.Clear();

    // Per-subresource state map keyed on MakeBarrierKey(ptr, mip, layer).
    // #TODO: Replace std::unordered_map with engine hash map (Phase G).
    std::unordered_map<u_int64, ResourceAccess> xState;

    auto QueryState = [&](void* pRes, u_int uMip, u_int uLayer) -> ResourceAccess
    {
        const u_int64 ulKey = MakeBarrierKey(pRes, uMip, uLayer);
        auto it = xState.find(ulKey);
        return (it == xState.end()) ? RESOURCE_ACCESS_UNDEFINED : it->second;
    };
    auto SetState = [&](void* pRes, u_int uMip, u_int uLayer, ResourceAccess e)
    {
        xState[MakeBarrierKey(pRes, uMip, uLayer)] = e;
    };

    auto ResolveSubresourceRange = [](const Flux_RenderGraph_ResourceUsage& rxUsage,
                                      u_int& uOutBaseMip, u_int& uOutMipCount,
                                      u_int& uOutBaseLayer, u_int& uOutLayerCount) -> bool
    {
        if (!rxUsage.m_xResource.IsImageLike()) return false;
        const Flux_SurfaceInfo& rxInfo = rxUsage.m_xResource.GetSurfaceInfo();
        uOutBaseMip = rxUsage.m_uMipLevel;
        uOutMipCount = (rxUsage.m_uMipCount == FLUX_RG_ALL_MIPS) ? rxInfo.m_uNumMips : rxUsage.m_uMipCount;
        uOutBaseLayer = rxUsage.m_uLayer;
        uOutLayerCount = (rxUsage.m_uLayerCount == FLUX_RG_ALL_LAYERS) ? rxInfo.m_uNumLayers : rxUsage.m_uLayerCount;
        return true;
    };

    // Process one declared (read or write) access on a subresource range.
    // Emits a prologue barrier per (mip, layer) where current ≠ required OR
    // either side is a write (WAW / RAW within matching layout still needs
    // a memory barrier). Updates the per-subresource tracker.
    auto ProcessAccess = [&](Flux_RenderGraph_Pass* pxPass,
                             const Flux_RenderGraph_ResourceUsage& rxUsage)
    {
        u_int uBaseMip, uMipCount, uBaseLayer, uLayerCount;
        if (!ResolveSubresourceRange(rxUsage, uBaseMip, uMipCount, uBaseLayer, uLayerCount))
            return; // buffer — barriers elided for now (Phase B is image-only)

        // Reject the resource if the underlying VRAM isn't allocated yet
        // (shouldn't happen post-AllocateTransients but guards against
        // future ordering bugs).
        if (!rxUsage.m_xResource.IsImageLike()) return;
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
                const ResourceAccess eSrc = QueryState(pRes, uMip, uLayer);
                const bool bLayoutMatches = SameLayout(eSrc, eRequired);
                const bool bSrcIsWrite = AccessIsWrite(eSrc);
                // Need a barrier if the layout is changing OR either side is
                // a write (so WAW/RAW still gets a memory barrier even when
                // layout doesn't change — e.g. two consecutive RTV writes).
                if (!bLayoutMatches || bWriteAccess || bSrcIsWrite)
                {
                    Flux_RenderGraph_Barrier xBarrier;
                    xBarrier.m_xResource = rxUsage.m_xResource;
                    xBarrier.m_uBaseMip = uMip;
                    xBarrier.m_uMipCount = 1;
                    xBarrier.m_uBaseLayer = uLayer;
                    xBarrier.m_uLayerCount = 1;
                    xBarrier.m_eSrcAccess = bLayoutMatches ? eRequired : eSrc;
                    xBarrier.m_eDstAccess = eRequired;
                    pxPass->m_xPrologueBarriers.PushBack(xBarrier);
                }
                SetState(pRes, uMip, uLayer, ePost);
            }
        }
    };

    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(itE.GetData());
        if (!pxPass->m_bEnabled) continue;

        // Reads first so READWRITE_UAV (which appears as a Read declaration when
        // the caller used the read-modify-write convention) sets state to the
        // RMW layout before any subsequent write tries to transition again.
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
            ProcessAccess(pxPass, it.GetData());
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
            ProcessAccess(pxPass, it.GetData());
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

struct Flux_RenderGraph_RecordTaskData { Flux_RenderGraph* m_pxGraph; u_int m_uPassIndex; };

// ---- Current-pass / current-graph thread-local recording context -------
// Set around each pfnOnRecord callback so Flux_ShaderBinder can cross-reference
// bound VRAM handles against the current pass's declared Read/Write sets.
// Defined here so Flux_RenderGraph_RecordPassTask (just below) can use them;
// the public entry point Flux_RenderGraph::AssertBoundResourceDeclared is
// defined later in the file.
static thread_local const Flux_RenderGraph_Pass* tls_pxCurrentRecordingPass = nullptr;
static thread_local const Flux_RenderGraph* tls_pxCurrentRecordingGraph = nullptr;

namespace
{
    class CurrentGraphScope
    {
    public:
        CurrentGraphScope(const Flux_RenderGraph* pxGraph) : m_pxPrev(tls_pxCurrentRecordingGraph)
        {
            tls_pxCurrentRecordingGraph = pxGraph;
        }
        ~CurrentGraphScope()
        {
            tls_pxCurrentRecordingGraph = m_pxPrev;
        }
    private:
        const Flux_RenderGraph* m_pxPrev;
    };
}

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

    // Set the thread-local current-pass and current-graph pointers so
    // Flux_ShaderBinder bind-time assertions can cross-reference against this
    // pass's declared Read/Write sets and skip non-tracked static assets.
    // Scopes restore prior values on exit even if the callback throws.
    Flux_RenderGraph::CurrentPassScope xPassScope(&xPass);
    CurrentGraphScope xGraphScope(pxGraph);

    // Prologue barriers from SynthesizeBarriers are NOT injected into the
    // command list — that path runs INSIDE the render pass for graphics
    // passes (vkCmdBeginRenderPass already issued by the backend before
    // IterateCommands), where vkCmdPipelineBarrier needs subpass self-deps.
    // The backend reads xPass.m_xPrologueBarriers directly and emits them
    // right before TransitionTargetsForRenderPass / Dispatch entry, outside
    // any active render pass. See Zenith_Vulkan.cpp::RecordCommandBuffersTask.
    xPass.m_pfnOnRecord(xPass.m_pxCommandList, xPass.m_pUserData);
}

void Flux_RenderGraph::Execute()
{
    Zenith_Assert(m_bCompiled, "Flux_RenderGraph::Execute: must call Compile() first");
    Zenith_Assert(!m_bDirty, "Flux_RenderGraph::Execute: graph is dirty — AddPass/Read/Write was called after Compile(). Call Compile() again.");
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::Execute: must be called from main thread");
    if (m_xExecutionOrder.GetSize() == 0) return;

    // Every enabled pass in the execution order must have a valid command list —
    // a null list means the pass was destroyed without being removed from the
    // exec order, which corrupts downstream parallel recording.
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_Pass* pxP = m_xPasses.Get(it.GetData());
        Zenith_Assert(pxP != nullptr,
            "Flux_RenderGraph::Execute: null pass at execution order index %u", it.GetData());
        if (!pxP->m_bEnabled) continue;
        Zenith_Assert(pxP->m_pxCommandList != nullptr,
            "Flux_RenderGraph::Execute: enabled pass '%s' has null command list", pxP->DebugName());
        Zenith_Assert(pxP->m_pfnOnRecord != nullptr,
            "Flux_RenderGraph::Execute: enabled pass '%s' has null record callback", pxP->DebugName());
    }

    if (m_bEnabledMaskDirty)
    {
        // SetEnabled flipped at least one pass's enable bit since last Execute.
        // The cheap path (no full recompile) re-resolves clear ownership AND
        // resynthesises barriers — both depend on which passes are actually
        // running this frame. Without the barrier re-run, a pass that was
        // enabled last compile but disabled now leaves the graph's compile-
        // time state tracker out of sync with the actual GPU layout, and
        // downstream consumers' barriers transition from a layout the resource
        // is no longer in (sync-validator layout-mismatch error).
        ResolveClearFlags();
        SynthesizeBarriers();
        m_bEnabledMaskDirty = false;
    }
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
        // A pass with zero recorded commands is normally pruned, but if it has
        // graph-synthesised prologue barriers (e.g. the "Final RT Layout
        // Transition" no-op pass that exists purely to flip the swapchain
        // source target into SHADER_READ_ONLY) we MUST submit it so the
        // backend gets a chance to emit those barriers. Same goes for clear-only
        // passes that need their target zeroed.
        const bool bHasBarriers = pxPass->m_xPrologueBarriers.GetSize() > 0;
        if (pxPass->m_pxCommandList->GetCommandCount() == 0 && !pxPass->m_bClearTargets && !bHasBarriers) continue;
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

// ---- Current-pass thread-local recording context ------------------------
// tls_pxCurrentRecordingPass / tls_pxCurrentRecordingGraph and CurrentGraphScope
// are defined near Flux_RenderGraph_RecordPassTask earlier in the file.

const Flux_RenderGraph_Pass* Flux_RenderGraph::GetCurrentRecordingPass()
{
    return tls_pxCurrentRecordingPass;
}

Flux_RenderGraph::CurrentPassScope::CurrentPassScope(const Flux_RenderGraph_Pass* pxPass)
    : m_pxPrev(tls_pxCurrentRecordingPass)
{
    tls_pxCurrentRecordingPass = pxPass;
}

Flux_RenderGraph::CurrentPassScope::~CurrentPassScope()
{
    tls_pxCurrentRecordingPass = m_pxPrev;
}

// Walk every resource the graph knows about (both declared attachments/buffers
// via m_xResources, and transient attachments) and check whether xVRAMHandle
// matches one. Externally-managed static assets (e.g. skybox cubemap loaded
// from disk, BRDF LUT preserved across frames) are NOT tracked — binding one
// of those without a Read/Write is legal because its layout is fixed.
static bool IsGraphTrackedVRAMHandle(const Flux_RenderGraph* pxGraph, Flux_VRAMHandle xVRAMHandle)
{
    if (pxGraph == nullptr) return false;
    for (auto& xPair : pxGraph->GetResources())
    {
        const Flux_RenderGraph_Resource& rxRes = xPair.second;
        if (rxRes.m_xResource.IsImageLike())
        {
            if (rxRes.m_xResource.GetVRAMHandle() == xVRAMHandle) return true;
        }
        else if (rxRes.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
        {
            if (rxRes.m_xResource.AsBuffer()->m_xVRAMHandle == xVRAMHandle) return true;
        }
    }
    return false;
}

void Flux_RenderGraph::AssertBoundResourceDeclared(Flux_VRAMHandle xVRAMHandle, bool bIsWrite, const char* szBindCall)
{
    const Flux_RenderGraph_Pass* pxPass = tls_pxCurrentRecordingPass;
    if (pxPass == nullptr)
    {
        // Outside a render-graph recording window — e.g. Initialise-time binding
        // path or unit test. Legal.
        return;
    }
    if (!xVRAMHandle.IsValid())
    {
        // Null/invalid VRAM handle cannot be cross-referenced — likely the caller
        // already asserted on a higher level (or bound a placeholder for a
        // disabled feature). Skip silently; the Vulkan validator will catch
        // actual unbound-descriptor usage.
        return;
    }

    // Scan the pass's declared reads and writes for a resource whose image-like
    // VRAM handle matches the bound view's VRAM handle. Buffers compare by the
    // buffer pointer's VRAM handle too (Flux_Buffer::m_xVRAMHandle).
    auto ScanUsages = [&](const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, bool bExpectReads) -> bool
    {
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxUsages); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
            Flux_VRAMHandle xDeclHandle;
            if (rxUsage.m_xResource.IsImageLike())
                xDeclHandle = rxUsage.m_xResource.GetVRAMHandle();
            else if (rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
                xDeclHandle = rxUsage.m_xResource.AsBuffer()->m_xVRAMHandle;
            else
                continue;

            if (!(xDeclHandle == xVRAMHandle)) continue;
            // Match — enforce that the access direction is compatible with the bind call.
            if (bIsWrite)
            {
                if (bExpectReads)
                {
                    // Read-list entry; only READWRITE_UAV on the read side is meaningful,
                    // because the graph tracks it as a reader too.
                    if (rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV) return true;
                    continue;
                }
                return true;
            }
            else
            {
                // Read binding (SRV). Must be declared as a Read with SRV-compatible access
                // OR as READWRITE_UAV (which implies both read and write).
                if (!bExpectReads)
                {
                    // Write-list entry — only READWRITE_UAV qualifies (read-modify-write).
                    if (rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV) return true;
                    continue;
                }
                if (rxUsage.m_eAccess == RESOURCE_ACCESS_READ_SRV
                 || rxUsage.m_eAccess == RESOURCE_ACCESS_READ_DEPTH
                 || rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV)
                    return true;
                continue;
            }
        }
        return false;
    };

    const bool bFoundInReads  = ScanUsages(pxPass->m_xReads,  true);
    const bool bFoundInWrites = ScanUsages(pxPass->m_xWrites, false);
    if (bFoundInReads || bFoundInWrites)
        return;

    // Not in this pass's declarations. If the resource ISN'T tracked by the
    // graph at all (e.g. a disk-loaded skybox texture, a BRDF LUT baked at
    // init, a frame constants buffer used by every pass), it's an external
    // static asset — binding is legal, no barrier needed. Only trip the
    // assertion when the resource IS graph-tracked but this pass didn't declare
    // it — that's the missed-dependency bug we want to catch.
    if (!IsGraphTrackedVRAMHandle(tls_pxCurrentRecordingGraph, xVRAMHandle))
        return;

    Zenith_Assert(false,
        "Flux_ShaderBinder::%s: pass '%s' binding graph-tracked VRAM handle %u without declaring it as a %s. "
        "Add the missing Read()/Write() in SetupRenderGraph or the graph cannot emit correct barriers.",
        szBindCall, pxPass->DebugName(), xVRAMHandle.AsUInt(), bIsWrite ? "Write" : "Read");
}