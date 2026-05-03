#include "Zenith.h"
#include "UI/Zenith_UIToggle.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Flux/Text/Flux_Text.h"
#include "Input/Zenith_Input.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_TOGGLE_VERSION = 1;

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

Zenith_UIToggle::Zenith_UIToggle(const std::string& strText, const std::string& strName)
	: Zenith_UIElement(strName)
	, m_strText(strText)
{
	m_xSize = { 200.0f, 50.0f };

	// Toggles are focusable by default
	m_bFocusable = true;

	// Default on/off styles
	m_xOnStyle.m_xFillColor = {0.2f, 0.6f, 0.3f, 1.0f};
	m_xOnStyle.m_fBorderThickness = 2.0f;
	m_xOnStyle.m_xBorderColor = {0.3f, 0.7f, 0.4f, 1.0f};

	m_xOffStyle.m_xFillColor = {0.35f, 0.35f, 0.40f, 1.0f};
	m_xOffStyle.m_fBorderThickness = 2.0f;
	m_xOffStyle.m_xBorderColor = {0.5f, 0.5f, 0.6f, 1.0f};

	m_xCurrentStyle = m_xOffStyle;
}

void Zenith_UIToggle::SetIsOn(bool bOn)
{
	bool bChanged = (m_bIsOn != bOn);
	m_bIsOn = bOn;
	if (bChanged && m_pfnOnValueChanged)
	{
		m_pfnOnValueChanged(m_bIsOn, m_pxUserData);
	}
}

void Zenith_UIToggle::ResetInteractionStateForEditor()
{
#ifdef ZENITH_TOOLS
	if (Zenith_Editor::GetEditorMode() == EditorMode::Stopped
#ifdef ZENITH_INPUT_SIMULATOR
		&& !Zenith_InputSimulator::IsEnabled()
#endif
	)
	{
		m_bFocused = false;
		m_bMousePressedInside = false;
		m_bMouseDownLastFrame = false;
	}
#endif
}


bool Zenith_UIToggle::ComputeHovered(bool bInteractable, float fMouseX, float fMouseY) const
{
	Zenith_Maths::Vector4 xBounds = GetScreenBounds();
	return bInteractable
		&& fMouseX >= xBounds.x
		&& fMouseX <= xBounds.z
		&& fMouseY >= xBounds.y
		&& fMouseY <= xBounds.w;
}

void Zenith_UIToggle::FireValueChangedCallback()
{
	if (m_pfnOnValueChanged)
	{
		m_pfnOnValueChanged(m_bIsOn, m_pxUserData);
	}
}

void Zenith_UIToggle::HandleMouseInteraction(bool bHovered, bool bMouseDown)
{
	if (bMouseDown && !m_bMouseDownLastFrame && bHovered)
	{
		m_bMousePressedInside = true;
	}
	if (!bMouseDown)
	{
		if (m_bMouseDownLastFrame && m_bMousePressedInside && bHovered)
		{
			// Toggle state
			m_bIsOn = !m_bIsOn;
			FireValueChangedCallback();
		}
		m_bMousePressedInside = false;
	}
	m_bMouseDownLastFrame = bMouseDown;
}

void Zenith_UIToggle::HandleKeyboardActivation()
{
	bool bActivated = m_bFocused
		&& (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ENTER)
			|| Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE));
	if (bActivated)
	{
		m_bIsOn = !m_bIsOn;
		FireValueChangedCallback();
	}
}

void Zenith_UIToggle::UpdateVisualFromState(float fDt)
{
	// Lerp toward target style based on on/off state
	const UIStyle& xTargetStyle = m_bIsOn ? m_xOnStyle : m_xOffStyle;

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
}

void Zenith_UIToggle::Update(float fDt)
{
	if (!m_bVisible)
		return;

	ResetInteractionStateForEditor();

	bool bInteractable = IsGroupInteractable();

	float fMouseX;
	float fMouseY;
	GetTransformedMousePosition(fMouseX, fMouseY);

	bool bHovered = ComputeHovered(bInteractable, fMouseX, fMouseY);
	bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);

	if (bInteractable)
	{
		HandleMouseInteraction(bHovered, bMouseDown);
		HandleKeyboardActivation();
	}

	UpdateVisualFromState(fDt);

	Zenith_UIElement::Update(fDt);
}

void Zenith_UIToggle::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	float fAlpha = GetEffectiveAlpha();
	Zenith_Maths::Vector4 xBounds = GetScreenBounds();

	UIStyle xRenderStyle = m_xCurrentStyle;

	// Focus border
	if (m_bFocused)
	{
		xRenderStyle.m_xBorderColor = {0.3f, 0.6f, 1.0f, 1.0f};
		xRenderStyle.m_fBorderThickness = 3.0f;
	}

	UIStyleRenderer::RenderStyledRect(xCanvas, xRenderStyle, xBounds, fAlpha);

	// Render text centered
	if (!m_strText.empty())
	{
		float fCharWidth = m_fFontSize * fCHAR_SPACING;
		float fTextWidth = static_cast<float>(m_strText.length()) * fCharWidth;
		float fBoundsW = xBounds.z - xBounds.x;
		float fBoundsH = xBounds.w - xBounds.y;

		Zenith_Maths::Vector2 xTextPos = {
			xBounds.x + (fBoundsW - fTextWidth) * 0.5f,
			xBounds.y + (fBoundsH - m_fFontSize) * 0.5f
		};

		Zenith_Maths::Vector4 xCol = m_xTextColor;
		xCol.w *= fAlpha;
		xCanvas.SubmitText(m_strText, xTextPos, m_fFontSize, xCol);
	}

	Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIToggle::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_TOGGLE_VERSION;
	xStream << m_strText;
	xStream << m_fFontSize;
	xStream << m_bIsOn;
	WriteStyleToStream(xStream, m_xOnStyle);
	WriteStyleToStream(xStream, m_xOffStyle);
}

void Zenith_UIToggle::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion == UI_TOGGLE_VERSION, "UIToggle version mismatch");

	xStream >> m_strText;
	xStream >> m_fFontSize;
	xStream >> m_bIsOn;
	ReadStyleFromStream(xStream, m_xOnStyle);
	ReadStyleFromStream(xStream, m_xOffStyle);

	m_xCurrentStyle = m_bIsOn ? m_xOnStyle : m_xOffStyle;
}

#ifdef ZENITH_TOOLS
void Zenith_UIToggle::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIToggleProps");

	ImGui::Separator();
	ImGui::Text("Toggle Properties");

	char szTextBuffer[256];
	strncpy_s(szTextBuffer, m_strText.c_str(), sizeof(szTextBuffer) - 1);
	if (ImGui::InputText("Text", szTextBuffer, sizeof(szTextBuffer)))
	{
		m_strText = szTextBuffer;
	}

	ImGui::DragFloat("Font Size", &m_fFontSize, 1.0f, 8.0f, 200.0f);
	ImGui::Checkbox("Is On", &m_bIsOn);

	float fOnColor[4] = { m_xOnStyle.m_xFillColor.x, m_xOnStyle.m_xFillColor.y, m_xOnStyle.m_xFillColor.z, m_xOnStyle.m_xFillColor.w };
	if (ImGui::ColorEdit4("On Color", fOnColor))
	{
		m_xOnStyle.m_xFillColor = { fOnColor[0], fOnColor[1], fOnColor[2], fOnColor[3] };
	}

	float fOffColor[4] = { m_xOffStyle.m_xFillColor.x, m_xOffStyle.m_xFillColor.y, m_xOffStyle.m_xFillColor.z, m_xOffStyle.m_xFillColor.w };
	if (ImGui::ColorEdit4("Off Color", fOffColor))
	{
		m_xOffStyle.m_xFillColor = { fOffColor[0], fOffColor[1], fOffColor[2], fOffColor[3] };
	}

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
