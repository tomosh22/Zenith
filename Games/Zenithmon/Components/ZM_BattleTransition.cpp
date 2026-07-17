#include "Zenith.h"

#include "Zenithmon/Components/ZM_BattleTransition.h"

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
// ZM_GameStateManager.h is included HERE and deliberately NEVER in
// ZM_BattleTransition.h: ZM_GameStateManager.cpp already includes this
// component's header, so keeping the dependency in the .cpp is what keeps it
// one-directional.
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Source/UI/ZM_FadeOverlay.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>
#include <string>

namespace
{
	constexpr float fFADE_OPAQUE = 1.0f;
	constexpr float fFADE_TRANSPARENT = 0.0f;
}

#ifdef ZENITH_TOOLS
namespace
{
	const char* TransitionStateToString(ZM_BATTLE_TRANSITION_STATE eState)
	{
		switch (eState)
		{
		case ZM_BATTLE_TRANSITION_IDLE: return "IDLE";
		case ZM_BATTLE_TRANSITION_FADING_OUT: return "FADING_OUT";
		case ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE: return "WAITING_FOR_SCENE";
		case ZM_BATTLE_TRANSITION_ENTERING: return "ENTERING";
		case ZM_BATTLE_TRANSITION_FADING_IN: return "FADING_IN";
		case ZM_BATTLE_TRANSITION_IN_BATTLE: return "IN_BATTLE";
		case ZM_BATTLE_TRANSITION_FADING_TO_OVERWORLD: return "FADING_TO_OVERWORLD";
		case ZM_BATTLE_TRANSITION_RESUMING: return "RESUMING";
		case ZM_BATTLE_TRANSITION_RESUME_FADING_IN: return "RESUME_FADING_IN";
		default: return "INVALID";
		}
	}
}
#endif

Zenith_EntityID ZM_BattleTransition::s_xSingletonEntityID = INVALID_ENTITY_ID;
Zenith_EventHandle ZM_BattleTransition::s_uEncounterSubscription = INVALID_EVENT_HANDLE;
bool ZM_BattleTransition::s_bTransitionActive = false;
bool ZM_BattleTransition::s_bPendingEncounter = false;
bool ZM_BattleTransition::s_bBattleEndRequested = false;
ZM_SPECIES_ID ZM_BattleTransition::s_ePendingSpecies = ZM_SPECIES_NONE;
u_int ZM_BattleTransition::s_uPendingLevel = 0u;
ZM_SCENE_ID ZM_BattleTransition::s_ePendingScene = ZM_SCENE_NONE;

ZM_BattleTransition::ZM_BattleTransition(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_BattleTransition::OnStart()
{
	const Zenith_EntityID xOwnEntityID = m_xParentEntity.GetEntityID();
	if (s_xSingletonEntityID == xOwnEntityID)
	{
		return;
	}

	Zenith_Entity xExisting =
		g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	if (xExisting.IsValid()
		&& xExisting.TryGetComponent<ZM_BattleTransition>() != nullptr)
	{
		m_xParentEntity.Destroy();
		return;
	}

	s_xSingletonEntityID = xOwnEntityID;
	m_eState = ZM_BATTLE_TRANSITION_IDLE;
	m_fFadeAlpha = 0.0f;
	ApplyFadeVisual();

	// Subscribe with a STATIC function pointer: this component relocates on pool
	// swap-and-pop and on the DontDestroyOnLoad move below, so a `this`-capturing
	// callback would be left bound to a freed address.
	if (s_uEncounterSubscription == INVALID_EVENT_HANDLE)
	{
		s_uEncounterSubscription = Zenith_EventDispatcher::Get()
			.Subscribe<ZM_OnWildEncounter>(&ZM_BattleTransition::OnWildEncounterEvent);
	}

	// Moving to the persistent scene relocates this component's pool entry.
	// Nothing may access `this` after the call -- keep it LAST.
	m_xParentEntity.DontDestroyOnLoad();
}

void ZM_BattleTransition::OnUpdate(float fDeltaTime)
{
	if (!IsAuthoritativeSingleton()
		|| !std::isfinite(fDeltaTime)
		|| fDeltaTime <= 0.0f)
	{
		return;
	}

	// The fade is the safety boundary, exactly as for the warp machine: if the
	// persistent root loses its authored overlay, lock every non-idle state opaque
	// rather than reveal a half-built arena or a half-restored overworld.
	if (!ApplyFadeVisual())
	{
		if (OwnsFade(m_eState))
		{
			m_fFadeAlpha = fFADE_OPAQUE;
		}
		return;
	}

	switch (m_eState)
	{
	case ZM_BATTLE_TRANSITION_IDLE:
		AcceptPendingEncounter();
		break;
	case ZM_BATTLE_TRANSITION_FADING_OUT:
		AdvanceFadeOut(fDeltaTime, ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE);
		break;
	case ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE:
		if (AdvancePollDeadline(fDeltaTime, "the additive Battle load never landed"))
		{
			break;
		}
		PollForBattleScene();
		break;
	case ZM_BATTLE_TRANSITION_ENTERING:
		if (AdvancePollDeadline(fDeltaTime, "the battle arena/camera never became ready"))
		{
			break;
		}
		if (!m_bBattleEntered)
		{
			EnterBattleOnce();   // one-shot; poll from the NEXT frame
			break;
		}
		PollForBattleReady();
		break;
	case ZM_BATTLE_TRANSITION_FADING_IN:
		AdvanceFadeIn(fDeltaTime, ZM_BATTLE_TRANSITION_IN_BATTLE);
		break;
	case ZM_BATTLE_TRANSITION_IN_BATTLE:
		// Item 4 (ZM_BattleDirector) owns battle resolution and calls
		// RequestBattleEnd(). There is deliberately NO timer here.
		if (s_bBattleEndRequested)
		{
			s_bBattleEndRequested = false;
			m_eState = ZM_BATTLE_TRANSITION_FADING_TO_OVERWORLD;
		}
		break;
	case ZM_BATTLE_TRANSITION_FADING_TO_OVERWORLD:
		AdvanceFadeOut(fDeltaTime, ZM_BATTLE_TRANSITION_RESUMING);
		break;
	case ZM_BATTLE_TRANSITION_RESUMING:
		ResumeOverworld(true);
		break;
	case ZM_BATTLE_TRANSITION_RESUME_FADING_IN:
		AdvanceFadeIn(fDeltaTime, ZM_BATTLE_TRANSITION_IDLE);
		break;
	default:
		break;
	}
}

void ZM_BattleTransition::OnDestroy()
{
	if (s_xSingletonEntityID != m_xParentEntity.GetEntityID())
	{
		return;
	}

	if (s_uEncounterSubscription != INVALID_EVENT_HANDLE)
	{
		Zenith_EventDispatcher::Get().Unsubscribe(s_uEncounterSubscription);
		s_uEncounterSubscription = INVALID_EVENT_HANDLE;
	}
	s_bTransitionActive = false;
	s_bPendingEncounter = false;
	s_bBattleEndRequested = false;
	s_xSingletonEntityID = INVALID_ENTITY_ID;
}

void ZM_BattleTransition::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
}

void ZM_BattleTransition::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;

	// Reset-first (the ZM_WarpTrigger::ClearConfiguration idiom): a serialized
	// component never carries live transition state, and an unrecognised version
	// must leave a fully-defaulted component behind rather than whatever the
	// instance happened to hold.
	m_xOverworldScene = Zenith_Scene();
	m_xBattleScene = Zenith_Scene();
	m_xParkedPlayerEntityID = INVALID_ENTITY_ID;
	m_xParkedPlayerPosition = Zenith_Maths::Vector3(0.0f);
	m_eState = ZM_BATTLE_TRANSITION_IDLE;
	m_eBattleSpecies = ZM_SPECIES_NONE;
	m_eSourceScene = ZM_SCENE_NONE;
	m_uBattleLevel = 0u;
	m_uIssuedLoadRequestCount = 0u;
	m_uObservedEncounterCount = 0u;
	m_uCompletedBattleCount = 0u;
	m_uAbortedTransitionCount = 0u;
	m_bBattleEntered = false;
	m_bGrassCleared = false;
	m_fPollSeconds = 0.0f;
	m_fFadeAlpha = 0.0f;

	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}
}

#ifdef ZENITH_TOOLS
void ZM_BattleTransition::RenderPropertiesPanel()
{
	ImGui::Text("Authoritative singleton: %s",
		IsAuthoritativeSingleton() ? "true" : "false");
	ImGui::Text("Transition: %s", TransitionStateToString(m_eState));
	ImGui::Text("Battle species: %u", static_cast<u_int>(m_eBattleSpecies));
	ImGui::Text("Battle level: %u", m_uBattleLevel);
	ImGui::Text("Source scene: %s", ZM_GetSceneName(m_eSourceScene));
	ImGui::Text("Fade alpha: %.3f", m_fFadeAlpha);
	ImGui::Text("Issued load requests: %u", m_uIssuedLoadRequestCount);
	ImGui::Text("Observed encounters: %u", m_uObservedEncounterCount);
	ImGui::Text("Completed battles: %u", m_uCompletedBattleCount);
	ImGui::Text("Aborted transitions: %u", m_uAbortedTransitionCount);
}
#endif

void ZM_BattleTransition::OnWildEncounterEvent(const ZM_OnWildEncounter& xEvent)
{
	// FAIL CLOSED while a round trip is in flight. On the frame SetScenePaused
	// lands, the overworld is still inside Zenith_SceneSystem::Update's snapshot,
	// so ZM_TallGrassSystem can dispatch one more stray encounter; a stale latch
	// would re-enter the battle the instant we returned to IDLE.
	if (s_bTransitionActive || s_bPendingEncounter)
	{
		return;
	}
	if (!IsEncounterPayloadValid(xEvent.m_eSpecies, xEvent.m_uLevel, xEvent.m_eSourceScene))
	{
		return;
	}

	s_bPendingEncounter = true;
	s_ePendingSpecies = xEvent.m_eSpecies;
	s_uPendingLevel = xEvent.m_uLevel;
	s_ePendingScene = xEvent.m_eSourceScene;
}

bool ZM_BattleTransition::IsEncounterPayloadValid(
	ZM_SPECIES_ID eSpecies,
	u_int uLevel,
	ZM_SCENE_ID eScene)
{
	// The range check COVERS ZM_SPECIES_NONE: ZM_SpeciesData.h:342-343 declares
	// ZM_SPECIES_COUNT then ZM_SPECIES_NONE = ZM_SPECIES_COUNT, so they share a
	// value and a separate sentinel clause would be dead code.
	if (static_cast<u_int>(eSpecies) >= static_cast<u_int>(ZM_SPECIES_COUNT))
	{
		return false;
	}
	return uLevel >= 1u && uLevel <= 100u && IsSceneEligibleForBattle(eScene);
}

bool ZM_BattleTransition::IsSceneEligibleForBattle(ZM_SCENE_ID eScene)
{
	if (eScene >= ZM_SCENE_COUNT)
	{
		return false;   // covers ZM_SCENE_NONE (ZM_WorldSpec.h:54-55 share a value)
	}

	const ZM_SCENE_KIND eKind = ZM_GetWorldSpec(eScene).m_eKind;
	// The battle scene cannot spawn a battle over itself, and the FrontEnd has no
	// world to pause or resume.
	return eKind != ZM_SCENE_KIND_BATTLE && eKind != ZM_SCENE_KIND_FRONTEND;
}

ZM_BATTLE_BIOME ZM_BattleTransition::BiomeForScene(ZM_SCENE_ID eScene)
{
	// S5 v1 mapping. ZM_WorldSpec carries no biome column yet; index by
	// ZM_SCENE_ID, MEADOW for anything unmapped/out of range. One row per
	// enumerator, in ZM_SCENE_ID order -- appending a scene appends a row.
	static const ZM_BATTLE_BIOME ls_aeBiome[ZM_SCENE_COUNT] =
	{
		ZM_BATTLE_BIOME_MEADOW,    // ZM_SCENE_FRONTEND   (never eligible; MEADOW default)
		ZM_BATTLE_BIOME_MEADOW,    // ZM_SCENE_BATTLE     (never eligible; MEADOW default)
		ZM_BATTLE_BIOME_MEADOW,    // ZM_SCENE_DAWNMERE
		ZM_BATTLE_BIOME_WETLAND,   // ZM_SCENE_THORNACRE
		ZM_BATTLE_BIOME_MEADOW,    // ZM_SCENE_ROUTE1
		ZM_BATTLE_BIOME_MEADOW,    // ZM_SCENE_PLAYERHOME
		ZM_BATTLE_BIOME_MEADOW,    // ZM_SCENE_PROFLAB
		ZM_BATTLE_BIOME_CANYON,    // ZM_SCENE_GYM1
	};
	return (eScene < ZM_SCENE_COUNT) ? ls_aeBiome[eScene] : ZM_BATTLE_BIOME_MEADOW;
}

bool ZM_BattleTransition::ShouldAcceptBattleEnd(ZM_BATTLE_TRANSITION_STATE eState)
{
	return eState == ZM_BATTLE_TRANSITION_IN_BATTLE;
}

bool ZM_BattleTransition::OwnsFade(ZM_BATTLE_TRANSITION_STATE eState)
{
	return eState != ZM_BATTLE_TRANSITION_IDLE;
}

bool ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_STATE eState)
{
	return eState == ZM_BATTLE_TRANSITION_ENTERING
		|| eState == ZM_BATTLE_TRANSITION_FADING_IN
		|| eState == ZM_BATTLE_TRANSITION_IN_BATTLE
		|| eState == ZM_BATTLE_TRANSITION_FADING_TO_OVERWORLD
		|| eState == ZM_BATTLE_TRANSITION_RESUMING;
}

bool ZM_BattleTransition::RequestBattleEnd()
{
	Zenith_EntityID xID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xID))
	{
		return false;
	}

	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
	ZM_BattleTransition* pxSelf = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_BattleTransition>()
		: nullptr;
	if (pxSelf == nullptr || !ShouldAcceptBattleEnd(pxSelf->m_eState))
	{
		return false;
	}

	s_bBattleEndRequested = true;
	return true;
}

bool ZM_BattleTransition::IsTransitionActive()
{
	return s_bTransitionActive;
}

void ZM_BattleTransition::ResetRuntimeStateForTests()
{
	// Every mutable static EXCEPT s_uEncounterSubscription: the live subscription
	// belongs to the still-live singleton and must survive a between-tests reset
	// (dropping it would silently deafen the game for every later test).
	s_bTransitionActive = false;
	s_bPendingEncounter = false;
	s_bBattleEndRequested = false;
	s_ePendingSpecies = ZM_SPECIES_NONE;
	s_uPendingLevel = 0u;
	s_ePendingScene = ZM_SCENE_NONE;

	Zenith_Entity xTransitionEntity =
		g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	ZM_BattleTransition* pxTransition = xTransitionEntity.IsValid()
		? xTransitionEntity.TryGetComponent<ZM_BattleTransition>()
		: nullptr;
	if (pxTransition == nullptr)
	{
		s_xSingletonEntityID = INVALID_ENTITY_ID;
		return;
	}

	pxTransition->m_eState = ZM_BATTLE_TRANSITION_IDLE;
	pxTransition->m_fFadeAlpha = 0.0f;
	pxTransition->ApplyFadeVisual();
	pxTransition->m_xOverworldScene = Zenith_Scene();
	pxTransition->m_xBattleScene = Zenith_Scene();
	// Release a still-PARKED player before dropping the ID that identifies it,
	// mirroring the warp machine's ResetTransitionState(true). Parking drops
	// gravity and disables input on the live body; clearing the ID alone would
	// strand a floating, uncontrollable player in every later test. A no-op when
	// nothing was parked (the resolve fails closed).
	pxTransition->TryRestoreOverworldPlayer(true);
	pxTransition->m_xParkedPlayerEntityID = INVALID_ENTITY_ID;
	pxTransition->m_xParkedPlayerPosition = Zenith_Maths::Vector3(0.0f);
	pxTransition->m_eBattleSpecies = ZM_SPECIES_NONE;
	pxTransition->m_eSourceScene = ZM_SCENE_NONE;
	pxTransition->m_uBattleLevel = 0u;
	pxTransition->m_uIssuedLoadRequestCount = 0u;
	pxTransition->m_uObservedEncounterCount = 0u;
	pxTransition->m_uCompletedBattleCount = 0u;
	pxTransition->m_uAbortedTransitionCount = 0u;
	pxTransition->m_bBattleEntered = false;
	pxTransition->m_bGrassCleared = false;
	pxTransition->m_fPollSeconds = 0.0f;
}

bool ZM_BattleTransition::TryGetUniqueSingletonEntityID(
	Zenith_EntityID& xEntityIDOut)
{
	xEntityIDOut = INVALID_ENTITY_ID;
	u_int uTransitionCount = 0u;
	g_xEngine.Scenes().QueryAllScenes<ZM_BattleTransition>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_BattleTransition&)
		{
			++uTransitionCount;
			if (uTransitionCount == 1u)
			{
				xEntityIDOut = xEntityID;
			}
		});

	if (uTransitionCount != 1u)
	{
		xEntityIDOut = INVALID_ENTITY_ID;
		return false;
	}
	return g_xEngine.Scenes().ResolveEntity(xEntityIDOut).IsValid();
}

bool ZM_BattleTransition::IsAuthoritativeSingleton() const
{
	return m_xParentEntity.IsValid()
		&& s_xSingletonEntityID == m_xParentEntity.GetEntityID();
}

bool ZM_BattleTransition::ApplyFadeVisual()
{
	return ZM_FadeOverlay::Apply(m_xParentEntity, szFADE_ELEMENT_NAME, m_fFadeAlpha);
}

void ZM_BattleTransition::AcceptPendingEncounter()
{
	if (!s_bPendingEncounter)
	{
		return;
	}
	s_bPendingEncounter = false;

	// Never race the SINGLE-warp machine for the screen (only one pending load
	// survives per frame). TryQueueWarp already rejects on IsTransitionActive(),
	// so this closes the other direction.
	if (ZM_GameStateManager::IsWarpInProgress())
	{
		return;
	}

	const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
	const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xActive);
	if (!xInfo.m_bLoaded || xInfo.m_iBuildIndex < 0)
	{
		return;
	}
	if (!IsSceneEligibleForBattle(
		ZM_FindSceneByBuildIndex(static_cast<u_int>(xInfo.m_iBuildIndex))))
	{
		return;
	}

	m_xOverworldScene = xActive;   // generation-bearing handle, NEVER a SceneData*
	m_eBattleSpecies = s_ePendingSpecies;
	m_uBattleLevel = s_uPendingLevel;
	m_eSourceScene = s_ePendingScene;
	++m_uObservedEncounterCount;
	m_fFadeAlpha = fFADE_TRANSPARENT;
	ApplyFadeVisual();
	m_fPollSeconds = 0.0f;
	m_bBattleEntered = false;
	m_bGrassCleared = false;
	s_bTransitionActive = true;    // from here the subscriber + TryQueueWarp fail closed
	m_eState = ZM_BATTLE_TRANSITION_FADING_OUT;
}

void ZM_BattleTransition::AdvanceFadeOut(
	float fDeltaTime,
	ZM_BATTLE_TRANSITION_STATE eNextState)
{
	m_fFadeAlpha = ZM_GameStateManager::AdvanceFadeAlpha(
		m_fFadeAlpha, fFADE_OPAQUE, fDeltaTime);
	if (!ApplyFadeVisual())
	{
		return;
	}
	if (m_fFadeAlpha < fFADE_OPAQUE)
	{
		return;
	}

	if (eNextState == ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE)
	{
		IssueAdditiveBattleLoad();
		return;
	}
	m_fPollSeconds = 0.0f;
	m_eState = eNextState;
}

void ZM_BattleTransition::IssueAdditiveBattleLoad()
{
	// FIRE AND FORGET. This runs inside a component OnUpdate, so the load DEFERS
	// and returns INVALID_SCENE; the engine drains the pending slot at the end of
	// this Update and we poll for the handle next frame. Mirrors the shipped
	// ZM_GameStateManager::IssueSingleLoad: issue ONCE here, poll ONLY in
	// PollForBattleScene.
	++m_uIssuedLoadRequestCount;
	m_fPollSeconds = 0.0f;
	m_eState = ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE;
	g_xEngine.Scenes().LoadSceneByIndex(iBATTLE_BUILD_INDEX, SCENE_LOAD_ADDITIVE);
}

void ZM_BattleTransition::PollForBattleScene()
{
	const Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
		std::string(GAME_ASSETS_DIR) + "Scenes/Battle" + ZENITH_SCENE_EXT);
	if (!xBattle.IsValid())
	{
		return;   // the deadline is owned by OnUpdate
	}

	m_xBattleScene = xBattle;
	m_fPollSeconds = 0.0f;
	m_eState = ZM_BATTLE_TRANSITION_ENTERING;
}

void ZM_BattleTransition::EnterBattleOnce()
{
	// ORDER IS LOAD-BEARING. Runs EXACTLY ONCE per round trip (m_bBattleEntered).
	// 1) Park the player FIRST, while the overworld is still ACTIVE:
	//    TryGetUniqueActiveScenePlayerEntityID is active-scene bound and goes blind
	//    the instant focus moves to Battle. If the player is not resolvable yet,
	//    leave m_bBattleEntered false and retry next frame -- the deadline still
	//    bounds it.
	if (!TryParkOverworldPlayer())
	{
		return;
	}

	// 2) Pause the overworld. This gates ONLY the ECS update dispatch; physics is
	//    global and keeps stepping, which is why step 1 parks the body.
	g_xEngine.Scenes().SetScenePaused(m_xOverworldScene, true);

	// 3) Focus = the camera switch (FindMainCameraEntityAcrossScenes scans the
	//    active scene first, and Battle authors its own main camera). Battle is
	//    NEVER paused: the pause gates pending-Start dispatch, so a paused Battle
	//    would never run ZM_BattleArena::OnStart and the arena would never build.
	if (!g_xEngine.Scenes().SetActiveScene(m_xBattleScene))
	{
		AbortToOverworld("SetActiveScene(Battle) was refused");
		return;
	}

	// 4) Additive loads never hit the engine's SINGLE-only render-reset hook, so
	//    clear the engine-owned grass singleton explicitly. The overworld's
	//    ZM_TallGrassSystem keeps its OWN CPU map, so gameplay sampling is
	//    untouched. m_bGrassCleared is what licenses the matching regenerate on
	//    resume -- no other path may regenerate.
	ClearOverworldGrass();
	m_bGrassCleared = true;
	m_bBattleEntered = true;
	m_fPollSeconds = 0.0f;
}

void ZM_BattleTransition::PollForBattleReady()
{
	// Re-resolved every frame: a component pool may swap-and-pop under us, so an
	// arena pointer must never outlive this call.
	ZM_BattleArena* pxArena = ResolveUniqueBattleArena();
	if (pxArena == nullptr
		|| !pxArena->IsFullyBuilt()
		|| !IsBattleCameraActive(m_xBattleScene))
	{
		return;   // deadline owned by OnUpdate
	}

	pxArena->SetBiome(BiomeForScene(m_eSourceScene));
	m_fPollSeconds = 0.0f;
	m_eState = ZM_BATTLE_TRANSITION_FADING_IN;
}

void ZM_BattleTransition::AdvanceFadeIn(
	float fDeltaTime,
	ZM_BATTLE_TRANSITION_STATE eNextState)
{
	m_fFadeAlpha = ZM_GameStateManager::AdvanceFadeAlpha(
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

	if (eNextState == ZM_BATTLE_TRANSITION_IN_BATTLE)
	{
		s_bBattleEndRequested = false;
		m_eState = ZM_BATTLE_TRANSITION_IN_BATTLE;
		return;
	}

	// Resume complete: hand input back only now, mirroring the warp machine.
	TryRestoreOverworldPlayer(true);
	// m_xParkedPlayerPosition is DELIBERATELY retained -- it is the drift baseline
	// the windowed gate reads via GetParkedPlayerPosition() after IDLE.
	m_xParkedPlayerEntityID = INVALID_ENTITY_ID;
	m_xBattleScene = Zenith_Scene();
	m_xOverworldScene = Zenith_Scene();
	m_eBattleSpecies = ZM_SPECIES_NONE;
	m_uBattleLevel = 0u;
	m_eSourceScene = ZM_SCENE_NONE;
	m_bBattleEntered = false;
	m_bGrassCleared = false;
	s_bPendingEncounter = false;   // drop anything the stray pause-frame tick latched
	s_bTransitionActive = false;
	m_eState = ZM_BATTLE_TRANSITION_IDLE;
}

void ZM_BattleTransition::ResumeOverworld(bool bCompletedBattle)
{
	// ORDER IS LOAD-BEARING: activate the overworld BEFORE unloading Battle, so we
	// never depend on the unload's lowest-build-index reselection (FrontEnd would
	// win if it were ever loaded).
	if (m_xOverworldScene.IsValid())
	{
		g_xEngine.Scenes().SetActiveScene(m_xOverworldScene);
	}
	if (m_xBattleScene.IsValid())
	{
		// Legal: this component lives on the PERSISTENT scene, so this is not the
		// unsupported self-unload-from-own-dispatch case.
		g_xEngine.Scenes().UnloadScene(m_xBattleScene);
		m_xBattleScene = Zenith_Scene();
	}
	if (m_xOverworldScene.IsValid())
	{
		g_xEngine.Scenes().SetScenePaused(m_xOverworldScene, false);
	}

	// Gravity + zeroed velocity; input stays off until the fade completes.
	TryRestoreOverworldPlayer(false);

	// ONLY a path that actually cleared may regenerate: RegenerateForSceneResume
	// ALWAYS clears first, so calling it on an abort that never entered the battle
	// would destroy and rebuild live overworld grass.
	if (m_bGrassCleared)
	{
		RegenerateOverworldGrass();
		m_bGrassCleared = false;
	}

	if (bCompletedBattle)
	{
		++m_uCompletedBattleCount;
	}
	else
	{
		++m_uAbortedTransitionCount;
	}
	m_bBattleEntered = false;
	m_eState = ZM_BATTLE_TRANSITION_RESUME_FADING_IN;
}

void ZM_BattleTransition::AbortToOverworld(const char* szReason)
{
	// LOG_CATEGORY_GAMEPLAY (not UNITTEST): this is shipping runtime code.
	Zenith_Error(LOG_CATEGORY_GAMEPLAY,
		"[ZM_BattleTransition] aborting the battle round trip: %s", szReason);
	ResumeOverworld(false);   // an ABORT, not a completion; skips the grass regen unless it cleared
}

bool ZM_BattleTransition::TryParkOverworldPlayer()
{
	Zenith_EntityID xPlayerID = INVALID_ENTITY_ID;
	if (!ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(xPlayerID))
	{
		return false;
	}

	Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(xPlayerID);
	ZM_PlayerController* pxController = xPlayer.IsValid()
		? xPlayer.TryGetComponent<ZM_PlayerController>()
		: nullptr;
	Zenith_ColliderComponent* pxCollider = xPlayer.IsValid()
		? xPlayer.TryGetComponent<Zenith_ColliderComponent>()
		: nullptr;
	if (pxController == nullptr
		|| pxCollider == nullptr
		|| !pxCollider->HasValidBody())
	{
		return false;
	}

	// PARK, do not merely freeze: the pause stops the ECS dispatch only; physics
	// keeps stepping every body and the transform sync keeps writing the pose back.
	// Zeroing velocity + dropping gravity removes the accumulating force. This is
	// NOT teleportation -- no position is written.
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	xPhysics.SetLinearVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetAngularVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetGravityEnabled(xBodyID, false);
	pxController->SetMovementEnabled(false);
	m_xParkedPlayerEntityID = xPlayerID;
	m_xParkedPlayerPosition = xPhysics.GetBodyPosition(xBodyID);   // the ONLY sound drift baseline
	return true;
}

bool ZM_BattleTransition::TryRestoreOverworldPlayer(bool bEnableMovement)
{
	Zenith_Entity xPlayer =
		g_xEngine.Scenes().ResolveEntity(m_xParkedPlayerEntityID);
	ZM_PlayerController* pxController = xPlayer.IsValid()
		? xPlayer.TryGetComponent<ZM_PlayerController>()
		: nullptr;
	Zenith_ColliderComponent* pxCollider = xPlayer.IsValid()
		? xPlayer.TryGetComponent<Zenith_ColliderComponent>()
		: nullptr;
	if (pxController == nullptr
		|| pxCollider == nullptr
		|| !pxCollider->HasValidBody())
	{
		return false;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	xPhysics.SetLinearVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetAngularVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
	xPhysics.SetGravityEnabled(xBodyID, true);
	if (bEnableMovement)
	{
		pxController->ResetRuntimeState();
		pxController->SetMovementEnabled(true);
	}
	else
	{
		pxController->SetMovementEnabled(false);
	}
	return true;
}

bool ZM_BattleTransition::AdvancePollDeadline(
	float fDeltaTime,
	const char* szReason)
{
	m_fPollSeconds += fDeltaTime;
	if (m_fPollSeconds <= fPOLL_DEADLINE_SECONDS)
	{
		return false;
	}

	AbortToOverworld(szReason);
	return true;
}

bool ZM_BattleTransition::IsBattleCameraActive(Zenith_Scene xBattleScene)
{
	// Used within this call only -- SceneData storage is recyclable.
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xBattleScene);
	if (pxData == nullptr)
	{
		return false;
	}

	const Zenith_EntityID xCameraID = pxData->GetMainCameraEntity();
	return xCameraID.IsValid()
		&& g_xEngine.Scenes().FindMainCameraEntityAcrossScenes() == xCameraID;
}

ZM_BattleArena* ZM_BattleTransition::ResolveUniqueBattleArena()
{
	u_int uCount = 0u;
	ZM_BattleArena* pxFound = nullptr;
	g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().ForEach(
		[&](Zenith_EntityID, ZM_BattleArena& xArena)
		{
			++uCount;
			if (uCount == 1u)
			{
				pxFound = &xArena;
			}
		});
	return (uCount == 1u) ? pxFound : nullptr;   // pointer valid only for this call
}

void ZM_BattleTransition::ClearOverworldGrass()
{
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}
	g_xEngine.Grass().ClearSceneData();
}

void ZM_BattleTransition::RegenerateOverworldGrass()
{
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}

	// QueryAllScenes, NOT QueryActiveScene: under an additive Battle the overworld
	// is loaded but NOT active, so an active-scene query would find nothing.
	g_xEngine.Scenes().QueryAllScenes<ZM_TerrainGrass>().ForEach(
		[](Zenith_EntityID, ZM_TerrainGrass& xGrass)
		{
			xGrass.RegenerateForSceneResume();
		});
}
