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
#include "Combat_EnemyComponent.h"
#include "Combat_DamageSystem.h"

// Shared self-component resolvers for the graph nodes (self == the entity the
// driving event fires on: the player for "AttackTick", the enemy for
// "EnemyBrainTick").
namespace Combat_GraphNodeDetail
{
	inline Combat_PlayerComponent* ResolvePlayer(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Combat_PlayerComponent>() : nullptr;
	}

	inline Combat_EnemyComponent* ResolveEnemy(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Combat_EnemyComponent>() : nullptr;
	}

	inline Combat_GameComponent* ResolveGame(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Combat_GameComponent>() : nullptr;
	}
}

// ----------------------------------------------------------------------------
// Combat_PlayerAttack.bgraph (decomposed CombatAttackFlow)
// ----------------------------------------------------------------------------
// The old CombatAttackFlow mega-node (Combat_PlayerComponent::UpdateAttack) ran
// three independent guards top-to-bottom every AttackTick. It decomposes into a
// query node + single-action nodes chained under three "AttackTick"
// OnCustomEvent sources (fired in node order, preserving the guard order):
//   1. QueryAttackState -> Branch(attackJustStarted) -> ActivateHitbox
//   2. Branch(hitFrameReady) -> RegisterHits -> Compare(uHits>0) -> NotifyComboHit
//   3. Branch(isAttacking).false -> DeactivateHitbox
// QueryAttackState heads the first chain so the blackboard is populated before
// the later guards read it. All numbers/order/short-circuits are preserved.

// Reads the player controller's attack state into the blackboard. FAILS
// (aborting its chain) when the player component or transform is missing - the
// two early-FAILURE guards of the old mega-node.
class CombatNode_QueryAttackState : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_QueryAttackState)
public:
	ZENITH_PROPERTY(std::string, m_strAttackStartedVar, "attackJustStarted")
	ZENITH_PROPERTY(std::string, m_strIsAttackingVar, "isAttacking")
	ZENITH_PROPERTY(std::string, m_strAttackTypeVar, "attackType")
	ZENITH_PROPERTY(std::string, m_strComboCountVar, "comboCount")
	ZENITH_PROPERTY(std::string, m_strHitFrameVar, "hitFrameReady")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>() == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const Combat_PlayerController& xController = pxShim->GetController();

		Zenith_PropertyValue xVal;
		xVal.SetBool(xController.WasAttackJustStarted());
		xContext.m_pxBlackboard->SetValue(m_strAttackStartedVar, xVal);
		const bool bIsAttacking = xController.IsAttacking();
		xVal.SetBool(bIsAttacking);
		xContext.m_pxBlackboard->SetValue(m_strIsAttackingVar, xVal);
		xVal.SetInt32(static_cast<int32_t>(xController.GetCurrentAttackType()));
		xContext.m_pxBlackboard->SetValue(m_strAttackTypeVar, xVal);
		xVal.SetInt32(static_cast<int32_t>(xController.GetComboCount()));
		xContext.m_pxBlackboard->SetValue(m_strComboCountVar, xVal);
		// Guard B's short-circuit AND (IsAttacking() first, then hit-frame).
		xVal.SetBool(bIsAttacking && pxShim->AnimController().IsAttackHitFrame());
		xContext.m_pxBlackboard->SetValue(m_strHitFrameVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatQueryAttackState"; }
};

// Activates the attack hitbox on attack start. Damage/range per attack type are
// designer node properties (10/1.5 light, 25/2.0 heavy); combo flag = combo > 1.
class CombatNode_ActivateHitbox : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_ActivateHitbox)
public:
	ZENITH_PROPERTY(float, m_fLightDamage, 10.0f)
	ZENITH_PROPERTY(float, m_fLightRange, 1.5f)
	ZENITH_PROPERTY(float, m_fHeavyDamage, 25.0f)
	ZENITH_PROPERTY(float, m_fHeavyRange, 2.0f)
	ZENITH_PROPERTY(std::string, m_strAttackTypeVar, "attackType")
	ZENITH_PROPERTY(std::string, m_strComboCountVar, "comboCount")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const int32_t iType = xContext.m_pxBlackboard->GetInt32(m_strAttackTypeVar, 0);
		const bool bHeavy = (iType == static_cast<int32_t>(Combat_AttackType::HEAVY));
		const float fDamage = bHeavy ? m_fHeavyDamage : m_fLightDamage;
		const float fRange = bHeavy ? m_fHeavyRange : m_fLightRange;
		const uint32_t uCombo = static_cast<uint32_t>(xContext.m_pxBlackboard->GetInt32(m_strComboCountVar, 0));
		pxShim->HitDetection().ActivateHitbox(fDamage, fRange, uCombo, uCombo > 1);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatActivateHitbox"; }
};

// Registers hits on the attack hit-frame (overlap scan) and writes the number
// of new hits this frame to the blackboard for the combo-push guard.
class CombatNode_RegisterHits : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_RegisterHits)
public:
	ZENITH_PROPERTY(std::string, m_strHitCountVar, "uHits")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_TransformComponent* pxTransform = xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const uint32_t uHits = pxShim->HitDetection().Update(*pxTransform);
		Zenith_PropertyValue xVal;
		xVal.SetInt32(static_cast<int32_t>(uHits));
		xContext.m_pxBlackboard->SetValue(m_strHitCountVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatRegisterHits"; }
};

// Pushes combo state up to the GameManager HUD when the swing landed a hit.
// Combo count rides the blackboard (the same value QueryAttackState cached, so
// this matches the old node's second GetComboCount() read).
class CombatNode_NotifyComboHit : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_NotifyComboHit)
public:
	ZENITH_PROPERTY(float, m_fComboHitTimer, 2.0f)
	ZENITH_PROPERTY(std::string, m_strComboCountVar, "comboCount")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const uint32_t uCombo = static_cast<uint32_t>(xContext.m_pxBlackboard->GetInt32(m_strComboCountVar, 0));
		Combat_GameComponent::NotifyComboHit(uCombo, m_fComboHitTimer);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatNotifyComboHit"; }
};

// Deactivates the attack hitbox when the player is no longer attacking. Requires
// a transform (as the old mega-node did - its transform-null guard sat above the
// deactivate block, so a null-transform entity performed no deactivation).
class CombatNode_DeactivateHitbox : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>() == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->HitDetection().DeactivateHitbox();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatDeactivateHitbox"; }
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

// ----------------------------------------------------------------------------
// Combat_RoundFlow.bgraph (decomposed CombatCheckRoundState)
// ----------------------------------------------------------------------------
// CombatTickComboTimer above stays a single node deliberately: the combo count
// and timer are cross-entity shared state - written by the player's attack
// graph (NotifyComboHit on the player entity), ticked here on the GameManager,
// and read by the HUD - so a Combat_GameComponent static is their natural home,
// not a per-graph blackboard. The win/lose mega-node decomposes into count +
// compare + SetGameState, preserving VICTORY-then-GAME_OVER order and the
// both-independent semantics (a all-dead + player-dead frame sets VICTORY then
// overwrites with GAME_OVER, exactly as the old body did).

// Counts living registered enemies into the blackboard (aliveCount) and records
// whether any enemy was ever registered (hasEnemies - the !empty victory guard).
class CombatNode_CountAliveEnemies : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_CountAliveEnemies)
public:
	ZENITH_PROPERTY(std::string, m_strAliveCountVar, "aliveCount")
	ZENITH_PROPERTY(std::string, m_strHasEnemiesVar, "hasEnemies")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		uint32_t uAlive = 0;
		for (const Zenith_EntityID uID : Combat_GameComponent::GetEnemyEntityIDs())
		{
			if (!Combat_DamageSystem::IsDead(uID))
			{
				++uAlive;
			}
		}
		Zenith_PropertyValue xVal;
		xVal.SetInt32(static_cast<int32_t>(uAlive));
		xContext.m_pxBlackboard->SetValue(m_strAliveCountVar, xVal);
		xVal.SetBool(!Combat_GameComponent::GetEnemyEntityIDs().empty());
		xContext.m_pxBlackboard->SetValue(m_strHasEnemiesVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatCountAliveEnemies"; }
};

// Writes whether the registered player is dead (valid id AND IsDead) - the
// GAME_OVER guard (short-circuit: valid-id first, then IsDead).
class CombatNode_CheckPlayerDead : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_CheckPlayerDead)
public:
	ZENITH_PROPERTY(std::string, m_strPlayerDeadVar, "playerDead")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID uPlayerID = Combat_GameComponent::GetPlayerEntityID();
		const bool bDead = (uPlayerID != INVALID_ENTITY_ID && Combat_DamageSystem::IsDead(uPlayerID));
		Zenith_PropertyValue xVal;
		xVal.SetBool(bDead);
		xContext.m_pxBlackboard->SetValue(m_strPlayerDeadVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatCheckPlayerDead"; }
};

// Sets the game state to a designer-chosen value (VICTORY / GAME_OVER etc).
// Idempotent - the C++ OnDeathEvent GAME_OVER path and a graph GAME_OVER coexist.
class CombatNode_SetGameState : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_SetGameState)
public:
	ZENITH_PROPERTY(int32_t, m_iState, 1)

	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
		Combat_GameComponent::SetGameState(static_cast<Combat_GameState>(m_iState));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatSetGameState"; }
};

// ----------------------------------------------------------------------------
// Combat_PlayerState.bgraph (attached alongside Combat_PlayerAttack on the player)
// ----------------------------------------------------------------------------
// The old Combat_PlayerController::Update decision switch is deleted; its
// dispatch lives in this graph's StateMachine(playerState). Two "PlayerTick"
// sources run in node order: source 1 = PreTick (clear flags + snapshot +
// UpdateTimers) then the per-state handler via the StateMachine; source 2 =
// PostTick (recompute state-changed). The handler leaves are the old switch
// cases verbatim. dt rides the payload. m_eState stays the C++ source of truth;
// the blackboard mirror only drives the dispatch. No RUNNING leaves.

// Pre-switch tick: clear per-frame flags, snapshot the previous state, tick the
// timers, then sync the (post-UpdateTimers) state to the blackboard so the
// StateMachine dispatches on exactly what the old switch(m_eState) read. FAILS
// if the transform/collider are missing.
class CombatNode_PlayerPreTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_PlayerPreTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")
	ZENITH_PROPERTY(std::string, m_strStateVar, "playerState")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const float fDt = xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f);
		if (!pxShim->Graph_PreTick(fDt)) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xVal;
		xVal.SetInt32(pxShim->Graph_GetPlayerStateInt());
		xContext.m_pxBlackboard->SetValue(m_strStateVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatPlayerPreTick"; }
};

// IDLE / WALKING handler (input -> attack/dodge/move transitions + locomotion).
class CombatNode_PlayerMovementTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_PlayerMovementTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Graph_MovementTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatPlayerMovementTick"; }
};

// Attack-state handler (attack timer + combo-window open/continue).
class CombatNode_PlayerAttackTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_PlayerAttackTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Graph_AttackTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatPlayerAttackTick"; }
};

// DODGING handler (dodge timer + dodge velocity + cooldown arm on finish).
class CombatNode_PlayerDodgeTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_PlayerDodgeTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Graph_DodgeTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatPlayerDodgeTick"; }
};

// HIT_STUN handler (stun timer -> idle).
class CombatNode_PlayerHitStunTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_PlayerHitStunTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Graph_HitStunTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatPlayerHitStunTick"; }
};

// Post-switch tick: recompute the state-changed edge (drives the anim triggers
// back in Combat_PlayerComponent).
class CombatNode_PlayerPostTick : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_PlayerComponent* pxShim = Combat_GraphNodeDetail::ResolvePlayer(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Graph_PostTick();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatPlayerPostTick"; }
};

// ----------------------------------------------------------------------------
// Combat_EnemyBrain.bgraph (runtime-attached per enemy)
// ----------------------------------------------------------------------------
// The old Combat_EnemyAI::Update decision switch is deleted; its dispatch lives
// in this graph's StateMachine(enemyState). Two "EnemyBrainTick" sources run in
// node order: source 1 = PreTick (guards + death-check + cooldown) then the
// per-state handler via the StateMachine; source 2 = PostTick (anim + IK). The
// handler leaves are the old switch cases verbatim (Combat_EnemyAI::UpdateXxx).
// dt rides the payload (custom-event context dt is 0). No RUNNING leaves (enemy
// movement is direct SetLinearVelocity, not navmesh), so the StateMachine's
// transition-abort is a no-op - no ping-pong.

// Pre-switch tick: guards + EnforceUpright + death-check + cooldown decrement,
// then syncs the (post-death-check) state to the blackboard so the StateMachine
// dispatches on exactly what the old switch(m_eState) read. FAILS if the entity
// or transform is gone (the old early-returns).
class CombatNode_EnemyPreTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_EnemyPreTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")
	ZENITH_PROPERTY(std::string, m_strStateVar, "enemyState")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_EnemyComponent* pxEnemy = Combat_GraphNodeDetail::ResolveEnemy(xContext);
		if (pxEnemy == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const float fDt = xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f);
		if (!pxEnemy->Graph_PreTick(fDt)) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xVal;
		xVal.SetInt32(pxEnemy->Graph_GetStateInt());
		xContext.m_pxBlackboard->SetValue(m_strStateVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatEnemyPreTick"; }
};

// IDLE handler (detection + target acquisition).
class CombatNode_EnemyIdleTick : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_EnemyComponent* pxEnemy = Combat_GraphNodeDetail::ResolveEnemy(xContext);
		if (pxEnemy == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxEnemy->Graph_IdleTick();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatEnemyIdleTick"; }
};

// CHASING handler (retarget / attack-gate / move-or-stop).
class CombatNode_EnemyChaseTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_EnemyChaseTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_EnemyComponent* pxEnemy = Combat_GraphNodeDetail::ResolveEnemy(xContext);
		if (pxEnemy == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxEnemy->Graph_ChaseTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatEnemyChaseTick"; }
};

// ATTACKING handler (attack-duration timer + hit overlap).
class CombatNode_EnemyAttackTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_EnemyAttackTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_EnemyComponent* pxEnemy = Combat_GraphNodeDetail::ResolveEnemy(xContext);
		if (pxEnemy == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxEnemy->Graph_AttackTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatEnemyAttackTick"; }
};

// HIT_STUN handler (stun timer -> CHASING).
class CombatNode_EnemyHitStunTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_EnemyHitStunTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_EnemyComponent* pxEnemy = Combat_GraphNodeDetail::ResolveEnemy(xContext);
		if (pxEnemy == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxEnemy->Graph_HitStunTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatEnemyHitStunTick"; }
};

// Post-switch tick: animation + IK from the post-dispatch state.
class CombatNode_EnemyPostTick : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_EnemyPostTick)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_EnemyComponent* pxEnemy = Combat_GraphNodeDetail::ResolveEnemy(xContext);
		if (pxEnemy == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxEnemy->Graph_PostTick(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatEnemyPostTick"; }
};

// ----------------------------------------------------------------------------
// Combat_GameFlow.bgraph (attached to both GameManagers)
// ----------------------------------------------------------------------------
// The menu/pause/reset input DECISIONS move here. The graph runs at order 60
// (before CombatGame's systems at order 100), so a transition it makes this
// frame is seen by the systems pass, reproducing the old switch's
// early-return-skips-systems behaviour. s_eGameState stays the static source of
// truth (SetGameState authoritative); CombatGetGameState mirrors it to the
// blackboard only to gate the P/R/Escape chains.

// Reads the game-state static into the blackboard for the P/R/Escape gates.
class CombatNode_GetGameState : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_GetGameState)
public:
	ZENITH_PROPERTY(std::string, m_strStateVar, "gameState")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_PropertyValue xVal;
		xVal.SetInt32(static_cast<int32_t>(Combat_GameComponent::GetGameState()));
		xContext.m_pxBlackboard->SetValue(m_strStateVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatGetGameState"; }
};

// Pauses / unpauses the arena scene (the old SetScenePaused calls).
class CombatNode_SetScenePaused : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(CombatNode_SetScenePaused)
public:
	ZENITH_PROPERTY(bool, m_bPaused, true)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_GameComponent* pxGame = Combat_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_SetScenePaused(m_bPaused);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatSetScenePaused"; }
};

// Restarts the round (the old ResetGame).
class CombatNode_ResetGame : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_GameComponent* pxGame = Combat_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ResetGame();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatResetGame"; }
};

// Returns to the main menu (the old ReturnToMenu).
class CombatNode_ReturnToMenu : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_GameComponent* pxGame = Combat_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ReturnToMenu();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatReturnToMenu"; }
};

// Keeps the menu Play button focused (the old UpdateMenuInput; a no-op in the
// arena, where there is no MenuPlay button).
class CombatNode_FocusPlayButton : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Combat_GameComponent* pxGame = Combat_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_FocusPlayButton();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "CombatFocusPlayButton"; }
};

inline void Combat_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	// Combat_PlayerAttack.bgraph (decomposed CombatAttackFlow).
	xRegistry.RegisterNodeType<CombatNode_QueryAttackState>("CombatQueryAttackState", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_ActivateHitbox>("CombatActivateHitbox", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_RegisterHits>("CombatRegisterHits", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_NotifyComboHit>("CombatNotifyComboHit", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_DeactivateHitbox>("CombatDeactivateHitbox", GRAPH_EVENT_NONE, 1, false, "Combat");
	// Combat_RoundFlow.bgraph (TickComboTimer kept; CheckRoundState decomposed).
	xRegistry.RegisterNodeType<CombatNode_TickComboTimer>("CombatTickComboTimer", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_CountAliveEnemies>("CombatCountAliveEnemies", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_CheckPlayerDead>("CombatCheckPlayerDead", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_SetGameState>("CombatSetGameState", GRAPH_EVENT_NONE, 1, false, "Combat");
	// Combat_PlayerState.bgraph (deleted Combat_PlayerController::Update switch).
	xRegistry.RegisterNodeType<CombatNode_PlayerPreTick>("CombatPlayerPreTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_PlayerMovementTick>("CombatPlayerMovementTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_PlayerAttackTick>("CombatPlayerAttackTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_PlayerDodgeTick>("CombatPlayerDodgeTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_PlayerHitStunTick>("CombatPlayerHitStunTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_PlayerPostTick>("CombatPlayerPostTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	// Combat_EnemyBrain.bgraph (deleted Combat_EnemyAI::Update decision switch).
	xRegistry.RegisterNodeType<CombatNode_EnemyPreTick>("CombatEnemyPreTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_EnemyIdleTick>("CombatEnemyIdleTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_EnemyChaseTick>("CombatEnemyChaseTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_EnemyAttackTick>("CombatEnemyAttackTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_EnemyHitStunTick>("CombatEnemyHitStunTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_EnemyPostTick>("CombatEnemyPostTick", GRAPH_EVENT_NONE, 1, false, "Combat");
	// Combat_GameFlow.bgraph (menu/pause/reset input decisions).
	xRegistry.RegisterNodeType<CombatNode_GetGameState>("CombatGetGameState", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_SetScenePaused>("CombatSetScenePaused", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_ResetGame>("CombatResetGame", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_ReturnToMenu>("CombatReturnToMenu", GRAPH_EVENT_NONE, 1, false, "Combat");
	xRegistry.RegisterNodeType<CombatNode_FocusPlayButton>("CombatFocusPlayButton", GRAPH_EVENT_NONE, 1, false, "Combat");
}
