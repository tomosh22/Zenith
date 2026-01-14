#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTDecorators.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "DataStream/Zenith_DataStream.h"

// ========== Zenith_BTInverter ==========

BTNodeStatus Zenith_BTInverter::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	Zenith_BTNode* pxChild = GetChild();
	if (pxChild == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

	switch (eChildStatus)
	{
	case BTNodeStatus::SUCCESS:
		m_eLastStatus = BTNodeStatus::FAILURE;
		break;
	case BTNodeStatus::FAILURE:
		m_eLastStatus = BTNodeStatus::SUCCESS;
		break;
	case BTNodeStatus::RUNNING:
		m_eLastStatus = BTNodeStatus::RUNNING;
		break;
	}

	return m_eLastStatus;
}

// ========== Zenith_BTSucceeder ==========

BTNodeStatus Zenith_BTSucceeder::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	Zenith_BTNode* pxChild = GetChild();
	if (pxChild == nullptr)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
		return m_eLastStatus;
	}

	BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

	if (eChildStatus == BTNodeStatus::RUNNING)
	{
		m_eLastStatus = BTNodeStatus::RUNNING;
	}
	else
	{
		// Always return SUCCESS once child completes
		m_eLastStatus = BTNodeStatus::SUCCESS;
	}

	return m_eLastStatus;
}

// ========== Zenith_BTRepeater ==========

Zenith_BTRepeater::Zenith_BTRepeater(int32_t iRepeatCount, bool bStopOnFailure)
	: m_iRepeatCount(iRepeatCount)
	, m_bStopOnFailure(bStopOnFailure)
{
}

void Zenith_BTRepeater::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	Zenith_BTDecorator::OnEnter(xAgent, xBlackboard);
	m_iCurrentIteration = 0;
}

BTNodeStatus Zenith_BTRepeater::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	Zenith_BTNode* pxChild = GetChild();
	if (pxChild == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Check if we've completed all iterations (for non-infinite)
	if (m_iRepeatCount != INFINITE && m_iCurrentIteration >= m_iRepeatCount)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
		return m_eLastStatus;
	}

	BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

	if (eChildStatus == BTNodeStatus::RUNNING)
	{
		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	// Child completed
	pxChild->OnExit(xAgent, xBlackboard);

	if (eChildStatus == BTNodeStatus::FAILURE && m_bStopOnFailure)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Increment iteration count
	++m_iCurrentIteration;

	// Check if we need to continue
	if (m_iRepeatCount == INFINITE || m_iCurrentIteration < m_iRepeatCount)
	{
		// Restart child
		pxChild->OnEnter(xAgent, xBlackboard);
		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	// All iterations complete
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTRepeater::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTDecorator::WriteToDataStream(xStream);
	xStream << m_iRepeatCount;
	xStream << m_bStopOnFailure;
}

void Zenith_BTRepeater::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTDecorator::ReadFromDataStream(xStream);
	xStream >> m_iRepeatCount;
	xStream >> m_bStopOnFailure;
}

// ========== Zenith_BTCooldown ==========

Zenith_BTCooldown::Zenith_BTCooldown(float fCooldownDuration)
	: m_fCooldownDuration(fCooldownDuration)
	, m_fTimeSinceCompletion(fCooldownDuration)  // Start ready
{
}

void Zenith_BTCooldown::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	Zenith_BTDecorator::OnEnter(xAgent, xBlackboard);
	m_bChildRunning = false;
}

BTNodeStatus Zenith_BTCooldown::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Update cooldown timer
	m_fTimeSinceCompletion += fDt;

	// If on cooldown, return FAILURE
	if (!m_bChildRunning && m_fTimeSinceCompletion < m_fCooldownDuration)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	Zenith_BTNode* pxChild = GetChild();
	if (pxChild == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Execute child
	if (!m_bChildRunning)
	{
		pxChild->OnEnter(xAgent, xBlackboard);
		m_bChildRunning = true;
	}

	BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

	if (eChildStatus == BTNodeStatus::RUNNING)
	{
		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	// Child completed - start cooldown
	pxChild->OnExit(xAgent, xBlackboard);
	m_fTimeSinceCompletion = 0.0f;
	m_bChildRunning = false;
	m_eLastStatus = eChildStatus;
	return m_eLastStatus;
}

float Zenith_BTCooldown::GetRemainingCooldown() const
{
	float fRemaining = m_fCooldownDuration - m_fTimeSinceCompletion;
	return fRemaining > 0.0f ? fRemaining : 0.0f;
}

void Zenith_BTCooldown::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTDecorator::WriteToDataStream(xStream);
	xStream << m_fCooldownDuration;
}

void Zenith_BTCooldown::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTDecorator::ReadFromDataStream(xStream);
	xStream >> m_fCooldownDuration;
	m_fTimeSinceCompletion = m_fCooldownDuration;  // Start ready
}

// ========== Zenith_BTConditionalLoop ==========

Zenith_BTConditionalLoop::Zenith_BTConditionalLoop(const std::string& strConditionKey)
	: m_strConditionKey(strConditionKey)
{
}

BTNodeStatus Zenith_BTConditionalLoop::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Check condition
	if (!xBlackboard.GetBool(m_strConditionKey, false))
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
		return m_eLastStatus;
	}

	Zenith_BTNode* pxChild = GetChild();
	if (pxChild == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

	if (eChildStatus == BTNodeStatus::RUNNING)
	{
		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	// Child completed, check condition again for next iteration
	pxChild->OnExit(xAgent, xBlackboard);

	// If condition still true, keep running
	if (xBlackboard.GetBool(m_strConditionKey, false))
	{
		pxChild->OnEnter(xAgent, xBlackboard);
		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	// Condition became false, we're done
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTConditionalLoop::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTDecorator::WriteToDataStream(xStream);

	uint32_t uLen = static_cast<uint32_t>(m_strConditionKey.length());
	xStream << uLen;
	if (uLen > 0)
	{
		xStream.Write(m_strConditionKey.data(), uLen);
	}
}

void Zenith_BTConditionalLoop::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTDecorator::ReadFromDataStream(xStream);

	uint32_t uLen = 0;
	xStream >> uLen;
	if (uLen > 0)
	{
		m_strConditionKey.resize(uLen);
		xStream.Read(m_strConditionKey.data(), uLen);
	}
	else
	{
		m_strConditionKey.clear();
	}
}

// ========== Zenith_BTTimeLimit ==========

Zenith_BTTimeLimit::Zenith_BTTimeLimit(float fTimeLimit)
	: m_fTimeLimit(fTimeLimit)
{
}

void Zenith_BTTimeLimit::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	Zenith_BTDecorator::OnEnter(xAgent, xBlackboard);
	m_fElapsedTime = 0.0f;
}

BTNodeStatus Zenith_BTTimeLimit::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	m_fElapsedTime += fDt;

	// Check time limit
	if (m_fElapsedTime >= m_fTimeLimit)
	{
		Zenith_BTNode* pxChild = GetChild();
		if (pxChild != nullptr)
		{
			pxChild->OnAbort(xAgent, xBlackboard);
		}
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	Zenith_BTNode* pxChild = GetChild();
	if (pxChild == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);
	m_eLastStatus = eChildStatus;
	return m_eLastStatus;
}

void Zenith_BTTimeLimit::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTDecorator::WriteToDataStream(xStream);
	xStream << m_fTimeLimit;
}

void Zenith_BTTimeLimit::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTDecorator::ReadFromDataStream(xStream);
	xStream >> m_fTimeLimit;
}
