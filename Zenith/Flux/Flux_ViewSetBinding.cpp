#include "Zenith.h"
#include "Flux/Flux_ViewSetBinding.h"
#include "Core/Zenith_Engine.h"               // g_xEngine.Shadows()
#include "Flux/Shadows/Flux_ShadowsImpl.h"    // GetCSMArraySRV (Phase 5.4 GRAPH_RESOURCE row)
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
