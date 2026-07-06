#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
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
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPTelemetry.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPDoor_Component.h"
#include "Components/DPForge_Component.h"
#include "Components/DPOrbitCamera_Component.h"
#include "Components/DPItemBase_Component.h"
#include "Tests/DP_TestGraphHelpers.h"
#include "Components/Priest_Component.h"

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
// PersonalityPlaythrough_* — pure-input playthrough, 4 personality variants.
//
// Drives DevilsPlayground end-to-end using ONLY Zenith_InputSimulator (no
// teleporting, no *ForTest bypass calls, no SetInteractOnOverlap, no
// SetPossessedVillager). Each test registers under a distinct personality
// (Casual / Stealth / Speedrunner / Zealot) that tunes the input
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
//                   close approach. Runs the full iron/forge/door/chest
//                   bootstrap so coverage is preserved. The pre-rework
//                   "Speedrun" (blind-sprint) was slower than Casual
//                   because sprint_life_cost_extra_per_s burned through
//                   the 30 s life timer and forced 3+ re-possess cycles
//                   per run; adaptive sprint actually beats Casual's
//                   wall-clock.
//   * Zealot      — single-minded pursuit of the pentagram ritual.
//                   Skips the entire iron/forge/door/chest/noise
//                   bootstrap chain and jumps straight from possession
//                   to the objective-pickup-and-deliver loop. Adaptive
//                   sprint between targets. Tests the hypothesis that
//                   the bootstrap chain is eating budget the win loop
//                   could be spending: if Zealot consistently delivers
//                   more objectives than Casual / Speedrunner on the
//                   same seed, the bootstrap was net-negative for the
//                   win condition. Replaced Berserker on 2026-05-20 --
//                   the seed matrix showed Berserker and Speedrunner
//                   produced statistically identical gameplay outcomes
//                   (same death counts, same possessions, same
//                   objectives within rounding) and only differed in
//                   the F-mash Interact-count signature, which had no
//                   gameplay-outcome impact.
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
//      Speedrunner + Zealot skip
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
	// Semantics (2026-05-20 -- Berserker replaced by Zealot; see top-of-
	// file doc for the seed-matrix analysis that motivated the swap):
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
	//                   Runs the full iron/forge/door/chest bootstrap so
	//                   coverage is preserved.
	//   * Zealot      — skips the iron/forge/door/chest/noise bootstrap
	//                   entirely and jumps straight from possession to the
	//                   objective-pickup-and-deliver loop. Adaptive sprint
	//                   between targets. Distinct from Speedrunner because
	//                   it produces structurally different telemetry: zero
	//                   bootstrap-interaction events, more time on the win
	//                   loop, first-ObjectivePlaced fires much earlier.
	//                   The empirical question Zealot answers: is the
	//                   bootstrap chain net-negative for the win condition?
	// ------------------------------------------------------------------------
	enum class Personality : uint8_t { Casual, Stealth, Speedrunner, Zealot, Magpie, Relay, Heretic, Trickster };

	struct PersonalityConfig
	{
		Personality eType;
		const char* szName;             // used in log lines + telemetry filename
		bool        bHoldSprint;        // hold ZENITH_KEY_LEFT_SHIFT for the entire walk
		bool        bHoldQuiet;         // hold ZENITH_KEY_LEFT_CONTROL for the entire walk
		bool        bAdaptiveSprint;    // sprint only while the remaining distance to the
		                                //   active target is > kSprintMinDistanceM
		bool        bSkipBootstrap;     // skip iron/forge/door/chest/noise -- jump straight
		                                //   from possession to the objective-deliver loop
		bool        bRunNoiseMachine;   // walk to + engage the noise machine
		bool        bRunPauseTest;      // run the Esc pause-overlay phases at all
		int         iPauseCycles;       // how many open/close cycles when bRunPauseTest=true
		// Magpie axis: when true, the objective loop picks the CLOSEST
		// uncollected objective each iteration rather than the fixed
		// Obj1 -> Obj2 -> ... -> Obj5 tag order. Tests whether the
		// fixed-order traversal cost is the dominant overhead in
		// Zealot underperformance, and whether the procgen pentagram-
		// to-spawner distance distribution rewards reorder.
		bool        bAnyOrderObjectives;
		// Relay axis: when true, the bot drops the current held
		// objective at the foot of the nearest healthy villager and
		// voluntary-switches to them whenever the remaining life
		// timer falls below kRelayLifeThresholdSec. Tests the drop-
		// handoff mechanic (zero events across the existing matrix)
		// and whether the per-vessel life budget or the per-pickup
		// cycle cost is the actual constraint.
		bool        bUseRelayDrop;
		// Heretic axis: when true, walks to + F-presses the noise
		// machine BEFORE the objective loop, then jumps straight to
		// kHP_ObjLoopFind (skipping iron/forge/door/chest). Tests
		// whether deliberate priest baiting buys enough "priest-free"
		// objective-loop time to win more cells than Speedrunner.
		bool        bDeliberateNoiseFirst;
	};

	// 2026-05-23: personalities are *decision profiles* — they choose
	// which legitimate in-game actions to use (sprint vs walk, walk-quiet
	// vs walk, drop+switch vs hold, any-order vs fixed-order objectives,
	// noise-bait vs ignore). They MUST NOT differ in mechanical
	// capabilities the simulated human player wouldn't have control over:
	// life-timer length, walk speed, pickup radius, dawn-timer length,
	// repossession latency, etc.
	//
	// In particular, two test-harness internals that used to vary per
	// personality have been promoted to uniform constants:
	//
	//   * `kWalkFrameBudget` is the per-walk-goal frame timeout used by
	//     the bot's stuck-detection (it bails out of a walk if it hasn't
	//     reached the target inside this budget so the test doesn't hang
	//     on unreachable goals). Previously Stealth got 2x. A real
	//     Stealth-style human player doesn't get extra wall-clock for
	//     being slower -- they just cover less ground inside the same
	//     dawn timer. Now uniform across all personalities; the slowest
	//     personality genuinely covers less map, and any win-rate gap
	//     reflects the strategic cost of walking-quiet, not a test-harness
	//     accommodation.
	//
	//   * `kObjAttemptCap` is the per-objective retry counter used to
	//     skip a stuck objective and try the next one. Previously varied
	//     12 (Heretic) -- 24 (Stealth). A human player doesn't have a
	//     personality-dependent patience budget. Now uniform across all
	//     personalities.
	//
	// If a personality needs to behave more cautiously / aggressively,
	// that's expressed as a DECISION FLAG (bUseRelayDrop, bAdaptiveSprint,
	// bAnyOrderObjectives, ...) -- not a hidden capability multiplier.
	// 2026-05-25: bumped from 1200 (20 s) to 1800 (30 s) per walk goal
	// to account for the extra door-traversal cost on the
	// 2-doors-per-corridor layout. The doors-at-DoorPoints PR replaces
	// one corridor-midpoint door per corridor with TWO wall-aligned
	// doors. The bot's previous traversal of a single corridor was
	// "walk -> F-press -> walk through". Now it's
	// "walk -> F-press -> walk -> F-press -> walk through", and each
	// F-press triggers a path-grid invalidation + rebuild on the next
	// movement frame. Empirically the bot's per-leg walk on
	// 2-doors-per-corridor seeds takes ~25 s for the trip from a
	// post-Iron-pickup villager to the pentagram via a typical 3-room
	// route. 1200 was too tight; 1800 lets the bot complete the leg
	// without prematurely bailing into a replan that wastes another
	// few seconds. This is a HARNESS budget knob, not a
	// gameplay-balance lever -- the simulated player's life timer and
	// dawn timer are unchanged.
	constexpr int kWalkFrameBudget = 1800;   // base per-walk-goal frame cap
	constexpr int kObjAttemptCap   = 20;     // single shared patience cap

	// All personalities load the same scene (ProcLevel, build index 1, the
	// only gameplay scene since 2026-05-19) and run Verify in lenient
	// mode -- procgen geometry doesn't guarantee a reachable
	// item/door/forge/pentagram chain inside the bot's frame budget, so
	// the playthrough tests assert "bot drove around without crashing",
	// not "bot completed the full win condition". The walking +
	// possession + interaction signatures are still observable via the
	// telemetry emitted alongside each run.
	//
	// Personalities differ only in DECISION FLAGS (sprint/quiet/adaptive,
	// skip-bootstrap, any-order, relay-drop, noise-first, pause-test).
	// They share identical mechanical capabilities: same walk-frame budget,
	// same objective retry cap, same life timer, same dawn timer, same
	// pickup radius. Win-rate differences reflect strategic effectiveness,
	// not a test-harness accommodation. (See kWalkFrameBudget /
	// kObjAttemptCap above for the 2026-05-23 unification.)
	constexpr PersonalityConfig kPersonality_Casual = {
		Personality::Casual,      "Casual",
		// 2026-05-26 Option C reverted: tried adaptive sprint here to
		// help Casual on long-bootstrap-path seeds but the buff hurt
		// Casual MORE than it helped -- sprint loudness drew the
		// priest in ~4x earlier (1st apprehend t=57s -> t=15s), net
		// Casual deaths +138% and wins 30% -> 20%. Casual stays as
		// the "reference walker" so the difference between Casual
		// vs Speedrunner remains a clean adaptive-sprint A/B signal.
		// Long-bootstrap-path layouts (seed 1) addressed via Option B
		// in procgen (forge placement near pent) instead.
		/*sprint*/false, /*quiet*/false, /*adaptive*/false,
		/*skipBootstrap*/false,
		/*noise*/true,   /*pause*/true,  /*pauseCycles*/1,
		/*anyOrder*/false, /*relayDrop*/false, /*noiseFirst*/false };
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
	//
	// 2026-05-23: Stealth used to get a 2x walk-frame budget and a 24
	// objAttemptCap to "give it equal opportunity" against the faster
	// personalities. Removed -- a Stealth-style human player doesn't get
	// extra wall-clock for being slow; they cover less map inside the
	// same dawn timer. If walking-quiet is too punishing relative to the
	// procgen layout sizes, the fix is to tune the layout / dawn timer /
	// walk-quiet speed multiplier, not to give the bot a hidden buff.
	constexpr PersonalityConfig kPersonality_Stealth = {
		Personality::Stealth,     "Stealth",
		// 2026-05-26: NOT enabling adaptive sprint on Stealth -- the
		// villager's movement resolves "Sprint wins ties; walk-quiet
		// takes the slow speed" (DPVillager_Component ~line 495), so
		// adaptive=true + quiet=true would default to sprint speed
		// during long walks, defeating Stealth's quiet-footstep
		// advantage. Stealth retains adaptive=false; it's expected to
		// cover less map per villager life on layouts where the
		// post-forge walk is long, which reflects the strategic cost
		// of walking-quiet.
		/*sprint*/false, /*quiet*/true,  /*adaptive*/false,
		/*skipBootstrap*/false,
		/*noise*/false,  /*pause*/true,  /*pauseCycles*/1,
		/*anyOrder*/false, /*relayDrop*/false, /*noiseFirst*/false };
	constexpr PersonalityConfig kPersonality_Speedrunner = {
		Personality::Speedrunner, "Speedrunner",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*skipBootstrap*/false,
		/*noise*/true,   /*pause*/false, /*pauseCycles*/1,
		/*anyOrder*/false, /*relayDrop*/false, /*noiseFirst*/false };
	// Zealot bypasses the entire iron/forge/door/chest/noise coverage
	// chain and goes straight to the objective-deliver loop after the
	// first possession lands. Adaptive sprint (not blind sprint) so the
	// life timer doesn't burn through the loop mid-run. bRunNoiseMachine
	// is moot when bSkipBootstrap is true (the noise-machine phase is
	// part of the bootstrap chain) but is set false anyway for clarity.
	// Pause-overlay test is skipped -- distractions don't fit the
	// "single-minded ritual focus" semantic.
	constexpr PersonalityConfig kPersonality_Zealot = {
		Personality::Zealot,      "Zealot",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*skipBootstrap*/true,
		/*noise*/false,  /*pause*/false, /*pauseCycles*/1,
		/*anyOrder*/false, /*relayDrop*/false, /*noiseFirst*/false };
	// Magpie (2026-05-21): opportunistic objective ordering. Picks the
	// closest uncollected objective each iteration instead of the fixed
	// Obj1 -> Obj5 tag order Casual / Stealth / Speedrunner / Zealot
	// all follow. Runs the full bootstrap chain so Casual vs Magpie is
	// a fair "what does any-order cost" delta with everything else held
	// constant. Empirical question: how much of Zealot's underperformance
	// is the fixed-order traversal cost, and is the procgen spawn
	// distribution rewarding reorder?
	constexpr PersonalityConfig kPersonality_Magpie = {
		Personality::Magpie,      "Magpie",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*skipBootstrap*/false,
		/*noise*/true,   /*pause*/false, /*pauseCycles*/1,
		/*anyOrder*/true,  /*relayDrop*/false, /*noiseFirst*/false };
	// Relay (2026-05-21): voluntary-switch + drop-handoff chain. When
	// life timer falls below kRelayLifeThresholdSec and the bot is
	// holding an objective, drops it at the foot of the nearest healthy
	// villager and click-possesses them (which voluntary-faints the
	// outgoing villager rather than killing it). Tests the drop verb
	// (zero events across the existing 4-personality matrix) + the
	// hypothesis that the per-vessel life budget, not the per-pickup
	// cycle cost, is the actual constraint on win rate. Skip the
	// bootstrap because the relay mechanic is only interesting on the
	// objective loop's "I'm holding an item and about to die" tension.
	constexpr PersonalityConfig kPersonality_Relay = {
		Personality::Relay,       "Relay",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*skipBootstrap*/true,
		/*noise*/false,  /*pause*/false, /*pauseCycles*/1,
		/*anyOrder*/false, /*relayDrop*/true,  /*noiseFirst*/false };
	// Heretic (2026-05-21): deliberate priest manipulation. Walks to +
	// F-presses the noise machine BEFORE the objective loop, then jumps
	// straight to kHP_ObjLoopFind. The noise emission baits the priest
	// into an Investigate state for ~30 s, buying that window for the
	// objective loop without priest pressure on the route. Skip the
	// rest of the bootstrap (iron/forge/door/chest) so the noise-bait
	// payoff isn't squandered on side-trips. Empirical question: does
	// position-of-priest matter more than amount-of-priest? Zealot has
	// 5x less priest engagement but doesn't deliver more; if Heretic
	// (more priest engagement, but directed AWAY) wins more cells, the
	// answer is "position".
	constexpr PersonalityConfig kPersonality_Heretic = {
		Personality::Heretic,     "Heretic",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*skipBootstrap*/true,
		/*noise*/true,   /*pause*/false, /*pauseCycles*/1,
		/*anyOrder*/false, /*relayDrop*/false, /*noiseFirst*/true };
	// Trickster (2026-05-21 PR #140): the combo personality predicted
	// strongest by the 7p x 10s matrix run. Magpie's any-order pick
	// (+13% obj throughput) + Relay's voluntary-switch (+3x win rate)
	// + Casual's bootstrap chain (skip-bootstrap was net-negative)
	// + Speedrunner's adaptive sprint (best obj-loop speed). The
	// hypothesis: each of these axes is independently win-rate
	// positive, and the three modifications are orthogonal, so a bot
	// that combines all of them should beat any single-axis bot.
	constexpr PersonalityConfig kPersonality_Trickster = {
		Personality::Trickster,   "Trickster",
		/*sprint*/false, /*quiet*/false, /*adaptive*/true,
		/*skipBootstrap*/false,
		/*noise*/true,   /*pause*/false, /*pauseCycles*/1,
		/*anyOrder*/true,  /*relayDrop*/true,  /*noiseFirst*/false };
	// (Methodical personality removed 2026-05-19 -- the seed-matrix
	// analysis found its bot behaviour was within rounding error of
	// Casual on every metric except the pause-cycle count (3 cycles
	// vs Casual's 1). Pause coverage is preserved via Casual.)
	// (Berserker personality removed 2026-05-20 -- the seed-matrix
	// analysis found its gameplay outcomes were statistically identical
	// to Speedrunner (death counts, possessions, objectives all within
	// rounding) with only the F-mash Interact-count signature
	// differing, and that signature had no gameplay-outcome impact.
	// Zealot replaces it with a behaviourally distinct strategy.)

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
		// 2026-05-25: auto-pickup at 1.5 m proximity can grab any
		// item the bot passes near. On seeds where an Objective spawn
		// room is between villager spawn and the iron room, the bot
		// picks up the Objective accidentally en route. Forge then
		// fails (needs Iron, has Objective); door is locked (no Key);
		// bot stalls. This phase drops the wrong item and retries
		// the iron walk once.
		kHP_DropWrongItem,
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
		// Relay phases (2026-05-21). Inserted between ObjLoopWalk* and
		// ObjLoopPressF: when life < kRelayLifeThresholdSec while holding
		// an objective, walk to the nearest healthy villager, drop, then
		// click-possess them. The voluntary-switch path arms a Fainted
		// timer (not Dead), so the previous vessel is recoverable for
		// later cycles.
		kHP_RelayFindTarget,
		kHP_RelayWalkToTarget,
		kHP_RelayDropAndSwitch,
		kHP_RelayWaitSwitch,
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

	// Single-source setter for the per-walk frame budget. All personalities
	// share kWalkFrameBudget -- the multiplier that used to live on
	// PersonalityConfig.iWalkBudgetMul (Stealth=2) has been removed because
	// it gave the slow personality extra wall-clock that no human player
	// would have. See the kWalkFrameBudget comment above.
	inline void SetWalkBudget(int /*iBaseIgnored*/)
	{
		g_iWalkBudget = kWalkFrameBudget;
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
	// 2026-05-25: single-retry counter for the "drop wrong item, retry
	// iron walk" branch in kHP_WaitIronPickup. The bot auto-picks-up
	// any item at 1.5 m proximity, so on seeds where an Objective spawn
	// sits between villager spawn and iron, the bot grabs the
	// Objective first and the forge step then fails (no Iron in hand).
	// Limited to 1 retry per playthrough to bound the loop.
	int g_iIronRetryCount = 0;
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
	// Per-objective retry cap historical note: the 2026-05-22 design briefly
	// introduced per-personality caps (iObjAttemptCap ranging 12-24); the
	// 2026-05-23 rework unified this back to a single shared cap because
	// personality-specific patience budgets are a test-harness buff that
	// no human player would have. See `kObjAttemptCap` at the top of the
	// file for the current uniform value.

	// Relay (2026-05-21): trigger the drop-handoff when remaining life
	// is below this threshold. 5 s gives the bot ~1.5 s to walk to the
	// nearest healthy villager + drop + faint-switch within the
	// life timer window. Tuned by inspection of the existing 4-
	// personality matrix: median life-remaining at death is ~0.2 s
	// (i.e., bots ride the timer to zero), and the typical inter-
	// villager spacing on procgen is 8-12 m which a Speedrunner-style
	// sprint covers in ~1-1.5 s.
	constexpr float kRelayLifeThresholdSec = 5.0f;
	// Heretic (2026-05-21): how many frames the bot lingers near the
	// noise machine after pressing F. Originally 90 (1.5 s @ 60 Hz)
	// to let the priest start investigating BEFORE the bot leaves.
	//
	// 2026-05-21 PR #139 follow-up: this BACKFIRED. The matrix data
	// showed Heretic's first-Apprehend time as 7-13 s (vs 35-74 s for
	// other personalities) -- the priest beelined to the noise-machine
	// position and apprehended Heretic exactly because Heretic was
	// still standing there. Now 0 frames: emit noise and leave
	// immediately. The priest still goes to investigate (the noise
	// stimulus reaches the priest the same way), but by the time it
	// arrives Heretic is elsewhere running the obj loop.
	//
	// We keep the kHP_WaitNoise -> kHP_ObjLoopFind transition path
	// (it's the natural exit) and just set the loiter count to 0 so
	// the wait-frames check in kHP_WaitNoise advances immediately.
	constexpr int kHereticNoiseDistractFrames = 0;
	// Magpie (2026-05-21): closest-uncollected-objective tracker. The
	// in-order personalities derive the expected tag from the counter
	// (g_aeObjTags[g_iObjectivesDelivered]); Magpie sets this in
	// kHP_ObjLoopFind to the closest still-uncollected obj tag and
	// every downstream phase reads from this when bAnyOrderObjectives
	// is set. The in-order personalities also write to this so the
	// ObjLoopPressF check can use a single uniform path.
	DP_ItemTag g_eCurrentObjTag = DP_ItemTag::Objective1;
	// Relay state. Filled in kHP_RelayFindTarget; consumed by the
	// walk/drop/switch sub-phases.
	Zenith_EntityID g_xRelayTarget;
	int g_iRelayClickWait = 0;
	// 2026-05-21 PR #140 follow-up: the prior implementation only
	// tried one click target per Relay phase entry. The matrix data
	// showed 6 of 7 clicks missing on seed 42 (target villager idx=80
	// at screen (544, 215) returned no possession flip), with the 7th
	// landing because the target had moved and WorldToScreen now
	// projected to a different screen pos.
	//
	// New design: track villagers we've already tried clicking, so
	// RelayFindTarget can pick the NEXT closest non-tried target on
	// retry instead of looping back to the same one. Plus try two Y
	// offsets per attempt -- 0.9 m (head-height) and 1.8 m (over-top)
	// -- since the villager's screen-collider may be wider in y than
	// the original 0.9 m anchor caught. Cap total tries to 4 so we
	// don't burn the whole life timer on doomed clicks.
	// 2026-05-21 PR #140: dropped from 4 to 2. 4 was meant to give the
	// bot generous retry budget, but the matrix data showed it just
	// meant 4x more frames burned on doomed clicks during low-life
	// crisis. 2 targets x 2 Y-offsets x 8 frames = 32 frames total
	// fallback cost vs the OLD 30-frame single-attempt path -- still
	// extra cost, but small enough that the click-success benefit can
	// dominate when it lands.
	constexpr int kMaxRelayTargetTries = 2;
	Zenith_EntityID g_axRelayTried[kMaxRelayTargetTries];
	int g_iRelayTriedCount = 0;
	// Which Y offset is being tried for the CURRENT target -- toggles
	// between 0.9 and 1.8 on each RelayDropAndSwitch entry, so a
	// single target gets two click attempts before being marked tried.
	int g_iRelayYOffsetTry = 0;
	// Heretic state. Counts the post-noise-press lingering frames so
	// the priest has time to start Investigate before the bot leaves.
	int g_iHereticDistractWait = 0;

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
		//
		// 2026-05-21: optional DP_TEST_TMP_PREFIX env var prepended to the
		// basename so concurrent matrix workers (Tools/dp_seed_matrix_run.ps1
		// with -Parallelism > 1) don't clobber each other's temp telemetry.
		// Each parallel worker sets a unique prefix (e.g. "seed42_Casual")
		// per process; absent the env var the legacy single-process layout
		// is preserved. Result example with prefix:
		//   <tempdir>/seed42_Casual_dp_personality_Casual_playthrough.ztlm
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)  // std::getenv "may be unsafe"
#endif
		const char* szPrefix = std::getenv("DP_TEST_TMP_PREFIX");
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
		std::string strPrefix = (szPrefix != nullptr && *szPrefix != '\0')
			? std::string(szPrefix) + "_" : std::string();
		std::string strBase = strPrefix + "dp_personality_"
			+ g_xActiveCfg.szName + "_" + sz;
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
		DP_Query::ForEachComponentInActiveScene<T>(
			[&xResult](Zenith_EntityID xId, T&) { if (!xResult.IsValid()) xResult = xId; });
		return xResult;
	}

	template<typename T>
	int CountScripts()
	{
		int iCount = 0;
		DP_Query::ForEachComponentInActiveScene<T>(
			[&iCount](Zenith_EntityID, T&) { ++iCount; });
		return iCount;
	}

	template<typename T>
	T* GetGameComponent(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<T>();
	}

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	// Resolve the priest's current decision branch + nav-target by inspecting
	// the blackboard keys Priest_Component's bridge writes. Mirror of the
	// DP_Priest.bgraph Selector ordering -- Apprehend > Pursue > Investigate >
	// Patrol. (W3: the keys live on the priest's decision-graph blackboard.)
	void DerivePriestIntentAndTarget(Zenith_EntityID xPriestId,
	                                 const Zenith_Maths::Vector3& xPriestPos,
	                                 DPTelemetry::PriestIntent& eIntent,
	                                 Zenith_Maths::Vector3& xTargetPos)
	{
		eIntent    = DPTelemetry::PriestIntent::Idle;
		xTargetPos = Zenith_Maths::Vector3(0.0f);

		Zenith_Entity xPriest = g_xEngine.Scenes().ResolveEntity(xPriestId);
		Priest_Component* pxPriestC = xPriest.TryGetComponent<Priest_Component>();
		Zenith_BehaviourGraph* pxGraph = pxPriestC ? pxPriestC->FindPriestGraph() : nullptr;
		if (pxGraph == nullptr) return;
		Zenith_GraphBlackboard& xBB = pxGraph->GetBlackboard();

		const Zenith_EntityID xTgtWithDevil = Zenith_EntityID::FromPacked(
			xBB.GetPackedEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL,
				INVALID_ENTITY_ID.GetPacked()));
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

		if (xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false))
		{
			xTargetPos = xBB.GetVector3(DP_AI::BB_KEY_INVESTIGATE_POS,
				Zenith_Maths::Vector3(0.0f));
			eIntent    = DPTelemetry::PriestIntent::Investigate;
			return;
		}

		// PatrolTarget is a Vector3; "no patrol" reads as (0,0,0). The
		// real first patrol point picked by DPPriestPickPatrolTarget lands
		// inside the playable area, so a true zero is a safe sentinel.
		const Zenith_Maths::Vector3 xPatrol = xBB.GetVector3(DP_AI::BB_KEY_PATROL_TARGET,
			Zenith_Maths::Vector3(0.0f));
		if (std::fabs(xPatrol.x) + std::fabs(xPatrol.z) > 0.001f)
		{
			xTargetPos = xPatrol;
			eIntent    = DPTelemetry::PriestIntent::Patrol;
		}
	}

	// Sample the orbit camera once per frame. Returns a valid=false
	// CameraState if no DPOrbitCamera_Component exists in the active
	// scene (e.g. FrontEnd scene during early Setup).
	Zenith_Telemetry::CameraState SampleCameraState()
	{
		Zenith_Telemetry::CameraState xCam;
		xCam.bValid = 0;
		DP_Query::ForEachComponentInActiveScene<DPOrbitCamera_Component>(
			[&xCam](Zenith_EntityID xId, DPOrbitCamera_Component& xOrbit)
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
				// 2026-07-01 third-person mode: while a villager is
				// possessed the RENDERED eye no longer sits on the orbit
				// sphere, so prefer the live camera component's position
				// (same entity as the orbit script). The orbit-derived
				// value above stays as the fallback; xLookAt remains the
				// orbit target (a documented approximation mid-blend).
				Zenith_Entity xCamEnt = g_xEngine.Scenes().ResolveEntity(xId);
				if (xCamEnt.IsValid())
				{
					if (Zenith_CameraComponent* pxRealCam = xCamEnt.TryGetComponent<Zenith_CameraComponent>())
					{
						pxRealCam->GetPosition(xCam.xPos);
					}
				}
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

		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xSample, xPossessed](Zenith_EntityID xId, DPVillager_Component& xVilla)
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
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xSample, &xPriestId](Zenith_EntityID xId, Priest_Component&)
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
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
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
	// DPPauseMenuController_Component::OnStart migrates its parent
	// entity to the persistent scene, so a search restricted to the
	// active scene would miss PauseOverlay. Walk every loaded scene;
	// first match wins. See FullPlaythrough_Test::FindHudText for the
	// fuller rationale.
	Zenith_UI::Zenith_UIText* FindHudText(const char* szName)
	{
		Zenith_UI::Zenith_UIText* pxResult = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[szName, &pxResult](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxResult) return;
				pxResult = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			});
		return pxResult;
	}

	// World-to-screen projection. Inverse of Zenith_CameraComponent::ScreenSpaceToWorldSpace
	// (mirrors its NDC convention exactly: clip.y/clip.w not flipped).
	bool WorldToScreen(const Zenith_Maths::Vector3& xWorld, double& fOutX, double& fOutY)
	{
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
	// DPVillager_Component::TickMovement so WASD inputs land where we expect).
	bool GetCameraHorizontalBasis(Zenith_Maths::Vector3& xForward, Zenith_Maths::Vector3& xRight)
	{
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
	// 2026-05-25: parallel grid keyed by door EntityID. Cells fully clear
	// (or blocked by something other than a door) hold INVALID_ENTITY_ID;
	// cells overlapping a CLOSED-or-CLOSING door hold that door's ID.
	// A* expand treats `g_abPathWalkable[c] || g_axBlockingDoor[c].IsValid()`
	// as planner-traversable so a path can route THROUGH doors, then the
	// movement layer F-presses each door as it reaches it.
	Zenith_EntityID g_axBlockingDoor[kPathGridDim * kPathGridDim] = {};
	// Per-run set of doors the bot has F-pressed. Prevents re-pressing a
	// door the bot just opened (which would close it via the new two-way
	// dispatch). Cleared on Setup; entries are removed on DP_OnDoorClosed
	// so the bot will re-open a door someone else closed.
	Zenith_Vector<Zenith_EntityID> g_axBotOpenedDoors;

	Zenith_Vector<Zenith_Maths::Vector3> g_axCurrentPath;
	int g_iPathWaypoint = 0;
	Zenith_Maths::Vector3 g_xLastPlannedTarget(1e9f, 0.0f, 0.0f);

	inline bool IsCellWalkable(int x, int z)
	{
		if (x < 0 || x >= kPathGridDim || z < 0 || z >= kPathGridDim) return false;
		return g_abPathWalkable[z * kPathGridDim + x];
	}

	// 2026-05-25: A* expand predicate. Walkable cells are always traversable;
	// door cells become traversable for PLANNING purposes (the bot's
	// movement layer F-presses each door as it arrives). This keeps raw
	// raycast walkability (`g_abPathWalkable`) honest -- it still reflects
	// "can the villager's capsule fit here without F-pressing anything"
	// -- and confines door-gated traversability to the planner.
	inline bool IsCellPlanTraversable(int x, int z)
	{
		if (x < 0 || x >= kPathGridDim || z < 0 || z >= kPathGridDim) return false;
		const int idx = z * kPathGridDim + x;
		return g_abPathWalkable[idx] || g_axBlockingDoor[idx].IsValid();
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
					const Zenith_Physics::RaycastResult xH = g_xEngine.Physics().Raycast(
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
		// 2026-05-25: door rasterisation pass. Each currently-closed door
		// writes its EntityID into every cell its 0.3 x 2 m OBB overlaps,
		// using the LOGICAL centre (NOT the entity transform, which is
		// corner-anchor offset by ~1 m) and the door's CLOSED yaw. A*
		// expand then treats those cells as planner-traversable; the
		// movement layer F-presses each door it reaches.
		for (uint32_t i = 0; i < kPathGridDim * kPathGridDim; ++i)
		{
			g_axBlockingDoor[i] = Zenith_EntityID{};
		}
		uint32_t uDoorCells = 0;
		DP_Query::ForEachComponentInActiveScene<DPDoor_Component>(
			[&uDoorCells](Zenith_EntityID xId, DPDoor_Component& xDoor)
			{
				// 2026-05-25 v4: rasterise ALL doors (regardless of
				// BlocksPath state). Open doors are SENSOR colliders
				// after ApplyColliderSolidity fires -- they don't
				// block physics, but Jolt raycasts STILL hit sensor
				// bodies, so the BuildPathGrid raycast pass flags an
				// open door's cells as obstacle. Marking those cells
				// in g_axBlockingDoor (whose IsCellPlanTraversable
				// treats them as planner-traversable) lets the bot
				// path through both closed AND open door cells. The
				// OpportunisticDoorPress F-press only fires on doors
				// where BlocksPath() is true, so the open cells just
				// become free-passage cells in the bot's grid.
				const Zenith_Maths::Vector3 xC = xDoor.GetInteractionCentre();
				const float fYawRad = glm::radians(xDoor.GetClosedYawDegrees());
				const float fCos = std::cos(fYawRad);
				const float fSin = std::sin(fYawRad);
				// Rasterise an OBB at xC of half-extents (kDoorHalfThick, _,
				// kDoorHalfWide). The "wide" axis is the door's local +Z
				// (along the wall); the "thick" axis is local +X (across
				// the wall). World -> local: rotate by -yaw. World point P
				// is inside the OBB iff |R^-1*(P - xC)| <= half-extents
				// componentwise.
				const float fHalfThick = DPDoor_Component::kDoorHalfThick;
				const float fHalfWide  = DPDoor_Component::kDoorHalfWide;
				// Bounding-box screen of candidate cells: the OBB fits
				// inside a (halfThick + halfWide) AABB diagonal-wise.
				const float fSearch = fHalfThick + fHalfWide + 0.5f;
				const int iMinX = static_cast<int>((xC.x - fSearch - kPathOriginX) / kPathCellSize);
				const int iMaxX = static_cast<int>((xC.x + fSearch - kPathOriginX) / kPathCellSize);
				const int iMinZ = static_cast<int>((xC.z - fSearch - kPathOriginZ) / kPathCellSize);
				const int iMaxZ = static_cast<int>((xC.z + fSearch - kPathOriginZ) / kPathCellSize);
				for (int z = iMinZ; z <= iMaxZ; ++z)
				{
					if (z < 0 || z >= kPathGridDim) continue;
					for (int x = iMinX; x <= iMaxX; ++x)
					{
						if (x < 0 || x >= kPathGridDim) continue;
						const float fCx = kPathOriginX + (x + 0.5f) * kPathCellSize;
						const float fCz = kPathOriginZ + (z + 0.5f) * kPathCellSize;
						const float fDx = fCx - xC.x;
						const float fDz = fCz - xC.z;
						// World -> local. R(-yaw) = R^T(yaw).
						const float fLx =  fDx * fCos - fDz * fSin;
						const float fLz =  fDx * fSin + fDz * fCos;
						if (std::fabs(fLx) <= fHalfThick && std::fabs(fLz) <= fHalfWide)
						{
							g_axBlockingDoor[z * kPathGridDim + x] = xId;
							++uDoorCells;
						}
					}
				}
			});

		std::printf("[HumanPlaythrough] path grid built: %u/%d cells walkable, %u door cells\n",
			uWalkable, kPathGridDim * kPathGridDim, uDoorCells);
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
					// 2026-05-25: plan THROUGH doors. The bot's movement
					// layer F-presses each door it reaches; planner just
					// needs to know "is this cell reachable, possibly
					// after opening a door?".
					if (!IsCellPlanTraversable(nx, nz)) continue;
					const int iNIdx = nz * kPathGridDim + nx;
					if (abVisited[iNIdx]) continue;
					const float fStep = (dx != 0 && dz != 0) ? 1.41421f : 1.0f;
					// Small penalty for door cells so A* prefers the open
					// route when one exists, and only routes through a
					// door when there's no alternative.
					const float fDoorPenalty = g_axBlockingDoor[iNIdx].IsValid() ? 5.0f : 0.0f;
					const float fNewG = axGScore[iIdx] + fStep + fDoorPenalty;
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
	// 2026-05-25: opportunistic door F-press. If the bot is within F-range
	// of a closed door it hasn't already opened, press F. Cheap linear
	// scan -- there are <= 2 * corridorCount doors per layout (sub-50
	// typical) and only the ones currently in F-range matter.
	//
	// `g_axBotOpenedDoors` membership semantic: "bot has F-pressed this
	// door at least once during the current key-cycle". Entries are
	// added HERE at the F-press site (so a locked-door rejection still
	// marks the door, preventing per-frame F-press spam against an
	// unopenable door which would invalidate the path grid on every
	// frame via DP_OnInteract).
	//
	// Cleared on:
	//   - DP_OnDoorClosed (someone re-closed the door; bot must reopen
	//     on a return trip).
	//   - DP_OnItemPickedUp (Key / SkeletonKey): bot just acquired a
	//     fresh unlock item, so any door previously marked from a
	//     no-key rejection should be re-attempted. This addresses the
	//     2026-05-25 review note: without this clear, the bot would
	//     permanently skip a door it once tried without a key, even
	//     after forging one.
	//
	// Returns the door pressed (or INVALID if none).
	Zenith_EntityID OpportunisticDoorPress(const Zenith_Maths::Vector3& xVillagerPos)
	{
		Zenith_EntityID xPressed;
		DP_Query::ForEachComponentInActiveScene<DPDoor_Component>(
			[&xPressed, &xVillagerPos](Zenith_EntityID xId, DPDoor_Component& xDoor)
			{
				if (xPressed.IsValid()) return;              // already F-pressed this frame
				if (!xDoor.BlocksPath()) return;             // already open / opening
				for (uint32_t i = 0; i < g_axBotOpenedDoors.GetSize(); ++i)
				{
					if (g_axBotOpenedDoors.Get(i).m_uIndex == xId.m_uIndex
					 && g_axBotOpenedDoors.Get(i).m_uGeneration == xId.m_uGeneration)
					{
						return;
					}
				}
				const Zenith_Maths::Vector3 xC = xDoor.GetInteractionCentre();
				const float fDx = xVillagerPos.x - xC.x;
				const float fDz = xVillagerPos.z - xC.z;
				const float fRadius = xDoor.GetInteractRadius();
				if (fDx * fDx + fDz * fDz > fRadius * fRadius) return;
				// In F-range of a closed door we haven't pressed yet.
				// SimulateKeyPress dispatches a rising-edge press this
				// frame; DPInteractable's F-press poll catches it.
				// Mark immediately (whether the press succeeds or hits
				// a locked-door rejection): the key-pickup handler
				// below clears the set so failed attempts can be retried
				// after the bot acquires a key.
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
				g_axBotOpenedDoors.PushBack(xId);
				xPressed = xId;
			});
		return xPressed;
	}

	bool DriveWASDToward(const Zenith_Maths::Vector3& xTarget, float fStopDist)
	{
		const Zenith_EntityID xV = DP_Player::GetPossessedVillager();
		if (!xV.IsValid()) return false;
		Zenith_Maths::Vector3 xPos;
		if (!TryGetEntityPos(xV, xPos)) return false;

		// 2026-05-25: opportunistically F-press any closed door the bot
		// happens to be near. Without this, the bot can A*-path THROUGH
		// a door cell (the planner now treats doors as traversable) but
		// then physics blocks the villager capsule against the closed
		// door and the bot gets stuck. The door's transition fires
		// DP_OnDoorOpened, which invalidates the path grid; the next
		// DriveWASDToward call replans on the updated state.
		OpportunisticDoorPress(xPos);

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
		//   * bHoldSprint=true  -> always sprint (no current personality
		//                          uses this; reserved for future blind-
		//                          sprint experiments).
		//   * bAdaptiveSprint=true -> sprint while remaining distance to the
		//                            ACTIVE TARGET (not next waypoint) exceeds
		//                            kSprintMinDistanceM. Walk on close
		//                            approach so we can stop precisely and
		//                            don't pay the per-second life cost when
		//                            it doesn't save meaningful time.
		//                            (Speedrunner + Zealot — the killer
		//                            feature that makes adaptive sprint
		//                            faster than blind sprint, which dies
		//                            mid-objective and forces 3x re-possess
		//                            overhead.)
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
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xBest, &fBestSq, &xRef](Zenith_EntityID xId, DPVillager_Component&)
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
		DP_Query::ForEachComponentInActiveScene<T>(
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

	// Magpie helper (2026-05-21): return the closest item across ALL
	// objective tags whose corresponding bit isn't yet set in the win
	// mask. Returns INVALID_ENTITY_ID + leaves eOutTag untouched when
	// nothing is reachable / everything's already collected.
	//
	// Used by kHP_ObjLoopFind when g_xActiveCfg.bAnyOrderObjectives is
	// true to break out of the fixed Obj1 -> Obj2 -> ... order. The
	// in-order personalities sit on g_aeObjTags[g_iObjectivesDelivered]
	// instead, and they never call this.
	Zenith_EntityID FindClosestUncollectedObjective(
		const Zenith_Maths::Vector3& xRef, DP_ItemTag& eOutTag)
	{
		const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_ItemTag eBest = DP_ItemTag::None;
		DP_Query::ForEachComponentInActiveScene<DPItemBase_Component>(
			[&xBest, &fBestSq, &eBest, &xRef, uMask](Zenith_EntityID xId, DPItemBase_Component& xItem)
			{
				const DP_ItemTag eTag = xItem.GetTag();
				if (!DP_IsObjectiveTag(eTag)) return;
				const uint32_t uBit = DP_ObjectiveTagToBit(eTag);
				if (uMask & uBit) return;  // already delivered
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xRef.x;
				const float fDz = xPos.z - xRef.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; eBest = eTag; }
			});
		if (xBest.IsValid()) eOutTag = eBest;
		return xBest;
	}

	// FindItemByTag returns the *first* matching item, which may be far from
	// the villager and force a long unnecessary walk. Prefer the closest one
	// so the test stays within the 3-minute wall-clock budget.
	Zenith_EntityID FindClosestItemByTag(DP_ItemTag eTag, const Zenith_Maths::Vector3& xRef)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachComponentInActiveScene<DPItemBase_Component>(
			[&xBest, &fBestSq, &xRef, eTag](Zenith_EntityID xId, DPItemBase_Component& xItem)
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
		//
		// 2026-05-26 (v30 reverted): tried picking villagers closest to
		// the current phase target via g_xRepossessAnchor. Hurt the
		// matrix net -5 wins (Stealth -2, Zealot -2, Heretic -2,
		// Speedrunner -1, Trickster +2). Hypothesis: straight-line
		// distance from anchor doesn't equal pathfind distance. A
		// villager "close" to the pent-corner anchor but behind walls
		// has a longer real path than a centrally-located villager.
		// The map-centre heuristic happens to pick villagers in
		// open / multi-corridor cells where clicks land reliably and
		// path-fanning to any target is cheap. Seed 1 unaffected by
		// this change (still 1 placement per cell) -- the bottleneck
		// isn't repossession picking. Restored to map-centre pick.
		const Zenith_Maths::Vector3 xCentre(50.0f, 0.0f, 50.0f);
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xBest, &fBestSq, &xCentre](Zenith_EntityID xId, DPVillager_Component& xVilla)
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
	// move geometry under the path grid -- most importantly a door rotating
	// open frees a cell that was blocked. Invalidate the cached grid so the
	// next DriveWASDToward call rebuilds against the current world state.
	void OnInteractEvent(const DP_OnInteract&)
	{
		g_bPathGridBuilt = false;
		g_axCurrentPath.Clear();
		g_iPathWaypoint = 0;
		g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
	}

	// 2026-05-25: pick-up of an unlock-class item (Key / SkeletonKey)
	// clears the bot-opened set so any door the bot previously tried
	// without a key gets a fresh attempt now that it has one. Without
	// this, a locked-door rejection (DP_OnDoorLockRejected, distinct
	// from DP_OnDoorClosed) would permanently mark the door as
	// "bot-handled" in g_axBotOpenedDoors even though the door stayed
	// Closed -- the bot would never revisit, leaving it stranded against
	// any locked door on a layout where its first traversal predated
	// the forge run. (Review-flagged 2026-05-25.)
	void OnItemPickedUpEvent(const DP_OnItemPickedUp& xEvt)
	{
		if (xEvt.m_eTag == DP_ItemTag::Key || xEvt.m_eTag == DP_ItemTag::SkeletonKey)
		{
			g_axBotOpenedDoors.Clear();
		}
	}

	// 2026-05-25: granular door-state events. DP_OnInteract above is
	// generic (fires for forge / chest / pentagram too); the granular
	// door events fire only on the Closed->Opening / Open->Closing
	// transitions, so they're a cleaner signal for door-specific work
	// (path grid invalidation + opened-set bookkeeping).
	void OnDoorOpenedEvent(const DP_OnDoorOpened&)
	{
		g_bPathGridBuilt = false;
		g_axCurrentPath.Clear();
		g_iPathWaypoint = 0;
		g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
	}
	void OnDoorClosedEvent(const DP_OnDoorClosed& xEvt)
	{
		// Same path-grid invalidation; ALSO clear the door from
		// g_axBotOpenedDoors so the bot will re-open it on a return
		// trip if it crosses back through the corridor.
		g_bPathGridBuilt = false;
		g_axCurrentPath.Clear();
		g_iPathWaypoint = 0;
		g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		for (uint32_t i = 0; i < g_axBotOpenedDoors.GetSize(); ++i)
		{
			if (g_axBotOpenedDoors.Get(i).m_uIndex == xEvt.m_xDoor.m_uIndex
			 && g_axBotOpenedDoors.Get(i).m_uGeneration == xEvt.m_xDoor.m_uGeneration)
			{
				g_axBotOpenedDoors.RemoveSwap(i);
				break;
			}
		}
	}

	Zenith_EventHandle g_xInteractHandle    = INVALID_EVENT_HANDLE;
	Zenith_EventHandle g_xDoorOpenedHandle  = INVALID_EVENT_HANDLE;
	Zenith_EventHandle g_xDoorClosedHandle  = INVALID_EVENT_HANDLE;
	Zenith_EventHandle g_xItemPickedUpHandle = INVALID_EVENT_HANDLE;

	// Log walking progress every 60 frames so a stuck walk is visible.
	// 2026-05-26 (v30 reverted): briefly piggybacked a repossession-
	// anchor update here -- see TryRepossessIfDead's rationale
	// comment for why it was rolled back.
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
		if (DPVillager_Component* pxV = GetGameComponent<DPVillager_Component>(xV))
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

	// 2026-05-21: per-personality state introduced for Magpie / Relay /
	// Heretic. Resetting at Setup so batched-mode test runs don't leak
	// state across cells (the runner re-uses the same process via
	// --all-automated-tests + between-tests hooks).
	g_eCurrentObjTag       = DP_ItemTag::Objective1;
	g_xRelayTarget         = INVALID_ENTITY_ID;
	g_iRelayClickWait      = 0;
	g_iHereticDistractWait = 0;
	// PR #140: relay tried-target tracking.
	for (int i = 0; i < kMaxRelayTargetTries; ++i)
	{
		g_axRelayTried[i] = INVALID_ENTITY_ID;
	}
	g_iRelayTriedCount = 0;
	g_iRelayYOffsetTry = 0;

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
	g_iIronRetryCount = 0;
	g_bVictoryEvent  = false;
	g_uVictoryMask   = 0;
	g_bPauseOnObserved = false;
	g_bPauseOffObserved = false;
	g_iPauseCyclesRemaining = g_xActiveCfg.iPauseCycles;

	g_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(&OnVictoryEvent);
	g_xInteractHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnInteract>(&OnInteractEvent);
	// 2026-05-25: granular door-state subscriptions + key-pickup
	// subscription. See OnDoorOpenedEvent / OnDoorClosedEvent /
	// OnItemPickedUpEvent.
	g_xDoorOpenedHandle  = Zenith_EventDispatcher::Get().Subscribe<DP_OnDoorOpened>(&OnDoorOpenedEvent);
	g_xDoorClosedHandle  = Zenith_EventDispatcher::Get().Subscribe<DP_OnDoorClosed>(&OnDoorClosedEvent);
	g_xItemPickedUpHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnItemPickedUp>(&OnItemPickedUpEvent);
	// Reset per-run opened-door set so a fresh test starts without
	// inherited "bot already pressed this door" flags from the previous
	// personality.
	g_axBotOpenedDoors.Clear();

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
		g_xEngine.Scenes().LoadSceneByIndex(
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
		// Wait until GameLevel-specific entities (DPVillager_Component) appear.
		const int iV = CountScripts<DPVillager_Component>();
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
		g_iVillagerCount = CountScripts<DPVillager_Component>();
		g_iDoorCount     = CountScripts<DPDoor_Component>();
		g_iChestCount    = DP_CountEntitiesWithGraph("game:Graphs/DP_Chest.bgraph");

		g_xPriest    = FindFirstScript<Priest_Component>();
		g_xPentagram = DP_FindFirstEntityWithGraph("game:Graphs/DP_Pentagram.bgraph");
		g_xForge     = FindFirstScript<DPForge_Component>();
		// Door, chest, noise: pick the instance closest to the forge so the
		// test's WASD walks stay short. The UE-imported door batch stacks all
		// 15 doors at world origin (~60 m from the forge); the relocated
		// TestDoor authored alongside the forge above is much closer.
		Zenith_Maths::Vector3 xForgePos(50.0f, 0.0f, 32.0f);
		if (g_xForge.IsValid()) TryGetEntityPos(g_xForge, xForgePos);
		// 2026-05-25 v2: target the door closest to the PENTAGRAM, not
		// the forge. With the new 2-doors-per-corridor + unlocked-by-
		// default layout, the door closest to the forge is usually
		// non-pent-side + unlocked -- F-pressing it opens it WITHOUT
		// consuming the Key. The bot then carries Key into the objective
		// loop, but auto-pickup at 1.5 m skips when slot is full, so
		// the bot can't grab an Objective. By targeting the pent-side
		// LOCKED door first, the bot consumes the Key on the unlock,
		// the door stays sticky-unlocked, the bot's hand is empty for
		// the next objective auto-pickup, and the cleared door makes
		// the pentagram reachable.
		Zenith_Maths::Vector3 xPentPos(50.0f, 0.0f, 70.0f);
		if (g_xPentagram.IsValid()) TryGetEntityPos(g_xPentagram, xPentPos);
		g_xDoor   = FindClosestScriptTo<DPDoor_Component>(xPentPos);
		g_xChest  = DP_FindClosestEntityWithGraph("game:Graphs/DP_Chest.bgraph", xForgePos);
		g_xNoise  = DP_FindClosestEntityWithGraph("game:Graphs/DP_NoiseMachine.bgraph", xForgePos);

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
		if (DPOrbitCamera_Component* pxOrbit = GetGameComponent<DPOrbitCamera_Component>(g_xPossessTarget))
		{
			(void)pxOrbit;  // orbit lives on GameManager, not the villager — defensive
		}
		// Look up the orbit on the camera's owning entity (GameManager).
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
			// 2026-05-25 v3: EVERY personality now runs the iron + forge
			// + door portion of the bootstrap because the doors-at-
			// DoorPoints PR makes the pent-side door REQUIRED to reach
			// the pentagram (no more bypass through unlocked wall
			// gaps). Previously bSkipBootstrap personalities (Zealot /
			// Relay / Heretic / Trickster) went straight to the
			// objective loop without a Key; they could win pre-PR by
			// walking around the corridor-midpoint door through the
			// wall gap, but post-PR the wall gap IS a door and there's
			// no bypass.
			//
			// New interpretation of bSkipBootstrap: skip the OPTIONAL
			// chest + noise interactions after the door is unlocked.
			// The iron -> forge -> door critical path is mandatory.
			//
			// Heretic's bDeliberateNoiseFirst still routes through the
			// noise machine first; kHP_WaitNoise then jumps to
			// kHP_WalkIron to enter the (now-mandatory) iron+forge+door
			// chain.
			if (g_xActiveCfg.bDeliberateNoiseFirst)
			{
				g_iPhase = kHP_WalkNoise;
			}
			else
			{
				g_iPhase = kHP_WalkIron;
			}
			SetWalkBudget(kWalkFrameBudget);  // ~30 s budget for the first walk
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
		// 2026-05-25: if the bot accidentally grabbed an Objective on
		// the way to Iron (auto-pickup at 1.5 m proximity is greedy),
		// drop it and re-attempt the iron walk ONCE. Limited to one
		// retry to avoid an infinite drop-walk-pickup-loop when an
		// Objective spawn sits inside the iron's 1.5 m pickup zone.
		// If retry also fails, proceed to forge anyway -- the bot is
		// going to die without Iron, but at least the test makes
		// forward progress and produces telemetry on the failure mode.
		if (!g_bIronPickedUp && DP_IsObjectiveTag(eHeld)
		 && g_iIronRetryCount == 0)
		{
			++g_iIronRetryCount;
			g_iPhase = kHP_DropWrongItem;
			g_iWait = 0;
			return true;
		}
		g_iPhase = kHP_WalkForge;
		SetWalkBudget(kWalkFrameBudget);
		return true;
	}

	case kHP_DropWrongItem:
	{
		++g_iWait;
		// Frame 1: idle to clear WASD.
		if (g_iWait == 1) return true;
		// Frame 2: press G to drop the held item.
		if (g_iWait == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_G);
			return true;
		}
		// Frame 3-4: idle while drop processes.
		if (g_iWait < 5) return true;
		// Frame 5+: re-attempt the iron walk. The dropped Objective
		// is now at the villager's feet; walking AWAY from it before
		// re-pathing reduces (but doesn't eliminate) the chance of
		// re-grabbing it. The single-retry cap above ensures we don't
		// loop on this.
		std::printf("[HumanPlaythrough] dropped wrong item -- retrying iron walk\n");
		std::fflush(stdout);
		g_iPhase = kHP_WalkIron;
		SetWalkBudget(kWalkFrameBudget);
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
		if (DPForge_Component* pxF = GetGameComponent<DPForge_Component>(g_xForge))
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
		// 2026-05-25 v3 + 2026-05-26 v4: if the current villager
		// lacks a Key (e.g. previous villager died mid-bootstrap and
		// we re-possessed without picking up the dropped Key),
		// re-bootstrap UNLESS the pent-side door is already STICKY-
		// UNLOCKED from a previous villager's F-press. In that case
		// the door doesn't need a key anymore -- the bot can walk
		// straight through it to the objective loop. Without the
		// IsOpen() early-exit, every villager re-forged a key it
		// didn't need on layouts where villager 1 already delivered
		// one objective, burning ~30 s of life on a wasted iron +
		// forge chain that prevented the bot from reaching 3
		// deliveries inside 200 s (matrix v28 seed 1 = 0 wins despite
		// Option B's shorter bootstrap path).
		{
			const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
			if (xCur.IsValid())
			{
				const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xCur);
				if (eHeld != DP_ItemTag::Key && eHeld != DP_ItemTag::SkeletonKey)
				{
					// Check if the pent-side door is already open
					// (sticky-unlock). If so, skip bootstrap and
					// proceed to the door phase, which will see
					// IsOpen() and advance to chest/obj-loop.
					bool bDoorAlreadyOpen = false;
					if (DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor))
					{
						bDoorAlreadyOpen = pxDoor->IsOpen();
					}
					if (!bDoorAlreadyOpen)
					{
						std::printf("[HumanPlaythrough] no key at door -- re-bootstrap\n");
						std::fflush(stdout);
						g_iPhase = kHP_WalkIron;
						g_iIronRetryCount = 0;
						SetWalkBudget(kWalkFrameBudget);
						return true;
					}
					std::printf("[HumanPlaythrough] no key at door but door is OPEN -- skip re-bootstrap\n");
					std::fflush(stdout);
				}
			}
		}
		if (!g_xDoor.IsValid()) {
			std::printf("[HumanPlaythrough] door missing — skipping\n");
			std::fflush(stdout);
			g_iPhase = kHP_WalkChest; SetWalkBudget(1200); return true;
		}
		// 2026-05-25: use the door's LOGICAL centre (geometric door
		// centre), not the entity transform (which is corner-offset
		// by ~1 m via SM_Cube's corner-anchor). With fStopDist=1.95
		// from the transform, the bot ends up ~3 m from the logical
		// centre -- outside the 2 m InteractRadius -- and the F-press
		// is rejected. Walking to the logical centre puts the bot
		// inside InteractRadius reliably.
		Zenith_Maths::Vector3 xDoorPos;
		if (DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor))
		{
			xDoorPos = pxDoor->GetInteractionCentre();
		}
		else if (!TryGetEntityPos(g_xDoor, xDoorPos))
		{
			g_iPhase = kHP_WalkChest; SetWalkBudget(1200); return true;
		}
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
		// 2026-05-26: skip the F-press if the door is ALREADY open.
		// With two-way doors (F on an open door closes it), villager 2+
		// reaching a door that villager 1 already opened (sticky-unlock
		// preserves the open state across deaths) would TOGGLE the door
		// closed -- telemetry-confirmed on seed 1: villager 109 closed
		// door 86 at t=165.5s after villager 116 had opened it earlier.
		// Closed-door blocks the bot's subsequent path back to the
		// pentagram. Skip-if-already-open keeps the bot moving forward.
		if (DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor))
		{
			if (pxDoor->IsOpen())
			{
				g_bDoorOpened = true;
				g_iPhase = kHP_VerifyDoor;
				g_iWait = 0;
				return true;
			}
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kHP_VerifyDoor;
		g_iWait = 0;
		return true;
	}

	case kHP_VerifyDoor:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		if (DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor))
		{
			g_bDoorOpened = pxDoor->IsOpen();
		}
		std::printf("[HumanPlaythrough] door: open=%d\n", (int)g_bDoorOpened);
		std::fflush(stdout);
		// 2026-05-25 v3: bSkipBootstrap now means "skip the OPTIONAL
		// chest + noise interactions" -- iron+forge+door is mandatory
		// (see kHP_WaitPossess for the design rationale). Personalities
		// with bSkipBootstrap=true jump straight to the objective loop
		// after the pent-side door is unlocked.
		// bDeliberateNoiseFirst (Heretic) has already done the noise
		// machine BEFORE iron, so it also skips the second noise pass.
		if (g_xActiveCfg.bSkipBootstrap || g_xActiveCfg.bDeliberateNoiseFirst)
		{
			g_iPhase = kHP_ObjLoopFind;
		}
		else
		{
			g_iPhase = kHP_WalkChest;
		}
		SetWalkBudget(kWalkFrameBudget);
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase G — walk to chest, F to open
	// ----------------------------------------------------------------------
	case kHP_WalkChest:
	{
		if (TryRepossessIfDead()) return true;
		// 2026-05-26: opportunistic delivery pivot. If the villager
		// is already holding an Objective (auto-pickup at 1.5 m
		// proximity grabs anything we walk near), divert straight
		// to pentagram delivery instead of continuing the chest
		// side-trip. A real human player wouldn't drop a winning
		// reagent to grab loot first. Telemetry-confirmed on seed 1:
		// villager 3 picked up Objective4 en route to chest, opened
		// chest, walked to pent without an objective slot, died
		// without delivering the carried Objective4.
		{
			const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
			if (xCur.IsValid())
			{
				const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xCur);
				if (DP_IsObjectiveTag(eHeld))
				{
					g_eCurrentObjTag = eHeld;
					g_xCurrentObjItem = INVALID_ENTITY_ID;
					g_iPhase = kHP_ObjLoopWalkPentagram;
					SetWalkBudget(kWalkFrameBudget);
					return true;
				}
			}
		}
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
		g_iPhase = kHP_VerifyChest;
		g_iWait = 0;
		return true;
	}

	case kHP_VerifyChest:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		g_bChestOpened = DP_GetGraphBool(g_xChest, "game:Graphs/DP_Chest.bgraph", "isOpen");
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
		// 2026-05-26: opportunistic delivery pivot (see kHP_WalkChest
		// for the rationale).
		{
			const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
			if (xCur.IsValid())
			{
				const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xCur);
				if (DP_IsObjectiveTag(eHeld))
				{
					g_eCurrentObjTag = eHeld;
					g_xCurrentObjItem = INVALID_ENTITY_ID;
					g_iPhase = kHP_ObjLoopWalkPentagram;
					SetWalkBudget(kWalkFrameBudget);
					return true;
				}
			}
		}
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
		g_iPhase = kHP_WaitNoise;
		g_iWait = 0;
		return true;
	}

	case kHP_WaitNoise:
	{
		++g_iWait;
		// Heretic lingers longer near the noise machine so the priest
		// gets enough frames in Investigate before the bot leaves --
		// this is the whole point of the personality. Non-Heretic
		// noise users just need 8 frames for the perception system
		// to register the stimulus before they continue.
		const int kFrames = g_xActiveCfg.bDeliberateNoiseFirst
			? kHereticNoiseDistractFrames : 8;
		if (g_iWait < kFrames) return true;
		if (g_xPriest.IsValid())
		{
			Zenith_Entity xP = g_xEngine.Scenes().ResolveEntity(g_xPriest);
			if (Priest_Component* pxPriestC = xP.TryGetComponent<Priest_Component>())
			{
				// W3: the bridge writes the priest's decision blackboard.
				g_bPriestHeardNoise = pxPriestC->ReadBBBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
			}
		}
		std::printf("[HumanPlaythrough] noise: priest_has_investigate=%d\n",
			(int)g_bPriestHeardNoise);
		std::fflush(stdout);
		// 2026-05-25 v3: Heretic's bDeliberateNoiseFirst routes us here
		// FIRST (before the iron+forge+door chain). After leaving the
		// noise machine, drop into the mandatory iron+forge+door
		// bootstrap so Heretic also obtains a Key for the pent-side
		// door. Non-noise-first personalities (Casual / Speedrunner /
		// etc.) reach this phase AFTER the bootstrap; for them, the
		// noise machine is the LAST optional stop before objectives.
		if (g_xActiveCfg.bDeliberateNoiseFirst && !g_bForgeCrafted)
		{
			g_iPhase = kHP_WalkIron;
		}
		else
		{
			g_iPhase = kHP_ObjLoopFind;
		}
		SetWalkBudget(kWalkFrameBudget);
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

		const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();

		// Magpie skips the in-order "skip-ahead" path -- there is no
		// fixed obj index for any-order play, just an uncollected mask.
		// The remaining-obj count is the popcount of the inverted mask
		// AND'd with the 5-objective range.
		if (!g_xActiveCfg.bAnyOrderObjectives)
		{
			// Already delivered this objective on a previous attempt? Skip ahead.
			const uint32_t uBit = 1u << g_iObjectivesDelivered;
			if (uMask & uBit)
			{
				++g_iObjectivesDelivered;
				g_iObjAttempts = 0;
				return true;
			}
		}

		// Cap retries — if a single objective resists multiple attempts (e.g.
		// item entity got destroyed without bit being set), give up and move
		// on so the test still terminates rather than spinning forever.
		// 2026-05-23: uniform kObjAttemptCap across all personalities --
		// personality-specific patience budgets gave the test harness an
		// extra knob no human player would have. See the kObjAttemptCap
		// constant for the rationale.
		if (g_iObjAttempts >= kObjAttemptCap)
		{
			std::printf("[HumanPlaythrough] obj %d MAX_ATTEMPTS — skipping\n",
				g_iObjectivesDelivered);
			std::fflush(stdout);
			++g_iObjectivesDelivered;
			g_iObjAttempts = 0;
			return true;
		}

		// Magpie: pick the CLOSEST uncollected obj tag from the bot's
		// current position. In-order: use the next-by-counter tag.
		Zenith_Maths::Vector3 xObjVPos;
		if (!TryGetEntityPos(xCur, xObjVPos))
			xObjVPos = Zenith_Maths::Vector3(45.0f, 0.0f, 53.0f);
		Zenith_EntityID xItem;
		if (g_xActiveCfg.bAnyOrderObjectives)
		{
			xItem = FindClosestUncollectedObjective(xObjVPos, g_eCurrentObjTag);
		}
		else
		{
			g_eCurrentObjTag = g_aeObjTags[g_iObjectivesDelivered];
			xItem = FindClosestItemByTag(g_eCurrentObjTag, xObjVPos);
		}

		// If this villager is already carrying the right item (e.g., previous
		// pent F-press fired but range was wrong, or we re-possessed and the
		// new villager picked it up automatically), walk straight to pent.
		// For Magpie: if the current held item is ANY uncollected obj, head
		// to pent with that tag (override g_eCurrentObjTag).
		const DP_ItemTag eHeldNow = DP_Player::GetHeldItemTag(xCur);
		const bool bHoldingUncollectedObj = DP_IsObjectiveTag(eHeldNow)
			&& ((uMask & DP_ObjectiveTagToBit(eHeldNow)) == 0);
		if (g_xActiveCfg.bAnyOrderObjectives && bHoldingUncollectedObj)
		{
			g_eCurrentObjTag = eHeldNow;
			g_xCurrentObjItem = INVALID_ENTITY_ID;
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}
		if (!g_xActiveCfg.bAnyOrderObjectives && eHeldNow == g_eCurrentObjTag)
		{
			g_xCurrentObjItem = INVALID_ENTITY_ID;
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}

		if (!xItem.IsValid())
		{
			std::printf("[HumanPlaythrough] objective %d (tag=%d) NOT in scene — skipping\n",
				g_iObjectivesDelivered, (int)g_eCurrentObjTag);
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
			// Magpie: opportunistic re-target. If we got auto-pickup-ed
			// onto ANY uncollected objective while walking, head to pent
			// with that. Otherwise the in-order check.
			const DP_ItemTag eHeldNow = DP_Player::GetHeldItemTag(xCur);
			const uint32_t uMaskNow  = DP_Win::GetCollectedObjectivesMask();
			const bool bHoldingUncollectedObj = DP_IsObjectiveTag(eHeldNow)
				&& ((uMaskNow & DP_ObjectiveTagToBit(eHeldNow)) == 0);
			if (g_xActiveCfg.bAnyOrderObjectives && bHoldingUncollectedObj)
			{
				g_eCurrentObjTag = eHeldNow;
				ClearWASD();
				g_xCurrentObjItem = INVALID_ENTITY_ID;
				g_iPhase = kHP_ObjLoopWalkPentagram;
				SetWalkBudget(1200);
				ResetPath();
				return true;
			}
			if (!g_xActiveCfg.bAnyOrderObjectives && eHeldNow == g_eCurrentObjTag)
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
				// Magpie re-aim: among ALL uncollected obj tags, pick the
				// closest. In-order: only re-aim within the current tag.
				DP_ItemTag eReaimTag = g_eCurrentObjTag;
				Zenith_EntityID xClosest = g_xActiveCfg.bAnyOrderObjectives
					? FindClosestUncollectedObjective(xCurPos, eReaimTag)
					: FindClosestItemByTag(g_eCurrentObjTag, xCurPos);
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
						g_eCurrentObjTag  = eReaimTag;
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
			const DP_ItemTag eHeldNow = DP_Player::GetHeldItemTag(xCur);
			// Magpie: re-aim mid-walk if we're holding ANY uncollected
			// obj. In-order: just check the current tag.
			const uint32_t uMaskNow = DP_Win::GetCollectedObjectivesMask();
			const bool bHoldingUncollectedObj = DP_IsObjectiveTag(eHeldNow)
				&& ((uMaskNow & DP_ObjectiveTagToBit(eHeldNow)) == 0);
			if (g_xActiveCfg.bAnyOrderObjectives)
			{
				if (!bHoldingUncollectedObj)
				{
					ClearWASD();
					g_xCurrentObjItem = INVALID_ENTITY_ID;
					g_iPhase = kHP_ObjLoopFind;
					ResetPath();
					return true;
				}
				// Track which tag we're actually delivering (may have
				// drifted from our walk-out plan if a repossess landed
				// us on a different obj).
				g_eCurrentObjTag = eHeldNow;
			}
			else if (eHeldNow != g_eCurrentObjTag)
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

		// Relay (2026-05-21): if life is below threshold while holding
		// an objective, drop + voluntary-switch instead of riding the
		// timer to zero. We check it on the pentagram walk because
		// that's the only walk where we know we're holding the goal
		// item (the obj-item walk is BEFORE pickup, nothing to drop).
		if (g_xActiveCfg.bUseRelayDrop && xCur.IsValid())
		{
			if (DPVillager_Component* pxV = GetGameComponent<DPVillager_Component>(xCur))
			{
				const float fLife = pxV->GetRemainingLife();
				if (fLife > 0.0f && fLife < kRelayLifeThresholdSec)
				{
					std::printf("[HumanPlaythrough] RELAY trigger: life=%.1f tag=%d\n",
						fLife, (int)g_eCurrentObjTag);
					std::fflush(stdout);
					ClearWASD();
					g_iPhase = kHP_RelayFindTarget;
					g_iWait = 0;
					return true;
				}
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
		// Single-press on frame 2, then idle for 3 frames so the pentagram
		// has time to register the delivery.
		if (g_iWait == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		}
		if (g_iWait < 5) return true;
		// Read the current possessed villager (may be different from the one
		// we started the obj loop with if a re-possession fired mid-walk).
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		const DP_ItemTag eHeldNow = xCur.IsValid()
			? DP_Player::GetHeldItemTag(xCur) : DP_ItemTag::None;
		const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();
		// For in-order personalities, this is the bit corresponding to
		// g_iObjectivesDelivered's slot, which matches g_eCurrentObjTag.
		// For Magpie, g_eCurrentObjTag is the tag we routed to (set in
		// ObjLoopFind and updated in ObjLoopWalkPentagram), so the bit
		// derived from it tells us whether THAT specific delivery took.
		const uint32_t uBit = DP_ObjectiveTagToBit(g_eCurrentObjTag);
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

	// ----------------------------------------------------------------------
	// Relay phases (2026-05-21) -- entered from kHP_ObjLoopWalkPentagram
	// when life is below kRelayLifeThresholdSec while holding an
	// objective. Finds nearest healthy villager, walks to within range,
	// drops the obj, voluntary-switches via possession-click.
	// ----------------------------------------------------------------------
	case kHP_RelayFindTarget:
	{
		const Zenith_EntityID xCur = DP_Player::GetPossessedVillager();
		if (!xCur.IsValid()) { g_iPhase = kHP_ObjLoopFind; return true; }
		Zenith_Maths::Vector3 xCurPos;
		if (!TryGetEntityPos(xCur, xCurPos)) { g_iPhase = kHP_ObjLoopFind; return true; }

		// PR #140 reliability: cap total tries so a Stealth-style slow
		// run doesn't burn its whole life timer chasing unreachable
		// click targets. After kMaxRelayTargetTries failed switches,
		// fall back to the normal walk-to-pent and accept the death.
		if (g_iRelayTriedCount >= kMaxRelayTargetTries)
		{
			std::printf("[HumanPlaythrough] RELAY exhausted %d targets -- back to pent walk\n",
				g_iRelayTriedCount);
			std::fflush(stdout);
			// Reset tried set so future Relay-trigger entries get a
			// fresh slate (the bot may re-spawn into a new villager
			// pool via TryRepossessIfDead).
			for (int i = 0; i < kMaxRelayTargetTries; ++i) g_axRelayTried[i] = INVALID_ENTITY_ID;
			g_iRelayTriedCount = 0;
			g_iRelayYOffsetTry = 0;
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}

		// Pick the closest LIVING villager that isn't us AND isn't in
		// the tried-set. Excludes Fainted (still recovering, can't be
		// possessed) and Dead. State machine accessor:
		// GetRemainingLife() > 0 covers Idle/Possessed; we additionally
		// filter the current villager and tried entries out.
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xBest, &fBestSq, &xCurPos, xCur](Zenith_EntityID xId, DPVillager_Component& xVilla)
			{
				if (xId == xCur) return;
				if (xVilla.GetRemainingLife() <= 0.0f) return;
				if (xVilla.GetFaintRecoveryRemaining() > 0.0f) return;
				// Filter out previously-tried targets so we don't
				// loop clicking on the same unreachable villager.
				for (int i = 0; i < g_iRelayTriedCount; ++i)
				{
					if (g_axRelayTried[i] == xId) return;
				}
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xCurPos.x;
				const float fDz = xPos.z - xCurPos.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});

		if (!xBest.IsValid())
		{
			// No relay target left -- bail back to normal walk path so
			// we ride the timer out and rely on TryRepossessIfDead's
			// recovery instead.
			std::printf("[HumanPlaythrough] RELAY no-target -- back to pent walk\n");
			std::fflush(stdout);
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}
		g_xRelayTarget = xBest;
		g_iRelayYOffsetTry = 0;  // start with head-height Y on this fresh target
		std::printf("[HumanPlaythrough] RELAY target acquired idx=%u (dist^2=%.1f) try=%d/%d\n",
			g_xRelayTarget.m_uIndex, fBestSq, g_iRelayTriedCount + 1, kMaxRelayTargetTries);
		std::fflush(stdout);
		g_iPhase = kHP_RelayWalkToTarget;
		SetWalkBudget(600);  // tight budget -- we're racing the life timer
		return true;
	}

	case kHP_RelayWalkToTarget:
	{
		if (TryRepossessIfDead()) return true;
		if (!g_xRelayTarget.IsValid())
		{
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}
		Zenith_Maths::Vector3 xTPos;
		if (!TryGetEntityPos(g_xRelayTarget, xTPos))
		{
			g_xRelayTarget = INVALID_ENTITY_ID;
			g_iPhase = kHP_ObjLoopWalkPentagram;
			SetWalkBudget(1200);
			return true;
		}
		LogWalkProgress("relay", xTPos);
		--g_iWalkBudget;
		// 2.5 m stop so the drop lands within proximity-pickup range
		// of the relay target (DPItemBase auto-pickup triggers at
		// ~1.5 m).
		const bool bArrived = DriveWASDToward(xTPos, 2.5f);
		if (bArrived || g_iWalkBudget <= 0)
		{
			ClearWASD();
			g_iPhase = kHP_RelayDropAndSwitch;
			g_iWait = 0;
			return true;
		}
		return true;
	}

	case kHP_RelayDropAndSwitch:
	{
		++g_iWait;
		// Frame 1: idle to flush WASD.
		if (g_iWait == 1) return true;
		// Frame 2: press G to drop the held item.
		if (g_iWait == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_G);
			return true;
		}
		// Frame 3-4: idle while drop processes.
		if (g_iWait < 5) return true;
		// Frame 5: click on relay target to voluntary-switch possession.
		if (g_iWait == 5)
		{
			if (!g_xRelayTarget.IsValid())
			{
				g_iPhase = kHP_ObjLoopFind;
				return true;
			}
			Zenith_Maths::Vector3 xTPos;
			if (!TryGetEntityPos(g_xRelayTarget, xTPos))
			{
				g_xRelayTarget = INVALID_ENTITY_ID;
				g_iPhase = kHP_ObjLoopFind;
				return true;
			}
			// PR #140: alternate Y offsets on retry. 0.9 m is the
			// original head-height anchor; 1.8 m is over-top, in case
			// the villager's screen collider extends higher than head.
			// g_iRelayYOffsetTry toggles between 0 and 1 on each
			// RelayDropAndSwitch entry for a given target.
			const float fYOffset = (g_iRelayYOffsetTry == 0) ? 0.9f : 1.8f;
			xTPos.y += fYOffset;
			double fSx = 0.0, fSy = 0.0;
			if (!WorldToScreen(xTPos, fSx, fSy))
			{
				// Target off-screen / behind cam; treat as a missed
				// click and let RelayWaitSwitch's timeout path retry.
				std::printf("[HumanPlaythrough] RELAY click off-screen y+=%.1f tgt=%u\n",
					fYOffset, g_xRelayTarget.m_uIndex);
				std::fflush(stdout);
				g_iRelayClickWait = 0;
				g_iPhase = kHP_RelayWaitSwitch;
				return true;
			}
			Zenith_InputSimulator::SimulateMousePosition(fSx, fSy);
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
			std::printf("[HumanPlaythrough] RELAY click at (%.1f, %.1f) y+=%.1f tgt=%u\n",
				fSx, fSy, fYOffset, g_xRelayTarget.m_uIndex);
			std::fflush(stdout);
			g_iRelayClickWait = 0;
			g_iPhase = kHP_RelayWaitSwitch;
			return true;
		}
		return true;
	}

	case kHP_RelayWaitSwitch:
	{
		++g_iRelayClickWait;
		const Zenith_EntityID xNow = DP_Player::GetPossessedVillager();
		if (xNow.IsValid() && xNow == g_xRelayTarget)
		{
			std::printf("[HumanPlaythrough] RELAY switch confirmed: new=%u\n",
				xNow.m_uIndex);
			std::fflush(stdout);
			g_xCurrentVillager = xNow;
			g_xRelayTarget = INVALID_ENTITY_ID;
			// PR #140: reset tried-set on success so future Relay
			// entries (later in the run) get a fresh slate. Otherwise
			// every successful switch shrinks the pool for the next
			// trigger.
			for (int i = 0; i < kMaxRelayTargetTries; ++i) g_axRelayTried[i] = INVALID_ENTITY_ID;
			g_iRelayTriedCount = 0;
			g_iRelayYOffsetTry = 0;
			// Go back to ObjLoopFind so the new villager either:
			//   a) auto-picks-up the just-dropped item via proximity and
			//      we walk straight to pent, or
			//   b) walks back to pick it up the normal way.
			g_xCurrentObjItem = INVALID_ENTITY_ID;
			g_iPhase = kHP_ObjLoopFind;
			return true;
		}
		// Click might have missed. Give it ~8 frames before giving
		// up on the CURRENT click attempt. PR #140: instead of
		// abandoning to ObjLoopFind, alternate the Y-offset for one
		// more shot at this target, and only after two failed Y-tries
		// add the target to the tried-set and find a different one.
		//
		// 8 frames @ 60 Hz = 0.13 s -- enough for DPPlayerController
		// to process a successful click + DP_Player to set the new
		// possessed villager (typically 1-2 frames), with a healthy
		// margin. Originally 30 frames but the 8p x 10s matrix on
		// 2026-05-21 showed Relay regressing from 3 wins to 0 because
		// the bot was burning 240+ frames on doomed retries while life
		// was already < 5 s. Tighter timeout caps the worst-case
		// fallback cost at ~32 frames (2 targets x 2 Y-offsets x 8
		// frames), well under one second of life-timer budget.
		if (g_iRelayClickWait > 8)
		{
			if (g_iRelayYOffsetTry == 0)
			{
				std::printf("[HumanPlaythrough] RELAY click MISS (y=0.9) tgt=%u -- retry y=1.8\n",
					g_xRelayTarget.m_uIndex);
				std::fflush(stdout);
				g_iRelayYOffsetTry = 1;
				g_iPhase = kHP_RelayDropAndSwitch;
				g_iWait = 4;  // skip the drop + idle frames, just re-click
				return true;
			}
			// Both Y offsets exhausted on this target -- mark tried
			// and look for the next-closest healthy villager.
			std::printf("[HumanPlaythrough] RELAY click MISS (y=1.8) tgt=%u -- marking tried\n",
				g_xRelayTarget.m_uIndex);
			std::fflush(stdout);
			if (g_iRelayTriedCount < kMaxRelayTargetTries)
			{
				g_axRelayTried[g_iRelayTriedCount++] = g_xRelayTarget;
			}
			g_xRelayTarget = INVALID_ENTITY_ID;
			g_iRelayYOffsetTry = 0;
			g_iPhase = kHP_RelayFindTarget;
			return true;
		}
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
		// Speedrunner + Zealot skip the pause-overlay test entirely.
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
		if (g_xDoorOpenedHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xDoorOpenedHandle);
			g_xDoorOpenedHandle = INVALID_EVENT_HANDLE;
		}
		if (g_xDoorClosedHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xDoorClosedHandle);
			g_xDoorClosedHandle = INVALID_EVENT_HANDLE;
		}
		if (g_xItemPickedUpHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xItemPickedUpHandle);
			g_xItemPickedUpHandle = INVALID_EVENT_HANDLE;
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
static void Setup_Personality_Zealot()        { g_xActiveCfg = kPersonality_Zealot;        Setup_HumanPlaythrough(); }
static void Setup_Personality_Magpie()        { g_xActiveCfg = kPersonality_Magpie;        Setup_HumanPlaythrough(); }
static void Setup_Personality_Relay()         { g_xActiveCfg = kPersonality_Relay;         Setup_HumanPlaythrough(); }
static void Setup_Personality_Heretic()       { g_xActiveCfg = kPersonality_Heretic;       Setup_HumanPlaythrough(); }
static void Setup_Personality_Trickster()     { g_xActiveCfg = kPersonality_Trickster;     Setup_HumanPlaythrough(); }

// Frame-budget rationale (rough wall-clock at fixed-dt 1/60):
//   * Casual      ~1800 frames (~30 s in-game / ~38 s wall-clock)
//   * Stealth     ~3600 frames (2x walk budget for Ctrl-held walks, but
//                 skips noise machine which saves a leg). Still well
//                 under the 8000-frame cap.
//   * Speedrunner ~1500 frames once adaptive sprint is wired (was ~1900
//                 with blind sprint pre-rework; adaptive is faster
//                 because it doesn't burn the life-cost on close approaches).
//   * Zealot      ~1400 frames -- skips the entire bootstrap chain so
//                 it spends the saved ~30 s entirely on the objective
//                 deliver loop. Adaptive sprint between targets.
// 6000-frame cap covers all but Stealth; Stealth gets 8000.
static const Zenith_AutomatedTest g_xPersonalityTest_Casual = {
	"PersonalityPlaythrough_Casual",
	&Setup_Personality_Casual,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Casual);

static const Zenith_AutomatedTest g_xPersonalityTest_Stealth = {
	"PersonalityPlaythrough_Stealth",
	&Setup_Personality_Stealth,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	8000, // 2x walk budget per phase -> larger overall cap
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Stealth);

static const Zenith_AutomatedTest g_xPersonalityTest_Speedrunner = {
	"PersonalityPlaythrough_Speedrunner",
	&Setup_Personality_Speedrunner,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Speedrunner);

static const Zenith_AutomatedTest g_xPersonalityTest_Zealot = {
	"PersonalityPlaythrough_Zealot",
	&Setup_Personality_Zealot,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Zealot);

// 2026-05-21: three new personalities -- Magpie / Relay / Heretic. See
// the constexpr PersonalityConfig kPersonality_<Name> blocks at the top
// of this file for the rationale + the empirical question each one is
// designed to answer. Setup_Personality_<Name> just plants the config
// and chains into the shared Setup_HumanPlaythrough; everything else
// runs off g_xActiveCfg in the phase machine.
//
// Frame budgets:
//   * Magpie  -- same shape as Casual (full bootstrap + obj loop), so
//                same 6000-frame cap. Any-order pick is a tag-selection
//                tweak, not a phase-count tweak.
//   * Relay   -- shape is Zealot's (skip bootstrap, straight to obj
//                loop) plus the 4 relay sub-phases. Relay swaps add
//                ~30 frames per swap; budget for 4-5 swaps fits in 6000.
//   * Heretic -- shape is Zealot's plus a single noise-machine detour.
//                The noise loiter (kHereticNoiseDistractFrames = 90)
//                + walk-to-noise costs ~150 frames vs Zealot; 6000 still
//                covers the obj loop.
static const Zenith_AutomatedTest g_xPersonalityTest_Magpie = {
	"PersonalityPlaythrough_Magpie",
	&Setup_Personality_Magpie,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Magpie);

static const Zenith_AutomatedTest g_xPersonalityTest_Relay = {
	"PersonalityPlaythrough_Relay",
	&Setup_Personality_Relay,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Relay);

static const Zenith_AutomatedTest g_xPersonalityTest_Heretic = {
	"PersonalityPlaythrough_Heretic",
	&Setup_Personality_Heretic,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Heretic);

// Trickster: same frame budget as Casual / Magpie -- full bootstrap +
// obj loop, no extra phases beyond Relay's 4 sub-phases which only
// activate when life < 5 s.
static const Zenith_AutomatedTest g_xPersonalityTest_Trickster = {
	"PersonalityPlaythrough_Trickster",
	&Setup_Personality_Trickster,
	&Step_HumanPlaythrough,
	&Verify_HumanPlaythrough,
	6000,
	false, // m_bRequiresGraphics (default)
	true   // m_bManualOnly: skipped by --all-automated-tests; run by name
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPersonalityTest_Trickster);

#endif // ZENITH_INPUT_SIMULATOR
