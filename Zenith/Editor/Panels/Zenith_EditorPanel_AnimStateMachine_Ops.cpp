#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.h"
// ★ THE DOPE SHEET IS REACHED FOR EXACTLY ONE THING (WU-7.2): telling its bone-
// mask sub-panel which layer's blend mode a mask assignment would land on. Both
// includes are in the .cpp, never the header — this panel's declarations need
// neither type, and pulling the dope sheet's document / preview session /
// pose-ring vocabulary into every TU that includes this header to reach one
// setter would be the wrong trade.
#include "Editor/Zenith_Editor.h"
#include "Editor/Panels/Zenith_EditorPanel_Animation.h"

#include <cstdio>

//=============================================================================
// The OPERATIONS half — and NOT ONE LINE OF ImGui in it.
//
// ★ THAT IS THE WHOLE POINT OF THE SPLIT, and it is the discipline the graph
// editor and the dope sheet both settled on. An Action_* reads no mouse, no
// modifier and no focus: every gesture-shaped input (which state, which
// transition index, which value) is a PARAMETER. So a unit with no frame open
// and an AddStep_AnimSm* recipe can perform exactly what a click performs,
// rather than synthesising input and discovering later that a failure could
// have been the click, the bridge, the hit rect or the operation.
//
// ★ EVERY MUTATION GOES THROUGH Zenith_AnimControllerDocument. Nothing here
// touches Flux_AnimatorControllerDef: the document is the only writer because
// every mutation has to mark dirty and push exactly one undo command in the
// same breath, and a caller that reached past it would skip both.
//=============================================================================

//=============================================================================
// Selection — pure panel state.
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::Action_SelectState(const std::string& strStateName)
{
	if (!m_xDocument.IsOpen() || !m_xDocument.HasState(strStateName))
	{
		return false;
	}
	m_strSelectedState = strStateName;
	m_bHasTransitionSelection = false;
	m_uSelectedTransition = uINVALID_ANIMSM_TRANSITION;
	m_strSelectedTransitionFrom.clear();
	snprintf(m_acStateNameBuffer, sizeof(m_acStateNameBuffer), "%s", strStateName.c_str());
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SelectTransition(const std::string& strFromState, u_int uIndex)
{
	if (!m_xDocument.IsOpen())
	{
		return false;
	}
	Flux_StateTransition xProbe;
	if (!m_xDocument.GetTransition(strFromState, uIndex, xProbe))
	{
		return false;
	}
	m_strSelectedTransitionFrom = strFromState;
	m_uSelectedTransition = uIndex;
	m_bHasTransitionSelection = true;
	m_strSelectedState.clear();
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_ClearSelection()
{
	const bool bHad = !m_strSelectedState.empty() || m_bHasTransitionSelection;
	m_strSelectedState.clear();
	m_strSelectedTransitionFrom.clear();
	m_uSelectedTransition = uINVALID_ANIMSM_TRANSITION;
	m_bHasTransitionSelection = false;
	return bHad;
}

bool Zenith_EditorPanel_AnimStateMachine::GetSelectedTransition(std::string& strOutFrom, u_int& uOutIndex) const
{
	if (!m_bHasTransitionSelection)
	{
		return false;
	}
	strOutFrom = m_strSelectedTransitionFrom;
	uOutIndex = m_uSelectedTransition;
	return true;
}

//=============================================================================
// Machine selection
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::Action_SelectLayerMachine(u_int uLayerId)
{
	if (!m_xDocument.SelectMachine(uLayerId))
	{
		return false;
	}
	// A state NAME means nothing in a different machine, and a transition index
	// means less than nothing.
	Action_ClearSelection();
	RebuildAutoLayout();
	RefreshHighlightedState();
	PushSelectedLayerBlendModeToMaskPanel();
	// The DETAIL block's fields follow the selection; the "+ Layer" field does
	// not, because it is about a layer that does not exist yet.
	snprintf(m_acLayerRenameBuffer, sizeof(m_acLayerRenameBuffer), "%s", CurrentLayerName().c_str());
	snprintf(m_acLayerMaskPathBuffer, sizeof(m_acLayerMaskPathBuffer), "%s", CurrentLayerMaskPath().c_str());
	return true;
}

//=============================================================================
// The layer list (WU-7.2)
//=============================================================================

u_int Zenith_EditorPanel_AnimStateMachine::GetSelectedLayerId() const
{
	return m_xDocument.GetSelectedMachineId();
}

const std::string& Zenith_EditorPanel_AnimStateMachine::GetLayerNotice() const
{
	return m_xDocument.GetLastLayerDiagnostic();
}

std::string Zenith_EditorPanel_AnimStateMachine::CurrentLayerName() const
{
	std::string strName;
	m_xDocument.GetLayerName(m_xDocument.GetSelectedMachineId(), strName);
	return strName;
}

std::string Zenith_EditorPanel_AnimStateMachine::CurrentLayerMaskPath() const
{
	std::string strPath;
	m_xDocument.GetLayerMaskAssetPath(m_xDocument.GetSelectedMachineId(), strPath);
	return strPath;
}

void Zenith_EditorPanel_AnimStateMachine::PushSelectedLayerBlendModeToMaskPanel()
{
	// ★ GUARDED, BECAUSE Instance() ASSERTS. It resolves the EDITOR-owned dope
	// sheet, and a unit builds this panel on the stack with no editor allocated
	// at all — an unguarded call would turn every such unit into an assert about
	// a panel the test never asked for.
	if (!g_xEngine.HasEditor())
	{
		return;
	}
	Zenith_EditorPanel_Animation* pxDopeSheet = g_xEngine.Editor().TryGetAnimationPanel();
	if (pxDopeSheet == nullptr)
	{
		return;
	}

	// ★ THE TOP-LEVEL MACHINE IS NOT A LAYER AND HAS NO BLEND MODE, so it pushes
	// OVERRIDE — the mode a mask means something to, and the sub-panel's own
	// default. Pushing "additive" for it would hide the mask control on a
	// selection that has nothing to do with layering.
	Flux_LayerBlendMode eMode = LAYER_BLEND_OVERRIDE;
	m_xDocument.GetLayerBlendMode(m_xDocument.GetSelectedMachineId(), eMode);
	pxDopeSheet->SetMaskTargetLayerBlendMode(eMode);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SelectLayer(u_int uLayerId)
{
	// The top-level machine is reached through Action_SelectLayerMachine; this
	// verb is the LIST's, and a row in it is always a layer.
	if (uLayerId == uANIMCTRL_TOP_LEVEL_MACHINE)
	{
		return false;
	}
	u_int uIndex = 0;
	if (!m_xDocument.GetLayerIndex(uLayerId, uIndex))
	{
		return false;
	}
	return Action_SelectLayerMachine(uLayerId);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_AddLayer(const std::string& strLayerName)
{
	const u_int uNewId = m_xDocument.AddLayer(strLayerName);
	if (uNewId == uFLUX_INVALID_LAYER_ID)
	{
		return false;
	}
	// The new layer becomes the canvas's machine, so the obvious next gesture —
	// give it a state — needs no second click. Same rule as Action_AddState.
	Action_SelectLayer(uNewId);
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RemoveLayer(u_int uLayerId)
{
	if (!m_xDocument.RemoveLayer(uLayerId))
	{
		return false;
	}
	// The document has already dropped a machine selection that pointed at the
	// removed layer; the panel's own state has to follow it, because a state name
	// and a transition index mean nothing in the machine it fell back to.
	Action_ClearSelection();
	RebuildAutoLayout();
	RefreshHighlightedState();
	PushSelectedLayerBlendModeToMaskPanel();
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RenameLayer(u_int uLayerId, const std::string& strLayerName)
{
	return m_xDocument.RenameLayer(uLayerId, strLayerName);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetLayerWeight(u_int uLayerId, float fWeight)
{
	return m_xDocument.SetLayerWeight(uLayerId, fWeight);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetLayerBlendMode(u_int uLayerId, Flux_LayerBlendMode eBlendMode)
{
	if (!m_xDocument.SetLayerBlendMode(uLayerId, eBlendMode))
	{
		return false;
	}
	// ★ THE MASK SUB-PANEL HAS TO HEAR ABOUT THIS, not just about a selection
	// change. Switching the SELECTED layer to additive without re-pushing would
	// leave WU-7.1's section still offering an assignment that the runtime will
	// never consult.
	if (uLayerId == m_xDocument.GetSelectedMachineId())
	{
		PushSelectedLayerBlendModeToMaskPanel();
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetLayerEmitEvents(u_int uLayerId, bool bEmitEvents)
{
	return m_xDocument.SetLayerEmitEvents(uLayerId, bEmitEvents);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetLayerMaskAssetPath(u_int uLayerId,
	const std::string& strAssetPath)
{
	if (!m_xDocument.SetLayerMaskAssetPath(uLayerId, strAssetPath))
	{
		// The refusal already carries its reason in the document's diagnostic,
		// which GetLayerNotice() forwards and the strip prints. Logged as well so
		// an authoring recipe's failure names the rule rather than the step.
		Zenith_Log(LOG_CATEGORY_EDITOR, "[AnimSM] layer %u: %s", uLayerId, GetLayerNotice().c_str());
		return false;
	}
	if (uLayerId == m_xDocument.GetSelectedMachineId())
	{
		snprintf(m_acLayerMaskPathBuffer, sizeof(m_acLayerMaskPathBuffer), "%s", CurrentLayerMaskPath().c_str());
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_MoveLayer(u_int uLayerId, u_int uNewIndex)
{
	// ★ THE SELECTION IS AN ID AND SURVIVES THE MOVE UNTOUCHED, which is the
	// whole reason WU-6.3 exists: reordering renumbers every index and moves no
	// identity, so there is nothing to repair here.
	return m_xDocument.MoveLayer(uLayerId, uNewIndex);
}

//=============================================================================
// States
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::Action_AddState(const std::string& strStateName)
{
	if (!m_xDocument.AddState(strStateName))
	{
		return false;
	}
	RebuildAutoLayout();
	Action_SelectState(strStateName);
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RemoveState(const std::string& strStateName)
{
	if (!m_xDocument.RemoveState(strStateName))
	{
		return false;
	}
	if (m_strSelectedState == strStateName)
	{
		Action_ClearSelection();
	}
	RebuildAutoLayout();
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RenameState(const std::string& strOldName,
	const std::string& strNewName)
{
	if (!m_xDocument.RenameState(strOldName, strNewName))
	{
		return false;
	}
	RebuildAutoLayout();
	if (m_strSelectedState == strOldName)
	{
		Action_SelectState(strNewName);
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetDefaultState(const std::string& strStateName)
{
	return m_xDocument.SetDefaultState(strStateName);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetStateClip(const std::string& strStateName,
	const std::string& strClipName)
{
	// ★ THE REFUSAL IS NAMED, NOT A BARE FALSE. A state whose tree is a blend
	// space or a sub-machine is WU-7.3's, and assigning a clip to it would
	// delete the whole sub-graph and report success.
	if (m_xDocument.GetStateTreeKind(strStateName) == ZENITH_ANIMCTRL_TREE_COMPLEX)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[AnimSM] '%s' is %s", strStateName.c_str(), BlendTreeRefusalText());
		return false;
	}
	return m_xDocument.SetStateClip(strStateName, strClipName);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetStatePosition(const std::string& strStateName,
	float fX, float fY)
{
	// ★ THE ORIGIN IS RESERVED. GetNodePosition reads exactly (0, 0) as "this
	// state has never been placed" and hands out an automatic grid slot instead,
	// so committing a drag that landed there would make the node jump to the
	// grid on the next frame — a drag that visibly refuses to stick. Nudging by
	// a hundredth of a unit costs nothing visible and keeps one meaning per
	// value.
	const float fSafeX = (fX == 0.0f && fY == 0.0f) ? 0.01f : fX;
	return m_xDocument.SetStateEditorPosition(strStateName, Zenith_Maths::Vector2(fSafeX, fY));
}

//=============================================================================
// Transitions
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::Action_AddTransition(const std::string& strFromState,
	const std::string& strToState)
{
	if (!m_xDocument.AddTransition(strFromState, strToState))
	{
		return false;
	}
	// The new one becomes the selection, so the obvious next gesture — give it a
	// condition — needs no second click.
	const u_int uCount = m_xDocument.GetTransitionCount(strFromState);
	if (uCount > 0)
	{
		Action_SelectTransition(strFromState, uCount - 1);
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RemoveTransition(const std::string& strFromState, u_int uIndex)
{
	if (!m_xDocument.RemoveTransition(strFromState, uIndex))
	{
		return false;
	}
	if (m_bHasTransitionSelection && m_strSelectedTransitionFrom == strFromState && m_uSelectedTransition == uIndex)
	{
		Action_ClearSelection();
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetTransitionDuration(const std::string& strFromState,
	u_int uIndex, float fSeconds)
{
	return m_xDocument.SetTransitionDuration(strFromState, uIndex, fSeconds);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetTransitionExitTime(const std::string& strFromState,
	u_int uIndex, bool bHasExitTime, float fNormalizedExitTime)
{
	return m_xDocument.SetTransitionExitTime(strFromState, uIndex, bHasExitTime, fNormalizedExitTime);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetTransitionInterruptible(const std::string& strFromState,
	u_int uIndex, bool bInterruptible)
{
	return m_xDocument.SetTransitionInterruptible(strFromState, uIndex, bInterruptible);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_AddCondition(const std::string& strFromState, u_int uIndex,
	const std::string& strParameterName, Flux_TransitionCondition::CompareOp eCompareOp, float fThreshold)
{
	return m_xDocument.AddCondition(strFromState, uIndex, strParameterName, eCompareOp, fThreshold);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RemoveCondition(const std::string& strFromState, u_int uIndex,
	u_int uConditionIndex)
{
	return m_xDocument.RemoveCondition(strFromState, uIndex, uConditionIndex);
}

//=============================================================================
// Parameters and clips
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::Action_AddParameter(const std::string& strName,
	Flux_AnimationParameters::ParamType eType, float fDefault)
{
	Zenith_AnimCtrlParameterDecl xDecl;
	switch (eType)
	{
	case Flux_AnimationParameters::ParamType::Float:
		xDecl = Zenith_AnimCtrlParameterDecl::Float(strName, fDefault);
		break;
	case Flux_AnimationParameters::ParamType::Int:
		xDecl = Zenith_AnimCtrlParameterDecl::Int(strName, static_cast<int32_t>(fDefault));
		break;
	case Flux_AnimationParameters::ParamType::Bool:
		xDecl = Zenith_AnimCtrlParameterDecl::Bool(strName, fDefault != 0.0f);
		break;
	case Flux_AnimationParameters::ParamType::Trigger:
		xDecl = Zenith_AnimCtrlParameterDecl::Trigger(strName);
		break;
	}
	return m_xDocument.AddParameter(xDecl);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RemoveParameter(const std::string& strName)
{
	return m_xDocument.RemoveParameter(strName);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_AddClipPath(const std::string& strAssetPath)
{
	return m_xDocument.AddClipPath(strAssetPath);
}

bool Zenith_EditorPanel_AnimStateMachine::Action_RemoveClipPath(const std::string& strAssetPath)
{
	return m_xDocument.RemoveClipPath(strAssetPath);
}

//=============================================================================
// Undo / save / apply
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::Action_Undo()
{
	if (!m_xDocument.CanUndo())
	{
		return false;
	}
	m_xDocument.Undo();
	// An undo can delete the selected state or the selected transition out from
	// under the inspector, and can change the state set the layout is built on.
	// It can also delete or re-add the selected LAYER (WU-7.2) — the document
	// drops a machine selection that no longer resolves, so the blend mode the
	// mask sub-panel is holding has to be re-pushed from whatever survived.
	RebuildAutoLayout();
	PushSelectedLayerBlendModeToMaskPanel();
	if (!m_strSelectedState.empty() && !m_xDocument.HasState(m_strSelectedState))
	{
		Action_ClearSelection();
	}
	if (m_bHasTransitionSelection)
	{
		Flux_StateTransition xProbe;
		if (!m_xDocument.GetTransition(m_strSelectedTransitionFrom, m_uSelectedTransition, xProbe))
		{
			Action_ClearSelection();
		}
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_Redo()
{
	if (!m_xDocument.CanRedo())
	{
		return false;
	}
	m_xDocument.Redo();
	RebuildAutoLayout();
	PushSelectedLayerBlendModeToMaskPanel();
	if (!m_strSelectedState.empty() && !m_xDocument.HasState(m_strSelectedState))
	{
		Action_ClearSelection();
	}
	if (m_bHasTransitionSelection)
	{
		Flux_StateTransition xProbe;
		if (!m_xDocument.GetTransition(m_strSelectedTransitionFrom, m_uSelectedTransition, xProbe))
		{
			Action_ClearSelection();
		}
	}
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_Save()
{
	m_eLastSaveResult = m_xDocument.Save();
	m_bExternalConflict = (m_eLastSaveResult == ZENITH_ANIMCTRLDOC_SAVE_CONFLICT_EXTERNAL);
	return m_eLastSaveResult == ZENITH_ANIMCTRLDOC_SAVE_OK;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_Apply()
{
	if (!m_xDocument.IsOpen())
	{
		return false;
	}
	if (!m_bPreviewEnabled)
	{
		// Nothing is running yet, so there is no playback to carry across — a
		// build IS the reload in that case, and refusing here would make the
		// first Apply of a session a no-op nobody expects.
		return Action_SetPreviewEnabled(true);
	}

	// ★ RELOAD, NOT REBUILD (D45). BuildFromControllerDef is a demolition: it
	// deletes every layer, resets every machine's runtime and puts every
	// blend-tree playhead back at zero, so the graph would snap to its default
	// state at frame 0 — which is precisely what makes editing a controller with
	// the preview running useless.
	m_bPreviewComplete = m_xPreviewController.ReloadFromControllerDef(m_xDocument.GetDef(), nullptr);
	m_xPreviewController.PublishSharedParameters();
	RefreshHighlightedState();
	return true;
}

//=============================================================================
// The preview
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::DropPreview()
{
	// ★ WHILE THE REGISTRY IS STILL ALIVE. A Flux_AnimationController holds one
	// owning AnimationHandle per clip the def named, and a handle released after
	// Zenith_AssetRegistry::Shutdown writes into freed memory — asserting only
	// when the freed word happens to read zero and corrupting silently otherwise.
	m_xPreviewController.ReleaseAssetReferences();
	m_bPreviewEnabled = false;
	m_bPreviewComplete = true;
	m_strHighlightedState.clear();
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetPreviewEnabled(bool bEnabled)
{
	if (!bEnabled)
	{
		DropPreview();
		return true;
	}
	if (!m_xDocument.IsOpen())
	{
		return false;
	}

	// No skeleton for the masks: a bone mask is skeleton-scoped and this preview
	// has no rig (see the header). A masked layer therefore builds UNMASKED and
	// BuildFromControllerDef reports it through the return value, which is what
	// IsPreviewComplete() surfaces rather than swallowing.
	m_bPreviewComplete = m_xPreviewController.BuildFromControllerDef(m_xDocument.GetDef(), nullptr);
	// Publication is LAZY on a real controller (first Update) and this preview
	// never calls Update — it drives the machine directly — so the bind has to be
	// asked for explicitly or every condition would read an empty parameter set.
	m_xPreviewController.PublishSharedParameters();
	m_bPreviewEnabled = true;
	RefreshHighlightedState();
	return m_bPreviewComplete;
}

Flux_AnimationStateMachine* Zenith_EditorPanel_AnimStateMachine::GetPreviewMachine()
{
	if (!m_bPreviewEnabled)
	{
		return nullptr;
	}
	const u_int uMachineId = m_xDocument.GetSelectedMachineId();
	if (uMachineId == uANIMCTRL_TOP_LEVEL_MACHINE)
	{
		return m_xPreviewController.HasStateMachine() ? &m_xPreviewController.GetStateMachine() : nullptr;
	}
	// ★ BY ID, RESOLVED PER USE (WU-6.3 / D44). A Flux_AnimationLayer* is valid
	// only until the next BuildFromControllerDef or ReloadFromControllerDef, both
	// of which delete every layer the controller owns — and Action_Apply calls
	// one of them. Caching the pointer would be a use-after-free with no assert
	// in front of it.
	Flux_AnimationLayer* pxLayer = m_xPreviewController.GetLayerById(uMachineId);
	if (pxLayer == nullptr || pxLayer->GetStateMachinePtr() == nullptr)
	{
		return nullptr;
	}
	return &pxLayer->GetStateMachine();
}

bool Zenith_EditorPanel_AnimStateMachine::Action_TickPreview(float fDtSeconds)
{
	Flux_AnimationStateMachine* pxMachine = GetPreviewMachine();
	if (pxMachine == nullptr)
	{
		return false;
	}
	if (!(fDtSeconds >= 0.0f) || fDtSeconds > 1.0e3f)
	{
		return false;
	}

	// ★ THE MACHINE, NOT THE CONTROLLER, AND AGAINST A ZERO-BONE RIG.
	// Flux_AnimationController::Update returns on its first line without a
	// Flux_SkeletonInstance, and this preview has none — but transition
	// evaluation, exit times, trigger consumption and every blend-tree playhead
	// live on the MACHINE and need only a Zenith_SkeletonAsset& to satisfy the
	// signature. A zero-bone one makes the pose a no-op and leaves every piece
	// of logic a highlight depends on running exactly as it does in a game.
	pxMachine->Update(fDtSeconds, m_xPreviewPose, m_xPreviewStubSkeleton);
	RefreshHighlightedState();
	return true;
}

void Zenith_EditorPanel_AnimStateMachine::RefreshHighlightedState()
{
	Flux_AnimationStateMachine* pxMachine = GetPreviewMachine();
	if (pxMachine == nullptr)
	{
		m_strHighlightedState.clear();
		return;
	}
	m_strHighlightedState = pxMachine->GetCurrentStateName();
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetPreviewFloat(const std::string& strName, float fValue)
{
	if (!m_bPreviewEnabled || strName.empty())
	{
		return false;
	}
	m_xPreviewController.SetFloat(strName, fValue);
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetPreviewInt(const std::string& strName, int32_t iValue)
{
	if (!m_bPreviewEnabled || strName.empty())
	{
		return false;
	}
	m_xPreviewController.SetInt(strName, iValue);
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetPreviewBool(const std::string& strName, bool bValue)
{
	if (!m_bPreviewEnabled || strName.empty())
	{
		return false;
	}
	m_xPreviewController.SetBool(strName, bValue);
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::Action_SetPreviewTrigger(const std::string& strName)
{
	if (!m_bPreviewEnabled || strName.empty())
	{
		return false;
	}
	m_xPreviewController.SetTrigger(strName);
	return true;
}

#endif // ZENITH_TOOLS
