#pragma once

#include "UI/Zenith_UIElement.h"
#include "Maths/Zenith_Maths.h"
#include <string>

/**
 * Zenith_UIText - Text widget for UI
 *
 * Renders text at a specified position using the Flux_Text system.
 * Supports color, size, and alignment options.
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

    void SetText(const std::string& strText) { m_strText = strText; }
    const std::string& GetText() const { return m_strText; }

    // ========== Text Appearance ==========

    void SetFontSize(float fSize) { m_fFontSize = fSize; }
    float GetFontSize() const { return m_fFontSize; }

    // ========== Text Layout ==========

    void SetAlignment(TextAlignment eAlign) { m_eAlignment = eAlign; }
    TextAlignment GetAlignment() const { return m_eAlignment; }

    void SetVerticalAlignment(TextVerticalAlignment eAlign) { m_eVerticalAlignment = eAlign; }
    TextVerticalAlignment GetVerticalAlignment() const { return m_eVerticalAlignment; }

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
    float GetTextHeight() const { return m_fFontSize; }

    // ========== Overrides ==========

    virtual void Render(Zenith_UICanvas& xCanvas) override;
    virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
    virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
    virtual void RenderPropertiesPanel() override;
#endif

private:
    std::string m_strText;
    float m_fFontSize = 24.0f;
    TextAlignment m_eAlignment = TextAlignment::Left;
    TextVerticalAlignment m_eVerticalAlignment = TextVerticalAlignment::Top;

    // Shadow
    bool m_bShadowEnabled = false;
    Zenith_Maths::Vector4 m_xShadowColor = {0.0f, 0.0f, 0.0f, 0.5f};
    Zenith_Maths::Vector2 m_xShadowOffset = {2.0f, 2.0f};
};

} // namespace Zenith_UI
