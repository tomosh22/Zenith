#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"

class RenderTest_PlayerBehaviour : public Zenith_ScriptBehaviour
{
public:
	RenderTest_PlayerBehaviour(Zenith_Entity& xEntity)
		: Zenith_ScriptBehaviour()
	{
		m_xParentEntity = xEntity;
	}

	ZENITH_BEHAVIOUR_TYPE_NAME(RenderTest_PlayerBehaviour)

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
			SetupStateMachine();
		}
	}

	void OnUpdate(float fDt) override
	{
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>() ||
			!m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
		{
			return;
		}

		Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();

		if (xCollider.HasValidBody())
		{
			Zenith_Physics::EnforceUpright(xCollider.GetBodyID());
		}

		Zenith_Maths::Vector3 xInput = ReadMovementInput();
		const float fInputLen = glm::length(xInput);

		Zenith_Maths::Vector3 xMoveDir(0.0f);
		float fSpeed = 0.0f;

		if (fInputLen > 0.01f)
		{
			xMoveDir = xInput / fInputLen;
			fSpeed = m_fMoveSpeed;

			if (xCollider.HasValidBody())
			{
				Zenith_Maths::Vector3 xVelocity = xMoveDir * m_fMoveSpeed;
				xVelocity.y = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID()).y;
				Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
			}

			RotateTowards(xTransform, xMoveDir, fDt);
		}
		else if (xCollider.HasValidBody())
		{
			Zenith_Maths::Vector3 xVelocity = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
			xVelocity.x = 0.0f;
			xVelocity.z = 0.0f;
			Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}

		if (m_pxAnimator)
		{
			m_pxAnimator->SetFloat("Speed", fSpeed);
		}
	}

private:
	static constexpr const char* sk_szStateIdle = "Idle";
	static constexpr const char* sk_szStateWalk = "Walk";
	static constexpr const char* sk_szParamSpeed = "Speed";

	Zenith_Maths::Vector3 ReadMovementInput() const
	{
		Zenith_Maths::Vector3 xInput(0.0f);

		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))
			xInput.z += 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))
			xInput.z -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))
			xInput.x -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT))
			xInput.x += 1.0f;

		return xInput;
	}

	void RotateTowards(Zenith_TransformComponent& xTransform, const Zenith_Maths::Vector3& xTargetDir, float fDt) const
	{
		if (glm::length(xTargetDir) < 0.01f)
			return;

		Zenith_Maths::Quat xCurrentRot;
		xTransform.GetRotation(xCurrentRot);

		const float fTargetYaw = atan2(xTargetDir.x, xTargetDir.z);
		const Zenith_Maths::Quat xTargetRot = glm::angleAxis(fTargetYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

		const Zenith_Maths::Quat xNewRot = glm::slerp(xCurrentRot, xTargetRot, fDt * m_fRotationSpeed);
		xTransform.SetRotation(xNewRot);
	}

	void SetupStateMachine()
	{
		if (!m_pxAnimator)
			return;

		Flux_AnimationController& xController = m_pxAnimator->GetController();

		static const std::string s_strAssetDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Idle" ZENITH_ANIMATION_EXT);
		xController.AddClipFromFile(s_strAssetDir + "StickFigure_Walk" ZENITH_ANIMATION_EXT);

		Flux_AnimationStateMachine* pxSM = xController.CreateStateMachine("RenderTestStateMachine");
		Flux_AnimationClipCollection& xClips = xController.GetClipCollection();

		pxSM->GetParameters().AddFloat(sk_szParamSpeed, 0.0f);

		AddClipState(pxSM, xClips, sk_szStateIdle, "Idle");
		AddClipState(pxSM, xClips, sk_szStateWalk, "Walk");

		// Idle -> Walk on Speed > 0.1
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = sk_szStateWalk;
			xTrans.m_fTransitionDuration = 0.15f;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = sk_szParamSpeed;
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
			xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
			xCond.m_fThreshold = 0.1f;
			xTrans.m_xConditions.PushBack(xCond);

			pxSM->GetState(sk_szStateIdle)->AddTransition(xTrans);
		}

		// Walk -> Idle on Speed <= 0.1
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = sk_szStateIdle;
			xTrans.m_fTransitionDuration = 0.15f;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = sk_szParamSpeed;
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
			xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
			xCond.m_fThreshold = 0.1f;
			xTrans.m_xConditions.PushBack(xCond);

			pxSM->GetState(sk_szStateWalk)->AddTransition(xTrans);
		}

		pxSM->SetDefaultState(sk_szStateIdle);
		pxSM->ResolveClipReferences(&xClips);
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

	Zenith_AnimatorComponent* m_pxAnimator = nullptr;
	float m_fMoveSpeed = 5.0f;
	float m_fRotationSpeed = 10.0f;
};
