#include "Zenith.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "Zenith_Tools_GltfExport.h"
#include "Zenith_Tools_TestAssetExport.h"
#include <filesystem>

//------------------------------------------------------------------------------
// Bone indices for stick figure skeleton
//------------------------------------------------------------------------------
static constexpr uint32_t STICK_BONE_ROOT = 0;
static constexpr uint32_t STICK_BONE_SPINE = 1;
static constexpr uint32_t STICK_BONE_NECK = 2;
static constexpr uint32_t STICK_BONE_HEAD = 3;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_ARM = 4;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_ARM = 5;
static constexpr uint32_t STICK_BONE_LEFT_HAND = 6;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_ARM = 7;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_ARM = 8;
static constexpr uint32_t STICK_BONE_RIGHT_HAND = 9;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_LEG = 10;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_LEG = 11;
static constexpr uint32_t STICK_BONE_LEFT_FOOT = 12;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_LEG = 13;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_LEG = 14;
static constexpr uint32_t STICK_BONE_RIGHT_FOOT = 15;
static constexpr uint32_t STICK_BONE_COUNT = 16;

//------------------------------------------------------------------------------
// Cube geometry constants
//------------------------------------------------------------------------------
static const Zenith_Maths::Vector3 s_axCubeOffsets[8] = {
	{-0.05f, -0.05f, -0.05f}, // 0: left-bottom-back
	{ 0.05f, -0.05f, -0.05f}, // 1: right-bottom-back
	{ 0.05f,  0.05f, -0.05f}, // 2: right-top-back
	{-0.05f,  0.05f, -0.05f}, // 3: left-top-back
	{-0.05f, -0.05f,  0.05f}, // 4: left-bottom-front
	{ 0.05f, -0.05f,  0.05f}, // 5: right-bottom-front
	{ 0.05f,  0.05f,  0.05f}, // 6: right-top-front
	{-0.05f,  0.05f,  0.05f}, // 7: left-top-front
};

static const uint32_t s_auCubeIndices[36] = {
	// Back face
	0, 2, 1, 0, 3, 2,
	// Front face
	4, 5, 6, 4, 6, 7,
	// Left face
	0, 4, 7, 0, 7, 3,
	// Right face
	1, 2, 6, 1, 6, 5,
	// Bottom face
	0, 1, 5, 0, 5, 4,
	// Top face
	3, 7, 6, 3, 6, 2,
};

//------------------------------------------------------------------------------
// Per-bone scale factors for humanoid proportions (half-extents in X, Y, Z)
//------------------------------------------------------------------------------
static const Zenith_Maths::Vector3 s_axBoneScales[STICK_BONE_COUNT] = {
	{0.10f, 0.06f, 0.06f},  // 0: Root (pelvis)
	{0.18f, 0.65f, 0.10f},  // 1: Spine (torso)
	{0.05f, 0.10f, 0.05f},  // 2: Neck
	{0.12f, 0.12f, 0.10f},  // 3: Head
	{0.05f, 0.20f, 0.05f},  // 4: LeftUpperArm
	{0.04f, 0.15f, 0.04f},  // 5: LeftLowerArm (Y matches bone-to-Hand 0.30 to avoid hand overlap)
	{0.04f, 0.06f, 0.02f},  // 6: LeftHand
	{0.05f, 0.20f, 0.05f},  // 7: RightUpperArm
	{0.04f, 0.15f, 0.04f},  // 8: RightLowerArm
	{0.04f, 0.06f, 0.02f},  // 9: RightHand
	{0.07f, 0.25f, 0.07f},  // 10: LeftUpperLeg
	{0.05f, 0.25f, 0.05f},  // 11: LeftLowerLeg
	{0.05f, 0.03f, 0.10f},  // 12: LeftFoot
	{0.07f, 0.25f, 0.07f},  // 13: RightUpperLeg
	{0.05f, 0.25f, 0.05f},  // 14: RightLowerLeg
	{0.05f, 0.03f, 0.10f},  // 15: RightFoot
};

// Per-bone cube center offsets (bone-local). For limb bones, shift the cube
// along the bone's local Y so the cube spans from the bone's pivot to its
// child's pivot — when the bone rotates around its pivot, the top face stays
// planted at the joint and the cube stays connected to its parent's cube.
// Junction bones (Root, Spine, Neck, Head) are NOT shifted: their cubes are
// sized as volumes (torso, head) rather than single segments, and shifting
// them would push the cube past adjacent body parts (e.g. Spine into Head).
static const Zenith_Maths::Vector3 s_axBoneCenterOffsets[STICK_BONE_COUNT] = {
	{ 0.0f,  0.0f,  0.0f},  // 0: Root (junction)
	{ 0.0f,  0.0f,  0.0f},  // 1: Spine (torso volume — shift would intersect head)
	{ 0.0f,  0.0f,  0.0f},  // 2: Neck (junction)
	{ 0.0f,  0.0f,  0.0f},  // 3: Head (terminal)
	{ 0.0f, -0.20f, 0.0f},  // 4: LeftUpperArm — child LowerArm at -Y 0.4
	{ 0.0f, -0.15f, 0.0f},  // 5: LeftLowerArm — child Hand at -Y 0.3
	{ 0.0f,  0.0f,  0.0f},  // 6: LeftHand (terminal)
	{ 0.0f, -0.20f, 0.0f},  // 7: RightUpperArm
	{ 0.0f, -0.15f, 0.0f},  // 8: RightLowerArm
	{ 0.0f,  0.0f,  0.0f},  // 9: RightHand (terminal)
	{ 0.0f, -0.25f, 0.0f},  // 10: LeftUpperLeg — child LowerLeg at -Y 0.5
	{ 0.0f, -0.25f, 0.0f},  // 11: LeftLowerLeg — child Foot at -Y 0.5
	{ 0.0f,  0.0f,  0.0f},  // 12: LeftFoot (terminal)
	{ 0.0f, -0.25f, 0.0f},  // 13: RightUpperLeg
	{ 0.0f, -0.25f, 0.0f},  // 14: RightLowerLeg
	{ 0.0f,  0.0f,  0.0f},  // 15: RightFoot (terminal)
};

//------------------------------------------------------------------------------
// StickFigure Helper Functions
//------------------------------------------------------------------------------
// (The ProceduralTree generation moved to Zenith_Tools_TreeAssetExport.cpp —
// the high-quality branching tree with bark/leaf textures and sway VATs.)

static Zenith_SkeletonAsset* CreateStickFigureSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root (at origin)
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);

	// Spine (up from root)
	pxSkel->AddBone("Spine", STICK_BONE_ROOT, Zenith_Maths::Vector3(0, 0.5f, 0), xIdentity, xUnitScale);

	// Neck (up from spine)
	pxSkel->AddBone("Neck", STICK_BONE_SPINE, Zenith_Maths::Vector3(0, 0.7f, 0), xIdentity, xUnitScale);

	// Head (up from neck)
	pxSkel->AddBone("Head", STICK_BONE_NECK, Zenith_Maths::Vector3(0, 0.2f, 0), xIdentity, xUnitScale);

	// Left arm chain
	pxSkel->AddBone("LeftUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(-0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerArm", STICK_BONE_LEFT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftHand", STICK_BONE_LEFT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Right arm chain
	pxSkel->AddBone("RightUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerArm", STICK_BONE_RIGHT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightHand", STICK_BONE_RIGHT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Left leg chain
	pxSkel->AddBone("LeftUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(-0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerLeg", STICK_BONE_LEFT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftFoot", STICK_BONE_LEFT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	// Right leg chain
	pxSkel->AddBone("RightUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerLeg", STICK_BONE_RIGHT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightFoot", STICK_BONE_RIGHT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

static Zenith_MeshAsset* CreateStickFigureMesh(const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	const uint32_t uVertsPerBone = 8;
	const uint32_t uIndicesPerBone = 36;
	pxMesh->Reserve(STICK_BONE_COUNT * uVertsPerBone, STICK_BONE_COUNT * uIndicesPerBone);

	// Add a scaled cube at each bone position
	for (uint32_t uBone = 0; uBone < STICK_BONE_COUNT; uBone++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		// Get world position from bind pose model matrix
		Zenith_Maths::Vector3 xBoneWorldPos = Zenith_Maths::Vector3(xBone.m_xBindPoseModel[3]);

		// Get per-bone scale
		Zenith_Maths::Vector3 xScale = s_axBoneScales[uBone];

		// Bone-local cube center shift so the top face sits on the bone pivot
		// and the bottom face sits on the child's pivot — joints stay connected
		// when bones rotate. Bind pose has identity rotations, so bone-local axes
		// equal world axes here and we can add the offset directly.
		Zenith_Maths::Vector3 xCenterOffset = s_axBoneCenterOffsets[uBone];

		uint32_t uBaseVertex = pxMesh->GetNumVerts();

		// Add 8 cube vertices with per-bone scaling
		for (int i = 0; i < 8; i++)
		{
			// Scale the cube offsets by the bone's scale factors
			Zenith_Maths::Vector3 xScaledOffset = s_axCubeOffsets[i] * 2.0f;
			xScaledOffset.x *= xScale.x * 10.0f;
			xScaledOffset.y *= xScale.y * 10.0f;
			xScaledOffset.z *= xScale.z * 10.0f;

			Zenith_Maths::Vector3 xPos = xBoneWorldPos + xCenterOffset + xScaledOffset;

			// Calculate proper face normal based on vertex position
			Zenith_Maths::Vector3 xNormal = glm::normalize(s_axCubeOffsets[i]);

			pxMesh->AddVertex(xPos, xNormal, Zenith_Maths::Vector2(0, 0));
			pxMesh->SetVertexSkinning(
				uBaseVertex + i,
				glm::uvec4(uBone, 0, 0, 0),
				glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}

		// Add 12 triangles (36 indices)
		for (int i = 0; i < 36; i += 3)
		{
			pxMesh->AddTriangle(
				uBaseVertex + s_auCubeIndices[i],
				uBaseVertex + s_auCubeIndices[i + 1],
				uBaseVertex + s_auCubeIndices[i + 2]);
		}
	}

	pxMesh->AddSubmesh(0, STICK_BONE_COUNT * uIndicesPerBone, 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

// Non-static: shared with Zenith_Tools_TreeAssetExport.cpp (declared in the header).
Flux_MeshGeometry* Zenith_Tools_CreateFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset, const Zenith_SkeletonAsset* pxSkeleton)
{
	Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();

	const uint32_t uNumVerts = pxMeshAsset->GetNumVerts();
	const uint32_t uNumIndices = pxMeshAsset->GetNumIndices();
	const uint32_t uNumBones = pxSkeleton->GetNumBones();

	pxGeometry->m_uNumVerts = uNumVerts;
	pxGeometry->m_uNumIndices = uNumIndices;
	pxGeometry->m_uNumBones = uNumBones;
	pxGeometry->m_xMaterialColor = pxMeshAsset->m_xMaterialColor;

	// Copy positions
	pxGeometry->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxGeometry->m_pxPositions[i] = pxMeshAsset->m_xPositions.Get(i);
	}

	// Copy normals
	if (pxMeshAsset->m_xNormals.GetSize() > 0)
	{
		pxGeometry->m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxNormals[i] = pxMeshAsset->m_xNormals.Get(i);
		}
	}

	// Copy UVs
	if (pxMeshAsset->m_xUVs.GetSize() > 0)
	{
		pxGeometry->m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxUVs[i] = pxMeshAsset->m_xUVs.Get(i);
		}
	}

	// Copy tangents
	if (pxMeshAsset->m_xTangents.GetSize() > 0)
	{
		pxGeometry->m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxTangents[i] = pxMeshAsset->m_xTangents.Get(i);
		}
	}

	// Copy colors
	if (pxMeshAsset->m_xColors.GetSize() > 0)
	{
		pxGeometry->m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxColors[i] = pxMeshAsset->m_xColors.Get(i);
		}
	}

	// Copy indices
	pxGeometry->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	for (uint32_t i = 0; i < uNumIndices; i++)
	{
		pxGeometry->m_puIndices[i] = pxMeshAsset->m_xIndices.Get(i);
	}

	// Copy bone IDs (flatten uvec4 to uint32_t array)
	if (pxMeshAsset->m_xBoneIndices.GetSize() > 0)
	{
		pxGeometry->m_puBoneIDs = static_cast<uint32_t*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(uint32_t)));
		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const glm::uvec4& xIndices = pxMeshAsset->m_xBoneIndices.Get(v);
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 0] = xIndices.x;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 1] = xIndices.y;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 2] = xIndices.z;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 3] = xIndices.w;
		}
	}

	// Copy bone weights (flatten vec4 to float array)
	if (pxMeshAsset->m_xBoneWeights.GetSize() > 0)
	{
		pxGeometry->m_pfBoneWeights = static_cast<float*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(float)));
		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const glm::vec4& xWeights = pxMeshAsset->m_xBoneWeights.Get(v);
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 0] = xWeights.x;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 1] = xWeights.y;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 2] = xWeights.z;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 3] = xWeights.w;
		}
	}

	// Build bone name to ID and offset matrix map from skeleton
	for (uint32_t b = 0; b < uNumBones; b++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(b);
		Zenith_Maths::Matrix4 xOffsetMat = glm::inverse(xBone.m_xBindPoseModel);
		pxGeometry->m_xBoneNameToIdAndOffset[xBone.m_strName] = std::make_pair(b, xOffsetMat);
	}

	// Generate buffer layout and interleaved vertex data
	pxGeometry->GenerateLayoutAndVertexData();

	return pxGeometry;
}

// Non-static: shared with Zenith_Tools_TreeAssetExport.cpp (declared in the header).
Flux_MeshGeometry* Zenith_Tools_CreateStaticFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset)
{
	Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();

	const uint32_t uNumVerts = pxMeshAsset->GetNumVerts();
	const uint32_t uNumIndices = pxMeshAsset->GetNumIndices();

	pxGeometry->m_uNumVerts = uNumVerts;
	pxGeometry->m_uNumIndices = uNumIndices;
	pxGeometry->m_uNumBones = 0;  // No bones for static mesh
	pxGeometry->m_xMaterialColor = pxMeshAsset->m_xMaterialColor;

	// Copy positions
	pxGeometry->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxGeometry->m_pxPositions[i] = pxMeshAsset->m_xPositions.Get(i);
	}

	// Copy normals (or generate default up vector)
	pxGeometry->m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xNormals.GetSize() > 0)
			pxGeometry->m_pxNormals[i] = pxMeshAsset->m_xNormals.Get(i);
		else
			pxGeometry->m_pxNormals[i] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}

	// Copy UVs (or generate default zero)
	pxGeometry->m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xUVs.GetSize() > 0)
			pxGeometry->m_pxUVs[i] = pxMeshAsset->m_xUVs.Get(i);
		else
			pxGeometry->m_pxUVs[i] = Zenith_Maths::Vector2(0.0f, 0.0f);
	}

	// Copy tangents (or generate default)
	pxGeometry->m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xTangents.GetSize() > 0)
			pxGeometry->m_pxTangents[i] = pxMeshAsset->m_xTangents.Get(i);
		else
			pxGeometry->m_pxTangents[i] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	}

	// Copy bitangents (or generate default)
	pxGeometry->m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xBitangents.GetSize() > 0)
			pxGeometry->m_pxBitangents[i] = pxMeshAsset->m_xBitangents.Get(i);
		else
			pxGeometry->m_pxBitangents[i] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}

	// Copy colors (or generate default white)
	pxGeometry->m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xColors.GetSize() > 0)
			pxGeometry->m_pxColors[i] = pxMeshAsset->m_xColors.Get(i);
		else
			pxGeometry->m_pxColors[i] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Copy indices
	pxGeometry->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	for (uint32_t i = 0; i < uNumIndices; i++)
	{
		pxGeometry->m_puIndices[i] = pxMeshAsset->m_xIndices.Get(i);
	}

	// NO bone IDs or weights - this is a static mesh

	// Generate buffer layout and interleaved vertex data
	pxGeometry->GenerateLayoutAndVertexData();

	return pxGeometry;
}

//------------------------------------------------------------------------------
// Animation Creation Functions
//------------------------------------------------------------------------------

static Flux_AnimationClip* CreateIdleAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Idle");
	pxClip->SetDuration(2.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Spine breathing motion
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0.5f, 0));
		xChannel.AddPositionKeyframe(24.0f, Zenith_Maths::Vector3(0, 0.52f, 0));
		xChannel.AddPositionKeyframe(48.0f, Zenith_Maths::Vector3(0, 0.5f, 0));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateWalkAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Walk");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Left Upper Leg rotation
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperLeg", std::move(xChannel));
	}

	// Right Upper Leg rotation (opposite phase)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	// Left Upper Arm swing
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right Upper Arm swing
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateRunAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Run");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Left Upper Leg rotation (45 degrees)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperLeg", std::move(xChannel));
	}

	// Right Upper Leg rotation (opposite phase)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	// Left Upper Arm swing (35 degrees)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right Upper Arm swing
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateAttack1Animation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack1");
	pxClip->SetDuration(0.4f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Right arm jab forward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(3.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(60.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	// Slight spine lean forward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(5.0f, glm::angleAxis(glm::radians(15.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateAttack2Animation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack2");
	pxClip->SetDuration(0.4f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xYAxis(0, 1, 0);

	// Left arm swing across body
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(3.0f, glm::angleAxis(glm::radians(-30.0f), xYAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(75.0f), xYAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right arm pull back
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(5.0f, glm::angleAxis(glm::radians(-25.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	// Spine twist left
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(5.0f, glm::angleAxis(glm::radians(-20.0f), xYAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateAttack3Animation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack3");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Both arms raise up then swing down
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f, glm::angleAxis(glm::radians(-120.0f), xXAxis));
		xChannel.AddRotationKeyframe(8.0f, glm::angleAxis(glm::radians(60.0f), xXAxis));
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f, glm::angleAxis(glm::radians(-120.0f), xXAxis));
		xChannel.AddRotationKeyframe(8.0f, glm::angleAxis(glm::radians(60.0f), xXAxis));
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Spine lean back then forward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(8.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Root position - slight hop forward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(6.0f, Zenith_Maths::Vector3(0, 0.1f, 0.15f));
		xChannel.AddPositionKeyframe(12.0f, Zenith_Maths::Vector3(0, 0, 0.1f));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateDodgeAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Dodge");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);

	// Root translation - sidestep right
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(6.0f, Zenith_Maths::Vector3(0.5f, -0.2f, 0));
		xChannel.AddPositionKeyframe(12.0f, Zenith_Maths::Vector3(0.8f, 0, 0));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// Spine lean into dodge
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(30.0f), xZAxis));
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Right leg step out
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-30.0f), xZAxis));
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateHitAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Hit");
	pxClip->SetDuration(0.3f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Root stagger backward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(4.0f, Zenith_Maths::Vector3(0, 0, -0.3f));
		xChannel.AddPositionKeyframe(7.0f, Zenith_Maths::Vector3(0, 0, -0.2f));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// Spine lean backward from impact
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(3.0f, glm::angleAxis(glm::radians(-25.0f), xXAxis));
		xChannel.AddRotationKeyframe(7.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Head snap back
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(2.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(7.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Head", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateDeathAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Death");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);

	// Root drops down and backward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(12.0f, Zenith_Maths::Vector3(0, -0.3f, -0.2f));
		xChannel.AddPositionKeyframe(24.0f, Zenith_Maths::Vector3(0, -1.0f, -0.4f));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// Spine collapses backward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-90.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Head goes limp
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Head", std::move(xChannel));
	}

	// Arms fall limp
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(45.0f), xZAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(60.0f), xZAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xZAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-60.0f), xZAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

//------------------------------------------------------------------------------
// Aim Hold Pose
//
// Shared upper-body rest pose used by the Aim, Fire, and Reload clips. Each
// clip's start/end keyframes match this pose so transitions between them
// don't snap arms back to identity.
//------------------------------------------------------------------------------
namespace StickFigureAimHoldPose
{
	inline Zenith_Maths::Quat RightUpperArm()
	{
		return glm::angleAxis(glm::radians(-75.0f), Zenith_Maths::Vector3(1, 0, 0))
		     * glm::angleAxis(glm::radians( 20.0f), Zenith_Maths::Vector3(0, 1, 0));
	}
	inline Zenith_Maths::Quat RightLowerArm()
	{
		return glm::angleAxis(glm::radians(-45.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
	inline Zenith_Maths::Quat LeftUpperArm()
	{
		return glm::angleAxis(glm::radians(-65.0f), Zenith_Maths::Vector3(1, 0, 0))
		     * glm::angleAxis(glm::radians(-25.0f), Zenith_Maths::Vector3(0, 1, 0));
	}
	inline Zenith_Maths::Quat LeftLowerArm()
	{
		return glm::angleAxis(glm::radians(-50.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
	inline Zenith_Maths::Quat Spine()
	{
		return glm::angleAxis(glm::radians(-10.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
	inline Zenith_Maths::Quat Head()
	{
		return glm::angleAxis(glm::radians(-5.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
}

static Flux_AnimationClip* CreateAimAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Aim");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Stable hold — both keyframes are the aim pose, so looping doesn't pulse.
	auto AddHold = [&](const char* szBone, const Zenith_Maths::Quat& xPose)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, xPose);
		xChannel.AddRotationKeyframe(12.0f, xPose);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};

	AddHold("RightUpperArm", StickFigureAimHoldPose::RightUpperArm());
	AddHold("RightLowerArm", StickFigureAimHoldPose::RightLowerArm());
	AddHold("LeftUpperArm",  StickFigureAimHoldPose::LeftUpperArm());
	AddHold("LeftLowerArm",  StickFigureAimHoldPose::LeftLowerArm());
	AddHold("Spine",         StickFigureAimHoldPose::Spine());
	AddHold("Head",          StickFigureAimHoldPose::Head());

	return pxClip;
}

static Flux_AnimationClip* CreateFireAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Fire");
	pxClip->SetDuration(0.20f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Recoil deltas applied on top of the aim hold pose, so arms stay raised.
	auto AddRecoil = [&](const char* szBone, const Zenith_Maths::Quat& xRest, float fKickDeg)
	{
		const Zenith_Maths::Quat xKick = glm::angleAxis(glm::radians(fKickDeg), xXAxis) * xRest;
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, xRest);
		xChannel.AddRotationKeyframe(2.0f, xKick);
		xChannel.AddRotationKeyframe(5.0f, xRest);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};

	AddRecoil("RightUpperArm", StickFigureAimHoldPose::RightUpperArm(), 15.0f);
	AddRecoil("RightLowerArm", StickFigureAimHoldPose::RightLowerArm(), -10.0f);
	AddRecoil("Spine",         StickFigureAimHoldPose::Spine(),         3.0f);
	AddRecoil("Head",          StickFigureAimHoldPose::Head(),          2.0f);

	return pxClip;
}

static Flux_AnimationClip* CreateReloadAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Reload");
	pxClip->SetDuration(1.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xYAxis(0, 1, 0);

	const Zenith_Maths::Quat xLeftUpperRest  = StickFigureAimHoldPose::LeftUpperArm();
	const Zenith_Maths::Quat xLeftLowerRest  = StickFigureAimHoldPose::LeftLowerArm();
	const Zenith_Maths::Quat xRightUpperRest = StickFigureAimHoldPose::RightUpperArm();

	// Left arm: drop free hand, reach across body for magazine, return.
	{
		Flux_BoneChannel xChannel;
		const Zenith_Maths::Quat xDrop  = glm::angleAxis(glm::radians(-90.0f), xXAxis) * xLeftUpperRest;
		const Zenith_Maths::Quat xReach = glm::angleAxis(glm::radians(-30.0f), xXAxis)
		                                * glm::angleAxis(glm::radians( 60.0f), xYAxis) * xLeftUpperRest;
		const Zenith_Maths::Quat xLift  = glm::angleAxis(glm::radians(-90.0f), xXAxis) * xLeftUpperRest;
		xChannel.AddRotationKeyframe(0.0f,  xLeftUpperRest);
		xChannel.AddRotationKeyframe(8.0f,  xDrop);
		xChannel.AddRotationKeyframe(20.0f, xReach);
		xChannel.AddRotationKeyframe(28.0f, xLift);
		xChannel.AddRotationKeyframe(36.0f, xLeftUpperRest);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	{
		Flux_BoneChannel xChannel;
		const Zenith_Maths::Quat xBend = glm::angleAxis(glm::radians(-100.0f), xXAxis) * xLeftLowerRest;
		const Zenith_Maths::Quat xMid  = glm::angleAxis(glm::radians( -60.0f), xXAxis) * xLeftLowerRest;
		const Zenith_Maths::Quat xRet  = glm::angleAxis(glm::radians( -50.0f), xXAxis) * xLeftLowerRest;
		xChannel.AddRotationKeyframe(0.0f,  xLeftLowerRest);
		xChannel.AddRotationKeyframe(8.0f,  xBend);
		xChannel.AddRotationKeyframe(20.0f, xMid);
		xChannel.AddRotationKeyframe(28.0f, xRet);
		xChannel.AddRotationKeyframe(36.0f, xLeftLowerRest);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftLowerArm", std::move(xChannel));
	}

	// Right arm holds gun-grip pose with a subtle dip in the middle.
	{
		Flux_BoneChannel xChannel;
		const Zenith_Maths::Quat xDip = glm::angleAxis(glm::radians(-15.0f), xXAxis) * xRightUpperRest;
		xChannel.AddRotationKeyframe(0.0f,  xRightUpperRest);
		xChannel.AddRotationKeyframe(8.0f,  xDip);
		xChannel.AddRotationKeyframe(20.0f, xDip);
		xChannel.AddRotationKeyframe(28.0f, xDip);
		xChannel.AddRotationKeyframe(36.0f, xRightUpperRest);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateJumpAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Jump");
	pxClip->SetDuration(0.8f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Spine: crouch, neutral on push-off, slight back lean on land, recover.
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f,  glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f,  glm::angleAxis(glm::radians( 20.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(16.0f, glm::angleAxis(glm::radians(-10.0f), xXAxis));
		xChannel.AddRotationKeyframe(19.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Both upper legs: crouch, push, tuck, recover.
	auto AddUpperLeg = [&](const char* szBone)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f,  glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f,  glm::angleAxis(glm::radians( 30.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(16.0f, glm::angleAxis(glm::radians( 40.0f), xXAxis));
		xChannel.AddRotationKeyframe(19.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};
	AddUpperLeg("LeftUpperLeg");
	AddUpperLeg("RightUpperLeg");

	// Lower legs: knee bend during crouch, big tuck mid-jump.
	auto AddLowerLeg = [&](const char* szBone)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f,  glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f,  glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(16.0f, glm::angleAxis(glm::radians(-90.0f), xXAxis));
		xChannel.AddRotationKeyframe(19.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};
	AddLowerLeg("LeftLowerLeg");
	AddLowerLeg("RightLowerLeg");

	// Arms swing for momentum.
	auto AddArmSwing = [&](const char* szBone)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f,  glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f,  glm::angleAxis(glm::radians( 20.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(16.0f, glm::angleAxis(glm::radians( 30.0f), xXAxis));
		xChannel.AddRotationKeyframe(19.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};
	AddArmSwing("LeftUpperArm");
	AddArmSwing("RightUpperArm");

	return pxClip;
}

//------------------------------------------------------------------------------
// Bullet sphere mesh
//
// Generates a unit sphere into a Zenith_MeshAsset. Used by GenerateRenderTestAssets
// to produce the bullet projectile asset on disk.
//------------------------------------------------------------------------------
static void GenerateUnitSphereMeshAsset(Zenith_MeshAsset& xMeshOut, uint32_t uSegments, uint32_t uRings)
{
	xMeshOut.Reset();

	const uint32_t uVertCount = (uRings + 1) * (uSegments + 1);
	const uint32_t uIdxCount  = uRings * uSegments * 6;
	xMeshOut.Reserve(uVertCount, uIdxCount);

	for (uint32_t uRing = 0; uRing <= uRings; ++uRing)
	{
		const float fV = static_cast<float>(uRing) / static_cast<float>(uRings);
		const float fPhi = fV * glm::pi<float>();
		const float fY = cosf(fPhi);
		const float fSinPhi = sinf(fPhi);

		for (uint32_t uSeg = 0; uSeg <= uSegments; ++uSeg)
		{
			const float fU = static_cast<float>(uSeg) / static_cast<float>(uSegments);
			const float fTheta = fU * glm::pi<float>() * 2.0f;
			const Zenith_Maths::Vector3 xPos(fSinPhi * cosf(fTheta), fY, fSinPhi * sinf(fTheta));
			xMeshOut.AddVertex(xPos * 0.5f, xPos /* normal */, Zenith_Maths::Vector2(fU, fV));
		}
	}

	for (uint32_t uRing = 0; uRing < uRings; ++uRing)
	{
		for (uint32_t uSeg = 0; uSeg < uSegments; ++uSeg)
		{
			const uint32_t uA = uRing * (uSegments + 1) + uSeg;
			const uint32_t uB = uA + (uSegments + 1);
			xMeshOut.AddTriangle(uA, uB, uA + 1);
			xMeshOut.AddTriangle(uA + 1, uB, uB + 1);
		}
	}

	xMeshOut.AddSubmesh(0, uIdxCount, 0);
	xMeshOut.ComputeBounds();
}

//------------------------------------------------------------------------------
// Public Asset Generation Functions
//------------------------------------------------------------------------------

void GenerateStickFigureAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Generating StickFigure test assets...");

	// Create all assets
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);
	Flux_AnimationClip* pxIdleClip = CreateIdleAnimation();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_AnimationClip* pxRunClip = CreateRunAnimation();
	Flux_AnimationClip* pxAttack1Clip = CreateAttack1Animation();
	Flux_AnimationClip* pxAttack2Clip = CreateAttack2Animation();
	Flux_AnimationClip* pxAttack3Clip = CreateAttack3Animation();
	Flux_AnimationClip* pxDodgeClip = CreateDodgeAnimation();
	Flux_AnimationClip* pxHitClip = CreateHitAnimation();
	Flux_AnimationClip* pxDeathClip = CreateDeathAnimation();
	Flux_AnimationClip* pxAimClip = CreateAimAnimation();
	Flux_AnimationClip* pxFireClip = CreateFireAnimation();
	Flux_AnimationClip* pxReloadClip = CreateReloadAnimation();
	Flux_AnimationClip* pxJumpClip = CreateJumpAnimation();

	// Create output directory
	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
	std::filesystem::create_directories(strOutputDir);

	// Export skeleton
	std::string strSkelPath = strOutputDir + "StickFigure" ZENITH_SKELETON_EXT;
	pxSkel->Export(strSkelPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported skeleton to: %s", strSkelPath.c_str());

	// Set skeleton path on mesh before export
	pxMesh->SetSkeletonPath("Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT);

	// Export mesh in Zenith_MeshAsset format
	std::string strMeshAssetPath = strOutputDir + "StickFigure" ZENITH_MESH_ASSET_EXT;
	pxMesh->Export(strMeshAssetPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh asset to: %s", strMeshAssetPath.c_str());

#ifdef ZENITH_TOOLS
	// Export mesh in Flux_MeshGeometry format
	Flux_MeshGeometry* pxFluxGeometry = Zenith_Tools_CreateFluxMeshGeometry(pxMesh, pxSkel);
	std::string strMeshPath = strOutputDir + "StickFigure" ZENITH_MESH_EXT;
	pxFluxGeometry->Export(strMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh geometry to: %s", strMeshPath.c_str());
	delete pxFluxGeometry;

	// Export static mesh
	Flux_MeshGeometry* pxStaticGeometry = Zenith_Tools_CreateStaticFluxMeshGeometry(pxMesh);
	std::string strStaticMeshPath = strOutputDir + "StickFigure_Static" ZENITH_MESH_EXT;
	pxStaticGeometry->Export(strStaticMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported static mesh geometry to: %s", strStaticMeshPath.c_str());
	delete pxStaticGeometry;
#endif

	// Export animations
	std::string strIdlePath = strOutputDir + "StickFigure_Idle" ZENITH_ANIMATION_EXT;
	pxIdleClip->Export(strIdlePath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported idle animation to: %s", strIdlePath.c_str());

	std::string strWalkPath = strOutputDir + "StickFigure_Walk" ZENITH_ANIMATION_EXT;
	pxWalkClip->Export(strWalkPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported walk animation to: %s", strWalkPath.c_str());

	std::string strRunPath = strOutputDir + "StickFigure_Run" ZENITH_ANIMATION_EXT;
	pxRunClip->Export(strRunPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported run animation to: %s", strRunPath.c_str());

	std::string strAttack1Path = strOutputDir + "StickFigure_Attack1" ZENITH_ANIMATION_EXT;
	pxAttack1Clip->Export(strAttack1Path);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported attack1 animation to: %s", strAttack1Path.c_str());

	std::string strAttack2Path = strOutputDir + "StickFigure_Attack2" ZENITH_ANIMATION_EXT;
	pxAttack2Clip->Export(strAttack2Path);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported attack2 animation to: %s", strAttack2Path.c_str());

	std::string strAttack3Path = strOutputDir + "StickFigure_Attack3" ZENITH_ANIMATION_EXT;
	pxAttack3Clip->Export(strAttack3Path);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported attack3 animation to: %s", strAttack3Path.c_str());

	std::string strDodgePath = strOutputDir + "StickFigure_Dodge" ZENITH_ANIMATION_EXT;
	pxDodgeClip->Export(strDodgePath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported dodge animation to: %s", strDodgePath.c_str());

	std::string strHitPath = strOutputDir + "StickFigure_Hit" ZENITH_ANIMATION_EXT;
	pxHitClip->Export(strHitPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported hit animation to: %s", strHitPath.c_str());

	std::string strDeathPath = strOutputDir + "StickFigure_Death" ZENITH_ANIMATION_EXT;
	pxDeathClip->Export(strDeathPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported death animation to: %s", strDeathPath.c_str());

	std::string strAimPath = strOutputDir + "StickFigure_Aim" ZENITH_ANIMATION_EXT;
	pxAimClip->Export(strAimPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported aim animation to: %s", strAimPath.c_str());

	std::string strFirePath = strOutputDir + "StickFigure_Fire" ZENITH_ANIMATION_EXT;
	pxFireClip->Export(strFirePath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported fire animation to: %s", strFirePath.c_str());

	std::string strReloadPath = strOutputDir + "StickFigure_Reload" ZENITH_ANIMATION_EXT;
	pxReloadClip->Export(strReloadPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported reload animation to: %s", strReloadPath.c_str());

	std::string strJumpPath = strOutputDir + "StickFigure_Jump" ZENITH_ANIMATION_EXT;
	pxJumpClip->Export(strJumpPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported jump animation to: %s", strJumpPath.c_str());

	// Export to glTF format for editing in Blender
	{
		std::vector<const Flux_AnimationClip*> axClips = {
			pxIdleClip, pxWalkClip, pxRunClip, pxAttack1Clip, pxAttack2Clip,
			pxAttack3Clip, pxDodgeClip, pxHitClip, pxDeathClip,
			pxAimClip, pxFireClip, pxReloadClip, pxJumpClip
		};
		std::string strGltfPath = strOutputDir + "StickFigure.gltf";
		if (Zenith_Tools_GltfExport::ExportToGltf(strGltfPath.c_str(), pxMesh, pxSkel, axClips))
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  Exported glTF to: %s", strGltfPath.c_str());
		}
	}

	// Cleanup
	delete pxJumpClip;
	delete pxReloadClip;
	delete pxFireClip;
	delete pxAimClip;
	delete pxDeathClip;
	delete pxHitClip;
	delete pxDodgeClip;
	delete pxAttack3Clip;
	delete pxAttack2Clip;
	delete pxAttack1Clip;
	delete pxRunClip;
	delete pxWalkClip;
	delete pxIdleClip;
	delete pxMesh;
	delete pxSkel;

	Zenith_Log(LOG_CATEGORY_ASSET, "StickFigure assets generated at: %s", strOutputDir.c_str());
}

void GenerateRenderTestAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Generating RenderTest game assets...");

	// Shared engine asset (any game can reference it). Mirrors how StickFigure
	// and ProceduralTree live under ENGINE_ASSETS_DIR/Meshes/. GAME_ASSETS_DIR
	// is per-game and isn't defined in the engine-library translation unit.
	const std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/Bullet_Sphere/";
	std::filesystem::create_directories(strOutputDir);

	const std::string strMeshAssetPath = strOutputDir + "Bullet_Sphere" ZENITH_MESH_ASSET_EXT;
	const std::string strModelPath     = strOutputDir + "Bullet_Sphere" ZENITH_MODEL_EXT;

	if (!std::filesystem::exists(strMeshAssetPath))
	{
		Zenith_MeshAsset xSphere;
		GenerateUnitSphereMeshAsset(xSphere, /*uSegments=*/16, /*uRings=*/8);
		xSphere.Export(strMeshAssetPath.c_str());
		Zenith_Log(LOG_CATEGORY_ASSET, "  Exported bullet sphere mesh to: %s", strMeshAssetPath.c_str());
	}

	if (!std::filesystem::exists(strModelPath))
	{
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		pxModel->SetName("BulletSphere");
		Zenith_Vector<std::string> xEmptyMaterials;
		pxModel->AddMeshByPath(strMeshAssetPath, xEmptyMaterials);
		pxModel->Export(strModelPath.c_str());
		Zenith_Log(LOG_CATEGORY_ASSET, "  Exported bullet sphere model to: %s", strModelPath.c_str());
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "RenderTest assets generated at: %s", strOutputDir.c_str());
}

void GenerateTestAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "=== Generating Test Assets ===");
	GenerateStickFigureAssets();
	GenerateProceduralTreeAssets();
	GenerateRenderTestAssets();
	Zenith_Log(LOG_CATEGORY_ASSET, "=== Test Asset Generation Complete ===");
}

#include "Zenith_Tools_TestAssetExport.Tests.inl"
