#include "Zenith.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UICanvas.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_TEXT_VERSION = 1;

// Character width as fraction of height (typical monospace ratio is ~0.5-0.6)
// Must match CHAR_ASPECT_RATIO in Flux_Text.vert and Flux_Text.cpp
static constexpr float CHAR_ASPECT_RATIO = 0.5f;

// Character spacing includes a small gap (10% of char width) for natural appearance
// Must match CHAR_SPACING in Flux_Text.cpp
static constexpr float CHAR_SPACING = CHAR_ASPECT_RATIO * 1.1f;

Zenith_UIText::Zenith_UIText(const std::string& strText, const std::string& strName)
    : Zenith_UIElement(strName)
    , m_strText(strText)
{
}

void Zenith_UIText::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible || m_strText.empty())
        return;

    // Get our screen bounds
    Zenith_Maths::Vector4 xBounds = GetScreenBounds();
    float fLeft = xBounds.x;
    float fTop = xBounds.y;
    float fWidth = xBounds.z - xBounds.x;
    float fHeight = xBounds.w - xBounds.y;

    // Character spacing matches CHAR_SPACING used in shader (includes small gap for natural look)
    float fCharWidth = m_fFontSize * CHAR_SPACING;
    float fTextWidth = m_strText.length() * fCharWidth;
    float fTextHeight = m_fFontSize;

    Zenith_Maths::Vector2 xTextPos = { fLeft, fTop };

    // Horizontal alignment within element bounds
    switch (m_eAlignment)
    {
    case TextAlignment::Left:
        xTextPos.x = fLeft;
        break;
    case TextAlignment::Center:
        xTextPos.x = fLeft + (fWidth - fTextWidth) * 0.5f;
        break;
    case TextAlignment::Right:
        xTextPos.x = fLeft + fWidth - fTextWidth;
        break;
    }

    // Vertical alignment within element bounds
    switch (m_eVerticalAlignment)
    {
    case TextVerticalAlignment::Top:
        xTextPos.y = fTop;
        break;
    case TextVerticalAlignment::Middle:
        xTextPos.y = fTop + (fHeight - fTextHeight) * 0.5f;
        break;
    case TextVerticalAlignment::Bottom:
        xTextPos.y = fTop + fHeight - fTextHeight;
        break;
    }

    // Submit text to canvas for rendering (uses Flux_Text)
    xCanvas.SubmitText(m_strText, xTextPos, m_fFontSize, m_xColor);

    // Render children (if any)
    Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIText::WriteToDataStream(Zenith_DataStream& xStream) const
{
    // Write base class data
    Zenith_UIElement::WriteToDataStream(xStream);

    // Write text-specific data
    xStream << UI_TEXT_VERSION;
    xStream << m_strText;
    xStream << m_fFontSize;
    xStream << static_cast<uint32_t>(m_eAlignment);
    xStream << static_cast<uint32_t>(m_eVerticalAlignment);
}

void Zenith_UIText::ReadFromDataStream(Zenith_DataStream& xStream)
{
    // Read base class data
    Zenith_UIElement::ReadFromDataStream(xStream);

    // Read text-specific data
    uint32_t uVersion;
    xStream >> uVersion;

    xStream >> m_strText;
    xStream >> m_fFontSize;

    uint32_t uAlign;
    xStream >> uAlign;
    m_eAlignment = static_cast<TextAlignment>(uAlign);

    xStream >> uAlign;
    m_eVerticalAlignment = static_cast<TextVerticalAlignment>(uAlign);
}

#ifdef ZENITH_TOOLS
void Zenith_UIText::RenderPropertiesPanel()
{
    // Render base properties
    Zenith_UIElement::RenderPropertiesPanel();

    // Push unique ID scope for text properties
    ImGui::PushID("UITextProps");

    ImGui::Separator();
    ImGui::Text("Text Element Properties");

    // Text content with multi-line support
    char szTextBuffer[1024];
    strncpy_s(szTextBuffer, m_strText.c_str(), sizeof(szTextBuffer) - 1);
    if (ImGui::InputTextMultiline("Content", szTextBuffer, sizeof(szTextBuffer), ImVec2(-1, 60)))
    {
        m_strText = szTextBuffer;
    }

    ImGui::DragFloat("Font Size", &m_fFontSize, 1.0f, 8.0f, 200.0f);

    const char* szAlignments[] = { "Left", "Center", "Right" };
    int iAlign = static_cast<int>(m_eAlignment);
    if (ImGui::Combo("H Align", &iAlign, szAlignments, 3))
    {
        m_eAlignment = static_cast<TextAlignment>(iAlign);
    }

    const char* szVAlignments[] = { "Top", "Middle", "Bottom" };
    int iVAlign = static_cast<int>(m_eVerticalAlignment);
    if (ImGui::Combo("V Align", &iVAlign, szVAlignments, 3))
    {
        m_eVerticalAlignment = static_cast<TextVerticalAlignment>(iVAlign);
    }

    ImGui::PopID();
}
#endif

} // namespace Zenith_UI
