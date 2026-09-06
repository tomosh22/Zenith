#include "Zenith.h"

//------------------------------------------------------------------------------
// RT_AnimationEditorLiveAuthoring (WU-3.4) — the dope sheet driven by a
// SIMULATED MOUSE, over the hit rects WU-3.2 records, asserted against the
// Action_* twins WU-3.3 exposes.
//
// The claim, in one sentence: **a gesture and the action it is supposed to mean
// produce the same edit.** Two halves, each an equivalence rather than a
// hard-coded expectation:
//
//   1. A rubber band dragged across two keys selects exactly what
//      Action_BoxSelect(the same four screen coordinates) selects.
//   2. A key dragged N pixels lands where Action_MoveSelection(the same delta,
//      snapped) puts a key that started at the same time.
//
// ★ AN EQUIVALENCE IS THE POINT, NOT A CONVENIENCE. A test that asserted "the
// key ended at 1.1333 s" would pin the panel's arithmetic twice and could be
// made to pass by copying the bug into the expectation. Comparing the two paths
// pins the thing that actually has to hold — that the mouse handler in
// _Render.cpp adds NOTHING of its own to the Action_* call it ends in — and it
// is the only assertion in the repo that can catch the handler drifting.
//
// ★ NOT requiresGraphics, DELIBERATELY, and this is the one line to re-read
// before "fixing" a failure here by setting it. ImGui is genuinely live on the
// Null backend (Zenith/Null/CLAUDE.md): a real context, a rasterised font atlas,
// a real GLFW window that is merely hidden, and therefore real layout and real
// hit-testing — only the draw data is discarded. Nothing below reads a pixel.
// The Graph Editor's equivalent IS marked requiresGraphics and that is a
// LIABILITY, not a precedent: such a test is SKIPPED-AS-PASSED headless, so it
// rots while the gate stays green (two were found red-but-unreported that way —
// see Games/DevilsPlayground/Tests/CLAUDE.md).
//
// ★ INSIDE Step() ONLY SIMULATOR STATE-SETTERS ARE LEGAL. SimulateMouseClick and
// StepFrame nest Zenith_MainLoop and deadlock a windowed run, so every gesture
// is spread over several frames: set the position / button state for THIS frame
// and return, and let the NEXT Step observe the result. Zenith_ImGuiInputBridge
// pumps the simulated state into ImGui between the platform NewFrame and
// ImGui::NewFrame, so a position set in Step(i) is the position the panel is
// hit-tested against in frame i's own render, and the rects that render records
// are readable in Step(i + 1).
//
// ★ THE WINDOW IS UNDOCKED FOR THE RUN. The Animation Editor is a TAB in the
// bottom dock node, behind the Content Browser, and the bottom strip is ~28% of
// the work area — a docked sheet would be both hidden and too short to hold the
// rows this test clicks, and RequestWindowPlacement is ignored for a docked
// window. So the window is floated, given an explicit geometry (exactly what the
// panel's own units do), and RE-DOCKED to its original node in Teardown.
//------------------------------------------------------------------------------

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Zenith_AnimTimelineMath.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

#include "imgui.h"
#include "imgui_internal.h"	// FindWindowByName + DockBuilderDockWindow (undock/re-dock)

#include <cmath>
#include <filesystem>

namespace
{
	// How many keys the rubber band is expected to be able to hold. The band in
	// this test covers two; the array is the recording buffer, not the assertion.
	constexpr u_int uMAX_RECORDED_SELECTION = 16u;

	// The drag distance, expressed in FRAMES of the clip's own grid and converted
	// to pixels through the live view. Four frames at 30 fps is 0.1333 s — far
	// enough from the neighbouring keys at 0 s and 2 s that the drop cannot be
	// refused by a D11 collision, and an exact multiple of the frame grid so the
	// snap has an unambiguous answer.
	constexpr float fDRAG_FRAMES = 4.0f;

	struct AnimLiveAuthoringState
	{
		std::string m_strClipPath;
		std::string m_strDirectory;

		// Restored in Teardown — see the header comment on undocking.
		ImGuiID m_uOriginalDockId = 0;
		bool m_bDockIdCaptured = false;

		int m_iBaseFrame = -1;			// the frame the sheet was confirmed drawn on

		u_int m_uHipKeyAtZero = uINVALID_ANIM_KEY_ID;
		u_int m_uHipKeyAtOne = uINVALID_ANIM_KEY_ID;
		u_int m_uSpineKeyAtOne = uINVALID_ANIM_KEY_ID;

		// ---- the box-select half ----
		float m_fBandX0 = 0.0f, m_fBandY0 = 0.0f, m_fBandX1 = 0.0f, m_fBandY1 = 0.0f;
		u_int m_auMouseSelection[uMAX_RECORDED_SELECTION] = {};
		u_int m_uMouseSelectionCount = 0;
		u_int m_auActionSelection[uMAX_RECORDED_SELECTION] = {};
		u_int m_uActionSelectionCount = 0;
		bool m_bMouseSelectionAllOnHipTrack = false;
		bool m_bActionSelectionAllOnHipTrack = false;
		bool m_bBoxSelectionsMatch = false;

		// ---- the drag half ----
		Zenith_AnimTimelineView m_xDragView;
		float m_fDragPixels = 0.0f;
		float m_fRawDeltaSeconds = 0.0f;
		float m_fHipTimeBefore = 0.0f;
		float m_fHipTimeAfter = 0.0f;
		float m_fSpineTimeBefore = 0.0f;
		float m_fSpineTimeAfter = 0.0f;
		bool m_bDragStarted = false;		// the panel actually entered its key drag
		bool m_bActionMoveApplied = false;

		bool m_bFailedHard = false;
		char m_acFailReason[256] = {};
	};

	AnimLiveAuthoringState g_xAnim;

	void FailHard(const char* szReason)
	{
		if (!g_xAnim.m_bFailedHard)
		{
			g_xAnim.m_bFailedHard = true;
			snprintf(g_xAnim.m_acFailReason, sizeof(g_xAnim.m_acFailReason), "%s", szReason);
			Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimEditorLiveAuthoring] FAIL: %s", szReason);
		}
	}

	Zenith_EditorPanel_Animation& Panel() { return Zenith_EditorPanel_Animation::Instance(); }

	Zenith_AnimTrackId HipPosition()   { return Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION); }
	Zenith_AnimTrackId SpinePosition() { return Zenith_AnimTrackId::Bone("Spine", FLUX_ANIM_TRACK_POSITION); }

	// State-setter only (see the header comment).
	void MouseTo(float fX, float fY)
	{
		Zenith_InputSimulator::SimulateMousePosition(static_cast<double>(fX), static_cast<double>(fY));
	}

	// Two bones, three position keys each at 0 / 1 / 2 s, 30 fps, duration 2 s.
	// Two bones rather than one because the drag half needs a SECOND key that
	// started at the same time as the dragged one, to move through the action
	// path and compare against; three keys rather than two because the rubber
	// band has to be able to select some and leave others out.
	void WriteProbeClip(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("AnimEditorLiveAuthoringProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_bGenerated = false;	// D21 refuses a GENERATED clip outright
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		Flux_BoneChannel xSpine;
		xSpine.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xSpine.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f));
		xSpine.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(1.0f, 2.0f, 0.0f));
		xSpine.SortKeyframes();
		xClip.AddBoneChannel("Spine", std::move(xSpine));

		xClip.Export(strPath);
	}

	// Snapshot the panel's key selection into one of the two recording buffers,
	// and report whether every entry sits on the Hip position track (the band is
	// aimed at exactly that row, so anything else means the hit test leaked
	// vertically into a neighbouring row).
	u_int RecordSelection(u_int* puOut, bool& bOutAllOnHipTrack)
	{
		const Zenith_AnimTrackId xHip = HipPosition();
		const u_int uCount = Panel().GetSelectedKeyCount();
		const u_int uRecorded = uCount < uMAX_RECORDED_SELECTION ? uCount : uMAX_RECORDED_SELECTION;
		bOutAllOnHipTrack = true;
		for (u_int u = 0; u < uRecorded; ++u)
		{
			Zenith_AnimTrackId xTrack;
			u_int uKeyId = uINVALID_ANIM_KEY_ID;
			Panel().GetSelectedKeyAt(u, xTrack, uKeyId);
			puOut[u] = uKeyId;
			if (!(xTrack == xHip))
			{
				bOutAllOnHipTrack = false;
			}
		}
		return uRecorded;
	}

	bool ContainsId(const u_int* puIds, u_int uCount, u_int uWanted)
	{
		for (u_int u = 0; u < uCount; ++u)
		{
			if (puIds[u] == uWanted)
			{
				return true;
			}
		}
		return false;
	}

	// ★ EVERY FAILURE IN THIS TEST HAS FOUR SUSPECTS, AND A BARE `false` NAMES
	// NONE OF THEM: the simulated click never reached ImGui, it reached it at the
	// wrong place, the panel was not drawing, or the operation itself refused.
	// The panel exposes the discriminators for the last two on purpose (see the
	// hit-rect note in its header) and the IO answers the first two, so one line
	// separates all four rather than costing a rebuild to find out. This is the
	// lesson Test_GraphEditorLiveAuthoring paid for.
	void LogSheetDiagnostics(const char* szWhen)
	{
		const ImGuiIO& xIO = ImGui::GetIO();
		Zenith_Log(LOG_CATEGORY_EDITOR,
			"[AnimEditorLiveAuthoring] DIAG %s: open=%d shown=%d renderedFrames=%u drawnLastFrame=%d "
			"trackWidth=%.1f recordedDisplay=(%.0f, %.0f) liveDisplay=(%.0f, %.0f) "
			"mouse=(%.1f, %.1f) down0=%d wantCapture=%d dragging=%d boxing=%d selected=%u",
			szWhen, Panel().IsOpen() ? 1 : 0, Panel().IsShown() ? 1 : 0,
			Panel().GetRenderedFrameCount(), Panel().WasSheetDrawnLastFrame() ? 1 : 0,
			Panel().GetLastTrackWidth(), Panel().GetRecordedDisplayWidth(), Panel().GetRecordedDisplayHeight(),
			xIO.DisplaySize.x, xIO.DisplaySize.y,
			xIO.MousePos.x, xIO.MousePos.y, xIO.MouseDown[0] ? 1 : 0, xIO.WantCaptureMouse ? 1 : 0,
			Panel().IsDraggingKeys() ? 1 : 0, Panel().IsBoxSelecting() ? 1 : 0,
			Panel().GetSelectedKeyCount());
	}

	//--------------------------------------------------------------------------
	// Setup
	//--------------------------------------------------------------------------
	void Setup_AnimEditorLiveAuthoring()
	{
		g_xAnim = AnimLiveAuthoringState();

		Zenith_InputSimulator::Enable();

		// A private temp directory, removed in Teardown. Nothing here touches a
		// tracked asset: the clip is a fixture, not content.
		std::error_code xError;
		std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
		if (xError)
		{
			xRoot = ".";
		}
		const std::filesystem::path xDirectory = xRoot / "zenith_rt_animeditor_live";
		std::filesystem::remove_all(xDirectory, xError);
		std::filesystem::create_directories(xDirectory, xError);
		g_xAnim.m_strDirectory = xDirectory.generic_string();
		g_xAnim.m_strClipPath = (xDirectory / "live.zanim").generic_string();
		WriteProbeClip(g_xAnim.m_strClipPath);

		Panel().ShowFlag() = true;
		if (!Panel().OpenClip(g_xAnim.m_strClipPath))
		{
			FailHard("the probe clip did not open into the dope sheet");
		}
	}

	//--------------------------------------------------------------------------
	// Step
	//--------------------------------------------------------------------------

	// Float the window and pin its geometry. Runs while the window still may not
	// exist (the panel only calls Begin once it is shown), so the dock id is
	// captured opportunistically and the placement is re-requested afterwards.
	void FloatAndPlaceWindow()
	{
		if (!g_xAnim.m_bDockIdCaptured)
		{
			if (ImGuiWindow* pxWindow = ImGui::FindWindowByName(szEDITOR_WINDOW_ANIMATION_EDITOR))
			{
				g_xAnim.m_uOriginalDockId = pxWindow->DockId;
				g_xAnim.m_bDockIdCaptured = true;
			}
		}
		// Node 0 = floating. Applies at the window's next Begin.
		ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_ANIMATION_EDITOR, 0);

		// A generous, display-relative geometry: big enough that every row and
		// every key of the probe is on screen, and entirely inside the display so
		// the off-screen gate cannot refuse a rect.
		const ImVec2 xDisplay = ImGui::GetIO().DisplaySize;
		const float fWidth = (xDisplay.x - 40.0f) < 1000.0f ? (xDisplay.x - 40.0f) : 1000.0f;
		const float fHeight = (xDisplay.y - 40.0f) < 560.0f ? (xDisplay.y - 40.0f) : 560.0f;
		Panel().RequestWindowPlacement(20.0f, 20.0f, fWidth, fHeight);

		// Focusing a docked window selects its tab on the NEXT frame's
		// DockNodeUpdateTabBar; focusing a floating one brings it to the display
		// front. Both are wanted, and which applies depends on whether the undock
		// above has landed yet, so it is issued on every setup frame.
		ImGui::SetWindowFocus(szEDITOR_WINDOW_ANIMATION_EDITOR);
	}

	// Everything the scripted phases below need, captured once. False (with the
	// diagnostics logged) until the sheet has actually been drawn and every rect
	// this test clicks resolves.
	bool TryCaptureSheet()
	{
		if (!Panel().IsOpen() || !Panel().WasSheetDrawnLastFrame()
		 || Panel().GetLastTrackWidth() <= 0.0f || Panel().GetRecordedDisplayWidth() <= 0.0f)
		{
			return false;
		}

		const Zenith_AnimTrackId xHip = HipPosition();
		const Zenith_AnimTrackId xSpine = SpinePosition();
		if (Panel().Document().GetKeyCount(xHip) != 3u || Panel().Document().GetKeyCount(xSpine) != 3u)
		{
			return false;
		}
		g_xAnim.m_uHipKeyAtZero = Panel().Document().GetKeyIdAtIndex(xHip, 0u);
		g_xAnim.m_uHipKeyAtOne = Panel().Document().GetKeyIdAtIndex(xHip, 1u);
		g_xAnim.m_uSpineKeyAtOne = Panel().Document().GetKeyIdAtIndex(xSpine, 1u);
		if (g_xAnim.m_uHipKeyAtZero == uINVALID_ANIM_KEY_ID
		 || g_xAnim.m_uHipKeyAtOne == uINVALID_ANIM_KEY_ID
		 || g_xAnim.m_uSpineKeyAtOne == uINVALID_ANIM_KEY_ID)
		{
			return false;
		}

		Zenith_AnimPanelRect xRectAtZero;
		Zenith_AnimPanelRect xRectAtOne;
		if (!Panel().GetKeyRect(xHip, g_xAnim.m_uHipKeyAtZero, xRectAtZero)
		 || !Panel().GetKeyRect(xHip, g_xAnim.m_uHipKeyAtOne, xRectAtOne))
		{
			return false;
		}

		// The band: from EMPTY canvas just right of the key at t=1 (a press on a
		// key would start a key drag instead of a rubber band) leftwards onto the
		// centre of the key at t=0. Vertically it is the key row's own extent
		// plus a single pixel — widening it would risk reaching the keys of the
		// row above or below, which would make "what the band selects" depend on
		// the row height rather than on the band.
		g_xAnim.m_fBandX0 = xRectAtOne.m_fMaxX + 12.0f;
		g_xAnim.m_fBandY0 = xRectAtOne.m_fMinY - 1.0f;
		g_xAnim.m_fBandX1 = xRectAtZero.Centre().x;
		g_xAnim.m_fBandY1 = xRectAtZero.m_fMaxY + 1.0f;
		return true;
	}

	bool Step_AnimEditorLiveAuthoring(int iFrame)
	{
		if (g_xAnim.m_bFailedHard)
		{
			return false;
		}

		// ---- setup: float, place, focus, then wait for a drawn sheet ----------
		if (g_xAnim.m_iBaseFrame < 0)
		{
			if (iFrame == 1 || iFrame == 3 || iFrame == 5)
			{
				FloatAndPlaceWindow();
			}
			if (iFrame >= 8 && TryCaptureSheet())
			{
				g_xAnim.m_iBaseFrame = iFrame;
				LogSheetDiagnostics("sheet ready");
			}
			else if (iFrame > 60)
			{
				LogSheetDiagnostics("timed out waiting for the sheet");
				FailHard("the dope sheet never drew a usable canvas");
			}
			return true;
		}

		const int iF = iFrame - g_xAnim.m_iBaseFrame;
		const Zenith_AnimTrackId xHip = HipPosition();
		const Zenith_AnimTrackId xSpine = SpinePosition();

		switch (iF)
		{
		// ---- half 1: the rubber band ------------------------------------------
		case 2: MouseTo(g_xAnim.m_fBandX0, g_xAnim.m_fBandY0); break;
		case 5: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 8:
			// Mid-band, so the gesture is a real drag rather than a teleport: the
			// panel decides "band" vs "click" from the total travel, and a single
			// jump would exercise neither the tracking nor the slop test.
			MouseTo((g_xAnim.m_fBandX0 + g_xAnim.m_fBandX1) * 0.5f,
				(g_xAnim.m_fBandY0 + g_xAnim.m_fBandY1) * 0.5f);
			break;
		case 11: MouseTo(g_xAnim.m_fBandX1, g_xAnim.m_fBandY1); break;
		case 14:
			if (!Panel().IsBoxSelecting())
			{
				LogSheetDiagnostics("band should be live");
				FailHard("the press on empty canvas did not start a rubber band");
			}
			Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1);
			break;

		case 17:
		{
			g_xAnim.m_uMouseSelectionCount =
				RecordSelection(g_xAnim.m_auMouseSelection, g_xAnim.m_bMouseSelectionAllOnHipTrack);

			// ★ THE EQUIVALENCE. The same four screen coordinates, straight into
			// the action the handler ends in. If these two ever disagree, the
			// mouse path has grown arithmetic of its own.
			Panel().Action_ClearSelection();
			Panel().Action_BoxSelect(g_xAnim.m_fBandX0, g_xAnim.m_fBandY0,
				g_xAnim.m_fBandX1, g_xAnim.m_fBandY1, ZENITH_ANIMSELECT_REPLACE);
			g_xAnim.m_uActionSelectionCount =
				RecordSelection(g_xAnim.m_auActionSelection, g_xAnim.m_bActionSelectionAllOnHipTrack);

			bool bMatch = g_xAnim.m_uMouseSelectionCount == g_xAnim.m_uActionSelectionCount;
			for (u_int u = 0; bMatch && u < g_xAnim.m_uMouseSelectionCount; ++u)
			{
				bMatch = ContainsId(g_xAnim.m_auActionSelection, g_xAnim.m_uActionSelectionCount,
					g_xAnim.m_auMouseSelection[u]);
			}
			g_xAnim.m_bBoxSelectionsMatch = bMatch;
			break;
		}

		// ---- half 2: the key drag ---------------------------------------------
		case 20:
		{
			// ★ CLEAR FIRST. The band left the key at t=1 selected, and a plain
			// click on an ALREADY-SELECTED key deliberately KEEPS the selection
			// (so a multi-key drag is not collapsed by the mouse-down that starts
			// it) — which would make this drag move two keys and the comparison
			// against a one-key action meaningless.
			Panel().Action_ClearSelection();

			Zenith_AnimPanelRect xRect;
			if (!Panel().GetKeyRect(xHip, g_xAnim.m_uHipKeyAtOne, xRect))
			{
				LogSheetDiagnostics("aiming at the key to drag");
				FailHard("the key to drag has no on-screen rect");
				break;
			}

			// The view is captured with the aim, not re-read after the drop: the
			// delta the panel used is a function of the view at drag time, and
			// reading it later would silently forgive a zoom that moved.
			g_xAnim.m_xDragView = Panel().View();
			const u_int uFrameRate = Panel().GetFrameRate();
			if (uFrameRate == 0u)
			{
				FailHard("the probe clip reports no frame grid, so a snapped drag has no meaning");
				break;
			}
			g_xAnim.m_fDragPixels = Zenith_AnimTimelineSecondsToPixels(g_xAnim.m_xDragView,
				fDRAG_FRAMES / static_cast<float>(uFrameRate));
			g_xAnim.m_fRawDeltaSeconds =
				Zenith_AnimTimelinePixelsToSeconds(g_xAnim.m_xDragView, g_xAnim.m_fDragPixels);

			Panel().Document().GetKeyTime(xHip, g_xAnim.m_uHipKeyAtOne, g_xAnim.m_fHipTimeBefore);
			Panel().Document().GetKeyTime(xSpine, g_xAnim.m_uSpineKeyAtOne, g_xAnim.m_fSpineTimeBefore);

			MouseTo(xRect.Centre().x, xRect.Centre().y);
			break;
		}
		case 23: Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_1); break;
		case 26:
		{
			// The press must have both selected the key and armed the drag.
			g_xAnim.m_bDragStarted = Panel().IsDraggingKeys()
				&& Panel().IsKeySelected(xHip, g_xAnim.m_uHipKeyAtOne)
				&& Panel().GetSelectedKeyCount() == 1u;
			if (!g_xAnim.m_bDragStarted)
			{
				LogSheetDiagnostics("drag should be live");
				FailHard("the press on the key did not start a single-key drag");
				break;
			}
			Zenith_AnimPanelRect xRect;
			if (Panel().GetKeyRect(xHip, g_xAnim.m_uHipKeyAtOne, xRect))
			{
				MouseTo(xRect.Centre().x + g_xAnim.m_fDragPixels * 0.5f, xRect.Centre().y);
			}
			break;
		}
		case 29:
		{
			// The final position, measured from the key's rect centre — which is
			// still where it was, because a drag MUTATES NOTHING until it is
			// released. That centre IS the x the panel recorded as its drag
			// start, so the pixel delta the drop applies is exactly m_fDragPixels
			// with no arithmetic of this test's own in between.
			Zenith_AnimPanelRect xRect;
			if (!Panel().GetKeyRect(xHip, g_xAnim.m_uHipKeyAtOne, xRect))
			{
				LogSheetDiagnostics("completing the drag");
				FailHard("the dragged key lost its rect mid-drag");
				break;
			}
			MouseTo(xRect.Centre().x + g_xAnim.m_fDragPixels, xRect.Centre().y);
			break;
		}
		case 32: Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1); break;

		case 36:
		{
			Panel().Document().GetKeyTime(xHip, g_xAnim.m_uHipKeyAtOne, g_xAnim.m_fHipTimeAfter);

			// ★ THE EQUIVALENCE, again. A key that started at the same time, moved
			// through the ACTION with the same raw pixel-derived delta and the
			// same snap, must land in the same place as the one the mouse dragged.
			Panel().Action_ClearSelection();
			if (!Panel().Action_SelectKey(xSpine, g_xAnim.m_uSpineKeyAtOne, ZENITH_ANIMSELECT_REPLACE))
			{
				FailHard("could not select the comparison key on Spine");
				break;
			}
			g_xAnim.m_bActionMoveApplied = Panel().Action_MoveSelection(g_xAnim.m_fRawDeltaSeconds, true);
			Panel().Document().GetKeyTime(xSpine, g_xAnim.m_uSpineKeyAtOne, g_xAnim.m_fSpineTimeAfter);
			break;
		}

		case 39:
			return false;	// done

		default:
			break;
		}

		return true;
	}

	//--------------------------------------------------------------------------
	// Verify
	//--------------------------------------------------------------------------
	bool Verify_AnimEditorLiveAuthoring()
	{
		if (g_xAnim.m_bFailedHard)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimEditorLiveAuthoring] failed: %s", g_xAnim.m_acFailReason);
			return false;
		}

		bool bPass = true;

		// ---- half 1 ----
		if (g_xAnim.m_uMouseSelectionCount < 2u)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] the simulated band selected %u keys; it must select at least 2, "
				"or the comparison below is two empty sets agreeing", g_xAnim.m_uMouseSelectionCount);
			bPass = false;
		}
		if (!g_xAnim.m_bBoxSelectionsMatch)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] the simulated band (%u keys) and Action_BoxSelect over the SAME "
				"rectangle (%u keys) disagree", g_xAnim.m_uMouseSelectionCount, g_xAnim.m_uActionSelectionCount);
			bPass = false;
		}
		if (!g_xAnim.m_bMouseSelectionAllOnHipTrack || !g_xAnim.m_bActionSelectionAllOnHipTrack)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] the band reached a row it was not aimed at (mouse=%d action=%d)",
				g_xAnim.m_bMouseSelectionAllOnHipTrack ? 1 : 0, g_xAnim.m_bActionSelectionAllOnHipTrack ? 1 : 0);
			bPass = false;
		}
		// The band deliberately excludes the key at t=2, so the selection must be
		// a STRICT SUBSET of the track — otherwise "it selects what the action
		// selects" would hold trivially for a hit test that selects everything.
		if (g_xAnim.m_uMouseSelectionCount >= Panel().Document().GetKeyCount(HipPosition()))
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] the band selected %u keys — it was aimed at a strict subset",
				g_xAnim.m_uMouseSelectionCount);
			bPass = false;
		}

		// ---- half 2 ----
		if (!g_xAnim.m_bDragStarted || !g_xAnim.m_bActionMoveApplied)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] dragStarted=%d actionMoveApplied=%d",
				g_xAnim.m_bDragStarted ? 1 : 0, g_xAnim.m_bActionMoveApplied ? 1 : 0);
			bPass = false;
		}
		// "It moved at all" is asserted separately from "it moved to the same
		// place": a drag that did nothing would otherwise pass the equivalence
		// against an action move that also did nothing.
		if (std::fabs(g_xAnim.m_fHipTimeAfter - g_xAnim.m_fHipTimeBefore) < 1.0e-4f)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] the simulated drag moved the key nowhere (%.6f -> %.6f s over %.1f px)",
				g_xAnim.m_fHipTimeBefore, g_xAnim.m_fHipTimeAfter, g_xAnim.m_fDragPixels);
			bPass = false;
		}
		const float fMouseDelta = g_xAnim.m_fHipTimeAfter - g_xAnim.m_fHipTimeBefore;
		const float fActionDelta = g_xAnim.m_fSpineTimeAfter - g_xAnim.m_fSpineTimeBefore;
		if (std::fabs(fMouseDelta - fActionDelta) > 1.0e-4f)
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[AnimEditorLiveAuthoring] a %.1f px drag moved the key %.6f s, but Action_MoveSelection with "
				"the same raw delta (%.6f s, snapped) moved its twin %.6f s",
				g_xAnim.m_fDragPixels, fMouseDelta, g_xAnim.m_fRawDeltaSeconds, fActionDelta);
			bPass = false;
		}

		Zenith_Log(LOG_CATEGORY_EDITOR,
			"[AnimEditorLiveAuthoring] band: mouse=%u action=%u match=%d | drag: %.1f px, raw %.6f s, "
			"hip %.6f -> %.6f, spine %.6f -> %.6f",
			g_xAnim.m_uMouseSelectionCount, g_xAnim.m_uActionSelectionCount, g_xAnim.m_bBoxSelectionsMatch ? 1 : 0,
			g_xAnim.m_fDragPixels, g_xAnim.m_fRawDeltaSeconds,
			g_xAnim.m_fHipTimeBefore, g_xAnim.m_fHipTimeAfter,
			g_xAnim.m_fSpineTimeBefore, g_xAnim.m_fSpineTimeAfter);

		return bPass;
	}

	//--------------------------------------------------------------------------
	// Teardown — runs on every normal path, including a timeout or a failed
	// Verify, which is exactly when the residue would otherwise be left behind.
	//
	// This test installs three things no entity and no scene owns: a floating
	// dock state, a shown panel holding an open document, and a registry entry
	// for a file that is about to be deleted. Note that it does NOT touch the
	// editor MODE — the dope sheet is composed on every editor frame regardless
	// of Playing / Stopped, so there was never a reason to leave Stopped behind
	// (the leak RenderTest's CLAUDE.md warns about, and which its showcase tests
	// have to undo here).
	//--------------------------------------------------------------------------
	void Teardown_AnimEditorLiveAuthoring()
	{
		// Never leave a button held: the next test's first click would arrive as
		// a release.
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_1);

		Panel().CloseClip();
		Panel().ShowFlag() = false;

		if (g_xAnim.m_bDockIdCaptured)
		{
			ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_ANIMATION_EDITOR, g_xAnim.m_uOriginalDockId);
		}

		if (!g_xAnim.m_strClipPath.empty())
		{
			Zenith_AssetRegistry::ForceUnload(g_xAnim.m_strClipPath);
		}
		if (!g_xAnim.m_strDirectory.empty())
		{
			std::error_code xError;
			std::filesystem::remove_all(std::filesystem::path(g_xAnim.m_strDirectory), xError);
		}
	}

	const Zenith_AutomatedTest g_xAnimEditorLiveAuthoringTest = {
		"RT_AnimationEditorLiveAuthoring",
		&Setup_AnimEditorLiveAuthoring,
		&Step_AnimEditorLiveAuthoring,
		&Verify_AnimEditorLiveAuthoring,
		/* maxFrames */ 180,
		false /* m_bRequiresGraphics — see the header comment; this runs headless */,
		false /* m_bManualOnly */,
		&Teardown_AnimEditorLiveAuthoring,
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xAnimEditorLiveAuthoringTest);
}

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
