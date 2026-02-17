#include "Zenith.h"
#include "Flux_AnimationStateMachine.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Core/Zenith_Core.h"
#include <algorithm>
#include <fstream>

//=============================================================================
// Flux_AnimatorStateInfo
//=============================================================================

bool Flux_AnimatorStateInfo::IsName(const char* szName) const
{
	return m_strStateName == szName;
}

//=============================================================================
// Flux_AnimationParameters
//=============================================================================
void Flux_AnimationParameters::AddFloat(const std::string& strName, float fDefault)
{
	Parameter xParam;
	xParam.m_eType = ParamType::Float;
	xParam.m_strName = strName;
	xParam.m_fValue = fDefault;
	m_xParameters[strName] = xParam;
}

void Flux_AnimationParameters::AddInt(const std::string& strName, int32_t iDefault)
{
	Parameter xParam;
	xParam.m_eType = ParamType::Int;
	xParam.m_strName = strName;
	xParam.m_iValue = iDefault;
	m_xParameters[strName] = xParam;
}

void Flux_AnimationParameters::AddBool(const std::string& strName, bool bDefault)
{
	Parameter xParam;
	xParam.m_eType = ParamType::Bool;
	xParam.m_strName = strName;
	xParam.m_bValue = bDefault;
	m_xParameters[strName] = xParam;
}

void Flux_AnimationParameters::AddTrigger(const std::string& strName)
{
	Parameter xParam;
	xParam.m_eType = ParamType::Trigger;
	xParam.m_strName = strName;
	xParam.m_bValue = false;
	m_xParameters[strName] = xParam;
}

void Flux_AnimationParameters::SetFloat(const std::string& strName, float fValue)
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Float)
		it->second.m_fValue = fValue;
}

void Flux_AnimationParameters::SetInt(const std::string& strName, int32_t iValue)
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Int)
		it->second.m_iValue = iValue;
}

void Flux_AnimationParameters::SetBool(const std::string& strName, bool bValue)
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Bool)
		it->second.m_bValue = bValue;
}

void Flux_AnimationParameters::SetTrigger(const std::string& strName)
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Trigger)
		it->second.m_bValue = true;
}

float Flux_AnimationParameters::GetFloat(const std::string& strName) const
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Float)
		return it->second.m_fValue;
	return 0.0f;
}

int32_t Flux_AnimationParameters::GetInt(const std::string& strName) const
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Int)
		return it->second.m_iValue;
	return 0;
}

bool Flux_AnimationParameters::GetBool(const std::string& strName) const
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Bool)
		return it->second.m_bValue;
	return false;
}

bool Flux_AnimationParameters::PeekTrigger(const std::string& strName) const
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Trigger)
		return it->second.m_bValue;
	return false;
}

bool Flux_AnimationParameters::ConsumeTrigger(const std::string& strName)
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end() && it->second.m_eType == ParamType::Trigger)
	{
		bool bWasSet = it->second.m_bValue;
		it->second.m_bValue = false;
		return bWasSet;
	}
	return false;
}

bool Flux_AnimationParameters::HasParameter(const std::string& strName) const
{
	return m_xParameters.find(strName) != m_xParameters.end();
}

Flux_AnimationParameters::ParamType Flux_AnimationParameters::GetParameterType(const std::string& strName) const
{
	auto it = m_xParameters.find(strName);
	if (it != m_xParameters.end())
		return it->second.m_eType;
	return ParamType::Float;
}

void Flux_AnimationParameters::RemoveParameter(const std::string& strName)
{
	m_xParameters.erase(strName);
}

void Flux_AnimationParameters::ResetTriggers()
{
	for (auto& xPair : m_xParameters)
	{
		if (xPair.second.m_eType == ParamType::Trigger)
			xPair.second.m_bValue = false;
	}
}

void Flux_AnimationParameters::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumParams = static_cast<uint32_t>(m_xParameters.size());
	xStream << uNumParams;

	for (const auto& xPair : m_xParameters)
	{
		const Parameter& xParam = xPair.second;
		xStream << xParam.m_strName;
		xStream << static_cast<uint8_t>(xParam.m_eType);

		switch (xParam.m_eType)
		{
		case ParamType::Float:
			xStream << xParam.m_fValue;
			break;
		case ParamType::Int:
			xStream << xParam.m_iValue;
			break;
		case ParamType::Bool:
		case ParamType::Trigger:
			xStream << xParam.m_bValue;
			break;
		}
	}
}

void Flux_AnimationParameters::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xParameters.clear();

	uint32_t uNumParams = 0;
	xStream >> uNumParams;

	// Sanity check to prevent OOM from corrupted data
	constexpr uint32_t uMAX_PARAMS = 10000;
	Zenith_Assert(uNumParams <= uMAX_PARAMS,
		"AnimationParameters: Param count %u exceeds limit - possible corruption", uNumParams);
	if (uNumParams > uMAX_PARAMS) return;

	for (uint32_t i = 0; i < uNumParams; ++i)
	{
		Parameter xParam;
		xStream >> xParam.m_strName;

		uint8_t uType = 0;
		xStream >> uType;

		Zenith_Assert(uType <= static_cast<uint8_t>(ParamType::Trigger), "AnimationParameters: Invalid param type %u for '%s' - skipping",
			uType, xParam.m_strName.c_str());
		xParam.m_eType = static_cast<ParamType>(uType);

		switch (xParam.m_eType)
		{
		case ParamType::Float:
			xStream >> xParam.m_fValue;
			break;
		case ParamType::Int:
			xStream >> xParam.m_iValue;
			break;
		case ParamType::Bool:
		case ParamType::Trigger:
			xStream >> xParam.m_bValue;
			break;
		}

		m_xParameters[xParam.m_strName] = xParam;
	}
}

//=============================================================================
// Flux_TransitionCondition
//=============================================================================
bool Flux_TransitionCondition::Evaluate(const Flux_AnimationParameters& xParams) const
{
	if (!xParams.HasParameter(m_strParameterName))
		return false;

	switch (m_eParamType)
	{
	case Flux_AnimationParameters::ParamType::Float:
	{
		float fValue = xParams.GetFloat(m_strParameterName);
		switch (m_eCompareOp)
		{
		case CompareOp::Equal:        return fValue == m_fThreshold;
		case CompareOp::NotEqual:     return fValue != m_fThreshold;
		case CompareOp::Greater:      return fValue > m_fThreshold;
		case CompareOp::Less:         return fValue < m_fThreshold;
		case CompareOp::GreaterEqual: return fValue >= m_fThreshold;
		case CompareOp::LessEqual:    return fValue <= m_fThreshold;
		}
		break;
	}

	case Flux_AnimationParameters::ParamType::Int:
	{
		int32_t iValue = xParams.GetInt(m_strParameterName);
		switch (m_eCompareOp)
		{
		case CompareOp::Equal:        return iValue == m_iThreshold;
		case CompareOp::NotEqual:     return iValue != m_iThreshold;
		case CompareOp::Greater:      return iValue > m_iThreshold;
		case CompareOp::Less:         return iValue < m_iThreshold;
		case CompareOp::GreaterEqual: return iValue >= m_iThreshold;
		case CompareOp::LessEqual:    return iValue <= m_iThreshold;
		}
		break;
	}

	case Flux_AnimationParameters::ParamType::Bool:
	{
		bool bValue = xParams.GetBool(m_strParameterName);
		switch (m_eCompareOp)
		{
		case CompareOp::Equal:    return bValue == m_bThreshold;
		case CompareOp::NotEqual: return bValue != m_bThreshold;
		default: return bValue == m_bThreshold;
		}
		break;
	}

	case Flux_AnimationParameters::ParamType::Trigger:
	{
		// Only peek at trigger value - consumption happens in CanTransition
		// after ALL conditions pass, to avoid losing triggers on partial matches
		return xParams.PeekTrigger(m_strParameterName);
	}
	}

	return false;
}

void Flux_TransitionCondition::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strParameterName;
	xStream << static_cast<uint8_t>(m_eCompareOp);
	xStream << static_cast<uint8_t>(m_eParamType);

	switch (m_eParamType)
	{
	case Flux_AnimationParameters::ParamType::Float:
		xStream << m_fThreshold;
		break;
	case Flux_AnimationParameters::ParamType::Int:
		xStream << m_iThreshold;
		break;
	case Flux_AnimationParameters::ParamType::Bool:
	case Flux_AnimationParameters::ParamType::Trigger:
		xStream << m_bThreshold;
		break;
	}
}

void Flux_TransitionCondition::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strParameterName;

	uint8_t uOp = 0, uType = 0;
	xStream >> uOp;
	xStream >> uType;
	m_eCompareOp = static_cast<CompareOp>(uOp);
	m_eParamType = static_cast<Flux_AnimationParameters::ParamType>(uType);

	switch (m_eParamType)
	{
	case Flux_AnimationParameters::ParamType::Float:
		xStream >> m_fThreshold;
		break;
	case Flux_AnimationParameters::ParamType::Int:
		xStream >> m_iThreshold;
		break;
	case Flux_AnimationParameters::ParamType::Bool:
	case Flux_AnimationParameters::ParamType::Trigger:
		xStream >> m_bThreshold;
		break;
	}
}

//=============================================================================
// Flux_StateTransition
//=============================================================================
bool Flux_StateTransition::CanTransition(Flux_AnimationParameters& xParams,
	float fCurrentNormalizedTime) const
{
	// Check exit time condition
	if (m_bHasExitTime && m_fExitTime >= 0.0f)
	{
		if (fCurrentNormalizedTime < m_fExitTime)
			return false;
	}

	// Check all conditions (Evaluate now only peeks at triggers, doesn't consume)
	for (Zenith_Vector<Flux_TransitionCondition>::Iterator xIt(m_xConditions); !xIt.Done(); xIt.Next())
	{
		if (!xIt.GetData().Evaluate(xParams))
			return false;
	}

	// All conditions passed - now consume any triggers
	for (Zenith_Vector<Flux_TransitionCondition>::Iterator xIt(m_xConditions); !xIt.Done(); xIt.Next())
	{
		if (xIt.GetData().m_eParamType == Flux_AnimationParameters::ParamType::Trigger)
			xParams.ConsumeTrigger(xIt.GetData().m_strParameterName);
	}

	return true;
}

void Flux_StateTransition::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strTargetStateName;
	xStream << m_fTransitionDuration;
	xStream << m_fExitTime;
	xStream << m_bHasExitTime;
	xStream << m_bInterruptible;
	xStream << m_iPriority;

	uint32_t uNumConditions = m_xConditions.GetSize();
	xStream << uNumConditions;
	for (Zenith_Vector<Flux_TransitionCondition>::Iterator xIt(m_xConditions); !xIt.Done(); xIt.Next())
	{
		xIt.GetData().WriteToDataStream(xStream);
	}
}

void Flux_StateTransition::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strTargetStateName;
	xStream >> m_fTransitionDuration;
	xStream >> m_fExitTime;
	xStream >> m_bHasExitTime;
	xStream >> m_bInterruptible;
	xStream >> m_iPriority;

	uint32_t uNumConditions = 0;
	xStream >> uNumConditions;
	m_xConditions.Clear();
	m_xConditions.Reserve(uNumConditions);
	for (uint32_t i = 0; i < uNumConditions; ++i)
	{
		Flux_TransitionCondition xCondition;
		xCondition.ReadFromDataStream(xStream);
		m_xConditions.PushBack(xCondition);
	}
}

//=============================================================================
// Flux_AnimationState
//=============================================================================
Flux_AnimationState::Flux_AnimationState(const std::string& strName)
	: m_strName(strName)
{
}

Flux_AnimationState::~Flux_AnimationState()
{
	delete m_pxBlendTree;
	delete m_pxSubStateMachine;
}

Flux_AnimationStateMachine* Flux_AnimationState::CreateSubStateMachine(const std::string& strName)
{
	delete m_pxSubStateMachine;
	m_pxSubStateMachine = new Flux_AnimationStateMachine(strName);
	return m_pxSubStateMachine;
}

void Flux_AnimationState::AddTransition(const Flux_StateTransition& xTransition)
{
	m_xTransitions.PushBack(xTransition);

	// Sort by priority (higher first) using simple bubble sort
	for (u_int i = 0; i < m_xTransitions.GetSize(); ++i)
	{
		for (u_int j = i + 1; j < m_xTransitions.GetSize(); ++j)
		{
			if (m_xTransitions.Get(j).m_iPriority > m_xTransitions.Get(i).m_iPriority)
			{
				Flux_StateTransition xTemp = m_xTransitions.Get(i);
				m_xTransitions.Get(i) = m_xTransitions.Get(j);
				m_xTransitions.Get(j) = xTemp;
			}
		}
	}
}

void Flux_AnimationState::RemoveTransition(uint32_t uIndex)
{
	if (uIndex < m_xTransitions.GetSize())
		m_xTransitions.Remove(static_cast<u_int>(uIndex));
}

const Flux_StateTransition* Flux_AnimationState::CheckTransitions(Flux_AnimationParameters& xParams, int32_t iMinPriority) const
{
	float fNormalizedTime = m_pxBlendTree ? m_pxBlendTree->GetNormalizedTime() : 0.0f;

	// Check transitions in priority order (sorted highest first)
	for (u_int i = 0; i < m_xTransitions.GetSize(); ++i)
	{
		// Transitions are sorted by priority descending - stop early once below threshold
		if (m_xTransitions.Get(i).m_iPriority <= iMinPriority)
			break;

		if (m_xTransitions.Get(i).CanTransition(xParams, fNormalizedTime))
			return &m_xTransitions.Get(i);
	}

	return nullptr;
}

void Flux_AnimationState::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;

#ifdef ZENITH_TOOLS
	xStream << m_xEditorPosition.x;
	xStream << m_xEditorPosition.y;
#else
	xStream << 0.0f;
	xStream << 0.0f;  // Placeholder for tools data
#endif

	// Blend tree
	bool bHasBlendTree = (m_pxBlendTree != nullptr);
	xStream << bHasBlendTree;
	if (bHasBlendTree)
	{
		std::string strType = m_pxBlendTree->GetNodeTypeName();
		xStream << strType;
		m_pxBlendTree->WriteToDataStream(xStream);
	}

	// Sub-state machine
	bool bHasSubSM = (m_pxSubStateMachine != nullptr);
	xStream << bHasSubSM;
	if (bHasSubSM)
	{
		m_pxSubStateMachine->WriteToDataStream(xStream);
	}

	// Transitions
	uint32_t uNumTransitions = m_xTransitions.GetSize();
	xStream << uNumTransitions;
	for (Zenith_Vector<Flux_StateTransition>::Iterator xIt(m_xTransitions); !xIt.Done(); xIt.Next())
	{
		xIt.GetData().WriteToDataStream(xStream);
	}
}

void Flux_AnimationState::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strName;

	float fEditorX, fEditorY;
	xStream >> fEditorX;
	xStream >> fEditorY;
#ifdef ZENITH_TOOLS
	m_xEditorPosition = Zenith_Maths::Vector2(fEditorX, fEditorY);
#endif

	// Blend tree
	bool bHasBlendTree = false;
	xStream >> bHasBlendTree;
	if (bHasBlendTree)
	{
		std::string strType;
		xStream >> strType;
		m_pxBlendTree = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxBlendTree)
			m_pxBlendTree->ReadFromDataStream(xStream);
	}

	// Sub-state machine
	bool bHasSubSM = false;
	xStream >> bHasSubSM;
	if (bHasSubSM)
	{
		delete m_pxSubStateMachine;
		m_pxSubStateMachine = new Flux_AnimationStateMachine();
		m_pxSubStateMachine->ReadFromDataStream(xStream);
	}

	// Transitions
	uint32_t uNumTransitions = 0;
	xStream >> uNumTransitions;
	m_xTransitions.Clear();
	m_xTransitions.Reserve(uNumTransitions);
	for (uint32_t i = 0; i < uNumTransitions; ++i)
	{
		Flux_StateTransition xTransition;
		xTransition.ReadFromDataStream(xStream);
		m_xTransitions.PushBack(xTransition);
	}
}

//=============================================================================
// Flux_AnimationStateMachine
//=============================================================================
Flux_AnimationStateMachine::Flux_AnimationStateMachine(const std::string& strName)
	: m_strName(strName)
{
}

Flux_AnimationStateMachine::~Flux_AnimationStateMachine()
{
	for (auto& xPair : m_xStates)
		delete xPair.second;

	delete m_pxActiveTransition;
}

Flux_AnimationState* Flux_AnimationStateMachine::AddState(const std::string& strName)
{
	if (HasState(strName))
		return m_xStates[strName];

	Flux_AnimationState* pxState = new Flux_AnimationState(strName);
	m_xStates[strName] = pxState;

	// If this is the first state, make it the default
	if (m_strDefaultStateName.empty())
		m_strDefaultStateName = strName;

	return pxState;
}

void Flux_AnimationStateMachine::RemoveState(const std::string& strName)
{
	auto it = m_xStates.find(strName);
	if (it != m_xStates.end())
	{
		// Clear current state if removing it
		if (m_pxCurrentState == it->second)
			m_pxCurrentState = nullptr;

		delete it->second;
		m_xStates.erase(it);

		// Clear default if removing it
		if (m_strDefaultStateName == strName)
			m_strDefaultStateName.clear();
	}
}

Flux_AnimationState* Flux_AnimationStateMachine::GetState(const std::string& strName)
{
	auto it = m_xStates.find(strName);
	return (it != m_xStates.end()) ? it->second : nullptr;
}

const Flux_AnimationState* Flux_AnimationStateMachine::GetState(const std::string& strName) const
{
	auto it = m_xStates.find(strName);
	return (it != m_xStates.end()) ? it->second : nullptr;
}

bool Flux_AnimationStateMachine::HasState(const std::string& strName) const
{
	return m_xStates.find(strName) != m_xStates.end();
}

void Flux_AnimationStateMachine::SetDefaultState(const std::string& strName)
{
	if (HasState(strName))
		m_strDefaultStateName = strName;
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

void Flux_AnimationStateMachine::AddAnyStateTransition(const Flux_StateTransition& xTransition)
{
	// Insert sorted by priority (highest first) for deterministic evaluation
	bool bInserted = false;
	for (uint32_t i = 0; i < m_xAnyStateTransitions.GetSize(); ++i)
	{
		if (xTransition.m_iPriority > m_xAnyStateTransitions.Get(i).m_iPriority)
		{
			// Insert before this element
			m_xAnyStateTransitions.PushBack(xTransition);
			// Shift the new element into position
			for (uint32_t j = m_xAnyStateTransitions.GetSize() - 1; j > i; --j)
			{
				std::swap(m_xAnyStateTransitions.Get(j), m_xAnyStateTransitions.Get(j - 1));
			}
			bInserted = true;
			break;
		}
	}
	if (!bInserted)
	{
		m_xAnyStateTransitions.PushBack(xTransition);
	}
}

void Flux_AnimationStateMachine::RemoveAnyStateTransition(uint32_t uIndex)
{
	if (uIndex < m_xAnyStateTransitions.GetSize())
	{
		m_xAnyStateTransitions.Remove(static_cast<u_int>(uIndex));
	}
}

const Flux_StateTransition* Flux_AnimationStateMachine::CheckAnyStateTransitions(int32_t iMinPriority)
{
	if (!m_pxCurrentState)
		return nullptr;

	const std::string& strCurrentName = m_pxCurrentState->GetName();

	for (uint32_t i = 0; i < m_xAnyStateTransitions.GetSize(); ++i)
	{
		const Flux_StateTransition& xTrans = m_xAnyStateTransitions.Get(i);

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
	if (!m_pxCurrentState && !m_strDefaultStateName.empty())
	{
		SetState(m_strDefaultStateName);
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
	if (m_pxCurrentState->IsSubStateMachine())
	{
		// Delegate to child state machine
		Flux_AnimationStateMachine* pxSubSM = m_pxCurrentState->GetSubStateMachine();
		pxSubSM->Update(fDt, m_xCurrentPose, xSkeleton);
	}
	else if (m_pxCurrentState->GetBlendTree())
	{
		m_pxCurrentState->GetBlendTree()->Evaluate(fDt, m_xCurrentPose, xSkeleton);
	}
	else
	{
		m_xCurrentPose.Reset();
	}

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
	if (m_pxTransitionTargetState->IsSubStateMachine())
	{
		Flux_AnimationStateMachine* pxSubSM = m_pxTransitionTargetState->GetSubStateMachine();
		pxSubSM->Update(fDt, m_xTargetPose, xSkeleton);
	}
	else if (m_pxTransitionTargetState->GetBlendTree())
	{
		m_pxTransitionTargetState->GetBlendTree()->Evaluate(fDt, m_xTargetPose, xSkeleton);
	}
	else
	{
		m_xTargetPose.Reset();
	}
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

static void ResolveClipReferencesRecursive(Flux_BlendTreeNode* pxNode, Flux_AnimationClipCollection* pxCollection)
{
	if (!pxNode)
		return;

	const char* szType = pxNode->GetNodeTypeName();

	if (strcmp(szType, "Clip") == 0)
	{
		static_cast<Flux_BlendTreeNode_Clip*>(pxNode)->ResolveClip(pxCollection);
	}
	else if (strcmp(szType, "Blend") == 0)
	{
		Flux_BlendTreeNode_Blend* pxBlend = static_cast<Flux_BlendTreeNode_Blend*>(pxNode);
		ResolveClipReferencesRecursive(pxBlend->GetChildA(), pxCollection);
		ResolveClipReferencesRecursive(pxBlend->GetChildB(), pxCollection);
	}
	else if (strcmp(szType, "BlendSpace1D") == 0)
	{
		Flux_BlendTreeNode_BlendSpace1D* pxBS = static_cast<Flux_BlendTreeNode_BlendSpace1D*>(pxNode);
		const Zenith_Vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = pxBS->GetBlendPoints();
		for (uint32_t i = 0; i < xPoints.GetSize(); ++i)
			ResolveClipReferencesRecursive(xPoints.Get(i).m_pxNode, pxCollection);
	}
	else if (strcmp(szType, "BlendSpace2D") == 0)
	{
		Flux_BlendTreeNode_BlendSpace2D* pxBS = static_cast<Flux_BlendTreeNode_BlendSpace2D*>(pxNode);
		const Zenith_Vector<Flux_BlendTreeNode_BlendSpace2D::BlendPoint>& xPoints = pxBS->GetBlendPoints();
		for (uint32_t i = 0; i < xPoints.GetSize(); ++i)
			ResolveClipReferencesRecursive(xPoints.Get(i).m_pxNode, pxCollection);
	}
	else if (strcmp(szType, "Additive") == 0)
	{
		Flux_BlendTreeNode_Additive* pxAdditive = static_cast<Flux_BlendTreeNode_Additive*>(pxNode);
		ResolveClipReferencesRecursive(pxAdditive->GetBaseNode(), pxCollection);
		ResolveClipReferencesRecursive(pxAdditive->GetAdditiveNode(), pxCollection);
	}
	else if (strcmp(szType, "Masked") == 0)
	{
		Flux_BlendTreeNode_Masked* pxMasked = static_cast<Flux_BlendTreeNode_Masked*>(pxNode);
		ResolveClipReferencesRecursive(pxMasked->GetBaseNode(), pxCollection);
		ResolveClipReferencesRecursive(pxMasked->GetOverrideNode(), pxCollection);
	}
	else if (strcmp(szType, "Select") == 0)
	{
		Flux_BlendTreeNode_Select* pxSelect = static_cast<Flux_BlendTreeNode_Select*>(pxNode);
		const Zenith_Vector<Flux_BlendTreeNode*>& xChildren = pxSelect->GetChildren();
		for (uint32_t i = 0; i < xChildren.GetSize(); ++i)
			ResolveClipReferencesRecursive(xChildren.Get(i), pxCollection);
	}
}

void Flux_AnimationStateMachine::ResolveClipReferences(Flux_AnimationClipCollection* pxCollection)
{
	for (auto& xPair : m_xStates)
	{
		Flux_BlendTreeNode* pxBlendTree = xPair.second->GetBlendTree();
		if (pxBlendTree)
		{
			ResolveClipReferencesRecursive(pxBlendTree, pxCollection);
		}
	}
}

Flux_AnimationStateMachine* Flux_AnimationStateMachine::LoadFromFile(const std::string& strPath)
{
	std::ifstream xFile(strPath, std::ios::binary);
	if (!xFile.is_open())
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationStateMachine] Failed to open file: %s", strPath.c_str());
		return nullptr;
	}

	// Read file contents
	xFile.seekg(0, std::ios::end);
	size_t uSize = xFile.tellg();
	xFile.seekg(0, std::ios::beg);

	char* pBuffer = static_cast<char*>(Zenith_MemoryManagement::Allocate(uSize));
	xFile.read(pBuffer, uSize);
	xFile.close();

	Zenith_DataStream xStream(pBuffer, uSize);

	Flux_AnimationStateMachine* pxStateMachine = new Flux_AnimationStateMachine();
	pxStateMachine->ReadFromDataStream(xStream);

	Zenith_MemoryManagement::Deallocate(pBuffer);
	return pxStateMachine;
}

void Flux_AnimationStateMachine::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;
	xStream << m_strDefaultStateName;

	// Parameters
	m_xParameters.WriteToDataStream(xStream);

	// States
	uint32_t uNumStates = static_cast<uint32_t>(m_xStates.size());
	xStream << uNumStates;
	for (const auto& xPair : m_xStates)
	{
		xPair.second->WriteToDataStream(xStream);
	}

	// Any-state transitions
	uint32_t uNumAnyState = m_xAnyStateTransitions.GetSize();
	xStream << uNumAnyState;
	for (uint32_t i = 0; i < uNumAnyState; ++i)
	{
		m_xAnyStateTransitions.Get(i).WriteToDataStream(xStream);
	}
}

void Flux_AnimationStateMachine::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Clear existing data
	for (auto& xPair : m_xStates)
		delete xPair.second;
	m_xStates.clear();

	xStream >> m_strName;
	xStream >> m_strDefaultStateName;

	// Parameters
	m_xParameters.ReadFromDataStream(xStream);

	// States
	uint32_t uNumStates = 0;
	xStream >> uNumStates;
	for (uint32_t i = 0; i < uNumStates; ++i)
	{
		Flux_AnimationState* pxState = new Flux_AnimationState();
		pxState->ReadFromDataStream(xStream);
		m_xStates[pxState->GetName()] = pxState;
	}

	// Any-state transitions
	uint32_t uNumAnyState = 0;
	xStream >> uNumAnyState;
	m_xAnyStateTransitions.Clear();
	for (uint32_t i = 0; i < uNumAnyState; ++i)
	{
		Flux_StateTransition xTrans;
		xTrans.ReadFromDataStream(xStream);
		m_xAnyStateTransitions.PushBack(std::move(xTrans));
	}

	m_pxCurrentState = nullptr;
	m_pxTransitionTargetState = nullptr;
	delete m_pxActiveTransition;
	m_pxActiveTransition = nullptr;
}
