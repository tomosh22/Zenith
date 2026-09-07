#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_BoneMaskDocument.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <cmath>
#include <filesystem>

bool Zenith_BoneMaskDocument_ForceLink()
{
	return true;
}

namespace
{
	// Two weights within this ARE the same weight. Deliberately far below
	// anything a slider or an authoring recipe produces and far above float
	// spacing in [0,1], so "the value you asked for is already in place" cannot
	// be answered wrongly in either direction.
	constexpr float fBONEMASK_WEIGHT_EPSILON = 1.0e-6f;

	bool IsFiniteWeight(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	void EnsureParentDirectory(const std::string& strResolvedPath)
	{
		std::error_code xEC;
		const std::filesystem::path xParent = std::filesystem::path(strResolvedPath).parent_path();
		if (!xParent.empty())
		{
			std::filesystem::create_directories(xParent, xEC);
		}
	}
}

//==============================================================================
// The layer rule (WU-7.1)
//==============================================================================

bool Zenith_BoneMaskDocument::LayerAcceptsMask(Flux_LayerBlendMode eBlendMode)
{
	// ★ READ THE COMPOSITION, NOT THE FIELD LIST. Every Flux_AnimationLayer has
	// an m_xAvatarMask whatever its blend mode, so "does this layer have a mask"
	// is answerable for an additive layer and completely beside the point:
	// Flux_AnimationController's layer loop tests LAYER_BLEND_ADDITIVE first and
	// goes to Flux_SkeletonPose::AdditiveBlend, whose signature has no mask in
	// it. Only the OVERRIDE branch reaches MaskedBlend.
	return eBlendMode == LAYER_BLEND_OVERRIDE;
}

const char* Zenith_BoneMaskDocument::AdditiveLayerMaskNotice()
{
	return "masks do not apply to additive layers";
}

//==============================================================================
// Lifecycle
//==============================================================================

Zenith_BoneMaskDocument::~Zenith_BoneMaskDocument()
{
	if (m_pxOpenCompound != nullptr)
	{
		delete m_pxOpenCompound;
		m_pxOpenCompound = nullptr;
	}
	// ★ THE ONE LINE THAT MAKES THE COMMANDS' RAW DOCUMENT POINTER SAFE. Every
	// command this document pushed lives in this stack and nowhere else, and
	// Clear() deletes them all.
	m_xUndoSystem.Clear();
}

void Zenith_BoneMaskDocument::ResetToClosed()
{
	if (m_pxOpenCompound != nullptr)
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument: reset with a compound still open");
		delete m_pxOpenCompound;
		m_pxOpenCompound = nullptr;
	}
	m_xUndoSystem.Clear();

	Zenith_BoneMaskAsset xEmpty;
	m_xWorkingMask.CopyFrom(xEmpty);

	m_strAssetPath.clear();
	m_strResolvedPath.clear();
	m_ulRecordedFileHash = 0;
	m_bHasRecordedFile = false;
	m_bOpen = false;
	m_bDirty = false;
}

Zenith_BoneMaskAsset* Zenith_BoneMaskDocument::GetAsset() const
{
	if (!m_bOpen || m_strAssetPath.empty())
	{
		return nullptr;
	}
	return Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(m_strAssetPath);
}

Zenith_BoneMaskDocOpenResult Zenith_BoneMaskDocument::Open(const std::string& strAssetPath)
{
	if (m_bOpen && m_bDirty)
	{
		return ZENITH_BONEMASKDOC_OPEN_REFUSED_DIRTY;
	}
	if (strAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::Open: empty asset path");
		return ZENITH_BONEMASKDOC_OPEN_FAILED_NO_ASSET;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strAssetPath);
	const Zenith_BoneMaskAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(strNormalized);
	if (pxAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[BoneMaskDoc] '%s' did not load as a " ZENITH_ANIMMASK_EXT,
			strNormalized.c_str());
		return ZENITH_BONEMASKDOC_OPEN_FAILED_NO_ASSET;
	}

	ResetToClosed();

	m_strAssetPath = strNormalized;
	m_strResolvedPath = Zenith_AssetRegistry::ResolvePath(strNormalized);
	m_xWorkingMask.CopyFrom(*pxAsset);   // the deep copy every edit lands in
	m_bOpen = true;
	m_bDirty = false;
	m_bHasRecordedFile = HashFileContents(m_strResolvedPath, m_ulRecordedFileHash);

	return ZENITH_BONEMASKDOC_OPEN_OK;
}

Zenith_BoneMaskDocOpenResult Zenith_BoneMaskDocument::OpenFresh(const std::string& strAssetPath)
{
	if (m_bOpen && m_bDirty)
	{
		return ZENITH_BONEMASKDOC_OPEN_REFUSED_DIRTY;
	}
	if (strAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::OpenFresh: empty asset path");
		return ZENITH_BONEMASKDOC_OPEN_FAILED_NO_ASSET;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strAssetPath);

	ResetToClosed();

	m_strAssetPath = strNormalized;
	m_strResolvedPath = Zenith_AssetRegistry::ResolvePath(strNormalized);
	m_bOpen = true;
	// ★ FRESH IS DIRTY. Nothing has been written yet and the file on disk (if
	// there is one) does not describe the working copy, so the panel's UNSAVED
	// badge is telling the truth from the first frame.
	m_bDirty = true;
	// No recorded hash: an OpenFresh over an existing file is a deliberate
	// replacement, and reporting it as an external conflict on the first Save
	// would block the one thing the caller asked for.
	m_bHasRecordedFile = false;
	m_ulRecordedFileHash = 0;

	// ★ A FRESH MASK IS A MASK (D47). Zenith_BoneMaskAsset defaults the flag to
	// true and ResetToClosed copies a default-constructed one over the working
	// copy, so this needs no line of its own — but it is worth saying, because
	// the alternative (an empty mask that is "not a mask") would make the first
	// weight painted into it silently do nothing to the layer.
	return ZENITH_BONEMASKDOC_OPEN_OK;
}

Zenith_BoneMaskDocCloseResult Zenith_BoneMaskDocument::Close()
{
	if (!m_bOpen)
	{
		return ZENITH_BONEMASKDOC_CLOSE_OK;
	}
	if (m_bDirty)
	{
		return ZENITH_BONEMASKDOC_CLOSE_REFUSED_DIRTY;
	}
	ResetToClosed();
	return ZENITH_BONEMASKDOC_CLOSE_OK;
}

void Zenith_BoneMaskDocument::CloseDiscardingChanges()
{
	ResetToClosed();
}

bool Zenith_BoneMaskDocument::DiscardChanges()
{
	if (!m_bOpen)
	{
		return false;
	}
	const Zenith_BoneMaskAsset* pxAsset = GetAsset();
	if (pxAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[BoneMaskDoc] cannot discard '%s': it no longer loads",
			m_strAssetPath.c_str());
		return false;
	}

	m_xWorkingMask.CopyFrom(*pxAsset);
	// Every command on the stack describes an edit to content this re-copy has
	// just replaced wholesale.
	m_xUndoSystem.Clear();
	m_bDirty = false;
	return true;
}

//==============================================================================
// Saving
//==============================================================================

bool Zenith_BoneMaskDocument::WriteWorkingMaskToFile(const std::string& strResolvedPath, u_int64& ulOutHash) const
{
	if (strResolvedPath.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument: refusing to write to an empty path");
		return false;
	}

	EnsureParentDirectory(strResolvedPath);

	Zenith_DataStream xStream;
	// WriteToDataStream leads with the shared stream envelope (type id 8,
	// schema 1), so this is the byte layout ParseStream demands. Note this does
	// NOT go through Zenith_BoneMaskAsset::Export: Export resolves the path
	// itself, and SaveAs has already done that against a path this document
	// chose.
	m_xWorkingMask.WriteToDataStream(xStream);
	const u_int64 ulExpected = HashBytes(xStream.GetData(), xStream.GetCursor());
	xStream.WriteToFile(strResolvedPath.c_str());

	// ★ VERIFY THE WRITE BY READING IT BACK. Zenith_FileAccess::WriteFile is
	// void, so without this a full disk or a read-only file would be reported as
	// a successful save and the document would clear its dirty flag over work
	// that never left memory.
	u_int64 ulActual = 0;
	if (!HashFileContents(strResolvedPath, ulActual) || ulActual != ulExpected)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[BoneMaskDoc] wrote '%s' but it does not read back as written",
			strResolvedPath.c_str());
		return false;
	}

	ulOutHash = ulActual;
	return true;
}

void Zenith_BoneMaskDocument::RefreshLiveAssetFromWorkingMask() const
{
	// ★ IN PLACE, NOT ForceUnload + re-acquire. ForceUnload deletes regardless of
	// refcount, and anything holding a view of this asset would be reading freed
	// memory a frame later. Nothing borrows a pointer INTO the entries —
	// Zenith_BoneMaskAsset::ResolveTo copies what it needs into a Flux_BoneMask —
	// so overwriting the content is the whole of the refresh.
	if (m_strAssetPath.empty() || !Zenith_AssetRegistry::IsLoaded(m_strAssetPath))
	{
		return;
	}
	Zenith_BoneMaskAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(m_strAssetPath);
	if (pxAsset != nullptr)
	{
		pxAsset->CopyFrom(m_xWorkingMask);
	}
}

Zenith_BoneMaskDocSaveResult Zenith_BoneMaskDocument::Save()
{
	if (!m_bOpen)
	{
		return ZENITH_BONEMASKDOC_SAVE_FAILED_NO_DOCUMENT;
	}
	if (HasExternalModification())
	{
		// ★ NOTHING IS WRITTEN. A surfaced conflict, never a silent replace.
		Zenith_Log(LOG_CATEGORY_EDITOR, "[BoneMaskDoc] '%s' changed on disk since it was opened — save refused",
			m_strResolvedPath.c_str());
		return ZENITH_BONEMASKDOC_SAVE_CONFLICT_EXTERNAL;
	}
	return SaveOverwritingExternal();
}

Zenith_BoneMaskDocSaveResult Zenith_BoneMaskDocument::SaveOverwritingExternal()
{
	if (!m_bOpen)
	{
		return ZENITH_BONEMASKDOC_SAVE_FAILED_NO_DOCUMENT;
	}

	u_int64 ulHash = 0;
	if (!WriteWorkingMaskToFile(m_strResolvedPath, ulHash))
	{
		return ZENITH_BONEMASKDOC_SAVE_FAILED_WRITE;
	}
	m_ulRecordedFileHash = ulHash;
	m_bHasRecordedFile = true;

	RefreshLiveAssetFromWorkingMask();

	m_bDirty = false;
	// Undo history SURVIVES a save, deliberately — undoing across one is an
	// ordinary thing to want, and it simply makes the document dirty again.
	return ZENITH_BONEMASKDOC_SAVE_OK;
}

Zenith_BoneMaskDocSaveResult Zenith_BoneMaskDocument::SaveAs(const std::string& strNewAssetPath)
{
	if (!m_bOpen)
	{
		return ZENITH_BONEMASKDOC_SAVE_FAILED_NO_DOCUMENT;
	}
	if (strNewAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::SaveAs: empty path");
		return ZENITH_BONEMASKDOC_SAVE_FAILED_WRITE;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strNewAssetPath);
	const std::string strResolved = Zenith_AssetRegistry::ResolvePath(strNormalized);

	u_int64 ulHash = 0;
	if (!WriteWorkingMaskToFile(strResolved, ulHash))
	{
		return ZENITH_BONEMASKDOC_SAVE_FAILED_WRITE;
	}

	m_strAssetPath = strNormalized;
	m_strResolvedPath = strResolved;
	m_ulRecordedFileHash = ulHash;
	m_bHasRecordedFile = true;
	m_bDirty = false;

	RefreshLiveAssetFromWorkingMask();

	// ★ CLEARED, unlike Save: the stack describes edits made to a file this
	// document has just stopped pointing at.
	m_xUndoSystem.Clear();

	return ZENITH_BONEMASKDOC_SAVE_OK;
}

//==============================================================================
// External-modification detection
//==============================================================================

u_int64 Zenith_BoneMaskDocument::HashBytes(const void* pData, u_int64 ulSize)
{
	// FNV-1a 64, the same "did these bytes change" oracle the clip and controller
	// documents use, and for the same reason: a timestamp compares two clocks
	// written by two processes, a hash compares the thing the question is about.
	u_int64 ulHash = 1469598103934665603ull;
	const uint8_t* puBytes = static_cast<const uint8_t*>(pData);
	for (u_int64 ul = 0; ul < ulSize; ++ul)
	{
		ulHash ^= static_cast<u_int64>(puBytes[ul]);
		ulHash *= 1099511628211ull;
	}
	return ulHash;
}

bool Zenith_BoneMaskDocument::HashFileContents(const std::string& strResolvedPath, u_int64& ulOutHash)
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

bool Zenith_BoneMaskDocument::HasExternalModification() const
{
	if (!m_bOpen || !m_bHasRecordedFile)
	{
		return false;
	}
	u_int64 ulHash = 0;
	if (!HashFileContents(m_strResolvedPath, ulHash))
	{
		// The file we opened is gone or unreadable. That is a change.
		return true;
	}
	return ulHash != m_ulRecordedFileHash;
}

//==============================================================================
// Undo
//==============================================================================

void Zenith_BoneMaskDocument::PushCommand(Zenith_UndoCommand* pxCommand)
{
	if (pxCommand == nullptr)
	{
		return;
	}
	// A group INTERCEPTS the push, and this one branch is the whole of the
	// grouping mechanism as far as the verbs are concerned.
	if (m_pxOpenCompound != nullptr)
	{
		m_pxOpenCompound->Adopt(pxCommand);
		return;
	}
	m_xUndoSystem.Record(pxCommand);
}

bool Zenith_BoneMaskDocument::BeginCompound()
{
	if (m_pxOpenCompound != nullptr)
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::BeginCompound: a group is already open");
		return false;
	}
	m_pxOpenCompound = new Zenith_BoneMaskCommand_Compound(this, "Edit Bone Mask");
	return true;
}

bool Zenith_BoneMaskDocument::EndCompound(const char* szDescription, bool bKeep)
{
	if (m_pxOpenCompound == nullptr)
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::EndCompound: no group is open");
		return false;
	}

	Zenith_BoneMaskCommand_Compound* pxGroup = m_pxOpenCompound;
	m_pxOpenCompound = nullptr;

	if (!bKeep)
	{
		// Roll the partial application back, in reverse, and throw it away.
		pxGroup->Undo();
		delete pxGroup;
		return false;
	}
	if (pxGroup->GetChildCount() == 0)
	{
		// An operation that turned out to be a no-op leaves no undo entry.
		delete pxGroup;
		return false;
	}

	pxGroup->SetDescription(szDescription);
	m_xUndoSystem.Record(pxGroup);
	return true;
}

void Zenith_BoneMaskDocument::Undo()
{
	if (!m_xUndoSystem.CanUndo())
	{
		return;
	}
	m_xUndoSystem.Undo();
	MarkDirty();
}

void Zenith_BoneMaskDocument::Redo()
{
	if (!m_xUndoSystem.CanRedo())
	{
		return;
	}
	m_xUndoSystem.Redo();
	MarkDirty();
}

//==============================================================================
// Inspection
//==============================================================================

float Zenith_BoneMaskDocument::GetBoneWeight(const std::string& strBoneName) const
{
	return m_xWorkingMask.GetBoneWeight(strBoneName);
}

bool Zenith_BoneMaskDocument::HasBone(const std::string& strBoneName) const
{
	return m_xWorkingMask.HasBone(strBoneName);
}

//==============================================================================
// The applied primitives — ONLY the commands and the public verbs call these.
//==============================================================================

void Zenith_BoneMaskDocument::ApplyWeight(const std::string& strBoneName, float fWeight, bool bHasEntry)
{
	if (bHasEntry)
	{
		m_xWorkingMask.SetBoneWeight(strBoneName, fWeight);
	}
	else
	{
		// ★ NOT "SET IT TO ZERO". An entry at 0.0 and no entry at all are
		// different files, and this primitive's whole job is to be able to put
		// either one back.
		m_xWorkingMask.RemoveBone(strBoneName);
	}
	MarkDirty();
}

void Zenith_BoneMaskDocument::ApplyHasAvatarMask(bool bHasAvatarMask)
{
	m_xWorkingMask.SetHasAvatarMask(bHasAvatarMask);
	MarkDirty();
}

//==============================================================================
// Mutation
//==============================================================================

bool Zenith_BoneMaskDocument::SetBoneWeight(const std::string& strBoneName, float fWeight)
{
	if (!m_bOpen)
	{
		return false;
	}
	if (strBoneName.empty())
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::SetBoneWeight: a mask entry must name a bone");
		return false;
	}
	if (!IsFiniteWeight(fWeight))
	{
		// ★ REFUSED, NOT CLAMPED. A NaN here is a caller's arithmetic slip;
		// clamping it to 0 or 1 turns a bug into an authored value that nothing
		// downstream could tell from a deliberate one.
		Zenith_Assert(false, "Zenith_BoneMaskDocument::SetBoneWeight: non-finite weight for bone '%s'",
			strBoneName.c_str());
		return false;
	}

	// Clamped up front, matching Flux_BoneMask::SetBoneWeight, so the document
	// cannot hold a value the resolve would quietly change underneath it.
	const float fClamped = fWeight < 0.0f ? 0.0f : (fWeight > 1.0f ? 1.0f : fWeight);

	const bool bHadEntry = m_xWorkingMask.HasBone(strBoneName);
	const float fOldWeight = m_xWorkingMask.GetBoneWeight(strBoneName);
	if (bHadEntry && std::fabs(fOldWeight - fClamped) < fBONEMASK_WEIGHT_EPSILON)
	{
		// ASSIGNMENT: the value asked for is already in place. True, no mutation,
		// no undo entry. See the header for why this is the right side.
		return true;
	}

	ApplyWeight(strBoneName, fClamped, true);
	PushCommand(new Zenith_BoneMaskCommand_Weight(this, strBoneName, fOldWeight, bHadEntry,
		fClamped, true, "Set Bone Mask Weight"));
	return true;
}

bool Zenith_BoneMaskDocument::SetSubtreeWeight(const Zenith_SkeletonAsset& xSkeleton,
	const std::string& strRootBoneName, float fWeight)
{
	if (!m_bOpen)
	{
		return false;
	}
	const int32_t iRoot = xSkeleton.GetBoneIndex(strRootBoneName);
	if (iRoot == Zenith_SkeletonAsset::INVALID_BONE_INDEX)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[BoneMaskDoc] subtree root '%s' is not a bone of this rig",
			strRootBoneName.c_str());
		return false;
	}
	if (!IsFiniteWeight(fWeight))
	{
		Zenith_Assert(false, "Zenith_BoneMaskDocument::SetSubtreeWeight: non-finite weight");
		return false;
	}

	// ★ ONE FORWARD PASS, NOT A RECURSION, and that rests on the skeleton
	// invariant Flux_SkeletonPose::ComputeModelSpaceMatricesFromSkeleton already
	// asserts: a parent always precedes its children in storage order. So a bone
	// is in the subtree exactly when it IS the root or its parent already is, and
	// the answer for every bone is known by the time it is reached.
	const uint32_t uNumBones = xSkeleton.GetNumBones();
	Zenith_Vector<bool> abInSubtree;
	abInSubtree.Reserve(uNumBones);
	for (uint32_t u = 0; u < uNumBones; ++u)
	{
		abInSubtree.PushBack(false);
	}

	BeginCompound();
	for (uint32_t u = 0; u < uNumBones; ++u)
	{
		if (static_cast<int32_t>(u) == iRoot)
		{
			abInSubtree.Get(u) = true;
		}
		else
		{
			const int32_t iParent = xSkeleton.GetBone(u).m_iParentIndex;
			abInSubtree.Get(u) = iParent != Zenith_SkeletonAsset::INVALID_BONE_INDEX &&
				static_cast<uint32_t>(iParent) < u && abInSubtree.Get(static_cast<uint32_t>(iParent));
		}

		if (abInSubtree.Get(u))
		{
			SetBoneWeight(xSkeleton.GetBone(u).m_strName, fWeight);
		}
	}
	// May return false when every bone was already at that weight — which is a
	// no-op, not a failure, so it does not change what this verb answers.
	EndCompound("Set Bone Mask Subtree");

	return true;
}

bool Zenith_BoneMaskDocument::SetHasAvatarMask(bool bHasAvatarMask)
{
	if (!m_bOpen)
	{
		return false;
	}
	if (m_xWorkingMask.HasAvatarMask() == bHasAvatarMask)
	{
		return true;   // ASSIGNMENT — already what you asked for.
	}

	const bool bOld = m_xWorkingMask.HasAvatarMask();
	ApplyHasAvatarMask(bHasAvatarMask);
	PushCommand(new Zenith_BoneMaskCommand_HasAvatar(this, bOld, bHasAvatarMask));
	return true;
}

bool Zenith_BoneMaskDocument::RemoveBone(const std::string& strBoneName)
{
	if (!m_bOpen)
	{
		return false;
	}
	if (!m_xWorkingMask.HasBone(strBoneName))
	{
		// A REMOVAL, so a miss is a genuine refusal: the caller expected there to
		// be one.
		return false;
	}

	const float fOldWeight = m_xWorkingMask.GetBoneWeight(strBoneName);
	ApplyWeight(strBoneName, 0.0f, false);
	PushCommand(new Zenith_BoneMaskCommand_Weight(this, strBoneName, fOldWeight, true,
		0.0f, false, "Remove Bone Mask Entry"));
	return true;
}

//==============================================================================
// The commands
//==============================================================================

Zenith_BoneMaskCommandBase::Zenith_BoneMaskCommandBase(Zenith_BoneMaskDocument* pxDocument, const char* szDescription)
	: m_pxDocument(pxDocument)
	, m_strDescription(szDescription != nullptr ? szDescription : "Edit Bone Mask")
{
	Zenith_Assert(m_pxDocument != nullptr, "Zenith_BoneMaskCommandBase: null document");
}

Zenith_BoneMaskCommand_Weight::Zenith_BoneMaskCommand_Weight(Zenith_BoneMaskDocument* pxDocument,
	const std::string& strBoneName, float fOldWeight, bool bHadEntry, float fNewWeight, bool bHasEntry,
	const char* szDescription)
	: Zenith_BoneMaskCommandBase(pxDocument, szDescription)
	, m_strBoneName(strBoneName)
	, m_fOldWeight(fOldWeight)
	, m_bHadEntry(bHadEntry)
	, m_fNewWeight(fNewWeight)
	, m_bHasEntry(bHasEntry)
{
}

void Zenith_BoneMaskCommand_Weight::Execute()
{
	m_pxDocument->ApplyWeight(m_strBoneName, m_fNewWeight, m_bHasEntry);
}

void Zenith_BoneMaskCommand_Weight::Undo()
{
	m_pxDocument->ApplyWeight(m_strBoneName, m_fOldWeight, m_bHadEntry);
}

Zenith_BoneMaskCommand_HasAvatar::Zenith_BoneMaskCommand_HasAvatar(Zenith_BoneMaskDocument* pxDocument,
	bool bOld, bool bNew)
	: Zenith_BoneMaskCommandBase(pxDocument, "Set Has Avatar Mask")
	, m_bOld(bOld)
	, m_bNew(bNew)
{
}

void Zenith_BoneMaskCommand_HasAvatar::Execute()
{
	m_pxDocument->ApplyHasAvatarMask(m_bNew);
}

void Zenith_BoneMaskCommand_HasAvatar::Undo()
{
	m_pxDocument->ApplyHasAvatarMask(m_bOld);
}

Zenith_BoneMaskCommand_Compound::Zenith_BoneMaskCommand_Compound(Zenith_BoneMaskDocument* pxDocument,
	const char* szDescription)
	: Zenith_BoneMaskCommandBase(pxDocument, szDescription)
{
}

Zenith_BoneMaskCommand_Compound::~Zenith_BoneMaskCommand_Compound()
{
	for (u_int u = 0; u < m_apxChildren.GetSize(); ++u)
	{
		delete m_apxChildren.Get(u);
	}
	m_apxChildren.Clear();
}

void Zenith_BoneMaskCommand_Compound::Adopt(Zenith_UndoCommand* pxChild)
{
	if (pxChild == nullptr)
	{
		return;
	}
	m_apxChildren.PushBack(pxChild);
}

void Zenith_BoneMaskCommand_Compound::SetDescription(const char* szDescription)
{
	if (szDescription != nullptr)
	{
		m_strDescription = szDescription;
	}
}

void Zenith_BoneMaskCommand_Compound::Execute()
{
	for (u_int u = 0; u < m_apxChildren.GetSize(); ++u)
	{
		m_apxChildren.Get(u)->Execute();
	}
}

void Zenith_BoneMaskCommand_Compound::Undo()
{
	// ★ REVERSE. Two children may name the same bone (a subtree paint that
	// re-states its root), and only the exact inverse order puts the earlier
	// value back.
	for (u_int u = m_apxChildren.GetSize(); u > 0; --u)
	{
		m_apxChildren.Get(u - 1)->Undo();
	}
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_BoneMaskDocument.Tests.inl"
#endif

#endif // ZENITH_TOOLS
