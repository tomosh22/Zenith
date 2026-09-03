#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Viewport.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/FrameContext.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorUI.h"
#include "Flux/Flux_GraphicsImpl.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace
{
	// Rebinds the ImGui texture when the render target's image view changes
	// (resize); the old handle is released after the frames in flight drain.
	void RefreshViewportTexture(Zenith_Editor& xEditor, const Flux_ShaderResourceView& xGameRenderSRV)
	{
		if (xEditor.m_xCachedImageViewHandle.AsUInt() == xGameRenderSRV.m_xImageViewHandle.AsUInt())
		{
			return;
		}
		if (xEditor.m_xCachedGameTextureHandle.IsValid())
		{
			constexpr u_int FRAMES_TO_WAIT = 3;
			xEditor.m_xPendingDeletions.PushBack(PendingImGuiTextureDeletion{ xEditor.m_xCachedGameTextureHandle, FRAMES_TO_WAIT });
		}
		xEditor.m_xCachedGameTextureHandle = Flux_ImGuiIntegration::RegisterTexture(xGameRenderSRV, g_xEngine.FluxGraphics().m_xRepeatSampler);
		xEditor.m_xCachedImageViewHandle = xGameRenderSRV.m_xImageViewHandle;
	}

	//-------------------------------------------------------------------------
	// Overlays
	//-------------------------------------------------------------------------
	void DrawModeBadge(Zenith_Editor& xEditor, const ImVec2& xImageMin, float fImageWidth)
	{
		const EditorMode eMode = xEditor.GetEditorMode();
		if (eMode == EditorMode::Stopped)
		{
			return;
		}
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const char* szText = (eMode == EditorMode::Playing) ? "PLAYING" : "PAUSED";
		const ImU32 uColour = (eMode == EditorMode::Playing) ? xP.m_uPlay : xP.m_uPause;
		Zenith_EditorUI::PushSmallFont();
		const float fWidth = ImGui::CalcTextSize(szText).x + Zenith_EditorUI::Px(12.0f);
		Zenith_EditorUI::PopFont();
		Zenith_EditorUI::DrawBadge(ImGui::GetWindowDrawList(),
			ImVec2(xImageMin.x + (fImageWidth - fWidth) * 0.5f, xImageMin.y + Zenith_EditorUI::Px(10.0f)),
			szText, uColour, Zenith_EditorUI::Colour(0.05f, 0.05f, 0.05f));
	}

	void DrawStatsBlock(Zenith_Editor& xEditor, const ImVec2& xImageMin)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const Zenith_EditorState& xState = xEditor.m_xEditorState;
		const float fMs = Zenith_EditorUI::SmoothedFrameMs(g_xEngine.Frame().GetDt());
		const float fFps = fMs > 0.0f ? 1000.0f / fMs : 0.0f;

		const char* szGizmo = "Move";
		if (xState.m_eGizmoMode == EditorGizmoMode::Rotate) szGizmo = "Rotate";
		else if (xState.m_eGizmoMode == EditorGizmoMode::Scale) szGizmo = "Scale";

		const Zenith_EditorCameraState& xCam = xState.m_xCamera;
		const char* szGesture = "idle";
		if (xCam.m_bLooking)       szGesture = "LOOK";
		else if (xCam.m_bOrbiting) szGesture = "ORBIT";
		else if (xCam.m_bPanning)  szGesture = "PAN";
		char acLine1[96], acLine2[128], acLine3[96], acLine4[128];
		snprintf(acLine1, sizeof(acLine1), "%.0f fps  %.2f ms", fFps, fMs);
		snprintf(acLine2, sizeof(acLine2), "%s  %s%s", szGizmo, xState.m_xPrefs.m_bGizmoLocalSpace ? "Local" : "World",
			xState.m_xPrefs.m_bSnapEnabled ? "  Snap" : "");
		const Zenith_Maths::Vector3& xPos = xState.m_xCamera.m_xPosition;
		snprintf(acLine3, sizeof(acLine3), "Camera %.1f, %.1f, %.1f   speed %.0f", xPos.x, xPos.y, xPos.z, xState.m_xCamera.m_fMoveSpeed);
		snprintf(acLine4, sizeof(acLine4), "%s  yaw %.0f  pitch %.0f   mouse %+.0f %+.0f",
			szGesture, glm::degrees(xCam.m_fYaw), glm::degrees(xCam.m_fPitch),
			xCam.m_xLastMouseDelta.x, xCam.m_xLastMouseDelta.y);

		Zenith_EditorUI::PushSmallFont();
		const float fLine = ImGui::GetTextLineHeight();
		const float fPad = Zenith_EditorUI::Px(8.0f);
		float fWidth = ImMax(ImMax(ImGui::CalcTextSize(acLine1).x, ImGui::CalcTextSize(acLine4).x),
			ImMax(ImGui::CalcTextSize(acLine2).x, ImGui::CalcTextSize(acLine3).x));
		const ImVec2 xMin(xImageMin.x + Zenith_EditorUI::Px(10.0f), xImageMin.y + Zenith_EditorUI::Px(10.0f));
		const ImVec2 xMax(xMin.x + fWidth + fPad * 2.0f, xMin.y + fLine * 4.0f + fPad * 2.0f);
		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		Zenith_EditorUI::DrawOverlayBackground(pxDraw, xMin, xMax);
		pxDraw->AddText(ImVec2(xMin.x + fPad, xMin.y + fPad), xP.m_uTextBright, acLine1);
		pxDraw->AddText(ImVec2(xMin.x + fPad, xMin.y + fPad + fLine), xP.m_uText, acLine2);
		pxDraw->AddText(ImVec2(xMin.x + fPad, xMin.y + fPad + fLine * 2.0f), xP.m_uTextDim, acLine3);
		pxDraw->AddText(ImVec2(xMin.x + fPad, xMin.y + fPad + fLine * 3.0f), xP.m_uTextBright, acLine4);
		Zenith_EditorUI::PopFont();
	}

	void DrawNavigationHint(Zenith_Editor& xEditor, const ImVec2& xImageMin, const ImVec2& xImageMax)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const Zenith_EditorCameraState& xCamera = xEditor.m_xEditorState.m_xCamera;
		const char* szHint = xCamera.m_bLooking
			? "WASD fly   Q/E down/up   Shift fast   wheel: speed"
			: "RMB look   Alt+LMB orbit   MMB pan   wheel zoom   F focus";
		Zenith_EditorUI::PushSmallFont();
		const ImVec2 xSize = ImGui::CalcTextSize(szHint);
		const float fPad = Zenith_EditorUI::Px(6.0f);
		const ImVec2 xMin(xImageMin.x + Zenith_EditorUI::Px(10.0f), xImageMax.y - xSize.y - fPad * 2.0f - Zenith_EditorUI::Px(10.0f));
		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		Zenith_EditorUI::DrawOverlayBackground(pxDraw, xMin, ImVec2(xMin.x + xSize.x + fPad * 2.0f, xMin.y + xSize.y + fPad * 2.0f));
		pxDraw->AddText(ImVec2(xMin.x + fPad, xMin.y + fPad), xP.m_uTextDim, szHint);
		Zenith_EditorUI::PopFont();
	}

	void DrawAxisWidget(Zenith_Editor& xEditor, const ImVec2& xImageMax)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		Zenith_Maths::Matrix4 xView;
		xEditor.BuildViewMatrix(xView);

		const float fRadius = Zenith_EditorUI::Px(28.0f);
		const ImVec2 xCentre(xImageMax.x - fRadius - Zenith_EditorUI::Px(18.0f), xImageMax.y - fRadius - Zenith_EditorUI::Px(18.0f));
		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		pxDraw->AddCircleFilled(xCentre, fRadius + Zenith_EditorUI::Px(8.0f), xP.m_uOverlayBg);

		struct Axis { Zenith_Maths::Vector3 m_xDir; ImU32 m_uColour; const char* m_szLabel; Zenith_Maths::Vector2 m_xScreen; float m_fDepth; };
		Axis axAxes[3] =
		{
			{ Zenith_Maths::Vector3(1, 0, 0), xP.m_uAxisX, "X", {}, 0.0f },
			{ Zenith_Maths::Vector3(0, 1, 0), xP.m_uAxisY, "Y", {}, 0.0f },
			{ Zenith_Maths::Vector3(0, 0, 1), xP.m_uAxisZ, "Z", {}, 0.0f },
		};
		for (Axis& xAxis : axAxes)
		{
			xAxis.m_fDepth = Zenith_EditorPanelViewport::ProjectAxisForWidget(xView, xAxis.m_xDir, xAxis.m_xScreen);
		}
		// Far axes first so the near ones draw on top.
		int aiOrder[3] = { 0, 1, 2 };
		for (int i = 0; i < 3; ++i)
			for (int j = i + 1; j < 3; ++j)
				if (axAxes[aiOrder[j]].m_fDepth > axAxes[aiOrder[i]].m_fDepth) { const int t = aiOrder[i]; aiOrder[i] = aiOrder[j]; aiOrder[j] = t; }

		Zenith_EditorUI::PushSmallFont();
		for (int i = 0; i < 3; ++i)
		{
			const Axis& xAxis = axAxes[aiOrder[i]];
			const ImVec2 xTip(xCentre.x + xAxis.m_xScreen.x * fRadius, xCentre.y + xAxis.m_xScreen.y * fRadius);
			// Axes pointing away from the camera draw dimmer and thinner.
			const bool bTowards = xAxis.m_fDepth <= 0.0f;
			const ImU32 uColour = bTowards ? xAxis.m_uColour : (xAxis.m_uColour & 0x00FFFFFF) | 0x80000000;
			pxDraw->AddLine(xCentre, xTip, uColour, Zenith_EditorUI::Px(bTowards ? 2.0f : 1.5f));
			pxDraw->AddCircleFilled(xTip, Zenith_EditorUI::Px(6.0f), uColour);
			const ImVec2 xLabelSize = ImGui::CalcTextSize(xAxis.m_szLabel);
			pxDraw->AddText(ImVec2(xTip.x - xLabelSize.x * 0.5f, xTip.y - xLabelSize.y * 0.5f), Zenith_EditorUI::Colour(0.06f, 0.06f, 0.06f), xAxis.m_szLabel);
		}
		Zenith_EditorUI::PopFont();
	}

	// Draw-list only: the overlays submit no ImGui items and never move the
	// cursor, so the Image stays the panel's last (and only) item.
	void DrawOverlays(Zenith_Editor& xEditor, const ImVec2& xImageMin, const ImVec2& xImageSize)
	{
		const ImVec2 xImageMax(xImageMin.x + xImageSize.x, xImageMin.y + xImageSize.y);
		const Zenith_EditorPrefs& xPrefs = xEditor.m_xEditorState.m_xPrefs;
		if (xEditor.m_xEditorState.m_bViewportOverlaysHidden)
		{
			return;
		}

		DrawModeBadge(xEditor, xImageMin, xImageSize.x);
		if (xPrefs.m_bShowViewportStats)
		{
			DrawStatsBlock(xEditor, xImageMin);
		}
		if (xEditor.GetEditorMode() != EditorMode::Playing)
		{
			DrawNavigationHint(xEditor, xImageMin, xImageMax);
			if (xPrefs.m_bShowViewportAxes)
			{
				DrawAxisWidget(xEditor, xImageMax);
			}
		}
	}
}

namespace Zenith_EditorPanelViewport
{

float ProjectAxisForWidget(const Zenith_Maths::Matrix4& xViewMatrix, const Zenith_Maths::Vector3& xWorldAxis, Zenith_Maths::Vector2& xOutScreenDir)
{
	// Rotation part only (w = 0). View space is +Z forward, +Y up; ImGui's
	// pixel space is y-down, hence the flip.
	const Zenith_Maths::Vector4 xView = xViewMatrix * Zenith_Maths::Vector4(xWorldAxis, 0.0f);
	xOutScreenDir = Zenith_Maths::Vector2(xView.x, -xView.y);
	return xView.z;
}

void Render(Zenith_Editor& xEditor)
{
	Zenith_EditorViewportState& xViewport = xEditor.m_xEditorState.m_xViewport;

	// No padding: the image fills the panel edge to edge.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin(szEDITOR_WINDOW_VIEWPORT, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	const ImVec2 xViewportPanelPos = ImGui::GetCursorScreenPos();
	xViewport.m_xPosition = { xViewportPanelPos.x, xViewportPanelPos.y };

	Flux_ShaderResourceView& xGameRenderSRV = g_xEngine.FluxGraphics().GetFinalRenderTarget().SRV();
	if (!xGameRenderSRV.m_xImageViewHandle.IsValid())
	{
		ImGui::TextDisabled("  Game render target not available");
		ImGui::End();
		return;
	}
	RefreshViewportTexture(xEditor, xGameRenderSRV);

	const ImVec2 xPanelSize = ImGui::GetContentRegionAvail();
	xViewport.m_xSize = { ImMax(xPanelSize.x, 1.0f), ImMax(xPanelSize.y, 1.0f) };
	xViewport.m_bHovered = ImGui::IsWindowHovered();
	xViewport.m_bFocused = ImGui::IsWindowFocused();

	if (xEditor.m_xCachedGameTextureHandle.IsValid())
	{
		ImGui::Image((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xEditor.m_xCachedGameTextureHandle), xPanelSize);
		// A click on the image focuses the viewport so W/E/R/F/Delete reach it.
		if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
		{
			ImGui::SetWindowFocus();
		}
		DrawOverlays(xEditor, xViewportPanelPos, xPanelSize);
	}
	else
	{
		ImGui::TextDisabled("  Viewport texture not yet initialized");
	}

	ImGui::End();
}

} // namespace Zenith_EditorPanelViewport

#endif // ZENITH_TOOLS
