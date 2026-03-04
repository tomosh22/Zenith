#include "Zenith.h"
#include "UI/Zenith_UILayoutGroup.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UICanvas.h"
#include "Flux/Text/Flux_Text.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_LAYOUT_GROUP_VERSION = 1;

Zenith_UILayoutGroup::Zenith_UILayoutGroup(const std::string& strName)
	: Zenith_UIElement(strName)
{
}

// ========== Setters ==========

void Zenith_UILayoutGroup::SetDirection(LayoutDirection eDir)
{
	m_eDirection = eDir;
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetChildAlignment(ChildAlignment eAlign)
{
	m_eChildAlignment = eAlign;
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetPadding(float fLeft, float fTop, float fRight, float fBottom)
{
	m_xPadding = { fLeft, fTop, fRight, fBottom };
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetSpacing(float fSpacing)
{
	m_fSpacing = fSpacing;
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetChildForceExpandWidth(bool b)
{
	m_bChildForceExpandWidth = b;
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetChildForceExpandHeight(bool b)
{
	m_bChildForceExpandHeight = b;
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetReverseArrangement(bool b)
{
	m_bReverseArrangement = b;
	m_bLayoutDirty = true;
}

void Zenith_UILayoutGroup::SetFitToContent(bool b)
{
	m_bFitToContent = b;
	m_bLayoutDirty = true;
}

// ========== Layout Algorithm ==========

static float GetChildPrimarySize(Zenith_UIElement* pxChild, LayoutDirection eDir)
{
	if (pxChild->GetType() == UIElementType::Text)
	{
		Zenith_UIText* pxText = static_cast<Zenith_UIText*>(pxChild);
		if (eDir == LayoutDirection::Horizontal)
			return pxText->GetTextWidth();
		else
			return pxText->GetTextHeight();
	}

	if (eDir == LayoutDirection::Horizontal)
		return pxChild->GetSize().x;
	else
		return pxChild->GetSize().y;
}

static float GetChildCrossSize(Zenith_UIElement* pxChild, LayoutDirection eDir)
{
	if (pxChild->GetType() == UIElementType::Text)
	{
		Zenith_UIText* pxText = static_cast<Zenith_UIText*>(pxChild);
		if (eDir == LayoutDirection::Horizontal)
			return pxText->GetTextHeight();
		else
			return pxText->GetTextWidth();
	}

	if (eDir == LayoutDirection::Horizontal)
		return pxChild->GetSize().y;
	else
		return pxChild->GetSize().x;
}

void Zenith_UILayoutGroup::RecalculateLayout()
{
	m_bLayoutDirty = false;

	float fPadLeft = m_xPadding.x;
	float fPadTop = m_xPadding.y;
	float fPadRight = m_xPadding.z;
	float fPadBottom = m_xPadding.w;

	// Phase 1 — Measure: accumulate primary-axis extent and max cross-axis extent
	float fTotalPrimary = 0.f;
	float fMaxCross = 0.f;
	uint32_t uVisibleCount = 0;

	for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
	{
		Zenith_UIElement* pxChild = m_xChildren.Get(i);
		if (!pxChild || !pxChild->IsVisible())
			continue;

		float fPrimary = GetChildPrimarySize(pxChild, m_eDirection);
		float fCross = GetChildCrossSize(pxChild, m_eDirection);

		fTotalPrimary += fPrimary;
		if (fCross > fMaxCross)
			fMaxCross = fCross;
		uVisibleCount++;
	}

	if (uVisibleCount > 1)
		fTotalPrimary += m_fSpacing * (uVisibleCount - 1);

	// Phase 2 — Fit container to content
	if (m_bFitToContent)
	{
		if (m_eDirection == LayoutDirection::Horizontal)
		{
			m_xSize.x = fTotalPrimary + fPadLeft + fPadRight;
			m_xSize.y = fMaxCross + fPadTop + fPadBottom;
		}
		else
		{
			m_xSize.x = fMaxCross + fPadLeft + fPadRight;
			m_xSize.y = fTotalPrimary + fPadTop + fPadBottom;
		}
		m_bTransformDirty = true;
	}

	// Calculate available space for cross-axis alignment
	float fAvailableCross;
	if (m_eDirection == LayoutDirection::Horizontal)
		fAvailableCross = m_xSize.y - fPadTop - fPadBottom;
	else
		fAvailableCross = m_xSize.x - fPadLeft - fPadRight;

	// Phase 4 — Force expand: calculate expanded sizes if needed
	bool bForceExpandPrimary = (m_eDirection == LayoutDirection::Horizontal)
		? m_bChildForceExpandWidth : m_bChildForceExpandHeight;

	float fExpandedPrimarySize = 0.f;
	if (bForceExpandPrimary && uVisibleCount > 0 && !m_bFitToContent)
	{
		float fAvailablePrimary;
		if (m_eDirection == LayoutDirection::Horizontal)
			fAvailablePrimary = m_xSize.x - fPadLeft - fPadRight;
		else
			fAvailablePrimary = m_xSize.y - fPadTop - fPadBottom;

		float fSpacingTotal = (uVisibleCount > 1) ? m_fSpacing * (uVisibleCount - 1) : 0.f;
		fExpandedPrimarySize = (fAvailablePrimary - fSpacingTotal) / uVisibleCount;
	}

	// Extract alignment components
	uint32_t uAlignVal = static_cast<uint32_t>(m_eChildAlignment);
	uint32_t uAlignRow = uAlignVal / 3;  // 0=Upper, 1=Middle, 2=Lower
	uint32_t uAlignCol = uAlignVal % 3;  // 0=Left, 1=Center, 2=Right

	// For horizontal layout: cross-axis uses row (Upper/Middle/Lower)
	// For vertical layout: cross-axis uses col (Left/Center/Right)
	uint32_t uCrossAlign = (m_eDirection == LayoutDirection::Horizontal) ? uAlignRow : uAlignCol;

	// Phase 3 — Position children
	float fCursor = (m_eDirection == LayoutDirection::Horizontal) ? fPadLeft : fPadTop;

	auto PlaceChild = [&](uint32_t uIndex)
	{
		Zenith_UIElement* pxChild = m_xChildren.Get(uIndex);
		if (!pxChild || !pxChild->IsVisible())
			return;

		float fPrimary = GetChildPrimarySize(pxChild, m_eDirection);
		float fCross = GetChildCrossSize(pxChild, m_eDirection);

		// Force expand: override primary size
		if (bForceExpandPrimary && !m_bFitToContent)
		{
			fPrimary = fExpandedPrimarySize;
			if (m_eDirection == LayoutDirection::Horizontal)
				pxChild->SetSize(fPrimary, pxChild->GetSize().y);
			else
				pxChild->SetSize(pxChild->GetSize().x, fPrimary);
		}

		// Calculate cross-axis offset
		float fCrossOffset;
		float fCrossPad = (m_eDirection == LayoutDirection::Horizontal) ? fPadTop : fPadLeft;
		switch (uCrossAlign)
		{
		case 0: // Upper / Left
			fCrossOffset = fCrossPad;
			break;
		case 1: // Middle / Center
			fCrossOffset = fCrossPad + (fAvailableCross - fCross) * 0.5f;
			break;
		case 2: // Lower / Right
		default:
			fCrossOffset = fCrossPad + fAvailableCross - fCross;
			break;
		}

		// Text glyph correction: SDF atlas glyphs sit below the top of the cell
		// due to ascender padding, so shift non-text children up to match the
		// perceived text baseline.  Only applies on the cross axis.
		if (pxChild->GetType() != UIElementType::Text)
		{
			// Find the first visible text sibling to derive the correction magnitude
			for (uint32_t s = 0; s < m_xChildren.GetSize(); ++s)
			{
				Zenith_UIElement* pxSibling = m_xChildren.Get(s);
				if (pxSibling && pxSibling->IsVisible() && pxSibling->GetType() == UIElementType::Text)
				{
					Zenith_UIText* pxText = static_cast<Zenith_UIText*>(pxSibling);
					fCrossOffset -= pxText->GetFontSize() * fFONT_ASCENDER_RATIO;
					break;
				}
			}
		}

		// Set child position
		if (m_eDirection == LayoutDirection::Horizontal)
			pxChild->SetPosition(fCursor, fCrossOffset);
		else
			pxChild->SetPosition(fCrossOffset, fCursor);

		fCursor += fPrimary + m_fSpacing;
	};

	if (m_bReverseArrangement)
	{
		for (int32_t i = static_cast<int32_t>(m_xChildren.GetSize()) - 1; i >= 0; --i)
			PlaceChild(static_cast<uint32_t>(i));
	}
	else
	{
		for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
			PlaceChild(i);
	}

	// Force expand cross-axis: expand all children to fill cross dimension
	bool bForceExpandCross = (m_eDirection == LayoutDirection::Horizontal)
		? m_bChildForceExpandHeight : m_bChildForceExpandWidth;

	if (bForceExpandCross)
	{
		for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
		{
			Zenith_UIElement* pxChild = m_xChildren.Get(i);
			if (!pxChild || !pxChild->IsVisible())
				continue;

			if (m_eDirection == LayoutDirection::Horizontal)
				pxChild->SetSize(pxChild->GetSize().x, fAvailableCross);
			else
				pxChild->SetSize(fAvailableCross, pxChild->GetSize().y);
		}
	}
}

// ========== Update / Render ==========

void Zenith_UILayoutGroup::Update(float fDt)
{
	if (m_bLayoutDirty)
	{
		RecalculateLayout();
	}

	Zenith_UIElement::Update(fDt);
}

void Zenith_UILayoutGroup::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	// Layout group itself is invisible — just renders children
	Zenith_UIElement::Render(xCanvas);
}

// ========== Serialization ==========

void Zenith_UILayoutGroup::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_LAYOUT_GROUP_VERSION;
	xStream << static_cast<uint32_t>(m_eDirection);
	xStream << static_cast<uint32_t>(m_eChildAlignment);
	xStream << m_xPadding.x;
	xStream << m_xPadding.y;
	xStream << m_xPadding.z;
	xStream << m_xPadding.w;
	xStream << m_fSpacing;
	xStream << m_bChildForceExpandWidth;
	xStream << m_bChildForceExpandHeight;
	xStream << m_bReverseArrangement;
	xStream << m_bFitToContent;
}

void Zenith_UILayoutGroup::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	uint32_t uDir;
	xStream >> uDir;
	m_eDirection = static_cast<LayoutDirection>(uDir);

	uint32_t uAlign;
	xStream >> uAlign;
	m_eChildAlignment = static_cast<ChildAlignment>(uAlign);

	xStream >> m_xPadding.x;
	xStream >> m_xPadding.y;
	xStream >> m_xPadding.z;
	xStream >> m_xPadding.w;
	xStream >> m_fSpacing;
	xStream >> m_bChildForceExpandWidth;
	xStream >> m_bChildForceExpandHeight;
	xStream >> m_bReverseArrangement;
	xStream >> m_bFitToContent;

	m_bLayoutDirty = true;
}

// ========== Editor Properties Panel ==========

#ifdef ZENITH_TOOLS
void Zenith_UILayoutGroup::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UILayoutGroupProps");

	ImGui::Separator();
	ImGui::Text("Layout Group Properties");

	const char* szDirections[] = { "Horizontal", "Vertical" };
	int iDir = static_cast<int>(m_eDirection);
	if (ImGui::Combo("Direction", &iDir, szDirections, 2))
	{
		SetDirection(static_cast<LayoutDirection>(iDir));
	}

	const char* szAlignments[] = {
		"Upper Left", "Upper Center", "Upper Right",
		"Middle Left", "Middle Center", "Middle Right",
		"Lower Left", "Lower Center", "Lower Right"
	};
	int iAlign = static_cast<int>(m_eChildAlignment);
	if (ImGui::Combo("Child Alignment", &iAlign, szAlignments, 9))
	{
		SetChildAlignment(static_cast<ChildAlignment>(iAlign));
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

	float fSpacing = m_fSpacing;
	if (ImGui::DragFloat("Spacing", &fSpacing, 1.f, 0.f, 200.f))
	{
		SetSpacing(fSpacing);
	}

	bool bForceW = m_bChildForceExpandWidth;
	if (ImGui::Checkbox("Force Expand Width", &bForceW))
	{
		SetChildForceExpandWidth(bForceW);
	}

	bool bForceH = m_bChildForceExpandHeight;
	if (ImGui::Checkbox("Force Expand Height", &bForceH))
	{
		SetChildForceExpandHeight(bForceH);
	}

	bool bReverse = m_bReverseArrangement;
	if (ImGui::Checkbox("Reverse Arrangement", &bReverse))
	{
		SetReverseArrangement(bReverse);
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
