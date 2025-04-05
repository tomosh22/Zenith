#include "Zenith.h"
#include "SuperSecret/SuperSecret_State_InGame.h"
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

Zenith_State* Zenith_StateMachine::s_pxCurrentState = new SuperSecret_State_InGame;

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



static void LoadAssets()
{
	Zenith_AssetHandler::AddMesh("StickyMcStickFace", ASSETS_ROOT"Meshes/StickyMcStickface_Mesh0_Mat0.zmsh");
	Zenith_AssetHandler::AddMesh("Barrel", ASSETS_ROOT"Meshes/barrel_Mesh0_Mat0.zmsh");
	{
		Zenith_AssetHandler::AddTexture2D("Barrel_Diffuse", ASSETS_ROOT"Meshes/barrel_Diffuse_0.ztx");
		Zenith_AssetHandler::AddTexture2D("Barrel_Metallic", ASSETS_ROOT"Meshes/barrel_Shininess_0.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Barrel_Diffuse");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Barrel_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("Barrel");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetMetallic(&xMetallic);
	}

	Zenith_AssetHandler::AddMesh("Capsule", ASSETS_ROOT"Meshes/capsule_Mesh0_Mat0.zmsh");
	Zenith_AssetHandler::AddMesh("Sphere_Smooth", ASSETS_ROOT"Meshes/sphereSmooth_Mesh0_Mat0.zmsh");
	{
		Zenith_AssetHandler::AddTexture2D("Crystal_Diffuse", ASSETS_ROOT"Textures/crystal2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D("Crystal_Normal", ASSETS_ROOT"Textures/crystal2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D("Crystal_Roughness", ASSETS_ROOT"Textures/crystal2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D("Crystal_Metallic", ASSETS_ROOT"Textures/crystal2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Crystal_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Crystal_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Crystal_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Crystal_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("Crystal");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
	}
	{
		Zenith_AssetHandler::AddTexture2D("MuddyGrass_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D("MuddyGrass_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D("MuddyGrass_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D("MuddyGrass_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/muddyGrass2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("MuddyGrass_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("MuddyGrass_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("MuddyGrass_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("MuddyGrass_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("MuddyGrass");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
	}
	{
		Zenith_AssetHandler::AddTexture2D("SupplyCrate_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D("SupplyCrate_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D("SupplyCrate_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D("SupplyCrate_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/supplyCrate2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("SupplyCrate_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("SupplyCrate_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("SupplyCrate_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("SupplyCrate_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("SupplyCrate");
		xMat.SetDiffuse(&xDiffuse);
		xMat.SetNormal(&xNormal);
		xMat.SetRoughness(&xRoughness);
		xMat.SetMetallic(&xMetallic);
	}
	{
		Zenith_AssetHandler::AddTexture2D("Rock_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture2D("Rock_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/normal.ztx");
		Zenith_AssetHandler::AddTexture2D("Rock_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture2D("Rock_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Rock_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Rock_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Rock_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Rock_Metallic");

		Flux_Material& xMat = Zenith_AssetHandler::AddMaterial("Rock");
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
			Zenith_AssetHandler::AddMesh("Terrain_Render" + strSuffix, std::string("C:/dev/Zenith/Games/Test/Assets/Terrain/Render_" + strSuffix + ".zmsh").c_str(), true);
			Zenith_AssetHandler::AddMesh("Terrain_Physics" + strSuffix, std::string("C:/dev/Zenith/Games/Test/Assets/Terrain/Physics_" + strSuffix + ".zmsh").c_str(), true);
#else
			Zenith_AssetHandler::AddMesh("Terrain" + strSuffix, std::string("C:/dev/Zenith/Games/Test/Assets/Terrain/" + strSuffix + ".zmsh").c_str(), false);
#endif
		}
	}
}

void SuperSecret_State_InGame::OnEnter()
{
	Flux_MemoryManager::BeginFrame();
	LoadAssets();
	Flux_MemoryManager::EndFrame(false);

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

	Zenith_TextComponent& xText = s_xPlayer.AddComponent<Zenith_TextComponent>();
	TextEntry xTextEntry = { "Super Secret Project Don't Tell Chloe", { 100, 200 }, 0.1f };
	xText.AddText(xTextEntry);
}

void SuperSecret_State_InGame::OnUpdate()
{
	Zenith_Core::Zenith_MainLoop();
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_P))
	{
		//Zenith_StateMachine::RequestState(new Test_State_MainMenu);
	}
}

void SuperSecret_State_InGame::OnExit()
{
	Zenith_Scene::GetCurrentScene().Reset();
}