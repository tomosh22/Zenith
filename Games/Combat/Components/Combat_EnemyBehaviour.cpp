#include "Zenith.h"
#include "Combat_EnemyBehaviour.h"

#include "EntityComponent/Components/Zenith_AnimatorComponent.h"

#include "Combat_Behaviour.h"   // static GameManager state (enemy registry, game state)

void Combat_EnemyBehaviour::OnAwake()
{
	Combat_Behaviour::RegisterEnemy(m_xParentEntity.GetEntityID());
	Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Combat_EnemyBehaviour] OnAwake on entity %u (registered, %zu enemies total)",
		m_xParentEntity.GetEntityID().m_uIndex,
		Combat_Behaviour::GetEnemyEntityIDs().size());
}

void Combat_EnemyBehaviour::OnStart()
{
	EnsureInitialized();
}

void Combat_EnemyBehaviour::OnDestroy()
{
	Combat_Behaviour::UnregisterEnemy(m_xParentEntity.GetEntityID());
}

void Combat_EnemyBehaviour::OnUpdate(float fDt)
{
	if (!Combat_Behaviour::IsInPlayingState())
	{
		return;
	}
	EnsureInitialized();
	m_xAI.Update(fDt);
}

void Combat_EnemyBehaviour::TriggerHitStun()
{
	EnsureInitialized();
	m_xAI.TriggerHitStun();
}

void Combat_EnemyBehaviour::EnsureInitialized()
{
	if (m_bInitialized)
	{
		return;
	}
	Zenith_AnimatorComponent* pxAnimator = nullptr;
	if (m_xParentEntity.HasComponent<Zenith_AnimatorComponent>())
	{
		pxAnimator = &m_xParentEntity.GetComponent<Zenith_AnimatorComponent>();
	}
	m_xAI.Initialize(m_xParentEntity.GetEntityID(), m_xConfig, pxAnimator);
	m_bInitialized = true;
}
