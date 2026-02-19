#pragma once

#include <string>

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include <opencv2/core/mat.hpp>
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"

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
 * @param strHeightmapPath    Full path to the heightmap texture (.ztxtr or .tif format)
 * @param strOutputDir        Full path to output directory (must end with '/' or '\')
 */
void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);

/**
 * Export terrain meshes from an already-loaded heightmap cv::Mat.
 * Avoids re-loading from disk when the heightmap is already in memory.
 *
 * @param xHeightmap    cv::Mat heightmap (CV_32FC1 format)
 * @param strOutputDir  Full path to output directory (must end with '/' or '\')
 */
void ExportHeightmapFromMat(const cv::Mat& xHeightmap, const std::string& strOutputDir);

/**
 * Export terrain meshes using default hardcoded paths.
 * For backward compatibility with the debug menu button.
 * Uses: GAME_ASSETS_DIR/Textures/Heightmaps/Test/gaeaHeight.tif
 *       GAME_ASSETS_DIR/Textures/Heightmaps/Test/gaeaMaterial.tif
 *       GAME_ASSETS_DIR/Terrain/
 */
void ExportHeightmap();
