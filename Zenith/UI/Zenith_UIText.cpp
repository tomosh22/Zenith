#include "Zenith.h"
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

static constexpr uint32_t UI_TEXT_VERSION = 2;

Zenith_UIText::Zenith_UIText(const std::string& strText, const std::string& strName)
    : Zenith_UIElement(strName)
    , m_strText(strText)
{
}

// ========== Word Wrapping ==========

void Zenith_UIText::RebuildWrappedText()
{
    if (m_fMaxWidth <= 0.f || m_strText.empty())
    {
        m_strWrappedText = m_strText;
        return;
    }

    float fCharWidth = m_fFontSize * fCHAR_SPACING;
    m_strWrappedText.clear();
    m_strWrappedText.reserve(m_strText.length() + 10);

    float fLineWidth = 0.f;
    bool bLineHasContent = false;

    size_t i = 0;
    while (i < m_strText.length())
    {
        // Handle explicit newlines
        if (m_strText[i] == '\n')
        {
            m_strWrappedText += '\n';
            fLineWidth = 0.f;
            bLineHasContent = false;
            ++i;
            continue;
        }

        // Find the next word (sequence of non-space, non-newline characters)
        size_t uWordStart = i;
        while (i < m_strText.length() && m_strText[i] != ' ' && m_strText[i] != '\n')
            ++i;

        size_t uWordLen = i - uWordStart;
        float fWordWidth = uWordLen * fCharWidth;

        // Check if adding space + word exceeds max width
        float fNeededWidth = bLineHasContent ? (fCharWidth + fWordWidth) : fWordWidth;

        if (bLineHasContent && fLineWidth + fNeededWidth > m_fMaxWidth)
        {
            // Wrap to new line
            m_strWrappedText += '\n';
            fLineWidth = 0.f;
            bLineHasContent = false;
        }

        if (bLineHasContent)
        {
            m_strWrappedText += ' ';
            fLineWidth += fCharWidth;
        }

        m_strWrappedText.append(m_strText, uWordStart, uWordLen);
        fLineWidth += fWordWidth;
        bLineHasContent = true;

        // Skip spaces after word
        while (i < m_strText.length() && m_strText[i] == ' ')
            ++i;
    }
}

// ========== Text Metrics ==========

float Zenith_UIText::GetTextWidth() const
{
    const std::string& strDisplay = GetDisplayText();
    float fMaxWidth = 0.f;
    float fCurrentWidth = 0.f;
    for (size_t i = 0; i < strDisplay.length(); ++i)
    {
        if (strDisplay[i] == '\n')
        {
            if (fCurrentWidth > fMaxWidth)
                fMaxWidth = fCurrentWidth;
            fCurrentWidth = 0.f;
        }
        else
        {
            fCurrentWidth += m_fFontSize * fCHAR_SPACING;
        }
    }
    if (fCurrentWidth > fMaxWidth)
        fMaxWidth = fCurrentWidth;
    return fMaxWidth;
}

float Zenith_UIText::GetTextHeight() const
{
    const std::string& strDisplay = GetDisplayText();
    uint32_t uLineCount = 1;
    for (size_t i = 0; i < strDisplay.length(); ++i)
    {
        if (strDisplay[i] == '\n')
            ++uLineCount;
    }
    return m_fFontSize * uLineCount;
}

// ========== Rendering ==========

void Zenith_UIText::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible || m_strText.empty())
        return;

    const std::string& strDisplay = GetDisplayText();

    // Get our screen bounds
    Zenith_Maths::Vector4 xBounds = GetScreenBounds();
    float fLeft = xBounds.x;
    float fTop = xBounds.y;
    float fWidth = xBounds.z - xBounds.x;
    float fHeight = xBounds.w - xBounds.y;

    float fTextHeight = GetTextHeight();

    // Vertical alignment within element bounds
    float fStartY = fTop;
    switch (m_eVerticalAlignment)
    {
    case TextVerticalAlignment::Top:
        fStartY = fTop;
        break;
    case TextVerticalAlignment::Middle:
        fStartY = fTop + (fHeight - fTextHeight) * 0.5f;
        break;
    case TextVerticalAlignment::Bottom:
        fStartY = fTop + fHeight - fTextHeight;
        break;
    }

    float fAlpha = GetEffectiveAlpha();
    float fCharWidth = m_fFontSize * fCHAR_SPACING;

    // For Center/Right alignment with multi-line text, render each line separately
    // so each line is independently centered/right-aligned within the element bounds
    bool bMultiLine = strDisplay.find('\n') != std::string::npos;
    if (m_eAlignment != TextAlignment::Left && bMultiLine)
    {
        float fLineY = fStartY;
        size_t uLineStart = 0;

        while (uLineStart <= strDisplay.length())
        {
            size_t uLineEnd = strDisplay.find('\n', uLineStart);
            if (uLineEnd == std::string::npos)
                uLineEnd = strDisplay.length();

            size_t uLineLen = uLineEnd - uLineStart;
            if (uLineLen > 0)
            {
                std::string strLine = strDisplay.substr(uLineStart, uLineLen);
                float fLineWidth = static_cast<float>(uLineLen) * fCharWidth;

                float fLineX = fLeft;
                if (m_eAlignment == TextAlignment::Center)
                    fLineX = fLeft + (fWidth - fLineWidth) * 0.5f;
                else
                    fLineX = fLeft + fWidth - fLineWidth;

                Zenith_Maths::Vector2 xLinePos = { fLineX, fLineY };

                if (m_bShadowEnabled)
                {
                    Zenith_Maths::Vector2 xShadowPos = {
                        xLinePos.x + m_xShadowOffset.x,
                        xLinePos.y + m_xShadowOffset.y
                    };
                    Zenith_Maths::Vector4 xShadowColor = m_xShadowColor;
                    xShadowColor.a *= fAlpha;
                    xCanvas.SubmitText(strLine, xShadowPos, m_fFontSize, xShadowColor);
                }

                Zenith_Maths::Vector4 xTextColor = m_xColor;
                xTextColor.a *= fAlpha;
                xCanvas.SubmitText(strLine, xLinePos, m_fFontSize, xTextColor);
            }

            fLineY += m_fFontSize;
            uLineStart = uLineEnd + 1;
        }
    }
    else
    {
        // Single line or Left alignment: render as a single block
        float fTextWidth = GetTextWidth();
        Zenith_Maths::Vector2 xTextPos = { fLeft, fStartY };

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

        if (m_bShadowEnabled)
        {
            Zenith_Maths::Vector2 xShadowPos = {
                xTextPos.x + m_xShadowOffset.x,
                xTextPos.y + m_xShadowOffset.y
            };
            Zenith_Maths::Vector4 xShadowColor = m_xShadowColor;
            xShadowColor.a *= fAlpha;
            xCanvas.SubmitText(strDisplay, xShadowPos, m_fFontSize, xShadowColor);
        }

        Zenith_Maths::Vector4 xTextColor = m_xColor;
        xTextColor.a *= fAlpha;
        xCanvas.SubmitText(strDisplay, xTextPos, m_fFontSize, xTextColor);
    }

    // Render children (if any)
    Zenith_UIElement::Render(xCanvas);
}

// ========== Serialization ==========

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
    xStream << m_bShadowEnabled;
    xStream << m_xShadowColor.x; xStream << m_xShadowColor.y; xStream << m_xShadowColor.z; xStream << m_xShadowColor.w;
    xStream << m_xShadowOffset.x; xStream << m_xShadowOffset.y;
}

void Zenith_UIText::ReadFromDataStream(Zenith_DataStream& xStream)
{
    // Read base class data
    Zenith_UIElement::ReadFromDataStream(xStream);

    // Read text-specific data
    uint32_t uVersion;
    xStream >> uVersion;

    Zenith_Assert(uVersion == UI_TEXT_VERSION, "UIText version mismatch");

    xStream >> m_strText;
    xStream >> m_fFontSize;

    uint32_t uAlign;
    xStream >> uAlign;
    m_eAlignment = static_cast<TextAlignment>(uAlign);

    xStream >> uAlign;
    m_eVerticalAlignment = static_cast<TextVerticalAlignment>(uAlign);

    xStream >> m_bShadowEnabled;
    xStream >> m_xShadowColor.x; xStream >> m_xShadowColor.y; xStream >> m_xShadowColor.z; xStream >> m_xShadowColor.w;
    xStream >> m_xShadowOffset.x; xStream >> m_xShadowOffset.y;
}

// ========== Editor Properties Panel ==========

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
        RebuildWrappedText();
    }

    if (ImGui::DragFloat("Font Size", &m_fFontSize, 1.0f, 8.0f, 200.0f))
    {
        RebuildWrappedText();
    }

    float fMaxW = m_fMaxWidth;
    if (ImGui::DragFloat("Max Width", &fMaxW, 1.0f, 0.0f, 2000.0f))
    {
        SetMaxWidth(fMaxW);
    }

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
