#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_ShootCharacterization - characterization test for the wave-2 graph
 * conversion of the Test game's shoot action.
 *
 * Written against the C++ version FIRST (E-press inside
 * Test_PlayerControllerComponent::OnUpdate calls Shoot(): bullet ring-slot
 * spawn from the bullet prefab + launch impulse along the camera facing);
 * the graph version must keep it green unchanged.
 *
 *   Test_ShootAction_Test - constructs the player entity the way a user
 *      authors it (collider + camera + TestPlayerController + the
 *      Test_PlayerActions graph), presses E through the real input path, and
 *      asserts a bullet entity exists with the launch velocity applied.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Test/Components/Test_PlayerControllerComponent.h"

namespace
{
	enum class ShootPhase { Boot, Settle, Construct, PressE, AwaitBullet, Done };

	ShootPhase      g_eShootPhase = ShootPhase::Boot;
	int             g_iShootFrame = 0;
	Zenith_EntityID g_xPlayer;
	bool            g_bBulletSeen = false;
	float           g_fBulletSpeed = 0.0f;
}

static void Setup_ShootAction()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eShootPhase = ShootPhase::Boot;
	g_iShootFrame = 0;
	g_xPlayer = Zenith_EntityID();
	g_bBulletSeen = false;
	g_fBulletSpeed = 0.0f;
}

static bool Step_ShootAction(int /*iFrame*/)
{
	switch (g_eShootPhase)
	{
	case ShootPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eShootPhase = ShootPhase::Settle;
		g_iShootFrame = 0;
		return true;

	case ShootPhase::Settle:
		if (++g_iShootFrame < 10) return true;
		g_eShootPhase = ShootPhase::Construct;
		return true;

	case ShootPhase::Construct:
	{
		// Build the player exactly as a user authors it in the editor:
		// collider (the component's constructor requires it), camera (the
		// shoot direction source), the controller component, and its actions
		// graph. The graph slot is attached unconditionally - before the
		// conversion the asset doesn't exist and the slot just stays
		// unresolved (the C++ path drives); after it, the graph drives.
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) return false;

		// The committed Bullet.zprfb is a stale v1 prefab the current loader
		// rejects, so build a fresh bullet fixture the way the games' resource
		// inits do (CreateFromEntity -> SaveToFile -> registry resolve).
		Zenith_Entity xBulletTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "BulletTemplate");
		xBulletTemplate.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
		Zenith_Prefab* pxBulletPrefab = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxBulletPrefab->CreateFromEntity(xBulletTemplate, "Bullet");
		const std::string strBulletPath = GAME_ASSETS_DIR "Prefabs/TestShootBullet" ZENITH_PREFAB_EXT;
		pxBulletPrefab->SaveToFile(strBulletPath);
		xBulletTemplate.Destroy();

		Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "ShootTestPlayer");
		xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 30.0f, 0.0f));
		Zenith_ColliderComponent& xCollider = xPlayer.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCapsuleCollider(0.5f, 1.0f, RIGIDBODY_TYPE_DYNAMIC);
		xPlayer.AddComponent<Zenith_CameraComponent>();
		Test_PlayerControllerComponent& xController = xPlayer.AddComponent<Test_PlayerControllerComponent>();
		xController.SetBulletPrefabPath(strBulletPath);
		xPlayer.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Test_PlayerActions.bgraph");
		g_xPlayer = xPlayer.GetEntityID();

		g_iShootFrame = 0;
		g_eShootPhase = ShootPhase::PressE;
		return true;
	}

	case ShootPhase::PressE:
		// Give the component a frame to awaken via the wave drain, then fire
		// the shoot input through the real path.
		if (++g_iShootFrame < 5) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
		g_iShootFrame = 0;
		g_eShootPhase = ShootPhase::AwaitBullet;
		return true;

	case ShootPhase::AwaitBullet:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) return false;
		Zenith_Entity xBullet = pxSceneData->FindEntityByName("Bullet0");
		Zenith_ColliderComponent* pxBulletCollider = xBullet.IsValid() ? xBullet.TryGetComponent<Zenith_ColliderComponent>() : nullptr;
		if (pxBulletCollider != nullptr)
		{
			Zenith_ColliderComponent& xCollider = *pxBulletCollider;
			if (xCollider.HasValidBody())
			{
				g_bBulletSeen = true;
				g_fBulletSpeed = glm::length(g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID()));
				g_eShootPhase = ShootPhase::Done;
				return false;
			}
		}
		// Retry the press if the first one raced the component's awake.
		if (++g_iShootFrame % 30 == 0)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
		}
		return g_iShootFrame < 300;
	}

	case ShootPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ShootAction()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bBulletSeen)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ShootAction] no bullet entity appeared after E-press");
		return false;
	}
	// Shoot() launches at 50 u/s along the camera facing; gravity may have
	// bent it slightly by the frame we sample.
	if (g_fBulletSpeed < 25.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ShootAction] bullet speed %.1f too low - launch impulse missing", g_fBulletSpeed);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xShootActionTest = {
	"Test_ShootAction_Test",
	&Setup_ShootAction,
	&Step_ShootAction,
	&Verify_ShootAction,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xShootActionTest);

#endif // ZENITH_INPUT_SIMULATOR
