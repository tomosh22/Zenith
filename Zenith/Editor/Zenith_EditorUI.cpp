#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorUI.h"
#include "Editor/Zenith_EditorFontData.generated.h"
#include "Core/Zenith_EditorFontHook.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Zenith_Windows_Window.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>
#include <cctype>
#include <cstring>

//=============================================================================
// Module state
//=============================================================================
namespace
{
	struct EditorUIState
	{
		ImFont* m_pxFont = nullptr;
		float m_fUIScale = 1.0f;
		bool m_bFontsLoaded = false;
		Zenith_EditorPalette m_xPalette;
		// Number of style colours PushPlayModeTint pushed (0 when not tinted).
		int m_iPlayTintPushCount = 0;
	};

	EditorUIState& State()
	{
		static EditorUIState s_xState;
		return s_xState;
	}

	constexpr float fFONT_SIZE_BASE = 15.0f;
	constexpr float fFONT_SIZE_SMALL = 12.5f;
	constexpr float fFONT_SIZE_HEADING = 18.0f;

	ImVec4 ToLinear(float fR, float fG, float fB, float fA)
	{
		return ImVec4(powf(fR, 2.2f), powf(fG, 2.2f), powf(fB, 2.2f), fA);
	}

	ImVec4 Grey(float fV, float fA = 1.0f)
	{
		return ImVec4(fV, fV, fV, fA);
	}

	// Reads the platform DPI once. The window exists (hidden or not) in every
	// configuration that has an ImGui context, so this is always answerable.
	float QueryContentScale()
	{
		const float fScale = Zenith_Window::GetInstance()->GetContentScale();
		return (fScale > 0.5f && fScale < 8.0f) ? fScale : 1.0f;
	}
}

//=============================================================================
// Fonts
//=============================================================================

void Zenith_EditorFonts_Load()
{
	Zenith_EditorUI::LoadFonts();
}

namespace Zenith_EditorUI
{

void LoadFonts()
{
	EditorUIState& xState = State();
	if (xState.m_bFontsLoaded || ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	xState.m_bFontsLoaded = true;
	xState.m_fUIScale = QueryContentScale();

	ImGuiIO& xIO = ImGui::GetIO();
	ImFontConfig xConfig;
	// The data is a compile-time constant; ImGui must not try to free it.
	xConfig.FontDataOwnedByAtlas = false;
	// Bake at the DPI-scaled size so glyphs are hinted for the pixels they land on.
	xConfig.RasterizerDensity = xState.m_fUIScale;
	xState.m_pxFont = xIO.Fonts->AddFontFromMemoryCompressedTTF(
		RobotoMedium_compressed_data, static_cast<int>(RobotoMedium_compressed_size), fFONT_SIZE_BASE, &xConfig);
	if (xState.m_pxFont != nullptr)
	{
		xIO.FontDefault = xState.m_pxFont;
	}
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor font loaded (Roboto Medium, base %.1f px, UI scale %.2f)", fFONT_SIZE_BASE, xState.m_fUIScale);
}

float GetUIScale()
{
	return State().m_fUIScale;
}

float Px(float fPixelsAt1x)
{
	return fPixelsAt1x * State().m_fUIScale;
}

ImFont* GetFont()
{
	return State().m_pxFont;
}

void PushSmallFont()
{
	ImGui::PushFont(nullptr, fFONT_SIZE_SMALL);
}

void PushHeadingFont()
{
	ImGui::PushFont(nullptr, fFONT_SIZE_HEADING);
}

void PopFont()
{
	ImGui::PopFont();
}

//=============================================================================
// Theme
//=============================================================================

ImU32 Colour(float fR, float fG, float fB, float fA)
{
	return ImGui::ColorConvertFloat4ToU32(ToLinear(fR, fG, fB, fA));
}

const Zenith_EditorPalette& Palette()
{
	return State().m_xPalette;
}

namespace
{
	void FillPalette(Zenith_EditorPalette& xP)
	{
		xP.m_uAccent      = Colour(0.28f, 0.56f, 0.96f);
		xP.m_uAccentHover = Colour(0.40f, 0.66f, 1.00f);
		xP.m_uAccentDim   = Colour(0.28f, 0.56f, 0.96f, 0.35f);
		xP.m_uSelection   = Colour(0.20f, 0.36f, 0.60f);
		xP.m_uText        = Colour(0.86f, 0.86f, 0.86f);
		xP.m_uTextDim     = Colour(0.56f, 0.56f, 0.56f);
		xP.m_uTextBright  = Colour(0.98f, 0.98f, 0.98f);
		xP.m_uPlay        = Colour(0.45f, 0.80f, 0.42f);
		xP.m_uPause       = Colour(0.95f, 0.75f, 0.30f);
		xP.m_uStop        = Colour(0.92f, 0.38f, 0.36f);
		xP.m_uSuccess     = Colour(0.45f, 0.80f, 0.42f);
		xP.m_uWarning     = Colour(0.96f, 0.74f, 0.26f);
		xP.m_uError       = Colour(0.95f, 0.40f, 0.38f);
		xP.m_uInfo        = Colour(0.62f, 0.62f, 0.62f);
		xP.m_uAxisX       = Colour(0.88f, 0.32f, 0.33f);
		xP.m_uAxisY       = Colour(0.48f, 0.78f, 0.30f);
		xP.m_uAxisZ       = Colour(0.32f, 0.55f, 0.95f);
		xP.m_uPanelBg     = Colour(0.13f, 0.13f, 0.13f);
		xP.m_uPanelBgAlt  = Colour(0.16f, 0.16f, 0.16f);
		xP.m_uToolbarBg   = Colour(0.15f, 0.15f, 0.15f);
		xP.m_uStatusBg    = Colour(0.11f, 0.11f, 0.11f);
		xP.m_uFrame       = Colour(0.20f, 0.20f, 0.20f);
		xP.m_uFrameHover  = Colour(0.25f, 0.25f, 0.25f);
		xP.m_uFrameActive = Colour(0.30f, 0.30f, 0.30f);
		xP.m_uBorder      = Colour(0.06f, 0.06f, 0.06f);
		xP.m_uOverlayBg   = Colour(0.05f, 0.05f, 0.05f, 0.62f);
		xP.m_uTypeTexture   = Colour(0.36f, 0.62f, 0.86f);
		xP.m_uTypeMaterial  = Colour(0.36f, 0.76f, 0.56f);
		xP.m_uTypeMesh      = Colour(0.86f, 0.56f, 0.34f);
		xP.m_uTypeModel     = Colour(0.82f, 0.46f, 0.62f);
		xP.m_uTypePrefab    = Colour(0.46f, 0.66f, 0.96f);
		xP.m_uTypeScene     = Colour(0.90f, 0.76f, 0.34f);
		xP.m_uTypeAnimation = Colour(0.74f, 0.46f, 0.86f);
		xP.m_uTypeGraph     = Colour(0.34f, 0.80f, 0.80f);
		xP.m_uTypeFolder    = Colour(0.86f, 0.70f, 0.40f);
		xP.m_uTypeOther     = Colour(0.50f, 0.50f, 0.52f);
		xP.m_xPlayTint      = ToLinear(0.55f, 0.36f, 0.12f, 1.0f);
	}

	void ApplyLayout(ImGuiStyle& xStyle)
	{
		xStyle.WindowPadding      = ImVec2(8.0f, 8.0f);
		xStyle.FramePadding       = ImVec2(6.0f, 4.0f);
		xStyle.CellPadding        = ImVec2(6.0f, 3.0f);
		xStyle.ItemSpacing        = ImVec2(8.0f, 5.0f);
		xStyle.ItemInnerSpacing   = ImVec2(5.0f, 4.0f);
		xStyle.IndentSpacing      = 16.0f;
		xStyle.ScrollbarSize      = 12.0f;
		xStyle.GrabMinSize        = 10.0f;
		xStyle.WindowRounding     = 0.0f;
		xStyle.ChildRounding      = 4.0f;
		xStyle.FrameRounding      = 3.0f;
		xStyle.PopupRounding      = 5.0f;
		xStyle.ScrollbarRounding  = 6.0f;
		xStyle.GrabRounding       = 3.0f;
		xStyle.TabRounding        = 4.0f;
		xStyle.WindowBorderSize   = 0.0f;
		xStyle.ChildBorderSize    = 1.0f;
		xStyle.FrameBorderSize    = 0.0f;
		xStyle.PopupBorderSize    = 1.0f;
		xStyle.TabBorderSize      = 0.0f;
		xStyle.TabBarBorderSize   = 1.0f;
		xStyle.TabBarOverlineSize = 2.0f;
		xStyle.DockingSeparatorSize = 2.0f;
		xStyle.WindowTitleAlign   = ImVec2(0.0f, 0.5f);
		xStyle.WindowMenuButtonPosition = ImGuiDir_None;
		xStyle.SeparatorTextBorderSize = 1.0f;
		xStyle.SeparatorTextPadding = ImVec2(12.0f, 3.0f);
		xStyle.HoverDelayShort    = 0.35f;
		xStyle.HoverDelayNormal   = 0.55f;
		xStyle.HoverStationaryDelay = 0.15f;
	}

	void ApplyColours(ImVec4* axColours)
	{
		const ImVec4 xAccent(0.28f, 0.56f, 0.96f, 1.00f);
		axColours[ImGuiCol_Text]                  = Grey(0.86f);
		axColours[ImGuiCol_TextDisabled]          = Grey(0.50f);
		axColours[ImGuiCol_WindowBg]              = Grey(0.13f);
		axColours[ImGuiCol_ChildBg]               = Grey(0.13f, 0.0f);
		axColours[ImGuiCol_PopupBg]               = Grey(0.12f, 0.98f);
		axColours[ImGuiCol_Border]                = Grey(0.06f);
		axColours[ImGuiCol_BorderShadow]          = Grey(0.00f, 0.0f);
		axColours[ImGuiCol_FrameBg]               = Grey(0.20f);
		axColours[ImGuiCol_FrameBgHovered]        = Grey(0.25f);
		axColours[ImGuiCol_FrameBgActive]         = Grey(0.30f);
		axColours[ImGuiCol_TitleBg]               = Grey(0.10f);
		axColours[ImGuiCol_TitleBgActive]         = Grey(0.14f);
		axColours[ImGuiCol_TitleBgCollapsed]      = Grey(0.10f, 0.75f);
		axColours[ImGuiCol_MenuBarBg]             = Grey(0.15f);
		axColours[ImGuiCol_ScrollbarBg]           = Grey(0.11f);
		axColours[ImGuiCol_ScrollbarGrab]         = Grey(0.30f);
		axColours[ImGuiCol_ScrollbarGrabHovered]  = Grey(0.38f);
		axColours[ImGuiCol_ScrollbarGrabActive]   = Grey(0.45f);
		axColours[ImGuiCol_CheckMark]             = xAccent;
		axColours[ImGuiCol_SliderGrab]            = ImVec4(0.28f, 0.56f, 0.96f, 0.85f);
		axColours[ImGuiCol_SliderGrabActive]      = xAccent;
		axColours[ImGuiCol_Button]                = Grey(0.24f);
		axColours[ImGuiCol_ButtonHovered]         = Grey(0.31f);
		axColours[ImGuiCol_ButtonActive]          = Grey(0.38f);
		axColours[ImGuiCol_Header]                = ImVec4(0.20f, 0.36f, 0.60f, 1.00f);
		axColours[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.42f, 0.68f, 1.00f);
		axColours[ImGuiCol_HeaderActive]          = ImVec4(0.28f, 0.48f, 0.76f, 1.00f);
		axColours[ImGuiCol_Separator]             = Grey(0.07f);
		axColours[ImGuiCol_SeparatorHovered]      = ImVec4(0.28f, 0.56f, 0.96f, 0.78f);
		axColours[ImGuiCol_SeparatorActive]       = xAccent;
		axColours[ImGuiCol_ResizeGrip]            = ImVec4(0.28f, 0.56f, 0.96f, 0.15f);
		axColours[ImGuiCol_ResizeGripHovered]     = ImVec4(0.28f, 0.56f, 0.96f, 0.60f);
		axColours[ImGuiCol_ResizeGripActive]      = ImVec4(0.28f, 0.56f, 0.96f, 0.90f);
		axColours[ImGuiCol_Tab]                   = Grey(0.10f);
		axColours[ImGuiCol_TabHovered]            = Grey(0.22f);
		axColours[ImGuiCol_TabSelected]           = Grey(0.13f);
		axColours[ImGuiCol_TabSelectedOverline]   = xAccent;
		axColours[ImGuiCol_TabDimmed]             = Grey(0.09f);
		axColours[ImGuiCol_TabDimmedSelected]     = Grey(0.12f);
		axColours[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.28f, 0.56f, 0.96f, 0.40f);
		axColours[ImGuiCol_DockingPreview]        = ImVec4(0.28f, 0.56f, 0.96f, 0.60f);
		axColours[ImGuiCol_DockingEmptyBg]        = Grey(0.09f);
		axColours[ImGuiCol_TableHeaderBg]         = Grey(0.17f);
		axColours[ImGuiCol_TableBorderStrong]     = Grey(0.07f);
		axColours[ImGuiCol_TableBorderLight]      = Grey(0.16f);
		axColours[ImGuiCol_TableRowBg]            = Grey(0.00f, 0.0f);
		axColours[ImGuiCol_TableRowBgAlt]         = Grey(1.00f, 0.025f);
		axColours[ImGuiCol_TextSelectedBg]        = ImVec4(0.28f, 0.56f, 0.96f, 0.35f);
		axColours[ImGuiCol_DragDropTarget]        = ImVec4(0.28f, 0.56f, 0.96f, 0.90f);
		axColours[ImGuiCol_NavCursor]             = xAccent;
		axColours[ImGuiCol_NavWindowingHighlight] = Grey(1.00f, 0.70f);
		axColours[ImGuiCol_NavWindowingDimBg]     = Grey(0.80f, 0.20f);
		axColours[ImGuiCol_ModalWindowDimBg]      = Grey(0.00f, 0.55f);
		axColours[ImGuiCol_PlotLines]             = Grey(0.61f);
		axColours[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		axColours[ImGuiCol_PlotHistogram]         = ImVec4(0.28f, 0.56f, 0.96f, 0.85f);
		axColours[ImGuiCol_PlotHistogramHovered]  = xAccent;

		// The swapchain is sRGB: it converts linear -> sRGB on write, so the style
		// must carry LINEAR values to come out as the sRGB colours authored above.
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			axColours[i].x = powf(axColours[i].x, 2.2f);
			axColours[i].y = powf(axColours[i].y, 2.2f);
			axColours[i].z = powf(axColours[i].z, 2.2f);
		}
	}
}

void ApplyTheme()
{
	EditorUIState& xState = State();
	if (!xState.m_bFontsLoaded)
	{
		// The style is scale-dependent, so settle the scale first.
		xState.m_fUIScale = QueryContentScale();
	}

	ImGuiStyle& xStyle = ImGui::GetStyle();
	xStyle = ImGuiStyle();
	ApplyLayout(xStyle);
	ApplyColours(xStyle.Colors);
	xStyle.ScaleAllSizes(xState.m_fUIScale);
	xStyle.FontSizeBase = fFONT_SIZE_BASE;
	xStyle.FontScaleDpi = xState.m_fUIScale;

	FillPalette(xState.m_xPalette);
}

void PushPlayModeTint()
{
	EditorUIState& xState = State();
	const ImVec4& xTint = xState.m_xPalette.m_xPlayTint;
	const ImGuiCol aeTinted[] =
	{
		ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_MenuBarBg,
		ImGuiCol_Tab, ImGuiCol_TabSelected, ImGuiCol_TabHovered, ImGuiCol_TabDimmed, ImGuiCol_TabDimmedSelected,
		ImGuiCol_WindowBg, ImGuiCol_TableHeaderBg, ImGuiCol_DockingEmptyBg,
	};
	xState.m_iPlayTintPushCount = 0;
	for (ImGuiCol eCol : aeTinted)
	{
		const ImVec4 xBase = ImGui::GetStyleColorVec4(eCol);
		const float fMix = 0.22f;
		const ImVec4 xMixed(
			xBase.x + (xTint.x - xBase.x) * fMix,
			xBase.y + (xTint.y - xBase.y) * fMix,
			xBase.z + (xTint.z - xBase.z) * fMix,
			xBase.w);
		ImGui::PushStyleColor(eCol, xMixed);
		++xState.m_iPlayTintPushCount;
	}
}

void PopPlayModeTint()
{
	EditorUIState& xState = State();
	if (xState.m_iPlayTintPushCount > 0)
	{
		ImGui::PopStyleColor(xState.m_iPlayTintPushCount);
		xState.m_iPlayTintPushCount = 0;
	}
}

//=============================================================================
// Icons
//=============================================================================
namespace
{
	struct IconContext
	{
		ImDrawList* m_pxDraw;
		ImVec2 m_xC;      // centre
		float m_fH;       // half size
		float m_fT;       // stroke thickness
		ImU32 m_uCol;

		ImVec2 P(float fX, float fY) const { return ImVec2(m_xC.x + fX * m_fH, m_xC.y + fY * m_fH); }
		void Line(float fX0, float fY0, float fX1, float fY1) const { m_pxDraw->AddLine(P(fX0, fY0), P(fX1, fY1), m_uCol, m_fT); }
		void Circle(float fX, float fY, float fR) const { m_pxDraw->AddCircle(P(fX, fY), fR * m_fH, m_uCol, 0, m_fT); }
		void Disc(float fX, float fY, float fR) const { m_pxDraw->AddCircleFilled(P(fX, fY), fR * m_fH, m_uCol); }
		void Rect(float fX0, float fY0, float fX1, float fY1, float fRound = 0.0f) const { m_pxDraw->AddRect(P(fX0, fY0), P(fX1, fY1), m_uCol, fRound * m_fH, 0, m_fT); }
		void FillRect(float fX0, float fY0, float fX1, float fY1, float fRound = 0.0f) const { m_pxDraw->AddRectFilled(P(fX0, fY0), P(fX1, fY1), m_uCol, fRound * m_fH); }
		void Tri(float fX0, float fY0, float fX1, float fY1, float fX2, float fY2) const { m_pxDraw->AddTriangleFilled(P(fX0, fY0), P(fX1, fY1), P(fX2, fY2), m_uCol); }

		// Arrow head pointing along (dx,dy) with its tip at (x,y).
		void Head(float fX, float fY, float fDX, float fDY, float fSize) const
		{
			const float fLen = sqrtf(fDX * fDX + fDY * fDY);
			if (fLen < 1e-4f) return;
			const float fUX = fDX / fLen, fUY = fDY / fLen;
			const float fPX = -fUY, fPY = fUX;
			Tri(fX, fY,
				fX - fUX * fSize + fPX * fSize * 0.6f, fY - fUY * fSize + fPY * fSize * 0.6f,
				fX - fUX * fSize - fPX * fSize * 0.6f, fY - fUY * fSize - fPY * fSize * 0.6f);
		}

		void Arc(float fX, float fY, float fR, float fA0, float fA1) const
		{
			m_pxDraw->PathArcTo(P(fX, fY), fR * m_fH, fA0, fA1, 24);
			m_pxDraw->PathStroke(m_uCol, 0, m_fT);
		}

		// Curved arrow: an arc from fA0 to fA1 with a head at the fA1 end.
		void ArcArrow(float fR, float fA0, float fA1, float fHeadSize) const
		{
			Arc(0.0f, 0.0f, fR, fA0, fA1);
			const float fTipX = cosf(fA1) * fR, fTipY = sinf(fA1) * fR;
			const float fDir = (fA1 > fA0) ? 1.0f : -1.0f;
			Head(fTipX, fTipY, -sinf(fA1) * fDir, cosf(fA1) * fDir, fHeadSize);
		}

		// Hexagonal wireframe cube (isometric).
		void Cube(bool bFilled) const
		{
			const float fS = 0.85f;
			ImVec2 axPts[6] =
			{
				P(0.0f, -fS), P(fS * 0.87f, -fS * 0.5f), P(fS * 0.87f, fS * 0.5f),
				P(0.0f, fS), P(-fS * 0.87f, fS * 0.5f), P(-fS * 0.87f, -fS * 0.5f)
			};
			if (bFilled)
			{
				m_pxDraw->AddConvexPolyFilled(axPts, 6, (m_uCol & 0x00FFFFFF) | 0x40000000);
			}
			m_pxDraw->AddPolyline(axPts, 6, m_uCol, ImDrawFlags_Closed, m_fT);
			Line(0.0f, 0.0f, 0.0f, fS);
			Line(0.0f, 0.0f, fS * 0.87f, -fS * 0.5f);
			Line(0.0f, 0.0f, -fS * 0.87f, -fS * 0.5f);
		}
	};

	void DrawIconTransport(const IconContext& xI, Zenith_EditorIcon eIcon)
	{
		switch (eIcon)
		{
		case Zenith_EditorIcon::Play:   xI.Tri(-0.65f, -0.85f, -0.65f, 0.85f, 0.85f, 0.0f); break;
		case Zenith_EditorIcon::Pause:  xI.FillRect(-0.7f, -0.8f, -0.2f, 0.8f, 0.15f); xI.FillRect(0.2f, -0.8f, 0.7f, 0.8f, 0.15f); break;
		case Zenith_EditorIcon::Stop:   xI.FillRect(-0.7f, -0.7f, 0.7f, 0.7f, 0.2f); break;
		case Zenith_EditorIcon::Undo:   xI.ArcArrow(0.62f, -0.35f * 3.14159f, 1.15f * 3.14159f, 0.55f); break;
		case Zenith_EditorIcon::Redo:   xI.ArcArrow(0.62f, 1.35f * 3.14159f, -0.15f * 3.14159f, 0.55f); break;
		case Zenith_EditorIcon::Refresh: xI.ArcArrow(0.62f, -0.4f * 3.14159f, 1.35f * 3.14159f, 0.5f); break;
		default: break;
		}
	}

	void DrawIconGizmo(const IconContext& xI, Zenith_EditorIcon eIcon)
	{
		switch (eIcon)
		{
		case Zenith_EditorIcon::Translate:
			xI.Line(-0.9f, 0.0f, 0.9f, 0.0f); xI.Line(0.0f, -0.9f, 0.0f, 0.9f);
			xI.Head(0.95f, 0.0f, 1.0f, 0.0f, 0.4f); xI.Head(-0.95f, 0.0f, -1.0f, 0.0f, 0.4f);
			xI.Head(0.0f, -0.95f, 0.0f, -1.0f, 0.4f); xI.Head(0.0f, 0.95f, 0.0f, 1.0f, 0.4f);
			break;
		case Zenith_EditorIcon::Rotate:
			xI.ArcArrow(0.7f, -0.45f * 3.14159f, 1.3f * 3.14159f, 0.5f);
			xI.Disc(0.0f, 0.0f, 0.14f);
			break;
		case Zenith_EditorIcon::Scale:
			xI.Line(-0.55f, 0.55f, 0.7f, -0.7f);
			xI.Line(0.75f, -0.75f, 0.75f, -0.15f); xI.Line(0.75f, -0.75f, 0.15f, -0.75f);
			xI.Rect(-0.85f, 0.05f, -0.05f, 0.85f);
			break;
		case Zenith_EditorIcon::World:
			xI.Circle(0.0f, 0.0f, 0.8f);
			xI.Line(-0.8f, 0.0f, 0.8f, 0.0f);
			xI.Arc(0.0f, 0.0f, 0.8f, 0.0f, 0.0f);
			xI.m_pxDraw->PathArcTo(xI.P(0.0f, 0.0f), 0.8f * xI.m_fH, 1.5708f, 4.7124f, 16);
			xI.m_pxDraw->PathStroke(xI.m_uCol, 0, xI.m_fT);
			xI.m_pxDraw->AddEllipse(xI.P(0.0f, 0.0f), ImVec2(0.35f * xI.m_fH, 0.8f * xI.m_fH), xI.m_uCol, 0.0f, 0, xI.m_fT);
			break;
		case Zenith_EditorIcon::Local:
		case Zenith_EditorIcon::Mesh:
		case Zenith_EditorIcon::Prefab:
			xI.Cube(eIcon != Zenith_EditorIcon::Local);
			break;
		case Zenith_EditorIcon::Snap:
		case Zenith_EditorIcon::Grid:
			for (int i = 0; i < 3; ++i)
			{
				const float fA = -0.75f + 0.75f * static_cast<float>(i);
				xI.Line(fA, -0.8f, fA, 0.8f);
				xI.Line(-0.8f, fA, 0.8f, fA);
			}
			if (eIcon == Zenith_EditorIcon::Snap) xI.Disc(0.0f, 0.0f, 0.3f);
			break;
		case Zenith_EditorIcon::Focus:
			xI.Circle(0.0f, 0.0f, 0.5f);
			xI.Line(0.0f, -0.95f, 0.0f, -0.55f); xI.Line(0.0f, 0.55f, 0.0f, 0.95f);
			xI.Line(-0.95f, 0.0f, -0.55f, 0.0f); xI.Line(0.55f, 0.0f, 0.95f, 0.0f);
			xI.Disc(0.0f, 0.0f, 0.12f);
			break;
		default: break;
		}
	}

	void DrawIconFiles(const IconContext& xI, Zenith_EditorIcon eIcon)
	{
		switch (eIcon)
		{
		case Zenith_EditorIcon::Folder:
		case Zenith_EditorIcon::FolderOpen:
			xI.FillRect(-0.9f, -0.75f, -0.15f, -0.4f, 0.15f);
			xI.FillRect(-0.9f, -0.55f, 0.9f, 0.7f, 0.15f);
			if (eIcon == Zenith_EditorIcon::FolderOpen)
			{
				xI.m_pxDraw->AddLine(xI.P(-0.9f, -0.15f), xI.P(0.9f, -0.15f), (xI.m_uCol & 0x00FFFFFF) | 0x60000000, xI.m_fT);
			}
			break;
		case Zenith_EditorIcon::File:
		case Zenith_EditorIcon::Text:
			xI.Rect(-0.6f, -0.85f, 0.6f, 0.85f, 0.1f);
			xI.Line(0.2f, -0.85f, 0.6f, -0.45f); xI.Line(0.2f, -0.85f, 0.2f, -0.45f); xI.Line(0.2f, -0.45f, 0.6f, -0.45f);
			if (eIcon == Zenith_EditorIcon::Text)
			{
				xI.Line(-0.35f, 0.0f, 0.35f, 0.0f); xI.Line(-0.35f, 0.3f, 0.35f, 0.3f); xI.Line(-0.35f, 0.6f, 0.1f, 0.6f);
			}
			break;
		case Zenith_EditorIcon::Texture:
			xI.Rect(-0.85f, -0.85f, 0.85f, 0.85f, 0.1f);
			xI.FillRect(-0.85f, -0.85f, 0.0f, 0.0f); xI.FillRect(0.0f, 0.0f, 0.85f, 0.85f);
			break;
		case Zenith_EditorIcon::Material:
			xI.Disc(0.0f, 0.0f, 0.8f);
			xI.m_pxDraw->AddCircleFilled(xI.P(-0.28f, -0.3f), 0.22f * xI.m_fH, (xI.m_uCol & 0x00FFFFFF) | 0xA0FFFFFF);
			break;
		case Zenith_EditorIcon::Scene:
			xI.Rect(-0.9f, -0.7f, 0.9f, 0.7f, 0.1f);
			xI.Line(-0.9f, 0.4f, -0.3f, -0.2f); xI.Line(-0.3f, -0.2f, 0.1f, 0.2f); xI.Line(0.1f, 0.2f, 0.45f, -0.1f); xI.Line(0.45f, -0.1f, 0.9f, 0.4f);
			xI.Disc(0.45f, -0.35f, 0.14f);
			break;
		case Zenith_EditorIcon::Animation:
			xI.Circle(0.0f, 0.0f, 0.85f);
			xI.Tri(-0.3f, -0.5f, -0.3f, 0.5f, 0.55f, 0.0f);
			break;
		case Zenith_EditorIcon::Graph:
			xI.Line(-0.55f, -0.5f, 0.55f, 0.0f); xI.Line(-0.55f, 0.5f, 0.55f, 0.0f);
			xI.Disc(-0.55f, -0.5f, 0.3f); xI.Disc(-0.55f, 0.5f, 0.3f); xI.Disc(0.55f, 0.0f, 0.3f);
			break;
		case Zenith_EditorIcon::Save:
			xI.Rect(-0.8f, -0.8f, 0.8f, 0.8f, 0.1f);
			xI.FillRect(-0.45f, -0.8f, 0.45f, -0.25f);
			xI.Rect(-0.45f, 0.2f, 0.45f, 0.8f);
			break;
		case Zenith_EditorIcon::Copy:
			xI.Rect(-0.85f, -0.85f, 0.35f, 0.35f, 0.1f);
			xI.FillRect(-0.35f, -0.35f, 0.85f, 0.85f, 0.1f);
			break;
		case Zenith_EditorIcon::Trash:
			xI.Line(-0.85f, -0.55f, 0.85f, -0.55f);
			xI.Line(-0.3f, -0.85f, 0.3f, -0.85f);
			xI.Rect(-0.6f, -0.55f, 0.6f, 0.85f, 0.1f);
			xI.Line(-0.2f, -0.2f, -0.2f, 0.55f); xI.Line(0.2f, -0.2f, 0.2f, 0.55f);
			break;
		default: break;
		}
	}

	void DrawIconEntities(const IconContext& xI, Zenith_EditorIcon eIcon)
	{
		switch (eIcon)
		{
		case Zenith_EditorIcon::Camera:
			xI.FillRect(-0.9f, -0.45f, 0.5f, 0.6f, 0.15f);
			xI.Tri(0.5f, -0.1f, 0.95f, -0.45f, 0.95f, 0.6f);
			xI.FillRect(-0.7f, -0.75f, -0.2f, -0.45f, 0.1f);
			break;
		case Zenith_EditorIcon::Light:
			xI.Circle(0.0f, -0.2f, 0.5f);
			xI.FillRect(-0.25f, 0.35f, 0.25f, 0.65f, 0.1f);
			xI.Line(0.0f, -1.0f, 0.0f, -0.85f); xI.Line(-0.9f, -0.2f, -0.72f, -0.2f); xI.Line(0.72f, -0.2f, 0.9f, -0.2f);
			xI.Line(-0.65f, -0.8f, -0.52f, -0.67f); xI.Line(0.65f, -0.8f, 0.52f, -0.67f);
			break;
		case Zenith_EditorIcon::Sun:
			xI.Disc(0.0f, 0.0f, 0.38f);
			for (int i = 0; i < 8; ++i)
			{
				const float fA = static_cast<float>(i) * 0.7854f;
				xI.Line(cosf(fA) * 0.6f, sinf(fA) * 0.6f, cosf(fA) * 0.95f, sinf(fA) * 0.95f);
			}
			break;
		case Zenith_EditorIcon::Terrain:
			xI.Line(-0.95f, 0.7f, -0.35f, -0.4f); xI.Line(-0.35f, -0.4f, 0.05f, 0.2f);
			xI.Line(0.05f, 0.2f, 0.4f, -0.7f); xI.Line(0.4f, -0.7f, 0.95f, 0.7f); xI.Line(-0.95f, 0.7f, 0.95f, 0.7f);
			break;
		case Zenith_EditorIcon::Entity:
			xI.Rect(-0.6f, -0.6f, 0.6f, 0.6f, 0.25f);
			xI.Disc(0.0f, 0.0f, 0.2f);
			break;
		case Zenith_EditorIcon::Particles:
			xI.Disc(-0.45f, 0.35f, 0.3f); xI.Disc(0.4f, -0.4f, 0.4f); xI.Disc(0.45f, 0.5f, 0.18f); xI.Disc(-0.35f, -0.55f, 0.16f);
			break;
		case Zenith_EditorIcon::UI:
			xI.Rect(-0.9f, -0.7f, 0.9f, 0.7f, 0.1f);
			xI.Line(-0.9f, -0.3f, 0.9f, -0.3f);
			xI.FillRect(-0.6f, 0.0f, 0.1f, 0.35f, 0.1f);
			break;
		case Zenith_EditorIcon::Collider:
			xI.Rect(-0.8f, -0.8f, 0.8f, 0.8f, 0.15f);
			xI.Circle(0.0f, 0.0f, 0.4f);
			break;
		case Zenith_EditorIcon::Attachment:
			xI.Circle(-0.4f, 0.0f, 0.4f); xI.Circle(0.4f, 0.0f, 0.4f);
			break;
		case Zenith_EditorIcon::Console:
			xI.Rect(-0.9f, -0.7f, 0.9f, 0.7f, 0.1f);
			xI.Line(-0.6f, -0.3f, -0.2f, 0.0f); xI.Line(-0.2f, 0.0f, -0.6f, 0.3f); xI.Line(0.0f, 0.35f, 0.5f, 0.35f);
			break;
		case Zenith_EditorIcon::Layout:
			xI.Rect(-0.9f, -0.8f, 0.9f, 0.8f, 0.1f);
			xI.Line(-0.3f, -0.8f, -0.3f, 0.8f); xI.Line(-0.3f, 0.1f, 0.9f, 0.1f);
			break;
		default: break;
		}
	}

	void DrawIconGlyphs(const IconContext& xI, Zenith_EditorIcon eIcon)
	{
		switch (eIcon)
		{
		case Zenith_EditorIcon::Search:
			xI.Circle(-0.2f, -0.2f, 0.55f);
			xI.m_pxDraw->AddLine(xI.P(0.2f, 0.2f), xI.P(0.85f, 0.85f), xI.m_uCol, xI.m_fT * 1.6f);
			break;
		case Zenith_EditorIcon::Eye:
		case Zenith_EditorIcon::EyeOff:
			xI.m_pxDraw->PathArcTo(xI.P(0.0f, 0.55f), 1.0f * xI.m_fH, 3.8f, 5.6f, 16); xI.m_pxDraw->PathStroke(xI.m_uCol, 0, xI.m_fT);
			xI.m_pxDraw->PathArcTo(xI.P(0.0f, -0.55f), 1.0f * xI.m_fH, 0.65f, 2.5f, 16); xI.m_pxDraw->PathStroke(xI.m_uCol, 0, xI.m_fT);
			xI.Disc(0.0f, 0.0f, 0.28f);
			if (eIcon == Zenith_EditorIcon::EyeOff) xI.m_pxDraw->AddLine(xI.P(-0.8f, 0.8f), xI.P(0.8f, -0.8f), xI.m_uCol, xI.m_fT * 1.3f);
			break;
		case Zenith_EditorIcon::Close:
			xI.Line(-0.6f, -0.6f, 0.6f, 0.6f); xI.Line(-0.6f, 0.6f, 0.6f, -0.6f);
			break;
		case Zenith_EditorIcon::Plus:
			xI.Line(-0.7f, 0.0f, 0.7f, 0.0f); xI.Line(0.0f, -0.7f, 0.0f, 0.7f);
			break;
		case Zenith_EditorIcon::Minus:
			xI.Line(-0.7f, 0.0f, 0.7f, 0.0f);
			break;
		case Zenith_EditorIcon::ArrowLeft:  xI.Line(0.4f, -0.7f, -0.35f, 0.0f); xI.Line(-0.35f, 0.0f, 0.4f, 0.7f); break;
		case Zenith_EditorIcon::ArrowRight: xI.Line(-0.4f, -0.7f, 0.35f, 0.0f); xI.Line(0.35f, 0.0f, -0.4f, 0.7f); break;
		case Zenith_EditorIcon::ArrowUp:    xI.Line(-0.7f, 0.4f, 0.0f, -0.35f); xI.Line(0.0f, -0.35f, 0.7f, 0.4f); break;
		case Zenith_EditorIcon::ArrowDown:  xI.Line(-0.7f, -0.4f, 0.0f, 0.35f); xI.Line(0.0f, 0.35f, 0.7f, -0.4f); break;
		case Zenith_EditorIcon::Info:
			xI.Circle(0.0f, 0.0f, 0.85f); xI.Disc(0.0f, -0.42f, 0.12f); xI.Line(0.0f, -0.1f, 0.0f, 0.5f);
			break;
		case Zenith_EditorIcon::Warning:
			xI.m_pxDraw->AddTriangle(xI.P(0.0f, -0.85f), xI.P(0.95f, 0.8f), xI.P(-0.95f, 0.8f), xI.m_uCol, xI.m_fT);
			xI.Line(0.0f, -0.35f, 0.0f, 0.25f); xI.Disc(0.0f, 0.52f, 0.11f);
			break;
		case Zenith_EditorIcon::Error:
			xI.Circle(0.0f, 0.0f, 0.85f); xI.Line(-0.4f, -0.4f, 0.4f, 0.4f); xI.Line(-0.4f, 0.4f, 0.4f, -0.4f);
			break;
		case Zenith_EditorIcon::Settings:
			xI.Circle(0.0f, 0.0f, 0.55f); xI.Disc(0.0f, 0.0f, 0.2f);
			for (int i = 0; i < 8; ++i)
			{
				const float fA = static_cast<float>(i) * 0.7854f;
				xI.Line(cosf(fA) * 0.55f, sinf(fA) * 0.55f, cosf(fA) * 0.9f, sinf(fA) * 0.9f);
			}
			break;
		case Zenith_EditorIcon::Lock:
			xI.FillRect(-0.7f, -0.1f, 0.7f, 0.85f, 0.15f);
			xI.m_pxDraw->PathArcTo(xI.P(0.0f, -0.15f), 0.45f * xI.m_fH, 3.14159f, 6.28318f, 12); xI.m_pxDraw->PathStroke(xI.m_uCol, 0, xI.m_fT);
			break;
		default: break;
		}
	}
}

void DrawIcon(ImDrawList* pxDraw, Zenith_EditorIcon eIcon, ImVec2 xCentre, float fSize, ImU32 uColour)
{
	IconContext xI;
	xI.m_pxDraw = pxDraw;
	xI.m_xC = xCentre;
	xI.m_fH = fSize * 0.5f;
	xI.m_fT = ImMax(1.0f, fSize * 0.11f);
	xI.m_uCol = uColour;

	// Each group is a switch over its own icons and ignores the rest, so the
	// dispatch is simply "try every group".
	DrawIconTransport(xI, eIcon);
	DrawIconGizmo(xI, eIcon);
	DrawIconFiles(xI, eIcon);
	DrawIconEntities(xI, eIcon);
	DrawIconGlyphs(xI, eIcon);
}

Zenith_EditorAssetTypeStyle GetAssetTypeStyle(const char* szExtension, bool bIsDirectory)
{
	const Zenith_EditorPalette& xP = Palette();
	Zenith_EditorAssetTypeStyle xStyle;
	if (bIsDirectory)
	{
		xStyle.m_uColour = xP.m_uTypeFolder;
		xStyle.m_szShortLabel = "";
		xStyle.m_eIcon = Zenith_EditorIcon::Folder;
		return xStyle;
	}

	struct Row { const char* m_szExt; ImU32 m_uColour; const char* m_szLabel; Zenith_EditorIcon m_eIcon; };
	const Row axRows[] =
	{
		{ ZENITH_TEXTURE_EXT,   xP.m_uTypeTexture,   "TEX",  Zenith_EditorIcon::Texture },
		{ ZENITH_MATERIAL_EXT,  xP.m_uTypeMaterial,  "MAT",  Zenith_EditorIcon::Material },
		{ ZENITH_MESH_EXT,      xP.m_uTypeMesh,      "MESH", Zenith_EditorIcon::Mesh },
		{ ZENITH_GEOMETRY_EXT,  xP.m_uTypeMesh,      "GEO",  Zenith_EditorIcon::Mesh },
		{ ZENITH_MODEL_EXT,     xP.m_uTypeModel,     "MDL",  Zenith_EditorIcon::Mesh },
		{ ZENITH_PREFAB_EXT,    xP.m_uTypePrefab,    "PFB",  Zenith_EditorIcon::Prefab },
		{ ZENITH_SCENE_EXT,     xP.m_uTypeScene,     "SCN",  Zenith_EditorIcon::Scene },
		{ ZENITH_ANIMATION_EXT, xP.m_uTypeAnimation, "ANIM", Zenith_EditorIcon::Animation },
		{ ZENITH_BGRAPH_EXT,    xP.m_uTypeGraph,     "BGR",  Zenith_EditorIcon::Graph },
	};
	for (const Row& xRow : axRows)
	{
		if (szExtension != nullptr && strcmp(szExtension, xRow.m_szExt) == 0)
		{
			xStyle.m_uColour = xRow.m_uColour;
			xStyle.m_szShortLabel = xRow.m_szLabel;
			xStyle.m_eIcon = xRow.m_eIcon;
			return xStyle;
		}
	}
	xStyle.m_uColour = xP.m_uTypeOther;
	xStyle.m_szShortLabel = (szExtension != nullptr && szExtension[0] == '.') ? szExtension + 1 : "";
	xStyle.m_eIcon = Zenith_EditorIcon::File;
	return xStyle;
}

//=============================================================================
// Widgets
//=============================================================================
namespace
{
	// Background + hover/press feedback shared by both icon-button flavours.
	// Returns the item's screen rect.
	struct ButtonVisual
	{
		ImVec2 m_xMin;
		ImVec2 m_xMax;
		bool m_bHovered;
		bool m_bHeld;
		bool m_bPressed;   // released over the button this frame (ImGui button semantics)
	};

	ButtonVisual DrawButtonFrame(const ImVec2& xSize, const Zenith_EditorIconButtonOptions& xOptions)
	{
		ButtonVisual xVis;
		xVis.m_xMin = ImGui::GetCursorScreenPos();
		xVis.m_xMax = ImVec2(xVis.m_xMin.x + xSize.x, xVis.m_xMin.y + xSize.y);
		xVis.m_bPressed = ImGui::InvisibleButton("##btn", xSize);
		xVis.m_bHovered = ImGui::IsItemHovered();
		xVis.m_bHeld = ImGui::IsItemActive();

		const Zenith_EditorPalette& xP = Palette();
		ImU32 uBg = 0;
		if (!xOptions.m_bEnabled)
		{
			uBg = 0;
		}
		else if (xVis.m_bHeld)
		{
			uBg = xP.m_uFrameActive;
		}
		else if (xVis.m_bHovered)
		{
			uBg = xP.m_uFrameHover;
		}
		else if (xOptions.m_bSelected)
		{
			uBg = xP.m_uSelection;
		}
		else if (!xOptions.m_bFrameless)
		{
			uBg = xP.m_uFrame;
		}
		if (uBg != 0)
		{
			ImGui::GetWindowDrawList()->AddRectFilled(xVis.m_xMin, xVis.m_xMax, uBg, ImGui::GetStyle().FrameRounding);
		}
		if (xOptions.m_bSelected && xOptions.m_bEnabled)
		{
			// Accent underline marks the active mode even while hovered.
			ImGui::GetWindowDrawList()->AddRectFilled(
				ImVec2(xVis.m_xMin.x + Px(4.0f), xVis.m_xMax.y - Px(2.0f)),
				ImVec2(xVis.m_xMax.x - Px(4.0f), xVis.m_xMax.y),
				xP.m_uAccent, 1.0f);
		}
		return xVis;
	}

	ImU32 IconTint(const Zenith_EditorIconButtonOptions& xOptions)
	{
		const Zenith_EditorPalette& xP = Palette();
		if (!xOptions.m_bEnabled) return xP.m_uTextDim;
		if (xOptions.m_uTint != 0) return xOptions.m_uTint;
		return xOptions.m_bSelected ? xP.m_uTextBright : xP.m_uText;
	}

	float ButtonSize(const Zenith_EditorIconButtonOptions& xOptions)
	{
		return xOptions.m_fSize > 0.0f ? xOptions.m_fSize : Px(26.0f);
	}
}

bool IconButton(const char* szID, Zenith_EditorIcon eIcon, const char* szTooltip, const Zenith_EditorIconButtonOptions& xOptions)
{
	ImGui::PushID(szID);
	if (!xOptions.m_bEnabled) ImGui::BeginDisabled();

	const float fSize = ButtonSize(xOptions);
	const ButtonVisual xVis = DrawButtonFrame(ImVec2(fSize, fSize), xOptions);

	const ImVec2 xCentre((xVis.m_xMin.x + xVis.m_xMax.x) * 0.5f, (xVis.m_xMin.y + xVis.m_xMax.y) * 0.5f);
	DrawIcon(ImGui::GetWindowDrawList(), eIcon, xCentre, fSize * 0.58f, IconTint(xOptions));

	if (!xOptions.m_bEnabled) ImGui::EndDisabled();
	if (szTooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("%s", szTooltip);
	}
	ImGui::PopID();
	return xVis.m_bPressed && xOptions.m_bEnabled;
}

bool IconTextButton(const char* szID, Zenith_EditorIcon eIcon, const char* szLabel, const char* szTooltip, const Zenith_EditorIconButtonOptions& xOptions)
{
	ImGui::PushID(szID);
	if (!xOptions.m_bEnabled) ImGui::BeginDisabled();

	const float fHeight = ButtonSize(xOptions);
	const float fIcon = fHeight * 0.55f;
	const ImVec2 xTextSize = ImGui::CalcTextSize(szLabel);
	const float fPad = Px(8.0f);
	const ImVec2 xSize(fPad + fIcon + Px(6.0f) + xTextSize.x + fPad, fHeight);
	const ButtonVisual xVis = DrawButtonFrame(xSize, xOptions);

	const ImU32 uTint = IconTint(xOptions);
	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	DrawIcon(pxDraw, eIcon, ImVec2(xVis.m_xMin.x + fPad + fIcon * 0.5f, (xVis.m_xMin.y + xVis.m_xMax.y) * 0.5f), fIcon, uTint);
	pxDraw->AddText(ImVec2(xVis.m_xMin.x + fPad + fIcon + Px(6.0f), xVis.m_xMin.y + (fHeight - xTextSize.y) * 0.5f), uTint, szLabel);

	if (!xOptions.m_bEnabled) ImGui::EndDisabled();
	if (szTooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("%s", szTooltip);
	}
	ImGui::PopID();
	return xVis.m_bPressed && xOptions.m_bEnabled;
}

void ToolbarSeparator()
{
	ImGui::SameLine(0.0f, Px(6.0f));
	const ImVec2 xPos = ImGui::GetCursorScreenPos();
	const float fHeight = Px(26.0f);
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(xPos.x, xPos.y + Px(5.0f)), ImVec2(xPos.x, xPos.y + fHeight - Px(5.0f)),
		Palette().m_uBorder, 1.0f);
	ImGui::Dummy(ImVec2(1.0f, fHeight));
	ImGui::SameLine(0.0f, Px(6.0f));
}

bool SearchBox(const char* szID, char* pcBuffer, size_t uBufferSize, const char* szHint, float fWidth)
{
	ImGui::PushID(szID);
	const float fHeight = ImGui::GetFrameHeight();
	const float fIconPad = fHeight;
	const ImVec2 xStart = ImGui::GetCursorScreenPos();
	if (fWidth <= 0.0f) fWidth = ImGui::GetContentRegionAvail().x;

	// Leave room for the magnifier on the left and the clear button on the right.
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(fIconPad, ImGui::GetStyle().FramePadding.y));
	ImGui::SetNextItemWidth(fWidth);
	const bool bChanged = ImGui::InputTextWithHint("##search", szHint, pcBuffer, uBufferSize);
	ImGui::PopStyleVar();

	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	const Zenith_EditorPalette& xP = Palette();
	DrawIcon(pxDraw, Zenith_EditorIcon::Search, ImVec2(xStart.x + fIconPad * 0.5f, xStart.y + fHeight * 0.5f), fHeight * 0.5f, xP.m_uTextDim);

	bool bCleared = false;
	if (pcBuffer[0] != '\0')
	{
		const ImVec2 xClearCentre(xStart.x + fWidth - fIconPad * 0.5f, xStart.y + fHeight * 0.5f);
		const ImVec2 xClearMin(xClearCentre.x - fHeight * 0.5f, xStart.y);
		const bool bHovered = ImGui::IsMouseHoveringRect(xClearMin, ImVec2(xClearMin.x + fHeight, xStart.y + fHeight));
		DrawIcon(pxDraw, Zenith_EditorIcon::Close, xClearCentre, fHeight * 0.42f, bHovered ? xP.m_uTextBright : xP.m_uTextDim);
		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			pcBuffer[0] = '\0';
			bCleared = true;
			ImGui::ClearActiveID();
		}
	}
	ImGui::PopID();
	return bChanged || bCleared;
}

ImVec2 DrawBadge(ImDrawList* pxDraw, ImVec2 xPos, const char* szText, ImU32 uBackground, ImU32 uForeground)
{
	PushSmallFont();
	const ImVec2 xTextSize = ImGui::CalcTextSize(szText);
	const float fPadX = Px(6.0f);
	const float fPadY = Px(2.0f);
	const ImVec2 xSize(xTextSize.x + fPadX * 2.0f, xTextSize.y + fPadY * 2.0f);
	pxDraw->AddRectFilled(xPos, ImVec2(xPos.x + xSize.x, xPos.y + xSize.y), uBackground, xSize.y * 0.5f);
	pxDraw->AddText(ImVec2(xPos.x + fPadX, xPos.y + fPadY), uForeground, szText);
	PopFont();
	return xSize;
}

void Badge(const char* szText, ImU32 uBackground, ImU32 uForeground)
{
	const ImVec2 xSize = DrawBadge(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), szText, uBackground, uForeground);
	ImGui::Dummy(xSize);
}

bool ComponentHeader(const char* szID, Zenith_EditorIcon eIcon, const char* szTitle, bool bAllowRemove, bool* pbRemoveClicked)
{
	if (pbRemoveClicked != nullptr) *pbRemoveClicked = false;

	ImGui::PushID(szID);
	const Zenith_EditorPalette& xP = Palette();

	// The header is a framed tree node with the icon drawn into the space the
	// title text is pushed right to leave. AllowOverlap lets the remove button
	// sit ON the header row without stealing its click.
	ImGui::PushStyleColor(ImGuiCol_Header, xP.m_uPanelBgAlt);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, xP.m_uFrameHover);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, xP.m_uFrameActive);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, Px(5.0f)));

	const ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed
		| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_OpenOnArrow
		| ImGuiTreeNodeFlags_OpenOnDoubleClick;
	const float fIconSize = ImGui::GetFontSize();
	// Reserve icon space by prefixing the label with the width in spaces.
	char acLabel[128];
	snprintf(acLabel, sizeof(acLabel), "     %s", szTitle);
	const ImVec2 xRowMin = ImGui::GetCursorScreenPos();
	const bool bOpen = ImGui::TreeNodeEx("##hdr", eFlags, "%s", acLabel);
	const ImVec2 xRowMax = ImGui::GetItemRectMax();
	const float fRowHeight = xRowMax.y - xRowMin.y;

	// Icon: after the arrow, before the title.
	const float fArrowSpace = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f;
	DrawIcon(ImGui::GetWindowDrawList(), eIcon,
		ImVec2(xRowMin.x + fArrowSpace + fIconSize * 0.5f, xRowMin.y + fRowHeight * 0.5f), fIconSize * 0.85f, xP.m_uAccentHover);

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	if (bAllowRemove)
	{
		// Right-aligned remove button overlapping the header row.
		const float fBtn = fRowHeight - Px(4.0f);
		ImGui::SameLine(ImGui::GetContentRegionMax().x - fBtn - Px(2.0f));
		ImGui::SetCursorScreenPos(ImVec2(xRowMax.x - fBtn - Px(4.0f), xRowMin.y + Px(2.0f)));
		Zenith_EditorIconButtonOptions xOpts;
		xOpts.m_fSize = fBtn;
		xOpts.m_uTint = xP.m_uTextDim;
		if (IconButton("remove", Zenith_EditorIcon::Close, "Remove component", xOpts) && pbRemoveClicked != nullptr)
		{
			*pbRemoveClicked = true;
		}
	}

	if (bOpen)
	{
		ImGui::TreePop();
		// Body indent is handled by the caller via Indent so component bodies keep
		// their own layout; nothing pushed here. (TreePop already ran.)
		ImGui::Spacing();
	}
	ImGui::PopID();
	return bOpen;
}

void IconLabel(Zenith_EditorIcon eIcon, const char* szText, ImU32 uIconColour)
{
	const float fIcon = ImGui::GetFontSize();
	const ImVec2 xPos = ImGui::GetCursorScreenPos();
	DrawIcon(ImGui::GetWindowDrawList(), eIcon, ImVec2(xPos.x + fIcon * 0.5f, xPos.y + fIcon * 0.5f + Px(1.0f)), fIcon * 0.85f,
		uIconColour != 0 ? uIconColour : Palette().m_uTextDim);
	ImGui::Dummy(ImVec2(fIcon, fIcon));
	ImGui::SameLine(0.0f, Px(5.0f));
	ImGui::TextUnformatted(szText);
}

void DrawOverlayBackground(ImDrawList* pxDraw, ImVec2 xMin, ImVec2 xMax)
{
	pxDraw->AddRectFilled(xMin, xMax, Palette().m_uOverlayBg, Px(6.0f));
}

bool ContainsCaseInsensitive(const char* szHaystack, const char* szNeedle)
{
	if (szNeedle == nullptr || szNeedle[0] == '\0') return true;
	if (szHaystack == nullptr) return false;
	const size_t uNeedleLen = strlen(szNeedle);
	for (const char* pc = szHaystack; *pc != '\0'; ++pc)
	{
		size_t u = 0;
		while (u < uNeedleLen && pc[u] != '\0' && tolower(static_cast<unsigned char>(pc[u])) == tolower(static_cast<unsigned char>(szNeedle[u])))
		{
			++u;
		}
		if (u == uNeedleLen) return true;
	}
	return false;
}

float SmoothedFrameMs(float fDt)
{
	static float s_fSmoothed = 16.6f;
	s_fSmoothed += (fDt * 1000.0f - s_fSmoothed) * 0.08f;
	return s_fSmoothed;
}

} // namespace Zenith_EditorUI

#endif // ZENITH_TOOLS
