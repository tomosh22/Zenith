#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "UI/Zenith_UIOverlay.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Flux/Text/Flux_TextImpl.h" // SetOverlayClipRect

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_OVERLAY_VERSION = 1;

Zenith_UIOverlay::Zenith_UIOverlay(const std::string& strName)
	: Zenith_UIElement(strName)
{
	m_bVisible = false;
	m_iSortOrder = 100;

	// Center the overlay so children anchor relative to the content area
	SetAnchorAndPivot(AnchorPreset::Center);
	SetSize(m_xContentSize.x, m_xContentSize.y);

	// Default content style: dark semi-transparent box with rounded corners
	m_xContentStyle.m_xFillColor = {0.15f, 0.15f, 0.20f, 1.0f};
	m_xContentStyle.m_fCornerRadius = 12.0f;
	m_xContentStyle.m_fBorderThickness = 2.0f;
	m_xContentStyle.m_xBorderColor = {0.4f, 0.4f, 0.5f, 1.0f};
}

void Zenith_UIOverlay::Show()
{
	// If already fully showing and visible, nothing to do
	if (m_bShowing && m_bVisible && !m_bHiding)
	{
		if (m_fFadeDuration <= 0.0f)
		{
			m_fCurrentDimAlpha = m_xDimColor.w;
		}
		Zenith_Log(LOG_CATEGORY_UI, "UIOverlay::Show() '%s' - already showing, skipping", m_strName.c_str());
		return;
	}

	Zenith_Log(LOG_CATEGORY_UI, "UIOverlay::Show() '%s' - canvas=%p visible=%d showing=%d hiding=%d",
		m_strName.c_str(), m_pxCanvas, m_bVisible, m_bShowing, m_bHiding);
	m_bShowing = true;
	m_bHiding = false;
	m_bVisible = true;
	m_fCurrentDimAlpha = m_fFadeDuration <= 0.0f
		? m_xDimColor.w
		: 0.0f;

	SetSiblingInteractable(false);
}

void Zenith_UIOverlay::Hide()
{
	if (!m_bShowing)
		return;

	if (m_fFadeDuration <= 0.0f)
	{
		m_bShowing = false;
		m_bHiding = false;
		m_bVisible = false;
		m_fCurrentDimAlpha = 0.0f;
		SetSiblingInteractable(true);
		return;
	}

	m_bHiding = true;
}

void Zenith_UIOverlay::Update(float fDt)
{
	if (!m_bVisible)
		return;

	float fTargetAlpha = m_bHiding ? 0.0f : m_xDimColor.w;

	if (m_fFadeDuration > 0.0f)
	{
		float fSpeed = fDt / m_fFadeDuration;
		if (m_fCurrentDimAlpha < fTargetAlpha)
		{
			m_fCurrentDimAlpha = glm::min(m_fCurrentDimAlpha + fSpeed, fTargetAlpha);
		}
		else if (m_fCurrentDimAlpha > fTargetAlpha)
		{
			m_fCurrentDimAlpha = glm::max(m_fCurrentDimAlpha - fSpeed, fTargetAlpha);
		}
	}
	else
	{
		m_fCurrentDimAlpha = fTargetAlpha;
	}

	// Fade out complete
	if (m_bHiding && m_fCurrentDimAlpha <= 0.001f)
	{
		m_bShowing = false;
		m_bHiding = false;
		m_bVisible = false;
		m_fCurrentDimAlpha = 0.0f;
		SetSiblingInteractable(true);
	}

	Zenith_UIElement::Update(fDt);
}

void Zenith_UIOverlay::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	float fAlpha = GetEffectiveAlpha();

	// One-shot diagnostic: log first render after Show()
	if (m_fCurrentDimAlpha < 0.01f && m_bShowing && !m_bHiding)
	{
		Zenith_Maths::Vector4 xBounds = GetScreenBounds();
		Zenith_Log(LOG_CATEGORY_UI, "UIOverlay::Render() '%s' - dimAlpha=%.3f effAlpha=%.3f bounds=(%.0f,%.0f,%.0f,%.0f) children=%u canvas=%p",
			m_strName.c_str(), m_fCurrentDimAlpha, fAlpha, xBounds.x, xBounds.y, xBounds.z, xBounds.w,
			GetChildren().GetSize(), m_pxCanvas);
	}

	// Render full-screen dim background
	if (m_pxCanvas)
	{
		Zenith_Maths::Vector2 xCanvasSize = m_pxCanvas->GetSize();
		Zenith_Maths::Vector4 xDimBounds = {0.0f, 0.0f, xCanvasSize.x, xCanvasSize.y};
		Zenith_Maths::Vector4 xDimCol = {m_xDimColor.x, m_xDimColor.y, m_xDimColor.z, m_fCurrentDimAlpha * fAlpha};
		xCanvas.SubmitQuad(xDimBounds, xDimCol);
	}

	// Render content container (centered via anchor/pivot system)
	if (m_xContentSize.x > 0.f && m_xContentSize.y > 0.f)
	{
		Zenith_Maths::Vector4 xContentBounds = GetScreenBounds();
		UIStyleRenderer::RenderStyledRect(xCanvas, m_xContentStyle, xContentBounds, m_fCurrentDimAlpha / glm::max(m_xDimColor.w, 0.01f) * fAlpha);
	}

	// Set clip rect so text from lower sort-order elements is discarded
	// within the content box bounds (handled in the Flux_Text fragment shader)
	g_xEngine.Text().SetOverlayClipRect(GetScreenBounds(), GetSortOrder());

	// Render children (submits overlay's own text entries)
	Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIOverlay::SetSiblingInteractable(bool bInteractable)
{
	if (!m_pxParent && m_pxCanvas)
	{
		const Zenith_Vector<Zenith_UIElement*>& xRoots = m_pxCanvas->GetElements();
		for (uint32_t u = 0; u < xRoots.GetSize(); u++)
		{
			Zenith_UIElement* pxSibling = xRoots.Get(u);
			if (pxSibling && pxSibling != this)
			{
				pxSibling->SetGroupInteractable(bInteractable);
			}
		}
	}
	else if (m_pxParent)
	{
		const Zenith_Vector<Zenith_UIElement*>& xChildren = m_pxParent->GetChildren();
		for (uint32_t u = 0; u < xChildren.GetSize(); u++)
		{
			Zenith_UIElement* pxSibling = xChildren.Get(u);
			if (pxSibling && pxSibling != this)
			{
				pxSibling->SetGroupInteractable(bInteractable);
			}
		}
	}
}

void Zenith_UIOverlay::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_OVERLAY_VERSION;
	xStream << m_xDimColor.x; xStream << m_xDimColor.y; xStream << m_xDimColor.z; xStream << m_xDimColor.w;
	xStream << m_fFadeDuration;
	xStream << m_xContentSize.x; xStream << m_xContentSize.y;

	// Content style
	xStream << m_xContentStyle.m_xFillColor.x; xStream << m_xContentStyle.m_xFillColor.y; xStream << m_xContentStyle.m_xFillColor.z; xStream << m_xContentStyle.m_xFillColor.w;
	xStream << m_xContentStyle.m_fCornerRadius;
	xStream << m_xContentStyle.m_fBorderThickness;
	xStream << m_xContentStyle.m_xBorderColor.x; xStream << m_xContentStyle.m_xBorderColor.y; xStream << m_xContentStyle.m_xBorderColor.z; xStream << m_xContentStyle.m_xBorderColor.w;
}

void Zenith_UIOverlay::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion == UI_OVERLAY_VERSION, "UIOverlay version mismatch");

	xStream >> m_xDimColor.x; xStream >> m_xDimColor.y; xStream >> m_xDimColor.z; xStream >> m_xDimColor.w;
	xStream >> m_fFadeDuration;
	xStream >> m_xContentSize.x; xStream >> m_xContentSize.y;
	// Overlay must always be centered — override whatever anchor/pivot was saved
	SetAnchorAndPivot(AnchorPreset::Center);
	SetSize(m_xContentSize.x, m_xContentSize.y);

	xStream >> m_xContentStyle.m_xFillColor.x; xStream >> m_xContentStyle.m_xFillColor.y; xStream >> m_xContentStyle.m_xFillColor.z; xStream >> m_xContentStyle.m_xFillColor.w;
	xStream >> m_xContentStyle.m_fCornerRadius;
	xStream >> m_xContentStyle.m_fBorderThickness;
	xStream >> m_xContentStyle.m_xBorderColor.x; xStream >> m_xContentStyle.m_xBorderColor.y; xStream >> m_xContentStyle.m_xBorderColor.z; xStream >> m_xContentStyle.m_xBorderColor.w;
}

#ifdef ZENITH_TOOLS
void Zenith_UIOverlay::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIOverlayProps");

	ImGui::Separator();
	ImGui::Text("Overlay Properties");

	float fDimCol[4] = {m_xDimColor.x, m_xDimColor.y, m_xDimColor.z, m_xDimColor.w};
	if (ImGui::ColorEdit4("Dim Color", fDimCol))
	{
		m_xDimColor = {fDimCol[0], fDimCol[1], fDimCol[2], fDimCol[3]};
	}

	ImGui::DragFloat("Fade Duration", &m_fFadeDuration, 0.01f, 0.0f, 2.0f);

	float fContentSz[2] = {m_xContentSize.x, m_xContentSize.y};
	if (ImGui::DragFloat2("Content Size", fContentSz, 1.f, 0.f, 4096.f))
	{
		m_xContentSize = {fContentSz[0], fContentSz[1]};
	}

	ImGui::Text("Showing: %s", m_bShowing ? "Yes" : "No");

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
