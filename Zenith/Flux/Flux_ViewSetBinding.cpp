#include "Zenith.h"
#include "Flux/Flux_ViewSetBinding.h"
#include "Core/Zenith_Engine.h"               // g_xEngine.Shadows()
#include "Flux/Shadows/Flux_ShadowsImpl.h"    // GetCSMArraySRV (Phase 5.4 GRAPH_RESOURCE row)
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"  // GetClusterLight*SRV (Phase 5.4 GRAPH_RESOURCE rows)
#include "Flux/Flux_PersistentSetLayouts.h"   // VIEW-set binding indices

#include <cstring>   // strcmp

// The canonical VIEW/GLOBAL spine-member registry. Today the persistent GLOBAL
// (set 0) holds g_xGlobal (frame constants) + g_axMaterials (material-table
// SSBO), and VIEW (set 1) holds g_xView (camera constants) — all frame-indexed
// CPU buffers (CPU_GLOBAL_BUFFER), which never need a graph Read(). Phase 5.4
// appends one GRAPH_RESOURCE row per resource it moves into the persistent VIEW
// set (CSM array, G-buffer, depth, HiZ, SSR/SSGI/SSAO, light clusters), each
// wiring m_pfnGetVRAMHandle to the owning subsystem's current-frame SRV accessor
// and m_pfnIsEnabled to its feature toggle. Built once (thread-safe magic static).
const Zenith_Vector<Flux_ViewSetBinding>& Flux_GetViewSetBindingRegistry()
{
	static const Zenith_Vector<Flux_ViewSetBinding> s_xRegistry = []()
	{
		Zenith_Vector<Flux_ViewSetBinding> xReg;
		xReg.PushBack({ "g_xGlobal",     0u, FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, nullptr, nullptr });
		xReg.PushBack({ "g_axMaterials", 0u, FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, nullptr, nullptr });
		xReg.PushBack({ "g_xView",       1u, FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, nullptr, nullptr });
		// Phase 5.4: g_xCSM promoted into the persistent VIEW set. Graph-tracked (a
		// transient depth array), so any pass that samples it must declare a Read() —
		// the validator enforces that now the per-pass BindSRV(hg_xCSM) is gone. The
		// accessor returns the CSM array's current-frame VRAM handle, which matches the
		// handle the consumers' ReadTransient(GetCSMArrayHandle()) resolves to. Always
		// enabled (the array is always allocated; cleared-to-far when shadows are off).
		xReg.PushBack({ "g_xCSM", Flux_PersistentSetLayouts::kuSetView, FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE,
			[]() -> Flux_VRAMHandle { return g_xEngine.Shadows().GetCSMArraySRV().m_xVRAMHandle; },
			nullptr });
		// Phase 5.4: g_xShadowMatrices (all-cascade sun view×proj SSBO) promoted into the
		// persistent VIEW set. It is a frame-indexed Flux_DynamicReadWriteBuffer — graph-
		// INVISIBLE by contract (host-coherent; visibility via the submit barrier), exactly
		// like g_xView / g_axMaterials — so it is CPU_GLOBAL_BUFFER (no graph Read() demanded).
		// Not flagged per-camera: although the cascade fit uses the main camera's frustum,
		// the only secondary view (MaterialPreview) disables shadows — same view-sharing
		// rationale as g_xCSM above (see kbFluxMultiViewSupported).
		xReg.PushBack({ "g_xShadowMatrices", Flux_PersistentSetLayouts::kuSetView,
			FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, nullptr, nullptr });
		// Phase 5.4: clustered-lighting read buffers. g_xLightBuffer is a frame-indexed
		// Flux_DynamicReadWriteBuffer (graph-invisible) → CPU_GLOBAL_BUFFER (exempt). The
		// cluster count/index buffers are GPU-only Flux_ReadWriteBuffers, written by the
		// LightClustering compute (UAV) and read by the consumers (SRV) → GRAPH_RESOURCE: the
		// consumers' ReadBuffer decls (guarded by IsInitialised) drive the UAV→SRV barrier and
		// the validator enforces them; IsEnabled mirrors that guard. Like g_xCSM / g_xShadowMatrices,
		// the cluster grid is MAIN-camera-derived (ComputeClusterAABB uses g_xView), but it is
		// shareable across views — not flagged per-camera — because the only secondary view
		// (MaterialPreview) disables dynamic lights, so it never meaningfully samples it.
		xReg.PushBack({ "g_xLightBuffer", Flux_PersistentSetLayouts::kuSetView,
			FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, nullptr, nullptr });
		xReg.PushBack({ "g_xClusterLightCounts", Flux_PersistentSetLayouts::kuSetView, FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE,
			[]() -> Flux_VRAMHandle { return g_xEngine.LightClustering().GetClusterLightCountsSRV().m_xVRAMHandle; },
			[]() -> bool { return g_xEngine.LightClustering().IsInitialised(); } });
		xReg.PushBack({ "g_xClusterLightIndices", Flux_PersistentSetLayouts::kuSetView, FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE,
			[]() -> Flux_VRAMHandle { return g_xEngine.LightClustering().GetClusterLightIndicesSRV().m_xVRAMHandle; },
			[]() -> bool { return g_xEngine.LightClustering().IsInitialised(); } });

		// Per-camera view-sharing guard (W2): with multi-view (Phase 5.6) unsupported the
		// VIEW set is shared by every view, so a per-camera resource here would alias the
		// main camera for secondary views (MaterialPreview). Fail loud at build-of-registry
		// rather than render silently wrong. No per-camera VIEW row exists today (CSM is
		// view-invariant); this guards future promotions.
		const char* szOffender = nullptr;
		Zenith_Assert(Flux_ViewSetRegistryRespectsViewSharing(xReg.GetDataPointer(), xReg.GetSize(),
			kbFluxMultiViewSupported, &szOffender),
			"Flux_ViewSetBinding: per-camera member '%s' promoted into the SHARED persistent VIEW set while "
			"multi-view (Phase 5.6) is unsupported — keep it per-pass (set 3) or implement 5.6.",
			szOffender ? szOffender : "?");
		return xReg;
	}();
	return s_xRegistry;
}

const Flux_ViewSetBinding* Flux_FindViewSetBinding(const char* szMemberName)
{
	if (!szMemberName) return nullptr;
	const Zenith_Vector<Flux_ViewSetBinding>& xReg = Flux_GetViewSetBindingRegistry();
	for (u_int u = 0; u < xReg.GetSize(); u++)
	{
		const Flux_ViewSetBinding& xRow = xReg.Get(u);
		if (xRow.m_szMemberName && strcmp(xRow.m_szMemberName, szMemberName) == 0)
		{
			return &xRow;
		}
	}
	return nullptr;
}

#include "Flux/Flux_ViewSetBinding.Tests.inl"
