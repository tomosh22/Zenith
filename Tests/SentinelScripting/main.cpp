#include "Zenith.h"

// =============================================================================
// SentinelScripting — leaf-proof executable for ZenithScripting. See
// Build/Sharpmake_SentinelScripting.cs for the full rationale.
//
// This exe links ONLY zenithscripting.lib + zenithecs.lib + zenithdatastream.lib
// + zenithmaths.lib + zenithcore.lib. Its job is to drive the Scripting core
// (a Zenith_GraphBlackboard round-trip) plus the one direct ECS edge
// Zenith_BehaviourGraph.cpp itself declares (bringing up the SceneSystem),
// without ever naming g_xEngine, Flux, Physics, AI or a concrete component.
// =============================================================================

#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"

namespace
{
	int s_iFailures = 0;

	void Expect(bool bCondition, const char* szWhat)
	{
		if (!bCondition)
		{
			++s_iFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelScripting FAIL: %s", szWhat);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelScripting ok:   %s", szWhat);
		}
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelScripting: Scripting leaf-proof starting "
		"(zenithscripting.lib + zenithecs.lib + zenithdatastream.lib + zenithmaths.lib + zenithcore.lib only)");

	// 1. Zenith_GraphBlackboard — the Behaviour Graph's own real, out-of-line
	//    class (Zenith_GraphBlackboard.cpp). No ECS, no engine involved.
	{
		Zenith_GraphBlackboard xBlackboard;
		Expect(!xBlackboard.HasValue("score"), "blackboard starts empty");

		Zenith_PropertyValue xValue;
		xValue.SetFloat(42.5f);
		xBlackboard.SetValue("score", xValue);

		Expect(xBlackboard.HasValue("score"), "SetValue makes HasValue true");
		Expect(xBlackboard.GetFloat("score") == 42.5f, "GetFloat round-trips the stored value");
		Expect(xBlackboard.GetBool("missing", true) == true, "GetBool returns the caller default for a missing key");
		Expect(xBlackboard.GetCount() == 1u, "GetCount reflects the one stored value");

		xBlackboard.RemoveValue("score");
		Expect(!xBlackboard.HasValue("score"), "RemoveValue clears the key");
	}

	// 2. ZenithECS — the one direct edge Zenith_BehaviourGraph.cpp itself
	//    declares (ResolveTargetEntity reaches through Zenith_SceneSystem).
	//    Bring up the SceneSystem singleton to force zenithecs.lib to link,
	//    mirroring step 1 of Tests/SentinelECS/main.cpp.
	{
		Zenith_SceneSystem* pxScenes = new Zenith_SceneSystem();
		Expect(pxScenes != nullptr, "Zenith_SceneSystem constructed (the Scripting -> ECS edge resolves)");
		Expect(&Zenith_SceneSystem::Get() == pxScenes, "Zenith_SceneSystem::Get() resolves to the live instance");
		delete pxScenes;
	}

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelScripting: PASS (Scripting leaf links + runs with no engine externals)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelScripting: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
