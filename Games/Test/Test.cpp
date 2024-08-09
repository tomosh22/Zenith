#include "Zenith.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Material.h"
#include "Flux/Flux_Graphics.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"

static Zenith_Entity s_xPlayer;
static Zenith_Entity s_xSphere0;
static Zenith_Entity s_xSphere1;
static Zenith_Entity s_axRotatingSpheres[3];
static Zenith_Entity s_xTerrain[16][16];

static Flux_Material s_xCrystalMaterial;
static Flux_Material s_xRockMaterial;


//#TO_TODO: these need to be in a header file for tools terrain export

#define MAX_TERRAIN_HEIGHT 2048
//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 8

void LoadAssets()
{
	Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Sphere_Smooth", "C:/dev/Zenith/Games/Test/Assets/Meshes/sphereSmooth.zmsh");
	Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Particle", "C:/dev/Zenith/Games/Test/Assets/Textures/particle.ztx");

	{
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Crystal_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Crystal_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Crystal_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Crystal_Metallic");

		s_xCrystalMaterial.SetDiffuse(&xDiffuse);
		s_xCrystalMaterial.SetNormal(&xNormal);
		s_xCrystalMaterial.SetRoughness(&xRoughness);
		s_xCrystalMaterial.SetMetallic(&xMetallic);
	}

	{
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Rock_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Rock_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Rock_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Rock_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Rock_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Rock_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Rock_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Rock_Metallic");

		s_xRockMaterial.SetDiffuse(&xDiffuse);
		s_xRockMaterial.SetNormal(&xNormal);
		s_xRockMaterial.SetRoughness(&xRoughness);
		s_xRockMaterial.SetMetallic(&xMetallic);
	}

	for (uint32_t x = 0; x < 16; x++)
	{
		for (uint32_t y = 0; y < 16; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
			Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Terrain" + strSuffix, std::string("C:/dev/Zenith/Games/Test/Assets/Terrain/" + strSuffix + ".zmsh").c_str());
		}
	}

	Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Water_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/water/normal.ztx");

	Zenith_AssetHandler::AddTextureCube(Zenith_GUID(), "Cubemap",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/px.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/nx.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/py.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/ny.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/pz.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/nz.ztx"
		);
}

void Zenith_Core::Project_Startup()
{
	LoadAssets();

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	s_xPlayer.Initialise(&xScene, "Game Controller");

	Zenith_CameraComponent& xCamera = s_xPlayer.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xPos = { 0, 0, 0 };
	const float fPitch = 0;
	const float fYaw = 0;
	const float fFOV = 45;
	const float fNear = 1;
	const float fFar = 5000;
	const float fAspectRatio = 16. / 9.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xScene.SetMainCameraEntity(s_xPlayer);

	Zenith_TransformComponent& xTrans = s_xPlayer.GetComponent<Zenith_TransformComponent>();
	xTrans.SetPosition({ 100,1000,100 });
	xTrans.SetScale({ 10,10,10 });

	Zenith_ColliderComponent& xCollider = s_xPlayer.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	xTrans.m_pxRigidBody->enableGravity(true);

	Zenith_ScriptComponent& xScript = s_xPlayer.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<PlayerController_Behaviour>();

	
	

	Flux_MeshGeometry& xSphereMesh = Zenith_AssetHandler::GetMesh("Sphere_Smooth");

	s_xPlayer.AddComponent<Zenith_ModelComponent>(xSphereMesh, s_xCrystalMaterial);

	{
		s_xSphere0.Initialise(&xScene, "Sphere0");
		s_xSphere0.AddComponent<Zenith_ModelComponent>(xSphereMesh, s_xCrystalMaterial);
		Zenith_TransformComponent& xTrans = s_xSphere0.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 10,1010,10 });
		xTrans.SetScale({ 10,10,10 });

		Zenith_ScriptComponent& xScript = s_xSphere0.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<HookesLaw_Behaviour>();
		HookesLaw_Behaviour& xBehaviour = *(HookesLaw_Behaviour*)xScript.m_pxScriptBehaviour;
		xBehaviour.SetDesiredPosition({ 20.,1000,20. });

		Zenith_ColliderComponent& xCollider = s_xSphere0.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	}
	{
		s_xSphere1.Initialise(&xScene, "Sphere1");
		s_xSphere1.AddComponent<Zenith_ModelComponent>(xSphereMesh, s_xRockMaterial);
		Zenith_TransformComponent& xTrans = s_xSphere1.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ -10,1010,-10 });
		xTrans.SetScale({ 10,10,10 });

		Zenith_ScriptComponent& xScript = s_xSphere1.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<HookesLaw_Behaviour>();
		HookesLaw_Behaviour& xBehaviour = *(HookesLaw_Behaviour*)xScript.m_pxScriptBehaviour;
		xBehaviour.SetDesiredPosition({-20.,1000,-20.});

		Zenith_ColliderComponent& xCollider = s_xSphere1.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	}

	uint32_t uCount = 0;
	for(Zenith_Entity& xEntity : s_axRotatingSpheres)
	{
		xEntity.Initialise(&xScene, "Rotating Sphere");
		xEntity.AddComponent<Zenith_ModelComponent>(xSphereMesh, s_xRockMaterial);
		Zenith_TransformComponent& xTrans = xEntity.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 500 + 200 * uCount,1010,100});
		xTrans.SetScale({ 100,100,100 });

		Zenith_ScriptComponent& xScript = xEntity.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<RotationBehaviour_Behaviour>();
		RotationBehaviour_Behaviour& xBehaviour = *(RotationBehaviour_Behaviour*)xScript.m_pxScriptBehaviour;
		if(uCount % 3 == 0)
		{
			xBehaviour.SetAngularVel({ 1.,0.,0. });
		}
		else if (uCount % 3 == 1)
		{
			xBehaviour.SetAngularVel({ 0.,1.,0. });
		}
		else
		{
			xBehaviour.SetAngularVel({ 0.,0.,1. });
		}

		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
		xCollider.GetRigidBody()->enableGravity(false);

		uCount++;
	}

	//#TO_TODO: why does rp3d refuse to make colliders for the far edges? (15 not 16)
	for (uint32_t x = 0; x < 15; x++)
	{
		for (uint32_t y = 0; y < 15; y++)
		{
			std::string strMeshName = "Terrain" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainMesh = Zenith_AssetHandler::GetMesh(strMeshName);

			Zenith_Entity& xTerrain = s_xTerrain[x][y];

			xTerrain.Initialise(&xScene, strMeshName);
			Zenith_Maths::Matrix4 xWaterTransform =
				glm::translate(glm::identity<Zenith_Maths::Matrix4>(), Zenith_Maths::Vector3(x * TERRAIN_SIZE * TERRAIN_SCALE + (TERRAIN_SIZE * TERRAIN_SCALE/2), MAX_TERRAIN_HEIGHT / 2, y * TERRAIN_SIZE * TERRAIN_SCALE + (TERRAIN_SIZE * TERRAIN_SCALE / 2))) *
				Zenith_Maths::EulerRotationToMatrix4(90, {1.,0.,0.}) *
				glm::scale(glm::identity<Zenith_Maths::Matrix4>(), Zenith_Maths::Vector3(TERRAIN_SIZE * TERRAIN_SCALE/2, TERRAIN_SIZE * TERRAIN_SCALE/2, TERRAIN_SIZE * TERRAIN_SCALE/2));

			xTerrain.AddComponent<Zenith_TerrainComponent>(xTerrainMesh, s_xRockMaterial, s_xCrystalMaterial, xWaterTransform);
			Zenith_TransformComponent& xTrans = xTerrain.GetComponent<Zenith_TransformComponent>();

			Zenith_ColliderComponent& xCollider = xTerrain.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
		}
	}
}
