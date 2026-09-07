#include "Zenith.h"
#include "AssetHandling/Zenith_BoneMaskAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"   // Flux_BoneMask — the resolve target

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

//=============================================================================
// Zenith_BoneMaskEntry
//=============================================================================
void Zenith_BoneMaskEntry::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strBoneName;
	xStream << m_fWeight;
}

void Zenith_BoneMaskEntry::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strBoneName;
	xStream >> m_fWeight;
}

//=============================================================================
// Authoring
//=============================================================================
void Zenith_BoneMaskAsset::SetBoneWeight(const std::string& strBoneName, float fWeight)
{
	if (strBoneName.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskAsset::SetBoneWeight: a mask entry must name a bone");
		return;
	}

	for (u_int u = 0; u < m_xEntries.GetSize(); ++u)
	{
		if (m_xEntries.Get(u).m_strBoneName == strBoneName)
		{
			m_xEntries.Get(u).m_fWeight = fWeight;
			return;
		}
	}

	Zenith_BoneMaskEntry xEntry;
	xEntry.m_strBoneName = strBoneName;
	xEntry.m_fWeight = fWeight;
	m_xEntries.PushBack(xEntry);
}

float Zenith_BoneMaskAsset::GetBoneWeight(const std::string& strBoneName) const
{
	for (u_int u = 0; u < m_xEntries.GetSize(); ++u)
	{
		if (m_xEntries.Get(u).m_strBoneName == strBoneName)
		{
			return m_xEntries.Get(u).m_fWeight;
		}
	}
	return 0.0f;
}

bool Zenith_BoneMaskAsset::HasBone(const std::string& strBoneName) const
{
	for (u_int u = 0; u < m_xEntries.GetSize(); ++u)
	{
		if (m_xEntries.Get(u).m_strBoneName == strBoneName)
		{
			return true;
		}
	}
	return false;
}

void Zenith_BoneMaskAsset::RemoveBone(const std::string& strBoneName)
{
	for (u_int u = 0; u < m_xEntries.GetSize(); ++u)
	{
		if (m_xEntries.Get(u).m_strBoneName == strBoneName)
		{
			// Order-preserving: the entry list is what an editor lists, and a
			// swap-remove would shuffle the rows a user is looking at.
			m_xEntries.Remove(u);
			return;
		}
	}
}

//=============================================================================
// Resolution
//=============================================================================
void Zenith_BoneMaskAsset::CopyFrom(const Zenith_BoneMaskAsset& xOther)
{
	// ★ THE AUTHORED CONTENT ONLY — never the Zenith_Asset half. The base carries
	// the registry PATH and the REFCOUNT, and copying either would give two
	// objects one identity: an editor working copy that claimed to be the cached
	// asset, or a refcount the registry never handed out. This is the same split
	// Flux_AnimatorControllerDef::CopyFrom makes, and it is what lets an editor
	// document hold a Zenith_BoneMaskAsset by value as a deep working copy.
	m_xEntries.Clear();
	m_xEntries.Reserve(xOther.m_xEntries.GetSize());
	for (u_int u = 0; u < xOther.m_xEntries.GetSize(); ++u)
	{
		m_xEntries.PushBack(xOther.m_xEntries.Get(u));
	}
	m_bHasAvatarMask = xOther.m_bHasAvatarMask;
}

bool Zenith_BoneMaskAsset::ResolveTo(const Zenith_SkeletonAsset& xSkeleton, Flux_BoneMask& xOutMask) const
{
	// ★ THE NAME->INDEX WALK LIVES ON Flux_BoneMask, NOT HERE (WU-7.1). It used
	// to be a loop in this function against Zenith_SkeletonAsset::GetBoneIndex,
	// beside a SECOND resolve inside Flux_BoneMask that went through
	// Flux_MeshGeometry's entirely different bone map — two answers to "which
	// index is this bone", one of them the one the whole runtime pose path
	// actually uses. There is one now, and this is a caller of it.
	Zenith_Vector<std::string> xNames;
	Zenith_Vector<float> xWeights;
	xNames.Reserve(m_xEntries.GetSize());
	xWeights.Reserve(m_xEntries.GetSize());
	for (u_int u = 0; u < m_xEntries.GetSize(); ++u)
	{
		xNames.PushBack(m_xEntries.Get(u).m_strBoneName);
		xWeights.PushBack(m_xEntries.Get(u).m_fWeight);
	}

	Zenith_Vector<std::string> xUnresolved;
	const bool bAllResolved = xOutMask.SetFromBoneNames(xSkeleton, xNames, xWeights, &xUnresolved);

	// ★ NAMED, NOT COUNTED. "3 bones did not resolve" is not actionable; the name
	// is, because it is either a typo or the wrong rig. Flux_BoneMask collects
	// them rather than logging: it does not know which asset they came from, and
	// the asset PATH is most of what makes the message useful.
	for (u_int u = 0; u < xUnresolved.GetSize(); ++u)
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION,
			"[BoneMask] '%s' names bone '%s', which the skeleton does not carry — that bone's weight is DROPPED",
			GetPath().c_str(), xUnresolved.Get(u).c_str());
	}

	return bAllResolved;
}

//=============================================================================
// Serialization
//=============================================================================
void Zenith_BoneMaskAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_WriteStreamHeader(xStream, uZENITH_ANIMMASK_ASSET_TYPE_ID, uZENITH_ANIMMASK_SCHEMA_CURRENT);

	// D47: WRITTEN, never inferred from the weights below.
	xStream << m_bHasAvatarMask;

	const u_int uNumEntries = m_xEntries.GetSize();
	xStream << uNumEntries;
	for (u_int u = 0; u < uNumEntries; ++u)
	{
		m_xEntries.Get(u).WriteToDataStream(xStream);
	}
}

Zenith_Status Zenith_BoneMaskAsset::ParseStream(Zenith_DataStream& xStream)
{
	// The envelope is MANDATORY and the schema must be EXACTLY current — the same
	// contract .zanim carries. Zenith_ReadStreamHeader restores the cursor on every
	// failure path, so a refused stream is handed back untouched.
	Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMMASK_ASSET_TYPE_ID);
	if (!xHeader.IsOk())
	{
		Zenith_Assert(false, "Zenith_BoneMaskAsset::ParseStream: stream carries no valid " ZENITH_ANIMMASK_EXT " envelope");
		ClearEntries();
		return xHeader.Error();
	}
	if (xHeader.Value().m_uSchemaVersion != uZENITH_ANIMMASK_SCHEMA_CURRENT)
	{
		Zenith_Assert(false, "Zenith_BoneMaskAsset::ParseStream: " ZENITH_ANIMMASK_EXT " schema %u is not the current %u",
			xHeader.Value().m_uSchemaVersion, uZENITH_ANIMMASK_SCHEMA_CURRENT);
		ClearEntries();
		return Zenith_ErrorCode::VERSION_MISMATCH;
	}

	ClearEntries();

	xStream >> m_bHasAvatarMask;

	u_int uNumEntries = 0;
	xStream >> uNumEntries;
	// One entry per bone at most in any sane mask; Zenith_SkeletonAsset::MAX_BONES
	// is 100, so this is generous rather than tight.
	constexpr u_int uMAX_ENTRIES = 4096u;
	Zenith_Assert(uNumEntries <= uMAX_ENTRIES,
		"Zenith_BoneMaskAsset: entry count %u exceeds limit - possible corruption", uNumEntries);
	if (uNumEntries > uMAX_ENTRIES)
	{
		ClearEntries();
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	m_xEntries.Reserve(uNumEntries);
	for (u_int u = 0; u < uNumEntries; ++u)
	{
		Zenith_BoneMaskEntry xEntry;
		xEntry.ReadFromDataStream(xStream);
		m_xEntries.PushBack(xEntry);
	}

	return true;
}

void Zenith_BoneMaskAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	ParseStream(xStream);
}

bool Zenith_BoneMaskAsset::Export(const std::string& strPath) const
{
	if (strPath.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskAsset::Export: empty path");
		return false;
	}

	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(Zenith_AssetRegistry::ResolvePath(strPath).c_str());
	return true;
}

Zenith_Status Zenith_BoneMaskAsset::LoadFromFile(const std::string& strPath)
{
	if (strPath.empty())
	{
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		// A missing file is a reportable condition, not a programming error — no
		// assert, matching Zenith_AnimationAsset's .zanim path.
		Zenith_Log(LOG_CATEGORY_ANIMATION, "Failed to read bone mask file: %s", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	return ParseStream(xStream);
}

#ifdef ZENITH_TOOLS
void Zenith_BoneMaskAsset::RenderPropertiesPanel()
{
	ImGui::Text("Bone Mask");
	ImGui::Text("Bones: %u   HasAvatarMask: %s", m_xEntries.GetSize(), m_bHasAvatarMask ? "yes" : "no");
	for (u_int u = 0; u < m_xEntries.GetSize(); ++u)
	{
		ImGui::Text("  %s = %.3f", m_xEntries.Get(u).m_strBoneName.c_str(), m_xEntries.Get(u).m_fWeight);
	}
}
#endif

#ifdef ZENITH_TESTING
#include "AssetHandling/Zenith_BoneMaskAsset.Tests.inl"
#endif
