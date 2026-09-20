#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor_MaterialUI.h"
#include "Zenith_Editor.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#include "imgui.h"

#include <filesystem>

//=============================================================================
// Texture Preview Cache
//=============================================================================

// Phase 5.5d: texture preview cache lives on Zenith_EditorMaterialUI
// held by Zenith_Engine.

//=============================================================================
// Implementation
//=============================================================================

// Was `namespace Zenith_Editor_MaterialUI { ... }` wrapping all of these
// free functions. After the namespace → class collapse, each function is
// a method on `Zenith_EditorMaterialUI` and needs the class qualifier
// explicitly.

// How many frames an entry may go undrawn before it is dropped. The panels that
// use this cache redraw every frame they are visible, so "not touched recently"
// means "not on screen" -- closing the Material Editor releases its pins shortly
// after, and a session that browses many materials does not accumulate them.
static constexpr u_int uTEXTURE_PREVIEW_EVICT_AFTER_FRAMES = 120;

bool Zenith_EditorMaterialUI::ShouldEvictPreviewEntry(u_int uNow, u_int uLastTouched)
{
	// Both operands are u_int and the subtraction stays u_int, so a wrapped
	// counter yields the true small age by modular arithmetic. Widening either
	// side (to u_int64, or to a signed type) breaks exactly that.
	return (uNow - uLastTouched) > uTEXTURE_PREVIEW_EVICT_AFTER_FRAMES;
}

Flux_ImGuiTextureHandle Zenith_EditorMaterialUI::GetOrCreateTexturePreviewHandle(Zenith_TextureAsset* pxTexture)
{
	if (!pxTexture || !pxTexture->m_xVRAMHandle.IsValid() || !pxTexture->m_xSRV.m_xImageViewHandle.IsValid())
	{
		return Flux_ImGuiTextureHandle();
	}

	Zenith_EditorMaterialUI& xSelf = g_xEngine.EditorMaterialUI();
	xSelf.SweepTexturePreviewCache();

	// The key is the PINNED asset address (see the header). Not the VRAM handle:
	// that is a recycled free-list index, so two different textures collide on it.
	const u_int64 ulKey = reinterpret_cast<u_int64>(pxTexture);
	const u_int64 ulImageViewHandle = pxTexture->m_xSRV.m_xImageViewHandle.AsUInt();
	const u_int uFrame = g_xEngine.Frame().GetFrameIndex();

	TexturePreviewCacheEntry* pxEntry = xSelf.m_xTexturePreviewCache.TryGet(ulKey);
	if (pxEntry != nullptr)
	{
		pxEntry->m_uLastTouchedFrame = uFrame;
		// Still meaningful with the pin in place: a reload REPLACES the GPU
		// resources of the SAME asset object (ReleaseGPU then a fresh upload), so
		// the pointer is unchanged while the view underneath it is not.
		if (pxEntry->m_ulImageViewHandle == ulImageViewHandle)
		{
			return pxEntry->m_xHandle;
		}
		Flux_ImGuiIntegration::UnregisterTexture(pxEntry->m_xHandle);
		pxEntry->m_xHandle = Flux_ImGuiIntegration::RegisterTexture(pxTexture->m_xSRV, g_xEngine.FluxGraphics().m_xClampSampler);
		pxEntry->m_ulImageViewHandle = ulImageViewHandle;
		return pxEntry->m_xHandle;
	}

	TexturePreviewCacheEntry xNew;
	// Set() AddRefs, which is the whole point: the descriptor set built below
	// outlives nothing, because the asset behind it cannot now be collected.
	xNew.m_xTexture.Set(pxTexture);
	xNew.m_xHandle = Flux_ImGuiIntegration::RegisterTexture(pxTexture->m_xSRV, g_xEngine.FluxGraphics().m_xClampSampler);
	xNew.m_ulImageViewHandle = ulImageViewHandle;
	xNew.m_uLastTouchedFrame = uFrame;

	xSelf.m_xTexturePreviewCache[ulKey] = xNew;
	return xNew.m_xHandle;
}

void Zenith_EditorMaterialUI::SweepTexturePreviewCache()
{
	Zenith_EditorMaterialUI& xSelf = g_xEngine.EditorMaterialUI();
	const u_int uFrame = g_xEngine.Frame().GetFrameIndex();
	if (uFrame == xSelf.m_uTexturePreviewSweepFrame)
	{
		return;   // at most one sweep per frame
	}
	xSelf.m_uTexturePreviewSweepFrame = uFrame;

	Zenith_HashMap<u_int64, TexturePreviewCacheEntry>& xCache = xSelf.m_xTexturePreviewCache;

	// Collect first: Zenith_HashMap's Iterator asserts on mid-iteration mutation.
	Zenith_Vector<u_int64> xExpired;
	for (Zenith_HashMap<u_int64, TexturePreviewCacheEntry>::Iterator xIt(xCache); !xIt.Done(); xIt.Next())
	{
		const TexturePreviewCacheEntry& xEntry = xIt.GetValue();
		if (ShouldEvictPreviewEntry(uFrame, xEntry.m_uLastTouchedFrame))
		{
			xExpired.PushBack(xIt.GetKey());
		}
	}

	for (u_int u = 0; u < xExpired.GetSize(); u++)
	{
		TexturePreviewCacheEntry* pxEntry = xCache.TryGet(xExpired.Get(u));
		if (pxEntry != nullptr && pxEntry->m_xHandle.IsValid())
		{
			Flux_ImGuiIntegration::UnregisterTexture(pxEntry->m_xHandle);
		}
		xCache.Remove(xExpired.Get(u));   // destroys the TextureHandle, releasing the pin
	}
}

void Zenith_EditorMaterialUI::ClearTexturePreviewCache()
{
	Zenith_HashMap<u_int64, TexturePreviewCacheEntry>& xCache = g_xEngine.EditorMaterialUI().m_xTexturePreviewCache;
	for (Zenith_HashMap<u_int64, TexturePreviewCacheEntry>::Iterator xIt(xCache); !xIt.Done(); xIt.Next())
	{
		Flux_ImGuiIntegration::UnregisterTexture(xIt.GetValue().m_xHandle);
	}
	// Clear() destroys every TextureHandle, releasing the pins with the handles.
	xCache.Clear();
}

std::string Zenith_EditorMaterialUI::GetTexturePathForSlot(const Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot)
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

void Zenith_EditorMaterialUI::SetTexturePathForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot, const std::string& strPath)
{
	TextureHandle xHandle(strPath);
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		xMaterial.SetDiffuseTexture(std::move(xHandle));
		break;
	case TEXTURE_SLOT_NORMAL:
		xMaterial.SetNormalTexture(std::move(xHandle));
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		xMaterial.SetRoughnessMetallicTexture(std::move(xHandle));
		break;
	case TEXTURE_SLOT_OCCLUSION:
		xMaterial.SetOcclusionTexture(std::move(xHandle));
		break;
	case TEXTURE_SLOT_EMISSIVE:
		xMaterial.SetEmissiveTexture(std::move(xHandle));
		break;
	}
}

Zenith_TextureAsset* Zenith_EditorMaterialUI::GetTextureForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot)
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

void Zenith_EditorMaterialUI::RenderMaterialProperties(Zenith_MaterialAsset* pxMaterial, const char* szIdSuffix)
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

// Resolve the display name + fully-qualified path of the texture currently bound
// to a material slot. Falls back to the texture's own path when the material
// slot path is empty (can happen for runtime-loaded textures).
static void ResolveTextureSlotDisplay(
	const Zenith_MaterialAsset& xMaterial,
	Zenith_EditorMaterialUI::TextureSlotType eSlot,
	std::string& strCurrentPathOut,
	std::string& strTextureNameOut,
	Zenith_TextureAsset*& pxCurrentTextureOut,
	bool& bHasTextureOut)
{
	strCurrentPathOut = g_xEngine.EditorMaterialUI().GetTexturePathForSlot(xMaterial, eSlot);
	pxCurrentTextureOut = g_xEngine.EditorMaterialUI().GetTextureForSlot(const_cast<Zenith_MaterialAsset&>(xMaterial), eSlot);

	if (strCurrentPathOut.empty() && pxCurrentTextureOut && !pxCurrentTextureOut->GetPath().empty())
	{
		strCurrentPathOut = pxCurrentTextureOut->GetPath();
	}

	bHasTextureOut = pxCurrentTextureOut && pxCurrentTextureOut->m_xVRAMHandle.IsValid();
	strTextureNameOut = "(none)";
	if (!bHasTextureOut)
		return;

	if (!strCurrentPathOut.empty())
	{
		std::filesystem::path xPath(strCurrentPathOut);
		strTextureNameOut = xPath.filename().string();
	}
	else
	{
		char szBuf[64];
		snprintf(szBuf, sizeof(szBuf), "(generated %ux%u)",
			pxCurrentTextureOut->m_xSurfaceInfo.m_uWidth,
			pxCurrentTextureOut->m_xSurfaceInfo.m_uHeight);
		strTextureNameOut = szBuf;
	}
}

// Handles the drag-drop target that wraps the texture preview button. Fires
// pfnOnAssign if supplied, otherwise applies the default SetTexturePathForSlot.
static void HandleTextureSlotDragDrop(
	const char* szLabel,
	Zenith_MaterialAsset& xMaterial,
	Zenith_EditorMaterialUI::TextureSlotType eSlot,
	Zenith_EditorMaterialUI::TextureAssignCallback pfnOnAssign,
	void* pOnAssignContext)
{
	if (!ImGui::BeginDragDropTarget())
		return;

	if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
	{
		const DragDropFilePayload* pFilePayload =
			static_cast<const DragDropFilePayload*>(pPayload->Data);

		if (pfnOnAssign)
		{
			pfnOnAssign(pOnAssignContext, pFilePayload->m_szFilePath);
		}
		else
		{
			g_xEngine.EditorMaterialUI().SetTexturePathForSlot(xMaterial, eSlot, pFilePayload->m_szFilePath);
		}
		Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialUI] Set %s texture: %s", szLabel, pFilePayload->m_szFilePath);
	}
	ImGui::EndDragDropTarget();
}

void Zenith_EditorMaterialUI::RenderTextureSlot(
	const char* szLabel,
	Zenith_MaterialAsset& xMaterial,
	TextureSlotType eSlot,
	float fPreviewSize,
	TextureAssignCallback pfnOnAssign,
	void* pOnAssignContext)
{
	ImGui::PushID(szLabel);

	std::string strCurrentPath, strTextureName;
	Zenith_TextureAsset* pxCurrentTexture = nullptr;
	bool bHasTexture = false;
	ResolveTextureSlotDisplay(xMaterial, eSlot, strCurrentPath, strTextureName, pxCurrentTexture, bHasTexture);

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

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
		ImGui::Button("...", ImVec2(fPreviewSize, fPreviewSize));
	}

	HandleTextureSlotDragDrop(szLabel, xMaterial, eSlot, pfnOnAssign, pOnAssignContext);

	if (ImGui::IsItemHovered())
	{
		if (bHasTexture)
		{
			if (!strCurrentPath.empty())
				ImGui::SetTooltip("%s\nPath: %s\nDrop a .ztxtr texture here to change", strTextureName.c_str(), strCurrentPath.c_str());
			else
				ImGui::SetTooltip("%s\nDrop a .ztxtr texture here to change", strTextureName.c_str());
		}
		else
		{
			ImGui::SetTooltip("No texture\nDrop a .ztxtr texture here");
		}
	}

	ImGui::PopID();
}

void Zenith_EditorMaterialUI::RenderAllTextureSlots(Zenith_MaterialAsset& xMaterial, bool /*bShowPreview*/)
{
	RenderTextureSlot("Diffuse", xMaterial, TEXTURE_SLOT_DIFFUSE);
	RenderTextureSlot("Normal", xMaterial, TEXTURE_SLOT_NORMAL);
	RenderTextureSlot("Roughness/Metallic", xMaterial, TEXTURE_SLOT_ROUGHNESS_METALLIC);
	RenderTextureSlot("Occlusion", xMaterial, TEXTURE_SLOT_OCCLUSION);
	RenderTextureSlot("Emissive", xMaterial, TEXTURE_SLOT_EMISSIVE);
}

// (namespace Zenith_Editor_MaterialUI closing brace removed — class collapse)
#ifdef ZENITH_TESTING
#include "Editor/Zenith_Editor_MaterialUI.Tests.inl"
#endif

#endif // ZENITH_TOOLS
