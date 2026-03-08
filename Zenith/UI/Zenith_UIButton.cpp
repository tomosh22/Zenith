#include "Zenith.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Flux/Text/Flux_Text.h"
#include "Input/Zenith_Input.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_BUTTON_VERSION = 2;

static void WriteStyleToStream(Zenith_DataStream& xStream, const UIStyle& xStyle)
{
	xStream << xStyle.m_xFillColor.x; xStream << xStyle.m_xFillColor.y; xStream << xStyle.m_xFillColor.z; xStream << xStyle.m_xFillColor.w;
	xStream << xStyle.m_xGradientBottomColor.x; xStream << xStyle.m_xGradientBottomColor.y; xStream << xStyle.m_xGradientBottomColor.z; xStream << xStyle.m_xGradientBottomColor.w;
	xStream << xStyle.m_xBorderColor.x; xStream << xStyle.m_xBorderColor.y; xStream << xStyle.m_xBorderColor.z; xStream << xStyle.m_xBorderColor.w;
	xStream << xStyle.m_fBorderThickness;
	xStream << xStyle.m_fCornerRadius;
	xStream << xStyle.m_bShadowEnabled;
	xStream << xStyle.m_xShadowColor.x; xStream << xStyle.m_xShadowColor.y; xStream << xStyle.m_xShadowColor.z; xStream << xStyle.m_xShadowColor.w;
	xStream << xStyle.m_xShadowOffset.x; xStream << xStyle.m_xShadowOffset.y;
	xStream << xStyle.m_fShadowSpread;
}

static void ReadStyleFromStream(Zenith_DataStream& xStream, UIStyle& xStyle)
{
	xStream >> xStyle.m_xFillColor.x; xStream >> xStyle.m_xFillColor.y; xStream >> xStyle.m_xFillColor.z; xStream >> xStyle.m_xFillColor.w;
	xStream >> xStyle.m_xGradientBottomColor.x; xStream >> xStyle.m_xGradientBottomColor.y; xStream >> xStyle.m_xGradientBottomColor.z; xStream >> xStyle.m_xGradientBottomColor.w;
	xStream >> xStyle.m_xBorderColor.x; xStream >> xStyle.m_xBorderColor.y; xStream >> xStyle.m_xBorderColor.z; xStream >> xStyle.m_xBorderColor.w;
	xStream >> xStyle.m_fBorderThickness;
	xStream >> xStyle.m_fCornerRadius;
	xStream >> xStyle.m_bShadowEnabled;
	xStream >> xStyle.m_xShadowColor.x; xStream >> xStyle.m_xShadowColor.y; xStream >> xStyle.m_xShadowColor.z; xStream >> xStyle.m_xShadowColor.w;
	xStream >> xStyle.m_xShadowOffset.x; xStream >> xStyle.m_xShadowOffset.y;
	xStream >> xStyle.m_fShadowSpread;
}

Zenith_UIButton::Zenith_UIButton(const std::string& strText, const std::string& strName)
	: Zenith_UIElement(strName)
	, m_strText(strText)
{
	// Default button size
	m_xSize = { 200.0f, 50.0f };

	// Default per-state colors
	m_xNormalStyle.m_xFillColor = {0.25f, 0.25f, 0.30f, 1.0f};
	m_xHoveredStyle.m_xFillColor = {0.35f, 0.35f, 0.45f, 1.0f};
	m_xPressedStyle.m_xFillColor = {0.15f, 0.15f, 0.20f, 1.0f};

	// Default border on all states
	m_xNormalStyle.m_fBorderThickness = 2.0f;
	m_xNormalStyle.m_xBorderColor = {0.5f, 0.5f, 0.6f, 1.0f};
	m_xHoveredStyle.m_fBorderThickness = 2.0f;
	m_xHoveredStyle.m_xBorderColor = {0.5f, 0.5f, 0.6f, 1.0f};
	m_xPressedStyle.m_fBorderThickness = 2.0f;
	m_xPressedStyle.m_xBorderColor = {0.5f, 0.5f, 0.6f, 1.0f};

	m_xCurrentStyle = m_xNormalStyle;
}

void Zenith_UIButton::Update(float fDt)
{
	if (!m_bVisible)
	{
		m_eState = ButtonState::NORMAL;
		return;
	}

#ifdef ZENITH_TOOLS
	if (Zenith_Editor::GetEditorMode() == EditorMode::Stopped)
	{
		m_bFocused = false;
		m_bMousePressedInside = false;
		m_bMouseDownLastFrame = false;
	}
#endif

	// Check group interactable
	bool bInteractable = IsGroupInteractable();

	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_Input::GetMousePosition(xMousePos);

	float fMouseX = static_cast<float>(xMousePos.x);
	float fMouseY = static_cast<float>(xMousePos.y);
#ifdef ZENITH_TOOLS
	{
		Zenith_Maths::Vector2 xViewportPos = Zenith_Editor::GetViewportPos();
		Zenith_Maths::Vector2 xViewportSize = Zenith_Editor::GetViewportSize();
		if (xViewportSize.x > 0.f && xViewportSize.y > 0.f && m_pxCanvas)
		{
			Zenith_Maths::Vector2 xCanvasSize = m_pxCanvas->GetSize();
			fMouseX = (fMouseX - xViewportPos.x) * (xCanvasSize.x / xViewportSize.x);
			fMouseY = (fMouseY - xViewportPos.y) * (xCanvasSize.y / xViewportSize.y);
		}
	}
#endif

	Zenith_Maths::Vector4 xBounds = GetScreenBounds();
	bool bHovered = bInteractable
		&& fMouseX >= xBounds.x
		&& fMouseX <= xBounds.z
		&& fMouseY >= xBounds.y
		&& fMouseY <= xBounds.w;

	bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);

	if (bInteractable)
	{
		if (bMouseDown && !m_bMouseDownLastFrame && bHovered)
		{
			m_bMousePressedInside = true;
		}
		if (!bMouseDown)
		{
			if (m_bMouseDownLastFrame && m_bMousePressedInside && bHovered)
			{
				if (m_pfnOnClick)
				{
					m_pfnOnClick(m_pxUserData);
				}
			}
			m_bMousePressedInside = false;
		}
		m_bMouseDownLastFrame = bMouseDown;

		bool bActivated = m_bFocused
			&& (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ENTER)
				|| Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE));
		if (bActivated)
		{
			if (m_pfnOnClick)
			{
				m_pfnOnClick(m_pxUserData);
			}
		}
	}

	if (m_bMousePressedInside && bHovered && bMouseDown)
	{
		m_eState = ButtonState::PRESSED;
	}
	else if (bHovered)
	{
		m_eState = ButtonState::HOVERED;
	}
	else
	{
		m_eState = ButtonState::NORMAL;
	}

	// Lerp current style toward target
	const UIStyle& xTargetStyle = (m_eState == ButtonState::PRESSED) ? m_xPressedStyle
		: (m_eState == ButtonState::HOVERED) ? m_xHoveredStyle
		: m_xNormalStyle;

	if (m_fTransitionDuration > 0.0f)
	{
		float fSpeed = fDt / m_fTransitionDuration;
		float fT = glm::min(fSpeed, 1.0f);
		m_xCurrentStyle = UIStyle::Lerp(m_xCurrentStyle, xTargetStyle, fT);
	}
	else
	{
		m_xCurrentStyle = xTargetStyle;
	}

	Zenith_UIElement::Update(fDt);
}

void Zenith_UIButton::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	float fAlpha = GetEffectiveAlpha();
	Zenith_Maths::Vector4 xBounds = GetScreenBounds();

	// Override focus border color
	UIStyle xRenderStyle = m_xCurrentStyle;
	if (m_bFocused)
	{
		xRenderStyle.m_xBorderColor = {1.0f, 1.0f, 1.0f, 1.0f};
	}

	UIStyleRenderer::RenderStyledRect(xCanvas, xRenderStyle, xBounds, fAlpha);

	// Render text
	if (!m_strText.empty())
	{
		float fCharWidth = m_fFontSize * fCHAR_SPACING;
		float fTextWidth = m_strText.length() * fCharWidth;
		float fTextHeight = m_fFontSize;

		float fBoundsWidth = xBounds.z - xBounds.x;
		float fBoundsHeight = xBounds.w - xBounds.y;

		Zenith_Maths::Vector2 xTextPos = {
			xBounds.x + (fBoundsWidth - fTextWidth) * 0.5f,
			xBounds.y + (fBoundsHeight - fTextHeight) * 0.5f
		};

		// Text shadow
		if (m_bTextShadowEnabled)
		{
			Zenith_Maths::Vector2 xShadowPos = {
				xTextPos.x + m_xTextShadowOffset.x,
				xTextPos.y + m_xTextShadowOffset.y
			};
			Zenith_Maths::Vector4 xShadowColor = m_xTextShadowColor;
			xShadowColor.a *= fAlpha;
			xCanvas.SubmitText(m_strText, xShadowPos, m_fFontSize, xShadowColor);
		}

		Zenith_Maths::Vector4 xTextColor = m_xTextColor;
		xTextColor.a *= fAlpha;
		xCanvas.SubmitText(m_strText, xTextPos, m_fFontSize, xTextColor);
	}

	Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIButton::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_BUTTON_VERSION;
	xStream << m_strText;
	xStream << m_fFontSize;

	xStream << m_xTextColor.x; xStream << m_xTextColor.y; xStream << m_xTextColor.z; xStream << m_xTextColor.w;

	WriteStyleToStream(xStream, m_xNormalStyle);
	WriteStyleToStream(xStream, m_xHoveredStyle);
	WriteStyleToStream(xStream, m_xPressedStyle);

	xStream << m_fTransitionDuration;

	xStream << m_bTextShadowEnabled;
	xStream << m_xTextShadowColor.x; xStream << m_xTextShadowColor.y; xStream << m_xTextShadowColor.z; xStream << m_xTextShadowColor.w;
	xStream << m_xTextShadowOffset.x; xStream << m_xTextShadowOffset.y;
}

void Zenith_UIButton::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion == UI_BUTTON_VERSION, "UIButton version mismatch");

	xStream >> m_strText;
	xStream >> m_fFontSize;

	xStream >> m_xTextColor.x; xStream >> m_xTextColor.y; xStream >> m_xTextColor.z; xStream >> m_xTextColor.w;

	ReadStyleFromStream(xStream, m_xNormalStyle);
	ReadStyleFromStream(xStream, m_xHoveredStyle);
	ReadStyleFromStream(xStream, m_xPressedStyle);

	xStream >> m_fTransitionDuration;

	xStream >> m_bTextShadowEnabled;
	xStream >> m_xTextShadowColor.x; xStream >> m_xTextShadowColor.y; xStream >> m_xTextShadowColor.z; xStream >> m_xTextShadowColor.w;
	xStream >> m_xTextShadowOffset.x; xStream >> m_xTextShadowOffset.y;

	m_xCurrentStyle = m_xNormalStyle;
}

#ifdef ZENITH_TOOLS
void Zenith_UIButton::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIButtonProps");

	ImGui::Separator();
	ImGui::Text("Button Properties");

	char szTextBuffer[256];
	strncpy_s(szTextBuffer, m_strText.c_str(), sizeof(szTextBuffer) - 1);
	if (ImGui::InputText("Button Text", szTextBuffer, sizeof(szTextBuffer)))
	{
		m_strText = szTextBuffer;
	}

	ImGui::DragFloat("Font Size", &m_fFontSize, 1.0f, 8.0f, 200.0f);

	float fTextColor[4] = { m_xTextColor.x, m_xTextColor.y, m_xTextColor.z, m_xTextColor.w };
	if (ImGui::ColorEdit4("Text Color", fTextColor))
	{
		m_xTextColor = { fTextColor[0], fTextColor[1], fTextColor[2], fTextColor[3] };
	}

	ImGui::Separator();
	ImGui::Text("State Colors");

	float fNormalColor[4] = { m_xNormalStyle.m_xFillColor.x, m_xNormalStyle.m_xFillColor.y, m_xNormalStyle.m_xFillColor.z, m_xNormalStyle.m_xFillColor.w };
	if (ImGui::ColorEdit4("Normal", fNormalColor))
	{
		m_xNormalStyle.m_xFillColor = { fNormalColor[0], fNormalColor[1], fNormalColor[2], fNormalColor[3] };
	}

	float fHoverColor[4] = { m_xHoveredStyle.m_xFillColor.x, m_xHoveredStyle.m_xFillColor.y, m_xHoveredStyle.m_xFillColor.z, m_xHoveredStyle.m_xFillColor.w };
	if (ImGui::ColorEdit4("Hover", fHoverColor))
	{
		m_xHoveredStyle.m_xFillColor = { fHoverColor[0], fHoverColor[1], fHoverColor[2], fHoverColor[3] };
	}

	float fPressedColor[4] = { m_xPressedStyle.m_xFillColor.x, m_xPressedStyle.m_xFillColor.y, m_xPressedStyle.m_xFillColor.z, m_xPressedStyle.m_xFillColor.w };
	if (ImGui::ColorEdit4("Pressed", fPressedColor))
	{
		m_xPressedStyle.m_xFillColor = { fPressedColor[0], fPressedColor[1], fPressedColor[2], fPressedColor[3] };
	}

	ImGui::Separator();
	ImGui::Text("Style (all states)");
	ImGui::DragFloat("Corner Radius", &m_xNormalStyle.m_fCornerRadius, 0.5f, 0.0f, 100.0f);
	ImGui::DragFloat("Border Thickness", &m_xNormalStyle.m_fBorderThickness, 0.5f, 0.0f, 50.0f);
	ImGui::DragFloat("Transition Duration", &m_fTransitionDuration, 0.01f, 0.0f, 2.0f);

	ImGui::Checkbox("Shadow", &m_xNormalStyle.m_bShadowEnabled);
	ImGui::Checkbox("Text Shadow", &m_bTextShadowEnabled);

	const char* szStates[] = { "Normal", "Hovered", "Pressed" };
	ImGui::Text("Current State: %s", szStates[static_cast<int>(m_eState)]);
	ImGui::Text("Focused: %s", m_bFocused ? "Yes" : "No");

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
