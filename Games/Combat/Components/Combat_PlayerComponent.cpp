#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Combat_PlayerComponent.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Physics/Zenith_Physics.h"

#include "Combat_GameComponent.h"   // static GameManager state (player/enemy registry, game state)
#include "Combat_DamageSystem.h"

void Combat_PlayerComponent::OnAwake()
{
	// Register so the GameManager and other components (enemy AI) can find us via
	// Combat_GameComponent::GetPlayerEntityID() instead of name scans.
	Combat_GameComponent::RegisterPlayer(m_xParentEntity.GetEntityID());
	m_xHitDetection.SetOwner(m_xParentEntity.GetEntityID());
	Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Combat_PlayerComponent] OnAwake on entity %u (registered)",
		m_xParentEntity.GetEntityID().m_uIndex);
}

void Combat_PlayerComponent::OnStart()
{
	// Animator skeleton is auto-discovered from ModelComponent during the entity's
	// first frame, so initialise the animation controller here rather than in OnAwake.
	if (Zenith_AnimatorComponent* pxAnimator = m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>())
	{
		Zenith_AnimatorComponent& xAnimator = *pxAnimator;
		m_xAnimController.Initialize(xAnimator);
	}
}

void Combat_PlayerComponent::OnDestroy()
{
	Combat_GameComponent::UnregisterPlayer(m_xParentEntity.GetEntityID());
}

void Combat_PlayerComponent::OnUpdate(float fDt)
{
	if (!Combat_GameComponent::IsInPlayingState())
	{
		return;
	}
	Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	if (pxTransform == nullptr || pxCollider == nullptr)
	{
		return;
	}

	Zenith_TransformComponent& xTransform = *pxTransform;
	Zenith_ColliderComponent& xCollider = *pxCollider;

	if (xCollider.HasValidBody())
	{
		g_xEngine.Physics().EnforceUpright(xCollider.GetBodyID());
	}

	if (Combat_DamageSystem::IsDead(m_xParentEntity.GetEntityID()))
	{
		m_xController.TriggerDeath();
	}

	// The player controller state machine lives in the Combat_PlayerState graph
	// now; fire its driving event at exactly the point m_xController.Update ran,
	// so the animation/IK updates below still see the post-tick controller state
	// and per-frame ordering is unchanged. dt rides the payload (custom-event
	// context dt is 0). The graph resolves the transform/collider it needs.
	FirePlayerTick(fDt);
	m_xAnimController.UpdateFromPlayerState(m_xController);

	const bool bCanUseIK = !m_xController.IsDodging() &&
		m_xController.GetState() != Combat_PlayerState::DEAD;
	m_xIKController.UpdateWithAutoTarget(xTransform, m_xParentEntity.GetEntityID(), 0.0f, bCanUseIK, fDt);

	// The attack flow lives in the Combat_PlayerAttack graph; fire its driving
	// event at exactly the point the old UpdateAttack body ran so per-frame
	// ordering against the controller/animation updates above is unchanged.
	FireAttackTick();
}

void Combat_PlayerComponent::TriggerHitStun(float fDuration)
{
	m_xController.TriggerHitStun(fDuration);
}

void Combat_PlayerComponent::FireAttackTick()
{
	Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
	if (pxGraph == nullptr)
	{
		return;
	}
	pxGraph->FireCustomEvent("AttackTick");
}

bool Combat_PlayerComponent::Graph_PreTick(float fDt)
{
	Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	if (pxTransform == nullptr || pxCollider == nullptr)
	{
		return false;
	}
	m_xController.GraphPreTick(*pxTransform, *pxCollider, fDt);
	return true;
}

void Combat_PlayerComponent::FirePlayerTick(float fDt)
{
	Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
	if (pxGraph == nullptr)
	{
		return;
	}
	Zenith_PropertyValue xDt;
	xDt.SetFloat(fDt);
	pxGraph->FireCustomEvent("PlayerTick", &xDt);
}
