#include "Zenith.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
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
	{0.04f, 0.18f, 0.04f},  // 5: LeftLowerArm
	{0.04f, 0.06f, 0.02f},  // 6: LeftHand
	{0.05f, 0.20f, 0.05f},  // 7: RightUpperArm
	{0.04f, 0.18f, 0.04f},  // 8: RightLowerArm
	{0.04f, 0.06f, 0.02f},  // 9: RightHand
	{0.07f, 0.25f, 0.07f},  // 10: LeftUpperLeg
	{0.05f, 0.25f, 0.05f},  // 11: LeftLowerLeg
	{0.05f, 0.03f, 0.10f},  // 12: LeftFoot
	{0.07f, 0.25f, 0.07f},  // 13: RightUpperLeg
	{0.05f, 0.25f, 0.05f},  // 14: RightLowerLeg
	{0.05f, 0.03f, 0.10f},  // 15: RightFoot
};

//------------------------------------------------------------------------------
// Tree bone indices
//------------------------------------------------------------------------------
static constexpr uint32_t TREE_BONE_COUNT = 9;
enum TreeBone
{
	TREE_BONE_ROOT = 0,          // Ground anchor
	TREE_BONE_TRUNK_LOWER = 1,   // Lower trunk
	TREE_BONE_TRUNK_UPPER = 2,   // Upper trunk
	TREE_BONE_BRANCH_0 = 3,      // Branch at trunk lower
	TREE_BONE_BRANCH_1 = 4,      // Branch at trunk upper (left)
	TREE_BONE_BRANCH_2 = 5,      // Branch at trunk upper (right)
	TREE_BONE_BRANCH_3 = 6,      // Branch at trunk top
	TREE_BONE_LEAVES_0 = 7,      // Leaf cluster at branch 3
	TREE_BONE_LEAVES_1 = 8,      // Leaf cluster at branch 1
};

// Tree bone scales (half-extents for box geometry)
static const Zenith_Maths::Vector3 s_axTreeBoneScales[TREE_BONE_COUNT] = {
	{0.05f, 0.05f, 0.05f},   // 0: Root (small anchor point)
	{0.15f, 1.0f, 0.15f},    // 1: TrunkLower (thick lower trunk)
	{0.12f, 1.0f, 0.12f},    // 2: TrunkUpper (slightly thinner upper trunk)
	{0.06f, 0.6f, 0.06f},    // 3: Branch0 (branch from lower trunk)
	{0.05f, 0.7f, 0.05f},    // 4: Branch1 (branch from upper trunk, left)
	{0.05f, 0.7f, 0.05f},    // 5: Branch2 (branch from upper trunk, right)
	{0.04f, 0.5f, 0.04f},    // 6: Branch3 (top branch)
	{0.4f, 0.3f, 0.4f},      // 7: Leaves0 (leaf cluster at branch 3)
	{0.35f, 0.25f, 0.35f},   // 8: Leaves1 (leaf cluster at branch 1)
};

//------------------------------------------------------------------------------
// StickFigure Helper Functions
//------------------------------------------------------------------------------

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

		uint32_t uBaseVertex = pxMesh->GetNumVerts();

		// Add 8 cube vertices with per-bone scaling
		for (int i = 0; i < 8; i++)
		{
			// Scale the cube offsets by the bone's scale factors
			Zenith_Maths::Vector3 xScaledOffset = s_axCubeOffsets[i] * 2.0f;
			xScaledOffset.x *= xScale.x * 10.0f;
			xScaledOffset.y *= xScale.y * 10.0f;
			xScaledOffset.z *= xScale.z * 10.0f;

			Zenith_Maths::Vector3 xPos = xBoneWorldPos + xScaledOffset;

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

static Flux_MeshGeometry* CreateFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset, const Zenith_SkeletonAsset* pxSkeleton)
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

static Flux_MeshGeometry* CreateStaticFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset)
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
// Tree Helper Functions
//------------------------------------------------------------------------------

static Zenith_SkeletonAsset* CreateTreeSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root at ground level
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);

	// Trunk segments (vertical along Y axis)
	pxSkel->AddBone("TrunkLower", TREE_BONE_ROOT, Zenith_Maths::Vector3(0, 1.0f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("TrunkUpper", TREE_BONE_TRUNK_LOWER, Zenith_Maths::Vector3(0, 2.0f, 0), xIdentity, xUnitScale);

	// Branches attached to trunk
	pxSkel->AddBone("Branch0", TREE_BONE_TRUNK_LOWER, Zenith_Maths::Vector3(0.8f, 0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Branch1", TREE_BONE_TRUNK_UPPER, Zenith_Maths::Vector3(-1.0f, 0.5f, 0.3f), xIdentity, xUnitScale);
	pxSkel->AddBone("Branch2", TREE_BONE_TRUNK_UPPER, Zenith_Maths::Vector3(1.0f, 0.5f, -0.3f), xIdentity, xUnitScale);
	pxSkel->AddBone("Branch3", TREE_BONE_TRUNK_UPPER, Zenith_Maths::Vector3(0, 1.5f, 0), xIdentity, xUnitScale);

	// Leaf clusters at branch tips
	pxSkel->AddBone("Leaves0", TREE_BONE_BRANCH_3, Zenith_Maths::Vector3(0, 0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Leaves1", TREE_BONE_BRANCH_1, Zenith_Maths::Vector3(-0.5f, 0.3f, 0), xIdentity, xUnitScale);

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

static Zenith_MeshAsset* CreateTreeMesh(const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	const uint32_t uVertsPerBone = 8;
	const uint32_t uIndicesPerBone = 36;
	pxMesh->Reserve(TREE_BONE_COUNT * uVertsPerBone, TREE_BONE_COUNT * uIndicesPerBone);

	// Add a scaled cube at each bone position
	for (uint32_t uBone = 0; uBone < TREE_BONE_COUNT; uBone++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		Zenith_Maths::Vector3 xBoneWorldPos = Zenith_Maths::Vector3(xBone.m_xBindPoseModel[3]);

		Zenith_Maths::Vector3 xScale = s_axTreeBoneScales[uBone];

		uint32_t uBaseVertex = pxMesh->GetNumVerts();

		for (int i = 0; i < 8; i++)
		{
			Zenith_Maths::Vector3 xScaledOffset = s_axCubeOffsets[i] * 2.0f;
			xScaledOffset.x *= xScale.x * 10.0f;
			xScaledOffset.y *= xScale.y * 10.0f;
			xScaledOffset.z *= xScale.z * 10.0f;

			Zenith_Maths::Vector3 xPos = xBoneWorldPos + xScaledOffset;
			Zenith_Maths::Vector3 xNormal = glm::normalize(s_axCubeOffsets[i]);

			pxMesh->AddVertex(xPos, xNormal, Zenith_Maths::Vector2(0, 0));
			pxMesh->SetVertexSkinning(
				uBaseVertex + i,
				glm::uvec4(uBone, 0, 0, 0),
				glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}

		for (int i = 0; i < 36; i += 3)
		{
			pxMesh->AddTriangle(
				uBaseVertex + s_auCubeIndices[i],
				uBaseVertex + s_auCubeIndices[i + 1],
				uBaseVertex + s_auCubeIndices[i + 2]);
		}
	}

	pxMesh->AddSubmesh(0, TREE_BONE_COUNT * uIndicesPerBone, 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

static Flux_AnimationClip* CreateTreeSwayAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Sway");
	pxClip->SetDuration(2.0f);
	pxClip->SetTicksPerSecond(30);
	pxClip->SetLooping(true);

	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);
	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Root stays stationary
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// TrunkLower sways gently
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(15.0f, glm::angleAxis(glm::radians(1.0f), xZAxis));
		xChannel.AddRotationKeyframe(30.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(45.0f, glm::angleAxis(glm::radians(-1.0f), xZAxis));
		xChannel.AddRotationKeyframe(60.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("TrunkLower", std::move(xChannel));
	}

	// TrunkUpper sways more
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(15.0f, glm::angleAxis(glm::radians(2.0f), xZAxis));
		xChannel.AddRotationKeyframe(30.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(45.0f, glm::angleAxis(glm::radians(-2.0f), xZAxis));
		xChannel.AddRotationKeyframe(60.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("TrunkUpper", std::move(xChannel));
	}

	// Branches sway with phase offsets
	const char* aszBranchNames[] = {"Branch0", "Branch1", "Branch2", "Branch3"};
	const float afPhaseOffsets[] = {0.0f, 7.5f, 3.75f, 11.25f};
	for (int i = 0; i < 4; ++i)
	{
		Flux_BoneChannel xChannel;
		float fPhase = afPhaseOffsets[i];
		xChannel.AddRotationKeyframe(fmod(0.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(15.0f + fPhase, 60.0f), glm::angleAxis(glm::radians(5.0f), xZAxis));
		xChannel.AddRotationKeyframe(fmod(30.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(45.0f + fPhase, 60.0f), glm::angleAxis(glm::radians(-5.0f), xZAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(aszBranchNames[i], std::move(xChannel));
	}

	// Leaves sway most dramatically
	const char* aszLeafNames[] = {"Leaves0", "Leaves1"};
	const float afLeafPhaseOffsets[] = {5.0f, 12.0f};
	for (int i = 0; i < 2; ++i)
	{
		Flux_BoneChannel xChannel;
		float fPhase = afLeafPhaseOffsets[i];
		Zenith_Maths::Quat xSwayPos = glm::angleAxis(glm::radians(8.0f), xZAxis) *
			glm::angleAxis(glm::radians(3.0f), xXAxis);
		Zenith_Maths::Quat xSwayNeg = glm::angleAxis(glm::radians(-8.0f), xZAxis) *
			glm::angleAxis(glm::radians(-3.0f), xXAxis);

		xChannel.AddRotationKeyframe(fmod(0.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(15.0f + fPhase, 60.0f), xSwayPos);
		xChannel.AddRotationKeyframe(fmod(30.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(45.0f + fPhase, 60.0f), xSwayNeg);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(aszLeafNames[i], std::move(xChannel));
	}

	return pxClip;
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

	// Create output directory
	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
	std::filesystem::create_directories(strOutputDir);

	// Export skeleton
	std::string strSkelPath = strOutputDir + "StickFigure.zskel";
	pxSkel->Export(strSkelPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported skeleton to: %s", strSkelPath.c_str());

	// Set skeleton path on mesh before export
	pxMesh->SetSkeletonPath("Meshes/StickFigure/StickFigure.zskel");

	// Export mesh in Zenith_MeshAsset format
	std::string strMeshAssetPath = strOutputDir + "StickFigure.zasset";
	pxMesh->Export(strMeshAssetPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh asset to: %s", strMeshAssetPath.c_str());

#ifdef ZENITH_TOOLS
	// Export mesh in Flux_MeshGeometry format
	Flux_MeshGeometry* pxFluxGeometry = CreateFluxMeshGeometry(pxMesh, pxSkel);
	std::string strMeshPath = strOutputDir + "StickFigure.zmesh";
	pxFluxGeometry->Export(strMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh geometry to: %s", strMeshPath.c_str());
	delete pxFluxGeometry;

	// Export static mesh
	Flux_MeshGeometry* pxStaticGeometry = CreateStaticFluxMeshGeometry(pxMesh);
	std::string strStaticMeshPath = strOutputDir + "StickFigure_Static.zmesh";
	pxStaticGeometry->Export(strStaticMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported static mesh geometry to: %s", strStaticMeshPath.c_str());
	delete pxStaticGeometry;
#endif

	// Export animations
	std::string strIdlePath = strOutputDir + "StickFigure_Idle.zanim";
	pxIdleClip->Export(strIdlePath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported idle animation to: %s", strIdlePath.c_str());

	std::string strWalkPath = strOutputDir + "StickFigure_Walk.zanim";
	pxWalkClip->Export(strWalkPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported walk animation to: %s", strWalkPath.c_str());

	std::string strRunPath = strOutputDir + "StickFigure_Run.zanim";
	pxRunClip->Export(strRunPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported run animation to: %s", strRunPath.c_str());

	std::string strAttack1Path = strOutputDir + "StickFigure_Attack1.zanim";
	pxAttack1Clip->Export(strAttack1Path);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported attack1 animation to: %s", strAttack1Path.c_str());

	std::string strAttack2Path = strOutputDir + "StickFigure_Attack2.zanim";
	pxAttack2Clip->Export(strAttack2Path);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported attack2 animation to: %s", strAttack2Path.c_str());

	std::string strAttack3Path = strOutputDir + "StickFigure_Attack3.zanim";
	pxAttack3Clip->Export(strAttack3Path);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported attack3 animation to: %s", strAttack3Path.c_str());

	std::string strDodgePath = strOutputDir + "StickFigure_Dodge.zanim";
	pxDodgeClip->Export(strDodgePath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported dodge animation to: %s", strDodgePath.c_str());

	std::string strHitPath = strOutputDir + "StickFigure_Hit.zanim";
	pxHitClip->Export(strHitPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported hit animation to: %s", strHitPath.c_str());

	std::string strDeathPath = strOutputDir + "StickFigure_Death.zanim";
	pxDeathClip->Export(strDeathPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported death animation to: %s", strDeathPath.c_str());

	// Cleanup
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

void GenerateProceduralTreeAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Generating ProceduralTree test assets...");

	// Create all assets
	Zenith_SkeletonAsset* pxSkel = CreateTreeSkeleton();
	Zenith_MeshAsset* pxMesh = CreateTreeMesh(pxSkel);
	Flux_AnimationClip* pxSwayClip = CreateTreeSwayAnimation();

	// Create output directory
	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
	std::filesystem::create_directories(strOutputDir);

	// Export skeleton
	std::string strSkelPath = strOutputDir + "Tree.zskel";
	pxSkel->Export(strSkelPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported skeleton to: %s", strSkelPath.c_str());

	// Set skeleton path on mesh before export
	pxMesh->SetSkeletonPath("Meshes/ProceduralTree/Tree.zskel");

	// Export mesh in Zenith_MeshAsset format
	std::string strMeshAssetPath = strOutputDir + "Tree.zasset";
	pxMesh->Export(strMeshAssetPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh asset to: %s", strMeshAssetPath.c_str());

#ifdef ZENITH_TOOLS
	// Export mesh in Flux_MeshGeometry format
	Flux_MeshGeometry* pxFluxGeometry = CreateFluxMeshGeometry(pxMesh, pxSkel);
	std::string strMeshPath = strOutputDir + "Tree.zmesh";
	pxFluxGeometry->Export(strMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh geometry to: %s", strMeshPath.c_str());

	// Export static mesh
	Flux_MeshGeometry* pxStaticGeometry = CreateStaticFluxMeshGeometry(pxMesh);
	std::string strStaticMeshPath = strOutputDir + "Tree_Static.zmesh";
	pxStaticGeometry->Export(strStaticMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported static mesh geometry to: %s", strStaticMeshPath.c_str());

	// Bake and export VAT
	Flux_AnimationTexture* pxVAT = new Flux_AnimationTexture();
	std::vector<Flux_AnimationClip*> axAnimations;
	axAnimations.push_back(pxSwayClip);
	bool bBakeSuccess = pxVAT->BakeFromAnimations(pxFluxGeometry, pxSkel, axAnimations, 30);
	if (bBakeSuccess)
	{
		std::string strVATPath = strOutputDir + "Tree_Sway.zanmt";
		pxVAT->Export(strVATPath);
		Zenith_Log(LOG_CATEGORY_ASSET, "  Exported VAT to: %s", strVATPath.c_str());
		Zenith_Log(LOG_CATEGORY_ASSET, "    VAT dimensions: %u x %u (verts x frames)",
			pxVAT->GetTextureWidth(), pxVAT->GetTextureHeight());
	}
	delete pxVAT;
	delete pxStaticGeometry;
	delete pxFluxGeometry;
#endif

	// Export animation
	std::string strSwayPath = strOutputDir + "Tree_Sway.zanim";
	pxSwayClip->Export(strSwayPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported sway animation to: %s", strSwayPath.c_str());

	// Cleanup
	delete pxSwayClip;
	delete pxMesh;
	delete pxSkel;

	Zenith_Log(LOG_CATEGORY_ASSET, "ProceduralTree assets generated at: %s", strOutputDir.c_str());
}

void GenerateTestAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "=== Generating Test Assets ===");
	GenerateStickFigureAssets();
	GenerateProceduralTreeAssets();
	Zenith_Log(LOG_CATEGORY_ASSET, "=== Test Asset Generation Complete ===");
}
