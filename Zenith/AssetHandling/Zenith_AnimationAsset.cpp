#include "Zenith.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"   // ResolvePath — the reload takes a prefixed OR plain path
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#endif

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

namespace
{
	// One place that decides whether a path names the binary clip format. Both the
	// initial load and the reload ask the same question, and a reload of anything else
	// is refused rather than being routed into the importer.
	bool AnimationPathIsZanim(const std::string& strPath)
	{
		const size_t uDotPos = strPath.rfind('.');
		if (uDotPos == std::string::npos)
		{
			return false;
		}
		return strPath.substr(uDotPos) == ZENITH_ANIMATION_EXT;
	}
}

Zenith_Status Zenith_AnimationAsset::LoadZanimIntoLiveClip(const std::string& strResolvedPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strResolvedPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "Failed to read animation file: %s", strResolvedPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	// ★ D27: PARSE INTO A TEMPORARY, THEN SWAP. xStaging is a STACK LOCAL — no second
	// Flux_AnimationClip is ever allocated on this path, so calling it repeatedly
	// cannot leak one (which is exactly what the old `m_pxClip = new ...` line did the
	// moment anything called it twice). Nothing below the live clip is touched until
	// the parse has succeeded, so a refused file leaves a playing clip exactly as it
	// was rather than emptying it.
	Flux_AnimationClip xStaging;
	const Zenith_Status xParseStatus = xStaging.ParseStream(xStream);
	if (!xParseStatus.IsOk())
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION,
			"Refused animation file (not a current .zanim): %s", strResolvedPath.c_str());
		return xParseStatus.Error();
	}

	if (m_pxClip == nullptr)
	{
		// First load. A fresh clip is unnamed, so the D28 name guard treats the replace
		// below as "populate", not "rename".
		m_pxClip = new Flux_AnimationClip();
		m_bOwnsClip = true;
	}

	// D26/D28. The ONLY write to the live clip, and it keeps the clip's ADDRESS — the
	// pointer a controller borrowed and a state machine resolved stays valid and starts
	// sampling the new content. The name-immutability rule is enforced inside
	// ReplaceContentsFrom, so a file naming a different clip leaves this one untouched.
	if (!m_pxClip->ReplaceContentsFrom(xStaging))
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION,
			"Animation file '%s' holds clip '%s' but this asset's live clip is '%s' — a reload may not rename a clip (that is Save As)",
			strResolvedPath.c_str(), xStaging.GetName().c_str(), m_pxClip->GetName().c_str());
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	return true;
}

bool Zenith_AnimationAsset::ReloadFromDisk()
{
	if (GetPath().empty())
	{
		Zenith_Assert(false, "Zenith_AnimationAsset::ReloadFromDisk: this asset has no path — a procedural clip has no file to reload from. Use the explicit-path overload.");
		return false;
	}
	return ReloadFromDisk(GetPath());
}

bool Zenith_AnimationAsset::ReloadFromDisk(const std::string& strPath)
{
	ZENITH_PROFILE_SCOPE("Animation Reload");

	// A prefixed path ("game:Anims/walk.zanim") or a plain filesystem path both land
	// here; ResolvePath is the one translation and passes an unprefixed path through.
	const std::string strResolvedPath = Zenith_AssetRegistry::ResolvePath(strPath);

	if (!AnimationPathIsZanim(strResolvedPath))
	{
		// Refused BEFORE the stream is read, so a non-.zanim path produces one clear
		// message instead of tripping ParseStream's missing-envelope assert.
		Zenith_Assert(false, "Zenith_AnimationAsset::ReloadFromDisk: '%s' is not a " ZENITH_ANIMATION_EXT " — re-importing a source file is an import, not a reload", strResolvedPath.c_str());
		return false;
	}

	return LoadZanimIntoLiveClip(strResolvedPath).IsOk();
}

Zenith_Status Zenith_AnimationAsset::LoadFromFile(const std::string& strPath)
{
	ZENITH_PROFILE_SCOPE("Animation Load");
	if (strPath.empty())
	{
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	if (AnimationPathIsZanim(strPath))
	{
		// The registry hands LoadFromFile an already-resolved path. Same read/validate/
		// swap primitive the reload uses, so the two cannot drift apart — and the
		// STATUS reaches the caller, where it used to be an unconditional `true`.
		const Zenith_Status xStatus = LoadZanimIntoLiveClip(strPath);
		if (!xStatus.IsOk())
		{
			return xStatus.Error();
		}

		Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation from zanim: %s", strPath.c_str());
		return true;
	}
	else
	{
#ifdef ZENITH_TOOLS
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
			return Zenith_ErrorCode::CORRUPT_DATA;
		}

		if (pxScene->mNumAnimations == 0)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "No animations found in: %s", strPath.c_str());
			return Zenith_ErrorCode::CORRUPT_DATA;
		}

		// SetClip, not a bare `m_pxClip = new ...`: it deletes whatever this asset
		// already owned. The raw assignment leaked the previous clip outright, which is
		// the same defect the .zanim branch above carried before it grew a reload path.
		SetClip(new Flux_AnimationClip());
		m_pxClip->LoadFromAssimp(pxScene->mAnimations[0], pxScene->mRootNode);
		m_pxClip->SetSourcePath(strPath);

		Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation via Assimp: %s", strPath.c_str());
		return true;
#else
		Zenith_Log(LOG_CATEGORY_ANIMATION, "Source format loading not supported without tools: %s", strPath.c_str());
		return Zenith_ErrorCode::INVALID_ARGUMENT;
#endif
	}
}

#include "AssetHandling/Zenith_AnimationAsset.Tests.inl"
