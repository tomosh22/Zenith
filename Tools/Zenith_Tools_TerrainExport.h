#pragma once

#include <string>

/**
 * Export terrain meshes from heightmap and material textures.
 * 
 * This function generates all LOD levels (LOD0-LOD3) plus physics meshes for
 * the entire terrain grid. The resulting files follow the naming convention
 * required by Flux_TerrainStreamingManager:
 *   - Render_X_Y ZENITH_MESH_EXT      (LOD0, highest detail)
 *   - Render_LOD1_X_Y ZENITH_MESH_EXT (LOD1)
 *   - Render_LOD2_X_Y ZENITH_MESH_EXT (LOD2)
 *   - Render_LOD3_X_Y ZENITH_MESH_EXT (LOD3, lowest detail, always resident)
 *   - Physics_X_Y ZENITH_MESH_EXT    (Physics collision mesh)
 * 
 * where X and Y are chunk coordinates in the 64x64 grid (0-63).
 * 
 * @param strHeightmapPath    Full path to the heightmap texture (.tif format, 16-bit grayscale)
 * @param strMaterialPath     Full path to the material interpolation texture (.tif format, 16-bit grayscale)
 * @param strOutputDir        Full path to output directory (must end with '/' or '\')
 */
void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strMaterialPath, const std::string& strOutputDir);

/**
 * Export terrain meshes using default hardcoded paths.
 * For backward compatibility with the debug menu button.
 * Uses: GAME_ASSETS_DIR/Textures/Heightmaps/Test/gaeaHeight.tif
 *       GAME_ASSETS_DIR/Textures/Heightmaps/Test/gaeaMaterial.tif
 *       GAME_ASSETS_DIR/Terrain/
 */
void ExportHeightmap();
