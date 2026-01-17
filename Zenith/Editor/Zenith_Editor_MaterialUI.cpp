#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor_MaterialUI.h"
#include "Zenith_Editor.h"
#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <filesystem>
#include <unordered_map>

//=============================================================================
// Texture Preview Cache
//=============================================================================

namespace
{
	struct TexturePreviewCacheEntry
	{
		Flux_ImGuiTextureHandle m_xHandle;
		u_int64 m_ulImageViewHandle = 0;  // Cached to detect changes
	};

	// Cache keyed by VRAM handle (unique per texture)
	static std::unordered_map<u_int64, TexturePreviewCacheEntry> s_xTexturePreviewCache;
}

//=============================================================================
// Implementation
//=============================================================================

namespace Zenith_Editor_MaterialUI
{

Flux_ImGuiTextureHandle GetOrCreateTexturePreviewHandle(const Zenith_TextureAsset* pxTexture)
{
	if (!pxTexture || !pxTexture->m_xVRAMHandle.IsValid() || !pxTexture->m_xSRV.m_xImageViewHandle.IsValid())
	{
		return Flux_ImGuiTextureHandle();
	}

	u_int64 ulKey = pxTexture->m_xVRAMHandle.AsUInt();
	u_int64 ulImageViewHandle = pxTexture->m_xSRV.m_xImageViewHandle.AsUInt();

	auto it = s_xTexturePreviewCache.find(ulKey);
	if (it != s_xTexturePreviewCache.end())
	{
		// Check if image view changed (e.g., texture was reloaded)
		if (it->second.m_ulImageViewHandle == ulImageViewHandle)
		{
			return it->second.m_xHandle;
		}
		// Image view changed - unregister old and create new
		Flux_ImGuiIntegration::UnregisterTexture(it->second.m_xHandle);
	}

	// Register new texture with ImGui
	Flux_ImGuiTextureHandle xHandle = Flux_ImGuiIntegration::RegisterTexture(
		pxTexture->m_xSRV,
		Flux_Graphics::s_xClampSampler
	);

	s_xTexturePreviewCache[ulKey] = { xHandle, ulImageViewHandle };
	return xHandle;
}

void ClearTexturePreviewCache()
{
	for (auto& xEntry : s_xTexturePreviewCache)
	{
		Flux_ImGuiIntegration::UnregisterTexture(xEntry.second.m_xHandle);
	}
	s_xTexturePreviewCache.clear();
}

std::string GetTexturePathForSlot(const Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot)
{
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		return xMaterial.GetDiffuseTexturePath();
	case TEXTURE_SLOT_NORMAL:
		return xMaterial.GetNormalTexturePath();
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		return xMaterial.GetRoughnessMetallicTexturePath();
	case TEXTURE_SLOT_OCCLUSION:
		return xMaterial.GetOcclusionTexturePath();
	case TEXTURE_SLOT_EMISSIVE:
		return xMaterial.GetEmissiveTexturePath();
	default:
		return "";
	}
}

void SetTexturePathForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot, const std::string& strPath)
{
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		xMaterial.SetDiffuseTexturePath(strPath);
		break;
	case TEXTURE_SLOT_NORMAL:
		xMaterial.SetNormalTexturePath(strPath);
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		xMaterial.SetRoughnessMetallicTexturePath(strPath);
		break;
	case TEXTURE_SLOT_OCCLUSION:
		xMaterial.SetOcclusionTexturePath(strPath);
		break;
	case TEXTURE_SLOT_EMISSIVE:
		xMaterial.SetEmissiveTexturePath(strPath);
		break;
	}
}

const Zenith_TextureAsset* GetTextureForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot)
{
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		return xMaterial.GetDiffuseTexture();
	case TEXTURE_SLOT_NORMAL:
		return xMaterial.GetNormalTexture();
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		return xMaterial.GetRoughnessMetallicTexture();
	case TEXTURE_SLOT_OCCLUSION:
		return xMaterial.GetOcclusionTexture();
	case TEXTURE_SLOT_EMISSIVE:
		return xMaterial.GetEmissiveTexture();
	default:
		return nullptr;
	}
}

void RenderMaterialProperties(Zenith_MaterialAsset* pxMaterial, const char* szIdSuffix)
{
	if (!pxMaterial)
		return;

	ImGui::PushID(szIdSuffix);

	// Basic properties
	Zenith_Maths::Vector4 xBaseColor = pxMaterial->GetBaseColor();
	float fColor[4] = { xBaseColor.x, xBaseColor.y, xBaseColor.z, xBaseColor.w };
	if (ImGui::ColorEdit4("Base Color", fColor))
	{
		pxMaterial->SetBaseColor({ fColor[0], fColor[1], fColor[2], fColor[3] });
	}

	float fMetallic = pxMaterial->GetMetallic();
	if (ImGui::SliderFloat("Metallic", &fMetallic, 0.0f, 1.0f))
	{
		pxMaterial->SetMetallic(fMetallic);
	}

	float fRoughness = pxMaterial->GetRoughness();
	if (ImGui::SliderFloat("Roughness", &fRoughness, 0.0f, 1.0f))
	{
		pxMaterial->SetRoughness(fRoughness);
	}

	// Emissive
	Zenith_Maths::Vector3 xEmissive = pxMaterial->GetEmissiveColor();
	float fEmissive[3] = { xEmissive.x, xEmissive.y, xEmissive.z };
	if (ImGui::ColorEdit3("Emissive Color", fEmissive))
	{
		pxMaterial->SetEmissiveColor({ fEmissive[0], fEmissive[1], fEmissive[2] });
	}

	float fEmissiveIntensity = pxMaterial->GetEmissiveIntensity();
	if (ImGui::SliderFloat("Emissive Intensity", &fEmissiveIntensity, 0.0f, 10.0f))
	{
		pxMaterial->SetEmissiveIntensity(fEmissiveIntensity);
	}

	ImGui::Separator();

	// Transparency
	bool bTransparent = pxMaterial->IsTransparent();
	if (ImGui::Checkbox("Transparent", &bTransparent))
	{
		pxMaterial->SetTransparent(bTransparent);
	}

	if (bTransparent)
	{
		float fAlphaCutoff = pxMaterial->GetAlphaCutoff();
		if (ImGui::SliderFloat("Alpha Cutoff", &fAlphaCutoff, 0.0f, 1.0f))
		{
			pxMaterial->SetAlphaCutoff(fAlphaCutoff);
		}
	}

	ImGui::Separator();

	// UV Controls
	Zenith_Maths::Vector2 xTiling = pxMaterial->GetUVTiling();
	float fTiling[2] = { xTiling.x, xTiling.y };
	if (ImGui::DragFloat2("UV Tiling", fTiling, 0.01f, 0.01f, 100.0f))
	{
		pxMaterial->SetUVTiling({ fTiling[0], fTiling[1] });
	}

	Zenith_Maths::Vector2 xOffset = pxMaterial->GetUVOffset();
	float fOffset[2] = { xOffset.x, xOffset.y };
	if (ImGui::DragFloat2("UV Offset", fOffset, 0.01f, -100.0f, 100.0f))
	{
		pxMaterial->SetUVOffset({ fOffset[0], fOffset[1] });
	}

	// Occlusion Strength
	float fOccStrength = pxMaterial->GetOcclusionStrength();
	if (ImGui::SliderFloat("Occlusion Strength", &fOccStrength, 0.0f, 1.0f))
	{
		pxMaterial->SetOcclusionStrength(fOccStrength);
	}

	ImGui::Separator();

	// Rendering flags
	bool bTwoSided = pxMaterial->IsTwoSided();
	if (ImGui::Checkbox("Two-Sided", &bTwoSided))
	{
		pxMaterial->SetTwoSided(bTwoSided);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Render both front and back faces");
	}

	bool bUnlit = pxMaterial->IsUnlit();
	if (ImGui::Checkbox("Unlit", &bUnlit))
	{
		pxMaterial->SetUnlit(bUnlit);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Skip lighting calculations");
	}

	ImGui::PopID();
}

void RenderTextureSlot(
	const char* szLabel,
	Zenith_MaterialAsset& xMaterial,
	TextureSlotType eSlot,
	bool bShowPreview,
	float fPreviewSize,
	TextureAssignCallback pfnOnAssign)
{
	ImGui::PushID(szLabel);

	std::string strCurrentPath = GetTexturePathForSlot(xMaterial, eSlot);
	const Zenith_TextureAsset* pxCurrentTexture = GetTextureForSlot(xMaterial, eSlot);

	// Fall back to texture's path if material path is empty
	// (happens when texture loaded directly without setting material texture path)
	if (strCurrentPath.empty() && pxCurrentTexture && !pxCurrentTexture->GetPath().empty())
	{
		strCurrentPath = pxCurrentTexture->GetPath();
	}

	std::string strTextureName = "(none)";
	bool bHasTexture = pxCurrentTexture && pxCurrentTexture->m_xVRAMHandle.IsValid();
	if (bHasTexture)
	{
		if (!strCurrentPath.empty())
		{
			std::filesystem::path xPath(strCurrentPath);
			strTextureName = xPath.filename().string();
		}
		else
		{
			// Runtime-generated texture - show dimensions for identification
			char szBuf[64];
			snprintf(szBuf, sizeof(szBuf), "(generated %ux%u)",
				pxCurrentTexture->m_xSurfaceInfo.m_uWidth,
				pxCurrentTexture->m_xSurfaceInfo.m_uHeight);
			strTextureName = szBuf;
		}
	}

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

	// Always show texture preview or placeholder button
	if (bHasTexture)
	{
		Flux_ImGuiTextureHandle xHandle = GetOrCreateTexturePreviewHandle(pxCurrentTexture);
		if (xHandle.IsValid())
		{
			ImGui::Image(
				(ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xHandle),
				ImVec2(fPreviewSize, fPreviewSize)
			);
		}
		else
		{
			// Texture exists but couldn't create preview handle
			ImGui::Button("[?]", ImVec2(fPreviewSize, fPreviewSize));
		}
	}
	else
	{
		// Empty slot - show placeholder button
		ImGui::Button("...", ImVec2(fPreviewSize, fPreviewSize));
	}

	// Drag-drop target
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			// Use custom callback if provided, otherwise use default behavior
			if (pfnOnAssign)
			{
				pfnOnAssign(pFilePayload->m_szFilePath);
			}
			else
			{
				SetTexturePathForSlot(xMaterial, eSlot, pFilePayload->m_szFilePath);
			}
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialUI] Set %s texture: %s", szLabel, pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	// Tooltip with texture name and path (shown on hover)
	if (ImGui::IsItemHovered())
	{
		if (bHasTexture)
		{
			if (!strCurrentPath.empty())
			{
				ImGui::SetTooltip("%s\nPath: %s\nDrop a .ztxtr texture here to change", strTextureName.c_str(), strCurrentPath.c_str());
			}
			else
			{
				ImGui::SetTooltip("%s\nDrop a .ztxtr texture here to change", strTextureName.c_str());
			}
		}
		else
		{
			ImGui::SetTooltip("No texture\nDrop a .ztxtr texture here");
		}
	}

	ImGui::PopID();
}

void RenderAllTextureSlots(Zenith_MaterialAsset& xMaterial, bool bShowPreview)
{
	RenderTextureSlot("Diffuse", xMaterial, TEXTURE_SLOT_DIFFUSE, bShowPreview);
	RenderTextureSlot("Normal", xMaterial, TEXTURE_SLOT_NORMAL, bShowPreview);
	RenderTextureSlot("Roughness/Metallic", xMaterial, TEXTURE_SLOT_ROUGHNESS_METALLIC, bShowPreview);
	RenderTextureSlot("Occlusion", xMaterial, TEXTURE_SLOT_OCCLUSION, bShowPreview);
	RenderTextureSlot("Emissive", xMaterial, TEXTURE_SLOT_EMISSIVE, bShowPreview);
}

} // namespace Zenith_Editor_MaterialUI

#endif // ZENITH_TOOLS
