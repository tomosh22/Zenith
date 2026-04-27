#pragma once
/**
 * Combat_PlayerBehaviour.h - Per-entity script attached to the player.
 *
 * Unity-style: this script lives on the player's Zenith_ScriptComponent rather than on
 * the central GameManager. It owns the player's controller, animation, IK, and hit
 * detection state. The central Combat_Behaviour (GameManager) only orchestrates game
 * state and routes damage events; the player ticks itself.
 *
 * Lifecycle:
 *   - OnAwake:  register self with Combat_Behaviour::RegisterPlayer; set hit-detection owner
 *   - OnStart:  initialize the animation controller from the entity's AnimatorComponent
 *               (deferred to OnStart because the animator's skeleton is auto-discovered
 *               from the ModelComponent at OnStart time)
 *   - OnUpdate: per-frame movement / animation / IK / attack work, gated on the
 *               GameManager being in the PLAYING state
 *   - OnDestroy: unregister
 *
 * Implementation lives in Combat_PlayerBehaviour.cpp so the bodies can reference
 * Combat_Behaviour's static state directly without the cyclic include that would
 * otherwise arise (Combat_Behaviour.h already includes this header to attach the
 * script in CreateArena).
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include "Combat_PlayerController.h"
#include "Combat_AnimationController.h"
#include "Combat_IKController.h"
#include "Combat_HitDetection.h"

class Zenith_TransformComponent;

class Combat_PlayerBehaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Combat_PlayerBehaviour)

	Combat_PlayerBehaviour() = delete;
	Combat_PlayerBehaviour(Zenith_Entity&) {}
	~Combat_PlayerBehaviour() = default;

	void OnAwake() ZENITH_FINAL override;
	void OnStart() ZENITH_FINAL override;
	void OnDestroy() ZENITH_FINAL override;
	void OnUpdate(float fDt) ZENITH_FINAL override;

	// Called by Combat_Behaviour::OnDamageEvent when the damage target is the player.
	void TriggerHitStun(float fDuration = 0.3f);

	uint32_t GetComboCount() const { return m_xController.GetComboCount(); }
	const Combat_PlayerController& GetController() const { return m_xController; }
	const Combat_AnimationController& GetAnimController() const { return m_xAnimController; }

private:
	void UpdateAttack(Zenith_TransformComponent& xTransform);

	Combat_PlayerController    m_xController;
	Combat_AnimationController m_xAnimController;
	Combat_IKController        m_xIKController;
	Combat_HitDetection        m_xHitDetection;
};
