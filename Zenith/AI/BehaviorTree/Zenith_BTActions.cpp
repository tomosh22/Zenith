#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTActions.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

// ========== Zenith_BTAction_Wait ==========

Zenith_BTAction_Wait::Zenith_BTAction_Wait(float fDuration)
	: m_fDuration(fDuration)
{
}

void Zenith_BTAction_Wait::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	m_fElapsed = 0.0f;

	// If using blackboard key, read duration
	if (!m_strDurationKey.empty())
	{
		m_fDuration = xBlackboard.GetFloat(m_strDurationKey, m_fDuration);
	}
}

BTNodeStatus Zenith_BTAction_Wait::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
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

	uint32_t uKeyLen = static_cast<uint32_t>(m_strDurationKey.length());
	xStream << uKeyLen;
	if (uKeyLen > 0)
	{
		xStream.Write(m_strDurationKey.data(), uKeyLen);
	}
}

void Zenith_BTAction_Wait::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_fDuration;

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	if (uKeyLen > 0)
	{
		m_strDurationKey.resize(uKeyLen);
		xStream.Read(m_strDurationKey.data(), uKeyLen);
	}
}

// ========== Zenith_BTAction_MoveTo ==========

Zenith_BTAction_MoveTo::Zenith_BTAction_MoveTo(const std::string& strTargetKey)
	: m_strTargetKey(strTargetKey)
{
}

void Zenith_BTAction_MoveTo::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	m_bPathRequested = false;
}

void Zenith_BTAction_MoveTo::OnExit(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	// Stop movement
	if (xAgent.HasComponent<Zenith_AIAgentComponent>())
	{
		Zenith_AIAgentComponent& xAI = xAgent.GetComponent<Zenith_AIAgentComponent>();
		Zenith_NavMeshAgent* pxNav = xAI.GetNavMeshAgent();
		if (pxNav)
		{
			pxNav->Stop();
		}
	}
}

void Zenith_BTAction_MoveTo::OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	OnExit(xAgent, xBlackboard);
}

BTNodeStatus Zenith_BTAction_MoveTo::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	// Get target position from blackboard
	Zenith_Maths::Vector3 xTargetPos = xBlackboard.GetVector3(m_strTargetKey);

	// Get nav agent
	if (!xAgent.HasComponent<Zenith_AIAgentComponent>())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	Zenith_AIAgentComponent& xAI = xAgent.GetComponent<Zenith_AIAgentComponent>();
	Zenith_NavMeshAgent* pxNav = xAI.GetNavMeshAgent();

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

	// Check if arrived
	if (pxNav->HasReachedDestination())
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
		return m_eLastStatus;
	}

	// Check if path failed
	if (!pxNav->HasPath())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	m_eLastStatus = BTNodeStatus::RUNNING;
	return m_eLastStatus;
}

void Zenith_BTAction_MoveTo::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strTargetKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strTargetKey.data(), uKeyLen);

	xStream << m_fAcceptanceRadius;
}

void Zenith_BTAction_MoveTo::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strTargetKey.resize(uKeyLen);
	xStream.Read(m_strTargetKey.data(), uKeyLen);

	xStream >> m_fAcceptanceRadius;
}

// ========== Zenith_BTAction_MoveToEntity ==========

Zenith_BTAction_MoveToEntity::Zenith_BTAction_MoveToEntity(const std::string& strTargetKey)
	: m_strTargetKey(strTargetKey)
{
}

void Zenith_BTAction_MoveToEntity::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	m_bPathRequested = false;
	m_fTimeSinceRepath = m_fRepathInterval;  // Trigger immediate path
}

void Zenith_BTAction_MoveToEntity::OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	if (xAgent.HasComponent<Zenith_AIAgentComponent>())
	{
		Zenith_AIAgentComponent& xAI = xAgent.GetComponent<Zenith_AIAgentComponent>();
		Zenith_NavMeshAgent* pxNav = xAI.GetNavMeshAgent();
		if (pxNav)
		{
			pxNav->Stop();
		}
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

	// Get target position
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	Zenith_Entity xTargetEntity = xScene.TryGetEntity(xTargetID);
	if (!xTargetEntity.IsValid() || !xTargetEntity.HasComponent<Zenith_TransformComponent>())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	Zenith_Maths::Vector3 xTargetPos;
	xTargetEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xTargetPos);

	// Get nav agent
	if (!xAgent.HasComponent<Zenith_AIAgentComponent>())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	Zenith_AIAgentComponent& xAI = xAgent.GetComponent<Zenith_AIAgentComponent>();
	Zenith_NavMeshAgent* pxNav = xAI.GetNavMeshAgent();

	if (pxNav == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Check current distance
	if (xAgent.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_Maths::Vector3 xAgentPos;
		xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xAgentPos);
		float fDist = Zenith_Maths::Length(xTargetPos - xAgentPos);

		if (fDist <= m_fAcceptanceRadius)
		{
			pxNav->Stop();
			m_eLastStatus = BTNodeStatus::SUCCESS;
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

	// Check if path failed
	if (m_bPathRequested && !pxNav->HasPath())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	m_eLastStatus = BTNodeStatus::RUNNING;
	return m_eLastStatus;
}

void Zenith_BTAction_MoveToEntity::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strTargetKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strTargetKey.data(), uKeyLen);

	xStream << m_fAcceptanceRadius;
	xStream << m_fRepathInterval;
}

void Zenith_BTAction_MoveToEntity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strTargetKey.resize(uKeyLen);
	xStream.Read(m_strTargetKey.data(), uKeyLen);

	xStream >> m_fAcceptanceRadius;
	xStream >> m_fRepathInterval;
}

// ========== Zenith_BTAction_SetBlackboardBool ==========

Zenith_BTAction_SetBlackboardBool::Zenith_BTAction_SetBlackboardBool(const std::string& strKey, bool bValue)
	: m_strKey(strKey)
	, m_bValue(bValue)
{
}

BTNodeStatus Zenith_BTAction_SetBlackboardBool::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	xBlackboard.SetBool(m_strKey, m_bValue);
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTAction_SetBlackboardBool::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strKey.data(), uKeyLen);

	xStream << m_bValue;
}

void Zenith_BTAction_SetBlackboardBool::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strKey.resize(uKeyLen);
	xStream.Read(m_strKey.data(), uKeyLen);

	xStream >> m_bValue;
}

// ========== Zenith_BTAction_SetBlackboardFloat ==========

Zenith_BTAction_SetBlackboardFloat::Zenith_BTAction_SetBlackboardFloat(const std::string& strKey, float fValue)
	: m_strKey(strKey)
	, m_fValue(fValue)
{
}

BTNodeStatus Zenith_BTAction_SetBlackboardFloat::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	xBlackboard.SetFloat(m_strKey, m_fValue);
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTAction_SetBlackboardFloat::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strKey.data(), uKeyLen);

	xStream << m_fValue;
}

void Zenith_BTAction_SetBlackboardFloat::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strKey.resize(uKeyLen);
	xStream.Read(m_strKey.data(), uKeyLen);

	xStream >> m_fValue;
}

// ========== Zenith_BTAction_Log ==========

Zenith_BTAction_Log::Zenith_BTAction_Log(const std::string& strMessage)
	: m_strMessage(strMessage)
{
}

BTNodeStatus Zenith_BTAction_Log::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	Zenith_Log(LOG_CATEGORY_AI, "[BT Log] Entity %u: %s", xAgent.GetEntityID().m_uIndex, m_strMessage.c_str());
	m_eLastStatus = BTNodeStatus::SUCCESS;
	return m_eLastStatus;
}

void Zenith_BTAction_Log::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uMsgLen = static_cast<uint32_t>(m_strMessage.length());
	xStream << uMsgLen;
	xStream.Write(m_strMessage.data(), uMsgLen);
}

void Zenith_BTAction_Log::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uMsgLen = 0;
	xStream >> uMsgLen;
	m_strMessage.resize(uMsgLen);
	xStream.Read(m_strMessage.data(), uMsgLen);
}

// ========== Zenith_BTAction_FindPrimaryTarget ==========

BTNodeStatus Zenith_BTAction_FindPrimaryTarget::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	Zenith_EntityID xTarget = Zenith_PerceptionSystem::GetPrimaryTarget(xAgent.GetEntityID());

	if (xTarget.IsValid())
	{
		xBlackboard.SetEntityID(m_strOutputKey, xTarget);

		// Also store target position
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		Zenith_Entity xTargetEntity = xScene.TryGetEntity(xTarget);
		if (xTargetEntity.IsValid() && xTargetEntity.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Vector3 xPos;
			xTargetEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
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
