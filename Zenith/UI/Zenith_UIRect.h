#pragma once

#include "UI/Zenith_UIElement.h"
#include "Maths/Zenith_Maths.h"

/**
 * Zenith_UIRect - Colored rectangle widget
 *
 * Renders a solid colored rectangle. Useful for:
 *   - Health bars
 *   - Progress bars
 *   - Backgrounds
 *   - Borders
 *
 * Features:
 *   - Fill amount (0-1) for progress bar functionality
 *   - Fill direction (horizontal/vertical, left-to-right or right-to-left)
 *   - Border with configurable color and thickness
 *   - Glow effect for highlighting
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

    // Fill amount (0-1, where 1 = fully filled)
    void SetFillAmount(float fAmount) { m_fFillAmount = Zenith_Maths::Clamp(fAmount, 0.0f, 1.0f); }
    float GetFillAmount() const { return m_fFillAmount; }

    void SetFillDirection(FillDirection eDir) { m_eFillDirection = eDir; }
    FillDirection GetFillDirection() const { return m_eFillDirection; }

    // ========== Border Properties ==========

    void SetBorderColor(const Zenith_Maths::Vector4& xColor) { m_xBorderColor = xColor; }
    Zenith_Maths::Vector4 GetBorderColor() const { return m_xBorderColor; }

    void SetBorderThickness(float fThickness) { m_fBorderThickness = fThickness; }
    float GetBorderThickness() const { return m_fBorderThickness; }

    // ========== Glow Effect ==========

    void SetGlowEnabled(bool bEnabled) { m_bGlowEnabled = bEnabled; }
    bool IsGlowEnabled() const { return m_bGlowEnabled; }

    void SetGlowColor(const Zenith_Maths::Vector4& xColor) { m_xGlowColor = xColor; }
    Zenith_Maths::Vector4 GetGlowColor() const { return m_xGlowColor; }

    void SetGlowSize(float fSize) { m_fGlowSize = fSize; }
    float GetGlowSize() const { return m_fGlowSize; }

    // ========== Overrides ==========

    virtual void Render(Zenith_UICanvas& xCanvas) override;
    virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
    virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
    virtual void RenderPropertiesPanel() override;
#endif

private:
    // Fill properties
    float m_fFillAmount = 1.0f;
    FillDirection m_eFillDirection = FillDirection::LeftToRight;

    // Border properties
    Zenith_Maths::Vector4 m_xBorderColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    float m_fBorderThickness = 0.0f;

    // Glow effect
    bool m_bGlowEnabled = false;
    Zenith_Maths::Vector4 m_xGlowColor = { 1.0f, 1.0f, 0.0f, 0.5f };
    float m_fGlowSize = 8.0f;
};

} // namespace Zenith_UI
