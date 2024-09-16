#include "Zenith.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Material.h"
#include "Flux/Flux_Graphics.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"

#define TERRAIN_EXPORT_DIMS 64

static Zenith_Entity s_xPlayer;
static Zenith_Entity s_xBarrel;
static Zenith_Entity s_xSphere0;
static Zenith_Entity s_xSphere1;
static Zenith_Entity s_axRotatingSpheres[3];
static Zenith_Entity s_xTerrain[TERRAIN_EXPORT_DIMS][TERRAIN_EXPORT_DIMS];
static Zenith_Entity s_xOgre;

//#TO_TODO: these need to be in a header file for tools terrain export

#define MAX_TERRAIN_HEIGHT 2048
//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 8



void LoadAssets()
{
	Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Barrel", "C:/dev/Zenith/Games/Test/Assets/Meshes/barrel_0.zmsh");
	{
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Barrel_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Meshes/barrelDiffuse.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Barrel_Metallic", "C:/dev/Zenith/Games/Test/Assets/Meshes/barrelShininess.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Barrel_Diffuse");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Barrel_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial(Zenith_GUID(), "Barrel");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetMetallic(&xMetallic);
	}

	Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Sphere_Smooth", "C:/dev/Zenith/Games/Test/Assets/Meshes/sphereSmooth_0.zmsh");
	{
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Crystal_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Crystal_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Crystal_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Crystal_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Crystal_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial(Zenith_GUID(), "Crystal");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
	}
	{
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "MuddyGrass_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "MuddyGrass_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "MuddyGrass_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "MuddyGrass_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("MuddyGrass_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("MuddyGrass_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("MuddyGrass_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("MuddyGrass_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial(Zenith_GUID(), "MuddyGrass");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
	}
	{
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "SupplyCrate_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "SupplyCrate_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "SupplyCrate_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "SupplyCrate_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("SupplyCrate_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("SupplyCrate_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("SupplyCrate_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("SupplyCrate_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial(Zenith_GUID(), "SupplyCrate");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
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

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial(Zenith_GUID(), "Rock");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
	}

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
#if 1
			Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Terrain" + strSuffix, std::string("C:/dev/Zenith/Games/Test/Assets/Terrain/" + strSuffix + ".zmsh").c_str(), true);
#else
			Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Terrain" + strSuffix, std::string("C:/dev/Zenith/Games/Test/Assets/Terrain/" + strSuffix + ".zmsh").c_str(), false);
#endif
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
	xTrans.m_pxRigidBody->enableGravity(false);

	Zenith_ScriptComponent& xScript = s_xPlayer.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<PlayerController_Behaviour>();

	Zenith_TextComponent& xText = s_xPlayer.AddComponent<Zenith_TextComponent>();
	TextEntry xTextEntry = { "abcdefghijklmnopqrstuvwxyz", { 0, 0 }, 1. };
	xText.AddText(xTextEntry);

	Flux_MeshGeometry& xSphereMesh = Zenith_AssetHandler::GetMesh("Sphere_Smooth");

	Zenith_ModelComponent& xModel = s_xPlayer.AddComponent<Zenith_ModelComponent>();
	xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("Crystal"));

	{
		s_xSphere0.Initialise(&xScene, "Sphere0");
		Zenith_ModelComponent& xModel = s_xSphere0.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("Crystal"));
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
		Zenith_ModelComponent& xModel = s_xSphere1.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("Rock"));
		Zenith_TransformComponent& xTrans = s_xSphere1.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ -10,1010,-10 });
		xTrans.SetScale({ 10,10,10 });

		Zenith_ScriptComponent& xScript = s_xSphere1.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<HookesLaw_Behaviour>();
		HookesLaw_Behaviour& xBehaviour = *(HookesLaw_Behaviour*)xScript.m_pxScriptBehaviour;
		xBehaviour.SetDesiredPosition({ -20.,1000,-20. });

		Zenith_ColliderComponent& xCollider = s_xSphere1.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	}

	Flux_MeshGeometry& xBarrelMesh = Zenith_AssetHandler::GetMesh("Barrel");
	{
		s_xBarrel.Initialise(&xScene, "Barrel");
		Zenith_ModelComponent& xModel = s_xBarrel.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(xBarrelMesh, Zenith_AssetHandler::GetMaterial("Barrel"));
		Zenith_TransformComponent& xTrans = s_xBarrel.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 1500,1200,100 });
		xTrans.SetScale({ 10,10,10 });
	}

	uint32_t uCount = 0;
	for (Zenith_Entity& xEntity : s_axRotatingSpheres)
	{
		xEntity.Initialise(&xScene, "Rotating Sphere");
		Zenith_TransformComponent& xTrans = xEntity.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 500 + 200 * uCount,1200,100 });
		xTrans.SetScale({ 100,100,100 });

		Zenith_ScriptComponent& xScript = xEntity.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<RotationBehaviour_Behaviour>();
		RotationBehaviour_Behaviour& xBehaviour = *(RotationBehaviour_Behaviour*)xScript.m_pxScriptBehaviour;
		if (uCount % 3 == 0)
		{
			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("Rock"));
			xBehaviour.SetAngularVel({ 1.,0.,0. });
		}
		else if (uCount % 3 == 1)
		{
			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("MuddyGrass"));
			xBehaviour.SetAngularVel({ 0.,1.,0. });
		}
		else
		{
			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("SupplyCrate"));
			xBehaviour.SetAngularVel({ 0.,0.,1. });
		}

		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
		xCollider.GetRigidBody()->enableGravity(false);

		uCount++;
	}

	//#TO_TODO: why does rp3d refuse to make colliders for the far edges? (TERRAIN_EXPORT_DIMS - 1 not TERRAIN_EXPORT_DIMS)
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strMeshName = "Terrain" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainMesh = Zenith_AssetHandler::GetMesh(strMeshName);

			Zenith_Entity& xTerrain = s_xTerrain[x][y];

			xTerrain.Initialise(&xScene, strMeshName);
			Zenith_Maths::Matrix4 xWaterTransform =
				glm::translate(glm::identity<Zenith_Maths::Matrix4>(), Zenith_Maths::Vector3(x * TERRAIN_SIZE * TERRAIN_SCALE + (TERRAIN_SIZE * TERRAIN_SCALE / 2), MAX_TERRAIN_HEIGHT / 2, y * TERRAIN_SIZE * TERRAIN_SCALE + (TERRAIN_SIZE * TERRAIN_SCALE / 2))) *
				Zenith_Maths::EulerRotationToMatrix4(90, { 1.,0.,0. }) *
				glm::scale(glm::identity<Zenith_Maths::Matrix4>(), Zenith_Maths::Vector3(TERRAIN_SIZE * TERRAIN_SCALE / 2, TERRAIN_SIZE * TERRAIN_SCALE / 2, TERRAIN_SIZE * TERRAIN_SCALE / 2));

			xTerrain.AddComponent<Zenith_TerrainComponent>(xTerrainMesh, Zenith_AssetHandler::GetMaterial("Rock"), Zenith_AssetHandler::GetMaterial("Crystal"), xWaterTransform, Zenith_Maths::Vector2(x * TERRAIN_SIZE * TERRAIN_SCALE, y * TERRAIN_SIZE * TERRAIN_SCALE));

			{
				Zenith_TextComponent& xText = xTerrain.AddComponent<Zenith_TextComponent>();
				TextEntry_World xTextEntry = { std::to_string(x * TERRAIN_SIZE * TERRAIN_SCALE) + " " + std::to_string(MAX_TERRAIN_HEIGHT / 2) + " " + std::to_string(y * TERRAIN_SIZE * TERRAIN_SCALE), {x * TERRAIN_SIZE * TERRAIN_SCALE, MAX_TERRAIN_HEIGHT / 2, y * TERRAIN_SIZE * TERRAIN_SCALE}, 1. };
				xText.AddText_World(xTextEntry);
			}


#if 0
			Zenith_ColliderComponent& xCollider = xTerrain.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
#endif
		}
	}

	{
		s_xOgre.Initialise(&xScene, "Ogre");
		Zenith_ModelComponent& xModel = s_xOgre.AddComponent<Zenith_ModelComponent>();
		xModel.LoadMeshesFromDir("C:/dev/Zenith/Games/Test/Assets/Meshes/ogre");
	}
}