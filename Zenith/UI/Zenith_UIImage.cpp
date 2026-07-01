#include "Zenith.h"
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_Buffers.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_IMAGE_VERSION = 2;

Zenith_UIImage::Zenith_UIImage(const std::string& strName)
    : Zenith_UIElement(strName)
{
}

Zenith_UIImage::~Zenith_UIImage()
{
}

Zenith_TextureAsset* Zenith_UIImage::GetTexture() const
{
    if (!m_xTexture.GetPath().empty())
        return Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xTexture.GetPath());
    return m_xTexture.GetDirect();
}

void Zenith_UIImage::SetTexturePath(const std::string& strPath)
{
    m_xTexture.SetPath(strPath);
    LoadTexture();
}

void Zenith_UIImage::LoadTexture()
{
    if (!m_xTexture.IsSet())
    {
        return;
    }

    Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xTexture.GetPath());

    if (pxTexture)
    {
        pxTexture->MarkAsBindless();
    }
    else
    {
        Zenith_Log(LOG_CATEGORY_UI, "[UIImage] Failed to load texture: %s", m_xTexture.GetPath().c_str());
    }
}

void Zenith_UIImage::SetSpriteSheetFrame(int iCol, int iRow, int iTotalCols, int iTotalRows)
{
    float fColWidth = 1.0f / iTotalCols;
    float fRowHeight = 1.0f / iTotalRows;

    m_xUVMin = { iCol * fColWidth, iRow * fRowHeight };
    m_xUVMax = { (iCol + 1) * fColWidth, (iRow + 1) * fRowHeight };
}

void Zenith_UIImage::Render(Zenith_UICanvas& xCanvas)
{
    if (!m_bVisible)
        return;

    float fAlpha = GetEffectiveAlpha();
    Zenith_Maths::Vector4 xBounds = GetScreenBounds();

    // Render shadow if enabled
    if (m_xStyle.m_bShadowEnabled)
    {
        Zenith_Maths::Vector4 xShadowBounds = {
            xBounds.x + m_xStyle.m_xShadowOffset.x - m_xStyle.m_fShadowSpread,
            xBounds.y + m_xStyle.m_xShadowOffset.y - m_xStyle.m_fShadowSpread,
            xBounds.z + m_xStyle.m_xShadowOffset.x + m_xStyle.m_fShadowSpread,
            xBounds.w + m_xStyle.m_xShadowOffset.y + m_xStyle.m_fShadowSpread
        };
        Zenith_Maths::Vector4 xShadowColor = m_xStyle.m_xShadowColor;
        xShadowColor.a *= fAlpha;
        xCanvas.SubmitQuad(xShadowBounds, xShadowColor, 0,
            m_xStyle.m_fCornerRadius + m_xStyle.m_fShadowSpread);
    }

    // Render the image with bindless texture
    uint32_t uTextureID = 0;
    Zenith_TextureAsset* pxTexture = !m_xTexture.GetPath().empty()
        ? Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xTexture.GetPath())
        : m_xTexture.GetDirect();
    if (pxTexture && pxTexture->IsValid() && pxTexture->m_xSRV.m_xImageViewHandle.IsValid())
    {
        Zenith_Assert(pxTexture->IsMarkedBindless(),
            "UIImage '%s': texture has a valid SRV but MarkAsBindless() was never called. "
            "The shader will sample an unregistered descriptor slot (silent transparent output). "
            "Call MarkAsBindless() after CreateFromData().", m_strName.c_str());
        // The bindless table index is the allocator-assigned slot, NOT the raw
        // image-view handle. Fall back to 0 (untextured sentinel) if unassigned.
        const u_int uIdx = pxTexture->m_xSRV.m_uBindlessIndex;
        uTextureID = (uIdx != uFLUX_INVALID_BINDLESS_INDEX) ? uIdx : 0u;
    }

    Zenith_Maths::Vector4 xColor = m_xColor;
    xColor.a *= fAlpha;
    xCanvas.SubmitQuadWithUV(xBounds, xColor, uTextureID, m_xUVMin, m_xUVMax);

    // Render children
    Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIImage::WriteToDataStream(Zenith_DataStream& xStream) const
{
    Zenith_UIElement::WriteToDataStream(xStream);

    xStream << UI_IMAGE_VERSION;
    std::string strTexturePath = Zenith_AssetRegistry::NormalizeAssetPath(m_xTexture.GetPath());
    xStream << strTexturePath;
    xStream << m_xUVMin.x; xStream << m_xUVMin.y;
    xStream << m_xUVMax.x; xStream << m_xUVMax.y;

    // UIStyle
    xStream << m_xStyle.m_bShadowEnabled;
    xStream << m_xStyle.m_xShadowColor.x; xStream << m_xStyle.m_xShadowColor.y; xStream << m_xStyle.m_xShadowColor.z; xStream << m_xStyle.m_xShadowColor.w;
    xStream << m_xStyle.m_xShadowOffset.x; xStream << m_xStyle.m_xShadowOffset.y;
    xStream << m_xStyle.m_fShadowSpread;
    xStream << m_xStyle.m_fCornerRadius;
}

void Zenith_UIImage::ReadFromDataStream(Zenith_DataStream& xStream)
{
    Zenith_UIElement::ReadFromDataStream(xStream);

    uint32_t uVersion;
    xStream >> uVersion;

    Zenith_Assert(uVersion == UI_IMAGE_VERSION, "UIImage version mismatch");

    std::string strTexturePath;
    xStream >> strTexturePath;
    m_xTexture.SetPath(Zenith_AssetRegistry::NormalizeAssetPath(strTexturePath));

    xStream >> m_xUVMin.x; xStream >> m_xUVMin.y;
    xStream >> m_xUVMax.x; xStream >> m_xUVMax.y;

    xStream >> m_xStyle.m_bShadowEnabled;
    xStream >> m_xStyle.m_xShadowColor.x; xStream >> m_xStyle.m_xShadowColor.y; xStream >> m_xStyle.m_xShadowColor.z; xStream >> m_xStyle.m_xShadowColor.w;
    xStream >> m_xStyle.m_xShadowOffset.x; xStream >> m_xStyle.m_xShadowOffset.y;
    xStream >> m_xStyle.m_fShadowSpread;
    xStream >> m_xStyle.m_fCornerRadius;

    LoadTexture();
}

#ifdef ZENITH_TOOLS
void Zenith_UIImage::RenderPropertiesPanel()
{
    Zenith_UIElement::RenderPropertiesPanel();

    ImGui::Separator();
    ImGui::Text("Image Properties");

    char szPathBuffer[512];
    const std::string& strTexturePath = m_xTexture.GetPath();
    strncpy_s(szPathBuffer, strTexturePath.c_str(), sizeof(szPathBuffer) - 1);
    if (ImGui::InputText("Texture Path", szPathBuffer, sizeof(szPathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        SetTexturePath(szPathBuffer);
    }

    Zenith_TextureAsset* pxTexture = GetTexture();
    if (pxTexture && pxTexture->IsValid())
    {
        ImGui::Text("Texture loaded: Yes");
    }
    else
    {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Texture not loaded");
    }

    ImGui::Separator();
    ImGui::Text("UV Coordinates");

    float fUVMin[2] = { m_xUVMin.x, m_xUVMin.y };
    if (ImGui::DragFloat2("UV Min", fUVMin, 0.01f, 0.0f, 1.0f))
    {
        m_xUVMin = { fUVMin[0], fUVMin[1] };
    }

    float fUVMax[2] = { m_xUVMax.x, m_xUVMax.y };
    if (ImGui::DragFloat2("UV Max", fUVMax, 0.01f, 0.0f, 1.0f))
    {
        m_xUVMax = { fUVMax[0], fUVMax[1] };
    }

    ImGui::Separator();
    ImGui::Text("Shadow");

    ImGui::Checkbox("Enable Shadow##Image", &m_xStyle.m_bShadowEnabled);
    if (m_xStyle.m_bShadowEnabled)
    {
        float fShadowColor[4] = { m_xStyle.m_xShadowColor.x, m_xStyle.m_xShadowColor.y, m_xStyle.m_xShadowColor.z, m_xStyle.m_xShadowColor.w };
        if (ImGui::ColorEdit4("Shadow Color##Image", fShadowColor))
        {
            m_xStyle.m_xShadowColor = { fShadowColor[0], fShadowColor[1], fShadowColor[2], fShadowColor[3] };
        }
        ImGui::DragFloat2("Shadow Offset##Image", &m_xStyle.m_xShadowOffset.x, 0.5f, -50.0f, 50.0f);
        ImGui::DragFloat("Shadow Spread##Image", &m_xStyle.m_fShadowSpread, 0.5f, 0.0f, 50.0f);
    }

    ImGui::DragFloat("Corner Radius##Image", &m_xStyle.m_fCornerRadius, 0.5f, 0.0f, 100.0f);
}
#endif

} // namespace Zenith_UI
