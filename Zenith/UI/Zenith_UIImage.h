#pragma once

#include "UI/Zenith_UIElement.h"
#include "Maths/Zenith_Maths.h"
#include <string>

class Flux_Texture;

/**
 * Zenith_UIImage - Textured image widget
 *
 * Renders a texture/sprite for:
 *   - Icons (inventory items, abilities)
 *   - Backgrounds
 *   - Compass elements
 *   - Portraits
 *
 * Features:
 *   - Texture loading via path
 *   - UV coordinates for sprite sheets
 *   - Glow effect for highlighting selected items
 */

namespace Zenith_UI {

class Zenith_UIImage : public Zenith_UIElement
{
public:
    Zenith_UIImage(const std::string& strName = "UIImage");
    virtual ~Zenith_UIImage();

    virtual UIElementType GetType() const override { return UIElementType::Image; }

    // ========== Texture ==========

    // Set texture from path (loads texture if not already loaded)
    void SetTexturePath(const std::string& strPath);
    const std::string& GetTexturePath() const { return m_strTexturePath; }

    // Set texture directly (for textures already loaded elsewhere)
    void SetTexture(Flux_Texture* pxTexture) { m_pxTexture = pxTexture; }
    Flux_Texture* GetTexture() const { return m_pxTexture; }

    // ========== UV Coordinates ==========

    // UV min/max for sprite sheet support
    void SetUVMin(const Zenith_Maths::Vector2& xUV) { m_xUVMin = xUV; }
    Zenith_Maths::Vector2 GetUVMin() const { return m_xUVMin; }

    void SetUVMax(const Zenith_Maths::Vector2& xUV) { m_xUVMax = xUV; }
    Zenith_Maths::Vector2 GetUVMax() const { return m_xUVMax; }

    // Convenience for sprite sheets
    void SetSpriteSheetFrame(int iCol, int iRow, int iTotalCols, int iTotalRows);

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
    void LoadTexture();

    std::string m_strTexturePath;
    Flux_Texture* m_pxTexture = nullptr;
    bool m_bOwnsTexture = false;  // True if we loaded the texture ourselves

    // UV coordinates (default to full texture)
    Zenith_Maths::Vector2 m_xUVMin = { 0.0f, 0.0f };
    Zenith_Maths::Vector2 m_xUVMax = { 1.0f, 1.0f };

    // Glow effect
    bool m_bGlowEnabled = false;
    Zenith_Maths::Vector4 m_xGlowColor = { 1.0f, 1.0f, 0.0f, 0.5f };
    float m_fGlowSize = 8.0f;
};

} // namespace Zenith_UI
