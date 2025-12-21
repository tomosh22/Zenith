#include "Zenith.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
#include "Flux/Quads/Flux_Quads.h"
#include "DataStream/Zenith_DataStream.h"

namespace Zenith_UI {

static constexpr uint32_t UI_CANVAS_VERSION = 1;

// Static member initialization
Zenith_UICanvas* Zenith_UICanvas::s_pxPrimaryCanvas = nullptr;
std::vector<UITextEntry> Zenith_UICanvas::s_xPendingTextEntries;

void Zenith_UICanvas::Initialise()
{
    Zenith_Log("Zenith_UICanvas system initialized");
}

void Zenith_UICanvas::Shutdown()
{
    s_pxPrimaryCanvas = nullptr;
    s_xPendingTextEntries.clear();
    Zenith_Log("Zenith_UICanvas system shutdown");
}

Zenith_UICanvas::Zenith_UICanvas()
{
    UpdateSize();

    if (!s_pxPrimaryCanvas)
    {
        s_pxPrimaryCanvas = this;
    }
}

Zenith_UICanvas::~Zenith_UICanvas()
{
    Clear();

    if (s_pxPrimaryCanvas == this)
    {
        s_pxPrimaryCanvas = nullptr;
    }
}

void Zenith_UICanvas::AddElement(Zenith_UIElement* pxElement)
{
    if (pxElement)
    {
        pxElement->m_pxParent = nullptr;
        pxElement->m_pxCanvas = this;
        pxElement->m_bTransformDirty = true;

        m_xAllElements.push_back(pxElement);
        m_xRootElements.push_back(pxElement);
    }
}

void Zenith_UICanvas::RemoveElement(Zenith_UIElement* pxElement)
{
    if (!pxElement)
        return;

    // Remove from root elements
    auto rootIt = std::remove(m_xRootElements.begin(), m_xRootElements.end(), pxElement);
    m_xRootElements.erase(rootIt, m_xRootElements.end());

    // Remove from all elements and delete
    auto allIt = std::remove(m_xAllElements.begin(), m_xAllElements.end(), pxElement);
    m_xAllElements.erase(allIt, m_xAllElements.end());

    delete pxElement;
}

void Zenith_UICanvas::Clear()
{
    // Delete all elements
    for (Zenith_UIElement* pxElement : m_xAllElements)
    {
        delete pxElement;
    }

    m_xAllElements.clear();
    m_xRootElements.clear();
}

Zenith_UIElement* Zenith_UICanvas::FindElement(const std::string& strName) const
{
    for (Zenith_UIElement* pxElement : m_xRootElements)
    {
        Zenith_UIElement* pxFound = FindElementRecursive(pxElement, strName);
        if (pxFound)
        {
            return pxFound;
        }
    }
    return nullptr;
}

Zenith_UIElement* Zenith_UICanvas::FindElementRecursive(Zenith_UIElement* pxElement, const std::string& strName) const
{
    if (!pxElement)
        return nullptr;

    if (pxElement->GetName() == strName)
        return pxElement;

    for (Zenith_UIElement* pxChild : pxElement->GetChildren())
    {
        Zenith_UIElement* pxFound = FindElementRecursive(pxChild, strName);
        if (pxFound)
        {
            return pxFound;
        }
    }

    return nullptr;
}

void Zenith_UICanvas::Update(float fDt)
{
    UpdateSize();

    for (Zenith_UIElement* pxElement : m_xRootElements)
    {
        if (pxElement && pxElement->IsVisible())
        {
            pxElement->Update(fDt);
        }
    }
}

void Zenith_UICanvas::Render()
{
    // Update canvas size before rendering - this marks elements dirty if window was resized
    // This is necessary because Update() may not be called every frame
    UpdateSize();

    for (Zenith_UIElement* pxElement : m_xRootElements)
    {
        if (pxElement && pxElement->IsVisible())
        {
            pxElement->Render(*this);
        }
    }
}

void Zenith_UICanvas::UpdateSize()
{
    int32_t iWidth, iHeight;
    Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);

    Zenith_Maths::Vector2 xNewSize = { static_cast<float>(iWidth), static_cast<float>(iHeight) };

    // If size changed, mark all elements as dirty so they recalculate their bounds
    if (xNewSize.x != m_xSize.x || xNewSize.y != m_xSize.y)
    {
        m_xSize = xNewSize;
        m_fScaleFactor = m_xSize.y / m_xReferenceResolution.y;

        // Mark all elements as transform dirty
        for (Zenith_UIElement* pxElement : m_xAllElements)
        {
            if (pxElement)
            {
                pxElement->m_bTransformDirty = true;
            }
        }
    }
}

void Zenith_UICanvas::SetReferenceResolution(float fWidth, float fHeight)
{
    m_xReferenceResolution = { fWidth, fHeight };
    UpdateSize();
}

void Zenith_UICanvas::SubmitQuad(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID)
{
    float fLeft = xBounds.x;
    float fTop = xBounds.y;
    float fRight = xBounds.z;
    float fBottom = xBounds.w;

    float fWidth = fRight - fLeft;
    float fHeight = fBottom - fTop;

    Zenith_Maths::UVector4 xPositionSize = {
        static_cast<uint32_t>(fLeft),
        static_cast<uint32_t>(fTop),
        static_cast<uint32_t>(fWidth),
        static_cast<uint32_t>(fHeight)
    };

    Zenith_Maths::Vector2 xUVMultAdd = { 1.0f, 0.0f };

    Flux_Quads::Quad xQuad(xPositionSize, xColor, uTextureID, xUVMultAdd);
    Flux_Quads::UploadQuad(xQuad);
}

void Zenith_UICanvas::SubmitQuadWithUV(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID, const Zenith_Maths::Vector2& xUVMin, const Zenith_Maths::Vector2& xUVMax)
{
    float fLeft = xBounds.x;
    float fTop = xBounds.y;
    float fRight = xBounds.z;
    float fBottom = xBounds.w;

    float fWidth = fRight - fLeft;
    float fHeight = fBottom - fTop;

    Zenith_Maths::UVector4 xPositionSize = {
        static_cast<uint32_t>(fLeft),
        static_cast<uint32_t>(fTop),
        static_cast<uint32_t>(fWidth),
        static_cast<uint32_t>(fHeight)
    };

    // UV mult/add format: multiply by (max-min), add min
    float fUVMult = xUVMax.x - xUVMin.x;
    float fUVAdd = xUVMin.x;
    Zenith_Maths::Vector2 xUVMultAdd = { fUVMult, fUVAdd };

    Flux_Quads::Quad xQuad(xPositionSize, xColor, uTextureID, xUVMultAdd);
    Flux_Quads::UploadQuad(xQuad);
}

void Zenith_UICanvas::SubmitText(const std::string& strText, const Zenith_Maths::Vector2& xPosition, float fSize, const Zenith_Maths::Vector4& xColor)
{
    if (strText.empty())
        return;

    UITextEntry xEntry;
    xEntry.m_strText = strText;
    xEntry.m_xPosition = xPosition;
    xEntry.m_fSize = fSize;
    xEntry.m_xColor = xColor;

    s_xPendingTextEntries.push_back(xEntry);
}

void Zenith_UICanvas::WriteToDataStream(Zenith_DataStream& xStream) const
{
    xStream << UI_CANVAS_VERSION;

    // Write number of root elements
    uint32_t uNumElements = static_cast<uint32_t>(m_xRootElements.size());
    xStream << uNumElements;

    // Write each root element (they will write their children)
    for (const Zenith_UIElement* pxElement : m_xRootElements)
    {
        if (pxElement)
        {
            // Write type first so we can create correct type on load
            xStream << static_cast<uint32_t>(pxElement->GetType());
            pxElement->WriteToDataStream(xStream);

            // Write children count and children
            uint32_t uNumChildren = static_cast<uint32_t>(pxElement->GetChildCount());
            xStream << uNumChildren;

            for (size_t i = 0; i < uNumChildren; i++)
            {
                Zenith_UIElement* pxChild = pxElement->GetChild(i);
                if (pxChild)
                {
                    xStream << static_cast<uint32_t>(pxChild->GetType());
                    pxChild->WriteToDataStream(xStream);
                }
            }
        }
    }
}

void Zenith_UICanvas::ReadFromDataStream(Zenith_DataStream& xStream)
{
    Clear();

    uint32_t uVersion;
    xStream >> uVersion;

    uint32_t uNumElements;
    xStream >> uNumElements;

    for (uint32_t i = 0; i < uNumElements; i++)
    {
        uint32_t uType;
        xStream >> uType;

        UIElementType eType = static_cast<UIElementType>(uType);
        Zenith_UIElement* pxElement = Zenith_UIElement::CreateFromType(eType);

        if (pxElement)
        {
            pxElement->ReadFromDataStream(xStream);

            // Read children
            uint32_t uNumChildren;
            xStream >> uNumChildren;

            for (uint32_t c = 0; c < uNumChildren; c++)
            {
                uint32_t uChildType;
                xStream >> uChildType;

                UIElementType eChildType = static_cast<UIElementType>(uChildType);
                Zenith_UIElement* pxChild = Zenith_UIElement::CreateFromType(eChildType);

                if (pxChild)
                {
                    pxChild->ReadFromDataStream(xStream);
                    pxElement->AddChild(pxChild);
                    m_xAllElements.push_back(pxChild);
                }
            }

            AddElement(pxElement);
        }
    }
}

} // namespace Zenith_UI
