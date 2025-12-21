#include "Zenith.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_UIComponent::Zenith_UIComponent(Zenith_Entity& xParentEntity)
    : m_xParentEntity(xParentEntity)
{
    Zenith_Log("[UIComponent] Created for entity %u", xParentEntity.GetEntityID());
}

Zenith_UIComponent::~Zenith_UIComponent()
{
    // Canvas destructor handles cleanup of all elements
}

Zenith_UI::Zenith_UIText* Zenith_UIComponent::CreateText(const std::string& strName, const std::string& strText)
{
    Zenith_UI::Zenith_UIText* pxText = new Zenith_UI::Zenith_UIText(strText, strName);
    m_xCanvas.AddElement(pxText);
    return pxText;
}

Zenith_UI::Zenith_UIRect* Zenith_UIComponent::CreateRect(const std::string& strName)
{
    Zenith_UI::Zenith_UIRect* pxRect = new Zenith_UI::Zenith_UIRect(strName);
    m_xCanvas.AddElement(pxRect);
    return pxRect;
}

Zenith_UI::Zenith_UIImage* Zenith_UIComponent::CreateImage(const std::string& strName)
{
    Zenith_UI::Zenith_UIImage* pxImage = new Zenith_UI::Zenith_UIImage(strName);
    m_xCanvas.AddElement(pxImage);
    return pxImage;
}

Zenith_UI::Zenith_UIElement* Zenith_UIComponent::CreateElement(const std::string& strName)
{
    Zenith_UI::Zenith_UIElement* pxElement = new Zenith_UI::Zenith_UIElement(strName);
    m_xCanvas.AddElement(pxElement);
    return pxElement;
}

void Zenith_UIComponent::AddElement(Zenith_UI::Zenith_UIElement* pxElement)
{
    if (pxElement)
    {
        m_xCanvas.AddElement(pxElement);
    }
}

Zenith_UI::Zenith_UIElement* Zenith_UIComponent::FindElement(const std::string& strName)
{
    return m_xCanvas.FindElement(strName);
}

void Zenith_UIComponent::RemoveElement(const std::string& strName)
{
    Zenith_UI::Zenith_UIElement* pxElement = m_xCanvas.FindElement(strName);
    if (pxElement)
    {
        m_xCanvas.RemoveElement(pxElement);
    }
}

void Zenith_UIComponent::ClearElements()
{
    m_xCanvas.Clear();
}

void Zenith_UIComponent::Update(float fDt)
{
    if (m_bVisible)
    {
        m_xCanvas.Update(fDt);
    }
}

void Zenith_UIComponent::Render()
{
    if (m_bVisible)
    {
        m_xCanvas.Render();
    }
}

// ========== Serialization ==========

static constexpr uint32_t UI_COMPONENT_VERSION = 2;

void Zenith_UIComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
    xStream << UI_COMPONENT_VERSION;
    xStream << m_bVisible;

    // Serialize the canvas and all its elements
    m_xCanvas.WriteToDataStream(xStream);
}

void Zenith_UIComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
    uint32_t uVersion;
    xStream >> uVersion;

    xStream >> m_bVisible;

    if (uVersion >= 2)
    {
        // Deserialize the canvas and all its elements
        m_xCanvas.ReadFromDataStream(xStream);
    }
}

// ========== Editor UI ==========

#ifdef ZENITH_TOOLS

void Zenith_UIComponent::RenderPropertiesPanel()
{
    if (ImGui::CollapsingHeader("UI Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Visible", &m_bVisible);

        ImGui::Separator();

        // Add element buttons
        ImGui::Text("Add Element:");
        ImGui::SameLine();

        if (ImGui::Button("Text"))
        {
            static int s_iTextCount = 0;
            std::string strName = "Text_" + std::to_string(s_iTextCount++);
            Zenith_UI::Zenith_UIText* pxText = CreateText(strName, "New Text");
            pxText->SetSize(200, 30);
            m_pxSelectedElement = pxText;
        }
        ImGui::SameLine();

        if (ImGui::Button("Rect"))
        {
            static int s_iRectCount = 0;
            std::string strName = "Rect_" + std::to_string(s_iRectCount++);
            Zenith_UI::Zenith_UIRect* pxRect = CreateRect(strName);
            pxRect->SetSize(100, 50);
            pxRect->SetColor({ 0.5f, 0.5f, 0.5f, 1.0f });
            m_pxSelectedElement = pxRect;
        }
        ImGui::SameLine();

        if (ImGui::Button("Image"))
        {
            static int s_iImageCount = 0;
            std::string strName = "Image_" + std::to_string(s_iImageCount++);
            Zenith_UI::Zenith_UIImage* pxImage = CreateImage(strName);
            pxImage->SetSize(64, 64);
            m_pxSelectedElement = pxImage;
        }

        ImGui::Separator();

        // Element hierarchy
        ImGui::Text("Elements (%zu):", m_xCanvas.GetElementCount());

        const auto& xElements = m_xCanvas.GetElements();
        if (xElements.empty())
        {
            ImGui::TextDisabled("No UI elements");
        }
        else
        {
            for (Zenith_UI::Zenith_UIElement* pxElement : xElements)
            {
                if (pxElement)
                {
                    RenderElementTree(pxElement, 0);
                }
            }
        }

        // Selected element properties
        if (m_pxSelectedElement)
        {
            ImGui::Separator();
            ImGui::Text("Selected Element Properties:");

            // Delete button
            if (ImGui::Button("Delete Selected"))
            {
                m_xCanvas.RemoveElement(m_pxSelectedElement);
                m_pxSelectedElement = nullptr;
            }
            else
            {
                ImGui::Separator();
                m_pxSelectedElement->RenderPropertiesPanel();
            }
        }
    }
}

void Zenith_UIComponent::RenderElementTree(Zenith_UI::Zenith_UIElement* pxElement, int iDepth)
{
    if (!pxElement)
        return;

    ImGui::PushID(pxElement);

    // Indent based on depth
    if (iDepth > 0)
    {
        ImGui::Indent(16.0f);
    }

    // Visibility checkbox
    bool bVisible = pxElement->IsVisible();
    if (ImGui::Checkbox("##vis", &bVisible))
    {
        pxElement->SetVisible(bVisible);
    }
    ImGui::SameLine();

    // Type indicator
    const char* szTypeChar = "E";
    ImVec4 xTypeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

    switch (pxElement->GetType())
    {
    case Zenith_UI::UIElementType::Text:
        szTypeChar = "T";
        xTypeColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
        break;
    case Zenith_UI::UIElementType::Rect:
        szTypeChar = "R";
        xTypeColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
        break;
    case Zenith_UI::UIElementType::Image:
        szTypeChar = "I";
        xTypeColor = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
        break;
    default:
        break;
    }

    ImGui::TextColored(xTypeColor, "[%s]", szTypeChar);
    ImGui::SameLine();

    // Selectable name
    bool bSelected = (m_pxSelectedElement == pxElement);
    if (ImGui::Selectable(pxElement->GetName().c_str(), bSelected))
    {
        m_pxSelectedElement = pxElement;
    }

    // Tooltip with details
    if (ImGui::IsItemHovered())
    {
        Zenith_Maths::Vector2 xPos = pxElement->GetPosition();
        Zenith_Maths::Vector2 xSize = pxElement->GetSize();
        Zenith_Maths::Vector4 xBounds = pxElement->GetScreenBounds();
        ImGui::SetTooltip("Position: (%.1f, %.1f)\nSize: (%.1f, %.1f)\nScreen: (%.0f,%.0f)-(%.0f,%.0f)",
            xPos.x, xPos.y, xSize.x, xSize.y,
            xBounds.x, xBounds.y, xBounds.z, xBounds.w);
    }

    // Recurse into children
    for (Zenith_UI::Zenith_UIElement* pxChild : pxElement->GetChildren())
    {
        RenderElementTree(pxChild, iDepth + 1);
    }

    if (iDepth > 0)
    {
        ImGui::Unindent(16.0f);
    }

    ImGui::PopID();
}

#endif
