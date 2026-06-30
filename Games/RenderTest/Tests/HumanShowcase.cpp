#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "RenderTest/Components/RenderTest_GameplayState.h"
#include "Flux/Flux_GraphicsImpl.h"   // SetSunOverride (cinematic key for the capture)
#include "Flux/IBL/Flux_IBLImpl.h"    // IBL fill intensity
#include "Flux/HDR/Flux_HDRImpl.h"    // pinned manual exposure
#include "Core/Zenith_GraphicsOptions.h"  // toggle auto-exposure off for the capture

// Manual-only VISUAL showcase of the StickFigure human: parks the photo camera
// in front of the player and walks the animation set (idle, face close-up,
// walk, run, the three attacks, reload, death), logging
// "[RenderTest] HUMAN_MARK <name> rel=<n> global=<N>" at each transition so a
// capture harness can take deterministic --screenshot dumps at the global
// frame indices (fixed-dt runs replay identically).
//
// The player's controller composes LAYERS, which bypasses the editor's
// direct-play path entirely — so the showcase drives the BASE layer's state
// machine instead: locomotion through the real Speed/IsSprinting parameters
// (photo mode freezes the player component's per-frame writes), and the non-state
// clips (attacks/reload/death) through showcase-added states entered via
// CrossFade. Those states have no outgoing transitions, so each holds until
// the next CrossFade.
//
// m_bManualOnly: a multi-minute screenshot demo has no per-commit signal —
// run it by name: --automated-test HumanShowcase.

namespace
{
	bool s_bHumanShowcaseSawAnimator = false;

	// CLI float override (mirrors RenderTest_Tennis::RT_TennisArgFloat) so the
	// photo camera can be orbited from the command line for multi-angle shadow
	// captures: --humancam-x/y/z/yaw/pitch=<float>. Yaw convention (see
	// Zenith_CameraComponent forward = (-sin yaw, sin pitch, cos yaw)): to look at
	// the player from offset (ox,oz), yaw = atan2(ox, -oz).
	float HumanShowcase_ArgFloat(const char* szPrefix, float fDefault)
	{
#ifdef ZENITH_WINDOWS
		const size_t ulLen = std::strlen(szPrefix);
		for (int i = 1; i < __argc; i++)
			if (std::strncmp(__argv[i], szPrefix, ulLen) == 0)
				return static_cast<float>(std::atof(__argv[i] + ulLen));
#else
		(void)szPrefix;
#endif
		return fDefault;
	}

	bool HumanShowcase_HasArgPrefix(const char* szPrefix)
	{
#ifdef ZENITH_WINDOWS
		const size_t ulLen = std::strlen(szPrefix);
		for (int i = 1; i < __argc; i++)
			if (std::strncmp(__argv[i], szPrefix, ulLen) == 0) return true;
#else
		(void)szPrefix;
#endif
		return false;
	}

	// When the camera is driven from the CLI (--humancam-*), the scripted camera
	// re-parks + locomotion in Step_HumanShowcase are suppressed so the figure
	// holds an idle pose under the requested camera for the whole run — lets a
	// capture harness grab any frame from any angle without racing the schedule.
	bool s_bHumanCamOverride = false;

	void Human_Mark(const char* szName, int iRelFrame)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[RenderTest] HUMAN_MARK %s rel=%d global=%u",
			szName, iRelFrame, g_xEngine.Frame().GetFrameIndex());
	}

	Zenith_Entity Human_FindPlayer()
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		if (!xScene.IsValid()) return Zenith_Entity();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (!pxSceneData) return Zenith_Entity();
		return pxSceneData->FindEntityByName("Player");
	}

	// Base layer (index 0) state machine of the player's controller, or null
	// while the player's OnStart hasn't built the layers yet.
	Flux_AnimationStateMachine* Human_BaseSM()
	{
		Zenith_Entity xPlayer = Human_FindPlayer();
		Zenith_AnimatorComponent* pxAnimator = xPlayer.TryGetComponent<Zenith_AnimatorComponent>();
		if (pxAnimator == nullptr)
		{
			return nullptr;
		}
		Flux_AnimationController& xController = pxAnimator->GetController();
		Flux_AnimationLayer* pxBase = xController.GetLayer(0);
		return pxBase ? &pxBase->GetStateMachine() : nullptr;
	}

	void Human_SetCamera(float fX, float fY, float fZ, float fYaw, float fPitch)
	{
		RenderTest_GameplayState::s_fPhotoOffsetX = fX;
		RenderTest_GameplayState::s_fPhotoOffsetY = fY;
		RenderTest_GameplayState::s_fPhotoOffsetZ = fZ;
		RenderTest_GameplayState::s_fPhotoYaw = fYaw;
		RenderTest_GameplayState::s_fPhotoPitch = fPitch;
	}

	void Human_CrossFade(const char* szState, float fDuration)
	{
		if (Flux_AnimationStateMachine* pxSM = Human_BaseSM())
		{
			pxSM->CrossFade(szState, fDuration);
			s_bHumanShowcaseSawAnimator = true;
		}
	}

	void Human_SetLocomotion(float fSpeed, bool bSprinting)
	{
		if (Flux_AnimationStateMachine* pxSM = Human_BaseSM())
		{
			pxSM->GetParameters().SetFloat("Speed", fSpeed);
			pxSM->GetParameters().SetBool("IsSprinting", bSprinting);
			s_bHumanShowcaseSawAnimator = true;
		}
	}

	void Setup_HumanShowcase()
	{
		s_bHumanShowcaseSawAnimator = false;

		// Full-body three-quarter view from the sunlit (+X) side: the model
		// faces +Z at spawn and the directional light favours +X, so a camera
		// at +X+Z looking back (yaw = pi + pi/4) keeps the front lit. Photo
		// mode also freezes the player component's animation parameters and ground IK.
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		s_bHumanCamOverride = HumanShowcase_HasArgPrefix("--humancam-");
		// Default is the front-3/4 lit view; overridable per-run from the CLI so a
		// capture harness can orbit the camera for multi-angle shadow proofs. When
		// overridden the figure holds idle under this camera all run (see Step).
		Human_SetCamera(
			HumanShowcase_ArgFloat("--humancam-x=",     2.1f),
			HumanShowcase_ArgFloat("--humancam-y=",     0.4f),
			HumanShowcase_ArgFloat("--humancam-z=",     2.1f),
			HumanShowcase_ArgFloat("--humancam-yaw=",   2.3562f),
			HumanShowcase_ArgFloat("--humancam-pitch=", -0.04f));

		// Cinematic lighting for the capture. The global default sun is a weak,
		// near-overhead key (dim/flat 'CG' look); pose a real warm key from the
		// front-upper-right (camera side) at high HDR radiance and drop the IBL to
		// a fill, giving form-defining shading + specular rolloff. Scoped via the
		// per-scene override (default-off everywhere else); restored in Verify.
		g_xEngine.FluxGraphics().SetSunOverride(
			Zenith_Maths::Vector3(-0.38f, -0.58f, -0.52f),
			Zenith_Maths::Vector4(1.0f, 0.96f, 0.88f, 3.2f));
		g_xEngine.IBL().SetIntensity(0.8f);
		// Pin exposure: the bright sky otherwise drives auto-exposure to stop down
		// and underexpose the figure. Lock a manual exposure for a stable, well-lit
		// portrait. Restored in Verify.
		Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled = false;
		g_xEngine.HDR().SetExposure(1.9f);
	}

	bool Step_HumanShowcase(int iFrame)
	{
		// CLI camera override: hold idle + the requested camera for the whole run
		// (return true = keep the test/scene alive so any frame can be captured).
		if (s_bHumanCamOverride)
		{
			if (iFrame == 30) Human_Mark("idle_override", iFrame);
			return true;
		}

		switch (iFrame)
		{
		case 30:
		{
			// Player OnStart has run by now. Load the clips the RenderTest
			// state machine doesn't use (the Combat set) and wrap each in a
			// transition-less showcase state the marks below CrossFade into.
			Zenith_Entity xPlayer = Human_FindPlayer();
			Flux_AnimationStateMachine* pxSM = Human_BaseSM();
			Zenith_AnimatorComponent* pxAnimator = xPlayer.TryGetComponent<Zenith_AnimatorComponent>();
			if (pxAnimator != nullptr && pxSM)
			{
				Zenith_AnimatorComponent& xAnimator = *pxAnimator;
				const std::string strDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";

				struct ShowState { const char* szState; const char* szFile; };
				const ShowState axStates[] = {
					{ "ShowAttack1", "StickFigure_Attack1" },
					{ "ShowAttack2", "StickFigure_Attack2" },
					{ "ShowAttack3", "StickFigure_Attack3" },
					{ "ShowReload",  "StickFigure_Reload"  },
					{ "ShowDeath",   "StickFigure_Death"   },
				};
				for (const ShowState& xShow : axStates)
				{
					Flux_AnimationClip* pxClip = xAnimator.AddClipFromFile(strDir + xShow.szFile + ZENITH_ANIMATION_EXT);
					if (pxClip)
					{
						Flux_AnimationState* pxState = pxSM->AddState(xShow.szState);
						pxState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClip, 1.0f));
					}
				}
				s_bHumanShowcaseSawAnimator = true;
			}
			Human_Mark("idle_front", iFrame);
			break;
		}

		case 170:
			// Face close-up while the idle keeps breathing — slightly off-axis
			// from the lit side.
			Human_SetCamera(0.40f, 1.38f, 0.72f, 2.6350f, 0.02f);
			Human_Mark("face_closeup", iFrame);
			break;

		case 290:
			Human_SetCamera(2.1f, 0.4f, 2.1f, 2.3562f, -0.04f);
			Human_SetLocomotion(2.0f, false);    // Idle -> Walk via the real SM transition
			Human_Mark("walk", iFrame);
			break;

		case 470:
			Human_SetLocomotion(5.0f, true);     // Walk -> Run
			Human_Mark("run", iFrame);
			break;

		case 620:
			Human_SetLocomotion(0.0f, false);
			Human_CrossFade("ShowAttack1", 0.10f);
			Human_Mark("attack1", iFrame);
			break;

		case 660:
			Human_CrossFade("ShowAttack2", 0.10f);
			Human_Mark("attack2", iFrame);
			break;

		case 700:
			Human_CrossFade("ShowAttack3", 0.10f);
			Human_Mark("attack3", iFrame);
			break;

		case 770:
			Human_CrossFade("ShowReload", 0.15f);
			Human_Mark("reload", iFrame);
			break;

		case 880:
			Human_CrossFade("ShowDeath", 0.15f);
			Human_Mark("death", iFrame);
			break;

		case 990:
			Human_Mark("complete", iFrame);
			break;

		default:
			break;
		}

		return iFrame < 1020;
	}

	bool Verify_HumanShowcase()
	{
		// Visual demo — pass as long as the player's animator was reachable and
		// the showcase drove it.
		RenderTest_GameplayState::s_bPhotoModeActive = false;
		g_xEngine.FluxGraphics().ClearSunOverride();
		g_xEngine.IBL().SetIntensity(1.0f);
		Zenith_GraphicsOptions::Get().m_bHDRAutoExposureEnabled = true;
		return s_bHumanShowcaseSawAnimator;
	}

	const Zenith_AutomatedTest g_xHumanShowcase = {
		"HumanShowcase",
		&Setup_HumanShowcase,
		&Step_HumanShowcase,
		&Verify_HumanShowcase,
		1200,
		true /* m_bRequiresGraphics */,
		true /* m_bManualOnly */
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xHumanShowcase);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
