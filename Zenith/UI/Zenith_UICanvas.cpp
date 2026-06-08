#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIToggle.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Input/Zenith_Input.h"
#include <algorithm>

namespace Zenith_UI {

static constexpr uint32_t UI_CANVAS_VERSION = 2;

// Static member initialization
Zenith_UICanvas* Zenith_UICanvas::s_pxPrimaryCanvas = nullptr;
Zenith_Vector<UITextEntry> Zenith_UICanvas::s_xPendingTextEntries;

void Zenith_UICanvas::Initialise()
{
    Zenith_Log(LOG_CATEGORY_UI, "Zenith_UICanvas system initialized");
}

void Zenith_UICanvas::Shutdown()
{
    s_pxPrimaryCanvas = nullptr;
    s_xPendingTextEntries.Clear();
    Zenith_Log(LOG_CATEGORY_UI, "Zenith_UICanvas system shutdown");
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

Zenith_UICanvas::Zenith_UICanvas(Zenith_UICanvas&& xOther)
    : m_xAllElements(std::move(xOther.m_xAllElements))
    , m_xRootElements(std::move(xOther.m_xRootElements))
    , m_xSize(xOther.m_xSize)
    , m_xReferenceResolution(xOther.m_xReferenceResolution)
    , m_fScaleFactor(xOther.m_fScaleFactor)
{
    // Update element canvas pointers to point to this canvas
    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xAllElements); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxElement = xIt.GetData();
        if (pxElement)
        {
            pxElement->m_pxCanvas = this;
        }
    }

    // If the other canvas was the primary canvas, transfer that role
    if (s_pxPrimaryCanvas == &xOther)
    {
        s_pxPrimaryCanvas = this;
    }
}

Zenith_UICanvas& Zenith_UICanvas::operator=(Zenith_UICanvas&& xOther)
{
    if (this != &xOther)
    {
        // Clear existing elements
        Clear();

        // Transfer ownership
        m_xAllElements = std::move(xOther.m_xAllElements);
        m_xRootElements = std::move(xOther.m_xRootElements);
        m_xSize = xOther.m_xSize;
        m_xReferenceResolution = xOther.m_xReferenceResolution;
        m_fScaleFactor = xOther.m_fScaleFactor;

        // Update element canvas pointers
        for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xAllElements); !xIt.Done(); xIt.Next())
        {
            Zenith_UIElement* pxElement = xIt.GetData();
            if (pxElement)
            {
                pxElement->m_pxCanvas = this;
            }
        }

        // Handle primary canvas transfer
        if (s_pxPrimaryCanvas == &xOther)
        {
            s_pxPrimaryCanvas = this;
        }
    }
    return *this;
}

void Zenith_UICanvas::AddElement(Zenith_UIElement* pxElement)
{
    if (pxElement)
    {
        pxElement->m_pxParent = nullptr;
        pxElement->m_pxCanvas = this;
        pxElement->m_bTransformDirty = true;

        m_xAllElements.PushBack(pxElement);
        m_xRootElements.PushBack(pxElement);
    }
}

void Zenith_UICanvas::RemoveElement(Zenith_UIElement* pxElement)
{
    if (!pxElement)
        return;

    // Remove from root elements
    m_xRootElements.EraseValue(pxElement);

    // Remove from all elements and delete
    m_xAllElements.EraseValue(pxElement);

    delete pxElement;
}

void Zenith_UICanvas::ReparentElement(Zenith_UIElement* pxChild, Zenith_UIElement* pxNewParent)
{
    if (!pxChild || !pxNewParent)
        return;

    // Remove from root elements if present
    m_xRootElements.EraseValue(pxChild);

    // Remove from old parent if any
    if (pxChild->GetParent())
    {
        pxChild->GetParent()->RemoveChild(pxChild);
    }

    // Add to new parent
    pxNewParent->AddChild(pxChild);
}

void Zenith_UICanvas::Clear()
{
    m_pxFocusedElement = nullptr;

    // Delete all elements
    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xAllElements); !xIt.Done(); xIt.Next())
    {
        delete xIt.GetData();
    }

    m_xAllElements.Clear();
    m_xRootElements.Clear();
}

Zenith_UIElement* Zenith_UICanvas::FindElement(const std::string& strName) const
{
    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xRootElements); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxFound = FindElementRecursive(xIt.GetData(), strName);
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

    const Zenith_Vector<Zenith_UIElement*>& xChildren = pxElement->GetChildren();
    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(xChildren); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxFound = FindElementRecursive(xIt.GetData(), strName);
        if (pxFound)
        {
            return pxFound;
        }
    }

    return nullptr;
}

void Zenith_UICanvas::UpdateFocusNavigation()
{
    if (!m_pxFocusedElement)
        return;

    if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_UP))
        NavigateUp();
    else if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_DOWN))
        NavigateDown();
    else if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_LEFT))
        NavigateLeft();
    else if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_RIGHT))
        NavigateRight();

    if (!g_xEngine.Input().IsGamepadConnected())
        return;

    if (g_xEngine.Input().WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_DPAD_UP))
        NavigateUp();
    else if (g_xEngine.Input().WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN))
        NavigateDown();
    else if (g_xEngine.Input().WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_DPAD_LEFT))
        NavigateLeft();
    else if (g_xEngine.Input().WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT))
        NavigateRight();

    if (g_xEngine.Input().WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_A))
        ActivateFocused();
}

void Zenith_UICanvas::Update(float fDt)
{
    UpdateSize();

    UpdateFocusNavigation();

    // SceneUpdateDeferralGuard around the outer Zenith_UIComponent iteration
    // ensures LoadScene calls fired from button callbacks defer until the
    // guard scope closes — m_xRootElements stays stable through this pass.
    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xRootElements); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxElement = xIt.GetData();
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

    // Build a sorted array of root elements by sort order (stable sort preserves insertion order)
    static constexpr uint32_t uMAX_SORTED_ROOTS = 128;
    Zenith_UIElement* apxSorted[uMAX_SORTED_ROOTS];
    uint32_t uCount = 0;

    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xRootElements); !xIt.Done(); xIt.Next())
    {
        Zenith_UIElement* pxElement = xIt.GetData();
        if (pxElement && pxElement->IsVisible() && uCount < uMAX_SORTED_ROOTS)
        {
            apxSorted[uCount++] = pxElement;
        }
    }

    std::stable_sort(apxSorted, apxSorted + uCount, [](const Zenith_UIElement* pxA, const Zenith_UIElement* pxB)
    {
        return pxA->GetSortOrder() < pxB->GetSortOrder();
    });

    for (uint32_t u = 0; u < uCount; ++u)
    {
        m_iCurrentSortOrder = apxSorted[u]->GetSortOrder();
        apxSorted[u]->Render(*this);
    }

    // Reset sort order after render loop so text submitted outside Render
    // (e.g. during script Update on the next frame) gets sort order 0,
    // not the last rendered element's sort order
    m_iCurrentSortOrder = 0;
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
        for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xAllElements); !xIt.Done(); xIt.Next())
        {
            Zenith_UIElement* pxElement = xIt.GetData();
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

// ========== Clip Rect Stack ==========

void Zenith_UICanvas::PushClipRect(const Zenith_Maths::Vector4& xBounds)
{
    Zenith_Assert(m_uClipRectStackDepth < uMAX_CLIP_RECT_DEPTH, "Clip rect stack overflow");

    if (m_uClipRectStackDepth > 0)
    {
        // Intersect with current active clip rect
        const Zenith_Maths::Vector4& xCurrent = m_axClipRectStack[m_uClipRectStackDepth - 1];
        Zenith_Maths::Vector4 xIntersected = {
            glm::max(xBounds.x, xCurrent.x),
            glm::max(xBounds.y, xCurrent.y),
            glm::min(xBounds.z, xCurrent.z),
            glm::min(xBounds.w, xCurrent.w)
        };
        m_axClipRectStack[m_uClipRectStackDepth] = xIntersected;
    }
    else
    {
        m_axClipRectStack[m_uClipRectStackDepth] = xBounds;
    }

    m_uClipRectStackDepth++;
}

void Zenith_UICanvas::PopClipRect()
{
    if (m_uClipRectStackDepth > 0)
    {
        m_uClipRectStackDepth--;
    }
}

Zenith_Maths::Vector4 Zenith_UICanvas::GetActiveClipRect() const
{
    if (m_uClipRectStackDepth > 0)
    {
        return m_axClipRectStack[m_uClipRectStackDepth - 1];
    }
    return {0.f, 0.f, 99999.f, 99999.f};
}

void Zenith_UICanvas::SubmitQuad(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID,
    float fCornerRadius, const Zenith_Maths::Vector4& xGradientColor)
{
    float fLeft = xBounds.x;
    float fTop = xBounds.y;
    float fRight = xBounds.z;
    float fBottom = xBounds.w;

    // CPU-side clip rect clamping
    if (m_uClipRectStackDepth > 0)
    {
        const Zenith_Maths::Vector4& xClip = m_axClipRectStack[m_uClipRectStackDepth - 1];
        fLeft = glm::max(fLeft, xClip.x);
        fTop = glm::max(fTop, xClip.y);
        fRight = glm::min(fRight, xClip.z);
        fBottom = glm::min(fBottom, xClip.w);
        if (fLeft >= fRight || fTop >= fBottom)
            return; // Fully clipped
    }

    float fWidth = fRight - fLeft;
    float fHeight = fBottom - fTop;

    Zenith_Maths::UVector4 xPositionSize = {
        static_cast<uint32_t>(fLeft),
        static_cast<uint32_t>(fTop),
        static_cast<uint32_t>(fWidth),
        static_cast<uint32_t>(fHeight)
    };

    Zenith_Maths::Vector2 xUVMultAdd = { 1.0f, 0.0f };
    Zenith_Maths::Vector2 xSizePixels = { fWidth, fHeight };

    Flux_QuadsImpl::Quad xQuad(xPositionSize, xColor, uTextureID, xUVMultAdd, fCornerRadius, xSizePixels, xGradientColor);
    g_xEngine.Quads().UploadQuad(xQuad);
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

    Flux_QuadsImpl::Quad xQuad(xPositionSize, xColor, uTextureID, xUVMultAdd);
    g_xEngine.Quads().UploadQuad(xQuad);
}

void Zenith_UICanvas::SubmitText(const std::string& strText, const Zenith_Maths::Vector2& xPosition, float fSize, const Zenith_Maths::Vector4& xColor)
{
    if (strText.empty())
        return;

    // CPU-side clip: skip text entries fully outside the clip rect
    if (m_uClipRectStackDepth > 0)
    {
        const Zenith_Maths::Vector4& xClip = m_axClipRectStack[m_uClipRectStackDepth - 1];
        if (xPosition.x > xClip.z || xPosition.y > xClip.w ||
            xPosition.y + fSize < xClip.y)
        {
            return; // Fully outside clip region
        }
    }

    UITextEntry xEntry;
    xEntry.m_strText = strText;
    xEntry.m_xPosition = xPosition;
    xEntry.m_fSize = fSize;
    xEntry.m_xColor = xColor;
    xEntry.m_iSortOrder = m_iCurrentSortOrder;

    s_xPendingTextEntries.PushBack(xEntry);
}

// ========== Focus Navigation ==========

void Zenith_UICanvas::SetFocusedElement(Zenith_UIElement* pxElement)
{
	// Unfocus previous
	if (m_pxFocusedElement)
	{
		if (m_pxFocusedElement->GetType() == UIElementType::Button)
		{
			static_cast<Zenith_UIButton*>(m_pxFocusedElement)->SetFocused(false);
		}
		else if (m_pxFocusedElement->GetType() == UIElementType::Toggle)
		{
			static_cast<Zenith_UIToggle*>(m_pxFocusedElement)->SetFocused(false);
		}
	}

	m_pxFocusedElement = pxElement;

	// Focus new
	if (m_pxFocusedElement)
	{
		if (m_pxFocusedElement->GetType() == UIElementType::Button)
		{
			static_cast<Zenith_UIButton*>(m_pxFocusedElement)->SetFocused(true);
		}
		else if (m_pxFocusedElement->GetType() == UIElementType::Toggle)
		{
			static_cast<Zenith_UIToggle*>(m_pxFocusedElement)->SetFocused(true);
		}
	}
}

void Zenith_UICanvas::CollectFocusableElements(Zenith_UIElement* pxElement, Zenith_UIElement** apxOut, uint32_t& uCount, uint32_t uMax) const
{
	if (!pxElement || !pxElement->IsVisible() || uCount >= uMax)
		return;

	if (pxElement->IsFocusable() && pxElement->IsGroupInteractable())
	{
		apxOut[uCount++] = pxElement;
	}

	const Zenith_Vector<Zenith_UIElement*>& xChildren = pxElement->GetChildren();
	for (uint32_t u = 0; u < xChildren.GetSize() && uCount < uMax; u++)
	{
		CollectFocusableElements(xChildren.Get(u), apxOut, uCount, uMax);
	}
}

Zenith_UIElement* Zenith_UICanvas::FindNearestFocusable(Zenith_UIElement* pxFrom, float fDirX, float fDirY) const
{
	if (!pxFrom)
		return nullptr;

	static constexpr uint32_t uMAX_FOCUSABLE = 256;
	Zenith_UIElement* apxFocusable[uMAX_FOCUSABLE];
	uint32_t uCount = 0;

	for (uint32_t u = 0; u < m_xRootElements.GetSize(); u++)
	{
		CollectFocusableElements(m_xRootElements.Get(u), apxFocusable, uCount, uMAX_FOCUSABLE);
	}

	Zenith_Maths::Vector4 xFromBounds = pxFrom->GetScreenBounds();
	float fFromCX = (xFromBounds.x + xFromBounds.z) * 0.5f;
	float fFromCY = (xFromBounds.y + xFromBounds.w) * 0.5f;

	Zenith_UIElement* pxBest = nullptr;
	float fBestScore = 1e18f;

	for (uint32_t u = 0; u < uCount; u++)
	{
		Zenith_UIElement* pxCandidate = apxFocusable[u];
		if (pxCandidate == pxFrom)
			continue;

		Zenith_Maths::Vector4 xCandBounds = pxCandidate->GetScreenBounds();
		float fCandCX = (xCandBounds.x + xCandBounds.z) * 0.5f;
		float fCandCY = (xCandBounds.y + xCandBounds.w) * 0.5f;

		float fDX = fCandCX - fFromCX;
		float fDY = fCandCY - fFromCY;

		// Dot product with direction — must be positive (candidate is in desired direction)
		float fDot = fDX * fDirX + fDY * fDirY;
		if (fDot <= 0.f)
			continue;

		// Score: distance, weighted to prefer aligned elements
		float fDist = fDX * fDX + fDY * fDY;
		if (fDist < fBestScore)
		{
			fBestScore = fDist;
			pxBest = pxCandidate;
		}
	}

	return pxBest;
}

void Zenith_UICanvas::NavigateUp()
{
	if (!m_pxFocusedElement)
		return;

	Zenith_UIElement* pxNext = m_pxFocusedElement->GetNavUp();
	if (!pxNext)
	{
		pxNext = FindNearestFocusable(m_pxFocusedElement, 0.f, -1.f);
	}

	if (pxNext && pxNext->IsVisible() && pxNext->IsFocusable())
	{
		SetFocusedElement(pxNext);
	}
}

void Zenith_UICanvas::NavigateDown()
{
	if (!m_pxFocusedElement)
		return;

	Zenith_UIElement* pxNext = m_pxFocusedElement->GetNavDown();
	if (!pxNext)
	{
		pxNext = FindNearestFocusable(m_pxFocusedElement, 0.f, 1.f);
	}

	if (pxNext && pxNext->IsVisible() && pxNext->IsFocusable())
	{
		SetFocusedElement(pxNext);
	}
}

void Zenith_UICanvas::NavigateLeft()
{
	if (!m_pxFocusedElement)
		return;

	Zenith_UIElement* pxNext = m_pxFocusedElement->GetNavLeft();
	if (!pxNext)
	{
		pxNext = FindNearestFocusable(m_pxFocusedElement, -1.f, 0.f);
	}

	if (pxNext && pxNext->IsVisible() && pxNext->IsFocusable())
	{
		SetFocusedElement(pxNext);
	}
}

void Zenith_UICanvas::NavigateRight()
{
	if (!m_pxFocusedElement)
		return;

	Zenith_UIElement* pxNext = m_pxFocusedElement->GetNavRight();
	if (!pxNext)
	{
		pxNext = FindNearestFocusable(m_pxFocusedElement, 1.f, 0.f);
	}

	if (pxNext && pxNext->IsVisible() && pxNext->IsFocusable())
	{
		SetFocusedElement(pxNext);
	}
}

void Zenith_UICanvas::ActivateFocused()
{
	if (!m_pxFocusedElement)
		return;

	if (m_pxFocusedElement->GetType() == UIElementType::Button)
	{
		Zenith_UIButton* pxButton = static_cast<Zenith_UIButton*>(m_pxFocusedElement);
		pxButton->Activate();
	}
	else if (m_pxFocusedElement->GetType() == UIElementType::Toggle)
	{
		Zenith_UIToggle* pxToggle = static_cast<Zenith_UIToggle*>(m_pxFocusedElement);
		pxToggle->SetIsOn(!pxToggle->IsOn());
	}
}

// Recursive helper: write element + children tree
static void WriteElementTree(const Zenith_UIElement* pxElement, Zenith_DataStream& xStream)
{
    xStream << static_cast<uint32_t>(pxElement->GetType());
    pxElement->WriteToDataStream(xStream);

    uint32_t uNumChildren = static_cast<uint32_t>(pxElement->GetChildCount());
    xStream << uNumChildren;

    for (size_t i = 0; i < uNumChildren; i++)
    {
        Zenith_UIElement* pxChild = pxElement->GetChild(i);
        if (pxChild)
        {
            WriteElementTree(pxChild, xStream);
        }
    }
}

// Recursive helper: read element + children tree
static Zenith_UIElement* ReadElementTree(Zenith_DataStream& xStream, Zenith_Vector<Zenith_UIElement*>& xAllElements)
{
    uint32_t uType;
    xStream >> uType;

    UIElementType eType = static_cast<UIElementType>(uType);
    Zenith_UIElement* pxElement = Zenith_UIElement::CreateFromType(eType);

    if (pxElement)
    {
        pxElement->ReadFromDataStream(xStream);
        xAllElements.PushBack(pxElement);

        uint32_t uNumChildren;
        xStream >> uNumChildren;

        for (uint32_t c = 0; c < uNumChildren; c++)
        {
            Zenith_UIElement* pxChild = ReadElementTree(xStream, xAllElements);
            if (pxChild)
            {
                pxElement->AddChild(pxChild);
            }
        }
    }

    return pxElement;
}

void Zenith_UICanvas::WriteToDataStream(Zenith_DataStream& xStream) const
{
    xStream << UI_CANVAS_VERSION;

    uint32_t uNumElements = static_cast<uint32_t>(m_xRootElements.GetSize());
    xStream << uNumElements;

    for (Zenith_Vector<Zenith_UIElement*>::Iterator xIt(m_xRootElements); !xIt.Done(); xIt.Next())
    {
        const Zenith_UIElement* pxElement = xIt.GetData();
        if (pxElement)
        {
            WriteElementTree(pxElement, xStream);
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
        Zenith_UIElement* pxElement = ReadElementTree(xStream, m_xAllElements);
        if (pxElement)
        {
            pxElement->m_pxCanvas = this;
            m_xRootElements.PushBack(pxElement);
        }
    }

    // Set canvas pointer on all elements (AddChild propagates from parent,
    // but root elements need it set first, then propagate to children)
    for (uint32_t i = 0; i < m_xAllElements.GetSize(); i++)
    {
        m_xAllElements.Get(i)->m_pxCanvas = this;
    }
}

} // namespace Zenith_UI
