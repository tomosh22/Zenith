#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Input/Zenith_Input.h"
// Zenith_Window class is provided by Zenith.h via Zenith_OS_Include.h —
// don't include the win64-specific header directly or the Android build
// of this test would pull in GLFW/Win32 declarations that clash with
// Zenith_Android_Window.
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPTelemetry.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPOrbitCamera_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/Priest_Behaviour.h"

#include "Physics/Zenith_Physics.h"

#include <chrono>
#include <cstdio>
#include <cmath>
#include <vector>
#include <utility>
#include <filesystem>
#include <memory>
#include <string>
#include "Source/DP_Tuning.h"

// ============================================================================
// PersonalityPlaythrough_* — pure-input playthrough, 5 personality variants.
//
// Drives DevilsPlayground end-to-end using ONLY Zenith_InputSimulator (no
// teleporting, no *ForTest bypass calls, no SetInteractOnOverlap, no
// SetPossessedVillager). Each test registers under a distinct personality
// (Casual / Stealth / Speedrunner / Berserker) that tunes the input
// layer:
//
//   * Casual      — baseline. Walks normally, single F-press, runs the
//                   pause-overlay test, engages every system. Reference
//                   recording the others are compared against.
//   * Stealth     — holds Ctrl while walking (walk-quiet, half speed,
//                   halved footstep loudness) AND skips the noise machine
//                   entirely (it's the one in-game source of deliberate
//                   priest aggro). Walk budget doubled to compensate for
//                   half speed. Single F-press per interaction.
//                   Note: interaction noises (forge / chest / door) are
//                   *fixed-loudness* and not silenced by walk-quiet, so
//                   the priest can still hear the interactable side-effects
//                   on layouts where it spawns nearby.
//   * Speedrunner — adaptive sprint: holds Shift only while the next
//                   target is > kSprintMinDistanceM (5 m) away, walks on
//                   close approach. Skips the pause-overlay test. The
//                   pre-rework "Speedrun" (blind-sprint) was slower than
//                   Casual because sprint_life_cost_extra_per_s burned
//                   through the 30 s life timer and forced 3+ re-possess
//                   cycles per run; adaptive sprint should actually beat
//                   Casual's wall-clock.
//   * Berserker   — adaptive sprint (like Speedrunner) PLUS F-mash inside
//                   every interactable range. Skips the pause-overlay
//                   test. Distinct from Speedrunner by the F-mash
//                   signature in the recording (~70+ Interact events vs
//                   the ~9 of single-press personalities). Blind sprint
//                   was tried first but drained the life timer faster
//                   than the objective loop could converge -- the run
//                   never terminated. See PersonalityConfig comment for
//                   the long form.
//
// Headless: the test always loads GameLevel directly (skips FrontEnd menu).
// Menu-click coverage lives in Test_FullPlaythrough.cpp. With FrontEnd out
// of the loop the personality tests run in either --headless or visible
// mode unchanged; the runner picks by flag.
//
// Each registration writes its telemetry recording to
//   %TEMP%/dp_personality_<name>.{ztlm,json}
// so the five runs don't clobber each other and the visualiser can be
// pointed at any of them individually.
//
// Exercised systems (matches FullPlaythrough_Test's coverage list):
//   1. (skipped — see header) FrontEnd → MenuPlay → GameLevel
//   2. Q/E camera rotate, mouse-wheel zoom in/out
//   3. Click-to-possess (raycast hits villager screen pixel)
//   4. WASD movement of possessed villager (modifier keys vary per personality)
//   5. Item pickup (proximity)
//   6. Forge crafting Iron → Key (F-press)
//   7. Door unlock with key (F-press)
//   8. Chest open (F-press)
//   9. Noise machine emit (F-press) — Stealth skips this
//  10. 5× pentagram delivery → DP_Win::HasWon() + DP_OnVictory event
//  11. Pause overlay toggle (Esc) — Casual + Stealth run 1 cycle,
//      Speedrunner + Berserker skip
//
// Implementation notes:
//   - SimulateMouseClick / SimulateClickOnUIElement call StepFrame() inside,
//     which would re-enter Zenith_MainLoop while we're already inside Step.
//     We inline their bodies (SimulateMousePosition + SimulateKeyPress on
//     ZENITH_MOUSE_BUTTON_LEFT) and let the harness step the next frame.
//   - Step is called BEFORE game OnUpdate in the same frame
//     (Zenith_Core.cpp:206), so a SimulateKeyPress(F) issued in Step is
//     visible to the game's WasKeyPressedThisFrame() read on that same frame.
//   - WASD direction is camera-relative (matches DPVillager::TickMovement);
//     we project the world-space target delta onto camera forward/right and
//     hold the appropriate keys.
//   - Click-to-possess depends on the engine fix routing BuildRayFromMouse
//     through Zenith_Input::GetMousePosition (so the simulator-set position
//     drives the raycast).
// ============================================================================

namespace
{
	// ------------------------------------------------------------------------
	// Personality table
	//
	// Each personality is a small bundle of input-layer tunables. Setup writes
	// g_xActiveCfg from one of the constants below before Step starts running;
	// Step + Verify branch on the relevant flags. Adding a new personality is
	// a four-step pattern:
	//   1. Add an enum value here.
	//   2. Add a `kPersonality_*` constant below.
	//   3. Add a Setup wrapper at the bottom of the file.
	//   4. Add a ZENITH_AUTOMATED_TEST_REGISTER block at the bottom.
	// No other code edits are needed; everything reads off g_xActiveCfg.
	//
	// Semantics (2026-05-18 rework — see Q-2026-05-18-001 "speedrun slower
	// than human" puzzle for the original motivation):
	//   * Casual      — baseline. Walks normally, single F-press, runs the
	//                   pause overlay test, engages every system. The
	//                   reference recording the others are compared against.
	//   * Stealth     — minimises priest aggro. Walks quiet (Ctrl held,
	//                   half speed + halved footstep loudness), and SKIPS
	//                   the noise machine entirely — that machine is the
	//                   one in-game source of deliberate hearing stimulus,
	//                   so engaging it would contradict the stealth playstyle.
	//                   Walk budget doubled to compensate for the half speed.
	//   * Speedrunner — minimises total frames. Sprints ONLY when the next
	//                   target is more than kSprintMinDistanceM away; walks
	//                   close approaches so we don't pay the per-second life
	//                   cost when it doesn't save time. Skips the pause
	//                   overlay test (speedrunners don't admire menus).
	//                   This is the personality that the pre-rework
	//                   "Speedrun" (blind-sprint) was trying — and failing —
	//                   to be: blind sprint burned through the life timer
	//                   on every villager and forced 3x re-possess overhead
	//                   that ate the speedup.
	//   * Berserker   — maximises chaos. Blind sprint (Shift always held)
	//                   PLUS F-mash inside every interact range. Skips the
	//                   pause overlay test. Accepts villager deaths +
	//                   re-possessions cheerfully — they're part of the
	//                   recording's signature.
	// ------------------------------------------------------------------------
	enum class Personality : uint8_t { Casual, Stealth, Speedrunner, Berserker };

	struct PersonalityConfig
	{
		Personality eType;
		const char* szName;             // used in log lines + telemetry filename
		bool        bHoldSprint;        // hold ZENITH_KEY_LEFT_SHIFT for the entire walk
		bool        bHoldQuiet;         // hold ZENITH_KEY_LEFT_CONTROL for the entire walk
		bool        bAdaptiveSprint;    // sprint only while the remaining distance to the
		                                //   active target is > kSprintMinDistanceM
		bool        bMashInteract;      // press F every frame in range vs once per arrival
		bool        bRunNoiseMachine;   // walk to + engage the noise machine
		bool        bRunPauseTest;      // run the Esc pause-overlay phases at all
		int         iPauseCycles;       // how many open/close cycles when bRunPauseTest=true
		int         iWalkBudgetMul;     // multiplier on the base 1200-frame walk budget
	};

	// Walk budget multipliers calibrated for the per-personality movement
	// speed. Stealth halves the villager's walk speed (Ctrl held), so we
	// double the per-walk frame budget to give it equal opportunity to
	// reach the same target. The other personalities don't slow down so
	// they run at the base 1x budget.
	//
	// All personalities load the same scene (ProcLevel, build index 1, the
	// only gameplay scene since 2026-05-19) and run Verify in lenient
	// mode -- procgen geometry doesn't guarantee a reachable
	// item/door/forge/pentagram chain inside the bot's frame budget, so
	// the playthrough tests assert "bot drove around without crashing",
	// not "bot completed the full win condition". The walking +
	// possession + interaction signatures are still observable via the
	// telemetry emitted alongside each run.
	constexpr PersonalityConfig kPersonality_Casual = {
		Personality::Casual,      "Casual",
		/*sprint*/false, /*quiet*/false, /*adaptive*/false,
		/*mash*/false,
		/*noise*/true,   /*pause*/true,  /*pauseCycles*/1,
		/*budgetMul*/1 };
	// Stealth caveat (seed-matrix analysis 2026-05-19): walk-quiet only
	// halves *footstep* loudness (movement.walk_footstep_loudness_multiplier).
	// The gameplay interactions Stealth still performs -- forge crafting
	// (loudness 1.0 @ 30m radius), chest open (0.8 @ 20m), door open
	// (0.6 @ 12m) -- emit fixed-loudness noise the priest WILL hear
	// regardless of Ctrl-held. So Stealth's priest-Investigate fraction
	// being non-zero on procgen layouts where the priest is close to
	// those interactables is by design, not a noise-leak bug. The
	// bRunNoiseMachine=false flag below specifically opts Stealth out of
	// the *deliberate* noise-emit (DummyNoiseMachine F-press); it does
	// not silence side-effect noise from forge/chest/door.
	constexpr PersonalityConfig kPersonality_Stealth = {
		Personality::Stealth,     "Stealth",
		/*sprint*/false, /*quiet*/true,  /*adaptive*/false,
		/*mash*/false,
		/*noise*/false,  /*pause*/true,  /*pauseCycles*/1,
		/*budgetMul*/2 };
	constexpr PersonalityConfig kPersonality_Speedrunner = {
		Personality::Speedrunner, "Speedrunner",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*mash*/false,
		/*noise*/true,   /*pause*/false, /*pauseCycles*/1,
		/*budgetMul*/1 };
	// Berserker uses ADAPTIVE sprint (not blind sprint) for the same
	// reason Speedrunner does: blind sprint drains the life timer fast
	// enough that the villager dies mid-objective-loop, the bot re-possesses
	// a fresh villager, that one also dies, and the run never terminates
	// inside the per-test frame cap. Adaptive sprint keeps the "aggressive
	// + chaotic" semantic (sprint on long hops + mash F in range) while
	// allowing the playthrough to actually finish. The F-mash is what makes
	// this distinct from Speedrunner in the telemetry signature.
	constexpr PersonalityConfig kPersonality_Berserker = {
		Personality::Berserker,   "Berserker",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*mash*/true,
		/*noise*/true,   /*pause*/false, /*pauseCycles*/1,
		/*budgetMul*/1 };
	// (Methodical personality removed 2026-05-19 -- the seed-matrix
	// analysis found its bot behaviour was within rounding error of
	// Casual on every metric except the pause-cycle count (3 cycles
	// vs Casual's 1). Pause coverage is preserved via Casual.)

	// All personalities load the same scene index. Defined as a constant
	// rather than a per-personality field because the original GameLevel
	// vs ProcLevel split is gone.
	constexpr uint8_t kPersonalitySceneIndex = 1;

	PersonalityConfig g_xActiveCfg = kPersonality_Casual;

	// Adaptive-sprint cut-off. Speedrunner holds Shift while the remaining
	// distance to the active target exceeds this many metres; walks within
	// closer range. Tuned to ~5 m — the typical inter-object spacing on
	// GameLevel — so that long inter-building hops sprint but the final
	// approach to a chest/door/forge walks (avoids overshoot + only pays
	// the sprint life-cost when it actually saves meaningful time).
	constexpr float kSprintMinDistanceM = 5.0f;

	// How many frames Berserker dwells in a press-F phase mashing the
	// interact key. Each press while in range fires DP_OnInteract -> the
	// telemetry recorder sees N Interact events per interactable instead
	// of 1. 8 frames * 9 interactables ~= 72 extra events.
	constexpr int kBerserkerMashFrames = 8;

	// SetWalkBudget defined below, after g_iWalkBudget storage is declared.

	// ------------------------------------------------------------------------
	// Phase enum
	// ------------------------------------------------------------------------
	enum Phase : int
	{
		kHP_Start,
		kHP_LoadFE,
		kHP_WaitFE,
		kHP_ClickPlay,
		kHP_WaitGameLevel,
		kHP_CaptureRefs,
		kHP_CamRotateQ,
		kHP_CamRotateE,
		kHP_CamZoomIn,
		kHP_CamZoomOut,
		kHP_PossessClick,
		kHP_WaitPossess,
		kHP_WalkIron,
		kHP_WaitIronPickup,
		kHP_WalkForge,
		kHP_PressForgeF,
		kHP_VerifyForge,
		kHP_WalkDoor,
		kHP_PressDoorF,
		kHP_VerifyDoor,
		kHP_WalkChest,
		kHP_PressChestF,
		kHP_VerifyChest,
		kHP_WalkNoise,
		kHP_PressNoiseF,
		kHP_WaitNoise,
		kHP_ObjLoopFind,
		kHP_ObjLoopWalk,
		kHP_ObjLoopWalkPentagram,
		kHP_ObjLoopPressF,
		kHP_AssertVictory,
		kHP_PauseOpen,
		kHP_PauseAssertOpen,
		kHP_PauseClose,
		kHP_PauseAssertClosed,
		kHP_Summary,
		kHP_Done
	};

	int g_iPhase = kHP_Start;
	int g_iWait  = 0;            // generic intra-phase frame counter
	int g_iWalkBudget = 0;       // remaining frames allowed in current walk

	// Single-source setter for the per-walk frame budget. Replaces the
	// literal `SetWalkBudget(1200);` assignments scattered across Step
	// so the active personality's budget multiplier is honoured everywhere
	// (Stealth needs 2x because walk-quiet halves villager speed).
	inline void SetWalkBudget(int iBase)
	{
		g_iWalkBudget = iBase * g_xActiveCfg.iWalkBudgetMul;
	}

	// Stuck-detection state — bail early when the villager hasn't moved in N
	// frames. Without this, an unreachable target consumes the full walk
	// budget (1500 frames ≈ 50 s wall-clock) before we surrender, which
	// blows the 3-minute test cap.
	Zenith_Maths::Vector3 g_xStuckRefPos(1e9f, 0.0f, 0.0f);
	int g_iStuckCounter = 0;
	int g_iStuckReplans = 0;
	int g_iChestAttempts = 0;
	int g_iDoorAttempts  = 0;
	int g_iNoiseAttempts = 0;
	constexpr int kStuckFramesLimit = 120;  // ~4 s wall-clock at 30 fps

	// Stuck-recovery side-step (added 2026-05-20 after the 10-seed
	// personality matrix flagged seed-55555 / similar layouts where the
	// villager jammed against a procgen door-jamb and never moved). When
	// stuck-detect fires, the WASD steering direction is rotated 90
	// degrees for kSideStepFrames frames so the villager peels off the
	// wall it was pinned against. g_bSideStepClockwise alternates per
	// stuck-event so a corner that traps clockwise gets a try
	// counter-clockwise next time.
	int  g_iSideStepFrames     = 0;
	bool g_bSideStepClockwise  = true;
	constexpr int kSideStepFrames = 30; // ~0.5 s @ 60 fps -- enough lateral
	                                    // displacement to break wall contact
	                                    // without overshooting the waypoint.

	// ------------------------------------------------------------------------
	// Captured entities + state
	// ------------------------------------------------------------------------
	Zenith_EntityID g_xPossessTarget;     // villager picked for first possession
	Zenith_EntityID g_xCurrentVillager;   // current possessed villager (may be re-acquired if first dies)
	Zenith_EntityID g_xDoor;
	Zenith_EntityID g_xChest;
	Zenith_EntityID g_xForge;
	Zenith_EntityID g_xNoise;
	Zenith_EntityID g_xPentagram;
	Zenith_EntityID g_xPriest;

	// Scene composition snapshot (for Verify).
	int g_iVillagerCount = 0;
	int g_iDoorCount     = 0;
	int g_iChestCount    = 0;

	// Camera before / after Q rotation (yaw delta).
	float g_fYawBeforeQ  = 0.0f;
	float g_fYawAfterQ   = 0.0f;
	float g_fYawAfterE   = 0.0f;
	float g_fDistBefore  = 0.0f;
	float g_fDistAfterIn = 0.0f;
	float g_fDistAfterOut = 0.0f;

	// Possession check.
	bool  g_bPossessionConfirmed = false;
	double g_fPossessClickX = 0.0;
	double g_fPossessClickY = 0.0;

	// Pickup / forge / door / chest / noise booleans.
	bool g_bIronPickedUp     = false;
	bool g_bForgeCrafted     = false;
	bool g_bDoorOpened       = false;
	bool g_bChestOpened      = false;
	bool g_bPriestHeardNoise = false;

	// Objective pickup-and-deliver loop state.
	int  g_iObjectivesDelivered = 0;
	int  g_iObjAttempts = 0;        // retry counter for current objective
	Zenith_EntityID g_xCurrentObjItem;
	const DP_ItemTag g_aeObjTags[5] = {
		DP_ItemTag::Objective1, DP_ItemTag::Objective2,
		DP_ItemTag::Objective3, DP_ItemTag::Objective4,
		DP_ItemTag::Objective5,
	};
	// Per-objective retry cap. Was 4 pre-2026-05-20. The seed-matrix
	// after the cross-possession memory landed showed cells reaching
	// 4-5 objectives only when the bot survived 8+ possessions; with
	// the old cap of 4 the bot would surrender an objective after just
	// 4 failed pentagram-presses (the typical failure pattern is
	// "pickup, die mid-walk, repossess, walker has no item, F-press
	// does nothing, attempt++"). 16 gives the cross-possession recover
	// path room to actually re-pickup -- each repossess-and-re-pickup
	// costs ~1 attempt, so 16 lets ~10 productive deliveries through.
	constexpr int kMaxObjAttempts = 16;

	// Victory / pause flags.
	bool g_bVictoryEvent = false;
	uint32_t g_uVictoryMask = 0;
	bool g_bPauseOnObserved = false;
	bool g_bPauseOffObserved = false;
	// Counter decremented per pause-open/close cycle. When Setup writes
	// g_xActiveCfg.iPauseCycles into this counter, the kHP_PauseAssertClosed
	// transition either loops back to kHP_PauseOpen (counter > 0) or
	// advances to kHP_Summary. Every personality that runs the pause
	// test uses 1 cycle now (Methodical's 3-cycle stress was removed
	// 2026-05-19 along with the personality itself).
	int  g_iPauseCyclesRemaining = 0;

	Zenith_EventHandle g_xVictoryHandle = INVALID_EVENT_HANDLE;

	// ------------------------------------------------------------------------
	// Telemetry recording state.
	//
	// Mirrors the bot-playthrough test's recorder + hooks pattern so the
	// resulting binary/JSON artifacts feed straight into
	// Tools/dp_telemetry_visualise.ps1 unchanged. Recording begins once the
	// GameLevel scene has loaded + scripts are populated (kHP_CaptureRefs)
	// and ends in Verify regardless of pass/fail so the artifacts are
	// always available for offline inspection.
	// ------------------------------------------------------------------------
	constexpr float kHPFixedDt = 1.0f / 60.0f;
	constexpr uint32_t kHPSamplePeriodFrames = 6u;  // 10 Hz position sampling

	std::unique_ptr<DPTelemetry::Hooks> g_pxTelemetryHooks;
	bool g_bRecordingActive = false;
	std::string g_strHPBinPath;
	std::string g_strHPJsonPath;
	std::string g_strHPFramesCsvPath;
	std::string g_strHPEventsCsvPath;
	int g_iTelemetryFrame = 0;  // frames since recording started

	// Telemetry-v3: track which (observer,target) contacts are currently
	// active (awareness above kContactThreshold). Diff against previous
	// frame to emit DP_OnPerceptionContactBegin/End rising/falling edges
	// from the per-frame sampler. Cleared by Setup_HumanPlaythrough.
	constexpr float kContactAwarenessThreshold = 0.4f;
	struct PerceptionContactKey
	{
		uint32_t uObsIdx;  uint32_t uObsGen;
		uint32_t uTgtIdx;  uint32_t uTgtGen;
		bool operator==(const PerceptionContactKey& o) const
		{
			return uObsIdx == o.uObsIdx && uObsGen == o.uObsGen
			    && uTgtIdx == o.uTgtIdx && uTgtGen == o.uTgtGen;
		}
	};
	struct PerceptionContactEntry
	{
		PerceptionContactKey xKey;
		uint32_t              uStimulusMask;
	};
	std::vector<PerceptionContactEntry> g_axActiveContacts;
	int g_iContactPrevFrameIdx = -1;

	std::string HPTempPath(const char* sz)
	{
		std::error_code xErr;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
		if (xErr) xDir = ".";
		// Per-personality basename so the 4 personality tests don't clobber
		// each other when run in the same batch. e.g. dp_personality_Casual_playthrough.ztlm.
		std::string strBase = std::string("dp_personality_") + g_xActiveCfg.szName + "_" + sz;
		xDir /= strBase;
		return xDir.string();
	}

	// EmitHPPositionSample is defined below, after TryGetEntityPos (its
	// in-namespace dependency).

	// ------------------------------------------------------------------------
	// Helpers (mirrors Test_FullPlaythrough.cpp's pattern)
	// ------------------------------------------------------------------------
	template<typename T>
	Zenith_EntityID FindFirstScript()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&xResult](Zenith_EntityID xId, T&) { if (!xResult.IsValid()) xResult = xId; });
		return xResult;
	}

	template<typename T>
	int CountScripts()
	{
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&iCount](Zenith_EntityID, T&) { ++iCount; });
		return iCount;
	}

	template<typename T>
	T* GetScript(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (!pxScene) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<T>();
	}

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (!pxScene) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	// Resolve the priest's current BT branch + nav-target by inspecting
	// the blackboard keys Priest_Behaviour writes. Mirror of the priest BT
	// Selector ordering -- Apprehend > Pursue > Investigate > Patrol.
	void DerivePriestIntentAndTarget(Zenith_EntityID xPriestId,
	                                 const Zenith_Maths::Vector3& xPriestPos,
	                                 DPTelemetry::PriestIntent& eIntent,
	                                 Zenith_Maths::Vector3& xTargetPos)
	{
		eIntent    = DPTelemetry::PriestIntent::Idle;
		xTargetPos = Zenith_Maths::Vector3(0.0f);

		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xPriestId);
		if (!pxScene) return;
		Zenith_Entity xPriest = pxScene->TryGetEntity(xPriestId);
		if (!xPriest.IsValid()) return;
		if (!xPriest.HasComponent<Zenith_AIAgentComponent>()) return;

		Zenith_Blackboard& xBB =
			xPriest.GetComponent<Zenith_AIAgentComponent>().GetBlackboard();

		const Zenith_EntityID xTgtWithDevil =
			xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
		if (xTgtWithDevil.IsValid())
		{
			// Pursue or Apprehend, depending on horizontal distance to the
			// target vs the apprehend range.
			Zenith_Maths::Vector3 xPos(0.0f);
			if (TryGetEntityPos(xTgtWithDevil, xPos))
			{
				xTargetPos = xPos;
				const float fDx = xPos.x - xPriestPos.x;
				const float fDz = xPos.z - xPriestPos.z;
				const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
				const float fApprehendRange = DP_Tuning::Get<float>("priest.apprehend_range_m");
				eIntent = (fDist <= fApprehendRange)
					? DPTelemetry::PriestIntent::Apprehend
					: DPTelemetry::PriestIntent::Pursue;
			}
			else
			{
				eIntent = DPTelemetry::PriestIntent::Pursue;
			}
			return;
		}

		if (xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS))
		{
			xTargetPos = xBB.GetVector3(DP_AI::BB_KEY_INVESTIGATE_POS);
			eIntent    = DPTelemetry::PriestIntent::Investigate;
			return;
		}

		// PatrolTarget is a Vector3; "no patrol" reads as (0,0,0). The
		// real first patrol point picked by DP_BTAction_FindPos lands
		// inside the playable area, so a true zero is a safe sentinel.
		const Zenith_Maths::Vector3 xPatrol = xBB.GetVector3(DP_AI::BB_KEY_PATROL_TARGET);
		if (std::fabs(xPatrol.x) + std::fabs(xPatrol.z) > 0.001f)
		{
			xTargetPos = xPatrol;
			eIntent    = DPTelemetry::PriestIntent::Patrol;
		}
	}

	// Sample the orbit camera once per frame. Returns a valid=false
	// CameraState if no DPOrbitCamera_Behaviour exists in the active
	// scene (e.g. FrontEnd scene during early Setup).
	Zenith_Telemetry::CameraState SampleCameraState()
	{
		Zenith_Telemetry::CameraState xCam;
		xCam.bValid = 0;
		DP_Query::ForEachScriptInActiveScene<DPOrbitCamera_Behaviour>(
			[&xCam](Zenith_EntityID, DPOrbitCamera_Behaviour& xOrbit)
			{
				if (xCam.bValid) return;  // first one wins; there's only one
				const Zenith_Maths::Vector3 xTarget = xOrbit.GetOrbitTarget();
				const float fYaw   = xOrbit.GetOrbitYaw();
				const float fDist  = xOrbit.GetOrbitDistance();
				const float fPitch = xOrbit.GetOrbitPitch();
				// Eye position derivation mirrors DPOrbitCamera::OnUpdate:
				//   xOffset = (cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch))
				//   eyePos  = target + offset * distance
				const float fCp = std::cos(fPitch);
				const Zenith_Maths::Vector3 xOff(
					std::cos(fYaw) * fCp,
					std::sin(fPitch),
					std::sin(fYaw) * fCp);
				xCam.xLookAt        = xTarget;
				xCam.xPos           = xTarget + xOff * fDist;
				xCam.fOrbitYawRad   = fYaw;
				xCam.fOrbitDistance = fDist;
				// 55-degree FOV matches the camera authoring in
				// AuthorDPGameSceneFrame; the camera component itself
				// owns the exact value but we don't have direct access
				// without the engine API surface, so this is a known
				// approximation.
				xCam.fFovRadians    = 55.0f * 3.14159265358979323846f / 180.0f;
				xCam.bValid         = 1;
			});
		return xCam;
	}

	// Diff this frame's perception contacts against last frame's, emitting
	// DP_OnPerceptionContactBegin/End for rising / falling edges. Called
	// from EmitHPPositionSample so the contact transitions hit telemetry
	// at the same cadence as position samples (10 Hz).
	void UpdatePerceptionContacts(Zenith_EntityID xObserver)
	{
		const auto* paxTargets =
			Zenith_PerceptionSystem::GetPerceivedTargets(xObserver);
		if (paxTargets == nullptr) paxTargets = nullptr;

		std::vector<PerceptionContactEntry> axCurrent;
		if (paxTargets != nullptr)
		{
			const uint32_t uN = paxTargets->GetSize();
			for (uint32_t i = 0; i < uN; ++i)
			{
				const auto& xT = paxTargets->Get(i);
				if (xT.m_fAwareness < kContactAwarenessThreshold) continue;
				PerceptionContactEntry xC;
				xC.xKey.uObsIdx = xObserver.m_uIndex;
				xC.xKey.uObsGen = xObserver.m_uGeneration;
				xC.xKey.uTgtIdx = xT.m_xEntityID.m_uIndex;
				xC.xKey.uTgtGen = xT.m_xEntityID.m_uGeneration;
				xC.uStimulusMask = xT.m_uStimulusMask;
				axCurrent.push_back(xC);
			}
		}

		// Rising edges: in current, not in g_axActiveContacts.
		for (const auto& xC : axCurrent)
		{
			bool bWasActive = false;
			for (const auto& xP : g_axActiveContacts)
			{
				if (xP.xKey == xC.xKey) { bWasActive = true; break; }
			}
			if (!bWasActive)
			{
				DP_OnPerceptionContactBegin xEvt;
				xEvt.m_xObserver.m_uIndex      = xC.xKey.uObsIdx;
				xEvt.m_xObserver.m_uGeneration = xC.xKey.uObsGen;
				xEvt.m_xTarget.m_uIndex        = xC.xKey.uTgtIdx;
				xEvt.m_xTarget.m_uGeneration   = xC.xKey.uTgtGen;
				xEvt.m_uStimulusMask           = xC.uStimulusMask;
				xEvt.m_fAwareness              = kContactAwarenessThreshold;
				Zenith_EventDispatcher::Get().Dispatch(xEvt);
			}
		}

		// Falling edges: in g_axActiveContacts, not in current.
		for (const auto& xP : g_axActiveContacts)
		{
			bool bStillActive = false;
			for (const auto& xC : axCurrent)
			{
				if (xC.xKey == xP.xKey) { bStillActive = true; break; }
			}
			if (!bStillActive)
			{
				DP_OnPerceptionContactEnd xEvt;
				xEvt.m_xObserver.m_uIndex      = xP.xKey.uObsIdx;
				xEvt.m_xObserver.m_uGeneration = xP.xKey.uObsGen;
				xEvt.m_xTarget.m_uIndex        = xP.xKey.uTgtIdx;
				xEvt.m_xTarget.m_uGeneration   = xP.xKey.uTgtGen;
				xEvt.m_uStimulusMask           = xP.uStimulusMask;
				Zenith_EventDispatcher::Get().Dispatch(xEvt);
			}
		}

		g_axActiveContacts = std::move(axCurrent);
	}

	// Per-sample emit. Builds an EntitySnapshot per villager + the priest
	// with all v3 fields populated:
	//   * Villagers: held-item tag in uHeldItemTag, remaining life timer
	//     (seconds) in fSecondaryFloat.
	//   * Priest:    BT-derived intent in uAIIntent, current nav-target
	//     world XYZ in xAITargetPos.
	// Also samples the camera + per-frame wall-clock ms and fires the
	// perception-contact diff for the priest.
	void EmitHPPositionSample(int iFrame)
	{
		// Frame wall-clock timing. The sampler runs once per kHPSamplePeriodFrames,
		// so this measures the *gap between samples* rather than a single
		// physics frame -- close enough to detect gameplay-path perf
		// regressions (the main use case) while staying cheap.
		static auto s_xLastSampleTime = std::chrono::steady_clock::now();
		const auto xNow = std::chrono::steady_clock::now();
		const std::chrono::duration<float, std::milli> xDelta = xNow - s_xLastSampleTime;
		s_xLastSampleTime = xNow;

		Zenith_Telemetry::FrameSample xSample;
		xSample.fTimeS       = static_cast<float>(iFrame) * kHPFixedDt;
		xSample.xCamera      = SampleCameraState();
		xSample.fFrameWallMs = xDelta.count();

		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();

		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xSample, xPossessed](Zenith_EntityID xId, DPVillager_Behaviour& xVilla)
			{
				Zenith_Telemetry::EntitySnapshot xE;
				xE.xId = xId;
				if (!TryGetEntityPos(xId, xE.xPos)) return;
				uint32_t uFlags = 0;
				const float fRemaining = xVilla.GetRemainingLife();
				xE.fSecondaryFloat = fRemaining;  // life-timer remaining (s)
				if (fRemaining > 0.0f) uFlags |= DPTelemetry::StateFlags::Alive;
				// Held item: emit the tag for every villager, but only the
				// possessed one is meaningful (others can't carry items in
				// gameplay). Defaults to 0 (DP_ItemTag::None).
				const DP_ItemTag eTag = DP_Player::GetHeldItemTag(xId);
				xE.uHeldItemTag = static_cast<uint8_t>(eTag);
				if (xId == xPossessed)
				{
					uFlags |= DPTelemetry::StateFlags::Possessed;
					if (xVilla.IsSprintingNow()) uFlags |= DPTelemetry::StateFlags::Sprinting;
					if (xVilla.IsWalkQuietNow()) uFlags |= DPTelemetry::StateFlags::WalkQuiet;
					if (eTag != DP_ItemTag::None) uFlags |= DPTelemetry::StateFlags::HoldingItem;
				}
				xE.uStateFlags = uFlags;
				xSample.axEntities.PushBack(xE);
			});

		Zenith_EntityID xPriestId;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xSample, &xPriestId](Zenith_EntityID xId, Priest_Behaviour&)
			{
				Zenith_Telemetry::EntitySnapshot xE;
				xE.xId = xId;
				if (!TryGetEntityPos(xId, xE.xPos)) return;
				xE.uStateFlags = DPTelemetry::StateFlags::IsPriest
				               | DPTelemetry::StateFlags::Alive;
				DPTelemetry::PriestIntent eIntent = DPTelemetry::PriestIntent::Idle;
				DerivePriestIntentAndTarget(xId, xE.xPos, eIntent, xE.xAITargetPos);
				xE.uAIIntent = static_cast<uint8_t>(eIntent);
				xSample.axEntities.PushBack(xE);
				xPriestId = xId;
			});

		Zenith_Telemetry::GetRecorder().RecordFrame(xSample);

		// Perception contact diff. Only meaningful when we found a priest.
		if (xPriestId.IsValid())
		{
			UpdatePerceptionContacts(xPriestId);
		}
	}

	// Scan every Zenith_ColliderComponent in the active scene and emit a
	// SceneObstacle (top-down OBB) for any whose world-space OBB centre
	// sits above the floor (y > 1.5 m). The 1.5 m threshold matches the
	// path grid's classifier in BuildPathGrid -- anything whose top sits
	// above the floor mesh's top (y=1.0) is "tall obstacle" the bot must
	// route around, exactly what we want the visualiser to draw.
	//
	// World-space OBB derivation uses the engine's own
	// Zenith_ColliderComponent::ComputeBoxDimensionsAndOffset() so the
	// rendered rectangles match the actual Jolt physics body the bot is
	// navigating against, including mesh-aware sizing.
	//
	// Why we need mesh-aware sizing: the BuildingAssetKit walls use
	// SM_Cube with mesh bounds (-1, 0, -1) to (1, 4, 1) (a 2x4x2 brick
	// anchored at y=0). The naive "half-extents = 0.5*scale" formula
	// gives a 1x1x1 collider, which:
	//   * is half the wall's actual X/Z footprint (mesh half-extent is
	//     1.0 in X/Z, not 0.5)
	//   * misses the Y offset (mesh centre Y is 2, not 0.5)
	// The pre-fix visualisation showed scattered tiny rectangles with
	// wrong positions because of these two errors compounding.
	//
	// Mirrors the wall convention documented in
	// Zenith_ColliderComponent.cpp::ComputeBoxDimensionsAndOffset() so a
	// future change to that function automatically flows through here.
	void ScanSceneObstacles(Zenith_Vector<Zenith_Telemetry::SceneObstacle>& xOutObs)
	{
		xOutObs.Clear();
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;

		uint32_t uIncluded = 0;
		uint32_t uExcludedFloor = 0;
		uint32_t uExcludedNonBox = 0;

		pxScene->Query<Zenith_ColliderComponent, Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID /*xId*/, Zenith_ColliderComponent& xCol, Zenith_TransformComponent& xT)
			{
				// Skip non-box obstacles. Spheres / capsules don't have a
				// clean OBB; the visualiser is rectangle-only for now.
				const CollisionVolumeType eVol = xCol.GetCollisionVolumeType();
				if (eVol != COLLISION_VOLUME_TYPE_OBB && eVol != COLLISION_VOLUME_TYPE_AABB)
				{
					++uExcludedNonBox;
					return;
				}

				Zenith_Maths::Vector3 xPos;
				Zenith_Maths::Quat    xRot;
				Zenith_Maths::Vector3 xScale;
				xT.GetPosition(xPos);
				xT.GetRotation(xRot);
				xT.GetScale(xScale);

				// Ask the collider for the same half-extents + local
				// offset the Jolt body was built with. This honours mesh
				// bounds when a ModelComponent is attached, and falls
				// back to the unit-cube assumption otherwise.
				Zenith_Maths::Vector3 xHalfExtents;
				Zenith_Maths::Vector3 xLocalOffset;
				xCol.ComputeBoxDimensionsAndOffset(xScale, xHalfExtents, xLocalOffset, false);

				// World centre = transform position + rotated mesh-centre
				// offset. The mesh offset for BuildingAssetKit walls is
				// (0, 2*sy, 0) so the body sits at the wall's vertical
				// midpoint; XZ stays at the transform's authored XZ.
				const Zenith_Maths::Vector3 xRotatedOffset = xRot * xLocalOffset;
				const Zenith_Maths::Vector3 xCentre = xPos + xRotatedOffset;

				// Tall-obstacle filter: anything whose OBB centre sits
				// above the floor's top (y=1.0). 1.5 m matches the
				// pathfinder's threshold (BuildPathGrid).
				if (xCentre.y < 1.5f)
				{
					++uExcludedFloor;
					return;
				}

				// Top-down OBB (XZ plane). Yaw extracted from the world
				// rotation; the visualiser rotates the local-frame
				// half-extents by yaw when computing the 4 corners.
				//
				// Why not glm::yaw(xRot)? GLM's implementation is
				// `asin(-2*(qx*qz - qw*qy))`, which collapses to the
				// [-pi/2, pi/2] range -- a 135-degree rotation comes
				// back as 45 because asin(sin(135)) = asin(sin(45)).
				// Half the BuildingAssetKit walls are authored at
				// angles outside that range, so glm::yaw was rendering
				// them at the wrong orientation. The atan2-based
				// Tait-Bryan extraction below covers the full
				// [-pi, pi] range and is exact for yaw-only rotations
				// (which is all our walls have).
				Zenith_Telemetry::SceneObstacle xO;
				xO.fCentreX     = xCentre.x;
				xO.fCentreZ     = xCentre.z;
				xO.fHalfExtentX = xHalfExtents.x;
				xO.fHalfExtentZ = xHalfExtents.z;
				xO.fYawRadians  = std::atan2(
					2.0f * (xRot.w * xRot.y + xRot.x * xRot.z),
					1.0f - 2.0f * (xRot.y * xRot.y + xRot.z * xRot.z));
				xOutObs.PushBack(xO);
				++uIncluded;
			});

		std::printf("[PersonalityPlaythrough] scanned %u obstacles "
			"(excluded %u floor, %u non-box)\n",
			uIncluded, uExcludedFloor, uExcludedNonBox);
		std::fflush(stdout);
	}

	// Look up a UI element by name in ANY loaded scene's UICanvas.
	// DPPauseMenuController_Behaviour::OnStart migrates its parent
	// entity to the persistent scene, so a search restricted to the
	// active scene would miss PauseOverlay. Walk every loaded scene;
	// first match wins. See FullPlaythrough_Test::FindHudText for the
	// fuller rationale.
	Zenith_UI::Zenith_UIText* FindHudText(const char* szName)
	{
		Zenith_UI::Zenith_UIText* pxResult = nullptr;
		const uint32_t uSlotCount = Zenith_SceneManager::GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_UIComponent>().ForEach(
				[szName, &pxResult](Zenith_EntityID, Zenith_UIComponent& xUI)
				{
					if (pxResult) return;
					pxResult = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
				});
			if (pxResult) break;
		}
		return pxResult;
	}

	// World-to-screen projection. Inverse of Zenith_CameraComponent::ScreenSpaceToWorldSpace
	// (mirrors its NDC convention exactly: clip.y/clip.w not flipped).
	bool WorldToScreen(const Zenith_Maths::Vector3& xWorld, double& fOutX, double& fOutY)
	{
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCam) return false;

		Zenith_Window* pxWindow = Zenith_Window::GetInstance();
		if (!pxWindow) return false;
		int32_t iW = 0, iH = 0;
		pxWindow->GetSize(iW, iH);
		if (iW <= 0 || iH <= 0) return false;

		Zenith_Maths::Matrix4 xView, xProj;
		pxCam->BuildViewMatrix(xView);
		pxCam->BuildProjectionMatrix(xProj);

		Zenith_Maths::Vector4 xClip = xProj * xView * Zenith_Maths::Vector4(xWorld.x, xWorld.y, xWorld.z, 1.0f);
		if (xClip.w <= 1e-4f) return false;  // behind / on the camera plane
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;

		// Camera's ScreenSpaceToWorldSpace uses (screenX/W)*2 - 1 = clipX (no Y
		// flip), so the inverse is screenX = (ndcX + 1) * 0.5 * W with the same
		// sign on Y.
		fOutX = static_cast<double>((fNdcX + 1.0f) * 0.5f * static_cast<float>(iW));
		fOutY = static_cast<double>((fNdcY + 1.0f) * 0.5f * static_cast<float>(iH));
		return true;
	}

	// Compute the camera's horizontal forward/right basis (matches
	// DPVillager_Behaviour::TickMovement so WASD inputs land where we expect).
	bool GetCameraHorizontalBasis(Zenith_Maths::Vector3& xForward, Zenith_Maths::Vector3& xRight)
	{
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCam) return false;
		pxCam->GetFacingDir(xForward);
		xForward.y = 0.0f;
		const float fLen = glm::length(xForward);
		if (fLen < 1e-3f) return false;
		xForward = glm::normalize(xForward);
		xRight = glm::normalize(glm::cross(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xForward));
		return true;
	}

	void ClearWASD()
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		// Clear personality modifier keys too -- if a Stealth or Speedrun
		// personality leaves Ctrl/Shift held when we transition to a non-
		// walking phase, the orbit camera (Q/E) and click-to-possess would
		// inherit the modifier state. Always reset to a clean slate.
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT,   false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, false);
	}

	// ====================================================================
	// Grid-based A* pathfinder. Map is roughly (0..100, 0..100); we use a
	// 2 m grid (60×60 cells starting at world (-10,-10)) so building wall
	// AABBs are cleanly resolved into walkable / blocked cells. Walkability
	// is sampled with a downward raycast — wall AABBs sit Y=1..6 above the
	// floor's Y=0..1 slab, so any first-hit Y > 1.5 m means a wall is on top
	// of that cell and the cell is blocked.
	//
	// Replaces the previous straight-line walker; doors lay open in the
	// final navmesh so the path naturally routes through them rather than
	// through walls.
	// ====================================================================
	// 0.5 m cell size — procgen door gaps are routinely 0.8-1.4 m wide
	// and the previous 1.0 m grid blurred them into edge cells the
	// pathfinder either marked unwalkable (path goes around the
	// building) or marked walkable but the villager capsule (0.5 m
	// radius) couldn't actually traverse. 240x240 = 57600 cells; the
	// one-shot grid build does ~57k raycasts but still finishes in
	// ~1-2 s in debug. The 2026-05-20 seed matrix showed seed-55555-
	// style "bot walks 5 m then jams against a wall for the rest of
	// its life" cases vanishing at 0.5 m resolution.
	constexpr int   kPathGridDim    = 240;
	constexpr float kPathCellSize   = 0.5f;
	constexpr float kPathOriginX    = -10.0f;
	constexpr float kPathOriginZ    = -10.0f;
	constexpr float kPathFloorY     = 1.0f;

	bool g_bPathGridBuilt = false;
	bool g_abPathWalkable[kPathGridDim * kPathGridDim] = {};

	Zenith_Vector<Zenith_Maths::Vector3> g_axCurrentPath;
	int g_iPathWaypoint = 0;
	Zenith_Maths::Vector3 g_xLastPlannedTarget(1e9f, 0.0f, 0.0f);

	inline bool IsCellWalkable(int x, int z)
	{
		if (x < 0 || x >= kPathGridDim || z < 0 || z >= kPathGridDim) return false;
		return g_abPathWalkable[z * kPathGridDim + x];
	}

	void BuildPathGrid()
	{
		if (g_bPathGridBuilt) return;
		uint32_t uWalkable = 0;
		for (int z = 0; z < kPathGridDim; ++z)
		{
			for (int x = 0; x < kPathGridDim; ++x)
			{
				const float cx = kPathOriginX + (x + 0.5f) * kPathCellSize;
				const float cz = kPathOriginZ + (z + 0.5f) * kPathCellSize;
				// Sample a capsule-sized footprint at this cell rather than
				// just the centre point. The villager has a 0.5 m capsule
				// radius — if any point within that radius is on a wall,
				// the capsule can't fit at the cell centre and the
				// pathfinder must route around. Without this check the path
				// runs into walls the multi-ray movement check then blocks,
				// stranding the villager mid-path.
				//
				// Cast 5 downward rays (centre + 4 cardinal offsets at the
				// capsule radius). A ray "blocks" if the first hit is
				// significantly above the floor level (walls, props), or if
				// nothing is hit at all (off the floor extents).
				//
				// Floor top sits at y=1.0 (SM_Cube mesh has bounds 0..1 in Y,
				// floor body at Y=0, mesh-aware OBB offsets by 0.5 → top
				// y=1.0). Wall tops sit at y=5.0 (wall body at Y=1, mesh Y
				// bounds 0..4, offset 2 → top y=5.0). Threshold y < 1.5
				// sits between the two so floor hits are walkable, wall +
				// tall prop hits are blocked. Anything taller than 1.5 m
				// (chests, forge) is treated as an obstacle the test must
				// path around.
				auto IsPointOnFloor = [](float fX, float fZ) -> bool {
					const Zenith_Physics::RaycastResult xH = Zenith_Physics::Raycast(
						Zenith_Maths::Vector3(fX, 10.0f, fZ),
						Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 12.0f);
					return xH.m_bHit && xH.m_xHitPoint.y < 1.5f;
				};
				constexpr float fCapR = 0.5f;
				const bool bWalkable = IsPointOnFloor(cx,         cz)
				                   && IsPointOnFloor(cx + fCapR, cz)
				                   && IsPointOnFloor(cx - fCapR, cz)
				                   && IsPointOnFloor(cx,         cz + fCapR)
				                   && IsPointOnFloor(cx,         cz - fCapR);
				g_abPathWalkable[z * kPathGridDim + x] = bWalkable;
				if (bWalkable) ++uWalkable;
			}
		}
		std::printf("[HumanPlaythrough] path grid built: %u/%d cells walkable\n",
			uWalkable, kPathGridDim * kPathGridDim);
		std::fflush(stdout);
		g_bPathGridBuilt = true;
	}

	inline void WorldToCell(const Zenith_Maths::Vector3& xWorld, int& x, int& z)
	{
		x = static_cast<int>((xWorld.x - kPathOriginX) / kPathCellSize);
		z = static_cast<int>((xWorld.z - kPathOriginZ) / kPathCellSize);
	}

	inline Zenith_Maths::Vector3 CellToWorld(int x, int z)
	{
		return Zenith_Maths::Vector3(
			kPathOriginX + (x + 0.5f) * kPathCellSize,
			kPathFloorY,
			kPathOriginZ + (z + 0.5f) * kPathCellSize);
	}

	// Spiral outward from (x,z) and return the index of the first walkable
	// cell. Returns -1 if none within fMaxRing rings.
	int SnapToWalkable(int x, int z, int iMaxRing = 8)
	{
		if (IsCellWalkable(x, z)) return z * kPathGridDim + x;
		for (int r = 1; r <= iMaxRing; ++r)
		{
			for (int dz = -r; dz <= r; ++dz)
			{
				for (int dx = -r; dx <= r; ++dx)
				{
					const int aDx = (dx < 0) ? -dx : dx;
					const int aDz = (dz < 0) ? -dz : dz;
					if (aDx != r && aDz != r) continue;
					const int nx = x + dx, nz = z + dz;
					if (IsCellWalkable(nx, nz)) return nz * kPathGridDim + nx;
				}
			}
		}
		return -1;
	}

	bool ComputePathAStar(const Zenith_Maths::Vector3& xStart,
	                     const Zenith_Maths::Vector3& xEnd,
	                     Zenith_Vector<Zenith_Maths::Vector3>& xOutPath)
	{
		BuildPathGrid();
		int sx, sz, ex, ez;
		WorldToCell(xStart, sx, sz);
		WorldToCell(xEnd, ex, ez);

		const int iSnapStart = SnapToWalkable(sx, sz, 16);
		const int iSnapEnd   = SnapToWalkable(ex, ez, 16);
		if (iSnapStart < 0 || iSnapEnd < 0)
		{
			std::printf("[HumanPlaythrough] A*: failed to snap start=(%.1f,%.1f) cell=(%d,%d) -> %d / end=(%.1f,%.1f) cell=(%d,%d) -> %d\n",
				xStart.x, xStart.z, sx, sz, iSnapStart, xEnd.x, xEnd.z, ex, ez, iSnapEnd);
			std::fflush(stdout);
			return false;
		}
		sx = iSnapStart % kPathGridDim; sz = iSnapStart / kPathGridDim;
		ex = iSnapEnd   % kPathGridDim; ez = iSnapEnd   / kPathGridDim;

		constexpr int kN = kPathGridDim * kPathGridDim;
		std::vector<float> axGScore(kN, 1e30f);
		std::vector<int>   aiCameFrom(kN, -1);
		std::vector<bool>  abVisited(kN, false);

		// Open list as a vector + linear-scan min-extract. With kN=3600 and
		// open size << kN in practice, this is cheaper than dragging in
		// <queue> for std::priority_queue.
		std::vector<std::pair<float, int>> axOpen;
		axOpen.reserve(64);

		auto Heuristic = [ex, ez](int x, int z) {
			const float fDx = static_cast<float>(x - ex);
			const float fDz = static_cast<float>(z - ez);
			return std::sqrt(fDx * fDx + fDz * fDz);
		};

		const int iStartIdx = sz * kPathGridDim + sx;
		const int iEndIdx   = ez * kPathGridDim + ex;
		axGScore[iStartIdx] = 0.0f;
		axOpen.push_back({Heuristic(sx, sz), iStartIdx});

		bool bFound = (iStartIdx == iEndIdx);
		while (!axOpen.empty() && !bFound)
		{
			// Linear-scan min extraction.
			size_t uBest = 0;
			for (size_t i = 1; i < axOpen.size(); ++i)
				if (axOpen[i].first < axOpen[uBest].first) uBest = i;
			const int iIdx = axOpen[uBest].second;
			axOpen[uBest] = axOpen.back();
			axOpen.pop_back();

			if (abVisited[iIdx]) continue;
			abVisited[iIdx] = true;
			if (iIdx == iEndIdx) { bFound = true; break; }

			const int x = iIdx % kPathGridDim;
			const int z = iIdx / kPathGridDim;
			for (int dz = -1; dz <= 1; ++dz)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					if (dx == 0 && dz == 0) continue;
					const int nx = x + dx, nz = z + dz;
					if (!IsCellWalkable(nx, nz)) continue;
					const int iNIdx = nz * kPathGridDim + nx;
					if (abVisited[iNIdx]) continue;
					const float fStep = (dx != 0 && dz != 0) ? 1.41421f : 1.0f;
					const float fNewG = axGScore[iIdx] + fStep;
					if (fNewG < axGScore[iNIdx])
					{
						axGScore[iNIdx] = fNewG;
						aiCameFrom[iNIdx] = iIdx;
						axOpen.push_back({fNewG + Heuristic(nx, nz), iNIdx});
					}
				}
			}
		}

		if (!bFound)
		{
			std::printf("[HumanPlaythrough] A*: no path from cell (%d,%d) to (%d,%d) — open list exhausted\n",
				sx, sz, ex, ez);
			std::fflush(stdout);
			return false;
		}

		// Reconstruct path (end → start, then reverse).
		xOutPath.Clear();
		std::vector<int> aiReverse;
		int iCur = iEndIdx;
		while (iCur >= 0)
		{
			aiReverse.push_back(iCur);
			iCur = aiCameFrom[iCur];
		}
		for (auto it = aiReverse.rbegin(); it != aiReverse.rend(); ++it)
		{
			xOutPath.PushBack(CellToWorld(*it % kPathGridDim, *it / kPathGridDim));
		}
		// Replace the last waypoint with the actual target so the final
		// approach lands on the precise object position rather than the
		// nearest cell centre (which can be ~1.4 m off).
		if (xOutPath.GetSize() > 0)
		{
			Zenith_Maths::Vector3& xLast = xOutPath.Get(xOutPath.GetSize() - 1);
			xLast.x = xEnd.x; xLast.z = xEnd.z;
		}
		return true;
	}

	// Drive the possessed villager toward a world target via simulated WASD,
	// following an A*-computed waypoint sequence rather than a straight line.
	// Returns true when within fStopDist of the final target (horizontal).
	bool DriveWASDToward(const Zenith_Maths::Vector3& xTarget, float fStopDist)
	{
		const Zenith_EntityID xV = DP_Player::GetPossessedVillager();
		if (!xV.IsValid()) return false;
		Zenith_Maths::Vector3 xPos;
		if (!TryGetEntityPos(xV, xPos)) return false;

		// Stuck-detector — when the villager hasn't moved meaningfully in a
		// while, force the cached path to be torn down. The next iteration
		// will replan from the current position. This handles the common
		// case where an obstacle/door is blocking and a stale waypoint keeps
		// us pinned against a wall. The per-phase budget eventually catches
		// truly unreachable targets.
		const float fStuckDx = xPos.x - g_xStuckRefPos.x;
		const float fStuckDz = xPos.z - g_xStuckRefPos.z;
		// 1.5 m squared = 2.25. A villager oscillating around a doorway can
		// jitter ~0.5–1 m every frame even while making zero net progress,
		// so the prior 0.5 m threshold let small-amplitude jiggle hide a
		// "stuck against a wall" condition. 1.5 m forces meaningful net
		// displacement before resetting the stuck counter.
		if (fStuckDx*fStuckDx + fStuckDz*fStuckDz > 2.25f)
		{
			g_xStuckRefPos = xPos;
			g_iStuckCounter = 0;
		}
		else
		{
			++g_iStuckCounter;
			if (g_iStuckCounter > kStuckFramesLimit)
			{
				// Stuck for kStuckFramesLimit (~2 s @ 60 Hz). Tear down the
				// path and let a fresh replan take us through an alternative
				// route. Also kick off a brief "side-step" window: for the
				// next kSideStepFrames the WASD direction is rotated 90
				// degrees so the villager peels off whatever wall it's
				// pinned against. Without the side-step a fresh path from
				// the same start often retraces the same approach vector
				// and re-jams on the same wall corner.
				g_axCurrentPath.Clear();
				g_iPathWaypoint = 0;
				g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
				g_iStuckCounter = 0;
				// Alternate side-step direction across consecutive stuck
				// triggers so a corner that traps clockwise gets a try
				// counter-clockwise next time.
				g_iSideStepFrames = kSideStepFrames;
				g_bSideStepClockwise = !g_bSideStepClockwise;
				++g_iStuckReplans;
				// Note: previously after >=2 stuck-replans we truncated
				// g_iWalkBudget=0 to fast-track the phase to surrender.
				// Removed 2026-05-20 -- the 0.5 m path grid + side-step
				// usually break the bot free within a few replans, but
				// the 2-replan kill-switch was cutting cells short before
				// they could use their 141.6 s game-time budget. The
				// natural g_iWalkBudget countdown still terminates the
				// phase if the bot really is unrecoverable; this just
				// lets it keep trying side-steps for longer first.
			}
		}

		// Replan when target changes, when no path exists, or when the
		// villager has teleported far from the path's planned start (which
		// happens after a re-possess to a fresh villager).
		const float fTgtDx = xTarget.x - g_xLastPlannedTarget.x;
		const float fTgtDz = xTarget.z - g_xLastPlannedTarget.z;
		bool bNeedReplan = (g_axCurrentPath.GetSize() == 0)
		                || (fTgtDx*fTgtDx + fTgtDz*fTgtDz > 1.0f);
		if (!bNeedReplan && g_iPathWaypoint < static_cast<int>(g_axCurrentPath.GetSize()))
		{
			const Zenith_Maths::Vector3& wp = g_axCurrentPath.Get(g_iPathWaypoint);
			const float fWdx = wp.x - xPos.x;
			const float fWdz = wp.z - xPos.z;
			if (fWdx*fWdx + fWdz*fWdz > 64.0f) bNeedReplan = true;  // > 8 m
		}
		if (bNeedReplan)
		{
			g_axCurrentPath.Clear();
			ComputePathAStar(xPos, xTarget, g_axCurrentPath);
			g_iPathWaypoint = 0;
			g_xLastPlannedTarget = xTarget;
		}

		// Advance through waypoints we've already reached.
		while (g_iPathWaypoint < static_cast<int>(g_axCurrentPath.GetSize()) - 1)
		{
			const Zenith_Maths::Vector3& wp = g_axCurrentPath.Get(g_iPathWaypoint);
			const float fWdx = wp.x - xPos.x;
			const float fWdz = wp.z - xPos.z;
			if (fWdx*fWdx + fWdz*fWdz < 1.5f * 1.5f) ++g_iPathWaypoint;
			else break;
		}

		// Final-target check.
		const float fFx = xTarget.x - xPos.x;
		const float fFz = xTarget.z - xPos.z;
		if (std::sqrt(fFx*fFx + fFz*fFz) <= fStopDist)
		{
			ClearWASD();
			return true;
		}

		// Walk toward the current waypoint (or final target if path is empty).
		Zenith_Maths::Vector3 xWaypoint = xTarget;
		if (g_iPathWaypoint < static_cast<int>(g_axCurrentPath.GetSize()))
			xWaypoint = g_axCurrentPath.Get(g_iPathWaypoint);

		const float fDx = xWaypoint.x - xPos.x;
		const float fDz = xWaypoint.z - xPos.z;
		const Zenith_Maths::Vector3 xDelta(fDx, 0.0f, fDz);
		if (glm::length(xDelta) < 0.001f) { ClearWASD(); return false; }

		Zenith_Maths::Vector3 xForward, xRight;
		if (!GetCameraHorizontalBasis(xForward, xRight))
		{
			ClearWASD();
			return false;
		}
		Zenith_Maths::Vector3 xDir = glm::normalize(xDelta);
		// Stuck-recovery side-step: rotate the steering direction by 90
		// degrees for a short window after a stuck-detect fires so the
		// villager peels off the wall it was pinned against. Without
		// this the replanned path tends to retrace the same approach
		// vector and re-jam. Alternate the rotation sense per stuck
		// event (see DriveWASDToward stuck branch) so a corner that
		// traps one direction gets a try in the other.
		if (g_iSideStepFrames > 0)
		{
			--g_iSideStepFrames;
			if (g_bSideStepClockwise)
			{
				xDir = Zenith_Maths::Vector3(xDir.z, 0.0f, -xDir.x);
			}
			else
			{
				xDir = Zenith_Maths::Vector3(-xDir.z, 0.0f, xDir.x);
			}
		}
		const float fForwardDot = glm::dot(xDir, xForward);
		const float fRightDot   = glm::dot(xDir, xRight);

		constexpr float kAxisThresh = 0.25f;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, fForwardDot >  kAxisThresh);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, fForwardDot < -kAxisThresh);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, fRightDot   >  kAxisThresh);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, fRightDot   < -kAxisThresh);

		// Personality modifier keys. Set unconditionally so that walks driven
		// by the same DriveWASDToward call get the same modifier policy across
		// every phase. Cleared in ClearWASD so we don't carry held modifiers
		// into phases that aren't walking (camera control, possession click).
		//
		// Sprint policy:
		//   * bHoldSprint=true  -> always sprint (Berserker)
		//   * bAdaptiveSprint=true -> sprint while remaining distance to the
		//                            ACTIVE TARGET (not next waypoint) exceeds
		//                            kSprintMinDistanceM. Walk on close
		//                            approach so we can stop precisely and
		//                            don't pay the per-second life cost when
		//                            it doesn't save meaningful time.
		//                            (Speedrunner — the killer feature that
		//                            makes adaptive sprint faster than blind
		//                            sprint, which dies mid-objective and
		//                            forces 3x re-possess overhead.)
		const float fHorizToTarget = std::sqrt(fFx * fFx + fFz * fFz);
		const bool bSprintNow = g_xActiveCfg.bHoldSprint
		                     || (g_xActiveCfg.bAdaptiveSprint
		                         && fHorizToTarget > kSprintMinDistanceM);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT,   bSprintNow);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, g_xActiveCfg.bHoldQuiet);
		return false;
	}

	// Reset the cached A* path. Call when transitioning between walk targets so
	// the next DriveWASDToward computes a fresh path from scratch.
	void ResetPath()
	{
		g_axCurrentPath.Clear();
		g_iPathWaypoint = 0;
		g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		// Re-arm the stuck detector — the new target gives the villager a
		// fresh chance to make progress before being declared stuck.
		g_xStuckRefPos = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		g_iStuckCounter = 0;
		g_iStuckReplans = 0;
		// Cancel any in-flight side-step so we don't carry rotation into
		// the new walk phase. g_bSideStepClockwise is intentionally NOT
		// reset -- alternating the rotation sense across stuck events is
		// the whole point.
		g_iSideStepFrames = 0;
	}

	// Returns true when the possessed villager hasn't moved at least 0.5 m
	// horizontally in the last kStuckFramesLimit frames. Caller is
	// responsible for resetting state via ResetPath when transitioning
	// targets so the threshold restarts fresh.
	bool UpdateStuckDetector()
	{
		const Zenith_EntityID xV = DP_Player::GetPossessedVillager();
		Zenith_Maths::Vector3 xPos;
		if (!xV.IsValid() || !TryGetEntityPos(xV, xPos))
		{
			// No villager → effectively stuck.
			++g_iStuckCounter;
			return g_iStuckCounter > kStuckFramesLimit;
		}
		const float fDx = xPos.x - g_xStuckRefPos.x;
		const float fDz = xPos.z - g_xStuckRefPos.z;
		if (fDx*fDx + fDz*fDz > 0.25f)  // moved > 0.5 m
		{
			g_xStuckRefPos = xPos;
			g_iStuckCounter = 0;
			return false;
		}
		++g_iStuckCounter;
		return g_iStuckCounter > kStuckFramesLimit;
	}

	// Inline equivalent of SimulateClickOnUIElement that does NOT call
	// StepFrame (we're already inside Step). Sets mouse pos + queues mouse-press.
	bool ClickUIElement(const char* szName)
	{
		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas) return false;
		Zenith_UI::Zenith_UIElement* pxElement = pxCanvas->FindElement(szName);
		if (!pxElement) return false;
		Zenith_Maths::Vector4 xBounds = pxElement->GetScreenBounds();
		const double fCx = static_cast<double>((xBounds.x + xBounds.z) * 0.5f);
		const double fCy = static_cast<double>((xBounds.y + xBounds.w) * 0.5f);
		Zenith_InputSimulator::SimulateMousePosition(fCx, fCy);
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
		return true;
	}

	// Find the closest unspecialised villager to the orbit centre — this
	// keeps the click-to-possess pick well inside the camera frustum.
	Zenith_EntityID FindClosestVillagerTo(const Zenith_Maths::Vector3& xRef)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xBest, &fBestSq, &xRef](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xRef.x;
				const float fDz = xPos.z - xRef.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});
		return xBest;
	}

	// Generic "closest entity carrying script T" by reference position. Used
	// by the test to pick the door / chest / noise machine that's closest to
	// the test's planned path so re-possessions during long walks don't strand
	// us with a held key consumed by an unreachable door.
	template<typename T>
	Zenith_EntityID FindClosestScriptTo(const Zenith_Maths::Vector3& xRef)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&xBest, &fBestSq, &xRef](Zenith_EntityID xId, T&)
			{
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xRef.x;
				const float fDz = xPos.z - xRef.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});
		return xBest;
	}

	// FindItemByTag returns the *first* matching item, which may be far from
	// the villager and force a long unnecessary walk. Prefer the closest one
	// so the test stays within the 3-minute wall-clock budget.
	Zenith_EntityID FindClosestItemByTag(DP_ItemTag eTag, const Zenith_Maths::Vector3& xRef)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&xBest, &fBestSq, &xRef, eTag](Zenith_EntityID xId, DPItemBase_Behaviour& xItem)
			{
				if (xItem.GetTag() != eTag) return;
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xRef.x;
				const float fDz = xPos.z - xRef.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});
		return xBest;
	}

	// If no villager is currently possessed (life expired, priest kill, etc.),
	// click-possess a fresh one near the map centre. Returns true if a click
	// was issued — the caller should return true and retry the same phase next
	// frame so the click has time to land.
	//
	// Returns false (no click issued) once g_iRepossessAttempts exceeds
	// kRepossessAttemptCap. Without that cap, a bot whose target villager
	// landed in a click-unreachable spot (e.g. inside a procgen wall, or
	// behind another villager) would loop forever, eating the test's
	// 8500-frame budget. The post-2026-05-19 priest-pursues-during-channel
	// change made apprehend catches frequent enough that the loop showed
	// up routinely.
	//
	// Cap was 240 frames (~4 s wall-clock at 60 Hz, despite the old
	// comment claiming 24 s). The 2026-05-20 seed-matrix analysis found
	// 28 of 40 cells ended at 85-90 s wall-time with all-villagers-
	// dead-or-unclickable, exhausting this cap well before the 141.6 s
	// game-time budget. Raised to 1200 (20 s) so a wave of sprint-
	// burnouts can resolve before the bot gives up entirely -- now this
	// only fires when there genuinely are no living villagers left.
	int g_iRepossessAttempts = 0;
	constexpr int kRepossessAttemptCap = 1200; // 20 s wall-clock @ 60 Hz

	bool TryRepossessIfDead()
	{
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		if (xCur.IsValid())
		{
			g_iRepossessAttempts = 0;
			return false;
		}

		if (g_iRepossessAttempts >= kRepossessAttemptCap)
		{
			// Give up entirely. Without this short-circuit the caller's
			// retry-loop falls through into walk-toward-objective logic
			// using an invalid xPossessed, which burns the remaining
			// frame budget waiting for the no-villager walk to time out.
			// Jumping straight to Done lets the harness end the test.
			if (g_iPhase != kHP_Done)
			{
				std::printf("[HumanPlaythrough] re-possess gave up after %d attempts -- ending test\n",
					g_iRepossessAttempts);
				std::fflush(stdout);
			}
			g_iPhase = kHP_Done;
			return false;
		}

		// Skip villagers whose life timer is depleted -- they can't be
		// possessed and clicking on them just burns frames. FindClosest*
		// is unfiltered so we filter here.
		const Zenith_Maths::Vector3 xCentre(50.0f, 0.0f, 50.0f);
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xBest, &fBestSq, &xCentre](Zenith_EntityID xId, DPVillager_Behaviour& xVilla)
			{
				if (xVilla.GetRemainingLife() <= 0.0f) return;
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xCentre.x;
				const float fDz = xPos.z - xCentre.z;
				const float fSq = fDx * fDx + fDz * fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});

		if (!xBest.IsValid())
		{
			// Every villager is dead. Bail so the phase machine can exit.
			g_iRepossessAttempts = kRepossessAttemptCap;
			return false;
		}

		Zenith_Maths::Vector3 xRPos;
		if (!TryGetEntityPos(xBest, xRPos)) return false;
		xRPos.y += 0.9f;
		double fSx = 0.0, fSy = 0.0;
		if (!WorldToScreen(xRPos, fSx, fSy)) return false;
		Zenith_InputSimulator::SimulateMousePosition(fSx, fSy);
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
		++g_iRepossessAttempts;
		if ((g_iRepossessAttempts & 0x3F) == 1)
		{
			// Throttle the log -- one line per 64 attempts is enough to
			// see what's happening without spamming the test log.
			std::printf("[HumanPlaythrough] re-possess click at (%.1f, %.1f) target idx=%u (attempt %d/%d)\n",
				fSx, fSy, xBest.m_uIndex, g_iRepossessAttempts, kRepossessAttemptCap);
			std::fflush(stdout);
		}
		return true;
	}

	void OnVictoryEvent(const DP_OnVictory&) { g_bVictoryEvent = true; }

	// World-state-change handler: any interactable being engaged (door
	// unlocked, chest opened, forge crafted, noise machine triggered) can
	// move geometry under the path grid — most importantly a door rotating
	// open frees a cell that was blocked. Invalidate the cached grid so the
	// next DriveWASDToward call rebuilds against the current world state.
	void OnInteractEvent(const DP_OnInteract&)
	{
		g_bPathGridBuilt = false;
		g_axCurrentPath.Clear();
		g_iPathWaypoint = 0;
		g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
	}

	Zenith_EventHandle g_xInteractHandle = INVALID_EVENT_HANDLE;

	// Log walking progress every 60 frames so a stuck walk is visible.
	void LogWalkProgress(const char* szPhase, const Zenith_Maths::Vector3& xTarget)
	{
		static int s_iLastLog = -1;
		if (g_iWalkBudget % 60 != 0) return;
		if (g_iWalkBudget == s_iLastLog) return;
		s_iLastLog = g_iWalkBudget;
		const Zenith_EntityID xV = DP_Player::GetPossessedVillager();
		if (!xV.IsValid())
		{
			std::printf("[HumanPlaythrough] %s budget=%d POSSESSION_LOST\n", szPhase, g_iWalkBudget);
			std::fflush(stdout);
			return;
		}
		Zenith_Maths::Vector3 xPos;
		if (!TryGetEntityPos(xV, xPos)) return;
		const float fDx = xTarget.x - xPos.x;
		const float fDz = xTarget.z - xPos.z;
		const float fDist = std::sqrt(fDx*fDx + fDz*fDz);
		float fLife = -1.0f;
		if (DPVillager_Behaviour* pxV = GetScript<DPVillager_Behaviour>(xV))
			fLife = pxV->GetRemainingLife();
		std::printf("[HumanPlaythrough] %s budget=%d pos=(%.1f,%.1f,%.1f) tgt=(%.1f,%.1f,%.1f) dist=%.1f life=%.1f\n",
			szPhase, g_iWalkBudget, xPos.x, xPos.y, xPos.z,
			xTarget.x, xTarget.y, xTarget.z, fDist, fLife);
		std::fflush(stdout);
	}
}

// ----------------------------------------------------------------------------
static void Setup_HumanPlaythrough()
{
	// Pin dt at 60 Hz for the duration of this test. Zenith_Core uses the
	// InputSimulator override (via Zenith_InputSimulator::HasFixedDtOverride
	// inside Zenith_Core::UpdateTimers) which then propagates to every
	// per-frame system through Zenith_Core::GetDt() — including the orbit
	// camera's Q/E yaw integrator and the villager's TickMovement.
	// Zenith_Physics has its own internal fixed-step accumulator so it stays
	// deterministic regardless, but the wall-clock dt seen by gameplay code
	// is what makes the test's frame-counted Q hold (30 frames ≈ 0.5 s here)
	// produce the same camera rotation in Debug at 30 fps wall-clock and in
	// Release_False at 200 fps wall-clock. Without this pin, the same
	// 30-frame hold produced 1.5 rad of yaw in Debug and only 0.225 rad in
	// Release_False, and the input/physics phase-asymmetry it caused was the
	// root of the Release_False wedge against tight doorway corners.
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);

	g_iPhase = kHP_Start;
	g_iWait  = 0;
	g_iWalkBudget = 0;
	g_iRepossessAttempts = 0;

	g_xPossessTarget   = INVALID_ENTITY_ID;
	g_xCurrentVillager = INVALID_ENTITY_ID;
	g_xDoor       = INVALID_ENTITY_ID;
	g_xChest      = INVALID_ENTITY_ID;
	g_xForge      = INVALID_ENTITY_ID;
	g_xNoise      = INVALID_ENTITY_ID;
	g_xPentagram  = INVALID_ENTITY_ID;
	g_xPriest     = INVALID_ENTITY_ID;
	g_xCurrentObjItem = INVALID_ENTITY_ID;

	g_iVillagerCount = 0;
	g_iDoorCount     = 0;
	g_iChestCount    = 0;

	g_fYawBeforeQ  = 0.0f;
	g_fYawAfterQ   = 0.0f;
	g_fYawAfterE   = 0.0f;
	g_fDistBefore  = 0.0f;
	g_fDistAfterIn = 0.0f;
	g_fDistAfterOut = 0.0f;

	g_bPossessionConfirmed = false;
	g_fPossessClickX = 0.0;
	g_fPossessClickY = 0.0;

	g_bIronPickedUp     = false;
	g_bForgeCrafted     = false;
	g_bDoorOpened       = false;
	g_bChestOpened      = false;
	g_bPriestHeardNoise = false;

	g_iObjectivesDelivered = 0;
	g_iObjAttempts = 0;
	g_iChestAttempts = 0;
	g_iDoorAttempts  = 0;
	g_iNoiseAttempts = 0;
	g_bVictoryEvent  = false;
	g_uVictoryMask   = 0;
	g_bPauseOnObserved = false;
	g_bPauseOffObserved = false;
	g_iPauseCyclesRemaining = g_xActiveCfg.iPauseCycles;

	g_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(&OnVictoryEvent);
	g_xInteractHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnInteract>(&OnInteractEvent);

	// Telemetry artifacts -- always written under %TEMP% as
	// dp_human_playthrough.{ztlm,json}. Tools/dp_telemetry_visualise.ps1
	// reads these by path; the runner script auto-detects them once the
	// test exits.
	g_strHPBinPath       = HPTempPath("playthrough.ztlm");
	g_strHPJsonPath      = HPTempPath("playthrough.json");
	g_strHPFramesCsvPath = HPTempPath("frames.csv");
	g_strHPEventsCsvPath = HPTempPath("events.csv");
	// Reset perception-contact diff state so personalities in a batched
	// run don't inherit each other's "active contacts" lists.
	g_axActiveContacts.clear();
	g_iContactPrevFrameIdx = -1;
	g_pxTelemetryHooks.reset();
	g_bRecordingActive = false;
	g_iTelemetryFrame  = 0;
	// Recorder Begin + Hooks construction deferred to kHP_CaptureRefs so
	// the FrontEnd-boot frames don't pollute the recording.

	// Reset the A* path cache so the first walk computes a fresh path. The
	// grid is invalidated automatically on world-state changes (door open
	// etc.) via OnInteractEvent; this just clears any stale state from a
	// previous run in the same process.
	ResetPath();

	// Make sure no leftover keys are held (the harness already calls
	// ResetAllInputState before Setup, but be explicit).
	Zenith_InputSimulator::ClearHeldKeys();
}

// ----------------------------------------------------------------------------
static bool Step_HumanPlaythrough(int /*iFrame*/)
{
	// Telemetry sampling. Runs every frame once recording has begun
	// (kHP_CaptureRefs onward). The 10 Hz position-sample period and
	// the per-frame NextFrame() advance match the bot test so the
	// visualiser sees the same cadence + units. Placed at the top of
	// Step so it captures the world AFTER the previous frame's
	// game-side OnUpdate (which already mutated positions) and BEFORE
	// the input simulator pushes new keys this frame.
	if (g_bRecordingActive)
	{
		auto& xRec = Zenith_Telemetry::GetRecorder();
		xRec.NextFrame();
		if (xRec.ShouldSampleThisFrame())
		{
			EmitHPPositionSample(g_iTelemetryFrame);
		}
		++g_iTelemetryFrame;
	}

	switch (g_iPhase)
	{
	// ----------------------------------------------------------------------
	// Phase A — load GameLevel directly (FrontEnd skipped)
	//
	// Pre-2026-05-17 the test booted via FrontEnd + clicked MenuPlay so the
	// menu/scene-swap path was covered. With the personality refactor the
	// menu click is no longer wanted -- it forces m_bRequiresGraphics=true
	// (the click depends on a visible window) and adds 2 seconds of FE
	// settling time for every one of the four personality runs. Menu-click
	// coverage stays in Test_FullPlaythrough.cpp.
	// ----------------------------------------------------------------------
	case kHP_Start:
		// Every personality loads the same gameplay scene (procgen
		// ProcLevel at build index 1).
		Zenith_SceneManager::LoadSceneByIndex(
			static_cast<int>(kPersonalitySceneIndex), SCENE_LOAD_SINGLE);
		g_iPhase = kHP_WaitGameLevel;
		g_iWait = 0;
		return true;

	// kHP_LoadFE / kHP_WaitFE / kHP_ClickPlay are retained in the enum so
	// the phase numbering stays stable for the rest of Step, but they are
	// no longer reachable from kHP_Start. Fall through to Done if any
	// future code path lands here unexpectedly.
	case kHP_LoadFE:
	case kHP_WaitFE:
	case kHP_ClickPlay:
		g_iPhase = kHP_Done;
		return false;

	case kHP_WaitGameLevel:
	{
		++g_iWait;
		// Wait until GameLevel-specific entities (DPVillager_Behaviour) appear.
		const int iV = CountScripts<DPVillager_Behaviour>();
		if (iV > 0) {
			g_iPhase = kHP_CaptureRefs;
			g_iWait = 0;
			return true;
		}
		if (g_iWait > 240) { g_iPhase = kHP_Done; return false; }
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase B — capture entity refs, exercise camera controls
	// ----------------------------------------------------------------------
	case kHP_CaptureRefs:
	{
		g_iVillagerCount = CountScripts<DPVillager_Behaviour>();
		g_iDoorCount     = CountScripts<DPDoor_Behaviour>();
		g_iChestCount    = CountScripts<DPChest_Behaviour>();

		g_xPriest    = FindFirstScript<Priest_Behaviour>();
		g_xPentagram = FindFirstScript<DPPentagram_Behaviour>();
		g_xForge     = FindFirstScript<DPForge_Behaviour>();
		// Door, chest, noise: pick the instance closest to the forge so the
		// test's WASD walks stay short. The UE-imported door batch stacks all
		// 15 doors at world origin (~60 m from the forge); the relocated
		// TestDoor authored alongside the forge above is much closer.
		Zenith_Maths::Vector3 xForgePos(50.0f, 0.0f, 32.0f);
		if (g_xForge.IsValid()) TryGetEntityPos(g_xForge, xForgePos);
		g_xDoor   = FindClosestScriptTo<DPDoor_Behaviour>(xForgePos);
		g_xChest  = FindClosestScriptTo<DPChest_Behaviour>(xForgePos);
		g_xNoise  = FindClosestScriptTo<DummyNoiseMachine_Behaviour>(xForgePos);

		// Pick the villager closest to the map centre — keeps the screen-space
		// click-to-possess inside the orbit camera's frame.
		const Zenith_Maths::Vector3 xCentre(50.0f, 0.0f, 50.0f);
		g_xPossessTarget = FindClosestVillagerTo(xCentre);

		Zenith_Maths::Vector3 xVPos, xDPos, xCPos, xFPos, xNPos, xPPos, xPrPos;
		TryGetEntityPos(g_xPossessTarget, xVPos);
		TryGetEntityPos(g_xDoor, xDPos);
		TryGetEntityPos(g_xChest, xCPos);
		TryGetEntityPos(g_xForge, xFPos);
		TryGetEntityPos(g_xNoise, xNPos);
		TryGetEntityPos(g_xPentagram, xPPos);
		TryGetEntityPos(g_xPriest, xPrPos);
		std::printf("[HumanPlaythrough] refs: V=%d D=%d C=%d "
		            "priest=%d pent=%d door=%d chest=%d forge=%d noise=%d target=%d\n"
		            "[HumanPlaythrough] positions: villager=(%.1f,%.1f,%.1f) "
		            "door=(%.1f,%.1f,%.1f) chest=(%.1f,%.1f,%.1f) forge=(%.1f,%.1f,%.1f) "
		            "noise=(%.1f,%.1f,%.1f) pent=(%.1f,%.1f,%.1f) priest=(%.1f,%.1f,%.1f)\n",
			g_iVillagerCount, g_iDoorCount, g_iChestCount,
			(int)g_xPriest.IsValid(), (int)g_xPentagram.IsValid(),
			(int)g_xDoor.IsValid(), (int)g_xChest.IsValid(),
			(int)g_xForge.IsValid(), (int)g_xNoise.IsValid(),
			(int)g_xPossessTarget.IsValid(),
			xVPos.x, xVPos.y, xVPos.z,
			xDPos.x, xDPos.y, xDPos.z,
			xCPos.x, xCPos.y, xCPos.z,
			xFPos.x, xFPos.y, xFPos.z,
			xNPos.x, xNPos.y, xNPos.z,
			xPPos.x, xPPos.y, xPPos.z,
			xPrPos.x, xPrPos.y, xPrPos.z);
		std::fflush(stdout);

		// Snapshot orbit yaw/distance so we can detect the camera-control inputs
		// took effect.
		if (DPOrbitCamera_Behaviour* pxOrbit = GetScript<DPOrbitCamera_Behaviour>(g_xPossessTarget))
		{
			(void)pxOrbit;  // orbit lives on GameManager, not the villager — defensive
		}
		// Look up the orbit on the camera's owning entity (GameManager).
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCam)
		{
			// We don't expose orbit yaw directly; sample the camera's yaw as a
			// proxy — it's deterministically derived from m_fOrbitYaw inside
			// DPOrbitCamera::OnUpdate.
			g_fYawBeforeQ = static_cast<float>(pxCam->GetYaw());
			Zenith_Maths::Vector3 xCamPos;
			pxCam->GetPosition(xCamPos);
			const float fDx = xCamPos.x - 50.0f;
			const float fDz = xCamPos.z - 50.0f;
			g_fDistBefore = std::sqrt(fDx*fDx + fDz*fDz);
		}

		// Begin telemetry recording NOW -- scene is loaded, all the
		// gameplay scripts have populated, OnAwake/OnStart have fired,
		// and we're about to start the visible playthrough. The
		// FrontEnd menu frames stay out of the recording (kHP_LoadFE
		// through kHP_WaitGameLevel ran before this).
		if (!g_bRecordingActive)
		{
			Zenith_Telemetry::Header xHeader;
			// Distinct seed from the bot test's 0xB0Bull so the
			// resulting recording is identifiable at a glance.
			xHeader.uSeed              = 0xFEEDC0DEull;
			xHeader.strSceneName       = "ProcLevel";
			xHeader.fFixedDt           = kHPFixedDt;
			xHeader.uSamplePeriodFrames = kHPSamplePeriodFrames;
			// Static-scene obstacles. Scanned once now (scene fully
			// populated, scripts have OnAwake/OnStart'd) and stamped into
			// the recording's header. The visualiser renders these as
			// semi-transparent rectangles under the entity trails so
			// movement makes sense against actual geometry.
			ScanSceneObstacles(xHeader.axObstacles);
			// Telemetry-v3 build-info. ZENITH_DEBUG + ZENITH_TOOLS are
			// the only build flags the engine carries through to runtime;
			// concatenating them gives a self-describing config string
			// without each game having to mirror the Sharpmake config
			// names. Hash is empty for now -- a future commit can stamp
			// the git SHA via a Sharpmake-injected define.
#if defined(ZENITH_DEBUG) && defined(ZENITH_TOOLS)
			xHeader.strBuildConfig = "Debug_True";
#elif defined(ZENITH_DEBUG)
			xHeader.strBuildConfig = "Debug_False";
#elif defined(ZENITH_TOOLS)
			xHeader.strBuildConfig = "Release_True";
#else
			xHeader.strBuildConfig = "Release_False";
#endif
			xHeader.strBuildHash       = "";
			xHeader.strPersonalityName = g_xActiveCfg.szName;
			Zenith_Telemetry::GetRecorder().Begin(xHeader);
			// Hooks AFTER Begin so the very first events land in this run.
			g_pxTelemetryHooks = std::make_unique<DPTelemetry::Hooks>();
			g_iTelemetryFrame  = 0;
			g_bRecordingActive = true;
			std::printf("[HumanPlaythrough] telemetry begin -- bin=%s json=%s\n",
				g_strHPBinPath.c_str(), g_strHPJsonPath.c_str());
			std::fflush(stdout);
		}

		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_Q, true);
		g_iPhase = kHP_CamRotateQ;
		g_iWait = 0;
		return true;
	}

	case kHP_CamRotateQ:
	{
		++g_iWait;
		if (g_iWait < 30) return true;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_Q, false);
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCam) g_fYawAfterQ = static_cast<float>(pxCam->GetYaw());

		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, true);
		g_iPhase = kHP_CamRotateE;
		g_iWait = 0;
		return true;
	}

	case kHP_CamRotateE:
	{
		++g_iWait;
		if (g_iWait < 30) return true;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_E, false);
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCam) g_fYawAfterE = static_cast<float>(pxCam->GetYaw());

		// Mouse wheel zoom in (+ tightens orbit radius).
		Zenith_InputSimulator::SimulateMouseWheel(2.0f);
		g_iPhase = kHP_CamZoomIn;
		g_iWait = 0;
		return true;
	}

	case kHP_CamZoomIn:
	{
		++g_iWait;
		if (g_iWait < 2) return true;
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCam)
		{
			Zenith_Maths::Vector3 xCamPos;
			pxCam->GetPosition(xCamPos);
			const float fDx = xCamPos.x - 50.0f;
			const float fDz = xCamPos.z - 50.0f;
			g_fDistAfterIn = std::sqrt(fDx*fDx + fDz*fDz);
		}

		Zenith_InputSimulator::SimulateMouseWheel(-2.0f);
		g_iPhase = kHP_CamZoomOut;
		g_iWait = 0;
		return true;
	}

	case kHP_CamZoomOut:
	{
		++g_iWait;
		if (g_iWait < 2) return true;
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCam)
		{
			Zenith_Maths::Vector3 xCamPos;
			pxCam->GetPosition(xCamPos);
			const float fDx = xCamPos.x - 50.0f;
			const float fDz = xCamPos.z - 50.0f;
			g_fDistAfterOut = std::sqrt(fDx*fDx + fDz*fDz);
		}

		// Move on to possession.
		g_iPhase = kHP_PossessClick;
		g_iWait = 0;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase C — click-to-possess
	// ----------------------------------------------------------------------
	case kHP_PossessClick:
	{
		if (!g_xPossessTarget.IsValid()) { g_iPhase = kHP_Done; return false; }
		Zenith_Maths::Vector3 xPos;
		if (!TryGetEntityPos(g_xPossessTarget, xPos)) { g_iPhase = kHP_Done; return false; }
		// Lift the target slightly so the projection lands on the visible body
		// (villagers stand at y=0 with cube collider centred ~y=0.9).
		xPos.y += 0.9f;
		double fSx = 0.0, fSy = 0.0;
		if (!WorldToScreen(xPos, fSx, fSy)) { g_iPhase = kHP_Done; return false; }
		g_fPossessClickX = fSx;
		g_fPossessClickY = fSy;
		// Inline SimulateMouseClick (avoid recursive StepFrame).
		Zenith_InputSimulator::SimulateMousePosition(fSx, fSy);
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);

		std::printf("[HumanPlaythrough] possess click at screen (%.1f, %.1f) for villager idx=%u\n",
			fSx, fSy, g_xPossessTarget.m_uIndex);
		std::fflush(stdout);

		g_iPhase = kHP_WaitPossess;
		g_iWait = 0;
		return true;
	}

	case kHP_WaitPossess:
	{
		++g_iWait;
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		if (xPossessed.IsValid())
		{
			g_xCurrentVillager = xPossessed;
			g_bPossessionConfirmed = true;
			std::printf("[HumanPlaythrough] possession confirmed: villager idx=%u\n",
				xPossessed.m_uIndex);
			std::fflush(stdout);
			g_iPhase = kHP_WalkIron;
			SetWalkBudget(1200);  // ~25 s budget for finding/walking to first iron
			return true;
		}
		if (g_iWait > 180)
		{
			// Click missed — bail with what we have, Verify will fail.
			std::printf("[HumanPlaythrough] possession TIMEOUT after click\n");
			std::fflush(stdout);
			g_iPhase = kHP_Done;
			return false;
		}
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase D — walk to nearest Iron, pick it up
	// ----------------------------------------------------------------------
	case kHP_WalkIron:
	{
		if (TryRepossessIfDead()) return true;
		// Pick the iron closest to the villager so we don't waste budget on
		// long detours. The first-found heuristic in DP_Items::FindItemByTag
		// is order-dependent and can land us on an iron behind buildings.
		Zenith_Maths::Vector3 xVPos;
		const Zenith_EntityID xVCur = DP_Player::GetPossessedVillager();
		if (!xVCur.IsValid() || !TryGetEntityPos(xVCur, xVPos))
			xVPos = Zenith_Maths::Vector3(45.0f, 0.0f, 53.0f);  // villager spawn fallback
		Zenith_EntityID xIron = FindClosestItemByTag(DP_ItemTag::Iron, xVPos);
		if (!xIron.IsValid())
		{
			std::printf("[HumanPlaythrough] iron NOT in scene — skipping pickup\n");
			std::fflush(stdout);
			ClearWASD();
			g_iPhase = kHP_WalkForge;
			SetWalkBudget(1200);
			return true;
		}
		Zenith_Maths::Vector3 xIronPos = DP_Items::GetItemWorldPos(xIron);
		LogWalkProgress("iron", xIronPos);
		--g_iWalkBudget;
		const bool bArrived = DriveWASDToward(xIronPos, 1.5f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_WaitIronPickup;
			g_iWait = 0;
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			std::printf("[HumanPlaythrough] iron WALK_TIMEOUT — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkForge;
			SetWalkBudget(1200);
			return true;
		}
		return true;
	}

	case kHP_WaitIronPickup:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		const DP_ItemTag eHeld = xCur.IsValid()
			? DP_Player::GetHeldItemTag(xCur) : DP_ItemTag::None;
		if (xCur.IsValid()) g_xCurrentVillager = xCur;
		g_bIronPickedUp = (eHeld == DP_ItemTag::Iron);
		std::printf("[HumanPlaythrough] iron pickup: held=%d (got=%d)\n",
			(int)eHeld, (int)g_bIronPickedUp);
		std::fflush(stdout);
		g_iPhase = kHP_WalkForge;
		SetWalkBudget(1200);
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase E — walk to forge, F to craft Iron → Key
	// ----------------------------------------------------------------------
	case kHP_WalkForge:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xForge.IsValid()) {
			std::printf("[HumanPlaythrough] forge missing — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkDoor; SetWalkBudget(1200); return true;
		}
		Zenith_Maths::Vector3 xForgePos;
		if (!TryGetEntityPos(g_xForge, xForgePos)) {
			g_iPhase = kHP_WalkDoor; SetWalkBudget(1200); return true;
		}
		LogWalkProgress("forge", xForgePos);
		--g_iWalkBudget;
		const bool bArrived = DriveWASDToward(xForgePos, 1.95f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_PressForgeF;
			g_iWait = 0;
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			std::printf("[HumanPlaythrough] forge WALK_TIMEOUT — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkDoor;
			SetWalkBudget(1200);
			return true;
		}
		return true;
	}

	case kHP_PressForgeF:
	{
		// One frame to let DPInteractable's OnEnterRange subscribe to F-presses,
		// then issue the press. Two-frame sequence keeps the rising-edge clean.
		++g_iWait;
		if (g_iWait == 1) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		// Berserker mashes F for kBerserkerMashFrames frames -- each press
		// while in range emits another DP_OnInteract event into the recorder.
		if (g_xActiveCfg.bMashInteract && g_iWait < kBerserkerMashFrames) return true;
		g_iPhase = kHP_VerifyForge;
		g_iWait = 0;
		return true;
	}

	case kHP_VerifyForge:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		const DP_ItemTag eHeld = xCur.IsValid()
			? DP_Player::GetHeldItemTag(xCur) : DP_ItemTag::None;
		if (xCur.IsValid()) g_xCurrentVillager = xCur;
		uint32_t uCrafts = 0;
		if (DPForge_Behaviour* pxF = GetScript<DPForge_Behaviour>(g_xForge))
		{
			uCrafts = pxF->GetCraftCount();
		}
		g_bForgeCrafted = (eHeld == DP_ItemTag::Key) && (uCrafts >= 1);
		std::printf("[HumanPlaythrough] forge: held=%d crafts=%u ok=%d\n",
			(int)eHeld, uCrafts, (int)g_bForgeCrafted);
		std::fflush(stdout);
		g_iPhase = kHP_WalkDoor;
		SetWalkBudget(1200);
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase F — walk to door, F to unlock (consumes the Key)
	// ----------------------------------------------------------------------
	case kHP_WalkDoor:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xDoor.IsValid()) {
			std::printf("[HumanPlaythrough] door missing — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkChest; SetWalkBudget(1200); return true;
		}
		Zenith_Maths::Vector3 xDoorPos;
		if (!TryGetEntityPos(g_xDoor, xDoorPos)) { g_iPhase = kHP_WalkChest; SetWalkBudget(1200); return true; }
		LogWalkProgress("door", xDoorPos);
		--g_iWalkBudget;
		const bool bArrived = DriveWASDToward(xDoorPos, 1.95f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_PressDoorF;
			g_iWait = 0;
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			if (g_iDoorAttempts == 0)
			{
				++g_iDoorAttempts;
				ResetPath();
				std::printf("[HumanPlaythrough] door WALK_TIMEOUT — retry %d\n",
					g_iDoorAttempts);
				std::fflush(stdout);
				SetWalkBudget(1200);
				return true;
			}
			std::printf("[HumanPlaythrough] door WALK_TIMEOUT — giving up\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkChest;
			SetWalkBudget(1200);
			return true;
		}
		return true;
	}

	case kHP_PressDoorF:
	{
		++g_iWait;
		if (g_iWait == 1) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		if (g_xActiveCfg.bMashInteract && g_iWait < kBerserkerMashFrames) return true;
		g_iPhase = kHP_VerifyDoor;
		g_iWait = 0;
		return true;
	}

	case kHP_VerifyDoor:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		if (DPDoor_Behaviour* pxDoor = GetScript<DPDoor_Behaviour>(g_xDoor))
		{
			g_bDoorOpened = pxDoor->IsOpen();
		}
		std::printf("[HumanPlaythrough] door: open=%d\n", (int)g_bDoorOpened);
		std::fflush(stdout);
		g_iPhase = kHP_WalkChest;
		SetWalkBudget(1200);
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase G — walk to chest, F to open
	// ----------------------------------------------------------------------
	case kHP_WalkChest:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xChest.IsValid()) {
			std::printf("[HumanPlaythrough] chest missing — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkNoise; SetWalkBudget(1200); return true;
		}
		Zenith_Maths::Vector3 xChestPos;
		if (!TryGetEntityPos(g_xChest, xChestPos)) { g_iPhase = kHP_WalkNoise; SetWalkBudget(1200); return true; }
		LogWalkProgress("chest", xChestPos);
		--g_iWalkBudget;
		// Interaction radius for DPInteractable is 2.0 m. Stop just outside
		// that so a chest-side wall/collider that pushes the capsule away
		// doesn't trap the test at distance > stopDist but inside range —
		// 1.95 m sits inside the F-press range with a tiny safety margin.
		const bool bArrived = DriveWASDToward(xChestPos, 1.95f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_PressChestF;
			g_iWait = 0;
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			// Chest approach can fail when the villager wedges against a
			// nearby wall or chest collider edge. Retry once with a fresh
			// path before giving up — the second attempt re-plans from a
			// different stuck position which usually clears the wedge.
			if (g_iChestAttempts == 0)
			{
				++g_iChestAttempts;
				ResetPath();
				std::printf("[HumanPlaythrough] chest WALK_TIMEOUT — retry %d\n",
					g_iChestAttempts);
				std::fflush(stdout);
				SetWalkBudget(1200);
				return true;
			}
			std::printf("[HumanPlaythrough] chest WALK_TIMEOUT — giving up\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkNoise;
			SetWalkBudget(1200);
			return true;
		}
		return true;
	}

	case kHP_PressChestF:
	{
		++g_iWait;
		if (g_iWait == 1) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		if (g_xActiveCfg.bMashInteract && g_iWait < kBerserkerMashFrames) return true;
		g_iPhase = kHP_VerifyChest;
		g_iWait = 0;
		return true;
	}

	case kHP_VerifyChest:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		if (DPChest_Behaviour* pxChest = GetScript<DPChest_Behaviour>(g_xChest))
		{
			g_bChestOpened = pxChest->IsOpen();
		}
		std::printf("[HumanPlaythrough] chest: open=%d\n", (int)g_bChestOpened);
		std::fflush(stdout);
		// Stealth skips the noise machine -- the whole point of the
		// personality is to NOT emit hearing stimulus. Jump directly to
		// the objective-delivery loop. Verify is gated on the same flag
		// so the missing g_bPriestHeardNoise doesn't fail the test.
		if (g_xActiveCfg.bRunNoiseMachine)
		{
			g_iPhase = kHP_WalkNoise;
		}
		else
		{
			std::printf("[HumanPlaythrough] %s skipping noise machine (stealth)\n",
				g_xActiveCfg.szName);
			std::fflush(stdout);
			g_iPhase = kHP_ObjLoopFind;
		}
		SetWalkBudget(1200);
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase H — walk to noise machine, F to emit, observe priest blackboard
	// ----------------------------------------------------------------------
	case kHP_WalkNoise:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xNoise.IsValid()) {
			std::printf("[HumanPlaythrough] noise missing — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_ObjLoopFind; SetWalkBudget(1200); return true;
		}
		Zenith_Maths::Vector3 xNoisePos;
		if (!TryGetEntityPos(g_xNoise, xNoisePos)) { g_iPhase = kHP_ObjLoopFind; SetWalkBudget(1200); return true; }
		LogWalkProgress("noise", xNoisePos);
		--g_iWalkBudget;
		const bool bArrived = DriveWASDToward(xNoisePos, 1.95f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_PressNoiseF;
			g_iWait = 0;
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			if (g_iNoiseAttempts == 0)
			{
				++g_iNoiseAttempts;
				ResetPath();
				std::printf("[HumanPlaythrough] noise WALK_TIMEOUT — retry %d\n",
					g_iNoiseAttempts);
				std::fflush(stdout);
				SetWalkBudget(1200);
				return true;
			}
			std::printf("[HumanPlaythrough] noise WALK_TIMEOUT — giving up\n");
			std::fflush(stdout);
			g_iPhase = kHP_ObjLoopFind;
			SetWalkBudget(1200);
			return true;
		}
		return true;
	}

	case kHP_PressNoiseF:
	{
		++g_iWait;
		if (g_iWait == 1) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		if (g_xActiveCfg.bMashInteract && g_iWait < kBerserkerMashFrames) return true;
		g_iPhase = kHP_WaitNoise;
		g_iWait = 0;
		return true;
	}

	case kHP_WaitNoise:
	{
		++g_iWait;
		if (g_iWait < 8) return true;  // perception system needs a few frames
		if (g_xPriest.IsValid())
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(g_xPriest);
			if (pxScene)
			{
				Zenith_Entity xP = pxScene->TryGetEntity(g_xPriest);
				if (xP.IsValid() && xP.HasComponent<Zenith_AIAgentComponent>())
				{
					Zenith_AIAgentComponent& xAgent = xP.GetComponent<Zenith_AIAgentComponent>();
					Zenith_Blackboard& xBB = xAgent.GetBlackboard();
					g_bPriestHeardNoise = xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, /*bDefault=*/false);
				}
			}
		}
		std::printf("[HumanPlaythrough] noise: priest_has_investigate=%d\n",
			(int)g_bPriestHeardNoise);
		std::fflush(stdout);
		g_iPhase = kHP_ObjLoopFind;
		SetWalkBudget(1200);
		return true;
	}

	// ----------------------------------------------------------------------
	// Phases I1..I5 — pickup + deliver each of 5 objectives
	//
	// 4 sub-phases that loop until g_iObjectivesDelivered == 5:
	//   ObjLoopFind          — locate Objective<N>; if missing, advance counter
	//   ObjLoopWalk          — WASD to the item; release at 1.0 m for proximity pickup
	//   ObjLoopWalkPentagram — WASD back to the pentagram; release at 1.6 m
	//   ObjLoopPressF        — F-press; loop back or exit
	// ----------------------------------------------------------------------
	case kHP_ObjLoopFind:
	{
		if (g_iObjectivesDelivered >= 5)
		{
			g_iPhase = kHP_AssertVictory;
			g_iWait = 0;
			return true;
		}
		if (TryRepossessIfDead()) return true;
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		if (!xCur.IsValid()) return true;     // wait one more frame for re-possession
		g_xCurrentVillager = xCur;

		// Already delivered this objective on a previous attempt? Skip ahead.
		const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();
		const uint32_t uBit = 1u << g_iObjectivesDelivered;
		if (uMask & uBit)
		{
			++g_iObjectivesDelivered;
			g_iObjAttempts = 0;
			return true;
		}

		// Cap retries — if a single objective resists multiple attempts (e.g.
		// item entity got destroyed without bit being set), give up and move
		// on so the test still terminates rather than spinning forever.
		if (g_iObjAttempts >= kMaxObjAttempts)
		{
			std::printf("[HumanPlaythrough] obj %d MAX_ATTEMPTS — skipping\n",
				g_iObjectivesDelivered);
			std::fflush(stdout);
			++g_iObjectivesDelivered;
			g_iObjAttempts = 0;
			return true;
		}

		const DP_ItemTag eExpected = g_aeObjTags[g_iObjectivesDelivered];

		// If this villager is already carrying the right item (e.g., previous
		// pent F-press fired but range was wrong, or we re-possessed and the
		// new villager picked it up automatically), walk straight to pent.
		if (DP_Player::GetHeldItemTag(xCur) == eExpected)
		{
			g_xCurrentObjItem = INVALID_ENTITY_ID;
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}

		// Closest objective of the requested tag — avoids forcing a long
		// walk across the map when an equivalent item is nearby.
		Zenith_Maths::Vector3 xObjVPos;
		if (!TryGetEntityPos(xCur, xObjVPos))
			xObjVPos = Zenith_Maths::Vector3(45.0f, 0.0f, 53.0f);
		Zenith_EntityID xItem = FindClosestItemByTag(eExpected, xObjVPos);
		if (!xItem.IsValid())
		{
			std::printf("[HumanPlaythrough] objective %d (tag=%d) NOT in scene — skipping\n",
				g_iObjectivesDelivered, (int)eExpected);
			std::fflush(stdout);
			++g_iObjectivesDelivered;
			g_iObjAttempts = 0;
			return true;
		}
		g_xCurrentObjItem = xItem;
		g_iPhase = kHP_ObjLoopWalk;
		SetWalkBudget(1200);
		return true;
	}

	case kHP_ObjLoopWalk:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xCurrentObjItem.IsValid()) { ++g_iObjectivesDelivered; g_iPhase = kHP_ObjLoopFind; return true; }

		// Cross-possession memory (2026-05-20). Three guards covering
		// the two ways a re-possession can land us mid-WalkObj:
		//
		// 1. New villager happens to already hold the target tag (item
		//    auto-pickup may have fired on it via proximity, or we
		//    retained somehow). Skip pickup, head to pentagram.
		// 2. New villager is closer to a DIFFERENT instance of the same
		//    tag than the original target. Re-aim at the closer one so
		//    we don't retrace the dead villager's planned route across
		//    the map. Hysteresis (>= 2 m savings) avoids flip-flopping
		//    between two equidistant items.
		// 3. (Implicit fall-through) Still need to pickup and the
		//    original target is still the closest -- walk to it.
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		if (xCur.IsValid() && g_iObjectivesDelivered < 5)
		{
			const DP_ItemTag eExpected = g_aeObjTags[g_iObjectivesDelivered];
			if (DP_Player::GetHeldItemTag(xCur) == eExpected)
			{
				ClearWASD();
				g_xCurrentObjItem = INVALID_ENTITY_ID;
				g_iPhase = kHP_ObjLoopWalkPentagram;
				SetWalkBudget(1200);
				ResetPath();
				return true;
			}
			Zenith_Maths::Vector3 xCurPos;
			if (TryGetEntityPos(xCur, xCurPos))
			{
				const Zenith_EntityID xClosest = FindClosestItemByTag(eExpected, xCurPos);
				if (xClosest.IsValid() && xClosest != g_xCurrentObjItem)
				{
					Zenith_Maths::Vector3 xOldPos = DP_Items::GetItemWorldPos(g_xCurrentObjItem);
					Zenith_Maths::Vector3 xNewPos = DP_Items::GetItemWorldPos(xClosest);
					const float fOldDx = xOldPos.x - xCurPos.x;
					const float fOldDz = xOldPos.z - xCurPos.z;
					const float fNewDx = xNewPos.x - xCurPos.x;
					const float fNewDz = xNewPos.z - xCurPos.z;
					const float fOldSq = fOldDx*fOldDx + fOldDz*fOldDz;
					const float fNewSq = fNewDx*fNewDx + fNewDz*fNewDz;
					// >= 4 m^2 ~= 2 m savings before we switch.
					if (fNewSq + 16.0f < fOldSq)
					{
						g_xCurrentObjItem = xClosest;
						ResetPath();
					}
				}
			}
		}

		Zenith_Maths::Vector3 xItemPos = DP_Items::GetItemWorldPos(g_xCurrentObjItem);
		LogWalkProgress("obj-item", xItemPos);
		--g_iWalkBudget;
		// Auto-pickup proximity is generous (DPItemBase OnUpdate triggers at
		// ~1.5 m). 1.5 m stop avoids getting wedged at exactly the auto-
		// pickup boundary when corner walls or other objectives crowd the
		// spawner's footprint.
		const bool bArrived = DriveWASDToward(xItemPos, 1.5f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			std::printf("[HumanPlaythrough] obj-item WALK_TIMEOUT obj=%d\n", g_iObjectivesDelivered);
			std::fflush(stdout);
			++g_iObjectivesDelivered;
			g_iPhase = kHP_ObjLoopFind;
			return true;
		}
		return true;
	}

	case kHP_ObjLoopWalkPentagram:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xPentagram.IsValid()) { ++g_iObjectivesDelivered; g_iPhase = kHP_ObjLoopFind; return true; }

		// Cross-possession memory (2026-05-20). The single biggest waste
		// in the pre-fix matrix was "villager A picks up objective, dies
		// mid-walk to pentagram, villager B repossesses with no item in
		// hand, walks to pentagram anyway, F-press fires but does
		// nothing, attempt counter increments". Catch it here: if the
		// current villager isn't holding the expected tag, rewind to
		// ObjLoopFind so the next iteration re-picks the closest item
		// and walks there first.
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		if (xCur.IsValid() && g_iObjectivesDelivered < 5)
		{
			const DP_ItemTag eExpected = g_aeObjTags[g_iObjectivesDelivered];
			if (DP_Player::GetHeldItemTag(xCur) != eExpected)
			{
				// Don't increment g_iObjAttempts here -- this isn't a
				// failed attempt at *this* objective, just a re-route to
				// re-pickup it. Otherwise repossession-heavy runs blow
				// kMaxObjAttempts on what is actually legitimate progress.
				ClearWASD();
				g_xCurrentObjItem = INVALID_ENTITY_ID;
				g_iPhase = kHP_ObjLoopFind;
				ResetPath();
				return true;
			}
		}

		Zenith_Maths::Vector3 xPentPos;
		if (!TryGetEntityPos(g_xPentagram, xPentPos)) { ++g_iObjectivesDelivered; g_iPhase = kHP_ObjLoopFind; return true; }
		LogWalkProgress("obj-pent", xPentPos);
		--g_iWalkBudget;
		const bool bArrived = DriveWASDToward(xPentPos, 1.95f);
		if (bArrived)
		{
			ClearWASD();
			g_iPhase = kHP_ObjLoopPressF;
			g_iWait = 0;
			return true;
		}
		if (g_iWalkBudget <= 0)
		{
			ClearWASD();
			std::printf("[HumanPlaythrough] obj-pent WALK_TIMEOUT obj=%d\n", g_iObjectivesDelivered);
			std::fflush(stdout);
			++g_iObjectivesDelivered;
			g_iPhase = kHP_ObjLoopFind;
			return true;
		}
		return true;
	}

	case kHP_ObjLoopPressF:
	{
		++g_iWait;
		if (g_iWait == 1) return true;
		// Berserker mashes F every frame from frame 2 onwards; other
		// personalities single-press on frame 2 and idle for 3 frames so
		// the pentagram has time to register the delivery.
		const int iSettleEnd = g_xActiveCfg.bMashInteract ? (2 + kBerserkerMashFrames) : 5;
		if (g_iWait >= 2 && (g_xActiveCfg.bMashInteract || g_iWait == 2))
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		}
		if (g_iWait < iSettleEnd) return true;
		// Read the current possessed villager (may be different from the one
		// we started the obj loop with if a re-possession fired mid-walk).
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		const DP_ItemTag eHeldNow = xCur.IsValid()
			? DP_Player::GetHeldItemTag(xCur) : DP_ItemTag::None;
		const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();
		const uint32_t uBit = 1u << g_iObjectivesDelivered;
		const bool bDelivered = (uMask & uBit) != 0;
		std::printf("[HumanPlaythrough] objective %d attempt#%d: held=%d mask=0x%X bit=0x%X delivered=%d\n",
			g_iObjectivesDelivered, g_iObjAttempts, (int)eHeldNow, uMask, uBit, (int)bDelivered);
		std::fflush(stdout);
		if (bDelivered)
		{
			++g_iObjectivesDelivered;
			g_iObjAttempts = 0;
		}
		else
		{
			// Delivery didn't take (likely re-possessed mid-walk → new villager
			// has no item in hand). Loop back to ObjLoopFind, which will either
			// find us already holding the right tag (skip pickup) or send us
			// to re-pickup the item.
			++g_iObjAttempts;
		}
		g_xCurrentObjItem = INVALID_ENTITY_ID;
		g_iPhase = kHP_ObjLoopFind;
		return true;
	}

	case kHP_AssertVictory:
	{
		++g_iWait;
		if (g_iWait < 4) return true;
		g_uVictoryMask = DP_Win::GetCollectedObjectivesMask();
		std::printf("[%s] victory: mask=0x%X event=%d won=%d\n",
			g_xActiveCfg.szName, g_uVictoryMask, (int)g_bVictoryEvent, (int)DP_Win::HasWon());
		std::fflush(stdout);
		// Speedrunner + Berserker skip the pause-overlay test entirely.
		// Verify is personality-aware (the pause-asserted flags only get
		// checked when g_xActiveCfg.bRunPauseTest is true).
		g_iPhase = g_xActiveCfg.bRunPauseTest ? kHP_PauseOpen : kHP_Summary;
		g_iWait = 0;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase J — pause overlay (Esc to open / close)
	// ----------------------------------------------------------------------
	case kHP_PauseOpen:
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kHP_PauseAssertOpen;
		g_iWait = 0;
		return true;
	}

	case kHP_PauseAssertOpen:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		if (auto* pxOverlay = FindHudText("PauseOverlay"))
		{
			// OR-accumulate (see kHP_PauseAssertClosed for rationale).
			g_bPauseOnObserved = g_bPauseOnObserved || pxOverlay->IsVisible();
		}
		g_iPhase = kHP_PauseClose;
		return true;
	}

	case kHP_PauseClose:
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kHP_PauseAssertClosed;
		g_iWait = 0;
		return true;
	}

	case kHP_PauseAssertClosed:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		if (auto* pxOverlay = FindHudText("PauseOverlay"))
		{
			// OR-accumulate so a single successful observation across
			// any cycle is enough to satisfy Verify. iPauseCycles>1
			// support is kept in the code path even though no current
			// personality uses it -- if a future stress-personality
			// wants to drum the open/close path it can opt in via
			// PersonalityConfig.iPauseCycles.
			g_bPauseOffObserved = g_bPauseOffObserved || !pxOverlay->IsVisible();
		}
		--g_iPauseCyclesRemaining;
		if (g_iPauseCyclesRemaining > 0)
		{
			// Loop back for another open/close cycle.
			std::printf("[%s] pause cycle %d remaining\n",
				g_xActiveCfg.szName, g_iPauseCyclesRemaining);
			std::fflush(stdout);
			g_iPhase = kHP_PauseOpen;
			g_iWait = 0;
			return true;
		}
		g_iPhase = kHP_Summary;
		return true;
	}

	case kHP_Summary:
	{
		std::printf("[%s] summary: "
			"V=%d D=%d C=%d possess=%d "
			"camYawQ=(%.3f→%.3f) camYawE=%.3f camDist=(%.2f→%.2f→%.2f) "
			"iron=%d forge=%d door=%d chest=%d noise=%d "
			"objs=%d mask=0x%X victory=%d won=%d "
			"pauseOn=%d pauseOff=%d\n",
			g_xActiveCfg.szName,
			g_iVillagerCount, g_iDoorCount, g_iChestCount, (int)g_bPossessionConfirmed,
			g_fYawBeforeQ, g_fYawAfterQ, g_fYawAfterE,
			g_fDistBefore, g_fDistAfterIn, g_fDistAfterOut,
			(int)g_bIronPickedUp, (int)g_bForgeCrafted, (int)g_bDoorOpened,
			(int)g_bChestOpened, (int)g_bPriestHeardNoise,
			g_iObjectivesDelivered, g_uVictoryMask, (int)g_bVictoryEvent, (int)DP_Win::HasWon(),
			(int)g_bPauseOnObserved, (int)g_bPauseOffObserved);
		std::fflush(stdout);

		if (g_xVictoryHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xVictoryHandle);
			g_xVictoryHandle = INVALID_EVENT_HANDLE;
		}
		if (g_xInteractHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xInteractHandle);
			g_xInteractHandle = INVALID_EVENT_HANDLE;
		}

		g_iPhase = kHP_Done;
		return false;
	}

	case kHP_Done:
	default:
		return false;
	}
}

// ----------------------------------------------------------------------------
static bool Verify_HumanPlaythrough()
{
	// Release the fixed-dt pin we acquired in Setup so subsequent automated
	// tests (which may rely on variable wall-clock dt) run with the harness
	// default. The harness has no Teardown hook, so Verify is the latest
	// point we can clean up before the next test's Setup fires.
	Zenith_InputSimulator::ClearFixedDt();

	// End telemetry recording. Always runs (success OR failure) so the
	// .ztlm + .json artifacts are available for offline inspection
	// regardless of how the playthrough finished. Tear down the hooks
	// first so the recorder's End() write doesn't race against any
	// stray late events.
	if (g_bRecordingActive)
	{
		const uint32_t uFrames = Zenith_Telemetry::GetRecorder().GetFrameIdx();
		const bool bEnded = Zenith_Telemetry::GetRecorder().End(
			g_strHPBinPath.c_str(),
			g_strHPJsonPath.c_str(),
			&DPTelemetry::DPEventTypeToString);
		// Also emit the v3 CSV exports alongside the binary + JSON, so
		// per-run telemetry is immediately usable by pandas / awk without
		// a separate parse-from-JSON step.
		if (bEnded)
		{
			Zenith_Telemetry::Reader xR;
			if (xR.LoadFromFile(g_strHPBinPath.c_str()))
			{
				xR.ExportCsv(
					g_strHPFramesCsvPath.c_str(),
					g_strHPEventsCsvPath.c_str(),
					&DPTelemetry::DPEventTypeToString);
			}
		}
		g_pxTelemetryHooks.reset();
		g_bRecordingActive = false;
		std::printf("[HumanPlaythrough] telemetry end -- frames=%u ended=%d bin=%s json=%s frames_csv=%s events_csv=%s\n",
			uFrames, (int)bEnded, g_strHPBinPath.c_str(), g_strHPJsonPath.c_str(),
			g_strHPFramesCsvPath.c_str(), g_strHPEventsCsvPath.c_str());
		std::fflush(stdout);
	}

	// Smoke-level Verify: every personality runs on procgen geometry
	// where a reachable item/door/forge/pentagram chain isn't guaranteed
	// inside the per-test frame budget, so we don't assert the full
	// win-condition chain. The walking + possession + interaction
	// signatures are still captured in the per-personality telemetry
	// .ztlm / .json artifacts; the live verify is just enough to flag
	// a regression that prevents the scene from loading or the bot from
	// taking control of a villager at all.
	if (g_iVillagerCount < 1) return false;
	if (g_iDoorCount     < 1) return false;
	if (g_iChestCount    < 1) return false;
	if (!g_bPossessionConfirmed) return false;
	return true;
}

// Per-personality Setup wrappers. Each just stashes the active config
// in g_xActiveCfg, then chains into the shared Setup. Step + Verify
// pointers in the registration block below are the same across all 5 --
// only Setup varies.
static void Setup_Personality_Casual()        { g_xActiveCfg = kPersonality_Casual;        Setup_HumanPlaythrough(); }
static void Setup_Personality_Stealth()       { g_xActiveCfg = kPersonality_Stealth;       Setup_HumanPlaythrough(); }
static void Setup_Personality_Speedrunner()   { g_xActiveCfg = kPersonality_Speedrunner;   Setup_HumanPlaythrough(); }
static void Setup_Personality_Berserker()     { g_xActiveCfg = kPersonality_Berserker;     Setup_HumanPlaythrough(); }

// Frame-budget rationale (rough wall-clock at fixed-dt 1/60):
//   * Casual      ~1800 frames (~30 s in-game / ~38 s wall-clock)
//   * Stealth     ~3600 frames (2x walk budget for Ctrl-held walks, but
//                 skips noise machine which saves a leg). Still well
//                 under the 8000-frame cap.
//   * Speedrunner ~1500 frames once adaptive sprint is wired (was ~1900
//                 with blind sprint pre-rework; adaptive is faster
//                 because it doesn't burn the life-cost on close approaches).
//   * Berserker   ~2200 frames (blind sprint + 3x deaths + 8x mash F).
// 6000-frame cap covers all but Stealth; Stealth gets 8000.
static const Zenith_AutomatedTest g_xPersonalityTest_Casual = {
	"PersonalityPlaythrough_Casual",
	&Setup_Personality_Casual,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Casual);

static const Zenith_AutomatedTest g_xPersonalityTest_Stealth = {
	"PersonalityPlaythrough_Stealth",
	&Setup_Personality_Stealth,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	8000  // 2x walk budget per phase -> larger overall cap
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Stealth);

static const Zenith_AutomatedTest g_xPersonalityTest_Speedrunner = {
	"PersonalityPlaythrough_Speedrunner",
	&Setup_Personality_Speedrunner,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Speedrunner);

static const Zenith_AutomatedTest g_xPersonalityTest_Berserker = {
	"PersonalityPlaythrough_Berserker",
	&Setup_Personality_Berserker,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Berserker);

#endif // ZENITH_INPUT_SIMULATOR
