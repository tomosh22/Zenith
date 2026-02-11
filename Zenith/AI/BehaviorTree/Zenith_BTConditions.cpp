#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTConditions.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include <random>

// ========== Zenith_BTCondition_HasTarget ==========

Zenith_BTCondition_HasTarget::Zenith_BTCondition_HasTarget(const std::string& strTargetKey)
	: m_strTargetKey(strTargetKey)
{
}

BTNodeStatus Zenith_BTCondition_HasTarget::Execute(Zenith_Entity&, Zenith_Blackboard& xBlackboard, float)
{
	Zenith_EntityID xTarget = xBlackboard.GetEntityID(m_strTargetKey);

	if (xTarget.IsValid())
	{
		// Verify entity still exists
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData)
		{
			Zenith_Entity xTargetEntity = pxSceneData->TryGetEntity(xTarget);

			if (xTargetEntity.IsValid())
			{
				m_eLastStatus = BTNodeStatus::SUCCESS;
				return m_eLastStatus;
			}
		}
	}

	m_eLastStatus = BTNodeStatus::FAILURE;
	return m_eLastStatus;
}

void Zenith_BTCondition_HasTarget::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strTargetKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strTargetKey.data(), uKeyLen);
}

void Zenith_BTCondition_HasTarget::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strTargetKey.resize(uKeyLen);
	xStream.Read(m_strTargetKey.data(), uKeyLen);
}

// ========== Zenith_BTCondition_InRange ==========

Zenith_BTCondition_InRange::Zenith_BTCondition_InRange(float fRange, const std::string& strTargetKey)
	: m_fRange(fRange)
	, m_strTargetKey(strTargetKey)
{
}

BTNodeStatus Zenith_BTCondition_InRange::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float)
{
	// Get agent position
	if (!xAgent.HasComponent<Zenith_TransformComponent>())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}
	Zenith_Maths::Vector3 xAgentPos;
	xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xAgentPos);

	// Get target position
	Zenith_Maths::Vector3 xTargetPos;

	// First try as entity
	Zenith_EntityID xTargetID = xBlackboard.GetEntityID(m_strTargetKey);
	if (xTargetID.IsValid())
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData)
		{
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}

		Zenith_Entity xTargetEntity = pxSceneData->TryGetEntity(xTargetID);

		if (xTargetEntity.IsValid() && xTargetEntity.HasComponent<Zenith_TransformComponent>())
		{
			xTargetEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xTargetPos);
		}
		else
		{
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}
	}
	else
	{
		// Try as position
		xTargetPos = xBlackboard.GetVector3(m_strTargetKey);
	}

	// Check distance
	float fDist = Zenith_Maths::Length(xTargetPos - xAgentPos);

	if (fDist <= m_fRange)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
	}
	else
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
	}

	return m_eLastStatus;
}

void Zenith_BTCondition_InRange::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	xStream << m_fRange;

	uint32_t uKeyLen = static_cast<uint32_t>(m_strTargetKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strTargetKey.data(), uKeyLen);
}

void Zenith_BTCondition_InRange::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	xStream >> m_fRange;

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strTargetKey.resize(uKeyLen);
	xStream.Read(m_strTargetKey.data(), uKeyLen);
}

// ========== Zenith_BTCondition_CanSeeTarget ==========

Zenith_BTCondition_CanSeeTarget::Zenith_BTCondition_CanSeeTarget(const std::string& strTargetKey)
	: m_strTargetKey(strTargetKey)
{
}

BTNodeStatus Zenith_BTCondition_CanSeeTarget::Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float)
{
	Zenith_EntityID xTargetID = xBlackboard.GetEntityID(m_strTargetKey);

	if (!xTargetID.IsValid())
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	float fAwareness = Zenith_PerceptionSystem::GetAwarenessOf(xAgent.GetEntityID(), xTargetID);

	if (fAwareness >= m_fMinAwareness)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
	}
	else
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
	}

	return m_eLastStatus;
}

void Zenith_BTCondition_CanSeeTarget::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strTargetKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strTargetKey.data(), uKeyLen);

	xStream << m_fMinAwareness;
}

void Zenith_BTCondition_CanSeeTarget::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strTargetKey.resize(uKeyLen);
	xStream.Read(m_strTargetKey.data(), uKeyLen);

	xStream >> m_fMinAwareness;
}

// ========== Zenith_BTCondition_BlackboardBool ==========

Zenith_BTCondition_BlackboardBool::Zenith_BTCondition_BlackboardBool(const std::string& strKey, bool bExpectedValue)
	: m_strKey(strKey)
	, m_bExpectedValue(bExpectedValue)
{
}

BTNodeStatus Zenith_BTCondition_BlackboardBool::Execute(Zenith_Entity&, Zenith_Blackboard& xBlackboard, float)
{
	bool bValue = xBlackboard.GetBool(m_strKey, !m_bExpectedValue);

	if (bValue == m_bExpectedValue)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
	}
	else
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
	}

	return m_eLastStatus;
}

void Zenith_BTCondition_BlackboardBool::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strKey.data(), uKeyLen);

	xStream << m_bExpectedValue;
}

void Zenith_BTCondition_BlackboardBool::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strKey.resize(uKeyLen);
	xStream.Read(m_strKey.data(), uKeyLen);

	xStream >> m_bExpectedValue;
}

// ========== Zenith_BTCondition_BlackboardCompare ==========

Zenith_BTCondition_BlackboardCompare::Zenith_BTCondition_BlackboardCompare(
	const std::string& strKey, Comparison eComp, float fValue)
	: m_strKey(strKey)
	, m_eComparison(eComp)
	, m_fValue(fValue)
{
}

BTNodeStatus Zenith_BTCondition_BlackboardCompare::Execute(Zenith_Entity&, Zenith_Blackboard& xBlackboard, float)
{
	float fBBValue = xBlackboard.GetFloat(m_strKey, 0.0f);
	bool bResult = false;

	switch (m_eComparison)
	{
	case Comparison::EQUAL:
		bResult = (std::abs(fBBValue - m_fValue) < 0.0001f);
		break;
	case Comparison::NOT_EQUAL:
		bResult = (std::abs(fBBValue - m_fValue) >= 0.0001f);
		break;
	case Comparison::LESS_THAN:
		bResult = (fBBValue < m_fValue);
		break;
	case Comparison::LESS_EQUAL:
		bResult = (fBBValue <= m_fValue);
		break;
	case Comparison::GREATER_THAN:
		bResult = (fBBValue > m_fValue);
		break;
	case Comparison::GREATER_EQUAL:
		bResult = (fBBValue >= m_fValue);
		break;
	}

	m_eLastStatus = bResult ? BTNodeStatus::SUCCESS : BTNodeStatus::FAILURE;
	return m_eLastStatus;
}

void Zenith_BTCondition_BlackboardCompare::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);

	uint32_t uKeyLen = static_cast<uint32_t>(m_strKey.length());
	xStream << uKeyLen;
	xStream.Write(m_strKey.data(), uKeyLen);

	xStream << static_cast<uint8_t>(m_eComparison);
	xStream << m_fValue;
}

void Zenith_BTCondition_BlackboardCompare::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);

	uint32_t uKeyLen = 0;
	xStream >> uKeyLen;
	m_strKey.resize(uKeyLen);
	xStream.Read(m_strKey.data(), uKeyLen);

	uint8_t uComp = 0;
	xStream >> uComp;
	m_eComparison = static_cast<Comparison>(uComp);

	xStream >> m_fValue;
}

// ========== Zenith_BTCondition_HasAwareness ==========

BTNodeStatus Zenith_BTCondition_HasAwareness::Execute(Zenith_Entity& xAgent, Zenith_Blackboard&, float)
{
	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	if (pxTargets == nullptr || pxTargets->GetSize() == 0)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		return m_eLastStatus;
	}

	// Check if any target has sufficient awareness
	for (uint32_t u = 0; u < pxTargets->GetSize(); ++u)
	{
		if (pxTargets->Get(u).m_fAwareness >= m_fMinAwareness)
		{
			m_eLastStatus = BTNodeStatus::SUCCESS;
			return m_eLastStatus;
		}
	}

	m_eLastStatus = BTNodeStatus::FAILURE;
	return m_eLastStatus;
}

// ========== Zenith_BTCondition_Random ==========

Zenith_BTCondition_Random::Zenith_BTCondition_Random(float fProbability)
	: m_fProbability(fProbability)
{
}

BTNodeStatus Zenith_BTCondition_Random::Execute(Zenith_Entity&, Zenith_Blackboard&, float)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<float> dis(0.0f, 1.0f);

	float fRoll = dis(gen);

	if (fRoll < m_fProbability)
	{
		m_eLastStatus = BTNodeStatus::SUCCESS;
	}
	else
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
	}

	return m_eLastStatus;
}

void Zenith_BTCondition_Random::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_BTLeaf::WriteToDataStream(xStream);
	xStream << m_fProbability;
}

void Zenith_BTCondition_Random::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_BTLeaf::ReadFromDataStream(xStream);
	xStream >> m_fProbability;
}
