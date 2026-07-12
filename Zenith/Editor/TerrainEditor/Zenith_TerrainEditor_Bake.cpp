#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"

#include "Core/Zenith_CommandLine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

// Tools export entry point (extern declaration).
extern void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir);
extern bool ExportHeightmapFromMatRect(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Flux_TerrainExportRect& xRect);

//=============================================================================
// Persistence: texture save (.ztxtr to the game assets dir), chunk-mesh bake,
// and the full Regenerate-pattern bake on the target component.
//=============================================================================

namespace
{
	std::string GetProjectTerrainRoot()
	{
		return (std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").string();
	}

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

bool Zenith_TerrainEditor::ResolveValidatedTargetForTerrainRoot(
	const std::string& strTerrainRoot, std::string& strDirectoryOut) const
{
	const std::filesystem::path xRoot(strTerrainRoot);
	const std::filesystem::path xTarget = m_strAssetSet.empty()
		? xRoot
		: xRoot / m_strAssetSet;
	strDirectoryOut = xTarget.generic_string();
	while (!strDirectoryOut.empty() && strDirectoryOut.back() == '/')
	{
		strDirectoryOut.pop_back();
	}
	strDirectoryOut += '/';
	if (!Zenith_TerrainComponent::ValidateTerrainAssetSetTarget(
		m_strAssetSet, strTerrainRoot, strDirectoryOut))
	{
		strDirectoryOut.clear();
		return false;
	}
	return true;
}

void Zenith_TerrainEditor::RegenerateBrushTextures()
{
	// Brush indicator mask, sampled by the decal Apply shader's textured-
	// brush mode across the brush's full diameter (UV [0,1]^2, brush circle
	// inscribed). RGB stays white — the per-tool tint is applied per-decal —
	// and ALPHA carries the mask: a bright rim ring at the brush radius, a
	// soft interior falloff gradient, and a centre dot.
	constexpr u_int uSIZE = 256;
	static u_int8 s_aucPixels[uSIZE * uSIZE * 4];

	auto SmoothStep = [](float fEdge0, float fEdge1, float fX) -> float
	{
		const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
		return fT * fT * (3.0f - 2.0f * fT);
	};

	for (u_int uY = 0; uY < uSIZE; uY++)
	{
		for (u_int uX = 0; uX < uSIZE; uX++)
		{
			const float fDX = ((static_cast<float>(uX) + 0.5f) / uSIZE) * 2.0f - 1.0f;
			const float fDY = ((static_cast<float>(uY) + 0.5f) / uSIZE) * 2.0f - 1.0f;
			const float fR = sqrtf(fDX * fDX + fDY * fDY);   // 1.0 == brush radius

			// Bright rim ring just inside the radius.
			const float fRing = SmoothStep(0.86f, 0.92f, fR) * (1.0f - SmoothStep(0.95f, 1.0f, fR));
			// Soft interior gradient — strongest at the centre, reads as the
			// falloff-weighted application area.
			const float fDisc = (1.0f - SmoothStep(0.0f, 0.92f, fR)) * 0.40f;
			// Centre dot marking the exact cursor position.
			const float fDot = 1.0f - SmoothStep(0.025f, 0.055f, fR);

			const float fMask = std::clamp(std::max(std::max(fRing, fDisc), fDot), 0.0f, 1.0f);

			u_int8* pucPixel = &s_aucPixels[(uY * uSIZE + uX) * 4];
			pucPixel[0] = 255;
			pucPixel[1] = 255;
			pucPixel[2] = 255;
			pucPixel[3] = static_cast<u_int8>(fMask * 255.0f + 0.5f);
		}
	}

	const std::string strDir = std::string(ENGINE_ASSETS_DIR) + "Textures/Brushes/";
	std::error_code xError;
	std::filesystem::create_directories(strDir, xError);
	if (xError)
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "[TerrainEditor] Failed to create brush texture dir '%s' (%s)", strDir.c_str(), xError.message().c_str());
		return;
	}

	WriteZtxtr(strDir + "BrushIndicator" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uSIZE), static_cast<int32_t>(uSIZE),
		TEXTURE_FORMAT_RGBA8_UNORM, s_aucPixels, sizeof(s_aucPixels));

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Regenerated brush indicator texture (%ux%u) in %s", uSIZE, uSIZE, strDir.c_str());
}

bool Zenith_TerrainEditor::SaveTextures()
{
	return SaveTexturesForTerrainRoot(GetProjectTerrainRoot());
}

bool Zenith_TerrainEditor::SaveTexturesForTerrainRoot(const std::string& strTerrainRoot)
{
	EnsureImagesAllocated();

	std::string strValidatedMeshDirectory;
	if (!ResolveValidatedTargetForTerrainRoot(strTerrainRoot, strValidatedMeshDirectory))
	{
		m_strAssetSetValidationError = "Cannot save textures: the staged terrain set is invalid.";
		m_strStatus = m_strAssetSetValidationError;
		return false;
	}

	const std::string strTexDir = m_strAssetSet.empty()
		? (std::filesystem::path(strTerrainRoot).parent_path() / "Textures" / "Terrain").generic_string() + "/"
		: strValidatedMeshDirectory;
	if (!m_strAssetSet.empty() && strTexDir != strValidatedMeshDirectory)
	{
		m_strStatus = "Cannot save textures: resolved target diverged from its validated terrain set.";
		return false;
	}
	std::error_code xDirectoryError;
	std::filesystem::create_directories(strTexDir, xDirectoryError);
	if (xDirectoryError)
	{
		m_strStatus = "Failed to create terrain texture directory: " + xDirectoryError.message();
		return false;
	}

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
	return true;
}

void Zenith_TerrainEditor::BakeMeshes()
{
	BakeMeshesForTerrainRoot(GetProjectTerrainRoot());
}

void Zenith_TerrainEditor::BakeMeshesForTerrainRoot(const std::string& strTerrainRoot)
{
	EnsureImagesAllocated();

	std::string strOutputDir;
	if (!ResolveValidatedTargetForTerrainRoot(strTerrainRoot, strOutputDir))
	{
		m_strStatus = "Cannot export meshes: the staged terrain target is unsafe.";
		return;
	}
	std::error_code xDirectoryError;
	std::filesystem::create_directories(strOutputDir, xDirectoryError);
	if (xDirectoryError)
	{
		m_strStatus = "Cannot export meshes: failed to create validated output directory.";
		return;
	}

	m_strStatus = "Exporting terrain chunk meshes (this takes a while)...";
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Exporting chunk meshes to %s", strOutputDir.c_str());

	ExportHeightmapFromMat(m_xHeightfield, strOutputDir);

	m_strStatus = "Terrain chunk meshes exported";
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Chunk-mesh export complete");
}

bool Zenith_TerrainEditor::BakeMeshesRect(const Flux_TerrainExportRect& xRect)
{
	return BakeMeshesRectForTerrainRoot(xRect, GetProjectTerrainRoot());
}

bool Zenith_TerrainEditor::BakeMeshesRectForTerrainRoot(
	const Flux_TerrainExportRect& xRect, const std::string& strTerrainRoot)
{
	// Rect validation is deliberately first: a bad automation payload must not
	// allocate editor maps, resolve/create directories, or delete prior meshes.
	if (!xRect.IsValid())
	{
		m_strStatus = "Cannot export meshes: invalid terrain chunk rectangle.";
		return false;
	}

	std::string strOutputDir;
	if (!ResolveValidatedTargetForTerrainRoot(strTerrainRoot, strOutputDir))
	{
		m_strStatus = "Cannot export meshes: the staged terrain target is unsafe.";
		return false;
	}

	// The public production path revalidates against the project root directly
	// before deletion. The alternate-root branch is reachable only through the
	// private Zenith_UnitTests friend seam and has already passed the same E1
	// canonical containment check above.
	const std::string strProjectTerrainRoot = GetProjectTerrainRoot();
	const bool bProductionRoot = std::filesystem::path(strTerrainRoot).lexically_normal() ==
		std::filesystem::path(strProjectTerrainRoot).lexically_normal();
	const bool bCleaned = bProductionRoot
		? Zenith_TerrainComponent::DeleteExistingTerrainFilesForAssetSet(m_strAssetSet, strOutputDir)
		: Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(strOutputDir);
	if (!bCleaned)
	{
		m_strStatus = "Cannot export meshes: failed to clean the validated output directory.";
		return false;
	}

	std::error_code xDirectoryError;
	std::filesystem::create_directories(strOutputDir, xDirectoryError);
	if (xDirectoryError)
	{
		m_strStatus = "Cannot export meshes: failed to create validated output directory.";
		return false;
	}

	EnsureImagesAllocated();
	const u_int uChunkCount = static_cast<u_int>(xRect.ChunkCount());
	const u_int uFileCount = uChunkCount * 3;
	m_strStatus = "Exporting bounded terrain chunk meshes (this takes a while)...";
	Zenith_Log(LOG_CATEGORY_EDITOR,
		"[TerrainEditor] Exporting bounds [%d,%d]-[%d,%d]: %u chunks / %u files to %s",
		xRect.GetMinX(), xRect.GetMinY(), xRect.GetMaxX(), xRect.GetMaxY(),
		uChunkCount, uFileCount, strOutputDir.c_str());

	if (!ExportHeightmapFromMatRect(m_xHeightfield, strOutputDir, xRect))
	{
		m_strStatus = "Bounded terrain chunk-mesh export failed.";
		return false;
	}

	m_strStatus = "Bounded terrain chunk meshes exported";
	Zenith_Log(LOG_CATEGORY_EDITOR,
		"[TerrainEditor] Bounded chunk-mesh export complete: %u chunks / %u files",
		uChunkCount, uFileCount);
	return true;
}

void Zenith_TerrainEditor::BakeFull()
{
	BakeFullForTerrainRoot(GetProjectTerrainRoot());
}

bool Zenith_TerrainEditor::BakeFullForTerrainRoot(const std::string& strTerrainRoot)
{
	// This is the shared production/test operation gate: canonical rejection is
	// the first action, before texture/mesh writes, component commit, progress,
	// or live regeneration/teardown.
	std::string strValidatedMeshDirectory;
	if (!ResolveValidatedTargetForTerrainRoot(strTerrainRoot, strValidatedMeshDirectory))
	{
		m_strStatus = "Terrain bake refused an unsafe canonical target.";
		return false;
	}

	Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
	if (pxTerrain == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] BakeFull: no terrain target - saving textures + meshes only");
		if (SaveTexturesForTerrainRoot(strTerrainRoot))
		{
			BakeMeshesForTerrainRoot(strTerrainRoot);
			return true;
		}
		return false;
	}

	if (!SaveTexturesForTerrainRoot(strTerrainRoot))
	{
		return false;
	}

	m_strStatus = "Baking terrain (cleanup / export / physics / render re-init)...";

	// Commit only after staged textures are safely written. The following call
	// is synchronous and derives cleanup/export paths from this same committed
	// set, preventing mixed old-live/new-bake state.
	std::string strCommitDirectory;
	if (!ResolveValidatedTargetForTerrainRoot(strTerrainRoot, strCommitDirectory) ||
		strCommitDirectory != strValidatedMeshDirectory)
	{
		m_strStatus = "Terrain bake refused an unsafe target before component commit.";
		return false;
	}
	if (!pxTerrain->SetTerrainAssetSet(m_strAssetSet))
	{
		m_strStatus = "Terrain bake failed to commit the validated staged set.";
		return false;
	}
	const bool bRegenerated = pxTerrain->RegenerateFromHeightfield(m_xHeightfield);
	if (!bRegenerated)
	{
		m_strStatus = "Terrain bake failed while reinitializing render/physics state; edits remain dirty.";
		return false;
	}

	// Only a complete synchronous reinitialization makes the live heightfield
	// identical to the baked bytes. Dirty state must survive every failure path.
	memset(m_aulSessionDirtyBits, 0, sizeof(m_aulSessionDirtyBits));
	memset(m_aulPendingEvictBits, 0, sizeof(m_aulPendingEvictBits));
	m_bSessionDirty = false;

	// The streaming-state object survives the regenerate (only its buffers are
	// recreated), but re-pin the hook defensively.
	RegisterHook();

	RebuildGrass();

	m_strStatus = "Terrain bake complete";
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Full bake complete");
	return true;
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
