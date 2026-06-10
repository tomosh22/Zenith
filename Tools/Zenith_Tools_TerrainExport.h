#pragma once

#include <string>

#include "AssetHandling/Zenith_Image.h"

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
 * Export terrain meshes using default hardcoded paths.
 * For backward compatibility with the debug menu button.
 * Uses: GAME_ASSETS_DIR/Textures/Heightmaps/Test/gaeaHeight.ztxtr
 *       GAME_ASSETS_DIR/Terrain/
 */
void ExportHeightmap();
