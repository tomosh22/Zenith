#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Zenith_EditorUI.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/Zenith_DragDropPayloads.h"
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

	if (!m_bShow)
	{
		return;
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
	RenderBanners();
	RenderPreviewPane();
	ImGui::Separator();
	RenderSheet();

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
			m_xDocument.Save();
			m_bCloseRefusedDirty = false;
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

	// ---- dispossessed (D32) -------------------------------------------------
	if (!m_xSession.HasPreviewSlot())
	{
		// There is exactly ONE preview view slot, shared last-opened-wins. The
		// dispossessed panel says WHO has it rather than showing a stale or black
		// image, and must NOT deactivate the view — that would tear down what the
		// current owner is staging into the same frame.
		const std::string& strOwner = m_xSession.GetPreviewSlotOwnerName();
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(xPalette.m_uTextDim));
		ImGui::TextWrapped("The shared preview view is currently held by '%s'.",
			strOwner.empty() ? "(nobody)" : strOwner.c_str());
		ImGui::PopStyleColor();
		if (ImGui::Button("Reclaim preview"))
		{
			m_xSession.ReclaimPreviewSlot();
		}
		return;
	}

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

	if (!m_bPreviewImageRegistered)
	{
		// The persistent preview LDR the per-view tonemap writes — NOT a
		// transient, so the registration stays valid across graph rebuilds. On a
		// backend with no device the SRV is invalid and RegisterTexture hands back
		// an invalid handle, which is why the draw below is gated rather than
		// asserted.
		const Flux_ImGuiTextureHandle xHandle = Flux_ImGuiIntegration::RegisterTexture(
			g_xEngine.FluxGraphics().GetPreviewLDR().SRV(), g_xEngine.FluxGraphics().m_xClampSampler);
		m_ulPreviewImageHandle = xHandle.AsUInt64();
		m_bPreviewImageRegistered = true;
	}

	if (m_ulPreviewImageHandle == 0u)
	{
		ImGui::TextDisabled("(no preview image on this backend)");
		return;
	}

	Flux_ImGuiTextureHandle xHandle;
	xHandle.SetValue(m_ulPreviewImageHandle);
	ImGui::Image((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xHandle),
		Vec(fPreviewSize, fPreviewSize));

	if (ImGui::IsItemHovered())
	{
		const ImGuiIO& xIO = ImGui::GetIO();
		if (xIO.MouseWheel != 0.0f)
		{
			m_xSession.ZoomCamera(xIO.MouseWheel * 0.2f);
		}
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		{
			m_xSession.OrbitCamera(-xIO.MouseDelta.x * 0.01f, -xIO.MouseDelta.y * 0.01f);
		}
	}

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextDisabled("Rig: %s", m_xSession.GetSkeletonPath().c_str());
	ImGui::TextDisabled("Mesh: %s%s", m_xSession.GetPreviewModelPath().c_str(),
		m_xSession.IsPreviewMeshBareMeshAsset() ? "  (bare mesh)" : "");
	ImGui::TextDisabled("(drag = orbit, wheel = zoom)");
	ImGui::EndGroup();
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
	DrawRows(pxDraw, xLayout, bCanvasHovered);
	DrawPlayhead(pxDraw, xLayout);

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
		const ImU32 uFill = bPastDuration ? xPalette.m_uWarning : xPalette.m_uAccent;

		// A diamond, the dope-sheet convention.
		const ImVec2 axPoints[4] =
		{
			Vec(fCentreX, fCentreY - fHalf),
			Vec(fCentreX + fHalf, fCentreY),
			Vec(fCentreX, fCentreY + fHalf),
			Vec(fCentreX - fHalf, fCentreY),
		};
		pxDraw->AddConvexPolyFilled(axPoints, 4, uFill);
		pxDraw->AddPolyline(axPoints, 4, xPalette.m_uBorder, ImDrawFlags_Closed, 1.0f);

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
		const ImU32 uFill = bPastDuration ? xPalette.m_uWarning : xPalette.m_uTypeAnimation;

		// A flag, so an event never reads as a key.
		const ImVec2 axPoints[3] =
		{
			Vec(fCentreX - fHalf, fCentreY - fHalf),
			Vec(fCentreX + fHalf, fCentreY),
			Vec(fCentreX - fHalf, fCentreY + fHalf),
		};
		pxDraw->AddConvexPolyFilled(axPoints, 3, uFill);
		pxDraw->AddPolyline(axPoints, 3, xPalette.m_uBorder, ImDrawFlags_Closed, 1.0f);

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

#endif // ZENITH_TOOLS
