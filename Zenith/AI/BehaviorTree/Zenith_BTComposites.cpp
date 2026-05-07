#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"

// ========== Sequence / Selector shared body ==========
//
// Sequence and Selector are duals: both walk children left-to-right, both
// resume from m_uCurrentChild after a RUNNING child, both call OnEnter /
// OnExit at the same boundaries. The only difference is which child status
// triggers the early-exit return:
//   - Sequence exits on FAILURE (and returns SUCCESS if all children pass)
//   - Selector exits on SUCCESS (and returns FAILURE if all children fail)
//
// Function-pointer template parameter (not std::function — forbidden by
// project conventions) makes the predicate a compile-time constant so the
// branch optimises away.

static bool IsFailureStatus(BTNodeStatus eStatus) { return eStatus == BTNodeStatus::FAILURE; }
static bool IsSuccessStatus(BTNodeStatus eStatus) { return eStatus == BTNodeStatus::SUCCESS; }

template<bool (*EarlyExit)(BTNodeStatus), BTNodeStatus FinalStatus>
static BTNodeStatus ExecuteCompositeBody(
	Zenith_BTComposite& xSelf,
	uint32_t& uCurrentChild, BTNodeStatus& eLastStatus,
	Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Continue from where we left off (uCurrentChild is reset in OnEnter)
	while (uCurrentChild < xSelf.GetChildCount())
	{
		Zenith_BTNode* pxChild = xSelf.GetChild(uCurrentChild);

		// If this is a new child, call OnEnter
		if (pxChild->GetLastStatus() != BTNodeStatus::RUNNING)
		{
			pxChild->OnEnter(xAgent, xBlackboard);
		}

		BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);

		if (eChildStatus == BTNodeStatus::RUNNING)
		{
			// Child still running, return RUNNING and resume here next tick
			eLastStatus = BTNodeStatus::RUNNING;
			return eLastStatus;
		}

		// Child completed, call OnExit
		pxChild->OnExit(xAgent, xBlackboard);

		if (EarlyExit(eChildStatus))
		{
			eLastStatus = eChildStatus;
			return eLastStatus;
		}

		++uCurrentChild;
	}

	eLastStatus = FinalStatus;
	return eLastStatus;
}

// ========== Zenith_BTSequence ==========

BTNodeStatus Zenith_BTSequence::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	return ExecuteCompositeBody<&IsFailureStatus, BTNodeStatus::SUCCESS>(
		*this, m_uCurrentChild, m_eLastStatus, xAgent, xBlackboard, fDt);
}

// ========== Zenith_BTSelector ==========

BTNodeStatus Zenith_BTSelector::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	return ExecuteCompositeBody<&IsSuccessStatus, BTNodeStatus::FAILURE>(
		*this, m_uCurrentChild, m_eLastStatus, xAgent, xBlackboard, fDt);
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

void Zenith_BTParallel::EnsureChildResultsInitialised(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	if (m_axChildResults.GetSize() == GetChildCount()) return;

	m_axChildResults.Clear();
	m_axChildResults.Reserve(GetChildCount());
	for (uint32_t u = 0; u < GetChildCount(); ++u)
	{
		m_axChildResults.PushBack(BTNodeStatus::RUNNING);
		GetChild(u)->OnEnter(xAgent, xBlackboard);
	}
}

Zenith_BTParallel::ChildStatusCounts Zenith_BTParallel::TickChildrenAndTally(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	ChildStatusCounts xCounts;
	for (uint32_t u = 0; u < GetChildCount(); ++u)
	{
		// Already-completed children carry their status into the tally.
		if (m_axChildResults.Get(u) != BTNodeStatus::RUNNING)
		{
			if (m_axChildResults.Get(u) == BTNodeStatus::SUCCESS) ++xCounts.m_uSuccess;
			else ++xCounts.m_uFailure;
			continue;
		}

		Zenith_BTNode* pxChild = GetChild(u);
		const BTNodeStatus eChildStatus = pxChild->Execute(xAgent, xBlackboard, fDt);
		m_axChildResults.Get(u) = eChildStatus;

		if (eChildStatus == BTNodeStatus::SUCCESS)
		{
			pxChild->OnExit(xAgent, xBlackboard);
			++xCounts.m_uSuccess;
		}
		else if (eChildStatus == BTNodeStatus::FAILURE)
		{
			pxChild->OnExit(xAgent, xBlackboard);
			++xCounts.m_uFailure;
		}
		else
		{
			++xCounts.m_uRunning;
		}
	}
	return xCounts;
}

bool Zenith_BTParallel::SuccessPolicyMet(const ChildStatusCounts& xCounts) const
{
	return m_eSuccessPolicy == Policy::REQUIRE_ONE
		? xCounts.m_uSuccess > 0
		: xCounts.m_uSuccess == GetChildCount();
}

bool Zenith_BTParallel::FailurePolicyMet(const ChildStatusCounts& xCounts) const
{
	return m_eFailurePolicy == Policy::REQUIRE_ONE
		? xCounts.m_uFailure > 0
		: xCounts.m_uFailure == GetChildCount();
}

BTNodeStatus Zenith_BTParallel::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	EnsureChildResultsInitialised(xAgent, xBlackboard);
	const ChildStatusCounts xCounts = TickChildrenAndTally(xAgent, xBlackboard, fDt);

	if (SuccessPolicyMet(xCounts))
	{
		// REQUIRE_ONE success wins early — abort the rest. REQUIRE_ALL only
		// succeeds when every child already completed, so nothing is running.
		if (m_eSuccessPolicy == Policy::REQUIRE_ONE) AbortRunningChildren(xAgent, xBlackboard);
		m_eLastStatus = BTNodeStatus::SUCCESS;
		return m_eLastStatus;
	}

	if (FailurePolicyMet(xCounts))
	{
		if (m_eFailurePolicy == Policy::REQUIRE_ONE) AbortRunningChildren(xAgent, xBlackboard);
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Still running if any children are running; otherwise all completed but
	// neither policy was met (e.g. REQUIRE_ALL success with some failures) — default to failure.
	m_eLastStatus = xCounts.m_uRunning > 0 ? BTNodeStatus::RUNNING : BTNodeStatus::FAILURE;
	return m_eLastStatus;
}

void Zenith_BTParallel::AbortRunningChildren(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	for (uint32_t u = 0; u < GetChildCount(); ++u)
	{
		if (m_axChildResults.Get(u) == BTNodeStatus::RUNNING)
		{
			GetChild(u)->OnAbort(xAgent, xBlackboard);
		}
	}
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
