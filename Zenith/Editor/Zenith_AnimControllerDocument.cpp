#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimControllerDocument.h"
#include "Editor/Zenith_EditorAnimCtrlCommands.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AnimatorControllerAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

bool Zenith_AnimControllerDocument_ForceLink()
{
	// ★ ANCHORS BOTH TUs FROM ONE CALL, the Zenith_AnimationDocument_ForceLink
	// idiom: an .obj the linker never pulls in takes its ZENITH_TEST registrars
	// with it, the unit count moves by zero and nothing reds.
	return Zenith_EditorAnimCtrlCommands_ForceLink();
}

namespace
{
	void EnsureParentDirectory(const std::string& strResolvedPath)
	{
		std::error_code xEC;
		const std::filesystem::path xParent = std::filesystem::path(strResolvedPath).parent_path();
		if (!xParent.empty())
		{
			std::filesystem::create_directories(xParent, xEC);
		}
	}

	// A state's whole payload, through Flux_AnimationState's own stream walk.
	void ReadStateFromBytes(Flux_AnimationState& xState, const Zenith_Vector<char>& axBytes)
	{
		if (axBytes.GetSize() == 0)
		{
			return;
		}
		// The stream does not own the bytes — it is a cursor over the command's
		// own copy, which outlives the read.
		Zenith_DataStream xStream(const_cast<char*>(&axBytes.Get(0)), axBytes.GetSize());
		xState.ReadFromDataStream(xStream);
	}

	// Copy every transition out of a Zenith_Vector into another one. Written out
	// rather than assigned so a reader can see there is no aliasing: a command
	// holds its OWN copy of a list the def is about to overwrite.
	void CopyTransitions(const Zenith_Vector<Flux_StateTransition>& xFrom, Zenith_Vector<Flux_StateTransition>& xTo)
	{
		xTo.Clear();
		xTo.Reserve(xFrom.GetSize());
		for (u_int u = 0; u < xFrom.GetSize(); ++u)
		{
			xTo.PushBack(xFrom.Get(u));
		}
	}
}

//==============================================================================
// Zenith_AnimCtrlParameterDecl
//==============================================================================

Zenith_AnimCtrlParameterDecl Zenith_AnimCtrlParameterDecl::Float(const std::string& strName, float fDefault)
{
	Zenith_AnimCtrlParameterDecl xDecl;
	xDecl.m_strName = strName;
	xDecl.m_eType = Flux_AnimationParameters::ParamType::Float;
	xDecl.m_fDefault = fDefault;
	return xDecl;
}

Zenith_AnimCtrlParameterDecl Zenith_AnimCtrlParameterDecl::Int(const std::string& strName, int32_t iDefault)
{
	Zenith_AnimCtrlParameterDecl xDecl;
	xDecl.m_strName = strName;
	xDecl.m_eType = Flux_AnimationParameters::ParamType::Int;
	xDecl.m_iDefault = iDefault;
	return xDecl;
}

Zenith_AnimCtrlParameterDecl Zenith_AnimCtrlParameterDecl::Bool(const std::string& strName, bool bDefault)
{
	Zenith_AnimCtrlParameterDecl xDecl;
	xDecl.m_strName = strName;
	xDecl.m_eType = Flux_AnimationParameters::ParamType::Bool;
	xDecl.m_bDefault = bDefault;
	return xDecl;
}

Zenith_AnimCtrlParameterDecl Zenith_AnimCtrlParameterDecl::Trigger(const std::string& strName)
{
	Zenith_AnimCtrlParameterDecl xDecl;
	xDecl.m_strName = strName;
	xDecl.m_eType = Flux_AnimationParameters::ParamType::Trigger;
	return xDecl;
}

//==============================================================================
// Lifecycle
//==============================================================================

Zenith_AnimControllerDocument::~Zenith_AnimControllerDocument()
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

void Zenith_AnimControllerDocument::ResetToClosed()
{
	if (m_pxOpenCompound != nullptr)
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument: reset with a compound still open");
		delete m_pxOpenCompound;
		m_pxOpenCompound = nullptr;
	}
	m_xUndoSystem.Clear();
	m_xWorkingDef.Clear();
	m_strAssetPath.clear();
	m_strResolvedPath.clear();
	m_uSelectedMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;
	m_ulRecordedFileHash = 0;
	m_bHasRecordedFile = false;
	m_bOpen = false;
	m_bDirty = false;
}

Zenith_AnimatorControllerAsset* Zenith_AnimControllerDocument::GetAsset() const
{
	if (!m_bOpen || m_strAssetPath.empty())
	{
		return nullptr;
	}
	return Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(m_strAssetPath);
}

Zenith_AnimCtrlDocOpenResult Zenith_AnimControllerDocument::Open(const std::string& strAssetPath)
{
	if (m_bOpen && m_bDirty)
	{
		return ZENITH_ANIMCTRLDOC_OPEN_REFUSED_DIRTY;
	}
	if (strAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument::Open: empty asset path");
		return ZENITH_ANIMCTRLDOC_OPEN_FAILED_NO_ASSET;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strAssetPath);
	const Zenith_AnimatorControllerAsset* pxAsset =
		Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(strNormalized);
	if (pxAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimCtrlDoc] '%s' did not load as a " ZENITH_ANIMCTRL_EXT,
			strNormalized.c_str());
		return ZENITH_ANIMCTRLDOC_OPEN_FAILED_NO_ASSET;
	}

	ResetToClosed();

	m_strAssetPath = strNormalized;
	m_strResolvedPath = Zenith_AssetRegistry::ResolvePath(strNormalized);
	m_xWorkingDef.CopyFrom(pxAsset->GetDef());   // the deep copy every edit lands in
	m_bOpen = true;
	m_bDirty = false;
	m_uSelectedMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;
	m_bHasRecordedFile = HashFileContents(m_strResolvedPath, m_ulRecordedFileHash);

	return ZENITH_ANIMCTRLDOC_OPEN_OK;
}

Zenith_AnimCtrlDocOpenResult Zenith_AnimControllerDocument::OpenFresh(const std::string& strAssetPath)
{
	if (m_bOpen && m_bDirty)
	{
		return ZENITH_ANIMCTRLDOC_OPEN_REFUSED_DIRTY;
	}
	if (strAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument::OpenFresh: empty asset path");
		return ZENITH_ANIMCTRLDOC_OPEN_FAILED_NO_ASSET;
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
	m_uSelectedMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;
	// No recorded hash: an OpenFresh over an existing file is a deliberate
	// replacement, and reporting it as an external conflict on the first Save
	// would block the one thing the caller asked for.
	m_bHasRecordedFile = false;
	m_ulRecordedFileHash = 0;

	// A fresh controller gets its top-level machine immediately, so a recipe's
	// first AddState has somewhere to go without an implicit creation buried in
	// a mutation verb.
	m_xWorkingDef.GetOrCreateStateMachineDef();

	return ZENITH_ANIMCTRLDOC_OPEN_OK;
}

Zenith_AnimCtrlDocCloseResult Zenith_AnimControllerDocument::Close()
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMCTRLDOC_CLOSE_OK;
	}
	if (m_bDirty)
	{
		return ZENITH_ANIMCTRLDOC_CLOSE_REFUSED_DIRTY;
	}
	ResetToClosed();
	return ZENITH_ANIMCTRLDOC_CLOSE_OK;
}

void Zenith_AnimControllerDocument::CloseDiscardingChanges()
{
	ResetToClosed();
}

bool Zenith_AnimControllerDocument::DiscardChanges()
{
	if (!m_bOpen)
	{
		return false;
	}
	const Zenith_AnimatorControllerAsset* pxAsset = GetAsset();
	if (pxAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimCtrlDoc] cannot discard '%s': it no longer loads",
			m_strAssetPath.c_str());
		return false;
	}

	m_xWorkingDef.CopyFrom(pxAsset->GetDef());
	// Every command on the stack describes an edit to a def this re-copy has
	// just replaced wholesale.
	m_xUndoSystem.Clear();
	m_bDirty = false;
	if (FindMachine(m_uSelectedMachineId) == nullptr)
	{
		m_uSelectedMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;
	}
	return true;
}

//==============================================================================
// Saving
//==============================================================================

bool Zenith_AnimControllerDocument::WriteWorkingDefToFile(const std::string& strResolvedPath, u_int64& ulOutHash) const
{
	if (strResolvedPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument: refusing to write to an empty path");
		return false;
	}

	EnsureParentDirectory(strResolvedPath);

	Zenith_DataStream xStream;
	// WriteToDataStream leads with the shared stream envelope (type id 7,
	// schema 1), so this is the byte layout ParseStream demands.
	m_xWorkingDef.WriteToDataStream(xStream);
	const u_int64 ulExpected = HashBytes(xStream.GetData(), xStream.GetCursor());
	xStream.WriteToFile(strResolvedPath.c_str());

	// ★ VERIFY THE WRITE BY READING IT BACK. Zenith_FileAccess::WriteFile is
	// void, so without this a full disk or a read-only file would be reported as
	// a successful save and the document would clear its dirty flag over work
	// that never left memory.
	u_int64 ulActual = 0;
	if (!HashFileContents(strResolvedPath, ulActual) || ulActual != ulExpected)
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "[AnimCtrlDoc] wrote '%s' but it does not read back as written",
			strResolvedPath.c_str());
		return false;
	}

	ulOutHash = ulActual;
	return true;
}

void Zenith_AnimControllerDocument::RefreshLiveAssetFromWorkingDef() const
{
	// ★ IN PLACE, NOT ForceUnload + re-acquire. ForceUnload deletes regardless
	// of refcount, and anything holding a view of this asset would be reading
	// freed memory a frame later. Nothing borrows a pointer INTO the def —
	// Flux_AnimationController::BuildFromControllerDef copies everything it
	// needs — so overwriting the def is the whole of the refresh.
	if (m_strAssetPath.empty() || !Zenith_AssetRegistry::IsLoaded(m_strAssetPath))
	{
		return;
	}
	Zenith_AnimatorControllerAsset* pxAsset =
		Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(m_strAssetPath);
	if (pxAsset != nullptr)
	{
		pxAsset->GetDef().CopyFrom(m_xWorkingDef);
	}
}

Zenith_AnimCtrlDocSaveResult Zenith_AnimControllerDocument::Save()
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMCTRLDOC_SAVE_FAILED_NO_DOCUMENT;
	}
	if (HasExternalModification())
	{
		// ★ NOTHING IS WRITTEN. A surfaced conflict, never a silent replace.
		Zenith_Log(LOG_CATEGORY_EDITOR, "[AnimCtrlDoc] '%s' changed on disk since it was opened — save refused",
			m_strResolvedPath.c_str());
		return ZENITH_ANIMCTRLDOC_SAVE_CONFLICT_EXTERNAL;
	}
	return SaveOverwritingExternal();
}

Zenith_AnimCtrlDocSaveResult Zenith_AnimControllerDocument::SaveOverwritingExternal()
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMCTRLDOC_SAVE_FAILED_NO_DOCUMENT;
	}

	u_int64 ulHash = 0;
	if (!WriteWorkingDefToFile(m_strResolvedPath, ulHash))
	{
		return ZENITH_ANIMCTRLDOC_SAVE_FAILED_WRITE;
	}
	m_ulRecordedFileHash = ulHash;
	m_bHasRecordedFile = true;

	RefreshLiveAssetFromWorkingDef();

	m_bDirty = false;
	// Undo history SURVIVES a save, deliberately — undoing across one is an
	// ordinary thing to want, and it simply makes the document dirty again.
	return ZENITH_ANIMCTRLDOC_SAVE_OK;
}

Zenith_AnimCtrlDocSaveResult Zenith_AnimControllerDocument::SaveAs(const std::string& strNewAssetPath)
{
	if (!m_bOpen)
	{
		return ZENITH_ANIMCTRLDOC_SAVE_FAILED_NO_DOCUMENT;
	}
	if (strNewAssetPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument::SaveAs: empty path");
		return ZENITH_ANIMCTRLDOC_SAVE_FAILED_WRITE;
	}

	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strNewAssetPath);
	const std::string strResolved = Zenith_AssetRegistry::ResolvePath(strNormalized);

	u_int64 ulHash = 0;
	if (!WriteWorkingDefToFile(strResolved, ulHash))
	{
		return ZENITH_ANIMCTRLDOC_SAVE_FAILED_WRITE;
	}

	m_strAssetPath = strNormalized;
	m_strResolvedPath = strResolved;
	m_ulRecordedFileHash = ulHash;
	m_bHasRecordedFile = true;
	m_bDirty = false;

	RefreshLiveAssetFromWorkingDef();

	// ★ CLEARED, unlike Save: the stack describes edits made to a file this
	// document has just stopped pointing at.
	m_xUndoSystem.Clear();

	return ZENITH_ANIMCTRLDOC_SAVE_OK;
}

//==============================================================================
// External-modification detection
//==============================================================================

u_int64 Zenith_AnimControllerDocument::HashBytes(const void* pData, u_int64 ulSize)
{
	// FNV-1a 64, the same "did these bytes change" oracle
	// Zenith_AnimationDocument uses, and for the same reason: a timestamp
	// compares two clocks written by two processes, a hash compares the thing
	// the question is about.
	u_int64 ulHash = 1469598103934665603ull;
	const uint8_t* puBytes = static_cast<const uint8_t*>(pData);
	for (u_int64 ul = 0; ul < ulSize; ++ul)
	{
		ulHash ^= static_cast<u_int64>(puBytes[ul]);
		ulHash *= 1099511628211ull;
	}
	return ulHash;
}

bool Zenith_AnimControllerDocument::HashFileContents(const std::string& strResolvedPath, u_int64& ulOutHash)
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

bool Zenith_AnimControllerDocument::HasExternalModification() const
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

void Zenith_AnimControllerDocument::PushCommand(Zenith_UndoCommand* pxCommand)
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

bool Zenith_AnimControllerDocument::BeginCompound()
{
	if (m_pxOpenCompound != nullptr)
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument::BeginCompound: a group is already open");
		return false;
	}
	m_pxOpenCompound = new Zenith_AnimCtrlCommand_Compound(this, "Edit Animator Controller");
	return true;
}

bool Zenith_AnimControllerDocument::EndCompound(const char* szDescription, bool bKeep)
{
	if (m_pxOpenCompound == nullptr)
	{
		Zenith_Assert(false, "Zenith_AnimControllerDocument::EndCompound: no group is open");
		return false;
	}

	Zenith_AnimCtrlCommand_Compound* pxGroup = m_pxOpenCompound;
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

void Zenith_AnimControllerDocument::Undo()
{
	if (!m_xUndoSystem.CanUndo())
	{
		return;
	}
	m_xUndoSystem.Undo();
	MarkDirty();
}

void Zenith_AnimControllerDocument::Redo()
{
	if (!m_xUndoSystem.CanRedo())
	{
		return;
	}
	m_xUndoSystem.Redo();
	MarkDirty();
}

//==============================================================================
// Machine selection
//==============================================================================

Flux_AnimationStateMachineDef* Zenith_AnimControllerDocument::FindMachine(u_int uMachineId)
{
	if (uMachineId == uANIMCTRL_TOP_LEVEL_MACHINE)
	{
		return m_xWorkingDef.GetStateMachineDef();
	}
	Flux_AnimatorControllerLayerDef* pxLayer = m_xWorkingDef.FindLayerById(uMachineId);
	return pxLayer != nullptr ? &pxLayer->GetStateMachineDef() : nullptr;
}

const Flux_AnimationStateMachineDef* Zenith_AnimControllerDocument::FindMachine(u_int uMachineId) const
{
	if (uMachineId == uANIMCTRL_TOP_LEVEL_MACHINE)
	{
		return m_xWorkingDef.GetStateMachineDef();
	}
	const Flux_AnimatorControllerLayerDef* pxLayer = m_xWorkingDef.FindLayerById(uMachineId);
	return pxLayer != nullptr ? &pxLayer->GetStateMachineDef() : nullptr;
}

Flux_AnimationStateMachineDef* Zenith_AnimControllerDocument::EnsureSelectedMachine()
{
	if (!m_bOpen)
	{
		return nullptr;
	}
	if (m_uSelectedMachineId == uANIMCTRL_TOP_LEVEL_MACHINE)
	{
		return &m_xWorkingDef.GetOrCreateStateMachineDef();
	}
	return FindMachine(m_uSelectedMachineId);
}

bool Zenith_AnimControllerDocument::SelectMachine(u_int uLayerId)
{
	if (!m_bOpen)
	{
		return false;
	}
	if (uLayerId != uANIMCTRL_TOP_LEVEL_MACHINE && m_xWorkingDef.FindLayerById(uLayerId) == nullptr)
	{
		return false;
	}
	m_uSelectedMachineId = uLayerId;
	return true;
}

const Flux_AnimationStateMachineDef* Zenith_AnimControllerDocument::GetSelectedMachineDef() const
{
	return FindMachine(m_uSelectedMachineId);
}

void Zenith_AnimControllerDocument::GetLayerIds(Zenith_Vector<u_int>& auOut) const
{
	auOut.Clear();
	for (u_int u = 0; u < m_xWorkingDef.GetLayerCount(); ++u)
	{
		const Flux_AnimatorControllerLayerDef* pxLayer = m_xWorkingDef.GetLayer(u);
		if (pxLayer != nullptr)
		{
			auOut.PushBack(pxLayer->GetLayerId());
		}
	}
}

bool Zenith_AnimControllerDocument::GetLayerName(u_int uLayerId, std::string& strOut) const
{
	const Flux_AnimatorControllerLayerDef* pxLayer = m_xWorkingDef.FindLayerById(uLayerId);
	if (pxLayer == nullptr)
	{
		return false;
	}
	strOut = pxLayer->GetName();
	return true;
}

//==============================================================================
// Inspection
//==============================================================================

void Zenith_AnimControllerDocument::GetStateNamesSorted(Zenith_Vector<std::string>& axOut) const
{
	axOut.Clear();
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return;
	}
	const Zenith_HashMap<std::string, Flux_AnimationState*>& xStates = pxMachine->GetStates();
	axOut.Reserve(xStates.GetSize());
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(xStates); !xIt.Done(); xIt.Next())
	{
		axOut.PushBack(xIt.GetKey());
	}
	std::sort(axOut.begin(), axOut.end());
}

u_int Zenith_AnimControllerDocument::GetStateCount() const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	return pxMachine != nullptr ? pxMachine->GetStates().GetSize() : 0u;
}

bool Zenith_AnimControllerDocument::HasState(const std::string& strStateName) const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	return pxMachine != nullptr && pxMachine->HasState(strStateName);
}

const std::string& Zenith_AnimControllerDocument::GetDefaultStateName() const
{
	static const std::string ls_strEmpty;
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	return pxMachine != nullptr ? pxMachine->GetDefaultStateName() : ls_strEmpty;
}

bool Zenith_AnimControllerDocument::GetStateEditorPosition(const std::string& strStateName,
	Zenith_Maths::Vector2& xOut) const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return false;
	}
	const Flux_AnimationState* pxState = pxMachine->GetState(strStateName);
	if (pxState == nullptr)
	{
		return false;
	}
	xOut = pxState->m_xEditorPosition;
	return true;
}

Zenith_AnimCtrlStateTreeKind Zenith_AnimControllerDocument::ClassifyBlendTree(const Flux_BlendTreeNode* pxRoot)
{
	if (pxRoot == nullptr)
	{
		return ZENITH_ANIMCTRL_TREE_EMPTY;
	}
	// The type NAME, not RTTI — that string is already this hierarchy's
	// discriminator (Flux_BlendTreeNode::CreateFromTypeName reads the same
	// values out of a stream), so there is one vocabulary rather than two.
	return (std::strcmp(pxRoot->GetNodeTypeName(), "Clip") == 0)
		? ZENITH_ANIMCTRL_TREE_SINGLE_CLIP
		: ZENITH_ANIMCTRL_TREE_COMPLEX;
}

Zenith_AnimCtrlStateTreeKind Zenith_AnimControllerDocument::GetStateTreeKind(const std::string& strStateName) const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return ZENITH_ANIMCTRL_TREE_EMPTY;
	}
	const Flux_AnimationState* pxState = pxMachine->GetState(strStateName);
	if (pxState == nullptr)
	{
		return ZENITH_ANIMCTRL_TREE_EMPTY;
	}
	// A CONTAINER state is complex whatever its tree is: its pose comes from a
	// nested machine, and giving it a clip leaf would silently shadow one.
	if (pxState->IsSubStateMachine())
	{
		return ZENITH_ANIMCTRL_TREE_COMPLEX;
	}
	return ClassifyBlendTree(pxState->GetBlendTree());
}

bool Zenith_AnimControllerDocument::GetStateClipName(const std::string& strStateName, std::string& strOut) const
{
	if (GetStateTreeKind(strStateName) != ZENITH_ANIMCTRL_TREE_SINGLE_CLIP)
	{
		return false;
	}
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	const Flux_AnimationState* pxState = pxMachine->GetState(strStateName);
	const Flux_BlendTreeNode_Clip* pxClip = static_cast<const Flux_BlendTreeNode_Clip*>(pxState->GetBlendTree());
	strOut = pxClip->GetClipName();
	return true;
}

u_int Zenith_AnimControllerDocument::GetTransitionCount(const std::string& strFromState) const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return 0u;
	}
	if (strFromState.empty())
	{
		return pxMachine->GetAnyStateTransitions().GetSize();
	}
	const Flux_AnimationState* pxState = pxMachine->GetState(strFromState);
	return pxState != nullptr ? pxState->GetTransitions().GetSize() : 0u;
}

bool Zenith_AnimControllerDocument::GetTransition(const std::string& strFromState, u_int uIndex,
	Flux_StateTransition& xOut) const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return false;
	}
	if (strFromState.empty())
	{
		const Zenith_Vector<Flux_StateTransition>& xAny = pxMachine->GetAnyStateTransitions();
		if (uIndex >= xAny.GetSize())
		{
			return false;
		}
		xOut = xAny.Get(uIndex);
		return true;
	}
	const Flux_AnimationState* pxState = pxMachine->GetState(strFromState);
	if (pxState == nullptr || uIndex >= pxState->GetTransitions().GetSize())
	{
		return false;
	}
	xOut = pxState->GetTransitions().Get(uIndex);
	return true;
}

void Zenith_AnimControllerDocument::GetParameterNamesSorted(Zenith_Vector<std::string>& axOut) const
{
	axOut.Clear();
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return;
	}
	const Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>& xParams =
		pxMachine->GetParameterDeclarations().GetParameters();
	axOut.Reserve(xParams.GetSize());
	for (Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>::Iterator xIt(xParams); !xIt.Done(); xIt.Next())
	{
		axOut.PushBack(xIt.GetKey());
	}
	std::sort(axOut.begin(), axOut.end());
}

bool Zenith_AnimControllerDocument::GetParameter(const std::string& strName, Zenith_AnimCtrlParameterDecl& xOut) const
{
	const Flux_AnimationStateMachineDef* pxMachine = GetSelectedMachineDef();
	if (pxMachine == nullptr)
	{
		return false;
	}
	const Flux_AnimationParameters& xParams = pxMachine->GetParameterDeclarations();
	if (!xParams.HasParameter(strName))
	{
		return false;
	}
	const Flux_AnimationParameters::Parameter* pxParam = xParams.GetParameters().TryGet(strName);
	if (pxParam == nullptr)
	{
		return false;
	}
	xOut = Zenith_AnimCtrlParameterDecl();
	xOut.m_strName = strName;
	xOut.m_eType = pxParam->m_eType;
	switch (pxParam->m_eType)
	{
	case Flux_AnimationParameters::ParamType::Float:   xOut.m_fDefault = pxParam->m_fValue; break;
	case Flux_AnimationParameters::ParamType::Int:     xOut.m_iDefault = pxParam->m_iValue; break;
	case Flux_AnimationParameters::ParamType::Bool:
	case Flux_AnimationParameters::ParamType::Trigger: xOut.m_bDefault = pxParam->m_bValue; break;
	}
	return true;
}

u_int Zenith_AnimControllerDocument::GetClipPathCount() const
{
	return m_xWorkingDef.GetClipPaths().GetSize();
}

bool Zenith_AnimControllerDocument::GetClipPathAt(u_int uIndex, std::string& strOut) const
{
	if (uIndex >= m_xWorkingDef.GetClipPaths().GetSize())
	{
		return false;
	}
	strOut = m_xWorkingDef.GetClipPaths().Get(uIndex);
	return true;
}

//==============================================================================
// Capture helpers
//==============================================================================

void Zenith_AnimControllerDocument::CaptureStateBytes(const Flux_AnimationState& xState, Zenith_Vector<char>& axOut)
{
	Zenith_DataStream xStream(1);
	xState.WriteToDataStream(xStream);
	const u_int64 ulSize = xStream.GetCursor();
	axOut.Clear();
	axOut.Reserve(static_cast<u_int>(ulSize));
	const char* pcBytes = static_cast<const char*>(xStream.GetData());
	for (u_int64 ul = 0; ul < ulSize; ++ul)
	{
		axOut.PushBack(pcBytes[ul]);
	}
}

bool Zenith_AnimControllerDocument::ReadTransitionList(u_int uMachineId, const std::string& strOwner,
	Zenith_AnimCtrlTransitionList& xOut) const
{
	const Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return false;
	}
	xOut.m_strOwnerStateName = strOwner;
	if (strOwner.empty())
	{
		CopyTransitions(pxMachine->GetAnyStateTransitions(), xOut.m_xTransitions);
		return true;
	}
	const Flux_AnimationState* pxState = pxMachine->GetState(strOwner);
	if (pxState == nullptr)
	{
		return false;
	}
	CopyTransitions(pxState->GetTransitions(), xOut.m_xTransitions);
	return true;
}

void Zenith_AnimControllerDocument::CaptureAllTransitionLists(u_int uMachineId,
	Zenith_Vector<Zenith_AnimCtrlTransitionList>& axOut) const
{
	axOut.Clear();
	const Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return;
	}

	Zenith_AnimCtrlTransitionList xAny;
	xAny.m_strOwnerStateName.clear();
	CopyTransitions(pxMachine->GetAnyStateTransitions(), xAny.m_xTransitions);
	axOut.PushBack(xAny);

	const Zenith_HashMap<std::string, Flux_AnimationState*>& xStates = pxMachine->GetStates();
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(xStates); !xIt.Done(); xIt.Next())
	{
		const Flux_AnimationState* pxState = xIt.GetValue();
		if (pxState == nullptr)
		{
			continue;
		}
		Zenith_AnimCtrlTransitionList xList;
		xList.m_strOwnerStateName = xIt.GetKey();
		CopyTransitions(pxState->GetTransitions(), xList.m_xTransitions);
		axOut.PushBack(xList);
	}
}

void Zenith_AnimControllerDocument::CaptureParameters(u_int uMachineId,
	Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axOut) const
{
	axOut.Clear();
	const Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return;
	}
	const Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>& xParams =
		pxMachine->GetParameterDeclarations().GetParameters();
	for (Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>::Iterator xIt(xParams); !xIt.Done(); xIt.Next())
	{
		Zenith_AnimCtrlParameterDecl xDecl;
		xDecl.m_strName = xIt.GetKey();
		xDecl.m_eType = xIt.GetValue().m_eType;
		switch (xDecl.m_eType)
		{
		case Flux_AnimationParameters::ParamType::Float:   xDecl.m_fDefault = xIt.GetValue().m_fValue; break;
		case Flux_AnimationParameters::ParamType::Int:     xDecl.m_iDefault = xIt.GetValue().m_iValue; break;
		case Flux_AnimationParameters::ParamType::Bool:
		case Flux_AnimationParameters::ParamType::Trigger: xDecl.m_bDefault = xIt.GetValue().m_bValue; break;
		}
		axOut.PushBack(xDecl);
	}
	// SLOT order is not an order. Sorting here means an undo record and a
	// re-capture of the same table compare equal.
	std::sort(axOut.begin(), axOut.end(),
		[](const Zenith_AnimCtrlParameterDecl& xA, const Zenith_AnimCtrlParameterDecl& xB)
		{
			return xA.m_strName < xB.m_strName;
		});
}

void Zenith_AnimControllerDocument::CaptureClipPaths(Zenith_Vector<std::string>& axOut) const
{
	axOut.Clear();
	const Zenith_Vector<std::string>& xPaths = m_xWorkingDef.GetClipPaths();
	axOut.Reserve(xPaths.GetSize());
	for (u_int u = 0; u < xPaths.GetSize(); ++u)
	{
		axOut.PushBack(xPaths.Get(u));
	}
}

//==============================================================================
// Apply primitives — the commands' half. No validation beyond "does it resolve",
// no dirty flag, no command push.
//==============================================================================

bool Zenith_AnimControllerDocument::ApplyAddState(u_int uMachineId, const std::string& strName,
	const Zenith_Vector<char>* pxStateBytes)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr || strName.empty() || pxMachine->HasState(strName))
	{
		return false;
	}
	Flux_AnimationState* pxState = pxMachine->AddState(strName);
	if (pxState == nullptr)
	{
		return false;
	}
	if (pxStateBytes != nullptr)
	{
		ReadStateFromBytes(*pxState, *pxStateBytes);
	}
	return true;
}

bool Zenith_AnimControllerDocument::ApplyRemoveState(u_int uMachineId, const std::string& strName)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr || !pxMachine->HasState(strName))
	{
		return false;
	}
	pxMachine->RemoveState(strName);
	return true;
}

bool Zenith_AnimControllerDocument::ApplyRenameState(u_int uMachineId, const std::string& strOldName,
	const std::string& strNewName)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr || strNewName.empty() || strOldName == strNewName)
	{
		return false;
	}
	Flux_AnimationState* pxOld = pxMachine->GetState(strOldName);
	if (pxOld == nullptr || pxMachine->HasState(strNewName))
	{
		return false;
	}
	// A container state cannot be moved: Flux_AnimationState has no
	// SetSubStateMachine, so the nested graph would be destroyed by the remove
	// below. Refused rather than silently deleted.
	if (pxOld->IsSubStateMachine())
	{
		Zenith_Error(LOG_CATEGORY_EDITOR,
			"[AnimCtrlDoc] '%s' is a container state; renaming it would destroy its nested machine",
			strOldName.c_str());
		return false;
	}

	const bool bWasDefault = (pxMachine->GetDefaultStateName() == strOldName);

	Flux_AnimationState* pxNew = pxMachine->AddState(strNewName);
	if (pxNew == nullptr)
	{
		return false;
	}
	// ★ THE TREE IS MOVED, NOT COPIED, AND THE SOURCE IS CLEARED FIRST. A blend
	// tree has no clone verb and Flux_AnimationState's destructor deletes the
	// pointer it holds, so leaving it on the old state would free the tree the
	// new one is now using.
	pxNew->SetBlendTree(pxOld->GetBlendTree());
	pxOld->SetBlendTree(nullptr);
	pxNew->m_xEditorPosition = pxOld->m_xEditorPosition;
	pxNew->GetTransitions().Clear();
	for (u_int u = 0; u < pxOld->GetTransitions().GetSize(); ++u)
	{
		pxNew->GetTransitions().PushBack(pxOld->GetTransitions().Get(u));
	}

	pxMachine->RemoveState(strOldName);
	if (bWasDefault)
	{
		pxMachine->SetDefaultState(strNewName);
	}

	// Retarget every transition that named the old state — the machine's
	// any-state list and every state's own, the new one included (a self-loop
	// still names itself).
	for (u_int u = 0; u < pxMachine->GetAnyStateTransitions().GetSize(); ++u)
	{
		Flux_StateTransition& xTransition = pxMachine->GetAnyStateTransitions().Get(u);
		if (xTransition.m_strTargetStateName == strOldName)
		{
			xTransition.m_strTargetStateName = strNewName;
		}
	}
	Zenith_Vector<std::string> axNames;
	{
		const Zenith_HashMap<std::string, Flux_AnimationState*>& xStates = pxMachine->GetStates();
		for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(xStates); !xIt.Done(); xIt.Next())
		{
			axNames.PushBack(xIt.GetKey());
		}
	}
	for (u_int u = 0; u < axNames.GetSize(); ++u)
	{
		Flux_AnimationState* pxState = pxMachine->GetState(axNames.Get(u));
		if (pxState == nullptr)
		{
			continue;
		}
		for (u_int v = 0; v < pxState->GetTransitions().GetSize(); ++v)
		{
			Flux_StateTransition& xTransition = pxState->GetTransitions().Get(v);
			if (xTransition.m_strTargetStateName == strOldName)
			{
				xTransition.m_strTargetStateName = strNewName;
			}
		}
	}
	return true;
}

bool Zenith_AnimControllerDocument::ApplySetDefaultState(u_int uMachineId, const std::string& strName)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return false;
	}
	if (strName.empty())
	{
		// ★ Flux_AnimationStateMachineDef::SetDefaultState CANNOT CLEAR — it
		// no-ops on a name the machine does not have, and "" is one of those. A
		// document that restored an empty default through it would silently
		// leave the previous one in place, which is exactly the case an undo of
		// "add the first state" hits. Nothing here can write the field, so the
		// caller is told rather than being reported success.
		return pxMachine->GetDefaultStateName().empty();
	}
	if (!pxMachine->HasState(strName))
	{
		return false;
	}
	pxMachine->SetDefaultState(strName);
	return true;
}

bool Zenith_AnimControllerDocument::ApplySetStateClip(u_int uMachineId, const std::string& strName,
	const std::string& strClipName)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return false;
	}
	Flux_AnimationState* pxState = pxMachine->GetState(strName);
	if (pxState == nullptr || pxState->IsSubStateMachine())
	{
		return false;
	}

	Flux_BlendTreeNode* pxOldTree = pxState->GetBlendTree();
	if (strClipName.empty())
	{
		pxState->SetBlendTree(nullptr);
		delete pxOldTree;
		return true;
	}

	if (pxOldTree != nullptr && ClassifyBlendTree(pxOldTree) == ZENITH_ANIMCTRL_TREE_SINGLE_CLIP)
	{
		// Rename in place: the leaf keeps its playhead and its playback rate,
		// which is what an author swapping a clip on a running preview expects.
		static_cast<Flux_BlendTreeNode_Clip*>(pxOldTree)->SetClipName(strClipName);
		return true;
	}

	Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip();
	pxLeaf->SetClipName(strClipName);
	pxState->SetBlendTree(pxLeaf);
	delete pxOldTree;
	return true;
}

bool Zenith_AnimControllerDocument::ApplySetStatePosition(u_int uMachineId, const std::string& strName,
	const Zenith_Maths::Vector2& xPos)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return false;
	}
	Flux_AnimationState* pxState = pxMachine->GetState(strName);
	if (pxState == nullptr)
	{
		return false;
	}
	pxState->m_xEditorPosition = xPos;
	return true;
}

bool Zenith_AnimControllerDocument::ApplySetTransitions(u_int uMachineId, const Zenith_AnimCtrlTransitionList& xList)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return false;
	}
	if (xList.m_strOwnerStateName.empty())
	{
		CopyTransitions(xList.m_xTransitions, pxMachine->GetAnyStateTransitions());
		return true;
	}
	Flux_AnimationState* pxState = pxMachine->GetState(xList.m_strOwnerStateName);
	if (pxState == nullptr)
	{
		return false;
	}
	// ★ WRITTEN VERBATIM, NOT THROUGH AddTransition. That verb inserts by
	// PRIORITY, so replaying a captured list through it would reorder equal
	// priorities and an undo would not restore the indices it recorded.
	CopyTransitions(xList.m_xTransitions, pxState->GetTransitions());
	return true;
}

bool Zenith_AnimControllerDocument::ApplySetParameters(u_int uMachineId,
	const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axDecls)
{
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(uMachineId);
	if (pxMachine == nullptr)
	{
		return false;
	}
	Flux_AnimationParameters& xParams = pxMachine->GetParameterDeclarations();

	// Two passes: the hash map's iterator asserts on a mid-iteration rehash, so
	// the names come out first and the removals happen afterwards.
	Zenith_Vector<std::string> axExisting;
	for (Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>::Iterator xIt(xParams.GetParameters());
		!xIt.Done(); xIt.Next())
	{
		axExisting.PushBack(xIt.GetKey());
	}
	for (u_int u = 0; u < axExisting.GetSize(); ++u)
	{
		xParams.RemoveParameter(axExisting.Get(u));
	}

	for (u_int u = 0; u < axDecls.GetSize(); ++u)
	{
		const Zenith_AnimCtrlParameterDecl& xDecl = axDecls.Get(u);
		switch (xDecl.m_eType)
		{
		case Flux_AnimationParameters::ParamType::Float:   xParams.AddFloat(xDecl.m_strName, xDecl.m_fDefault); break;
		case Flux_AnimationParameters::ParamType::Int:     xParams.AddInt(xDecl.m_strName, xDecl.m_iDefault); break;
		case Flux_AnimationParameters::ParamType::Bool:    xParams.AddBool(xDecl.m_strName, xDecl.m_bDefault); break;
		case Flux_AnimationParameters::ParamType::Trigger: xParams.AddTrigger(xDecl.m_strName); break;
		}
	}
	return true;
}

bool Zenith_AnimControllerDocument::ApplySetClipPaths(const Zenith_Vector<std::string>& axPaths)
{
	m_xWorkingDef.ClearClipPaths();
	for (u_int u = 0; u < axPaths.GetSize(); ++u)
	{
		m_xWorkingDef.AddClipPath(axPaths.Get(u));
	}
	return true;
}

//==============================================================================
// Mutation verbs — validate, mutate, mark dirty, push ONE command.
//==============================================================================

bool Zenith_AnimControllerDocument::AddState(const std::string& strStateName)
{
	if (!m_bOpen || strStateName.empty())
	{
		return false;
	}
	Flux_AnimationStateMachineDef* pxMachine = EnsureSelectedMachine();
	if (pxMachine == nullptr || pxMachine->HasState(strStateName))
	{
		return false;
	}

	const std::string strPrevDefault = pxMachine->GetDefaultStateName();
	if (!ApplyAddState(m_uSelectedMachineId, strStateName, nullptr))
	{
		return false;
	}

	Zenith_Vector<char> axBytes;
	CaptureStateBytes(*pxMachine->GetState(strStateName), axBytes);
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_StateAdd(this, m_uSelectedMachineId, strStateName, axBytes, strPrevDefault));
	return true;
}

bool Zenith_AnimControllerDocument::RemoveState(const std::string& strStateName)
{
	if (!m_bOpen)
	{
		return false;
	}
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(m_uSelectedMachineId);
	if (pxMachine == nullptr || !pxMachine->HasState(strStateName))
	{
		return false;
	}

	Zenith_Vector<char> axBytes;
	CaptureStateBytes(*pxMachine->GetState(strStateName), axBytes);
	const std::string strPrevDefault = pxMachine->GetDefaultStateName();

	Zenith_Vector<Zenith_AnimCtrlTransitionList> axPrevLists;
	CaptureAllTransitionLists(m_uSelectedMachineId, axPrevLists);

	if (!ApplyRemoveState(m_uSelectedMachineId, strStateName))
	{
		return false;
	}

	// ★ THE INBOUND TRANSITIONS GO WITH IT. Flux_AnimationStateMachineDef::
	// RemoveState leaves them, and Flux_AnimationStateMachine::StartTransition
	// then resolves the name to nullptr every frame the condition holds —
	// silently, because it simply does not transition.
	for (u_int u = 0; u < axPrevLists.GetSize(); ++u)
	{
		const Zenith_AnimCtrlTransitionList& xPrev = axPrevLists.Get(u);
		if (xPrev.m_strOwnerStateName == strStateName)
		{
			continue;   // gone with the state
		}
		Zenith_AnimCtrlTransitionList xPruned;
		xPruned.m_strOwnerStateName = xPrev.m_strOwnerStateName;
		for (u_int v = 0; v < xPrev.m_xTransitions.GetSize(); ++v)
		{
			if (xPrev.m_xTransitions.Get(v).m_strTargetStateName != strStateName)
			{
				xPruned.m_xTransitions.PushBack(xPrev.m_xTransitions.Get(v));
			}
		}
		if (xPruned.m_xTransitions.GetSize() != xPrev.m_xTransitions.GetSize())
		{
			ApplySetTransitions(m_uSelectedMachineId, xPruned);
		}
	}

	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_StateRemove(this, m_uSelectedMachineId, strStateName, axBytes,
		strPrevDefault, axPrevLists));
	return true;
}

bool Zenith_AnimControllerDocument::RenameState(const std::string& strOldName, const std::string& strNewName)
{
	if (!m_bOpen || strNewName.empty() || strOldName == strNewName)
	{
		return false;
	}
	if (!ApplyRenameState(m_uSelectedMachineId, strOldName, strNewName))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_StateRename(this, m_uSelectedMachineId, strOldName, strNewName));
	return true;
}

bool Zenith_AnimControllerDocument::SetDefaultState(const std::string& strStateName)
{
	if (!m_bOpen)
	{
		return false;
	}
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(m_uSelectedMachineId);
	if (pxMachine == nullptr || !pxMachine->HasState(strStateName))
	{
		return false;
	}
	const std::string strOld = pxMachine->GetDefaultStateName();
	if (strOld == strStateName)
	{
		// ★ ALREADY THE DEFAULT IS SUCCESS, NOT REFUSAL — see the ASSIGNMENT vs
		// CREATION rule in the header. This is not a hypothetical: AddState makes
		// the FIRST state of a machine its default, so the most natural authoring
		// order there is — AddState("Idle"), AddState("Walk"),
		// SetDefaultState("Idle") — hits it every single time, and returning
		// false made a recipe that merely stated its intent explicitly fail at
		// boot under AnimSmActionChecked.
		return true;
	}
	if (!ApplySetDefaultState(m_uSelectedMachineId, strStateName))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_DefaultState(this, m_uSelectedMachineId, strOld, strStateName));
	return true;
}

bool Zenith_AnimControllerDocument::SetStateClip(const std::string& strStateName, const std::string& strClipName)
{
	if (!m_bOpen)
	{
		return false;
	}
	const Zenith_AnimCtrlStateTreeKind eKind = GetStateTreeKind(strStateName);
	if (!HasState(strStateName) || eKind == ZENITH_ANIMCTRL_TREE_COMPLEX)
	{
		return false;
	}

	std::string strOldClip;
	if (eKind == ZENITH_ANIMCTRL_TREE_SINGLE_CLIP)
	{
		GetStateClipName(strStateName, strOldClip);
	}
	if (strOldClip == strClipName)
	{
		return true;   // ASSIGNMENT: the value asked for is the value in place
	}
	if (!ApplySetStateClip(m_uSelectedMachineId, strStateName, strClipName))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_StateClip(this, m_uSelectedMachineId, strStateName, strOldClip, strClipName));
	return true;
}

bool Zenith_AnimControllerDocument::SetStateEditorPosition(const std::string& strStateName,
	const Zenith_Maths::Vector2& xPosition)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_Maths::Vector2 xOld(0.0f);
	if (!GetStateEditorPosition(strStateName, xOld))
	{
		return false;
	}
	if (xOld.x == xPosition.x && xOld.y == xPosition.y)
	{
		// ASSIGNMENT: a drag that ended where it started is not a failed drag.
		// The panel's click-slop guard usually catches this first; this is the
		// case where it landed on the same value anyway.
		return true;
	}
	if (!ApplySetStatePosition(m_uSelectedMachineId, strStateName, xPosition))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_StatePosition(this, m_uSelectedMachineId, strStateName, xOld, xPosition));
	return true;
}

bool Zenith_AnimControllerDocument::AddTransition(const std::string& strFromState, const std::string& strToState)
{
	if (!m_bOpen || strToState.empty())
	{
		return false;
	}
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(m_uSelectedMachineId);
	if (pxMachine == nullptr || !pxMachine->HasState(strToState))
	{
		return false;
	}
	// An EMPTY from-state addresses the machine's any-state list.
	if (!strFromState.empty() && !pxMachine->HasState(strFromState))
	{
		return false;
	}

	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld))
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xNew = xOld;
	Flux_StateTransition xTransition;
	xTransition.m_strTargetStateName = strToState;
	xNew.m_xTransitions.PushBack(xTransition);

	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Add Transition"));
	return true;
}

bool Zenith_AnimControllerDocument::RemoveTransition(const std::string& strFromState, u_int uIndex)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld) || uIndex >= xOld.m_xTransitions.GetSize())
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xNew;
	xNew.m_strOwnerStateName = strFromState;
	for (u_int u = 0; u < xOld.m_xTransitions.GetSize(); ++u)
	{
		if (u != uIndex)
		{
			xNew.m_xTransitions.PushBack(xOld.m_xTransitions.Get(u));
		}
	}
	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Remove Transition"));
	return true;
}

//------------------------------------------------------------------------------
// The four transition FIELD edits and the two condition edits all share one
// body: read the list, change one entry, write the list, push one command. The
// only difference is the mutation and the description, so they are one helper
// away from being six copies of the same six lines.
//------------------------------------------------------------------------------
namespace
{
	bool ResolveTransitionForEdit(const Zenith_AnimCtrlTransitionList& xOld, u_int uIndex)
	{
		return uIndex < xOld.m_xTransitions.GetSize();
	}
}

bool Zenith_AnimControllerDocument::SetTransitionDuration(const std::string& strFromState, u_int uIndex, float fSeconds)
{
	if (!m_bOpen || !(fSeconds >= 0.0f) || fSeconds > 1.0e6f)
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld) || !ResolveTransitionForEdit(xOld, uIndex))
	{
		return false;
	}
	if (xOld.m_xTransitions.Get(uIndex).m_fTransitionDuration == fSeconds)
	{
		return true;   // ASSIGNMENT: the value asked for is the value in place
	}
	Zenith_AnimCtrlTransitionList xNew = xOld;
	xNew.m_xTransitions.Get(uIndex).m_fTransitionDuration = fSeconds;
	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Set Transition Duration"));
	return true;
}

bool Zenith_AnimControllerDocument::SetTransitionExitTime(const std::string& strFromState, u_int uIndex,
	bool bHasExitTime, float fNormalizedExitTime)
{
	if (!m_bOpen)
	{
		return false;
	}
	// The exit time is a NORMALIZED fraction of the source state's clip, so a
	// value outside [0,1] names a moment the state never reaches.
	if (bHasExitTime && !(fNormalizedExitTime >= 0.0f && fNormalizedExitTime <= 1.0f))
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld) || !ResolveTransitionForEdit(xOld, uIndex))
	{
		return false;
	}
	const Flux_StateTransition& xCurrent = xOld.m_xTransitions.Get(uIndex);
	const float fTarget = bHasExitTime ? fNormalizedExitTime : -1.0f;
	if (xCurrent.m_bHasExitTime == bHasExitTime && xCurrent.m_fExitTime == fTarget)
	{
		return true;   // ASSIGNMENT: the value asked for is the value in place
	}
	Zenith_AnimCtrlTransitionList xNew = xOld;
	xNew.m_xTransitions.Get(uIndex).m_bHasExitTime = bHasExitTime;
	xNew.m_xTransitions.Get(uIndex).m_fExitTime = fTarget;
	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Set Transition Exit Time"));
	return true;
}

bool Zenith_AnimControllerDocument::SetTransitionInterruptible(const std::string& strFromState, u_int uIndex,
	bool bInterruptible)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld) || !ResolveTransitionForEdit(xOld, uIndex))
	{
		return false;
	}
	if (xOld.m_xTransitions.Get(uIndex).m_bInterruptible == bInterruptible)
	{
		return true;   // ASSIGNMENT: the value asked for is the value in place
	}
	Zenith_AnimCtrlTransitionList xNew = xOld;
	xNew.m_xTransitions.Get(uIndex).m_bInterruptible = bInterruptible;
	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Set Transition Interruptible"));
	return true;
}

bool Zenith_AnimControllerDocument::AddCondition(const std::string& strFromState, u_int uIndex,
	const std::string& strParameterName, Flux_TransitionCondition::CompareOp eCompareOp, float fThreshold)
{
	if (!m_bOpen || strParameterName.empty())
	{
		return false;
	}
	// ★ THE TYPE COMES FROM THE DECLARATION, NEVER FROM THE CALLER.
	// Flux_TransitionCondition::Evaluate switches on m_eParamType and reads the
	// matching union member, so a condition whose type disagrees with its
	// parameter compares a reinterpreted bit pattern and is never true.
	Zenith_AnimCtrlParameterDecl xDecl;
	if (!GetParameter(strParameterName, xDecl))
	{
		return false;
	}

	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld) || !ResolveTransitionForEdit(xOld, uIndex))
	{
		return false;
	}

	Flux_TransitionCondition xCondition;
	xCondition.m_strParameterName = strParameterName;
	xCondition.m_eCompareOp = eCompareOp;
	xCondition.m_eParamType = xDecl.m_eType;
	switch (xDecl.m_eType)
	{
	case Flux_AnimationParameters::ParamType::Float:   xCondition.m_fThreshold = fThreshold; break;
	case Flux_AnimationParameters::ParamType::Int:     xCondition.m_iThreshold = static_cast<int32_t>(fThreshold); break;
	case Flux_AnimationParameters::ParamType::Bool:
	case Flux_AnimationParameters::ParamType::Trigger: xCondition.m_bThreshold = (fThreshold != 0.0f); break;
	}

	Zenith_AnimCtrlTransitionList xNew = xOld;
	xNew.m_xTransitions.Get(uIndex).m_xConditions.PushBack(xCondition);
	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Add Condition"));
	return true;
}

bool Zenith_AnimControllerDocument::RemoveCondition(const std::string& strFromState, u_int uIndex, u_int uConditionIndex)
{
	if (!m_bOpen)
	{
		return false;
	}
	Zenith_AnimCtrlTransitionList xOld;
	if (!ReadTransitionList(m_uSelectedMachineId, strFromState, xOld) || !ResolveTransitionForEdit(xOld, uIndex))
	{
		return false;
	}
	if (uConditionIndex >= xOld.m_xTransitions.Get(uIndex).m_xConditions.GetSize())
	{
		return false;
	}

	Zenith_AnimCtrlTransitionList xNew = xOld;
	Zenith_Vector<Flux_TransitionCondition>& xConditions = xNew.m_xTransitions.Get(uIndex).m_xConditions;
	xConditions.Remove(uConditionIndex);
	if (!ApplySetTransitions(m_uSelectedMachineId, xNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Transitions(this, m_uSelectedMachineId, xOld, xNew, "Remove Condition"));
	return true;
}

bool Zenith_AnimControllerDocument::AddParameter(const Zenith_AnimCtrlParameterDecl& xDecl)
{
	if (!m_bOpen || xDecl.m_strName.empty())
	{
		return false;
	}
	Flux_AnimationStateMachineDef* pxMachine = EnsureSelectedMachine();
	if (pxMachine == nullptr || pxMachine->GetParameterDeclarations().HasParameter(xDecl.m_strName))
	{
		return false;
	}

	Zenith_Vector<Zenith_AnimCtrlParameterDecl> axOld;
	CaptureParameters(m_uSelectedMachineId, axOld);
	Zenith_Vector<Zenith_AnimCtrlParameterDecl> axNew;
	CaptureParameters(m_uSelectedMachineId, axNew);
	axNew.PushBack(xDecl);

	if (!ApplySetParameters(m_uSelectedMachineId, axNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Parameters(this, m_uSelectedMachineId, axOld, axNew, "Add Parameter"));
	return true;
}

bool Zenith_AnimControllerDocument::RemoveParameter(const std::string& strName)
{
	if (!m_bOpen || strName.empty())
	{
		return false;
	}
	Flux_AnimationStateMachineDef* pxMachine = FindMachine(m_uSelectedMachineId);
	if (pxMachine == nullptr || !pxMachine->GetParameterDeclarations().HasParameter(strName))
	{
		return false;
	}

	Zenith_Vector<Zenith_AnimCtrlParameterDecl> axOld;
	CaptureParameters(m_uSelectedMachineId, axOld);
	Zenith_Vector<Zenith_AnimCtrlParameterDecl> axNew;
	for (u_int u = 0; u < axOld.GetSize(); ++u)
	{
		if (axOld.Get(u).m_strName != strName)
		{
			axNew.PushBack(axOld.Get(u));
		}
	}
	if (!ApplySetParameters(m_uSelectedMachineId, axNew))
	{
		return false;
	}
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_Parameters(this, m_uSelectedMachineId, axOld, axNew, "Remove Parameter"));
	return true;
}

bool Zenith_AnimControllerDocument::AddClipPath(const std::string& strPath)
{
	if (!m_bOpen || strPath.empty())
	{
		return false;
	}
	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strPath);

	Zenith_Vector<std::string> axOld;
	CaptureClipPaths(axOld);
	for (u_int u = 0; u < axOld.GetSize(); ++u)
	{
		if (axOld.Get(u) == strNormalized)
		{
			return false;   // the def ignores a duplicate; an undo entry would reverse nothing
		}
	}
	Zenith_Vector<std::string> axNew;
	CaptureClipPaths(axNew);
	axNew.PushBack(strNormalized);

	ApplySetClipPaths(axNew);
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_ClipPaths(this, axOld, axNew, "Add Clip"));
	return true;
}

bool Zenith_AnimControllerDocument::RemoveClipPath(const std::string& strPath)
{
	if (!m_bOpen || strPath.empty())
	{
		return false;
	}
	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strPath);

	Zenith_Vector<std::string> axOld;
	CaptureClipPaths(axOld);
	Zenith_Vector<std::string> axNew;
	for (u_int u = 0; u < axOld.GetSize(); ++u)
	{
		if (axOld.Get(u) != strNormalized)
		{
			axNew.PushBack(axOld.Get(u));
		}
	}
	if (axNew.GetSize() == axOld.GetSize())
	{
		return false;
	}
	ApplySetClipPaths(axNew);
	MarkDirty();
	PushCommand(new Zenith_AnimCtrlCommand_ClipPaths(this, axOld, axNew, "Remove Clip"));
	return true;
}

//==============================================================================
// Pure naming helpers
//==============================================================================

const char* Zenith_AnimControllerDocument::CompareOpName(Flux_TransitionCondition::CompareOp eOp)
{
	switch (eOp)
	{
	case Flux_TransitionCondition::CompareOp::Equal:        return "==";
	case Flux_TransitionCondition::CompareOp::NotEqual:     return "!=";
	case Flux_TransitionCondition::CompareOp::Greater:      return ">";
	case Flux_TransitionCondition::CompareOp::Less:         return "<";
	case Flux_TransitionCondition::CompareOp::GreaterEqual: return ">=";
	case Flux_TransitionCondition::CompareOp::LessEqual:    return "<=";
	}
	return "?";
}

const char* Zenith_AnimControllerDocument::ParamTypeName(Flux_AnimationParameters::ParamType eType)
{
	switch (eType)
	{
	case Flux_AnimationParameters::ParamType::Float:   return "Float";
	case Flux_AnimationParameters::ParamType::Int:     return "Int";
	case Flux_AnimationParameters::ParamType::Bool:    return "Bool";
	case Flux_AnimationParameters::ParamType::Trigger: return "Trigger";
	}
	return "?";
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_AnimControllerDocument.Tests.inl"
#endif

#endif // ZENITH_TOOLS
