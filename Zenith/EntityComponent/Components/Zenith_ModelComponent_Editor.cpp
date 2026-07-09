#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_ModelComponent.h"
#include "imgui.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_DragDropPayloads.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ModelInstance.h"
#include <filesystem>

//=============================================================================
// ModelComponent Editor UI
//
// Editor-only code for the model component properties panel. Every helper is a
// file-local static that reaches the component through its PUBLIC API, so the
// component header declares only RenderPropertiesPanel() and carries no ImGui /
// editor includes. (RenderPropertiesPanel itself stays a member so the editor
// registry can take &Zenith_ModelComponent::RenderPropertiesPanel.)
//=============================================================================

namespace
{
	//-------------------------------------------------------------------------
	// Assign a texture file to a material slot (creates a new material instance
	// so the drop doesn't mutate a shared asset).
	//-------------------------------------------------------------------------
	void AssignTextureToSlot(Zenith_ModelComponent& xModel, const char* szFilePath, uint32_t uMeshIdx, Zenith_EditorMaterialUI::TextureSlotType eSlot)
	{
		// Bring TextureSlotType enumerators into local scope so the `case TEXTURE_SLOT_X:`
		// labels below work without qualification.
		using enum Zenith_EditorMaterialUI::TextureSlotType;

		// Validate texture can be loaded via registry
		Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(szFilePath);
		if (!pxTexture)
		{
			Zenith_Error(LOG_CATEGORY_MESH, "Failed to load texture: %s", szFilePath);
			return;
		}

		Zenith_Log(LOG_CATEGORY_MESH, "Loaded texture from: %s", szFilePath);

		Flux_ModelInstance* pxInstance = xModel.GetModelInstance();
		Zenith_MaterialAsset* pxOldMaterial = pxInstance ? pxInstance->GetMaterial(uMeshIdx) : nullptr;

		auto xhNewMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxNewMaterial = xhNewMaterial.GetDirect();
		if (!pxNewMaterial)
		{
			Zenith_Error(LOG_CATEGORY_MATERIAL, "Failed to create new material instance");
			return;
		}
		pxNewMaterial->SetName("Material_" + std::to_string(uMeshIdx));

		Zenith_Log(LOG_CATEGORY_MATERIAL, "Created new material instance");

		if (pxOldMaterial)
		{
			pxNewMaterial->SetDiffuseTexture(pxOldMaterial->GetDiffuseTextureHandle());
			pxNewMaterial->SetNormalTexture(pxOldMaterial->GetNormalTextureHandle());
			pxNewMaterial->SetRoughnessMetallicTexture(pxOldMaterial->GetRoughnessMetallicTextureHandle());
			pxNewMaterial->SetOcclusionTexture(pxOldMaterial->GetOcclusionTextureHandle());
			pxNewMaterial->SetEmissiveTexture(pxOldMaterial->GetEmissiveTextureHandle());

			pxNewMaterial->SetBaseColor(pxOldMaterial->GetBaseColor());
		}

		TextureHandle xPickedHandle{std::string(szFilePath)};
		switch (eSlot)
		{
		case TEXTURE_SLOT_DIFFUSE:
			pxNewMaterial->SetDiffuseTexture(std::move(xPickedHandle));
			Zenith_Log(LOG_CATEGORY_MATERIAL, "Set diffuse texture");
			break;
		case TEXTURE_SLOT_NORMAL:
			pxNewMaterial->SetNormalTexture(std::move(xPickedHandle));
			Zenith_Log(LOG_CATEGORY_MATERIAL, "Set normal texture");
			break;
		case TEXTURE_SLOT_ROUGHNESS_METALLIC:
			pxNewMaterial->SetRoughnessMetallicTexture(std::move(xPickedHandle));
			Zenith_Log(LOG_CATEGORY_MATERIAL, "Set roughness/metallic texture");
			break;
		case TEXTURE_SLOT_OCCLUSION:
			pxNewMaterial->SetOcclusionTexture(std::move(xPickedHandle));
			Zenith_Log(LOG_CATEGORY_MATERIAL, "Set occlusion texture");
			break;
		case TEXTURE_SLOT_EMISSIVE:
			pxNewMaterial->SetEmissiveTexture(std::move(xPickedHandle));
			Zenith_Log(LOG_CATEGORY_MATERIAL, "Set emissive texture");
			break;
		}

		if (pxInstance)
		{
			pxInstance->SetMaterial(uMeshIdx, pxNewMaterial);
		}
	}

	// Context for the texture-assign callback (engine convention: NO std::function).
	// The callback fires synchronously inside RenderTextureSlot, so a stack-local
	// context per slot is sufficient.
	struct ModelTextureAssignContext
	{
		Zenith_ModelComponent* m_pxComponent;
		uint32_t m_uMeshIdx;
		Zenith_EditorMaterialUI::TextureSlotType m_eSlot;
	};

	void ModelTextureAssign_OnAssign(void* pContext, const char* szFilePath)
	{
		const ModelTextureAssignContext* pxCtx =
			static_cast<const ModelTextureAssignContext*>(pContext);
		AssignTextureToSlot(*pxCtx->m_pxComponent, szFilePath, pxCtx->m_uMeshIdx, pxCtx->m_eSlot);
	}

	//-------------------------------------------------------------------------
	// Renders the five texture slots (diffuse/normal/RM/occlusion/emissive) for a
	// single mesh index, wiring each slot's assignment callback back through
	// AssignTextureToSlot so drops create a new material instance correctly.
	//-------------------------------------------------------------------------
	void RenderMeshMaterialSlots(Zenith_ModelComponent& xModel, uint32_t uMeshIdx, Zenith_MaterialAsset& xMaterial)
	{
		using MUI = Zenith_EditorMaterialUI;

		// One context per slot; the callback fires synchronously inside
		// RenderTextureSlot, so these stack locals outlive every invocation.
		ModelTextureAssignContext xDiffuseCtx   = { &xModel, uMeshIdx, MUI::TEXTURE_SLOT_DIFFUSE };
		ModelTextureAssignContext xNormalCtx    = { &xModel, uMeshIdx, MUI::TEXTURE_SLOT_NORMAL };
		ModelTextureAssignContext xRMCtx        = { &xModel, uMeshIdx, MUI::TEXTURE_SLOT_ROUGHNESS_METALLIC };
		ModelTextureAssignContext xOcclusionCtx = { &xModel, uMeshIdx, MUI::TEXTURE_SLOT_OCCLUSION };
		ModelTextureAssignContext xEmissiveCtx  = { &xModel, uMeshIdx, MUI::TEXTURE_SLOT_EMISSIVE };

		g_xEngine.EditorMaterialUI().RenderTextureSlot("Diffuse",            xMaterial, MUI::TEXTURE_SLOT_DIFFUSE,             48.0f, &ModelTextureAssign_OnAssign, &xDiffuseCtx);
		g_xEngine.EditorMaterialUI().RenderTextureSlot("Normal",             xMaterial, MUI::TEXTURE_SLOT_NORMAL,              48.0f, &ModelTextureAssign_OnAssign, &xNormalCtx);
		g_xEngine.EditorMaterialUI().RenderTextureSlot("Roughness/Metallic", xMaterial, MUI::TEXTURE_SLOT_ROUGHNESS_METALLIC,  48.0f, &ModelTextureAssign_OnAssign, &xRMCtx);
		g_xEngine.EditorMaterialUI().RenderTextureSlot("Occlusion",          xMaterial, MUI::TEXTURE_SLOT_OCCLUSION,           48.0f, &ModelTextureAssign_OnAssign, &xOcclusionCtx);
		g_xEngine.EditorMaterialUI().RenderTextureSlot("Emissive",           xMaterial, MUI::TEXTURE_SLOT_EMISSIVE,            48.0f, &ModelTextureAssign_OnAssign, &xEmissiveCtx);
	}

	//-------------------------------------------------------------------------
	// Short status summary (path, mesh count, skeleton flag), or "No model
	// loaded" when the component is empty.
	//-------------------------------------------------------------------------
	void RenderModelStatusSection(Zenith_ModelComponent& xModel)
	{
		if (Flux_ModelInstance* pxInstance = xModel.GetModelInstance())
		{
			ImGui::Text("Model Path: %s", xModel.GetModelPath().c_str());
			ImGui::Text("Meshes: %u", pxInstance->GetNumMeshes());
			ImGui::Text("Has Skeleton: %s", pxInstance->HasSkeleton() ? "Yes" : "No");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No model loaded");
		}
	}

	//-------------------------------------------------------------------------
	// Button-shaped drop target for .zmodel files plus a Clear action. Accepts
	// DRAGDROP_PAYLOAD_MODEL drags from the content browser.
	//-------------------------------------------------------------------------
	void RenderModelDropTargetSection(Zenith_ModelComponent& xModel)
	{
		ImGui::Text("Model:");
		ImGui::SameLine();

		std::string strModelName = "(none)";
		if (xModel.GetModelInstance() && !xModel.GetModelPath().empty())
		{
			std::filesystem::path xPath(xModel.GetModelPath());
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
				xModel.LoadModel(pFilePayload->m_szFilePath);
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
			xModel.ClearModel();
		}
	}

	//-------------------------------------------------------------------------
	// Fallback for loading a model by typing its path manually. Kept as a
	// TreeNode so it stays collapsed by default.
	//-------------------------------------------------------------------------
	void RenderManualLoadSection(Zenith_ModelComponent& xModel)
	{
		if (!ImGui::TreeNode("Load Model (Manual Path)"))
			return;

		static char s_szModelPath[512] = "";
		ImGui::InputText("Model Path", s_szModelPath, sizeof(s_szModelPath));

		if (ImGui::Button("Load Model"))
		{
			if (strlen(s_szModelPath) > 0)
			{
				xModel.LoadModel(s_szModelPath);
			}
		}

		ImGui::TreePop();
	}

	//-------------------------------------------------------------------------
	// Per-mesh material UI for the Flux_ModelInstance system. Each mesh is
	// wrapped in its own TreeNode with an "Edit" shortcut to the material editor.
	//-------------------------------------------------------------------------
	void RenderModelInstanceMaterialsSection(Zenith_ModelComponent& xModel)
	{
		Flux_ModelInstance* pxInstance = xModel.GetModelInstance();
		if (!pxInstance)
			return;

		ImGui::Text("Materials (%u meshes):", pxInstance->GetNumMeshes());
		for (uint32_t uMeshIdx = 0; uMeshIdx < pxInstance->GetNumMeshes(); ++uMeshIdx)
		{
			ImGui::PushID(uMeshIdx);

			Zenith_MaterialAsset* pxMaterial = pxInstance->GetMaterial(uMeshIdx);
			if (pxMaterial)
			{
				// Material header with name and edit button
				bool bExpanded = ImGui::TreeNode("Material", "Mesh %u: %s", uMeshIdx, pxMaterial->GetName().c_str());

				// Button to select in Material Editor
				ImGui::SameLine();
				if (ImGui::SmallButton("Edit"))
				{
					g_xEditorQuery.m_pfnSelectMaterial(pxMaterial);
				}

				if (bExpanded)
				{
					// Material properties (using shared utility)
					g_xEngine.EditorMaterialUI().RenderMaterialProperties(pxMaterial, "ModelInstance");

					ImGui::Separator();

					RenderMeshMaterialSlots(xModel, uMeshIdx, *pxMaterial);

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
}

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI. Stays a member (the editor registry
// binds &Zenith_ModelComponent::RenderPropertiesPanel); delegates to the
// file-local section helpers above.
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::Separator();
	RenderModelStatusSection(*this);

	ImGui::Separator();
	RenderModelDropTargetSection(*this);

	RenderManualLoadSection(*this);

	ImGui::Separator();
	RenderModelInstanceMaterialsSection(*this);
}

#endif // ZENITH_TOOLS
