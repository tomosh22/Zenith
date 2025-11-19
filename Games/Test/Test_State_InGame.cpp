#include "Zenith.h"
#include "Test/Test_State_InGame.h"
#include "Test/Test_State_MainMenu.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Material.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "Input/Zenith_Input.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"


Zenith_State* Zenith_StateMachine::s_pxCurrentState = nullptr;
void Zenith_StateMachine::Project_Initialise()
{
	s_pxCurrentState = new Test_State_InGame;
}

#define TERRAIN_EXPORT_DIMS 64

static Zenith_Entity s_xPlayer;
static Zenith_Entity s_xBarrel;
static Zenith_Entity s_xSphere0;
static Zenith_Entity s_xSphere1;
static Zenith_Entity s_axRotatingSpheres[3];
static Zenith_Entity s_xTerrain[TERRAIN_EXPORT_DIMS][TERRAIN_EXPORT_DIMS];
static Zenith_Entity s_xOgre;
static Zenith_Entity s_xGltfTest[3];

//#TO_TODO: these need to be in a header file for tools terrain export

#define MAX_TERRAIN_HEIGHT 2048
//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 8

static Zenith_Maths::Vector3 s_xPlayerSpawn = { 2100,-566,1500 };

static void LoadAssets()
{
	Zenith_AssetHandler::AddMesh("Barrel", ASSETS_ROOT"Meshes/barrel_Mesh0_Mat0.zmsh");
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Meshes/barrel_Diffuse_0.ztx");
		Zenith_AssetHandler::AddTexture("Barrel_Diffuse", xDiffuseData);
		xDiffuseData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Meshes/barrel_Shininess_0.ztx");
		Zenith_AssetHandler::AddTexture("Barrel_Metallic", xMetallicData);
		xMetallicData.FreeAllocatedData();

		Flux_Texture xDiffuse = Zenith_AssetHandler::GetTexture("Barrel_Diffuse");
		Flux_Texture xMetallic = Zenith_AssetHandler::GetTexture("Barrel_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("Barrel");
		xMat.SetDiffuse(xDiffuse);
		xMat.SetRoughnessMetallic(xMetallic);
	}

	Zenith_AssetHandler::AddMesh("Capsule", ASSETS_ROOT"Meshes/capsule_Mesh0_Mat0.zmsh");
	Zenith_AssetHandler::AddMesh("Sphere_Smooth", ASSETS_ROOT"Meshes/sphereSmooth_Mesh0_Mat0.zmsh");
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture("Crystal_Diffuse", xDiffuseData);
		xDiffuseData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/normal.ztx");
		Zenith_AssetHandler::AddTexture("Crystal_Normal", xNormalData);
		xNormalData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture("Crystal_Roughness", xRoughnessData);
		xRoughnessData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/metallic.ztx");
		Zenith_AssetHandler::AddTexture("Crystal_Metallic", xMetallicData);
		xMetallicData.FreeAllocatedData();

		Flux_Texture xDiffuse = Zenith_AssetHandler::GetTexture("Crystal_Diffuse");
		Flux_Texture xNormal = Zenith_AssetHandler::GetTexture("Crystal_Normal");
		Flux_Texture xRoughness = Zenith_AssetHandler::GetTexture("Crystal_Roughness");
		Flux_Texture xMetallic = Zenith_AssetHandler::GetTexture("Crystal_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("Crystal");
		xMat.SetDiffuse(xDiffuse);
		xMat.SetNormal(xNormal);
		xMat.SetRoughnessMetallic(xRoughness);
	}
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture("MuddyGrass_Diffuse", xDiffuseData);
		xDiffuseData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/normal.ztx");
		Zenith_AssetHandler::AddTexture("MuddyGrass_Normal", xNormalData);
		xNormalData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture("MuddyGrass_Roughness", xRoughnessData);
		xRoughnessData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/metallic.ztx");
		Zenith_AssetHandler::AddTexture("MuddyGrass_Metallic", xMetallicData);
		xMetallicData.FreeAllocatedData();

		Flux_Texture xDiffuse = Zenith_AssetHandler::GetTexture("MuddyGrass_Diffuse");
		Flux_Texture xNormal = Zenith_AssetHandler::GetTexture("MuddyGrass_Normal");
		Flux_Texture xRoughness = Zenith_AssetHandler::GetTexture("MuddyGrass_Roughness");
		Flux_Texture xMetallic = Zenith_AssetHandler::GetTexture("MuddyGrass_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("MuddyGrass");
		xMat.SetDiffuse(xDiffuse);
		xMat.SetNormal(xNormal);
		xMat.SetRoughnessMetallic(xRoughness);
	}
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture("SupplyCrate_Diffuse", xDiffuseData);
		xDiffuseData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/normal.ztx");
		Zenith_AssetHandler::AddTexture("SupplyCrate_Normal", xNormalData);
		xNormalData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture("SupplyCrate_Roughness", xRoughnessData);
		xRoughnessData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/metallic.ztx");
		Zenith_AssetHandler::AddTexture("SupplyCrate_Metallic", xMetallicData);
		xMetallicData.FreeAllocatedData();

		Flux_Texture xDiffuse = Zenith_AssetHandler::GetTexture("SupplyCrate_Diffuse");
		Flux_Texture xNormal = Zenith_AssetHandler::GetTexture("SupplyCrate_Normal");
		Flux_Texture xRoughness = Zenith_AssetHandler::GetTexture("SupplyCrate_Roughness");
		Flux_Texture xMetallic = Zenith_AssetHandler::GetTexture("SupplyCrate_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("SupplyCrate");
	 xMat.SetDiffuse(xDiffuse);
		xMat.SetNormal(xNormal);
	 xMat.SetRoughnessMetallic(xRoughness);
	}
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture("Rock_Diffuse", xDiffuseData);
		xDiffuseData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/normal.ztx");
		Zenith_AssetHandler::AddTexture("Rock_Normal", xNormalData);
		xNormalData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture("Rock_Roughness", xRoughnessData);
		xRoughnessData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/metallic.ztx");
		Zenith_AssetHandler::AddTexture("Rock_Metallic", xMetallicData);
		xMetallicData.FreeAllocatedData();

		Flux_Texture xDiffuse = Zenith_AssetHandler::GetTexture("Rock_Diffuse");
		Flux_Texture xNormal = Zenith_AssetHandler::GetTexture("Rock_Normal");
		Flux_Texture xRoughness = Zenith_AssetHandler::GetTexture("Rock_Roughness");
		Flux_Texture xMetallic = Zenith_AssetHandler::GetTexture("Rock_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("Rock");
		xMat.SetDiffuse(xDiffuse);
		xMat.SetNormal(xNormal);
		xMat.SetRoughnessMetallic(xRoughness);
	}

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);

			Zenith_AssetHandler::AddMesh("Terrain_Render" + strSuffix, std::string(ASSETS_ROOT"Terrain/Render_" + strSuffix + ".zmsh").c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
			Zenith_AssetHandler::AddMesh("Terrain_Physics" + strSuffix, std::string(ASSETS_ROOT"Terrain/Physics_" + strSuffix + ".zmsh").c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

			Zenith_Maths::Matrix4 xWaterTransform =
				glm::translate(glm::identity<Zenith_Maths::Matrix4>(), Zenith_Maths::Vector3(x * TERRAIN_SIZE * TERRAIN_SCALE + (TERRAIN_SIZE * TERRAIN_SCALE / 2), MAX_TERRAIN_HEIGHT / 2, y * TERRAIN_SIZE * TERRAIN_SCALE + (TERRAIN_SIZE * TERRAIN_SCALE / 2))) *
				Zenith_Maths::EulerRotationToMatrix4(90, { 1.,0.,0. }) *
				glm::scale(glm::identity<Zenith_Maths::Matrix4>(), Zenith_Maths::Vector3(TERRAIN_SIZE * TERRAIN_SCALE / 2, TERRAIN_SIZE * TERRAIN_SCALE / 2, TERRAIN_SIZE * TERRAIN_SCALE / 2));
			Flux_MeshGeometry& xWaterMesh = Zenith_AssetHandler::AddMesh("Terrain_Water" + strSuffix);
			Flux_MeshGeometry::GenerateFullscreenQuad(xWaterMesh, xWaterTransform);
			Flux_MemoryManager::InitialiseVertexBuffer(xWaterMesh.GetVertexData(), xWaterMesh.GetVertexDataSize(), xWaterMesh.GetVertexBuffer());
			Flux_MemoryManager::InitialiseIndexBuffer(xWaterMesh.GetIndexData(), xWaterMesh.GetIndexDataSize(), xWaterMesh.GetIndexBuffer());
		}
	}
}

void Test_State_InGame::OnEnter()
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
	const float fFar = 2000;
	const float fAspectRatio = 16. / 9.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xScene.SetMainCameraEntity(s_xPlayer);

	Zenith_TransformComponent& xTrans = s_xPlayer.GetComponent<Zenith_TransformComponent>();
	xTrans.SetPosition({ 2100,-566,1500 });
	xTrans.SetScale({ 2,2,2 });

	Zenith_ColliderComponent& xCollider = s_xPlayer.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);


	Zenith_ScriptComponent& xScript = s_xPlayer.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<PlayerController_Behaviour>();

	Zenith_TextComponent& xText = s_xPlayer.AddComponent<Zenith_TextComponent>();
	TextEntry xTextEntry = { "abcdefghijklmnopqrstuvwxyz", { 0, 0 }, 1. };
	xText.AddText(xTextEntry);

	Flux_MeshGeometry& xSphereMesh = Zenith_AssetHandler::GetMesh("Sphere_Smooth");

	Zenith_ModelComponent& xModel = s_xPlayer.AddComponent<Zenith_ModelComponent>();
	//xModel.AddMeshEntry(Zenith_AssetHandler::GetMesh("StickyMcStickFace"), Zenith_AssetHandler::GetMaterial("Crystal"));
	Flux_Material& xCrystalMaterial = Zenith_AssetHandler::GetMaterial("Crystal");
	xModel.LoadMeshesFromDir(ASSETS_ROOT"Meshes/stickymcstickface_anim", &xCrystalMaterial);
	for(u_int u = 0; u < xModel.GetNumMeshEntires(); u++)
	{
		xModel.GetMeshGeometryAtIndex(u).m_pxAnimation = new Flux_MeshAnimation(ASSETS_ROOT"Meshes/stickymcstickface_anim/StickyMcStickface_Anim.fbx", xModel.GetMeshGeometryAtIndex(u));
	}

	{
		s_xSphere0.Initialise(&xScene, "Sphere0");
		Zenith_ModelComponent& xModel = s_xSphere0.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("Crystal"));
		Zenith_TransformComponent& xTrans = s_xSphere0.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 1,101,1 });
		xTrans.SetScale({ 1,1,1 });

		Zenith_ScriptComponent& xScript = s_xSphere0.AddComponent<Zenith_ScriptComponent>();
	 xScript.SetBehaviour<HookesLaw_Behaviour>();
		HookesLaw_Behaviour& xBehaviour = *(HookesLaw_Behaviour*)xScript.m_pxScriptBehaviour;
	 xBehaviour.SetDesiredPosition({ 2.,100,2. });

		Zenith_ColliderComponent& xCollider = s_xSphere0.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	}
	{
		s_xSphere1.Initialise(&xScene, "Sphere1");
		Zenith_ModelComponent& xModel = s_xSphere1.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(xSphereMesh, Zenith_AssetHandler::GetMaterial("Rock"));
		Zenith_TransformComponent& xTrans = s_xSphere1.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ -1,101,-1 });
		xTrans.SetScale({ 1,1,1 });

		Zenith_ScriptComponent& xScript = s_xSphere1.AddComponent<Zenith_ScriptComponent>();
	 xScript.SetBehaviour<HookesLaw_Behaviour>();
		HookesLaw_Behaviour& xBehaviour = *(HookesLaw_Behaviour*)xScript.m_pxScriptBehaviour;
	 xBehaviour.SetDesiredPosition({ -2.,100,-2. });

		Zenith_ColliderComponent& xCollider = s_xSphere1.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	}

	Flux_MeshGeometry& xBarrelMesh = Zenith_AssetHandler::GetMesh("Barrel");
	{
		s_xBarrel.Initialise(&xScene, "Barrel");
		Zenith_ModelComponent& xModel = s_xBarrel.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(xBarrelMesh, Zenith_AssetHandler::GetMaterial("Barrel"));
		Zenith_TransformComponent& xTrans = s_xBarrel.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 150,120,10 });
		xTrans.SetScale({ 1,1,1 });
	}

	uint32_t uCount = 0;
	for (Zenith_Entity& xEntity : s_axRotatingSpheres)
	{
		xEntity.Initialise(&xScene, "Rotating Sphere");
		Zenith_TransformComponent& xTrans = xEntity.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 50 + 20 * uCount,120,10 });
		xTrans.SetScale({ 10,10,10 });

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
		Zenith_Physics::SetGravityEnabled(xCollider.GetRigidBody(), false);

		uCount++;
	}

	//#TO_TODO: why does rp3d refuse to make colliders for the far edges? (TERRAIN_EXPORT_DIMS - 1 not TERRAIN_EXPORT_DIMS)
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strRenderMeshName = "Terrain_Render" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainRenderMesh = Zenith_AssetHandler::GetMesh(strRenderMeshName);
			std::string strPhysicsMeshName = "Terrain_Physics" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainPhysicsMesh = Zenith_AssetHandler::GetMesh(strPhysicsMeshName);
			std::string strWaterMeshName = "Terrain_Water" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainWaterMesh = Zenith_AssetHandler::GetMesh(strWaterMeshName);

			Zenith_Entity& xTerrain = s_xTerrain[x][y];

			xTerrain.Initialise(&xScene, strRenderMeshName);

			xTerrain.AddComponent<Zenith_TerrainComponent>(xTerrainRenderMesh, xTerrainPhysicsMesh, xTerrainWaterMesh, Zenith_AssetHandler::GetMaterial("Rock"), Zenith_AssetHandler::GetMaterial("Crystal"), Zenith_Maths::Vector2(x * TERRAIN_SIZE, y * TERRAIN_SIZE));


#if 1
			Zenith_ColliderComponent& xCollider = xTerrain.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
#endif
		}
	}

	{
		s_xOgre.Initialise(&xScene, "Ogre");
		Zenith_TransformComponent& xTrans = s_xOgre.GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition({ 60, 170, -20 });
		xTrans.SetRotation({ 0.7071, 0, 0.7071, 0});
		Zenith_ModelComponent& xModel = s_xOgre.AddComponent<Zenith_ModelComponent>();
		xModel.LoadMeshesFromDir(ASSETS_ROOT"Meshes/ogre");
		Flux_MeshGeometry& xMesh0 = xModel.GetMeshGeometryAtIndex(0);
		xMesh0.m_pxAnimation = new Flux_MeshAnimation(ASSETS_ROOT"Meshes/ogre/ogre.fbx", xMesh0);
		Flux_MeshGeometry& xMesh1 = xModel.GetMeshGeometryAtIndex(1);
		xMesh1.m_pxAnimation = new Flux_MeshAnimation(ASSETS_ROOT"Meshes/ogre/ogre.fbx", xMesh1);
	}

	const char* aszAssetNames[] = 
	{
		ASSETS_ROOT"Meshes/Khronos_GLTF_Models/Sponza/glTF",
		ASSETS_ROOT"Meshes/Khronos_GLTF_Models/Avocado/glTF",
		ASSETS_ROOT"Meshes/Khronos_GLTF_Models/BrainStem/glTF",
	};
	float afScales[] = 
	{
		0.1f,
		100.f,
		10.f,
	};
	const char* aszFileNames[] =
	{
		ASSETS_ROOT"Meshes/Khronos_GLTF_Models/Sponza/glTF/Sponza.gltf",
		"",
		ASSETS_ROOT"Meshes/Khronos_GLTF_Models/BrainStem/glTF/BrainStem.gltf",
	};

	for(u_int u = 0; u < COUNT_OF(s_xGltfTest); u++)
	{
		s_xGltfTest[u].Initialise(&xScene, "GLTF Test");
		Zenith_TransformComponent& xTrans = s_xGltfTest[u].GetComponent<Zenith_TransformComponent>();
		xTrans.SetPosition(s_xPlayerSpawn + Zenith_Maths::Vector3(u * 10, 100, 0));
		xTrans.SetScale({ afScales[u],afScales[u],afScales[u] });
		Zenith_ModelComponent& xModel = s_xGltfTest[u].AddComponent<Zenith_ModelComponent>();
		xModel.LoadMeshesFromDir(aszAssetNames[u]);
		for (u_int uMeshEntry = 0; uMeshEntry < xModel.GetNumMeshEntires(); uMeshEntry++)
		{
			Flux_MeshGeometry& xMesh = xModel.GetMeshGeometryAtIndex(uMeshEntry);
			if (xMesh.GetNumBones())
			{
				xMesh.m_pxAnimation = new Flux_MeshAnimation(aszFileNames[u], xMesh);
			}
		}
	}
}

void Test_State_InGame::OnUpdate()
{
	Zenith_Core::Zenith_MainLoop();
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_P))
	{
		Zenith_StateMachine::RequestState(new Test_State_MainMenu);
	}
}

void Test_State_InGame::OnExit()
{
	

	// Now delete all the assets (meshes, textures, materials)
	Zenith_AssetHandler::DeleteMesh("Barrel");
	Zenith_AssetHandler::DeleteMesh("Capsule");
	Zenith_AssetHandler::DeleteMesh("Sphere_Smooth");

	{
		Zenith_AssetHandler::DeleteTexture("Barrel_Diffuse");
		Zenith_AssetHandler::DeleteTexture("Barrel_Metallic");
		Zenith_AssetHandler::DeleteMaterial("Barrel");
	}

	{
		Zenith_AssetHandler::DeleteTexture("Crystal_Diffuse");
		Zenith_AssetHandler::DeleteTexture("Crystal_Normal");
		Zenith_AssetHandler::DeleteTexture("Crystal_Roughness");
		Zenith_AssetHandler::DeleteTexture("Crystal_Metallic");
		Zenith_AssetHandler::DeleteMaterial("Crystal");
	}

	{
		Zenith_AssetHandler::DeleteTexture("MuddyGrass_Diffuse");
		Zenith_AssetHandler::DeleteTexture("MuddyGrass_Normal");
		Zenith_AssetHandler::DeleteTexture("MuddyGrass_Roughness");
		Zenith_AssetHandler::DeleteTexture("MuddyGrass_Metallic");
		Zenith_AssetHandler::DeleteMaterial("MuddyGrass");
	}

	{
		Zenith_AssetHandler::DeleteTexture("SupplyCrate_Diffuse");
		Zenith_AssetHandler::DeleteTexture("SupplyCrate_Normal");
		Zenith_AssetHandler::DeleteTexture("SupplyCrate_Roughness");
		Zenith_AssetHandler::DeleteTexture("SupplyCrate_Metallic");
		Zenith_AssetHandler::DeleteMaterial("SupplyCrate");
	}

	{
		Zenith_AssetHandler::DeleteTexture("Rock_Diffuse");
		Zenith_AssetHandler::DeleteTexture("Rock_Normal");
		Zenith_AssetHandler::DeleteTexture("Rock_Roughness");
		Zenith_AssetHandler::DeleteTexture("Rock_Metallic");
		Zenith_AssetHandler::DeleteMaterial("Rock");
	}

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
			Zenith_AssetHandler::DeleteMesh("Terrain_Render" + strSuffix);
			Zenith_AssetHandler::DeleteMesh("Terrain_Physics" + strSuffix);
			Zenith_AssetHandler::DeleteMesh("Terrain_Water" + strSuffix);
		}
	}
}