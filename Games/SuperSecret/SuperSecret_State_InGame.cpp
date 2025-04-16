#include "Zenith.h"
#include "SuperSecret/SuperSecret_State_InGame.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"
#include "Input/Zenith_Input.h"

Zenith_State* Zenith_StateMachine::s_pxCurrentState = new SuperSecret_State_InGame;

static Zenith_Entity s_xController;

static Flux_Texture* g_pxTextures[SUPERSECRET_TEXTURE_INDEX__COUNT];

static void LoadAssets()
{
	for (uint32_t u = 0; u < SUPERSECRET_TEXTURE_INDEX__COUNT; u++)
	{
		g_pxTextures[u] = &Zenith_AssetHandler::AddTexture2D(g_aszTextureNames[u], g_aszTextureFilenames[u]);
		Flux::RegisterBindlessTexture(g_pxTextures[u], u);
	}
}

void SuperSecret_State_InGame::OnEnter()
{
	Flux_MemoryManager::BeginFrame();
	LoadAssets();
	Flux_MemoryManager::EndFrame(false);

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	s_xController.Initialise(&xScene, "Controller");

	Zenith_CameraComponent& xCamera = s_xController.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xPos = { 0, 0, 0 };
	const float fPitch = 0;
	const float fYaw = 0;
	const float fFOV = 45;
	const float fNear = 1;
	const float fFar = 5000;
	const float fAspectRatio = 16. / 9.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xScene.SetMainCameraEntity(s_xController);

	Zenith_TextComponent& xText = s_xController.AddComponent<Zenith_TextComponent>();
	TextEntry xTextEntry = { "Super Secret Project Don't Tell Chloe", { 100, 200 }, 0.1f };
	xText.AddText(xTextEntry);
}

void SuperSecret_State_InGame::OnUpdate()
{
	Zenith_Core::Zenith_MainLoop();
}

void SuperSecret_State_InGame::OnExit()
{
	Zenith_Scene::GetCurrentScene().Reset();
}