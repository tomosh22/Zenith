#pragma once

#include "UI/Zenith_UIElement.h"

/**
 * Zenith_UIRect - Styled rectangle widget
 *
 * Renders a styled rectangle using UIStyle. Useful for:
 *   - Health bars
 *   - Progress bars
 *   - Backgrounds
 *   - Borders
 *   - Pill-shaped HUD displays
 *
 * Features:
 *   - Fill amount (0-1) for progress bar functionality
 *   - Fill direction (horizontal/vertical, left-to-right or right-to-left)
 *   - Full UIStyle support (rounded corners, gradient, shadow, border)
 */

namespace Zenith_UI {

enum class FillDirection
{
    LeftToRight,
    RightToLeft,
    BottomToTop,
    TopToBottom
};

class Zenith_UIRect : public Zenith_UIElement
{
public:
    Zenith_UIRect(const std::string& strName = "UIRect");
    virtual ~Zenith_UIRect() = default;

    virtual UIElementType GetType() const override { return UIElementType::Rect; }

    // ========== Fill Properties ==========

    void SetFillAmount(float fAmount) { m_fFillAmount = Zenith_Maths::Clamp(fAmount, 0.0f, 1.0f); }
    float GetFillAmount() const { return m_fFillAmount; }

    void SetFillDirection(FillDirection eDir) { m_eFillDirection = eDir; }
    FillDirection GetFillDirection() const { return m_eFillDirection; }

    // ========== Style ==========

    void SetStyle(const UIStyle& xStyle) { m_xStyle = xStyle; }
    const UIStyle& GetStyle() const { return m_xStyle; }

    void SetCornerRadius(float fRadius) { m_xStyle.m_fCornerRadius = fRadius; }
    void SetBorderColor(const Zenith_Maths::Vector4& xColor) { m_xStyle.m_xBorderColor = xColor; }
    void SetBorderThickness(float fThickness) { m_xStyle.m_fBorderThickness = fThickness; }
    void SetGradientColor(const Zenith_Maths::Vector4& xColor) { m_xStyle.m_xGradientBottomColor = xColor; }
    void SetShadowEnabled(bool bEnabled) { m_xStyle.m_bShadowEnabled = bEnabled; }
    void SetShadowColor(const Zenith_Maths::Vector4& xColor) { m_xStyle.m_xShadowColor = xColor; }
    void SetShadowOffset(const Zenith_Maths::Vector2& xOffset) { m_xStyle.m_xShadowOffset = xOffset; }
    void SetShadowSpread(float fSpread) { m_xStyle.m_fShadowSpread = fSpread; }

    // ========== Overrides ==========

    virtual void Render(Zenith_UICanvas& xCanvas) override;
    virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
    virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
    virtual void RenderPropertiesPanel() override;
#endif

private:
    UIStyle m_xStyle;

    // Fill properties
    float m_fFillAmount = 1.0f;
    FillDirection m_eFillDirection = FillDirection::LeftToRight;
};

} // namespace Zenith_UI
