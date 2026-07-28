#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"   // ZM_BattleDirectorCore, ZM_DIRECTOR_STATE, ZM_BuildWildEnemySpec
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"         // ZM_BattleMonsterSpec
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"           // ZM_BattleConfig, ZM_SIDE
#include "Zenithmon/Source/Data/ZM_ItemData.h"                // ZM_ITEM_ID (SC4 test-only catch-ball override)
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"             // ZM_SPECIES_ID
#include "Zenithmon/Source/Data/ZM_TrainerData.h"             // ZM_TRAINER_ID / ZM_TRAINER_NONE (S7 item 3 SC5)
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"              // ZM_UI_BattleHUD (director-owned battle HUD, SC4)

class Zenith_DataStream;
class ZM_BattleTransition;   // RunSetup takes it by const& (full type included in the .cpp)

// Test-only catch-ball override (S5 item 5 SC4). Mirrors ZM_SetInstantBattlesForTests: a
// process-lifetime seam the RUNNING drive substitutes onto any ITEM (catch) action the
// player submits, so the windowed catch test can force a guaranteed capture with
// ZM_ITEM_PRIMEORB. Default ZM_ITEM_CATCHORB makes the production override a no-op.
void ZM_SetCatchBallForTests(ZM_ITEM_ID eBall);

// The presenter-driver ECS component (S5 item 4 SC3, order 111). It lives on its
// own entity inside the additively-loaded Battle scene (build index 1, world Y ~
// ZM_BattleArena::fARENA_WORLD_Y). Each frame it re-resolves the persistent
// ZM_BattleTransition singleton; once that transition reaches
// ZM_BATTLE_TRANSITION_IN_BATTLE it (one-shot) reads the encounter payload, Begins
// a pure ZM_BattleDirectorCore for a deterministic AI-vs-AI wild battle, places the
// two creature models on the arena platforms (best effort -- a missing bundle never
// aborts the battle), drives the core turn by turn, and calls
// ZM_BattleTransition::RequestBattleEnd() EXACTLY ONCE when the battle resolves.
// It also owns and drives a ZM_UI_BattleHUD (the first visible battle UI; SC4).
// Deterministic under zm_instant_battles. ZM-D-102/103.
enum ZM_BATTLE_DIRECTOR_PHASE : u_int
{
	ZM_BD_WAIT_FOR_IN_BATTLE,   // before the transition reaches IN_BATTLE
	ZM_BD_SETUP,                // one-shot: Begin the core + place the two models
	ZM_BD_RUNNING,              // driving the core turn-by-turn
	ZM_BD_RESOLVED,             // core reached OVER; RequestBattleEnd fired exactly once
	ZM_BD_DONE                  // terminal; idle until the Battle scene unloads
};

class ZM_BattleDirector
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;

	ZM_BattleDirector() = delete;
	explicit ZM_BattleDirector(Zenith_Entity& xParentEntity);

	// Move-CONSTRUCTIBLE only (the ECS component pool move-constructs on Grow /
	// swap-and-pop / cross-scene transfer). Deliberately NOT declared noexcept:
	// ZM_BattleDirectorCore deep-owns a Zenith_Vector whose move is not noexcept,
	// so a `noexcept = default` move op would be DEFINED AS DELETED under C++20 and
	// break the pool. Plain `= default` keeps the move ctor defined. Copy is deleted.
	ZM_BattleDirector(const ZM_BattleDirector&) = delete;
	ZM_BattleDirector& operator=(const ZM_BattleDirector&) = delete;
	ZM_BattleDirector(ZM_BattleDirector&&) = default;

	void OnStart();
	void OnUpdate(float fDeltaSeconds);
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	ZM_BATTLE_DIRECTOR_PHASE GetPhase() const { return m_ePhase; }

	// --- Read accessors for the windowed SC5 tests (menu/core inspection) ---
	const ZM_BattleDirectorCore& GetCore() const { return m_xCore; }
	ZM_BattleMenuScreen GetHudMenuScreen() const { return m_xHud.GetMenuScreen(); }
	int                 GetHudMenuCursor() const { return m_xHud.GetMenuCursor(); }
	Zenith_EntityID     GetCreatureModelEntityID(ZM_SIDE eSide) const
	{
		return eSide < ZM_SIDE_COUNT ? m_axCreatureModelIDs[eSide] : INVALID_ENTITY_ID;
	}

	// --- Pure static decision surface (unit-tested with NO entity/scene/graphics) ---

	// A deterministic, reproducible placeholder spec for the player side (the real
	// party comes from a later box): a fixed valid starter at level 5, so every call
	// returns the same bytes and m_eSpecies != NONE / m_aeMoves[0] != NONE.
	static ZM_BattleMonsterSpec BuildPlaceholderPlayerSpec();
	// The S5 wild-battle config: wild, no exp award (all other fields default).
	static ZM_BattleConfig      BuildBattleConfig();
	// The S7 item-3 TRAINER-battle config: not wild, no catching, no fleeing, a
	// trainer battle (gross exp x1.5), no level cap, and exp OFF BY CONSTRUCTION.
	// Exp-off mirrors BuildBattleConfig() exactly: SC5's caller flips m_bAwardExp
	// on a LOCAL COPY for a real-party battle, the same way RunSetup already does
	// for the wild arm, so this static stays a constant a unit can pin.
	//
	// m_bCanFlee = false is a LOADED VALUE, not decoration: ZM_BattleEngine
	// Zenith_Asserts on a RUN action at TWO sites (SubmitAction and DoRunAction),
	// and Zenith_Assert breaks the process in every configuration. Nothing may
	// Begin a battle with this config until the HUD Run-gate is in place: SC4
	// landed that gate (ZM_BattleDirectorCore::IsFleeAllowed +
	// ZM_UI_BattleHUD::MenuRootItemIsAllowed) and SC5's RunTrainerSetup is its
	// first and only caller.
	static ZM_BattleConfig      BuildTrainerBattleConfig();
	// Pure deterministic hash of (species, level) -> the core's battle seed, so a
	// windowed drive is reproducible.
	static u_int64              DeriveBattleSeed(ZM_SPECIES_ID eSpecies, u_int uLevel);
	// True iff ePhase == ZM_BD_WAIT_FOR_IN_BATTLE && bTransitionInBattle && !bAlreadySetUp.
	static bool ShouldRunSetup(ZM_BATTLE_DIRECTOR_PHASE ePhase, bool bTransitionInBattle, bool bAlreadySetUp);
	// True iff ePhase == ZM_BD_RUNNING && bCoreShouldEnd && !bAlreadyRequested.
	static bool ShouldRequestEndNow(ZM_BATTLE_DIRECTOR_PHASE ePhase, bool bCoreShouldEnd, bool bAlreadyRequested);

private:
	// Wall-clock softlock guard: if RUNNING outlasts this without resolving, request
	// the end once anyway (never triggers under zm_instant_battles). WALL-CLOCK, not
	// a frame count -- nothing pins the frame rate.
	static constexpr float fRUNNING_DEADLINE_SECONDS = 30.0f;
	// How far above a platform's centre a creature model is placed so it sits on the
	// slab rather than intersecting it.
	static constexpr float fCREATURE_Y_LIFT = 1.0f;

	// One-shot: read the payload, Begin the core, place both models, -> RUNNING.
	void RunSetup(const ZM_BattleTransition& xTransition);
	// One-shot TRAINER setup (S7 item 3 SC5): build the fixed party from the roster
	// row, Begin with the row's AI tier and the trainer config, -> RUNNING. Called
	// ONLY from RunSetup's dispatch prefix; the wild body below that prefix is
	// untouched.
	void RunTrainerSetup(ZM_TRAINER_ID eTrainer);
	// Best-effort model placement onto the unique arena's platforms (1=player,
	// 2=enemy). A missing arena/bundle silently skips; it never aborts the battle.
	void PlaceCreatureModels(ZM_SPECIES_ID ePlayerSpecies, ZM_SPECIES_ID eEnemySpecies);
	// Apply every newly-presented SWITCH_IN to the corresponding live arena model.
	void SyncCreatureModelsToPresentedEvents();
	void SetCreatureModelSpecies(ZM_SIDE eSide, ZM_SPECIES_ID eSpecies);

	// The pure, headless battle heart -- deep-owned (drives the AI-vs-AI battle).
	ZM_BattleDirectorCore    m_xCore;
	// Stored BY VALUE (not a reference): a reference member would dangle on the
	// temporary ctor handle and would break the pool's move-construct. Mirrors
	// ZM_BattleArena / ZM_BattleTransition. Zenith_Entity has no mutable state --
	// GetSceneData() re-resolves the parent's own scene slot every call.
	Zenith_Entity            m_xParentEntity;
	ZM_BATTLE_DIRECTOR_PHASE m_ePhase        = ZM_BD_WAIT_FOR_IN_BATTLE;
	bool                     m_bEndRequested = false;   // RequestBattleEnd fired exactly once
	Zenith_EntityID          m_axCreatureModelIDs[ZM_SIDE_COUNT] =
	{
		INVALID_ENTITY_ID,
		INVALID_ENTITY_ID
	};
	u_int                    m_uCreatureModelEventCursor = 0u;
	// True iff RunSetup built the player side from the REAL persistent party lead (not
	// the placeholder). Only then is exp awarded + the result written back to the lead
	// on resolve (SC3). POD -- keeps the pool's move-construct; reset in OnStart / Read.
	bool                     m_bWriteBackToLead = false;
	// The trainer this battle is for, or ZM_TRAINER_NONE for a WILD battle (SC5). It
	// is the ONLY gate on the prize/defeat-flag payout, and it is POD -- so the
	// deliberately-not-noexcept move ctor above stays defined. Reset in OnStart and
	// in ReadFromDataStream, exactly like m_bWriteBackToLead.
	ZM_TRAINER_ID            m_eTrainer = ZM_TRAINER_NONE;
	float                    m_fRunningSeconds = 0.0f;  // wall-clock time spent driving (deadline guard)
	// The director-owned battle HUD (SC4): authored onto THIS entity's UI component
	// at bake time, revealed at Setup, driven each frame, hidden before the end-fade.
	// Held by value (only a std::string + float), so the pool's move-construct is
	// preserved (see the move-ctor note above).
	ZM_UI_BattleHUD          m_xHud;
};
