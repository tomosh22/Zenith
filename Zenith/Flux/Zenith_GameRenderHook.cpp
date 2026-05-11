#include "Zenith.h"

#include "Flux/Zenith_GameRenderHook.h"
#include "Collections/Zenith_Vector.h"

namespace Zenith_GameRenderHook
{
	// Static registry — game-thread only. SetupRenderGraph is called on the
	// main thread (Flux.cpp:223 / RequestGraphRebuild on main between frames),
	// and game-side Register/Unregister calls happen during Project_Init or
	// behaviour OnAwake (also main-thread), so no synchronization is needed.
	static Zenith_Vector<PostFogPassRegistrationFn> s_axRegistrations;

	void RegisterPostFogPass(PostFogPassRegistrationFn pfn)
	{
		if (pfn == nullptr) return;
		// Idempotent — silently skip duplicates so Editor Stop/Play and
		// graph-rebuild loops don't accumulate stale entries.
		for (uint32_t i = 0; i < s_axRegistrations.GetSize(); ++i)
		{
			if (s_axRegistrations.Get(i) == pfn) return;
		}
		s_axRegistrations.PushBack(pfn);
	}

	void UnregisterPostFogPass(PostFogPassRegistrationFn pfn)
	{
		if (pfn == nullptr) return;
		for (uint32_t i = 0; i < s_axRegistrations.GetSize(); ++i)
		{
			if (s_axRegistrations.Get(i) == pfn)
			{
				s_axRegistrations.RemoveSwap(i);
				return;
			}
		}
	}

	void ResetAllRegistrations()
	{
		s_axRegistrations.Clear();
	}

	void InvokePostFogRegistrations(Flux_RenderGraph& xGraph)
	{
		for (uint32_t i = 0; i < s_axRegistrations.GetSize(); ++i)
		{
			PostFogPassRegistrationFn pfn = s_axRegistrations.Get(i);
			if (pfn != nullptr)
			{
				pfn(xGraph);
			}
		}
	}
}
