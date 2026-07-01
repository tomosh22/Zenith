#include "Zenith.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_RECT_VERSION = 2;

Zenith_UIRect::Zenith_UIRect(const std::string& strName)
    : Zenith_UIElement(strName)
{
}

void Zenith_UIRect::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible)
        return;

    float fAlpha = GetEffectiveAlpha();
    Zenith_Maths::Vector4 xBounds = GetScreenBounds();

    // Apply fill color from element color
    UIStyle xRenderStyle = m_xStyle;
    xRenderStyle.m_xFillColor = m_xColor;

    // Handle fill amount — adjust bounds before rendering
    if (m_fFillAmount < 1.0f && m_fFillAmount > 0.0f)
    {
        float fWidth = xBounds.z - xBounds.x;
        float fHeight = xBounds.w - xBounds.y;

        Zenith_Maths::Vector4 xFillBounds = xBounds;
        switch (m_eFillDirection)
        {
        case FillDirection::LeftToRight:
            xFillBounds.z = xBounds.x + (fWidth * m_fFillAmount);
            break;
        case FillDirection::RightToLeft:
            xFillBounds.x = xBounds.z - (fWidth * m_fFillAmount);
            break;
        case FillDirection::BottomToTop:
            xFillBounds.y = xBounds.w - (fHeight * m_fFillAmount);
            break;
        case FillDirection::TopToBottom:
            xFillBounds.w = xBounds.y + (fHeight * m_fFillAmount);
            break;
        }

        UIStyleRenderer::RenderStyledRect(xCanvas, xRenderStyle, xFillBounds, fAlpha);
    }
    else if (m_fFillAmount >= 1.0f)
    {
        UIStyleRenderer::RenderStyledRect(xCanvas, xRenderStyle, xBounds, fAlpha);
    }

    // Render children
    Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIRect::WriteToDataStream(Zenith_DataStream& xStream) const
{
    Zenith_UIElement::WriteToDataStream(xStream);

    xStream << UI_RECT_VERSION;
    xStream << m_fFillAmount;
    xStream << static_cast<uint32_t>(m_eFillDirection);

    // UIStyle fields
    xStream << m_xStyle.m_xFillColor.x; xStream << m_xStyle.m_xFillColor.y; xStream << m_xStyle.m_xFillColor.z; xStream << m_xStyle.m_xFillColor.w;
    xStream << m_xStyle.m_xGradientBottomColor.x; xStream << m_xStyle.m_xGradientBottomColor.y; xStream << m_xStyle.m_xGradientBottomColor.z; xStream << m_xStyle.m_xGradientBottomColor.w;
    xStream << m_xStyle.m_xBorderColor.x; xStream << m_xStyle.m_xBorderColor.y; xStream << m_xStyle.m_xBorderColor.z; xStream << m_xStyle.m_xBorderColor.w;
    xStream << m_xStyle.m_fBorderThickness;
    xStream << m_xStyle.m_fCornerRadius;
    xStream << m_xStyle.m_bShadowEnabled;
    xStream << m_xStyle.m_xShadowColor.x; xStream << m_xStyle.m_xShadowColor.y; xStream << m_xStyle.m_xShadowColor.z; xStream << m_xStyle.m_xShadowColor.w;
    xStream << m_xStyle.m_xShadowOffset.x; xStream << m_xStyle.m_xShadowOffset.y;
    xStream << m_xStyle.m_fShadowSpread;
}

void Zenith_UIRect::ReadFromDataStream(Zenith_DataStream& xStream)
{
    Zenith_UIElement::ReadFromDataStream(xStream);

    uint32_t uVersion;
    xStream >> uVersion;

    Zenith_Assert(uVersion == UI_RECT_VERSION, "UIRect version mismatch");

    xStream >> m_fFillAmount;
    uint32_t uDir;
    xStream >> uDir;
    m_eFillDirection = static_cast<FillDirection>(uDir);

    xStream >> m_xStyle.m_xFillColor.x; xStream >> m_xStyle.m_xFillColor.y; xStream >> m_xStyle.m_xFillColor.z; xStream >> m_xStyle.m_xFillColor.w;
    xStream >> m_xStyle.m_xGradientBottomColor.x; xStream >> m_xStyle.m_xGradientBottomColor.y; xStream >> m_xStyle.m_xGradientBottomColor.z; xStream >> m_xStyle.m_xGradientBottomColor.w;
    xStream >> m_xStyle.m_xBorderColor.x; xStream >> m_xStyle.m_xBorderColor.y; xStream >> m_xStyle.m_xBorderColor.z; xStream >> m_xStyle.m_xBorderColor.w;
    xStream >> m_xStyle.m_fBorderThickness;
    xStream >> m_xStyle.m_fCornerRadius;
    xStream >> m_xStyle.m_bShadowEnabled;
    xStream >> m_xStyle.m_xShadowColor.x; xStream >> m_xStyle.m_xShadowColor.y; xStream >> m_xStyle.m_xShadowColor.z; xStream >> m_xStyle.m_xShadowColor.w;
    xStream >> m_xStyle.m_xShadowOffset.x; xStream >> m_xStyle.m_xShadowOffset.y;
    xStream >> m_xStyle.m_fShadowSpread;
}

#ifdef ZENITH_TOOLS
void Zenith_UIRect::RenderPropertiesPanel()
{
    Zenith_UIElement::RenderPropertiesPanel();

    ImGui::Separator();
    ImGui::Text("Rect Properties");

    ImGui::SliderFloat("Fill Amount", &m_fFillAmount, 0.0f, 1.0f);

    const char* szDirections[] = { "Left to Right", "Right to Left", "Bottom to Top", "Top to Bottom" };
    int iDir = static_cast<int>(m_eFillDirection);
    if (ImGui::Combo("Fill Direction", &iDir, szDirections, 4))
    {
        m_eFillDirection = static_cast<FillDirection>(iDir);
    }

    ImGui::Separator();
    ImGui::Text("Style");

    ImGui::DragFloat("Corner Radius", &m_xStyle.m_fCornerRadius, 0.5f, 0.0f, 100.0f);
    ImGui::DragFloat("Border Thickness", &m_xStyle.m_fBorderThickness, 0.5f, 0.0f, 50.0f);

    float fBorderColor[4] = { m_xStyle.m_xBorderColor.x, m_xStyle.m_xBorderColor.y, m_xStyle.m_xBorderColor.z, m_xStyle.m_xBorderColor.w };
    if (ImGui::ColorEdit4("Border Color", fBorderColor))
    {
        m_xStyle.m_xBorderColor = { fBorderColor[0], fBorderColor[1], fBorderColor[2], fBorderColor[3] };
    }

    ImGui::Separator();
    ImGui::Text("Shadow");

    ImGui::Checkbox("Enable Shadow", &m_xStyle.m_bShadowEnabled);
    if (m_xStyle.m_bShadowEnabled)
    {
        float fShadowColor[4] = { m_xStyle.m_xShadowColor.x, m_xStyle.m_xShadowColor.y, m_xStyle.m_xShadowColor.z, m_xStyle.m_xShadowColor.w };
        if (ImGui::ColorEdit4("Shadow Color", fShadowColor))
        {
            m_xStyle.m_xShadowColor = { fShadowColor[0], fShadowColor[1], fShadowColor[2], fShadowColor[3] };
        }
        ImGui::DragFloat2("Shadow Offset", &m_xStyle.m_xShadowOffset.x, 0.5f, -50.0f, 50.0f);
        ImGui::DragFloat("Shadow Spread", &m_xStyle.m_fShadowSpread, 0.5f, 0.0f, 50.0f);
    }
}
#endif

} // namespace Zenith_UI
