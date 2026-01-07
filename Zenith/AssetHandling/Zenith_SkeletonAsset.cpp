#include "Zenith.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

//------------------------------------------------------------------------------
// Bone Serialization
//------------------------------------------------------------------------------

void Zenith_SkeletonAsset::Bone::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;
	xStream << m_iParentIndex;

	xStream << m_xBindPosition.x;
	xStream << m_xBindPosition.y;
	xStream << m_xBindPosition.z;
	xStream << m_xBindRotation.w;
	xStream << m_xBindRotation.x;
	xStream << m_xBindRotation.y;
	xStream << m_xBindRotation.z;
	xStream << m_xBindScale.x;
	xStream << m_xBindScale.y;
	xStream << m_xBindScale.z;

	// Write matrices (16 floats each)
	xStream.WriteData(&m_xBindPoseLocal[0][0], sizeof(Zenith_Maths::Matrix4));
	xStream.WriteData(&m_xBindPoseModel[0][0], sizeof(Zenith_Maths::Matrix4));
	xStream.WriteData(&m_xInverseBindPose[0][0], sizeof(Zenith_Maths::Matrix4));

	// Write flag
	xStream << m_bHasAssimpInverseBindPose;
}

void Zenith_SkeletonAsset::Bone::ReadFromDataStream(Zenith_DataStream& xStream, uint32_t uVersion)
{
	xStream >> m_strName;
	xStream >> m_iParentIndex;

	xStream >> m_xBindPosition.x;
	xStream >> m_xBindPosition.y;
	xStream >> m_xBindPosition.z;
	xStream >> m_xBindRotation.w;
	xStream >> m_xBindRotation.x;
	xStream >> m_xBindRotation.y;
	xStream >> m_xBindRotation.z;
	xStream >> m_xBindScale.x;
	xStream >> m_xBindScale.y;
	xStream >> m_xBindScale.z;

	xStream.ReadData(&m_xBindPoseLocal[0][0], sizeof(Zenith_Maths::Matrix4));
	xStream.ReadData(&m_xBindPoseModel[0][0], sizeof(Zenith_Maths::Matrix4));
	xStream.ReadData(&m_xInverseBindPose[0][0], sizeof(Zenith_Maths::Matrix4));

	// Read flag (added in version 2)
	if (uVersion >= 2)
	{
		xStream >> m_bHasAssimpInverseBindPose;
	}
}

//------------------------------------------------------------------------------
// Move Constructor / Assignment
//------------------------------------------------------------------------------

Zenith_SkeletonAsset::Zenith_SkeletonAsset(Zenith_SkeletonAsset&& xOther)
{
	m_xBones = std::move(xOther.m_xBones);
	m_xBoneNameToIndex = std::move(xOther.m_xBoneNameToIndex);
	m_strSourcePath = std::move(xOther.m_strSourcePath);
}

Zenith_SkeletonAsset& Zenith_SkeletonAsset::operator=(Zenith_SkeletonAsset&& xOther)
{
	if (this != &xOther)
	{
		m_xBones = std::move(xOther.m_xBones);
		m_xBoneNameToIndex = std::move(xOther.m_xBoneNameToIndex);
		m_strSourcePath = std::move(xOther.m_strSourcePath);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Loading and Saving
//------------------------------------------------------------------------------

Zenith_SkeletonAsset* Zenith_SkeletonAsset::LoadFromFile(const char* szPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	// Validate file was loaded successfully
	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "LoadFromFile: Failed to read skeleton file '%s'", szPath);
		return nullptr;
	}

	Zenith_SkeletonAsset* pxAsset = new Zenith_SkeletonAsset();
	pxAsset->ReadFromDataStream(xStream);
	pxAsset->m_strSourcePath = szPath;

	// Debug logging for skeleton
	Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded %s with %u bones:", szPath, pxAsset->GetNumBones());
	for (uint32_t u = 0; u < pxAsset->GetNumBones(); u++)
	{
		const Bone& xBone = pxAsset->GetBone(u);
		Zenith_Maths::Vector3 xBindPos = Zenith_Maths::Vector3(xBone.m_xBindPoseModel[3]);
		Zenith_Log(LOG_CATEGORY_ANIMATION, "  [%u] '%s' parent=%d, BindPoseModel translation=(%.2f, %.2f, %.2f)",
			u, xBone.m_strName.c_str(), xBone.m_iParentIndex,
			xBindPos.x, xBindPos.y, xBindPos.z);
	}

	return pxAsset;
}

void Zenith_SkeletonAsset::Export(const char* szPath) const
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(szPath);
}

void Zenith_SkeletonAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Version
	xStream << static_cast<uint32_t>(ZENITH_SKELETON_ASSET_VERSION);

	// Bone count
	uint32_t uNumBones = static_cast<uint32_t>(m_xBones.GetSize());
	xStream << uNumBones;

	// Write each bone
	for (uint32_t u = 0; u < uNumBones; u++)
	{
		m_xBones.Get(u).WriteToDataStream(xStream);
	}
}

void Zenith_SkeletonAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Reset();

	// Version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion != ZENITH_SKELETON_ASSET_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "Version mismatch: expected %u, got %u", ZENITH_SKELETON_ASSET_VERSION, uVersion);
	}

	// Bone count
	uint32_t uNumBones;
	xStream >> uNumBones;

	// Read each bone
	for (uint32_t u = 0; u < uNumBones; u++)
	{
		Bone xBone;
		xBone.ReadFromDataStream(xStream, uVersion);

		m_xBoneNameToIndex[xBone.m_strName] = u;
		m_xBones.PushBack(std::move(xBone));
	}
}

//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

int32_t Zenith_SkeletonAsset::GetBoneIndex(const std::string& strName) const
{
	auto it = m_xBoneNameToIndex.find(strName);
	if (it != m_xBoneNameToIndex.end())
	{
		return static_cast<int32_t>(it->second);
	}
	return INVALID_BONE_INDEX;
}

bool Zenith_SkeletonAsset::HasBone(const std::string& strName) const
{
	return m_xBoneNameToIndex.find(strName) != m_xBoneNameToIndex.end();
}

Zenith_Vector<uint32_t> Zenith_SkeletonAsset::GetRootBones() const
{
	Zenith_Vector<uint32_t> xRoots;
	for (uint32_t u = 0; u < m_xBones.GetSize(); u++)
	{
		if (m_xBones.Get(u).m_iParentIndex == INVALID_BONE_INDEX)
		{
			xRoots.PushBack(u);
		}
	}
	return xRoots;
}

Zenith_Vector<uint32_t> Zenith_SkeletonAsset::GetChildBones(uint32_t uParentIndex) const
{
	Zenith_Vector<uint32_t> xChildren;
	for (uint32_t u = 0; u < m_xBones.GetSize(); u++)
	{
		if (m_xBones.Get(u).m_iParentIndex == static_cast<int32_t>(uParentIndex))
		{
			xChildren.PushBack(u);
		}
	}
	return xChildren;
}

//------------------------------------------------------------------------------
// Skeleton Building
//------------------------------------------------------------------------------

uint32_t Zenith_SkeletonAsset::AddBone(
	const std::string& strName,
	int32_t iParentIndex,
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Quat& xRotation,
	const Zenith_Maths::Vector3& xScale)
{
	Zenith_Assert(m_xBones.GetSize() < MAX_BONES, "Exceeded maximum bone count");
	Zenith_Assert(m_xBoneNameToIndex.find(strName) == m_xBoneNameToIndex.end(), "Duplicate bone name");

	Bone xBone;
	xBone.m_strName = strName;
	xBone.m_iParentIndex = iParentIndex;
	xBone.m_xBindPosition = xPosition;
	xBone.m_xBindRotation = xRotation;
	xBone.m_xBindScale = xScale;

	// Compute local bind pose matrix
	Zenith_Maths::Matrix4 xTranslation = glm::translate(glm::identity<Zenith_Maths::Matrix4>(), xPosition);
	Zenith_Maths::Matrix4 xRotationMat = glm::toMat4(xRotation);
	Zenith_Maths::Matrix4 xScaleMat = glm::scale(glm::identity<Zenith_Maths::Matrix4>(), xScale);
	xBone.m_xBindPoseLocal = xTranslation * xRotationMat * xScaleMat;

	uint32_t uIndex = static_cast<uint32_t>(m_xBones.GetSize());
	m_xBoneNameToIndex[strName] = uIndex;
	m_xBones.PushBack(std::move(xBone));

	return uIndex;
}

void Zenith_SkeletonAsset::SetInverseBindPose(uint32_t uBoneIndex, const Zenith_Maths::Matrix4& xInvBindPose)
{
	Zenith_Assert(uBoneIndex < m_xBones.GetSize(), "Invalid bone index");
	Bone& xBone = m_xBones.Get(uBoneIndex);
	xBone.m_xInverseBindPose = xInvBindPose;
	xBone.m_xBindPoseModel = glm::inverse(xInvBindPose);
	xBone.m_bHasAssimpInverseBindPose = true;
}

void Zenith_SkeletonAsset::ComputeBindPoseMatrices()
{
	// First, compute local bind pose matrix from TRS for each bone
	for (uint32_t u = 0; u < m_xBones.GetSize(); u++)
	{
		Bone& xBone = m_xBones.Get(u);
		Zenith_Maths::Matrix4 xTranslation = glm::translate(glm::identity<Zenith_Maths::Matrix4>(), xBone.m_xBindPosition);
		Zenith_Maths::Matrix4 xRotation = glm::toMat4(xBone.m_xBindRotation);
		Zenith_Maths::Matrix4 xScale = glm::scale(glm::identity<Zenith_Maths::Matrix4>(), xBone.m_xBindScale);
		xBone.m_xBindPoseLocal = xTranslation * xRotation * xScale;
	}

	// Then, recursively compute model-space bind pose from TRS hierarchy
	// This overwrites the mesh-relative m_xBindPoseModel with world-relative values
	// computed from the TRS hierarchy (which includes baked non-bone ancestor transforms)
	Zenith_Vector<uint32_t> xRoots = GetRootBones();
	for (uint32_t u = 0; u < xRoots.GetSize(); u++)
	{
		ComputeBindPoseRecursive(xRoots.Get(u), glm::identity<Zenith_Maths::Matrix4>());
	}
}

void Zenith_SkeletonAsset::ComputeBindPoseRecursive(uint32_t uBoneIndex, const Zenith_Maths::Matrix4& xParentModelPose)
{
	Bone& xBone = m_xBones.Get(uBoneIndex);

	// All bones in the skeleton should have m_bHasAssimpInverseBindPose = true
	// because we only export actual bones (not non-bone ancestors like Armature)
	Zenith_Assert(xBone.m_bHasAssimpInverseBindPose, "Bone should have Assimp inverse bind pose");

	// Compute model-space bind pose from TRS hierarchy
	xBone.m_xBindPoseModel = xParentModelPose * xBone.m_xBindPoseLocal;

	// IMPORTANT: Do NOT overwrite m_xBindPosition/Rotation/Scale here!
	// These were set from scene graph node transforms in AddBone(), and that's
	// what animations expect. Animations (from aiNodeAnim) provide transforms
	// relative to scene graph parents, so we must preserve these for animation
	// compatibility.
	//
	// The local TRS values are used for:
	// 1. Initializing skeleton instance at bind pose
	// 2. As fallback when animation doesn't have a channel for this bone
	//
	// m_xBindPoseModel (computed above from TRS) gives world-space bone positions
	// m_xInverseBindPose (from Assimp) transforms mesh-local to bone-local space

	// Process children recursively
	Zenith_Vector<uint32_t> xChildren = GetChildBones(uBoneIndex);
	for (uint32_t u = 0; u < xChildren.GetSize(); u++)
	{
		ComputeBindPoseRecursive(xChildren.Get(u), xBone.m_xBindPoseModel);
	}
}

void Zenith_SkeletonAsset::Reset()
{
	m_xBones.Clear();
	m_xBoneNameToIndex.clear();
	m_strSourcePath.clear();
}
