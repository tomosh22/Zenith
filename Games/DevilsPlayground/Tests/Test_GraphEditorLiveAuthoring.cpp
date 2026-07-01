#include "Zenith.h"

//------------------------------------------------------------------------------
// Test_GraphEditorLiveAuthoring - the Behaviour Graphs flagship test.
//
// The input SIMULATOR drives the Graph Editor panel in real time, windowed:
//   1. clicks "OnUpdate" and "RotateEntity" in the palette (nodes appear),
//   2. drags from the OnUpdate output pin to the RotateEntity input pin
//      (edge connects),
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
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"

#include "imgui.h"

#include <cmath>
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

		bool m_bGraphAuthored = false;		// 2 nodes + 1 edge present after authoring
		bool m_bAssetSaved = false;			// file exists after the first Save click
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

	bool MouseToPaletteEntry(const char* szTypeName)
	{
		Zenith_Maths::Vector2 xPos;
		if (!Zenith_GraphEditorPanel::GetPaletteEntryScreenPos(szTypeName, xPos))
		{
			FailHard("palette entry not found");
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

		Zenith_GraphEditorPanel::OpenAsset(szGRAPH_ASSET_PATH);
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

	bool Step_GraphEditorLiveAuthoring(int iFrame)
	{
		if (g_xLiveAuthoring.m_bFailedHard)
		{
			return false;
		}

		// IO diagnostics at the two slider-click moments (authoring vs in-play).
		if (iFrame == 91 || iFrame == 92 || iFrame == 220 || iFrame == 221)
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

		// --- author: place the two nodes from the palette ------------------
		case 30: MouseToPaletteEntry("OnUpdate"); break;
		case 33: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 36: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 42: MouseToPaletteEntry("RotateEntity"); break;
		case 45: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 48: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		// --- connect: drag output pin -> input pin -------------------------
		case 54:
		{
			g_xLiveAuthoring.m_uSourceNodeID = Zenith_GraphEditorPanel::FindNodeIDByType("OnUpdate");
			g_xLiveAuthoring.m_uRotateNodeID = Zenith_GraphEditorPanel::FindNodeIDByType("RotateEntity");
			if (g_xLiveAuthoring.m_uSourceNodeID == 0 || g_xLiveAuthoring.m_uRotateNodeID == 0)
			{
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
		case 57: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 60:
		case 63:
		case 66:
		{
			// Glide toward the input pin across a few frames.
			Zenith_Maths::Vector2 xTarget;
			if (Zenith_GraphEditorPanel::GetPinScreenPos(g_xLiveAuthoring.m_uRotateNodeID, 0, true, xTarget))
			{
				Zenith_Maths::Vector2 xSource;
				Zenith_GraphEditorPanel::GetPinScreenPos(g_xLiveAuthoring.m_uSourceNodeID, 0, false, xSource);
				const float fT = (iFrame == 60) ? 0.4f : (iFrame == 63) ? 0.8f : 1.0f;
				MouseTo(Zenith_Maths::Vector2(xSource.x + (xTarget.x - xSource.x) * fT, xSource.y + (xTarget.y - xSource.y) * fT));
			}
			break;
		}
		case 69: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		// --- select the rotate node, set its rate slider to MAX ------------
		case 75:
		{
			Zenith_Maths::Vector2 xNode;
			if (Zenith_GraphEditorPanel::GetNodeScreenPos(g_xLiveAuthoring.m_uRotateNodeID, xNode))
			{
				MouseTo(xNode);
			}
			break;
		}
		case 78: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 81: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 87: MouseToSliderFraction("m_fDegreesPerSecond", 1.0f); break;
		case 90: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 93: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 98:
		{
			g_xLiveAuthoring.m_bGraphAuthored =
				(Zenith_GraphEditorPanel::GetNodeCount() == 2 && Zenith_GraphEditorPanel::GetEdgeCount() == 1);
			if (!g_xLiveAuthoring.m_bGraphAuthored)
			{
				FailHard("authored graph is not 2 nodes + 1 edge");
			}
			float fRate = 0.0f;
			if (!Zenith_GraphEditorPanel::GetSelectedNodeParamFloat("m_fDegreesPerSecond", fRate) || fRate < 1000.0f)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] slider value after MAX click: %.1f", fRate);
				FailHard("authoring slider click did not set the rate to max");
			}
			break;
		}

		case 100:
			// Screenshot marker: authored graph on canvas, node selected,
			// properties panel showing the maxed slider.
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] marker1_authored");
			break;

		// --- save -----------------------------------------------------------
		case 102:
		{
			Zenith_Maths::Vector2 xSave;
			if (Zenith_GraphEditorPanel::GetToolbarButtonScreenPos("Save", xSave))
			{
				MouseTo(xSave);
			}
			break;
		}
		case 105: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 108: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 114:
		{
			std::error_code xEC;
			g_xLiveAuthoring.m_bAssetSaved = std::filesystem::exists(Zenith_AssetRegistry::ResolvePath(szGRAPH_ASSET_PATH), xEC);
			if (!g_xLiveAuthoring.m_bAssetSaved)
			{
				FailHard("Save click did not write the .bgraph file");
			}
			break;
		}

		// --- bind + play -----------------------------------------------------
		case 118:
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
		case 120:
			// Select the entity in the editor so the panel's live execution
			// highlighting has a target while playing.
			g_xEngine.Editor().SelectEntityByName("GraphAuthorTarget");
			break;
		case 122: g_xEngine.Editor().SetEditorMode(EditorMode::Playing); break;

		// --- measure rate 1 (~ +1080 deg/s) ---------------------------------
		case 140: BeginRateMeasurement(); break;
		case 170:
			// Screenshot marker: playing, live execution highlight on the
			// OnUpdate -> RotateEntity chain.
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] marker2_playing");
			break;
		case 200:
			g_xLiveAuthoring.m_fMeasuredRate1 = FinishRateMeasurement();
			break;

		// --- live edit DURING play: slider to MIN, save, hot reload ---------
		case 205:
		{
			Zenith_Maths::Vector2 xNode;
			if (Zenith_GraphEditorPanel::GetNodeScreenPos(g_xLiveAuthoring.m_uRotateNodeID, xNode))
			{
				MouseTo(xNode);
			}
			break;
		}
		case 208: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 211: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 216: MouseToSliderFraction("m_fDegreesPerSecond", 0.0f); break;
		case 219: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 222: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 225:
		{
			float fRate = 0.0f;
			if (!Zenith_GraphEditorPanel::GetSelectedNodeParamFloat("m_fDegreesPerSecond", fRate) || fRate > -1000.0f)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorLiveAuthoring] slider value after MIN click (in play): %.1f", fRate);
				FailHard("in-play slider click did not set the rate to min");
			}
			break;
		}
		case 226:
		{
			Zenith_Maths::Vector2 xSave;
			if (Zenith_GraphEditorPanel::GetToolbarButtonScreenPos("Save", xSave))
			{
				MouseTo(xSave);
			}
			break;
		}
		case 229: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 232: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 240:
			g_xLiveAuthoring.m_bReloadObserved = Zenith_GraphReload::GetReloadCount() > g_xLiveAuthoring.m_uReloadCountAtBind;
			if (!g_xLiveAuthoring.m_bReloadObserved)
			{
				FailHard("hot reload did not fire after the in-play Save");
			}
			break;

		// --- measure rate 2 (~ -1080 deg/s, live-reversed) -------------------
		case 245: BeginRateMeasurement(); break;
		case 275:
			// Screenshot marker: still playing, after the live hot reload
			// (slider at min, reload status line in the toolbar).
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] marker3_live_reloaded");
			break;
		case 305:
			g_xLiveAuthoring.m_fMeasuredRate2 = FinishRateMeasurement();
			break;

		// --- teardown ---------------------------------------------------------
		case 310: g_xEngine.Editor().SetEditorMode(EditorMode::Stopped); break;
		case 330:
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
		if ((iFrame > 140 && iFrame <= 200) || (iFrame > 245 && iFrame <= 305))
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
			"[GraphEditorLiveAuthoring] authored=%d saved=%d bound=%d reload=%d rate1=%.1f deg/s (expect ~+1080) rate2=%.1f deg/s (expect ~-1080)",
			g_xLiveAuthoring.m_bGraphAuthored ? 1 : 0,
			g_xLiveAuthoring.m_bAssetSaved ? 1 : 0,
			g_xLiveAuthoring.m_bGraphBound ? 1 : 0,
			g_xLiveAuthoring.m_bReloadObserved ? 1 : 0,
			g_xLiveAuthoring.m_fMeasuredRate1,
			g_xLiveAuthoring.m_fMeasuredRate2);

		return g_xLiveAuthoring.m_bGraphAuthored && g_xLiveAuthoring.m_bAssetSaved && g_xLiveAuthoring.m_bGraphBound
			&& g_xLiveAuthoring.m_bReloadObserved && bRate1Ok && bRate2Ok;
	}

	const Zenith_AutomatedTest g_xGraphEditorLiveAuthoringTest = {
		"Test_GraphEditorLiveAuthoring",
		&Setup_GraphEditorLiveAuthoring,
		&Step_GraphEditorLiveAuthoring,
		&Verify_GraphEditorLiveAuthoring,
		/*maxFrames*/ 400,
		/*requiresGraphics*/ true
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xGraphEditorLiveAuthoringTest);
}

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
