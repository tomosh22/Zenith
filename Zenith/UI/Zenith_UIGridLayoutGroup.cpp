#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "UI/Zenith_UIGridLayoutGroup.h"
#include "UI/Zenith_UICanvas.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_GRID_LAYOUT_GROUP_VERSION = 1;

Zenith_UIGridLayoutGroup::Zenith_UIGridLayoutGroup(const std::string& strName)
	: Zenith_UIElement(strName)
{
}

// ========== Setters ==========

void Zenith_UIGridLayoutGroup::SetColumns(uint32_t uColumns)
{
	m_uColumns = uColumns < 1u ? 1u : uColumns;
	m_bLayoutDirty = true;
}

void Zenith_UIGridLayoutGroup::SetCellSize(float fWidth, float fHeight)
{
	m_xCellSize = { fWidth, fHeight };
	m_bLayoutDirty = true;
}

void Zenith_UIGridLayoutGroup::SetSpacing(float fHorizontal, float fVertical)
{
	m_xSpacing = { fHorizontal, fVertical };
	m_bLayoutDirty = true;
}

void Zenith_UIGridLayoutGroup::SetPadding(float fLeft, float fTop, float fRight, float fBottom)
{
	m_xPadding = { fLeft, fTop, fRight, fBottom };
	m_bLayoutDirty = true;
}

void Zenith_UIGridLayoutGroup::SetFitToContent(bool b)
{
	m_bFitToContent = b;
	m_bLayoutDirty = true;
}

// ========== Layout Algorithm ==========

void Zenith_UIGridLayoutGroup::RecalculateLayout()
{
	ZENITH_PROFILE_SCOPE("UI Grid Layout Solve");
	m_bLayoutDirty = false;

	// Defensive against a serialized 0 sneaking past the setter clamp.
	const uint32_t uCols = m_uColumns < 1u ? 1u : m_uColumns;

	// Place each VISIBLE child into its row-major cell. uVisibleIndex counts only
	// visible children, so hidden children leave NO gap in the grid.
	uint32_t uVisibleIndex = 0;
	for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
	{
		Zenith_UIElement* pxChild = m_xChildren.Get(i);
		if (!pxChild || !pxChild->IsVisible())
			continue;

		const uint32_t uCol = uVisibleIndex % uCols;
		const uint32_t uRow = uVisibleIndex / uCols;

		const float fX = m_xPadding.x + uCol * (m_xCellSize.x + m_xSpacing.x);
		const float fY = m_xPadding.y + uRow * (m_xCellSize.y + m_xSpacing.y);

		// Anti-thrash guard: SetSize fires OnChildSizeChanged -> re-dirties us, so only
		// resize when the child does not already match the cell. This lets the layout
		// converge (a subsequent Update finds every child already cell-sized and stops).
		// Compared component-wise (exact) matching the codebase idiom for glm vectors;
		// SetSize stores the cell size verbatim so the next pass compares equal and stops.
		const Zenith_Maths::Vector2 xChildSize = pxChild->GetSize();
		if (xChildSize.x != m_xCellSize.x || xChildSize.y != m_xCellSize.y)
			pxChild->SetSize(m_xCellSize.x, m_xCellSize.y);

		pxChild->SetPosition(fX, fY);

		++uVisibleIndex;
	}

	// Auto-size the container to the tight bounds of the occupied cells.
	if (m_bFitToContent)
	{
		const uint32_t uVisibleCount = uVisibleIndex;
		const uint32_t uUsedCols = (uVisibleCount == 0) ? 0u : (uVisibleCount < uCols ? uVisibleCount : uCols);
		const uint32_t uUsedRows = (uVisibleCount == 0) ? 0u : (uVisibleCount + uCols - 1) / uCols;

		const float fContentW = (uUsedCols == 0)
			? 0.f
			: uUsedCols * m_xCellSize.x + (uUsedCols - 1) * m_xSpacing.x;
		const float fContentH = (uUsedRows == 0)
			? 0.f
			: uUsedRows * m_xCellSize.y + (uUsedRows - 1) * m_xSpacing.y;

		m_xSize.x = m_xPadding.x + fContentW + m_xPadding.z;
		m_xSize.y = m_xPadding.y + fContentH + m_xPadding.w;
		m_bTransformDirty = true;
	}
}

// ========== Update / Render ==========

void Zenith_UIGridLayoutGroup::Update(float fDt)
{
	if (m_bLayoutDirty)
	{
		RecalculateLayout();
	}

	Zenith_UIElement::Update(fDt);
}

void Zenith_UIGridLayoutGroup::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	// Grid group itself is invisible — just renders children.
	Zenith_UIElement::Render(xCanvas);
}

// ========== Serialization ==========

void Zenith_UIGridLayoutGroup::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_GRID_LAYOUT_GROUP_VERSION;
	xStream << m_uColumns;
	xStream << m_xCellSize.x;
	xStream << m_xCellSize.y;
	xStream << m_xSpacing.x;
	xStream << m_xSpacing.y;
	xStream << m_xPadding.x;
	xStream << m_xPadding.y;
	xStream << m_xPadding.z;
	xStream << m_xPadding.w;
	xStream << m_bFitToContent;
}

void Zenith_UIGridLayoutGroup::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	xStream >> m_uColumns;
	xStream >> m_xCellSize.x;
	xStream >> m_xCellSize.y;
	xStream >> m_xSpacing.x;
	xStream >> m_xSpacing.y;
	xStream >> m_xPadding.x;
	xStream >> m_xPadding.y;
	xStream >> m_xPadding.z;
	xStream >> m_xPadding.w;
	xStream >> m_bFitToContent;

	m_bLayoutDirty = true;
}

// ========== Editor Properties Panel ==========

#ifdef ZENITH_TOOLS
void Zenith_UIGridLayoutGroup::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIGridLayoutGroupProps");

	ImGui::Separator();
	ImGui::Text("Grid Layout Group Properties");

	int iColumns = static_cast<int>(m_uColumns);
	if (ImGui::DragInt("Columns", &iColumns, 1.f, 1, 64))
	{
		SetColumns(static_cast<uint32_t>(iColumns < 1 ? 1 : iColumns));
	}

	float fCellSize[2] = { m_xCellSize.x, m_xCellSize.y };
	if (ImGui::DragFloat2("Cell Size", fCellSize, 1.f, 0.f, 2000.f))
	{
		SetCellSize(fCellSize[0], fCellSize[1]);
	}

	float fSpacing[2] = { m_xSpacing.x, m_xSpacing.y };
	if (ImGui::DragFloat2("Spacing", fSpacing, 1.f, 0.f, 500.f))
	{
		SetSpacing(fSpacing[0], fSpacing[1]);
	}

	float fPadding[4] = { m_xPadding.x, m_xPadding.y, m_xPadding.z, m_xPadding.w };
	bool bPaddingChanged = false;
	bPaddingChanged |= ImGui::DragFloat("Pad Left", &fPadding[0], 1.f, 0.f, 500.f);
	bPaddingChanged |= ImGui::DragFloat("Pad Top", &fPadding[1], 1.f, 0.f, 500.f);
	bPaddingChanged |= ImGui::DragFloat("Pad Right", &fPadding[2], 1.f, 0.f, 500.f);
	bPaddingChanged |= ImGui::DragFloat("Pad Bottom", &fPadding[3], 1.f, 0.f, 500.f);
	if (bPaddingChanged)
	{
		SetPadding(fPadding[0], fPadding[1], fPadding[2], fPadding[3]);
	}

	bool bFit = m_bFitToContent;
	if (ImGui::Checkbox("Fit To Content", &bFit))
	{
		SetFitToContent(bFit);
	}

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI

// ZENITH_TEST macros self-noop when ZENITH_TESTING is undefined, so this include
// stays unconditional (matching every other .Tests.inl host).
#include "UI/Zenith_UIGridLayoutGroup.Tests.inl"
