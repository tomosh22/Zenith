#pragma once

#include "UI/Zenith_UIElement.h"

/**
 * Zenith_UIOverlay - Modal overlay widget
 *
 * Provides a full-screen dim background with a centered content container.
 * On Show(): fades in the dim background, sets sort order high, and disables
 * GroupInteractable on sibling elements to block input behind the overlay.
 * On Hide(): fades out and restores sibling interactability.
 *
 * Children added to the overlay are parented to the internal content container.
 * Uses the tween system for fade animation.
 */

namespace Zenith_UI {

class Zenith_UIOverlay : public Zenith_UIElement
{
public:
	Zenith_UIOverlay(const std::string& strName = "UIOverlay");
	virtual ~Zenith_UIOverlay() = default;

	virtual UIElementType GetType() const override { return UIElementType::Overlay; }

	// ========== Show/Hide ==========

	void Show();
	void Hide();
	bool IsShowing() const { return m_bShowing; }

	// ========== Dim Background ==========

	void SetDimColor(const Zenith_Maths::Vector4& xColor) { m_xDimColor = xColor; }
	Zenith_Maths::Vector4 GetDimColor() const { return m_xDimColor; }

	void SetFadeDuration(float fDuration) { m_fFadeDuration = fDuration; }
	float GetFadeDuration() const { return m_fFadeDuration; }

	// ========== Content Container ==========

	void SetContentSize(float fW, float fH) { m_xContentSize = {fW, fH}; SetSize(fW, fH); }
	Zenith_Maths::Vector2 GetContentSize() const { return m_xContentSize; }

	void SetContentStyle(const UIStyle& xStyle) { m_xContentStyle = xStyle; }
	const UIStyle& GetContentStyle() const { return m_xContentStyle; }

	// ========== Overrides ==========

	virtual void Update(float fDt) override;
	virtual void Render(Zenith_UICanvas& xCanvas) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	virtual void RenderPropertiesPanel() override;
#endif

private:
	void SetSiblingInteractable(bool bInteractable);

	bool m_bShowing = false;
	bool m_bHiding = false;

	// Dim background
	Zenith_Maths::Vector4 m_xDimColor = {0.0f, 0.0f, 0.0f, 0.7f};
	float m_fFadeDuration = 0.2f;
	float m_fCurrentDimAlpha = 0.0f;

	// Content container
	Zenith_Maths::Vector2 m_xContentSize = {400.0f, 300.0f};
	UIStyle m_xContentStyle;
};

} // namespace Zenith_UI
