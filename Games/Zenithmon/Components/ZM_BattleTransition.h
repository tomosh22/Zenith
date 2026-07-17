#pragma once

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_EventSystem.h"                // Zenith_EventHandle
#include "ZenithECS/Zenith_Scene.h"
#include "Zenithmon/Components/ZM_BattleArena.h"          // ZM_BATTLE_BIOME
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"         // ZM_SPECIES_ID
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"           // ZM_SCENE_ID
#include "Zenithmon/Source/World/ZM_EncounterEvents.h"    // ZM_OnWildEncounter

class Zenith_DataStream;

// The overworld <-> battle round-trip machine (S5 item 3). It subscribes to
// ZM_OnWildEncounter (emitted by ZM_TallGrassSystem, order 109), latches the
// accepted payload, and -- from SC4 -- fades out, additively loads the Battle
// scene (build index 1), pauses/parks the overworld, and reverses all of it when
// item 4 calls RequestBattleEnd(). RequestBattleEnd() is the SOLE exit from
// ZM_BATTLE_TRANSITION_IN_BATTLE: item 4 owns battle resolution, this component
// owns nothing but the transition.
//
// It lives on its OWN persistent entity (ZM_BattleTransitionRoot) with its OWN
// BattleFade overlay, NOT on ZM_GameStateRoot, for two independent reasons:
//   1. ZM_GameStateManager::OnUpdate calls its ApplyFadeVisual() UNCONDITIONALLY
//      every frame, even while IDLE. A second fade owner sharing the WarpFade
//      overlay would have its alpha stomped by the warp machine's idle write on
//      the very next update.
//   2. ZM_GameStateManager::OnStart ends in DontDestroyOnLoad(), a cross-scene
//      move that MOVE-CONSTRUCTS every component on that entity (its own comment
//      says nothing may access the object afterwards). Sharing the entity would
//      relocate this component mid-Start-dispatch, out from under the caller.
// The two overlays never fight for the top of the canvas either: BattleFade is
// authored at sort order 10001, one above WarpFade's 10000 (ZM-D-097).
//
// SC3b SCOPE: this component is deliberately SCENE-INERT. It observes and latches
// encounters and holds the state enum, but OnUpdate touches no scene, issues no
// load, pauses nothing, and never leaves ZM_BATTLE_TRANSITION_IDLE. SC4 wires the
// state machine.
enum ZM_BATTLE_TRANSITION_STATE : u_int
{
	ZM_BATTLE_TRANSITION_IDLE,
	ZM_BATTLE_TRANSITION_FADING_OUT,          // encounter accepted; alpha -> opaque
	ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE,   // additive load issued; poll for the Battle handle
	ZM_BATTLE_TRANSITION_ENTERING,            // one-shot entry done; poll arena/camera readiness
	ZM_BATTLE_TRANSITION_FADING_IN,           // alpha -> transparent
	ZM_BATTLE_TRANSITION_IN_BATTLE,           // item 4 owns resolution; awaits RequestBattleEnd
	ZM_BATTLE_TRANSITION_FADING_TO_OVERWORLD, // alpha -> opaque
	ZM_BATTLE_TRANSITION_RESUMING,            // unload + reactivate + unpause + restore + regrow grass
	ZM_BATTLE_TRANSITION_RESUME_FADING_IN     // alpha -> transparent -> IDLE
};

class ZM_BattleTransition
{
public:
	static constexpr u_int  uSERIALIZATION_VERSION = 1u;
	static constexpr int    iBATTLE_BUILD_INDEX    = 1;
	static constexpr const char* szFADE_ELEMENT_NAME = "BattleFade";

	ZM_BattleTransition() = delete;
	explicit ZM_BattleTransition(Zenith_Entity& xParentEntity);
	ZM_BattleTransition(const ZM_BattleTransition&) = delete;
	ZM_BattleTransition& operator=(const ZM_BattleTransition&) = delete;
	ZM_BattleTransition(ZM_BattleTransition&&) noexcept = default;
	ZM_BattleTransition& operator=(ZM_BattleTransition&&) noexcept = default;

	void OnStart();
	void OnUpdate(float fDeltaTime);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	// ---- pure policy (headless, unit-tested) ----

	// A dispatched encounter payload this component will act on: an in-range
	// species, a level in [1, 100], and a scene a battle may launch from.
	static bool IsEncounterPayloadValid(ZM_SPECIES_ID eSpecies, u_int uLevel, ZM_SCENE_ID eScene);
	// Every scene kind except BATTLE (cannot spawn a battle over itself) and
	// FRONTEND (no world to pause or resume). Out-of-range scenes fail closed.
	static bool IsSceneEligibleForBattle(ZM_SCENE_ID eScene);
	// The arena dressing a battle launched from eScene wears. MEADOW for anything
	// unmapped or out of range.
	static ZM_BATTLE_BIOME BiomeForScene(ZM_SCENE_ID eScene);

	// ---- item 4 seam ----

	// The SOLE exit from ZM_BATTLE_TRANSITION_IN_BATTLE. Returns false when there
	// is no battle to end. SC3b has no state machine to drive, so it always
	// returns false; SC4 wires it.
	static bool RequestBattleEnd();
	// True while a round trip owns the screen. ZM_GameStateManager::TryQueueWarp
	// rejects on it so no warp can race the battle.
	static bool IsTransitionActive();

	// ---- test/tools observation ----
	static void ResetRuntimeStateForTests();
	static bool TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut);
	bool IsAuthoritativeSingleton() const;
	ZM_BATTLE_TRANSITION_STATE GetTransitionState() const { return m_eState; }
	ZM_SPECIES_ID GetBattleSpecies() const { return m_eBattleSpecies; }
	u_int         GetBattleLevel()   const { return m_uBattleLevel; }
	ZM_SCENE_ID   GetSourceScene()   const { return m_eSourceScene; }
	Zenith_Scene  GetBattleScene()   const { return m_xBattleScene; }
	Zenith_Scene  GetOverworldScene() const { return m_xOverworldScene; }
	// The player's body position at the instant it was PARKED -- NOT the position
	// it held before it walked into the grass. The player must walk >= 1 m to cross
	// a tile boundary and trigger the encounter at all, and it keeps moving through
	// the fade-out, so the park-instant position is the only sound baseline against
	// which a resume-drift assertion means anything.
	const Zenith_Maths::Vector3& GetParkedPlayerPosition() const { return m_xParkedPlayerPosition; }
	u_int GetIssuedLoadRequestCount() const { return m_uIssuedLoadRequestCount; }
	u_int GetObservedEncounterCount() const { return m_uObservedEncounterCount; }
	u_int GetCompletedBattleCount()  const { return m_uCompletedBattleCount; }
	u_int GetAbortedTransitionCount() const { return m_uAbortedTransitionCount; }
	float GetFadeAlpha() const { return m_fFadeAlpha; }

private:
	static void OnWildEncounterEvent(const ZM_OnWildEncounter& xEvent);
	bool ApplyFadeVisual();

	static Zenith_EntityID s_xSingletonEntityID;
	static Zenith_EventHandle s_uEncounterSubscription;
	static bool s_bTransitionActive;
	static bool s_bPendingEncounter;
	static bool s_bBattleEndRequested;
	static ZM_SPECIES_ID s_ePendingSpecies;
	static u_int s_uPendingLevel;
	static ZM_SCENE_ID s_ePendingScene;

	Zenith_Entity m_xParentEntity;
	Zenith_Scene  m_xOverworldScene;
	Zenith_Scene  m_xBattleScene;
	Zenith_EntityID m_xParkedPlayerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 m_xParkedPlayerPosition = Zenith_Maths::Vector3(0.0f);
	ZM_BATTLE_TRANSITION_STATE m_eState = ZM_BATTLE_TRANSITION_IDLE;
	ZM_SPECIES_ID m_eBattleSpecies = ZM_SPECIES_NONE;
	ZM_SCENE_ID   m_eSourceScene = ZM_SCENE_NONE;
	u_int m_uBattleLevel = 0u;
	u_int m_uIssuedLoadRequestCount = 0u;
	u_int m_uObservedEncounterCount = 0u;
	u_int m_uCompletedBattleCount = 0u;
	u_int m_uAbortedTransitionCount = 0u;
	bool  m_bBattleEntered = false;   // SC4: one-shot latch for the EnterBattle prefix
	bool  m_bGrassCleared = false;    // SC4: only a path that cleared may regenerate
	float m_fPollSeconds = 0.0f;
	float m_fFadeAlpha = 0.0f;
};
