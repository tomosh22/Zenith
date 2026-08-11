#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "UI/Zenith_UIToggle.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "AssetHandling/Zenith_FontAsset.h"
#include "Input/Zenith_Input.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

#ifdef ZENITH_TOOLS
#include "Core/Zenith_EditorQuery.h"
#include "imgui.h"
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

bool Zenith_UIToggle::IsInputSuppressedByEditor() const
{
#ifdef ZENITH_TOOLS
	// Stopped editor with no simulator driving: the canvas is being AUTHORED,
	// so clicks belong to the editor, not to the widget.
	return g_xEditorQuery.m_pfnIsEditorStopped()
#ifdef ZENITH_INPUT_SIMULATOR
		&& !Zenith_InputSimulator::IsEnabled()
#endif
		;
#else
	return false;
#endif
}

void Zenith_UIToggle::AbandonCapture()
{
	ReleaseCapturedPointer();
	m_bPointerInside = false;
}


void Zenith_UIToggle::FireValueChangedCallback()
{
	if (m_pfnOnValueChanged)
	{
		m_pfnOnValueChanged(m_bIsOn, m_pxUserData);
	}
}

void Zenith_UIToggle::UpdatePointerInput(Zenith_Pointers& xPointers, float fDt)
{
	(void)fDt;   // a toggle's capture is edge-driven; only a drag needs a rate

	const bool bEditorSuppressed = IsInputSuppressedByEditor();
	if (bEditorSuppressed)
	{
		m_bFocused = false;
	}
	if (!m_bVisible || !IsGroupInteractable() || bEditorSuppressed)
	{
		AbandonCapture();
		return;
	}

	// Already holding a pointer: click-on-release, drag-off keeps the claim.
	const Zenith_Pointer* pxCaptured = ResolveCapturedPointer(xPointers);
	if (pxCaptured != nullptr)
	{
		m_bPointerInside = ContainsSurfacePosition(pxCaptured->m_xPosition);

		if (pxCaptured->m_bCancelledThisFrame)
		{
			AbandonCapture();
		}
		else if (pxCaptured->m_bUpThisFrame)
		{
			const bool bInside = m_bPointerInside;
			AbandonCapture();
			if (bInside)
			{
				m_bIsOn = !m_bIsOn;
				FireValueChangedCallback();
			}
		}
		return;
	}

	m_bPointerInside = false;

	for (u_int32 u = 0; u < Zenith_Pointers::uMAX_POINTERS; u++)
	{
		const Zenith_Pointer& xPointer = xPointers.GetPointer(u);
		if (!xPointer.m_bDownThisFrame || xPointer.IsClaimed())
		{
			continue;
		}
		if (!ContainsSurfacePosition(xPointer.m_xPosition))
		{
			continue;
		}
		if (!CapturePointer(xPointers, xPointers.GetHandle(u)))
		{
			continue;
		}

		m_bPointerInside = true;

		// Press and release inside ONE frame (a quick tap, a simulated click):
		// both edges ride the pointer we just claimed, so complete it here.
		if (xPointer.m_bUpThisFrame || xPointer.m_bCancelledThisFrame)
		{
			const bool bToggled = xPointer.m_bUpThisFrame && ContainsSurfacePosition(xPointer.m_xPosition);
			AbandonCapture();
			if (bToggled)
			{
				m_bIsOn = !m_bIsOn;
				FireValueChangedCallback();
			}
		}
		break;
	}
}

void Zenith_UIToggle::HandleKeyboardActivation()
{
	Zenith_Input& xInput = g_xEngine.Input();
	bool bActivated = m_bFocused
		&& (xInput.WasKeyPressedThisFrame(ZENITH_KEY_ENTER)
			|| xInput.WasKeyPressedThisFrame(ZENITH_KEY_SPACE));
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
	// VISUAL ONLY — the capture / toggle decision moved to UpdatePointerInput,
	// which runs on every frame rather than only the ones that render.
	if (!m_bVisible)
		return;

	if (IsGroupInteractable())
	{
		// Keyboard/gamepad self-activation is untouched by the pointer migration.
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
		float fCharWidth = m_fFontSize * Zenith_FontAsset::GetActiveOrDefaultMetrics().fEmAdvance;
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
