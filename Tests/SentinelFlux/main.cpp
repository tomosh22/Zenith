#include "Zenith.h"

// =============================================================================
// SentinelFlux — leaf-proof executable for the Flux cluster. See
// Build/Sharpmake_SentinelFlux.cs for the full rationale.
//
// This exe links zenithflux.lib + zenithassethandling.lib + ZenithFlux's own
// declared dependency set + the active render backend for this target. Its
// job is to drive Flux with a real, GPU-free, out-of-line call — Flux itself
// has almost no other kind, since nearly every top-level Flux .cpp reaches
// g_xEngine (the composition root, never linked here). Zenith_RenderBus is
// the one that does not.
// =============================================================================

#include "Flux/Zenith_RenderBus.h"

namespace
{
	int s_iFailures = 0;

	void Expect(bool bCondition, const char* szWhat)
	{
		if (!bCondition)
		{
			++s_iFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelFlux FAIL: %s", szWhat);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelFlux ok:   %s", szWhat);
		}
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelFlux: Flux-cluster leaf-proof starting "
		"(zenithflux.lib + zenithassethandling.lib + the active render backend only)");

	// Zenith_RenderBus::RecordDrawCall — a real, out-of-line, GPU-free Flux
	// function (Zenith_RenderBus.cpp references no g_xEngine, no VkXxx, no
	// glfw). Calling it forces zenithflux.lib to link; a stray g_xEngine (or
	// any other engine-composition) reference anywhere in the Flux/
	// AssetHandling/backend closure this TU pulls in would surface as an
	// unresolved external here, never silently.
	Zenith_Maths::Matrix4 xIdentity(1.0f);
	Zenith_RenderBus::RecordDrawCall("SentinelFlux.Probe", xIdentity, 3u, 0u);
	Expect(true, "Zenith_RenderBus::RecordDrawCall resolves from zenithflux.lib with the active backend linked");

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelFlux: PASS (Flux cluster + active backend link with no Editor/EntityComponent/UI reach)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelFlux: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
