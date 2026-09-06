#include "Zenith.h"
#include "Flux_AnimationStateMachine.h"
#include "Flux_AnimationClip.h"

//=============================================================================
// Flux_AnimatorStateInfo
//=============================================================================

bool Flux_AnimatorStateInfo::IsName(const char* szName) const
{
	return m_strStateName == szName;
}

//=============================================================================
// Flux_AnimationStateMachine
//=============================================================================
Flux_AnimationStateMachine::Flux_AnimationStateMachine(const std::string& strName)
	: m_xDef(strName)
{
}

Flux_AnimationStateMachine::~Flux_AnimationStateMachine()
{
	// The states (and their blend trees and nested machines) belong to the def.
	delete m_pxActiveTransition;
}

void Flux_AnimationStateMachine::ResetRuntime()
{
	m_pxCurrentState = nullptr;
	m_pxTransitionTargetState = nullptr;
	delete m_pxActiveTransition;
	m_pxActiveTransition = nullptr;
	m_bActiveTransitionInterruptible = true;
	m_iActiveTransitionPriority = 0;
}

void Flux_AnimationStateMachine::BuildFromDef(const Flux_AnimationStateMachineDef& xDef,
	Flux_AnimationClipCollection* pxClipCollection)
{
	// ★ THE RUNTIME POINTERS GO FIRST. Every one of them points INTO the def
	// about to be replaced, and CopyFrom deletes the states they name.
	ResetRuntime();

	m_xDef.CopyFrom(xDef);

	if (pxClipCollection)
		m_xDef.ResolveClipReferences(pxClipCollection);
}

void Flux_AnimationStateMachine::RemoveState(const std::string& strName)
{
	Flux_AnimationState* pxState = m_xDef.GetState(strName);
	if (!pxState)
		return;

	// The runtime half points into the def, so a removed state has to be let go
	// of here before the def frees it — including as a transition TARGET, which
	// the pre-WU-6.1 version left dangling.
	if (m_pxCurrentState == pxState)
		m_pxCurrentState = nullptr;

	if (m_pxTransitionTargetState == pxState)
	{
		m_pxTransitionTargetState = nullptr;
		delete m_pxActiveTransition;
		m_pxActiveTransition = nullptr;
	}

	m_xDef.RemoveState(strName);
}

const std::string& Flux_AnimationStateMachine::GetCurrentStateName() const
{
	static std::string s_strEmpty;
	return m_pxCurrentState ? m_pxCurrentState->GetName() : s_strEmpty;
}

Flux_AnimatorStateInfo Flux_AnimationStateMachine::GetCurrentStateInfo() const
{
	Flux_AnimatorStateInfo xInfo;

	if (!m_pxCurrentState)
		return xInfo;

	xInfo.m_strStateName = m_pxCurrentState->GetName();

	Flux_BlendTreeNode* pxBlendTree = m_pxCurrentState->GetBlendTree();
	if (pxBlendTree)
	{
		xInfo.m_fNormalizedTime = pxBlendTree->GetNormalizedTime();
		xInfo.m_bHasLooped = !pxBlendTree->IsFinished() && pxBlendTree->GetNormalizedTime() > 1.0f;
	}

	xInfo.m_bIsTransitioning = (m_pxActiveTransition != nullptr);
	if (m_pxActiveTransition)
	{
		xInfo.m_fTransitionProgress = m_pxActiveTransition->GetBlendWeight();
	}

	return xInfo;
}

//=============================================================================
// Any-State Transitions
//=============================================================================

const Flux_StateTransition* Flux_AnimationStateMachine::CheckAnyStateTransitions(int32_t iMinPriority)
{
	if (!m_pxCurrentState)
		return nullptr;

	const std::string& strCurrentName = m_pxCurrentState->GetName();
	const Zenith_Vector<Flux_StateTransition>& xAnyState = m_xDef.GetAnyStateTransitions();

	for (uint32_t i = 0; i < xAnyState.GetSize(); ++i)
	{
		const Flux_StateTransition& xTrans = xAnyState.Get(i);

		// Any-state transitions are sorted by priority descending - stop early once below threshold
		if (xTrans.m_iPriority <= iMinPriority)
			break;

		// Skip transitions targeting the current state (prevents self-loops)
		if (xTrans.m_strTargetStateName == strCurrentName)
			continue;

		// Get normalized time from current state's blend tree
		float fNormalizedTime = 0.0f;
		if (m_pxCurrentState->GetBlendTree())
			fNormalizedTime = m_pxCurrentState->GetBlendTree()->GetNormalizedTime();

		if (xTrans.CanTransition(GetParameters(), fNormalizedTime))
			return &xTrans;
	}

	return nullptr;
}

void Flux_AnimationStateMachine::CrossFade(const std::string& strStateName, float fDuration)
{
	if (!HasState(strStateName))
		return;

	// If we're already in this state and not transitioning, do nothing
	if (m_pxCurrentState && m_pxCurrentState->GetName() == strStateName && !m_pxActiveTransition)
		return;

	// Create a synthetic transition with no conditions
	Flux_StateTransition xTransition;
	xTransition.m_strTargetStateName = strStateName;
	xTransition.m_fTransitionDuration = fDuration;
	xTransition.m_bHasExitTime = false;
	xTransition.m_bInterruptible = true;

	StartTransition(xTransition);
}

void Flux_AnimationStateMachine::SetState(const std::string& strStateName)
{
	Flux_AnimationState* pxNewState = GetState(strStateName);
	if (!pxNewState)
		return;

	// Call exit callback on old state
	if (m_pxCurrentState && m_pxCurrentState->m_pfnOnExit)
		m_pxCurrentState->m_pfnOnExit(m_pxCurrentState->m_pCallbackUserData);

	// Cancel any active transition
	delete m_pxActiveTransition;
	m_pxActiveTransition = nullptr;
	m_pxTransitionTargetState = nullptr;

	// Set new state
	m_pxCurrentState = pxNewState;

	// Reset blend tree or sub-state machine
	if (m_pxCurrentState->IsSubStateMachine())
	{
		Flux_AnimationStateMachine* pxSubSM = m_pxCurrentState->GetSubStateMachine();
		pxSubSM->SetSharedParameters(&GetParameters());
		pxSubSM->SetState(pxSubSM->GetDefaultStateName());
	}
	else if (m_pxCurrentState->GetBlendTree())
	{
		m_pxCurrentState->GetBlendTree()->Reset();
	}

	// Call enter callback
	if (m_pxCurrentState->m_pfnOnEnter)
		m_pxCurrentState->m_pfnOnEnter(m_pxCurrentState->m_pCallbackUserData);
}

void Flux_AnimationStateMachine::Update(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	// Initialize to default state if needed
	if (!m_pxCurrentState && !GetDefaultStateName().empty())
	{
		SetState(GetDefaultStateName());
	}

	if (!m_pxCurrentState)
	{
		xOutPose.Reset();
		return;
	}

	// Check for new transitions (when not transitioning, or when active transition is interruptible)
	if (!m_pxActiveTransition || m_bActiveTransitionInterruptible)
	{
		// Pre-filter by active transition priority so triggers are never consumed
		// for transitions that can't actually start (prevents lost triggers)
		int32_t iMinPriority = m_pxActiveTransition ? m_iActiveTransitionPriority : INT32_MIN;

		// Any-state transitions are checked first (highest priority)
		const Flux_StateTransition* pxTransition = CheckAnyStateTransitions(iMinPriority);

		// Fall back to per-state transitions
		if (!pxTransition)
			pxTransition = m_pxCurrentState->CheckTransitions(GetParameters(), iMinPriority);

		if (pxTransition)
		{
			StartTransition(*pxTransition);
		}
	}

	// Update transition if active
	if (m_pxActiveTransition)
	{
		UpdateTransition(fDt, xSkeleton);

		if (m_pxActiveTransition->IsComplete())
		{
			CompleteTransition();
			// Target pose was already evaluated in UpdateTransition() - use it directly
			// (falling through to normal state update would double-advance the blend tree)
			xOutPose.CopyFrom(m_xCurrentPose);
			return;
		}
		else
		{
			// Continue blending
			m_pxActiveTransition->Blend(xOutPose, m_xTargetPose);
			return;
		}
	}

	// Normal state update
	EvaluateState(m_pxCurrentState, fDt, m_xCurrentPose, xSkeleton);

	// Call update callback
	if (m_pxCurrentState->m_pfnOnUpdate)
		m_pxCurrentState->m_pfnOnUpdate(m_pxCurrentState->m_pCallbackUserData, fDt);

	xOutPose.CopyFrom(m_xCurrentPose);
}

void Flux_AnimationStateMachine::StartTransition(const Flux_StateTransition& xTransition)
{
	Flux_AnimationState* pxTargetState = GetState(xTransition.m_strTargetStateName);
	if (!pxTargetState)
		return;

	// Call exit callback on current state
	if (m_pxCurrentState && m_pxCurrentState->m_pfnOnExit)
		m_pxCurrentState->m_pfnOnExit(m_pxCurrentState->m_pCallbackUserData);

	// Create transition
	delete m_pxActiveTransition;
	m_pxActiveTransition = new Flux_CrossFadeTransition();
	m_pxActiveTransition->Start(m_xCurrentPose, xTransition.m_fTransitionDuration);

	m_pxTransitionTargetState = pxTargetState;
	m_bActiveTransitionInterruptible = xTransition.m_bInterruptible;
	m_iActiveTransitionPriority = xTransition.m_iPriority;

	// Reset target blend tree or sub-state machine
	if (m_pxTransitionTargetState->IsSubStateMachine())
	{
		Flux_AnimationStateMachine* pxSubSM = m_pxTransitionTargetState->GetSubStateMachine();
		pxSubSM->SetSharedParameters(&GetParameters());
		pxSubSM->SetState(pxSubSM->GetDefaultStateName());
	}
	else if (m_pxTransitionTargetState->GetBlendTree())
	{
		m_pxTransitionTargetState->GetBlendTree()->Reset();
	}

	// Call enter callback on target state
	if (m_pxTransitionTargetState->m_pfnOnEnter)
		m_pxTransitionTargetState->m_pfnOnEnter(m_pxTransitionTargetState->m_pCallbackUserData);
}

void Flux_AnimationStateMachine::UpdateTransition(float fDt, const Zenith_SkeletonAsset& xSkeleton)
{
	if (!m_pxActiveTransition || !m_pxTransitionTargetState)
		return;

	// Update transition timer
	m_pxActiveTransition->Update(fDt);

	// Evaluate target state
	EvaluateState(m_pxTransitionTargetState, fDt, m_xTargetPose, xSkeleton);
}

void Flux_AnimationStateMachine::EvaluateState(Flux_AnimationState* pxState, float fDt, Flux_SkeletonPose& xOutPose, const Zenith_SkeletonAsset& xSkeleton)
{
	if (pxState->IsSubStateMachine())
	{
		Flux_AnimationStateMachine* pxSubSM = pxState->GetSubStateMachine();
		pxSubSM->Update(fDt, xOutPose, xSkeleton);
	}
	else if (pxState->GetBlendTree())
	{
		Flux_BlendTreeNode* pxTree = pxState->GetBlendTree();

		// ★ D48 — THE BLEND POSITION IS READ FROM THE NAMED PARAMETER, EVERY
		// EVALUATE. Until WU-6.1 nothing passed Flux_AnimationParameters into a
		// blend tree at all: this call site read `pxState->GetBlendTree()->
		// Evaluate(fDt, xOutPose, xSkeleton)` and Flux_BlendTreeNode::Evaluate has
		// no parameter argument, so a 1D/2D blend space in a running game sat
		// frozen at whatever literal it was deserialized with. The only thing that
		// could move it was SetParameter, which nothing outside the unit tests
		// ever called. This is the same late-binding shape the file already uses
		// for clips (m_strClipName + ResolveClip): a NAME in the def, resolved
		// against the live set here.
		pxTree->ResolveParameters(GetParameters());

		// WU-5A (D34): the ROOT of a state's tree carries the whole of the
		// state's contribution; every fraction below it is applied by the
		// composites on the way down.
		pxTree->SetEvalWeight(1.0f);
		pxTree->Evaluate(fDt, xOutPose, xSkeleton);
	}
	else
	{
		xOutPose.Reset();
	}
}

//=============================================================================
// Event-span collection (WU-5A) — see the header for D37.
//=============================================================================

void Flux_AnimationStateMachine::CollectStateEventSpans(Flux_AnimationState* pxState,
	Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	if (!pxState)
		return;

	if (pxState->IsSubStateMachine())
	{
		Flux_AnimationStateMachine* pxSubSM = pxState->GetSubStateMachine();
		if (pxSubSM)
			pxSubSM->CollectEventSpans(pxOutSpans);
		return;
	}

	if (pxState->GetBlendTree())
		pxState->GetBlendTree()->CollectEventSpans(pxOutSpans);
}

void Flux_AnimationStateMachine::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	if (m_pxActiveTransition && m_pxTransitionTargetState)
	{
		// D37: only the side at weight >= 0.5 may emit; a dead-even 0.5 goes to
		// the TARGET. The other side is still WALKED, with a null sink, so a
		// span it produced this frame is cleared rather than saved up to fire
		// the moment the weights cross.
		const float fTargetWeight = m_pxActiveTransition->GetBlendWeight();
		Flux_AnimationState* pxEmitter = fTargetWeight >= 0.5f ? m_pxTransitionTargetState : m_pxCurrentState;
		Flux_AnimationState* pxSilent = fTargetWeight >= 0.5f ? m_pxCurrentState : m_pxTransitionTargetState;

		// ★ CLEAR THE SILENT SIDE FIRST, AND ONLY IF IT IS A DIFFERENT STATE. A
		// self-transition has one state on both sides and ONE set of leaves;
		// clearing it as "the loser" would drop the very span we are about to
		// ask it for.
		if (pxSilent != pxEmitter)
			CollectStateEventSpans(pxSilent, nullptr);
		CollectStateEventSpans(pxEmitter, pxOutSpans);
		return;
	}

	CollectStateEventSpans(m_pxCurrentState, pxOutSpans);
}

void Flux_AnimationStateMachine::CompleteTransition()
{
	if (!m_pxTransitionTargetState)
		return;

	m_pxCurrentState = m_pxTransitionTargetState;
	m_pxTransitionTargetState = nullptr;

	delete m_pxActiveTransition;
	m_pxActiveTransition = nullptr;
	m_bActiveTransitionInterruptible = true;
	m_iActiveTransitionPriority = 0;

	// Copy target pose to current
	m_xCurrentPose.CopyFrom(m_xTargetPose);
}

//=============================================================================
// Serialization — the DEF is the payload; the runtime half is not data.
//=============================================================================

void Flux_AnimationStateMachine::WriteToDataStream(Zenith_DataStream& xStream) const
{
	m_xDef.WriteToDataStream(xStream);
}

void Flux_AnimationStateMachine::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// The runtime pointers name states this read is about to delete.
	ResetRuntime();
	m_xDef.ReadFromDataStream(xStream);
}

#ifdef ZENITH_TESTING
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.Tests.inl"
#endif
