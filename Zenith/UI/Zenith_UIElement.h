#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
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

    void SetPosition(float fX, float fY) { m_xPosition = { fX, fY }; m_bTransformDirty = true; }
    void SetPosition(const Zenith_Maths::Vector2& xPos) { m_xPosition = xPos; m_bTransformDirty = true; }
    Zenith_Maths::Vector2 GetPosition() const { return m_xPosition; }

    void SetSize(float fWidth, float fHeight) { m_xSize = { fWidth, fHeight }; m_bTransformDirty = true; }
    void SetSize(const Zenith_Maths::Vector2& xSize) { m_xSize = xSize; m_bTransformDirty = true; }
    Zenith_Maths::Vector2 GetSize() const { return m_xSize; }

    void SetAnchor(float fX, float fY) { m_xAnchor = { fX, fY }; m_bTransformDirty = true; }
    void SetAnchor(const Zenith_Maths::Vector2& xAnchor) { m_xAnchor = xAnchor; m_bTransformDirty = true; }
    void SetAnchor(AnchorPreset ePreset) { m_xAnchor = AnchorPresetToValue(ePreset); m_bTransformDirty = true; }
    Zenith_Maths::Vector2 GetAnchor() const { return m_xAnchor; }

    void SetPivot(float fX, float fY) { m_xPivot = { fX, fY }; m_bTransformDirty = true; }
    void SetPivot(const Zenith_Maths::Vector2& xPivot) { m_xPivot = xPivot; m_bTransformDirty = true; }
    void SetPivot(AnchorPreset ePreset) { m_xPivot = AnchorPresetToValue(ePreset); m_bTransformDirty = true; }
    Zenith_Maths::Vector2 GetPivot() const { return m_xPivot; }

    void SetAnchorAndPivot(AnchorPreset ePreset)
    {
        Zenith_Maths::Vector2 xValue = AnchorPresetToValue(ePreset);
        m_xAnchor = xValue;
        m_xPivot = xValue;
        m_bTransformDirty = true;
    }

    // ========== Appearance ==========

    void SetColor(const Zenith_Maths::Vector4& xColor) { m_xColor = xColor; }
    Zenith_Maths::Vector4 GetColor() const { return m_xColor; }

    void SetVisible(bool bVisible) { m_bVisible = bVisible; }
    bool IsVisible() const { return m_bVisible; }

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

    // Hierarchy - raw pointers, canvas owns all elements
    Zenith_UIElement* m_pxParent = nullptr;
    Zenith_Vector<Zenith_UIElement*> m_xChildren;

    // Cached bounds
    mutable bool m_bTransformDirty = true;
    mutable Zenith_Maths::Vector4 m_xCachedScreenBounds = { 0, 0, 100, 100 };

    friend class Zenith_UICanvas;
    Zenith_UICanvas* m_pxCanvas = nullptr;
};

} // namespace Zenith_UI
