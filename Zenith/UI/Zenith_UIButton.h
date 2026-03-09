#pragma once

#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIStyle.h"
#include "Maths/Zenith_Maths.h"
#include <string>

/**
 * Zenith_UIButton - Clickable/tappable button widget
 *
 * Renders a styled rectangle background with centered text label.
 * Supports mouse click (desktop) and touch tap (mobile).
 *
 * Features:
 *   - Hit testing via GetScreenBounds() + mouse/touch position
 *   - Three visual states: Normal, Hovered, Pressed (each with its own UIStyle)
 *   - Smooth transitions between states via UIStyle::Lerp
 *   - Function pointer callback on click (NO std::function)
 *   - Keyboard focus for accessibility (SetFocused + Enter to activate)
 *   - Text shadow support
 *   - Group interactable check
 */

namespace Zenith_UI {

typedef void(*UIButtonCallback)(void* pxUserData);

class Zenith_UIButton : public Zenith_UIElement
{
public:
	enum class ButtonState : uint32_t { NORMAL, HOVERED, PRESSED };

	Zenith_UIButton(const std::string& strText = "", const std::string& strName = "UIButton");
	virtual ~Zenith_UIButton() = default;

	virtual UIElementType GetType() const override { return UIElementType::Button; }

	// ========== Text ==========

	void SetText(const std::string& strText) { m_strText = strText; }
	const std::string& GetText() const { return m_strText; }

	void SetFontSize(float fSize) { m_fFontSize = fSize; }
	float GetFontSize() const { return m_fFontSize; }

	// ========== Callback ==========

	void SetOnClick(UIButtonCallback pfnCallback, void* pxUserData = nullptr)
	{
		m_pfnOnClick = pfnCallback;
		m_pxUserData = pxUserData;
	}

	// ========== Per-State Styles ==========

	void SetNormalStyle(const UIStyle& xStyle) { m_xNormalStyle = xStyle; }
	const UIStyle& GetNormalStyle() const { return m_xNormalStyle; }

	void SetHoveredStyle(const UIStyle& xStyle) { m_xHoveredStyle = xStyle; }
	const UIStyle& GetHoveredStyle() const { return m_xHoveredStyle; }

	void SetPressedStyle(const UIStyle& xStyle) { m_xPressedStyle = xStyle; }
	const UIStyle& GetPressedStyle() const { return m_xPressedStyle; }

	// Convenience setters/getters that access fill color on the appropriate state
	void SetNormalColor(const Zenith_Maths::Vector4& xColor) { m_xNormalStyle.m_xFillColor = xColor; m_xCurrentStyle.m_xFillColor = xColor; }
	Zenith_Maths::Vector4 GetNormalColor() const { return m_xNormalStyle.m_xFillColor; }
	void SetHoverColor(const Zenith_Maths::Vector4& xColor) { m_xHoveredStyle.m_xFillColor = xColor; }
	Zenith_Maths::Vector4 GetHoverColor() const { return m_xHoveredStyle.m_xFillColor; }
	void SetPressedColor(const Zenith_Maths::Vector4& xColor) { m_xPressedStyle.m_xFillColor = xColor; }
	Zenith_Maths::Vector4 GetPressedColor() const { return m_xPressedStyle.m_xFillColor; }

	// Convenience setters that apply to all states
	void SetCornerRadius(float fRadius) { m_xNormalStyle.m_fCornerRadius = fRadius; m_xHoveredStyle.m_fCornerRadius = fRadius; m_xPressedStyle.m_fCornerRadius = fRadius; }
	void SetBorderColor(const Zenith_Maths::Vector4& xColor) { m_xNormalStyle.m_xBorderColor = xColor; m_xHoveredStyle.m_xBorderColor = xColor; m_xPressedStyle.m_xBorderColor = xColor; }
	void SetBorderThickness(float fThickness) { m_xNormalStyle.m_fBorderThickness = fThickness; m_xHoveredStyle.m_fBorderThickness = fThickness; m_xPressedStyle.m_fBorderThickness = fThickness; }
	void SetShadowEnabled(bool bEnabled) { m_xNormalStyle.m_bShadowEnabled = bEnabled; m_xHoveredStyle.m_bShadowEnabled = bEnabled; m_xPressedStyle.m_bShadowEnabled = bEnabled; }
	void SetShadowOffset(const Zenith_Maths::Vector2& xOffset) { m_xNormalStyle.m_xShadowOffset = xOffset; m_xHoveredStyle.m_xShadowOffset = xOffset; m_xPressedStyle.m_xShadowOffset = xOffset; }
	void SetShadowSpread(float fSpread) { m_xNormalStyle.m_fShadowSpread = fSpread; m_xHoveredStyle.m_fShadowSpread = fSpread; m_xPressedStyle.m_fShadowSpread = fSpread; }
	void SetShadowColor(const Zenith_Maths::Vector4& xColor) { m_xNormalStyle.m_xShadowColor = xColor; m_xHoveredStyle.m_xShadowColor = xColor; m_xPressedStyle.m_xShadowColor = xColor; }
	void SetGradientColor(const Zenith_Maths::Vector4& xColor) { m_xNormalStyle.m_xGradientBottomColor = xColor; }
	void SetTransitionDuration(float fDuration) { m_fTransitionDuration = fDuration; }

	// ========== Text Color ==========

	void SetTextColor(const Zenith_Maths::Vector4& xColor) { m_xTextColor = xColor; }
	Zenith_Maths::Vector4 GetTextColor() const { return m_xTextColor; }

	// ========== Text Shadow ==========

	void SetTextShadowEnabled(bool bEnabled) { m_bTextShadowEnabled = bEnabled; }
	void SetTextShadowColor(const Zenith_Maths::Vector4& xColor) { m_xTextShadowColor = xColor; }
	void SetTextShadowOffset(const Zenith_Maths::Vector2& xOffset) { m_xTextShadowOffset = xOffset; }

	// ========== State ==========

	ButtonState GetState() const { return m_eState; }

	// ========== Keyboard Focus ==========

	void SetFocused(bool bFocused) { m_bFocused = bFocused; }
	bool IsFocused() const { return m_bFocused; }

	// ========== Overrides ==========

	virtual void Update(float fDt) override;
	virtual void Render(Zenith_UICanvas& xCanvas) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	virtual void RenderPropertiesPanel() override;
#endif

private:
	// Callback
	UIButtonCallback m_pfnOnClick = nullptr;
	void* m_pxUserData = nullptr;

	// State
	ButtonState m_eState = ButtonState::NORMAL;
	bool m_bFocused = false;
	bool m_bWasInvisible = true;

	// Mouse tracking for click-on-release
	bool m_bMousePressedInside = false;
	bool m_bMouseDownLastFrame = false;

	// Text
	std::string m_strText;
	float m_fFontSize = 24.0f;
	Zenith_Maths::Vector4 m_xTextColor = {1.0f, 1.0f, 1.0f, 1.0f};

	// Text shadow
	bool m_bTextShadowEnabled = false;
	Zenith_Maths::Vector4 m_xTextShadowColor = {0.0f, 0.0f, 0.0f, 0.5f};
	Zenith_Maths::Vector2 m_xTextShadowOffset = {2.0f, 2.0f};

	// Per-state styles
	UIStyle m_xNormalStyle;
	UIStyle m_xHoveredStyle;
	UIStyle m_xPressedStyle;
	UIStyle m_xCurrentStyle; // runtime interpolated

	// Transition
	float m_fTransitionDuration = 0.1f;
	float m_fTransitionT = 0.0f;
};

} // namespace Zenith_UI
