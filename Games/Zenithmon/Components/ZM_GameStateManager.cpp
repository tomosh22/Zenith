#include "Zenith.h"

#include "Zenithmon/Components/ZM_GameStateManager.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"   // ZM_MakeStarterGameState
#include "Zenithmon/Source/UI/ZM_FadeOverlay.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float fFADE_OPAQUE = 1.0f;
	constexpr float fFADE_TRANSPARENT = 0.0f;

	void FreezeActiveScenePlayers()
	{
		g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().ForEach(
			[](Zenith_EntityID, ZM_PlayerController& xController)
			{
				xController.SetMovementEnabled(false);
			});
	}
}

#ifdef ZENITH_TOOLS
namespace
{
	const char* TransitionStateToString(ZM_WARP_TRANSITION_STATE eState)
	{
		switch (eState)
		{
		case ZM_WARP_TRANSITION_IDLE: return "IDLE";
		case ZM_WARP_TRANSITION_QUEUED: return "QUEUED";
		case ZM_WARP_TRANSITION_WAITING_FOR_SCENE: return "WAITING_FOR_SCENE";
		case ZM_WARP_TRANSITION_WAITING_FOR_SPAWN: return "WAITING_FOR_SPAWN";
		case ZM_WARP_TRANSITION_WAITING_FOR_CAMERA: return "WAITING_FOR_CAMERA";
		case ZM_WARP_TRANSITION_FADING_IN: return "FADING_IN";
		default: return "INVALID";
		}
	}
}
#endif

Zenith_EntityID ZM_GameStateManager::s_xSingletonEntityID = INVALID_ENTITY_ID;
ZM_GameStateManager::LoadSceneRequestCallback
	ZM_GameStateManager::s_pfnLoadSceneRequestForTests = nullptr;

ZM_GameStateManager::ZM_GameStateManager(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_GameStateManager::OnStart()
{
	const Zenith_EntityID xOwnEntityID = m_xParentEntity.GetEntityID();
	if (s_xSingletonEntityID == xOwnEntityID)
	{
		return;
	}

	Zenith_Entity xExisting =
		g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	if (xExisting.IsValid()
		&& xExisting.TryGetComponent<ZM_GameStateManager>() != nullptr)
	{
		m_xParentEntity.Destroy();
		return;
	}

	s_xSingletonEntityID = xOwnEntityID;
	ResetTransitionState(false);
	m_uIssuedLoadRequestCount = 0u;

	// First-boot seed of the persistent GameState (S5 item 5 SC2, D4). Reached
	// exactly once for the lifetime of the authoritative singleton: every later
	// re-authored manager is retired as a duplicate above and never gets here, and
	// the singleton's OnStart runs only once. Seeding BEFORE DontDestroyOnLoad lets
	// the starter ride the cross-scene move with the rest of this component.
	m_xGameState = ZM_MakeStarterGameState();

	// Moving to the persistent scene relocates this component's pool entry.
	// Nothing may access `this` after the call.
	m_xParentEntity.DontDestroyOnLoad();
}

void ZM_GameStateManager::OnUpdate(float fDeltaTime)
{
	if (!IsAuthoritativeSingleton())
	{
		return;
	}
	if (!std::isfinite(fDeltaTime) || fDeltaTime <= 0.0f)
	{
		return;
	}

	// The fade is part of the transition safety boundary, not decoration. If
	// the persistent root loses its exact authored overlay, keep every non-idle
	// transition opaque/locked at its current state rather than revealing a
	// partially initialised destination.
	if (!ApplyFadeVisual())
	{
		if (m_eTransitionState != ZM_WARP_TRANSITION_IDLE)
		{
			m_fFadeAlpha = fFADE_OPAQUE;
			FreezeActiveScenePlayers();
		}
		return;
	}

	// SC5 whiteout consume: a battle loss latched m_bPendingWhiteout in the write-back
	// (ZM_ApplyBattleResultToParty). Full-heal the whole party (idempotent) and warp to
	// Dawnmere's TownCenter. Only fire while nothing else owns the screen. The heal runs
	// BEFORE TryQueueWarp freezes/unfreezes the player, so the party is always whole before
	// it can re-enter grass -- which is why no fainted-lead guard is needed anywhere. Clear
	// the latch ONLY on an accepted warp, so a not-yet-ready frame retries rather than
	// silently dropping the whiteout (double-fire guard).
	if (m_xGameState.m_bPendingWhiteout
		&& m_eTransitionState == ZM_WARP_TRANSITION_IDLE
		&& !ZM_BattleTransition::IsTransitionActive())
	{
		m_xGameState.m_xParty.HealAllFull();
		if (TryQueueWarp(uWHITEOUT_BUILD_INDEX, szWHITEOUT_SPAWN_TAG))
		{
			m_xGameState.m_bPendingWhiteout = false;
		}
	}

	switch (m_eTransitionState)
	{
	case ZM_WARP_TRANSITION_QUEUED:
		m_fFadeAlpha = AdvanceFadeAlpha(
			m_fFadeAlpha, fFADE_OPAQUE, fDeltaTime);
		if (!ApplyFadeVisual())
		{
			return;
		}
		if (m_fFadeAlpha >= fFADE_OPAQUE)
		{
			IssueSingleLoad();
		}
		break;
	case ZM_WARP_TRANSITION_WAITING_FOR_SCENE:
		PollForTargetScene();
		break;
	case ZM_WARP_TRANSITION_WAITING_FOR_SPAWN:
		PollForSpawnAndPlacePlayer();
		break;
	case ZM_WARP_TRANSITION_WAITING_FOR_CAMERA:
		PollForCameraAndBeginFadeIn();
		break;
	case ZM_WARP_TRANSITION_FADING_IN:
		AdvanceFadeIn(fDeltaTime);
		break;
	case ZM_WARP_TRANSITION_IDLE:
	default:
		break;
	}
}

void ZM_GameStateManager::OnDestroy()
{
	if (s_xSingletonEntityID != m_xParentEntity.GetEntityID())
	{
		return;
	}

	ResetTransitionState(true);
	s_xSingletonEntityID = INVALID_ENTITY_ID;
}

void ZM_GameStateManager::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
}

void ZM_GameStateManager::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	(void)uVersion;
	// Serialized components never own live transition state. In particular, a
	// duplicate manager being deserialized must not unfreeze the authoritative
	// manager's active-scene Player before OnStart retires the duplicate. A live
	// authoritative instance being reset does release its own frozen Player.
	ResetTransitionState(IsAuthoritativeSingleton());
	m_uIssuedLoadRequestCount = 0u;
}

#ifdef ZENITH_TOOLS
void ZM_GameStateManager::RenderPropertiesPanel()
{
	ImGui::Text("Authoritative singleton: %s",
		IsAuthoritativeSingleton() ? "true" : "false");
	ImGui::Text("Transition: %s",
		TransitionStateToString(m_eTransitionState));
	ImGui::Text("Target build index: %u", m_uTargetBuildIndex);
	ImGui::Text("Target spawn tag: %s", m_szTargetSpawnTag[0] != '\0'
		? m_szTargetSpawnTag : "<none>");
	ImGui::Text("Fade alpha: %.3f", m_fFadeAlpha);
	ImGui::Text("Issued load requests: %u", m_uIssuedLoadRequestCount);
}
#endif

bool ZM_GameStateManager::RequestWarp(
	u_int uTargetBuildIndex,
	const char* szSpawnTag)
{
	Zenith_EntityID xManagerEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xManagerEntityID))
	{
		return false;
	}

	Zenith_Entity xManagerEntity =
		g_xEngine.Scenes().ResolveEntity(xManagerEntityID);
	ZM_GameStateManager* pxManager = xManagerEntity.IsValid()
		? xManagerEntity.TryGetComponent<ZM_GameStateManager>()
		: nullptr;
	return pxManager != nullptr
		&& pxManager->IsAuthoritativeSingleton()
		&& pxManager->TryQueueWarp(uTargetBuildIndex, szSpawnTag);
}

bool ZM_GameStateManager::IsWarpInProgress()
{
	Zenith_Entity xManagerEntity =
		g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	ZM_GameStateManager* pxManager = xManagerEntity.IsValid()
		? xManagerEntity.TryGetComponent<ZM_GameStateManager>()
		: nullptr;
	return pxManager != nullptr
		&& pxManager->IsAuthoritativeSingleton()
		&& pxManager->m_eTransitionState != ZM_WARP_TRANSITION_IDLE;
}

bool ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(
	Zenith_EntityID& xEntityIDOut)
{
	Zenith_Maths::Vector3 xUnusedScale(1.0f);
	return FindUniquePlayerInScene(xEntityIDOut, xUnusedScale);
}

bool ZM_GameStateManager::TryGetUniqueSingletonEntityID(
	Zenith_EntityID& xEntityIDOut)
{
	xEntityIDOut = INVALID_ENTITY_ID;
	u_int uManagerCount = 0u;
	g_xEngine.Scenes().QueryAllScenes<ZM_GameStateManager>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_GameStateManager&)
		{
			++uManagerCount;
			if (uManagerCount == 1u)
			{
				xEntityIDOut = xEntityID;
			}
		});

	if (uManagerCount != 1u)
	{
		xEntityIDOut = INVALID_ENTITY_ID;
		return false;
	}
	return g_xEngine.Scenes().ResolveEntity(xEntityIDOut).IsValid();
}

bool ZM_GameStateManager::TryGetGameState(ZM_GameState*& pxGameStateOut)
{
	pxGameStateOut = nullptr;

	// Resolve the unique manager fresh every call -- never cache it. The manager is
	// DontDestroyOnLoad, so this reaches its owned state from any active scene (e.g.
	// the additively-loaded Battle scene doing the SC3-SC5 write-back).
	Zenith_EntityID xManagerEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xManagerEntityID))
	{
		return false;
	}

	Zenith_Entity xManagerEntity =
		g_xEngine.Scenes().ResolveEntity(xManagerEntityID);
	ZM_GameStateManager* pxManager = xManagerEntity.IsValid()
		? xManagerEntity.TryGetComponent<ZM_GameStateManager>()
		: nullptr;
	if (pxManager == nullptr)
	{
		return false;
	}

	pxGameStateOut = &pxManager->m_xGameState;
	return true;
}

bool ZM_GameStateManager::IsWarpDestinationValid(
	u_int uTargetBuildIndex,
	const char* szSpawnTag)
{
	if (!ZM_SpawnPoint::IsTagValid(szSpawnTag))
	{
		return false;
	}

	const ZM_SCENE_ID eTargetScene =
		ZM_FindSceneByBuildIndex(uTargetBuildIndex);
	if (eTargetScene == ZM_SCENE_NONE)
	{
		return false;
	}

	const ZM_WorldSpec& xTargetSpec = ZM_GetWorldSpec(eTargetScene);
	for (u_int uTagIndex = 0u;
		uTagIndex < xTargetSpec.m_uSpawnTagCount;
		++uTagIndex)
	{
		if (std::strcmp(xTargetSpec.m_pszSpawnTags[uTagIndex], szSpawnTag) == 0)
		{
			return true;
		}
	}
	return false;
}

Zenith_Maths::Vector3 ZM_GameStateManager::CalculateSpawnCenter(
	const Zenith_Maths::Vector3& xMarkerFeetPosition,
	const Zenith_Maths::Vector3& xPlayerScale)
{
	return xMarkerFeetPosition + Zenith_Maths::Vector3(
		0.0f,
		ZM_PlayerController::CalculateCapsuleHalfExtent(xPlayerScale),
		0.0f);
}

float ZM_GameStateManager::AdvanceFadeAlpha(
	float fCurrentAlpha,
	float fTargetAlpha,
	float fDeltaTime)
{
	if (!std::isfinite(fCurrentAlpha))
	{
		fCurrentAlpha = fFADE_TRANSPARENT;
	}
	fCurrentAlpha = std::clamp(
		fCurrentAlpha, fFADE_TRANSPARENT, fFADE_OPAQUE);
	if (!std::isfinite(fTargetAlpha)
		|| !std::isfinite(fDeltaTime)
		|| fDeltaTime <= 0.0f)
	{
		return fCurrentAlpha;
	}

	fTargetAlpha = std::clamp(
		fTargetAlpha, fFADE_TRANSPARENT, fFADE_OPAQUE);
	const float fStep = fDeltaTime / fFADE_DURATION_SECONDS;
	if (!std::isfinite(fStep) || fStep >= fFADE_OPAQUE)
	{
		return fTargetAlpha;
	}
	if (fCurrentAlpha < fTargetAlpha)
	{
		return std::min(fTargetAlpha, fCurrentAlpha + fStep);
	}
	return std::max(fTargetAlpha, fCurrentAlpha - fStep);
}

void ZM_GameStateManager::ResetRuntimeStateForTests()
{
	s_pfnLoadSceneRequestForTests = nullptr;

	Zenith_Entity xManagerEntity =
		g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	ZM_GameStateManager* pxManager = xManagerEntity.IsValid()
		? xManagerEntity.TryGetComponent<ZM_GameStateManager>()
		: nullptr;
	if (pxManager == nullptr)
	{
		s_xSingletonEntityID = INVALID_ENTITY_ID;
		return;
	}

	pxManager->ResetTransitionState(true);
	pxManager->m_uIssuedLoadRequestCount = 0u;
}

void ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(
	LoadSceneRequestCallback pfnCallback)
{
	s_pfnLoadSceneRequestForTests = pfnCallback;
}

void ZM_GameStateManager::ResetGameStateForTests()
{
	// The DontDestroyOnLoad manager (and its m_xGameState) usually survives between
	// batched tests, so a test that caught a monster or levelled the party would leak
	// forward. Re-seed to the fixed starter. A safe no-op when no manager exists yet.
	ZM_GameState* pxGameState = nullptr;
	if (TryGetGameState(pxGameState))
	{
		*pxGameState = ZM_MakeStarterGameState();
	}
}

bool ZM_GameStateManager::IsAuthoritativeSingleton() const
{
	return m_xParentEntity.IsValid()
		&& s_xSingletonEntityID == m_xParentEntity.GetEntityID();
}

bool ZM_GameStateManager::TryQueueWarp(
	u_int uTargetBuildIndex,
	const char* szSpawnTag)
{
	if (!IsAuthoritativeSingleton()
		|| ZM_BattleTransition::IsTransitionActive()   // no warp may race the battle round trip
		|| m_eTransitionState != ZM_WARP_TRANSITION_IDLE
		|| !IsWarpDestinationValid(uTargetBuildIndex, szSpawnTag)
		|| !ApplyFadeVisual())
	{
		return false;
	}

	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 xUnusedScale(1.0f);
	const bool bHasUniquePlayer =
		FindUniquePlayerInScene(xPlayerEntityID, xUnusedScale);
	Zenith_Entity xPlayerEntity = bHasUniquePlayer
		? g_xEngine.Scenes().ResolveEntity(xPlayerEntityID)
		: Zenith_Entity();
	ZM_PlayerController* pxPlayerController = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<ZM_PlayerController>()
		: nullptr;

	// FrontEnd is the sole playerless source scene: choosing New Game uses the
	// same validated transition path into Dawnmere before a Player exists. Any
	// PlayerController authored there must still satisfy the normal unique-body
	// contract; every non-FrontEnd source always requires one.
	const Zenith_Scene xSourceScene = g_xEngine.Scenes().GetActiveScene();
	const Zenith_SceneInfo xSourceInfo =
		g_xEngine.Scenes().GetSceneInfo(xSourceScene);
	const bool bPlayerlessFrontEnd = xSourceInfo.m_bLoaded
		&& xSourceInfo.m_iBuildIndex == 0
		&& g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().Count() == 0u;
	if (pxPlayerController == nullptr && !bPlayerlessFrontEnd)
	{
		return false;
	}

	char szValidatedTag[uTAG_CAPACITY] = {};
	const size_t ulTagLength = std::strlen(szSpawnTag);
	std::memcpy(szValidatedTag, szSpawnTag, ulTagLength);

	// Input freezes immediately on acceptance. QUEUED owns the deterministic
	// fade-out; the actual SINGLE load cannot issue until the screen is opaque.
	if (pxPlayerController != nullptr)
	{
		pxPlayerController->SetMovementEnabled(false);
		m_xFrozenPlayerEntityID = xPlayerEntityID;
	}
	m_fFadeAlpha = fFADE_TRANSPARENT;
	ApplyFadeVisual();
	m_uTargetBuildIndex = uTargetBuildIndex;
	std::memcpy(m_szTargetSpawnTag, szValidatedTag,
		sizeof(m_szTargetSpawnTag));
	m_eTransitionState = ZM_WARP_TRANSITION_QUEUED;
	return true;
}

void ZM_GameStateManager::ResetTransitionState(bool bEnableFrozenPlayer)
{
	if (bEnableFrozenPlayer && m_xFrozenPlayerEntityID.IsValid())
	{
		Zenith_Entity xPlayerEntity =
			g_xEngine.Scenes().ResolveEntity(m_xFrozenPlayerEntityID);
		if (xPlayerEntity.IsValid())
		{
			if (ZM_PlayerController* pxController =
				xPlayerEntity.TryGetComponent<ZM_PlayerController>())
			{
				pxController->ResetRuntimeState();
				pxController->SetMovementEnabled(true);
			}
		}
	}
	if (bEnableFrozenPlayer)
	{
		// A replacement Player freezes itself in OnStart before the manager's
		// later component order can record its generation-bearing ID. Releasing a
		// cancelled/test-reset transition therefore also visits the active scene.
		g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().ForEach(
			[](Zenith_EntityID, ZM_PlayerController& xController)
			{
				xController.ResetRuntimeState();
				xController.SetMovementEnabled(true);
			});
	}

	m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
	m_eTransitionState = ZM_WARP_TRANSITION_IDLE;
	m_uTargetBuildIndex = uINVALID_BUILD_INDEX;
	m_fFadeAlpha = fFADE_TRANSPARENT;
	std::memset(m_szTargetSpawnTag, 0, sizeof(m_szTargetSpawnTag));
	ApplyFadeVisual();
}

void ZM_GameStateManager::IssueSingleLoad()
{
	if (m_eTransitionState != ZM_WARP_TRANSITION_QUEUED
		|| m_fFadeAlpha < fFADE_OPAQUE)
	{
		return;
	}

	m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SCENE;
	++m_uIssuedLoadRequestCount;
	if (s_pfnLoadSceneRequestForTests != nullptr)
	{
		s_pfnLoadSceneRequestForTests(m_uTargetBuildIndex);
		return;
	}

	g_xEngine.Scenes().LoadSceneByIndex(
		static_cast<int>(m_uTargetBuildIndex), SCENE_LOAD_SINGLE);
}

void ZM_GameStateManager::PollForTargetScene()
{
	if (!IsTargetSceneActive())
	{
		return;
	}

	m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SPAWN;
}

void ZM_GameStateManager::PollForSpawnAndPlacePlayer()
{
	const Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	if (!IsTargetSceneActive())
	{
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SCENE;
		return;
	}

	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 xPlayerScale(1.0f);
	if (!FindUniquePlayerInScene(xPlayerEntityID, xPlayerScale))
	{
		FreezeActiveScenePlayers();
		return;
	}

	Zenith_Entity xPlayerEntity =
		g_xEngine.Scenes().ResolveEntity(xPlayerEntityID);
	ZM_PlayerController* pxPlayerController = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<ZM_PlayerController>()
		: nullptr;
	if (pxPlayerController == nullptr)
	{
		return;
	}

	// Each newly-resolved target-scene generation is frozen before any marker
	// validation. Missing/duplicate markers cannot leak player input.
	pxPlayerController->SetMovementEnabled(false);
	m_xFrozenPlayerEntityID = xPlayerEntityID;

	Zenith_EntityID xSpawnEntityID = INVALID_ENTITY_ID;
	if (ZM_SpawnPoint::FindUniqueInScene(
		xActiveScene, m_szTargetSpawnTag, xSpawnEntityID)
		!= ZM_SPAWN_POINT_LOOKUP_FOUND)
	{
		return;
	}

	Zenith_Entity xSpawnEntity =
		g_xEngine.Scenes().ResolveEntity(xSpawnEntityID);
	Zenith_TransformComponent* pxSpawnTransform = xSpawnEntity.IsValid()
		? xSpawnEntity.TryGetComponent<Zenith_TransformComponent>()
		: nullptr;
	Zenith_ColliderComponent* pxPlayerCollider =
		xPlayerEntity.TryGetComponent<Zenith_ColliderComponent>();
	if (pxSpawnTransform == nullptr
		|| pxPlayerCollider == nullptr
		|| !pxPlayerCollider->HasValidBody()
		|| pxPlayerCollider->GetCollisionVolumeType()
			!= COLLISION_VOLUME_TYPE_CAPSULE
		|| pxPlayerCollider->GetRigidBodyType() != RIGIDBODY_TYPE_DYNAMIC)
	{
		return;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (!xPhysics.HasActiveSimulation())
	{
		return;
	}

	Zenith_Maths::Vector3 xMarkerFeetPosition;
	pxSpawnTransform->GetPosition(xMarkerFeetPosition);
	const Zenith_Maths::Vector3 xSpawnCenter = CalculateSpawnCenter(
		xMarkerFeetPosition, xPlayerScale);
	const Zenith_PhysicsBodyID xPlayerBodyID = pxPlayerCollider->GetBodyID();
	xPhysics.TeleportBody(xPlayerBodyID, xSpawnCenter);
	xPhysics.SetLinearVelocity(xPlayerBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetAngularVelocity(xPlayerBodyID, Zenith_Maths::Vector3(0.0f));

	pxPlayerController->ResetRuntimeState();
	pxPlayerController->SetMovementEnabled(false);
	m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_CAMERA;
}

void ZM_GameStateManager::PollForCameraAndBeginFadeIn()
{
	if (!IsTargetSceneActive())
	{
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SCENE;
		return;
	}

	ZM_PlayerController* pxPlayerController = nullptr;
	if (!TryResolveFrozenTargetPlayer(pxPlayerController))
	{
		FreezeActiveScenePlayers();
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SPAWN;
		return;
	}

	pxPlayerController->SetMovementEnabled(false);
	if (!HasUniqueReadyFollowCamera(m_xFrozenPlayerEntityID))
	{
		return;
	}

	// Placement happened on an earlier manager update. Requiring this separate
	// camera-wait update gives the order-103 follow camera a complete scene
	// update to generation-safely acquire the replacement Player while black.
	m_eTransitionState = ZM_WARP_TRANSITION_FADING_IN;
}

void ZM_GameStateManager::AdvanceFadeIn(float fDeltaTime)
{
	if (!IsTargetSceneActive())
	{
		m_fFadeAlpha = fFADE_OPAQUE;
		ApplyFadeVisual();
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SCENE;
		return;
	}

	ZM_PlayerController* pxPlayerController = nullptr;
	if (!TryResolveFrozenTargetPlayer(pxPlayerController))
	{
		FreezeActiveScenePlayers();
		m_fFadeAlpha = fFADE_OPAQUE;
		ApplyFadeVisual();
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SPAWN;
		return;
	}

	pxPlayerController->SetMovementEnabled(false);
	if (!HasUniqueReadyFollowCamera(m_xFrozenPlayerEntityID))
	{
		m_fFadeAlpha = fFADE_OPAQUE;
		ApplyFadeVisual();
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_CAMERA;
		return;
	}

	m_fFadeAlpha = AdvanceFadeAlpha(
		m_fFadeAlpha, fFADE_TRANSPARENT, fDeltaTime);
	if (!ApplyFadeVisual())
	{
		m_fFadeAlpha = fFADE_OPAQUE;
		return;
	}
	if (m_fFadeAlpha > fFADE_TRANSPARENT)
	{
		return;
	}

	pxPlayerController->ResetRuntimeState();
	pxPlayerController->SetMovementEnabled(true);
	ResetTransitionState(false);
}

bool ZM_GameStateManager::ApplyFadeVisual()
{
	return ZM_FadeOverlay::Apply(m_xParentEntity, szFADE_ELEMENT_NAME, m_fFadeAlpha);
}

bool ZM_GameStateManager::IsTargetSceneActive() const
{
	const Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	if (!xActiveScene.IsValid())
	{
		return false;
	}

	const Zenith_SceneInfo xSceneInfo =
		g_xEngine.Scenes().GetSceneInfo(xActiveScene);
	return xSceneInfo.m_bLoaded
		&& xSceneInfo.m_iBuildIndex == static_cast<int>(m_uTargetBuildIndex);
}

bool ZM_GameStateManager::TryResolveFrozenTargetPlayer(
	ZM_PlayerController*& pxControllerOut) const
{
	pxControllerOut = nullptr;
	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 xUnusedScale(1.0f);
	if (!FindUniquePlayerInScene(xPlayerEntityID, xUnusedScale)
		|| xPlayerEntityID != m_xFrozenPlayerEntityID)
	{
		return false;
	}

	Zenith_Entity xPlayerEntity =
		g_xEngine.Scenes().ResolveEntity(xPlayerEntityID);
	pxControllerOut = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<ZM_PlayerController>()
		: nullptr;
	return pxControllerOut != nullptr;
}

bool ZM_GameStateManager::HasUniqueReadyFollowCamera(
	Zenith_EntityID xPlayerEntityID)
{
	u_int uCameraCount = 0u;
	bool bTargetMatches = false;
	Zenith_EntityID xFollowCameraEntityID = INVALID_ENTITY_ID;
	g_xEngine.Scenes().QueryActiveScene<ZM_FollowCamera>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_FollowCamera& xCamera)
		{
			++uCameraCount;
			if (uCameraCount == 1u)
			{
				xFollowCameraEntityID = xEntityID;
				bTargetMatches =
					xCamera.GetTargetEntityID() == xPlayerEntityID;
			}
		});
	if (uCameraCount != 1u || !bTargetMatches)
	{
		return false;
	}

	Zenith_Entity xCameraEntity =
		g_xEngine.Scenes().ResolveEntity(xFollowCameraEntityID);
	if (!xCameraEntity.IsValid()
		|| xCameraEntity.TryGetComponent<Zenith_CameraComponent>() == nullptr)
	{
		return false;
	}

	const Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxActiveSceneData =
		g_xEngine.Scenes().GetSceneData(xActiveScene);
	return pxActiveSceneData != nullptr
		&& pxActiveSceneData->GetMainCameraEntity() == xFollowCameraEntityID;
}

bool ZM_GameStateManager::FindUniquePlayerInScene(
	Zenith_EntityID& xPlayerEntityIDOut,
	Zenith_Maths::Vector3& xPlayerScaleOut)
{
	xPlayerEntityIDOut = INVALID_ENTITY_ID;
	xPlayerScaleOut = Zenith_Maths::Vector3(1.0f);
	if (!g_xEngine.Physics().HasActiveSimulation())
	{
		return false;
	}

	u_int uPlayerCount = 0u;
	g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_PlayerController&)
		{
			++uPlayerCount;
			if (uPlayerCount == 1u)
			{
				xPlayerEntityIDOut = xEntityID;
			}
		});

	if (uPlayerCount != 1u)
	{
		xPlayerEntityIDOut = INVALID_ENTITY_ID;
		xPlayerScaleOut = Zenith_Maths::Vector3(1.0f);
		return false;
	}

	Zenith_Entity xPlayerEntity =
		g_xEngine.Scenes().ResolveEntity(xPlayerEntityIDOut);
	Zenith_TransformComponent* pxTransform = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<Zenith_TransformComponent>()
		: nullptr;
	Zenith_ColliderComponent* pxCollider = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<Zenith_ColliderComponent>()
		: nullptr;
	if (pxTransform == nullptr
		|| pxCollider == nullptr
		|| !pxCollider->HasValidBody()
		|| pxCollider->GetCollisionVolumeType()
			!= COLLISION_VOLUME_TYPE_CAPSULE
		|| pxCollider->GetRigidBodyType() != RIGIDBODY_TYPE_DYNAMIC)
	{
		xPlayerEntityIDOut = INVALID_ENTITY_ID;
		xPlayerScaleOut = Zenith_Maths::Vector3(1.0f);
		return false;
	}

	pxTransform->GetScale(xPlayerScaleOut);
	return true;
}
