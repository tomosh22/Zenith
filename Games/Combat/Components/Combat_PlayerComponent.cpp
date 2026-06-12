#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Combat_PlayerComponent.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
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
	if (m_xParentEntity.HasComponent<Zenith_AnimatorComponent>())
	{
		Zenith_AnimatorComponent& xAnimator = m_xParentEntity.GetComponent<Zenith_AnimatorComponent>();
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
	if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>() ||
		!m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
	{
		return;
	}

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();

	if (xCollider.HasValidBody())
	{
		g_xEngine.Physics().EnforceUpright(xCollider.GetBodyID());
	}

	if (Combat_DamageSystem::IsDead(m_xParentEntity.GetEntityID()))
	{
		m_xController.TriggerDeath();
	}

	m_xController.Update(xTransform, xCollider, fDt);
	m_xAnimController.UpdateFromPlayerState(m_xController);

	const bool bCanUseIK = !m_xController.IsDodging() &&
		m_xController.GetState() != Combat_PlayerState::DEAD;
	m_xIKController.UpdateWithAutoTarget(xTransform, m_xParentEntity.GetEntityID(), 0.0f, bCanUseIK, fDt);

	UpdateAttack(xTransform);
}

void Combat_PlayerComponent::TriggerHitStun(float fDuration)
{
	m_xController.TriggerHitStun(fDuration);
}

void Combat_PlayerComponent::UpdateAttack(Zenith_TransformComponent& xTransform)
{
	if (m_xController.WasAttackJustStarted())
	{
		Combat_AttackType eType = m_xController.GetCurrentAttackType();
		float fDamage = (eType == Combat_AttackType::HEAVY) ? 25.0f : 10.0f;
		float fRange = (eType == Combat_AttackType::HEAVY) ? 2.0f : 1.5f;
		uint32_t uCombo = m_xController.GetComboCount();
		m_xHitDetection.ActivateHitbox(fDamage, fRange, uCombo, uCombo > 1);
	}

	if (m_xController.IsAttacking() && m_xAnimController.IsAttackHitFrame())
	{
		uint32_t uHits = m_xHitDetection.Update(xTransform);
		if (uHits > 0)
		{
			// Push combo state up to the GameManager so the central HUD picks it up
			// without poking back into us.
			Combat_GameComponent::NotifyComboHit(m_xController.GetComboCount(), 2.0f);
		}
	}

	if (!m_xController.IsAttacking())
	{
		m_xHitDetection.DeactivateHitbox();
	}
}
