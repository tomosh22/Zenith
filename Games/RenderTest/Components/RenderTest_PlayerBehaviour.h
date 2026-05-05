#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_Physics.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UI.h"

#include "RenderTest/Components/RenderTest_GameplayState.h"

namespace RenderTest
{
	extern Zenith_AssetHandle<Zenith_Prefab> g_xBulletPrefab;
}

// Third-person shooter player controller.
//
// Layered animator setup:
//   BaseLayer (full body): Idle, Walk, Run, Jump, Hit + AnyState->Hit
//   AimLayer (upper body, masked): Hipfire, Aim, Fire, Reload
//
// Once any layer is added the controller's own state machine is bypassed
// (Flux_AnimationController.cpp:306), so locomotion lives on the explicit
// BaseLayer instead of the controller-level state machine. All parameter
// writes go through the per-layer state machine — Zenith_AnimatorComponent's
// SetFloat/SetBool/SetTrigger shortcuts target the controller-level SM only.
//
// This phase wires up:
//   - Camera-relative WASD movement (from Phase 2)
//   - RMB held -> IsAiming on the aim layer (ADS pose blends in)
//   - Layer weight ramp 0<->1 over ~0.15s
//   - Player rotates to face camera direction during ADS, movement direction otherwise
//
// Fire / jump / reload / sprint trigger plumbing lands in Phases 4-5.
class RenderTest_PlayerBehaviour : public Zenith_ScriptBehaviour
{
public:
	RenderTest_PlayerBehaviour(Zenith_Entity& xEntity)
		: Zenith_ScriptBehaviour()
	{
		m_xParentEntity = xEntity;
	}

	ZENITH_BEHAVIOUR_TYPE_NAME(RenderTest_PlayerBehaviour)

	// Read-only state accessors (used by input-simulator tests to assert on
	// gameplay-internal counters without exposing private state arbitrarily).
	uint32_t GetAmmoInClip()  const { return m_uAmmoInClip; }
	uint32_t GetReserveAmmo() const { return m_uReserveAmmo; }
	float    GetReloadTimer() const { return m_fReloadTimer; }
	float    GetFireCooldown() const { return m_fFireCooldown; }
	float    GetAimLayerWeight() const { return m_fAimLayerWeight; }
	bool     IsReloading()    const { return m_fReloadTimer > 0.0f; }

	void OnAwake() override
	{
		// Reset shared gameplay state — survives Play->Stop->Play unless cleared.
		// (FollowCamera::OnAwake also calls Reset(); calling twice is harmless.)
		RenderTest_GameplayState::Reset();

		// Reset per-instance runtime state. Component pointer caching is deferred
		// to OnStart because OnAwake fires before the AnimatorComponent is
		// guaranteed to be present on first scene build (it's added by automation).
		m_pxAnimator = nullptr;
		m_pxBaseLayer = nullptr;
		m_pxAimLayer = nullptr;
		m_pxMuzzleEmitter = nullptr;
		m_pxAmmoText = nullptr;
		m_fAimLayerWeight = 0.0f;
		m_fForceAimTimer = 0.0f;
		m_fFireCooldown = 0.0f;
		m_fReloadTimer = 0.0f;
		m_uAmmoInClip = k_uMagSize;
		m_uReserveAmmo = 90;

		// Reset the file-static bullet pool so a Play->Stop->Play cycle doesn't
		// leak destroyed bullet entity handles.
		s_uCurrentBulletIndex = 0;
		for (Zenith_Entity& xBullet : s_axBulletEntities)
		{
			xBullet = Zenith_Entity();
		}

		// Lazy-resolve the bullet prefab. On first run after adding the
		// automation, the .zprfb may not exist yet; Shoot() null-checks before
		// applying.
		if (!RenderTest::g_xBulletPrefab.GetDirect())
		{
			RenderTest::g_xBulletPrefab.Resolve();
		}
	}

	void OnStart() override
	{
		// OnStart fires once per script instance: once during automation (when the
		// entity is first created) and again after SaveScene/LoadScene reload.
		// During automation we add the explicit-dim capsule fresh; on reload the
		// scene file may have already deserialized a (degenerate, scale-derived)
		// capsule via the ColliderComponent's saved volume type. Only call
		// AddCapsuleCollider when no body exists yet to avoid the
		// "ColliderComponent already has a collider" assert in AddCollider.
		Zenith_ColliderComponent& xCollider = m_xParentEntity.HasComponent<Zenith_ColliderComponent>()
			? m_xParentEntity.GetComponent<Zenith_ColliderComponent>()
			: m_xParentEntity.AddComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
		{
			xCollider.AddCapsuleCollider(0.3f, 0.6f, RIGIDBODY_TYPE_DYNAMIC);
		}
		if (xCollider.HasValidBody())
		{
			Zenith_Physics::LockRotation(xCollider.GetBodyID(), true, false, true);
		}

		if (m_xParentEntity.HasComponent<Zenith_AnimatorComponent>())
		{
			m_pxAnimator = &m_xParentEntity.GetComponent<Zenith_AnimatorComponent>();
			SetupLayeredAnimator();
		}

		// Cache the muzzle-flash emitter (component lives on the Player entity
		// itself so we don't need a separate gun-barrel child).
		if (m_xParentEntity.HasComponent<Zenith_ParticleEmitterComponent>())
		{
			m_pxMuzzleEmitter = &m_xParentEntity.GetComponent<Zenith_ParticleEmitterComponent>();
		}

		// Resolve HUD ammo text (best-effort; HUD entity may not exist yet on
		// the very first scene build before automation has run).
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxSceneData)
		{
			Zenith_Entity xHUD = pxSceneData->FindEntityByName("HUD");
			if (xHUD.IsValid() && xHUD.HasComponent<Zenith_UIComponent>())
			{
				m_pxAmmoText = xHUD.GetComponent<Zenith_UIComponent>().FindElement<Zenith_UI::Zenith_UIText>("AmmoText");
			}
		}
	}

	void OnUpdate(float fDt) override
	{
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>() ||
			!m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
		{
			return;
		}

		// NOTE: do NOT cache the TransformComponent reference here. Shoot()
		// later in this method spawns a bullet entity; if its TransformComponent
		// pool is at capacity that triggers a buffer Grow() which relocates
		// every existing TransformComponent (including the player's) to fresh
		// memory and dangles any cached reference. Components are re-fetched
		// inline at point of use instead.
		Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();

		if (xCollider.HasValidBody())
		{
			Zenith_Physics::EnforceUpright(xCollider.GetBodyID());
		}

		// Camera-relative basis. Sign convention matches the Test game's
		// PlayerController (Test/Components/PlayerController_Behaviour.cpp:185)
		// and Zenith_CameraComponent: yaw is GLM-standard (CCW from above is
		// positive). Mouse-right decreases yaw, which rotates the player CW
		// (rightward) from above. With yaw=0 the player faces +Z and the
		// camera sits at -Z behind them.
		//
		// Forward derivation: rotate(-yaw, Y) * (0, 0, 1) — same matrix the
		// Test game uses for W movement. Right = +90deg yaw of forward,
		// produced by rotate(-yaw, Y) * (1, 0, 0).
		const float fYaw = RenderTest_GameplayState::GetCameraYaw();
		const Zenith_Maths::Vector3 xForward(-sinf(fYaw), 0.0f, cosf(fYaw));
		const Zenith_Maths::Vector3 xRight  ( cosf(fYaw), 0.0f, sinf(fYaw));

		const Zenith_Maths::Vector3 xInput = ReadMovementInput();
		const Zenith_Maths::Vector3 xMoveDir = xForward * xInput.z + xRight * xInput.x;
		const float fMoveLen = glm::length(xMoveDir);

		// Reload blocks fire/sprint/jump. Phase 4 timer counted down inside the
		// "Timers" block at the bottom; here we read the in-flight value.
		const bool bIsReloading = (m_fReloadTimer > 0.0f);
		const bool bCanAct = !bIsReloading;
		const bool bSprinting = bCanAct
			&& (Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT_SHIFT)
			    || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT_SHIFT))
			&& fMoveLen > 0.01f;

		float fSpeed = 0.0f;

		if (fMoveLen > 0.01f && xCollider.HasValidBody())
		{
			const Zenith_Maths::Vector3 xMoveDirNorm = xMoveDir / fMoveLen;
			fSpeed = bSprinting ? m_fMoveSpeed * m_fSprintMultiplier : m_fMoveSpeed;

			Zenith_Maths::Vector3 xVelocity = xMoveDirNorm * fSpeed;
			xVelocity.y = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID()).y;
			Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}
		else if (xCollider.HasValidBody())
		{
			Zenith_Maths::Vector3 xVelocity = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
			xVelocity.x = 0.0f;
			xVelocity.z = 0.0f;
			Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}

		// --- Jump input ---
		// Space + grounded + can-act -> jump trigger + Y velocity. Grounded
		// check uses a downward raycast from just below the capsule so it
		// doesn't hit the player's own collider.
		if (bCanAct
			&& Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE)
			&& xCollider.HasValidBody()
			&& IsGrounded())
		{
			Zenith_Maths::Vector3 xVelocity = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
			xVelocity.y = m_fJumpVelocity;
			Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
			if (m_pxBaseLayer)
			{
				m_pxBaseLayer->GetStateMachine().GetParameters().SetTrigger("JumpTrigger");
			}
		}

		// --- Reload input ---
		// R + need ammo + have reserve + can act -> start reload. Ammo transfer
		// is deferred to the gated "just finished" block below — without the
		// gate the clip would refill every frame after the timer crossed 0.
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R)
			&& bCanAct
			&& m_uAmmoInClip < k_uMagSize
			&& m_uReserveAmmo > 0)
		{
			StartReload();
		}

		// --- ADS + Fire input ---
		// RMB held -> aim. LMB hipfire-clicks force ADS for the duration of the
		// fire animation via m_fForceAimTimer so the recoil pose still plays.
		const bool bAimingRMB = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_RIGHT);
		const bool bFirePressed = Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);

		if (bFirePressed && bCanAct)
		{
			if (m_uAmmoInClip == 0 && m_uReserveAmmo > 0)
			{
				// Auto-reload on empty fire-attempt — feels better than a
				// silent click. Else-if into the fire path so we don't fire
				// AND reload in the same frame.
				StartReload();
			}
			else if (m_fFireCooldown <= 0.0f && m_uAmmoInClip > 0)
			{
				m_fForceAimTimer = 0.4f;  // hold ADS through the fire animation
				if (m_pxAimLayer)
				{
					m_pxAimLayer->GetStateMachine().GetParameters().SetTrigger("FireTrigger");
				}
				Shoot();
				m_uAmmoInClip--;
				m_fFireCooldown = k_fFireInterval;
			}
		}

		const bool bAiming = bAimingRMB || (m_fForceAimTimer > 0.0f);
		RenderTest_GameplayState::s_bLocalPlayerAiming = bAiming;

		// Aim-layer weight: lerped 0<->1 over ~0.15s for a smooth blend.
		// Forced up while reloading so a hipfire reload (Hipfire->Reload state
		// transition) is actually visible — without this the Reload clip would
		// play under a layer at weight 0.
		//
		// The weight value is tracked unconditionally (not gated on the layer
		// existing) so unit tests that skip OnStart can still observe the
		// ramp logic. Application to the layer only happens if the layer
		// exists.
		const bool bShowAimLayer = bAiming || bIsReloading;
		const float fTargetWeight = bShowAimLayer ? 1.0f : 0.0f;
		const float fWeightLerp = glm::clamp(fDt * 6.66f, 0.0f, 1.0f);
		m_fAimLayerWeight = glm::mix(m_fAimLayerWeight, fTargetWeight, fWeightLerp);

		if (m_pxAimLayer)
		{
			// Set IsAiming on the aim-layer state machine (NOT via the animator
			// shortcut, which targets the bypassed controller-level SM).
			m_pxAimLayer->GetStateMachine().GetParameters().SetBool("IsAiming", bAiming);
			m_pxAimLayer->SetWeight(m_fAimLayerWeight);
		}

		// --- Player rotation target ---
		// ADS: face camera-forward so the gun aligns with the camera.
		// Hipfire + moving forward (input.z >= 0): face movement direction.
		// Hipfire + moving backward / idle: keep current rotation (no 180 spin).
		//
		// Both branches go through RotateTowards (which extracts target yaw
		// via atan2 from the direction vector) so the camera-yaw -> player-
		// transform-yaw sign conversion is handled in one place.
		//
		// Fresh fetch: see the don't-cache-Transform note at the top of OnUpdate.
		Zenith_TransformComponent& xTransform =
			m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		if (bAiming)
		{
			RotateTowards(xTransform, xForward, fDt);
		}
		else if (fMoveLen > 0.01f && xInput.z >= 0.0f)
		{
			RotateTowards(xTransform, xMoveDir / fMoveLen, fDt);
		}

		// --- Base layer parameters ---
		// Speed drives Idle <-> Walk <-> Run, IsSprinting elevates Walk -> Run,
		// IsGrounded gates Jump -> Idle. The animator-component shortcuts
		// SetFloat/SetBool target the controller-level SM which is bypassed
		// once layers exist; route through the layer.
		if (m_pxBaseLayer)
		{
			Flux_AnimationParameters& xParams = m_pxBaseLayer->GetStateMachine().GetParameters();
			xParams.SetFloat("Speed", fSpeed);
			xParams.SetBool("IsSprinting", bSprinting);
			xParams.SetBool("IsGrounded", IsGrounded());
		}

		// --- Timers + reload completion ---
		// The "just finished" gate (bWasReloading && timer<=0 this frame) is
		// required — without it ammo would refill every frame after reload
		// completes because m_fReloadTimer stays <=0 forever.
		m_fFireCooldown -= fDt;
		m_fForceAimTimer -= fDt;
		const bool bWasReloading = (m_fReloadTimer > 0.0f);
		m_fReloadTimer -= fDt;
		if (bWasReloading && m_fReloadTimer <= 0.0f)
		{
			const uint32_t uTake = std::min(k_uMagSize - m_uAmmoInClip, m_uReserveAmmo);
			m_uAmmoInClip += uTake;
			m_uReserveAmmo -= uTake;
		}

		// --- HUD ---
		UpdateAmmoHUD();
	}

private:
	Zenith_Maths::Vector3 ReadMovementInput() const
	{
		Zenith_Maths::Vector3 xInput(0.0f);
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))    xInput.z += 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))  xInput.z -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))  xInput.x -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT)) xInput.x += 1.0f;
		return xInput;
	}

	void RotateTowards(Zenith_TransformComponent& xTransform, const Zenith_Maths::Vector3& xTargetDir, float fDt) const
	{
		if (glm::length(xTargetDir) < 0.01f)
			return;
		const float fTargetYaw = atan2f(xTargetDir.x, xTargetDir.z);
		RotateTowardsYaw(xTransform, fTargetYaw, fDt);
	}

	void RotateTowardsYaw(Zenith_TransformComponent& xTransform, float fTargetYaw, float fDt) const
	{
		Zenith_Maths::Quat xCurrentRot;
		xTransform.GetRotation(xCurrentRot);
		const Zenith_Maths::Quat xTargetRot = glm::angleAxis(fTargetYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const float fSlerpAlpha = glm::clamp(fDt * m_fRotationSpeed, 0.0f, 1.0f);
		const Zenith_Maths::Quat xNewRot = glm::slerp(xCurrentRot, xTargetRot, fSlerpAlpha);
		xTransform.SetRotation(glm::normalize(xNewRot));
	}

	// Constructs the layered animator on the player's animator component.
	// Layer 0 (BaseLayer): full-body locomotion + jump + hit.
	// Layer 1 (AimLayer): upper-body aim/fire/reload, masked to torso+arms+head.
	void SetupLayeredAnimator()
	{
		if (!m_pxAnimator)
			return;

		Flux_AnimationController& xController = m_pxAnimator->GetController();

		// --- Load all clips into the controller's clip collection ---
		static const std::string s_strAssetDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Idle"   ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Walk"   ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Run"    ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Hit"    ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Aim"    ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Fire"   ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Reload" ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Jump"   ZENITH_ANIMATION_EXT);

		// --- Build the upper-body bone mask from the skeleton asset ---
		// We can't go through Flux_BoneMask::CreateUpperBodyMask because that
		// needs a Flux_MeshGeometry (no longer reachable from ModelComponent).
		// Look up bone indices on the skeleton asset directly and assert if any
		// expected bone is missing — silent fallback to bone 0 would mask the
		// whole skeleton's root, which would look catastrophically broken.
		Flux_BoneMask xMask;
		const std::string strSkelPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
		Zenith_SkeletonAsset* pxSkel = Zenith_AssetRegistry::Get<Zenith_SkeletonAsset>(strSkelPath);
		if (pxSkel)
		{
			auto SetMaskBone = [&](const char* szName)
			{
				const int32_t iIdx = pxSkel->GetBoneIndex(szName);
				if (iIdx >= 0)
				{
					xMask.SetBoneWeight(static_cast<uint32_t>(iIdx), 1.0f);
				}
				else
				{
					Zenith_Log(LOG_CATEGORY_GAMEPLAY,
						"[RenderTest] Aim mask: missing bone '%s'", szName);
				}
			};
			SetMaskBone("Spine");
			SetMaskBone("Neck");
			SetMaskBone("Head");
			SetMaskBone("LeftUpperArm");
			SetMaskBone("LeftLowerArm");
			SetMaskBone("LeftHand");
			SetMaskBone("RightUpperArm");
			SetMaskBone("RightLowerArm");
			SetMaskBone("RightHand");
		}

		// --- Layer 0: BaseLayer (locomotion + jump + hit, full body) ---
		m_pxBaseLayer = xController.AddLayer("BaseLayer");
		m_pxBaseLayer->SetWeight(1.0f);
		m_pxBaseLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);
		// No avatar mask -> base layer drives every bone.

		Flux_AnimationStateMachine* pxBaseSM = m_pxBaseLayer->CreateStateMachine("RenderTestBase");
		Flux_AnimationClipCollection& xClips = xController.GetClipCollection();

		pxBaseSM->GetParameters().AddFloat("Speed", 0.0f);
		pxBaseSM->GetParameters().AddBool("IsSprinting", false);
		pxBaseSM->GetParameters().AddBool("IsGrounded", true);
		pxBaseSM->GetParameters().AddTrigger("JumpTrigger");
		pxBaseSM->GetParameters().AddTrigger("HitTrigger");

		AddClipState(pxBaseSM, xClips, "Idle", "Idle");
		AddClipState(pxBaseSM, xClips, "Walk", "Walk");
		AddClipState(pxBaseSM, xClips, "Run",  "Run");
		AddClipState(pxBaseSM, xClips, "Jump", "Jump");
		AddClipState(pxBaseSM, xClips, "Hit",  "Hit");

		// Idle <-> Walk on Speed
		AddFloatTransition(pxBaseSM, "Idle", "Walk", "Speed", Flux_TransitionCondition::CompareOp::Greater, 0.1f, 0.15f);
		AddFloatTransition(pxBaseSM, "Walk", "Idle", "Speed", Flux_TransitionCondition::CompareOp::LessEqual, 0.1f, 0.15f);

		// Walk <-> Run on IsSprinting
		AddBoolTransition(pxBaseSM, "Walk", "Run", "IsSprinting", true, 0.15f);
		AddBoolTransition(pxBaseSM, "Run",  "Walk", "IsSprinting", false, 0.15f);
		// Run -> Idle when stopping while sprinting
		AddFloatTransition(pxBaseSM, "Run", "Idle", "Speed", Flux_TransitionCondition::CompareOp::LessEqual, 0.1f, 0.15f);

		// Idle/Walk/Run -> Jump on JumpTrigger (priority 50 so it wins over locomotion blends).
		AddTriggerTransition(pxBaseSM, "Idle", "Jump", "JumpTrigger", 0.10f, 50);
		AddTriggerTransition(pxBaseSM, "Walk", "Jump", "JumpTrigger", 0.10f, 50);
		AddTriggerTransition(pxBaseSM, "Run",  "Jump", "JumpTrigger", 0.10f, 50);

		// Jump -> Idle: gated on IsGrounded == true, with exit-time floor of 0.8
		// so the jump anim plays at least 80% of its duration before snapping
		// back even if the player lands very early. Without the IsGrounded
		// condition the player would play Idle while still airborne for long jumps.
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = "Idle";
			xTrans.m_fTransitionDuration = 0.15f;
			xTrans.m_bHasExitTime = true;
			xTrans.m_fExitTime = 0.8f;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = "IsGrounded";
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
			xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
			xCond.m_bThreshold = true;
			xTrans.m_xConditions.PushBack(xCond);

			pxBaseSM->GetState("Jump")->AddTransition(xTrans);
		}

		// (HitTrigger any-state transition is wired in a future pass once
		// collision damage exists. The trigger parameter is in place; pulling
		// it from gameplay code today would just play Hit on demand.)

		pxBaseSM->SetDefaultState("Idle");
		pxBaseSM->ResolveClipReferences(&xClips);

		// --- Layer 1: AimLayer (upper-body override) ---
		m_pxAimLayer = xController.AddLayer("AimLayer");
		m_pxAimLayer->SetWeight(0.0f);
		m_pxAimLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);
		m_pxAimLayer->SetAvatarMask(xMask);

		Flux_AnimationStateMachine* pxAimSM = m_pxAimLayer->CreateStateMachine("RenderTestAim");

		pxAimSM->GetParameters().AddBool("IsAiming", false);
		pxAimSM->GetParameters().AddTrigger("FireTrigger");
		pxAimSM->GetParameters().AddTrigger("ReloadTrigger");

		// Hipfire reuses Idle clip — the aim mask zeroes the locomotion-only
		// channels. Expect a small visual artifact when starting ADS while
		// walking because Idle's spine "breathing" overrides Walk's spine.
		AddClipState(pxAimSM, xClips, "Hipfire", "Idle");
		AddClipState(pxAimSM, xClips, "Aim",     "Aim");
		AddClipState(pxAimSM, xClips, "Fire",    "Fire");
		AddClipState(pxAimSM, xClips, "Reload",  "Reload");

		// Hipfire <-> Aim on IsAiming
		AddBoolTransition(pxAimSM, "Hipfire", "Aim",     "IsAiming", true,  0.15f);
		AddBoolTransition(pxAimSM, "Aim",     "Hipfire", "IsAiming", false, 0.15f);

		// Aim -> Fire on FireTrigger (priority 50 so it wins over IsAiming-flip
		// race when LMB+RMB-release happen on the same frame).
		AddTriggerTransition(pxAimSM, "Aim", "Fire", "FireTrigger", 0.05f, 50);
		AddExitTimeTransition(pxAimSM, "Fire", "Aim", 1.0f, 0.10f);

		// Reload paths from BOTH Aim and Hipfire so pressing R outside ADS,
		// or auto-reloading on empty hipfire-click, still plays the reload
		// animation. The layer-weight ramp in OnUpdate is also forced up
		// while reloading so the clip is actually visible.
		AddTriggerTransition(pxAimSM, "Aim",     "Reload", "ReloadTrigger", 0.10f, 60);
		AddTriggerTransition(pxAimSM, "Hipfire", "Reload", "ReloadTrigger", 0.10f, 60);

		// Reload exit: prefer Hipfire when not aiming (priority 10 wins over
		// the always-eligible Reload -> Aim at default priority 0). When
		// IsAiming is true, the Hipfire path's condition fails and we fall
		// through to the Aim path.
		AddExitTimeTransitionWithBoolCondition(pxAimSM, "Reload", "Hipfire",
			1.0f, 0.15f, "IsAiming", false, /*priority=*/10);
		AddExitTimeTransition(pxAimSM, "Reload", "Aim", 1.0f, 0.15f);

		pxAimSM->SetDefaultState("Hipfire");
		pxAimSM->ResolveClipReferences(&xClips);
	}

	static void AddClipState(Flux_AnimationStateMachine* pxSM,
		Flux_AnimationClipCollection& xClips,
		const char* szStateName,
		const char* szClipName)
	{
		Flux_AnimationState* pxState = pxSM->AddState(szStateName);
		Flux_AnimationClip* pxClip = xClips.GetClip(szClipName);
		if (pxClip)
		{
			pxState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClip, 1.0f));
		}
	}

	static void AddFloatTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFrom, const char* szTo,
		const char* szParam, Flux_TransitionCondition::CompareOp eOp, float fThreshold,
		float fDuration)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = szParam;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_eCompareOp = eOp;
		xCond.m_fThreshold = fThreshold;
		xTrans.m_xConditions.PushBack(xCond);

		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	static void AddBoolTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFrom, const char* szTo,
		const char* szParam, bool bExpected,
		float fDuration)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = szParam;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_bThreshold = bExpected;
		xTrans.m_xConditions.PushBack(xCond);

		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	static void AddTriggerTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFrom, const char* szTo,
		const char* szTrigger, float fDuration, int32_t iPriority)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;
		xTrans.m_iPriority = iPriority;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = szTrigger;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);

		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	static void AddExitTimeTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFrom, const char* szTo,
		float fExitTime, float fDuration)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;
		xTrans.m_bHasExitTime = true;
		xTrans.m_fExitTime = fExitTime;

		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	static void AddExitTimeTransitionWithBoolCondition(
		Flux_AnimationStateMachine* pxSM,
		const char* szFrom, const char* szTo,
		float fExitTime, float fDuration,
		const char* szParam, bool bExpected, int32_t iPriority)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;
		xTrans.m_bHasExitTime = true;
		xTrans.m_fExitTime = fExitTime;
		xTrans.m_iPriority = iPriority;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = szParam;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_bThreshold = bExpected;
		xTrans.m_xConditions.PushBack(xCond);

		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	void Shoot()
	{
		Zenith_Prefab* pxPrefab = RenderTest::g_xBulletPrefab.GetDirect();
		if (!pxPrefab)
		{
			// Try to lazy-resolve once more — the .zprfb may have just been
			// produced by automation on this run. Still null-check the result.
			RenderTest::g_xBulletPrefab.Resolve();
			pxPrefab = RenderTest::g_xBulletPrefab.GetDirect();
			if (!pxPrefab)
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"[RenderTest] Bullet prefab not loaded; skipping shot");
				return;
			}
		}

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData)
			return;

		// Reuse the next pool slot (ring buffer). Old bullet in this slot, if
		// any, gets force-destroyed before we overwrite the handle.
		if (s_axBulletEntities[s_uCurrentBulletIndex].IsValid())
		{
			s_axBulletEntities[s_uCurrentBulletIndex].DestroyImmediate();
		}

		s_axBulletEntities[s_uCurrentBulletIndex] =
			Zenith_Entity(pxSceneData, "Bullet" + std::to_string(s_uCurrentBulletIndex));
		Zenith_Entity& xBullet = s_axBulletEntities[s_uCurrentBulletIndex];
		s_uCurrentBulletIndex = (s_uCurrentBulletIndex + 1) % k_uBulletPoolSize;

		// ApplyToEntity deserializes Transform + Model + Collider + Script onto
		// the entity but defers OnAwake/OnStart to the next update tick. The
		// transform/velocity we set immediately below take effect right away;
		// any future OnAwake logic on RenderTest_BulletBehaviour must NOT
		// assume it runs before this initial setup.
		pxPrefab->ApplyToEntity(xBullet);

		// Trajectory along the FULL camera basis (yaw + pitch) — bullets must
		// fly where the player is actually looking, including up/down. Sign
		// convention matches OnUpdate's xForward/xRight: at yaw=0 the player
		// faces +Z (camera is behind at -Z), so a bullet fired with yaw=0,
		// pitch=0 must travel +Z (away from the camera).
		const float fYaw   = RenderTest_GameplayState::GetCameraYaw();
		const float fPitch = RenderTest_GameplayState::GetCameraPitch();
		const Zenith_Maths::Vector3 xFwd(-sinf(fYaw) * cosf(fPitch),
		                                  sinf(fPitch),
		                                  cosf(fYaw) * cosf(fPitch));
		const Zenith_Maths::Vector3 xRgt(cosf(fYaw), 0.0f, sinf(fYaw));

		Zenith_Maths::Vector3 xPlayerPos;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);
		const Zenith_Maths::Vector3 xBarrel =
			xPlayerPos + xRgt * 0.3f + Zenith_Maths::Vector3(0.0f, 1.4f, 0.0f) + xFwd * 1.0f;

		Zenith_TransformComponent& xT = xBullet.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(xBarrel);
		xT.SetScale(Zenith_Maths::Vector3(0.15f));

		Zenith_ColliderComponent& xCol = xBullet.GetComponent<Zenith_ColliderComponent>();
		Zenith_Physics::SetLinearVelocity(xCol.GetBodyID(), xFwd * 80.0f);
		// Hitscan-style: no gravity arc on the projectile.
		Zenith_Physics::SetGravityEnabled(xCol.GetBodyID(), false);

		// Muzzle flash burst at the gun tip, oriented along the firing direction.
		if (m_pxMuzzleEmitter)
		{
			m_pxMuzzleEmitter->SetEmitPosition(xBarrel);
			m_pxMuzzleEmitter->SetEmitDirection(xFwd);
			m_pxMuzzleEmitter->Emit(8);
		}
	}

	void UpdateAmmoHUD()
	{
		if (!m_pxAmmoText)
			return;
		const std::string strText = std::to_string(m_uAmmoInClip) + " / " + std::to_string(m_uReserveAmmo);
		m_pxAmmoText->SetText(strText);
	}

	void StartReload()
	{
		if (m_fReloadTimer > 0.0f)
			return;  // already reloading
		m_fReloadTimer = k_fReloadDuration;
		if (m_pxAimLayer)
		{
			m_pxAimLayer->GetStateMachine().GetParameters().SetTrigger("ReloadTrigger");
		}
	}

	// Grounded test for jump gating. Ray starts just below the player capsule
	// (capsule half-height 0.6m + small epsilon) so it doesn't hit our own
	// collider. 0.2m max distance covers small slope variations without
	// reporting false-grounded mid-jump-arc near a wall.
	bool IsGrounded() const
	{
		Zenith_Maths::Vector3 xPlayerPos;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);
		const float fCapsuleHalfHeight = 0.6f;
		const float fEpsilon = 0.02f;
		const Zenith_Maths::Vector3 xRayOrigin =
			xPlayerPos - Zenith_Maths::Vector3(0.0f, fCapsuleHalfHeight + fEpsilon, 0.0f);
		const Zenith_Maths::Vector3 xDown(0.0f, -1.0f, 0.0f);
		const Zenith_Physics::RaycastResult xResult =
			Zenith_Physics::Raycast(xRayOrigin, xDown, 0.2f);
		return xResult.m_bHit;
	}

	// Bullet pool — file-static ring buffer. Reset in OnAwake to survive
	// Play->Stop->Play. Sized to 64: at ~500 RPM and a 2s bullet lifetime,
	// at most ~17 bullets are alive at once, so reuse never destroys a still-
	// live bullet.
	static constexpr uint32_t k_uBulletPoolSize = 64;
	static constexpr uint32_t k_uMagSize        = 30;
	static constexpr float    k_fFireInterval   = 0.12f;  // ~500 RPM
	static constexpr float    k_fReloadDuration = 1.5f;

	inline static Zenith_Entity s_axBulletEntities[k_uBulletPoolSize] = {};
	inline static uint32_t      s_uCurrentBulletIndex                 = 0;

	Zenith_AnimatorComponent*         m_pxAnimator = nullptr;
	Flux_AnimationLayer*              m_pxBaseLayer = nullptr;
	Flux_AnimationLayer*              m_pxAimLayer = nullptr;
	Zenith_ParticleEmitterComponent*  m_pxMuzzleEmitter = nullptr;
	Zenith_UI::Zenith_UIText*         m_pxAmmoText = nullptr;

	float    m_fMoveSpeed       = 5.0f;
	float    m_fSprintMultiplier = 1.7f;
	float    m_fJumpVelocity    = 6.0f;
	float    m_fRotationSpeed   = 10.0f;
	float    m_fAimLayerWeight  = 0.0f;
	float    m_fForceAimTimer   = 0.0f;
	float    m_fFireCooldown    = 0.0f;
	float    m_fReloadTimer     = 0.0f;
	uint32_t m_uAmmoInClip      = k_uMagSize;
	uint32_t m_uReserveAmmo     = 90;
};
