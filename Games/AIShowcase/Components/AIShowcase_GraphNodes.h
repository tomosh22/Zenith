#pragma once
/**
 * AIShowcase_GraphNodes - AIShowcase's Behaviour Graph node library (W3, the
 * AI node-family pilot).
 *
 * Two boot-authored graphs drive the game:
 *   AIShowcase_GameFlow  (on the GameManager) - menu/state machine + the
 *      discrete player verbs (sound / formation / reset), reaching the systems
 *      component through the GameFlow nodes below.
 *   AIShowcase_EnemyBrain (per enemy, attached in CreateEnemy) - the per-enemy
 *      decision: perceive the player -> chase its last-known position + share
 *      with the squad; else patrol a waypoint. Fired synchronously per enemy
 *      by "EnemyBrainTick" from the C++ driver's Phase 1 (exactly where the old
 *      inline decision sat), so the batch pathfinding + nav movement + the
 *      double-drive quirk are all preserved.
 *
 * GameFlow nodes resolve the GameManager's AIShowcase_GameComponent (self);
 * EnemyBrain nodes work off the enemy entity + the AI systems directly (self =
 * the enemy, perception/nav/squad keyed by its id). Registered from
 * Project_RegisterGameComponents via AIShowcase_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "UI/Zenith_UIButton.h"

#include "AIShowcase_GameComponent.h"

namespace
{
	inline AIShowcase_GameComponent* AIShowcase_ResolveGame(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<AIShowcase_GameComponent>() : nullptr;
	}
}

// ============================================================================
// GameFlow nodes (self = the GameManager entity)
// ============================================================================

// The single menu button holds keyboard focus every frame (Enter activates
// through the UIButton's own focused-Enter path into the engine trampoline).
class AIShowcaseNode_FocusPlayButton : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_UIComponent* pxUI = xContext.m_xSelf.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_UI::Zenith_UIButton* pxPlay = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxPlay->SetFocused(true);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseFocusPlayButton"; }
};

// Tear down the arena and return to the main menu (the old ReturnToMenu).
class AIShowcaseNode_ReturnToMenu : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		AIShowcase_GameComponent* pxGame = AIShowcase_ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ReturnToMenu();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseReturnToMenu"; }
};

// Rebuild + re-initialise the arena (the old ResetDemo).
class AIShowcaseNode_ResetDemo : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		AIShowcase_GameComponent* pxGame = AIShowcase_ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ResetDemo();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseResetDemo"; }
};

// Pause / unpause the arena scene (the StateMachine Enter/Exit_Paused seam).
class AIShowcaseNode_SetArenaPaused : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(AIShowcaseNode_SetArenaPaused)
public:
	ZENITH_PROPERTY(bool, m_bPaused, true)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		AIShowcase_GameComponent* pxGame = AIShowcase_ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_SetArenaPaused(m_bPaused);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseSetArenaPaused"; }
};

// Emit the player's "attack" hearing stimulus (the old Space handler).
class AIShowcaseNode_EmitPlayerSound : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		AIShowcase_GameComponent* pxGame = AIShowcase_ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_EmitPlayerSound();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseEmitPlayerSound"; }
};

// Select a squad formation (the old 1-5 handlers; 0..4 = Line/Wedge/Column/
// Circle/Skirmish).
class AIShowcaseNode_SetFormation : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(AIShowcaseNode_SetFormation)
public:
	ZENITH_PROPERTY(int32_t, m_iFormation, 0)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		AIShowcase_GameComponent* pxGame = AIShowcase_ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (m_iFormation < 0 || m_iFormation > 4) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_SetFormation(static_cast<uint32_t>(m_iFormation));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseSetFormation"; }
};

// ============================================================================
// EnemyBrain nodes (self = the enemy entity)
// ============================================================================

// The perception query: does this enemy currently perceive the player? On
// SUCCESS writes the player's last-known position to chaseDest (the old inline
// "iterate perceived targets, match the player id, chase its last-known pos").
// FAILURE = not perceived (the Selector falls through to patrol).
class AIShowcaseNode_SensePlayer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(AIShowcaseNode_SensePlayer)
public:
	ZENITH_PROPERTY(std::string, m_strPlayerVar, "playerTarget")
	ZENITH_PROPERTY(std::string, m_strChaseDestVar, "chaseDest")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		const u_int64 ulPlayer = xContext.m_pxBlackboard->GetPackedEntityID(m_strPlayerVar, 0);
		if (ulPlayer == 0) return GRAPH_NODE_STATUS_FAILURE;	// unseeded target
		const Zenith_EntityID xPlayer = Zenith_EntityID::FromPacked(ulPlayer);

		const Zenith_Vector<Zenith_PerceivedTarget>* paxTargets =
			Zenith_PerceptionSystem::GetPerceivedTargets(xContext.m_xSelf.GetEntityID());
		if (paxTargets == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		for (u_int u = 0; u < paxTargets->GetSize(); ++u)
		{
			const Zenith_PerceivedTarget& xTarget = paxTargets->Get(u);
			if (xTarget.m_xEntityID == xPlayer)
			{
				Zenith_PropertyValue xValue;
				xValue.SetVector3(xTarget.m_xLastKnownPosition);
				xContext.m_pxBlackboard->SetValue(m_strChaseDestVar, xValue);
				return GRAPH_NODE_STATUS_SUCCESS;
			}
		}
		return GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "AIShowcaseSensePlayer"; }
};

// Share the perceived player + its position with this enemy's squad (best
// effort - no squad is a no-op, so the chase chain still sets the destination).
class AIShowcaseNode_ShareTargetWithSquad : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(AIShowcaseNode_ShareTargetWithSquad)
public:
	ZENITH_PROPERTY(std::string, m_strPlayerVar, "playerTarget")
	ZENITH_PROPERTY(std::string, m_strChaseDestVar, "chaseDest")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		const Zenith_EntityID xEnemy = xContext.m_xSelf.GetEntityID();
		Zenith_Squad* pxSquad = Zenith_SquadManager::GetSquadForEntity(xEnemy);
		if (pxSquad != nullptr)
		{
			const Zenith_EntityID xPlayer =
				Zenith_EntityID::FromPacked(xContext.m_pxBlackboard->GetPackedEntityID(m_strPlayerVar, 0));
			const Zenith_Maths::Vector3 xPos =
				xContext.m_pxBlackboard->GetVector3(m_strChaseDestVar, Zenith_Maths::Vector3(0.0f));
			pxSquad->ShareTargetInfo(xPlayer, xPos, xEnemy);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcaseShareTargetWithSquad"; }
};

// The patrol fallback: when the enemy's nav is idle or has arrived, pick the
// next waypoint (enemy index + patrol timer, the old formula) and issue it.
// Always SUCCESS. Reads the patrol timer from the EnemyBrainTick payload.
class AIShowcaseNode_Patrol : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(AIShowcaseNode_Patrol)
public:
	ZENITH_PROPERTY(std::string, m_strEnemyIndexVar, "enemyIndex")
	ZENITH_PROPERTY(std::string, m_strPatrolTimerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_AIAgentComponent* pxAgent = xContext.m_xSelf.TryGetComponent<Zenith_AIAgentComponent>();
		Zenith_NavMeshAgent* pxNav = pxAgent ? pxAgent->GetNavMeshAgent() : nullptr;
		if (pxNav == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		if (!pxNav->HasPath() || pxNav->HasReachedDestination())
		{
			static const Zenith_Maths::Vector3 s_axPatrolPoints[4] = {
				Zenith_Maths::Vector3(-15.0f, 0.0f, 0.0f),
				Zenith_Maths::Vector3(15.0f, 0.0f, 0.0f),
				Zenith_Maths::Vector3(0.0f, 0.0f, -12.0f),
				Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f),
			};
			const int32_t iIndex = xContext.m_pxBlackboard->GetInt32(m_strEnemyIndexVar, 0);
			const float fTimer = xContext.m_pxBlackboard->GetFloat(m_strPatrolTimerVar, 0.0f);
			const uint32_t uIdx = (static_cast<uint32_t>(iIndex) + static_cast<uint32_t>(fTimer * 0.5f)) % 4;
			pxNav->SetDestination(s_axPatrolPoints[uIdx]);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "AIShowcasePatrol"; }
};

// ============================================================================
// Registration
// ============================================================================
inline void AIShowcase_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	// GameFlow
	xRegistry.RegisterNodeType<AIShowcaseNode_FocusPlayButton>("AIShowcaseFocusPlayButton", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_ReturnToMenu>("AIShowcaseReturnToMenu", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_ResetDemo>("AIShowcaseResetDemo", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_SetArenaPaused>("AIShowcaseSetArenaPaused", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_EmitPlayerSound>("AIShowcaseEmitPlayerSound", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_SetFormation>("AIShowcaseSetFormation", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	// EnemyBrain
	xRegistry.RegisterNodeType<AIShowcaseNode_SensePlayer>("AIShowcaseSensePlayer", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_ShareTargetWithSquad>("AIShowcaseShareTargetWithSquad", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
	xRegistry.RegisterNodeType<AIShowcaseNode_Patrol>("AIShowcasePatrol", GRAPH_EVENT_NONE, 1, false, "AIShowcase");
}
