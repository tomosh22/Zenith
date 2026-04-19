#pragma once

#include "UI/Zenith_UIElement.h"

namespace Zenith_UI {

enum class LayoutDirection : uint32_t
{
	Horizontal,
	Vertical
};

enum class ChildAlignment : uint32_t
{
	UpperLeft = 0,
	UpperCenter,
	UpperRight,
	MiddleLeft,
	MiddleCenter,
	MiddleRight,
	LowerLeft,
	LowerCenter,
	LowerRight
};

class Zenith_UILayoutGroup : public Zenith_UIElement
{
public:
	Zenith_UILayoutGroup(const std::string& strName = "UILayoutGroup");
	virtual ~Zenith_UILayoutGroup() = default;

	virtual UIElementType GetType() const override { return UIElementType::LayoutGroup; }

	// ========== Layout Properties (Unity-compatible) ==========

	void SetDirection(LayoutDirection eDir);
	LayoutDirection GetDirection() const { return m_eDirection; }

	void SetChildAlignment(ChildAlignment eAlign);
	ChildAlignment GetChildAlignment() const { return m_eChildAlignment; }

	void SetPadding(float fLeft, float fTop, float fRight, float fBottom);
	Zenith_Maths::Vector4 GetPadding() const { return m_xPadding; }

	void SetSpacing(float fSpacing);
	float GetSpacing() const { return m_fSpacing; }

	void SetChildForceExpandWidth(bool b);
	void SetChildForceExpandHeight(bool b);
	bool GetChildForceExpandWidth() const { return m_bChildForceExpandWidth; }
	bool GetChildForceExpandHeight() const { return m_bChildForceExpandHeight; }

	void SetReverseArrangement(bool b);
	bool GetReverseArrangement() const { return m_bReverseArrangement; }

	void SetFitToContent(bool b);
	bool GetFitToContent() const { return m_bFitToContent; }

	void MarkLayoutDirty() { m_bLayoutDirty = true; }

	virtual void OnChildVisibilityChanged() override { m_bLayoutDirty = true; }
	virtual void OnChildSizeChanged() override { m_bLayoutDirty = true; }
	virtual void OnChildAdded() override { m_bLayoutDirty = true; }
	virtual void OnChildRemoved() override { m_bLayoutDirty = true; }

	// ========== Overrides ==========

	virtual void Update(float fDt) override;
	virtual void Render(Zenith_UICanvas& xCanvas) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	virtual void RenderPropertiesPanel() override;
#endif

private:
	void RecalculateLayout();

	// Internal layout phases, extracted from RecalculateLayout to keep each step focused.

	// Flatten ChildAlignment (9-value enum) into independent row/col selectors.
	struct AlignmentAxes
	{
		uint32_t m_uRow; // 0=Upper, 1=Middle, 2=Lower
		uint32_t m_uCol; // 0=Left,  1=Center, 2=Right
	};

	// Shared state used by PlaceChild; filled in by the outer RecalculateLayout and
	// mutated for the running cursor. Struct exists only to keep PlaceChild's
	// signature manageable (lots of geometry bundled as one parameter).
	struct PlacementContext
	{
		float m_fAvailableCross;
		float m_fExpandedPrimarySize;
		float m_fCrossPad;
		float m_fCursor;           // advances as children are placed
		uint32_t m_uCrossAlign;
		bool m_bForceExpandPrimary;
		bool m_bForceExpandCross;
	};

	static AlignmentAxes DecodeAlignment(ChildAlignment eAlignment);

	void RecalculateDirtyChildLayouts();
	void WrapTextChildrenToCrossSize(float fPadLeft, float fPadTop, float fPadRight, float fPadBottom);
	void MeasureChildren(float& fOutTotalPrimary, float& fOutMaxCross, uint32_t& uOutVisibleCount) const;
	void FitContainerToContent(float fTotalPrimary, float fMaxCross,
		float fPadLeft, float fPadTop, float fPadRight, float fPadBottom);
	void PlaceChild(uint32_t uIndex, PlacementContext& xCtx);
	void ApplyForceExpandCross(float fAvailableCross);

	LayoutDirection m_eDirection = LayoutDirection::Horizontal;
	ChildAlignment m_eChildAlignment = ChildAlignment::MiddleCenter;
	Zenith_Maths::Vector4 m_xPadding = { 0.f, 0.f, 0.f, 0.f };  // left, top, right, bottom
	float m_fSpacing = 0.0f;
	bool m_bChildForceExpandWidth = false;
	bool m_bChildForceExpandHeight = false;
	bool m_bReverseArrangement = false;
	bool m_bFitToContent = true;
	mutable bool m_bLayoutDirty = true;
};

} // namespace Zenith_UI
