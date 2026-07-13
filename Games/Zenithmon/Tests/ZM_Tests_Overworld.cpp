#include "Zenith.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_PhysicsTransformSync.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Input/Zenith_InputSimulator.h"
#include "Physics/Zenith_Physics.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Source/ZM_InputActions.h"

#include <cmath>
#include <limits>

namespace
{
	constexpr float fTEST_DT = 1.0f / 60.0f;
	constexpr float fTEST_EPSILON = 0.001f;

	bool IsFinite(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x)
			&& std::isfinite(xValue.y)
			&& std::isfinite(xValue.z);
	}

#ifdef ZENITH_INPUT_SIMULATOR
	struct InputScope
	{
		InputScope()
		{
			Zenith_InputSimulator::Enable();
			Zenith_InputSimulator::ResetAllInputState();
		}

		~InputScope()
		{
			Zenith_InputSimulator::ResetAllInputState();
			Zenith_InputSimulator::ClearFixedDt();
			Zenith_InputSimulator::Disable();
		}
	};
#endif

	struct SceneScope
	{
		Zenith_Scene m_xPreviousScene;
		Zenith_Scene m_xScene;
		Zenith_SceneData* m_pxSceneData = nullptr;

		explicit SceneScope(const char* szName)
		{
			m_xPreviousScene = g_xEngine.Scenes().GetActiveScene();
			m_xScene = g_xEngine.Scenes().LoadScene(
				szName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			m_pxSceneData = g_xEngine.Scenes().GetSceneData(m_xScene);
			if (m_xScene.IsValid())
			{
				g_xEngine.Scenes().SetActiveScene(m_xScene);
			}
		}

		~SceneScope()
		{
			if (m_xPreviousScene.IsValid())
			{
				g_xEngine.Scenes().SetActiveScene(m_xPreviousScene);
			}
			if (m_xScene.IsValid())
			{
				g_xEngine.Scenes().UnloadSceneForced(m_xScene);
			}
		}
	};

	struct PhysicsSceneScope : SceneScope
	{
		bool m_bReady = false;

		explicit PhysicsSceneScope(const char* szName)
			: SceneScope(szName)
		{
			const u_int uLiveColliders =
				g_xEngine.Scenes().QueryAllScenes<Zenith_ColliderComponent>().Count();
			ZENITH_ASSERT_EQ(uLiveColliders, 0u,
				"physics fixture requires an empty collider world before Reset");
			if (m_pxSceneData == nullptr || uLiveColliders != 0u)
			{
				return;
			}

			// Reset is safe only after the zero-live-collider check above. It prevents
			// bodies left by an earlier physics test from affecting this fixture.
			g_xEngine.Physics().Reset();
			g_xEngine.Physics().m_fTimestepAccumulator = 0.0;
			m_bReady = true;
		}
	};

	Zenith_Entity CreateBox(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xScale,
		RigidBodyType eBodyType,
		const Zenith_Maths::Quat& xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		bool bOriented = false)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPosition);
		xTransform.SetScale(xScale);
		xTransform.SetRotation(xRotation);
		xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(
			bOriented ? COLLISION_VOLUME_TYPE_OBB : COLLISION_VOLUME_TYPE_AABB,
			eBodyType);
		return xEntity;
	}

	Zenith_Entity CreateCapsule(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPosition);
		xTransform.SetScale({ 0.8f, 1.8f, 0.8f });
		xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		return xEntity;
	}

	void StepPhysics(u_int uFrames)
	{
		for (u_int uFrame = 0u; uFrame < uFrames; ++uFrame)
		{
			g_xEngine.Physics().Update(fTEST_DT);
			Zenith_SyncPhysicsTransforms();
		}
	}

	Zenith_Maths::Vector3 GetPosition(const Zenith_Entity& xEntity)
	{
		Zenith_Maths::Vector3 xPosition(0.0f);
		xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPosition);
		return xPosition;
	}
}

// -----------------------------------------------------------------------------
// Input actions (5)
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_OverworldInput, MoveMapsKeyboardAndArrowAxes)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;

	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT, true);
	Zenith_Maths::Vector2 xMove = ZM_InputActions::ReadMove();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 1.0f, fTEST_EPSILON);

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT, true);
	xMove = ZM_InputActions::ReadMove();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, -1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, -1.0f, fTEST_EPSILON);

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_UP, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_DOWN, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
	xMove = ZM_InputActions::ReadMove();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 0.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 0.0f, fTEST_EPSILON);

	const Zenith_Maths::Vector2 xResolved =
		ZM_InputActions::ResolveMove(false, true, true, false);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.x, -1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.y, -1.0f, fTEST_EPSILON);
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_OverworldInput, ConfirmUsesEnterOrSpacePressedEdge)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadConfirmPressed());
	Zenith_InputSimulator::EndTestFrame();
	ZENITH_ASSERT_FALSE(ZM_InputActions::ReadConfirmPressed(),
		"held Enter must not repeat a pressed-edge action");
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadConfirmPressed());
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_OverworldInput, CancelUsesEscapeOrBackspacePressedEdge)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadCancelPressed());
	Zenith_InputSimulator::EndTestFrame();
	ZENITH_ASSERT_FALSE(ZM_InputActions::ReadCancelPressed());
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_BACKSPACE);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadCancelPressed());
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_OverworldInput, MenuUsesMOrTabPressedEdge)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadMenuPressed());
	Zenith_InputSimulator::EndTestFrame();
	ZENITH_ASSERT_FALSE(ZM_InputActions::ReadMenuPressed());
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_TAB);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadMenuPressed());
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_OverworldInput, RunUsesEitherShiftHeldState)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadRunHeld());
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT_SHIFT, true);
	ZENITH_ASSERT_TRUE(ZM_InputActions::ReadRunHeld());
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT_SHIFT, false);
	ZENITH_ASSERT_FALSE(ZM_InputActions::ReadRunHeld());
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

// -----------------------------------------------------------------------------
// Pure player-controller logic (4)
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_OverworldController, CameraRelativeDirectionFlattensAndNormalizes)
{
	const Zenith_Maths::Vector3 xForward =
		ZM_PlayerController::BuildCameraRelativeDirection(
			{ 0.0f, 1.0f }, { 0.0f, 0.7f, 1.0f });
	ZENITH_ASSERT_NEAR_VEC3(xForward, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		fTEST_EPSILON);

	const Zenith_Maths::Vector3 xRight =
		ZM_PlayerController::BuildCameraRelativeDirection(
			{ 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
	ZENITH_ASSERT_NEAR_VEC3(xRight, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		fTEST_EPSILON);

	const Zenith_Maths::Vector3 xQuarterTurnForward =
		ZM_PlayerController::BuildCameraRelativeDirection(
			{ 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f });
	ZENITH_ASSERT_NEAR_VEC3(xQuarterTurnForward,
		Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), fTEST_EPSILON);

	const Zenith_Maths::Vector3 xDiagonal =
		ZM_PlayerController::BuildCameraRelativeDirection(
			{ 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f });
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xDiagonal), 1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xDiagonal.y, 0.0f, fTEST_EPSILON);
}

ZENITH_TEST(ZM_OverworldController, SpeedAndHorizontalVelocityPreserveVerticalState)
{
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::SelectRequestedSpeed(false, true), 0.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::SelectRequestedSpeed(true, false),
		ZM_PlayerController::fWALK_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::SelectRequestedSpeed(true, true),
		ZM_PlayerController::fRUN_SPEED, fTEST_EPSILON);

	const Zenith_Maths::Vector3 xVelocity =
		ZM_PlayerController::BuildHorizontalVelocity(
			{ 0.6f, 0.0f, 0.8f }, ZM_PlayerController::fRUN_SPEED,
			{ 9.0f, -2.5f, 9.0f });
	ZENITH_ASSERT_NEAR_VEC3(xVelocity, Zenith_Maths::Vector3(4.2f, -2.5f, 5.6f),
		fTEST_EPSILON);

	const Zenith_Maths::Vector3 xIdle =
		ZM_PlayerController::BuildHorizontalVelocity(
			Zenith_Maths::Vector3(0.0f), 0.0f, { 3.0f, 1.25f, -4.0f });
	ZENITH_ASSERT_NEAR_VEC3(xIdle, Zenith_Maths::Vector3(0.0f, 1.25f, 0.0f),
		fTEST_EPSILON);

	// A normalized ground tangent has a shorter XZ projection on a slope. The
	// drive policy restores that horizontal direction to unit length before
	// applying speed, while retaining the body's vertical velocity verbatim.
	const Zenith_Maths::Vector3 xSlopeVelocity =
		ZM_PlayerController::BuildHorizontalVelocity(
			glm::normalize(Zenith_Maths::Vector3(0.0f, 1.0f, 1.0f)),
			ZM_PlayerController::fWALK_SPEED,
			{ 0.0f, -3.25f, 0.0f });
	ZENITH_ASSERT_EQ_FLOAT(
		glm::length(Zenith_Maths::Vector2(xSlopeVelocity.x, xSlopeVelocity.z)),
		ZM_PlayerController::fWALK_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xSlopeVelocity.y, -3.25f, fTEST_EPSILON);

	const Zenith_Maths::Vector3 xDownslopeTangent = glm::normalize(
		Zenith_Maths::Vector3(0.0f, -0.5f, 0.8660254f));
	const float fExpectedAdhesion =
		(xDownslopeTangent.y / glm::length(Zenith_Maths::Vector2(
			xDownslopeTangent.x, xDownslopeTangent.z)))
		* ZM_PlayerController::fWALK_SPEED;
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateGroundedSlopeVerticalVelocity(
			xDownslopeTangent, ZM_PlayerController::fWALK_SPEED, 0.0f, true),
		fExpectedAdhesion, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateGroundedSlopeVerticalVelocity(
			xDownslopeTangent, ZM_PlayerController::fWALK_SPEED, -4.0f, true),
		-4.0f, fTEST_EPSILON,
		"downslope adhesion must preserve a stronger existing fall");
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateGroundedSlopeVerticalVelocity(
			xDownslopeTangent, ZM_PlayerController::fWALK_SPEED, 1.5f, true),
		1.5f, fTEST_EPSILON,
		"downslope adhesion must preserve an active upward step assist");
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateGroundedSlopeVerticalVelocity(
			-xDownslopeTangent, ZM_PlayerController::fWALK_SPEED, -0.75f, true),
		-0.75f, fTEST_EPSILON,
		"uphill movement must never inject upward velocity");
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateGroundedSlopeVerticalVelocity(
			xDownslopeTangent, ZM_PlayerController::fWALK_SPEED, -0.75f, false),
		-0.75f, fTEST_EPSILON,
		"airborne movement must remain gravity-owned");
}

ZENITH_TEST(ZM_OverworldController, SlopeThresholdIsInclusiveAndProjectionIsTangential)
{
	const float fThreshold = glm::cos(glm::radians(
		ZM_PlayerController::fMAX_SLOPE_DEGREES));
	const Zenith_Maths::Vector3 xBoundaryNormal = glm::normalize(
		Zenith_Maths::Vector3(std::sqrt(1.0f - fThreshold * fThreshold),
			fThreshold, 0.0f));
	ZENITH_ASSERT_TRUE(ZM_PlayerController::IsWalkableSlope(xBoundaryNormal));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsWalkableSlope(
		glm::normalize(Zenith_Maths::Vector3(xBoundaryNormal.x,
			fThreshold - 0.01f, 0.0f))));

	const Zenith_Maths::Vector3 xProjected =
		ZM_PlayerController::ProjectOntoGround(
			{ 1.0f, 0.0f, 0.0f }, xBoundaryNormal);
	ZENITH_ASSERT_EQ_FLOAT(glm::dot(xProjected, xBoundaryNormal), 0.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xProjected), 1.0f, fTEST_EPSILON);
}

ZENITH_TEST(ZM_OverworldController, StepPredicateRequiresClearWalkableLanding)
{
	const Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
	ZENITH_ASSERT_TRUE(ZM_PlayerController::IsStepCandidateValid(
		true, false, true, ZM_PlayerController::fMAX_STEP_HEIGHT, xUp));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		false, false, true, 0.2f, xUp));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		true, true, true, 0.2f, xUp));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		true, false, false, 0.2f, xUp));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		true, false, true, -0.01f, xUp));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		true, false, true, ZM_PlayerController::fMAX_STEP_HEIGHT + 0.01f, xUp));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		true, false, true, 0.2f,
		glm::normalize(Zenith_Maths::Vector3(1.0f, 0.2f, 0.0f))));
	const float fSmallStepAssist =
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, 0.05f, fTEST_DT);
	const float fMidStepAssist =
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, 0.2f, fTEST_DT);
	ZENITH_ASSERT_GT(fSmallStepAssist, 0.0f);
	ZENITH_ASSERT_GT(fMidStepAssist, fSmallStepAssist);
	ZENITH_ASSERT_LE(fMidStepAssist, ZM_PlayerController::fSTEP_ASSIST_SPEED);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, -0.1f, fTEST_DT),
		-1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, 0.2f, 0.0f),
		-1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, 0.2f, -fTEST_DT),
		-1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, 0.2f,
			std::numeric_limits<float>::quiet_NaN()),
		-1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateStepAssistVelocity(-1.0f, 0.2f,
			std::numeric_limits<float>::infinity()),
		-1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_PlayerController::CalculateStepAssistVelocity(5.0f, 0.2f, fTEST_DT),
		5.0f, fTEST_EPSILON,
		"step assist must preserve a stronger existing upward velocity");
}

// -----------------------------------------------------------------------------
// Real Jolt capsule/query behavior (5)
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_OverworldPhysics, GenericCapsuleFallsGroundsAndStaysUpright)
{
	PhysicsSceneScope xFixture("ZM_Physics_CapsuleGrounding");
	if (!xFixture.m_bReady) { return; }

	CreateBox(xFixture.m_pxSceneData, "Floor", { 0.0f, -0.25f, 0.0f },
		{ 8.0f, 0.5f, 8.0f }, RIGIDBODY_TYPE_STATIC);
	Zenith_Entity xPlayer = CreateCapsule(
		xFixture.m_pxSceneData, "Player", { 0.0f, 3.0f, 0.0f });
	Zenith_ColliderComponent& xCollider =
		xPlayer.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, false, true);

	StepPhysics(180u);
	const Zenith_Maths::Vector3 xPosition = GetPosition(xPlayer);
	const Zenith_Maths::Quat xRotation =
		g_xEngine.Physics().GetBodyRotation(xCollider.GetBodyID());
	ZENITH_ASSERT_TRUE(xCollider.HasValidBody());
	ZENITH_ASSERT_EQ((u_int)xCollider.GetCollisionVolumeType(),
		(u_int)COLLISION_VOLUME_TYPE_CAPSULE);
	ZENITH_ASSERT_LT(xPosition.y, 3.0f, "gravity must move the capsule down");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 0.9f, 0.08f,
		"scale-derived capsule feet should settle on the floor");
	ZENITH_ASSERT_EQ_FLOAT(xRotation.x, 0.0f, 0.02f);
	ZENITH_ASSERT_EQ_FLOAT(xRotation.z, 0.0f, 0.02f);
}

ZENITH_TEST(ZM_OverworldPhysics, ControllerWalkRunReleaseDrivesRealBody)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
	PhysicsSceneScope xFixture("ZM_Physics_ControllerVelocity");
	if (!xFixture.m_bReady) { return; }

	CreateBox(xFixture.m_pxSceneData, "Floor", { 0.0f, -0.25f, 0.0f },
		{ 20.0f, 0.5f, 20.0f }, RIGIDBODY_TYPE_STATIC);
	Zenith_Entity xPlayer = CreateCapsule(
		xFixture.m_pxSceneData, "Player", { 0.0f, 0.95f, 0.0f });
	Zenith_AnimatorComponent& xAnimator =
		xPlayer.AddComponent<Zenith_AnimatorComponent>();
	xAnimator.CreateStateMachine("ZM_ControllerSpeed_Test")
		->GetParameters().AddFloat("Speed", 0.0f);
	ZM_PlayerController& xController = xPlayer.AddComponent<ZM_PlayerController>();

	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "MainCamera");
	Zenith_CameraComponent& xCameraComponent =
		xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetYaw(0.0);
	xCameraComponent.SetPitch(0.0);
	Zenith_UnitTests::SetMainCameraForTest(xFixture.m_pxSceneData,
		xCamera.GetEntityID());

	xController.OnStart();
	for (u_int uFrame = 0u; uFrame < 90u; ++uFrame)
	{
		StepPhysics(1u);
		xController.OnUpdate(fTEST_DT);
	}
	ZENITH_ASSERT_TRUE(xController.IsGrounded());
	const Zenith_PhysicsBodyID xBody =
		xPlayer.GetComponent<Zenith_ColliderComponent>().GetBodyID();

	// Seed every externally visible controller output on a valid frame first.
	// Invalid frame deltas must then be literal no-ops: controller state,
	// Animator Speed, the full body velocity, and facing all remain unchanged.
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
	g_xEngine.Physics().SetLinearVelocity(xBody, { 0.0f, -1.25f, 0.0f });
	Zenith_Maths::Quat xRotationBeforeSeed;
	xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xRotationBeforeSeed);
	xController.OnUpdate(fTEST_DT);
	const float fSeedRequestedSpeed = xController.GetRequestedSpeed();
	const bool bSeedGrounded = xController.IsGrounded();
	const Zenith_Maths::Vector3 xSeedMoveDirection = xController.GetMoveDirection();
	const float fSeedAnimatorSpeed = xAnimator.GetFloat("Speed");
	const Zenith_Maths::Vector3 xSeedVelocity =
		g_xEngine.Physics().GetLinearVelocity(xBody);
	Zenith_Maths::Quat xSeedRotation;
	xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xSeedRotation);
	ZENITH_ASSERT_EQ_FLOAT(fSeedRequestedSpeed,
		ZM_PlayerController::fWALK_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_TRUE(bSeedGrounded);
	ZENITH_ASSERT_NEAR_VEC3(xSeedMoveDirection,
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(fSeedAnimatorSpeed,
		ZM_PlayerController::fWALK_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xSeedVelocity.y, -1.25f, fTEST_EPSILON,
		"finite horizontal planning must preserve vertical velocity");
	ZENITH_ASSERT_LT(std::abs(glm::dot(
		glm::normalize(xRotationBeforeSeed), glm::normalize(xSeedRotation))),
		0.9999f, "a finite frame must still rotate toward movement");

	const float afRejectedDeltaTimes[] = {
		0.0f,
		-fTEST_DT,
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity()
	};
	for (float fRejectedDeltaTime : afRejectedDeltaTimes)
	{
		xController.OnUpdate(fRejectedDeltaTime);
		const Zenith_Maths::Vector3 xRejectedVelocity =
			g_xEngine.Physics().GetLinearVelocity(xBody);
		Zenith_Maths::Quat xRejectedRotation;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xRejectedRotation);
		ZENITH_ASSERT_EQ_FLOAT(xController.GetRequestedSpeed(),
			fSeedRequestedSpeed, fTEST_EPSILON);
		ZENITH_ASSERT_EQ((u_int)xController.IsGrounded(), (u_int)bSeedGrounded);
		ZENITH_ASSERT_NEAR_VEC3(xController.GetMoveDirection(),
			xSeedMoveDirection, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xAnimator.GetFloat("Speed"),
			fSeedAnimatorSpeed, fTEST_EPSILON);
		ZENITH_ASSERT_NEAR_VEC3(xRejectedVelocity, xSeedVelocity, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRejectedRotation.w, xSeedRotation.w, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRejectedRotation.x, xSeedRotation.x, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRejectedRotation.y, xSeedRotation.y, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRejectedRotation.z, xSeedRotation.z, fTEST_EPSILON);
	}
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
	g_xEngine.Physics().SetLinearVelocity(xBody, Zenith_Maths::Vector3(0.0f));

	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
	xController.OnUpdate(fTEST_DT);
	Zenith_Maths::Vector3 xVelocity = g_xEngine.Physics().GetLinearVelocity(xBody);
	ZENITH_ASSERT_EQ_FLOAT(xController.GetRequestedSpeed(),
		ZM_PlayerController::fWALK_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xAnimator.GetFloat("Speed"),
		ZM_PlayerController::fWALK_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(glm::length(Zenith_Maths::Vector2(xVelocity.x, xVelocity.z)),
		ZM_PlayerController::fWALK_SPEED, 0.02f);
	const Zenith_Maths::Vector3 xWalkStart = GetPosition(xPlayer);
	for (u_int uFrame = 0u; uFrame < 30u; ++uFrame)
	{
		StepPhysics(1u);
		xController.OnUpdate(fTEST_DT);
	}
	const Zenith_Maths::Vector3 xWalkEnd = GetPosition(xPlayer);
	ZENITH_ASSERT_GT(xWalkEnd.z, xWalkStart.z + 1.0f);

	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
	xController.OnUpdate(fTEST_DT);
	xVelocity = g_xEngine.Physics().GetLinearVelocity(xBody);
	ZENITH_ASSERT_EQ_FLOAT(xController.GetRequestedSpeed(),
		ZM_PlayerController::fRUN_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xAnimator.GetFloat("Speed"),
		ZM_PlayerController::fRUN_SPEED, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(glm::length(Zenith_Maths::Vector2(xVelocity.x, xVelocity.z)),
		ZM_PlayerController::fRUN_SPEED, 0.02f);
	for (u_int uFrame = 0u; uFrame < 30u; ++uFrame)
	{
		StepPhysics(1u);
		xController.OnUpdate(fTEST_DT);
	}
	const Zenith_Maths::Vector3 xRunEnd = GetPosition(xPlayer);
	ZENITH_ASSERT_GT(xRunEnd.z - xWalkEnd.z,
		(xWalkEnd.z - xWalkStart.z) * 1.4f,
		"equal-duration run displacement must exceed walk displacement");

	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
	xController.OnUpdate(fTEST_DT);
	xVelocity = g_xEngine.Physics().GetLinearVelocity(xBody);
	ZENITH_ASSERT_EQ_FLOAT(xController.GetRequestedSpeed(), 0.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xAnimator.GetFloat("Speed"), 0.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.x, 0.0f, 0.02f);
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.z, 0.0f, 0.02f);
	ZENITH_ASSERT_TRUE(std::isfinite(xVelocity.y),
		"horizontal planning must preserve a finite vertical velocity");
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_OverworldPhysics, DynamicCapsuleIsBlockedByStaticWall)
{
	PhysicsSceneScope xFixture("ZM_Physics_WallBlock");
	if (!xFixture.m_bReady) { return; }

	Zenith_Entity xPlayer = CreateCapsule(
		xFixture.m_pxSceneData, "Player", { 0.0f, 0.0f, 0.0f });
	CreateBox(xFixture.m_pxSceneData, "Wall", { 0.0f, 0.0f, 2.0f },
		{ 4.0f, 4.0f, 0.5f }, RIGIDBODY_TYPE_STATIC);
	Zenith_ColliderComponent& xCollider =
		xPlayer.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);
	g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, false, true);
	g_xEngine.Physics().SetLinearVelocity(
		xCollider.GetBodyID(), { 0.0f, 0.0f, 12.0f });
	StepPhysics(60u);

	const Zenith_Maths::Vector3 xPosition = GetPosition(xPlayer);
	ZENITH_ASSERT_LT(xPosition.z, 1.8f,
		"capsule crossed the far face of the static wall");
	ZENITH_ASSERT_TRUE(IsFinite(xPosition));
}

ZENITH_TEST(ZM_OverworldPhysics, JoltRampNormalsDriveSlopeClassification)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
#endif
	PhysicsSceneScope xFixture("ZM_Physics_RampQueries");
	if (!xFixture.m_bReady) { return; }

	const Zenith_Maths::Quat xWalkableRotation = glm::angleAxis(
		glm::radians(30.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	const Zenith_Maths::Quat xSteepRotation = glm::angleAxis(
		glm::radians(60.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	CreateBox(xFixture.m_pxSceneData, "WalkableRamp", { -3.0f, 0.0f, 0.0f },
		{ 4.0f, 0.4f, 4.0f }, RIGIDBODY_TYPE_STATIC, xWalkableRotation, true);
	CreateBox(xFixture.m_pxSceneData, "SteepRamp", { 3.0f, 0.0f, 0.0f },
		{ 4.0f, 0.4f, 4.0f }, RIGIDBODY_TYPE_STATIC, xSteepRotation, true);

	const Zenith_Physics::RaycastResult xWalkableHit =
		g_xEngine.Physics().Raycast({ -3.0f, 4.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f }, 8.0f);
	const Zenith_Physics::RaycastResult xSteepHit =
		g_xEngine.Physics().Raycast({ 3.0f, 4.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f }, 8.0f);
	ZENITH_ASSERT_TRUE(xWalkableHit.m_bHit);
	ZENITH_ASSERT_TRUE(xSteepHit.m_bHit);
	ZENITH_ASSERT_TRUE(ZM_PlayerController::IsWalkableSlope(
		xWalkableHit.m_xHitNormal));
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsWalkableSlope(
		xSteepHit.m_xHitNormal));

#ifdef ZENITH_INPUT_SIMULATOR
	// Drive the same real capsule both downhill (+Z) and uphill (-Z) on the
	// walkable ramp. Each controller write must request the full horizontal
	// walk speed (not speed*cos(slope)), Jolt must keep contact in both
	// directions, and gravity/contact response must supply the height change.
	Zenith_Entity xPlayer = CreateCapsule(
		xFixture.m_pxSceneData, "RampPlayer", { -3.0f, 1.5f, 0.0f });
	ZM_PlayerController& xController = xPlayer.AddComponent<ZM_PlayerController>();
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "RampCamera");
	Zenith_CameraComponent& xCameraComponent =
		xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetYaw(0.0);
	xCameraComponent.SetPitch(0.0);
	Zenith_UnitTests::SetMainCameraForTest(xFixture.m_pxSceneData,
		xCamera.GetEntityID());
	xController.OnStart();
	const Zenith_PhysicsBodyID xBody =
		xPlayer.GetComponent<Zenith_ColliderComponent>().GetBodyID();

	const auto DriveRamp = [&](Zenith_KeyCode eKey,
		float fExpectedZSign, float fExpectedYSign)
	{
		Zenith_InputSimulator::ResetAllInputState();
		g_xEngine.Physics().TeleportBody(xBody, { -3.0f, 1.5f, 0.0f });
		for (u_int uFrame = 0u; uFrame < 90u; ++uFrame)
		{
			StepPhysics(1u);
			xController.OnUpdate(fTEST_DT);
		}
		ZENITH_ASSERT_TRUE(xController.IsGrounded());
		const Zenith_Maths::Vector3 xStart = GetPosition(xPlayer);

		Zenith_InputSimulator::SetKeyHeld(eKey, true);
		xController.OnUpdate(fTEST_DT);
		for (u_int uFrame = 0u; uFrame < 8u; ++uFrame)
		{
			ZENITH_ASSERT_TRUE(xController.IsGrounded(),
				"walkable-ramp traversal lost ground contact mid-drive");
			const Zenith_Maths::Vector3 xVelocity =
				g_xEngine.Physics().GetLinearVelocity(xBody);
			ZENITH_ASSERT_EQ_FLOAT(
				glm::length(Zenith_Maths::Vector2(xVelocity.x, xVelocity.z)),
				ZM_PlayerController::fWALK_SPEED, 0.03f);
			ZENITH_ASSERT_TRUE(IsFinite(xVelocity));
			StepPhysics(1u);
			xController.OnUpdate(fTEST_DT);
		}
		const Zenith_Maths::Vector3 xEnd = GetPosition(xPlayer);
		ZENITH_ASSERT_TRUE(xController.IsGrounded(),
			"walkable-ramp traversal lost ground contact");
		ZENITH_ASSERT_GT((xEnd.z - xStart.z) * fExpectedZSign, 0.25f);
		ZENITH_ASSERT_GT((xEnd.y - xStart.y) * fExpectedYSign, 0.08f);
		ZENITH_ASSERT_TRUE(IsFinite(xEnd));
	};

	ZENITH_ASSERT_GT(xWalkableHit.m_xHitNormal.z, 0.1f,
		"fixture expects +Z to be downhill");
	DriveRamp(ZENITH_KEY_W, 1.0f, -1.0f);
	DriveRamp(ZENITH_KEY_S, -1.0f, 1.0f);
#endif
}

ZENITH_TEST(ZM_OverworldPhysics, JoltStepQueriesAcceptLowAndRejectTallObstacle)
{
#ifdef ZENITH_INPUT_SIMULATOR
	InputScope xInput;
#endif
	PhysicsSceneScope xFixture("ZM_Physics_StepQueries");
	if (!xFixture.m_bReady) { return; }

	CreateBox(xFixture.m_pxSceneData, "LowStep", { -2.0f, 0.15f, 0.0f },
		{ 1.0f, 0.30f, 1.0f }, RIGIDBODY_TYPE_STATIC);
	CreateBox(xFixture.m_pxSceneData, "TallStep", { 2.0f, 0.40f, 0.0f },
		{ 1.0f, 0.80f, 1.0f }, RIGIDBODY_TYPE_STATIC);

	const auto ProbeStep = [](float fX)
	{
		struct StepProbe
		{
			Zenith_Physics::RaycastResult m_xLower;
			Zenith_Physics::RaycastResult m_xUpper;
			Zenith_Physics::RaycastResult m_xLanding;
		};
		return StepProbe{
			g_xEngine.Physics().Raycast({ fX, 0.10f, -1.0f },
				{ 0.0f, 0.0f, 1.0f }, 2.0f),
			g_xEngine.Physics().Raycast({ fX, 0.60f, -1.0f },
				{ 0.0f, 0.0f, 1.0f }, 2.0f),
			g_xEngine.Physics().Raycast({ fX, 1.5f, 0.0f },
				{ 0.0f, -1.0f, 0.0f }, 2.0f) };
	};

	const auto xLow = ProbeStep(-2.0f);
	const auto xTall = ProbeStep(2.0f);
	ZENITH_ASSERT_TRUE(xLow.m_xLower.m_bHit);
	ZENITH_ASSERT_FALSE(xLow.m_xUpper.m_bHit);
	ZENITH_ASSERT_TRUE(xLow.m_xLanding.m_bHit);
	ZENITH_ASSERT_TRUE(ZM_PlayerController::IsStepCandidateValid(
		xLow.m_xLower.m_bHit, xLow.m_xUpper.m_bHit,
		xLow.m_xLanding.m_bHit, xLow.m_xLanding.m_xHitPoint.y,
		xLow.m_xLanding.m_xHitNormal));

	ZENITH_ASSERT_TRUE(xTall.m_xLower.m_bHit);
	ZENITH_ASSERT_TRUE(xTall.m_xUpper.m_bHit);
	ZENITH_ASSERT_TRUE(xTall.m_xLanding.m_bHit);
	ZENITH_ASSERT_FALSE(ZM_PlayerController::IsStepCandidateValid(
		xTall.m_xLower.m_bHit, xTall.m_xUpper.m_bHit,
		xTall.m_xLanding.m_bHit, xTall.m_xLanding.m_xHitPoint.y,
		xTall.m_xLanding.m_xHitNormal));

#ifdef ZENITH_INPUT_SIMULATOR
	// Real multi-frame interaction: a low step sits in front of a capsule on a
	// walkable downslope. Frame one must generate exactly one upward assist.
	// While the ground probe still sees the ramp on frame two, downslope
	// adhesion must retain that positive Y; TryApplyStep then sees the body is
	// already rising and cannot reboost it to the original impulse.
	const Zenith_Maths::Quat xRampRotation = glm::angleAxis(
		glm::radians(10.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	CreateBox(xFixture.m_pxSceneData, "StepRamp", { 6.0f, 0.0f, 0.0f },
		{ 4.0f, 0.4f, 4.0f }, RIGIDBODY_TYPE_STATIC, xRampRotation, true);
	const float fPlayerStartZ = -0.65f;
	const float fStepFrontZ = -0.20f;
	const Zenith_Physics::RaycastResult xStartGround =
		g_xEngine.Physics().Raycast({ 6.0f, 4.0f, fPlayerStartZ },
			{ 0.0f, -1.0f, 0.0f }, 8.0f);
	const Zenith_Physics::RaycastResult xStepBase =
		g_xEngine.Physics().Raycast({ 6.0f, 4.0f, fStepFrontZ },
			{ 0.0f, -1.0f, 0.0f }, 8.0f);
	ZENITH_ASSERT_TRUE(xStartGround.m_bHit);
	ZENITH_ASSERT_TRUE(xStepBase.m_bHit);
	const float fStepTop = xStartGround.m_xHitPoint.y + 0.20f;
	const float fStepHeight = fStepTop - xStepBase.m_xHitPoint.y;
	ZENITH_ASSERT_GT(fStepHeight, 0.20f);
	ZENITH_ASSERT_LT(fStepHeight, ZM_PlayerController::fMAX_STEP_HEIGHT);
	CreateBox(xFixture.m_pxSceneData, "DownslopeLowStep",
		{ 6.0f, xStepBase.m_xHitPoint.y + fStepHeight * 0.5f, 0.05f },
		{ 1.2f, fStepHeight, 0.5f }, RIGIDBODY_TYPE_STATIC);

	Zenith_Entity xPlayer = CreateCapsule(xFixture.m_pxSceneData, "StepPlayer",
		{ 6.0f, xStartGround.m_xHitPoint.y + 0.95f, fPlayerStartZ });
	ZM_PlayerController& xController = xPlayer.AddComponent<ZM_PlayerController>();
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "StepCamera");
	Zenith_CameraComponent& xCameraComponent =
		xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetYaw(0.0);
	xCameraComponent.SetPitch(0.0);
	Zenith_UnitTests::SetMainCameraForTest(xFixture.m_pxSceneData,
		xCamera.GetEntityID());
	xController.OnStart();
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
	xController.OnUpdate(fTEST_DT);
	const Zenith_PhysicsBodyID xBody =
		xPlayer.GetComponent<Zenith_ColliderComponent>().GetBodyID();
	const float fInitialAssist =
		g_xEngine.Physics().GetLinearVelocity(xBody).y;
	ZENITH_ASSERT_TRUE(xController.IsGrounded());
	ZENITH_ASSERT_GT(fInitialAssist, 0.0f,
		"qualified real step did not produce an upward assist");
	ZENITH_ASSERT_LE(fInitialAssist, ZM_PlayerController::fSTEP_ASSIST_SPEED);

	StepPhysics(1u);
	xController.OnUpdate(fTEST_DT);
	const float fAfterOnePhysicsFrame =
		g_xEngine.Physics().GetLinearVelocity(xBody).y;
	ZENITH_ASSERT_TRUE(xController.IsGrounded(),
		"fixture must remain ground-probed while rising on the downslope");
	ZENITH_ASSERT_GT(fAfterOnePhysicsFrame, 0.0f,
		"downslope adhesion cancelled the active step assist");
	ZENITH_ASSERT_LT(fAfterOnePhysicsFrame, fInitialAssist - 0.001f,
		"step assist was reapplied instead of naturally decaying");

	xController.OnUpdate(fTEST_DT);
	ZENITH_ASSERT_EQ_FLOAT(
		g_xEngine.Physics().GetLinearVelocity(xBody).y,
		fAfterOnePhysicsFrame, fTEST_EPSILON,
		"a second controller update without physics must not reboost the assist");
#endif
}

// -----------------------------------------------------------------------------
// Follow camera (4)
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_OverworldCamera, DesiredPoseUsesFixedHeadingAndLooksAtPivot)
{
	const Zenith_Maths::Vector3 xPlayer(10.0f, 2.0f, 20.0f);
	const Zenith_Maths::Vector3 xDesired =
		ZM_FollowCamera::ComputeDesiredPosition(xPlayer, 0.0f);
	ZENITH_ASSERT_NEAR_VEC3(xDesired,
		Zenith_Maths::Vector3(10.0f,
			2.0f + ZM_FollowCamera::GetCameraHeight(),
			20.0f - ZM_FollowCamera::GetArmLength()), fTEST_EPSILON);

	const Zenith_Maths::Vector3 xPivot = xPlayer
		+ Zenith_Maths::Vector3(0.0f, ZM_FollowCamera::GetPivotHeight(), 0.0f);
	const ZM_FollowCameraPose xPose = ZM_FollowCamera::BuildLookAtPose(
		xDesired, xPivot, 1.0f, 1.0f);
	const float fExpectedPitch = std::atan2(
		xPivot.y - xDesired.y, ZM_FollowCamera::GetArmLength());
	ZENITH_ASSERT_NEAR_VEC3(xPose.m_xPosition, xDesired, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xPose.m_fYaw, 0.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xPose.m_fPitch, fExpectedPitch, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(ZM_FollowCamera::GetFOVDegrees(), 65.0f, 0.0f);
}

ZENITH_TEST(ZM_OverworldCamera, CriticalSpringConvergesWithoutOvershoot)
{
	const Zenith_Maths::Vector3 xTarget(10.0f, -2.0f, 4.0f);
	Zenith_Maths::Vector3 xPosition(0.0f);
	Zenith_Maths::Vector3 xVelocity(0.0f);
	float fPreviousDistance = glm::length(xTarget - xPosition);
	for (u_int uFrame = 0u; uFrame < 120u; ++uFrame)
	{
		xPosition = ZM_FollowCamera::StepCriticalSpring(
			xPosition, xTarget, xVelocity, fTEST_DT);
		const float fDistance = glm::length(xTarget - xPosition);
		ZENITH_ASSERT_TRUE(IsFinite(xPosition));
		ZENITH_ASSERT_TRUE(IsFinite(xVelocity));
		ZENITH_ASSERT_LE(fDistance, fPreviousDistance + 0.00001f);
		fPreviousDistance = fDistance;
	}
	ZENITH_ASSERT_NEAR_VEC3(xPosition, xTarget, 0.001f);

	Zenith_Maths::Vector3 xHalfStepPosition(0.0f);
	Zenith_Maths::Vector3 xHalfStepVelocity(0.0f);
	for (u_int uFrame = 0u; uFrame < 240u; ++uFrame)
	{
		xHalfStepPosition = ZM_FollowCamera::StepCriticalSpring(
			xHalfStepPosition, xTarget, xHalfStepVelocity, fTEST_DT * 0.5f);
	}
	ZENITH_ASSERT_NEAR_VEC3(xHalfStepPosition, xPosition, 0.001f);
	ZENITH_ASSERT_EQ_FLOAT(ZM_FollowCamera::GetSpringOmega(), 8.0f, 0.0f);
}

ZENITH_TEST(ZM_OverworldCamera, ArmClampUsesPaddingAndMinimumDistance)
{
	const float fDesired = ZM_FollowCamera::GetArmLength();
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_FollowCamera::ClampArmDistance(fDesired, false, 0.0f),
		fDesired, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_FollowCamera::ClampArmDistance(fDesired, true, 3.0f),
		3.0f - ZM_FollowCamera::GetCollisionPadding(), fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_FollowCamera::ClampArmDistance(fDesired, true, 0.2f),
		ZM_FollowCamera::GetMinimumArmLength(), fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_FollowCamera::ClampArmDistance(0.5f, true, 0.1f), 0.5f,
		fTEST_EPSILON,
		"minimum arm must not exceed a shorter desired arm");
}

ZENITH_TEST(ZM_OverworldCamera, RealOccluderPushesInThenCameraRecoversOutward)
{
	PhysicsSceneScope xFixture("ZM_Physics_CameraOcclusion");
	if (!xFixture.m_bReady) { return; }

	Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Player");
	xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition({ 0.0f, 0.0f, 0.0f });
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "FollowCamera");
	Zenith_CameraComponent& xCameraComponent =
		xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetYaw(0.0);
	ZM_FollowCamera& xFollow = xCamera.AddComponent<ZM_FollowCamera>();
	xFollow.OnStart();
	xFollow.OnLateUpdate(fTEST_DT);
	Zenith_Maths::Vector3 xFirstCameraPosition;
	xCameraComponent.GetPosition(xFirstCameraPosition);
	ZENITH_ASSERT_NEAR_VEC3(xFirstCameraPosition,
		ZM_FollowCamera::ComputeDesiredPosition({ 0.0f, 0.0f, 0.0f }, 0.0f),
		fTEST_EPSILON,
		"first valid target acquisition must snap to the desired pose");

	Zenith_Entity xWall = CreateBox(xFixture.m_pxSceneData, "CameraWall",
		{ 0.0f, 1.7f, -2.5f }, { 4.0f, 3.0f, 0.2f }, RIGIDBODY_TYPE_STATIC);
	xFollow.OnLateUpdate(fTEST_DT);

	const Zenith_Maths::Vector3 xPivot(0.0f,
		ZM_FollowCamera::GetPivotHeight(), 0.0f);
	const float fUnobstructedDistance = glm::length(
		ZM_FollowCamera::ComputeDesiredPosition({ 0.0f, 0.0f, 0.0f }, 0.0f)
		- xPivot);
	const float fConstrainedDistance = xFollow.GetCurrentArmDistance();
	ZENITH_ASSERT_TRUE(xFollow.IsCollisionConstrained());
	ZENITH_ASSERT_LT(fConstrainedDistance, fUnobstructedDistance);
	ZENITH_ASSERT_GE(fConstrainedDistance,
		ZM_FollowCamera::GetMinimumArmLength());

	xWall.DestroyImmediate();
	float fPreviousDistance = fConstrainedDistance;
	for (u_int uFrame = 0u; uFrame < 90u; ++uFrame)
	{
		xFollow.OnLateUpdate(fTEST_DT);
		ZENITH_ASSERT_GE(xFollow.GetCurrentArmDistance() + 0.0001f,
			fPreviousDistance);
		fPreviousDistance = xFollow.GetCurrentArmDistance();
	}
	ZENITH_ASSERT_FALSE(xFollow.IsCollisionConstrained());
	ZENITH_ASSERT_GT(xFollow.GetCurrentArmDistance(), fConstrainedDistance + 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xFollow.GetCurrentArmDistance(),
		fUnobstructedDistance, 0.02f);
}

// -----------------------------------------------------------------------------
// ECS serialization/reflection (2)
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_OverworldECS, ComponentsSerializeOnlyVersionOne)
{
	SceneScope xFixture("ZM_Overworld_Serialization");
	ZENITH_ASSERT_NOT_NULL(xFixture.m_pxSceneData);
	if (xFixture.m_pxSceneData == nullptr) { return; }

	Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Player");
	ZM_PlayerController& xController = xPlayer.AddComponent<ZM_PlayerController>();
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Camera");
	ZM_FollowCamera& xFollow = xCamera.AddComponent<ZM_FollowCamera>();

	Zenith_DataStream xControllerStream;
	xController.WriteToDataStream(xControllerStream);
	ZENITH_ASSERT_EQ(xControllerStream.GetCursor(), sizeof(u_int));
	xControllerStream.SetCursor(0u);
	u_int uControllerVersion = 0u;
	xControllerStream >> uControllerVersion;
	ZENITH_ASSERT_EQ(uControllerVersion, 1u);
	xControllerStream.SetCursor(0u);
	xController.ReadFromDataStream(xControllerStream);
	ZENITH_ASSERT_EQ(xControllerStream.GetCursor(), sizeof(u_int));

	Zenith_DataStream xFollowStream;
	xFollow.WriteToDataStream(xFollowStream);
	ZENITH_ASSERT_EQ(xFollowStream.GetCursor(), sizeof(u_int));
	xFollowStream.SetCursor(0u);
	u_int uFollowVersion = 0u;
	xFollowStream >> uFollowVersion;
	ZENITH_ASSERT_EQ(uFollowVersion, 1u);
	xFollowStream.SetCursor(0u);
	xFollow.ReadFromDataStream(xFollowStream);
	ZENITH_ASSERT_EQ(xFollowStream.GetCursor(), sizeof(u_int));
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), INVALID_ENTITY_ID,
		"deserialization must clear runtime target state");
}

ZENITH_TEST(ZM_OverworldECS, MetaOrdersLifecycleAndMissingDependenciesAreSafe)
{
	const Zenith_ComponentMeta* pxControllerMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("ZM_PlayerController");
	const Zenith_ComponentMeta* pxFollowMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("ZM_FollowCamera");
	ZENITH_ASSERT_NOT_NULL(pxControllerMeta);
	ZENITH_ASSERT_NOT_NULL(pxFollowMeta);
	if (pxControllerMeta != nullptr)
	{
		ZENITH_ASSERT_EQ(pxControllerMeta->m_uSerializationOrder, 102u);
		ZENITH_ASSERT_EQ(pxControllerMeta->m_uSchemaVersion, 1u);
		ZENITH_ASSERT_NOT_NULL(pxControllerMeta->m_pfnOnStart);
		ZENITH_ASSERT_NOT_NULL(pxControllerMeta->m_pfnOnUpdate);
		ZENITH_ASSERT_NOT_NULL(pxControllerMeta->m_pfnOnDisable);
		ZENITH_ASSERT_NULL(pxControllerMeta->m_pfnOnLateUpdate);
	}
	if (pxFollowMeta != nullptr)
	{
		ZENITH_ASSERT_EQ(pxFollowMeta->m_uSerializationOrder, 103u);
		ZENITH_ASSERT_EQ(pxFollowMeta->m_uSchemaVersion, 1u);
		ZENITH_ASSERT_NOT_NULL(pxFollowMeta->m_pfnOnStart);
		ZENITH_ASSERT_NULL(pxFollowMeta->m_pfnOnUpdate);
		ZENITH_ASSERT_NOT_NULL(pxFollowMeta->m_pfnOnLateUpdate);
		ZENITH_ASSERT_NOT_NULL(pxFollowMeta->m_pfnOnDestroy);
	}

	SceneScope xFixture("ZM_Overworld_MissingDependencies");
	if (xFixture.m_pxSceneData == nullptr) { return; }
	Zenith_Entity xOrphan = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "NoCameraOrTarget");
	ZM_FollowCamera& xFollow = xOrphan.AddComponent<ZM_FollowCamera>();
	xFollow.OnStart();
	xFollow.OnLateUpdate(fTEST_DT);
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), INVALID_ENTITY_ID);

	// Cached IDs are process-global and remain valid across MoveToScene. A
	// follow camera must nevertheless reject a Player that left its own scene,
	// then reacquire the same-scene replacement. Keep the generation-reuse
	// phase in this existing test so the T0 registration count remains fixed.
	SceneScope xOtherFixture("ZM_Overworld_CameraCacheForeignScene");
	ZENITH_ASSERT_NOT_NULL(xOtherFixture.m_pxSceneData);
	if (xOtherFixture.m_pxSceneData == nullptr) { return; }

	Zenith_Entity xOriginalPlayer = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Player");
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "CacheOwnerCamera");
	xCamera.AddComponent<Zenith_CameraComponent>();
	ZM_FollowCamera& xCacheFollow = xCamera.AddComponent<ZM_FollowCamera>();
	xCacheFollow.OnStart();
	xCacheFollow.OnLateUpdate(fTEST_DT);
	const Zenith_EntityID xOriginalPlayerID = xOriginalPlayer.GetEntityID();
	ZENITH_ASSERT_EQ(xCacheFollow.GetTargetEntityID(), xOriginalPlayerID);

	xOriginalPlayer.MoveToScene(xOtherFixture.m_xScene);
	ZENITH_ASSERT_EQ(xOriginalPlayer.GetEntityID(), xOriginalPlayerID,
		"cross-scene moves must preserve the generation-safe entity ID");
	ZENITH_ASSERT_EQ(
		g_xEngine.Scenes().GetSceneDataForEntity(xOriginalPlayerID),
		xOtherFixture.m_pxSceneData);

	Zenith_Entity xSameSceneReplacement = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Player");
	xCacheFollow.OnLateUpdate(fTEST_DT);
	ZENITH_ASSERT_EQ(xCacheFollow.GetTargetEntityID(),
		xSameSceneReplacement.GetEntityID(),
		"a live cached Player owned by another scene must be rejected");

	// Reuse the current target's global slot in the foreign scene. Its
	// incremented generation must not alias the stale cached ID; the camera
	// still reacquires only the Player owned by its parent scene.
	const Zenith_EntityID xStaleTargetID = xSameSceneReplacement.GetEntityID();
	xSameSceneReplacement.DestroyImmediate();
	Zenith_Entity xForeignGenerationReuse = g_xEngine.Scenes().CreateEntity(
		xOtherFixture.m_pxSceneData, "ForeignGenerationReuse");
	const Zenith_EntityID xForeignReuseID = xForeignGenerationReuse.GetEntityID();
	ZENITH_ASSERT_EQ(xForeignReuseID.m_uIndex, xStaleTargetID.m_uIndex);
	ZENITH_ASSERT_NE(xForeignReuseID.m_uGeneration,
		xStaleTargetID.m_uGeneration);

	Zenith_Entity xGenerationSafeReplacement = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Player");
	xCacheFollow.OnLateUpdate(fTEST_DT);
	ZENITH_ASSERT_EQ(xCacheFollow.GetTargetEntityID(),
		xGenerationSafeReplacement.GetEntityID(),
		"a reused foreign slot must not alias a stale cached target generation");
}
