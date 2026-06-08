#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_TerrainComponent.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include <filesystem>

// Windows file dialog support
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

// Terrain export functionality (extern declaration to avoid include path issues)
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);

//=============================================================================
// TerrainComponent Editor UI
//
// Editor-only code for the terrain component properties panel.
// Separated from runtime code to improve maintainability.
//=============================================================================

// Static state for terrain creation UI
static char s_szHeightmapPath[512] = "";
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
// Helper: Render heightmap path input with drag-drop and browse button
//-----------------------------------------------------------------------------
static void RenderHeightmapPathInput(const char* szImGuiId)
{
	ImGui::Text("Heightmap Texture:");
	ImGui::PushItemWidth(300);
	char szInputId[64];
	snprintf(szInputId, sizeof(szInputId), "##%s", szImGuiId);
	ImGui::InputText(szInputId, s_szHeightmapPath, sizeof(s_szHeightmapPath), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);
			strncpy_s(s_szHeightmapPath, sizeof(s_szHeightmapPath), pFilePayload->m_szFilePath, _TRUNCATE);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Dropped heightmap: %s", s_szHeightmapPath);
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();
	char szBrowseId[64];
	snprintf(szBrowseId, sizeof(szBrowseId), "Browse...##%s", szImGuiId);
	if (ImGui::Button(szBrowseId))
	{
		std::string strPath = ShowTifOpenFileDialog();
		if (!strPath.empty())
		{
			strncpy_s(s_szHeightmapPath, sizeof(s_szHeightmapPath), strPath.c_str(), _TRUNCATE);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected heightmap: %s", s_szHeightmapPath);
		}
	}
}

//-----------------------------------------------------------------------------
// Helper: Render terrain export status with color coding
//-----------------------------------------------------------------------------
static void RenderTerrainStatusDisplay()
{
	if (s_strTerrainExportStatus.empty())
		return;

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

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI. Delegates to per-section helpers so
// each section stays focused on its own concern.
//-----------------------------------------------------------------------------
void Zenith_TerrainComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Terrain Component", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const bool bTerrainInitialized = m_pxStreamingState->m_ulUnifiedVertexBufferSize > 0;

	if (!bTerrainInitialized)
	{
		RenderTerrainCreationSection();
		ImGui::Separator();
	}
	else
	{
		RenderTerrainRegenerationSection();
		ImGui::Separator();
	}

	RenderTerrainStatisticsSection();
	RenderDebugVisualizationSection();

	ImGui::Separator();
	RenderMaterialPalette();

	ImGui::Separator();
	RenderSplatmapSlot();
}

//-----------------------------------------------------------------------------
// Load every per-chunk Physics_x_y mesh from strOutputDir and merge them into
// xPhysicsGeometry (which already holds chunk 0,0). Pre-allocates the combined
// buffers before walking the chunk grid.
//-----------------------------------------------------------------------------
static void CombinePhysicsChunksFromDisk(const std::string& strOutputDir, Flux_MeshGeometry& xPhysicsGeometry)
{
	const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TOTAL_CHUNKS;
	const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TOTAL_CHUNKS;
	const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.GetNumVerts() * sizeof(Zenith_Maths::Vector3) * TOTAL_CHUNKS;

	xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
	xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
	xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
	xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

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
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Physics mesh combined: %u vertices, %u indices",
		xPhysicsGeometry.GetNumVerts(), xPhysicsGeometry.GetNumIndices());
}

//-----------------------------------------------------------------------------
// Initial terrain creation from a heightmap (shown only when terrain is not
// yet initialised).
//-----------------------------------------------------------------------------
void Zenith_TerrainComponent::RenderTerrainCreationSection()
{
	if (!ImGui::TreeNode("Create Terrain From Heightmap"))
		return;

	ImGui::TextWrapped("Specify a heightmap texture to generate terrain geometry. Use .ztxtr files (exported from .tif via content browser) or .tif files directly. Textures should be 4096x4096 single-channel (grayscale).");
	ImGui::Separator();

	RenderHeightmapPathInput("HeightmapPath");

	ImGui::Separator();

	const std::string strOutputDir = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
	ImGui::Text("Output Directory: %s", strOutputDir.c_str());

	const bool bCanCreate = strlen(s_szHeightmapPath) > 0 && !s_bTerrainExportInProgress;

	if (!bCanCreate)
		ImGui::BeginDisabled();

	if (ImGui::Button("Create Terrain", ImVec2(200, 30)))
	{
		s_bTerrainExportInProgress = true;
		s_strTerrainExportStatus = "Exporting terrain meshes...";

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain export...");
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strOutputDir.c_str());

		ExportHeightmapFromPaths(s_szHeightmapPath, strOutputDir);

		s_strTerrainExportStatus = "Export complete. Initializing terrain...";
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Export complete. Initializing terrain...");

		// Create blank materials for initial rendering
		const std::string strEntityName = m_xParentEntity.GetName().empty()
			? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
			: m_xParentEntity.GetName();

		Zenith_MaterialAsset* pxMat0 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat1 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		if (pxMat0) pxMat0->SetName(strEntityName + "_Terrain_Mat0");
		if (pxMat1) pxMat1->SetName(strEntityName + "_Terrain_Mat1");
		m_axMaterials[0].Set(pxMat0);
		m_axMaterials[1].Set(pxMat1);

		// Load physics geometry (same as constructor/deserialization) by combining
		// every per-chunk physics mesh into one geometry object.
		if (m_pxPhysicsGeometry == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading and combining all physics chunks...");

			m_pxPhysicsGeometry = new Flux_MeshGeometry();
			Flux_MeshGeometry::LoadFromFile(
				(strOutputDir + "Physics_0_0" ZENITH_MESH_EXT).c_str(),
				*m_pxPhysicsGeometry,
				1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

			if (m_pxPhysicsGeometry->GetNumVerts() > 0)
			{
				CombinePhysicsChunksFromDisk(strOutputDir, *m_pxPhysicsGeometry);
			}
		}

		InitializeRenderResources();

		s_bTerrainExportInProgress = false;
		s_strTerrainExportStatus = "Terrain created successfully!";
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation complete!");
	}

	if (!bCanCreate)
		ImGui::EndDisabled();

	RenderTerrainStatusDisplay();
	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// Regenerate an existing terrain from a new heightmap. Destroys prior GPU /
// physics / buffer state via CleanupPriorGenerationForRegenerate() before
// re-running the export.
//-----------------------------------------------------------------------------
// Delete every regular file under strOutputDir. Errors are warned, not
// fatal — partial cleanup is recoverable since the next ExportHeightmapFromPaths
// will overwrite anything that survives.
static void DeleteExistingTerrainFiles(const std::string& strOutputDir)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleting existing terrain files in %s", strOutputDir.c_str());
	try
	{
		if (std::filesystem::exists(strOutputDir))
		{
			for (const auto& entry : std::filesystem::directory_iterator(strOutputDir))
			{
				if (entry.is_regular_file()) std::filesystem::remove(entry.path());
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleted existing terrain files");
		}
	}
	catch (const std::exception& e)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Warning: Failed to delete some terrain files: %s", e.what());
	}
}

// Allocate a fresh material asset into any empty slot, named after the
// owning entity. Called between physics-geometry load and render-init so
// the upcoming InitializeRenderResources sees a fully-populated slot table.
void Zenith_TerrainComponent::EnsureMaterialSlotsPopulated()
{
	const std::string strEntityName = m_xParentEntity.GetName().empty()
		? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
		: m_xParentEntity.GetName();
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (m_axMaterials[u].GetDirect()) continue;
		Zenith_MaterialAsset* pxMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		if (pxMat) pxMat->SetName(strEntityName + "_Terrain_Mat" + std::to_string(u));
		m_axMaterials[u].Set(pxMat);
	}
}

// End-to-end execution of the Regenerate button: cleanup → delete files →
// export → reload physics → re-init render. Each step updates the export
// status string so the editor footer reflects progress.
void Zenith_TerrainComponent::RunTerrainRegeneration(const std::string& strOutputDir)
{
	s_bTerrainExportInProgress = true;
	s_strTerrainExportStatus = "Cleaning up existing terrain...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain regeneration...");

	CleanupPriorGenerationForRegenerate();

	s_strTerrainExportStatus = "Deleting existing terrain files...";
	DeleteExistingTerrainFiles(strOutputDir);

	s_strTerrainExportStatus = "Exporting new terrain meshes...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Exporting new terrain...");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strOutputDir.c_str());
	ExportHeightmapFromPaths(s_szHeightmapPath, strOutputDir);

	s_strTerrainExportStatus = "Loading physics geometry...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading new physics geometry...");
	LoadCombinedPhysicsGeometry();

	s_strTerrainExportStatus = "Initializing render resources...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Reinitializing render resources...");
	EnsureMaterialSlotsPopulated();
	InitializeRenderResources();

	s_bTerrainExportInProgress = false;
	s_strTerrainExportStatus = "Terrain regenerated successfully!";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain regeneration complete!");
}

void Zenith_TerrainComponent::RenderTerrainRegenerationSection()
{
	if (!ImGui::TreeNode("Regenerate Terrain"))
		return;

	ImGui::TextWrapped("Regenerate terrain from new heightmap and material interpolation textures. This will delete existing terrain files and recreate all chunks.");
	ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: This operation cannot be undone!");
	ImGui::Separator();

	RenderHeightmapPathInput("RegenHeightmapPath");

	ImGui::Separator();

	const std::string strOutputDir = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
	ImGui::Text("Output Directory: %s", strOutputDir.c_str());

	const bool bCanRegenerate = strlen(s_szHeightmapPath) > 0 && !s_bTerrainExportInProgress;

	if (!bCanRegenerate) ImGui::BeginDisabled();

	if (ImGui::Button("Regenerate Terrain", ImVec2(200, 30)))
	{
		RunTerrainRegeneration(strOutputDir);
	}

	if (!bCanRegenerate) ImGui::EndDisabled();

	RenderTerrainStatusDisplay();
	ImGui::TreePop();
}

// Teardown sequence used by the Regenerate button. The order matters:
// culling resources -> streaming manager -> unified buffers -> physics geometry.
// Keeping this together makes the contract visible to readers.
void Zenith_TerrainComponent::CleanupPriorGenerationForRegenerate()
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing culling resources...");
	DestroyCullingResources();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Unregistering terrain buffers from streaming manager...");
	// Component-aware overload — the legacy no-arg form resolves through
	// s_pxPrimary, which on a non-primary terrain would unregister the
	// wrong state and leave THIS component still in the registry pointing
	// at buffers we're about to destroy.
	g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(m_pxStreamingState);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing unified buffers...");
	g_xEngine.FluxMemory().DestroyVertexBuffer(m_pxStreamingState->m_xUnifiedVertexBuffer);
	g_xEngine.FluxMemory().DestroyIndexBuffer(m_pxStreamingState->m_xUnifiedIndexBuffer);
	m_pxStreamingState->m_ulUnifiedVertexBufferSize = 0;
	m_pxStreamingState->m_ulUnifiedIndexBufferSize = 0;

	if (m_pxPhysicsGeometry)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing physics geometry...");
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
	}
}

void Zenith_TerrainComponent::RenderTerrainStatisticsSection()
{
	if (!ImGui::TreeNode("Statistics"))
		return;

	ImGui::Text("Chunks: %d x %d", CHUNK_GRID_SIZE, CHUNK_GRID_SIZE);
	ImGui::Text("Total Chunks: %d", TOTAL_CHUNKS);
	ImGui::Text("LOD Count: %d", LOD_COUNT);
	ImGui::Text("Vertex Buffer Size: %.2f MB", m_pxStreamingState->m_ulUnifiedVertexBufferSize / (1024.0f * 1024.0f));
	ImGui::Text("Index Buffer Size: %.2f MB", m_pxStreamingState->m_ulUnifiedIndexBufferSize / (1024.0f * 1024.0f));
	ImGui::Text("LOW LOD Vertices: %u", m_pxStreamingState->m_uLowLODVertexCount);
	ImGui::Text("LOW LOD Indices: %u", m_pxStreamingState->m_uLowLODIndexCount);
	bool bTemp = m_pxStreamingState->m_bCullingResourcesInitialized;
	ImGui::Checkbox("Culling Resources Initialized", &bTemp);
	ImGui::TreePop();
}

void Zenith_TerrainComponent::RenderDebugVisualizationSection()
{
	if (!ImGui::TreeNode("Debug Visualization"))
		return;

	static const char* aszDebugModeNames[] = {
		"Off", "LOD Level", "World Normals", "UVs", "Material Blend",
		"Roughness", "Metallic", "Occlusion", "World Position", "Chunk Grid",
		"Tangent", "Bitangent Sign", "Source Chunk Hash"
	};
	u_int& uDebugMode = g_xEngine.Terrain().GetDebugMode();
	int iDebugMode = static_cast<int>(uDebugMode);
	if (ImGui::Combo("Visualization Mode", &iDebugMode, aszDebugModeNames, IM_ARRAYSIZE(aszDebugModeNames)))
	{
		uDebugMode = static_cast<u_int>(iDebugMode);
	}

	bool& bWireframe = g_xEngine.Terrain().GetWireframeMode();
	ImGui::Checkbox("Wireframe", &bWireframe);

	ImGui::Separator();

	ImGui::Text("Streaming Statistics");
	// Component-aware: pull this terrain's own stats. Falls back to a zeroed
	// snapshot if streaming state isn't yet initialised (component just
	// constructed, not yet registered).
	static const Flux_TerrainStreamingManagerImpl::StreamingStats kxZeroStats{};
	const Flux_TerrainStreamingState* pxState = m_pxStreamingState;
	const Flux_TerrainStreamingManagerImpl::StreamingStats& xStats = pxState ? pxState->m_xStats : kxZeroStats;

	ImGui::Text("HIGH LOD Chunks: %u / %u", xStats.m_uHighLODChunksResident, TOTAL_CHUNKS);
	ImGui::Text("Streams This Frame: %u", xStats.m_uStreamsThisFrame);
	ImGui::Text("Evictions This Frame: %u", xStats.m_uEvictionsThisFrame);

	ImGui::Separator();

	const float fVertexUsage = xStats.m_uVertexBufferTotalMB > 0
		? static_cast<float>(xStats.m_uVertexBufferUsedMB) / static_cast<float>(xStats.m_uVertexBufferTotalMB)
		: 0.0f;
	char szVertexLabel[64];
	snprintf(szVertexLabel, sizeof(szVertexLabel), "Vertex Buffer: %u / %u MB", xStats.m_uVertexBufferUsedMB, xStats.m_uVertexBufferTotalMB);
	ImGui::ProgressBar(fVertexUsage, ImVec2(-1, 0), szVertexLabel);
	ImGui::Text("Vertex Fragments: %u", xStats.m_uVertexFragments);

	const float fIndexUsage = xStats.m_uIndexBufferTotalMB > 0
		? static_cast<float>(xStats.m_uIndexBufferUsedMB) / static_cast<float>(xStats.m_uIndexBufferTotalMB)
		: 0.0f;
	char szIndexLabel[64];
	snprintf(szIndexLabel, sizeof(szIndexLabel), "Index Buffer: %u / %u MB", xStats.m_uIndexBufferUsedMB, xStats.m_uIndexBufferTotalMB);
	ImGui::ProgressBar(fIndexUsage, ImVec2(-1, 0), szIndexLabel);
	ImGui::Text("Index Fragments: %u", xStats.m_uIndexFragments);

	ImGui::TreePop();
}

void Zenith_TerrainComponent::RenderMaterialPalette()
{
	static const char* aszMaterialNames[] = { "Material 0", "Material 1", "Material 2", "Material 3" };
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		Zenith_MaterialAsset* pxMat = m_axMaterials[u].GetDirect();
		if (!pxMat)
		{
			ImGui::TextDisabled("%s: (not set)", aszMaterialNames[u]);
			continue;
		}

		char szLabel[64];
		snprintf(szLabel, sizeof(szLabel), "%s##TerrainMat%u", aszMaterialNames[u], u);
		if (!ImGui::TreeNode(szLabel))
			continue;

		ImGui::Text("Name: %s", pxMat->GetName().c_str());

		char szImGuiId[32];
		snprintf(szImGuiId, sizeof(szImGuiId), "TerrainMat%u", u);
		g_xEngine.EditorMaterialUI().RenderMaterialProperties(pxMat, szImGuiId);

		ImGui::Separator();
		ImGui::Text("Textures:");
		g_xEngine.EditorMaterialUI().RenderAllTextureSlots(*pxMat, false);

		ImGui::TreePop();
	}
}

void Zenith_TerrainComponent::RenderSplatmapSlot()
{
	if (!ImGui::TreeNode("Splatmap Texture"))
		return;

	if (Zenith_TextureAsset* pxSplatmap = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xSplatmap.GetPath()))
	{
		Flux_ImGuiTextureHandle xSplatmapHandle = g_xEngine.EditorMaterialUI().GetOrCreateTexturePreviewHandle(pxSplatmap);
		if (xSplatmapHandle.IsValid())
		{
			ImGui::Image(
				(ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xSplatmapHandle),
				ImVec2(128, 128)
			);
		}
		ImGui::TextWrapped("%s", m_xSplatmap.GetPath().c_str());
	}
	else
	{
		ImGui::TextDisabled("(not set)");
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);
			m_xSplatmap.SetPath(pFilePayload->m_szFilePath);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Set splatmap: %s", pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::TreePop();
}

#endif // ZENITH_TOOLS
