#pragma once
/**
 * DummyNoiseMachine_Component - gym-map prop. On interact, emits a sound
 * stimulus via DP_AI::EmitNoise so the priest's perception (and its BT)
 * picks up the noise and investigates.
 */

#include "Components/DPInteractable_Base.h"
#include "Source/DP_Tuning.h"

class DummyNoiseMachine_Component ZENITH_FINAL : public DPInteractable_Base
{
public:
	DummyNoiseMachine_Component() = delete;
	DummyNoiseMachine_Component(Zenith_Entity& xParentEntity)
		: DPInteractable_Base(xParentEntity)
	{}

	void OnAwake()
	{
		DPInteractable_Base::OnAwake();
		// MVP-0.1.4: tuning reads.
		m_fLoudness = DP_Tuning::Get<float>("interactables.noise_machine_loudness");
		m_fRadius   = DP_Tuning::Get<float>("interactables.noise_machine_radius_m");
	}

protected:
	void HandleInteract(Zenith_EntityID /*xVillager*/) override
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		}
		DP_AI::EmitNoise(xPos, m_fLoudness, m_fRadius, m_xParentEntity.GetEntityID());
	}

private:
	float m_fLoudness = 1.0f;  // Fallback; OnAwake reads DP_Tuning.
	float m_fRadius   = 20.0f; // Fallback; OnAwake reads DP_Tuning.

#ifdef ZENITH_INPUT_SIMULATOR
public:
	float GetLoudness() const { return m_fLoudness; }
	float GetRadius() const   { return m_fRadius; }
#endif
};
