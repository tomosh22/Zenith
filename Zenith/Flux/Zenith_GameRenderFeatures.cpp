#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Zenith_GameRenderFeatures.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_FeatureRegistry.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>

namespace Zenith_GameRenderFeatures
{
	struct RegisteredFeature
	{
		Zenith_GameRenderFeatureDesc m_xDesc;
		bool m_bInitialised = false;
	};

	// Static registry — main-thread only. Register/Unregister happen during
	// Project init or behaviour OnAwake; InvokeFeaturesAnchoredAfter /
	// InitialiseAllPending / ShutdownAll run on the main thread inside the Flux
	// setup / lifecycle walk. No synchronization needed. Persists for the whole
	// process (mirrors the old hook's static vector).
	static Zenith_Vector<RegisteredFeature> s_axFeatures;

	static u_int FindIndexByName(const char* szName)
	{
		for (u_int i = 0; i < s_axFeatures.GetSize(); i++)
		{
			const char* szExisting = s_axFeatures.Get(i).m_xDesc.m_szName;
			if (szExisting != nullptr && strcmp(szExisting, szName) == 0) return i;
		}
		return UINT32_MAX;
	}

	static bool RunAfterMatches(const char* a, const char* b)
	{
		if (a == nullptr || b == nullptr) return a == b;
		return strcmp(a, b) == 0;
	}

	static bool DescsMatch(const Zenith_GameRenderFeatureDesc& a, const Zenith_GameRenderFeatureDesc& b)
	{
		return a.m_pfnInitialise == b.m_pfnInitialise
			&& a.m_pfnSetupRenderGraph == b.m_pfnSetupRenderGraph
			&& a.m_pfnShutdown == b.m_pfnShutdown
			&& RunAfterMatches(a.m_szRunAfter, b.m_szRunAfter);
	}

	void Register(const Zenith_GameRenderFeatureDesc& xDesc)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Zenith_GameRenderFeatures::Register: main-thread only");
		Zenith_Assert(xDesc.m_szName != nullptr && xDesc.m_szName[0] != '\0',
			"Zenith_GameRenderFeatures::Register: feature needs a non-empty name");
		Zenith_Assert(xDesc.m_pfnSetupRenderGraph != nullptr,
			"Zenith_GameRenderFeatures::Register: feature '%s' needs a SetupRenderGraph callback", xDesc.m_szName);

		const u_int uExisting = FindIndexByName(xDesc.m_szName);
		if (uExisting != UINT32_MAX)
		{
			// Idempotent by name: identical desc = no-op; different = error (no
			// silent replacement, so Editor Stop/Play and rebuild loops can't
			// accumulate or swap registrations behind the game's back).
			Zenith_Check(DescsMatch(s_axFeatures.Get(uExisting).m_xDesc, xDesc),
				"Zenith_GameRenderFeatures::Register: feature '%s' already registered with different callbacks/runAfter — Unregister before re-registering", xDesc.m_szName);
			return;
		}

		RegisteredFeature xFeat;
		xFeat.m_xDesc = xDesc;
		xFeat.m_bInitialised = false;
		s_axFeatures.PushBack(xFeat);

		// Late-registration path (graph already valid — the common case for games
		// that register during Project_RegisterGameComponents): initialise now
		// and request a rebuild so this feature's setup runs before the next frame.
		// Otherwise InitialiseAllPending() (from LateInitialise) runs it later.
		if (g_xEngine.FluxRenderer().IsRenderGraphValid())
		{
			if (xDesc.m_pfnInitialise != nullptr) xDesc.m_pfnInitialise();
			s_axFeatures.Get(s_axFeatures.GetSize() - 1).m_bInitialised = true;
			g_xEngine.FluxRenderer().RequestGraphRebuild();
		}
	}

	void Unregister(const char* szName)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Zenith_GameRenderFeatures::Unregister: main-thread only");
		if (szName == nullptr) return;
		const u_int uIndex = FindIndexByName(szName);
		if (uIndex == UINT32_MAX) return;

		RegisteredFeature& xFeat = s_axFeatures.Get(uIndex);
		if (xFeat.m_bInitialised && xFeat.m_xDesc.m_pfnShutdown != nullptr)
			xFeat.m_xDesc.m_pfnShutdown();

		// Order-preserving erase (NOT RemoveSwap): features anchored after the same
		// engine step rely on registration order for their relative declaration order.
		s_axFeatures.Remove(uIndex);

		if (g_xEngine.FluxRenderer().IsRenderGraphValid())
			g_xEngine.FluxRenderer().RequestGraphRebuild();
	}

	void ResetAll()
	{
		// Shutdown initialised features in reverse registration order (they may own
		// GPU resources), then drop everything.
		for (u_int i = s_axFeatures.GetSize(); i-- > 0; )
		{
			RegisteredFeature& xFeat = s_axFeatures.Get(i);
			if (xFeat.m_bInitialised && xFeat.m_xDesc.m_pfnShutdown != nullptr)
				xFeat.m_xDesc.m_pfnShutdown();
		}
		s_axFeatures.Clear();
	}

	void InvokeFeaturesAnchoredAfter(const char* szStepName, Flux_RenderGraph& xGraph)
	{
		if (szStepName == nullptr) return;
		for (u_int i = 0; i < s_axFeatures.GetSize(); i++)
		{
			RegisteredFeature& xFeat = s_axFeatures.Get(i);
			if (!RunAfterMatches(xFeat.m_xDesc.m_szRunAfter, szStepName)) continue;
			// Owner-tag this feature's passes (so the force-disable overlay can
			// target the whole feature by name), run its setup, then clear the tag.
			xGraph.SetCurrentSetupOwner(xFeat.m_xDesc.m_szName);
			xFeat.m_xDesc.m_pfnSetupRenderGraph(xGraph);
			xGraph.SetCurrentSetupOwner(nullptr);
		}
	}

	void InitialiseAllPending()
	{
		for (u_int i = 0; i < s_axFeatures.GetSize(); i++)
		{
			RegisteredFeature& xFeat = s_axFeatures.Get(i);
			if (xFeat.m_bInitialised) continue;
			if (xFeat.m_xDesc.m_pfnInitialise != nullptr) xFeat.m_xDesc.m_pfnInitialise();
			xFeat.m_bInitialised = true;
		}
	}

	void ShutdownAll()
	{
		// Reverse registration order, mirroring the engine feature shutdown walk.
		// Keeps the registrations (ResetAll is the explicit drop path); a feature
		// that re-inits in-process is brought back by InitialiseAllPending.
		for (u_int i = s_axFeatures.GetSize(); i-- > 0; )
		{
			RegisteredFeature& xFeat = s_axFeatures.Get(i);
			if (xFeat.m_bInitialised && xFeat.m_xDesc.m_pfnShutdown != nullptr)
				xFeat.m_xDesc.m_pfnShutdown();
			xFeat.m_bInitialised = false;
		}
	}

#ifdef ZENITH_RUNTIME_CHECKS
	void VerifyGameFeatureAnchors()
	{
		for (u_int i = 0; i < s_axFeatures.GetSize(); i++)
		{
			const Zenith_GameRenderFeatureDesc& xDesc = s_axFeatures.Get(i).m_xDesc;
			if (xDesc.m_szRunAfter == nullptr) continue;
			Zenith_Check(Flux_FeatureRegistry::Get().HasSetupStepNamed(xDesc.m_szRunAfter),
				"Zenith_GameRenderFeatures::VerifyGameFeatureAnchors: feature '%s' anchors runAfter='%s', which is not a registered engine setup step",
				xDesc.m_szName, xDesc.m_szRunAfter);
		}
	}
#endif
}
