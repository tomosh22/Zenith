#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorAnimCtrlCommands.h"

bool Zenith_EditorAnimCtrlCommands_ForceLink()
{
	return true;
}

namespace
{
	// Remove every transition in xIn that targets strStateName. Returns true when
	// anything was dropped, so a caller can skip a write that changes nothing.
	bool PruneTransitionsTargeting(const Zenith_AnimCtrlTransitionList& xIn, const std::string& strStateName,
		Zenith_AnimCtrlTransitionList& xOut)
	{
		xOut.m_strOwnerStateName = xIn.m_strOwnerStateName;
		xOut.m_xTransitions.Clear();
		for (u_int u = 0; u < xIn.m_xTransitions.GetSize(); ++u)
		{
			if (xIn.m_xTransitions.Get(u).m_strTargetStateName != strStateName)
			{
				xOut.m_xTransitions.PushBack(xIn.m_xTransitions.Get(u));
			}
		}
		return xOut.m_xTransitions.GetSize() != xIn.m_xTransitions.GetSize();
	}
}

//==============================================================================
// Base
//==============================================================================

Zenith_AnimCtrlCommandBase::Zenith_AnimCtrlCommandBase(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const char* szDescription)
	: m_pxDocument(pxDocument)
	, m_uMachineId(uMachineId)
	, m_strDescription(szDescription != nullptr ? szDescription : "Edit Animator Controller")
{
	Zenith_Assert(m_pxDocument != nullptr, "Zenith_AnimCtrlCommandBase: null document");
}

//==============================================================================
// State add / remove / rename
//==============================================================================

Zenith_AnimCtrlCommand_StateAdd::Zenith_AnimCtrlCommand_StateAdd(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strStateName, const Zenith_Vector<char>& axStateBytes,
	const std::string& strPrevDefault)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, "Add State")
	, m_strStateName(strStateName)
	, m_axStateBytes(axStateBytes)
	, m_strPrevDefault(strPrevDefault)
{
}

void Zenith_AnimCtrlCommand_StateAdd::Execute()
{
	m_pxDocument->ApplyAddState(m_uMachineId, m_strStateName, &m_axStateBytes);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_StateAdd::Undo()
{
	m_pxDocument->ApplyRemoveState(m_uMachineId, m_strStateName);
	// ★ THE DEFAULT HAS TO BE PUT BACK EXPLICITLY. AddState makes the FIRST
	// state of a machine its default, so undoing that add would otherwise leave
	// the machine pointing at a state it no longer has — which
	// Flux_AnimationStateMachine::Update resolves to nullptr and answers with an
	// empty pose, silently.
	m_pxDocument->ApplySetDefaultState(m_uMachineId, m_strPrevDefault);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_StateRemove::Zenith_AnimCtrlCommand_StateRemove(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strStateName, const Zenith_Vector<char>& axStateBytes,
	const std::string& strPrevDefault, const Zenith_Vector<Zenith_AnimCtrlTransitionList>& axPrevLists)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, "Remove State")
	, m_strStateName(strStateName)
	, m_axStateBytes(axStateBytes)
	, m_strPrevDefault(strPrevDefault)
	, m_axPrevLists(axPrevLists)
{
}

void Zenith_AnimCtrlCommand_StateRemove::Execute()
{
	m_pxDocument->ApplyRemoveState(m_uMachineId, m_strStateName);

	// The inbound transitions go with it, re-derived from the lists as they
	// stand NOW rather than replayed from the capture: a redo runs after an undo
	// that restored them, and anything the user changed in between belongs to
	// them, not to this command.
	for (u_int u = 0; u < m_axPrevLists.GetSize(); ++u)
	{
		const std::string& strOwner = m_axPrevLists.Get(u).m_strOwnerStateName;
		if (strOwner == m_strStateName)
		{
			continue;
		}
		Zenith_AnimCtrlTransitionList xCurrent;
		if (!m_pxDocument->ReadTransitionList(m_uMachineId, strOwner, xCurrent))
		{
			continue;
		}
		Zenith_AnimCtrlTransitionList xPruned;
		if (PruneTransitionsTargeting(xCurrent, m_strStateName, xPruned))
		{
			m_pxDocument->ApplySetTransitions(m_uMachineId, xPruned);
		}
	}
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_StateRemove::Undo()
{
	m_pxDocument->ApplyAddState(m_uMachineId, m_strStateName, &m_axStateBytes);
	for (u_int u = 0; u < m_axPrevLists.GetSize(); ++u)
	{
		m_pxDocument->ApplySetTransitions(m_uMachineId, m_axPrevLists.Get(u));
	}
	m_pxDocument->ApplySetDefaultState(m_uMachineId, m_strPrevDefault);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_StateRename::Zenith_AnimCtrlCommand_StateRename(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strOldName, const std::string& strNewName)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, "Rename State")
	, m_strOldName(strOldName)
	, m_strNewName(strNewName)
{
}

void Zenith_AnimCtrlCommand_StateRename::Execute()
{
	m_pxDocument->ApplyRenameState(m_uMachineId, m_strOldName, m_strNewName);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_StateRename::Undo()
{
	m_pxDocument->ApplyRenameState(m_uMachineId, m_strNewName, m_strOldName);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_DefaultState::Zenith_AnimCtrlCommand_DefaultState(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strOldName, const std::string& strNewName)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, "Set Default State")
	, m_strOldName(strOldName)
	, m_strNewName(strNewName)
{
}

void Zenith_AnimCtrlCommand_DefaultState::Execute()
{
	m_pxDocument->ApplySetDefaultState(m_uMachineId, m_strNewName);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_DefaultState::Undo()
{
	m_pxDocument->ApplySetDefaultState(m_uMachineId, m_strOldName);
	m_pxDocument->MarkDirty();
}

//==============================================================================
// State clip / position
//==============================================================================

Zenith_AnimCtrlCommand_StateClip::Zenith_AnimCtrlCommand_StateClip(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strStateName, const std::string& strOldClip, const std::string& strNewClip)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, "Set State Clip")
	, m_strStateName(strStateName)
	, m_strOldClip(strOldClip)
	, m_strNewClip(strNewClip)
{
}

void Zenith_AnimCtrlCommand_StateClip::Execute()
{
	m_pxDocument->ApplySetStateClip(m_uMachineId, m_strStateName, m_strNewClip);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_StateClip::Undo()
{
	m_pxDocument->ApplySetStateClip(m_uMachineId, m_strStateName, m_strOldClip);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_StateTree::Zenith_AnimCtrlCommand_StateTree(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strStateName,
	const Zenith_Vector<char>& axOldBytes, const Zenith_Vector<char>& axNewBytes, const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, szDescription)
	, m_strStateName(strStateName)
	, m_axOldBytes(axOldBytes)
	, m_axNewBytes(axNewBytes)
{
}

void Zenith_AnimCtrlCommand_StateTree::Execute()
{
	m_pxDocument->ApplyRestoreState(m_uMachineId, m_strStateName, m_axNewBytes);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_StateTree::Undo()
{
	// ★ THE WHOLE PAYLOAD GOES BACK, which is what makes an undo of a CONVERSION
	// exact rather than approximate: a clip leaf that became a blend space lost
	// its playback rate and its playhead on the way, and there is nothing in the
	// space that could reconstruct them.
	m_pxDocument->ApplyRestoreState(m_uMachineId, m_strStateName, m_axOldBytes);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_StatePosition::Zenith_AnimCtrlCommand_StatePosition(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const std::string& strStateName,
	const Zenith_Maths::Vector2& xOld, const Zenith_Maths::Vector2& xNew)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, "Move State Node")
	, m_strStateName(strStateName)
	, m_xOld(xOld)
	, m_xNew(xNew)
{
}

void Zenith_AnimCtrlCommand_StatePosition::Execute()
{
	m_pxDocument->ApplySetStatePosition(m_uMachineId, m_strStateName, m_xNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_StatePosition::Undo()
{
	m_pxDocument->ApplySetStatePosition(m_uMachineId, m_strStateName, m_xOld);
	m_pxDocument->MarkDirty();
}

//==============================================================================
// Transition list / parameters / clip paths
//==============================================================================

Zenith_AnimCtrlCommand_Transitions::Zenith_AnimCtrlCommand_Transitions(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const Zenith_AnimCtrlTransitionList& xOld, const Zenith_AnimCtrlTransitionList& xNew,
	const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, szDescription)
	, m_xOld(xOld)
	, m_xNew(xNew)
{
}

void Zenith_AnimCtrlCommand_Transitions::Execute()
{
	m_pxDocument->ApplySetTransitions(m_uMachineId, m_xNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_Transitions::Undo()
{
	m_pxDocument->ApplySetTransitions(m_uMachineId, m_xOld);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_Parameters::Zenith_AnimCtrlCommand_Parameters(Zenith_AnimControllerDocument* pxDocument,
	u_int uMachineId, const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axOld,
	const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axNew, const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uMachineId, szDescription)
	, m_axOld(axOld)
	, m_axNew(axNew)
{
}

void Zenith_AnimCtrlCommand_Parameters::Execute()
{
	m_pxDocument->ApplySetParameters(m_uMachineId, m_axNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_Parameters::Undo()
{
	m_pxDocument->ApplySetParameters(m_uMachineId, m_axOld);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_ClipPaths::Zenith_AnimCtrlCommand_ClipPaths(Zenith_AnimControllerDocument* pxDocument,
	const Zenith_Vector<std::string>& axOld, const Zenith_Vector<std::string>& axNew, const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uANIMCTRL_TOP_LEVEL_MACHINE, szDescription)
	, m_axOld(axOld)
	, m_axNew(axNew)
{
}

void Zenith_AnimCtrlCommand_ClipPaths::Execute()
{
	m_pxDocument->ApplySetClipPaths(m_axNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_ClipPaths::Undo()
{
	m_pxDocument->ApplySetClipPaths(m_axOld);
	m_pxDocument->MarkDirty();
}

//==============================================================================
// The layer list (WU-7.2)
//==============================================================================

Zenith_AnimCtrlCommand_Layers::Zenith_AnimCtrlCommand_Layers(Zenith_AnimControllerDocument* pxDocument,
	const Zenith_Vector<Zenith_AnimCtrlLayerSnapshot>& axOld,
	const Zenith_Vector<Zenith_AnimCtrlLayerSnapshot>& axNew, const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uANIMCTRL_TOP_LEVEL_MACHINE, szDescription)
	, m_axOld(axOld)
	, m_axNew(axNew)
{
}

void Zenith_AnimCtrlCommand_Layers::Execute()
{
	m_pxDocument->ApplySetLayers(m_axNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_Layers::Undo()
{
	// ★ THE EXACT PREVIOUS ORDER, id for id and payload for payload. That is the
	// property a reorder needs and the one an "unmove it back" inverse would only
	// have for a single move: two moves in a row, or a move after a remove, put
	// the destination index somewhere the second inverse can no longer name.
	m_pxDocument->ApplySetLayers(m_axOld);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCtrlCommand_LayerFields::Zenith_AnimCtrlCommand_LayerFields(Zenith_AnimControllerDocument* pxDocument,
	u_int uLayerId, const Zenith_AnimCtrlLayerFields& xOld, const Zenith_AnimCtrlLayerFields& xNew,
	const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uANIMCTRL_TOP_LEVEL_MACHINE, szDescription)
	, m_uLayerId(uLayerId)
	, m_xOld(xOld)
	, m_xNew(xNew)
{
}

void Zenith_AnimCtrlCommand_LayerFields::Execute()
{
	m_pxDocument->ApplySetLayerFields(m_uLayerId, m_xNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCtrlCommand_LayerFields::Undo()
{
	m_pxDocument->ApplySetLayerFields(m_uLayerId, m_xOld);
	m_pxDocument->MarkDirty();
}

//==============================================================================
// Compound
//==============================================================================

Zenith_AnimCtrlCommand_Compound::Zenith_AnimCtrlCommand_Compound(Zenith_AnimControllerDocument* pxDocument,
	const char* szDescription)
	: Zenith_AnimCtrlCommandBase(pxDocument, uANIMCTRL_TOP_LEVEL_MACHINE, szDescription)
{
}

Zenith_AnimCtrlCommand_Compound::~Zenith_AnimCtrlCommand_Compound()
{
	for (u_int u = 0; u < m_apxChildren.GetSize(); ++u)
	{
		delete m_apxChildren.Get(u);
	}
	m_apxChildren.Clear();
}

void Zenith_AnimCtrlCommand_Compound::Adopt(Zenith_UndoCommand* pxChild)
{
	if (pxChild == nullptr)
	{
		return;
	}
	m_apxChildren.PushBack(pxChild);
}

void Zenith_AnimCtrlCommand_Compound::SetDescription(const char* szDescription)
{
	if (szDescription != nullptr)
	{
		m_strDescription = szDescription;
	}
}

void Zenith_AnimCtrlCommand_Compound::Execute()
{
	for (u_int u = 0; u < m_apxChildren.GetSize(); ++u)
	{
		m_apxChildren.Get(u)->Execute();
	}
}

void Zenith_AnimCtrlCommand_Compound::Undo()
{
	// ★ REVERSE, and that is not cosmetic: the children were applied in an order
	// whose intermediate states are each legal, and the exact inverse of that
	// order is the only order whose intermediates are equally legal. Undoing
	// "add state, add transition to it" forwards would try to remove a state
	// something still points at.
	for (u_int u = m_apxChildren.GetSize(); u > 0; --u)
	{
		m_apxChildren.Get(u - 1)->Undo();
	}
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_EditorAnimCtrlCommands.Tests.inl"
#endif

#endif // ZENITH_TOOLS
