#pragma once

#include "Core/ZenithConfig.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Flux_Types.h"                  // Flux_VRAMHandle (complete, for the accessor trampoline)
#include "Flux/Flux_PersistentSetLayouts.h"  // kuSetView (per-camera view-sharing guard)

// =====================================================================
// Phase 5.5 — VIEW/GLOBAL-set graph-Read() validator (pure core + registry).
//
// Background. Sets 0 (GLOBAL) and 1 (VIEW) are PERSISTENT: their descriptors are
// written once per frame in PreparePersistentSets, not per-pass. The render
// graph's per-bind safety net (Flux_RenderGraph::AssertBoundResourceDeclared)
// only fires on a per-pass BindSRV, so once a graph-tracked resource (G-buffer,
// depth, CSM array, HiZ, SSR/SSGI/SSAO, light clusters) moves into the VIEW set
// (Phase 5.4) and is no longer bound per pass, a pass could SAMPLE it without
// declaring the graph Read() that drives barrier synthesis — a silent missing-
// barrier bug. This validator replaces the lost enforcement: for each pass it
// asks "which VIEW/GLOBAL members does the bound shader STATICALLY sample, and
// for the graph-tracked ones, did the pass declare a Read()?".
//
// The DECISION is a pure function (unit-tested, device-free). The backend glue
// (in Zenith_Vulkan_CommandBuffer) resolves the inputs from the bound pipeline's
// reflection + the registry + the current recording pass, then calls it.
// =====================================================================

// Source of a VIEW/GLOBAL member — ORTHOGONAL to the persistence axis
// (FluxFrequencyClass). Determines whether sampling the member obliges a Read().
enum Flux_ViewSetSourceClass : u_int
{
	FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE,      // graph-tracked → REQUIRES a declared Read()
	FLUX_VIEWSET_SOURCE_EXTERNAL_PERSISTENT, // disk / long-lived, untracked (IBL cubes, BRDF LUT) → exempt
	FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER,   // frame-indexed CPU buffer (g_xGlobal/g_xView/g_axMaterials) → exempt
	FLUX_VIEWSET_SOURCE_FALLBACK_DISABLED,   // feature off, a dummy is bound → exempt
};

// One sampled VIEW/GLOBAL member, fully resolved for a single pass. The pure
// validator decides demand/exempt purely from these already-resolved facts.
struct Flux_ViewSetSampledMember
{
	const char*             m_szMemberName      = nullptr; // for the diagnostic message
	Flux_ViewSetSourceClass m_eSource           = FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER;
	bool                    m_bEnabled          = true;    // false ⇒ feature off ⇒ exempt
	bool                    m_bGraphHandleValid = false;   // GRAPH_RESOURCE: the accessor yielded a valid handle this frame
	bool                    m_bReadDeclared     = false;   // the pass declared a matching graph Read()
};

struct Flux_ViewSetReadCheck
{
	bool        m_bAllDeclared    = true;
	const char* m_szMissingMember = nullptr;   // first GRAPH_RESOURCE sampled without a Read()
};

// A GRAPH_RESOURCE member that is enabled, produced this frame (valid handle),
// and statically sampled MUST be declared as a graph Read(). Every other source
// class, every disabled feature, and any handle not produced this frame are
// exempt. Reports the first violation. Pure; device-free; unit-tested.
inline Flux_ViewSetReadCheck Flux_ValidateViewSetReads(const Flux_ViewSetSampledMember* axMembers, u_int uCount)
{
	Flux_ViewSetReadCheck xResult;
	for (u_int u = 0; u < uCount; u++)
	{
		const Flux_ViewSetSampledMember& xM = axMembers[u];
		if (xM.m_eSource != FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE) continue; // only graph resources need a Read()
		if (!xM.m_bEnabled)          continue; // disabled feature binds a placeholder — no real dependency
		if (!xM.m_bGraphHandleValid) continue; // not produced this frame (mirrors the bind-hook invalid-handle early-out)
		if (xM.m_bReadDeclared)      continue; // declared correctly
		xResult.m_bAllDeclared    = false;
		xResult.m_szMissingMember = xM.m_szMemberName;
		return xResult;
	}
	return xResult;
}

// Registry row: maps a VIEW/GLOBAL spine member (by reflection name) to its
// source class and — for GRAPH_RESOURCE — the live accessor that yields its VRAM
// handle THIS frame plus an optional feature-enabled predicate. Captureless
// trampolines (no std::function), matching the Flux_FeatureDesc / registry idiom.
struct Flux_ViewSetBinding
{
	const char*             m_szMemberName        = nullptr;
	u_int                   m_uSet                = 1;                                  // 0=GLOBAL, 1=VIEW
	Flux_ViewSetSourceClass m_eSource             = FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER;
	Flux_VRAMHandle       (*m_pfnGetVRAMHandle)() = nullptr;                            // GRAPH_RESOURCE only; live handle this frame
	bool                  (*m_pfnIsEnabled)()     = nullptr;                            // null ⇒ always enabled
	bool                    m_bPerCamera          = false;                              // contents depend on the active camera (depth/HiZ/G-buffer/SSR) → unsafe in the SHARED VIEW set until multi-view (Phase 5.6)
};

// Multi-view exists at the CAMERA level: each render view (Flux/RenderViews/
// Flux_RenderViews.h) owns a VIEW descriptor-set instance whose binding 0
// (g_xView) is per-view. Bindings 1-8, however, are REPLICATED-SHARED — the
// same descriptor is fanned out to every view's set (WritePersistentViewImage/
// Buffer), so their CONTENTS are still single-copy. A PER-CAMERA resource — one
// whose contents depend on the active camera (scene depth, HiZ, G-buffer,
// SSR/SSGI/SSAO) — MUST NOT be promoted into VIEW until a per-view WRITE path
// for bindings 1+ exists, or a secondary view silently reads the main camera's
// data and no validator catches it. View-invariant resources (CSM, IBL, shadow
// matrices, light clusters) are safe to share; secondary views gate shadow/
// cluster SAMPLING off via their per-view flags. Flip this if/when per-view
// resource writes land.
inline constexpr bool kbFluxPerViewSharedResourcesSupported = false;

// Pure guard: false (naming the offender) if any VIEW (set 1) row is per-camera while
// multi-view is unsupported. Device-free; unit-tested; asserted once at registry build,
// so a future per-camera promotion fails loud instead of silently aliasing the camera.
inline bool Flux_ViewSetRegistryRespectsViewSharing(const Flux_ViewSetBinding* axRows, u_int uCount,
	bool bMultiViewSupported, const char** pszOffenderOut = nullptr)
{
	if (bMultiViewSupported) { return true; }
	for (u_int u = 0; u < uCount; u++)
	{
		if (axRows[u].m_uSet == Flux_PersistentSetLayouts::kuSetView && axRows[u].m_bPerCamera)
		{
			if (pszOffenderOut) { *pszOffenderOut = axRows[u].m_szMemberName; }
			return false;
		}
	}
	return true;
}

// The canonical VIEW/GLOBAL member registry, built once. Currently only the
// always-exempt CPU global buffers; Phase 5.4 adds one GRAPH_RESOURCE row per
// member it promotes into the persistent VIEW set, in the same change that
// deletes that member's per-pass BindSRV (so the draw-time validator takes over
// enforcement with no gap).
const Zenith_Vector<Flux_ViewSetBinding>& Flux_GetViewSetBindingRegistry();

// Look up a registry row by member name; nullptr if the member is not registered
// (an unregistered set-0/1 member is treated as exempt by the caller).
const Flux_ViewSetBinding* Flux_FindViewSetBinding(const char* szMemberName);
