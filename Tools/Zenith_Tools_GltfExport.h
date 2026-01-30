#pragma once

#include <vector>
#include <string>

class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Flux_AnimationClip;

//=============================================================================
// Zenith_Tools_GltfExport
// Export Zenith assets to glTF 2.0 format using Assimp's exporter
//=============================================================================
namespace Zenith_Tools_GltfExport
{
	/**
	 * Export a skinned mesh with skeleton and animations to glTF format.
	 *
	 * @param szOutputPath  Output file path (.gltf for JSON, .glb for binary)
	 * @param pxMesh        Mesh asset to export
	 * @param pxSkeleton    Skeleton asset (can be nullptr for static meshes)
	 * @param axAnimations  Animation clips to include (can be empty)
	 * @return true on success, false on failure
	 */
	bool ExportToGltf(
		const char* szOutputPath,
		const Zenith_MeshAsset* pxMesh,
		const Zenith_SkeletonAsset* pxSkeleton,
		const std::vector<const Flux_AnimationClip*>& axAnimations
	);

	/**
	 * Export a static mesh (no skeleton or animations) to glTF format.
	 *
	 * @param szOutputPath  Output file path (.gltf for JSON, .glb for binary)
	 * @param pxMesh        Mesh asset to export
	 * @return true on success, false on failure
	 */
	bool ExportStaticMeshToGltf(
		const char* szOutputPath,
		const Zenith_MeshAsset* pxMesh
	);

} // namespace Zenith_Tools_GltfExport
