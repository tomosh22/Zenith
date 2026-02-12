#include "Zenith.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_Primitives.h"

static Zenith_Maths::Vector3 RoleToDebugColor(SquadRole eRole)
{
	switch (eRole)
	{
	case SquadRole::LEADER:    return Zenith_Maths::Vector3(1.0f, 0.84f, 0.0f);
	case SquadRole::ASSAULT:   return Zenith_Maths::Vector3(1.0f, 0.3f, 0.3f);
	case SquadRole::SUPPORT:   return Zenith_Maths::Vector3(0.3f, 0.3f, 1.0f);
	case SquadRole::FLANKER:   return Zenith_Maths::Vector3(1.0f, 0.6f, 0.2f);
	case SquadRole::OVERWATCH: return Zenith_Maths::Vector3(0.8f, 0.2f, 0.8f);
	case SquadRole::MEDIC:     return Zenith_Maths::Vector3(0.2f, 1.0f, 0.2f);
	default:                   return Zenith_Maths::Vector3(0.7f, 0.7f, 0.7f);
	}
}
#endif

// ========== Zenith_Squad ==========

Zenith_Squad::Zenith_Squad()
	: m_strName("Unnamed Squad")
{
	m_pxFormation = Zenith_Formation::GetWedge();
}

Zenith_Squad::Zenith_Squad(const std::string& strName)
	: m_strName(strName)
{
	m_pxFormation = Zenith_Formation::GetWedge();
}

void Zenith_Squad::AddMember(Zenith_EntityID xEntity, SquadRole eRole)
{
	// Check if already a member
	if (HasMember(xEntity))
	{
		return;
	}

	Zenith_SquadMember xMember;
	xMember.m_xEntityID = xEntity;
	xMember.m_eRole = eRole;
	xMember.m_bAlive = true;
	m_axMembers.PushBack(xMember);

	// Auto-assign leader if explicitly LEADER role, or first member with default ASSAULT role
	// Don't auto-promote members who were explicitly assigned non-leader roles (FLANKER, SUPPORT, etc)
	if (eRole == SquadRole::LEADER || (!HasLeader() && eRole == SquadRole::ASSAULT))
	{
		SetLeader(xEntity);
	}

	// Reassign formation slots
	AssignFormationSlots();

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Added member %u as %s",
		m_strName.c_str(), xEntity.m_uIndex, GetSquadRoleName(eRole));
}

void Zenith_Squad::RemoveMember(Zenith_EntityID xEntity)
{
	int32_t iIndex = FindMemberIndex(xEntity);
	if (iIndex < 0)
	{
		return;
	}

	// Swap-and-pop removal
	uint32_t uIndex = static_cast<uint32_t>(iIndex);
	uint32_t uLast = m_axMembers.GetSize() - 1;
	if (uIndex != uLast)
	{
		m_axMembers.Get(uIndex) = m_axMembers.Get(uLast);
	}
	m_axMembers.PopBack();

	// If this was the leader, assign a new one
	if (m_xLeaderID == xEntity)
	{
		m_xLeaderID = Zenith_EntityID();
		AutoAssignLeader();
	}

	AssignFormationSlots();

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Removed member %u", m_strName.c_str(), xEntity.m_uIndex);
}

bool Zenith_Squad::HasMember(Zenith_EntityID xEntity) const
{
	return FindMemberIndex(xEntity) >= 0;
}

Zenith_SquadMember* Zenith_Squad::GetMember(Zenith_EntityID xEntity)
{
	int32_t iIndex = FindMemberIndex(xEntity);
	if (iIndex >= 0)
	{
		return &m_axMembers.Get(static_cast<uint32_t>(iIndex));
	}
	return nullptr;
}

const Zenith_SquadMember* Zenith_Squad::GetMember(Zenith_EntityID xEntity) const
{
	int32_t iIndex = FindMemberIndex(xEntity);
	if (iIndex >= 0)
	{
		return &m_axMembers.Get(static_cast<uint32_t>(iIndex));
	}
	return nullptr;
}

uint32_t Zenith_Squad::GetAliveMemberCount() const
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		if (m_axMembers.Get(u).m_bAlive)
		{
			++uCount;
		}
	}
	return uCount;
}

void Zenith_Squad::SetLeader(Zenith_EntityID xEntity)
{
	// Update old leader's role if they exist
	if (m_xLeaderID.IsValid() && m_xLeaderID != xEntity)
	{
		Zenith_SquadMember* pxOldLeader = GetMember(m_xLeaderID);
		if (pxOldLeader && pxOldLeader->m_eRole == SquadRole::LEADER)
		{
			pxOldLeader->m_eRole = SquadRole::ASSAULT;
		}
	}

	m_xLeaderID = xEntity;

	// Update new leader's role
	Zenith_SquadMember* pxNewLeader = GetMember(xEntity);
	if (pxNewLeader)
	{
		pxNewLeader->m_eRole = SquadRole::LEADER;
	}

	AssignFormationSlots();
}

void Zenith_Squad::SetFormation(const Zenith_Formation* pxFormation)
{
	m_pxFormation = pxFormation;
	AssignFormationSlots();
}

void Zenith_Squad::UpdateFormationPositions()
{
	if (m_pxFormation == nullptr || !HasLeader())
	{
		return;
	}

	// Get leader position and rotation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return;
	}

	Zenith_Entity xLeaderEntity = pxSceneData->TryGetEntity(m_xLeaderID);
	if (!xLeaderEntity.IsValid() || !xLeaderEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_TransformComponent& xLeaderTransform = xLeaderEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLeaderPos;
	xLeaderTransform.GetPosition(xLeaderPos);
	Zenith_Maths::Quaternion xLeaderRot;
	xLeaderTransform.GetRotation(xLeaderRot);

	// Update each member's target formation position
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		Zenith_SquadMember& xMember = m_axMembers.Get(u);
		if (xMember.m_iFormationSlot >= 0)
		{
			xMember.m_xFormationOffset = m_pxFormation->GetWorldPositionForSlot(
				static_cast<uint32_t>(xMember.m_iFormationSlot),
				xLeaderPos,
				xLeaderRot
			);
		}
		else
		{
			// No slot assigned, just follow leader
			xMember.m_xFormationOffset = xLeaderPos;
		}
	}
}

Zenith_Maths::Vector3 Zenith_Squad::GetFormationPositionFor(Zenith_EntityID xEntity) const
{
	const Zenith_SquadMember* pxMember = GetMember(xEntity);
	if (pxMember)
	{
		return pxMember->m_xFormationOffset;
	}
	return Zenith_Maths::Vector3(0.0f);
}

// ========== Orders ==========

void Zenith_Squad::OrderMoveTo(const Zenith_Maths::Vector3& xPosition)
{
	m_xCurrentOrder.m_eType = SquadOrderType::MOVE_TO;
	m_xCurrentOrder.m_xTargetPosition = xPosition;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
	m_xCurrentOrder.m_fTimeIssued = 0.0f; // Would use game time

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to move to (%.1f, %.1f, %.1f)",
		m_strName.c_str(), xPosition.x, xPosition.y, xPosition.z);
}

void Zenith_Squad::OrderAttack(Zenith_EntityID xTarget)
{
	m_xCurrentOrder.m_eType = SquadOrderType::ATTACK;
	m_xCurrentOrder.m_xTargetEntity = xTarget;
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to attack target %u",
		m_strName.c_str(), xTarget.m_uIndex);
}

void Zenith_Squad::OrderDefend(const Zenith_Maths::Vector3& xPosition)
{
	m_xCurrentOrder.m_eType = SquadOrderType::DEFEND;
	m_xCurrentOrder.m_xTargetPosition = xPosition;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to defend (%.1f, %.1f, %.1f)",
		m_strName.c_str(), xPosition.x, xPosition.y, xPosition.z);
}

void Zenith_Squad::OrderFlank(Zenith_EntityID xTarget)
{
	m_xCurrentOrder.m_eType = SquadOrderType::FLANK;
	m_xCurrentOrder.m_xTargetEntity = xTarget;
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to flank target %u",
		m_strName.c_str(), xTarget.m_uIndex);
}

void Zenith_Squad::OrderSuppress(const Zenith_Maths::Vector3& xTargetArea)
{
	m_xCurrentOrder.m_eType = SquadOrderType::SUPPRESS;
	m_xCurrentOrder.m_xTargetPosition = xTargetArea;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to suppress area (%.1f, %.1f, %.1f)",
		m_strName.c_str(), xTargetArea.x, xTargetArea.y, xTargetArea.z);
}

void Zenith_Squad::OrderRegroup()
{
	m_xCurrentOrder.m_eType = SquadOrderType::REGROUP;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to regroup", m_strName.c_str());
}

void Zenith_Squad::OrderRetreat(const Zenith_Maths::Vector3& xFallbackPosition)
{
	m_xCurrentOrder.m_eType = SquadOrderType::RETREAT;
	m_xCurrentOrder.m_xTargetPosition = xFallbackPosition;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to retreat to (%.1f, %.1f, %.1f)",
		m_strName.c_str(), xFallbackPosition.x, xFallbackPosition.y, xFallbackPosition.z);
}

void Zenith_Squad::OrderHoldPosition()
{
	m_xCurrentOrder.m_eType = SquadOrderType::HOLD_POSITION;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
	m_xCurrentOrder.m_fTimeIssued = 0.0f;

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Ordered to hold position", m_strName.c_str());
}

void Zenith_Squad::ClearOrder()
{
	m_xCurrentOrder.m_eType = SquadOrderType::NONE;
	m_xCurrentOrder.m_xTargetEntity = Zenith_EntityID();
}

// ========== Shared Knowledge ==========

void Zenith_Squad::ShareTargetInfo(Zenith_EntityID xTarget, const Zenith_Maths::Vector3& xPosition, Zenith_EntityID xReportedBy)
{
	// Check if we already know about this target
	for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
	{
		if (m_axSharedTargets.Get(u).m_xTargetID == xTarget)
		{
			// Update existing info
			m_axSharedTargets.Get(u).m_xLastKnownPosition = xPosition;
			m_axSharedTargets.Get(u).m_fTimeLastSeen = 0.0f;
			m_axSharedTargets.Get(u).m_xReportedBy = xReportedBy;
			return;
		}
	}

	// Add new target
	Zenith_SharedTarget xSharedTarget;
	xSharedTarget.m_xTargetID = xTarget;
	xSharedTarget.m_xLastKnownPosition = xPosition;
	xSharedTarget.m_fTimeLastSeen = 0.0f;
	xSharedTarget.m_xReportedBy = xReportedBy;
	xSharedTarget.m_bEngaged = false;
	m_axSharedTargets.PushBack(xSharedTarget);

	Zenith_Log(LOG_CATEGORY_AI, "Squad '%s': Shared target %u at (%.1f, %.1f, %.1f)",
		m_strName.c_str(), xTarget.m_uIndex, xPosition.x, xPosition.y, xPosition.z);
}

bool Zenith_Squad::IsTargetKnown(Zenith_EntityID xTarget) const
{
	for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
	{
		if (m_axSharedTargets.Get(u).m_xTargetID == xTarget)
		{
			return true;
		}
	}
	return false;
}

const Zenith_SharedTarget* Zenith_Squad::GetSharedTarget(Zenith_EntityID xTarget) const
{
	for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
	{
		if (m_axSharedTargets.Get(u).m_xTargetID == xTarget)
		{
			return &m_axSharedTargets.Get(u);
		}
	}
	return nullptr;
}

void Zenith_Squad::SetTargetEngaged(Zenith_EntityID xTarget, Zenith_EntityID xEngagedBy)
{
	for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
	{
		if (m_axSharedTargets.Get(u).m_xTargetID == xTarget)
		{
			m_axSharedTargets.Get(u).m_bEngaged = true;
			m_axSharedTargets.Get(u).m_xEngagedBy = xEngagedBy;
			return;
		}
	}
}

bool Zenith_Squad::IsTargetEngaged(Zenith_EntityID xTarget) const
{
	for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
	{
		if (m_axSharedTargets.Get(u).m_xTargetID == xTarget)
		{
			return m_axSharedTargets.Get(u).m_bEngaged;
		}
	}
	return false;
}

Zenith_EntityID Zenith_Squad::GetPriorityTarget() const
{
	// Return most recently seen, unengaged target
	Zenith_EntityID xBestTarget;
	float fBestTime = FLT_MAX;

	for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
	{
		const Zenith_SharedTarget& xTarget = m_axSharedTargets.Get(u);
		if (!xTarget.m_bEngaged && xTarget.m_fTimeLastSeen < fBestTime)
		{
			xBestTarget = xTarget.m_xTargetID;
			fBestTime = xTarget.m_fTimeLastSeen;
		}
	}

	// If all engaged, return most recently seen anyway
	if (!xBestTarget.IsValid() && m_axSharedTargets.GetSize() > 0)
	{
		for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
		{
			if (m_axSharedTargets.Get(u).m_fTimeLastSeen < fBestTime)
			{
				xBestTarget = m_axSharedTargets.Get(u).m_xTargetID;
				fBestTime = m_axSharedTargets.Get(u).m_fTimeLastSeen;
			}
		}
	}

	return xBestTarget;
}

// ========== Update ==========

void Zenith_Squad::Update(float fDt)
{
	// Update formation positions periodically
	m_fTimeSinceFormationUpdate += fDt;
	if (m_fTimeSinceFormationUpdate >= m_fFormationUpdateInterval)
	{
		UpdateFormationPositions();
		m_fTimeSinceFormationUpdate = 0.0f;
	}

	// Update shared knowledge
	UpdateSharedKnowledge(fDt);

	// Validate members still exist
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return;
	}

	for (int32_t i = static_cast<int32_t>(m_axMembers.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_Entity xEntity = pxSceneData->TryGetEntity(m_axMembers.Get(static_cast<uint32_t>(i)).m_xEntityID);
		if (!xEntity.IsValid())
		{
			RemoveMember(m_axMembers.Get(static_cast<uint32_t>(i)).m_xEntityID);
		}
	}
}

// ========== Role Management ==========

void Zenith_Squad::AssignRole(Zenith_EntityID xEntity, SquadRole eRole)
{
	Zenith_SquadMember* pxMember = GetMember(xEntity);
	if (pxMember)
	{
		pxMember->m_eRole = eRole;

		// If assigned as leader, update leader
		if (eRole == SquadRole::LEADER)
		{
			SetLeader(xEntity);
		}
	}
}

SquadRole Zenith_Squad::GetMemberRole(Zenith_EntityID xEntity) const
{
	const Zenith_SquadMember* pxMember = GetMember(xEntity);
	if (pxMember)
	{
		return pxMember->m_eRole;
	}
	return SquadRole::ASSAULT; // Default
}

Zenith_Vector<Zenith_EntityID> Zenith_Squad::GetMembersWithRole(SquadRole eRole) const
{
	Zenith_Vector<Zenith_EntityID> axResult;
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		if (m_axMembers.Get(u).m_eRole == eRole && m_axMembers.Get(u).m_bAlive)
		{
			axResult.PushBack(m_axMembers.Get(u).m_xEntityID);
		}
	}
	return axResult;
}

void Zenith_Squad::MarkMemberDead(Zenith_EntityID xEntity)
{
	Zenith_SquadMember* pxMember = GetMember(xEntity);
	if (pxMember)
	{
		pxMember->m_bAlive = false;

		// If leader died, find new leader
		if (xEntity == m_xLeaderID)
		{
			AutoAssignLeader();
		}
	}
}

void Zenith_Squad::MarkMemberAlive(Zenith_EntityID xEntity)
{
	Zenith_SquadMember* pxMember = GetMember(xEntity);
	if (pxMember)
	{
		pxMember->m_bAlive = true;
	}
}

bool Zenith_Squad::IsMemberAlive(Zenith_EntityID xEntity) const
{
	const Zenith_SquadMember* pxMember = GetMember(xEntity);
	if (pxMember)
	{
		return pxMember->m_bAlive;
	}
	return false;
}

#ifdef ZENITH_TOOLS
void Zenith_Squad::DebugDraw() const
{
	if (!Zenith_AIDebugVariables::s_bEnableAllAIDebug)
	{
		return;
	}

	if (!HasLeader() || m_axMembers.GetSize() == 0)
	{
		return;
	}

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return;
	}

	// Get leader position
	Zenith_Entity xLeaderEntity = pxSceneData->TryGetEntity(m_xLeaderID);
	if (!xLeaderEntity.IsValid() || !xLeaderEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_Maths::Vector3 xLeaderPos;
	xLeaderEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xLeaderPos);
	xLeaderPos.y += 2.0f; // Raise above head

	// Draw lines from leader to all members (controlled by s_bDrawSquadLinks)
	if (Zenith_AIDebugVariables::s_bDrawSquadLinks)
	{
		for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
		{
			const Zenith_SquadMember& xMember = m_axMembers.Get(u);
			if (!xMember.m_bAlive || xMember.m_xEntityID == m_xLeaderID)
			{
				continue;
			}

			Zenith_Entity xMemberEntity = pxSceneData->TryGetEntity(xMember.m_xEntityID);
			if (!xMemberEntity.IsValid() || !xMemberEntity.HasComponent<Zenith_TransformComponent>())
			{
				continue;
			}

			Zenith_Maths::Vector3 xMemberPos;
			xMemberEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xMemberPos);
			xMemberPos.y += 2.0f;

			Flux_Primitives::AddLine(xLeaderPos, xMemberPos, RoleToDebugColor(xMember.m_eRole));
		}

		// Draw leader marker (gold crown)
		Flux_Primitives::AddSphere(xLeaderPos + Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f), 0.2f, Zenith_Maths::Vector3(1.0f, 0.84f, 0.0f));
	}

	// Draw formation target positions (controlled by s_bDrawFormationPositions)
	if (Zenith_AIDebugVariables::s_bDrawFormationPositions)
	{
		for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
		{
			const Zenith_SquadMember& xMember = m_axMembers.Get(u);
			if (!xMember.m_bAlive)
			{
				continue;
			}

			// Draw formation target position (dimmed color)
			Flux_Primitives::AddSphere(xMember.m_xFormationOffset, 0.3f, RoleToDebugColor(xMember.m_eRole) * 0.5f);
		}
	}

	// Draw shared targets (controlled by s_bDrawSharedTargets)
	if (Zenith_AIDebugVariables::s_bDrawSharedTargets)
	{
		for (uint32_t u = 0; u < m_axSharedTargets.GetSize(); ++u)
		{
			const Zenith_SharedTarget& xTarget = m_axSharedTargets.Get(u);
			Zenith_Maths::Vector3 xTargetColor = xTarget.m_bEngaged
				? Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f)
				: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

			Flux_Primitives::AddCross(xTarget.m_xLastKnownPosition + Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.5f, xTargetColor);
		}
	}

	// Draw role labels/indicators (controlled by s_bDrawRoleLabels)
	if (Zenith_AIDebugVariables::s_bDrawRoleLabels)
	{
		for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
		{
			const Zenith_SquadMember& xMember = m_axMembers.Get(u);
			if (!xMember.m_bAlive)
			{
				continue;
			}

			Zenith_Entity xMemberEntity = pxSceneData->TryGetEntity(xMember.m_xEntityID);
			if (!xMemberEntity.IsValid() || !xMemberEntity.HasComponent<Zenith_TransformComponent>())
			{
				continue;
			}

			Zenith_Maths::Vector3 xMemberPos;
			xMemberEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xMemberPos);
			Zenith_Maths::Vector3 xLabelPos = xMemberPos + Zenith_Maths::Vector3(0.0f, 2.5f, 0.0f);

			// Draw role-specific icon/marker above agent
			Zenith_Maths::Vector3 xRoleColor = RoleToDebugColor(xMember.m_eRole);
			switch (xMember.m_eRole)
			{
			case SquadRole::LEADER:
				Flux_Primitives::AddSphere(xLabelPos, 0.15f, xRoleColor);
				Flux_Primitives::AddLine(xLabelPos, xLabelPos + Zenith_Maths::Vector3(-0.15f, 0.25f, 0.0f), xRoleColor);
				Flux_Primitives::AddLine(xLabelPos, xLabelPos + Zenith_Maths::Vector3(0.0f, 0.3f, 0.0f), xRoleColor);
				Flux_Primitives::AddLine(xLabelPos, xLabelPos + Zenith_Maths::Vector3(0.15f, 0.25f, 0.0f), xRoleColor);
				break;
			case SquadRole::ASSAULT:
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(-0.15f, 0.0f, 0.15f),
					xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, -0.15f), xRoleColor, 0.03f);
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(0.15f, 0.0f, 0.15f),
					xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, -0.15f), xRoleColor, 0.03f);
				break;
			case SquadRole::SUPPORT:
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(-0.15f, 0.0f, 0.0f),
					xLabelPos + Zenith_Maths::Vector3(0.15f, 0.0f, 0.0f), xRoleColor, 0.03f);
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, -0.15f),
					xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, 0.15f), xRoleColor, 0.03f);
				break;
			case SquadRole::FLANKER:
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(-0.15f, 0.0f, 0.0f),
					xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, 0.15f), xRoleColor, 0.03f);
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, 0.15f),
					xLabelPos + Zenith_Maths::Vector3(0.15f, 0.0f, 0.0f), xRoleColor, 0.03f);
				break;
			case SquadRole::OVERWATCH:
				Flux_Primitives::AddCircle(xLabelPos, 0.12f, xRoleColor);
				Flux_Primitives::AddSphere(xLabelPos, 0.05f, xRoleColor);
				break;
			case SquadRole::MEDIC:
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(-0.12f, 0.0f, 0.0f),
					xLabelPos + Zenith_Maths::Vector3(0.12f, 0.0f, 0.0f), xRoleColor, 0.04f);
				Flux_Primitives::AddLine(xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, -0.12f),
					xLabelPos + Zenith_Maths::Vector3(0.0f, 0.0f, 0.12f), xRoleColor, 0.04f);
				break;
			default:
				Flux_Primitives::AddSphere(xLabelPos, 0.1f, xRoleColor);
				break;
			}
		}
	}
}
#endif

// ========== Internal Helpers ==========

void Zenith_Squad::AutoAssignLeader()
{
	m_xLeaderID = Zenith_EntityID();

	// Find first alive member to be leader
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		if (m_axMembers.Get(u).m_bAlive)
		{
			SetLeader(m_axMembers.Get(u).m_xEntityID);
			break;
		}
	}
}

void Zenith_Squad::AssignFormationSlots()
{
	if (m_pxFormation == nullptr)
	{
		return;
	}

	// Reset all slots
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		m_axMembers.Get(u).m_iFormationSlot = -1;
	}

	// Track which slots are taken
	Zenith_Vector<bool> axSlotTaken;
	axSlotTaken.Reserve(m_pxFormation->GetSlotCount());
	for (uint32_t u = 0; u < m_pxFormation->GetSlotCount(); ++u)
	{
		axSlotTaken.PushBack(false);
	}

	// First pass: assign leader to slot 0 (leader slot)
	if (HasLeader())
	{
		Zenith_SquadMember* pxLeader = GetMember(m_xLeaderID);
		if (pxLeader && m_pxFormation->GetSlotCount() > 0)
		{
			pxLeader->m_iFormationSlot = 0;
			axSlotTaken.Get(0) = true;
		}
	}

	// Second pass: assign members to slots matching their role
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		Zenith_SquadMember& xMember = m_axMembers.Get(u);
		if (xMember.m_iFormationSlot >= 0 || !xMember.m_bAlive)
		{
			continue; // Already assigned or dead
		}

		// Find best slot for this role
		for (uint32_t uSlot = 0; uSlot < m_pxFormation->GetSlotCount(); ++uSlot)
		{
			if (axSlotTaken.Get(uSlot))
			{
				continue;
			}

			const Zenith_FormationSlot& xSlot = m_pxFormation->GetSlot(uSlot);
			if (xSlot.m_ePreferredRole == xMember.m_eRole)
			{
				xMember.m_iFormationSlot = static_cast<int32_t>(uSlot);
				axSlotTaken.Get(uSlot) = true;
				break;
			}
		}
	}

	// Third pass: assign remaining members to any available slot
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		Zenith_SquadMember& xMember = m_axMembers.Get(u);
		if (xMember.m_iFormationSlot >= 0 || !xMember.m_bAlive)
		{
			continue;
		}

		for (uint32_t uSlot = 0; uSlot < m_pxFormation->GetSlotCount(); ++uSlot)
		{
			if (!axSlotTaken.Get(uSlot))
			{
				xMember.m_iFormationSlot = static_cast<int32_t>(uSlot);
				axSlotTaken.Get(uSlot) = true;
				break;
			}
		}
	}

	UpdateFormationPositions();
}

void Zenith_Squad::UpdateSharedKnowledge(float fDt)
{
	// Update time since seen and remove old targets
	for (int32_t i = static_cast<int32_t>(m_axSharedTargets.GetSize()) - 1; i >= 0; --i)
	{
		uint32_t uIdx = static_cast<uint32_t>(i);
		m_axSharedTargets.Get(uIdx).m_fTimeLastSeen += fDt;

		if (m_axSharedTargets.Get(uIdx).m_fTimeLastSeen > m_fTargetKnowledgeTimeout)
		{
			// Swap-and-pop removal
			uint32_t uLast = m_axSharedTargets.GetSize() - 1;
			if (uIdx != uLast)
			{
				m_axSharedTargets.Get(uIdx) = m_axSharedTargets.Get(uLast);
			}
			m_axSharedTargets.PopBack();
		}
	}
}

int32_t Zenith_Squad::FindMemberIndex(Zenith_EntityID xEntity) const
{
	for (uint32_t u = 0; u < m_axMembers.GetSize(); ++u)
	{
		if (m_axMembers.Get(u).m_xEntityID == xEntity)
		{
			return static_cast<int32_t>(u);
		}
	}
	return -1;
}

// ========== Zenith_SquadManager ==========

Zenith_Vector<Zenith_Squad*> Zenith_SquadManager::s_axSquads;
bool Zenith_SquadManager::s_bInitialised = false;

void Zenith_SquadManager::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	s_axSquads.Clear();
	s_bInitialised = true;

	Zenith_Log(LOG_CATEGORY_AI, "SquadManager initialised");
}

void Zenith_SquadManager::Shutdown()
{
	for (uint32_t u = 0; u < s_axSquads.GetSize(); ++u)
	{
		delete s_axSquads.Get(u);
	}
	s_axSquads.Clear();
	s_bInitialised = false;

	Zenith_Log(LOG_CATEGORY_AI, "SquadManager shutdown");
}

void Zenith_SquadManager::Update(float fDt)
{
	Zenith_Assert(s_bInitialised, "SquadManager::Update called before Initialise()");

	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_SQUAD_UPDATE);

	for (uint32_t u = 0; u < s_axSquads.GetSize(); ++u)
	{
		s_axSquads.Get(u)->Update(fDt);
	}
}

Zenith_Squad* Zenith_SquadManager::CreateSquad(const std::string& strName)
{
	Zenith_Assert(s_bInitialised, "SquadManager::CreateSquad called before Initialise()");

	Zenith_Squad* pxSquad = new Zenith_Squad(strName);
	s_axSquads.PushBack(pxSquad);

	Zenith_Log(LOG_CATEGORY_AI, "Created squad '%s'", strName.c_str());

	return pxSquad;
}

void Zenith_SquadManager::DestroySquad(Zenith_Squad* pxSquad)
{
	Zenith_Assert(s_bInitialised, "SquadManager::DestroySquad called before Initialise()");

	for (uint32_t u = 0; u < s_axSquads.GetSize(); ++u)
	{
		if (s_axSquads.Get(u) == pxSquad)
		{
			Zenith_Log(LOG_CATEGORY_AI, "Destroyed squad '%s'", pxSquad->GetName().c_str());
			delete pxSquad;
			// Swap-and-pop removal
			uint32_t uLast = s_axSquads.GetSize() - 1;
			if (u != uLast)
			{
				s_axSquads.Get(u) = s_axSquads.Get(uLast);
			}
			s_axSquads.PopBack();
			return;
		}
	}
}

Zenith_Squad* Zenith_SquadManager::GetSquadByName(const std::string& strName)
{
	Zenith_Assert(s_bInitialised, "SquadManager::GetSquadByName called before Initialise()");

	for (uint32_t u = 0; u < s_axSquads.GetSize(); ++u)
	{
		if (s_axSquads.Get(u)->GetName() == strName)
		{
			return s_axSquads.Get(u);
		}
	}
	return nullptr;
}

Zenith_Squad* Zenith_SquadManager::GetSquadForEntity(Zenith_EntityID xEntity)
{
	Zenith_Assert(s_bInitialised, "SquadManager::GetSquadForEntity called before Initialise()");

	for (uint32_t u = 0; u < s_axSquads.GetSize(); ++u)
	{
		if (s_axSquads.Get(u)->HasMember(xEntity))
		{
			return s_axSquads.Get(u);
		}
	}
	return nullptr;
}

uint32_t Zenith_SquadManager::GetSquadCount()
{
	Zenith_Assert(s_bInitialised, "SquadManager::GetSquadCount called before Initialise()");
	return s_axSquads.GetSize();
}

const Zenith_Vector<Zenith_Squad*>& Zenith_SquadManager::GetAllSquads()
{
	Zenith_Assert(s_bInitialised, "SquadManager::GetAllSquads called before Initialise()");
	return s_axSquads;
}

#ifdef ZENITH_TOOLS
void Zenith_SquadManager::DebugDrawAllSquads()
{
	Zenith_Assert(s_bInitialised, "SquadManager::DebugDrawAllSquads called before Initialise()");

	for (uint32_t u = 0; u < s_axSquads.GetSize(); ++u)
	{
		s_axSquads.Get(u)->DebugDraw();
	}
}
#endif
