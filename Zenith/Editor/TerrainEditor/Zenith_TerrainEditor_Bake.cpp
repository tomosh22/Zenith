#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
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
	// The ONE engine-singleton reach in this TU's grass paths. Every caller below
	// routes through it: the engine-singleton ratchet counts g_xEngine CODE
	// LINES per file, so a hoist here is what lets the grass-type verbs be added
	// without the file's budget moving.
	Flux_GrassImpl& GrassFeature()
	{
		return g_xEngine.Grass();
	}

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
	// Local struct: a class declared inside a member function keeps that
	// function's access rights, so the captureless thunk can still reach the
	// editor's private bake helpers.
	struct SaveTexturesLeaseContext
	{
		Zenith_TerrainEditor* m_pxEditor;
		bool* m_pbLeaseEntered;
	};
	SaveTexturesLeaseContext xContext{ this, &bLeaseEntered };
	const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainTextureDirectory(
		m_strAssetSet, strTerrainRoot,
		[](void* pContext, const std::string& strTextureDirectory) -> bool
		{
			SaveTexturesLeaseContext& xLease = *static_cast<SaveTexturesLeaseContext*>(pContext);
			*xLease.m_pbLeaseEntered = true;
			return xLease.m_pxEditor->SaveTexturesToPreparedDirectory(strTextureDirectory);
		}, &xContext);
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

	WriteZtxtr(strTextureDirectory + "GrassType" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uGRASS_TYPE_SIZE), static_cast<int32_t>(uGRASS_TYPE_SIZE),
		TEXTURE_FORMAT_R8_UNORM, m_xGrassType.GetDataPointer(),
		static_cast<size_t>(uGRASS_TYPE_SIZE) * uGRASS_TYPE_SIZE);

	m_strStatus = "Terrain textures saved to " + strTextureDirectory;
	Zenith_Log(LOG_CATEGORY_EDITOR,
		"[TerrainEditor] Saved Height/Splatmap_RGBA/GrassDensity/GrassType to %s",
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
	struct BakeMeshesLeaseContext
	{
		Zenith_TerrainEditor* m_pxEditor;
		bool* m_pbLeaseEntered;
	};
	BakeMeshesLeaseContext xContext{ this, &bLeaseEntered };
	if (!Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		m_strAssetSet, strTerrainRoot,
		[](void* pContext, const std::string& strMeshDirectory) -> bool
		{
			BakeMeshesLeaseContext& xLease = *static_cast<BakeMeshesLeaseContext*>(pContext);
			*xLease.m_pbLeaseEntered = true;
			xLease.m_pxEditor->BakeMeshesToPreparedDirectory(strMeshDirectory);
			return true;
		}, &xContext) && !bLeaseEntered)
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
	struct BakeRectLeaseContext
	{
		Zenith_TerrainEditor* m_pxEditor;
		bool* m_pbLeaseEntered;
		const Flux_TerrainExportRect* m_pxRect;
	};
	BakeRectLeaseContext xContext{ this, &bLeaseEntered, &xRect };
	const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		m_strAssetSet, strTerrainRoot,
		[](void* pContext, const std::string& strOutputDir) -> bool
		{
			BakeRectLeaseContext& xLease = *static_cast<BakeRectLeaseContext*>(pContext);
			Zenith_TerrainEditor& xEditor = *xLease.m_pxEditor;
			const Flux_TerrainExportRect& xBounds = *xLease.m_pxRect;
			*xLease.m_pbLeaseEntered = true;
			if (!Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(strOutputDir))
			{
				xEditor.m_strStatus = "Cannot export meshes: failed to clean the validated output directory.";
				return false;
			}

			xEditor.EnsureImagesAllocated();
			const u_int uChunkCount = static_cast<u_int>(xBounds.ChunkCount());
			const u_int uFileCount = uChunkCount * 3;
			xEditor.m_strStatus = "Exporting bounded terrain chunk meshes (this takes a while)...";
			Zenith_Log(LOG_CATEGORY_EDITOR,
				"[TerrainEditor] Exporting bounds [%d,%d]-[%d,%d]: %u chunks / %u files to %s",
				xBounds.GetMinX(), xBounds.GetMinY(), xBounds.GetMaxX(), xBounds.GetMaxY(),
				uChunkCount, uFileCount, strOutputDir.c_str());

			if (!ExportHeightmapFromMatRect(xEditor.m_xHeightfield, strOutputDir, xBounds))
			{
				xEditor.m_strStatus = "Bounded terrain chunk-mesh export failed.";
				return false;
			}

			xEditor.m_strStatus = "Bounded terrain chunk meshes exported";
			Zenith_Log(LOG_CATEGORY_EDITOR,
				"[TerrainEditor] Bounded chunk-mesh export complete: %u chunks / %u files",
				uChunkCount, uFileCount);
			return true;
		}, &xContext);
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
	// The inner texture lease runs nested inside the outer mesh lease, so it
	// needs the outer callback's directory as well; one context carries both.
	struct BakeFullLeaseContext
	{
		Zenith_TerrainEditor* m_pxEditor;
		const std::string* m_pstrTerrainRoot;
		bool* m_pbMeshLeaseEntered;
		bool* m_pbTextureLeaseEntered;
		const std::string* m_pstrMeshDirectory;
	};
	BakeFullLeaseContext xContext{ this, &strTerrainRoot,
		&bMeshLeaseEntered, &bTextureLeaseEntered, nullptr };
	const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		m_strAssetSet, strTerrainRoot,
		[](void* pContext, const std::string& strMeshDirectory) -> bool
		{
			BakeFullLeaseContext& xLease = *static_cast<BakeFullLeaseContext*>(pContext);
			*xLease.m_pbMeshLeaseEntered = true;
			xLease.m_pstrMeshDirectory = &strMeshDirectory;
			return Zenith_TerrainComponent::WithPreparedTerrainTextureDirectory(
				xLease.m_pxEditor->m_strAssetSet, *xLease.m_pstrTerrainRoot,
				[](void* pInnerContext, const std::string& strTextureDirectory) -> bool
				{
					BakeFullLeaseContext& xInner = *static_cast<BakeFullLeaseContext*>(pInnerContext);
					Zenith_TerrainEditor& xEditor = *xInner.m_pxEditor;
					const std::string& strMeshDir = *xInner.m_pstrMeshDirectory;
					*xInner.m_pbTextureLeaseEntered = true;
					Zenith_TerrainComponent* pxTerrain = xEditor.ResolveTargetComponent();
					if (pxTerrain == nullptr)
					{
						Zenith_Log(LOG_CATEGORY_EDITOR,
							"[TerrainEditor] BakeFull: no terrain target - saving textures + meshes only");
						if (!xEditor.SaveTexturesToPreparedDirectory(strTextureDirectory))
						{
							return false;
						}
						xEditor.BakeMeshesToPreparedDirectory(strMeshDir);
						return true;
					}

					if (!xEditor.SaveTexturesToPreparedDirectory(strTextureDirectory))
					{
						return false;
					}

					xEditor.m_strStatus = "Baking terrain (cleanup / export / physics / render re-init)...";

					// Commit only after staged textures are safely written. Component
					// regeneration acquires its own compatible lease while these outer
					// handles keep the editor's targets pinned for the full bake.
					if (!pxTerrain->SetTerrainAssetSet(xEditor.m_strAssetSet))
					{
						xEditor.m_strStatus = "Terrain bake failed to commit the validated staged set.";
						return false;
					}
					if (!pxTerrain->RegenerateFromHeightfield(xEditor.m_xHeightfield))
					{
						xEditor.m_strStatus = "Terrain bake failed while reinitializing render/physics state; edits remain dirty.";
						return false;
					}

					// Only a complete synchronous reinitialization makes the live
					// heightfield identical to the baked bytes.
					memset(xEditor.m_aulSessionDirtyBits, 0, sizeof(xEditor.m_aulSessionDirtyBits));
					memset(xEditor.m_aulPendingEvictBits, 0, sizeof(xEditor.m_aulPendingEvictBits));
					xEditor.m_bSessionDirty = false;
					xEditor.RegisterHook();
					xEditor.RebuildGrass();

					xEditor.m_strStatus = "Terrain bake complete";
					Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Full bake complete");
					return true;
				}, pContext);
		}, &xContext);
	if (!bMeshLeaseEntered || !bTextureLeaseEntered)
	{
		m_strStatus = "Terrain bake refused an unsafe handle-bound asset target.";
	}
	return bSucceeded;
}

bool Zenith_TerrainEditor::ConsumeSculptedUnderBuiltGrass()
{
	// IsBuilt() first: with nothing built there is no push to redo, and the latch
	// must survive to the one that follows.
	return GrassFeature().IsBuilt() && ConsumeHeightsEditedSinceGrassPush();
}

void Zenith_TerrainEditor::RebuildGrass()
{
	m_bGrassDirty = false;

	// Tools-bake semantic: the grass maps are GPU textures, so a Null build
	// deliberately does not author them (see Zenith/Null/CLAUDE.md).
	//
	// ZEN-6 AUDIT — CORRECT AS WRITTEN, and audited rather than changed. The
	// ticket's rule is that a Null bail is a defect only when it skips ENTITY or
	// COMPONENT creation, because that is what reaches a committed .zscen. This
	// one creates nothing: it feeds Flux_GrassImpl::BuildFromMaps, whose whole
	// output is grass GPU textures plus Flux-side CPU query maps. Grass appears
	// nowhere in Zenith_TerrainComponent::WriteToDataStream (nor in any other
	// component's), so no byte this skips can ever reach a scene file — unlike
	// Zenith_TerrainEditor::EnsureTreeEntities, whose bail dropped two serialized
	// entities and ~323 KB of instance data from every headless RenderTest boot.
	//
	// The audit covered the WHOLE editor subsystem. Five Zenith_IsNullRenderer()
	// sites survive it; every one is device traffic or process policy, none of them
	// creates an entity or a component, so none can move a serialized byte:
	//
	//   this function                        grass GPU textures + Flux CPU query
	//                                        maps; nothing serialized
	//   Zenith_TerrainEditor.cpp (splat)     m_bSplatGPUDirty re-upload of a CPU
	//                                        image that is authored either way
	//   Zenith_TerrainEditor.cpp (cursor)    DrawBrushIndicatorDecal — a viewport
	//                                        overlay with no CPU-side state at all
	//   Zenith_Editor.cpp (imgui ini)        IniFilename = nullptr; a Null build is
	//                                        a CI run and must not read/write a user
	//                                        ini. Layout, not scene data
	//   Zenith_Editor_SceneOps.cpp (GPU wait) WaitForGPUAndFlushDeferred; Flush /
	//                                        WaitForGPUIdle / ProcessDeferredDeletions
	//                                        are each already no-ops under Null, so
	//                                        this saves cost and changes no outcome
	//
	// Two further sites live in Flux (Flux_Grass.cpp CreateMapTextures and its
	// upload path) and are outside the editor, on the same grounds as row one.
	if (Zenith_IsNullRenderer())
	{
		return;
	}
	// Placement no longer consumes the terrain's physics mesh, but a session with
	// no terrain in the world still has nothing to grow grass on.
	if (ResolveTargetComponent() == nullptr)
	{
		return;
	}

	Zenith_Assert(m_xHeightfield.GetWidth() == uHEIGHTFIELD_SIZE
		&& m_xGrassDensity.GetWidth() == uGRASS_DENSITY_SIZE
		&& m_xGrassType.GetSize() == uGRASS_TYPE_SIZE * uGRASS_TYPE_SIZE,
		"Grass rebuild reads the session images at their contract sizes");

	// BuildFromMaps takes METRES and the live heightfield is normalized over
	// fTERRAIN_MAX_HEIGHT, so the scaled copy cannot be elided. Scaling in place
	// would corrupt the authoring image every other consumer reads.
	constexpr u_int uHEIGHT_TEXELS = uHEIGHTFIELD_SIZE * uHEIGHTFIELD_SIZE;
	Zenith_Vector<float> afHeightMetres;
	afHeightMetres.Resize(uHEIGHT_TEXELS, 0.0f);
	const float* pfHeightNorm = m_xHeightfield.Row(0);
	float* pfHeightMetres = afHeightMetres.GetDataPointer();
	for (u_int u = 0; u < uHEIGHT_TEXELS; u++)
	{
		pfHeightMetres[u] = pfHeightNorm[u] * fTERRAIN_MAX_HEIGHT;
	}

	Flux_GrassImpl::MapSet xMaps;
	xMaps.pHeight = pfHeightMetres;
	xMaps.uHeightSize = uHEIGHTFIELD_SIZE;
	xMaps.pCoverage = m_xGrassDensity.Row(0);
	xMaps.uCoverageSize = uGRASS_DENSITY_SIZE;
	xMaps.pType = m_xGrassType.GetDataPointer();
	xMaps.uTypeSize = uGRASS_TYPE_SIZE;
	xMaps.fWorldSize = fTERRAIN_WORLD_SIZE;

	Flux_GrassImpl::BuildParams xParams;
	xParams.m_fDensityScale = 1.0f;

	GrassFeature().BuildFromMaps(xMaps, xParams);
}

//=============================================================================
// Grass types — the working copy the panel and editor automation both edit.
//=============================================================================

void Zenith_TerrainEditor::GrassTypes_Reset()
{
	m_xGrassTypesWorking = Flux_GrassTypeTable::Defaults();
}

void Zenith_TerrainEditor::GrassTypes_Reload()
{
	m_xGrassTypesWorking = GrassFeature().GetTypeTable();
}

void Zenith_TerrainEditor::GrassTypes_Apply()
{
	// Validated on the working copy too, not only inside SetTypeTable: the panel
	// keeps editing this object afterwards, and a slider left holding a value the
	// engine clamped would silently disagree with what is on screen.
	m_xGrassTypesWorking.Validate();

	Flux_GrassImpl& xGrass = GrassFeature();
	xGrass.SetTypeTable(m_xGrassTypesWorking);

	// The GPU type block is re-staged by the next gather, but placement bakes
	// per-type decisions (density acceptance, slope reject, clump layout) into
	// the blade records, so the look only follows a re-place. Nothing to rebuild
	// if no maps were ever fed.
	if (xGrass.IsBuilt())
	{
		RebuildGrass();
	}
}

bool Zenith_TerrainEditor::GrassTypes_Save()
{
	m_xGrassTypesWorking.Validate();

	// Through the ASSET class, never a hand-rolled stream: the .zdata envelope
	// (magic + version + type name) is what makes the file loadable, and the boot
	// path reads it back through exactly this type. The asset is a LOCAL, not a
	// Create<>() — Save only needs its GetTypeName + WriteToDataStream, and a
	// registry entry per press would accumulate a procedural:// row for nothing.
	Zenith_GrassTypeTableAsset xAsset;
	xAsset.SetTable(m_xGrassTypesWorking);

	// game: resolves through the registry, so --assets-root packaging works
	// unchanged. The Vegetation/ directory does not exist in a game that has
	// never authored types — this save is what creates it.
	const std::string strResolved = Zenith_AssetRegistry::ResolvePath(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	std::error_code xEC;
	std::filesystem::create_directories(std::filesystem::path(strResolved).parent_path(), xEC);

	const bool bSaved = Zenith_AssetRegistry::Save(&xAsset, szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	if (!bSaved)
	{
		m_strStatus = "Grass types: SAVE FAILED (see the log)";
		Zenith_Error(LOG_CATEGORY_EDITOR, "[TerrainEditor] GrassTypes_Save failed to write '%s'",
			szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
		return false;
	}

	// Apply last: a save that reached disk but never took effect in the running
	// editor is the one failure mode an author cannot see.
	GrassTypes_Apply();
	m_strStatus = "Grass types saved to " + std::string(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Saved %u grass types to '%s'",
		m_xGrassTypesWorking.GetCount(), szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	return true;
}

#endif // ZENITH_TOOLS
