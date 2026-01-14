#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"
#include "DataStream/Zenith_DataStream.h"

// ========== Zenith_BTSequence ==========

BTNodeStatus Zenith_BTSequence::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Continue from where we left off (m_uCurrentChild is reset in OnEnter)
	while (m_uCurrentChild < GetChildCount())
	{
		Zenith_BTNode* pxChild = GetChild(m_uCurrentChild);

		// If this is a new child, call OnEnter
		if (pxChild->GetLastStatus() != BTNodeStatus::RUNNING)
		{
			pxChild->OnEnter(xAgent, xBlackboard);
		}

		BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

		if (eChildStatus == BTNodeStatus::RUNNING)
		{
			// Child still running, return RUNNING and resume here next tick
			m_eLastStatus = BTNodeStatus::RUNNING;
			return m_eLastStatus;
		}

		// Child completed, call OnExit
		pxChild->OnExit(xAgent, xBlackboard);

		if (eChildStatus == BTNodeStatus::FAILURE)
		{
			// Sequence fails on first failure
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}

		// Child succeeded, move to next
		++m_uCurrentChild;
	}

	// All children succeeded
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

// ========== Zenith_BTSelector ==========

BTNodeStatus Zenith_BTSelector::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Continue from where we left off
	while (m_uCurrentChild < GetChildCount())
	{
		Zenith_BTNode* pxChild = GetChild(m_uCurrentChild);

		// If this is a new child, call OnEnter
		if (pxChild->GetLastStatus() != BTNodeStatus::RUNNING)
		{
			pxChild->OnEnter(xAgent, xBlackboard);
		}

		BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

		if (eChildStatus == BTNodeStatus::RUNNING)
		{
			// Child still running, return RUNNING and resume here next tick
			m_eLastStatus = BTNodeStatus::RUNNING;
			return m_eLastStatus;
		}

		// Child completed, call OnExit
		pxChild->OnExit(xAgent, xBlackboard);

		if (eChildStatus == BTNodeStatus::SUCCESS)
		{
			// Selector succeeds on first success
			m_eLastStatus = BTNodeStatus::SUCCESS;
			return m_eLastStatus;
		}

		// Child failed, try next
		++m_uCurrentChild;
	}

	// All children failed
	m_eLastStatus = BTNodeStatus::FAILURE;
	return m_eLastStatus;
}

// ========== Zenith_BTParallel ==========

Zenith_BTParallel::Zenith_BTParallel(Policy eSuccessPolicy, Policy eFailurePolicy)
	: m_eSuccessPolicy(eSuccessPolicy)
	, m_eFailurePolicy(eFailurePolicy)
{
}

void Zenith_BTParallel::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	Zenith_BTComposite::OnEnter(xAgent, xBlackboard);

	// Reset child results
	m_axChildResults.Clear();
	m_axChildResults.Reserve(GetChildCount());
	for (uint32_t u = 0; u < GetChildCount(); ++u)
	{
		m_axChildResults.PushBack(BTNodeStatus::RUNNING);
	}

	// Call OnEnter for all children
	for (uint32_t u = 0; u < GetChildCount(); ++u)
	{
		GetChild(u)->OnEnter(xAgent, xBlackboard);
	}
}

BTNodeStatus Zenith_BTParallel::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Lazy initialization if OnEnter wasn't called
	if (m_axChildResults.GetSize() != GetChildCount())
	{
		m_axChildResults.Clear();
		m_axChildResults.Reserve(GetChildCount());
		for (uint32_t u = 0; u < GetChildCount(); ++u)
		{
			m_axChildResults.PushBack(BTNodeStatus::RUNNING);
			GetChild(u)->OnEnter(xAgent, xBlackboard);
		}
	}

	uint32_t uSuccessCount = 0;
	uint32_t uFailureCount = 0;
	uint32_t uRunningCount = 0;

	// Execute all children that are still running
	for (uint32_t u = 0; u < GetChildCount(); ++u)
	{
		// Skip children that have already completed
		if (m_axChildResults.Get(u) != BTNodeStatus::RUNNING)
		{
			if (m_axChildResults.Get(u) == BTNodeStatus::SUCCESS)
			{
				++uSuccessCount;
			}
			else
			{
				++uFailureCount;
			}
			continue;
		}

		Zenith_BTNode* pxChild = GetChild(u);
		BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);
		m_axChildResults.Get(u) = eChildStatus;

		if (eChildStatus == BTNodeStatus::SUCCESS)
		{
			pxChild->OnExit(xAgent, xBlackboard);
			++uSuccessCount;
		}
		else if (eChildStatus == BTNodeStatus::FAILURE)
		{
			pxChild->OnExit(xAgent, xBlackboard);
			++uFailureCount;
		}
		else
		{
			++uRunningCount;
		}
	}

	// Check success policy
	if (m_eSuccessPolicy == Policy::REQUIRE_ONE)
	{
		if (uSuccessCount > 0)
		{
			// Abort any still-running children
			for (uint32_t u = 0; u < GetChildCount(); ++u)
			{
				if (m_axChildResults.Get(u) == BTNodeStatus::RUNNING)
				{
					GetChild(u)->OnAbort(xAgent, xBlackboard);
				}
			}
			m_eLastStatus = BTNodeStatus::SUCCESS;
			return m_eLastStatus;
		}
	}
	else // REQUIRE_ALL
	{
		if (uSuccessCount == GetChildCount())
		{
			m_eLastStatus = BTNodeStatus::SUCCESS;
			return m_eLastStatus;
		}
	}

	// Check failure policy
	if (m_eFailurePolicy == Policy::REQUIRE_ONE)
	{
		if (uFailureCount > 0)
		{
			// Abort any still-running children
			for (uint32_t u = 0; u < GetChildCount(); ++u)
			{
				if (m_axChildResults.Get(u) == BTNodeStatus::RUNNING)
				{
					GetChild(u)->OnAbort(xAgent, xBlackboard);
				}
			}
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}
	}
	else // REQUIRE_ALL
	{
		if (uFailureCount == GetChildCount())
		{
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}
	}

	// Still running if any children are running
	if (uRunningCount > 0)
	{
		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	// All children completed but neither success nor failure policy was met
	// This happens when REQUIRE_ALL success but some failed, or vice versa
	// Default to failure in this edge case
	m_eLastStatus = BTNodeStatus::FAILURE;
	return m_eLastStatus;
}

void Zenith_BTParallel::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTComposite::WriteToDataStream(xStream);

	xStream << static_cast<uint8_t>(m_eSuccessPolicy);
	xStream << static_cast<uint8_t>(m_eFailurePolicy);
}

void Zenith_BTParallel::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTComposite::ReadFromDataStream(xStream);

	uint8_t uSuccessPolicy = 0;
	uint8_t uFailurePolicy = 0;
	xStream >> uSuccessPolicy;
	xStream >> uFailurePolicy;
	m_eSuccessPolicy = static_cast<Policy>(uSuccessPolicy);
	m_eFailurePolicy = static_cast<Policy>(uFailurePolicy);
}
