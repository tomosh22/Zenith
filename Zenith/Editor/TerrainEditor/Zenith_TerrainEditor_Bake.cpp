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
	bool bLeaseEntered = false;
	const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainTextureDirectory(
		m_strAssetSet, strTerrainRoot,
		[&](const std::string& strTextureDirectory)
		{
			bLeaseEntered = true;
			return SaveTexturesToPreparedDirectory(strTextureDirectory);
		});
	if (!bLeaseEntered)
	{
		m_strAssetSetValidationError =
			"Cannot save textures: the staged terrain texture target is unsafe.";
		m_strStatus = m_strAssetSetValidationError;
	}
	return bSucceeded;
}

bool Zenith_TerrainEditor::SaveTexturesToPreparedDirectory(
	const std::string& strTextureDirectory)
{
	EnsureImagesAllocated();
	m_strStatus = "Saving terrain textures...";

	WriteZtxtr(strTextureDirectory + "Height" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uHEIGHTFIELD_SIZE), static_cast<int32_t>(uHEIGHTFIELD_SIZE),
		TEXTURE_FORMAT_R32_SFLOAT, m_xHeightfield.Row(0),
		static_cast<size_t>(uHEIGHTFIELD_SIZE) * uHEIGHTFIELD_SIZE * sizeof(float));

	WriteZtxtr(strTextureDirectory + "Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uSPLATMAP_SIZE), static_cast<int32_t>(uSPLATMAP_SIZE),
		TEXTURE_FORMAT_RGBA8_UNORM, m_xSplatmap.GetDataPointer(),
		static_cast<size_t>(uSPLATMAP_SIZE) * uSPLATMAP_SIZE * 4);

	WriteZtxtr(strTextureDirectory + "GrassDensity" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uGRASS_DENSITY_SIZE), static_cast<int32_t>(uGRASS_DENSITY_SIZE),
		TEXTURE_FORMAT_R32_SFLOAT, m_xGrassDensity.Row(0),
		static_cast<size_t>(uGRASS_DENSITY_SIZE) * uGRASS_DENSITY_SIZE * sizeof(float));

	m_strStatus = "Terrain textures saved to " + strTextureDirectory;
	Zenith_Log(LOG_CATEGORY_EDITOR,
		"[TerrainEditor] Saved Height/Splatmap_RGBA/GrassDensity to %s",
		strTextureDirectory.c_str());
	return true;
}

void Zenith_TerrainEditor::BakeMeshes()
{
	BakeMeshesForTerrainRoot(GetProjectTerrainRoot());
}

void Zenith_TerrainEditor::BakeMeshesForTerrainRoot(const std::string& strTerrainRoot)
{
	bool bLeaseEntered = false;
	if (!Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		m_strAssetSet, strTerrainRoot,
		[&](const std::string& strMeshDirectory)
		{
			bLeaseEntered = true;
			BakeMeshesToPreparedDirectory(strMeshDirectory);
			return true;
		}) && !bLeaseEntered)
	{
		m_strStatus = "Cannot export meshes: the staged terrain target is unsafe.";
	}
}

void Zenith_TerrainEditor::BakeMeshesToPreparedDirectory(
	const std::string& strMeshDirectory)
{
	EnsureImagesAllocated();

	m_strStatus = "Exporting terrain chunk meshes (this takes a while)...";
	Zenith_Log(LOG_CATEGORY_EDITOR,
		"[TerrainEditor] Exporting chunk meshes to %s", strMeshDirectory.c_str());

	ExportHeightmapFromMat(m_xHeightfield, strMeshDirectory);

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

	bool bLeaseEntered = false;
	const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		m_strAssetSet, strTerrainRoot,
		[&](const std::string& strOutputDir)
		{
			bLeaseEntered = true;
			if (!Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(strOutputDir))
			{
				m_strStatus = "Cannot export meshes: failed to clean the validated output directory.";
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
		});
	if (!bLeaseEntered)
	{
		m_strStatus = "Cannot export meshes: the staged terrain target is unsafe.";
	}
	return bSucceeded;
}

void Zenith_TerrainEditor::BakeFull()
{
	BakeFullForTerrainRoot(GetProjectTerrainRoot());
}

bool Zenith_TerrainEditor::BakeFullForTerrainRoot(const std::string& strTerrainRoot)
{
	// Keep both actual destinations leased for the entire synchronous bake. For
	// named sets these are two compatible leases on the same directory; legacy
	// mode additionally secures the Assets/Textures/Terrain sibling.
	bool bMeshLeaseEntered = false;
	bool bTextureLeaseEntered = false;
	const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		m_strAssetSet, strTerrainRoot,
		[&](const std::string& strMeshDirectory)
		{
			bMeshLeaseEntered = true;
			return Zenith_TerrainComponent::WithPreparedTerrainTextureDirectory(
				m_strAssetSet, strTerrainRoot,
				[&](const std::string& strTextureDirectory)
				{
					bTextureLeaseEntered = true;
					Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
					if (pxTerrain == nullptr)
					{
						Zenith_Log(LOG_CATEGORY_EDITOR,
							"[TerrainEditor] BakeFull: no terrain target - saving textures + meshes only");
						if (!SaveTexturesToPreparedDirectory(strTextureDirectory))
						{
							return false;
						}
						BakeMeshesToPreparedDirectory(strMeshDirectory);
						return true;
					}

					if (!SaveTexturesToPreparedDirectory(strTextureDirectory))
					{
						return false;
					}

					m_strStatus = "Baking terrain (cleanup / export / physics / render re-init)...";

					// Commit only after staged textures are safely written. Component
					// regeneration acquires its own compatible lease while these outer
					// handles keep the editor's targets pinned for the full bake.
					if (!pxTerrain->SetTerrainAssetSet(m_strAssetSet))
					{
						m_strStatus = "Terrain bake failed to commit the validated staged set.";
						return false;
					}
					if (!pxTerrain->RegenerateFromHeightfield(m_xHeightfield))
					{
						m_strStatus = "Terrain bake failed while reinitializing render/physics state; edits remain dirty.";
						return false;
					}

					// Only a complete synchronous reinitialization makes the live
					// heightfield identical to the baked bytes.
					memset(m_aulSessionDirtyBits, 0, sizeof(m_aulSessionDirtyBits));
					memset(m_aulPendingEvictBits, 0, sizeof(m_aulPendingEvictBits));
					m_bSessionDirty = false;
					RegisterHook();
					RebuildGrass();

					m_strStatus = "Terrain bake complete";
					Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Full bake complete");
					return true;
				});
		});
	if (!bMeshLeaseEntered || !bTextureLeaseEntered)
	{
		m_strStatus = "Terrain bake refused an unsafe handle-bound asset target.";
	}
	return bSucceeded;
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
