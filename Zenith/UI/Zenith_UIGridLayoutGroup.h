#pragma once

#include "UI/Zenith_UIElement.h"

namespace Zenith_UI {

// Fixed-column, fixed-cell-size grid container. Grid analogue of
// Zenith_UILayoutGroup (which only does horizontal/vertical single-axis
// arrangement). Places visible children row-major into uniform cells.
//
// Coordinate convention (shared with the rest of the UI system): origin
// top-left, +X right, +Y DOWN, units in pixels. Emitted cell positions are
// LOCAL offsets from the group's top-left; children keep their default
// anchor/pivot.
class Zenith_UIGridLayoutGroup : public Zenith_UIElement
{
public:
	Zenith_UIGridLayoutGroup(const std::string& strName = "UIGridLayoutGroup");
	virtual ~Zenith_UIGridLayoutGroup() = default;

	virtual UIElementType GetType() const override { return UIElementType::GridLayoutGroup; }

	// ========== Grid Properties ==========

	void SetColumns(uint32_t uColumns);                 // clamp to >= 1, mark dirty
	uint32_t GetColumns() const { return m_uColumns; }

	void SetCellSize(float fWidth, float fHeight);      // mark dirty
	Zenith_Maths::Vector2 GetCellSize() const { return m_xCellSize; }

	void SetSpacing(float fHorizontal, float fVertical);// mark dirty
	Zenith_Maths::Vector2 GetSpacing() const { return m_xSpacing; }   // x=horiz, y=vert

	void SetPadding(float fLeft, float fTop, float fRight, float fBottom); // mark dirty
	Zenith_Maths::Vector4 GetPadding() const { return m_xPadding; }  // l,t,r,b

	void SetFitToContent(bool b);                       // mark dirty
	bool GetFitToContent() const { return m_bFitToContent; }

	void MarkLayoutDirty() { m_bLayoutDirty = true; }

	virtual void OnChildVisibilityChanged() override { m_bLayoutDirty = true; }
	virtual void OnChildSizeChanged() override        { m_bLayoutDirty = true; }
	virtual void OnChildAdded() override              { m_bLayoutDirty = true; }
	virtual void OnChildRemoved() override            { m_bLayoutDirty = true; }

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

	uint32_t              m_uColumns      = 1;
	Zenith_Maths::Vector2 m_xCellSize     = { 100.f, 100.f };
	Zenith_Maths::Vector2 m_xSpacing      = { 0.f, 0.f };
	Zenith_Maths::Vector4 m_xPadding      = { 0.f, 0.f, 0.f, 0.f };  // l,t,r,b
	bool                  m_bFitToContent = true;
	mutable bool          m_bLayoutDirty  = true;
};

} // namespace Zenith_UI
