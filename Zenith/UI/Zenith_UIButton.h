#pragma once

#include "UI/Zenith_UIElement.h"
#include "Maths/Zenith_Maths.h"
#include <string>

/**
 * Zenith_UIButton - Clickable/tappable button widget
 *
 * Renders a colored rectangle background with centered text label.
 * Supports mouse click (desktop) and touch tap (mobile).
 *
 * Features:
 *   - Hit testing via GetScreenBounds() + mouse/touch position
 *   - Three visual states: Normal, Hovered, Pressed
 *   - Function pointer callback on click (NO std::function)
 *   - Keyboard focus for accessibility (SetFocused + Enter to activate)
 *   - Configurable colors per state, border, and text
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

	// Function pointer callback (NOT std::function)
	// pxUserData is passed as the argument when the button is clicked
	void SetOnClick(UIButtonCallback pfnCallback, void* pxUserData = nullptr)
	{
		m_pfnOnClick = pfnCallback;
		m_pxUserData = pxUserData;
	}

	// ========== Per-State Colors ==========

	void SetNormalColor(const Zenith_Maths::Vector4& xColor) { m_xNormalColor = xColor; }
	Zenith_Maths::Vector4 GetNormalColor() const { return m_xNormalColor; }

	void SetHoverColor(const Zenith_Maths::Vector4& xColor) { m_xHoverColor = xColor; }
	Zenith_Maths::Vector4 GetHoverColor() const { return m_xHoverColor; }

	void SetPressedColor(const Zenith_Maths::Vector4& xColor) { m_xPressedColor = xColor; }
	Zenith_Maths::Vector4 GetPressedColor() const { return m_xPressedColor; }

	// ========== Text Color ==========

	void SetTextColor(const Zenith_Maths::Vector4& xColor) { m_xTextColor = xColor; }
	Zenith_Maths::Vector4 GetTextColor() const { return m_xTextColor; }

	// ========== Border ==========

	void SetBorderThickness(float fThickness) { m_fBorderThickness = fThickness; }
	float GetBorderThickness() const { return m_fBorderThickness; }

	void SetBorderColor(const Zenith_Maths::Vector4& xColor) { m_xBorderColor = xColor; }
	Zenith_Maths::Vector4 GetBorderColor() const { return m_xBorderColor; }

	// ========== State ==========

	ButtonState GetState() const { return m_eState; }

	// ========== Keyboard Focus ==========

	// When focused, the button shows hover color and responds to Enter/Space
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

	// Mouse tracking for click-on-release
	bool m_bMousePressedInside = false;
	bool m_bMouseDownLastFrame = false;

	// Text
	std::string m_strText;
	float m_fFontSize = 24.0f;
	Zenith_Maths::Vector4 m_xTextColor = {1.0f, 1.0f, 1.0f, 1.0f};

	// Background colors per state
	Zenith_Maths::Vector4 m_xNormalColor  = {0.25f, 0.25f, 0.30f, 1.0f};
	Zenith_Maths::Vector4 m_xHoverColor   = {0.35f, 0.35f, 0.45f, 1.0f};
	Zenith_Maths::Vector4 m_xPressedColor = {0.15f, 0.15f, 0.20f, 1.0f};

	// Border
	float m_fBorderThickness = 2.0f;
	Zenith_Maths::Vector4 m_xBorderColor = {0.5f, 0.5f, 0.6f, 1.0f};
};

} // namespace Zenith_UI
