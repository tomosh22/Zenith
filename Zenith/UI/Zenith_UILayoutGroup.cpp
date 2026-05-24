#include "Zenith.h"
#include "UI/Zenith_UILayoutGroup.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UICanvas.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "AssetHandling/Zenith_FontAsset.h"

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

	const float fPadLeft   = m_xPadding.x;
	const float fPadTop    = m_xPadding.y;
	const float fPadRight  = m_xPadding.z;
	const float fPadBottom = m_xPadding.w;

	RecalculateDirtyChildLayouts();
	WrapTextChildrenToCrossSize(fPadLeft, fPadTop, fPadRight, fPadBottom);

	// Phase 1 — Measure
	float fTotalPrimary = 0.f;
	float fMaxCross = 0.f;
	uint32_t uVisibleCount = 0;
	MeasureChildren(fTotalPrimary, fMaxCross, uVisibleCount);

	// Phase 2 — Fit container to content (may expand m_xSize)
	FitContainerToContent(fTotalPrimary, fMaxCross, fPadLeft, fPadTop, fPadRight, fPadBottom);

	// Phase 3 — Position children. Bundle shared placement state in a context struct
	// so PlaceChild has a sane signature and we can update fCursor as we go.
	const float fAvailableCross = (m_eDirection == LayoutDirection::Horizontal)
		? (m_xSize.y - fPadTop - fPadBottom)
		: (m_xSize.x - fPadLeft - fPadRight);

	const bool bForceExpandPrimary = (m_eDirection == LayoutDirection::Horizontal)
		? m_bChildForceExpandWidth : m_bChildForceExpandHeight;
	const bool bForceExpandCross = (m_eDirection == LayoutDirection::Horizontal)
		? m_bChildForceExpandHeight : m_bChildForceExpandWidth;

	float fExpandedPrimarySize = 0.f;
	if (bForceExpandPrimary && uVisibleCount > 0 && !m_bFitToContent)
	{
		const float fAvailablePrimary = (m_eDirection == LayoutDirection::Horizontal)
			? (m_xSize.x - fPadLeft - fPadRight)
			: (m_xSize.y - fPadTop - fPadBottom);
		const float fSpacingTotal = (uVisibleCount > 1) ? m_fSpacing * (uVisibleCount - 1) : 0.f;
		fExpandedPrimarySize = (fAvailablePrimary - fSpacingTotal) / uVisibleCount;
	}

	const AlignmentAxes xAxes = DecodeAlignment(m_eChildAlignment);
	const uint32_t uCrossAlign = (m_eDirection == LayoutDirection::Horizontal) ? xAxes.m_uRow : xAxes.m_uCol;

	PlacementContext xCtx;
	xCtx.m_fAvailableCross     = fAvailableCross;
	xCtx.m_fExpandedPrimarySize = fExpandedPrimarySize;
	xCtx.m_fCrossPad           = (m_eDirection == LayoutDirection::Horizontal) ? fPadTop : fPadLeft;
	xCtx.m_fCursor             = (m_eDirection == LayoutDirection::Horizontal) ? fPadLeft : fPadTop;
	xCtx.m_uCrossAlign         = uCrossAlign;
	xCtx.m_bForceExpandPrimary = bForceExpandPrimary;
	xCtx.m_bForceExpandCross   = bForceExpandCross;

	if (m_bReverseArrangement)
	{
		for (int32_t i = static_cast<int32_t>(m_xChildren.GetSize()) - 1; i >= 0; --i)
			PlaceChild(static_cast<uint32_t>(i), xCtx);
	}
	else
	{
		for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
			PlaceChild(i, xCtx);
	}

	// Phase 4 — Force-expand cross axis (overrides per-child sizes set during placement).
	if (bForceExpandCross)
		ApplyForceExpandCross(fAvailableCross);
}

void Zenith_UILayoutGroup::RecalculateDirtyChildLayouts()
{
	// Recurse into dirty child layout groups first so their sizes are up-to-date when
	// we measure them below.
	for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
	{
		Zenith_UIElement* pxChild = m_xChildren.Get(i);
		if (pxChild && pxChild->GetType() == UIElementType::LayoutGroup)
		{
			Zenith_UILayoutGroup* pxChildLayout = static_cast<Zenith_UILayoutGroup*>(pxChild);
			if (pxChildLayout->m_bLayoutDirty)
				pxChildLayout->RecalculateLayout();
		}
	}
}

void Zenith_UILayoutGroup::WrapTextChildrenToCrossSize(float fPadLeft, float fPadTop, float fPadRight, float fPadBottom)
{
	// Only when the group has a fixed size — otherwise there's no authoritative cross
	// size to wrap against (and Measure will derive one from children instead).
	if (m_bFitToContent)
		return;

	const float fCrossSpace = (m_eDirection == LayoutDirection::Horizontal)
		? (m_xSize.y - fPadTop - fPadBottom)
		: (m_xSize.x - fPadLeft - fPadRight);
	if (fCrossSpace <= 0.f)
		return;

	for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
	{
		Zenith_UIElement* pxChild = m_xChildren.Get(i);
		if (pxChild && pxChild->IsVisible() && pxChild->GetType() == UIElementType::Text)
		{
			static_cast<Zenith_UIText*>(pxChild)->SetMaxWidth(fCrossSpace);
		}
	}
}

void Zenith_UILayoutGroup::MeasureChildren(float& fOutTotalPrimary, float& fOutMaxCross, uint32_t& uOutVisibleCount) const
{
	fOutTotalPrimary = 0.f;
	fOutMaxCross = 0.f;
	uOutVisibleCount = 0;

	for (uint32_t i = 0; i < m_xChildren.GetSize(); ++i)
	{
		Zenith_UIElement* pxChild = m_xChildren.Get(i);
		if (!pxChild || !pxChild->IsVisible())
			continue;

		fOutTotalPrimary += GetChildPrimarySize(pxChild, m_eDirection);
		const float fCross = GetChildCrossSize(pxChild, m_eDirection);
		if (fCross > fOutMaxCross)
			fOutMaxCross = fCross;
		uOutVisibleCount++;
	}

	if (uOutVisibleCount > 1)
		fOutTotalPrimary += m_fSpacing * (uOutVisibleCount - 1);
}

void Zenith_UILayoutGroup::FitContainerToContent(float fTotalPrimary, float fMaxCross,
	float fPadLeft, float fPadTop, float fPadRight, float fPadBottom)
{
	if (!m_bFitToContent)
		return;

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

Zenith_UILayoutGroup::AlignmentAxes Zenith_UILayoutGroup::DecodeAlignment(ChildAlignment eAlignment)
{
	// ChildAlignment is laid out as a 3x3 grid: row * 3 + col where row is
	// Upper/Middle/Lower (0..2) and col is Left/Center/Right (0..2).
	const uint32_t uVal = static_cast<uint32_t>(eAlignment);
	return AlignmentAxes{ uVal / 3, uVal % 3 };
}

void Zenith_UILayoutGroup::PlaceChild(uint32_t uIndex, PlacementContext& xCtx)
{
	Zenith_UIElement* pxChild = m_xChildren.Get(uIndex);
	if (!pxChild || !pxChild->IsVisible())
		return;

	float fPrimary = GetChildPrimarySize(pxChild, m_eDirection);
	float fCross = GetChildCrossSize(pxChild, m_eDirection);

	// Sync text element size to its measured text dimensions so bounds match what's
	// drawn (prevents overlap with siblings).
	if (pxChild->GetType() == UIElementType::Text)
	{
		Zenith_UIText* pxText = static_cast<Zenith_UIText*>(pxChild);
		pxText->SetSize(pxText->GetTextWidth(), pxText->GetTextHeight());
	}

	// When force-expanding cross-axis, text elements are expanded to fill the space
	// and self-center within their bounds. Use the expanded size for alignment here
	// to avoid double-centering (layout + text alignment both applying).
	if (xCtx.m_bForceExpandCross && pxChild->GetType() == UIElementType::Text)
		fCross = xCtx.m_fAvailableCross;

	if (xCtx.m_bForceExpandPrimary && !m_bFitToContent)
	{
		fPrimary = xCtx.m_fExpandedPrimarySize;
		if (m_eDirection == LayoutDirection::Horizontal)
			pxChild->SetSize(fPrimary, pxChild->GetSize().y);
		else
			pxChild->SetSize(pxChild->GetSize().x, fPrimary);
	}

	// Cross-axis offset (Left/Center/Right or Upper/Middle/Lower)
	float fCrossOffset;
	switch (xCtx.m_uCrossAlign)
	{
	case 0: // Upper / Left
		fCrossOffset = xCtx.m_fCrossPad;
		break;
	case 1: // Middle / Center
		fCrossOffset = xCtx.m_fCrossPad + (xCtx.m_fAvailableCross - fCross) * 0.5f;
		break;
	case 2: // Lower / Right
	default:
		fCrossOffset = xCtx.m_fCrossPad + xCtx.m_fAvailableCross - fCross;
		break;
	}

	// Text glyph correction: SDF atlas glyphs sit below the top of the cell due to
	// ascender padding, so shift non-text children up to align with the perceived
	// text baseline. Uses the first visible text sibling's font size as reference.
	if (pxChild->GetType() != UIElementType::Text)
	{
		for (uint32_t s = 0; s < m_xChildren.GetSize(); ++s)
		{
			Zenith_UIElement* pxSibling = m_xChildren.Get(s);
			if (pxSibling && pxSibling->IsVisible() && pxSibling->GetType() == UIElementType::Text)
			{
				fCrossOffset -= static_cast<Zenith_UIText*>(pxSibling)->GetFontSize() * Zenith_FontAsset::GetActiveOrDefaultMetrics().fLayoutAscenderCorrection;
				break;
			}
		}
	}

	if (m_eDirection == LayoutDirection::Horizontal)
		pxChild->SetPosition(xCtx.m_fCursor, fCrossOffset);
	else
		pxChild->SetPosition(fCrossOffset, xCtx.m_fCursor);

	xCtx.m_fCursor += fPrimary + m_fSpacing;
}

void Zenith_UILayoutGroup::ApplyForceExpandCross(float fAvailableCross)
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
