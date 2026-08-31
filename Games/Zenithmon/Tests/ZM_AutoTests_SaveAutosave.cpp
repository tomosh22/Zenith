#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_SaveAutosave -- the S7 item 2 SC6 milestone-autosave proof at
// the DISK layer. TEST-ONLY closure: this file changes no production code.
//
// ZM_ResumePlacement_Test (ZM_AutoTests_SaveResume.cpp) already proves the
// arrival latch fires and the Auto slot goes READY after ONE warp, and its
// live-blocker probe pins the WARP term of the refusal policy mid-transition.
// What it deliberately does NOT pin is the producer mechanics this test owns:
//
//   1. WHAT the drain writes. A distinctive live state (money 777777, a real
//      story flag, a second party member nicknamed "AutoProof") is installed
//      with the world position left UNSET, and the autosave that lands after
//      the arrival warp must contain exactly that state PLUS the captured
//      TownCenter body pose -- proven by reading the slot back and
//      canonical-comparing the candidate against the live state.
//   2. THE CAPTURE-INTO-LIVE ORDERING. ZM_TryAutosave must run
//      CaptureWorldPosition into the LIVE ZM_GameState before WriteState.
//      Dropping the capture is today's green hole: the counter still rises and
//      the slot still goes READY, so the resume-placement assertions stay
//      green. Here it reds THREE ways: the LIVE state's world-position scene
//      stays UNSET (g_bMAPositiveLiveSceneCaptured), the disk candidate's
//      scene is not Dawnmere (g_bMAPositiveSceneTag) and ZM_ValidateResume
//      reports INVALID_SCENE (g_bMAPositiveResumeValid).
//   3. THE MENU-OPEN WIRING. The pure boot units pin ZM_ShouldAutosave's menu
//      term; they cannot pin the ZM_UI_MenuStack::IsMenuOpen() CONSULT inside
//      ZM_TryAutosave. With the ROOT menu open, with blocker==NONE and a
//      capture PROVEN to succeed on the same frame, a direct
//      ZM_TryAutosave(SCENE_ENTERED) must return FALSE with the counter
//      unmoved and zero WRITE_STATE. The menu term is the SOLE refuser on that
//      frame, so the refusal is NOT over-determined here.
//   4. THE DRAIN'S REFUSAL UNDER AN OPEN MENU. The same Dawnmere->TownCenter
//      warp is then run with the menu held open THROUGH the arrival: the Auto
//      slot's raw bytes must stay byte-identical, the counter must not move,
//      and the trace must show zero WRITE_STATE / READ_STATE across the whole
//      window. (During the in-flight warp itself a refusal WOULD be
//      over-determined -- the WARP blocker AND the menu both refuse -- which
//      is exactly why this test never attempts one there. The drain only
//      fires on the IDLE frame after, where the menu term stands alone.)
//   5. CONSUME-BEFORE-ATTEMPT. After the menu closes, 30 IDLE menu-closed
//      frames must pass with the counter STILL flat, the trace STILL empty
//      and the bytes STILL identical: a drain that kept the latch on a
//      REFUSED attempt would fire the moment the menu term cleared. The
//      SampleBlocked phase alone cannot distinguish "the drain ran and was
//      refused" from "the drain never ran" -- both leave the same flat
//      observables -- so THIS watch is the phase that separates the two, and
//      SampleReArm then proves latching still works afterwards.
//   6. LATCH RE-ARMING. A third warp must land a SECOND autosave (+1 raw on
//      the counter, exactly one more WRITE_STATE on AUTO, the read-back
//      candidate canonical-equal to the live state). A one-shot-per-process
//      latch reds here.
//
// ---- PROVENANCE -------------------------------------------------------------
// The phase-machine shape (thin dispatch Step over per-phase drivers), the
// slot-operation observer trace (64-event capacity + per-op/per-slot
// counters), the raw slot-byte read, the canonical-byte state compare, the
// "_Test" slot-alias interlock (alias failure = fail + refuse all disk
// access, NEVER a skip), the Setup ordering and the unconditional Verify
// teardown are copied from ZM_AutoTests_SaveContinue.cpp (the S7 item 2 SC5
// test). That file's ordinal-sweep / exactly-one trace helpers are
// deliberately NOT carried: every window here asserts RAW per-operation
// counts rather than probe orderings, so those helpers would be dead code --
// the same reason ZM_AutoTests_SaveResume.cpp does not copy the NPC
// interaction machine. The Dawnmere settle wait, the warp-ran-then-IDLE
// arrival await, the settled/grounded player predicates and the RequestSkip
// guard shapes are copied from ZM_AutoTests_SaveResume.cpp (S7 item 2 SC3).
//
// ---- DESIGN RULES (inherited from both) -------------------------------------
//   * EVERY phase flag DEFAULTS TO FAILING, so a phase that never runs fails.
//   * Counters are sampled as RAW DELTAS (before/after), never "changed"
//     booleans, and the raw values are printed in Verify.
//   * Every waiting phase owns its OWN deadline with its OWN diagnostic.
//   * Input is exclusively Zenith_InputSimulator key edges (M / ESCAPE).
//     SetFocusedElement and the programmatic menu seams appear NOWHERE.
//   * Teardown is UNCONDITIONAL on every exit path, including a mid-phase
//     failure: input, fixed dt, the menu (possibly open), the manager, the
//     slot files, the aliases, the observer and the boot scene.
//
// FRAME ORDERING this test depends on: Zenith_AutomatedTestRunner::Tick()
// (which calls Step) runs BEFORE g_xEngine.Scenes().Update() in
// Zenith_MainLoop, so a key edge injected in Step is consumed later in the
// SAME frame and observable from the NEXT Step call. The manager's autosave
// drain (`m_bArrivalAutosavePending && IDLE` in ZM_GameStateManager::OnUpdate)
// runs inside the scene update, so the write lands at the earliest on the
// scene-update half of the frame AFTER the arrival tail latched it -- every
// sample phase below polls or settles across that boundary rather than
// assuming a frame count.
//
// HEADLESS NOTE: m_bRequiresGraphics = true, so the headless CI batch SKIPS
// this test and a skip counts as a PASS. A green zm-tests proves NOTHING
// about it -- it must be run windowed, locally, with
// `--filter ZM_MilestoneAutosave_Test`.
// ============================================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "SaveData/Zenith_SaveData.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"
#include "Zenithmon/Source/Save/ZM_Autosave.h"
#include "Zenithmon/Source/Save/ZM_ResumePoint.h"
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"

namespace
{
	constexpr float fMA_FIXED_DT = 1.0f / 60.0f;
	constexpr int iMA_FRONTEND_BUILD = 0;
	constexpr int iMA_DAWNMERE_BUILD = 2;
	constexpr const char* szMA_TOWNCENTER_TAG = "TownCenter";

	// The distinctive install. 777777 sits under uZM_MONEY_CAP and far from the
	// 3000 starter purse; ZM_SPECIES_NIBBIN is a real species that is NOT the
	// FERNFAWN starter; the flag is a real registry id.
	constexpr u_int uMA_DISTINCTIVE_MONEY = 777777u;
	constexpr const char* szMA_PROOF_NICKNAME = "AutoProof";

	// Pose tolerances, identical to the SC5/SC3 pose gates. The drain captures
	// the live body CENTRE and the player keeps resting on the same terrain
	// contact, so the disk candidate sits well inside these bounds; every
	// mutation this test is written to catch moves the pose by METRES.
	constexpr float fMA_MAX_PLANAR_ERROR = 0.05f;
	constexpr float fMA_MAX_VERTICAL_ERROR = 0.10f;
	constexpr float fMA_MAX_YAW_ERROR = 0.05f;

	// ---- phase budgets. 600 + 8 + 480 + (12+1) + 120 + 8 + 480 + (12+1)
	// + 60 + 30 + 480 + (12+1) = 2305 < the 2600-frame cap, so every phase
	// owns a REAL deadline and the harness cap is only a backstop. The 8s and
	// the "+1"s are slack for the non-waiting phases (the install, the wiring
	// probe and the batteries all complete on a fixed early frame); only
	// values a guard actually compares against get a named constant. ----
	constexpr int iMA_DAWNMERE_DEADLINE = 600;
	constexpr int iMA_POSITIVE_WARP_DEADLINE = 480;
	constexpr int iMA_POSITIVE_DRAIN_POLLS = 12;
	constexpr int iMA_MENU_OPEN_DEADLINE = 120;
	constexpr int iMA_MENU_PROBE_SETTLE_FRAMES = 2;
	constexpr int iMA_BLOCKED_WARP_DEADLINE = 480;
	constexpr int iMA_BLOCKED_SETTLE_FRAMES = 4;
	constexpr int iMA_MENU_CLOSE_DEADLINE = 60;
	constexpr int iMA_NORETRY_WATCH_FRAMES = 30;
	constexpr int iMA_REARM_WARP_DEADLINE = 480;
	constexpr int iMA_REARM_DRAIN_POLLS = 12;

	constexpr u_int uMA_SLOT_TRACE_CAPACITY = 64u;

	// ------------------------------------------------------------------------
	// The slot-operation observer trace, copied from
	// ZM_AutoTests_SaveContinue.cpp (the S7 item 2 SC5 test) with its reasoning
	// intact: one event per PUBLIC slot-layer call, fired on entry, so a window
	// can assert raw per-operation / per-slot counts at the DISK layer rather
	// than trusting UI counters. The ordinal-sweep / exactly-one helpers are
	// not carried (see the provenance note above).
	// ------------------------------------------------------------------------
	struct MASlotOperation
	{
		ZM_SAVE_SLOT_OPERATION_FOR_TESTS m_eOperation = ZM_SAVE_SLOT_OPERATION_PROBE_SLOT;
		ZM_SAVE_SLOT m_eSlot = ZM_SAVE_SLOT_NONE;
	};

	MASlotOperation g_axMASlotOperations[uMA_SLOT_TRACE_CAPACITY];
	u_int g_uMASlotOperationCount = 0u;
	bool g_bMASlotOperationOverflow = false;

	void MAResetSlotOperationTrace()
	{
		g_uMASlotOperationCount = 0u;
		g_bMASlotOperationOverflow = false;
	}

	void MAObserveSlotOperation(
		ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation,
		ZM_SAVE_SLOT eSlot)
	{
		if (g_uMASlotOperationCount >= uMA_SLOT_TRACE_CAPACITY)
		{
			g_bMASlotOperationOverflow = true;
			return;
		}
		g_axMASlotOperations[g_uMASlotOperationCount].m_eOperation = eOperation;
		g_axMASlotOperations[g_uMASlotOperationCount].m_eSlot = eSlot;
		++g_uMASlotOperationCount;
	}

	u_int MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation)
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < g_uMASlotOperationCount; ++u)
		{
			if (g_axMASlotOperations[u].m_eOperation == eOperation) { ++uCount; }
		}
		return uCount;
	}

	u_int MACountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation,
		ZM_SAVE_SLOT eSlot)
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < g_uMASlotOperationCount; ++u)
		{
			if (g_axMASlotOperations[u].m_eOperation == eOperation
				&& g_axMASlotOperations[u].m_eSlot == eSlot)
			{
				++uCount;
			}
		}
		return uCount;
	}

	// ------------------------------------------------------------------------
	// Live resolvers + player pose, copied from ZM_AutoTests_SaveContinue.cpp.
	// NOTHING is cached across frames: the component pools relocate on
	// swap-and-pop, and the scene reloads rebuild every entity.
	// ------------------------------------------------------------------------
	struct MAPlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	int MAActiveBuildIndex()
	{
		return g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
	}

	ZM_GameStateManager* MAResolveManager()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_GameStateManager>() : nullptr;
	}

	ZM_UI_MenuStack* MAResolveMenu()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
	}

	bool MAFindPlayer(MAPlayerView& xOut)
	{
		xOut = MAPlayerView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent&)
			{
				++uCount;
				if (uCount == 1u)
				{
					xOut.m_xEntityID = xID;
					xOut.m_pxController = &xController;
					xOut.m_pxCollider = &xCollider;
				}
			});
		return uCount == 1u;
	}

	// The body CENTRE, read straight from physics -- the same convention the
	// capture and every pose assertion in this file speaks (copied from
	// SCReadPlayerPose; markers store FEET and mixing the two is a silent 0.9 m
	// error).
	bool MAReadPlayerPose(Zenith_Maths::Vector3& xCentreOut, float& fYawOut)
	{
		MAPlayerView xPlayer;
		if (!MAFindPlayer(xPlayer) || xPlayer.m_pxCollider == nullptr
			|| !xPlayer.m_pxCollider->HasValidBody())
		{
			return false;
		}
		const Zenith_PhysicsBodyID xBodyID = xPlayer.m_pxCollider->GetBodyID();
		xCentreOut = g_xEngine.Physics().GetBodyPosition(xBodyID);
		fYawOut = ZM_YawFromRotation(g_xEngine.Physics().GetBodyRotation(xBodyID));
		return true;
	}

	float MAPlanarDistance(const Zenith_Maths::Vector3& xA,
		const Zenith_Maths::Vector3& xB)
	{
		const float fX = xA.x - xB.x;
		const float fZ = xA.z - xB.z;
		return std::sqrt(fX * fX + fZ * fZ);
	}

	float MAWrappedAngleDifference(float fA, float fB)
	{
		float fDelta = fA - fB;
		while (fDelta > 3.14159265358979323846f) { fDelta -= 6.28318530717958647692f; }
		while (fDelta < -3.14159265358979323846f) { fDelta += 6.28318530717958647692f; }
		return fDelta;
	}

	// Copied from SCDawnmereReady: a settled, playable Dawnmere with an IDLE
	// warp machine, a live simulation and a unique grounded bodied player.
	bool MADawnmereReady()
	{
		if (MAActiveBuildIndex() != iMA_DAWNMERE_BUILD
			|| ZM_GameStateManager::IsWarpInProgress()
			|| !g_xEngine.Physics().HasActiveSimulation())
		{
			return false;
		}
		ZM_GameStateManager* pxManager = MAResolveManager();
		MAPlayerView xPlayer;
		return pxManager != nullptr
			&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
			&& MAFindPlayer(xPlayer)
			&& xPlayer.m_pxCollider != nullptr && xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController != nullptr && xPlayer.m_pxController->IsGrounded();
	}

	// The FrontEnd/Dawnmere baked-asset guard, copied verbatim from
	// SCRequiredAssetsPresent: a skip is only ever about absent git-ignored
	// inputs, never about behaviour.
	bool MARequiredAssetsPresent()
	{
		const char* aszRelative[] =
		{
			"Scenes/FrontEnd" ZENITH_SCENE_EXT,
			"Scenes/Dawnmere" ZENITH_SCENE_EXT,
			"Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			"Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			"Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			"Terrain/Dawnmere/Physics_0_0" ZENITH_GEOMETRY_EXT,
			"Terrain/Dawnmere/Render_LOW_0_0" ZENITH_GEOMETRY_EXT,
			"Terrain/Dawnmere/Render_0_0" ZENITH_GEOMETRY_EXT,
		};
		for (u_int u = 0u; u < (u_int)(sizeof(aszRelative) / sizeof(aszRelative[0])); ++u)
		{
			char acPath[ZENITH_MAX_PATH_LENGTH];
			std::snprintf(acPath, sizeof(acPath), "%s%s", GAME_ASSETS_DIR, aszRelative[u]);
			std::error_code xError;
			if (!std::filesystem::is_regular_file(acPath, xError) || xError
				|| std::filesystem::file_size(acPath, xError) == 0u || xError)
			{
				return false;
			}
		}
		return true;
	}

	// The "_Test" slot-alias interlock, copied from SCTestSlotNamesActive: a
	// failure here is a hard FAIL with all disk access refused, NEVER a skip --
	// running any further would address the developer's real save files.
	bool MATestSlotNamesActive()
	{
		for (u_int u = 0u; u < (u_int)ZM_SAVE_SLOT_COUNT; ++u)
		{
			const ZM_SAVE_SLOT eSlot = static_cast<ZM_SAVE_SLOT>(u);
			const char* szLive = ZM_SaveSlots::SlotName(eSlot);
			const char* szShipping = ZM_SaveSlots::SlotShippingName(eSlot);
			if (szLive == nullptr || szShipping == nullptr
				|| std::strcmp(szLive, szShipping) == 0)
			{
				return false;
			}
		}
		return true;
	}

	// Raw slot-byte access, copied from SCBuildSlotPath / SCReadSlotBytes /
	// SCBytesEqual. These go through Zenith_FileAccess DIRECTLY -- never the
	// slot layer -- so they do not append to the observer trace and can run
	// inside a counted window.
	void MABuildSlotPath(ZM_SAVE_SLOT eSlot, char* pszOut, size_t uCapacity)
	{
		std::snprintf(pszOut, uCapacity, "%s%s%s", Zenith_SaveData::GetSaveDirectory(),
			ZM_SaveSlots::SlotName(eSlot), ZENITH_SAVE_EXT);
	}

	bool MAReadSlotBytes(ZM_SAVE_SLOT eSlot, Zenith_Vector<u_int8>& xOut)
	{
		xOut.Clear();
		char acPath[ZENITH_MAX_PATH_LENGTH];
		MABuildSlotPath(eSlot, acPath, sizeof(acPath));
		if (!Zenith_FileAccess::FileExists(acPath)) { return false; }
		uint64_t ulSize = 0u;
		char* pData = Zenith_FileAccess::ReadFile(acPath, ulSize);
		if (pData == nullptr || ulSize == 0u)
		{
			if (pData != nullptr) { Zenith_FileAccess::FreeFileData(pData); }
			return false;
		}
		xOut.Resize((u_int)ulSize);
		std::memcpy(xOut.GetDataPointer(), pData, (size_t)ulSize);
		Zenith_FileAccess::FreeFileData(pData);
		return true;
	}

	bool MABytesEqual(const Zenith_Vector<u_int8>& xA, const Zenith_Vector<u_int8>& xB)
	{
		return xA.GetSize() == xB.GetSize()
			&& (xA.GetSize() == 0u
				|| std::memcmp(xA.GetDataPointer(), xB.GetDataPointer(), xA.GetSize()) == 0);
	}

	// Canonical-byte state comparison, copied from SCCanonicalBytes /
	// SCCanonicalEqual: two states are equal iff the FROZEN codec maps them to
	// the same wire image. A codec-valid UNSET world position (scene UNSET with
	// an all-zero tag) round-trips through this exactly like any other.
	bool MACanonicalBytes(const ZM_GameState& xState, Zenith_Vector<u_int8>& xOut)
	{
		xOut.Clear();
		Zenith_DataStream xStream;
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
		if (!xStatus.IsOk() || xStream.GetCursor() == 0u) { return false; }
		xOut.Resize((u_int)xStream.GetCursor());
		std::memcpy(xOut.GetDataPointer(), xStream.GetData(), xOut.GetSize());
		return true;
	}

	bool MACanonicalEqual(const ZM_GameState& xA, const ZM_GameState& xB)
	{
		Zenith_Vector<u_int8> xABytes;
		Zenith_Vector<u_int8> xBBytes;
		return MACanonicalBytes(xA, xABytes) && MACanonicalBytes(xB, xBBytes)
			&& MABytesEqual(xABytes, xBBytes);
	}

	// Reset one ZM_GameState global per CALL, so no frame ever holds more than
	// one ZM_GameState temporary (copied from SCResetGameState): in a Debug
	// (/Od) build the compiler reserves a distinct stack slot for practically
	// every local and temporary in a function, and a single ZM_GameState is on
	// the order of 100-200 KB.
	void MAResetGameState(ZM_GameState& xState)
	{
		xState = ZM_GameState{};
	}

	// The shared "the requested warp actually RAN and then completed back to a
	// settled Dawnmere" predicate, factored out of the SC3 arrival phase
	// (ArriveViaWarp) because this test runs the same await THREE times.
	bool MAWarpCompletedToDawnmere(bool bSawWarpRun)
	{
		ZM_GameStateManager* pxManager = MAResolveManager();
		MAPlayerView xPlayer;
		return pxManager != nullptr
			&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
			&& bSawWarpRun
			&& MAActiveBuildIndex() == iMA_DAWNMERE_BUILD
			&& !ZM_GameStateManager::IsWarpInProgress()
			&& MAFindPlayer(xPlayer)
			&& xPlayer.m_pxCollider != nullptr && xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController != nullptr && xPlayer.m_pxController->IsGrounded();
	}
}

namespace
{
	enum class MAPhase
	{
		AwaitDawnmere,
		InstallDistinctive,
		PositiveArrival,
		SamplePositive,
		OpenMenu,
		MenuOpenProbe,
		BlockedArrival,
		SampleBlocked,
		CloseMenu,
		NoRetryWatch,
		ReArmArrival,
		SampleReArm,
		Done,
	};

	MAPhase g_eMAPhase = MAPhase::Done;
	int g_iMAPhaseFrames = 0;
	bool g_bMAActive = false;
	bool g_bMASkipped = false;
	bool g_bMAFailed = false;
	bool g_bMAAliasesActive = false;
	const char* g_szMAFailure = "test did not reach verification";

	// The distinctive install and the throwaway capture control. FILE-SCOPE,
	// never stack: a ZM_GameState is ~100-200 KB and the stack reserve is 1 MB.
	ZM_GameState g_xMADistinctive;
	ZM_GameState g_xMAThrowaway;
	// The raw Auto-slot image taken the moment the blocked window opens. Any
	// write inside the window must show up as a byte difference.
	Zenith_Vector<u_int8> g_xMAAutoBytesBeforeBlocked;

	// ---- RAW counter samples. "After" values default to 0xffffffff so an
	// unsampled run FAILS rather than passing on 0 == 0 (the SC3 idiom). ----
	u_int g_uMAPreInstallMoney = uMA_DISTINCTIVE_MONEY;   // defaults to FAILING the "differed" flag
	u_int g_uMAPreInstallPartyCount = 2u;                 // defaults to the INSTALLED count, i.e. failing
	u_int g_uMACounterBeforePositive = 0u;
	u_int g_uMACounterAfterPositive = 0xffffffffu;
	u_int g_uMACounterBeforeMenuProbe = 0u;
	u_int g_uMACounterAfterMenuProbe = 0xffffffffu;
	u_int g_uMACounterBeforeBlocked = 0u;
	u_int g_uMACounterAfterBlocked = 0xffffffffu;
	u_int g_uMACounterAfterNoRetry = 0xffffffffu;
	u_int g_uMACounterBeforeReArm = 0u;
	u_int g_uMACounterAfterReArm = 0xffffffffu;
	float g_fMAPositivePlanarError = 9999.0f;
	float g_fMAPositiveVerticalError = 9999.0f;
	float g_fMAPositiveYawError = 9999.0f;

	// ---- EVERY phase flag defaults to FAILING ----
	bool g_bMADawnmereReady = false;
	bool g_bMADistinctiveBuilt = false;
	bool g_bMAPreInstallDistinctive = false;
	bool g_bMAInstallVerified = false;
	bool g_bMAWarpRequested = false;
	bool g_bMASawPositiveWarpRun = false;
	bool g_bMAPositiveArrived = false;
	bool g_bMAPositiveCounterDelta = false;
	bool g_bMAPositiveTraceExact = false;
	bool g_bMAPositiveAutoReady = false;
	bool g_bMAPositiveReadOk = false;
	bool g_bMAPositiveSceneTag = false;
	bool g_bMAPositivePoseMatch = false;
	bool g_bMAPositiveFieldsMatch = false;
	bool g_bMAPositiveResumeValid = false;
	bool g_bMAPositiveCanonicalLive = false;
	bool g_bMAPositiveLiveSceneCaptured = false;
	bool g_bMAMenuOpened = false;
	bool g_bMAMenuProbeBlockerNone = false;
	bool g_bMAMenuProbeCaptureWouldWork = false;
	bool g_bMAMenuProbeRefused = false;
	bool g_bMAMenuProbeCounterUnchanged = false;
	bool g_bMAMenuProbeTraceNoWrite = false;
	bool g_bMABlockedBytesSnapshot = false;
	bool g_bMABlockedWarpRequested = false;
	bool g_bMASawBlockedWarpRun = false;
	bool g_bMABlockedArrived = false;
	bool g_bMABlockedMenuStillOpen = false;
	bool g_bMABlockedCounterUnchanged = false;
	bool g_bMABlockedTraceClean = false;
	bool g_bMABlockedBytesIdentical = false;
	bool g_bMABlockedMenuOpenAtSample = false;
	bool g_bMABlockedBlockerNone = false;
	bool g_bMABlockedCaptureWouldWork = false;
	bool g_bMAMenuClosed = false;
	bool g_bMANoRetryStayedIdle = false;
	bool g_bMANoRetryCounterStill = false;
	bool g_bMANoRetryTraceStill = false;
	bool g_bMANoRetryBytesStill = false;
	bool g_bMAReArmWarpRequested = false;
	bool g_bMASawReArmWarpRun = false;
	bool g_bMAReArmArrived = false;
	bool g_bMAReArmCounterDelta = false;
	bool g_bMAReArmTraceExact = false;
	bool g_bMAReArmAutoReady = false;
	bool g_bMAReArmReadOk = false;
	bool g_bMAReArmCanonicalLive = false;
	bool g_bMAReArmSceneTag = false;

	void Setup_ZMMilestoneAutosave();
	bool Step_ZMMilestoneAutosave(int iFrame);
	bool Verify_ZMMilestoneAutosave();
}

static const Zenith_AutomatedTest g_xZMMilestoneAutosaveTest = {
	"ZM_MilestoneAutosave_Test",
	&Setup_ZMMilestoneAutosave,
	&Step_ZMMilestoneAutosave,
	&Verify_ZMMilestoneAutosave,
	// Phase-local maxima sum to 2305 frames (600 + 8 + 480 + 13 + 120 + 8
	// + 480 + 13 + 60 + 30 + 480 + 13, including all three 480-frame warp
	// settles and the 600-frame Dawnmere settle). The harness cap sits above
	// that sum so a named phase deadline, not this backstop, diagnoses every
	// ordinary stall.
	/* maxFrames */ 2600,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMMilestoneAutosaveTest);

namespace
{
	void MAFail(const char* szReason)
	{
		g_szMAFailure = szReason;
		g_bMAFailed = true;
		g_eMAPhase = MAPhase::Done;
	}

	void MAResetObservations()
	{
		g_bMADawnmereReady = false;
		g_bMADistinctiveBuilt = false;
		g_bMAPreInstallDistinctive = false;
		g_bMAInstallVerified = false;
		g_bMAWarpRequested = false;
		g_bMASawPositiveWarpRun = false;
		g_bMAPositiveArrived = false;
		g_bMAPositiveCounterDelta = false;
		g_bMAPositiveTraceExact = false;
		g_bMAPositiveAutoReady = false;
		g_bMAPositiveReadOk = false;
		g_bMAPositiveSceneTag = false;
		g_bMAPositivePoseMatch = false;
		g_bMAPositiveFieldsMatch = false;
		g_bMAPositiveResumeValid = false;
		g_bMAPositiveCanonicalLive = false;
		g_bMAPositiveLiveSceneCaptured = false;
		g_bMAMenuOpened = false;
		g_bMAMenuProbeBlockerNone = false;
		g_bMAMenuProbeCaptureWouldWork = false;
		g_bMAMenuProbeRefused = false;
		g_bMAMenuProbeCounterUnchanged = false;
		g_bMAMenuProbeTraceNoWrite = false;
		g_bMABlockedBytesSnapshot = false;
		g_bMABlockedWarpRequested = false;
		g_bMASawBlockedWarpRun = false;
		g_bMABlockedArrived = false;
		g_bMABlockedMenuStillOpen = false;
		g_bMABlockedCounterUnchanged = false;
		g_bMABlockedTraceClean = false;
		g_bMABlockedBytesIdentical = false;
		g_bMABlockedMenuOpenAtSample = false;
		g_bMABlockedBlockerNone = false;
		g_bMABlockedCaptureWouldWork = false;
		g_bMAMenuClosed = false;
		g_bMANoRetryStayedIdle = false;
		g_bMANoRetryCounterStill = false;
		g_bMANoRetryTraceStill = false;
		g_bMANoRetryBytesStill = false;
		g_bMAReArmWarpRequested = false;
		g_bMASawReArmWarpRun = false;
		g_bMAReArmArrived = false;
		g_bMAReArmCounterDelta = false;
		g_bMAReArmTraceExact = false;
		g_bMAReArmAutoReady = false;
		g_bMAReArmReadOk = false;
		g_bMAReArmCanonicalLive = false;
		g_bMAReArmSceneTag = false;
	}

	void Setup_ZMMilestoneAutosave()
	{
		// A failed/skipped predecessor must never leave our fixed trace callback installed.
		ZM_SaveSlots::SetOperationObserverForTests(nullptr);
		MAResetSlotOperationTrace();
		g_eMAPhase = MAPhase::Done;
		g_iMAPhaseFrames = 0;
		g_bMAActive = false;
		g_bMASkipped = false;
		g_bMAFailed = false;
		g_bMAAliasesActive = false;
		g_szMAFailure = "test did not reach verification";
		MAResetObservations();
		MAResetGameState(g_xMADistinctive);
		MAResetGameState(g_xMAThrowaway);
		g_xMAAutoBytesBeforeBlocked.Clear();
		g_uMAPreInstallMoney = uMA_DISTINCTIVE_MONEY;
		g_uMAPreInstallPartyCount = 2u;
		g_uMACounterBeforePositive = 0u;
		g_uMACounterAfterPositive = 0xffffffffu;
		g_uMACounterBeforeMenuProbe = 0u;
		g_uMACounterAfterMenuProbe = 0xffffffffu;
		g_uMACounterBeforeBlocked = 0u;
		g_uMACounterAfterBlocked = 0xffffffffu;
		g_uMACounterAfterNoRetry = 0xffffffffu;
		g_uMACounterBeforeReArm = 0u;
		g_uMACounterAfterReArm = 0xffffffffu;
		g_fMAPositivePlanarError = 9999.0f;
		g_fMAPositiveVerticalError = 9999.0f;
		g_fMAPositiveYawError = 9999.0f;

		// A skip bypasses Verify, so EVERY guard precedes every fixed-dt, scene,
		// alias and disk mutation. First: the baked assets this test walks
		// through (run a *_True config once to bake them).
		if (!MARequiredAssetsPresent())
		{
			g_bMASkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_MilestoneAutosave] FrontEnd/Dawnmere assets are absent");
			return;
		}
		// Second: no persistent manager means FrontEnd.zscen was never baked,
		// so there is no warp machine to arrive through.
		{
			Zenith_EntityID xManagerID = INVALID_ENTITY_ID;
			if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerID))
			{
				g_bMASkipped = true;
				Zenith_AutomatedTestRunner::RequestSkip(
					"[ZM_MilestoneAutosave] no persistent ZM_GameStateManager singleton -- "
					"FrontEnd.zscen has not been baked, so there is no warp machine to "
					"arrive through");
				return;
			}
		}
		// Third and LAST skip: no persistent menu means there is no ROOT pause
		// menu to open, and half the phases below are about it.
		{
			Zenith_EntityID xMenuID = INVALID_ENTITY_ID;
			if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuID))
			{
				g_bMASkipped = true;
				Zenith_AutomatedTestRunner::RequestSkip(
					"[ZM_MilestoneAutosave] no persistent ZM_UI_MenuStack singleton -- "
					"FrontEnd.zscen has not been baked, so there is no pause menu to open");
				return;
			}
		}

		Zenith_InputSimulator::ResetAllInputState();
		ZM_SaveSlots::SetTestSlotNamesForTests(true);
		g_bMAAliasesActive = MATestSlotNamesActive();
		if (!g_bMAAliasesActive)
		{
			// SC5 precedent: an alias failure is a hard FAIL and ALL disk access
			// is refused from here on -- never a skip, because running further
			// would address the developer's real save files.
			MAFail("_Test slot aliases did not activate; refusing all disk access");
			g_bMAActive = true;
			return;
		}
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		MAResetSlotOperationTrace();
		ZM_SaveSlots::SetOperationObserverForTests(&MAObserveSlotOperation);
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		Zenith_InputSimulator::SetFixedDt(fMA_FIXED_DT);
		// Dawnmere is loaded DIRECTLY, exactly like the SC3 tests: a raw load is
		// NOT an arrival, latches no autosave and records no spawn tag, so the
		// first warp below is the first arrival this session sees.
		g_xEngine.Scenes().LoadSceneByIndex(iMA_DAWNMERE_BUILD, SCENE_LOAD_SINGLE);
		g_eMAPhase = MAPhase::AwaitDawnmere;
		g_bMAActive = true;
	}

	// ---- Assertion batteries -------------------------------------------------
	// Each battery is its OWN file-local function and none holds more than ONE
	// ZM_GameState local -- the same load-bearing stack rule as the per-phase
	// drivers (see the comment above Step). Within a battery the observer-trace
	// counts are ALWAYS taken FIRST: the ProbeSlot / ReadState calls a battery
	// then makes append to the same trace.

	void MASamplePositiveBattery()
	{
		// The whole-window trace: from the install's trace reset to HERE, the
		// only slot operation that may have run is the drain's ONE WriteState on
		// AUTO (its verify re-probe is a PROBE_SLOT, which is not counted here).
		// A second WRITE_STATE -- or one on any MANUAL slot -- reds this flag.
		g_bMAPositiveTraceExact = !g_bMASlotOperationOverflow
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_WRITE_STATE) == 1u
			&& MACountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_WRITE_STATE,
				ZM_SAVE_SLOT_AUTO) == 1u
			&& MACountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_WRITE_STATE,
				ZM_SAVE_SLOT_0) == 0u
			&& MACountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_WRITE_STATE,
				ZM_SAVE_SLOT_1) == 0u
			&& MACountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_WRITE_STATE,
				ZM_SAVE_SLOT_2) == 0u;
		g_bMAPositiveAutoReady =
			ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_READY;
		ZM_GameState xCandidate;
		g_bMAPositiveReadOk =
			ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_AUTO, xCandidate).IsOk();
		if (!g_bMAPositiveReadOk) { return; }
		Zenith_Maths::Vector3 xLiveCentre(0.0f);
		float fLiveYaw = 0.0f;
		ZM_GameState* pxLive = nullptr;
		if (!MAReadPlayerPose(xLiveCentre, fLiveYaw)
			|| !ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr)
		{
			return;
		}
		const Zenith_Maths::Vector3 xCandidateCentre(
			xCandidate.m_xWorldPosition.m_afPosition[0],
			xCandidate.m_xWorldPosition.m_afPosition[1],
			xCandidate.m_xWorldPosition.m_afPosition[2]);
		g_fMAPositivePlanarError = MAPlanarDistance(xCandidateCentre, xLiveCentre);
		g_fMAPositiveVerticalError = std::fabs(xCandidateCentre.y - xLiveCentre.y);
		g_fMAPositiveYawError = std::fabs(
			MAWrappedAngleDifference(xCandidate.m_xWorldPosition.m_fYaw, fLiveYaw));
		g_bMAPositivePoseMatch = g_fMAPositivePlanarError <= fMA_MAX_PLANAR_ERROR
			&& g_fMAPositiveVerticalError <= fMA_MAX_VERTICAL_ERROR
			&& g_fMAPositiveYawError <= fMA_MAX_YAW_ERROR;
		g_bMAPositiveSceneTag =
			xCandidate.m_xWorldPosition.m_uSceneBuildIndex
				== static_cast<u_int>(iMA_DAWNMERE_BUILD)
			&& std::strcmp(xCandidate.m_xWorldPosition.m_szSpawnTag,
				szMA_TOWNCENTER_TAG) == 0;
		// The distinctive-content proof: the disk candidate carries the
		// INSTALLED fields, so the write demonstrably serialized THIS test's
		// state rather than whatever the manager happened to hold. Get(1u) is
		// short-circuit-guarded by the count check -- the party Get asserts on
		// an out-of-range index in EVERY configuration.
		g_bMAPositiveFieldsMatch = xCandidate.m_uMoney == uMA_DISTINCTIVE_MONEY
			&& ZM_IsStoryFlagSet(xCandidate, ZM_STORY_FLAG_MET_PROFESSOR)
			&& xCandidate.m_xParty.Count() == 2u
			&& xCandidate.m_xParty.Get(1u).m_eSpecies == ZM_SPECIES_NIBBIN
			&& std::strcmp(xCandidate.m_xParty.Get(1u).m_szNickname,
				szMA_PROOF_NICKNAME) == 0;
		const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(
			xCandidate.m_xWorldPosition,
			ZM_SpawnPoint::IsTagValid(xCandidate.m_xWorldPosition.m_szSpawnTag));
		g_bMAPositiveResumeValid = eValidity == ZM_RESUME_VALID
			&& ZM_CanResume(eValidity);
		g_bMAPositiveCanonicalLive = MACanonicalEqual(xCandidate, *pxLive);
		// THE capture-into-live proof. The install left the live world position
		// UNSET; only ZM_TryAutosave's CaptureWorldPosition(*pxState) call can
		// have filled it. Drop that call and this scene stays UNSET -- along
		// with the disk candidate's scene and the resume validity above. This
		// is the assertion that reds the mutation today's green hole hides.
		g_bMAPositiveLiveSceneCaptured =
			pxLive->m_xWorldPosition.m_uSceneBuildIndex
				== static_cast<u_int>(iMA_DAWNMERE_BUILD);
	}

	void MASampleBlockedBattery()
	{
		// RAW samples first. The counter baseline was taken as the blocked
		// window opened; the trace has not been reset since, so these counts
		// span the whole menu-open warp AND the drain's refused attempt.
		g_uMACounterAfterBlocked = ZM_GetAutosaveCount();
		g_bMABlockedCounterUnchanged =
			g_uMACounterAfterBlocked == g_uMACounterBeforeBlocked;
		g_bMABlockedTraceClean = !g_bMASlotOperationOverflow
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_WRITE_STATE) == 0u
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_READ_STATE) == 0u;
		// The disk image, read DIRECTLY (no slot-layer call), must be
		// byte-identical to the pre-warp snapshot. This is the assertion a
		// dropped IsMenuOpen consult cannot dodge: the drain's write would
		// rewrite the file even if every counter were faked.
		Zenith_Vector<u_int8> xNow;
		g_bMABlockedBytesIdentical = MAReadSlotBytes(ZM_SAVE_SLOT_AUTO, xNow)
			&& MABytesEqual(g_xMAAutoBytesBeforeBlocked, xNow);
		ZM_UI_MenuStack* pxMenu = MAResolveMenu();
		g_bMABlockedMenuOpenAtSample = pxMenu != nullptr
			&& pxMenu->IsOpen()
			&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT;
		// The two controls that make the refusal attributable to the menu term
		// ALONE on this IDLE frame: no save blocker is live, and a capture
		// would have succeeded (into a THROWAWAY state, never the live one).
		g_bMABlockedBlockerNone = ZM_SaveSlots::ResolveLiveSaveBlocker()
			== ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
		MAResetGameState(g_xMAThrowaway);
		g_bMABlockedCaptureWouldWork =
			ZM_GameStateManager::CaptureWorldPosition(g_xMAThrowaway);
	}

	void MASampleReArmBattery()
	{
		// The re-arm window's trace was reset as the third warp was requested:
		// exactly ONE WRITE_STATE, on AUTO. A one-shot-per-process latch never
		// reaches this battery at all (the counter gate above it deadlines).
		g_bMAReArmTraceExact = !g_bMASlotOperationOverflow
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_WRITE_STATE) == 1u
			&& MACountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_WRITE_STATE,
				ZM_SAVE_SLOT_AUTO) == 1u;
		g_bMAReArmAutoReady =
			ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_READY;
		ZM_GameState xCandidate;
		g_bMAReArmReadOk =
			ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_AUTO, xCandidate).IsOk();
		if (!g_bMAReArmReadOk) { return; }
		ZM_GameState* pxLive = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr)
		{
			return;
		}
		g_bMAReArmCanonicalLive = MACanonicalEqual(xCandidate, *pxLive);
		g_bMAReArmSceneTag =
			xCandidate.m_xWorldPosition.m_uSceneBuildIndex
				== static_cast<u_int>(iMA_DAWNMERE_BUILD)
			&& std::strcmp(xCandidate.m_xWorldPosition.m_szSpawnTag,
				szMA_TOWNCENTER_TAG) == 0;
	}

	// Builds the distinctive install into g_xMADistinctive. Owns the ONE
	// ZM_GameState temporary of its frame (the starter factory return).
	void MABuildDistinctive()
	{
		g_xMADistinctive = ZM_MakeNewGameState();
		ZM_ApplyStarterChoice(g_xMADistinctive, ZM_STARTER_CHOICE_FERNFAWN);
		g_xMADistinctive.m_uMoney = uMA_DISTINCTIVE_MONEY;
		ZM_SetStoryFlag(g_xMADistinctive, ZM_STORY_FLAG_MET_PROFESSOR, true);
		ZM_Monster xProof = ZM_BuildMonsterRecord(ZM_SPECIES_NIBBIN, 9u);
		std::snprintf(xProof.m_szNickname, sizeof(xProof.m_szNickname), "%s",
			szMA_PROOF_NICKNAME);
		const bool bAdded = g_xMADistinctive.m_xParty.Add(xProof);
		// The transient whiteout latch stays false so only the warp below owns
		// the next transition, and the world position is DELIBERATELY left
		// UNSET (the starter default): the arrival drain's CaptureWorldPosition
		// must be the thing that fills it, which is what makes the
		// capture-into-live assertion below red-able.
		g_xMADistinctive.m_bPendingWhiteout = false;
		g_bMADistinctiveBuilt = bAdded
			&& g_xMADistinctive.m_xParty.Count() == 2u
			&& ZM_IsStoryFlagSet(g_xMADistinctive, ZM_STORY_FLAG_MET_PROFESSOR)
			&& g_xMADistinctive.m_xWorldPosition.m_uSceneBuildIndex
				== uZM_WORLD_SCENE_UNSET;
	}

	// ---- Per-phase drivers ---------------------------------------------------
	// Every phase body lives in its OWN file-local function and
	// Step_ZMMilestoneAutosave is a THIN dispatch over them. This is
	// load-bearing, not stylistic (copied from ZM_AutoTests_SaveContinue.cpp):
	// in a Debug (/Od) build the compiler reserves a distinct stack slot for
	// practically every local in a function, and a single ZM_GameState is on
	// the order of 100-200 KB (6-mon party + 16x30 box storage). With all 12
	// phase bodies inline, Step's ONE frame would aggregate well over 1 MB of
	// locals and overflow the exe's 1 MB main-thread stack reserve inside
	// __chkstk. Split per phase, no frame holds more than ONE ZM_GameState
	// value -- most hold none. Each function reads and writes the same
	// file-scope phase/latch globals, returns true to keep stepping and false
	// to stop, and the shared ++g_iMAPhaseFrames stays in the dispatch.

	bool MAPhaseAwaitDawnmere()
	{
		if (MADawnmereReady())
		{
			g_bMADawnmereReady = true;
			g_eMAPhase = MAPhase::InstallDistinctive;
			g_iMAPhaseFrames = 0; return true;
		}
		if (g_iMAPhaseFrames > iMA_DAWNMERE_DEADLINE)
		{
			MAFail("Dawnmere never became playable after the direct Setup load "
				"(scene / unique player / valid body / grounded / IDLE warp machine)");
			return false;
		}
		return true;
	}

	bool MAPhaseInstallDistinctive()
	{
		ZM_GameState* pxLive = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr)
		{
			MAFail("persistent live state was unavailable for the distinctive install");
			return false;
		}
		// The pre-install state must DIFFER from the install, or every
		// downstream "the autosave carried the installed state" assertion could
		// pass on data the manager already held. Money and party size are the
		// cheap witnesses; the canonical compare in SamplePositive is the
		// complete one.
		g_uMAPreInstallMoney = pxLive->m_uMoney;
		g_uMAPreInstallPartyCount = pxLive->m_xParty.Count();
		g_bMAPreInstallDistinctive = g_uMAPreInstallMoney != uMA_DISTINCTIVE_MONEY
			&& g_uMAPreInstallPartyCount == 1u;
		MABuildDistinctive();
		*pxLive = g_xMADistinctive;
		// Codec-canonical verification of the install: the live state now IS
		// the distinctive state (the codec accepts the UNSET world position),
		// and the live position really is still UNSET -- the drain's capture
		// has not run yet, so nothing may have filled it.
		g_bMAInstallVerified = g_bMADistinctiveBuilt
			&& MACanonicalEqual(*pxLive, g_xMADistinctive)
			&& pxLive->m_xWorldPosition.m_uSceneBuildIndex == uZM_WORLD_SCENE_UNSET;
		if (!g_bMAPreInstallDistinctive || !g_bMAInstallVerified)
		{
			MAFail("could not install and verify the distinctive live state");
			return false;
		}
		// Snapshot the counter IMMEDIATELY before the warp request: the arrival
		// tail latches the autosave and the drain runs on a LATER IDLE frame, so
		// the delta sampled in SamplePositive belongs to THIS warp and to
		// nothing earlier (a stale latch inherited from a previous batched test
		// would have drained during the many IDLE frames AwaitDawnmere spent).
		g_uMACounterBeforePositive = ZM_GetAutosaveCount();
		MAResetSlotOperationTrace();
		g_bMAWarpRequested = ZM_GameStateManager::RequestWarp(
			static_cast<u_int>(iMA_DAWNMERE_BUILD), szMA_TOWNCENTER_TAG);
		if (!g_bMAWarpRequested)
		{
			MAFail("RequestWarp(Dawnmere, TownCenter) was refused from a settled, "
				"IDLE Dawnmere carrying the distinctive state");
			return false;
		}
		g_eMAPhase = MAPhase::PositiveArrival;
		g_iMAPhaseFrames = 0; return true;
	}

	bool MAPhasePositiveArrival()
	{
		ZM_GameStateManager* pxManager = MAResolveManager();
		if (pxManager != nullptr)
		{
			g_bMASawPositiveWarpRun |=
				pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE;
		}
		if (MAWarpCompletedToDawnmere(g_bMASawPositiveWarpRun))
		{
			g_bMAPositiveArrived = true;
			g_eMAPhase = MAPhase::SamplePositive;
			g_iMAPhaseFrames = 0; return true;
		}
		if (g_iMAPhaseFrames > iMA_POSITIVE_WARP_DEADLINE)
		{
			MAFail("the distinctive-state arrival warp never completed back to an "
				"IDLE machine with a grounded player");
			return false;
		}
		return true;
	}

	bool MAPhaseSamplePositive()
	{
		// Poll up to 12 settled frames for the drain to land the write (the
		// drain runs inside the scene update AFTER this Step, so the +1 cannot
		// be assumed on the arrival frame itself), then run the battery on the
		// frame it is observed -- the "+1" of the budget.
		if (ZM_GetAutosaveCount() != g_uMACounterBeforePositive + 1u)
		{
			if (g_iMAPhaseFrames > iMA_POSITIVE_DRAIN_POLLS)
			{
				MAFail("the arrival drain never landed the milestone autosave "
					"(the counter stayed flat for 12 settled frames)");
				return false;
			}
			return true;
		}
		g_uMACounterAfterPositive = ZM_GetAutosaveCount();
		g_bMAPositiveCounterDelta =
			g_uMACounterAfterPositive == g_uMACounterBeforePositive + 1u;
		MASamplePositiveBattery();
		g_eMAPhase = MAPhase::OpenMenu;
		g_iMAPhaseFrames = 0; return true;
	}

	bool MAPhaseOpenMenu()
	{
		ZM_UI_MenuStack* pxMenu = MAResolveMenu();
		if (pxMenu != nullptr && pxMenu->IsOpen()
			&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
		{
			g_bMAMenuOpened = true;
			g_eMAPhase = MAPhase::MenuOpenProbe;
			g_iMAPhaseFrames = 0; return true;
		}
		if (g_iMAPhaseFrames > iMA_MENU_OPEN_DEADLINE)
		{
			MAFail("the M press never opened the ROOT pause menu over a settled "
				"Dawnmere");
			return false;
		}
		if ((g_iMAPhaseFrames % 8) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
		}
		return true;
	}

	bool MAPhaseMenuOpenProbe()
	{
		if (g_iMAPhaseFrames < iMA_MENU_PROBE_SETTLE_FRAMES) { return true; }
		ZM_UI_MenuStack* pxMenu = MAResolveMenu();
		if (pxMenu == nullptr || !pxMenu->IsOpen()
			|| pxMenu->GetTopScreen() != ZM_MENU_SCREEN_ROOT)
		{
			MAFail("the ROOT menu did not stay open for the wiring probe");
			return false;
		}
		// THE MENU-WIRING PROBE. On this frame the blocker resolves NONE and the
		// throwaway capture SUCCEEDS, so the menu-open term of ZM_ShouldAutosave
		// is the ONLY thing that can refuse the direct call below -- this is the
		// one place the IsMenuOpen() consult inside ZM_TryAutosave is pinned at
		// the integration boundary (the pure units pin the predicate, never the
		// consult). Drop the consult and all three of: the FALSE return, the +0
		// counter delta and the empty WRITE_STATE trace red at once.
		g_bMAMenuProbeBlockerNone = ZM_SaveSlots::ResolveLiveSaveBlocker()
			== ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
		MAResetGameState(g_xMAThrowaway);
		g_bMAMenuProbeCaptureWouldWork =
			ZM_GameStateManager::CaptureWorldPosition(g_xMAThrowaway);
		MAResetSlotOperationTrace();
		g_uMACounterBeforeMenuProbe = ZM_GetAutosaveCount();
		g_bMAMenuProbeRefused =
			!ZM_TryAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED);
		g_uMACounterAfterMenuProbe = ZM_GetAutosaveCount();
		g_bMAMenuProbeCounterUnchanged =
			g_uMACounterAfterMenuProbe == g_uMACounterBeforeMenuProbe;
		g_bMAMenuProbeTraceNoWrite = !g_bMASlotOperationOverflow
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_WRITE_STATE) == 0u;
		g_eMAPhase = MAPhase::BlockedArrival;
		g_iMAPhaseFrames = 0; return true;
	}

	bool MAPhaseBlockedArrival()
	{
		if (g_iMAPhaseFrames == 1)
		{
			// The blocked window OPENS: snapshot the Auto slot's raw bytes by
			// direct file read (no slot-layer call, so nothing is appended to
			// the trace the window is about to count), reset the trace, take
			// the RAW counter baseline, and request the SAME warp again WITH
			// THE MENU STILL OPEN.
			ZM_UI_MenuStack* pxMenu = MAResolveMenu();
			if (pxMenu == nullptr || !pxMenu->IsOpen())
			{
				MAFail("the menu closed before the blocked warp could be requested "
					"with it open");
				return false;
			}
			g_bMABlockedBytesSnapshot =
				MAReadSlotBytes(ZM_SAVE_SLOT_AUTO, g_xMAAutoBytesBeforeBlocked);
			if (!g_bMABlockedBytesSnapshot)
			{
				MAFail("could not snapshot the Auto slot bytes before the blocked "
					"warp");
				return false;
			}
			MAResetSlotOperationTrace();
			g_uMACounterBeforeBlocked = ZM_GetAutosaveCount();
			g_bMABlockedWarpRequested = ZM_GameStateManager::RequestWarp(
				static_cast<u_int>(iMA_DAWNMERE_BUILD), szMA_TOWNCENTER_TAG);
			if (!g_bMABlockedWarpRequested)
			{
				MAFail("RequestWarp was refused with the ROOT pause menu open");
				return false;
			}
			return true;
		}
		ZM_GameStateManager* pxManager = MAResolveManager();
		if (pxManager != nullptr)
		{
			g_bMASawBlockedWarpRun |=
				pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE;
		}
		if (MAWarpCompletedToDawnmere(g_bMASawBlockedWarpRun))
		{
			// THE ATTRIBUTION LATCH: the menu must STILL own the screen. If a
			// future change force-closes ROOT on warp, the drain's refusal is no
			// longer attributable to the menu-open term and this test must say
			// so BY NAME rather than silently keep passing.
			ZM_UI_MenuStack* pxMenu = MAResolveMenu();
			g_bMABlockedMenuStillOpen = pxMenu != nullptr
				&& pxMenu->IsOpen()
				&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT;
			if (!g_bMABlockedMenuStillOpen)
			{
				MAFail("the warp force-closed the ROOT menu -- the blocked-window "
					"refusal can no longer be attributed to the menu-open term");
				return false;
			}
			g_bMABlockedArrived = true;
			g_eMAPhase = MAPhase::SampleBlocked;
			g_iMAPhaseFrames = 0; return true;
		}
		if (g_iMAPhaseFrames > iMA_BLOCKED_WARP_DEADLINE)
		{
			MAFail("the menu-open warp never completed back to an IDLE machine "
				"with a grounded player");
			return false;
		}
		return true;
	}

	bool MAPhaseSampleBlocked()
	{
		// Four settled frames: the drain consumes the latch and is refused on
		// the scene-update half of the first IDLE frame after the arrival tail,
		// so by frame 4 its (deliberately traceless) attempt is behind us.
		if (g_iMAPhaseFrames < iMA_BLOCKED_SETTLE_FRAMES) { return true; }
		MASampleBlockedBattery();
		g_eMAPhase = MAPhase::CloseMenu;
		g_iMAPhaseFrames = 0; return true;
	}

	bool MAPhaseCloseMenu()
	{
		ZM_UI_MenuStack* pxMenu = MAResolveMenu();
		if (pxMenu != nullptr && !pxMenu->IsOpen())
		{
			g_bMAMenuClosed = true;
			g_eMAPhase = MAPhase::NoRetryWatch;
			g_iMAPhaseFrames = 0; return true;
		}
		if (g_iMAPhaseFrames > iMA_MENU_CLOSE_DEADLINE)
		{
			MAFail("ESCAPE never closed the ROOT pause menu");
			return false;
		}
		if ((g_iMAPhaseFrames % 8) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		}
		return true;
	}

	bool MAPhaseNoRetryWatch()
	{
		// 30 IDLE menu-closed frames. If the drain were CONSUME-AFTER-SUCCESS,
		// the latch it failed to spend under the open menu would still be set
		// and would fire the moment the menu term cleared -- this window is
		// where a retry-on-refusal implementation MUST show up, and where the
		// shipped consume-before-attempt drain provably never does.
		ZM_GameStateManager* pxManager = MAResolveManager();
		if (pxManager == nullptr
			|| pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE)
		{
			MAFail("the warp machine went non-IDLE during the 30-frame no-retry "
				"watch -- something re-armed a transition after the menu closed");
			return false;
		}
		if (g_iMAPhaseFrames < iMA_NORETRY_WATCH_FRAMES) { return true; }
		g_bMANoRetryStayedIdle = true;
		// The MenuOpenProbe baseline subsumes the whole blocked window: the
		// counter must STILL equal it here. Printed raw in Verify.
		g_uMACounterAfterNoRetry = ZM_GetAutosaveCount();
		g_bMANoRetryCounterStill =
			g_uMACounterAfterNoRetry == g_uMACounterBeforeMenuProbe;
		// No trace reset since the blocked window opened: these counts span the
		// blocked warp, the drain's refused attempt, the menu close AND this
		// watch.
		g_bMANoRetryTraceStill = !g_bMASlotOperationOverflow
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_WRITE_STATE) == 0u
			&& MACountSlotOperations(ZM_SAVE_SLOT_OPERATION_READ_STATE) == 0u;
		Zenith_Vector<u_int8> xNow;
		g_bMANoRetryBytesStill = MAReadSlotBytes(ZM_SAVE_SLOT_AUTO, xNow)
			&& MABytesEqual(g_xMAAutoBytesBeforeBlocked, xNow);
		g_eMAPhase = MAPhase::ReArmArrival;
		g_iMAPhaseFrames = 0; return true;
	}

	bool MAPhaseReArmArrival()
	{
		if (g_iMAPhaseFrames == 1)
		{
			// THE RE-ARM: the SAME warp a third time, from a settled IDLE
			// menu-closed Dawnmere. A one-shot-per-process latch never fires
			// again and SampleReArm deadlines by name; a latch that re-arms
			// lands a SECOND autosave exactly like the first.
			MAResetSlotOperationTrace();
			g_uMACounterBeforeReArm = ZM_GetAutosaveCount();
			g_bMAReArmWarpRequested = ZM_GameStateManager::RequestWarp(
				static_cast<u_int>(iMA_DAWNMERE_BUILD), szMA_TOWNCENTER_TAG);
			if (!g_bMAReArmWarpRequested)
			{
				MAFail("the re-arm warp was refused from a settled menu-closed "
					"Dawnmere");
				return false;
			}
			return true;
		}
		ZM_GameStateManager* pxManager = MAResolveManager();
		if (pxManager != nullptr)
		{
			g_bMASawReArmWarpRun |=
				pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE;
		}
		if (MAWarpCompletedToDawnmere(g_bMASawReArmWarpRun))
		{
			g_bMAReArmArrived = true;
			g_eMAPhase = MAPhase::SampleReArm;
			g_iMAPhaseFrames = 0; return true;
		}
		if (g_iMAPhaseFrames > iMA_REARM_WARP_DEADLINE)
		{
			MAFail("the re-arm warp never completed back to an IDLE machine with "
				"a grounded player");
			return false;
		}
		return true;
	}

	bool MAPhaseSampleReArm()
	{
		if (ZM_GetAutosaveCount() != g_uMACounterBeforeReArm + 1u)
		{
			if (g_iMAPhaseFrames > iMA_REARM_DRAIN_POLLS)
			{
				MAFail("the re-armed arrival drain never landed the second "
					"autosave (the counter stayed flat for 12 settled frames) -- "
					"the arrival latch did not re-arm");
				return false;
			}
			return true;
		}
		g_uMACounterAfterReArm = ZM_GetAutosaveCount();
		g_bMAReArmCounterDelta =
			g_uMACounterAfterReArm == g_uMACounterBeforeReArm + 1u;
		MASampleReArmBattery();
		g_eMAPhase = MAPhase::Done;
		return false;
	}

	bool Step_ZMMilestoneAutosave(int)
	{
		if (!g_bMAActive || g_bMAFailed || g_eMAPhase == MAPhase::Done) { return false; }
		++g_iMAPhaseFrames;

		switch (g_eMAPhase)
		{
		case MAPhase::AwaitDawnmere:      return MAPhaseAwaitDawnmere();
		case MAPhase::InstallDistinctive: return MAPhaseInstallDistinctive();
		case MAPhase::PositiveArrival:    return MAPhasePositiveArrival();
		case MAPhase::SamplePositive:     return MAPhaseSamplePositive();
		case MAPhase::OpenMenu:           return MAPhaseOpenMenu();
		case MAPhase::MenuOpenProbe:      return MAPhaseMenuOpenProbe();
		case MAPhase::BlockedArrival:     return MAPhaseBlockedArrival();
		case MAPhase::SampleBlocked:      return MAPhaseSampleBlocked();
		case MAPhase::CloseMenu:          return MAPhaseCloseMenu();
		case MAPhase::NoRetryWatch:       return MAPhaseNoRetryWatch();
		case MAPhase::ReArmArrival:       return MAPhaseReArmArrival();
		case MAPhase::SampleReArm:        return MAPhaseSampleReArm();
		case MAPhase::Done:
			return false;
		}
		return false;
	}
}

namespace
{
	bool Verify_ZMMilestoneAutosave()
	{
		if (g_bMASkipped) { return true; }

		const bool bInstall = g_bMADawnmereReady && g_bMADistinctiveBuilt
			&& g_bMAPreInstallDistinctive && g_bMAInstallVerified && g_bMAWarpRequested;
		const bool bPositive = g_bMASawPositiveWarpRun && g_bMAPositiveArrived
			&& g_bMAPositiveCounterDelta && g_bMAPositiveTraceExact
			&& g_bMAPositiveAutoReady && g_bMAPositiveReadOk
			&& g_bMAPositiveSceneTag && g_bMAPositivePoseMatch
			&& g_bMAPositiveFieldsMatch && g_bMAPositiveResumeValid
			&& g_bMAPositiveCanonicalLive && g_bMAPositiveLiveSceneCaptured;
		const bool bMenuProbe = g_bMAMenuOpened && g_bMAMenuProbeBlockerNone
			&& g_bMAMenuProbeCaptureWouldWork && g_bMAMenuProbeRefused
			&& g_bMAMenuProbeCounterUnchanged && g_bMAMenuProbeTraceNoWrite;
		const bool bBlocked = g_bMABlockedBytesSnapshot && g_bMABlockedWarpRequested
			&& g_bMASawBlockedWarpRun && g_bMABlockedArrived
			&& g_bMABlockedMenuStillOpen && g_bMABlockedCounterUnchanged
			&& g_bMABlockedTraceClean && g_bMABlockedBytesIdentical
			&& g_bMABlockedMenuOpenAtSample && g_bMABlockedBlockerNone
			&& g_bMABlockedCaptureWouldWork;
		const bool bNoRetry = g_bMAMenuClosed && g_bMANoRetryStayedIdle
			&& g_bMANoRetryCounterStill && g_bMANoRetryTraceStill
			&& g_bMANoRetryBytesStill;
		const bool bReArm = g_bMAReArmWarpRequested && g_bMASawReArmWarpRun
			&& g_bMAReArmArrived && g_bMAReArmCounterDelta && g_bMAReArmTraceExact
			&& g_bMAReArmAutoReady && g_bMAReArmReadOk
			&& g_bMAReArmCanonicalLive && g_bMAReArmSceneTag;
		const bool bPassed = !g_bMAFailed && g_bMAAliasesActive && bInstall
			&& bPositive && bMenuProbe && bBlocked && bNoRetry && bReArm;

		// Teardown precedes every diagnostic/return and runs on EVERY exit path,
		// including a mid-phase failure. Delete while the verified aliases are
		// still active; only then clear instrumentation and restore shipping
		// names. The menu reset runs whether or not the menu is open -- a
		// failure path can leave it up.
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_M);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_ESCAPE);
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		ZM_SaveSlots::SetOperationObserverForTests(nullptr);
		if (g_bMAAliasesActive)
		{
			ZM_SaveSlots::DeleteAllSlotsForTests();
			Zenith_SaveData::ClearForTest();
		}
		ZM_SaveSlots::SetTestSlotNamesForTests(false);
		g_xEngine.Scenes().LoadSceneByIndex(iMA_FRONTEND_BUILD, SCENE_LOAD_SINGLE);
		g_bMAActive = false;

		// Everything captured, on one line, so a failure is fully localisable
		// from the log alone without a rebuild.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_MilestoneAutosave] failed=%s (%s) aliases=%s | dawnmere=%s "
			"built=%s preInstall=%s (money %u->%u party %u->2) installed=%s "
			"warpReq=%s | sawWarp=%s arrived=%s counter %u->%u (want +1) "
			"traceExact=%s autoReady=%s readOk=%s sceneTag=%s pose=%s "
			"(planar=%.4f vertical=%.4f yaw=%.4f) fields=%s resumeValid=%s "
			"canonicalLive=%s liveSceneCaptured=%s | menuOpen=%s blockerNone=%s "
			"captureOk=%s refused=%s probeCounter %u->%u (want +0) "
			"probeNoWrite=%s | blockedSnap=%s blockedReq=%s sawBlocked=%s "
			"blockedArrived=%s menuStillOpen=%s blockedCounter %u->%u (want +0) "
			"traceClean=%s bytesSame=%s menuOpenAtSample=%s blockedBlockerNone=%s "
			"blockedCaptureOk=%s | closed=%s stayedIdle=%s noRetryCounter %u->%u "
			"(want +0) noRetryTrace=%s noRetryBytes=%s | reArmReq=%s sawReArm=%s "
			"reArmArrived=%s reArmCounter %u->%u (want +1) reArmTrace=%s "
			"reArmReady=%s reArmRead=%s reArmCanonical=%s reArmSceneTag=%s",
			g_bMAFailed ? "true" : "false", g_szMAFailure,
			g_bMAAliasesActive ? "true" : "false",
			g_bMADawnmereReady ? "true" : "false",
			g_bMADistinctiveBuilt ? "true" : "false",
			g_bMAPreInstallDistinctive ? "true" : "false",
			g_uMAPreInstallMoney, uMA_DISTINCTIVE_MONEY, g_uMAPreInstallPartyCount,
			g_bMAInstallVerified ? "true" : "false",
			g_bMAWarpRequested ? "true" : "false",
			g_bMASawPositiveWarpRun ? "true" : "false",
			g_bMAPositiveArrived ? "true" : "false",
			g_uMACounterBeforePositive, g_uMACounterAfterPositive,
			g_bMAPositiveTraceExact ? "true" : "false",
			g_bMAPositiveAutoReady ? "true" : "false",
			g_bMAPositiveReadOk ? "true" : "false",
			g_bMAPositiveSceneTag ? "true" : "false",
			g_bMAPositivePoseMatch ? "true" : "false",
			(double)g_fMAPositivePlanarError, (double)g_fMAPositiveVerticalError,
			(double)g_fMAPositiveYawError,
			g_bMAPositiveFieldsMatch ? "true" : "false",
			g_bMAPositiveResumeValid ? "true" : "false",
			g_bMAPositiveCanonicalLive ? "true" : "false",
			g_bMAPositiveLiveSceneCaptured ? "true" : "false",
			g_bMAMenuOpened ? "true" : "false",
			g_bMAMenuProbeBlockerNone ? "true" : "false",
			g_bMAMenuProbeCaptureWouldWork ? "true" : "false",
			g_bMAMenuProbeRefused ? "true" : "false",
			g_uMACounterBeforeMenuProbe, g_uMACounterAfterMenuProbe,
			g_bMAMenuProbeTraceNoWrite ? "true" : "false",
			g_bMABlockedBytesSnapshot ? "true" : "false",
			g_bMABlockedWarpRequested ? "true" : "false",
			g_bMASawBlockedWarpRun ? "true" : "false",
			g_bMABlockedArrived ? "true" : "false",
			g_bMABlockedMenuStillOpen ? "true" : "false",
			g_uMACounterBeforeBlocked, g_uMACounterAfterBlocked,
			g_bMABlockedTraceClean ? "true" : "false",
			g_bMABlockedBytesIdentical ? "true" : "false",
			g_bMABlockedMenuOpenAtSample ? "true" : "false",
			g_bMABlockedBlockerNone ? "true" : "false",
			g_bMABlockedCaptureWouldWork ? "true" : "false",
			g_bMAMenuClosed ? "true" : "false",
			g_bMANoRetryStayedIdle ? "true" : "false",
			g_uMACounterBeforeMenuProbe, g_uMACounterAfterNoRetry,
			g_bMANoRetryTraceStill ? "true" : "false",
			g_bMANoRetryBytesStill ? "true" : "false",
			g_bMAReArmWarpRequested ? "true" : "false",
			g_bMASawReArmWarpRun ? "true" : "false",
			g_bMAReArmArrived ? "true" : "false",
			g_uMACounterBeforeReArm, g_uMACounterAfterReArm,
			g_bMAReArmTraceExact ? "true" : "false",
			g_bMAReArmAutoReady ? "true" : "false",
			g_bMAReArmReadOk ? "true" : "false",
			g_bMAReArmCanonicalLive ? "true" : "false",
			g_bMAReArmSceneTag ? "true" : "false");

		if (g_bMAFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_MilestoneAutosave] %s",
				g_szMAFailure);
		}
		if (!bInstall)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_MilestoneAutosave] dawnmere/install failed "
				"(ready/built/preInstall/installed/warpReq=%s/%s/%s/%s/%s)",
				g_bMADawnmereReady ? "true" : "false",
				g_bMADistinctiveBuilt ? "true" : "false",
				g_bMAPreInstallDistinctive ? "true" : "false",
				g_bMAInstallVerified ? "true" : "false",
				g_bMAWarpRequested ? "true" : "false");
		}
		if (!bPositive)
		{
			// The drop-capture mutation reds liveSceneCaptured, sceneTag and
			// resumeValid together; a double-WriteState reds traceExact alone.
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_MilestoneAutosave] positive-window failed "
				"(sawWarp/arrived/delta/trace/ready/read/sceneTag/pose/fields/"
				"resume/canonical/liveScene=%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bMASawPositiveWarpRun ? "true" : "false",
				g_bMAPositiveArrived ? "true" : "false",
				g_bMAPositiveCounterDelta ? "true" : "false",
				g_bMAPositiveTraceExact ? "true" : "false",
				g_bMAPositiveAutoReady ? "true" : "false",
				g_bMAPositiveReadOk ? "true" : "false",
				g_bMAPositiveSceneTag ? "true" : "false",
				g_bMAPositivePoseMatch ? "true" : "false",
				g_bMAPositiveFieldsMatch ? "true" : "false",
				g_bMAPositiveResumeValid ? "true" : "false",
				g_bMAPositiveCanonicalLive ? "true" : "false",
				g_bMAPositiveLiveSceneCaptured ? "true" : "false");
		}
		if (!bMenuProbe)
		{
			// The drop-menu-consult mutation reds refused, the +0 delta and
			// probeNoWrite together while BOTH controls stay green.
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_MilestoneAutosave] menu-open wiring probe failed "
				"(opened/blockerNone/captureOk/refused/delta/noWrite="
				"%s/%s/%s/%s/%s/%s)",
				g_bMAMenuOpened ? "true" : "false",
				g_bMAMenuProbeBlockerNone ? "true" : "false",
				g_bMAMenuProbeCaptureWouldWork ? "true" : "false",
				g_bMAMenuProbeRefused ? "true" : "false",
				g_bMAMenuProbeCounterUnchanged ? "true" : "false",
				g_bMAMenuProbeTraceNoWrite ? "true" : "false");
		}
		if (!bBlocked)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_MilestoneAutosave] blocked-window failed "
				"(snap/req/saw/arrived/menuOpen/delta/trace/bytes/menuAtSample/"
				"blockerNone/captureOk=%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bMABlockedBytesSnapshot ? "true" : "false",
				g_bMABlockedWarpRequested ? "true" : "false",
				g_bMASawBlockedWarpRun ? "true" : "false",
				g_bMABlockedArrived ? "true" : "false",
				g_bMABlockedMenuStillOpen ? "true" : "false",
				g_bMABlockedCounterUnchanged ? "true" : "false",
				g_bMABlockedTraceClean ? "true" : "false",
				g_bMABlockedBytesIdentical ? "true" : "false",
				g_bMABlockedMenuOpenAtSample ? "true" : "false",
				g_bMABlockedBlockerNone ? "true" : "false",
				g_bMABlockedCaptureWouldWork ? "true" : "false");
		}
		if (!bNoRetry)
		{
			// A retry-on-refusal drain reds noRetryCounter, noRetryTrace and
			// noRetryBytes together: the spent latch would fire the moment the
			// menu term cleared.
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_MilestoneAutosave] no-retry watch failed "
				"(closed/idle/counter/trace/bytes=%s/%s/%s/%s/%s)",
				g_bMAMenuClosed ? "true" : "false",
				g_bMANoRetryStayedIdle ? "true" : "false",
				g_bMANoRetryCounterStill ? "true" : "false",
				g_bMANoRetryTraceStill ? "true" : "false",
				g_bMANoRetryBytesStill ? "true" : "false");
		}
		if (!bReArm)
		{
			// A one-shot-per-process latch reds here: the counter never rises,
			// the phase deadlines by name, and every reArm flag stays false.
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_MilestoneAutosave] re-arm failed "
				"(req/saw/arrived/delta/trace/ready/read/canonical/sceneTag="
				"%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bMAReArmWarpRequested ? "true" : "false",
				g_bMASawReArmWarpRun ? "true" : "false",
				g_bMAReArmArrived ? "true" : "false",
				g_bMAReArmCounterDelta ? "true" : "false",
				g_bMAReArmTraceExact ? "true" : "false",
				g_bMAReArmAutoReady ? "true" : "false",
				g_bMAReArmReadOk ? "true" : "false",
				g_bMAReArmCanonicalLive ? "true" : "false",
				g_bMAReArmSceneTag ? "true" : "false");
		}
		return bPassed;
	}
}

#endif // ZENITH_INPUT_SIMULATOR
