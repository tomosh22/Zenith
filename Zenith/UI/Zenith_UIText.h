#pragma once

#include "UI/Zenith_UIElement.h"
#include <string>

/**
 * Zenith_UIText - Text widget for UI
 *
 * Renders text at a specified position using the Flux_Text system.
 * Supports color, size, alignment, and word wrapping options.
 *
 * Note: Text metrics (GetTextWidth/GetTextHeight) assume a monospace font.
 * The Flux_Text system uses a fixed-advance SDF font atlas. Proportional
 * fonts are not supported.
 */

namespace Zenith_UI {

enum class TextAlignment
{
    Left,
    Center,
    Right
};

enum class TextVerticalAlignment
{
    Top,
    Middle,
    Bottom
};

class Zenith_UIText : public Zenith_UIElement
{
public:
    Zenith_UIText(const std::string& strText = "", const std::string& strName = "UIText");
    virtual ~Zenith_UIText() = default;

    virtual UIElementType GetType() const override { return UIElementType::Text; }

    // ========== Text Content ==========

    void SetText(const std::string& strText) { m_strText = strText; RebuildWrappedText(); }
    const std::string& GetText() const { return m_strText; }

    // ========== Text Appearance ==========

    void SetFontSize(float fSize) { m_fFontSize = fSize; RebuildWrappedText(); }
    float GetFontSize() const { return m_fFontSize; }

    // ========== Text Layout ==========

    void SetAlignment(TextAlignment eAlign) { m_eAlignment = eAlign; }
    TextAlignment GetAlignment() const { return m_eAlignment; }

    void SetVerticalAlignment(TextVerticalAlignment eAlign) { m_eVerticalAlignment = eAlign; }
    TextVerticalAlignment GetVerticalAlignment() const { return m_eVerticalAlignment; }

    void SetMaxWidth(float fMaxWidth) { m_fMaxWidth = fMaxWidth; RebuildWrappedText(); }
    float GetMaxWidth() const { return m_fMaxWidth; }

    // ========== Text Shadow ==========

    void SetTextShadow(bool bEnabled, const Zenith_Maths::Vector4& xColor = {0,0,0,0.5f}, const Zenith_Maths::Vector2& xOffset = {2,2})
    {
        m_bShadowEnabled = bEnabled;
        m_xShadowColor = xColor;
        m_xShadowOffset = xOffset;
    }

    void SetShadowEnabled(bool bEnabled) { m_bShadowEnabled = bEnabled; }
    void SetShadowColor(const Zenith_Maths::Vector4& xColor) { m_xShadowColor = xColor; }
    void SetShadowOffset(const Zenith_Maths::Vector2& xOffset) { m_xShadowOffset = xOffset; }

    // ========== Text Metrics ==========

    float GetTextWidth() const;
    float GetTextHeight() const;

    // ========== Alignment Helpers (static, testable) ==========

    // Resolve horizontal start X given left edge, element width, line width, and alignment.
    static float ComputeHorizontalStartX(float fLeft, float fWidth, float fLineWidth, TextAlignment eAlignment);
    // Resolve vertical start Y given top edge, element height, text height, and alignment.
    static float ComputeVerticalStartY(float fTop, float fHeight, float fTextHeight, TextVerticalAlignment eAlignment);

    // ========== Overrides ==========

    virtual void Render(Zenith_UICanvas& xCanvas) override;
    virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
    virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
    virtual void RenderPropertiesPanel() override;
#endif

private:
    void RebuildWrappedText();
    const std::string& GetDisplayText() const { return m_fMaxWidth > 0.f ? m_strWrappedText : m_strText; }

    // Submit text + optional shadow at the given position (shadow drawn behind).
    void SubmitTextWithShadow(Zenith_UICanvas& xCanvas, const std::string& strText,
                              const Zenith_Maths::Vector2& xPos, float fAlpha);
    // Render each newline-separated line independently aligned within [fLeft, fLeft+fWidth].
    void RenderMultilineAligned(Zenith_UICanvas& xCanvas, const std::string& strDisplay,
                                float fLeft, float fWidth, float fStartY, float fAlpha);

    std::string m_strText;
    std::string m_strWrappedText;
    float m_fFontSize = 24.0f;
    float m_fMaxWidth = 0.f;
    TextAlignment m_eAlignment = TextAlignment::Left;
    TextVerticalAlignment m_eVerticalAlignment = TextVerticalAlignment::Top;

    // Shadow
    bool m_bShadowEnabled = false;
    Zenith_Maths::Vector4 m_xShadowColor = {0.0f, 0.0f, 0.0f, 0.5f};
    Zenith_Maths::Vector2 m_xShadowOffset = {2.0f, 2.0f};

    // Off-screen warning state -- one bool per edge. Render() logs a
    // Zenith_Warning the FIRST time the rendered text extends past
    // that edge of the canvas, then flips the bool true to suppress
    // future warnings for the same edge on the same element. The warning
    // catches the "anchored TopRight + default Left alignment = text
    // flows off the right edge" class of authoring bugs.
    mutable bool m_bWarnedOffLeft   = false;
    mutable bool m_bWarnedOffRight  = false;
    mutable bool m_bWarnedOffTop    = false;
    mutable bool m_bWarnedOffBottom = false;
    // Alignment-anchor-mismatch warning: catches the "Center anchor +
    // Left alignment = text appears off-centre" bug where text fits
    // inside the canvas but is visually offset toward one edge.
    mutable bool m_bWarnedAlignmentMismatch = false;
};

} // namespace Zenith_UI
