#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_TerrainComponent.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include <filesystem>

// Windows file dialog support
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

// Terrain export functionality (extern declaration to avoid include path issues)
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strMaterialPath, const std::string& strOutputDir);

//=============================================================================
// TerrainComponent Editor UI
//
// Editor-only code for the terrain component properties panel.
// Separated from runtime code to improve maintainability.
//=============================================================================

// Static state for terrain creation UI
static char s_szHeightmapPath[512] = "";
static char s_szMaterialPath[512] = "";
static bool s_bTerrainExportInProgress = false;
static std::string s_strTerrainExportStatus = "";

//-----------------------------------------------------------------------------
// Helper function to show Windows Open File dialog for terrain textures
// Supports .ztxtr (preferred) and .tif files
//-----------------------------------------------------------------------------
static std::string ShowTifOpenFileDialog()
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "Zenith Texture (*.ztxtr)\0*.ztxtr\0TIF Files (*.tif)\0*.tif\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "ztxtr";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI for terrain component
//-----------------------------------------------------------------------------
void Zenith_TerrainComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Terrain Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ========== Terrain Creation Section ==========
		// Only show if terrain is not yet initialized (no vertex buffer)
		bool bTerrainInitialized = m_ulUnifiedVertexBufferSize > 0;

		if (!bTerrainInitialized)
		{
			if (ImGui::TreeNode("Create Terrain From Heightmap"))
			{
				ImGui::TextWrapped("Specify heightmap and material interpolation textures to generate terrain geometry. Use .ztxtr files (exported from .tif via content browser) or .tif files directly. Textures should be 4096x4096 single-channel (grayscale).");
				ImGui::Separator();

				// Heightmap path input
				ImGui::Text("Heightmap Texture:");
				ImGui::PushItemWidth(300);
				ImGui::InputText("##HeightmapPath", s_szHeightmapPath, sizeof(s_szHeightmapPath), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();

				// Drag-drop target for heightmap texture
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);
						strncpy(s_szHeightmapPath, pFilePayload->m_szFilePath, sizeof(s_szHeightmapPath) - 1);
						s_szHeightmapPath[sizeof(s_szHeightmapPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Dropped heightmap: %s", s_szHeightmapPath);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##Heightmap"))
				{
					std::string strPath = ShowTifOpenFileDialog();
					if (!strPath.empty())
					{
						strncpy(s_szHeightmapPath, strPath.c_str(), sizeof(s_szHeightmapPath) - 1);
						s_szHeightmapPath[sizeof(s_szHeightmapPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected heightmap: %s", s_szHeightmapPath);
					}
				}

				// Material path input
				ImGui::Text("Material Interpolation Texture:");
				ImGui::PushItemWidth(300);
				ImGui::InputText("##MaterialPath", s_szMaterialPath, sizeof(s_szMaterialPath), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();

				// Drag-drop target for material interpolation texture
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);
						strncpy(s_szMaterialPath, pFilePayload->m_szFilePath, sizeof(s_szMaterialPath) - 1);
						s_szMaterialPath[sizeof(s_szMaterialPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Dropped material texture: %s", s_szMaterialPath);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##Material"))
				{
					std::string strPath = ShowTifOpenFileDialog();
					if (!strPath.empty())
					{
						strncpy(s_szMaterialPath, strPath.c_str(), sizeof(s_szMaterialPath) - 1);
						s_szMaterialPath[sizeof(s_szMaterialPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected material texture: %s", s_szMaterialPath);
					}
				}

				ImGui::Separator();

				// Output directory info
				std::string strOutputDir = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
				ImGui::Text("Output Directory: %s", strOutputDir.c_str());

				// Create terrain button
				bool bCanCreate = strlen(s_szHeightmapPath) > 0 && strlen(s_szMaterialPath) > 0 && !s_bTerrainExportInProgress;

				if (!bCanCreate)
					ImGui::BeginDisabled();

				if (ImGui::Button("Create Terrain", ImVec2(200, 30)))
				{
					s_bTerrainExportInProgress = true;
					s_strTerrainExportStatus = "Exporting terrain meshes...";

					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain export...");
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Material: %s", s_szMaterialPath);
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strOutputDir.c_str());

					// Perform the terrain export
					ExportHeightmapFromPaths(s_szHeightmapPath, s_szMaterialPath, strOutputDir);

					s_strTerrainExportStatus = "Export complete. Initializing terrain...";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Export complete. Initializing terrain...");

					// Create blank materials for initial rendering
					std::string strEntityName = m_xParentEntity.GetName().empty() ?
						("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex)) : m_xParentEntity.GetName();

					m_pxMaterial0 = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
					m_pxMaterial1 = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
					if (m_pxMaterial0)
					{
						m_pxMaterial0->SetName(strEntityName + "_Terrain_Mat0");
						m_pxMaterial0->AddRef();
					}
					if (m_pxMaterial1)
					{
						m_pxMaterial1->SetName(strEntityName + "_Terrain_Mat1");
						m_pxMaterial1->AddRef();
					}
					m_bOwnsMaterials = true;

					// Load physics geometry (same as constructor/deserialization)
					if (m_pxPhysicsGeometry == nullptr)
					{
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading and combining all physics chunks...");

						// Load first physics chunk
						m_pxPhysicsGeometry = new Flux_MeshGeometry();
						Flux_MeshGeometry::LoadFromFile(
							(strOutputDir + "Physics_0_0" ZENITH_MESH_EXT).c_str(),
							*m_pxPhysicsGeometry,
							1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

						if (m_pxPhysicsGeometry->GetNumVerts() > 0)
						{
							Flux_MeshGeometry& xPhysicsGeometry = *m_pxPhysicsGeometry;

							// Pre-allocate for all chunks
							const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TOTAL_CHUNKS;
							const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TOTAL_CHUNKS;
							const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.GetNumVerts() * sizeof(Zenith_Maths::Vector3) * TOTAL_CHUNKS;

							xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
							xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

							xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
							xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

							xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
							xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

							// Combine remaining physics chunks
							for (uint32_t x = 0; x < CHUNK_GRID_SIZE; x++)
							{
								for (uint32_t y = 0; y < CHUNK_GRID_SIZE; y++)
								{
									if (x == 0 && y == 0) continue;

									std::string strPhysicsPath = strOutputDir + "Physics_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
									Flux_MeshGeometry xTerrainPhysicsMesh;
									Flux_MeshGeometry::LoadFromFile(
										strPhysicsPath.c_str(),
										xTerrainPhysicsMesh,
										1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

									if (xTerrainPhysicsMesh.GetNumVerts() > 0)
									{
										Flux_MeshGeometry::Combine(xPhysicsGeometry, xTerrainPhysicsMesh);
									}
									// xTerrainPhysicsMesh automatically destroyed when going out of scope
								}
							}

							Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Physics mesh combined: %u vertices, %u indices",
								xPhysicsGeometry.GetNumVerts(), xPhysicsGeometry.GetNumIndices());
						}
					}

					// Initialize render resources (LOW LOD meshes, buffers, culling)
					InitializeRenderResources(*m_pxMaterial0, *m_pxMaterial1);

					s_bTerrainExportInProgress = false;
					s_strTerrainExportStatus = "Terrain created successfully!";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation complete!");
				}

				if (!bCanCreate)
					ImGui::EndDisabled();

				// Status display
				if (!s_strTerrainExportStatus.empty())
				{
					ImGui::Separator();
					if (s_bTerrainExportInProgress)
					{
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
					else if (s_strTerrainExportStatus.find("success") != std::string::npos ||
					         s_strTerrainExportStatus.find("complete") != std::string::npos)
					{
						ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
					else
					{
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
				}

				ImGui::TreePop();
			}

			ImGui::Separator();
		}

		// ========== Regenerate Terrain Section ==========
		// Show when terrain IS initialized - allows regenerating with new heightmaps
		if (bTerrainInitialized)
		{
			if (ImGui::TreeNode("Regenerate Terrain"))
			{
				ImGui::TextWrapped("Regenerate terrain from new heightmap and material interpolation textures. This will delete existing terrain files and recreate all chunks.");
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: This operation cannot be undone!");
				ImGui::Separator();

				// Heightmap path input
				ImGui::Text("New Heightmap Texture:");
				ImGui::PushItemWidth(300);
				ImGui::InputText("##RegenHeightmapPath", s_szHeightmapPath, sizeof(s_szHeightmapPath), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();

				// Drag-drop target for heightmap texture
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);
						strncpy(s_szHeightmapPath, pFilePayload->m_szFilePath, sizeof(s_szHeightmapPath) - 1);
						s_szHeightmapPath[sizeof(s_szHeightmapPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Dropped new heightmap: %s", s_szHeightmapPath);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##RegenHeightmap"))
				{
					std::string strPath = ShowTifOpenFileDialog();
					if (!strPath.empty())
					{
						strncpy(s_szHeightmapPath, strPath.c_str(), sizeof(s_szHeightmapPath) - 1);
						s_szHeightmapPath[sizeof(s_szHeightmapPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected new heightmap: %s", s_szHeightmapPath);
					}
				}

				// Material path input
				ImGui::Text("New Material Interpolation Texture:");
				ImGui::PushItemWidth(300);
				ImGui::InputText("##RegenMaterialPath", s_szMaterialPath, sizeof(s_szMaterialPath), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();

				// Drag-drop target for material interpolation texture
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);
						strncpy(s_szMaterialPath, pFilePayload->m_szFilePath, sizeof(s_szMaterialPath) - 1);
						s_szMaterialPath[sizeof(s_szMaterialPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Dropped new material texture: %s", s_szMaterialPath);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##RegenMaterial"))
				{
					std::string strPath = ShowTifOpenFileDialog();
					if (!strPath.empty())
					{
						strncpy(s_szMaterialPath, strPath.c_str(), sizeof(s_szMaterialPath) - 1);
						s_szMaterialPath[sizeof(s_szMaterialPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected new material texture: %s", s_szMaterialPath);
					}
				}

				ImGui::Separator();

				// Output directory info
				std::string strOutputDir = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
				ImGui::Text("Output Directory: %s", strOutputDir.c_str());

				// Regenerate terrain button
				bool bCanRegenerate = strlen(s_szHeightmapPath) > 0 && strlen(s_szMaterialPath) > 0 && !s_bTerrainExportInProgress;

				if (!bCanRegenerate)
					ImGui::BeginDisabled();

				if (ImGui::Button("Regenerate Terrain", ImVec2(200, 30)))
				{
					s_bTerrainExportInProgress = true;
					s_strTerrainExportStatus = "Cleaning up existing terrain...";

					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain regeneration...");

					// ========== Step 1: Clean up existing GPU resources ==========
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing culling resources...");
					DestroyCullingResources();

					// Unregister buffers from streaming manager
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Unregistering terrain buffers from streaming manager...");
					Flux_TerrainStreamingManager::UnregisterTerrainBuffers();

					// Destroy existing unified buffers
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing unified buffers...");
					Flux_MemoryManager::DestroyVertexBuffer(m_xUnifiedVertexBuffer);
					Flux_MemoryManager::DestroyIndexBuffer(m_xUnifiedIndexBuffer);
					m_ulUnifiedVertexBufferSize = 0;
					m_ulUnifiedIndexBufferSize = 0;

					// Clean up physics geometry
					if (m_pxPhysicsGeometry)
					{
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing physics geometry...");
						delete m_pxPhysicsGeometry;
						m_pxPhysicsGeometry = nullptr;
					}

					// ========== Step 2: Delete existing terrain files ==========
					s_strTerrainExportStatus = "Deleting existing terrain files...";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleting existing terrain files in %s", strOutputDir.c_str());

					try
					{
						if (std::filesystem::exists(strOutputDir))
						{
							for (const auto& entry : std::filesystem::directory_iterator(strOutputDir))
							{
								if (entry.is_regular_file())
								{
									std::filesystem::remove(entry.path());
								}
							}
							Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleted existing terrain files");
						}
					}
					catch (const std::exception& e)
					{
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Warning: Failed to delete some terrain files: %s", e.what());
					}

					// ========== Step 3: Export new terrain meshes ==========
					s_strTerrainExportStatus = "Exporting new terrain meshes...";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Exporting new terrain...");
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Material: %s", s_szMaterialPath);
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strOutputDir.c_str());

					ExportHeightmapFromPaths(s_szHeightmapPath, s_szMaterialPath, strOutputDir);

					// ========== Step 4: Reload physics geometry ==========
					s_strTerrainExportStatus = "Loading physics geometry...";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading new physics geometry...");
					LoadCombinedPhysicsGeometry();

					// ========== Step 5: Reinitialize render resources ==========
					s_strTerrainExportStatus = "Initializing render resources...";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Reinitializing render resources...");

					// Use existing materials or create new ones if needed
					if (!m_pxMaterial0)
					{
						std::string strEntityName = m_xParentEntity.GetName().empty() ?
							("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex)) : m_xParentEntity.GetName();
						m_pxMaterial0 = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
						if (m_pxMaterial0)
						{
							m_pxMaterial0->SetName(strEntityName + "_Terrain_Mat0");
							m_pxMaterial0->AddRef();
						}
						m_bOwnsMaterials = true;
					}
					if (!m_pxMaterial1)
					{
						std::string strEntityName = m_xParentEntity.GetName().empty() ?
							("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex)) : m_xParentEntity.GetName();
						m_pxMaterial1 = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
						if (m_pxMaterial1)
						{
							m_pxMaterial1->SetName(strEntityName + "_Terrain_Mat1");
							m_pxMaterial1->AddRef();
						}
						m_bOwnsMaterials = true;
					}

					InitializeRenderResources(*m_pxMaterial0, *m_pxMaterial1);

					s_bTerrainExportInProgress = false;
					s_strTerrainExportStatus = "Terrain regenerated successfully!";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain regeneration complete!");
				}

				if (!bCanRegenerate)
					ImGui::EndDisabled();

				// Status display
				if (!s_strTerrainExportStatus.empty())
				{
					ImGui::Separator();
					if (s_bTerrainExportInProgress)
					{
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
					else if (s_strTerrainExportStatus.find("success") != std::string::npos ||
					         s_strTerrainExportStatus.find("complete") != std::string::npos)
					{
						ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
					else
					{
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
				}

				ImGui::TreePop();
			}

			ImGui::Separator();
		}

		// Terrain statistics
		if (ImGui::TreeNode("Statistics"))
		{
			ImGui::Text("Chunks: %d x %d", CHUNK_GRID_SIZE, CHUNK_GRID_SIZE);
			ImGui::Text("Total Chunks: %d", TOTAL_CHUNKS);
			ImGui::Text("LOD Count: %d", LOD_COUNT);
			ImGui::Text("Vertex Buffer Size: %.2f MB", m_ulUnifiedVertexBufferSize / (1024.0f * 1024.0f));
			ImGui::Text("Index Buffer Size: %.2f MB", m_ulUnifiedIndexBufferSize / (1024.0f * 1024.0f));
			ImGui::Text("LOW LOD Vertices: %u", m_uLowLODVertexCount);
			ImGui::Text("LOW LOD Indices: %u", m_uLowLODIndexCount);
			bool bTemp = m_bCullingResourcesInitialized;
			ImGui::Checkbox("Culling Resources Initialized", &bTemp);
			ImGui::TreePop();
		}

		ImGui::Separator();

		// Material 0 editing (full material system)
		if (m_pxMaterial0)
		{
			if (ImGui::TreeNode("Material 0 (Base)"))
			{
				ImGui::Text("Name: %s", m_pxMaterial0->GetName().c_str());

				// Full material properties (same as static meshes)
				Zenith_Editor_MaterialUI::RenderMaterialProperties(m_pxMaterial0, "TerrainMat0");

				// Texture slots
				ImGui::Separator();
				ImGui::Text("Textures:");
				Zenith_Editor_MaterialUI::RenderAllTextureSlots(*m_pxMaterial0, false);

				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextDisabled("Material 0: (not set)");
		}

		// Material 1 editing (full material system)
		if (m_pxMaterial1)
		{
			if (ImGui::TreeNode("Material 1 (Blend)"))
			{
				ImGui::Text("Name: %s", m_pxMaterial1->GetName().c_str());

				// Full material properties (same as static meshes)
				Zenith_Editor_MaterialUI::RenderMaterialProperties(m_pxMaterial1, "TerrainMat1");

				// Texture slots
				ImGui::Separator();
				ImGui::Text("Textures:");
				Zenith_Editor_MaterialUI::RenderAllTextureSlots(*m_pxMaterial1, false);

				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextDisabled("Material 1: (not set)");
		}
	}
}

#endif // ZENITH_TOOLS
