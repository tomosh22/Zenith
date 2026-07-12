#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_TerrainComponent.h"
#include "imgui.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_Image.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include <filesystem>

#ifdef ZENITH_TESTING
#include "UnitTests/Zenith_UnitTests.h"
#endif

// Windows file dialog support
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

// Terrain export functionality (extern declarations to avoid include path issues)
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);
extern void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir);

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
// Supports .ztxtr (preferred) and PNG files
//-----------------------------------------------------------------------------
static std::string ShowHeightmapOpenFileDialog()
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "Zenith Texture (*.ztxtr)\0*.ztxtr\0PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
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
		std::string strPath = ShowHeightmapOpenFileDialog();
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

	// Sculpting / painting lives in the dedicated terrain editor. Routed
	// through Zenith_Editor (not the terrain-editor header) — this TU sits in
	// the EntityComponent layer, which may not include Editor/TerrainEditor.
	// Keep this available for fresh components: the dedicated editor is where
	// the first validated, staged asset set is chosen before BakeFull commits it.
	if (ImGui::Button("Open Terrain Editor", ImVec2(200, 28)))
	{
		g_xEngine.Editor().OpenTerrainEditor(m_xParentEntity.GetEntityID());
	}
	ImGui::Separator();

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

bool Zenith_TerrainComponent::IsTerrainInitializedForEditor() const
{
	return m_pxStreamingState != nullptr &&
		m_pxStreamingState->m_ulUnifiedVertexBufferSize > 0;
}

//-----------------------------------------------------------------------------
// Initial terrain creation from a heightmap (shown only when terrain is not
// yet initialised).
//-----------------------------------------------------------------------------
void Zenith_TerrainComponent::RenderTerrainCreationSection()
{
	if (!ImGui::TreeNode("Create Terrain From Heightmap"))
		return;

	ImGui::TextWrapped("Specify a heightmap texture to generate terrain geometry. Use .ztxtr files (preferred) or 16-bit PNG. Textures should be 4096x4096 single-channel (grayscale).");
	ImGui::Separator();

	RenderHeightmapPathInput("HeightmapPath");

	ImGui::Separator();

	const std::string strOutputDir = GetTerrainAssetDirectory();
	ImGui::Text("Output Directory: %s", strOutputDir.c_str());

	const bool bCanCreate = strlen(s_szHeightmapPath) > 0 && !s_bTerrainExportInProgress;

	if (!bCanCreate)
		ImGui::BeginDisabled();

	if (ImGui::Button("Create Terrain", ImVec2(200, 30)))
	{
		[&]()
		{
		std::string strValidatedOutputDir;
		if (!TryResolveValidatedTerrainAssetDirectory(m_strTerrainAssetSet, strValidatedOutputDir) ||
			strValidatedOutputDir != strOutputDir)
		{
			s_strTerrainExportStatus = "Terrain creation refused an unsafe asset-set target.";
			return;
		}
		s_bTerrainExportInProgress = true;
		s_strTerrainExportStatus = "Exporting terrain meshes...";

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain export...");
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strValidatedOutputDir.c_str());

		std::filesystem::create_directories(strValidatedOutputDir);
		ExportHeightmapFromPaths(s_szHeightmapPath, strValidatedOutputDir);

		s_strTerrainExportStatus = "Export complete. Initializing terrain...";
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Export complete. Initializing terrain...");

		// Create blank materials for initial rendering
		const std::string strEntityName = m_xParentEntity.GetName().empty()
			? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
			: m_xParentEntity.GetName();

		auto xhMat0 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat0 = xhMat0.GetDirect();
		auto xhMat1 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat1 = xhMat1.GetDirect();
		if (pxMat0) pxMat0->SetName(strEntityName + "_Terrain_Mat0");
		if (pxMat1) pxMat1->SetName(strEntityName + "_Terrain_Mat1");
		m_axMaterials[0].Set(pxMat0);
		m_axMaterials[1].Set(pxMat1);

		EnsureMaterialSlotsPopulated();
		InitializeRenderResources();
		LoadCombinedPhysicsGeometry();

		s_bTerrainExportInProgress = false;
		if (IsTerrainInitializedForEditor() && HasPhysicsGeometry() && !m_bTerrainGeometryUnusable)
		{
			s_strTerrainExportStatus = "Terrain created successfully!";
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation complete!");
		}
		else
		{
			s_strTerrainExportStatus = "Terrain creation failed to initialize render/physics state.";
			Zenith_Warning(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation did not produce complete live state");
		}
		}();
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
bool Zenith_TerrainComponent::ValidateTerrainAssetSetTarget(const std::string& strAssetSet,
	const std::string& strTerrainRoot, const std::string& strResolvedTarget)
{
	if (!IsValidTerrainAssetSetName(strAssetSet))
	{
		return false;
	}

	std::error_code xError;
	const std::filesystem::path xCanonicalRoot = std::filesystem::weakly_canonical(
		std::filesystem::path(strTerrainRoot), xError);
	if (xError)
	{
		return false;
	}
	const std::filesystem::path xCanonicalTarget = std::filesystem::weakly_canonical(
		std::filesystem::path(strResolvedTarget), xError);
	if (xError)
	{
		return false;
	}

	// Require a component-wise root prefix. Named sets must add exactly their
	// validated component below the canonical root; legacy empty mode is the
	// root itself. Equality with the lexical expected target also rejects an
	// existing junction/symlink that canonicalizes into a sibling directory.
	auto xRootPart = xCanonicalRoot.begin();
	auto xTargetPart = xCanonicalTarget.begin();
	for (; xRootPart != xCanonicalRoot.end(); ++xRootPart, ++xTargetPart)
	{
		if (xTargetPart == xCanonicalTarget.end() || *xRootPart != *xTargetPart)
		{
			return false;
		}
	}
	const std::filesystem::path xExpectedCanonicalTarget = strAssetSet.empty()
		? xCanonicalRoot
		: (xCanonicalRoot / strAssetSet).lexically_normal();
	if (xCanonicalTarget != xExpectedCanonicalTarget ||
		(!strAssetSet.empty() && xTargetPart == xCanonicalTarget.end()))
	{
		return false;
	}

	return true;
}

bool Zenith_TerrainComponent::TryResolveValidatedTerrainAssetDirectory(
	const std::string& strAssetSet, std::string& strDirectoryOut)
{
	strDirectoryOut.clear();
	std::string strResolvedDirectory;
	if (!TryResolveTerrainAssetDirectory(strAssetSet, strResolvedDirectory))
	{
		return false;
	}
	const std::string strTerrainRoot =
		(std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").string();
	if (!ValidateTerrainAssetSetTarget(strAssetSet, strTerrainRoot, strResolvedDirectory))
	{
		return false;
	}
	strDirectoryOut = std::move(strResolvedDirectory);
	return true;
}

// Production cleanup wrapper re-runs the non-destructive canonical check
// immediately before deletion as defense in depth against target replacement.
bool Zenith_TerrainComponent::DeleteExistingTerrainFilesForAssetSet(
	const std::string& strAssetSet, const std::string& strResolvedDirectory)
{
	std::string strValidatedDirectory;
	if (!TryResolveValidatedTerrainAssetDirectory(strAssetSet, strValidatedDirectory) ||
		strValidatedDirectory != strResolvedDirectory)
	{
		return false;
	}
	return DeleteExistingTerrainFilesInDirectory(strValidatedDirectory);
}

// Private non-recursive core. Named sets also keep Height/Splatmap/GrassDensity
// textures here, so only direct generated .zmesh files are removed. Production
// reaches this only through the canonical wrapper; the friend test seam may use
// an arbitrary Build/artifacts sandbox.
bool Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(const std::string& strDirectory)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleting existing terrain meshes in %s", strDirectory.c_str());
	try
	{
		if (std::filesystem::exists(strDirectory))
		{
			for (const auto& entry : std::filesystem::directory_iterator(strDirectory))
			{
				if (entry.is_regular_file() && entry.path().extension().string() == ZENITH_MESH_EXT)
				{
					std::filesystem::remove(entry.path());
				}
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleted existing terrain meshes");
		}
		return true;
	}
	catch (const std::exception& e)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Warning: Failed to delete some terrain files: %s", e.what());
		return false;
	}
}

#ifdef ZENITH_TESTING
void Zenith_UnitTests::DeleteExistingTerrainFilesForTest(const std::string& strOutputDir)
{
	Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(strOutputDir);
}
#endif

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
		auto xhMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat = xhMat.GetDirect();
		if (pxMat) pxMat->SetName(strEntityName + "_Terrain_Mat" + std::to_string(u));
		m_axMaterials[u].Set(pxMat);
	}
}

// End-to-end execution of the Regenerate button: cleanup → delete files →
// export → reload physics → re-init render. Each step updates the export
// status string so the editor footer reflects progress.
bool Zenith_TerrainComponent::RunTerrainRegeneration(const std::string& strOutputDir)
{
	return RunTerrainRegenerationInternal(strOutputDir, nullptr);
}

// Terrain-editor bake entry point: identical pipeline, but the export source
// is the editor's live in-memory heightfield instead of a heightmap file.
bool Zenith_TerrainComponent::RegenerateFromHeightfield(const Zenith_Image& xHeightfield)
{
	const std::string strOutputDir = GetTerrainAssetDirectory();
	return RunTerrainRegenerationInternal(strOutputDir, &xHeightfield);
}

bool Zenith_TerrainComponent::RunTerrainRegenerationInternal(const std::string& strOutputDir, const Zenith_Image* pxHeightfield)
{
	const std::string strTerrainRoot =
		(std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").string();
	return RunTerrainRegenerationInternalForTerrainRoot(
		strTerrainRoot, strOutputDir, pxHeightfield);
}

bool Zenith_TerrainComponent::RunTerrainRegenerationInternalForTerrainRoot(
	const std::string& strTerrainRoot, const std::string& strOutputDir,
	const Zenith_Image* pxHeightfield)
{
	// Shared production/test operation gate. Canonical rejection is the first
	// action and therefore precedes progress, cleanup, deletion, export, and all
	// render/physics teardown.
	if (!ValidateTerrainAssetSetTarget(m_strTerrainAssetSet, strTerrainRoot, strOutputDir))
	{
		s_strTerrainExportStatus = "Terrain regeneration refused an unvalidated output directory.";
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[TerrainComponent] Refusing terrain regeneration outside the validated component asset set");
		return false;
	}
	const std::string& strValidatedDirectory = strOutputDir;

	s_bTerrainExportInProgress = true;
	s_strTerrainExportStatus = "Cleaning up existing terrain...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain regeneration...");

	if (!CleanupPriorGenerationForRegenerate(strValidatedDirectory))
	{
		s_bTerrainExportInProgress = false;
		s_strTerrainExportStatus = "Terrain regeneration refused unsafe cleanup target.";
		return false;
	}

	s_strTerrainExportStatus = "Deleting existing terrain meshes...";
	if (!DeleteExistingTerrainFilesForAssetSet(m_strTerrainAssetSet, strValidatedDirectory))
	{
		s_bTerrainExportInProgress = false;
		s_strTerrainExportStatus = "Terrain regeneration failed while cleaning existing meshes.";
		return false;
	}
	std::error_code xDirectoryError;
	std::filesystem::create_directories(strValidatedDirectory, xDirectoryError);
	if (xDirectoryError)
	{
		s_bTerrainExportInProgress = false;
		s_strTerrainExportStatus = "Terrain regeneration could not create its validated output directory.";
		return false;
	}

	s_strTerrainExportStatus = "Exporting new terrain meshes...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Exporting new terrain...");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strValidatedDirectory.c_str());
	if (pxHeightfield != nullptr)
	{
		ExportHeightmapFromMat(*pxHeightfield, strValidatedDirectory);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
		ExportHeightmapFromPaths(s_szHeightmapPath, strValidatedDirectory);
	}

	s_strTerrainExportStatus = "Initializing render resources...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Reinitializing render resources...");
	EnsureMaterialSlotsPopulated();
	InitializeRenderResources();

	s_strTerrainExportStatus = "Loading physics geometry...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading new physics geometry...");
	LoadCombinedPhysicsGeometry();

	s_bTerrainExportInProgress = false;
	const bool bInitialized = IsTerrainInitializedForEditor() &&
		HasPhysicsGeometry() && !m_bTerrainGeometryUnusable;
	if (bInitialized)
	{
		s_strTerrainExportStatus = "Terrain regenerated successfully!";
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain regeneration complete!");
	}
	else
	{
		s_strTerrainExportStatus = "Terrain regeneration failed to initialize complete render/physics state.";
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[TerrainComponent] Terrain regeneration did not produce complete live state");
	}
	return bInitialized;
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

	const std::string strOutputDir = GetTerrainAssetDirectory();
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
bool Zenith_TerrainComponent::CleanupPriorGenerationForRegenerate(const std::string& strValidatedDirectory)
{
	std::string strRevalidatedDirectory;
	if (!TryResolveValidatedTerrainAssetDirectory(m_strTerrainAssetSet, strRevalidatedDirectory) ||
		strRevalidatedDirectory != strValidatedDirectory)
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[TerrainComponent] Refusing live-resource teardown for unsafe terrain target");
		return false;
	}

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
	return true;
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

	if (Zenith_TextureAsset* pxSplatmap = m_xSplatmap.Resolve())
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
