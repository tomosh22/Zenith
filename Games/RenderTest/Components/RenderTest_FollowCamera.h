#pragma once
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
// Platform aggregator: pulls in the active platform's Zenith_Window (GLFW on
// Windows, ANativeWindow stub on Android). Direct inclusion of the Windows
// header here would break AGDE/Android compilation since the Android project
// doesn't have GLFW headers on its include path.
#include "Zenith_OS_Include.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

#include "RenderTest/Components/RenderTest_GameplayState.h"

// Third-person shooter follow camera.
//
// - Mouse moves yaw + pitch directly. Pitch clamps to a sensible cone, yaw
//   wraps to [0, 2pi] to avoid float drift after extended play.
// - Cursor is captured on Awake (no separate "press to look" toggle).
// - Hipfire offset is over-the-shoulder; ADS offset pulls in closer with a
//   narrower FOV. Both offsets, plus FOV, lerp smoothly so toggling ADS
//   doesn't snap the view.
// - Player entity is resolved once on Awake and cached. Yaw/pitch are
//   published to RenderTest_GameplayState so the player behaviour can read
//   them for camera-relative movement without an include cycle.
class RenderTest_FollowCamera : public Zenith_ScriptBehaviour
{
public:
	RenderTest_FollowCamera(Zenith_Entity& xEntity)
		: Zenith_ScriptBehaviour()
	{
		m_xParentEntity = xEntity;
	}

	ZENITH_BEHAVIOUR_TYPE_NAME(RenderTest_FollowCamera)

	void OnAwake() override
	{
		// Reset shared state — survives Play->Stop->Play unless we clear it.
		RenderTest_GameplayState::Reset();

		// Cache the player entity so OnLateUpdate doesn't re-search by name
		// every frame. Asserts only if the entity is missing.
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxSceneData)
		{
			Zenith_Entity xPlayer = pxSceneData->FindEntityByName("Player");
			if (xPlayer.IsValid())
			{
				m_uPlayerEntityID = xPlayer.GetEntityID();
			}
		}

		// Cursor capture is deferred to OnLateUpdate (which only runs in Play
		// mode). Capturing in OnAwake would grab the cursor at scene load —
		// before the user enters Play — making the editor unusable.
	}

	void OnDisable() override
	{
		// Released on Play->Stop transition or scene unload.
		ReleaseCursorIfHeld();
	}

	void OnDestroy() override
	{
		ReleaseCursorIfHeld();
	}

	void OnLateUpdate(float fDt) override
	{
		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (!pxSceneData)
			return;

		// Lazily capture the cursor on the first Play-mode frame. OnLateUpdate
		// only runs when the editor is Playing (or in non-tools builds where
		// there's no editor at all), so by the time we reach this line we're
		// already in the right mode. Skipped under input simulator so tests
		// don't fight the user's real cursor.
		EnsureCursorCaptured();

		// Re-resolve player if we lost it (entity destroyed / scene reload).
		if (m_uPlayerEntityID == INVALID_ENTITY_ID || !pxSceneData->EntityExists(m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = pxSceneData->FindEntityByName("Player");
			if (!xPlayer.IsValid())
				return;
			m_uPlayerEntityID = xPlayer.GetEntityID();
		}

		Zenith_Entity xPlayer = pxSceneData->GetEntity(m_uPlayerEntityID);
		if (!xPlayer.IsValid() || !xPlayer.HasComponent<Zenith_TransformComponent>())
			return;

		// --- Photo mode (visual tests / capture harnesses) ---
		// Same camera-component view path as normal play; just a scripted
		// world-space offset + yaw/pitch instead of mouse-look (yaw 0 = the
		// spawn-facing direction, pi = parked in front looking back).
		if (RenderTest_GameplayState::s_bPhotoModeActive)
		{
			Zenith_Maths::Vector3 xPhotoPlayerPos;
			xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPhotoPlayerPos);
			Zenith_CameraComponent& xPhotoCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
			xPhotoCamera.SetPosition(xPhotoPlayerPos + Zenith_Maths::Vector3(
				RenderTest_GameplayState::s_fPhotoOffsetX,
				RenderTest_GameplayState::s_fPhotoOffsetY,
				RenderTest_GameplayState::s_fPhotoOffsetZ));
			xPhotoCamera.SetYaw(RenderTest_GameplayState::s_fPhotoYaw);
			xPhotoCamera.SetPitch(RenderTest_GameplayState::s_fPhotoPitch);
			return;
		}

		// --- Mouse-look ---
		// Skip the first frame's delta to avoid a camera jump caused by the
		// large delta between (0,0) and the cursor's actual screen position
		// the first time we sample.
		Zenith_Maths::Vector2_64 xMouseDelta;
		g_xEngine.Input().GetMouseDelta(xMouseDelta);
		if (m_bFirstMouseSample)
		{
			xMouseDelta = Zenith_Maths::Vector2_64(0.0, 0.0);
			m_bFirstMouseSample = false;
		}

		const float k_fMouseSensitivity = 1.0f / 500.0f;
		// Match the Test game's sign convention (PlayerController_Behaviour.cpp:55-58):
		// mouse-right and mouse-up DECREASE yaw/pitch.
		m_fCameraYaw   -= static_cast<float>(xMouseDelta.x) * k_fMouseSensitivity;
		m_fCameraPitch -= static_cast<float>(xMouseDelta.y) * k_fMouseSensitivity;

		// Wrap yaw to [0, 2pi] (prevents float-precision drift over time).
		const float fTwoPi = static_cast<float>(Zenith_Maths::Pi * 2.0);
		if (m_fCameraYaw < 0.0f) m_fCameraYaw += fTwoPi;
		if (m_fCameraYaw > fTwoPi) m_fCameraYaw -= fTwoPi;

		// Clamp pitch so we can't go fully overhead / under-foot.
		m_fCameraPitch = glm::clamp(m_fCameraPitch, -1.2f, 0.6f);

		// Publish to shared state so the player behaviour can read camera
		// yaw/pitch for camera-relative movement and bullet aiming.
		RenderTest_GameplayState::s_fCameraYaw   = m_fCameraYaw;
		RenderTest_GameplayState::s_fCameraPitch = m_fCameraPitch;

		// --- Offset selection (over-the-shoulder hipfire vs tighter ADS) ---
		const Zenith_Maths::Vector3 xHipfireOffset(0.5f, 2.0f, -4.0f);
		const Zenith_Maths::Vector3 xAdsOffset    (0.4f, 1.8f, -2.5f);
		const bool bAiming = RenderTest_GameplayState::IsLocalPlayerAiming();
		const Zenith_Maths::Vector3 xTargetOffset = bAiming ? xAdsOffset : xHipfireOffset;
		const float fOffsetLerp = glm::clamp(fDt * 8.0f, 0.0f, 1.0f);
		m_xCurrentOffset = glm::mix(m_xCurrentOffset, xTargetOffset, fOffsetLerp);

		// Rotate the local offset by yaw so it stays relative to the camera's
		// facing. Note the NEGATED yaw: the camera-component's SetYaw uses the
		// GLM convention (CCW from above is positive), but the offset needs
		// to rotate the OPPOSITE way to keep the camera behind a player whose
		// world-yaw is the negation of the camera's yaw. This matches the
		// Test game's PlayerController_Behaviour.cpp:221 convention — they
		// happen to use a manual `(sin yaw, 0, -cos yaw) * dist` formula
		// which is equivalent to rotate(-yaw) * (0, 0, -dist). Without the
		// negation, the camera ends up on the WRONG side of the player and
		// W moves the player toward the camera instead of away from it.
		// Pitch is applied to the camera angle, not the offset, so the
		// camera orbits the player rather than translating up/down with pitch.
		const Zenith_Maths::Matrix4 xYawRot = glm::rotate(-m_fCameraYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const Zenith_Maths::Vector4 xRotatedOffset4 = xYawRot * Zenith_Maths::Vector4(m_xCurrentOffset, 0.0f);
		const Zenith_Maths::Vector3 xRotatedOffset(xRotatedOffset4);

		Zenith_Maths::Vector3 xPlayerPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);

		Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
		xCamera.SetPosition(xPlayerPos + xRotatedOffset);
		xCamera.SetYaw(m_fCameraYaw);
		xCamera.SetPitch(m_fCameraPitch);

		// --- FOV (tightens slightly during ADS for sight-line feel) ---
		const float fHipfireFOV = glm::radians(70.0f);
		const float fAdsFOV     = glm::radians(60.0f);
		const float fTargetFOV  = bAiming ? fAdsFOV : fHipfireFOV;
		const float fFOVLerp = glm::clamp(fDt * 8.0f, 0.0f, 1.0f);
		m_fCurrentFOV = glm::mix(m_fCurrentFOV, fTargetFOV, fFOVLerp);
		xCamera.SetFOV(m_fCurrentFOV);
	}

private:
	// Cursor capture policy:
	//
	// In tools builds the cursor is NEVER captured. The editor needs the
	// cursor available for menus, panels, and the IDE-style workflow; the
	// previous Play-mode-gated capture path was unreliable across the
	// scene-reset that fires on Stop, leaving the cursor stuck in the window.
	// Mouse-look still works in tools builds via GetMouseDelta — the cursor
	// just stays visible. Edge: when the cursor leaves the window, mouse
	// delta drops to zero so look-around stops at the window edge. Acceptable
	// for editor preview; click back inside the window to resume.
	//
	// In non-tools (shipping) builds we capture on first Play frame and
	// release on shutdown. Player can also toggle with Tab during play.
	void EnsureCursorCaptured()
	{
#ifdef ZENITH_TOOLS
		// Disabled in editor builds. See above.
		return;
#else
		if (m_bCursorCaptured)
			return;
#ifdef ZENITH_INPUT_SIMULATOR
		if (Zenith_InputSimulator::IsEnabled())
			return;
#endif
		if (Zenith_Window::GetInstance())
		{
			Zenith_Window::GetInstance()->EnableCaptureCursor();
			m_bCursorCaptured = true;
		}
#endif
	}

	void ReleaseCursorIfHeld()
	{
		if (!m_bCursorCaptured)
			return;
		if (Zenith_Window::GetInstance())
		{
			Zenith_Window::GetInstance()->DisableCaptureCursor();
		}
		m_bCursorCaptured = false;
	}

	// Camera angles in radians.
	float m_fCameraYaw   = 0.0f;
	float m_fCameraPitch = -0.15f;

	// Local-space offset from player; rotated by yaw before being applied.
	// Initialized to the hipfire offset so the very first frame snaps there
	// instead of lerping from zero.
	Zenith_Maths::Vector3 m_xCurrentOffset = Zenith_Maths::Vector3(0.5f, 2.0f, -4.0f);

	// Initial FOV matches the hipfire target so we don't lerp from 0.
	float m_fCurrentFOV = glm::radians(70.0f);

	// Skip first frame's mouse delta to avoid an initial camera-yaw jump.
	bool m_bFirstMouseSample = true;

	// Tracks whether we've grabbed the OS cursor so OnDisable can release it
	// cleanly when leaving Play mode.
	bool m_bCursorCaptured = false;

	Zenith_EntityID m_uPlayerEntityID = INVALID_ENTITY_ID;
};
