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
// (photo mode freezes the behaviour's per-frame writes), and the non-state
// clips (attacks/reload/death) through showcase-added states entered via
// CrossFade. Those states have no outgoing transitions, so each holds until
// the next CrossFade.
//
// m_bManualOnly: a multi-minute screenshot demo has no per-commit signal —
// run it by name: --automated-test HumanShowcase.

namespace
{
	bool s_bHumanShowcaseSawAnimator = false;

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
		if (!xPlayer.IsValid() || !xPlayer.HasComponent<Zenith_AnimatorComponent>())
		{
			return nullptr;
		}
		Flux_AnimationController& xController = xPlayer.GetComponent<Zenith_AnimatorComponent>().GetController();
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
		// mode also freezes the behaviour's animation parameters and ground IK.
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		Human_SetCamera(2.1f, 0.4f, 2.1f, 2.3562f, -0.04f);
	}

	bool Step_HumanShowcase(int iFrame)
	{
		switch (iFrame)
		{
		case 30:
		{
			// Player OnStart has run by now. Load the clips the RenderTest
			// state machine doesn't use (the Combat set) and wrap each in a
			// transition-less showcase state the marks below CrossFade into.
			Zenith_Entity xPlayer = Human_FindPlayer();
			Flux_AnimationStateMachine* pxSM = Human_BaseSM();
			if (xPlayer.IsValid() && xPlayer.HasComponent<Zenith_AnimatorComponent>() && pxSM)
			{
				Zenith_AnimatorComponent& xAnimator = xPlayer.GetComponent<Zenith_AnimatorComponent>();
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
