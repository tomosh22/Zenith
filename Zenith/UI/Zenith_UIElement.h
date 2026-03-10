#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "UI/Zenith_UIStyle.h"
#include "UI/Zenith_UITween.h"
#include <string>

class Zenith_DataStream;

/**
 * Zenith_UIElement - Base class for all UI elements
 *
 * The UI system uses screen-space coordinates where:
 *   - Origin (0,0) is top-left of the screen
 *   - X increases to the right
 *   - Y increases downward
 *   - Units are in pixels
 *
 * Anchoring system:
 *   - Anchor defines which point on the parent the element is positioned relative to
 *   - Pivot defines which point on this element is placed at the anchor position
 *   - Both use normalized coordinates (0-1)
 */

namespace Zenith_UI {

// UI Element types for serialization
enum class UIElementType : uint32_t
{
    Base = 0,
    Text,
    Rect,
    Image,
    Button,
    LayoutGroup,
    Toggle,
    Overlay,
    ScrollView,
    COUNT
};

// Anchor presets for common positioning
enum class AnchorPreset
{
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    StretchAll
};

// Helper to convert preset to anchor/pivot values
inline Zenith_Maths::Vector2 AnchorPresetToValue(AnchorPreset ePreset)
{
    switch (ePreset)
    {
    case AnchorPreset::TopLeft:       return { 0.0f, 0.0f };
    case AnchorPreset::TopCenter:     return { 0.5f, 0.0f };
    case AnchorPreset::TopRight:      return { 1.0f, 0.0f };
    case AnchorPreset::MiddleLeft:    return { 0.0f, 0.5f };
    case AnchorPreset::Center:        return { 0.5f, 0.5f };
    case AnchorPreset::MiddleRight:   return { 1.0f, 0.5f };
    case AnchorPreset::BottomLeft:    return { 0.0f, 1.0f };
    case AnchorPreset::BottomCenter:  return { 0.5f, 1.0f };
    case AnchorPreset::BottomRight:   return { 1.0f, 1.0f };
    case AnchorPreset::StretchAll:    return { 0.5f, 0.5f };
    }
    return { 0.0f, 0.0f };
}

class Zenith_UICanvas;

class Zenith_UIElement
{
public:
    Zenith_UIElement(const std::string& strName = "UIElement");
    virtual ~Zenith_UIElement();

    // Prevent copying
    Zenith_UIElement(const Zenith_UIElement&) = delete;
    Zenith_UIElement& operator=(const Zenith_UIElement&) = delete;

    // ========== Type Identification ==========

    virtual UIElementType GetType() const { return UIElementType::Base; }
    static const char* GetTypeName(UIElementType eType);

    // ========== Transform ==========

    void SetPosition(float fX, float fY) { m_xPosition = { fX, fY }; MarkTransformDirty(); }
    void SetPosition(const Zenith_Maths::Vector2& xPos) { m_xPosition = xPos; MarkTransformDirty(); }
    Zenith_Maths::Vector2 GetPosition() const { return m_xPosition; }

    void SetSize(float fWidth, float fHeight) { m_xSize = { fWidth, fHeight }; MarkTransformDirty(); if (m_pxParent) m_pxParent->OnChildSizeChanged(); }
    void SetSize(const Zenith_Maths::Vector2& xSize) { m_xSize = xSize; MarkTransformDirty(); if (m_pxParent) m_pxParent->OnChildSizeChanged(); }
    Zenith_Maths::Vector2 GetSize() const { return m_xSize; }

    void SetAnchor(float fX, float fY) { m_xAnchor = { fX, fY }; MarkTransformDirty(); }
    void SetAnchor(const Zenith_Maths::Vector2& xAnchor) { m_xAnchor = xAnchor; MarkTransformDirty(); }
    void SetAnchor(AnchorPreset ePreset) { m_xAnchor = AnchorPresetToValue(ePreset); MarkTransformDirty(); }
    Zenith_Maths::Vector2 GetAnchor() const { return m_xAnchor; }

    void SetPivot(float fX, float fY) { m_xPivot = { fX, fY }; MarkTransformDirty(); }
    void SetPivot(const Zenith_Maths::Vector2& xPivot) { m_xPivot = xPivot; MarkTransformDirty(); }
    void SetPivot(AnchorPreset ePreset) { m_xPivot = AnchorPresetToValue(ePreset); MarkTransformDirty(); }
    Zenith_Maths::Vector2 GetPivot() const { return m_xPivot; }

    void SetAnchorAndPivot(AnchorPreset ePreset)
    {
        Zenith_Maths::Vector2 xValue = AnchorPresetToValue(ePreset);
        m_xAnchor = xValue;
        m_xPivot = xValue;
        m_bStretchAll = (ePreset == AnchorPreset::StretchAll);
        MarkTransformDirty();
    }

    bool IsStretchAll() const { return m_bStretchAll; }

    // ========== Tweens ==========

    void TweenAlpha(float fTo, float fDuration, TweenEasing eEasing = TweenEasing::LINEAR, float fDelay = 0.f);
    void TweenPosition(const Zenith_Maths::Vector2& xTo, float fDuration, TweenEasing eEasing = TweenEasing::LINEAR, float fDelay = 0.f);
    void TweenColor(const Zenith_Maths::Vector4& xTo, float fDuration, TweenEasing eEasing = TweenEasing::LINEAR, float fDelay = 0.f);
    void TweenSize(const Zenith_Maths::Vector2& xTo, float fDuration, TweenEasing eEasing = TweenEasing::LINEAR, float fDelay = 0.f);
    void CancelTweens();
    bool IsTweening() const;

    // ========== Sort Order ==========

    void SetSortOrder(int iOrder) { m_iSortOrder = iOrder; }
    int GetSortOrder() const { return m_iSortOrder; }

    // ========== Appearance ==========

    void SetColor(const Zenith_Maths::Vector4& xColor) { m_xColor = xColor; }
    Zenith_Maths::Vector4 GetColor() const { return m_xColor; }

    void SetVisible(bool bVisible) { m_bVisible = bVisible; if (m_pxParent) m_pxParent->OnChildVisibilityChanged(); }
    bool IsVisible() const { return m_bVisible; }

    virtual void OnChildVisibilityChanged() {}
    virtual void OnChildSizeChanged() {}
    virtual void OnChildAdded() {}
    virtual void OnChildRemoved() {}

    // ========== Group Alpha / Interactable ==========

    void SetGroupAlpha(float fAlpha) { m_fGroupAlpha = fAlpha; }
    float GetGroupAlpha() const { return m_fGroupAlpha; }

    float GetEffectiveAlpha() const
    {
        float fAlpha = m_fGroupAlpha;
        Zenith_UIElement* pxCurrent = m_pxParent;
        while (pxCurrent)
        {
            fAlpha *= pxCurrent->m_fGroupAlpha;
            pxCurrent = pxCurrent->m_pxParent;
        }
        return fAlpha;
    }

    void SetGroupInteractable(bool bInteractable) { m_bGroupInteractable = bInteractable; }
    bool IsGroupInteractable() const
    {
        if (!m_bGroupInteractable) return false;
        return m_pxParent ? m_pxParent->IsGroupInteractable() : true;
    }

    // ========== Focus Navigation ==========

    void SetFocusable(bool bFocusable) { m_bFocusable = bFocusable; }
    bool IsFocusable() const { return m_bFocusable; }

    void SetNavigation(Zenith_UIElement* pxUp, Zenith_UIElement* pxDown, Zenith_UIElement* pxLeft, Zenith_UIElement* pxRight)
    {
        m_pxNavUp = pxUp;
        m_pxNavDown = pxDown;
        m_pxNavLeft = pxLeft;
        m_pxNavRight = pxRight;
    }

    Zenith_UIElement* GetNavUp() const { return m_pxNavUp; }
    Zenith_UIElement* GetNavDown() const { return m_pxNavDown; }
    Zenith_UIElement* GetNavLeft() const { return m_pxNavLeft; }
    Zenith_UIElement* GetNavRight() const { return m_pxNavRight; }

    // ========== Background Style ==========

    void SetBackgroundEnabled(bool bEnabled) { m_bHasBackground = bEnabled; }
    bool HasBackground() const { return m_bHasBackground; }

    void SetBackgroundColor(const Zenith_Maths::Vector4& xColor)
    {
        m_xBackgroundStyle.m_xFillColor = xColor;
        m_bHasBackground = true;
    }

    void SetBackgroundCornerRadius(float fRadius) { m_xBackgroundStyle.m_fCornerRadius = fRadius; }
    void SetBackgroundBorderColor(const Zenith_Maths::Vector4& xColor) { m_xBackgroundStyle.m_xBorderColor = xColor; }
    void SetBackgroundBorderThickness(float fThickness) { m_xBackgroundStyle.m_fBorderThickness = fThickness; }
    void SetBackgroundStyle(const UIStyle& xStyle) { m_xBackgroundStyle = xStyle; m_bHasBackground = true; }
    const UIStyle& GetBackgroundStyle() const { return m_xBackgroundStyle; }

    // ========== Hierarchy ==========

    void AddChild(Zenith_UIElement* pxChild);
    void RemoveChild(Zenith_UIElement* pxChild);
    void ClearChildren();

    Zenith_UIElement* GetParent() const { return m_pxParent; }
    const Zenith_Vector<Zenith_UIElement*>& GetChildren() const { return m_xChildren; }
    size_t GetChildCount() const { return m_xChildren.GetSize(); }
    Zenith_UIElement* GetChild(size_t uIndex) const;

    // ========== Computed Values ==========

    Zenith_Maths::Vector4 GetScreenBounds() const;
    Zenith_Maths::Vector2 GetScreenPosition() const;

    // ========== Identification ==========

    void SetName(const std::string& strName) { m_strName = strName; }
    const std::string& GetName() const { return m_strName; }

    // ========== Serialization ==========

    virtual void WriteToDataStream(Zenith_DataStream& xStream) const;
    virtual void ReadFromDataStream(Zenith_DataStream& xStream);

    // Factory method to create element from type
    static Zenith_UIElement* CreateFromType(UIElementType eType, const std::string& strName = "UIElement");

    // ========== Virtual Methods ==========

    virtual void Update(float fDt);
    virtual void Render(Zenith_UICanvas& xCanvas);

#ifdef ZENITH_TOOLS
    virtual void RenderPropertiesPanel();
#endif

protected:
    void MarkTransformDirty();
    void RecalculateScreenBounds() const;

    std::string m_strName;

    // Transform
    Zenith_Maths::Vector2 m_xPosition = { 0.0f, 0.0f };
    Zenith_Maths::Vector2 m_xSize = { 100.0f, 100.0f };
    Zenith_Maths::Vector2 m_xAnchor = { 0.0f, 0.0f };
    Zenith_Maths::Vector2 m_xPivot = { 0.0f, 0.0f };

    // Appearance
    Zenith_Maths::Vector4 m_xColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool m_bVisible = true;
    float m_fGroupAlpha = 1.0f;
    bool m_bGroupInteractable = true;

    // Background
    bool m_bHasBackground = false;
    UIStyle m_xBackgroundStyle;

    // Hierarchy - raw pointers, canvas owns all elements
    Zenith_UIElement* m_pxParent = nullptr;
    Zenith_Vector<Zenith_UIElement*> m_xChildren;

    // Active tweens (runtime-only, not serialized)
    static constexpr uint32_t uMAX_TWEENS = 8;
    Zenith_UITween m_axTweens[uMAX_TWEENS];
    uint32_t m_uActiveTweenCount = 0;

    // Sort order (higher renders on top)
    int m_iSortOrder = 0;

    // Focus navigation
    bool m_bFocusable = false;
    Zenith_UIElement* m_pxNavUp = nullptr;
    Zenith_UIElement* m_pxNavDown = nullptr;
    Zenith_UIElement* m_pxNavLeft = nullptr;
    Zenith_UIElement* m_pxNavRight = nullptr;

    // StretchAll mode
    bool m_bStretchAll = false;

    // Cached bounds
    mutable bool m_bTransformDirty = true;
    mutable Zenith_Maths::Vector4 m_xCachedScreenBounds = { 0, 0, 100, 100 };

    friend class Zenith_UICanvas;
    Zenith_UICanvas* m_pxCanvas = nullptr;
};

} // namespace Zenith_UI
