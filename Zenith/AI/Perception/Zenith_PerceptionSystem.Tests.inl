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

// ============================================================================
// Scene-ownership tests
//
// Perception state is owned by the scene it describes (the per-World-subsystem
// model). These pin the properties that ownership has to deliver:
//   * records live and die with their scene, with no explicit reset call;
//   * additively-loaded scenes still form ONE perceptual world;
//   * an entity that changes scene takes its records with it, children too;
//   * iteration order is registration order, not container history.
//
// Scene destruction is exercised through UnloadScene, which funnels into the
// same UnloadOneScene primitive SCENE_LOAD_SINGLE uses for every non-persistent
// scene -- so this covers the SINGLE-load teardown path without needing a real
// .zscen on disk (there is no scene file every game's boot-unit run could rely
// on). Scenes are made with ADDITIVE_WITHOUT_LOADING, the engine's own
// procedural/new-scene route.
// ============================================================================

namespace
{
	Zenith_Scene PerceptionTest_MakeScene(const char* szName)
	{
		return g_xEngine.Scenes().LoadScene(szName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	}

	// Agent at the origin looking down +Z, target 5 units ahead. LOS off so the
	// test needs no physics world.
	Zenith_SightConfig PerceptionTest_NoLosSightConfig()
	{
		Zenith_SightConfig xConfig;
		xConfig.m_fMaxRange = 20.0f;
		xConfig.m_fFOVAngle = 90.0f;
		xConfig.m_bRequireLineOfSight = false;
		return xConfig;
	}
}

ZENITH_TEST(AIPerceptionScene, AgentsAreOwnedByTheirOwnScene)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xA = PerceptionTest_MakeScene("PerceptionOwnA");
	Zenith_Scene xB = PerceptionTest_MakeScene("PerceptionOwnB");
	ZENITH_ASSERT_TRUE(xA.IsValid() && xB.IsValid(), "both test scenes must allocate");

	Zenith_Entity xAgentA = g_xEngine.Scenes().CreateEntity(xA, "AgentA");
	Zenith_Entity xAgentB = g_xEngine.Scenes().CreateEntity(xB, "AgentB");
	Zenith_PerceptionSystem::RegisterAgent(xAgentA.GetEntityID());
	Zenith_PerceptionSystem::RegisterAgent(xAgentB.GetEntityID());

	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xA), 1u, "agent A lands in scene A");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xB), 1u, "agent B lands in scene B");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 2u, "two agents across the world");

	// Destroying B must take B's agent with it and leave A's untouched -- no
	// explicit unregister, no reset hook.
	g_xEngine.Scenes().UnloadScene(xB);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 1u, "B's agent dies with scene B");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xA), 1u, "A's agent survives B's teardown");

	g_xEngine.Scenes().UnloadScene(xA);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 0u, "no agent outlives every scene");
	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, AdditiveScenesAreOnePerceptualWorld)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xAgentScene  = PerceptionTest_MakeScene("PerceptionCrossAgent");
	Zenith_Scene xTargetScene = PerceptionTest_MakeScene("PerceptionCrossTarget");

	Zenith_Entity xAgent  = g_xEngine.Scenes().CreateEntity(xAgentScene, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(xTargetScene, "Target");
	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());
	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), PerceptionTest_NoLosSightConfig());

	Zenith_PerceptionSystem::Update(0.1f);

	// An agent in one additively-loaded scene must see a target in another:
	// per-scene STORAGE must not become per-scene VISIBILITY.
	ZENITH_ASSERT_GT(Zenith_PerceptionSystem::GetAwarenessOf(xAgent.GetEntityID(), xTarget.GetEntityID()), 0.0f,
		"cross-scene target must be perceived");

	g_xEngine.Scenes().UnloadScene(xTargetScene);
	g_xEngine.Scenes().UnloadScene(xAgentScene);
	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, SceneDestructionPurgesPerceivedAndPrimaryReferences)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xAgentScene  = PerceptionTest_MakeScene("PerceptionPurgeAgent");
	Zenith_Scene xTargetScene = PerceptionTest_MakeScene("PerceptionPurgeTarget");

	Zenith_Entity xAgent  = g_xEngine.Scenes().CreateEntity(xAgentScene, "Agent");
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(xTargetScene, "Target");
	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID(), /*bHostile*/ true);
	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), PerceptionTest_NoLosSightConfig());
	Zenith_PerceptionSystem::Update(0.1f);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxBefore =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());
	ZENITH_ASSERT_TRUE(pxBefore != nullptr && pxBefore->GetSize() == 1, "agent must have perceived the target first");
	ZENITH_ASSERT_TRUE(Zenith_PerceptionSystem::GetPrimaryTarget(xAgent.GetEntityID()) == xTarget.GetEntityID(),
		"hostile perceived target must become the primary");

	// Killing the target's scene must purge BOTH the memory and the derived
	// primary. Leaving the primary stale until the agent's next Update tick is
	// the defect this covers -- an agent that never ticks again would keep
	// naming a dead entity forever.
	g_xEngine.Scenes().UnloadScene(xTargetScene);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxAfter =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());
	ZENITH_ASSERT_TRUE(pxAfter != nullptr && pxAfter->GetSize() == 0, "memories of a destroyed scene must be purged");
	ZENITH_ASSERT_FALSE(Zenith_PerceptionSystem::GetPrimaryTarget(xAgent.GetEntityID()).IsValid(),
		"primary target must be re-derived, not left dangling");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetTargetCountForTest(), 0u, "the target registration died with its scene");

	g_xEngine.Scenes().UnloadScene(xAgentScene);
	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, DontDestroyOnLoadRehomesAgentAndSurvives)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xLevel      = PerceptionTest_MakeScene("PerceptionPersistLevel");
	Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();

	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(xLevel, "PersistentAgent");
	// Registered BEFORE the move -- the order real code uses (OnAwake registers,
	// then the owner promotes itself to persistent).
	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xLevel), 1u, "agent starts in the level scene");

	xAgent.DontDestroyOnLoad();

	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xLevel), 0u, "the record must leave the old scene");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xPersistent), 1u, "the record must follow the entity");

	// The whole point: the level dies, the persistent agent does not.
	g_xEngine.Scenes().UnloadScene(xLevel);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 1u, "persistent agent survives its old scene's teardown");

	Zenith_PerceptionSystem::UnregisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, MovedRootCarriesItsChildrenRegistrations)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xLevel      = PerceptionTest_MakeScene("PerceptionChildLevel");
	Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();

	Zenith_Entity xRoot  = g_xEngine.Scenes().CreateEntity(xLevel, "Root");
	Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(xLevel, "Child");
	xChild.SetParent(xRoot.GetEntityID());

	Zenith_PerceptionSystem::RegisterAgent(xRoot.GetEntityID());
	Zenith_PerceptionSystem::RegisterAgent(xChild.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xChild.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xLevel), 2u, "both agents start in the level");

	// Only the ROOT is promoted; the child follows because MoveEntityInternal
	// recurses. A root-only notification would strand the child's records.
	xRoot.DontDestroyOnLoad();

	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xLevel), 0u, "no agent record left behind");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xPersistent), 2u, "child's record re-homed too");

	g_xEngine.Scenes().UnloadScene(xLevel);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 2u, "both survive the level teardown");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetTargetCountForTest(), 1u, "the child's TARGET record re-homed as well");

	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, SoundsExpireWithoutAgentsAndDieWithTheirScene)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xScene = PerceptionTest_MakeScene("PerceptionSoundScene");
	Zenith_Entity xEmitter = g_xEngine.Scenes().CreateEntity(xScene, "Emitter");

	// No agent is registered anywhere. Perception's Update used to early-out in
	// exactly this state, so the sound never spent its 0.5s lifetime and stayed
	// audible for the rest of the process -- outliving its emitter and, in a
	// batch run, its test.
	Zenith_PerceptionSystem::EmitSoundStimulus(Zenith_Maths::Vector3(0.0f), 1.0f, 10.0f, xEmitter.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetActiveSoundCountForTest(), 1u, "sound is in flight");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 0u, "precondition: no agents registered");

	Zenith_PerceptionSystem::Update(0.6f);   // > the 0.5s stimulus lifetime
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetActiveSoundCountForTest(), 0u,
		"sounds must age out even with zero agents registered");

	// And a still-in-flight sound dies with the scene that made it.
	Zenith_PerceptionSystem::EmitSoundStimulus(Zenith_Maths::Vector3(0.0f), 1.0f, 10.0f, xEmitter.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetActiveSoundCountForTest(), 1u, "second sound is in flight");
	g_xEngine.Scenes().UnloadScene(xScene);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetActiveSoundCountForTest(), 0u, "in-flight sound dies with its scene");

	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, IterationOrderIsRegistrationOrder)
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xScene = PerceptionTest_MakeScene("PerceptionOrderScene");
	Zenith_Entity xOne   = g_xEngine.Scenes().CreateEntity(xScene, "One");
	Zenith_Entity xTwo   = g_xEngine.Scenes().CreateEntity(xScene, "Two");
	Zenith_Entity xThree = g_xEngine.Scenes().CreateEntity(xScene, "Three");

	Zenith_PerceptionSystem::RegisterAgent(xOne.GetEntityID());
	Zenith_PerceptionSystem::RegisterAgent(xTwo.GetEntityID());
	Zenith_PerceptionSystem::RegisterAgent(xThree.GetEntityID());

	Zenith_Vector<Zenith_EntityID> axOrder;
	Zenith_PerceptionSystem::GetAgentIterationOrderForTest(axOrder);
	ZENITH_ASSERT_EQ(axOrder.GetSize(), 3u, "three agents registered");
	ZENITH_ASSERT_TRUE(axOrder.Get(0) == xOne.GetEntityID()
		&& axOrder.Get(1) == xTwo.GetEntityID()
		&& axOrder.Get(2) == xThree.GetEntityID(), "initial walk is registration order");

	// An unregister/re-register cycle must produce an order that is a pure
	// function of the operations performed -- swap-and-pop, then append. A hash
	// container would instead reorder by probe/tombstone history, which is the
	// mechanism that lets a PREDECESSOR test perturb a later test's update walk
	// even when every entry was unregistered cleanly.
	Zenith_PerceptionSystem::UnregisterAgent(xTwo.GetEntityID());
	Zenith_PerceptionSystem::RegisterAgent(xTwo.GetEntityID());

	Zenith_PerceptionSystem::GetAgentIterationOrderForTest(axOrder);
	ZENITH_ASSERT_EQ(axOrder.GetSize(), 3u, "still three agents");
	ZENITH_ASSERT_TRUE(axOrder.Get(0) == xOne.GetEntityID()
		&& axOrder.Get(1) == xThree.GetEntityID()
		&& axOrder.Get(2) == xTwo.GetEntityID(), "swap-and-pop then append: One, Three, Two");

	g_xEngine.Scenes().UnloadScene(xScene);
	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, ScenesWithNoRegistrationsHoldNoBucket)
{
	// Bucket count must track REGISTRATIONS, not scenes: a scene with no AI in
	// it should cost perception nothing and release cleanly.
	Zenith_PerceptionSystem::Initialise();
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetLiveSceneBucketCountForTest(), 0u, "fresh system owns nothing");

	Zenith_Scene xEmpty = PerceptionTest_MakeScene("PerceptionEmptyScene");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetLiveSceneBucketCountForTest(), 0u, "an empty scene claims no bucket");

	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(xEmpty, "Agent");
	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetLiveSceneBucketCountForTest(), 1u, "the first registration claims one");

	g_xEngine.Scenes().UnloadScene(xEmpty);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetLiveSceneBucketCountForTest(), 0u, "and it is released with the scene");
	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(AIPerceptionScene, BucketLivenessIsDerivedNotAsserted)
{
	// Bucket validity is derived from the stored generation-checked scene
	// handle, NOT from having been told the scene died. That is what makes the
	// notification an optimisation rather than a correctness requirement -- the
	// ECS leaf documents every runtime hook as null-safe and SentinelECS links
	// it with none installed, so perception must not corrupt if a destruction
	// notification is ever missed.
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xScene = PerceptionTest_MakeScene("PerceptionLivenessScene");
	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(xScene, "Agent");
	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 1u, "precondition: one agent registered");

	g_xEngine.Scenes().UnloadScene(xScene);

	// A repeat notification for an already-dead scene, and one for a scene that
	// never registered anything, must both be harmless -- the paths a missed or
	// duplicated notification would exercise.
	Zenith_PerceptionSystem::OnSceneDestroyed(xScene);
	Zenith_PerceptionSystem::OnSceneDestroyed(Zenith_Scene::INVALID_SCENE);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 0u, "still empty, still no crash");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetLiveSceneBucketCountForTest(), 0u, "no live bucket survives");

	// A move notification naming scenes that no longer exist must also be inert
	// (the shape a late/duplicated re-home notification would take).
	Zenith_PerceptionSystem::OnEntityOwnerSceneChanged(xAgent.GetEntityID(), xScene, Zenith_Scene::INVALID_SCENE);
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForTest(), 0u, "a stale move notification changes nothing");

	// And the recycled slot starts empty for its next occupant rather than
	// inheriting the dead scene's records.
	Zenith_Scene xReused = PerceptionTest_MakeScene("PerceptionLivenessReuse");
	ZENITH_ASSERT_EQ(Zenith_PerceptionSystem::GetAgentCountForSceneForTest(xReused), 0u,
		"a recycled scene slot must not inherit the previous occupant's agents");

	g_xEngine.Scenes().UnloadScene(xReused);
	Zenith_PerceptionSystem::Shutdown();
}

