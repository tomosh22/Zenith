#pragma once

#include <string>

#include "AssetHandling/Zenith_Image.h"
#include "Core/Zenith_TerrainDimensions.h"
#include "Flux/Terrain/Flux_TerrainExportRect.h"

/**
 * Export terrain meshes from heightmap.
 *
 * This function generates all LOD levels plus physics meshes for
 * the entire terrain grid. The resulting files follow the naming convention
 * required by Flux_TerrainStreamingManager:
 *   - Render_X_Y ZENITH_MESH_EXT      (HIGH detail, streamed dynamically)
 *   - Render_LOW_X_Y ZENITH_MESH_EXT  (LOW detail, always resident)
 *   - Physics_X_Y ZENITH_MESH_EXT     (Physics collision mesh)
 *
 * where X and Y are chunk coordinates in the active grid xDims describes, which
 * may be smaller than the fixed 64x64 chunk capacity and need not be square.
 *
 * A TerrainDims.zdata manifest recording xDims is written beside the chunks.
 * A baked set without one is treated as stale by the runtime loader, because a
 * quantised chunk cannot be decoded without the dimensions it was baked at.
 *
 * @param strHeightmapPath    Full path to the heightmap texture (.ztxtr or 16-bit PNG)
 * @param strOutputDir        Full path to output directory (must end with '/' or '\')
 * @param xDims               Chunk size, vertex density and grid extent to bake at
 */
void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims);

/**
 * Export terrain meshes from an already-loaded heightmap image.
 * Avoids re-loading from disk when the heightmap is already in memory.
 *
 * @param xHeightmap    Single-channel float heightmap, square, spanning
 *                      [0, xDims.MaxWorldSize()] in both axes
 * @param strOutputDir  Full path to output directory (must end with '/' or '\')
 * @param xDims         Chunk size, vertex density and grid extent to bake at
 */
void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims);

/**
 * Export HIGH, LOW and physics meshes for only the chunks inside xRect.
 * Chunk filenames retain their absolute coordinates in the active grid.
 * Returns false without exporting when the image, dimensions or rectangle are
 * invalid, or when the set's existing TerrainDims.zdata manifest disagrees with
 * xDims -- a partial re-bake into a set baked at other dimensions is corruption,
 * so it is refused rather than performed.
 */
bool ExportHeightmapFromMatRect(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Zenith_TerrainDimensions& xDims,
	const Flux_TerrainExportRect& xRect);

/**
 * Load a heightmap from .ztxtr or a common image format (PNG etc.) into a
 * single-channel float image normalized to [0,1]. Returns an empty image on
 * failure. Exposed for the terrain editor, which seeds its live heightfield
 * through the same loader the export pipeline uses.
 */
Zenith_Image Zenith_Tools_LoadHeightmapAuto(const std::string& strPath);
