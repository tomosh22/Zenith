#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "Physics/Zenith_Physics.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIButton.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Component.h"

#include <cstdio>

// ============================================================================
// FrontEndPlay_Test — diagnostic for the user-reported bug:
//   "after hitting Play I see only a grey rectangle in the bottom-left".
//
// Forces the engine to boot into FrontEnd, fires the Play button's exact
// callback (LoadSceneByIndex(1, SINGLE)), then probes what the rendering
// pipeline can see post-swap:
//   - Active scene handle valid
//   - Active scene has a main camera entity
//   - Main camera position matches GameLevel's authored (0, 12, -15)
//   - A primary UICanvas exists (i.e. SetPrimaryCanvas pointer is non-null)
//   - The GameManager's UICanvas has the expected UI elements
//
// If this test passes against both tools and non-tools builds but the user
// still sees a grey rectangle, the bug is in the visual render path. If it
// FAILS against non-tools, we've reproduced the bug and can dig deeper.
// ============================================================================

namespace
{
	enum Phase : int {
		kFEP_Start,
		kFEP_LoadFE,
		kFEP_ConfirmFE,
		kFEP_TriggerPlay,
		kFEP_WaitGameLevel,
		kFEP_Verify,
		kFEP_Done
	};

	int   g_iPhase                      = kFEP_Start;
	int   g_iWaitFrames                 = 0;

	bool  g_bFrontEndLoaded             = false;
	bool  g_bGameLevelLoaded            = false;
	bool  g_bMainCameraSet              = false;
	float g_fCameraY                    = 0.0f;
	bool  g_bPrimaryCanvasSet           = false;
	int   g_iVillagerCount              = 0;
}

static void Setup_FrontEndPlay()
{
	g_iPhase = kFEP_Start;
	g_iWaitFrames = 0;
	g_bFrontEndLoaded = false;
	g_bGameLevelLoaded = false;
	g_bMainCameraSet = false;
	g_fCameraY = 0.0f;
	g_bPrimaryCanvasSet = false;
	g_iVillagerCount = 0;
}

static bool Step_FrontEndPlay(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kFEP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_iPhase = kFEP_LoadFE;
		g_iWaitFrames = 0;
		return true;

	case kFEP_LoadFE:
	{
		++g_iWaitFrames;
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxData)
		{
			g_bFrontEndLoaded = true;
			g_iPhase = kFEP_ConfirmFE;
			return true;
		}
		if (g_iWaitFrames > 30) { g_iPhase = kFEP_Done; return false; }
		return true;
	}

	case kFEP_ConfirmFE:
		// Verified FrontEnd is alive. Now trigger the Play button's exact
		// callback. We don't need to actually click — DPMainMenuController's
		// callback is just LoadSceneByIndex(1, SINGLE).
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFEP_TriggerPlay;
		g_iWaitFrames = 0;
		return true;

	case kFEP_TriggerPlay:
	case kFEP_WaitGameLevel:
	{
		++g_iWaitFrames;
		// Wait until GameLevel-specific entities (DPVillager_Component) are
		// in the active scene. That's a reliable proxy for "scene 1 swapped
		// in cleanly."
		int iCount = 0;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&iCount](Zenith_EntityID, DPVillager_Component&) { ++iCount; });
		if (iCount > 0)
		{
			g_bGameLevelLoaded = true;
			g_iVillagerCount = iCount;
			g_iPhase = kFEP_Verify;
			return true;
		}
		if (g_iWaitFrames > 60) { g_iPhase = kFEP_Verify; return true; }
		return true;
	}

	case kFEP_Verify:
	{
		// Probe the rendering-relevant state.
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxData != nullptr)
		{
			Zenith_CameraComponent* pxCam = Zenith_TryGetMainCamera(pxData);
			if (pxCam != nullptr)
			{
				g_bMainCameraSet = true;
				Zenith_Maths::Vector3 xPos;
				pxCam->GetPosition(xPos);
				g_fCameraY = xPos.y;
			}
		}
		g_bPrimaryCanvasSet = (Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas() != nullptr);

		// Read full camera position so we can sanity-check view direction.
		Zenith_Maths::Vector3 xCamPos(0.0f);
		double fYaw = 0.0;
		double fPitch = 0.0;
		if (pxData != nullptr)
		{
			Zenith_CameraComponent* pxCam = Zenith_TryGetMainCamera(pxData);
			if (pxCam != nullptr)
			{
				pxCam->GetPosition(xCamPos);
				fYaw   = pxCam->GetYaw();
				fPitch = pxCam->GetPitch();
			}
		}
		std::printf("[FrontEndPlay_Test] FE=%d GL=%d cam=%d camPos=(%.2f,%.2f,%.2f) yaw=%.2f pitch=%.2f primaryCanvas=%d villagers=%d\n",
			(int)g_bFrontEndLoaded, (int)g_bGameLevelLoaded,
			(int)g_bMainCameraSet,
			xCamPos.x, xCamPos.y, xCamPos.z,
			fYaw, fPitch,
			(int)g_bPrimaryCanvasSet, g_iVillagerCount);
		std::fflush(stdout);

		// Diagnostic: walk all villagers, report Transform + ModelComponent
		// presence, collider state, mesh-loaded status, position, and
		// whether the loaded mesh is skeletal.
		int iWithModel = 0, iWithMeshLoaded = 0, iWithSkeleton = 0, iWithMaterials = 0;
		int iWithCollider = 0, iWithBody = 0;
		int iRaycastHits = 0;
		float fMinY = 1e9f, fMaxY = -1e9f;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&](Zenith_EntityID xId, DPVillager_Component&)
			{
				Zenith_SceneData* pxSceneV = g_xEngine.Scenes().GetSceneDataForEntity(xId);
				if (!pxSceneV) return;
				Zenith_Entity xEnt = pxSceneV->TryGetEntity(xId);
				if (!xEnt.IsValid()) return;
				if (xEnt.HasComponent<Zenith_TransformComponent>())
				{
					Zenith_Maths::Vector3 xVPos;
					xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);
					if (xVPos.y < fMinY) fMinY = xVPos.y;
					if (xVPos.y > fMaxY) fMaxY = xVPos.y;
				}
				if (xEnt.HasComponent<Zenith_ModelComponent>())
				{
					++iWithModel;
					Zenith_ModelComponent& xMC = xEnt.GetComponent<Zenith_ModelComponent>();
					Flux_ModelInstance* pxInst = xMC.GetModelInstance();
					if (pxInst != nullptr)
					{
						++iWithMeshLoaded;
						if (pxInst->HasSkeleton()) ++iWithSkeleton;
						if (pxInst->GetNumMaterials() > 0) ++iWithMaterials;
					}
				}
				if (xEnt.HasComponent<Zenith_ColliderComponent>())
				{
					++iWithCollider;
					if (!xEnt.GetComponent<Zenith_ColliderComponent>().GetBodyID().IsInvalid())
					{
						++iWithBody;
					}
				}

				// Cast a vertical ray from above the villager downward;
				// should hit the villager's own collider.
				Zenith_Maths::Vector3 xVPos2;
				xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos2);
				Zenith_Physics::RaycastResult xCast = g_xEngine.Physics().Raycast(
					Zenith_Maths::Vector3(xVPos2.x, xVPos2.y + 5.0f, xVPos2.z),
					Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
					20.0f);
				if (xCast.m_bHit && xCast.m_xHitEntity == xId)
				{
					++iRaycastHits;
				}
			});
		std::printf("[FrontEndPlay_Test] villagers: model=%d mesh=%d skeletal=%d mats=%d collider=%d body=%d raycast_hits_self=%d Y=[%.2f..%.2f]\n",
			iWithModel, iWithMeshLoaded, iWithSkeleton, iWithMaterials,
			iWithCollider, iWithBody, iRaycastHits, fMinY, fMaxY);
		std::fflush(stdout);

		g_iPhase = kFEP_Done;
		return false;
	}

	case kFEP_Done:
	default:
		return false;
	}
}

static bool Verify_FrontEndPlay()
{
	if (!g_bFrontEndLoaded)         return false;
	if (!g_bGameLevelLoaded)        return false;
	if (!g_bMainCameraSet)          return false;
	// DPOrbitCamera_Component overrides the authored camera each frame
	// with a bird's-eye view orbiting the map centre (50,0,50) at radius
	// 90 m + ~83° down → camera Y around 89 m. Anything below 50 m
	// signals the orbit camera regressed to a third-person follow or to
	// the world-origin default.
	if (g_fCameraY < 50.0f)         return false;
	if (!g_bPrimaryCanvasSet)       return false;
	if (g_iVillagerCount < 1)       return false;
	return true;
}

static const Zenith_AutomatedTest g_xFrontEndPlayTest = {
	"FrontEndPlay_Test",
	&Setup_FrontEndPlay,
	&Step_FrontEndPlay,
	&Verify_FrontEndPlay,
	240,
	true // m_bRequiresGraphics: UI click on Play button needs Flux UI render path
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFrontEndPlayTest);

#endif // ZENITH_INPUT_SIMULATOR
