#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_ModelComponent.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_ModelInstance.h"
#include <filesystem>
#include <unordered_map>

// Windows file dialog support
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#endif

//=============================================================================
// ModelComponent Editor UI
//
// Editor-only code for the model component properties panel.
// Separated from runtime code to improve maintainability.
//=============================================================================

//-----------------------------------------------------------------------------
// Helper: Assign a texture file to a material slot
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, Zenith_Editor_MaterialUI::TextureSlotType eSlot)
{
	using namespace Zenith_Editor_MaterialUI;

	// Validate texture can be loaded via registry
	Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(szFilePath);
	if (!pxTexture)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to load texture: %s", szFilePath);
		return;
	}

	Zenith_Log(LOG_CATEGORY_MESH, "Loaded texture from: %s", szFilePath);

	Zenith_MaterialAsset* pxOldMaterial = m_pxModelInstance ? m_pxModelInstance->GetMaterial(uMeshIdx) : nullptr;

	Zenith_MaterialAsset* pxNewMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	if (!pxNewMaterial)
	{
		Zenith_Error(LOG_CATEGORY_MATERIAL, "Failed to create new material instance");
		return;
	}
	pxNewMaterial->SetName("Material_" + std::to_string(uMeshIdx));

	Zenith_Log(LOG_CATEGORY_MATERIAL, "Created new material instance");

	if (pxOldMaterial)
	{
		if (!pxOldMaterial->GetDiffuseTexturePath().empty())
			pxNewMaterial->SetDiffuseTexturePath(pxOldMaterial->GetDiffuseTexturePath());
		if (!pxOldMaterial->GetNormalTexturePath().empty())
			pxNewMaterial->SetNormalTexturePath(pxOldMaterial->GetNormalTexturePath());
		if (!pxOldMaterial->GetRoughnessMetallicTexturePath().empty())
			pxNewMaterial->SetRoughnessMetallicTexturePath(pxOldMaterial->GetRoughnessMetallicTexturePath());
		if (!pxOldMaterial->GetOcclusionTexturePath().empty())
			pxNewMaterial->SetOcclusionTexturePath(pxOldMaterial->GetOcclusionTexturePath());
		if (!pxOldMaterial->GetEmissiveTexturePath().empty())
			pxNewMaterial->SetEmissiveTexturePath(pxOldMaterial->GetEmissiveTexturePath());

		pxNewMaterial->SetBaseColor(pxOldMaterial->GetBaseColor());
	}

	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		pxNewMaterial->SetDiffuseTexturePath(szFilePath);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set diffuse texture");
		break;
	case TEXTURE_SLOT_NORMAL:
		pxNewMaterial->SetNormalTexturePath(szFilePath);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set normal texture");
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		pxNewMaterial->SetRoughnessMetallicTexturePath(szFilePath);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set roughness/metallic texture");
		break;
	case TEXTURE_SLOT_OCCLUSION:
		pxNewMaterial->SetOcclusionTexturePath(szFilePath);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set occlusion texture");
		break;
	case TEXTURE_SLOT_EMISSIVE:
		pxNewMaterial->SetEmissiveTexturePath(szFilePath);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set emissive texture");
		break;
	}

	if (m_pxModelInstance)
	{
		m_pxModelInstance->SetMaterial(uMeshIdx, pxNewMaterial);
	}
}

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI. Delegates to per-section helpers so
// each block of the properties panel stays focused on its own concern.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::Separator();
	RenderModelStatusSection();

	ImGui::Separator();
	RenderModelDropTargetSection();

	RenderManualLoadSection();

	ImGui::Separator();
	RenderModelInstanceMaterialsSection();
}

//-----------------------------------------------------------------------------
// Shows a short status summary (path, mesh count, skeleton flag) for the
// currently-loaded model, or "No model loaded" when the component is empty.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderModelStatusSection()
{
	if (m_pxModelInstance)
	{
		ImGui::Text("Model Path: %s", m_strModelPath.c_str());
		ImGui::Text("Meshes: %u", m_pxModelInstance->GetNumMeshes());
		ImGui::Text("Has Skeleton: %s", m_pxModelInstance->HasSkeleton() ? "Yes" : "No");
	}
	else
	{
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No model loaded");
	}

	ImGui::Text("Physics Mesh: %s", HasPhysicsMesh() ? "Generated" : "None");

	if (!HasPhysicsMesh())
	{
		ImGui::BeginDisabled();
	}

	ImGui::Checkbox("Draw Debug Physics Mesh", &m_bDebugDrawPhysicsMesh);

	if (!HasPhysicsMesh())
	{
		ImGui::EndDisabled();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Shown only in stopped editor mode.");
	}
}

//-----------------------------------------------------------------------------
// Button-shaped drop target for .zmodel files plus a Clear action. Accepts
// DRAGDROP_PAYLOAD_MODEL drags from the content browser.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderModelDropTargetSection()
{
	ImGui::Text("Model:");
	ImGui::SameLine();

	std::string strModelName = "(none)";
	if (m_pxModelInstance && !m_strModelPath.empty())
	{
		std::filesystem::path xPath(m_strModelPath);
		strModelName = xPath.filename().string();
	}

	ImVec2 xButtonSize(200, 20);
	ImGui::Button(strModelName.c_str(), xButtonSize);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MODEL))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			Zenith_Log(LOG_CATEGORY_MESH, "Model dropped: %s", pFilePayload->m_szFilePath);
			LoadModel(pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Drop a .zmodel file here to load it");
	}

	ImGui::SameLine();
	if (ImGui::Button("Clear##ClearModel"))
	{
		ClearModel();
	}
}

//-----------------------------------------------------------------------------
// Fallback for loading a model by typing its path manually. Kept as a
// TreeNode so it stays collapsed by default.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderManualLoadSection()
{
	if (!ImGui::TreeNode("Load Model (Manual Path)"))
		return;

	static char s_szModelPath[512] = "";
	ImGui::InputText("Model Path", s_szModelPath, sizeof(s_szModelPath));

	if (ImGui::Button("Load Model"))
	{
		if (strlen(s_szModelPath) > 0)
		{
			LoadModel(s_szModelPath);
		}
	}

	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// Per-mesh material UI for the new Flux_ModelInstance system. Each mesh is
// wrapped in its own TreeNode with an "Edit" shortcut to the material editor.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderModelInstanceMaterialsSection()
{
	if (!m_pxModelInstance)
		return;

	ImGui::Text("Materials (%u meshes):", m_pxModelInstance->GetNumMeshes());
	for (uint32_t uMeshIdx = 0; uMeshIdx < m_pxModelInstance->GetNumMeshes(); ++uMeshIdx)
	{
		ImGui::PushID(uMeshIdx);

		Zenith_MaterialAsset* pxMaterial = m_pxModelInstance->GetMaterial(uMeshIdx);
		if (pxMaterial)
		{
			// Material header with name and edit button
			bool bExpanded = ImGui::TreeNode("Material", "Mesh %u: %s", uMeshIdx, pxMaterial->GetName().c_str());

			// Button to select in Material Editor
			ImGui::SameLine();
			if (ImGui::SmallButton("Edit"))
			{
				Zenith_Editor::SelectMaterial(pxMaterial);
			}

			if (bExpanded)
			{
				// Material properties (using shared utility)
				Zenith_Editor_MaterialUI::RenderMaterialProperties(pxMaterial, "ModelInstance");

				ImGui::Separator();

				RenderMeshMaterialSlots(uMeshIdx, *pxMaterial);

				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::Text("Mesh %u: (no material)", uMeshIdx);
		}

		ImGui::PopID();
	}
}

//-----------------------------------------------------------------------------
// Renders the five texture slots (diffuse/normal/RM/occlusion/emissive) for a
// single mesh index, wiring each slot's assignment callback back through
// AssignTextureToSlot so drops create a new material instance correctly.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderMeshMaterialSlots(uint32_t uMeshIdx, Zenith_MaterialAsset& xMaterial)
{
	using namespace Zenith_Editor_MaterialUI;
	auto fnAssign = [this, uMeshIdx](TextureSlotType eSlot) {
		return [this, uMeshIdx, eSlot](const char* szPath) {
			AssignTextureToSlot(szPath, uMeshIdx, eSlot);
		};
	};
	RenderTextureSlot("Diffuse", xMaterial, TEXTURE_SLOT_DIFFUSE, 48.0f, fnAssign(TEXTURE_SLOT_DIFFUSE));
	RenderTextureSlot("Normal", xMaterial, TEXTURE_SLOT_NORMAL, 48.0f, fnAssign(TEXTURE_SLOT_NORMAL));
	RenderTextureSlot("Roughness/Metallic", xMaterial, TEXTURE_SLOT_ROUGHNESS_METALLIC, 48.0f, fnAssign(TEXTURE_SLOT_ROUGHNESS_METALLIC));
	RenderTextureSlot("Occlusion", xMaterial, TEXTURE_SLOT_OCCLUSION, 48.0f, fnAssign(TEXTURE_SLOT_OCCLUSION));
	RenderTextureSlot("Emissive", xMaterial, TEXTURE_SLOT_EMISSIVE, 48.0f, fnAssign(TEXTURE_SLOT_EMISSIVE));
}

#endif // ZENITH_TOOLS
