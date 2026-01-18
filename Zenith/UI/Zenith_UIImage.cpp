#include "Zenith.h"
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UICanvas.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_Buffers.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_IMAGE_VERSION = 1;

Zenith_UIImage::Zenith_UIImage(const std::string& strName)
    : Zenith_UIElement(strName)
{
}

Zenith_UIImage::~Zenith_UIImage()
{
    // Texture lifetime managed by asset registry
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

    // Load texture via handle (handles caching and ref counting)
    Zenith_TextureAsset* pxTexture = m_xTexture.Get();

    if (!pxTexture)
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

    Zenith_Maths::Vector4 xBounds = GetScreenBounds();

    // Render glow effect first (behind image)
    if (m_bGlowEnabled && m_fGlowSize > 0.0f)
    {
        Zenith_Maths::Vector4 xGlowBounds = {
            xBounds.x - m_fGlowSize,
            xBounds.y - m_fGlowSize,
            xBounds.z + m_fGlowSize,
            xBounds.w + m_fGlowSize
        };
        xCanvas.SubmitQuad(xGlowBounds, m_xGlowColor, 0);
    }

    // Render the image
    // TODO: Implement proper texture ID retrieval from Flux_Texture
    uint32_t uTextureID = 0;
    // if (m_pxTexture)
    // {
    //     uTextureID = ???;  // Need to determine correct way to get texture ID
    // }

    // Apply color tint
    xCanvas.SubmitQuadWithUV(xBounds, m_xColor, uTextureID, m_xUVMin, m_xUVMax);

    // Render children
    Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIImage::WriteToDataStream(Zenith_DataStream& xStream) const
{
    // Write base class data
    Zenith_UIElement::WriteToDataStream(xStream);

    // Write image-specific data
    xStream << UI_IMAGE_VERSION;
    std::string strTexturePath = m_xTexture.GetPath();
    xStream << strTexturePath;
    xStream << m_xUVMin.x;
    xStream << m_xUVMin.y;
    xStream << m_xUVMax.x;
    xStream << m_xUVMax.y;
    xStream << m_bGlowEnabled;
    xStream << m_xGlowColor.x;
    xStream << m_xGlowColor.y;
    xStream << m_xGlowColor.z;
    xStream << m_xGlowColor.w;
    xStream << m_fGlowSize;
}

void Zenith_UIImage::ReadFromDataStream(Zenith_DataStream& xStream)
{
    // Read base class data
    Zenith_UIElement::ReadFromDataStream(xStream);

    // Read image-specific data
    uint32_t uVersion;
    xStream >> uVersion;

    std::string strTexturePath;
    xStream >> strTexturePath;
    m_xTexture.SetPath(strTexturePath);

    xStream >> m_xUVMin.x;
    xStream >> m_xUVMin.y;
    xStream >> m_xUVMax.x;
    xStream >> m_xUVMax.y;
    xStream >> m_bGlowEnabled;
    xStream >> m_xGlowColor.x;
    xStream >> m_xGlowColor.y;
    xStream >> m_xGlowColor.z;
    xStream >> m_xGlowColor.w;
    xStream >> m_fGlowSize;

    // Reload texture
    LoadTexture();
}

#ifdef ZENITH_TOOLS
void Zenith_UIImage::RenderPropertiesPanel()
{
    // Render base properties
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

    if (m_xTexture.IsLoaded())
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
    ImGui::Text("Glow Effect");

    ImGui::Checkbox("Enable Glow##Image", &m_bGlowEnabled);

    if (m_bGlowEnabled)
    {
        ImGui::DragFloat("Glow Size##Image", &m_fGlowSize, 0.5f, 0.0f, 50.0f);

        float fGlowColor[4] = { m_xGlowColor.x, m_xGlowColor.y, m_xGlowColor.z, m_xGlowColor.w };
        if (ImGui::ColorEdit4("Glow Color##Image", fGlowColor))
        {
            m_xGlowColor = { fGlowColor[0], fGlowColor[1], fGlowColor[2], fGlowColor[3] };
        }
    }
}
#endif

} // namespace Zenith_UI
