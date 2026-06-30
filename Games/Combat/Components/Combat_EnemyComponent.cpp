#include "Zenith.h"
#include "Combat_EnemyComponent.h"

#include "EntityComponent/Components/Zenith_AnimatorComponent.h"

#include "Combat_GameComponent.h"   // static GameManager state (enemy registry, game state)

void Combat_EnemyComponent::OnAwake()
{
	Combat_GameComponent::RegisterEnemy(m_xParentEntity.GetEntityID());
	Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Combat_EnemyComponent] OnAwake on entity %u (registered, %zu enemies total)",
		m_xParentEntity.GetEntityID().m_uIndex,
		Combat_GameComponent::GetEnemyEntityIDs().size());
}

void Combat_EnemyComponent::OnStart()
{
	EnsureInitialized();
}

void Combat_EnemyComponent::OnDestroy()
{
	Combat_GameComponent::UnregisterEnemy(m_xParentEntity.GetEntityID());
}

void Combat_EnemyComponent::OnUpdate(float fDt)
{
	if (!Combat_GameComponent::IsInPlayingState())
	{
		return;
	}
	EnsureInitialized();
	m_xAI.Update(fDt);
}

void Combat_EnemyComponent::TriggerHitStun()
{
	EnsureInitialized();
	m_xAI.TriggerHitStun();
}

void Combat_EnemyComponent::EnsureInitialized()
{
	if (m_bInitialized)
	{
		return;
	}
	Zenith_AnimatorComponent* pxAnimator = m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>();
	m_xAI.Initialize(m_xParentEntity.GetEntityID(), m_xConfig, pxAnimator);
	m_bInitialized = true;
}
