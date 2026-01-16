#include "Zenith.h"
#include "Flux_AnimationStateMachine.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Core/Zenith_Core.h"
#include <algorithm>
#include <fstream>

//=============================================================================
// Flux_AnimationParameters
//=============================================================================
void Flux_AnimationParameters::AddFloat(const std::string& strName, float fDefault)
{
	Parameter param;
	param.m_eType = ParamType::Float;
	param.m_strName = strName;
	param.m_fValue = fDefault;
	m_xParameters[strName] = param;
}

void Flux_AnimationParameters::AddInt(const std::string& strName, int32_t iDefault)
{
	Parameter param;
	param.m_eType = ParamType::Int;
	param.m_strName = strName;
	param.m_iValue = iDefault;
	m_xParameters[strName] = param;
}

void Flux_AnimationParameters::AddBool(const std::string& strName, bool bDefault)
{
	Parameter param;
	param.m_eType = ParamType::Bool;
	param.m_strName = strName;
	param.m_bValue = bDefault;
	m_xParameters[strName] = param;
}

void Flux_AnimationParameters::AddTrigger(const std::string& strName)
{
	Parameter param;
	param.m_eType = ParamType::Trigger;
	param.m_strName = strName;
	param.m_bValue = false;
	m_xParameters[strName] = param;
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
		const Parameter& param = xPair.second;
		xStream << param.m_strName;
		xStream << static_cast<uint8_t>(param.m_eType);

		switch (param.m_eType)
		{
		case ParamType::Float:
			xStream << param.m_fValue;
			break;
		case ParamType::Int:
			xStream << param.m_iValue;
			break;
		case ParamType::Bool:
		case ParamType::Trigger:
			xStream << param.m_bValue;
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
		Parameter param;
		xStream >> param.m_strName;

		uint8_t uType = 0;
		xStream >> uType;

		Zenith_Assert(uType <= static_cast<uint8_t>(ParamType::Trigger), "AnimationParameters: Invalid param type %u for '%s' - skipping",
			uType, param.m_strName.c_str());
		param.m_eType = static_cast<ParamType>(uType);

		switch (param.m_eType)
		{
		case ParamType::Float:
			xStream >> param.m_fValue;
			break;
		case ParamType::Int:
			xStream >> param.m_iValue;
			break;
		case ParamType::Bool:
		case ParamType::Trigger:
			xStream >> param.m_bValue;
			break;
		}

		m_xParameters[param.m_strName] = param;
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
		// Triggers are handled specially - just check if it's true
		// The trigger will be consumed after evaluation
		// Note: This cast away const is intentional for trigger consumption
		return const_cast<Flux_AnimationParameters&>(xParams).ConsumeTrigger(m_strParameterName);
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
bool Flux_StateTransition::CanTransition(const Flux_AnimationParameters& xParams,
	float fCurrentNormalizedTime) const
{
	// Check exit time condition
	if (m_bHasExitTime && m_fExitTime >= 0.0f)
	{
		if (fCurrentNormalizedTime < m_fExitTime)
			return false;
	}

	// All conditions must be true (AND logic)
	for (Zenith_Vector<Flux_TransitionCondition>::Iterator xIt(m_xConditions); !xIt.Done(); xIt.Next())
	{
		if (!xIt.GetData().Evaluate(xParams))
			return false;
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

void Flux_AnimationState::RemoveTransition(size_t uIndex)
{
	if (uIndex < m_xTransitions.GetSize())
		m_xTransitions.Remove(static_cast<u_int>(uIndex));
}

const Flux_StateTransition* Flux_AnimationState::CheckTransitions(const Flux_AnimationParameters& xParams) const
{
	float fNormalizedTime = m_pxBlendTree ? m_pxBlendTree->GetNormalizedTime() : 0.0f;

	// Check transitions in priority order
	for (u_int i = 0; i < m_xTransitions.GetSize(); ++i)
	{
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

void Flux_AnimationStateMachine::SetState(const std::string& strStateName)
{
	Flux_AnimationState* pxNewState = GetState(strStateName);
	if (!pxNewState)
		return;

	// Call exit callback on old state
	if (m_pxCurrentState && m_pxCurrentState->m_fnOnExit)
		m_pxCurrentState->m_fnOnExit();

	// Cancel any active transition
	delete m_pxActiveTransition;
	m_pxActiveTransition = nullptr;
	m_pxTransitionTargetState = nullptr;

	// Set new state
	m_pxCurrentState = pxNewState;

	// Reset blend tree
	if (m_pxCurrentState->GetBlendTree())
		m_pxCurrentState->GetBlendTree()->Reset();

	// Call enter callback
	if (m_pxCurrentState->m_fnOnEnter)
		m_pxCurrentState->m_fnOnEnter();
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

	// Check for new transitions (only if not already transitioning)
	if (!m_pxActiveTransition)
	{
		const Flux_StateTransition* pxTransition = m_pxCurrentState->CheckTransitions(m_xParameters);
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
		}
		else
		{
			// Continue blending
			m_pxActiveTransition->Blend(xOutPose, m_xTargetPose);
			return;
		}
	}

	// Normal state update
	if (m_pxCurrentState->GetBlendTree())
	{
		m_pxCurrentState->GetBlendTree()->Evaluate(fDt, m_xCurrentPose, xSkeleton);
	}
	else
	{
		m_xCurrentPose.Reset();
	}

	// Call update callback
	if (m_pxCurrentState->m_fnOnUpdate)
		m_pxCurrentState->m_fnOnUpdate(fDt);

	xOutPose.CopyFrom(m_xCurrentPose);
}

void Flux_AnimationStateMachine::StartTransition(const Flux_StateTransition& xTransition)
{
	Flux_AnimationState* pxTargetState = GetState(xTransition.m_strTargetStateName);
	if (!pxTargetState)
		return;

	// Call exit callback on current state
	if (m_pxCurrentState && m_pxCurrentState->m_fnOnExit)
		m_pxCurrentState->m_fnOnExit();

	// Create transition
	delete m_pxActiveTransition;
	m_pxActiveTransition = new Flux_CrossFadeTransition();
	m_pxActiveTransition->Start(m_xCurrentPose, xTransition.m_fTransitionDuration);

	m_pxTransitionTargetState = pxTargetState;

	// Reset target blend tree
	if (m_pxTransitionTargetState->GetBlendTree())
		m_pxTransitionTargetState->GetBlendTree()->Reset();

	// Call enter callback on target state
	if (m_pxTransitionTargetState->m_fnOnEnter)
		m_pxTransitionTargetState->m_fnOnEnter();
}

void Flux_AnimationStateMachine::UpdateTransition(float fDt, const Zenith_SkeletonAsset& xSkeleton)
{
	if (!m_pxActiveTransition || !m_pxTransitionTargetState)
		return;

	// Update transition timer
	m_pxActiveTransition->Update(fDt);

	// Evaluate target state
	if (m_pxTransitionTargetState->GetBlendTree())
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

	// Copy target pose to current
	m_xCurrentPose.CopyFrom(m_xTargetPose);
}

void Flux_AnimationStateMachine::ResolveClipReferences(Flux_AnimationClipCollection* pxCollection)
{
	// Recursively resolve clip references in all blend trees
	for (auto& xPair : m_xStates)
	{
		Flux_BlendTreeNode* pxBlendTree = xPair.second->GetBlendTree();
		if (pxBlendTree)
		{
			// Use type name check instead of dynamic_cast to avoid RTTI issues
			// across compilation units
			if (strcmp(pxBlendTree->GetNodeTypeName(), "Clip") == 0)
			{
				Flux_BlendTreeNode_Clip* pxClipNode = static_cast<Flux_BlendTreeNode_Clip*>(pxBlendTree);
				pxClipNode->ResolveClip(pxCollection);
			}
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

	m_pxCurrentState = nullptr;
	m_pxTransitionTargetState = nullptr;
	delete m_pxActiveTransition;
	m_pxActiveTransition = nullptr;
}
