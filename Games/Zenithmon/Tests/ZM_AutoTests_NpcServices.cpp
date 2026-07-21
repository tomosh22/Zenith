#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_NpcServices -- ZM_NpcShop_Test + ZM_NpcHeal_Test (S6 item 3 SC6).
//
// SC5 landed the TALK proof (ZM_AutoTests_NpcTalk.cpp): a player, in the real
// baked Dawnmere, under real simulated input, WALKS UP to an authored NPC and
// talks to it. SC6 is the SAME proof for the other two NPC roles -- the two that
// carry SERVICES rather than words:
//
//   * ZM_NpcShop_Test -- walk to the authored Trade Post clerk, press E, and the
//     SHOP screen comes up carrying THAT CLERK'S OWN STOCK; buy one entry with
//     real key edges and the LIVE money + bag move by EXACTLY the right amounts;
//     escape out.
//   * ZM_NpcHeal_Test -- walk to the authored Caretaker, press E, and the Care
//     Center yes/no prompt comes up; answer YES (the LIVE party is healed) and
//     then, in the SAME session, answer NO (the LIVE party is NOT healed).
//
// WHAT THIS ADDS OVER THE SHIPPED SCREEN GATES. ZM_ShopScreen_Test and
// ZM_CareCenterHeal_Test (S6 item 2 SC7/SC8) already walk both screens end to end
// -- but they raise them by calling ZM_UI_MenuStack::TryOpenShop /
// TryOpenCareCenterPrompt DIRECTLY, from a static seam, with a fixture stock list
// spelled in the test. Neither can tell you whether an NPC carrying that role
// exists in Dawnmere, whether a player can physically reach it, or whether the
// row's own content reaches the screen. These two can, and the stock-identity
// assertion below (the screen's inventory IS the clerk row's, index by index) is
// the part ZM_ShopScreen_Test is structurally incapable of proving.
//
// ---- WHY THIS FILE CARRIES ITS OWN COPY OF THE WALK MACHINERY --------------
// There is no shared test-helper header in Games/Zenithmon/Tests -- the directory
// is .cpp only -- and every helper in ZM_AutoTests_NpcTalk.cpp and
// ZM_AutoTests_UI.cpp lives inside a per-file ANONYMOUS NAMESPACE and is therefore
// unreachable from a new translation unit. So the walk-up machine is copied here
// ONCE and PARAMETERISED (WalkContext below): both tests drive the same copy, and
// SC7's consolidated gate lands in this same file and reuses it a third time.
//
// ---- FIXED DT IS 1/60, AND WHAT THAT COSTS ---------------------------------
// ZM_AutoTests_NpcTalk.cpp runs at 1/60; ZM_AutoTests_UI.cpp's shop and heal tests
// run at 1/30. These tests combine both kinds of phase, and they use 1/60 because
// the WALK is the physics-coupled risky part and must match the proven walk
// EXACTLY. The consequence is applied throughout: every frame budget copied from
// the 1/30 UI tests is DOUBLED --
//     iSHOP_WALK_DEADLINE   200 -> 400
//     iSHOP_CLOSE_DEADLINE  120 -> 240
//     iCC_READ_DEADLINE     120 -> 240
//     iCC_WALK_DEADLINE     160 -> 320
//     ready deadline        420 -> 840   (folded into iBOOT_DEADLINE_FRAMES)
// The "press on frame 2, sample on frame 6" pair and the spaced-edge cadence
// "(frames % 4) == 1" are FRAME-COUNT patterns, not time patterns, so they are
// carried over unchanged.
//
// ---- DESIGN RULES, each one a failure this project has actually shipped ----
//   * EVERY phase flag DEFAULTS TO FAILING, so a phase that never runs fails.
//   * The negatives are load-bearing. An interactor stubbed permanently-true, or
//     a re-raise loop, passes a positives-only version of this test outright.
//   * The press is EVENT-DRIVEN: the test polls EvaluateForTests until the live
//     decision says OK *at the target NPC*, then presses. It never walks N frames
//     and hopes.
//   * The walk has a PROGRESS WATCHDOG, and it reports the LIVE REJECT REASON.
//   * SetPosition appears NOWHERE. Motion is physics-driven and is ASSERTED to be.
//   * SetFocusedElement appears NOWHERE. Every menu traversal is real arrow /
//     Enter / Escape key edges, deadline-guarded.
//   * No reentrant Zenith_MainLoop and no reentrant InputSimulator helpers inside
//     a Step -- that deadlocks on vkWaitForFences.
//
// FRAME ORDERING both tests depend on: Zenith_AutomatedTestRunner::Tick() (which
// calls Step) runs BEFORE g_xEngine.Scenes().Update() in Zenith_MainLoop. A key
// edge injected in Step is therefore consumed by ZM_PlayerController::OnUpdate
// LATER IN THE SAME FRAME, and is observable from the NEXT Step call. Every
// "press here, assert next frame" pair below is that, and nothing more.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"   // GetFacingDir -- the walk's live basis
#include "EntityComponent/Zenith_CameraResolve.h"       // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"   // TryGetGameState (the LIVE purse / bag / party)
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/CareCenter/ZM_CareCenter.h"  // the healed line + the needs-healing predicate
#include "Zenithmon/Source/Data/ZM_ItemData.h"          // ZM_GetItemName (diagnostics) / ZM_ITEM_COUNT
#include "Zenithmon/Source/Data/ZM_NpcData.h"           // the CLERK's own row -- read at RUNTIME, never re-spelled
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"         // ZM_ShopBuyPrice / ZM_SHOP_RESULT
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"
#include "Zenithmon/Source/UI/ZM_UI_Party.h"
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{
	// ------------------------------------------------------------------------
	// Shared constants.
	// ------------------------------------------------------------------------

	// 1/60, matching the PROVEN walk in ZM_AutoTests_NpcTalk.cpp rather than the
	// 1/30 of the UI screen gates -- see the file header for the doubling this
	// forces on every menu budget below.
	constexpr float fNPC_FIXED_DT = 1.0f / 60.0f;

	// Dawnmere's build index (ZM_WorldSpec / ZM-D-012).
	constexpr int iNPC_OVERWORLD_BUILD_INDEX = 2;

	// The SC5 authoring contract, restated here on purpose: a windowed test names
	// the authored entity so that DELETING an NPC from Zenithmon.cpp fails this test
	// loudly rather than quietly removing what it was proving.
	constexpr const char* szCLERK_ENTITY_NAME     = "Npc_TradePostClerk";
	constexpr const char* szCARETAKER_ENTITY_NAME = "Npc_Caretaker";
	// SC7's gate walks all THREE authored Dawnmere NPCs in one session, so it needs
	// the villager's name too (SC5's own test spells it in its own file's anonymous
	// namespace, which is unreachable from here).
	constexpr const char* szVILLAGER_ENTITY_NAME  = "Npc_Villager";
	// SC8's fourth NPC is deliberately covered in this SAME TU so its moving-target
	// approach reuses the camera-relative WalkContext instead of forking a fourth
	// DriveTowardXZ implementation.
	constexpr const char* szWANDERER_ENTITY_NAME   = "Npc_Wanderer";

	// ---- Walk budgets. Each waiting phase owns a deadline that FAILS with its own
	// diagnostic; none of them may be reached by the harness's m_iMaxFrames instead,
	// because that path says nothing about WHERE the test stalled. ----
	// 840 = the UI tests' 420-frame Dawnmere first-load ready window, doubled for
	// 1/60 (the boot phase here waits on the same streaming + grounding).
	constexpr int iBOOT_DEADLINE_FRAMES     = 840;
	constexpr int iBASIS_PROBE_FRAMES       = 30;    // ~0.5 s at 60 Hz -- fails FAST
	constexpr int iAPPROACH_DEADLINE_FRAMES = 1200;  // 22.8 m at 7 m/s is ~200 frames
	constexpr int iARM_DEADLINE_FRAMES      = 240;

	// The approach must keep CLOSING on the target. 60 consecutive frames (1 s)
	// without a real improvement means stuck-on-geometry / wrong-basis / oscillation,
	// and the failure names the distances, the held keys AND the live reject reason.
	constexpr int   iSTALL_LIMIT_FRAMES  = 60;
	constexpr float fSTALL_IMPROVEMENT   = 0.01f;
	// Each basis-probe leg must show meaningful, AXIS-DOMINANT travel.
	constexpr float fBASIS_MIN_TRAVEL    = 0.5f;
	// "Really moved" for the physics-motion evidence, in metres...
	constexpr float fPHYSICS_MIN_TRAVEL  = 1.0f;
	// ...and a real XZ speed rather than a nudge, in m/s.
	constexpr float fPHYSICS_MIN_SPEED   = 1.0f;
	constexpr float fRUN_SPEED_TOLERANCE = 0.001f;

	// ---- Menu budgets, DOUBLED from the 1/30 originals (see the file header) ----
	constexpr int iPRESS_FRAME           = 2;    // frame-count pattern -- NOT doubled
	constexpr int iSETTLE_FRAME          = 6;    // ditto
	constexpr int iSHOP_WALK_DEADLINE    = 400;  // was 200 at 1/30
	constexpr int iSHOP_CLOSE_DEADLINE   = 240;  // was 120 at 1/30
	constexpr int iREAD_DEADLINE         = 240;  // was 120 at 1/30 (iCC_READ_DEADLINE)
	constexpr int iCHOICE_WALK_DEADLINE  = 320;  // was 160 at 1/30 (iCC_WALK_DEADLINE)

	// ---- SC7 gate budgets. RULING I: this file's fixed dt is 1/60, so every budget
	// copied from the 1/30 ZM_AutoTests_UI.cpp gate is DOUBLED, exactly as the four
	// above already are. ----
	constexpr int iGATE_OPEN_DEADLINE      = 240;  // was iGATE_OPEN_DEADLINE 120 at 1/30
	constexpr int iGATE_ROOT_WALK_DEADLINE = 320;  // was iGATE_WALK_DEADLINE 160 at 1/30
	// The final "the session left the player playable" hold. NOT copied from a 1/30
	// test, so nothing to double: 30 frames is 0.5 s at 60 Hz, the same window the
	// basis probes use to establish that a held key produces real motion.
	constexpr int iGATE_UNFREEZE_HOLD_FRAMES = 30;

	// How many spaced interact edges the re-raise negative emits. The villager row carries
	// THREE lines, and a confirming key needs one press to finish each line's typewriter
	// reveal plus one to consume it -- so six presses would read the whole conversation and
	// CLOSE the box. Anything less cannot distinguish a blocked interact key from a
	// confirm absorbed by an in-progress reveal (measured -- see the phase comment).
	constexpr u_int uiGATE_REPRESS_COUNT = 6u;

	// ------------------------------------------------------------------------
	// Shared views + resolvers. NOTHING here is cached across frames: the ECS pool
	// relocates components on swap-and-pop, so a held ZM_Interactable* or
	// ZM_UI_MenuStack* is a dangling read waiting to happen.
	// ------------------------------------------------------------------------

	struct NpcPlayerView
	{
		Zenith_EntityID           m_xEntityID    = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3     m_xPosition    = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController*      m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider   = nullptr;
	};

	float PlanarDistance(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	bool RequiredDawnmereAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			std::error_code xError;
			if (!std::filesystem::is_regular_file(strPath, xError) || xError)
			{
				return false;
			}
			const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
			if (xError || ulSize == 0u)
			{
				return false;
			}
		}
		return true;
	}

	// The UNIQUE active-scene player. Uniqueness is part of the contract: two
	// players would make "which one did the interaction runtime reason from?"
	// undecidable, and EvaluateForTests answers DEGENERATE_ORIGIN in that case.
	bool FindActiveNpcPlayer(NpcPlayerView& xOut)
	{
		xOut = NpcPlayerView{};
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
			});
		return uCount == 1u;
	}

	// Resolve an AUTHORED NPC BY NAME and refresh its world position + live
	// candidacy. Re-resolved EVERY frame by every caller (see the note above).
	bool ResolveNpcByName(
		const char* szEntityName,
		Zenith_EntityID& xEntityIDOut,
		Zenith_Maths::Vector3& xPositionOut,
		bool& bInteractableOut)
	{
		xEntityIDOut = INVALID_ENTITY_ID;
		bInteractableOut = false;
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxData == nullptr || szEntityName == nullptr)
		{
			return false;
		}
		Zenith_Entity xEntity = pxData->FindEntityByName(szEntityName);
		if (!xEntity.IsValid())
		{
			return false;
		}
		ZM_Interactable* pxInteractable = xEntity.TryGetComponent<ZM_Interactable>();
		Zenith_TransformComponent* pxTransform =
			xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxInteractable == nullptr || pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xPositionOut);
		xEntityIDOut = xEntity.GetEntityID();
		bInteractableOut = pxInteractable->IsInteractable();
		return true;
	}

	// The LIVE menu-stack singleton. Null whenever the persistent ZM_MenuRoot is
	// absent -- which Setup has already turned into a skip, so reaching a null here
	// mid-run is a genuine failure, never a skip.
	ZM_UI_MenuStack* ResolveMenuStack()
	{
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			return nullptr;
		}
		Zenith_Entity xMenuRoot = g_xEngine.Scenes().ResolveEntity(xMenuRootID);
		return xMenuRoot.IsValid()
			? xMenuRoot.TryGetComponent<ZM_UI_MenuStack>()
			: nullptr;
	}

	// ...and the same entity's Zenith_UIComponent (the authored menu canvas).
	Zenith_UIComponent* ResolveMenuRootUI()
	{
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			return nullptr;
		}
		Zenith_Entity xMenuRoot = g_xEngine.Scenes().ResolveEntity(xMenuRootID);
		return xMenuRoot.IsValid()
			? xMenuRoot.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}

	// The canvas focus's element name this frame ("" when nothing holds it).
	std::string ReadMenuFocusName()
	{
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return std::string();
		}
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		return (pxFocused != nullptr) ? pxFocused->GetName() : std::string();
	}

	bool MenuFocusCleared()
	{
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			// A FAILURE, not a vacuous pass. This deliberately TIGHTENS the shipped
			// ZM_AutoTests_UI.cpp:275-283 version (which returns true here) rather than
			// copying it: the only caller reaches this on a frame where ResolveMenuStack()
			// has ALREADY resolved the same ZM_MenuRoot, so an unresolved UI component at
			// this point means the menu root vanished -- which must fail the close-side
			// focus assertion, not satisfy it.
			return false;
		}
		return pxUI->GetCanvas().GetFocusedElement() == nullptr;
	}

	// ------------------------------------------------------------------------
	// THE SHARED WALK-UP MACHINE. One copy, parameterised by WalkContext; both
	// tests drive it, and SC7's consolidated gate will drive it a third time.
	//
	// Stages: Boot -> BasisProbeZ -> BasisProbeX -> NegativePress -> NegativeAssert
	//         -> Approach -> ArmAndPress -> Pressed.
	// ------------------------------------------------------------------------

	enum class WalkStage
	{
		Boot,
		BasisProbeZ,      // held W must move the player +Z (leg 1)
		BasisProbeX,      // held D must move the player +X (leg 2)
		NegativePress,    // out of range: press E
		NegativeAssert,   // ...and prove nothing happened, for the RIGHT reason
		Approach,
		ArmAndPress,
		Pressed,          // terminal: the interact edge has been emitted
		Failed,
	};

	// What one TickWalk call decided. Pressed is returned on the SINGLE frame the
	// interact edge was emitted; the owning test asserts the raise on the next one.
	enum class WalkTick : u_int
	{
		Running,
		Pressed,
		Failed,
	};

	struct WalkContext
	{
		// ---- configuration, written once by the owning test's Setup ----
		const char* m_szTargetEntityName = nullptr;
		// Called ONCE, on the frame Boot completes and BEFORE any probe key is held.
		// Returns nullptr on success, or the failure text to fail the walk with.
		// (A captureless function pointer, not std::function -- project rule.)
		const char* (*m_pfnAfterBoot)() = nullptr;
		// Optional final press gate, sampled in ArmAndPress immediately BEFORE the E
		// edge is injected. Stationary-NPC tests leave it null. SC8 uses it to prove
		// the wanderer was genuinely moving (and not naturally dwelling) on the
		// causal frame before opening dialogue.
		bool (*m_pfnCanPress)() = nullptr;

		// ---- live bookkeeping (also the failure diagnostics) ----
		WalkStage             m_eStage = WalkStage::Failed;
		int                   m_iStageFrames = 0;
		Zenith_EntityID       m_xTargetEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xTargetPosition = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xApproachStart = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xBasisStart = Zenith_Maths::Vector3(0.0f);
		u_int                 m_uRaiseCountBefore = 0u;
		float                 m_fSpawnDistance = 0.0f;
		float                 m_fBestDistance = 0.0f;
		float                 m_fCurrentDistance = 0.0f;
		int                   m_iStallFrames = 0;
		bool                  m_abHeldKeys[4] = { false, false, false, false };   // W A S D

		// Basis-probe diagnostics, reported verbatim in the failure block. FOUR
		// measured deltas: leg 1 characterises +Z, leg 2 characterises the X AXIS.
		float m_fBasisZLegDeltaX = 0.0f;
		float m_fBasisZLegDeltaZ = 0.0f;
		float m_fBasisXLegDeltaX = 0.0f;
		float m_fBasisXLegDeltaZ = 0.0f;

		// The LIVE reject reason, refreshed EVERY approach frame. Without it a
		// mis-authored NPC height reads as a mystery timeout instead of as
		// ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND -- see TickWalk's Approach arm.
		const char* m_szLastRejectName = "<not sampled>";
		const char* m_szNegativeReject = "<not sampled>";

		// Baseline for the owning test's FREEZE assertion, captured while the player
		// is demonstrably movable so "frozen afterwards" is a real change.
		bool m_bMovementEnabledBefore = false;

		// ---- Boot diagnostics: exactly WHAT never resolved ----
		bool m_bSawScene      = false;
		bool m_bSawPlayer     = false;
		bool m_bSawPlayerBody = false;
		bool m_bSawNpc        = false;

		// ---- Phase flags. ALL DEFAULT TO FAILING. ----
		bool m_bBootPassed        = false;
		bool m_bBasisZPassed      = false;
		bool m_bBasisXPassed      = false;
		bool m_bNegativePassed    = false;
		bool m_bApproachPassed    = false;
		bool m_bPhysicsMotionSeen = false;

		// ---- Failure-instant snapshot -------------------------------------------
		// FailWalk calls ClearWalkInput, which ZEROES m_abHeldKeys -- so printing that
		// array from the diagnostic reports "no keys held" on EVERY stall, destroying the
		// single most useful piece of evidence about a walk that stopped closing. Snapshot
		// the held set and the live positions BEFORE the clear, and print these instead.
		bool                  m_abHeldKeysAtFailure[4] = { false, false, false, false };
		Zenith_Maths::Vector3 m_xPlayerPosAtFailure = Zenith_Maths::Vector3(0.0f);

		// The player's position, refreshed every approach frame so the snapshot above has
		// something current to capture when the watchdog fires.
		Zenith_Maths::Vector3 m_xLivePlayerPos = Zenith_Maths::Vector3(0.0f);

		// Only FailWalk ever overwrites this, so it is what Verify prints when the
		// harness frame cap ends the run mid-walk. It must therefore describe THAT, not
		// "never ran" -- the genuine never-ran case is already reported by the
		// m_bBootPassed == false entry on the walk-flags line.
		const char* m_szFailure =
			"the walk was still in progress when the run ended (no phase reported a failure)";
	};

	// ★ RULING F. This releases ONLY the movement keys and the run modifier, and it
	// deliberately does NOT release UP / DOWN / LEFT / RIGHT / Enter / Escape -- because
	// there is nothing to release. Every arrow / Enter / Escape edge in this file is
	// emitted exclusively via the ONE-SHOT Zenith_InputSimulator::SimulateKeyPress, which
	// auto-releases at frame end, so no held arrow state ever exists and releasing them
	// would be dead code.
	// Note for anyone tempted to restate the older reasoning: a SetKeyHeld(..., false)
	// CANNOT eat a menu press. SetKeyHeld writes the LEVEL array (s_abKeyState) only,
	// while every menu consumer reads the EDGE -- Zenith_UICanvas navigation and
	// ZM_InputActions::ReadConfirmPressed / ReadCancelPressed all go through
	// WasKeyPressedThisFrame, served from the separate s_abKeyPressedThisFrame array. The
	// "it would eat the press outright" claim is wrong and must not be repeated.
	// Full input clearing happens ONLY in Setup and Verify, via
	// Zenith_InputSimulator::ResetAllInputState().
	void ClearWalkInput(WalkContext& xWalk)
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT_SHIFT, false);
		xWalk.m_abHeldKeys[0] = false;
		xWalk.m_abHeldKeys[1] = false;
		xWalk.m_abHeldKeys[2] = false;
		xWalk.m_abHeldKeys[3] = false;
	}

	void FailWalk(WalkContext& xWalk, const char* szReason)
	{
		xWalk.m_szFailure = szReason;
		xWalk.m_eStage = WalkStage::Failed;
		// Snapshot BEFORE the clear -- ClearWalkInput zeroes m_abHeldKeys, so a diagnostic
		// reading that array after this point always reports "no keys held" regardless of
		// what the walk was actually doing when it gave up.
		xWalk.m_abHeldKeysAtFailure[0] = xWalk.m_abHeldKeys[0];
		xWalk.m_abHeldKeysAtFailure[1] = xWalk.m_abHeldKeys[1];
		xWalk.m_abHeldKeysAtFailure[2] = xWalk.m_abHeldKeys[2];
		xWalk.m_abHeldKeysAtFailure[3] = xWalk.m_abHeldKeys[3];
		xWalk.m_xPlayerPosAtFailure = xWalk.m_xLivePlayerPos;
		ClearWalkInput(xWalk);
	}

	// ★★ THE TRAVERSAL DRIVE IS CAMERA-RELATIVE, and it MUST be. ★★
	//
	// Player movement is camera-relative: ZM_PlayerController::OnUpdate reads the main
	// camera's facing dir and hands it to BuildCameraRelativeDirection, which builds
	// xForward = flatten(cameraForward), xRight = (xForward.z, 0, -xForward.x) and moves
	// along xForward * input.y + xRight * input.x (ZM_PlayerController.cpp:140-147,
	// 244-271). So "W" does NOT mean +Z -- it means "the way the camera is facing".
	//
	// ZM_FollowCamera is a SPRING follow: it places the camera from the fixed authored
	// yaw but then AIMS it at the player with
	// fYaw = atan2(-direction.x, direction.z) over the LAGGING camera-to-player vector
	// (ZM_FollowCamera.cpp:309-323). So the camera's facing swings as the player turns,
	// and the world-space meaning of W/A/S/D rotates with it.
	//
	// The older world-space version of this function (inherited from the shipped
	// traversal test and copied into ZM_AutoTests_NpcTalk.cpp) picked keys by comparing
	// world dx/dz directly. That is correct ONLY while the camera is still near its
	// authored yaw -- i.e. for a SINGLE leg walked from rest, which is all any shipped
	// test did. MEASURED: on this file's third leg, which needs a ~120-degree heading
	// change, the camera had swung far enough that held W drove the player -Z; the walk
	// settled onto a stable 45-degree WRONG heading, receded from 9.25 m to 12.34 m and
	// died on the stall watchdog at (501.3, 486.1) with W+A held. It is not a physics
	// bug, an obstruction or a budget problem -- the keys were simply being chosen in
	// the wrong frame.
	//
	// The fix is the exact INVERSE of the controller's mapping: project the desired
	// world direction onto the live camera basis and choose keys from those components.
	// At the authored yaw of 0 this degenerates to the old behaviour exactly
	// (forward == +Z, right == +X), so the already-green single-leg walks are unchanged.
	void DriveTowardXZ(
		WalkContext& xWalk,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		ClearWalkInput(xWalk);
		constexpr float fDEAD_ZONE = 0.08f;

		// The LIVE camera basis, resolved exactly as the controller resolves it. The
		// fallback matches BuildCameraRelativeDirection's own: +Z forward.
		Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
		{
			pxCamera->GetFacingDir(xCameraForward);
		}
		Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
		const float fForwardLengthSq = xForward.x * xForward.x + xForward.z * xForward.z;
		if (fForwardLengthSq <= 0.000001f)
		{
			xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		}
		else
		{
			xForward /= std::sqrt(fForwardLengthSq);
		}
		const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);

		// The desired WORLD direction, decomposed onto that basis. fForwardAmount is the
		// W/S axis and fRightAmount the D/A axis -- the same two components the
		// controller will multiply back out.
		const Zenith_Maths::Vector3 xToTarget(
			xTarget.x - xPosition.x, 0.0f, xTarget.z - xPosition.z);
		const float fForwardAmount = xToTarget.x * xForward.x + xToTarget.z * xForward.z;
		const float fRightAmount   = xToTarget.x * xRight.x   + xToTarget.z * xRight.z;

		const float fDeltaX = fRightAmount;
		const float fDeltaZ = fForwardAmount;
		if (fDeltaX < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
			xWalk.m_abHeldKeys[1] = true;
		}
		else if (fDeltaX > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
			xWalk.m_abHeldKeys[3] = true;
		}
		if (fDeltaZ < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, true);
			xWalk.m_abHeldKeys[2] = true;
		}
		else if (fDeltaZ > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			xWalk.m_abHeldKeys[0] = true;
		}
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
	}

	// Fold this frame's evidence that the motion is PHYSICS-DRIVEN rather than a
	// teleport: the controller is asking for RUN speed, the BODY has a real planar
	// velocity, and the player has covered real ground since the APPROACH began.
	void AccumulatePhysicsMotionEvidence(WalkContext& xWalk, const NpcPlayerView& xPlayer)
	{
		if (xPlayer.m_pxCollider == nullptr || !xPlayer.m_pxCollider->HasValidBody())
		{
			return;
		}
		const Zenith_Maths::Vector3 xVelocity =
			g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
		const float fPlanarSpeed = std::sqrt(
			xVelocity.x * xVelocity.x + xVelocity.z * xVelocity.z);
		xWalk.m_bPhysicsMotionSeen |=
			PlanarDistance(xPlayer.m_xPosition, xWalk.m_xApproachStart) > fPHYSICS_MIN_TRAVEL
			&& std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
				- ZM_PlayerController::fRUN_SPEED) <= fRUN_SPEED_TOLERANCE
			&& fPlanarSpeed > fPHYSICS_MIN_SPEED;
	}

	// Re-enter the APPROACH leg for a SECOND interact press in the same session (the
	// heal test's NO pass). This is NOT correcting drift -- there is none to correct:
	// the approach releases every movement key when it hands off to ArmAndPress and
	// ZM_PlayerController::BuildHorizontalVelocity SETS horizontal velocity from input
	// each frame, so XZ velocity is already zero before the press, after which the raise
	// freezes the player and OnUpdate early-outs for the whole prompt. What this does is
	// RE-BASELINE the stall watchdog, the deadline and the displacement origin for a
	// second traversal leg. With the target still in range the Approach arm transitions
	// on its very first frame without moving at all, which is exactly the "re-poll
	// first, only re-walk if you have to" shape the NO pass needs.
	void RearmWalkApproach(
		WalkContext& xWalk,
		const Zenith_Maths::Vector3& xPlayerPosition,
		u_int uRaiseCountNow)
	{
		ClearWalkInput(xWalk);
		xWalk.m_uRaiseCountBefore = uRaiseCountNow;
		xWalk.m_fCurrentDistance = PlanarDistance(xPlayerPosition, xWalk.m_xTargetPosition);
		xWalk.m_fBestDistance = xWalk.m_fCurrentDistance;
		xWalk.m_iStallFrames = 0;
		xWalk.m_xApproachStart = xPlayerPosition;
		xWalk.m_eStage = WalkStage::Approach;
		xWalk.m_iStageFrames = 0;
	}

	// ★ RULING H. Point an EXISTING walk context at a DIFFERENT authored NPC and
	// re-enter the APPROACH leg. SC7's consolidated gate walks three NPCs in ONE
	// session, and forking the machine per target is exactly how the three walks
	// would drift apart.
	//
	// This is RearmWalkApproach GENERALISED rather than duplicated: the only thing
	// re-targeting adds over re-arming is swapping the target NAME / ID / POSITION and
	// dropping the stale live-reject diagnostic, after which the distance, stall,
	// deadline and displacement re-baselining is literally the same work -- so it is
	// done by CALLING RearmWalkApproach, whose behaviour for its existing caller
	// (ZM_NpcHeal_Test's NO pass) is untouched. Re-resolving the target BEFORE that
	// call is load-bearing: RearmWalkApproach baselines m_fBestDistance off
	// m_xTargetPosition, so re-arming against the OLD position would seed the stall
	// watchdog with a distance the new walk can never beat and the approach would die
	// on a 60-frame stall rather than walking.
	//
	// It deliberately does NOT touch m_bBootPassed / m_bBasisZPassed / m_bBasisXPassed
	// / m_bNegativePassed / m_bPhysicsMotionSeen: boot, both basis legs and the
	// out-of-range negative are proved ONCE per session, against the first target, and
	// stay proved. (m_bApproachPassed is likewise left set -- it is a "some approach
	// reached its target" flag, and every subsequent leg re-earns it on arrival.)
	//
	// Void by design: a resolve failure routes through FailWalk, so the caller's next
	// TickWalk returns WalkTick::Failed and its ordinary three-way branch reports it
	// with the shared diagnostics.
	void RetargetWalk(WalkContext& xWalk, const char* szNewTargetEntityName)
	{
		ClearWalkInput(xWalk);
		NpcPlayerView xPlayer;
		if (!FindActiveNpcPlayer(xPlayer))
		{
			FailWalk(xWalk, "the player disappeared while re-targeting the walk at the "
				"next authored NPC");
			return;
		}

		xWalk.m_szTargetEntityName = szNewTargetEntityName;
		bool bNpcInteractable = false;
		if (!ResolveNpcByName(xWalk.m_szTargetEntityName,
			xWalk.m_xTargetEntityID, xWalk.m_xTargetPosition, bNpcInteractable)
			|| !bNpcInteractable)
		{
			FailWalk(xWalk, "the NEXT authored NPC did not resolve as an armed "
				"interactable -- it is missing from Dawnmere, or was authored "
				"non-interactable (the target name is part of the SC5 authoring contract)");
			return;
		}
		// The previous target's live reject reason would be a lie about this walk.
		xWalk.m_szLastRejectName = "<not sampled>";

		RearmWalkApproach(xWalk, xPlayer.m_xPosition,
			xPlayer.m_pxController->GetInteractionRuntime().GetRaiseCount());
	}

	WalkTick TickWalk(WalkContext& xWalk)
	{
		if (xWalk.m_eStage == WalkStage::Failed)
		{
			return WalkTick::Failed;
		}
		if (xWalk.m_eStage == WalkStage::Pressed)
		{
			return WalkTick::Pressed;
		}
		++xWalk.m_iStageFrames;

		switch (xWalk.m_eStage)
		{
		// ------------------------------------------------------------------
		// 1. BOOT / SETTLE. Wait for the scene, the unique player, the player's
		//    grounded physics body, and the AUTHORED target's ARMED ZM_Interactable.
		//    All four are tracked SEPARATELY so expiry names exactly which one never
		//    resolved -- "Dawnmere did not become ready" on its own has cost this
		//    project entire debugging sessions.
		// ------------------------------------------------------------------
		case WalkStage::Boot:
		{
			NpcPlayerView xPlayer;
			bool bNpcInteractable = false;
			const bool bScene = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
					== iNPC_OVERWORLD_BUILD_INDEX;
			const bool bPlayer = bScene && FindActiveNpcPlayer(xPlayer);
			const bool bBody = bPlayer
				&& xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded();
			const bool bNpc = bScene && ResolveNpcByName(xWalk.m_szTargetEntityName,
				xWalk.m_xTargetEntityID, xWalk.m_xTargetPosition, bNpcInteractable);
			xWalk.m_bSawScene |= bScene;
			xWalk.m_bSawPlayer |= bPlayer;
			xWalk.m_bSawPlayerBody |= bBody;
			xWalk.m_bSawNpc |= bNpc && bNpcInteractable;

			if (!bScene || !bPlayer || !bBody || !bNpc || !bNpcInteractable)
			{
				if (xWalk.m_iStageFrames > iBOOT_DEADLINE_FRAMES)
				{
					// A MISSING NPC, an UNCONFIGURED NPC and a MUTE NPC all land here --
					// none of them may skip, or the test reports PASS for content that
					// does not exist.
					FailWalk(xWalk, "Dawnmere never reached an interactable state (see the "
						"scene/player/body/npcArmed flags logged below -- a false npcArmed "
						"flag means the SC5 authored NPC is missing, or was authored "
						"non-interactable)");
					return WalkTick::Failed;
				}
				return WalkTick::Running;
			}

			// The walk-up target must genuinely be a walk AWAY, not a spawn-camp. TWICE
			// the global reach, so the two short basis probes below cannot accidentally
			// consume the whole separation and leave the approach vacuous. The SC5
			// authoring places both flank NPCs 22.8 m out against a 2.9 m effective
			// reach, so this has ample headroom -- it exists to catch a LATER
			// re-placement that quietly parks an NPC on the spawn.
			xWalk.m_fSpawnDistance =
				PlanarDistance(xPlayer.m_xPosition, xWalk.m_xTargetPosition);
			xWalk.m_fCurrentDistance = xWalk.m_fSpawnDistance;
			if (xWalk.m_fSpawnDistance <= fZM_INTERACT_MAX_DISTANCE * 2.0f)
			{
				FailWalk(xWalk, "the authored NPC spawns too close to the player -- the "
					"walk-up phase would prove nothing");
				return WalkTick::Failed;
			}

			xWalk.m_bMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();

			if (xWalk.m_pfnAfterBoot != nullptr)
			{
				const char* szHookFailure = xWalk.m_pfnAfterBoot();
				if (szHookFailure != nullptr)
				{
					FailWalk(xWalk, szHookFailure);
					return WalkTick::Failed;
				}
			}

			xWalk.m_bBootPassed = true;
			xWalk.m_xBasisStart = xPlayer.m_xPosition;
			// Plain held W (NO run modifier): this probe characterises the BASIS, and it
			// must match the shipped ZM_DawnmerePlayerCamera_Test evidence exactly.
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			xWalk.m_abHeldKeys[0] = true;
			xWalk.m_eStage = WalkStage::BasisProbeZ;
			xWalk.m_iStageFrames = 0;
			return WalkTick::Running;
		}

		// ------------------------------------------------------------------
		// 2. BASIS PROBE, LEG 1 (+Z). Held W must move the player +Z, and +Z must
		//    dominate. This is the leg ZM_NpcTalk_Test already runs, and it is the
		//    only movement axis with pre-existing characterisation in this repo.
		// ------------------------------------------------------------------
		case WalkStage::BasisProbeZ:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailWalk(xWalk, "the player disappeared during the +Z basis probe");
				return WalkTick::Failed;
			}
			if (xWalk.m_iStageFrames < iBASIS_PROBE_FRAMES)
			{
				return WalkTick::Running;
			}
			xWalk.m_fBasisZLegDeltaX = xPlayer.m_xPosition.x - xWalk.m_xBasisStart.x;
			xWalk.m_fBasisZLegDeltaZ = xPlayer.m_xPosition.z - xWalk.m_xBasisStart.z;
			ClearWalkInput(xWalk);
			if (xWalk.m_fBasisZLegDeltaZ < fBASIS_MIN_TRAVEL
				|| std::fabs(xWalk.m_fBasisZLegDeltaZ) <= std::fabs(xWalk.m_fBasisZLegDeltaX))
			{
				FailWalk(xWalk, "held W did not move the player forward along +Z -- the "
					"movement basis is wrong (all four measured deltas are logged below)");
				return WalkTick::Failed;
			}
			xWalk.m_bBasisZPassed = true;

			// ★ RULING C, LEG 2. BOTH SC6 targets are DIAGONAL from the spawn
			// (dx = +/-14, dz = +18), so the X half of these walks has NO
			// characterisation anywhere in this repo -- ZM_NpcTalk_Test's villager was
			// placed at pure +Z precisely to avoid needing one. This leg characterises
			// THE X AXIS: it proves held D moves the player +X. The caretaker walk uses
			// -X (key A), which is the SAME code path with the opposite input sign
			// (ZM_InputActions::ResolveMove writes xMove.x = -1 for A and +1 for D, and
			// BuildCameraRelativeDirection multiplies ONE right vector by it) -- this leg
			// does NOT execute that sign, and the claim is stated no stronger than that.
			xWalk.m_xBasisStart = xPlayer.m_xPosition;
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
			xWalk.m_abHeldKeys[3] = true;
			xWalk.m_eStage = WalkStage::BasisProbeX;
			xWalk.m_iStageFrames = 0;
			return WalkTick::Running;
		}

		// ------------------------------------------------------------------
		// 3. BASIS PROBE, LEG 2 (+X). See the ruling-C note above.
		// ------------------------------------------------------------------
		case WalkStage::BasisProbeX:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailWalk(xWalk, "the player disappeared during the +X basis probe");
				return WalkTick::Failed;
			}
			if (xWalk.m_iStageFrames < iBASIS_PROBE_FRAMES)
			{
				return WalkTick::Running;
			}
			xWalk.m_fBasisXLegDeltaX = xPlayer.m_xPosition.x - xWalk.m_xBasisStart.x;
			xWalk.m_fBasisXLegDeltaZ = xPlayer.m_xPosition.z - xWalk.m_xBasisStart.z;
			ClearWalkInput(xWalk);
			if (xWalk.m_fBasisXLegDeltaX < fBASIS_MIN_TRAVEL
				|| std::fabs(xWalk.m_fBasisXLegDeltaX) <= std::fabs(xWalk.m_fBasisXLegDeltaZ))
			{
				FailWalk(xWalk, "held D did not move the player along +X -- the strafe half "
					"of the movement basis is wrong, and the DIAGONAL approach below would "
					"stall with no explanation (all four measured deltas are logged below)");
				return WalkTick::Failed;
			}
			xWalk.m_bBasisXPassed = true;
			xWalk.m_eStage = WalkStage::NegativePress;
			xWalk.m_iStageFrames = 0;
			return WalkTick::Running;
		}

		// ------------------------------------------------------------------
		// 4. OUT-OF-RANGE NEGATIVE (before the walk). WITHOUT THIS, an interactor
		//    stubbed permanently-true passes this entire test. The seam must refuse
		//    from here, and an actual E press must raise NOTHING.
		//    The two probe legs together move the player about 2 m +Z and 2 m +X
		//    against a 22.8 m separation, so this is still unambiguously "too far".
		// ------------------------------------------------------------------
		case WalkStage::NegativePress:
		{
			NpcPlayerView xPlayer;
			bool bNpcInteractable = false;
			if (!FindActiveNpcPlayer(xPlayer)
				|| !ResolveNpcByName(xWalk.m_szTargetEntityName,
					xWalk.m_xTargetEntityID, xWalk.m_xTargetPosition, bNpcInteractable)
				|| !bNpcInteractable)
			{
				FailWalk(xWalk, "player or armed NPC was lost before the out-of-range negative");
				return WalkTick::Failed;
			}
			xWalk.m_fCurrentDistance =
				PlanarDistance(xPlayer.m_xPosition, xWalk.m_xTargetPosition);
			// 2 x fZM_INTERACT_MAX_DISTANCE (5.0 m) is beyond the 2.9 m effective reach
			// these three Dawnmere NPCs are actually authored with (2.5 global + 0.4
			// authored), so the refusal below is unambiguously "too far", never a boundary
			// case. It does NOT cover the 10.5 m an author could reach by pushing the
			// per-NPC radius up to ZM_Interactable::fMAX_RADIUS (8.0 m).
			if (xWalk.m_fCurrentDistance <= fZM_INTERACT_MAX_DISTANCE * 2.0f)
			{
				FailWalk(xWalk, "the basis probes walked too close to the NPC -- the "
					"out-of-range negative would be a boundary case, not a negative");
				return WalkTick::Failed;
			}

			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
			if (eSeam == ZM_INTERACT_OK || xSeamTarget != INVALID_ENTITY_ID)
			{
				FailWalk(xWalk, "the interaction seam accepted a target from OUT OF RANGE -- "
					"the picker or the interactor is stubbed / always-true");
				return WalkTick::Failed;
			}
			if (ZM_UI_MenuStack::IsMenuOpen())
			{
				FailWalk(xWalk, "a menu was already open before the first interact press");
				return WalkTick::Failed;
			}

			// The RAW key code, deliberately: a windowed test characterises the binding
			// rather than restating ZM_InputActions' constant back to itself.
			xWalk.m_uRaiseCountBefore = xRuntime.GetRaiseCount();
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
			xWalk.m_eStage = WalkStage::NegativeAssert;
			xWalk.m_iStageFrames = 0;
			return WalkTick::Running;
		}

		case WalkStage::NegativeAssert:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailWalk(xWalk, "the player disappeared during the out-of-range negative");
				return WalkTick::Failed;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			// The edge was consumed by the controller's OnUpdate one frame ago (Step runs
			// before Scenes().Update -- see the file header).
			if (xRuntime.GetRaiseCount() != xWalk.m_uRaiseCountBefore)
			{
				FailWalk(xWalk, "pressing E from out of range RAISED a screen");
				return WalkTick::Failed;
			}
			// ★ TWO DIFFERENT PROOFS, and they are NOT interchangeable.
			// HasLatchedResult() proves only THE RUNTIME TICKED AT ALL: ZM_InteractionRuntime
			// ::Tick sets s_bHasLatchedResult = true UNCONDITIONALLY, outside the
			// `if (bInteractPressed)` branch that writes s_eLastResult / s_xLastTarget (the
			// runtime source says so itself -- "s_bHasLatchedResult still moves every tick, so
			// it remains an honest 'the runtime ran' signal"). It would therefore go false
			// only if ZM_PlayerController::OnUpdate stopped calling Tick at all.
			// The EDGE proof is the GetLastResult() == OUT_OF_RANGE check below: s_eLastResult
			// is written ONLY under bInteractPressed and its reset value is NO_INPUT_EDGE, so
			// that clause -- and only that clause -- fails if the E press never landed. It is
			// also the half that survives a stubbed-true interactor: a real refusal for a real
			// reason, not merely "nothing happened".
			if (!xRuntime.HasLatchedResult())
			{
				FailWalk(xWalk, "the interaction runtime never ticked at all "
					"(ZM_PlayerController::OnUpdate is no longer driving it)");
				return WalkTick::Failed;
			}
			xWalk.m_szNegativeReject = ZM_InteractRejectName(xRuntime.GetLastResult());
			if (xRuntime.GetLastResult() != ZM_INTERACT_REJECT_OUT_OF_RANGE)
			{
				FailWalk(xWalk, "the out-of-range press was refused for the WRONG reason "
					"(the observed reject name is logged below)");
				return WalkTick::Failed;
			}
			if (xRuntime.GetLastTarget() != INVALID_ENTITY_ID)
			{
				FailWalk(xWalk, "an out-of-range refusal still named a target entity");
				return WalkTick::Failed;
			}
			if (ZM_UI_MenuStack::IsMenuOpen())
			{
				FailWalk(xWalk, "pressing E from out of range opened the menu");
				return WalkTick::Failed;
			}
			// No deadline here on purpose: this stage resolves on its FIRST frame either
			// way, so a timeout would be unreachable code pretending to guard.
			xWalk.m_bNegativePassed = true;
			xWalk.m_fBestDistance = xWalk.m_fCurrentDistance;
			xWalk.m_iStallFrames = 0;
			// RE-BASELINE the displacement HERE, at the APPROACH's start. Captured at
			// spawn instead, the two basis probes (60 frames of held keys, ~2.8 m) would
			// already have satisfied fPHYSICS_MIN_TRAVEL before the walk began, so the
			// displacement clause of the physics-motion proof would measure the PROBES
			// rather than the approach it is written to measure.
			xWalk.m_xApproachStart = xPlayer.m_xPosition;
			xWalk.m_eStage = WalkStage::Approach;
			xWalk.m_iStageFrames = 0;
			return WalkTick::Running;
		}

		// ------------------------------------------------------------------
		// 5. APPROACH -- a CLOSED LOOP on the live position, with a progress
		//    watchdog. It ends the instant the LIVE decision says the target is
		//    reachable; it never counts frames and hopes.
		// ------------------------------------------------------------------
		case WalkStage::Approach:
		{
			NpcPlayerView xPlayer;
			bool bNpcInteractable = false;
			if (!FindActiveNpcPlayer(xPlayer)
				|| !ResolveNpcByName(xWalk.m_szTargetEntityName,
					xWalk.m_xTargetEntityID, xWalk.m_xTargetPosition, bNpcInteractable)
				|| !bNpcInteractable)
			{
				FailWalk(xWalk, "player or armed NPC was lost during the approach");
				return WalkTick::Failed;
			}

			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			// Nothing may raise while the player is merely WALKING: no interact edge is
			// injected in this stage, so any movement here is a spurious raise.
			if (xRuntime.GetRaiseCount() != xWalk.m_uRaiseCountBefore)
			{
				FailWalk(xWalk, "a screen was raised during the walk, with no interact press");
				return WalkTick::Failed;
			}

			AccumulatePhysicsMotionEvidence(xWalk, xPlayer);

			// Refreshed every frame purely so FailWalk's snapshot has a current position to
			// capture -- a stall that cannot say WHERE the player stopped is a stall nobody
			// can diagnose without a rebuild.
			xWalk.m_xLivePlayerPos = xPlayer.m_xPosition;
			xWalk.m_fCurrentDistance =
				PlanarDistance(xPlayer.m_xPosition, xWalk.m_xTargetPosition);

			// ★ RULING D. Record the LIVE reject reason EVERY frame. The authored Y of
			// both SC6 targets is an UNVERIFIED ASSUMPTION: Zenithmon.cpp gives all three
			// Dawnmere NPCs the ONE terrain height sampled at the town centre, and says in
			// so many words that the clerk and the caretaker are not yet covered by a
			// walk-up test, so "treat a mute one as a height check first". These two tests
			// are what closes that gap -- so a failure caused by the height assumption
			// must read as ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND in the log, never as a
			// mystery timeout.
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
			xWalk.m_szLastRejectName = ZM_InteractRejectName(eSeam);
			if (eSeam == ZM_INTERACT_OK && xSeamTarget == xWalk.m_xTargetEntityID)
			{
				ClearWalkInput(xWalk);
				xWalk.m_bApproachPassed = true;
				xWalk.m_eStage = WalkStage::ArmAndPress;
				xWalk.m_iStageFrames = 0;
				return WalkTick::Running;
			}

			// The watchdog: the walk must keep CLOSING. Stalling for a second means
			// stuck geometry, a wrong basis or an oscillation, and the failure names the
			// current distance, the best distance, the held keys AND the live reason.
			if (xWalk.m_fCurrentDistance < xWalk.m_fBestDistance - fSTALL_IMPROVEMENT)
			{
				xWalk.m_fBestDistance = xWalk.m_fCurrentDistance;
				xWalk.m_iStallFrames = 0;
			}
			else if (++xWalk.m_iStallFrames > iSTALL_LIMIT_FRAMES)
			{
				FailWalk(xWalk, "the walk-up STALLED -- the player stopped closing on the "
					"NPC (distances, held keys and the LIVE reject reason logged below)");
				return WalkTick::Failed;
			}

			if (xWalk.m_iStageFrames > iAPPROACH_DEADLINE_FRAMES)
			{
				FailWalk(xWalk, "the walk-up never brought the NPC into interaction range "
					"(the LIVE reject reason is logged below -- OUT_OF_VERTICAL_BAND means "
					"the authored NPC height, not the walk, is what failed)");
				return WalkTick::Failed;
			}

			DriveTowardXZ(xWalk, xPlayer.m_xPosition, xWalk.m_xTargetPosition);
			return WalkTick::Running;
		}

		// ------------------------------------------------------------------
		// 6. ARM + PRESS -- EVENT-DRIVEN. Re-poll after the keys were released (the
		//    player is still decelerating) and press only once the live decision
		//    still names the TARGET NPC by entity id.
		// ------------------------------------------------------------------
		case WalkStage::ArmAndPress:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailWalk(xWalk, "the player disappeared while arming the interact press");
				return WalkTick::Failed;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
			xWalk.m_szLastRejectName = ZM_InteractRejectName(eSeam);
			if (eSeam != ZM_INTERACT_OK || xSeamTarget != xWalk.m_xTargetEntityID)
			{
				if (xWalk.m_iStageFrames > iARM_DEADLINE_FRAMES)
				{
					FailWalk(xWalk, "the NPC never armed as the interaction target after the "
						"approach reported it reachable");
					return WalkTick::Failed;
				}
				return WalkTick::Running;
			}
			if (xWalk.m_pfnCanPress != nullptr && !xWalk.m_pfnCanPress())
			{
				if (xWalk.m_iStageFrames > iARM_DEADLINE_FRAMES)
				{
					FailWalk(xWalk, "the target stayed reachable but never satisfied the "
						"test-specific causal press gate before its deadline");
					return WalkTick::Failed;
				}
				return WalkTick::Running;
			}
			if (xRuntime.HasLatchedResult()
				&& xRuntime.GetRaiseCount() != xWalk.m_uRaiseCountBefore)
			{
				FailWalk(xWalk, "a screen was raised before the arming press");
				return WalkTick::Failed;
			}

			xWalk.m_uRaiseCountBefore = xRuntime.GetRaiseCount();
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
			xWalk.m_eStage = WalkStage::Pressed;
			xWalk.m_iStageFrames = 0;
			return WalkTick::Pressed;
		}

		case WalkStage::Pressed:
		case WalkStage::Failed:
			break;
		}
		return WalkTick::Failed;
	}

	// The walk half of a failure block. Both tests call this with their own tag so
	// the four basis deltas, the distances, the held keys and BOTH reject names land
	// in the log verbatim rather than being formatted into a failure string.
	void LogWalkDiagnostics(const WalkContext& xWalk, const char* szTag)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] walk: %s", szTag, xWalk.m_szFailure);
		// FIRST-LEG-ONLY evidence: RetargetWalk deliberately leaves these alone, so on a
		// second or third leg they still describe the FIRST target, not the one that
		// failed. The approach line below names the LIVE target.
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] boot flags (FIRST LEG ONLY): scene=%d player=%d body=%d npcArmed=%d "
			"spawnDistance=%.3f (first leg's target)",
			szTag, (int)xWalk.m_bSawScene, (int)xWalk.m_bSawPlayer,
			(int)xWalk.m_bSawPlayerBody, (int)xWalk.m_bSawNpc, xWalk.m_fSpawnDistance);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] basis probe: legZ dx=%.3f dz=%.3f | legX dx=%.3f dz=%.3f "
			"(each leg's own axis must exceed %.3f and dominate)",
			szTag, xWalk.m_fBasisZLegDeltaX, xWalk.m_fBasisZLegDeltaZ,
			xWalk.m_fBasisXLegDeltaX, xWalk.m_fBasisXLegDeltaZ, fBASIS_MIN_TRAVEL);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] approach: target='%s' at (%.2f, %.2f, %.2f) | player at (%.2f, %.2f, %.2f) "
			"| distance=%.3f best=%.3f stallFrames=%d "
			"held-at-failure W=%d A=%d S=%d D=%d lastLiveReject=%s",
			szTag, xWalk.m_szTargetEntityName,
			xWalk.m_xTargetPosition.x, xWalk.m_xTargetPosition.y, xWalk.m_xTargetPosition.z,
			xWalk.m_xPlayerPosAtFailure.x, xWalk.m_xPlayerPosAtFailure.y,
			xWalk.m_xPlayerPosAtFailure.z,
			xWalk.m_fCurrentDistance, xWalk.m_fBestDistance, xWalk.m_iStallFrames,
			(int)xWalk.m_abHeldKeysAtFailure[0], (int)xWalk.m_abHeldKeysAtFailure[1],
			(int)xWalk.m_abHeldKeysAtFailure[2], (int)xWalk.m_abHeldKeysAtFailure[3],
			xWalk.m_szLastRejectName);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] out-of-range negative: observed reject = %s (expected OUT_OF_RANGE)",
			szTag, xWalk.m_szNegativeReject);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] walk flags: boot=%d basisZ=%d basisX=%d negative=%d approach=%d "
			"physicsMotion=%d movableBeforeWalk=%d",
			szTag, (int)xWalk.m_bBootPassed, (int)xWalk.m_bBasisZPassed,
			(int)xWalk.m_bBasisXPassed, (int)xWalk.m_bNegativePassed,
			(int)xWalk.m_bApproachPassed, (int)xWalk.m_bPhysicsMotionSeen,
			(int)xWalk.m_bMovementEnabledBefore);
	}

	// Every walk-side flag that must be true for EITHER test to pass. The
	// physics-motion flag defaults false and is only ever set from a live body
	// velocity + run speed + real displacement, so a walk faked by any means at all
	// leaves it false. (SetPosition appears NOWHERE in this file -- project rule: no
	// teleportation for movement, even in tests.)
	bool WalkPassed(const WalkContext& xWalk)
	{
		return xWalk.m_bBootPassed
			&& xWalk.m_bBasisZPassed
			&& xWalk.m_bBasisXPassed
			&& xWalk.m_bNegativePassed
			&& xWalk.m_bApproachPassed
			&& xWalk.m_bPhysicsMotionSeen;
	}

	// The LIVE purse + the held count of one item; false when no game state resolves.
	bool ReadLiveMoneyAndHeld(ZM_ITEM_ID eItem, u_int& uMoneyOut, u_int& uHeldOut)
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return false;
		}
		uMoneyOut = pxState->m_uMoney;
		uHeldOut = ((u_int)eItem < (u_int)ZM_ITEM_COUNT)
			? pxState->m_xBag.GetCount(eItem)
			: 0u;
		return true;
	}

	// ========================================================================
	// ZM_NpcShop_Test -- walk up to the AUTHORED Trade Post clerk and trade.
	//
	// Phases after the shared walk: assert the raise, assert the SCREEN'S STOCK IS
	// THE CLERK'S OWN ROW, walk the focus onto Confirm with real Down edges, buy the
	// SELECTED entry with one Enter, and Escape out.
	// ========================================================================

	enum class ShopPhase
	{
		Walk,             // the shared walk-up machine (boot -> basis -> negative -> approach -> press)
		AssertRaise,
		AssertStock,
		WalkToConfirm,    // spaced Down edges: the ENGINE spatial nav walks onto CONFIRM
		BuySelected,      // ONE Enter edge -> the selected entry is bought
		CloseShop,        // spaced Escape edges until the shop closes
		Done,
	};

	WalkContext g_xShopWalk;
	ShopPhase   g_eShopPhase = ShopPhase::Done;
	int         g_iShopPhaseFrames = 0;
	bool        g_bShopSkipped = false;
	bool        g_bShopPrereqsPresent = false;
	const char* g_szShopFailure = "test did not reach verification";

	// ---- Captured observations. Every flag DEFAULTS TO FAILING. ----
	bool  g_bShopRaisePassed     = false;
	bool  g_bShopStockPassed     = false;
	bool  g_bShopBuyPassed       = false;
	bool  g_bShopClosePassed     = false;

	u_int g_uShopRaiseDelta      = 0u;                            // want exactly 1
	u_int g_eShopTopOnOpen       = (u_int)ZM_MENU_SCREEN_NONE;    // want SHOP
	u_int g_uShopDepthOnOpen     = 0u;                            // want 1 (NO pause menu)
	bool  g_bShopPlayerFrozen    = false;
	u_int g_eShopModeOnOpen      = (u_int)ZM_SHOP_MODE_COUNT;     // want BUY
	u_int g_uShopQtyOnOpen       = 0u;                            // want exactly 1 (the open default)

	// ---- The stock-identity proof (Ruling E) ----
	u_int g_uShopRowStockCount   = 0u;    // the CLERK ROW's own count, read at runtime
	u_int g_uShopScreenStockCount = 0u;   // ...and what the screen is listing
	int   g_iShopStockMismatchIndex = -1; // the first index that differed (-1 == none)

	// ---- The Down walk onto CONFIRM (real arrow edges -- defaults to FAILING) ----
	bool        g_bShopReachedConfirm = false;
	std::string g_strShopWalkFocusName;
	int         g_iShopCursorOnLastRow = -99;

	// ---- The purchase, anchored on the entry the cursor is ACTUALLY on ----
	int   g_iShopCursorAtConfirm = -99;
	u_int g_eShopBoughtItem      = (u_int)ZM_ITEM_NONE;
	u_int g_uShopBoughtPrice     = 0u;
	u_int g_uShopBoughtQuantity  = 0u;
	u_int g_uShopMoneyBeforeBuy  = 0u;
	u_int g_uShopMoneyAfterBuy   = 0u;
	u_int g_uShopHeldBeforeBuy   = 0u;
	u_int g_uShopHeldAfterBuy    = 0u;
	u_int g_eShopResultAfterBuy  = (u_int)ZM_SHOP_RESULT_COUNT;   // want ZM_SHOP_OK
	bool  g_bShopReportedResult  = false;

	// ---- The close ----
	bool  g_bShopClosed              = false;
	u_int g_uShopDepthOnClose        = 99u;    // diagnostic only -- see the note in Verify
	bool  g_bShopMovementReenabled   = false;
	bool  g_bShopFocusClearedOnClose = false;

	void FailShop(const char* szReason)
	{
		g_szShopFailure = szReason;
		g_eShopPhase = ShopPhase::Done;
		ClearWalkInput(g_xShopWalk);
	}

	void Setup_NpcShop()
	{
		g_xShopWalk = WalkContext{};
		g_xShopWalk.m_szTargetEntityName = szCLERK_ENTITY_NAME;
		g_xShopWalk.m_pfnAfterBoot = nullptr;

		g_eShopPhase = ShopPhase::Done;
		g_iShopPhaseFrames = 0;
		g_bShopSkipped = false;
		g_bShopPrereqsPresent = false;
		g_szShopFailure = "test did not reach verification";

		g_bShopRaisePassed = false;
		g_bShopStockPassed = false;
		g_bShopBuyPassed = false;
		g_bShopClosePassed = false;

		g_uShopRaiseDelta = 0u;
		g_eShopTopOnOpen = (u_int)ZM_MENU_SCREEN_NONE;
		g_uShopDepthOnOpen = 0u;
		g_bShopPlayerFrozen = false;
		g_eShopModeOnOpen = (u_int)ZM_SHOP_MODE_COUNT;
		g_uShopQtyOnOpen = 0u;

		g_uShopRowStockCount = 0u;
		g_uShopScreenStockCount = 0u;
		g_iShopStockMismatchIndex = -1;

		g_bShopReachedConfirm = false;
		g_strShopWalkFocusName.clear();
		g_iShopCursorOnLastRow = -99;

		g_iShopCursorAtConfirm = -99;
		g_eShopBoughtItem = (u_int)ZM_ITEM_NONE;
		g_uShopBoughtPrice = 0u;
		g_uShopBoughtQuantity = 0u;
		g_uShopMoneyBeforeBuy = 0u;
		g_uShopMoneyAfterBuy = 0u;
		g_uShopHeldBeforeBuy = 0u;
		g_uShopHeldAfterBuy = 0u;
		g_eShopResultAfterBuy = (u_int)ZM_SHOP_RESULT_COUNT;
		g_bShopReportedResult = false;

		g_bShopClosed = false;
		g_uShopDepthOnClose = 99u;
		g_bShopMovementReenabled = false;
		g_bShopFocusClearedOnClose = false;

		Zenith_InputSimulator::ResetAllInputState();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// ---- The ONLY two skips, both "a baked dependency does not exist" ----
		// Deliberately narrow: a test that always skips reports PASS forever and is
		// worse than no test at all. A MISSING clerk, an unconfigured clerk, an
		// unreachable clerk and a clerk with the wrong stock all FAIL below.
		//
		// Guard ORDER IS MANDATORY: RequestSkip bypasses Verify, so no fixed-dt or
		// scene-load state may be installed until both prerequisites are known present.
		g_bShopPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bShopPrereqsPresent)
		{
			g_bShopSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcShop] the Dawnmere scene / terrain bake is absent or incomplete -- "
				"there is no world to walk through (run a *_True config once to bake it)");
			return;
		}
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bShopSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcShop] no persistent ZM_MenuRoot / ZM_UI_MenuStack singleton -- "
				"FrontEnd.zscen has not been baked, so there is no screen for the clerk to raise");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fNPC_FIXED_DT);
		g_xShopWalk.m_eStage = WalkStage::Boot;
		g_eShopPhase = ShopPhase::Walk;
		g_xEngine.Scenes().LoadSceneByIndex(
			iNPC_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_NpcShop(int)
	{
		if (g_eShopPhase == ShopPhase::Done)
		{
			return false;
		}
		++g_iShopPhaseFrames;

		switch (g_eShopPhase)
		{
		// ------------------------------------------------------------------
		// 1-6. The shared walk: boot, both basis legs, the out-of-range negative,
		//      the closed-loop approach, and the event-driven interact press.
		// ------------------------------------------------------------------
		case ShopPhase::Walk:
		{
			const WalkTick eTick = TickWalk(g_xShopWalk);
			if (eTick == WalkTick::Failed)
			{
				FailShop(g_xShopWalk.m_szFailure);
				return false;
			}
			if (eTick == WalkTick::Pressed)
			{
				g_eShopPhase = ShopPhase::AssertRaise;
				g_iShopPhaseFrames = 0;
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 7. THE RAISE. Exactly one screen, at the RIGHT entity, and it is the MART
		//    -- over an otherwise EMPTY stack, with the player frozen.
		// ------------------------------------------------------------------
		case ShopPhase::AssertRaise:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailShop("the player disappeared before the raise could be asserted");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			g_uShopRaiseDelta = xRuntime.GetRaiseCount() - g_xShopWalk.m_uRaiseCountBefore;
			if (xRuntime.GetRaiseCount() != g_xShopWalk.m_uRaiseCountBefore + 1u)
			{
				FailShop("the walk-up press did not raise EXACTLY one screen");
				return false;
			}
			if (xRuntime.GetLastResult() != ZM_INTERACT_OK)
			{
				FailShop("the walk-up press did not latch ZM_INTERACT_OK");
				return false;
			}
			if (xRuntime.GetLastTarget() != g_xShopWalk.m_xTargetEntityID)
			{
				FailShop("the raised interaction named an entity that is not the clerk");
				return false;
			}
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				FailShop("the clerk's raise did not open the menu stack");
				return false;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_MenuRoot singleton vanished between Setup and the raise");
				return false;
			}
			g_eShopTopOnOpen = (u_int)pxMenu->GetTopScreen();
			g_uShopDepthOnOpen = pxMenu->GetDepth();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_SHOP)
			{
				// The SHOPKEEP arm of ZM_Interactable::Interact does NOT show the row's
				// greeting lines first -- it opens the mart directly. A regression that
				// pushed dialogue instead would land exactly here.
				FailShop("a SHOPKEEP NPC raised something other than the SHOP screen");
				return false;
			}
			if (pxMenu->GetDepth() != 1u)
			{
				FailShop("the mart came up at a stack depth other than 1 -- a shop must be "
					"reachable by talking to its clerk, with NO pause menu underneath");
				return false;
			}

			// The freeze, against the baseline the walk captured while the player was
			// demonstrably movable (without that baseline this pair is vacuous on a
			// player who was frozen the whole time).
			g_bShopPlayerFrozen = !xPlayer.m_pxController->IsMovementEnabled();
			if (!g_xShopWalk.m_bMovementEnabledBefore)
			{
				FailShop("the player was not movable before the walk -- the freeze assertion "
					"would be vacuous");
				return false;
			}
			if (!g_bShopPlayerFrozen)
			{
				FailShop("the player was NOT frozen while the mart was open");
				return false;
			}

			g_bShopRaisePassed = true;
			g_eShopPhase = ShopPhase::AssertStock;
			g_iShopPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 8. THE STOCK IDENTITY (Ruling E). This is the assertion ZM_ShopScreen_Test
		//    is structurally incapable of making: it configures the mart from a
		//    fixture array spelled in the test, so it can only prove "the screen
		//    lists what I passed it". Here the stock comes from THE CLERK'S OWN ROW,
		//    read out of ZM_GetNpcData at RUNTIME -- never re-spelled as a literal
		//    array, which would reduce the check to "the table equals my copy of the
		//    table".
		// ------------------------------------------------------------------
		case ShopPhase::AssertStock:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving while sampling the mart");
				return false;
			}
			const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_TRADE_POST_CLERK);

			g_uShopRowStockCount = xRow.m_uStockCount;
			g_uShopScreenStockCount = xShop.GetInventoryCount();
			g_eShopModeOnOpen = (u_int)xShop.GetMode();
			g_uShopQtyOnOpen = xShop.GetQuantity();

			// Non-empty FIRST, or the per-index walk below is a loop over nothing and the
			// whole identity proof passes on a clerk who stocks air.
			if (xRow.m_uStockCount == 0u || xRow.m_paeStock == nullptr)
			{
				FailShop("the clerk's ZM_NpcData row carries NO stock -- the per-entry "
					"identity check below would be vacuous");
				return false;
			}
			if (g_uShopScreenStockCount != xRow.m_uStockCount)
			{
				FailShop("the open mart is listing a different NUMBER of entries than the "
					"clerk's own row stocks");
				return false;
			}
			for (u_int u = 0u; u < xRow.m_uStockCount; ++u)
			{
				if (xShop.GetInventoryItem(u) != xRow.m_paeStock[u])
				{
					g_iShopStockMismatchIndex = (int)u;
					FailShop("the open mart is NOT showing the clerk's own stock -- the NPC's "
						"row did not reach the screen (the first differing index is logged below)");
					return false;
				}
			}
			// BUY mode matters to every purchase assertion downstream: in SELL mode
			// GetSelectedEntryIndex indexes the player's BAG, so the "buy" below would be
			// a sale and the money/held arithmetic would be inverted.
			if (g_eShopModeOnOpen != (u_int)ZM_SHOP_MODE_BUY)
			{
				FailShop("the clerk's mart did not open in BUY mode -- the purchase "
					"assertions below would be measuring a SALE");
				return false;
			}
			// NOT "!= 0": GetQuantity() is (u_int)ClampQuantity(m_iQuantity) and
			// ClampQuantity floors at iMIN_QUANTITY == 1, so a zero is structurally
			// impossible and guarding it would guard nothing. What IS reachable is the
			// session reset: SetInventory installs the documented open-state default of
			// exactly 1, and a regression in the stepper's reset shows up right here.
			if (g_uShopQtyOnOpen != 1u)
			{
				FailShop("the mart did not open at the default quantity of 1 -- the shop "
					"screen's session reset did not run, so the purchase below would trade "
					"an unknown amount");
				return false;
			}

			g_bShopStockPassed = true;
			g_eShopPhase = ShopPhase::WalkToConfirm;
			g_iShopPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 9. WALK THE FOCUS ONTO CONFIRM with REAL Down edges. Nothing on this screen
		//    carries explicit navigation links, so this walks purely on the ENGINE
		//    SPATIAL search -- and CONFIRM is the only element that moves money.
		//    Parking the focus programmatically would prove nothing about whether a
		//    player can ever reach the button, so SetFocusedElement appears nowhere.
		// ------------------------------------------------------------------
		case ShopPhase::WalkToConfirm:
		{
			// The selection-survival check in Verify (g_iShopCursorOnLastRow > 0) is only
			// ever satisfiable because the Down walk passes through rows 1..N-1 on its way
			// to Confirm -- which needs a MULTI-ROW list. Nothing else in this file pins
			// the clerk's stock size, so trimming the row to a single id would red this
			// test with no message naming the cause. Say it here instead.
			if (ZM_GetNpcData(ZM_NPC_TRADE_POST_CLERK).m_uStockCount < 2u)
			{
				FailShop("the selection-survival check needs a MULTI-ROW mart list, and the "
					"clerk's ZM_NpcData row no longer provides one -- the Down walk could "
					"never leave row 0, so the cursor comparison in Verify would be vacuous");
				return false;
			}
			Zenith_UIComponent* pxUI = ResolveMenuRootUI();
			if (pxUI == nullptr)
			{
				FailShop("the ZM_MenuRoot UI component stopped resolving while walking to Confirm");
				return false;
			}
			Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
			// While the focus is still ON the list, mirror the screen's selection. The LAST
			// value captured here is the one the selection must STILL read once the walk
			// has landed on the control -- without it the survival deviation this screen
			// makes from the bag (ZM_UI_Shop::SettleFocus) is untested: a cursor reset to 0
			// on a control would simply buy stock entry 0 and every purchase assertion
			// below would still pass.
			if (pxFocused != nullptr
				&& ZM_UI_Shop::RowIndexFromElementName(pxFocused->GetName().c_str()) >= 0)
			{
				if (ZM_UI_MenuStack* pxWalkMenu = ResolveMenuStack())
				{
					g_iShopCursorOnLastRow = pxWalkMenu->GetShopScreen().GetCursor();
				}
			}
			if (pxFocused != nullptr && pxFocused->GetName() == ZM_UI_Shop::szCONFIRM_NAME)
			{
				g_bShopReachedConfirm = true;
				g_strShopWalkFocusName = pxFocused->GetName();
				g_eShopPhase = ShopPhase::BuySelected;
				g_iShopPhaseFrames = 0;
				return true;
			}
			if (g_iShopPhaseFrames > iSHOP_WALK_DEADLINE)
			{
				g_strShopWalkFocusName =
					(pxFocused != nullptr) ? pxFocused->GetName() : std::string();
				FailShop("the Down edges never walked the focus onto the Confirm control -- "
					"the clerk's mart cannot be transacted with by keyboard");
				return false;
			}
			// Spaced edges (one every fourth frame) so each press is a clean edge. This is
			// a FRAME-COUNT cadence, not a time one, so it is NOT doubled for 1/60.
			if ((g_iShopPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 10. BUY THE SELECTED ENTRY. ONE Enter edge, with idle frames on either side
		//     so the edge-detected ZM_InputActions::ReadConfirmPressed fires exactly
		//     once. NOTHING about which item is bought is hardcoded: the flat entry
		//     index, the item and the price are all read OFF THE SCREEN at the press
		//     frame, because the walk above is what decided where the selection ended
		//     up and the selection SURVIVING that walk is part of what is under test.
		// ------------------------------------------------------------------
		case ShopPhase::BuySelected:
		{
			if (g_iShopPhaseFrames == iPRESS_FRAME)
			{
				ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
				if (pxMenu == nullptr)
				{
					FailShop("the ZM_UI_MenuStack singleton stopped resolving before the purchase");
					return false;
				}
				const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
				g_iShopCursorAtConfirm = xShop.GetCursor();
				g_uShopBoughtQuantity = xShop.GetQuantity();

				ZM_GameState* pxState = nullptr;
				if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
				{
					FailShop("the live game state did not resolve before the purchase");
					return false;
				}
				// The FLAT entry index resolved BY THE SCREEN -- never the raw cursor, which
				// is a PAGE-RELATIVE row and would name the wrong stock entry on any page but
				// the first.
				const int iEntry = xShop.GetSelectedEntryIndex(pxState->m_xBag);
				const ZM_ITEM_ID eItem = (iEntry >= 0)
					? xShop.GetInventoryItem((u_int)iEntry)
					: ZM_ITEM_NONE;
				g_eShopBoughtItem = (u_int)eItem;
				g_uShopBoughtPrice = ZM_ShopBuyPrice(eItem);
				if (!ReadLiveMoneyAndHeld(eItem, g_uShopMoneyBeforeBuy, g_uShopHeldBeforeBuy))
				{
					FailShop("the live game state did not resolve before the purchase");
					return false;
				}
				// AFFORDABILITY, named explicitly. The Down walk lands on whichever entry the
				// clerk's row happens to end on; if that row is ever reordered or extended so
				// the landing entry costs more than the live purse, ZM_ShopLogic refuses and
				// the test would otherwise red on bare money arithmetic with no diagnostic.
				if (g_uShopMoneyBeforeBuy < g_uShopBoughtPrice * g_uShopBoughtQuantity)
				{
					FailShop("the mart's selected entry costs more than the LIVE purse -- the "
						"purchase assertions cannot run (the clerk's stock row or the starting "
						"money changed; prices, quantity and money are logged below)");
					return false;
				}

				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iShopPhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving after the purchase");
				return false;
			}
			g_eShopResultAfterBuy = (u_int)pxMenu->GetShopScreen().GetLastResult();
			g_bShopReportedResult = pxMenu->GetShopScreen().HasResult();
			if (!ReadLiveMoneyAndHeld((ZM_ITEM_ID)g_eShopBoughtItem,
				g_uShopMoneyAfterBuy, g_uShopHeldAfterBuy))
			{
				FailShop("the live game state did not resolve after the purchase");
				return false;
			}

			g_bShopBuyPassed = true;
			g_eShopPhase = ShopPhase::CloseShop;
			g_iShopPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 11. LEAVE by spaced ESCAPE edges (Ruling G -- matching ZM_ShopScreen_Test,
		//     NOT via the Menu_ShopExit control, which is a different code path).
		// ------------------------------------------------------------------
		case ShopPhase::CloseShop:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving while closing the mart");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bShopClosed = true;
				g_uShopDepthOnClose = pxMenu->GetDepth();
				g_bShopFocusClearedOnClose = MenuFocusCleared();

				NpcPlayerView xPlayer;
				if (!FindActiveNpcPlayer(xPlayer))
				{
					FailShop("the player disappeared while the mart was closing");
					return false;
				}
				g_bShopMovementReenabled = xPlayer.m_pxController->IsMovementEnabled();

				g_bShopClosePassed = true;
				g_eShopPhase = ShopPhase::Done;
				return false;
			}
			if (g_iShopPhaseFrames > iSHOP_CLOSE_DEADLINE)
			{
				FailShop("the mart never closed after the Escape edges");
				return false;
			}
			// The mart was raised over an EMPTY stack, so one pop empties it and closes
			// the menu. Frame-count cadence again -- not doubled for 1/60.
			if ((g_iShopPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			}
			return true;
		}

		case ShopPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_NpcShop()
	{
		ClearWalkInput(g_xShopWalk);
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		if (g_bShopSkipped)
		{
			// The harness already recorded the skip + its reason in Setup.
			return true;
		}

		bool bPassed = g_bShopRaisePassed
			&& g_bShopStockPassed
			&& g_bShopBuyPassed
			&& g_bShopClosePassed
			&& WalkPassed(g_xShopWalk);

		// --- CONFIRM is KEY-REACHABLE, and the SELECTION SURVIVED the walk ---
		if (!g_bShopReachedConfirm)
		{
			bPassed = false;
		}
		// The survival contract has to be pinned BY VALUE across the walk: with the
		// cursor simply reset to 0 on a control, every purchase assertion below still
		// passes (they are all derived from whatever the cursor reads at the press
		// frame), so only comparing the selection ACROSS the walk can catch it. The
		// walk must also have moved off row 0 first, or the comparison is vacuous --
		// with a six-item mart it passes through rows 1..5 on its way to Confirm.
		if (g_iShopCursorOnLastRow <= 0 || g_iShopCursorAtConfirm != g_iShopCursorOnLastRow)
		{
			bPassed = false;
		}
		// The purchase must have been anchored on a REAL, PRICED entry, or the money
		// arithmetic below is satisfied by "0 changed by 0". Both of these ARE
		// falsifiable: the selected entry can resolve to ZM_ITEM_NONE / out of range, and
		// ZM_ShopBuyPrice can return 0. A "quantity != 0" clause was deliberately NOT
		// added alongside them -- ZM_UI_Shop::GetQuantity clamps at iMIN_QUANTITY == 1, so
		// a zero there is unreachable and the clause could never fire. The reachable
		// quantity regression is guarded at the open, in the AssertStock phase.
		if (g_eShopBoughtItem >= (u_int)ZM_ITEM_COUNT
			|| g_uShopBoughtPrice == 0u)
		{
			bPassed = false;
		}
		else
		{
			const u_int uTotal = g_uShopBoughtPrice * g_uShopBoughtQuantity;
			if (g_eShopResultAfterBuy != (u_int)ZM_SHOP_OK || !g_bShopReportedResult)
			{
				bPassed = false;
			}
			if (g_uShopMoneyBeforeBuy < uTotal
				|| g_uShopMoneyAfterBuy != g_uShopMoneyBeforeBuy - uTotal)
			{
				bPassed = false;
			}
			if (g_uShopHeldAfterBuy != g_uShopHeldBeforeBuy + g_uShopBoughtQuantity)
			{
				bPassed = false;
			}
		}
		if (!g_bShopClosed || !g_bShopMovementReenabled || !g_bShopFocusClearedOnClose)
		{
			bPassed = false;
		}

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_NpcShop] %s", g_szShopFailure);
			LogWalkDiagnostics(g_xShopWalk, "ZM_NpcShop");
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcShop] raise: delta=%u (want 1) top=%u (want SHOP=%u) depth=%u (want 1) "
				"frozen=%d movableBefore=%d",
				g_uShopRaiseDelta, g_eShopTopOnOpen, (u_int)ZM_MENU_SCREEN_SHOP,
				g_uShopDepthOnOpen, (int)g_bShopPlayerFrozen,
				(int)g_xShopWalk.m_bMovementEnabledBefore);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcShop] stock: clerkRowCount=%u screenCount=%u firstMismatchIndex=%d "
				"mode=%u (want BUY=%u) qty=%u",
				g_uShopRowStockCount, g_uShopScreenStockCount, g_iShopStockMismatchIndex,
				g_eShopModeOnOpen, (u_int)ZM_SHOP_MODE_BUY, g_uShopQtyOnOpen);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcShop] purchase: reachedConfirm=%d focus='%s' cursorOnLastRow=%d "
				"cursorAtConfirm=%d item=%u ('%s') price=%u qty=%u money %u->%u held %u->%u "
				"result=%u (want OK=%u) reported=%d",
				(int)g_bShopReachedConfirm, g_strShopWalkFocusName.c_str(),
				g_iShopCursorOnLastRow, g_iShopCursorAtConfirm, g_eShopBoughtItem,
				ZM_GetItemName((ZM_ITEM_ID)g_eShopBoughtItem),
				g_uShopBoughtPrice, g_uShopBoughtQuantity,
				g_uShopMoneyBeforeBuy, g_uShopMoneyAfterBuy,
				g_uShopHeldBeforeBuy, g_uShopHeldAfterBuy,
				g_eShopResultAfterBuy, (u_int)ZM_SHOP_OK, (int)g_bShopReportedResult);
			// The depth on close is logged but deliberately NOT asserted: IsOpen() is
			// literally !m_xStack.IsEmpty(), so "depth == 0 once IsOpen() is false" is a
			// restatement of the implementation rather than a check of it. The real
			// close-side assertions are movement re-enabled + focus cleared.
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcShop] close: closed=%d depth=%u movReenabled=%d focusCleared=%d "
				"phaseFlags raise=%d stock=%d buy=%d close=%d",
				(int)g_bShopClosed, g_uShopDepthOnClose, (int)g_bShopMovementReenabled,
				(int)g_bShopFocusClearedOnClose, (int)g_bShopRaisePassed,
				(int)g_bShopStockPassed, (int)g_bShopBuyPassed, (int)g_bShopClosePassed);
		}
		return bPassed;
	}

	// ========================================================================
	// ZM_NpcHeal_Test -- walk up to the AUTHORED Caretaker and answer its prompt
	// BOTH ways in ONE session.
	//
	// The two passes must DIFFER IN OUTCOME, or neither is falsifiable: YES leaves
	// the LIVE party at full HP and puts the healed line on screen, NO leaves it
	// damaged and closes immediately. The party is damaged before EACH pass and
	// ZM_PartyNeedsHealing is asserted TRUE before each -- without that,
	// ZM_ApplyCareCenterHeal returns false, the menu stack never queues the healed
	// line, and every downstream heal assertion becomes unfalsifiable.
	// ========================================================================

	// What the AUTHORED dialogue widgets report this frame. The model-only answers
	// cannot see a typo'd element name, a missing AddStep_CreateUIButton or a
	// ZM_ConfigureMenuRoot FindElement that silently returned nullptr --
	// ZM_UI_DialogueBox null-guards all three, so nothing would render while every
	// model assertion stayed green.
	struct HealChoiceView
	{
		bool        m_bResolved     = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bPanelFound   = false;
		bool        m_bPanelVisible = false;
		bool        m_bTextFound    = false;
		bool        m_bTextVisible  = false;
		std::string m_strText;
		bool        m_bYesFound     = false;
		bool        m_bNoFound      = false;
		bool        m_bYesVisible   = false;
		bool        m_bNoVisible    = false;
		// Focusability is HALF the contract: the engine nav collects visible + FOCUSABLE
		// elements, so a non-focusable button is an unanswerable question.
		bool        m_bYesFocusable = false;
		bool        m_bNoFocusable  = false;
		std::string m_strFocusedName;
	};

	HealChoiceView ReadHealChoiceElements()
	{
		HealChoiceView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		if (Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_DialogueBox::szPANEL_NAME))
		{
			xView.m_bPanelFound = true;
			xView.m_bPanelVisible = pxPanel->IsVisible();
		}
		if (Zenith_UI::Zenith_UIText* pxText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_DialogueBox::szTEXT_NAME))
		{
			xView.m_bTextFound = true;
			xView.m_bTextVisible = pxText->IsVisible();
			xView.m_strText = pxText->GetText();
		}
		if (Zenith_UI::Zenith_UIButton* pxYes =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_DialogueBox::szYES_NAME))
		{
			xView.m_bYesFound = true;
			xView.m_bYesVisible = pxYes->IsVisible();
			xView.m_bYesFocusable = pxYes->IsFocusable();
		}
		if (Zenith_UI::Zenith_UIButton* pxNo =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_DialogueBox::szNO_NAME))
		{
			xView.m_bNoFound = true;
			xView.m_bNoVisible = pxNo->IsVisible();
			xView.m_bNoFocusable = pxNo->IsFocusable();
		}
		if (Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement())
		{
			xView.m_strFocusedName = pxFocused->GetName();
		}
		return xView;
	}

	// The LIVE party's health, read FRESH (never cached -- the pool relocates).
	struct HealPartyView
	{
		bool  m_bResolved      = false;
		u_int m_uCount         = 0u;
		u_int m_uFullHpMembers = 0u;
		u_int m_uLeadCurrentHp = 0u;
		u_int m_uLeadMaxHp     = 0u;
	};

	HealPartyView ReadLiveParty()
	{
		HealPartyView xView;
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;
		xView.m_uCount = pxState->m_xParty.Count();
		for (u_int u = 0u; u < xView.m_uCount; ++u)
		{
			const ZM_Monster& xMember = pxState->m_xParty.Get(u);
			if (xMember.m_uCurrentHp >= xMember.GetMaxHP())
			{
				++xView.m_uFullHpMembers;
			}
		}
		if (xView.m_uCount > 0u)
		{
			const ZM_Monster& xLead = pxState->m_xParty.Get(0u);
			xView.m_uLeadCurrentHp = xLead.m_uCurrentHp;
			xView.m_uLeadMaxHp = xLead.GetMaxHP();
		}
		return xView;
	}

	// Drop every member of the LIVE party to 1 HP. A state-setter on the real game
	// state (no battle needed to produce a damaged party); false when nothing could
	// be damaged, which would make every heal assertion vacuous.
	bool DamageLiveParty(u_int& uDamagedOut)
	{
		uDamagedOut = 0u;
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return false;
		}
		for (u_int u = 0u; u < pxState->m_xParty.Count(); ++u)
		{
			ZM_Monster& xMember = pxState->m_xParty.Get(u);
			if (xMember.GetMaxHP() <= 1u)
			{
				continue;   // nothing to take off it
			}
			xMember.m_uCurrentHp = 1u;
			++uDamagedOut;
		}
		return uDamagedOut > 0u;
	}

	// ZM_PartyNeedsHealing over the LIVE party; false when no game state resolves.
	bool ReadPartyNeedsHealing(bool& bNeedsOut)
	{
		bNeedsOut = false;
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return false;
		}
		bNeedsOut = ZM_PartyNeedsHealing(pxState->m_xParty);
		return true;
	}

	enum class HealPhase
	{
		Walk,             // the shared walk-up machine (damage happens in its after-boot hook)
		AssertRaiseYes,
		ReadPrompt,       // spaced Enter edges until the box awaits the answer
		SampleChoice,     // ...and what the authored widgets are showing
		WalkToNo,         // REAL arrow edges: the focus must reach No...
		WalkBackToYes,    // ...and come back, so BOTH answers are proved reachable
		ConfirmYes,       // ONE Enter on Yes -> the LIVE party is healed
		ReadHealedLine,   // ...spaced Enter edges read the confirmation out, THEN it closes
		DamageAgain,      // re-damage for the NO pass
		ReRaiseWalk,      // re-poll / re-approach the caretaker, then press E again
		AssertRaiseNo,
		ReadPromptNo,
		WalkToNoAgain,
		ConfirmNo,        // ONE Enter on No -> nothing is healed, the menu closes
		Done,
	};

	WalkContext g_xHealWalk;
	HealPhase   g_eHealPhase = HealPhase::Done;
	int         g_iHealPhaseFrames = 0;
	bool        g_bHealSkipped = false;
	bool        g_bHealPrereqsPresent = false;
	const char* g_szHealFailure = "test did not reach verification";

	// ---- Phase flags. ALL DEFAULT TO FAILING. ----
	bool g_bHealRaiseYesPassed  = false;
	bool g_bHealPromptPassed    = false;
	bool g_bHealChoicePassed    = false;
	bool g_bHealWalkYesNoPassed = false;
	bool g_bHealYesPassed       = false;
	bool g_bHealHealedLinePassed = false;
	bool g_bHealRaiseNoPassed   = false;
	bool g_bHealNoPassed        = false;

	// ---- The YES pass ----
	u_int         g_uHealDamagedYes    = 0u;
	bool          g_bHealNeededYes     = false;
	HealPartyView g_xHealPartyDamagedYes;
	u_int         g_uHealRaiseDeltaYes = 0u;
	u_int         g_eHealTopOnOpen     = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE
	u_int         g_uHealDepthOnOpen   = 0u;                           // want 1
	bool          g_bHealPlayerFrozen  = false;
	u_int         g_eHealActionOnOpen  = (u_int)ZM_DIALOGUE_ACTION_NONE;   // want HEAL_PARTY
	bool          g_bHealAwaitingChoice = false;
	int           g_iHealReadPresses   = 0;
	HealChoiceView g_xHealChoiceOnAwait;
	bool          g_bHealReachedNo     = false;
	bool          g_bHealReturnedToYes = false;
	std::string   g_strHealWalkFocusName;
	u_int         g_eHealAnswerYes     = (u_int)ZM_DIALOGUE_CHOICE_NONE;   // want YES
	HealPartyView g_xHealPartyAfterYes;
	u_int         g_eHealTopAfterYes   = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE (NOT closed)
	bool          g_bHealHealedLineActive = false;
	HealChoiceView g_xHealHealedLineElements;
	int           g_iHealHealedLinePresses = 0;
	HealChoiceView g_xHealChoiceOnClose;
	bool          g_bHealClosedAfterYes = false;
	bool          g_bHealMovementReenabled = false;

	// ---- The NO pass ----
	u_int         g_uHealDamagedNo    = 0u;
	bool          g_bHealNeededNo     = false;
	HealPartyView g_xHealPartyDamagedNo;
	u_int         g_uHealRaiseDeltaNo = 0u;
	u_int         g_eHealTopOnOpenNo  = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE
	bool          g_bHealAwaitingChoiceNo = false;
	bool          g_bHealFocusedNoOnConfirm = false;
	u_int         g_eHealAnswerNo     = (u_int)ZM_DIALOGUE_CHOICE_NONE;   // want NO
	HealPartyView g_xHealPartyAfterNo;
	bool          g_bHealClosedAfterNo = false;

	void FailHeal(const char* szReason)
	{
		g_szHealFailure = szReason;
		g_eHealPhase = HealPhase::Done;
		ClearWalkInput(g_xHealWalk);
	}

	// The walk's after-boot hook: damage the LIVE party BEFORE the walk, so the YES
	// answer has something to change. A captureless function pointer -- it returns
	// nullptr on success and the failure text otherwise.
	const char* Hook_DamagePartyBeforeWalk()
	{
		if (!DamageLiveParty(g_uHealDamagedYes))
		{
			return "the LIVE party could not be damaged before the walk -- every heal "
				"assertion downstream would be vacuous";
		}
		g_xHealPartyDamagedYes = ReadLiveParty();
		if (!ReadPartyNeedsHealing(g_bHealNeededYes))
		{
			return "the live game state did not resolve while damaging the party";
		}
		if (!g_bHealNeededYes)
		{
			// ZM_ApplyCareCenterHeal returns FALSE when nothing needed healing, in which
			// case ZM_UI_MenuStack does NOT queue the healed line and the prompt pops
			// immediately -- so without this the whole YES pass proves nothing.
			return "ZM_PartyNeedsHealing said the party needs nothing right after it was "
				"damaged -- the YES pass could not prove a heal";
		}
		return nullptr;
	}

	void Setup_NpcHeal()
	{
		g_xHealWalk = WalkContext{};
		g_xHealWalk.m_szTargetEntityName = szCARETAKER_ENTITY_NAME;
		g_xHealWalk.m_pfnAfterBoot = &Hook_DamagePartyBeforeWalk;

		g_eHealPhase = HealPhase::Done;
		g_iHealPhaseFrames = 0;
		g_bHealSkipped = false;
		g_bHealPrereqsPresent = false;
		g_szHealFailure = "test did not reach verification";

		g_bHealRaiseYesPassed = false;
		g_bHealPromptPassed = false;
		g_bHealChoicePassed = false;
		g_bHealWalkYesNoPassed = false;
		g_bHealYesPassed = false;
		g_bHealHealedLinePassed = false;
		g_bHealRaiseNoPassed = false;
		g_bHealNoPassed = false;

		g_uHealDamagedYes = 0u;
		g_bHealNeededYes = false;
		g_xHealPartyDamagedYes = HealPartyView{};
		g_uHealRaiseDeltaYes = 0u;
		g_eHealTopOnOpen = (u_int)ZM_MENU_SCREEN_NONE;
		g_uHealDepthOnOpen = 0u;
		g_bHealPlayerFrozen = false;
		g_eHealActionOnOpen = (u_int)ZM_DIALOGUE_ACTION_NONE;
		g_bHealAwaitingChoice = false;
		g_iHealReadPresses = 0;
		g_xHealChoiceOnAwait = HealChoiceView{};
		g_bHealReachedNo = false;
		g_bHealReturnedToYes = false;
		g_strHealWalkFocusName.clear();
		g_eHealAnswerYes = (u_int)ZM_DIALOGUE_CHOICE_NONE;
		g_xHealPartyAfterYes = HealPartyView{};
		g_eHealTopAfterYes = (u_int)ZM_MENU_SCREEN_NONE;
		g_bHealHealedLineActive = false;
		g_xHealHealedLineElements = HealChoiceView{};
		g_iHealHealedLinePresses = 0;
		g_xHealChoiceOnClose = HealChoiceView{};
		g_bHealClosedAfterYes = false;
		g_bHealMovementReenabled = false;

		g_uHealDamagedNo = 0u;
		g_bHealNeededNo = false;
		g_xHealPartyDamagedNo = HealPartyView{};
		g_uHealRaiseDeltaNo = 0u;
		g_eHealTopOnOpenNo = (u_int)ZM_MENU_SCREEN_NONE;
		g_bHealAwaitingChoiceNo = false;
		g_bHealFocusedNoOnConfirm = false;
		g_eHealAnswerNo = (u_int)ZM_DIALOGUE_CHOICE_NONE;
		g_xHealPartyAfterNo = HealPartyView{};
		g_bHealClosedAfterNo = false;

		Zenith_InputSimulator::ResetAllInputState();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// ---- The ONLY two skips (see ZM_NpcShop's Setup for the guard-order rule) ----
		g_bHealPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bHealPrereqsPresent)
		{
			g_bHealSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcHeal] the Dawnmere scene / terrain bake is absent or incomplete -- "
				"there is no world to walk through (run a *_True config once to bake it)");
			return;
		}
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bHealSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcHeal] no persistent ZM_MenuRoot / ZM_UI_MenuStack singleton -- "
				"FrontEnd.zscen has not been baked, so there is no prompt for the caretaker "
				"to raise");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fNPC_FIXED_DT);
		g_xHealWalk.m_eStage = WalkStage::Boot;
		g_eHealPhase = HealPhase::Walk;
		g_xEngine.Scenes().LoadSceneByIndex(
			iNPC_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_NpcHeal(int)
	{
		if (g_eHealPhase == HealPhase::Done)
		{
			return false;
		}
		++g_iHealPhaseFrames;

		switch (g_eHealPhase)
		{
		// ------------------------------------------------------------------
		// 1-6. The shared walk. NOTE the approach is -X here (DriveTowardXZ holds A
		//      rather than D), and the party is damaged by the after-boot hook BEFORE
		//      the first probe key is held.
		// ------------------------------------------------------------------
		case HealPhase::Walk:
		{
			const WalkTick eTick = TickWalk(g_xHealWalk);
			if (eTick == WalkTick::Failed)
			{
				FailHeal(g_xHealWalk.m_szFailure);
				return false;
			}
			if (eTick == WalkTick::Pressed)
			{
				g_eHealPhase = HealPhase::AssertRaiseYes;
				g_iHealPhaseFrames = 0;
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 7. THE RAISE. One screen, at the caretaker, and it is the PROMPT -- with
		//    HEAL_PARTY armed on it, over an otherwise EMPTY stack, player frozen.
		// ------------------------------------------------------------------
		case HealPhase::AssertRaiseYes:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailHeal("the player disappeared before the raise could be asserted");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			g_uHealRaiseDeltaYes = xRuntime.GetRaiseCount() - g_xHealWalk.m_uRaiseCountBefore;
			if (xRuntime.GetRaiseCount() != g_xHealWalk.m_uRaiseCountBefore + 1u)
			{
				FailHeal("the walk-up press did not raise EXACTLY one screen");
				return false;
			}
			if (xRuntime.GetLastResult() != ZM_INTERACT_OK)
			{
				FailHeal("the walk-up press did not latch ZM_INTERACT_OK");
				return false;
			}
			if (xRuntime.GetLastTarget() != g_xHealWalk.m_xTargetEntityID)
			{
				FailHeal("the raised interaction named an entity that is not the caretaker");
				return false;
			}
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				FailHeal("the caretaker's raise did not open the menu stack");
				return false;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_MenuRoot singleton vanished between Setup and the raise");
				return false;
			}
			g_eHealTopOnOpen = (u_int)pxMenu->GetTopScreen();
			g_uHealDepthOnOpen = pxMenu->GetDepth();
			g_eHealActionOnOpen = (u_int)pxMenu->GetPendingDialogueAction();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailHeal("a CARETAKER NPC raised something other than the DIALOGUE screen");
				return false;
			}
			if (pxMenu->GetDepth() != 1u)
			{
				FailHeal("the Care Center prompt came up at a stack depth other than 1 -- it "
					"must be reachable by talking to the caretaker, with NO pause menu under it");
				return false;
			}
			// The CARETAKER arm raises the PROMPT, not the row's greeting: without this
			// the test would pass on an NPC wired to TryPushDialogue, which would put its
			// lines on screen and heal nothing.
			if (pxMenu->GetPendingDialogueAction() != ZM_DIALOGUE_ACTION_HEAL_PARTY)
			{
				FailHeal("the caretaker's raise did not arm the HEAL_PARTY action -- it "
					"raised a plain conversation, not the Care Center prompt");
				return false;
			}

			g_bHealPlayerFrozen = !xPlayer.m_pxController->IsMovementEnabled();
			if (!g_xHealWalk.m_bMovementEnabledBefore)
			{
				FailHeal("the player was not movable before the walk -- the freeze assertion "
					"would be vacuous");
				return false;
			}
			if (!g_bHealPlayerFrozen)
			{
				FailHeal("the player was NOT frozen while the Care Center prompt was up");
				return false;
			}

			g_bHealRaiseYesPassed = true;
			g_eHealPhase = HealPhase::ReadPrompt;
			g_iHealPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 8. READ PAST THE PROMPT LINE. Spaced Enter edges until the box AWAITS the
		//    answer. A box that CLOSES instead is the exact regression the prompt
		//    exists to prevent, so it fails immediately rather than on a deadline.
		// ------------------------------------------------------------------
		case HealPhase::ReadPrompt:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_UI_MenuStack singleton stopped resolving while reading the prompt");
				return false;
			}
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				g_bHealAwaitingChoice = true;
				g_bHealPromptPassed = true;
				g_eHealPhase = HealPhase::SampleChoice;
				g_iHealPhaseFrames = 0;
				return true;
			}
			if (!pxMenu->IsOpen())
			{
				FailHeal("the prompt CLOSED instead of awaiting an answer");
				return false;
			}
			if (g_iHealPhaseFrames > iREAD_DEADLINE)
			{
				FailHeal("the Enter edges never read the prompt through to its question");
				return false;
			}
			if ((g_iHealPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				++g_iHealReadPresses;
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 9. SAMPLE THE AUTHORED CHOICE WIDGETS.
		// ------------------------------------------------------------------
		case HealPhase::SampleChoice:
		{
			g_xHealChoiceOnAwait = ReadHealChoiceElements();
			g_eHealPhase = HealPhase::WalkToNo;
			g_iHealPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 10. BOTH ANSWERS MUST BE KEY-REACHABLE. Real arrow edges Yes -> No -> Yes;
		//     the two buttons carry NO explicit navigation links, so this walks purely
		//     on the ENGINE SPATIAL search.
		// ------------------------------------------------------------------
		case HealPhase::WalkToNo:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szNO_NAME)
			{
				g_bHealReachedNo = true;
				g_strHealWalkFocusName = strFocus;
				g_eHealPhase = HealPhase::WalkBackToYes;
				g_iHealPhaseFrames = 0;
				return true;
			}
			if (g_iHealPhaseFrames > iCHOICE_WALK_DEADLINE)
			{
				g_strHealWalkFocusName = strFocus;
				FailHeal("the Right edges never walked the focus onto the No button");
				return false;
			}
			if ((g_iHealPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_RIGHT);
			}
			return true;
		}

		case HealPhase::WalkBackToYes:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szYES_NAME)
			{
				g_bHealReturnedToYes = true;
				g_bHealWalkYesNoPassed = true;
				g_strHealWalkFocusName = strFocus;
				g_eHealPhase = HealPhase::ConfirmYes;
				g_iHealPhaseFrames = 0;
				return true;
			}
			if (g_iHealPhaseFrames > iCHOICE_WALK_DEADLINE)
			{
				g_strHealWalkFocusName = strFocus;
				FailHeal("the Left edges never walked the focus back onto the Yes button");
				return false;
			}
			if ((g_iHealPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_LEFT);
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 11. ANSWER YES. ONE Enter edge on the Yes button (proved focused by the walk
		//     above, never assumed). The answer is read from the HOST LATCH -- see the
		//     note at the read below.
		// ------------------------------------------------------------------
		case HealPhase::ConfirmYes:
		{
			if (g_iHealPhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iHealPhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_UI_MenuStack singleton stopped resolving after the YES answer");
				return false;
			}
			// ★ THE LATCH IS STALE-SATISFIABLE, so prove the press RESOLVED the choice
			// first. m_eLastDialogueAnswer is cleared ONLY in OnStart and
			// ReadFromDataStream -- CloseMenu deliberately never touches it -- and
			// ZM_MenuRoot is DontDestroyOnLoad, so it survives every test boundary in a
			// batched run. If the Enter edge were never consumed the box would still be
			// awaiting, the DIALOGUE screen would still be top, and the read below would
			// return whatever an EARLIER test latched.
			// NOT done as "capture the answer before the press and require it to change":
			// a prior test that ended on YES would make that a FALSE failure.
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				FailHeal("the Enter on Yes never resolved the armed choice -- the box is still "
					"awaiting an answer, so GetLastDialogueAnswer() below would read a STALE latch "
					"from an earlier test (the latch survives every test boundary -- see F1)");
				return false;
			}
			// ★ THE HOST LATCH, and it MUST be this one. A prompt raised over an EMPTY
			// stack pops to EMPTY on resolve, which CloseMenu()s and Reset()s the box and
			// clears its stored answer -- all in ONE OnUpdate. GetDialogue().GetChoice()
			// after the fact therefore reads NONE no matter what was answered. Both of
			// this file's answers are raised over an empty stack, so both are affected.
			g_eHealAnswerYes = (u_int)pxMenu->GetLastDialogueAnswer();
			g_xHealPartyAfterYes = ReadLiveParty();
			g_eHealTopAfterYes = (u_int)pxMenu->GetTopScreen();
			g_bHealHealedLineActive = pxMenu->GetDialogue().IsActive();
			g_xHealHealedLineElements = ReadHealChoiceElements();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				// A heal that actually changed something is never silent: the menu stack
				// queues ZM_CareCenterHealedLine() onto the (reset, unarmed) box and does
				// NOT pop, so the DIALOGUE screen must still be up here.
				FailHeal("the healed confirmation line never came up after the YES answer");
				return false;
			}

			g_bHealYesPassed = true;
			g_eHealPhase = HealPhase::ReadHealedLine;
			g_iHealPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 12. READ THE HEALED LINE OUT, and the menu comes down.
		// ------------------------------------------------------------------
		case HealPhase::ReadHealedLine:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_UI_MenuStack singleton stopped resolving on the healed line");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bHealClosedAfterYes = true;
				// CloseMenu hides the widgets directly (the per-frame Present stops running
				// the moment the stack empties), so they must be down here.
				g_xHealChoiceOnClose = ReadHealChoiceElements();

				NpcPlayerView xPlayer;
				if (!FindActiveNpcPlayer(xPlayer))
				{
					FailHeal("the player disappeared as the Care Center prompt closed");
					return false;
				}
				g_bHealMovementReenabled = xPlayer.m_pxController->IsMovementEnabled();

				g_bHealHealedLinePassed = true;
				g_eHealPhase = HealPhase::DamageAgain;
				g_iHealPhaseFrames = 0;
				return true;
			}
			if (g_iHealPhaseFrames > iREAD_DEADLINE)
			{
				FailHeal("the healed confirmation line never closed after the Enter edges");
				return false;
			}
			if ((g_iHealPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				++g_iHealHealedLinePresses;
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 13. RE-DAMAGE for the NO pass. Without this the NO pass is unfalsifiable:
		//     a party that is already whole is "still not healed" no matter what the
		//     answer did.
		// ------------------------------------------------------------------
		case HealPhase::DamageAgain:
		{
			if (!DamageLiveParty(g_uHealDamagedNo))
			{
				FailHeal("the LIVE party could not be re-damaged for the NO pass");
				return false;
			}
			g_xHealPartyDamagedNo = ReadLiveParty();
			if (!ReadPartyNeedsHealing(g_bHealNeededNo) || !g_bHealNeededNo)
			{
				FailHeal("ZM_PartyNeedsHealing said the party needs nothing before the NO "
					"pass -- the 'still damaged' assertion would prove nothing");
				return false;
			}

			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailHeal("the player disappeared before the second interact press");
				return false;
			}
			// Re-enter the SHARED approach leg. Not a drift correction (the player has
			// been stationary and frozen since the first press -- see RearmWalkApproach):
			// it re-baselines the watchdog / deadline / displacement origin for a second
			// leg, and re-polls before pressing E again, re-walking only if it has to.
			RearmWalkApproach(g_xHealWalk, xPlayer.m_xPosition,
				xPlayer.m_pxController->GetInteractionRuntime().GetRaiseCount());

			g_eHealPhase = HealPhase::ReRaiseWalk;
			g_iHealPhaseFrames = 0;
			return true;
		}

		case HealPhase::ReRaiseWalk:
		{
			const WalkTick eTick = TickWalk(g_xHealWalk);
			if (eTick == WalkTick::Failed)
			{
				FailHeal(g_xHealWalk.m_szFailure);
				return false;
			}
			if (eTick == WalkTick::Pressed)
			{
				g_eHealPhase = HealPhase::AssertRaiseNo;
				g_iHealPhaseFrames = 0;
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 14. THE SECOND RAISE. A STALE ARMED CHOICE on the box would make
		//     OpenCareCenterPrompt refuse outright (a prompt owns the box; it never
		//     interleaves itself into someone else's conversation), so the answered
		//     box having reset itself completely is exactly what this catches.
		// ------------------------------------------------------------------
		case HealPhase::AssertRaiseNo:
		{
			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailHeal("the player disappeared before the second raise could be asserted");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			g_uHealRaiseDeltaNo = xRuntime.GetRaiseCount() - g_xHealWalk.m_uRaiseCountBefore;
			if (xRuntime.GetRaiseCount() != g_xHealWalk.m_uRaiseCountBefore + 1u)
			{
				FailHeal("the SECOND interact press did not raise exactly one more screen -- "
					"the answered prompt did not reset the dialogue box");
				return false;
			}
			// Symmetric with the YES raise: the count and the target alone do not say the
			// SECOND press was accepted for the right reason.
			if (xRuntime.GetLastResult() != ZM_INTERACT_OK)
			{
				FailHeal("the SECOND interact press did not latch ZM_INTERACT_OK");
				return false;
			}
			if (xRuntime.GetLastTarget() != g_xHealWalk.m_xTargetEntityID)
			{
				FailHeal("the second raise named an entity that is not the caretaker");
				return false;
			}
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_UI_MenuStack singleton stopped resolving on the second raise");
				return false;
			}
			g_eHealTopOnOpenNo = (u_int)pxMenu->GetTopScreen();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailHeal("the DIALOGUE screen never came up for the NO pass");
				return false;
			}

			g_bHealRaiseNoPassed = true;
			g_eHealPhase = HealPhase::ReadPromptNo;
			g_iHealPhaseFrames = 0;
			return true;
		}

		case HealPhase::ReadPromptNo:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_UI_MenuStack singleton stopped resolving on the NO pass");
				return false;
			}
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				g_bHealAwaitingChoiceNo = true;
				g_eHealPhase = HealPhase::WalkToNoAgain;
				g_iHealPhaseFrames = 0;
				return true;
			}
			if (!pxMenu->IsOpen())
			{
				FailHeal("the NO pass's prompt CLOSED instead of awaiting an answer");
				return false;
			}
			if (g_iHealPhaseFrames > iREAD_DEADLINE)
			{
				FailHeal("the NO pass never read its prompt through to the question");
				return false;
			}
			if ((g_iHealPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case HealPhase::WalkToNoAgain:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szNO_NAME)
			{
				g_bHealFocusedNoOnConfirm = true;
				g_eHealPhase = HealPhase::ConfirmNo;
				g_iHealPhaseFrames = 0;
				return true;
			}
			if (g_iHealPhaseFrames > iCHOICE_WALK_DEADLINE)
			{
				g_strHealWalkFocusName = strFocus;
				FailHeal("the focus never reached the No button on the NO pass");
				return false;
			}
			if ((g_iHealPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_RIGHT);
			}
			return true;
		}

		// ------------------------------------------------------------------
		// 15. ANSWER NO. Nothing is healed, so there is no confirmation line to read
		//     and the menu closes on the answer.
		// ------------------------------------------------------------------
		case HealPhase::ConfirmNo:
		{
			if (g_iHealPhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iHealPhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailHeal("the ZM_UI_MenuStack singleton stopped resolving after the NO answer");
				return false;
			}
			// Same stale-latch guard as the YES pass: the answer is cleared only in
			// OnStart / ReadFromDataStream, never by CloseMenu, and ZM_MenuRoot is
			// DontDestroyOnLoad. Again NOT written as "require the answer to change" --
			// an earlier test that ended on NO would make that a false failure.
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				FailHeal("the Enter on No never resolved the armed choice -- the box is still "
					"awaiting an answer, so GetLastDialogueAnswer() below would read a STALE latch "
					"from an earlier test (the latch survives every test boundary -- see F1)");
				return false;
			}
			g_eHealAnswerNo = (u_int)pxMenu->GetLastDialogueAnswer();   // the HOST latch again
			g_bHealClosedAfterNo = !pxMenu->IsOpen();
			g_xHealPartyAfterNo = ReadLiveParty();

			g_bHealNoPassed = true;
			g_eHealPhase = HealPhase::Done;
			return false;
		}

		case HealPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_NpcHeal()
	{
		ClearWalkInput(g_xHealWalk);
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		if (g_bHealSkipped)
		{
			return true;
		}

		bool bPassed = g_bHealRaiseYesPassed
			&& g_bHealPromptPassed
			&& g_bHealWalkYesNoPassed
			&& g_bHealYesPassed
			&& g_bHealHealedLinePassed
			&& g_bHealRaiseNoPassed
			&& g_bHealNoPassed
			&& WalkPassed(g_xHealWalk);

		// --- the damage baselines were REAL (both passes) ---
		if (g_uHealDamagedYes == 0u || !g_bHealNeededYes
			|| !g_xHealPartyDamagedYes.m_bResolved
			|| g_xHealPartyDamagedYes.m_uCount == 0u
			|| g_xHealPartyDamagedYes.m_uFullHpMembers != 0u)
		{
			bPassed = false;
		}
		// The m_uCount clause is REQUIRED for symmetry with the YES baseline above: on an
		// EMPTY party m_uFullHpMembers is 0 too, so without it this block passes on a
		// party that does not exist.
		if (g_uHealDamagedNo == 0u || !g_bHealNeededNo
			|| !g_xHealPartyDamagedNo.m_bResolved
			|| g_xHealPartyDamagedNo.m_uCount == 0u
			|| g_xHealPartyDamagedNo.m_uFullHpMembers != 0u)
		{
			bPassed = false;
		}

		// --- the AUTHORED choice widgets really render, and the QUESTION with them ---
		// A prompt awaiting its answer is IsActive() == false, so ZM_UI_DialogueBox
		// deliberately keeps the panel + line shown through `IsActive() ||
		// IsAwaitingChoice()`. A regression to a bare `bShown = IsActive()` would blank
		// the question and leave two labelled buttons floating over the world (the
		// ZM-D-112 bleed-through class) while every button-only assertion stayed green.
		if (!g_xHealChoiceOnAwait.m_bResolved
			|| !g_xHealChoiceOnAwait.m_bYesFound || !g_xHealChoiceOnAwait.m_bNoFound
			|| !g_xHealChoiceOnAwait.m_bYesVisible || !g_xHealChoiceOnAwait.m_bNoVisible
			|| !g_xHealChoiceOnAwait.m_bYesFocusable || !g_xHealChoiceOnAwait.m_bNoFocusable
			|| !g_xHealChoiceOnAwait.m_bPanelFound || !g_xHealChoiceOnAwait.m_bPanelVisible
			|| !g_xHealChoiceOnAwait.m_bTextFound || !g_xHealChoiceOnAwait.m_bTextVisible
			|| g_xHealChoiceOnAwait.m_strText.empty())
		{
			bPassed = false;
		}
		else
		{
			g_bHealChoicePassed = true;
		}

		// --- YES: the answer latched, the LIVE party is whole, and it SAID so ---
		if (g_eHealAnswerYes != (u_int)ZM_DIALOGUE_CHOICE_YES)
		{
			bPassed = false;
		}
		if (!g_xHealPartyAfterYes.m_bResolved
			|| g_xHealPartyAfterYes.m_uCount == 0u
			|| g_xHealPartyAfterYes.m_uFullHpMembers != g_xHealPartyAfterYes.m_uCount)
		{
			bPassed = false;
		}
		if (g_eHealTopAfterYes != (u_int)ZM_MENU_SCREEN_DIALOGUE
			|| !g_bHealHealedLineActive
			|| !g_xHealHealedLineElements.m_bPanelFound
			|| !g_xHealHealedLineElements.m_bTextFound
			|| !g_xHealHealedLineElements.m_bPanelVisible
			|| !g_xHealHealedLineElements.m_bTextVisible
			|| g_xHealHealedLineElements.m_strText != ZM_CareCenterHealedLine())
		{
			bPassed = false;
		}
		// ...and once it is read out the box comes down and the player moves again. The
		// PANEL + TEXT are what the CLOSE takes down (ZM_UI_DialogueBox::Hide), so they
		// are what is asserted hidden here. The button-visibility clause that used to sit
		// in this block was REMOVED: PresentChoiceButtons hides and un-focuses both
		// buttons on every frame IsAwaitingChoice() is false, which is true from the
		// instant ResolveChoice reset the box -- i.e. throughout the whole healed-line
		// phase, long BEFORE the menu closed -- so it could never fail for the reason it
		// claimed. The *Found flags stay: they prove the capture actually RAN, since a
		// default-initialised view would satisfy the "hidden" half vacuously.
		if (!g_bHealClosedAfterYes
			|| !g_xHealChoiceOnClose.m_bYesFound || !g_xHealChoiceOnClose.m_bNoFound
			|| !g_xHealChoiceOnClose.m_bPanelFound || !g_xHealChoiceOnClose.m_bTextFound
			|| g_xHealChoiceOnClose.m_bPanelVisible || g_xHealChoiceOnClose.m_bTextVisible
			|| !g_bHealMovementReenabled)
		{
			bPassed = false;
		}

		// --- NO: the answer latched, the menu closed, and the party is STILL damaged ---
		// This is what makes the two passes differ in OUTCOME, so neither can be vacuous.
		if (g_eHealAnswerNo != (u_int)ZM_DIALOGUE_CHOICE_NO || !g_bHealClosedAfterNo)
		{
			bPassed = false;
		}
		if (!g_xHealPartyAfterNo.m_bResolved
			|| g_xHealPartyAfterNo.m_uCount == 0u
			|| g_xHealPartyAfterNo.m_uFullHpMembers >= g_xHealPartyAfterNo.m_uCount)
		{
			bPassed = false;
		}

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_NpcHeal] %s", g_szHealFailure);
			LogWalkDiagnostics(g_xHealWalk, "ZM_NpcHeal");
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcHeal] raise: delta=%u (want 1) top=%u (want DIALOGUE=%u) depth=%u "
				"(want 1) frozen=%d action=%u (want HEAL=%u) awaiting=%d readPresses=%d",
				g_uHealRaiseDeltaYes, g_eHealTopOnOpen, (u_int)ZM_MENU_SCREEN_DIALOGUE,
				g_uHealDepthOnOpen, (int)g_bHealPlayerFrozen,
				g_eHealActionOnOpen, (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY,
				(int)g_bHealAwaitingChoice, g_iHealReadPresses);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcHeal] widgets onAwait: resolved=%d panel=%d/%d text=%d/%d line='%s' "
				"yes=%d/%d/%d no=%d/%d/%d focus='%s' (found/visible/focusable)",
				(int)g_xHealChoiceOnAwait.m_bResolved,
				(int)g_xHealChoiceOnAwait.m_bPanelFound, (int)g_xHealChoiceOnAwait.m_bPanelVisible,
				(int)g_xHealChoiceOnAwait.m_bTextFound, (int)g_xHealChoiceOnAwait.m_bTextVisible,
				g_xHealChoiceOnAwait.m_strText.c_str(),
				(int)g_xHealChoiceOnAwait.m_bYesFound, (int)g_xHealChoiceOnAwait.m_bYesVisible,
				(int)g_xHealChoiceOnAwait.m_bYesFocusable,
				(int)g_xHealChoiceOnAwait.m_bNoFound, (int)g_xHealChoiceOnAwait.m_bNoVisible,
				(int)g_xHealChoiceOnAwait.m_bNoFocusable,
				g_xHealChoiceOnAwait.m_strFocusedName.c_str());
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcHeal] YES: damaged=%u needed=%d leadHp %u/%u full=%u/%u -> answer=%u "
				"(want YES=%u) leadHp %u/%u full=%u/%u reachedNo=%d backToYes=%d lastFocus='%s'",
				g_uHealDamagedYes, (int)g_bHealNeededYes,
				g_xHealPartyDamagedYes.m_uLeadCurrentHp, g_xHealPartyDamagedYes.m_uLeadMaxHp,
				g_xHealPartyDamagedYes.m_uFullHpMembers, g_xHealPartyDamagedYes.m_uCount,
				g_eHealAnswerYes, (u_int)ZM_DIALOGUE_CHOICE_YES,
				g_xHealPartyAfterYes.m_uLeadCurrentHp, g_xHealPartyAfterYes.m_uLeadMaxHp,
				g_xHealPartyAfterYes.m_uFullHpMembers, g_xHealPartyAfterYes.m_uCount,
				(int)g_bHealReachedNo, (int)g_bHealReturnedToYes,
				g_strHealWalkFocusName.c_str());
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcHeal] healed line: top=%u (want DIALOGUE=%u) active=%d panel=%d/%d "
				"text=%d/%d text='%s' (want '%s') presses=%d closed=%d movReenabled=%d "
				"buttonsOnClose yes=%d/%d no=%d/%d",
				g_eHealTopAfterYes, (u_int)ZM_MENU_SCREEN_DIALOGUE,
				(int)g_bHealHealedLineActive,
				(int)g_xHealHealedLineElements.m_bPanelFound,
				(int)g_xHealHealedLineElements.m_bPanelVisible,
				(int)g_xHealHealedLineElements.m_bTextFound,
				(int)g_xHealHealedLineElements.m_bTextVisible,
				g_xHealHealedLineElements.m_strText.c_str(), ZM_CareCenterHealedLine(),
				g_iHealHealedLinePresses, (int)g_bHealClosedAfterYes,
				(int)g_bHealMovementReenabled,
				(int)g_xHealChoiceOnClose.m_bYesFound, (int)g_xHealChoiceOnClose.m_bYesVisible,
				(int)g_xHealChoiceOnClose.m_bNoFound, (int)g_xHealChoiceOnClose.m_bNoVisible);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcHeal] NO: damaged=%u needed=%d raiseDelta=%u top=%u awaiting=%d "
				"focusedNo=%d answer=%u (want NO=%u) closed=%d leadHp %u/%u full=%u/%u",
				g_uHealDamagedNo, (int)g_bHealNeededNo, g_uHealRaiseDeltaNo,
				g_eHealTopOnOpenNo, (int)g_bHealAwaitingChoiceNo,
				(int)g_bHealFocusedNoOnConfirm,
				g_eHealAnswerNo, (u_int)ZM_DIALOGUE_CHOICE_NO,
				(int)g_bHealClosedAfterNo,
				g_xHealPartyAfterNo.m_uLeadCurrentHp, g_xHealPartyAfterNo.m_uLeadMaxHp,
				g_xHealPartyAfterNo.m_uFullHpMembers, g_xHealPartyAfterNo.m_uCount);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcHeal] phase flags: raiseYes=%d prompt=%d choice=%d walkYesNo=%d yes=%d "
				"healedLine=%d raiseNo=%d no=%d",
				(int)g_bHealRaiseYesPassed, (int)g_bHealPromptPassed, (int)g_bHealChoicePassed,
				(int)g_bHealWalkYesNoPassed, (int)g_bHealYesPassed,
				(int)g_bHealHealedLinePassed, (int)g_bHealRaiseNoPassed, (int)g_bHealNoPassed);
		}
		return bPassed;
	}

	// ========================================================================
	// ZM_S6InteractGate_Test (S6 item 3 SC7) -- the CONSOLIDATED S6 GATE.
	//
	// ONE uninterrupted Dawnmere session, ONE scene load, NO between-beats reset,
	// proving all four clauses of the S6 gate sentence THROUGH THE NPCs:
	//
	//   talk  -- walk to Npc_Villager, press E, read and close a conversation whose
	//            FIRST LINE IS THE ZM_NpcData ROW'S OWN, and end unfrozen.
	//   buy   -- walk to Npc_TradePostClerk, press E, reach Confirm by real Down
	//            edges, buy, and the LIVE purse falls by exactly
	//            ZM_ShopBuyPrice(item) x quantity while the bag rises by exactly
	//            quantity. Escape out.
	//   heal  -- damage the party (ZM_PartyNeedsHealing TRUE), walk to Npc_Caretaker,
	//            press E, arrow onto Yes, ENTER, GetLastDialogueAnswer() reads YES,
	//            the party is whole and the healed line is on screen. Close.
	//   menus -- press the menu key, then Party / Bag / Dex / Exit purely by real
	//            arrow / ENTER / ESCAPE edges, each sub-screen backed out to ROOT and
	//            Exit finally emptying the stack.
	//
	// ...then a CLEAN EXIT beat: the menu is closed, the player is unfrozen, and
	// holding W AGAIN produces a non-zero GetRequestedSpeed() -- the session left the
	// player playable.
	//
	// WHAT THIS ADDS OVER EVERYTHING ALREADY GREEN. ZM_S6UIGate_Test (item 2 SC9)
	// walks the same four clauses in one session, but raises talk / buy / heal through
	// the STATIC seams (TryPushDialogue / TryOpenShop / TryOpenCareCenterPrompt), so
	// it cannot see an NPC that is missing, mute, unreachable, or wired to the wrong
	// seam. ZM_NpcTalk_Test / ZM_NpcShop_Test / ZM_NpcHeal_Test each walk up to ONE
	// NPC in their OWN session, so none of them can see one beat's leftovers poison
	// the next. This test is the intersection: EVERY raise here comes from
	// SimulateKeyPress(ZENITH_KEY_E) against an authored NPC -- the test never calls
	// TryPushDialogue / TryOpenShop / TryOpenCareCenterPrompt -- and every beat runs
	// on top of whatever the previous beat left behind.
	//
	// THE TWO NEGATIVES (the S6 gate criteria require BOTH of every walk-up test):
	//   1. OUT OF RANGE -- run ONCE at the start by the shared walk machine, against
	//      the villager, before any approach.
	//   2. RE-RAISE -- with the conversation OPEN, press E again. See GatePhase::
	//      TalkRePress for which half of that negative is load-bearing and which half
	//      is structurally incapable of failing.
	//
	// EVERY BLEED ASSERTION IS PAIRED WITH A POPULATION ASSERTION (ZM-D-122, the item-2
	// SC9 entry, records that its review found two assertions that could not fail --
	// that is the decision this rule comes from). The box is asserted to hold the
	// villager's REAL line before it is asserted empty; the answer latch is asserted
	// NONE before the heal and YES after; the mart's inventory is asserted non-empty
	// before it is asserted dropped; and the interaction target is asserted to CHANGE
	// villager -> clerk -> caretaker, each against an entity resolved by NAME on that
	// same frame.
	// ========================================================================

	// The three sub-screens the ROOT pause menu pushes, in visit order. EXIT is NOT in
	// this table: confirming it closes the whole menu rather than pushing a screen, so
	// it gets its own two phases instead of an arm full of special cases.
	struct GateMenuVisit
	{
		ZM_MENU_ROOT_ITEM m_eRootItem;
		ZM_MENU_SCREEN    m_eScreen;
		const char*       m_szPanelName;   // that screen's OWN authored panel
		const char*       m_szLabel;       // for the diagnostics only
	};

	constexpr u_int uGATE_VISIT_COUNT = 3u;
	const GateMenuVisit axGATE_VISITS[uGATE_VISIT_COUNT] = {
		// ⚠ VISIT 0's entry IS the ROOT default focus: PresentTopScreen parks the canvas
		// focus on RootItemElementName(ZM_MENU_ROOT_PARTY) whenever no ROOT entry holds it
		// (ZM_UI_MenuStack.cpp), which is the state on a fresh open AND on every return
		// from a sub-screen. So THIS row's m_bReachedEntry is free -- it is earned by no
		// key edge at all. Only visits 1..N and the Exit walk are earned by real edges,
		// and only the nav probe below exercises UP.
		{ ZM_MENU_ROOT_PARTY, ZM_MENU_SCREEN_PARTY, ZM_UI_Party::szPANEL_NAME, "Party" },
		{ ZM_MENU_ROOT_BAG,   ZM_MENU_SCREEN_BAG,   ZM_UI_Bag::szPANEL_NAME,   "Bag"   },
		{ ZM_MENU_ROOT_DEX,   ZM_MENU_SCREEN_DEX,   ZM_UI_Dex::szPANEL_NAME,   "Dex"   },
	};

	// The ROOT entry the nav probe walks DOWN to before the first visit: the genuinely
	// LAST ROOT entry, NOT the last row of the visit table. Exit is never CONFIRMED
	// there -- only walked onto.
	constexpr ZM_MENU_ROOT_ITEM eGATE_NAV_PROBE_ITEM = ZM_MENU_ROOT_EXIT;

	// Per-visit observations. ALL DEFAULT TO FAILING.
	struct GateVisitResult
	{
		// Arrow edges walked the ROOT focus onto the entry -- EXCEPT for visit 0, whose
		// entry is where PresentTopScreen re-parks the focus anyway (see axGATE_VISITS):
		// that one is structurally true and the ProbeNavDown / ProbeNavUp pair is what
		// actually proves both directions of the focus-nav.
		bool m_bReachedEntry   = false;
		bool m_bScreenOpened   = false;   // ...and ONE Enter made that screen the top one
		bool m_bPanelFound     = false;   // the screen's OWN authored panel resolved by name
		bool m_bPanelVisible   = false;   // ...and is actually drawn
		bool m_bReturnedToRoot = false;   // ONE Escape put ROOT back on top
	};

	enum class GatePhase
	{
		TalkWalk,           // the shared machine: boot, both basis legs, the out-of-range
		                    //   negative, the approach, the event-driven E press
		TalkRaise,          // the villager's DIALOGUE is up, carrying the ROW's own line
		TalkRePress,        // negative 2: press E again with the box open
		TalkRead,           // spaced Enter edges until the conversation closes
		TalkSettle,         // ...unfrozen, and the box is genuinely EMPTY again
		ShopRetarget,       // point the SAME walk context at the clerk
		ShopWalk,
		ShopRaise,
		ShopWalkToConfirm,  // spaced Down edges -- the ENGINE spatial nav reaches Confirm
		ShopBuy,            // ONE Enter on the selected entry
		ShopClose,          // spaced Escape edges until the mart closes
		ShopSettle,         // ...the mart is no longer top, and it dropped its stock
		HealArm,            // damage the LIVE party + capture the PRE-answer latch
		HealRetarget,       // ...and point the walk context at the caretaker
		HealWalk,
		HealRaise,
		HealRead,           // spaced Enter edges until the box awaits the answer
		HealWalkToNo,       // real Right edges...
		HealWalkToYes,      // ...and real Left edges back, so the Yes below is EARNED
		HealConfirm,        // ONE Enter on Yes
		HealClose,          // spaced Enter edges read the healed line out, then it closes
		MenuOpen,           // the menu key opens ROOT
		ProbeNavDown,       // DOWN edges to the LAST ROOT entry...
		ProbeNavUp,         // ...and UP edges back to the first, so BOTH directions are
		                    //   proved before the visits (which only ever go DOWN)
		MenuVisitWalk,      // arrow edges onto the current visit's ROOT entry -- FREE for
		                    //   visit 0, whose entry is the ROOT default focus
		MenuVisitConfirm,   // ONE Enter -> that screen is top and its own panel is shown
		MenuVisitEscape,    // ONE Escape -> ROOT is back on top
		MenuExitWalk,       // arrow edges onto Exit...
		MenuExitConfirm,    // ...and ONE Enter empties the stack
		CleanExitHold,      // hold W again...
		CleanExitAssert,    // ...and the player is demonstrably playable
		Done,
	};

	WalkContext g_xGateWalk;
	GatePhase   g_eGatePhase = GatePhase::Done;
	int         g_iGatePhaseFrames = 0;
	bool        g_bGateSkipped = false;
	bool        g_bGatePrereqsPresent = false;
	const char* g_szGateFailure = "test did not reach verification";

	// ---- Phase flags. EVERY ONE DEFAULTS TO FAILING, so a phase that never runs
	//      fails the test rather than silently passing it. ----
	bool g_bGateTalkRaisePassed    = false;
	bool g_bGateTalkLinePassed     = false;   // the box holds the ROW's own first line
	bool g_bGateTalkRePressPassed  = false;   // negative 2
	bool g_bGateTalkRePressLinePassed = false;   // ...and its ONE press-sensitive clause
	bool g_bGateTalkClosePassed    = false;
	bool g_bGateTalkBoxClearPassed = false;   // ...and the box is EMPTY afterwards
	bool g_bGateShopRetargetPassed = false;
	bool g_bGateShopRaisePassed    = false;
	bool g_bGateShopStockPassed    = false;
	bool g_bGateShopConfirmPassed  = false;
	bool g_bGateShopBuyPassed      = false;
	bool g_bGateShopClosePassed    = false;
	bool g_bGateShopSettlePassed   = false;
	bool g_bGateHealArmPassed      = false;
	bool g_bGateHealRetargetPassed = false;
	bool g_bGateHealRaisePassed    = false;
	bool g_bGateHealPromptPassed   = false;
	bool g_bGateHealReachedNo      = false;
	bool g_bGateHealReturnedToYes  = false;
	bool g_bGateHealAnswerPassed   = false;
	bool g_bGateHealPartyPassed    = false;
	bool g_bGateHealLinePassed     = false;
	bool g_bGateHealClosePassed    = false;
	bool g_bGateMenuOpenPassed     = false;
	bool g_bGateProbeReachedLast     = false;   // DOWN edges walked ROOT focus to the last entry
	bool g_bGateProbeReturnedToFirst = false;   // ...and UP edges walked it back to the first
	bool g_bGateMenuVisitsPassed   = false;
	bool g_bGateMenuExitPassed     = false;
	bool g_bGateCleanExitPassed    = false;
	bool g_bGateTargetsDistinct    = false;   // villager -> clerk -> caretaker, BY ID

	// ---- talk beat observations ----
	u_int       g_uGateTalkRaiseDelta = 0u;                          // want exactly 1
	u_int       g_eGateTalkTop        = (u_int)ZM_MENU_SCREEN_NONE;  // want DIALOGUE
	u_int       g_uGateTalkDepth      = 0u;                          // want 1
	bool        g_bGateTalkFrozen     = false;
	std::string g_strGateTalkLine;          // what the box is showing...
	std::string g_strGateTalkExpectedLine;  // ...and the villager row's own line 0
	// negative 2
	u_int g_eGateRePressReject     = (u_int)ZM_INTERACT_REJECT_COUNT;  // want MENU_OPEN
	u_int g_uGateRePressRaiseDelta = 99u;                              // want 0
	u_int g_eGateRePressTop        = (u_int)ZM_MENU_SCREEN_NONE;       // want DIALOGUE
	// The conversation's own read cursor, side by side across the press. THE ONLY pair
	// in that phase the press can move (see the phase comment). The defaults DIFFER on
	// both halves, so a phase that never ran cannot read as "unchanged".
	// Counts the interact edges the re-raise negative actually emitted. Asserted equal to
	// uiGATE_REPRESS_COUNT so a phase that silently emitted none cannot pass.
	int         g_iGateRePressEdges = 0;
	u_int       g_uGateRePressLineIdxBefore = 99u;
	u_int       g_uGateRePressLineIdxAfter  = 98u;
	std::string g_strGateRePressLineBefore;
	std::string g_strGateRePressLineAfter   = "<not sampled>";
	// the close
	bool  g_bGateTalkMovementReenabled = false;
	u_int g_uGateTalkBoxLinesAfter     = 99u;    // want 0
	bool  g_bGateTalkBoxActiveAfter    = true;   // want false
	bool  g_bGateTalkBoxArmedAfter     = true;   // want false

	// ---- buy beat observations ----
	u_int       g_uGateShopRaiseDelta   = 0u;
	u_int       g_eGateShopTop          = (u_int)ZM_MENU_SCREEN_NONE;   // want SHOP
	u_int       g_uGateShopDepth        = 0u;                           // want 1
	bool        g_bGateShopFrozen       = false;
	u_int       g_uGateShopRowStock     = 0u;    // the clerk row's own count
	u_int       g_uGateShopScreenStock  = 0u;    // ...and what the mart is listing
	int         g_iGateShopMismatchIdx  = -1;
	std::string g_strGateShopFocusName;
	u_int       g_eGateShopBoughtItem   = (u_int)ZM_ITEM_NONE;
	u_int       g_uGateShopPrice        = 0u;
	u_int       g_uGateShopQuantity     = 0u;
	u_int       g_uGateShopMoneyBefore  = 0u;
	u_int       g_uGateShopMoneyAfter   = 0u;
	u_int       g_uGateShopHeldBefore   = 0u;
	u_int       g_uGateShopHeldAfter    = 0u;
	u_int       g_eGateShopResult       = (u_int)ZM_SHOP_RESULT_COUNT;  // want ZM_SHOP_OK
	bool        g_bGateShopReportedResult = false;
	// DIAGNOSTIC ONLY -- structurally NONE once ShopClose has advanced (see ShopSettle).
	u_int       g_eGateShopTopAfterClose  = (u_int)ZM_MENU_SCREEN_SHOP;
	u_int       g_uGateShopStockAfterClose = 99u;                       // want 0
	u_int       g_uGateShopDepthAfterClose = 99u;                       // want 0
	bool        g_bGateShopResultLatchAfterClose = true;                // want false

	// ---- heal beat observations ----
	u_int         g_uGateHealDamaged      = 0u;
	bool          g_bGateHealNeeded       = false;
	HealPartyView g_xGateHealPartyDamaged;
	u_int         g_eGateAnswerBeforeHeal = (u_int)ZM_DIALOGUE_CHOICE_YES;  // want NONE
	u_int         g_uGateHealRaiseDelta   = 0u;
	u_int         g_eGateHealTop          = (u_int)ZM_MENU_SCREEN_NONE;     // want DIALOGUE
	u_int         g_uGateHealDepth        = 0u;                             // want 1
	u_int         g_eGateHealAction       = (u_int)ZM_DIALOGUE_ACTION_NONE; // want HEAL_PARTY
	bool          g_bGateHealFrozen       = false;
	bool          g_bGateHealAwaiting     = false;
	HealChoiceView g_xGateHealChoiceOnAwait;
	std::string   g_strGateHealFocusName;
	bool          g_bGateHealChoiceResolved = false;
	u_int         g_eGateAnswerAfterHeal    = (u_int)ZM_DIALOGUE_CHOICE_NONE;  // want YES
	HealPartyView g_xGateHealPartyAfter;
	u_int         g_eGateHealTopAfterYes    = (u_int)ZM_MENU_SCREEN_NONE;      // want DIALOGUE
	HealChoiceView g_xGateHealedLineElements;
	bool          g_bGateHealMovementReenabled = false;

	// ---- the three interaction targets, BY ENTITY ID ----
	Zenith_EntityID g_xGateTalkLatchedTarget = INVALID_ENTITY_ID;
	Zenith_EntityID g_xGateTalkNamedTarget   = INVALID_ENTITY_ID;
	Zenith_EntityID g_xGateShopLatchedTarget = INVALID_ENTITY_ID;
	Zenith_EntityID g_xGateShopNamedTarget   = INVALID_ENTITY_ID;
	Zenith_EntityID g_xGateHealLatchedTarget = INVALID_ENTITY_ID;
	Zenith_EntityID g_xGateHealNamedTarget   = INVALID_ENTITY_ID;

	// ---- menu beat observations ----
	u_int           g_uGateVisitIndex = 0u;
	GateVisitResult g_axGateVisits[uGATE_VISIT_COUNT];
	bool            g_bGateExitReached   = false;
	bool            g_bGateMenuClosed    = false;
	u_int           g_uGateDepthAfterExit = 99u;   // want 0

	// ---- clean-exit beat ----
	bool  g_bGateFinalClosed          = false;
	bool  g_bGateFinalFocusCleared    = false;
	bool  g_bGateFinalMovementEnabled = false;
	float g_fGateFinalRequestedSpeed  = 0.0f;      // want > 0

	void FailGate(const char* szReason)
	{
		g_szGateFailure = szReason;
		g_eGatePhase = GatePhase::Done;
		ClearWalkInput(g_xGateWalk);
	}

	// Walk the ROOT focus onto one ROOT entry with REAL arrow edges, one every fourth
	// frame, direction chosen from where the focus currently is. Returns true the
	// frame the focus is already there. An unfocused / non-ROOT element reads as -1,
	// which sends the walk DOWN -- the ROOT list re-parks the focus on its first entry
	// whenever a non-entry holds it, so DOWN always converges.
	// SetFocusedElement appears NOWHERE: parking the focus programmatically would pass
	// with the engine focus navigation completely broken.
	bool StepGateRootFocusWalk(int iTargetItem, int iPhaseFrames)
	{
		const std::string strFocus = ReadMenuFocusName();
		const int iFocused = ZM_UI_MenuStack::RootItemIndexFromElementName(strFocus.c_str());
		if (iFocused == iTargetItem)
		{
			return true;
		}
		if ((iPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(
				(iFocused > iTargetItem) ? ZENITH_KEY_UP : ZENITH_KEY_DOWN);
		}
		return false;
	}

	void Setup_S6InteractGate()
	{
		g_xGateWalk = WalkContext{};
		// The walk STARTS at the villager. Boot, both basis legs and the out-of-range
		// negative therefore run EXACTLY ONCE, here; the clerk and the caretaker are
		// reached later by RetargetWalk, which re-enters the approach only.
		g_xGateWalk.m_szTargetEntityName = szVILLAGER_ENTITY_NAME;
		g_xGateWalk.m_pfnAfterBoot = nullptr;

		g_eGatePhase = GatePhase::Done;
		g_iGatePhaseFrames = 0;
		g_bGateSkipped = false;
		g_bGatePrereqsPresent = false;
		g_szGateFailure = "test did not reach verification";

		g_bGateTalkRaisePassed = false;
		g_bGateTalkLinePassed = false;
		g_bGateTalkRePressPassed = false;
		g_bGateTalkRePressLinePassed = false;
		g_bGateTalkClosePassed = false;
		g_bGateTalkBoxClearPassed = false;
		g_bGateShopRetargetPassed = false;
		g_bGateShopRaisePassed = false;
		g_bGateShopStockPassed = false;
		g_bGateShopConfirmPassed = false;
		g_bGateShopBuyPassed = false;
		g_bGateShopClosePassed = false;
		g_bGateShopSettlePassed = false;
		g_bGateHealArmPassed = false;
		g_bGateHealRetargetPassed = false;
		g_bGateHealRaisePassed = false;
		g_bGateHealPromptPassed = false;
		g_bGateHealReachedNo = false;
		g_bGateHealReturnedToYes = false;
		g_bGateHealAnswerPassed = false;
		g_bGateHealPartyPassed = false;
		g_bGateHealLinePassed = false;
		g_bGateHealClosePassed = false;
		g_bGateMenuOpenPassed = false;
		g_bGateProbeReachedLast = false;
		g_bGateProbeReturnedToFirst = false;
		g_bGateMenuVisitsPassed = false;
		g_bGateMenuExitPassed = false;
		g_bGateCleanExitPassed = false;
		g_bGateTargetsDistinct = false;

		g_uGateTalkRaiseDelta = 0u;
		g_eGateTalkTop = (u_int)ZM_MENU_SCREEN_NONE;
		g_uGateTalkDepth = 0u;
		g_bGateTalkFrozen = false;
		g_strGateTalkLine.clear();
		g_strGateTalkExpectedLine.clear();
		g_eGateRePressReject = (u_int)ZM_INTERACT_REJECT_COUNT;
		g_uGateRePressRaiseDelta = 99u;
		g_eGateRePressTop = (u_int)ZM_MENU_SCREEN_NONE;
		g_iGateRePressEdges = 0;
		g_uGateRePressLineIdxBefore = 99u;
		g_uGateRePressLineIdxAfter = 98u;
		g_strGateRePressLineBefore.clear();
		g_strGateRePressLineAfter = "<not sampled>";
		g_bGateTalkMovementReenabled = false;
		g_uGateTalkBoxLinesAfter = 99u;
		g_bGateTalkBoxActiveAfter = true;
		g_bGateTalkBoxArmedAfter = true;

		g_uGateShopRaiseDelta = 0u;
		g_eGateShopTop = (u_int)ZM_MENU_SCREEN_NONE;
		g_uGateShopDepth = 0u;
		g_bGateShopFrozen = false;
		g_uGateShopRowStock = 0u;
		g_uGateShopScreenStock = 0u;
		g_iGateShopMismatchIdx = -1;
		g_strGateShopFocusName.clear();
		g_eGateShopBoughtItem = (u_int)ZM_ITEM_NONE;
		g_uGateShopPrice = 0u;
		g_uGateShopQuantity = 0u;
		g_uGateShopMoneyBefore = 0u;
		g_uGateShopMoneyAfter = 0u;
		g_uGateShopHeldBefore = 0u;
		g_uGateShopHeldAfter = 0u;
		g_eGateShopResult = (u_int)ZM_SHOP_RESULT_COUNT;
		g_bGateShopReportedResult = false;
		g_eGateShopTopAfterClose = (u_int)ZM_MENU_SCREEN_SHOP;
		g_uGateShopStockAfterClose = 99u;
		g_uGateShopDepthAfterClose = 99u;
		g_bGateShopResultLatchAfterClose = true;

		g_uGateHealDamaged = 0u;
		g_bGateHealNeeded = false;
		g_xGateHealPartyDamaged = HealPartyView{};
		g_eGateAnswerBeforeHeal = (u_int)ZM_DIALOGUE_CHOICE_YES;
		g_uGateHealRaiseDelta = 0u;
		g_eGateHealTop = (u_int)ZM_MENU_SCREEN_NONE;
		g_uGateHealDepth = 0u;
		g_eGateHealAction = (u_int)ZM_DIALOGUE_ACTION_NONE;
		g_bGateHealFrozen = false;
		g_bGateHealAwaiting = false;
		g_xGateHealChoiceOnAwait = HealChoiceView{};
		g_strGateHealFocusName.clear();
		g_bGateHealChoiceResolved = false;
		g_eGateAnswerAfterHeal = (u_int)ZM_DIALOGUE_CHOICE_NONE;
		g_xGateHealPartyAfter = HealPartyView{};
		g_eGateHealTopAfterYes = (u_int)ZM_MENU_SCREEN_NONE;
		g_xGateHealedLineElements = HealChoiceView{};
		g_bGateHealMovementReenabled = false;

		g_xGateTalkLatchedTarget = INVALID_ENTITY_ID;
		g_xGateTalkNamedTarget = INVALID_ENTITY_ID;
		g_xGateShopLatchedTarget = INVALID_ENTITY_ID;
		g_xGateShopNamedTarget = INVALID_ENTITY_ID;
		g_xGateHealLatchedTarget = INVALID_ENTITY_ID;
		g_xGateHealNamedTarget = INVALID_ENTITY_ID;

		g_uGateVisitIndex = 0u;
		for (u_int u = 0u; u < uGATE_VISIT_COUNT; ++u)
		{
			g_axGateVisits[u] = GateVisitResult{};
		}
		g_bGateExitReached = false;
		g_bGateMenuClosed = false;
		g_uGateDepthAfterExit = 99u;

		g_bGateFinalClosed = false;
		g_bGateFinalFocusCleared = false;
		g_bGateFinalMovementEnabled = false;
		g_fGateFinalRequestedSpeed = 0.0f;

		Zenith_InputSimulator::ResetAllInputState();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// ---- The ONLY two skips (see ZM_NpcShop's Setup for the guard-order rule:
		// RequestSkip BYPASSES Verify, so NO process state -- fixed dt, scene load --
		// may be installed until both prerequisites are known present). A missing,
		// unconfigured, unreachable or MUTE NPC must FAIL, never skip. ----
		g_bGatePrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bGatePrereqsPresent)
		{
			g_bGateSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_S6InteractGate] the Dawnmere scene / terrain bake is absent or incomplete "
				"-- there is no world to walk through (run a *_True config once to bake it)");
			return;
		}
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bGateSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_S6InteractGate] no persistent ZM_MenuRoot / ZM_UI_MenuStack singleton -- "
				"FrontEnd.zscen has not been baked, so there is no screen for any NPC to raise");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fNPC_FIXED_DT);
		g_xGateWalk.m_eStage = WalkStage::Boot;
		g_eGatePhase = GatePhase::TalkWalk;
		// The ONLY scene load in this test. Everything from here to teardown happens in
		// ONE uninterrupted session -- that is the whole point of the gate.
		g_xEngine.Scenes().LoadSceneByIndex(
			iNPC_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	// The three-way TickWalk branch, shared by the gate's three walk phases so the
	// legs cannot drift apart. Returns false when the caller must stop stepping.
	bool StepGateWalkLeg(GatePhase eOnPressed)
	{
		const WalkTick eTick = TickWalk(g_xGateWalk);
		if (eTick == WalkTick::Failed)
		{
			FailGate(g_xGateWalk.m_szFailure);
			return false;
		}
		if (eTick == WalkTick::Pressed)
		{
			g_eGatePhase = eOnPressed;
			g_iGatePhaseFrames = 0;
		}
		return true;
	}

	// The raise assertions every beat shares: EXACTLY one more screen, latched OK, and
	// the latched target IS the entity this frame's NAME lookup resolved. The named /
	// latched pair is written out so Verify can prove the target CHANGED across the
	// three beats -- an "unchanged" or "still the villager" bug is otherwise invisible
	// to any single-NPC test.
	bool AssertGateRaise(
		const char* szEntityName,
		Zenith_EntityID& xLatchedOut,
		Zenith_EntityID& xNamedOut,
		u_int& uRaiseDeltaOut,
		bool& bFrozenOut,
		const char* szWrongEntityFailure)
	{
		NpcPlayerView xPlayer;
		if (!FindActiveNpcPlayer(xPlayer))
		{
			FailGate("the player disappeared before a raise could be asserted");
			return false;
		}
		const ZM_InteractionRuntime& xRuntime =
			xPlayer.m_pxController->GetInteractionRuntime();
		uRaiseDeltaOut = xRuntime.GetRaiseCount() - g_xGateWalk.m_uRaiseCountBefore;
		xLatchedOut = xRuntime.GetLastTarget();

		Zenith_Maths::Vector3 xNamedPosition(0.0f);
		bool bNamedInteractable = false;
		if (!ResolveNpcByName(szEntityName, xNamedOut, xNamedPosition, bNamedInteractable))
		{
			FailGate("the authored NPC this beat pressed E at no longer resolves by name");
			return false;
		}
		if (xRuntime.GetRaiseCount() != g_xGateWalk.m_uRaiseCountBefore + 1u)
		{
			FailGate("the walk-up press did not raise EXACTLY one screen");
			return false;
		}
		if (xRuntime.GetLastResult() != ZM_INTERACT_OK)
		{
			FailGate("the walk-up press did not latch ZM_INTERACT_OK");
			return false;
		}
		if (xLatchedOut != xNamedOut)
		{
			FailGate(szWrongEntityFailure);
			return false;
		}
		// IMPLIED by the raise-delta check above under the CURRENT seam -- GetRaiseCount
		// only moves when Interact() returned true, i.e. the push succeeded -- and kept
		// as a forward guard against a raise that counts without opening anything.
		if (!ZM_UI_MenuStack::IsMenuOpen())
		{
			FailGate("the NPC's raise did not open the menu stack");
			return false;
		}
		// The freeze, against the baseline the walk captured while the player was
		// demonstrably movable -- without it the pair is vacuous on a player who was
		// frozen the whole session.
		bFrozenOut = !xPlayer.m_pxController->IsMovementEnabled();
		if (!g_xGateWalk.m_bMovementEnabledBefore)
		{
			FailGate("the player was not movable before the walk -- every freeze / unfreeze "
				"assertion in this session would be vacuous");
			return false;
		}
		if (!bFrozenOut)
		{
			FailGate("the player was NOT frozen while an NPC's screen was up");
			return false;
		}
		return true;
	}

	bool Step_S6InteractGate(int)
	{
		if (g_eGatePhase == GatePhase::Done)
		{
			return false;
		}
		++g_iGatePhaseFrames;

		switch (g_eGatePhase)
		{
		// ==================================================================
		// BEAT 1 -- TALK. Walk to the villager and press E.
		// ==================================================================
		case GatePhase::TalkWalk:
			return StepGateWalkLeg(GatePhase::TalkRaise);

		case GatePhase::TalkRaise:
		{
			if (!AssertGateRaise(szVILLAGER_ENTITY_NAME,
				g_xGateTalkLatchedTarget, g_xGateTalkNamedTarget,
				g_uGateTalkRaiseDelta, g_bGateTalkFrozen,
				"the raised interaction named an entity that is not the villager"))
			{
				return false;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_MenuRoot singleton vanished between Setup and the talk raise");
				return false;
			}
			g_eGateTalkTop = (u_int)pxMenu->GetTopScreen();
			g_uGateTalkDepth = pxMenu->GetDepth();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailGate("a TALKER NPC raised something other than the DIALOGUE screen");
				return false;
			}
			if (pxMenu->GetDepth() != 1u)
			{
				FailGate("the conversation came up at a stack depth other than 1 -- an NPC "
					"must be talkable with NO pause menu underneath");
				return false;
			}

			// ★ RULING L, POPULATION HALF. The box must be showing the VILLAGER ROW'S
			// OWN first line, read out of ZM_GetNpcData at RUNTIME rather than
			// re-spelled here. This is what makes the "the box is empty again" bleed
			// assertion in TalkSettle non-vacuous -- without it, "empty" would be
			// comparing nothing against nothing.
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_VILLAGER);
			if (xRow.m_uLineCount == 0u || xRow.m_paszLines == nullptr
				|| xRow.m_paszLines[0] == nullptr)
			{
				FailGate("the villager's ZM_NpcData row carries NO lines -- the conversation "
					"content assertion below would be vacuous");
				return false;
			}
			g_strGateTalkExpectedLine = xRow.m_paszLines[0];
			g_strGateTalkLine = pxMenu->GetDialogue().GetCurrentLine();
			if (!pxMenu->GetDialogue().IsActive()
				|| g_strGateTalkLine != g_strGateTalkExpectedLine)
			{
				FailGate("the open conversation is not showing the villager row's own first "
					"line -- the NPC's authored content did not reach the screen");
				return false;
			}
			g_bGateTalkLinePassed = true;

			g_bGateTalkRaisePassed = true;
			g_eGatePhase = GatePhase::TalkRePress;
			g_iGatePhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// NEGATIVE 2 -- THE RE-RAISE. Press E again with the conversation OPEN.
		//
		// ★ HONEST ACCOUNTING OF WHAT EACH CLAUSE HERE PROVES.
		//   * The EvaluateForTests(...) == MENU_OPEN check is PRESS-INDEPENDENT: the seam
		//     hard-codes bInteractPressed = true (ZM_InteractionRuntime.cpp:99-103, which
		//     says so in as many words -- reading the real edge there would make it
		//     report NO_INPUT_EDGE on every polling frame and be useless for deciding
		//     WHEN to press). It would answer MENU_OPEN on this frame whether or not E
		//     was ever pressed. It is therefore NOT the press proof -- it is the proof
		//     that the blocker PRECEDENCE is right, i.e. that MENU_OPEN outranks
		//     PLAYER_FROZEN in ZM_ShouldInteract.
		//   * The raise-count-unchanged check is dominated by the PLAYER FREEZE and cannot
		//     fail on its own: raising a screen freezes the player, and
		//     ZM_PlayerController::OnUpdate early-outs on !m_bMovementEnabled STRICTLY
		//     BEFORE the m_xInteraction.Tick() call site, so with a box open the runtime
		//     cannot tick at all. It is a re-raise-LOOP guard, not a menu-open proof.
		//   * The line-index / line-text-unchanged pair below is THE ONLY clause in this
		//     phase that the live press can move. It reds if E ever enters
		//     ZM_CONFIRM_KEYS or the dialogue starts consuming the interact key.
		//
		// ★ WHY THE INTERACT KEY IS PRESSED uiGATE_REPRESS_COUNT TIMES, NOT ONCE.
		// MEASURED, not assumed: a single confirm on a dialogue whose typewriter is still
		// running only COMPLETES THE REVEAL (ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL) -- it
		// does NOT advance the line. A one-press version of this negative was mutation-
		// tested by swapping the key for ZENITH_KEY_ENTER (a real confirm key) and it
		// STILL PASSED, because the single confirm was absorbed by the reveal and the line
		// index never moved. One press therefore cannot detect a confirming interact key
		// at all. Six spaced presses can: the villager row carries THREE lines, so six
		// confirms would complete the reveal, read all three lines and CLOSE the box --
		// which reds the line index, the line text, the open-ness and the top screen
		// together. Pressing the interact key repeatedly with a box open is also a
		// strictly stronger negative in its own right: it is the re-raise LOOP the
		// menu-open blocker exists to stop.
		// ------------------------------------------------------------------
		case GatePhase::TalkRePress:
		{
			// Spaced one-shot edges after the baseline frame, so each is a clean edge.
			if (g_iGatePhaseFrames > iPRESS_FRAME
				&& g_iGatePhaseFrames < iPRESS_FRAME + (int)(uiGATE_REPRESS_COUNT * 4u)
				&& ((g_iGatePhaseFrames - iPRESS_FRAME) % 4) == 0)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
				++g_iGateRePressEdges;
			}
			if (g_iGatePhaseFrames == iPRESS_FRAME)
			{
				NpcPlayerView xPlayer;
				if (!FindActiveNpcPlayer(xPlayer))
				{
					FailGate("the player disappeared before the re-raise negative");
					return false;
				}
				ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
				if (pxMenu == nullptr)
				{
					FailGate("the ZM_UI_MenuStack singleton stopped resolving before the "
						"re-raise negative");
					return false;
				}
				// The PRESS-SENSITIVE baseline (see the phase comment above): where the
				// conversation's read cursor is, and what it is showing, on the frame BEFORE
				// the interact key goes down.
				g_uGateRePressLineIdxBefore = pxMenu->GetDialogue().GetCurrentLineIndex();
				g_strGateRePressLineBefore = pxMenu->GetDialogue().GetCurrentLine();
				g_xGateWalk.m_uRaiseCountBefore =
					xPlayer.m_pxController->GetInteractionRuntime().GetRaiseCount();
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
				++g_iGateRePressEdges;
				return true;
			}
			// Settle only AFTER every spaced edge has been emitted and consumed.
			if (g_iGatePhaseFrames < iPRESS_FRAME + (int)(uiGATE_REPRESS_COUNT * 4u) + iSETTLE_FRAME)
			{
				return true;
			}

			NpcPlayerView xPlayer;
			if (!FindActiveNpcPlayer(xPlayer))
			{
				FailGate("the player disappeared during the re-raise negative");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			g_eGateRePressReject = (u_int)xRuntime.EvaluateForTests(xSeamTarget);
			g_uGateRePressRaiseDelta =
				xRuntime.GetRaiseCount() - g_xGateWalk.m_uRaiseCountBefore;

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving during the re-raise "
					"negative");
				return false;
			}
			g_eGateRePressTop = (u_int)pxMenu->GetTopScreen();
			g_uGateRePressLineIdxAfter = pxMenu->GetDialogue().GetCurrentLineIndex();
			g_strGateRePressLineAfter = pxMenu->GetDialogue().GetCurrentLine();

			// The PRECEDENCE check (press-independent -- see the phase comment).
			if (g_eGateRePressReject != (u_int)ZM_INTERACT_REJECT_MENU_OPEN)
			{
				FailGate("pressing E with a conversation already open was NOT refused with "
					"ZM_INTERACT_REJECT_MENU_OPEN -- the interaction gate no longer blocks on "
					"an open menu, so a second NPC screen could stack over the first");
				return false;
			}
			if (g_uGateRePressRaiseDelta != 0u)
			{
				FailGate("pressing E with a conversation already open RAISED another screen");
				return false;
			}
			if (g_eGateRePressTop != (u_int)ZM_MENU_SCREEN_DIALOGUE)
			{
				FailGate("the conversation stopped being the top screen while E was pressed "
					"against it");
				return false;
			}
			// The press proof is only a proof if the presses HAPPENED. Without this, a
			// phase whose edge-emitting arm stopped firing would satisfy every
			// "unchanged" clause below by pressing nothing at all -- the exact vacuity
			// this whole phase exists to avoid.
			if (g_iGateRePressEdges != (int)uiGATE_REPRESS_COUNT)
			{
				FailGate("the re-raise negative did not emit the interact edges it is "
					"supposed to emit, so its 'nothing moved' clauses would prove nothing "
					"(the emitted count is logged below)");
				return false;
			}
			// THE press proof: the ONLY clause here the live E edges can move. Six of them,
			// because ONE is absorbed by the typewriter reveal and moves nothing -- a
			// one-press version of this check was mutation-tested with ZENITH_KEY_ENTER
			// and still passed.
			if (g_uGateRePressLineIdxAfter != g_uGateRePressLineIdxBefore
				|| g_strGateRePressLineAfter != g_strGateRePressLineBefore)
			{
				FailGate("pressing E repeatedly with a conversation already open ADVANCED it "
					"-- the interact key is being consumed as a CONFIRM, so E can no longer "
					"be held while reading (the line index and text either side of the "
					"presses are logged below)");
				return false;
			}
			g_bGateTalkRePressLinePassed = true;

			g_bGateTalkRePressPassed = true;
			g_eGatePhase = GatePhase::TalkRead;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::TalkRead:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving while reading the "
					"conversation");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_eGatePhase = GatePhase::TalkSettle;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iREAD_DEADLINE)
			{
				FailGate("the conversation never closed after the Enter edges");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case GatePhase::TalkSettle:
		{
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			NpcPlayerView xPlayer;
			if (pxMenu == nullptr || !FindActiveNpcPlayer(xPlayer))
			{
				FailGate("the menu singleton or the player stopped resolving after the "
					"conversation closed");
				return false;
			}
			g_bGateTalkMovementReenabled = xPlayer.m_pxController->IsMovementEnabled();
			if (!g_bGateTalkMovementReenabled)
			{
				FailGate("the closed conversation left the player FROZEN -- a soft-lock");
				return false;
			}
			g_bGateTalkClosePassed = true;

			// ★ RULING L, BLEED HALF. The box that TalkRaise proved was holding the
			// villager's real line must now be EMPTY. It is asserted here rather than
			// at the heal beat because everything between the two would inherit the
			// leftovers: OpenCareCenterPrompt refuses outright while the box is active
			// or a choice is armed, so a surviving queue would make the heal raise
			// silently impossible several hundred frames later.
			const ZM_UI_DialogueBox& xBox = pxMenu->GetDialogue();
			g_uGateTalkBoxLinesAfter = xBox.GetQueuedLineCount();
			g_bGateTalkBoxActiveAfter = xBox.IsActive();
			g_bGateTalkBoxArmedAfter = xBox.IsChoiceArmed();
			if (g_uGateTalkBoxLinesAfter != 0u || g_bGateTalkBoxActiveAfter
				|| g_bGateTalkBoxArmedAfter)
			{
				FailGate("the read-out conversation left content in the dialogue box -- the "
					"NEXT NPC's raise would be refused (a prompt owns the box outright)");
				return false;
			}
			g_bGateTalkBoxClearPassed = true;

			g_eGatePhase = GatePhase::ShopRetarget;
			g_iGatePhaseFrames = 0;
			return true;
		}

		// ==================================================================
		// BEAT 2 -- BUY. Re-target the SAME walk context at the clerk and walk.
		// ==================================================================
		case GatePhase::ShopRetarget:
		{
			RetargetWalk(g_xGateWalk, szCLERK_ENTITY_NAME);
			if (g_xGateWalk.m_eStage == WalkStage::Failed)
			{
				FailGate(g_xGateWalk.m_szFailure);
				return false;
			}
			g_bGateShopRetargetPassed = true;
			g_eGatePhase = GatePhase::ShopWalk;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::ShopWalk:
			return StepGateWalkLeg(GatePhase::ShopRaise);

		case GatePhase::ShopRaise:
		{
			if (!AssertGateRaise(szCLERK_ENTITY_NAME,
				g_xGateShopLatchedTarget, g_xGateShopNamedTarget,
				g_uGateShopRaiseDelta, g_bGateShopFrozen,
				"the raised interaction named an entity that is not the Trade Post clerk"))
			{
				return false;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_MenuRoot singleton vanished before the mart raise");
				return false;
			}
			g_eGateShopTop = (u_int)pxMenu->GetTopScreen();
			g_uGateShopDepth = pxMenu->GetDepth();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_SHOP)
			{
				FailGate("a SHOPKEEP NPC raised something other than the SHOP screen");
				return false;
			}
			if (pxMenu->GetDepth() != 1u)
			{
				FailGate("the mart came up at a stack depth other than 1 -- a shop must be "
					"reachable by talking to its clerk, with NO pause menu underneath");
				return false;
			}

			// ★ RULING L, POPULATION HALF for the mart. NON-EMPTY first, and the
			// clerk's OWN row, read at runtime -- a mart listing nothing would satisfy
			// every "the shop dropped its stock" assertion in ShopSettle for free.
			const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_TRADE_POST_CLERK);
			g_uGateShopRowStock = xRow.m_uStockCount;
			g_uGateShopScreenStock = xShop.GetInventoryCount();
			if (xRow.m_uStockCount == 0u || xRow.m_paeStock == nullptr)
			{
				FailGate("the clerk's ZM_NpcData row carries NO stock -- every mart assertion "
					"in this beat would be vacuous");
				return false;
			}
			if (g_uGateShopScreenStock != xRow.m_uStockCount)
			{
				FailGate("the open mart is listing a different NUMBER of entries than the "
					"clerk's own row stocks");
				return false;
			}
			for (u_int u = 0u; u < xRow.m_uStockCount; ++u)
			{
				if (xShop.GetInventoryItem(u) != xRow.m_paeStock[u])
				{
					g_iGateShopMismatchIdx = (int)u;
					FailGate("the open mart is NOT showing the clerk's own stock -- the NPC's "
						"row did not reach the screen");
					return false;
				}
			}
			if (xShop.GetMode() != ZM_SHOP_MODE_BUY)
			{
				FailGate("the clerk's mart did not open in BUY mode -- the purchase assertions "
					"below would be measuring a SALE");
				return false;
			}

			g_bGateShopRaisePassed = true;
			g_bGateShopStockPassed = true;
			g_eGatePhase = GatePhase::ShopWalkToConfirm;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::ShopWalkToConfirm:
		{
			Zenith_UIComponent* pxUI = ResolveMenuRootUI();
			if (pxUI == nullptr)
			{
				FailGate("the ZM_MenuRoot UI component stopped resolving while walking to "
					"the mart's Confirm control");
				return false;
			}
			Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
			if (pxFocused != nullptr && pxFocused->GetName() == ZM_UI_Shop::szCONFIRM_NAME)
			{
				g_strGateShopFocusName = pxFocused->GetName();
				g_bGateShopConfirmPassed = true;
				g_eGatePhase = GatePhase::ShopBuy;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iSHOP_WALK_DEADLINE)
			{
				g_strGateShopFocusName =
					(pxFocused != nullptr) ? pxFocused->GetName() : std::string();
				FailGate("the Down edges never walked the focus onto the mart's Confirm "
					"control -- the clerk's mart cannot be transacted with by keyboard");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			return true;
		}

		case GatePhase::ShopBuy:
		{
			if (g_iGatePhaseFrames == iPRESS_FRAME)
			{
				ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
				if (pxMenu == nullptr)
				{
					FailGate("the ZM_UI_MenuStack singleton stopped resolving before the purchase");
					return false;
				}
				const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
				g_uGateShopQuantity = xShop.GetQuantity();

				ZM_GameState* pxState = nullptr;
				if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
				{
					FailGate("the live game state did not resolve before the purchase");
					return false;
				}
				// The FLAT entry index resolved BY THE SCREEN -- never the raw cursor,
				// which is a PAGE-RELATIVE row.
				const int iEntry = xShop.GetSelectedEntryIndex(pxState->m_xBag);
				const ZM_ITEM_ID eItem = (iEntry >= 0)
					? xShop.GetInventoryItem((u_int)iEntry)
					: ZM_ITEM_NONE;
				g_eGateShopBoughtItem = (u_int)eItem;
				g_uGateShopPrice = ZM_ShopBuyPrice(eItem);   // the TABLE price, never a literal
				if (!ReadLiveMoneyAndHeld(eItem,
					g_uGateShopMoneyBefore, g_uGateShopHeldBefore))
				{
					FailGate("the live game state did not resolve before the purchase");
					return false;
				}
				if (g_uGateShopMoneyBefore < g_uGateShopPrice * g_uGateShopQuantity)
				{
					FailGate("the mart's selected entry costs more than the LIVE purse -- the "
						"purchase assertions cannot run (the clerk's stock row or the starting "
						"money changed; prices, quantity and money are logged below)");
					return false;
				}

				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving after the purchase");
				return false;
			}
			g_eGateShopResult = (u_int)pxMenu->GetShopScreen().GetLastResult();
			g_bGateShopReportedResult = pxMenu->GetShopScreen().HasResult();
			if (!ReadLiveMoneyAndHeld((ZM_ITEM_ID)g_eGateShopBoughtItem,
				g_uGateShopMoneyAfter, g_uGateShopHeldAfter))
			{
				FailGate("the live game state did not resolve after the purchase");
				return false;
			}

			g_bGateShopBuyPassed = true;
			g_eGatePhase = GatePhase::ShopClose;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::ShopClose:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving while closing the mart");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bGateShopClosePassed = true;
				g_eGatePhase = GatePhase::ShopSettle;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iSHOP_CLOSE_DEADLINE)
			{
				FailGate("the mart never closed after the Escape edges");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			}
			return true;
		}

		case GatePhase::ShopSettle:
		{
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			NpcPlayerView xPlayer;
			if (pxMenu == nullptr || !FindActiveNpcPlayer(xPlayer))
			{
				FailGate("the menu singleton or the player stopped resolving after the mart "
					"closed");
				return false;
			}
			if (!xPlayer.m_pxController->IsMovementEnabled())
			{
				FailGate("the closed mart left the player FROZEN -- a soft-lock");
				return false;
			}
			// ★ RULING L, BLEED HALF for the mart. ShopRaise proved this screen was
			// carrying six real entries, so the counts below can genuinely differ from
			// what was observed there.
			// The TOP SCREEN is captured for the DIAGNOSTIC ONLY: ShopClose advances only
			// on !IsOpen(), IsOpen() is !m_xStack.IsEmpty(), and Top() returns
			// ZM_MENU_SCREEN_NONE at depth 0 -- so "the mart is no longer top" is true BY
			// CONSTRUCTION here and no CloseMenu / Reset change could ever red it.
			// What the close CAN get wrong, and what is asserted instead: the stack must
			// actually be EMPTY, and the shop's per-session result latch -- the thing
			// m_xShop.Reset() exists to clear -- must be gone, or the NEXT mart opens
			// reporting a previous session's purchase in its header. The INVENTORY and the
			// HasResult latch are the real bleed proofs.
			g_eGateShopTopAfterClose = (u_int)pxMenu->GetTopScreen();
			g_uGateShopStockAfterClose = pxMenu->GetShopScreen().GetInventoryCount();
			g_uGateShopDepthAfterClose = pxMenu->GetDepth();
			g_bGateShopResultLatchAfterClose = pxMenu->GetShopScreen().HasResult();
			if (g_uGateShopDepthAfterClose != 0u)
			{
				FailGate("the closed mart left the menu stack non-empty");
				return false;
			}
			if (g_bGateShopResultLatchAfterClose)
			{
				FailGate("the closed mart kept its per-session RESULT latch -- the next mart "
					"in this session would open reporting a previous purchase");
				return false;
			}
			if (g_uGateShopStockAfterClose != 0u)
			{
				FailGate("the closed mart kept the clerk's stock -- the NEXT mart in this "
					"session would open carrying a previous clerk's inventory");
				return false;
			}
			g_bGateShopSettlePassed = true;

			g_eGatePhase = GatePhase::HealArm;
			g_iGatePhaseFrames = 0;
			return true;
		}

		// ==================================================================
		// BEAT 3 -- HEAL.
		// ==================================================================
		case GatePhase::HealArm:
		{
			if (!DamageLiveParty(g_uGateHealDamaged))
			{
				FailGate("the LIVE party could not be damaged -- every heal assertion "
					"downstream would be vacuous");
				return false;
			}
			g_xGateHealPartyDamaged = ReadLiveParty();
			if (!ReadPartyNeedsHealing(g_bGateHealNeeded))
			{
				FailGate("the live game state did not resolve while damaging the party");
				return false;
			}
			if (!g_bGateHealNeeded)
			{
				// ZM_ApplyCareCenterHeal returns FALSE when nothing needed healing, in
				// which case the menu stack never queues the healed line and the prompt
				// pops immediately -- the whole beat would prove nothing.
				FailGate("ZM_PartyNeedsHealing said the party needs nothing right after it "
					"was damaged -- the heal beat could not prove a heal");
				return false;
			}

			// ★ RULING L, the BEFORE half of the before/after answer pair. The HOST
			// latch must read NONE here: Setup cleared it, and nothing in the talk or
			// buy beats resolves a choice. Asserting only the YES afterwards would be
			// satisfied by a latch that was already YES.
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving while arming the heal");
				return false;
			}
			g_eGateAnswerBeforeHeal = (u_int)pxMenu->GetLastDialogueAnswer();
			if (g_eGateAnswerBeforeHeal != (u_int)ZM_DIALOGUE_CHOICE_NONE)
			{
				FailGate("a dialogue ANSWER was already latched before the Care Center prompt "
					"was ever raised -- the YES assertion below would be satisfiable without "
					"a press");
				return false;
			}

			g_bGateHealArmPassed = true;
			g_eGatePhase = GatePhase::HealRetarget;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::HealRetarget:
		{
			RetargetWalk(g_xGateWalk, szCARETAKER_ENTITY_NAME);
			if (g_xGateWalk.m_eStage == WalkStage::Failed)
			{
				FailGate(g_xGateWalk.m_szFailure);
				return false;
			}
			g_bGateHealRetargetPassed = true;
			g_eGatePhase = GatePhase::HealWalk;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::HealWalk:
			return StepGateWalkLeg(GatePhase::HealRaise);

		case GatePhase::HealRaise:
		{
			if (!AssertGateRaise(szCARETAKER_ENTITY_NAME,
				g_xGateHealLatchedTarget, g_xGateHealNamedTarget,
				g_uGateHealRaiseDelta, g_bGateHealFrozen,
				"the raised interaction named an entity that is not the caretaker"))
			{
				return false;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_MenuRoot singleton vanished before the Care Center raise");
				return false;
			}
			g_eGateHealTop = (u_int)pxMenu->GetTopScreen();
			g_uGateHealDepth = pxMenu->GetDepth();
			g_eGateHealAction = (u_int)pxMenu->GetPendingDialogueAction();
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailGate("a CARETAKER NPC raised something other than the DIALOGUE screen");
				return false;
			}
			if (pxMenu->GetDepth() != 1u)
			{
				FailGate("the Care Center prompt came up at a stack depth other than 1");
				return false;
			}
			// Without this the beat would pass on a caretaker wired to TryPushDialogue,
			// which would put lines on screen and heal nothing.
			if (pxMenu->GetPendingDialogueAction() != ZM_DIALOGUE_ACTION_HEAL_PARTY)
			{
				FailGate("the caretaker's raise did not arm the HEAL_PARTY action -- it "
					"raised a plain conversation, not the Care Center prompt");
				return false;
			}

			g_bGateHealRaisePassed = true;
			g_eGatePhase = GatePhase::HealRead;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::HealRead:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving while reading the prompt");
				return false;
			}
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				g_bGateHealAwaiting = true;
				g_xGateHealChoiceOnAwait = ReadHealChoiceElements();
				g_bGateHealPromptPassed = true;
				g_eGatePhase = GatePhase::HealWalkToNo;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (!pxMenu->IsOpen())
			{
				FailGate("the Care Center prompt CLOSED instead of awaiting an answer");
				return false;
			}
			if (g_iGatePhaseFrames > iREAD_DEADLINE)
			{
				FailGate("the Enter edges never read the prompt through to its question");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		// ------------------------------------------------------------------
		// The prompt raises with the focus ALREADY parked on Yes, so a bare "arrow
		// onto Yes" phase would transition on its first frame having emitted ZERO
		// arrow edges -- a navigation proof that passes with navigation dead. Walking
		// Yes -> No -> Yes forces real edges in both directions and proves BOTH
		// answers are key-reachable, which is strictly what the Yes below needs.
		// ------------------------------------------------------------------
		case GatePhase::HealWalkToNo:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szNO_NAME)
			{
				g_bGateHealReachedNo = true;
				g_strGateHealFocusName = strFocus;
				g_eGatePhase = GatePhase::HealWalkToYes;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iCHOICE_WALK_DEADLINE)
			{
				g_strGateHealFocusName = strFocus;
				FailGate("the Right edges never walked the prompt focus onto the No button");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_RIGHT);
			}
			return true;
		}

		case GatePhase::HealWalkToYes:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szYES_NAME)
			{
				g_bGateHealReturnedToYes = true;
				g_strGateHealFocusName = strFocus;
				g_eGatePhase = GatePhase::HealConfirm;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iCHOICE_WALK_DEADLINE)
			{
				g_strGateHealFocusName = strFocus;
				FailGate("the Left edges never walked the prompt focus back onto the Yes button");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_LEFT);
			}
			return true;
		}

		case GatePhase::HealConfirm:
		{
			if (g_iGatePhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving after the YES answer");
				return false;
			}
			// ★ THE DIALOGUE-ANSWER TRAP, half (b). The HOST latch is NOT per-test
			// state (CloseMenu deliberately never clears it, and ZM_MenuRoot is
			// DontDestroyOnLoad), so before trusting it, prove the press genuinely
			// RESOLVED the choice. NOT written as "capture before and require a
			// change" -- a prior test ending on YES would make that a false failure.
			// The before/after NONE -> YES pair this beat DOES assert is safe because
			// HealArm proved the latch was NONE at the start of THIS beat.
			g_bGateHealChoiceResolved = !pxMenu->IsDialogueAwaitingChoice();
			if (!g_bGateHealChoiceResolved)
			{
				FailGate("the Enter on Yes never resolved the armed choice -- the box is still "
					"awaiting an answer, so the latch read below would be a STALE one");
				return false;
			}
			// ★ THE DIALOGUE-ANSWER TRAP, half (a). Read the HOST latch, never
			// GetDialogue().GetChoice(): a prompt raised over an EMPTY stack pops to
			// empty on resolve, which CloseMenu()s and Reset()s the box and clears its
			// stored answer -- all inside ONE OnUpdate.
			g_eGateAnswerAfterHeal = (u_int)pxMenu->GetLastDialogueAnswer();
			g_xGateHealPartyAfter = ReadLiveParty();
			g_eGateHealTopAfterYes = (u_int)pxMenu->GetTopScreen();
			g_xGateHealedLineElements = ReadHealChoiceElements();
			if (g_eGateAnswerAfterHeal != (u_int)ZM_DIALOGUE_CHOICE_YES)
			{
				FailGate("the latched answer was not YES after Enter landed on the Yes button");
				return false;
			}
			g_bGateHealAnswerPassed = true;

			if (!g_xGateHealPartyAfter.m_bResolved
				|| g_xGateHealPartyAfter.m_uCount == 0u
				|| g_xGateHealPartyAfter.m_uFullHpMembers != g_xGateHealPartyAfter.m_uCount)
			{
				FailGate("the YES answer did not restore the LIVE party to full HP");
				return false;
			}
			g_bGateHealPartyPassed = true;

			// A heal that actually changed something is never silent: the menu stack
			// queues ZM_CareCenterHealedLine() onto the (reset, unarmed) box and does
			// NOT pop, so the DIALOGUE screen must still be up with that exact text.
			if (g_eGateHealTopAfterYes != (u_int)ZM_MENU_SCREEN_DIALOGUE
				|| !g_xGateHealedLineElements.m_bPanelVisible
				|| !g_xGateHealedLineElements.m_bTextVisible
				|| g_xGateHealedLineElements.m_strText != ZM_CareCenterHealedLine())
			{
				FailGate("the healed confirmation line never came up after the YES answer");
				return false;
			}
			g_bGateHealLinePassed = true;

			g_eGatePhase = GatePhase::HealClose;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::HealClose:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving on the healed line");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				NpcPlayerView xPlayer;
				if (!FindActiveNpcPlayer(xPlayer))
				{
					FailGate("the player disappeared as the Care Center prompt closed");
					return false;
				}
				g_bGateHealMovementReenabled = xPlayer.m_pxController->IsMovementEnabled();
				if (!g_bGateHealMovementReenabled)
				{
					FailGate("the closed Care Center prompt left the player FROZEN");
					return false;
				}
				g_bGateHealClosePassed = true;
				g_eGatePhase = GatePhase::MenuOpen;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iREAD_DEADLINE)
			{
				FailGate("the healed confirmation line never closed after the Enter edges");
				return false;
			}
			if ((g_iGatePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		// ==================================================================
		// BEAT 4 -- MENUS. The pause tree, entirely by real key edges, on top of
		// everything the three NPC beats left behind.
		// ==================================================================
		case GatePhase::MenuOpen:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving while opening the menu");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bGateMenuOpenPassed = true;
				g_eGatePhase = GatePhase::ProbeNavDown;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iGATE_OPEN_DEADLINE)
			{
				FailGate("the ROOT pause menu never opened on the menu key -- three NPC "
					"screens in the same session left the menu stack unable to open");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		// ------------------------------------------------------------------
		// THE NAV PROBE. DOWN edges to the LAST ROOT entry, then UP edges back to the
		// first. BOTH directions are load-bearing for the visits below, and a
		// one-directional walk would hide a broken NavigateUp entirely: PresentTopScreen
		// re-parks the ROOT focus on the FIRST entry before every visit and the visit
		// targets ASCEND, so the visit walks below emit DOWN edges and nothing else --
		// ProbeNavUp is the ONLY phase in this test that can make StepGateRootFocusWalk
		// emit a ZENITH_KEY_UP edge at all.
		// ------------------------------------------------------------------
		case GatePhase::ProbeNavDown:
		{
			if (StepGateRootFocusWalk((int)eGATE_NAV_PROBE_ITEM, g_iGatePhaseFrames))
			{
				g_bGateProbeReachedLast = true;
				g_eGatePhase = GatePhase::ProbeNavUp;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iGATE_ROOT_WALK_DEADLINE)
			{
				FailGate("the DOWN edges never walked the ROOT focus to the last entry");
				return false;
			}
			return true;
		}

		case GatePhase::ProbeNavUp:
		{
			if (StepGateRootFocusWalk((int)axGATE_VISITS[0].m_eRootItem, g_iGatePhaseFrames))
			{
				g_bGateProbeReturnedToFirst = true;
				g_eGatePhase = GatePhase::MenuVisitWalk;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iGATE_ROOT_WALK_DEADLINE)
			{
				FailGate("the UP edges never walked the ROOT focus back to the first entry");
				return false;
			}
			return true;
		}

		// Arrow edges onto the current visit's ROOT entry. NOTE that visit 0's entry is
		// exactly where PresentTopScreen re-parks the focus, so THAT visit's
		// m_bReachedEntry costs no key edge at all -- the probe pair above is what proves
		// the focus-nav works in both directions.
		case GatePhase::MenuVisitWalk:
		{
			if (g_uGateVisitIndex >= uGATE_VISIT_COUNT)
			{
				FailGate("the screen-visit index ran past the visit table");
				return false;
			}
			if (StepGateRootFocusWalk((int)axGATE_VISITS[g_uGateVisitIndex].m_eRootItem,
				g_iGatePhaseFrames))
			{
				g_axGateVisits[g_uGateVisitIndex].m_bReachedEntry = true;
				g_eGatePhase = GatePhase::MenuVisitConfirm;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iGATE_ROOT_WALK_DEADLINE)
			{
				FailGate("the arrow edges never walked the ROOT focus onto a menu entry");
				return false;
			}
			return true;
		}

		case GatePhase::MenuVisitConfirm:
		{
			if (g_iGatePhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving after a screen confirm");
				return false;
			}
			const GateMenuVisit& xVisit = axGATE_VISITS[g_uGateVisitIndex];
			GateVisitResult& xResult = g_axGateVisits[g_uGateVisitIndex];
			xResult.m_bScreenOpened = (pxMenu->GetTopScreen() == xVisit.m_eScreen);
			if (Zenith_UIComponent* pxUI = ResolveMenuRootUI())
			{
				if (Zenith_UI::Zenith_UIRect* pxPanel =
					pxUI->FindElement<Zenith_UI::Zenith_UIRect>(xVisit.m_szPanelName))
				{
					xResult.m_bPanelFound = true;
					xResult.m_bPanelVisible = pxPanel->IsVisible();
				}
			}
			if (!xResult.m_bScreenOpened)
			{
				FailGate("confirming a ROOT entry did not raise its screen");
				return false;
			}
			if (!xResult.m_bPanelFound || !xResult.m_bPanelVisible)
			{
				FailGate("a ROOT entry raised its screen but that screen's OWN authored panel "
					"is missing or not drawn");
				return false;
			}

			g_eGatePhase = GatePhase::MenuVisitEscape;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::MenuVisitEscape:
		{
			if (g_iGatePhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				return true;
			}
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving after a screen escape");
				return false;
			}
			GateVisitResult& xResult = g_axGateVisits[g_uGateVisitIndex];
			xResult.m_bReturnedToRoot =
				pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT;
			if (!xResult.m_bReturnedToRoot)
			{
				FailGate("Escape did not return the stack to ROOT after a screen visit");
				return false;
			}

			++g_uGateVisitIndex;
			g_eGatePhase = (g_uGateVisitIndex < uGATE_VISIT_COUNT)
				? GatePhase::MenuVisitWalk
				: GatePhase::MenuExitWalk;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::MenuExitWalk:
		{
			if (StepGateRootFocusWalk((int)ZM_MENU_ROOT_EXIT, g_iGatePhaseFrames))
			{
				g_bGateExitReached = true;
				g_eGatePhase = GatePhase::MenuExitConfirm;
				g_iGatePhaseFrames = 0;
				return true;
			}
			if (g_iGatePhaseFrames > iGATE_ROOT_WALK_DEADLINE)
			{
				FailGate("the arrow edges never walked the ROOT focus onto the Exit entry");
				return false;
			}
			return true;
		}

		case GatePhase::MenuExitConfirm:
		{
			if (g_iGatePhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iGatePhaseFrames < iSETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailGate("the ZM_UI_MenuStack singleton stopped resolving after the Exit confirm");
				return false;
			}
			g_bGateMenuClosed = !pxMenu->IsOpen();
			g_uGateDepthAfterExit = pxMenu->GetDepth();
			if (!g_bGateMenuClosed || g_uGateDepthAfterExit != 0u)
			{
				FailGate("confirming Exit did not empty the menu stack -- it is still open, "
					"or its DEPTH did not fall to 0");
				return false;
			}
			g_bGateMenuExitPassed = true;

			// Hold W for the clean-exit beat. Plain W, NO run modifier: the question is
			// whether the player responds to input at all after a full pass over the S6
			// surface, not how fast.
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			g_xGateWalk.m_abHeldKeys[0] = true;
			g_eGatePhase = GatePhase::CleanExitHold;
			g_iGatePhaseFrames = 0;
			return true;
		}

		// ==================================================================
		// CLEAN EXIT -- the session left the player PLAYABLE.
		// ==================================================================
		case GatePhase::CleanExitHold:
		{
			if (g_iGatePhaseFrames < iGATE_UNFREEZE_HOLD_FRAMES)
			{
				return true;
			}
			g_eGatePhase = GatePhase::CleanExitAssert;
			g_iGatePhaseFrames = 0;
			return true;
		}

		case GatePhase::CleanExitAssert:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			NpcPlayerView xPlayer;
			if (pxMenu == nullptr || !FindActiveNpcPlayer(xPlayer))
			{
				FailGate("the menu singleton or the player stopped resolving for the final "
					"clean-exit check");
				return false;
			}
			g_bGateFinalClosed = !pxMenu->IsOpen();
			g_bGateFinalFocusCleared = MenuFocusCleared();
			g_bGateFinalMovementEnabled = xPlayer.m_pxController->IsMovementEnabled();
			// Read BEFORE releasing: m_fRequestedSpeed is rebuilt from input every
			// OnUpdate (ResetMovementObservables zeroes it first), so it is only
			// non-zero on a frame where a held movement key actually reached the
			// controller through a player that is not frozen.
			g_fGateFinalRequestedSpeed = xPlayer.m_pxController->GetRequestedSpeed();
			ClearWalkInput(g_xGateWalk);

			if (!g_bGateFinalClosed || !g_bGateFinalFocusCleared
				|| !g_bGateFinalMovementEnabled || g_fGateFinalRequestedSpeed <= 0.0f)
			{
				FailGate("the session did not end PLAYABLE -- after talking, buying, healing "
					"and walking the whole pause tree, holding W must move the player again "
					"with the menu closed and the canvas focus idle");
				return false;
			}
			g_bGateCleanExitPassed = true;

			g_eGatePhase = GatePhase::Done;
			return false;
		}

		case GatePhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_S6InteractGate()
	{
		ClearWalkInput(g_xGateWalk);
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		if (g_bGateSkipped)
		{
			// The harness already recorded the skip + its reason in Setup.
			return true;
		}

		bool bPassed = g_bGateTalkRaisePassed
			&& g_bGateTalkLinePassed
			&& g_bGateTalkRePressPassed
			&& g_bGateTalkRePressLinePassed
			&& g_bGateTalkClosePassed
			&& g_bGateTalkBoxClearPassed
			&& g_bGateShopRetargetPassed
			&& g_bGateShopRaisePassed
			&& g_bGateShopStockPassed
			&& g_bGateShopConfirmPassed
			&& g_bGateShopBuyPassed
			&& g_bGateShopClosePassed
			&& g_bGateShopSettlePassed
			&& g_bGateHealArmPassed
			&& g_bGateHealRetargetPassed
			&& g_bGateHealRaisePassed
			&& g_bGateHealPromptPassed
			&& g_bGateHealReachedNo
			&& g_bGateHealReturnedToYes
			&& g_bGateHealAnswerPassed
			&& g_bGateHealPartyPassed
			&& g_bGateHealLinePassed
			&& g_bGateHealClosePassed
			&& g_bGateMenuOpenPassed
			&& g_bGateProbeReachedLast
			&& g_bGateProbeReturnedToFirst
			&& g_bGateMenuExitPassed
			&& g_bGateCleanExitPassed
			&& WalkPassed(g_xGateWalk);

		// --- the talk beat carried the ROW's own words, and gave them back ---
		if (g_strGateTalkExpectedLine.empty()
			|| g_strGateTalkLine != g_strGateTalkExpectedLine)
		{
			bPassed = false;
		}
		// --- negative 2 refused for the RIGHT reason ---
		if (g_eGateRePressReject != (u_int)ZM_INTERACT_REJECT_MENU_OPEN
			|| g_uGateRePressRaiseDelta != 0u)
		{
			bPassed = false;
		}

		// --- the purchase was anchored on a REAL, PRICED entry, then moved the LIVE
		//     purse and bag by EXACTLY the right amounts ---
		if (g_eGateShopBoughtItem >= (u_int)ZM_ITEM_COUNT || g_uGateShopPrice == 0u)
		{
			bPassed = false;
		}
		else
		{
			const u_int uTotal = g_uGateShopPrice * g_uGateShopQuantity;
			if (g_eGateShopResult != (u_int)ZM_SHOP_OK || !g_bGateShopReportedResult)
			{
				bPassed = false;
			}
			if (g_uGateShopMoneyBefore < uTotal
				|| g_uGateShopMoneyAfter != g_uGateShopMoneyBefore - uTotal)
			{
				bPassed = false;
			}
			if (g_uGateShopHeldAfter != g_uGateShopHeldBefore + g_uGateShopQuantity)
			{
				bPassed = false;
			}
		}

		// --- the damage baseline was REAL, so the restored party is a real change ---
		if (g_uGateHealDamaged == 0u || !g_bGateHealNeeded
			|| !g_xGateHealPartyDamaged.m_bResolved
			|| g_xGateHealPartyDamaged.m_uCount == 0u
			|| g_xGateHealPartyDamaged.m_uFullHpMembers != 0u)
		{
			bPassed = false;
		}
		// --- ...and the AUTHORED choice widgets really rendered the question ---
		if (!g_xGateHealChoiceOnAwait.m_bResolved
			|| !g_xGateHealChoiceOnAwait.m_bYesFound || !g_xGateHealChoiceOnAwait.m_bNoFound
			|| !g_xGateHealChoiceOnAwait.m_bYesVisible || !g_xGateHealChoiceOnAwait.m_bNoVisible
			|| !g_xGateHealChoiceOnAwait.m_bYesFocusable || !g_xGateHealChoiceOnAwait.m_bNoFocusable
			|| !g_xGateHealChoiceOnAwait.m_bPanelVisible
			|| !g_xGateHealChoiceOnAwait.m_bTextVisible
			|| g_xGateHealChoiceOnAwait.m_strText.empty())
		{
			bPassed = false;
		}
		// --- the answer latch moved NONE -> YES across the beat (a PAIR, not a bare
		//     after: a latch that was already YES cannot satisfy both halves) ---
		if (g_eGateAnswerBeforeHeal != (u_int)ZM_DIALOGUE_CHOICE_NONE
			|| !g_bGateHealChoiceResolved
			|| g_eGateAnswerAfterHeal != (u_int)ZM_DIALOGUE_CHOICE_YES)
		{
			bPassed = false;
		}

		// --- the mart was POPULATED and then genuinely dropped. The top-screen value is
		//     NOT restated here: it is structurally NONE once ShopClose advanced (see
		//     ShopSettle). The stack DEPTH and the per-session HasResult latch are what
		//     a bad close can actually get wrong. ---
		if (g_uGateShopRowStock == 0u
			|| g_uGateShopScreenStock != g_uGateShopRowStock
			|| g_iGateShopMismatchIdx >= 0
			|| g_uGateShopDepthAfterClose != 0u
			|| g_bGateShopResultLatchAfterClose
			|| g_uGateShopStockAfterClose != 0u)
		{
			bPassed = false;
		}

		// --- THE TARGET CHANGED, BY ENTITY ID, on every beat. Each latched id is
		//     compared to the entity that beat resolved BY NAME on the same frame, and
		//     the three are required to be MUTUALLY DISTINCT -- an interaction that
		//     kept naming the first NPC would satisfy every per-beat screen assertion
		//     (the screens are raised by the seam, not by the id) and only this fails.
		g_bGateTargetsDistinct =
			g_xGateTalkNamedTarget != INVALID_ENTITY_ID
			&& g_xGateShopNamedTarget != INVALID_ENTITY_ID
			&& g_xGateHealNamedTarget != INVALID_ENTITY_ID
			&& g_xGateTalkLatchedTarget == g_xGateTalkNamedTarget
			&& g_xGateShopLatchedTarget == g_xGateShopNamedTarget
			&& g_xGateHealLatchedTarget == g_xGateHealNamedTarget
			&& g_xGateTalkNamedTarget != g_xGateShopNamedTarget
			&& g_xGateShopNamedTarget != g_xGateHealNamedTarget
			&& g_xGateTalkNamedTarget != g_xGateHealNamedTarget;
		if (!g_bGateTargetsDistinct)
		{
			bPassed = false;
		}

		// --- every pause-menu round trip completed by real key edges ---
		{
			bool bAllVisits = true;
			for (u_int u = 0u; u < uGATE_VISIT_COUNT; ++u)
			{
				const GateVisitResult& xResult = g_axGateVisits[u];
				bAllVisits = bAllVisits
					&& xResult.m_bReachedEntry && xResult.m_bScreenOpened
					&& xResult.m_bPanelFound && xResult.m_bPanelVisible
					&& xResult.m_bReturnedToRoot;
			}
			g_bGateMenuVisitsPassed = bAllVisits && g_bGateExitReached && g_bGateMenuClosed;
			if (!g_bGateMenuVisitsPassed)
			{
				bPassed = false;
			}
		}

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_S6InteractGate] %s", g_szGateFailure);
			LogWalkDiagnostics(g_xGateWalk, "ZM_S6InteractGate");
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_S6InteractGate] talk: raiseDelta=%u (want 1) top=%u (want DIALOGUE=%u) "
				"depth=%u (want 1) frozen=%d line='%s' expected='%s' | rePress: reject=%u "
				"(want MENU_OPEN=%u) raiseDelta=%u (want 0) top=%u edges=%d (want %d) "
				"lineIdx %u->%u "
				"line '%s'->'%s' (both must be UNCHANGED) | close: movReenabled=%d "
				"boxLines=%u (want 0) boxActive=%d boxArmed=%d",
				g_uGateTalkRaiseDelta, g_eGateTalkTop, (u_int)ZM_MENU_SCREEN_DIALOGUE,
				g_uGateTalkDepth, (int)g_bGateTalkFrozen,
				g_strGateTalkLine.c_str(), g_strGateTalkExpectedLine.c_str(),
				g_eGateRePressReject, (u_int)ZM_INTERACT_REJECT_MENU_OPEN,
				g_uGateRePressRaiseDelta, g_eGateRePressTop,
				g_iGateRePressEdges, (int)uiGATE_REPRESS_COUNT,
				g_uGateRePressLineIdxBefore, g_uGateRePressLineIdxAfter,
				g_strGateRePressLineBefore.c_str(), g_strGateRePressLineAfter.c_str(),
				(int)g_bGateTalkMovementReenabled, g_uGateTalkBoxLinesAfter,
				(int)g_bGateTalkBoxActiveAfter, (int)g_bGateTalkBoxArmedAfter);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_S6InteractGate] buy: raiseDelta=%u top=%u (want SHOP=%u) depth=%u frozen=%d "
				"rowStock=%u screenStock=%u firstMismatch=%d focus='%s' item=%u ('%s') price=%u "
				"qty=%u money %u->%u held %u->%u result=%u (want OK=%u) reported=%d | afterClose "
				"top=%u (structural) stock=%u (want 0) depth=%u (want 0) hasResult=%d "
				"(want 0)",
				g_uGateShopRaiseDelta, g_eGateShopTop, (u_int)ZM_MENU_SCREEN_SHOP,
				g_uGateShopDepth, (int)g_bGateShopFrozen,
				g_uGateShopRowStock, g_uGateShopScreenStock, g_iGateShopMismatchIdx,
				g_strGateShopFocusName.c_str(),
				g_eGateShopBoughtItem, ZM_GetItemName((ZM_ITEM_ID)g_eGateShopBoughtItem),
				g_uGateShopPrice, g_uGateShopQuantity,
				g_uGateShopMoneyBefore, g_uGateShopMoneyAfter,
				g_uGateShopHeldBefore, g_uGateShopHeldAfter,
				g_eGateShopResult, (u_int)ZM_SHOP_OK, (int)g_bGateShopReportedResult,
				g_eGateShopTopAfterClose, g_uGateShopStockAfterClose,
				g_uGateShopDepthAfterClose, (int)g_bGateShopResultLatchAfterClose);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_S6InteractGate] heal: damaged=%u needed=%d leadHp %u/%u full=%u/%u "
				"answerBefore=%u (want NONE=%u) raiseDelta=%u top=%u depth=%u action=%u "
				"(want HEAL=%u) frozen=%d awaiting=%d reachedNo=%d backToYes=%d focus='%s' "
				"resolved=%d answerAfter=%u "
				"(want YES=%u) afterHeal full=%u/%u topAfterYes=%u healedText='%s' (want '%s') "
				"movReenabled=%d",
				g_uGateHealDamaged, (int)g_bGateHealNeeded,
				g_xGateHealPartyDamaged.m_uLeadCurrentHp, g_xGateHealPartyDamaged.m_uLeadMaxHp,
				g_xGateHealPartyDamaged.m_uFullHpMembers, g_xGateHealPartyDamaged.m_uCount,
				g_eGateAnswerBeforeHeal, (u_int)ZM_DIALOGUE_CHOICE_NONE,
				g_uGateHealRaiseDelta, g_eGateHealTop, g_uGateHealDepth,
				g_eGateHealAction, (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY,
				(int)g_bGateHealFrozen,
				(int)g_bGateHealAwaiting, (int)g_bGateHealReachedNo,
				(int)g_bGateHealReturnedToYes, g_strGateHealFocusName.c_str(),
				(int)g_bGateHealChoiceResolved,
				g_eGateAnswerAfterHeal, (u_int)ZM_DIALOGUE_CHOICE_YES,
				g_xGateHealPartyAfter.m_uFullHpMembers, g_xGateHealPartyAfter.m_uCount,
				g_eGateHealTopAfterYes, g_xGateHealedLineElements.m_strText.c_str(),
				ZM_CareCenterHealedLine(), (int)g_bGateHealMovementReenabled);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_S6InteractGate] targets (entity index, latched/named): talk %u/%u "
				"clerk %u/%u caretaker %u/%u distinct=%d",
				g_xGateTalkLatchedTarget.m_uIndex, g_xGateTalkNamedTarget.m_uIndex,
				g_xGateShopLatchedTarget.m_uIndex, g_xGateShopNamedTarget.m_uIndex,
				g_xGateHealLatchedTarget.m_uIndex, g_xGateHealNamedTarget.m_uIndex,
				(int)g_bGateTargetsDistinct);
			for (u_int u = 0u; u < uGATE_VISIT_COUNT; ++u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_S6InteractGate] menu visit %s: reachedEntry=%d screenOpened=%d "
					"panel '%s' found=%d visible=%d returnedToRoot=%d",
					axGATE_VISITS[u].m_szLabel,
					(int)g_axGateVisits[u].m_bReachedEntry,
					(int)g_axGateVisits[u].m_bScreenOpened,
					axGATE_VISITS[u].m_szPanelName,
					(int)g_axGateVisits[u].m_bPanelFound,
					(int)g_axGateVisits[u].m_bPanelVisible,
					(int)g_axGateVisits[u].m_bReturnedToRoot);
			}
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_S6InteractGate] menu: opened=%d navProbe down=%d up=%d visitIndex=%u "
				"exitReached=%d closed=%d "
				"depthAfterExit=%u (want 0) | cleanExit: closed=%d focusCleared=%d movable=%d "
				"requestedSpeed=%.3f (want > 0)",
				(int)g_bGateMenuOpenPassed,
				(int)g_bGateProbeReachedLast, (int)g_bGateProbeReturnedToFirst,
				g_uGateVisitIndex, (int)g_bGateExitReached,
				(int)g_bGateMenuClosed, g_uGateDepthAfterExit,
				(int)g_bGateFinalClosed, (int)g_bGateFinalFocusCleared,
				(int)g_bGateFinalMovementEnabled, g_fGateFinalRequestedSpeed);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_S6InteractGate] phase flags: talk raise=%d line=%d rePress=%d "
				"rePressLine=%d close=%d "
				"boxClear=%d | shop retarget=%d raise=%d stock=%d confirm=%d buy=%d close=%d "
				"settle=%d | heal arm=%d retarget=%d raise=%d prompt=%d no=%d yes=%d answer=%d "
				"party=%d line=%d close=%d | menu open=%d navDown=%d navUp=%d visits=%d exit=%d "
				"| cleanExit=%d targetsDistinct=%d",
				(int)g_bGateTalkRaisePassed, (int)g_bGateTalkLinePassed,
				(int)g_bGateTalkRePressPassed, (int)g_bGateTalkRePressLinePassed,
				(int)g_bGateTalkClosePassed,
				(int)g_bGateTalkBoxClearPassed,
				(int)g_bGateShopRetargetPassed, (int)g_bGateShopRaisePassed,
				(int)g_bGateShopStockPassed, (int)g_bGateShopConfirmPassed,
				(int)g_bGateShopBuyPassed, (int)g_bGateShopClosePassed,
				(int)g_bGateShopSettlePassed,
				(int)g_bGateHealArmPassed, (int)g_bGateHealRetargetPassed,
				(int)g_bGateHealRaisePassed, (int)g_bGateHealPromptPassed,
				(int)g_bGateHealReachedNo, (int)g_bGateHealReturnedToYes,
				(int)g_bGateHealAnswerPassed, (int)g_bGateHealPartyPassed,
				(int)g_bGateHealLinePassed, (int)g_bGateHealClosePassed,
				(int)g_bGateMenuOpenPassed,
				(int)g_bGateProbeReachedLast, (int)g_bGateProbeReturnedToFirst,
				(int)g_bGateMenuVisitsPassed,
				(int)g_bGateMenuExitPassed, (int)g_bGateCleanExitPassed,
				(int)g_bGateTargetsDistinct);
		}
		return bPassed;
	}

	// ========================================================================
	// ZM_NpcWander_Test -- SC8's authored-waypoint patrol, end to end.
	//
	// This deliberately lives beside WalkContext. The target MOVES while the
	// player approaches, so reusing the camera-relative closed loop is more
	// important here than in any stationary-NPC test. The test first watches the
	// wanderer from safely outside interaction range (proving its dynamic body is
	// moving without player contact), then walks to the LIVE target, talks, proves
	// it halts for the dialogue, closes the dialogue and proves it resumes.
	// ========================================================================

	constexpr int iWANDER_READY_DEADLINE_FRAMES   = 840;
	constexpr int iWANDER_OBSERVE_DEADLINE_FRAMES = 900;
	constexpr int iWANDER_HALT_FRAMES             = 30;
	constexpr int iWANDER_CLOSE_DEADLINE_FRAMES   = 480;
	constexpr int iWANDER_RESUME_DEADLINE_FRAMES  = 360;
	constexpr float fWANDER_MIN_OBSERVED_PATH      = 2.0f;
	constexpr float fWANDER_MIN_RESUME_PATH        = 0.2f;
	constexpr float fWANDER_MIN_BODY_SPEED         = 0.25f;
	constexpr float fWANDER_MAX_HALT_DRIFT         = 0.15f;
	constexpr float fWANDER_MAX_HALT_SPEED         = 0.15f;
	constexpr float fWANDER_MIN_MOTION_ALIGNMENT   = 0.80f;
	constexpr float fWANDER_MOTION_ABS_TOLERANCE   = 0.01f;
	constexpr float fWANDER_MOTION_REL_TOLERANCE   = 0.50f;
	constexpr u_int uWANDER_OBSERVE_COUPLED_SAMPLES = 8u;
	constexpr u_int uWANDER_RESUME_COUPLED_SAMPLES  = 6u;

	struct WandererView
	{
		Zenith_EntityID           m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3     m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_Interactable*           m_pxInteractable = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	bool ResolveWanderer(WandererView& xOut)
	{
		xOut = WandererView{};
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxData == nullptr)
		{
			return false;
		}

		Zenith_Entity xEntity = pxData->FindEntityByName(szWANDERER_ENTITY_NAME);
		if (!xEntity.IsValid())
		{
			return false;
		}

		xOut.m_pxInteractable = xEntity.TryGetComponent<ZM_Interactable>();
		xOut.m_pxCollider = xEntity.TryGetComponent<Zenith_ColliderComponent>();
		Zenith_TransformComponent* pxTransform =
			xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (xOut.m_pxInteractable == nullptr
			|| xOut.m_pxCollider == nullptr
			|| pxTransform == nullptr)
		{
			return false;
		}

		xOut.m_xEntityID = xEntity.GetEntityID();
		pxTransform->GetPosition(xOut.m_xPosition);
		return true;
	}

	bool ReadWandererVelocity(
		const WandererView& xView,
		Zenith_Maths::Vector3& xVelocityOut)
	{
		xVelocityOut = Zenith_Maths::Vector3(0.0f);
		if (xView.m_pxCollider == nullptr || !xView.m_pxCollider->HasValidBody())
		{
			return false;
		}
		xVelocityOut =
			g_xEngine.Physics().GetLinearVelocity(xView.m_pxCollider->GetBodyID());
		return true;
	}

	float PlanarSpeed(const Zenith_Maths::Vector3& xVelocity)
	{
		return std::sqrt(
			xVelocity.x * xVelocity.x + xVelocity.z * xVelocity.z);
	}

	struct CoupledMotionEvidence
	{
		bool  m_bMatched = false;
		float m_fDisplacement = 0.0f;
		float m_fVelocitySpeed = 0.0f;
		float m_fExpectedDisplacement = 0.0f;
		float m_fAlignment = -1.0f;
		float m_fMagnitudeError = 99999.0f;
	};

	CoupledMotionEvidence MatchDisplacementToVelocity(
		const Zenith_Maths::Vector3& xDelta,
		const Zenith_Maths::Vector3& xVelocity,
		float fDt)
	{
		CoupledMotionEvidence xEvidence;
		xEvidence.m_fDisplacement =
			std::sqrt(xDelta.x * xDelta.x + xDelta.z * xDelta.z);
		xEvidence.m_fVelocitySpeed = PlanarSpeed(xVelocity);
		if (xEvidence.m_fDisplacement <= 0.0001f
			|| xEvidence.m_fVelocitySpeed <= fWANDER_MIN_BODY_SPEED
			|| fDt <= 0.0f)
		{
			return xEvidence;
		}

		xEvidence.m_fAlignment =
			(xDelta.x * xVelocity.x + xDelta.z * xVelocity.z)
			/ (xEvidence.m_fDisplacement * xEvidence.m_fVelocitySpeed);
		xEvidence.m_fExpectedDisplacement = xEvidence.m_fVelocitySpeed * fDt;
		xEvidence.m_fMagnitudeError = std::fabs(
			xEvidence.m_fDisplacement - xEvidence.m_fExpectedDisplacement);
		const float fTolerance = fWANDER_MOTION_ABS_TOLERANCE
			+ xEvidence.m_fExpectedDisplacement * fWANDER_MOTION_REL_TOLERANCE;
		xEvidence.m_bMatched =
			xEvidence.m_fAlignment >= fWANDER_MIN_MOTION_ALIGNMENT
			&& xEvidence.m_fMagnitudeError <= fTolerance;
		return xEvidence;
	}

	// Step() samples before Scenes().Update(). Depending on whether a component
	// changed velocity on the preceding update, this frame's transform delta can
	// correspond to either the velocity sampled LAST Step or the velocity sampled
	// NOW. Compare both and accept the better real match. A teleport paired with an
	// unrelated non-zero velocity fails alignment and/or speed*dt magnitude.
	CoupledMotionEvidence CoupleMotionAcrossFrame(
		const Zenith_Maths::Vector3& xPreviousPosition,
		const Zenith_Maths::Vector3& xCurrentPosition,
		const Zenith_Maths::Vector3& xPreviousVelocity,
		const Zenith_Maths::Vector3& xCurrentVelocity)
	{
		const Zenith_Maths::Vector3 xDelta(
			xCurrentPosition.x - xPreviousPosition.x,
			0.0f,
			xCurrentPosition.z - xPreviousPosition.z);
		const CoupledMotionEvidence xPrevious =
			MatchDisplacementToVelocity(xDelta, xPreviousVelocity, fNPC_FIXED_DT);
		const CoupledMotionEvidence xCurrent =
			MatchDisplacementToVelocity(xDelta, xCurrentVelocity, fNPC_FIXED_DT);
		if (xPrevious.m_bMatched && xCurrent.m_bMatched)
		{
			return xPrevious.m_fMagnitudeError <= xCurrent.m_fMagnitudeError
				? xPrevious : xCurrent;
		}
		return xPrevious.m_bMatched ? xPrevious : xCurrent;
	}

	enum class WanderPhase
	{
		AwaitReady,
		ObserveWander,
		Walk,
		AssertRaise,
		AssertHalt,
		CloseDialogue,
		AssertResume,
		Done,
	};

	WanderPhase g_eWanderPhase = WanderPhase::Done;
	int g_iWanderPhaseFrames = 0;
	WalkContext g_xWanderWalk;
	bool g_bWanderSkipped = false;
	bool g_bWanderPrereqsPresent = false;
	const char* g_szWanderFailure = "test did not reach verification";

	// Every outcome starts false. A phase that never runs therefore fails Verify.
	bool g_bWanderAuthored = false;
	bool g_bWanderDynamicCapsule = false;
	bool g_bWanderObservedAwayFromPlayer = false;
	bool g_bWanderMoved = false;
	bool g_bWanderWaypointAdvanced = false;
	bool g_bWanderPhysicsDrivenSeen = false;
	bool g_bWanderMovingAtPress = false;
	bool g_bWanderWaypointStableAcrossPress = false;
	bool g_bWanderDialogueRaised = false;
	bool g_bWanderContentPassed = false;
	bool g_bWanderPlayerFrozen = false;
	bool g_bWanderHaltedDuringDialogue = false;
	bool g_bWanderDialogueClosed = false;
	bool g_bWanderPlayerUnfrozen = false;
	bool g_bWanderResumed = false;

	Zenith_EntityID g_xWanderNamedTarget = INVALID_ENTITY_ID;
	Zenith_EntityID g_xWanderLatchedTarget = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xWanderPreviousPosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xWanderPreviousVelocity = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xWanderHaltStart = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xWanderResumePrevious = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xWanderResumePreviousVelocity = Zenith_Maths::Vector3(0.0f);
	u_int g_uWanderInitialWaypoint = 99u;
	u_int g_uWanderLastWaypoint = 99u;
	u_int g_uWanderWaypointAtPress = 99u;
	u_int g_uWanderWaypointAfterPress = 99u;
	u_int g_uWanderRaiseDelta = 0u;
	u_int g_uWanderObserveConsecutiveCoupled = 0u;
	u_int g_uWanderObserveMaxConsecutiveCoupled = 0u;
	u_int g_uWanderResumeConsecutiveCoupled = 0u;
	u_int g_uWanderResumeMaxConsecutiveCoupled = 0u;
	u_int g_uWanderHaltMenuSamples = 0u;
	u_int g_uWanderCloseEnterEdges = 0u;
	float g_fWanderObservedPath = 0.0f;
	float g_fWanderObserveMinPlayerDistance = 99999.0f;
	float g_fWanderMaxObservedSpeed = 0.0f;
	float g_fWanderMaxHaltDrift = 0.0f;
	float g_fWanderMaxHaltSpeed = 0.0f;
	float g_fWanderResumePath = 0.0f;
	float g_fWanderMaxResumeSpeed = 0.0f;
	float g_fWanderPrePressSpeed = 0.0f;
	CoupledMotionEvidence g_xWanderLastObserveCoupling;
	CoupledMotionEvidence g_xWanderLatestWalkCoupling;
	CoupledMotionEvidence g_xWanderLastResumeCoupling;
	std::string g_strWanderActualLine;
	std::string g_strWanderExpectedLine;

	void FailWander(const char* szReason)
	{
		g_szWanderFailure = szReason;
		g_eWanderPhase = WanderPhase::Done;
		ClearWalkInput(g_xWanderWalk);
	}

	bool CanPressMovingWanderer()
	{
		WandererView xWanderer;
		Zenith_Maths::Vector3 xVelocity(0.0f);
		if (!ResolveWanderer(xWanderer)
			|| !ReadWandererVelocity(xWanderer, xVelocity))
		{
			g_bWanderMovingAtPress = false;
			g_uWanderWaypointAtPress = 99u;
			return false;
		}

		g_fWanderPrePressSpeed = PlanarSpeed(xVelocity);
		g_bWanderMovingAtPress =
			g_xWanderLatestWalkCoupling.m_bMatched
			&& g_fWanderPrePressSpeed > fWANDER_MIN_BODY_SPEED;
		if (g_bWanderMovingAtPress)
		{
			// TickWalk injects E immediately after this gate returns true, so this
			// is the live cursor on the causal pre-press sample.
			g_uWanderWaypointAtPress =
				xWanderer.m_pxInteractable->GetWaypointIndex();
		}
		return g_bWanderMovingAtPress;
	}

	void Setup_NpcWander()
	{
		g_xWanderWalk = WalkContext{};
		g_xWanderWalk.m_szTargetEntityName = szWANDERER_ENTITY_NAME;
		g_xWanderWalk.m_pfnAfterBoot = nullptr;
		g_xWanderWalk.m_pfnCanPress = &CanPressMovingWanderer;
		g_eWanderPhase = WanderPhase::Done;
		g_iWanderPhaseFrames = 0;
		g_bWanderSkipped = false;
		g_bWanderPrereqsPresent = false;
		g_szWanderFailure = "test did not reach verification";

		g_bWanderAuthored = false;
		g_bWanderDynamicCapsule = false;
		g_bWanderObservedAwayFromPlayer = false;
		g_bWanderMoved = false;
		g_bWanderWaypointAdvanced = false;
		g_bWanderPhysicsDrivenSeen = false;
		g_bWanderMovingAtPress = false;
		g_bWanderWaypointStableAcrossPress = false;
		g_bWanderDialogueRaised = false;
		g_bWanderContentPassed = false;
		g_bWanderPlayerFrozen = false;
		g_bWanderHaltedDuringDialogue = false;
		g_bWanderDialogueClosed = false;
		g_bWanderPlayerUnfrozen = false;
		g_bWanderResumed = false;

		g_xWanderNamedTarget = INVALID_ENTITY_ID;
		g_xWanderLatchedTarget = INVALID_ENTITY_ID;
		g_xWanderPreviousPosition = Zenith_Maths::Vector3(0.0f);
		g_xWanderPreviousVelocity = Zenith_Maths::Vector3(0.0f);
		g_xWanderHaltStart = Zenith_Maths::Vector3(0.0f);
		g_xWanderResumePrevious = Zenith_Maths::Vector3(0.0f);
		g_xWanderResumePreviousVelocity = Zenith_Maths::Vector3(0.0f);
		g_uWanderInitialWaypoint = 99u;
		g_uWanderLastWaypoint = 99u;
		g_uWanderWaypointAtPress = 99u;
		g_uWanderWaypointAfterPress = 99u;
		g_uWanderRaiseDelta = 0u;
		g_uWanderObserveConsecutiveCoupled = 0u;
		g_uWanderObserveMaxConsecutiveCoupled = 0u;
		g_uWanderResumeConsecutiveCoupled = 0u;
		g_uWanderResumeMaxConsecutiveCoupled = 0u;
		g_uWanderHaltMenuSamples = 0u;
		g_uWanderCloseEnterEdges = 0u;
		g_fWanderObservedPath = 0.0f;
		g_fWanderObserveMinPlayerDistance = 99999.0f;
		g_fWanderMaxObservedSpeed = 0.0f;
		g_fWanderMaxHaltDrift = 0.0f;
		g_fWanderMaxHaltSpeed = 0.0f;
		g_fWanderResumePath = 0.0f;
		g_fWanderMaxResumeSpeed = 0.0f;
		g_fWanderPrePressSpeed = 0.0f;
		g_xWanderLastObserveCoupling = CoupledMotionEvidence{};
		g_xWanderLatestWalkCoupling = CoupledMotionEvidence{};
		g_xWanderLastResumeCoupling = CoupledMotionEvidence{};
		g_strWanderActualLine.clear();
		g_strWanderExpectedLine.clear();

		Zenith_InputSimulator::ResetAllInputState();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// RequestSkip bypasses Verify. Install no fixed dt and load no scene until
		// both baked prerequisites are known present. A missing or misconfigured
		// wanderer is NOT a skip: AwaitReady fails it below.
		g_bWanderPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bWanderPrereqsPresent)
		{
			g_bWanderSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcWander] the Dawnmere scene / terrain bake is absent or incomplete -- "
				"there is no world in which to prove a waypoint patrol");
			return;
		}
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bWanderSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcWander] no persistent ZM_MenuRoot singleton -- FrontEnd.zscen "
				"has not been baked, so the wanderer's dialogue cannot be raised");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fNPC_FIXED_DT);
		g_eWanderPhase = WanderPhase::AwaitReady;
		g_xEngine.Scenes().LoadSceneByIndex(
			iNPC_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_NpcWander(int)
	{
		if (g_eWanderPhase == WanderPhase::Done)
		{
			return false;
		}
		++g_iWanderPhaseFrames;

		switch (g_eWanderPhase)
		{
		case WanderPhase::AwaitReady:
		{
			NpcPlayerView xPlayer;
			WandererView xWanderer;
			const bool bScene = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
					== iNPC_OVERWORLD_BUILD_INDEX;
			const bool bPlayer = bScene && FindActiveNpcPlayer(xPlayer)
				&& xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded();
			const bool bWanderer = bScene && ResolveWanderer(xWanderer)
				&& xWanderer.m_pxCollider->HasValidBody();
			if (!bPlayer || !bWanderer)
			{
				if (g_iWanderPhaseFrames > iWANDER_READY_DEADLINE_FRAMES)
				{
					FailWander("Dawnmere never produced a grounded player plus the authored "
						"Npc_Wanderer with a valid physics body");
					return false;
				}
				return true;
			}

			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_WANDERER);
			g_bWanderDynamicCapsule =
				xWanderer.m_pxCollider->GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC
				&& xWanderer.m_pxCollider->GetCollisionVolumeType()
					== COLLISION_VOLUME_TYPE_CAPSULE;
			g_bWanderAuthored =
				xWanderer.m_pxInteractable->GetNpcId() == ZM_NPC_WANDERER
				&& xRow.m_bWanders
				&& xWanderer.m_pxInteractable->IsInteractable()
				&& xWanderer.m_pxInteractable->IsWanderEnabled()
				&& xWanderer.m_pxInteractable->GetWaypointCount() == 2u;
			if (!g_bWanderAuthored || !g_bWanderDynamicCapsule)
			{
				FailWander("Npc_Wanderer resolved but was not the armed WANDERER row with "
					"an enabled two-waypoint patrol on a dynamic capsule");
				return false;
			}

			g_fWanderObserveMinPlayerDistance =
				PlanarDistance(xPlayer.m_xPosition, xWanderer.m_xPosition);
			g_bWanderObservedAwayFromPlayer =
				g_fWanderObserveMinPlayerDistance > fZM_INTERACT_MAX_DISTANCE * 2.0f;
			if (!g_bWanderObservedAwayFromPlayer)
			{
				FailWander("the wanderer authored inside the player's interaction radius -- "
					"the observation could not distinguish self-motion from player contact");
				return false;
			}

			g_xWanderNamedTarget = xWanderer.m_xEntityID;
			g_xWanderPreviousPosition = xWanderer.m_xPosition;
			if (!ReadWandererVelocity(xWanderer, g_xWanderPreviousVelocity))
			{
				FailWander("the wanderer's body became invalid before motion sampling began");
				return false;
			}
			g_uWanderInitialWaypoint = xWanderer.m_pxInteractable->GetWaypointIndex();
			g_uWanderLastWaypoint = g_uWanderInitialWaypoint;
			g_eWanderPhase = WanderPhase::ObserveWander;
			g_iWanderPhaseFrames = 0;
			return true;
		}

		case WanderPhase::ObserveWander:
		{
			NpcPlayerView xPlayer;
			WandererView xWanderer;
			if (!FindActiveNpcPlayer(xPlayer) || !ResolveWanderer(xWanderer))
			{
				FailWander("the player or wanderer disappeared during the no-contact motion observation");
				return false;
			}

			Zenith_Maths::Vector3 xCurrentVelocity(0.0f);
			if (!ReadWandererVelocity(xWanderer, xCurrentVelocity))
			{
				FailWander("the wanderer's body became invalid during coupled motion sampling");
				return false;
			}
			g_xWanderLastObserveCoupling = CoupleMotionAcrossFrame(
				g_xWanderPreviousPosition, xWanderer.m_xPosition,
				g_xWanderPreviousVelocity, xCurrentVelocity);
			g_fWanderObservedPath += g_xWanderLastObserveCoupling.m_fDisplacement;
			if (g_xWanderLastObserveCoupling.m_bMatched)
			{
				++g_uWanderObserveConsecutiveCoupled;
				if (g_uWanderObserveConsecutiveCoupled
					> g_uWanderObserveMaxConsecutiveCoupled)
				{
					g_uWanderObserveMaxConsecutiveCoupled =
						g_uWanderObserveConsecutiveCoupled;
				}
			}
			else
			{
				g_uWanderObserveConsecutiveCoupled = 0u;
			}
			g_xWanderPreviousPosition = xWanderer.m_xPosition;
			g_xWanderPreviousVelocity = xCurrentVelocity;
			const float fSpeed = PlanarSpeed(xCurrentVelocity);
			if (fSpeed > g_fWanderMaxObservedSpeed)
			{
				g_fWanderMaxObservedSpeed = fSpeed;
			}
			const float fPlayerDistance =
				PlanarDistance(xPlayer.m_xPosition, xWanderer.m_xPosition);
			if (fPlayerDistance < g_fWanderObserveMinPlayerDistance)
			{
				g_fWanderObserveMinPlayerDistance = fPlayerDistance;
			}
			if (g_fWanderObserveMinPlayerDistance <= fZM_INTERACT_MAX_DISTANCE * 2.0f)
			{
				FailWander("the wanderer entered the player's interaction radius during the "
					"self-motion observation -- authored waypoints must remain away from spawn");
				return false;
			}

			g_uWanderLastWaypoint = xWanderer.m_pxInteractable->GetWaypointIndex();
			g_bWanderWaypointAdvanced |=
				g_uWanderLastWaypoint != g_uWanderInitialWaypoint;
			g_bWanderMoved |= g_fWanderObservedPath > fWANDER_MIN_OBSERVED_PATH;
			g_bWanderPhysicsDrivenSeen |=
				g_bWanderMoved
				&& g_uWanderObserveMaxConsecutiveCoupled
					>= uWANDER_OBSERVE_COUPLED_SAMPLES;

			if (g_bWanderMoved
				&& g_bWanderWaypointAdvanced
				&& g_bWanderPhysicsDrivenSeen)
			{
				g_xWanderWalk.m_eStage = WalkStage::Boot;
				g_eWanderPhase = WanderPhase::Walk;
				g_iWanderPhaseFrames = 0;
				return true;
			}
			if (g_iWanderPhaseFrames > iWANDER_OBSERVE_DEADLINE_FRAMES)
			{
				FailWander("the authored wanderer did not cover 2 m, produce eight "
					"consecutive displacement/velocity-coupled samples and advance its "
					"waypoint cursor within the observation budget");
				return false;
			}
			return true;
		}

		case WanderPhase::Walk:
		{
			WandererView xWanderer;
			Zenith_Maths::Vector3 xCurrentVelocity(0.0f);
			if (!ResolveWanderer(xWanderer)
				|| !ReadWandererVelocity(xWanderer, xCurrentVelocity))
			{
				FailWander("the wanderer disappeared while the shared moving-target walk ran");
				return false;
			}
			g_xWanderLatestWalkCoupling = CoupleMotionAcrossFrame(
				g_xWanderPreviousPosition, xWanderer.m_xPosition,
				g_xWanderPreviousVelocity, xCurrentVelocity);
			g_xWanderPreviousPosition = xWanderer.m_xPosition;
			g_xWanderPreviousVelocity = xCurrentVelocity;

			const WalkTick eTick = TickWalk(g_xWanderWalk);
			if (eTick == WalkTick::Failed)
			{
				FailWander(g_xWanderWalk.m_szFailure);
				return false;
			}
			if (eTick == WalkTick::Pressed)
			{
				if (!g_bWanderMovingAtPress)
				{
					FailWander("the shared walk emitted E without coupled live motion above "
						"the body-speed threshold");
					return false;
				}
				g_eWanderPhase = WanderPhase::AssertRaise;
				g_iWanderPhaseFrames = 0;
			}
			return true;
		}

		case WanderPhase::AssertRaise:
		{
			NpcPlayerView xPlayer;
			WandererView xWanderer;
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (!FindActiveNpcPlayer(xPlayer) || !ResolveWanderer(xWanderer)
				|| pxMenu == nullptr)
			{
				FailWander("player, wanderer or menu root vanished while asserting the dialogue raise");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			g_uWanderRaiseDelta =
				xRuntime.GetRaiseCount() - g_xWanderWalk.m_uRaiseCountBefore;
			g_xWanderLatchedTarget = xRuntime.GetLastTarget();
			g_bWanderPlayerFrozen =
				g_xWanderWalk.m_bMovementEnabledBefore
				&& !xPlayer.m_pxController->IsMovementEnabled();

			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_WANDERER);
			if (xRow.m_paszLines != nullptr && xRow.m_uLineCount > 0u)
			{
				g_strWanderExpectedLine = xRow.m_paszLines[0];
			}
			g_strWanderActualLine = pxMenu->GetDialogue().GetCurrentLine();
			g_bWanderContentPassed = !g_strWanderExpectedLine.empty()
				&& g_strWanderActualLine == g_strWanderExpectedLine;

			// The pre-press gate proved coupled motion and latched the live target
			// cursor immediately before TickWalk injected E. Correct dialogue ownership
			// may zero XZ velocity on that same update, so cursor continuity -- not
			// post-press speed -- proves no natural arrival started the halt.
			g_uWanderWaypointAfterPress =
				xWanderer.m_pxInteractable->GetWaypointIndex();
			g_bWanderWaypointStableAcrossPress =
				g_uWanderWaypointAtPress != 99u
				&& g_uWanderWaypointAfterPress == g_uWanderWaypointAtPress;
			g_bWanderDialogueRaised =
				g_uWanderRaiseDelta == 1u
				&& xRuntime.GetLastResult() == ZM_INTERACT_OK
				&& g_xWanderLatchedTarget == xWanderer.m_xEntityID
				&& xWanderer.m_xEntityID == g_xWanderNamedTarget
				&& ZM_UI_MenuStack::IsMenuOpen()
				&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_DIALOGUE
				&& pxMenu->GetDepth() == 1u
				&& pxMenu->GetDialogue().IsActive()
				&& g_bWanderWaypointStableAcrossPress
				&& xWanderer.m_pxInteractable->IsWanderEnabled();
			if (!g_bWanderDialogueRaised
				|| !g_bWanderContentPassed
				|| !g_bWanderPlayerFrozen)
			{
				FailWander("walking to Npc_Wanderer and pressing E did not raise exactly its "
					"own dialogue at the named entity while freezing the player");
				return false;
			}

			g_xWanderHaltStart = xWanderer.m_xPosition;
			g_eWanderPhase = WanderPhase::AssertHalt;
			g_iWanderPhaseFrames = 0;
			return true;
		}

		case WanderPhase::AssertHalt:
		{
			WandererView xWanderer;
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (!ResolveWanderer(xWanderer)
				|| pxMenu == nullptr
				|| !ZM_UI_MenuStack::IsMenuOpen()
				|| pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE
				|| !pxMenu->GetDialogue().IsActive())
			{
				FailWander("a halt sample lacked the active DIALOGUE screen owning the "
					"open menu");
				return false;
			}
			++g_uWanderHaltMenuSamples;
			const float fDrift =
				PlanarDistance(g_xWanderHaltStart, xWanderer.m_xPosition);
			if (fDrift > g_fWanderMaxHaltDrift)
			{
				g_fWanderMaxHaltDrift = fDrift;
			}
			Zenith_Maths::Vector3 xVelocity(0.0f);
			if (!ReadWandererVelocity(xWanderer, xVelocity))
			{
				FailWander("the wanderer's body became invalid during the halt samples");
				return false;
			}
			const float fSpeed = PlanarSpeed(xVelocity);
			if (fSpeed > g_fWanderMaxHaltSpeed)
			{
				g_fWanderMaxHaltSpeed = fSpeed;
			}

			if (g_iWanderPhaseFrames >= iWANDER_HALT_FRAMES)
			{
				g_bWanderHaltedDuringDialogue =
					g_fWanderMaxHaltDrift < fWANDER_MAX_HALT_DRIFT
					&& g_fWanderMaxHaltSpeed < fWANDER_MAX_HALT_SPEED
					&& g_uWanderHaltMenuSamples == (u_int)iWANDER_HALT_FRAMES;
				if (!g_bWanderHaltedDuringDialogue)
				{
					FailWander("the waypoint patrol kept moving while its dialogue owned the menu");
					return false;
				}
				g_eWanderPhase = WanderPhase::CloseDialogue;
				g_iWanderPhaseFrames = 0;
			}
			return true;
		}

		case WanderPhase::CloseDialogue:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailWander("the menu root vanished while closing the wanderer's dialogue");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				if (g_uWanderCloseEnterEdges == 0u)
				{
					FailWander("the wanderer's dialogue closed without an explicit Enter edge");
					return false;
				}
				NpcPlayerView xPlayer;
				WandererView xWanderer;
				if (!FindActiveNpcPlayer(xPlayer) || !ResolveWanderer(xWanderer))
				{
					FailWander("player or wanderer vanished immediately after dialogue close");
					return false;
				}
				if (!ReadWandererVelocity(
					xWanderer, g_xWanderResumePreviousVelocity))
				{
					FailWander("the wanderer's body became invalid as dialogue closed");
					return false;
				}
				g_bWanderDialogueClosed = g_uWanderCloseEnterEdges > 0u;
				g_bWanderPlayerUnfrozen = xPlayer.m_pxController->IsMovementEnabled();
				g_xWanderResumePrevious = xWanderer.m_xPosition;
				g_eWanderPhase = WanderPhase::AssertResume;
				g_iWanderPhaseFrames = 0;
				return true;
			}
			if (g_iWanderPhaseFrames > iWANDER_CLOSE_DEADLINE_FRAMES)
			{
				FailWander("the wanderer's dialogue never closed under spaced Enter edges");
				return false;
			}
			if ((g_iWanderPhaseFrames % 4) == 1)
			{
				++g_uWanderCloseEnterEdges;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case WanderPhase::AssertResume:
		{
			WandererView xWanderer;
			Zenith_Maths::Vector3 xCurrentVelocity(0.0f);
			if (!ResolveWanderer(xWanderer)
				|| !ReadWandererVelocity(xWanderer, xCurrentVelocity))
			{
				FailWander("the wanderer or its physics body disappeared while its patrol "
					"should resume");
				return false;
			}
			g_xWanderLastResumeCoupling = CoupleMotionAcrossFrame(
				g_xWanderResumePrevious, xWanderer.m_xPosition,
				g_xWanderResumePreviousVelocity, xCurrentVelocity);
			g_fWanderResumePath += g_xWanderLastResumeCoupling.m_fDisplacement;
			if (g_xWanderLastResumeCoupling.m_bMatched)
			{
				++g_uWanderResumeConsecutiveCoupled;
				if (g_uWanderResumeConsecutiveCoupled
					> g_uWanderResumeMaxConsecutiveCoupled)
				{
					g_uWanderResumeMaxConsecutiveCoupled =
						g_uWanderResumeConsecutiveCoupled;
				}
			}
			else
			{
				g_uWanderResumeConsecutiveCoupled = 0u;
			}
			g_xWanderResumePrevious = xWanderer.m_xPosition;
			g_xWanderResumePreviousVelocity = xCurrentVelocity;
			const float fSpeed = PlanarSpeed(xCurrentVelocity);
			if (fSpeed > g_fWanderMaxResumeSpeed)
			{
				g_fWanderMaxResumeSpeed = fSpeed;
			}
			g_bWanderResumed =
				xWanderer.m_pxInteractable->IsWanderEnabled()
				&& g_fWanderResumePath > fWANDER_MIN_RESUME_PATH
				&& g_uWanderResumeMaxConsecutiveCoupled
					>= uWANDER_RESUME_COUPLED_SAMPLES;
			if (g_bWanderResumed)
			{
				g_eWanderPhase = WanderPhase::Done;
				return false;
			}
			if (g_iWanderPhaseFrames > iWANDER_RESUME_DEADLINE_FRAMES)
			{
				FailWander("the wanderer never produced six consecutive displacement/velocity-"
					"coupled resume samples after its dialogue closed");
				return false;
			}
			return true;
		}

		case WanderPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_NpcWander()
	{
		ClearWalkInput(g_xWanderWalk);
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		if (g_bWanderSkipped)
		{
			return true;
		}

		const bool bPassed =
			g_bWanderAuthored
			&& g_bWanderDynamicCapsule
			&& g_bWanderObservedAwayFromPlayer
			&& g_bWanderMoved
			&& g_bWanderWaypointAdvanced
			&& g_bWanderPhysicsDrivenSeen
			&& WalkPassed(g_xWanderWalk)
			&& g_bWanderMovingAtPress
			&& g_bWanderWaypointStableAcrossPress
			&& g_bWanderDialogueRaised
			&& g_bWanderContentPassed
			&& g_bWanderPlayerFrozen
			&& g_bWanderHaltedDuringDialogue
			&& g_uWanderHaltMenuSamples == (u_int)iWANDER_HALT_FRAMES
			&& g_bWanderDialogueClosed
			&& g_uWanderCloseEnterEdges > 0u
			&& g_bWanderPlayerUnfrozen
			&& g_bWanderResumed;

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcWander] %s", g_szWanderFailure);
			LogWalkDiagnostics(g_xWanderWalk, "ZM_NpcWander");
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcWander] authored=%d dynamicCapsule=%d awayFromPlayer=%d "
				"minPlayerDist=%.3f | observed path=%.3f (want > %.3f) maxSpeed=%.3f "
				"(want > %.3f) waypoint %u->%u advanced=%d physicsDriven=%d "
				"coupledMax=%u (want >= %u)",
				(int)g_bWanderAuthored, (int)g_bWanderDynamicCapsule,
				(int)g_bWanderObservedAwayFromPlayer, g_fWanderObserveMinPlayerDistance,
				g_fWanderObservedPath, fWANDER_MIN_OBSERVED_PATH,
				g_fWanderMaxObservedSpeed, fWANDER_MIN_BODY_SPEED,
				g_uWanderInitialWaypoint, g_uWanderLastWaypoint,
				(int)g_bWanderWaypointAdvanced, (int)g_bWanderPhysicsDrivenSeen,
				g_uWanderObserveMaxConsecutiveCoupled,
				uWANDER_OBSERVE_COUPLED_SAMPLES);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcWander] observe coupling matched=%d displacement=%.5f "
				"speed=%.3f expected=%.5f alignment=%.3f error=%.5f | "
				"prePress speed=%.3f moving=%d latestCoupled=%d | pressWaypoint "
				"%u->%u stable=%d",
				(int)g_xWanderLastObserveCoupling.m_bMatched,
				g_xWanderLastObserveCoupling.m_fDisplacement,
				g_xWanderLastObserveCoupling.m_fVelocitySpeed,
				g_xWanderLastObserveCoupling.m_fExpectedDisplacement,
				g_xWanderLastObserveCoupling.m_fAlignment,
				g_xWanderLastObserveCoupling.m_fMagnitudeError,
				g_fWanderPrePressSpeed, (int)g_bWanderMovingAtPress,
				(int)g_xWanderLatestWalkCoupling.m_bMatched,
				g_uWanderWaypointAtPress, g_uWanderWaypointAfterPress,
				(int)g_bWanderWaypointStableAcrossPress);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcWander] raiseDelta=%u latchedTarget=%u namedTarget=%u raised=%d "
				"content=%d line='%s' expected='%s' playerFrozen=%d | halt drift=%.3f "
				"(want < %.3f) speed=%.3f (want < %.3f) menuSamples=%u (want %d) "
				"halted=%d | close=%d enterEdges=%u playerUnfrozen=%d resumePath=%.3f "
				"(want > %.3f) resumeSpeed=%.3f coupledMax=%u (want >= %u) resumed=%d",
				g_uWanderRaiseDelta, g_xWanderLatchedTarget.m_uIndex,
				g_xWanderNamedTarget.m_uIndex, (int)g_bWanderDialogueRaised,
				(int)g_bWanderContentPassed, g_strWanderActualLine.c_str(),
				g_strWanderExpectedLine.c_str(), (int)g_bWanderPlayerFrozen,
				g_fWanderMaxHaltDrift, fWANDER_MAX_HALT_DRIFT,
				g_fWanderMaxHaltSpeed, fWANDER_MAX_HALT_SPEED,
				g_uWanderHaltMenuSamples, iWANDER_HALT_FRAMES,
				(int)g_bWanderHaltedDuringDialogue,
				(int)g_bWanderDialogueClosed, g_uWanderCloseEnterEdges,
				(int)g_bWanderPlayerUnfrozen,
				g_fWanderResumePath, fWANDER_MIN_RESUME_PATH,
				g_fWanderMaxResumeSpeed, g_uWanderResumeMaxConsecutiveCoupled,
				uWANDER_RESUME_COUPLED_SAMPLES,
				(int)g_bWanderResumed);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcWander] resume coupling matched=%d displacement=%.5f "
				"speed=%.3f expected=%.5f alignment=%.3f error=%.5f",
				(int)g_xWanderLastResumeCoupling.m_bMatched,
				g_xWanderLastResumeCoupling.m_fDisplacement,
				g_xWanderLastResumeCoupling.m_fVelocitySpeed,
				g_xWanderLastResumeCoupling.m_fExpectedDisplacement,
				g_xWanderLastResumeCoupling.m_fAlignment,
				g_xWanderLastResumeCoupling.m_fMagnitudeError);
		}
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMNpcShopTest = {
	"ZM_NpcShop_Test",
	&Setup_NpcShop,
	&Step_NpcShop,
	&Verify_NpcShop,
	// Every waiting phase owns a deadline that FAILS with its own diagnostic, so this
	// cap must sit ABOVE their SUM or it silently pre-empts them. Summed explicitly:
	// 840 boot + 30 + 30 basis + 2 negative + 1200 approach + 240 arm + 1 assertRaise
	// + 1 assertStock + 400 walk-to-Confirm + 6 buy + 240 close = 2990. 3600 is
	// deliberately above that, so hitting this cap means a phase-machine bug rather
	// than a gameplay timeout.
	/* maxFrames */ 3600,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMNpcShopTest);

static const Zenith_AutomatedTest g_xZMNpcHealTest = {
	"ZM_NpcHeal_Test",
	&Setup_NpcHeal,
	&Step_NpcHeal,
	&Verify_NpcHeal,
	// Same backstop rule, over the LONGER phase list. Summed explicitly: 840 boot +
	// 30 + 30 basis + 2 negative + 1200 approach + 240 arm + 1 assertRaise + 240 read
	// + 1 sample + 320 + 320 yes/no walks + 6 confirm + 240 healed line + 1 re-damage,
	// then the whole NO pass again (1200 re-approach + 240 arm + 1 assertRaise + 240
	// read + 320 walk + 6 confirm) = 5478. 6000 is deliberately ABOVE that sum, so
	// every phase gets to expire with its OWN named diagnostic first and hitting this
	// cap means a phase-machine bug rather than a gameplay timeout. No healthy run
	// comes near either number -- the real run is ~1000 frames.
	/* maxFrames */ 6000,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMNpcHealTest);

static const Zenith_AutomatedTest g_xZMS6InteractGateTest = {
	"ZM_S6InteractGate_Test",
	&Setup_S6InteractGate,
	&Step_S6InteractGate,
	&Verify_S6InteractGate,
	// ★ RULING M. This cap must sit ABOVE the SUM of the per-phase deadlines, or it
	// silently pre-empts the phase that would have named WHERE the run stalled -- and
	// hitting it must therefore mean a phase-machine bug, never a gameplay timeout.
	// (The SC6 heal test originally had this backwards.) Summed explicitly, at the
	// fixed 1/60 dt, with every 1/30-derived budget already doubled:
	//
	//   walk leg 1 (villager) 840 boot + 30 + 30 basis + 2 negative
	//                         + 1200 approach + 240 arm .................. 2342
	//   talk    1 raise + 6 rePress + 240 read + 6 settle ................  253
	//   buy     1 retarget + (1200 approach + 240 arm) + 1 raise
	//                         + 400 walk-to-Confirm + 6 buy
	//                         + 240 close + 6 settle .................... 2094
	//   heal    1 arm + 1 retarget + (1200 approach + 240 arm) + 1 raise
	//                         + 240 read + 320 + 320 yes/no walks
	//                         + 6 confirm + 240 close ................... 2569
	//   menus   240 open + 320 + 320 nav probes (down, up)
	//                         + 3x320 visit walks + 3x6 confirms
	//                         + 3x6 escapes + 320 exit walk
	//                         + 6 exit confirm .......................... 2202
	//   exit    30 hold + 1 assert ......................................   31
	//                                                           TOTAL == 9491
	//
	// 10000 sits above that sum with ~5% headroom. No healthy run comes near it --
	// the three approaches are ~200 frames each in practice, not 1200, and each root
	// walk is a handful of spaced edges rather than its whole 320-frame budget.
	/* maxFrames */ 10000,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMS6InteractGateTest);

static const Zenith_AutomatedTest g_xZMNpcWanderTest = {
	"ZM_NpcWander_Test",
	&Setup_NpcWander,
	&Step_NpcWander,
	&Verify_NpcWander,
	// Per-phase deadlines at the fixed 1/60 dt: 840 ready + 900 no-contact
	// observation + the shared walk's 2342 worst case + 1 raise + 30 halt +
	// 480 close + 360 resume = 4953. The harness cap sits above that sum so a
	// named phase failure always fires before the generic frame cap.
	/* maxFrames */ 5500,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMNpcWanderTest);

#endif // ZENITH_INPUT_SIMULATOR
