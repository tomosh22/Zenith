#include "Zenith.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "Input/Zenith_Input.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_BUTTON_VERSION = 1;

// Character width as fraction of height (must match Flux_Text.vert and Flux_Text.cpp)
static constexpr float CHAR_ASPECT_RATIO = 0.5f;
static constexpr float CHAR_SPACING = CHAR_ASPECT_RATIO * 1.1f;

Zenith_UIButton::Zenith_UIButton(const std::string& strText, const std::string& strName)
	: Zenith_UIElement(strName)
	, m_strText(strText)
{
	// Default button size
	m_xSize = { 200.0f, 50.0f };
}

void Zenith_UIButton::Update(float fDt)
{
	if (!m_bVisible)
	{
		m_eState = ButtonState::NORMAL;
		return;
	}

#ifdef ZENITH_TOOLS
	// Clear transient runtime state when editor is Stopped - DontDestroyOnLoad
	// entities survive the Play/Stop cycle but these are set by game scripts
	if (Zenith_Editor::GetEditorMode() == EditorMode::Stopped)
	{
		m_bFocused = false;
		m_bMousePressedInside = false;
		m_bMouseDownLastFrame = false;
	}
#endif

	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_Input::GetMousePosition(xMousePos);

	// In tools builds, transform mouse from window space to render-target space.
	// The game renders to an offscreen texture displayed inside an ImGui viewport panel,
	// so GLFW window coordinates don't match render-target coordinates.
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

	Zenith_Maths::Vector4 xBounds = GetScreenBounds(); // {left, top, right, bottom}
	bool bHovered = fMouseX >= xBounds.x
		&& fMouseX <= xBounds.z
		&& fMouseY >= xBounds.y
		&& fMouseY <= xBounds.w;

	bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);

	// Track mouse press inside button for click-on-release
	if (bMouseDown && !m_bMouseDownLastFrame && bHovered)
	{
		m_bMousePressedInside = true;
	}
	if (!bMouseDown)
	{
		// Mouse released - fire callback if released while hovering and press started inside
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

	// Keyboard activation (Enter/Space when focused)
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

	// Visual state: mouse hover shows HOVERED, focus only affects border (not background)
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

	// Update children
	Zenith_UIElement::Update(fDt);
}

void Zenith_UIButton::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	Zenith_Maths::Vector4 xBounds = GetScreenBounds();

	// 1. Render border (full bounds, brighter when focused for keyboard navigation)
	if (m_fBorderThickness > 0.0f)
	{
		Zenith_Maths::Vector4 xBorder = m_xBorderColor;
		if (m_bFocused)
		{
			xBorder = {1.0f, 1.0f, 1.0f, 1.0f};
		}
		xCanvas.SubmitQuad(xBounds, xBorder, 0);

		// Inset for the background area
		xBounds.x += m_fBorderThickness;
		xBounds.y += m_fBorderThickness;
		xBounds.z -= m_fBorderThickness;
		xBounds.w -= m_fBorderThickness;
	}

	// 2. Render background (color based on state)
	Zenith_Maths::Vector4 xBgColor;
	switch (m_eState)
	{
	case ButtonState::HOVERED:
		xBgColor = m_xHoverColor;
		break;
	case ButtonState::PRESSED:
		xBgColor = m_xPressedColor;
		break;
	default:
		xBgColor = m_xNormalColor;
		break;
	}
	xCanvas.SubmitQuad(xBounds, xBgColor, 0);

	// 3. Render centered text
	if (!m_strText.empty())
	{
		float fCharWidth = m_fFontSize * CHAR_SPACING;
		float fTextWidth = m_strText.length() * fCharWidth;
		float fTextHeight = m_fFontSize;

		float fBoundsWidth = xBounds.z - xBounds.x;
		float fBoundsHeight = xBounds.w - xBounds.y;

		Zenith_Maths::Vector2 xTextPos = {
			xBounds.x + (fBoundsWidth - fTextWidth) * 0.5f,
			xBounds.y + (fBoundsHeight - fTextHeight) * 0.5f
		};

		xCanvas.SubmitText(m_strText, xTextPos, m_fFontSize, m_xTextColor);
	}

	// Render children
	Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIButton::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write base class data
	Zenith_UIElement::WriteToDataStream(xStream);

	// Write button-specific data
	xStream << UI_BUTTON_VERSION;
	xStream << m_strText;
	xStream << m_fFontSize;

	// Text color
	xStream << m_xTextColor.x;
	xStream << m_xTextColor.y;
	xStream << m_xTextColor.z;
	xStream << m_xTextColor.w;

	// State colors
	xStream << m_xNormalColor.x;
	xStream << m_xNormalColor.y;
	xStream << m_xNormalColor.z;
	xStream << m_xNormalColor.w;

	xStream << m_xHoverColor.x;
	xStream << m_xHoverColor.y;
	xStream << m_xHoverColor.z;
	xStream << m_xHoverColor.w;

	xStream << m_xPressedColor.x;
	xStream << m_xPressedColor.y;
	xStream << m_xPressedColor.z;
	xStream << m_xPressedColor.w;

	// Border
	xStream << m_fBorderThickness;
	xStream << m_xBorderColor.x;
	xStream << m_xBorderColor.y;
	xStream << m_xBorderColor.z;
	xStream << m_xBorderColor.w;
}

void Zenith_UIButton::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read base class data
	Zenith_UIElement::ReadFromDataStream(xStream);

	// Read button-specific data
	uint32_t uVersion;
	xStream >> uVersion;

	xStream >> m_strText;
	xStream >> m_fFontSize;

	// Text color
	xStream >> m_xTextColor.x;
	xStream >> m_xTextColor.y;
	xStream >> m_xTextColor.z;
	xStream >> m_xTextColor.w;

	// State colors
	xStream >> m_xNormalColor.x;
	xStream >> m_xNormalColor.y;
	xStream >> m_xNormalColor.z;
	xStream >> m_xNormalColor.w;

	xStream >> m_xHoverColor.x;
	xStream >> m_xHoverColor.y;
	xStream >> m_xHoverColor.z;
	xStream >> m_xHoverColor.w;

	xStream >> m_xPressedColor.x;
	xStream >> m_xPressedColor.y;
	xStream >> m_xPressedColor.z;
	xStream >> m_xPressedColor.w;

	// Border
	xStream >> m_fBorderThickness;
	xStream >> m_xBorderColor.x;
	xStream >> m_xBorderColor.y;
	xStream >> m_xBorderColor.z;
	xStream >> m_xBorderColor.w;
}

#ifdef ZENITH_TOOLS
void Zenith_UIButton::RenderPropertiesPanel()
{
	// Render base properties
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIButtonProps");

	ImGui::Separator();
	ImGui::Text("Button Properties");

	// Text content
	char szTextBuffer[256];
	strncpy_s(szTextBuffer, m_strText.c_str(), sizeof(szTextBuffer) - 1);
	if (ImGui::InputText("Button Text", szTextBuffer, sizeof(szTextBuffer)))
	{
		m_strText = szTextBuffer;
	}

	ImGui::DragFloat("Font Size", &m_fFontSize, 1.0f, 8.0f, 200.0f);

	// Text color
	float fTextColor[4] = { m_xTextColor.x, m_xTextColor.y, m_xTextColor.z, m_xTextColor.w };
	if (ImGui::ColorEdit4("Text Color", fTextColor))
	{
		m_xTextColor = { fTextColor[0], fTextColor[1], fTextColor[2], fTextColor[3] };
	}

	ImGui::Separator();
	ImGui::Text("State Colors");

	float fNormalColor[4] = { m_xNormalColor.x, m_xNormalColor.y, m_xNormalColor.z, m_xNormalColor.w };
	if (ImGui::ColorEdit4("Normal", fNormalColor))
	{
		m_xNormalColor = { fNormalColor[0], fNormalColor[1], fNormalColor[2], fNormalColor[3] };
	}

	float fHoverColor[4] = { m_xHoverColor.x, m_xHoverColor.y, m_xHoverColor.z, m_xHoverColor.w };
	if (ImGui::ColorEdit4("Hover", fHoverColor))
	{
		m_xHoverColor = { fHoverColor[0], fHoverColor[1], fHoverColor[2], fHoverColor[3] };
	}

	float fPressedColor[4] = { m_xPressedColor.x, m_xPressedColor.y, m_xPressedColor.z, m_xPressedColor.w };
	if (ImGui::ColorEdit4("Pressed", fPressedColor))
	{
		m_xPressedColor = { fPressedColor[0], fPressedColor[1], fPressedColor[2], fPressedColor[3] };
	}

	ImGui::Separator();
	ImGui::Text("Border");

	ImGui::DragFloat("Border Thickness", &m_fBorderThickness, 0.5f, 0.0f, 50.0f);

	float fBorderColor[4] = { m_xBorderColor.x, m_xBorderColor.y, m_xBorderColor.z, m_xBorderColor.w };
	if (ImGui::ColorEdit4("Border Color", fBorderColor))
	{
		m_xBorderColor = { fBorderColor[0], fBorderColor[1], fBorderColor[2], fBorderColor[3] };
	}

	// State display (read-only)
	const char* szStates[] = { "Normal", "Hovered", "Pressed" };
	ImGui::Text("Current State: %s", szStates[static_cast<int>(m_eState)]);
	ImGui::Text("Focused: %s", m_bFocused ? "Yes" : "No");

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
