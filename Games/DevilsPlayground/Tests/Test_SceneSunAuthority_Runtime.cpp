#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Components/Zenith_SunComponent.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "ZenithECS/Zenith_SceneSystem.h"

namespace
{
	bool g_bSunRuntimeFailed = false;
	bool g_bSunRuntimeChanged = false;
	bool g_bSunRuntimeSawRegen = false;
	bool g_bSunRuntimeCSMChanged = false;
	bool g_bSunRuntimeRestoring = false;
	bool g_bSunRuntimeSawRestoreRegen = false;
	bool g_bSunRuntimeRestored = false;
	int g_iSunRuntimeChangeFrame = -1;
	int g_iSunRuntimeCompleteFrame = -1;
	int g_iSunRuntimeRestoreFrame = -1;
	Zenith_Maths::Matrix4 g_xSunRuntimeBeforeCSM(1.0f);

	void SunRuntimeFail(const char* szReason)
	{
		if (!g_bSunRuntimeFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[DPSceneSunRuntime] %s", szReason);
		}
		g_bSunRuntimeFailed = true;
	}

	Zenith_SunComponent* FindActiveSun()
	{
		Zenith_SunComponent* pxSun = nullptr;
		g_xEngine.Scenes().QueryActiveScene<Zenith_SunComponent>().ForEach(
			[&](Zenith_EntityID, Zenith_SunComponent& xSun)
			{
				if (pxSun == nullptr) pxSun = &xSun;
			});
		return pxSun;
	}

	bool MatricesDiffer(const Zenith_Maths::Matrix4& xA, const Zenith_Maths::Matrix4& xB)
	{
		for (u_int uColumn = 0u; uColumn < 4u; uColumn++)
		{
			for (u_int uRow = 0u; uRow < 4u; uRow++)
			{
				if (fabsf(xA[uColumn][uRow] - xB[uColumn][uRow]) > 0.0001f)
				{
					return true;
				}
			}
		}
		return false;
	}
}

static void Setup_SceneSunAuthorityRuntime()
{
	g_bSunRuntimeFailed = false;
	g_bSunRuntimeChanged = false;
	g_bSunRuntimeSawRegen = false;
	g_bSunRuntimeCSMChanged = false;
	g_bSunRuntimeRestoring = false;
	g_bSunRuntimeSawRestoreRegen = false;
	g_bSunRuntimeRestored = false;
	g_iSunRuntimeChangeFrame = -1;
	g_iSunRuntimeCompleteFrame = -1;
	g_iSunRuntimeRestoreFrame = -1;
	g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
}

static bool Step_SceneSunAuthorityRuntime(int iFrame)
{
	if (g_bSunRuntimeFailed) return false;

	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	Zenith_SunComponent* pxSun = FindActiveSun();
	if (pxSun == nullptr)
	{
		if (iFrame > 30) SunRuntimeFail("ProcLevel has no authored Zenith_SunComponent");
		return !g_bSunRuntimeFailed;
	}

	if (!g_bSunRuntimeChanged)
	{
		// The test callback runs before the first graphics upload.  Give the
		// scene-owned authority one complete frame to reach frame constants.
		if (iFrame < 2)
		{
			return true;
		}
		if (!xIBL.IsReady() || xIBL.m_bSkyIBLDirty || xIBL.m_eRegenState != IBL_REGEN_IDLE)
		{
			if (iFrame >= 120)
			{
				SunRuntimeFail("initial scene IBL did not become ready/idle within 120 frames");
				return false;
			}
			return true;
		}

		const Zenith_Maths::Vector3 xNightDirection = g_xEngine.FluxGraphics().GetSunDir();
		const Zenith_Maths::Vector4 xNightKey = g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunColour_Pad;
		if (pxSun->GetDirectionMode() != SUN_DIRECTION_MODE_TIME_OF_DAY
			|| fabsf(pxSun->GetTimeOfDayAngleDegrees() - 270.0f) > 0.001f)
		{
			SunRuntimeFail("ProcLevel Sun is not authored at the midnight time-of-day angle");
			return false;
		}
		if (xNightDirection.y < 0.99f || xNightKey.x != 0.0f || xNightKey.y != 0.0f || xNightKey.z != 0.0f)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[DPSceneSunRuntime] OBSERVED midnight dir=(%.4f, %.4f, %.4f) key=(%.4f, %.4f, %.4f)",
				xNightDirection.x, xNightDirection.y, xNightDirection.z,
				xNightKey.x, xNightKey.y, xNightKey.z);
			SunRuntimeFail("midnight did not resolve to an upward travel direction and zero transmittance key");
			return false;
		}

		g_xSunRuntimeBeforeCSM = g_xEngine.Shadows().GetSunViewProjMatrix(0u);
		pxSun->SetTimeOfDayAngleDegrees(90.0f);
		g_iSunRuntimeChangeFrame = iFrame;
		g_bSunRuntimeChanged = true;
		return true;
	}

	if (!xIBL.IsReady())
	{
		SunRuntimeFail("IsReady unlatched during a runtime environment regeneration");
		return false;
	}

	if (g_bSunRuntimeRestoring)
	{
		if (xIBL.m_bSkyIBLDirty || xIBL.m_eRegenState != IBL_REGEN_IDLE)
		{
			g_bSunRuntimeSawRestoreRegen = true;
		}
		if (g_bSunRuntimeSawRestoreRegen
			&& !xIBL.m_bSkyIBLDirty && xIBL.m_eRegenState == IBL_REGEN_IDLE)
		{
			const Zenith_Maths::Vector3 xNightDirection = g_xEngine.FluxGraphics().GetSunDir();
			const Zenith_Maths::Vector4 xNightKey = g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunColour_Pad;
			if (pxSun->GetDirectionMode() != SUN_DIRECTION_MODE_TIME_OF_DAY
				|| fabsf(pxSun->GetTimeOfDayAngleDegrees() - 270.0f) > 0.001f
				|| xNightDirection.y < 0.99f
				|| xNightKey.x != 0.0f || xNightKey.y != 0.0f || xNightKey.z != 0.0f)
			{
				SunRuntimeFail("batch teardown did not restore the authored midnight environment");
				return false;
			}
			g_bSunRuntimeRestored = true;
			return false;
		}
		if (iFrame - g_iSunRuntimeRestoreFrame > 30)
		{
			SunRuntimeFail("midnight teardown IBL regeneration did not converge within 30 frames");
			return false;
		}
		return true;
	}
	if (xIBL.m_bSkyIBLDirty || xIBL.m_eRegenState != IBL_REGEN_IDLE)
	{
		g_bSunRuntimeSawRegen = true;
	}

	if (MatricesDiffer(g_xSunRuntimeBeforeCSM, g_xEngine.Shadows().GetSunViewProjMatrix(0u)))
	{
		g_bSunRuntimeCSMChanged = true;
	}

	if (g_bSunRuntimeSawRegen && !xIBL.m_bSkyIBLDirty && xIBL.m_eRegenState == IBL_REGEN_IDLE)
	{
		g_iSunRuntimeCompleteFrame = iFrame;
		const Zenith_Maths::Vector3 xDayDirection = g_xEngine.FluxGraphics().GetSunDir();
		const Zenith_Maths::Vector4 xDayKey = g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunColour_Pad;
		if (xDayDirection.y > -0.99f || xDayKey.x <= 0.0f || xDayKey.y <= 0.0f || xDayKey.z <= 0.0f)
		{
			SunRuntimeFail("noon direction/key did not reach frame constants after regeneration");
			return false;
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[DPSceneSunRuntime] OBSERVED IBL converged in %d frames; IsReady stayed latched; CSM changed=%d",
			g_iSunRuntimeCompleteFrame - g_iSunRuntimeChangeFrame,
			g_bSunRuntimeCSMChanged ? 1 : 0);
		// Restore the scene's committed midnight state and wait for its IBL work
		// to finish. Batch-mode tests share renderer state, so a runtime-authority
		// regression must leave no transient noon environment for the next case.
		pxSun->SetTimeOfDayAngleDegrees(270.0f);
		g_bSunRuntimeRestoring = true;
		g_iSunRuntimeRestoreFrame = iFrame;
		return true;
	}

	if (iFrame - g_iSunRuntimeChangeFrame > 30)
	{
		SunRuntimeFail("amortised IBL regeneration did not converge within 30 frames");
		return false;
	}
	return true;
}

static bool Verify_SceneSunAuthorityRuntime()
{
	return !g_bSunRuntimeFailed
		&& g_bSunRuntimeChanged
		&& g_bSunRuntimeSawRegen
		&& g_bSunRuntimeCSMChanged
		&& g_bSunRuntimeRestored
		&& g_iSunRuntimeCompleteFrame > g_iSunRuntimeChangeFrame;
}

static const Zenith_AutomatedTest g_xSceneSunAuthorityRuntimeTest = {
	"Test_SceneSunAuthority_Runtime",
	&Setup_SceneSunAuthorityRuntime,
	&Step_SceneSunAuthorityRuntime,
	&Verify_SceneSunAuthorityRuntime,
	/* maxFrames */ 180,
	/* requiresGraphics */ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSceneSunAuthorityRuntimeTest);

#endif // ZENITH_INPUT_SIMULATOR
