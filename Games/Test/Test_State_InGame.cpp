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
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "Input/Zenith_Input.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"


Zenith_State* Zenith_StateMachine::s_pxCurrentState = nullptr;
void Zenith_StateMachine::Project_Initialise()
{
	s_pxCurrentState = new Test_State_InGame;
}

static Zenith_Entity s_xPlayer;
static Zenith_Entity s_xBarrel;
static Zenith_Entity s_xSphere0;
static Zenith_Entity s_xSphere1;
static Zenith_Entity s_axRotatingSpheres[3];
static Zenith_Entity s_xTerrain;
static Zenith_Entity s_xOgre;
static Zenith_Entity s_xGltfTest[3];

static Zenith_Maths::Vector3 s_xPlayerSpawn = { 2100,-566,1500 };

// Asset pointers - stored for direct access and cleanup
static Flux_MeshGeometry* s_pxBarrelMesh = nullptr;
static Flux_MeshGeometry* s_pxCapsuleMesh = nullptr;
static Flux_MeshGeometry* s_pxSphereSmoothMesh = nullptr;

static Flux_Texture* s_pxBarrelDiffuse = nullptr;
static Flux_Texture* s_pxBarrelMetallic = nullptr;
static Flux_Material* s_pxBarrelMaterial = nullptr;

static Flux_Texture* s_pxCrystalDiffuse = nullptr;
static Flux_Texture* s_pxCrystalNormal = nullptr;
static Flux_Texture* s_pxCrystalRoughness = nullptr;
static Flux_Texture* s_pxCrystalMetallic = nullptr;
static Flux_Material* s_pxCrystalMaterial = nullptr;

static Flux_Texture* s_pxMuddyGrassDiffuse = nullptr;
static Flux_Texture* s_pxMuddyGrassNormal = nullptr;
static Flux_Texture* s_pxMuddyGrassRoughness = nullptr;
static Flux_Texture* s_pxMuddyGrassMetallic = nullptr;
static Flux_Material* s_pxMuddyGrassMaterial = nullptr;

static Flux_Texture* s_pxSupplyCrateDiffuse = nullptr;
static Flux_Texture* s_pxSupplyCrateNormal = nullptr;
static Flux_Texture* s_pxSupplyCrateRoughness = nullptr;
static Flux_Texture* s_pxSupplyCrateMetallic = nullptr;
static Flux_Material* s_pxSupplyCrateMaterial = nullptr;

static Flux_Texture* s_pxRockDiffuse = nullptr;
static Flux_Texture* s_pxRockNormal = nullptr;
static Flux_Texture* s_pxRockRoughness = nullptr;
static Flux_Texture* s_pxRockMetallic = nullptr;
static Flux_Material* s_pxRockMaterial = nullptr;

static void LoadAssets()
{
	// Load meshes
	s_pxBarrelMesh = Zenith_AssetHandler::AddMeshFromFile(ASSETS_ROOT"Meshes/barrel_Mesh0_Mat0.zmsh");
	s_pxCapsuleMesh = Zenith_AssetHandler::AddMeshFromFile(ASSETS_ROOT"Meshes/capsule_Mesh0_Mat0.zmsh");
	s_pxSphereSmoothMesh = Zenith_AssetHandler::AddMeshFromFile(ASSETS_ROOT"Meshes/sphereSmooth_Mesh0_Mat0.zmsh");

	// Barrel material
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Meshes/barrel_Diffuse_0.ztx");
		s_pxBarrelDiffuse = Zenith_AssetHandler::AddTexture(xDiffuseData);
		xDiffuseData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Meshes/barrel_Shininess_0.ztx");
		s_pxBarrelMetallic = Zenith_AssetHandler::AddTexture(xMetallicData);
		xMetallicData.FreeAllocatedData();

		s_pxBarrelMaterial = Zenith_AssetHandler::AddMaterial();
		s_pxBarrelMaterial->SetDiffuseWithPath(*s_pxBarrelDiffuse, ASSETS_ROOT"Meshes/barrel_Diffuse_0.ztx");
		s_pxBarrelMaterial->SetRoughnessMetallicWithPath(*s_pxBarrelMetallic, ASSETS_ROOT"Meshes/barrel_Shininess_0.ztx");
	}

	// Crystal material
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/diffuse.ztx");
		s_pxCrystalDiffuse = Zenith_AssetHandler::AddTexture(xDiffuseData);
		xDiffuseData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/normal.ztx");
		s_pxCrystalNormal = Zenith_AssetHandler::AddTexture(xNormalData);
		xNormalData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/roughness.ztx");
		s_pxCrystalRoughness = Zenith_AssetHandler::AddTexture(xRoughnessData);
		xRoughnessData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/crystal2k/metallic.ztx");
		s_pxCrystalMetallic = Zenith_AssetHandler::AddTexture(xMetallicData);
		xMetallicData.FreeAllocatedData();

		s_pxCrystalMaterial = Zenith_AssetHandler::AddMaterial();
		s_pxCrystalMaterial->SetDiffuseWithPath(*s_pxCrystalDiffuse, ASSETS_ROOT"Textures/crystal2k/diffuse.ztx");
		s_pxCrystalMaterial->SetNormalWithPath(*s_pxCrystalNormal, ASSETS_ROOT"Textures/crystal2k/normal.ztx");
		s_pxCrystalMaterial->SetRoughnessMetallicWithPath(*s_pxCrystalRoughness, ASSETS_ROOT"Textures/crystal2k/roughness.ztx");
	}

	// MuddyGrass material
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/diffuse.ztx");
		s_pxMuddyGrassDiffuse = Zenith_AssetHandler::AddTexture(xDiffuseData);
		xDiffuseData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/normal.ztx");
		s_pxMuddyGrassNormal = Zenith_AssetHandler::AddTexture(xNormalData);
		xNormalData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/roughness.ztx");
		s_pxMuddyGrassRoughness = Zenith_AssetHandler::AddTexture(xRoughnessData);
		xRoughnessData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/muddyGrass2k/metallic.ztx");
		s_pxMuddyGrassMetallic = Zenith_AssetHandler::AddTexture(xMetallicData);
		xMetallicData.FreeAllocatedData();

		s_pxMuddyGrassMaterial = Zenith_AssetHandler::AddMaterial();
		s_pxMuddyGrassMaterial->SetDiffuseWithPath(*s_pxMuddyGrassDiffuse, ASSETS_ROOT"Textures/muddyGrass2k/diffuse.ztx");
		s_pxMuddyGrassMaterial->SetNormalWithPath(*s_pxMuddyGrassNormal, ASSETS_ROOT"Textures/muddyGrass2k/normal.ztx");
		s_pxMuddyGrassMaterial->SetRoughnessMetallicWithPath(*s_pxMuddyGrassRoughness, ASSETS_ROOT"Textures/muddyGrass2k/roughness.ztx");
	}

	// SupplyCrate material
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/diffuse.ztx");
		s_pxSupplyCrateDiffuse = Zenith_AssetHandler::AddTexture(xDiffuseData);
		xDiffuseData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/normal.ztx");
		s_pxSupplyCrateNormal = Zenith_AssetHandler::AddTexture(xNormalData);
		xNormalData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/roughness.ztx");
		s_pxSupplyCrateRoughness = Zenith_AssetHandler::AddTexture(xRoughnessData);
		xRoughnessData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/supplyCrate2k/metallic.ztx");
		s_pxSupplyCrateMetallic = Zenith_AssetHandler::AddTexture(xMetallicData);
		xMetallicData.FreeAllocatedData();

		s_pxSupplyCrateMaterial = Zenith_AssetHandler::AddMaterial();
		s_pxSupplyCrateMaterial->SetDiffuseWithPath(*s_pxSupplyCrateDiffuse, ASSETS_ROOT"Textures/supplyCrate2k/diffuse.ztx");
		s_pxSupplyCrateMaterial->SetNormalWithPath(*s_pxSupplyCrateNormal, ASSETS_ROOT"Textures/supplyCrate2k/normal.ztx");
		s_pxSupplyCrateMaterial->SetRoughnessMetallicWithPath(*s_pxSupplyCrateRoughness, ASSETS_ROOT"Textures/supplyCrate2k/roughness.ztx");
	}

	// Rock material
	{
		Zenith_AssetHandler::TextureData xDiffuseData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/diffuse.ztx");
		s_pxRockDiffuse = Zenith_AssetHandler::AddTexture(xDiffuseData);
		xDiffuseData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xNormalData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/normal.ztx");
		s_pxRockNormal = Zenith_AssetHandler::AddTexture(xNormalData);
		xNormalData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xRoughnessData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/roughness.ztx");
		s_pxRockRoughness = Zenith_AssetHandler::AddTexture(xRoughnessData);
		xRoughnessData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xMetallicData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/rock2k/metallic.ztx");
		s_pxRockMetallic = Zenith_AssetHandler::AddTexture(xMetallicData);
		xMetallicData.FreeAllocatedData();

		s_pxRockMaterial = Zenith_AssetHandler::AddMaterial();
		s_pxRockMaterial->SetDiffuseWithPath(*s_pxRockDiffuse, ASSETS_ROOT"Textures/rock2k/diffuse.ztx");
		s_pxRockMaterial->SetNormalWithPath(*s_pxRockNormal, ASSETS_ROOT"Textures/rock2k/normal.ztx");
		s_pxRockMaterial->SetRoughnessMetallicWithPath(*s_pxRockRoughness, ASSETS_ROOT"Textures/rock2k/roughness.ztx");
	}
}

void Test_State_InGame::OnEnter()
{
	// Register all script behaviors with the behavior registry for serialization support
	// This must happen before any scene save/load operations
	static bool s_bBehavioursRegistered = false;
	if (!s_bBehavioursRegistered)
	{
		s_bBehavioursRegistered = true;
		PlayerController_Behaviour::RegisterBehaviour();
		HookesLaw_Behaviour::RegisterBehaviour();
		RotationBehaviour_Behaviour::RegisterBehaviour();
		Zenith_Log("Script behaviours registered with serialization factory");
	}

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

	Zenith_ModelComponent& xModel = s_xPlayer.AddComponent<Zenith_ModelComponent>();
	xModel.LoadMeshesFromDir(ASSETS_ROOT"Meshes/stickymcstickface_anim", s_pxCrystalMaterial);
	for(u_int u = 0; u < xModel.GetNumMeshEntries(); u++)
	{
		xModel.GetMeshGeometryAtIndex(u).m_pxAnimation = new Flux_MeshAnimation(ASSETS_ROOT"Meshes/stickymcstickface_anim/StickyMcStickface_Anim.fbx", xModel.GetMeshGeometryAtIndex(u));
	}

	{
		s_xSphere0.Initialise(&xScene, "Sphere0");
		Zenith_ModelComponent& xModel = s_xSphere0.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxSphereSmoothMesh, *s_pxCrystalMaterial);
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
		xModel.AddMeshEntry(*s_pxSphereSmoothMesh, *s_pxRockMaterial);
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

	{
		s_xBarrel.Initialise(&xScene, "Barrel");
		Zenith_ModelComponent& xModel = s_xBarrel.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxBarrelMesh, *s_pxBarrelMaterial);
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
			xModel.AddMeshEntry(*s_pxSphereSmoothMesh, *s_pxRockMaterial);
			xBehaviour.SetAngularVel({ 1.,0.,0. });
		}
		else if (uCount % 3 == 1)
		{
			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*s_pxSphereSmoothMesh, *s_pxMuddyGrassMaterial);
			xBehaviour.SetAngularVel({ 0.,1.,0. });
		}
		else
		{
			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*s_pxSphereSmoothMesh, *s_pxSupplyCrateMaterial);
		 xBehaviour.SetAngularVel({ 0.,0.,1. });
		}

		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
		Zenith_Physics::SetGravityEnabled(xCollider.GetRigidBody(), false);

		uCount++;
	}

	{
		s_xTerrain.Initialise(&xScene, "Terrain");
		s_xTerrain.AddComponent<Zenith_TerrainComponent>(*s_pxRockMaterial, *s_pxCrystalMaterial);
		Zenith_ColliderComponent& xCollider = s_xTerrain.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
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
		for (u_int uMeshEntry = 0; uMeshEntry < xModel.GetNumMeshEntries(); uMeshEntry++)
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
	// Delete meshes
	Zenith_AssetHandler::DeleteMesh(s_pxBarrelMesh);
	Zenith_AssetHandler::DeleteMesh(s_pxCapsuleMesh);
	Zenith_AssetHandler::DeleteMesh(s_pxSphereSmoothMesh);

	// Delete barrel assets
	Zenith_AssetHandler::DeleteTexture(s_pxBarrelDiffuse);
	Zenith_AssetHandler::DeleteTexture(s_pxBarrelMetallic);
	Zenith_AssetHandler::DeleteMaterial(s_pxBarrelMaterial);

	// Delete crystal assets
	Zenith_AssetHandler::DeleteTexture(s_pxCrystalDiffuse);
	Zenith_AssetHandler::DeleteTexture(s_pxCrystalNormal);
	Zenith_AssetHandler::DeleteTexture(s_pxCrystalRoughness);
	Zenith_AssetHandler::DeleteTexture(s_pxCrystalMetallic);
	Zenith_AssetHandler::DeleteMaterial(s_pxCrystalMaterial);

	// Delete muddy grass assets
	Zenith_AssetHandler::DeleteTexture(s_pxMuddyGrassDiffuse);
	Zenith_AssetHandler::DeleteTexture(s_pxMuddyGrassNormal);
	Zenith_AssetHandler::DeleteTexture(s_pxMuddyGrassRoughness);
	Zenith_AssetHandler::DeleteTexture(s_pxMuddyGrassMetallic);
	Zenith_AssetHandler::DeleteMaterial(s_pxMuddyGrassMaterial);

	// Delete supply crate assets
	Zenith_AssetHandler::DeleteTexture(s_pxSupplyCrateDiffuse);
	Zenith_AssetHandler::DeleteTexture(s_pxSupplyCrateNormal);
	Zenith_AssetHandler::DeleteTexture(s_pxSupplyCrateRoughness);
	Zenith_AssetHandler::DeleteTexture(s_pxSupplyCrateMetallic);
	Zenith_AssetHandler::DeleteMaterial(s_pxSupplyCrateMaterial);

	// Delete rock assets
	Zenith_AssetHandler::DeleteTexture(s_pxRockDiffuse);
	Zenith_AssetHandler::DeleteTexture(s_pxRockNormal);
	Zenith_AssetHandler::DeleteTexture(s_pxRockRoughness);
	Zenith_AssetHandler::DeleteTexture(s_pxRockMetallic);
	Zenith_AssetHandler::DeleteMaterial(s_pxRockMaterial);

	// Clear pointers
	s_pxBarrelMesh = nullptr;
	s_pxCapsuleMesh = nullptr;
	s_pxSphereSmoothMesh = nullptr;
	s_pxBarrelDiffuse = nullptr;
	s_pxBarrelMetallic = nullptr;
	s_pxBarrelMaterial = nullptr;
	s_pxCrystalDiffuse = nullptr;
	s_pxCrystalNormal = nullptr;
	s_pxCrystalRoughness = nullptr;
	s_pxCrystalMetallic = nullptr;
	s_pxCrystalMaterial = nullptr;
	s_pxMuddyGrassDiffuse = nullptr;
	s_pxMuddyGrassNormal = nullptr;
	s_pxMuddyGrassRoughness = nullptr;
	s_pxMuddyGrassMetallic = nullptr;
	s_pxMuddyGrassMaterial = nullptr;
	s_pxSupplyCrateDiffuse = nullptr;
	s_pxSupplyCrateNormal = nullptr;
	s_pxSupplyCrateRoughness = nullptr;
	s_pxSupplyCrateMetallic = nullptr;
	s_pxSupplyCrateMaterial = nullptr;
	s_pxRockDiffuse = nullptr;
	s_pxRockNormal = nullptr;
	s_pxRockRoughness = nullptr;
	s_pxRockMetallic = nullptr;
	s_pxRockMaterial = nullptr;
}
