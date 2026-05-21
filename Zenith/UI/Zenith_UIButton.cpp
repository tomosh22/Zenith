#include "Zenith.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Input/Zenith_Input.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif
#include "AssetHandling/Zenith_TextureAsset.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_BUTTON_VERSION = 3;

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

	// Buttons are focusable by default
	m_bFocusable = true;

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

void Zenith_UIButton::SetIconTexturePath(const std::string& strPath)
{
	m_xIconTexture.SetPath(strPath);
	if (m_xIconTexture.IsSet())
	{
		Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xIconTexture.GetPath()); // trigger load
	}
}

void Zenith_UIButton::Update(float fDt)
{
	if (!m_bVisible)
	{
		m_eState = ButtonState::NORMAL;
		m_bWasInvisible = true;
		return;
	}

	HandleFirstVisibleFrame();

#ifdef ZENITH_TOOLS
	HandleEditorStoppedState();
#endif

	bool bInteractable = IsGroupInteractable();

	float fMouseX = 0.0f;
	float fMouseY = 0.0f;
	GetTransformedMousePosition(fMouseX, fMouseY);

	Zenith_Maths::Vector4 xBounds = GetScreenBounds();
	bool bHovered = bInteractable
		&& fMouseX >= xBounds.x
		&& fMouseX <= xBounds.z
		&& fMouseY >= xBounds.y
		&& fMouseY <= xBounds.w;

	bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);

	HandleInputEvents(bInteractable, bHovered, bMouseDown);
	ResolveState(bHovered, bMouseDown);
	UpdateVisualTransition(fDt);

	Zenith_UIElement::Update(fDt);
}

void Zenith_UIButton::HandleFirstVisibleFrame()
{
	// Snap current style to target on first visible frame to avoid stale-style transitions
	if (m_bWasInvisible)
	{
		m_xCurrentStyle = m_xNormalStyle;
		m_bWasInvisible = false;
		// Sync mouse state to prevent false press/release detection on the first visible frame
		m_bMousePressedInside = false;
		m_bMouseDownLastFrame = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	}
}

#ifdef ZENITH_TOOLS
void Zenith_UIButton::HandleEditorStoppedState()
{
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
}
#endif


void Zenith_UIButton::HandleInputEvents(bool bInteractable, bool bHovered, bool bMouseDown)
{
	if (!bInteractable)
	{
		return;
	}

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

void Zenith_UIButton::ResolveState(bool bHovered, bool bMouseDown)
{
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
}

void Zenith_UIButton::UpdateVisualTransition(float fDt)
{
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
}

void Zenith_UIButton::ResolveVisualState(float& fAlpha, UIStyle& xRenderStyle) const
{
	fAlpha = GetEffectiveAlpha();
	xRenderStyle = m_xCurrentStyle;
	if (m_bFocused)
	{
		xRenderStyle.m_xBorderColor = {1.0f, 1.0f, 1.0f, 1.0f};
	}
}

Zenith_UIButton::ButtonIconLayout Zenith_UIButton::CalculateIconTextPositions(const Zenith_Maths::Vector4& xBounds) const
{
	ButtonIconLayout xLayout;

	if (m_xIconTexture.IsSet() && m_xIconSize.x > 0.f && m_xIconSize.y > 0.f)
	{
		Zenith_TextureAsset* pxIconTex = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xIconTexture.GetPath());
		if (pxIconTex && pxIconTex->IsValid() && pxIconTex->m_xSRV.m_xImageViewHandle.IsValid())
		{
			xLayout.bHasIcon = true;
		}
	}

	const float fBoundsW = xBounds.z - xBounds.x;
	const float fBoundsH = xBounds.w - xBounds.y;
	const float fCharWidth = m_fFontSize * fCHAR_SPACING;
	const float fTextWidth = m_strText.empty() ? 0.f : static_cast<float>(m_strText.length()) * fCharWidth;
	const float fTextHeight = m_fFontSize;

	if (xLayout.bHasIcon && m_eIconPlacement == IconPlacement::ICON_ONLY)
	{
		xLayout.xIconPos = {
			xBounds.x + (fBoundsW - m_xIconSize.x) * 0.5f,
			xBounds.y + (fBoundsH - m_xIconSize.y) * 0.5f
		};
		return xLayout;
	}

	if (xLayout.bHasIcon && !m_strText.empty())
	{
		if (m_eIconPlacement == IconPlacement::LEFT || m_eIconPlacement == IconPlacement::RIGHT)
		{
			const float fTotalW = m_xIconSize.x + m_fIconPadding + fTextWidth;
			const float fStartX = xBounds.x + (fBoundsW - fTotalW) * 0.5f;
			const float fCenterY = xBounds.y + fBoundsH * 0.5f;

			if (m_eIconPlacement == IconPlacement::LEFT)
			{
				xLayout.xIconPos = {fStartX, fCenterY - m_xIconSize.y * 0.5f};
				xLayout.xTextPos = {fStartX + m_xIconSize.x + m_fIconPadding, fCenterY - fTextHeight * 0.5f};
			}
			else
			{
				xLayout.xTextPos = {fStartX, fCenterY - fTextHeight * 0.5f};
				xLayout.xIconPos = {fStartX + fTextWidth + m_fIconPadding, fCenterY - m_xIconSize.y * 0.5f};
			}
		}
		else // TOP or BOTTOM
		{
			const float fTotalH = m_xIconSize.y + m_fIconPadding + fTextHeight;
			const float fStartY = xBounds.y + (fBoundsH - fTotalH) * 0.5f;
			const float fCenterX = xBounds.x + fBoundsW * 0.5f;

			if (m_eIconPlacement == IconPlacement::TOP)
			{
				xLayout.xIconPos = {fCenterX - m_xIconSize.x * 0.5f, fStartY};
				xLayout.xTextPos = {fCenterX - fTextWidth * 0.5f, fStartY + m_xIconSize.y + m_fIconPadding};
			}
			else
			{
				xLayout.xTextPos = {fCenterX - fTextWidth * 0.5f, fStartY};
				xLayout.xIconPos = {fCenterX - m_xIconSize.x * 0.5f, fStartY + fTextHeight + m_fIconPadding};
			}
		}
		return xLayout;
	}

	if (!m_strText.empty())
	{
		xLayout.xTextPos = {
			xBounds.x + (fBoundsW - fTextWidth) * 0.5f,
			xBounds.y + (fBoundsH - fTextHeight) * 0.5f
		};
	}
	return xLayout;
}

void Zenith_UIButton::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	float fAlpha;
	UIStyle xRenderStyle;
	ResolveVisualState(fAlpha, xRenderStyle);

	const Zenith_Maths::Vector4 xBounds = GetScreenBounds();
	UIStyleRenderer::RenderStyledRect(xCanvas, xRenderStyle, xBounds, fAlpha);

	const ButtonIconLayout xLayout = CalculateIconTextPositions(xBounds);

	if (xLayout.bHasIcon)
	{
		Zenith_TextureAsset* pxIconTex = !m_xIconTexture.GetPath().empty()
			? Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xIconTexture.GetPath())
			: m_xIconTexture.GetDirect();
		const uint32_t uIconTextureID = (pxIconTex && pxIconTex->IsValid())
			? pxIconTex->m_xSRV.m_xImageViewHandle.AsUInt()
			: 0;
		const Zenith_Maths::Vector4 xIconBounds = {
			xLayout.xIconPos.x, xLayout.xIconPos.y,
			xLayout.xIconPos.x + m_xIconSize.x, xLayout.xIconPos.y + m_xIconSize.y
		};
		const Zenith_Maths::Vector4 xIconColor = {1.f, 1.f, 1.f, fAlpha};
		xCanvas.SubmitQuadWithUV(xIconBounds, xIconColor, uIconTextureID, {0.f, 0.f}, {1.f, 1.f});
	}

	if (!m_strText.empty() && m_eIconPlacement != IconPlacement::ICON_ONLY)
	{
		if (m_bTextShadowEnabled)
		{
			const Zenith_Maths::Vector2 xShadowPos = {
				xLayout.xTextPos.x + m_xTextShadowOffset.x,
				xLayout.xTextPos.y + m_xTextShadowOffset.y
			};
			Zenith_Maths::Vector4 xShadowColor = m_xTextShadowColor;
			xShadowColor.a *= fAlpha;
			xCanvas.SubmitText(m_strText, xShadowPos, m_fFontSize, xShadowColor);
		}

		Zenith_Maths::Vector4 xTextColor = m_xTextColor;
		xTextColor.a *= fAlpha;
		xCanvas.SubmitText(m_strText, xLayout.xTextPos, m_fFontSize, xTextColor);
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

	// v3: icon data
	std::string strIconPath = m_xIconTexture.GetPath();
	xStream << strIconPath;
	xStream << m_xIconSize.x; xStream << m_xIconSize.y;
	xStream << static_cast<uint32_t>(m_eIconPlacement);
	xStream << m_fIconPadding;
}

void Zenith_UIButton::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion <= UI_BUTTON_VERSION, "UIButton version mismatch");

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

	if (uVersion >= 3)
	{
		std::string strIconPath;
		xStream >> strIconPath;
		if (!strIconPath.empty())
		{
			SetIconTexturePath(strIconPath);
		}
		xStream >> m_xIconSize.x; xStream >> m_xIconSize.y;
		uint32_t uPlacement;
		xStream >> uPlacement;
		m_eIconPlacement = static_cast<IconPlacement>(uPlacement);
		xStream >> m_fIconPadding;
	}

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

	ImGui::Separator();
	ImGui::Text("Icon");

	char szIconPathBuffer[512];
	const std::string& strIconPath = m_xIconTexture.GetPath();
	strncpy_s(szIconPathBuffer, strIconPath.c_str(), sizeof(szIconPathBuffer) - 1);
	if (ImGui::InputText("Icon Path", szIconPathBuffer, sizeof(szIconPathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		SetIconTexturePath(szIconPathBuffer);
	}

	float fIconSz[2] = {m_xIconSize.x, m_xIconSize.y};
	if (ImGui::DragFloat2("Icon Size", fIconSz, 1.f, 0.f, 256.f))
	{
		m_xIconSize = {fIconSz[0], fIconSz[1]};
	}

	const char* szPlacements[] = {"Left", "Right", "Top", "Bottom", "Icon Only"};
	int iPlacement = static_cast<int>(m_eIconPlacement);
	if (ImGui::Combo("Icon Placement", &iPlacement, szPlacements, 5))
	{
		m_eIconPlacement = static_cast<IconPlacement>(iPlacement);
	}

	ImGui::DragFloat("Icon Padding", &m_fIconPadding, 0.5f, 0.f, 50.f);

	ImGui::Separator();
	const char* szStates[] = { "Normal", "Hovered", "Pressed" };
	ImGui::Text("Current State: %s", szStates[static_cast<int>(m_eState)]);
	ImGui::Text("Focused: %s", m_bFocused ? "Yes" : "No");

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
