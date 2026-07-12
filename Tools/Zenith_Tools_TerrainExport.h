#pragma once

#include <string>

#include "AssetHandling/Zenith_Image.h"
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
 * where X and Y are chunk coordinates in the 64x64 grid (0-63).
 *
 * @param strHeightmapPath    Full path to the heightmap texture (.ztxtr or 16-bit PNG)
 * @param strOutputDir        Full path to output directory (must end with '/' or '\')
 */
void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);

/**
 * Export terrain meshes from an already-loaded heightmap image.
 * Avoids re-loading from disk when the heightmap is already in memory.
 *
 * @param xHeightmap    Single-channel float heightmap
 * @param strOutputDir  Full path to output directory (must end with '/' or '\')
 */
void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir);

/**
 * Export HIGH, LOW and physics meshes for only the chunks inside xRect.
 * Chunk filenames retain their absolute coordinates in the fixed 64x64 grid.
 * Returns false without exporting when the image or rectangle is invalid.
 */
bool ExportHeightmapFromMatRect(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Flux_TerrainExportRect& xRect);

/**
 * Export terrain meshes using default hardcoded paths.
 * For backward compatibility with the debug menu button.
 * Uses: GAME_ASSETS_DIR/Textures/Heightmaps/Test/gaeaHeight.ztxtr
 *       GAME_ASSETS_DIR/Terrain/
 */
void ExportHeightmap();

/**
 * Load a heightmap from .ztxtr or a common image format (PNG etc.) into a
 * single-channel float image normalized to [0,1]. Returns an empty image on
 * failure. Exposed for the terrain editor, which seeds its live heightfield
 * through the same loader the export pipeline uses.
 */
Zenith_Image Zenith_Tools_LoadHeightmapAuto(const std::string& strPath);
