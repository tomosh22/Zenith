#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTActions.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Zenith_AIWorldHooks.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

// ========== Zenith_BTAction_Wait ==========

Zenith_BTAction_Wait::Zenith_BTAction_Wait(float fDuration)
	: m_fDuration(fDuration)
{
}

void Zenith_BTAction_Wait::OnEnter(Zenith_Entity&, Zenith_Blackboard& xBlackboard)
{
	m_fElapsed = 0.0f;

	// If using blackboard key, read duration
	if (!m_strDurationKey.empty())
	{
		m_fDuration = xBlackboard.GetFloat(m_strDurationKey, m_fDuration);
	}
}

BTNodeStatus Zenith_BTAction_Wait::Execute(Zenith_Entity&, Zenith_Blackboard&, float fDt)
{
	m_fElapsed += fDt;

	if (m_fElapsed >= m_fDuration)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
	}
	else
	{
		m_eLastStatus = BTNodeStatus::RUNNING;
	}

	return m_eLastStatus;
}

void Zenith_BTAction_Wait::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_fDuration;
	xStream << m_strDurationKey;
}

void Zenith_BTAction_Wait::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_fDuration;
	xStream >> m_strDurationKey;
}

// ========== Zenith_BTAction_MoveTo ==========

Zenith_BTAction_MoveTo::Zenith_BTAction_MoveTo(const std::string& strTargetKey)
	: m_strTargetKey(strTargetKey)
{
}

void Zenith_BTAction_MoveTo::OnEnter(Zenith_Entity&, Zenith_Blackboard&)
{
	m_bPathRequested = false;
}

void Zenith_BTAction_MoveTo::OnExit(Zenith_Entity& xAgent, Zenith_Blackboard&)
{
	// Stop movement
	Zenith_NavMeshAgent* pxNav = Zenith_AI_GetNavMeshAgent(xAgent.GetEntityID());
	if (pxNav)
	{
		pxNav->Stop();
	}
}

void Zenith_BTAction_MoveTo::OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	OnExit(xAgent, xBlackboard);
}

BTNodeStatus Zenith_BTAction_MoveTo::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float)
{
	// Get target position from blackboard
	Zenith_Maths::Vector3 xTargetPos = xBlackboard.GetVector3(m_strTargetKey);

	// Get nav agent
	Zenith_NavMeshAgent* pxNav = Zenith_AI_GetNavMeshAgent(xAgent.GetEntityID());

	if (pxNav == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Request path if not yet done
	if (!m_bPathRequested)
	{
		if (!pxNav->SetDestination(xTargetPos))
		{
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}
		m_bPathRequested = true;
	}

	// Check if arrived. "Arrived" means the agent is actually within
	// m_fAcceptanceRadius of the requested position -- NOT just "has
	// reached the end of whatever path the navmesh found." When the
	// requested position is unreachable, Zenith_Pathfinding::FindPath
	// returns Status::PARTIAL with waypoints to the nearest reachable
	// navmesh polygon; the agent walks the path, reaches its end, and
	// sets m_bReachedDestination = true. Without the actual-position
	// distance check below, the action returned SUCCESS even though the
	// agent stopped short of the requested point -- and on a recurring
	// trigger (e.g. priest BT polling InvestigatePos every frame from
	// perception), the action enters a tight SUCCESS / re-fire loop with
	// no progress (`Build/dp_telemetry/find_stuck.py` 2026-05-23).
	//
	// On a PARTIAL path the agent has done all it can; return FAILURE so
	// the parent Selector falls through to a lower-priority branch.
	if (pxNav->HasReachedDestination())
	{
		Zenith_Maths::Vector3 xAgentPos;
		if (Zenith_AI_GetEntityPosition(xAgent.GetEntityID(), xAgentPos))
		{
			const float fDist = Zenith_Maths::Length(xTargetPos - xAgentPos);
			if (fDist > m_fAcceptanceRadius)
			{
				m_eLastStatus = BTNodeStatus::FAILURE;
				return m_eLastStatus;
			}
		}
		m_eLastStatus = BTNodeStatus::SUCCESS;
		return m_eLastStatus;
	}

	// Check if path failed. A path that's still being computed asynchronously
	// (m_bPathPending=true && !HasPath()) is NOT a failure — keep RUNNING so
	// the agent's next NavMeshAgent::Update gets to call FindPath. Without
	// this gate, MoveTo would fail in the same frame SetDestination is
	// called because BT.Tick runs ahead of NavMeshAgent::Update in the
	// per-frame schedule.
	if (!pxNav->HasPath())
	{
		if (pxNav->NeedsPath())
		{
			m_eLastStatus = BTNodeStatus::RUNNING;
			return m_eLastStatus;
		}
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	m_eLastStatus = BTNodeStatus::RUNNING;
	return m_eLastStatus;
}

void Zenith_BTAction_MoveTo::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_strTargetKey;
	xStream << m_fAcceptanceRadius;
}

void Zenith_BTAction_MoveTo::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_strTargetKey;
	xStream >> m_fAcceptanceRadius;
}

// ========== Zenith_BTAction_MoveToEntity ==========

Zenith_BTAction_MoveToEntity::Zenith_BTAction_MoveToEntity(const std::string& strTargetKey)
	: m_strTargetKey(strTargetKey)
{
}

void Zenith_BTAction_MoveToEntity::OnEnter(Zenith_Entity&, Zenith_Blackboard&)
{
	m_bPathRequested = false;
	m_fTimeSinceRepath = m_fRepathInterval;  // Trigger immediate path
}

void Zenith_BTAction_MoveToEntity::OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard&)
{
	Zenith_NavMeshAgent* pxNav = Zenith_AI_GetNavMeshAgent(xAgent.GetEntityID());
	if (pxNav)
	{
		pxNav->Stop();
	}
}

BTNodeStatus Zenith_BTAction_MoveToEntity::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Get target entity from blackboard
	Zenith_EntityID xTargetID = xBlackboard.GetEntityID(m_strTargetKey);
	if (!xTargetID.IsValid())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Get target position. Audit §3.18 fix: resolve target's OWN scene so
	// MoveToEntity works for targets in persistent or additively-loaded scenes.
	// Ref: https://docs.unity3d.com/ScriptReference/GameObject-scene.html
	Zenith_Maths::Vector3 xTargetPos;
	if (!Zenith_AI_GetEntityPosition(xTargetID, xTargetPos))
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Get nav agent
	Zenith_NavMeshAgent* pxNav = Zenith_AI_GetNavMeshAgent(xAgent.GetEntityID());

	if (pxNav == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Check current distance
	Zenith_Maths::Vector3 xAgentPos;
	if (Zenith_AI_GetEntityPosition(xAgent.GetEntityID(), xAgentPos))
	{
		float fDist = Zenith_Maths::Length(xTargetPos - xAgentPos);

		if (fDist <= m_fAcceptanceRadius)
		{
			pxNav->Stop();
			m_eLastStatus = BTNodeStatus::SUCCESS;
			return m_eLastStatus;
		}

		// 2026-05-23: detect "agent reached the end of its path but the
		// actual target is still beyond the acceptance radius." This is
		// the PARTIAL-path case: Zenith_Pathfinding::FindPath returns a
		// path to the nearest reachable navmesh polygon when the actual
		// destination isn't connected (disconnected polygon island,
		// blocked-by-door corridor that's still BLOCKED at the time of
		// pathfind, etc.). The agent dutifully walks to that partial
		// endpoint and sets m_bReachedDestination = true -- but we're
		// still fDist metres short of the requested target. Without this
		// check, MoveToEntity would re-issue SetDestination every
		// m_fRepathInterval seconds, FindPath would return the same
		// partial path, the agent would "re-arrive" immediately (already
		// at the endpoint), and the BT action would loop forever. The
		// priest in DevilsPlayground hit this on seed 12345 -- frozen
		// for 175 s of a 200 s run (`Build/dp_telemetry/find_stuck.py`).
		// Returning FAILURE lets the parent Selector fall through to a
		// lower-priority branch (e.g. Patrol) that might actually be
		// reachable.
		if (pxNav->HasReachedDestination())
		{
			pxNav->Stop();
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}
	}

	// Re-path periodically to track moving target
	m_fTimeSinceRepath += fDt;
	if (m_fTimeSinceRepath >= m_fRepathInterval)
	{
		pxNav->SetDestination(xTargetPos);
		m_fTimeSinceRepath = 0.0f;
		m_bPathRequested = true;
	}

	// Check if path failed. As with MoveTo::Execute, a path that's still
	// being computed asynchronously (m_bPathPending=true && !HasPath()) is
	// NOT a failure — keep RUNNING so the agent's next NavMeshAgent::Update
	// gets to call FindPath. Without this gate, MoveToEntity returns
	// FAILURE the same frame SetDestination is called (BT.Tick runs ahead
	// of NavMeshAgent::Update in the per-frame schedule), and the parent
	// Sequence's repath loop never gets to consume a successful path.
	if (m_bPathRequested && !pxNav->HasPath())
	{
		if (pxNav->NeedsPath())
		{
			m_eLastStatus = BTNodeStatus::RUNNING;
			return m_eLastStatus;
		}
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	m_eLastStatus = BTNodeStatus::RUNNING;
	return m_eLastStatus;
}

void Zenith_BTAction_MoveToEntity::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_strTargetKey;
	xStream << m_fAcceptanceRadius;
	xStream << m_fRepathInterval;
}

void Zenith_BTAction_MoveToEntity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_strTargetKey;
	xStream >> m_fAcceptanceRadius;
	xStream >> m_fRepathInterval;
}

// ========== Zenith_BTAction_SetBlackboardBool ==========

Zenith_BTAction_SetBlackboardBool::Zenith_BTAction_SetBlackboardBool(const std::string& strKey, bool bValue)
	: m_strKey(strKey)
	, m_bValue(bValue)
{
}

BTNodeStatus Zenith_BTAction_SetBlackboardBool::Execute(Zenith_Entity&, Zenith_Blackboard& xBlackboard, float)
{
	xBlackboard.SetBool(m_strKey, m_bValue);
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTAction_SetBlackboardBool::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_strKey;
	xStream << m_bValue;
}

void Zenith_BTAction_SetBlackboardBool::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_strKey;
	xStream >> m_bValue;
}

// ========== Zenith_BTAction_SetBlackboardFloat ==========

Zenith_BTAction_SetBlackboardFloat::Zenith_BTAction_SetBlackboardFloat(const std::string& strKey, float fValue)
	: m_strKey(strKey)
	, m_fValue(fValue)
{
}

BTNodeStatus Zenith_BTAction_SetBlackboardFloat::Execute(Zenith_Entity&, Zenith_Blackboard& xBlackboard, float)
{
	xBlackboard.SetFloat(m_strKey, m_fValue);
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTAction_SetBlackboardFloat::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_strKey;
	xStream << m_fValue;
}

void Zenith_BTAction_SetBlackboardFloat::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_strKey;
	xStream >> m_fValue;
}

// ========== Zenith_BTAction_Log ==========

Zenith_BTAction_Log::Zenith_BTAction_Log(const std::string& strMessage)
	: m_strMessage(strMessage)
{
}

BTNodeStatus Zenith_BTAction_Log::Execute(Zenith_Entity& xAgent, Zenith_Blackboard&, float)
{
	Zenith_Log(LOG_CATEGORY_AI, "[BT Log] Entity %u: %s", xAgent.GetEntityID().m_uIndex, m_strMessage.c_str());
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTAction_Log::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_strMessage;
}

void Zenith_BTAction_Log::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_strMessage;
}

// ========== Zenith_BTAction_FindPrimaryTarget ==========

BTNodeStatus Zenith_BTAction_FindPrimaryTarget::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float)
{
	Zenith_EntityID xTarget = Zenith_PerceptionSystem::GetPrimaryTarget(xAgent.GetEntityID());

	if (xTarget.IsValid())
	{
		xBlackboard.SetEntityID(m_strOutputKey, xTarget);

		// Also store target position. Audit §3.18 fix: resolve target's OWN scene.
		Zenith_Maths::Vector3 xPos;
		if (Zenith_AI_GetEntityPosition(xTarget, xPos))
		{
			xBlackboard.SetVector3("TargetPosition", xPos);
		}

		m_eLastStatus = BTNodeStatus::SUCCESS;
	}
	else
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
	}

	return m_eLastStatus;
}
