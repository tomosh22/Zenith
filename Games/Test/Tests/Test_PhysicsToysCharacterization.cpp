#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_PhysicsToysCharacterization - characterization tests for the W1 graph
 * decomposition of the Test game's physics toys.
 *
 * Written against the C++ mega-nodes FIRST (TestSpinPlatform: constant
 * angular velocity + zeroed linear velocity; TestHookesForce: force =
 * displacement toward a target); the decomposed engine-node graphs
 * (SetAngularVelocity + SetVelocity; ReadEntityPosition + vector sub +
 * ApplyForce) must keep them green unchanged.
 *
 *   Test_PhysicsToys_Test - loads the Test scene and pins:
 *     - Spinner: angular velocity (0,2,0), linear velocity zero (anchored).
 *     - Spring: pulled toward (0,5,0) from (3,0,0) - X strictly decreases
 *       (the X axis isolates the spring force from gravity).
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"

namespace
{
	enum class ToyPhase { Boot, Settle, SampleEarly, Run, SampleLate, Done };

	ToyPhase g_eToyPhase = ToyPhase::Boot;
	int      g_iToyFrame = 0;
	float    g_fSpringEarlyX = 0.0f;
	float    g_fSpringLateX = 0.0f;
	bool     g_bSpringSampled = false;
	bool     g_bSpinnerOk = false;
	float    g_fSpinnerAngularY = 0.0f;
	float    g_fSpinnerLinearSpeed = -1.0f;

	Zenith_ColliderComponent* FindToyCollider(const char* szName)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) return nullptr;
		Zenith_Entity xEntity = pxSceneData->FindEntityByName(szName);
		return xEntity.IsValid() ? xEntity.TryGetComponent<Zenith_ColliderComponent>() : nullptr;
	}

	bool ReadEntityX(const char* szName, float& fOutX)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) return false;
		Zenith_Entity xEntity = pxSceneData->FindEntityByName(szName);
		Zenith_TransformComponent* pxTransform = xEntity.IsValid() ? xEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxTransform == nullptr) return false;
		Zenith_Maths::Vector3 xPosition;
		pxTransform->GetPosition(xPosition);
		fOutX = xPosition.x;
		return true;
	}
}

static void Setup_PhysicsToys()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eToyPhase = ToyPhase::Boot;
	g_iToyFrame = 0;
	g_fSpringEarlyX = 0.0f;
	g_fSpringLateX = 0.0f;
	g_bSpringSampled = false;
	g_bSpinnerOk = false;
	g_fSpinnerAngularY = 0.0f;
	g_fSpinnerLinearSpeed = -1.0f;
}

static bool Step_PhysicsToys(int /*iFrame*/)
{
	switch (g_eToyPhase)
	{
	case ToyPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eToyPhase = ToyPhase::Settle;
		g_iToyFrame = 0;
		return true;

	case ToyPhase::Settle:
		// Let the scene finish loading + the graphs run a few updates.
		if (++g_iToyFrame < 20) return true;
		g_eToyPhase = ToyPhase::SampleEarly;
		return true;

	case ToyPhase::SampleEarly:
		g_bSpringSampled = ReadEntityX("Spring", g_fSpringEarlyX);
		g_eToyPhase = ToyPhase::Run;
		g_iToyFrame = 0;
		return true;

	case ToyPhase::Run:
		// The spring force is weak vs the body mass (force = displacement in
		// Newtons): ~0.008 units of X in 2s, quadratic-ish - 5s gives a
		// robustly measurable pull.
		if (++g_iToyFrame < 300) return true;
		g_eToyPhase = ToyPhase::SampleLate;
		return true;

	case ToyPhase::SampleLate:
	{
		if (Zenith_ColliderComponent* pxSpinner = FindToyCollider("Spinner"))
		{
			if (pxSpinner->HasValidBody())
			{
				g_fSpinnerAngularY = g_xEngine.Physics().GetAngularVelocity(pxSpinner->GetBodyID()).y;
				g_fSpinnerLinearSpeed = glm::length(g_xEngine.Physics().GetLinearVelocity(pxSpinner->GetBodyID()));
				g_bSpinnerOk = true;
			}
		}
		float fLateX = 0.0f;
		if (g_bSpringSampled && ReadEntityX("Spring", fLateX))
		{
			g_fSpringLateX = fLateX;
		}
		else
		{
			g_bSpringSampled = false;
		}
		g_eToyPhase = ToyPhase::Done;
		return false;
	}

	case ToyPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PhysicsToys()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSpinnerOk)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsToys] Spinner entity/body missing");
		return false;
	}
	if (g_fSpinnerAngularY < 1.9f || g_fSpinnerAngularY > 2.1f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsToys] Spinner angular Y %.3f != 2.0", g_fSpinnerAngularY);
		return false;
	}
	if (g_fSpinnerLinearSpeed < 0.0f || g_fSpinnerLinearSpeed > 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsToys] Spinner drifted: |v| = %.3f", g_fSpinnerLinearSpeed);
		return false;
	}
	if (!g_bSpringSampled)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsToys] Spring entity missing");
		return false;
	}
	// Spring at (3,0,0) pulled toward (0,5,0): the X-axis component isolates
	// the spring force (gravity is pure -Y), so X must strictly shrink
	// (measured C++ baseline: ~0.05 over 5s).
	if (g_fSpringLateX > g_fSpringEarlyX - 0.02f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsToys] Spring X did not move toward target: %.3f -> %.3f",
			g_fSpringEarlyX, g_fSpringLateX);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPhysicsToysTest = {
	"Test_PhysicsToys_Test",
	&Setup_PhysicsToys,
	&Step_PhysicsToys,
	&Verify_PhysicsToys,
	/*maxFrames*/ 600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPhysicsToysTest);

#endif // ZENITH_INPUT_SIMULATOR
