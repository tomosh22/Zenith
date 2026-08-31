#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_SaveContinue -- the S7 item 2 SC5 disk-authentic Continue proof.
//
// The persistent ZM_GameStateManager survives SINGLE scene loads. A test which
// merely quits and presses Continue can therefore pass without reading one byte:
// RAM already contains the state it expects. This test deliberately overwrites
// that live state with a DIFFERENT, valid SCRAMBLE after reaching FrontEnd, then
// observes the scramble on a later frame before it sends any title input. Only a
// real disk read of the unique Auto fixture can restore the expected party,
// story flags, money and non-marker body pose.
//
// The Auto fixture is deleted BEFORE the quit so the re-raised title can be
// proven live with ONLY a DAMAGED slot on disk (DAMAGED still counts as
// occupied), then restored at that open title from the exact bytes captured
// before the deletion. The title's own Continue Enter re-probes every slot
// uncached, so the restore needs no screen refresh to become loadable.
//
// Input is exclusively Zenith_InputSimulator key state. SetFocusedElement and
// the public programmatic TITLE/LOAD seams do not appear in this file. Every wait
// has a phase-local deadline and every semantic latch defaults false.
//
// ★ THIS TEST IS **NOT** GRAPHICS-REQUIRED, and this comment used to claim it was.
// Its registration below passes `false /* m_bRequiresGraphics */`, which is correct
// and deliberate: every assertion here is about BYTES ON DISK and live model state,
// never about a pixel, so it RUNS FOR REAL on the Null backend instead of skipping.
// That matters because a skip counts as a PASS in the headless `zm-tests` gate --
// so this test is CI-visible, and calling it graphics-required invited a future
// session to "restore" a true flag and silently delete the coverage. A doc audit
// caught the mismatch on 2026-07-29; the registration was right, the comment wrong.
//
// ★ ZM-D-176 -- WHERE A NEW GAME NOW LANDS, AND WHY THIS FIXTURE STILL WALKS IN
// DAWNMERE.
//
// A new run no longer materialises in the town square: it begins in the player's
// bedroom, at ZM_GameStateManager::uNEW_GAME_BUILD_INDEX (PlayerHome) on its
// szNEW_GAME_SPAWN_TAG marker, and leaves through the shipped
// PlayerHomeExitTrigger. The arrival phases below were re-pointed accordingly and
// a new AwaitPlayerHome phase proves the arrival on BOTH axes -- the active scene
// AND the tag the transition recorded -- because a scene match alone would also
// be satisfied by a warp that landed on the wrong marker in the right room.
//
// The phase then issues an EXPLICIT RequestWarp into Dawnmere's TownCenter and
// every downstream clause resumes unchanged. That is deliberate, not laziness:
//   * the walk/capture/save/resume clauses are calibrated to DAWNMERE's geometry
//     (fSC_MIN_WALK_DISTANCE against the open square, fSC_MIN_FROM_TOWNCENTER
//     against the marker this test warps to, the saved blob's build index). Run
//     inside the 16 x 12 m bedroom instead and a held W reaches the exit sensor
//     in ~1.1 m -- under the 3 m walk floor -- so the fixture would be measuring
//     a different, weaker property while still looking green;
//   * driving OUT through the exit trigger instead is the same trap from the far
//     side: Dawnmere's "FromHome" marker sits ~5 m from the home sensor, so a 3 m
//     walk lands nearly back inside it;
//   * and nothing is lost by warping, because the PlayerHome -> Dawnmere DRIVE is
//     already proven end to end by ZM_PlayerHomeRoundTrip_Test.
// The explicit warp costs one extra scene load, which is why the load-count clause
// below expects +2 rather than +1.
//
// ★ THE CONSTANTS ARE READ, NEVER RE-SPELLED. GetTargetBuildIndex() is compared
// against ZM_GameStateManager::uNEW_GAME_BUILD_INDEX, so this fixture cannot
// disagree with the shipped flow. That alone would still pass if someone reverted
// the constant to Dawnmere, so a SECOND, NEGATIVE latch requires the destination
// to differ from iSC_DAWNMERE_BUILD. Pinning the constant's VALUE is a different
// job and belongs to the boot units in Tests/ZM_Tests_NewGameEntry.cpp; the split
// is deliberate and neither layer is decorative.
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
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "SaveData/Zenith_SaveData.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_MakeNewGameState
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"    // ZM_ApplyStarterChoice / ZM_STARTER_CHOICE_FERNFAWN
#include "Zenithmon/Source/Save/ZM_ResumePoint.h"
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"
#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"
#include "Zenithmon/Source/UI/ZM_UI_TitleMenu.h"

namespace
{
	constexpr float fSC_FIXED_DT = 1.0f / 60.0f;
	constexpr int iSC_FRONTEND_BUILD = 0;
	// ★ STILL 2, AND STILL NEEDED AFTER ZM-D-176. Dawnmere is no longer where a
	// new run lands, but it is still the EXPLICIT warp target this fixture drives
	// to, the build index the saved blob must carry, and the scene the resume arm
	// returns to. There is deliberately NO iSC_PLAYERHOME_BUILD beside it: the new
	// entry point is read from ZM_GameStateManager::uNEW_GAME_BUILD_INDEX, because
	// a literal 40 here could not red a drift in the constant the game actually
	// uses -- both sides would simply be typed twice.
	constexpr int iSC_DAWNMERE_BUILD = 2;

	constexpr float fSC_MIN_WALK_DISTANCE = 3.0f;
	constexpr float fSC_MAX_PLANAR_ERROR = 0.05f;
	constexpr float fSC_MAX_VERTICAL_ERROR = 0.10f;
	constexpr float fSC_MAX_YAW_ERROR = 0.05f;
	constexpr float fSC_MIN_FROM_TOWNCENTER = 2.0f;
	constexpr float fSC_MIN_FROM_SCRAMBLE = 2.0f;

	constexpr int iSC_FRONTEND_DEADLINE = 600;
	constexpr int iSC_TRANSITION_DEADLINE = 480;
	// ZM-D-176: the new-game arrival gets its OWN deadline and its OWN failure
	// text. Folding it into the Dawnmere budget would report "never settled in
	// Dawnmere" for a bedroom that never loaded, which names the wrong room.
	constexpr int iSC_PLAYERHOME_DEADLINE = 600;
	constexpr int iSC_DAWNMERE_DEADLINE = 600;
	constexpr int iSC_WALK_DEADLINE = 360;
	constexpr int iSC_REST_DEADLINE = 180;
	constexpr int iSC_FOCUS_DEADLINE = 120;
	constexpr int iSC_SCREEN_DEADLINE = 120;
	constexpr int iSC_DIALOGUE_DEADLINE = 180;
	constexpr int iSC_SCRAMBLE_SETTLE = 8;
	constexpr int iSC_FINAL_SETTLE = 20;
	constexpr u_int uSC_SLOT_TRACE_CAPACITY = 64u;

	struct SCSlotOperation
	{
		ZM_SAVE_SLOT_OPERATION_FOR_TESTS m_eOperation = ZM_SAVE_SLOT_OPERATION_PROBE_SLOT;
		ZM_SAVE_SLOT m_eSlot = ZM_SAVE_SLOT_NONE;
	};

	SCSlotOperation g_axSCSlotOperations[uSC_SLOT_TRACE_CAPACITY];
	u_int g_uSCSlotOperationCount = 0u;
	bool g_bSCSlotOperationOverflow = false;

	void SCResetSlotOperationTrace()
	{
		g_uSCSlotOperationCount = 0u;
		g_bSCSlotOperationOverflow = false;
	}

	void SCObserveSlotOperation(
		ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation,
		ZM_SAVE_SLOT eSlot)
	{
		if (g_uSCSlotOperationCount >= uSC_SLOT_TRACE_CAPACITY)
		{
			g_bSCSlotOperationOverflow = true;
			return;
		}
		g_axSCSlotOperations[g_uSCSlotOperationCount].m_eOperation = eOperation;
		g_axSCSlotOperations[g_uSCSlotOperationCount].m_eSlot = eSlot;
		++g_uSCSlotOperationCount;
	}

	u_int SCCountSlotOperations(ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation)
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < g_uSCSlotOperationCount; ++u)
		{
			if (g_axSCSlotOperations[u].m_eOperation == eOperation) { ++uCount; }
		}
		return uCount;
	}

	u_int SCCountSlotOperationsOn(ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation,
		ZM_SAVE_SLOT eSlot)
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < g_uSCSlotOperationCount; ++u)
		{
			if (g_axSCSlotOperations[u].m_eOperation == eOperation
				&& g_axSCSlotOperations[u].m_eSlot == eSlot)
			{
				++uCount;
			}
		}
		return uCount;
	}

	bool SCTraceIsExactlyOne(
		ZM_SAVE_SLOT_OPERATION_FOR_TESTS eOperation,
		ZM_SAVE_SLOT eSlot)
	{
		return !g_bSCSlotOperationOverflow && g_uSCSlotOperationCount == 1u
			&& g_axSCSlotOperations[0].m_eOperation == eOperation
			&& g_axSCSlotOperations[0].m_eSlot == eSlot;
	}

	bool SCTraceIsOrdinalProbeSweep()
	{
		if (g_bSCSlotOperationOverflow
			|| g_uSCSlotOperationCount != static_cast<u_int>(ZM_SAVE_SLOT_COUNT))
		{
			return false;
		}
		for (u_int u = 0u; u < static_cast<u_int>(ZM_SAVE_SLOT_COUNT); ++u)
		{
			if (g_axSCSlotOperations[u].m_eOperation != ZM_SAVE_SLOT_OPERATION_PROBE_SLOT
				|| g_axSCSlotOperations[u].m_eSlot != static_cast<ZM_SAVE_SLOT>(u))
			{
				return false;
			}
		}
		return true;
	}

	struct SCPlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	int SCActiveBuildIndex()
	{
		return g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
	}

	ZM_GameStateManager* SCResolveManager()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_GameStateManager>() : nullptr;
	}

	ZM_UI_MenuStack* SCResolveMenu()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
	}

	Zenith_UIComponent* SCResolveMenuUI()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xID)) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
	}

	bool SCFocusIs(const char* szExpected)
	{
		Zenith_UIComponent* pxUI = SCResolveMenuUI();
		if (pxUI == nullptr || szExpected == nullptr) { return false; }
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		return pxFocused != nullptr
			&& std::strcmp(pxFocused->GetName().c_str(), szExpected) == 0;
	}

	bool SCElementShownAndFocusable(const char* szName, bool bExpectedShown)
	{
		Zenith_UIComponent* pxUI = SCResolveMenuUI();
		Zenith_UI::Zenith_UIElement* pxElement =
			(pxUI != nullptr && szName != nullptr) ? pxUI->FindElement(szName) : nullptr;
		return pxElement != nullptr
			&& pxElement->IsVisible() == bExpectedShown
			&& pxElement->IsFocusable() == bExpectedShown;
	}

	bool SCRowLabelIs(u_int uRow, const char* szExpected)
	{
		Zenith_UIComponent* pxUI = SCResolveMenuUI();
		if (pxUI == nullptr || szExpected == nullptr) { return false; }
		Zenith_UI::Zenith_UIButton* pxRow = pxUI->FindElement<Zenith_UI::Zenith_UIButton>(
			ZM_UI_SaveSlots::RowElementName(uRow));
		return pxRow != nullptr && pxRow->IsVisible() && pxRow->IsFocusable()
			&& std::strcmp(pxRow->GetText().c_str(), szExpected) == 0;
	}

	bool SCFindPlayer(SCPlayerView& xOut)
	{
		xOut = SCPlayerView{};
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

	bool SCReadPlayerPose(Zenith_Maths::Vector3& xCentreOut, float& fYawOut)
	{
		SCPlayerView xPlayer;
		if (!SCFindPlayer(xPlayer) || xPlayer.m_pxCollider == nullptr
			|| !xPlayer.m_pxCollider->HasValidBody())
		{
			return false;
		}
		const Zenith_PhysicsBodyID xBodyID = xPlayer.m_pxCollider->GetBodyID();
		xCentreOut = g_xEngine.Physics().GetBodyPosition(xBodyID);
		fYawOut = ZM_YawFromRotation(g_xEngine.Physics().GetBodyRotation(xBodyID));
		return true;
	}

	float SCPlanarDistance(const Zenith_Maths::Vector3& xA,
		const Zenith_Maths::Vector3& xB)
	{
		const float fX = xA.x - xB.x;
		const float fZ = xA.z - xB.z;
		return std::sqrt(fX * fX + fZ * fZ);
	}

	float SCWrappedAngleDifference(float fA, float fB)
	{
		float fDelta = fA - fB;
		while (fDelta > 3.14159265358979323846f) { fDelta -= 6.28318530717958647692f; }
		while (fDelta < -3.14159265358979323846f) { fDelta += 6.28318530717958647692f; }
		return fDelta;
	}

	bool SCDawnmereReady()
	{
		if (SCActiveBuildIndex() != iSC_DAWNMERE_BUILD
			|| ZM_GameStateManager::IsWarpInProgress()
			|| !g_xEngine.Physics().HasActiveSimulation())
		{
			return false;
		}
		ZM_GameStateManager* pxManager = SCResolveManager();
		SCPlayerView xPlayer;
		return pxManager != nullptr
			&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
			&& SCFindPlayer(xPlayer)
			&& xPlayer.m_pxCollider != nullptr && xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController != nullptr && xPlayer.m_pxController->IsGrounded();
	}

	// ZM-D-176. The new-game arrival gate, stated in the SAME three terms
	// SCDawnmereReady uses for its scene half -- the destination is live, no warp
	// still owns the screen, and physics is actually simulating -- but keyed on
	// the SHIPPED constant rather than on a literal, so the fixture follows the
	// entry point wherever it moves. The player/grounded clauses SCDawnmereReady
	// adds are deliberately absent: this phase drives no input, it only observes
	// the arrival and then warps on, so waiting for a settled capsule would buy
	// nothing and could stall on an interior's tighter spawn.
	bool SCPlayerHomeReady()
	{
		return SCActiveBuildIndex()
				== static_cast<int>(ZM_GameStateManager::uNEW_GAME_BUILD_INDEX)
			&& !ZM_GameStateManager::IsWarpInProgress()
			&& g_xEngine.Physics().HasActiveSimulation();
	}

	bool SCFrontEndTitleReady()
	{
		ZM_GameStateManager* pxManager = SCResolveManager();
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		return SCActiveBuildIndex() == iSC_FRONTEND_BUILD
			&& pxManager != nullptr && pxMenu != nullptr
			&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
			&& !ZM_GameStateManager::IsWarpInProgress()
			&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_TITLE;
	}

	bool SCRequiredAssetsPresent()
	{
		const char* aszRelative[] =
		{
			"Scenes/FrontEnd" ZENITH_SCENE_EXT,
			"Scenes/Dawnmere" ZENITH_SCENE_EXT,
			// ZM-D-176: a new run now begins in PlayerHome, so this fixture
			// depends on that interior's scene file. It is a TRACKED asset
			// (ZM-D-148) and so should always be present, but the guard is listed
			// with the rest rather than assumed: a fresh checkout that somehow
			// lacks it should SKIP with a named prerequisite, not die 600 frames
			// later complaining that a bedroom never settled.
			"Scenes/PlayerHome" ZENITH_SCENE_EXT,
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

	bool SCTestSlotNamesActive()
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

	void SCBuildSlotPath(ZM_SAVE_SLOT eSlot, char* pszOut, size_t uCapacity)
	{
		std::snprintf(pszOut, uCapacity, "%s%s%s", Zenith_SaveData::GetSaveDirectory(),
			ZM_SaveSlots::SlotName(eSlot), ZENITH_SAVE_EXT);
	}

	bool SCReadSlotBytes(ZM_SAVE_SLOT eSlot, Zenith_Vector<u_int8>& xOut)
	{
		xOut.Clear();
		char acPath[ZENITH_MAX_PATH_LENGTH];
		SCBuildSlotPath(eSlot, acPath, sizeof(acPath));
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

	bool SCCorruptSlot(ZM_SAVE_SLOT eSlot)
	{
		Zenith_Vector<u_int8> xBytes;
		if (!SCReadSlotBytes(eSlot, xBytes)
			|| xBytes.GetSize() <= (u_int)sizeof(Zenith_SaveFileHeader))
		{
			return false;
		}
		xBytes.GetBack() = (u_int8)(xBytes.GetBack() ^ 0xffu);
		char acPath[ZENITH_MAX_PATH_LENGTH];
		SCBuildSlotPath(eSlot, acPath, sizeof(acPath));
		Zenith_FileAccess::WriteFile(acPath, xBytes.GetDataPointer(), xBytes.GetSize());
		return ZM_SaveSlots::ProbeSlot(eSlot) == ZM_SAVE_SLOT_DAMAGED;
	}

	bool SCBytesEqual(const Zenith_Vector<u_int8>& xA, const Zenith_Vector<u_int8>& xB)
	{
		return xA.GetSize() == xB.GetSize()
			&& (xA.GetSize() == 0u
				|| std::memcmp(xA.GetDataPointer(), xB.GetDataPointer(), xA.GetSize()) == 0);
	}

	bool SCCanonicalBytes(const ZM_GameState& xState, Zenith_Vector<u_int8>& xOut)
	{
		xOut.Clear();
		Zenith_DataStream xStream;
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
		if (!xStatus.IsOk() || xStream.GetCursor() == 0u) { return false; }
		xOut.Resize((u_int)xStream.GetCursor());
		std::memcpy(xOut.GetDataPointer(), xStream.GetData(), xOut.GetSize());
		return true;
	}

	bool SCCanonicalEqual(const ZM_GameState& xA, const ZM_GameState& xB)
	{
		Zenith_Vector<u_int8> xABytes;
		Zenith_Vector<u_int8> xBBytes;
		return SCCanonicalBytes(xA, xABytes) && SCCanonicalBytes(xB, xBBytes)
			&& SCBytesEqual(xABytes, xBBytes);
	}

	// ★ THE LIVE STATE AFTER A RESUME IS NOT A COPY OF THE SAVED ONE -- ITS POSE
	// HAS BEEN THROUGH THE PHYSICS. Restoring writes the saved yaw into the body
	// as ZM_RotationFromYaw(yaw); a later CaptureWorldPosition reads it back as
	// ZM_YawFromRotation(quat). That is a float round trip through a quaternion
	// and an atan2, so it returns the same yaw only for values that happen to
	// survive it -- and which values those are depends on nothing more meaningful
	// than where the player was facing.
	//
	// So the FINAL clause compares the state in two pieces, and both halves are
	// asserted: everything that is NOT the pose must come back BYTE-IDENTICAL,
	// and the pose itself is checked by the pose clause against the tolerances
	// that exist for exactly this reason (fSC_MAX_PLANAR/VERTICAL/YAW_ERROR).
	// Demanding byte-equality of the whole state asserted that a yaw survives a
	// quaternion round trip, which is a claim about libm, not about save/restore.
	//
	// This helper is deliberately NOT used for the disk readback and scramble
	// comparisons: those hold two in-memory states that never touched a body, and
	// there byte-equality is exactly the right claim.
	ZM_GameState SCWithSavedPose(const ZM_GameState& xLive, const ZM_GameState& xSaved)
	{
		ZM_GameState xCopy = xLive;
		xCopy.m_xWorldPosition = xSaved.m_xWorldPosition;
		return xCopy;
	}

	// WHICH field differs, as a name. SCFieldsEqual answers yes/no, and a bare
	// "fields=false" in the failure line is the one thing a reader cannot act on:
	// this state carries a party, money, story flags, a scene index, a spawn tag,
	// a pose and a yaw, and "one of those" is not a finding.
	const char* SCFirstDifferingField(const ZM_GameState& xA, const ZM_GameState& xB)
	{
		if (xA.m_xParty.Count() != xB.m_xParty.Count()) { return "partyCount"; }
		if (xA.m_uMoney != xB.m_uMoney) { return "money"; }
		if (std::memcmp(xA.m_xStoryFlags.m_auFlags, xB.m_xStoryFlags.m_auFlags,
			uZM_STORY_FLAG_BYTE_COUNT) != 0) { return "storyFlags"; }
		if (xA.m_xWorldPosition.m_uSceneBuildIndex
			!= xB.m_xWorldPosition.m_uSceneBuildIndex) { return "sceneBuildIndex"; }
		if (std::strcmp(xA.m_xWorldPosition.m_szSpawnTag,
			xB.m_xWorldPosition.m_szSpawnTag) != 0) { return "spawnTag"; }
		if (xA.m_xWorldPosition.m_afPosition[0]
			!= xB.m_xWorldPosition.m_afPosition[0]) { return "position.x"; }
		if (xA.m_xWorldPosition.m_afPosition[1]
			!= xB.m_xWorldPosition.m_afPosition[1]) { return "position.y"; }
		if (xA.m_xWorldPosition.m_afPosition[2]
			!= xB.m_xWorldPosition.m_afPosition[2]) { return "position.z"; }
		if (xA.m_xWorldPosition.m_fYaw != xB.m_xWorldPosition.m_fYaw) { return "yaw"; }
		for (u_int u = 0u; u < xA.m_xParty.Count(); ++u)
		{
			const ZM_Monster& xAMon = xA.m_xParty.Get(u);
			const ZM_Monster& xBMon = xB.m_xParty.Get(u);
			if (xAMon.m_eSpecies != xBMon.m_eSpecies) { return "party.species"; }
			if (xAMon.m_uLevel != xBMon.m_uLevel) { return "party.level"; }
			if (xAMon.m_uCurrentHp != xBMon.m_uCurrentHp) { return "party.currentHp"; }
			if (std::strcmp(xAMon.m_szNickname, xBMon.m_szNickname) != 0)
			{
				return "party.nickname";
			}
		}
		return "<none>";
	}

	bool SCFieldsEqual(const ZM_GameState& xA, const ZM_GameState& xB)
	{
		if (xA.m_xParty.Count() != xB.m_xParty.Count()
			|| xA.m_uMoney != xB.m_uMoney
			|| std::memcmp(xA.m_xStoryFlags.m_auFlags, xB.m_xStoryFlags.m_auFlags,
				uZM_STORY_FLAG_BYTE_COUNT) != 0
			|| xA.m_xWorldPosition.m_uSceneBuildIndex
				!= xB.m_xWorldPosition.m_uSceneBuildIndex
			|| std::strcmp(xA.m_xWorldPosition.m_szSpawnTag,
				xB.m_xWorldPosition.m_szSpawnTag) != 0
			|| xA.m_xWorldPosition.m_afPosition[0] != xB.m_xWorldPosition.m_afPosition[0]
			|| xA.m_xWorldPosition.m_afPosition[1] != xB.m_xWorldPosition.m_afPosition[1]
			|| xA.m_xWorldPosition.m_afPosition[2] != xB.m_xWorldPosition.m_afPosition[2]
			|| xA.m_xWorldPosition.m_fYaw != xB.m_xWorldPosition.m_fYaw)
		{
			return false;
		}
		for (u_int u = 0u; u < xA.m_xParty.Count(); ++u)
		{
			const ZM_Monster& xAMon = xA.m_xParty.Get(u);
			const ZM_Monster& xBMon = xB.m_xParty.Get(u);
			if (xAMon.m_eSpecies != xBMon.m_eSpecies
				|| xAMon.m_uLevel != xBMon.m_uLevel
				|| xAMon.m_uCurrentHp != xBMon.m_uCurrentHp
				|| std::strcmp(xAMon.m_szNickname, xBMon.m_szNickname) != 0)
			{
				return false;
			}
		}
		return true;
	}

	bool SCDistinctiveSavedModulesMatch(const ZM_GameState& xState)
	{
		const ZM_Monster* pxBoxProof = xState.m_xBoxes.TryGet(2u, 5u);
		const ZM_Monster& xDayProof = xState.m_xDaycare.m_axParents[0];
		return xState.m_xParty.Count() == 2u
			&& xState.m_xParty.Get(0u).m_eSpecies == ZM_SPECIES_NIBBIN
			&& xState.m_xParty.Get(0u).m_uLevel == 17u
			&& std::strcmp(xState.m_xParty.Get(0u).m_szNickname, "DiskProof") == 0
			&& xState.m_xParty.Get(1u).m_eSpecies == ZM_SPECIES_KINDLET
			&& xState.m_xParty.Get(1u).m_uLevel == 9u
			&& std::strcmp(xState.m_xParty.Get(1u).m_szNickname, "ByteTwin") == 0
			&& xState.m_xBoxes.Count() == 1u
			&& pxBoxProof != nullptr
			&& pxBoxProof->m_eSpecies == ZM_SPECIES_NIBBIN
			&& pxBoxProof->m_uLevel == 11u
			&& std::strcmp(pxBoxProof->m_szNickname, "BoxProof") == 0
			&& xState.GetSeenCount() == 2u
			&& xState.IsSeen(ZM_SPECIES_NIBBIN)
			&& xState.IsSeen(ZM_SPECIES_KINDLET)
			&& xState.GetCaughtCount() == 1u
			&& !xState.IsCaught(ZM_SPECIES_NIBBIN)
			&& xState.IsCaught(ZM_SPECIES_KINDLET)
			&& xState.m_xStoryFlags.Count() == 2u
			&& ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_WARDEN_CLEARED)
			&& ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_ROUTE1_OPEN)
			&& xState.GetBadgeCount() == 2u
			&& xState.HasBadge(1u) && xState.HasBadge(6u)
			&& xState.m_xBag.TotalStackCount() == 1u
			&& xState.m_xBag.GetCount(ZM_ITEM_SALVE) == 7u
			&& xState.m_xBag.GetCount(ZM_ITEM_CATCHORB) == 0u
			&& xState.m_uMoney == 424242u
			&& xState.m_xDaycare.m_uParentCount == 1u
			&& xDayProof.m_eSpecies == ZM_SPECIES_KINDLET
			&& xDayProof.m_uLevel == 14u
			&& std::strcmp(xDayProof.m_szNickname, "DayProof") == 0
			&& !xState.m_xDaycare.m_bEggPresent
			&& xState.m_xDaycare.m_uEggStepsRemaining == 0u
			&& xState.m_xTowerRun.m_uCurrentStreak == 3u
			&& xState.m_xTowerRun.m_uBestStreak == 12u
			&& xState.m_xTowerRun.m_ulSeed == 0x5eed1234ull
			&& xState.m_xWorldPosition.m_uSceneBuildIndex
				== static_cast<u_int>(iSC_DAWNMERE_BUILD)
			&& xState.m_xWorldPosition.m_szSpawnTag[0] != '\0'
			&& xState.m_xOptions.m_eTextSpeed == ZM_TEXT_SPEED_FAST
			&& !xState.m_bPendingWhiteout;
	}

	void SCReleaseMovement()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
	}
}

namespace
{
	enum class SCPhase
	{
		AwaitEmptyTitle,
		ActivateNewGame,
		AwaitNewGameAccepted,
		// ZM-D-176. INSERTED between acceptance and the Dawnmere fixture: the
		// bedroom a new run now begins in. It observes the two-sided arrival and
		// then warps EXPLICITLY into Dawnmere, leaving every later phase against
		// the geometry it was measured on (see the file header).
		AwaitPlayerHome,
		AwaitDawnmere,
		WalkFromTownCenter,
		RestAfterWalk,
		BuildDiskFixtures,
		ProbeValidQueueRefusal,
		AwaitQuitAccepted,
		AwaitLoadedTitle,
		RestoreAutoFixture,
		InstallScramble,
		ObserveScramble,
		ActivateContinue,
		AwaitLoadScreen,
		InspectLoadRows,
		ActivateDamaged,
		DismissDamaged,
		WalkToEmpty,
		ActivateEmpty,
		DismissEmpty,
		WalkToAuto,
		ActivateAuto,
		AwaitAutoPrompt,
		AnswerAutoYes,
		AwaitContinueAccepted,
		AwaitRestoredDawnmere,
		FinalSettle,
		Done,
	};

	SCPhase g_eSCPhase = SCPhase::Done;
	int g_iSCPhaseFrames = 0;
	bool g_bSCActive = false;
	bool g_bSCSkipped = false;
	bool g_bSCFailed = false;
	bool g_bSCAliasesActive = false;
	const char* g_szSCFailure = "test did not reach verification";

	ZM_GameState g_xSCSaved;
	ZM_GameState g_xSCScramble;
	ZM_GameState g_xSCNewGameCanary;
	ZM_GameState g_xSCQueueCanary;
	Zenith_Vector<u_int8> g_xSCDamagedBytesBefore;
	Zenith_Vector<u_int8> g_xSCAutoBytesBeforeContinue;
	Zenith_Maths::Vector3 g_xSCTownCenter = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xSCSavedPose = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xSCScramblePose = Zenith_Maths::Vector3(0.0f);
	float g_fSCSavedYaw = 0.0f;
	float g_fSCFinalPlanarError = 9999.0f;
	float g_fSCFinalVerticalError = 9999.0f;
	float g_fSCFinalYawError = 9999.0f;
	float g_fSCFinalFromTownCenter = 0.0f;
	float g_fSCFinalFromScramble = 0.0f;
	u_int g_uSCNewGameLoadsBefore = 0xffffffffu;
	u_int g_uSCQuitLoadsBefore = 0xffffffffu;
	u_int g_uSCContinueLoadsBefore = 0xffffffffu;
	u_int g_uSCWritesBeforeNewGame = 0xffffffffu;

	bool g_bSCEmptyTitle = false;
	bool g_bSCContinueHidden = false;
	bool g_bSCNewGameFocused = false;
	bool g_bSCNewGameCanaryInstalled = false;
	bool g_bSCNewGameAccepted = false;
	// ZM-D-176. The NEGATIVE half of the acceptance proof. g_bSCNewGameAccepted
	// reads the shipped constant, which proves the FLOW HONOURS THE CONSTANT --
	// and would go on passing if someone quietly reverted that constant to
	// Dawnmere. This latch proves the DESTINATION ACTUALLY MOVED. Neither clause
	// subsumes the other, and pinning the constant's VALUE is the boot units' job.
	bool g_bSCNewGameNotDawnmere = false;
	bool g_bSCNewGamePublishedStarter = false;
	// The two-sided PlayerHome arrival: the right scene, on the right marker.
	bool g_bSCPlayerHomeReady = false;
	bool g_bSCPlayerHomeSpawnTag = false;
	bool g_bSCNewGameProbeTraceExact = false;
	bool g_bSCNewGameNoSlotTouch = false;
	bool g_bSCTitleClosedOnNewGameWarp = false;
	bool g_bSCDawnmereReady = false;
	bool g_bSCWalked = false;
	bool g_bSCRested = false;
	bool g_bSCCaptured = false;
	bool g_bSCSavedStateDistinctive = false;
	bool g_bSCAutoReady = false;
	bool g_bSCAutoDiskReadback = false;
	bool g_bSCDamagedReady = false;
	bool g_bSCEmptySlots = false;
	bool g_bSCDamagedSnapshot = false;
	bool g_bSCQueueRefusalExactRead = false;
	bool g_bSCQueueRefusalStateUnchanged = false;
	bool g_bSCQueueRefusalTransitionUnchanged = false;
	bool g_bSCQuitAccepted = false;
	bool g_bSCQuitReachedTitle = false;
	bool g_bSCDamagedOnlyTitleGate = false;
	bool g_bSCAutoRestoredWhileTitleOpen = false;
	bool g_bSCScrambleInstalled = false;
	bool g_bSCScrambleObservedLater = false;
	bool g_bSCSavedDiffersScramble = false;
	bool g_bSCContinueVisible = false;
	bool g_bSCContinueFocused = false;
	bool g_bSCLoadOpenedByInput = false;
	bool g_bSCLoadRowsExact = false;
	bool g_bSCDamagedRefusalRaised = false;
	bool g_bSCDamagedExactRefusal = false;
	bool g_bSCDamagedNoRead = false;
	bool g_bSCDamagedNoTransition = false;
	bool g_bSCDamagedStateUnchanged = false;
	bool g_bSCDamagedBytesUnchanged = false;
	bool g_bSCEmptyReached = false;
	bool g_bSCEmptyRefusalRaised = false;
	bool g_bSCEmptyExactRefusal = false;
	bool g_bSCEmptyNoRead = false;
	bool g_bSCEmptyNoTransition = false;
	bool g_bSCEmptyStateUnchanged = false;
	bool g_bSCAutoReached = false;
	bool g_bSCAutoPrompt = false;
	bool g_bSCAutoPending = false;
	bool g_bSCAutoPreYesNoRead = false;
	bool g_bSCAutoPreYesNoTransition = false;
	bool g_bSCAutoPreYesStateScrambled = false;
	bool g_bSCAutoYesFocused = false;
	bool g_bSCExactlyOneRead = false;
	bool g_bSCContinueTraceExact = false;
	bool g_bSCLastReadAutoSuccess = false;
	bool g_bSCResumeAccepted = false;
	bool g_bSCStatePublishedFromDisk = false;
	bool g_bSCUIClosedOnContinue = false;
	bool g_bSCRestoredDawnmere = false;
	bool g_bSCFinalCanonicalSaved = false;
	bool g_bSCFinalFieldsSaved = false;
	const char* g_szSCFinalDifferingField = "<unread>";
	bool g_bSCFinalNotScramble = false;
	bool g_bSCFinalPoseSaved = false;
	bool g_bSCFinalNotTownCenter = false;
	bool g_bSCFinalNotScramblePose = false;
	bool g_bSCFinalDamagedUnchanged = false;
	bool g_bSCFinalAutoUnchanged = false;
	bool g_bSCFinalModulesSaved = false;

	void Setup_ZMSaveContinue();
	bool Step_ZMSaveContinue(int iFrame);
	bool Verify_ZMSaveContinue();
}

static const Zenith_AutomatedTest g_xZMSaveContinueTest = {
	"ZM_SaveContinue_Test",
	&Setup_ZMSaveContinue,
	&Step_ZMSaveContinue,
	&Verify_ZMSaveContinue,
	// Phase-local maxima sum to less than 6500 frames (including both 600-frame
	// Dawnmere settles, both 600-frame FrontEnd settles, ZM-D-176's 600-frame
	// PlayerHome settle, every refusal/prompt and every focus walk). The harness
	// cap sits above that sum so a named phase deadline, not this backstop,
	// diagnoses every ordinary stall -- which is why adding the PlayerHome phase
	// RAISED this cap by its own budget instead of eating the existing margin.
	/* maxFrames */ 7200,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMSaveContinueTest);

namespace
{
	// Seed an EXISTING state to the D4 BATCH FIXTURE composition -- a new game plus the
	// Fernfawn grant -- which is what ZM_GameStateManager::ResetGameStateForTests (the
	// test-only reseed) still installs before every test. Several fixtures below
	// nickname or read m_xParty.Get(0u) -- and Get() Zenith_Asserts on an empty party,
	// which breaks the process in EVERY config -- so this must stay the COMPOSED state,
	// never a bare ZM_MakeNewGameState().
	//
	// ★ IT NO LONGER DESCRIBES WHAT RequestNewGame PUBLISHES. See SCSeedPublishedNewGame
	// immediately below.
	//
	// ★ BY REFERENCE, NOT A RETURNING FACTORY, AND THAT IS THE STACK BUDGET TALKING.
	// A `ZM_GameState SCMake...()` would add a frame holding one more live
	// ZM_GameState at the deepest point of every call: under /Od NRVO does not fire,
	// so the helper's named local is copied into the caller's slot while
	// ZM_MakeNewGameState's own local is still alive. At ~100-200 KB apiece against
	// this exe's 1 MB main-thread reserve that is exactly the arithmetic that
	// overflowed __chkstk here before the per-phase split (see the note above
	// SCPhaseAwaitEmptyTitle). Written this way the peak is IDENTICAL to the
	// single-factory call it replaces, and SCWriteDiskFixtures keeps its documented
	// two-ZM_GameState ceiling.
	void SCSeedFernfawnStarter(ZM_GameState& xStateOut)
	{
		xStateOut = ZM_MakeNewGameState();
		ZM_ApplyStarterChoice(xStateOut, ZM_STARTER_CHOICE_FERNFAWN);
	}

	// ★★ WHAT **RequestNewGame** ACTUALLY PUBLISHES, AND IT IS NO LONGER THE ABOVE
	// (ZM-D-188). The lab beat deleted the Fernfawn grant from both production seed
	// sites, so a new game now begins with an EMPTY PARTY and the player picks a
	// starter from Professor Aster. The two helpers are kept SEPARATE rather than one
	// being redefined as the other, because they answer different questions and only
	// one of them moved:
	//
	//   SCSeedFernfawnStarter  -- the DISK FIXTURE composition. Several fixtures below
	//     nickname or read m_xParty.Get(0u), which Zenith_Asserts on an empty party and
	//     therefore BREAKS THE PROCESS in every configuration. It must stay composed,
	//     and it still matches ZM_GameStateManager::ResetGameStateForTests (the
	//     test-only reseed, which still grants -- see the block at that function).
	//   SCSeedPublishedNewGame -- what SCPhaseAwaitNewGameAccepted compares the LIVE
	//     state against after the real Enter edge. Bare, party-free, and the reason
	//     re-adding a grant to RequestNewGame reds this test rather than passing
	//     quietly.
	void SCSeedPublishedNewGame(ZM_GameState& xStateOut)
	{
		xStateOut = ZM_MakeNewGameState();
	}

	void SCFail(const char* szReason)
	{
		g_szSCFailure = szReason;
		g_bSCFailed = true;
		g_eSCPhase = SCPhase::Done;
	}

	void SCResetObservations()
	{
		g_bSCEmptyTitle = false;
		g_bSCContinueHidden = false;
		g_bSCNewGameFocused = false;
		g_bSCNewGameCanaryInstalled = false;
		g_bSCNewGameAccepted = false;
		g_bSCNewGameNotDawnmere = false;
		g_bSCNewGamePublishedStarter = false;
		g_bSCPlayerHomeReady = false;
		g_bSCPlayerHomeSpawnTag = false;
		g_bSCNewGameProbeTraceExact = false;
		g_bSCNewGameNoSlotTouch = false;
		g_bSCTitleClosedOnNewGameWarp = false;
		g_bSCDawnmereReady = false;
		g_bSCWalked = false;
		g_bSCRested = false;
		g_bSCCaptured = false;
		g_bSCSavedStateDistinctive = false;
		g_bSCAutoReady = false;
		g_bSCAutoDiskReadback = false;
		g_bSCDamagedReady = false;
		g_bSCEmptySlots = false;
		g_bSCDamagedSnapshot = false;
		g_bSCQueueRefusalExactRead = false;
		g_bSCQueueRefusalStateUnchanged = false;
		g_bSCQueueRefusalTransitionUnchanged = false;
		g_bSCQuitAccepted = false;
		g_bSCQuitReachedTitle = false;
		g_bSCDamagedOnlyTitleGate = false;
		g_bSCAutoRestoredWhileTitleOpen = false;
		g_bSCScrambleInstalled = false;
		g_bSCScrambleObservedLater = false;
		g_bSCSavedDiffersScramble = false;
		g_bSCContinueVisible = false;
		g_bSCContinueFocused = false;
		g_bSCLoadOpenedByInput = false;
		g_bSCLoadRowsExact = false;
		g_bSCDamagedRefusalRaised = false;
		g_bSCDamagedExactRefusal = false;
		g_bSCDamagedNoRead = false;
		g_bSCDamagedNoTransition = false;
		g_bSCDamagedStateUnchanged = false;
		g_bSCDamagedBytesUnchanged = false;
		g_bSCEmptyReached = false;
		g_bSCEmptyRefusalRaised = false;
		g_bSCEmptyExactRefusal = false;
		g_bSCEmptyNoRead = false;
		g_bSCEmptyNoTransition = false;
		g_bSCEmptyStateUnchanged = false;
		g_bSCAutoReached = false;
		g_bSCAutoPrompt = false;
		g_bSCAutoPending = false;
		g_bSCAutoPreYesNoRead = false;
		g_bSCAutoPreYesNoTransition = false;
		g_bSCAutoPreYesStateScrambled = false;
		g_bSCAutoYesFocused = false;
		g_bSCExactlyOneRead = false;
		g_bSCContinueTraceExact = false;
		g_bSCLastReadAutoSuccess = false;
		g_bSCResumeAccepted = false;
		g_bSCStatePublishedFromDisk = false;
		g_bSCUIClosedOnContinue = false;
		g_bSCRestoredDawnmere = false;
		g_bSCFinalCanonicalSaved = false;
		g_bSCFinalFieldsSaved = false;
		g_szSCFinalDifferingField = "<unread>";
		g_bSCFinalNotScramble = false;
		g_bSCFinalPoseSaved = false;
		g_bSCFinalNotTownCenter = false;
		g_bSCFinalNotScramblePose = false;
		g_bSCFinalDamagedUnchanged = false;
		g_bSCFinalAutoUnchanged = false;
		g_bSCFinalModulesSaved = false;
	}

	// Reset one ZM_GameState global per CALL, so no frame ever holds more than one
	// ZM_GameState temporary: in a Debug (/Od) build the compiler reserves a
	// distinct stack slot for practically every local and temporary in a function,
	// and a single ZM_GameState is on the order of 100-200 KB.
	void SCResetGameState(ZM_GameState& xState)
	{
		xState = ZM_GameState{};
	}

	void Setup_ZMSaveContinue()
	{
		// A failed/skipped predecessor must never leave our fixed trace callback installed.
		ZM_SaveSlots::SetOperationObserverForTests(nullptr);
		SCResetSlotOperationTrace();
		g_eSCPhase = SCPhase::Done;
		g_iSCPhaseFrames = 0;
		g_bSCActive = false;
		g_bSCSkipped = false;
		g_bSCFailed = false;
		g_bSCAliasesActive = false;
		g_szSCFailure = "test did not reach verification";
		SCResetObservations();
		SCResetGameState(g_xSCSaved);
		SCResetGameState(g_xSCScramble);
		SCResetGameState(g_xSCNewGameCanary);
		SCResetGameState(g_xSCQueueCanary);
		g_xSCDamagedBytesBefore.Clear();
		g_xSCAutoBytesBeforeContinue.Clear();
		g_xSCTownCenter = Zenith_Maths::Vector3(0.0f);
		g_xSCSavedPose = Zenith_Maths::Vector3(0.0f);
		g_xSCScramblePose = Zenith_Maths::Vector3(0.0f);
		g_fSCSavedYaw = 0.0f;
		g_fSCFinalPlanarError = 9999.0f;
		g_fSCFinalVerticalError = 9999.0f;
		g_fSCFinalYawError = 9999.0f;
		g_fSCFinalFromTownCenter = 0.0f;
		g_fSCFinalFromScramble = 0.0f;
		g_uSCNewGameLoadsBefore = 0xffffffffu;
		g_uSCQuitLoadsBefore = 0xffffffffu;
		g_uSCContinueLoadsBefore = 0xffffffffu;
		g_uSCWritesBeforeNewGame = 0xffffffffu;

		// A skip bypasses Verify, so the complete asset guard precedes every fixed-dt,
		// scene, alias and disk mutation.
		if (!SCRequiredAssetsPresent())
		{
			g_bSCSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_SaveContinue] FrontEnd/PlayerHome/Dawnmere assets are absent");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		ZM_SaveSlots::SetTestSlotNamesForTests(true);
		g_bSCAliasesActive = SCTestSlotNamesActive();
		if (!g_bSCAliasesActive)
		{
			SCFail("_Test slot aliases did not activate; refusing all disk access");
			g_bSCActive = true;
			return;
		}
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		SCResetSlotOperationTrace();
		ZM_SaveSlots::SetOperationObserverForTests(&SCObserveSlotOperation);
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		Zenith_InputSimulator::SetFixedDt(fSC_FIXED_DT);
		g_xEngine.Scenes().LoadSceneByIndex(iSC_FRONTEND_BUILD, SCENE_LOAD_SINGLE);
		g_eSCPhase = SCPhase::AwaitEmptyTitle;
		g_bSCActive = true;
	}

	bool SCWalkDownTo(const char* szTarget)
	{
		if (SCFocusIs(szTarget)) { return true; }
		if ((g_iSCPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
		}
		return false;
	}

	// ---- Per-phase drivers ---------------------------------------------------
	// Every phase body of the SC5 phase machine lives in its OWN file-local
	// function and Step_ZMSaveContinue is a THIN dispatch over them. This is
	// load-bearing, not stylistic: in a Debug (/Od) build the compiler reserves a
	// distinct stack slot for practically every local in a function, and a single
	// ZM_GameState is on the order of 100-200 KB (6-mon party + 16x30 box
	// storage). With all 29 phase bodies inline, Step's ONE frame aggregated
	// ~1.3 MB of locals and overflowed the exe's 1 MB main-thread stack reserve
	// inside __chkstk on the very first call. Split per phase, no frame holds
	// more than two ZM_GameState values -- most hold none. The extraction is
	// semantics-preserving: each function reads and writes the same file-scope
	// phase/latch globals the inline body did, returns true to keep stepping and
	// false to stop, and the shared ++g_iSCPhaseFrames stays in the dispatch.

	bool SCPhaseAwaitEmptyTitle()
	{
		if (SCFrontEndTitleReady())
		{
			ZM_GameStateManager* pxManager = SCResolveManager();
			ZM_GameState* pxLive = nullptr;
			g_bSCEmptyTitle = ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_2) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_EMPTY;
			g_bSCContinueHidden = SCElementShownAndFocusable(
				ZM_UI_TitleMenu::szCONTINUE_NAME, false);
			g_bSCNewGameFocused = SCFocusIs(ZM_UI_TitleMenu::szNEW_GAME_NAME)
				&& SCElementShownAndFocusable(ZM_UI_TitleMenu::szNEW_GAME_NAME, true);
			g_uSCNewGameLoadsBefore = pxManager != nullptr
				? pxManager->GetIssuedLoadRequestCount() : 0xffffffffu;
			g_uSCWritesBeforeNewGame = Zenith_SaveData::GetWrittenSlotsForTest().GetSize();

			// New Game must replace a real, valid non-starter rather than merely leave the
			// manager's default starter untouched. Keep the transient whiteout latch false
			// so only the simulated Enter edge can own the next transition.
			SCSeedFernfawnStarter(g_xSCNewGameCanary);
			std::snprintf(g_xSCNewGameCanary.m_xParty.Get(0u).m_szNickname,
				sizeof(g_xSCNewGameCanary.m_xParty.Get(0u).m_szNickname), "NewCanary");
			g_xSCNewGameCanary.m_uMoney = 8080u;
			ZM_SetStoryFlag(g_xSCNewGameCanary, ZM_STORY_FLAG_GYM1_DEFEATED, true);
			g_xSCNewGameCanary.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
			g_xSCNewGameCanary.m_bPendingWhiteout = false;
			g_bSCNewGameCanaryInstalled =
				ZM_GameStateManager::TryGetGameState(pxLive) && pxLive != nullptr;
			if (g_bSCNewGameCanaryInstalled)
			{
				*pxLive = g_xSCNewGameCanary;
				g_bSCNewGameCanaryInstalled = SCCanonicalEqual(*pxLive, g_xSCNewGameCanary)
					&& !pxLive->m_bPendingWhiteout;
			}
			// Without a verified non-starter in the live slot, the accepted-New-Game
			// observation below could pass on the manager's untouched default starter
			// and the "New Game REPLACES real state" claim would be vacuous.
			if (!g_bSCNewGameCanaryInstalled)
			{
				SCFail("could not install a live non-starter canary before New Game");
				return false;
			}
			SCResetSlotOperationTrace();
			g_eSCPhase = SCPhase::ActivateNewGame;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_FRONTEND_DEADLINE)
		{
			SCFail("clean FrontEnd never auto-raised a settled TITLE"); return false;
		}
		return true;
	}

	bool SCPhaseActivateNewGame()
	{
		if (g_iSCPhaseFrames == 2)
		{
			if (!SCFocusIs(ZM_UI_TitleMenu::szNEW_GAME_NAME))
			{
				SCFail("New Game lost real focus before its Enter edge"); return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		if (g_iSCPhaseFrames >= 3)
		{
			g_eSCPhase = SCPhase::AwaitNewGameAccepted;
			g_iSCPhaseFrames = 0;
		}
		return true;
	}

	bool SCPhaseAwaitNewGameAccepted()
	{
		ZM_GameStateManager* pxManager = SCResolveManager();
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		ZM_GameState* pxLive = nullptr;
		if (pxManager != nullptr && pxMenu != nullptr
			&& ZM_GameStateManager::IsWarpInProgress())
		{
			// ZM-D-188: the PUBLISHED new game, which is partyless. Deliberately NOT
			// SCSeedFernfawnStarter -- comparing against the composed fixture here would
			// red the moment the lab beat landed, and "fixing" it by re-adding the grant
			// to production is exactly the regression this clause now guards.
			ZM_GameState xStarter;
			SCSeedPublishedNewGame(xStarter);
			// ZM-D-176, and READ FROM THE CONSTANT: whatever the shipped new-game
			// destination is, the real Enter edge must have queued a transition to
			// exactly it. GetTargetBuildIndex() is u_int (ZM_GameStateManager.h),
			// so the comparison is made in that type rather than against an int.
			g_bSCNewGameAccepted = pxManager->GetTargetBuildIndex()
				== ZM_GameStateManager::uNEW_GAME_BUILD_INDEX;
			// ...and the negative that a reverted constant cannot satisfy.
			g_bSCNewGameNotDawnmere = pxManager->GetTargetBuildIndex()
				!= static_cast<u_int>(iSC_DAWNMERE_BUILD);
			g_bSCNewGamePublishedStarter =
				ZM_GameStateManager::TryGetGameState(pxLive) && pxLive != nullptr
				&& SCCanonicalEqual(*pxLive, xStarter)
				&& !SCCanonicalEqual(*pxLive, g_xSCNewGameCanary)
				&& !pxLive->m_bPendingWhiteout;
			g_bSCNewGameProbeTraceExact = SCTraceIsOrdinalProbeSweep();
			g_bSCNewGameNoSlotTouch =
				Zenith_SaveData::GetWrittenSlotsForTest().GetSize() == g_uSCWritesBeforeNewGame
				&& g_bSCNewGameProbeTraceExact
				&& !Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0))
				&& !Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_1))
				&& !Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_2))
				&& !Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_AUTO));
			g_bSCTitleClosedOnNewGameWarp = pxMenu->GetTopScreen() != ZM_MENU_SCREEN_TITLE
				&& SCElementShownAndFocusable(ZM_UI_TitleMenu::szNEW_GAME_NAME, false);
			// THE replacement proof: the live state after the real Enter edge must be
			// the fresh starter and must NOT still be the installed canary. Consumed
			// here, not just in Verify -- a New Game that left the canary in place
			// makes every downstream disk claim meaningless.
			if (!g_bSCNewGamePublishedStarter)
			{
				SCFail("New Game did not replace the installed canary with a fresh starter");
				return false;
			}
			g_eSCPhase = SCPhase::AwaitPlayerHome;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_TRANSITION_DEADLINE)
		{
			SCFail("real New Game Enter was not accepted as a PlayerHome transition");
			return false;
		}
		return true;
	}

	// ZM-D-176. THE NEW-GAME ARRIVAL, PROVEN ON BOTH AXES, then handed to the
	// shipped Dawnmere fixture by an EXPLICIT warp (see the file header for why
	// the walk/save/resume legs are not re-homed into the bedroom).
	//
	// The scene half alone is not the claim: a warp that loaded the right room and
	// then placed the player on some other marker would satisfy it. The tag half
	// is what makes "landed at the authored door" a checked property, and it is
	// read from ZM_GameStateManager::szNEW_GAME_SPAWN_TAG rather than typed here.
	//
	// No input is held in this phase, and the "Door" marker does not overlap the
	// PlayerHomeExitTrigger, so the player cannot re-warp out from under us before
	// the observation is taken.
	bool SCPhaseAwaitPlayerHome()
	{
		if (SCPlayerHomeReady())
		{
			ZM_GameStateManager* pxManager = SCResolveManager();
			const char* szArrived =
				ZM_GameStateManager::GetActiveSceneArrivedSpawnTag();
			g_bSCPlayerHomeReady = pxManager != nullptr
				&& pxManager->GetIssuedLoadRequestCount()
					== g_uSCNewGameLoadsBefore + 1u;
			g_bSCPlayerHomeSpawnTag = szArrived != nullptr
				&& std::strcmp(szArrived,
					ZM_GameStateManager::szNEW_GAME_SPAWN_TAG) == 0;
			if (!g_bSCPlayerHomeReady || !g_bSCPlayerHomeSpawnTag)
			{
				SCFail("New Game reached PlayerHome but not on its authored marker "
					"(or issued the wrong number of loads getting there)");
				return false;
			}
			// The one deliberate hand-off. Everything downstream of here is the
			// SHIPPED Dawnmere fixture, unchanged and calibrated to Dawnmere's
			// geometry; this warp is what puts it back on that ground.
			if (!ZM_GameStateManager::RequestWarp(
				static_cast<u_int>(iSC_DAWNMERE_BUILD), "TownCenter"))
			{
				SCFail("the explicit PlayerHome -> Dawnmere warp was refused");
				return false;
			}
			g_eSCPhase = SCPhase::AwaitDawnmere;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_PLAYERHOME_DEADLINE)
		{
			SCFail("New Game never settled in PlayerHome"); return false;
		}
		return true;
	}

	bool SCPhaseAwaitDawnmere()
	{
		if (SCDawnmereReady())
		{
			ZM_GameStateManager* pxManager = SCResolveManager();
			float fYaw = 0.0f;
			// ZM-D-176: TWO loads, not one -- New Game's own load of PlayerHome,
			// plus the explicit warp this fixture issues to get back onto
			// Dawnmere's geometry. A run that somehow reached Dawnmere in a single
			// load did not go through the new entry point at all.
			g_bSCDawnmereReady = pxManager != nullptr
				&& pxManager->GetIssuedLoadRequestCount() == g_uSCNewGameLoadsBefore + 2u
				&& SCReadPlayerPose(g_xSCTownCenter, fYaw);
			g_eSCPhase = SCPhase::WalkFromTownCenter;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DAWNMERE_DEADLINE)
		{
			SCFail("New Game never settled in Dawnmere"); return false;
		}
		return true;
	}

	bool SCPhaseWalkFromTownCenter()
	{
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		Zenith_Maths::Vector3 xPose(0.0f);
		float fYaw = 0.0f;
		if (SCReadPlayerPose(xPose, fYaw)
			&& SCPlanarDistance(xPose, g_xSCTownCenter) >= fSC_MIN_WALK_DISTANCE)
		{
			SCReleaseMovement();
			g_bSCWalked = true;
			g_eSCPhase = SCPhase::RestAfterWalk;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_WALK_DEADLINE)
		{
			SCReleaseMovement();
			SCFail("simulated W never moved the real player away from TownCenter");
			return false;
		}
		return true;
	}

	bool SCPhaseRestAfterWalk()
	{
		SCReleaseMovement();
		if (g_iSCPhaseFrames >= 30 && SCDawnmereReady())
		{
			g_bSCRested = true;
			g_eSCPhase = SCPhase::BuildDiskFixtures;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_REST_DEADLINE)
		{
			SCFail("walked player never re-settled before capture"); return false;
		}
		return true;
	}

	// The fixture phase is itself split in TWO so the monster-staging frame stays
	// free of disk-read state and the disk frame stays free of monsters:
	// SCBuildSavedStateFixture owns the three ZM_Monster staging records and NO
	// ZM_GameState local at all -- SCSeedFernfawnStarter writes the composed seed
	// straight into the global, which is why it takes a reference rather than
	// returning one -- while SCWriteDiskFixtures owns the
	// read-back candidate and the damage source (two ZM_GameState -- the
	// per-function ceiling noted above). Call order is the original straight-line
	// order -- build the saved state completely, then touch disk.
	void SCBuildSavedStateFixture()
	{
		SCSeedFernfawnStarter(g_xSCSaved);
		g_xSCSaved.m_xParty = ZM_Party{};
		ZM_Monster xLead = ZM_BuildMonsterRecord(ZM_SPECIES_NIBBIN, 17u);
		std::snprintf(xLead.m_szNickname, sizeof(xLead.m_szNickname), "DiskProof");
		if (xLead.m_uCurrentHp > 4u) { xLead.m_uCurrentHp -= 4u; }
		ZM_Monster xBench = ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 9u);
		std::snprintf(xBench.m_szNickname, sizeof(xBench.m_szNickname), "ByteTwin");
		g_xSCSaved.m_xParty.Add(xLead);
		g_xSCSaved.m_xParty.Add(xBench);

		ZM_Monster xBoxProof = ZM_BuildMonsterRecord(ZM_SPECIES_NIBBIN, 11u);
		std::snprintf(xBoxProof.m_szNickname, sizeof(xBoxProof.m_szNickname), "BoxProof");
		g_xSCSaved.m_xBoxes = ZM_BoxStorage{};
		const bool bBoxStored = g_xSCSaved.m_xBoxes.StoreAt(2u, 5u, xBoxProof);

		g_xSCSaved.m_xSeen = ZM_SpeciesSet{};
		g_xSCSaved.m_xCaught = ZM_SpeciesSet{};
		g_xSCSaved.MarkSeen(ZM_SPECIES_NIBBIN);
		g_xSCSaved.MarkCaught(ZM_SPECIES_KINDLET);
		ZM_SetStoryFlag(g_xSCSaved, ZM_STORY_FLAG_WARDEN_CLEARED, true);
		ZM_SetStoryFlag(g_xSCSaved, ZM_STORY_FLAG_ROUTE1_OPEN, true);
		g_xSCSaved.m_uBadgeMask = 0u;
		const bool bBadgesAwarded = g_xSCSaved.AwardBadge(1u)
			&& g_xSCSaved.AwardBadge(6u);
		g_xSCSaved.m_xBag = ZM_Bag{};
		const bool bBagSeeded = g_xSCSaved.m_xBag.Add(ZM_ITEM_SALVE, 7u);
		g_xSCSaved.m_uMoney = 424242u;

		g_xSCSaved.m_xDaycare = ZM_DaycareProgress{};
		g_xSCSaved.m_xDaycare.m_uParentCount = 1u;
		g_xSCSaved.m_xDaycare.m_axParents[0] =
			ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 14u);
		std::snprintf(g_xSCSaved.m_xDaycare.m_axParents[0].m_szNickname,
			sizeof(g_xSCSaved.m_xDaycare.m_axParents[0].m_szNickname), "DayProof");
		g_xSCSaved.m_xTowerRun.m_uCurrentStreak = 3u;
		g_xSCSaved.m_xTowerRun.m_uBestStreak = 12u;
		g_xSCSaved.m_xTowerRun.m_ulSeed = 0x5eed1234ull;
		g_xSCSaved.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		g_xSCSaved.m_bPendingWhiteout = false;
		g_bSCCaptured = ZM_GameStateManager::CaptureWorldPosition(g_xSCSaved);
		g_xSCSavedPose = Zenith_Maths::Vector3(
			g_xSCSaved.m_xWorldPosition.m_afPosition[0],
			g_xSCSaved.m_xWorldPosition.m_afPosition[1],
			g_xSCSaved.m_xWorldPosition.m_afPosition[2]);
		g_fSCSavedYaw = g_xSCSaved.m_xWorldPosition.m_fYaw;
		g_bSCSavedStateDistinctive = bBoxStored && bBadgesAwarded && bBagSeeded
			&& SCDistinctiveSavedModulesMatch(g_xSCSaved)
			&& SCPlanarDistance(g_xSCSavedPose, g_xSCTownCenter)
				>= fSC_MIN_WALK_DISTANCE;
	}

	void SCWriteDiskFixtures()
	{
		const Zenith_Status xAutoWrite = ZM_SaveSlots::WriteState(
			g_xSCSaved, ZM_SAVE_SLOT_AUTO);
		g_bSCAutoReady = xAutoWrite.IsOk()
			&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_READY;
		ZM_GameState xAutoReadback;
		g_bSCAutoDiskReadback = ZM_SaveSlots::ReadState(
			ZM_SAVE_SLOT_AUTO, xAutoReadback).IsOk()
			&& SCCanonicalEqual(g_xSCSaved, xAutoReadback);

		ZM_GameState xDamageSource;
		SCSeedFernfawnStarter(xDamageSource);
		g_bSCDamagedReady = ZM_SaveSlots::WriteState(
			xDamageSource, ZM_SAVE_SLOT_0).IsOk()
			&& SCCorruptSlot(ZM_SAVE_SLOT_0)
			&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0) == ZM_SAVE_SLOT_DAMAGED;
		g_bSCDamagedSnapshot = SCReadSlotBytes(
			ZM_SAVE_SLOT_0, g_xSCDamagedBytesBefore);
		g_bSCEmptySlots = ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1) == ZM_SAVE_SLOT_EMPTY
			&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_2) == ZM_SAVE_SLOT_EMPTY;
	}

	bool SCPhaseBuildDiskFixtures()
	{
		SCBuildSavedStateFixture();
		SCWriteDiskFixtures();
		if (!g_bSCCaptured || !g_bSCSavedStateDistinctive || !g_bSCAutoReady
			|| !g_bSCAutoDiskReadback || !g_bSCDamagedReady
			|| !g_bSCDamagedSnapshot || !g_bSCEmptySlots)
		{
			SCFail("could not establish the disk-authentic saved/damaged/empty fixture");
			return false;
		}

		// Clear engine instrumentation only: the real files remain. This guarantees the
		// Continue cannot be served by a staged readback or an old write record.
		Zenith_SaveData::ClearForTest();
		g_eSCPhase = SCPhase::ProbeValidQueueRefusal;
		g_iSCPhaseFrames = 0; return true;
	}

	bool SCPhaseProbeValidQueueRefusal()
	{
		ZM_GameStateManager* pxManager = SCResolveManager();
		ZM_GameState* pxLive = nullptr;
		if (pxManager == nullptr
			|| !ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr)
		{
			SCFail("manager/live state unavailable for synchronous busy-queue proof");
			return false;
		}

		SCSeedFernfawnStarter(g_xSCQueueCanary);
		std::snprintf(g_xSCQueueCanary.m_xParty.Get(0u).m_szNickname,
			sizeof(g_xSCQueueCanary.m_xParty.Get(0u).m_szNickname), "QueueCanary");
		g_xSCQueueCanary.m_uMoney = 606060u;
		ZM_SetStoryFlag(g_xSCQueueCanary, ZM_STORY_FLAG_GYM1_DEFEATED, true);
		g_xSCQueueCanary.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		g_xSCQueueCanary.m_bPendingWhiteout = true;
		*pxLive = g_xSCQueueCanary;

		const bool bPrequeued = ZM_GameStateManager::RequestWarp(
			static_cast<u_int>(iSC_DAWNMERE_BUILD), "TownCenter");
		const ZM_WARP_TRANSITION_STATE eTransitionBefore = pxManager->GetTransitionState();
		const u_int uTargetBefore = pxManager->GetTargetBuildIndex();
		char acTargetTagBefore[uZM_WORLD_SPAWN_TAG_CAPACITY] = {};
		std::snprintf(acTargetTagBefore, sizeof(acTargetTagBefore), "%s",
			pxManager->GetTargetSpawnTag());
		const u_int uLoadCountBefore = pxManager->GetIssuedLoadRequestCount();
		const bool bResumePendingBefore = pxManager->IsResumePending();

		SCResetSlotOperationTrace();
		const Zenith_Status xRefused =
			ZM_GameStateManager::RequestContinue(ZM_SAVE_SLOT_AUTO);
		g_bSCQueueRefusalExactRead = bPrequeued && !xRefused.IsOk()
			&& xRefused.Error() == Zenith_ErrorCode::QUEUE_FULL
			&& SCTraceIsExactlyOne(
				ZM_SAVE_SLOT_OPERATION_READ_STATE, ZM_SAVE_SLOT_AUTO);
		g_bSCQueueRefusalStateUnchanged = SCCanonicalEqual(*pxLive, g_xSCQueueCanary)
			&& SCFieldsEqual(*pxLive, g_xSCQueueCanary)
			&& pxLive->m_bPendingWhiteout == g_xSCQueueCanary.m_bPendingWhiteout;
		g_bSCQueueRefusalTransitionUnchanged = bPrequeued
			&& pxManager->GetTransitionState() == eTransitionBefore
			&& pxManager->GetTargetBuildIndex() == uTargetBefore
			&& std::strcmp(pxManager->GetTargetSpawnTag(), acTargetTagBefore) == 0
			&& pxManager->GetIssuedLoadRequestCount() == uLoadCountBefore
			&& pxManager->IsResumePending() == bResumePendingBefore;

		// The synthetic busy transition and transient whiteout canary must not survive
		// this callback. No runtime frame sees either one.
		ZM_GameStateManager::ResetRuntimeStateForTests();
		SCSeedFernfawnStarter(*pxLive);
		SCResetSlotOperationTrace();
		if (!g_bSCQueueRefusalExactRead || !g_bSCQueueRefusalStateUnchanged
			|| !g_bSCQueueRefusalTransitionUnchanged)
		{
			SCFail("busy RequestContinue did not read-local then refuse without mutation");
			return false;
		}

		// Snapshot the Auto fixture's raw bytes BEFORE it leaves disk: the title is
		// about to be proven live on a DAMAGED-only disk, and RestoreAutoFixture
		// later puts these EXACT bytes back, so the Continue below reads the same
		// disk file the fixture phase wrote and round-tripped.
		if (!SCReadSlotBytes(ZM_SAVE_SLOT_AUTO, g_xSCAutoBytesBeforeContinue))
		{
			SCFail("could not snapshot the Auto fixture bytes before deleting it");
			return false;
		}
		if (!ZM_SaveSlots::DeleteSlotFile(ZM_SAVE_SLOT_AUTO))
		{
			SCFail("could not delete Auto before the damaged-only title gate");
			return false;
		}
		ZM_GameStateManager* pxResetManager = SCResolveManager();
		g_uSCQuitLoadsBefore = pxResetManager != nullptr
			? pxResetManager->GetIssuedLoadRequestCount() : 0xffffffffu;
		g_bSCQuitAccepted = ZM_GameStateManager::RequestQuitToFrontEnd();
		g_eSCPhase = SCPhase::AwaitQuitAccepted;
		g_iSCPhaseFrames = 0; return true;
	}

	bool SCPhaseAwaitQuitAccepted()
	{
		if (g_bSCQuitAccepted && ZM_GameStateManager::IsWarpInProgress())
		{
			g_eSCPhase = SCPhase::AwaitLoadedTitle;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_TRANSITION_DEADLINE)
		{
			SCFail("ordinary quit-to-FrontEnd request was not accepted"); return false;
		}
		return true;
	}

	bool SCPhaseAwaitLoadedTitle()
	{
		if (SCFrontEndTitleReady())
		{
			ZM_GameStateManager* pxManager = SCResolveManager();
			g_bSCQuitReachedTitle = pxManager != nullptr
				&& pxManager->GetIssuedLoadRequestCount() == g_uSCQuitLoadsBefore + 1u;
			// THE point of deleting Auto before the quit: with ONLY a DAMAGED slot
			// on disk, Continue must STILL be shown and focusable -- DAMAGED counts
			// as occupied, because the LOAD screen is how a recoverable file is
			// surfaced. If a damaged-only disk hid Continue, New Game could silently
			// clobber that file.
			g_bSCDamagedOnlyTitleGate =
				ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0) == ZM_SAVE_SLOT_DAMAGED
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_2) == ZM_SAVE_SLOT_EMPTY
				&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_EMPTY
				&& SCElementShownAndFocusable(ZM_UI_TitleMenu::szCONTINUE_NAME, true);
			if (!g_bSCDamagedOnlyTitleGate)
			{
				SCFail("title did not keep Continue live on a DAMAGED-only disk");
				return false;
			}
			g_eSCPhase = SCPhase::RestoreAutoFixture;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_FRONTEND_DEADLINE)
		{
			SCFail("quit never settled at the auto-raised FrontEnd TITLE"); return false;
		}
		return true;
	}

	bool SCPhaseRestoreAutoFixture()
	{
		// The Auto fixture returns BEFORE any title input, restored from the exact
		// bytes snapshotted ahead of its deletion -- the same disk file the fixture
		// phase wrote and round-tripped, put back with a straight file write. The
		// title stays OPEN throughout: the Continue Enter below re-probes every
		// slot uncached, so this restore must not (and does not) rely on any screen
		// refresh to become visible.
		if (g_xSCAutoBytesBeforeContinue.GetSize() == 0u)
		{
			SCFail("no Auto fixture bytes survived to restore");
			return false;
		}
		char acPath[ZENITH_MAX_PATH_LENGTH];
		SCBuildSlotPath(ZM_SAVE_SLOT_AUTO, acPath, sizeof(acPath));
		Zenith_FileAccess::WriteFile(acPath, g_xSCAutoBytesBeforeContinue.GetDataPointer(),
			g_xSCAutoBytesBeforeContinue.GetSize());
		Zenith_Vector<u_int8> xRestored;
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		g_bSCAutoRestoredWhileTitleOpen = pxMenu != nullptr
			&& pxMenu->GetTopScreen() == ZM_MENU_SCREEN_TITLE
			&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_READY
			&& SCReadSlotBytes(ZM_SAVE_SLOT_AUTO, xRestored)
			&& SCBytesEqual(g_xSCAutoBytesBeforeContinue, xRestored);
		if (!g_bSCAutoRestoredWhileTitleOpen)
		{
			SCFail("could not restore the Auto disk fixture while the title was open");
			return false;
		}
		g_eSCPhase = SCPhase::InstallScramble;
		g_iSCPhaseFrames = 0; return true;
	}

	bool SCPhaseInstallScramble()
	{
		ZM_GameState* pxLive = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr)
		{
			SCFail("persistent live state was unavailable for deliberate scramble");
			return false;
		}
		SCSeedFernfawnStarter(g_xSCScramble);
		g_xSCScramble.m_uMoney = 73u;
		ZM_SetStoryFlag(g_xSCScramble, ZM_STORY_FLAG_GYM1_DEFEATED, true);
		g_xSCScramble.m_xWorldPosition = g_xSCSaved.m_xWorldPosition;
		g_xSCScramble.m_xWorldPosition.m_afPosition[0] += 4.0f;
		g_xSCScramble.m_xWorldPosition.m_fYaw += 0.75f;
		g_xSCScramblePose = Zenith_Maths::Vector3(
			g_xSCScramble.m_xWorldPosition.m_afPosition[0],
			g_xSCScramble.m_xWorldPosition.m_afPosition[1],
			g_xSCScramble.m_xWorldPosition.m_afPosition[2]);
		g_bSCSavedDiffersScramble = !SCCanonicalEqual(g_xSCSaved, g_xSCScramble);
		*pxLive = g_xSCScramble;
		g_bSCScrambleInstalled = true;
		g_eSCPhase = SCPhase::ObserveScramble;
		g_iSCPhaseFrames = 0; return true;
	}

	bool SCPhaseObserveScramble()
	{
		if (g_iSCPhaseFrames < iSC_SCRAMBLE_SETTLE) { return true; }
		ZM_GameState* pxLive = nullptr;
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		ZM_GameStateManager* pxManager = SCResolveManager();
		if (!ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr
			|| pxMenu == nullptr || pxManager == nullptr)
		{
			SCFail("live state/menu vanished before later-frame scramble observation");
			return false;
		}
		g_bSCScrambleObservedLater = SCCanonicalEqual(*pxLive, g_xSCScramble)
			&& !SCCanonicalEqual(*pxLive, g_xSCSaved);
		g_bSCContinueVisible = SCElementShownAndFocusable(
			ZM_UI_TitleMenu::szCONTINUE_NAME, true);
		g_bSCContinueFocused = SCFocusIs(ZM_UI_TitleMenu::szCONTINUE_NAME);
		g_uSCContinueLoadsBefore = pxManager->GetIssuedLoadRequestCount();
		if (!g_bSCScrambleObservedLater || !g_bSCSavedDiffersScramble
			|| !g_bSCContinueVisible || !g_bSCContinueFocused)
		{
			SCFail("scramble/Continue precondition failed (DAMAGED may have hidden Continue)");
			return false;
		}
		// The continue-observation window opens HERE. From the real Continue Enter to
		// the load Yes, the flow may PROBE (two uncached four-slot sweeps: the title
		// re-probe on the input boundary and the LOAD screen's Open) but must perform
		// exactly ONE READ_STATE -- on Auto, at the Yes -- and no WRITE_STATE. The
		// title-gate and fixture-restore probes above stay out of the count.
		SCResetSlotOperationTrace();
		g_eSCPhase = SCPhase::ActivateContinue;
		g_iSCPhaseFrames = 0; return true;
	}

	bool SCPhaseActivateContinue()
	{
		if (g_iSCPhaseFrames == 2)
		{
			if (!SCFocusIs(ZM_UI_TitleMenu::szCONTINUE_NAME))
			{
				SCFail("Continue lost real focus before Enter"); return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		if (g_iSCPhaseFrames >= 3)
		{
			g_eSCPhase = SCPhase::AwaitLoadScreen;
			g_iSCPhaseFrames = 0;
		}
		return true;
	}

	bool SCPhaseAwaitLoadScreen()
	{
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE
			&& pxMenu->GetSaveScreen().GetMode() == ZM_SAVE_SCREEN_MODE_LOAD)
		{
			g_bSCLoadOpenedByInput = pxMenu->GetDepth() == 2u;
			g_eSCPhase = SCPhase::InspectLoadRows;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_SCREEN_DEADLINE)
		{
			SCFail("real Continue Enter never opened LOAD above TITLE"); return false;
		}
		return true;
	}

	bool SCPhaseInspectLoadRows()
	{
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		g_bSCLoadRowsExact = pxMenu != nullptr
			&& pxMenu->GetSaveScreen().GetRowStatus(0u) == ZM_SAVE_SLOT_DAMAGED
			&& pxMenu->GetSaveScreen().GetRowStatus(1u) == ZM_SAVE_SLOT_EMPTY
			&& pxMenu->GetSaveScreen().GetRowStatus(2u) == ZM_SAVE_SLOT_EMPTY
			&& pxMenu->GetSaveScreen().GetRowStatus(3u) == ZM_SAVE_SLOT_READY
			&& SCRowLabelIs(0u, "Slot 1 -- Damaged")
			&& SCRowLabelIs(1u, "Slot 2 -- Empty")
			&& SCRowLabelIs(2u, "Slot 3 -- Empty")
			&& SCRowLabelIs(3u, "Auto -- Ready")
			&& SCFocusIs(ZM_UI_SaveSlots::RowElementName(0u));
		if (!g_bSCLoadRowsExact)
		{
			SCFail("LOAD did not surface exact DAMAGED/EMPTY/EMPTY/READY rows");
			return false;
		}
		g_eSCPhase = SCPhase::ActivateDamaged;
		g_iSCPhaseFrames = 0; return true;
	}

	bool SCPhaseActivateDamaged()
	{
		if (g_iSCPhaseFrames == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
		{
			ZM_GameState* pxLive = nullptr;
			Zenith_Vector<u_int8> xNow;
			ZM_GameStateManager* pxManager = SCResolveManager();
			g_bSCDamagedRefusalRaised = true;
			// EXACTLY a refusal: a read-to-the-end line, never an armed yes/no --
			// a DAMAGED row that offered a load choice is the bug this arm exists
			// to catch.
			g_bSCDamagedExactRefusal = !pxMenu->IsDialogueAwaitingChoice();
			g_bSCDamagedNoRead = pxMenu->GetLoadReadCount() == 0u;
			g_bSCDamagedNoTransition = pxManager != nullptr
				&& pxManager->GetIssuedLoadRequestCount() == g_uSCContinueLoadsBefore
				&& !ZM_GameStateManager::IsWarpInProgress();
			g_bSCDamagedStateUnchanged = ZM_GameStateManager::TryGetGameState(pxLive)
				&& pxLive != nullptr && SCCanonicalEqual(*pxLive, g_xSCScramble);
			g_bSCDamagedBytesUnchanged = SCReadSlotBytes(ZM_SAVE_SLOT_0, xNow)
				&& SCBytesEqual(g_xSCDamagedBytesBefore, xNow);
			g_eSCPhase = SCPhase::DismissDamaged;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DIALOGUE_DEADLINE)
		{
			SCFail("DAMAGED row did not raise its refusal dialogue"); return false;
		}
		return true;
	}

	bool SCPhaseDismissDamaged()
	{
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE)
		{
			g_eSCPhase = SCPhase::WalkToEmpty;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DIALOGUE_DEADLINE)
		{
			SCFail("DAMAGED refusal did not return to LOAD"); return false;
		}
		if ((g_iSCPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		return true;
	}

	bool SCPhaseWalkToEmpty()
	{
		if (SCWalkDownTo(ZM_UI_SaveSlots::RowElementName(1u)))
		{
			g_bSCEmptyReached = true;
			g_eSCPhase = SCPhase::ActivateEmpty;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_FOCUS_DEADLINE)
		{
			SCFail("real Down never reached the EMPTY row"); return false;
		}
		return true;
	}

	bool SCPhaseActivateEmpty()
	{
		if (g_iSCPhaseFrames == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
		{
			ZM_GameState* pxLive = nullptr;
			ZM_GameStateManager* pxManager = SCResolveManager();
			g_bSCEmptyRefusalRaised = true;
			// Same exactness as the DAMAGED arm: a plain refusal line, never an
			// armed load choice on a slot with nothing to load.
			g_bSCEmptyExactRefusal = !pxMenu->IsDialogueAwaitingChoice();
			g_bSCEmptyNoRead = pxMenu->GetLoadReadCount() == 0u;
			g_bSCEmptyNoTransition = pxManager != nullptr
				&& pxManager->GetIssuedLoadRequestCount() == g_uSCContinueLoadsBefore
				&& !ZM_GameStateManager::IsWarpInProgress();
			g_bSCEmptyStateUnchanged = ZM_GameStateManager::TryGetGameState(pxLive)
				&& pxLive != nullptr && SCCanonicalEqual(*pxLive, g_xSCScramble)
				&& !Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_1));
			g_eSCPhase = SCPhase::DismissEmpty;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DIALOGUE_DEADLINE)
		{
			SCFail("EMPTY row did not raise its refusal dialogue"); return false;
		}
		return true;
	}

	bool SCPhaseDismissEmpty()
	{
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		if (pxMenu != nullptr && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_SAVE)
		{
			g_eSCPhase = SCPhase::WalkToAuto;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DIALOGUE_DEADLINE)
		{
			SCFail("EMPTY refusal did not return to LOAD"); return false;
		}
		if ((g_iSCPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		return true;
	}

	bool SCPhaseWalkToAuto()
	{
		if (SCWalkDownTo(ZM_UI_SaveSlots::RowElementName(3u)))
		{
			g_bSCAutoReached = true;
			g_eSCPhase = SCPhase::ActivateAuto;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_FOCUS_DEADLINE)
		{
			SCFail("real Down never reached READY Auto"); return false;
		}
		return true;
	}

	bool SCPhaseActivateAuto()
	{
		if (g_iSCPhaseFrames == 2)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		if (g_iSCPhaseFrames >= 3)
		{
			g_eSCPhase = SCPhase::AwaitAutoPrompt;
			g_iSCPhaseFrames = 0;
		}
		return true;
	}

	bool SCPhaseAwaitAutoPrompt()
	{
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		ZM_GameStateManager* pxManager = SCResolveManager();
		ZM_GameState* pxLive = nullptr;
		if (pxMenu != nullptr && pxManager != nullptr
			&& pxMenu->IsDialogueAwaitingChoice())
		{
			g_bSCAutoPrompt = true;
			g_bSCAutoPending = pxMenu->GetPendingLoadSlot() == ZM_SAVE_SLOT_AUTO
				&& pxMenu->GetPendingDialogueAction() == ZM_DIALOGUE_ACTION_LOAD_SAVE_SLOT;
			g_bSCAutoPreYesNoRead = pxMenu->GetLoadReadCount() == 0u;
			g_bSCAutoPreYesNoTransition =
				pxManager->GetIssuedLoadRequestCount() == g_uSCContinueLoadsBefore
				&& !ZM_GameStateManager::IsWarpInProgress();
			g_bSCAutoPreYesStateScrambled =
				ZM_GameStateManager::TryGetGameState(pxLive) && pxLive != nullptr
				&& SCCanonicalEqual(*pxLive, g_xSCScramble);
			g_eSCPhase = SCPhase::AnswerAutoYes;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DIALOGUE_DEADLINE)
		{
			SCFail("READY Auto never armed a real load Yes/No prompt"); return false;
		}
		if ((g_iSCPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		return true;
	}

	bool SCPhaseAnswerAutoYes()
	{
		if (SCFocusIs(ZM_UI_DialogueBox::szYES_NAME))
		{
			g_bSCAutoYesFocused = true;
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			g_eSCPhase = SCPhase::AwaitContinueAccepted;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_FOCUS_DEADLINE)
		{
			SCFail("real Left never focused Yes on Auto load confirmation"); return false;
		}
		if ((g_iSCPhaseFrames % 4) == 1)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_LEFT);
		}
		return true;
	}

	bool SCPhaseAwaitContinueAccepted()
	{
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		ZM_GameStateManager* pxManager = SCResolveManager();
		ZM_GameState* pxLive = nullptr;
		if (pxMenu != nullptr && pxManager != nullptr
			&& pxMenu->GetLoadReadCount() > 0u)
		{
			g_bSCExactlyOneRead = pxMenu->GetLoadReadCount() == 1u;
			// The slot-layer twin of the menu counter, over the WHOLE continue window
			// (the trace was reset as Continue was activated on the title): exactly
			// one READ_STATE, on Auto, and no WRITE_STATE. This is what makes "exactly
			// ONE READ_STATE on AUTO across the continue" a disk-layer fact rather
			// than a UI-counter reading. The window's probe sweeps are recorded too
			// but are deliberately not exact-pinned: how often a screen refreshes is
			// presentation, not transaction.
			g_bSCContinueTraceExact = !g_bSCSlotOperationOverflow
				&& SCCountSlotOperations(ZM_SAVE_SLOT_OPERATION_READ_STATE) == 1u
				&& SCCountSlotOperationsOn(
					ZM_SAVE_SLOT_OPERATION_READ_STATE, ZM_SAVE_SLOT_AUTO) == 1u
				&& SCCountSlotOperations(ZM_SAVE_SLOT_OPERATION_WRITE_STATE) == 0u;
			g_bSCLastReadAutoSuccess = pxMenu->GetLastLoadSlot() == ZM_SAVE_SLOT_AUTO
				&& pxMenu->GetLastLoadStatus().IsOk();
			g_bSCStatePublishedFromDisk = ZM_GameStateManager::TryGetGameState(pxLive)
				&& pxLive != nullptr && SCCanonicalEqual(*pxLive, g_xSCSaved)
				&& !SCCanonicalEqual(*pxLive, g_xSCScramble);
			g_bSCResumeAccepted = ZM_GameStateManager::IsWarpInProgress()
				&& pxManager->GetTargetBuildIndex() == iSC_DAWNMERE_BUILD;
			g_bSCUIClosedOnContinue = pxMenu->GetTopScreen() == ZM_MENU_SCREEN_NONE;
			if (!g_bSCExactlyOneRead || !g_bSCContinueTraceExact
				|| !g_bSCLastReadAutoSuccess
				|| !g_bSCStatePublishedFromDisk || !g_bSCResumeAccepted
				|| !g_bSCUIClosedOnContinue)
			{
				SCFail("Auto Yes did not atomically read, accept resume, publish, and close UI");
				return false;
			}
			g_eSCPhase = SCPhase::AwaitRestoredDawnmere;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_TRANSITION_DEADLINE)
		{
			SCFail("Auto Yes never produced its one definitive disk read"); return false;
		}
		return true;
	}

	bool SCPhaseAwaitRestoredDawnmere()
	{
		if (SCDawnmereReady())
		{
			ZM_GameStateManager* pxManager = SCResolveManager();
			g_bSCRestoredDawnmere = pxManager != nullptr
				&& pxManager->GetIssuedLoadRequestCount() == g_uSCContinueLoadsBefore + 1u;
			g_eSCPhase = SCPhase::FinalSettle;
			g_iSCPhaseFrames = 0; return true;
		}
		if (g_iSCPhaseFrames > iSC_DAWNMERE_DEADLINE)
		{
			SCFail("Continue never settled back in Dawnmere"); return false;
		}
		return true;
	}

	bool SCPhaseFinalSettle()
	{
		if (g_iSCPhaseFrames < iSC_FINAL_SETTLE) { return true; }
		ZM_GameState* pxLive = nullptr;
		Zenith_Maths::Vector3 xFinal(0.0f);
		float fFinalYaw = 0.0f;
		Zenith_Vector<u_int8> xDamagedNow;
		ZM_UI_MenuStack* pxMenu = SCResolveMenu();
		if (!ZM_GameStateManager::TryGetGameState(pxLive) || pxLive == nullptr
			|| !SCReadPlayerPose(xFinal, fFinalYaw) || pxMenu == nullptr)
		{
			SCFail("final restored state/body/menu observation was unavailable");
			return false;
		}
		g_fSCFinalPlanarError = SCPlanarDistance(xFinal, g_xSCSavedPose);
		g_fSCFinalVerticalError = std::fabs(xFinal.y - g_xSCSavedPose.y);
		g_fSCFinalYawError = std::fabs(SCWrappedAngleDifference(fFinalYaw, g_fSCSavedYaw));
		g_fSCFinalFromTownCenter = SCPlanarDistance(xFinal, g_xSCTownCenter);
		g_fSCFinalFromScramble = SCPlanarDistance(xFinal, g_xSCScramblePose);
		// Pose-normalised: see SCWithSavedPose. The pose is judged by
		// g_bSCFinalPoseSaved below, against its own tolerances.
		const ZM_GameState xLiveComparable = SCWithSavedPose(*pxLive, g_xSCSaved);
		g_bSCFinalCanonicalSaved = SCCanonicalEqual(xLiveComparable, g_xSCSaved);
		g_bSCFinalFieldsSaved = SCFieldsEqual(xLiveComparable, g_xSCSaved);
		g_szSCFinalDifferingField =
			SCFirstDifferingField(xLiveComparable, g_xSCSaved);
		// The scramble check keeps the REAL live state: it must differ from the
		// scrambled one in a way no pose normalisation could manufacture.
		g_bSCFinalNotScramble = !SCCanonicalEqual(*pxLive, g_xSCScramble);
		g_bSCFinalPoseSaved = g_fSCFinalPlanarError <= fSC_MAX_PLANAR_ERROR
			&& g_fSCFinalVerticalError <= fSC_MAX_VERTICAL_ERROR
			&& g_fSCFinalYawError <= fSC_MAX_YAW_ERROR;
		g_bSCFinalNotTownCenter = g_fSCFinalFromTownCenter >= fSC_MIN_FROM_TOWNCENTER;
		g_bSCFinalNotScramblePose = g_fSCFinalFromScramble >= fSC_MIN_FROM_SCRAMBLE;
		g_bSCFinalDamagedUnchanged = SCReadSlotBytes(ZM_SAVE_SLOT_0, xDamagedNow)
			&& SCBytesEqual(g_xSCDamagedBytesBefore, xDamagedNow)
			&& ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0) == ZM_SAVE_SLOT_DAMAGED
			&& pxMenu->GetLoadReadCount() == 1u;
		g_eSCPhase = SCPhase::Done;
		return false;
	}

	bool Step_ZMSaveContinue(int)
	{
		if (!g_bSCActive || g_bSCFailed || g_eSCPhase == SCPhase::Done) { return false; }
		++g_iSCPhaseFrames;

		switch (g_eSCPhase)
		{
		case SCPhase::AwaitEmptyTitle:         return SCPhaseAwaitEmptyTitle();
		case SCPhase::ActivateNewGame:         return SCPhaseActivateNewGame();
		case SCPhase::AwaitNewGameAccepted:    return SCPhaseAwaitNewGameAccepted();
		case SCPhase::AwaitPlayerHome:         return SCPhaseAwaitPlayerHome();
		case SCPhase::AwaitDawnmere:           return SCPhaseAwaitDawnmere();
		case SCPhase::WalkFromTownCenter:      return SCPhaseWalkFromTownCenter();
		case SCPhase::RestAfterWalk:           return SCPhaseRestAfterWalk();
		case SCPhase::BuildDiskFixtures:       return SCPhaseBuildDiskFixtures();
		case SCPhase::ProbeValidQueueRefusal:  return SCPhaseProbeValidQueueRefusal();
		case SCPhase::AwaitQuitAccepted:       return SCPhaseAwaitQuitAccepted();
		case SCPhase::AwaitLoadedTitle:        return SCPhaseAwaitLoadedTitle();
		case SCPhase::RestoreAutoFixture:      return SCPhaseRestoreAutoFixture();
		case SCPhase::InstallScramble:         return SCPhaseInstallScramble();
		case SCPhase::ObserveScramble:         return SCPhaseObserveScramble();
		case SCPhase::ActivateContinue:        return SCPhaseActivateContinue();
		case SCPhase::AwaitLoadScreen:         return SCPhaseAwaitLoadScreen();
		case SCPhase::InspectLoadRows:         return SCPhaseInspectLoadRows();
		case SCPhase::ActivateDamaged:         return SCPhaseActivateDamaged();
		case SCPhase::DismissDamaged:          return SCPhaseDismissDamaged();
		case SCPhase::WalkToEmpty:             return SCPhaseWalkToEmpty();
		case SCPhase::ActivateEmpty:           return SCPhaseActivateEmpty();
		case SCPhase::DismissEmpty:            return SCPhaseDismissEmpty();
		case SCPhase::WalkToAuto:              return SCPhaseWalkToAuto();
		case SCPhase::ActivateAuto:            return SCPhaseActivateAuto();
		case SCPhase::AwaitAutoPrompt:         return SCPhaseAwaitAutoPrompt();
		case SCPhase::AnswerAutoYes:           return SCPhaseAnswerAutoYes();
		case SCPhase::AwaitContinueAccepted:   return SCPhaseAwaitContinueAccepted();
		case SCPhase::AwaitRestoredDawnmere:   return SCPhaseAwaitRestoredDawnmere();
		case SCPhase::FinalSettle:             return SCPhaseFinalSettle();
		case SCPhase::Done:
			return false;
		}
		return false;
	}
}

namespace
{
	bool Verify_ZMSaveContinue()
	{
		if (g_bSCSkipped) { return true; }

		const bool bTitleAndNewGame = g_bSCEmptyTitle && g_bSCContinueHidden
			&& g_bSCNewGameFocused && g_bSCNewGameCanaryInstalled
			&& g_bSCNewGameAccepted && g_bSCNewGameNotDawnmere
			&& g_bSCNewGamePublishedStarter
			&& g_bSCNewGameNoSlotTouch && g_bSCTitleClosedOnNewGameWarp
			&& g_bSCPlayerHomeReady && g_bSCPlayerHomeSpawnTag
			&& g_bSCDawnmereReady && g_bSCWalked && g_bSCRested;
		const bool bFixture = g_bSCAliasesActive && g_bSCCaptured
			&& g_bSCSavedStateDistinctive && g_bSCAutoReady && g_bSCAutoDiskReadback
			&& g_bSCDamagedReady && g_bSCEmptySlots && g_bSCDamagedSnapshot
			&& g_bSCQuitAccepted && g_bSCQuitReachedTitle
			&& g_bSCDamagedOnlyTitleGate && g_bSCAutoRestoredWhileTitleOpen;
		const bool bScrambleAndOpen = g_bSCScrambleInstalled
			&& g_bSCScrambleObservedLater && g_bSCSavedDiffersScramble
			&& g_bSCContinueVisible && g_bSCContinueFocused
			&& g_bSCLoadOpenedByInput && g_bSCLoadRowsExact;
		const bool bNegativeRows = g_bSCDamagedRefusalRaised && g_bSCDamagedExactRefusal
			&& g_bSCDamagedNoRead
			&& g_bSCDamagedNoTransition && g_bSCDamagedStateUnchanged
			&& g_bSCDamagedBytesUnchanged && g_bSCEmptyReached
			&& g_bSCEmptyRefusalRaised && g_bSCEmptyExactRefusal && g_bSCEmptyNoRead
			&& g_bSCEmptyNoTransition && g_bSCEmptyStateUnchanged;
		const bool bAutoTransaction = g_bSCAutoReached && g_bSCAutoPrompt
			&& g_bSCAutoPending && g_bSCAutoPreYesNoRead
			&& g_bSCAutoPreYesNoTransition && g_bSCAutoPreYesStateScrambled
			&& g_bSCAutoYesFocused && g_bSCExactlyOneRead && g_bSCContinueTraceExact
			&& g_bSCLastReadAutoSuccess && g_bSCResumeAccepted
			&& g_bSCStatePublishedFromDisk && g_bSCUIClosedOnContinue;
		const bool bFinal = g_bSCRestoredDawnmere && g_bSCFinalCanonicalSaved
			&& g_bSCFinalFieldsSaved && g_bSCFinalNotScramble
			&& g_bSCFinalPoseSaved && g_bSCFinalNotTownCenter
			&& g_bSCFinalNotScramblePose && g_bSCFinalDamagedUnchanged;
		bool bPassed = !g_bSCFailed && bTitleAndNewGame && bFixture
			&& bScrambleAndOpen && bNegativeRows && bAutoTransaction && bFinal;

		// Teardown precedes every diagnostic/return. Delete while the verified aliases
		// are still active; only then clear instrumentation and restore shipping names.
		SCReleaseMovement();
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		if (g_bSCAliasesActive)
		{
			ZM_SaveSlots::DeleteAllSlotsForTests();
			Zenith_SaveData::ClearForTest();
		}
		ZM_SaveSlots::SetTestSlotNamesForTests(false);
		g_xEngine.Scenes().LoadSceneByIndex(iSC_FRONTEND_BUILD, SCENE_LOAD_SINGLE);
		g_bSCActive = false;

		if (g_bSCFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_SaveContinue] %s", g_szSCFailure);
		}
		if (!bTitleAndNewGame)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveContinue] TITLE/NewGame failed "
				"(empty/hidden/focus/canary/accepted/notDawnmere/starter/noSlot/closed/"
				"homeReady/homeTag/ready/walk/rest="
				"%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bSCEmptyTitle ? "true" : "false", g_bSCContinueHidden ? "true" : "false",
				g_bSCNewGameFocused ? "true" : "false",
				g_bSCNewGameCanaryInstalled ? "true" : "false",
				g_bSCNewGameAccepted ? "true" : "false",
				g_bSCNewGameNotDawnmere ? "true" : "false",
				g_bSCNewGamePublishedStarter ? "true" : "false",
				g_bSCNewGameNoSlotTouch ? "true" : "false",
				g_bSCTitleClosedOnNewGameWarp ? "true" : "false",
				g_bSCPlayerHomeReady ? "true" : "false",
				g_bSCPlayerHomeSpawnTag ? "true" : "false",
				g_bSCDawnmereReady ? "true" : "false", g_bSCWalked ? "true" : "false",
				g_bSCRested ? "true" : "false");
		}
		if (!bFixture || !bScrambleAndOpen)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveContinue] disk/scramble/open failed "
				"(aliases/capture/distinct/auto/readback/damaged/empty/snapshot/quit/title/"
				"gate/restored/installed/observed/different/visible/focused/open/rows="
				"%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bSCAliasesActive ? "true" : "false", g_bSCCaptured ? "true" : "false",
				g_bSCSavedStateDistinctive ? "true" : "false", g_bSCAutoReady ? "true" : "false",
				g_bSCAutoDiskReadback ? "true" : "false", g_bSCDamagedReady ? "true" : "false",
				g_bSCEmptySlots ? "true" : "false", g_bSCDamagedSnapshot ? "true" : "false",
				g_bSCQuitAccepted ? "true" : "false", g_bSCQuitReachedTitle ? "true" : "false",
				g_bSCDamagedOnlyTitleGate ? "true" : "false",
				g_bSCAutoRestoredWhileTitleOpen ? "true" : "false",
				g_bSCScrambleInstalled ? "true" : "false",
				g_bSCScrambleObservedLater ? "true" : "false",
				g_bSCSavedDiffersScramble ? "true" : "false",
				g_bSCContinueVisible ? "true" : "false", g_bSCContinueFocused ? "true" : "false",
				g_bSCLoadOpenedByInput ? "true" : "false", g_bSCLoadRowsExact ? "true" : "false");
		}
		if (!bNegativeRows || !bAutoTransaction)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveContinue] row/transaction failed "
				"(dPrompt/dExact/dRead/dWarp/dState/dBytes/eReached/ePrompt/eExact/eRead/"
				"eWarp/eState/auto/prompt/pending/preRead/preWarp/preState/yes/read/trace/"
				"status/resume/publish/close="
				"%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s)",
				g_bSCDamagedRefusalRaised ? "true" : "false",
				g_bSCDamagedExactRefusal ? "true" : "false",
				g_bSCDamagedNoRead ? "true" : "false",
				g_bSCDamagedNoTransition ? "true" : "false",
				g_bSCDamagedStateUnchanged ? "true" : "false",
				g_bSCDamagedBytesUnchanged ? "true" : "false",
				g_bSCEmptyReached ? "true" : "false", g_bSCEmptyRefusalRaised ? "true" : "false",
				g_bSCEmptyExactRefusal ? "true" : "false",
				g_bSCEmptyNoRead ? "true" : "false", g_bSCEmptyNoTransition ? "true" : "false",
				g_bSCEmptyStateUnchanged ? "true" : "false", g_bSCAutoReached ? "true" : "false",
				g_bSCAutoPrompt ? "true" : "false", g_bSCAutoPending ? "true" : "false",
				g_bSCAutoPreYesNoRead ? "true" : "false",
				g_bSCAutoPreYesNoTransition ? "true" : "false",
				g_bSCAutoPreYesStateScrambled ? "true" : "false",
				g_bSCAutoYesFocused ? "true" : "false", g_bSCExactlyOneRead ? "true" : "false",
				g_bSCContinueTraceExact ? "true" : "false",
				g_bSCLastReadAutoSuccess ? "true" : "false",
				g_bSCResumeAccepted ? "true" : "false",
				g_bSCStatePublishedFromDisk ? "true" : "false",
				g_bSCUIClosedOnContinue ? "true" : "false");
		}
		if (!bFinal)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SaveContinue] final restore failed "
				"(settled/canonical/fields/notRAM/pose/notTown/notScramble/damaged="
				"%s/%s/%s/%s/%s/%s/%s/%s; planar=%.4f vertical=%.4f yaw=%.4f "
				"fromTown=%.4f fromScramble=%.4f firstDifferingField=%s)",
				g_bSCRestoredDawnmere ? "true" : "false",
				g_bSCFinalCanonicalSaved ? "true" : "false",
				g_bSCFinalFieldsSaved ? "true" : "false",
				g_bSCFinalNotScramble ? "true" : "false", g_bSCFinalPoseSaved ? "true" : "false",
				g_bSCFinalNotTownCenter ? "true" : "false",
				g_bSCFinalNotScramblePose ? "true" : "false",
				g_bSCFinalDamagedUnchanged ? "true" : "false",
				g_fSCFinalPlanarError, g_fSCFinalVerticalError, g_fSCFinalYawError,
				g_fSCFinalFromTownCenter, g_fSCFinalFromScramble,
				g_szSCFinalDifferingField);
		}
		return bPassed;
	}
}

#endif // ZENITH_INPUT_SIMULATOR
