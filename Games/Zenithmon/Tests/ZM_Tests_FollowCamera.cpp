#include "Zenith.h"

// ============================================================================
// ZM_Tests_FollowCamera -- the unit gate for ZM_FollowCamera::ResolveTarget's
// ACQUISITION rule (category ZM_FollowCamera).
//
// ★ WHAT THIS FILE EXISTS TO PIN, AND WHY IT COULD NOT BE PINNED BEFORE.
// ResolveTarget used to acquire its subject with
// pxSceneData->FindEntityByName("Player") -- the only FindEntityByName call in
// the whole game layer. That made a bare string literal load-bearing in every
// scene ever authored: rename the player entity in ONE scene and the camera
// silently acquires nothing, ZM_GameStateManager::PollForCameraAndBeginFadeIn
// bare-returns on a camera with no target, and the warp waits on a barrier that
// has NO TIMEOUT -- a permanent black screen behind an opaque fade, with every
// existing unit still green. Acquisition is now BY COMPONENT: the unique
// ZM_PlayerController in the CAMERA'S OWN SCENE.
//
// Every case below is written so it RED-FAILS on the pre-change name lookup;
// none of them merely re-spell a constant:
//
//   1. TargetIsTheControllerAndNeverTheEntityNamedPlayer
//        -- an entity literally NAMED "Player" that carries no controller must
//           NOT be acquired (the old code returned exactly that entity), and a
//           controller named something else MUST be (the old code could not see
//           it at all). Both halves red before the change.
//   2. TwoControllersInOneSceneYieldNoTargetRatherThanAGuess
//        -- ambiguity is a refusal, not a coin toss; then the survivor of a
//           destroyed pair resolves (that half is red before the change, since
//           neither entity is named "Player").
//   3. AControllerOwnedByAnotherSceneIsNeverAcquired
//        -- the scope guard. The foreign scene is deliberately the ACTIVE one,
//           so a later "simplification" to QueryActiveScene reds here, and the
//           second phase (a controller in EACH scene, one target expected) reds
//           on an unfiltered QueryAllScenes too.
//
// These are PURE boot units: no physics, no baked asset, no scene file. They
// never call OnLateUpdate, which raycasts -- target identity is fully
// observable through GetTargetEntityID(), and keeping the raycast out keeps the
// gate hermetic and independent of what an earlier unit left in the physics
// world. The camera-arm / occlusion behaviour is covered by the ZM_OverworldCamera
// units in ZM_Tests_Overworld.cpp, which own the live-physics fixture.
//
// SceneScope is a deliberate FILE-LOCAL COPY of the fixture in
// ZM_Tests_Overworld.cpp / ZM_Tests_TrainerSightProbe.cpp: those live in their
// files' anonymous namespaces and are not reachable from here.
// ============================================================================

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_TestFramework.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"

namespace
{
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

	// A player as the scenes actually author one, minus the collider: the
	// component IS the identity now, so the fixture deliberately gives it nothing
	// else that could be mistaken for the thing being matched on.
	Zenith_Entity CreatePlayerCarryingController(
		Zenith_SceneData* pxSceneData, const char* szName)
	{
		Zenith_Entity xPlayer =
			g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		xPlayer.AddComponent<ZM_PlayerController>();
		return xPlayer;
	}

	// The camera exactly as every scene authors it: a Camera component plus the
	// follow component. OnStart is the caller's business -- each case below drives
	// it explicitly, because WHEN it runs is part of what is being asserted.
	Zenith_Entity CreateFollowCamera(
		Zenith_SceneData* pxSceneData, const char* szName)
	{
		Zenith_Entity xCamera =
			g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_CameraComponent& xCameraComponent =
			xCamera.AddComponent<Zenith_CameraComponent>();
		xCameraComponent.SetYaw(0.0);
		xCameraComponent.SetPitch(0.0);
		xCamera.AddComponent<ZM_FollowCamera>();
		return xCamera;
	}
}

// -----------------------------------------------------------------------------
// 1. THE NAME HAS NO POWER; THE COMPONENT HAS ALL OF IT.
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_FollowCamera, TargetIsTheControllerAndNeverTheEntityNamedPlayer)
{
	SceneScope xFixture("ZM_FollowCamera_ByComponent");
	ZENITH_ASSERT_NOT_NULL(xFixture.m_pxSceneData);
	if (xFixture.m_pxSceneData == nullptr) { return; }

	// Created FIRST and named EXACTLY what the retired lookup asked for, so the
	// pre-change FindEntityByName("Player") would have returned this entity.
	Zenith_Entity xDecoy = g_xEngine.Scenes().CreateEntity(
		xFixture.m_pxSceneData, "Player");
	ZENITH_ASSERT_TRUE(xDecoy.IsValid());

	Zenith_Entity xCamera = CreateFollowCamera(
		xFixture.m_pxSceneData, "FollowCamera");
	ZM_FollowCamera& xFollow = xCamera.GetComponent<ZM_FollowCamera>();

	// PHASE A -- a scene whose only "Player" is a name. There is nothing here for
	// a follow camera to follow, and it must say so rather than lock onto a
	// prop/spawn marker/blockout that happens to carry the name.
	xFollow.OnStart();
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), INVALID_ENTITY_ID,
		"an entity named 'Player' with no ZM_PlayerController must NOT be acquired "
		"-- the name is not the contract any more, the component is");

	// PHASE B -- the real subject, named the scene-unique way a tidy-minded author
	// would name it. Before the change this entity was invisible to the camera and
	// the decoy above was the target; both facts are what shipped the no-timeout
	// black screen risk.
	Zenith_Entity xAvatar = CreatePlayerCarryingController(
		xFixture.m_pxSceneData, "Route1Player");
	ZENITH_ASSERT_TRUE(xAvatar.IsValid());

	xFollow.OnStart();
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), xAvatar.GetEntityID(),
		"the unique ZM_PlayerController in this scene must be the target no matter "
		"what its entity is called");
	ZENITH_ASSERT_NE(xFollow.GetTargetEntityID(), xDecoy.GetEntityID(),
		"the entity named 'Player' must lose to the entity carrying the component");
}

// -----------------------------------------------------------------------------
// 2. AMBIGUITY IS A REFUSAL, NOT A COIN TOSS.
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_FollowCamera, TwoControllersInOneSceneYieldNoTargetRatherThanAGuess)
{
	SceneScope xFixture("ZM_FollowCamera_Ambiguous");
	ZENITH_ASSERT_NOT_NULL(xFixture.m_pxSceneData);
	if (xFixture.m_pxSceneData == nullptr) { return; }

	// ★ THE AMBIGUITY IS INTRODUCED BEFORE THE CAMERA EVER RESOLVES, AND THAT
	// ORDERING IS DELIBERATE. ResolveTarget's cached-target fast path returns a
	// still-live, still-owned target WITHOUT re-counting, so a second controller
	// appearing later legitimately does not unseat an established target. What
	// must never happen is a FRESH acquisition silently picking one of several.
	Zenith_Entity xFirstPlayer = CreatePlayerCarryingController(
		xFixture.m_pxSceneData, "PlayerOne");
	Zenith_Entity xSecondPlayer = CreatePlayerCarryingController(
		xFixture.m_pxSceneData, "PlayerTwo");
	ZENITH_ASSERT_TRUE(xFirstPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xSecondPlayer.IsValid());

	Zenith_Entity xCamera = CreateFollowCamera(
		xFixture.m_pxSceneData, "AmbiguousFollowCamera");
	ZM_FollowCamera& xFollow = xCamera.GetComponent<ZM_FollowCamera>();

	xFollow.OnStart();
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), INVALID_ENTITY_ID,
		"two players in one scene is an authoring defect; following an arbitrary "
		"one of them would hide it");

	// Resolve the ambiguity and the camera acquires the survivor. This half is the
	// anti-vacuity clause: without it, a ResolveTarget that never acquired ANYTHING
	// would satisfy the assertion above.
	xSecondPlayer.DestroyImmediate();
	xFollow.OnStart();
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), xFirstPlayer.GetEntityID(),
		"once exactly one controller remains it must be acquired");
}

// -----------------------------------------------------------------------------
// 3. THE SCOPE GUARD -- THIS CAMERA'S SCENE, NOT THE ACTIVE ONE, NOT ALL OF THEM.
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_FollowCamera, AControllerOwnedByAnotherSceneIsNeverAcquired)
{
	SceneScope xCameraFixture("ZM_FollowCamera_OwnScene");
	ZENITH_ASSERT_NOT_NULL(xCameraFixture.m_pxSceneData);
	if (xCameraFixture.m_pxSceneData == nullptr) { return; }

	// Loaded SECOND, so it is the ACTIVE scene for the whole test -- which is what
	// gives phase A its teeth. S5 really does load Battle additively and make it
	// active while an overworld follow camera is still alive, so "active scene" and
	// "this camera's scene" are routinely different things.
	SceneScope xForeignFixture("ZM_FollowCamera_ForeignScene");
	ZENITH_ASSERT_NOT_NULL(xForeignFixture.m_pxSceneData);
	if (xForeignFixture.m_pxSceneData == nullptr) { return; }

	Zenith_Entity xForeignPlayer = CreatePlayerCarryingController(
		xForeignFixture.m_pxSceneData, "ForeignPlayer");
	ZENITH_ASSERT_TRUE(xForeignPlayer.IsValid());
	ZENITH_ASSERT_EQ(
		g_xEngine.Scenes().GetSceneDataForEntity(xForeignPlayer.GetEntityID()),
		xForeignFixture.m_pxSceneData,
		"fixture precondition: the foreign player must be owned by the foreign scene");

	Zenith_Entity xCamera = CreateFollowCamera(
		xCameraFixture.m_pxSceneData, "ScopedFollowCamera");
	ZM_FollowCamera& xFollow = xCamera.GetComponent<ZM_FollowCamera>();

	// PHASE A -- the only controller in the process belongs to the ACTIVE scene,
	// and this camera does not live there. QueryActiveScene would acquire it.
	xFollow.OnStart();
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), INVALID_ENTITY_ID,
		"a follow camera must not adopt a player from a scene it does not belong to, "
		"however active that scene is");

	// PHASE B -- one controller in EACH scene. An unfiltered cross-scene query
	// would count two and refuse; the active-scene query would still take the
	// foreign one. Only the camera's OWN scene gives the answer asserted here.
	Zenith_Entity xOwnPlayer = CreatePlayerCarryingController(
		xCameraFixture.m_pxSceneData, "PlayerInCameraScene");
	ZENITH_ASSERT_TRUE(xOwnPlayer.IsValid());

	xFollow.OnStart();
	ZENITH_ASSERT_EQ(xFollow.GetTargetEntityID(), xOwnPlayer.GetEntityID(),
		"the camera must follow the controller its own scene owns, while another "
		"loaded scene holds one of its own");
	ZENITH_ASSERT_NE(xFollow.GetTargetEntityID(), xForeignPlayer.GetEntityID());
}
