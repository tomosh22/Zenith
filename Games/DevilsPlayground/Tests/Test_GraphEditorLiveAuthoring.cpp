#include "Zenith.h"

//------------------------------------------------------------------------------
// Test_GraphEditorLiveAuthoring - the Behaviour Graphs flagship test.
//
// The input SIMULATOR drives the Graph Editor panel in real time, windowed:
//   1. clicks OnUpdate, RotateEntity, ReadKeyState and Branch in the palette,
//   2. drags from the OnUpdate output pin to the RotateEntity input pin
//      and ReadKeyState.Result to Branch.Condition (both edges connect),
//   3. selects the RotateEntity node and clicks the right edge of its
//      m_fDegreesPerSecond slider (value -> max, +1080 deg/s),
//   4. clicks Save (writes game:Graphs/LiveAuthoring_Test.bgraph),
//   5. binds the graph to a test entity and enters Play mode,
//   6. measures the entity's yaw rate over 60 frames (expect ~ +1080 deg/s),
//   7. WHILE STILL PLAYING: reselects the node, clicks the LEFT edge of the
//      slider (value -> min, -1080 deg/s), clicks Save - the hot-reload path
//      re-instantiates the live graph at the next safe point,
//   8. measures again (expect ~ -1080 deg/s - reversed, live, no restart).
//
// Hard constraint: inside Step() only simulator STATE-SETTERS are legal
// (SimulateMousePosition / SimulateMouseButtonDown/Up) - the reentrant helpers
// (SimulateMouseClick / StepFrame) nest Zenith_MainLoop and deadlock windowed.
// Mouse interactions are therefore spread across frames; the
// Zenith_ImGuiInputBridge feeds the simulated state into ImGui each frame.
//------------------------------------------------------------------------------

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.h"
#include "EntityComponent/Zenith_GraphReload.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"

#include "imgui.h"

#include <cmath>
#include <cstring>
#include <filesystem>

namespace
{
	constexpr const char* szGRAPH_ASSET_PATH = "game:Graphs/LiveAuthoring_Test.bgraph";
	constexpr float fFIXED_DT = 0.01666f;

	struct LiveAuthoringState
	{
		Zenith_EntityID m_xEntityID;
		u_int m_uSourceNodeID = 0;
		u_int m_uRotateNodeID = 0;
		u_int m_uReadKeyNodeID = 0;
		u_int m_uBranchNodeID = 0;

		bool m_bGraphAuthored = false;		// 4 nodes, 1 exec edge, 1 data edge after authoring
		bool m_bAssetSaved = false;			// file exists after the first Save click
		bool m_bFirstDiskSnapshot = false;
		bool m_bSecondDiskSnapshot = false;
		bool m_bReopenRenderedWire = false;
		bool m_bGraphBound = false;
		u_int m_uReloadCountAtBind = 0;
		bool m_bReloadObserved = false;

		Zenith_Maths::Vector3 m_xPreviousForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		float m_fAccumulatedDegrees = 0.0f;
		u_int m_uMeasuredFrames = 0;
		float m_fMeasuredRate1 = 0.0f;
		float m_fMeasuredRate2 = 0.0f;

		bool m_bFailedHard = false;
		char m_acFailReason[256] = {};
	};

	LiveAuthoringState g_xLiveAuthoring;

	void FailHard(const char* szReason)
	{
		if (!g_xLiveAuthoring.m_bFailedHard)
		{
			g_xLiveAuthoring.m_bFailedHard = true;
			snprintf(g_xLiveAuthoring.m_acFailReason, sizeof(g_xLiveAuthoring.m_acFailReason), "%s", szReason);
			Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] FAIL: %s", szReason);
		}
	}

	Zenith_Entity GetTestEntity()
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(g_xLiveAuthoring.m_xEntityID);
		return pxSceneData ? pxSceneData->GetEntity(g_xLiveAuthoring.m_xEntityID) : Zenith_Entity();
	}

	Zenith_Maths::Vector3 GetEntityForward()
	{
		Zenith_Entity xEntity = GetTestEntity();
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_TransformComponent>())
		{
			return Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		}
		Zenith_Maths::Quat xRotation;
		xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRotation);
		return xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}

	// Signed yaw delta (degrees) between successive forward vectors. Positive
	// for a positive-rate RotateEntity (angleAxis(+rad, +Y) maps +Z toward +X).
	float SignedYawDeltaDegrees(const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo)
	{
		const float fDot = xFrom.x * xTo.x + xFrom.z * xTo.z;
		const float fCross = xFrom.z * xTo.x - xFrom.x * xTo.z;
		return glm::degrees(std::atan2(fCross, fDot));
	}

	void BeginRateMeasurement()
	{
		g_xLiveAuthoring.m_xPreviousForward = GetEntityForward();
		g_xLiveAuthoring.m_fAccumulatedDegrees = 0.0f;
		g_xLiveAuthoring.m_uMeasuredFrames = 0;
	}

	void TickRateMeasurement()
	{
		const Zenith_Maths::Vector3 xForward = GetEntityForward();
		g_xLiveAuthoring.m_fAccumulatedDegrees += SignedYawDeltaDegrees(g_xLiveAuthoring.m_xPreviousForward, xForward);
		g_xLiveAuthoring.m_xPreviousForward = xForward;
		g_xLiveAuthoring.m_uMeasuredFrames++;
	}

	float FinishRateMeasurement()
	{
		const float fSeconds = static_cast<float>(g_xLiveAuthoring.m_uMeasuredFrames) * fFIXED_DT;
		return (fSeconds > 0.0f) ? (g_xLiveAuthoring.m_fAccumulatedDegrees / fSeconds) : 0.0f;
	}

	// Mouse helpers (state-setters only).
	void MouseTo(const Zenith_Maths::Vector2& xPos)
	{
		Zenith_InputSimulator::SimulateMousePosition(static_cast<double>(xPos.x), static_cast<double>(xPos.y));
	}

	// The palette lists every registered node type, so its column is thousands of
	// pixels tall and the wanted row is almost always scrolled out of view. A
	// clipped ImGui row is not interactable, so it must be scrolled in FIRST and
	// the scroll must be allowed to land (it applies on the next Render) before
	// the position is read. Two separate Step frames, hence two helpers.
	bool RequestPaletteScroll(const char* szTypeName)
	{
		if (!Zenith_GraphEditorPanel::ScrollPaletteEntryIntoView(szTypeName))
		{
			FailHard("palette entry not registered");
			return false;
		}
		return true;
	}

	bool MouseToPaletteEntry(const char* szTypeName)
	{
		Zenith_Maths::Vector2 xPos;
		if (!Zenith_GraphEditorPanel::GetPaletteEntryScreenPos(szTypeName, xPos))
		{
			// Only reachable if the scroll never landed - the accessor reports
			// visible rows only, so it can no longer return an off-screen point.
			FailHard("palette entry not visible after scroll");
			return false;
		}
		MouseTo(xPos);
		return true;
	}

	bool MouseToSliderFraction(const char* szPropertyName, float fFraction)
	{
		Zenith_Maths::Vector2 xMin;
		Zenith_Maths::Vector2 xMax;
		if (!Zenith_GraphEditorPanel::GetPropertyRowScreenRect(szPropertyName, xMin, xMax))
		{
			FailHard("property row not found");
			return false;
		}
		// The slider FRAME is the panel's pinned 160px item width; the row rect
		// extends further right over the label, which a click must not hit.
		constexpr float fSLIDER_FRAME_WIDTH = 160.0f;
		const float fX = xMin.x + 4.0f + (fSLIDER_FRAME_WIDTH - 8.0f) * fFraction;
		MouseTo(Zenith_Maths::Vector2(fX, (xMin.y + xMax.y) * 0.5f));
		return true;
	}

	bool MoveToDataPin(u_int uNodeID, const char* szPinName, bool bInput, const char* szFailure)
	{
		Zenith_Maths::Vector2 xPin;
		if (!Zenith_GraphEditorPanel::GetDataPinScreenPos(uNodeID, szPinName, bInput, xPin))
		{
			FailHard(szFailure);
			return false;
		}
		MouseTo(xPin);
		return true;
	}

	bool VerifyDiskSnapshot(const char* szStage, float fExpectedRate)
	{
		Zenith_Result<Zenith_Asset*> xLoaded = LoadSerializableAsset(Zenith_AssetRegistry::ResolvePath(szGRAPH_ASSET_PATH));
		if (!xLoaded.IsOk() || xLoaded.Value() == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] %s disk load failed", szStage);
			return false;
		}

		Zenith_Asset* pxAsset = xLoaded.Value();
		const bool bTypeOk = std::strcmp(pxAsset->GetTypeName(), Zenith_BehaviourGraphAsset::StaticTypeName()) == 0;
		Zenith_BehaviourGraphAsset* pxGraphAsset = bTypeOk ? static_cast<Zenith_BehaviourGraphAsset*>(pxAsset) : nullptr;
		bool bOk = pxGraphAsset != nullptr && pxGraphAsset->LoadedOk();
		float fObservedRate = 0.0f;
		if (bOk)
		{
			const Zenith_GraphDefinition& xDef = pxGraphAsset->GetDefinition();
			bOk = xDef.GetNodeCount() == 4u && xDef.GetEdgeCount() == 1u && xDef.GetDataEdgeCount() == 1u;
			const Zenith_GraphNodeDef* pxSourceDef = bOk ? xDef.FindNodeDef(g_xLiveAuthoring.m_uSourceNodeID) : nullptr;
			const Zenith_GraphNodeDef* pxRotateDef = bOk ? xDef.FindNodeDef(g_xLiveAuthoring.m_uRotateNodeID) : nullptr;
			const Zenith_GraphNodeDef* pxReadKeyDef = bOk ? xDef.FindNodeDef(g_xLiveAuthoring.m_uReadKeyNodeID) : nullptr;
			const Zenith_GraphNodeDef* pxBranchDef = bOk ? xDef.FindNodeDef(g_xLiveAuthoring.m_uBranchNodeID) : nullptr;
			bOk = bOk && pxSourceDef != nullptr && pxRotateDef != nullptr && pxReadKeyDef != nullptr && pxBranchDef != nullptr
				&& pxSourceDef->m_strTypeName == "OnUpdate" && pxRotateDef->m_strTypeName == "RotateEntity"
				&& pxReadKeyDef->m_strTypeName == "ReadKeyState" && pxBranchDef->m_strTypeName == "Branch";
			const Zenith_GraphEdge& xExec = bOk ? xDef.GetEdgeAt(0u) : Zenith_GraphEdge();
			bOk = bOk && xExec.m_uSrcNodeID == g_xLiveAuthoring.m_uSourceNodeID && xExec.m_uSrcPin == 0u
				&& xExec.m_uDstNodeID == g_xLiveAuthoring.m_uRotateNodeID;
			const Zenith_GraphDataEdge& xData = bOk ? xDef.GetDataEdgeAt(0u) : Zenith_GraphDataEdge();
			bOk = bOk && xData.m_uSrcNodeID == g_xLiveAuthoring.m_uReadKeyNodeID
				&& xData.m_strSrcPin == "Result" && xData.m_uDstNodeID == g_xLiveAuthoring.m_uBranchNodeID
				&& xData.m_strDstPin == "Condition";

			const Zenith_GraphNodeTypeInfo* pxRotateInfo = Zenith_GraphNodeRegistry::Get().Find("RotateEntity");
			Zenith_GraphNode* pxRotate = pxRotateInfo ? pxRotateInfo->m_pfnCreate() : nullptr;
			bOk = bOk && pxRotate != nullptr && xDef.ApplyNodeParams(g_xLiveAuthoring.m_uRotateNodeID, pxRotate, *pxRotateInfo);
			if (bOk)
			{
				const Zenith_ReflectedProperty* pxRate = pxRotateInfo->m_pfnGetPropertyTable()->FindProperty("m_fDegreesPerSecond");
				Zenith_PropertyValue xRate;
				if (pxRate == nullptr || pxRate->m_eType != PROPERTY_TYPE_FLOAT)
				{
					bOk = false;
				}
				else
				{
					pxRate->m_pfnGet(pxRotate, xRate);
					fObservedRate = xRate.GetFloat();
					bOk = std::fabs(fObservedRate - fExpectedRate) < 0.1f;
				}
			}
			delete pxRotate;
		}
		Zenith_Log(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] %s disk snapshot type=%d topology=%d rate=%.1f expected=%.1f",
			szStage, bTypeOk ? 1 : 0, bOk ? 1 : 0, fObservedRate, fExpectedRate);
		delete pxAsset;
		return bOk;
	}

	//--------------------------------------------------------------------------
	// Setup
	//--------------------------------------------------------------------------
	void Setup_GraphEditorLiveAuthoring()
	{
		g_xLiveAuthoring = LiveAuthoringState();

		Zenith_InputSimulator::Enable();
		// May trigger a deferred play-backup scene restore - so the test entity
		// is created in an early Step frame, AFTER the restore has processed
		// (creating it here would see it destroyed by the restore).
		g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);

		// Fresh asset every run.
		std::error_code xEC;
		std::filesystem::remove(Zenith_AssetRegistry::ResolvePath(szGRAPH_ASSET_PATH), xEC);

		Zenith_GraphEditorPanel::OpenAssetFresh(szGRAPH_ASSET_PATH);
	}

	//--------------------------------------------------------------------------
	// Step - a frame-indexed interaction script.
	//--------------------------------------------------------------------------
	void LogImGuiMouseState(int iFrame)
	{
		const ImGuiIO& xIO = ImGui::GetIO();
		Zenith_Log(LOG_CATEGORY_CORE,
			"[GraphEditorLiveAuthoring] f%d io.MousePos=(%.0f, %.0f) down0=%d wantCapture=%d",
			iFrame, xIO.MousePos.x, xIO.MousePos.y, xIO.MouseDown[0] ? 1 : 0, xIO.WantCaptureMouse ? 1 : 0);
	}

	// Why a click failed to land. "The nodes were not created" on its own cannot
	// distinguish a click that never reached ImGui from one that reached it at
	// the wrong place from a panel that would have refused the node anyway, and
	// this test is the ONLY coverage of the simulated-input -> ImGui bridge in the
	// whole repo -- so when it breaks, it has to say which of the three it is.
	void LogClickDiagnostics(const char* szWhen)
	{
		const ImGuiIO& xIO = ImGui::GetIO();
		Zenith_Maths::Vector2 xPalette(0.0f, 0.0f);
		const bool bHavePalette = Zenith_GraphEditorPanel::GetPaletteEntryScreenPos("OnUpdate", xPalette);
		Zenith_Log(LOG_CATEGORY_CORE,
			"[GraphEditorLiveAuthoring] DIAG %s: io.MousePos=(%.1f, %.1f) down0=%d wantCapture=%d "
			"displaySize=(%.0f, %.0f) paletteOnUpdate=%s(%.1f, %.1f) nodeCount=%u",
			szWhen, xIO.MousePos.x, xIO.MousePos.y, xIO.MouseDown[0] ? 1 : 0,
			xIO.WantCaptureMouse ? 1 : 0, xIO.DisplaySize.x, xIO.DisplaySize.y,
			bHavePalette ? "yes" : "NO", xPalette.x, xPalette.y,
			Zenith_GraphEditorPanel::GetNodeCount());
	}

	bool Step_GraphEditorLiveAuthoring(int iFrame)
	{
		if (g_xLiveAuthoring.m_bFailedHard)
		{
			return false;
		}

		// IO diagnostics at the two slider-click moments (authoring vs in-play).
		if (iFrame == 136 || iFrame == 137 || iFrame == 265 || iFrame == 266)
		{
			LogImGuiMouseState(iFrame);
		}

		switch (iFrame)
		{
		// --- create the entity the graph will drive (after the Stopped-mode
		// --- restore from Setup has fully processed) ------------------------
		case 12:
		{
			Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
			Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
			Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphAuthorTarget");
			xEntity.AddComponent<Zenith_GraphComponent>();
			g_xLiveAuthoring.m_xEntityID = xEntity.GetEntityID();
			break;
		}

		// --- author: place four nodes from the palette ----------------------
		case 27: RequestPaletteScroll("OnUpdate"); break;
		case 30: MouseToPaletteEntry("OnUpdate"); LogClickDiagnostics("f30 moved-to-palette"); break;
		case 33: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 36: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;
		case 38: LogClickDiagnostics("f38 after-first-click"); break;

		case 39: RequestPaletteScroll("RotateEntity"); break;
		case 42: MouseToPaletteEntry("RotateEntity"); break;
		case 45: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 48: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 51: RequestPaletteScroll("ReadKeyState"); break;
		case 54: MouseToPaletteEntry("ReadKeyState"); break;
		case 57: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 60: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 63: RequestPaletteScroll("Branch"); break;
		case 66: MouseToPaletteEntry("Branch"); break;
		case 69: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 72: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		// --- connect: drag output pin -> input pin -------------------------
		case 78:
		{
			g_xLiveAuthoring.m_uSourceNodeID = Zenith_GraphEditorPanel::FindNodeIDByType("OnUpdate");
			g_xLiveAuthoring.m_uRotateNodeID = Zenith_GraphEditorPanel::FindNodeIDByType("RotateEntity");
			g_xLiveAuthoring.m_uReadKeyNodeID = Zenith_GraphEditorPanel::FindNodeIDByType("ReadKeyState");
			g_xLiveAuthoring.m_uBranchNodeID = Zenith_GraphEditorPanel::FindNodeIDByType("Branch");
			if (g_xLiveAuthoring.m_uSourceNodeID == 0 || g_xLiveAuthoring.m_uRotateNodeID == 0
				|| g_xLiveAuthoring.m_uReadKeyNodeID == 0 || g_xLiveAuthoring.m_uBranchNodeID == 0)
			{
				LogClickDiagnostics("f54 node-check");
				// Splits the failure in half: Action_AddNode is documented to run
				// EXACTLY the palette-click handler's body, minus ImGui. If it
				// succeeds here, the panel and the open definition are healthy and
				// the fault is purely that the simulated click never reached the
				// Selectable; if it fails too, the clicks were innocent.
				const bool bDirectAddWorks = Zenith_GraphEditorPanel::Action_AddNode("OnUpdate");
				Zenith_Log(LOG_CATEGORY_CORE,
					"[GraphEditorLiveAuthoring] DIAG Action_AddNode(\"OnUpdate\") without ImGui -> %s "
					"(nodeCount now %u). %s",
					bDirectAddWorks ? "OK" : "FAILED", Zenith_GraphEditorPanel::GetNodeCount(),
					bDirectAddWorks
						? "=> panel+definition healthy; the simulated click did not reach the palette Selectable."
						: "=> the panel/definition itself is refusing nodes; clicks are not the cause.");
				FailHard("palette clicks did not create the nodes");
				break;
			}
			Zenith_Maths::Vector2 xPin;
			if (Zenith_GraphEditorPanel::GetPinScreenPos(g_xLiveAuthoring.m_uSourceNodeID, 0, false, xPin))
			{
				MouseTo(xPin);
			}
			else
			{
				FailHard("source output pin not found");
			}
			break;
		}
		case 81: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 84:
		case 87:
		case 90:
		{
			// Glide toward the input pin across a few frames.
			Zenith_Maths::Vector2 xTarget;
			Zenith_Maths::Vector2 xSource;
			if (Zenith_GraphEditorPanel::GetPinScreenPos(g_xLiveAuthoring.m_uRotateNodeID, 0, true, xTarget)
				&& Zenith_GraphEditorPanel::GetPinScreenPos(g_xLiveAuthoring.m_uSourceNodeID, 0, false, xSource))
			{
				const float fT = (iFrame == 84) ? 0.4f : (iFrame == 87) ? 0.8f : 1.0f;
				MouseTo(Zenith_Maths::Vector2(xSource.x + (xTarget.x - xSource.x) * fT, xSource.y + (xTarget.y - xSource.y) * fT));
			}
			else
			{
				FailHard("exec-wire pin disappeared during drag");
			}
			break;
		}
		case 93: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		// --- connect: real DATA drag ReadKeyState.Result -> Branch.Condition --
		case 99:
			if (Zenith_GraphEditorPanel::GetDataEdgeCount() != 0u)
			{
				FailHard("data edge existed before the author drag");
				break;
			}
			MoveToDataPin(g_xLiveAuthoring.m_uReadKeyNodeID, "Result", false, "ReadKeyState Result output pin not found");
			break;
		case 102: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 105:
		case 108:
		case 111:
		{
			Zenith_Maths::Vector2 xSource;
			Zenith_Maths::Vector2 xTarget;
			if (!Zenith_GraphEditorPanel::GetDataPinScreenPos(g_xLiveAuthoring.m_uReadKeyNodeID, "Result", false, xSource)
				|| !Zenith_GraphEditorPanel::GetDataPinScreenPos(g_xLiveAuthoring.m_uBranchNodeID, "Condition", true, xTarget))
			{
				FailHard("data-wire pin disappeared during drag");
				break;
			}
			const float fT = (iFrame == 105) ? 0.4f : (iFrame == 108) ? 0.8f : 1.0f;
			MouseTo(Zenith_Maths::Vector2(xSource.x + (xTarget.x - xSource.x) * fT, xSource.y + (xTarget.y - xSource.y) * fT));
			break;
		}
		case 114: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		// --- select the rotate node, set its rate slider to MAX ------------
		case 120:
		{
			Zenith_Maths::Vector2 xNode;
			if (Zenith_GraphEditorPanel::GetNodeScreenPos(g_xLiveAuthoring.m_uRotateNodeID, xNode))
			{
				MouseTo(xNode);
			}
			else
			{
				FailHard("RotateEntity node not visible for first slider edit");
			}
			break;
		}
		case 123: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 126: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 132: MouseToSliderFraction("m_fDegreesPerSecond", 1.0f); break;
		case 135: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 138: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 144:
		{
			g_xLiveAuthoring.m_bGraphAuthored =
				(Zenith_GraphEditorPanel::GetNodeCount() == 4 && Zenith_GraphEditorPanel::GetEdgeCount() == 1
					&& Zenith_GraphEditorPanel::GetDataEdgeCount() == 1 && Zenith_GraphEditorPanel::GetValidationErrorCount() == 0
					&& Zenith_GraphEditorPanel::GetConnectRefusalText()[0] == '\0');
			if (!g_xLiveAuthoring.m_bGraphAuthored)
			{
				FailHard("authored graph is not 4 nodes + 1 exec edge + 1 data edge");
			}
			float fRate = 0.0f;
			if (!Zenith_GraphEditorPanel::GetSelectedNodeParamFloat("m_fDegreesPerSecond", fRate) || fRate < 1000.0f)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] slider value after MAX click: %.1f", fRate);
				FailHard("authoring slider click did not set the rate to max");
			}
			break;
		}

		case 146:
			// Screenshot marker: authored graph on canvas, node selected,
			// properties panel showing the maxed slider.
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] marker1_authored");
			break;

		// --- save -----------------------------------------------------------
		case 148:
		{
			Zenith_Maths::Vector2 xSave;
			if (Zenith_GraphEditorPanel::GetToolbarButtonScreenPos("Save", xSave))
			{
				MouseTo(xSave);
			}
			else
			{
				FailHard("first Save button not visible");
			}
			break;
		}
		case 151: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 154: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 160:
		{
			std::error_code xEC;
			g_xLiveAuthoring.m_bAssetSaved = std::filesystem::exists(Zenith_AssetRegistry::ResolvePath(szGRAPH_ASSET_PATH), xEC);
			if (!g_xLiveAuthoring.m_bAssetSaved)
			{
				FailHard("Save click did not write the .bgraph file");
			}
			g_xLiveAuthoring.m_bFirstDiskSnapshot = VerifyDiskSnapshot("first Save", 1080.0f);
			if (!g_xLiveAuthoring.m_bFirstDiskSnapshot)
			{
				FailHard("first Save disk snapshot did not preserve data wire and rate");
			}
			break;
		}

		// --- bind + play -----------------------------------------------------
		case 164:
		{
			Zenith_Entity xEntity = GetTestEntity();
			if (xEntity.IsValid() && xEntity.HasComponent<Zenith_GraphComponent>())
			{
				g_xLiveAuthoring.m_bGraphBound =
					xEntity.GetComponent<Zenith_GraphComponent>().AddGraphByAssetPath(szGRAPH_ASSET_PATH) != nullptr;
			}
			if (!g_xLiveAuthoring.m_bGraphBound)
			{
				FailHard("could not bind the authored graph to the test entity");
			}
			g_xLiveAuthoring.m_uReloadCountAtBind = Zenith_GraphReload::GetReloadCount();
			break;
		}
		case 166:
			// Select the entity in the editor so the panel's live execution
			// highlighting has a target while playing.
			g_xEngine.Editor().SelectEntityByName("GraphAuthorTarget");
			break;
		case 168: g_xEngine.Editor().SetEditorMode(EditorMode::Playing); break;

		// --- measure rate 1 (~ +1080 deg/s) ---------------------------------
		case 186: BeginRateMeasurement(); break;
		case 216:
			// Screenshot marker: playing, live execution highlight on the
			// OnUpdate -> RotateEntity chain.
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] marker2_playing");
			break;
		case 246:
			g_xLiveAuthoring.m_fMeasuredRate1 = FinishRateMeasurement();
			break;

		// --- live edit DURING play: slider to MIN, save, hot reload ---------
		case 251:
		{
			Zenith_Maths::Vector2 xNode;
			if (Zenith_GraphEditorPanel::GetNodeScreenPos(g_xLiveAuthoring.m_uRotateNodeID, xNode))
			{
				MouseTo(xNode);
			}
			else
			{
				FailHard("RotateEntity node not visible for second slider edit");
			}
			break;
		}
		case 254: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 257: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 262: MouseToSliderFraction("m_fDegreesPerSecond", 0.0f); break;
		case 265: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 268: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 271:
		{
			float fRate = 0.0f;
			if (!Zenith_GraphEditorPanel::GetSelectedNodeParamFloat("m_fDegreesPerSecond", fRate) || fRate > -1000.0f)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] slider value after MIN click (in play): %.1f", fRate);
				FailHard("in-play slider click did not set the rate to min");
			}
			break;
		}
		case 272:
		{
			Zenith_Maths::Vector2 xSave;
			if (Zenith_GraphEditorPanel::GetToolbarButtonScreenPos("Save", xSave))
			{
				MouseTo(xSave);
			}
			else
			{
				FailHard("second Save button not visible");
			}
			break;
		}
		case 275: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 278: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 286:
			g_xLiveAuthoring.m_bReloadObserved = Zenith_GraphReload::GetReloadCount() > g_xLiveAuthoring.m_uReloadCountAtBind;
			if (!g_xLiveAuthoring.m_bReloadObserved)
			{
				FailHard("hot reload did not fire after the in-play Save");
			}
			g_xLiveAuthoring.m_bSecondDiskSnapshot = VerifyDiskSnapshot("second Save", -1080.0f);
			if (!g_xLiveAuthoring.m_bSecondDiskSnapshot)
			{
				FailHard("second Save disk snapshot did not preserve data wire and rate");
			}
			break;
		case 289: Zenith_GraphEditorPanel::Close(); break;
		case 292: Zenith_GraphEditorPanel::OpenAsset(szGRAPH_ASSET_PATH); break;
		case 298:
		{
			Zenith_Maths::Vector2 xResult(0.0f, 0.0f);
			Zenith_Maths::Vector2 xCondition(0.0f, 0.0f);
			g_xLiveAuthoring.m_bReopenRenderedWire = Zenith_GraphEditorPanel::GetDataEdgeCount() == 1u
				&& Zenith_GraphEditorPanel::GetUnresolvableEdgeDrawCountForTest() == 0u
				&& Zenith_GraphEditorPanel::GetDataPinScreenPos(g_xLiveAuthoring.m_uReadKeyNodeID, "Result", false, xResult)
				&& Zenith_GraphEditorPanel::GetDataPinScreenPos(g_xLiveAuthoring.m_uBranchNodeID, "Condition", true, xCondition);
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] reopened data wire=%d result=(%.0f,%.0f) condition=(%.0f,%.0f)",
				g_xLiveAuthoring.m_bReopenRenderedWire ? 1 : 0, xResult.x, xResult.y, xCondition.x, xCondition.y);
			if (!g_xLiveAuthoring.m_bReopenRenderedWire)
			{
				FailHard("normal reopen did not render the saved data wire");
			}
			break;
		}

		// --- measure rate 2 (~ -1080 deg/s, live-reversed) -------------------
		case 303: BeginRateMeasurement(); break;
		case 333:
			// Screenshot marker: still playing, after the live hot reload
			// (slider at min, reload status line in the toolbar).
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] marker3_live_reloaded");
			break;
		case 363:
			g_xLiveAuthoring.m_fMeasuredRate2 = FinishRateMeasurement();
			break;

		// --- teardown ---------------------------------------------------------
		case 368: g_xEngine.Editor().SetEditorMode(EditorMode::Stopped); break;
		case 388:
		{
			Zenith_GraphEditorPanel::Close();
			Zenith_Entity xEntity = GetTestEntity();
			if (xEntity.IsValid())
			{
				xEntity.DestroyImmediate();
			}
			std::error_code xEC;
			std::filesystem::remove(Zenith_AssetRegistry::ResolvePath(szGRAPH_ASSET_PATH), xEC);
			return false;	// done
		}
		default:
			break;
		}

		// Continuous measurement ticks between the boundary frames.
		if ((iFrame > 186 && iFrame <= 246) || (iFrame > 303 && iFrame <= 363))
		{
			TickRateMeasurement();
		}

		return true;
	}

	//--------------------------------------------------------------------------
	// Verify
	//--------------------------------------------------------------------------
	bool Verify_GraphEditorLiveAuthoring()
	{
		if (g_xLiveAuthoring.m_bFailedHard)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] failed: %s", g_xLiveAuthoring.m_acFailReason);
			return false;
		}

		// 20% tolerance: editor/game frame jitter + the reload landing a frame
		// or two into the measurement window.
		const bool bRate1Ok = g_xLiveAuthoring.m_fMeasuredRate1 > 1080.0f * 0.8f && g_xLiveAuthoring.m_fMeasuredRate1 < 1080.0f * 1.2f;
		const bool bRate2Ok = g_xLiveAuthoring.m_fMeasuredRate2 < -1080.0f * 0.8f && g_xLiveAuthoring.m_fMeasuredRate2 > -1080.0f * 1.2f;

		Zenith_Log(LOG_CATEGORY_CORE,
			"[GraphEditorLiveAuthoring] authored=%d saved=%d disk1=%d disk2=%d reopened=%d bound=%d reload=%d rate1=%.1f deg/s (expect ~+1080) rate2=%.1f deg/s (expect ~-1080)",
			g_xLiveAuthoring.m_bGraphAuthored ? 1 : 0,
			g_xLiveAuthoring.m_bAssetSaved ? 1 : 0,
			g_xLiveAuthoring.m_bFirstDiskSnapshot ? 1 : 0,
			g_xLiveAuthoring.m_bSecondDiskSnapshot ? 1 : 0,
			g_xLiveAuthoring.m_bReopenRenderedWire ? 1 : 0,
			g_xLiveAuthoring.m_bGraphBound ? 1 : 0,
			g_xLiveAuthoring.m_bReloadObserved ? 1 : 0,
			g_xLiveAuthoring.m_fMeasuredRate1,
			g_xLiveAuthoring.m_fMeasuredRate2);

		return g_xLiveAuthoring.m_bGraphAuthored && g_xLiveAuthoring.m_bAssetSaved && g_xLiveAuthoring.m_bFirstDiskSnapshot
			&& g_xLiveAuthoring.m_bSecondDiskSnapshot && g_xLiveAuthoring.m_bReopenRenderedWire && g_xLiveAuthoring.m_bGraphBound
			&& g_xLiveAuthoring.m_bReloadObserved && bRate1Ok && bRate2Ok;
	}

	// This test drives the mode BOTH ways (Stopped in Setup, Playing at Step 168,
	// Stopped again at Step 368) and so ends Stopped -- GLOBAL editor state that
	// outlives it, and which an early-out (failed assert, timeout,
	// --exit-after-frames short of 368) leaks even on the paths Step 368 covers.
	// Restored centrally in DevilsPlayground.cpp's between-tests hook, NOT in a
	// Teardown here -- see Tests/CLAUDE.md, "Editor mode leaks between tests".
	const Zenith_AutomatedTest g_xGraphEditorLiveAuthoringTest = {
		"Test_GraphEditorLiveAuthoring",
		&Setup_GraphEditorLiveAuthoring,
		&Step_GraphEditorLiveAuthoring,
		&Verify_GraphEditorLiveAuthoring,
		/*maxFrames*/ 460,
		/*requiresGraphics*/ true
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xGraphEditorLiveAuthoringTest);
}

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
