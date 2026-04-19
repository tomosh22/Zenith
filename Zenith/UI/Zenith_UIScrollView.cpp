#include "Zenith.h"
#include "UI/Zenith_UIScrollView.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Input/Zenith_Input.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_SCROLLVIEW_VERSION = 1;

Zenith_UIScrollView::Zenith_UIScrollView(const std::string& strName)
	: Zenith_UIElement(strName)
{
}

void Zenith_UIScrollView::SetScrollPosition(float fX, float fY)
{
	m_xScrollPosition = {fX, fY};
	ClampScrollPosition();
}

void Zenith_UIScrollView::ClampScrollPosition()
{
	Zenith_Maths::Vector2 xViewSize = GetSize();

	float fMaxX = glm::max(m_xContentSize.x - xViewSize.x, 0.f);
	float fMaxY = glm::max(m_xContentSize.y - xViewSize.y, 0.f);

	if (m_eScrollDirection == ScrollDirection::VERTICAL || m_eScrollDirection == ScrollDirection::BOTH)
	{
		m_xScrollPosition.y = glm::clamp(m_xScrollPosition.y, 0.f, fMaxY);
	}
	else
	{
		m_xScrollPosition.y = 0.f;
	}

	if (m_eScrollDirection == ScrollDirection::HORIZONTAL || m_eScrollDirection == ScrollDirection::BOTH)
	{
		m_xScrollPosition.x = glm::clamp(m_xScrollPosition.x, 0.f, fMaxX);
	}
	else
	{
		m_xScrollPosition.x = 0.f;
	}
}

void Zenith_UIScrollView::GetTransformedMousePosition(float& fMouseX, float& fMouseY) const
{
	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_Input::GetMousePosition(xMousePos);
	fMouseX = static_cast<float>(xMousePos.x);
	fMouseY = static_cast<float>(xMousePos.y);

#ifdef ZENITH_TOOLS
#ifdef ZENITH_INPUT_SIMULATOR
	if (!Zenith_InputSimulator::IsEnabled())
#endif
	{
		Zenith_Maths::Vector2 xViewportPos = Zenith_Editor::GetViewportPos();
		Zenith_Maths::Vector2 xViewportSize = Zenith_Editor::GetViewportSize();
		if (xViewportSize.x > 0.f && xViewportSize.y > 0.f && m_pxCanvas)
		{
			Zenith_Maths::Vector2 xCanvasSize = m_pxCanvas->GetSize();
			fMouseX = (fMouseX - xViewportPos.x) * (xCanvasSize.x / xViewportSize.x);
			fMouseY = (fMouseY - xViewportPos.y) * (xCanvasSize.y / xViewportSize.y);
		}
	}
#endif
}

void Zenith_UIScrollView::HandleDragInput(float fMouseX, float fMouseY, bool bInside, float fDt)
{
	bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);

	if (bMouseDown && !m_bMouseDownLastFrame && bInside)
	{
		m_bDragging = true;
		m_xDragStart = {fMouseX, fMouseY};
		m_xScrollStart = m_xScrollPosition;
		m_xScrollVelocity = {0.f, 0.f};
	}

	if (m_bDragging && bMouseDown)
	{
		float fDX = m_xDragStart.x - fMouseX;
		float fDY = m_xDragStart.y - fMouseY;

		Zenith_Maths::Vector2 xNewScroll = m_xScrollStart;
		if (m_eScrollDirection == ScrollDirection::HORIZONTAL || m_eScrollDirection == ScrollDirection::BOTH)
		{
			xNewScroll.x += fDX;
		}
		if (m_eScrollDirection == ScrollDirection::VERTICAL || m_eScrollDirection == ScrollDirection::BOTH)
		{
			xNewScroll.y += fDY;
		}

		Zenith_Maths::Vector2 xOldPos = m_xScrollPosition;
		m_xScrollPosition = xNewScroll;
		ClampScrollPosition();

		if (fDt > 0.f)
		{
			m_xScrollVelocity = {
				(m_xScrollPosition.x - xOldPos.x) / fDt,
				(m_xScrollPosition.y - xOldPos.y) / fDt
			};
		}
	}

	if (!bMouseDown && m_bDragging)
	{
		m_bDragging = false;
	}

	m_bMouseDownLastFrame = bMouseDown;
}

void Zenith_UIScrollView::UpdateInertia(float fDt)
{
	if (m_bDragging || !m_bInertia)
		return;

	float fVelMag = m_xScrollVelocity.x * m_xScrollVelocity.x + m_xScrollVelocity.y * m_xScrollVelocity.y;
	if (fVelMag > 0.1f)
	{
		m_xScrollPosition.x += m_xScrollVelocity.x * fDt;
		m_xScrollPosition.y += m_xScrollVelocity.y * fDt;
		ClampScrollPosition();

		float fDecel = glm::pow(m_fDecelerationRate, fDt);
		m_xScrollVelocity.x *= fDecel;
		m_xScrollVelocity.y *= fDecel;
	}
	else
	{
		m_xScrollVelocity = {0.f, 0.f};
	}
}

void Zenith_UIScrollView::Update(float fDt)
{
	if (!m_bVisible)
		return;

	if (IsGroupInteractable())
	{
		float fMouseX;
		float fMouseY;
		GetTransformedMousePosition(fMouseX, fMouseY);

		Zenith_Maths::Vector4 xBounds = GetScreenBounds();
		bool bInside = fMouseX >= xBounds.x && fMouseX <= xBounds.z
			&& fMouseY >= xBounds.y && fMouseY <= xBounds.w;

		HandleDragInput(fMouseX, fMouseY, bInside, fDt);
	}

	UpdateInertia(fDt);

	Zenith_UIElement::Update(fDt);
}

void Zenith_UIScrollView::Render(Zenith_UICanvas& xCanvas)
{
	if (!m_bVisible)
		return;

	float fAlpha = GetEffectiveAlpha();
	Zenith_Maths::Vector4 xBounds = GetScreenBounds();

	// Render background if enabled
	if (m_bHasBackground)
	{
		UIStyleRenderer::RenderStyledRect(xCanvas, m_xBackgroundStyle, xBounds, fAlpha);
	}

	// Push clip rect for children
	xCanvas.PushClipRect(xBounds);

	// Render children with scroll offset
	const Zenith_Vector<Zenith_UIElement*>& xChildren = GetChildren();
	for (uint32_t u = 0; u < xChildren.GetSize(); u++)
	{
		Zenith_UIElement* pxChild = xChildren.Get(u);
		if (pxChild && pxChild->IsVisible())
		{
			// Temporarily offset child position by scroll amount
			Zenith_Maths::Vector2 xOrigPos = pxChild->GetPosition();
			pxChild->SetPosition(xOrigPos.x - m_xScrollPosition.x, xOrigPos.y - m_xScrollPosition.y);
			pxChild->Render(xCanvas);
			pxChild->SetPosition(xOrigPos);
		}
	}

	xCanvas.PopClipRect();
}

void Zenith_UIScrollView::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_SCROLLVIEW_VERSION;
	xStream << m_xContentSize.x; xStream << m_xContentSize.y;
	xStream << static_cast<uint32_t>(m_eScrollDirection);
	xStream << m_bInertia;
	xStream << m_fDecelerationRate;
}

void Zenith_UIScrollView::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion <= UI_SCROLLVIEW_VERSION, "UIScrollView version mismatch");

	xStream >> m_xContentSize.x; xStream >> m_xContentSize.y;
	uint32_t uDir;
	xStream >> uDir;
	m_eScrollDirection = static_cast<ScrollDirection>(uDir);
	xStream >> m_bInertia;
	xStream >> m_fDecelerationRate;
}

#ifdef ZENITH_TOOLS
void Zenith_UIScrollView::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIScrollViewProps");

	ImGui::Separator();
	ImGui::Text("Scroll View Properties");

	float fContentSz[2] = {m_xContentSize.x, m_xContentSize.y};
	if (ImGui::DragFloat2("Content Size", fContentSz, 1.f, 0.f, 10000.f))
	{
		m_xContentSize = {fContentSz[0], fContentSz[1]};
	}

	const char* szDirs[] = {"Vertical", "Horizontal", "Both"};
	int iDir = static_cast<int>(m_eScrollDirection);
	if (ImGui::Combo("Scroll Direction", &iDir, szDirs, 3))
	{
		m_eScrollDirection = static_cast<ScrollDirection>(iDir);
	}

	ImGui::Checkbox("Inertia", &m_bInertia);
	ImGui::DragFloat("Deceleration Rate", &m_fDecelerationRate, 0.01f, 0.001f, 1.0f);

	float fScroll[2] = {m_xScrollPosition.x, m_xScrollPosition.y};
	ImGui::Text("Scroll: (%.1f, %.1f)", fScroll[0], fScroll[1]);

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
