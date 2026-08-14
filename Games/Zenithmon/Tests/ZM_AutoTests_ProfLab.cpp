#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_ProfLab -- the arrival gate for Aster's Lab (build index read
// from ZM_GetWorldSpec(ZM_SCENE_PROFLAB), never spelled here).
//
// WHAT IT PROVES, and why a WorldSpec boot unit cannot: the ProfLab row has
// shipped in the compiled world table for some time, and every one of the 12
// ZM_Data WorldSpec units is GREEN against it -- including the
// FrontEnd-reachability walk, which visits ProfLab through Dawnmere's already
// declared connection. None of them can see the live wedge, because
// ZM_GameStateManager::IsWarpDestinationValid consults ONLY the compiled
// ZM_WorldSpec tag list and never Zenith_SceneSystem's build-index registry. An
// UNREGISTERED destination therefore passes validation, the warp is ACCEPTED,
// and the machine then parks in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN -- which
// has NO timeout -- leaving the player frozen behind a permanently opaque fade.
// A silent hang, not a crash. This test is what turns that into a red run.
//
// C6 DEVIATION -- ZM-D-147, stated verbatim from Docs/TestPlan.md:
//   "DEVIATION -- the two COMMITTED asset families do NOT skip (ZM-D-147).
//   Assets/Navmesh/Dawnmere.znavmesh and Assets/Scenes/Dawnmere.zscen are
//   TRACKED, so on CI they are always present and their absence is a DEFECT,
//   not a condition to tolerate. ZM_NavmeshAsset_Test and
//   ZM_DawnmereHeadless_Test therefore carry NO RequestSkip. Skipping would
//   also be self-defeating: a skip counts as a PASS, so the one gate proving
//   the committed navmesh loads would go quiet exactly when it broke. C6
//   continues to apply to every git-ignored family."
// Assets/Scenes/ProfLab.zscen is in that same TRACKED family (.gitignore
// re-includes **/*.zscen), so this test performs NO asset probe and calls
// RequestSkip NOWHERE. It also never touches the git-ignored Dawnmere terrain
// family: the warp is issued by RequestWarp from the PLAYERLESS FrontEnd, not
// by walking a Dawnmere trigger, so there is no ignored prerequisite left to
// guard and therefore no split-guard to get wrong.
//
// C2 (fixed dt): 1/60, the stated default. This test presses no key and drives
// no traversal -- every phase waits on the warp machine -- so the 30 Hz cadence
// ZM_PlayerHomeRoundTrip_Test justifies for its 128 m input drive buys nothing
// here and would only halve the resolution of the fade/barrier observations.
//
// STACK-FRAME RULE (ZM-D-141): per-phase driver functions with a thin dispatch
// Step. This test holds no ZM_GameState local, so the rule does not compel the
// split -- it is adopted anyway because it costs nothing and is immune if a
// later clause reaches for one. (A monolithic /Od Step frame is the SUM of
// every local in every phase; ~six ZM_GameState locals measured 1,312,136 bytes
// against a 1,048,576-byte reserve and died in __chkstk on the first call.)
//
// EVERY placement value this file asserts is READ from
// Source/World/ZM_ProfLabPlacement.h -- the same header the ZENITH_TOOLS
// authoring block in Zenithmon.cpp reads. Re-spelling a literal at both sites
// would make the assertion decorative: it could not red a drift, because both
// sides would move together. The only numeric literals below are epsilons and
// frame deadlines.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"          // clause I4 -- the exit sensor's live configuration
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // the SHIPPED facing helpers -- run, not re-derived
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
	// See the C2 note in the file header: no input is driven, so the stated
	// 1/60 default is the right cadence and no deviation needs justifying.
	constexpr float fPROFLAB_FIXED_DT = 1.0f / 60.0f;
	constexpr float fPOSITION_EPSILON = 0.001f;
	constexpr float fVELOCITY_EPSILON = 0.0001f;
	constexpr float fYAW_EPSILON = 0.0001f;

	// The tolerance on a RAYCAST-MEASURED body face (clause I3). Looser than
	// fPOSITION_EPSILON on purpose and for a stated reason: a probe measures a
	// SURFACE through the physics broadphase rather than reading an authored
	// number, so it carries the cast's own resolution as well as the shape's. Still
	// an order of magnitude tighter than the gap between the compiled body contract
	// (0.400 m half-width) and the scale-derived box that would replace it if
	// ZM_GreyboxVisual stopped installing the explicit dimensions (~0.346 m), which
	// is the substitution this epsilon has to be able to tell apart.
	constexpr float fBODY_PROBE_EPSILON = 0.01f;

	// Phase budgets. Their sum (180 + 600 + 120 = 900) sits well inside the
	// registered 1,200-frame cap ON PURPOSE: a phase deadline must fire BEFORE
	// the harness cap, or the diagnosis is replaced by a bare "timed out" with
	// no captured state -- which is precisely the failure mode the missing
	// RegisterSceneBuildIndex line produces if nothing names it.
	constexpr int iPROFLAB_BOOTSTRAP_DEADLINE = 180;
	constexpr int iPROFLAB_WARP_IN_DEADLINE = 600;
	constexpr int iPROFLAB_ARRIVAL_DEADLINE = 120;

	// ---- The two failure texts that must diagnose themselves ---------------

	// The whole point of the WarpIn deadline. WAITING_FOR_SPAWN has no timeout,
	// so without this text the ONLY symptom of an unregistered build index is a
	// frame cap that names nothing.
	const char* const szPROFLAB_WARP_IN_DEADLINE_FAILURE =
		"ProfLab never became the active scene. FIRST SUSPECT: a MISSING "
		"RegisterSceneBuildIndex(41, GAME_ASSETS_DIR \"Scenes/ProfLab\" "
		"ZENITH_SCENE_EXT) line in Project_LoadInitialScene (Zenithmon.cpp) -- "
		"ZM_GameStateManager::IsWarpDestinationValid consults ONLY the compiled "
		"ZM_WorldSpec tag list and never the scene registry, so RequestWarp is "
		"ACCEPTED for an unregistered index and the machine then parks in "
		"ZM_WARP_TRANSITION_WAITING_FOR_SPAWN, which has NO timeout -- making a "
		"bare frame cap the only symptom";

	// The primary-mutation detector. Re-pointing the registration at another
	// interior leaves every fade/opacity/camera/teardown clause GREEN, because
	// a warp really does complete: only the CONTENT is wrong.
	const char* const szPROFLAB_WRONG_SCENE_FAILURE =
		"the scene loaded at ProfLab's build index is NOT ProfLab -- its floor "
		"entity is missing or misplaced. Check that the RegisterSceneBuildIndex "
		"line in Project_LoadInitialScene (Zenithmon.cpp) points that index at "
		"GAME_ASSETS_DIR \"Scenes/ProfLab\" ZENITH_SCENE_EXT and not at another "
		"interior";

	const char* const szPROFLAB_BARRIER_FAILURE =
		"the ProfLab transition skipped a fade/opacity/camera barrier";

	// ---- Scene-shaped views (the shipped WorldTraversal shape) -------------

	struct ManagerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_GameStateManager* m_pxManager = nullptr;
		u_int m_uCount = 0u;
	};

	struct PlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xScale = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	struct CameraView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera* m_pxFollow = nullptr;
		Zenith_CameraComponent* m_pxCamera = nullptr;
	};

	bool FindUniqueManager(ManagerView& xOut)
	{
		xOut = ManagerView{};
		g_xEngine.Scenes().QueryAllScenes<ZM_GameStateManager>().ForEach(
			[&xOut](Zenith_EntityID xEntityID, ZM_GameStateManager& xManager)
			{
				++xOut.m_uCount;
				if (xOut.m_uCount == 1u)
				{
					xOut.m_xEntityID = xEntityID;
					xOut.m_pxManager = &xManager;
				}
			});
		return xOut.m_uCount == 1u && xOut.m_pxManager != nullptr;
	}

	bool FindUniqueActivePlayer(PlayerView& xOut)
	{
		xOut = PlayerView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xEntityID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xEntityID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
				xTransform.GetScale(xOut.m_xScale);
			});
		return uCount == 1u;
	}

	bool FindUniqueActiveCamera(CameraView& xOut)
	{
		xOut = CameraView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_FollowCamera,
			Zenith_CameraComponent>().ForEach(
			[&](Zenith_EntityID xEntityID,
				ZM_FollowCamera& xFollow,
				Zenith_CameraComponent& xCamera)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xEntityID;
				xOut.m_pxFollow = &xFollow;
				xOut.m_pxCamera = &xCamera;
			});
		return uCount == 1u;
	}

	bool FadeVisualMatchesManager(const ManagerView& xManager)
	{
		Zenith_Entity xEntity =
			g_xEngine.Scenes().ResolveEntity(xManager.m_xEntityID);
		Zenith_UIComponent* pxUI = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
		Zenith_UI::Zenith_UIElement* pxElement = pxUI != nullptr
			? pxUI->FindElement(ZM_GameStateManager::szFADE_ELEMENT_NAME) : nullptr;
		if (pxElement == nullptr
			|| pxElement->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return false;
		}
		Zenith_UI::Zenith_UIOverlay* pxFade =
			static_cast<Zenith_UI::Zenith_UIOverlay*>(pxElement);
		const float fAlpha = xManager.m_pxManager->GetFadeAlpha();
		return std::isfinite(fAlpha) && fAlpha >= 0.0f && fAlpha <= 1.0f
			&& std::fabs(pxFade->GetGroupAlpha() - fAlpha) <= 0.0001f
			&& (fAlpha == 0.0f || pxFade->IsVisible());
	}

	bool ManagerFadeOverlayIsHidden(const ManagerView& xManager)
	{
		Zenith_Entity xEntity =
			g_xEngine.Scenes().ResolveEntity(xManager.m_xEntityID);
		Zenith_UIComponent* pxUI = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
		Zenith_UI::Zenith_UIElement* pxElement = pxUI != nullptr
			? pxUI->FindElement(ZM_GameStateManager::szFADE_ELEMENT_NAME) : nullptr;
		return pxElement != nullptr
			&& pxElement->GetType() == Zenith_UI::UIElementType::Overlay
			&& !pxElement->IsVisible();
	}

	// ---- The destination, resolved rather than spelled ---------------------
	//
	// 41 is never used as a VALUE here -- it is resolved, never typed. (It does
	// appear as PROSE inside szPROFLAB_WARP_IN_DEADLINE_FAILURE above, which
	// quotes the registration line a reader has to go and check; nothing
	// compares against that text.) The value exists in exactly one place in the
	// game -- the ZM_WorldSpec row -- and in one more in the authoring
	// (Project_LoadInitialScene). Both this test and that registration read the
	// row, so a re-numbered scene moves them together instead of silently
	// leaving one behind.
	u_int ProfLabBuildIndex()
	{
		return ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex;
	}

	// The spawn tag is the HEADER's, not the compiled table's, and that choice
	// has teeth. ZM_GameStateManager::IsWarpDestinationValid validates against
	// the ZM_WorldSpec tag list, so driving the warp with the header's mirror
	// means a header/table divergence is REFUSED here -- loudly, in
	// FrontEndBootstrap, with accepted=0 and both tags printed. Reading the
	// table instead would have made the two agree by construction and quietly
	// dropped that check. (The static half of the same claim is
	// ZM_WorldTraversal/ProfLab_HeaderSpawnTagMatchesTheWorldSpecRow in
	// Tests/ZM_Tests_ProfLabPlacement.cpp.)
	const char* ProfLabSpawnTag()
	{
		return szZM_PROFLAB_SPAWN_TAG;
	}

	const char* TransitionStateName(ZM_WARP_TRANSITION_STATE eState)
	{
		switch (eState)
		{
		case ZM_WARP_TRANSITION_IDLE:               return "IDLE";
		case ZM_WARP_TRANSITION_QUEUED:             return "QUEUED";
		case ZM_WARP_TRANSITION_WAITING_FOR_SCENE:  return "WAITING_FOR_SCENE";
		case ZM_WARP_TRANSITION_WAITING_FOR_SPAWN:  return "WAITING_FOR_SPAWN";
		case ZM_WARP_TRANSITION_WAITING_FOR_CAMERA: return "WAITING_FOR_CAMERA";
		case ZM_WARP_TRANSITION_FADING_IN:          return "FADING_IN";
		}
		return "UNKNOWN";
	}

	const char* SpawnLookupResultName(ZM_SPAWN_POINT_LOOKUP_RESULT eResult)
	{
		switch (eResult)
		{
		case ZM_SPAWN_POINT_LOOKUP_FOUND:         return "FOUND";
		case ZM_SPAWN_POINT_LOOKUP_MISSING:       return "MISSING";
		case ZM_SPAWN_POINT_LOOKUP_DUPLICATE:     return "DUPLICATE";
		case ZM_SPAWN_POINT_LOOKUP_INVALID_TAG:   return "INVALID_TAG";
		case ZM_SPAWN_POINT_LOOKUP_INVALID_SCENE: return "INVALID_SCENE";
		}
		return "UNKNOWN";
	}

	// ---- Phases + the state every clause reads -----------------------------

	enum class ProfLabPhase
	{
		FrontEndBootstrap,
		WarpIn,
		VerifyArrival,
		Done,
	};

	ProfLabPhase g_eProfLabPhase = ProfLabPhase::Done;
	int g_iProfLabPhaseFrames = 0;
	bool g_bProfLabPassed = false;
	bool g_bProfLabFadeOutSeen = false;
	bool g_bProfLabOpaqueLoadSeen = false;
	bool g_bProfLabCameraBarrierSeen = false;
	bool g_bProfLabFadeInSeen = false;
	const char* g_szProfLabFailure = "test did not reach verification";
	Zenith_Scene g_xProfLabFrontEndScene;
	Zenith_EntityID g_xProfLabManagerEntityID = INVALID_ENTITY_ID;

	// Verify logs ONE const char*, so anything captured has to be formatted
	// into storage that outlives the failing frame. A file-scope buffer is that
	// storage: a red run must be diagnosable from its log alone, without a
	// rerun, because the harness's JSON `failures` array is always empty and
	// this Zenith_Error line is the only diagnostic channel there is.
	char g_aszProfLabFailureDetail[1536] = {};

	void FailProfLab(const char* szReason)
	{
		g_szProfLabFailure = szReason;
		g_bProfLabPassed = false;
		g_eProfLabPhase = ProfLabPhase::Done;
	}

	void FailProfLabDetailed(const char* szFormat, ...)
	{
		va_list xArgs;
		va_start(xArgs, szFormat);
		std::vsnprintf(g_aszProfLabFailureDetail,
			sizeof(g_aszProfLabFailureDetail), szFormat, xArgs);
		va_end(xArgs);
		FailProfLab(g_aszProfLabFailureDetail);
	}

	// The four fade/opacity/camera latches, appended to any failure text so a
	// red run says WHICH barrier was never observed rather than merely that one
	// was not.
	void AppendBarrierLatches(char* pszBuffer, size_t ulCapacity)
	{
		const size_t ulUsed = std::strlen(pszBuffer);
		if (ulUsed + 1u >= ulCapacity)
		{
			return;
		}
		std::snprintf(pszBuffer + ulUsed, ulCapacity - ulUsed,
			" [barriers: fadeOut=%d opaqueLoad=%d cameraBarrier=%d fadeIn=%d]",
			g_bProfLabFadeOutSeen ? 1 : 0,
			g_bProfLabOpaqueLoadSeen ? 1 : 0,
			g_bProfLabCameraBarrierSeen ? 1 : 0,
			g_bProfLabFadeInSeen ? 1 : 0);
	}

	// ---- PHASE 1: the playerless FrontEnd issues the warp -------------------

	bool ProfLabPhaseFrontEndBootstrap()
	{
		const Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		ManagerView xManager;
		if (!xScene.IsValid()
			|| g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex != 0
			|| !FindUniqueManager(xManager))
		{
			if (g_iProfLabPhaseFrames > iPROFLAB_BOOTSTRAP_DEADLINE)
			{
				FailProfLabDetailed(
					"FrontEnd/manager bootstrap did not settle in %d frames "
					"[activeSceneValid=%d activeBuildIndex=%d uniqueManagers=%u]",
					iPROFLAB_BOOTSTRAP_DEADLINE,
					xScene.IsValid() ? 1 : 0,
					g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex,
					xManager.m_uCount);
				return false;
			}
			return true;
		}

		ZM_GameStateManager& xState = *xManager.m_pxManager;
		if (!xState.IsAuthoritativeSingleton()
			|| xState.GetTransitionState() != ZM_WARP_TRANSITION_IDLE
			|| xState.GetIssuedLoadRequestCount() != 0u
			|| xState.GetFadeAlpha() != 0.0f
			|| !FadeVisualMatchesManager(xManager)
			|| !ManagerFadeOverlayIsHidden(xManager)
			|| g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().Count() != 0u)
		{
			FailProfLabDetailed(
				"FrontEnd traversal authority was not idle/playerless "
				"[authoritative=%d state=%s issuedLoads=%u fadeAlpha=%.5f "
				"fadeVisualOK=%d overlayHidden=%d activePlayers=%u]",
				xState.IsAuthoritativeSingleton() ? 1 : 0,
				TransitionStateName(xState.GetTransitionState()),
				xState.GetIssuedLoadRequestCount(),
				(double)xState.GetFadeAlpha(),
				FadeVisualMatchesManager(xManager) ? 1 : 0,
				ManagerFadeOverlayIsHidden(xManager) ? 1 : 0,
				g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().Count());
			return false;
		}

		g_xProfLabFrontEndScene = xScene;
		g_xProfLabManagerEntityID = xManager.m_xEntityID;

		const u_int uTargetBuildIndex = ProfLabBuildIndex();
		const char* szSpawnTag = ProfLabSpawnTag();
		const bool bAccepted =
			ZM_GameStateManager::RequestWarp(uTargetBuildIndex, szSpawnTag);
		if (!bAccepted
			|| xState.GetTransitionState() != ZM_WARP_TRANSITION_QUEUED
			|| xState.GetTargetBuildIndex() != uTargetBuildIndex
			|| std::strcmp(xState.GetTargetSpawnTag(), szSpawnTag) != 0
			|| xState.GetIssuedLoadRequestCount() != 0u
			|| xState.GetFadeAlpha() != 0.0f
			|| g_xEngine.Scenes().GetActiveScene() != g_xProfLabFrontEndScene)
		{
			// The offered-tag list is printed alongside the requested tag because
			// the most likely reason RequestWarp is REFUSED here is that
			// ZM_ProfLabPlacement.h's szZM_PROFLAB_SPAWN_TAG and the ProfLab row
			// in ZM_WorldSpec.cpp have drifted apart -- IsWarpDestinationValid
			// consults only the latter, so seeing both side by side turns a bare
			// "accepted=0" into the actual diagnosis.
			const ZM_WorldSpec& xProfLabSpec = ZM_GetWorldSpec(ZM_SCENE_PROFLAB);
			FailProfLabDetailed(
				"RequestWarp(%u, \"%s\") was not a queued, deferred FrontEnd "
				"SINGLE transition [accepted=%d state=%s targetIndex=%u "
				"targetTag='%s' issuedLoads=%u fadeAlpha=%.5f "
				"activeStillFrontEnd=%d; ZM_WorldSpec row '%s' offers %u tag(s), "
				"first='%s']",
				uTargetBuildIndex, szSpawnTag,
				bAccepted ? 1 : 0,
				TransitionStateName(xState.GetTransitionState()),
				xState.GetTargetBuildIndex(),
				xState.GetTargetSpawnTag(),
				xState.GetIssuedLoadRequestCount(),
				(double)xState.GetFadeAlpha(),
				g_xEngine.Scenes().GetActiveScene() == g_xProfLabFrontEndScene ? 1 : 0,
				xProfLabSpec.m_szName,
				xProfLabSpec.m_uSpawnTagCount,
				xProfLabSpec.m_uSpawnTagCount > 0u
					? xProfLabSpec.m_pszSpawnTags[0] : "<none>");
			return false;
		}

		g_eProfLabPhase = ProfLabPhase::WarpIn;
		g_iProfLabPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 2: ride the transition, latching every barrier ---------------

	bool ProfLabPhaseWarpIn()
	{
		ManagerView xManager;
		if (!FindUniqueManager(xManager)
			|| xManager.m_xEntityID != g_xProfLabManagerEntityID
			|| !FadeVisualMatchesManager(xManager))
		{
			FailProfLabDetailed(
				"the persistent manager or its fade visual changed mid-warp "
				"[uniqueManagers=%u sameEntity=%d fadeVisualOK=%d]",
				xManager.m_uCount,
				xManager.m_xEntityID == g_xProfLabManagerEntityID ? 1 : 0,
				xManager.m_pxManager != nullptr
					&& FadeVisualMatchesManager(xManager) ? 1 : 0);
			return false;
		}

		ZM_GameStateManager& xState = *xManager.m_pxManager;
		const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		const int iActiveBuildIndex =
			g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
		const int iTargetBuildIndex = (int)ProfLabBuildIndex();

		// The SINGLE load must be deferred until the screen is fully opaque.
		if (xState.GetTransitionState() == ZM_WARP_TRANSITION_QUEUED)
		{
			if (xState.GetIssuedLoadRequestCount() != 0u
				|| xState.GetFadeAlpha() >= 1.0f)
			{
				FailProfLabDetailed(
					"the ProfLab SINGLE load was issued before the opaque "
					"fade-out [issuedLoads=%u fadeAlpha=%.5f]",
					xState.GetIssuedLoadRequestCount(),
					(double)xState.GetFadeAlpha());
				return false;
			}
			g_bProfLabFadeOutSeen |= xState.GetFadeAlpha() > 0.0f;
		}
		if (xState.GetIssuedLoadRequestCount() == 1u)
		{
			g_bProfLabOpaqueLoadSeen |= xState.GetFadeAlpha() == 1.0f;
		}

		if (iActiveBuildIndex == iTargetBuildIndex)
		{
			PlayerView xPlayer;
			CameraView xCamera;
			Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);

			if (xState.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_CAMERA
				&& FindUniqueActivePlayer(xPlayer)
				&& FindUniqueActiveCamera(xCamera)
				&& pxData != nullptr
				&& pxData->GetMainCameraEntity() == xCamera.m_xEntityID
				&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
				&& !xPlayer.m_pxController->IsMovementEnabled()
				&& xState.GetFadeAlpha() == 1.0f)
			{
				g_bProfLabCameraBarrierSeen = true;
			}

			if (xState.GetTransitionState() == ZM_WARP_TRANSITION_FADING_IN)
			{
				// Every probe is evaluated ONCE into a local before anything is
				// formatted. FindUniqueActivePlayer / FindUniqueActiveCamera
				// OVERWRITE their out-param, and the order in which function
				// arguments are evaluated is unspecified in C++ -- calling them
				// again inside the failure formatter would let one argument
				// clobber the view another argument is reading.
				const bool bPlayerFound = FindUniqueActivePlayer(xPlayer);
				const bool bCameraFound = FindUniqueActiveCamera(xCamera);
				const bool bCameraIsMain = bCameraFound && pxData != nullptr
					&& pxData->GetMainCameraEntity() == xCamera.m_xEntityID;
				const bool bCameraTargetsPlayer = bCameraFound && bPlayerFound
					&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID;
				const bool bMovementEnabled = bPlayerFound
					&& xPlayer.m_pxController->IsMovementEnabled();
				if (!bPlayerFound || !bCameraFound || pxData == nullptr
					|| !bCameraIsMain || !bCameraTargetsPlayer || bMovementEnabled)
				{
					FailProfLabDetailed(
						"the ProfLab fade-in crossed an unready camera/input "
						"barrier [player=%d camera=%d sceneData=%d "
						"cameraIsMain=%d cameraTargetsPlayer=%d "
						"movementEnabled=%d fadeAlpha=%.5f]",
						bPlayerFound ? 1 : 0,
						bCameraFound ? 1 : 0,
						pxData != nullptr ? 1 : 0,
						bCameraIsMain ? 1 : 0,
						bCameraTargetsPlayer ? 1 : 0,
						bMovementEnabled ? 1 : 0,
						(double)xState.GetFadeAlpha());
					return false;
				}
				g_bProfLabFadeInSeen |= xState.GetFadeAlpha() < 1.0f;
			}

			// pxData is part of the handover condition on purpose. VerifyArrival
			// opens with the wrong-scene clause, and that clause reads the
			// scene's entity table -- so entering with a null table would
			// report "this is not ProfLab" when the truth is "the scene was not
			// resolvable yet". Waiting instead costs a frame and keeps the
			// primary mutation's diagnosis honest.
			if (xState.GetTransitionState() == ZM_WARP_TRANSITION_IDLE
				&& pxData != nullptr)
			{
				g_eProfLabPhase = ProfLabPhase::VerifyArrival;
				g_iProfLabPhaseFrames = 0;
				return true;
			}
		}
		else if (iActiveBuildIndex > 0)
		{
			// A THIRD scene became active. Only a positive index counts:
			// GetSceneInfo hands back a default-constructed snapshot
			// (m_iBuildIndex == -1) for an invalid/stale/unloaded handle, which
			// a SINGLE load can transiently present, and 0 is the FrontEnd this
			// warp departs from. Treating either as "wrong scene" would make
			// this a flake rather than a gate.
			FailProfLabDetailed(
				"an unexpected scene became active during the ProfLab warp "
				"[activeBuildIndex=%d expected=%d state=%s issuedLoads=%u]",
				iActiveBuildIndex, iTargetBuildIndex,
				TransitionStateName(xState.GetTransitionState()),
				xState.GetIssuedLoadRequestCount());
			return false;
		}

		if (g_iProfLabPhaseFrames > iPROFLAB_WARP_IN_DEADLINE)
		{
			FailProfLabDetailed(
				"%s [captured after %d frames: state=%s activeBuildIndex=%d "
				"expectedBuildIndex=%d targetIndex=%u targetTag='%s' "
				"issuedLoads=%u fadeAlpha=%.5f]",
				szPROFLAB_WARP_IN_DEADLINE_FAILURE,
				g_iProfLabPhaseFrames,
				TransitionStateName(xState.GetTransitionState()),
				iActiveBuildIndex, iTargetBuildIndex,
				xState.GetTargetBuildIndex(),
				xState.GetTargetSpawnTag(),
				xState.GetIssuedLoadRequestCount(),
				(double)xState.GetFadeAlpha());
			AppendBarrierLatches(g_aszProfLabFailureDetail,
				sizeof(g_aszProfLabFailureDetail));
			return false;
		}
		return true;
	}

	// ---- CLAUSE I3's body: the authored professor ---------------------------
	//
	// Split out as its own function under the ZM-D-141 stack-frame rule: at /Od a
	// monolithic Step frame is the SUM of every local in every clause, and the
	// measurements below add a second transform pair, two raycast results and two
	// quaternions to a function that already carries several.
	//
	// ★ THE BODY'S SIZE IS MEASURED, NOT READ. Zenith_ColliderComponent exposes the
	// volume TYPE and the rigid-body type but no accessor for the explicit box
	// half-extents ZM_GreyboxVisual installs from the compiled body contract, so the
	// only way to observe the shape the game actually gave him is to probe it. Two
	// axis-aligned casts do that: one along +X into his side at body-centre height,
	// one straight down onto the crown of his head. Both origins sit in open air --
	// nothing in this room stands between them and him -- and each is required to
	// hit HIS entity, so a miss or a hit on the shell is a named failure rather than
	// a wrong number.
	bool ProfLabPhaseVerifyProfessor(Zenith_SceneData* pxData)
	{
		const char* const szAsterName = szZM_PROFLAB_ASTER_ENTITY_NAME;
		Zenith_Entity xAster = pxData != nullptr
			? pxData->FindEntityByName(szAsterName) : Zenith_Entity();
		if (!xAster.IsValid())
		{
			FailProfLabDetailed(
				"ProfLab's committed scene bytes carry no entity named '%s'. The "
				"interior authors exactly one inhabitant -- Professor Aster -- so "
				"either the authoring step is missing from the ProfLab block in "
				"Zenithmon.cpp, or the scene was never re-authored by a *_True "
				"tools boot after that step landed and the shipped lab is empty",
				szAsterName);
			return false;
		}

		// ---- his transform: the authored anchor, and the authored scale ------
		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Vector3 xScale(0.0f);
		Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
		Zenith_TransformComponent& xTransform =
			xAster.GetComponent<Zenith_TransformComponent>();
		xTransform.GetPosition(xPosition);
		xTransform.GetScale(xScale);
		xTransform.GetRotation(xRotation);

		const Zenith_Maths::Vector3 xExpectedCentre = ZM_GetProfLabAsterCenter();
		if (glm::length(xPosition - xExpectedCentre) > fPOSITION_EPSILON)
		{
			FailProfLabDetailed(
				"Professor Aster stands at (%.5f, %.5f, %.5f) in the loaded scene "
				"but Source/World/ZM_ProfLabPlacement.h derives (%.5f, %.5f, %.5f) "
				"[delta %.6f against epsilon %.6f]. A MIRRORED x is the mutation "
				"this catches and nothing else in the gate can: the pure units "
				"read the same header the authoring writes from, so both sides "
				"move together and stay green while the professor is authored into "
				"the other half of the room, outside the arrival frame",
				(double)xPosition.x, (double)xPosition.y, (double)xPosition.z,
				(double)xExpectedCentre.x, (double)xExpectedCentre.y,
				(double)xExpectedCentre.z,
				(double)glm::length(xPosition - xExpectedCentre),
				(double)fPOSITION_EPSILON);
			return false;
		}

		const Zenith_Maths::Vector3 xExpectedScale(fZM_HUMAN_VISUAL_SCALE);
		if (glm::length(xScale - xExpectedScale) > fPOSITION_EPSILON)
		{
			FailProfLabDetailed(
				"Professor Aster is authored at scale (%.5f, %.5f, %.5f), not the "
				"one human visual scale (%.5f) every human in this game wears "
				"[delta %.6f]. That scale exists to land ZM_HumanGen's measured "
				"bind space on the body contract, so any other value draws him at "
				"the wrong size beside a correctly sized player",
				(double)xScale.x, (double)xScale.y, (double)xScale.z,
				(double)fZM_HUMAN_VISUAL_SCALE,
				(double)glm::length(xScale - xExpectedScale));
			return false;
		}

		// ---- his rotation: HE FACES THE PLAYER (SC-E) -------------------------
		//
		// ★★ THIS IS A DOT PRODUCT AND NOT A BIT COMPARISON, DELIBERATELY. Both
		// this clause and the authoring would read ZM_ProfLabAsterFacing(), so a
		// clause that compared the live quaternion against that constant could not
		// see the constant move -- the self-referential-guard failure this repo has
		// already paid for twice. The property worth holding here is "the professor
		// is turned toward the person who just walked in", so that is what is
		// measured: his LIVE forward, through the SHIPPED ZM_ForwardFromRotation
		// (the same function the interact picker uses), dotted against the bearing
		// to the arrival pose.
		//
		// ★ AND IT IS A LIVE-SCENE CLAIM THE BOOT UNIT CANNOT MAKE.
		// ProfLab_AsterFacingIsTurnedTowardTheArrival runs the same arithmetic over
		// the COMPILED constant; this runs it over the transform that came out of
		// the committed .zscen. The mutation between them is real and has happened
		// before on this exact property: author him with COLLISION_VOLUME_TYPE_AABB
		// and Zenith_ColliderComponent forces his Jolt body to identity, the
		// physics->transform sync writes that identity back into the SAVED BYTES,
		// and every pure unit stays green while the shipped professor faces the
		// wall again (ZM-D-156, paid for once on rival Vesper).
		//
		// The floors are the boot unit's, restated here rather than shared because
		// the two files may not include one another; both are derived in the block
		// on ProfLab_AsterFacingIsTurnedTowardTheArrival in
		// Tests/ZM_Tests_ProfLabPlacement.cpp. The pre-SC-E identity value scores
		// the NEGATIVES of today's dots, so both clauses red on it.
		constexpr float fASTER_FACES_PLAYER_MIN_DOT = 0.20f;
		constexpr float fASTER_FACES_CAMERA_MIN_DOT = 0.70f;

		const Zenith_Maths::Vector3 xAsterForward =
			ZM_ForwardFromRotation(xRotation);
		const Zenith_Maths::Vector3 xToArrival =
			ZM_FlattenXZ(ZM_GetProfLabArrivalPivot() - xPosition);
		const Zenith_Maths::Vector3 xToCamera =
			ZM_FlattenXZ(ZM_GetProfLabSettledCameraPosition() - xPosition);
		const float fArrivalDot =
			xAsterForward.x * xToArrival.x + xAsterForward.z * xToArrival.z;
		const float fCameraDot =
			xAsterForward.x * xToCamera.x + xAsterForward.z * xToCamera.z;

		if (fArrivalDot <= fASTER_FACES_PLAYER_MIN_DOT
			|| fCameraDot <= fASTER_FACES_CAMERA_MIN_DOT)
		{
			FailProfLabDetailed(
				"Professor Aster's LIVE rotation (x %.6f y %.6f z %.6f w %.6f) "
				"faces (%.5f, %.5f) in XZ, which dots %.5f with the direction to "
				"the arrival point (floor %.2f) and %.5f with the direction to the "
				"pose the follow camera settles at (floor %.2f). A NEGATIVE dot "
				"means the shipped scene greets the player with the professor's "
				"back turned -- the exact defect SC-E fixed. FIRST SUSPECT: his "
				"collider was changed back to COLLISION_VOLUME_TYPE_AABB, which is "
				"axis-aligned BY DEFINITION and forces its Jolt body to identity; "
				"the physics->transform sync then writes that identity over the "
				"authored facing and into the saved bytes, with every pure unit "
				"still green. SECOND SUSPECT: the scene was never re-authored after "
				"ZM_ProfLabAsterFacing() landed",
				(double)xRotation.x, (double)xRotation.y, (double)xRotation.z,
				(double)xRotation.w, (double)xAsterForward.x,
				(double)xAsterForward.z, (double)fArrivalDot,
				(double)fASTER_FACES_PLAYER_MIN_DOT, (double)fCameraDot,
				(double)fASTER_FACES_CAMERA_MIN_DOT);
			return false;
		}

		// ...and the quaternion that came off disk is still UNIT. Jolt normalises
		// the body's rotation, so a non-unit authored value would make the saved
		// bytes and the compiled constant disagree -- a ZM-D-179-shaped drift with
		// a different cause, and one the dot products above are blind to because a
		// scaled quaternion still rotates to the same direction.
		const float fRotationLengthSquared =
			xRotation.x * xRotation.x + xRotation.y * xRotation.y
			+ xRotation.z * xRotation.z + xRotation.w * xRotation.w;
		if (std::fabs(fRotationLengthSquared - 1.0f) > fYAW_EPSILON)
		{
			FailProfLabDetailed(
				"Professor Aster's committed rotation has squared length %.7f, not "
				"1 [epsilon %.6f]. The authored value is a frozen unit quaternion; "
				"a non-unit one on disk means something rewrote it between the "
				"authoring step and the save",
				(double)fRotationLengthSquared, (double)fYAW_EPSILON);
			return false;
		}

		// ---- his body: the SHAPE, the body class, and its measured size ------
		Zenith_ColliderComponent* pxCollider =
			xAster.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr || !pxCollider->HasValidBody())
		{
			FailProfLabDetailed(
				"Professor Aster carries no collider with a live body "
				"[collider=%d body=%d]. A talker the player can walk THROUGH ends "
				"the walk-up inside him rather than in contact",
				pxCollider != nullptr ? 1 : 0,
				pxCollider != nullptr && pxCollider->HasValidBody() ? 1 : 0);
			return false;
		}
		// ★ OBB, AND **AABB IS NOW FORBIDDEN ON HIM FOREVER** (SC-E / ZM-D-156).
		// He was AABB while he was authored at identity, and that was correct: an
		// AABB destroys an authored rotation, and there was none to destroy. Now
		// there is. Zenith_ColliderComponent's body creation reads
		//     const JPH::Quat xJoltRot = (eVolumeType == COLLISION_VOLUME_TYPE_AABB)
		//         ? JPH::Quat::sIdentity() : JPH::Quat(...);
		// so an AABB would force his body to identity and the physics->transform
		// sync would write that identity straight back over his facing and into the
		// SAVED BYTES -- with every pure unit green, because they read the compiled
		// constants and the damage lives in the file. OBB is the same box shape and
		// differs ONLY in applying the rotation. He is still STATIC, and
		// deliberately not the rival's dynamic capsule: that shape exists because
		// the rival WALKS, and a dynamic body could be shoved off the anchor every
		// clearance figure in Source/World/ZM_ProfLabPlacement.h is derived at.
		if (pxCollider->GetCollisionVolumeType() != COLLISION_VOLUME_TYPE_OBB
			|| pxCollider->GetRigidBodyType() != RIGIDBODY_TYPE_STATIC)
		{
			FailProfLabDetailed(
				"Professor Aster's body is volumeType=%d rigidBodyType=%d, not the "
				"authored COLLISION_VOLUME_TYPE_OBB (%d) + RIGIDBODY_TYPE_STATIC "
				"(%d). If this reads AABB (%d), that is the ZM-D-156 defect and it "
				"has ALREADY silently wiped his authored facing out of the saved "
				"bytes -- an AABB is axis-aligned by definition, so its Jolt body is "
				"forced to identity and the physics->transform sync writes that "
				"identity back. A SPHERE would make his footprint round where the "
				"walk-up standoff is measured against a box, and a DYNAMIC body "
				"could be shoved off the anchor every clearance figure in "
				"Source/World/ZM_ProfLabPlacement.h is derived at",
				(int)pxCollider->GetCollisionVolumeType(),
				(int)pxCollider->GetRigidBodyType(),
				(int)COLLISION_VOLUME_TYPE_OBB, (int)RIGIDBODY_TYPE_STATIC,
				(int)COLLISION_VOLUME_TYPE_AABB);
			return false;
		}

		const float fExpectedHalfWidth = fZM_HUMAN_BODY_FOOTPRINT * 0.5f;
		const float fExpectedHalfHeight = fZM_HUMAN_BODY_HALF_HEIGHT;
		const float fPROBE_STANDOFF = 3.0f;   // open air on both probe axes in this room

		const Zenith_Physics::RaycastResult xSideHit = g_xEngine.Physics().Raycast(
			xExpectedCentre - Zenith_Maths::Vector3(fPROBE_STANDOFF, 0.0f, 0.0f),
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), fPROBE_STANDOFF);
		const Zenith_Physics::RaycastResult xCrownHit = g_xEngine.Physics().Raycast(
			xExpectedCentre + Zenith_Maths::Vector3(0.0f, fPROBE_STANDOFF, 0.0f),
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), fPROBE_STANDOFF);
		if (!xSideHit.m_bHit || xSideHit.m_xHitEntity != xAster.GetEntityID()
			|| !xCrownHit.m_bHit || xCrownHit.m_xHitEntity != xAster.GetEntityID())
		{
			FailProfLabDetailed(
				"the probes that measure Professor Aster's body did not both land "
				"on him [side hit=%d entityMatch=%d dist=%.4f; crown hit=%d "
				"entityMatch=%d dist=%.4f]. Either his body is not where his "
				"transform is, or something else in this room is now standing "
				"between the probe origins and him",
				xSideHit.m_bHit ? 1 : 0,
				xSideHit.m_xHitEntity == xAster.GetEntityID() ? 1 : 0,
				(double)xSideHit.m_fDistance,
				xCrownHit.m_bHit ? 1 : 0,
				xCrownHit.m_xHitEntity == xAster.GetEntityID() ? 1 : 0,
				(double)xCrownHit.m_fDistance);
			return false;
		}

		const float fMeasuredHalfWidth = fPROBE_STANDOFF - xSideHit.m_fDistance;
		const float fMeasuredHalfHeight = fPROBE_STANDOFF - xCrownHit.m_fDistance;
		if (std::fabs(fMeasuredHalfWidth - fExpectedHalfWidth) > fBODY_PROBE_EPSILON
			|| std::fabs(fMeasuredHalfHeight - fExpectedHalfHeight)
				> fBODY_PROBE_EPSILON)
		{
			FailProfLabDetailed(
				"Professor Aster's body measures %.4f m half-width and %.4f m "
				"half-height, against the compiled ZM_HumanBody contract's %.4f / "
				"%.4f [epsilon %.4f]. The body is supposed to come from that "
				"contract explicitly and NEVER from the transform scale -- a "
				"scale-derived box would measure %.4f here -- so this says "
				"ZM_GreyboxVisual::InstallHumanBody did not reach him",
				(double)fMeasuredHalfWidth, (double)fMeasuredHalfHeight,
				(double)fExpectedHalfWidth, (double)fExpectedHalfHeight,
				(double)fBODY_PROBE_EPSILON,
				(double)(fZM_HUMAN_VISUAL_SCALE * 0.5f));
			return false;
		}

		// ---- and he is TALKABLE, as the professor -----------------------------
		ZM_Interactable* pxInteractable = xAster.TryGetComponent<ZM_Interactable>();
		if (pxInteractable == nullptr)
		{
			FailProfLabDetailed(
				"Professor Aster carries no ZM_Interactable. He is a TALKER row "
				"(ZM_NPC_PROF_ASTER in Source/Data/ZM_NpcData.cpp) standing in a "
				"room whose only purpose is to be talked to in -- without the "
				"component the picker never sees him and the interact press does "
				"nothing, forever, silently");
			return false;
		}
		if (pxInteractable->GetNpcId() != ZM_NPC_PROF_ASTER
			|| !pxInteractable->IsInteractable())
		{
			FailProfLabDetailed(
				"Professor Aster's ZM_Interactable resolves to NPC row %u "
				"(expected ZM_NPC_PROF_ASTER = %u) and reports interactable=%d. "
				"SetNpcId fails CLOSED by clearing the candidacy flag, so a bad "
				"row id authors a MUTE NPC rather than a loud failure",
				(u_int)pxInteractable->GetNpcId(), (u_int)ZM_NPC_PROF_ASTER,
				pxInteractable->IsInteractable() ? 1 : 0);
			return false;
		}
		if (std::fabs(pxInteractable->GetRadius() - fZM_PROFLAB_ASTER_REACH_BONUS)
			> fPOSITION_EPSILON)
		{
			FailProfLabDetailed(
				"Professor Aster's authored interact radius is %.5f, not the "
				"%.5f m the placement header states his reach bonus to be. The "
				"boot unit ProfLab_AsterStandsOutsideArrivalReachInsideTheShell "
				"proves he is out of reach on arrival USING that figure, so a "
				"live radius that disagrees with it makes that proof about a "
				"different NPC than the one the scene ships",
				(double)pxInteractable->GetRadius(),
				(double)fZM_PROFLAB_ASTER_REACH_BONUS);
			return false;
		}

		return true;
	}

	// ---- CLAUSE I4's body: the exit sensor, in the LOADED scene -------------
	//
	// ★★ THIS IS THE ONLY CI-VISIBLE PROOF THAT THE LAB DOOR GOES ANYWHERE, AND
	// THE SLICE HAD NONE BY DEFAULT. Everything else that could check the seam is
	// blind to the mutations that matter:
	//   * every pure unit reads the compiled constants the AUTHORING reads, so both
	//     sides move together -- delete the AddStep_Custom(&ZM_ConfigureProfLabExit
	//     Trigger) call outright and not one of them changes colour;
	//   * the round-trip walk (ZM_LabRoundTrip_Test) DOES drive the real door, but
	//     it needs the GITIGNORED Dawnmere terrain bake and RequestSkips without it
	//     on CI -- and a skip counts as a PASS;
	//   * the committed-bytes needle proves a STRING is in Dawnmere.zscen, which
	//     says nothing about what THIS scene's sensor is pointed at.
	// This file is m_bRequiresGraphics=false, calls RequestSkip NOWHERE (see the
	// ZM-D-147 deviation in the header) and has no terrain dependency, so it runs
	// on every Null CI boot. That is why the clause lives here.
	//
	// ★ EVERY EXPECTED VALUE IS RESOLVED, NEVER SPELLED. The target build index
	// comes from ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex and the tag from
	// ZM_GetProfLabExitSpawnTag() -- the same two reads the authoring makes. A
	// literal 2 or "FromLab" here would be a second inventory, and the specific
	// mutation "the configure function writes the PlayerHome build index instead"
	// would then need BOTH sites edited to stay green, which is the property that
	// makes this clause worth having.
	//
	// Split into its own function under the ZM-D-141 stack-frame rule: a monolithic
	// /Od Step frame is the SUM of every local in every clause.
	bool ProfLabPhaseVerifyExitSensor(Zenith_SceneData* pxData)
	{
		const char* const szSensorName = szZM_PROFLAB_EXIT_TRIGGER_ENTITY_NAME;
		Zenith_Entity xSensor = pxData != nullptr
			? pxData->FindEntityByName(szSensorName) : Zenith_Entity();
		if (!xSensor.IsValid())
		{
			FailProfLabDetailed(
				"ProfLab's committed scene bytes carry no entity named '%s'. This "
				"room's ONLY way out is missing: a player who warps in here can "
				"never leave, and no other check in the gate can see it -- the pure "
				"units read the same header the authoring writes from. Either the "
				"authoring block in Zenithmon.cpp lost its exit-sensor steps, or the "
				"scene was never re-authored by a *_True tools boot after they "
				"landed", szSensorName);
			return false;
		}

		// (I4a) THE VOLUME, against the compiled accessor. A sensor that no longer
		//       spans the aperture is one a player can walk around, out through the
		//       gap and into open space.
		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Vector3 xScale(0.0f);
		Zenith_TransformComponent& xTransform =
			xSensor.GetComponent<Zenith_TransformComponent>();
		xTransform.GetPosition(xPosition);
		xTransform.GetScale(xScale);

		const ZM_ProfLabBlockout xExpected = ZM_GetProfLabExitTrigger();
		if (glm::length(xPosition - xExpected.m_xCenter) > fPOSITION_EPSILON
			|| glm::length(xScale - xExpected.m_xScale) > fPOSITION_EPSILON)
		{
			FailProfLabDetailed(
				"ProfLab's committed scene bytes disagree with "
				"Source/World/ZM_ProfLabPlacement.h at the exit sensor '%s' -- the "
				"scene was not re-authored after the header changed [scene centre "
				"(%.5f, %.5f, %.5f) vs ZM_GetProfLabExitTrigger() (%.5f, %.5f, "
				"%.5f); scene scale (%.5f, %.5f, %.5f) vs header (%.5f, %.5f, "
				"%.5f); centre delta %.6f scale delta %.6f against epsilon %.6f]",
				szSensorName,
				(double)xPosition.x, (double)xPosition.y, (double)xPosition.z,
				(double)xExpected.m_xCenter.x, (double)xExpected.m_xCenter.y,
				(double)xExpected.m_xCenter.z,
				(double)xScale.x, (double)xScale.y, (double)xScale.z,
				(double)xExpected.m_xScale.x, (double)xExpected.m_xScale.y,
				(double)xExpected.m_xScale.z,
				(double)glm::length(xPosition - xExpected.m_xCenter),
				(double)glm::length(xScale - xExpected.m_xScale),
				(double)fPOSITION_EPSILON);
			return false;
		}

		// (I4b) A LIVE BODY of the authored class. A ZM_WarpTrigger with no body
		//       never receives OnCollisionEnter, so the door is decorative.
		Zenith_ColliderComponent* pxCollider =
			xSensor.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr || !pxCollider->HasValidBody()
			|| pxCollider->GetCollisionVolumeType() != COLLISION_VOLUME_TYPE_AABB
			|| pxCollider->GetRigidBodyType() != RIGIDBODY_TYPE_STATIC)
		{
			FailProfLabDetailed(
				"ProfLab's exit sensor '%s' carries collider=%d liveBody=%d "
				"volumeType=%d rigidBodyType=%d, not a live "
				"COLLISION_VOLUME_TYPE_AABB (%d) + RIGIDBODY_TYPE_STATIC (%d). "
				"ZM_WarpTrigger fires from OnCollisionEnter, so a sensor with no "
				"body is a door that never opens -- and unlike a missing entity, "
				"that failure is completely silent",
				szSensorName, pxCollider != nullptr ? 1 : 0,
				pxCollider != nullptr && pxCollider->HasValidBody() ? 1 : 0,
				pxCollider != nullptr
					? (int)pxCollider->GetCollisionVolumeType() : -1,
				pxCollider != nullptr ? (int)pxCollider->GetRigidBodyType() : -1,
				(int)COLLISION_VOLUME_TYPE_AABB, (int)RIGIDBODY_TYPE_STATIC);
			return false;
		}

		// (I4c) ★ WHERE IT ACTUALLY GOES. The mutation this exists for is a
		//       configure function that wrote SOME OTHER build index -- PlayerHome's
		//       is the obvious slip, since the shipped Home pair three functions
		//       above still spells 40u and 2u as literals. Every unit stays green,
		//       the round trip skips on CI, and the shipped game's lab door drops
		//       the player into somebody's bedroom.
		ZM_WarpTrigger* pxWarp = xSensor.TryGetComponent<ZM_WarpTrigger>();
		if (pxWarp == nullptr)
		{
			FailProfLabDetailed(
				"ProfLab's exit sensor '%s' carries no ZM_WarpTrigger -- it is a "
				"solid box in a doorway, and walking into it does nothing at all",
				szSensorName);
			return false;
		}

		const u_int uExpectedBuildIndex =
			ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex;
		if (pxWarp->GetTargetBuildIndex() != uExpectedBuildIndex)
		{
			FailProfLabDetailed(
				"ProfLab's exit sensor '%s' targets build index %u, but the "
				"compiled world table puts Dawnmere (ZM_SCENE_DAWNMERE) at %u. "
				"PlayerHome is at %u and the FrontEnd at %u, for comparison. The "
				"lab door leads somewhere other than the town it stands in, and no "
				"pure unit can see this -- they read the same constants the "
				"authoring does",
				szSensorName, pxWarp->GetTargetBuildIndex(), uExpectedBuildIndex,
				ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME).m_uBuildIndex,
				ZM_GetWorldSpec(ZM_SCENE_FRONTEND).m_uBuildIndex);
			return false;
		}

		const char* const szExpectedTag = ZM_GetProfLabExitSpawnTag();
		if (szExpectedTag == nullptr || szExpectedTag[0] == '\0'
			|| std::strcmp(pxWarp->GetSpawnTag(), szExpectedTag) != 0)
		{
			FailProfLabDetailed(
				"ProfLab's exit sensor '%s' asks Dawnmere for spawn tag '%s', not "
				"the '%s' its ZM_SCENE_PROFLAB->ZM_SCENE_DAWNMERE connection "
				"declares. ZM_GameStateManager::IsWarpDestinationValid consults ONLY "
				"the compiled tag list, so a tag Dawnmere does not offer is REJECTED "
				"(a silent no-op door) while a tag it offers but no marker carries "
				"is ACCEPTED and parks the machine in "
				"ZM_WARP_TRANSITION_WAITING_FOR_SPAWN, which has NO timeout -- an "
				"opaque fade and a frozen player, forever",
				szSensorName, pxWarp->GetSpawnTag(),
				szExpectedTag != nullptr ? szExpectedTag : "<unresolved>");
			return false;
		}

		// (I4d) ...and it is NOT already latched on the arrival tick. A sensor that
		//       contains the arrival point fires immediately and bounces the player
		//       back and forth between the two scenes forever with no input
		//       accepted. The boot unit
		//       ProfLab_ExitSensorFillsTheApertureAndClearsTheArrivingBody proves
		//       the GEOMETRY leaves clearance; this proves the live body agrees.
		if (pxWarp->IsLatched())
		{
			FailProfLabDetailed(
				"ProfLab's exit sensor '%s' is ALREADY latched on the arrival tick "
				"-- the arriving player's capsule overlaps it, so the warp fires "
				"immediately and the player bounces between Dawnmere and ProfLab "
				"forever, frozen, behind a fade. The header's arrival clearance "
				"claims %.4f m; something has moved the sensor or the marker",
				szSensorName,
				(double)ZM_GetProfLabExitTriggerArrivalClearance());
			return false;
		}

		return true;
	}

	// ---- PHASE 3: the arrival battery, clauses A..I ------------------------

	bool ProfLabPhaseVerifyArrival()
	{
		const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
		const int iActiveBuildIndex =
			g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;

		// ---- CLAUSE A: is this actually ProfLab? --------------------------
		//
		// FIRST, and HARD (no retry), because it is the clause the primary
		// mutation must trip. Re-point the registration at another interior and
		// a warp still completes cleanly: clauses B..H all stay GREEN and only
		// the CONTENT is wrong. Every shell entity exists from the moment the
		// scene finishes loading -- long before the machine reaches IDLE, which
		// is the only way this phase is entered -- so there is nothing here for
		// a retry to wait on, and a retry would convert the mutation's
		// self-naming failure into an anonymous timeout.
		const char* szFloorName =
			ZM_GetProfLabBlockName(ZM_PROFLAB_BLOCK_FLOOR);
		Zenith_Entity xFloor = pxData != nullptr
			? pxData->FindEntityByName(szFloorName) : Zenith_Entity();
		if (!xFloor.IsValid())
		{
			FailProfLabDetailed(
				"%s [looked for entity '%s' in the scene now active at build "
				"index %d; sceneData=%d]",
				szPROFLAB_WRONG_SCENE_FAILURE, szFloorName, iActiveBuildIndex,
				pxData != nullptr ? 1 : 0);
			return false;
		}
		{
			const ZM_ProfLabBlockout xFloorBlock =
				ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);
			Zenith_Maths::Vector3 xFloorPosition(0.0f);
			Zenith_Maths::Vector3 xFloorScale(0.0f);
			Zenith_TransformComponent& xFloorTransform =
				xFloor.GetComponent<Zenith_TransformComponent>();
			xFloorTransform.GetPosition(xFloorPosition);
			xFloorTransform.GetScale(xFloorScale);
			if (glm::length(xFloorPosition - xFloorBlock.m_xCenter) > fPOSITION_EPSILON
				|| glm::length(xFloorScale - xFloorBlock.m_xScale) > fPOSITION_EPSILON)
			{
				FailProfLabDetailed(
					"%s [entity '%s' exists but sits at centre "
					"(%.5f, %.5f, %.5f) scale (%.5f, %.5f, %.5f); the header "
					"says centre (%.5f, %.5f, %.5f) scale (%.5f, %.5f, %.5f)]",
					szPROFLAB_WRONG_SCENE_FAILURE, szFloorName,
					(double)xFloorPosition.x, (double)xFloorPosition.y,
					(double)xFloorPosition.z,
					(double)xFloorScale.x, (double)xFloorScale.y,
					(double)xFloorScale.z,
					(double)xFloorBlock.m_xCenter.x, (double)xFloorBlock.m_xCenter.y,
					(double)xFloorBlock.m_xCenter.z,
					(double)xFloorBlock.m_xScale.x, (double)xFloorBlock.m_xScale.y,
					(double)xFloorBlock.m_xScale.z);
				return false;
			}
		}

		// ---- Readiness gate (retry-with-deadline) -------------------------
		//
		// Deliberately AFTER clause A. These are the handles every remaining
		// clause dereferences; a genuinely slow settle gets frames, and a
		// wrong scene has already been named above.
		ManagerView xManager;
		PlayerView xPlayer;
		CameraView xCamera;
		Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
		Zenith_Entity xSpawn;
		const char* szSpawnTag = ProfLabSpawnTag();
		const ZM_SPAWN_POINT_LOOKUP_RESULT eSpawnResult =
			ZM_SpawnPoint::FindUniqueInScene(xActive, szSpawnTag, xSpawnID);
		const bool bManagerReady = FindUniqueManager(xManager);
		const bool bPlayerReady = FindUniqueActivePlayer(xPlayer);
		const bool bCameraReady = FindUniqueActiveCamera(xCamera);
		if (!bManagerReady || !bPlayerReady || !bCameraReady
			|| pxData == nullptr
			|| eSpawnResult != ZM_SPAWN_POINT_LOOKUP_FOUND
			|| !(xSpawn = g_xEngine.Scenes().ResolveEntity(xSpawnID)).IsValid()
			|| !xPlayer.m_pxCollider->HasValidBody())
		{
			if (g_iProfLabPhaseFrames > iPROFLAB_ARRIVAL_DEADLINE)
			{
				FailProfLabDetailed(
					"the ProfLab arrival state never settled in %d frames "
					"[uniqueManager=%d uniquePlayer=%d uniqueCamera=%d "
					"sceneData=%d spawnLookup('%s')=%s spawnEntityValid=%d "
					"playerBody=%d]",
					iPROFLAB_ARRIVAL_DEADLINE,
					bManagerReady ? 1 : 0, bPlayerReady ? 1 : 0,
					bCameraReady ? 1 : 0, pxData != nullptr ? 1 : 0,
					szSpawnTag, SpawnLookupResultName(eSpawnResult),
					xSpawn.IsValid() ? 1 : 0,
					xPlayer.m_pxCollider != nullptr
						&& xPlayer.m_pxCollider->HasValidBody() ? 1 : 0);
				return false;
			}
			return true;
		}

		ZM_GameStateManager& xState = *xManager.m_pxManager;

		// ---- CLAUSE B: the player stands on the marker, at rest -----------
		//
		// Spawn MARKERS store FEET; bodies store CENTRES. CalculateSpawnCenter
		// is the only place in the game those two conventions meet, so it is
		// the only thing this clause is allowed to compare against -- adding a
		// half-extent here by hand would re-implement the very function under
		// test.
		Zenith_Maths::Vector3 xMarkerFeet(0.0f);
		xSpawn.GetComponent<Zenith_TransformComponent>().GetPosition(xMarkerFeet);
		const Zenith_Maths::Vector3 xExpectedCenter =
			ZM_GameStateManager::CalculateSpawnCenter(xMarkerFeet);
		const Zenith_PhysicsBodyID xBody = xPlayer.m_pxCollider->GetBodyID();
		const Zenith_Maths::Vector3 xBodyPosition =
			g_xEngine.Physics().GetBodyPosition(xBody);
		const Zenith_Maths::Vector3 xLinearVelocity =
			g_xEngine.Physics().GetLinearVelocity(xBody);
		const Zenith_Maths::Vector3 xAngularVelocity =
			g_xEngine.Physics().GetAngularVelocity(xBody);
		if (glm::length(xPlayer.m_xPosition - xExpectedCenter) > fPOSITION_EPSILON
			|| glm::length(xBodyPosition - xExpectedCenter) > fPOSITION_EPSILON
			|| glm::length(xLinearVelocity) > fVELOCITY_EPSILON
			|| glm::length(xAngularVelocity) > fVELOCITY_EPSILON
			|| xPlayer.m_pxController->GetRequestedSpeed() != 0.0f
			|| glm::length(xPlayer.m_pxController->GetMoveDirection()) > fVELOCITY_EPSILON)
		{
			FailProfLabDetailed(
				"the ProfLab arrival did not place the player on its '%s' "
				"marker at rest [markerFeet (%.5f, %.5f, %.5f) scale "
				"(%.5f, %.5f, %.5f) -> expected centre (%.5f, %.5f, %.5f); "
				"transform (%.5f, %.5f, %.5f) body (%.5f, %.5f, %.5f); "
				"linVel %.6f angVel %.6f requestedSpeed %.5f moveDir %.6f]",
				szSpawnTag,
				(double)xMarkerFeet.x, (double)xMarkerFeet.y, (double)xMarkerFeet.z,
				(double)xPlayer.m_xScale.x, (double)xPlayer.m_xScale.y,
				(double)xPlayer.m_xScale.z,
				(double)xExpectedCenter.x, (double)xExpectedCenter.y,
				(double)xExpectedCenter.z,
				(double)xPlayer.m_xPosition.x, (double)xPlayer.m_xPosition.y,
				(double)xPlayer.m_xPosition.z,
				(double)xBodyPosition.x, (double)xBodyPosition.y,
				(double)xBodyPosition.z,
				(double)glm::length(xLinearVelocity),
				(double)glm::length(xAngularVelocity),
				(double)xPlayer.m_pxController->GetRequestedSpeed(),
				(double)glm::length(xPlayer.m_pxController->GetMoveDirection()));
			return false;
		}

		// ---- CLAUSE C: camera is main + aimed, control handed back --------
		if (xState.GetTransitionState() != ZM_WARP_TRANSITION_IDLE
			|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
			|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
			|| !xPlayer.m_pxController->IsMovementEnabled()
			|| xState.GetFadeAlpha() != 0.0f
			|| !ManagerFadeOverlayIsHidden(xManager)
			|| g_xEngine.Scenes().ResolveEntity(xPlayer.m_xEntityID).GetScene() != xActive
			|| g_xEngine.Scenes().ResolveEntity(xCamera.m_xEntityID).GetScene() != xActive)
		{
			FailProfLabDetailed(
				"ProfLab settled without handing control back cleanly "
				"[state=%s cameraIsMain=%d cameraTargetsPlayer=%d "
				"movementEnabled=%d fadeAlpha=%.5f overlayHidden=%d "
				"playerInActiveScene=%d cameraInActiveScene=%d]",
				TransitionStateName(xState.GetTransitionState()),
				pxData->GetMainCameraEntity() == xCamera.m_xEntityID ? 1 : 0,
				xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID ? 1 : 0,
				xPlayer.m_pxController->IsMovementEnabled() ? 1 : 0,
				(double)xState.GetFadeAlpha(),
				ManagerFadeOverlayIsHidden(xManager) ? 1 : 0,
				g_xEngine.Scenes().ResolveEntity(xPlayer.m_xEntityID).GetScene()
					== xActive ? 1 : 0,
				g_xEngine.Scenes().ResolveEntity(xCamera.m_xEntityID).GetScene()
					== xActive ? 1 : 0);
			return false;
		}

		// ---- CLAUSE D: the camera kept the yaw the clearances assume ------
		//
		// Every clearance figure in ZM_ProfLabPlacement.h is derived at this
		// yaw. If the scene authored a different one, the arm sweeps somewhere
		// the header never reasoned about and the clearance units go quiet
		// while the real camera clips the shell.
		if (std::fabs(xCamera.m_pxFollow->GetAuthoredYaw() - fZM_PROFLAB_CAMERA_YAW)
			> fYAW_EPSILON)
		{
			FailProfLabDetailed(
				"the ProfLab camera captured authored yaw %.6f but every "
				"clearance figure in Source/World/ZM_ProfLabPlacement.h is "
				"derived at fZM_PROFLAB_CAMERA_YAW = %.6f",
				(double)xCamera.m_pxFollow->GetAuthoredYaw(),
				(double)fZM_PROFLAB_CAMERA_YAW);
			return false;
		}

		// ---- CLAUSE E: no barrier was skipped -----------------------------
		if (!g_bProfLabFadeOutSeen || !g_bProfLabOpaqueLoadSeen
			|| !g_bProfLabCameraBarrierSeen || !g_bProfLabFadeInSeen)
		{
			FailProfLabDetailed("%s", szPROFLAB_BARRIER_FAILURE);
			AppendBarrierLatches(g_aszProfLabFailureDetail,
				sizeof(g_aszProfLabFailureDetail));
			return false;
		}

		// ---- CLAUSE F: exactly one SINGLE load, target cleared ------------
		if (xState.GetIssuedLoadRequestCount() != 1u
			|| xState.GetTargetBuildIndex() != ZM_GameStateManager::uINVALID_BUILD_INDEX)
		{
			FailProfLabDetailed(
				"the ProfLab warp did not complete as exactly one SINGLE load "
				"with a cleared target [issuedLoads=%u (expected 1) "
				"targetBuildIndex=%u (expected uINVALID_BUILD_INDEX=%u)]",
				xState.GetIssuedLoadRequestCount(),
				xState.GetTargetBuildIndex(),
				ZM_GameStateManager::uINVALID_BUILD_INDEX);
			return false;
		}

		// ---- CLAUSE G: the FrontEnd is gone -------------------------------
		if (g_xProfLabFrontEndScene.IsValid())
		{
			FailProfLabDetailed(
				"the FrontEnd scene survived the SINGLE load into ProfLab -- "
				"its handle is still valid after arrival [activeBuildIndex=%d]",
				iActiveBuildIndex);
			return false;
		}

		// ---- CLAUSE H: an interior owns no terrain grass ------------------
		{
			const u_int uGrassCount =
				g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().Count();
			if (uGrassCount != 0u)
			{
				FailProfLabDetailed(
					"ProfLab is an INTERIOR (ZM_SCENE_KIND_INTERIOR, terrain "
					"set \"\") yet its active scene owns %u ZM_TerrainGrass "
					"component(s)",
					uGrassCount);
				return false;
			}
		}

		// ---- CLAUSE I: the committed bytes agree with the header ----------
		//
		// A bytes-vs-compiled-authoring check, NOT a self-comparison: ProfLab
		// is re-authored by every tools boot including Null/CI, so the two
		// sides are expected to agree and a mismatch is a real regression --
		// the header moved and the scene was never re-authored.
		//
		// I1 is the seven shell blocks; I2 below is the three entities that are
		// NOT shell blocks. Both halves are needed: clause B proves the player
		// stands on whatever marker the scene happens to carry, but it DERIVES
		// its expectation from that marker's own transform, so it cannot see the
		// marker itself drifting away from the header.
		for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
		{
			const ZM_PROFLAB_BLOCK eBlock = (ZM_PROFLAB_BLOCK)u;
			const char* szBlockName = ZM_GetProfLabBlockName(eBlock);
			const ZM_ProfLabBlockout xBlock = ZM_GetProfLabBlock(eBlock);
			Zenith_Entity xEntity = pxData->FindEntityByName(szBlockName);
			if (!xEntity.IsValid())
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at block '%s' -- the "
					"scene was not re-authored after the header changed "
					"[no entity of that name exists in the scene active at "
					"build index %d; block %u of %u]",
					szBlockName, iActiveBuildIndex, u,
					(u_int)ZM_PROFLAB_BLOCK_COUNT);
				return false;
			}

			Zenith_Maths::Vector3 xPosition(0.0f);
			Zenith_Maths::Vector3 xScale(0.0f);
			Zenith_TransformComponent& xTransform =
				xEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.GetPosition(xPosition);
			xTransform.GetScale(xScale);
			if (glm::length(xPosition - xBlock.m_xCenter) > fPOSITION_EPSILON
				|| glm::length(xScale - xBlock.m_xScale) > fPOSITION_EPSILON)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at block '%s' -- the "
					"scene was not re-authored after the header changed "
					"[scene centre (%.5f, %.5f, %.5f) vs header "
					"(%.5f, %.5f, %.5f); scene scale (%.5f, %.5f, %.5f) vs "
					"header (%.5f, %.5f, %.5f); centre delta %.6f scale delta "
					"%.6f against epsilon %.6f]",
					szBlockName,
					(double)xPosition.x, (double)xPosition.y, (double)xPosition.z,
					(double)xBlock.m_xCenter.x, (double)xBlock.m_xCenter.y,
					(double)xBlock.m_xCenter.z,
					(double)xScale.x, (double)xScale.y, (double)xScale.z,
					(double)xBlock.m_xScale.x, (double)xBlock.m_xScale.y,
					(double)xBlock.m_xScale.z,
					(double)glm::length(xPosition - xBlock.m_xCenter),
					(double)glm::length(xScale - xBlock.m_xScale),
					(double)fPOSITION_EPSILON);
				return false;
			}
		}

		// ---- CLAUSE I2: the three authored entities that are not blocks ---
		//
		// Editing fZM_PROFLAB_SPAWN_Z (or the player scale) in the header and
		// not re-authoring the scene is EXACTLY the drift clause I claims to
		// catch, and the shell walk above cannot see it -- the shell does not
		// move when the marker does. Each failure names the offending entity.
		{
			// (I2a) THE ARRIVAL MARKER, by NAME as well as by tag. Nothing at
			//       runtime writes this transform (the marker carries no
			//       collider and no component that moves it), so what is read
			//       here is still the committed bytes. xMarkerFeet was read in
			//       clause B from the TAG-resolved entity, so requiring the
			//       name to resolve to that same entity also pins the two
			//       lookup keys to one another.
			Zenith_Entity xSpawnByName =
				pxData->FindEntityByName(szZM_PROFLAB_SPAWN_ENTITY_NAME);
			const bool bSpawnNamed = xSpawnByName.IsValid();
			if (!bSpawnNamed || xSpawnByName.GetEntityID() != xSpawnID)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at the arrival marker "
					"'%s' -- no entity of that name exists, or it is not the "
					"entity the '%s' spawn tag resolves to [nameFound=%d "
					"sameEntityAsTag=%d]",
					szZM_PROFLAB_SPAWN_ENTITY_NAME, szSpawnTag,
					bSpawnNamed ? 1 : 0,
					bSpawnNamed && xSpawnByName.GetEntityID() == xSpawnID
						? 1 : 0);
				return false;
			}

			const Zenith_Maths::Vector3 xHeaderFeet = ZM_GetProfLabSpawnFeet();
			if (glm::length(xMarkerFeet - xHeaderFeet) > fPOSITION_EPSILON)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at the arrival marker "
					"'%s' -- the scene was not re-authored after the header "
					"changed [scene feet (%.5f, %.5f, %.5f) vs "
					"ZM_GetProfLabSpawnFeet() (%.5f, %.5f, %.5f); delta %.6f "
					"against epsilon %.6f]. Clause B cannot see this: it takes "
					"its expected centre FROM this marker",
					szZM_PROFLAB_SPAWN_ENTITY_NAME,
					(double)xMarkerFeet.x, (double)xMarkerFeet.y,
					(double)xMarkerFeet.z,
					(double)xHeaderFeet.x, (double)xHeaderFeet.y,
					(double)xHeaderFeet.z,
					(double)glm::length(xMarkerFeet - xHeaderFeet),
					(double)fPOSITION_EPSILON);
				return false;
			}

			// (I2b) THE PLAYER'S AUTHORED SCALE. Its POSITION is deliberately
			//       not compared here: the warp teleports the body to
			//       CalculateSpawnCenter on arrival, which is clause B's
			//       property, so the authored centre is no longer observable.
			//       The scale IS still committed bytes, and it is what the
			//       header's doorway width, headroom and capsule half-extent
			//       are all derived from.
			Zenith_Entity xPlayerByName =
				pxData->FindEntityByName(szZM_PROFLAB_PLAYER_ENTITY_NAME);
			const bool bPlayerNamed = xPlayerByName.IsValid();
			if (!bPlayerNamed
				|| xPlayerByName.GetEntityID() != xPlayer.m_xEntityID)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at the player entity "
					"'%s' -- no entity of that name exists, or it is not the "
					"scene's one ZM_PlayerController body [nameFound=%d "
					"sameEntityAsController=%d]",
					szZM_PROFLAB_PLAYER_ENTITY_NAME,
					bPlayerNamed ? 1 : 0,
					bPlayerNamed
						&& xPlayerByName.GetEntityID() == xPlayer.m_xEntityID
						? 1 : 0);
				return false;
			}

			// The authored scale is the human MODEL scale, uniform, and NOT the body
			// box -- the body is installed from the compiled contract at runtime.
			const Zenith_Maths::Vector3 xHeaderScale(fZM_HUMAN_VISUAL_SCALE);
			if (glm::length(xPlayer.m_xScale - xHeaderScale) > fPOSITION_EPSILON)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at the player entity "
					"'%s' -- the scene was not re-authored after the header "
					"changed [scene scale (%.5f, %.5f, %.5f) vs header "
					"(%.5f, %.5f, %.5f); delta %.6f against epsilon %.6f]. "
					"Every clearance figure in that header -- doorway width, "
					"lintel headroom, capsule half-extent -- is derived from "
					"the header's numbers, not the scene's",
					szZM_PROFLAB_PLAYER_ENTITY_NAME,
					(double)xPlayer.m_xScale.x, (double)xPlayer.m_xScale.y,
					(double)xPlayer.m_xScale.z,
					(double)xHeaderScale.x, (double)xHeaderScale.y,
					(double)xHeaderScale.z,
					(double)glm::length(xPlayer.m_xScale - xHeaderScale),
					(double)fPOSITION_EPSILON);
				return false;
			}

			// (I2c) THE CAMERA. Its NEAR and FAR planes are the only authored
			//       camera fields still readable at arrival, and that is a
			//       statement about the component, not a shortcut:
			//       ZM_FollowCamera::OnLateUpdate OVERWRITES the camera
			//       component's position, yaw, pitch and FOV every frame (and
			//       OnStart clears the spring, so the first such frame SNAPS),
			//       long before this phase runs. The authored POSITION is
			//       therefore not observable here at all -- the boot unit
			//       ZM_WorldTraversal/
			//       ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw
			//       clause (5) owns it, against the compiled header -- and the
			//       authored YAW is clause D above, read from the value the
			//       component CAPTURED at OnStart.
			Zenith_Entity xCameraByName =
				pxData->FindEntityByName(szZM_PROFLAB_CAMERA_ENTITY_NAME);
			const bool bCameraNamed = xCameraByName.IsValid();
			if (!bCameraNamed
				|| xCameraByName.GetEntityID() != xCamera.m_xEntityID)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at the camera entity "
					"'%s' -- no entity of that name exists, or it is not the "
					"scene's one ZM_FollowCamera [nameFound=%d "
					"sameEntityAsFollowCamera=%d]",
					szZM_PROFLAB_CAMERA_ENTITY_NAME,
					bCameraNamed ? 1 : 0,
					bCameraNamed
						&& xCameraByName.GetEntityID() == xCamera.m_xEntityID
						? 1 : 0);
				return false;
			}

			const float fSceneNear = xCamera.m_pxCamera->GetNearPlane();
			const float fSceneFar = xCamera.m_pxCamera->GetFarPlane();
			if (std::fabs(fSceneNear - fZM_PROFLAB_CAMERA_NEAR) > fPOSITION_EPSILON
				|| std::fabs(fSceneFar - fZM_PROFLAB_CAMERA_FAR) > fPOSITION_EPSILON)
			{
				FailProfLabDetailed(
					"ProfLab's committed scene bytes disagree with "
					"Source/World/ZM_ProfLabPlacement.h at the camera entity "
					"'%s' -- the scene was not re-authored after the header "
					"changed [scene near %.5f far %.5f vs header near %.5f far "
					"%.5f, epsilon %.6f]",
					szZM_PROFLAB_CAMERA_ENTITY_NAME,
					(double)fSceneNear, (double)fSceneFar,
					(double)fZM_PROFLAB_CAMERA_NEAR,
					(double)fZM_PROFLAB_CAMERA_FAR,
					(double)fPOSITION_EPSILON);
				return false;
			}
		}

		// ---- CLAUSE I3: PROFESSOR ASTER, THE ONE THING IN THIS ROOM THE
		//                 PLAYER CAME HERE FOR -----------------------------
		//
		// ★ WHY THE PROOF LIVES HERE AND NOWHERE ELSE. Every pure unit about
		// Aster in Tests/ZM_Tests_ProfLabPlacement.cpp reads
		// ZM_ProfLabPlacement.h -- and so does the authoring, so both sides of
		// each of those claims move together and not one of them can see the
		// SCENE drift away from the header. The committed-bytes needle
		// (ZM_CommittedSceneBytes/ProfLabCarriesTheAuthoredProfessorEntityName)
		// only proves a STRING is in the file. This clause is the only place
		// the AUTHORED RESULT is compared against the compiled contract, and it
		// is the mutation detector for all four of these, none of which any
		// other check in the gate can see:
		//   * author him at +x instead of -x -- a mirror image, into the other
		//     side of the room and out of the arrival frame;
		//   * pass AddStep_SetTransformScale(1.0f) instead of the human visual
		//     scale -- a professor drawn half again too big;
		//   * COLLISION_VOLUME_TYPE_SPHERE instead of AABB -- a body that no
		//     longer matches the contract the walk-up standoff is measured in;
		//   * omit AddStep_AddComponent("ZM_Interactable") entirely -- a
		//     professor who stands there and can never be spoken to.
		if (!ProfLabPhaseVerifyProfessor(pxData))
		{
			return false;
		}

		// ---- CLAUSE I4: THE WAY OUT (S8 SC-E) ----------------------------
		//
		// ★ THE SLICE THAT ADDED THE LAB DOOR HAD NO CI-VISIBLE PROOF WITHOUT
		// THIS. Read the block on ProfLabPhaseVerifyExitSensor for the full
		// argument; the short version is that the pure units compare compiled
		// constants against compiled constants, the round-trip walk needs the
		// gitignored Dawnmere terrain and skips on CI (a skip counts as a
		// PASS), and this file is the one place in the gate that looks at the
		// LOADED ProfLab with no terrain and no GPU. It catches, and nothing
		// else does:
		//   * the exit-sensor authoring steps deleted entirely;
		//   * AddStep_Custom(&ZM_ConfigureProfLabExitTrigger) omitted, leaving
		//     an unconfigured ZM_WarpTrigger on an invalid build index;
		//   * a configure function that resolved (or spelled) the WRONG build
		//     index -- PlayerHome's being the obvious slip;
		//   * a spawn tag that no longer matches the compiled connection;
		//   * a sensor moved so it contains the arrival point, which turns the
		//     door into an infinite warp loop.
		if (!ProfLabPhaseVerifyExitSensor(pxData))
		{
			return false;
		}

		g_bProfLabPassed = true;
		g_eProfLabPhase = ProfLabPhase::Done;
		return false;
	}

	// ---- Setup / dispatch / Verify -----------------------------------------

	void Setup_ProfLabWarp()
	{
		g_eProfLabPhase = ProfLabPhase::Done;
		g_iProfLabPhaseFrames = 0;
		g_bProfLabPassed = false;
		g_bProfLabFadeOutSeen = false;
		g_bProfLabOpaqueLoadSeen = false;
		g_bProfLabCameraBarrierSeen = false;
		g_bProfLabFadeInSeen = false;
		g_szProfLabFailure = "test did not reach verification";
		g_aszProfLabFailureDetail[0] = '\0';
		g_xProfLabFrontEndScene = Zenith_Scene();
		g_xProfLabManagerEntityID = INVALID_ENTITY_ID;

		// NO asset probe and NO RequestSkip -- see the ZM-D-147 deviation in
		// the file header. ProfLab.zscen is TRACKED, so its absence is a
		// DEFECT; a skip counts as a PASS and would silence this gate exactly
		// when it broke.
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fPROFLAB_FIXED_DT);
		g_eProfLabPhase = ProfLabPhase::FrontEndBootstrap;
	}

	bool Step_ProfLabWarp(int)
	{
		if (g_eProfLabPhase == ProfLabPhase::Done) { return false; }
		++g_iProfLabPhaseFrames;
		switch (g_eProfLabPhase)
		{
		case ProfLabPhase::FrontEndBootstrap: return ProfLabPhaseFrontEndBootstrap();
		case ProfLabPhase::WarpIn:            return ProfLabPhaseWarpIn();
		case ProfLabPhase::VerifyArrival:     return ProfLabPhaseVerifyArrival();
		case ProfLabPhase::Done:              return false;
		}
		return false;
	}

	bool Verify_ProfLabWarp()
	{
		Zenith_InputSimulator::ClearFixedDt();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		if (!g_bProfLabPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ProfLabWarp] %s", g_szProfLabFailure);
		}
		return g_bProfLabPassed;
	}
}   // close the anonymous namespace BEFORE the registration

static const Zenith_AutomatedTest g_xZMProfLabWarpTest = {
	"ZM_ProfLabWarp_Test",
	&Setup_ProfLabWarp,
	&Step_ProfLabWarp,
	&Verify_ProfLabWarp,
	/* maxFrames */ 1200,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMProfLabWarpTest);

#endif // ZENITH_INPUT_SIMULATOR
