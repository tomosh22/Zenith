#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_SaveMenu -- ZM_SaveMenuFlow_Test (S7 item 2 SC4).
//
// The end-to-end proof of the manual SAVE flow: in the REAL baked Dawnmere, under
// REAL simulated key edges, open the pause menu, walk the ROOT focus onto the new
// Save entry BY NAME, enter the SAVE screen, pick Slot 2, and watch a REAL slot
// file go from EMPTY to READY through the real ZM_SaveSlots::WriteState via the
// real menu dispatch -- then re-open, pick the SAME (now READY) slot, and prove the
// overwrite goes through the yes/no CONFIRM prompt rather than a silent stomp.
//
// WHY IT IS NOT VACUOUS. A save test proves nothing if it only checks a status code
// came back: it must prove a REAL slot STATUS CHANGED. So:
//   * The slot is ProbeSlot()-ed READY, SlotExists() confirms the file landed, and
//     the recorded payload is decoded back through ZM_SaveSlots::ReadState -- whose
//     world-position module is asserted to carry Dawnmere's build index (2), NOT the
//     UNSET sentinel. A save that captured no world position reds there.
//   * The overwrite path asserts IsDialogueAwaitingChoice() + GetPendingSaveSlot()
//     BEFORE answering, so a direct overwrite that bypassed the confirm reds (the
//     write count would jump to 2 with no prompt).
//   * Every phase flag DEFAULTS TO FAILING, so a phase that never runs fails.
//   * SetFocusedElement appears NOWHERE: every traversal is real Down / Enter / Left
//     / Escape edges, deadline-guarded, resolving the focused element BY NAME.
//
// DISK SAFETY. This test writes a REAL .zsave. It runs ONLY under the automated-test
// harness (--all-automated-tests), where Zenith_CommandLine::IsAutomatedTestRun() is
// set and every slot name already carries its "_Test" alias; on top of that Setup
// calls ZM_SaveSlots::SetTestSlotNamesForTests(true) explicitly and
// DeleteAllSlotsForTests() at BOTH ends, so it can never read, overwrite or delete
// the developer's real Save0/1/2/Auto .zsave -- only Save1_Test.zsave, which it
// deletes on every exit path. ClearForTest does NOT touch disk (Zenith_SaveData.h),
// so the DeleteAllSlotsForTests() call is what actually removes the file.
//
// CI VISIBILITY. m_bRequiresGraphics is true and zm-tests runs headless, so a
// headless SKIP counts as a PASS: THIS TEST IS INVISIBLE TO CI. It is a local /
// graphics-run detector, run via `zenith test Zenithmon --filter ZM_SaveMenuFlow_Test`.
//
// FIXED DT 1/60, matching the physics-coupled Dawnmere first-load window of the
// sibling walk tests. FRAME ORDERING: Zenith_AutomatedTestRunner::Tick() (Step) runs
// BEFORE g_xEngine.Scenes().Update(), so a key edge injected in Step is consumed by
// the menu / player LATER THE SAME FRAME and observed from the NEXT Step call --
// every "press here, assert next frame" pair below is exactly that.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "SaveData/Zenith_SaveData.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"
#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"

namespace
{
	constexpr float fSAVE_FIXED_DT = 1.0f / 60.0f;
	constexpr int   iSAVE_OVERWORLD_BUILD_INDEX = 2;   // Dawnmere (ZM-D-012)

	// The slot the manual flow writes: Slot 2 (row index 1). Chosen so the assertions
	// are about a MIDDLE manual slot, never Auto and never the first slot.
	constexpr ZM_SAVE_SLOT eTARGET_SLOT = ZM_SAVE_SLOT_1;
	constexpr u_int        uTARGET_ROW  = 1u;
	// A different EMPTY manual slot for the write-boundary negative. The screen is
	// opened while saving is legal; only then does a real warp blocker arise before
	// the Enter edge. Keeping it separate from eTARGET_SLOT makes "unchanged" mean
	// EMPTY->EMPTY, never "a prior READY file happened to survive".
	constexpr ZM_SAVE_SLOT eBLOCKED_SLOT = ZM_SAVE_SLOT_0;
	constexpr u_int        uBLOCKED_ROW  = 0u;

	// The frozen v1 golden blob is 824 bytes; framed with the 4-byte length prefix the
	// engine payload is at least 828. A LITERAL -- the point is a KNOWN framed size.
	constexpr u_int uMIN_FRAMED_PAYLOAD_BYTES = 828u;

	// ---- Per-phase deadlines (frames at 1/60). Their SUM (~1764) sits below the
	// harness m_iMaxFrames of 1800, so a stalled phase fails with its OWN diagnostic
	// before the frame cap can pre-empt it. (Itemised in the g_xZMSaveMenuFlowTest
	// registration below.) ----
	constexpr int iREADY_DEADLINE   = 600;   // Dawnmere physics first-load + grounding
	constexpr int iOPEN_DEADLINE    = 90;
	constexpr int iWALK_DEADLINE    = 120;
	constexpr int iWRITE_DEADLINE   = 60;    // frames for a WRITE to tick the counter
	constexpr int iDISMISS_DEADLINE = 120;
	constexpr int iPROMPT_DEADLINE  = 120;
	constexpr int iPRESS_FRAME      = 2;
	constexpr int iSETTLE_FRAME     = 6;

	// ------------------------------------------------------------------------
	// Views + resolvers. NOTHING cached across frames (the ECS pool relocates).
	// ------------------------------------------------------------------------

	struct SavePlayerView
	{
		Zenith_EntityID           m_xEntityID    = INVALID_ENTITY_ID;
		ZM_PlayerController*      m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider   = nullptr;
	};

	bool FindActivePlayer(SavePlayerView& xOut)
	{
		xOut = SavePlayerView{};
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
				if (uCount != 1u) { return; }
				xOut.m_xEntityID = xID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
			});
		return uCount == 1u;
	}

	ZM_UI_MenuStack* ResolveMenuStack()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
	}

	Zenith_UIComponent* ResolveMenuRootUI()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
	}

	std::string ReadMenuFocusName()
	{
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr) { return std::string(); }
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		return (pxFocused != nullptr) ? pxFocused->GetName() : std::string();
	}

	// Everything the manual save flow needs to capture a world position: the unique
	// player with a valid, grounded body, an active physics sim, and the persistent
	// ZM_MenuRoot singleton. Lighter than the UI tests' DawnmereRuntimeReady (which
	// also waits on grass + camera) -- neither matters to saving.
	bool SaveFlowReady()
	{
		SavePlayerView xPlayer;
		if (!FindActivePlayer(xPlayer)) { return false; }
		if (xPlayer.m_pxCollider == nullptr || !xPlayer.m_pxCollider->HasValidBody()) { return false; }
		if (xPlayer.m_pxController == nullptr || !xPlayer.m_pxController->IsGrounded()) { return false; }
		if (!g_xEngine.Physics().HasActiveSimulation()) { return false; }
		Zenith_EntityID xMenuID = INVALID_ENTITY_ID;
		return ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuID);
	}

	bool RequiredDawnmereAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::string astrRequired[] = {
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
			if (!std::filesystem::is_regular_file(strPath, xError) || xError) { return false; }
			const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
			if (xError || ulSize == 0u) { return false; }
		}
		return true;
	}

	// ------------------------------------------------------------------------
	// The phase machine + observation globals. EVERY flag defaults to FAILING.
	// ------------------------------------------------------------------------

	enum class SavePhase
	{
		AwaitReady,
		ValidatePromptTargets,
		OpenMenu,
		WalkToSave,
		EnterSave,
		WalkToRow,
		WriteRow,
		AwaitWrite1,
		DismissToClosed,   // return the menu to fully closed so the re-open re-probes
		Reopen,
		WalkToSave2,
		EnterSave2,
		WalkToRow2,
		ConfirmRow2,       // Enter on the now-READY row -> the overwrite CONFIRM prompt
		ReadPrompt,
		AnswerYes,
		AwaitWrite2,
		DismissAfterWrite2,
		WalkToBlockedRow,
		ArmLiveBlocker,
		AwaitBlockedRefusal,
		Done,
	};

	SavePhase   g_ePhase        = SavePhase::Done;
	int         g_iPhaseFrames  = 0;
	bool        g_bActive       = false;
	bool        g_bFailed       = false;
	bool        g_bSkipped      = false;
	const char* g_szFailure     = "test did not reach verification";

	// P0.
	bool g_bStartedClean        = false;   // Slot 2 probed EMPTY before the test wrote anything
	bool g_bAutoPromptRefused   = false;
	bool g_bNonePromptRefused   = false;
	bool g_bRangePromptRefused  = false;
	bool g_bInvalidPromptsLeftNoTarget = false;
	bool g_bInvalidPromptsWroteNothing = false;

	// P1-P4 (first save).
	bool  g_bReady              = false;
	bool  g_bMenuOpened         = false;
	bool  g_bReachedSaveEntry   = false;
	bool  g_bSaveScreenOpened   = false;   // top == SCREEN_SAVE after Enter on Save
	u_int g_uSaveModeOnOpen     = (u_int)ZM_SAVE_SCREEN_MODE_COUNT;   // want SAVE
	bool  g_bReachedRow         = false;

	// P5 (the first save landed for real).
	bool  g_bWrite1Observed     = false;
	u_int g_uWriteCountAfter1   = 0u;      // want exactly 1
	bool  g_bStatus1Ok          = false;
	u_int g_uProbeAfter1        = (u_int)ZM_SAVE_SLOT_STATUS_COUNT;   // want READY
	bool  g_bSlotFileExists1    = false;
	u_int g_uPayloadBytes1      = 0u;      // want >= uMIN_FRAMED_PAYLOAD_BYTES
	bool  g_bPayloadMagicOk1    = false;   // 'Z','M','S','V' at framed bytes [4..7]
	bool  g_bReadback1Ok        = false;   // ReadState round-tripped the file
	u_int g_uSceneAfter1        = uZM_WORLD_SCENE_UNSET;   // want the Dawnmere build index

	// P6 (the overwrite went through the confirm prompt).
	bool  g_bMenuClosedForReopen = false;
	bool  g_bMenuReopened        = false;
	bool  g_bReachedSaveEntry2   = false;
	bool  g_bSaveScreenOpened2   = false;
	u_int g_uRow1StatusOnReopen  = (u_int)ZM_SAVE_SLOT_STATUS_COUNT;   // want READY (the re-probe saw the write)
	bool  g_bReachedRow2         = false;
	bool  g_bPromptAwaiting      = false;
	u_int g_uPendingSlotOnPrompt = (u_int)ZM_SAVE_SLOT_NONE;   // want the target slot
	bool  g_bReachedYes          = false;
	bool  g_bWrite2Observed      = false;
	u_int g_uWriteCountAfter2    = 0u;      // want exactly 2
	bool  g_bStatus2Ok           = false;
	u_int g_uProbeAfter2         = (u_int)ZM_SAVE_SLOT_STATUS_COUNT;   // want READY
	u_int g_uSceneAfter2         = uZM_WORLD_SCENE_UNSET;   // want the Dawnmere build index

	// P7: the screen was opened while legal, then a live WARP blocker arose before
	// a REAL Enter activation. Every default is deliberately failing.
	bool  g_bBlockedScreenWasOpenAllowed = false;
	bool  g_bBlockedReachedRow           = false;
	bool  g_bBlockedCaptureProbeSucceeded = false;
	bool  g_bBlockedWorldSnapshotTaken   = false;
	bool  g_bBlockedWarpRequested        = false;
	u_int g_uBlockedLiveBlocker          = (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
	u_int g_uBlockedWriteCountBefore     = 0xFFFFFFFFu;
	u_int g_uBlockedWriteCountAfter      = 0u;
	u_int g_uBlockedWriteLogBefore       = 0xFFFFFFFFu;
	u_int g_uBlockedWriteLogAfter        = 0u;
	u_int g_uBlockedProbeBefore          = (u_int)ZM_SAVE_SLOT_READY;
	u_int g_uBlockedProbeAfter           = (u_int)ZM_SAVE_SLOT_READY;
	bool  g_bBlockedFileBefore           = true;
	bool  g_bBlockedFileAfter            = true;
	bool  g_bBlockedWorldUnchanged       = false;
	u_int8 g_auBlockedWorldBefore[sizeof(ZM_WorldPosition)] = {};

	void FailSave(const char* szReason)
	{
		g_szFailure = szReason;
		g_bFailed = true;
		g_ePhase = SavePhase::Done;
	}

	// Decode the recorded framed payload of the most recent Save() call and prove it is
	// [u32 length][ZMSV blob]: size >= the golden framed size and the ZMSV magic sits at
	// framed bytes [4..7]. ('Z','M','S','V' == the LE bytes of uMAGIC.)
	void CaptureFramedPayloadEvidence(u_int& uBytesOut, bool& bMagicOut)
	{
		uBytesOut = 0u;
		bMagicOut = false;
		const Zenith_Vector<Zenith_SaveData::WrittenSlot>& xLog =
			Zenith_SaveData::GetWrittenSlotsForTest();
		if (xLog.GetSize() == 0u) { return; }
		const Zenith_SaveData::WrittenSlot& xEntry = xLog.GetBack();
		uBytesOut = xEntry.m_xPayload.GetSize();
		if (xEntry.m_xPayload.GetSize() < 8u) { return; }
		bMagicOut = xEntry.m_xPayload.Get(4u) == (uint8_t)'Z'
			&& xEntry.m_xPayload.Get(5u) == (uint8_t)'M'
			&& xEntry.m_xPayload.Get(6u) == (uint8_t)'S'
			&& xEntry.m_xPayload.Get(7u) == (uint8_t)'V';
	}

	// Read the target slot back through the REAL codec and report the persisted world
	// position's scene build index (uZM_WORLD_SCENE_UNSET on any failure). This is what
	// proves the save recorded a REAL position, not that a status merely came back.
	u_int ReadBackWorldScene(bool& bOkOut)
	{
		bOkOut = false;
		ZM_GameState xOut;
		const Zenith_Status xStatus = ZM_SaveSlots::ReadState(eTARGET_SLOT, xOut);
		if (!xStatus.IsOk()) { return uZM_WORLD_SCENE_UNSET; }
		bOkOut = true;
		return xOut.m_xWorldPosition.m_uSceneBuildIndex;
	}

	void Setup_ZMSaveMenuFlow()
	{
		g_ePhase       = SavePhase::Done;
		g_iPhaseFrames = 0;
		g_bActive      = false;
		g_bFailed      = false;
		g_bSkipped     = false;
		g_szFailure    = "test did not reach verification";

		g_bStartedClean       = false;
		g_bAutoPromptRefused  = false;
		g_bNonePromptRefused  = false;
		g_bRangePromptRefused = false;
		g_bInvalidPromptsLeftNoTarget = false;
		g_bInvalidPromptsWroteNothing = false;
		g_bReady              = false;
		g_bMenuOpened         = false;
		g_bReachedSaveEntry   = false;
		g_bSaveScreenOpened   = false;
		g_uSaveModeOnOpen     = (u_int)ZM_SAVE_SCREEN_MODE_COUNT;
		g_bReachedRow         = false;

		g_bWrite1Observed     = false;
		g_uWriteCountAfter1   = 0u;
		g_bStatus1Ok          = false;
		g_uProbeAfter1        = (u_int)ZM_SAVE_SLOT_STATUS_COUNT;
		g_bSlotFileExists1    = false;
		g_uPayloadBytes1      = 0u;
		g_bPayloadMagicOk1    = false;
		g_bReadback1Ok        = false;
		g_uSceneAfter1        = uZM_WORLD_SCENE_UNSET;

		g_bMenuClosedForReopen = false;
		g_bMenuReopened        = false;
		g_bReachedSaveEntry2   = false;
		g_bSaveScreenOpened2   = false;
		g_uRow1StatusOnReopen  = (u_int)ZM_SAVE_SLOT_STATUS_COUNT;
		g_bReachedRow2         = false;
		g_bPromptAwaiting      = false;
		g_uPendingSlotOnPrompt = (u_int)ZM_SAVE_SLOT_NONE;
		g_bReachedYes          = false;
		g_bWrite2Observed      = false;
		g_uWriteCountAfter2    = 0u;
		g_bStatus2Ok           = false;
		g_uProbeAfter2         = (u_int)ZM_SAVE_SLOT_STATUS_COUNT;
		g_uSceneAfter2         = uZM_WORLD_SCENE_UNSET;

		g_bBlockedScreenWasOpenAllowed = false;
		g_bBlockedReachedRow            = false;
		g_bBlockedCaptureProbeSucceeded = false;
		g_bBlockedWorldSnapshotTaken    = false;
		g_bBlockedWarpRequested         = false;
		g_uBlockedLiveBlocker           = (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
		g_uBlockedWriteCountBefore      = 0xFFFFFFFFu;
		g_uBlockedWriteCountAfter       = 0u;
		g_uBlockedWriteLogBefore        = 0xFFFFFFFFu;
		g_uBlockedWriteLogAfter         = 0u;
		g_uBlockedProbeBefore           = (u_int)ZM_SAVE_SLOT_READY;
		g_uBlockedProbeAfter            = (u_int)ZM_SAVE_SLOT_READY;
		g_bBlockedFileBefore            = true;
		g_bBlockedFileAfter             = true;
		g_bBlockedWorldUnchanged        = false;
		std::memset(g_auBlockedWorldBefore, 0, sizeof(g_auBlockedWorldBefore));

		Zenith_InputSimulator::ResetAllInputState();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// Guard ORDER is mandatory: RequestSkip bypasses Verify, so install no fixed dt
		// / scene / disk-redirection state until every git-ignored dependency is present.
		if (!RequiredDawnmereAssetsPresent())
		{
			g_bSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_SaveMenuFlow] the Dawnmere scene / terrain bake is absent -- run a *_True "
				"config once to bake it");
			return;
		}
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_SaveMenuFlow] no persistent ZM_MenuRoot -- FrontEnd.zscen is not baked, so "
				"there is no menu to raise");
			return;
		}

		// Redirect every slot name onto its "_Test" alias and wipe those files, so the
		// real save is written to Save1_Test.zsave -- never the player's Save1.zsave.
		ZM_SaveSlots::SetTestSlotNamesForTests(true);
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		// An ambient .zsave from a prior process must not carry this test: prove the slot
		// really is EMPTY before anything writes it.
		g_bStartedClean = (ZM_SaveSlots::ProbeSlot(eTARGET_SLOT) == ZM_SAVE_SLOT_EMPTY)
			&& (ZM_SaveSlots::ProbeSlot(eBLOCKED_SLOT) == ZM_SAVE_SLOT_EMPTY);

		Zenith_InputSimulator::SetFixedDt(fSAVE_FIXED_DT);
		g_xEngine.Scenes().LoadSceneByIndex(iSAVE_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
		g_ePhase = SavePhase::AwaitReady;
		g_bActive = true;
	}

	// Walk the ROOT / SAVE-screen focus DOWN onto szTarget with spaced Down edges. True
	// once the focus name is already on it. Direction is always DOWN: every target here
	// sits below the entry the screen parks focus on when it opens.
	bool StepWalkDownTo(const char* szTarget)
	{
		if (ReadMenuFocusName() == szTarget) { return true; }
		if ((g_iPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
		}
		return false;
	}

	bool Step_ZMSaveMenuFlow(int)
	{
		if (!g_bActive || g_bFailed || g_ePhase == SavePhase::Done) { return false; }
		++g_iPhaseFrames;

		switch (g_ePhase)
		{
		case SavePhase::AwaitReady:
		{
			if (SaveFlowReady())
			{
				g_bReady = true;
				g_ePhase = SavePhase::ValidatePromptTargets;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iREADY_DEADLINE)
			{
				FailSave("Dawnmere never became save-ready (no unique grounded player body "
					"with an active physics sim, or no ZM_MenuRoot)");
				return false;
			}
			return true;
		}

		case SavePhase::ValidatePromptTargets:
		{
			// OpenSaveConfirmPrompt is public because the row dispatcher and future callers
			// share it. Pin its OWN boundary: the pure row resolver is not enough, because
			// a caller can invoke this seam directly and otherwise arm Auto/NONE/garbage.
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving during overwrite-target validation");
				return false;
			}
			g_bAutoPromptRefused = !pxMenu->OpenSaveConfirmPrompt(ZM_SAVE_SLOT_AUTO);
			const bool bAutoLeftNoTarget = pxMenu->GetPendingSaveSlot() == ZM_SAVE_SLOT_NONE;
			ZM_UI_MenuStack::ResetRuntimeStateForTests();

			pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr) { FailSave("menu vanished after the Auto prompt probe"); return false; }
			g_bNonePromptRefused = !pxMenu->OpenSaveConfirmPrompt(ZM_SAVE_SLOT_NONE);
			const bool bNoneLeftNoTarget = pxMenu->GetPendingSaveSlot() == ZM_SAVE_SLOT_NONE;
			ZM_UI_MenuStack::ResetRuntimeStateForTests();

			pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr) { FailSave("menu vanished after the NONE prompt probe"); return false; }
			const ZM_SAVE_SLOT eOutOfRange =
				(ZM_SAVE_SLOT)((u_int)ZM_SAVE_SLOT_COUNT + 17u);
			g_bRangePromptRefused = !pxMenu->OpenSaveConfirmPrompt(eOutOfRange);
			const bool bRangeLeftNoTarget = pxMenu->GetPendingSaveSlot() == ZM_SAVE_SLOT_NONE;
			ZM_UI_MenuStack::ResetRuntimeStateForTests();

			pxMenu = ResolveMenuStack();
			g_bInvalidPromptsLeftNoTarget = bAutoLeftNoTarget
				&& bNoneLeftNoTarget && bRangeLeftNoTarget;
			g_bInvalidPromptsWroteNothing = pxMenu != nullptr
				&& pxMenu->GetSaveWriteCount() == 0u
				&& Zenith_SaveData::GetWrittenSlotsForTest().GetSize() == 0u
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(eTARGET_SLOT) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(eBLOCKED_SLOT) == ZM_SAVE_SLOT_EMPTY;
			g_ePhase = SavePhase::OpenMenu;
			g_iPhaseFrames = 0;
			return true;
		}

		case SavePhase::OpenMenu:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the ZM_UI_MenuStack singleton stopped resolving while opening the menu");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bMenuOpened = true;
				g_ePhase = SavePhase::WalkToSave;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iOPEN_DEADLINE)
			{
				FailSave("the ROOT menu never opened on the M press");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case SavePhase::WalkToSave:
		{
			if (StepWalkDownTo(ZM_UI_MenuStack::szROOT_SAVE_NAME))
			{
				g_bReachedSaveEntry = true;
				g_ePhase = SavePhase::EnterSave;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWALK_DEADLINE)
			{
				FailSave("the Down edges never walked the ROOT focus onto the Save entry -- "
					"Menu_RootSave is missing a nav link, or hidden by a phantom save blocker");
				return false;
			}
			return true;
		}

		case SavePhase::EnterSave:
		{
			if (g_iPhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iPhaseFrames < iSETTLE_FRAME) { return true; }
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving after entering the Save screen");
				return false;
			}
			g_bSaveScreenOpened = (pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE);
			g_uSaveModeOnOpen = (u_int)pxMenu->GetSaveScreen().GetMode();
			if (!g_bSaveScreenOpened)
			{
				FailSave("Enter on the Save entry did not raise the SAVE screen");
				return false;
			}
			g_ePhase = SavePhase::WalkToRow;
			g_iPhaseFrames = 0;
			return true;
		}

		case SavePhase::WalkToRow:
		{
			if (StepWalkDownTo(ZM_UI_SaveSlots::RowElementName(uTARGET_ROW)))
			{
				g_bReachedRow = true;
				g_ePhase = SavePhase::WriteRow;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWALK_DEADLINE)
			{
				FailSave("the Down edges never walked the focus onto the target save row");
				return false;
			}
			return true;
		}

		case SavePhase::WriteRow:
		{
			// A fresh (EMPTY) manual slot resolves to WRITE, so this Enter saves straight
			// away -- NO confirm prompt. AwaitWrite1 asserts the taken path rather than
			// assuming one.
			if (g_iPhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			if (g_iPhaseFrames >= iSETTLE_FRAME)
			{
				g_ePhase = SavePhase::AwaitWrite1;
				g_iPhaseFrames = 0;
			}
			return true;
		}

		case SavePhase::AwaitWrite1:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving while awaiting the first write");
				return false;
			}
			if (pxMenu->GetSaveWriteCount() >= 1u)
			{
				g_bWrite1Observed = true;
				g_uWriteCountAfter1 = pxMenu->GetSaveWriteCount();
				g_bStatus1Ok = pxMenu->GetLastSaveStatus().IsOk();
				g_uProbeAfter1 = (u_int)ZM_SaveSlots::ProbeSlot(eTARGET_SLOT);
				g_bSlotFileExists1 = Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(eTARGET_SLOT));
				CaptureFramedPayloadEvidence(g_uPayloadBytes1, g_bPayloadMagicOk1);
				g_uSceneAfter1 = ReadBackWorldScene(g_bReadback1Ok);
				g_ePhase = SavePhase::DismissToClosed;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWRITE_DEADLINE)
			{
				FailSave("Enter on an EMPTY manual slot never performed the immediate write "
					"(GetSaveWriteCount stayed 0)");
				return false;
			}
			return true;
		}

		case SavePhase::DismissToClosed:
		{
			// Return the menu to FULLY CLOSED so the re-open below re-probes the slots and
			// sees the write. A save-result dialogue (if any) is MODAL -- advanced only by
			// Enter -- so press Enter while a DIALOGUE is on top and Escape to pop any other
			// screen, until nothing is open.
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving while closing the menu");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bMenuClosedForReopen = true;
				g_ePhase = SavePhase::Reopen;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iDISMISS_DEADLINE)
			{
				FailSave("the menu never returned to fully closed after the first save");
				return false;
			}
			if ((g_iPhaseFrames % 6) == 1)
			{
				if (pxMenu->GetTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
				{
					Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				}
				else
				{
					Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				}
			}
			return true;
		}

		case SavePhase::Reopen:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving while re-opening the menu");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bMenuReopened = true;
				g_ePhase = SavePhase::WalkToSave2;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iOPEN_DEADLINE)
			{
				FailSave("the ROOT menu never re-opened for the overwrite pass");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case SavePhase::WalkToSave2:
		{
			if (StepWalkDownTo(ZM_UI_MenuStack::szROOT_SAVE_NAME))
			{
				g_bReachedSaveEntry2 = true;
				g_ePhase = SavePhase::EnterSave2;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWALK_DEADLINE)
			{
				FailSave("the Down edges never re-reached the Save entry for the overwrite pass");
				return false;
			}
			return true;
		}

		case SavePhase::EnterSave2:
		{
			if (g_iPhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iPhaseFrames < iSETTLE_FRAME) { return true; }
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving after re-entering the Save screen");
				return false;
			}
			g_bSaveScreenOpened2 = (pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE);
			if (!g_bSaveScreenOpened2)
			{
				FailSave("Enter on the Save entry did not re-raise the SAVE screen");
				return false;
			}
			// The re-opened screen RE-PROBED, so the target row must now read READY -- proof
			// the first write actually landed and the screen sees it.
			g_uRow1StatusOnReopen = (u_int)pxMenu->GetSaveScreen().GetRowStatus(uTARGET_ROW);
			g_ePhase = SavePhase::WalkToRow2;
			g_iPhaseFrames = 0;
			return true;
		}

		case SavePhase::WalkToRow2:
		{
			if (StepWalkDownTo(ZM_UI_SaveSlots::RowElementName(uTARGET_ROW)))
			{
				g_bReachedRow2 = true;
				g_ePhase = SavePhase::ConfirmRow2;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWALK_DEADLINE)
			{
				FailSave("the Down edges never re-reached the target save row");
				return false;
			}
			return true;
		}

		case SavePhase::ConfirmRow2:
		{
			// The row is now READY, so this Enter must resolve CONFIRM_WRITE and open the
			// yes/no prompt -- NOT overwrite directly.
			if (g_iPhaseFrames == iPRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			if (g_iPhaseFrames >= iSETTLE_FRAME)
			{
				g_ePhase = SavePhase::ReadPrompt;
				g_iPhaseFrames = 0;
			}
			return true;
		}

		case SavePhase::ReadPrompt:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving while reading the overwrite prompt");
				return false;
			}
			// The overwrite must NEVER happen without a confirm: if the write count already
			// ticked to 2, the READY-slot overwrite bypassed the prompt.
			if (pxMenu->GetSaveWriteCount() >= 2u)
			{
				FailSave("the overwrite of a READY slot wrote WITHOUT the yes/no confirm prompt");
				return false;
			}
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				g_bPromptAwaiting = true;
				g_uPendingSlotOnPrompt = (u_int)pxMenu->GetPendingSaveSlot();
				g_ePhase = SavePhase::AnswerYes;
				g_iPhaseFrames = 0;
				return true;
			}
			if (!pxMenu->IsOpen())
			{
				FailSave("the overwrite confirm prompt closed instead of awaiting an answer");
				return false;
			}
			if (g_iPhaseFrames > iPROMPT_DEADLINE)
			{
				FailSave("the overwrite confirm prompt never came up on the READY row");
				return false;
			}
			// Spaced Enter edges read the prompt line through to its yes/no question.
			if ((g_iPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case SavePhase::AnswerYes:
		{
			// A REAL keyboard answer on YES: walk the focus onto the Yes button by name
			// (Left edges), then press Enter exactly once. Never SetFocusedElement.
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szYES_NAME)
			{
				g_bReachedYes = true;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				g_ePhase = SavePhase::AwaitWrite2;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWALK_DEADLINE)
			{
				FailSave("the Left edges never walked the prompt focus onto the Yes button");
				return false;
			}
			if ((g_iPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_LEFT);
			}
			return true;
		}

		case SavePhase::AwaitWrite2:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu singleton stopped resolving while awaiting the confirmed overwrite");
				return false;
			}
			if (pxMenu->GetSaveWriteCount() >= 2u)
			{
				g_bWrite2Observed = true;
				g_uWriteCountAfter2 = pxMenu->GetSaveWriteCount();
				g_bStatus2Ok = pxMenu->GetLastSaveStatus().IsOk();
				g_uProbeAfter2 = (u_int)ZM_SaveSlots::ProbeSlot(eTARGET_SLOT);
				bool bReadbackOk = false;
				g_uSceneAfter2 = ReadBackWorldScene(bReadbackOk);
				g_ePhase = SavePhase::DismissAfterWrite2;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWRITE_DEADLINE)
			{
				FailSave("answering Yes on the overwrite prompt never performed the write "
					"(GetSaveWriteCount stayed 1)");
				return false;
			}
			return true;
		}

		case SavePhase::DismissAfterWrite2:
		{
			// The allowed overwrite queued a result line over the still-open SAVE screen.
			// Read it away with real Enter edges; the screen underneath was opened while
			// ResolveLiveSaveBlocker reported NONE and remains the paired allowed control.
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailSave("the menu vanished while dismissing the allowed overwrite result");
				return false;
			}
			if (pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE)
			{
				g_bBlockedScreenWasOpenAllowed =
					ZM_SaveSlots::ResolveLiveSaveBlocker() == ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
				g_ePhase = SavePhase::WalkToBlockedRow;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iDISMISS_DEADLINE)
			{
				FailSave("the allowed overwrite result never returned to the SAVE screen");
				return false;
			}
			if ((g_iPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case SavePhase::WalkToBlockedRow:
		{
			if (ReadMenuFocusName() == ZM_UI_SaveSlots::RowElementName(uBLOCKED_ROW))
			{
				g_bBlockedReachedRow = true;
				g_ePhase = SavePhase::ArmLiveBlocker;
				g_iPhaseFrames = 0;
				return true;
			}
			if (g_iPhaseFrames > iWALK_DEADLINE)
			{
				FailSave("real Up edges never reached the EMPTY row used by the blocker probe");
				return false;
			}
			if ((g_iPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_UP);
			}
			return true;
		}

		case SavePhase::ArmLiveBlocker:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			ZM_GameState* pxState = nullptr;
			if (pxMenu == nullptr || !ZM_GameStateManager::TryGetGameState(pxState)
				|| pxState == nullptr)
			{
				FailSave("the live player/state vanished before the blocker boundary probe");
				return false;
			}

			// Prove CaptureWorldPosition WOULD work on this exact frame, but target a COPY;
			// the live state's sentinel below must remain byte-identical after activation.
			ZM_GameState xCaptureProbe = *pxState;
			g_bBlockedCaptureProbeSucceeded =
				ZM_GameStateManager::CaptureWorldPosition(xCaptureProbe);
			if (!g_bBlockedCaptureProbeSucceeded)
			{
				FailSave("CaptureWorldPosition on the copy failed before the blocker was raised");
				return false;
			}

			pxState->m_xWorldPosition = ZM_WorldPosition();
			pxState->m_xWorldPosition.m_uSceneBuildIndex =
				(u_int)iSAVE_OVERWORLD_BUILD_INDEX;
			std::memcpy(pxState->m_xWorldPosition.m_szSpawnTag, "TownCenter", 11u);
			pxState->m_xWorldPosition.m_afPosition[0] = 111.0f;
			pxState->m_xWorldPosition.m_afPosition[1] = 222.0f;
			pxState->m_xWorldPosition.m_afPosition[2] = 333.0f;
			pxState->m_xWorldPosition.m_fYaw = 1.25f;
			std::memcpy(g_auBlockedWorldBefore, &pxState->m_xWorldPosition,
				sizeof(g_auBlockedWorldBefore));
			g_bBlockedWorldSnapshotTaken = true;
			g_uBlockedWriteCountBefore = pxMenu->GetSaveWriteCount();
			g_uBlockedWriteLogBefore = Zenith_SaveData::GetWrittenSlotsForTest().GetSize();
			g_uBlockedProbeBefore = (u_int)ZM_SaveSlots::ProbeSlot(eBLOCKED_SLOT);
			g_bBlockedFileBefore = Zenith_SaveData::SlotExists(
				ZM_SaveSlots::SlotName(eBLOCKED_SLOT));

			g_bBlockedWarpRequested = ZM_GameStateManager::RequestQuitToFrontEnd();
			g_uBlockedLiveBlocker = (u_int)ZM_SaveSlots::ResolveLiveSaveBlocker();
			// The activation itself is a REAL Enter edge. Removing the write-boundary
			// ResolveLiveSaveBlocker recheck makes this capture + write immediately.
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			g_ePhase = SavePhase::AwaitBlockedRefusal;
			g_iPhaseFrames = 0;
			return true;
		}

		case SavePhase::AwaitBlockedRefusal:
		{
			if (g_iPhaseFrames < iSETTLE_FRAME) { return true; }
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			ZM_GameState* pxState = nullptr;
			if (pxMenu == nullptr || !ZM_GameStateManager::TryGetGameState(pxState)
				|| pxState == nullptr)
			{
				FailSave("the blocker result could not be sampled before the fade load");
				return false;
			}
			g_uBlockedWriteCountAfter = pxMenu->GetSaveWriteCount();
			g_uBlockedWriteLogAfter = Zenith_SaveData::GetWrittenSlotsForTest().GetSize();
			g_uBlockedProbeAfter = (u_int)ZM_SaveSlots::ProbeSlot(eBLOCKED_SLOT);
			g_bBlockedFileAfter = Zenith_SaveData::SlotExists(
				ZM_SaveSlots::SlotName(eBLOCKED_SLOT));
			g_bBlockedWorldUnchanged = g_bBlockedWorldSnapshotTaken
				&& std::memcmp(g_auBlockedWorldBefore, &pxState->m_xWorldPosition,
					sizeof(g_auBlockedWorldBefore)) == 0;
			g_ePhase = SavePhase::Done;
			return false;
		}

		case SavePhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMSaveMenuFlow()
	{
		// Unconditional teardown FIRST, on every exit path: restore input, close the
		// menu, delete the _Test slot files (while the redirection is still live), then
		// release the redirection. ClearForTest does not touch disk, so
		// DeleteAllSlotsForTests is what removes Save1_Test.zsave.
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		if (!g_bSkipped)
		{
			ZM_SaveSlots::DeleteAllSlotsForTests();
			Zenith_SaveData::ClearForTest();
			ZM_SaveSlots::SetTestSlotNamesForTests(false);
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		}

		if (g_bSkipped)
		{
			// A skip is a pass; nothing ran. (RequestSkip already reported the reason.)
			return true;
		}

		bool bPassed = true;

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_SaveMenuFlow] session: failed=%s (%s) startedClean=%s ready=%s opened=%s "
			"sawSave=%s saveScreen=%s mode=%u(want %u) row=%s",
			g_bFailed ? "true" : "false", g_szFailure,
			g_bStartedClean ? "true" : "false", g_bReady ? "true" : "false",
			g_bMenuOpened ? "true" : "false", g_bReachedSaveEntry ? "true" : "false",
			g_bSaveScreenOpened ? "true" : "false", g_uSaveModeOnOpen,
			(u_int)ZM_SAVE_SCREEN_MODE_SAVE, g_bReachedRow ? "true" : "false");
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_SaveMenuFlow] first save: observed=%s count=%u(want 1) statusOk=%s probe=%u"
			"(want %u=READY) fileExists=%s payloadBytes=%u(want >=%u) magicOk=%s readback=%s "
			"scene=%u(want %d)",
			g_bWrite1Observed ? "true" : "false", g_uWriteCountAfter1, g_bStatus1Ok ? "true" : "false",
			g_uProbeAfter1, (u_int)ZM_SAVE_SLOT_READY, g_bSlotFileExists1 ? "true" : "false",
			g_uPayloadBytes1, uMIN_FRAMED_PAYLOAD_BYTES, g_bPayloadMagicOk1 ? "true" : "false",
			g_bReadback1Ok ? "true" : "false", g_uSceneAfter1, iSAVE_OVERWORLD_BUILD_INDEX);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_SaveMenuFlow] overwrite: closed=%s reopened=%s sawSave2=%s saveScreen2=%s "
			"rowStatusOnReopen=%u(want %u=READY) row2=%s promptAwaiting=%s pendingSlot=%u"
			"(want %u) reachedYes=%s write2=%s count=%u(want 2) statusOk=%s probe=%u scene=%u",
			g_bMenuClosedForReopen ? "true" : "false", g_bMenuReopened ? "true" : "false",
			g_bReachedSaveEntry2 ? "true" : "false", g_bSaveScreenOpened2 ? "true" : "false",
			g_uRow1StatusOnReopen, (u_int)ZM_SAVE_SLOT_READY, g_bReachedRow2 ? "true" : "false",
			g_bPromptAwaiting ? "true" : "false", g_uPendingSlotOnPrompt, (u_int)eTARGET_SLOT,
			g_bReachedYes ? "true" : "false", g_bWrite2Observed ? "true" : "false",
			g_uWriteCountAfter2, g_bStatus2Ok ? "true" : "false", g_uProbeAfter2, g_uSceneAfter2);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_SaveMenuFlow] target guards: Auto/NONE/range refused=%s/%s/%s noTarget=%s "
			"noWrite=%s | boundary: openAllowed=%s row=%s captureProbe=%s warp=%s blocker=%u "
			"count=%u->%u log=%u->%u probe=%u->%u file=%s->%s worldUnchanged=%s",
			g_bAutoPromptRefused ? "true" : "false",
			g_bNonePromptRefused ? "true" : "false",
			g_bRangePromptRefused ? "true" : "false",
			g_bInvalidPromptsLeftNoTarget ? "true" : "false",
			g_bInvalidPromptsWroteNothing ? "true" : "false",
			g_bBlockedScreenWasOpenAllowed ? "true" : "false",
			g_bBlockedReachedRow ? "true" : "false",
			g_bBlockedCaptureProbeSucceeded ? "true" : "false",
			g_bBlockedWarpRequested ? "true" : "false", g_uBlockedLiveBlocker,
			g_uBlockedWriteCountBefore, g_uBlockedWriteCountAfter,
			g_uBlockedWriteLogBefore, g_uBlockedWriteLogAfter,
			g_uBlockedProbeBefore, g_uBlockedProbeAfter,
			g_bBlockedFileBefore ? "true" : "false",
			g_bBlockedFileAfter ? "true" : "false",
			g_bBlockedWorldUnchanged ? "true" : "false");

		if (g_bFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_SaveMenuFlow] %s", g_szFailure);
			bPassed = false;
		}

		// ---- P0: a clean fixture ----
		if (!g_bStartedClean)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the target slot was NOT empty before the test wrote it -- an "
				"ambient .zsave carried the fixture and the write assertions are vacuous");
			bPassed = false;
		}
		if (!g_bAutoPromptRefused || !g_bNonePromptRefused || !g_bRangePromptRefused
			|| !g_bInvalidPromptsLeftNoTarget || !g_bInvalidPromptsWroteNothing)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] OpenSaveConfirmPrompt must reject Auto, NONE and every "
				"out-of-range slot without arming a target or reaching a write (refused=%s/%s/%s "
				"noTarget=%s noWrite=%s); only Save0-2 are manual overwrite targets",
				g_bAutoPromptRefused ? "true" : "false",
				g_bNonePromptRefused ? "true" : "false",
				g_bRangePromptRefused ? "true" : "false",
				g_bInvalidPromptsLeftNoTarget ? "true" : "false",
				g_bInvalidPromptsWroteNothing ? "true" : "false");
			bPassed = false;
		}

		// ---- P1-P4: the menu path to the SAVE screen ----
		if (!g_bReady || !g_bMenuOpened || !g_bReachedSaveEntry)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the real menu path to the Save entry did not complete "
				"(ready=%s opened=%s reachedSave=%s)",
				g_bReady ? "true" : "false", g_bMenuOpened ? "true" : "false",
				g_bReachedSaveEntry ? "true" : "false");
			bPassed = false;
		}
		if (!g_bSaveScreenOpened || g_uSaveModeOnOpen != (u_int)ZM_SAVE_SCREEN_MODE_SAVE
			|| !g_bReachedRow)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the SAVE screen did not open in SAVE mode with the target row "
				"reached (screen=%s mode=%u row=%s)",
				g_bSaveScreenOpened ? "true" : "false", g_uSaveModeOnOpen,
				g_bReachedRow ? "true" : "false");
			bPassed = false;
		}

		// ---- P5: a REAL slot file went READY, carrying a REAL world position ----
		if (!g_bWrite1Observed || g_uWriteCountAfter1 != 1u || !g_bStatus1Ok)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the immediate write did not happen exactly once with an OK "
				"status (observed=%s count=%u statusOk=%s)",
				g_bWrite1Observed ? "true" : "false", g_uWriteCountAfter1,
				g_bStatus1Ok ? "true" : "false");
			bPassed = false;
		}
		if (g_uProbeAfter1 != (u_int)ZM_SAVE_SLOT_READY || !g_bSlotFileExists1)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the slot did not become a READY file on disk (probe=%u "
				"fileExists=%s) -- the save was not proven by a re-probe",
				g_uProbeAfter1, g_bSlotFileExists1 ? "true" : "false");
			bPassed = false;
		}
		if (g_uPayloadBytes1 < uMIN_FRAMED_PAYLOAD_BYTES || !g_bPayloadMagicOk1)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the recorded payload is not a framed ZMSV blob (bytes=%u "
				"want >=%u, magicOk=%s)",
				g_uPayloadBytes1, uMIN_FRAMED_PAYLOAD_BYTES, g_bPayloadMagicOk1 ? "true" : "false");
			bPassed = false;
		}
		if (!g_bReadback1Ok || g_uSceneAfter1 != (u_int)iSAVE_OVERWORLD_BUILD_INDEX)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the saved world position was UNSET or wrong (readback=%s "
				"scene=%u want %d) -- the manual save must record where the player is",
				g_bReadback1Ok ? "true" : "false", g_uSceneAfter1, iSAVE_OVERWORLD_BUILD_INDEX);
			bPassed = false;
		}

		// ---- P6: the overwrite goes through the confirm prompt ----
		if (!g_bMenuClosedForReopen || !g_bMenuReopened || !g_bReachedSaveEntry2
			|| !g_bSaveScreenOpened2 || !g_bReachedRow2)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the re-open path to the target row did not complete "
				"(closed=%s reopened=%s sawSave2=%s screen2=%s row2=%s)",
				g_bMenuClosedForReopen ? "true" : "false", g_bMenuReopened ? "true" : "false",
				g_bReachedSaveEntry2 ? "true" : "false", g_bSaveScreenOpened2 ? "true" : "false",
				g_bReachedRow2 ? "true" : "false");
			bPassed = false;
		}
		if (g_uRow1StatusOnReopen != (u_int)ZM_SAVE_SLOT_READY)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the re-opened SAVE screen did not re-probe the written slot as "
				"READY (status=%u) -- the overwrite would resolve as a fresh WRITE, not a confirm",
				g_uRow1StatusOnReopen);
			bPassed = false;
		}
		if (!g_bPromptAwaiting || g_uPendingSlotOnPrompt != (u_int)eTARGET_SLOT || !g_bReachedYes)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the overwrite did not go through the yes/no confirm on the "
				"target slot (awaiting=%s pendingSlot=%u want %u, reachedYes=%s)",
				g_bPromptAwaiting ? "true" : "false", g_uPendingSlotOnPrompt, (u_int)eTARGET_SLOT,
				g_bReachedYes ? "true" : "false");
			bPassed = false;
		}
		if (!g_bWrite2Observed || g_uWriteCountAfter2 != 2u || !g_bStatus2Ok
			|| g_uProbeAfter2 != (u_int)ZM_SAVE_SLOT_READY
			|| g_uSceneAfter2 != (u_int)iSAVE_OVERWORLD_BUILD_INDEX)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the confirmed overwrite did not complete as a second READY "
				"write (write2=%s count=%u statusOk=%s probe=%u scene=%u want %d)",
				g_bWrite2Observed ? "true" : "false", g_uWriteCountAfter2, g_bStatus2Ok ? "true" : "false",
				g_uProbeAfter2, g_uSceneAfter2, iSAVE_OVERWORLD_BUILD_INDEX);
			bPassed = false;
		}

		// ---- P7: permission is rechecked at the irreversible write boundary ----
		if (!g_bBlockedScreenWasOpenAllowed || !g_bBlockedReachedRow
			|| !g_bBlockedCaptureProbeSucceeded || !g_bBlockedWarpRequested
			|| g_uBlockedLiveBlocker != (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] the live-boundary preconditions were not proven "
				"(openAllowed=%s row=%s captureProbe=%s warp=%s blocker=%u want %u)",
				g_bBlockedScreenWasOpenAllowed ? "true" : "false",
				g_bBlockedReachedRow ? "true" : "false",
				g_bBlockedCaptureProbeSucceeded ? "true" : "false",
				g_bBlockedWarpRequested ? "true" : "false", g_uBlockedLiveBlocker,
				(u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP);
			bPassed = false;
		}
		if (!g_bBlockedWorldSnapshotTaken || !g_bBlockedWorldUnchanged
			|| g_uBlockedWriteCountAfter != g_uBlockedWriteCountBefore
			|| g_uBlockedWriteLogAfter != g_uBlockedWriteLogBefore
			|| g_uBlockedProbeBefore != (u_int)ZM_SAVE_SLOT_EMPTY
			|| g_uBlockedProbeAfter != (u_int)ZM_SAVE_SLOT_EMPTY
			|| g_bBlockedFileBefore || g_bBlockedFileAfter)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveMenuFlow] real Enter under the live WARP blocker captured or wrote "
				"(snapshot=%s worldUnchanged=%s count=%u->%u log=%u->%u probe=%u->%u "
				"file=%s->%s) -- PerformSaveToSlot must recheck ResolveLiveSaveBlocker "
				"before CaptureWorldPosition/WriteState",
				g_bBlockedWorldSnapshotTaken ? "true" : "false",
				g_bBlockedWorldUnchanged ? "true" : "false",
				g_uBlockedWriteCountBefore, g_uBlockedWriteCountAfter,
				g_uBlockedWriteLogBefore, g_uBlockedWriteLogAfter,
				g_uBlockedProbeBefore, g_uBlockedProbeAfter,
				g_bBlockedFileBefore ? "true" : "false",
				g_bBlockedFileAfter ? "true" : "false");
			bPassed = false;
		}

		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMSaveMenuFlowTest = {
	"ZM_SaveMenuFlow_Test",
	&Setup_ZMSaveMenuFlow,
	&Step_ZMSaveMenuFlow,
	&Verify_ZMSaveMenuFlow,
	// The harness backstop must sit ABOVE the SUM of the per-phase deadlines, so a
	// stalled phase fails with its OWN named diagnostic before the cap can pre-empt it.
	// Summed at 1/60: 600 ready + 90 open + 120 walk + 6 enter + 120 walk + 6 write +
	// 60 awaitWrite + 120 dismiss + 90 reopen + 120 walk + 6 enter + 120 walk + 6
	// confirm + 120 prompt + 120 answer + 60 awaitWrite2 = ~1764 worst case, below this
	// plus target validation + 120 result-dismiss + 120 blocked-row walk + 6 boundary
	// settle remains below 2200, so a stalled phase still fails by its own deadline.
	// Healthy runs remain far below the backstop.
	/* maxFrames */ 2200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMSaveMenuFlowTest);

namespace
{
	// ========================================================================
	// ZM_RootQuitAndBlockedSave_Test
	//
	// One real-input companion for the two ROOT-only contracts SC4 cannot prove
	// through presenter units: Quit's No->Yes end-to-end path, and a live WARP
	// blocker removing Save from BOTH focus directions. The blocker is raised only
	// after real M/Down input has opened ROOT and focused Save itself, so hiding Save
	// must also re-home the canvas focus onto a recognized live ROOT entry. All
	// blocked traversal remains real input. After the real Yes reaches FrontEnd, the
	// public LOAD seam proves Auto remains presentable there.
	// ========================================================================

	constexpr int iRQ_READY_DEADLINE    = 600;
	constexpr int iRQ_OPEN_DEADLINE     = 90;
	constexpr int iRQ_WALK_DEADLINE     = 120;
	constexpr int iRQ_PROMPT_DEADLINE   = 120;
	constexpr int iRQ_FRONTEND_DEADLINE = 420;
	constexpr int iRQ_IDLE_DEADLINE     = 160;
	constexpr int iRQ_SETTLE_FRAMES     = 6;

	enum class RQPhase
	{
		AwaitDawnmere,
		OpenRootNo,
		WalkQuitNo,
		OpenPromptNo,
		ReadPromptNo,
		ObserveNo,
		WalkToSaveForBlocker,
		RaiseBlockerAndRehome,
		ProbeBlockedOpenSave,
		BlockedDown,
		BlockedUp,
		BlockedAccept,
		OpenRootYes,
		WalkQuitYes,
		OpenPromptYes,
		ReadPromptYes,
		AnswerYes,
		AwaitFrontEnd,
		AwaitFrontEndIdle,
		OpenLoad,
		WalkToAuto,
		ActivateAuto,
		Done,
	};

	RQPhase g_eRQPhase = RQPhase::Done;
	int g_iRQPhaseFrames = 0;
	bool g_bRQActive = false;
	bool g_bRQSkipped = false;
	bool g_bRQFailed = false;
	const char* g_szRQFailure = "test did not reach verification";

	// Every semantic observation defaults to failing.
	bool g_bRQReady = false;
	bool g_bRQOpenedNo = false;
	bool g_bRQReachedQuitNo = false;
	bool g_bRQPromptNo = false;
	bool g_bRQNoReturnedRoot = false;
	bool g_bRQNoStayedDawnmere = false;
	bool g_bRQNoStartedNoWarp = false;
	bool g_bRQReachedQuitYes = false;
	bool g_bRQPromptYes = false;
	bool g_bRQReachedYes = false;
	bool g_bRQReachedFrontEnd = false;
	bool g_bRQFrontEndIdle = false;
	u_int g_uRQLoadsBeforeNo = 0xFFFFFFFFu;
	u_int g_uRQLoadsAfterNo = 0u;
	u_int g_uRQLoadsAfterYes = 0u;

	bool g_bRQAutoFixtureReady = false;
	bool g_bRQWarpBlockerLive = false;
	bool g_bRQReachedSaveBeforeBlocker = false;
	bool g_bRQSaveHidden = false;
	bool g_bRQSaveNotFocusable = false;
	bool g_bRQFocusRehomedToLiveRoot = false;
	bool g_bRQFocusedSaveWhileBlocked = false;
	bool g_bRQDownStayedOnLiveRoot = false;
	bool g_bRQUpStayedOnLiveRoot = false;
	bool g_bRQAcceptDidNotActivateSave = false;
	bool g_bRQBlockedSaveOpenRefused = false;
	bool g_bRQBlockedSaveStackUnchanged = false;
	bool g_bRQBlockedSaveModeUnchanged = false;
	bool g_bRQBlockedSaveWriteUnchanged = false;
	bool g_bRQBlockedSaveSlotUnchanged = false;
	bool g_bRQLoadOpened = false;
	bool g_bRQLoadMode = false;
	bool g_bRQAutoReadyInLoad = false;
	bool g_bRQAutoVisible = false;
	bool g_bRQAutoFocusable = false;
	bool g_bRQReachedAuto = false;
	bool g_bRQAutoActivationStayedLoad = false;
	bool g_bRQAutoActivationDidNotWrite = false;

	int RQActiveBuildIndex()
	{
		return g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
	}

	ZM_GameStateManager* RQResolveManager()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_GameStateManager>() : nullptr;
	}

	void ZM_IgnoreRQBlockerLoadRequest(u_int)
	{
		// The synthetic WARP exists only long enough to exercise blocked ROOT input.
		// Manager reset clears this callback before the later genuine Quit Yes path.
	}

	void FailRQ(const char* szReason)
	{
		g_szRQFailure = szReason;
		g_bRQFailed = true;
		g_eRQPhase = RQPhase::Done;
	}

	bool StepRQDownTo(const char* szName)
	{
		const std::string strFocus = ReadMenuFocusName();
		if (strFocus == szName) { return true; }
		if ((g_iRQPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
		}
		return false;
	}

	bool RQReadLiveRootFocus(bool& bSaveFocusedOut)
	{
		bSaveFocusedOut = false;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr) { return false; }
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		if (pxFocused == nullptr) { return false; }
		const char* szName = pxFocused->GetName().c_str();
		if (ZM_UI_MenuStack::RootItemIndexFromElementName(szName) < 0) { return false; }
		bSaveFocusedOut = std::strcmp(szName, ZM_UI_MenuStack::szROOT_SAVE_NAME) == 0;
		return pxFocused->IsVisible() && pxFocused->IsFocusable();
	}

	void Setup_ZMRootQuitAndBlockedSave()
	{
		g_eRQPhase = RQPhase::Done;
		g_iRQPhaseFrames = 0;
		g_bRQActive = false;
		g_bRQSkipped = false;
		g_bRQFailed = false;
		g_szRQFailure = "test did not reach verification";

		g_bRQReady = false;
		g_bRQOpenedNo = false;
		g_bRQReachedQuitNo = false;
		g_bRQPromptNo = false;
		g_bRQNoReturnedRoot = false;
		g_bRQNoStayedDawnmere = false;
		g_bRQNoStartedNoWarp = false;
		g_bRQReachedQuitYes = false;
		g_bRQPromptYes = false;
		g_bRQReachedYes = false;
		g_bRQReachedFrontEnd = false;
		g_bRQFrontEndIdle = false;
		g_uRQLoadsBeforeNo = 0xFFFFFFFFu;
		g_uRQLoadsAfterNo = 0u;
		g_uRQLoadsAfterYes = 0u;
		g_bRQAutoFixtureReady = false;
		g_bRQWarpBlockerLive = false;
		g_bRQReachedSaveBeforeBlocker = false;
		g_bRQSaveHidden = false;
		g_bRQSaveNotFocusable = false;
		g_bRQFocusRehomedToLiveRoot = false;
		g_bRQFocusedSaveWhileBlocked = false;
		g_bRQDownStayedOnLiveRoot = false;
		g_bRQUpStayedOnLiveRoot = false;
		g_bRQAcceptDidNotActivateSave = false;
		g_bRQBlockedSaveOpenRefused = false;
		g_bRQBlockedSaveStackUnchanged = false;
		g_bRQBlockedSaveModeUnchanged = false;
		g_bRQBlockedSaveWriteUnchanged = false;
		g_bRQBlockedSaveSlotUnchanged = false;
		g_bRQLoadOpened = false;
		g_bRQLoadMode = false;
		g_bRQAutoReadyInLoad = false;
		g_bRQAutoVisible = false;
		g_bRQAutoFocusable = false;
		g_bRQReachedAuto = false;
		g_bRQAutoActivationStayedLoad = false;
		g_bRQAutoActivationDidNotWrite = false;

		Zenith_InputSimulator::ResetAllInputState();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		if (!RequiredDawnmereAssetsPresent())
		{
			g_bRQSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_RootQuitAndBlockedSave] Dawnmere/FrontEnd assets are absent");
			return;
		}
		Zenith_EntityID xMenuID = INVALID_ENTITY_ID;
		Zenith_EntityID xManagerID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuID)
			|| !ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerID))
		{
			g_bRQSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_RootQuitAndBlockedSave] persistent menu/manager roots are not baked");
			return;
		}

		ZM_SaveSlots::SetTestSlotNamesForTests(true);
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		const ZM_GameState xFixture = ZM_MakeStarterGameState();
		g_bRQAutoFixtureReady = ZM_SaveSlots::WriteState(
			xFixture, ZM_SAVE_SLOT_AUTO).IsOk()
			&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_READY;

		Zenith_InputSimulator::SetFixedDt(fSAVE_FIXED_DT);
		g_xEngine.Scenes().LoadSceneByIndex(iSAVE_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
		g_eRQPhase = RQPhase::AwaitDawnmere;
		g_bRQActive = true;
	}

	bool Step_ZMRootQuitAndBlockedSave(int)
	{
		if (!g_bRQActive || g_bRQFailed || g_eRQPhase == RQPhase::Done) { return false; }
		++g_iRQPhaseFrames;

		switch (g_eRQPhase)
		{
		case RQPhase::AwaitDawnmere:
		{
			ZM_GameStateManager* pxManager = RQResolveManager();
			if (RQActiveBuildIndex() == iSAVE_OVERWORLD_BUILD_INDEX && SaveFlowReady()
				&& pxManager != nullptr
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
			{
				g_bRQReady = true;
				g_bRQNoStartedNoWarp = !ZM_GameStateManager::IsWarpInProgress();
				g_uRQLoadsBeforeNo = pxManager->GetIssuedLoadRequestCount();
				g_eRQPhase = RQPhase::OpenRootNo;
				g_iRQPhaseFrames = 0;
				return true;
			}
			if (g_iRQPhaseFrames > iRQ_READY_DEADLINE)
			{
				FailRQ("Dawnmere never became ready for the root Quit flow"); return false;
			}
			return true;
		}
		case RQPhase::OpenRootNo:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bRQOpenedNo = true;
				g_eRQPhase = RQPhase::WalkQuitNo; g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_OPEN_DEADLINE)
			{
				FailRQ("M never opened ROOT for the No pass"); return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}
		case RQPhase::WalkQuitNo:
			if (StepRQDownTo(ZM_UI_MenuStack::szROOT_QUIT_NAME))
			{
				g_bRQReachedQuitNo = true; g_eRQPhase = RQPhase::OpenPromptNo;
				g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_WALK_DEADLINE)
			{
				FailRQ("Down never reached Quit for the No pass"); return false;
			}
			return true;
		case RQPhase::OpenPromptNo:
			if (g_iRQPhaseFrames == 2) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER); }
			if (g_iRQPhaseFrames >= iRQ_SETTLE_FRAMES)
			{
				g_eRQPhase = RQPhase::ReadPromptNo; g_iRQPhaseFrames = 0;
			}
			return true;
		case RQPhase::ReadPromptNo:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu != nullptr && pxMenu->IsDialogueAwaitingChoice())
			{
				g_bRQPromptNo = true;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				g_eRQPhase = RQPhase::ObserveNo; g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_PROMPT_DEADLINE)
			{
				FailRQ("Quit No prompt never reached its choice"); return false;
			}
			if ((g_iRQPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}
		case RQPhase::ObserveNo:
		{
			if (g_iRQPhaseFrames < iRQ_SETTLE_FRAMES) { return true; }
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			ZM_GameStateManager* pxManager = RQResolveManager();
			if (pxMenu == nullptr || pxManager == nullptr)
			{
				FailRQ("menu/manager vanished after answering No"); return false;
			}
			g_bRQNoReturnedRoot = pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT
				&& pxMenu->GetLastDialogueAnswer() == ZM_DIALOGUE_CHOICE_NO;
			g_bRQNoStayedDawnmere = RQActiveBuildIndex() == iSAVE_OVERWORLD_BUILD_INDEX
				&& !ZM_GameStateManager::IsWarpInProgress();
			g_uRQLoadsAfterNo = pxManager->GetIssuedLoadRequestCount();
			g_eRQPhase = RQPhase::WalkToSaveForBlocker; g_iRQPhaseFrames = 0; return true;
		}
		case RQPhase::WalkToSaveForBlocker:
			if (StepRQDownTo(ZM_UI_MenuStack::szROOT_SAVE_NAME))
			{
				g_bRQReachedSaveBeforeBlocker = true;
				g_eRQPhase = RQPhase::RaiseBlockerAndRehome; g_iRQPhaseFrames = 0;
				return true;
			}
			if (g_iRQPhaseFrames > iRQ_WALK_DEADLINE)
			{
				FailRQ("real Down never focused Save before the live WARP blocker"); return false;
			}
			return true;
		case RQPhase::RaiseBlockerAndRehome:
		{
			if (g_iRQPhaseFrames == 1)
			{
				// Public runtime seam: creates a synchronous QUEUED/WARP blocker, then the
				// menu's ordinary later-this-frame presentation must hide, rewire AND move
				// focus off the now-dead Save entry while Dawnmere remains the active scene.
				ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(
					&ZM_IgnoreRQBlockerLoadRequest);
				g_bRQWarpBlockerLive = ZM_GameStateManager::RequestQuitToFrontEnd()
					&& ZM_SaveSlots::ResolveLiveSaveBlocker()
						== ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP;
				return true;
			}
			if (g_iRQPhaseFrames == 2)
			{
				Zenith_UIComponent* pxUI = ResolveMenuRootUI();
				Zenith_UI::Zenith_UIElement* pxSave = pxUI != nullptr
					? pxUI->FindElement(ZM_UI_MenuStack::szROOT_SAVE_NAME) : nullptr;
				g_bRQSaveHidden = pxSave != nullptr && !pxSave->IsVisible();
				g_bRQSaveNotFocusable = pxSave != nullptr && !pxSave->IsFocusable();
				bool bSaveFocused = false;
				const bool bLiveRootFocus = RQReadLiveRootFocus(bSaveFocused);
				g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
				g_bRQFocusRehomedToLiveRoot = bLiveRootFocus && !bSaveFocused;
				g_eRQPhase = RQPhase::ProbeBlockedOpenSave; g_iRQPhaseFrames = 0;
			}
			return true;
		}
		case RQPhase::ProbeBlockedOpenSave:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailRQ("menu vanished before the blocked public SAVE-open probe"); return false;
			}
			const u_int uDepthBefore = pxMenu->GetDepth();
			const ZM_MENU_SCREEN eTopBefore = pxMenu->GetTopScreen();
			const ZM_SAVE_SCREEN_MODE eModeBefore = pxMenu->GetSaveScreen().GetMode();
			const u_int uWriteCountBefore = pxMenu->GetSaveWriteCount();
			const u_int uWriteLogBefore = Zenith_SaveData::GetWrittenSlotsForTest().GetSize();
			const ZM_SAVE_SLOT_STATUS eSlotBefore = ZM_SaveSlots::ProbeSlot(eBLOCKED_SLOT);
			const bool bFileBefore = Zenith_SaveData::SlotExists(
				ZM_SaveSlots::SlotName(eBLOCKED_SLOT));

			const bool bOpened = ZM_UI_MenuStack::TryOpenSaveScreen(ZM_SAVE_SCREEN_MODE_SAVE);
			pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailRQ("menu vanished after the blocked public SAVE-open probe"); return false;
			}
			g_bRQBlockedSaveOpenRefused = !bOpened;
			g_bRQBlockedSaveStackUnchanged = pxMenu->GetDepth() == uDepthBefore
				&& pxMenu->GetTopScreen() == eTopBefore;
			g_bRQBlockedSaveModeUnchanged = pxMenu->GetSaveScreen().GetMode() == eModeBefore;
			g_bRQBlockedSaveWriteUnchanged = pxMenu->GetSaveWriteCount() == uWriteCountBefore
				&& Zenith_SaveData::GetWrittenSlotsForTest().GetSize() == uWriteLogBefore;
			g_bRQBlockedSaveSlotUnchanged = ZM_SaveSlots::ProbeSlot(eBLOCKED_SLOT) == eSlotBefore
				&& Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(eBLOCKED_SLOT)) == bFileBefore;

			bool bSaveFocused = false;
			RQReadLiveRootFocus(bSaveFocused);
			g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
			g_eRQPhase = RQPhase::BlockedDown; g_iRQPhaseFrames = 0;
			return true;
		}
		case RQPhase::BlockedDown:
		{
			bool bSaveFocused = false;
			RQReadLiveRootFocus(bSaveFocused);
			g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
			if (g_iRQPhaseFrames == 2) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN); }
			if (g_iRQPhaseFrames >= iRQ_SETTLE_FRAMES)
			{
				g_bRQDownStayedOnLiveRoot = RQReadLiveRootFocus(bSaveFocused) && !bSaveFocused;
				g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
				g_eRQPhase = RQPhase::BlockedUp; g_iRQPhaseFrames = 0;
			}
			return true;
		}
		case RQPhase::BlockedUp:
		{
			bool bSaveFocused = false;
			RQReadLiveRootFocus(bSaveFocused);
			g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
			if (g_iRQPhaseFrames == 2) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_UP); }
			if (g_iRQPhaseFrames >= iRQ_SETTLE_FRAMES)
			{
				g_bRQUpStayedOnLiveRoot = RQReadLiveRootFocus(bSaveFocused) && !bSaveFocused;
				g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
				g_eRQPhase = RQPhase::BlockedAccept; g_iRQPhaseFrames = 0;
			}
			return true;
		}
		case RQPhase::BlockedAccept:
		{
			bool bSaveFocused = false;
			RQReadLiveRootFocus(bSaveFocused);
			g_bRQFocusedSaveWhileBlocked |= bSaveFocused;
			if (g_iRQPhaseFrames == 8) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER); }
			if (g_iRQPhaseFrames >= 10)
			{
				ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
				g_bRQAcceptDidNotActivateSave = pxMenu != nullptr
					&& pxMenu->GetTopScreen() != ZM_MENU_SCREEN_SAVE
					&& pxMenu->GetSaveWriteCount() == 0u;
				// Clear the no-op loader + cancel the deliberately injected warp, then close
				// any screen opened by Accept through existing public test seams. The next phase reopens
				// ROOT with a real M edge and the real loader for the actual Yes flow.
				ZM_GameStateManager::ResetRuntimeStateForTests();
				ZM_UI_MenuStack::ResetRuntimeStateForTests();
				g_eRQPhase = RQPhase::OpenRootYes; g_iRQPhaseFrames = 0;
			}
			return true;
		}
		case RQPhase::OpenRootYes:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_eRQPhase = RQPhase::WalkQuitYes; g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_OPEN_DEADLINE)
			{
				FailRQ("M never reopened ROOT after the blocked-navigation probe"); return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}
		case RQPhase::WalkQuitYes:
			if (StepRQDownTo(ZM_UI_MenuStack::szROOT_QUIT_NAME))
			{
				g_bRQReachedQuitYes = true; g_eRQPhase = RQPhase::OpenPromptYes;
				g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_WALK_DEADLINE)
			{
				FailRQ("Down never re-reached Quit for the Yes pass"); return false;
			}
			return true;
		case RQPhase::OpenPromptYes:
			if (g_iRQPhaseFrames == 2) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER); }
			if (g_iRQPhaseFrames >= iRQ_SETTLE_FRAMES)
			{
				g_eRQPhase = RQPhase::ReadPromptYes; g_iRQPhaseFrames = 0;
			}
			return true;
		case RQPhase::ReadPromptYes:
		{
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu != nullptr && pxMenu->IsDialogueAwaitingChoice())
			{
				g_bRQPromptYes = true; g_eRQPhase = RQPhase::AnswerYes;
				g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_PROMPT_DEADLINE)
			{
				FailRQ("Quit Yes prompt never reached its choice"); return false;
			}
			if ((g_iRQPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}
		case RQPhase::AnswerYes:
		{
			if (ReadMenuFocusName() == ZM_UI_DialogueBox::szYES_NAME)
			{
				g_bRQReachedYes = true;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				g_eRQPhase = RQPhase::AwaitFrontEnd; g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_WALK_DEADLINE)
			{
				FailRQ("Left never reached Yes on the Quit prompt"); return false;
			}
			if ((g_iRQPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_LEFT);
			}
			return true;
		}
		case RQPhase::AwaitFrontEnd:
			if (RQActiveBuildIndex() == 0)
			{
				g_bRQReachedFrontEnd = true; g_eRQPhase = RQPhase::AwaitFrontEndIdle;
				g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_FRONTEND_DEADLINE)
			{
				FailRQ("Yes never reached FrontEnd"); return false;
			}
			return true;
		case RQPhase::AwaitFrontEndIdle:
		{
			ZM_GameStateManager* pxManager = RQResolveManager();
			if (pxManager != nullptr
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
				&& ResolveMenuStack() != nullptr)
			{
				g_bRQFrontEndIdle = true;
				g_uRQLoadsAfterYes = pxManager->GetIssuedLoadRequestCount();
				g_eRQPhase = RQPhase::OpenLoad; g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_IDLE_DEADLINE)
			{
				FailRQ("FrontEnd never settled with unique manager/menu roots"); return false;
			}
			return true;
		}
		case RQPhase::OpenLoad:
		{
			ZM_UI_MenuStack::ResetRuntimeStateForTests();
			g_bRQLoadOpened = ZM_UI_MenuStack::TryOpenSaveScreen(ZM_SAVE_SCREEN_MODE_LOAD);
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			Zenith_UIComponent* pxUI = ResolveMenuRootUI();
			Zenith_UI::Zenith_UIElement* pxAuto = pxUI != nullptr
				? pxUI->FindElement(ZM_UI_SaveSlots::RowElementName((u_int)ZM_SAVE_SLOT_AUTO))
				: nullptr;
			g_bRQLoadMode = pxMenu != nullptr
				&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE
				&& pxMenu->GetSaveScreen().GetMode() == ZM_SAVE_SCREEN_MODE_LOAD;
			g_bRQAutoReadyInLoad = pxMenu != nullptr
				&& pxMenu->GetSaveScreen().GetRowStatus((u_int)ZM_SAVE_SLOT_AUTO)
					== ZM_SAVE_SLOT_READY;
			g_bRQAutoVisible = pxAuto != nullptr && pxAuto->IsVisible();
			g_bRQAutoFocusable = pxAuto != nullptr && pxAuto->IsFocusable();
			g_eRQPhase = RQPhase::WalkToAuto; g_iRQPhaseFrames = 0; return true;
		}
		case RQPhase::WalkToAuto:
			if (StepRQDownTo(ZM_UI_SaveSlots::RowElementName((u_int)ZM_SAVE_SLOT_AUTO)))
			{
				g_bRQReachedAuto = true; g_eRQPhase = RQPhase::ActivateAuto;
				g_iRQPhaseFrames = 0; return true;
			}
			if (g_iRQPhaseFrames > iRQ_WALK_DEADLINE)
			{
				FailRQ("real Down never reached Auto in FrontEnd LOAD mode"); return false;
			}
			return true;
		case RQPhase::ActivateAuto:
			if (g_iRQPhaseFrames == 2) { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER); }
			if (g_iRQPhaseFrames >= iRQ_SETTLE_FRAMES)
			{
				ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
				g_bRQAutoActivationStayedLoad = pxMenu != nullptr
					&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE
					&& pxMenu->GetSaveScreen().GetMode() == ZM_SAVE_SCREEN_MODE_LOAD;
				g_bRQAutoActivationDidNotWrite = pxMenu != nullptr
					&& pxMenu->GetSaveWriteCount() == 0u
					&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_READY;
				g_eRQPhase = RQPhase::Done; return false;
			}
			return true;
		case RQPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMRootQuitAndBlockedSave()
	{
		if (g_bRQSkipped) { return true; }
		bool bPassed = !g_bRQFailed;
		if (g_bRQFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RootQuitAndBlockedSave] %s", g_szRQFailure);
		}
		if (!g_bRQReady || !g_bRQOpenedNo || !g_bRQReachedQuitNo || !g_bRQPromptNo
			|| !g_bRQNoReturnedRoot || !g_bRQNoStayedDawnmere || !g_bRQNoStartedNoWarp
			|| g_uRQLoadsAfterNo != g_uRQLoadsBeforeNo)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RootQuitAndBlockedSave] real No did not dismiss back to Dawnmere ROOT "
				"without a quit (ready/open/quit/prompt/return/stay/noWarp=%s/%s/%s/%s/%s/%s/%s "
				"loads=%u->%u)", g_bRQReady ? "true" : "false", g_bRQOpenedNo ? "true" : "false",
				g_bRQReachedQuitNo ? "true" : "false", g_bRQPromptNo ? "true" : "false",
				g_bRQNoReturnedRoot ? "true" : "false", g_bRQNoStayedDawnmere ? "true" : "false",
				g_bRQNoStartedNoWarp ? "true" : "false", g_uRQLoadsBeforeNo, g_uRQLoadsAfterNo);
			bPassed = false;
		}
		if (!g_bRQReachedQuitYes || !g_bRQPromptYes || !g_bRQReachedYes
			|| !g_bRQReachedFrontEnd || !g_bRQFrontEndIdle
			|| g_uRQLoadsAfterYes != g_uRQLoadsBeforeNo + 1u)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RootQuitAndBlockedSave] real Yes did not request exactly one quit and "
				"reach settled FrontEnd (quit/prompt/yes/front/idle=%s/%s/%s/%s/%s loads=%u->%u)",
				g_bRQReachedQuitYes ? "true" : "false", g_bRQPromptYes ? "true" : "false",
				g_bRQReachedYes ? "true" : "false", g_bRQReachedFrontEnd ? "true" : "false",
				g_bRQFrontEndIdle ? "true" : "false", g_uRQLoadsBeforeNo, g_uRQLoadsAfterYes);
			bPassed = false;
		}
		if (!g_bRQWarpBlockerLive || !g_bRQReachedSaveBeforeBlocker
			|| !g_bRQSaveHidden || !g_bRQSaveNotFocusable
			|| !g_bRQFocusRehomedToLiveRoot || g_bRQFocusedSaveWhileBlocked
			|| !g_bRQDownStayedOnLiveRoot || !g_bRQUpStayedOnLiveRoot
			|| !g_bRQAcceptDidNotActivateSave)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RootQuitAndBlockedSave] live Dawnmere WARP blocker did not immediately "
				"re-home focused Save or keep real Up/Down/Accept on live ROOT entries "
				"(blocker/reachedSave/hidden/notFocusable/rehomed/focusedSave(want false)/"
				"downLive/upLive/acceptNoSave=%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bRQWarpBlockerLive ? "true" : "false",
				g_bRQReachedSaveBeforeBlocker ? "true" : "false",
				g_bRQSaveHidden ? "true" : "false",
				g_bRQSaveNotFocusable ? "true" : "false",
				g_bRQFocusRehomedToLiveRoot ? "true" : "false",
				g_bRQFocusedSaveWhileBlocked ? "true" : "false",
				g_bRQDownStayedOnLiveRoot ? "true" : "false",
				g_bRQUpStayedOnLiveRoot ? "true" : "false",
				g_bRQAcceptDidNotActivateSave ? "true" : "false");
			bPassed = false;
		}
		if (!g_bRQBlockedSaveOpenRefused || !g_bRQBlockedSaveStackUnchanged
			|| !g_bRQBlockedSaveModeUnchanged || !g_bRQBlockedSaveWriteUnchanged
			|| !g_bRQBlockedSaveSlotUnchanged)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RootQuitAndBlockedSave] TryOpenSaveScreen(SAVE) did not fail closed "
				"under the live WARP blocker (refused/stack/mode/writes/slotFile=%s/%s/%s/%s/%s)",
				g_bRQBlockedSaveOpenRefused ? "true" : "false",
				g_bRQBlockedSaveStackUnchanged ? "true" : "false",
				g_bRQBlockedSaveModeUnchanged ? "true" : "false",
				g_bRQBlockedSaveWriteUnchanged ? "true" : "false",
				g_bRQBlockedSaveSlotUnchanged ? "true" : "false");
			bPassed = false;
		}
		if (!g_bRQAutoFixtureReady || !g_bRQLoadOpened || !g_bRQLoadMode
			|| !g_bRQAutoReadyInLoad || !g_bRQAutoVisible || !g_bRQAutoFocusable
			|| !g_bRQReachedAuto || !g_bRQAutoActivationStayedLoad
			|| !g_bRQAutoActivationDidNotWrite)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RootQuitAndBlockedSave] LOAD from FrontEnd (including Auto) was not "
				"preserved (fixture/open/mode/ready/visible/focusable/reached/stayed/noWrite="
				"%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bRQAutoFixtureReady ? "true" : "false", g_bRQLoadOpened ? "true" : "false",
				g_bRQLoadMode ? "true" : "false", g_bRQAutoReadyInLoad ? "true" : "false",
				g_bRQAutoVisible ? "true" : "false", g_bRQAutoFocusable ? "true" : "false",
				g_bRQReachedAuto ? "true" : "false",
				g_bRQAutoActivationStayedLoad ? "true" : "false",
				g_bRQAutoActivationDidNotWrite ? "true" : "false");
			bPassed = false;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		ZM_SaveSlots::SetTestSlotNamesForTests(false);
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_bRQActive = false;
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMRootQuitAndBlockedSaveTest = {
	"ZM_RootQuitAndBlockedSave_Test",
	&Setup_ZMRootQuitAndBlockedSave,
	&Step_ZMRootQuitAndBlockedSave,
	&Verify_ZMRootQuitAndBlockedSave,
	// 600 ready + 2x(90 open/walk + 120 prompt/walk) + 420 FrontEnd + 160 idle
	// + four 120-frame nav walks + fixed settles stays below this backstop.
	/* maxFrames */ 2500,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMRootQuitAndBlockedSaveTest);

#endif // ZENITH_INPUT_SIMULATOR
