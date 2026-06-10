#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"

#include "Core/Zenith_CommandLine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"

#include <filesystem>

// Game-project hook + tools export entry points (extern declarations, same
// pattern as Zenith_TerrainComponent_Editor.cpp).
extern const char* Project_GetGameAssetsDirectory();
extern void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir);

//=============================================================================
// Persistence: texture save (.ztxtr to the game assets dir), chunk-mesh bake,
// and the full Regenerate-pattern bake on the target component.
//=============================================================================

namespace
{
	// .ztxtr writer matching Zenith_TextureAsset's loader layout:
	// i32 width, i32 height, i32 depth, TextureFormat, u64 dataSize, pixels.
	void WriteZtxtr(const std::string& strPath, int32_t iWidth, int32_t iHeight,
		TextureFormat eFormat, const void* pPixels, size_t ulDataSize)
	{
		Zenith_DataStream xStream;
		xStream << iWidth;
		xStream << iHeight;
		xStream << static_cast<int32_t>(1);
		xStream << eFormat;
		xStream << ulDataSize;
		xStream.WriteData(pPixels, ulDataSize);
		xStream.WriteToFile(strPath.c_str());
	}
}

void Zenith_TerrainEditor::SaveTextures()
{
	EnsureImagesAllocated();

	const std::string strTexDir = std::string(Project_GetGameAssetsDirectory()) + "Textures/Terrain/";
	std::filesystem::create_directories(strTexDir);

	m_strStatus = "Saving terrain textures...";

	WriteZtxtr(strTexDir + "Height" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uHEIGHTFIELD_SIZE), static_cast<int32_t>(uHEIGHTFIELD_SIZE),
		TEXTURE_FORMAT_R32_SFLOAT, m_xHeightfield.Row(0),
		static_cast<size_t>(uHEIGHTFIELD_SIZE) * uHEIGHTFIELD_SIZE * sizeof(float));

	WriteZtxtr(strTexDir + "Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uSPLATMAP_SIZE), static_cast<int32_t>(uSPLATMAP_SIZE),
		TEXTURE_FORMAT_RGBA8_UNORM, m_xSplatmap.GetDataPointer(),
		static_cast<size_t>(uSPLATMAP_SIZE) * uSPLATMAP_SIZE * 4);

	WriteZtxtr(strTexDir + "GrassDensity" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uGRASS_DENSITY_SIZE), static_cast<int32_t>(uGRASS_DENSITY_SIZE),
		TEXTURE_FORMAT_R32_SFLOAT, m_xGrassDensity.Row(0),
		static_cast<size_t>(uGRASS_DENSITY_SIZE) * uGRASS_DENSITY_SIZE * sizeof(float));

	m_strStatus = "Terrain textures saved to " + strTexDir;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Saved Height/Splatmap_RGBA/GrassDensity to %s", strTexDir.c_str());
}

void Zenith_TerrainEditor::BakeMeshes()
{
	EnsureImagesAllocated();

	const std::string strOutputDir = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
	std::filesystem::create_directories(strOutputDir);

	m_strStatus = "Exporting terrain chunk meshes (this takes a while)...";
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Exporting chunk meshes to %s", strOutputDir.c_str());

	ExportHeightmapFromMat(m_xHeightfield, strOutputDir);

	m_strStatus = "Terrain chunk meshes exported";
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Chunk-mesh export complete");
}

void Zenith_TerrainEditor::BakeFull()
{
	Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
	if (pxTerrain == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] BakeFull: no terrain target - saving textures + meshes only");
		SaveTextures();
		BakeMeshes();
		return;
	}

	SaveTextures();

	// The freshly-exported chunks ARE the live heightfield now — clear the
	// session-dirty state BEFORE the re-init so the hook no-ops on every
	// post-bake stream-in (baked bytes pass through untouched).
	memset(m_aulSessionDirtyBits, 0, sizeof(m_aulSessionDirtyBits));
	memset(m_aulPendingEvictBits, 0, sizeof(m_aulPendingEvictBits));
	m_bSessionDirty = false;

	m_strStatus = "Baking terrain (cleanup / export / physics / render re-init)...";

	// The existing Regenerate pattern: cleanup -> delete files ->
	// ExportHeightmapFromMat -> physics reload -> InitializeRenderResources.
	// Recomputes exact chunk AABBs and refreshes the LOW LOD too.
	pxTerrain->RegenerateFromHeightfield(m_xHeightfield);

	// The streaming-state object survives the regenerate (only its buffers are
	// recreated), but re-pin the hook defensively.
	RegisterHook();

	RebuildGrass();

	m_strStatus = "Terrain bake complete";
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Full bake complete");
}

void Zenith_TerrainEditor::RebuildGrass()
{
	m_bGrassDirty = false;

	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}
	Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
	if (pxTerrain == nullptr || !pxTerrain->HasPhysicsGeometry())
	{
		return;
	}

	g_xEngine.Grass().SetDensityMap(m_xGrassDensity.Row(0),
		uGRASS_DENSITY_SIZE, uGRASS_DENSITY_SIZE, fTERRAIN_WORLD_SIZE);
	g_xEngine.Grass().GenerateFromTerrain(pxTerrain->GetPhysicsMeshGeometry());
}

#endif // ZENITH_TOOLS
