#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_ModelComponent.h"
#include "imgui.h"
#include "Editor/Zenith_Editor.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include <filesystem>

// Windows file dialog support
#ifdef _WIN32
#define NOMINMAX
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
// Helper: Render a texture slot with drag-drop support
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderTextureSlot(const char* szLabel, Flux_MaterialAsset& xMaterial, uint32_t uMeshIdx, TextureSlotType eSlot)
{
	ImGui::PushID(szLabel);

	const Flux_Texture* pxCurrentTexture = nullptr;
	std::string strCurrentPath;
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		pxCurrentTexture = xMaterial.GetDiffuseTexture();
		strCurrentPath = xMaterial.GetDiffuseTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_NORMAL:
		pxCurrentTexture = xMaterial.GetNormalTexture();
		strCurrentPath = xMaterial.GetNormalTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		pxCurrentTexture = xMaterial.GetRoughnessMetallicTexture();
		strCurrentPath = xMaterial.GetRoughnessMetallicTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_OCCLUSION:
		pxCurrentTexture = xMaterial.GetOcclusionTexture();
		strCurrentPath = xMaterial.GetOcclusionTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_EMISSIVE:
		pxCurrentTexture = xMaterial.GetEmissiveTexture();
		strCurrentPath = xMaterial.GetEmissiveTextureRef().GetPath();
		break;
	}

	std::string strTextureName = "(none)";
	if (pxCurrentTexture && pxCurrentTexture->m_xVRAMHandle.IsValid())
	{
		if (!strCurrentPath.empty())
		{
			std::filesystem::path xPath(strCurrentPath);
			strTextureName = xPath.filename().string();
		}
		else
		{
			strTextureName = "(loaded)";
		}
	}

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

	ImVec2 xButtonSize(150, 20);
	ImGui::Button(strTextureName.c_str(), xButtonSize);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			Zenith_Log(LOG_CATEGORY_MESH, "Texture dropped on %s: %s",
				szLabel, pFilePayload->m_szFilePath);

			AssignTextureToSlot(pFilePayload->m_szFilePath, uMeshIdx, eSlot);
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsItemHovered())
	{
		if (!strCurrentPath.empty())
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here\nPath: %s", strCurrentPath.c_str());
		}
		else
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here\nCurrent: %s", strTextureName.c_str());
		}
	}

	ImGui::PopID();
}

//-----------------------------------------------------------------------------
// Helper: Assign a texture file to a material slot
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, TextureSlotType eSlot)
{
	Zenith_AssetHandler::TextureData xTexData =
		Zenith_AssetHandler::LoadTexture2DFromFile(szFilePath);
	Flux_Texture* pxTexture = Zenith_AssetHandler::AddTexture(xTexData);
	xTexData.FreeAllocatedData();

	if (!pxTexture)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to load texture: %s", szFilePath);
		return;
	}

	pxTexture->m_strSourcePath = szFilePath;
	Zenith_Log(LOG_CATEGORY_MESH, "Loaded texture from: %s", szFilePath);

	Flux_MaterialAsset* pxOldMaterial = m_xMeshEntries.Get(uMeshIdx).m_pxMaterial;

	Flux_MaterialAsset* pxNewMaterial = Flux_MaterialAsset::Create("Material_" + std::to_string(uMeshIdx));
	if (!pxNewMaterial)
	{
		Zenith_Error(LOG_CATEGORY_MATERIAL, "Failed to create new material instance");
		return;
	}

	Zenith_Log(LOG_CATEGORY_MATERIAL, "Created new material instance");

	if (pxOldMaterial)
	{
		if (!pxOldMaterial->GetDiffuseTextureRef().GetPath().empty())
			pxNewMaterial->SetDiffuseTextureRef(pxOldMaterial->GetDiffuseTextureRef());
		if (!pxOldMaterial->GetNormalTextureRef().GetPath().empty())
			pxNewMaterial->SetNormalTextureRef(pxOldMaterial->GetNormalTextureRef());
		if (!pxOldMaterial->GetRoughnessMetallicTextureRef().GetPath().empty())
			pxNewMaterial->SetRoughnessMetallicTextureRef(pxOldMaterial->GetRoughnessMetallicTextureRef());
		if (!pxOldMaterial->GetOcclusionTextureRef().GetPath().empty())
			pxNewMaterial->SetOcclusionTextureRef(pxOldMaterial->GetOcclusionTextureRef());
		if (!pxOldMaterial->GetEmissiveTextureRef().GetPath().empty())
			pxNewMaterial->SetEmissiveTextureRef(pxOldMaterial->GetEmissiveTextureRef());

		pxNewMaterial->SetBaseColor(pxOldMaterial->GetBaseColor());
	}

	TextureRef xRef;
	xRef.SetFromPath(szFilePath);
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		pxNewMaterial->SetDiffuseTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set diffuse texture");
		break;
	case TEXTURE_SLOT_NORMAL:
		pxNewMaterial->SetNormalTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set normal texture");
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		pxNewMaterial->SetRoughnessMetallicTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set roughness/metallic texture");
		break;
	case TEXTURE_SLOT_OCCLUSION:
		pxNewMaterial->SetOcclusionTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set occlusion texture");
		break;
	case TEXTURE_SLOT_EMISSIVE:
		pxNewMaterial->SetEmissiveTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set emissive texture");
		break;
	}

	m_xMeshEntries.Get(uMeshIdx).m_pxMaterial = pxNewMaterial;
}

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI for model component
//-----------------------------------------------------------------------------
void Zenith_ModelComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Draw Physics Mesh", &m_bDebugDrawPhysicsMesh);

		ImGui::Separator();

		// Show which system is being used
		if (m_pxModelInstance)
		{
			ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Using: New Model Instance System");
			ImGui::Text("Model Path: %s", m_strModelPath.c_str());
			ImGui::Text("Meshes: %u", m_pxModelInstance->GetNumMeshes());
			ImGui::Text("Has Skeleton: %s", m_pxModelInstance->HasSkeleton() ? "Yes" : "No");
		}
		else if (m_xMeshEntries.GetSize() > 0)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Using: Procedural Mesh Entries");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No model loaded");
		}

		ImGui::Separator();

		// Model drop target - drag .zmodel files here
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

		// Load model from .zmodel file (new system) - manual path entry
		if (ImGui::TreeNode("Load Model (Manual Path)"))
		{
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

		// Animation section for new model instance system
		if (m_pxModelInstance && m_pxModelInstance->HasSkeleton() && ImGui::TreeNode("Animations (.zanim)"))
		{
			// Animation drop target - drag .zanim files here
			{
				ImVec2 xDropZoneSize(ImGui::GetContentRegionAvail().x, 30);
				ImGui::Button("Drop .zanim file here to add animation", xDropZoneSize);

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMATION))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);

						Zenith_Log(LOG_CATEGORY_ANIMATION, "Animation dropped: %s", pFilePayload->m_szFilePath);

						Flux_AnimationController* pxController = GetOrCreateAnimationController();
						if (pxController)
						{
							Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(pFilePayload->m_szFilePath);
							if (pxClip)
							{
								pxController->GetClipCollection().AddClip(pxClip);
								Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation: %s", pxClip->GetName().c_str());
							}
							else
							{
								Zenith_Error(LOG_CATEGORY_ANIMATION, "Failed to load animation from: %s", pFilePayload->m_szFilePath);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			ImGui::Separator();

			// Manual path entry section
			if (ImGui::TreeNode("Load Animation (Manual Path)"))
			{
				static char s_szAnimPath[512] = "";
				ImGui::InputText("Animation Path", s_szAnimPath, sizeof(s_szAnimPath));

				if (ImGui::Button("Load .zanim"))
				{
					if (strlen(s_szAnimPath) > 0)
					{
						Flux_AnimationController* pxController = GetOrCreateAnimationController();
						if (pxController)
						{
							// Load from .zanim file using our new loader
							Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(s_szAnimPath);
							if (pxClip)
							{
								pxController->GetClipCollection().AddClip(pxClip);
								Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation from: %s", s_szAnimPath);
							}
							else
							{
								Zenith_Error(LOG_CATEGORY_ANIMATION, "Failed to load animation from: %s", s_szAnimPath);
							}
						}
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##AnimBrowse"))
				{
#ifdef _WIN32
					OPENFILENAMEA ofn = { 0 };
					char szFile[512] = "";
					ofn.lStructSize = sizeof(ofn);
					ofn.lpstrFilter = "Animation Files (*.zanim)\0*.zanim\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
					if (GetOpenFileNameA(&ofn))
					{
						strncpy(s_szAnimPath, szFile, sizeof(s_szAnimPath) - 1);
					}
#endif
				}

				ImGui::TreePop();
			}

			// Show animation controller info
			Flux_AnimationController* pxController = GetAnimationController();
			if (pxController)
			{
				ImGui::Separator();

				const Flux_AnimationClipCollection& xClips = pxController->GetClipCollection();
				ImGui::Text("Loaded Clips: %u", xClips.GetClipCount());

				// List clips and allow playing them
				if (xClips.GetClipCount() > 0)
				{
					for (const Flux_AnimationClip* pxClip : xClips.GetClips())
					{
						if (pxClip)
						{
							ImGui::PushID(pxClip);
							if (ImGui::Button(pxClip->GetName().c_str()))
							{
								pxController->PlayClip(pxClip->GetName());
								Zenith_Log(LOG_CATEGORY_ANIMATION, "Playing animation: %s", pxClip->GetName().c_str());
							}
							ImGui::SameLine();
							ImGui::Text("(%.2fs)", pxClip->GetDuration());
							ImGui::PopID();
						}
					}
				}

				ImGui::Separator();

				// Playback controls
				bool bPaused = pxController->IsPaused();
				if (ImGui::Checkbox("Paused##NewModel", &bPaused))
				{
					pxController->SetPaused(bPaused);
				}

				float fSpeed = pxController->GetPlaybackSpeed();
				if (ImGui::SliderFloat("Speed##NewModel", &fSpeed, 0.0f, 2.0f))
				{
					pxController->SetPlaybackSpeed(fSpeed);
				}

				if (ImGui::Button("Stop"))
				{
					pxController->Stop();
				}

				// Editor preview: Update animation even when not in Play mode
				// This allows previewing animations in the editor
				if (pxController->HasAnimationContent())
				{
					float fPreviewDt = Zenith_Core::GetDt();
					pxController->Update(fPreviewDt);

					// Also update the model instance skeleton if using new system
					if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
					{
						m_pxModelInstance->UpdateAnimation();
					}
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Load an animation to create controller");
			}

			ImGui::TreePop();
		}

		// Animation loading section (only for procedural mesh entries)
		if (m_xMeshEntries.GetSize() > 0 && ImGui::TreeNode("Animations"))
		{
			static char s_szAnimFilePath[512] = "";
			ImGui::InputText("Animation File (.fbx/.gltf)", s_szAnimFilePath, sizeof(s_szAnimFilePath));

			static int s_iTargetMeshIndex = 0;
			int iMaxIndex = static_cast<int>(m_xMeshEntries.GetSize()) - 1;
			ImGui::SliderInt("Target Mesh Index", &s_iTargetMeshIndex, 0, iMaxIndex);

			if (ImGui::Button("Load Animation"))
			{
				if (strlen(s_szAnimFilePath) > 0 && s_iTargetMeshIndex >= 0 && s_iTargetMeshIndex < static_cast<int>(m_xMeshEntries.GetSize()))
				{
					Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(s_iTargetMeshIndex);
					if (xMesh.GetNumBones() > 0)
					{
						if (xMesh.m_pxAnimation)
						{
							delete xMesh.m_pxAnimation;
						}
						xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
						Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation from: %s for mesh %d", s_szAnimFilePath, s_iTargetMeshIndex);
					}
					else
					{
						Zenith_Error(LOG_CATEGORY_ANIMATION, "Cannot load animation: mesh %d has no bones", s_iTargetMeshIndex);
					}
				}
			}

			if (ImGui::Button("Load Animation for All Meshes"))
			{
				if (strlen(s_szAnimFilePath) > 0)
				{
					for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
					{
						Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
						if (xMesh.GetNumBones() > 0)
						{
							if (xMesh.m_pxAnimation)
							{
								delete xMesh.m_pxAnimation;
							}
							xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
							Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation for mesh %u", u);
						}
					}
				}
			}

			ImGui::Separator();
			for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
			{
				Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
				if (xMesh.m_pxAnimation)
				{
					ImGui::Text("Mesh %u: Animation loaded (%s)", u, xMesh.m_pxAnimation->GetSourcePath().c_str());
				}
				else if (xMesh.GetNumBones() > 0)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mesh %u: Has %u bones, no animation", u, xMesh.GetNumBones());
				}
			}

			// Animation Controller section
			Flux_AnimationController* pxController = GetAnimationController();
			if (pxController)
			{
				ImGui::Separator();
				ImGui::Text("Animation Controller");

				bool bPaused = pxController->IsPaused();
				if (ImGui::Checkbox("Paused", &bPaused))
				{
					pxController->SetPaused(bPaused);
				}

				float fSpeed = pxController->GetPlaybackSpeed();
				if (ImGui::SliderFloat("Playback Speed", &fSpeed, 0.0f, 2.0f))
				{
					pxController->SetPlaybackSpeed(fSpeed);
				}

				const Flux_AnimationClipCollection& xClips = pxController->GetClipCollection();
				ImGui::Text("Clips loaded: %u", static_cast<uint32_t>(xClips.GetClips().size()));

				if (!xClips.GetClips().empty())
				{
					if (ImGui::TreeNode("Clip List"))
					{
						for (const Flux_AnimationClip* pxClip : xClips.GetClips())
						{
							if (pxClip && ImGui::Selectable(pxClip->GetName().c_str()))
							{
								pxController->PlayClip(pxClip->GetName());
							}
						}
						ImGui::TreePop();
					}
				}
			}

			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::Text("Mesh Entries: %u", GetNumMeshes());

		// Display each procedural mesh entry with its material
		for (uint32_t uMeshIdx = 0; uMeshIdx < m_xMeshEntries.GetSize(); ++uMeshIdx)
		{
			ImGui::PushID(uMeshIdx);

			Flux_MaterialAsset& xMaterial = GetMaterialAtIndex(uMeshIdx);

			if (ImGui::TreeNode("MeshEntry", "Mesh Entry %u", uMeshIdx))
			{
				Flux_MeshGeometry& xGeom = GetMeshGeometryAtIndex(uMeshIdx);
				if (!xGeom.m_strSourcePath.empty())
				{
					ImGui::TextWrapped("Source: %s", xGeom.m_strSourcePath.c_str());
				}

				RenderTextureSlot("Diffuse", xMaterial, uMeshIdx, TEXTURE_SLOT_DIFFUSE);
				RenderTextureSlot("Normal", xMaterial, uMeshIdx, TEXTURE_SLOT_NORMAL);
				RenderTextureSlot("Roughness/Metallic", xMaterial, uMeshIdx, TEXTURE_SLOT_ROUGHNESS_METALLIC);
				RenderTextureSlot("Occlusion", xMaterial, uMeshIdx, TEXTURE_SLOT_OCCLUSION);
				RenderTextureSlot("Emissive", xMaterial, uMeshIdx, TEXTURE_SLOT_EMISSIVE);

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
}

#endif // ZENITH_TOOLS
