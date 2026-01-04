#pragma once

#include "UI/Zenith_UIElement.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
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

// Text entry for batch submission to Flux_Text
struct UITextEntry
{
    std::string m_strText;
    Zenith_Maths::Vector2 m_xPosition;
    float m_fSize;
    Zenith_Maths::Vector4 m_xColor;
};

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

    // ========== Rendering Interface ==========

    // Submit a quad (called by UI elements)
    void SubmitQuad(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID = 0);
    void SubmitQuadWithUV(const Zenith_Maths::Vector4& xBounds, const Zenith_Maths::Vector4& xColor, uint32_t uTextureID, const Zenith_Maths::Vector2& xUVMin, const Zenith_Maths::Vector2& xUVMax);

    // Submit text (called by UI elements, batched and sent to Flux_Text)
    void SubmitText(const std::string& strText, const Zenith_Maths::Vector2& xPosition, float fSize, const Zenith_Maths::Vector4& xColor = {1,1,1,1});

    // Get pending text entries (for Flux_Text to process)
    static Zenith_Vector<UITextEntry>& GetPendingTextEntries() { return s_xPendingTextEntries; }
    static void ClearPendingTextEntries() { s_xPendingTextEntries.Clear(); }

    // ========== Serialization ==========

    void WriteToDataStream(Zenith_DataStream& xStream) const;
    void ReadFromDataStream(Zenith_DataStream& xStream);

private:
    Zenith_UIElement* FindElementRecursive(Zenith_UIElement* pxElement, const std::string& strName) const;
    void UpdateSize();

    // All elements owned by canvas (flat list for ownership/deletion)
    Zenith_Vector<Zenith_UIElement*> m_xAllElements;

    // Root elements (top-level, not children of other elements)
    Zenith_Vector<Zenith_UIElement*> m_xRootElements;

    // Canvas dimensions
    Zenith_Maths::Vector2 m_xSize = { 1920.0f, 1080.0f };
    Zenith_Maths::Vector2 m_xReferenceResolution = { 1920.0f, 1080.0f };
    float m_fScaleFactor = 1.0f;

    // Primary canvas
    static Zenith_UICanvas* s_pxPrimaryCanvas;

    // Static text entries for Flux_Text integration
    static Zenith_Vector<UITextEntry> s_xPendingTextEntries;
};

} // namespace Zenith_UI
