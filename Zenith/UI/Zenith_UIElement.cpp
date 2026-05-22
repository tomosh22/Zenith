#include "Zenith.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UILayoutGroup.h"
#include "UI/Zenith_UIToggle.h"
#include "UI/Zenith_UIOverlay.h"
#include "UI/Zenith_UIScrollView.h"
#include "Input/Zenith_InputImpl.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_ELEMENT_VERSION = 3;

Zenith_UIElement::Zenith_UIElement(const std::string& strName)
    : m_strName(strName)
{
}

Zenith_UIElement::~Zenith_UIElement()
{
    // Children are NOT owned by parent - canvas owns all elements
    // Just clear the vector, don't delete
    m_xChildren.Clear();
}

const char* Zenith_UIElement::GetTypeName(UIElementType eType)
{
    switch (eType)
    {
    case UIElementType::Base:  return "Element";
    case UIElementType::Text:  return "Text";
    case UIElementType::Rect:  return "Rect";
    case UIElementType::Image: return "Image";
    case UIElementType::Button: return "Button";
    case UIElementType::LayoutGroup: return "LayoutGroup";
    case UIElementType::Toggle: return "Toggle";
    case UIElementType::Overlay: return "Overlay";
    case UIElementType::ScrollView: return "ScrollView";
    default: return "Unknown";
    }
}

Zenith_UIElement* Zenith_UIElement::CreateFromType(UIElementType eType, const std::string& strName)
{
    switch (eType)
    {
    case UIElementType::Base:  return new Zenith_UIElement(strName);
    case UIElementType::Text:  return new Zenith_UIText("", strName);
    case UIElementType::Rect:  return new Zenith_UIRect(strName);
    case UIElementType::Image: return new Zenith_UIImage(strName);
    case UIElementType::Button: return new Zenith_UIButton("", strName);
    case UIElementType::LayoutGroup: return new Zenith_UILayoutGroup(strName);
    case UIElementType::Toggle: return new Zenith_UIToggle("", strName);
    case UIElementType::Overlay: return new Zenith_UIOverlay(strName);
    case UIElementType::ScrollView: return new Zenith_UIScrollView(strName);
    default: return nullptr;
    }
}

void Zenith_UIElement::AddChild(Zenith_UIElement* pxChild)
{
    if (pxChild)
    {
        pxChild->m_pxParent = this;
        pxChild->m_pxCanvas = m_pxCanvas;
        pxChild->m_bTransformDirty = true;
        m_xChildren.PushBack(pxChild);
        OnChildAdded();
    }
}

void Zenith_UIElement::RemoveChild(Zenith_UIElement* pxChild)
{
    m_xChildren.EraseValue(pxChild);
    OnChildRemoved();
    // Note: Does not delete - canvas owns all elements
}

void Zenith_UIElement::ClearChildren()
{
    // Note: Does not delete - canvas owns all elements
    m_xChildren.Clear();
}

Zenith_UIElement* Zenith_UIElement::GetChild(size_t uIndex) const
{
    if (uIndex < m_xChildren.GetSize())
    {
        return m_xChildren.Get(static_cast<u_int>(uIndex));
    }
    return nullptr;
}

Zenith_Maths::Vector4 Zenith_UIElement::GetScreenBounds() const
{
    if (m_bTransformDirty)
    {
        RecalculateScreenBounds();
    }
    return m_xCachedScreenBounds;
}

Zenith_Maths::Vector2 Zenith_UIElement::GetScreenPosition() const
{
    Zenith_Maths::Vector4 xBounds = GetScreenBounds();
    return { xBounds.x, xBounds.y };
}

void Zenith_UIElement::MarkTransformDirty()
{
    m_bTransformDirty = true;
    for (uint32_t u = 0; u < m_xChildren.GetSize(); ++u)
    {
        m_xChildren.Get(u)->MarkTransformDirty();
    }
}

void Zenith_UIElement::RecalculateScreenBounds() const
{
    Zenith_Maths::Vector2 xParentPos = { 0.0f, 0.0f };
    Zenith_Maths::Vector2 xParentSize = { 1920.0f, 1080.0f };

    if (m_pxParent)
    {
        Zenith_Maths::Vector4 xParentBounds = m_pxParent->GetScreenBounds();
        xParentPos = { xParentBounds.x, xParentBounds.y };
        xParentSize = { xParentBounds.z - xParentBounds.x, xParentBounds.w - xParentBounds.y };
    }
    else if (m_pxCanvas)
    {
        xParentSize = m_pxCanvas->GetSize();
    }

    if (m_bStretchAll)
    {
        m_xCachedScreenBounds = { xParentPos.x, xParentPos.y, xParentPos.x + xParentSize.x, xParentPos.y + xParentSize.y };
    }
    else
    {
        float fAnchorX = xParentPos.x + (m_xAnchor.x * xParentSize.x);
        float fAnchorY = xParentPos.y + (m_xAnchor.y * xParentSize.y);

        float fLeft = fAnchorX + m_xPosition.x - (m_xPivot.x * m_xSize.x);
        float fTop = fAnchorY + m_xPosition.y - (m_xPivot.y * m_xSize.y);

        m_xCachedScreenBounds = { fLeft, fTop, fLeft + m_xSize.x, fTop + m_xSize.y };
    }
    m_bTransformDirty = false;
}

void Zenith_UIElement::Update(float fDt)
{
    // Tick active tweens
    for (uint32_t u = 0; u < m_uActiveTweenCount; )
    {
        Zenith_UITween& xTween = m_axTweens[u];
        xTween.m_fElapsed += fDt;

        if (xTween.m_fElapsed < xTween.m_fDelay)
        {
            ++u;
            continue;
        }

        float fActiveTime = xTween.m_fElapsed - xTween.m_fDelay;
        float fT = (xTween.m_fDuration > 0.f) ? std::min(fActiveTime / xTween.m_fDuration, 1.f) : 1.f;
        float fEased = ApplyEasing(fT, xTween.m_eEasing);
        float fValue = xTween.m_fFrom + (xTween.m_fTo - xTween.m_fFrom) * fEased;

        switch (xTween.m_eProperty)
        {
        case TweenProperty::ALPHA:      m_fGroupAlpha = fValue; break;
        case TweenProperty::POSITION_X: m_xPosition.x = fValue; MarkTransformDirty(); break;
        case TweenProperty::POSITION_Y: m_xPosition.y = fValue; MarkTransformDirty(); break;
        case TweenProperty::SIZE_X:     m_xSize.x = fValue; MarkTransformDirty(); break;
        case TweenProperty::SIZE_Y:     m_xSize.y = fValue; MarkTransformDirty(); break;
        case TweenProperty::COLOR_R:    m_xColor.x = fValue; break;
        case TweenProperty::COLOR_G:    m_xColor.y = fValue; break;
        case TweenProperty::COLOR_B:    m_xColor.z = fValue; break;
        case TweenProperty::COLOR_A:    m_xColor.w = fValue; break;
        }

        if (fT >= 1.f)
        {
            // Remove completed tween by swapping with last
            m_axTweens[u] = m_axTweens[m_uActiveTweenCount - 1];
            m_uActiveTweenCount--;
        }
        else
        {
            ++u;
        }
    }

    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xChildren); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxChild = xIt.GetData();
        if (pxChild && pxChild->IsVisible())
        {
            pxChild->Update(fDt);
        }
    }
}

void Zenith_UIElement::TweenAlpha(float fTo, float fDuration, TweenEasing eEasing, float fDelay)
{
    if (m_uActiveTweenCount >= uMAX_TWEENS) return;
    Zenith_UITween& xTween = m_axTweens[m_uActiveTweenCount++];
    xTween.m_eProperty = TweenProperty::ALPHA;
    xTween.m_fFrom = m_fGroupAlpha;
    xTween.m_fTo = fTo;
    xTween.m_fDuration = fDuration;
    xTween.m_fDelay = fDelay;
    xTween.m_fElapsed = 0.f;
    xTween.m_eEasing = eEasing;
    xTween.m_bActive = true;
}

void Zenith_UIElement::TweenPosition(const Zenith_Maths::Vector2& xTo, float fDuration, TweenEasing eEasing, float fDelay)
{
    if (m_uActiveTweenCount + 1 >= uMAX_TWEENS) return;

    Zenith_UITween& xTweenX = m_axTweens[m_uActiveTweenCount++];
    xTweenX.m_eProperty = TweenProperty::POSITION_X;
    xTweenX.m_fFrom = m_xPosition.x;
    xTweenX.m_fTo = xTo.x;
    xTweenX.m_fDuration = fDuration;
    xTweenX.m_fDelay = fDelay;
    xTweenX.m_fElapsed = 0.f;
    xTweenX.m_eEasing = eEasing;
    xTweenX.m_bActive = true;

    Zenith_UITween& xTweenY = m_axTweens[m_uActiveTweenCount++];
    xTweenY.m_eProperty = TweenProperty::POSITION_Y;
    xTweenY.m_fFrom = m_xPosition.y;
    xTweenY.m_fTo = xTo.y;
    xTweenY.m_fDuration = fDuration;
    xTweenY.m_fDelay = fDelay;
    xTweenY.m_fElapsed = 0.f;
    xTweenY.m_eEasing = eEasing;
    xTweenY.m_bActive = true;
}

void Zenith_UIElement::TweenColor(const Zenith_Maths::Vector4& xTo, float fDuration, TweenEasing eEasing, float fDelay)
{
    if (m_uActiveTweenCount + 3 >= uMAX_TWEENS) return;

    TweenProperty aeProps[] = { TweenProperty::COLOR_R, TweenProperty::COLOR_G, TweenProperty::COLOR_B, TweenProperty::COLOR_A };
    float afFrom[] = { m_xColor.x, m_xColor.y, m_xColor.z, m_xColor.w };
    float afTo[] = { xTo.x, xTo.y, xTo.z, xTo.w };

    for (uint32_t u = 0; u < 4; ++u)
    {
        Zenith_UITween& xTween = m_axTweens[m_uActiveTweenCount++];
        xTween.m_eProperty = aeProps[u];
        xTween.m_fFrom = afFrom[u];
        xTween.m_fTo = afTo[u];
        xTween.m_fDuration = fDuration;
        xTween.m_fDelay = fDelay;
        xTween.m_fElapsed = 0.f;
        xTween.m_eEasing = eEasing;
        xTween.m_bActive = true;
    }
}

void Zenith_UIElement::TweenSize(const Zenith_Maths::Vector2& xTo, float fDuration, TweenEasing eEasing, float fDelay)
{
    if (m_uActiveTweenCount + 1 >= uMAX_TWEENS) return;

    Zenith_UITween& xTweenX = m_axTweens[m_uActiveTweenCount++];
    xTweenX.m_eProperty = TweenProperty::SIZE_X;
    xTweenX.m_fFrom = m_xSize.x;
    xTweenX.m_fTo = xTo.x;
    xTweenX.m_fDuration = fDuration;
    xTweenX.m_fDelay = fDelay;
    xTweenX.m_fElapsed = 0.f;
    xTweenX.m_eEasing = eEasing;
    xTweenX.m_bActive = true;

    Zenith_UITween& xTweenY = m_axTweens[m_uActiveTweenCount++];
    xTweenY.m_eProperty = TweenProperty::SIZE_Y;
    xTweenY.m_fFrom = m_xSize.y;
    xTweenY.m_fTo = xTo.y;
    xTweenY.m_fDuration = fDuration;
    xTweenY.m_fDelay = fDelay;
    xTweenY.m_fElapsed = 0.f;
    xTweenY.m_eEasing = eEasing;
    xTweenY.m_bActive = true;
}

void Zenith_UIElement::CancelTweens()
{
    m_uActiveTweenCount = 0;
}

bool Zenith_UIElement::IsTweening() const
{
    return m_uActiveTweenCount > 0;
}

void Zenith_UIElement::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible)
        return;

    // Render background if enabled (before children so it appears behind)
    if (m_bHasBackground)
    {
        Zenith_Maths::Vector4 xBounds = GetScreenBounds();
        float fAlpha = GetEffectiveAlpha();
        UIStyle xBgStyle = m_xBackgroundStyle;
        xBgStyle.m_xFillColor = m_xBackgroundStyle.m_xFillColor;
        UIStyleRenderer::RenderStyledRect(xCanvas, xBgStyle, xBounds, fAlpha);
    }

    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xChildren); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxChild = xIt.GetData();
        if (pxChild && pxChild->IsVisible())
        {
            pxChild->Render(xCanvas);
        }
    }
}

void Zenith_UIElement::WriteToDataStream(Zenith_DataStream& xStream) const
{
    xStream << UI_ELEMENT_VERSION;
    xStream << static_cast<uint32_t>(GetType());
    xStream << m_strName;

    // Transform
    xStream << m_xPosition.x;
    xStream << m_xPosition.y;
    xStream << m_xSize.x;
    xStream << m_xSize.y;
    xStream << m_xAnchor.x;
    xStream << m_xAnchor.y;
    xStream << m_xPivot.x;
    xStream << m_xPivot.y;

    // Appearance
    xStream << m_xColor.x;
    xStream << m_xColor.y;
    xStream << m_xColor.z;
    xStream << m_xColor.w;
    xStream << m_bVisible;

    // Background (v2)
    xStream << m_bHasBackground;
    xStream << m_xBackgroundStyle.m_xFillColor.x;
    xStream << m_xBackgroundStyle.m_xFillColor.y;
    xStream << m_xBackgroundStyle.m_xFillColor.z;
    xStream << m_xBackgroundStyle.m_xFillColor.w;
    xStream << m_xBackgroundStyle.m_fCornerRadius;
    xStream << m_xBackgroundStyle.m_xBorderColor.x;
    xStream << m_xBackgroundStyle.m_xBorderColor.y;
    xStream << m_xBackgroundStyle.m_xBorderColor.z;
    xStream << m_xBackgroundStyle.m_xBorderColor.w;
    xStream << m_xBackgroundStyle.m_fBorderThickness;

    // StretchAll + SortOrder (v3)
    xStream << m_bStretchAll;
    xStream << m_iSortOrder;
}

void Zenith_UIElement::ReadFromDataStream(Zenith_DataStream& xStream)
{
    uint32_t uVersion;
    xStream >> uVersion;

    uint32_t uType;
    xStream >> uType;  // Type is read by canvas to create correct derived type

    xStream >> m_strName;

    // Transform
    xStream >> m_xPosition.x;
    xStream >> m_xPosition.y;
    xStream >> m_xSize.x;
    xStream >> m_xSize.y;
    xStream >> m_xAnchor.x;
    xStream >> m_xAnchor.y;
    xStream >> m_xPivot.x;
    xStream >> m_xPivot.y;

    // Appearance
    xStream >> m_xColor.x;
    xStream >> m_xColor.y;
    xStream >> m_xColor.z;
    xStream >> m_xColor.w;
    xStream >> m_bVisible;

    // Background (v2)
    if (uVersion >= 2)
    {
        xStream >> m_bHasBackground;
        xStream >> m_xBackgroundStyle.m_xFillColor.x;
        xStream >> m_xBackgroundStyle.m_xFillColor.y;
        xStream >> m_xBackgroundStyle.m_xFillColor.z;
        xStream >> m_xBackgroundStyle.m_xFillColor.w;
        xStream >> m_xBackgroundStyle.m_fCornerRadius;
        xStream >> m_xBackgroundStyle.m_xBorderColor.x;
        xStream >> m_xBackgroundStyle.m_xBorderColor.y;
        xStream >> m_xBackgroundStyle.m_xBorderColor.z;
        xStream >> m_xBackgroundStyle.m_xBorderColor.w;
        xStream >> m_xBackgroundStyle.m_fBorderThickness;
    }

    // StretchAll + SortOrder (v3)
    if (uVersion >= 3)
    {
        xStream >> m_bStretchAll;
        xStream >> m_iSortOrder;
    }

    m_bTransformDirty = true;
}

#ifdef ZENITH_TOOLS

// Helper to detect current anchor preset from anchor/pivot values
// Returns preset index (0-8) or -1 for custom values
static int DetectAnchorPreset(const Zenith_Maths::Vector2& xAnchor, const Zenith_Maths::Vector2& xPivot)
{
    constexpr float fEpsilon = 0.001f;

    auto fApproxEqual = [fEpsilon](float a, float b) { return std::abs(a - b) < fEpsilon; };
    auto xApproxEqual = [&fApproxEqual](const Zenith_Maths::Vector2& a, const Zenith_Maths::Vector2& b)
    {
        return fApproxEqual(a.x, b.x) && fApproxEqual(a.y, b.y);
    };

    // Only match if anchor == pivot (as SetAnchorAndPivot sets both to same value)
    if (!xApproxEqual(xAnchor, xPivot))
        return -1;

    for (int i = 0; i < static_cast<int>(AnchorPreset::StretchAll); ++i)
    {
        Zenith_Maths::Vector2 xPresetValue = AnchorPresetToValue(static_cast<AnchorPreset>(i));
        if (xApproxEqual(xAnchor, xPresetValue))
            return i;
    }

    return -1;
}

void Zenith_UIElement::RenderPropertiesPanel()
{
    // Push unique ID scope for UI element properties to avoid conflicts with
    // Entity/TransformComponent properties that use the same widget labels
    ImGui::PushID("UIElement");

    ImGui::Text("Type: %s", GetTypeName(GetType()));

    char szNameBuffer[256];
    strncpy_s(szNameBuffer, m_strName.c_str(), sizeof(szNameBuffer) - 1);
    if (ImGui::InputText("Element Name", szNameBuffer, sizeof(szNameBuffer)))
    {
        m_strName = szNameBuffer;
    }

    ImGui::Separator();
    ImGui::Text("UI Transform");

    // Anchor preset dropdown
    const char* szPresets[] = {
        "Top Left", "Top Center", "Top Right",
        "Middle Left", "Center", "Middle Right",
        "Bottom Left", "Bottom Center", "Bottom Right",
        "Custom"
    };
    int iCurrentPreset = DetectAnchorPreset(m_xAnchor, m_xPivot);
    int iComboIndex = (iCurrentPreset >= 0) ? iCurrentPreset : 9; // 9 = "Custom"

    if (ImGui::Combo("Anchor Preset", &iComboIndex, szPresets, 10))
    {
        if (iComboIndex < 9) // Not "Custom"
        {
            SetAnchorAndPivot(static_cast<AnchorPreset>(iComboIndex));
        }
    }

    float fPos[2] = { m_xPosition.x, m_xPosition.y };
    if (ImGui::DragFloat2("UI Position", fPos, 1.0f))
    {
        SetPosition(fPos[0], fPos[1]);
    }

    float fSize[2] = { m_xSize.x, m_xSize.y };
    if (ImGui::DragFloat2("UI Size", fSize, 1.0f, 0.0f, 10000.0f))
    {
        SetSize(fSize[0], fSize[1]);
    }

    float fAnchor[2] = { m_xAnchor.x, m_xAnchor.y };
    if (ImGui::DragFloat2("Anchor", fAnchor, 0.01f, 0.0f, 1.0f))
    {
        SetAnchor(fAnchor[0], fAnchor[1]);
    }

    float fPivot[2] = { m_xPivot.x, m_xPivot.y };
    if (ImGui::DragFloat2("Pivot", fPivot, 0.01f, 0.0f, 1.0f))
    {
        SetPivot(fPivot[0], fPivot[1]);
    }

    ImGui::Separator();
    ImGui::Text("Appearance");

    float fColor[4] = { m_xColor.x, m_xColor.y, m_xColor.z, m_xColor.w };
    if (ImGui::ColorEdit4("Element Color", fColor))
    {
        SetColor({ fColor[0], fColor[1], fColor[2], fColor[3] });
    }

    ImGui::Checkbox("Element Visible", &m_bVisible);

    ImGui::Separator();
    ImGui::Text("Background");

    ImGui::Checkbox("Has Background", &m_bHasBackground);
    if (m_bHasBackground)
    {
        float fBgColor[4] = { m_xBackgroundStyle.m_xFillColor.x, m_xBackgroundStyle.m_xFillColor.y, m_xBackgroundStyle.m_xFillColor.z, m_xBackgroundStyle.m_xFillColor.w };
        if (ImGui::ColorEdit4("Bg Color", fBgColor))
        {
            m_xBackgroundStyle.m_xFillColor = { fBgColor[0], fBgColor[1], fBgColor[2], fBgColor[3] };
        }
        ImGui::DragFloat("Bg Corner Radius", &m_xBackgroundStyle.m_fCornerRadius, 0.5f, 0.0f, 100.0f);
        ImGui::DragFloat("Bg Border Thickness", &m_xBackgroundStyle.m_fBorderThickness, 0.5f, 0.0f, 50.0f);
        float fBorderColor[4] = { m_xBackgroundStyle.m_xBorderColor.x, m_xBackgroundStyle.m_xBorderColor.y, m_xBackgroundStyle.m_xBorderColor.z, m_xBackgroundStyle.m_xBorderColor.w };
        if (ImGui::ColorEdit4("Bg Border Color", fBorderColor))
        {
            m_xBackgroundStyle.m_xBorderColor = { fBorderColor[0], fBorderColor[1], fBorderColor[2], fBorderColor[3] };
        }
    }

    ImGui::PopID();
}
#endif

void Zenith_UIElement::GetTransformedMousePosition(float& fMouseX, float& fMouseY) const
{
    Zenith_Maths::Vector2_64 xMousePos;
    g_xEngine.Input().GetMousePosition(xMousePos);
    fMouseX = static_cast<float>(xMousePos.x);
    fMouseY = static_cast<float>(xMousePos.y);

#ifdef ZENITH_TOOLS
#ifdef ZENITH_INPUT_SIMULATOR
    if (!Zenith_InputSimulator::IsEnabled())
#endif
    {
        Zenith_Maths::Vector2 xViewportPos = Zenith_Editor::GetViewportPos();
        Zenith_Maths::Vector2 xViewportSize = Zenith_Editor::GetViewportSize();
        if (xViewportSize.x > 0.f && xViewportSize.y > 0.f && m_pxCanvas)
        {
            Zenith_Maths::Vector2 xCanvasSize = m_pxCanvas->GetSize();
            fMouseX = (fMouseX - xViewportPos.x) * (xCanvasSize.x / xViewportSize.x);
            fMouseY = (fMouseY - xViewportPos.y) * (xCanvasSize.y / xViewportSize.y);
        }
    }
#endif
}

} // namespace Zenith_UI
