#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Zenith_EditorUI.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/Zenith_DragDropPayloads.h"
#include "FileAccess/Zenith_FileAccess.h"   // ZENITH_ANIMMASK_EXT, in the section's prompt
#include "Editor/Animation/Zenith_BoneSpace.h"                 // E1: the IK handle's leader starts at the joint
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"          // ...which is read off the session's instance
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ImGuiIntegration.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <string>

//=============================================================================
// The DRAWING half of the dope sheet.
//
// ★ EVERYTHING BELOW THE TOOLBAR IS DRAW-LIST WORK OVER ONE InvisibleButton
// (Editor/CLAUDE.md's "draw-list decorations, not items"). The toolbar and the
// preview pane are ordinary ImGui items laid out by the cursor; the sheet is
// not, and must not be — a row is a decoration painted at an absolute screen
// coordinate, and placing items there with SetCursorScreenPos + restore trips
// ErrorCheckUsingSetCursorPosToExtendParentBoundaries, which in a windowed
// build is a modal CRT dialog nothing logs.
//
// ★ NO FUNCTION HERE CONVERTS SECONDS TO PIXELS ITSELF. Every one of them asks
// Zenith_AnimTimelineTimeToPixel, so "where the panel drew the key" and "what
// WU-3.1's pure mapping says" cannot drift — which is exactly the equality a
// unit can check without a screenshot.
//=============================================================================

namespace
{
	// Layout, at 1x DPI. Everything goes through Zenith_EditorUI::Px.
	constexpr float fSHEET_LABEL_WIDTH_1X   = 190.0f;
	constexpr float fSHEET_RULER_HEIGHT_1X  = 22.0f;
	constexpr float fSHEET_ROW_HEIGHT_1X    = 18.0f;
	constexpr float fSHEET_KEY_HALF_1X      = 5.0f;
	constexpr float fSHEET_EVENT_HALF_1X    = 5.0f;
	constexpr float fSHEET_PLAYHEAD_HALF_1X = 1.5f;
	constexpr float fSHEET_PREVIEW_SIZE_1X  = 192.0f;
	// How near the clip-end line counts as grabbing the duration handle.
	constexpr float fSHEET_DURATION_GRAB_1X = 5.0f;
	// A press-and-release inside this many pixels is a CLICK, not a rubber band.
	// Without it every click on empty space would run a zero-area box select,
	// which selects nothing and so looks identical — until a click that drifted
	// one pixel while the button was down silently became a band.
	constexpr float fSHEET_CLICK_SLOP_1X = 3.0f;

	// A ruler cannot be allowed to iterate unboundedly however degenerate the
	// view is: ChooseTicks guarantees a strictly positive step, and this is the
	// belt to that pair of braces.
	constexpr u_int uSHEET_MAX_TICKS = 4096u;

	ImVec2 Vec(float fX, float fY) { return ImVec2(fX, fY); }

	bool IsFiniteFloat(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	// Rows a bone header owns are drawn indented; this is that indent.
	float LabelIndentForRow(Zenith_AnimSheetRowKind eKind)
	{
		switch (eKind)
		{
			case ZENITH_ANIMSHEET_ROW_BONE_TRACK:
			case ZENITH_ANIMSHEET_ROW_ROOT_MOTION_TRACK:
				return Zenith_EditorUI::Px(22.0f);
			default:
				return Zenith_EditorUI::Px(6.0f);
		}
	}
}

//=============================================================================
// Frame
//=============================================================================

void Zenith_EditorPanel_Animation::Render(float fDtSeconds)
{
	// ★ CLEARED UNCONDITIONALLY, BEFORE THE EARLY RETURNS. A panel that is
	// hidden, collapsed or on an unselected dock tab draws nothing, and the one
	// thing it must not do is keep answering rect queries with last frame's
	// coordinates — that is a click into a window nobody can see.
	ClearFrameRects();

	// D11's flash is measured in FRAMES, not seconds: the caller may legitimately
	// pass dt 0 (the editor's Paused mode passes exactly that), and a flash on a
	// wall clock would then never expire.
	if (m_uCollisionFlashFrames > 0u)
	{
		--m_uCollisionFlashFrames;
	}

	if (!m_bShow)
	{
		// ★ A HIDDEN PANEL LOWERS ITS PREVIEW VIEW, AND THIS IS THE ONLY PLACE THAT
		// CAN. Nothing else in the engine deactivates the animation preview slot —
		// the material preview's per-frame janitor lowers slot 5 and only slot 5 —
		// and every other call into the session is below this return, so a panel
		// hidden with a clip still open would leave a full per-view pass chain
		// rendering a preview nobody can see. The call is idempotent (see
		// Zenith_AnimationPreviewSession::DeactivatePreviewView), so calling it on
		// every hidden frame requests exactly one graph rebuild.
		m_xSession.DeactivatePreviewView();

		// ★ A GESTURE CANNOT SURVIVE THE PANEL BEING HIDDEN. The mouse-up that
		// would have ended it is delivered to whatever is on screen now, so a drag
		// left in flight here would apply itself to a later, unrelated release.
		m_bDraggingKeys = false;
		m_bBoxSelecting = false;
		m_bScrubbing = false;
		m_bDraggingDuration = false;
		m_fDragDeltaSeconds = 0.0f;
		m_bDraggingEvents = false;
		m_fEventDragDeltaNormalized = 0.0f;
		m_bEventContextMenuRequested = false;
		// WU-8.2's handle drag, for the same reason: it commits on RELEASE, and the
		// release that would have committed it goes to whatever is on screen now.
		m_bCurveHandleDragActive = false;
		m_bCurveDragMoved = false;
		m_uCurveDragKeyId = uINVALID_ANIM_KEY_ID;
		// ★ AND BOTH POSE MANIPULATORS, WHICH THIS RETURN USED TO WALK PAST (E1).
		// Clearing their flags the way the seven above are cleared would be WRONG:
		// each of them owns a LATCHED bone rotation and an open drag bracket on the
		// session, so dropping the flag alone leaves the bone wherever the last
		// mouse move put it and leaves clip evaluation suspended with nothing left
		// to resume it. The cancel restores the latch and closes the bracket.
		CancelAllPoseGestures();
		return;
	}

	// ★ A RIG RE-RESOLVE INVALIDATES EVERY LATCHED ROTATION A MANIPULATOR IS
	// HOLDING, and this is the only thing the panel can observe it through:
	// ResolveRigInternal deletes and rebuilds the skeleton instance on every
	// call, but the SESSION only drops its own drag when the bone COUNT moved —
	// so a re-resolve to the same rig (the "Use this rig" button pressed twice,
	// a reload) left a live gesture pointing at a pose that no longer exists.
	// Checked before anything below can draw a handle from it.
	if (m_xSession.GetRigGeneration() != m_uSeenRigGeneration)
	{
		m_uSeenRigGeneration = m_xSession.GetRigGeneration();
		CancelAllPoseGestures();
	}
	if (m_xDocument.IsOpen())
	{
		RebuildRows();
		RecountKeysPastDuration();
		SyncSessionWithDocument();

		if (m_xSession.IsOpen() && m_xSession.IsPlaying() && fDtSeconds > 0.0f)
		{
			// The session owns a PRIVATE controller (D30), so there is no drive
			// guard to take here: nothing else can be advancing it.
			m_xSession.Tick(fDtSeconds);
		}
	}

	// AFTER the tick and the clip refresh, deliberately: the seed reads the
	// model-transform cache, and taking it above would draw this frame's handle
	// off last frame's pose — one frame of lag behind the bone it is supposed to
	// sit on. Cheap and unconditional (one matrix read), and it returns
	// immediately while a drag is live, which owns the target.
	SeedIKTargetFromSelection();

	if (m_bPlacementRequested)
	{
		ImGui::SetNextWindowPos(Vec(m_fPlacementX, m_fPlacementY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(Vec(m_fPlacementWidth, m_fPlacementHeight), ImGuiCond_Always);
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
		m_bPlacementRequested = false;
	}

	// ★ THE WINDOW TITLE IS THE CONSTANT AND NOTHING ELSE. DockBuilderDockWindow
	// matches BY NAME, so a title that carried a dirty marker would hash to a
	// different id than the dock layout's entry and the window would silently
	// float. The dirty flag and the external-modification state are shown in the
	// TOOLBAR for exactly that reason.
	const ImGuiWindowFlags uFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (!ImGui::Begin(szEDITOR_WINDOW_ANIMATION_EDITOR, &m_bShow, uFlags))
	{
		// Collapsed, or an unselected dock tab — the panel is shown but nothing of
		// it is on screen, and the preview pane below is never reached. Same rule
		// and same idempotent call as the !m_bShow return above: a preview that
		// costs a per-view pass chain per frame must not outlive being looked at.
		m_xSession.DeactivatePreviewView();
		ImGui::End();
		return;
	}

	// One file read on the focus TRANSITION, not per frame — the document's own
	// header says HasExternalModification costs a read and asks for exactly this.
	const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bFocused && !m_bWasFocused)
	{
		RefreshExternalModificationState();
	}
	m_bWasFocused = bFocused;

	RenderToolbar();
	RenderEventToolbar();
	// WU-4.3's line — Set Key / Auto-key / Angle snap. Drawn in
	// Zenith_EditorPanel_Animation_Pose.cpp, beside the manipulator it drives.
	RenderPoseToolbar();
	RenderBanners();
	RenderPreviewPane();
	RenderEventInspector();
	// WU-7.1's "Bone Masks" section. Drawn ABOVE the separator with the other
	// cursor-laid-out items and never below it: everything past that line is the
	// draw-list sheet, and an ImGui item placed inside that region trips
	// ErrorCheckUsingSetCursorPosToExtendParentBoundaries (a modal CRT dialog
	// nothing logs — see the header).
	RenderMaskSection();
	ImGui::Separator();
	RenderSheet();
	// AFTER the sheet: HandleSheetInput runs at the end of RenderSheet and is
	// what raises the request, so opening the popup here catches it in the same
	// frame the right-click happened rather than one late.
	RenderEventContextMenu();

	// Shortcuts are scoped to THIS window's focus, the way the editor scopes its
	// entity keys to the viewport and the hierarchy: a Delete pressed in the
	// console must never remove a keyframe.
	if (bFocused)
	{
		HandleSheetKeyboard();
	}

	++m_uRenderedFrames;
	ImGui::End();
}

//=============================================================================
// Toolbar
//=============================================================================

void Zenith_EditorPanel_Animation::RenderToolbar()
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(320.0f));
	const bool bPathCommitted = ImGui::InputText("##AnimClipPath", m_acPathBuffer, sizeof(m_acPathBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);

	// Drop a .zanim straight onto the path field. The content browser already
	// emits this payload for animation assets, so nothing new is authored here.
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMATION);
		if (pxPayload == nullptr)
		{
			pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_FILE_GENERIC);
		}
		if (pxPayload != nullptr && pxPayload->Data != nullptr)
		{
			const DragDropFilePayload* pxFile = static_cast<const DragDropFilePayload*>(pxPayload->Data);
			snprintf(m_acPathBuffer, sizeof(m_acPathBuffer), "%s", pxFile->m_szFilePath);
			OpenClip(std::string(m_acPathBuffer));
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();
	if (ImGui::Button("Open") || bPathCommitted)
	{
		OpenClip(std::string(m_acPathBuffer));
	}

	ImGui::SameLine();
	if (ImGui::Button("Close"))
	{
		RequestCloseClip();
	}

	// ★ THE BONE-MASK SECTION'S TOGGLE, ON THIS ROW AND ABOVE THE no-clip EARLY
	// RETURN BELOW. On this row because the section itself now draws NOTHING when
	// it is off (see RenderMaskSection) and would otherwise be unreachable — and a
	// toggle needs no line of its own, which is the whole point of the rule it is
	// serving. Above the early return because a mask can legitimately be opened
	// with no clip loaded; the section will simply show its no-rig prompt.
	ImGui::SameLine();
	bool bShowMasks = m_bShowMaskSection;
	if (ImGui::Checkbox("Masks", &bShowMasks))
	{
		m_bShowMaskSection = bShowMasks;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Show the Bone Masks section (.zanimmask authoring). Off by default: while it is off it "
			"draws no items and takes no height, so the dope sheet below keeps the full window.");
	}

	// ★ WU-8.2's "Curves" TOGGLE AND ITS FOUR COMPANIONS, ON THIS SAME EXISTING
	// ROW. The curve view replaces the sheet's ROWS rather than sitting above
	// them, so it costs the canvas nothing — and its controls must not either,
	// which is why they are here and not on a line of their own. Drawn in
	// Zenith_EditorPanel_Animation_Curve.cpp, beside the view they drive.
	RenderCurveToolbarItems();

	if (!m_xDocument.IsOpen())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(no clip open)");
		return;
	}

	// ---- playback -----------------------------------------------------------
	// ToolbarSeparator does its OWN SameLine at both ends — wrapping it in one
	// more would double the gap and, on the trailing side, break the row.
	Zenith_EditorUI::ToolbarSeparator();

	const bool bPlaying = m_xSession.IsPlaying();
	Zenith_EditorIconButtonOptions xIconOptions;
	xIconOptions.m_bSelected = bPlaying;
	if (Zenith_EditorUI::IconButton("##AnimPlayPause",
		bPlaying ? Zenith_EditorIcon::Pause : Zenith_EditorIcon::Play,
		bPlaying ? "Pause the preview" : "Play the preview", xIconOptions))
	{
		if (bPlaying) { m_xSession.Pause(); } else { m_xSession.Play(); }
	}

	const float fTime = m_xSession.IsOpen() ? m_xSession.GetTime() : 0.0f;
	const float fDuration = m_xDocument.GetDuration();
	const u_int uFrameRate = GetFrameRate();

	ImGui::SameLine();
	ImGui::Text("%.3f / %.3f s", fTime, fDuration);

	ImGui::SameLine();
	if (uFrameRate > 0u)
	{
		ImGui::TextDisabled("frame %u / %u @ %u fps",
			Zenith_AnimTimelineTimeToFrame(fTime, uFrameRate),
			Zenith_AnimTimelineTimeToFrame(fDuration, uFrameRate),
			uFrameRate);
	}
	else
	{
		ImGui::TextDisabled("(no frame grid)");
	}

	Zenith_EditorUI::ToolbarSeparator();
	if (ImGui::Button("Zoom To Fit"))
	{
		Zenith_AnimTimelineFrameAll(m_xView, fDuration);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%.0f px/s", Zenith_AnimTimelineEffectivePixelsPerSecond(m_xView));

	// ---- read-only state --------------------------------------------------
	if (m_xDocument.IsDirty())
	{
		ImGui::SameLine();
		Zenith_EditorUI::Badge("UNSAVED", xPalette.m_uWarning, xPalette.m_uTextBright);
	}
	if (m_bExternalConflict)
	{
		ImGui::SameLine();
		Zenith_EditorUI::Badge("CHANGED ON DISK", xPalette.m_uError, xPalette.m_uTextBright);
	}
}

//=============================================================================
// The EVENT toolbar (WU-5B) — its own line, and that is not a style choice.
// The first toolbar row already runs wider than a 900 px window; a SameLine
// past the right edge produces a control nobody can click and nothing warns.
//=============================================================================

void Zenith_EditorPanel_Animation::RenderEventToolbar()
{
	if (!m_xDocument.IsOpen())
	{
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const float fDuration = m_xDocument.GetDuration();
	const float fPlayheadSeconds = m_xSession.IsOpen() ? m_xSession.GetTime() : 0.0f;
	const float fPlayheadNormalized = fDuration > 0.0f ? fPlayheadSeconds / fDuration : 0.0f;

	if (ImGui::Button("Add Event"))
	{
		// ★ AT THE PLAYHEAD, which is the one time the user can SEE. A keyboard or
		// toolbar gesture has no cursor position of its own, so anything else would
		// place the event somewhere the author did not choose. D4: the stored value
		// is the FRACTION, so the playhead's seconds are divided by the duration
		// here and never the other way round.
		Action_AddEvent(fPlayheadNormalized, DefaultEventName());
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Add an animation event at the play head (double-click the Events row to place one anywhere)");
	}

	ImGui::SameLine();
	bool bEmitOnScrub = GetEmitEventsOnScrub();
	if (ImGui::Checkbox("Emit events while scrubbing", &bEmitOnScrub))
	{
		Action_SetEmitEventsOnScrub(bEmitOnScrub);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Off by default (D40): a seek moves the event bookkeeping mark WITHOUT firing what the play head skipped, so dragging across a clip does not replay every footstep in it.");
	}

	// ---- what actually fired ------------------------------------------------
	ImGui::SameLine();
	if (m_astrEmittedEvents.GetSize() == 0u)
	{
		ImGui::TextDisabled("| no events emitted yet");
		return;
	}

	ImGui::TextDisabled("| fired (%u):", m_uEmittedEventTotal);
	for (u_int u = 0; u < m_astrEmittedEvents.GetSize(); ++u)
	{
		std::string strName;
		if (!GetEmittedEventNameAt(u, strName))
		{
			continue;
		}
		ImGui::SameLine();
		// The most recent one is bright and the rest fade back, so a glance at the
		// strip during playback answers "what just fired" rather than "what has
		// fired at some point".
		ImGui::PushStyleColor(ImGuiCol_Text,
			ImGui::ColorConvertU32ToFloat4(u == 0u ? xPalette.m_uTextBright : xPalette.m_uTextDim));
		ImGui::TextUnformatted(strName.c_str());
		ImGui::PopStyleColor();
	}
}

//=============================================================================
// The BONE MASK sub-panel (WU-7.1).
//
// ★ A COLLAPSIBLE SECTION INSIDE THE DOPE SHEET, NOT A WINDOW OF ITS OWN,
// because the RIG is here. A mask is authored against a specific skeleton's bone
// NAMES, and the only place the editor already holds a resolved, previewed rig
// is this panel's Zenith_AnimationPreviewSession — with its metadata resolution,
// its remembered per-clip override (D31) and its prompt. A standalone window
// would have to grow all three.
//
// ★ IT IS ORDINARY ImGui ITEMS, NOT DRAW-LIST DECORATIONS, and that is the
// opposite of the sheet below it. The sheet is one InvisibleButton painted at
// absolute coordinates because placing items there trips
// ErrorCheckUsingSetCursorPosToExtendParentBoundaries; this section is laid out
// by the cursor like the toolbars, so sliders and checkboxes are exactly what it
// should be made of. It records no rects.
//
// ★ EVERY GESTURE ENDS IN AN Action_Mask* CALL and this function decides
// nothing. What it DOES own is the three diagnostics — was the section drawn,
// was the ASSIGNMENT control drawn, and what is it explaining — because those
// are facts about a frame and only the drawing knows them.
//
// ★★ NOTHING SHOWN DRAWS NOTHING — NOT EVEN A COLLAPSED HEADER. This is the
// panel's standing rule (Editor/CLAUDE.md), the same one RenderPoseToolbar and
// RenderEventInspector follow, and it is load-bearing rather than tidy:
// RenderSheet takes `ImGui::GetContentRegionAvail()`, so EVERY item emitted
// above it comes straight out of the sheet's height, and the EVENTS ROW IS THE
// LAST ROW OF THE SHEET. This section shipped as a collapsed CollapsingHeader —
// one row, always present, ~24 px — and that alone pushed the events row below
// the canvas bottom in the 900x600 test window:
// `AnimPanel::ChangingTheDurationMovesTheEventRowAndNotTheStoredValue` went red
// on all three Null_ exes with GetEventRect false BEFORE and AFTER the duration
// change, which reads as "the event row is broken" and was in fact "the sheet
// lost 24 px". A collapsed header is not free, and the off-screen gate turns
// the cost into a flat `false` a long way from its cause.
//
// So the gate is FIRST and returns before a single item is submitted. The
// toggle lives on the existing toolbar row (RenderToolbar), which costs no
// height at all.
//=============================================================================

void Zenith_EditorPanel_Animation::RenderMaskSection()
{
	// ★ ZERO ITEMS, ZERO HEIGHT. See the block above.
	//
	// The `|| IsOpen()` is a SAFETY NET, not a second mode: every panel route
	// into the document (Action_MaskOpen / Action_MaskOpenFresh) raises the flag
	// and Action_MaskClose clears it, so in any reachable state the two agree and
	// the toolbar checkbox never disagrees with what is on screen. What it buys
	// is that a mask opened by some future path that forgot the flag can never be
	// INVISIBLE while it is open — which for a document that can be dirty is the
	// failure worth spending a branch on.
	if (!m_bShowMaskSection && !m_xMaskDocument.IsOpen())
	{
		return;
	}
	m_bMaskSectionDrawn = true;

	ImGui::Separator();
	ImGui::TextUnformatted("Bone Masks");

	// ---- the path field + lifecycle buttons ---------------------------------
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(320.0f));
	const bool bPathCommitted = ImGui::InputText("##AnimMaskPath", m_acMaskPathBuffer, sizeof(m_acMaskPathBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);

	ImGui::SameLine();
	if (ImGui::Button("Open##Mask") || bPathCommitted)
	{
		Action_MaskOpen(std::string(m_acMaskPathBuffer));
	}
	ImGui::SameLine();
	if (ImGui::Button("New##Mask"))
	{
		Action_MaskOpenFresh(std::string(m_acMaskPathBuffer));
	}

	if (!m_xMaskDocument.IsOpen())
	{
		// Named rather than left blank, because "nothing is drawn" has four causes
		// on this panel and a test that can only see a bool reports the wrong one.
		m_strMaskNotice = "no " ZENITH_ANIMMASK_EXT " open — type a path and press Open, or New";
		ImGui::TextDisabled("%s", m_strMaskNotice.c_str());
		return;
	}

	ImGui::SameLine();
	if (ImGui::Button("Save##Mask"))
	{
		Action_MaskSave();
	}
	ImGui::SameLine();
	if (ImGui::Button("Close##Mask"))
	{
		Action_MaskClose();
		return;
	}
	if (m_xMaskDocument.IsDirty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("| UNSAVED");
	}

	// ---- D47's explicit flag -------------------------------------------------
	bool bHasAvatarMask = m_xMaskDocument.HasAvatarMask();
	if (ImGui::Checkbox("Has avatar mask", &bHasAvatarMask))
	{
		Action_MaskSetHasAvatar(bHasAvatarMask);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Written to the file rather than derived from the weights (D47). An ALL-ZERO mask is a "
			"meaningful mask — 'this layer overrides nothing yet' — and a layer that came back unmasked would "
			"override the WHOLE skeleton instead.");
	}

	// ---- the assignment control, and the additive-layer refusal --------------
	//
	// ★ THE RULE IS ASKED, NOT RE-DERIVED. Zenith_BoneMaskDocument::LayerAcceptsMask
	// is the one place "does this blend mode use a mask" is answered, so WU-7.2's
	// layer list and this section cannot disagree.
	if (Zenith_BoneMaskDocument::LayerAcceptsMask(m_eMaskTargetBlendMode))
	{
		m_bMaskAssignmentDrawn = true;
		ImGui::SameLine();
		// The assignment itself belongs to WU-7.2, which owns the layer list; what
		// is offered here is the control and the state it depends on.
		ImGui::TextDisabled("| target layer: Override");
	}
	else
	{
		// ★ NOT GREYED OUT AND NOT ABSENT — REFUSED, WITH THE REASON. An additive
		// layer never reaches MaskedBlend at all, so a user who authored a whole
		// mask and assigned it here would see literally no change and have nothing
		// to grep for.
		m_bMaskAssignmentDrawn = false;
		m_strMaskNotice = Zenith_BoneMaskDocument::AdditiveLayerMaskNotice();
		ImGui::TextDisabled("%s", m_strMaskNotice.c_str());
	}

	// ---- the per-bone list ---------------------------------------------------
	Zenith_Vector<std::string> axBoneNames;
	GetMaskRigBoneNames(axBoneNames);
	if (axBoneNames.GetSize() == 0u)
	{
		// ★ A PROMPT, NOT AN EMPTY LIST. A mask names bones, and without a rig
		// there is nothing to name — offering a blank list would read as "this rig
		// has no bones" rather than as "there is no rig".
		static const char* szNO_RIG = "no rig previewed — open a clip whose skeleton resolves, or pick one above";
		// ★ IT DOES NOT OVERWRITE THE ADDITIVE-LAYER NOTICE. Both conditions can
		// hold at once, and only one of them is about a CONTROL THAT WAS NOT DRAWN;
		// a diagnostic that reported "no rig" for an additive target would send a
		// reader looking for a skeleton when the answer is the blend mode.
		if (m_strMaskNotice.empty())
		{
			m_strMaskNotice = szNO_RIG;
		}
		ImGui::TextDisabled("%s", szNO_RIG);
		return;
	}

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(90.0f));
	ImGui::SliderFloat("Subtree value", &m_fMaskSubtreeWeight, 0.0f, 1.0f, "%.2f");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("What a row's 'Subtree' button sets that bone AND every descendant of it to, as ONE undo step.");
	}

	for (u_int u = 0; u < axBoneNames.GetSize(); ++u)
	{
		const std::string& strBoneName = axBoneNames.Get(u);
		ImGui::PushID(static_cast<int>(u));

		float fWeight = m_xMaskDocument.GetBoneWeight(strBoneName);
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(180.0f));
		ImGui::SliderFloat(strBoneName.c_str(), &fWeight, 0.0f, 1.0f, "%.2f");
		// ★ ON EDIT-COMPLETE, NOT PER FRAME OF THE DRAG. A slider reports a new
		// value every frame it is held; writing each one would push one undo command
		// per frame and make Ctrl+Z crawl back through positions the user was only
		// passing through. This is the same "preview, then commit" shape the key
		// drag, the duration handle and the event drag all use.
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			Action_MaskSetWeight(strBoneName, fWeight);
		}

		ImGui::SameLine();
		if (ImGui::Button("Subtree"))
		{
			Action_MaskSetSubtree(strBoneName, m_fMaskSubtreeWeight);
		}

		++m_uMaskBoneRowsDrawn;
		ImGui::PopID();
	}
}

//=============================================================================
// The EVENT inspector strip — name and Vector4 payload for the selected event.
//
// ★ ONE COMMAND PER COMPLETED EDIT, NOT PER KEYSTROKE. Both fields commit on
// IsItemDeactivatedAfterEdit, so typing "FootstepLeft" is one undo step and not
// twelve — and a field that was focused and left alone commits nothing at all,
// because both actions refuse a value the event already has.
//=============================================================================

void Zenith_EditorPanel_Animation::SyncEventInspectorBuffers()
{
	const u_int uEventId = GetInspectorEventId();
	if (uEventId == m_uInspectorBufferEventId && m_bEventInspectorEditing)
	{
		// ★ NEVER RE-READ WHILE A FIELD IS BEING TYPED INTO. The buffers hold what
		// the user has entered but not yet committed; re-filling them from the
		// document would undo each keystroke before the edit-complete that would
		// have made it real. Every other frame DOES re-read, which is what makes
		// an undo of a rename visible in the box.
		return;
	}

	m_uInspectorBufferEventId = uEventId;
	m_acEventNameBuffer[0] = '\0';
	m_afEventPayloadBuffer[0] = 0.0f;
	m_afEventPayloadBuffer[1] = 0.0f;
	m_afEventPayloadBuffer[2] = 0.0f;
	m_afEventPayloadBuffer[3] = 0.0f;

	Flux_AnimationEvent xEvent;
	if (uEventId == uINVALID_ANIM_KEY_ID || !m_xDocument.GetEvent(uEventId, xEvent))
	{
		return;
	}
	snprintf(m_acEventNameBuffer, sizeof(m_acEventNameBuffer), "%s", xEvent.m_strEventName.c_str());
	m_afEventPayloadBuffer[0] = xEvent.m_xData.x;
	m_afEventPayloadBuffer[1] = xEvent.m_xData.y;
	m_afEventPayloadBuffer[2] = xEvent.m_xData.z;
	m_afEventPayloadBuffer[3] = xEvent.m_xData.w;
}

void Zenith_EditorPanel_Animation::RenderEventInspector()
{
	SyncEventInspectorBuffers();

	const u_int uEventId = m_uInspectorBufferEventId;
	Flux_AnimationEvent xEvent;
	if (!m_xDocument.IsOpen() || uEventId == uINVALID_ANIM_KEY_ID || !m_xDocument.GetEvent(uEventId, xEvent))
	{
		// Nothing selected draws NOTHING, not a disabled strip: the sheet below is
		// sized from the remaining space, and a permanent strip would cost two rows
		// of dope sheet on every frame nobody is editing an event.
		m_bEventInspectorEditing = false;
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const float fDuration = m_xDocument.GetDuration();

	ImGui::Separator();

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(220.0f));
	ImGui::InputText("Event", m_acEventNameBuffer, sizeof(m_acEventNameBuffer));
	const bool bNameActive = ImGui::IsItemActive();
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Action_RenameEvent(uEventId, std::string(m_acEventNameBuffer));
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(320.0f));
	ImGui::InputFloat4("Payload", m_afEventPayloadBuffer);
	const bool bPayloadActive = ImGui::IsItemActive();
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Action_SetEventPayload(uEventId, Zenith_Maths::Vector4(
			m_afEventPayloadBuffer[0], m_afEventPayloadBuffer[1],
			m_afEventPayloadBuffer[2], m_afEventPayloadBuffer[3]));
	}

	m_bEventInspectorEditing = bNameActive || bPayloadActive;

	ImGui::SameLine();
	// BOTH numbers, because the stored one is the fraction (D4) and the useful one
	// is the second — showing only the fraction makes every event look like it is
	// at "0.25" of nothing in particular.
	ImGui::TextDisabled("t = %.4f  (%.3f s)", xEvent.m_fNormalizedTime, xEvent.m_fNormalizedTime * fDuration);

	if (xEvent.m_fNormalizedTime > 1.0f + fANIM_TIME_EPSILON)
	{
		ImGui::SameLine();
		Zenith_EditorUI::Badge("PAST END", xPalette.m_uWarning, xPalette.m_uTextBright);
	}
}

void Zenith_EditorPanel_Animation::RenderEventContextMenu()
{
	static const char* const szEVENT_CONTEXT_POPUP = "##AnimEventContext";

	if (m_bEventContextMenuRequested)
	{
		m_bEventContextMenuRequested = false;
		ImGui::OpenPopup(szEVENT_CONTEXT_POPUP);
	}

	if (!ImGui::BeginPopup(szEVENT_CONTEXT_POPUP))
	{
		return;
	}

	const u_int uSelected = m_auSelectedEventIds.GetSize();
	if (ImGui::MenuItem(uSelected > 1u ? "Delete Events" : "Delete Event"))
	{
		// The same action the Delete key runs: it removes every selected KEY and
		// every selected EVENT as one undo step, and leaves the selection alone so
		// the undo hands it back with them.
		Action_DeleteSelection();
	}
	ImGui::EndPopup();
}

//=============================================================================
// Banners — every refusal and every warning the panel has to SAY OUT LOUD.
//=============================================================================

void Zenith_EditorPanel_Animation::RenderBanners()
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();

	// D21: a GENERATED clip is rewritten in full on every tools boot, so an
	// in-place edit would be silently thrown away by the next run. The document
	// refuses to open it and this is the offer that makes the refusal actionable.
	if (m_eLastOpenResult == ZENITH_ANIMDOC_OPEN_REFUSED_GENERATED)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning));
		ImGui::TextWrapped("'%s' is a GENERATED clip: the asset bake rewrites it on every tools boot, so an edit here would not survive the next run.",
			m_strLastOpenAttemptPath.c_str());
		ImGui::PopStyleColor();
		if (ImGui::Button("Promote to authored override"))
		{
			PromoteAndOpenAuthoredOverride(m_strLastOpenAttemptPath);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("copies it under Authored/ and opens THAT; the bake keeps owning the source");
		ImGui::Separator();
	}
	else if (m_eLastOpenResult == ZENITH_ANIMDOC_OPEN_REFUSED_DIRTY)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning));
		ImGui::TextWrapped("This document has unsaved edits. Save or discard them before opening '%s'.",
			m_strLastOpenAttemptPath.c_str());
		ImGui::PopStyleColor();
		ImGui::Separator();
	}
	else if (m_eLastOpenResult == ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET && !m_strLastOpenAttemptPath.empty() && !m_xDocument.IsOpen())
	{
		ImGui::TextDisabled("'%s' did not resolve to a loaded animation asset.", m_strLastOpenAttemptPath.c_str());
		ImGui::Separator();
	}

	if (m_bCloseRefusedDirty && m_xDocument.IsOpen())
	{
		ImGui::TextWrapped("Unsaved edits.");
		ImGui::SameLine();
		if (ImGui::Button("Save##AnimCloseSave"))
		{
			Action_Save();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard##AnimCloseDiscard"))
		{
			CloseClip();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##AnimCloseCancel"))
		{
			m_bCloseRefusedDirty = false;
		}
		ImGui::Separator();
	}

	if (!m_xDocument.IsOpen())
	{
		return;
	}

	if (m_bExternalConflict)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uError));
		ImGui::TextWrapped("The file on disk has changed since this document was opened. A save will be refused until it is overwritten deliberately.");
		ImGui::PopStyleColor();
	}

	// ★ D13. Shrinking a duration does not move a key, so a clip can legally hold
	// keys nothing will ever sample. Silence here is the failure mode: the keys
	// are still in the file, still round-trip, and simply stop having any effect.
	if (m_uKeysPastDuration > 0u || m_uEventsPastDuration > 0u)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning));
		ImGui::TextWrapped("! %u key(s) and %u event(s) sit past the clip's duration of %.3f s and will never be sampled.",
			m_uKeysPastDuration, m_uEventsPastDuration, m_xDocument.GetDuration());
		ImGui::PopStyleColor();
	}
}

//=============================================================================
// Preview pane
//=============================================================================

void Zenith_EditorPanel_Animation::RenderPreviewPane()
{
	if (!m_xSession.IsOpen())
	{
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const float fPreviewSize = Zenith_EditorUI::Px(fSHEET_PREVIEW_SIZE_1X);

	// ★ NO DISPOSSESSED STATE EXISTS ANY MORE (R5). This pane used to open with a
	// placeholder naming whoever held the one shared preview view, plus a Reclaim
	// button. The animation editor has its OWN view slot now
	// (kuFluxViewSlotPreviewAnim) with its own persistent LDR, and nothing else
	// stages it — so there is no owner to name and nothing to reclaim, and a
	// button for a state that cannot occur is worse than no button.

	// ---- no rig (D31) -------------------------------------------------------
	if (m_xSession.NeedsRigSelection())
	{
		static const char* const aszStatus[] =
		{
			"ok",
			"the session is not open",
			"the clip records no skeleton path",
			"the clip's skeleton path does not resolve",
			"the clip records no preview model path",
			"the clip's preview model path does not resolve",
		};
		const u_int uStatus = static_cast<u_int>(m_xSession.GetRigStatus());
		const u_int uStatusCount = static_cast<u_int>(IM_ARRAYSIZE(aszStatus));
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning));
		ImGui::TextWrapped("No rig for this clip: %s. Pick one to preview it.",
			uStatus < uStatusCount ? aszStatus[uStatus] : "unknown");
		ImGui::PopStyleColor();

		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(300.0f));
		ImGui::InputText("Skeleton", m_acSkeletonBuffer, sizeof(m_acSkeletonBuffer));
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(300.0f));
		ImGui::InputText("Preview mesh", m_acPreviewModelBuffer, sizeof(m_acPreviewModelBuffer));
		if (ImGui::Button("Use this rig"))
		{
			// A human's answer outranks the metadata field they already found
			// wanting, and the session remembers it per clip.
			m_xSession.SetRigOverride(std::string(m_acSkeletonBuffer), std::string(m_acPreviewModelBuffer));
		}
		return;
	}

	// ---- live image ---------------------------------------------------------
	m_xSession.UpdatePreviewView();

	// ★ THE ACCESSOR IS THE TRY- ONE, and that is not belt and braces. The pane is
	// now drawn on a rig that resolves headlessly (the overlay and the pick ray
	// need the pane's geometry, not a texture), so this line is reached in runs
	// that have no Flux at all — a boot-time unit batch among them. Registration
	// is left unattempted rather than recorded as done, so a later frame with a
	// live renderer still picks the image up.
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (!m_bPreviewImageRegistered && pxGraphics != nullptr)
	{
		// The persistent preview LDR the per-view tonemap writes — NOT a
		// transient, so the registration stays valid across graph rebuilds. On a
		// backend with no device the SRV is invalid and RegisterTexture hands back
		// an invalid handle, which is why the draw below is gated rather than
		// asserted.
		const Flux_ImGuiTextureHandle xHandle = Flux_ImGuiIntegration::RegisterTexture(
			// THE ANIMATION PREVIEW'S OWN LDR — built at Flux_Graphics::Initialise
			// for this slot whether or not the view is active, which is what lets
			// the registration happen on the first frame the pane is drawn.
			pxGraphics->GetPreviewLDR(kuFluxViewSlotPreviewAnim).SRV(), pxGraphics->m_xClampSampler);
		m_ulPreviewImageHandle = xHandle.AsUInt64();
		m_bPreviewImageRegistered = true;
	}

	// ★ THE SAME RECTANGLE IS OCCUPIED WHETHER OR NOT THERE IS AN IMAGE TO PUT IN
	// IT. Bone picking and the bone overlay are pure CPU maths over the session's
	// orbit camera — they need the pane's geometry, not a texture — and on a
	// backend with no device the registration above hands back an invalid handle.
	// Drawing nothing there would make every unit that exercises picking
	// requiresGraphics, i.e. skipped-as-passed headless, i.e. rotting.
	if (m_ulPreviewImageHandle != 0u)
	{
		Flux_ImGuiTextureHandle xHandle;
		xHandle.SetValue(m_ulPreviewImageHandle);
		ImGui::Image((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xHandle),
			Vec(fPreviewSize, fPreviewSize));
	}
	else
	{
		ImGui::Dummy(Vec(fPreviewSize, fPreviewSize));
		ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
			xPalette.m_uTextDim);
	}

	const ImVec2 xImageMin = ImGui::GetItemRectMin();
	const ImVec2 xImageMax = ImGui::GetItemRectMax();
	m_xPreviewImageRect.m_fMinX = xImageMin.x;
	m_xPreviewImageRect.m_fMinY = xImageMin.y;
	m_xPreviewImageRect.m_fMaxX = xImageMax.x;
	m_xPreviewImageRect.m_fMaxY = xImageMax.y;
	m_bPreviewImageRectValid = true;

	// ★ THE MANIPULATORS GET THE GESTURE FIRST, and only what BOTH decline
	// reaches the pick / orbit handler. A press on a rotation ring (or on the IK
	// target) that also re-selected whatever bone the ray passed through and
	// started an orbit is the classic "the gizmo moves the camera" bug, and
	// three handlers cannot all own one mouse-down.
	//
	// ★ THE RINGS ARE ASKED BEFORE THE IK HANDLE, and the order costs the handle
	// nothing: the handle opens at the ring set's own CENTRE, and a ring is only
	// grabbable within fANIM_POSE_RING_GRAB_PIXELS of its projected POLYLINE —
	// which is a ring radius away from that centre. The two claim disjoint
	// pixels until the target has been dragged off the joint, and after that the
	// handle is nowhere near a ring at all.
	// (WU-4.3 Zenith_EditorPanel_Animation_Pose.cpp; E1 below and _IK.cpp.)
	const bool bPreviewImageHovered = ImGui::IsItemHovered();
	if (!HandlePoseManipulatorInput(bPreviewImageHovered) && !HandleIKTargetInput(bPreviewImageHovered))
	{
		HandlePreviewPaneInput(bPreviewImageHovered);
	}
	DrawBoneOverlay(ImGui::GetWindowDrawList());
	// Over the bone lines, so a ring is never hidden by the segment it turns.
	DrawPoseManipulator(ImGui::GetWindowDrawList());
	// And the target over both: it is the thing being aimed, and a handle behind
	// a ring is a handle nobody can see they have grabbed.
	DrawIKTargetHandle(ImGui::GetWindowDrawList());

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextDisabled("Rig: %s", m_xSession.GetSkeletonPath().c_str());
	ImGui::TextDisabled("Mesh: %s%s", m_xSession.GetPreviewModelPath().c_str(),
		m_xSession.IsPreviewMeshBareMeshAsset() ? "  (bare mesh)" : "");
	ImGui::TextDisabled("(drag = orbit, wheel = zoom, click = select bone)");
	if (m_ulPreviewImageHandle == 0u)
	{
		ImGui::TextDisabled("(no preview image on this backend)");
	}
	if (m_xSession.HasBoneSelection())
	{
		ImGui::Text("Bone %u selected", m_xSession.GetSelectedBoneIndex());
	}
	if (m_xSession.HasUnkeyedPose())
	{
		// ★ THE ONE THING A USER CAN SILENTLY LOSE. With auto-key off, a released
		// drag writes no key and no undo entry, and the next seek re-evaluates from
		// the clip and destroys it. Saying so is the whole of §4.4's requirement.
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning));
		ImGui::TextWrapped("UNKEYED POSE — seeking discards it. Set Key to keep it.");
		ImGui::PopStyleColor();
	}
	ImGui::EndGroup();
}

//=============================================================================
// Preview pane input + the bone overlay.
//
// ★ THE ONLY PLACE AN ABSOLUTE MOUSE POSITION BECOMES A PREVIEW PIXEL. Every
// Action_* below takes the image-relative coordinate, so nothing else has to
// know where the pane landed — which is also what lets a unit aim at a joint it
// projected itself, with no frame geometry in its own arithmetic.
//=============================================================================

void Zenith_EditorPanel_Animation::HandlePreviewPaneInput(bool bImageHovered)
{
	if (!bImageHovered)
	{
		// Hover is a per-frame paint hint; a cursor that left the image must not
		// leave a bone lit behind it.
		m_xSession.SetHoveredBoneIndex(kuINVALID_BONE_SELECTION);
		return;
	}

	const ImGuiIO& xIO = ImGui::GetIO();
	if (xIO.MouseWheel != 0.0f)
	{
		m_xSession.ZoomCamera(xIO.MouseWheel * 0.2f);
	}

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
	{
		// An orbit is a camera gesture, not a pick: hit-testing mid-drag would
		// light a different bone on every frame of it.
		m_xSession.OrbitCamera(-xIO.MouseDelta.x * 0.01f, -xIO.MouseDelta.y * 0.01f);
		m_xSession.SetHoveredBoneIndex(kuINVALID_BONE_SELECTION);
		return;
	}

	const float fLocalX = xIO.MousePos.x - m_xPreviewImageRect.m_fMinX;
	const float fLocalY = xIO.MousePos.y - m_xPreviewImageRect.m_fMinY;

	Zenith_Maths::Vector3 xOrigin(0.0f);
	Zenith_Maths::Vector3 xDir(0.0f);
	u_int uHovered = kuINVALID_BONE_SELECTION;
	if (BuildPreviewRay(fLocalX, fLocalY, xOrigin, xDir))
	{
		if (!m_xSession.PickBone(xOrigin, xDir, uHovered))
		{
			uHovered = kuINVALID_BONE_SELECTION;
		}
	}
	m_xSession.SetHoveredBoneIndex(uHovered);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		// Straight into the action, adding nothing of its own — the handler
		// translates input, the action decides what happens. A miss changes
		// nothing (see Action_PickBoneAtPreviewPixel).
		Action_PickBoneAtPreviewPixel(fLocalX, fLocalY);
	}
}

void Zenith_EditorPanel_Animation::DrawBoneOverlay(ImDrawList* pxDraw)
{
	if (pxDraw == nullptr || !m_bPreviewImageRectValid)
	{
		return;
	}

	const u_int uSelected = m_xSession.GetSelectedBoneIndex();
	const u_int uHovered = m_xSession.GetHoveredBoneIndex();
	if (uSelected == kuINVALID_BONE_SELECTION && uHovered == kuINVALID_BONE_SELECTION)
	{
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const Zenith_BonePickSet& xSet = m_xSession.GetBonePickSet();

	// ★ DRAWN FROM THE PICK SET, NOT FROM A SECOND WALK OF THE SKELETON. What the
	// user sees highlighted is then, by construction, exactly what a click there
	// would select — a separate derivation is how a highlight ends up one bone
	// away from the thing it is advertising.
	pxDraw->PushClipRect(Vec(m_xPreviewImageRect.m_fMinX, m_xPreviewImageRect.m_fMinY),
		Vec(m_xPreviewImageRect.m_fMaxX, m_xPreviewImageRect.m_fMaxY), true);

	for (u_int u = 0; u < xSet.m_xShapes.GetSize(); ++u)
	{
		const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);
		const bool bIsSelected = (xShape.m_uBoneIndex == uSelected);
		const bool bIsHovered = (xShape.m_uBoneIndex == uHovered);
		if (!bIsSelected && !bIsHovered)
		{
			continue;
		}

		float fAx = 0.0f;
		float fAy = 0.0f;
		float fBx = 0.0f;
		float fBy = 0.0f;
		if (!ProjectPreviewWorldPoint(xShape.m_xA, fAx, fAy) ||
			!ProjectPreviewWorldPoint(xShape.m_xB, fBx, fBy))
		{
			// Behind the camera: there is no pixel, and drawing one anyway would
			// smear a line across the pane from a mirrored coordinate.
			continue;
		}

		const float fOriginX = m_xPreviewImageRect.m_fMinX;
		const float fOriginY = m_xPreviewImageRect.m_fMinY;
		const ImU32 uColour = bIsSelected ? xPalette.m_uAccent : xPalette.m_uTextDim;
		const float fThickness = Zenith_EditorUI::Px(bIsSelected ? 2.5f : 1.5f);

		if (!xShape.m_bIsJointOnly)
		{
			pxDraw->AddLine(Vec(fOriginX + fAx, fOriginY + fAy), Vec(fOriginX + fBx, fOriginY + fBy),
				uColour, fThickness);
		}
		// The circle marks the joint the bone's rotation PIVOTS about — its own
		// joint, which is m_xA for a capsule and the single point for a sphere.
		pxDraw->AddCircle(Vec(fOriginX + fAx, fOriginY + fAy),
			Zenith_EditorUI::Px(bIsSelected ? 5.0f : 3.5f), uColour, 0, fThickness);
	}

	pxDraw->PopClipRect();
}

//=============================================================================
// THE IK TARGET WIDGET (E1) — input translation and the handle.
//
// ★ THESE TWO FUNCTIONS ARE THE ONLY IK CODE THAT READS ImGui, and each branch
// ends in an Action_*. Everything they decide — where the handle IS, whether a
// pixel grabbed it, what a moved target does to the chain — lives in
// Zenith_EditorPanel_Animation_IK.cpp, which has no ImGui in it at all, so a
// unit performs the whole gesture through the verbs with no frame open.
//=============================================================================

bool Zenith_EditorPanel_Animation::HandleIKTargetInput(bool bImageHovered)
{
	if (!m_bPreviewImageRectValid)
	{
		m_bIKHandleHovered = false;
		return false;
	}

	const ImGuiIO& xIO = ImGui::GetIO();
	const float fLocalX = xIO.MousePos.x - m_xPreviewImageRect.m_fMinX;
	const float fLocalY = xIO.MousePos.y - m_xPreviewImageRect.m_fMinY;

	if (m_bIKDragActive)
	{
		// ★ ESCAPE IS TESTED BEFORE THE RELEASE, exactly as the ring drag tests
		// it: otherwise a cancel would be followed immediately by the mouse-up's
		// bake and the user would get the key they had just asked not to have.
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			Action_CancelIKDrag();
			return true;
		}
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			Action_UpdateIKDragToPixel(fLocalX, fLocalY);
		}
		else
		{
			Action_EndIKDrag();
		}
		// A live drag owns the pointer whether or not it is still over the image,
		// for the ring drag's reason: a cursor that wandered off the pane
		// mid-gesture must keep moving the target, not start orbiting the camera.
		return true;
	}

	if (!bImageHovered)
	{
		m_bIKHandleHovered = false;
		return false;
	}

	float fHandleX = 0.0f;
	float fHandleY = 0.0f;
	if (!GetIKHandlePixel(fHandleX, fHandleY))
	{
		// No selection, no seeded target, or the target is behind the camera —
		// there is nothing drawn to grab, so nothing is claimed.
		m_bIKHandleHovered = false;
		return false;
	}

	const float fDx = fLocalX - fHandleX;
	const float fDy = fLocalY - fHandleY;
	m_bIKHandleHovered = (std::sqrt(fDx * fDx + fDy * fDy) <= fANIM_POSE_RING_GRAB_PIXELS);
	if (!m_bIKHandleHovered)
	{
		return false;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		return Action_BeginIKDragAtPixel(fLocalX, fLocalY);
	}
	// Merely hovering claims nothing: the wheel still zooms and the pane still
	// lights the bone under the cursor.
	return false;
}

void Zenith_EditorPanel_Animation::DrawIKTargetHandle(ImDrawList* pxDraw)
{
	if (pxDraw == nullptr || !m_bPreviewImageRectValid)
	{
		return;
	}

	// ★ NOTHING IS DRAWN WITHOUT A BONE SELECTION, which is this panel's standing
	// rule and which GetIKHandlePixel answers for: with no effector the handle
	// would be a control that cannot act, and one drawn at the origin would
	// invite a drag that refuses.
	float fHandleX = 0.0f;
	float fHandleY = 0.0f;
	if (!GetIKHandlePixel(fHandleX, fHandleY))
	{
		return;
	}

	const float fOriginX = m_xPreviewImageRect.m_fMinX;
	const float fOriginY = m_xPreviewImageRect.m_fMinY;

	pxDraw->PushClipRect(Vec(m_xPreviewImageRect.m_fMinX, m_xPreviewImageRect.m_fMinY),
		Vec(m_xPreviewImageRect.m_fMaxX, m_xPreviewImageRect.m_fMaxY), true);

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const ImU32 uLIT_COLOUR = IM_COL32(255, 225, 120, 255);
	const bool bLit = m_bIKDragActive || m_bIKHandleHovered;
	const ImU32 uColour = bLit ? uLIT_COLOUR : xPalette.m_uAccent;

	// ★ A LEADER FROM THE EFFECTOR'S JOINT TO THE TARGET, drawn only while the
	// two are apart. It is the whole readout of an IK drag: how far the solver
	// is being asked to reach and in which direction. Both ends come from the
	// same projection the hit test uses, so what is painted and what can be
	// grabbed cannot drift.
	float fJointX = 0.0f;
	float fJointY = 0.0f;
	const Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance != nullptr && m_xSession.HasBoneSelection())
	{
		const Zenith_Maths::Vector3 xJoint = Zenith_BoneSpace::BoneWorldPosition(
			m_xSession.GetSessionModelMatrix(), *pxInstance, m_xSession.GetSelectedBoneIndex());
		if (ProjectPreviewWorldPoint(xJoint, fJointX, fJointY))
		{
			const float fLeaderDx = fHandleX - fJointX;
			const float fLeaderDy = fHandleY - fJointY;
			if ((fLeaderDx * fLeaderDx + fLeaderDy * fLeaderDy) > 1.0f)
			{
				pxDraw->AddLine(Vec(fOriginX + fJointX, fOriginY + fJointY),
					Vec(fOriginX + fHandleX, fOriginY + fHandleY),
					xPalette.m_uTextDim, Zenith_EditorUI::Px(1.0f));
			}
		}
	}

	const float fRadius = Zenith_EditorUI::Px(fANIM_IK_HANDLE_RADIUS_1X);
	pxDraw->AddCircleFilled(Vec(fOriginX + fHandleX, fOriginY + fHandleY), fRadius, uColour);
	pxDraw->AddCircle(Vec(fOriginX + fHandleX, fOriginY + fHandleY),
		fRadius + Zenith_EditorUI::Px(2.0f), uColour, 0, Zenith_EditorUI::Px(bLit ? 2.0f : 1.0f));

	pxDraw->PopClipRect();

	// The diagnostic is raised HERE and nowhere else, so
	// WasIKHandleDrawnLastFrame() answers "was it painted" rather than "would it
	// have been" — the distinction every other draw diagnostic on this panel
	// keeps, and the one a frame-level unit needs.
	m_bIKHandleDrawn = true;
}

//=============================================================================
// The sheet
//=============================================================================

float Zenith_EditorPanel_Animation::TotalRowsHeight(const SheetLayout& xLayout) const
{
	return static_cast<float>(m_axRows.GetSize()) * xLayout.m_fRowHeight;
}

float Zenith_EditorPanel_Animation::VisibleRowsHeight(const SheetLayout& xLayout) const
{
	const float fHeight = xLayout.m_fCanvasBottom - xLayout.m_fRowsTop;
	return fHeight > 0.0f ? fHeight : 0.0f;
}

void Zenith_EditorPanel_Animation::RenderSheet()
{
	const ImVec2 xOrigin = ImGui::GetCursorScreenPos();
	ImVec2 xAvail = ImGui::GetContentRegionAvail();
	if (xAvail.x < 1.0f) { xAvail.x = 1.0f; }
	if (xAvail.y < 1.0f) { xAvail.y = 1.0f; }

	// ONE item for the whole sheet. Everything after this is draw-list work.
	ImGui::InvisibleButton("##AnimSheetCanvas", xAvail,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	SheetLayout xLayout;
	xLayout.m_fCanvasLeft   = xOrigin.x;
	xLayout.m_fCanvasTop    = xOrigin.y;
	xLayout.m_fCanvasRight  = xOrigin.x + xAvail.x;
	xLayout.m_fCanvasBottom = xOrigin.y + xAvail.y;
	xLayout.m_fRulerHeight  = Zenith_EditorUI::Px(fSHEET_RULER_HEIGHT_1X);
	xLayout.m_fRowHeight    = Zenith_EditorUI::Px(fSHEET_ROW_HEIGHT_1X);
	xLayout.m_fLabelWidth   = Zenith_EditorUI::Px(fSHEET_LABEL_WIDTH_1X);
	if (xLayout.m_fLabelWidth > xAvail.x * 0.5f)
	{
		xLayout.m_fLabelWidth = xAvail.x * 0.5f;
	}
	xLayout.m_fTrackLeft = xLayout.m_fCanvasLeft + xLayout.m_fLabelWidth;
	xLayout.m_fTrackWidth = xAvail.x - xLayout.m_fLabelWidth;
	if (xLayout.m_fTrackWidth < 0.0f) { xLayout.m_fTrackWidth = 0.0f; }
	xLayout.m_fRowsTop = xLayout.m_fCanvasTop + xLayout.m_fRulerHeight;

	m_xCanvasRect.m_fMinX = xLayout.m_fCanvasLeft;
	m_xCanvasRect.m_fMinY = xLayout.m_fCanvasTop;
	m_xCanvasRect.m_fMaxX = xLayout.m_fCanvasRight;
	m_xCanvasRect.m_fMaxY = xLayout.m_fCanvasBottom;
	m_bCanvasRectValid = true;

	// ★ CAPTURE THE DISPLAY BOUND HERE, where the rects are about to be made, so
	// PublishRect judges each one against the frame it belongs to. Reading it at
	// query time instead is the defect described in PublishRect.
	const ImVec2 xDisplaySize = ImGui::GetIO().DisplaySize;
	m_fRecordedDisplayWidth = xDisplaySize.x;
	m_fRecordedDisplayHeight = xDisplaySize.y;
	m_fLastTrackWidth = xLayout.m_fTrackWidth;

	// The view's track rect is a PROPERTY OF THE WINDOW, so it is rewritten every
	// frame; the zoom and the scroll are the panel's own state and survive.
	m_xView.m_fTrackLeftPixel = xLayout.m_fTrackLeft;
	m_xView.m_fTrackWidthPixels = xLayout.m_fTrackWidth;
	const float fDuration = m_xDocument.IsOpen() ? m_xDocument.GetDuration() : 0.0f;
	Zenith_AnimTimelineClamp(m_xView, fDuration);

	HandleViewInput(xLayout, bCanvasHovered);
	ApplyPendingScrolls(xLayout);
	Zenith_AnimTimelineClamp(m_xView, fDuration);

	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	pxDraw->PushClipRect(Vec(xLayout.m_fCanvasLeft, xLayout.m_fCanvasTop),
		Vec(xLayout.m_fCanvasRight, xLayout.m_fCanvasBottom), true);

	DrawSheetBackground(pxDraw, xLayout);
	DrawRuler(pxDraw, xLayout);
	// ★ ONE OR THE OTHER, NEVER BOTH (WU-8.2). The curve view occupies exactly the
	// rectangle the rows would have — same canvas, same ruler above it, same X
	// mapping — so switching views costs the sheet no height and moves neither the
	// playhead nor the duration shade. It also means the dope-sheet row / key /
	// event rects are simply NOT RECORDED while the curve view is up, which is the
	// honest answer: those rows were not painted, and handing out a coordinate for
	// one would be a click into a row nobody can see.
	if (m_bShowCurveView)
	{
		DrawCurveView(pxDraw, xLayout);
	}
	else
	{
		DrawRows(pxDraw, xLayout, bCanvasHovered);
	}
	DrawPlayhead(pxDraw, xLayout);
	DrawSelectionOverlays(pxDraw, xLayout);

	pxDraw->PopClipRect();

	if (xLayout.m_fTrackWidth > 0.0f)
	{
		m_xRulerRect.m_fMinX = xLayout.m_fTrackLeft;
		m_xRulerRect.m_fMinY = xLayout.m_fCanvasTop;
		m_xRulerRect.m_fMaxX = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
		m_xRulerRect.m_fMaxY = xLayout.m_fRowsTop;
		m_bRulerRectValid = true;

		m_xTrackAreaRect.m_fMinX = xLayout.m_fTrackLeft;
		m_xTrackAreaRect.m_fMinY = xLayout.m_fCanvasTop;
		m_xTrackAreaRect.m_fMaxX = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
		m_xTrackAreaRect.m_fMaxY = xLayout.m_fCanvasBottom;
		m_bTrackAreaRectValid = true;
	}

	// ★ INPUT IS TRANSLATED LAST, AFTER THE RECTS EXIST. Every gesture below
	// hit-tests against what was painted THIS frame, through the same accessors
	// a caller uses — so a click can never land on a key the off-screen gate
	// would have refused to hand out a coordinate for. The cost is that a
	// mutation made here is not visible until the next frame's draw, which is
	// exactly the deferral the collapse toggle above already lives with.
	//
	// ★ THE CURVE VIEW GETS FIRST REFUSAL, AND ONLY WHEN IT CLAIMS THE GESTURE IS
	// THE SHEET HANDLER SKIPPED. A press on a tangent handle that also reached
	// HandleSheetInput would be read there as a click on empty space — i.e. as
	// "clear the selection" — because the row and key rect maps are empty in this
	// view. Everything the curve view does NOT claim (the ruler scrub, the
	// duration handle, the rubber band) still runs, and behaves identically,
	// because the X axis is the same one.
	if (m_bShowCurveView && HandleCurveInput(xLayout, bCanvasHovered))
	{
		return;
	}
	HandleSheetInput(xLayout, bCanvasHovered);
}

void Zenith_EditorPanel_Animation::HandleViewInput(const SheetLayout& xLayout, bool bCanvasHovered)
{
	if (!bCanvasHovered)
	{
		return;
	}
	const ImGuiIO& xIO = ImGui::GetIO();
	if (xIO.MouseWheel == 0.0f)
	{
		return;
	}

	if (xIO.KeyCtrl)
	{
		// Zoom about the cursor so the thing under it holds still — the mapping
		// owns that rule (and both clamps), the panel only supplies the pixel.
		Zenith_AnimTimelineZoomAroundPixel(m_xView, xIO.MousePos.x, 1.0f + xIO.MouseWheel * 0.15f);
		return;
	}

	const float fMaxScroll = TotalRowsHeight(xLayout) - VisibleRowsHeight(xLayout);
	m_fRowScrollPixels -= xIO.MouseWheel * xLayout.m_fRowHeight * 3.0f;
	if (m_fRowScrollPixels > fMaxScroll) { m_fRowScrollPixels = fMaxScroll; }
	if (m_fRowScrollPixels < 0.0f) { m_fRowScrollPixels = 0.0f; }
}

void Zenith_EditorPanel_Animation::ApplyPendingScrolls(const SheetLayout& xLayout)
{
	// FIRST, because a fit rewrites the zoom the scroll below is computed against.
	// This is the fit OnDocumentOpened could not perform: it runs before the panel
	// has been laid out, so FrameAll had no width and fell back to the default
	// zoom (see m_bPendingFrameAll).
	if (m_bPendingFrameAll && xLayout.m_fTrackWidth > 0.0f)
	{
		m_bPendingFrameAll = false;
		Zenith_AnimTimelineFrameAll(m_xView, m_xDocument.IsOpen() ? m_xDocument.GetDuration() : 0.0f);
	}

	if (m_bPendingTimeScroll)
	{
		m_bPendingTimeScroll = false;
		float fStart = 0.0f;
		float fEnd = 0.0f;
		Zenith_AnimTimelineVisibleRange(m_xView, fStart, fEnd);
		const float fSpan = fEnd - fStart;
		if (fSpan > 0.0f)
		{
			const float fMargin = fSpan * 0.1f;
			if (m_fPendingTimeScroll < fStart + fMargin)
			{
				m_xView.m_fScrollSeconds = m_fPendingTimeScroll - fMargin;
			}
			else if (m_fPendingTimeScroll > fEnd - fMargin)
			{
				m_xView.m_fScrollSeconds = m_fPendingTimeScroll - fSpan + fMargin;
			}
		}
	}

	if (m_bPendingRowScroll)
	{
		m_bPendingRowScroll = false;
		if (m_uPendingRowScroll < m_axRows.GetSize())
		{
			const float fRowTop = static_cast<float>(m_uPendingRowScroll) * xLayout.m_fRowHeight;
			const float fVisible = VisibleRowsHeight(xLayout);
			if (fRowTop < m_fRowScrollPixels)
			{
				m_fRowScrollPixels = fRowTop;
			}
			else if (fRowTop + xLayout.m_fRowHeight > m_fRowScrollPixels + fVisible)
			{
				m_fRowScrollPixels = fRowTop + xLayout.m_fRowHeight - fVisible;
			}
			const float fMaxScroll = TotalRowsHeight(xLayout) - fVisible;
			if (m_fRowScrollPixels > fMaxScroll) { m_fRowScrollPixels = fMaxScroll; }
			if (m_fRowScrollPixels < 0.0f) { m_fRowScrollPixels = 0.0f; }
		}
	}
}

void Zenith_EditorPanel_Animation::DrawSheetBackground(ImDrawList* pxDraw, const SheetLayout& xLayout)
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();

	pxDraw->AddRectFilled(Vec(xLayout.m_fCanvasLeft, xLayout.m_fCanvasTop),
		Vec(xLayout.m_fCanvasRight, xLayout.m_fCanvasBottom), xPalette.m_uPanelBg);
	// The label gutter reads as chrome, the key lane as content.
	pxDraw->AddRectFilled(Vec(xLayout.m_fCanvasLeft, xLayout.m_fCanvasTop),
		Vec(xLayout.m_fTrackLeft, xLayout.m_fCanvasBottom), xPalette.m_uPanelBgAlt);
	pxDraw->AddLine(Vec(xLayout.m_fTrackLeft, xLayout.m_fCanvasTop),
		Vec(xLayout.m_fTrackLeft, xLayout.m_fCanvasBottom), xPalette.m_uBorder);

	if (!m_xDocument.IsOpen() || xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}

	// D13's spatial half of the warning: the region PAST the duration is shaded,
	// so a key drawn in it reads as out of bounds without having to be counted.
	const float fEndPixel = Zenith_AnimTimelineTimeToPixel(m_xView, m_xDocument.GetDuration());
	const float fTrackRight = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
	if (IsFiniteFloat(fEndPixel) && fEndPixel < fTrackRight)
	{
		const float fFrom = fEndPixel > xLayout.m_fTrackLeft ? fEndPixel : xLayout.m_fTrackLeft;
		pxDraw->AddRectFilled(Vec(fFrom, xLayout.m_fCanvasTop), Vec(fTrackRight, xLayout.m_fCanvasBottom),
			IM_COL32(0, 0, 0, 60));
		pxDraw->AddLine(Vec(fEndPixel, xLayout.m_fCanvasTop), Vec(fEndPixel, xLayout.m_fCanvasBottom),
			xPalette.m_uTextDim);
	}
}

void Zenith_EditorPanel_Animation::DrawRuler(ImDrawList* pxDraw, const SheetLayout& xLayout)
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	pxDraw->AddRectFilled(Vec(xLayout.m_fTrackLeft, xLayout.m_fCanvasTop),
		Vec(xLayout.m_fTrackLeft + xLayout.m_fTrackWidth, xLayout.m_fRowsTop), xPalette.m_uToolbarBg);
	pxDraw->AddLine(Vec(xLayout.m_fCanvasLeft, xLayout.m_fRowsTop),
		Vec(xLayout.m_fCanvasRight, xLayout.m_fRowsTop), xPalette.m_uBorder);

	if (xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}

	// The ladder is WU-3.1's; both steps are guaranteed strictly positive and
	// finite for every view, including a degenerate one — which is precisely what
	// keeps the two loops below from being infinite.
	const Zenith_AnimTimelineTicks xTicks = Zenith_AnimTimelineChooseTicks(m_xView, GetFrameRate());
	float fStart = 0.0f;
	float fEnd = 0.0f;
	Zenith_AnimTimelineVisibleRange(m_xView, fStart, fEnd);

	const float fClipRight = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
	const ImVec4 xClip(xLayout.m_fTrackLeft, xLayout.m_fCanvasTop, fClipRight, xLayout.m_fRowsTop);
	ImFont* pxFont = ImGui::GetFont();
	const float fFontSize = ImGui::GetFontSize();

	const double dMinor = static_cast<double>(xTicks.m_fMinorSeconds);
	double dTick = std::floor(static_cast<double>(fStart) / dMinor) * dMinor;
	for (u_int u = 0; u < uSHEET_MAX_TICKS && dTick <= static_cast<double>(fEnd) + dMinor * 0.5; ++u, dTick += dMinor)
	{
		const float fPixel = Zenith_AnimTimelineTimeToPixel(m_xView, static_cast<float>(dTick));
		if (!IsFiniteFloat(fPixel) || fPixel < xLayout.m_fTrackLeft || fPixel > fClipRight)
		{
			continue;
		}
		pxDraw->AddLine(Vec(fPixel, xLayout.m_fRowsTop - Zenith_EditorUI::Px(5.0f)),
			Vec(fPixel, xLayout.m_fRowsTop), xPalette.m_uTextDim);
	}

	const double dMajor = static_cast<double>(xTicks.m_fMajorSeconds);
	dTick = std::floor(static_cast<double>(fStart) / dMajor) * dMajor;
	for (u_int u = 0; u < uSHEET_MAX_TICKS && dTick <= static_cast<double>(fEnd) + dMajor * 0.5; ++u, dTick += dMajor)
	{
		const float fSeconds = static_cast<float>(dTick);
		const float fPixel = Zenith_AnimTimelineTimeToPixel(m_xView, fSeconds);
		if (!IsFiniteFloat(fPixel) || fPixel < xLayout.m_fTrackLeft || fPixel > fClipRight)
		{
			continue;
		}
		pxDraw->AddLine(Vec(fPixel, xLayout.m_fCanvasTop), Vec(fPixel, xLayout.m_fRowsTop), xPalette.m_uTextDim);
		pxDraw->AddLine(Vec(fPixel, xLayout.m_fRowsTop), Vec(fPixel, xLayout.m_fCanvasBottom), xPalette.m_uFrame);

		// Two literal calls rather than a ternary format string: MSVC's C4774
		// fires on a non-literal format argument, and this build is /WX.
		char acLabel[32];
		if (dMajor < 0.5)
		{
			snprintf(acLabel, sizeof(acLabel), "%.2f", fSeconds);
		}
		else
		{
			snprintf(acLabel, sizeof(acLabel), "%.1f", fSeconds);
		}
		pxDraw->AddText(pxFont, fFontSize, Vec(fPixel + Zenith_EditorUI::Px(3.0f), xLayout.m_fCanvasTop + Zenith_EditorUI::Px(2.0f)),
			xPalette.m_uText, acLabel, nullptr, 0.0f, &xClip);
	}
}

void Zenith_EditorPanel_Animation::DrawRows(ImDrawList* pxDraw, const SheetLayout& xLayout, bool bCanvasHovered)
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	ImFont* pxFont = ImGui::GetFont();
	const float fFontSize = ImGui::GetFontSize();
	const ImVec4 xLabelClip(xLayout.m_fCanvasLeft, xLayout.m_fRowsTop, xLayout.m_fTrackLeft, xLayout.m_fCanvasBottom);

	// A collapse toggle is a VIEW change, not a mutation, so it belongs in this
	// unit. It is deferred out of the loop because SetGroupCollapsed rebuilds the
	// very vector being walked.
	std::string strToggleGroup;
	bool bToggleCollapsed = false;
	bool bHasToggle = false;

	const float fTrackRight = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;

	for (u_int uRow = 0; uRow < m_axRows.GetSize(); ++uRow)
	{
		const float fRowTop = xLayout.m_fRowsTop + static_cast<float>(uRow) * xLayout.m_fRowHeight - m_fRowScrollPixels;
		const float fRowBottom = fRowTop + xLayout.m_fRowHeight;
		if (fRowBottom <= xLayout.m_fRowsTop)
		{
			continue;
		}
		if (fRowTop >= xLayout.m_fCanvasBottom)
		{
			break;
		}

		const Zenith_AnimSheetRow& xRow = m_axRows.Get(uRow);
		const bool bHeader = (xRow.m_eKind == ZENITH_ANIMSHEET_ROW_BONE_HEADER)
		                  || (xRow.m_eKind == ZENITH_ANIMSHEET_ROW_ROOT_MOTION_HEADER);

		if ((uRow & 1u) != 0u)
		{
			pxDraw->AddRectFilled(Vec(xLayout.m_fCanvasLeft, fRowTop), Vec(xLayout.m_fCanvasRight, fRowBottom),
				xPalette.m_uPanelBgAlt);
		}
		if (bHeader)
		{
			pxDraw->AddRectFilled(Vec(xLayout.m_fCanvasLeft, fRowTop), Vec(xLayout.m_fCanvasRight, fRowBottom),
				xPalette.m_uFrame);
		}
		pxDraw->AddLine(Vec(xLayout.m_fCanvasLeft, fRowBottom), Vec(xLayout.m_fCanvasRight, fRowBottom),
			xPalette.m_uBorder);

		// Label.
		const float fIndent = LabelIndentForRow(xRow.m_eKind);
		if (bHeader)
		{
			const bool bCollapsed = IsGroupCollapsed(xRow.m_strGroupKey);
			Zenith_EditorUI::DrawIcon(pxDraw,
				bCollapsed ? Zenith_EditorIcon::ArrowRight : Zenith_EditorIcon::ArrowDown,
				Vec(xLayout.m_fCanvasLeft + Zenith_EditorUI::Px(10.0f), (fRowTop + fRowBottom) * 0.5f),
				Zenith_EditorUI::Px(10.0f), xPalette.m_uTextDim);

			// Hit-tested by hand, because the arrow is a decoration and not an item.
			if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				const ImVec2 xMouse = ImGui::GetIO().MousePos;
				if (xMouse.x >= xLayout.m_fCanvasLeft && xMouse.x <= xLayout.m_fTrackLeft
				 && xMouse.y >= fRowTop && xMouse.y <= fRowBottom)
				{
					strToggleGroup = xRow.m_strGroupKey;
					bToggleCollapsed = !bCollapsed;
					bHasToggle = true;
				}
			}
		}
		pxDraw->AddText(pxFont, fFontSize,
			Vec(xLayout.m_fCanvasLeft + fIndent, fRowTop + Zenith_EditorUI::Px(2.0f)),
			bHeader ? xPalette.m_uTextBright : xPalette.m_uText,
			xRow.m_strLabel.c_str(), nullptr, 0.0f, &xLabelClip);

		// ★ RECORDED ONLY WHEN THE ROW'S CENTRE IS INSIDE THE ROW BAND. A row half
		// under the ruler has a screen rect, but its centre is not somewhere a
		// click reaches the row — handing it out is the same defect as handing out
		// a scrolled-away one.
		const float fRowCentreY = (fRowTop + fRowBottom) * 0.5f;
		const bool bRowRecordable = fRowCentreY >= xLayout.m_fRowsTop && fRowCentreY <= xLayout.m_fCanvasBottom;
		if (bRowRecordable)
		{
			Zenith_AnimPanelRect xFull;
			xFull.m_fMinX = xLayout.m_fCanvasLeft;
			xFull.m_fMinY = fRowTop;
			xFull.m_fMaxX = xLayout.m_fCanvasRight;
			xFull.m_fMaxY = fRowBottom;
			m_xRowRects[uRow] = xFull;

			if (xLayout.m_fTrackWidth > 0.0f)
			{
				Zenith_AnimPanelRect xLane;
				xLane.m_fMinX = xLayout.m_fTrackLeft;
				xLane.m_fMinY = fRowTop;
				xLane.m_fMaxX = fTrackRight;
				xLane.m_fMaxY = fRowBottom;
				m_xRowTrackRects[uRow] = xLane;
			}
		}

		// Keys and events are drawn only on a row that was RECORDED. Painting a
		// diamond on a half-clipped row while refusing to hand out its rect would
		// put something on screen that nothing can be told the position of, which
		// is the same trap as the reverse — one of them just fails more quietly.
		if (!bRowRecordable)
		{
			continue;
		}
		if (xRow.m_eKind == ZENITH_ANIMSHEET_ROW_EVENTS)
		{
			DrawEventsForRow(pxDraw, xLayout, fRowTop);
		}
		else if (xRow.m_bHasTrack)
		{
			DrawKeysForRow(pxDraw, xLayout, uRow, fRowTop);
		}
	}

	if (bHasToggle)
	{
		SetGroupCollapsed(strToggleGroup, bToggleCollapsed);
	}
}

void Zenith_EditorPanel_Animation::DrawKeysForRow(ImDrawList* pxDraw, const SheetLayout& xLayout, u_int uRowIndex, float fRowTop)
{
	if (!m_xDocument.IsOpen() || xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const Zenith_AnimSheetRow& xRow = m_axRows.Get(uRowIndex);
	const float fCentreY = fRowTop + xLayout.m_fRowHeight * 0.5f;
	const float fHalf = Zenith_EditorUI::Px(fSHEET_KEY_HALF_1X);

	const u_int uKeyCount = m_xDocument.GetKeyCount(xRow.m_xTrack);
	for (u_int uKey = 0; uKey < uKeyCount; ++uKey)
	{
		const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xRow.m_xTrack, uKey);
		float fTime = 0.0f;
		if (!m_xDocument.GetKeyTime(xRow.m_xTrack, uKeyId, fTime))
		{
			continue;
		}
		// ★ THE VISIBILITY TEST IS THE MAPPING'S, not a pixel compare of our own:
		// it is false for a zero-width track, which is the one case a bare compare
		// gets wrong (a key drawn on a 0 px strip).
		if (!Zenith_AnimTimelineIsVisible(m_xView, fTime))
		{
			continue;
		}
		const float fCentreX = Zenith_AnimTimelineTimeToPixel(m_xView, fTime);
		if (!IsFiniteFloat(fCentreX))
		{
			continue;
		}

		const bool bPastDuration = m_xKeysPastDuration.Contains(MakeKeyRectKey(uRowIndex, uKeyId));
		const bool bSelected = IsKeySelected(xRow.m_xTrack, uKeyId);
		// D11's visible half: the key that REFUSED a drop is lit until the flash
		// runs out, so a drag that springs back reads as a refusal rather than as
		// the panel not working.
		const bool bBlocked = m_uCollisionFlashFrames > 0u
			&& m_uCollisionFlashKeyId == uKeyId && m_xCollisionFlashTrack == xRow.m_xTrack;

		ImU32 uFill = bPastDuration ? xPalette.m_uWarning : xPalette.m_uAccent;
		if (bSelected) { uFill = xPalette.m_uSelection; }
		if (bBlocked) { uFill = xPalette.m_uError; }

		// A diamond, the dope-sheet convention.
		const ImVec2 axPoints[4] =
		{
			Vec(fCentreX, fCentreY - fHalf),
			Vec(fCentreX + fHalf, fCentreY),
			Vec(fCentreX, fCentreY + fHalf),
			Vec(fCentreX - fHalf, fCentreY),
		};
		pxDraw->AddConvexPolyFilled(axPoints, 4, uFill);
		pxDraw->AddPolyline(axPoints, 4, bSelected ? xPalette.m_uTextBright : xPalette.m_uBorder,
			ImDrawFlags_Closed, bSelected ? 2.0f : 1.0f);

		// ★ THE DRAG GHOST — WHERE THE KEY WOULD LAND, WITH THE KEY STILL WHERE IT
		// IS. Nothing is mutated until the button comes up (see m_bDraggingKeys),
		// so the preview has to be drawn rather than read back out of the document;
		// and it uses EffectiveDragDelta, the SAME function the drop applies, so a
		// preview cannot show a position the drop does not produce.
		if (m_bDraggingKeys && bSelected)
		{
			const float fGhostX = Zenith_AnimTimelineTimeToPixel(m_xView, fTime + m_fDragDeltaSeconds);
			if (IsFiniteFloat(fGhostX))
			{
				const ImVec2 axGhost[4] =
				{
					Vec(fGhostX, fCentreY - fHalf),
					Vec(fGhostX + fHalf, fCentreY),
					Vec(fGhostX, fCentreY + fHalf),
					Vec(fGhostX - fHalf, fCentreY),
				};
				pxDraw->AddPolyline(axGhost, 4, xPalette.m_uTextBright, ImDrawFlags_Closed, 1.5f);
			}
		}

		if (bPastDuration)
		{
			Zenith_EditorUI::DrawIcon(pxDraw, Zenith_EditorIcon::Warning,
				Vec(fCentreX, fCentreY - fHalf - Zenith_EditorUI::Px(4.0f)),
				Zenith_EditorUI::Px(9.0f), xPalette.m_uWarning);
		}

		Zenith_AnimPanelRect xRect;
		xRect.m_fMinX = fCentreX - fHalf;
		xRect.m_fMinY = fCentreY - fHalf;
		xRect.m_fMaxX = fCentreX + fHalf;
		xRect.m_fMaxY = fCentreY + fHalf;
		m_xKeyRects[MakeKeyRectKey(uRowIndex, uKeyId)] = xRect;
	}
}

void Zenith_EditorPanel_Animation::DrawEventsForRow(ImDrawList* pxDraw, const SheetLayout& xLayout, float fRowTop)
{
	// No row index: an event rect is keyed by the EVENT's stable id, because
	// there is exactly one events row and an event is addressed by identity, not
	// by which lane it happened to be drawn in.
	if (!m_xDocument.IsOpen() || xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const float fCentreY = fRowTop + xLayout.m_fRowHeight * 0.5f;
	const float fHalf = Zenith_EditorUI::Px(fSHEET_EVENT_HALF_1X);
	const float fDuration = m_xDocument.GetDuration();
	ImFont* pxFont = ImGui::GetFont();
	const float fFontSize = ImGui::GetFontSize();
	// The name is clipped to the KEY LANE, so a long one cannot spill back over
	// the label gutter or out past the canvas.
	const ImVec4 xLaneClip(xLayout.m_fTrackLeft, fRowTop,
		xLayout.m_fTrackLeft + xLayout.m_fTrackWidth, fRowTop + xLayout.m_fRowHeight);

	const u_int uEventCount = m_xDocument.GetEventCount();
	for (u_int u = 0; u < uEventCount; ++u)
	{
		const u_int uEventId = m_xDocument.GetEventIdAtIndex(u);
		Flux_AnimationEvent xEvent;
		if (!m_xDocument.GetEvent(uEventId, xEvent))
		{
			continue;
		}
		// D4: an event time is a [0,1] FRACTION. The sheet's x axis is seconds, so
		// this multiply is the one conversion — and it lives here, next to the only
		// thing that needs it, rather than in the shared mapping.
		const float fTime = xEvent.m_fNormalizedTime * fDuration;
		if (!Zenith_AnimTimelineIsVisible(m_xView, fTime))
		{
			continue;
		}
		const float fCentreX = Zenith_AnimTimelineTimeToPixel(m_xView, fTime);
		if (!IsFiniteFloat(fCentreX))
		{
			continue;
		}

		const bool bPastDuration = xEvent.m_fNormalizedTime > 1.0f + fANIM_TIME_EPSILON;
		const bool bSelected = IsEventSelected(uEventId);
		ImU32 uFill = bPastDuration ? xPalette.m_uWarning : xPalette.m_uTypeAnimation;
		if (bSelected) { uFill = xPalette.m_uSelection; }

		// A flag, so an event never reads as a key.
		const ImVec2 axPoints[3] =
		{
			Vec(fCentreX - fHalf, fCentreY - fHalf),
			Vec(fCentreX + fHalf, fCentreY),
			Vec(fCentreX - fHalf, fCentreY + fHalf),
		};
		pxDraw->AddConvexPolyFilled(axPoints, 3, uFill);
		pxDraw->AddPolyline(axPoints, 3, bSelected ? xPalette.m_uTextBright : xPalette.m_uBorder,
			ImDrawFlags_Closed, bSelected ? 2.0f : 1.0f);

		// ★ THE NAME IS DRAWN, AND IT IS THE WHOLE REASON AN EVENT IS NOT A KEY.
		// A row of unlabelled flags says an event exists and nothing about which;
		// the name is what a listener matches on, so it is the one field an author
		// has to be able to read without clicking.
		pxDraw->AddText(pxFont, fFontSize,
			Vec(fCentreX + fHalf + Zenith_EditorUI::Px(3.0f), fRowTop + Zenith_EditorUI::Px(2.0f)),
			bSelected ? xPalette.m_uTextBright : xPalette.m_uText,
			xEvent.m_strEventName.c_str(), nullptr, 0.0f, &xLaneClip);

		// The drag GHOST — where the flag would land, with the flag still where it
		// is. Nothing is mutated until the button comes up, and the offset is
		// EffectiveEventDragDelta, the same function the drop applies, so the
		// preview cannot promise a position the drop will not deliver.
		if (m_bDraggingEvents && bSelected)
		{
			const float fGhostX = Zenith_AnimTimelineTimeToPixel(m_xView,
				(xEvent.m_fNormalizedTime + m_fEventDragDeltaNormalized) * fDuration);
			if (IsFiniteFloat(fGhostX))
			{
				const ImVec2 axGhost[3] =
				{
					Vec(fGhostX - fHalf, fCentreY - fHalf),
					Vec(fGhostX + fHalf, fCentreY),
					Vec(fGhostX - fHalf, fCentreY + fHalf),
				};
				pxDraw->AddPolyline(axGhost, 3, xPalette.m_uTextBright, ImDrawFlags_Closed, 1.5f);
			}
		}

		// D13's per-item half, the same glyph a key past the duration gets. An
		// event past 1.0 is never sampled and is otherwise silent.
		if (bPastDuration)
		{
			Zenith_EditorUI::DrawIcon(pxDraw, Zenith_EditorIcon::Warning,
				Vec(fCentreX, fCentreY - fHalf - Zenith_EditorUI::Px(4.0f)),
				Zenith_EditorUI::Px(9.0f), xPalette.m_uWarning);
		}

		Zenith_AnimPanelRect xRect;
		xRect.m_fMinX = fCentreX - fHalf;
		xRect.m_fMinY = fCentreY - fHalf;
		xRect.m_fMaxX = fCentreX + fHalf;
		xRect.m_fMaxY = fCentreY + fHalf;
		m_xEventRects[uEventId] = xRect;
	}
}

void Zenith_EditorPanel_Animation::DrawPlayhead(ImDrawList* pxDraw, const SheetLayout& xLayout)
{
	if (!m_xSession.IsOpen() || xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}
	const float fTime = m_xSession.GetTime();
	if (!Zenith_AnimTimelineIsVisible(m_xView, fTime))
	{
		return;
	}
	const float fPixel = Zenith_AnimTimelineTimeToPixel(m_xView, fTime);
	if (!IsFiniteFloat(fPixel))
	{
		return;
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const float fHalf = Zenith_EditorUI::Px(fSHEET_PLAYHEAD_HALF_1X);
	pxDraw->AddRectFilled(Vec(fPixel - fHalf, xLayout.m_fCanvasTop), Vec(fPixel + fHalf, xLayout.m_fCanvasBottom),
		xPalette.m_uPlay);
	// A grab handle in the ruler, so WU-3.3 has somewhere obvious to scrub from.
	pxDraw->AddRectFilled(Vec(fPixel - Zenith_EditorUI::Px(4.0f), xLayout.m_fCanvasTop),
		Vec(fPixel + Zenith_EditorUI::Px(4.0f), xLayout.m_fCanvasTop + Zenith_EditorUI::Px(6.0f)),
		xPalette.m_uPlay);

	m_xPlayheadRect.m_fMinX = fPixel - Zenith_EditorUI::Px(4.0f);
	m_xPlayheadRect.m_fMinY = xLayout.m_fCanvasTop;
	m_xPlayheadRect.m_fMaxX = fPixel + Zenith_EditorUI::Px(4.0f);
	m_xPlayheadRect.m_fMaxY = xLayout.m_fCanvasBottom;
	m_bPlayheadRectValid = true;
}

void Zenith_EditorPanel_Animation::DrawSelectionOverlays(ImDrawList* pxDraw, const SheetLayout& xLayout)
{
	if (xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();

	// ---- the rubber band ----------------------------------------------------
	if (m_bBoxSelecting)
	{
		const float fMinX = m_fBoxStartX < m_fBoxEndX ? m_fBoxStartX : m_fBoxEndX;
		const float fMaxX = m_fBoxStartX < m_fBoxEndX ? m_fBoxEndX : m_fBoxStartX;
		const float fMinY = m_fBoxStartY < m_fBoxEndY ? m_fBoxStartY : m_fBoxEndY;
		const float fMaxY = m_fBoxStartY < m_fBoxEndY ? m_fBoxEndY : m_fBoxStartY;
		if (IsFiniteFloat(fMinX) && IsFiniteFloat(fMinY) && IsFiniteFloat(fMaxX) && IsFiniteFloat(fMaxY))
		{
			pxDraw->AddRectFilled(Vec(fMinX, fMinY), Vec(fMaxX, fMaxY), IM_COL32(90, 140, 220, 48));
			pxDraw->AddRect(Vec(fMinX, fMinY), Vec(fMaxX, fMaxY), xPalette.m_uSelection);
		}
	}

	// ---- the duration handle ------------------------------------------------
	if (!m_xDocument.IsOpen())
	{
		return;
	}
	// While it is being dragged the LIVE value is the dragged one — nothing has
	// been written to the document yet (see m_bDraggingDuration).
	const float fShownDuration = m_bDraggingDuration ? m_fDurationDragSeconds : m_xDocument.GetDuration();
	const float fEndPixel = Zenith_AnimTimelineTimeToPixel(m_xView, fShownDuration);
	const float fTrackRight = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
	if (!IsFiniteFloat(fEndPixel) || fEndPixel < xLayout.m_fTrackLeft || fEndPixel > fTrackRight)
	{
		return;
	}

	const float fGrab = Zenith_EditorUI::Px(fSHEET_DURATION_GRAB_1X);
	pxDraw->AddRectFilled(Vec(fEndPixel - fGrab, xLayout.m_fCanvasTop),
		Vec(fEndPixel + fGrab, xLayout.m_fRowsTop),
		m_bDraggingDuration ? xPalette.m_uAccentHover : xPalette.m_uAccentDim);
	if (m_bDraggingDuration)
	{
		// The ghost line, so the drop position is visible before it is committed.
		pxDraw->AddLine(Vec(fEndPixel, xLayout.m_fCanvasTop), Vec(fEndPixel, xLayout.m_fCanvasBottom),
			xPalette.m_uAccentHover, 2.0f);
	}
}

//=============================================================================
// INPUT TRANSLATION.
//
// ★ THE ONLY FUNCTIONS IN THE PANEL THAT READ ImGui STATE, and each one ends in
// an Action_* call that reads none. That split is the whole reason WU-3.4 can
// drive the sheet without synthesising a click: "what the gesture means" lives
// here, in one readable place, and "what the operation does" lives in the _Ops
// TU where a unit can call it directly. A handler that did its own document
// work would be a second mutation path that no test exercises.
//=============================================================================

Zenith_AnimSelectMode Zenith_EditorPanel_Animation::SelectModeFromModifiers()
{
	const ImGuiIO& xIO = ImGui::GetIO();
	if (xIO.KeyCtrl)
	{
		return ZENITH_ANIMSELECT_TOGGLE;
	}
	if (xIO.KeyShift)
	{
		return ZENITH_ANIMSELECT_ADD;
	}
	return ZENITH_ANIMSELECT_REPLACE;
}

bool Zenith_EditorPanel_Animation::FindKeyAtScreenPos(float fX, float fY,
	Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const
{
	for (u_int uRow = 0; uRow < m_axRows.GetSize(); ++uRow)
	{
		const Zenith_AnimSheetRow& xRow = m_axRows.Get(uRow);
		if (!xRow.m_bHasTrack)
		{
			continue;
		}
		const u_int uKeyCount = m_xDocument.GetKeyCount(xRow.m_xTrack);
		for (u_int u = 0; u < uKeyCount; ++u)
		{
			const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xRow.m_xTrack, u);
			Zenith_AnimPanelRect xRect;
			// ★ THROUGH THE PUBLISHED ACCESSOR, so a key the off-screen gate refuses
			// is a key no click can pick — the click and the coordinate handed to a
			// test agree by construction.
			if (!GetKeyRect(xRow.m_xTrack, uKeyId, xRect))
			{
				continue;
			}
			if (fX >= xRect.m_fMinX && fX <= xRect.m_fMaxX && fY >= xRect.m_fMinY && fY <= xRect.m_fMaxY)
			{
				xOutTrack = xRow.m_xTrack;
				uOutKeyId = uKeyId;
				return true;
			}
		}
	}
	return false;
}

bool Zenith_EditorPanel_Animation::FindEventAtScreenPos(float fX, float fY, u_int& uOutEventId) const
{
	const u_int uEventCount = m_xDocument.GetEventCount();
	for (u_int u = 0; u < uEventCount; ++u)
	{
		const u_int uEventId = m_xDocument.GetEventIdAtIndex(u);
		Zenith_AnimPanelRect xRect;
		if (!GetEventRect(uEventId, xRect))
		{
			continue;
		}
		if (fX >= xRect.m_fMinX && fX <= xRect.m_fMaxX && fY >= xRect.m_fMinY && fY <= xRect.m_fMaxY)
		{
			uOutEventId = uEventId;
			return true;
		}
	}
	return false;
}

bool Zenith_EditorPanel_Animation::IsInEventsRowLane(float fX, float fY) const
{
	if (m_uEventsRowIndex == uINVALID_ANIM_SHEET_ROW)
	{
		return false;
	}
	Zenith_AnimPanelRect xLane;
	// ★ THROUGH THE PUBLISHED ACCESSOR, like every other hit test here: a row the
	// off-screen gate refuses is a row no gesture may land on, so "double-click to
	// add" cannot place an event on a row nobody can see.
	if (!GetRowTrackRectByIndex(m_uEventsRowIndex, xLane))
	{
		return false;
	}
	return fX >= xLane.m_fMinX && fX <= xLane.m_fMaxX && fY >= xLane.m_fMinY && fY <= xLane.m_fMaxY;
}

bool Zenith_EditorPanel_Animation::UpdateDurationDrag(float fMouseX, bool bDown, bool bReleased, u_int uFrameRate)
{
	if (!m_bDraggingDuration) { return false; }
	float fTime = Zenith_AnimTimelinePixelToTime(m_xView, fMouseX);
	if (!ImGui::GetIO().KeyShift) { fTime = Zenith_AnimTimelineSnapToFrame(fTime, uFrameRate); }
	if (fTime < 0.0f) { fTime = 0.0f; }
	if (IsFiniteFloat(fTime)) { m_fDurationDragSeconds = fTime; }
	if (bReleased || !bDown)
	{
		m_bDraggingDuration = false;
		Action_SetDuration(m_fDurationDragSeconds);
	}
	return true;
}

bool Zenith_EditorPanel_Animation::UpdateScrub(float fMouseX, bool bDown, bool bReleased, u_int uFrameRate)
{
	if (!m_bScrubbing) { return false; }
	float fTime = Zenith_AnimTimelinePixelToTime(m_xView, fMouseX);
	if (!ImGui::GetIO().KeyShift) { fTime = Zenith_AnimTimelineSnapToFrame(fTime, uFrameRate); }
	Action_Scrub(fTime);
	if (bReleased || !bDown) { m_bScrubbing = false; }
	return true;
}

bool Zenith_EditorPanel_Animation::UpdateKeyDrag(float fMouseX, bool bDown, bool bReleased, bool bShiftHeld)
{
	if (!m_bDraggingKeys) { return false; }
	const float fRawDelta = Zenith_AnimTimelinePixelsToSeconds(m_xView, fMouseX - m_fDragStartMouseX);
	m_fDragDeltaSeconds = EffectiveDragDelta(fRawDelta, !bShiftHeld);
	if (bReleased || !bDown)
	{
		m_bDraggingKeys = false;
		Action_MoveSelection(fRawDelta, !bShiftHeld);
		m_fDragDeltaSeconds = 0.0f;
	}
	return true;
}

bool Zenith_EditorPanel_Animation::UpdateEventDrag(float fMouseX, bool bDown, bool bReleased, bool bShiftHeld)
{
	if (!m_bDraggingEvents) { return false; }
	const float fRawSeconds = Zenith_AnimTimelinePixelsToSeconds(m_xView, fMouseX - m_fEventDragStartMouseX);
	const float fDocDuration = m_xDocument.GetDuration();
	const float fRawNormalized = fDocDuration > 0.0f ? fRawSeconds / fDocDuration : 0.0f;
	m_fEventDragDeltaNormalized = EffectiveEventDragDelta(fRawNormalized, !bShiftHeld);
	if (bReleased || !bDown)
	{
		m_bDraggingEvents = false;
		Action_MoveSelectedEvents(fRawNormalized, !bShiftHeld);
		m_fEventDragDeltaNormalized = 0.0f;
	}
	return true;
}

bool Zenith_EditorPanel_Animation::UpdateBoxSelection(float fMouseX, float fMouseY, bool bDown, bool bReleased)
{
	if (!m_bBoxSelecting) { return false; }
	m_fBoxEndX = fMouseX;
	m_fBoxEndY = fMouseY;
	if (bReleased || !bDown)
	{
		m_bBoxSelecting = false;
		const float fSlop = Zenith_EditorUI::Px(fSHEET_CLICK_SLOP_1X);
		const bool bIsClick = std::fabs(m_fBoxEndX - m_fBoxStartX) <= fSlop
			&& std::fabs(m_fBoxEndY - m_fBoxStartY) <= fSlop;
		if (bIsClick && SelectModeFromModifiers() == ZENITH_ANIMSELECT_REPLACE)
		{
			Action_ClearSelection();
		}
		else if (!bIsClick)
		{
			Action_BoxSelect(m_fBoxStartX, m_fBoxStartY, m_fBoxEndX, m_fBoxEndY, SelectModeFromModifiers());
		}
	}
	return true;
}

void Zenith_EditorPanel_Animation::HandleSheetInput(const SheetLayout& xLayout, bool bCanvasHovered)
{
	if (!m_xDocument.IsOpen() || xLayout.m_fTrackWidth <= 0.0f)
	{
		return;
	}

	const ImGuiIO& xIO = ImGui::GetIO();
	const float fMouseX = xIO.MousePos.x;
	const float fMouseY = xIO.MousePos.y;
	const bool bDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool bReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
	const float fTrackRight = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
	const u_int uFrameRate = GetFrameRate();

	// ★ AN IN-FLIGHT GESTURE IS SERVICED FIRST AND WITHOUT A HOVER TEST. The
	// cursor routinely leaves the canvas mid-drag (that is what dragging a key
	// past the right edge IS), and a gesture that stopped tracking there would
	// freeze at the boundary and then apply a stale delta on release.

	// ---- duration handle ----------------------------------------------------
	if (UpdateDurationDrag(fMouseX, bDown, bReleased, uFrameRate)) { return; }

	// ---- playhead scrub -----------------------------------------------------
	if (UpdateScrub(fMouseX, bDown, bReleased, uFrameRate)) { return; }

	// ---- key drag -----------------------------------------------------------
	if (UpdateKeyDrag(fMouseX, bDown, bReleased, xIO.KeyShift)) { return; }

	// ---- event drag ---------------------------------------------------------
	if (UpdateEventDrag(fMouseX, bDown, bReleased, xIO.KeyShift)) { return; }

	// ---- rubber band --------------------------------------------------------
	if (UpdateBoxSelection(fMouseX, fMouseY, bDown, bReleased)) { return; }

	// ---- nothing in flight: can a new gesture start? ------------------------

	// RIGHT-click first, and it only ever means one thing on this sheet: the
	// event context menu. It is tested before the left-button gate because the
	// two buttons are independent and a right-click must not have to wait for one.
	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		u_int uContextEventId = uINVALID_ANIM_KEY_ID;
		if (FindEventAtScreenPos(fMouseX, fMouseY, uContextEventId))
		{
			// A right-click on something NOT already picked selects it; one on a
			// member of a multi-event selection leaves the selection alone, so
			// "delete these five" does not collapse to "delete this one".
			if (!IsEventSelected(uContextEventId))
			{
				Action_SelectEvent(uContextEventId, ZENITH_ANIMSELECT_REPLACE);
			}
			// The popup is OPENED from the window scope (RenderEventContextMenu),
			// not from here: the whole sheet is one InvisibleButton and this
			// function runs after it has been submitted.
			m_bEventContextMenuRequested = true;
		}
		return;
	}

	if (!bCanvasHovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		return;
	}
	if (fMouseX < xLayout.m_fTrackLeft || fMouseX > fTrackRight)
	{
		// The label gutter. DrawRows owns that column's collapse hit-test.
		return;
	}

	// ---- double-click the events row to ADD one there -----------------------
	// ★ TESTED BEFORE EVERYTHING ELSE BELOW, because IsMouseClicked is ALSO true
	// on the second press of a double-click: reaching the marker hit-test first
	// would turn a double-click into a select-and-drag and the event would never
	// be created.
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && IsInEventsRowLane(fMouseX, fMouseY))
	{
		u_int uUnderCursorId = uINVALID_ANIM_KEY_ID;
		if (!FindEventAtScreenPos(fMouseX, fMouseY, uUnderCursorId))
		{
			float fSeconds = Zenith_AnimTimelinePixelToTime(m_xView, fMouseX);
			if (!xIO.KeyShift)
			{
				fSeconds = Zenith_AnimTimelineSnapToFrame(fSeconds, uFrameRate);
			}
			if (fSeconds < 0.0f) { fSeconds = 0.0f; }
			const float fDocDuration = m_xDocument.GetDuration();
			// D4 again: what is stored is the FRACTION. A clip with no duration has
			// no fraction to place one at, so the gesture is refused rather than
			// dividing by zero and writing a NaN into the .zanim.
			if (fDocDuration > 0.0f && IsFiniteFloat(fSeconds))
			{
				Action_AddEvent(fSeconds / fDocDuration, DefaultEventName());
			}
		}
		return;
	}

	const bool bInRuler = fMouseY >= xLayout.m_fCanvasTop && fMouseY <= xLayout.m_fRowsTop;

	// The duration handle wins over the scrub: it is drawn ON the ruler and the
	// two bands overlap, so the more specific target has to be tested first.
	const float fDurationPixel = Zenith_AnimTimelineTimeToPixel(m_xView, m_xDocument.GetDuration());
	if (bInRuler && IsFiniteFloat(fDurationPixel)
	 && std::fabs(fMouseX - fDurationPixel) <= Zenith_EditorUI::Px(fSHEET_DURATION_GRAB_1X))
	{
		m_bDraggingDuration = true;
		m_fDurationDragSeconds = m_xDocument.GetDuration();
		return;
	}

	if (bInRuler)
	{
		m_bScrubbing = true;
		float fTime = Zenith_AnimTimelinePixelToTime(m_xView, fMouseX);
		if (!xIO.KeyShift)
		{
			fTime = Zenith_AnimTimelineSnapToFrame(fTime, uFrameRate);
		}
		Action_Scrub(fTime);
		return;
	}

	Zenith_AnimTrackId xHitTrack;
	u_int uHitKeyId = uINVALID_ANIM_KEY_ID;
	if (FindKeyAtScreenPos(fMouseX, fMouseY, xHitTrack, uHitKeyId))
	{
		const Zenith_AnimSelectMode eMode = SelectModeFromModifiers();
		// ★ CLICKING AN ALREADY-SELECTED KEY WITH NO MODIFIER KEEPS THE SELECTION.
		// Re-running REPLACE there would collapse a multi-key selection to one key
		// on the mouse DOWN of the drag that was meant to move all of them — the
		// gesture would look like it worked and move a single key.
		if (eMode != ZENITH_ANIMSELECT_REPLACE || !IsKeySelected(xHitTrack, uHitKeyId))
		{
			Action_SelectKey(xHitTrack, uHitKeyId, eMode);
		}
		else
		{
			// Still make the clicked key the primary, so the snap is computed
			// against the one under the cursor.
			Action_SelectKey(xHitTrack, uHitKeyId, ZENITH_ANIMSELECT_ADD);
		}

		if (IsKeySelected(xHitTrack, uHitKeyId))
		{
			m_bDraggingKeys = true;
			m_fDragStartMouseX = fMouseX;
			m_fDragDeltaSeconds = 0.0f;
		}
		return;
	}

	u_int uHitEventId = uINVALID_ANIM_KEY_ID;
	if (FindEventAtScreenPos(fMouseX, fMouseY, uHitEventId))
	{
		const Zenith_AnimSelectMode eEventMode = SelectModeFromModifiers();
		// Same rule as a key: clicking an already-selected event with no modifier
		// KEEPS the selection and only re-primaries it. Re-running REPLACE there
		// would collapse a multi-event selection to one on the mouse DOWN of the
		// drag that was meant to move all of them.
		if (eEventMode != ZENITH_ANIMSELECT_REPLACE || !IsEventSelected(uHitEventId))
		{
			Action_SelectEvent(uHitEventId, eEventMode);
		}
		else
		{
			Action_SelectEvent(uHitEventId, ZENITH_ANIMSELECT_ADD);
		}

		if (IsEventSelected(uHitEventId))
		{
			m_bDraggingEvents = true;
			m_fEventDragStartMouseX = fMouseX;
			m_fEventDragDeltaNormalized = 0.0f;
		}
		return;
	}

	m_bBoxSelecting = true;
	m_fBoxStartX = fMouseX;
	m_fBoxStartY = fMouseY;
	m_fBoxEndX = fMouseX;
	m_fBoxEndY = fMouseY;
}

void Zenith_EditorPanel_Animation::HandleSheetKeyboard()
{
	if (!m_xDocument.IsOpen())
	{
		return;
	}
	const ImGuiIO& xIO = ImGui::GetIO();
	// The toolbar's path / skeleton / mesh fields are ordinary InputTexts, and a
	// Ctrl+C typed into one of them belongs to the text, not to the sheet.
	if (xIO.WantTextInput)
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		Action_DeleteSelection();
	}

	// K — Set Key for the selected bone (WU-4.3). Unmodified, like Delete, and
	// scoped to this window's focus for the same reason: a K typed in the console
	// must not write a keyframe.
	if (!xIO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false))
	{
		Action_SetKeyForSelectedBone();
	}

	if (!xIO.KeyCtrl)
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
	{
		if (xIO.KeyShift) { Action_Redo(); } else { Action_Undo(); }
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
	{
		Action_Redo();
	}
	if (ImGui::IsKeyPressed(ImGuiKey_C, false))
	{
		Action_CopySelection();
	}
	if (ImGui::IsKeyPressed(ImGuiKey_V, false))
	{
		// ★ THE PLAYHEAD IS THE PASTE ORIGIN, and the last-touched bone is the
		// target. Both are things the user can SEE, which is the requirement for a
		// keyboard gesture that has no cursor position of its own.
		const float fOffset = m_xSession.IsOpen() ? m_xSession.GetTime() : 0.0f;
		Action_PasteToBone(m_strPasteTargetBone, fOffset);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_D, false))
	{
		Action_DuplicateSelection();
	}
}

#endif // ZENITH_TOOLS
