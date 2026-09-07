#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimationDocument.h"
#include "Editor/Zenith_EditorAnimCommands.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

bool Zenith_AnimationDocument_ForceLink()
{
	// ★ ANCHORS BOTH TUs FROM ONE CALL. Zenith_Editor::Initialise calls this;
	// this calls the commands TU's anchor. Neither .obj is named by any live
	// caller yet (the dope-sheet panel is WU-3.2), and an .obj the linker never
	// pulls in takes its ZENITH_TEST registrars with it — the unit count moves by
	// zero and nothing reds. Same idiom, same reason, as
	// Zenith_BehaviourGraphAsset_ForceLink.
	return Zenith_EditorAnimCommands_ForceLink();
}

namespace
{
	//--------------------------------------------------------------------------
	// ★ ONE MUTATION PATH FOR A BONE CHANNEL AND FOR ROOT MOTION.
	//
	// Flux_BoneChannel and Flux_RootMotion expose the SAME six mutators with the
	// SAME signatures and the SAME contract (D16) — but they share no base class,
	// and giving them one would put a vtable on two structs that are serialized
	// by the thousand. This tiny value type resolves the difference ONCE, at the
	// top of each verb, so nothing below it branches on "is this root motion"
	// again. Every duplicated branch would be a place the two paths could drift.
	//--------------------------------------------------------------------------
	struct AnimTrackAccess
	{
		Flux_BoneChannel* m_pxChannel = nullptr;
		Flux_RootMotion* m_pxRoot = nullptr;
		Flux_AnimTrack m_eTrack = FLUX_ANIM_TRACK_POSITION;

		bool IsValid() const { return m_pxChannel != nullptr || m_pxRoot != nullptr; }

		u_int GetKeyframeCount() const
		{
			return m_pxChannel != nullptr ? m_pxChannel->GetKeyframeCount(m_eTrack) : m_pxRoot->GetKeyframeCount(m_eTrack);
		}

		u_int FindKeyframeAtTime(float fTimeSeconds) const
		{
			return m_pxChannel != nullptr ? m_pxChannel->FindKeyframeAtTime(m_eTrack, fTimeSeconds)
			                              : m_pxRoot->FindKeyframeAtTime(m_eTrack, fTimeSeconds);
		}

		bool SetKeyframeTime(u_int uKeyIndex, float fNewTimeSeconds, u_int* puOutKeyIndex) const
		{
			return m_pxChannel != nullptr ? m_pxChannel->SetKeyframeTime(m_eTrack, uKeyIndex, fNewTimeSeconds, puOutKeyIndex)
			                              : m_pxRoot->SetKeyframeTime(m_eTrack, uKeyIndex, fNewTimeSeconds, puOutKeyIndex);
		}

		bool SetKeyframeValue(u_int uKeyIndex, const Zenith_Maths::Vector3& xValue) const
		{
			return m_pxChannel != nullptr ? m_pxChannel->SetKeyframeValue(m_eTrack, uKeyIndex, xValue)
			                              : m_pxRoot->SetKeyframeValue(m_eTrack, uKeyIndex, xValue);
		}

		bool SetKeyframeValue(u_int uKeyIndex, const Zenith_Maths::Quat& xRotation) const
		{
			return m_pxChannel != nullptr ? m_pxChannel->SetKeyframeValue(m_eTrack, uKeyIndex, xRotation)
			                              : m_pxRoot->SetKeyframeValue(m_eTrack, uKeyIndex, xRotation);
		}

		bool InsertKeyframeAt(float fTimeSeconds, const Zenith_Maths::Vector3& xValue, u_int* puOutKeyIndex) const
		{
			return m_pxChannel != nullptr ? m_pxChannel->InsertKeyframeAt(m_eTrack, fTimeSeconds, xValue, puOutKeyIndex)
			                              : m_pxRoot->InsertKeyframeAt(m_eTrack, fTimeSeconds, xValue, puOutKeyIndex);
		}

		bool InsertKeyframeAt(float fTimeSeconds, const Zenith_Maths::Quat& xRotation, u_int* puOutKeyIndex) const
		{
			return m_pxChannel != nullptr ? m_pxChannel->InsertKeyframeAt(m_eTrack, fTimeSeconds, xRotation, puOutKeyIndex)
			                              : m_pxRoot->InsertKeyframeAt(m_eTrack, fTimeSeconds, xRotation, puOutKeyIndex);
		}
	};

	AnimTrackAccess ResolveTrack(Flux_AnimationClip& xClip, const Zenith_AnimTrackId& xTrack, bool bCreateChannel)
	{
		AnimTrackAccess xAccess;
		xAccess.m_eTrack = xTrack.m_eTrack;
		if (xTrack.m_bRootMotion)
		{
			xAccess.m_pxRoot = &xClip.GetRootMotion();
			return xAccess;
		}
		xAccess.m_pxChannel = bCreateChannel ? &xClip.GetOrAddBoneChannel(xTrack.m_strBoneName)
		                                     : xClip.GetBoneChannelMutable(xTrack.m_strBoneName);
		return xAccess;
	}

	u_int ClipTrackKeyCount(const Flux_AnimationClip& xClip, const Zenith_AnimTrackId& xTrack)
	{
		if (xTrack.m_bRootMotion)
		{
			return xClip.GetRootMotion().GetKeyframeCount(xTrack.m_eTrack);
		}
		const Flux_BoneChannel* pxChannel = xClip.GetBoneChannel(xTrack.m_strBoneName);
		return pxChannel != nullptr ? pxChannel->GetKeyframeCount(xTrack.m_eTrack) : 0u;
	}

	bool ClipTrackKeyTime(const Flux_AnimationClip& xClip, const Zenith_AnimTrackId& xTrack, u_int uKeyIndex, float& fOutTimeSeconds)
	{
		if (xTrack.m_bRootMotion)
		{
			return xClip.GetRootMotion().GetKeyframeTime(xTrack.m_eTrack, uKeyIndex, fOutTimeSeconds);
		}
		const Flux_BoneChannel* pxChannel = xClip.GetBoneChannel(xTrack.m_strBoneName);
		return pxChannel != nullptr && pxChannel->GetKeyframeTime(xTrack.m_eTrack, uKeyIndex, fOutTimeSeconds);
	}

	bool ClipTrackKeyValue(const Flux_AnimationClip& xClip, const Zenith_AnimTrackId& xTrack, u_int uKeyIndex, Zenith_AnimKeyValue& xOut)
	{
		if (xTrack.m_bRootMotion)
		{
			const Flux_RootMotion& xRoot = xClip.GetRootMotion();
			if (xTrack.m_eTrack == FLUX_ANIM_TRACK_ROTATION)
			{
				if (uKeyIndex >= xRoot.m_xRotationDeltas.GetSize()) return false;
				xOut = Zenith_AnimKeyValue::FromQuat(xRoot.m_xRotationDeltas.Get(uKeyIndex).first);
				return true;
			}
			if (xTrack.m_eTrack != FLUX_ANIM_TRACK_POSITION) return false;
			if (uKeyIndex >= xRoot.m_xPositionDeltas.GetSize()) return false;
			xOut = Zenith_AnimKeyValue::FromVector(xRoot.m_xPositionDeltas.Get(uKeyIndex).first);
			return true;
		}

		const Flux_BoneChannel* pxChannel = xClip.GetBoneChannel(xTrack.m_strBoneName);
		if (pxChannel == nullptr) return false;

		switch (xTrack.m_eTrack)
		{
		case FLUX_ANIM_TRACK_POSITION:
			if (uKeyIndex >= pxChannel->GetPositionKeyframes().GetSize()) return false;
			xOut = Zenith_AnimKeyValue::FromVector(pxChannel->GetPositionKeyframes().Get(uKeyIndex).first);
			return true;
		case FLUX_ANIM_TRACK_ROTATION:
			if (uKeyIndex >= pxChannel->GetRotationKeyframes().GetSize()) return false;
			xOut = Zenith_AnimKeyValue::FromQuat(pxChannel->GetRotationKeyframes().Get(uKeyIndex).first);
			return true;
		case FLUX_ANIM_TRACK_SCALE:
			if (uKeyIndex >= pxChannel->GetScaleKeyframes().GetSize()) return false;
			xOut = Zenith_AnimKeyValue::FromVector(pxChannel->GetScaleKeyframes().Get(uKeyIndex).first);
			return true;
		}
		return false;
	}

	//--------------------------------------------------------------------------
	// TANGENTS (WU-8.2).
	//
	// ★ ONLY A BONE CHANNEL HAS THEM. Flux_RootMotion carries no parallel
	// Flux_KeyTangents array (D17 declined to give it one, because that moves the
	// .zanim layout), so every one of these answers null / does nothing for a
	// root-motion track and the callers turn that into a plain refusal.
	//--------------------------------------------------------------------------
	const Zenith_Vector<Flux_KeyTangents>* ChannelTangents(const Flux_BoneChannel& xChannel, Flux_AnimTrack eTrack)
	{
		switch (eTrack)
		{
		case FLUX_ANIM_TRACK_POSITION: return &xChannel.GetPositionTangents();
		case FLUX_ANIM_TRACK_ROTATION: return &xChannel.GetRotationTangents();
		case FLUX_ANIM_TRACK_SCALE:    return &xChannel.GetScaleTangents();
		}
		return nullptr;
	}

	void ChannelSetTangent(Flux_BoneChannel& xChannel, Flux_AnimTrack eTrack, u_int uKeyIndex,
		const Flux_KeyTangents& xTangents)
	{
		switch (eTrack)
		{
		case FLUX_ANIM_TRACK_POSITION: xChannel.SetPositionTangent(uKeyIndex, xTangents); break;
		case FLUX_ANIM_TRACK_ROTATION: xChannel.SetRotationTangent(uKeyIndex, xTangents); break;
		case FLUX_ANIM_TRACK_SCALE:    xChannel.SetScaleTangent(uKeyIndex, xTangents);    break;
		}
	}

	// ★ A SECOND COPY OF Flux_AnimationClip.cpp's ROTATION-VECTOR HELPER, AND IT
	// IS A COPY BECAUSE THE ORIGINAL IS IN AN ANONYMOUS NAMESPACE. Exporting it
	// would edit a Flux header this unit does not own; reproducing it is eight
	// lines and the rule it encodes ("always the short way round") is stated at
	// both sites. If it ever gains a public home, delete this one.
	//
	// A unit quaternion to its axis * radians vector. q and -q are the same
	// rotation but only one has a non-negative scalar part, and taking the other
	// reports an angle past pi — a tangent pointing the long way round the sphere,
	// which the sampler would faithfully reproduce as a spin nobody authored.
	Zenith_Maths::Vector3 AnimDocRotationVectorFromQuat(const Zenith_Maths::Quat& xQuat)
	{
		const Zenith_Maths::Quat xShortest = (xQuat.w < 0.0f) ? -xQuat : xQuat;
		const Zenith_Maths::Vector3 xImaginary(xShortest.x, xShortest.y, xShortest.z);
		const float fSinHalfAngle = glm::length(xImaginary);
		// The same 1e-6 floor Flux_AnimationClip.cpp's fANIM_MIN_ROTATION_VECTOR
		// is: below it the axis is numerical noise and the honest answer is "no
		// rotation", which is the zero (linear) tangent.
		if (fSinHalfAngle < 1.0e-6f)
		{
			return Zenith_Maths::Vector3(0.0f);
		}
		// atan2 rather than 2*acos(w): acos loses every bit of precision it has as
		// w approaches 1, which is exactly the small-angle case a key-to-key delta is.
		const float fAngle = 2.0f * std::atan2(fSinHalfAngle, xShortest.w);
		return xImaginary * (fAngle / fSinHalfAngle);
	}

	// The ONE place the id->index accelerator is written, for both keys and
	// events. It copies the index->id authority wholesale, so the two cannot
	// disagree about anything except by not being called.
	void RebuildIdIndexMap(const Zenith_Vector<u_int>& auIdByIndex, Zenith_HashMap<u_int, u_int>& xIndexById)
	{
		xIndexById.Clear();
		for (u_int u = 0; u < auIdByIndex.GetSize(); ++u)
		{
			xIndexById.Insert(auIdByIndex.Get(u), u);
		}
	}

	bool EventsIdentical(const Flux_AnimationEvent& xA, const Flux_AnimationEvent& xB)
	{
		return xA.m_fNormalizedTime == xB.m_fNormalizedTime
			&& xA.m_strEventName == xB.m_strEventName
			&& xA.m_xData == xB.m_xData;
	}

	void EnsureParentDirectory(const std::string& strFilePath)
	{
		std::error_code xError;
		const std::filesystem::path xPath(strFilePath);
		const std::filesystem::path xParent = xPath.parent_path();
		if (!xParent.empty())
		{
			std::filesystem::create_directories(xParent, xError);
		}
	}

	std::string LeafNameOf(const std::string& strPath)
	{
		const size_t uSlash = strPath.find_last_of("/\\");
		return (uSlash == std::string::npos) ? strPath : strPath.substr(uSlash + 1);
	}
}

//==============================================================================
// Zenith_AnimTrackId / Zenith_AnimKeyValue
//==============================================================================

Zenith_AnimTrackId Zenith_AnimTrackId::Bone(const std::string& strBoneName, Flux_AnimTrack eTrack)
{
	Zenith_AnimTrackId xTrack;
	xTrack.m_strBoneName = strBoneName;
	xTrack.m_eTrack = eTrack;
	xTrack.m_bRootMotion = false;
	return xTrack;
}

Zenith_AnimTrackId Zenith_AnimTrackId::RootMotion(Flux_AnimTrack eTrack)
{
	Zenith_AnimTrackId xTrack;
	xTrack.m_eTrack = eTrack;
	xTrack.m_bRootMotion = true;
	return xTrack;
}

bool Zenith_AnimTrackId::operator==(const Zenith_AnimTrackId& xOther) const
{
	if (m_bRootMotion != xOther.m_bRootMotion || m_eTrack != xOther.m_eTrack)
	{
		return false;
	}
	// A root-motion track carries no bone name, so comparing one would make two
	// spellings of the same track compare unequal.
	return m_bRootMotion || m_strBoneName == xOther.m_strBoneName;
}

Zenith_AnimKeyValue Zenith_AnimKeyValue::FromVector(const Zenith_Maths::Vector3& xValue)
{
	Zenith_AnimKeyValue xOut;
	xOut.m_xVector = xValue;
	xOut.m_bIsRotation = false;
	return xOut;
}

Zenith_AnimKeyValue Zenith_AnimKeyValue::FromQuat(const Zenith_Maths::Quat& xRotation)
{
	Zenith_AnimKeyValue xOut;
	xOut.m_xQuat = xRotation;
	xOut.m_bIsRotation = true;
	return xOut;
}

//==============================================================================
// Lifetime
//==============================================================================

Zenith_AnimationDocument::~Zenith_AnimationDocument()
{
	// Same reasoning as ResetToClosed: an abandoned group owns commands, and they
	// go with it.
	delete m_pxOpenCompound;
	m_pxOpenCompound = nullptr;
	// ★ THE ONE LINE THAT MAKES THE COMMANDS' RAW DOCUMENT POINTER SAFE. Every
	// command this document pushed lives in this stack and nowhere else, and
	// Clear() deletes them all — so no command can survive the document it
	// addresses, whichever order the two would otherwise have been destroyed in.
	m_xUndoSystem.Clear();
}

void Zenith_AnimationDocument::ResetToClosed()
{
	// An abandoned group is DISCARDED, not pushed. Reaching here with one open
	// means an operation returned without closing its own bracket, which is a
	// defect in that operation — but leaking the group (and every command it
	// adopted) on top of it would turn a logic bug into a memory one.
	if (m_pxOpenCompound != nullptr)
	{
		Zenith_Assert(false, "Zenith_AnimationDocument: the document was reset with a compound still open");
		delete m_pxOpenCompound;
		m_pxOpenCompound = nullptr;
	}
	m_xUndoSystem.Clear();
	m_xWorkingClip = Flux_AnimationClip();
	m_xAsset.Clear();
	m_strAssetPath.clear();
	m_strResolvedPath.clear();
	m_xTrackIds.Clear();
	m_auEventIdByIndex.Clear();
	m_xEventIndexById.Clear();
	m_ulRecordedFileHash = 0;
	m_bHasRecordedFile = false;
	m_bOpen = false;
	m_bDirty = false;
	// m_uNextKeyId / m_uNextEventId are deliberately NOT reset — see the header.
}

Zenith_AnimationAsset* Zenith_AnimationDocument::GetAsset() const
{
	return m_bOpen ? m_xAsset.Resolve() : nullptr;
}

Zenith_AnimDocOpenResult Zenith_AnimationDocument::Open(const std::string& strAssetPath)
{
	if (m_bOpen && m_bDirty)
	{
		return ZENITH_ANIMDOC_OPEN_REFUSED_DIRTY;
	}
	if (strAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::Open: empty asset path");
		return ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strAssetPath);
	AnimationHandle xHandle = Zenith_AssetRegistry::Acquire<Zenith_AnimationAsset>(strNormalized);
	Zenith_AnimationAsset* pxAsset = xHandle.Resolve();
	if (pxAsset == nullptr || pxAsset->GetClip() == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimDoc] '%s' did not resolve to a loadable animation clip", strNormalized.c_str());
		return ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET;
	}

	// ★ D21. A generated clip is REWRITTEN IN FULL by the next tools boot, so an
	// edit made here would survive exactly until someone ran the game again —
	// the worst possible failure shape, because the editor would report success.
	if (pxAsset->GetClip()->GetMetadata().m_bGenerated)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR,
			"[AnimDoc] '%s' is a GENERATED clip and is read-only; promote it to an authored override to edit it",
			strNormalized.c_str());
		return ZENITH_ANIMDOC_OPEN_REFUSED_GENERATED;
	}

	ResetToClosed();

	m_xAsset = xHandle;
	m_strAssetPath = strNormalized;
	m_strResolvedPath = Zenith_AssetRegistry::ResolvePath(strNormalized);
	m_xWorkingClip = *pxAsset->GetClip();   // D20: the deep copy every edit lands in
	m_bOpen = true;
	m_bDirty = false;

	RebuildAllIdsFromWorkingClip();
	m_bHasRecordedFile = HashFileContents(m_strResolvedPath, m_ulRecordedFileHash);

	return ZENITH_ANIMDOC_OPEN_OK;
}

Zenith_AnimDocOpenResult Zenith_AnimationDocument::PromoteToAuthoredOverride(const std::string& strSourceAssetPath)
{
	if (m_bOpen && m_bDirty)
	{
		return ZENITH_ANIMDOC_OPEN_REFUSED_DIRTY;
	}

	const std::string strSourceNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strSourceAssetPath);
	AnimationHandle xSource = Zenith_AssetRegistry::Acquire<Zenith_AnimationAsset>(strSourceNormalized);
	Zenith_AnimationAsset* pxSource = xSource.Resolve();
	if (pxSource == nullptr || pxSource->GetClip() == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimDoc] cannot promote '%s': no clip", strSourceNormalized.c_str());
		return ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET;
	}

	const std::string strTargetFile = ResolveAuthoredOverridePath(strSourceNormalized);
	if (strTargetFile.empty())
	{
		Zenith_Error(LOG_CATEGORY_EDITOR,
			"[AnimDoc] cannot promote '%s': it is under neither 'engine:' nor 'game:', so there is no asset root to place Authored/ under",
			strSourceNormalized.c_str());
		return ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET;
	}

	// ★ THE SOURCE FILE IS NOT TOUCHED. The bake still owns it and still rewrites
	// it on every boot; the override is a SEPARATE asset that the editor (and
	// then whatever references it) reads instead.
	Flux_AnimationClip xPromoted = *pxSource->GetClip();
	xPromoted.GetMetadata().m_bGenerated = false;

	EnsureParentDirectory(strTargetFile);

	Zenith_DataStream xStream;
	xPromoted.WriteToDataStream(xStream);
	const u_int64 ulExpected = HashBytes(xStream.GetData(), xStream.GetCursor());
	xStream.WriteToFile(strTargetFile.c_str());

	u_int64 ulActual = 0;
	if (!HashFileContents(strTargetFile, ulActual) || ulActual != ulExpected)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimDoc] promotion wrote '%s' but the file does not read back as written", strTargetFile.c_str());
		return ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET;
	}

	// The asset path the document will hold. With a root override in force the
	// target is a plain filesystem path (a test sandbox); otherwise it is the
	// prefixed authored path, which is what a scene or a controller would name.
	const std::string strTargetAssetPath = m_strAuthoredRootOverride.empty()
		? BuildAuthoredAssetPath(strSourceNormalized)
		: strTargetFile;
	const std::string strTargetNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strTargetAssetPath);

	// A previous promotion may have left the registry holding the OLD bytes of
	// this path. Reload it in place rather than force-unloading, so anything
	// already borrowing that clip pointer keeps working (D26).
	if (Zenith_AssetRegistry::IsLoaded(strTargetNormalized))
	{
		Zenith_AnimationAsset* pxExisting = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(strTargetNormalized);
		if (pxExisting != nullptr)
		{
			pxExisting->ReloadFromDisk(strTargetFile);
		}
	}

	return Open(strTargetNormalized);
}

Zenith_AnimDocCloseResult Zenith_AnimationDocument::Close()
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMDOC_CLOSE_OK;
	}
	if (m_bDirty)
	{
		// ★ The document RECORDS the refusal and changes nothing. Raising the
		// prompt is the panel's job (WU-3.x); putting it here would make every
		// headless caller — the unit suite included — need a UI.
		return ZENITH_ANIMDOC_CLOSE_REFUSED_DIRTY;
	}
	ResetToClosed();
	return ZENITH_ANIMDOC_CLOSE_OK;
}

void Zenith_AnimationDocument::CloseDiscardingChanges()
{
	ResetToClosed();
}

bool Zenith_AnimationDocument::DiscardChanges()
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_AnimationAsset* pxAsset = GetAsset();
	if (pxAsset == nullptr || pxAsset->GetClip() == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimDoc] cannot discard: the live asset no longer holds a clip");
		return false;
	}

	m_xWorkingClip = *pxAsset->GetClip();
	// Every id the old working copy handed out is retired by this re-copy, so
	// every command on the stack addresses keys that no longer exist. Clearing
	// is not a policy choice here; a surviving command would silently no-op or,
	// worse, hit a recycled id — which is exactly why ids are never recycled.
	RebuildAllIdsFromWorkingClip();
	m_xUndoSystem.Clear();
	m_bDirty = false;
	return true;
}

//==============================================================================
// Saving
//==============================================================================

bool Zenith_AnimationDocument::WriteWorkingClipToFile(const std::string& strResolvedPath, u_int64& ulOutHash) const
{
	if (strResolvedPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimationDocument: refusing to write to an empty path");
		return false;
	}

	EnsureParentDirectory(strResolvedPath);

	Zenith_DataStream xStream;
	// WriteToDataStream leads with the shared stream envelope (D1), so this is
	// the same byte layout Flux_AnimationClip::Export produces and the same one
	// ParseStream demands.
	m_xWorkingClip.WriteToDataStream(xStream);
	const u_int64 ulExpected = HashBytes(xStream.GetData(), xStream.GetCursor());
	xStream.WriteToFile(strResolvedPath.c_str());

	// ★ VERIFY THE WRITE BY READING IT BACK. Zenith_FileAccess::WriteFile is
	// void, so without this a full disk or a read-only file would be reported to
	// the user as a successful save — and the document would clear its dirty
	// flag over work that never left memory.
	u_int64 ulActual = 0;
	if (!HashFileContents(strResolvedPath, ulActual) || ulActual != ulExpected)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimDoc] wrote '%s' but it does not read back as written", strResolvedPath.c_str());
		return false;
	}

	ulOutHash = ulActual;
	return true;
}

Zenith_AnimDocSaveResult Zenith_AnimationDocument::Save()
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMDOC_SAVE_FAILED_NO_DOCUMENT;
	}
	if (HasExternalModification())
	{
		// ★ NOTHING IS WRITTEN. A surfaced conflict, never a silent replace: the
		// other writer's changes are still on disk and still recoverable, which
		// they would not be if this had gone ahead.
		Zenith_Log(LOG_CATEGORY_EDITOR, "[AnimDoc] '%s' changed on disk since it was opened — save refused", m_strResolvedPath.c_str());
		return ZENITH_ANIMDOC_SAVE_CONFLICT_EXTERNAL;
	}
	return SaveOverwritingExternal();
}

Zenith_AnimDocSaveResult Zenith_AnimationDocument::SaveOverwritingExternal()
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMDOC_SAVE_FAILED_NO_DOCUMENT;
	}

	u_int64 ulHash = 0;
	if (!WriteWorkingClipToFile(m_strResolvedPath, ulHash))
	{
		return ZENITH_ANIMDOC_SAVE_FAILED_WRITE;
	}
	m_ulRecordedFileHash = ulHash;
	m_bHasRecordedFile = true;

	// ★ THE LIVE ASSET PICKS THE FILE UP IN PLACE (WU-2.1 / D26). Not a
	// force-unload plus a fresh acquire: a controller borrows the clip POINTER,
	// so moving the clip would leave it pointing at freed memory. ReloadFromDisk
	// replaces the CONTENTS of the clip the asset already owns.
	Zenith_AnimationAsset* pxAsset = GetAsset();
	if (pxAsset != nullptr && !pxAsset->ReloadFromDisk(m_strResolvedPath))
	{
		// The bytes ARE on disk (verified above) — this is the live clip failing
		// to adopt them, which is a real problem worth shouting about, but not a
		// failed save.
		Zenith_Error(LOG_CATEGORY_EDITOR,
			"[AnimDoc] saved '%s' but the live asset refused to reload it; the clip in memory is now STALE",
			m_strResolvedPath.c_str());
	}

	m_bDirty = false;
	// ★ UNDO HISTORY SURVIVES A SAVE, deliberately. Saving is not a semantic
	// boundary in the edit — undoing across one is an ordinary thing to want,
	// and it simply makes the document dirty again.
	return ZENITH_ANIMDOC_SAVE_OK;
}

Zenith_AnimDocSaveResult Zenith_AnimationDocument::SaveAs(const std::string& strNewAssetPath)
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMDOC_SAVE_FAILED_NO_DOCUMENT;
	}
	if (strNewAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::SaveAs: empty path");
		return ZENITH_ANIMDOC_SAVE_FAILED_WRITE;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strNewAssetPath);
	const std::string strResolved = Zenith_AssetRegistry::ResolvePath(strNormalized);

	u_int64 ulHash = 0;
	if (!WriteWorkingClipToFile(strResolved, ulHash))
	{
		return ZENITH_ANIMDOC_SAVE_FAILED_WRITE;
	}

	if (Zenith_AssetRegistry::IsLoaded(strNormalized))
	{
		Zenith_AnimationAsset* pxExisting = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(strNormalized);
		if (pxExisting != nullptr)
		{
			pxExisting->ReloadFromDisk(strResolved);
		}
	}

	AnimationHandle xHandle = Zenith_AssetRegistry::Acquire<Zenith_AnimationAsset>(strNormalized);
	if (xHandle.Resolve() == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimDoc] Save As wrote '%s' but it does not load back as an animation asset", strResolved.c_str());
		return ZENITH_ANIMDOC_SAVE_FAILED_WRITE;
	}

	m_xAsset = xHandle;
	m_strAssetPath = strNormalized;
	m_strResolvedPath = strResolved;
	m_ulRecordedFileHash = ulHash;
	m_bHasRecordedFile = true;
	m_bDirty = false;

	// ★ CLEARED, unlike Save. The stack describes edits made to a file this
	// document has just stopped pointing at; replaying them would write them
	// into the NEW file, which is not what "undo" means to anyone.
	//
	// The KEY IDS are NOT rebuilt — the working copy did not change — so a
	// selection held across a Save As still resolves.
	m_xUndoSystem.Clear();

	return ZENITH_ANIMDOC_SAVE_OK;
}

//==============================================================================
// External-modification detection
//==============================================================================

u_int64 Zenith_AnimationDocument::HashBytes(const void* pData, u_int64 ulSize)
{
	// FNV-1a 64. Not a security hash — it is a "did these bytes change" oracle,
	// and it is used in preference to a write TIME because a timestamp compares
	// two clocks written by two processes while a hash compares the thing the
	// question is actually about.
	u_int64 ulHash = 1469598103934665603ull;
	const uint8_t* puBytes = static_cast<const uint8_t*>(pData);
	for (u_int64 ul = 0; ul < ulSize; ++ul)
	{
		ulHash ^= static_cast<u_int64>(puBytes[ul]);
		ulHash *= 1099511628211ull;
	}
	return ulHash;
}

bool Zenith_AnimationDocument::HashFileContents(const std::string& strResolvedPath, u_int64& ulOutHash)
{
	if (strResolvedPath.empty() || !Zenith_FileAccess::FileExists(strResolvedPath.c_str()))
	{
		return false;
	}
	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(strResolvedPath.c_str(), ulSize);
	if (pData == nullptr)
	{
		return false;
	}
	ulOutHash = HashBytes(pData, ulSize);
	Zenith_FileAccess::FreeFileData(pData);
	return true;
}

bool Zenith_AnimationDocument::HasExternalModification() const
{
	if (!m_bOpen || !m_bHasRecordedFile)
	{
		return false;
	}
	u_int64 ulHash = 0;
	if (!HashFileContents(m_strResolvedPath, ulHash))
	{
		// The file we opened is gone or unreadable. That is a change, and the
		// user should be told before a save recreates it from a working copy
		// they may not have looked at in an hour.
		return true;
	}
	return ulHash != m_ulRecordedFileHash;
}

//==============================================================================
// Undo
//==============================================================================

void Zenith_AnimationDocument::PushCommand(Zenith_UndoCommand* pxCommand)
{
	if (pxCommand == nullptr)
	{
		return;
	}
	// ★ A GROUP INTERCEPTS THE PUSH, and this one branch is the whole of the
	// grouping mechanism as far as the verbs are concerned: they call PushCommand
	// exactly as they always did and never learn whether they were inside one.
	if (m_pxOpenCompound != nullptr)
	{
		m_pxOpenCompound->Adopt(pxCommand);
		return;
	}
	// Record, not Execute: the document has ALREADY performed the edit (it had
	// to — it needs the mutator's reported index to re-map the ids). Execute()
	// therefore only ever runs on a redo.
	m_xUndoSystem.Record(pxCommand);
}

bool Zenith_AnimationDocument::BeginCompound()
{
	if (!m_bOpen)
	{
		return false;
	}
	if (m_pxOpenCompound != nullptr)
	{
		// Refused rather than counted. A nested group would make "one undo step"
		// depend on which caller opened first, and every operation here is a leaf.
		Zenith_Assert(false, "Zenith_AnimationDocument::BeginCompound: a compound is already open");
		return false;
	}
	m_pxOpenCompound = new Zenith_AnimCommand_Compound(this, "Animation Edit");
	return true;
}

bool Zenith_AnimationDocument::EndCompound(const char* szDescription, bool bKeep)
{
	if (m_pxOpenCompound == nullptr)
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::EndCompound: no compound is open");
		return false;
	}

	// ★ CLEARED FIRST. Undo() below re-enters the commands, and any of them
	// reaching PushCommand while this pointer still pointed at the group being
	// discarded would adopt into a doomed object.
	Zenith_AnimCommand_Compound* pxCompound = m_pxOpenCompound;
	m_pxOpenCompound = nullptr;

	if (!bKeep)
	{
		// A refusal partway through a multi-key operation: everything already
		// applied is reversed, in reverse order, and the group is thrown away.
		pxCompound->Undo();
		delete pxCompound;
		return false;
	}

	if (pxCompound->GetChildCount() == 0)
	{
		// An operation that turned out to change nothing must not leave an undo
		// entry that reverses nothing — a Ctrl+Z that visibly does nothing is
		// indistinguishable from a broken undo.
		delete pxCompound;
		return false;
	}

	pxCompound->SetDescription(szDescription);
	m_xUndoSystem.Record(pxCompound);
	return true;
}

void Zenith_AnimationDocument::Undo()
{
	if (m_pxOpenCompound != nullptr)
	{
		// Undoing INTO a half-built group would pop a command the group has not
		// finished collecting around.
		Zenith_Assert(false, "Zenith_AnimationDocument::Undo: refused while a compound is open");
		return;
	}
	if (!m_xUndoSystem.CanUndo())
	{
		return;
	}
	m_xUndoSystem.Undo();
	// Undoing past a save makes the working copy differ from the file again.
	m_bDirty = true;
}

void Zenith_AnimationDocument::Redo()
{
	if (m_pxOpenCompound != nullptr)
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::Redo: refused while a compound is open");
		return;
	}
	if (!m_xUndoSystem.CanRedo())
	{
		return;
	}
	m_xUndoSystem.Redo();
	m_bDirty = true;
}

//==============================================================================
// Track / id bookkeeping
//==============================================================================

std::string Zenith_AnimationDocument::MakeTrackKey(const Zenith_AnimTrackId& xTrack)
{
	// FIXED-WIDTH prefix then the raw bone name, so a bone whose name contains
	// the separator cannot collide with another track.
	char acPrefix[4];
	acPrefix[0] = xTrack.m_bRootMotion ? 'r' : 'b';
	acPrefix[1] = static_cast<char>('0' + static_cast<int>(xTrack.m_eTrack));
	acPrefix[2] = ':';
	acPrefix[3] = '\0';
	std::string strKey(acPrefix);
	if (!xTrack.m_bRootMotion)
	{
		strKey += xTrack.m_strBoneName;
	}
	return strKey;
}

bool Zenith_AnimationDocument::IsTrackAddressable(const Zenith_AnimTrackId& xTrack)
{
	if (xTrack.m_eTrack != FLUX_ANIM_TRACK_POSITION
		&& xTrack.m_eTrack != FLUX_ANIM_TRACK_ROTATION
		&& xTrack.m_eTrack != FLUX_ANIM_TRACK_SCALE)
	{
		return false;
	}
	if (xTrack.m_bRootMotion)
	{
		// D16: root motion has a position and a rotation delta track and no third one.
		return xTrack.m_eTrack != FLUX_ANIM_TRACK_SCALE;
	}
	return !xTrack.m_strBoneName.empty();
}

Zenith_AnimationDocument::TrackIds& Zenith_AnimationDocument::GetOrAddTrackIds(const Zenith_AnimTrackId& xTrack)
{
	const std::string strKey = MakeTrackKey(xTrack);
	TrackIds* pxExisting = m_xTrackIds.TryGet(strKey);
	if (pxExisting != nullptr)
	{
		return *pxExisting;
	}
	return m_xTrackIds.Emplace(strKey);
}

const Zenith_AnimationDocument::TrackIds* Zenith_AnimationDocument::FindTrackIds(const Zenith_AnimTrackId& xTrack) const
{
	return m_xTrackIds.TryGet(MakeTrackKey(xTrack));
}

void Zenith_AnimationDocument::RebuildIndexById(TrackIds& xIds)
{
	RebuildIdIndexMap(xIds.m_auIdByIndex, xIds.m_xIndexById);
}

void Zenith_AnimationDocument::InsertIdAt(Zenith_Vector<u_int>& auIds, u_int uIndex, u_int uId)
{
	const u_int uClamped = (uIndex > auIds.GetSize()) ? auIds.GetSize() : uIndex;
	Zenith_Assert(uIndex <= auIds.GetSize(), "Zenith_AnimationDocument: id insert index %u past the end (%u)", uIndex, auIds.GetSize());

	auIds.PushBack(0u);
	for (u_int u = auIds.GetSize() - 1; u > uClamped; --u)
	{
		auIds.Get(u) = auIds.Get(u - 1);
	}
	auIds.Get(uClamped) = uId;
}

void Zenith_AnimationDocument::RebuildAllIdsFromWorkingClip()
{
	m_xTrackIds.Clear();
	m_auEventIdByIndex.Clear();
	m_xEventIndexById.Clear();

	const Flux_AnimTrack aeTracks[3] = { FLUX_ANIM_TRACK_POSITION, FLUX_ANIM_TRACK_ROTATION, FLUX_ANIM_TRACK_SCALE };

	Zenith_Vector<std::string> axBoneNames;
	GetBoneNamesSorted(axBoneNames);
	for (u_int uBone = 0; uBone < axBoneNames.GetSize(); ++uBone)
	{
		for (u_int uTrack = 0; uTrack < 3; ++uTrack)
		{
			const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone(axBoneNames.Get(uBone), aeTracks[uTrack]);
			const u_int uCount = ClipTrackKeyCount(m_xWorkingClip, xTrack);
			if (uCount == 0)
			{
				continue;
			}
			TrackIds& xIds = GetOrAddTrackIds(xTrack);
			for (u_int u = 0; u < uCount; ++u)
			{
				xIds.m_auIdByIndex.PushBack(AllocateKeyId());
			}
			RebuildIndexById(xIds);
		}
	}

	for (u_int uTrack = 0; uTrack < 2; ++uTrack)
	{
		const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::RootMotion(aeTracks[uTrack]);
		const u_int uCount = ClipTrackKeyCount(m_xWorkingClip, xTrack);
		if (uCount == 0)
		{
			continue;
		}
		TrackIds& xIds = GetOrAddTrackIds(xTrack);
		for (u_int u = 0; u < uCount; ++u)
		{
			xIds.m_auIdByIndex.PushBack(AllocateKeyId());
		}
		RebuildIndexById(xIds);
	}

	for (u_int u = 0; u < m_xWorkingClip.GetEvents().GetSize(); ++u)
	{
		m_auEventIdByIndex.PushBack(AllocateEventId());
	}
	RebuildIdIndexMap(m_auEventIdByIndex, m_xEventIndexById);
}

//==============================================================================
// Inspection
//==============================================================================

void Zenith_AnimationDocument::GetBoneNamesSorted(Zenith_Vector<std::string>& axOut) const
{
	axOut.Clear();
	const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = m_xWorkingClip.GetBoneChannels();
	axOut.Reserve(xChannels.GetSize());
	for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels); !xIt.Done(); xIt.Next())
	{
		axOut.PushBack(xIt.GetKey());
	}
	std::sort(axOut.begin(), axOut.end());
}

bool Zenith_AnimationDocument::TrackExists(const Zenith_AnimTrackId& xTrack) const
{
	if (!IsTrackAddressable(xTrack))
	{
		return false;
	}
	return xTrack.m_bRootMotion || m_xWorkingClip.HasBoneChannel(xTrack.m_strBoneName);
}

u_int Zenith_AnimationDocument::GetKeyCount(const Zenith_AnimTrackId& xTrack) const
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		return 0;
	}
	return ClipTrackKeyCount(m_xWorkingClip, xTrack);
}

u_int Zenith_AnimationDocument::GetKeyIdAtIndex(const Zenith_AnimTrackId& xTrack, u_int uIndex) const
{
	const TrackIds* pxIds = FindTrackIds(xTrack);
	if (pxIds == nullptr || uIndex >= pxIds->m_auIdByIndex.GetSize())
	{
		return uINVALID_ANIM_KEY_ID;
	}
	return pxIds->m_auIdByIndex.Get(uIndex);
}

u_int Zenith_AnimationDocument::GetKeyIndexForId(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const
{
	if (uKeyId == uINVALID_ANIM_KEY_ID)
	{
		return uINVALID_ANIM_KEY_INDEX;
	}
	const TrackIds* pxIds = FindTrackIds(xTrack);
	if (pxIds == nullptr)
	{
		return uINVALID_ANIM_KEY_INDEX;
	}
	const u_int* puIndex = pxIds->m_xIndexById.TryGet(uKeyId);
	return puIndex != nullptr ? *puIndex : uINVALID_ANIM_KEY_INDEX;
}

u_int Zenith_AnimationDocument::FindKeyIdAtTime(const Zenith_AnimTrackId& xTrack, float fTimeSeconds) const
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		return uINVALID_ANIM_KEY_ID;
	}
	const u_int uCount = ClipTrackKeyCount(m_xWorkingClip, xTrack);
	if (uCount == 0)
	{
		return uINVALID_ANIM_KEY_ID;
	}

	u_int uIndex = uCount;
	if (xTrack.m_bRootMotion)
	{
		uIndex = m_xWorkingClip.GetRootMotion().FindKeyframeAtTime(xTrack.m_eTrack, fTimeSeconds);
	}
	else
	{
		const Flux_BoneChannel* pxChannel = m_xWorkingClip.GetBoneChannel(xTrack.m_strBoneName);
		if (pxChannel != nullptr)
		{
			uIndex = pxChannel->FindKeyframeAtTime(xTrack.m_eTrack, fTimeSeconds);
		}
	}
	return (uIndex < uCount) ? GetKeyIdAtIndex(xTrack, uIndex) : uINVALID_ANIM_KEY_ID;
}

bool Zenith_AnimationDocument::GetKeyTime(const Zenith_AnimTrackId& xTrack, u_int uKeyId, float& fOutTimeSeconds) const
{
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}
	return ClipTrackKeyTime(m_xWorkingClip, xTrack, uIndex, fOutTimeSeconds);
}

bool Zenith_AnimationDocument::GetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, Zenith_AnimKeyValue& xOut) const
{
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}
	return ClipTrackKeyValue(m_xWorkingClip, xTrack, uIndex, xOut);
}

//==============================================================================
// Non-recording mutation primitives (the undo commands' entry points)
//==============================================================================

u_int Zenith_AnimationDocument::ApplyInsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds,
	const Zenith_AnimKeyValue& xValue, u_int uForcedKeyId)
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::ApplyInsertKey: no document, or a track that cannot be addressed");
		return uINVALID_ANIM_KEY_ID;
	}
	if (xValue.m_bIsRotation != (xTrack.m_eTrack == FLUX_ANIM_TRACK_ROTATION))
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::ApplyInsertKey: a rotation value belongs only on a rotation track");
		return uINVALID_ANIM_KEY_ID;
	}

	const AnimTrackAccess xAccess = ResolveTrack(m_xWorkingClip, xTrack, true);
	if (!xAccess.IsValid())
	{
		return uINVALID_ANIM_KEY_ID;
	}

	// Whether the slot is occupied has to be sampled BEFORE the insert: after
	// it, a replace and an insert look identical from the outside.
	const u_int uCountBefore = xAccess.GetKeyframeCount();
	const bool bOccupied = xAccess.FindKeyframeAtTime(fTimeSeconds) < uCountBefore;

	u_int uNewIndex = 0;
	const bool bOk = xValue.m_bIsRotation
		? xAccess.InsertKeyframeAt(fTimeSeconds, xValue.m_xQuat, &uNewIndex)
		: xAccess.InsertKeyframeAt(fTimeSeconds, xValue.m_xVector, &uNewIndex);
	if (!bOk)
	{
		// GetOrAddBoneChannel may have just created an EMPTY channel, which is
		// the one state D14 forbids inside a clip.
		if (!xTrack.m_bRootMotion)
		{
			m_xWorkingClip.PruneEmptyChannel(xTrack.m_strBoneName);
		}
		return uINVALID_ANIM_KEY_ID;
	}

	TrackIds& xIds = GetOrAddTrackIds(xTrack);

	if (bOccupied)
	{
		// ★ D25. The mutator replaced the value in the existing slot and kept its
		// stored time, so the key is the SAME KEY and keeps its id. Handing out a
		// new one here would strand any selection holding the old one.
		const u_int uExistingId = (uNewIndex < xIds.m_auIdByIndex.GetSize()) ? xIds.m_auIdByIndex.Get(uNewIndex) : uINVALID_ANIM_KEY_ID;
		Zenith_Assert(uForcedKeyId == uINVALID_ANIM_KEY_ID || uForcedKeyId == uExistingId,
			"Zenith_AnimationDocument: an undo tried to re-insert key %u onto a slot already held by key %u", uForcedKeyId, uExistingId);
		return uExistingId;
	}

	const u_int uKeyId = (uForcedKeyId != uINVALID_ANIM_KEY_ID) ? uForcedKeyId : AllocateKeyId();
	InsertIdAt(xIds.m_auIdByIndex, uNewIndex, uKeyId);
	RebuildIndexById(xIds);
	return uKeyId;
}

bool Zenith_AnimationDocument::ApplyRemoveKey(const Zenith_AnimTrackId& xTrack, u_int uKeyId)
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}

	// The CLIP-level remove for a bone (D14): it drops the channel when that was
	// its last key, so "this bone is not animated" keeps exactly one
	// representation. Root motion has no channel to prune.
	const bool bOk = xTrack.m_bRootMotion
		? m_xWorkingClip.GetRootMotion().RemoveKeyframe(xTrack.m_eTrack, uIndex)
		: m_xWorkingClip.RemoveKeyframe(xTrack.m_strBoneName, xTrack.m_eTrack, uIndex);
	if (!bOk)
	{
		return false;
	}

	TrackIds& xIds = GetOrAddTrackIds(xTrack);
	if (uIndex < xIds.m_auIdByIndex.GetSize())
	{
		xIds.m_auIdByIndex.Remove(uIndex);
	}
	RebuildIndexById(xIds);
	return true;
}

bool Zenith_AnimationDocument::ApplySetKeyTime(const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fNewTimeSeconds)
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}

	const AnimTrackAccess xAccess = ResolveTrack(m_xWorkingClip, xTrack, false);
	if (!xAccess.IsValid())
	{
		return false;
	}

	u_int uNewIndex = uIndex;
	if (!xAccess.SetKeyframeTime(uIndex, fNewTimeSeconds, &uNewIndex))
	{
		// Refused — another key is already within fANIM_TIME_EPSILON. Nothing
		// changed, so nothing to re-map (D11: no silent merge).
		return false;
	}

	// ★ THE ID MOVES WITH THE KEY. SetKeyframeTime is an erase at the old index
	// followed by an insert at the new SORTED index, and doing exactly that to
	// the id vector reproduces the permutation for every direction of move —
	// which is the whole reason an index is not an identity here.
	TrackIds& xIds = GetOrAddTrackIds(xTrack);
	if (uIndex < xIds.m_auIdByIndex.GetSize())
	{
		const u_int uMovedId = xIds.m_auIdByIndex.Get(uIndex);
		xIds.m_auIdByIndex.Remove(uIndex);
		InsertIdAt(xIds.m_auIdByIndex, uNewIndex, uMovedId);
	}
	RebuildIndexById(xIds);
	return true;
}

bool Zenith_AnimationDocument::ApplySetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_AnimKeyValue& xValue)
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	if (xValue.m_bIsRotation != (xTrack.m_eTrack == FLUX_ANIM_TRACK_ROTATION))
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::ApplySetKeyValue: a rotation value belongs only on a rotation track");
		return false;
	}
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}

	const AnimTrackAccess xAccess = ResolveTrack(m_xWorkingClip, xTrack, false);
	if (!xAccess.IsValid())
	{
		return false;
	}

	// A value edit keeps the slot, the time AND the tangent, so no id moves.
	return xValue.m_bIsRotation
		? xAccess.SetKeyframeValue(uIndex, xValue.m_xQuat)
		: xAccess.SetKeyframeValue(uIndex, xValue.m_xVector);
}

//------------------------------------------------------------------------------
// Tangents (WU-8.2) — the primitive plus its three private helpers.
//------------------------------------------------------------------------------

bool Zenith_AnimationDocument::TangentsEqual(const Flux_KeyTangents& xA, const Flux_KeyTangents& xB)
{
	return xA.m_xInTangent  == xB.m_xInTangent
	    && xA.m_xOutTangent == xB.m_xOutTangent;
}

bool Zenith_AnimationDocument::ReadKeyTangentsAtIndex(const Zenith_AnimTrackId& xTrack, u_int uKeyIndex,
	Flux_KeyTangents& xOut) const
{
	// Root motion has no tangent array at all, which is a refusal rather than an
	// empty answer: "this track's tangents are all zero" and "this track cannot
	// hold a tangent" are different facts and a curve editor must not draw a
	// handle for the second.
	if (xTrack.m_bRootMotion || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	const Flux_BoneChannel* pxChannel = m_xWorkingClip.GetBoneChannel(xTrack.m_strBoneName);
	if (pxChannel == nullptr)
	{
		return false;
	}
	const Zenith_Vector<Flux_KeyTangents>* pxTangents = ChannelTangents(*pxChannel, xTrack.m_eTrack);
	if (pxTangents == nullptr || uKeyIndex >= pxTangents->GetSize())
	{
		return false;
	}
	xOut = pxTangents->Get(uKeyIndex);
	return true;
}

bool Zenith_AnimationDocument::ComputeAutoTangentForKey(const Zenith_AnimTrackId& xTrack, u_int uKeyIndex,
	Flux_KeyTangents& xOut) const
{
	if (xTrack.m_bRootMotion || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	const Flux_BoneChannel* pxChannel = m_xWorkingClip.GetBoneChannel(xTrack.m_strBoneName);
	if (pxChannel == nullptr)
	{
		return false;
	}

	// ★ IN == OUT, which is what makes "Auto" a SMOOTH key rather than a corner.
	// A broken pair is what a hand drag produces; the preset's whole job is the
	// unbroken one.
	xOut = Flux_KeyTangents();

	if (xTrack.m_eTrack == FLUX_ANIM_TRACK_ROTATION)
	{
		const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys = pxChannel->GetRotationKeyframes();
		const u_int uCount = xKeys.GetSize();
		if (uKeyIndex >= uCount)
		{
			return false;
		}
		if (uCount < 2u)
		{
			// One key: no span to divide by, so zero — the LINEAR tangent, which
			// is also the only honest answer to "what slope?".
			return true;
		}
		const u_int uPrev = (uKeyIndex == 0u) ? 0u : (uKeyIndex - 1u);
		const u_int uNext = ((uKeyIndex + 1u) >= uCount) ? (uCount - 1u) : (uKeyIndex + 1u);
		const float fSpan = xKeys.Get(uNext).second - xKeys.Get(uPrev).second;
		if (fSpan <= 0.0f)
		{
			return true;
		}

		const Zenith_Maths::Quat xPrev = glm::normalize(xKeys.Get(uPrev).first);
		Zenith_Maths::Quat xNext = glm::normalize(xKeys.Get(uNext).first);
		if (glm::dot(xPrev, xNext) < 0.0f)
		{
			// SHORTEST ARC. A pair straddling the 180-degree seam otherwise
			// measures a velocity going the long way round, which the sampler
			// reproduces as an unauthored extra spin.
			xNext = -xNext;
		}
		const Zenith_Maths::Vector3 xInPrevFrame =
			AnimDocRotationVectorFromQuat(glm::inverse(xPrev) * xNext) / fSpan;

		// ★ INTO THIS KEY'S OWN BODY FRAME. q_prev^-1 * q_next comes out in the
		// PREVIOUS key's frame and the sampler reads key k's tangent in key k's,
		// so the result is carried across by q_k^-1 * q_prev.
		const Zenith_Maths::Quat xThis = glm::normalize(xKeys.Get(uKeyIndex).first);
		xOut.m_xInTangent = (glm::inverse(xThis) * xPrev) * xInPrevFrame;
		xOut.m_xOutTangent = xOut.m_xInTangent;
		return true;
	}

	const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xKeys =
		(xTrack.m_eTrack == FLUX_ANIM_TRACK_SCALE) ? pxChannel->GetScaleKeyframes()
		                                           : pxChannel->GetPositionKeyframes();
	const u_int uCount = xKeys.GetSize();
	if (uKeyIndex >= uCount)
	{
		return false;
	}
	if (uCount < 2u)
	{
		return true;
	}
	const u_int uPrev = (uKeyIndex == 0u) ? 0u : (uKeyIndex - 1u);
	const u_int uNext = ((uKeyIndex + 1u) >= uCount) ? (uCount - 1u) : (uKeyIndex + 1u);
	const float fSpan = xKeys.Get(uNext).second - xKeys.Get(uPrev).second;
	// NOT an epsilon compare: this divides by the span, and a span at or below
	// zero is the only value that cannot be divided by.
	if (fSpan <= 0.0f)
	{
		return true;
	}
	xOut.m_xInTangent = (xKeys.Get(uNext).first - xKeys.Get(uPrev).first) / fSpan;
	xOut.m_xOutTangent = xOut.m_xInTangent;
	return true;
}

bool Zenith_AnimationDocument::ApplySetKeyTangents(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	const Flux_KeyTangents& xTangents)
{
	if (!m_bOpen || xTrack.m_bRootMotion || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}
	Flux_BoneChannel* pxChannel = m_xWorkingClip.GetBoneChannelMutable(xTrack.m_strBoneName);
	if (pxChannel == nullptr)
	{
		return false;
	}
	const Zenith_Vector<Flux_KeyTangents>* pxTangents = ChannelTangents(*pxChannel, xTrack.m_eTrack);
	if (pxTangents == nullptr || uIndex >= pxTangents->GetSize())
	{
		return false;
	}
	// A tangent edit touches neither the time nor the value, so no index moves and
	// no id map has to be rebuilt — which is why this primitive is the shortest
	// one in the file.
	ChannelSetTangent(*pxChannel, xTrack.m_eTrack, uIndex, xTangents);
	return true;
}

bool Zenith_AnimationDocument::ApplySetDuration(float fDurationSeconds)
{
	if (!m_bOpen)
	{
		return false;
	}
	// `!(x >= 0)` rather than `x < 0` so a NaN is refused rather than accepted.
	if (!(fDurationSeconds >= 0.0f))
	{
		Zenith_Assert(false, "Zenith_AnimationDocument::ApplySetDuration: %f is not a usable clip duration", fDurationSeconds);
		return false;
	}
	m_xWorkingClip.SetDuration(fDurationSeconds);
	return true;
}

//==============================================================================
// Events
//==============================================================================

void Zenith_AnimationDocument::ReadEventRecords(Zenith_Vector<EventRecord>& axOut) const
{
	axOut.Clear();
	const Zenith_Vector<Flux_AnimationEvent>& axEvents = m_xWorkingClip.GetEvents();
	Zenith_Assert(axEvents.GetSize() == m_auEventIdByIndex.GetSize(),
		"Zenith_AnimationDocument: %u events but %u event ids — the id map was not maintained",
		axEvents.GetSize(), m_auEventIdByIndex.GetSize());

	axOut.Reserve(axEvents.GetSize());
	for (u_int u = 0; u < axEvents.GetSize(); ++u)
	{
		EventRecord xRecord;
		xRecord.m_xEvent = axEvents.Get(u);
		xRecord.m_uId = (u < m_auEventIdByIndex.GetSize()) ? m_auEventIdByIndex.Get(u) : uINVALID_ANIM_KEY_ID;
		axOut.PushBack(xRecord);
	}
}

void Zenith_AnimationDocument::WriteEventRecords(const Zenith_Vector<EventRecord>& axRecords)
{
	// ★ THE WHOLE LIST IS REWRITTEN, AND THE IDS ARE RE-DERIVED BY MATCHING.
	//
	// Flux_AnimationClip::AddEvent std::sorts the list on every add, and
	// std::sort is not stable — so for a list with equal event times the order
	// that comes back is not, in general, the order that went in. Assuming it
	// was would put the id vector one slot out of step with the events, which is
	// invisible until a panel deletes the wrong row. So: write them in the order
	// we want, then read back what actually landed and match each stored event
	// to its record. Two events that compare identical are interchangeable by
	// construction, so a greedy first-match is exact rather than merely close.
	Zenith_Vector<EventRecord> axSorted;
	axSorted.Reserve(axRecords.GetSize());
	for (u_int u = 0; u < axRecords.GetSize(); ++u)
	{
		axSorted.PushBack(axRecords.Get(u));
	}
	std::stable_sort(axSorted.begin(), axSorted.end(),
		[](const EventRecord& xA, const EventRecord& xB) { return xA.m_xEvent.m_fNormalizedTime < xB.m_xEvent.m_fNormalizedTime; });

	for (u_int u = m_xWorkingClip.GetEvents().GetSize(); u-- > 0;)
	{
		m_xWorkingClip.RemoveEvent(u);
	}
	for (u_int u = 0; u < axSorted.GetSize(); ++u)
	{
		m_xWorkingClip.AddEvent(axSorted.Get(u).m_xEvent);
	}

	Zenith_Vector<bool> abConsumed;
	abConsumed.Resize(axSorted.GetSize(), false);

	const Zenith_Vector<Flux_AnimationEvent>& axStored = m_xWorkingClip.GetEvents();
	m_auEventIdByIndex.Clear();
	m_auEventIdByIndex.Reserve(axStored.GetSize());
	for (u_int u = 0; u < axStored.GetSize(); ++u)
	{
		u_int uMatch = axSorted.GetSize();
		for (u_int v = 0; v < axSorted.GetSize(); ++v)
		{
			if (!abConsumed.Get(v) && EventsIdentical(axSorted.Get(v).m_xEvent, axStored.Get(u)))
			{
				uMatch = v;
				break;
			}
		}
		if (uMatch < axSorted.GetSize())
		{
			abConsumed.Get(uMatch) = true;
			m_auEventIdByIndex.PushBack(axSorted.Get(uMatch).m_uId);
		}
		else
		{
			Zenith_Assert(false, "Zenith_AnimationDocument: an event came back from the clip that no record describes");
			m_auEventIdByIndex.PushBack(AllocateEventId());
		}
	}
	RebuildIdIndexMap(m_auEventIdByIndex, m_xEventIndexById);
}

u_int Zenith_AnimationDocument::GetEventCount() const
{
	return m_xWorkingClip.GetEvents().GetSize();
}

u_int Zenith_AnimationDocument::GetEventIdAtIndex(u_int uIndex) const
{
	return (uIndex < m_auEventIdByIndex.GetSize()) ? m_auEventIdByIndex.Get(uIndex) : uINVALID_ANIM_KEY_ID;
}

u_int Zenith_AnimationDocument::GetEventIndexForId(u_int uEventId) const
{
	if (uEventId == uINVALID_ANIM_KEY_ID)
	{
		return uINVALID_ANIM_KEY_INDEX;
	}
	const u_int* puIndex = m_xEventIndexById.TryGet(uEventId);
	return puIndex != nullptr ? *puIndex : uINVALID_ANIM_KEY_INDEX;
}

bool Zenith_AnimationDocument::GetEvent(u_int uEventId, Flux_AnimationEvent& xOut) const
{
	const u_int uIndex = GetEventIndexForId(uEventId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX || uIndex >= m_xWorkingClip.GetEvents().GetSize())
	{
		return false;
	}
	xOut = m_xWorkingClip.GetEvents().Get(uIndex);
	return true;
}

u_int Zenith_AnimationDocument::ApplyAddEvent(const Flux_AnimationEvent& xEvent, u_int uForcedEventId)
{
	if (!m_bOpen)
	{
		return uINVALID_ANIM_KEY_ID;
	}
	Zenith_Vector<EventRecord> axRecords;
	ReadEventRecords(axRecords);

	EventRecord xNew;
	xNew.m_xEvent = xEvent;
	xNew.m_uId = (uForcedEventId != uINVALID_ANIM_KEY_ID) ? uForcedEventId : AllocateEventId();
	axRecords.PushBack(xNew);

	WriteEventRecords(axRecords);
	return xNew.m_uId;
}

bool Zenith_AnimationDocument::ApplyRemoveEvent(u_int uEventId)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_Vector<EventRecord> axRecords;
	ReadEventRecords(axRecords);

	u_int uFound = axRecords.GetSize();
	for (u_int u = 0; u < axRecords.GetSize(); ++u)
	{
		if (axRecords.Get(u).m_uId == uEventId)
		{
			uFound = u;
			break;
		}
	}
	if (uFound == axRecords.GetSize())
	{
		return false;
	}
	axRecords.Remove(uFound);
	WriteEventRecords(axRecords);
	return true;
}

bool Zenith_AnimationDocument::ApplySetEvent(u_int uEventId, const Flux_AnimationEvent& xEvent)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_Vector<EventRecord> axRecords;
	ReadEventRecords(axRecords);

	bool bFound = false;
	for (u_int u = 0; u < axRecords.GetSize(); ++u)
	{
		if (axRecords.Get(u).m_uId == uEventId)
		{
			axRecords.Get(u).m_xEvent = xEvent;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		return false;
	}
	WriteEventRecords(axRecords);
	return true;
}

//==============================================================================
// Public verbs — each is Apply* + MarkDirty + exactly one recorded command.
//==============================================================================

u_int Zenith_AnimationDocument::InsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_AnimKeyValue& xValue)
{
	if (!m_bOpen || !IsTrackAddressable(xTrack))
	{
		return uINVALID_ANIM_KEY_ID;
	}

	// ★ D25 decides WHICH COMMAND this is, not just what the mutator does. On an
	// occupied slot the existing key keeps its id and only its value changes, so
	// the undo has to restore that value — an insert command's undo would delete
	// a key the user never created.
	const u_int uExistingId = FindKeyIdAtTime(xTrack, fTimeSeconds);
	if (uExistingId != uINVALID_ANIM_KEY_ID)
	{
		Zenith_AnimKeyValue xBefore;
		if (!GetKeyValue(xTrack, uExistingId, xBefore))
		{
			return uINVALID_ANIM_KEY_ID;
		}
		if (!ApplySetKeyValue(xTrack, uExistingId, xValue))
		{
			return uINVALID_ANIM_KEY_ID;
		}
		MarkDirty();
		PushCommand(new Zenith_AnimCommand_KeyValue(this, xTrack, uExistingId, xBefore, xValue, "Replace Keyframe"));
		return uExistingId;
	}

	const u_int uKeyId = ApplyInsertKey(xTrack, fTimeSeconds, xValue, uINVALID_ANIM_KEY_ID);
	if (uKeyId == uINVALID_ANIM_KEY_ID)
	{
		return uINVALID_ANIM_KEY_ID;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_KeyInsert(this, xTrack, uKeyId, fTimeSeconds, xValue));
	return uKeyId;
}

u_int Zenith_AnimationDocument::InsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_Maths::Vector3& xValue)
{
	return InsertKey(xTrack, fTimeSeconds, Zenith_AnimKeyValue::FromVector(xValue));
}

u_int Zenith_AnimationDocument::InsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_Maths::Quat& xRotation)
{
	return InsertKey(xTrack, fTimeSeconds, Zenith_AnimKeyValue::FromQuat(xRotation));
}

bool Zenith_AnimationDocument::RemoveKey(const Zenith_AnimTrackId& xTrack, u_int uKeyId)
{
	if (!m_bOpen)
	{
		return false;
	}
	// Capture BEFORE the removal: the undo has to restore the time and the value
	// as well as the id, and neither is readable afterwards.
	float fTimeSeconds = 0.0f;
	Zenith_AnimKeyValue xValue;
	if (!GetKeyTime(xTrack, uKeyId, fTimeSeconds) || !GetKeyValue(xTrack, uKeyId, xValue))
	{
		return false;
	}
	if (!ApplyRemoveKey(xTrack, uKeyId))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_KeyRemove(this, xTrack, uKeyId, fTimeSeconds, xValue));
	return true;
}

bool Zenith_AnimationDocument::SetKeyTime(const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fNewTimeSeconds)
{
	if (!m_bOpen)
	{
		return false;
	}
	float fOldTimeSeconds = 0.0f;
	if (!GetKeyTime(xTrack, uKeyId, fOldTimeSeconds))
	{
		return false;
	}
	if (!ApplySetKeyTime(xTrack, uKeyId, fNewTimeSeconds))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_KeyTime(this, xTrack, uKeyId, fOldTimeSeconds, fNewTimeSeconds));
	return true;
}

bool Zenith_AnimationDocument::SetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_AnimKeyValue& xValue)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_AnimKeyValue xBefore;
	if (!GetKeyValue(xTrack, uKeyId, xBefore))
	{
		return false;
	}
	if (!ApplySetKeyValue(xTrack, uKeyId, xValue))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_KeyValue(this, xTrack, uKeyId, xBefore, xValue, "Edit Keyframe Value"));
	return true;
}

bool Zenith_AnimationDocument::SetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_Maths::Vector3& xValue)
{
	return SetKeyValue(xTrack, uKeyId, Zenith_AnimKeyValue::FromVector(xValue));
}

bool Zenith_AnimationDocument::SetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_Maths::Quat& xRotation)
{
	return SetKeyValue(xTrack, uKeyId, Zenith_AnimKeyValue::FromQuat(xRotation));
}

//------------------------------------------------------------------------------
// Tangent verbs (WU-8.2). ASSIGNMENTS: true means the value asked for is in
// place, and a call that changed nothing pushes nothing.
//------------------------------------------------------------------------------

bool Zenith_AnimationDocument::GetKeyTangents(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	Flux_KeyTangents& xOut) const
{
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}
	return ReadKeyTangentsAtIndex(xTrack, uIndex, xOut);
}

bool Zenith_AnimationDocument::SetKeyTangents(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	const Flux_KeyTangents& xTangents)
{
	if (!m_bOpen)
	{
		return false;
	}
	Flux_KeyTangents xBefore;
	if (!GetKeyTangents(xTrack, uKeyId, xBefore))
	{
		// The key does not resolve, or the track is root motion. Either way this is
		// a genuine refusal and not a satisfied assignment.
		return false;
	}
	if (TangentsEqual(xBefore, xTangents))
	{
		// ★ SATISFIED, NOT REFUSED — and it pushes NOTHING. A recipe that re-states
		// a tangent it already set is ordinary; making that read as failure is what
		// trips a checked automation wrapper on a step that did exactly what it was
		// asked. The undo-stack depth is the invariant, not the bool.
		return true;
	}
	if (!ApplySetKeyTangents(xTrack, uKeyId, xTangents))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_KeyTangents(this, xTrack, uKeyId, xBefore, xTangents, "Edit Key Tangents"));
	return true;
}

bool Zenith_AnimationDocument::SetKeyInTangent(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	const Zenith_Maths::Vector3& xInTangent)
{
	Flux_KeyTangents xTangents;
	if (!GetKeyTangents(xTrack, uKeyId, xTangents))
	{
		return false;
	}
	xTangents.m_xInTangent = xInTangent;
	return SetKeyTangents(xTrack, uKeyId, xTangents);
}

bool Zenith_AnimationDocument::SetKeyOutTangent(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	const Zenith_Maths::Vector3& xOutTangent)
{
	Flux_KeyTangents xTangents;
	if (!GetKeyTangents(xTrack, uKeyId, xTangents))
	{
		return false;
	}
	xTangents.m_xOutTangent = xOutTangent;
	return SetKeyTangents(xTrack, uKeyId, xTangents);
}

bool Zenith_AnimationDocument::SetKeyTangentsAuto(const Zenith_AnimTrackId& xTrack, u_int uKeyId)
{
	if (!m_bOpen)
	{
		return false;
	}
	const u_int uIndex = GetKeyIndexForId(xTrack, uKeyId);
	if (uIndex == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}
	Flux_KeyTangents xAuto;
	if (!ComputeAutoTangentForKey(xTrack, uIndex, xAuto))
	{
		return false;
	}
	// Through the ordinary verb, so the "already in place" case and the command
	// shape are decided in exactly one place.
	return SetKeyTangents(xTrack, uKeyId, xAuto);
}

bool Zenith_AnimationDocument::ApplyTrackTangentPreset(const Zenith_AnimTrackId& xTrack, bool bAuto,
	const char* szDescription)
{
	if (!m_bOpen || xTrack.m_bRootMotion || !IsTrackAddressable(xTrack))
	{
		return false;
	}
	Flux_BoneChannel* pxChannel = m_xWorkingClip.GetBoneChannelMutable(xTrack.m_strBoneName);
	if (pxChannel == nullptr)
	{
		return false;
	}
	const Zenith_Vector<Flux_KeyTangents>* pxTangents = ChannelTangents(*pxChannel, xTrack.m_eTrack);
	if (pxTangents == nullptr)
	{
		return false;
	}

	// ★ CAPTURE EVERY KEY'S PAIR FIRST, BY STABLE ID. The preset is a whole-track
	// rewrite performed by the channel, so the only way an undo can put the track
	// back EXACTLY — including keys the preset happened not to move, and including
	// a hand-authored pair the preset overwrote — is to have recorded each one
	// before it ran. Ids, not indices: nothing reorders here, but a command that
	// stored an index would be the one command on this stack that could go stale
	// under a later retime (D24).
	const u_int uCount = pxTangents->GetSize();
	Zenith_Vector<u_int> auKeyIds;
	Zenith_Vector<Flux_KeyTangents> axBefore;
	auKeyIds.Reserve(uCount);
	axBefore.Reserve(uCount);
	for (u_int u = 0; u < uCount; ++u)
	{
		auKeyIds.PushBack(GetKeyIdAtIndex(xTrack, u));
		axBefore.PushBack(pxTangents->Get(u));
	}

	// The channel's own preset is the one write path into the parallel arrays
	// (Flux/MeshAnimation/CLAUDE.md). ComputeFlatTangents writes ZEROES, which the
	// sampler reads as LINEAR — which is why the verb calling it is named Linear.
	if (bAuto)
	{
		pxChannel->ComputeAutoTangents(xTrack.m_eTrack);
	}
	else
	{
		pxChannel->ComputeFlatTangents(xTrack.m_eTrack);
	}

	if (!BeginCompound())
	{
		return false;
	}
	bool bAnyChanged = false;
	for (u_int u = 0; u < uCount && u < pxTangents->GetSize(); ++u)
	{
		const Flux_KeyTangents& xAfter = pxTangents->Get(u);
		if (auKeyIds.Get(u) == uINVALID_ANIM_KEY_ID || TangentsEqual(axBefore.Get(u), xAfter))
		{
			continue;
		}
		bAnyChanged = true;
		// RECORDED, not executed: the channel has already applied the change. The
		// command exists to carry the inverse.
		PushCommand(new Zenith_AnimCommand_KeyTangents(this, xTrack, auKeyIds.Get(u),
			axBefore.Get(u), xAfter, szDescription));
	}
	if (bAnyChanged)
	{
		MarkDirty();
	}
	// An EMPTY group is deleted and nothing is pushed, so a preset applied twice
	// leaves one undo step rather than two — the same "a no-op is not an edit"
	// rule the single-key verb follows.
	EndCompound(szDescription, /*bKeep*/ true);
	return true;
}

bool Zenith_AnimationDocument::SetTrackTangentsAuto(const Zenith_AnimTrackId& xTrack)
{
	return ApplyTrackTangentPreset(xTrack, /*bAuto*/ true, "Auto Tangents");
}

bool Zenith_AnimationDocument::SetTrackTangentsLinear(const Zenith_AnimTrackId& xTrack)
{
	return ApplyTrackTangentPreset(xTrack, /*bAuto*/ false, "Linear Tangents");
}

bool Zenith_AnimationDocument::SetDuration(float fDurationSeconds)
{
	if (!m_bOpen)
	{
		return false;
	}
	const float fOldSeconds = m_xWorkingClip.GetDuration();
	if (!ApplySetDuration(fDurationSeconds))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_Duration(this, fOldSeconds, fDurationSeconds));
	return true;
}

u_int Zenith_AnimationDocument::AddEvent(const std::string& strName, float fNormalizedTime, const Zenith_Maths::Vector4& xPayload)
{
	if (!m_bOpen)
	{
		return uINVALID_ANIM_KEY_ID;
	}
	Flux_AnimationEvent xEvent;
	xEvent.m_strEventName = strName;
	xEvent.m_fNormalizedTime = fNormalizedTime;
	xEvent.m_xData = xPayload;

	const u_int uEventId = ApplyAddEvent(xEvent, uINVALID_ANIM_KEY_ID);
	if (uEventId == uINVALID_ANIM_KEY_ID)
	{
		return uINVALID_ANIM_KEY_ID;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_EventAdd(this, uEventId, xEvent));
	return uEventId;
}

bool Zenith_AnimationDocument::RemoveEvent(u_int uEventId)
{
	if (!m_bOpen)
	{
		return false;
	}
	Flux_AnimationEvent xEvent;
	if (!GetEvent(uEventId, xEvent))
	{
		return false;
	}
	if (!ApplyRemoveEvent(uEventId))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_EventRemove(this, uEventId, xEvent));
	return true;
}

bool Zenith_AnimationDocument::SetEventTime(u_int uEventId, float fNormalizedTime)
{
	Flux_AnimationEvent xOld;
	if (!m_bOpen || !GetEvent(uEventId, xOld))
	{
		return false;
	}
	Flux_AnimationEvent xNew = xOld;
	xNew.m_fNormalizedTime = fNormalizedTime;
	if (!ApplySetEvent(uEventId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_EventEdit(this, uEventId, xOld, xNew, "Move Animation Event"));
	return true;
}

bool Zenith_AnimationDocument::SetEventName(u_int uEventId, const std::string& strName)
{
	Flux_AnimationEvent xOld;
	if (!m_bOpen || !GetEvent(uEventId, xOld))
	{
		return false;
	}
	Flux_AnimationEvent xNew = xOld;
	xNew.m_strEventName = strName;
	if (!ApplySetEvent(uEventId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_EventEdit(this, uEventId, xOld, xNew, "Rename Animation Event"));
	return true;
}

bool Zenith_AnimationDocument::SetEventPayload(u_int uEventId, const Zenith_Maths::Vector4& xPayload)
{
	Flux_AnimationEvent xOld;
	if (!m_bOpen || !GetEvent(uEventId, xOld))
	{
		return false;
	}
	Flux_AnimationEvent xNew = xOld;
	xNew.m_xData = xPayload;
	if (!ApplySetEvent(uEventId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCommand_EventEdit(this, uEventId, xOld, xNew, "Edit Animation Event Payload"));
	return true;
}

//==============================================================================
// Authored-override location
//==============================================================================

std::string Zenith_AnimationDocument::BuildAuthoredAssetPath(const std::string& strSourceAssetPath)
{
	static const char* const szENGINE_PREFIX = "engine:";
	static const char* const szGAME_PREFIX = "game:";
	static const char* const szAUTHORED = "Authored/";

	const std::string strEnginePrefix(szENGINE_PREFIX);
	const std::string strGamePrefix(szGAME_PREFIX);

	std::string strPrefix;
	std::string strRelative;
	if (strSourceAssetPath.compare(0, strEnginePrefix.size(), strEnginePrefix) == 0)
	{
		strPrefix = strEnginePrefix;
		strRelative = strSourceAssetPath.substr(strEnginePrefix.size());
	}
	else if (strSourceAssetPath.compare(0, strGamePrefix.size(), strGamePrefix) == 0)
	{
		strPrefix = strGamePrefix;
		strRelative = strSourceAssetPath.substr(strGamePrefix.size());
	}
	else
	{
		// No recognised root. There is nothing to hang Authored/ off, and
		// GUESSING one would put an override somewhere no consumer looks.
		return std::string();
	}

	// Already an override — promoting one twice must be idempotent rather than
	// producing Authored/Authored/...
	if (strRelative.compare(0, std::string(szAUTHORED).size(), std::string(szAUTHORED)) == 0)
	{
		return strSourceAssetPath;
	}

	return strPrefix + szAUTHORED + strRelative;
}

std::string Zenith_AnimationDocument::ResolveAuthoredOverridePath(const std::string& strSourceAssetPath) const
{
	if (!m_strAuthoredRootOverride.empty())
	{
		return m_strAuthoredRootOverride + "/" + LeafNameOf(strSourceAssetPath);
	}
	const std::string strAssetPath = BuildAuthoredAssetPath(strSourceAssetPath);
	if (strAssetPath.empty())
	{
		return std::string();
	}
	return Zenith_AssetRegistry::ResolvePath(strAssetPath);
}

void Zenith_AnimationDocument::SetAuthoredRootOverride(const std::string& strAbsoluteDirectory)
{
	m_strAuthoredRootOverride = strAbsoluteDirectory;
	while (!m_strAuthoredRootOverride.empty()
		&& (m_strAuthoredRootOverride.back() == '/' || m_strAuthoredRootOverride.back() == '\\'))
	{
		m_strAuthoredRootOverride.pop_back();
	}
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_AnimationDocument.Tests.inl"
#endif

#endif // ZENITH_TOOLS
