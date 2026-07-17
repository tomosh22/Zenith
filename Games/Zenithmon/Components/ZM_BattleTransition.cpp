#include "Zenith.h"

#include "Zenithmon/Components/ZM_BattleTransition.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/UI/ZM_FadeOverlay.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>

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

	// SC3b is deliberately SCENE-INERT: the latched encounter is recorded and
	// observable, but nothing loads, pauses, or switches scenes and the state
	// never leaves IDLE. SC4 wires the state machine onto this drain.
	if (s_bPendingEncounter)
	{
		m_eBattleSpecies = s_ePendingSpecies;
		m_uBattleLevel = s_uPendingLevel;
		m_eSourceScene = s_ePendingScene;
		++m_uObservedEncounterCount;
		s_bPendingEncounter = false;
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

bool ZM_BattleTransition::RequestBattleEnd()
{
	// SC4 wires the machine; SC3b never reaches IN_BATTLE, so there is never a
	// battle to end.
	return false;
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
