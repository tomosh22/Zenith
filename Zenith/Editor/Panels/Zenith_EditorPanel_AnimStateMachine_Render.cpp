#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.h"
#include "Editor/Zenith_EditorUI.h"
// WU-7.2's layer strip asks LayerAcceptsMask / AdditiveLayerMaskNotice — the ONE
// statement of the additive-layer rule and the ONE wording of its refusal.
#include "Editor/Zenith_BoneMaskDocument.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/Zenith_DragDropPayloads.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

//=============================================================================
// The DRAWING half of the state-machine graph.
//
// ★ THE CANVAS IS ONE InvisibleButton AND A DRAW LIST (Editor/CLAUDE.md's
// "draw-list decorations, not items"). Nodes, edges, arrowheads, the default-
// state marker and the highlight ring are all painted at absolute screen
// coordinates. Placing items there with SetCursorScreenPos + restore trips
// ErrorCheckUsingSetCursorPosToExtendParentBoundaries, which in a windowed
// build is a modal CRT dialog nothing logs: the process simply hangs.
//
// ★ NO FUNCTION HERE COMPUTES A NODE RECT ITSELF. Every one asks
// ComputeNodeScreenRect, so the edge pass, the node pass and the hit test are
// one derivation — the same rule the dope sheet applies to its seconds<->pixels
// mapping, and for the same reason.
//
// ★ EVERY HANDLER ENDS IN AN Action_*. Nothing here mutates the document.
//=============================================================================

namespace
{
	// Layout, at 1x DPI. Everything goes through Zenith_EditorUI::Px.
	// Widened by WU-7.2: the layer rows carry an index, a name, an id, a blend
	// mode and a weight, and the detail block under them a full-width path field.
	constexpr float fANIMSM_SIDE_PANEL_WIDTH_1X = 300.0f;
	constexpr float fANIMSM_INSPECTOR_HEIGHT_1X = 170.0f;
	// The layer list's OWN scroll child (see RenderLayerStrip): bounded, so a
	// controller with twenty layers cannot push the "+ Layer" button and the
	// selected layer's controls out of reach.
	constexpr float fANIMSM_LAYER_LIST_HEIGHT_1X = 116.0f;
	constexpr float fANIMSM_GRID_STEP_1X        = 32.0f;
	// How near a click has to be to an edge's midpoint marker to pick it.
	constexpr float fANIMSM_EDGE_GRAB_1X        = 9.0f;
	// A press-and-release inside this many pixels is a CLICK, not a drag.
	constexpr float fANIMSM_CLICK_SLOP_1X       = 3.0f;

	// ★ ONE WORDING FOR THE ANY-STATE LIST, everywhere it is named. The transition
	// inspector already showed "<Any State>" for an empty from-state; the
	// pseudo-node's label, the inspector header and the refusal below now read the
	// same string, so the canvas and the inspector cannot end up calling the same
	// thing two names.
	const char* const szANIMSM_ANY_STATE_LABEL = "<Any State>";
	// ★ THE REFUSAL IS DRAWN, NOT OMITTED. The node context menu and "Rename
	// Selected" are gated on a non-empty state selection, so with the pseudo-node
	// selected they would simply show nothing — indistinguishable from a bug. They
	// are drawn DISABLED with this on the tooltip instead.
	const char* const szANIMSM_ANY_STATE_REFUSAL = "Any State cannot be renamed or deleted";

	ImVec2 Vec(float fX, float fY) { return ImVec2(fX, fY); }

	bool IsFiniteFloat(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	const char* const aszPARAM_TYPE_NAMES[] = { "Float", "Int", "Bool", "Trigger" };
	const char* const aszCOMPARE_OP_NAMES[] = { "==", "!=", ">", "<", ">=", "<=" };
	// WU-7.3's tree-kind combo, in Zenith_AnimCtrlStateTreeKind's own order minus
	// the two kinds SetStateTreeKind refuses (EMPTY has its own verb, COMPLEX
	// cannot be synthesised), so the combo index is an OFFSET from SINGLE_CLIP
	// rather than a mapping table that can drift from the enum.
	const char* const aszTREE_KIND_NAMES[] = { "Single Clip", "Blend Space 1D", "Blend Space 2D" };

	// A clip's NAME from the def's clip PATH: from the asset when it loads, and
	// from the file stem when it does not — a path that will not load is still
	// worth offering, because the fix may be to author the state first. One
	// definition, because the state inspector's clip combo and the blend strip's
	// both need it and two would disagree the day a clip's name stopped matching
	// its stem.
	std::string AnimSmClipNameForPath(const std::string& strPath)
	{
		std::string strName = strPath;
		const size_t uSlash = strName.find_last_of("/\\");
		if (uSlash != std::string::npos)
		{
			strName = strName.substr(uSlash + 1);
		}
		const size_t uDot = strName.find_last_of('.');
		if (uDot != std::string::npos)
		{
			strName = strName.substr(0, uDot);
		}
		const Zenith_AnimationAsset* pxClipAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(strPath);
		if (pxClipAsset != nullptr && pxClipAsset->GetClip() != nullptr
			&& !pxClipAsset->GetClip()->GetName().empty())
		{
			strName = pxClipAsset->GetClip()->GetName();
		}
		return strName;
	}
	// In Flux_LayerBlendMode's own declaration order, so the combo index IS the
	// enumerator and no mapping table can drift from the enum.
	const char* const aszLAYER_BLEND_NAMES[] = { "Override", "Additive" };
}

//=============================================================================
// Frame
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::Render(float fDtSeconds)
{
	// ★ CLEARED UNCONDITIONALLY, BEFORE THE EARLY RETURNS. A panel that is
	// hidden, collapsed or on an unselected dock tab draws nothing, and the one
	// thing it must not do is keep answering rect queries with last frame's
	// coordinates — that is a click into a window nobody can see.
	ClearFrameRects();

	if (!m_bShow)
	{
		// A gesture cannot survive the panel being hidden: the mouse-up that
		// would have ended it goes to whatever is on screen now.
		m_bDraggingNode = false;
		m_bDraggingTransition = false;
		m_strDraggingState.clear();
		m_strTransitionDragFrom.clear();
		return;
	}

	if (m_xDocument.IsOpen())
	{
		// An undo, a redo or an authoring step can change the state set without
		// going through the actions that rebuild the layout.
		if (m_xDocument.GetStateCount() != m_uAutoLayoutStateCount
			|| m_xDocument.GetSelectedMachineId() != m_uAutoLayoutMachineId)
		{
			RebuildAutoLayout();
		}
		if (m_bPreviewEnabled && fDtSeconds > 0.0f)
		{
			Action_TickPreview(fDtSeconds);
		}
	}

	if (m_bPlacementRequested)
	{
		ImGui::SetNextWindowPos(Vec(m_fPlacementX, m_fPlacementY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(Vec(m_fPlacementWidth, m_fPlacementHeight), ImGuiCond_Always);
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
		m_bPlacementRequested = false;
	}

	// ★ THE WINDOW TITLE IS THE BARE CONSTANT. DockBuilderDockWindow matches BY
	// NAME and ImHashStr hashes the whole string, so a title decorated with a
	// dirty marker or the asset name would dock nothing and the window would
	// silently float. Both are shown in the toolbar instead.
	const ImGuiWindowFlags uFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (!ImGui::Begin(szEDITOR_WINDOW_ANIM_STATE_MACHINE, &m_bShow, uFlags))
	{
		ImGui::End();
		return;
	}

	// One file read on the FOCUS TRANSITION, not per frame — the document's own
	// header says HasExternalModification costs a read and asks for exactly this.
	const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bFocused && !m_bWasFocused && m_xDocument.IsOpen())
	{
		m_bExternalConflict = m_xDocument.HasExternalModification();
	}
	m_bWasFocused = bFocused;

	RenderToolbar();

	const float fInspectorHeight = Zenith_EditorUI::Px(fANIMSM_INSPECTOR_HEIGHT_1X);

	// ★ WU-7.2's LAYER STRIP LIVES INSIDE THE SIDE CHILD, WHICH IS THE WHOLE
	// POINT OF PUTTING IT THERE. The canvas child below is sized
	// Vec(0, -fInspectorHeight) out of the MAIN window's remaining region, so
	// anything emitted into the main window before it comes straight out of the
	// canvas's height — the dope sheet's "NOTHING SHOWN DRAWS NOTHING" defect,
	// one panel over. Inside this fixed-width, fixed-height child a layer list of
	// any length costs the canvas nothing, which is what
	// `AnimSmPanel::TheLayerStripDrawsNothingWhenClosedAndNeverTakesCanvasHeight`
	// measures.
	ImGui::BeginChild("##AnimSmSide", Vec(Zenith_EditorUI::Px(fANIMSM_SIDE_PANEL_WIDTH_1X), -fInspectorHeight), true);
	RenderLayerStrip();
	ImGui::Separator();
	RenderParameterPanel();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##AnimSmCanvas", Vec(0.0f, -fInspectorHeight), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	RenderCanvas();
	ImGui::EndChild();

	RenderInspector();

	++m_uRenderedFrames;
	ImGui::End();
}

//=============================================================================
// The two drop targets, as ImGui-free verbs (WU-9.2).
//
// Declared in the header with the reason they exist: a headless unit cannot
// fabricate an ImGui drag, so everything a drop DECIDES lives here and the
// ImGui block is only the accept call and the pointer cast.
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::HandleControllerPathDrop(const char* szPayloadType, const char* szPath)
{
	if (szPayloadType == nullptr || szPath == nullptr)
	{
		return false;
	}
	if (strcmp(szPayloadType, DRAGDROP_PAYLOAD_ANIMCTRL) != 0)
	{
		return false;
	}
	const std::string strPath(szPath);
	if (!strPath.ends_with(ZENITH_ANIMCTRL_EXT))
	{
		// A payload id is a claim, not a proof — the extension is what the reader
		// on the other side of OpenAsset actually depends on.
		Zenith_Log(LOG_CATEGORY_EDITOR, "[AnimSM] drop refused: '%s' is not a " ZENITH_ANIMCTRL_EXT, szPath);
		return false;
	}

	// The buffer moves FIRST, so the field shows what was dropped whether or not
	// the open succeeds — a refusal with an empty box says nothing about which
	// file was refused.
	snprintf(m_acPathBuffer, sizeof(m_acPathBuffer), "%s", szPath);
	return OpenAsset(strPath);
}

bool Zenith_EditorPanel_AnimStateMachine::HandleLayerMaskDrop(u_int uLayerId, const char* szPayloadType,
	const char* szPath)
{
	if (szPayloadType == nullptr || szPath == nullptr)
	{
		return false;
	}
	if (strcmp(szPayloadType, DRAGDROP_PAYLOAD_ANIMMASK) != 0)
	{
		return false;
	}
	const std::string strPath(szPath);
	if (!strPath.ends_with(ZENITH_ANIMMASK_EXT))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[AnimSM] drop refused: '%s' is not a " ZENITH_ANIMMASK_EXT, szPath);
		return false;
	}

	// ★ THE ADDITIVE-LAYER RULE IS STILL Action_SetLayerMaskAssetPath'S. A drop is
	// a route to the verb, never a second copy of what the verb is allowed to do —
	// and the verb is what puts the refusal in GetLayerNotice() for the strip to
	// print. It also re-syncs m_acLayerMaskPathBuffer from the document, so the
	// buffer is deliberately NOT written here.
	return Action_SetLayerMaskAssetPath(uLayerId, strPath);
}

//=============================================================================
// Toolbar
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RenderToolbar()
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(320.0f));
	const bool bPathCommitted = ImGui::InputText("##AnimSmPath", m_acPathBuffer, sizeof(m_acPathBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);

	// ★ THE CONTROLLER FIELD MOVED OFF THE GENERIC PAYLOAD IN THE SAME COMMIT THAT
	// GAVE THE CONTENT BROWSER'S .zanimctrl ROW ITS OWN ID (WU-9.2). It used to
	// accept DRAGDROP_PAYLOAD_FILE_GENERIC, which is what a .zanimctrl row emitted;
	// the moment the row started emitting DRAGDROP_PAYLOAD_ANIMCTRL this target
	// would have stopped matching, and a drag that quietly does nothing is the
	// hardest kind of nothing to notice.
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMCTRL);
		if (pxPayload != nullptr && pxPayload->Data != nullptr)
		{
			const DragDropFilePayload* pxFile = static_cast<const DragDropFilePayload*>(pxPayload->Data);
			HandleControllerPathDrop(DRAGDROP_PAYLOAD_ANIMCTRL, pxFile->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();
	if (ImGui::Button("Open") || bPathCommitted)
	{
		OpenAsset(std::string(m_acPathBuffer));
	}
	ImGui::SameLine();
	if (ImGui::Button("New"))
	{
		OpenAssetFresh(std::string(m_acPathBuffer));
	}
	ImGui::SameLine();
	if (ImGui::Button("Close"))
	{
		RequestCloseAsset();
	}

	if (!m_xDocument.IsOpen())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(no controller open)");
		return;
	}

	Zenith_EditorUI::ToolbarSeparator();
	if (ImGui::Button("Save"))
	{
		Action_Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("Apply"))
	{
		Action_Apply();
	}
	ImGui::SameLine();
	bool bPreview = m_bPreviewEnabled;
	if (ImGui::Checkbox("Preview", &bPreview))
	{
		Action_SetPreviewEnabled(bPreview);
	}

	Zenith_EditorUI::ToolbarSeparator();
	ImGui::BeginDisabled(!m_xDocument.CanUndo());
	if (ImGui::Button("Undo")) { Action_Undo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!m_xDocument.CanRedo());
	if (ImGui::Button("Redo")) { Action_Redo(); }
	ImGui::EndDisabled();

	// The two read-only badges the title is deliberately not allowed to carry.
	if (m_xDocument.IsDirty())
	{
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning), "UNSAVED");
	}
	if (m_bExternalConflict)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xPalette.m_uError), "CHANGED ON DISK");
	}
	if (m_bCloseRefusedDirty)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning),
			"unsaved edits — Save, or Close again to discard");
	}
	if (!m_bPreviewComplete)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xPalette.m_uError),
			"preview incomplete (a clip or mask did not resolve)");
	}
}

//=============================================================================
// The LAYERS strip (WU-7.2) — the machine picker AND the layer list, because
// they were always one question: "which machine am I editing" is answered by
// picking a row, and a row carries everything the layer is.
//
// ★ IT DRAWS NOTHING WITH NO DOCUMENT OPEN — not a header, not a disabled row,
// not the word "Layers". The dope sheet paid for that rule twice (Editor/
// CLAUDE.md → "NOTHING SHOWN DRAWS NOTHING"), and while this panel's canvas is a
// fixed-height child rather than a "whatever is left" one, the rule costs
// nothing to keep and the alternative is a height regression that arrives as a
// flat `false` from a rect accessor a long way from its cause.
//
// ★ THE LIST HAS ITS OWN SCROLL CHILD, which is the graph editor's hard-won
// lesson rather than decoration: sharing a scroll region between a list and the
// controls under it means scrolling to a row pushes the controls off the top
// (that panel observed y=-2488), and a clipped ImGui item is not interactable.
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RenderLayerStrip()
{
	if (!m_xDocument.IsOpen())
	{
		return;   // ★ before a single item is submitted
	}
	m_bLayerStripDrawn = true;

	ImGui::TextUnformatted("Layers");

	const u_int uSelected = m_xDocument.GetSelectedMachineId();

	ImGui::BeginChild("##AnimSmLayerList", Vec(0.0f, Zenith_EditorUI::Px(fANIMSM_LAYER_LIST_HEIGHT_1X)), true);

	// The def's OWN machine, listed first and marked as what it is. It is not a
	// layer — no weight, no blend mode, no mask — so it carries no controls.
	if (ImGui::Selectable("Top-level machine", uSelected == uANIMCTRL_TOP_LEVEL_MACHINE))
	{
		Action_SelectLayerMachine(uANIMCTRL_TOP_LEVEL_MACHINE);
	}

	Zenith_Vector<u_int> auLayerIds;
	m_xDocument.GetLayerIds(auLayerIds);
	for (u_int u = 0; u < auLayerIds.GetSize(); ++u)
	{
		const u_int uId = auLayerIds.Get(u);
		std::string strName;
		m_xDocument.GetLayerName(uId, strName);
		float fWeight = 1.0f;
		m_xDocument.GetLayerWeight(uId, fWeight);
		Flux_LayerBlendMode eMode = LAYER_BLEND_OVERRIDE;
		m_xDocument.GetLayerBlendMode(uId, eMode);

		ImGui::PushID(static_cast<int>(3000 + u));
		char acRow[224];
		// The INDEX is shown because it is the blend ORDER, which is what the
		// runtime composes in and what the reorder buttons change; the id is
		// shown because it is what every verb and every recipe types.
		snprintf(acRow, sizeof(acRow), "%u. %s  [id %u]  %s  w=%.2f",
			u, strName.empty() ? "(unnamed)" : strName.c_str(), uId,
			eMode == LAYER_BLEND_ADDITIVE ? "additive" : "override", fWeight);
		if (ImGui::Selectable(acRow, uSelected == uId))
		{
			Action_SelectLayer(uId);
		}
		ImGui::PopID();
		++m_uDrawnLayerRows;
	}
	ImGui::EndChild();

	// ---- add ----------------------------------------------------------------
	ImGui::SetNextItemWidth(-Zenith_EditorUI::Px(76.0f));
	ImGui::InputText("##AnimSmLayerName", m_acLayerNameBuffer, sizeof(m_acLayerNameBuffer));
	ImGui::SameLine();
	if (ImGui::Button("+ Layer"))
	{
		Action_AddLayer(std::string(m_acLayerNameBuffer));
	}

	// ---- the selected layer's own controls -----------------------------------
	// ★ RE-READ, NOT uSelected. A click on a row above, or the "+ Layer" button,
	// has already changed the selection THIS frame — drawing the detail block for
	// the previous one would show a name field belonging to a layer the user has
	// just stopped looking at, and a commit from it would edit that layer.
	const u_int uSelectedNow = m_xDocument.GetSelectedMachineId();
	if (uSelectedNow != uANIMCTRL_TOP_LEVEL_MACHINE)
	{
		RenderLayerDetail(uSelectedNow);
	}
}

void Zenith_EditorPanel_AnimStateMachine::RenderLayerDetail(u_int uLayerId)
{
	u_int uIndex = 0;
	if (!m_xDocument.GetLayerIndex(uLayerId, uIndex))
	{
		return;
	}

	ImGui::Separator();

	// ---- name (commit on edit-complete) --------------------------------------
	// ★ EDIT-COMPLETE, NOT PER KEYSTROKE, exactly as the transition inspector's
	// duration field is: a command per character makes Ctrl+Z walk backwards
	// through a half-typed name one letter at a time.
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##AnimSmLayerRename", m_acLayerRenameBuffer, sizeof(m_acLayerRenameBuffer));
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Action_RenameLayer(uLayerId, std::string(m_acLayerRenameBuffer));
	}

	// ---- weight (commit on edit-complete) ------------------------------------
	float fWeight = 1.0f;
	m_xDocument.GetLayerWeight(uLayerId, fWeight);
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::SliderFloat("##AnimSmLayerWeight", &fWeight, 0.0f, 1.0f, "weight %.3f");
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Action_SetLayerWeight(uLayerId, fWeight);
	}

	// ---- blend mode ----------------------------------------------------------
	Flux_LayerBlendMode eMode = LAYER_BLEND_OVERRIDE;
	m_xDocument.GetLayerBlendMode(uLayerId, eMode);
	int iMode = (eMode == LAYER_BLEND_ADDITIVE) ? 1 : 0;
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::Combo("##AnimSmLayerBlend", &iMode, aszLAYER_BLEND_NAMES, 2))
	{
		Action_SetLayerBlendMode(uLayerId, iMode == 1 ? LAYER_BLEND_ADDITIVE : LAYER_BLEND_OVERRIDE);
	}

	// ---- emit events (D36) ---------------------------------------------------
	bool bEmit = true;
	m_xDocument.GetLayerEmitEvents(uLayerId, bEmit);
	if (ImGui::Checkbox("Emit animation events", &bEmit))
	{
		Action_SetLayerEmitEvents(uLayerId, bEmit);
	}

	// ---- bone mask -----------------------------------------------------------
	// ★ DISABLED, WITH THE NOTICE, ON AN ADDITIVE LAYER. The rule is
	// Zenith_BoneMaskDocument::LayerAcceptsMask's and is not restated here; the
	// wording is AdditiveLayerMaskNotice()'s, so this strip, the dope sheet's
	// mask sub-panel and the units cannot describe the refusal three ways.
	const bool bAcceptsMask = Zenith_BoneMaskDocument::LayerAcceptsMask(eMode);
	ImGui::BeginDisabled(!bAcceptsMask);
	ImGui::SetNextItemWidth(-Zenith_EditorUI::Px(60.0f));
	ImGui::InputText("##AnimSmLayerMask", m_acLayerMaskPathBuffer, sizeof(m_acLayerMaskPathBuffer));
	if (bAcceptsMask && ImGui::BeginDragDropTarget())
	{
		// No registry enumeration of .zanimmask files exists, so the route is the
		// path field plus a drop from the content browser — which since WU-9.2
		// emits DRAGDROP_PAYLOAD_ANIMMASK for that row. ★ ONLY that id: the field
		// used to take the GENERIC file payload and write whatever it carried into
		// the buffer unvalidated, so a .zscen dropped here became a layer's bone
		// mask path, saved into the .zanimctrl, and resolved to nothing at runtime.
		const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMMASK);
		if (pxPayload != nullptr && pxPayload->Data != nullptr)
		{
			const DragDropFilePayload* pxFile = static_cast<const DragDropFilePayload*>(pxPayload->Data);
			HandleLayerMaskDrop(uLayerId, DRAGDROP_PAYLOAD_ANIMMASK, pxFile->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::SameLine();
	if (ImGui::Button("Mask"))
	{
		Action_SetLayerMaskAssetPath(uLayerId, std::string(m_acLayerMaskPathBuffer));
	}
	ImGui::EndDisabled();
	if (!bAcceptsMask)
	{
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Zenith_EditorUI::Palette().m_uWarning),
			"%s", Zenith_BoneMaskDocument::AdditiveLayerMaskNotice());
	}

	// ---- reorder + remove ----------------------------------------------------
	// The blend ORDER is what these change, so they move the layer by INDEX while
	// naming it by ID — the one place an index legitimately appears.
	ImGui::BeginDisabled(uIndex == 0);
	if (ImGui::Button("Up"))
	{
		Action_MoveLayer(uLayerId, uIndex - 1u);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(uIndex + 1u >= m_xDocument.GetLayerCount());
	if (ImGui::Button("Down"))
	{
		Action_MoveLayer(uLayerId, uIndex + 1u);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Remove Layer"))
	{
		Action_RemoveLayer(uLayerId);
		return;   // the list just moved under us
	}
}

//=============================================================================
// Parameters + the clip list
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RenderParameterPanel()
{
	if (!m_xDocument.IsOpen())
	{
		return;
	}

	ImGui::TextUnformatted("Parameters");

	Zenith_Vector<std::string> axNames;
	m_xDocument.GetParameterNamesSorted(axNames);
	for (u_int u = 0; u < axNames.GetSize(); ++u)
	{
		Zenith_AnimCtrlParameterDecl xDecl;
		if (!m_xDocument.GetParameter(axNames.Get(u), xDecl))
		{
			continue;
		}
		ImGui::PushID(static_cast<int>(u));
		char acLabel[224];
		switch (xDecl.m_eType)
		{
		case Flux_AnimationParameters::ParamType::Float:
			snprintf(acLabel, sizeof(acLabel), "%s : Float = %.3f", xDecl.m_strName.c_str(), xDecl.m_fDefault);
			break;
		case Flux_AnimationParameters::ParamType::Int:
			snprintf(acLabel, sizeof(acLabel), "%s : Int = %d", xDecl.m_strName.c_str(), xDecl.m_iDefault);
			break;
		case Flux_AnimationParameters::ParamType::Bool:
			snprintf(acLabel, sizeof(acLabel), "%s : Bool = %s", xDecl.m_strName.c_str(), xDecl.m_bDefault ? "true" : "false");
			break;
		default:
			snprintf(acLabel, sizeof(acLabel), "%s : Trigger", xDecl.m_strName.c_str());
			break;
		}
		ImGui::TextUnformatted(acLabel);
		ImGui::SameLine();
		if (ImGui::SmallButton("x"))
		{
			Action_RemoveParameter(xDecl.m_strName);
			ImGui::PopID();
			break;   // the list just moved under us
		}

		// The LIVE value, while a preview is running. Read-write: driving a
		// parameter is how an author sees their own condition fire.
		if (m_bPreviewEnabled)
		{
			ImGui::SameLine();
			switch (xDecl.m_eType)
			{
			case Flux_AnimationParameters::ParamType::Float:
			{
				float fLive = m_xPreviewController.GetParameters().GetFloat(xDecl.m_strName);
				ImGui::SetNextItemWidth(Zenith_EditorUI::Px(70.0f));
				if (ImGui::DragFloat("##live", &fLive, 0.01f))
				{
					Action_SetPreviewFloat(xDecl.m_strName, fLive);
				}
				break;
			}
			case Flux_AnimationParameters::ParamType::Int:
			{
				int iLive = static_cast<int>(m_xPreviewController.GetParameters().GetInt(xDecl.m_strName));
				ImGui::SetNextItemWidth(Zenith_EditorUI::Px(70.0f));
				if (ImGui::DragInt("##live", &iLive))
				{
					Action_SetPreviewInt(xDecl.m_strName, static_cast<int32_t>(iLive));
				}
				break;
			}
			case Flux_AnimationParameters::ParamType::Bool:
			{
				bool bLive = m_xPreviewController.GetParameters().GetBool(xDecl.m_strName);
				if (ImGui::Checkbox("##live", &bLive))
				{
					Action_SetPreviewBool(xDecl.m_strName, bLive);
				}
				break;
			}
			default:
				if (ImGui::SmallButton("fire"))
				{
					Action_SetPreviewTrigger(xDecl.m_strName);
				}
				break;
			}
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
	ImGui::InputText("##AnimSmNewParam", m_acParameterNameBuffer, sizeof(m_acParameterNameBuffer));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(74.0f));
	ImGui::Combo("##AnimSmNewParamType", &m_iNewParameterType, aszPARAM_TYPE_NAMES, 4);
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
	ImGui::InputFloat("##AnimSmNewParamDefault", &m_fNewParameterDefault);
	ImGui::SameLine();
	if (ImGui::Button("Add Parameter"))
	{
		Action_AddParameter(std::string(m_acParameterNameBuffer),
			static_cast<Flux_AnimationParameters::ParamType>(m_iNewParameterType), m_fNewParameterDefault);
		m_acParameterNameBuffer[0] = '\0';
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Clips");
	for (u_int u = 0; u < m_xDocument.GetClipPathCount(); ++u)
	{
		std::string strPath;
		if (!m_xDocument.GetClipPathAt(u, strPath))
		{
			continue;
		}
		ImGui::PushID(static_cast<int>(1000 + u));
		ImGui::TextUnformatted(strPath.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("x"))
		{
			Action_RemoveClipPath(strPath);
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
	ImGui::SetNextItemWidth(-Zenith_EditorUI::Px(48.0f));
	ImGui::InputText("##AnimSmNewClip", m_acClipPathBuffer, sizeof(m_acClipPathBuffer));
	ImGui::SameLine();
	if (ImGui::Button("Add"))
	{
		Action_AddClipPath(std::string(m_acClipPathBuffer));
		m_acClipPathBuffer[0] = '\0';
	}
}

//=============================================================================
// The canvas
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RenderCanvas()
{
	const ImVec2 xOrigin = ImGui::GetCursorScreenPos();
	const ImVec2 xAvail = ImGui::GetContentRegionAvail();
	if (xAvail.x < 8.0f || xAvail.y < 8.0f)
	{
		return;
	}

	// ★ ONE InvisibleButton FOR THE WHOLE CANVAS. Everything below is a
	// decoration on the window's draw list.
	ImGui::InvisibleButton("##AnimSmCanvasSurface", xAvail);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	CanvasLayout xLayout;
	xLayout.m_fLeft = xOrigin.x;
	xLayout.m_fTop = xOrigin.y;
	xLayout.m_fRight = xOrigin.x + xAvail.x;
	xLayout.m_fBottom = xOrigin.y + xAvail.y;
	xLayout.m_fNodeWidth = Zenith_EditorUI::Px(fANIMSM_NODE_WIDTH_1X);
	xLayout.m_fNodeHeight = Zenith_EditorUI::Px(fANIMSM_NODE_HEIGHT_1X);

	m_xCanvasRect.m_fMinX = xLayout.m_fLeft;
	m_xCanvasRect.m_fMinY = xLayout.m_fTop;
	m_xCanvasRect.m_fMaxX = xLayout.m_fRight;
	m_xCanvasRect.m_fMaxY = xLayout.m_fBottom;
	m_bCanvasRectValid = true;

	// ★ THE DISPLAY BOUND IS CAPTURED HERE, WITH THE RECTS. See PublishRect.
	const ImGuiIO& xIO = ImGui::GetIO();
	m_fRecordedDisplayWidth = xIO.DisplaySize.x;
	m_fRecordedDisplayHeight = xIO.DisplaySize.y;

	ApplyPendingScroll(xLayout);

	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	pxDraw->PushClipRect(Vec(xLayout.m_fLeft, xLayout.m_fTop), Vec(xLayout.m_fRight, xLayout.m_fBottom), true);
	DrawCanvasBackground(pxDraw, xLayout);
	// Edges first so the boxes sit on top of them, and because the edge pass is
	// what records the owner order every transition rect key is built from.
	DrawTransitions(pxDraw, xLayout);
	DrawNodes(pxDraw, xLayout);
	pxDraw->PopClipRect();

	HandleCanvasInput(xLayout, bCanvasHovered);
}

void Zenith_EditorPanel_AnimStateMachine::ApplyPendingScroll(const CanvasLayout& xLayout)
{
	if (!m_bPendingScrollToState)
	{
		return;
	}
	m_bPendingScrollToState = false;

	Zenith_Maths::Vector2 xGraph(0.0f);
	if (!GetNodePosition(m_strPendingScrollState, xGraph))
	{
		return;
	}
	const float fScale = Zenith_EditorUI::GetUIScale();
	const float fHalfW = (xLayout.m_fRight - xLayout.m_fLeft) * 0.5f / fScale;
	const float fHalfH = (xLayout.m_fBottom - xLayout.m_fTop) * 0.5f / fScale;
	m_fScrollX = xGraph.x - fHalfW + fANIMSM_NODE_WIDTH_1X * 0.5f;
	m_fScrollY = xGraph.y - fHalfH + fANIMSM_NODE_HEIGHT_1X * 0.5f;
}

void Zenith_EditorPanel_AnimStateMachine::DrawCanvasBackground(ImDrawList* pxDraw, const CanvasLayout& xLayout)
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	pxDraw->AddRectFilled(Vec(xLayout.m_fLeft, xLayout.m_fTop), Vec(xLayout.m_fRight, xLayout.m_fBottom),
		xPalette.m_uPanelBgAlt);

	const float fStep = Zenith_EditorUI::Px(fANIMSM_GRID_STEP_1X);
	if (fStep < 4.0f)
	{
		return;
	}
	const float fScale = Zenith_EditorUI::GetUIScale();
	const float fOffsetX = std::fmod(-m_fScrollX * fScale, fStep);
	const float fOffsetY = std::fmod(-m_fScrollY * fScale, fStep);
	for (float fX = xLayout.m_fLeft + fOffsetX; fX < xLayout.m_fRight; fX += fStep)
	{
		pxDraw->AddLine(Vec(fX, xLayout.m_fTop), Vec(fX, xLayout.m_fBottom), xPalette.m_uBorder);
	}
	for (float fY = xLayout.m_fTop + fOffsetY; fY < xLayout.m_fBottom; fY += fStep)
	{
		pxDraw->AddLine(Vec(xLayout.m_fLeft, fY), Vec(xLayout.m_fRight, fY), xPalette.m_uBorder);
	}
}

void Zenith_EditorPanel_AnimStateMachine::DrawTransitions(ImDrawList* pxDraw, const CanvasLayout& xLayout)
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();

	Zenith_Vector<std::string> axStates;
	m_xDocument.GetStateNamesSorted(axStates);

	// ★ THE OWNER ORDER IS RECORDED, NOT RE-DERIVED. GetTransitionMidpointRect
	// looks a from-state up in this list to rebuild the key the draw used, so a
	// state added between the draw and the query cannot silently shift the key
	// and hand out a different edge's rect.
	//
	// ★ AND THE ANY-STATE LIST IS OWNER 0 — THIS PUSH IS THE WHOLE SHIFT. An
	// EMPTY from-state addresses it on every document verb, so pushing "" here
	// makes owner 0 the any-state list and moves every state's index up by one in
	// the ONE place indices are assigned. Both readers (GetTransitionMidpointRect
	// and FindTransitionAtScreenPos) resolve an owner by scanning this vector BY
	// NAME, so they follow for free; adding a `+ 1` to either would double-shift
	// every key and hand out the wrong edge's rect.
	m_axRectOwnerOrder.Clear();
	m_axRectOwnerOrder.PushBack(std::string());
	for (u_int u = 0; u < axStates.GetSize(); ++u)
	{
		m_axRectOwnerOrder.PushBack(axStates.Get(u));
	}

	const float fEdgeHalf = Zenith_EditorUI::Px(fANIMSM_EDGE_GRAB_1X);

	for (u_int uOwner = 0; uOwner < m_axRectOwnerOrder.GetSize(); ++uOwner)
	{
		const std::string& strFrom = m_axRectOwnerOrder.Get(uOwner);
		Zenith_AnimCtrlPanelRect xFromRect;
		if (!ComputeNodeScreenRect(xLayout, strFrom, xFromRect))
		{
			continue;
		}

		const u_int uCount = m_xDocument.GetTransitionCount(strFrom);
		for (u_int uIndex = 0; uIndex < uCount; ++uIndex)
		{
			Flux_StateTransition xTransition;
			if (!m_xDocument.GetTransition(strFrom, uIndex, xTransition))
			{
				continue;
			}
			Zenith_AnimCtrlPanelRect xToRect;
			if (!ComputeNodeScreenRect(xLayout, xTransition.m_strTargetStateName, xToRect))
			{
				// A transition to a state that is not in this machine is a
				// DANGLING one; it is drawn as a stub off the source's right
				// edge rather than skipped, so a def carrying one is visible
				// instead of merely wrong at runtime.
				const Zenith_Maths::Vector2 xCentre = xFromRect.Centre();
				pxDraw->AddLine(Vec(xCentre.x, xCentre.y),
					Vec(xFromRect.m_fMaxX + xLayout.m_fNodeWidth * 0.35f, xCentre.y), xPalette.m_uError, 2.0f);
				continue;
			}

			const Zenith_Maths::Vector2 xA = xFromRect.Centre();
			const Zenith_Maths::Vector2 xB = xToRect.Centre();
			const bool bSelected = m_bHasTransitionSelection
				&& m_strSelectedTransitionFrom == strFrom && m_uSelectedTransition == uIndex;
			const ImU32 uColour = bSelected ? xPalette.m_uAccent : xPalette.m_uTextDim;
			pxDraw->AddLine(Vec(xA.x, xA.y), Vec(xB.x, xB.y), uColour, bSelected ? 3.0f : 2.0f);

			// The midpoint marker: the click target, and where the condition
			// count is shown.
			const float fMidX = (xA.x + xB.x) * 0.5f;
			const float fMidY = (xA.y + xB.y) * 0.5f;
			pxDraw->AddCircleFilled(Vec(fMidX, fMidY), fEdgeHalf * 0.6f, uColour);

			char acBadge[32];
			snprintf(acBadge, sizeof(acBadge), "%u", xTransition.m_xConditions.GetSize());
			pxDraw->AddText(Vec(fMidX + fEdgeHalf, fMidY - fEdgeHalf), xPalette.m_uText, acBadge);

			Zenith_AnimCtrlPanelRect xMid;
			xMid.m_fMinX = fMidX - fEdgeHalf;
			xMid.m_fMinY = fMidY - fEdgeHalf;
			xMid.m_fMaxX = fMidX + fEdgeHalf;
			xMid.m_fMaxY = fMidY + fEdgeHalf;
			// Recorded only when it was actually painted INSIDE the canvas —
			// the other half of the off-screen contract, and the half PublishRect
			// cannot supply.
			if (fMidX >= xLayout.m_fLeft && fMidX <= xLayout.m_fRight
				&& fMidY >= xLayout.m_fTop && fMidY <= xLayout.m_fBottom)
			{
				m_xTransitionRects.Insert(MakeTransitionRectKey(uOwner, uIndex), xMid);
			}
		}
	}
}

void Zenith_EditorPanel_AnimStateMachine::DrawNodes(ImDrawList* pxDraw, const CanvasLayout& xLayout)
{
	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const std::string& strDefault = m_xDocument.GetDefaultStateName();
	const std::string& strHighlight = GetHighlightedStateName();

	Zenith_Vector<std::string> axStates;
	m_xDocument.GetStateNamesSorted(axStates);

	for (u_int u = 0; u < axStates.GetSize(); ++u)
	{
		const std::string& strName = axStates.Get(u);
		Zenith_AnimCtrlPanelRect xRect;
		if (!ComputeNodeScreenRect(xLayout, strName, xRect))
		{
			continue;
		}
		// Culled entirely outside the canvas: not painted, and therefore not
		// recorded.
		if (xRect.m_fMaxX < xLayout.m_fLeft || xRect.m_fMinX > xLayout.m_fRight
			|| xRect.m_fMaxY < xLayout.m_fTop || xRect.m_fMinY > xLayout.m_fBottom)
		{
			continue;
		}

		const bool bSelected = (m_strSelectedState == strName);
		const bool bDefault = (strDefault == strName);
		const bool bLive = (!strHighlight.empty() && strHighlight == strName);

		pxDraw->AddRectFilled(Vec(xRect.m_fMinX, xRect.m_fMinY), Vec(xRect.m_fMaxX, xRect.m_fMaxY),
			bDefault ? xPalette.m_uAccentDim : xPalette.m_uFrame, 4.0f);
		pxDraw->AddRect(Vec(xRect.m_fMinX, xRect.m_fMinY), Vec(xRect.m_fMaxX, xRect.m_fMaxY),
			bSelected ? xPalette.m_uAccent : xPalette.m_uBorder, 4.0f, 0, bSelected ? 2.5f : 1.0f);
		if (bLive)
		{
			// The LIVE ring — drawn outside the box so it cannot be mistaken for
			// the selection outline.
			pxDraw->AddRect(Vec(xRect.m_fMinX - 3.0f, xRect.m_fMinY - 3.0f),
				Vec(xRect.m_fMaxX + 3.0f, xRect.m_fMaxY + 3.0f), xPalette.m_uPlay, 6.0f, 0, 2.5f);
		}

		pxDraw->AddText(Vec(xRect.m_fMinX + 8.0f, xRect.m_fMinY + 6.0f), xPalette.m_uTextBright, strName.c_str());

		// The second line says what the state PLAYS, and names the refusal when
		// the tree is one nothing here can edit.
		char acSubtitle[192];
		const Zenith_AnimCtrlStateTreeKind eKind = m_xDocument.GetStateTreeKind(strName);
		if (eKind == ZENITH_ANIMCTRL_TREE_COMPLEX)
		{
			snprintf(acSubtitle, sizeof(acSubtitle), "%s", BlendTreeRefusalText());
		}
		else if (eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D || eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D)
		{
			// WU-7.3: a blend space says what it IS and how big it is. The point
			// COUNT is the one number that distinguishes "a space somebody is
			// building" from "an empty space that poses the bind pose".
			snprintf(acSubtitle, sizeof(acSubtitle), "blend space %s (%u points)",
				eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D ? "1D" : "2D",
				m_xDocument.GetBlendPointCount(strName));
		}
		else
		{
			std::string strClip;
			if (m_xDocument.GetStateClipName(strName, strClip) && !strClip.empty())
			{
				snprintf(acSubtitle, sizeof(acSubtitle), "%s", strClip.c_str());
			}
			else
			{
				snprintf(acSubtitle, sizeof(acSubtitle), "(no clip)");
			}
		}
		pxDraw->AddText(Vec(xRect.m_fMinX + 8.0f, xRect.m_fMinY + 24.0f),
			eKind == ZENITH_ANIMCTRL_TREE_COMPLEX ? xPalette.m_uWarning : xPalette.m_uTextDim, acSubtitle);

		m_xNodeRects.Insert(strName, xRect);
	}

	// ---- the ANY-STATE pseudo-node ------------------------------------------
	//
	// ★ ALWAYS DRAWN, WHATEVER THE MACHINE HOLDS. It is the only handle an
	// any-state transition can be started from, so a node that appeared once the
	// list was non-empty would make the first one unauthorable on the canvas —
	// which is the residual this unit exists to remove.
	//
	// ★ PAINTED LAST, SO THE PAINT ORDER MATCHES THE HIT ORDER.
	// FindStateAtScreenPos checks it BEFORE the state nodes; drawing it under
	// them would give a state node that overlapped it the pixels while the
	// pseudo-node took the clicks.
	//
	// ★ AND IT IS NOT PUT IN m_xNodeRects. See GetAnyStateRect: GetDrawnNodeCount
	// counts state nodes, and a scroll-independent entry in that map would make it
	// never reach zero.
	Zenith_AnimCtrlPanelRect xAnyRect;
	if (ComputeNodeScreenRect(xLayout, std::string(), xAnyRect))
	{
		// The same "painted inside the canvas" cull every state node runs through.
		// A canvas too small to hold the box is the only way this fails, and the
		// contract is the same either way: what was not painted is not published.
		if (!(xAnyRect.m_fMaxX < xLayout.m_fLeft || xAnyRect.m_fMinX > xLayout.m_fRight
			|| xAnyRect.m_fMaxY < xLayout.m_fTop || xAnyRect.m_fMinY > xLayout.m_fBottom))
		{
			pxDraw->AddRectFilled(Vec(xAnyRect.m_fMinX, xAnyRect.m_fMinY), Vec(xAnyRect.m_fMaxX, xAnyRect.m_fMaxY),
				xPalette.m_uPanelBg, 4.0f);
			pxDraw->AddRect(Vec(xAnyRect.m_fMinX, xAnyRect.m_fMinY), Vec(xAnyRect.m_fMaxX, xAnyRect.m_fMaxY),
				m_bAnyStateSelected ? xPalette.m_uAccent : xPalette.m_uTextDim, 4.0f, 0,
				m_bAnyStateSelected ? 2.5f : 1.0f);
			pxDraw->AddText(Vec(xAnyRect.m_fMinX + 8.0f, xAnyRect.m_fMinY + 6.0f), xPalette.m_uTextBright,
				szANIMSM_ANY_STATE_LABEL);

			char acAnySubtitle[64];
			snprintf(acAnySubtitle, sizeof(acAnySubtitle), "%u transition(s)",
				m_xDocument.GetTransitionCount(std::string()));
			pxDraw->AddText(Vec(xAnyRect.m_fMinX + 8.0f, xAnyRect.m_fMinY + 24.0f), xPalette.m_uTextDim,
				acAnySubtitle);

			m_xAnyStateRect = xAnyRect;
			m_bAnyStateRectValid = true;
		}
	}
}

//=============================================================================
// Input translation — the ONLY functions here that read ImGui state, and every
// one of them ends in an Action_*.
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::FindStateAtScreenPos(float fX, float fY, std::string& strOut) const
{
	// ★ THE ANY-STATE PSEUDO-NODE IS CHECKED FIRST, AND EXPLICITLY. It is not in
	// m_xNodeRects (see GetAnyStateRect), so nothing below could ever find it; and
	// it is checked BEFORE the state scan because that scan runs in
	// Zenith_HashMap's slot order, which would make "who wins an overlap" depend on
	// a hash. It answers the EMPTY name — the same name every transition verb takes
	// for the any-state list — so the Ctrl-drag and the selection below need no
	// second vocabulary.
	if (m_bAnyStateRectValid
		&& fX >= m_xAnyStateRect.m_fMinX && fX <= m_xAnyStateRect.m_fMaxX
		&& fY >= m_xAnyStateRect.m_fMinY && fY <= m_xAnyStateRect.m_fMaxY)
	{
		strOut.clear();
		return true;
	}

	for (Zenith_HashMap<std::string, Zenith_AnimCtrlPanelRect>::Iterator xIt(m_xNodeRects); !xIt.Done(); xIt.Next())
	{
		const Zenith_AnimCtrlPanelRect& xRect = xIt.GetValue();
		if (fX >= xRect.m_fMinX && fX <= xRect.m_fMaxX && fY >= xRect.m_fMinY && fY <= xRect.m_fMaxY)
		{
			strOut = xIt.GetKey();
			return true;
		}
	}
	return false;
}

bool Zenith_EditorPanel_AnimStateMachine::FindTransitionAtScreenPos(float fX, float fY,
	std::string& strOutFrom, u_int& uOutIndex) const
{
	for (Zenith_HashMap<u_int64, Zenith_AnimCtrlPanelRect>::Iterator xIt(m_xTransitionRects); !xIt.Done(); xIt.Next())
	{
		const Zenith_AnimCtrlPanelRect& xRect = xIt.GetValue();
		if (fX >= xRect.m_fMinX && fX <= xRect.m_fMaxX && fY >= xRect.m_fMinY && fY <= xRect.m_fMaxY)
		{
			const u_int uOwner = static_cast<u_int>(xIt.GetKey() >> 32);
			if (uOwner >= m_axRectOwnerOrder.GetSize())
			{
				return false;
			}
			strOutFrom = m_axRectOwnerOrder.Get(uOwner);
			uOutIndex = static_cast<u_int>(xIt.GetKey() & 0xFFFFFFFFull);
			return true;
		}
	}
	return false;
}

void Zenith_EditorPanel_AnimStateMachine::HandleCanvasInput(const CanvasLayout& xLayout, bool bCanvasHovered)
{
	const ImGuiIO& xIO = ImGui::GetIO();
	const float fMouseX = xIO.MousePos.x;
	const float fMouseY = xIO.MousePos.y;
	const float fScale = Zenith_EditorUI::GetUIScale();

	// ---- middle-drag pans -----------------------------------------------
	if (bCanvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		const ImVec2 xDelta = ImGui::GetIO().MouseDelta;
		m_fScrollX -= xDelta.x / fScale;
		m_fScrollY -= xDelta.y / fScale;
	}

	// ---- press ------------------------------------------------------------
	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		std::string strState;
		if (FindStateAtScreenPos(fMouseX, fMouseY, strState))
		{
			// An EMPTY name is the any-state pseudo-node, and it has its own
			// selection flag: Action_SelectState("") would refuse (the machine has
			// no state called ""), leaving the click doing nothing at all.
			if (strState.empty())
			{
				Action_SelectAnyState();
			}
			else
			{
				Action_SelectState(strState);
			}
			if (xIO.KeyCtrl)
			{
				// Ctrl-drag from a node draws a TRANSITION rather than moving it.
				// From the pseudo-node that is strState == "", which is exactly
				// what Action_AddTransition takes for the any-state list.
				m_bDraggingTransition = true;
				m_strTransitionDragFrom = strState;
			}
			else
			{
				// ★ AND THE PLAIN DRAG NEVER STARTS FOR THE PSEUDO-NODE. Its box is
				// canvas-anchored and there is no verb that could store a position
				// for it — GetNodePosition("") answers false, which is the gate, and
				// it is a gate on purpose rather than an accident to be tidied away.
				Zenith_Maths::Vector2 xGraph(0.0f);
				if (GetNodePosition(strState, xGraph))
				{
					m_bDraggingNode = true;
					m_strDraggingState = strState;
					m_xDragStartPosition = xGraph;
					Zenith_AnimCtrlPanelRect xRect;
					if (ComputeNodeScreenRect(xLayout, strState, xRect))
					{
						m_xDragGrabOffset = Zenith_Maths::Vector2(fMouseX - xRect.m_fMinX, fMouseY - xRect.m_fMinY);
					}
				}
			}
		}
		else
		{
			std::string strFrom;
			u_int uIndex = 0;
			if (FindTransitionAtScreenPos(fMouseX, fMouseY, strFrom, uIndex))
			{
				Action_SelectTransition(strFrom, uIndex);
			}
			else
			{
				Action_ClearSelection();
			}
		}
	}

	// ---- drag preview -----------------------------------------------------
	if (m_bDraggingTransition)
	{
		Zenith_AnimCtrlPanelRect xFromRect;
		if (ComputeNodeScreenRect(xLayout, m_strTransitionDragFrom, xFromRect))
		{
			const Zenith_Maths::Vector2 xCentre = xFromRect.Centre();
			ImGui::GetWindowDrawList()->AddLine(Vec(xCentre.x, xCentre.y), Vec(fMouseX, fMouseY),
				Zenith_EditorUI::Palette().m_uAccentHover, 2.0f);
		}
	}

	// ---- release ----------------------------------------------------------
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (m_bDraggingTransition)
		{
			std::string strTarget;
			// ★ A DROP ONTO THE PSEUDO-NODE IS REFUSED, AND THE REFUSAL IS WRITTEN
			// HERE RATHER THAN LEFT TO THE DOCUMENT. AddTransition(from, "") already
			// returns false — nothing transitions INTO the any-state list, it is a
			// source only — but relying on that would make an empty target look like
			// a target that was simply not found, and the next reader would "fix"
			// the emptiness check.
			if (FindStateAtScreenPos(fMouseX, fMouseY, strTarget) && !strTarget.empty())
			{
				Action_AddTransition(m_strTransitionDragFrom, strTarget);
			}
			m_bDraggingTransition = false;
			m_strTransitionDragFrom.clear();
		}
		else if (m_bDraggingNode)
		{
			// ★ ONE DRAG IS ONE UNDO STEP, whatever it spanned, and a click that
			// never moved records nothing — Zenith_Editor::RecordGizmoDragUndo's
			// shape. Nothing was written between press and release: the node
			// followed the cursor through the LIVE position below, and the
			// document sees exactly one SetStateEditorPosition here.
			const float fNewX = (fMouseX - m_xDragGrabOffset.x - xLayout.m_fLeft) / fScale + m_fScrollX;
			const float fNewY = (fMouseY - m_xDragGrabOffset.y - xLayout.m_fTop) / fScale + m_fScrollY;
			const float fSlop = fANIMSM_CLICK_SLOP_1X;
			if (IsFiniteFloat(fNewX) && IsFiniteFloat(fNewY)
				&& (std::fabs(fNewX - m_xDragStartPosition.x) > fSlop
				 || std::fabs(fNewY - m_xDragStartPosition.y) > fSlop))
			{
				Action_SetStatePosition(m_strDraggingState, fNewX, fNewY);
			}
			m_bDraggingNode = false;
			m_strDraggingState.clear();
		}
	}

	// ---- right-click on a node --------------------------------------------
	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		std::string strState;
		if (FindStateAtScreenPos(fMouseX, fMouseY, strState))
		{
			if (strState.empty())
			{
				Action_SelectAnyState();
			}
			else
			{
				Action_SelectState(strState);
			}
			ImGui::OpenPopup("##AnimSmNodeMenu");
		}
	}
	if (ImGui::BeginPopup("##AnimSmNodeMenu"))
	{
		if (m_bAnyStateSelected)
		{
			// ★ DISABLED AND EXPLAINED, NOT ABSENT. Both verbs refuse an empty name
			// in the document (RemoveState finds no such state, RenameState finds no
			// such state to rename), so the pseudo-node cannot be renamed or deleted
			// either way — but a popup that simply showed nothing reads as a broken
			// menu, and this is a rule worth stating where it bites.
			ImGui::BeginDisabled();
			ImGui::MenuItem("Set As Default");
			ImGui::MenuItem("Delete State");
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", szANIMSM_ANY_STATE_REFUSAL);
			}
		}
		else if (!m_strSelectedState.empty())
		{
			if (ImGui::MenuItem("Set As Default")) { Action_SetDefaultState(m_strSelectedState); }
			if (ImGui::MenuItem("Delete State"))   { Action_RemoveState(m_strSelectedState); }
		}
		ImGui::EndPopup();
	}
}

//=============================================================================
// The inspector strip
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RenderInspector()
{
	ImGui::BeginChild("##AnimSmInspector", Vec(0.0f, 0.0f), true);
	if (!m_xDocument.IsOpen())
	{
		ImGui::TextDisabled("Open a " ZENITH_ANIMCTRL_EXT " to edit its state machine.");
		ImGui::EndChild();
		return;
	}

	// The one place a state is CREATED by hand; the canvas creates none.
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(160.0f));
	ImGui::InputText("##AnimSmStateName", m_acStateNameBuffer, sizeof(m_acStateNameBuffer));
	ImGui::SameLine();
	if (ImGui::Button("Add State"))
	{
		Action_AddState(std::string(m_acStateNameBuffer));
	}
	ImGui::SameLine();
	if (m_bAnyStateSelected)
	{
		// The same refusal the node context menu draws, in the other place it
		// bites. RenameState("") is refused by the document; showing the button
		// live would offer an edit that silently does nothing.
		ImGui::BeginDisabled();
		ImGui::Button("Rename Selected");
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", szANIMSM_ANY_STATE_REFUSAL);
		}
	}
	else if (ImGui::Button("Rename Selected") && !m_strSelectedState.empty())
	{
		Action_RenameState(m_strSelectedState, std::string(m_acStateNameBuffer));
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(Ctrl+drag a node onto another to add a transition)");

	ImGui::Separator();
	if (m_bHasTransitionSelection)
	{
		RenderTransitionInspector();
	}
	else if (m_bAnyStateSelected)
	{
		// ★ BEFORE THE NON-EMPTY TEST, AND THAT ORDER IS THE POINT. The any-state
		// selection carries an EMPTY state name, so the branch below would fall
		// through to "Select a state or a transition." and the pseudo-node would
		// look unselectable.
		RenderAnyStateInspector();
	}
	else if (!m_strSelectedState.empty())
	{
		RenderStateInspector();
	}
	else
	{
		ImGui::TextDisabled("Select a state or a transition.");
	}
	ImGui::EndChild();
}

void Zenith_EditorPanel_AnimStateMachine::RenderAnyStateInspector()
{
	ImGui::Text("%s", szANIMSM_ANY_STATE_LABEL);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", szANIMSM_ANY_STATE_REFUSAL);

	// ★ THE LIST IS READ WITH AN EMPTY OWNER, exactly as every document verb
	// addresses it. There is no second query shape for the any-state list, which
	// is what lets the transition inspector below edit one of these with the very
	// same (from, index) pair the state inspector produces.
	const u_int uCount = m_xDocument.GetTransitionCount(std::string());
	ImGui::Text("Outgoing transitions: %u", uCount);
	for (u_int u = 0; u < uCount; ++u)
	{
		Flux_StateTransition xTransition;
		if (!m_xDocument.GetTransition(std::string(), u, xTransition))
		{
			continue;
		}
		ImGui::PushID(static_cast<int>(u));
		char acLabel[224];
		snprintf(acLabel, sizeof(acLabel), "-> %s (%u condition(s))",
			xTransition.m_strTargetStateName.c_str(), xTransition.m_xConditions.GetSize());
		if (ImGui::Selectable(acLabel))
		{
			Action_SelectTransition(std::string(), u);
		}
		ImGui::PopID();
	}
	if (uCount == 0u)
	{
		ImGui::TextDisabled("(Ctrl+drag from the %s node onto a state to add one)", szANIMSM_ANY_STATE_LABEL);
	}
}

void Zenith_EditorPanel_AnimStateMachine::RenderStateInspector()
{
	const std::string strState = m_strSelectedState;
	ImGui::Text("State: %s", strState.c_str());

	const Zenith_AnimCtrlStateTreeKind eKind = m_xDocument.GetStateTreeKind(strState);
	if (eKind == ZENITH_ANIMCTRL_TREE_COMPLEX)
	{
		// ★ REFUSED, AND THE REASON IS ON SCREEN. A Blend / Additive / Masked /
		// Select nest, or a container's sub-machine: nothing here can edit one,
		// and assigning anything would delete it and report success.
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Zenith_EditorUI::Palette().m_uWarning),
			"%s", BlendTreeRefusalText());
		return;
	}

	// ---- what KIND of tree this state has (WU-7.3) ---------------------------
	// ★ THE COMBO INDEX IS AN OFFSET FROM SINGLE_CLIP, not a lookup table: the
	// three convertible kinds are contiguous in Zenith_AnimCtrlStateTreeKind, so
	// there is nothing here to drift from the enum. An EMPTY tree shows as
	// "Single Clip" because that is what picking anything else converts it from.
	int iKind = (eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D) ? 1
		: (eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D) ? 2 : 0;
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(160.0f));
	if (ImGui::Combo("Tree", &iKind, aszTREE_KIND_NAMES, 3))
	{
		Action_SetStateTreeKind(strState, static_cast<Zenith_AnimCtrlStateTreeKind>(
			static_cast<u_int>(ZENITH_ANIMCTRL_TREE_SINGLE_CLIP) + static_cast<u_int>(iKind)));
	}

	if (eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D || eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D)
	{
		RenderBlendStrip(strState, eKind);
	}
	else
	{
		std::string strCurrentClip;
		m_xDocument.GetStateClipName(strState, strCurrentClip);

		// The clip picker is the def's OWN clip list, resolved to clip NAMES: a
		// state references a clip by name through Flux_AnimationClipCollection,
		// and the collection is keyed on the name the .zanim carries, not on its
		// path.
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(280.0f));
		if (ImGui::BeginCombo("Clip", strCurrentClip.empty() ? "(none)" : strCurrentClip.c_str()))
		{
			if (ImGui::Selectable("(none)", strCurrentClip.empty()))
			{
				Action_SetStateClip(strState, std::string());
			}
			for (u_int u = 0; u < m_xDocument.GetClipPathCount(); ++u)
			{
				std::string strPath;
				if (!m_xDocument.GetClipPathAt(u, strPath))
				{
					continue;
				}
				const std::string strName = AnimSmClipNameForPath(strPath);
				if (ImGui::Selectable(strName.c_str(), strName == strCurrentClip))
				{
					Action_SetStateClip(strState, strName);
				}
			}
			ImGui::EndCombo();
		}
	}

	if (ImGui::Button("Set As Default State"))
	{
		Action_SetDefaultState(strState);
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete State"))
	{
		Action_RemoveState(strState);
		return;
	}

	ImGui::Text("Outgoing transitions: %u", m_xDocument.GetTransitionCount(strState));
	for (u_int u = 0; u < m_xDocument.GetTransitionCount(strState); ++u)
	{
		Flux_StateTransition xTransition;
		if (!m_xDocument.GetTransition(strState, u, xTransition))
		{
			continue;
		}
		ImGui::PushID(static_cast<int>(u));
		char acLabel[224];
		snprintf(acLabel, sizeof(acLabel), "-> %s (%u condition(s))",
			xTransition.m_strTargetStateName.c_str(), xTransition.m_xConditions.GetSize());
		if (ImGui::Selectable(acLabel))
		{
			Action_SelectTransition(strState, u);
		}
		ImGui::PopID();
	}
}

//=============================================================================
// THE BLEND-TREE STRIP (WU-7.3) — a 1D axis or a 2D square, its points as
// draggable markers, its parameter binding(s), and the LIVE PARAMETER DOT.
//
// ★ IT DRAWS NOTHING UNLESS THE SELECTED STATE IS A BLEND SPACE — not a header,
// not a disabled row, not the word "Blend Tree". Editor/CLAUDE.md's rule, and
// the caller has already checked the kind; the guard here is what makes the
// promise true of the FUNCTION rather than of one call site.
//
// ★ IT LIVES INSIDE THE INSPECTOR CHILD, WHICH IS A FIXED HEIGHT. The canvas is
// sized Vec(0, -fInspectorHeight) out of the main window, so nothing emitted
// inside the inspector can take a pixel from the graph — the same placement
// argument the layer strip makes for the side child, and the reason
// `TheBlendStripDrawsNothingForASingleClipStateAndNeverTakesCanvasHeight` can
// assert an EQUALITY rather than a bound.
//
// ★ MARKERS ARE DRAW-LIST DECORATIONS ON ONE InvisibleButton, like the canvas.
// Placing ImGui items at absolute positions and restoring the cursor trips
// ErrorCheckUsingSetCursorPosToExtendParentBoundaries, which in a windowed build
// is a modal CRT dialog nothing logs.
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RenderBlendStrip(const std::string& strStateName,
	Zenith_AnimCtrlStateTreeKind eKind)
{
	const bool bIs2D = (eKind == ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D);
	if (eKind != ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D && !bIs2D)
	{
		return;   // ★ before a single item is submitted
	}
	m_bBlendStripDrawn = true;

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const u_int uPointCount = m_xDocument.GetBlendPointCount(strStateName);

	// ---- the axis binding(s) -------------------------------------------------
	// The list is the def's own FLOAT declarations, because that is the only type
	// a blend axis can bind (the runtime reads it through GetFloat). Offering the
	// others would be offering an assignment the document refuses.
	Zenith_Vector<std::string> axAllParams;
	m_xDocument.GetParameterNamesSorted(axAllParams);
	Zenith_Vector<std::string> axFloatParams;
	for (u_int u = 0; u < axAllParams.GetSize(); ++u)
	{
		Zenith_AnimCtrlParameterDecl xDecl;
		if (m_xDocument.GetParameter(axAllParams.Get(u), xDecl)
			&& xDecl.m_eType == Flux_AnimationParameters::ParamType::Float)
		{
			axFloatParams.PushBack(axAllParams.Get(u));
		}
	}

	const u_int uAxisCount = bIs2D ? 2u : 1u;
	for (u_int uAxis = 0; uAxis < uAxisCount; ++uAxis)
	{
		const Zenith_AnimCtrlBlendAxis eAxis = (uAxis == 0)
			? ZENITH_ANIMCTRL_BLEND_AXIS_X : ZENITH_ANIMCTRL_BLEND_AXIS_Y;
		std::string strBound;
		m_xDocument.GetBlendSpaceParameterName(strStateName, eAxis, strBound);

		ImGui::PushID(static_cast<int>(4000 + uAxis));
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(150.0f));
		const char* szLabel = bIs2D ? (uAxis == 0 ? "X parameter" : "Y parameter") : "Parameter";
		if (ImGui::BeginCombo(szLabel, strBound.empty() ? "(unbound)" : strBound.c_str()))
		{
			// ★ UNBINDING IS AN OFFERED CHOICE, not something you achieve by
			// deleting a parameter. A space left on its authored literal is a
			// legitimate state (D48), and the only way back to it is an empty name.
			if (ImGui::Selectable("(unbound)", strBound.empty()))
			{
				Action_SetBlendSpaceParameter(strStateName, eAxis, std::string());
			}
			for (u_int u = 0; u < axFloatParams.GetSize(); ++u)
			{
				if (ImGui::Selectable(axFloatParams.Get(u).c_str(), axFloatParams.Get(u) == strBound))
				{
					Action_SetBlendSpaceParameter(strStateName, eAxis, axFloatParams.Get(u));
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
	}
	if (axFloatParams.GetSize() == 0)
	{
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xPalette.m_uWarning),
			"declare a Float parameter to drive this space — an unbound one never moves");
	}

	// ---- the axis RANGE the frame is drawn with -----------------------------
	// Derived from the points, then padded and floored by the pure helper, and
	// RECORDED — Action_DragBlendPointToPixel maps through exactly this, so the
	// drag and the draw cannot use two different ranges.
	float fMinX = 0.0f;
	float fMaxX = 0.0f;
	float fMinY = 0.0f;
	float fMaxY = 0.0f;
	bool bAnyPoint = false;
	for (u_int u = 0; u < uPointCount; ++u)
	{
		std::string strClip;
		Zenith_Maths::Vector2 xPosition(0.0f);
		if (!m_xDocument.GetBlendPoint(strStateName, u, strClip, xPosition))
		{
			continue;
		}
		if (!bAnyPoint)
		{
			fMinX = fMaxX = xPosition.x;
			fMinY = fMaxY = xPosition.y;
			bAnyPoint = true;
			continue;
		}
		fMinX = xPosition.x < fMinX ? xPosition.x : fMinX;
		fMaxX = xPosition.x > fMaxX ? xPosition.x : fMaxX;
		fMinY = xPosition.y < fMinY ? xPosition.y : fMinY;
		fMaxY = xPosition.y > fMaxY ? xPosition.y : fMaxY;
	}
	ComputeBlendAxisRange(fMinX, fMaxX, m_fBlendRangeMinX, m_fBlendRangeMaxX);
	ComputeBlendAxisRange(fMinY, fMaxY, m_fBlendRangeMinY, m_fBlendRangeMaxY);

	// ---- the strip surface ---------------------------------------------------
	const float fWidth = Zenith_EditorUI::Px(fANIMSM_BLEND_CANVAS_SIZE_1X) * (bIs2D ? 1.0f : 2.4f);
	const float fHeight = Zenith_EditorUI::Px(bIs2D ? fANIMSM_BLEND_CANVAS_SIZE_1X : 26.0f);
	const ImVec2 xOrigin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##AnimSmBlendSurface", Vec(fWidth, fHeight));
	const bool bSurfaceHovered = ImGui::IsItemHovered();
	// ★ THE INSPECTOR CHILD SCROLLS, AND A CLIPPED ImGui ITEM IS NOT
	// INTERACTABLE. This is the graph editor's palette-row rule: the strip is
	// drawn whether or not it fits, but nothing is RECORDED when the surface is
	// scrolled out of the child — a rect handed out for a surface no click can
	// reach is exactly the y=1768 failure that contract exists to prevent.
	const bool bSurfaceVisible = ImGui::IsItemVisible();

	m_xBlendStripRect.m_fMinX = xOrigin.x;
	m_xBlendStripRect.m_fMinY = xOrigin.y;
	m_xBlendStripRect.m_fMaxX = xOrigin.x + fWidth;
	m_xBlendStripRect.m_fMaxY = xOrigin.y + fHeight;
	m_bBlendStripRectValid = bSurfaceVisible;

	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	pxDraw->AddRectFilled(Vec(m_xBlendStripRect.m_fMinX, m_xBlendStripRect.m_fMinY),
		Vec(m_xBlendStripRect.m_fMaxX, m_xBlendStripRect.m_fMaxY), xPalette.m_uPanelBgAlt, 3.0f);
	pxDraw->AddRect(Vec(m_xBlendStripRect.m_fMinX, m_xBlendStripRect.m_fMinY),
		Vec(m_xBlendStripRect.m_fMaxX, m_xBlendStripRect.m_fMaxY), xPalette.m_uBorder, 3.0f);

	const float fMarkerHalf = Zenith_EditorUI::Px(fANIMSM_BLEND_MARKER_HALF_1X);
	const float fRowY = (m_xBlendStripRect.m_fMinY + m_xBlendStripRect.m_fMaxY) * 0.5f;

	// ---- the markers ---------------------------------------------------------
	for (u_int u = 0; u < uPointCount; ++u)
	{
		std::string strClip;
		Zenith_Maths::Vector2 xPosition(0.0f);
		if (!m_xDocument.GetBlendPoint(strStateName, u, strClip, xPosition))
		{
			continue;
		}
		const float fX = BlendPositionToPixel(xPosition.x, m_xBlendStripRect.m_fMinX, m_xBlendStripRect.m_fMaxX,
			m_fBlendRangeMinX, m_fBlendRangeMaxX);
		// ★ SCREEN Y IS INVERTED against the blend axis, so the mapping is handed
		// its pixel bounds the other way round — the same inversion
		// Action_DragBlendPointToPixel applies, and the reason it is written in
		// both places rather than baked into the pure helper: the helper is a
		// linear map between two ranges and knows nothing about which way a
		// screen grows.
		const float fY = bIs2D
			? BlendPositionToPixel(xPosition.y, m_xBlendStripRect.m_fMaxY, m_xBlendStripRect.m_fMinY,
				m_fBlendRangeMinY, m_fBlendRangeMaxY)
			: fRowY;

		const bool bSelected = (m_uSelectedBlendPoint == u);
		pxDraw->AddRectFilled(Vec(fX - fMarkerHalf, fY - fMarkerHalf), Vec(fX + fMarkerHalf, fY + fMarkerHalf),
			bSelected ? xPalette.m_uAccent : xPalette.m_uFrame, 2.0f);
		pxDraw->AddRect(Vec(fX - fMarkerHalf, fY - fMarkerHalf), Vec(fX + fMarkerHalf, fY + fMarkerHalf),
			bSelected ? xPalette.m_uAccentHover : xPalette.m_uBorder, 2.0f);

		Zenith_AnimCtrlPanelRect xMarker;
		xMarker.m_fMinX = fX - fMarkerHalf;
		xMarker.m_fMinY = fY - fMarkerHalf;
		xMarker.m_fMaxX = fX + fMarkerHalf;
		xMarker.m_fMaxY = fY + fMarkerHalf;
		// Recorded only when the strip is on screen AND the marker was actually
		// painted INSIDE it — the half of the off-screen contract PublishRect
		// cannot supply.
		if (bSurfaceVisible
			&& fX >= m_xBlendStripRect.m_fMinX && fX <= m_xBlendStripRect.m_fMaxX
			&& fY >= m_xBlendStripRect.m_fMinY && fY <= m_xBlendStripRect.m_fMaxY)
		{
			m_xBlendPointRects.Insert(u, xMarker);
		}
	}

	// ---- THE LIVE PARAMETER DOT ---------------------------------------------
	// ★ THIS IS THE THING WU-6.1 MADE POSSIBLE. Before D48's repair, nothing
	// passed the parameter set into a blend tree at all, so the position a dot
	// would draw could never move — the space sat on its deserialized literal
	// forever, in the editor and in a shipping game alike.
	float fDotX = 0.0f;
	float fDotY = 0.0f;
	if (GetLiveParameterDot(fDotX, fDotY))
	{
		const float fPixelX = BlendPositionToPixel(fDotX, m_xBlendStripRect.m_fMinX, m_xBlendStripRect.m_fMaxX,
			m_fBlendRangeMinX, m_fBlendRangeMaxX);
		const float fPixelY = bIs2D
			? BlendPositionToPixel(fDotY, m_xBlendStripRect.m_fMaxY, m_xBlendStripRect.m_fMinY,
				m_fBlendRangeMinY, m_fBlendRangeMaxY)
			: fRowY;
		pxDraw->AddCircleFilled(Vec(fPixelX, fPixelY), fMarkerHalf * 0.8f, xPalette.m_uPlay);

		m_xLiveDotRect.m_fMinX = fPixelX - fMarkerHalf;
		m_xLiveDotRect.m_fMinY = fPixelY - fMarkerHalf;
		m_xLiveDotRect.m_fMaxX = fPixelX + fMarkerHalf;
		m_xLiveDotRect.m_fMaxY = fPixelY + fMarkerHalf;
		// Recorded only when the dot landed INSIDE the strip: a parameter well
		// outside the authored range draws off the frame, and handing out that
		// coordinate would be the graph editor's y=1768 mistake in miniature.
		m_bLiveDotRectValid = (bSurfaceVisible
			&& fPixelX >= m_xBlendStripRect.m_fMinX && fPixelX <= m_xBlendStripRect.m_fMaxX
			&& fPixelY >= m_xBlendStripRect.m_fMinY && fPixelY <= m_xBlendStripRect.m_fMaxY);
	}

	// ---- the marker gesture --------------------------------------------------
	// Press picks the nearest marker under the cursor; release commits, and only
	// past the click slop, so a click that never moved SELECTS and records
	// nothing.
	const ImGuiIO& xIO = ImGui::GetIO();
	if (bSurfaceHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		u_int uHit = uINVALID_ANIMSM_BLEND_POINT;
		for (Zenith_HashMap<u_int, Zenith_AnimCtrlPanelRect>::Iterator xIt(m_xBlendPointRects); !xIt.Done(); xIt.Next())
		{
			const Zenith_AnimCtrlPanelRect& xRect = xIt.GetValue();
			if (xIO.MousePos.x >= xRect.m_fMinX && xIO.MousePos.x <= xRect.m_fMaxX
				&& xIO.MousePos.y >= xRect.m_fMinY && xIO.MousePos.y <= xRect.m_fMaxY)
			{
				uHit = xIt.GetKey();
				break;
			}
		}
		Action_SelectBlendPoint(uHit);
		m_bDraggingBlendPoint = (uHit != uINVALID_ANIMSM_BLEND_POINT);
		m_xBlendDragStartPixel = Zenith_Maths::Vector2(xIO.MousePos.x, xIO.MousePos.y);
	}
	if (m_bDraggingBlendPoint && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		const float fSlop = Zenith_EditorUI::Px(fANIMSM_CLICK_SLOP_1X);
		if (m_uSelectedBlendPoint != uINVALID_ANIMSM_BLEND_POINT
			&& (std::fabs(xIO.MousePos.x - m_xBlendDragStartPixel.x) > fSlop
			 || std::fabs(xIO.MousePos.y - m_xBlendDragStartPixel.y) > fSlop))
		{
			Action_DragBlendPointToPixel(m_uSelectedBlendPoint, xIO.MousePos.x, xIO.MousePos.y);
		}
		m_bDraggingBlendPoint = false;
	}

	// ---- add / remove / re-clip ---------------------------------------------
	Zenith_Vector<std::string> axClipNames;
	for (u_int u = 0; u < m_xDocument.GetClipPathCount(); ++u)
	{
		std::string strPath;
		if (m_xDocument.GetClipPathAt(u, strPath))
		{
			axClipNames.PushBack(AnimSmClipNameForPath(strPath));
		}
	}
	if (axClipNames.GetSize() == 0)
	{
		ImGui::TextDisabled("(add a clip to the controller's clip list first — a point plays one by name)");
		return;
	}
	if (m_iNewBlendPointClip >= static_cast<int>(axClipNames.GetSize()))
	{
		m_iNewBlendPointClip = 0;
	}

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(150.0f));
	if (ImGui::BeginCombo("##AnimSmBlendClip", axClipNames.Get(static_cast<u_int>(m_iNewBlendPointClip)).c_str()))
	{
		for (u_int u = 0; u < axClipNames.GetSize(); ++u)
		{
			if (ImGui::Selectable(axClipNames.Get(u).c_str(), static_cast<int>(u) == m_iNewBlendPointClip))
			{
				m_iNewBlendPointClip = static_cast<int>(u);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Point"))
	{
		// Placed one step past the current maximum so a new point is never on top
		// of an existing one — two markers at the same pixel cannot be told apart
		// and only the first is clickable.
		const float fNewX = bAnyPoint ? fMaxX + 1.0f : 0.0f;
		Action_AddBlendPoint(strStateName, axClipNames.Get(static_cast<u_int>(m_iNewBlendPointClip)), fNewX, 0.0f);
	}

	if (m_uSelectedBlendPoint == uINVALID_ANIMSM_BLEND_POINT)
	{
		return;
	}

	std::string strSelectedClip;
	Zenith_Maths::Vector2 xSelectedPos(0.0f);
	if (!m_xDocument.GetBlendPoint(strStateName, m_uSelectedBlendPoint, strSelectedClip, xSelectedPos))
	{
		return;
	}

	ImGui::SameLine();
	if (ImGui::Button("- Point"))
	{
		Action_RemoveBlendPoint(strStateName, m_uSelectedBlendPoint);
		return;   // the point list just moved under us
	}
	ImGui::SameLine();
	if (ImGui::Button("Set Clip"))
	{
		Action_SetBlendPointClip(strStateName, m_uSelectedBlendPoint,
			axClipNames.Get(static_cast<u_int>(m_iNewBlendPointClip)));
	}

	// The numeric twin of the drag, committed on edit-complete for the reason
	// every other field on this panel is: one command per keystroke makes Ctrl+Z
	// walk back through a half-typed number.
	float afPosition[2] = { xSelectedPos.x, xSelectedPos.y };
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(bIs2D ? 150.0f : 90.0f));
	if (bIs2D)
	{
		ImGui::InputFloat2("##AnimSmBlendPos", afPosition, "%.3f");
	}
	else
	{
		ImGui::InputFloat("##AnimSmBlendPos", &afPosition[0], 0.0f, 0.0f, "%.3f");
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Action_SetBlendPointPosition(strStateName, m_uSelectedBlendPoint, afPosition[0], afPosition[1]);
	}
	ImGui::SameLine();
	ImGui::Text("point %u: %s", m_uSelectedBlendPoint,
		strSelectedClip.empty() ? "(no clip)" : strSelectedClip.c_str());
}

void Zenith_EditorPanel_AnimStateMachine::RenderTransitionInspector()
{
	const std::string strFrom = m_strSelectedTransitionFrom;
	const u_int uIndex = m_uSelectedTransition;
	Flux_StateTransition xTransition;
	if (!m_xDocument.GetTransition(strFrom, uIndex, xTransition))
	{
		ImGui::TextDisabled("(the selected transition is gone)");
		return;
	}

	ImGui::Text("Transition: %s -> %s", strFrom.empty() ? szANIMSM_ANY_STATE_LABEL : strFrom.c_str(),
		xTransition.m_strTargetStateName.c_str());

	// ★ COMMITTED ON EDIT-COMPLETE, NOT PER KEYSTROKE. A command per character
	// would make Ctrl+Z walk backwards through "0.25" one digit at a time —
	// exactly the rule the dope sheet's event-name field follows.
	float fDuration = xTransition.m_fTransitionDuration;
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(120.0f));
	ImGui::InputFloat("Duration (s)", &fDuration, 0.01f, 0.1f, "%.3f");
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Action_SetTransitionDuration(strFrom, uIndex, fDuration);
	}

	bool bHasExit = xTransition.m_bHasExitTime;
	float fExit = xTransition.m_bHasExitTime ? xTransition.m_fExitTime : 0.0f;
	if (ImGui::Checkbox("Has Exit Time", &bHasExit))
	{
		Action_SetTransitionExitTime(strFrom, uIndex, bHasExit, fExit);
	}
	if (bHasExit)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(120.0f));
		ImGui::SliderFloat("Exit Time", &fExit, 0.0f, 1.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			Action_SetTransitionExitTime(strFrom, uIndex, true, fExit);
		}
	}

	bool bInterruptible = xTransition.m_bInterruptible;
	if (ImGui::Checkbox("Interruptible", &bInterruptible))
	{
		Action_SetTransitionInterruptible(strFrom, uIndex, bInterruptible);
	}

	ImGui::SameLine();
	if (ImGui::Button("Delete Transition"))
	{
		Action_RemoveTransition(strFrom, uIndex);
		return;
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Conditions");
	for (u_int u = 0; u < xTransition.m_xConditions.GetSize(); ++u)
	{
		const Flux_TransitionCondition& xCondition = xTransition.m_xConditions.Get(u);
		ImGui::PushID(static_cast<int>(2000 + u));
		char acLine[256];
		switch (xCondition.m_eParamType)
		{
		case Flux_AnimationParameters::ParamType::Float:
			snprintf(acLine, sizeof(acLine), "%s %s %.3f", xCondition.m_strParameterName.c_str(),
				Zenith_AnimControllerDocument::CompareOpName(xCondition.m_eCompareOp), xCondition.m_fThreshold);
			break;
		case Flux_AnimationParameters::ParamType::Int:
			snprintf(acLine, sizeof(acLine), "%s %s %d", xCondition.m_strParameterName.c_str(),
				Zenith_AnimControllerDocument::CompareOpName(xCondition.m_eCompareOp), xCondition.m_iThreshold);
			break;
		default:
			snprintf(acLine, sizeof(acLine), "%s %s %s", xCondition.m_strParameterName.c_str(),
				Zenith_AnimControllerDocument::CompareOpName(xCondition.m_eCompareOp),
				xCondition.m_bThreshold ? "true" : "false");
			break;
		}
		ImGui::TextUnformatted(acLine);
		ImGui::SameLine();
		if (ImGui::SmallButton("x"))
		{
			Action_RemoveCondition(strFrom, uIndex, u);
			ImGui::PopID();
			return;
		}
		ImGui::PopID();
	}

	Zenith_Vector<std::string> axParams;
	m_xDocument.GetParameterNamesSorted(axParams);
	if (axParams.GetSize() == 0)
	{
		ImGui::TextDisabled("(declare a parameter first — a condition's TYPE comes from the declaration)");
		return;
	}
	if (m_iConditionParameterIndex >= static_cast<int>(axParams.GetSize()))
	{
		m_iConditionParameterIndex = 0;
	}

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(120.0f));
	if (ImGui::BeginCombo("##AnimSmCondParam", axParams.Get(static_cast<u_int>(m_iConditionParameterIndex)).c_str()))
	{
		for (u_int u = 0; u < axParams.GetSize(); ++u)
		{
			if (ImGui::Selectable(axParams.Get(u).c_str(), static_cast<int>(u) == m_iConditionParameterIndex))
			{
				m_iConditionParameterIndex = static_cast<int>(u);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(60.0f));
	ImGui::Combo("##AnimSmCondOp", &m_iConditionCompareOp, aszCOMPARE_OP_NAMES, 6);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(90.0f));
	ImGui::InputFloat("##AnimSmCondValue", &m_fConditionThreshold);
	ImGui::SameLine();
	if (ImGui::Button("Add Condition"))
	{
		Action_AddCondition(strFrom, uIndex, axParams.Get(static_cast<u_int>(m_iConditionParameterIndex)),
			static_cast<Flux_TransitionCondition::CompareOp>(m_iConditionCompareOp), m_fConditionThreshold);
	}
}

#endif // ZENITH_TOOLS
