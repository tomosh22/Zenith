#include "Zenith.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UICanvas.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_RECT_VERSION = 1;

Zenith_UIRect::Zenith_UIRect(const std::string& strName)
    : Zenith_UIElement(strName)
{
}

void Zenith_UIRect::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible)
        return;

    Zenith_Maths::Vector4 xBounds = GetScreenBounds();

    // Render glow effect first (behind main rect)
    if (m_bGlowEnabled && m_fGlowSize > 0.0f)
    {
        Zenith_Maths::Vector4 xGlowBounds = {
            xBounds.x - m_fGlowSize,
            xBounds.y - m_fGlowSize,
            xBounds.z + m_fGlowSize,
            xBounds.w + m_fGlowSize
        };
        xCanvas.SubmitQuad(xGlowBounds, m_xGlowColor, 0);
    }

    // Render border (if any)
    if (m_fBorderThickness > 0.0f)
    {
        xCanvas.SubmitQuad(xBounds, m_xBorderColor, 0);

        // Inset for the fill area
        xBounds.x += m_fBorderThickness;
        xBounds.y += m_fBorderThickness;
        xBounds.z -= m_fBorderThickness;
        xBounds.w -= m_fBorderThickness;
    }

    // Render the fill rect based on fill amount and direction
    if (m_fFillAmount > 0.0f)
    {
        Zenith_Maths::Vector4 xFillBounds = xBounds;
        float fWidth = xBounds.z - xBounds.x;
        float fHeight = xBounds.w - xBounds.y;

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

        xCanvas.SubmitQuad(xFillBounds, m_xColor, 0);
    }

    // Render children
    Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIRect::WriteToDataStream(Zenith_DataStream& xStream) const
{
    // Write base class data
    Zenith_UIElement::WriteToDataStream(xStream);

    // Write rect-specific data
    xStream << UI_RECT_VERSION;
    xStream << m_fFillAmount;
    xStream << static_cast<uint32_t>(m_eFillDirection);
    xStream << m_xBorderColor.x;
    xStream << m_xBorderColor.y;
    xStream << m_xBorderColor.z;
    xStream << m_xBorderColor.w;
    xStream << m_fBorderThickness;
    xStream << m_bGlowEnabled;
    xStream << m_xGlowColor.x;
    xStream << m_xGlowColor.y;
    xStream << m_xGlowColor.z;
    xStream << m_xGlowColor.w;
    xStream << m_fGlowSize;
}

void Zenith_UIRect::ReadFromDataStream(Zenith_DataStream& xStream)
{
    // Read base class data
    Zenith_UIElement::ReadFromDataStream(xStream);

    // Read rect-specific data
    uint32_t uVersion;
    xStream >> uVersion;

    xStream >> m_fFillAmount;
    uint32_t uDir;
    xStream >> uDir;
    m_eFillDirection = static_cast<FillDirection>(uDir);
    xStream >> m_xBorderColor.x;
    xStream >> m_xBorderColor.y;
    xStream >> m_xBorderColor.z;
    xStream >> m_xBorderColor.w;
    xStream >> m_fBorderThickness;
    xStream >> m_bGlowEnabled;
    xStream >> m_xGlowColor.x;
    xStream >> m_xGlowColor.y;
    xStream >> m_xGlowColor.z;
    xStream >> m_xGlowColor.w;
    xStream >> m_fGlowSize;
}

#ifdef ZENITH_TOOLS
void Zenith_UIRect::RenderPropertiesPanel()
{
    // Render base properties
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
    ImGui::Text("Border");

    ImGui::DragFloat("Border Thickness", &m_fBorderThickness, 0.5f, 0.0f, 50.0f);

    float fBorderColor[4] = { m_xBorderColor.x, m_xBorderColor.y, m_xBorderColor.z, m_xBorderColor.w };
    if (ImGui::ColorEdit4("Border Color", fBorderColor))
    {
        m_xBorderColor = { fBorderColor[0], fBorderColor[1], fBorderColor[2], fBorderColor[3] };
    }

    ImGui::Separator();
    ImGui::Text("Glow Effect");

    ImGui::Checkbox("Enable Glow", &m_bGlowEnabled);

    if (m_bGlowEnabled)
    {
        ImGui::DragFloat("Glow Size", &m_fGlowSize, 0.5f, 0.0f, 50.0f);

        float fGlowColor[4] = { m_xGlowColor.x, m_xGlowColor.y, m_xGlowColor.z, m_xGlowColor.w };
        if (ImGui::ColorEdit4("Glow Color", fGlowColor))
        {
            m_xGlowColor = { fGlowColor[0], fGlowColor[1], fGlowColor[2], fGlowColor[3] };
        }
    }
}
#endif

} // namespace Zenith_UI
