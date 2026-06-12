#pragma once
/**
 * Combat_GraphNodes - Combat's Behaviour Graph node library (wave 2).
 *
 * Each node wraps EXACTLY the body of the C++ flow it replaced, so the graph
 * versions are behaviourally identical (proven by the Combat_* characterization
 * tests, which were written against the C++ versions first):
 *
 *   CombatAttackFlow     - Combat_PlayerComponent::UpdateAttack verbatim
 *                          (hitbox activation on attack start, hit registration
 *                          on attack hit-frames, combo push on hits, hitbox
 *                          deactivation when the attack ends). Damage/range
 *                          per attack type are node properties (designer
 *                          tuning), defaults matching the old constants.
 *   CombatTickComboTimer - Combat_GameComponent::UpdateComboTimer verbatim
 *                          (dt arrives as the RoundTick event payload).
 *   CombatCheckRoundState- Combat_GameComponent::CheckGameState verbatim
 *                          (all enemies dead -> VICTORY; player dead ->
 *                          GAME_OVER).
 *
 * The C++ shims fire the driving custom events at EXACTLY the points the old
 * bodies ran ("AttackTick" at the end of Combat_PlayerComponent::OnUpdate;
 * "RoundTick" in the PLAYING branch after deferred-event processing), so
 * same-frame ordering is preserved bit-for-bit.
 *
 * Registered from Project_RegisterGameComponents via Combat_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Combat_GameComponent.h"
#include "Combat_PlayerComponent.h"
#include "Combat_DamageSystem.h"

// The player attack flow: hitbox lifecycle + hit registration + combo push.
// Body is the pre-graph Combat_PlayerComponent::UpdateAttack verbatim; the
// shim exposes the controller (read-only), the animation controller (hit-frame
// query), and the hit detection (systems execution).
class CombatNode_AttackFlow : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_AttackFlow)
public:
	ZENITH_PROPERTY(float, m_fLightDamage, 10.0f)
	ZENITH_PROPERTY(float, m_fLightRange, 1.5f)
	ZENITH_PROPERTY(float, m_fHeavyDamage, 25.0f)
	ZENITH_PROPERTY(float, m_fHeavyRange, 2.0f)
	ZENITH_PROPERTY(float, m_fComboHitTimer, 2.0f)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Combat_PlayerComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (!xContext.m_xSelf.HasComponent<Zenith_TransformComponent>()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_TransformComponent& xTransform = xContext.m_xSelf.GetComponent<Zenith_TransformComponent>();
		const Combat_PlayerController& xController = pxShim->GetController();

		if (xController.WasAttackJustStarted())
		{
			const Combat_AttackType eType = xController.GetCurrentAttackType();
			const float fDamage = (eType == Combat_AttackType::HEAVY) ? m_fHeavyDamage : m_fLightDamage;
			const float fRange = (eType == Combat_AttackType::HEAVY) ? m_fHeavyRange : m_fLightRange;
			const uint32_t uCombo = xController.GetComboCount();
			pxShim->HitDetection().ActivateHitbox(fDamage, fRange, uCombo, uCombo > 1);
		}

		if (xController.IsAttacking() && pxShim->AnimController().IsAttackHitFrame())
		{
			const uint32_t uHits = pxShim->HitDetection().Update(xTransform);
			if (uHits > 0)
			{
				// Push combo state up to the GameManager so the central HUD
				// picks it up without poking back into the player.
				Combat_GameComponent::NotifyComboHit(xController.GetComboCount(), m_fComboHitTimer);
			}
		}

		if (!xController.IsAttacking())
		{
			pxShim->HitDetection().DeactivateHitbox();
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatAttackFlow"; }
};

// Combo-timer tick - Combat_GameComponent::UpdateComboTimer/TickComboTimer
// verbatim. dt rides the RoundTick event payload (stored to the blackboard by
// the OnCustomEvent source).
class CombatNode_TickComboTimer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_TickComboTimer)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const float fDt = xContext.m_pxBlackboard->GetFloat(m_strDtVar);
		Combat_GameComponent::TickComboTimer(fDt);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatTickComboTimer"; }
};

// Round win/lose decision - Combat_GameComponent::CheckGameState verbatim:
// every registered enemy dead (and at least one was registered) -> VICTORY;
// the player dead -> GAME_OVER.
class CombatNode_CheckRoundState : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
		uint32_t uAlive = 0;
		for (const Zenith_EntityID uID : Combat_GameComponent::GetEnemyEntityIDs())
		{
			if (!Combat_DamageSystem::IsDead(uID))
			{
				++uAlive;
			}
		}
		if (uAlive == 0 && !Combat_GameComponent::GetEnemyEntityIDs().empty())
		{
			Combat_GameComponent::SetGameState(Combat_GameState::VICTORY);
		}

		const Zenith_EntityID uPlayerID = Combat_GameComponent::GetPlayerEntityID();
		if (uPlayerID != INVALID_ENTITY_ID && Combat_DamageSystem::IsDead(uPlayerID))
		{
			Combat_GameComponent::SetGameState(Combat_GameState::GAME_OVER);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatCheckRoundState"; }
};

inline void Combat_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<CombatNode_AttackFlow>("CombatAttackFlow", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_TickComboTimer>("CombatTickComboTimer", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_CheckRoundState>("CombatCheckRoundState", GRAPH_EVENT_NONE, 1, false, "Combat");
}
