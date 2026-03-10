#pragma once

#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIStyle.h"
#include "Maths/Zenith_Maths.h"
#include <string>

/**
 * Zenith_UIToggle - Toggle (on/off) widget for UI
 *
 * Renders a styled rectangle background with centered text label.
 * Supports two visual states (on/off), each with hover/pressed substates.
 * On click, toggles the boolean state and fires a callback.
 */

namespace Zenith_UI {

typedef void(*UIToggleCallback)(bool bNewValue, void* pxUserData);

class Zenith_UIToggle : public Zenith_UIElement
{
public:
	Zenith_UIToggle(const std::string& strText = "", const std::string& strName = "UIToggle");
	virtual ~Zenith_UIToggle() = default;

	virtual UIElementType GetType() const override { return UIElementType::Toggle; }

	// ========== Toggle State ==========

	void SetIsOn(bool bOn);
	bool IsOn() const { return m_bIsOn; }

	// ========== Callback ==========

	void SetOnValueChanged(UIToggleCallback pfnCallback, void* pxUserData = nullptr)
	{
		m_pfnOnValueChanged = pfnCallback;
		m_pxUserData = pxUserData;
	}

	// ========== Text ==========

	void SetText(const std::string& strText) { m_strText = strText; }
	const std::string& GetText() const { return m_strText; }

	void SetFontSize(float fSize) { m_fFontSize = fSize; }
	float GetFontSize() const { return m_fFontSize; }

	// ========== Styles ==========

	void SetOnStyle(const UIStyle& xStyle) { m_xOnStyle = xStyle; }
	const UIStyle& GetOnStyle() const { return m_xOnStyle; }

	void SetOffStyle(const UIStyle& xStyle) { m_xOffStyle = xStyle; }
	const UIStyle& GetOffStyle() const { return m_xOffStyle; }

	void SetOnColor(const Zenith_Maths::Vector4& xColor) { m_xOnStyle.m_xFillColor = xColor; }
	void SetOffColor(const Zenith_Maths::Vector4& xColor) { m_xOffStyle.m_xFillColor = xColor; }

	void SetCornerRadius(float fRadius) { m_xOnStyle.m_fCornerRadius = fRadius; m_xOffStyle.m_fCornerRadius = fRadius; }
	void SetTransitionDuration(float fDuration) { m_fTransitionDuration = fDuration; }

	// ========== Text Color ==========

	void SetTextColor(const Zenith_Maths::Vector4& xColor) { m_xTextColor = xColor; }
	Zenith_Maths::Vector4 GetTextColor() const { return m_xTextColor; }

	// ========== Focus ==========

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
	UIToggleCallback m_pfnOnValueChanged = nullptr;
	void* m_pxUserData = nullptr;

	// State
	bool m_bIsOn = false;
	bool m_bFocused = false;

	// Mouse tracking for click-on-release
	bool m_bMousePressedInside = false;
	bool m_bMouseDownLastFrame = false;

	// Text
	std::string m_strText;
	float m_fFontSize = 24.0f;
	Zenith_Maths::Vector4 m_xTextColor = {1.0f, 1.0f, 1.0f, 1.0f};

	// Per-state styles (on/off)
	UIStyle m_xOnStyle;
	UIStyle m_xOffStyle;
	UIStyle m_xCurrentStyle; // runtime interpolated

	// Transition
	float m_fTransitionDuration = 0.1f;
};

} // namespace Zenith_UI
