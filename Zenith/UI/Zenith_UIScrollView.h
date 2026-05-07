#pragma once

#include "UI/Zenith_UIElement.h"

/**
 * Zenith_UIScrollView - Scrollable content container
 *
 * Defines a viewport that clips its children. Content can be scrolled
 * vertically, horizontally, or both. Children are offset by the scroll
 * position during rendering.
 *
 * Content clipping uses the canvas clip rect stack (CPU-side bounds clamping).
 */

namespace Zenith_UI {

enum class ScrollDirection : uint32_t
{
	VERTICAL,
	HORIZONTAL,
	BOTH
};

class Zenith_UIScrollView : public Zenith_UIElement
{
public:
	Zenith_UIScrollView(const std::string& strName = "UIScrollView");
	virtual ~Zenith_UIScrollView() = default;

	virtual UIElementType GetType() const override { return UIElementType::ScrollView; }

	// ========== Content ==========

	void SetContentSize(float fW, float fH) { m_xContentSize = {fW, fH}; }
	Zenith_Maths::Vector2 GetContentSize() const { return m_xContentSize; }

	// ========== Scroll Position ==========

	void SetScrollPosition(float fX, float fY);
	Zenith_Maths::Vector2 GetScrollPosition() const { return m_xScrollPosition; }

	// ========== Scroll Direction ==========

	void SetScrollDirection(ScrollDirection eDir) { m_eScrollDirection = eDir; }
	ScrollDirection GetScrollDirection() const { return m_eScrollDirection; }

	// ========== Inertia ==========

	void SetInertia(bool bEnabled) { m_bInertia = bEnabled; }
	bool HasInertia() const { return m_bInertia; }

	void SetDecelerationRate(float fRate) { m_fDecelerationRate = fRate; }
	float GetDecelerationRate() const { return m_fDecelerationRate; }

	// ========== Overrides ==========

	virtual void Update(float fDt) override;
	virtual void Render(Zenith_UICanvas& xCanvas) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	virtual void RenderPropertiesPanel() override;
#endif

private:
	void ClampScrollPosition();
	void HandleDragInput(float fMouseX, float fMouseY, bool bInside, float fDt);
	void UpdateInertia(float fDt);

	Zenith_Maths::Vector2 m_xContentSize = {0.f, 0.f};
	Zenith_Maths::Vector2 m_xScrollPosition = {0.f, 0.f};
	Zenith_Maths::Vector2 m_xScrollVelocity = {0.f, 0.f};
	ScrollDirection m_eScrollDirection = ScrollDirection::VERTICAL;
	bool m_bInertia = true;
	float m_fDecelerationRate = 0.135f;

	// Drag tracking
	bool m_bDragging = false;
	Zenith_Maths::Vector2 m_xDragStart = {0.f, 0.f};
	Zenith_Maths::Vector2 m_xScrollStart = {0.f, 0.f};
	bool m_bMouseDownLastFrame = false;
};

} // namespace Zenith_UI
