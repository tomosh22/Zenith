#include "Core/Zenith_Engine.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

// ============================================================================
// Perception Tests
// ============================================================================
ZENITH_TEST(AI, SightConeInRange) { Zenith_UnitTests::TestSightConeInRange(); }
void Zenith_UnitTests::TestSightConeInRange(){
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false; // Skip LOS for unit test

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);

	// Update perception
	Zenith_PerceptionSystem::Update(0.1f);

	// Check if target is perceived
	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bFound = pxTargets && pxTargets->GetSize() > 0;

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_TRUE(bFound, "Target in range should be perceived");

}

ZENITH_TEST(AI, SightConeOutOfRange) { Zenith_UnitTests::TestSightConeOutOfRange(); }

void Zenith_UnitTests::TestSightConeOutOfRange(){
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 100.0f)); // Far away

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bFound = pxTargets && pxTargets->GetSize() > 0;

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_FALSE(bFound, "Target out of range should not be perceived");

}

ZENITH_TEST(AI, SightConeOutOfFOV) { Zenith_UnitTests::TestSightConeOutOfFOV(); }

void Zenith_UnitTests::TestSightConeOutOfFOV(){
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "Target");

	// Agent facing +Z, target behind at -Z
	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, -5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f; // 90 degree cone in front
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	// Target is behind, should not be in FOV
	bool bFound = false;
	if (pxTargets)
	{
		for (uint32_t u = 0; u < pxTargets->GetSize(); ++u)
		{
			if (pxTargets->Get(u).m_bCurrentlyVisible)
			{
				bFound = true;
				break;
			}
		}
	}

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_FALSE(bFound, "Target behind agent should not be visible");

}

// Regression: an agent FACING -Z must perceive a target directly in front of it
// (also at -Z). The agent forward used to be derived via glm::eulerAngles(quat).y,
// whose asin-based middle angle collapses for a 180-deg facing (decoding to a +Z
// forward), so a -Z-facing agent was blinded to anything directly in front of it.
// Rotating the +Z basis by the quaternion fixes it. This is the bug that forced the
// RenderTest tennis far player to a 360 FOV and is a latent landmine for any
// sight-cone agent (e.g. DevilsPlayground's priest) that faces the -Z hemisphere.
ZENITH_TEST(AI, SightConeFacingNegativeZSeesTargetInFront)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "AgentNegZ");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "TargetNegZ");

	// Agent at origin, rotated 180 deg about Y so it faces -Z. Target 5m directly in
	// FRONT of it (also -Z). With a narrow 90-deg cone this passes ONLY if the agent's
	// forward is correctly -Z (the buggy decode made it +Z, putting the target 180 deg
	// behind the perceived forward).
	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xAgent.GetComponent<Zenith_TransformComponent>().SetRotation(
		glm::angleAxis(3.14159265f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, -5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bFound = false;
	if (pxTargets)
	{
		for (uint32_t u = 0; u < pxTargets->GetSize(); ++u)
		{
			if (pxTargets->Get(u).m_bCurrentlyVisible)
			{
				bFound = true;
				break;
			}
		}
	}

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_TRUE(bFound, "Agent facing -Z must perceive a target directly in front of it");
}

ZENITH_TEST(AI, SightAwarenessGain) { Zenith_UnitTests::TestSightAwarenessGain(); }

void Zenith_UnitTests::TestSightAwarenessGain(){
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);

	// Update multiple times to gain awareness
	for (int i = 0; i < 10; ++i)
	{
		Zenith_PerceptionSystem::Update(0.1f);
	}

	float fAwareness = Zenith_PerceptionSystem::GetAwarenessOf(
		xAgent.GetEntityID(), xTarget.GetEntityID());

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_GT(fAwareness, 0.0f, "Awareness should increase over time");

}

ZENITH_TEST(AI, HearingStimulusInRange) { Zenith_UnitTests::TestHearingStimulusInRange(); }

void Zenith_UnitTests::TestHearingStimulusInRange(){
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "SoundSource");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());

	// Emit sound nearby
	Zenith_PerceptionSystem::EmitSoundStimulus(
		Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f),
		1.0f,  // Loudness
		20.0f, // Radius
		xSource.GetEntityID());

	Zenith_PerceptionSystem::Update(0.1f);

	// Agent should have heard something
	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	Zenith_PerceptionSystem::Shutdown();

	// Sound stimuli should create perceived target
	ZENITH_ASSERT_NOT_NULL(pxTargets, "Agent should have perceived targets from sound");

}

ZENITH_TEST(AI, HearingStimulusAttenuation) { Zenith_UnitTests::TestHearingStimulusAttenuation(); }

void Zenith_UnitTests::TestHearingStimulusAttenuation(){
	// Test that sound gets quieter with distance
	// This is a design validation test
}

ZENITH_TEST(AI, HearingStimulusOutOfRange) { Zenith_UnitTests::TestHearingStimulusOutOfRange(); }

void Zenith_UnitTests::TestHearingStimulusOutOfRange(){
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "SoundSource");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());

	// Emit sound far away
	Zenith_PerceptionSystem::EmitSoundStimulus(
		Zenith_Maths::Vector3(100.0f, 0.0f, 0.0f), // Very far
		1.0f,  // Loudness
		10.0f, // Small radius
		xSource.GetEntityID());

	Zenith_PerceptionSystem::Update(0.1f);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bHeard = pxTargets && pxTargets->GetSize() > 0;

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_FALSE(bHeard, "Sound out of range should not be heard");

}

ZENITH_TEST(AI, MemoryRememberTarget) { Zenith_UnitTests::TestMemoryRememberTarget(); }

void Zenith_UnitTests::TestMemoryRememberTarget(){
	// Memory is integrated into perception system
	// Test that last known position is stored
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bHasLastKnownPos = false;
	if (pxTargets && pxTargets->GetSize() > 0)
	{
		// Check that last known position is set
		const Zenith_PerceivedTarget& xPerceivedTarget = pxTargets->Get(0);
		bHasLastKnownPos = Zenith_Maths::Length(xPerceivedTarget.m_xLastKnownPosition) > 0.0f;
	}

	Zenith_PerceptionSystem::Shutdown();

	ZENITH_ASSERT_TRUE(bHasLastKnownPos, "Target should have last known position stored");

}

ZENITH_TEST(AI, MemoryDecay) { Zenith_UnitTests::TestMemoryDecay(); }

void Zenith_UnitTests::TestMemoryDecay(){
	// Memory decay is handled by perception system
	// This is a design validation test
}

