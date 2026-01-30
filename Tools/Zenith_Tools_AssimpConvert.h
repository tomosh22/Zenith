#pragma once

//=============================================================================
// IMPORTANT: This header requires Assimp headers to be included BEFORE it.
// Include assimp headers wrapped in memory management disable/enable:
//
//     #include "Memory/Zenith_MemoryManagement_Disabled.h"
//     #include <assimp/scene.h>
//     #include <assimp/anim.h>
//     #include "Memory/Zenith_MemoryManagement_Enabled.h"
//     #include "Zenith_Tools_AssimpConvert.h"
//=============================================================================

#include "Maths/Zenith_Maths.h"
#include <unordered_map>
#include <string>
#include <vector>

// Forward declarations - Zenith types
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Flux_AnimationClip;
class Flux_BoneChannel;

//=============================================================================
// Zenith_AssimpConvert
// Bidirectional conversion utilities between Zenith and Assimp data types.
// Naming convention: AssimpToZenith / ZenithToAssimp
//=============================================================================
namespace Zenith_AssimpConvert
{
	//=========================================================================
	// Matrix Conversions
	//=========================================================================
	Zenith_Maths::Matrix4 AssimpToZenith(const aiMatrix4x4& xMat);
	aiMatrix4x4           ZenithToAssimp(const Zenith_Maths::Matrix4& xMat);

	//=========================================================================
	// Vector Conversions
	//=========================================================================
	Zenith_Maths::Vector3 AssimpToZenith(const aiVector3D& xVec);
	aiVector3D            ZenithToAssimp(const Zenith_Maths::Vector3& xVec);

	Zenith_Maths::Vector2 AssimpToZenith2D(const aiVector3D& xVec);
	aiVector3D            ZenithToAssimp2D(const Zenith_Maths::Vector2& xVec);

	//=========================================================================
	// Color Conversions
	//=========================================================================
	Zenith_Maths::Vector4 AssimpToZenith(const aiColor4D& xColor);
	aiColor4D             ZenithToAssimp(const Zenith_Maths::Vector4& xColor);

	//=========================================================================
	// Quaternion Conversions
	//=========================================================================
	Zenith_Maths::Quat AssimpToZenith(const aiQuaternion& xQuat);
	aiQuaternion       ZenithToAssimp(const Zenith_Maths::Quat& xQuat);

	//=========================================================================
	// Mesh Conversions
	//=========================================================================

	// Export: Zenith_MeshAsset -> aiMesh
	// Creates a new aiMesh from Zenith mesh data.
	// Caller takes ownership of returned pointer.
	aiMesh* ZenithToAssimp(
		const Zenith_MeshAsset* pxMesh,
		const Zenith_SkeletonAsset* pxSkeleton = nullptr);

	//=========================================================================
	// Skeleton/Node Conversions
	//=========================================================================

	// Export: Zenith_SkeletonAsset -> aiNode hierarchy
	// Creates a node tree with proper parent-child relationships.
	// Returns root node. Caller takes ownership of entire tree.
	aiNode* ZenithToAssimp(const Zenith_SkeletonAsset* pxSkeleton);

	//=========================================================================
	// Animation Conversions
	//=========================================================================

	// Export: Flux_AnimationClip -> aiAnimation
	// Creates a new aiAnimation from Zenith animation data.
	// Caller takes ownership of returned pointer.
	aiAnimation* ZenithToAssimp(const Flux_AnimationClip* pxClip);

	//=========================================================================
	// Bone Channel Conversions
	//=========================================================================

	// Export: Flux_BoneChannel -> aiNodeAnim
	// Creates a new aiNodeAnim from Zenith bone channel data.
	// Caller takes ownership of returned pointer.
	aiNodeAnim* ZenithToAssimp(const Flux_BoneChannel& xChannel, const std::string& strBoneName);

	//=========================================================================
	// Helper: Calculate world transform for a node
	//=========================================================================
	Zenith_Maths::Matrix4 CalculateNodeWorldTransform(aiNode* pxNode);

} // namespace Zenith_AssimpConvert
