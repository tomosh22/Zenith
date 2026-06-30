#pragma once
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Physics/Zenith_Physics.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
// Wave-19: Zenith_AnimatorComponent.h is now a Flux-include-free forwarding
// handle, so it no longer drags in Flux_AnimationController.h. This TU uses the
// complete Flux_AnimationController type (GetController().AddLayer/GetIKSolver/...)
// — include the header directly.
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UI.h"
#include "Flux/Decals/Flux_DecalsImpl.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "RenderTest/RenderTest_Guns.h"
#include "RenderTest/Components/RenderTest_GunComponent.h"
#include "RenderTest/RenderTest_Jetpack.h"
#include "RenderTest/Components/RenderTest_JetpackComponent.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

#include "RenderTest/Components/RenderTest_GameplayState.h"

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
class RenderTest_PlayerComponent
{
public:
	RenderTest_PlayerComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	// Read-only state accessors (used by input-simulator tests to assert on
	// gameplay-internal counters without exposing private state arbitrarily).
	uint32_t GetAmmoInClip()  const { return m_uAmmoInClip; }
	uint32_t GetReserveAmmo() const { return m_uReserveAmmo; }
	float    GetReloadTimer() const { return m_fReloadTimer; }
	float    GetFireCooldown() const { return m_fFireCooldown; }
	float    GetAimLayerWeight() const { return m_fAimLayerWeight; }
	bool     IsReloading()    const { return m_fReloadTimer > 0.0f; }

	// Pure jetpack thrust integrator: accelerate the vertical velocity upward by
	// one frame's thrust, capped at the ascent ceiling. Gravity is applied
	// separately by the physics step, so the net climb is (thrust accel -
	// gravity). Static + side-effect-free so it can be unit-tested without a
	// physics body / entity; the in-engine call site is the jetpack-thrust block
	// in OnUpdate.
	static float ApplyJetpackThrust(float fVyIn, float fDt)
	{
		return glm::min(fVyIn + k_fJetpackThrustAccel * fDt, k_fJetpackMaxAscent);
	}

	// Pure thrust-gating predicate (extracted so the "no jetpack => no thrust =>
	// ground movement unchanged" guarantee is unit-testable without a physics body):
	// the jetpack fires only when one is worn AND (Space is held OR the showcase
	// forces it). This is the exact condition the OnUpdate thrust block uses.
	static bool ShouldEngageJetpack(bool bEquipped, bool bSpaceHeld, bool bShowcaseForced)
	{
		return bEquipped && (bSpaceHeld || bShowcaseForced);
	}

	// Design constants exposed for the invariant test (the ascent cap must stay
	// above the jump pop so a jetpack-equipped jump isn't clamped flat).
	static constexpr float GetJetpackMaxAscent() { return k_fJetpackMaxAscent; }
	static constexpr float GetJumpVelocity()     { return k_fJumpVelocity; }

	// Single-player game; the smoke runner uses this to drive a test shot
	// from outside the input system. nullptr until OnAwake fires.
	static RenderTest_PlayerComponent* GetActiveInstance() { return s_pxActiveInstance; }

	// Smoke-runner hook: aim straight down and fire one shot, ignoring fire
	// cooldown / ammo / reload state. Restores the camera pitch on exit so
	// the player's normal aiming isn't disturbed.
	void TriggerShootDownwardForTest()
	{
		const float fSavedPitch = RenderTest_GameplayState::s_fCameraPitch;
		// FollowCamera clamps pitch to [-1.2, 0.6]; -1.2 rad ≈ 69° below
		// horizontal — close to straight-down but within the clamp range.
		RenderTest_GameplayState::s_fCameraPitch = -1.2f;
		Shoot();
		RenderTest_GameplayState::s_fCameraPitch = fSavedPitch;
	}

	void OnAwake()
	{
		s_pxActiveInstance = this;

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

		// Gun pickup/drop state. Empty-handed at spawn; the guns are on the floor.
		m_xHeldGun = Zenith_Entity();
		m_xNearestGun = Zenith_Entity();
		m_fNearestGunDist = 1e30f;
		m_bGunAutoEquipped = false;
		m_fGunIKWeight = 0.0f;
		m_fGunRecoilKick = 0.0f;
		m_pxGunPromptText = nullptr;
		// Empty-handed weapon params == the historical baseline so the existing
		// (gunless) input-simulator fire/reload tests are unaffected.
		m_uCurrentMagSize = k_uMagSize;
		m_fCurrentFireInterval = k_fFireInterval;

		// Jetpack state. Resolved lazily once the procedural jetpack has spawned;
		// stays unequipped (a no-op) in the input-sim tests / showcases that have
		// no jetpack entity.
		m_xJetpack = Zenith_Entity();
		m_uJetNozzleAlt = 0;
	}

	void OnStart()
	{
		// OnStart fires once per component instance: once during automation (when the
		// entity is first created) and again after SaveScene/LoadScene reload.
		// During automation we add the explicit-dim capsule fresh; on reload the
		// scene file may have already deserialized a (degenerate, scale-derived)
		// capsule via the ColliderComponent's saved volume type. Only call
		// AddCapsuleCollider when no body exists yet to avoid the
		// "ColliderComponent already has a collider" assert in AddCollider.
		Zenith_ColliderComponent* pxExistingCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		Zenith_ColliderComponent& xCollider = pxExistingCollider != nullptr
			? *pxExistingCollider
			: m_xParentEntity.AddComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
		{
			// Capsule sized so:
			//  - Total half-extent = 1.05m (halfCyl + radius), placing the model's
			//    foot bind position (-1.0m below player) at the IK ankle target Y
			//    when the capsule rests on the ground. No leg fold for stationary
			//    standing feet.
			//  - Radius = 0.10m, narrow enough that the foot bones (offset 0.15m
			//    from player center) are OUTSIDE the capsule. This lets a tall
			//    step cube sit directly under one foot WITHOUT the cube
			//    penetrating the capsule and pushing the player up off the main
			//    platform — the asymmetric-foot demo wouldn't be visible
			//    otherwise (capsule would just rest on top of the step).
			// Trade-off: the player's collision is narrower than the visible
			// stick-figure body. Acceptable for the IK demo; widen if other
			// gameplay needs broader collision.
			xCollider.AddCapsuleCollider(0.10f, 0.95f, RIGIDBODY_TYPE_DYNAMIC);
		}
		if (xCollider.HasValidBody())
		{
			g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, false, true);
		}

		if (Zenith_AnimatorComponent* pxAnimator = m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>())
		{
			m_pxAnimator = pxAnimator;
			SetupLayeredAnimator();
		}

		// Cache the muzzle-flash emitter (component lives on the Player entity
		// itself so we don't need a separate gun-barrel child).
		if (Zenith_ParticleEmitterComponent* pxEmitter = m_xParentEntity.TryGetComponent<Zenith_ParticleEmitterComponent>())
		{
			m_pxMuzzleEmitter = pxEmitter;
		}

		// Resolve HUD ammo text (best-effort; HUD entity may not exist yet on
		// the very first scene build before automation has run).
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxSceneData)
		{
			Zenith_Entity xHUD = pxSceneData->FindEntityByName("HUD");
			if (Zenith_UIComponent* pxUI = xHUD.TryGetComponent<Zenith_UIComponent>())
			{
				m_pxAmmoText = pxUI->FindElement<Zenith_UI::Zenith_UIText>("AmmoText");
				m_pxGunPromptText = pxUI->FindElement<Zenith_UI::Zenith_UIText>("GunPrompt");
			}
		}
	}

	void OnUpdate(float fDt)
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
			g_xEngine.Physics().EnforceUpright(xCollider.GetBodyID());
		}

		// Gun pickup/drop (E), proximity prompt, and showcase auto-equip. All gun
		// behaviour is gated on actually holding/seeing a gun, so the gunless
		// input-simulator tests (which never spawn a gun) are unaffected.
		UpdateGunHandling(fDt);

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
			&& (g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_SHIFT)
			    || g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_SHIFT))
			&& fMoveLen > 0.01f;

		float fSpeed = 0.0f;

		if (fMoveLen > 0.01f && xCollider.HasValidBody())
		{
			const Zenith_Maths::Vector3 xMoveDirNorm = xMoveDir / fMoveLen;
			fSpeed = bSprinting ? m_fMoveSpeed * m_fSprintMultiplier : m_fMoveSpeed;

			Zenith_Maths::Vector3 xVelocity = xMoveDirNorm * fSpeed;
			xVelocity.y = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID()).y;
			g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}
		else if (xCollider.HasValidBody())
		{
			Zenith_Maths::Vector3 xVelocity = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
			xVelocity.x = 0.0f;
			xVelocity.z = 0.0f;
			g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}

		// --- Jump input ---
		// Space + grounded + can-act -> jump trigger + Y velocity. Grounded
		// check uses a downward raycast from just below the capsule so it
		// doesn't hit the player's own collider.
		if (bCanAct
			&& g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_SPACE)
			&& xCollider.HasValidBody()
			&& IsGrounded())
		{
			Zenith_Maths::Vector3 xVelocity = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
			xVelocity.y = m_fJumpVelocity;
			g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
			if (m_pxBaseLayer)
			{
				m_pxBaseLayer->GetStateMachine().GetParameters().SetTrigger("JumpTrigger");
			}
		}

		// --- Jetpack thrust ---
		// Holding Space fires the jetpack (when one is worn), accelerating the
		// player upward up to a capped ascent speed. Mid-air horizontal control
		// comes for free: the camera-relative move block above is NOT grounded-
		// gated, so the arrow keys steer while airborne. Ground movement and the
		// jump above are untouched — without a jetpack equipped this is a no-op,
		// so the gunless/jetpack-less input-simulator tests are unaffected. The
		// showcase capture forces thrust so the rising player + trail can be shot.
		ResolveJetpack();
		const bool bJetpackThrust = ShouldEngageJetpack(
			IsJetpackEquipped(),
			g_xEngine.Input().IsKeyDown(ZENITH_KEY_SPACE),
			RenderTest_JetpackTuning::s_bShowcaseActive);
		if (bJetpackThrust && xCollider.HasValidBody())
		{
			Zenith_Maths::Vector3 xVelocity = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
			xVelocity.y = ApplyJetpackThrust(xVelocity.y, fDt);
			g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}

		// --- Reload input ---
		// R + need ammo + have reserve + can act -> start reload. Ammo transfer
		// is deferred to the gated "just finished" block below — without the
		// gate the clip would refill every frame after the timer crossed 0.
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_R)
			&& bCanAct
			&& m_uAmmoInClip < m_uCurrentMagSize
			&& m_uReserveAmmo > 0)
		{
			StartReload();
		}

		// --- ADS + Fire input ---
		// RMB held -> aim. LMB hipfire-clicks force ADS for the duration of the
		// fire animation via m_fForceAimTimer so the recoil pose still plays.
		const bool bAimingRMB = g_xEngine.Input().IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_RIGHT);
		const bool bFirePressed = g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);

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
				m_fFireCooldown = m_fCurrentFireInterval;
				if (IsHoldingGun())
					m_fGunRecoilKick = 1.0f;   // brief anchor pull-back (visible recoil)
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
		// once layers exist; route through the layer. Photo mode (capture
		// harnesses / HumanShowcase) freezes these writes — the showcase owns
		// the parameters and CrossFades the state machine itself.
		if (m_pxBaseLayer && !RenderTest_GameplayState::s_bPhotoModeActive)
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
			const uint32_t uTake = std::min(m_uCurrentMagSize - m_uAmmoInClip, m_uReserveAmmo);
			m_uAmmoInClip += uTake;
			m_uReserveAmmo -= uTake;
		}

		// --- HUD ---
		UpdateAmmoHUD();

		// --- IK foot placement ---
		// Run last in OnUpdate so the targets are derived from the player's
		// settled position this frame. The animator's component-update phase ran
		// before this component (Animator order 15 < RenderTestPlayer order 101),
		// so any IK targets we set here apply to NEXT frame's animator solve —
		// there's always one frame of latency. Setting them at the end uses the
		// freshest position; setting at the start uses last frame's position.
		UpdateFootIK();

		// Arm IK that puts the hands ON the held gun (see UpdateGunIK). Runs after
		// the foot IK; the arm chains are independent of the leg chains so the two
		// solves don't interact.
		UpdateGunIK(fDt);

		// Jet trail: emit from the jetpack's nozzles while thrusting, off
		// otherwise. Runs last so it reads the player's settled state this frame.
		UpdateJetpack(fDt, bJetpackThrust);
		if (RenderTest_JetpackTuning::s_bShowcaseActive)
			AssertJetpackShowcaseCamera();
	}

	// Component contract. Ammo/cooldown/aim state is runtime-only and reset on
	// OnAwake; nothing needs to persist beyond the version tag.
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Ammo: %u / %u", m_uAmmoInClip, m_uReserveAmmo);
		ImGui::Text("Reload timer: %.2f", m_fReloadTimer);
		ImGui::Text("Aim layer weight: %.2f", m_fAimLayerWeight);
	}
#endif

private:
	Zenith_Maths::Vector3 ReadMovementInput() const
	{
		Zenith_Maths::Vector3 xInput(0.0f);
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_W) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_UP))    xInput.z += 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_S) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_DOWN))  xInput.z -= 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_A) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT))  xInput.x -= 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_D) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT)) xInput.x += 1.0f;
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

		// IK foot-placement chains. CreateLegChain configures pole vector (0,0,1)
		// (forward) and a knee hinge constraint along (1,0,0). HasChain guards keep
		// SetupLayeredAnimator idempotent — OnStart can fire twice (once during
		// automation, once after SaveScene/LoadScene reload).
		Flux_IKSolver& xIK = xController.GetIKSolver();
		if (!xIK.HasChain("LeftLeg"))
		{
			Flux_IKChain xLeft = Flux_IKSolver::CreateLegChain("LeftLeg",
				"LeftUpperLeg", "LeftLowerLeg", "LeftFoot");
			// Bump iterations + tighten tolerance — FABRIK with pole vector and
			// hinge constraints converges slowly near full chain extension, which
			// is exactly the foot-IK case. The default 10 iterations leaves
			// ~10-15mm error on a bent leg; 30 iterations brings error under 3mm.
			xLeft.m_uMaxIterations = 30;
			xLeft.m_fTolerance = 0.0005f;
			xIK.AddChain(xLeft);
		}
		if (!xIK.HasChain("RightLeg"))
		{
			Flux_IKChain xRight = Flux_IKSolver::CreateLegChain("RightLeg",
				"RightUpperLeg", "RightLowerLeg", "RightFoot");
			xRight.m_uMaxIterations = 30;
			xRight.m_fTolerance = 0.0005f;
			xIK.AddChain(xRight);
		}

		// Arm IK chains for holding a gun. The RIGHT arm is driven to a body-anchored
		// hold (with an end-effector orientation that squares the gun barrel forward);
		// the LEFT arm reaches the gun's foregrip for a two-handed weapon. Both have
		// NO target until a gun is picked up, so an unused chain is a no-op in Solve
		// (the foot-IK demo and the gunless tests are unaffected). Tuned like the
		// leg chains for clean convergence near full extension.
		if (!xIK.HasChain("RightArm"))
		{
			Flux_IKChain xRightArm = Flux_IKSolver::CreateArmChain("RightArm",
				"RightUpperArm", "RightLowerArm", "RightHand");
			xRightArm.m_uMaxIterations = 30;
			xRightArm.m_fTolerance = 0.0005f;
			xIK.AddChain(xRightArm);
		}
		if (!xIK.HasChain("LeftArm"))
		{
			Flux_IKChain xLeftArm = Flux_IKSolver::CreateArmChain("LeftArm",
				"LeftUpperArm", "LeftLowerArm", "LeftHand");
			xLeftArm.m_uMaxIterations = 30;
			xLeftArm.m_fTolerance = 0.0005f;
			xIK.AddChain(xLeftArm);
		}
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
		// Trajectory along the FULL camera basis (yaw + pitch). Sign
		// convention matches OnUpdate's xForward/xRight: at yaw=0 the player
		// faces +Z (camera is behind at -Z), so a shot fired at yaw=0
		// pitch=0 travels +Z (away from the camera).
		const float fYaw   = RenderTest_GameplayState::GetCameraYaw();
		const float fPitch = RenderTest_GameplayState::GetCameraPitch();
		const Zenith_Maths::Vector3 xFwd(-sinf(fYaw) * cosf(fPitch),
		                                  sinf(fPitch),
		                                  cosf(fYaw) * cosf(fPitch));
		const Zenith_Maths::Vector3 xRgt(cosf(fYaw), 0.0f, sinf(fYaw));

		Zenith_Maths::Vector3 xPlayerPos;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);

		// Muzzle origin. When a gun is held, fire from its (mesh-local) muzzle point
		// transformed into world space — the gun rides the right hand, so this is
		// where the barrel actually is. Otherwise fall back to the historical
		// hand-relative barrel offset (keeps the gunless smoke/decal path identical).
		Zenith_Maths::Vector3 xBarrel =
			xPlayerPos + xRgt * 0.3f + Zenith_Maths::Vector3(0.0f, 1.4f, 0.0f) + xFwd * 1.0f;
		if (IsHoldingGun() && m_xHeldGun.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Matrix4 xGunWorld;
			m_xHeldGun.GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xGunWorld);
			const Zenith_Maths::Vector3 xMuzzleLocal =
				m_xHeldGun.GetComponent<RenderTest_GunComponent>().GetSpec().m_xMuzzleLocal;
			xBarrel = Zenith_Maths::Vector3(xGunWorld * Zenith_Maths::Vector4(xMuzzleLocal, 1.0f));
		}

		// Hitscan: raycast from the barrel along the camera forward. Ignore
		// the player's own collider per the IK precedent in UpdateFootIK.
		const Zenith_Physics::RaycastResult xHit = Zenith_PhysicsQuery::RaycastIgnoring(
			xBarrel, xFwd, k_fMaxRange, m_xParentEntity.GetEntityID());

		if (xHit.m_bHit)
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[SHOOT] HIT yaw=%.3f pitch=%.3f barrel=(%.2f,%.2f,%.2f) fwd=(%.3f,%.3f,%.3f) "
				"hitPoint=(%.2f,%.2f,%.2f) hitNormal=(%.3f,%.3f,%.3f) hitDist=%.2f hitEntity=%u",
				fYaw, fPitch,
				xBarrel.x, xBarrel.y, xBarrel.z,
				xFwd.x, xFwd.y, xFwd.z,
				xHit.m_xHitPoint.x, xHit.m_xHitPoint.y, xHit.m_xHitPoint.z,
				xHit.m_xHitNormal.x, xHit.m_xHitNormal.y, xHit.m_xHitNormal.z,
				xHit.m_fDistance,
				xHit.m_xHitEntity.m_uIndex);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[SHOOT] MISS yaw=%.3f pitch=%.3f barrel=(%.2f,%.2f,%.2f) fwd=(%.3f,%.3f,%.3f)",
				fYaw, fPitch,
				xBarrel.x, xBarrel.y, xBarrel.z,
				xFwd.x, xFwd.y, xFwd.z);
		}

		if (xHit.m_bHit)
		{
			g_xEngine.Decals().SpawnDecal(
				xHit.m_xHitPoint, xHit.m_xHitNormal,
				/*pxTexture*/ nullptr,
				k_fDecalSize, k_fDecalLifetime);
		}

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
		// Total half-extent of the capsule = half-cylinder + radius = 0.75 + 0.3.
		// Capsule bottom sits at playerY - 1.05. Raycast from just below the
		// capsule bottom to detect contact with the ground.
		const float fCapsuleHalfExtent = 1.05f;
		const float fEpsilon = 0.02f;
		const Zenith_Maths::Vector3 xRayOrigin =
			xPlayerPos - Zenith_Maths::Vector3(0.0f, fCapsuleHalfExtent + fEpsilon, 0.0f);
		const Zenith_Maths::Vector3 xDown(0.0f, -1.0f, 0.0f);
		const Zenith_Physics::RaycastResult xResult =
			g_xEngine.Physics().Raycast(xRayOrigin, xDown, 0.2f);
		return xResult.m_bHit;
	}

	// Per-frame foot IK update. Raycasts down from each foot's last-frame world
	// position and sets the IK target to the hit point if the ray strikes
	// something other than the player's own collider. Cleared on miss or when
	// airborne (fWeight=0). Robust to missing animator/model — early-outs on
	// every component-presence check so the input-simulator test fixture (which
	// instantiates the component without an animator) doesn't crash.
	void UpdateFootIK()
	{
		if (!m_pxAnimator) return;
		// Photo mode (capture harnesses / the HumanShowcase test) plays clips
		// in place — ground IK would glue the feet mid-stride and distort the
		// gait, so it stands down entirely.
		if (RenderTest_GameplayState::s_bPhotoModeActive)
		{
			m_pxAnimator->ClearIKTarget("LeftLeg");
			m_pxAnimator->ClearIKTarget("RightLeg");
			return;
		}
		Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (!pxModel || !pxModel->HasSkeleton()) return;
		Flux_SkeletonInstance* pxSkel = pxModel->GetSkeletonInstance();
		if (!pxSkel) return;
		Zenith_SkeletonAsset* pxSkelAsset = pxSkel->GetSourceSkeleton();
		if (!pxSkelAsset) return;

		Zenith_TransformComponent& xT = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Matrix4 xWorld; xT.BuildModelMatrix(xWorld);

		// IK weight: only plant feet when the player is grounded AND stationary.
		// While walking, the walk animation drives the leg swing — running IK on
		// top would feedback-loop on its own previous output (target XZ derived
		// from the post-IK foot, which was set by the previous frame's target,
		// etc.) and lock the foot at the first frame's animated pose, suppressing
		// the swing entirely. The standard solution is to disable IK during
		// locomotion and only plant feet when standing still. (See unit tests
		// IKLocksFootXZAcrossFramesAtFullWeight and IKLetsAnimationDriveFootXZAtZeroWeight.)
		float fHorizontalSpeedSq = 0.0f;
		if (Zenith_ColliderComponent* pxCollider2 = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>())
		{
			Zenith_ColliderComponent& xCollider2 = *pxCollider2;
			if (xCollider2.HasValidBody())
			{
				const Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xCollider2.GetBodyID());
				fHorizontalSpeedSq = xVel.x * xVel.x + xVel.z * xVel.z;
			}
		}
		const bool bIsStationary = fHorizontalSpeedSq < 0.01f;   // < 0.1 m/s
		const float fWeight = (IsGrounded() && bIsStationary) ? 1.0f : 0.0f;

		auto SolveOneFoot = [&](const char* szChain, const char* szFootBone)
		{
			const int32_t iFoot = pxSkelAsset->GetBoneIndex(szFootBone);
			if (iFoot < 0) return;

			// World-space foot position derived from the previous frame's skinned
			// pose. One frame stale, but recomputing live would double the
			// hierarchy walk for no visible benefit.
			const Zenith_Maths::Matrix4& xFootModel = pxSkel->GetBoneModelTransform((uint32_t)iFoot);
			Zenith_Maths::Vector4 xFootW = xWorld * xFootModel * Zenith_Maths::Vector4(0, 0, 0, 1);
			Zenith_Maths::Vector3 xFootPos(xFootW);

			const Zenith_Maths::Vector3 xOrigin = xFootPos + Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f);
			// Ignore the player's own capsule. Without this, the foot ray (origin
			// inside the capsule) hits self and the helper clears IK every frame.
			const Zenith_Physics::RaycastResult xHit = Zenith_PhysicsQuery::RaycastIgnoring(
				xOrigin, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 1.5f,
				m_xParentEntity.GetEntityID());

			if (!xHit.m_bHit)
			{
				m_pxAnimator->ClearIKTarget(szChain);
				return;
			}

			constexpr float k_fAnkleHeight = 0.05f;
			Zenith_Maths::Vector3 xTargetWorld = xHit.m_xHitPoint
				+ Zenith_Maths::Vector3(0.0f, k_fAnkleHeight, 0.0f);
			// Convert the world-space target to model space NOW, using the world
			// matrix that was current when we read the foot position. The
			// alternative — passing a world-space target and relying on Solve
			// to inverse-transform with whatever world matrix is current at
			// solve time — produces a per-frame drag of `velocity * dt` because
			// physics moves the player between target-set and IK-solve. Pinning
			// the target in model space at set-time eliminates that lag.
			const Zenith_Maths::Matrix4 xInvWorld = glm::inverse(xWorld);
			const Zenith_Maths::Vector4 xTargetModel4 = xInvWorld * Zenith_Maths::Vector4(xTargetWorld, 1.0f);
			m_pxAnimator->SetIKTargetModelSpace(szChain, Zenith_Maths::Vector3(xTargetModel4), fWeight);
		};

		SolveOneFoot("LeftLeg",  "LeftFoot");
		SolveOneFoot("RightLeg", "RightFoot");
	}

	//=========================================================================
	// Gun pickup / drop / hold
	//=========================================================================

	bool IsHoldingGun() const
	{
		return m_xHeldGun.IsValid() && m_xHeldGun.HasComponent<RenderTest_GunComponent>();
	}

	// Per-frame: showcase auto-equip + camera, nearest-gun proximity, E pickup/drop,
	// and the HUD prompt. Cheap and fully gated — does nothing useful (and never
	// queries) for an empty-handed player with no guns in the scene, so the
	// gunless input-simulator tests are unaffected.
	void UpdateGunHandling(float)
	{
		// Showcase capture mode: re-assert the front photo camera every frame (it
		// survives the Play->Stop->Play GameplayState::Reset) and auto-equip the
		// chosen gun once the animator is up.
		if (RenderTest_GunTuning::s_bShowcaseActive)
		{
			AssertShowcaseCamera();
			// Sentinel COUNT == "floor" mode: leave the guns on the deck (no equip)
			// so the spawned row can be screenshotted.
			const bool bFloorMode =
				RenderTest_GunTuning::s_uShowcaseType >= static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT);
			if (!bFloorMode && !m_bGunAutoEquipped && m_pxAnimator)
			{
				Zenith_Entity xGun = FindGunByType(
					static_cast<RenderTest_Guns::GunType>(RenderTest_GunTuning::s_uShowcaseType));
				if (xGun.IsValid())
				{
					EquipGun(xGun);
					m_bGunAutoEquipped = true;
				}
			}
		}

		// Nearest free gun (only relevant when empty-handed).
		m_xNearestGun = Zenith_Entity();
		m_fNearestGunDist = 1e30f;
		if (!IsHoldingGun())
			m_xNearestGun = FindNearestGun(m_fNearestGunDist);

		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_E))
		{
			if (IsHoldingGun())
				DropHeldGun();
			else if (m_xNearestGun.IsValid() && m_fNearestGunDist <= RenderTest_Guns::fPICKUP_RADIUS)
				EquipGun(m_xNearestGun);
		}

		UpdateGunPrompt();
	}

	Zenith_Entity FindNearestGun(float& fOutDist)
	{
		fOutDist = 1e30f;
		Zenith_Entity xBest;
		Zenith_TransformComponent* pxPlayerTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxPlayerTransform == nullptr)
			return xBest;
		Zenith_Maths::Vector3 xPlayerPos;
		pxPlayerTransform->GetPosition(xPlayerPos);
		g_xEngine.Scenes().QueryAllScenes<RenderTest_GunComponent>().ForEach(
			[&](Zenith_EntityID, RenderTest_GunComponent& xGun)
			{
				if (xGun.IsHeld())
					return;
				Zenith_Entity xEnt = xGun.GetParentEntity();
				Zenith_TransformComponent* pxEntTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
				if (pxEntTransform == nullptr)
					return;
				Zenith_Maths::Vector3 xPos;
				pxEntTransform->GetPosition(xPos);
				const float fD = glm::length(xPos - xPlayerPos);
				if (fD < fOutDist) { fOutDist = fD; xBest = xEnt; }
			});
		return xBest;
	}

	Zenith_Entity FindGunByType(RenderTest_Guns::GunType eType)
	{
		Zenith_Entity xBest;
		g_xEngine.Scenes().QueryAllScenes<RenderTest_GunComponent>().ForEach(
			[&](Zenith_EntityID, RenderTest_GunComponent& xGun)
			{
				if (!xBest.IsValid() && !xGun.IsHeld() && xGun.GetType() == eType)
					xBest = xGun.GetParentEntity();
			});
		return xBest;
	}

	void EquipGun(Zenith_Entity xGun)
	{
		if (!xGun.IsValid()
			|| !xGun.HasComponent<RenderTest_GunComponent>()
			|| !xGun.HasComponent<Zenith_AttachmentComponent>())
			return;
		RenderTest_GunComponent& xGunComp = xGun.GetComponent<RenderTest_GunComponent>();
		if (xGunComp.IsHeld())
			return;

		// Attach to the right hand. Mount is identity: the gun mesh is built with the
		// barrel along +Z, and the right-arm end-effector IK (UpdateGunIK) orients
		// the hand — and thus the gun — so no bone-local rotation is baked in here.
		xGun.GetComponent<Zenith_AttachmentComponent>().AttachToBone(
			m_xParentEntity, "RightHand", Zenith_Maths::Matrix4(1.0f));
		xGunComp.SetHeld(true);
		m_xHeldGun = xGun;

		// Adopt the gun's weapon params + persisted ammo.
		const RenderTest_Guns::GunSpec& xSpec = xGunComp.GetSpec();
		m_uCurrentMagSize = xSpec.m_uMagSize;
		m_fCurrentFireInterval = xSpec.m_fFireInterval;
		m_uAmmoInClip = xGunComp.GetAmmoInClip();
		m_uReserveAmmo = xGunComp.GetReserve();
		m_fReloadTimer = 0.0f;
		m_fFireCooldown = 0.0f;
		m_fGunIKWeight = 0.0f;
		m_fGunRecoilKick = 0.0f;
		m_bLoggedGunHold = false;   // re-arm the one-shot hold diagnostic for this pickup

		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Guns] picked up %s", xGunComp.GetName());
	}

	void DropHeldGun()
	{
		if (!IsHoldingGun())
			return;
		Zenith_Entity xGun = m_xHeldGun;
		RenderTest_GunComponent& xGunComp = xGun.GetComponent<RenderTest_GunComponent>();

		// Persist ammo so picking the same gun back up resumes its state.
		xGunComp.SetAmmo(m_uAmmoInClip, m_uReserveAmmo);
		xGunComp.SetHeld(false);
		if (Zenith_AttachmentComponent* pxAttachment = xGun.TryGetComponent<Zenith_AttachmentComponent>())
			pxAttachment->Detach();
		PlaceGunOnFloor(xGun);

		m_xHeldGun = Zenith_Entity();
		// Restore the empty-handed baseline (matches OnAwake / the gunless tests).
		m_uCurrentMagSize = k_uMagSize;
		m_fCurrentFireInterval = k_fFireInterval;
		m_uAmmoInClip = k_uMagSize;
		m_uReserveAmmo = 90;

		if (m_pxAnimator)
		{
			m_pxAnimator->ClearIKTarget("RightArm");
			m_pxAnimator->ClearIKTarget("LeftArm");
		}
		m_fGunIKWeight = 0.0f;

		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Guns] dropped %s", xGunComp.GetName());
	}

	// Settle a dropped gun on the floor a metre in front of the player (the
	// attachment is already detached, so its OnLateUpdate leaves this transform
	// alone). Lies the gun flat — same rest pose as the spawn.
	void PlaceGunOnFloor(Zenith_Entity xGun)
	{
		if (!xGun.HasComponent<Zenith_TransformComponent>()
			|| !m_xParentEntity.HasComponent<Zenith_TransformComponent>())
			return;
		Zenith_TransformComponent& xPT = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xPlayerPos; xPT.GetPosition(xPlayerPos);
		Zenith_Maths::Quat xPlayerRot; xPT.GetRotation(xPlayerRot);
		const Zenith_Maths::Vector3 xFwd =
			glm::normalize(xPlayerRot * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

		const Zenith_Maths::Vector3 xProbe =
			xPlayerPos + xFwd * 1.0f + Zenith_Maths::Vector3(0.0f, 0.6f, 0.0f);
		const Zenith_Physics::RaycastResult xHit = Zenith_PhysicsQuery::RaycastIgnoring(
			xProbe, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 4.0f, m_xParentEntity.GetEntityID());
		// On a hit, rest on the floor; on a miss (dropped over an edge with no floor
		// within range) fall back to roughly feet level rather than the player origin
		// (which sits ~1 m above the feet) so the gun doesn't hover in mid-air.
		const Zenith_Maths::Vector3 xRestPos = xHit.m_bHit
			? xHit.m_xHitPoint + Zenith_Maths::Vector3(0.0f, 0.05f, 0.0f)
			: xPlayerPos + xFwd * 1.0f - Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		const float fYaw = atan2f(xFwd.x, xFwd.z);
		const Zenith_Maths::Quat xRest =
			glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))
			* glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

		Zenith_TransformComponent& xGT = xGun.GetComponent<Zenith_TransformComponent>();
		xGT.SetPosition(xRestPos);
		xGT.SetRotation(xRest);
	}

	void UpdateGunPrompt()
	{
		if (!m_pxGunPromptText)
			return;
		if (IsHoldingGun())
		{
			m_pxGunPromptText->SetText(std::string("[E] drop ")
				+ m_xHeldGun.GetComponent<RenderTest_GunComponent>().GetName());
		}
		else if (m_xNearestGun.IsValid()
			&& m_fNearestGunDist <= RenderTest_Guns::fPICKUP_RADIUS
			&& m_xNearestGun.HasComponent<RenderTest_GunComponent>())
		{
			m_pxGunPromptText->SetText(std::string("[E] pick up ")
				+ m_xNearestGun.GetComponent<RenderTest_GunComponent>().GetName());
		}
		else
		{
			m_pxGunPromptText->SetText("");
		}
	}

	// Arm IK that places the hands ON the held gun. The right arm is driven to a
	// body-anchored hold (with an end-effector orientation squaring the gun barrel
	// to model +Z); for a two-handed gun the left (support) arm reaches the gun's
	// foregrip. The gun rides the right hand via the attachment, so the right hand
	// is always on the grip by construction — the right-arm IK just chooses WHERE,
	// keeping the hold within both arms' reach (see RenderTest_Guns.h). A pistol
	// uses only the right hand (no left-arm target). Weight ramps to 0 during a
	// reload so the reload clip plays.
	void UpdateGunIK(float fDt)
	{
		if (!m_pxAnimator)
			return;

		if (!IsHoldingGun())
		{
			if (m_fGunIKWeight > 0.0f)
			{
				m_pxAnimator->ClearIKTarget("RightArm");
				m_pxAnimator->ClearIKTarget("LeftArm");
				m_fGunIKWeight = 0.0f;
			}
			return;
		}

		Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
			return;
		Zenith_ModelComponent& xModel = *pxModel;
		if (!xModel.HasSkeleton())
			return;

		const RenderTest_Guns::GunSpec& xSpec =
			m_xHeldGun.GetComponent<RenderTest_GunComponent>().GetSpec();

		const bool bReloading = (m_fReloadTimer > 0.0f);
		const float fTarget = bReloading ? 0.0f : 1.0f;
		m_fGunIKWeight = glm::mix(m_fGunIKWeight, fTarget, glm::clamp(fDt * 10.0f, 0.0f, 1.0f));
		if (m_fGunIKWeight < 0.02f)
		{
			m_pxAnimator->ClearIKTarget("RightArm");
			m_pxAnimator->ClearIKTarget("LeftArm");
			return;
		}

		m_fGunRecoilKick = glm::max(0.0f, m_fGunRecoilKick - fDt * 6.0f);

		// Right-hand grip anchor (player model space) + live CLI overrides + recoil.
		Zenith_Maths::Vector3 xAnchor = xSpec.m_xHoldAnchorModel;
		if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fAnchorX)) xAnchor.x = RenderTest_GunTuning::s_fAnchorX;
		if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fAnchorY)) xAnchor.y = RenderTest_GunTuning::s_fAnchorY;
		if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fAnchorZ)) xAnchor.z = RenderTest_GunTuning::s_fAnchorZ;
		xAnchor.z -= m_fGunRecoilKick * 0.08f;

		// Desired gun orientation (model space): (0,0,0) => barrel along model +Z.
		Zenith_Maths::Vector3 xAimDeg = xSpec.m_xAimEulerDeg;
		if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fAimPitchDeg)) xAimDeg.x = RenderTest_GunTuning::s_fAimPitchDeg;
		if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fAimYawDeg))   xAimDeg.y = RenderTest_GunTuning::s_fAimYawDeg;
		if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fAimRollDeg))  xAimDeg.z = RenderTest_GunTuning::s_fAimRollDeg;
		const Zenith_Maths::Quat xAimRot = EulerDegToQuat(xAimDeg);

		m_pxAnimator->SetIKTargetModelSpace("RightArm", xAnchor, xAimRot, m_fGunIKWeight);

		if (xSpec.m_bTwoHanded)
		{
			// Foregrip is a GUN-local point; the gun rides the right hand with an
			// identity mount, so its model-space position is RightHandBoneModel *
			// foregripLocal. Reading the posed hand bone tracks the actual gun (one
			// frame stale, like the foot/tennis IK).
			Zenith_Maths::Matrix4 xHandModel;
			if (xModel.GetBoneModelMatrix("RightHand", xHandModel))
			{
				Zenith_Maths::Vector3 xFg = xSpec.m_xForegripLocal;
				if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fForegripY)) xFg.y = RenderTest_GunTuning::s_fForegripY;
				if (RenderTest_GunTuning::IsSet(RenderTest_GunTuning::s_fForegripZ)) xFg.z = RenderTest_GunTuning::s_fForegripZ;
				const Zenith_Maths::Vector4 xFgModel = xHandModel * Zenith_Maths::Vector4(xFg, 1.0f);
				m_pxAnimator->SetIKTargetModelSpace("LeftArm", Zenith_Maths::Vector3(xFgModel), m_fGunIKWeight);
			}
		}
		else
		{
			m_pxAnimator->ClearIKTarget("LeftArm");
		}

		// One-shot diagnostic once the hold has settled: log the world positions of
		// the hands, the gun, and the anchor so hands-on-gun can be confirmed
		// numerically (independent of the capture camera).
		if (!m_bLoggedGunHold && m_fGunIKWeight > 0.9f
			&& m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_bLoggedGunHold = true;
			Zenith_Maths::Matrix4 xPlayerW;
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xPlayerW);
			Zenith_Maths::Matrix4 xRH(1.0f), xLH(1.0f);
			xModel.GetBoneModelMatrix("RightHand", xRH);
			xModel.GetBoneModelMatrix("LeftHand", xLH);
			const Zenith_Maths::Vector3 xRHw(xPlayerW * xRH * Zenith_Maths::Vector4(0, 0, 0, 1));
			const Zenith_Maths::Vector3 xLHw(xPlayerW * xLH * Zenith_Maths::Vector4(0, 0, 0, 1));
			const Zenith_Maths::Vector3 xAnchorW(xPlayerW * Zenith_Maths::Vector4(xAnchor, 1.0f));
			Zenith_Maths::Vector3 xGunW(0.0f);
			if (Zenith_TransformComponent* pxGunTransform = m_xHeldGun.TryGetComponent<Zenith_TransformComponent>())
				pxGunTransform->GetPosition(xGunW);
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[GunIK] %s held: RightHand=(%.2f,%.2f,%.2f) gun=(%.2f,%.2f,%.2f) anchorW=(%.2f,%.2f,%.2f) LeftHand=(%.2f,%.2f,%.2f)",
				m_xHeldGun.GetComponent<RenderTest_GunComponent>().GetName(),
				xRHw.x, xRHw.y, xRHw.z, xGunW.x, xGunW.y, xGunW.z,
				xAnchorW.x, xAnchorW.y, xAnchorW.z, xLHw.x, xLHw.y, xLHw.z);
		}
	}

	// Euler (degrees, pitch/X yaw/Y roll/Z) -> quat, composed yaw * pitch * roll.
	static Zenith_Maths::Quat EulerDegToQuat(const Zenith_Maths::Vector3& xDeg)
	{
		return glm::angleAxis(glm::radians(xDeg.y), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))
		     * glm::angleAxis(glm::radians(xDeg.x), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f))
		     * glm::angleAxis(glm::radians(xDeg.z), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	}

	// Park a front 3/4 photo camera on the player for the --rendertest-gun-showcase
	// capture. Re-asserted every frame so it survives GameplayState::Reset.
	void AssertShowcaseCamera()
	{
		// The offset is world-space relative to the player entity origin; yaw/pitch
		// are DERIVED so the camera looks at the chosen point, independent of the
		// player's world position. Camera-component forward = (-sin yaw, sin pitch,
		// cos yaw), so yaw = atan2(-dir.x, dir.z), pitch = asin(dir.y).
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		const bool bFloorMode =
			RenderTest_GunTuning::s_uShowcaseType >= static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT);
		// Floor mode: an elevated vantage behind the player looking forward+down at
		// the gun row (the guns spawn ~5 m ahead at Z=261, the player faces +Z).
		// Held mode: a front 3/4 vantage looking at the chest where the gun is held.
		const Zenith_Maths::Vector3 xOffset = bFloorMode
			? Zenith_Maths::Vector3(0.0f, 2.0f, 2.2f)
			: Zenith_Maths::Vector3(1.8f, 0.9f, 2.8f);
		const Zenith_Maths::Vector3 xLook = bFloorMode
			? Zenith_Maths::Vector3(0.0f, -0.9f, 5.2f)    // close overhead view of the gun row
			: Zenith_Maths::Vector3(0.0f, 1.1f, 0.4f);    // chest/gun
		const Zenith_Maths::Vector3 xDir = glm::normalize(xLook - xOffset);
		RenderTest_GameplayState::s_fPhotoOffsetX = xOffset.x;
		RenderTest_GameplayState::s_fPhotoOffsetY = xOffset.y;
		RenderTest_GameplayState::s_fPhotoOffsetZ = xOffset.z;
		RenderTest_GameplayState::s_fPhotoYaw = atan2f(-xDir.x, xDir.z);
		RenderTest_GameplayState::s_fPhotoPitch = asinf(glm::clamp(xDir.y, -1.0f, 1.0f));
	}

	//=========================================================================
	// Jetpack
	//=========================================================================

	bool IsJetpackEquipped() const
	{
		return m_xJetpack.IsValid() && m_xJetpack.HasComponent<RenderTest_JetpackComponent>();
	}

	// Lazily resolve the procedural jetpack (spawned post scene-load, possibly
	// after the player's OnStart). Cheap once found — early-outs thereafter.
	// QueryAllScenes on a registered-but-instance-less type is a no-op, so this
	// is safe in the jetpack-less input-simulator tests (same pattern as the
	// gun handling's FindNearestGun).
	void ResolveJetpack()
	{
		if (IsJetpackEquipped())
			return;
		g_xEngine.Scenes().QueryAllScenes<RenderTest_JetpackComponent>().ForEach(
			[&](Zenith_EntityID, RenderTest_JetpackComponent& xJet)
			{
				if (!m_xJetpack.IsValid())
					m_xJetpack = xJet.GetParentEntity();
			});
	}

	// Drive the jetpack's jet-trail emitter. While thrusting it streams from the
	// two nozzles (alternating each frame for a twin-exhaust look), aimed along
	// the jetpack's local exhaust direction in world space; otherwise the emitter
	// is switched off and the in-flight particles fade out. The jetpack transform
	// is one frame stale (updated by the attachment in OnLateUpdate) — negligible
	// for a trail, same as the foot/gun IK.
	void UpdateJetpack(float fDt, bool bThrust)
	{
		(void)fDt;
		if (!IsJetpackEquipped())
			return;
		Zenith_Entity xJet = m_xJetpack;
		xJet.GetComponent<RenderTest_JetpackComponent>().SetThrusting(bThrust);

		if (!xJet.HasComponent<Zenith_ParticleEmitterComponent>()
			|| !xJet.HasComponent<Zenith_TransformComponent>())
			return;
		Zenith_ParticleEmitterComponent& xEmitter = xJet.GetComponent<Zenith_ParticleEmitterComponent>();

		if (!bThrust)
		{
			xEmitter.SetEmitting(false);
			return;
		}

		const RenderTest_JetpackComponent::Spec& xSpec =
			xJet.GetComponent<RenderTest_JetpackComponent>().GetSpec();

		Zenith_Maths::Matrix4 xJetWorld;
		xJet.GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xJetWorld);

		const Zenith_Maths::Vector3& xNozzleLocal = xSpec.m_axNozzleLocal[m_uJetNozzleAlt & 1u];
		m_uJetNozzleAlt++;

		const Zenith_Maths::Vector3 xNozzleWorld(
			xJetWorld * Zenith_Maths::Vector4(xNozzleLocal, 1.0f));
		Zenith_Maths::Vector3 xExhaustDir(
			xJetWorld * Zenith_Maths::Vector4(xSpec.m_xExhaustLocalDir, 0.0f));
		if (glm::length(xExhaustDir) < 1e-4f)
			xExhaustDir = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
		xExhaustDir = glm::normalize(xExhaustDir);

		xEmitter.SetEmitting(true);
		xEmitter.SetEmitPosition(xNozzleWorld);
		xEmitter.SetEmitDirection(xExhaustDir);
	}

	// Park a back-3/4 photo camera on the rising player for the
	// --rendertest-jetpack-showcase capture. The offset is world-space relative
	// to the player origin (so it tracks the player upward); yaw/pitch are derived
	// to look at the torso/back where the jetpack + trail are. Re-asserted every
	// frame so it survives GameplayState::Reset.
	void AssertJetpackShowcaseCamera()
	{
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		const Zenith_Maths::Vector3 xOffset(1.6f, 1.3f, -3.0f);   // right + up + behind the back
		const Zenith_Maths::Vector3 xLook(0.0f, 0.4f, -0.1f);     // torso/back
		const Zenith_Maths::Vector3 xDir = glm::normalize(xLook - xOffset);
		RenderTest_GameplayState::s_fPhotoOffsetX = xOffset.x;
		RenderTest_GameplayState::s_fPhotoOffsetY = xOffset.y;
		RenderTest_GameplayState::s_fPhotoOffsetZ = xOffset.z;
		RenderTest_GameplayState::s_fPhotoYaw = atan2f(-xDir.x, xDir.z);
		RenderTest_GameplayState::s_fPhotoPitch = asinf(glm::clamp(xDir.y, -1.0f, 1.0f));
	}

	static constexpr uint32_t k_uMagSize        = 30;
	static constexpr float    k_fFireInterval   = 0.12f;  // ~500 RPM
	static constexpr float    k_fReloadDuration = 1.5f;

	// Jetpack tuning. Thrust accel exceeds gravity (~9.8) so a held Space climbs;
	// the ascent cap sits above the jump velocity so the initial jump pop is
	// preserved when leaving the ground (the cap > jump invariant — when thrust
	// engages on the same grounded frame as the jump it must not clamp the pop
	// away; enforced by the JetpackThrustRaisesAndCapsVy unit test).
	static constexpr float k_fJetpackThrustAccel = 22.0f;  // m/s^2 upward while held
	static constexpr float k_fJetpackMaxAscent   = 8.0f;   // m/s ascent ceiling
	// Jump pop velocity. Also the default for the m_fJumpVelocity member below;
	// co-located here so the cap > jump relationship reads as one design unit.
	static constexpr float k_fJumpVelocity       = 6.0f;

	// Hitscan tuning. k_fMaxRange covers the central platform and the
	// surrounding terrain at RenderTest's playable scale. The decal lifetime
	// is generous so a player can shoot a wall and still see the holes ten
	// seconds later — the 64-slot ring buffer caps live decals regardless.
	static constexpr float k_fMaxRange      = 200.0f;
	static constexpr float k_fDecalSize     = 0.15f;
	static constexpr float k_fDecalLifetime = 30.0f;

	Zenith_Entity m_xParentEntity;

	Zenith_AnimatorComponent*         m_pxAnimator = nullptr;
	Flux_AnimationLayer*              m_pxBaseLayer = nullptr;
	Flux_AnimationLayer*              m_pxAimLayer = nullptr;
	Zenith_ParticleEmitterComponent*  m_pxMuzzleEmitter = nullptr;
	Zenith_UI::Zenith_UIText*         m_pxAmmoText = nullptr;

	float    m_fMoveSpeed       = 5.0f;
	float    m_fSprintMultiplier = 1.7f;
	float    m_fJumpVelocity    = k_fJumpVelocity;
	float    m_fRotationSpeed   = 10.0f;
	float    m_fAimLayerWeight  = 0.0f;
	float    m_fForceAimTimer   = 0.0f;
	float    m_fFireCooldown    = 0.0f;
	float    m_fReloadTimer     = 0.0f;
	uint32_t m_uAmmoInClip      = k_uMagSize;
	uint32_t m_uReserveAmmo     = 90;

	// --- Gun pickup/drop/hold state ---
	Zenith_Entity m_xHeldGun;                 // invalid == empty-handed
	Zenith_Entity m_xNearestGun;              // nearest free gun (refreshed each frame, empty-handed)
	float    m_fNearestGunDist  = 1e30f;
	bool     m_bGunAutoEquipped = false;      // showcase one-shot
	bool     m_bLoggedGunHold   = false;      // one-shot diagnostic latch (per pickup)
	float    m_fGunIKWeight     = 0.0f;       // arm-IK blend (0 while reloading/empty)
	float    m_fGunRecoilKick   = 0.0f;       // decaying anchor pull-back on fire
	// Active weapon params: the held gun's, or the historical baseline when
	// empty-handed (so the gunless input-simulator tests are unaffected).
	uint32_t m_uCurrentMagSize  = k_uMagSize;
	float    m_fCurrentFireInterval = k_fFireInterval;
	Zenith_UI::Zenith_UIText* m_pxGunPromptText = nullptr;

	// --- Jetpack state ---
	Zenith_Entity m_xJetpack;          // resolved lazily; invalid == no jetpack worn
	uint32_t      m_uJetNozzleAlt = 0; // frame counter to alternate the two nozzles

	inline static RenderTest_PlayerComponent* s_pxActiveInstance = nullptr;
};
