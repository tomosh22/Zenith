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
#include "Zenithmon/Components/ZM_Interactable.h"      // ZM_IntroStoryFlagForArrival -- the arrival tail's PURE decision
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"       // ZM_SetStoryFlag -- the intro flags' one producer
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"       // ZM_MakeNewGameState
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"   // ZM_ApplyStarterChoice -- the TEST-ONLY reseed only (see ResetGameStateForTests)
#include "Zenithmon/Source/Save/ZM_Autosave.h"     // the milestone autosave latch (SC3)
#include "Zenithmon/Source/Save/ZM_ResumePoint.h"  // the PURE resume decision surface (SC3)
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"
#include "Zenithmon/Source/UI/ZM_FadeOverlay.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // feet <-> centre conversion

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

	// The unique AUTHORITATIVE manager, or null. Resolved fresh every call and never
	// cached -- the manager is DontDestroyOnLoad and its pool entry relocates. Note
	// that this transiently returns null during a FrontEnd re-author, while the
	// duplicate manager exists and TryGetUniqueSingletonEntityID counts two.
	ZM_GameStateManager* ResolveAuthoritativeManager()
	{
		Zenith_EntityID xManagerEntityID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xManagerEntity =
			g_xEngine.Scenes().ResolveEntity(xManagerEntityID);
		ZM_GameStateManager* pxManager = xManagerEntity.IsValid()
			? xManagerEntity.TryGetComponent<ZM_GameStateManager>()
			: nullptr;
		return (pxManager != nullptr && pxManager->IsAuthoritativeSingleton())
			? pxManager
			: nullptr;
	}

	void FreezeActiveScenePlayers()
	{
		g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().ForEach(
			[](Zenith_EntityID, ZM_PlayerController& xController)
			{
				xController.SetMovementEnabled(false);
			});
	}

	// ★ NO LONGER TOOLS-ONLY. This used to live under #ifdef ZENITH_TOOLS purely to
	// feed RenderPropertiesPanel; the barrier-timeout report has to NAME the state it
	// died in, in every configuration -- a headless CI log and an on-device Android
	// logcat are exactly where this diagnostic earns its keep, and neither is a tools
	// build. Zenith_Log/Zenith_Error are unconditionally live (ZENITH_LOG is defined
	// unconditionally in Zenith.h), so this is never an unreferenced function.
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

	// The middle sentence of a timeout report: the LIKELY CAUSE, tailored per barrier.
	// THE LOG LINE IS THE WHOLE DELIVERABLE -- a reader who has just watched the game
	// sit on a black screen must come away with a one-line fix, not with "the game
	// hung" -- so each barrier names the specific authoring mistake that parks it,
	// rather than sharing one vague sentence between all three.
	const char* TransitionTimeoutLikelyCause(ZM_WARP_TRANSITION_STATE eState)
	{
		switch (eState)
		{
		case ZM_WARP_TRANSITION_WAITING_FOR_SCENE:
			return "The SINGLE load was issued but that build index never became the "
				"active LOADED scene: check for a missing or wrong "
				"Zenith_SceneSystem::RegisterSceneBuildIndex row for it.";
		case ZM_WARP_TRANSITION_WAITING_FOR_SPAWN:
			return "The destination scene loaded but nothing in it satisfied the "
				"placement barrier: either no UNIQUE bodied ZM_PlayerController "
				"(dynamic capsule with a live body), or no UNIQUE ZM_SpawnPoint "
				"offering that tag.";
		case ZM_WARP_TRANSITION_WAITING_FOR_CAMERA:
			return "The destination scene loaded and the player was placed, but no "
				"unique READY follow camera ever appeared: the scene needs exactly one "
				"ZM_FollowCamera that is its MAIN camera and whose target is this "
				"player generation (the camera acquires the player BY COMPONENT as of "
				"9b5a401b, so a destination with no ZM_PlayerController or no "
				"ZM_FollowCamera parks here).";
		default:
			return "";
		}
	}
}

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
	// the state ride the cross-scene move with the rest of this component.
	//
	// ★★ NO STARTER IS GRANTED HERE ANY MORE (ZM-D-188). This used to be
	// ZM_MakeNewGameState() plus an explicit Fernfawn, with a note saying "the later
	// lab beat replaces exactly this one line" -- this is that beat, and the line is
	// GONE rather than moved. A new game now begins with an EMPTY PARTY and the player
	// chooses their first partner from Professor Aster in his lab; a Fernfawn handed
	// out at boot would make that choice cosmetic and, because ZM_ApplyStarterChoice
	// refuses only a FULL party, would leave the player holding TWO monsters after it.
	//
	// ★ THE THIRD SEED SITE IS DELIBERATELY ASYMMETRIC -- see the block on
	// ResetGameStateForTests below. That one is TEST-ONLY and still grants.
	m_xGameState = ZM_MakeNewGameState();

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

	// S7 SC3 milestone autosave, drained on a LATER frame than the arrival that
	// latched it, and ONLY once the machine is fully IDLE. It cannot run from
	// AdvanceFadeIn's tail: ZM_SaveSlots::ResolveLiveSaveBlocker consults
	// IsWarpInProgress(), which is true for EVERY non-IDLE transition state, so an
	// in-tail autosave would always resolve ZM_SAVE_BLOCKER_WARP and silently never
	// save. Draining here also means the arrival tag has already been recorded, so
	// the capture has a spawn tag to write.
	// The latch is consumed BEFORE the attempt: a refused or failed autosave must
	// never be retried on the next frame (that is a disk-hammering loop, not a
	// recovery), and ZM_TryAutosave logs its own failure.
	if (m_bArrivalAutosavePending && m_eTransitionState == ZM_WARP_TRANSITION_IDLE)
	{
		m_bArrivalAutosavePending = false;
		ZM_TryAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED);
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

	// ---- S8 item 2: the shared barrier dwell counter, and its timeout ----------
	// Placed HERE, below every early return above, so "polled" means exactly what it
	// says: a frame on which the machine actually reached its transition switch. A
	// frame that bailed on a non-finite delta time, on a non-authoritative manager, or
	// on a lost fade overlay did not poll a barrier and must not spend its budget --
	// the lost-overlay path in particular deliberately PARKS the transition, and
	// charging it for frames it never polled would time out a warp that was never
	// given a chance to progress.
	TrackTransitionStateDwell();
	ApplyTransitionTimeoutIfExpired();

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
	ImGui::Text("Frames in state: %u (budget %u)",
		m_uFramesInTransitionState,
		GetTransitionTimeoutFrames(m_eTransitionState));
	ImGui::Text("Timed out: %s", m_bTransitionTimedOut ? "true" : "false");
	ImGui::Text("Target build index: %u", m_uTargetBuildIndex);
	ImGui::Text("Target spawn tag: %s", m_szTargetSpawnTag[0] != '\0'
		? m_szTargetSpawnTag : "<none>");
	ImGui::Text("Fade alpha: %.3f", m_fFadeAlpha);
	ImGui::Text("Issued load requests: %u", m_uIssuedLoadRequestCount);
	ImGui::Text("Arrived spawn tag: %s", m_szLastArrivedSpawnTag[0] != '\0'
		? m_szLastArrivedSpawnTag : "<none>");
	ImGui::Text("Playerless destination: %s",
		m_bTargetIsPlayerless ? "true" : "false");
	ImGui::Text("Resume pending: %s", m_bResumePending ? "true" : "false");
	ImGui::Text("Autosaves this run: %u", ZM_GetAutosaveCount());
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
	return FindUniquePlayerInScene(xEntityIDOut);
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

bool ZM_GameStateManager::RequestNewGame()
{
	// Build the replacement first. No live state changes until the ordinary warp
	// transaction has passed every validation gate and owns the transition.
	//
	// ★★ THE REAL PLAYER PATH, AND IT PUBLISHES A **PARTYLESS** STATE (ZM-D-188).
	// This is the one seed site a human being can actually reach -- FrontEnd's New
	// Game entry -- so it is the one that decides what "starting a new game" means.
	// It used to compose ZM_MakeNewGameState() with an explicit Fernfawn grant; that
	// grant is deleted, and the player now leaves PlayerHome with nothing and chooses
	// a starter from Professor Aster.
	//
	// ★ THIS MAKES TWO PREVIOUSLY-UNREACHABLE ARMS REACHABLE, ON PURPOSE, AND BOTH
	// ALREADY HAD COVERAGE WAITING FOR IT: ZM_CanEnterBattle answers FALSE for the
	// whole stretch between the front door and the lab, so ZM_MayTrainerEngage's
	// bPlayerCanBattle clause keeps rival Vesper silent when a partyless player
	// crosses his cone, and ZM_BattleDirector's own emptiness test refuses a battle
	// the player could not fight. Neither had ever run in a playthrough before this.
	const ZM_GameState xNewGame = ZM_MakeNewGameState();
	ZM_GameStateManager* pxManager = ResolveAuthoritativeManager();
	if (pxManager == nullptr
		|| !pxManager->TryQueueWarp(uNEW_GAME_BUILD_INDEX, szNEW_GAME_SPAWN_TAG))
	{
		return false;
	}

	pxManager->m_xGameState = xNewGame;
	return true;
}

Zenith_Status ZM_GameStateManager::RequestContinue(ZM_SAVE_SLOT eSlot)
{
	// Never decode into the persistent state. ReadState itself is transactional,
	// and keeping its destination local preserves both that guarantee and the exact
	// disk error for the title UI.
	ZM_GameState xCandidate;
	const Zenith_Status xReadStatus = ZM_SaveSlots::ReadState(eSlot, xCandidate);
	if (!xReadStatus.IsOk())
	{
		return xReadStatus.Error();
	}

	ZM_GameStateManager* pxManager = ResolveAuthoritativeManager();
	if (pxManager == nullptr)
	{
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	// QueueResume validates and latches the candidate's one-shot pose only after
	// TryQueueWarp accepts. Publish the entire candidate last, so any refusal leaves
	// the running game byte-for-byte untouched.
	const Zenith_Status xResumeStatus =
		pxManager->QueueResume(xCandidate.m_xWorldPosition);
	if (!xResumeStatus.IsOk())
	{
		return xResumeStatus.Error();
	}

	pxManager->m_xGameState = xCandidate;
	return true;
}

const char* ZM_GameStateManager::GetActiveSceneArrivedSpawnTag()
{
	const ZM_GameStateManager* pxManager = ResolveAuthoritativeManager();
	// "" and not nullptr: every caller either strcmps this or passes it to a
	// validator, and a null would turn "nobody has arrived anywhere" into a crash.
	return pxManager != nullptr ? pxManager->m_szLastArrivedSpawnTag : "";
}

bool ZM_GameStateManager::CaptureWorldPosition(ZM_GameState& xStateInOut)
{
	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	if (!FindUniquePlayerInScene(xPlayerEntityID))
	{
		// The ordinary answer on FrontEnd and in any headless context. Not an error:
		// "there is no player to capture" is exactly what a playerless scene means.
		return false;
	}

	Zenith_Entity xPlayerEntity =
		g_xEngine.Scenes().ResolveEntity(xPlayerEntityID);
	Zenith_ColliderComponent* pxPlayerCollider = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<Zenith_ColliderComponent>()
		: nullptr;
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (pxPlayerCollider == nullptr
		|| !pxPlayerCollider->HasValidBody()
		|| !xPhysics.HasActiveSimulation())
	{
		return false;
	}

	const Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	const Zenith_SceneInfo xSceneInfo =
		g_xEngine.Scenes().GetSceneInfo(xActiveScene);
	if (!xSceneInfo.m_bLoaded || xSceneInfo.m_iBuildIndex < 0)
	{
		return false;
	}
	const u_int uBuildIndex = static_cast<u_int>(xSceneInfo.m_iBuildIndex);
	// Resolved BEFORE ZM_GetWorldSpec is touched anywhere below: that accessor
	// asserts fatally on ZM_SCENE_NONE, and the active scene's build index is not
	// guaranteed to be one this game's world table names.
	const ZM_SCENE_ID eActiveScene = ZM_FindSceneByBuildIndex(uBuildIndex);
	if (eActiveScene == ZM_SCENE_NONE)
	{
		return false;
	}

	// The tag the player actually arrived at, when a transition recorded one.
	//
	// The FALLBACK is load-bearing, not defensive: the normal boot path and every
	// direct LoadSceneByIndex enter a scene WITHOUT a warp, so nothing has recorded
	// an arrival tag and the resume would be unsaveable (the codec rejects an empty
	// spawn tag on a set scene index). Falling back to the scene's FIRST offered tag
	// gives a valid, scene-owned marker to fall back to if the saved transform ever
	// turns out to be unusable -- which is precisely what the spawn-tag half of
	// "transform-first, spawn-tag fallback" is for.
	const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(eActiveScene);
	const char* szArrivedTag = GetActiveSceneArrivedSpawnTag();
	if (szArrivedTag == nullptr || szArrivedTag[0] == '\0')
	{
		if (xSpec.m_uSpawnTagCount == 0u || xSpec.m_pszSpawnTags == nullptr)
		{
			return false;
		}
		szArrivedTag = xSpec.m_pszSpawnTags[0];
	}

	const Zenith_PhysicsBodyID xPlayerBodyID = pxPlayerCollider->GetBodyID();
	// ★ THE BODY POSITION IS THE PLAYER'S FEET (ZM-D-223). It used to be the capsule
	// CENTRE, and this comment used to say so; the entity origin moved to the feet
	// and the capsule SHAPE is offset instead, so the same call now returns the feet.
	// Stored as-is and applied back as-is -- the save, the spawn markers and the
	// transform all speak one vocabulary now, which is what removed the conversions
	// this file used to carry.
	//
	// A save WRITTEN before that move stores a centre, and nothing on the wire can
	// tell the two apart: ZM_SaveSchema's v2->v3 migration is what converts it, and
	// is the reason the schema version had to move for a format that did not.
	const Zenith_Maths::Vector3 xFeet = xPhysics.GetBodyPosition(xPlayerBodyID);
	const float fYaw = ZM_YawFromRotation(xPhysics.GetBodyRotation(xPlayerBodyID));

	// Built into a LOCAL and published in one assignment, so a rejected pose leaves
	// the caller's m_xWorldPosition byte-identical.
	ZM_WorldPosition xCaptured;
	if (!ZM_MakeWorldPosition(uBuildIndex, szArrivedTag, xFeet, fYaw, xCaptured))
	{
		return false;
	}
	xStateInOut.m_xWorldPosition = xCaptured;
	return true;
}

bool ZM_GameStateManager::RequestResume(const ZM_WorldPosition& xResume)
{
	ZM_GameStateManager* pxManager = ResolveAuthoritativeManager();
	return pxManager != nullptr && pxManager->QueueResume(xResume).IsOk();
}

Zenith_Status ZM_GameStateManager::QueueResume(const ZM_WorldPosition& xResume)
{
	// The ECS half asks the pure half. ZM_SpawnPoint::IsTagValid is the grammar
	// answer ZM_ValidateResume takes as an argument precisely so the pure TU never
	// has to name a component.
	const bool bTagGrammarValid = ZM_SpawnPoint::IsTagValid(xResume.m_szSpawnTag);
	const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xResume, bTagGrammarValid);
	if (!ZM_CanResume(eValidity))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM GameStateManager] RequestResume refused: saved position is %s",
			ZM_ResumeValidityName(eValidity));
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	// The resume rides the ORDINARY validated warp -- same fade, same single load,
	// same spawn-marker placement. Nothing is latched until that warp is accepted,
	// so a refusal leaves no pose waiting to ambush the next transition.
	if (!TryQueueWarp(xResume.m_uSceneBuildIndex, xResume.m_szSpawnTag))
	{
		return Zenith_ErrorCode::QUEUE_FULL;
	}

	m_xPendingResume = xResume;
	// INVALID_TRANSFORM resumes with NO override: the warp still happens and the
	// spawn marker stands. That is the fallback half of the rule, and it costs
	// nothing because the marker placement is already on the path.
	m_bResumePending = ZM_ShouldUseSavedTransform(eValidity);
	return true;
}

bool ZM_GameStateManager::RequestQuitToFrontEnd()
{
	// Deliberately the SAME validated entry point every other warp uses, rather than
	// a bespoke load: it is what sets m_bTargetIsPlayerless, freezes the player,
	// runs the fade-out, and refuses while a battle or another warp owns the screen.
	// FrontEnd's ZM_WorldSpec row offers "Start", so IsWarpDestinationValid accepts
	// it even though no marker with that tag is ever authored -- the playerless
	// branches are what make that harmless.
	return RequestWarp(uFRONTEND_BUILD_INDEX, szFRONTEND_SPAWN_TAG);
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

Zenith_Maths::Vector3 ZM_GameStateManager::CalculateSpawnPosition(
	const Zenith_Maths::Vector3& xMarkerFeetPosition)
{
	// ★ THE IDENTITY, DELIBERATELY KEPT AS A FUNCTION. It used to add half a body
	// height, because a spawn MARKER stored feet while an entity stored its centre;
	// the entity stores its feet now, so the two vocabularies are one and there is
	// nothing to convert. The function survives because it is the single named place
	// that says so -- deleting it would scatter the claim "a marker IS a spawn
	// position" across every caller, where the next convention change could not find
	// it.
	return xMarkerFeetPosition;
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

#ifdef ZENITH_INPUT_SIMULATOR
	// ABOVE the no-manager bail below, and that placement is the whole point: the
	// autosave counter is a PROCESS global, and a batched run that has just
	// force-loaded FrontEnd reaches this function with no manager to resolve. Reset
	// after the early return and the next test inherits the previous test's count.
	ZM_ResetAutosaveForTests();
#endif

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
	// The SC3 session members ResetTransitionState deliberately does not touch,
	// because the arrival tail records them and then calls it. Between tests they
	// must go, or a test that never warps inherits the previous test's arrival tag
	// and its pending autosave fires into the next test's game state.
	pxManager->m_xPendingResume = ZM_WorldPosition();
	pxManager->m_bArrivalAutosavePending = false;
	std::memset(pxManager->m_szLastArrivedSpawnTag, 0,
		sizeof(pxManager->m_szLastArrivedSpawnTag));
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
	// forward. Re-seed to a new game PLUS the Fernfawn grant. A safe no-op when no
	// manager exists yet.
	//
	// ★★ READ THIS BEFORE "FIXING" THE ASYMMETRY. As of ZM-D-188 this is the ONLY
	// remaining site that grants a starter: the two PRODUCTION sites above -- OnStart's
	// first-boot seed and RequestNewGame's published state -- deliberately do NOT, so a
	// real player leaves home partyless and chooses at the lab. This one still does,
	// and the difference is not an oversight:
	//
	//   * IT IS TEST-ONLY. Nothing production calls it; its whole job is to restore the
	//     D4 batch FIXTURE (one full-health L5 Fernfawn) that ~60 windowed tests were
	//     written against. The harness fires it before EVERY test including the first
	//     (Zenith_AutomatedTest.cpp: FlushFirstFrameOnStart -> BetweenTests ->
	//     FireBetweenTestsHooks -> ResetSimulatorAndCallSetup), so it -- not OnStart --
	//     is what those tests actually observe.
	//   * IT CANNOT MASK THE PRODUCTION CHANGE. Any test that drives the real New Game
	//     entry gets RequestNewGame's partyless state written straight over this seed,
	//     which is exactly what ZM_IntroBeat_Test relies on: it asserts an EMPTY party
	//     at PlayerHome, so re-adding a grant to RequestNewGame reds it immediately.
	//   * THE ALTERNATIVE IS WORSE. Dropping the grant here would make ~60 unrelated
	//     tests start partyless -- several of them call ZM_Party::Get(0u), which
	//     Zenith_Asserts and therefore BREAKS THE PROCESS in every configuration --
	//     for no gameplay benefit whatsoever.
	//
	// If a future change needs the fixture partyless, that is a per-test decision made
	// in the tests, not a quiet edit here.
	ZM_GameState* pxGameState = nullptr;
	if (TryGetGameState(pxGameState))
	{
		*pxGameState = ZM_MakeNewGameState();
		ZM_ApplyStarterChoice(*pxGameState, ZM_STARTER_CHOICE_FERNFAWN);
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
	const bool bHasUniquePlayer =
		FindUniquePlayerInScene(xPlayerEntityID);
	Zenith_Entity xPlayerEntity = bHasUniquePlayer
		? g_xEngine.Scenes().ResolveEntity(xPlayerEntityID)
		: Zenith_Entity();
	ZM_PlayerController* pxPlayerController = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<ZM_PlayerController>()
		: nullptr;

	// FrontEnd is the sole playerless source scene: choosing New Game uses the
	// same validated transition path into PlayerHome (ZM-D-176) before a Player
	// exists -- the destination moved, this branch did not. Any
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
	// The playerless-DESTINATION mirror of the playerless-SOURCE branch above, and
	// the one place it is latched. Set ONLY here, on the accept path, and cleared
	// only by ResetTransitionState: every refusal above returns before this line, so
	// a refused warp cannot clobber the flag of a transition that is already in
	// flight. Compared against the literal index for the same reason the source
	// branch does -- ZM_GetWorldSpec asserts fatally on an unresolvable id, and
	// reaching it here would put an assert on a caller-supplied build index.
	m_bTargetIsPlayerless = (uTargetBuildIndex == uFRONTEND_BUILD_INDEX);
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
	// Both belong to ONE transition and must not survive it: a cancelled resume must
	// not apply its pose to whatever warp comes next, and a stale playerless flag
	// would skip the spawn and camera barriers for a destination that has both.
	// m_szLastArrivedSpawnTag and m_bArrivalAutosavePending are NOT cleared here --
	// the arrival tail records them and then calls this function.
	m_bResumePending = false;
	m_bTargetIsPlayerless = false;
	// S8 item 2. The latch belongs to ONE transition and must not survive it, exactly
	// like the two flags above: a stale timed-out latch would make the NEXT warp skip
	// every barrier and fade in on an unplaced player.
	//
	// ★ ZEROING THE COUNTER HERE IS BOOKKEEPING, NOT THE MECHANISM. The mechanism is
	// the state-change comparison in TrackTransitionStateDwell, which is what makes
	// every OTHER assignment site safe without a reset of its own -- do not read this
	// line as licence to add per-site resets. Keeping m_ePreviouslyPolledState in step
	// with the IDLE we just wrote also stops the next update logging a phantom
	// "left <state>" measurement for a transition that was CANCELLED, not completed.
	m_bTransitionTimedOut = false;
	m_uFramesInTransitionState = 0u;
	m_ePreviouslyPolledState = m_eTransitionState;
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

u_int ZM_GameStateManager::GetTransitionTimeoutFrames(
	ZM_WARP_TRANSITION_STATE eState)
{
	switch (eState)
	{
	case ZM_WARP_TRANSITION_WAITING_FOR_SCENE:
		return uWARP_TIMEOUT_FRAMES_WAITING_FOR_SCENE;
	case ZM_WARP_TRANSITION_WAITING_FOR_SPAWN:
		return uWARP_TIMEOUT_FRAMES_WAITING_FOR_SPAWN;
	case ZM_WARP_TRANSITION_WAITING_FOR_CAMERA:
		return uWARP_TIMEOUT_FRAMES_WAITING_FOR_CAMERA;
	// The three non-polling states, listed rather than swept into `default`, so
	// adding a seventh transition state is a compiler-visible decision here.
	case ZM_WARP_TRANSITION_IDLE:
	case ZM_WARP_TRANSITION_QUEUED:
	case ZM_WARP_TRANSITION_FADING_IN:
	default:
		// 0 == "no timeout". QUEUED and FADING_IN are driven by AdvanceFadeAlpha,
		// which is monotonic in delta time and therefore cannot stall; IDLE is not a
		// transition at all.
		return 0u;
	}
}

void ZM_GameStateManager::TrackTransitionStateDwell()
{
	if (m_eTransitionState == m_ePreviouslyPolledState)
	{
		// IDLE accumulates here too, and deliberately goes unguarded: IDLE's budget is
		// 0, so the count is never consulted, and an unsigned wrap after ~2 years of
		// continuous 60 Hz idling changes nothing. Special-casing it would be one more
		// state name to keep in step with the enum for no behavioural gain.
		++m_uFramesInTransitionState;
		return;
	}

	// The state MOVED since the previous polled update. Before resetting, report what
	// the barrier we just left actually cost.
	//
	// ★ THIS IS THE MEASUREMENT THE PROVISIONAL BUDGETS ARE MEANT TO BE TIGHTENED
	// FROM. It is at Zenith_Log (INFO) level, not warning: a successful transition is
	// not a problem, it is DATA. Nothing else in the machine makes a successful
	// barrier exit observable at all, so without this line the only way to pick a
	// tighter budget would be a second guess -- which is how a timeout ends up firing
	// on a slow cold scene load and becoming worse than the hang it replaced.
	if (GetTransitionTimeoutFrames(m_ePreviouslyPolledState) != 0u)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"[ZM GameStateManager] [Warp] left %s after %u frame(s) of a %u frame "
			"budget -> %s (target build index %u, spawn tag \"%s\")",
			TransitionStateToString(m_ePreviouslyPolledState),
			m_uFramesInTransitionState,
			GetTransitionTimeoutFrames(m_ePreviouslyPolledState),
			TransitionStateToString(m_eTransitionState),
			m_uTargetBuildIndex,
			m_szTargetSpawnTag);
	}

	m_uFramesInTransitionState = 0u;
	m_ePreviouslyPolledState = m_eTransitionState;
}

void ZM_GameStateManager::ApplyTransitionTimeoutIfExpired()
{
	const u_int uBudgetFrames = GetTransitionTimeoutFrames(m_eTransitionState);
	if (uBudgetFrames == 0u || m_uFramesInTransitionState < uBudgetFrames)
	{
		return;
	}

	// ★★ Zenith_Error, NOT Zenith_Assert, AND THAT IS DELIBERATE. Zenith_Assert calls
	// Zenith_DebugBreak() in EVERY configuration (Zenith.h defines ZENITH_ASSERT
	// unconditionally), and a cold or half-authored tree is a legitimate developer
	// state -- a missing spawn marker on a scene somebody is midway through authoring
	// would break every cold box and take the unit gate down with it. Loud, and
	// non-fatal.
	Zenith_Error(LOG_CATEGORY_GAMEPLAY,
		"[ZM GameStateManager] [Warp] TIMED OUT in %s after %u frames (budget %u): "
		"target build index %u, spawn tag \"%s\", %u load request(s) issued this "
		"session. %s IsWarpDestinationValid only consults the compiled ZM_WorldSpec "
		"world table, so this warp was ACCEPTED at issue time and the fault is in the "
		"DESTINATION, not in the request. Escaping to FADING_IN -- this is DIAGNOSIS, "
		"NOT RECOVERY: the screen comes back so the broken destination is VISIBLE "
		"instead of a permanent black screen with the player frozen.",
		TransitionStateToString(m_eTransitionState),
		m_uFramesInTransitionState,
		uBudgetFrames,
		m_uTargetBuildIndex,
		m_szTargetSpawnTag,
		m_uIssuedLoadRequestCount,
		TransitionTimeoutLikelyCause(m_eTransitionState));

	// ★ THE ESCAPE MIRRORS THE PLAYERLESS-DESTINATION PRECEDENT IN
	// PollForSpawnAndPlacePlayer RATHER THAN INVENTING A NEW ONE: clear the frozen
	// player id, jump straight to FADING_IN. There is no abort-back to offer --
	// IssueSingleLoad is SCENE_LOAD_SINGLE, so by WAITING_FOR_SPAWN the source scene
	// is already gone -- and a missing spawn marker cannot be conjured at runtime.
	//
	// m_bTransitionTimedOut is what makes the jump actually escape. AdvanceFadeIn
	// re-carries all three of these barriers (an active target scene, a resolvable
	// frozen player, a ready follow camera) and bounces back into whichever one fails;
	// since that bounce is a state CHANGE, it would also reset the counter, turning
	// the escape into a permanent black screen that logs one error per budget forever.
	// The latch makes AdvanceFadeIn take the same unconditional path the playerless
	// destination takes, so the screen comes back exactly ONCE per timed-out warp.
	m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
	m_bTransitionTimedOut = true;
	m_eTransitionState = ZM_WARP_TRANSITION_FADING_IN;
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

	// FrontEnd is the sole playerless DESTINATION: it authors no Player, no
	// ZM_SpawnPoint and no ZM_FollowCamera, so every barrier below would spin
	// forever on a fully opaque screen. Jump straight to FADING_IN rather than to
	// WAITING_FOR_CAMERA -- PollForCameraAndBeginFadeIn carries the SAME two
	// barriers (a resolvable frozen player and a ready follow camera) and would
	// wedge identically. There is no frozen player to remember.
	if (m_bTargetIsPlayerless)
	{
		m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
		m_eTransitionState = ZM_WARP_TRANSITION_FADING_IN;
		return;
	}

	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	if (!FindUniquePlayerInScene(xPlayerEntityID))
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
	const Zenith_Maths::Vector3 xSpawnCenter =
		CalculateSpawnPosition(xMarkerFeetPosition);
	const Zenith_PhysicsBodyID xPlayerBodyID = pxPlayerCollider->GetBodyID();
	xPhysics.TeleportBody(xPlayerBodyID, xSpawnCenter);
	xPhysics.SetLinearVelocity(xPlayerBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetAngularVelocity(xPlayerBodyID, Zenith_Maths::Vector3(0.0f));

	// TRANSFORM-FIRST, SPAWN-TAG FALLBACK, in exactly this position. The marker
	// teleport above IS the fallback and has just run; a latched resume now
	// overrides it. It cannot move earlier -- TeleportBody writes IDENTITY rotation,
	// so a pose written before it loses its yaw -- and it must stay before the
	// camera barrier so ZM_FollowCamera snaps its spring to the FINAL pose instead
	// of springing across the correction.
	ApplyPendingResumePlacement(xPlayerEntityID);

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
	// ★ THE TIMED-OUT ESCAPE SKIPS THIS BOUNCE, AND IT MUST. A WAITING_FOR_SCENE
	// expiry means precisely that the target scene never became active -- so this very
	// check would throw the machine straight back into the barrier that just expired.
	// Because that bounce is a state CHANGE, TrackTransitionStateDwell would reset the
	// counter too, and the "escape" would degrade into a permanent black screen that
	// logs one error every 3600 frames instead of a permanent black screen that logs
	// nothing. The same reasoning covers the player and camera barriers below.
	if (!m_bTransitionTimedOut && !IsTargetSceneActive())
	{
		m_fFadeAlpha = fFADE_OPAQUE;
		ApplyFadeVisual();
		m_eTransitionState = ZM_WARP_TRANSITION_WAITING_FOR_SCENE;
		return;
	}

	// The playerless-destination branch, and it is MANDATORY rather than an
	// optimisation. Below this point are TWO INDEPENDENT barriers that FrontEnd can
	// never satisfy: TryResolveFrozenTargetPlayer needs a unique bodied Player and
	// on failure forces the screen opaque and bounces the state back to
	// WAITING_FOR_SPAWN (which bounces straight back here) -- and, separately,
	// HasUniqueReadyFollowCamera needs a ZM_FollowCamera that is the scene's main
	// camera. Patching only PollForSpawnAndPlacePlayer leaves quit-to-title
	// ping-ponging forever on a black screen.
	// It also must not fall through to the unconditional
	// pxPlayerController->SetMovementEnabled(false) below, which would dereference
	// null.
	// ★ AND the S8 TIMED-OUT escape, which needs this branch for the same structural
	// reason FrontEnd does: the two barriers below are exactly the ones that just
	// expired, so re-imposing them would wedge the escape identically.
	if (m_bTargetIsPlayerless || m_bTransitionTimedOut)
	{
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

		// ★ A TIMED-OUT TRANSITION DOES NOT RUN THE ARRIVAL TAIL, AND THAT IS THE
		// POINT OF SPLITTING HERE. RecordArrivalAndLatchAutosave writes
		// m_szLastArrivedSpawnTag, can SET AN INTRO STORY FLAG, and arms a milestone
		// autosave -- it would persist a lie, because the player did NOT arrive: the
		// destination never produced the marker, player or camera the transition was
		// waiting on. ResetTransitionState(TRUE) instead ends the transition the way
		// the cancel path does, releasing the frozen player so the broken destination
		// is not merely visible but walkable, which is what makes it diagnosable.
		if (m_bTransitionTimedOut)
		{
			ResetTransitionState(true);
			return;
		}

		// Same tail as the playerful path, minus the controller work there is no
		// controller to do. The autosave latched here is refused by policy on arrival
		// (FrontEnd is not an overworld, so ResolveLiveSaveBlocker reports
		// NOT_OVERWORLD) -- the latch is set anyway so there is exactly ONE arrival
		// tail, not two that can drift.
		RecordArrivalAndLatchAutosave();
		ResetTransitionState(false);
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
	// Record BEFORE ResetTransitionState -- it memsets m_szTargetSpawnTag, which is
	// the only place the arrived tag exists. And this is hung off the SUCCESS tail
	// itself, never off ResetTransitionState(bEnableFrozenPlayer == true): the
	// successful path passes FALSE, and true is the cancel/test-reset path only.
	RecordArrivalAndLatchAutosave();
	ResetTransitionState(false);
}

void ZM_GameStateManager::RecordArrivalAndLatchAutosave()
{
	// Copy the WHOLE fixed-width field, not strlen bytes: m_szTargetSpawnTag is
	// always zero-padded to 32 bytes by TryQueueWarp, so this carries the padding
	// with it and m_szLastArrivedSpawnTag can never end up unterminated.
	static_assert(sizeof(m_szLastArrivedSpawnTag) == sizeof(m_szTargetSpawnTag),
		"the arrived-tag buffer must mirror the target-tag buffer exactly");
	std::memcpy(m_szLastArrivedSpawnTag, m_szTargetSpawnTag,
		sizeof(m_szLastArrivedSpawnTag));

	// ---- S8 item 1: the intro beat's two ARRIVAL flags ------------------------
	//
	// ★ THIS IS THE FIRST THING IN PRODUCTION THAT HAS EVER SET A STORY FLAG.
	// ZM_STORY_FLAG_INTRO_LEFT_HOME / MET_PROFESSOR / STARTER_RECEIVED have existed
	// since the registry landed with nothing writing any of them; the first two are
	// written here and the third by ZM_UI_MenuStack::ApplyStarterGrant.
	//
	// ★ IT RIDES THE EXISTING TAIL AND ARMS NO NEW AUTOSAVE. The write happens
	// BEFORE m_bArrivalAutosavePending is latched below, so the SCENE_ENTERED autosave
	// that this same arrival schedules persists the flag for free on a later frame.
	// ZM_AUTOSAVE_TRIGGER_STORY_FLAG_SET stays declared-but-not-live: shipping its
	// rising-edge resolver would be new production surface with a second producer, and
	// nothing here needs it.
	//
	// ★ IDEMPOTENT BY CONSTRUCTION. ZM_SetStoryFlag on an already-set bit is a no-op
	// and does not move a single save byte, so walking in and out of the lab ten times
	// costs nothing -- there is no "have I done this already" latch to get wrong.
	//
	// The DECISION is the pure ZM_IntroStoryFlagForArrival (Components/ZM_Interactable.h);
	// this site only performs it. The tag it reads is m_szTargetSpawnTag, which
	// ResetTransitionState memsets moments from now -- another reason this belongs in
	// the tail rather than anywhere downstream of it.
	const ZM_STORY_FLAG_ID eArrivalFlag =
		ZM_IntroStoryFlagForArrival(m_uTargetBuildIndex, m_szLastArrivedSpawnTag);
	if (eArrivalFlag != ZM_STORY_FLAG_NONE)
	{
		ZM_SetStoryFlag(m_xGameState, eArrivalFlag, true);
	}

	m_bArrivalAutosavePending = true;
}

void ZM_GameStateManager::ApplyPendingResumePlacement(Zenith_EntityID xPlayerEntityID)
{
	if (!m_bResumePending)
	{
		return;
	}
	// The latch is DELIBERATELY NOT cleared here -- it dies with the transition, in
	// ResetTransitionState. This function is NOT called once per transition: both
	// AdvanceFadeIn and PollForCameraAndBeginFadeIn push the state back to
	// WAITING_FOR_SPAWN when the frozen player id no longer matches the unique
	// player, and PollForSpawnAndPlacePlayer then re-runs the marker TeleportBody
	// and calls back in here. Spending the latch on the first attempt would let that
	// second marker teleport stand as the final placement, silently landing the
	// player on the default spawn with nothing in the log to say why.
	// There is no retry-forever risk: ResetTransitionState clears m_bResumePending
	// on the success tail AND on the cancel/test-reset path, and every entry below
	// re-validates the pose, so a permanently unusable resume simply keeps failing
	// closed to marker placement until the transition ends.

	// Re-validated rather than trusted. m_xPendingResume was checked at
	// RequestResume time, but a scene load happened in between and this is the
	// last gate before a pose reaches a physics body; a NaN here would corrupt the
	// simulation, not just misplace the player.
	if (!ZM_IsResumeTransformUsable(m_xPendingResume))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM GameStateManager] resume pose is unusable -- keeping the spawn marker "
			"placement");
		return;
	}

	Zenith_Entity xPlayerEntity =
		g_xEngine.Scenes().ResolveEntity(xPlayerEntityID);
	Zenith_ColliderComponent* pxPlayerCollider = xPlayerEntity.IsValid()
		? xPlayerEntity.TryGetComponent<Zenith_ColliderComponent>()
		: nullptr;
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (pxPlayerCollider == nullptr
		|| !pxPlayerCollider->HasValidBody()
		|| !xPhysics.HasActiveSimulation())
	{
		return;
	}

	// SetBodyPosition, NOT TeleportBody: TeleportBody forces identity rotation, so
	// using it here would throw away the yaw two lines later. The saved value is the
	// capsule CENTRE, which is exactly what SetBodyPosition expects and what
	// CaptureWorldPosition read.
	const Zenith_PhysicsBodyID xPlayerBodyID = pxPlayerCollider->GetBodyID();
	xPhysics.SetBodyPosition(xPlayerBodyID, Zenith_Maths::Vector3(
		m_xPendingResume.m_afPosition[0],
		m_xPendingResume.m_afPosition[1],
		m_xPendingResume.m_afPosition[2]));
	// Rotation AFTER position. It survives ZM_PlayerController's per-frame
	// Zenith_Physics::EnforceUpright because that call rebuilds a Y-axis-only
	// quaternion from the body's own heading -- it flattens pitch and roll and
	// PRESERVES yaw, which is why restoring the facing is a real contract here and
	// not a best effort.
	xPhysics.SetBodyRotation(xPlayerBodyID, ZM_RotationFromYaw(m_xPendingResume.m_fYaw));
	xPhysics.SetLinearVelocity(xPlayerBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetAngularVelocity(xPlayerBodyID, Zenith_Maths::Vector3(0.0f));

	// Commit the pose we just wrote into the owning entity's transform cache, THIS
	// frame. Zenith_Physics::TeleportBody fires the body-pose-changed hook (which
	// ends in exactly this call -- Zenith_PhysicsWorldHooksInstall.cpp:32), but
	// SetBodyPosition/SetBodyRotation deliberately do NOT: the leaf's contract
	// (Zenith_PhysicsWorldHooks.h) is that those two sit downstream of a
	// Zenith_TransformComponent setter that has already invalidated the cache, and
	// this call site drives the body directly. Without this the cache keeps the
	// MARKER pose the teleport above committed until the next post-physics sweep, so
	// the follow camera and the renderer read the pre-correction pose for a frame.
	// This introduces no new ordering: the marker TeleportBody a few lines earlier in
	// PollForSpawnAndPlacePlayer already reaches this same function on this same
	// entity in this same update. It is also re-entrancy-free -- it only reads the
	// body, writes the cached pose and bumps a revision counter; it calls no physics
	// mutator and dispatches no event.
	if (Zenith_TransformComponent* pxPlayerTransform =
		xPlayerEntity.TryGetComponent<Zenith_TransformComponent>())
	{
		pxPlayerTransform->SyncPhysicsPoseAndInvalidate();
	}
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
	if (!FindUniquePlayerInScene(xPlayerEntityID)
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
	Zenith_EntityID& xPlayerEntityIDOut)
{
	xPlayerEntityIDOut = INVALID_ENTITY_ID;
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
		return false;
	}

	return true;
}
