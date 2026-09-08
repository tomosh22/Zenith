#include "Zenith.h"
#include "Flux_AnimationStateMachineDef.h"
#include "Flux_AnimationStateMachine.h"
#include <algorithm>

//=============================================================================
// File-local helpers
//=============================================================================

template<typename T>
static bool CompareNumericValues(T tValue, T tThreshold, Flux_TransitionCondition::CompareOp eOp)
{
	switch (eOp)
	{
	case Flux_TransitionCondition::CompareOp::Equal:        return tValue == tThreshold;
	case Flux_TransitionCondition::CompareOp::NotEqual:     return tValue != tThreshold;
	case Flux_TransitionCondition::CompareOp::Greater:      return tValue > tThreshold;
	case Flux_TransitionCondition::CompareOp::Less:         return tValue < tThreshold;
	case Flux_TransitionCondition::CompareOp::GreaterEqual: return tValue >= tThreshold;
	case Flux_TransitionCondition::CompareOp::LessEqual:    return tValue <= tThreshold;
	}
	return false;
}

static void InsertTransitionSortedByPriority(Zenith_Vector<Flux_StateTransition>& xTransitions, const Flux_StateTransition& xTransition)
{
	uint32_t uInsertIdx = xTransitions.GetSize();
	for (uint32_t i = 0; i < xTransitions.GetSize(); ++i)
	{
		if (xTransition.m_iPriority > xTransitions.Get(i).m_iPriority)
		{
			uInsertIdx = i;
			break;
		}
	}

	xTransitions.PushBack(xTransition);
	for (uint32_t j = xTransitions.GetSize() - 1; j > uInsertIdx; --j)
	{
		std::swap(xTransitions.Get(j), xTransitions.Get(j - 1));
	}
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
	Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Float)
		pxParam->m_fValue = fValue;
}

void Flux_AnimationParameters::SetInt(const std::string& strName, int32_t iValue)
{
	Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Int)
		pxParam->m_iValue = iValue;
}

void Flux_AnimationParameters::SetBool(const std::string& strName, bool bValue)
{
	Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Bool)
		pxParam->m_bValue = bValue;
}

void Flux_AnimationParameters::SetTrigger(const std::string& strName)
{
	Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Trigger)
		pxParam->m_bValue = true;
}

float Flux_AnimationParameters::GetFloat(const std::string& strName) const
{
	const Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Float)
		return pxParam->m_fValue;
	return 0.0f;
}

int32_t Flux_AnimationParameters::GetInt(const std::string& strName) const
{
	const Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Int)
		return pxParam->m_iValue;
	return 0;
}

bool Flux_AnimationParameters::GetBool(const std::string& strName) const
{
	const Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Bool)
		return pxParam->m_bValue;
	return false;
}

bool Flux_AnimationParameters::PeekTrigger(const std::string& strName) const
{
	const Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Trigger)
		return pxParam->m_bValue;
	return false;
}

bool Flux_AnimationParameters::ConsumeTrigger(const std::string& strName)
{
	Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam && pxParam->m_eType == ParamType::Trigger)
	{
		bool bWasSet = pxParam->m_bValue;
		pxParam->m_bValue = false;
		return bWasSet;
	}
	return false;
}

bool Flux_AnimationParameters::HasParameter(const std::string& strName) const
{
	return m_xParameters.Contains(strName);
}

Flux_AnimationParameters::ParamType Flux_AnimationParameters::GetParameterType(const std::string& strName) const
{
	const Parameter* pxParam = m_xParameters.TryGet(strName);
	if (pxParam)
		return pxParam->m_eType;
	return ParamType::Float;
}

void Flux_AnimationParameters::RemoveParameter(const std::string& strName)
{
	m_xParameters.Remove(strName);
}

void Flux_AnimationParameters::Clear()
{
	m_xParameters.Clear();
}

void Flux_AnimationParameters::SeedInto(Flux_AnimationParameters& xOutLive) const
{
	if (&xOutLive == this)
		return;

	for (Zenith_HashMap<std::string, Parameter>::Iterator xIt(m_xParameters); !xIt.Done(); xIt.Next())
	{
		const Parameter& xParam = xIt.GetValue();
		// ★ NEVER OVERWRITE. Two layers on one controller commonly declare the
		// same "Speed"; re-seeding on the second would snap the live value back
		// to a default mid-play, which reads as a one-frame animation glitch and
		// points at nothing.
		if (xOutLive.HasParameter(xParam.m_strName))
			continue;

		switch (xParam.m_eType)
		{
		case ParamType::Float:   xOutLive.AddFloat(xParam.m_strName, xParam.m_fValue); break;
		case ParamType::Int:     xOutLive.AddInt(xParam.m_strName, xParam.m_iValue);   break;
		case ParamType::Bool:    xOutLive.AddBool(xParam.m_strName, xParam.m_bValue);  break;
		case ParamType::Trigger: xOutLive.AddTrigger(xParam.m_strName);                break;
		}
	}
}

void Flux_AnimationParameters::ResetTriggers()
{
	for (Zenith_HashMap<std::string, Parameter>::Iterator xIt(m_xParameters); !xIt.Done(); xIt.Next())
	{
		Parameter& xParam = xIt.GetValueMutable();
		if (xParam.m_eType == ParamType::Trigger)
			xParam.m_bValue = false;
	}
}

void Flux_AnimationParameters::WriteParamValueToStream(Zenith_DataStream& xStream, ParamType eType, float fVal, int32_t iVal, bool bVal)
{
	switch (eType)
	{
	case ParamType::Float:
		xStream << fVal;
		break;
	case ParamType::Int:
		xStream << iVal;
		break;
	case ParamType::Bool:
	case ParamType::Trigger:
		xStream << bVal;
		break;
	}
}

void Flux_AnimationParameters::ReadParamValueFromStream(Zenith_DataStream& xStream, ParamType eType, float& fVal, int32_t& iVal, bool& bVal)
{
	switch (eType)
	{
	case ParamType::Float:
		xStream >> fVal;
		break;
	case ParamType::Int:
		xStream >> iVal;
		break;
	case ParamType::Bool:
	case ParamType::Trigger:
		xStream >> bVal;
		break;
	}
}

void Flux_AnimationParameters::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumParams = static_cast<uint32_t>(m_xParameters.GetSize());
	xStream << uNumParams;

	for (Zenith_HashMap<std::string, Parameter>::Iterator xIt(m_xParameters); !xIt.Done(); xIt.Next())
	{
		const Parameter& xParam = xIt.GetValue();
		xStream << xParam.m_strName;
		xStream << static_cast<uint8_t>(xParam.m_eType);
		WriteParamValueToStream(xStream, xParam.m_eType, xParam.m_fValue, xParam.m_iValue, xParam.m_bValue);
	}
}

void Flux_AnimationParameters::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xParameters.Clear();

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
		ReadParamValueFromStream(xStream, xParam.m_eType, xParam.m_fValue, xParam.m_iValue, xParam.m_bValue);

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
		return CompareNumericValues(xParams.GetFloat(m_strParameterName), m_fThreshold, m_eCompareOp);

	case Flux_AnimationParameters::ParamType::Int:
		return CompareNumericValues(xParams.GetInt(m_strParameterName), m_iThreshold, m_eCompareOp);

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
	Flux_AnimationParameters::WriteParamValueToStream(xStream, m_eParamType, m_fThreshold, m_iThreshold, m_bThreshold);
}

void Flux_TransitionCondition::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strParameterName;

	uint8_t uOp = 0, uType = 0;
	xStream >> uOp;
	xStream >> uType;
	m_eCompareOp = static_cast<CompareOp>(uOp);
	m_eParamType = static_cast<Flux_AnimationParameters::ParamType>(uType);
	Flux_AnimationParameters::ReadParamValueFromStream(xStream, m_eParamType, m_fThreshold, m_iThreshold, m_bThreshold);
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
	InsertTransitionSortedByPriority(m_xTransitions, xTransition);
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

	// Sub-state machine — its DEF is what goes on the wire.
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
	delete m_pxBlendTree;
	m_pxBlendTree = nullptr;
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
	delete m_pxSubStateMachine;
	m_pxSubStateMachine = nullptr;
	bool bHasSubSM = false;
	xStream >> bHasSubSM;
	if (bHasSubSM)
	{
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
// Flux_AnimationStateMachineDef
//=============================================================================
Flux_AnimationStateMachineDef::Flux_AnimationStateMachineDef(const std::string& strName)
	: m_strName(strName)
{
}

Flux_AnimationStateMachineDef::~Flux_AnimationStateMachineDef()
{
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(m_xStates); !xIt.Done(); xIt.Next())
		delete xIt.GetValue();
}

void Flux_AnimationStateMachineDef::Clear()
{
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(m_xStates); !xIt.Done(); xIt.Next())
		delete xIt.GetValue();
	m_xStates.Clear();
	m_strDefaultStateName.clear();
	m_xAnyStateTransitions.Clear();

	// No-op on every reachable path today (the only caller is ReadFromDataStream,
	// which overwrites both) — Clear() is public, so the first external caller must
	// not inherit the previous def's name or declarations.
	m_strName.clear();
	m_xParameterDeclarations.Clear();
}

void Flux_AnimationStateMachineDef::CopyFrom(const Flux_AnimationStateMachineDef& xSource)
{
	if (this == &xSource)
		return;

	Zenith_DataStream xStream(1);
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	ReadFromDataStream(xStream);
}

Flux_AnimationState* Flux_AnimationStateMachineDef::AddState(const std::string& strName)
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

void Flux_AnimationStateMachineDef::RemoveState(const std::string& strName)
{
	Flux_AnimationState** ppxState = m_xStates.TryGet(strName);
	if (ppxState)
	{
		delete *ppxState;
		m_xStates.Remove(strName);

		// Clear default if removing it
		if (m_strDefaultStateName == strName)
			m_strDefaultStateName.clear();
	}
}

Flux_AnimationState* Flux_AnimationStateMachineDef::GetState(const std::string& strName)
{
	Flux_AnimationState** ppxState = m_xStates.TryGet(strName);
	return ppxState ? *ppxState : nullptr;
}

const Flux_AnimationState* Flux_AnimationStateMachineDef::GetState(const std::string& strName) const
{
	const Flux_AnimationState* const* ppxState = m_xStates.TryGet(strName);
	return ppxState ? *ppxState : nullptr;
}

bool Flux_AnimationStateMachineDef::HasState(const std::string& strName) const
{
	return m_xStates.Contains(strName);
}

void Flux_AnimationStateMachineDef::SetDefaultState(const std::string& strName)
{
	if (HasState(strName))
		m_strDefaultStateName = strName;
}

void Flux_AnimationStateMachineDef::AddAnyStateTransition(const Flux_StateTransition& xTransition)
{
	InsertTransitionSortedByPriority(m_xAnyStateTransitions, xTransition);
}

void Flux_AnimationStateMachineDef::RemoveAnyStateTransition(uint32_t uIndex)
{
	if (uIndex < m_xAnyStateTransitions.GetSize())
	{
		m_xAnyStateTransitions.Remove(static_cast<u_int>(uIndex));
	}
}

void Flux_AnimationStateMachineDef::SeedParametersInto(Flux_AnimationParameters& xOutLive) const
{
	m_xParameterDeclarations.SeedInto(xOutLive);

	// A container state's declarations are the controller's too (D42): a sub-state
	// machine reads the SAME live set, so a condition inside one that names a
	// parameter only IT declares has to find that name there.
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(m_xStates); !xIt.Done(); xIt.Next())
	{
		const Flux_AnimationState* pxState = xIt.GetValue();
		if (pxState && pxState->GetSubStateMachine())
			pxState->GetSubStateMachine()->GetDef().SeedParametersInto(xOutLive);
	}
}

//=============================================================================
// Clip reference resolution — by NAME, through the collection.
//=============================================================================
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

void Flux_AnimationStateMachineDef::ResolveClipReferences(Flux_AnimationClipCollection* pxCollection)
{
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(m_xStates); !xIt.Done(); xIt.Next())
	{
		Flux_AnimationState* pxState = xIt.GetValue();
		if (pxState == nullptr)
			continue;

		if (pxState->GetBlendTree())
		{
			ResolveClipReferencesRecursive(pxState->GetBlendTree(), pxCollection);
		}

		// ★ AND INTO CONTAINER STATES. The pre-WU-6.1 walk stopped at the top
		// level, so every clip leaf inside a sub-state machine stayed unresolved
		// after a load and posed the bind pose forever — silently, because an
		// unresolved leaf resets rather than asserting.
		if (pxState->GetSubStateMachine())
		{
			pxState->GetSubStateMachine()->GetDef().ResolveClipReferences(pxCollection);
		}
	}
}

void Flux_AnimationStateMachineDef::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;
	xStream << m_strDefaultStateName;

	// Parameter declarations + defaults
	m_xParameterDeclarations.WriteToDataStream(xStream);

	// States
	uint32_t uNumStates = static_cast<uint32_t>(m_xStates.GetSize());
	xStream << uNumStates;
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(m_xStates); !xIt.Done(); xIt.Next())
	{
		xIt.GetValue()->WriteToDataStream(xStream);
	}

	// Any-state transitions
	uint32_t uNumAnyState = m_xAnyStateTransitions.GetSize();
	xStream << uNumAnyState;
	for (uint32_t i = 0; i < uNumAnyState; ++i)
	{
		m_xAnyStateTransitions.Get(i).WriteToDataStream(xStream);
	}
}

void Flux_AnimationStateMachineDef::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Clear();

	xStream >> m_strName;
	xStream >> m_strDefaultStateName;

	// Parameter declarations + defaults
	m_xParameterDeclarations.ReadFromDataStream(xStream);

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
	for (uint32_t i = 0; i < uNumAnyState; ++i)
	{
		Flux_StateTransition xTrans;
		xTrans.ReadFromDataStream(xStream);
		m_xAnyStateTransitions.PushBack(std::move(xTrans));
	}
}
