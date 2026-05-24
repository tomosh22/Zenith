#include "Zenith.h"
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

    float fCharWidth = m_fFontSize * Zenith_FontAsset::GetActiveOrDefaultMetrics().fEmAdvance;
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
            fCurrentWidth += m_fFontSize * Zenith_FontAsset::GetActiveOrDefaultMetrics().fEmAdvance;
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

float Zenith_UIText::ComputeHorizontalStartX(float fLeft, float fWidth, float fLineWidth, TextAlignment eAlignment)
{
    // Overflow semantics (code-review note): when fLineWidth > fWidth, Center and
    // Right alignment return an X less than fLeft. This is intentional —
    // centred/right-aligned text wider than its container slides past the left
    // edge by design, matching CSS `text-align`, Unity's UGUI Text, and every
    // major UI framework. Clamping here would silently promote Center/Right to
    // Left alignment on overflow, which is a surprising behaviour change rather
    // than a fix. Callers that want hard clipping should configure the canvas
    // clip rect or enable text wrapping; callers that want truncation should
    // do it before measuring fLineWidth.
    switch (eAlignment)
    {
    case TextAlignment::Center: return fLeft + (fWidth - fLineWidth) * 0.5f;
    case TextAlignment::Right:  return fLeft + fWidth - fLineWidth;
    case TextAlignment::Left:
    default:                    return fLeft;
    }
}

float Zenith_UIText::ComputeVerticalStartY(float fTop, float fHeight, float fTextHeight, TextVerticalAlignment eAlignment)
{
    switch (eAlignment)
    {
    case TextVerticalAlignment::Middle: return fTop + (fHeight - fTextHeight) * 0.5f;
    case TextVerticalAlignment::Bottom: return fTop + fHeight - fTextHeight;
    case TextVerticalAlignment::Top:
    default:                            return fTop;
    }
}

void Zenith_UIText::SubmitTextWithShadow(Zenith_UICanvas& xCanvas, const std::string& strText,
                                         const Zenith_Maths::Vector2& xPos, float fAlpha)
{
    if (m_bShadowEnabled)
    {
        Zenith_Maths::Vector2 xShadowPos = { xPos.x + m_xShadowOffset.x, xPos.y + m_xShadowOffset.y };
        Zenith_Maths::Vector4 xShadowColor = m_xShadowColor;
        xShadowColor.a *= fAlpha;
        xCanvas.SubmitText(strText, xShadowPos, m_fFontSize, xShadowColor);
    }

    Zenith_Maths::Vector4 xTextColor = m_xColor;
    xTextColor.a *= fAlpha;
    xCanvas.SubmitText(strText, xPos, m_fFontSize, xTextColor);
}

void Zenith_UIText::RenderMultilineAligned(Zenith_UICanvas& xCanvas, const std::string& strDisplay,
                                           float fLeft, float fWidth, float fStartY, float fAlpha)
{
    const float fCharWidth = m_fFontSize * Zenith_FontAsset::GetActiveOrDefaultMetrics().fEmAdvance;
    float fLineY = fStartY;
    size_t uLineStart = 0;

    while (uLineStart <= strDisplay.length())
    {
        size_t uLineEnd = strDisplay.find('\n', uLineStart);
        if (uLineEnd == std::string::npos) uLineEnd = strDisplay.length();

        const size_t uLineLen = uLineEnd - uLineStart;
        if (uLineLen > 0)
        {
            const std::string strLine = strDisplay.substr(uLineStart, uLineLen);
            const float fLineWidth = static_cast<float>(uLineLen) * fCharWidth;
            const float fLineX = ComputeHorizontalStartX(fLeft, fWidth, fLineWidth, m_eAlignment);
            SubmitTextWithShadow(xCanvas, strLine, { fLineX, fLineY }, fAlpha);
        }

        fLineY += m_fFontSize;
        uLineStart = uLineEnd + 1;
    }
}

void Zenith_UIText::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible || m_strText.empty()) return;

    const std::string& strDisplay = GetDisplayText();

    const Zenith_Maths::Vector4 xBounds = GetScreenBounds();
    const float fLeft = xBounds.x;
    const float fTop = xBounds.y;
    const float fWidth = xBounds.z - xBounds.x;
    const float fHeight = xBounds.w - xBounds.y;

    const float fStartY = ComputeVerticalStartY(fTop, fHeight, GetTextHeight(), m_eVerticalAlignment);
    const float fAlpha = GetEffectiveAlpha();

    // Center/Right alignment with multi-line text renders each line independently so
    // each is aligned within the element bounds. Left alignment or single-line text
    // can submit the display string as one block.
    const bool bMultiLine = strDisplay.find('\n') != std::string::npos;
    const float fTextHeight = GetTextHeight();
    float fTextStartX = 0.0f;
    float fTextEndX = 0.0f;
    if (m_eAlignment != TextAlignment::Left && bMultiLine)
    {
        RenderMultilineAligned(xCanvas, strDisplay, fLeft, fWidth, fStartY, fAlpha);
        // For warning bounds: use the widest line as a worst-case
        // approximation. The actual rendered text may be narrower for
        // shorter lines, but it never exceeds GetTextWidth().
        const float fMaxLineWidth = GetTextWidth();
        const float fAlignedX = ComputeHorizontalStartX(fLeft, fWidth, fMaxLineWidth, m_eAlignment);
        fTextStartX = fAlignedX;
        fTextEndX = fAlignedX + fMaxLineWidth;
    }
    else
    {
        const float fLineWidth = GetTextWidth();
        const float fLineX = ComputeHorizontalStartX(fLeft, fWidth, fLineWidth, m_eAlignment);
        SubmitTextWithShadow(xCanvas, strDisplay, { fLineX, fStartY }, fAlpha);
        fTextStartX = fLineX;
        fTextEndX = fLineX + fLineWidth;
    }

    // Off-screen detection (MVP-UI-polish): if the rendered text extends
    // past any canvas edge, log a once-per-element warning. Catches the
    // "anchored top-right + Left alignment = text flows off the right
    // edge" bug class. Doesn't run the check every frame -- once we've
    // warned for a (name, edge) we don't re-warn until the element name
    // changes, which is essentially never. Use a `static set` per UIText
    // instance is overkill; use a per-instance dirty-bit set on the four
    // edges instead.
    const Zenith_Maths::Vector2 xCanvasSize = xCanvas.GetSize();
    const float fTextTopY = fStartY;
    const float fTextBotY = fStartY + fTextHeight;
    // Allow a tiny epsilon -- floating-point bound checks can otherwise
    // fire on the pixel-perfect-edge case. 0.5 px is well below
    // perceptual relevance.
    constexpr float kEpsilon = 0.5f;
    auto CheckEdge = [&](bool& bWarned, const char* szEdge, float fValue, const char* szRelation, float fLimit)
    {
        if (bWarned) return;
        Zenith_Warning(LOG_CATEGORY_UI,
            "Zenith_UIText '%s' renders past %s edge of canvas: %s=%.1f (limit %.1f). Likely missing TextAlignment for the anchor: TopRight/BottomRight anchors need TextAlignment::Right, *Center anchors need TextAlignment::Center, otherwise the text flows off the screen edge.",
            m_strName.c_str(), szEdge, szRelation, fValue, fLimit);
        bWarned = true;
    };
    if (fTextStartX < -kEpsilon)
    {
        CheckEdge(m_bWarnedOffLeft, "LEFT", fTextStartX, "startX", 0.0f);
    }
    if (fTextEndX > xCanvasSize.x + kEpsilon)
    {
        CheckEdge(m_bWarnedOffRight, "RIGHT", fTextEndX, "endX", xCanvasSize.x);
    }
    if (fTextTopY < -kEpsilon)
    {
        CheckEdge(m_bWarnedOffTop, "TOP", fTextTopY, "topY", 0.0f);
    }
    if (fTextBotY > xCanvasSize.y + kEpsilon)
    {
        CheckEdge(m_bWarnedOffBottom, "BOTTOM", fTextBotY, "bottomY", xCanvasSize.y);
    }

    // Anchor-alignment-mismatch warning: catches the "Center anchor +
    // Left alignment = text appears off-centre" bug class. The text
    // technically fits inside the canvas so the off-edge warning above
    // won't fire, but the visual effect is the same authoring bug:
    // the player sees text drifting toward one edge.
    //
    // Mismatch rules (anchor X position in normalized 0..1 space):
    //   anchor.x near 1.0 (right-anchored) -> expect Right alignment
    //   anchor.x near 0.5 (center-anchored) -> expect Center alignment
    //   anchor.x near 0.0 (left-anchored)   -> expect Left alignment
    // Tolerance 0.15 on the anchor value so TopCenter (0.5, 0) and
    // BottomCenter (0.5, 1) both count as centered for this check.
    if (!m_bWarnedAlignmentMismatch && fWidth < 1.0f) // only when size is 0 (anchor-driven)
    {
        const float fAnchorX = m_xAnchor.x;
        const char* szExpected = nullptr;
        if (fAnchorX > 0.85f && m_eAlignment != TextAlignment::Right)
        {
            szExpected = "Right";
        }
        else if (fAnchorX > 0.35f && fAnchorX < 0.65f && m_eAlignment != TextAlignment::Center)
        {
            szExpected = "Center";
        }
        else if (fAnchorX < 0.15f && m_eAlignment != TextAlignment::Left)
        {
            szExpected = "Left";
        }
        if (szExpected != nullptr)
        {
            const char* szActual = "Left";
            if (m_eAlignment == TextAlignment::Center) szActual = "Center";
            else if (m_eAlignment == TextAlignment::Right) szActual = "Right";
            Zenith_Warning(LOG_CATEGORY_UI,
                "Zenith_UIText '%s' has anchor.x=%.2f but TextAlignment::%s -- expected TextAlignment::%s for this anchor. Text will visually drift toward one edge instead of being aligned to the anchor.",
                m_strName.c_str(), fAnchorX, szActual, szExpected);
            m_bWarnedAlignmentMismatch = true;
        }
    }

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
