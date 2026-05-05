#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_Entity.h"

// Lightweight bullet projectile script. Counts down a lifetime each frame and
// destroys the entity when it expires, so bullets fired by the player
// controller don't accumulate indefinitely. Also gives a place to hook future
// OnCollision damage handling without touching the player controller.
//
// Attached to the Bullet prefab via Zenith_EditorAutomation::AddStep_AttachScript
// in RenderTest's automation-step registration.
class RenderTest_BulletBehaviour : public Zenith_ScriptBehaviour
{
public:
	RenderTest_BulletBehaviour(Zenith_Entity& xEntity)
		: Zenith_ScriptBehaviour()
	{
		m_xParentEntity = xEntity;
	}

	ZENITH_BEHAVIOUR_TYPE_NAME(RenderTest_BulletBehaviour)

	void OnUpdate(float fDt) override
	{
		m_fLifetime -= fDt;
		if (m_fLifetime <= 0.0f && m_xParentEntity.IsValid())
		{
			m_xParentEntity.Destroy();
		}
	}

private:
	// 2 seconds is enough for an 80 m/s bullet to clear the central platform
	// and the surrounding terrain, and small enough that a 64-slot pool can't
	// run dry under sustained ~500 RPM fire.
	float m_fLifetime = 2.0f;
};
