#pragma once

#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIStyle.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
#include <string>

class Zenith_TextureAsset;

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
 *   - UIStyle support (shadow, rounded corners, border)
 */

namespace Zenith_UI {

class Zenith_UIImage : public Zenith_UIElement
{
public:
    Zenith_UIImage(const std::string& strName = "UIImage");
    virtual ~Zenith_UIImage();

    virtual UIElementType GetType() const override { return UIElementType::Image; }

    // ========== Texture ==========

    void SetTexturePath(const std::string& strPath);
    const std::string& GetTexturePath() const { return m_xTexture.GetPath(); }

    void SetTexture(Zenith_TextureAsset* pxTexture) { m_xTexture.Set(pxTexture); }
    Zenith_TextureAsset* GetTexture() const { return m_xTexture.Get(); }

    TextureHandle& GetTextureHandle() { return m_xTexture; }

    // ========== UV Coordinates ==========

    void SetUVMin(const Zenith_Maths::Vector2& xUV) { m_xUVMin = xUV; }
    Zenith_Maths::Vector2 GetUVMin() const { return m_xUVMin; }

    void SetUVMax(const Zenith_Maths::Vector2& xUV) { m_xUVMax = xUV; }
    Zenith_Maths::Vector2 GetUVMax() const { return m_xUVMax; }

    void SetSpriteSheetFrame(int iCol, int iRow, int iTotalCols, int iTotalRows);

    // ========== Style ==========

    void SetStyle(const UIStyle& xStyle) { m_xStyle = xStyle; }
    const UIStyle& GetStyle() const { return m_xStyle; }

    void SetShadowEnabled(bool bEnabled) { m_xStyle.m_bShadowEnabled = bEnabled; }
    void SetShadowColor(const Zenith_Maths::Vector4& xColor) { m_xStyle.m_xShadowColor = xColor; }
    void SetShadowOffset(const Zenith_Maths::Vector2& xOffset) { m_xStyle.m_xShadowOffset = xOffset; }
    void SetShadowSpread(float fSpread) { m_xStyle.m_fShadowSpread = fSpread; }
    void SetCornerRadius(float fRadius) { m_xStyle.m_fCornerRadius = fRadius; }

    // ========== Overrides ==========

    virtual void Render(Zenith_UICanvas& xCanvas) override;
    virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
    virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
    virtual void RenderPropertiesPanel() override;
#endif

private:
    void LoadTexture();

    TextureHandle m_xTexture;

    Zenith_Maths::Vector2 m_xUVMin = { 0.0f, 0.0f };
    Zenith_Maths::Vector2 m_xUVMax = { 1.0f, 1.0f };

    UIStyle m_xStyle;
};

} // namespace Zenith_UI
