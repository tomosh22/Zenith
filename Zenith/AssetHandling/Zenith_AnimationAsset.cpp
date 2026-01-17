#include "Zenith.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "DataStream/Zenith_DataStream.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

Zenith_AnimationAsset::Zenith_AnimationAsset()
	: m_pxClip(nullptr)
	, m_bOwnsClip(true)
{
}

Zenith_AnimationAsset::~Zenith_AnimationAsset()
{
	if (m_bOwnsClip && m_pxClip)
	{
		delete m_pxClip;
		m_pxClip = nullptr;
	}
}

void Zenith_AnimationAsset::SetClip(Flux_AnimationClip* pxClip)
{
	if (m_bOwnsClip && m_pxClip)
	{
		delete m_pxClip;
	}
	m_pxClip = pxClip;
	m_bOwnsClip = true;
}

Flux_AnimationClip* Zenith_AnimationAsset::ReleaseClip()
{
	Flux_AnimationClip* pxClip = m_pxClip;
	m_pxClip = nullptr;
	m_bOwnsClip = false;
	return pxClip;
}

bool Zenith_AnimationAsset::LoadFromFile(const std::string& strPath)
{
	if (strPath.empty())
	{
		return false;
	}

	// Check file extension to determine loader
	bool bIsZanim = false;
	size_t uDotPos = strPath.rfind('.');
	if (uDotPos != std::string::npos)
	{
		std::string strExt = strPath.substr(uDotPos);
		bIsZanim = (strExt == ZENITH_ANIMATION_EXT || strExt == ".zanim");
	}

	if (bIsZanim)
	{
		// Load from binary .zanim format
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		if (!xStream.IsValid())
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "Failed to read animation file: %s", strPath.c_str());
			return false;
		}

		m_pxClip = new Flux_AnimationClip();
		m_pxClip->ReadFromDataStream(xStream);
		m_bOwnsClip = true;

		Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation from zanim: %s", strPath.c_str());
		return true;
	}
	else
	{
		// Load from source format (FBX, glTF, etc.) via Assimp
		Assimp::Importer xImporter;
		const aiScene* pxScene = xImporter.ReadFile(strPath,
			aiProcess_Triangulate |
			aiProcess_LimitBoneWeights |
			aiProcess_ValidateDataStructure
		);

		if (!pxScene || !pxScene->mRootNode)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "Failed to load animation via Assimp: %s", strPath.c_str());
			return false;
		}

		if (pxScene->mNumAnimations == 0)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "No animations found in: %s", strPath.c_str());
			return false;
		}

		m_pxClip = new Flux_AnimationClip();
		m_pxClip->LoadFromAssimp(pxScene->mAnimations[0], pxScene->mRootNode);
		m_pxClip->SetSourcePath(strPath);
		m_bOwnsClip = true;

		Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation via Assimp: %s", strPath.c_str());
		return true;
	}
}
