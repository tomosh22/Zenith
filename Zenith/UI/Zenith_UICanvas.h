#pragma once

#include "UI/Zenith_UIElement.h"
#include <string>

class Zenith_DataStream;

/**
 * Zenith_UICanvas - Root container for UI elements
 *
 * The canvas manages the UI element hierarchy and coordinates rendering.
 * It integrates with Flux rendering via Flux_Quads for images and Flux_Text for text.
 *
 * OWNERSHIP: Canvas owns all elements (including children of elements).
 * All elements must be allocated with 'new' and added via AddElement().
 * Canvas will delete all elements in destructor or Clear().
 */

namespace Zenith_UI {

class Zenith_UICanvas
{
public:
    Zenith_UICanvas();
    ~Zenith_UICanvas();

    // Prevent copying
    Zenith_UICanvas(const Zenith_UICanvas&) = delete;
    Zenith_UICanvas& operator=(const Zenith_UICanvas&) = delete;

    // Allow moving (for component pool swap-and-pop)
    Zenith_UICanvas(Zenith_UICanvas&& xOther);
    Zenith_UICanvas& operator=(Zenith_UICanvas&& xOther);

    // ========== Initialization ==========

    static void Initialise();
    static void Shutdown();

    // ========== Element Management ==========

    // Add element to canvas (canvas takes ownership)
    void AddElement(Zenith_UIElement* pxElement);

    // Remove element from canvas (deletes it)
    void RemoveElement(Zenith_UIElement* pxElement);

    // Clear all elements (deletes all)
    void Clear();

    // Get root elements (not including children)
    const Zenith_Vector<Zenith_UIElement*>& GetElements() const { return m_xRootElements; }
    size_t GetElementCount() const { return m_xRootElements.GetSize(); }

    // Find element by name (searches entire hierarchy)
    Zenith_UIElement* FindElement(const std::string& strName) const;

    // Move a child element under a new parent (removes from root elements if present)
    void ReparentElement(Zenith_UIElement* pxChild, Zenith_UIElement* pxNewParent);

    // ========== Frame Updates ==========

    void Update(float fDt);
    void Render();

    // ========== Canvas Properties ==========

    Zenith_Maths::Vector2 GetSize() const { return m_xSize; }

    void SetReferenceResolution(float fWidth, float fHeight);
    Zenith_Maths::Vector2 GetReferenceResolution() const { return m_xReferenceResolution; }

    float GetScaleFactor() const { return m_fScaleFactor; }

    // ========== Static Access ==========

    static Zenith_UICanvas* GetPrimaryCanvas() { return s_pxPrimaryCanvas; }
    static void SetPrimaryCanvas(Zenith_UICanvas* pxCanvas) { s_pxPrimaryCanvas = pxCanvas; }

#ifdef ZENITH_INPUT_SIMULATOR
    // ========== Automation Helper ==========
    //
    // Resolves a named element on the primary canvas to its screen center and
    // simulates a left click there via Zenith_InputSimulator::SimulateMouseClick.
    // Relocated here from Zenith_InputSimulator so the L1 Input layer no longer
    // depends UP on UI; this UI-layer helper calls DOWN into Input legitimately.
    // NOTE: SimulateMouseClick ticks a frame (StepFrame) internally, so this is
    // NOT safe to call reentrantly from inside an AutomatedTest Step — see the
    // inline-replicated equivalents in the game test suites.
    static void SimulateClickOnUIElement(const char* szElementName);
#endif

    // ========== Focus Navigation ==========

    void SetFocusedElement(Zenith_UIElement* pxElement);
    Zenith_UIElement* GetFocusedElement() const { return m_pxFocusedElement; }

    void NavigateUp();
    void NavigateDown();
    void NavigateLeft();
    void NavigateRight();
    void ActivateFocused();

    // ========== Clip Rect Stack ==========

    void PushClipRect(const Zenith_Maths::Vector4& xBounds);
    void PopClipRect();
    bool HasActiveClipRect() const { return m_uClipRectStackDepth > 0; }
    Zenith_Maths::Vector4 GetActiveClipRect() const;

    // ========== Rendering Interface ==========

    // Submit a quad (called by UI elements)
    void SubmitQuad(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID = 0,
        float fCornerRadius = 0.0f, const Zenith_Maths::Vector4& xGradientColor = {-1,-1,-1,-1});
    void SubmitQuadWithUV(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID, const Zenith_Maths::Vector2& xUVMin, const Zenith_Maths::Vector2& xUVMax);

    // Submit text (called by UI elements, batched and sent to Flux_Text via the
    // Flux-owned queue in Flux/Text/Flux_TextQueue.h)
    void SubmitText(const std::string& strText, const Zenith_Maths::Vector2& xPosition, float fSize, const Zenith_Maths::Vector4& xColor = {1,1,1,1});

    // ========== Serialization ==========

    void WriteToDataStream(Zenith_DataStream& xStream) const;
    void ReadFromDataStream(Zenith_DataStream& xStream);

private:
    Zenith_UIElement* FindElementRecursive(Zenith_UIElement* pxElement, const std::string& strName) const;
    void UpdateSize();
    void UpdateFocusNavigation();

    // All elements owned by canvas (flat list for ownership/deletion)
    Zenith_Vector<Zenith_UIElement*> m_xAllElements;

    // Root elements (top-level, not children of other elements)
    Zenith_Vector<Zenith_UIElement*> m_xRootElements;

    // Canvas dimensions
    Zenith_Maths::Vector2 m_xSize = { 1920.0f, 1080.0f };
    Zenith_Maths::Vector2 m_xReferenceResolution = { 1920.0f, 1080.0f };
    float m_fScaleFactor = 1.0f;

    // Clip rect stack (CPU-side bounds clamping for scroll views)
    static constexpr uint32_t uMAX_CLIP_RECT_DEPTH = 8;
    Zenith_Maths::Vector4 m_axClipRectStack[uMAX_CLIP_RECT_DEPTH];
    uint32_t m_uClipRectStackDepth = 0;

    // Focus navigation
    Zenith_UIElement* m_pxFocusedElement = nullptr;
    void CollectFocusableElements(Zenith_UIElement* pxElement, Zenith_UIElement** apxOut, uint32_t& uCount, uint32_t uMax) const;
    Zenith_UIElement* FindNearestFocusable(Zenith_UIElement* pxFrom, float fDirX, float fDirY) const;

    // Current sort order for text entry tagging (set during Render loop)
    int m_iCurrentSortOrder = 0;

    // Primary canvas
    static Zenith_UICanvas* s_pxPrimaryCanvas;
};

} // namespace Zenith_UI
