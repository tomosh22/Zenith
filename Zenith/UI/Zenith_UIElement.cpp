#include "Zenith.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_ELEMENT_VERSION = 1;

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
    }
}

void Zenith_UIElement::RemoveChild(Zenith_UIElement* pxChild)
{
    m_xChildren.EraseValue(pxChild);
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

    float fAnchorX = xParentPos.x + (m_xAnchor.x * xParentSize.x);
    float fAnchorY = xParentPos.y + (m_xAnchor.y * xParentSize.y);

    float fLeft = fAnchorX + m_xPosition.x - (m_xPivot.x * m_xSize.x);
    float fTop = fAnchorY + m_xPosition.y - (m_xPivot.y * m_xSize.y);

    m_xCachedScreenBounds = { fLeft, fTop, fLeft + m_xSize.x, fTop + m_xSize.y };
    m_bTransformDirty = false;
}

void Zenith_UIElement::Update(float fDt)
{
    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xChildren); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxChild = xIt.GetData();
        if (pxChild && pxChild->IsVisible())
        {
            pxChild->Update(fDt);
        }
    }
}

void Zenith_UIElement::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible)
        return;

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

    ImGui::PopID();
}
#endif

} // namespace Zenith_UI
