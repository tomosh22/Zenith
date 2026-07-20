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
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"
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
		ClearWalkInput(xWalk);
	}

	// The traversal drive, identical in dead zone, key choice and held run modifier
	// to the SHIPPED walk it characterises. It additionally records the held set so
	// a stall failure can name it.
	void DriveTowardXZ(
		WalkContext& xWalk,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		ClearWalkInput(xWalk);
		constexpr float fDEAD_ZONE = 0.08f;
		const float fDeltaX = xTarget.x - xPosition.x;
		const float fDeltaZ = xTarget.z - xPosition.z;
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
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] boot flags: scene=%d player=%d body=%d npcArmed=%d spawnDistance=%.3f",
			szTag, (int)xWalk.m_bSawScene, (int)xWalk.m_bSawPlayer,
			(int)xWalk.m_bSawPlayerBody, (int)xWalk.m_bSawNpc, xWalk.m_fSpawnDistance);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] basis probe: legZ dx=%.3f dz=%.3f | legX dx=%.3f dz=%.3f "
			"(each leg's own axis must exceed %.3f and dominate)",
			szTag, xWalk.m_fBasisZLegDeltaX, xWalk.m_fBasisZLegDeltaZ,
			xWalk.m_fBasisXLegDeltaX, xWalk.m_fBasisXLegDeltaZ, fBASIS_MIN_TRAVEL);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[%s] approach: distance=%.3f best=%.3f stallFrames=%d held W=%d A=%d S=%d D=%d "
			"lastLiveReject=%s",
			szTag, xWalk.m_fCurrentDistance, xWalk.m_fBestDistance, xWalk.m_iStallFrames,
			(int)xWalk.m_abHeldKeys[0], (int)xWalk.m_abHeldKeys[1],
			(int)xWalk.m_abHeldKeys[2], (int)xWalk.m_abHeldKeys[3],
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

#endif // ZENITH_INPUT_SIMULATOR
